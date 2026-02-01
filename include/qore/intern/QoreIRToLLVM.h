/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreIRToLLVM.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_QOREIRTOLLVM_H
#define _QORE_QOREIRTOLLVM_H

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class LocalVar;

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

class QoreIRFunction;
class QoreIRBasicBlock;
class QoreIRInstruction;

class QoreIRToLLVM {
public:
    explicit QoreIRToLLVM(llvm::LLVMContext& context) : ctx(context) {
    }

    //! Lower an entire QoreIRFunction to an LLVM function in the given module.
    //! The generated function has signature: uint64_t fname(ExceptionSink* xsink)
    //! Returns the NaN-boxed QoreValue as uint64_t.
    bool lowerFunction(const QoreIRFunction& func, llvm::Module& module, std::string& error);

private:
    llvm::LLVMContext& ctx;

    // Type cache
    llvm::Type* i64_type = nullptr;
    llvm::Type* i1_type = nullptr;
    llvm::Type* double_type = nullptr;
    llvm::Type* ptr_type = nullptr;
    llvm::Type* void_type = nullptr;

    // The ExceptionSink* parameter for the current function
    llvm::Value* xsink_arg = nullptr;

    // IR builder
    std::unique_ptr<llvm::IRBuilder<>> builder;

    // Block mapping: QoreIR blocks → LLVM blocks
    std::unordered_map<const QoreIRBasicBlock*, llvm::BasicBlock*> block_map;

    // Value mapping: QoreIR value IDs → LLVM values
    std::unordered_map<uint32_t, llvm::Value*> values;

    // Local variable allocas (LocalVar* address → alloca)
    std::unordered_map<const void*, llvm::Value*> local_allocas;

    // Ordered list of unique LocalVar* pointers that need instantiation/uninstantiation
    std::vector<LocalVar*> function_locals;

    // Track which value IDs already contain NaN-boxed i64 (from Invoke, Call, CatchException,
    // make_string, .any ops, LoadLocal).  Values NOT in this set are raw typed values.
    std::unordered_set<uint32_t> nanboxed_values;

    // Set of LocalVar* (as void*) that are pre-instantiated by the caller (tiered
    // compilation); skip qore_rt_instantiate_local / qore_rt_uninstantiate_local for these.
    const std::unordered_set<const void*>* pre_instantiated_locals = nullptr;

    // Initialize types and helpers
    void initTypes();

    // Declare external runtime helper functions in the module
    void declareRuntimeHelpers(llvm::Module& module);

    // Lower a single instruction; returns false on unsupported opcode
    bool lowerInstruction(const QoreIRInstruction* inst, llvm::Function* llvm_func,
            llvm::Module& module, std::string& error);

    // Helpers to get/create LLVM values
    llvm::Value* getVal(uint32_t id, std::string& error);

    // NaN-boxing helpers: encode typed LLVM values into i64 QoreValue representation
    llvm::Value* boxInt(llvm::Value* int_val);
    llvm::Value* boxFloat(llvm::Value* float_val);
    llvm::Value* boxBool(llvm::Value* bool_val);
    llvm::Value* boxNothing();

    // Unboxing helpers: extract typed values from i64 QoreValue
    llvm::Value* unboxInt(llvm::Value* qv);
    llvm::Value* unboxFloat(llvm::Value* qv);
    llvm::Value* unboxBool(llvm::Value* qv);

    // Get a declared runtime helper function
    llvm::FunctionCallee getHelper(llvm::Module& module, const char* name, llvm::FunctionType* ft);

    // Collect all unique LocalVar* pointers from the IR function
    void collectLocals(const QoreIRFunction& func);

    // Emit qore_rt_instantiate_local calls for all function locals at current insert point
    void emitLocalInstantiation(llvm::Module& module);

    // Emit qore_rt_uninstantiate_local calls for all function locals at current insert point
    void emitLocalUninstantiation(llvm::Module& module);
};

#endif
