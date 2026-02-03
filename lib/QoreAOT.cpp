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
#include "qore/intern/ModuleInfo.h"
#include "qore/intern/qore_thread_intern.h"

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

QoreAOTContext::~QoreAOTContext() {
    // Deref all held expression values (we took a ref in buildAOTContext)
    for (int i = 0; i < num_exprs; ++i) {
        QoreValue v;
        memcpy(&v, &exprs[i], sizeof(v));
        v.discard(nullptr);
    }
    free(locals);
    free(globals);
    free(exprs);
}

//! Descriptor for a function that was successfully compiled to LLVM IR
struct AOTCompiledFunc {
    std::string name;               //!< function name (e.g. "myFunc", "MyClass::myMethod")
    std::string llvm_symbol;        //!< LLVM symbol name in the module
    int num_locals = 0;             //!< number of local variable slots in AOT context
    int num_globals = 0;            //!< number of global variable slots in AOT context
    int num_exprs = 0;              //!< number of expression slots in AOT context
};

//! Try to lower a user function variant to IR
/** @param uvb the user variant base
    @param name the function name for IR lowering
    @param pgm the QoreProgram
    @param ir_func output: the lowered IR function (caller must delete)
    @param error output: error message on failure
    @return 0 = success, -1 = lowering failed
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

    return 0;
}

//! Walk a namespace and compile all user functions to LLVM IR using AOT mode
static void compileNamespaceFunctions(qore_ns_private* ns, QoreProgram* pgm,
        llvm::LLVMContext& ctx, llvm::Module& module,
        std::vector<AOTCompiledFunc>& compiled_funcs,
        int& total_funcs, int& compiled_count, int& failed_count) {
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
                // Build slot map for AOT pointer indirection
                AOTSlotMap slots;
                buildAOTSlotMap(*ir_func, slots);

                // Lower to LLVM with AOT mode
                QoreIRToLLVM lowerer(ctx);
                lowerer.setAOTMode(&slots);
                std::string llvm_error;
                if (lowerer.lowerFunction(*ir_func, module, llvm_error)) {
                    compiled_funcs.push_back({fname, ir_func->name,
                        static_cast<int>(slots.local_slots.size()),
                        static_cast<int>(slots.global_slots.size()),
                        static_cast<int>(slots.expr_slots.size())});
                    ++compiled_count;
                    printd(2, "AOT: compiled function '%s' to LLVM IR (locals=%d, globals=%d, exprs=%d)\n",
                        fname, (int)slots.local_slots.size(), (int)slots.global_slots.size(),
                        (int)slots.expr_slots.size());
                } else {
                    printd(2, "AOT: LLVM lowering failed for '%s': %s\n", fname, llvm_error.c_str());
                    ++failed_count;
                }
                delete ir_func;
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
                    // Build slot map for AOT pointer indirection
                    AOTSlotMap slots;
                    buildAOTSlotMap(*ir_func, slots);

                    // Lower to LLVM with AOT mode
                    QoreIRToLLVM lowerer(ctx);
                    lowerer.setAOTMode(&slots);
                    std::string llvm_error;
                    if (lowerer.lowerFunction(*ir_func, module, llvm_error)) {
                        compiled_funcs.push_back({method_name, ir_func->name,
                            static_cast<int>(slots.local_slots.size()),
                            static_cast<int>(slots.global_slots.size()),
                            static_cast<int>(slots.expr_slots.size())});
                        ++compiled_count;
                        printd(2, "AOT: compiled method '%s' to LLVM IR (locals=%d, globals=%d, exprs=%d)\n",
                            method_name.c_str(), (int)slots.local_slots.size(),
                            (int)slots.global_slots.size(), (int)slots.expr_slots.size());
                    } else {
                        printd(2, "AOT: LLVM lowering failed for '%s': %s\n",
                            method_name.c_str(), llvm_error.c_str());
                        ++failed_count;
                    }
                    delete ir_func;
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
                compiled_funcs, total_funcs, compiled_count, failed_count);
        }
    }
}

//! Map integer optimization level to LLVM OptimizationLevel
static llvm::OptimizationLevel getOptimizationLevel(int opt_level) {
    switch (opt_level) {
        case 0: return llvm::OptimizationLevel::O0;
        case 1: return llvm::OptimizationLevel::O1;
        case 2: return llvm::OptimizationLevel::O2;
        case 3: return llvm::OptimizationLevel::O3;
        default: return llvm::OptimizationLevel::O2;
    }
}

//! Emit an LLVM module to a native object file
static bool emitObjectFile(llvm::Module& module, const std::string& path, std::string& error,
        int opt_level = 2, const char* target_triple = nullptr) {
    std::string triple;
    if (target_triple) {
        triple = target_triple;
        // Initialize all targets for cross-compilation
        llvm::InitializeAllTargets();
        llvm::InitializeAllTargetMCs();
        llvm::InitializeAllAsmPrinters();
        llvm::InitializeAllAsmParsers();
    } else {
        triple = llvm::sys::getDefaultTargetTriple();
    }
    module.setTargetTriple(triple);

    std::string target_error;
    auto* target = llvm::TargetRegistry::lookupTarget(triple, target_error);
    if (!target) {
        error = "failed to look up target '" + triple + "': " + target_error;
        return false;
    }

    auto* tm = target->createTargetMachine(triple, "generic", "",
        llvm::TargetOptions{}, llvm::Reloc::PIC_);
    if (!tm) {
        error = "failed to create target machine for '" + triple + "'";
        return false;
    }
    module.setDataLayout(tm->createDataLayout());

    // Run optimization passes at the requested level
    llvm::OptimizationLevel llvm_opt = getOptimizationLevel(opt_level);
    if (llvm_opt != llvm::OptimizationLevel::O0) {
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
    }

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

//! AOT link configuration read from CMake-generated aot-link.conf
struct AOTLinkConfig {
    std::string cxx;            //!< C++ compiler path
    std::string dynamic_libs;   //!< extra libs for dynamic linking (system libs)
    std::string static_libs;    //!< all transitive deps for static linking
    bool loaded = false;
};

//! Load AOT link configuration from the CMake-generated config file
/** Search order:
    1. QORE_AOT_LINK_CONF environment variable (explicit override)
    2. QORE_LIBDIR env var + "/aot-link.conf" (development builds)
    3. Compiled-in QORE_LIBDIR + "/qore/aot-link.conf" (installed builds)
*/
static AOTLinkConfig loadAOTLinkConfig() {
    AOTLinkConfig config;
    // Default C++ compiler
    config.cxx = "c++";

    // Determine config file path
    std::string conf_path;
    const char* env_conf = getenv("QORE_AOT_LINK_CONF");
    if (env_conf) {
        conf_path = env_conf;
    } else {
        const char* env_libdir = getenv("QORE_LIBDIR");
        if (env_libdir) {
            // Development builds: config is in the build directory (same as QORE_LIBDIR)
            conf_path = std::string(env_libdir) + "/aot-link.conf";
        }
        if (conf_path.empty() || !std::ifstream(conf_path).good()) {
            // Installed builds: config is in QORE_LIBDIR/qore/
            conf_path = std::string(QORE_LIBDIR) + "/qore/aot-link.conf";
        }
    }

    std::ifstream f(conf_path);
    if (!f.is_open()) {
        printd(2, "AOT: link config not found at '%s' — using defaults\n", conf_path.c_str());
        return config;
    }

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        if (key == "cxx") {
            config.cxx = val;
        } else if (key == "dynamic_libs") {
            config.dynamic_libs = val;
        } else if (key == "static_libs") {
            config.static_libs = val;
        }
    }

    config.loaded = true;
    printd(2, "AOT: loaded link config from '%s'\n", conf_path.c_str());
    return config;
}

