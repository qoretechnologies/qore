/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreIRToLLVMOwnedEntries.cpp

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

// The AOT fast entries that take their arguments over (issue #6105): the decisions of the calls and the copies of the
// entries, in a unit of its own outside the single compilation unit of libqore (which is at GCC's inline growth
// limit, qore#6494), compiled with hidden visibility: the copy of a function (llvm::CloneFunction()) instantiates LLVM
// templates (llvm::ValueMap) that would otherwise be exported from libqore, which changes the dynamic symbol lookups
// of every process that loads it

#include "qore/intern/QoreJITIncludes.h"
#include "qore/intern/QoreIRToLLVM.h"
#include "qore/intern/QoreIRFrameCheck.h"
#include "qore/intern/QoreValueEncoding.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Cloning.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>

// documented in QoreIRToLLVM.h
bool QoreIRToLLVM::materializeOwnedFastEntries(llvm::Module& module, std::string& error) {
    std::vector<llvm::Function*> decls;
    for (llvm::Function& f : module) {
        // not a cancellation point: this only collects pointers, bounded by the module's size
        if (f.isDeclaration() && f.getName().contains(QORE_OWNED_FAST_ENTRY_INFIX)) {
            decls.push_back(&f);
        }
    }
    for (llvm::Function* decl : decls) {
        llvm::StringRef name = decl->getName();
        size_t pos = name.rfind(QORE_OWNED_FAST_ENTRY_INFIX);
        llvm::StringRef mask = name.substr(pos + strlen(QORE_OWNED_FAST_ENTRY_INFIX));
        llvm::Function* fast_fn = module.getFunction(name.substr(0, pos));
        if (!fast_fn || fast_fn->isDeclaration() || fast_fn->getFunctionType() != decl->getFunctionType()
                || mask.size() > fast_fn->arg_size()) {
            error = "internal error: the fast entry of '" + name.str() + "' is not defined in the module";
            return false;
        }
        llvm::ValueToValueMapTy vmap;
        llvm::Function* owned = llvm::CloneFunction(fast_fn, vmap);
        // the references that the prologue takes to the arguments that the callers hand over
        std::vector<bool> taken(mask.size(), false);
        std::vector<llvm::Instruction*> erase;
        for (llvm::BasicBlock& bb : *owned) {
            for (llvm::Instruction& inst : bb) {
                if (!inst.getMetadata(QORE_FAST_ENTRY_ARG_REF_MD)) {
                    continue;
                }
                auto* call = llvm::cast<llvm::CallInst>(&inst);
                auto* arg = llvm::dyn_cast<llvm::Argument>(call->getArgOperand(0));
                assert(arg);
                if (arg && arg->getArgNo() < mask.size() && mask[arg->getArgNo()] == '1') {
                    assert(!taken[arg->getArgNo()]);
                    taken[arg->getArgNo()] = true;
                    erase.push_back(call);
                }
            }
        }
        // an argument that the call hands over and to which the prologue takes no reference (a parameter that the
        // body does not use, or that it borrows) is released by the call when it returns, as the call releases the
        // argument of the fast entry
        std::string taken_mask(mask.size(), '0');
        std::vector<unsigned> release_after;
        for (size_t i = 0; i < mask.size(); ++i) {
            if (taken[i]) {
                taken_mask[i] = '1';
            } else if (mask[i] == '1') {
                release_after.push_back(static_cast<unsigned>(i));
            }
        }
        if (!release_after.empty()) {
            llvm::FunctionCallee decref = module.getOrInsertFunction("qore_rt_decref",
                llvm::FunctionType::get(llvm::Type::getVoidTy(module.getContext()),
                    {llvm::Type::getInt64Ty(module.getContext()), llvm::PointerType::getUnqual(module.getContext())},
                    false));
            std::vector<llvm::CallInst*> calls;
            for (llvm::User* user : decl->users()) {
                auto* call = llvm::dyn_cast<llvm::CallInst>(user);
                if (!call || call->getCalledOperand() != decl) {
                    owned->eraseFromParent();
                    error = "internal error: '" + name.str() + "' is used other than by a call";
                    return false;
                }
                calls.push_back(call);
            }
            for (llvm::CallInst* call : calls) {
                llvm::IRBuilder<> b(call->getNextNode());
                // the ExceptionSink is the last argument of a fast entry
                llvm::Value* xsink = call->getArgOperand(call->arg_size() - 1);
                for (unsigned i : release_after) {
                    b.CreateCall(decref, {call->getArgOperand(i), xsink});
                }
            }
        }
        for (llvm::Instruction* inst : erase) {
            inst->eraseFromParent();
        }
        std::string owned_name = name.str();
        decl->replaceAllUsesWith(owned);
        decl->eraseFromParent();
        owned->setName(owned_name);
        owned->setLinkage(llvm::Function::InternalLinkage);
        // the arguments that the copy owns, which the thunk of a large frame releases if it does not call the code
        // (QoreIRFrameCheck)
        if (taken_mask.find('1') != std::string::npos) {
            owned->addFnAttr(QORE_OWNED_FAST_ENTRY_ARGS_ATTR, taken_mask);
        }
#ifdef DEBUG
        // the module was verified before the copies were made
        assert(!llvm::verifyFunction(*owned, &llvm::errs()));
#endif
    }
    return true;
}

