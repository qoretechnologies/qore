/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreIRFrameCheck.cpp

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

#include <qore/Qore.h>
#include "qore/intern/qore_thread_intern.h"
#include "qore/intern/QoreIRFrameCheck.h"
#include "qore/intern/QoreValueEncoding.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DiagnosticHandler.h>
#include <llvm/IR/DiagnosticInfo.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include <cstring>
#include <memory>
#include <optional>
#include <vector>

namespace {
//! the margin above the frame size of the diagnostic that a thunk checks: the return address and the callee-saved
//! registers that the prologue saves outside of the frame, and the frame of the stack check's own call
constexpr uint64_t FrameCheckMargin = 256;

//! the bytes added to the bound of a frame for what the code generator adds to the values and allocas of a function:
//! the callee-saved registers, the alignment of the frame and its objects, and stack-passed call arguments
constexpr uint64_t FrameBoundOverhead = 2048;

//! the function attribute that the code generator gives a function whose entry checks the stack (qore_rt_check_stack())
/** its value is the kind of the value that the function returns when the check raises an exception: 'b' (boxed:
    VAL_NOTHING), 'i' (a native int: 0), 'f' (a native float: 0.0) or 'o' (a native bool: false); its ExceptionSink
    argument has the parameter attribute QORE_FRAME_CHECK_XSINK_ATTR, which LLVM keeps on the argument wherever its
    interprocedural passes leave it (qore#7072)
*/
constexpr const char* FrameCheckAttr = "qore-frame-check";

//! the suffix of the name of the variable with the frame size that the thunk of a function checks
constexpr const char* ReserveSuffix = ".qore.frame.reserve";

//! the stack-size diagnostics of a code generation
/** The diagnostic of the code of a function with a thunk sets the frame size that the thunk checks: the code generator
    emits the module's variables after the code of all of its functions, so the variable is emitted with the size.  A
    diagnostic of any other function is a frame above the threshold that its bound did not expect.
*/
class FrameSizeHandler : public llvm::DiagnosticHandler {
public:
    FrameSizeHandler(llvm::Module& m, std::map<std::string, uint64_t>& sizes,
            std::map<std::string, uint64_t>& unexpected, std::unique_ptr<llvm::DiagnosticHandler> prev)
            : m(m), sizes(sizes), unexpected(unexpected), prev(std::move(prev)) {
    }

    bool handleDiagnostics(const llvm::DiagnosticInfo& di) override {
        auto* ss = llvm::dyn_cast<llvm::DiagnosticInfoStackSize>(&di);
        if (!ss || ss->getFunction().getParent() != &m) {
            return prev->handleDiagnostics(di);
        }
        std::string name = ss->getFunction().getName().str();
        llvm::GlobalVariable* reserve = m.getNamedGlobal(name + ReserveSuffix);
        if (reserve) {
            reserve->setInitializer(llvm::ConstantInt::get(reserve->getValueType(),
                ss->getStackSize() + FrameCheckMargin));
            sizes[name] = ss->getStackSize();
        } else {
            unexpected[name] = ss->getStackSize();
        }
        return true;
    }

    bool isAnalysisRemarkEnabled(llvm::StringRef pass_name) const override {
        return prev->isAnalysisRemarkEnabled(pass_name);
    }

    bool isMissedOptRemarkEnabled(llvm::StringRef pass_name) const override {
        return prev->isMissedOptRemarkEnabled(pass_name);
    }

    bool isPassedOptRemarkEnabled(llvm::StringRef pass_name) const override {
        return prev->isPassedOptRemarkEnabled(pass_name);
    }

    bool isAnyRemarkEnabled() const override {
        return prev->isAnyRemarkEnabled();
    }

    std::unique_ptr<llvm::DiagnosticHandler> takePrevious() {
        return std::move(prev);
    }

private:
    llvm::Module& m;
    std::map<std::string, uint64_t>& sizes;
    std::map<std::string, uint64_t>& unexpected;
    std::unique_ptr<llvm::DiagnosticHandler> prev;
};

//! handles the stack-size diagnostics of a code generation in its scope
class FrameSizeScope {
public:
    FrameSizeScope(llvm::Module& m, std::map<std::string, uint64_t>& sizes,
            std::map<std::string, uint64_t>& unexpected) : ctx(m.getContext()) {
        std::unique_ptr<llvm::DiagnosticHandler> prev = ctx.getDiagnosticHandler();
        if (!prev) {
            prev = std::make_unique<llvm::DiagnosticHandler>();
        }
        ctx.setDiagnosticHandler(std::make_unique<FrameSizeHandler>(m, sizes, unexpected, std::move(prev)));
    }

