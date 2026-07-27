/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreAOT.h

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

#ifndef _QORE_QOREAOT_H
#define _QORE_QOREAOT_H

#include <cassert>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <qore/QoreParseOptions.h>
#include "qore/intern/QoreAOTBinary.h"
#include "qore/intern/qore_aot_deps.h"

class AbstractStatement;

class AbstractQoreFunctionVariant;
class CaseNodeRegex;
class ExceptionSink;
class FunctionEntry;
class LocalVar;
class QoreClass;
class QoreFunction;
class QoreIRFunction;
class QoreIRLValuePathInstruction;
class QoreMethod;
class QoreMemberInfo;
class QoreClass;
class QoreNamespace;
class QoreParseListNode;
class QoreProgram;
class QoreRecursiveThreadLock;
class QoreStringNode;
class QoreTypeInfo;
class QoreValue;
struct QoreModuleInitContext;
struct QoreAOTDebugMetadata;
struct QoreAOTLazyClosureIR;
class StatementBlock;
class TypedHashDecl;
class UserVariantBase;
class Var;
class qore_class_private;

/** Materialize a lazily retained source-stripped closure IR body.
    @param lazy_ir immutable serialized IR metadata
    @param uvb closure variant receiving the reconstructed IR
    @param xsink exception sink
    @param error output for structural deserialization errors
    @return reconstructed IR, or nullptr on error
*/
std::unique_ptr<QoreIRFunction> qore_aot_materialize_lazy_closure_ir(
    const QoreAOTLazyClosureIR& lazy_ir, UserVariantBase* uvb,
    ExceptionSink* xsink, std::string& error);

//! Resolve an AOT-serialized function name during runtime metadata reconstruction.
const FunctionEntry* qore_aot_resolve_function_entry_for_slot(QoreProgram* pgm, const char* name);

//! Resolve a namespace function for a serialized class-qualified call after method lookup fails.
const FunctionEntry* qore_aot_resolve_function_entry_for_static_call_fallback(
    QoreProgram* pgm, const QoreClass* qc, const char* class_ref, const char* method_name);

//! Create a dynamic call_function() fallback for an unresolved serialized function call.
QoreValue qore_aot_make_deferred_function_call(QoreProgram* pgm, const char* name, QoreParseListNode* args);

//! Key for source-stripped AOT hashdecl path resolution.
/** A serialized hashdecl path can contain generic receiver type variables; the same
    path must therefore be cached separately for each active receiver type context. */
struct QoreAOTHashDeclPathCacheKey {
    std::string path;
    const QoreTypeInfo* receiver_type_info = nullptr;

    bool operator==(const QoreAOTHashDeclPathCacheKey& other) const {
        return receiver_type_info == other.receiver_type_info && path == other.path;
    }
};

struct QoreAOTHashDeclPathCacheKeyHash {
    size_t operator()(const QoreAOTHashDeclPathCacheKey& key) const {
        size_t path_hash = std::hash<std::string>()(key.path);
        size_t receiver_hash = std::hash<const QoreTypeInfo*>()(key.receiver_type_info);
        return path_hash ^ (receiver_hash + 0x9e3779b97f4a7c15ULL + (path_hash << 6) + (path_hash >> 2));
    }
};

//! Immutable object-member metadata resolved for one AOT call site.
struct QoreAOTObjectMemberDescriptor {
    const QoreMemberInfo* info = nullptr;
    const qore_class_private* member_class_ctx = nullptr;
    size_t key_hash = 0;
};

//! Pre-resolved function call target for AOT fast calls (avoids per-call dynamic_cast)
struct QoreAOTCallTarget {
    const QoreFunction* func = nullptr;
    const AbstractQoreFunctionVariant* variant = nullptr;
    QoreProgram* pgm = nullptr;
    const UserVariantBase* uvb = nullptr;  //!< cached user variant base for inlined fast path
    const QoreMethod* method = nullptr;    //!< for static/self method calls (pre-resolved)
    const QoreClass* qc = nullptr;         //!< for dot-eval method calls (pre-resolved)
    const QoreTypeInfo* object_type_info = nullptr; //!< instantiated object type for constructor calls
    const QoreTypeInfo* receiver_type_info = nullptr; //!< parameterized receiver type for generic static calls
    const QoreTypeParamInstantiation* explicit_type_param_instantiation = nullptr;
    const char* method_name = nullptr;     //!< for dot-eval fallback (name-based dispatch)
    const char* class_path = nullptr;      //!< for lazy class resolution when registration order delays availability
    const char* variant_sig = nullptr;     //!< constructor/method signature text retained for diagnostics/lazy resolution
    const qore_class_private* class_ctx = nullptr; //!< class context for self/base method calls
    //! Lazily resolved immutable metadata for object member summary lowering.
    std::atomic<const QoreAOTObjectMemberDescriptor*> object_member_descriptor{
        nullptr};
    bool is_pseudo = false;                //!< for dot-eval pseudo-method calls
    bool is_static_method = false;         //!< method is a static method target
    bool is_self_method = false;           //!< method target came from SelfFunctionCallNode
    bool self_ns_single = false;           //!< unqualified self call; false for explicit base/namespace call
    bool self_is_copy = false;             //!< implicit self copy() call
    bool self_is_abstract = false;         //!< abstract self call requiring virtual dispatch

    ~QoreAOTCallTarget() {
        delete object_member_descriptor.load(std::memory_order_relaxed);
    }
};

//! AOT context: runtime-resolved pointer tables for AOT-compiled functions.
/** At compile time, each process-specific pointer (LocalVar*, Var*, QoreValue expr)
    is assigned a numeric slot index. At runtime, the fresh AST is re-lowered in the
    same deterministic order to collect fresh pointers into these arrays. The AOT-compiled
    native code uses index-based lookups instead of embedded pointers.
*/
struct QoreAOTContext {
    QoreAOTContext();

    QoreProgram* pgm = nullptr;     //!< Program owning the deserialized AOT symbols for type/name resolution

