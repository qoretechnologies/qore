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
#include <llvm/ExecutionEngine/Orc/Debugging/DebuggerSupport.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/TargetParser/Host.h>
#include "qore/intern/QoreIRToLLVM.h"
#include "qore/intern/JITRuntime.h"

#include <cstring>

#include "qore/intern/QoreIR.h"
#include "qore/intern/Function.h"
#include "qore/intern/QoreIRInterpreter.h"

// Tiered compilation threshold defaults (overridable via QORE_IR_THRESHOLD / QORE_JIT_THRESHOLD env vars)
uint64_t QoreJIT::ir_threshold = []() -> uint64_t {
    const char* env = getenv("QORE_IR_THRESHOLD");
    return env ? strtoull(env, nullptr, 10) : 100;
}();
uint64_t QoreJIT::jit_threshold = []() -> uint64_t {
    const char* env = getenv("QORE_JIT_THRESHOLD");
    return env ? strtoull(env, nullptr, 10) : 1000;
}();

// JIT optimization level: default O2, overridable via QORE_JIT_OPT_LEVEL env var
int QoreJIT::jit_opt_level = -1;  // -1 = not yet initialized

int QoreJIT::getJITOptLevel() {
    if (jit_opt_level < 0) {
        const char* env = getenv("QORE_JIT_OPT_LEVEL");
        if (env) {
            int level = atoi(env);
            jit_opt_level = (level >= 0 && level <= 3) ? level : 2;
        } else {
            jit_opt_level = 2;
        }
    }
    return jit_opt_level;
}

//! Run LLVM optimization passes on a module before JIT compilation
static void optimizeModule(llvm::Module& module, int opt_level) {
    if (opt_level <= 0) {
        return;
    }

    llvm::OptimizationLevel llvm_opt;
    switch (opt_level) {
        case 1: llvm_opt = llvm::OptimizationLevel::O1; break;
        case 3: llvm_opt = llvm::OptimizationLevel::O3; break;
        default: llvm_opt = llvm::OptimizationLevel::O2; break;
    }

    // Create a target machine for the native target (needed by PassBuilder for target-specific opts)
    std::string triple = llvm::sys::getDefaultTargetTriple();
    std::string target_error;
    auto* target = llvm::TargetRegistry::lookupTarget(triple, target_error);
    if (!target) {
        return;  // Fall back to unoptimized if target lookup fails
    }
    auto* tm = target->createTargetMachine(triple, "generic", "",
        llvm::TargetOptions{}, llvm::Reloc::PIC_);
    if (!tm) {
        return;
    }

    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;
    llvm::PassBuilder PB(tm);
    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);
    auto MPM = PB.buildPerModuleDefaultPipeline(llvm_opt);
    MPM.run(module, MAM);

    delete tm;
}

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
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

#if LLVM_VERSION_MAJOR == 20 && defined(__aarch64__)
    // Workaround: LLVM 20's greedy/basic register allocators crash on certain
    // valid IR patterns on aarch64 (complex control flow with many phi nodes).
    // https://github.com/llvm/llvm-project/issues/181566
    // Use CodeGenOptLevel::None to select the fast register allocator instead.
    // IR-level optimizations are still applied separately via optimizeModule().
    auto jtmb = llvm::orc::JITTargetMachineBuilder::detectHost();
    if (!jtmb) {
        error = llvm::toString(jtmb.takeError());
        return false;
    }
    jtmb->setCodeGenOptLevel(llvm::CodeGenOptLevel::None);
    auto jit_or_err = llvm::orc::LLJITBuilder()
        .setJITTargetMachineBuilder(std::move(*jtmb))
        .create();
#else
    auto jit_or_err = llvm::orc::LLJITBuilder().create();