    ~FrameSizeScope() {
        std::unique_ptr<llvm::DiagnosticHandler> mine = ctx.getDiagnosticHandler();
        ctx.setDiagnosticHandler(static_cast<FrameSizeHandler*>(mine.get())->takePrevious());
    }

    FrameSizeScope(const FrameSizeScope&) = delete;
    FrameSizeScope& operator=(const FrameSizeScope&) = delete;

private:
    llvm::LLVMContext& ctx;
};

//! returns a bound of the frame of a function, or UINT64_MAX if it has none (a dynamic alloca)
/** the frame holds the function's static allocas and the stack slots of the values that the register allocator spills,
    which are at most the size of every value the function computes, and what the code generator adds
    (FrameBoundOverhead); the bound is far above the frame of a function, so that a frame above the threshold is that
    of a function whose bound is above it
*/
uint64_t getFrameBound(const llvm::Function& f) {
    const llvm::DataLayout& dl = f.getParent()->getDataLayout();
    uint64_t bound = FrameBoundOverhead;
    for (const llvm::BasicBlock& bb : f) {
        for (const llvm::Instruction& inst : bb) {
            if (auto* ai = llvm::dyn_cast<llvm::AllocaInst>(&inst)) {
                std::optional<llvm::TypeSize> size = ai->getAllocationSize(dl);
                if (!size || size->isScalable()) {
                    return UINT64_MAX;
                }
                bound += size->getFixedValue();
                continue;
            }
            llvm::Type* t = inst.getType();
            if (!t->isVoidTy() && t->isSized()) {
                llvm::TypeSize size = dl.getTypeAllocSize(t);
                if (size.isScalable()) {
                    return UINT64_MAX;
                }
                bound += size.getFixedValue();
            }
        }
    }
    return bound;
}

//! returns the value that a function returns when its stack check raises an exception, or nullptr if unknown
llvm::Constant* getCheckReturnValue(llvm::Function& f, char kind) {
    llvm::Type* rt = f.getReturnType();
    switch (kind) {
        case 'b':
            return rt->isIntegerTy(64) ? llvm::ConstantInt::get(rt, VAL_NOTHING) : nullptr;
        case 'i':
            return rt->isIntegerTy(64) ? llvm::ConstantInt::get(rt, 0) : nullptr;
        case 'f':
            return rt->isDoubleTy() ? llvm::ConstantFP::get(rt, 0.0) : nullptr;
        case 'o':
            return rt->isIntegerTy(1) ? llvm::ConstantInt::getFalse(rt) : nullptr;
        default:
            return nullptr;
    }
}

//! makes \a f a thunk that checks the stack for the frame of the code of \a f before it calls the code with a tail
//! call
/** the frame size that the thunk checks is a variable that the diagnostic of the code's frame sets (FrameSizeHandler)

    @param f the function
    @param error set to why the thunk cannot be made for a function with a stack check (FrameCheckAttr)

    @return the function with the code of \a f, or nullptr if \a f has no stack check (\a error is empty then) or
    if its thunk cannot be made (\a error says why: the compilation fails, as the function would allocate a frame
    larger than its stack check covers; qore#7072)
*/
llvm::Function* makeThunk(llvm::Function& f, std::string& error) {
    if (!f.hasFnAttribute(FrameCheckAttr)) {
        return nullptr;
    }
    std::string fname = f.getName().str();
    if (f.isVarArg()) {
        error = "the function '" + fname + "' with a stack check takes a variable number of arguments";
        return nullptr;
    }
    llvm::StringRef spec = f.getFnAttribute(FrameCheckAttr).getValueAsString();
    if (spec.size() != 1) {
        error = "the stack check of '" + fname + "' has the invalid specification '" + spec.str() + "'";
        return nullptr;
    }
    // the ExceptionSink argument, wherever the optimization has left it
    unsigned xsink_index = f.arg_size();
    for (unsigned i = 0, e = f.arg_size(); i < e; ++i) {
        if (f.getAttributes().hasParamAttr(i, QORE_FRAME_CHECK_XSINK_ATTR)) {
            if (xsink_index != f.arg_size()) {
                error = "the function '" + fname + "' with a stack check has more than one ExceptionSink argument";
                return nullptr;
            }
            xsink_index = i;
        }
    }
    if (xsink_index == f.arg_size()) {
        error = "the function '" + fname + "' with a stack check has no ExceptionSink argument after optimization";
        return nullptr;
    }
    if (!f.getArg(xsink_index)->getType()->isPointerTy()) {
        error = "the ExceptionSink argument of '" + fname + "' with a stack check is not a pointer";
        return nullptr;
    }
    // the arguments whose references the function owns, which the thunk releases if it does not call the code
    std::vector<unsigned> owned;
    for (unsigned i = 0, e = f.arg_size(); i < e; ++i) {
        if (f.getAttributes().hasParamAttr(i, QORE_OWNED_FAST_ENTRY_ARG_ATTR)) {
            if (!f.getArg(i)->getType()->isIntegerTy(64)) {
                error = "the owned argument " + std::to_string(i) + " of '" + fname + "' with a stack check is not "
                    "a value";
                return nullptr;
            }
            owned.push_back(i);
        }
    }
    llvm::Constant* nothing = nullptr;
    if (!f.getReturnType()->isVoidTy()) {
        nothing = getCheckReturnValue(f, spec[0]);
        if (!nothing) {
            error = "the return type of '" + fname + "' with a stack check does not match its kind '" + spec.str()
                + "'";
            return nullptr;
        }
    }

    llvm::Module& m = *f.getParent();
    llvm::LLVMContext& ctx = m.getContext();
    llvm::Type* i32_type = llvm::Type::getInt32Ty(ctx);
    llvm::Type* i64_type = llvm::Type::getInt64Ty(ctx);
    llvm::Type* ptr_type = llvm::PointerType::getUnqual(ctx);

    // the thunk takes the name, linkage and every use of the function, whose code becomes a function of its own
    llvm::Function* thunk = llvm::Function::Create(f.getFunctionType(), f.getLinkage(), f.getAddressSpace(), "", &m);
    thunk->takeName(&f);
    f.setName(thunk->getName() + QoreIRFrameCheck::BodySuffix);
    thunk->setCallingConv(f.getCallingConv());
    thunk->setAttributes(f.getAttributes());
    thunk->removeFnAttr("warn-stack-size");
    thunk->removeFnAttr(FrameCheckAttr);
    thunk->removeFnAttr(llvm::Attribute::NoInline);
    // optnone requires noinline, which the thunk does not have
    thunk->removeFnAttr(llvm::Attribute::OptimizeNone);
    thunk->setVisibility(f.getVisibility());
    thunk->setDLLStorageClass(f.getDLLStorageClass());
    thunk->setUnnamedAddr(f.getUnnamedAddr());
    thunk->setAlignment(f.getAlign());
    f.replaceAllUsesWith(thunk);
    f.setLinkage(llvm::GlobalValue::InternalLinkage);
    f.setVisibility(llvm::GlobalValue::DefaultVisibility);
    f.setDLLStorageClass(llvm::GlobalValue::DefaultStorageClass);
    f.addFnAttr(llvm::Attribute::NoInline);
    f.removeFnAttr(llvm::Attribute::AlwaysInline);

    // the frame size that the thunk checks, set when the code's frame is laid out; until then the margin, as for an
    // empty frame (a frame of no bytes has no diagnostic)
    auto* reserve = new llvm::GlobalVariable(m, i64_type, false, llvm::GlobalValue::InternalLinkage,
        llvm::ConstantInt::get(i64_type, FrameCheckMargin), f.getName() + ReserveSuffix);

    llvm::FunctionCallee check_fn = m.getOrInsertFunction("qore_rt_check_stack_reserve",
        llvm::FunctionType::get(i32_type, {ptr_type, i64_type}, false));

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(ctx, "frame_check", thunk);
    llvm::BasicBlock* call_bb = llvm::BasicBlock::Create(ctx, "frame_ok", thunk);
    llvm::BasicBlock* overflow_bb = llvm::BasicBlock::Create(ctx, "frame_overflow", thunk);
    llvm::IRBuilder<> b(entry);
    llvm::Value* size = b.CreateLoad(i64_type, reserve, "frame_reserve");
    llvm::Value* err = b.CreateCall(check_fn, {thunk->getArg(xsink_index), size});
    b.CreateCondBr(b.CreateICmpEQ(err, llvm::ConstantInt::get(i32_type, 0)), call_bb, overflow_bb);

    b.SetInsertPoint(call_bb);
    std::vector<llvm::Value*> args;
    args.reserve(thunk->arg_size());
    for (llvm::Argument& a : thunk->args()) {
        args.push_back(&a);
    }
    llvm::CallInst* call = b.CreateCall(&f, args);
    call->setCallingConv(f.getCallingConv());
    call->setAttributes(f.getAttributes());
    // the frame of the thunk is released before the code of the function allocates its own
    call->setTailCallKind(llvm::CallInst::TCK_MustTail);
    if (thunk->getReturnType()->isVoidTy()) {
        b.CreateRetVoid();
    } else {
        b.CreateRet(call);
    }

    b.SetInsertPoint(overflow_bb);
    // a fast entry that takes its arguments over owns the references of its arguments with
    // QORE_OWNED_FAST_ENTRY_ARG_ATTR, which its code releases: the thunk that does not call the code releases them
    // (issue #6105)
    if (!owned.empty()) {
        llvm::FunctionCallee decref = m.getOrInsertFunction("qore_rt_decref",
            llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), {i64_type, ptr_type}, false));
        for (unsigned i : owned) {
            b.CreateCall(decref, {thunk->getArg(i), thunk->getArg(xsink_index)});
        }
    }
    if (nothing) {
        b.CreateRet(nothing);
    } else {
        b.CreateRetVoid();
    }
    return &f;
}
}

