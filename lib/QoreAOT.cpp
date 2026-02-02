/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreAOT.cpp

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

#include "qore/intern/QoreAOT.h"

#include <qore/Qore.h>

#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/FunctionList.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/QoreIR.h"
#include "qore/intern/QoreIRBuilder.h"
#include "qore/intern/QoreIRLowering.h"
#include "qore/intern/QoreIRVerifier.h"
#include "qore/intern/QoreIRToLLVM.h"
#include "qore/intern/StatementBlock.h"
#include "qore/intern/Function.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

//! Descriptor for a function that was successfully compiled to LLVM IR
struct AOTCompiledFunc {
    std::string name;               //!< function name (e.g. "myFunc", "MyClass::myMethod")
    std::string llvm_symbol;        //!< LLVM symbol name in the module
};

//! Check if a QoreIRFunction contains opcodes that embed process-specific pointers
//! Functions with Invoke opcodes embed AST node pointers as immediate constants.
//! Functions with LoadLocal/StoreLocal/LoadGlobal/StoreGlobal/LoadClosure/StoreClosure/
//! LoadThreadLocal/StoreThreadLocal embed LocalVar/Var pointers as immediate constants.
//! These pointers are process-specific and invalid in AOT-compiled object files.
//! @param func the IR function to check
//! @return true if the function has process-specific pointer opcodes, false if AOT-safe
static bool hasProcessSpecificOpcodes(const QoreIRFunction& func) {
    for (auto& block : func.blocks) {
        for (auto& inst : block->instructions) {
            switch (inst->opcode) {
                case QoreIROpcode::Invoke:
                case QoreIROpcode::LoadLocal:
                case QoreIROpcode::StoreLocal:
                case QoreIROpcode::LoadGlobal:
                case QoreIROpcode::StoreGlobal:
                case QoreIROpcode::LoadClosure:
                case QoreIROpcode::StoreClosure:
                case QoreIROpcode::LoadThreadLocal:
                case QoreIROpcode::StoreThreadLocal:
                case QoreIROpcode::LoadLValue:
                case QoreIROpcode::StoreLValue:
                case QoreIROpcode::PreIncLValue:
                case QoreIROpcode::PreDecLValue:
                case QoreIROpcode::PostIncLValue:
                case QoreIROpcode::PostDecLValue:
                case QoreIROpcode::Call:
                case QoreIROpcode::CallMethod:
                case QoreIROpcode::CallStatic:
                case QoreIROpcode::CallIndirect:
                    return true;
                default:
                    break;
            }
        }
    }
    return false;
}

//! Try to lower a user function variant to IR and check for Invoke opcodes
/** @param uvb the user variant base
    @param name the function name for IR lowering
    @param pgm the QoreProgram
    @param ir_func output: the lowered IR function (caller must delete)
    @param error output: error message on failure
    @return 0 = invoke-free (compiled), 1 = has invoke (skip), -1 = lowering failed
*/
static int tryLowerFunction(UserVariantBase* uvb, const char* name, QoreProgram* pgm,
        QoreIRFunction*& ir_func, std::string& error) {
    StatementBlock* statements = uvb->getStatementBlock();
    if (!statements) {
        error = "no statement block";
        return -1;
    }

    ir_func = new QoreIRFunction(name);

    // Record pre-instantiated locals from signature
    UserSignature* sig = uvb->getUserSignature();
    if (sig) {
        for (unsigned i = 0; i < sig->numParams(); ++i) {
            ir_func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(sig->lv[i]));
        }
        if (sig->argvid) {
            ir_func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(sig->argvid));
        }
        if (sig->selfid) {
            ir_func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(sig->selfid));
        }
    }

    QoreIRBuilder builder(ir_func);
    auto* entry = ir_func->createBlock("entry");
    builder.setBlock(entry);

    QoreParseContext parse_context(pgm);
    QoreIRLowering lowering(builder, &parse_context);
    if (!lowering.lowerStatementBlock(statements, error)) {
        delete ir_func;
        ir_func = nullptr;
        return -1;
    }
    // Ensure terminator
    if (ir_func->blocks.back()->instructions.empty() ||
            (ir_func->blocks.back()->instructions.back()->opcode != QoreIROpcode::Return &&
             ir_func->blocks.back()->instructions.back()->opcode != QoreIROpcode::ReturnNothing &&
             ir_func->blocks.back()->instructions.back()->opcode != QoreIROpcode::Br &&
             ir_func->blocks.back()->instructions.back()->opcode != QoreIROpcode::Rethrow)) {
        builder.createReturnNothing();
    }
    if (!QoreIRVerifier::verify(*ir_func, error)) {
        delete ir_func;
        ir_func = nullptr;
        return -1;
    }

    if (hasProcessSpecificOpcodes(*ir_func)) {
        delete ir_func;
        ir_func = nullptr;
        return 1;  // has process-specific pointers — defer to runtime JIT
    }

    return 0;  // AOT-safe — can be pre-compiled
}