#endif
    if (!jit_or_err) {
        error = llvm::toString(jit_or_err.takeError());
        return false;
    }
    jit = std::move(*jit_or_err);

    // Phase 5c: Enable GDB/LLDB debugger support for JIT-compiled code
    // This registers JIT-compiled functions with the debugger via the GDB JIT interface,
    // allowing debuggers to see symbols and source locations in JIT-compiled code.
    // Requires GDB 7.0+ or LLDB with jit-loader.gdb plugin enabled.
    // Thread-safe: compile_mutex serializes all JIT compilations.
    if (auto err = llvm::orc::enableDebuggerSupport(*jit)) {
        // Non-fatal: JIT works fine without debugger support
        printd(1, "QoreJIT::init() WARNING: failed to enable debugger support: %s\n",
            llvm::toString(std::move(err)).c_str());
    }

    if (!registerRuntimeSymbols(error)) {
        jit.reset();
        return false;
    }

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
    addSymbol("qore_rt_throw_value", reinterpret_cast<void*>(&qore_rt_throw_value));
    addSymbol("qore_rt_has_exception", reinterpret_cast<void*>(&qore_rt_has_exception));

    // Invoke helpers
    addSymbol("qore_rt_invoke_expr", reinterpret_cast<void*>(&qore_rt_invoke_expr));
    addSymbol("qore_rt_make_string", reinterpret_cast<void*>(&qore_rt_make_string));
    addSymbol("qore_rt_catch_exception", reinterpret_cast<void*>(&qore_rt_catch_exception));
    addSymbol("qore_rt_catch_end", reinterpret_cast<void*>(&qore_rt_catch_end));
    addSymbol("qore_rt_rethrow", reinterpret_cast<void*>(&qore_rt_rethrow));

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

    // Generic opcode dispatch helpers
    addSymbol("qore_rt_binary_op", reinterpret_cast<void*>(&qore_rt_binary_op));
    addSymbol("qore_rt_unary_op", reinterpret_cast<void*>(&qore_rt_unary_op));
    addSymbol("qore_rt_expr_op", reinterpret_cast<void*>(&qore_rt_expr_op));
    addSymbol("qore_rt_comparison_op", reinterpret_cast<void*>(&qore_rt_comparison_op));
    addSymbol("qore_rt_ternary_op", reinterpret_cast<void*>(&qore_rt_ternary_op));

    // Variable access helpers
    addSymbol("qore_rt_load_global", reinterpret_cast<void*>(&qore_rt_load_global));
    addSymbol("qore_rt_store_global", reinterpret_cast<void*>(&qore_rt_store_global));
    addSymbol("qore_rt_load_closure", reinterpret_cast<void*>(&qore_rt_load_closure));
    addSymbol("qore_rt_store_closure", reinterpret_cast<void*>(&qore_rt_store_closure));
    addSymbol("qore_rt_load_thread_local", reinterpret_cast<void*>(&qore_rt_load_thread_local));
    addSymbol("qore_rt_store_thread_local", reinterpret_cast<void*>(&qore_rt_store_thread_local));

    // LValue operation helpers
    addSymbol("qore_rt_lvalue_load", reinterpret_cast<void*>(&qore_rt_lvalue_load));
    addSymbol("qore_rt_lvalue_store", reinterpret_cast<void*>(&qore_rt_lvalue_store));
    addSymbol("qore_rt_lvalue_unary", reinterpret_cast<void*>(&qore_rt_lvalue_unary));
    addSymbol("qore_rt_lvalue_binary", reinterpret_cast<void*>(&qore_rt_lvalue_binary));
    addSymbol("qore_rt_lvalue_ternary", reinterpret_cast<void*>(&qore_rt_lvalue_ternary));

    // Container construction helpers
    addSymbol("qore_rt_make_list", reinterpret_cast<void*>(&qore_rt_make_list));
    addSymbol("qore_rt_make_hash", reinterpret_cast<void*>(&qore_rt_make_hash));

    // Statement execution helpers
    addSymbol("qore_rt_exec_statement", reinterpret_cast<void*>(&qore_rt_exec_statement));
    addSymbol("qore_rt_thread_exit", reinterpret_cast<void*>(&qore_rt_thread_exit));

    // Guard type helper
    addSymbol("qore_rt_guard_type", reinterpret_cast<void*>(&qore_rt_guard_type));

    // Date construction helper
    addSymbol("qore_rt_make_date", reinterpret_cast<void*>(&qore_rt_make_date));

    // Specialized access helpers (Phase 5b)
    addSymbol("qore_rt_hash_key_access", reinterpret_cast<void*>(&qore_rt_hash_key_access));
    addSymbol("qore_rt_list_index_access", reinterpret_cast<void*>(&qore_rt_list_index_access));
    addSymbol("qore_rt_string_concat", reinterpret_cast<void*>(&qore_rt_string_concat));

    // Optimized list iteration helpers (higher-order optimization)
    addSymbol("qore_rt_list_size", reinterpret_cast<void*>(&qore_rt_list_size));
    addSymbol("qore_rt_list_get_int", reinterpret_cast<void*>(&qore_rt_list_get_int));
    addSymbol("qore_rt_list_get_float", reinterpret_cast<void*>(&qore_rt_list_get_float));

    // AOT context-based helpers (Phase 7b)
    addSymbol("qore_rt_load_local_aot", reinterpret_cast<void*>(&qore_rt_load_local_aot));
    addSymbol("qore_rt_assign_local_aot", reinterpret_cast<void*>(&qore_rt_assign_local_aot));
    addSymbol("qore_rt_instantiate_local_aot", reinterpret_cast<void*>(&qore_rt_instantiate_local_aot));
    addSymbol("qore_rt_uninstantiate_local_aot", reinterpret_cast<void*>(&qore_rt_uninstantiate_local_aot));
    addSymbol("qore_rt_load_global_aot", reinterpret_cast<void*>(&qore_rt_load_global_aot));
    addSymbol("qore_rt_store_global_aot", reinterpret_cast<void*>(&qore_rt_store_global_aot));
    addSymbol("qore_rt_load_thread_local_aot", reinterpret_cast<void*>(&qore_rt_load_thread_local_aot));
    addSymbol("qore_rt_store_thread_local_aot", reinterpret_cast<void*>(&qore_rt_store_thread_local_aot));
    addSymbol("qore_rt_load_closure_aot", reinterpret_cast<void*>(&qore_rt_load_closure_aot));
    addSymbol("qore_rt_store_closure_aot", reinterpret_cast<void*>(&qore_rt_store_closure_aot));
    addSymbol("qore_rt_invoke_expr_aot", reinterpret_cast<void*>(&qore_rt_invoke_expr_aot));
    addSymbol("qore_rt_lvalue_load_aot", reinterpret_cast<void*>(&qore_rt_lvalue_load_aot));
    addSymbol("qore_rt_lvalue_store_aot", reinterpret_cast<void*>(&qore_rt_lvalue_store_aot));
    addSymbol("qore_rt_lvalue_unary_aot", reinterpret_cast<void*>(&qore_rt_lvalue_unary_aot));
    addSymbol("qore_rt_lvalue_binary_aot", reinterpret_cast<void*>(&qore_rt_lvalue_binary_aot));
    addSymbol("qore_rt_lvalue_ternary_aot", reinterpret_cast<void*>(&qore_rt_lvalue_ternary_aot));
    addSymbol("qore_rt_push_on_block_exit_aot", reinterpret_cast<void*>(&qore_rt_push_on_block_exit_aot));

    // Fast call with explicit target (multi-function module compilation)
    addSymbol("qore_rt_call_fast_with_target", reinterpret_cast<void*>(&qore_rt_call_fast_with_target));

    // Call with pre-evaluated args helpers
    addSymbol("qore_rt_call_with_args", reinterpret_cast<void*>(&qore_rt_call_with_args));
    addSymbol("qore_rt_call_with_args_aot", reinterpret_cast<void*>(&qore_rt_call_with_args_aot));

    // DotEval with pre-evaluated base helpers
    addSymbol("qore_rt_dot_eval_with_base", reinterpret_cast<void*>(&qore_rt_dot_eval_with_base));
    addSymbol("qore_rt_dot_eval_with_base_aot", reinterpret_cast<void*>(&qore_rt_dot_eval_with_base_aot));

    // Regex with pre-evaluated operand helpers
    addSymbol("qore_rt_regex_op_with_operand", reinterpret_cast<void*>(&qore_rt_regex_op_with_operand));
    addSymbol("qore_rt_regex_op_with_operand_aot", reinterpret_cast<void*>(&qore_rt_regex_op_with_operand_aot));

    auto err = jd.define(llvm::orc::absoluteSymbols(std::move(symbols)));
    if (err) {
        error = "failed to register JIT runtime symbols: " + llvm::toString(std::move(err));
        return false;
    }

    symbols_registered = true;
    return true;
}

