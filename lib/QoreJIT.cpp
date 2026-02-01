/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreJIT.cpp

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

#include "qore/intern/QoreJIT.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include "qore/intern/QoreIRToLLVM.h"
#include "qore/intern/JITRuntime.h"

#include <cstring>

#include "qore/intern/QoreIR.h"
#include "qore/intern/QoreIRInterpreter.h"

// Tiered compilation threshold defaults
uint64_t QoreJIT::ir_threshold = 100;
uint64_t QoreJIT::jit_threshold = 1000;

QoreJIT& QoreJIT::instance() {
    static QoreJIT jit;
    return jit;
}

bool QoreJIT::isEnabled() const {
    return true;
}

bool QoreJIT::canJit(int64 parse_options, std::string& reason) const {
    if ((parse_options & PO_MODERN) != PO_MODERN) {
        reason = "requires %modern (PO_MODERN)";
        return false;
    }
    return true;
}

bool QoreJIT::initialize(std::string& error) {
    if (initialized) {
        return true;
    }
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    auto jit_or_err = llvm::orc::LLJITBuilder().create();
    if (!jit_or_err) {
        error = llvm::toString(jit_or_err.takeError());
        return false;
    }
    jit = std::move(*jit_or_err);

    if (!registerRuntimeSymbols(error)) {
        jit.reset();
        return false;
    }

    initialized = true;
    return true;
}

bool QoreJIT::registerRuntimeSymbols(std::string& error) {
    if (symbols_registered) {
        return true;
    }

    auto& es = jit->getExecutionSession();
    auto& dl = jit->getDataLayout();
    auto& jd = jit->getMainJITDylib();

    // Build symbol map for all C ABI runtime helpers
    llvm::orc::SymbolMap symbols;

    auto addSymbol = [&](const char* name, void* addr) {
        auto flags = llvm::JITSymbolFlags::Exported | llvm::JITSymbolFlags::Callable;
        symbols[es.intern(name)] = {llvm::orc::ExecutorAddr::fromPtr(addr), flags};
    };

    // Arithmetic helpers
    addSymbol("qore_rt_add_any", reinterpret_cast<void*>(&qore_rt_add_any));
    addSymbol("qore_rt_sub_any", reinterpret_cast<void*>(&qore_rt_sub_any));
    addSymbol("qore_rt_mul_any", reinterpret_cast<void*>(&qore_rt_mul_any));
    addSymbol("qore_rt_div_any", reinterpret_cast<void*>(&qore_rt_div_any));
    addSymbol("qore_rt_mod_any", reinterpret_cast<void*>(&qore_rt_mod_any));
    addSymbol("qore_rt_div_int", reinterpret_cast<void*>(&qore_rt_div_int));
    addSymbol("qore_rt_mod_int", reinterpret_cast<void*>(&qore_rt_mod_int));
    addSymbol("qore_rt_div_float", reinterpret_cast<void*>(&qore_rt_div_float));

    // Conversion helpers
    addSymbol("qore_rt_to_int", reinterpret_cast<void*>(&qore_rt_to_int));
    addSymbol("qore_rt_to_float", reinterpret_cast<void*>(&qore_rt_to_float));
    addSymbol("qore_rt_to_bool", reinterpret_cast<void*>(&qore_rt_to_bool));

    // Refcount helpers
    addSymbol("qore_rt_incref", reinterpret_cast<void*>(&qore_rt_incref));
    addSymbol("qore_rt_decref", reinterpret_cast<void*>(&qore_rt_decref));
    addSymbol("qore_rt_decref_nothrow", reinterpret_cast<void*>(&qore_rt_decref_nothrow));

    // Exception helpers
    addSymbol("qore_rt_throw", reinterpret_cast<void*>(&qore_rt_throw));
    addSymbol("qore_rt_has_exception", reinterpret_cast<void*>(&qore_rt_has_exception));

    // Invoke helpers
    addSymbol("qore_rt_invoke_expr", reinterpret_cast<void*>(&qore_rt_invoke_expr));
    addSymbol("qore_rt_make_string", reinterpret_cast<void*>(&qore_rt_make_string));
    addSymbol("qore_rt_catch_exception", reinterpret_cast<void*>(&qore_rt_catch_exception));

    // Deopt helpers
    addSymbol("qore_rt_deopt", reinterpret_cast<void*>(&qore_rt_deopt));

    // Guard helpers
    addSymbol("qore_rt_guard_not_nothing", reinterpret_cast<void*>(&qore_rt_guard_not_nothing));
    addSymbol("qore_rt_guard_int", reinterpret_cast<void*>(&qore_rt_guard_int));
    addSymbol("qore_rt_guard_float", reinterpret_cast<void*>(&qore_rt_guard_float));

    // Local variable helpers
    addSymbol("qore_rt_instantiate_local", reinterpret_cast<void*>(&qore_rt_instantiate_local));
    addSymbol("qore_rt_assign_local", reinterpret_cast<void*>(&qore_rt_assign_local));
    addSymbol("qore_rt_load_local", reinterpret_cast<void*>(&qore_rt_load_local));
    addSymbol("qore_rt_uninstantiate_local", reinterpret_cast<void*>(&qore_rt_uninstantiate_local));

    auto err = jd.define(llvm::orc::absoluteSymbols(std::move(symbols)));
    if (err) {
        error = "failed to register JIT runtime symbols: " + llvm::toString(std::move(err));
        return false;
    }

    symbols_registered = true;
    return true;
}

