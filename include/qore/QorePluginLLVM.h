/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QorePluginLLVM.h

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
*/

#ifndef _QORE_QOREPLUGINLLVM_H
#define _QORE_QOREPLUGINLLVM_H

#include <qore/QorePluginType.h>

#include <llvm/Config/llvm-config.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

#include <cstdint>

#define QORE_PLUGIN_LLVM_CODEGEN_EXTENSION_ID "qore.plugin.llvm.codegen"
#define QORE_PLUGIN_LLVM_EXTENSION_ABI_VERSION 1
#define QORE_PLUGIN_LLVM_CURRENT_MAJOR LLVM_VERSION_MAJOR

//! Code-generation context passed to QORE_PLUGIN_LLVM_CODEGEN_EXTENSION_ID callbacks.
/** This struct is LLVM-major-version-coupled by design. Modules using it must
    build against the same LLVM major version as libqore and should set
    QorePluginLLVMExtension::llvm_major_version to QORE_PLUGIN_LLVM_CURRENT_MAJOR.

    Pointers in this struct are borrowed from the active QoreIRToLLVM lowering
    call and are valid only for the duration of the callback.
*/
struct QorePluginLLVMCodegenContext {
    //! Size of this struct as known to the caller.
    uint32_t struct_size;
    //! LLVM extension ABI version; must be QORE_PLUGIN_LLVM_EXTENSION_ABI_VERSION.
    uint32_t abi_version;
    //! Process-global operation id currently being lowered.
    uint32_t global_operation_id;
    //! Helper ABI declared by the registered operation signature.
    QorePluginHelperAbi helper_abi;
    //! Registered operation signature.
    const QorePluginOperationSignature* signature;
    //! Active LLVM context.
    llvm::LLVMContext* llvm_context;
    //! Active IR builder positioned at the plugin operation insertion point.
    llvm::IRBuilder<>* builder;
    //! Active LLVM module.
    llvm::Module* module;
    //! Active LLVM function.
    llvm::Function* function;
    //! Current ExceptionSink* LLVM value.
    llvm::Value* xsink_value;
    //! LLVM i64 type used for boxed QoreValue bit patterns.
    llvm::Type* qore_value_type;
    //! LLVM pointer type used by libqore lowering.
    llvm::Type* pointer_type;
    //! Boxed QoreValue operands in IR operand order.
    llvm::Value* const* boxed_operands;
    //! Number of entries in boxed_operands.
    uint32_t num_operands;
    //! Optional diagnostic pointer set by the callback on failure.
    const char** error_message;
};

//! Optional LLVM codegen callback for a registered plugin operation.
/** The callback returns an i64 QoreValue bit-pattern on success. Returning
    nullptr indicates a codegen failure; set
    QorePluginLLVMCodegenContext::error_message to provide a diagnostic.
    libqore uses the operation's runtime-helper fallback only when no valid
    LLVM codegen extension is registered.
*/
typedef llvm::Value* (*QorePluginLLVMCodegenCallback)(QorePluginLLVMCodegenContext* ctx);

//! Payload for the QORE_PLUGIN_LLVM_CODEGEN_EXTENSION_ID operation extension.
struct QorePluginLLVMExtension {
    //! Size of this struct; must be at least sizeof(QorePluginLLVMExtension).
    uint32_t struct_size;
    //! LLVM extension ABI version; must be QORE_PLUGIN_LLVM_EXTENSION_ABI_VERSION.
    uint32_t abi_version;
    //! LLVM major version used by the module; must match QORE_PLUGIN_LLVM_CURRENT_MAJOR.
    uint32_t llvm_major_version;
    //! Operation-specific LLVM codegen callback.
    QorePluginLLVMCodegenCallback codegen;
};

#endif