bool QoreJIT::compileFunction(const QoreIRFunction& func, std::string& error,
        void* deopt_counter) {
    // Thread-safe initialization using std::call_once
    std::call_once(init_flag, [this]() {
        init_success = initialize(init_error);
    });
    if (!init_success) {
        error = init_error;
        return false;
    }

    // Check if already compiled (fast path without compile lock)
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        if (compiled_functions.find(func.name) != compiled_functions.end()) {
            return true;
        }
    }

    // Serialize the entire compilation pipeline: LLVM's code generation (MCStreamer,
    // DWARF emission, debugger support) is not thread-safe for concurrent compilations.
    std::lock_guard<std::mutex> compile_lock(compile_mutex);

    return compileFunctionInternal(func, error, deopt_counter);
}

bool QoreJIT::compileFunctionLocked(const QoreIRFunction& func, std::string& error,
        void* deopt_counter) {
    // Thread-safe initialization using std::call_once
    std::call_once(init_flag, [this]() {
        init_success = initialize(init_error);
    });
    if (!init_success) {
        error = init_error;
        return false;
    }

    // Check if already compiled (fast path)
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        if (compiled_functions.find(func.name) != compiled_functions.end()) {
            return true;
        }
    }

    return compileFunctionInternal(func, error, deopt_counter);
}