bool QoreJIT::compileFunction(const QoreIRFunction& func, std::string& error) {
    if (!initialized && !initialize(error)) {
        return false;
    }

    // Check if already compiled
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        if (compiled_functions.find(func.name) != compiled_functions.end()) {
            return true;
        }
    }

    // Create a new LLVM context and module for this compilation
    auto ctx = std::make_unique<llvm::LLVMContext>();
    auto module = std::make_unique<llvm::Module>("qore_jit_" + func.name, *ctx);
    module->setDataLayout(jit->getDataLayout());

    // Lower IR to LLVM IR
    QoreIRToLLVM lowering(*ctx);
    if (!lowering.lowerFunction(func, *module, error)) {
        return false;
    }

    // Add the module to the JIT
    auto tsm = llvm::orc::ThreadSafeModule(std::move(module), std::move(ctx));
    auto err = jit->addIRModule(std::move(tsm));
    if (err) {
        error = "failed to add module to JIT: " + llvm::toString(std::move(err));
        return false;
    }

    // Look up the compiled function
    auto sym = jit->lookup(func.name);
    if (!sym) {
        error = "failed to look up compiled function '" + func.name + "': " + llvm::toString(sym.takeError());
        return false;
    }

    auto fn_ptr = sym->toPtr<uint64_t(ExceptionSink*)>();

    // Cache the compiled function pointer
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        compiled_functions[func.name] = fn_ptr;
    }

    return true;
}

JitFunctionPtr QoreJIT::lookupFunction(const std::string& name) const {
    std::lock_guard<std::mutex> lock(cache_mutex);
    auto it = compiled_functions.find(name);
    if (it == compiled_functions.end()) {
        return nullptr;
    }
    return it->second;
}

bool QoreJIT::executeWithFallback(const QoreIRFunction& func, QoreValue& return_value, ExceptionSink* xsink,
        std::string& error, const std::unordered_set<const LocalVar*>* pre_instantiated) {
    if (compileFunction(func, error)) {
        JitFunctionPtr fn = lookupFunction(func.name);
        if (fn) {
            // Execute the JIT-compiled function
            uint64_t result_bits = fn(xsink);
            // Reconstruct QoreValue from NaN-boxed bits
            QoreValue result;
            std::memcpy(&result, &result_bits, sizeof(result));
            return_value = result;
            return true;
        }
    }
    // Fallback to IR interpreter
    if (deopt_policy == DeoptPolicy::DisableJit) {
        return false;
    }
    return QoreIRInterpreter::execute(func, return_value, xsink, nullptr, nullptr, nullptr,
        pre_instantiated);
}

void QoreJIT::setDeoptPolicy(DeoptPolicy policy) {
    deopt_policy = policy;
}

uint64_t QoreJIT::getIRThreshold() {
    return ir_threshold;
}

uint64_t QoreJIT::getJITThreshold() {
    return jit_threshold;
}

void QoreJIT::setIRThreshold(uint64_t t) {
    ir_threshold = t;
}

void QoreJIT::setJITThreshold(uint64_t t) {
    jit_threshold = t;
}

void QoreJIT::shutdown() {
    jit.reset();
    symbols_registered = false;
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        compiled_functions.clear();
    }
    initialized = false;
}