    LocalVar** locals = nullptr;    //!< LoadLocal/StoreLocal/LoadClosure/StoreClosure/instantiate
    int num_locals = 0;
    Var** globals = nullptr;        //!< LoadGlobal/StoreGlobal/LoadThreadLocal/StoreThreadLocal
    int num_globals = 0;
    std::vector<std::string> global_names; //!< Runtime fallback names for lazily resolving globals
    std::vector<uint8_t> global_required_imports; //!< Nonzero when missing globals are required imports
    std::mutex global_resolution_mutex; //!< Guards lazy global slot resolution
    uint64_t* exprs = nullptr;      //!< NaN-boxed QoreValue for Invoke/Call/CallMethod/CallStatic/LValue
    int num_exprs = 0;
    const AbstractStatement** stmts = nullptr;  //!< OnBlockExit/Foreach statement pointers
    int num_stmts = 0;
    CaseNodeRegex** regex_cases = nullptr;       //!< Regex case objects for SwitchRegexMatch
    int num_regex_cases = 0;
    QoreIRLValuePathInstruction** lv_path_insts = nullptr;  //!< LValuePath instruction pointers
    int num_lv_path_insts = 0;

    //! All body locals from the fresh IR (needed by evalTiered for instantiation)
    std::vector<LocalVar*> all_body_locals;

    //! True if all body locals are IR-only (enables skipping instantiation in fast call path)
    bool all_body_locals_ir_only = false;

    //! Runtime implicit contexts required by the compiled function.
    bool uses_argv = true;
    bool uses_self = true;

    //! True if this context owns the regex_cases (created by buildContextFromSlotMap).
    //! False when regex_cases are borrowed pointers from the IR function (buildAOTContext).
    bool owns_regex_cases = true;

    //! Pre-resolved CallDirect targets indexed by expr slot.
    //! Populated during buildAOTContext() to avoid per-call dynamic_cast in qore_rt_call_direct_aot()
    //! Size == num_exprs; entries with func==nullptr are not CallDirect slots.
    QoreAOTCallTarget* call_targets = nullptr;

    //! Lazily resolved hashdecl paths used by source-stripped AOT typed-container helpers.
    //! The generation distinguishes TLS front-cache entries after context address reuse.
    uint64_t hashdecl_cache_generation = 0;
    std::mutex hashdecl_path_cache_mutex;
    std::unordered_map<QoreAOTHashDeclPathCacheKey, const TypedHashDecl*, QoreAOTHashDeclPathCacheKeyHash>
        hashdecl_path_cache;

    //! Lazily resolved full type paths used by source-stripped AOT typed-container helpers.
    std::mutex type_path_cache_mutex;
    std::unordered_map<QoreAOTHashDeclPathCacheKey, const QoreTypeInfo*, QoreAOTHashDeclPathCacheKeyHash>
        type_path_cache;

    //! Source locations for per-line runtime_loc tracking in AOT mode.
    //! Indexed by location slot assigned during LLVM codegen. Populated from serialized
    //! location table at load time. Used by the AOT runtime-location helpers.
    const QoreProgramLocation** locs = nullptr;
    int num_locs = 0;

    //! Deserialized handler IR functions for on_exit/on_success/on_error handlers.
    //! Indexed by statement slot index. Non-null entries can be executed by the IR interpreter.
    //! Used in strip-source mode where AST-based stmts[] are not available.
    std::vector<std::unique_ptr<QoreIRFunction>> handler_irs;

    //! Shared serialized AOT metadata used to lazily materialize debug IR.
    std::shared_ptr<const QoreAOTDebugMetadata> debug_metadata;
    //! Offset of this function's debug IR payload from its containing metadata section.
    uint32_t debug_ir_offset = 0;
    //! Serialized debug IR payload size. Zero means no lazy debug IR is available.
    uint32_t debug_ir_size = 0;
    //! True when debug_ir_offset addresses the separately compressed DEBUG_IR section.
    bool debug_ir_separate_section = false;

    //! Owned IR function kept alive for LValuePath instruction pointers.
    //! LValuePath instructions reference path data in the IR function; the function must
    //! outlive the context so that lv_path_insts[] pointers remain valid.
    std::unique_ptr<QoreIRFunction> lv_path_ir_func;

    //! Owned LValuePath instructions reconstructed from binary deserialization.
    //! The raw pointers in lv_path_insts[] point into these objects.
    std::vector<std::unique_ptr<QoreIRLValuePathInstruction>> owned_lv_path_insts;

    //! Owned StaticClassVarRefNode objects created during LValuePath deserialization.
    //! These resolve StaticVar path steps at runtime; deref'd in destructor.
    std::vector<AbstractQoreNode*> owned_static_var_refs;

    //! Owned QoreRegexSubstOperatorNode objects created for LValuePath RegexSubst
    //! binary_mut ops.  LValuePath instructions hold them via pattern_expr as
    //! non-owning QoreValue; the context owns them here and derefs them in the
    //! destructor.
    std::vector<AbstractQoreNode*> owned_regex_subst_nodes;

    //! Owned QoreTransliterationOperatorNode objects for LValuePath Transliterate
    //! binary_mut ops.  Same lifetime rules as owned_regex_subst_nodes.
    std::vector<AbstractQoreNode*> owned_transliteration_nodes;

    //! Owned string copies for call target method names from slot map deserialization.
    //! Uses std::list so that element pointers remain stable across insertions.
    std::list<std::string> owned_call_target_strings;

    //! Metadata-only statements registered for source-stripped ProgramControl lookups.
    std::vector<AbstractStatement*> owned_debug_statements;

    //! Destructor: deref all held expression values, then free arrays.
    //! Implemented in QoreAOT.cpp because it needs QoreValue.
    ~QoreAOTContext();

    //! Returns true when source-stripped debug IR can be materialized lazily.
    bool hasLazyDebugIR() const {
        return debug_metadata && debug_ir_size;
    }

    //! Deserialize the stored debug IR payload for debugger execution.
    std::unique_ptr<QoreIRFunction> materializeDebugIR(const char* name, std::string& error);