bool QoreJIT::compileFunctionInternal(const QoreIRFunction& func, std::string& error,
        void* deopt_counter) {
    // Re-check cache under compile lock (another thread may have compiled this function)
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
    // NOTE: the lowering object must be destroyed before we transfer the module/context
    // to the JIT (addIRModule + lookup below), because QoreIRToLLVM holds an IRBuilder
    // with DebugLoc metadata references into the LLVMContext.  After addIRModule() +
    // lookup() trigger materialization, the JIT may free the context; destroying the
    // IRBuilder afterwards would crash in MetadataTracking::untrack().
    {
        QoreIRToLLVM lowering(*ctx);
        if (deopt_counter) {
            lowering.setDeoptCounter(deopt_counter);
        }
        if (!lowering.lowerFunction(func, *module, error)) {
            return false;
        }
    }

    // Run LLVM optimization passes
    optimizeModule(*module, getJITOptLevel());

    // Dump LLVM IR if requested (after optimization)
    if (getenv("QORE_DUMP_LLVM_IR")) {
        llvm::raw_fd_ostream llvm_dump(2, false);
        llvm_dump << "=== LLVM IR for " << func.name << " ===\n";
        module->print(llvm_dump, nullptr);
        llvm_dump << "=== END LLVM IR ===\n";
    }

    // Add the module to the JIT
    auto tsm = llvm::orc::ThreadSafeModule(std::move(module), std::move(ctx));
    auto err = jit->addIRModule(std::move(tsm));
    if (err) {
        error = "failed to add module to JIT: " + llvm::toString(std::move(err));
        return false;
    }

    // Look up the compiled function (triggers materialization/code generation inline)
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

bool QoreJIT::compileFunctionBatch(const QoreIRFunction& root_func, std::string& error,
        void* root_deopt_counter,
        const std::vector<BatchCallee>& callees) {
    // Thread-safe initialization using std::call_once
    std::call_once(init_flag, [this]() {
        init_success = initialize(init_error);
    });
    if (!init_success) {
        error = init_error;
        return false;
    }

    // Check if already compiled (fast path without compile lock)
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        if (compiled_functions.find(root_func.name) != compiled_functions.end()) {
            return true;
        }
    }

    // Serialize the entire compilation pipeline
    std::lock_guard<std::mutex> compile_lock(compile_mutex);

    return compileFunctionBatchInternal(root_func, error, root_deopt_counter, callees);
}