//! Link an object file into a standalone executable
/** @param obj_path path to the object file
    @param exe_path path for the output executable
    @param error error message on failure
    @param target_triple target triple (nullptr = native; non-null = skip linking, emit .o only)
    @param static_link statically link libqore (requires libqore_static.a)
    @return true on success, false on failure
*/
static bool linkExecutable(const std::string& obj_path, const std::string& exe_path, std::string& error,
        const char* target_triple = nullptr, bool static_link = false) {
    // For cross-compilation, skip linking — user must link with their cross-toolchain
    if (target_triple) {
        printf("cross-compiled object: %s (link manually for target '%s')\n",
            obj_path.c_str(), target_triple);
        return true;
    }

    // Load CMake-generated link configuration
    AOTLinkConfig config = loadAOTLinkConfig();

    // Determine library directory
    std::string libqore_dir;
    const char* qore_prefix = getenv("QORE_LIBDIR");
    if (qore_prefix) {
        libqore_dir = qore_prefix;
    } else {
        libqore_dir = QORE_LIBDIR;
    }

    std::string cmd;
    if (static_link) {
        // Check for static libqore
        std::string static_lib = libqore_dir + "/libqore_static.a";
        {
            std::ifstream f(static_lib);
            if (!f.good()) {
                error = "static libqore not found at " + static_lib
                    + " (build with -DBUILD_STATIC_LIBQORE=ON)";
                return false;
            }
        }

        // Static link: use CXX compiler from config, link static lib + all transitive deps
        cmd = config.cxx + " -o " + exe_path + " " + obj_path
            + " " + static_lib;
        if (!config.static_libs.empty()) {
            cmd += " " + config.static_libs;
        }
    } else {
        // Dynamic link: use CXX compiler from config, link -lqore + system libs
        cmd = config.cxx + " -o " + exe_path + " " + obj_path
            + " -L" + libqore_dir + " -lqore"
            + " -Wl,-rpath," + libqore_dir;
        if (!config.dynamic_libs.empty()) {
            cmd += " " + config.dynamic_libs;
        }
    }

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

    // Build the function table: array of {i8*, i8*, i32, i32, i32}
    // QoreAOTFunc struct: { const char* name, AotFunctionPtr fn_ptr, int num_locals, int num_globals, int num_exprs }
    auto* func_entry_type = llvm::StructType::get(ctx, {ptr_type, ptr_type, i32_type, i32_type, i32_type});

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
            fn,
            llvm::ConstantInt::get(i32_type, cf.num_locals),
            llvm::ConstantInt::get(i32_type, cf.num_globals),
            llvm::ConstantInt::get(i32_type, cf.num_exprs)
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
                      std::string& error,
                      int opt_level,
                      const char* target_triple,
                      bool static_link) {
    // Initialize LLVM targets (needed for object emission)
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    // Create LLVM context and module
    llvm::LLVMContext ctx;
    auto module = std::make_unique<llvm::Module>("qore_aot_module", ctx);

    // Step 1: Enumerate and compile all user functions with AOT pointer indirection
    std::vector<AOTCompiledFunc> compiled_funcs;
    int total_funcs = 0;
    int compiled_count = 0;
    int failed_count = 0;

    qore_program_private* pp = qore_program_private::get(*pgm);
    qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);
    compileNamespaceFunctions(root_ns, pgm, ctx, *module,
        compiled_funcs, total_funcs, compiled_count, failed_count);

    // Step 2: Try to compile top-level code with AOT mode
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
                // Build slot map for AOT pointer indirection
                AOTSlotMap slots;
                buildAOTSlotMap(*ir_func, slots);

                QoreIRToLLVM llvm_lowerer(ctx);
                llvm_lowerer.setAOTMode(&slots);
                std::string llvm_error;
                if (llvm_lowerer.lowerFunction(*ir_func, *module, llvm_error)) {
                    compiled_funcs.push_back({"_toplevel", ir_func->name,
                        static_cast<int>(slots.local_slots.size()),
                        static_cast<int>(slots.global_slots.size()),
                        static_cast<int>(slots.expr_slots.size())});
                    ++compiled_count;
                    toplevel_ok = true;
                    printd(2, "AOT: compiled _toplevel to LLVM IR (locals=%d, globals=%d, exprs=%d)\n",
                        (int)slots.local_slots.size(), (int)slots.global_slots.size(),
                        (int)slots.expr_slots.size());
                } else {
                    printd(2, "AOT: LLVM lowering failed for _toplevel: %s\n", llvm_error.c_str());
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
    printf("AOT compilation: %d/%d functions pre-compiled (%d failed)\n",
        compiled_count, total_funcs + 1, failed_count);

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
    if (!emitObjectFile(*module, obj_path, error, opt_level, target_triple)) {
        return false;
    }
    printd(2, "AOT: emitted object file: %s (O%d)\n", obj_path.c_str(), opt_level);

    // Step 5: Link executable (skip for cross-compilation)
    if (!linkExecutable(obj_path, output_path, error, target_triple, static_link)) {
        // Clean up object file on link failure
        remove(obj_path.c_str());
        return false;
    }
    printd(2, "AOT: linked executable: %s\n", output_path.c_str());

    // Clean up object file (keep for cross-compilation since user needs it)
    if (!target_triple) {
        remove(obj_path.c_str());
    }

    return true;
}