    //! Allocate arrays based on slot counts
    void allocate() {
        if (num_locals > 0) {
            locals = static_cast<LocalVar**>(calloc(num_locals, sizeof(LocalVar*)));
        }
        if (num_globals > 0) {
            globals = static_cast<Var**>(calloc(num_globals, sizeof(Var*)));
        }
        if (num_exprs > 0) {
            exprs = static_cast<uint64_t*>(calloc(num_exprs, sizeof(uint64_t)));
            call_targets = new QoreAOTCallTarget[num_exprs]();
        }
        if (num_stmts > 0) {
            stmts = static_cast<const AbstractStatement**>(calloc(num_stmts, sizeof(const AbstractStatement*)));
        }
        if (num_regex_cases > 0) {
            regex_cases = static_cast<CaseNodeRegex**>(calloc(num_regex_cases, sizeof(CaseNodeRegex*)));
        }
        if (num_lv_path_insts > 0) {
            lv_path_insts = static_cast<QoreIRLValuePathInstruction**>(
                calloc(num_lv_path_insts, sizeof(QoreIRLValuePathInstruction*)));
        }
    }
};

//! Compile-time slot assignment map for AOT pointer indirection.
/** Assigns monotonically increasing slot indices to each unique pointer encountered
    during IR lowering. The same source produces the same AST produces the same IR
    produces the same walk order, guaranteeing that slot indices match between compile
    time and runtime.
*/
struct AOTSlotMap {
    struct DotEvalDirectTarget {
        const QoreClass* qc = nullptr;
        const QoreMethod* method = nullptr;
        const AbstractQoreFunctionVariant* variant = nullptr;
        const char* fallback_method_name = nullptr;
        bool pseudo = false;
    };

    std::unordered_map<const void*, int32_t> local_slots;   //!< LocalVar* -> slot index
    std::unordered_map<const void*, int32_t> global_slots;  //!< Var* -> slot index
    std::unordered_map<uint64_t, int32_t> expr_slots;       //!< NaN-boxed expr bits -> slot index
    std::unordered_map<uint64_t, std::string> expr_slot_sources; //!< Expr bits -> originating IR opcode diagnostic
    std::unordered_map<const void*, int32_t> stmt_slots;    //!< StatementBlock* -> slot index (OnBlockExit)
    std::unordered_map<const void*, int32_t> regex_case_slots;  //!< CaseNodeRegex* -> slot index
    std::unordered_map<const void*, int32_t> lv_path_slots;  //!< QoreIRLValuePathInstruction* -> slot
    std::unordered_set<uint64_t> dot_eval_direct_bits;  //!< expr bits from DotEvalMethodDirect (classify as DOT_EVAL_TARGET)
    std::unordered_map<uint64_t, DotEvalDirectTarget> dot_eval_direct_targets; //!< expr bits -> IR-selected target
    std::unordered_set<uint64_t> static_call_pre_evaluated_bits;  //!< CallStatic* expr bits whose args are IR operands
    std::unordered_map<uint64_t, QoreAOTCallRelocationTargetKind> call_relocation_kinds; //!< expr bits -> direct call relocation target kind

    //! Check if a slot already exists for a LocalVar*
    bool hasLocalSlot(const void* local) const {
        return local_slots.find(local) != local_slots.end();
    }

    //! Get or assign a slot for a LocalVar*
    int32_t getLocalSlot(const void* local) {
        auto it = local_slots.find(local);
        if (it != local_slots.end()) {
            return it->second;
        }
        int32_t slot = 0;
        for (const auto& i : local_slots) {
            slot = std::max(slot, i.second + 1);
        }
        local_slots[local] = slot;
        return slot;
    }

    //! Get or assign a slot for a Var* (global or thread-local)
    int32_t getGlobalSlot(const void* var) {
        auto it = global_slots.find(var);
        if (it != global_slots.end()) {
            return it->second;
        }
        int32_t slot = static_cast<int32_t>(global_slots.size());
        global_slots[var] = slot;
        return slot;
    }

    //! Get or assign a slot for an expression (NaN-boxed QoreValue bits)
    int32_t getExprSlot(uint64_t expr_bits, const std::string& source = std::string()) {
        if (!source.empty() && expr_slot_sources.find(expr_bits) == expr_slot_sources.end()) {
            expr_slot_sources[expr_bits] = source;
        }
        auto it = expr_slots.find(expr_bits);
        if (it != expr_slots.end()) {
            return it->second;
        }
        int32_t slot = static_cast<int32_t>(expr_slots.size());
        expr_slots[expr_bits] = slot;
        return slot;
    }

    //! Get or assign a slot for a StatementBlock* (OnBlockExit)
    int32_t getStmtSlot(const void* stmt) {
        auto it = stmt_slots.find(stmt);
        if (it != stmt_slots.end()) {
            return it->second;
        }
        int32_t slot = static_cast<int32_t>(stmt_slots.size());
        stmt_slots[stmt] = slot;
        return slot;
    }

    //! Get or assign a slot for a QoreIRLValuePathInstruction*
    int32_t getLVPathSlot(const void* inst) {
        auto it = lv_path_slots.find(inst);
        if (it != lv_path_slots.end()) {
            return it->second;
        }
        int32_t slot = static_cast<int32_t>(lv_path_slots.size());
        lv_path_slots[inst] = slot;
        return slot;
    }

    //! Get or assign a slot for a CaseNodeRegex* (SwitchRegexMatch)
    int32_t getRegexCaseSlot(const void* regex_case) {
        auto it = regex_case_slots.find(regex_case);
        if (it != regex_case_slots.end()) {
            return it->second;
        }
        int32_t slot = static_cast<int32_t>(regex_case_slots.size());
        regex_case_slots[regex_case] = slot;
        return slot;
    }
};

//! AOT function pointer type: takes QoreAOTContext* and ExceptionSink*
using AotFunctionPtr = uint64_t (*)(QoreAOTContext*, ExceptionSink*);

//! Descriptor for a pre-compiled AOT function
struct QoreAOTFunc {
    const char* name;                           //!< unique function identifier (from IR lowering)
    AotFunctionPtr fn_ptr;                      //!< pre-compiled native code pointer (ctx, xsink)
    int num_locals;                             //!< number of local variable slots
    int num_globals;                            //!< number of global variable slots
    int num_exprs;                              //!< number of expression slots
    int num_stmts;                              //!< number of statement slots (OnBlockExit)
    int num_regex_cases = 0;                    //!< number of regex case slots (SwitchRegexMatch)
};