//! Walk a namespace and compile all Invoke-free user functions to LLVM IR
static void compileNamespaceFunctions(qore_ns_private* ns, QoreProgram* pgm,
        llvm::LLVMContext& ctx, llvm::Module& module,
        std::vector<AOTCompiledFunc>& compiled_funcs,
        int& total_funcs, int& compiled_count, int& invoke_count, int& failed_count) {
    // Walk functions
    for (auto i = ns->func_list.begin(), e = ns->func_list.end(); i != e; ++i) {
        FunctionEntry* fe = i->second;
        QoreFunction* func = fe->getFunction();
        if (!func) {
            continue;
        }

        QoreFunctionIterator vit(*func);
        while (vit.next()) {
            const AbstractQoreFunctionVariant* variant = vit.getVariant();
            UserVariantBase* uvb = const_cast<AbstractQoreFunctionVariant*>(variant)->getUserVariantBase();
            if (!uvb || !uvb->hasBody()) {
                continue;
            }

            ++total_funcs;
            const char* fname = func->getName();
            QoreIRFunction* ir_func = nullptr;
            std::string lower_error;
            int rc = tryLowerFunction(uvb, fname, pgm, ir_func, lower_error);

            if (rc == 0 && ir_func) {
                // Invoke-free: lower to LLVM
                QoreIRToLLVM lowerer(ctx);
                std::string llvm_error;
                if (lowerer.lowerFunction(*ir_func, module, llvm_error)) {
                    compiled_funcs.push_back({fname, ir_func->name});
                    ++compiled_count;
                    printd(2, "AOT: compiled function '%s' to LLVM IR\n", fname);
                } else {
                    printd(2, "AOT: LLVM lowering failed for '%s': %s\n", fname, llvm_error.c_str());
                    ++failed_count;
                }
                delete ir_func;
            } else if (rc == 1) {
                printd(2, "AOT: function '%s' has process-specific opcodes, deferring to runtime JIT\n", fname);
                ++invoke_count;
            } else {
                printd(2, "AOT: IR lowering failed for '%s': %s\n", fname, lower_error.c_str());
                ++failed_count;
            }
        }
    }

    // Walk classes
    ClassListIterator cli(ns->classList);
    while (cli.next()) {
        QoreClass* qc = cli.get();
        if (!qc) {
            continue;
        }
        qore_class_private* qcp = qore_class_private::get(*qc);
        const char* class_name = qc->getName();

        // Helper lambda for method iteration
        auto processMethod = [&](QoreMethod* meth) {
            if (!meth->isUser()) {
                return;
            }
            qore_method_private* mp = qore_method_private::get(*meth);
            MethodFunctionBase* mfb = mp->getFunction();

            QoreFunctionIterator vit(*mfb);
            while (vit.next()) {
                const AbstractQoreFunctionVariant* variant = vit.getVariant();
                UserVariantBase* uvb = const_cast<AbstractQoreFunctionVariant*>(variant)->getUserVariantBase();
                if (!uvb || !uvb->hasBody()) {
                    continue;
                }

                ++total_funcs;
                std::string method_name = std::string(class_name) + "::" + meth->getName();
                QoreIRFunction* ir_func = nullptr;
                std::string lower_error;
                int rc = tryLowerFunction(uvb, method_name.c_str(), pgm, ir_func, lower_error);

                if (rc == 0 && ir_func) {
                    QoreIRToLLVM lowerer(ctx);
                    std::string llvm_error;
                    if (lowerer.lowerFunction(*ir_func, module, llvm_error)) {
                        compiled_funcs.push_back({method_name, ir_func->name});
                        ++compiled_count;
                        printd(2, "AOT: compiled method '%s' to LLVM IR\n", method_name.c_str());
                    } else {
                        printd(2, "AOT: LLVM lowering failed for '%s': %s\n",
                            method_name.c_str(), llvm_error.c_str());
                        ++failed_count;
                    }
                    delete ir_func;
                } else if (rc == 1) {
                    printd(2, "AOT: method '%s' has process-specific opcodes, deferring to runtime JIT\n", method_name.c_str());
                    ++invoke_count;
                } else {
                    printd(2, "AOT: IR lowering failed for '%s': %s\n",
                        method_name.c_str(), lower_error.c_str());
                    ++failed_count;
                }
            }
        };

        // Instance methods
        for (auto mi = qcp->hm.begin(), me = qcp->hm.end(); mi != me; ++mi) {
            processMethod(mi->second);
        }
        // Static methods
        for (auto mi = qcp->shm.begin(), me = qcp->shm.end(); mi != me; ++mi) {
            processMethod(mi->second);
        }
    }

    // Recurse into child namespaces
    for (auto ni = ns->nsl.nsmap.begin(), ne = ns->nsl.nsmap.end(); ni != ne; ++ni) {
        QoreNamespace* child_ns = ni->second;
        if (child_ns) {
            compileNamespaceFunctions(qore_ns_private::get(*child_ns), pgm, ctx, module,
                compiled_funcs, total_funcs, compiled_count, invoke_count, failed_count);
        }
    }
}