//! Parse module metadata from .qm source text
/** Extracts name, version, desc, author, url, and license from the module { ... } block.
    Falls back to deriving the module name from the filename label.
*/
static bool parseModuleMetadata(const char* source, int source_len, const char* label,
        QoreAOTModuleInfo& info, std::string& error) {
    std::string src(source, source_len);

    // Find "module NAME {" pattern
    size_t mod_pos = src.find("module");
    if (mod_pos == std::string::npos) {
        error = "no 'module' declaration found in source";
        return false;
    }

    // Skip whitespace after "module"
    size_t name_start = mod_pos + 6;
    while (name_start < src.size() && (src[name_start] == ' ' || src[name_start] == '\t')) {
        ++name_start;
    }

    // Read module name (up to whitespace or '{')
    size_t name_end = name_start;
    while (name_end < src.size() && src[name_end] != ' ' && src[name_end] != '\t'
            && src[name_end] != '{' && src[name_end] != '\n') {
        ++name_end;
    }
    if (name_end == name_start) {
        error = "empty module name in 'module' declaration";
        return false;
    }
    info.name = src.substr(name_start, name_end - name_start);

    // Find the module block: everything between { and matching }
    size_t brace_start = src.find('{', name_end);
    if (brace_start == std::string::npos) {
        error = "missing '{' after module declaration";
        return false;
    }
    size_t brace_end = src.find('}', brace_start);
    if (brace_end == std::string::npos) {
        error = "missing '}' in module declaration";
        return false;
    }
    std::string block = src.substr(brace_start + 1, brace_end - brace_start - 1);

    // Parse key = "value" pairs
    auto extractValue = [&](const std::string& key) -> std::string {
        std::string pattern = key + " ";
        size_t kpos = block.find(pattern);
        if (kpos == std::string::npos) {
            pattern = key + "=";
            kpos = block.find(pattern);
        }
        if (kpos == std::string::npos) {
            return "";
        }
        // Find '=' after key
        size_t eq = block.find('=', kpos + key.size());
        if (eq == std::string::npos) {
            return "";
        }
        // Find opening quote
        size_t q1 = block.find('"', eq + 1);
        if (q1 == std::string::npos) {
            return "";
        }
        // Find closing quote
        size_t q2 = block.find('"', q1 + 1);
        if (q2 == std::string::npos) {
            return "";
        }
        return block.substr(q1 + 1, q2 - q1 - 1);
    };

    info.version = extractValue("version");
    info.desc = extractValue("desc");
    info.author = extractValue("author");
    info.url = extractValue("url");
    info.license = extractValue("license");

    if (info.version.empty()) {
        error = "module 'version' not found in module block";
        return false;
    }
    if (info.desc.empty()) {
        error = "module 'desc' not found in module block";
        return false;
    }
    if (info.author.empty()) {
        error = "module 'author' not found in module block";
        return false;
    }
    if (info.license.empty()) {
        info.license = "MIT";
    }

    return true;
}