//! QoreAOTFunc::num_regex_cases keeps its ABI-sized i32 while carrying context-elision metadata.
constexpr uint32_t QORE_AOT_FUNC_NO_ARGV_CONTEXT = 1U << 30;
constexpr uint32_t QORE_AOT_FUNC_NO_SELF_CONTEXT = 1U << 31;
constexpr uint32_t QORE_AOT_FUNC_REGEX_COUNT_MASK = (1U << 30) - 1;

inline int qore_aot_func_num_regex_cases(const QoreAOTFunc& func) {
    return static_cast<int>(static_cast<uint32_t>(func.num_regex_cases)
        & QORE_AOT_FUNC_REGEX_COUNT_MASK);
}

inline bool qore_aot_func_uses_argv(const QoreAOTFunc& func) {
    return !(static_cast<uint32_t>(func.num_regex_cases) & QORE_AOT_FUNC_NO_ARGV_CONTEXT);
}

inline bool qore_aot_func_uses_self(const QoreAOTFunc& func) {
    return !(static_cast<uint32_t>(func.num_regex_cases) & QORE_AOT_FUNC_NO_SELF_CONTEXT);
}

//! C ABI entry point called by AOT-compiled binaries from their generated main()
/** Initializes the Qore runtime, re-parses the embedded source to build the AST/type system,
    registers pre-compiled function pointers, and runs the program.
    Functions without pre-compiled pointers fall back to runtime JIT compilation.

    @param argc command-line argument count
    @param argv command-line argument vector
    @param source embedded Qore source text
    @param source_len length of source text in bytes
    @param label label for the source (e.g. script filename)
    @param parse_options parse options used during original compilation
    @param functions array of pre-compiled function descriptors
    @param num_functions number of entries in functions array
    @return exit code (0 = success)
*/
extern "C" int qore_aot_run(
    int argc, char** argv,
    const char* source, int source_len,
    const char* label,
    int64_t parse_options,
    const QoreAOTFunc* functions, int num_functions
);

//! C ABI entry point called by source-stripped AOT binaries from their generated main()
/** Deserializes binary metadata to reconstruct the namespace tree (instead of re-parsing
    source), builds AOT contexts from slot maps, and runs the program.
    Functions with unsupported expression types are rejected during AOT compilation.

    @param argc command-line argument count
    @param argv command-line argument vector
    @param metadata serialized binary metadata blob
    @param metadata_len length of metadata blob in bytes
    @param label label for the source (e.g. script filename)
    @param parse_options parse options used during original compilation
    @param functions array of pre-compiled function descriptors
    @param num_functions number of entries in functions array
    @return exit code (0 = success)
*/
extern "C" int qore_aot_run_v2(
    int argc, char** argv,
    const uint8_t* metadata, int metadata_len,
    const char* label,
    int64_t parse_options,
    const QoreAOTFunc* functions, int num_functions
);

//! Module metadata extracted from .qm source
struct QoreAOTModuleInfo {
    std::string name;           //!< module feature name
    std::string version;        //!< version string
    std::string desc;           //!< description
    std::string author;         //!< author
    std::string url;            //!< URL (optional)
    std::string license;        //!< license string (optional, default "MIT")
    std::vector<std::string> dependencies;  //!< list of required modules (from %requires directives)
    //! list of optional child modules (from %try-child-module directives)
    std::vector<std::string> child_modules;
};

//! C ABI entry point called by AOT-compiled modules from their generated qore_module_init()
/** Parses the embedded source to build the module namespace tree, registers pre-compiled
    function pointers, and returns nullptr on success or an error string on failure.
*/
extern "C" QoreStringNode* qore_aot_module_init(
    const char* source, int source_len,
    const char* label,
    int64_t parse_options,
    const char* mod_name,
    const QoreAOTFunc* functions, int num_functions
);

//! C ABI entry point called by source-stripped AOT modules from their generated qore_module_init()
/** Deserializes binary metadata instead of re-parsing source.
*/
extern "C" QoreStringNode* qore_aot_module_init_v2(
    const uint8_t* metadata, int metadata_len,
    const char* label,
    int64_t parse_options,
    const char* mod_name,
    const QoreAOTFunc* functions, int num_functions
);

//! C ABI entry point called by source-stripped AOT binaries from their generated main() (v3 - full 128-bit parse_options)
/** Like qore_aot_run_v2, but accepts full 128-bit parse options as two int64_t values.
*/
extern "C" int qore_aot_run_v3(
    int argc, char** argv,
    const uint8_t* metadata, int metadata_len,
    const char* label,
    int64_t parse_options_lo, int64_t parse_options_hi,
    const QoreAOTFunc* functions, int num_functions
);

//! C ABI entry point called by source-stripped AOT modules from their generated qore_module_init() (v3 - full 128-bit parse_options)
/** Like qore_aot_module_init_v2, but accepts full 128-bit parse options as two int64_t values.
*/
extern "C" QoreStringNode* qore_aot_module_init_v3(
    const uint8_t* metadata, int metadata_len,
    const char* label,
    int64_t parse_options_lo, int64_t parse_options_hi,
    const char* mod_name,
    const QoreAOTFunc* functions, int num_functions
);

//! Captures the module loader path before entering the generated AOT module init implementation.
extern "C" void qore_aot_set_module_init_context_path(QoreModuleInitContext* ctx);

//! Clears the captured module loader path after generated AOT module init returns.
extern "C" void qore_aot_clear_module_init_context_path();

//! Legacy C ABI entry point called by AOT-compiled modules from their generated qore_module_ns_init()
extern "C" void qore_aot_module_ns_init(QoreNamespace* root_ns, QoreNamespace* qore_ns);

//! C ABI entry point called by AOT-compiled modules from their generated qore_module_ns_init() adapter
/** Accepts the module manager ExceptionSink so AOT namespace initialization failures
    are reported as module-load errors instead of being reduced to diagnostics.
*/
extern "C" void qore_aot_module_ns_init_v2(QoreNamespace* root_ns, QoreNamespace* qore_ns,
    ExceptionSink* xsink);