//! Emit an LLVM module to a native object file
static bool emitObjectFile(llvm::Module& module, const std::string& path, std::string& error) {
    auto triple = llvm::sys::getDefaultTargetTriple();
    module.setTargetTriple(triple);

    std::string target_error;
    auto* target = llvm::TargetRegistry::lookupTarget(triple, target_error);
    if (!target) {
        error = "failed to look up target: " + target_error;
        return false;
    }

    auto* tm = target->createTargetMachine(triple, "generic", "",
        llvm::TargetOptions{}, llvm::Reloc::PIC_);
    if (!tm) {
        error = "failed to create target machine";
        return false;
    }
    module.setDataLayout(tm->createDataLayout());

    // Run optimization passes (O2)
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
    auto MPM = PB.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);
    MPM.run(module, MAM);

    // Emit object file
    std::error_code EC;
    llvm::raw_fd_ostream dest(path, EC, llvm::sys::fs::OF_None);
    if (EC) {
        error = "failed to open output file: " + EC.message();
        delete tm;
        return false;
    }
    llvm::legacy::PassManager emit_pm;
    if (tm->addPassesToEmitFile(emit_pm, dest, nullptr, llvm::CodeGenFileType::ObjectFile)) {
        error = "target machine cannot emit object files";
        delete tm;
        return false;
    }
    emit_pm.run(module);
    dest.flush();
    delete tm;
    return true;
}

//! Link an object file into a standalone executable
static bool linkExecutable(const std::string& obj_path, const std::string& exe_path, std::string& error) {
    // Try the build directory first (for development builds), then installed libqore
    std::string libqore_dir;

    // Check for override via environment variable (for development builds)
    const char* qore_prefix = getenv("QORE_LIBDIR");
    if (qore_prefix) {
        libqore_dir = qore_prefix;
    } else {
        // Default to the system library path where libqore is installed
        libqore_dir = QORE_LIBDIR;
    }

    std::string cmd = "cc -o " + exe_path + " " + obj_path
        + " -L" + libqore_dir + " -lqore"
        + " -Wl,-rpath," + libqore_dir
        + " -lstdc++ -lpthread -ldl";

    printd(2, "AOT: link command: %s\n", cmd.c_str());
    int rc = system(cmd.c_str());
    if (rc != 0) {
        error = "linker command failed with exit code " + std::to_string(rc);
        return false;
    }
    return true;
}