//! Returns true if the fast entry of \a info takes a reference of its own to its argument \a i when it is entered
/** The rule of the prologue of a fast entry (QoreIRToLLVM::preCreateLocalAllocas(), the reference marked with
    QORE_FAST_ENTRY_ARG_REF_MD): a boxed parameter that is not borrowed (BatchCalleeInfo::param_noescape, the borrowed
    parameters of the AOT fast entries in QoreAOT.cpp), or that is borrowed but strips the complex type of its
    argument; a parameter of a native ABI kind takes no reference.  materializeOwnedFastEntries() checks that the copy
    of the entry has the reference of each argument that a call hands over to it (issue #6105)
*/
static bool qore_aot_fast_entry_param_takes_ref(const BatchCalleeInfo& info, const UserSignature* sig, unsigned i) {
    if (i >= info.num_params || (i < info.param_kinds.size() && info.param_kinds[i] != BatchCalleeParamKind::Boxed)) {
        return false;
    }
    if (i < info.param_noescape.size() && info.param_noescape[i]) {
        return sig && i < sig->numParams() && q_type_strips_complex_type(sig->lv[i]->getTypeInfoForLValue());
    }
    return true;
}

// documented in QoreIRToLLVM.h
bool QoreIRToLLVM::callCanTakeOverToFastEntry(const QoreIRInstruction* inst, int arg_start,
        const AbstractQoreFunctionVariant* variant, const BatchCalleeInfo& info) const {
    static constinit QoreEnvFlag disable_env("QORE_DISABLE_AOT_OWNED_FAST_ENTRY");
    if (!aot_mode || info.cross_module || info.preloaded_fast_entry || !info.approach_b_eligible
            || arg_start < 0 || disable_env.get()
            || !qore_ir_fast_entry_refuses_given_args(true, variant)) {
        return false;
    }
    const UserVariantBase* uvb = variant->getUserVariantBase();
    const UserSignature* sig = uvb ? uvb->getUserSignature() : nullptr;
    bool takes_over = false;
    // the test of buildArgCleanupArray() for an argument whose cleanup slot is given to the callee
    for (size_t i = static_cast<size_t>(arg_start); i < inst->operands.size(); ++i) {
        uint32_t value_id = inst->operands[i].id;
        auto uses_it = operand_remaining_uses.find(value_id);
        if (invoke_alloca_map.count(value_id)
                && (uses_it == operand_remaining_uses.end() || uses_it->second <= 1)
                && !(value_id < reused_call_args.size() && reused_call_args[value_id])) {
            if (qore_aot_fast_entry_param_takes_ref(info, sig, static_cast<unsigned>(i - arg_start))) {
                takes_over = true;
            }
            continue;
        }
        // the slot of a read through a reference is given only if it holds the value, which is known at run time
        if (givenReferenceReadSlot(inst, value_id)) {
            return false;
        }
    }
    return takes_over;
}