//! Returns the recursive lock serializing AOT module application and initializer execution.
/** Slow runtime module loads take this lock before taking a target Program's parse lock.
    This establishes one lock order for concurrent child-Program module loading and prevents
    AOT initializer callbacks from deadlocking while resolving committed APIs in another Program.
*/
DLLLOCAL QoreRecursiveThreadLock& qore_aot_get_init_execution_lock();

//! C ABI entry point called by AOT-compiled modules from their generated qore_module_delete()
extern "C" void qore_aot_module_delete();

struct QoreModuleInfo;

//! C ABI helper to populate a QoreModuleInfo from flat C parameters
/** Called by the generated module descriptor function to fill in the module info struct.
    @param mod_info pointer to the QoreModuleInfo to populate
    @param name module feature name
    @param version version string
    @param desc description
    @param author author string
    @param url URL (may be nullptr)
    @param license_str license string (may be nullptr)
    @param api_major API major version
    @param api_minor API minor version
    @param license license enum value (qore_license_t cast to int)
    @param init_fn module init callback
    @param ns_init_fn namespace init callback
    @param del_fn module delete callback
    @param deps array of dependency name strings (may be nullptr if num_deps == 0)
    @param num_deps number of entries in deps array
*/
extern "C" void qore_aot_fill_module_desc(QoreModuleInfo* mod_info,
        const char* name, const char* version, const char* desc,
        const char* author, const char* url, const char* license_str,
        int api_major, int api_minor, int license,
        void* init_fn, void* ns_init_fn, void* del_fn,
        const char** deps, int num_deps);

//! C ABI helper to convert a QoreStringNode* init error to an exception
/** Called by the init adapter when the AOT init implementation returns a non-null error.
    @param xsink exception sink to raise the error in
    @param err the error string node (takes ownership)
*/
extern "C" void qore_aot_raise_init_error(ExceptionSink* xsink, QoreStringNode* err);

//! C ABI bridge for `.qo`-linked statically-compiled AOT modules.
/** Invoked by the per-module `qore_MOD_register(QoreProgram*)` entry point that
    `qcc -c` emits into each `.qo`.  Internally delegates to
    `ModuleManager::registerAOTStaticModule`; any exception raised by the underlying
    load is reported to stderr via `xsink.handleExceptions()` — there is no return
    value to signal failure yet.  The plain C function-pointer type is used instead
    of `qore_binary_module_desc_t` to keep IR emission straightforward.
*/
extern "C" void qore_aot_register_into_program(QoreProgram* tpgm,
        void (*desc_fn)(QoreModuleInfo&), const char* path);

//! Returns true if \a name is registered as an AOT user module (a source Qore
//! module compiled to .qmod binary form).  AOT user modules must be tracked in
//! userFeatureList rather than the builtin featureList so that importSystemApi()
//! and child-program inheritance don't incorrectly mark the module as already
//! loaded (which would cause %requires in child programs to be silently skipped
//! without re-merging the module's user-public symbols).
DLLLOCAL bool qore_is_aot_user_module(const char* name);

//! Returns the QoreProgram owning \a name if registered as an AOT user module, nullptr otherwise.
/** Used by reexport() to detect self-reexport (where the dep being reexported is the
    module that owns the target pgm) for AOT user modules, mirroring the QoreUserModule
    getProgram() check for source user modules.
*/
DLLLOCAL QoreProgram* qore_aot_get_module_pgm(const char* name);

//! Encodes a class reference for AOT metadata.
/** Module-originated classes include the owning module name so runtime
    resolution can fall back to the module-private program when the class was
    not imported into the target program (for example private helper classes
    referenced by public module methods).
 */
DLLLOCAL std::string qore_aot_encode_class_ref(const QoreClass* qc, bool pseudo = false);

//! Resolves a class reference emitted by qore_aot_encode_class_ref().
DLLLOCAL const QoreClass* qore_aot_resolve_class_ref(QoreProgram* pgm,
        const char* class_ref, bool pseudo = false);

//! Clears namespace/global/static data for all registered AOT user modules.
/** AOT user modules are loaded as binary modules, so they are skipped by
    QoreModuleManager::delUser().  This helper lets shutdown clear their global
    objects before any user-module programs are destroyed, mirroring source
    user-module teardown.
 */
DLLLOCAL void qore_aot_clear_all_module_namespace_data(ExceptionSink& xsink);

//! AOT compiler class — compiles a parsed QoreProgram to a standalone executable
class QoreAOT {
public:
    //! Compile a parsed program to a standalone executable
    /** The binary uses serialized metadata.
        @param pgm parsed QoreProgram (must have been parsed successfully)
        @param source_text original source text
        @param source_len length of source text
        @param label source label (filename)
        @param output_path path for the output executable
        @param parse_options parse options to embed
        @param error error message on failure
        @param opt_level LLVM optimization level 0-3 (default: 2)
        @param target_triple target triple for cross-compilation (nullptr = native)
        @param static_link statically link libqore (requires libqore_static.a)
        @param include_source embed source text in metadata
        @return true on success, false on failure
    */
    static bool compile(QoreProgram* pgm,
                       const char* source_text, int source_len,
                       const char* label,
                       const std::string& output_path,
                       const QoreParseOptions& parse_options,
                       std::string& error,
                       int opt_level = 2,
                       const char* target_triple = nullptr,
                       bool static_link = false,
                       bool include_source = false);

    //! Compile a .qm user module to a .qmod binary module
    /** The binary uses serialized metadata.
        @param source_text original module source text
        @param source_len length of source text
        @param label source label (filename)
        @param output_path path for the output .qmod binary module
        @param parse_options parse options to embed
        @param error error message on failure
        @param opt_level LLVM optimization level 0-3 (default: 2)
        @param target_triple target triple for cross-compilation (nullptr = native)
        @param include_source embed source text in metadata
        @return true on success, false on failure
    */
    static bool compileModule(const char* source_text, int source_len,
                              const char* label,
                              const std::string& output_path,
                              const QoreParseOptions& parse_options,
                              std::string& error,
                              int opt_level = 2,
                              const char* target_triple = nullptr,
                              bool include_source = false);

