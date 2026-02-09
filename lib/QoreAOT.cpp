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
#include "qore/intern/QoreAOTBinary.h"
#include "qore/intern/QoreDir.h"

#include <qore/Qore.h>

#include <sys/stat.h>
#include <unordered_set>

#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/FunctionList.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/QoreIR.h"
#include "qore/intern/QoreIRBuilder.h"
#include "qore/intern/QoreIRLowering.h"
#include "qore/intern/QoreIRVerifier.h"
#include "qore/intern/QoreIRToLLVM.h"
#include "qore/intern/QoreIRPrinter.h"
#include "qore/intern/StatementBlock.h"
#include "qore/intern/Function.h"
#include "qore/intern/FunctionCallNode.h"
#include "qore/intern/SelfVarrefNode.h"
#include "qore/intern/VarRefNode.h"
#include "qore/intern/ModuleInfo.h"
#include "qore/intern/Variable.h"
#include "qore/intern/qore_thread_intern.h"

// Defined in Function.cpp - collects all local variables from a StatementBlock and nested blocks
extern void collectAllStatementLocals(const StatementBlock* block, std::vector<LocalVar*>& locals);

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
#include <llvm/TargetParser/Triple.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

//! Generate a unique variant key that includes parameter types to distinguish overloads
/** Format: "name(type1,type2,...)" - uses type paths for parameter types
    @param name the base function/method name
    @param variant the function variant
    @return unique key string
*/
std::string getVariantKey(const char* name, const AbstractQoreFunctionVariant* variant) {
    std::string key(name);
    key.append("(");
    AbstractFunctionSignature* sig = variant->getSignature();
    if (sig) {
        const type_vec_t& types = sig->getTypeList();
        for (size_t i = 0; i < types.size(); ++i) {
            if (i > 0) {
                key.append(",");
            }
            key.append(QoreTypeInfo::getPath(types[i]));
        }
    }
    key.append(")");
    return key;
}

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
    free(call_targets);
}

//! Descriptor for a function that was successfully compiled to LLVM IR
struct AOTCompiledFunc {
    std::string name;               //!< function name (e.g. "myFunc", "MyClass::myMethod")
    std::string llvm_symbol;        //!< LLVM symbol name in the module
    int num_locals = 0;             //!< number of local variable slots in AOT context
    int num_globals = 0;            //!< number of global variable slots in AOT context
    int num_exprs = 0;              //!< number of expression slots in AOT context
    int num_stmts = 0;              //!< number of statement slots in AOT context (OnBlockExit)
    AOTSlotIdentities slot_ids;     //!< extracted slot identities for source-stripped mode
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
        if (getenv("QORE_AOT_DEBUG")) {
            fprintf(stderr, "AOT-LOWER: lowering failed for '%s': %s\n", name, error.c_str());
        }
        delete ir_func;
        ir_func = nullptr;
        return -1;
    }
    // Ensure terminator for the current block (where the builder is pointing)
    // After lowering control flow constructs like try-catch, the builder may be pointing
    // to a merge block that needs an implicit return
    QoreIRBasicBlock* current_block = builder.getBlock();
    if (current_block && (current_block->instructions.empty() ||
            !isTerminator(current_block->instructions.back()->opcode))) {
        builder.createReturnNothing();
    }
    if (!QoreIRVerifier::verify(*ir_func, error)) {
        if (getenv("QORE_AOT_DUMP_IR")) {
            fprintf(stderr, "=== FAILED IR for %s ===\n", name);
            QoreIRPrinter::print(*ir_func, std::cerr);
            fprintf(stderr, "=== VERIFY ERROR: %s ===\n", error.c_str());
        }
        delete ir_func;
        ir_func = nullptr;
        return -1;
    }

    // Collect ALL body locals from the statement tree (includes nested blocks from
    // for/while/if/try/switch statements).  Mark them as pre-instantiated so the
    // LLVM lowerer doesn't emit instantiation/uninstantiation calls - the caller
    // (evalTiered) handles instantiation at runtime.
    collectAllStatementLocals(statements, ir_func->all_body_locals);
    for (LocalVar* lv : ir_func->all_body_locals) {
        ir_func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(lv));
    }
    // Classify locals as IR-only vs AST-visible for optimization
    ir_func->computeIROnlyLocals();

    return 0;
}

// Forward declarations for slot identity extraction (defined after buildAOTContext)
static AOTExprSlotId classifyExpression(uint64_t bits, const AOTSlotMap& slots);
static void extractAOTSlotIdentities(const QoreIRFunction& func, const AOTSlotMap& slots,
        UserVariantBase* uvb, AOTSlotIdentities& out);