//! Generate the LLVM IR for main() and the function registration table
static void generateMainAndTable(llvm::LLVMContext& ctx, llvm::Module& module,
        const char* source, int source_len, const char* label,
        int64_t parse_options, const std::vector<AOTCompiledFunc>& compiled_funcs) {
    // Types
    auto* i8_type = llvm::Type::getInt8Ty(ctx);
    auto* i32_type = llvm::Type::getInt32Ty(ctx);
    auto* i64_type = llvm::Type::getInt64Ty(ctx);
    auto* ptr_type = llvm::PointerType::get(ctx, 0);

    // Embed source as a global constant
    llvm::Constant* source_data = llvm::ConstantDataArray::getString(ctx,
        llvm::StringRef(source, source_len), false);
    auto* source_gv = new llvm::GlobalVariable(module,
        source_data->getType(), true, llvm::GlobalValue::PrivateLinkage,
        source_data, "qore_aot_source");

    // Embed label as a global constant
    llvm::Constant* label_data = llvm::ConstantDataArray::getString(ctx,
        llvm::StringRef(label), true);  // null-terminated
    auto* label_gv = new llvm::GlobalVariable(module,
        label_data->getType(), true, llvm::GlobalValue::PrivateLinkage,
        label_data, "qore_aot_label");

    // Build the function table: array of {i8*, i8*} (name, fn_ptr)
    // QoreAOTFunc struct: { const char* name, uint64_t (*fn_ptr)(ExceptionSink*) }
    auto* func_entry_type = llvm::StructType::get(ctx, {ptr_type, ptr_type});

    std::vector<llvm::Constant*> func_entries;
    for (auto& cf : compiled_funcs) {
        // Create global string for function name
        llvm::Constant* name_str = llvm::ConstantDataArray::getString(ctx, cf.name, true);
        auto* name_gv = new llvm::GlobalVariable(module,
            name_str->getType(), true, llvm::GlobalValue::PrivateLinkage,
            name_str, "qore_aot_fname_" + cf.llvm_symbol);

        // Get the compiled function
        llvm::Function* fn = module.getFunction(cf.llvm_symbol);
        if (!fn) {
            // Function was compiled but might have a different symbol name
            printd(1, "AOT: warning: compiled function '%s' not found as symbol '%s' in module\n",
                cf.name.c_str(), cf.llvm_symbol.c_str());
            continue;
        }

        llvm::Constant* entry = llvm::ConstantStruct::get(func_entry_type, {
            name_gv,
            fn
        });
        func_entries.push_back(entry);
    }

    // Create the function table global
    llvm::Constant* func_table_init;
    llvm::GlobalVariable* func_table_gv;
    int num_funcs = (int)func_entries.size();

    if (num_funcs > 0) {
        auto* table_type = llvm::ArrayType::get(func_entry_type, num_funcs);
        func_table_init = llvm::ConstantArray::get(table_type, func_entries);
        func_table_gv = new llvm::GlobalVariable(module,
            table_type, true, llvm::GlobalValue::PrivateLinkage,
            func_table_init, "qore_aot_funcs");
    } else {
        func_table_gv = nullptr;
    }

    // Declare qore_aot_run
    auto* aot_run_type = llvm::FunctionType::get(i32_type, {
        i32_type,       // argc
        ptr_type,       // argv
        ptr_type,       // source
        i32_type,       // source_len
        ptr_type,       // label
        i64_type,       // parse_options
        ptr_type,       // functions
        i32_type        // num_functions
    }, false);
    auto aot_run_fn = module.getOrInsertFunction("qore_aot_run", aot_run_type);

    // Generate main()
    auto* main_type = llvm::FunctionType::get(i32_type, {i32_type, ptr_type}, false);
    auto* main_fn = llvm::Function::Create(main_type, llvm::Function::ExternalLinkage, "main", module);

    auto* entry_bb = llvm::BasicBlock::Create(ctx, "entry", main_fn);
    llvm::IRBuilder<> builder(entry_bb);

    auto arg_it = main_fn->arg_begin();
    llvm::Value* argc_val = &*arg_it++;
    llvm::Value* argv_val = &*arg_it;

    // GEP to get pointers to source and label
    llvm::Value* src_ptr = builder.CreateInBoundsGEP(
        source_data->getType(), source_gv,
        {builder.getInt64(0), builder.getInt64(0)});
    llvm::Value* lbl_ptr = builder.CreateInBoundsGEP(
        label_data->getType(), label_gv,
        {builder.getInt64(0), builder.getInt64(0)});

    llvm::Value* funcs_ptr;
    if (func_table_gv) {
        auto* table_type = llvm::ArrayType::get(func_entry_type, num_funcs);
        funcs_ptr = builder.CreateBitCast(
            builder.CreateInBoundsGEP(table_type, func_table_gv,
                {builder.getInt64(0), builder.getInt64(0)}),
            ptr_type);
    } else {
        funcs_ptr = llvm::ConstantPointerNull::get(llvm::PointerType::get(ctx, 0));
    }

    llvm::Value* rc = builder.CreateCall(aot_run_fn, {
        argc_val,
        argv_val,
        src_ptr,
        builder.getInt32(source_len),
        lbl_ptr,
        builder.getInt64(parse_options),
        funcs_ptr,
        builder.getInt32(num_funcs)
    });

    builder.CreateRet(rc);
}