    //! Same as compileModule but with the `compile_only` knob to
    //! emit a `.qo` (ELF relocatable) instead of a `.qmod` shared object.
    /** Distinct overload (not a default arg) so the original mangled symbol
        stays intact for ABI compatibility with already-installed binaries
        that link against libqore (notably /usr/bin/qore built with
        BIND_NOW).
     */
    static bool compileModule(const char* source_text, int source_len,
                              const char* label,
                              const std::string& output_path,
                              const QoreParseOptions& parse_options,
                              std::string& error,
                              int opt_level,
                              const char* target_triple,
                              bool include_source,
                              bool compile_only);

    //! Compile a split (separated) .qm user module directory to a .qmod binary module
    /** Supports modules where code is spread across multiple files:
        - A main .qm file: {dir}/{basename}.qm
        - Multiple .qc and .ql component files (auto-discovered)

        The binary uses serialized metadata (no embedded source).

        @param dir_path path to the module directory
        @param output_path path for the output .qmod binary module
        @param parse_options parse options to embed
        @param error error message on failure
        @param opt_level LLVM optimization level 0-3 (default: 2)
        @param target_triple target triple for cross-compilation (nullptr = native)
        @param include_source embed source text in metadata
        @return true on success, false on failure
    */
    static bool compileSeparatedModule(const char* dir_path,
                                       const std::string& output_path,
                                       const QoreParseOptions& parse_options,
                                       std::string& error,
                                       int opt_level = 2,
                                       const char* target_triple = nullptr,
                                       bool include_source = false);

    //! Same as compileSeparatedModule with the `compile_only` knob.
    //! See the compileModule overload for ABI-compat rationale.
    static bool compileSeparatedModule(const char* dir_path,
                                       const std::string& output_path,
                                       const QoreParseOptions& parse_options,
                                       std::string& error,
                                       int opt_level,
                                       const char* target_triple,
                                       bool include_source,
                                       bool compile_only);

    //! Aggregate a set of per-file `.qo` files into a
    //! single `.qmod` without re-running LLVM codegen on already-compiled
    //! function bodies.
    /**
        The aggregator re-parses the source directory (fast — no codegen),
        walks the namespace tree to rebuild the full-module metadata, and
        emits a "glue" translation unit whose compiled-function entries
        are bare external declarations.  Those declarations resolve at
        link time against the actual bodies defined in the input `.qo`
        files (each previously built by `qcc -c --context=DIR FILE`).

        The result is the same functional `.qmod` that
        `qcc -m DIR` would produce, but the expensive LLVM emission
        happens once per per-file `.qo` (parallelizable via `make -j`)
        rather than once in a single serial `qcc -m` run.

        @param dir_path source directory for the split module (same
                        convention as `compileSeparatedModule`)
        @param object_paths list of `.qo` files produced by
                        `qcc -c --context=DIR` covering the
                        module's `.qm` + each `.qc`/`.ql`
        @param output_path path for the output `.qmod`
        @param parse_options parse options to seed the compile program
        @param error error message on failure
        @param opt_level LLVM optimization level 0-3 (default: 2)
        @param target_triple target triple for cross-compilation
                        (nullptr = native)
        @param include_source embed source text in metadata
        @return true on success, false on failure
    */
    static bool compileModuleFromObjects(const char* dir_path,
                                         const std::vector<std::string>& object_paths,
                                         const std::string& output_path,
                                         const QoreParseOptions& parse_options,
                                         std::string& error,
                                         int opt_level = 2,
                                         const char* target_triple = nullptr,
                                         bool include_source = false);

    //! Batch-compile a list of Qore application
    //! sources into one `.qo` per file, sharing a single parse cycle.
    /**
        Eliminates the O(N²) sibling-preload overhead of calling
        `compileScriptFile` N times (each invocation would re-parse
        every sibling `.qo`'s metadata).  Instead:

        1. Parse every source in @p target_files into a single
           QoreProgram (one `parsePending` per file, one
           `parseCommit`).  Cross-file references resolve through
           the standard parser name-walk — no `.qo` preload needed.
        2. For each source: compile its contributions with the
           per-file filter (`compile_file`), emit fragment
           + register symbols, write one collision-free `.qo` per target
           under `OUTPUT_DIR`.

        Functional output is identical to N separate
        `compileScriptFile` calls, just much faster at scale.  Batch
        output names and register symbols are derived from canonical source
        paths so same-basename sources cannot overwrite each other.  A
        qctl-shape build (24 files) does 1 parse instead of 24 with
        576 metadata preload ops.

        @param target_files absolute or relative paths of source
                files to compile together
        @param output_dir directory for output `.qo` files
                (created if missing; one collision-free `.qo` per target)
        @param parse_options parse options to seed the compile program
        @param error error message on failure
        @param opt_level LLVM optimization level 0-3 (default: 2)
        @param target_triple target triple for cross-compilation
                (nullptr = native)
        @param include_source embed source text in metadata
        @param require_modules modules to require while parsing the batch
        @param stub_files source files that provide declarations only
        @param parse_defines parse-time defines to apply to every target
        @param parse_option_flags parse-option flag names to apply to every target
        @param compiled_count_out optional output for the total number of
                emitted Qore code variants
        @param report_artifacts true to report each `.qo` as a user-visible
                qcc artifact; false when the objects are temporary
        @param depfile_dir optional directory for one Make depfile per output
                `.qo`; each depfile is named `<output-basename>.d`
        @return true on success, false on failure
    */
    static bool compileScriptFilesBatch(
            const std::vector<std::string>& target_files,
            const std::string& output_dir,
            const QoreParseOptions& parse_options,
            std::string& error,
            int opt_level = 2,
            const char* target_triple = nullptr,
            bool include_source = false,
            const std::vector<std::string>& require_modules = {},
            const std::vector<std::string>& stub_files = {},
            const std::vector<std::string>& parse_defines = {},
            const std::vector<std::string>& parse_option_flags = {},
            int* compiled_count_out = nullptr,
            bool report_artifacts = true,
            const std::string* depfile_dir = nullptr);