//! Walk a namespace and compile all user functions to LLVM IR using AOT mode
static void compileNamespaceFunctions(qore_ns_private* ns, QoreProgram* pgm,
        llvm::LLVMContext& ctx, llvm::Module& module,
        llvm::DIBuilder& di_builder, llvm::DICompileUnit* di_cu,
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
            // Generate unique key including parameter types to distinguish overloads
            std::string variant_key = getVariantKey(fname, variant);
            QoreIRFunction* ir_func = nullptr;
            std::string lower_error;
            int rc = tryLowerFunction(uvb, fname, pgm, ir_func, lower_error);

            if (rc == 0 && ir_func) {
                // Skip if another variant with the same LLVM function name was already compiled.
                // Overloaded variants share the same function name but only the first can be
                // lowered into a single LLVM function; re-lowering would corrupt debug info.
                llvm::Function* existing = module.getFunction(ir_func->name);
                if (existing && !existing->empty()) {
                    printd(2, "AOT: skipping duplicate variant '%s' (function '%s' already compiled)\n",
                        variant_key.c_str(), ir_func->name.c_str());
                    delete ir_func;
                    continue;
                }

                // Build slot map for AOT pointer indirection
                // (computeIROnlyLocals already called inside tryLowerFunction)
                AOTSlotMap slots;
                buildAOTSlotMap(*ir_func, slots);

                // Lower to LLVM with AOT mode
                QoreIRToLLVM lowerer(ctx);
                lowerer.setAOTMode(&slots);
                lowerer.setSharedDebugInfo(&di_builder, di_cu);
                std::string llvm_error;
                // Debug: dump IR before LLVM lowering if requested
                if (getenv("QORE_AOT_DUMP_IR")) {
                    fprintf(stderr, "=== IR for %s ===\n", variant_key.c_str());
                    QoreIRPrinter::print(*ir_func, std::cerr);
                    fprintf(stderr, "=================\n");
                }
                if (lowerer.lowerFunction(*ir_func, module, llvm_error)) {
                    AOTCompiledFunc cf;
                    cf.name = variant_key;  // Use variant key instead of plain name
                    cf.llvm_symbol = ir_func->name;
                    cf.num_locals = static_cast<int>(slots.local_slots.size());
                    cf.num_globals = static_cast<int>(slots.global_slots.size());
                    cf.num_exprs = static_cast<int>(slots.expr_slots.size());
                    cf.num_stmts = static_cast<int>(slots.stmt_slots.size());
                    // Extract slot identities for source-stripped mode
                    extractAOTSlotIdentities(*ir_func, slots, uvb, cf.slot_ids);
                    compiled_funcs.push_back(std::move(cf));
                    ++compiled_count;
                    printd(2, "AOT: compiled function '%s' to LLVM IR (locals=%d, globals=%d, exprs=%d, stmts=%d)\n",
                        variant_key.c_str(), (int)slots.local_slots.size(), (int)slots.global_slots.size(),
                        (int)slots.expr_slots.size(), (int)slots.stmt_slots.size());
                } else {
                    printd(2, "AOT: LLVM lowering failed for '%s': %s\n", variant_key.c_str(), llvm_error.c_str());
                    if (getenv("QORE_AOT_DEBUG")) {
                        fprintf(stderr, "AOT: LLVM lowering failed for '%s': %s\n", variant_key.c_str(), llvm_error.c_str());
                    }
                    ++failed_count;
                }
                delete ir_func;
            } else {
                printd(2, "AOT: IR lowering failed for '%s': %s\n", variant_key.c_str(), lower_error.c_str());
                if (getenv("QORE_AOT_DEBUG")) {
                    fprintf(stderr, "AOT: IR lowering failed for '%s': %s\n", variant_key.c_str(), lower_error.c_str());
                }
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
                // Generate unique key including parameter types to distinguish overloads
                std::string variant_key = getVariantKey(method_name.c_str(), variant);
                QoreIRFunction* ir_func = nullptr;
                std::string lower_error;
                int rc = tryLowerFunction(uvb, method_name.c_str(), pgm, ir_func, lower_error);

                if (rc == 0 && ir_func) {
                    // Skip if another variant with the same LLVM function name was already compiled
                    llvm::Function* existing = module.getFunction(ir_func->name);
                    if (existing && !existing->empty()) {
                        printd(2, "AOT: skipping duplicate variant '%s' (function '%s' already compiled)\n",
                            variant_key.c_str(), ir_func->name.c_str());
                        delete ir_func;
                        continue;
                    }

                    // Build slot map for AOT pointer indirection
                    // (computeIROnlyLocals already called inside tryLowerFunction)
                    AOTSlotMap slots;
                    buildAOTSlotMap(*ir_func, slots);

                    // Lower to LLVM with AOT mode
                    QoreIRToLLVM lowerer(ctx);
                    lowerer.setAOTMode(&slots);
                    lowerer.setSharedDebugInfo(&di_builder, di_cu);
                    std::string llvm_error;
                    if (lowerer.lowerFunction(*ir_func, module, llvm_error)) {
                        AOTCompiledFunc cf;
                        cf.name = variant_key;  // Use variant key instead of plain name
                        cf.llvm_symbol = ir_func->name;
                        cf.num_locals = static_cast<int>(slots.local_slots.size());
                        cf.num_globals = static_cast<int>(slots.global_slots.size());
                        cf.num_exprs = static_cast<int>(slots.expr_slots.size());
                        cf.num_stmts = static_cast<int>(slots.stmt_slots.size());
                        // Extract slot identities for source-stripped mode
                        extractAOTSlotIdentities(*ir_func, slots, uvb, cf.slot_ids);
                        compiled_funcs.push_back(std::move(cf));
                        ++compiled_count;
                        printd(2, "AOT: compiled method '%s' to LLVM IR (locals=%d, globals=%d, exprs=%d, stmts=%d)\n",
                            variant_key.c_str(), (int)slots.local_slots.size(),
                            (int)slots.global_slots.size(), (int)slots.expr_slots.size(),
                            (int)slots.stmt_slots.size());
                    } else {
                        printd(2, "AOT: LLVM lowering failed for '%s': %s\n",
                            variant_key.c_str(), llvm_error.c_str());
                        if (getenv("QORE_AOT_DEBUG")) {
                            fprintf(stderr, "AOT: LLVM lowering failed for '%s': %s\n",
                                variant_key.c_str(), llvm_error.c_str());
                        }
                        ++failed_count;
                    }
                    delete ir_func;
                } else {
                    printd(2, "AOT: IR lowering failed for '%s': %s\n",
                        variant_key.c_str(), lower_error.c_str());
                    if (getenv("QORE_AOT_DEBUG")) {
                        fprintf(stderr, "AOT: IR lowering failed for '%s': %s\n",
                            variant_key.c_str(), lower_error.c_str());
                    }
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
                di_builder, di_cu,
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
        default: return llvm::OptimizationLevel::O3;
    }
}

//! Emit an LLVM module to a native object file
static bool emitObjectFile(llvm::Module& module, const std::string& path, std::string& error,
        int opt_level = 3, const char* target_triple = nullptr) {
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
#if LLVM_VERSION_MAJOR >= 21
    module.setTargetTriple(llvm::Triple(triple));
#else
    module.setTargetTriple(triple);
#endif

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

    // Build the function table: array of {i8*, i8*, i32, i32, i32, i32}
    // QoreAOTFunc struct: { const char* name, AotFunctionPtr fn_ptr, int num_locals, int num_globals, int num_exprs, int num_stmts }
    auto* func_entry_type = llvm::StructType::get(ctx, {ptr_type, ptr_type, i32_type, i32_type, i32_type, i32_type});

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
            llvm::ConstantInt::get(i32_type, cf.num_exprs),
            llvm::ConstantInt::get(i32_type, cf.num_stmts)
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

//! Generate main() and function table for source-stripped AOT binary.
/** Embeds serialized metadata blob instead of source text, and calls
    qore_aot_run_v2() from main().
*/
static void generateMainAndTableV2(llvm::LLVMContext& ctx, llvm::Module& module,
        const std::vector<uint8_t>& metadata, const char* label,
        int64_t parse_options, const std::vector<AOTCompiledFunc>& compiled_funcs) {
    auto* i8_type = llvm::Type::getInt8Ty(ctx);
    auto* i32_type = llvm::Type::getInt32Ty(ctx);
    auto* i64_type = llvm::Type::getInt64Ty(ctx);
    auto* ptr_type = llvm::PointerType::get(ctx, 0);

    // Embed metadata blob as a global constant
    llvm::Constant* meta_data = llvm::ConstantDataArray::get(ctx,
        llvm::ArrayRef<uint8_t>(metadata.data(), metadata.size()));
    auto* meta_gv = new llvm::GlobalVariable(module,
        meta_data->getType(), true, llvm::GlobalValue::PrivateLinkage,
        meta_data, "qore_aot_metadata");

    // Embed label as a global constant
    llvm::Constant* label_data = llvm::ConstantDataArray::getString(ctx,
        llvm::StringRef(label), true);  // null-terminated
    auto* label_gv = new llvm::GlobalVariable(module,
        label_data->getType(), true, llvm::GlobalValue::PrivateLinkage,
        label_data, "qore_aot_label");

    // Build the function table (same as v1)
    auto* func_entry_type = llvm::StructType::get(ctx, {ptr_type, ptr_type, i32_type, i32_type, i32_type, i32_type});

    std::vector<llvm::Constant*> func_entries;
    for (auto& cf : compiled_funcs) {
        llvm::Constant* name_str = llvm::ConstantDataArray::getString(ctx, cf.name, true);
        auto* name_gv = new llvm::GlobalVariable(module,
            name_str->getType(), true, llvm::GlobalValue::PrivateLinkage,
            name_str, "qore_aot_fname_" + cf.llvm_symbol);

        llvm::Function* fn = module.getFunction(cf.llvm_symbol);
        if (!fn) {
            printd(1, "AOT: warning: compiled function '%s' not found as symbol '%s' in module\n",
                cf.name.c_str(), cf.llvm_symbol.c_str());
            continue;
        }

        llvm::Constant* entry = llvm::ConstantStruct::get(func_entry_type, {
            name_gv,
            fn,
            llvm::ConstantInt::get(i32_type, cf.num_locals),
            llvm::ConstantInt::get(i32_type, cf.num_globals),
            llvm::ConstantInt::get(i32_type, cf.num_exprs),
            llvm::ConstantInt::get(i32_type, cf.num_stmts)
        });
        func_entries.push_back(entry);
    }

    llvm::GlobalVariable* func_table_gv;
    int num_funcs = (int)func_entries.size();

    if (num_funcs > 0) {
        auto* table_type = llvm::ArrayType::get(func_entry_type, num_funcs);
        llvm::Constant* func_table_init = llvm::ConstantArray::get(table_type, func_entries);
        func_table_gv = new llvm::GlobalVariable(module,
            table_type, true, llvm::GlobalValue::PrivateLinkage,
            func_table_init, "qore_aot_funcs");
    } else {
        func_table_gv = nullptr;
    }

    // Declare qore_aot_run_v2
    auto* aot_run_type = llvm::FunctionType::get(i32_type, {
        i32_type,       // argc
        ptr_type,       // argv
        ptr_type,       // metadata (const uint8_t*)
        i32_type,       // metadata_len
        ptr_type,       // label
        i64_type,       // parse_options
        ptr_type,       // functions
        i32_type        // num_functions
    }, false);
    auto aot_run_fn = module.getOrInsertFunction("qore_aot_run_v2", aot_run_type);

    // Generate main()
    auto* main_type = llvm::FunctionType::get(i32_type, {i32_type, ptr_type}, false);
    auto* main_fn = llvm::Function::Create(main_type, llvm::Function::ExternalLinkage, "main", module);

    auto* entry_bb = llvm::BasicBlock::Create(ctx, "entry", main_fn);
    llvm::IRBuilder<> builder(entry_bb);

    auto arg_it = main_fn->arg_begin();
    llvm::Value* argc_val = &*arg_it++;
    llvm::Value* argv_val = &*arg_it;

    // GEP to get pointers to metadata and label
    llvm::Value* meta_ptr = builder.CreateInBoundsGEP(
        meta_data->getType(), meta_gv,
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
        meta_ptr,
        builder.getInt32(static_cast<int>(metadata.size())),
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
                      bool static_link,
                      bool strip_source) {
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

    // Create shared debug info for all functions in this module
    llvm::DIBuilder di_builder(*module);
    auto* di_file = di_builder.createFile("<aot>", ".");
    auto* di_cu = di_builder.createCompileUnit(
        llvm::dwarf::DW_LANG_lo_user, di_file, "Qore AOT", false, "", 0);
    if (!module->getModuleFlag("Dwarf Version")) {
        module->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 5);
    }
    if (!module->getModuleFlag("Debug Info Version")) {
        module->addModuleFlag(llvm::Module::Warning, "Debug Info Version",
                llvm::DEBUG_METADATA_VERSION);
    }

    qore_program_private* pp = qore_program_private::get(*pgm);
    qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);
    compileNamespaceFunctions(root_ns, pgm, ctx, *module, di_builder, di_cu,
        compiled_funcs, total_funcs, compiled_count, failed_count);

    // Step 2: Try to compile top-level code with AOT mode
    {
        TopLevelStatementBlock& sb = pp->sb;
        QoreIRFunction* ir_func = new QoreIRFunction("_toplevel");

        // Collect ALL body locals from the statement tree (top-level + nested blocks
        // from fully-lowered statements like if/for/while/try/switch).  These are
        // marked as pre-instantiated so the LLVM lowerer doesn't emit
        // qore_rt_instantiate_local/uninstantiate_local calls.
        collectAllStatementLocals(&sb, ir_func->all_body_locals);
        for (LocalVar* lv : ir_func->all_body_locals) {
            ir_func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(lv));
        }

        QoreIRBuilder builder(ir_func);
        auto* entry = ir_func->createBlock("entry");
        builder.setBlock(entry);

        QoreParseContext parse_context(pgm);
        QoreIRLowering lowering(builder, &parse_context);
        std::string lower_error;
        bool toplevel_ok = false;
        if (lowering.lowerStatementBlock(&sb, lower_error)) {
            // Ensure terminator for the current block (where the builder is pointing)
            // After lowering control flow constructs like try-catch, the builder may be pointing
            // to a merge block that needs an implicit return
            QoreIRBasicBlock* current_block = builder.getBlock();
            if (current_block && (current_block->instructions.empty() ||
                    !isTerminator(current_block->instructions.back()->opcode))) {
                builder.createReturnNothing();
            }
            if (getenv("QORE_AOT_DUMP_IR")) {
                fprintf(stderr, "=== IR for _toplevel ===\n");
                QoreIRPrinter::print(*ir_func, std::cerr);
                fprintf(stderr, "========================\n");
            }
            std::string verify_error;
            if (QoreIRVerifier::verify(*ir_func, verify_error)) {
                // Classify locals as IR-only vs AST-visible for optimization
                ir_func->computeIROnlyLocals();

                // Build slot map for AOT pointer indirection
                AOTSlotMap slots;
                buildAOTSlotMap(*ir_func, slots);

                QoreIRToLLVM llvm_lowerer(ctx);
                llvm_lowerer.setAOTMode(&slots);
                llvm_lowerer.setSharedDebugInfo(&di_builder, di_cu);
                std::string llvm_error;
                if (llvm_lowerer.lowerFunction(*ir_func, *module, llvm_error)) {
                    AOTCompiledFunc cf;
                    cf.name = "_toplevel";
                    cf.llvm_symbol = ir_func->name;
                    cf.num_locals = static_cast<int>(slots.local_slots.size());
                    cf.num_globals = static_cast<int>(slots.global_slots.size());
                    cf.num_exprs = static_cast<int>(slots.expr_slots.size());
                    cf.num_stmts = static_cast<int>(slots.stmt_slots.size());
                    // Extract slot identities (no uvb for toplevel)
                    extractAOTSlotIdentities(*ir_func, slots, nullptr, cf.slot_ids);
                    compiled_funcs.push_back(std::move(cf));
                    ++compiled_count;
                    toplevel_ok = true;
                    printd(2, "AOT: compiled _toplevel to LLVM IR (locals=%d, globals=%d, exprs=%d, stmts=%d)\n",
                        (int)slots.local_slots.size(), (int)slots.global_slots.size(),
                        (int)slots.expr_slots.size(), (int)slots.stmt_slots.size());
                } else {
                    if (getenv("QORE_AOT_DEBUG")) {
                        fprintf(stderr, "AOT: LLVM lowering failed for _toplevel: %s\n",
                            llvm_error.c_str());
                    }
                    printd(2, "AOT: LLVM lowering failed for _toplevel: %s\n", llvm_error.c_str());
                }
            } else {
                if (getenv("QORE_AOT_DEBUG")) {
                    fprintf(stderr, "AOT: _toplevel verification failed: %s\n",
                        verify_error.c_str());
                }
                printd(2, "AOT: _toplevel verification failed: %s\n", verify_error.c_str());
            }
        } else {
            if (getenv("QORE_AOT_DEBUG")) {
                fprintf(stderr, "AOT: _toplevel IR lowering failed: %s\n", lower_error.c_str());
            }
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

    // Use the program's actual parse options (includes directives like %modern)
    parse_options = pgm->getParseOptions64();

    // Step 3: Generate main() and function registration table
    if (strip_source) {
        // Build serialized metadata blob
        QoreAOTBinaryWriter writer;
        QoreAOTBinaryHeader hdr{};
        hdr.magic = QORE_AOT_BINARY_MAGIC;
        hdr.version = QORE_AOT_BINARY_VERSION;
        hdr.flags = QORE_AOT_FLAG_HAS_TOPLEVEL;
        hdr.parse_options = parse_options;
        hdr.label_offset = writer.strings.add(label);

        // Serialize namespace tree
        if (!serializeNamespaceTree(writer, root_ns)) {
            error = "failed to serialize namespace tree";
            return false;
        }

        // Build slot map descriptors and serialize
        std::vector<AOTCompiledFuncWithSlots> func_slots;
        for (auto& cf : compiled_funcs) {
            AOTCompiledFuncWithSlots fws;
            fws.name = cf.name;
            fws.num_locals = cf.num_locals;
            fws.num_globals = cf.num_globals;
            fws.num_exprs = cf.num_exprs;
            fws.slot_ids = cf.slot_ids;
            func_slots.push_back(std::move(fws));
        }
        serializeSlotMaps(writer, func_slots);

        // Serialize fallback sources (only if any functions need source fallback)
        serializeFallbackSources(writer, func_slots, source_text, source_len);

        // Finalize metadata blob
        std::vector<uint8_t> metadata;
        hdr.section_count = 0;  // filled by finalize
        if (!writer.finalize(hdr, metadata)) {
            error = "failed to finalize binary metadata";
            return false;
        }

        printf("AOT: metadata blob: %d bytes (source-stripped)\n", (int)metadata.size());

        generateMainAndTableV2(ctx, *module, metadata, label, parse_options, compiled_funcs);
    } else {
        generateMainAndTable(ctx, *module, source_text, source_len, label,
            parse_options, compiled_funcs);
    }

    // Finalize shared debug info after all functions are lowered
    di_builder.finalize();

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

//! Extract ALL module dependencies from source (including reexport)
/** For strip-source mode, we need to serialize all dependencies so they can be loaded
    at runtime before deserializing the namespace tree.
*/
static std::vector<std::string> extractAllDependencies(const char* source, int source_len) {
    std::vector<std::string> deps;
    const char* p = source;
    const char* end = source + source_len;
    bool in_block_comment = false;

    while (p < end) {
        // Check for block comment start/end
        if (!in_block_comment && p + 1 < end && p[0] == '/' && p[1] == '*') {
            in_block_comment = true;
            p += 2;
            continue;
        }
        if (in_block_comment && p + 1 < end && p[0] == '*' && p[1] == '/') {
            in_block_comment = false;
            p += 2;
            continue;
        }
        if (in_block_comment) {
            ++p;
            continue;
        }

        // Skip whitespace at start of line
        while (p < end && (*p == ' ' || *p == '\t')) {
            ++p;
        }

        // Skip line comments (# ...)
        if (p < end && *p == '#') {
            while (p < end && *p != '\n') {
                ++p;
            }
            if (p < end) {
                ++p;
            }
            continue;
        }

        // Check for %requires directive (include reexport)
        if (p + 9 <= end && strncmp(p, "%requires", 9) == 0) {
            p += 9;
            // Skip whitespace after %requires
            while (p < end && (*p == ' ' || *p == '\t')) {
                ++p;
            }
            // Skip optional (reexport) - but still include the module!
            if (p + 10 <= end && strncmp(p, "(reexport)", 10) == 0) {
                p += 10;
                while (p < end && (*p == ' ' || *p == '\t')) {
                    ++p;
                }
            }
            // Now read the module name (stop at whitespace, newline, or version operators)
            const char* name_start = p;
            while (p < end && *p != '\n' && *p != ' ' && *p != '\t' &&
                   *p != '<' && *p != '>' && *p != '=') {
                ++p;
            }
            if (p > name_start) {
                std::string dep_name(name_start, p - name_start);
                // Skip "qore" as it's always available
                if (dep_name != "qore") {
                    deps.push_back(dep_name);
                }
            }
        }

        // Skip to end of line
        while (p < end && *p != '\n') {
            if (p + 1 < end && p[0] == '/' && p[1] == '*') {
                in_block_comment = true;
                p += 2;
                break;
            }
            ++p;
        }
        if (p < end && *p == '\n') {
            ++p;
        }
    }

    return deps;
}

//! Parse module metadata from .qm source text
/** Extracts name, version, desc, author, url, and license from the module { ... } block.
    Falls back to deriving the module name from the filename label.
*/
static bool parseModuleMetadata(const char* source, int source_len, const char* label,
        QoreAOTModuleInfo& info, std::string& error) {
    std::string src(source, source_len);

    // Find "module NAME {" pattern - must be at start of line (not in comments)
    // Look for "module" preceded by nothing or whitespace only on its line
    size_t mod_pos = 0;
    bool found = false;
    while ((mod_pos = src.find("module", mod_pos)) != std::string::npos) {
        // Check if this "module" is at line start or preceded only by whitespace on its line
        bool valid_start = (mod_pos == 0);
        if (!valid_start) {
            // Walk backwards to start of line
            size_t line_start = mod_pos;
            while (line_start > 0 && src[line_start - 1] != '\n') {
                --line_start;
            }
            // Check if only whitespace between line start and "module"
            valid_start = true;
            for (size_t i = line_start; i < mod_pos; ++i) {
                if (src[i] != ' ' && src[i] != '\t') {
                    valid_start = false;
                    break;
                }
            }
        }
        if (valid_start) {
            // Check that "module" is followed by whitespace (not part of another word)
            size_t after = mod_pos + 6;
            if (after < src.size() && (src[after] == ' ' || src[after] == '\t')) {
                found = true;
                break;
            }
        }
        ++mod_pos;
    }
    if (!found) {
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

    // Extract non-reexport dependencies from %requires directives
    // These are exported so the module manager can load them before calling init.
    // IMPORTANT: We skip (reexport) dependencies to avoid circular dependency issues.
    // Reexported modules often depend on types from this module, creating a cycle
    // where the dependency can't load because this module's types don't exist yet.
    // NOTE: We must skip %requires inside comments (block comments and line comments).
    // Also check %try-module directives and warn if the module is not available.
    {
        const char* p = source;
        const char* end = source + source_len;
        bool in_block_comment = false;

        while (p < end) {
            // Check for block comment start/end
            if (!in_block_comment && p + 1 < end && p[0] == '/' && p[1] == '*') {
                in_block_comment = true;
                p += 2;
                continue;
            }
            if (in_block_comment && p + 1 < end && p[0] == '*' && p[1] == '/') {
                in_block_comment = false;
                p += 2;
                continue;
            }
            if (in_block_comment) {
                ++p;
                continue;
            }

            // Skip whitespace at start of line
            while (p < end && (*p == ' ' || *p == '\t')) {
                ++p;
            }

            // Skip line comments (# ...)
            if (p < end && *p == '#') {
                while (p < end && *p != '\n') {
                    ++p;
                }
                if (p < end) {
                    ++p;  // skip newline
                }
                continue;
            }

            // Check for %try-module directive and warn if module not available
            if (p + 11 <= end && strncmp(p, "%try-module", 11) == 0) {
                p += 11;
                while (p < end && (*p == ' ' || *p == '\t')) {
                    ++p;
                }
                const char* mod_start = p;
                while (p < end && *p != '\n' && *p != ' ' && *p != '\t') {
                    ++p;
                }
                if (p > mod_start) {
                    std::string mod_name(mod_start, p - mod_start);
                    // Try to load the module to check if it's available
                    ExceptionSink xsink;
                    int rc = MM.runTimeLoadModule(&xsink, mod_name.c_str(), nullptr);
                    if (rc < 0 || xsink) {
                        fprintf(stderr, "AOT warning: optional module '%s' is not available during compilation\n",
                            mod_name.c_str());
                        fprintf(stderr, "             '%s'-dependent functionality will be disabled in the compiled module\n",
                            mod_name.c_str());
                    }
                    xsink.clear();
                }
                // Skip to end of line
                while (p < end && *p != '\n') {
                    ++p;
                }
                if (p < end) {
                    ++p;
                }
                continue;
            }

            // Check for %requires directive (must be at start of logical line)
            if (p + 9 <= end && strncmp(p, "%requires", 9) == 0) {
                p += 9;
                // Skip whitespace
                while (p < end && (*p == ' ' || *p == '\t')) {
                    ++p;
                }
                // Check for (reexport) - skip reexport dependencies
                bool is_reexport = false;
                if (p + 10 <= end && strncmp(p, "(reexport)", 10) == 0) {
                    is_reexport = true;
                    p += 10;
                    while (p < end && (*p == ' ' || *p == '\t')) {
                        ++p;
                    }
                }
                // Now read the module name (stop at whitespace, newline, or version operators)
                const char* name_start = p;
                while (p < end && *p != '\n' && *p != ' ' && *p != '\t' &&
                       *p != '<' && *p != '>' && *p != '=') {
                    ++p;
                }
                if (p > name_start && !is_reexport) {
                    std::string dep_name(name_start, p - name_start);
                    // Skip common system modules that are always available
                    if (dep_name != "qore") {
                        info.dependencies.push_back(dep_name);
                    }
                }
            }

            // Skip to end of line
            while (p < end && *p != '\n') {
                // Also check for block comment start within the line
                if (p + 1 < end && p[0] == '/' && p[1] == '*') {
                    in_block_comment = true;
                    p += 2;
                    break;
                }
                ++p;
            }
            if (p < end && *p == '\n') {
                ++p;  // skip newline
            }
        }
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

    // Export module dependencies as null-terminated array of C strings
    // The module manager uses this to load dependencies before calling init
    if (!mod_info.dependencies.empty()) {
        std::vector<llvm::GlobalVariable*> dep_string_gvs;
        for (const auto& dep : mod_info.dependencies) {
            auto* dep_str = llvm::ConstantDataArray::getString(ctx, dep, true);
            auto* dep_gv = new llvm::GlobalVariable(module, dep_str->getType(), true,
                llvm::GlobalValue::PrivateLinkage, dep_str,
                "qore_aot_dep_" + dep);
            dep_string_gvs.push_back(dep_gv);
        }
        // Create array of pointers to dependency strings, plus null terminator
        std::vector<llvm::Constant*> dep_ptrs;
        for (auto* gv : dep_string_gvs) {
            dep_ptrs.push_back(llvm::ConstantExpr::getPointerCast(gv, ptr_type));
        }
        dep_ptrs.push_back(llvm::ConstantPointerNull::get(llvm::PointerType::get(ctx, 0)));
        auto* dep_array = llvm::ConstantArray::get(
            llvm::ArrayType::get(ptr_type, dep_ptrs.size()), dep_ptrs);
        auto* deps_gv = new llvm::GlobalVariable(module, dep_array->getType(), true,
            llvm::GlobalValue::ExternalLinkage, dep_array, "qore_module_dependencies");
        deps_gv->setDLLStorageClass(llvm::GlobalValue::DLLExportStorageClass);
    }

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
    auto* func_entry_type = llvm::StructType::get(ctx, {ptr_type, ptr_type, i32_type, i32_type, i32_type, i32_type});
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
            llvm::ConstantInt::get(i32_type, cf.num_exprs),
            llvm::ConstantInt::get(i32_type, cf.num_stmts)
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

    // Create internal implementation function for module init
    llvm::Function* init_impl_fn;
    {
        auto* fn_type = llvm::FunctionType::get(ptr_type, {}, false);
        init_impl_fn = llvm::Function::Create(fn_type, llvm::Function::InternalLinkage,
            "__qore_aot_module_init_impl", module);

        auto* entry_bb = llvm::BasicBlock::Create(ctx, "entry", init_impl_fn);
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

    // Create internal implementation function for ns_init
    llvm::Function* ns_init_impl_fn;
    {
        auto* fn_type = llvm::FunctionType::get(void_type, {ptr_type, ptr_type}, false);
        ns_init_impl_fn = llvm::Function::Create(fn_type, llvm::Function::InternalLinkage,
            "__qore_aot_module_ns_init_impl", module);

        auto* entry_bb = llvm::BasicBlock::Create(ctx, "entry", ns_init_impl_fn);
        llvm::IRBuilder<> builder(entry_bb);

        auto arg_it = ns_init_impl_fn->arg_begin();
        llvm::Value* root_ns_val = &*arg_it++;
        llvm::Value* qore_ns_val = &*arg_it;

        builder.CreateCall(aot_mod_ns_init_fn, {root_ns_val, qore_ns_val});
        builder.CreateRetVoid();
    }

    // Create internal implementation function for delete
    llvm::Function* del_impl_fn;
    {
        auto* fn_type = llvm::FunctionType::get(void_type, {}, false);
        del_impl_fn = llvm::Function::Create(fn_type, llvm::Function::InternalLinkage,
            "__qore_aot_module_delete_impl", module);

        auto* entry_bb = llvm::BasicBlock::Create(ctx, "entry", del_impl_fn);
        llvm::IRBuilder<> builder(entry_bb);

        builder.CreateCall(aot_mod_del_fn, {});
        builder.CreateRetVoid();
    }

    // Export global function pointer variables (as binary modules expect)
    // qore_module_init: qore_module_init_t (function pointer)
    {
        new llvm::GlobalVariable(module, ptr_type, true,
            llvm::GlobalValue::ExternalLinkage, init_impl_fn, "qore_module_init");
    }

    // qore_module_ns_init: qore_module_ns_init_t (function pointer)
    {
        new llvm::GlobalVariable(module, ptr_type, true,
            llvm::GlobalValue::ExternalLinkage, ns_init_impl_fn, "qore_module_ns_init");
    }

    // qore_module_delete: qore_module_delete_t (function pointer)
    {
        new llvm::GlobalVariable(module, ptr_type, true,
            llvm::GlobalValue::ExternalLinkage, del_impl_fn, "qore_module_delete");
    }
}

//! Generate LLVM IR for source-stripped binary module ABI symbols
/** Like generateModuleABI but embeds metadata blob instead of source text,
    and calls qore_aot_module_init_v2 instead of qore_aot_module_init.
*/
static void generateModuleABIV2(llvm::LLVMContext& ctx, llvm::Module& module,
        const std::vector<uint8_t>& metadata, const char* label,
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

    // Embed metadata blob as a private global constant
    llvm::Constant* meta_data = llvm::ConstantDataArray::get(ctx,
        llvm::ArrayRef<uint8_t>(metadata.data(), metadata.size()));
    auto* meta_gv = new llvm::GlobalVariable(module,
        meta_data->getType(), true, llvm::GlobalValue::PrivateLinkage,
        meta_data, "qore_aot_mod_metadata");

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
    auto* func_entry_type = llvm::StructType::get(ctx, {ptr_type, ptr_type, i32_type, i32_type, i32_type, i32_type});
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
            llvm::ConstantInt::get(i32_type, cf.num_exprs),
            llvm::ConstantInt::get(i32_type, cf.num_stmts)
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

    // Declare qore_aot_module_init_v2: QoreStringNode* (metadata, metadata_len, label, parse_options, mod_name, funcs, num_funcs)
    auto* init_ret_type = ptr_type;
    auto* aot_mod_init_type = llvm::FunctionType::get(init_ret_type, {
        ptr_type, i32_type, ptr_type, i64_type, ptr_type, ptr_type, i32_type
    }, false);
    auto aot_mod_init_fn = module.getOrInsertFunction("qore_aot_module_init_v2", aot_mod_init_type);

    // Declare qore_aot_module_ns_init: void (root_ns, qore_ns)
    auto* aot_mod_ns_init_type = llvm::FunctionType::get(void_type, {ptr_type, ptr_type}, false);
    auto aot_mod_ns_init_fn = module.getOrInsertFunction("qore_aot_module_ns_init", aot_mod_ns_init_type);

    // Declare qore_aot_module_delete: void ()
    auto* aot_mod_del_type = llvm::FunctionType::get(void_type, {}, false);
    auto aot_mod_del_fn = module.getOrInsertFunction("qore_aot_module_delete", aot_mod_del_type);

    // Create internal implementation function for module init
    llvm::Function* init_impl_fn;
    {
        auto* fn_type = llvm::FunctionType::get(ptr_type, {}, false);
        init_impl_fn = llvm::Function::Create(fn_type, llvm::Function::InternalLinkage,
            "__qore_aot_module_init_impl", module);

        auto* entry_bb = llvm::BasicBlock::Create(ctx, "entry", init_impl_fn);
        llvm::IRBuilder<> builder(entry_bb);

        llvm::Value* meta_ptr = builder.CreateInBoundsGEP(
            meta_data->getType(), meta_gv,
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
            meta_ptr,
            builder.getInt32(static_cast<int>(metadata.size())),
            lbl_ptr,
            builder.getInt64(parse_options),
            name_ptr,
            funcs_ptr,
            builder.getInt32(num_funcs)
        });
        builder.CreateRet(result);
    }

    // Create internal implementation function for ns_init
    llvm::Function* ns_init_impl_fn;
    {
        auto* fn_type = llvm::FunctionType::get(void_type, {ptr_type, ptr_type}, false);
        ns_init_impl_fn = llvm::Function::Create(fn_type, llvm::Function::InternalLinkage,
            "__qore_aot_module_ns_init_impl", module);

        auto* entry_bb = llvm::BasicBlock::Create(ctx, "entry", ns_init_impl_fn);
        llvm::IRBuilder<> builder(entry_bb);

        auto arg_it = ns_init_impl_fn->arg_begin();
        llvm::Value* root_ns_val = &*arg_it++;
        llvm::Value* qore_ns_val = &*arg_it;

        builder.CreateCall(aot_mod_ns_init_fn, {root_ns_val, qore_ns_val});
        builder.CreateRetVoid();
    }

    // Create internal implementation function for delete
    llvm::Function* del_impl_fn;
    {
        auto* fn_type = llvm::FunctionType::get(void_type, {}, false);
        del_impl_fn = llvm::Function::Create(fn_type, llvm::Function::InternalLinkage,
            "__qore_aot_module_delete_impl", module);

        auto* entry_bb = llvm::BasicBlock::Create(ctx, "entry", del_impl_fn);
        llvm::IRBuilder<> builder(entry_bb);

        builder.CreateCall(aot_mod_del_fn, {});
        builder.CreateRetVoid();
    }

    // Export global function pointer variables (as binary modules expect)
    // qore_module_init: qore_module_init_t (function pointer)
    {
        new llvm::GlobalVariable(module, ptr_type, true,
            llvm::GlobalValue::ExternalLinkage, init_impl_fn, "qore_module_init");
    }

    // qore_module_ns_init: qore_module_ns_init_t (function pointer)
    {
        new llvm::GlobalVariable(module, ptr_type, true,
            llvm::GlobalValue::ExternalLinkage, ns_init_impl_fn, "qore_module_ns_init");
    }

    // qore_module_delete: qore_module_delete_t (function pointer)
    {
        new llvm::GlobalVariable(module, ptr_type, true,
            llvm::GlobalValue::ExternalLinkage, del_impl_fn, "qore_module_delete");
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
                             const char* target_triple,
                             bool strip_source) {
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
    // Note: Do NOT call setNameInit() here - the scanner calls it when it parses
    // the "module Name" declaration, and calling it twice triggers an assertion.
    {
        QoreUserModuleDefContextHelper mod_ctx(mod_info.name.c_str(), label, *qpgm, xsink);

        // Use parsePending() + parseCommit() like interpreted module loading does.
        // This allows forward references in the init closure to be resolved during commit
        // after all module types/functions have been parsed.
        qpgm->parsePending(source_text, label, &xsink, &wsink, QP_WARN_DEFAULT);
        if (!xsink.isException()) {
            qpgm->parseCommit(&xsink, &wsink, QP_WARN_DEFAULT);
        }

        mod_ctx.close();
    }

    // Clean up phantom module dependencies created during compilation
    // (the module being compiled is not actually loaded, so any dependencies
    // created by trySetUserModuleDependency need to be removed)
    QMM.removeUserModuleDependency(mod_info.name.c_str());

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

    // Create shared debug info for all functions in this module
    llvm::DIBuilder di_builder(*module);
    auto* di_file = di_builder.createFile("<aot>", ".");
    auto* di_cu = di_builder.createCompileUnit(
        llvm::dwarf::DW_LANG_lo_user, di_file, "Qore AOT", false, "", 0);
    if (!module->getModuleFlag("Dwarf Version")) {
        module->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 5);
    }
    if (!module->getModuleFlag("Debug Info Version")) {
        module->addModuleFlag(llvm::Module::Warning, "Debug Info Version",
                llvm::DEBUG_METADATA_VERSION);
    }

    qore_program_private* pp = qore_program_private::get(**qpgm);
    qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);

    // Compile functions from namespaces that BELONG to this module (not from dependencies)
    // Use the namespace's from_module field to identify ownership:
    // - Empty from_module: namespace defined in current parsing context (this module)
    // - from_module == mod_info.name: explicitly marked as belonging to this module
    // - from_module == other: namespace came from a dependency (skip)
    bool found_any = false;
    for (auto ni = root_ns->nsl.nsmap.begin(); ni != root_ns->nsl.nsmap.end(); ++ni) {
        if (!ni->second) {
            continue;
        }
        qore_ns_private* ns_priv = qore_ns_private::get(*ni->second);
        const char* ns_module = ns_priv->getModuleName();

        // Skip namespaces from other modules (dependencies)
        if (ns_module && strcmp(ns_module, mod_info.name.c_str()) != 0) {
            printd(2, "AOT: skipping namespace '%s' from module '%s' (compiling '%s')\n",
                ni->first.c_str(), ns_module, mod_info.name.c_str());
            continue;
        }

        found_any = true;
        compileNamespaceFunctions(ns_priv, *qpgm, ctx, *module, di_builder, di_cu,
            compiled_funcs, total_funcs, compiled_count, failed_count);
    }
    if (!found_any) {
        error = "no module namespaces found after parsing (expected at least '" + mod_info.name + "')";
        return false;
    }

    printf("AOT module compilation: %d/%d functions pre-compiled (%d failed)\n",
        compiled_count, total_funcs, failed_count);

    // Step 4: Generate module ABI (instead of main + table)
    if (strip_source) {
        QoreAOTBinaryWriter writer;
        QoreAOTBinaryHeader hdr{};
        hdr.magic = QORE_AOT_BINARY_MAGIC;
        hdr.version = QORE_AOT_BINARY_VERSION;
        hdr.flags = QORE_AOT_FLAG_IS_MODULE;
        hdr.parse_options = mod_po;
        hdr.label_offset = writer.strings.add(label);

        // Serialize ALL dependencies (including reexport) so they can be loaded
        // at runtime before deserializing the namespace tree
        std::vector<std::string> all_deps = extractAllDependencies(source_text, source_len);
        serializeDependencies(writer, all_deps);

        // Pass module name to filter out items from reexported dependencies
        if (!serializeNamespaceTree(writer, root_ns, mod_info.name.c_str())) {
            error = "failed to serialize module namespace tree";
            return false;
        }

        std::vector<AOTCompiledFuncWithSlots> func_slots;
        for (auto& cf : compiled_funcs) {
            AOTCompiledFuncWithSlots fws;
            fws.name = cf.name;
            fws.num_locals = cf.num_locals;
            fws.num_globals = cf.num_globals;
            fws.num_exprs = cf.num_exprs;
            fws.slot_ids = cf.slot_ids;
            func_slots.push_back(std::move(fws));
        }
        serializeSlotMaps(writer, func_slots);
        serializeFallbackSources(writer, func_slots, source_text, source_len);

        std::vector<uint8_t> metadata;
        hdr.section_count = 0;
        if (!writer.finalize(hdr, metadata)) {
            error = "failed to finalize module binary metadata";
            return false;
        }

        printf("AOT: module metadata blob: %d bytes (source-stripped)\n", (int)metadata.size());

        generateModuleABIV2(ctx, *module, metadata, label, mod_po, mod_info, compiled_funcs);
    } else {
        generateModuleABI(ctx, *module, source_text, source_len, label,
            mod_po, mod_info, compiled_funcs);
    }

    // Finalize shared debug info after all functions are lowered
    di_builder.finalize();

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

bool QoreAOT::compileSeparatedModule(const char* dir_path,
                                     const std::string& output_path,
                                     int64_t parse_options,
                                     std::string& error,
                                     int opt_level,
                                     const char* target_triple,
                                     bool strip_source) {
    // Step 1: Extract module name from directory basename (handle trailing slash)
    std::string dir_str(dir_path);
    // Remove trailing slashes
    while (!dir_str.empty() && dir_str.back() == '/') {
        dir_str.pop_back();
    }
    if (dir_str.empty()) {
        error = "empty directory path";
        return false;
    }

    // Extract basename (module name)
    std::string mod_name;
    size_t last_slash = dir_str.rfind('/');
    if (last_slash != std::string::npos) {
        mod_name = dir_str.substr(last_slash + 1);
    } else {
        mod_name = dir_str;
    }

    if (mod_name.empty()) {
        error = "cannot determine module name from directory path";
        return false;
    }

    // Step 2: Construct path to main .qm file
    std::string qm_path = dir_str + "/" + mod_name + ".qm";

    // Step 3: Verify main .qm file exists and read it
    std::string main_source = QoreDir::get_file_content(qm_path.c_str());
    if (main_source.empty()) {
        // Check if file exists at all
        struct stat st;
        if (stat(qm_path.c_str(), &st) != 0) {
            error = "main module file not found: " + qm_path;
            return false;
        }
        // File exists but is empty - that's an error
        error = "main module file is empty: " + qm_path;
        return false;
    }

    // Step 4: Parse module metadata from main .qm file
    QoreAOTModuleInfo mod_info;
    if (!parseModuleMetadata(main_source.c_str(), (int)main_source.size(), qm_path.c_str(), mod_info, error)) {
        return false;
    }

    // Verify module name matches directory name
    if (mod_info.name != mod_name) {
        error = "module name '" + mod_info.name + "' in " + qm_path +
                " does not match directory name '" + mod_name + "'";
        return false;
    }

    printd(2, "AOT split module: name='%s' version='%s' desc='%s' dir='%s'\n",
        mod_info.name.c_str(), mod_info.version.c_str(), mod_info.desc.c_str(), dir_str.c_str());

    // Step 5: Create QoreProgram and set up module context
    int64_t mod_po = parse_options | PO_IN_MODULE | PO_NO_TOP_LEVEL_STATEMENTS
        | PO_REQUIRE_PROTOTYPES | PO_REQUIRE_OUR;

    ExceptionSink xsink;
    ExceptionSink wsink;
    QoreProgramHelper qpgm(mod_po, xsink);
    if (xsink.isException()) {
        xsink.handleExceptions();
        error = "failed to create QoreProgram for split module parsing";
        return false;
    }

    // Step 6: Parse with QoreUserModuleDefContextHelper for proper module context
    {
        QoreUserModuleDefContextHelper mod_ctx(mod_info.name.c_str(), qm_path.c_str(), *qpgm, xsink);

        // Parse main .qm file with parsePending()
        qpgm->parsePending(main_source.c_str(), qm_path.c_str(), &xsink, &wsink, QP_WARN_DEFAULT);
        if (xsink.isException()) {
            mod_ctx.close();
            xsink.handleExceptions();
            error = "parse error in main module file: " + qm_path;
            return false;
        }

        // Step 7: Glob .qc/.ql files and parse each
        QoreString regexClassesFunc(".+\\.(qc|ql)$");
        QoreDir moduleDir(&xsink, QCS_DEFAULT, dir_str.c_str());
        if (xsink.isException()) {
            mod_ctx.close();
            xsink.handleExceptions();
            error = "failed to open module directory: " + dir_str;
            return false;
        }

        ReferenceHolder<QoreListNode> fileList(moduleDir.list(&xsink, S_IFREG, &regexClassesFunc), &xsink);
        if (xsink.isException()) {
            mod_ctx.close();
            xsink.handleExceptions();
            error = "failed to list files in module directory: " + dir_str;
            return false;
        }

        // Collect all source files for embedding - we need the complete namespace tree at runtime
        // to properly register AOT functions on all classes and functions from .qc/.ql files.
        std::string combined_source = main_source;

        // Parse each .qc/.ql file for compilation and add to combined source
        if (fileList && fileList->size() > 0) {
            for (size_t i = 0; i < fileList->size(); ++i) {
                const QoreStringNode* filename = fileList->retrieveEntry(i).get<const QoreStringNode>();
                if (!filename) {
                    continue;
                }

                std::string file_path = dir_str + "/" + filename->c_str();
                std::string file_source = QoreDir::get_file_content(file_path.c_str());
                if (file_source.empty()) {
                    // Skip empty files but warn
                    printd(2, "AOT: warning: skipping empty file: %s\n", file_path.c_str());
                    continue;
                }

                qpgm->parsePending(file_source.c_str(), file_path.c_str(), &xsink, &wsink, QP_WARN_DEFAULT);
                if (xsink.isException()) {
                    mod_ctx.close();
                    xsink.handleExceptions();
                    error = "parse error in component file: " + file_path;
                    return false;
                }

                // Add this file's source to combined source for embedding
                if (!combined_source.empty() && combined_source.back() != '\n') {
                    combined_source += '\n';
                }
                combined_source += file_source;
            }
        }

        // Step 8: Commit parsing
        qpgm->parseCommit(&xsink);
        if (xsink.isException()) {
            mod_ctx.close();
            xsink.handleExceptions();
            error = "parse commit failed for split module: " + mod_name;
            return false;
        }

        mod_ctx.close();

        // Handle warnings
        if (wsink.isException()) {
            wsink.handleWarnings();
        }

        // Step 9: Initialize LLVM and compile functions
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
        llvm::InitializeNativeTargetAsmParser();

        llvm::LLVMContext ctx;
        auto module = std::make_unique<llvm::Module>("qore_aot_module_" + mod_info.name, ctx);

        std::vector<AOTCompiledFunc> compiled_funcs;
        int total_funcs = 0;
        int compiled_count = 0;
        int failed_count = 0;

        // Create shared debug info for all functions in this module
        llvm::DIBuilder di_builder(*module);
        auto* di_file = di_builder.createFile("<aot>", ".");
        auto* di_cu = di_builder.createCompileUnit(
            llvm::dwarf::DW_LANG_lo_user, di_file, "Qore AOT", false, "", 0);
        if (!module->getModuleFlag("Dwarf Version")) {
            module->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 5);
        }
        if (!module->getModuleFlag("Debug Info Version")) {
            module->addModuleFlag(llvm::Module::Warning, "Debug Info Version",
                    llvm::DEBUG_METADATA_VERSION);
        }

        qore_program_private* pp = qore_program_private::get(**qpgm);
        qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);

        // Compile functions from namespaces that BELONG to this module (not from dependencies)
        // Use the namespace's from_module field to identify ownership
        bool found_any = false;
        for (auto ni = root_ns->nsl.nsmap.begin(); ni != root_ns->nsl.nsmap.end(); ++ni) {
            if (!ni->second) {
                continue;
            }
            qore_ns_private* ns_priv = qore_ns_private::get(*ni->second);
            const char* ns_module = ns_priv->getModuleName();

            // Skip namespaces from other modules (dependencies)
            if (ns_module && strcmp(ns_module, mod_info.name.c_str()) != 0) {
                printd(2, "AOT: skipping namespace '%s' from module '%s' (compiling '%s')\n",
                    ni->first.c_str(), ns_module, mod_info.name.c_str());
                continue;
            }

            found_any = true;
            compileNamespaceFunctions(ns_priv, *qpgm, ctx, *module, di_builder, di_cu,
                compiled_funcs, total_funcs, compiled_count, failed_count);
        }
        if (!found_any) {
            error = "no module namespaces found after parsing (expected at least '" + mod_name + "')";
            return false;
        }

        printf("AOT split module compilation: %d/%d functions pre-compiled (%d failed)\n",
            compiled_count, total_funcs, failed_count);

        // Step 10: Generate module ABI
        if (strip_source) {
            QoreAOTBinaryWriter writer;
            QoreAOTBinaryHeader hdr{};
            hdr.magic = QORE_AOT_BINARY_MAGIC;
            hdr.version = QORE_AOT_BINARY_VERSION;
            hdr.flags = QORE_AOT_FLAG_IS_MODULE;
            hdr.parse_options = mod_po;
            hdr.label_offset = writer.strings.add(qm_path.c_str());

            // Serialize ALL dependencies (including reexport) so they can be loaded
            // at runtime before deserializing the namespace tree
            std::vector<std::string> all_deps = extractAllDependencies(combined_source.c_str(),
                static_cast<int>(combined_source.size()));
            serializeDependencies(writer, all_deps);

            // Pass module name to filter out items from reexported dependencies
            if (!serializeNamespaceTree(writer, root_ns, mod_info.name.c_str())) {
                error = "failed to serialize split module namespace tree";
                return false;
            }

            std::vector<AOTCompiledFuncWithSlots> func_slots;
            for (auto& cf : compiled_funcs) {
                AOTCompiledFuncWithSlots fws;
                fws.name = cf.name;
                fws.num_locals = cf.num_locals;
                fws.num_globals = cf.num_globals;
                fws.num_exprs = cf.num_exprs;
                fws.slot_ids = cf.slot_ids;
                func_slots.push_back(std::move(fws));
            }
            serializeSlotMaps(writer, func_slots);
            serializeFallbackSources(writer, func_slots, combined_source.c_str(), (int)combined_source.size());

            std::vector<uint8_t> metadata;
            hdr.section_count = 0;
            if (!writer.finalize(hdr, metadata)) {
                error = "failed to finalize split module binary metadata";
                return false;
            }

            printf("AOT: split module metadata blob: %d bytes (source-stripped)\n", (int)metadata.size());

            generateModuleABIV2(ctx, *module, metadata, qm_path.c_str(), mod_po, mod_info, compiled_funcs);
        } else {
            // Embed combined source (main .qm + all .qc/.ql files) for building namespace at runtime
            generateModuleABI(ctx, *module, combined_source.c_str(), (int)combined_source.size(),
                qm_path.c_str(), mod_po, mod_info, compiled_funcs);
        }

        // Finalize shared debug info after all functions are lowered
        di_builder.finalize();

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

        // Step 11: Emit PIC object file
        std::string obj_path = output_path + ".o";
        if (!emitObjectFile(*module, obj_path, error, opt_level, target_triple)) {
            return false;
        }

        // Step 12: Link as shared library
        if (!linkSharedLib(obj_path, output_path, error, target_triple)) {
            remove(obj_path.c_str());
            return false;
        }

        // Clean up object file (keep for cross-compilation)
        if (!target_triple) {
            remove(obj_path.c_str());
        }
    }

    // Clean up phantom module dependencies created during compilation
    // (the module being compiled is not actually loaded, so any dependencies
    // created by trySetUserModuleDependency need to be removed)
    QMM.removeUserModuleDependency(mod_info.name.c_str());

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
                case QoreIROpcode::CallDirect: {
                    auto* di = static_cast<QoreIRCallDirectInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &di->expr, sizeof(bits));
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
                case QoreIROpcode::InvokeSimError:
                case QoreIROpcode::CallClosureDirect: {
                    auto* ei = static_cast<QoreIRExprInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &ei->expr, sizeof(bits));
                    slots.getExprSlot(bits);
                    break;
                }
                case QoreIROpcode::LoadStaticVar: {
                    auto* svi = static_cast<QoreIRStaticVarInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &svi->expr, sizeof(bits));
                    slots.getExprSlot(bits);
                    break;
                }
                case QoreIROpcode::NewObject: {
                    auto* noi = static_cast<QoreIRNewObjectInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &noi->expr, sizeof(bits));
                    slots.getExprSlot(bits);
                    break;
                }
                case QoreIROpcode::LoadConstant: {
                    auto* lci = static_cast<QoreIRLoadConstantInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &lci->expr, sizeof(bits));
                    slots.getExprSlot(bits);
                    break;
                }
                case QoreIROpcode::CreateClosure: {
                    auto* cci = static_cast<QoreIRCreateClosureInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &cci->expr, sizeof(bits));
                    slots.getExprSlot(bits);
                    break;
                }
                case QoreIROpcode::CreateCallRef: {
                    auto* cri = static_cast<QoreIRCreateCallRefInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &cri->expr, sizeof(bits));
                    slots.getExprSlot(bits);
                    break;
                }
                case QoreIROpcode::CreateMethodRef: {
                    auto* mri = static_cast<QoreIRCreateMethodRefInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &mri->expr, sizeof(bits));
                    slots.getExprSlot(bits);
                    break;
                }
                case QoreIROpcode::CreateParseRef: {
                    auto* pri = static_cast<QoreIRCreateParseRefInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &pri->expr, sizeof(bits));
                    slots.getExprSlot(bits);
                    break;
                }
                case QoreIROpcode::NewHashDecl: {
                    auto* nhdi = static_cast<QoreIRNewHashDeclInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &nhdi->expr, sizeof(bits));
                    slots.getExprSlot(bits);
                    break;
                }
                case QoreIROpcode::NewComplexHash: {
                    auto* nchi = static_cast<QoreIRNewComplexHashInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &nchi->expr, sizeof(bits));
                    slots.getExprSlot(bits);
                    break;
                }
                case QoreIROpcode::NewComplexList: {
                    auto* ncli = static_cast<QoreIRNewComplexListInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &ncli->expr, sizeof(bits));
                    slots.getExprSlot(bits);
                    break;
                }
                case QoreIROpcode::VrnConstruct: {
                    auto* vrni = static_cast<QoreIRVrnConstructInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &vrni->expr, sizeof(bits));
                    slots.getExprSlot(bits);
                    break;
                }
                case QoreIROpcode::CallStaticDirect: {
                    auto* csdi = static_cast<QoreIRCallStaticDirectInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &csdi->expr, sizeof(bits));
                    slots.getExprSlot(bits);
                    break;
                }
                case QoreIROpcode::DotEvalMethodDirect: {
                    auto* dmd = static_cast<QoreIRDotEvalMethodDirectInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &dmd->expr, sizeof(bits));
                    slots.getExprSlot(bits);
                    break;
                }
                case QoreIROpcode::InvokeDotEvalMethodDirect: {
                    auto* idmd = static_cast<QoreIRInvokeDotEvalMethodDirectInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &idmd->expr, sizeof(bits));
                    slots.getExprSlot(bits);
                    break;
                }
                case QoreIROpcode::OnBlockExit: {
                    auto* obei = static_cast<QoreIROnBlockExitInstruction*>(inst.get());
                    StatementBlock* code = obei->stmt->getCode();
                    slots.getStmtSlot(reinterpret_cast<const void*>(code));
                    break;
                }
                default:
                    break;
            }
        }
    }
}

