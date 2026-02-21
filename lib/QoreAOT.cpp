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
#include <set>
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
#include "qore/intern/QoreClosureParseNode.h"
#include "qore/intern/CallReferenceCallNode.h"
#include "qore/intern/StaticClassVarRefNode.h"
#include "qore/intern/ScopedObjectCallNode.h"
#include "qore/intern/QoreDotEvalOperatorNode.h"
#include "qore/intern/QoreHashObjectDereferenceOperatorNode.h"
#include "qore/intern/QoreSquareBracketsOperatorNode.h"
#include "qore/intern/QoreKeysOperatorNode.h"
#include "qore/intern/QoreElementsOperatorNode.h"
#include "qore/intern/QoreExistsOperatorNode.h"
#include "qore/intern/QoreDeleteOperatorNode.h"
#include "qore/intern/QoreRemoveOperatorNode.h"
#include "qore/intern/QoreBackgroundOperatorNode.h"
#include "qore/intern/QoreInstanceOfOperatorNode.h"
#include "qore/intern/QoreTrimOperatorNode.h"
#include "qore/intern/QoreChompOperatorNode.h"
#include "qore/intern/QorePushOperatorNode.h"
#include "qore/intern/QorePopOperatorNode.h"
#include "qore/intern/QoreUnshiftOperatorNode.h"
#include "qore/intern/QoreShiftOperatorNode.h"
#include "qore/intern/QoreListAssignmentOperatorNode.h"
#include "qore/intern/QoreExtractOperatorNode.h"
#include "qore/intern/QoreSpliceOperatorNode.h"
#include "qore/intern/ObjectMethodReferenceNode.h"
#include "qore/intern/ConstantList.h"
#include "qore/intern/QoreRegexMatchOperatorNode.h"
#include "qore/intern/QoreRegexNMatchOperatorNode.h"
#include "qore/intern/QoreRegexSubstOperatorNode.h"
#include "qore/intern/QoreRegexExtractOperatorNode.h"
#include "qore/intern/QoreTransliterationOperatorNode.h"
#include "qore/intern/QoreParseListNode.h"
#include "qore/intern/QoreAssignmentOperatorNode.h"
#include "qore/intern/QorePlusEqualsOperatorNode.h"
#include "qore/intern/QoreMinusEqualsOperatorNode.h"
#include "qore/intern/QorePlusOperatorNode.h"
#include "qore/intern/QoreMinusOperatorNode.h"
#include "qore/intern/QoreMultiplicationOperatorNode.h"
#include "qore/intern/QoreDivisionOperatorNode.h"
#include "qore/intern/QoreModuloOperatorNode.h"
#include "qore/intern/QoreLogicalAndOperatorNode.h"
#include "qore/intern/QoreLogicalOrOperatorNode.h"
#include "qore/intern/QoreLogicalEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalNotEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalAbsoluteEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalAbsoluteNotEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalLessThanOperatorNode.h"
#include "qore/intern/QoreLogicalGreaterThanOperatorNode.h"
#include "qore/intern/QoreLogicalLessThanOrEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalGreaterThanOrEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalComparisonOperatorNode.h"
#include "qore/intern/QoreLogicalNotOperatorNode.h"
#include "qore/intern/QoreNullCoalescingOperatorNode.h"
#include "qore/intern/QoreValueCoalescingOperatorNode.h"
#include "qore/intern/QoreQuestionMarkOperatorNode.h"
#include "qore/intern/QoreRangeOperatorNode.h"
#include "qore/intern/QoreUnaryMinusOperatorNode.h"
#include "qore/intern/QoreUnaryPlusOperatorNode.h"
#include "qore/intern/QoreBinaryNotOperatorNode.h"
#include "qore/intern/QoreShiftLeftOperatorNode.h"
#include "qore/intern/QoreShiftRightOperatorNode.h"
#include "qore/intern/QoreBinaryAndOperatorNode.h"
#include "qore/intern/QoreBinaryOrOperatorNode.h"
#include "qore/intern/QoreBinaryXorOperatorNode.h"
#include "qore/intern/QorePreIncrementOperatorNode.h"
#include "qore/intern/QorePreDecrementOperatorNode.h"
#include "qore/intern/QorePostIncrementOperatorNode.h"
#include "qore/intern/QorePostDecrementOperatorNode.h"
#include "qore/intern/QoreMultiplyEqualsOperatorNode.h"
#include "qore/intern/QoreDivideEqualsOperatorNode.h"
#include "qore/intern/QoreModuloEqualsOperatorNode.h"
#include "qore/intern/QoreAndEqualsOperatorNode.h"
#include "qore/intern/QoreOrEqualsOperatorNode.h"
#include "qore/intern/QoreXorEqualsOperatorNode.h"
#include "qore/intern/QoreShiftLeftEqualsOperatorNode.h"
#include "qore/intern/QoreShiftRightEqualsOperatorNode.h"
#include "qore/intern/QoreCastOperatorNode.h"
#include "qore/intern/QoreImplicitArgumentNode.h"
#include "qore/intern/QoreImplicitElementNode.h"
#include "qore/intern/ParseReferenceNode.h"
#include "qore/intern/QoreSquareBracketsRangeOperatorNode.h"
#include "qore/intern/QoreRegex.h"
#include "qore/intern/QoreRegexSubst.h"
#include "qore/intern/QoreTransliteration.h"
#include <qore/QoreNumberNode.h>
#include <qore/BinaryNode.h>

// Defined in Function.cpp - collects all local variables from a StatementBlock and nested blocks
extern void collectAllStatementLocals(const StatementBlock* block, std::vector<LocalVar*>& locals);

// Forward declaration
static std::vector<std::string> extractAllDependencies(const char* source, int source_len,
    std::vector<std::string>* reexport_deps = nullptr,
    std::unordered_set<std::string>* local_module_names = nullptr);

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
#include <dlfcn.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

//! Auto-detect the directory containing the loaded libqore using dladdr().
/** Falls back to QORE_LIBDIR env var, then compiled-in QORE_LIBDIR.
    @return directory path (without trailing slash)
*/
static std::string getLibqoreDir() {
    const char* env = getenv("QORE_LIBDIR");
    if (env) {
        return env;
    }
    // Use dladdr() to find where the loaded libqore.so lives
    Dl_info info;
    // Use a known symbol from libqore
    if (dladdr(reinterpret_cast<void*>(&qore_aot_run), &info) && info.dli_fname) {
        std::string path(info.dli_fname);
        size_t pos = path.rfind('/');
        if (pos != std::string::npos) {
            return path.substr(0, pos);
        }
    }
    return "";
}

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
    free(stmts);
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