    //! Compile aggregate script metadata for a list of already-compiled
    //! script-context `.qo` objects.
    /**
        Parses @p target_files with the same script-context options used by
        `compileScriptFilesBatch`, then emits one relocatable object at
        @p output_path containing:

        - one aggregate AOT metadata blob for all target-file declarations;
        - no aggregate native function table; per-file `.qo` objects keep
          ownership of their native bodies and matching slot maps;
        - an exported `init_<aggregate_symbol>_qo(QoreProgram*)` register
          function that calls `qore_aot_script_register()` once.
          When @p register_native_inputs is true, the same entry point then
          calls each linked per-file object's native-only register entry so
          native bodies are bound from their own slot maps without
          re-deserializing per-file declarations.

        The aggregate object is linked alongside the per-file `.qo` objects.
        Runtime startup sees one declaration metadata deserialization session
        plus the per-file native registration sessions. This avoids pairing
        aggregate slot maps with per-file native bodies compiled from a
        different slot layout.

        @param target_files source files represented by the linked `.qo` set
        @param output_path path for the aggregate relocatable object
        @param aggregate_symbol C-identifier tail used in
                `init_<aggregate_symbol>_qo`
        @param parse_options parse options to seed the compile program
        @param error error message on failure
        @param opt_level LLVM optimization level 0-3 (default: 2)
        @param target_triple target triple for cross-compilation
                (nullptr = native)
        @param include_source embed source text in metadata
        @param require_modules modules to require while parsing the aggregate
        @param stub_files source files that provide declarations only
        @param parse_defines parse-time defines to apply to every target
        @param parse_option_flags parse-option flag names to apply to every target
        @param compiled_count_out optional output for the total number of
                emitted Qore code variants
        @param register_native_inputs emit calls to each per-file
                `*_script_native_register` symbol after aggregate metadata
                registration
        @return true on success, false on failure
    */
    static bool compileScriptAggregate(
            const std::vector<std::string>& target_files,
            const std::string& output_path,
            const std::string& aggregate_symbol,
            const QoreParseOptions& parse_options,
            std::string& error,
            int opt_level = 2,
            const char* target_triple = nullptr,
            bool include_source = false,
            const std::vector<std::string>& require_modules = {},
            const std::vector<std::string>& stub_files = {},
            const std::vector<std::string>& parse_defines = {},
            const std::vector<std::string>& parse_option_flags = {},
            int* compiled_count_out = nullptr,
            bool register_native_inputs = false);

    //! Compile an object-driven script register aggregate for existing `.qo` inputs.
    /**
        Emits one relocatable object at @p output_path containing an exported
        `init_<aggregate_symbol>_qo(QoreProgram*)` function.  The function calls
        each symbol in @p register_symbols in order.  The generated object is
        linked alongside the input `.qo` files, so the native linker validates
        that every per-file register symbol exists.

        This is the first object/index-driven `--link-qo` aggregation form: it
        avoids reparsing original sources and keeps the per-file metadata
        registration semantics unchanged.

        @param register_symbols exported `*_script_register` symbols to call
        @param output_path path for the aggregate relocatable object
        @param aggregate_symbol C-identifier tail used in
                `init_<aggregate_symbol>_qo`
        @param error error message on failure
        @param opt_level LLVM optimization level 0-3
        @param target_triple target triple for cross-compilation
                (nullptr = native)
        @return true on success, false on failure
    */
    static bool compileScriptRegisterAggregate(
            const std::vector<std::string>& register_symbols,
            const std::string& output_path,
            const std::string& aggregate_symbol,
            std::string& error,
            int opt_level = 2,
            const char* target_triple = nullptr);

    //! Compile one file of a Qore application to a
    //! `.qo` in script-context mode (no module wrapper, no `.qm`).
    /**
        Enables a C/C++-style build model for multi-file Qore
        applications (e.g. Qorus's qctl):

        ```
        qcc -c -LBUILD_DIR -o foo.qo foo.qr
        ```

        Sibling `.qo` files discovered under each `-L` directory are
        preloaded via `QoreAOTBinaryMultiDeserializer` so that
        cross-file type references in @p target_file (inheritance,
        member access, typed expressions) resolve at parse time.
        Parse context is otherwise plain script context — no
        PO_IN_MODULE, no module-context helper, no `.qm` required.
        Use `.qr` for modern script/application sources; `.q` keeps legacy
        defaults unless the source contains an explicit `%modern` directive.

        The output `.qo` carries:
        - compiled LLVM native code for items declared in @p target_file;
        - a fragment metadata blob describing just those items
          (object-file format, ExternalLinkage so script-object preload in
          downstream compiles can read it from the ELF symbol table);
        - fragment accessor symbols.

        @param target_file absolute path of the source file to compile
        @param library_paths directories scanned for sibling `.qo`s
                        to preload before parsing (C's `-L` semantic)
        @param output_path path for the output `.qo`
        @param parse_options parse options to seed the compile program
                        (PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES
                        suggested; `.qr` sources and `%modern` directives
                        will OR-merge additional bits)
        @param error error message on failure
        @param opt_level LLVM optimization level 0-3 (default: 2)
        @param target_triple target triple for cross-compilation
                        (nullptr = native)
        @param include_source embed source text in metadata
        @param require_modules modules to require before parsing
        @param stub_files source files that provide declarations only
        @param parse_defines parse-time defines to apply to the target
        @param source_symbols build-group source symbols that should be
                        deferred instead of binding same-name loaded symbols
        @return true on success, false on failure
    */
    static bool compileScriptFile(const char* target_file,
                                  const std::vector<std::string>& library_paths,
                                  const std::string& output_path,
                                  const QoreParseOptions& parse_options,
                                  std::string& error,
                                  int opt_level = 2,
                                  const char* target_triple = nullptr,
                                  bool include_source = false,
                                  const std::vector<std::string>& require_modules = {},
                                  const std::vector<std::string>& stub_files = {},
                                  const std::vector<std::string>& parse_defines = {},
                                  std::vector<std::string>* parsed_files = nullptr,
                                  const QoreAOTSourceSymbolManifest* source_symbols = nullptr);