QoreAOTContext* buildAOTContext(const QoreIRFunction& func, int num_locals, int num_globals, int num_exprs, int num_stmts) {
    // Two-pass approach to avoid ref/deref issues on mismatch:
    // Pass 1: Count slots without taking refs
    // Pass 2: If counts match, take refs and fill context

    // Pass 1: Count unique slots
    int local_count = 0;
    int global_count = 0;
    int expr_count = 0;
    int stmt_count = 0;
    std::unordered_set<const void*> seen_locals;
    std::unordered_set<const void*> seen_globals;
    std::unordered_set<uint64_t> seen_exprs;
    std::unordered_set<const void*> seen_stmts;

    for (auto& block : func.blocks) {
        for (auto& inst : block->instructions) {
            switch (inst->opcode) {
                case QoreIROpcode::LoadLocal:
                case QoreIROpcode::StoreLocal: {
                    auto* li = static_cast<QoreIRLocalInstruction*>(inst.get());
                    const void* key = reinterpret_cast<const void*>(li->local);
                    if (seen_locals.insert(key).second) {
                        ++local_count;
                    }
                    break;
                }
                case QoreIROpcode::LoadGlobal:
                case QoreIROpcode::StoreGlobal:
                case QoreIROpcode::LoadThreadLocal:
                case QoreIROpcode::StoreThreadLocal: {
                    auto* vi = static_cast<QoreIRVarInstruction*>(inst.get());
                    const void* key = reinterpret_cast<const void*>(vi->var);
                    if (seen_globals.insert(key).second) {
                        ++global_count;
                    }
                    break;
                }
                case QoreIROpcode::LoadClosure:
                case QoreIROpcode::StoreClosure: {
                    auto* li = static_cast<QoreIRLocalInstruction*>(inst.get());
                    const void* key = reinterpret_cast<const void*>(li->local);
                    if (seen_locals.insert(key).second) {
                        ++local_count;
                    }
                    break;
                }
                case QoreIROpcode::Invoke: {
                    auto* ii = static_cast<QoreIRInvokeInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &ii->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        ++expr_count;
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
                    if (seen_exprs.insert(bits).second) {
                        ++expr_count;
                    }
                    break;
                }
                case QoreIROpcode::CallDirect: {
                    auto* di = static_cast<QoreIRCallDirectInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &di->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        ++expr_count;
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
                    if (seen_exprs.insert(bits).second) {
                        ++expr_count;
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
                case QoreIROpcode::InvokeSimError:
                case QoreIROpcode::CallClosureDirect: {
                    auto* ei = static_cast<QoreIRExprInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &ei->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        ++expr_count;
                    }
                    break;
                }
                case QoreIROpcode::LoadStaticVar: {
                    auto* svi = static_cast<QoreIRStaticVarInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &svi->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        ++expr_count;
                    }
                    break;
                }
                case QoreIROpcode::NewObject: {
                    auto* noi = static_cast<QoreIRNewObjectInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &noi->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        ++expr_count;
                    }
                    break;
                }
                case QoreIROpcode::LoadConstant: {
                    auto* lci = static_cast<QoreIRLoadConstantInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &lci->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        ++expr_count;
                    }
                    break;
                }
                case QoreIROpcode::CreateClosure: {
                    auto* cci = static_cast<QoreIRCreateClosureInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &cci->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        ++expr_count;
                    }
                    break;
                }
                case QoreIROpcode::CreateCallRef: {
                    auto* cri = static_cast<QoreIRCreateCallRefInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &cri->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        ++expr_count;
                    }
                    break;
                }
                case QoreIROpcode::CreateMethodRef: {
                    auto* mri = static_cast<QoreIRCreateMethodRefInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &mri->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        ++expr_count;
                    }
                    break;
                }
                case QoreIROpcode::CreateParseRef: {
                    auto* pri = static_cast<QoreIRCreateParseRefInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &pri->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        ++expr_count;
                    }
                    break;
                }
                case QoreIROpcode::NewHashDecl: {
                    auto* nhdi = static_cast<QoreIRNewHashDeclInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &nhdi->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        ++expr_count;
                    }
                    break;
                }
                case QoreIROpcode::NewComplexHash: {
                    auto* nchi = static_cast<QoreIRNewComplexHashInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &nchi->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        ++expr_count;
                    }
                    break;
                }
                case QoreIROpcode::NewComplexList: {
                    auto* ncli = static_cast<QoreIRNewComplexListInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &ncli->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        ++expr_count;
                    }
                    break;
                }
                case QoreIROpcode::VrnConstruct: {
                    auto* vrni = static_cast<QoreIRVrnConstructInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &vrni->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        ++expr_count;
                    }
                    break;
                }
                case QoreIROpcode::CallStaticDirect: {
                    auto* csdi = static_cast<QoreIRCallStaticDirectInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &csdi->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        ++expr_count;
                    }
                    break;
                }
                case QoreIROpcode::DotEvalMethodDirect: {
                    auto* dmd = static_cast<QoreIRDotEvalMethodDirectInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &dmd->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        ++expr_count;
                    }
                    break;
                }
                case QoreIROpcode::InvokeDotEvalMethodDirect: {
                    auto* idmd = static_cast<QoreIRInvokeDotEvalMethodDirectInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &idmd->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        ++expr_count;
                    }
                    break;
                }
                case QoreIROpcode::OnBlockExit: {
                    auto* obei = static_cast<QoreIROnBlockExitInstruction*>(inst.get());
                    StatementBlock* code = obei->stmt->getCode();
                    const void* key = reinterpret_cast<const void*>(code);
                    if (seen_stmts.insert(key).second) {
                        ++stmt_count;
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    // Check if counts match before taking any refs
    if (local_count != num_locals || global_count != num_globals ||
            expr_count != num_exprs || stmt_count != num_stmts) {
        // Slot count mismatch between compile time and runtime - this can happen due to
        // expression object sharing differences in the parser (e.g., ?? operator chains).
        // Return nullptr to fall back to AST interpretation for this function.
        printd(1, "buildAOTContext: slot mismatch for '%s' (expected l=%d g=%d e=%d s=%d, "
            "actual l=%d g=%d e=%d s=%d) - falling back to AST\n",
            func.name.c_str(), num_locals, num_globals, num_exprs, num_stmts,
            local_count, global_count, expr_count, stmt_count);
        return nullptr;
    }

    // Pass 2: Counts match, allocate context and fill with refs
    QoreAOTContext* ctx = new QoreAOTContext();
    ctx->num_locals = num_locals;
    ctx->num_globals = num_globals;
    ctx->num_exprs = num_exprs;
    ctx->num_stmts = num_stmts;
    ctx->allocate();

    // Copy all_body_locals from the fresh IR
    ctx->all_body_locals = func.all_body_locals;

    // Check if all body locals are IR-only (enables skipping instantiation in fast call path)
    ctx->all_body_locals_ir_only = func.areAllBodyLocalsIROnly();

    // Walk again in the same deterministic order to fill arrays
    int local_idx = 0;
    int global_idx = 0;
    int expr_idx = 0;
    int stmt_idx = 0;
    seen_locals.clear();
    seen_globals.clear();
    seen_exprs.clear();
    seen_stmts.clear();

    for (auto& block : func.blocks) {
        for (auto& inst : block->instructions) {
            switch (inst->opcode) {
                case QoreIROpcode::LoadLocal:
                case QoreIROpcode::StoreLocal: {
                    auto* li = static_cast<QoreIRLocalInstruction*>(inst.get());
                    const void* key = reinterpret_cast<const void*>(li->local);
                    if (seen_locals.insert(key).second) {
                        ctx->locals[local_idx++] = li->local;
                    }
                    break;
                }
                case QoreIROpcode::LoadGlobal:
                case QoreIROpcode::StoreGlobal:
                case QoreIROpcode::LoadThreadLocal:
                case QoreIROpcode::StoreThreadLocal: {
                    auto* vi = static_cast<QoreIRVarInstruction*>(inst.get());
                    const void* key = reinterpret_cast<const void*>(vi->var);
                    if (seen_globals.insert(key).second) {
                        ctx->globals[global_idx++] = vi->var;
                    }
                    break;
                }
                case QoreIROpcode::LoadClosure:
                case QoreIROpcode::StoreClosure: {
                    auto* li = static_cast<QoreIRLocalInstruction*>(inst.get());
                    const void* key = reinterpret_cast<const void*>(li->local);
                    if (seen_locals.insert(key).second) {
                        ctx->locals[local_idx++] = li->local;
                    }
                    break;
                }
                case QoreIROpcode::Invoke: {
                    auto* ii = static_cast<QoreIRInvokeInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &ii->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        // Take a ref so the expression survives IR function deletion
                        ii->expr.ref();
                        ctx->exprs[expr_idx++] = bits;
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
                    if (seen_exprs.insert(bits).second) {
                        // Take a ref so the expression survives IR function deletion
                        ei->expr.ref();
                        ctx->exprs[expr_idx++] = bits;
                    }
                    break;
                }
                case QoreIROpcode::CallDirect: {
                    auto* di = static_cast<QoreIRCallDirectInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &di->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        // Take a ref so the expression survives IR function deletion
                        di->expr.ref();
                        int32_t slot = expr_idx;
                        ctx->exprs[expr_idx++] = bits;

                        // Pre-resolve call target to avoid per-call dynamic_cast
                        QoreValue expr_val;
                        std::memcpy(&expr_val, &bits, sizeof(expr_val));
                        const auto* call = dynamic_cast<const FunctionCallNode*>(
                            expr_val.getInternalNode());
                        if (call && call->getFunction() && call->getVariant()) {
                            ctx->call_targets[slot].func = call->getFunction();
                            ctx->call_targets[slot].variant = call->getVariant();
                            ctx->call_targets[slot].pgm = call->getProgram();
                        }
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
                    if (seen_exprs.insert(bits).second) {
                        // Take a ref so the lvalue survives IR function deletion
                        lvi->lvalue.ref();
                        ctx->exprs[expr_idx++] = bits;
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
                case QoreIROpcode::InvokeSimError:
                case QoreIROpcode::CallClosureDirect: {
                    auto* ei = static_cast<QoreIRExprInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &ei->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        // Take a ref so the expression survives IR function deletion
                        ei->expr.ref();
                        ctx->exprs[expr_idx++] = bits;
                    }
                    break;
                }
                case QoreIROpcode::LoadStaticVar: {
                    auto* svi = static_cast<QoreIRStaticVarInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &svi->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        // Take a ref so the expression survives IR function deletion
                        svi->expr.ref();
                        ctx->exprs[expr_idx++] = bits;
                    }
                    break;
                }
                case QoreIROpcode::NewObject: {
                    auto* noi = static_cast<QoreIRNewObjectInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &noi->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        noi->expr.ref();
                        ctx->exprs[expr_idx++] = bits;
                    }
                    break;
                }
                case QoreIROpcode::LoadConstant: {
                    auto* lci = static_cast<QoreIRLoadConstantInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &lci->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        lci->expr.ref();
                        ctx->exprs[expr_idx++] = bits;
                    }
                    break;
                }
                case QoreIROpcode::CreateClosure: {
                    auto* cci = static_cast<QoreIRCreateClosureInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &cci->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        cci->expr.ref();
                        ctx->exprs[expr_idx++] = bits;
                    }
                    break;
                }
                case QoreIROpcode::CreateCallRef: {
                    auto* cri = static_cast<QoreIRCreateCallRefInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &cri->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        cri->expr.ref();
                        ctx->exprs[expr_idx++] = bits;
                    }
                    break;
                }
                case QoreIROpcode::CreateMethodRef: {
                    auto* mri = static_cast<QoreIRCreateMethodRefInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &mri->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        mri->expr.ref();
                        ctx->exprs[expr_idx++] = bits;
                    }
                    break;
                }
                case QoreIROpcode::CreateParseRef: {
                    auto* pri = static_cast<QoreIRCreateParseRefInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &pri->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        pri->expr.ref();
                        ctx->exprs[expr_idx++] = bits;
                    }
                    break;
                }
                case QoreIROpcode::NewHashDecl: {
                    auto* nhdi = static_cast<QoreIRNewHashDeclInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &nhdi->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        nhdi->expr.ref();
                        ctx->exprs[expr_idx++] = bits;
                    }
                    break;
                }
                case QoreIROpcode::NewComplexHash: {
                    auto* nchi = static_cast<QoreIRNewComplexHashInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &nchi->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        nchi->expr.ref();
                        ctx->exprs[expr_idx++] = bits;
                    }
                    break;
                }
                case QoreIROpcode::NewComplexList: {
                    auto* ncli = static_cast<QoreIRNewComplexListInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &ncli->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        ncli->expr.ref();
                        ctx->exprs[expr_idx++] = bits;
                    }
                    break;
                }
                case QoreIROpcode::VrnConstruct: {
                    auto* vrni = static_cast<QoreIRVrnConstructInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &vrni->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        vrni->expr.ref();
                        ctx->exprs[expr_idx++] = bits;
                    }
                    break;
                }
                case QoreIROpcode::CallStaticDirect: {
                    auto* csdi = static_cast<QoreIRCallStaticDirectInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &csdi->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        csdi->expr.ref();
                        ctx->exprs[expr_idx++] = bits;
                    }
                    break;
                }
                case QoreIROpcode::DotEvalMethodDirect: {
                    auto* dmd = static_cast<QoreIRDotEvalMethodDirectInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &dmd->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        dmd->expr.ref();
                        ctx->exprs[expr_idx++] = bits;
                    }
                    break;
                }
                case QoreIROpcode::InvokeDotEvalMethodDirect: {
                    auto* idmd = static_cast<QoreIRInvokeDotEvalMethodDirectInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &idmd->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        idmd->expr.ref();
                        ctx->exprs[expr_idx++] = bits;
                    }
                    break;
                }
                case QoreIROpcode::OnBlockExit: {
                    auto* obei = static_cast<QoreIROnBlockExitInstruction*>(inst.get());
                    StatementBlock* code = obei->stmt->getCode();
                    const void* key = reinterpret_cast<const void*>(code);
                    if (seen_stmts.insert(key).second) {
                        ctx->stmts[stmt_idx++] = code;
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    return ctx;
}

//! Helper: get type path string for a QoreTypeInfo (empty string if null)
static std::string getSlotTypePath(const QoreTypeInfo* ti) {
    if (!ti) {
        return {};
    }
    return QoreTypeInfo::getPath(ti);
}

//! Classify an expression QoreValue for slot map serialization
/** Checks the AST node type and extracts identity info for supported types.
    Returns AOTExprKind::GENERIC_EVAL for unsupported types (function needs source fallback).
*/
static AOTExprSlotId classifyExpression(uint64_t bits, const AOTSlotMap& slots) {
    AOTExprSlotId id;
    QoreValue v;
    memcpy(&v, &bits, sizeof(v));

    if (!v.hasNode()) {
        // Scalar constant — shouldn't normally appear in expression slots
        id.kind = AOTExprKind::GENERIC_EVAL;
        return id;
    }

    const AbstractQoreNode* node = v.getInternalNode();
    if (!node) {
        id.kind = AOTExprKind::GENERIC_EVAL;
        return id;
    }

    // FunctionCallNode: regular function call
    if (auto* call = dynamic_cast<const FunctionCallNode*>(node)) {
        id.kind = AOTExprKind::FUNC_CALL;
        id.ref1 = call->getName();
        return id;
    }

    // SelfFunctionCallNode: method call on self
    if (auto* call = dynamic_cast<const SelfFunctionCallNode*>(node)) {
        id.kind = AOTExprKind::SELF_METHOD_CALL;
        const QoreMethod* method = call->getMethod();
        if (method) {
            const QoreClass* qc = method->getClass();
            if (qc) {
                id.ref1 = qc->getName();
            }
        }
        id.ref2 = call->getName();
        return id;
    }

    // StaticMethodCallNode: static method call
    if (auto* call = dynamic_cast<const StaticMethodCallNode*>(node)) {
        id.kind = AOTExprKind::STATIC_METHOD_CALL;
        const QoreMethod* method = call->getMethod();
        if (method) {
            const QoreClass* qc = method->getClass();
            if (qc) {
                id.ref1 = qc->getName();
            }
        }
        id.ref2 = call->getName();
        return id;
    }

    // NewObjectCallNode: constructor call
    if (dynamic_cast<const NewObjectCallNode*>(node)) {
        id.kind = AOTExprKind::NEW_OBJECT;
        // NewObjectCallNode::getTypeName() returns class name via virtual dispatch
        id.ref1 = node->getTypeName();
        return id;
    }

    // SelfVarrefNode: self variable reference
    if (dynamic_cast<const SelfVarrefNode*>(node)) {
        id.kind = AOTExprKind::SELF_VARREF;
        return id;
    }

    // VarRefNode: local variable reference (for lvalue operations)
    if (auto* varref = dynamic_cast<const VarRefNode*>(node)) {
        if (varref->getType() == VT_LOCAL || varref->getType() == VT_LOCAL_TS ||
            varref->getType() == VT_CLOSURE) {
            // Look up the local slot index
            const void* lv_ptr = reinterpret_cast<const void*>(varref->ref.id);
            auto it = slots.local_slots.find(lv_ptr);
            if (it != slots.local_slots.end()) {
                id.kind = AOTExprKind::LOCAL_VARREF;
                id.ref1 = std::to_string(it->second); // Store slot index as string
                return id;
            }
        }
        // Global variables fall through to GENERIC_EVAL for now
    }

    // Unsupported expression type — function needs source fallback
    id.kind = AOTExprKind::GENERIC_EVAL;
    printd(3, "AOT: unsupported expression type '%s' for slot serialization\n", node->getTypeName());
    return id;
}

//! Extract slot identities from a compiled function's IR and slot map
/** For each slot in the slot map, extracts identity information (name, type, flags)
    that can be serialized into the binary and used at runtime to reconstruct the
    QoreAOTContext without re-parsing source.

    @param func the IR function (for all_body_locals)
    @param slots the compiled slot map
    @param uvb the user variant base (for signature info: params, self, argv)
    @param out receives the extracted identities
*/
void extractAOTSlotIdentities(const QoreIRFunction& func, const AOTSlotMap& slots,
        UserVariantBase* uvb, AOTSlotIdentities& out) {
    // Get signature info for local classification
    UserSignature* sig = uvb ? uvb->getUserSignature() : nullptr;
    std::unordered_map<const void*, uint16_t> param_indices;
    const void* self_ptr = nullptr;
    const void* argv_ptr = nullptr;

    if (sig) {
        for (unsigned i = 0; i < sig->numParams(); ++i) {
            param_indices[reinterpret_cast<const void*>(sig->lv[i])] = static_cast<uint16_t>(i);
        }
        if (sig->selfid) {
            self_ptr = reinterpret_cast<const void*>(sig->selfid);
        }
        if (sig->argvid) {
            argv_ptr = reinterpret_cast<const void*>(sig->argvid);
        }
    }

    // Extract local slot identities (indexed by slot)
    out.locals.resize(slots.local_slots.size());
    for (auto& [ptr, slot] : slots.local_slots) {
        const LocalVar* lv = reinterpret_cast<const LocalVar*>(ptr);
        AOTLocalSlotId& lid = out.locals[slot];
        lid.name = lv->getName();
        lid.type_path = getSlotTypePath(lv->getTypeInfo());

        lid.flags = 0;
        lid.param_index = 0;
        if (ptr == self_ptr) {
            lid.flags |= 0x04; // is_self
        } else if (ptr == argv_ptr) {
            lid.flags |= 0x08; // is_argv
        } else {
            auto pit = param_indices.find(ptr);
            if (pit != param_indices.end()) {
                lid.flags |= 0x01; // is_param
                lid.param_index = pit->second;
            }
        }
        if (lv->closureUse()) {
            lid.flags |= 0x02; // is_closure
        }
    }

    // Extract global slot identities
    out.globals.resize(slots.global_slots.size());
    for (auto& [ptr, slot] : slots.global_slots) {
        const Var* var = reinterpret_cast<const Var*>(ptr);
        AOTGlobalSlotId& gid = out.globals[slot];
        gid.name = var->getName();
        gid.type_path = getSlotTypePath(var->getTypeInfo());
        gid.is_thread_local = var->isThreadLocal();
    }

    // Extract expression slot identities
    out.exprs.resize(slots.expr_slots.size());
    out.has_unsupported_exprs = false;
    for (auto& [bits, slot] : slots.expr_slots) {
        out.exprs[slot] = classifyExpression(bits, slots);
        if (out.exprs[slot].kind == AOTExprKind::GENERIC_EVAL) {
            out.has_unsupported_exprs = true;
        }
    }

    // Extract body local identities
    for (LocalVar* lv : func.all_body_locals) {
        AOTBodyLocalId blid;
        blid.name = lv->getName();
        blid.type_path = getSlotTypePath(lv->getTypeInfo());
        blid.is_closure = lv->closureUse();
        out.body_locals.push_back(std::move(blid));
    }

    // If this function has on_exit/on_success/on_error blocks (stmt_slots),
    // it requires source fallback since StatementBlock* cannot be symbolically
    // resolved at runtime without re-lowering from source
    if (!slots.stmt_slots.empty()) {
        out.has_unsupported_exprs = true;
    }
}

void QoreAOT::printSupportedTargets() {
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmPrinters();

    std::string default_triple = llvm::sys::getDefaultTargetTriple();
    printf("Default target: %s\n\n", default_triple.c_str());
    printf("Target triple format: <arch>-<vendor>-<os>[-<environment>]\n\n");

    printf("Registered architectures:\n");
    for (const auto& target : llvm::TargetRegistry::targets()) {
        printf("  %-24s %s\n", target.getName(), target.getShortDescription());
    }

    printf("\nVendors:\n ");
    for (int i = (int)llvm::Triple::UnknownVendor + 1; i <= (int)llvm::Triple::LastVendorType; ++i) {
        llvm::StringRef name = llvm::Triple::getVendorTypeName((llvm::Triple::VendorType)i);
        if (!name.empty()) {
            printf(" %s", name.str().c_str());
        }
    }
    printf("\n");

    printf("\nOperating systems:\n ");
    for (int i = (int)llvm::Triple::UnknownOS + 1; i <= (int)llvm::Triple::LastOSType; ++i) {
        llvm::StringRef name = llvm::Triple::getOSTypeName((llvm::Triple::OSType)i);
        if (!name.empty()) {
            printf(" %s", name.str().c_str());
        }
    }
    printf("\n");

    printf("\nEnvironments:\n ");
    for (int i = (int)llvm::Triple::UnknownEnvironment + 1; i <= (int)llvm::Triple::LastEnvironmentType; ++i) {
        llvm::StringRef name = llvm::Triple::getEnvironmentTypeName((llvm::Triple::EnvironmentType)i);
        if (!name.empty()) {
            printf(" %s", name.str().c_str());
        }
    }
    printf("\n");

    printf("\nExamples:\n");
    printf("  x86_64-pc-linux-gnu       Linux x86-64 (GNU libc)\n");
    printf("  aarch64-unknown-linux-gnu  Linux ARM64 (GNU libc)\n");
    printf("  x86_64-unknown-linux-musl  Linux x86-64 (musl libc / Alpine)\n");
    printf("  aarch64-apple-darwin       macOS ARM64\n");
    printf("  x86_64-apple-darwin        macOS x86-64\n");
}