//! Generate LLVM IR for binary module ABI symbols and init/ns_init/delete functions
static void generateModuleABI(llvm::LLVMContext& ctx, llvm::Module& module,
        const char* source, int source_len, const char* label,
        int64_t parse_options, const QoreAOTModuleInfo& mod_info,
        const std::vector<AOTCompiledFunc>& compiled_funcs) {
    auto* i8_type = llvm::Type::getInt8Ty(ctx);
    auto* i32_type = llvm::Type::getInt32Ty(ctx);
    auto* i64_type = llvm::Type::getInt64Ty(ctx);
    auto* ptr_type = llvm::PointerType::get(ctx, 0);
    auto* void_type = llvm::Type::getVoidTy(ctx);

    // Helper to create a global exported C string
    auto createExportedString = [&](const std::string& name, const std::string& value) {
        auto* str_data = llvm::ConstantDataArray::getString(ctx, value, true);
        auto* gv = new llvm::GlobalVariable(module, str_data->getType(), true,
            llvm::GlobalValue::ExternalLinkage, str_data, name);
        gv->setDLLStorageClass(llvm::GlobalValue::DLLExportStorageClass);
        return gv;
    };

    // Module descriptor globals (exported symbols for dlsym)
    createExportedString("qore_module_name", mod_info.name);
    createExportedString("qore_module_version", mod_info.version);
    createExportedString("qore_module_description", mod_info.desc);
    createExportedString("qore_module_author", mod_info.author);
    createExportedString("qore_module_url", mod_info.url.empty() ? "" : mod_info.url);
    createExportedString("qore_module_license_str", mod_info.license);

    // Module API version (exported int globals)
    auto createExportedInt = [&](const std::string& name, int value) {
        auto* gv = new llvm::GlobalVariable(module, i32_type, true,
            llvm::GlobalValue::ExternalLinkage,
            llvm::ConstantInt::get(i32_type, value), name);
        gv->setDLLStorageClass(llvm::GlobalValue::DLLExportStorageClass);
        return gv;
    };
    createExportedInt("qore_module_api_major", QORE_MODULE_API_MAJOR);
    createExportedInt("qore_module_api_minor", QORE_MODULE_API_MINOR);

    // License enum (QL_MIT = 0)
    auto* license_gv = new llvm::GlobalVariable(module, i32_type, true,
        llvm::GlobalValue::ExternalLinkage,
        llvm::ConstantInt::get(i32_type, 0), "qore_module_license");
    license_gv->setDLLStorageClass(llvm::GlobalValue::DLLExportStorageClass);

    // Embed source as a private global constant
    llvm::Constant* source_data = llvm::ConstantDataArray::getString(ctx,
        llvm::StringRef(source, source_len), false);
    auto* source_gv = new llvm::GlobalVariable(module,
        source_data->getType(), true, llvm::GlobalValue::PrivateLinkage,
        source_data, "qore_aot_mod_source");

    // Embed label as a private global constant
    llvm::Constant* label_data = llvm::ConstantDataArray::getString(ctx,
        llvm::StringRef(label), true);
    auto* label_gv = new llvm::GlobalVariable(module,
        label_data->getType(), true, llvm::GlobalValue::PrivateLinkage,
        label_data, "qore_aot_mod_label");

    // Embed module name as a private global constant
    llvm::Constant* modname_data = llvm::ConstantDataArray::getString(ctx,
        mod_info.name, true);
    auto* modname_gv = new llvm::GlobalVariable(module,
        modname_data->getType(), true, llvm::GlobalValue::PrivateLinkage,
        modname_data, "qore_aot_mod_name");

    // Build function table (same format as executable AOT)
    auto* func_entry_type = llvm::StructType::get(ctx, {ptr_type, ptr_type, i32_type, i32_type, i32_type});
    std::vector<llvm::Constant*> func_entries;
    for (auto& cf : compiled_funcs) {
        llvm::Constant* name_str = llvm::ConstantDataArray::getString(ctx, cf.name, true);
        auto* name_gv = new llvm::GlobalVariable(module,
            name_str->getType(), true, llvm::GlobalValue::PrivateLinkage,
            name_str, "qore_aot_mfname_" + cf.llvm_symbol);
        llvm::Function* fn = module.getFunction(cf.llvm_symbol);
        if (!fn) {
            continue;
        }
        llvm::Constant* entry = llvm::ConstantStruct::get(func_entry_type, {
            name_gv, fn,
            llvm::ConstantInt::get(i32_type, cf.num_locals),
            llvm::ConstantInt::get(i32_type, cf.num_globals),
            llvm::ConstantInt::get(i32_type, cf.num_exprs)
        });
        func_entries.push_back(entry);
    }

    int num_funcs = (int)func_entries.size();
    llvm::GlobalVariable* func_table_gv = nullptr;
    if (num_funcs > 0) {
        auto* table_type = llvm::ArrayType::get(func_entry_type, num_funcs);
        auto* func_table_init = llvm::ConstantArray::get(table_type, func_entries);
        func_table_gv = new llvm::GlobalVariable(module,
            table_type, true, llvm::GlobalValue::PrivateLinkage,
            func_table_init, "qore_aot_mod_funcs");
    }

    // Declare qore_aot_module_init: QoreStringNode* (source, source_len, label, parse_options, mod_name, funcs, num_funcs)
    auto* init_ret_type = ptr_type;  // QoreStringNode*
    auto* aot_mod_init_type = llvm::FunctionType::get(init_ret_type, {
        ptr_type, i32_type, ptr_type, i64_type, ptr_type, ptr_type, i32_type
    }, false);
    auto aot_mod_init_fn = module.getOrInsertFunction("qore_aot_module_init", aot_mod_init_type);

    // Declare qore_aot_module_ns_init: void (root_ns, qore_ns)
    auto* aot_mod_ns_init_type = llvm::FunctionType::get(void_type, {ptr_type, ptr_type}, false);
    auto aot_mod_ns_init_fn = module.getOrInsertFunction("qore_aot_module_ns_init", aot_mod_ns_init_type);

    // Declare qore_aot_module_delete: void ()
    auto* aot_mod_del_type = llvm::FunctionType::get(void_type, {}, false);
    auto aot_mod_del_fn = module.getOrInsertFunction("qore_aot_module_delete", aot_mod_del_type);

    // Generate qore_module_init() — exported
    {
        auto* fn_type = llvm::FunctionType::get(ptr_type, {}, false);
        auto* fn = llvm::Function::Create(fn_type, llvm::Function::ExternalLinkage, "qore_module_init", module);
        fn->setDLLStorageClass(llvm::GlobalValue::DLLExportStorageClass);

        auto* entry_bb = llvm::BasicBlock::Create(ctx, "entry", fn);
        llvm::IRBuilder<> builder(entry_bb);

        llvm::Value* src_ptr = builder.CreateInBoundsGEP(
            source_data->getType(), source_gv,
            {builder.getInt64(0), builder.getInt64(0)});
        llvm::Value* lbl_ptr = builder.CreateInBoundsGEP(
            label_data->getType(), label_gv,
            {builder.getInt64(0), builder.getInt64(0)});
        llvm::Value* name_ptr = builder.CreateInBoundsGEP(
            modname_data->getType(), modname_gv,
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

        llvm::Value* result = builder.CreateCall(aot_mod_init_fn, {
            src_ptr,
            builder.getInt32(source_len),
            lbl_ptr,
            builder.getInt64(parse_options),
            name_ptr,
            funcs_ptr,
            builder.getInt32(num_funcs)
        });
        builder.CreateRet(result);
    }

    // Generate qore_module_ns_init(root_ns, qore_ns) — exported
    {
        auto* fn_type = llvm::FunctionType::get(void_type, {ptr_type, ptr_type}, false);
        auto* fn = llvm::Function::Create(fn_type, llvm::Function::ExternalLinkage, "qore_module_ns_init", module);
        fn->setDLLStorageClass(llvm::GlobalValue::DLLExportStorageClass);

        auto* entry_bb = llvm::BasicBlock::Create(ctx, "entry", fn);
        llvm::IRBuilder<> builder(entry_bb);

        auto arg_it = fn->arg_begin();
        llvm::Value* root_ns_val = &*arg_it++;
        llvm::Value* qore_ns_val = &*arg_it;

        builder.CreateCall(aot_mod_ns_init_fn, {root_ns_val, qore_ns_val});
        builder.CreateRetVoid();
    }

    // Generate qore_module_delete() — exported
    {
        auto* fn_type = llvm::FunctionType::get(void_type, {}, false);
        auto* fn = llvm::Function::Create(fn_type, llvm::Function::ExternalLinkage, "qore_module_delete", module);
        fn->setDLLStorageClass(llvm::GlobalValue::DLLExportStorageClass);

        auto* entry_bb = llvm::BasicBlock::Create(ctx, "entry", fn);
        llvm::IRBuilder<> builder(entry_bb);

        builder.CreateCall(aot_mod_del_fn, {});
        builder.CreateRetVoid();
    }
}

//! Link an object file into a shared library
static bool linkSharedLib(const std::string& obj_path, const std::string& so_path, std::string& error,
        const char* target_triple = nullptr) {
    if (target_triple) {
        printf("cross-compiled module object: %s (link manually for target '%s')\n",
            obj_path.c_str(), target_triple);
        return true;
    }

    AOTLinkConfig config = loadAOTLinkConfig();

    std::string libqore_dir;
    const char* qore_prefix = getenv("QORE_LIBDIR");
    if (qore_prefix) {
        libqore_dir = qore_prefix;
    } else {
        libqore_dir = QORE_LIBDIR;
    }

    std::string cmd = config.cxx + " -shared -o " + so_path + " " + obj_path
        + " -L" + libqore_dir + " -lqore"
        + " -Wl,-rpath," + libqore_dir;
    if (!config.dynamic_libs.empty()) {
        cmd += " " + config.dynamic_libs;
    }

    printd(2, "AOT: link shared lib command: %s\n", cmd.c_str());
    int rc = system(cmd.c_str());
    if (rc != 0) {
        error = "linker command failed with exit code " + std::to_string(rc);
        return false;
    }
    return true;
}

bool QoreAOT::compileModule(const char* source_text, int source_len,
                             const char* label,
                             const std::string& output_path,
                             int64_t parse_options,
                             std::string& error,
                             int opt_level,
                             const char* target_triple) {
    // Step 1: Parse module metadata from source
    QoreAOTModuleInfo mod_info;
    if (!parseModuleMetadata(source_text, source_len, label, mod_info, error)) {
        return false;
    }
    printd(2, "AOT module: name='%s' version='%s' desc='%s'\n",
        mod_info.name.c_str(), mod_info.version.c_str(), mod_info.desc.c_str());

    // Step 2: Parse the module source to get the namespace tree with functions
    // We need PO_IN_MODULE for the parser to handle the module block
    int64_t mod_po = parse_options | PO_IN_MODULE | PO_NO_TOP_LEVEL_STATEMENTS
        | PO_REQUIRE_PROTOTYPES | PO_REQUIRE_OUR;

    ExceptionSink xsink;
    ExceptionSink wsink;
    QoreProgramHelper qpgm(mod_po, xsink);
    if (xsink.isException()) {
        xsink.handleExceptions();
        error = "failed to create QoreProgram for module parsing";
        return false;
    }

    // Set up module context for the parser (must be QoreUserModuleDefContextHelper
    // because the parser static_casts to it)
    {
        QoreUserModuleDefContextHelper mod_ctx(mod_info.name.c_str(), label, *qpgm, xsink);
        mod_ctx.setNameInit(mod_info.name.c_str());

        qpgm->parse(source_text, label, &xsink, &wsink, QP_WARN_DEFAULT);

        mod_ctx.close();
    }

    if (wsink.isException()) {
        wsink.handleWarnings();
    }
    if (xsink.isException()) {
        xsink.handleExceptions();
        error = "module parsing failed";
        return false;
    }

    // Step 3: Initialize LLVM and compile functions
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    llvm::LLVMContext ctx;
    auto module = std::make_unique<llvm::Module>("qore_aot_module_" + mod_info.name, ctx);

    std::vector<AOTCompiledFunc> compiled_funcs;
    int total_funcs = 0;
    int compiled_count = 0;
    int failed_count = 0;

    qore_program_private* pp = qore_program_private::get(**qpgm);
    qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);
    compileNamespaceFunctions(root_ns, *qpgm, ctx, *module,
        compiled_funcs, total_funcs, compiled_count, failed_count);

    printf("AOT module compilation: %d/%d functions pre-compiled (%d failed)\n",
        compiled_count, total_funcs, failed_count);

    // Step 4: Generate module ABI (instead of main + table)
    generateModuleABI(ctx, *module, source_text, source_len, label,
        mod_po, mod_info, compiled_funcs);

    // Verify the module
    std::string verify_error;
    llvm::raw_string_ostream verify_os(verify_error);
    if (llvm::verifyModule(*module, &verify_os)) {
        verify_os.flush();
        error = "LLVM module verification failed: " + verify_error;
        return false;
    }

    if (getenv("QORE_DUMP_LLVM_IR")) {
        module->print(llvm::errs(), nullptr);
    }

    // Step 5: Emit PIC object file
    std::string obj_path = output_path + ".o";
    if (!emitObjectFile(*module, obj_path, error, opt_level, target_triple)) {
        return false;
    }

    // Step 6: Link as shared library
    if (!linkSharedLib(obj_path, output_path, error, target_triple)) {
        remove(obj_path.c_str());
        return false;
    }

    // Clean up object file (keep for cross-compilation)
    if (!target_triple) {
        remove(obj_path.c_str());
    }

    return true;
}