//! Report AOT compilation statistics
static void reportAOTCompileStats(const char* label, int compiled_count, int total_funcs,
        int failed_count, const std::vector<AOTCompiledFunc>& compiled_funcs) {
    int unsupported_count = 0;
    for (auto& cf : compiled_funcs) {
        if (cf.slot_ids.has_unsupported_exprs) {
            ++unsupported_count;
        }
    }
    printf("AOT %s: %d/%d functions pre-compiled (%d failed, %d skipped)\n",
        label, compiled_count, total_funcs, failed_count,
        total_funcs - compiled_count - failed_count);
    if (unsupported_count > 0) {
        printf("AOT: %d/%d functions fully serialized, %d need source fallback\n",
            compiled_count - unsupported_count, compiled_count, unsupported_count);
    } else {
        printf("AOT: all %d functions fully serialized (no source fallback needed)\n",
            compiled_count);
    }
}

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
        int& total_funcs, int& compiled_count, int& failed_count,
        size_t& total_ir_insts_all,
        std::set<std::string>* compiled_keys = nullptr) {
    // Track compiled variant keys to skip duplicates from iterator yielding
    // the same variant twice (committed + pending)
    std::set<std::string> local_keys;
    if (!compiled_keys) {
        compiled_keys = &local_keys;
    }

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
            // Skip duplicate variant keys (iterator may yield committed + pending)
            if (!compiled_keys->insert(variant_key).second) {
                continue;
            }
            QoreIRFunction* ir_func = nullptr;
            std::string lower_error;
            int rc = tryLowerFunction(uvb, fname, pgm, ir_func, lower_error);

            if (rc == 0 && ir_func) {
                // Use variant key as LLVM symbol name to distinguish overloaded variants
                ir_func->name = variant_key;

                // Skip if another variant with the same LLVM function name was already compiled
                llvm::Function* existing = module.getFunction(ir_func->name);
                if (existing && !existing->empty()) {
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
                // Count total IR instructions and warn for large functions
                size_t total_ir_insts = 0;
                for (const auto& block : ir_func->blocks) {
                    total_ir_insts += block->instructions.size();
                }
                total_ir_insts_all += total_ir_insts;
                if (getenv("QORE_AOT_DEBUG")) {
                    fprintf(stderr, "AOT: lowering function '%s' (blocks=%zu, insts=%zu)\n",
                        variant_key.c_str(), ir_func->blocks.size(), total_ir_insts);
                }
                if (total_ir_insts > 5000) {
                    fprintf(stderr, "warning: function '%s' has %zu IR instructions; "
                        "LLVM compilation may take a long time; consider breaking this "
                        "function into smaller functions\n",
                        variant_key.c_str(), total_ir_insts);
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
        // Use namespace-qualified class path for unique LLVM symbol names
        const char* class_path = qc->getPath();
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
                // Strip leading :: from class_path if present (getPath() returns ::ClassName)
                const char* class_name = class_path;
                if (class_name[0] == ':' && class_name[1] == ':') {
                    class_name += 2;
                }
                std::string method_name = std::string(class_name) + "::" + meth->getName();
                // Generate unique key including parameter types to distinguish overloads
                std::string variant_key = getVariantKey(method_name.c_str(), variant);
                // Skip duplicate variant keys (iterator may yield committed + pending)
                if (!compiled_keys->insert(variant_key).second) {
                    continue;
                }
                QoreIRFunction* ir_func = nullptr;
                std::string lower_error;
                int rc = tryLowerFunction(uvb, method_name.c_str(), pgm, ir_func, lower_error);

                if (rc == 0 && ir_func) {
                    // Use variant key as LLVM symbol name to distinguish overloaded variants
                    ir_func->name = variant_key;

                    // Skip if another variant with the same LLVM function name was already compiled
                    llvm::Function* existing = module.getFunction(ir_func->name);
                    if (existing && !existing->empty()) {
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
                    // Count total IR instructions and warn for large functions
                    size_t total_ir_insts = 0;
                    for (const auto& block : ir_func->blocks) {
                        total_ir_insts += block->instructions.size();
                    }
                    total_ir_insts_all += total_ir_insts;
                    if (getenv("QORE_AOT_DEBUG")) {
                        fprintf(stderr, "AOT: lowering method '%s' (blocks=%zu, insts=%zu)\n",
                            variant_key.c_str(), ir_func->blocks.size(), total_ir_insts);
                    }
                    if (total_ir_insts > 5000) {
                        fprintf(stderr, "warning: method '%s' has %zu IR instructions; "
                            "LLVM compilation may take a long time; consider breaking this "
                            "method into smaller methods\n",
                            variant_key.c_str(), total_ir_insts);
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
                compiled_funcs, total_funcs, compiled_count, failed_count,
                total_ir_insts_all, compiled_keys);
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
        // Try auto-detected libqore directory first (covers both dev and installed)
        std::string libdir = getLibqoreDir();
        conf_path = libdir + "/aot-link.conf";
        if (!std::ifstream(conf_path).good()) {
            // Installed builds: config is in LIBDIR/qore/
            conf_path = libdir + "/qore/aot-link.conf";
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

    // Determine library directory (auto-detect from loaded libqore.so)
    std::string libqore_dir = getLibqoreDir();

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
//! Generate main() and function table for AOT binary.
/** Embeds serialized metadata blob and calls qore_aot_run_v2() from main().
*/
static void generateMainAndTableV2(llvm::LLVMContext& ctx, llvm::Module& module,
        const std::vector<uint8_t>& metadata, const char* label,
        const QoreParseOptions& parse_options, const std::vector<AOTCompiledFunc>& compiled_funcs) {
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

    // Declare qore_aot_run_v3 (accepts full 128-bit parse options as two i64)
    auto* aot_run_type = llvm::FunctionType::get(i32_type, {
        i32_type,       // argc
        ptr_type,       // argv
        ptr_type,       // metadata (const uint8_t*)
        i32_type,       // metadata_len
        ptr_type,       // label
        i64_type,       // parse_options_lo
        i64_type,       // parse_options_hi
        ptr_type,       // functions
        i32_type        // num_functions
    }, false);
    auto aot_run_fn = module.getOrInsertFunction("qore_aot_run_v3", aot_run_type);

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
        builder.getInt64(parse_options.getLo()),
        builder.getInt64(parse_options.getHi()),
        funcs_ptr,
        builder.getInt32(num_funcs)
    });

    builder.CreateRet(rc);
}

bool QoreAOT::compile(QoreProgram* pgm,
                      const char* source_text, int source_len,
                      const char* label,
                      const std::string& output_path,
                      const QoreParseOptions& parse_options,
                      std::string& error,
                      int opt_level,
                      const char* target_triple,
                      bool static_link,
                      bool include_source) {
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
    size_t total_ir_insts_all = 0;  // Track total IR instructions across all functions

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
        compiled_funcs, total_funcs, compiled_count, failed_count, total_ir_insts_all);

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

                // Count toplevel IR instructions
                for (const auto& block : ir_func->blocks) {
                    total_ir_insts_all += block->instructions.size();
                }

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
    reportAOTCompileStats("compilation", compiled_count, total_funcs + 1,
        failed_count, compiled_funcs);

    // Step 3: Build serialized metadata and generate main() + function registration table
    {
        QoreAOTBinaryWriter writer;
        QoreAOTBinaryHeader hdr{};
        hdr.magic = QORE_AOT_BINARY_MAGIC;
        hdr.version = QORE_AOT_BINARY_VERSION;
        hdr.flags = QORE_AOT_FLAG_HAS_TOPLEVEL;
        hdr.parse_options = parse_options.getLo();
        hdr.label_offset = writer.strings.add(label);
        // v2 fields: opcode compatibility and Qore version
        hdr.max_opcode_id = QORE_IR_MAX_OPCODE;
        hdr.qore_version_major = QORE_VERSION_MAJOR;
        hdr.qore_version_minor = QORE_VERSION_MINOR;
        hdr.qore_version_patch = QORE_VERSION_PATCH;
        // v3 field: high 64 bits of parse options
        hdr.parse_options_hi = parse_options.getHi();

        // Serialize dependencies from the parsed program's feature lists.
        // This captures ALL module dependencies including those from %include'd files,
        // which extractAllDependencies() would miss since it only scans raw source text.
        std::unordered_set<std::string> local_module_names;
        std::vector<std::string> all_deps;
        {
            qore_program_private* pp = qore_program_private::get(*pgm);
            // featureList contains builtin module names, userFeatureList contains user module names
            for (const auto& feat : pp->featureList) {
                if (feat != "qore") {
                    all_deps.push_back(feat);
                }
            }
            for (const auto& feat : pp->userFeatureList) {
                if (feat != "qore") {
                    all_deps.push_back(feat);
                }
            }
            // Also scan raw source for local module paths (./foo, ../foo) since these
            // can't be loaded by name at runtime and need to be kept in metadata
            extractAllDependencies(source_text, source_len, nullptr, &local_module_names);
        }
        serializeDependencies(writer, all_deps);

        // Serialize namespace tree - filter out module-originated items to avoid
        // deserializing private/internal module types that can't be resolved;
        // dependencies are loaded at runtime so their items will be available.
        // Items from local modules (relative paths like ./MyModule.qm) are kept
        // since they can't be loaded by name at runtime.
        if (!serializeNamespaceTree(writer, root_ns, "",
                local_module_names.empty() ? nullptr : &local_module_names)) {
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
            fws.num_stmts = cf.num_stmts;
            fws.slot_ids = cf.slot_ids;
            func_slots.push_back(std::move(fws));
        }
        serializeSlotMaps(writer, func_slots);

        // Serialize fallback sources only if --include-source was specified
        if (include_source) {
            serializeFallbackSources(writer, func_slots, source_text, source_len);
        }

        // Finalize metadata blob
        std::vector<uint8_t> metadata;
        hdr.section_count = 0;  // filled by finalize
        if (!writer.finalize(hdr, metadata)) {
            error = "failed to finalize binary metadata";
            return false;
        }

        printf("AOT: metadata blob: %d bytes\n", (int)metadata.size());

        generateMainAndTableV2(ctx, *module, metadata, label, parse_options, compiled_funcs);
    }

    // Finalize shared debug info after all functions are lowered
    di_builder.finalize();

    // Verify the complete module
    if (getenv("QORE_AOT_DEBUG")) {
        fprintf(stderr, "AOT: verifying LLVM module...\n");
    }
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
    if (getenv("QORE_AOT_DEBUG")) {
        fprintf(stderr, "AOT: emitting object file (O%d, total IR insts: %zu)...\n",
            opt_level, total_ir_insts_all);
    }
    if (!emitObjectFile(*module, obj_path, error, opt_level, target_triple)) {
        return false;
    }
    if (getenv("QORE_AOT_DEBUG")) {
        fprintf(stderr, "AOT: object file emitted\n");
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
    Also extracts which deps have (reexport) flag.
*/
static std::vector<std::string> extractAllDependencies(const char* source, int source_len,
        std::vector<std::string>* reexport_deps,
        std::unordered_set<std::string>* local_module_names) {
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
            // Check for optional (reexport) flag
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
            if (p > name_start) {
                std::string raw_path(name_start, p - name_start);
                // Check if this is a local module (relative path starting with ./ or ../)
                bool is_local = (raw_path.size() >= 2 && raw_path[0] == '.'
                    && (raw_path[1] == '/' || raw_path[1] == '.'));
                // Convert file paths (e.g. "../../../../qlib/QUnit.qm") to module names
                // by extracting the basename and removing the .qm extension
                std::string dep_name = raw_path;
                {
                    size_t slash_pos = dep_name.rfind('/');
                    if (slash_pos != std::string::npos) {
                        dep_name = dep_name.substr(slash_pos + 1);
                    }
                    // Remove .qm extension if present
                    if (dep_name.size() > 3 && dep_name.compare(dep_name.size() - 3, 3, ".qm") == 0) {
                        dep_name = dep_name.substr(0, dep_name.size() - 3);
                    }
                }
                // Skip "qore" as it's always available
                if (dep_name != "qore") {
                    // Track local module names (items from these modules are kept in metadata
                    // since they can't be loaded by name at runtime)
                    if (is_local && local_module_names) {
                        local_module_names->insert(dep_name);
                    }
                    deps.push_back(dep_name);
                    if (is_reexport && reexport_deps) {
                        reexport_deps->push_back(dep_name);
                    }
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

//! Map a license string to a qore_license_t enum value
/** Returns the enum value for "GPL", "LGPL", or "MIT" (case-insensitive).
    Defaults to QL_MIT for unrecognized strings.
*/
static int getLicenseEnum(const std::string& license_str) {
    if (license_str == "GPL" || license_str == "gpl") {
        return QL_GPL;
    }
    if (license_str == "LGPL" || license_str == "lgpl") {
        return QL_LGPL;
    }
    return QL_MIT;
}

//! Generate the API 2.0 module descriptor function and adapter functions
/** Creates:
    - Init adapter: wraps the AOT init impl (returns QoreStringNode*) into the
      qore_module_init_t signature (void(QoreModuleInitContext&, ExceptionSink&))
    - NS init adapter: wraps the AOT ns_init impl (2-arg) into the
      qore_module_ns_init_t signature (3-arg, ignoring ExceptionSink)
    - Module desc function: {sanitized_name}_qore_module_desc(QoreModuleInfo&)
      that calls qore_aot_fill_module_desc() with all metadata

    @param ctx LLVM context
    @param module LLVM module
    @param init_impl_fn the __qore_aot_module_init_impl function (returns ptr)
    @param ns_init_impl_fn the __qore_aot_module_ns_init_impl function (ptr, ptr -> void)
    @param del_impl_fn the __qore_aot_module_delete_impl function (void -> void)
    @param mod_info module metadata
*/
static void emitModuleDescFunction(llvm::LLVMContext& ctx, llvm::Module& module,
        llvm::Function* init_impl_fn, llvm::Function* ns_init_impl_fn,
        llvm::Function* del_impl_fn, const QoreAOTModuleInfo& mod_info) {
    auto* i32_type = llvm::Type::getInt32Ty(ctx);
    auto* ptr_type = llvm::PointerType::get(ctx, 0);
    auto* void_type = llvm::Type::getVoidTy(ctx);

    // Helper to create a private global C string constant
    auto createPrivateString = [&](const std::string& gv_name, const std::string& value) -> llvm::GlobalVariable* {
        auto* str_data = llvm::ConstantDataArray::getString(ctx, value, true);
        return new llvm::GlobalVariable(module, str_data->getType(), true,
            llvm::GlobalValue::PrivateLinkage, str_data, gv_name);
    };

    // --- Init adapter: void(ptr %ctx, ptr %xsink) ---
    // Calls init_impl_fn() -> QoreStringNode*, then if non-null calls
    // qore_aot_raise_init_error(xsink, result)
    llvm::Function* init_adapter_fn;
    {
        auto* fn_type = llvm::FunctionType::get(void_type, {ptr_type, ptr_type}, false);
        init_adapter_fn = llvm::Function::Create(fn_type, llvm::Function::InternalLinkage,
            "__qore_aot_module_init_adapter", module);

        auto arg_it = init_adapter_fn->arg_begin();
        llvm::Value* ctx_arg = &*arg_it++;  // QoreModuleInitContext* (unused)
        (void)ctx_arg;
        llvm::Value* xsink_arg = &*arg_it;

        auto* entry_bb = llvm::BasicBlock::Create(ctx, "entry", init_adapter_fn);
        auto* err_bb = llvm::BasicBlock::Create(ctx, "has_error", init_adapter_fn);
        auto* ok_bb = llvm::BasicBlock::Create(ctx, "no_error", init_adapter_fn);

        llvm::IRBuilder<> builder(entry_bb);

        // Call the init impl
        llvm::Value* result = builder.CreateCall(init_impl_fn);

        // Check if result is non-null
        llvm::Value* is_null = builder.CreateICmpEQ(result,
            llvm::ConstantPointerNull::get(llvm::PointerType::get(ctx, 0)));
        builder.CreateCondBr(is_null, ok_bb, err_bb);

        // Error path: call qore_aot_raise_init_error(xsink, result)
        builder.SetInsertPoint(err_bb);
        auto* raise_fn_type = llvm::FunctionType::get(void_type, {ptr_type, ptr_type}, false);
        auto raise_fn = module.getOrInsertFunction("qore_aot_raise_init_error", raise_fn_type);
        builder.CreateCall(raise_fn, {xsink_arg, result});
        builder.CreateRetVoid();

        // OK path: just return
        builder.SetInsertPoint(ok_bb);
        builder.CreateRetVoid();
    }

    // --- NS init adapter: void(ptr %root, ptr %qore, ptr %xsink) ---
    // Calls ns_init_impl_fn(root, qore) (ignores xsink)
    llvm::Function* ns_init_adapter_fn;
    {
        auto* fn_type = llvm::FunctionType::get(void_type, {ptr_type, ptr_type, ptr_type}, false);
        ns_init_adapter_fn = llvm::Function::Create(fn_type, llvm::Function::InternalLinkage,
            "__qore_aot_module_ns_init_adapter", module);

        auto arg_it = ns_init_adapter_fn->arg_begin();
        llvm::Value* root_arg = &*arg_it++;
        llvm::Value* qore_arg = &*arg_it++;
        llvm::Value* xsink_arg = &*arg_it;
        (void)xsink_arg;

        auto* entry_bb = llvm::BasicBlock::Create(ctx, "entry", ns_init_adapter_fn);
        llvm::IRBuilder<> builder(entry_bb);

        builder.CreateCall(ns_init_impl_fn, {root_arg, qore_arg});
        builder.CreateRetVoid();
    }

    // --- Module desc function: {sanitized_name}_qore_module_desc(ptr %mod_info) ---
    // Calls qore_aot_fill_module_desc() with all metadata, adapter function pointers,
    // and dependency array

    // Sanitize module name: replace hyphens with underscores for valid C identifier
    std::string sanitized_name = mod_info.name;
    for (char& c : sanitized_name) {
        if (c == '-') {
            c = '_';
        }
    }

    // Create string constants for metadata
    auto* name_gv = createPrivateString("qore_aot_desc_name", mod_info.name);
    auto* version_gv = createPrivateString("qore_aot_desc_version", mod_info.version);
    auto* desc_gv = createPrivateString("qore_aot_desc_desc", mod_info.desc);
    auto* author_gv = createPrivateString("qore_aot_desc_author", mod_info.author);
    auto* url_gv = createPrivateString("qore_aot_desc_url", mod_info.url.empty() ? "" : mod_info.url);
    auto* license_gv = createPrivateString("qore_aot_desc_license", mod_info.license);

    // Create dependency array (array of const char* pointers)
    int num_deps = static_cast<int>(mod_info.dependencies.size());
    llvm::GlobalVariable* deps_array_gv = nullptr;
    if (num_deps > 0) {
        std::vector<llvm::Constant*> dep_ptrs;
        for (const auto& dep : mod_info.dependencies) {
            auto* dep_gv = createPrivateString("qore_aot_desc_dep_" + dep, dep);
            dep_ptrs.push_back(dep_gv);
        }
        auto* deps_array = llvm::ConstantArray::get(
            llvm::ArrayType::get(ptr_type, num_deps), dep_ptrs);
        deps_array_gv = new llvm::GlobalVariable(module, deps_array->getType(), true,
            llvm::GlobalValue::PrivateLinkage, deps_array, "qore_aot_desc_deps");
    }

    // Declare qore_aot_fill_module_desc
    auto* fill_fn_type = llvm::FunctionType::get(void_type, {
        ptr_type,   // QoreModuleInfo*
        ptr_type,   // name
        ptr_type,   // version
        ptr_type,   // desc
        ptr_type,   // author
        ptr_type,   // url
        ptr_type,   // license_str
        i32_type,   // api_major
        i32_type,   // api_minor
        i32_type,   // license
        ptr_type,   // init_fn
        ptr_type,   // ns_init_fn
        ptr_type,   // del_fn
        ptr_type,   // deps
        i32_type    // num_deps
    }, false);
    auto fill_fn = module.getOrInsertFunction("qore_aot_fill_module_desc", fill_fn_type);

    // Create the exported desc function
    std::string desc_fn_name = sanitized_name + "_qore_module_desc";
    auto* desc_fn_type = llvm::FunctionType::get(void_type, {ptr_type}, false);
    llvm::Function* desc_fn = llvm::Function::Create(desc_fn_type,
        llvm::Function::ExternalLinkage, desc_fn_name, module);
    desc_fn->setDLLStorageClass(llvm::GlobalValue::DLLExportStorageClass);

    auto* entry_bb = llvm::BasicBlock::Create(ctx, "entry", desc_fn);
    llvm::IRBuilder<> builder(entry_bb);

    llvm::Value* mod_info_arg = &*desc_fn->arg_begin();

    // Build deps pointer
    llvm::Value* deps_ptr;
    if (deps_array_gv) {
        deps_ptr = builder.CreateInBoundsGEP(
            llvm::ArrayType::get(ptr_type, num_deps), deps_array_gv,
            {builder.getInt64(0), builder.getInt64(0)});
    } else {
        deps_ptr = llvm::ConstantPointerNull::get(llvm::PointerType::get(ctx, 0));
    }

    builder.CreateCall(fill_fn, {
        mod_info_arg,
        name_gv,
        version_gv,
        desc_gv,
        author_gv,
        url_gv,
        license_gv,
        builder.getInt32(QORE_MODULE_API_MAJOR),
        builder.getInt32(QORE_MODULE_API_MINOR),
        builder.getInt32(getLicenseEnum(mod_info.license)),
        init_adapter_fn,
        ns_init_adapter_fn,
        del_impl_fn,
        deps_ptr,
        builder.getInt32(num_deps)
    });
    builder.CreateRetVoid();
}

//! Generate LLVM IR for binary module ABI symbols
/** Embeds serialized metadata blob and calls qore_aot_module_init_v3.
*/
static void generateModuleABIV2(llvm::LLVMContext& ctx, llvm::Module& module,
        const std::vector<uint8_t>& metadata, const char* label,
        const QoreParseOptions& parse_options, const QoreAOTModuleInfo& mod_info,
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

    // License enum
    auto* license_gv = new llvm::GlobalVariable(module, i32_type, true,
        llvm::GlobalValue::ExternalLinkage,
        llvm::ConstantInt::get(i32_type, getLicenseEnum(mod_info.license)), "qore_module_license");
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

    // Declare qore_aot_module_init_v3: QoreStringNode* (metadata, metadata_len, label, parse_options_lo, parse_options_hi, mod_name, funcs, num_funcs)
    auto* init_ret_type = ptr_type;
    auto* aot_mod_init_type = llvm::FunctionType::get(init_ret_type, {
        ptr_type, i32_type, ptr_type, i64_type, i64_type, ptr_type, ptr_type, i32_type
    }, false);
    auto aot_mod_init_fn = module.getOrInsertFunction("qore_aot_module_init_v3", aot_mod_init_type);

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
            builder.getInt64(parse_options.getLo()),
            builder.getInt64(parse_options.getHi()),
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

    // Generate API 2.0 module descriptor function
    emitModuleDescFunction(ctx, module, init_impl_fn, ns_init_impl_fn, del_impl_fn, mod_info);
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

    // Determine library directory (auto-detect from loaded libqore.so)
    std::string libqore_dir = getLibqoreDir();

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
                             const QoreParseOptions& parse_options,
                             std::string& error,
                             int opt_level,
                             const char* target_triple,
                             bool include_source) {
    // Step 1: Parse module metadata from source
    QoreAOTModuleInfo mod_info;
    if (!parseModuleMetadata(source_text, source_len, label, mod_info, error)) {
        return false;
    }
    printd(2, "AOT module: name='%s' version='%s' desc='%s'\n",
        mod_info.name.c_str(), mod_info.version.c_str(), mod_info.desc.c_str());

    // Step 2: Parse the module source to get the namespace tree with functions
    // We need PO_IN_MODULE for the parser to handle the module block
    QoreParseOptions mod_po = parse_options | QoreParseOptions(PO_IN_MODULE | PO_NO_TOP_LEVEL_STATEMENTS
        | PO_REQUIRE_PROTOTYPES | PO_REQUIRE_OUR);

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
    size_t total_ir_insts_all = 0;

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
            compiled_funcs, total_funcs, compiled_count, failed_count, total_ir_insts_all);
    }
    if (!found_any) {
        error = "no module namespaces found after parsing (expected at least '" + mod_info.name + "')";
        return false;
    }

    reportAOTCompileStats("module compilation", compiled_count, total_funcs,
        failed_count, compiled_funcs);

    // Step 4: Generate module ABI with serialized metadata
    {
        QoreAOTBinaryWriter writer;
        QoreAOTBinaryHeader hdr{};
        hdr.magic = QORE_AOT_BINARY_MAGIC;
        hdr.version = QORE_AOT_BINARY_VERSION;
        hdr.flags = QORE_AOT_FLAG_IS_MODULE;
        hdr.parse_options = mod_po.getLo();
        hdr.label_offset = writer.strings.add(label);
        // v2 fields: opcode compatibility and Qore version
        hdr.max_opcode_id = QORE_IR_MAX_OPCODE;
        hdr.qore_version_major = QORE_VERSION_MAJOR;
        hdr.qore_version_minor = QORE_VERSION_MINOR;
        hdr.qore_version_patch = QORE_VERSION_PATCH;
        // v3 field: high 64 bits of parse options
        hdr.parse_options_hi = mod_po.getHi();

        // Serialize ALL dependencies (including reexport) so they can be loaded
        // at runtime before deserializing the namespace tree
        std::vector<std::string> reexport_mods;
        std::vector<std::string> all_deps = extractAllDependencies(source_text, source_len, &reexport_mods);
        serializeDependencies(writer, all_deps);
        serializeReexportModules(writer, reexport_mods);

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
            fws.num_stmts = cf.num_stmts;
            fws.slot_ids = cf.slot_ids;
            func_slots.push_back(std::move(fws));
        }
        serializeSlotMaps(writer, func_slots);
        if (include_source) {
            serializeFallbackSources(writer, func_slots, source_text, source_len);
        }

        std::vector<uint8_t> metadata;
        hdr.section_count = 0;
        if (!writer.finalize(hdr, metadata)) {
            error = "failed to finalize module binary metadata";
            return false;
        }

        printf("AOT: module metadata blob: %d bytes\n", (int)metadata.size());

        generateModuleABIV2(ctx, *module, metadata, label, mod_po, mod_info, compiled_funcs);
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
                                     const QoreParseOptions& parse_options,
                                     std::string& error,
                                     int opt_level,
                                     const char* target_triple,
                                     bool include_source) {
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
    QoreParseOptions mod_po = parse_options | QoreParseOptions(PO_IN_MODULE | PO_NO_TOP_LEVEL_STATEMENTS
        | PO_REQUIRE_PROTOTYPES | PO_REQUIRE_OUR);

    ExceptionSink xsink;
    ExceptionSink wsink;
    QoreProgramHelper qpgm(mod_po, xsink);
    if (xsink.isException()) {
        xsink.handleExceptions();
        error = "failed to create QoreProgram for split module parsing";
        return false;
    }

    // Set script path so get_script_dir() works during parse time
    // (e.g., DataProvider/QoreApp.qc uses get_script_dir() to load SVG data)
    qpgm->setScriptPath(qm_path.c_str());

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
                // Insert file boundary marker so the AOT runtime can split the combined
                // source into separate parsePending() calls (one per file segment).
                // This is critical because the parser creates a fresh scanner per
                // parsePending() call, resetting line tracking to 1 — without this,
                // content after the module block gets start_line=0 in BCA nodes.
                if (!combined_source.empty() && combined_source.back() != '\n') {
                    combined_source += '\n';
                }
                combined_source += "# __AOT_FILE_BREAK__\n";
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
        size_t total_ir_insts_all = 0;

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
                if (getenv("QORE_AOT_DEBUG")) {
                    fprintf(stderr, "AOT: skipping namespace '%s' from module '%s' (compiling '%s')\n",
                        ni->first.c_str(), ns_module, mod_info.name.c_str());
                }
                continue;
            }

            found_any = true;
            compileNamespaceFunctions(ns_priv, *qpgm, ctx, *module, di_builder, di_cu,
                compiled_funcs, total_funcs, compiled_count, failed_count, total_ir_insts_all);
        }
        if (!found_any) {
            error = "no module namespaces found after parsing (expected at least '" + mod_name + "')";
            return false;
        }

        reportAOTCompileStats("split module compilation", compiled_count, total_funcs,
            failed_count, compiled_funcs);

        // Step 10: Generate module ABI with serialized metadata
        {
            QoreAOTBinaryWriter writer;
            QoreAOTBinaryHeader hdr{};
            hdr.magic = QORE_AOT_BINARY_MAGIC;
            hdr.version = QORE_AOT_BINARY_VERSION;
            hdr.flags = QORE_AOT_FLAG_IS_MODULE;
            hdr.parse_options = mod_po.getLo();
            hdr.label_offset = writer.strings.add(qm_path.c_str());
            // v2 fields: opcode compatibility and Qore version
            hdr.max_opcode_id = QORE_IR_MAX_OPCODE;
            hdr.qore_version_major = QORE_VERSION_MAJOR;
            hdr.qore_version_minor = QORE_VERSION_MINOR;
            hdr.qore_version_patch = QORE_VERSION_PATCH;
            // v3 field: high 64 bits of parse options
            hdr.parse_options_hi = mod_po.getHi();

            // Serialize ALL dependencies (including reexport) so they can be loaded
            // at runtime before deserializing the namespace tree
            std::vector<std::string> reexport_mods;
            std::vector<std::string> all_deps = extractAllDependencies(combined_source.c_str(),
                static_cast<int>(combined_source.size()), &reexport_mods);
            serializeDependencies(writer, all_deps);
            serializeReexportModules(writer, reexport_mods);

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
                fws.num_stmts = cf.num_stmts;
                fws.slot_ids = cf.slot_ids;
                func_slots.push_back(std::move(fws));
            }
            serializeSlotMaps(writer, func_slots);
            if (include_source) {
                serializeFallbackSources(writer, func_slots, combined_source.c_str(), (int)combined_source.size());
            }

            std::vector<uint8_t> metadata;
            hdr.section_count = 0;
            if (!writer.finalize(hdr, metadata)) {
                error = "failed to finalize split module binary metadata";
                return false;
            }

            printf("AOT: split module metadata blob: %d bytes\n", (int)metadata.size());

            generateModuleABIV2(ctx, *module, metadata, qm_path.c_str(), mod_po, mod_info, compiled_funcs);
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
                    // StoreLValue invoke: LLVM lowering uses the lvalue (extracted
                    // from the assignment expression) as a separate AOT slot
                    if (ii->invoke_opcode == QoreIROpcode::StoreLValue
                            && ii->expr.hasNode()) {
                        auto* assign = dynamic_cast<const QoreAssignmentOperatorNode*>(
                            ii->expr.getInternalNode());
                        if (assign) {
                            QoreValue lv = assign->getLeft();
                            uint64_t lv_bits;
                            memcpy(&lv_bits, &lv, sizeof(lv_bits));
                            slots.getExprSlot(lv_bits);
                        }
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
                case QoreIROpcode::CallMethodDirect: {
                    auto* cmdi = static_cast<QoreIRCallMethodDirectInstruction*>(inst.get());
                    if (cmdi->expr) {
                        uint64_t bits;
                        memcpy(&bits, &cmdi->expr, sizeof(bits));
                        slots.getExprSlot(bits);
                    }
                    break;
                }
                case QoreIROpcode::InvokeMethodDirect: {
                    auto* imdi = static_cast<QoreIRInvokeMethodDirectInstruction*>(inst.get());
                    if (imdi->expr) {
                        uint64_t bits;
                        memcpy(&bits, &imdi->expr, sizeof(bits));
                        slots.getExprSlot(bits);
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
                case QoreIROpcode::RefForeachInit: {
                    auto* ri = static_cast<QoreIRRefForeachInitInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &ri->expr, sizeof(bits));
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
                case QoreIROpcode::Foreach: {
                    auto* fi = static_cast<QoreIRForeachInstruction*>(inst.get());
                    slots.getStmtSlot(reinterpret_cast<const void*>(fi->stmt));
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
                    // StoreLValue invoke: count the lvalue slot too
                    if (ii->invoke_opcode == QoreIROpcode::StoreLValue
                            && ii->expr.hasNode()) {
                        auto* assign = dynamic_cast<const QoreAssignmentOperatorNode*>(
                            ii->expr.getInternalNode());
                        if (assign) {
                            QoreValue lv = assign->getLeft();
                            uint64_t lv_bits;
                            memcpy(&lv_bits, &lv, sizeof(lv_bits));
                            if (seen_exprs.insert(lv_bits).second) {
                                ++expr_count;
                            }
                        }
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
                case QoreIROpcode::RefForeachInit: {
                    auto* ri = static_cast<QoreIRRefForeachInitInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &ri->expr, sizeof(bits));
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
                case QoreIROpcode::CallMethodDirect: {
                    auto* cmdi = static_cast<QoreIRCallMethodDirectInstruction*>(inst.get());
                    if (cmdi->expr) {
                        uint64_t bits;
                        memcpy(&bits, &cmdi->expr, sizeof(bits));
                        if (seen_exprs.insert(bits).second) {
                            ++expr_count;
                        }
                    }
                    break;
                }
                case QoreIROpcode::InvokeMethodDirect: {
                    auto* imdi = static_cast<QoreIRInvokeMethodDirectInstruction*>(inst.get());
                    if (imdi->expr) {
                        uint64_t bits;
                        memcpy(&bits, &imdi->expr, sizeof(bits));
                        if (seen_exprs.insert(bits).second) {
                            ++expr_count;
                        }
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
                case QoreIROpcode::Foreach: {
                    auto* fi = static_cast<QoreIRForeachInstruction*>(inst.get());
                    const void* key = reinterpret_cast<const void*>(fi->stmt);
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
                    // StoreLValue invoke: fill the lvalue slot too
                    if (ii->invoke_opcode == QoreIROpcode::StoreLValue
                            && ii->expr.hasNode()) {
                        auto* assign = dynamic_cast<const QoreAssignmentOperatorNode*>(
                            ii->expr.getInternalNode());
                        if (assign) {
                            QoreValue lv = assign->getLeft();
                            uint64_t lv_bits;
                            memcpy(&lv_bits, &lv, sizeof(lv_bits));
                            if (seen_exprs.insert(lv_bits).second) {
                                lv.ref();
                                ctx->exprs[expr_idx++] = lv_bits;
                            }
                        }
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
                case QoreIROpcode::RefForeachInit: {
                    auto* ri = static_cast<QoreIRRefForeachInitInstruction*>(inst.get());
                    uint64_t bits;
                    memcpy(&bits, &ri->expr, sizeof(bits));
                    if (seen_exprs.insert(bits).second) {
                        ri->expr.ref();
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
                case QoreIROpcode::CallMethodDirect: {
                    auto* cmdi = static_cast<QoreIRCallMethodDirectInstruction*>(inst.get());
                    if (cmdi->expr) {
                        uint64_t bits;
                        memcpy(&bits, &cmdi->expr, sizeof(bits));
                        if (seen_exprs.insert(bits).second) {
                            cmdi->expr.ref();
                            ctx->exprs[expr_idx++] = bits;
                        }
                    }
                    break;
                }
                case QoreIROpcode::InvokeMethodDirect: {
                    auto* imdi = static_cast<QoreIRInvokeMethodDirectInstruction*>(inst.get());
                    if (imdi->expr) {
                        uint64_t bits;
                        memcpy(&bits, &imdi->expr, sizeof(bits));
                        if (seen_exprs.insert(bits).second) {
                            imdi->expr.ref();
                            ctx->exprs[expr_idx++] = bits;
                        }
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
                case QoreIROpcode::Foreach: {
                    auto* fi = static_cast<QoreIRForeachInstruction*>(inst.get());
                    const void* key = reinterpret_cast<const void*>(fi->stmt);
                    if (seen_stmts.insert(key).second) {
                        ctx->stmts[stmt_idx++] = fi->stmt;
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

// ---- Expression Tree Serializer ----

//! Serializes an AST expression tree into a compact binary blob for AOT EXPR_TREE slots
class ExprTreeSerializer {
    std::vector<uint8_t> buf;
    const AOTSlotMap& slots;
    bool debug;
    bool failed = false;

    void writeU8(uint8_t v) {
        buf.push_back(v);
    }

    void writeU16(uint16_t v) {
        buf.push_back(v & 0xFF);
        buf.push_back((v >> 8) & 0xFF);
    }

    void writeU32(uint32_t v) {
        buf.push_back(v & 0xFF);
        buf.push_back((v >> 8) & 0xFF);
        buf.push_back((v >> 16) & 0xFF);
        buf.push_back((v >> 24) & 0xFF);
    }

    void writeI64(int64_t v) {
        uint64_t u;
        memcpy(&u, &v, sizeof(u));
        for (int i = 0; i < 8; ++i) {
            buf.push_back(u & 0xFF);
            u >>= 8;
        }
    }

    void writeF64(double v) {
        uint64_t u;
        memcpy(&u, &v, sizeof(u));
        for (int i = 0; i < 8; ++i) {
            buf.push_back(u & 0xFF);
            u >>= 8;
        }
    }

    void writeStr(const char* s, size_t len) {
        if (len > 0xFFFF) {
            failed = true;
            return;
        }
        writeU16(static_cast<uint16_t>(len));
        if (len > 0 && s) {
            buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(s),
                reinterpret_cast<const uint8_t*>(s) + len);
        }
    }

    void writeStr(const char* s) {
        writeStr(s, s ? strlen(s) : 0);
    }

    void writeStr(const std::string& s) {
        writeStr(s.c_str(), s.size());
    }

    //! Write kind byte and serialize children for a unary operator
    bool serializeUnary(AOTExprNodeKind kind, QoreValue operand) {
        writeU8(static_cast<uint8_t>(kind));
        writeU16(1); // 1 child
        return serializeValue(operand);
    }

    //! Write kind byte and serialize children for a binary operator
    bool serializeBinary(AOTExprNodeKind kind, QoreValue left, QoreValue right) {
        writeU8(static_cast<uint8_t>(kind));
        writeU16(2); // 2 children
        if (!serializeValue(left)) {
            return false;
        }
        return serializeValue(right);
    }

    //! Serialize a QoreParseListNode or QoreListNode as argument children
    bool serializeArgs(const QoreParseListNode* pln, const QoreListNode* ln, uint16_t& count) {
        count = 0;
        if (pln) {
            count = static_cast<uint16_t>(pln->size());
            for (size_t i = 0; i < pln->size(); ++i) {
                if (!serializeValue(pln->get(i))) {
                    return false;
                }
            }
        } else if (ln) {
            count = static_cast<uint16_t>(ln->size());
            for (size_t i = 0; i < ln->size(); ++i) {
                if (!serializeValue(ln->retrieveEntry(i))) {
                    return false;
                }
            }
        }
        return true;
    }

    //! Serialize a FunctionCallBase's arguments
    bool serializeCallArgs(const FunctionCallBase* fcb, uint16_t& count) {
        return serializeArgs(fcb->getParseArgs(), fcb->getArgs(), count);
    }

    //! Serialize a single node
    bool serializeNode(const AbstractQoreNode* node) {
        if (!node) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_NOTHING));
            writeU16(0);
            return true;
        }

        // ---- Leaf constants ----

        // String constant
        if (auto* str = dynamic_cast<const QoreStringNode*>(node)) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_STRING));
            writeStr(str->c_str(), str->size());
            writeU16(0);
            return true;
        }

        // Number constant
        if (auto* num = dynamic_cast<const QoreNumberNode*>(node)) {
            QoreString s;
            num->toString(s);
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_NUMBER));
            writeStr(s.c_str(), s.size());
            writeU16(0);
            return true;
        }

        // Binary constant
        if (auto* bin = dynamic_cast<const BinaryNode*>(node)) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_BINARY));
            uint32_t len = static_cast<uint32_t>(bin->size());
            writeU32(len);
            const uint8_t* data = static_cast<const uint8_t*>(bin->getPtr());
            buf.insert(buf.end(), data, data + len);
            writeU16(0);
            return true;
        }

        // ---- Variable references ----

        // VarRefNode: local, global, closure
        if (auto* vr = dynamic_cast<const VarRefNode*>(node)) {
            qore_var_t vtype = vr->getType();
            if (vtype == VT_LOCAL || vtype == VT_LOCAL_TS || vtype == VT_CLOSURE) {
                const void* lv_ptr = reinterpret_cast<const void*>(vr->ref.id);
                auto it = slots.local_slots.find(lv_ptr);
                if (it != slots.local_slots.end()) {
                    writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_LOCAL_VAR));
                    writeU16(static_cast<uint16_t>(it->second));
                    writeU16(0);
                    return true;
                }
                if (debug) {
                    fprintf(stderr, "EXPR_TREE: local var '%s' not found in slot map\n", vr->getName());
                }
                return false;
            }
            if (vtype == VT_GLOBAL || vtype == VT_THREAD_LOCAL) {
                writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_GLOBAL_VAR));
                writeStr(vr->getName());
                writeU16(0);
                return true;
            }
            if (debug) {
                fprintf(stderr, "EXPR_TREE: unsupported var type %d for '%s'\n", (int)vtype, vr->getName());
            }
            return false;
        }

        // SelfVarrefNode: self.member
        if (auto* sv = dynamic_cast<const SelfVarrefNode*>(node)) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_SELF_REF));
            writeStr(sv->str ? sv->str : "");
            writeU16(0);
            return true;
        }

        // StaticClassVarRefNode: ClassName::var
        if (auto* scv = dynamic_cast<const StaticClassVarRefNode*>(node)) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_STATIC_VAR));
            writeStr(scv->qc.getName());
            writeStr(scv->str);
            writeU16(0);
            return true;
        }

        // RuntimeConstantRefNode
        if (auto* rcr = dynamic_cast<const RuntimeConstantRefNode*>(node)) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_CONST_REF));
            writeStr(rcr->getConstantEntry()->getName());
            writeU16(0);
            return true;
        }

        // ---- Call nodes ----

        // FunctionCallNode: function call with args
        if (auto* fc = dynamic_cast<const FunctionCallNode*>(node)) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_FUNC_CALL));
            writeStr(fc->getName());
            // Count and serialize args
            size_t count_pos = buf.size();
            writeU16(0); // placeholder for num_children
            uint16_t arg_count = 0;
            if (!serializeCallArgs(fc, arg_count)) {
                return false;
            }
            // Patch child count
            buf[count_pos] = arg_count & 0xFF;
            buf[count_pos + 1] = (arg_count >> 8) & 0xFF;
            return true;
        }

        // SelfFunctionCallNode: self method call
        if (auto* sfc = dynamic_cast<const SelfFunctionCallNode*>(node)) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_SELF_CALL));
            const QoreMethod* method = sfc->getMethod();
            if (method) {
                const QoreClass* qc = method->getClass();
                // Use getPath() for namespace-qualified name to avoid ambiguity
                // (e.g., "Test" could match user class or QUnit::Test base class)
                writeStr(qc ? qc->getPath() : "");
            } else {
                writeStr("");
            }
            writeStr(sfc->getName());
            // Args
            size_t count_pos = buf.size();
            writeU16(0);
            uint16_t arg_count = 0;
            if (!serializeCallArgs(sfc, arg_count)) {
                return false;
            }
            buf[count_pos] = arg_count & 0xFF;
            buf[count_pos + 1] = (arg_count >> 8) & 0xFF;
            return true;
        }

        // StaticMethodCallNode: static method call
        if (auto* smc = dynamic_cast<const StaticMethodCallNode*>(node)) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_STATIC_CALL));
            const QoreMethod* method = smc->getMethod();
            if (method) {
                const QoreClass* qc = method->getClass();
                writeStr(qc ? qc->getPath() : "");
            } else {
                writeStr("");
            }
            writeStr(smc->getName());
            // Args
            size_t count_pos = buf.size();
            writeU16(0);
            uint16_t arg_count = 0;
            if (!serializeCallArgs(smc, arg_count)) {
                return false;
            }
            buf[count_pos] = arg_count & 0xFF;
            buf[count_pos + 1] = (arg_count >> 8) & 0xFF;
            return true;
        }

        // QoreDotEvalOperatorNode: obj.method(args)
        if (auto* de = dynamic_cast<const QoreDotEvalOperatorNode*>(node)) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_DOT_EVAL));
            MethodCallNode* mc = de->getMethodCall();
            writeStr(mc ? mc->getName() : "");
            // children[0] = target expression, [1..] = args
            size_t count_pos = buf.size();
            writeU16(0);
            uint16_t child_count = 1;
            if (!serializeValue(de->getExpression())) {
                return false;
            }
            if (mc) {
                uint16_t arg_count = 0;
                if (!serializeCallArgs(mc, arg_count)) {
                    return false;
                }
                child_count += arg_count;
            }
            buf[count_pos] = child_count & 0xFF;
            buf[count_pos + 1] = (child_count >> 8) & 0xFF;
            return true;
        }

        // NewObjectCallNode: new ClassName(args)
        if (auto* no = dynamic_cast<const NewObjectCallNode*>(node)) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_NEW));
            const QoreClass* qc = no->getClass();
            writeStr(qc ? qc->getName() : "");
            size_t count_pos = buf.size();
            writeU16(0);
            uint16_t arg_count = 0;
            if (!serializeCallArgs(no, arg_count)) {
                return false;
            }
            buf[count_pos] = arg_count & 0xFF;
            buf[count_pos + 1] = (arg_count >> 8) & 0xFF;
            return true;
        }

        // ScopedObjectCallNode: new Namespace::ClassName(args)
        if (auto* so = dynamic_cast<const ScopedObjectCallNode*>(node)) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_SCOPED_NEW));
            writeStr(so->oc ? so->oc->getName() : "");
            size_t count_pos = buf.size();
            writeU16(0);
            uint16_t arg_count = 0;
            if (!serializeCallArgs(so, arg_count)) {
                return false;
            }
            buf[count_pos] = arg_count & 0xFF;
            buf[count_pos + 1] = (arg_count >> 8) & 0xFF;
            return true;
        }

        // CallReferenceCallNode: callref(args)
        if (auto* crc = dynamic_cast<const CallReferenceCallNode*>(node)) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_CALLREF_CALL));
            size_t count_pos = buf.size();
            writeU16(0);
            uint16_t child_count = 1;
            // First child: the call reference expression
            if (!serializeValue(crc->getExp())) {
                return false;
            }
            // Remaining children: args
            uint16_t arg_count = 0;
            if (!serializeArgs(crc->getParseArgs(), crc->getArgs(), arg_count)) {
                return false;
            }
            child_count += arg_count;
            buf[count_pos] = child_count & 0xFF;
            buf[count_pos + 1] = (child_count >> 8) & 0xFF;
            return true;
        }

        // ---- Access operators ----

        // QoreHashObjectDereferenceOperatorNode: hash.key or hash{key}
        if (auto* hd = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_HASH_DEREF, hd->getLeft(), hd->getRight());
        }

        // QoreSquareBracketsOperatorNode: list[idx]
        if (auto* sb = dynamic_cast<const QoreSquareBracketsOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_SQUARE_BRKT, sb->getLeft(), sb->getRight());
        }

        // ---- Unary operators ----

        if (auto* op = dynamic_cast<const QoreKeysOperatorNode*>(node)) {
            return serializeUnary(AOTExprNodeKind::EN_KEYS, op->getExp());
        }
        if (auto* op = dynamic_cast<const QoreElementsOperatorNode*>(node)) {
            return serializeUnary(AOTExprNodeKind::EN_ELEMENTS, op->getExp());
        }
        if (auto* op = dynamic_cast<const QoreExistsOperatorNode*>(node)) {
            return serializeUnary(AOTExprNodeKind::EN_EXISTS, op->getExp());
        }
        if (auto* op = dynamic_cast<const QoreDeleteOperatorNode*>(node)) {
            return serializeUnary(AOTExprNodeKind::EN_DELETE, op->getExp());
        }
        if (auto* op = dynamic_cast<const QoreRemoveOperatorNode*>(node)) {
            return serializeUnary(AOTExprNodeKind::EN_REMOVE, op->getExp());
        }
        if (auto* op = dynamic_cast<const QoreBackgroundOperatorNode*>(node)) {
            return serializeUnary(AOTExprNodeKind::EN_BACKGROUND, op->getExp());
        }
        if (auto* op = dynamic_cast<const QoreTrimOperatorNode*>(node)) {
            return serializeUnary(AOTExprNodeKind::EN_TRIM, op->getExp());
        }
        if (auto* op = dynamic_cast<const QoreChompOperatorNode*>(node)) {
            return serializeUnary(AOTExprNodeKind::EN_CHOMP, op->getExp());
        }
        if (auto* op = dynamic_cast<const QorePopOperatorNode*>(node)) {
            return serializeUnary(AOTExprNodeKind::EN_POP, op->getExp());
        }
        if (auto* op = dynamic_cast<const QoreShiftOperatorNode*>(node)) {
            return serializeUnary(AOTExprNodeKind::EN_SHIFT, op->getExp());
        }
        if (auto* op = dynamic_cast<const QoreUnaryMinusOperatorNode*>(node)) {
            return serializeUnary(AOTExprNodeKind::EN_UNARY_MINUS, op->getExp());
        }
        if (auto* op = dynamic_cast<const QoreUnaryPlusOperatorNode*>(node)) {
            return serializeUnary(AOTExprNodeKind::EN_UNARY_PLUS, op->getExp());
        }
        if (auto* op = dynamic_cast<const QoreLogicalNotOperatorNode*>(node)) {
            return serializeUnary(AOTExprNodeKind::EN_LOG_NOT, op->getExp());
        }
        if (auto* op = dynamic_cast<const QoreBinaryNotOperatorNode*>(node)) {
            return serializeUnary(AOTExprNodeKind::EN_BIT_NOT, op->getExp());
        }

        // instanceof: unary + type path
        if (auto* op = dynamic_cast<const QoreInstanceOfOperatorNode*>(node)) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_INSTANCEOF));
            // Get type path from the resolved type info
            const QoreTypeInfo* ti = op->getInstanceTypeInfo();
            if (ti) {
                writeStr(QoreTypeInfo::getPath(ti));
            } else {
                writeStr("");
            }
            writeU16(1);
            return serializeValue(op->getExp());
        }

        // cast<type>(expr)
        if (auto* op = dynamic_cast<const QoreCastOperatorNode*>(node)) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_CAST));
            const QoreTypeInfo* ti = op->getCastTypeInfo();
            writeStr(ti ? QoreTypeInfo::getPath(ti) : "");
            writeU8(op->isOrNothing() ? 1 : 0);
            writeU16(1);
            return serializeValue(op->getExp());
        }

        // typeof: needs special handling — check if it exists
        // typeof is actually just elements in disguise for some nodes, skip for now

        // ---- Pre/post increment/decrement ----
        if (auto* op = dynamic_cast<const QorePreIncrementOperatorNode*>(node)) {
            return serializeUnary(AOTExprNodeKind::EN_PRE_INC, op->getExp());
        }
        if (auto* op = dynamic_cast<const QorePreDecrementOperatorNode*>(node)) {
            return serializeUnary(AOTExprNodeKind::EN_PRE_DEC, op->getExp());
        }
        if (auto* op = dynamic_cast<const QorePostIncrementOperatorNode*>(node)) {
            return serializeUnary(AOTExprNodeKind::EN_POST_INC, op->getExp());
        }
        if (auto* op = dynamic_cast<const QorePostDecrementOperatorNode*>(node)) {
            return serializeUnary(AOTExprNodeKind::EN_POST_DEC, op->getExp());
        }

        // ---- Binary operators ----

        if (auto* op = dynamic_cast<const QorePushOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_PUSH, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreUnshiftOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_UNSHIFT, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreListAssignmentOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_LIST_ASSIGN, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QorePlusOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_PLUS, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreMinusOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_MINUS, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreMultiplicationOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_MULTIPLY, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreDivisionOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_DIVIDE, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreModuloOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_MODULO, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreShiftLeftOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_SHIFT_LEFT, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreShiftRightOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_SHIFT_RIGHT, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreBinaryAndOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_BIT_AND, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreBinaryOrOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_BIT_OR, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreBinaryXorOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_BIT_XOR, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreLogicalComparisonOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_LOG_CMP, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreLogicalAndOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_LOG_AND, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreLogicalOrOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_LOG_OR, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreLogicalEqualsOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_LOG_EQ, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreLogicalNotEqualsOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_LOG_NE, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreLogicalAbsoluteEqualsOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_LOG_AEQ, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreLogicalAbsoluteNotEqualsOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_LOG_ANE, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreLogicalLessThanOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_LOG_LT, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreLogicalGreaterThanOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_LOG_GT, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreLogicalLessThanOrEqualsOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_LOG_LE, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreLogicalGreaterThanOrEqualsOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_LOG_GE, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreNullCoalescingOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_NULL_COAL, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreValueCoalescingOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_VAL_COAL, op->getLeft(), op->getRight());
        }

        // Assignment operators
        if (auto* op = dynamic_cast<const QoreAssignmentOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_ASSIGN, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QorePlusEqualsOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_PLUS_EQ, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreMinusEqualsOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_MINUS_EQ, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreMultiplyEqualsOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_MULTIPLY_EQ, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreDivideEqualsOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_DIVIDE_EQ, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreModuloEqualsOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_MODULO_EQ, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreAndEqualsOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_AND_EQ, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreOrEqualsOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_OR_EQ, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreXorEqualsOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_XOR_EQ, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreShiftLeftEqualsOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_SHL_EQ, op->getLeft(), op->getRight());
        }
        if (auto* op = dynamic_cast<const QoreShiftRightEqualsOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_SHR_EQ, op->getLeft(), op->getRight());
        }

        // Ternary: question mark (? :)
        if (auto* op = dynamic_cast<const QoreQuestionMarkOperatorNode*>(node)) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_QUESTION));
            writeU16(3);
            if (!serializeValue(op->get(0))) {
                return false;
            }
            if (!serializeValue(op->get(1))) {
                return false;
            }
            return serializeValue(op->get(2));
        }

        // Range operator (binary: start..stop)
        if (auto* op = dynamic_cast<const QoreRangeOperatorNode*>(node)) {
            return serializeBinary(AOTExprNodeKind::EN_RANGE, op->getLeft(), op->getRight());
        }

        // ---- Regex operators ----
        // Pattern strings are preserved in pattern_cache via savePattern() for AOT serialization.

        if (auto* op = dynamic_cast<const QoreRegexMatchOperatorNode*>(node)) {
            if (dynamic_cast<const QoreRegexExtractOperatorNode*>(node)) {
                writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_REGEX_EXTRACT));
            } else if (dynamic_cast<const QoreRegexNMatchOperatorNode*>(node)) {
                writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_REGEX_NMATCH));
            } else {
                writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_REGEX_MATCH));
            }
            QoreRegex* re = op->getRegex();
            const char* pattern = re->getPatternCStr();
            if (!pattern) {
                if (debug) {
                    fprintf(stderr, "EXPR_TREE: regex pattern string not available\n");
                }
                return false;
            }
            writeStr(pattern);
            writeI64(re->getOptions());
            writeU16(1);
            return serializeValue(op->getExp());
        }

        if (auto* op = dynamic_cast<const QoreRegexSubstOperatorNode*>(node)) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_REGEX_SUBST));
            QoreRegexSubst* rs = op->getRegexSubst();
            const char* pattern = rs->getPatternCStr();
            if (!pattern) {
                if (debug) {
                    fprintf(stderr, "EXPR_TREE: regex subst pattern string not available\n");
                }
                return false;
            }
            writeStr(pattern);
            const QoreString* newstr = rs->getNewStr();
            writeStr(newstr ? newstr->c_str() : "", newstr ? newstr->size() : 0);
            writeI64(rs->getOptions());
            writeU8(rs->isGlobal() ? 1 : 0);
            writeU16(1);
            return serializeValue(op->getExp());
        }

        if (auto* op = dynamic_cast<const QoreTransliterationOperatorNode*>(node)) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_TRANSLIT));
            QoreTransliteration* tr = op->getTransliteration();
            if (!tr) {
                return false;
            }
            const QoreString& src = tr->getSource();
            const QoreString& tgt = tr->getTarget();
            writeStr(src.c_str(), src.size());
            writeStr(tgt.c_str(), tgt.size());
            writeU16(1);
            return serializeValue(op->getExp());
        }

        // ---- Special nodes ----

        // Method reference: \obj.method()
        if (auto* mr = dynamic_cast<const ParseObjectMethodReferenceNode*>(node)) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_OBJ_METH_REF));
            writeStr(mr->getMethodName().c_str(), mr->getMethodName().size());
            writeU16(1);
            return serializeValue(mr->getExp());
        }

        // Self method reference: \method()
        if (auto* smr = dynamic_cast<const ParseSelfMethodReferenceNode*>(node)) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_SELF_METH_REF));
            writeStr(smr->getMethodName().c_str(), smr->getMethodName().size());
            writeU16(0);
            return true;
        }

        // Extract operator: extract list, offset[, length[, replacement]]
        if (auto* op = dynamic_cast<const QoreExtractOperatorNode*>(node)) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_EXTRACT));
            uint16_t child_count = 2; // always have lvalue + offset
            if (!op->getLength().isNothing()) {
                child_count++;
            }
            if (!op->getNewValue().isNothing()) {
                child_count++;
            }
            writeU16(child_count);
            if (!serializeValue(op->getLValue())) {
                return false;
            }
            if (!serializeValue(op->getOffset())) {
                return false;
            }
            if (!op->getLength().isNothing()) {
                if (!serializeValue(op->getLength())) {
                    return false;
                }
            }
            if (!op->getNewValue().isNothing()) {
                if (!serializeValue(op->getNewValue())) {
                    return false;
                }
            }
            return true;
        }

        // Splice operator: splice list, offset[, length[, replacement]]
        if (auto* op = dynamic_cast<const QoreSpliceOperatorNode*>(node)) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_SPLICE));
            uint16_t child_count = 2;
            if (!op->getLength().isNothing()) {
                child_count++;
            }
            if (!op->getNewValue().isNothing()) {
                child_count++;
            }
            writeU16(child_count);
            if (!serializeValue(op->getLValue())) {
                return false;
            }
            if (!serializeValue(op->getOffset())) {
                return false;
            }
            if (!op->getLength().isNothing()) {
                if (!serializeValue(op->getLength())) {
                    return false;
                }
            }
            if (!op->getNewValue().isNothing()) {
                if (!serializeValue(op->getNewValue())) {
                    return false;
                }
            }
            return true;
        }

        // QoreParseListNode (used internally for arg lists)
        if (auto* pln = dynamic_cast<const QoreParseListNode*>(node)) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_PARSE_LIST));
            uint16_t count = static_cast<uint16_t>(pln->size());
            writeU16(count);
            for (size_t i = 0; i < pln->size(); ++i) {
                if (!serializeValue(pln->get(i))) {
                    return false;
                }
            }
            return true;
        }

        // QoreListNode: runtime list literal (already parsed)
        if (auto* ln = dynamic_cast<const QoreListNode*>(node)) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_LIST));
            uint16_t count = static_cast<uint16_t>(ln->size());
            writeU16(count);
            for (size_t i = 0; i < ln->size(); ++i) {
                if (!serializeValue(ln->retrieveEntry(i))) {
                    return false;
                }
            }
            return true;
        }

        // QoreHashNode: runtime hash literal (already parsed)
        if (auto* hn = dynamic_cast<const QoreHashNode*>(node)) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_HASH));
            uint16_t count = static_cast<uint16_t>(hn->size());
            writeU16(count);
            ConstHashIterator hi(*hn);
            while (hi.next()) {
                writeStr(hi.getKey());
                if (!serializeValue(hi.get())) {
                    return false;
                }
            }
            return true;
        }

        // QoreImplicitArgumentNode: $1, $2, $argv
        if (auto* ia = dynamic_cast<const QoreImplicitArgumentNode*>(node)) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_IMPLICIT_ARG));
            int16_t offset = static_cast<int16_t>(ia->getOffset());
            writeU16(static_cast<uint16_t>(offset));
            writeU16(0); // 0 children
            return true;
        }

        // QoreImplicitElementNode: $#
        if (dynamic_cast<const QoreImplicitElementNode*>(node)) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_IMPLICIT_ELEM));
            writeU16(0); // 0 children
            return true;
        }

        // ParseReferenceNode: \variable (reference to lvalue)
        if (auto* pr = dynamic_cast<const ParseReferenceNode*>(node)) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_REF_TO_LVALUE));
            writeU16(1); // 1 child
            return serializeValue(pr->getLVExp());
        }

        // QoreSquareBracketsRangeOperatorNode: x[m..n]
        if (auto* sbr = dynamic_cast<const QoreSquareBracketsRangeOperatorNode*>(node)) {
            writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_SQ_BRKT_RANGE));
            writeU16(3); // 3 children: target, start, end
            if (!serializeValue(sbr->get(0))) {
                return false;
            }
            if (!serializeValue(sbr->get(1))) {
                return false;
            }
            return serializeValue(sbr->get(2));
        }

        // Closure — cannot be serialized
        if (dynamic_cast<const QoreClosureParseNode*>(node)) {
            if (debug) {
                fprintf(stderr, "EXPR_TREE: closure cannot be serialized\n");
            }
            return false;
        }

        // Unsupported node type
        if (debug) {
            fprintf(stderr, "EXPR_TREE: unsupported node type '%s' (type %d)\n",
                node->getTypeName(), node->getType());
        }
        return false;
    }