bool QoreAOT::compile(QoreProgram* pgm,
                      const char* source_text, int source_len,
                      const char* label,
                      const std::string& output_path,
                      int64_t parse_options,
                      std::string& error) {
    // Initialize LLVM targets (needed for object emission)
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    // Create LLVM context and module
    llvm::LLVMContext ctx;
    auto module = std::make_unique<llvm::Module>("qore_aot_module", ctx);

    // Step 1: Enumerate and compile Invoke-free functions
    std::vector<AOTCompiledFunc> compiled_funcs;
    int total_funcs = 0;
    int compiled_count = 0;
    int invoke_count = 0;
    int failed_count = 0;

    qore_program_private* pp = qore_program_private::get(*pgm);
    qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);
    compileNamespaceFunctions(root_ns, pgm, ctx, *module,
        compiled_funcs, total_funcs, compiled_count, invoke_count, failed_count);

    // Step 2: Try to compile top-level code
    {
        TopLevelStatementBlock& sb = pp->sb;
        QoreIRFunction* ir_func = new QoreIRFunction("_toplevel");

        // Top-level locals are pre-instantiated by QoreProgram for each thread,
        // so we must mark them in pre_instantiated_locals to prevent the LLVM
        // lowerer from emitting qore_rt_instantiate_local/uninstantiate_local calls.
        const LVList* lv_list = sb.getLVList();
        if (lv_list) {
            for (unsigned i = 0; i < lv_list->size(); ++i) {
                ir_func->pre_instantiated_locals.insert(
                    reinterpret_cast<const void*>(lv_list->lv[i]));
            }
        }

        QoreIRBuilder builder(ir_func);
        auto* entry = ir_func->createBlock("entry");
        builder.setBlock(entry);

        QoreParseContext parse_context(pgm);
        QoreIRLowering lowering(builder, &parse_context);
        std::string lower_error;
        bool toplevel_ok = false;
        if (lowering.lowerStatementBlock(&sb, lower_error)) {
            // Ensure terminator
            auto& last_block = ir_func->blocks.back();
            if (last_block->instructions.empty() ||
                    (last_block->instructions.back()->opcode != QoreIROpcode::Return &&
                     last_block->instructions.back()->opcode != QoreIROpcode::ReturnNothing &&
                     last_block->instructions.back()->opcode != QoreIROpcode::Br &&
                     last_block->instructions.back()->opcode != QoreIROpcode::Rethrow)) {
                builder.createReturnNothing();
            }
            std::string verify_error;
            if (QoreIRVerifier::verify(*ir_func, verify_error)) {
                if (!hasProcessSpecificOpcodes(*ir_func)) {
                    QoreIRToLLVM llvm_lowerer(ctx);
                    std::string llvm_error;
                    if (llvm_lowerer.lowerFunction(*ir_func, *module, llvm_error)) {
                        compiled_funcs.push_back({"_toplevel", ir_func->name});
                        ++compiled_count;
                        toplevel_ok = true;
                        printd(2, "AOT: compiled _toplevel to LLVM IR\n");
                    } else {
                        printd(2, "AOT: LLVM lowering failed for _toplevel: %s\n", llvm_error.c_str());
                    }
                } else {
                    printd(2, "AOT: _toplevel has process-specific opcodes, deferring to runtime JIT\n");
                    ++invoke_count;
                    toplevel_ok = true;  // not a failure — deferred to runtime JIT
                }
            } else {
                printd(2, "AOT: _toplevel verification failed: %s\n", verify_error.c_str());
            }
        } else {
            printd(2, "AOT: _toplevel IR lowering failed: %s\n", lower_error.c_str());
        }
        delete ir_func;
        if (!toplevel_ok) {
            ++failed_count;
        }
    }

    // Report compilation stats
    printf("AOT compilation: %d/%d functions pre-compiled (%d invoke-fallback, %d failed)\n",
        compiled_count, total_funcs + 1, invoke_count, failed_count);

    // Step 3: Generate main() and function registration table
    generateMainAndTable(ctx, *module, source_text, source_len, label,
        parse_options, compiled_funcs);

    // Verify the complete module
    std::string verify_error;
    llvm::raw_string_ostream verify_os(verify_error);
    if (llvm::verifyModule(*module, &verify_os)) {
        verify_os.flush();
        error = "LLVM module verification failed: " + verify_error;
        return false;
    }

    // Dump LLVM IR if requested
    if (getenv("QORE_DUMP_LLVM_IR")) {
        module->print(llvm::errs(), nullptr);
    }

    // Step 4: Emit object file
    std::string obj_path = output_path + ".o";
    if (!emitObjectFile(*module, obj_path, error)) {
        return false;
    }
    printd(2, "AOT: emitted object file: %s\n", obj_path.c_str());

    // Step 5: Link executable
    if (!linkExecutable(obj_path, output_path, error)) {
        // Clean up object file on link failure
        remove(obj_path.c_str());
        return false;
    }
    printd(2, "AOT: linked executable: %s\n", output_path.c_str());

    // Clean up object file
    remove(obj_path.c_str());

    return true;
}