void buildAOTSlotMap(const QoreIRFunction& func, AOTSlotMap& slots) {
    // Walk blocks and instructions in deterministic order (same as LLVM lowering)
    // to assign slot indices for each unique process-specific pointer.
    for (auto& block : func.blocks) {
        for (auto& inst : block->instructions) {
            switch (inst->opcode) {
                case QoreIROpcode::LoadLocal:
                case QoreIROpcode::StoreLocal: {
                    auto* li = static_cast<QoreIRLocalInstruction*>(inst.get());
                    slots.getLocalSlot(reinterpret_cast<const void*>(li->local));
                    break;
                }
                case QoreIROpcode::LoadGlobal:
                case QoreIROpcode::StoreGlobal:
                case QoreIROpcode::LoadThreadLocal:
                case QoreIROpcode::StoreThreadLocal: {
                    auto* vi = static_cast<QoreIRVarInstruction*>(inst.get());
                    slots.getGlobalSlot(reinterpret_cast<const void*>(vi->var));
                    break;
                }
                case QoreIROpcode::LoadClosure:
                case QoreIROpcode::StoreClosure: {
                    // Closure vars use local slots (ClosureVarValue* cast to LocalVar*)
                    auto* li = static_cast<QoreIRLocalInstruction*>(inst.get());
                    slots.getLocalSlot(reinterpret_cast<const void*>(li->local));
                    break;
                }
                case QoreIROpcode::Invoke: {
                    auto* ii = static_cast<QoreIRInvokeInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &ii->expr, sizeof(bits));
                    slots.getExprSlot(bits);
                    break;
                }
                case QoreIROpcode::Call:
                case QoreIROpcode::CallMethod:
                case QoreIROpcode::CallStatic:
                case QoreIROpcode::CallIndirect: {
                    auto* ei = static_cast<QoreIRExprInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &ei->expr, sizeof(bits));
                    slots.getExprSlot(bits);
                    break;
                }
                case QoreIROpcode::LoadLValue:
                case QoreIROpcode::StoreLValue:
                case QoreIROpcode::PreIncLValue:
                case QoreIROpcode::PreDecLValue:
                case QoreIROpcode::PostIncLValue:
                case QoreIROpcode::PostDecLValue:
                case QoreIROpcode::AddAssignLValue:
                case QoreIROpcode::SubAssignLValue:
                case QoreIROpcode::MulAssignLValue:
                case QoreIROpcode::DivAssignLValue:
                case QoreIROpcode::ModAssignLValue:
                case QoreIROpcode::AndAssignLValue:
                case QoreIROpcode::OrAssignLValue:
                case QoreIROpcode::XorAssignLValue:
                case QoreIROpcode::ShlAssignLValue:
                case QoreIROpcode::ShrAssignLValue:
                case QoreIROpcode::ShiftLValue:
                case QoreIROpcode::UnshiftLValue: {
                    auto* lvi = static_cast<QoreIRLValueInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &lvi->lvalue, sizeof(bits));
                    slots.getExprSlot(bits);
                    break;
                }
                // Expression-based opcodes (DotEval*, Map*, Cast*, etc.)
                case QoreIROpcode::PopAny:
                case QoreIROpcode::PushAny:
                case QoreIROpcode::ExtractAny:
                case QoreIROpcode::ExtractList:
                case QoreIROpcode::ExtractString:
                case QoreIROpcode::ExtractBinary:
                case QoreIROpcode::RemoveAny:
                case QoreIROpcode::RemoveList:
                case QoreIROpcode::RemoveHash:
                case QoreIROpcode::RemoveObject:
                case QoreIROpcode::RemoveString:
                case QoreIROpcode::RemoveBinary:
                case QoreIROpcode::KeysAny:
                case QoreIROpcode::KeysList:
                case QoreIROpcode::KeysHash:
                case QoreIROpcode::RegexMatchAny:
                case QoreIROpcode::RegexMatchBool:
                case QoreIROpcode::RegexNMatchBool:
                case QoreIROpcode::RegexExtractAny:
                case QoreIROpcode::RegexExtractList:
                case QoreIROpcode::RegexSubstAny:
                case QoreIROpcode::RegexSubstString:
                case QoreIROpcode::InstanceOfBool:
                case QoreIROpcode::TrimAny:
                case QoreIROpcode::TrimString:
                case QoreIROpcode::ChompAny:
                case QoreIROpcode::ChompString:
                case QoreIROpcode::TransliterateAny:
                case QoreIROpcode::TransliterateString:
                case QoreIROpcode::BackgroundInt:
                case QoreIROpcode::ListAssignAny:
                case QoreIROpcode::ExistsAny:
                case QoreIROpcode::ExistsBool:
                case QoreIROpcode::ElementsAny:
                case QoreIROpcode::ElementsInt:
                case QoreIROpcode::DotEvalHash:
                case QoreIROpcode::DotEvalAny:
                case QoreIROpcode::DotEvalInt:
                case QoreIROpcode::DotEvalFloat:
                case QoreIROpcode::DotEvalString:
                case QoreIROpcode::DotEvalDate:
                case QoreIROpcode::DotEvalList:
                case QoreIROpcode::DotEvalObject:
                case QoreIROpcode::MapSelectList:
                case QoreIROpcode::MapSelectAny:
                case QoreIROpcode::HashMap:
                case QoreIROpcode::HashMapSelect:
                case QoreIROpcode::HashMapAny:
                case QoreIROpcode::HashMapSelectAny:
                case QoreIROpcode::CastAny:
                case QoreIROpcode::CastList:
                case QoreIROpcode::CastHash:
                case QoreIROpcode::CastObject:
                case QoreIROpcode::CastEnum:
                case QoreIROpcode::InvokeSimError: {
                    auto* ei = static_cast<QoreIRExprInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &ei->expr, sizeof(bits));
                    slots.getExprSlot(bits);
                    break;
                }
                default:
                    break;
            }
        }
    }
}