bool QoreJIT::compileFunctionBatchLocked(const QoreIRFunction& root_func, std::string& error,
        void* root_deopt_counter,
        const std::vector<BatchCallee>& callees) {
    // Thread-safe initialization using std::call_once
    std::call_once(init_flag, [this]() {
        init_success = initialize(init_error);
    });
    if (!init_success) {
        error = init_error;
        return false;
    }

    // Check if already compiled (fast path)
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        if (compiled_functions.find(root_func.name) != compiled_functions.end()) {
            return true;
        }
    }

    return compileFunctionBatchInternal(root_func, error, root_deopt_counter, callees);
}

bool QoreJIT::compileFunctionBatchInternal(const QoreIRFunction& root_func, std::string& error,
        void* root_deopt_counter,
        const std::vector<BatchCallee>& callees) {
    // Re-check cache under compile lock
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        if (compiled_functions.find(root_func.name) != compiled_functions.end()) {
            return true;
        }
    }

    // Create a shared LLVM context and module for the batch
    auto ctx = std::make_unique<llvm::LLVMContext>();
    auto module = std::make_unique<llvm::Module>("qore_jit_batch_" + root_func.name, *ctx);
    module->setDataLayout(jit->getDataLayout());

    // Build the batch callee map: variant → BatchCalleeInfo
    // This tells the lowerer which CallDirect targets are in-module
    // and whether they support direct LLVM arg passing (Approach B)
    std::unordered_map<const AbstractQoreFunctionVariant*, BatchCalleeInfo> batch_callee_map;
    for (const auto& callee : callees) {
        BatchCalleeInfo info;
        info.name = callee.ir_func->name;
        info.approach_b_eligible = callee.approach_b_eligible;
        info.num_params = callee.num_params;
        if (info.approach_b_eligible) {
            info.fast_name = callee.ir_func->name + "_fast";
        }
        batch_callee_map[callee.variant] = std::move(info);
    }

    // Determine which callees need their bodies compiled vs just forward-declared.
    // Callees already JIT-compiled just need a declaration (LLVM will resolve the
    // existing symbol); callees not yet compiled need full lowering.
    std::unordered_set<std::string> already_compiled;
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        for (const auto& callee : callees) {
            if (compiled_functions.find(callee.ir_func->name) != compiled_functions.end()) {
                already_compiled.insert(callee.ir_func->name);
            }
        }
    }

    // Forward-declare all callee functions in the module so they can be referenced
    // before their bodies are lowered.  All JIT functions share the same signature:
    // uint64_t fname(ExceptionSink* xsink)
    auto* i64_ty = llvm::Type::getInt64Ty(*ctx);
    auto* ptr_ty = llvm::PointerType::get(*ctx, 0);
    auto* fn_type = llvm::FunctionType::get(i64_ty, {ptr_ty}, false);
    for (const auto& callee : callees) {
        llvm::Function::Create(fn_type, llvm::Function::ExternalLinkage,
                callee.ir_func->name, *module);
    }

    // Forward-declare fast entry functions for Approach B eligible callees:
    // uint64_t callee_fast(i64 arg0, i64 arg1, ..., ptr xsink)
    for (const auto& callee : callees) {
        if (!callee.approach_b_eligible) {
            continue;
        }
        std::vector<llvm::Type*> fast_params(callee.num_params, i64_ty);
        fast_params.push_back(ptr_ty);  // xsink
        auto* fast_fn_type = llvm::FunctionType::get(i64_ty, fast_params, false);
        std::string fast_name = callee.ir_func->name + "_fast";
        llvm::Function* fast_fn = llvm::Function::Create(fast_fn_type,
                llvm::Function::ExternalLinkage, fast_name, *module);
        // Mark as InlineHint for LLVM's inliner
        fast_fn->addFnAttr(llvm::Attribute::InlineHint);
    }

    // Also forward-declare the root function
    llvm::Function::Create(fn_type, llvm::Function::ExternalLinkage,
            root_func.name, *module);

    // Create shared debug info for all functions in this batch module
    llvm::DIBuilder di_builder(*module);
    auto* di_file = di_builder.createFile("<jit-batch>", ".");
    auto* di_cu = di_builder.createCompileUnit(
        llvm::dwarf::DW_LANG_lo_user, di_file, "Qore JIT Batch", false, "", 0);
    if (!module->getModuleFlag("Dwarf Version")) {
        module->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 5);
    }
    if (!module->getModuleFlag("Debug Info Version")) {
        module->addModuleFlag(llvm::Module::Warning, "Debug Info Version",
                llvm::DEBUG_METADATA_VERSION);
    }

    // Lower bodies for callees that aren't already compiled
    for (const auto& callee : callees) {
        if (!already_compiled.count(callee.ir_func->name)) {
            // Lower standard entry (only if not already compiled)
            QoreIRToLLVM lowering(*ctx);
            if (callee.deopt_counter) {
                lowering.setDeoptCounter(callee.deopt_counter);
            }
            // Callees also get the batch map so they can call each other directly
            lowering.setBatchCallees(&batch_callee_map);
            lowering.setSharedDebugInfo(&di_builder, di_cu);
            if (!lowering.lowerFunction(*callee.ir_func, *module, error)) {
                // If a callee fails to lower, fall back to compiling root alone
                printd(2, "QoreJIT::compileFunctionBatch() callee '%s' lowering failed: %s\n",
                    callee.ir_func->name.c_str(), error.c_str());
                error.clear();
                return compileFunctionInternal(root_func, error, root_deopt_counter);
            }
        }

        // Lower fast entry for Approach B eligible callees (even if standard entry
        // is already compiled — the fast entry is new and needs its body)
        if (callee.approach_b_eligible) {
            std::string fast_name = callee.ir_func->name + "_fast";
            llvm::Function* fast_fn = module->getFunction(fast_name);
            assert(fast_fn && "fast entry function must be forward-declared");

            // Build param mapping: LocalVar* → LLVM function arg value
            const UserVariantBase* uvb = callee.variant->getUserVariantBase();
            const UserSignature* sig = uvb->getUserSignature();
            std::unordered_map<const void*, llvm::Value*> param_map;
            for (unsigned i = 0; i < callee.num_params; ++i) {
                const void* key = reinterpret_cast<const void*>(sig->lv[i]);
                param_map[key] = fast_fn->getArg(i);
                fast_fn->getArg(i)->setName(std::string("arg") + std::to_string(i));
            }

            QoreIRToLLVM fast_lowering(*ctx);
            if (callee.deopt_counter) {
                fast_lowering.setDeoptCounter(callee.deopt_counter);
            }
            fast_lowering.setBatchCallees(&batch_callee_map);
            fast_lowering.setSharedDebugInfo(&di_builder, di_cu);
            fast_lowering.setFastEntryMode(fast_name, &param_map);
            if (!fast_lowering.lowerFunction(*callee.ir_func, *module, error)) {
                // Fast entry failure is non-fatal: fall back to standard entry
                printd(2, "QoreJIT::compileFunctionBatch() fast entry '%s' lowering failed: %s\n",
                    fast_name.c_str(), error.c_str());
                error.clear();
                // Remove the fast entry from the batch map so callers don't try to use it
                auto map_it = batch_callee_map.find(callee.variant);
                if (map_it != batch_callee_map.end()) {
                    map_it->second.approach_b_eligible = false;
                }
            }
        }
    }

    // Lower the root function last (can call all callees)
    {
        QoreIRToLLVM lowering(*ctx);
        if (root_deopt_counter) {
            lowering.setDeoptCounter(root_deopt_counter);
        }
        lowering.setBatchCallees(&batch_callee_map);
        lowering.setSharedDebugInfo(&di_builder, di_cu);
        if (!lowering.lowerFunction(root_func, *module, error)) {
            return false;
        }
    }

    // Finalize shared debug info after all functions are lowered
    di_builder.finalize();

    // Run LLVM optimization passes
    optimizeModule(*module, getJITOptLevel());

    // Dump LLVM IR if requested (after optimization)
    if (getenv("QORE_DUMP_LLVM_IR")) {
        llvm::raw_fd_ostream llvm_dump(2, false);
        llvm_dump << "=== LLVM IR (batch) for " << root_func.name << " ===\n";
        module->print(llvm_dump, nullptr);
        llvm_dump << "=== END LLVM IR ===\n";
    }

    // Add the module to the JIT
    auto tsm = llvm::orc::ThreadSafeModule(std::move(module), std::move(ctx));
    auto err = jit->addIRModule(std::move(tsm));
    if (err) {
        error = "failed to add batch module to JIT: " + llvm::toString(std::move(err));
        return false;
    }

    // Look up the root function (triggers materialization/code generation)
    auto sym = jit->lookup(root_func.name);
    if (!sym) {
        error = "failed to look up compiled function '" + root_func.name + "': "
            + llvm::toString(sym.takeError());
        return false;
    }
    auto root_fn_ptr = sym->toPtr<uint64_t(ExceptionSink*)>();

    // Cache the root function
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        compiled_functions[root_func.name] = root_fn_ptr;
    }

    // Look up and cache all callee functions
    for (const auto& callee : callees) {
        auto callee_sym = jit->lookup(callee.ir_func->name);
        if (!callee_sym) {
            // Non-fatal: callees that fail lookup will fall back to runtime dispatch
            printd(2, "QoreJIT::compileFunctionBatch() callee '%s' lookup failed\n",
                callee.ir_func->name.c_str());
            continue;
        }
        auto callee_fn_ptr = callee_sym->toPtr<uint64_t(ExceptionSink*)>();
        {
            std::lock_guard<std::mutex> lock(cache_mutex);
            compiled_functions[callee.ir_func->name] = callee_fn_ptr;
        }
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

void QoreJIT::startBackgroundThread() {
    // Start the background compilation thread if not already running
    if (!bg_thread_running.exchange(true, std::memory_order_acq_rel)) {
        bg_compile_thread = std::thread(&QoreJIT::bgCompileThreadLoop, this);
    }
}

void QoreJIT::bgCompileThreadLoop() {
    // Background worker thread loop — compiles functions while main thread executes IR
    while (bg_thread_running.load(std::memory_order_acquire)) {
        BgCompileWork work;
        {
            std::unique_lock<std::mutex> lock(bg_queue_mutex);
            // Wait for work or shutdown signal
            bg_queue_cv.wait(lock, [this]() {
                return !bg_compile_queue.empty() || !bg_thread_running.load(std::memory_order_acquire);
            });
            if (bg_compile_queue.empty()) {
                // Signal that queue is empty (for waitForBgCompileQueue)
                bg_queue_empty_cv.notify_all();
                continue;  // Shutdown signal or spurious wakeup, check loop condition
            }
            work = bg_compile_queue.front();
            bg_compile_queue.pop();
        }

        // Acquire compile lock (blocking is OK here — this is a dedicated background thread)
        std::lock_guard<std::mutex> lock(compile_mutex);

        std::string error;
        bool success;

        if (getenv("QORE_JIT_TIMING")) {
            fprintf(stderr, "[BG-JIT] compiling '%s' on background thread\n", work.ir_func->name.c_str());
        }

        if (work.has_callees) {
            // Batch compile: root function + callees
            success = compileFunctionBatchInternal(*work.ir_func, error, work.deopt_ptr, work.callees);
        } else {
            // Single-function compilation
            success = compileFunctionInternal(*work.ir_func, error, work.deopt_ptr);
        }

        if (!success) {
            if (getenv("QORE_JIT_TIMING")) {
                fprintf(stderr, "[BG-JIT] failed to compile '%s': %s\n", work.ir_func->name.c_str(), error.c_str());
            }
            // Mark compilation as failed; uvb->jit_compile_failed will be set by evalTiered
            continue;
        }

        // Lookup the compiled function and register it
        JitFunctionPtr fn = lookupFunction(work.ir_func->name);
        if (!fn) {
            if (getenv("QORE_JIT_TIMING")) {
                fprintf(stderr, "[BG-JIT] lookup failed for '%s'\n", work.ir_func->name.c_str());
            }
            continue;
        }

        // Register the compiled function for the root uvb
        const UserVariantBase* root_uvb = work.uvb;
        if (root_uvb) {
            // Cast away const to set cached_jit_fn and tier
            UserVariantBase* mutable_uvb = const_cast<UserVariantBase*>(root_uvb);
            mutable_uvb->cached_jit_fn = fn;
            mutable_uvb->jit_compile_state.store(2, std::memory_order_relaxed);
            mutable_uvb->current_tier.store(UserVariantBase::TIER_JIT, std::memory_order_release);

            if (getenv("QORE_JIT_TIMING")) {
                fprintf(stderr, "[BG-JIT] compiled '%s' and promoted to JIT tier\n", work.ir_func->name.c_str());
            }
        }

        // Register compiled function pointers for batch-compiled callees
        if (work.has_callees) {
            for (const auto& callee : work.callees) {
                JitFunctionPtr callee_fn = lookupFunction(callee.ir_func->name);
                if (callee_fn && callee.variant) {
                    const UserVariantBase* callee_uvb = callee.variant->getUserVariantBase();
                    if (callee_uvb) {
                        UserVariantBase* mutable_callee = const_cast<UserVariantBase*>(callee_uvb);
                        mutable_callee->registerPrecompiledFunction(callee_fn);
                        if (getenv("QORE_JIT_TIMING")) {
                            fprintf(stderr, "[BG-JIT] batch-promoted callee '%s' to JIT tier\n",
                                    callee.ir_func->name.c_str());
                        }
                    }
                }
            }
        }

        // Signal that queue may now be empty (for waitForBgCompileQueue)
        {
            std::lock_guard<std::mutex> lock(bg_queue_mutex);
            if (bg_compile_queue.empty()) {
                bg_queue_empty_cv.notify_all();
            }
        }
    }
}

void QoreJIT::enqueueBgCompile(const UserVariantBase* uvb, const QoreIRFunction* ir_func,
        void* deopt_counter, const std::vector<BatchCallee>* callees) {
    // Ensure background thread is running
    startBackgroundThread();

    // Check if compilation was already enqueued (jit_compile_state is already 1)
    // to avoid duplicate work
    int expected = 0;
    if (!const_cast<UserVariantBase*>(uvb)->jit_compile_state.compare_exchange_strong(
            expected, 1, std::memory_order_acq_rel)) {
        // Already enqueued or completed
        return;
    }

    BgCompileWork work;
    work.uvb = uvb;
    work.ir_func = ir_func;
    work.deopt_ptr = deopt_counter;
    if (callees) {
        work.callees = *callees;
        work.has_callees = true;
    } else {
        work.has_callees = false;
    }

    {
        std::lock_guard<std::mutex> lock(bg_queue_mutex);
        bg_compile_queue.push(work);
    }
    bg_queue_cv.notify_one();

    if (getenv("QORE_JIT_TIMING")) {
        fprintf(stderr, "[BG-JIT] enqueued compilation of '%s'\n", ir_func->name.c_str());
    }
}

void QoreJIT::waitForBgCompileQueue() {
    // Wait for all pending compilations to complete using condition variable
    std::unique_lock<std::mutex> lock(bg_queue_mutex);
    bg_queue_empty_cv.wait(lock, [this]() {
        return bg_compile_queue.empty();
    });
}

void QoreJIT::shutdown() {
    // Ensure background thread is stopped BEFORE any other shutdown
    bool was_running = bg_thread_running.exchange(false, std::memory_order_acq_rel);
    if (was_running) {
        // Signal the thread to wake up and exit
        {
            std::lock_guard<std::mutex> lock(bg_queue_mutex);
        }  // Release lock before notify
        bg_queue_cv.notify_one();
        // Join the background thread (it should exit quickly since bg_thread_running is false)
        if (bg_compile_thread.joinable()) {
            bg_compile_thread.join();
        }
    }

    // Then shut down LLVM (acquire compile_mutex to serialize with any ongoing compilation)
    std::lock_guard<std::mutex> compile_lock(compile_mutex);
    jit.reset();
    symbols_registered = false;
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        compiled_functions.clear();
    }
    init_success = false;
}

bool QoreJIT::tryAcquireCompileLock() {
    return compile_mutex.try_lock();
}

void QoreJIT::releaseCompileLock() {
    compile_mutex.unlock();
}