public:
    ExprTreeSerializer(const AOTSlotMap& s) : slots(s), debug(getenv("QORE_AOT_DEBUG") != nullptr) {
    }

    //! Serialize a QoreValue (scalar or node) into a binary blob
    bool serializeValue(QoreValue v) {
        if (!v.hasNode()) {
            if (v.isNothing()) {
                writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_NOTHING));
                writeU16(0);
                return true;
            }
            if (v.isNull()) {
                writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_NULL));
                writeU16(0);
                return true;
            }
            qore_type_t t = v.getType();
            if (t == NT_INT) {
                writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_INT));
                writeI64(v.getAsBigInt());
                writeU16(0);
                return true;
            }
            if (t == NT_FLOAT) {
                writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_FLOAT));
                writeF64(v.getAsFloat());
                writeU16(0);
                return true;
            }
            if (t == NT_BOOLEAN) {
                writeU8(static_cast<uint8_t>(AOTExprNodeKind::EN_BOOL));
                writeU8(v.getAsBool() ? 1 : 0);
                writeU16(0);
                return true;
            }
            return false;
        }
        return serializeNode(v.getInternalNode());
    }

    //! Try to serialize a QoreValue expression tree, returning the blob data
    /** @return true if successful, false if the tree contains unsupported nodes
    */
    bool serialize(QoreValue v) {
        buf.clear();
        failed = false;
        if (!serializeValue(v)) {
            return false;
        }
        return !failed;
    }

    //! Get the serialized buffer data
    std::string getBuffer() const {
        return std::string(reinterpret_cast<const char*>(buf.data()), buf.size());
    }
};

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
                id.ref1 = qc->getPath();
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
                id.ref1 = qc->getPath();
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

    // SelfVarrefNode: self variable reference (e.g., self.member)
    if (auto* svn = dynamic_cast<const SelfVarrefNode*>(node)) {
        id.kind = AOTExprKind::SELF_VARREF;
        if (svn->str) {
            id.ref1 = svn->str;
        }
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

    // QoreNumberNode: number literal constant
    if (auto* num = dynamic_cast<const QoreNumberNode*>(node)) {
        id.kind = AOTExprKind::CONST_NUMBER;
        QoreString str;
        num->toString(str);
        id.ref1 = str.c_str();
        return id;
    }

    // BinaryNode: binary literal constant
    if (auto* bin = dynamic_cast<const BinaryNode*>(node)) {
        id.kind = AOTExprKind::CONST_BINARY;
        // Hex-encode the binary data
        std::string hex;
        const unsigned char* data = static_cast<const unsigned char*>(bin->getPtr());
        size_t len = bin->size();
        hex.reserve(len * 2);
        for (size_t i = 0; i < len; ++i) {
            char buf[3];
            snprintf(buf, sizeof(buf), "%02x", data[i]);
            hex.append(buf);
        }
        id.ref1 = std::move(hex);
        return id;
    }

    // ScopedObjectCallNode: scoped new object (new ClassName::SubClass())
    // Note: must check BEFORE NewObjectCallNode since ScopedObjectCallNode
    // does not inherit from NewObjectCallNode
    if (auto* sc = dynamic_cast<const ScopedObjectCallNode*>(node)) {
        id.kind = AOTExprKind::SCOPED_NEW_OBJECT;
        if (sc->oc) {
            id.ref1 = sc->oc->getName();
        }
        return id;
    }

    // StaticClassVarRefNode: static class variable reference
    if (auto* sv = dynamic_cast<const StaticClassVarRefNode*>(node)) {
        id.kind = AOTExprKind::STATIC_VARREF;
        id.ref1 = sv->qc.getName();
        id.ref2 = sv->str;
        return id;
    }

    // Try recursive expression tree serialization before falling back to GENERIC_EVAL
    {
        ExprTreeSerializer serializer(slots);
        if (serializer.serialize(v)) {
            id.kind = AOTExprKind::EXPR_TREE;
            id.ref1 = serializer.getBuffer();
            if (getenv("QORE_AOT_DEBUG")) {
                fprintf(stderr, "AOT: serialized EXPR_TREE for '%s' (node type %d, %zu bytes)\n",
                    node->getTypeName(), node->getType(), id.ref1.size());
            }
            return id;
        }
    }

    // Unsupported expression type — function needs source fallback
    id.kind = AOTExprKind::GENERIC_EVAL;
    printd(3, "AOT: unsupported expression type '%s' for slot serialization\n", node->getTypeName());
    if (getenv("QORE_AOT_DEBUG")) {
        fprintf(stderr, "AOT: unsupported expression type '%s' (node type %d) for slot serialization\n",
            node->getTypeName(), node->getType());
    }
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

    // stmt_slots (on_block_exit handlers) are resolved from the function's AST at
    // runtime in buildContextFromSlotMap(), so they no longer require source fallback.
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
    printf("  %-30s %s\n", "x86_64-pc-linux-gnu", "Linux x86-64 (GNU libc)");
    printf("  %-30s %s\n", "aarch64-unknown-linux-gnu", "Linux ARM64 (GNU libc)");
    printf("  %-30s %s\n", "x86_64-unknown-linux-musl", "Linux x86-64 (musl libc / Alpine)");
    printf("  %-30s %s\n", "aarch64-apple-darwin", "macOS ARM64");
    printf("  %-30s %s\n", "x86_64-apple-darwin", "macOS x86-64");
}