QoreAOTContext* buildAOTContext(const QoreIRFunction& func, int num_locals, int num_globals, int num_exprs) {
    QoreAOTContext* ctx = new QoreAOTContext();
    ctx->num_locals = num_locals;
    ctx->num_globals = num_globals;
    ctx->num_exprs = num_exprs;
    ctx->allocate();

    // Copy all_body_locals from the fresh IR
    ctx->all_body_locals = func.all_body_locals;

    // Walk in the same deterministic order as buildAOTSlotMap to fill arrays
    int local_idx = 0;
    int global_idx = 0;
    int expr_idx = 0;
    std::unordered_map<const void*, int> seen_locals;
    std::unordered_map<const void*, int> seen_globals;
    std::unordered_map<uint64_t, int> seen_exprs;

    for (auto& block : func.blocks) {
        for (auto& inst : block->instructions) {
            switch (inst->opcode) {
                case QoreIROpcode::LoadLocal:
                case QoreIROpcode::StoreLocal: {
                    auto* li = static_cast<QoreIRLocalInstruction*>(inst.get());
                    const void* key = reinterpret_cast<const void*>(li->local);
                    if (seen_locals.find(key) == seen_locals.end()) {
                        assert(local_idx < num_locals);
                        ctx->locals[local_idx] = li->local;
                        seen_locals[key] = local_idx;
                        ++local_idx;
                    }
                    break;
                }
                case QoreIROpcode::LoadGlobal:
                case QoreIROpcode::StoreGlobal:
                case QoreIROpcode::LoadThreadLocal:
                case QoreIROpcode::StoreThreadLocal: {
                    auto* vi = static_cast<QoreIRVarInstruction*>(inst.get());
                    const void* key = reinterpret_cast<const void*>(vi->var);
                    if (seen_globals.find(key) == seen_globals.end()) {
                        assert(global_idx < num_globals);
                        ctx->globals[global_idx] = vi->var;
                        seen_globals[key] = global_idx;
                        ++global_idx;
                    }
                    break;
                }
                case QoreIROpcode::LoadClosure:
                case QoreIROpcode::StoreClosure: {
                    auto* li = static_cast<QoreIRLocalInstruction*>(inst.get());
                    const void* key = reinterpret_cast<const void*>(li->local);
                    if (seen_locals.find(key) == seen_locals.end()) {
                        assert(local_idx < num_locals);
                        ctx->locals[local_idx] = li->local;
                        seen_locals[key] = local_idx;
                        ++local_idx;
                    }
                    break;
                }
                case QoreIROpcode::Invoke: {
                    auto* ii = static_cast<QoreIRInvokeInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &ii->expr, sizeof(bits));
                    if (seen_exprs.find(bits) == seen_exprs.end()) {
                        assert(expr_idx < num_exprs);
                        // Take a ref so the expression survives IR function deletion
                        ii->expr.ref();
                        ctx->exprs[expr_idx] = bits;
                        seen_exprs[bits] = expr_idx;
                        ++expr_idx;
                    }
                    break;
                }
                case QoreIROpcode::Call:
                case QoreIROpcode::CallMethod:
                case QoreIROpcode::CallStatic:
                case QoreIROpcode::CallIndirect: {
                    auto* ei = static_cast<QoreIRExprInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &ei->expr, sizeof(bits));
                    if (seen_exprs.find(bits) == seen_exprs.end()) {
                        assert(expr_idx < num_exprs);
                        // Take a ref so the expression survives IR function deletion
                        ei->expr.ref();
                        ctx->exprs[expr_idx] = bits;
                        seen_exprs[bits] = expr_idx;
                        ++expr_idx;
                    }
                    break;
                }
                case QoreIROpcode::LoadLValue:
                case QoreIROpcode::StoreLValue:
                case QoreIROpcode::PreIncLValue:
                case QoreIROpcode::PreDecLValue:
                case QoreIROpcode::PostIncLValue:
                case QoreIROpcode::PostDecLValue:
                case QoreIROpcode::AddAssignLValue:
                case QoreIROpcode::SubAssignLValue:
                case QoreIROpcode::MulAssignLValue:
                case QoreIROpcode::DivAssignLValue:
                case QoreIROpcode::ModAssignLValue:
                case QoreIROpcode::AndAssignLValue:
                case QoreIROpcode::OrAssignLValue:
                case QoreIROpcode::XorAssignLValue:
                case QoreIROpcode::ShlAssignLValue:
                case QoreIROpcode::ShrAssignLValue:
                case QoreIROpcode::ShiftLValue:
                case QoreIROpcode::UnshiftLValue: {
                    auto* lvi = static_cast<QoreIRLValueInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &lvi->lvalue, sizeof(bits));
                    if (seen_exprs.find(bits) == seen_exprs.end()) {
                        assert(expr_idx < num_exprs);
                        // Take a ref so the lvalue survives IR function deletion
                        lvi->lvalue.ref();
                        ctx->exprs[expr_idx] = bits;
                        seen_exprs[bits] = expr_idx;
                        ++expr_idx;
                    }
                    break;
                }
                // Expression-based opcodes (DotEval*, Map*, Cast*, etc.)
                case QoreIROpcode::PopAny:
                case QoreIROpcode::PushAny:
                case QoreIROpcode::ExtractAny:
                case QoreIROpcode::ExtractList:
                case QoreIROpcode::ExtractString:
                case QoreIROpcode::ExtractBinary:
                case QoreIROpcode::RemoveAny:
                case QoreIROpcode::RemoveList:
                case QoreIROpcode::RemoveHash:
                case QoreIROpcode::RemoveObject:
                case QoreIROpcode::RemoveString:
                case QoreIROpcode::RemoveBinary:
                case QoreIROpcode::KeysAny:
                case QoreIROpcode::KeysList:
                case QoreIROpcode::KeysHash:
                case QoreIROpcode::RegexMatchAny:
                case QoreIROpcode::RegexMatchBool:
                case QoreIROpcode::RegexNMatchBool:
                case QoreIROpcode::RegexExtractAny:
                case QoreIROpcode::RegexExtractList:
                case QoreIROpcode::RegexSubstAny:
                case QoreIROpcode::RegexSubstString:
                case QoreIROpcode::InstanceOfBool:
                case QoreIROpcode::TrimAny:
                case QoreIROpcode::TrimString:
                case QoreIROpcode::ChompAny:
                case QoreIROpcode::ChompString:
                case QoreIROpcode::TransliterateAny:
                case QoreIROpcode::TransliterateString:
                case QoreIROpcode::BackgroundInt:
                case QoreIROpcode::ListAssignAny:
                case QoreIROpcode::ExistsAny:
                case QoreIROpcode::ExistsBool:
                case QoreIROpcode::ElementsAny:
                case QoreIROpcode::ElementsInt:
                case QoreIROpcode::DotEvalHash:
                case QoreIROpcode::DotEvalAny:
                case QoreIROpcode::DotEvalInt:
                case QoreIROpcode::DotEvalFloat:
                case QoreIROpcode::DotEvalString:
                case QoreIROpcode::DotEvalDate:
                case QoreIROpcode::DotEvalList:
                case QoreIROpcode::DotEvalObject:
                case QoreIROpcode::MapSelectList:
                case QoreIROpcode::MapSelectAny:
                case QoreIROpcode::HashMap:
                case QoreIROpcode::HashMapSelect:
                case QoreIROpcode::HashMapAny:
                case QoreIROpcode::HashMapSelectAny:
                case QoreIROpcode::CastAny:
                case QoreIROpcode::CastList:
                case QoreIROpcode::CastHash:
                case QoreIROpcode::CastObject:
                case QoreIROpcode::CastEnum:
                case QoreIROpcode::InvokeSimError: {
                    auto* ei = static_cast<QoreIRExprInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &ei->expr, sizeof(bits));
                    if (seen_exprs.find(bits) == seen_exprs.end()) {
                        assert(expr_idx < num_exprs);
                        // Take a ref so the expression survives IR function deletion
                        ei->expr.ref();
                        ctx->exprs[expr_idx] = bits;
                        seen_exprs[bits] = expr_idx;
                        ++expr_idx;
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    assert(local_idx == num_locals);
    assert(global_idx == num_globals);
    assert(expr_idx == num_exprs);

    return ctx;
}