uint64_t QoreIRFrameCheck::getThreshold() {
    return QORE_STACK_GUARD / 2;
}

bool QoreIRFrameCheck::emit(llvm::Module& m, const std::function<bool(llvm::Module&, std::string&)>& emit_fn,
        std::string& error, std::map<std::string, uint64_t>* thunked) {
    uint64_t threshold = getThreshold();
    // the functions whose frame can be above the threshold get a thunk; every other function's frame is checked
    // against the threshold
    std::vector<llvm::Function*> large;
    std::vector<llvm::Function*> other;
    for (llvm::Function& f : m) {
        if (f.isDeclaration()) {
            continue;
        }
        if (getFrameBound(f) > threshold) {
            large.push_back(&f);
        } else {
            other.push_back(&f);
        }
    }
    if (large.empty()) {
        return emit_fn(m, error);
    }

    std::string t = std::to_string(threshold);
    for (llvm::Function* f : other) {
        f->addFnAttr("warn-stack-size", t);
    }
    // the code of each function with a thunk, whose frame size the thunk checks
    std::vector<std::string> bodies;
    for (llvm::Function* f : large) {
        std::string thunk_error;
        if (llvm::Function* body = makeThunk(*f, thunk_error)) {
            // every frame of the code has its diagnostic, which gives its size to the thunk
            body->addFnAttr("warn-stack-size", "0");
            bodies.push_back(body->getName().str());
        } else if (!thunk_error.empty()) {
            // a function with a stack check never loses it: the compilation fails (qore#7072)
            error = thunk_error;
            return false;
        } else {
            f->addFnAttr("warn-stack-size", t);
        }
    }

    std::map<std::string, uint64_t> sizes;
    std::map<std::string, uint64_t> unexpected;
    {
        FrameSizeScope scope(m, sizes, unexpected);
        if (!emit_fn(m, error)) {
            return false;
        }
    }

    // the size of every frame with a thunk was set: a frame of no bytes has no diagnostic, and the thunk then checks
    // the margin, which covers it
    for (const std::string& name : bodies) {
        if (!sizes.count(name)) {
            llvm::Function* body = m.getFunction(name);
            if (!body || body->isDeclaration()) {
                error = "the code of '" + name + "' was not generated with its stack check";
                return false;
            }
        }
    }

    // QORE_FRAME_CHECK_REPORT reports the frames on stderr
    bool report = q_env_is_set("QORE_FRAME_CHECK_REPORT");
    size_t suffix_len = strlen(BodySuffix);
    for (const auto& i : sizes) {
        std::string name = i.first.substr(0, i.first.size() - suffix_len);
        if (thunked) {
            (*thunked)[name] = i.second;
        }
        if (report) {
            fprintf(stderr, "frame check: %s: %llu bytes: thunk\n", name.c_str(),
                static_cast<unsigned long long>(i.second));
        }
    }
    // a function without a thunk whose frame is above the threshold: its bound is wrong, or it has no stack check
    for (const auto& i : unexpected) {
        llvm::Function* f = m.getFunction(i.first);
        if (f && f->hasFnAttribute(FrameCheckAttr)) {
            error = "the frame of '" + i.first + "' (" + std::to_string(i.second) + " bytes) is above the "
                + std::to_string(threshold) + " bytes that its stack check covers";
            return false;
        }
        if (report) {
            fprintf(stderr, "frame check: %s: %llu bytes: no stack check\n", i.first.c_str(),
                static_cast<unsigned long long>(i.second));
        }
    }
    return true;
}