// documented in QoreIRToLLVM.h
QoreIRToLLVM::FastEntryTakeOver QoreIRToLLVM::fastEntryTakeOverSlots(const AbstractQoreFunctionVariant* variant,
        const BatchCalleeInfo& info, const std::vector<llvm::Value*>& given_slots) const {
    const UserVariantBase* uvb = variant ? variant->getUserVariantBase() : nullptr;
    const UserSignature* sig = uvb ? uvb->getUserSignature() : nullptr;
    FastEntryTakeOver rv;
    rv.slots.assign(info.num_params, nullptr);
    for (size_t i = 0; i < given_slots.size(); ++i) {
        if (!given_slots[i]) {
            continue;
        }
        if (i < info.num_params && qore_aot_fast_entry_param_takes_ref(info, sig, static_cast<unsigned>(i))) {
            rv.slots[i] = given_slots[i];
        } else {
            rv.releases_given = true;
        }
    }
    return rv;
}

// documented in QoreIRToLLVM.h
llvm::Function* QoreIRToLLVM::ownedFastEntry(llvm::Function* fast_fn, const FastEntryTakeOver& take_over,
        llvm::Module& module, bool& releases_given) const {
    if (std::none_of(take_over.slots.begin(), take_over.slots.end(), [](llvm::Value* v) { return v != nullptr; })) {
        return nullptr;
    }
    releases_given = take_over.releases_given;
    std::string name = fast_fn->getName().str() + QORE_OWNED_FAST_ENTRY_INFIX;
    name.reserve(name.size() + take_over.slots.size());
    for (llvm::Value* slot : take_over.slots) {
        name += slot ? '1' : '0';
    }
    if (llvm::Function* fn = module.getFunction(name)) {
        assert(fn->getFunctionType() == fast_fn->getFunctionType());
        return fn;
    }
    // a declaration until materializeOwnedFastEntries() defines it as a copy of the fast entry
    llvm::Function* fn = llvm::Function::Create(fast_fn->getFunctionType(), llvm::Function::ExternalLinkage, name,
        module);
    fn->setCallingConv(fast_fn->getCallingConv());
    return fn;
}

// documented in QoreIRToLLVM.h
llvm::Value* QoreIRToLLVM::emitOwnedFastEntryCall(llvm::Function* owned_fn, const FastEntryTakeOver& take_over,
        const std::vector<llvm::Value*>& call_args) {
    // the entry takes over the references of the temporaries whose slots are cleared here: it releases them when the
    // parameters are written or when it returns, and the caller's landing pad finds the slots empty if it raises an
    // exception
    for (llvm::Value* slot : take_over.slots) {
        if (slot) {
            builder->CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), slot);
        }
    }
    return builder->CreateCall(owned_fn, call_args);
}

// documented in QoreIRToLLVM.h
void QoreIRToLLVM::emitFastEntryEarlyExitReleases(llvm::Module& module, llvm::BasicBlock* stack_overflow_bb) {
    if (fast_entry_param_allocas.empty() && fast_entry_converted_arg_refs.empty()) {
        return;
    }
    llvm::IRBuilderBase::InsertPoint saved_ip = builder->saveIP();
    auto decref = module.getOrInsertFunction("qore_rt_decref",
        llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
    for (llvm::BasicBlock* bb : {stack_overflow_bb, debug_fast_entry_end}) {
        llvm::Instruction* exit = bb ? bb->getTerminator() : nullptr;
        if (!exit) {
            continue;
        }
        assert(llvm::isa<llvm::ReturnInst>(exit));
        builder->SetInsertPoint(exit);
        for (llvm::AllocaInst* alloca : fast_entry_param_allocas) {
            builder->CreateCall(decref, {builder->CreateLoad(i64_type, alloca), xsink_arg});
        }
        for (llvm::Value* arg : fast_entry_converted_arg_refs) {
            builder->CreateCall(decref, {arg, xsink_arg});
        }
    }
    builder->restoreIP(saved_ip);
}
