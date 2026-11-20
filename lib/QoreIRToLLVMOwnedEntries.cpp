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

// The definition of the AOT fast entries that take their arguments over (issue #6105), in a unit of its own outside
// the single compilation unit of libqore, compiled with hidden visibility: the copy of a function
// (llvm::CloneFunction()) instantiates LLVM templates (llvm::ValueMap) that would otherwise be exported from libqore,
// which changes the dynamic symbol lookups of every process that loads it

#include "qore/intern/QoreJITIncludes.h"
#include "qore/intern/QoreIRToLLVM.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Cloning.h>

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
        for (size_t i = 0; i < mask.size(); ++i) {
            if (mask[i] == '1' && !taken[i]) {
                owned->eraseFromParent();
                error = "internal error: the fast entry '" + fast_fn->getName().str()
                    + "' takes no reference to argument " + std::to_string(i) + " that its caller hands over";
                return false;
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
#ifdef DEBUG
        // the module was verified before the copies were made
        assert(!llvm::verifyFunction(*owned, &llvm::errs()));
#endif
    }
    return true;
}