    //! Package a set of per-file `.qo` files into a
    //! `.qoa` static archive with a single `qore_qoa_register_all()`
    //! entry point, suitable for static linkage into a C++ host
    //! (e.g. qorus-core).
    /**
        Runs the same metadata-only aggregation as
        `compileModuleFromObjects` but emits the glue `.o` with
        `generateModuleABIV2(compile_only=true)` — prefixed
        module-info globals, the module descriptor, and
        `qore_MOD_register` — AND additionally exports a
        `qore_qoa_register_all(QoreProgram*)` symbol that delegates to
        `qore_aot_register_into_program`.

        The glue `.o` and every input `.qo` are bundled into a single
        `ar rcs` archive at `output_path`.  A C++ host links the
        resulting `.qoa` alongside `-lqore` and calls
        `qore_qoa_register_all(pgm)` once to register the module's
        contents into a `QoreProgram`.

        The prefixed globals keep multiple independent `.qo` files
        within the archive from colliding on module-info symbols
        (archive-registration convention).  The archive ABI exposes a
        single `qore_qoa_register_all` symbol; hosts that link multiple
        archives need unique wrapper symbols at the host integration
        layer.

        @param dir_path source directory for the split module (same
                        convention as `compileModuleFromObjects`)
        @param object_paths list of `.qo` files covering the module's
                        `.qm` + each `.qc`/`.ql`
        @param output_path path for the output `.qoa`
        @param parse_options parse options to seed the compile program
        @param error error message on failure
        @param opt_level LLVM optimization level 0-3 (default: 2)
        @param target_triple target triple for cross-compilation
                        (nullptr = native)
        @param include_source embed source text in metadata
        @return true on success, false on failure
    */
    static bool archiveModuleFromObjects(const char* dir_path,
                                         const std::vector<std::string>& object_paths,
                                         const std::string& output_path,
                                         const QoreParseOptions& parse_options,
                                         std::string& error,
                                         int opt_level = 2,
                                         const char* target_triple = nullptr,
                                         bool include_source = false);

    //! Compile one file of a split module to a `.qo`.
    /** The full split-module directory @a dir_path is parsed so
        cross-file type references resolve, but only items whose AST
        declaration location matches @a target_file are lowered to
        native code and emitted in the resulting object.

        @a target_file is classified as either primary (its basename
        equals `MOD.qm`, where `MOD` is the directory basename) or
        secondary (any `.qc`/`.ql`/`.q` sibling).  The primary `.qo`
        carries module-info globals, the module descriptor and the
        public `qore_MOD_register` entry point; secondary `.qo`s
        carry just the compiled code (no metadata blob, no module-info
        globals, no register function) so that multiple per-file
        objects can be relocated together into one binary without
        symbol collisions.

        Per-file metadata fragments support link-time aggregation into a
        `.qmod` or `.qoa` and runtime composition across secondary `.qo`s.

        @param dir_path path to the owning module directory
        @param target_file absolute path of the single file to compile
        @param output_path path for the output `.qo`
        @param parse_options parse options to seed the compile program
        @param error error message on failure
        @param opt_level LLVM optimization level 0-3 (default: 2)
        @param target_triple target triple for cross-compilation (nullptr = native)
        @param include_source embed source text in metadata
        @return true on success, false on failure
    */
    static bool compileSeparatedModuleFile(const char* dir_path,
                                           const char* target_file,
                                           const std::string& output_path,
                                           const QoreParseOptions& parse_options,
                                           std::string& error,
                                           int opt_level = 2,
                                           const char* target_triple = nullptr,
                                           bool include_source = false);

    //! Print supported LLVM target architectures to stdout
    static void printSupportedTargets();
};

//! Build an AOTSlotMap by walking an IR function's instructions in deterministic order.
/** Assigns slot indices to each unique LocalVar*, Var*, and expression QoreValue.
    @param func the IR function to walk
    @param slots output slot map
*/
void buildAOTSlotMap(const QoreIRFunction& func, AOTSlotMap& slots);

//! Build a QoreAOTContext by walking a freshly-lowered IR function in the same order.
/** The fresh IR must have been produced from the same source as the compile-time IR,
    guaranteeing the same walk order and slot index correspondence.
    @param func the freshly-lowered IR function
    @param slots the slot map from compile time (used only for validation)
    @return heap-allocated context (caller takes ownership), or nullptr on mismatch
*/
QoreAOTContext* buildAOTContext(const QoreIRFunction& func, int num_locals, int num_globals, int num_exprs, int num_stmts, int num_regex_cases);

class AbstractStatement;
class StatementBlock;

//! Collect statement pointers for AOT stmt_slots by walking the AST in depth-first order.
/** Collects OnBlockExitStatement handler code blocks and reference ForEachStatement pointers
    in the same order that buildAOTSlotMap() processes them in the IR.
    @param block the statement block to walk
    @param stmts output vector of statement pointers
*/
void collectStmtSlotStatements(const StatementBlock* block,
        std::vector<const AbstractStatement*>& stmts);

//! Serialize a QoreValue expression tree to a binary blob using the EXPR_TREE format.
/** Used by AOT binary serialization to persist BCA (Base Class Constructor Argument) expressions.
    @param v the expression value to serialize
    @param slots the slot map for resolving local/global variable references
    @param out receives the serialized binary blob
    @param debug if true, print debug info for unsupported node types
    @return true if successful, false if the tree contains unsupported nodes
*/
bool serializeExprTreeToBlob(QoreValue v, const AOTSlotMap& slots, std::vector<uint8_t>& out, bool debug = false, const AOTConstantReverseMap* const_reverse_map = nullptr);

//! Deserialize a binary EXPR_TREE blob back into a QoreValue expression tree.
/** Used by AOT binary deserialization to reconstruct BCA (Base Class Constructor Argument) expressions.
    @param data pointer to the blob data
    @param size size of the blob data
    @param pgm the QoreProgram for namespace resolution
    @param locals array of LocalVar* pointers for local var slot resolution
    @param num_locals number of locals in the array
    @return the deserialized QoreValue, or QoreValue() on failure
*/
QoreValue deserializeExprTreeFromBlob(const uint8_t* data, uint32_t size, QoreProgram* pgm,
        LocalVar** locals, int num_locals);

#endif
