/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreAOTBinary.h

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

#ifndef _QORE_INTERN_QOREAOTBINARY_H
#define _QORE_INTERN_QOREAOTBINARY_H

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <string_view>
#include <functional>
#include <deque>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <qore/QoreValue.h>
#include <qore/QoreEnumDecl.h>
#include <qore/TypedHashDecl.h>
#include "qore/intern/qore_aot_deps.h"

class AbstractQoreNode;
class QoreValue;
class QoreProgram;
class QoreTypeInfo;
class qore_class_private;
class qore_ns_private;
class QoreIRFunction;
class LocalVar;
class Var;
class UserVariantBase;
class UserSignature;
class QoreFunction;
class AbstractQoreFunctionVariant;
class QoreParseListNode;
struct QoreTypeParamInstantiation;
class ExceptionSink;
class RuntimeConstantRefNode;
struct QoreProgramLocation;
struct AOTCompiledFuncWithSlots;

bool qore_check_cancel(ExceptionSink* xsink, const char* operation);

//! Reverse map from constant value node pointer to fully-qualified constant name
typedef std::unordered_map<const AbstractQoreNode*, std::string> AOTConstantReverseMap;

//! Adds a constant value and any nested hash/list node values to the AOT reverse map.
void qore_aot_add_constant_value_reverse_mappings(AOTConstantReverseMap& crm,
    const QoreValue& v, const std::string& path);

//! Resolves a RuntimeConstantRefNode to an AOT-serializable constant path.
bool qore_aot_resolve_runtime_constant_path(const RuntimeConstantRefNode* node,
    const AOTConstantReverseMap* const_reverse_map, std::string& path);

//! Resolves a top-level or encoded nested AOT constant path to a runtime value.
QoreValue qore_aot_resolve_constant_path_value(QoreProgram* pgm, const char* path,
    bool defer_if_pending, bool wrap_top_level_if_ready = false, bool* resolved = nullptr);

//! Returns the canonical AOT-serializable type path for \a ti.
std::string qore_get_aot_serializable_type_path(const QoreTypeInfo* ti, bool no_narrow = false);

//! Marker for method-ref payloads that carry deferred call argument type metadata.
constexpr const char* QORE_AOT_STATIC_CALL_ARG_TYPES_MARKER = "@qore-aot-static-call-arg-types-v1";
constexpr const char* QORE_AOT_EXPLICIT_TYPE_ARGS_MARKER = "@qore-aot-explicit-type-args-v1";

struct QoreAOTStaticMethodRef {
    const char* method_name = nullptr;
    const char* variant_class_path = nullptr;
    const char* sig_text = nullptr;
    const char* arg_type_sig = nullptr;
    bool explicit_type_args_present = false;
    bool explicit_type_args_valid = true;
    std::vector<std::string> explicit_type_arg_paths;
    std::string encoded_storage;
    std::string method_name_storage;
    std::string variant_class_storage;

    QoreAOTStaticMethodRef(const char* encoded);
};

//! Encodes a static method reference, optionally with an exact variant or deferred argument type signature.
std::string qore_aot_encode_static_method_ref(const char* method_name,
    const AbstractQoreFunctionVariant* variant = nullptr,
    const type_vec_t* arg_types = nullptr,
    const QoreTypeParamInstantiation* explicit_type_param_instantiation = nullptr);

//! Resolves a static-call argument type signature and uses it to find a method variant.
const AbstractQoreFunctionVariant* qore_aot_resolve_variant_from_arg_type_signature(QoreProgram* pgm,
    QoreFunction* func, const char* arg_type_sig, const qore_class_private* class_ctx,
    const QoreTypeInfo* receiver_type_info, QoreTypeParamInstantiation* type_param_instantiation,
    std::string& error);

//! Resolves a serialized hashdecl path, including parameterized hashdecl paths.
const TypedHashDecl* qore_aot_resolve_hashdecl_path(QoreProgram* pgm, const char* path);

//! Magic number: "QORD" in little-endian (0x44524F51)
constexpr uint32_t QORE_AOT_BINARY_MAGIC = 0x44524F51;

//! Current binary format version
//! v1: initial format with full 128-bit parse options + source hash
//! v2: added BCA (Base Class Constructor Arguments) serialization for constructors
//! v3: added timezone metadata for IR date constants
//! v4: all serialized cast expression kinds carry their inner expression
//! v5: fixed-offset IR date constants serialize their UTC offset instead of an empty zone name
//! v6: added char value serialization tag
//! v7: serialized Find instructions include the explicit find mode
//! v8: serialized Phi instructions include the PHI value representation kind
//! v9: optional CALL_RELOCATIONS section describes pre-resolved direct call slots
//! v10: callable generic type-parameter declarations are preserved in variant signatures
//! v11: all serialized source line numbers use signed 32-bit values
//! v12: whole-body and sectioned Zstandard metadata compression are supported
//! v13: lazy debugger IR is stored in a separate section referenced by SLOT_MAPS ranges
//! v14: init-function descriptors can target namespace global variables
constexpr uint16_t QORE_AOT_BINARY_MIN_VERSION = 9;
constexpr uint16_t QORE_AOT_BINARY_VERSION = 14;
//! First format version storing lazy debugger IR in a separate section.
constexpr uint16_t QORE_AOT_SPLIT_DEBUG_IR_VERSION = 13;

constexpr uint8_t QORE_AOT_COMPRESSION_NONE = 0;
constexpr uint8_t QORE_AOT_COMPRESSION_ZLIB = 1;
constexpr uint8_t QORE_AOT_COMPRESSION_ZSTD = 2;
constexpr uint8_t QORE_AOT_COMPRESSION_SECTIONED_ZSTD = 3;

//! NEW_OBJECT expression-slot ref2 marker for constructors that must resolve their class at runtime.
constexpr const char* QORE_AOT_DEFERRED_CREATE_OBJECT_SLOT = "deferred-create-object";

//! On-disk header size (60 bytes)
constexpr uint32_t QORE_AOT_HEADER_SIZE = 60;

//! Binary header flags
constexpr uint16_t QORE_AOT_FLAG_HAS_TOPLEVEL = 0x0001;
constexpr uint16_t QORE_AOT_FLAG_IS_MODULE    = 0x0002;

//! Feature flags for binary compatibility (64-bit bitmask in header.feature_flags)
constexpr uint64_t QORE_AOT_FEAT_FOREACH_REF    = 1ULL << 0;  //!< RefForeach* opcodes (323-328)
constexpr uint64_t QORE_AOT_FEAT_NATIVE_CAST     = 1ULL << 1;  //!< Cast* opcodes (249-254)
constexpr uint64_t QORE_AOT_FEAT_BLOCK_EXIT      = 1ULL << 2;  //!< OnBlockExit opcode
constexpr uint64_t QORE_AOT_FEAT_DIRECT_INDEX    = 1ULL << 3;  //!< ListGet* opcodes (13-15)
constexpr uint64_t QORE_AOT_FEAT_HASH_KEY_ACCESS = 1ULL << 4;  //!< HashKeyAccess opcodes
constexpr uint64_t QORE_AOT_FEAT_FAST_CALL       = 1ULL << 5;  //!< CallMethodDirect/CallStaticDirect
constexpr uint64_t QORE_AOT_FEAT_COMPLEX_RETURN  = 1ULL << 6;  //!< reserved, set to 0 for now
constexpr uint64_t QORE_AOT_FEAT_HASH_KEY_STORE  = 1ULL << 7;  //!< HashKeyStore opcode (333)
constexpr uint64_t QORE_AOT_FEAT_LIST_INDEX_STORE = 1ULL << 8;  //!< ListIndexStore opcode (335)
constexpr uint64_t QORE_AOT_FEAT_TYPE_TABLE      = 1ULL << 9;  //!< per-blob pre-resolved type-path table (TYPE_TABLE section)
constexpr uint64_t QORE_AOT_FEAT_CONST_PENDING   = 1ULL << 10; //!< per-constant pending-init-func flag in CONSTANTS / CLASSES
constexpr uint64_t QORE_AOT_FEAT_SIG_LINES       = 1ULL << 11; //!< per-variant signature start/end line pair follows the flags u16 in writeVariantSignature
constexpr uint64_t QORE_AOT_FEAT_CONTEXT_IR      = 1ULL << 12; //!< native IR lowering of `context` statement (Context carries name+exp+where+sort; ContextMaxPos/SetPos/Destroy opcodes present)
constexpr uint64_t QORE_AOT_FEAT_LVPATH_SLICE    = 1ULL << 13; //!< LVPathStepKind::HashKeySlice / ListIndexSlice / ListRangeSlice with slice_operand_ids vector (multi-key hash / multi-index/range list remove/delete)
constexpr uint64_t QORE_AOT_FEAT_MODULE_PATH_LISTS = 1ULL << 14; //!< per-Program %prepend-module-path / %append-module-path lists (MODULE_PATH_PREPEND / MODULE_PATH_APPEND sections)
constexpr uint64_t QORE_AOT_FEAT_LVPATH_DELETE_EXPR = 1ULL << 15; //!< Legacy: LValuePath records include an AST delete/remove expression; readers skip it, writers no longer emit it.
constexpr uint64_t QORE_AOT_FEAT_LVPATH_PATTERN = 1ULL << 16; //!< serialized IR LValuePath records include optional regex/transliteration pattern metadata
constexpr uint64_t QORE_AOT_FEAT_FUNC_CALL_VARIANT = 1ULL << 17; //!< FUNC_CALL expression slots include parse-time variant signature metadata
constexpr uint64_t QORE_AOT_FEAT_BACKQUOTE = 1ULL << 18; //!< native IR Backquote opcode
constexpr uint64_t QORE_AOT_FEAT_FIND = 1ULL << 19; //!< native IR Find opcode
constexpr uint64_t QORE_AOT_FEAT_BACKGROUND_IR = 1ULL << 20; //!< native IR background call metadata
constexpr uint64_t QORE_AOT_FEAT_INLINE_CALL_ARGS = 1ULL << 21; //!< inline FUNC_CALL expression payloads include serialized argument expressions
constexpr uint64_t QORE_AOT_FEAT_LIST_SELECTOR_RANGE = 1ULL << 22; //!< ListIndexDynamic expression records include range-selector metadata
constexpr uint64_t QORE_AOT_FEAT_ENTRY_STMT_LINES = 1ULL << 23; //!< per-variant function-entry StatementBlock start/end line pair follows signature lines
constexpr uint64_t QORE_AOT_FEAT_PARSE_REF_TYPE = 1ULL << 24; //!< PARSE_REF records include the parse-time reference type path before the lvalue expression
constexpr uint64_t QORE_AOT_FEAT_STMT_LOC_TABLE = 1ULL << 25; //!< SLOT_MAPS entries may carry source-stripped metadata-only statement locations
constexpr uint64_t QORE_AOT_FEAT_DEBUG_IR = 1ULL << 26; //!< Function metadata carries full IR for source-stripped debugging
constexpr uint64_t QORE_AOT_FEAT_SELF_CALL_ARGS = 1ULL << 27; //!< inline SELF_METHOD_CALL expression payloads include serialized argument expressions
constexpr uint64_t QORE_AOT_FEAT_BODY_LOCAL_SLOT = 1ULL << 28; //!< body-local records include their local slot id for duplicate-name disambiguation
constexpr uint64_t QORE_AOT_FEAT_BCA_LINES = 1ULL << 29; //!< BCA records include source line ranges for parent-constructor argument callstacks
constexpr uint64_t QORE_AOT_FEAT_BCA_NATIVE_ARGS = 1ULL << 30; //!< BCA arg blobs contain native inline AOT expressions, not legacy EXPR_TREE blobs
constexpr uint64_t QORE_AOT_FEAT_CLOSURE_VARARGS_FLAGS = 1ULL << 31; //!< closure records split effective varargs from signature ellipsis
constexpr uint64_t QORE_AOT_FEAT_CONTAINER_TYPEINFO = 1ULL << 32; //!< MakeList/MakeHash records carry parse-time container typeInfo
constexpr uint64_t QORE_AOT_FEAT_CLASS_HASH = 1ULL << 33; //!< CLASSES records carry parser-produced class signature hashes
constexpr uint64_t QORE_AOT_FEAT_METHOD_SYNC = 1ULL << 34; //!< FUNCTIONS/METHODS variant flags preserve synchronized gates
constexpr uint64_t QORE_AOT_FEAT_TYPED_VALUE_CONTAINERS = 1ULL << 35; //!< Serialized list/hash values preserve complex/hashdecl runtime typeInfo
constexpr uint64_t QORE_AOT_FEAT_MODULE_COMMANDS = 1ULL << 36; //!< `%module-cmd` directives replayed from source-stripped AOT metadata
constexpr uint64_t QORE_AOT_FEAT_WIDE_IR_OPERANDS = 1ULL << 37; //!< Serialized debug/handler IR instruction operand counts are u16, not legacy u8
constexpr uint64_t QORE_AOT_FEAT_WIDE_LOC_TABLES = 1ULL << 38; //!< SLOT_MAPS location and statement-location table counts are u32, not legacy u16
constexpr uint64_t QORE_AOT_FEAT_LOCAL_DECL_ORDINAL = 1ULL << 39; //!< local slot records carry body-local ordinal for duplicate-name disambiguation
constexpr uint64_t QORE_AOT_FEAT_CLASS_INJECTION = 1ULL << 40; //!< class records preserve import/injection compatibility metadata
constexpr uint64_t QORE_AOT_FEAT_VARIANT_PARSE_OPTIONS = 1ULL << 41; //!< variant signatures carry original body parse options for source-stripped domain checks
constexpr uint64_t QORE_AOT_FEAT_BCA_NAMED_ARG_MAP = 1ULL << 42; //!< BCA records preserve named-argument source-to-parameter evaluation maps
constexpr uint64_t QORE_AOT_FEAT_NEW_OBJECT_TYPEINFO = 1ULL << 43; //!< NewObject records and slots preserve instantiated object type paths
constexpr uint64_t QORE_AOT_FEAT_CLASS_TYPE_PARAMS = 1ULL << 44; //!< class records preserve source generic type parameter names
constexpr uint64_t QORE_AOT_FEAT_CLASS_PARAM_BASES = 1ULL << 45; //!< class base records preserve parameterized generic parent type paths
constexpr uint64_t QORE_AOT_FEAT_CLASS_RAW_GENERIC = 1ULL << 46; //!< class records preserve source raw generic compatibility flags
constexpr uint64_t QORE_AOT_FEAT_STATIC_CALL_RECEIVER_TYPE = 1ULL << 47; //!< static method calls preserve parameterized receiver typeInfo
constexpr uint64_t QORE_AOT_FEAT_HASHDECL_TYPE_PARAMS = 1ULL << 48; //!< hashdecl records preserve source generic type parameter names
constexpr uint64_t QORE_AOT_FEAT_TYPE_PARAM_DEFAULTS = 1ULL << 49; //!< class/hashdecl type parameter records preserve default type arguments
constexpr uint64_t QORE_AOT_FEAT_HASHDECL_PARAM_PARENTS = 1ULL << 50; //!< hashdecl parent records may preserve parameterized generic parent type paths
constexpr uint64_t QORE_AOT_FEAT_TYPE_PARAM_BOUNDS = 1ULL << 51; //!< class/hashdecl type parameter records preserve bound type arguments
constexpr uint64_t QORE_AOT_FEAT_PLUGIN_DISPATCH = 1ULL << 52; //!< IR debug/AOT records may contain plugin dispatch opcodes
constexpr uint64_t QORE_AOT_FEAT_COMPLEX_BUFFER_INIT_KIND = 1ULL << 53; //!< complex buffer records/slots preserve sized/filled factory kind
constexpr uint64_t QORE_AOT_FEAT_READONLY_LOCALS = 1ULL << 54; //!< signatures and local slot metadata preserve read-only local bindings
constexpr uint64_t QORE_AOT_FEAT_CONST_METHODS = 1ULL << 55; //!< METHODS variant flags preserve const-method receiver contracts
constexpr uint64_t QORE_AOT_FEAT_CALL_CLOSURE_REF_ARGS = 1ULL << 56; //!< Closure-call records preserve caller-cache invalidation metadata
constexpr uint64_t QORE_AOT_FEAT_TYPED_PHI = 1ULL << 57; //!< Serialized Phi records preserve native/QoreValue representation metadata
constexpr uint64_t QORE_AOT_FEAT_CALL_RELOCATIONS = 1ULL << 58; //!< CALL_RELOCATIONS section records safe direct-call link candidates
constexpr uint64_t QORE_AOT_FEAT_HASH_DEREF_TYPEINFO = 1ULL << 59; //!< HASH_DEREF records preserve parse-time result typeInfo
constexpr uint64_t QORE_AOT_FEAT_GLOBAL_SLOT_FLAGS = 1ULL << 60; //!< SLOT_MAPS global records preserve required import flags
constexpr uint64_t QORE_AOT_FEAT_SELF_CALL_SLOT_ARGS = 1ULL << 61; //!< SLOT_MAPS SELF_METHOD_CALL records carry serialized call args (u8 num_args + N×expr), so AST/IR-interpreter fallback evaluation dispatches the self call with its arguments
constexpr uint64_t QORE_AOT_FEAT_DOT_EVAL_PSEUDO_FLAGS = 1ULL << 62; //!< DotEvalMethodDirect records preserve pseudo analysis flags for safe LLVM fast paths
constexpr uint64_t QORE_AOT_FEAT_NATIVE_CLOSURE_BODY = 1ULL << 63; //!< Closure records carry an optional native AOT body key
//! Mask of all currently supported features
constexpr uint64_t QORE_AOT_SUPPORTED_FEATURES   = 0xFFFFFFFFFFFFFFFFULL;

//! Section type IDs
enum class QoreAOTSectionType : uint16_t {
    STRINGS       = 1,
    NAMESPACES    = 2,
    CLASSES       = 3,
    HASHDECLS     = 4,
    ENUMS         = 5,
    TYPEDEFS      = 6,
    CONSTANTS     = 7,
    GLOBALS       = 8,
    FUNCTIONS     = 9,
    METHODS       = 10,
    SLOT_MAPS     = 11,
    TOPLEVEL      = 12,
    FUNC_SOURCES  = 13,  //!< Embedded source plus the legacy source-fallback function list
    DEPENDENCIES  = 14,  //!< Module dependencies (for strip-source modules)
    REEXPORT_MODULES = 15,  //!< Modules that should be reexported (for strip-source modules)
    PROGRAM_METADATA = 16,  //!< Program-level metadata (exec-class name, etc.)
    INIT_FUNCS       = 17,  //!< Init functions for constants/static vars with lowered init expressions
    TYPE_TABLE       = 18,  //!< Per-blob interned type-path table (bulk-resolved at shell phase)
    MODULE_PATH_PREPEND = 19,  //!< Per-Program %prepend-module-path expanded path list (count u32 + count × StringRef)
    MODULE_PATH_APPEND  = 20,  //!< Per-Program %append-module-path expanded path list (count u32 + count × StringRef)
    BUILD_INFO      = 21,  //!< Producer/build metadata as key-value string pairs
    MODULE_COMMANDS = 22,  //!< `%module-cmd` directives (count u32 + count × module StringRef + command StringRef)
    PLUGIN_TYPE_REGISTRY = 23,  //!< Plugin module/type/operation metadata for referenced plugin imports
    PLUGIN_IMPORTS       = 24,  //!< Required plugin modules and local type/operation ids
    PLUGIN_HELPER_REFS   = 25,  //!< AOT plugin helper slot refs to plugin imports
    SYMBOL_INDEX         = 26,  //!< Optional versioned Qore/native symbol and dependency index
    CALL_RELOCATIONS     = 27,  //!< Optional direct-call slot relocation candidates
    DEBUG_IR             = 28,  //!< Lazy debugger IR payloads referenced by SLOT_MAPS entries
};

//! Symbol kinds written to the optional SYMBOL_INDEX section.
enum class QoreAOTSymbolKind : uint8_t {
    NAMESPACE     = 1,
    CLASS         = 2,
    HASHDECL      = 3,
    ENUM          = 4,
    ENUM_MEMBER   = 5,
    TYPEDEF       = 6,
    CONSTANT      = 7,
    GLOBAL        = 8,
    FUNCTION      = 9,
    METHOD        = 10,
    STATIC_METHOD = 11,
    CONSTRUCTOR   = 12,
    STATIC_VAR    = 13,
    NATIVE        = 14,
};

//! Dependency class for symbol-index imports and advisory native records.
enum class QoreAOTDependencyClass : uint8_t {
    UNKNOWN        = 0,
    SOURCE_TEXT    = 1,
    QORE_API       = 2,
    QORE_VALUE     = 3,
    NATIVE_BODY    = 4,
    MODULE_API     = 5,
    MODULE_RUNTIME = 6,
    DYNAMIC        = 7,
};

//! SYMBOL_INDEX record flag: native symbol is defined by this object.
constexpr uint16_t QORE_AOT_SYMBOL_FLAG_NATIVE_DEFINED = 0x0001;
//! SYMBOL_INDEX record flag: import is advisory/optional and may fall back to runtime lookup.
constexpr uint16_t QORE_AOT_SYMBOL_FLAG_OPTIONAL_IMPORT = 0x0002;

//! Version of the optional SYMBOL_INDEX section wire format.
constexpr uint16_t QORE_AOT_SYMBOL_INDEX_VERSION = 37;

//! Maximum source files in one serialized body-summary provenance set.
constexpr size_t QORE_AOT_WIRE_BODY_DEPENDENCY_MAX_FILES = 100000;

//! Maximum serialized nodes in a bounded pure native-integer expression summary.
constexpr size_t QORE_AOT_WIRE_INT_EXPRESSION_MAX_NODES = 64;

//! Serialized node in a bounded pure native-integer expression summary.
struct QoreAOTIntExpressionNodeRecord {
    uint8_t kind = 0;
    uint8_t lhs = UINT8_MAX;
    uint8_t rhs = UINT8_MAX;
    uint8_t third = UINT8_MAX;
    int8_t param = -1;
    int64_t constant = 0;
    std::string key;
};

//! Maximum serialized nodes in a bounded pure native-float expression summary.
constexpr size_t QORE_AOT_WIRE_FLOAT_EXPRESSION_MAX_NODES = 64;

//! Serialized node in a bounded pure native-float expression summary.
struct QoreAOTFloatExpressionNodeRecord {
    uint8_t kind = 0;
    uint8_t lhs = UINT8_MAX;
    uint8_t rhs = UINT8_MAX;
    int8_t param = -1;
    double constant = 0.0;
    std::string key;
};

//! Serialized node in a bounded typed string expression summary.
constexpr size_t QORE_AOT_WIRE_STRING_EXPRESSION_MAX_NODES = 64;

struct QoreAOTStringExpressionNodeRecord {
    uint8_t kind = 0;
    uint8_t lhs = UINT8_MAX;
    uint8_t rhs = UINT8_MAX;
    uint8_t third = UINT8_MAX;
    int8_t param = -1;
    int64_t int_constant = 0;
    std::string string_constant;
};

struct QoreAOTAggregateReturnValueRecord {
    uint8_t kind = 0;
    int8_t param = -1;
    int64_t int_value = 0;
    double float_value = 0.0;
};

struct QoreAOTAggregateReturnSelectRecord {
    uint8_t value_index = 0;
    int8_t condition_param = -1;
    QoreAOTAggregateReturnValueRecord true_value;
    QoreAOTAggregateReturnValueRecord false_value;
};

constexpr uint32_t QORE_AOT_FAST_ENTRY_PRESENT = 0x0001; //!< Record describes a callable fast entry
constexpr uint32_t QORE_AOT_FAST_ENTRY_CONTEXT_INDEPENDENT = 0x0002; //!< No callee AOT context required
constexpr uint32_t QORE_AOT_FAST_ENTRY_MAY_INVALIDATE = 0x0004; //!< Callee may mutate caller-visible state
constexpr uint32_t QORE_AOT_FAST_ENTRY_NEVER_NOTHING = 0x0008; //!< All normal returns are non-NOTHING
constexpr uint32_t QORE_AOT_FAST_ENTRY_IMPLICIT_SELF = 0x0010; //!< Entry reuses caller self/class context
//! Runtime-local effect metadata is present; without this bit readers fall back
//! to QORE_AOT_FAST_ENTRY_MAY_INVALIDATE.
constexpr uint32_t QORE_AOT_FAST_ENTRY_PRECISE_LOCAL_EFFECTS = 0x0020;
constexpr uint32_t QORE_AOT_FAST_ENTRY_MAY_MODIFY_RUNTIME_LOCALS = 0x0040;
//! Entry is valid only for the serialized generic specialization dispatch key.
constexpr uint32_t QORE_AOT_FAST_ENTRY_GENERIC_SPECIALIZATION = 0x0080;

//! One record in the optional SYMBOL_INDEX section.
struct QoreAOTSymbolIndexRecord {
    QoreAOTSymbolKind kind = QoreAOTSymbolKind::NAMESPACE;
    QoreAOTDependencyClass dependency_class = QoreAOTDependencyClass::UNKNOWN;
    uint16_t flags = 0;
    uint32_t metadata_slot = UINT32_MAX;
    //! Serialization-only declaration location; represented as SYMBOL_INDEX
    //! context and deliberately not part of the per-record wire format.
    int32_t declaration_start_line = -1;
    int32_t declaration_end_line = -1;
    int32_t declaration_entry_start_line = -1;
    int32_t declaration_entry_end_line = -1;
    std::string qore_path;
    std::string source_file;
    std::string visibility;
    std::string signature_hash;
    std::string declaration_hash;
    std::string value_hash;
    std::string body_contract_hash;
    std::string native_symbol;
    std::string abi_kind;
    std::string consumer_source_file;
    std::string provider_source_file;
    uint32_t fast_entry_flags = 0;
    uint32_t fast_entry_num_params = 0;
    uint8_t fast_return_kind = 0;
    std::vector<uint8_t> fast_param_kinds;
    std::vector<uint8_t> fast_param_rejects_nothing;
    std::vector<uint8_t> fast_param_noescape;
    std::vector<uint8_t> fast_param_may_modify;
    uint8_t scalar_leaf_kind = 0;
    uint16_t scalar_leaf_opcode = 0;
    int8_t scalar_leaf_lhs_param = -1;
    int8_t scalar_leaf_rhs_param = -1;
    int64_t scalar_leaf_lhs_int = 0;
    int64_t scalar_leaf_rhs_int = 0;
    double scalar_leaf_lhs_float = 0.0;
    double scalar_leaf_rhs_float = 0.0;
    int64_t scalar_leaf_true_scale = 0;
    int64_t scalar_leaf_true_offset = 0;
    int64_t scalar_leaf_false_scale = 0;
    int64_t scalar_leaf_false_offset = 0;
    std::string object_getter_member;
    std::string object_set_get_member;
    int8_t object_set_get_param = -1;
    std::string object_compound_get_member;
    int8_t object_compound_get_param = -1;
    uint8_t object_compound_get_op = 0;
    uint8_t string_op_kind = 0;
    int8_t string_op_base_param = -1;
    int8_t string_op_arg0_param = -1;
    int8_t string_op_arg1_param = -1;
    uint8_t collection_op_kind = 0;
    int8_t collection_op_base_param = -1;
    int8_t collection_op_index_param = -1;
    bool collection_op_string_index_char = false;
    std::string collection_op_key;
    uint8_t composed_int_source_kind = 0;
    int8_t composed_int_base_param = -1;
    int8_t composed_int_value_param = -1;
    int64_t composed_int_source_scale = 0;
    int64_t composed_int_value_scale = 0;
    int64_t composed_int_offset = 0;
    int8_t global_int_value_param = -1;
    int32_t global_int_slot = -1;
    int64_t global_int_value_scale = 0;
    int64_t global_int_global_scale = 0;
    int64_t global_int_offset = 0;
    std::vector<QoreAOTIntExpressionNodeRecord> int_expression_nodes;
    std::vector<QoreAOTFloatExpressionNodeRecord> float_expression_nodes;
    std::vector<QoreAOTStringExpressionNodeRecord> string_expression_nodes;
    uint8_t aggregate_return_kind = 0;
    std::vector<int8_t> aggregate_return_value_params;
    std::vector<uint8_t> aggregate_return_value_kinds;
    std::vector<int64_t> aggregate_return_value_ints;
    std::vector<double> aggregate_return_value_floats;
    std::vector<std::string> aggregate_return_keys;
    int8_t aggregate_return_shape_condition_param = -1;
    uint8_t aggregate_return_shape_true_size = 0;
    uint8_t aggregate_return_shape_false_size = 0;
    std::vector<QoreAOTAggregateReturnSelectRecord>
        aggregate_return_value_selects;
    int8_t boxed_return_param = -1;
    std::string fast_specialization_key;
    uint8_t boxed_return_kind = 0;
    std::vector<std::string> body_dependency_files;
    std::vector<QoreAOTBodyContractDependency> body_contract_dependencies;
};

//! Compile-time fast-entry metadata keyed by the resolved variant.
struct QoreAOTFastEntryIndexInfo {
    std::string native_symbol;
    uint32_t flags = 0;
    uint32_t num_params = 0;
    uint8_t return_kind = 0;
    std::vector<uint8_t> param_kinds;
    std::vector<uint8_t> param_rejects_nothing;
    std::vector<uint8_t> param_noescape;
    std::vector<uint8_t> param_may_modify;
    uint8_t scalar_leaf_kind = 0;
    uint16_t scalar_leaf_opcode = 0;
    int8_t scalar_leaf_lhs_param = -1;
    int8_t scalar_leaf_rhs_param = -1;
    int64_t scalar_leaf_lhs_int = 0;
    int64_t scalar_leaf_rhs_int = 0;
    double scalar_leaf_lhs_float = 0.0;
    double scalar_leaf_rhs_float = 0.0;
    int64_t scalar_leaf_true_scale = 0;
    int64_t scalar_leaf_true_offset = 0;
    int64_t scalar_leaf_false_scale = 0;
    int64_t scalar_leaf_false_offset = 0;
    std::string object_getter_member;
    std::string object_set_get_member;
    int8_t object_set_get_param = -1;
    std::string object_compound_get_member;
    int8_t object_compound_get_param = -1;
    uint8_t object_compound_get_op = 0;
    uint8_t string_op_kind = 0;
    int8_t string_op_base_param = -1;
    int8_t string_op_arg0_param = -1;
    int8_t string_op_arg1_param = -1;
    uint8_t collection_op_kind = 0;
    int8_t collection_op_base_param = -1;
    int8_t collection_op_index_param = -1;
    bool collection_op_string_index_char = false;
    std::string collection_op_key;
    uint8_t composed_int_source_kind = 0;
    int8_t composed_int_base_param = -1;
    int8_t composed_int_value_param = -1;
    int64_t composed_int_source_scale = 0;
    int64_t composed_int_value_scale = 0;
    int64_t composed_int_offset = 0;
    int8_t global_int_value_param = -1;
    int32_t global_int_slot = -1;
    int64_t global_int_value_scale = 0;
    int64_t global_int_global_scale = 0;
    int64_t global_int_offset = 0;
    std::vector<QoreAOTIntExpressionNodeRecord> int_expression_nodes;
    std::vector<QoreAOTFloatExpressionNodeRecord> float_expression_nodes;
    std::vector<QoreAOTStringExpressionNodeRecord> string_expression_nodes;
    uint8_t aggregate_return_kind = 0;
    std::vector<int8_t> aggregate_return_value_params;
    std::vector<uint8_t> aggregate_return_value_kinds;
    std::vector<int64_t> aggregate_return_value_ints;
    std::vector<double> aggregate_return_value_floats;
    std::vector<std::string> aggregate_return_keys;
    int8_t aggregate_return_shape_condition_param = -1;
    uint8_t aggregate_return_shape_true_size = 0;
    uint8_t aggregate_return_shape_false_size = 0;
    std::vector<QoreAOTAggregateReturnSelectRecord>
        aggregate_return_value_selects;
    int8_t boxed_return_param = -1;
    std::string specialization_key;
    uint8_t boxed_return_kind = 0;
    std::string body_contract_source_file;
    std::string body_contract_hash;
    std::vector<std::string> body_dependency_files;
    std::vector<QoreAOTBodyContractDependency> body_contract_dependencies;
};

//! Parsed contents of the optional SYMBOL_INDEX section.
struct QoreAOTSymbolIndex {
    uint16_t version = 0;
    std::vector<QoreAOTSymbolIndexRecord> defined;
    std::vector<QoreAOTSymbolIndexRecord> imported;
    std::vector<QoreAOTSymbolIndexRecord> native;
    std::vector<std::pair<std::string, std::string>> context;
};

//! Direct-call target kind written to CALL_RELOCATIONS.
enum class QoreAOTCallRelocationTargetKind : uint8_t {
    NONE          = 0,
    FUNCTION      = 1,
    METHOD        = 2,
    STATIC_METHOD = 3,
    CONSTRUCTOR   = 4,
};

//! Whether a call relocation must resolve during link.
enum class QoreAOTCallRelocationStrictness : uint8_t {
    OPTIONAL = 0,
    REQUIRED = 1,
};

//! Version of the optional CALL_RELOCATIONS section wire format.
constexpr uint16_t QORE_AOT_CALL_RELOCATIONS_VERSION = 1;

//! One direct-call relocation candidate in a compiled function.
struct QoreAOTCallRelocationRecord {
    std::string function_name;
    uint32_t expr_slot = UINT32_MAX;
    QoreAOTCallRelocationTargetKind target_kind = QoreAOTCallRelocationTargetKind::NONE;
    QoreAOTCallRelocationStrictness strictness = QoreAOTCallRelocationStrictness::OPTIONAL;
    std::string qore_path;
    std::string signature_hash;
    std::string declaration_hash;
    std::string native_symbol;
    std::string fallback_descriptor;
};

//! Parsed contents of the optional CALL_RELOCATIONS section.
struct QoreAOTCallRelocations {
    uint16_t version = 0;
    std::vector<QoreAOTCallRelocationRecord> records;
};

//! Value type tags for serialized constant values
enum class QoreAOTValueTag : uint8_t {
    VT_NOTHING    = 0,
    VT_NULL       = 1,
    VT_BOOL       = 2,
    VT_INT64      = 3,
    VT_FLOAT64    = 4,
    VT_STRING     = 5,
    VT_ABS_DATE   = 6,
    VT_REL_DATE   = 7,
    VT_LIST       = 8,
    VT_HASH       = 9,
    VT_NUMBER     = 10,
    VT_BINARY     = 11,
    //! Marks a default parameter value that is a complex expression (e.g. function call)
    //! and cannot be serialized as a constant value. Deserialized as boolean True
    //! to mark the parameter as optional in the function signature.
    VT_OPAQUE_DEFAULT = 12,
    //! Absolute date with region name for DST-aware timezone reconstruction
    VT_ABS_DATE_REGION = 13,
    //! Enum value: namespace path + member name
    VT_ENUM = 14,
    //! New object constructor call expression: class path + serialized constructor args
    //! Used for member initializers like "Mutex m()" that need runtime evaluation
    VT_NEW_OBJECT = 15,
    //! Reference to another constant by fully-qualified name.
    //! Used when a serialized value (typically an object inside a folded hash/list
    //! literal, e.g. `{"int": IntType}` where IntType is a reflection constant)
    //! shares a node pointer with another constant in the program reverse map.
    //! At load time, the value is resolved by looking up the referenced constant.
    VT_CONST_REF  = 16,
    //! Complex-type default construction expression: NewComplexListNode /
    //! NewComplexHashNode / NewComplexBufferNode / NewHashDeclNode. Encoded as kind(u8) + type path +
    //! num_args + recursive args. Used for class member initializers declared
    //! with the constructor-call form, e.g. `list<auto> elems()`.
    VT_NEW_COMPLEX_DEFAULT = 17,
    //! Serialized expression tree for computed member defaults.
    //! Used when a member initializer is an AST expression such as a function call.
    VT_EXPR_TREE = 18,
    //! Native AOT expression payload for computed member defaults.
    //! Encoded as size(u32) + one inline AOTExprKind expression.
    VT_EXPR_NATIVE = 19,
    //! Module-defined plugin value instance.
    //! Encoded as import_idx(u16) + local_type_id(u16) + serializer_format_version(u16)
    //! + reserved(u16) + payload_len(u32) + payload bytes.
    VT_PLUGIN_INSTANCE = 20,
    //! Unicode char value.
    //! Encoded as codepoint(u32).
    VT_CHAR = 21,
};

//! Optional value-container type metadata kind, present in VT_LIST/VT_HASH
//! payloads when QORE_AOT_FEAT_TYPED_VALUE_CONTAINERS is set.
enum class QoreAOTContainerValueType : uint8_t {
    Plain = 0,
    Complex = 1,
    HashDecl = 2,
};

//! Section header in the binary format
struct QoreAOTSectionHeader {
    uint16_t type;      //!< QoreAOTSectionType
    uint16_t reserved;  //!< per-section QORE_AOT_COMPRESSION_* value in sectioned metadata
    uint32_t offset;    //!< byte offset from start of data area
    uint32_t size;      //!< stored size on disk; exposed as decompressed size by the reader
};

//! Binary file header (60 bytes total, no version dispatch needed)
struct QoreAOTBinaryHeader {
    uint32_t magic;              //!< QORE_AOT_BINARY_MAGIC
    uint16_t version;            //!< QORE_AOT_BINARY_VERSION
    uint16_t flags;              //!< QORE_AOT_FLAG_*
    int64_t parse_options_lo;    //!< low 64 bits of parse options (0-63)
    uint32_t section_count;      //!< number of sections
    uint32_t label_offset;       //!< offset into string pool for source label
    uint32_t label_length;       //!< length of source label
    uint16_t max_opcode_id;      //!< maximum IR opcode ID that this binary may use
    uint8_t qore_version_major;  //!< Qore version major that compiled this binary
    uint8_t qore_version_minor;  //!< Qore version minor
    uint16_t qore_version_patch; //!< Qore version patch
    uint8_t compression;         //!< QORE_AOT_COMPRESSION_* value
    uint8_t reserved;            //!< string-pool codec for sectioned metadata; otherwise 0
    int64_t parse_options_hi;    //!< high 64 bits of parse options (64-127)
    uint64_t source_hash;        //!< xxHash64 of source file bytes (0 = not set)
    uint64_t feature_flags;      //!< QORE_AOT_FEAT_* bitset of required IR features
};

//! String pool with deduplication for efficient string storage
class QoreAOTStringPool {
    std::vector<char> data;
    std::unordered_map<std::string, uint32_t> dedup;

public:
    QoreAOTStringPool() {
        // Reserve offset 0 for empty string
        data.push_back('\0');
    }

    //! Add a string to the pool and return its offset
    /** @param str the string to add (null-terminated)
        @return offset into pool data
    */
    uint32_t add(const char* str) {
        if (!str || !*str) {
            return 0;  // empty string at offset 0
        }
        std::string key(str);
        auto it = dedup.find(key);
        if (it != dedup.end()) {
            return it->second;
        }
        uint32_t offset = static_cast<uint32_t>(data.size());
        size_t len = key.size();
        data.insert(data.end(), str, str + len + 1);  // include null terminator
        dedup[std::move(key)] = offset;
        return offset;
    }

    //! Add a string with explicit length to the pool
    /** @param str pointer to string data
        @param len length of string (not including null terminator)
        @return offset into pool data
    */
    uint32_t add(const char* str, size_t len) {
        if (!str || len == 0) {
            return 0;
        }
        std::string key(str, len);
        auto it = dedup.find(key);
        if (it != dedup.end()) {
            return it->second;
        }
        uint32_t offset = static_cast<uint32_t>(data.size());
        data.insert(data.end(), str, str + len);
        data.push_back('\0');
        dedup[std::move(key)] = offset;
        return offset;
    }

    //! Get a string from the pool by offset
    /** @param offset byte offset into pool data
        @return null-terminated string, or nullptr if offset is out of range
    */
    const char* get(uint32_t offset) const {
        if (offset >= data.size()) {
            return nullptr;
        }
        return &data[offset];
    }

    //! Get the raw pool data
    const std::vector<char>& getData() const { return data; }

    //! Get the total size of the pool
    uint32_t size() const { return static_cast<uint32_t>(data.size()); }
};

//! Binary format writer for AOT metadata
class QoreAOTBinaryWriter {
public:
    struct PluginImportRecord {
        std::string module_name;
        std::string plugin_abi_version;
        std::string operation_set_version;
        std::vector<uint16_t> required_type_ids;
        std::vector<uint16_t> required_operation_ids;
    };

    struct PluginHelperRefRecord {
        uint16_t slot_idx = 0;
        uint16_t import_idx = 0;
        uint16_t op_local_id = 0;
        uint8_t canonical_signature_version = 0;
        uint64_t signature_hash = 0;
    };

    QoreAOTStringPool strings;
    //! Optional program-wide constant reverse map, used by writeValue to encode
    //! unserializable node pointers (e.g. QoreObject inside a folded hash literal)
    //! as VT_CONST_REF entries. Set before calling section writers that serialize
    //! user constant values.
    const AOTConstantReverseMap* const_reverse_map = nullptr;

    //! Header feature flags used by payload-level compatibility choices while
    //! section writers serialize values.
    uint64_t feature_flags = 0;

    //! Fully-qualified constant currently being serialized. Used to avoid
    //! encoding self-referential VT_CONST_REF values when a top-level constant's
    //! own container node appears in the program reverse map.
    std::string current_const_path;

    //! Position of the value writeValue() is currently serializing relative to
    //! the top-level value it was called with, rendered as a subscript chain
    //! (e.g. `["java-max-heap"]["desc"]`).  Maintained by the list and hash
    //! cases so a nested failure can name the member that could not be written.
    std::string current_value_path;

    //! Describes the innermost value writeValue() could not serialize: its
    //! position within the top-level value and its type name.  Set on the first
    //! failure and left alone by outer frames, so the reported location is the
    //! offending leaf rather than the container that propagated the failure.
    std::string value_failure_detail;

    //! Constants serialized by the current AOT blob.  If set, writeValue()
    //! must not emit references to same-blob constants unless they are already
    //! available at the current section's deserialization point.
    const std::unordered_set<std::string>* current_blob_const_fqns = nullptr;

    //! Constants already available to the reader while serializing the current
    //! value.  When current_blob_const_fqns is set, this is the complete allow
    //! list for VT_CONST_REF emission from namespace-constant values; constants
    //! from later same-blob positions or sibling `.qo` fragments are not
    //! available yet and must be serialized by value.
    const std::unordered_set<std::string>* available_const_ref_fqns = nullptr;

    //! Program and module ownership context for distinguishing constants
    //! provided by dependencies from constants emitted by this artifact.
    QoreProgram* serialization_program = nullptr;
    const char* serialization_module_name = nullptr;
    const std::unordered_set<std::string>* serialization_keep_modules = nullptr;

    //! Per-blob type-path interner — when non-empty, `writeVariantSignature`
    //! emits a `u32` index into this table instead of the legacy inline
    //! string.  The TYPE_TABLE section is written at the tail of
    //! serialization (see writeTypeTableSection) and the module header has
    //! the `QORE_AOT_FEAT_TYPE_TABLE` feature bit set so readers know to
    //! use the table.  Eliminates ~3.3 M per-param hash lookups in qwf.
    std::vector<std::string> type_path_table;
    std::unordered_map<std::string, uint32_t> type_path_index;

    //! Plugin imports collected while serializing plugin-dispatch IR.
    //! Process-global operation ids are intentionally not serialized; QORD
    //! stores module-local symbolic refs that resolve against the live process
    //! registry when the artifact is loaded.
    std::vector<PluginImportRecord> plugin_imports;
    std::unordered_map<std::string, uint16_t> plugin_import_index;
    std::vector<PluginHelperRefRecord> plugin_helper_refs;

    //! Intern a type path.  Returns a u32 index that the reader dereferences
    //! against the TYPE_TABLE section.  Empty/null path gets index 0
    //! (reserved — resolves to nullptr/no-constraint).
    uint32_t internTypePath(const char* path) {
        if (!path || !*path) {
            // Reserve index 0 for "empty" so readers can treat 0 as the
            // auto/no-constraint sentinel without consulting the table.
            if (type_path_table.empty()) {
                type_path_table.emplace_back();
                type_path_index.emplace(std::string(), 0u);
            }
            return 0;
        }
        if (type_path_table.empty()) {
            type_path_table.emplace_back();
            type_path_index.emplace(std::string(), 0u);
        }
        auto it = type_path_index.find(path);
        if (it != type_path_index.end()) {
            return it->second;
        }
        uint32_t idx = static_cast<uint32_t>(type_path_table.size());
        type_path_table.emplace_back(path);
        type_path_index.emplace(type_path_table.back(), idx);
        return idx;
    }

    //! Emit the TYPE_TABLE section: u32 count, then count × StringRef.
    //! Called once after all functions/methods have been serialized so
    //! the table contains every path referenced in variant signatures.
    void writeTypeTableSection();

    //! Record a module-local plugin operation reference for later PLUGIN_*
    //! section emission.
    bool addPluginOperationRef(const char* module_name, uint16_t op_local_id,
        uint8_t canonical_signature_version, uint64_t signature_hash);

    //! Record a module-local plugin type reference for later PLUGIN_* section emission.
    bool addPluginTypeRef(const char* module_name, uint16_t local_type_id, uint16_t* import_idx = nullptr);

    //! Emit PLUGIN_IMPORTS / PLUGIN_TYPE_REGISTRY / PLUGIN_HELPER_REFS.
    bool writePluginSections(std::string& error);

private:
    std::vector<uint8_t> buffer;
    std::vector<QoreAOTSectionHeader> sections;

public:
    //! Write an unsigned 8-bit integer
    void writeU8(uint8_t v) {
        buffer.push_back(v);
    }

    //! Write an unsigned 16-bit integer (little-endian)
    void writeU16(uint16_t v) {
        buffer.push_back(static_cast<uint8_t>(v & 0xFF));
        buffer.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    }

    //! Write an unsigned 32-bit integer (little-endian)
    void writeU32(uint32_t v) {
        buffer.push_back(static_cast<uint8_t>(v & 0xFF));
        buffer.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    }

    //! Write a signed 64-bit integer (little-endian)
    void writeI64(int64_t v) {
        uint64_t uv;
        memcpy(&uv, &v, sizeof(uv));
        for (int i = 0; i < 8; ++i) {
            buffer.push_back(static_cast<uint8_t>((uv >> (i * 8)) & 0xFF));
        }
    }

    //! Write a 64-bit float (little-endian, IEEE 754)
    void writeF64(double v) {
        uint64_t bits;
        memcpy(&bits, &v, sizeof(bits));
        for (int i = 0; i < 8; ++i) {
            buffer.push_back(static_cast<uint8_t>((bits >> (i * 8)) & 0xFF));
        }
    }

    //! Write raw bytes
    void writeBytes(const void* data, uint32_t len) {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        buffer.insert(buffer.end(), p, p + len);
    }

    //! Write a string reference (adds to pool, writes offset)
    void writeStringRef(const char* str) {
        uint32_t offset = strings.add(str);
        writeU32(offset);
    }

    //! Write a string reference with explicit length
    void writeStringRef(const char* str, size_t len) {
        uint32_t offset = strings.add(str, len);
        writeU32(offset);
    }

    //! Write a QoreValue (serializes constant value)
    /** Supports: nothing, null, bool, int64, float64, string, abs_date,
        rel_date, list, hash, number, binary.
        @param v the value to serialize
        @return true on success, false if value type is unsupported
    */
    bool writeValue(const QoreValue& v);

    //! Get current write position in buffer
    uint32_t position() const { return static_cast<uint32_t>(buffer.size()); }

    //! Overwrite a U32 at a previously recorded position (for patching size fields)
    void patchU32(uint32_t pos, uint32_t v) {
        assert(pos + 4 <= buffer.size());
        buffer[pos]     = static_cast<uint8_t>(v & 0xFF);
        buffer[pos + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
        buffer[pos + 2] = static_cast<uint8_t>((v >> 16) & 0xFF);
        buffer[pos + 3] = static_cast<uint8_t>((v >> 24) & 0xFF);
    }

    //! Truncate the output buffer to a previous position.
    /** Used by speculative serializers that must roll back a partially
        emitted payload when native lowering/classification reports a gap. */
    void truncate(uint32_t pos) {
        assert(pos <= buffer.size());
        buffer.resize(pos);
    }

    //! Begin a new section
    /** @param type the section type
        @return section index
    */
    uint32_t beginSection(QoreAOTSectionType type) {
        uint32_t idx = static_cast<uint32_t>(sections.size());
        QoreAOTSectionHeader hdr;
        hdr.type = static_cast<uint16_t>(type);
        hdr.reserved = 0;
        hdr.offset = position();
        hdr.size = 0;
        sections.push_back(hdr);
        return idx;
    }

    //! End a section (records its size)
    void endSection(uint32_t idx) {
        assert(idx < sections.size());
        sections[idx].size = position() - sections[idx].offset;
    }

    //! Finalize the binary and produce the complete output
    /** @param header the binary header to write
        @param output receives the complete binary blob
        @return true on success
    */
    bool finalize(const QoreAOTBinaryHeader& header, std::vector<uint8_t>& output);
};

//! Binary format reader for AOT metadata
/** Reader instances are confined to one metadata deserialization session and
    are not thread-safe. Sectioned metadata populates lazy decompression buffers
    from getSectionData().
*/
class QoreAOTBinaryReader {
    const uint8_t* data = nullptr;
    uint32_t total_size = 0;
    const uint8_t* input_data = nullptr;
    uint32_t input_size = 0;

    // Parsed header
    QoreAOTBinaryHeader header;

    // Section directory
    std::vector<QoreAOTSectionHeader> sections;

    // String pool location
    const char* string_pool = nullptr;
    uint32_t string_pool_size = 0;

    // Data area start (after header + section directory)
    const uint8_t* data_area = nullptr;
    uint32_t data_area_size = 0;

    // Holds decompressed data if compression was used
    std::vector<uint8_t> decompressed_body;

    // Holds the eagerly decompressed shared string pool for sectioned metadata.
    std::vector<uint8_t> decompressed_string_pool;

    // Physical section sizes and lazily decompressed section payloads for
    // QORE_AOT_COMPRESSION_SECTIONED_ZSTD metadata.
    std::vector<uint32_t> section_stored_sizes;
    mutable std::vector<std::vector<uint8_t>> decompressed_sections;

public:
    //! Releases the decompressed metadata pool and invalidates the reader
    /** Whole-body compressed metadata is inflated into @ref decompressed_body at
        open() time. Sectioned metadata uses @ref decompressed_string_pool and
        lazily populated @ref decompressed_sections instead. For a large program
        these buffers are substantial, and they are only needed while the namespace
        tree, classes, functions, and slot maps are being deserialized.

        Once deserialization and function registration are complete, nothing may
        reference the pool any more: string refs that outlive it (e.g.
        QoreProgramLocation::file) are interned into the program's string pool at
        deserialization time, and the debug metadata is copied by value.  Call this
        once registration is done to return the pool to the allocator instead of
        holding it for the life of the process.

        The reader is unusable afterwards: all section pointers are cleared so that
        any use-after-release faults immediately rather than reading recycled memory.
    */
    DLLLOCAL void releaseDecompressedBody() {
        // free the buffer's storage (clear() alone would keep the capacity)
        std::vector<uint8_t>().swap(decompressed_body);
        std::vector<uint8_t>().swap(decompressed_string_pool);
        std::vector<uint32_t>().swap(section_stored_sizes);
        std::vector<std::vector<uint8_t>>().swap(decompressed_sections);
        data = nullptr;
        total_size = 0;
        input_data = nullptr;
        input_size = 0;
        data_area = nullptr;
        data_area_size = 0;
        string_pool = nullptr;
        string_pool_size = 0;
        sections.clear();
        sections.shrink_to_fit();
    }

    //! Controls the VT_CONST_REF reader return path.  When true (set by the
    //! hashdecl-member reader), VT_CONST_REF resolves to a fresh
    //! `RuntimeConstantRefNode` wrapping the ConstantEntry rather than
    //! eagerly evaluating the constant's current value.  This preserves the
    //! lazy-eval semantics that source-parse provides — crucial for hashdecl
    //! member defaults like `hash<string, hash<MapperRuntimeKeyInfo>>
    //! mapper_keys = Mapper::MapperKeyInfo;` where the referenced constant
    //! has a looser declared type (hash<auto>) than the member.  Without the
    //! wrap, parse-time folding of `<DataProviderInfo>{...}` in a downstream
    //! module hits the stricter member type via the already-resolved loose
    //! value and fails narrowing.  Mutable so readValue can be called on a
    //! const-referenced reader (common from handler paths).
    mutable bool wrap_const_ref_in_rcr = false;

    //! When true, VT_CONST_REF returns a deferred RuntimeConstantRefNode if
    //! the ConstantEntry exists but its value has not been materialized yet.
    //! Used while resolving class-constant value blobs after all constant
    //! shells have been registered.
    mutable bool defer_unresolved_const_refs = false;

    //! When non-null, VT_EXPR_NATIVE copies its payload here and returns NOTHING
    //! instead of deserializing the expression tree in place.
    /** Set while reading param defaults so that the caller can resolve the tree in a
        later phase, once every symbol the tree can reference is registered; see
        QoreAOTBinaryDeserializer::PendingNativeExprDefault.  Mutable so readValue can be
        called on a const-referenced reader (common from handler paths).
    */
    mutable std::vector<uint8_t>* expr_native_capture = nullptr;

    //! Open and validate a binary blob
    /** @param data pointer to the binary data
        @param size size of the binary data
        @param error receives error message on failure
        @return true on success, false on failure
    */
    bool open(const uint8_t* data, uint32_t size, std::string& error);

    //! Get the parsed binary header
    const QoreAOTBinaryHeader& getHeader() const { return header; }

    //! Original encoded metadata passed to open(); valid until the caller releases it.
    const uint8_t* getInputData() const { return input_data; }
    uint32_t getInputSize() const { return input_size; }

    //! Get the number of sections
    uint32_t getSectionCount() const { return static_cast<uint32_t>(sections.size()); }

    //! Get a section header by index
    const QoreAOTSectionHeader* getSection(uint32_t idx) const {
        if (idx >= sections.size()) {
            return nullptr;
        }
        return &sections[idx];
    }

    //! Find a section by type
    /** @param type the section type to find
        @return pointer to the section header, or nullptr if not found
    */
    const QoreAOTSectionHeader* findSection(QoreAOTSectionType type) const {
        for (auto& s : sections) {
            if (s.type == static_cast<uint16_t>(type)) {
                return &s;
            }
        }
        return nullptr;
    }

    //! Get a pointer to section data
    /** @param section the section header
        @return pointer to the section data, or nullptr if invalid
    */
    const uint8_t* getSectionData(const QoreAOTSectionHeader& section) const;

    //! Get a string from the string pool
    /** @param offset byte offset into the string pool
        @return null-terminated string, or nullptr if offset is out of range
    */
    const char* getString(uint32_t offset) const {
        if (!string_pool || offset >= string_pool_size) {
            return nullptr;
        }
        return string_pool + offset;
    }

    //! Get the source label from the header
    const char* getLabel() const {
        return getString(header.label_offset);
    }

    //! Read an unsigned 8-bit integer from a data pointer
    static uint8_t readU8(const uint8_t*& ptr) {
        return *ptr++;
    }

    //! Read an unsigned 16-bit integer (little-endian) from a data pointer
    static uint16_t readU16(const uint8_t*& ptr) {
        uint16_t v = static_cast<uint16_t>(ptr[0])
                   | (static_cast<uint16_t>(ptr[1]) << 8);
        ptr += 2;
        return v;
    }

    //! Read an unsigned 32-bit integer (little-endian) from a data pointer
    static uint32_t readU32(const uint8_t*& ptr) {
        uint32_t v = static_cast<uint32_t>(ptr[0])
                   | (static_cast<uint32_t>(ptr[1]) << 8)
                   | (static_cast<uint32_t>(ptr[2]) << 16)
                   | (static_cast<uint32_t>(ptr[3]) << 24);
        ptr += 4;
        return v;
    }

    //! Read a signed 64-bit integer (little-endian) from a data pointer
    static int64_t readI64(const uint8_t*& ptr) {
        uint64_t uv = 0;
        for (int i = 0; i < 8; ++i) {
            uv |= static_cast<uint64_t>(ptr[i]) << (i * 8);
        }
        ptr += 8;
        int64_t v;
        memcpy(&v, &uv, sizeof(v));
        return v;
    }

    //! Read a 64-bit float (little-endian, IEEE 754) from a data pointer
    static double readF64(const uint8_t*& ptr) {
        uint64_t bits = 0;
        for (int i = 0; i < 8; ++i) {
            bits |= static_cast<uint64_t>(ptr[i]) << (i * 8);
        }
        ptr += 8;
        double v;
        memcpy(&v, &bits, sizeof(v));
        return v;
    }

    //! Read a string reference (offset into string pool) from a data pointer
    const char* readStringRef(const uint8_t*& ptr) const {
        uint32_t offset = readU32(ptr);
        return getString(offset);
    }

    //! Read a serialized QoreValue from a data pointer
    /** @param ptr data pointer (advanced past the value)
        @param end pointer past the end of valid data
        @param error receives error message on failure
        @return the deserialized value, or NOTHING on failure
    */
    QoreValue readValue(const uint8_t*& ptr, const uint8_t* end, std::string& error) const;

    //! Check if a data pointer range is valid
    bool checkRange(const uint8_t* ptr, uint32_t needed) const {
        return ptr && data_area && ptr >= data_area
            && (ptr + needed) <= (data_area + data_area_size);
    }
};

//! Serializes a source line number in the current wire format.
/** Source line numbers were originally written as u16, which silently truncated any file with more than 65535 lines
    and made lines 32768 - 65535 deserialize as negative numbers (Qorus generates single source files well over
    32767 lines). Version 11 and later use u32; the legacy encoding is retained when reading older blobs.

    -1 (unknown location) round-trips in both encodings.
*/
static inline void qore_aot_write_line(QoreAOTBinaryWriter& writer, int line) {
    writer.writeU32(static_cast<uint32_t>(line));
}

//! Deserializes a source line number written by qore_aot_write_line()
/** @param signed_legacy if true, the legacy u16 encoding reserves 0xffff for -1 (unknown), matching the call sites
    that stored the value in an int16_t; if false, 0xffff is a line number like any other, matching the call sites
    that stored it in a uint16_t.  The distinction only affects legacy blobs; the wide encoding is always
    sign-preserving.

    Legacy blobs were written from int16_t-narrowed line numbers, so a line in 32768 - 65535 was stored with its
    high bit set.  Zero-extending recovers the original line exactly (the narrowing was mod 65536); sign-extending
    would instead yield a negative line, which is not a legal QoreProgramLineLocation value and asserts (or produces
    negative lines in stack traces) as soon as a location is built from it.  Only the 0xffff sentinel is read back
    as -1, and only for the signed call sites.
*/
static inline int qore_aot_read_line(const QoreAOTBinaryReader& reader, const uint8_t*& ptr,
        bool signed_legacy = true) {
    if (reader.getHeader().version >= 11) {
        return static_cast<int32_t>(QoreAOTBinaryReader::readU32(ptr));
    }
    uint16_t v = QoreAOTBinaryReader::readU16(ptr);
    if (signed_legacy && v == 0xffff) {
        return -1;
    }
    return static_cast<int>(v);
}

//! Clamps a deserialized source line number to a value that is legal for QoreProgramLineLocation
/** Any line below -1 is impossible in a well-formed blob; a corrupt or hand-edited artifact must not be able to
    abort the process on the QoreProgramLineLocation assertions, so such values are reported as "unknown" (0).
*/
static inline int qore_aot_valid_line(int line) {
    return line >= -1 ? line : 0;
}

//! Returns the serialized size in bytes of a single source line number in the given blob
static inline unsigned qore_aot_line_size(const QoreAOTBinaryReader& reader) {
    return reader.getHeader().version >= 11 ? 4 : 2;
}

//! Type resolver: maps type path strings back to const QoreTypeInfo* pointers at runtime
/** In batch mode (multiple AOT blobs registered into one Program),
    every session's methods reference the same builtin types (`string`,
    `*hash<auto>`, etc.) — without a shared cache, each session redoes
    the linear-scan against the 60-entry builtin table plus the
    namespace-walk for class types.  On qwf (132 sessions, ~3.3M param
    type lookups) this is ~77% of `deserializeMethods`.

    `QoreAOTBinaryMultiDeserializer` creates one cache map and hands
    its pointer to every session via `setSharedCache()`.  Each
    session's resolve() then reads/writes the same map, so each path
    is looked up at most once per batch. */
class QoreAOTTypeResolver {
    using cache_t = std::unordered_map<std::string, const QoreTypeInfo*>;

    QoreProgram* pgm;
    cache_t owned_cache;
    cache_t* cache_ptr = &owned_cache;  // default: own our own cache
    const UserSignature* signature_type_param_owner = nullptr;

public:
    explicit QoreAOTTypeResolver(QoreProgram* pgm) : pgm(pgm) {}

    //! Resolve a type path string to a QoreTypeInfo pointer
    /** @param path the type path (from QoreTypeInfo::getPath())
        @param error receives error message on failure
        @return the resolved type, or nullptr on failure
    */
    const QoreTypeInfo* resolve(const char* path, std::string& error);

    //! Resolve a type path containing callable-owned type parameters.
    /** Signature-owned type parameters cannot use the shared path cache because
        equal serialized paths belong to distinct UserSignature instances.

        @param path the serialized type path
        @param error receives an error message on failure
        @param signature the callable that owns any signature type parameters
        @return the resolved type, or nullptr on failure
    */
    const QoreTypeInfo* resolveForSignature(const char* path, std::string& error,
        const UserSignature* signature);

    //! Swap in a caller-owned cache (for cross-session sharing).
    //! The caller must keep the map alive for the resolver's lifetime.
    void setSharedCache(cache_t* shared) { cache_ptr = shared ? shared : &owned_cache; }

    //! Returns the Program used for program-local type and class resolution.
    QoreProgram* getProgram() const { return pgm; }

private:
    const QoreTypeInfo* resolveBuiltin(const char* path);
    const QoreTypeInfo* resolveClassType(const char* path);
    const QoreTypeInfo* resolveHashDeclType(const char* path);
    const QoreTypeInfo* resolveEnumType(const char* path);
    const QoreTypeInfo* resolveTypeParameterType(const char* path);
    const QoreTypeInfo* resolveUnionShorthandType(const char* path);
    const QoreTypeInfo* resolveStructuredComplexType(const char* path);
    const QoreTypeInfo* resolveComplexType(const char* path);
};

//! Serialize the namespace tree metadata into binary sections
/** Writes all namespace structure sections (NAMESPACES, CLASSES, HASHDECLS,
    ENUMS, TYPEDEFS, CONSTANTS, GLOBALS, FUNCTIONS, METHODS) into the
    binary writer. Only user-defined (non-builtin) items are serialized.

    @param writer the binary writer to write to
    @param root_ns pointer to the root namespace private data
    @param module_name optional module name; when provided, only items belonging to this module are
           serialized (items from reexported dependencies are filtered out)
    @param keep_modules optional allow-list of module names; items in those modules are always kept
    @param compile_file optional per-file filter (Phase 4 slice 4); when
           provided, only items whose AST declaration location matches
           the given file path are serialized — used for per-file
           `.qo` metadata fragments
    @param compile_files optional multi-file filter; when provided, only
           items whose AST declaration location appears in the set are
           serialized — used for aggregate script metadata
    @param shared_const_reverse_map optional immutable program-wide constant
           map to reuse instead of rebuilding it for this serialization
    @param error optional output diagnostic; set on failure with enough
           owner/source/expression context to fix the unsupported lowering
    @return true on success, false if serialization failed
*/
bool serializeNamespaceTree(QoreAOTBinaryWriter& writer, qore_ns_private* root_ns,
    const char* module_name = nullptr,
    const std::unordered_set<std::string>* keep_modules = nullptr,
    const char* compile_file = nullptr,
    std::string* error = nullptr,
    const std::unordered_set<std::string>* compile_files = nullptr,
    const AOTConstantReverseMap* shared_const_reverse_map = nullptr);

//! Serialize the optional SYMBOL_INDEX section.
/** The section is advisory metadata for build tools and future linkers. It is
    not required by the runtime loader, and absence of the section is valid for
    older `.qo` files.

    @param writer the binary writer to write to
    @param root_ns pointer to the root namespace private data
    @param module_name optional module name filter matching serializeNamespaceTree()
    @param keep_modules optional allow-list matching serializeNamespaceTree()
    @param compile_file optional single-source filter matching serializeNamespaceTree()
    @param native_symbol_map optional Qore body key -> LLVM/native symbol map
    @param init_native_symbol_map optional init function key -> LLVM/native symbol map
    @param func_slots optional compiled slot identities used to emit advisory call imports
    @param error optional output diagnostic
    @param compile_files optional multi-source filter matching serializeNamespaceTree()
    @param fast_entry_map optional resolved variant -> native fast-entry ABI/effect metadata
    @return true on success, false on cancellation or serialization failure
*/
bool serializeSymbolIndex(QoreAOTBinaryWriter& writer, qore_ns_private* root_ns,
    const char* module_name = nullptr,
    const std::unordered_set<std::string>* keep_modules = nullptr,
    const char* compile_file = nullptr,
    const std::unordered_map<std::string, std::string>* native_symbol_map = nullptr,
    const std::unordered_map<std::string, std::string>* init_native_symbol_map = nullptr,
    const std::vector<AOTCompiledFuncWithSlots>* func_slots = nullptr,
    std::string* error = nullptr,
    const std::unordered_set<std::string>* compile_files = nullptr,
    const std::unordered_map<const AbstractQoreFunctionVariant*, QoreAOTFastEntryIndexInfo>*
        fast_entry_map = nullptr,
    const QoreAOTBodyContractDependencyMap* body_contract_imports = nullptr);

//! Read the optional SYMBOL_INDEX section.
/** @return true on success or if the section is absent, false on corrupt data
*/
bool readSymbolIndex(const QoreAOTBinaryReader& reader, QoreAOTSymbolIndex& index,
    std::string& error);

//! Read the optional CALL_RELOCATIONS section.
/** @return true on success or if the section is absent, false on corrupt data
*/
bool readCallRelocations(const QoreAOTBinaryReader& reader, QoreAOTCallRelocations& relocs,
    std::string& error);

const char* qoreAOTSymbolKindName(QoreAOTSymbolKind kind);
const char* qoreAOTDependencyClassName(QoreAOTDependencyClass dependency_class);
const char* qoreAOTCallRelocationTargetKindName(QoreAOTCallRelocationTargetKind kind);

//! Serialize module dependencies into the DEPENDENCIES binary section
/** Writes all module dependencies (including reexport) so they can be loaded
    before deserializing the namespace tree in strip-source mode.

    @param writer the binary writer to write to
    @param dependencies vector of dependency module names
*/
void serializeDependencies(QoreAOTBinaryWriter& writer, const std::vector<std::string>& dependencies);

//! Read module dependencies from binary metadata
/** Reads the DEPENDENCIES section from serialized binary metadata.
    This should be called before deserializeIntoProgram() to load dependencies
    that are needed for namespace tree deserialization (e.g., base classes).

    @param data pointer to the binary metadata blob
    @param size size of the binary metadata blob
    @param dependencies receives the list of dependency module names
    @param error receives error message on failure
    @return true on success, false on failure
*/
bool readDependencies(const uint8_t* data, uint32_t size, std::vector<std::string>& dependencies, std::string& error);
bool readDependencies(const QoreAOTBinaryReader& reader, std::vector<std::string>& dependencies,
        std::string& error);

//! Collect the set of source files that contributed user declarations to the
//! program rooted at @p ns (used by the single-file `-L` preload to avoid
//! re-registering declarations the target parse already produced).
/** @param ns root namespace to walk recursively
    @param files receives the (raw, parse-recorded) source file paths
*/
void collectDeclaredSourceFiles(qore_ns_private* ns, std::unordered_set<std::string>& files);

//! Serialize reexported module names into the REEXPORT_MODULES binary section
/** Writes the list of modules that should be reexported when this module is imported.
    When a compiled module is loaded as a binary module, the reexport mechanism from
    \%requires(reexport) must be preserved so that dependency namespaces (especially
    system classes from binary modules) are made available to importing programs.

    @param writer the binary writer to write to
    @param reexport_modules vector of module names to reexport
*/
void serializeReexportModules(QoreAOTBinaryWriter& writer, const std::vector<std::string>& reexport_modules);

//! Read reexported module names from binary metadata
/** Reads the REEXPORT_MODULES section from serialized binary metadata.

    @param data pointer to the binary metadata blob
    @param size size of the binary metadata blob
    @param reexport_modules receives the list of reexported module names
    @param error receives error message on failure
    @return true on success, false on failure
*/
bool readReexportModules(const uint8_t* data, uint32_t size, std::vector<std::string>& reexport_modules, std::string& error);
bool readReexportModules(const QoreAOTBinaryReader& reader, std::vector<std::string>& reexport_modules,
        std::string& error);

//! Serialize the per-Program %prepend-module-path / %append-module-path lists
//! into MODULE_PATH_PREPEND / MODULE_PATH_APPEND sections (if non-empty).
//! Also ORs `QORE_AOT_FEAT_MODULE_PATH_LISTS` into `feature_flags` when any list has entries.
/** Wire format per section: `u32 count` followed by `count × StringRef`.
    Readers without the feature bit set see empty lists (back-compat).
    @param writer the binary writer to emit sections into
    @param prepended the expanded %prepend-module-path list (front = most-recently-added)
    @param appended the expanded %append-module-path list (insertion order)
    @param feature_flags in/out bitset; gets `QORE_AOT_FEAT_MODULE_PATH_LISTS` OR'd in when needed
*/
void serializeModulePathLists(QoreAOTBinaryWriter& writer,
        const std::vector<std::string>& prepended,
        const std::vector<std::string>& appended,
        uint64_t& feature_flags);

//! Serialized `%module-cmd(<module>) <command>` directive.
struct AOTModuleCommand {
    std::string module;
    std::string command;
};

//! Serialize `%module-cmd` directives into the MODULE_COMMANDS section.
/** Wire format: `u32 count`, then `count × (module StringRef, command StringRef)`.
    `feature_flags` is ORed with `QORE_AOT_FEAT_MODULE_COMMANDS` when commands are emitted.
*/
void serializeModuleCommands(QoreAOTBinaryWriter& writer,
        const std::vector<AOTModuleCommand>& commands,
        uint64_t& feature_flags);

//! Read serialized `%module-cmd` directives from MODULE_COMMANDS.
bool readModuleCommands(const QoreAOTBinaryReader& reader,
        std::vector<AOTModuleCommand>& commands,
        std::string& error);

//! Read serialized `%module-cmd` directives from a raw metadata blob.
bool readModuleCommands(const uint8_t* data, uint32_t size,
        std::vector<AOTModuleCommand>& commands,
        std::string& error);

//! Read the MODULE_PATH_PREPEND / MODULE_PATH_APPEND sections (if present).
/** Populates `prepended` and `appended` from the blob.  Returns true on success
    (including the case where the sections are absent — this is back-compat for
    old blobs predating QORE_AOT_FEAT_MODULE_PATH_LISTS).  On failure sets `error`
    and returns false.
*/
bool readModulePathLists(const QoreAOTBinaryReader& reader,
        std::vector<std::string>& prepended,
        std::vector<std::string>& appended,
        std::string& error);

//! data+size convenience overload of readModulePathLists
bool readModulePathLists(const uint8_t* data, uint32_t size,
        std::vector<std::string>& prepended,
        std::vector<std::string>& appended,
        std::string& error);

//! Merge the given prepended/appended path lists onto a target Program's per-Program
//! search-path lists with silent dedup.  Used by the AOT runtime to apply a blob's
//! %prepend-module-path / %append-module-path state to the freshly created Program
//! BEFORE dependency modules load.
void applyModulePathListsToProgram(QoreProgram* pgm,
        const std::vector<std::string>& prepended,
        const std::vector<std::string>& appended);

//! Serialize program-level metadata into the PROGRAM_METADATA binary section
/** Writes exec-class name and other program-level settings.
    @param writer the binary writer to write to
    @param exec_class_name the exec-class name (empty string if not set)
*/
void serializeProgramMetadata(QoreAOTBinaryWriter& writer, const char* exec_class_name);

//! Read program-level metadata from binary metadata
/** Reads the PROGRAM_METADATA section from serialized binary metadata.
    @param data pointer to the binary metadata blob
    @param size size of the binary metadata blob
    @param exec_class_name receives the exec-class name (empty if not set)
    @param error receives error message on failure
    @return true on success, false on failure (missing section is not an error)
*/
bool readProgramMetadata(const uint8_t* data, uint32_t size, std::string& exec_class_name, std::string& error);
bool readProgramMetadata(const QoreAOTBinaryReader& reader, std::string& exec_class_name, std::string& error);

//! Serialize producer/build metadata into the BUILD_INFO binary section.
/** Wire format: u32 count, then count x (StringRef key, StringRef value).
    The section is informational only; runtimes can ignore it safely.
*/
void serializeBuildInfo(QoreAOTBinaryWriter& writer,
        const std::vector<std::pair<std::string, std::string>>& info);

//! Read producer/build metadata from an opened binary reader.
bool readBuildInfo(const QoreAOTBinaryReader& reader,
        std::vector<std::pair<std::string, std::string>>& info,
        std::string& error);

//! data+size convenience overload of readBuildInfo
bool readBuildInfo(const uint8_t* data, uint32_t size,
        std::vector<std::pair<std::string, std::string>>& info,
        std::string& error);

/** Read embedded source from an AOT binary metadata blob without full deserialization.
    @param data pointer to the metadata blob
    @param size size of the metadata blob
    @param source receives the embedded source text (empty if not present)
    @param source_len receives the embedded source length
    @param error receives error message on failure
    @return true on success, false on failure
*/
bool readEmbeddedSource(const uint8_t* data, uint32_t size, const char*& source, size_t& source_len,
    std::string& error);

// ---- Slot Map Serialization (Phase 5) ----

//! Expression identity kinds for serialized slot maps
enum class AOTExprKind : uint8_t {
    UNSUPPORTED        = 0,   //!< Compile-time-only invalid marker; never serialized in new AOT output
    FUNC_CALL          = 1,   //!< Regular function call: ref1=function_name
    SELF_METHOD_CALL   = 2,   //!< Self method call: ref1=class_path, ref2=method_name
    STATIC_METHOD_CALL = 3,   //!< Static method call: ref1=class_path, ref2=method_name
    NEW_OBJECT         = 4,   //!< New object constructor: ref1=class_name
    RUNTIME_CONST_REF  = 5,   //!< Runtime constant reference: ref1=const_name
    SELF_VARREF        = 6,   //!< Self variable reference (self keyword)
    LOCAL_VARREF       = 7,   //!< Local variable reference: ref1=local_slot_index (as string)
    GLOBAL_VARREF      = 8,   //!< Global variable reference: ref1=global_slot_index or "name:<global-name>"
    CONST_NUMBER       = 9,   //!< Number constant: ref1=string representation
    CONST_BINARY       = 10,  //!< Binary constant: ref1=hex-encoded bytes
    CLOSURE_CREATE     = 11,  //!< Closure/lambda: ref1=enclosing class name (empty if none)
    CALL_REF           = 12,  //!< Call reference call: ref1=function_name (if function ref)
    OBJ_METHOD_REF     = 13,  //!< Object method reference: ref1=method_name
    STATIC_VARREF      = 14,  //!< Static class variable: ref1=class_path, ref2=var_name
    SCOPED_NEW_OBJECT  = 15,  //!< Scoped new object: ref1=class_name
    HASHDECL_NEW       = 16,  //!< Hashdecl construction: ref1=hashdecl_path
    COMPLEX_HASH_NEW   = 17,  //!< Complex hash construction: ref1=type_path
    COMPLEX_LIST_NEW   = 18,  //!< Complex list construction: ref1=type_path
    CONST_ENUM         = 19,  //!< Enum constant: ref1=enum_path, ref2=member_name
    CONST_STRING       = 20,  //!< String constant: ref1=string content
    HASH_LITERAL       = 21,  //!< Hash literal: num_pairs(u8) + [key_str(stringref) + value(AOTExprKind)] * N
    HASH_DEREF         = 22,  //!< Hash/object dereference: [type_path if QORE_AOT_FEAT_HASH_DEREF_TYPEINFO] + left(AOTExprKind) + right(AOTExprKind)
    PARSE_REF          = 23,  //!< Parse reference (\var): [type_path if QORE_AOT_FEAT_PARSE_REF_TYPE] + inner_lvalue(AOTExprKind)
    CAST_HASHDECL      = 24,  //!< Hashdecl cast: ref1=hashdecl_path, u8 or_nothing, u8 has_inner, inner?
    CAST_COMPLEX_HASH  = 25,  //!< Complex hash cast: ref1=type_path, u8 or_nothing, u8 has_inner, inner?
    CAST_COMPLEX_LIST  = 26,  //!< Complex list cast: ref1=type_path, u8 or_nothing, u8 has_inner, inner?
    CAST_CLASS         = 27,  //!< Class cast: ref1=class_path, u8 or_nothing, u8 has_inner, inner?
    CAST_ENUM          = 28,  //!< Enum cast: ref1=enum_path, u8 or_nothing, u8 has_inner, inner?
    CONST_INT          = 29,  //!< Integer constant: i64 value (8 bytes LE)
    CONST_FLOAT        = 30,  //!< Float constant: f64 value (8 bytes LE, IEEE 754)
    CONST_BOOL         = 31,  //!< Boolean constant: u8 value (0 or 1)
    CONST_NOTHING      = 32,  //!< Nothing constant: no data
    LIST_LITERAL       = 33,  //!< List literal: count(u8) + [value(AOTExprKind)] * N
    CONST_NULL         = 34,  //!< NULL constant: no data
    DOT_EVAL_TARGET    = 35,  //!< Dot-eval method target: ref1=class_path, ref2=method_name[\nvariant_class\nvariant_sig], flags=is_pseudo
    FUNC_CALL_REF      = 36,  //!< Function call reference (\func): ref1=function_name
    BOUND_METHOD_REF   = 37,  //!< Bound method reference (\method): ref1=class_path, ref2=method_name
    STATIC_METHOD_REF  = 38,  //!< Static method reference (\Class::method): ref1=class_path, ref2=method_name
    SELF_METHOD_REF    = 39,  //!< Self method reference (\self.method): ref1=method_name
    OBJ_METHOD_REF_EXPR = 40, //!< Object method reference (\obj.method): ref1=method_name + inline child expr
    CONST_VALUE        = 41,  //!< Serialized QoreValue constant: QoreAOTValueTag payload
    PLUS               = 42,  //!< Plus operator: left(AOTExprKind) + right(AOTExprKind)
    SQUARE_BRACKET     = 43,  //!< Square-bracket operator: left(AOTExprKind) + right(AOTExprKind)
    PARSE_HASH         = 44,  //!< Parse hash: count(u8) + [key(AOTExprKind) + value(AOTExprKind)] * N
    EXISTS             = 45,  //!< Exists operator: operand(AOTExprKind)
    IMPLICIT_ARG       = 46,  //!< Implicit argument reference: offset(i64), -1 for argv
    MINUS              = 47,  //!< Minus operator: left(AOTExprKind) + right(AOTExprKind)
    KEYS               = 48,  //!< Keys operator: operand(AOTExprKind)
    MULTIPLY           = 49,  //!< Multiplication operator: left(AOTExprKind) + right(AOTExprKind)
    DIVIDE             = 50,  //!< Division operator: left(AOTExprKind) + right(AOTExprKind)
    MODULO             = 51,  //!< Modulo operator: left(AOTExprKind) + right(AOTExprKind)
    IMPLICIT_ELEM      = 52,  //!< Implicit element reference ($#): no data
    INSTANCEOF         = 53,  //!< Instanceof operator: ref1=type_path + operand(AOTExprKind)
    REGEX_MATCH        = 54,  //!< Regex match operator: pattern(stringref) + options(i64, incl. QRE_GLOBAL) + operand(AOTExprKind)
    REGEX_NMATCH       = 55,  //!< Regex negative match operator: pattern(stringref) + options(i64, incl. QRE_GLOBAL) + operand(AOTExprKind)
    REGEX_EXTRACT      = 56,  //!< Regex extract operator: pattern(stringref) + options(i64, incl. QRE_GLOBAL) + operand(AOTExprKind)
    PRE_INC            = 57,  //!< Pre-increment operator: lvalue(AOTExprKind)
    PRE_DEC            = 58,  //!< Pre-decrement operator: lvalue(AOTExprKind)
    POST_INC           = 59,  //!< Post-increment operator: lvalue(AOTExprKind)
    POST_DEC           = 60,  //!< Post-decrement operator: lvalue(AOTExprKind)
    LOG_EQ             = 61,  //!< Logical equality operator: left(AOTExprKind) + right(AOTExprKind)
    LOG_NE             = 62,  //!< Logical not-equals operator: left(AOTExprKind) + right(AOTExprKind)
    LOG_NOT            = 63,  //!< Logical not operator: operand(AOTExprKind)
    TRIM               = 64,  //!< Trim operator: lvalue(AOTExprKind)
    CHOMP              = 65,  //!< Chomp operator: lvalue(AOTExprKind)
    POP                = 66,  //!< Pop operator: lvalue(AOTExprKind)
    SHIFT              = 67,  //!< Shift operator: lvalue(AOTExprKind)
    PUSH               = 68,  //!< Push operator: lvalue(AOTExprKind) + value(AOTExprKind)
    UNSHIFT            = 69,  //!< Unshift operator: lvalue(AOTExprKind) + value(AOTExprKind)
    ELEMENTS           = 70,  //!< Elements operator: operand(AOTExprKind)
    DELETE             = 71,  //!< Delete operator: lvalue(AOTExprKind)
    REMOVE             = 72,  //!< Remove operator: lvalue(AOTExprKind)
    BACKGROUND         = 73,  //!< Background operator: operand(AOTExprKind)
    CONTEXT_REF        = 74,  //!< Context member reference: member(stringref)
    CONTEXT_ROW        = 75,  //!< Current context row reference: no data
    COMPLEX_CONTEXT_REF = 76, //!< Named context member reference: name(stringref) + member(stringref) + stack_offset(i64)
    NULL_COAL          = 77,  //!< Null coalescing operator: left(AOTExprKind) + right(AOTExprKind)
    VALUE_COAL         = 78,  //!< Value coalescing operator: left(AOTExprKind) + right(AOTExprKind)
    QUESTION           = 79,  //!< Ternary operator: condition + true expression + false expression
    FOLDL              = 80,  //!< Fold-left operator: fold expression + source expression
    FOLDR              = 81,  //!< Fold-right operator: fold expression + source expression
    MAP                = 82,  //!< Map operator: map expression + source expression
    MAP_SELECT         = 83,  //!< Map-select operator: map expression + source expression + where expression
    HASH_MAP_OP        = 84,  //!< Hash map operator: key expression + value expression + source expression
    HASH_MAP_SELECT_OP = 85,  //!< Hash map-select operator: key expression + value expression + source + where
    SELECT             = 86,  //!< Select operator: source expression + select expression
    LOG_LT             = 87,  //!< Logical less-than operator: left(AOTExprKind) + right(AOTExprKind)
    LOG_GT             = 88,  //!< Logical greater-than operator: left(AOTExprKind) + right(AOTExprKind)
    LOG_LE             = 89,  //!< Logical less-than-or-equals operator: left(AOTExprKind) + right(AOTExprKind)
    LOG_GE             = 90,  //!< Logical greater-than-or-equals operator: left(AOTExprKind) + right(AOTExprKind)
    LOG_AND            = 91,  //!< Logical AND operator: left(AOTExprKind) + right(AOTExprKind)
    LOG_OR             = 92,  //!< Logical OR operator: left(AOTExprKind) + right(AOTExprKind)
    CALLREF_CALL       = 93,  //!< Call reference call: callee expression + arguments
    RANGE              = 94,  //!< Range operator: left(AOTExprKind) + right(AOTExprKind)
    ASSIGN             = 95,  //!< Assignment operator: lvalue(AOTExprKind) + value(AOTExprKind)
    CAST_SCALAR        = 96,  //!< Scalar/identity cast: ref1=type_path, u8 or_nothing, u8 has_inner, inner?
    BIT_AND            = 97,  //!< Bitwise AND operator: left(AOTExprKind) + right(AOTExprKind)
    BIT_OR             = 98,  //!< Bitwise OR operator: left(AOTExprKind) + right(AOTExprKind)
    BIT_XOR            = 99,  //!< Bitwise XOR operator: left(AOTExprKind) + right(AOTExprKind)
    SHIFT_LEFT         = 100, //!< Left shift operator: left(AOTExprKind) + right(AOTExprKind)
    SHIFT_RIGHT        = 101, //!< Right shift operator: left(AOTExprKind) + right(AOTExprKind)
    SQUARE_BRACKET_RANGE = 102, //!< Range subscript operator: source + start + stop
    DOT_EVAL_EXPR      = 103, //!< Slot wrapper for a full dot-eval expression payload
    UNARY_MINUS        = 104, //!< Unary minus operator: operand(AOTExprKind)
    LOG_AEQ            = 105, //!< Logical absolute equality operator: left(AOTExprKind) + right(AOTExprKind)
    LOG_ANE            = 106, //!< Logical absolute not-equals operator: left(AOTExprKind) + right(AOTExprKind)
    COMPLEX_BUFFER_NEW = 107, //!< Complex buffer construction: type_path + init kind in expr streams; ref1=type_path, flags=init kind in slot maps
    ITERATE            = 108, //!< Iterate operator: source expression
    STREAMING          = 109, //!< Streaming operator: kind byte + predicate/count expression + source expression
    DEFERRED_STATIC_METHOD_REF = 110, //!< Deferred static method reference: ref1=class_path, ref2=method_name
    DEFERRED_FUNCTION_REF = 111, //!< Deferred function call reference: ref1=function_name
    // Compound-assignment operators: lvalue(AOTExprKind) + value(AOTExprKind).  Native encodings so
    // these expressions round-trip without the legacy EXPR_TREE fallback (e.g. as call args).
    PLUS_EQ            = 112, //!< += operator: lvalue(AOTExprKind) + value(AOTExprKind)
    MINUS_EQ           = 113, //!< -= operator: lvalue(AOTExprKind) + value(AOTExprKind)
    MULTIPLY_EQ        = 114, //!< *= operator: lvalue(AOTExprKind) + value(AOTExprKind)
    DIVIDE_EQ          = 115, //!< /= operator: lvalue(AOTExprKind) + value(AOTExprKind)
    MODULO_EQ          = 116, //!< %= operator: lvalue(AOTExprKind) + value(AOTExprKind)
    AND_EQ             = 117, //!< &= operator: lvalue(AOTExprKind) + value(AOTExprKind)
    OR_EQ              = 118, //!< |= operator: lvalue(AOTExprKind) + value(AOTExprKind)
    XOR_EQ             = 119, //!< ^= operator: lvalue(AOTExprKind) + value(AOTExprKind)
    SHL_EQ             = 120, //!< <<= operator: lvalue(AOTExprKind) + value(AOTExprKind)
    SHR_EQ             = 121, //!< >>= operator: lvalue(AOTExprKind) + value(AOTExprKind)
    EXPR_TREE          = 0xFE, //!< Legacy recursive expression tree marker; rejected for new AOT output
    GENERIC_EVAL       = 0xFF //!< Legacy unsupported expression marker; rejected for new AOT output
};

//! Node kinds for recursive expression tree serialization (EXPR_TREE blobs)
/** Each node in the tree blob has: kind (u8), metadata (variable), num_children (u16), children (recursive).
    Metadata format varies by kind. Strings are u16-length-prefixed.
*/
enum class AOTExprNodeKind : uint8_t {
    // Leaf constants (0 children)
    EN_NOTHING       = 0,   //!< QoreValue nothing
    EN_NULL          = 1,   //!< QoreValue null
    EN_INT           = 2,   //!< i64 value
    EN_FLOAT         = 3,   //!< f64 value
    EN_STRING        = 4,   //!< u16 len + bytes
    EN_BOOL          = 5,   //!< u8 (0/1)
    EN_NUMBER        = 6,   //!< u16 len + bytes (string repr)
    EN_BINARY        = 7,   //!< u32 len + bytes

    // Leaf references (0 children)
    EN_LOCAL_VAR     = 10,  //!< u16 slot_index
    EN_GLOBAL_VAR    = 11,  //!< u16 name_len + bytes
    EN_SELF_REF      = 12,  //!< u16 name_len + bytes (member name)
    EN_STATIC_VAR    = 13,  //!< u16 class_len + bytes + u16 var_len + bytes
    EN_CONST_REF     = 14,  //!< u16 name_len + bytes (fully qualified)
    EN_CONTEXT_REF   = 15,  //!< u16 member_len + bytes (%member)
    EN_CONTEXT_ROW   = 16,  //!< whole current context row (%%)
    EN_COMPLEX_CONTEXT_REF = 17, //!< u16 ctx_len + bytes + u16 member_len + bytes + u32 stack_offset (%ctx:member)

    // Call nodes (children = args)
    EN_FUNC_CALL     = 20,  //!< u16 name_len + bytes; children = args
    EN_SELF_CALL     = 21,  //!< u16 class_len + bytes + u16 method_len + bytes; children = args
    EN_STATIC_CALL   = 22,  //!< u16 class_len + bytes + u16 method_len + bytes; children = args
    EN_DOT_EVAL      = 23,  //!< u16 method_len + bytes; children[0] = target, [1..] = args
    EN_NEW           = 24,  //!< u16 class_len + bytes; children = args
    EN_CALLREF_CALL  = 25,  //!< no metadata; children[0] = callref, [1..] = args
    EN_SCOPED_NEW    = 26,  //!< u16 class_len + bytes; children = args

    // Access operators (children = operands)
    EN_HASH_DEREF    = 30,  //!< children = [target, key_expr]
    EN_SQUARE_BRKT   = 31,  //!< children = [target, index_expr]

    // Unary operators (1 child = operand)
    EN_KEYS          = 40,  //!< no metadata
    EN_ELEMENTS      = 41,  //!< no metadata
    EN_EXISTS        = 42,  //!< no metadata
    EN_DELETE        = 43,  //!< no metadata
    EN_REMOVE        = 44,  //!< no metadata
    EN_BACKGROUND    = 45,  //!< no metadata
    EN_RESERVED_46   = 46,  //!< reserved for binary compatibility (previously EN_TYPEOF, no Qore operator)
    EN_TRIM          = 47,  //!< no metadata
    EN_CHOMP         = 48,  //!< no metadata
    EN_POP           = 49,  //!< no metadata
    EN_INSTANCEOF    = 50,  //!< u16 type_path_len + bytes
    EN_UNARY_MINUS   = 51,  //!< no metadata
    EN_UNARY_PLUS    = 52,  //!< no metadata
    EN_LOG_NOT       = 53,  //!< no metadata
    EN_BIT_NOT       = 54,  //!< no metadata
    EN_SHIFT         = 55,  //!< no metadata (shift list)

    // Binary operators (2 children = [left, right])
    EN_PUSH          = 60,  //!< no metadata
    EN_UNSHIFT       = 61,  //!< no metadata
    EN_LIST_ASSIGN   = 62,  //!< no metadata
    EN_PLUS          = 63,  //!< no metadata
    EN_MINUS         = 64,  //!< no metadata
    EN_MULTIPLY      = 65,  //!< no metadata
    EN_DIVIDE        = 66,  //!< no metadata
    EN_MODULO        = 67,  //!< no metadata
    EN_SHIFT_LEFT    = 68,  //!< no metadata
    EN_SHIFT_RIGHT   = 69,  //!< no metadata
    EN_BIT_AND       = 70,  //!< no metadata
    EN_BIT_OR        = 71,  //!< no metadata
    EN_BIT_XOR       = 72,  //!< no metadata
    EN_LOG_CMP       = 73,  //!< no metadata (<=>)
    EN_LOG_AND       = 74,  //!< no metadata (&&)
    EN_LOG_OR        = 75,  //!< no metadata (||)
    EN_LOG_EQ        = 76,  //!< no metadata (==)
    EN_LOG_NE        = 77,  //!< no metadata (!=)
    EN_LOG_AEQ       = 78,  //!< no metadata (===)
    EN_LOG_ANE       = 79,  //!< no metadata (!==)
    EN_LOG_LT        = 80,  //!< no metadata (<)
    EN_LOG_GT        = 81,  //!< no metadata (>)
    EN_LOG_LE        = 82,  //!< no metadata (<=)
    EN_LOG_GE        = 83,  //!< no metadata (>=)
    EN_NULL_COAL     = 84,  //!< no metadata (??)
    EN_VAL_COAL      = 85,  //!< no metadata (?*)
    EN_QUESTION      = 86,  //!< 3 children: [cond, true_expr, false_expr]
    EN_RANGE         = 87,  //!< 3 children: [start, stop, step]

    // Regex operators (1 child = operand)
    EN_REGEX_MATCH   = 90,  //!< u16 pattern_len + bytes + i64 options (incl. QRE_GLOBAL)
    EN_REGEX_NMATCH  = 91,  //!< u16 pattern_len + bytes + i64 options (incl. QRE_GLOBAL)
    EN_REGEX_EXTRACT = 92,  //!< u16 pattern_len + bytes + i64 options (incl. QRE_GLOBAL)
    EN_REGEX_SUBST   = 93,  //!< u16 pat_len + bytes + u16 repl_len + bytes + i64 options + u8 global
    EN_TRANSLIT      = 94,  //!< u16 src_len + bytes + u16 tgt_len + bytes

    // Special nodes
    EN_OBJ_METH_REF  = 100, //!< u16 method_len + bytes; 1 child = target expr
    EN_SELF_METH_REF = 101, //!< u16 method_len + bytes; 0 children
    EN_CLOSURE       = 102, //!< u32 expr_slot_index; 0 children — references CLOSURE_CREATE expr slot
    EN_FUNC_REF      = 103, //!< u16 name_len + bytes; 0 children (function reference)
    EN_STATIC_METH_REF = 104, //!< u16 class_len + bytes + u16 method_len + bytes; 0 children
    EN_BOUND_METH_REF  = 105, //!< u16 class_len + bytes + u16 method_len + bytes; 0 children (bound to self)

    // Assignment (2 children = [lvalue, rvalue])
    EN_ASSIGN        = 110, //!< no metadata
    EN_PLUS_EQ       = 111, //!< no metadata
    EN_MINUS_EQ      = 112, //!< no metadata
    EN_MULTIPLY_EQ   = 113, //!< no metadata
    EN_DIVIDE_EQ     = 114, //!< no metadata
    EN_MODULO_EQ     = 115, //!< no metadata
    EN_AND_EQ        = 116, //!< no metadata
    EN_OR_EQ         = 117, //!< no metadata
    EN_XOR_EQ        = 118, //!< no metadata
    EN_SHL_EQ        = 119, //!< no metadata
    EN_SHR_EQ        = 120, //!< no metadata
    EN_PRE_INC       = 121, //!< 1 child (lvalue)
    EN_PRE_DEC       = 122, //!< 1 child (lvalue)
    EN_POST_INC      = 123, //!< 1 child (lvalue)
    EN_POST_DEC      = 124, //!< 1 child (lvalue)

    // Multi-child
    EN_EXTRACT       = 130, //!< 2-4 children: [lvalue, offset, [length, [new_val]]]
    EN_SPLICE        = 131, //!< 2-4 children: same as extract
    EN_PARSE_LIST    = 132, //!< N children (internal arg list)

    // Cast
    EN_CAST          = 140, //!< u16 type_path_len + bytes + u8 or_nothing; 1 child

    // Literal collections
    EN_LIST          = 150, //!< N children (list elements)
    EN_HASH          = 151, //!< u16 num_keys; for each: u16 key_len + bytes, then 1 child (value)

    // Implicit arguments ($1, $2, $#)
    EN_IMPLICIT_ARG  = 152, //!< i16 offset (-1 = $argv, 0 = $1, 1 = $2, ...); 0 children
    EN_IMPLICIT_ELEM = 153, //!< $# (implicit element index); 0 children

    // Reference to lvalue (\var)
    EN_REF_TO_LVALUE = 154, //!< 1 child (lvalue expression)

    // Parse-time hash literal (key expressions + value expressions)
    EN_PARSE_HASH    = 156, //!< u16 num_entries; for each: 1 child (key expr) + 1 child (value expr)

    // Square brackets range (x[m..n])
    EN_SQ_BRKT_RANGE = 155, //!< 3 children: [target, start, end]

    // List processing operators
    EN_MAP           = 160, //!< 2 children: [map_expr, source]
    EN_MAP_SELECT    = 161, //!< 3 children: [map_expr, source, where_expr]
    EN_HASH_MAP      = 162, //!< 3 children: [key_expr, val_expr, source]
    EN_HASH_MAP_SELECT = 163, //!< 4 children: [key_expr, val_expr, source, where_expr]
    EN_DATE          = 8,   //!< u8 is_relative + date data; 0 children
    EN_ENUM          = 9,   //!< u16 enum_path_len + bytes + u16 member_name_len + bytes; 0 children

    EN_FOLDL         = 164, //!< 2 children: [accumulator_expr, source]
    EN_FOLDR         = 165, //!< 2 children: [accumulator_expr, source]
    EN_ITERATE       = 166, //!< 1 child: [source]
    EN_STREAMING     = 167, //!< u8 kind; 2 children: [predicate_or_count, source]
};

//! Identity for a local variable slot
struct AOTLocalSlotId {
    std::string name;        //!< variable name
    std::string type_path;   //!< type path from QoreTypeInfo::getPath()
    uint8_t flags = 0;       //!< bit 0: is_param, bit 1: is_closure, bit 2: is_self, bit 3: is_argv, bit 4: read-only
    uint16_t param_index = 0;//!< parameter index (valid only if is_param flag set)
    uint32_t body_ordinal = UINT32_MAX; //!< index in all_body_locals when this slot is a body local
    const void* local_var_ptr = nullptr; //!< compile-time only: pointer to LocalVar for identity matching
};

//! Identity for a global variable slot
struct AOTGlobalSlotId {
    std::string name;        //!< qualified variable name
    std::string type_path;   //!< type path
    bool is_thread_local = false; //!< true if thread-local variable
    bool is_aot_import = false; //!< true if this slot must resolve from the linked/loaded context
    const Var* global_var = nullptr; //!< compile-time only: declaration used for dependency attribution
};

//! Identity for an expression slot
class UserClosureFunction;

struct AOTExprSlotId {
    AOTExprKind kind = AOTExprKind::UNSUPPORTED; //!< expression kind; UNSUPPORTED is compile-time-only
    std::string ref1;        //!< kind-specific: function name or class path
    std::string ref2;        //!< kind-specific: method name (for method calls)
    std::string ref3;        //!< kind-specific: instantiated object type path for NEW_OBJECT
    uint8_t flags = 0;       //!< kind-specific flags (e.g., DOT_EVAL_TARGET: bit 0 = is_pseudo)
    QoreAOTCallRelocationTargetKind call_relocation_kind = QoreAOTCallRelocationTargetKind::NONE;
    std::string reloc_qore_path; //!< canonical symbol-index path for safe direct-call relocation
    QoreValue child_expr;   //!< kind-specific child expression (e.g., OBJ_METHOD_REF_EXPR target)
    const QoreListNode* call_args = nullptr; //!< call args for NEW_OBJECT/SCOPED_NEW_OBJECT/STATIC_METHOD_CALL
    const QoreParseListNode* parse_args = nullptr; //!< parse args for HASHDECL_NEW
    const UserClosureFunction* closure_func = nullptr; //!< For CLOSURE_CREATE: source closure function
};

//! Identity for a body local variable (needed for instantiation management)
struct AOTBodyLocalId {
    std::string name;        //!< variable name
    std::string type_path;   //!< type path
    bool is_closure = false; //!< true if closure variable
    bool read_only = false;  //!< true if read-only binding
    uint32_t slot_id = UINT32_MAX; //!< local slot id when available
};

//! Identity for a regex case slot
struct AOTRegexCaseSlotId {
    std::string pattern;     //!< regex pattern string (from re->getPatternCStr())
    int64_t options = 0;     //!< PCRE2 options (from re->getOptions())
    bool is_negated = false; //!< true for CaseNodeNegRegex (~! match)
};

//! Identity for a single step in an LValuePath instruction
struct AOTLVPathStepId {
    uint8_t kind;              //!< LVPathStepKind
    uint32_t slot_id;          //!< local/global slot for variable resolution
    std::string name;          //!< key name for HashKeyConst/SelfMember/GlobalVar etc.
    uint32_t operand_idx;      //!< dynamic operand index (UINT32_MAX if static)
    //! For HashKeySlice / ListIndexSlice steps: SSA ids for each slice
    //! sub-operand (multi-key hash slice / multi-index list slice).  Empty
    //! for non-slice step kinds.  Gated by QORE_AOT_FEAT_LVPATH_SLICE.
    std::vector<uint32_t> slice_operand_ids;
};

//! Identity for a LValuePath instruction slot
struct AOTLVPathSlotId {
    uint16_t opcode;           //!< QoreIROpcode (LValuePathAssign etc.)
    uint8_t weak;              //!< weak assignment flag
    uint8_t compound_op;       //!< LVCompoundOp
    uint8_t unary_op;          //!< LVUnaryOp
    uint8_t binary_mut_op;     //!< LVBinaryMutOp
    uint8_t ternary_op;        //!< LVTernaryOp
    uint8_t ref_rv = 1;        //!< whether the return value of the operation is used
    //! For RegexSubst / Transliterate binary_mut ops — the pattern info needed to
    //! reconstruct the QoreRegexSubst / QoreTransliteration runtime object.  Empty
    //! (pattern_empty = true) for opcodes that don't use a pattern expression.
    bool pattern_empty = true;
    std::string pattern;       //!< regex or transliteration source pattern
    std::string pattern_newstr;//!< regex substitution / transliteration replacement
    int64_t pattern_options = 0;//!< PCRE2 options bitmask (regex only)
    uint8_t pattern_global = 0;//!< global (/g) flag (regex only)
    std::vector<AOTLVPathStepId> steps;
};

//! Complete slot identity set for a single compiled function
struct AOTSlotIdentities {
    std::vector<AOTLocalSlotId> locals;   //!< indexed by local slot index
    std::vector<AOTGlobalSlotId> globals; //!< indexed by global slot index
    std::vector<AOTExprSlotId> exprs;     //!< indexed by expression slot index
    std::vector<AOTBodyLocalId> body_locals; //!< body locals in order
    std::vector<AOTRegexCaseSlotId> regex_cases; //!< indexed by regex case slot index
    std::vector<AOTLVPathSlotId> lv_path_insts;  //!< indexed by lv_path slot index
    bool has_unsupported_exprs = false;   //!< true if any expression cannot be serialized without fallback
    bool has_closure_exprs = false;       //!< true if any expression is CLOSURE_CREATE
    bool uses_argv = true;                //!< function requires the caller's implicit argv context
    bool uses_self = true;                //!< function requires the caller's implicit self context
    std::vector<std::string> unsupported_expr_details; //!< compile-time diagnostics for unsupported expression slots
};

//! Descriptor for a compiled function with slot identities
struct AOTCompiledFuncWithSlots {
    std::string name;                //!< AOT function name (e.g. "myFunc", "MyClass::method")
    std::string llvm_symbol;         //!< LLVM/native symbol name in the emitted object (for PC->loc mapping)
    int num_locals = 0;              //!< number of local variable slots
    int num_globals = 0;             //!< number of global variable slots
    int num_exprs = 0;               //!< number of expression slots
    int num_stmts = 0;               //!< number of statement slots (OnBlockExit)
    int num_regex_cases = 0;         //!< number of regex case slots (SwitchRegexMatch)
    int num_lv_path_insts = 0;       //!< number of LValuePath instruction slots
    AOTSlotIdentities slot_ids;      //!< extracted slot identities
    //! Direct constant FQN that must be ignored while serializing this function's slot payloads.
    std::string const_reverse_map_exclude_direct_fqn;
    //! Constant FQNs whose nested reverse-map paths must be ignored while serializing this function's slot payloads.
    std::vector<std::string> const_reverse_map_exclude_fqns;
    //! Optional pre-filtered reverse map for functions whose init context must preserve alternate stable paths.
    std::shared_ptr<const AOTConstantReverseMap> const_reverse_map_override;
    //! Handler IR functions for each statement slot (indexed by stmt slot index).
    //! Non-null entries have serializable handler IR; null entries are rejected before AOT output is written.
    std::vector<const QoreIRFunction*> handler_irs;
    //! Optional full function IR used when source-stripped AOT code is debugged.
    const QoreIRFunction* debug_ir = nullptr;
    //! AOT location table entry (owns the file string copy)
    struct AOTLocEntry {
        int32_t start_line = 0;
        int32_t end_line = 0;
        std::string file;
    };
    //! AOT location table indexed by slot. Populated from QoreIRToLLVM::getAOTLocTable().
    std::vector<AOTLocEntry> aot_locs;
    //! Native-PC map: sorted (function-relative native offset -> loc-index into aot_locs).
    //! Built post-emission from the object's DWARF line table (column carries the exact
    //! loc-index). Drives lazy on-throw source-location recovery (replaces the eager updater).
    std::vector<std::pair<uint32_t, uint32_t>> pc_loc_map;
    //! Literal locations referenced by pc_loc_map indices at or above aot_locs.size().
    /** LLVM inlines one AOT function into another at -O3, and the inliner copies the callee's
        DILocation (line AND column) into the caller's line table.  Since the DWARF column carries a
        loc-index that is local to the function that emitted it, an inlined row's index means nothing
        in the enclosing function's table — resolving it there yields an unrelated line.  The enclosing
        function's serialized loc table cannot be extended at this point (the metadata blob is baked
        into the module before object emission, while the PC map is derived from the emitted object),
        so such locations are carried literally in the PC map itself: index `aot_locs.size() + k`
        selects `pc_extra_locs[k]`.  Readers that predate this field simply drop those entries (their
        index is >= ctx->num_locs), losing the inlined location but never reporting a wrong one.
    */
    std::vector<AOTLocEntry> pc_extra_locs;
    //! Source-stripped metadata-only statement locations for ProgramControl::findStatementId().
    struct AOTStmtLocEntry {
        int32_t start_line = 0;
        int32_t end_line = 0;
        int64_t offset = 0;
        std::string file;
        std::string source;
    };
    std::vector<AOTStmtLocEntry> aot_stmt_locs;
};

//! AOT PC->loc trailer: appended to the END of the final loaded AOT artifact
//! (.qo/.qmod/exe) after object emission and any debug-info stripping, carrying
//! the per-function native-PC -> loc-index maps used for lazy on-throw source
//! location recovery. Independent of the metadata blob (which is baked into the
//! module before object emission, so it cannot carry post-emission native offsets).
//!
//! File layout: [payload][footer]. The fixed 16-byte footer sits at EOF:
//!   uint64 payload_len; uint32 magic('QPCM'); uint32 version
//! Payload (little-endian):
//!   uint32 num_funcs
//!   repeat: uint32 sym_len; char[sym_len] symbol;
//!           uint32 num_entries; repeat: uint32 offset; uint32 loc_index
//! Absence/garbage in the last 16 bytes simply means "no map" (graceful for
//! pre-feature artifacts) — the loader falls back to the eager location path.
static constexpr uint32_t QORE_AOT_PCMAP_MAGIC = 0x4d435051u;   //!< 'QPCM' little-endian
static constexpr uint32_t QORE_AOT_PCMAP_VERSION = 1u;
static constexpr size_t QORE_AOT_PCMAP_FOOTER_SIZE = 16;

//! One function's parsed PC->loc map (symbol-keyed).
struct AOTPcLocFuncEntry {
    std::string symbol;                                  //!< native/LLVM symbol name
    std::vector<std::pair<uint32_t, uint32_t>> entries;  //!< sorted (offset -> loc-index)
    //! Literal locations for entries whose loc-index is at or above the function's loc-table size
    //! (see AOTCompiledFuncWithSlots::pc_extra_locs): index `num_locs + k` selects `extra_locs[k]`.
    std::vector<AOTCompiledFuncWithSlots::AOTLocEntry> extra_locs;
};

//! Marks the optional extra-location block appended after the PC->loc payload's function records
/** The block is appended to the END of the payload, after every function record.  Readers that
    predate it stop after the function-record count and ignore the trailing bytes, so the payload
    stays parseable by older libqore builds with no version bump and no second section.
*/
constexpr uint32_t QORE_AOT_PCMAP_EXTRA_MAGIC = 0x4c435051u;   //!< 'QPCL' little-endian

//! Serialize the per-function PC->loc maps from func_slots into a trailer payload.
//! Returns the number of functions written (functions with an empty map or symbol
//! are skipped).
size_t qoreAOTSerializePcLocPayload(const std::vector<AOTCompiledFuncWithSlots>& func_slots,
        std::vector<uint8_t>& out);

//! Parse a trailer payload (without footer) into per-function entries. Returns false
//! on truncation/corruption.
bool qoreAOTParsePcLocPayload(const uint8_t* data, size_t len,
        std::vector<AOTPcLocFuncEntry>& out);

//! Append a PC->loc trailer (payload + footer) to the file at `path`. No-op (returns
//! true) when `payload` is empty. Sets `error` and returns false on I/O failure.
bool qoreAOTAppendPcLocTrailer(const std::string& path, const std::vector<uint8_t>& payload,
        std::string& error);

//! Read and parse the PC->loc trailer from the file at `path`. Returns false (and
//! leaves `out` empty) when the file has no valid trailer.
bool qoreAOTReadPcLocTrailer(const std::string& path, std::vector<AOTPcLocFuncEntry>& out);

//! ELF section name carrying the PC->loc map. Unlike the EOF trailer (which is
//! dropped whenever a .qo is RE-LINKED into another artifact — e.g. qorus links its
//! per-file .qo's into the qorus-core executable via the system linker), a real ELF
//! section survives every link: the linker concatenates same-named input sections
//! into the output artifact. So qcc adds this section to every object it emits and the
//! map rides through arbitrary downstream linking with ZERO changes required of qore's
//! users. Read from the loaded artifact file at throw time (the section is non-alloc /
//! not mapped, so it is read from `dli_fname`, like the trailer). C-identifier name so
//! it stays a clean section label.
#define QORE_AOT_PCLOC_SECTION_NAME "qore_aot_pcloc"

//! Mach-O equivalent of QORE_AOT_PCLOC_SECTION_NAME. Mach-O section names are
//! (segment, section) pairs, each <=16 chars; GNU objcopy corrupts Mach-O, so the
//! section is added with llvm-objcopy as "__QORE,__pcloc". The Mach-O linker
//! concatenates same-named input sections exactly like ELF, so the relink-surviving
//! contract holds identically. The framed-record payload format is the same on both.
#define QORE_AOT_PCLOC_MACHO_SEG "__QORE"
#define QORE_AOT_PCLOC_MACHO_SECT "__pcloc"

//! Frame a serialized PC->loc payload as one self-delimiting section record:
//! [uint32 magic('QPCM')][uint32 payload_len][payload]. Multiple objects' records are
//! concatenated by the linker into one section; the reader walks them sequentially.
//! No-op (leaves `out` empty) when `payload` is empty.
void qoreAOTFramePcLocSectionRecord(const std::vector<uint8_t>& payload, std::vector<uint8_t>& out);

//! Read + parse the qore_aot_pcloc section from the artifact at `path`, walking all
//! concatenated records and accumulating their per-function entries into `out`. Handles
//! both 64-bit ELF (`qore_aot_pcloc`) and 64-bit Mach-O (`__QORE,__pcloc`, thin or FAT)
//! via self-contained parses (no libLLVM dependency on the throw path's TU). Returns
//! false (and leaves `out` empty) when the artifact has no such section.
bool qoreAOTReadPcLocSection(const std::string& path, std::vector<AOTPcLocFuncEntry>& out);

//! One ELF static-symbol-table FUNC entry: link-time value + size + name.
struct AOTElfFuncSym {
    uint64_t value;   //!< st_value (link-time address / offset within the image)
    uint64_t size;    //!< st_size
    std::string name; //!< symbol name
};

//! Read all STT_FUNC symbols from the artifact's static symbol table (`.symtab`). This
//! is needed because dladdr/dladdr1 only see the DYNAMIC table (`.dynsym`): functions
//! AOT-linked into an EXECUTABLE (e.g. qorus-core) live only in `.symtab`, so dladdr
//! cannot name them and the symbol-keyed PC->loc map can't be matched without this.
//! `out_is_et_dyn` receives true for ET_DYN images (PIE exe / .so — st_value is a
//! load-relative offset, runtime addr = dli_fbase + st_value) and false for ET_EXEC
//! (st_value is absolute). Self-contained 64-bit-ELF parse. Returns false (out empty)
//! when no `.symtab`.
bool qoreAOTReadElfFuncSymbols(const std::string& path, std::vector<AOTElfFuncSym>& out,
        bool& out_is_et_dyn);

//! Mach-O counterpart of qoreAOTReadElfFuncSymbols: read __TEXT function symbols from the
//! LC_SYMTAB of a thin/FAT 64-bit Mach-O. macOS dladdr provides no symbol size, so the
//! PC->loc range would be bounded too tightly (return addresses past the last mapped
//! offset fall outside, defeating lazy resolution); this recovers per-function sizes (gap
//! to the next function symbol). `value` is the offset from the image base and `out_is_pie`
//! is always true, so the runtime applies bias = dli_fbase exactly as for ELF ET_DYN. Names
//! have the Mach-O leading '_' stripped to match the dladdr/dli_sname spelling.
bool qoreAOTReadMachoFuncSymbols(const std::string& path, std::vector<AOTElfFuncSym>& out,
        bool& out_is_pie);

//! Descriptor for a compiled constant/static-var init function
struct AOTCompiledInitFunc {
    std::string name;               //!< init function name (e.g. "__const_init::Ns::ConstName")
    std::string llvm_symbol;        //!< LLVM symbol name in the module
    int num_locals = 0;
    int num_globals = 0;
    int num_exprs = 0;
    int num_stmts = 0;
    int num_regex_cases = 0;
    int num_lv_path_insts = 0;
    AOTSlotIdentities slot_ids;
    //! Direct constant FQN that must be ignored while serializing this init function's slot payloads.
    std::string const_reverse_map_exclude_direct_fqn;
    //! Constant FQNs whose nested reverse-map paths must be ignored while serializing this init function's slot payloads.
    std::vector<std::string> const_reverse_map_exclude_fqns;
    //! Pre-filtered reverse map used for serializing this init function's slot payloads.
    std::shared_ptr<const AOTConstantReverseMap> const_reverse_map_override;
    uint64_t feature_flags = 0;
    uint64_t source_order = 0;      //!< declaration order for ordered global initializers

    //! Target type for the init function result
    enum TargetType : uint8_t {
        NS_CONSTANT = 0,      //!< namespace-level constant
        CLASS_CONSTANT = 1,   //!< class-level constant
        STATIC_VAR = 2,       //!< static class variable
        MODULE_INIT = 3,      //!< module init closure body (side-effects only, return discarded)
        OUTLINED_HELPER = 4,  //!< outlined init-expression helper (Phase 1.5);
                              //!< LLVM-lowered but NOT executed at module load —
                              //!< its outer init calls it as a helper instead.
                              //!< Slot IDs still populate so the helper can
                              //!< resolve globals / constants it references.
        GLOBAL_VAR = 5,       //!< namespace-level global or thread-local variable
        GLOBAL_VAR_CONSTRUCT = 6, //!< initializer expression constructs and stores the value
    };
    TargetType target_type = NS_CONSTANT;
    std::string ns_path;            //!< namespace path or class path
    std::string item_name;          //!< constant or variable name

    //! Names of other init functions this one depends on (for topological sort)
    std::vector<std::string> deps;
};

//! Serialize slot maps for compiled functions into the SLOT_MAPS binary section
/** @param writer the binary writer to write to
    @param funcs vector of compiled function descriptors with slot identities
    @param const_reverse_map optional reverse map for constant node → FQN resolution
*/
bool serializeSlotMaps(QoreAOTBinaryWriter& writer, const std::vector<AOTCompiledFuncWithSlots>& funcs,
    const AOTConstantReverseMap* const_reverse_map, std::string& error);

//! Serialize embedded source into the FUNC_SOURCES binary section
/** Current AOT compilation rejects functions that would require source fallback.
    The section is written only for explicit source embedding, and the legacy
    fallback function-name list must remain empty for newly generated objects.

    @param writer the binary writer to write to
    @param funcs vector of compiled function descriptors with slot identities
    @param source_text the full source text to embed
    @param source_len the length of the source text
*/
void serializeEmbeddedSource(QoreAOTBinaryWriter& writer,
    const std::vector<AOTCompiledFuncWithSlots>& funcs,
    const char* source_text, int source_len);

//! Serialize init function descriptors into the INIT_FUNCS binary section
/** Each entry maps an init function name to its target (namespace constant,
    class constant, or static variable). The init functions themselves are
    registered via the SLOT_MAPS section like regular AOT functions.
*/
void serializeInitFuncs(QoreAOTBinaryWriter& writer,
    const std::vector<AOTCompiledInitFunc>& init_funcs);

//! Descriptor for a deserialized init function (read from INIT_FUNCS section)
struct AOTInitFuncDescriptor {
    std::string name;                               //!< init function name (matches QoreAOTFunc::name)
    AOTCompiledInitFunc::TargetType target_type;    //!< what the init function initializes
    std::string ns_path;                            //!< namespace path or class path
    std::string item_name;                          //!< constant or variable name
};

//! Read init function descriptors from binary metadata
/** Reads the INIT_FUNCS section from serialized binary metadata.
    Returns the list of init function descriptors that map init function names
    to their target constants/static vars.

    @param data pointer to the binary metadata blob
    @param size size of the binary metadata blob
    @param init_funcs receives the list of init function descriptors
    @param error receives error message on failure
    @return true on success (even if section is absent), false on failure
*/
bool readInitFuncs(const uint8_t* data, uint32_t size,
    std::vector<AOTInitFuncDescriptor>& init_funcs, std::string& error);
bool readInitFuncs(const QoreAOTBinaryReader& reader,
    std::vector<AOTInitFuncDescriptor>& init_funcs, std::string& error);

// ---- Namespace Deserialization (Phase 4) ----

class QoreClass;

//! Deserializer: reconstructs namespace tree from binary metadata (replaces parse())
/** Reads the binary metadata sections and creates namespace tree elements
    within an existing QoreProgram, including:
    - Namespace hierarchy
    - Classes with members, base classes, and constants
    - Hashdecls, enums, typedefs
    - Constants and global variables
    - Functions with proper UserSignature (no bodies)
    - Methods on classes with proper UserSignature (no bodies)
*/
class UserConstructorVariant;

//! Non-owning view of a serialized value in an open AOT metadata reader.
struct QoreAOTDeferredValueBlob {
    const uint8_t* ptr = nullptr;
    size_t len = 0;

    bool empty() const { return !len; }
    const uint8_t* data() const { return ptr; }
    size_t size() const { return len; }
    void assign(const uint8_t* data, size_t size) {
        ptr = data;
        len = size;
    }
    void clear() {
        ptr = nullptr;
        len = 0;
    }
};

//! Null-terminated string view backed by an open AOT metadata reader.
struct QoreAOTStringRef {
    const char* ptr = "";

    QoreAOTStringRef() = default;
    QoreAOTStringRef(const char* value) : ptr(value ? value : "") {
    }
    QoreAOTStringRef& operator=(const char* value) {
        ptr = value ? value : "";
        return *this;
    }
    bool empty() const { return !*ptr; }
    const char* c_str() const { return ptr; }
};

class QoreAOTBinaryDeserializer {
    QoreAOTBinaryReader reader;
    QoreAOTTypeResolver* type_resolver = nullptr;
    QoreProgram* pgm = nullptr;

    // Index maps: serialized index → created object
    std::vector<qore_ns_private*> ns_list;
    std::vector<QoreClass*> class_list;
    std::vector<std::string> class_signature_hashes;
    std::vector<QoreAOTStringRef> class_injected_paths;

    // Exact native slot key -> variant bindings populated while functions and
    // methods are deserialized.  Slot registration can consume these pointers
    // directly instead of walking the namespace/class tree and rebuilding each
    // variant signature a second time.  Only variants that have SLOT_MAPS
    // entries are retained, and the map lives no longer than this session.
    struct SlotVariantBinding {
        UserVariantBase* variant = nullptr;
        const qore_class_private* class_ctx = nullptr;
    };
    // Views borrow immutable names from reader's string pool and remain valid
    // through slot registration, when these lookup tables are consumed.
    std::unordered_set<std::string_view> slot_map_names;
    bool has_slot_map_section = false;
    bool cache_slot_variant_bindings = true;
    std::unordered_map<std::string_view, SlotVariantBinding> slot_variant_bindings;

    //! Batch-wide class lookup map installed by QoreAOTBinaryMultiDeserializer.
    //! Pending defaults can reference classes from sibling .qo sessions; the
    //! per-session class_list map is not sufficient for those references.
    const std::unordered_map<std::string, QoreClass*>* batch_class_lookup_map = nullptr;

    // Embedded source data (from FUNC_SOURCES section)
    const char* embedded_source = nullptr;       //!< embedded source text
    size_t embedded_source_len = 0;              //!< length of embedded source text
    std::vector<std::string> fallback_func_names; //!< legacy names of functions needing source fallback

    // Classes that already existed in the program (from module loading)
    // — skip methods/members for these since they're already committed
    std::unordered_set<uint32_t> preexisting_classes;

    // Pending base class info for two-pass class resolution
    struct PendingBaseClass {
        QoreAOTStringRef base_path;
        QoreAOTStringRef type_path;
        uint8_t access;  //!< ClassAccess value for the base class inheritance
        bool is_virtual;
    };
    std::vector<std::vector<PendingBaseClass>> pending_bases;

    //! Topological order for class processing (bases before derived)
    //! Computed in resolveClassBases(), reused in commitDeserializedClasses()
    std::vector<uint32_t> topo_order;

    // Pending instance member info for two-pass class resolution
    struct PendingInstanceMember {
        QoreAOTStringRef name;
        QoreAOTStringRef type_path;
        uint8_t access;
        uint8_t flags = 0;  // bit 0 = transient
        QoreValue default_val;
        //! When set, the member init is a `Class(args)` call whose target
        //! class is a forward reference to a class that has not yet been
        //! registered. Resolved into a ScopedObjectCallNode and installed
        //! into default_val in the second pass, after all classes exist.
        std::string pending_new_class_path;
        //! Evaluated constructor args for pending_new_class_path (owned).
        std::vector<QoreValue> pending_new_args;
        //! When set, the member init references an enum member that was not
        //! yet deserialized at class-read time (enums are deserialized after
        //! classes). Resolved into the enum member value in the second pass.
        std::string pending_enum_path;
        std::string pending_enum_member;
        //! When set, the member init is a complex-type default constructor
        //! (e.g. `hash<ComponentInfo>()`, `hash<string, T>()`, `list<T>()`)
        //! that references a type not yet registered at class-read time.
        //! Resolved in the second pass after all types exist.
        //! kind: 0=complex list, 1=complex hash, 2=hashdecl, 3=complex buffer
        int8_t pending_complex_default_kind = -1;
        int8_t pending_complex_buffer_init_kind = 0;
        std::string pending_complex_default_path;
        std::vector<QoreValue> pending_complex_default_args;
        //! Deferred VT_EXPR_TREE default.  Expression trees can reference
        //! class/namespace constants that are not registered while class shells
        //! are being read, so they are materialized after constants are added.
        std::vector<uint8_t> pending_expr_tree_blob;
        //! Deferred VT_CONST_REF default. Class/static member records are read
        //! before same-class constants are registered, so constant refs must
        //! be resolved during the member-resolution phase.
        std::string pending_const_ref_path;
        //! Deferred VT_EXPR_NATIVE default.  The payload uses string-pool refs,
        //! so it is materialized later with the owning binary reader.
        std::vector<uint8_t> pending_expr_native_blob;
        //! Serialized default payload.  Member records are read while only
        //! declaration shells exist; nested const refs inside values must be
        //! materialized after the batch-wide constant registration phase.
        QoreAOTDeferredValueBlob value_blob;
    };
    std::vector<std::vector<PendingInstanceMember>> pending_instance_members;

    // Pending static member info for two-pass class resolution
    struct PendingStaticMember {
        QoreAOTStringRef name;
        QoreAOTStringRef type_path;
        uint8_t access;
        QoreValue default_val;
        //! Same deferred-new-object channel as PendingInstanceMember.
        std::string pending_new_class_path;
        std::vector<QoreValue> pending_new_args;
        //! Same deferred-enum channel as PendingInstanceMember.
        std::string pending_enum_path;
        std::string pending_enum_member;
        //! Same deferred-complex-default channel as PendingInstanceMember.
        int8_t pending_complex_default_kind = -1;
        int8_t pending_complex_buffer_init_kind = 0;
        std::string pending_complex_default_path;
        std::vector<QoreValue> pending_complex_default_args;
        //! Same deferred-expression-tree channel as PendingInstanceMember.
        std::vector<uint8_t> pending_expr_tree_blob;
        //! Same deferred-constant-ref channel as PendingInstanceMember.
        std::string pending_const_ref_path;
        //! Same deferred-native-expression channel as PendingInstanceMember.
        std::vector<uint8_t> pending_expr_native_blob;
        //! Same serialized-default channel as PendingInstanceMember.
        QoreAOTDeferredValueBlob value_blob;
    };
    std::vector<std::vector<PendingStaticMember>> pending_static_members;

    // Pending class constant info for two-pass class resolution
    struct PendingClassConstant {
        QoreAOTStringRef name;
        QoreAOTStringRef type_path;
        uint8_t access;
        bool pending_init = false;  //!< init-func has not yet populated the value
        //! Serialized value payload. Class constants are registered as shells
        //! before this blob is deserialized so nested VT_CONST_REF entries can
        //! resolve against same-class or same-module constants.
        QoreAOTDeferredValueBlob value_blob;
    };
    std::vector<std::vector<PendingClassConstant>> pending_class_constants;

    // Pending hashdecl member info for two-pass resolution.
    //
    // The deferred-resolution fields below mirror PendingInstanceMember so
    // the shared `readDeferredMemberDefault` template can be instantiated
    // against this struct.  Hashdecls are deserialized BEFORE enums
    // AND before all classes/hashdecls in the module are committed
    // (see openAndDeserializeShells ordering), so member defaults that
    // reference an enum value, a class constructor, or a complex-type
    // default (`hash<X>()`, `list<X>()`, `hash<string, X>()`) need to
    // wait until resolveHashdeclMembers to produce a final QoreValue.
    struct PendingHashdeclMember {
        QoreAOTStringRef name;
        QoreAOTStringRef type_path;
        QoreValue default_val;

        // Deferred VT_ENUM: pending_enum_path::pending_enum_member.
        // Resolved via QoreProgram::findEnum + QoreEnumDecl::findMember.
        std::string pending_enum_path;
        std::string pending_enum_member;

        // Deferred VT_NEW_OBJECT: the member init was `Class(args)` where
        // Class was not yet registered at hashdecl-read time.  Resolved
        // into a ScopedObjectCallNode after all classes exist.
        std::string pending_new_class_path;
        std::vector<QoreValue> pending_new_args;

        // Deferred VT_NEW_COMPLEX_DEFAULT: kind 0=complex list,
        // 1=complex hash, 2=hashdecl, 3=complex buffer. path is the element/value/hashdecl
        // type path; args are the constructor args (owned).
        int8_t pending_complex_default_kind = -1;
        int8_t pending_complex_buffer_init_kind = 0;
        std::string pending_complex_default_path;
        std::vector<QoreValue> pending_complex_default_args;

        // Deferred VT_EXPR_TREE member default.
        std::vector<uint8_t> pending_expr_tree_blob;
        // Deferred VT_CONST_REF member default.
        std::string pending_const_ref_path;
        // Deferred VT_EXPR_NATIVE member default.
        std::vector<uint8_t> pending_expr_native_blob;
    };
    // Map from hashdecl pointer to pending members
    std::vector<std::pair<TypedHashDecl*, std::vector<PendingHashdeclMember>>> pending_hashdecl_members;

    // Pending typedef info for two-pass resolution
    struct PendingTypedef {
        QoreAOTStringRef name;
        QoreAOTStringRef type_path;
        uint32_t ns_idx;
        bool is_pub;
    };
    std::vector<PendingTypedef> pending_typedefs;

    // Pending enum base type info for two-pass resolution
    struct PendingEnumBaseType {
        QoreEnumDecl* ed;
        QoreAOTStringRef base_type_path;
    };
    std::vector<PendingEnumBaseType> pending_enum_base_types;

    //! Pending BCA arg blob for deferred deserialization.
    //! New blobs carry native inline AOT expressions; old blobs carry legacy
    //! EXPR_TREE. Both can reference static methods of the same class that
    //! have not been added yet during method deserialization, so resolution is
    //! deferred until after all methods are committed.
    struct PendingBCAArgBlob {
        const uint8_t* data;
        uint32_t size;
    };
    struct PendingBCAEntry {
        qore_classid_t classid;
        std::string base_path;
        int32_t start_line = 0;
        int32_t end_line = 0;
        size_t eval_result_size = 0;
        std::vector<size_t> source_to_param;
        std::vector<PendingBCAArgBlob> arg_blobs;
    };
    struct PendingBCA {
        QoreClass* qc;
        UserConstructorVariant* ucv;
        std::vector<LocalVar*> local_vars;
        std::vector<PendingBCAEntry> entries;
    };
    std::vector<PendingBCA> pending_bcas;

public:
    // Static-method default-arg fixups for params like
    //   `string b = MultiPartMessage::getBoundary()`
    // The referenced static method is not in the vlist yet during
    // function/method deserialization, so the ref is captured here
    // and resolved in finalize() after commitClasses().
    struct PendingStaticMethodDefault {
        std::string class_path;
        std::string method_name;
        UserVariantBase* uvb = nullptr;
        uint32_t param_index = 0;
    };

    //! Native-expression default-arg fixups for params whose default is a general
    //! expression tree (`VT_EXPR_NATIVE`), e.g.
    //!   `int sub f(int d = getDelay().durationSeconds())`
    /** These trees can reference any symbol in the module — including other functions in
        the same module.  Functions are added to their namespace only after their own
        variants have been read (see `deserializeFunctions`), and the FUNCTIONS section is
        written in `func_list` hash order, so whether a referenced sibling is already
        registered when the default is read is not deterministic.  Capturing the blob here
        and resolving it in `finalizePostIndex()` — after every function, method and class
        is registered and indexed — removes the ordering dependency entirely.
    */
    struct PendingNativeExprDefault {
        std::vector<uint8_t> blob;
        UserVariantBase* uvb = nullptr;
        uint32_t param_index = 0;
    };

private:
    std::vector<PendingStaticMethodDefault> pending_smd;
    std::vector<PendingNativeExprDefault> pending_ned;

    //! Legacy owned copies used only for deterministic borrowed-blob A/B tests.
    //! The pointed-to vector buffers remain stable when the outer vector grows.
    std::unique_ptr<std::vector<std::vector<uint8_t>>> deferred_value_copies;
    //! Legacy owned strings used only for deterministic reader-string A/B tests.
    std::unique_ptr<std::deque<std::string>> deferred_string_copies;

    //! Pre-resolved per-blob type table.  Populated at the start of
    //! phase 2b (deserializeFunctionsAndMethods) by reading the
    //! TYPE_TABLE section and resolving every entry via `type_resolver`.
    //! When non-empty, `readAndSetupVariantSignature` pulls return /
    //! param types by index instead of via per-param hash-lookup —
    //! cuts ~3.3 M resolver calls on qwf's 656 k variants.  Empty when
    //! loading a pre-feature-flag .qmod or a blob with no variants.
    std::vector<const QoreTypeInfo*> type_table_resolved;
    //! Non-owning raw paths into reader storage for signature-owned generic types.
    std::vector<const char*> type_table_paths;

    //! Set when the blob's header advertises QORE_AOT_FEAT_TYPE_TABLE.
    //! Signatures emit a u32 index rather than an inline string for
    //! return + param types.  Decided at `openAndDeserializeShells`
    //! time from the parsed header.
    bool uses_type_table = false;

    //! True after deserializeGlobals() has run successfully.  Globals are
    //! created before function/method signature deserialization so native
    //! default arguments can resolve cross-fragment globals; the older later
    //! phase still calls resolveStaticsAndConstants(), so it must be
    //! idempotent.
    bool globals_deserialized = false;

    //! Resolve every entry in the TYPE_TABLE section into
    //! type_table_resolved.  No-op when the section is absent.  Must
    //! run after shells across all sibling sessions exist so
    //! cross-blob complex types resolve correctly.
    bool resolveTypeTable(std::string& error);
    bool resolvePluginImports(std::string& error);
    QoreAOTStringRef makeDeferredStringRef(const char* value);
    bool readDeferredValueBlob(const uint8_t*& ptr, const uint8_t* end,
        std::string& error, QoreAOTDeferredValueBlob& value_blob);

    bool deserializeNamespaces(std::string& error);
    bool deserializeClasses(std::string& error);
    bool resolveClassBases(std::string& error);
    bool resolveInstanceMembers(std::string& error);
    bool importInheritedMembers(std::string& error);
    bool resolveStaticMembers(std::string& error);
    bool registerClassConstantShells(std::string& error);
    bool resolveClassConstantValues(std::string& error);
    bool resolveNamespaceConstants(std::string& error);
    bool resolveClassConstants(std::string& error);
    bool resolveHashdeclMembers(std::string& error);
    bool resolveTypedefs(std::string& error);
    bool resolveEnumBaseTypes(std::string& error);
    bool resolveBCAExpressions(std::string& error);

    //! resolves deferred general expression-tree param defaults; see PendingNativeExprDefault
    bool resolveNativeExprDefaults(std::string& error);
    bool deserializeHashDecls(std::string& error);
    bool deserializeEnums(std::string& error);
    bool deserializeTypedefs(std::string& error);
    bool deserializeConstants(std::string& error);
    bool deserializeGlobals(std::string& error);
    bool deserializeFunctions(std::string& error);
    bool deserializeMethods(std::string& error);
    bool deserializeEmbeddedSource(std::string& error);
    bool commitDeserializedClasses(std::string& error);
    const QoreClass* resolveClassRefForSession(const char* class_ref,
        const std::unordered_map<std::string, QoreClass*>* local_class_map = nullptr,
        bool pseudo = false) const;
    const QoreProgramLocation* getBlobLocation(int32_t start_line = 0, int32_t end_line = 0) const;
    bool deserializeShellsFromOpenReader(std::string& error);

public:
    //! Phase 4 slice 10: phase-1 entry point — open blob, create type
    //! resolver, create ONLY the shells (namespaces, classes,
    //! hashdecls, enums, typedefs).  Does NOT run any resolution
    //! passes.  After calling this on N blobs against the same
    //! program, call resolveAll() on each session to finish.
    /** Used by `QoreAOTBinaryMultiDeserializer` to enable cross-blob
        reference resolution: all shells across all input blobs land
        in the program's namespace tree before any resolution pass
        runs, so pgm->findClass cross-references resolve regardless
        of blob order.
        @param in_pgm the target program
        @param data blob bytes
        @param size blob byte count
        @param error error string on failure
        @return true on success
    */
    bool openAndDeserializeShells(QoreProgram* in_pgm, const uint8_t* data,
            uint32_t size, std::string& error);
    bool openAndDeserializeShells(QoreProgram* in_pgm, QoreAOTBinaryReader&& open_reader,
            std::string& error);

    //! Swap in a caller-owned type-cache map so this session's
    //! resolver shares lookup results with sibling sessions.
    //! Must be called after openAndDeserializeShells() but before
    //! any resolve() call — typically right after addBlob in the
    //! MultiDeserializer.  The caller owns the map and must keep it
    //! alive for the session's lifetime.
    void setSharedTypeCache(std::unordered_map<std::string, const QoreTypeInfo*>* shared) {
        if (type_resolver) {
            type_resolver->setSharedCache(shared);
        }
    }

    /** Return the exact deserialized variant for a native SLOT_MAPS key.
        @param key the exact native slot key
        @param class_ctx receives the owning class for method variants, or nullptr for functions
        @return the matching variant, or nullptr if no exact binding was cached
    */
    UserVariantBase* findSlotMapVariant(const char* key,
            const qore_class_private*& class_ctx) const {
        class_ctx = nullptr;
        if (!key) {
            return nullptr;
        }
        auto i = slot_variant_bindings.find(std::string_view(key));
        if (i == slot_variant_bindings.end()) {
            return nullptr;
        }
        class_ctx = i->second.class_ctx;
        return i->second.variant;
    }

    //! Add this session's class shells to a caller-owned lookup map, including
    //! anchored/unanchored aliases used by serialized class references.
    void appendClassesToLookupMap(std::unordered_map<std::string, QoreClass*>& map) const;

    //! Install/clear a caller-owned batch class map for the duration of a
    //! multi-blob resolution pass.
    void setBatchClassLookupMap(const std::unordered_map<std::string, QoreClass*>* map) {
        batch_class_lookup_map = map;
    }

    //! Phase 4 slice 10: phase-2 entry point — run all resolution
    //! passes on a session previously opened via
    //! openAndDeserializeShells.  Must be called after every session
    //! in a multi-blob batch has completed its shells-phase, so
    //! pgm->findClass can find cross-blob declarations.
    /** For single-blob callers, this is the complete phase 2.
        For multi-blob batch callers, prefer the phase-split helpers
        below; they let the MultiDeserializer interleave sub-phases
        across sessions so a derived class's parseCommit can't fire
        before its base class's methods have been deserialized in a
        sibling session. */
    bool resolveAll(std::string& error);

    //! Phase-split 2a-1 — resolve base classes and type-only metadata.
    bool resolveTypes(std::string& error);

    //! Phase-split 2a-2a — register class-constant shells only.
    /** In batch mode this must run across all sessions before class-constant
        value blobs are materialized, because class constants can reference
        constants declared in sibling fragments. */
    bool registerClassConstantShellsPhase(std::string& error) {
        return registerClassConstantShells(error);
    }

    //! Phase-split 2a-2b — register namespace constants.
    /** Must run after class-constant shells are registered so namespace
        constants can preserve references to class constants as runtime refs. */
    bool resolveNamespaceConstantsPhase(std::string& error) {
        return resolveNamespaceConstants(error);
    }

    //! Phase-split 2a-2c — materialize class-constant values.
    /** Must run after class-constant shells and namespace constants are
        registered across all sessions. */
    bool resolveClassConstantValuesPhase(std::string& error) {
        return resolveClassConstantValues(error);
    }

    //! Phase-split 2a-2 — register class and namespace constants.
    /** Must run across all sessions after resolveTypes() and before
        resolveMembers(), because member default expressions can reference
        constants from any sibling AOT blob in the batch. */
    bool resolveConstants(std::string& error);

    //! Phase-split 2a-2b — register class static members.
    /** Must run after resolveConstants() and before resolveMembers(), because
        instance-member default expressions can reference class static members. */
    bool resolveStaticMembersPhase(std::string& error);

    //! Phase-split 2a-post — top-level globals.
    /** Must run before function/method deserialization because variant
        signatures can contain native default expressions that reference
        globals declared in a sibling script fragment.  Idempotent after
        the first successful call. */
    bool resolveStaticsAndConstants(std::string& error);

    //! Phase-split 2a-3 — resolve this session's OWN instance members.
    /** Must run after resolveConstants() and resolveStaticMembersPhase() so
        expression-tree member defaults can resolve constants and static vars,
        and after deserializeFunctionsAndMethods() so defaults can call class
        static methods. */
    bool resolveMembers(std::string& error);

    //! Phase-split 2a compatibility entry point — resolve base classes, types,
    //! constants, static members, functions/methods, and this session's OWN
    //! members (no inherited imports yet).
    /** Must run before importInheritedMembersPhase() on ANY
        session, because that phase reads members from base
        classes that may live in sibling sessions. */
    bool resolveTypesAndMembers(std::string& error);

    //! Phase-split 2a-sml — re-propagate the super-class map list
    //! (`scl->sml`) on each class in this session, using the
    //! now-fully-populated base-class scls from sibling sessions.
    /** During `resolveClassBases`, `addBaseClass` walks the base's
        `scl->sml` to propagate grandparents into the derived
        class's sml.  If the base is owned by a sibling session
        whose `resolveClassBases` hasn't run yet, its sml is
        incomplete and grandparents never reach the derived
        class — so `processMemberInitializationList` later emits
        an empty `member_init_list` for the grandparents' local
        members.  This phase re-invokes
        `BCSMList::addBaseClassesToSubclass` (which is idempotent
        via duplicate-ID skip in `BCSMList::add`) once every
        session's bases are attached, completing the cross-session
        sml. */
    bool rebuildBaseClassSmlPhase(std::string& error);

    //! Phase-split 2a-import — copy base-class members into derived
    //! classes.  Batch mode: must wait until every session has
    //! finished resolveMembers() so base classes' member maps are
    //! populated. */
    bool importInheritedMembersPhase(std::string& error);

    //! Phase-split 2a-2c — deserialize functions and methods.
    /** Adds method variants to every class's pending method map
        (hm/shm).  No parseCommit fires here.  Must run after
        resolveTypes(), resolveConstants(), resolveStaticMembersPhase(), and
        resolveStaticsAndConstants(), and before resolveMembers() so member
        defaults can resolve static calls. */
    bool deserializeFunctionsAndMethods(std::string& error);

    //! Phase-split 2c — commit all newly deserialized classes.
    /** Calls parseCommit on each class in this session's class_list,
        which binds priv->constructor via checkAssignSpecial and moves
        pending method variants into the committed vlist.  Relies on
        every class's method map being populated — in batch mode, the
        MultiDeserializer runs 2b across ALL sessions before running
        2c on any session, so parseCommit's recursive base-class walk
        cannot finalize a sibling-session class before its methods
        were added. */
    bool commitClasses(std::string& error);

    //! Sub-phases of commitClasses, exposed so the MultiDeserializer
    //! can interleave across sessions.
    /** Order: prepare → doCommit → importAbstract → resolveAbstract → validate.
        - prepare: set initialized + has_new_user_changes,
          parseAddAncestors on each method.  No parseCommit.
        - doCommit: parseCommit on each class in topo order.
        - importAbstract: lift parent abstract methods into ahm
          where derived classes don't override them.  Runs after
          doCommit because it checks the committed vlist.
        - resolveAbstract: for each imported abstract, search sibling
          parent classes for a concrete override.  This mirrors the
          source-parse path's `qore_class_private::parseResolveAbstract()`
          which the AOT deserializer bypasses.  Without this, diamond
          inheritance like
              Derived : BaseCxx (provides concrete),
                        BaseQore (inherits abstract)
          is left with stale abstract entries that later trip the
          allow_abstract=false assertion in `execConstructor`.
        - validate: confirm base-class reachability.
        Must run in this order per session.  In batch mode,
        MultiDeserializer runs prepare across all sessions, then
        doCommit across all sessions, etc. */
    bool commitClassesPrepare(std::string& error);
    bool commitClassesDoCommit(std::string& error);
    bool commitClassesImportAbstract(std::string& error);
    bool commitClassesResolveAbstract(std::string& error);
    bool commitClassesValidate(std::string& error);
    bool resolveBCAExpressionsPhase(std::string& error) {
        return resolveBCAExpressions(error);
    }

    //! Phase-split 2d — resolve pending static-method defaults,
    //! embedded source metadata, rebuild indexes, and resolve BCA
    //! expression blobs.  Must run last.
    /** Single-blob callers invoke this directly.  The multi-deserializer
        instead splits it around a single cross-session index rebuild via
        `finalizePreIndex()` / `finalizePostIndex()` to avoid the O(N*T)
        rebuild (132 sessions × full-tree walk) that dominated the
        finalize phase. */
    bool finalize(std::string& error);

    //! Per-session finalize work that does NOT require rebuilt indexes
    //! (pending static-method defaults + embedded source metadata).
    //! Used by the multi-deserializer before the single cross-session
    //! index rebuild.
    bool finalizePreIndex(std::string& error);

    //! Per-session finalize work that requires rebuilt indexes (BCA
    //! expression resolution).  Used by the multi-deserializer AFTER
    //! the single cross-session index rebuild.
    bool finalizePostIndex(std::string& error);

    //! Install lazy executable IR on source-stripped variants for qcc parse-time calls.
    bool installSourceParseIRFallbacks(std::string& error);

    ~QoreAOTBinaryDeserializer() {
        delete type_resolver;
        // Clean up any pending QoreValues that weren't transferred to the namespace tree
        // (only fires on early-return error paths; normal path transfers ownership)
        for (auto& class_members : pending_instance_members) {
            for (auto& pim : class_members) {
                pim.default_val.discard(nullptr);
            }
        }
        for (auto& hd_pair : pending_hashdecl_members) {
            for (auto& phm : hd_pair.second) {
                phm.default_val.discard(nullptr);
            }
        }
    }

    //! Deserialize binary metadata into a QoreProgram's namespace tree
    /** @param pgm the target QoreProgram (must be set up with parse options)
        @param data pointer to the binary metadata blob
        @param size size of the binary metadata blob
        @param error receives error message on failure
        @return true on success, false on failure
    */
    bool deserializeIntoProgram(QoreProgram* pgm, const uint8_t* data, uint32_t size, std::string& error);
    bool deserializeIntoProgram(QoreProgram* pgm, QoreAOTBinaryReader&& open_reader, std::string& error);

    //! Get the reader for access to header info after deserialization
    const QoreAOTBinaryReader& getReader() const { return reader; }

    //! Transfer the open reader after all deserialization work is complete.
    QoreAOTBinaryReader takeReader() { return std::move(reader); }

    //! Releases the reader's decompressed metadata pool
    /** Call once deserialization and function registration are complete and the
        reader will not be used again; see
        QoreAOTBinaryReader::releaseDecompressedBody().  The pool is a large,
        process-lifetime allocation for big programs, so releasing it before the
        program runs keeps it out of the steady-state footprint.
    */
    DLLLOCAL void releaseReaderBody() { reader.releaseDecompressedBody(); }

    //! Expose the session's type resolver so the slot-map register
    //! phase can reuse its warmed cache (populated during
    //! deserializeFunctionsAndMethods).  Body-local slot resolutions
    //! for common type paths (`any`, `int`, `*hash<auto>`, ...) hit
    //! the same cache the variant signatures populated, avoiding
    //! ~3 M cold parser round-trips in qwf-scale batches.
    QoreAOTTypeResolver* getTypeResolver() const { return type_resolver; }

    //! Check if explicit --include-source data is present.
    bool hasEmbeddedSource() const { return embedded_source != nullptr; }

    //! Get the embedded source text.
    const char* getEmbeddedSource() const { return embedded_source; }

    //! Get the embedded source text length.
    size_t getEmbeddedSourceLen() const { return embedded_source_len; }

    //! Check if this legacy object names functions that require disabled source fallback.
    bool hasLegacyFallbackFunctions() const { return !fallback_func_names.empty(); }

    //! Get the legacy list of function names that need source fallback.
    const std::vector<std::string>& getFallbackFuncNames() const { return fallback_func_names; }

    //! Check if a specific function needs disabled source fallback.
    bool needsFallback(const char* func_name) const {
        for (auto& name : fallback_func_names) {
            if (name == func_name) {
                return true;
            }
        }
        return false;
    }
};

//! Phase 4 slice 10: multi-blob AOT metadata deserializer.
/** Loads N binary metadata blobs into a single QoreProgram with
    DEFERRED resolution — phase 1 (shell creation) runs per blob as it
    arrives, then phase 2 (cross-blob reference resolution) runs once
    after every blob is in.  This lets a caller load a set of per-file
    `.qo`s into a compile program without worrying about topological
    order between them — a class in blob A can inherit from a class in
    blob B regardless of which addBlob() was called first, because both
    classes' shells land in the program's namespace tree before
    resolveClassBases / resolveInstanceMembers / etc. runs.

    Used by:
    - slice 10c's `qcc -c -L<dir>` (preload sibling `.qo`s' decls into
      the compile program before parsing a target source);
    - (future) slice 6b's source-less aggregator (merge fragment
      blobs without re-parsing source).

    The existing single-blob entry point
    `QoreAOTBinaryDeserializer::deserializeIntoProgram` is preserved
    as a 1-blob shortcut (internally: openAndDeserializeShells +
    resolveAll), so existing runtime callers (qore_aot_module_init_v3
    etc.) are unchanged.
*/
class QoreAOTBinaryMultiDeserializer {
    QoreProgram* pgm = nullptr;
    std::vector<std::unique_ptr<QoreAOTBinaryDeserializer>> sessions;

    // Shared type-resolver cache across all sessions in the batch.
    // Every addBlob() injects a pointer to this map into the
    // session's type_resolver, so the first session pays for each
    // path lookup and subsequent sessions hit the cache.  Shrinks
    // readAndSetupVariantSignature time from O(sessions × paths)
    // to O(paths) on the hot path.
    std::unordered_map<std::string, const QoreTypeInfo*> shared_type_cache_;

    // Phase-timing accumulators (microseconds).  Populated only
    // when QORE_AOT_PHASE_TIMING env var is set.  Totals across
    // all sessions for each named phase.
    struct PhaseTiming {
        const char* name;
        uint64_t us_total = 0;
    };
    PhaseTiming timings_[13] = {
        {"addBlob",                  0},
        {"resolveTypesAndMembers",   0},
        {"rebuildBaseClassSml",      0},
        {"importInheritedMembers",   0},
        {"resolveStaticsAndConsts",  0},
        {"deserializeFuncsMethods",  0},
        {"commitClassesPrepare",     0},
        {"commitClassesDoCommit",    0},
        {"commitClassesImportAbs",   0},
        {"commitClassesResolveAbs",  0},
        {"commitClassesValidate",    0},
        {"finalize",                 0},
        {"TOTAL",                    0},
    };
    static bool timingEnabled() {
        static int cached = -1;
        if (cached < 0) {
            cached = getenv("QORE_AOT_PHASE_TIMING") ? 1 : 0;
        }
        return cached != 0;
    }
    static uint64_t nowMicros() {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t)ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000ULL;
    }

public:
    //! Create a multi-deserializer bound to a target program.
    explicit QoreAOTBinaryMultiDeserializer(QoreProgram* in_pgm)
            : pgm(in_pgm) {
    }

    ~QoreAOTBinaryMultiDeserializer() {
        if (timingEnabled()) {
            uint64_t total = 0;
            for (int i = 0; i < 12; ++i) {
                total += timings_[i].us_total;
            }
            timings_[12].us_total = total;
            fprintf(stderr, "[aot-timing] ===== phase totals (usec) "
                "sessions=%zu =====\n", sessions.size());
            for (int i = 0; i < 13; ++i) {
                fprintf(stderr, "[aot-timing]   %-26s %8lu us (%5.1f %%)\n",
                    timings_[i].name,
                    (unsigned long)timings_[i].us_total,
                    total ? 100.0 * timings_[i].us_total / total : 0.0);
            }
            // Sub-breakdown of deserializeFuncsMethods (across all sessions)
            extern uint64_t g_aot_sum_funcs_us;
            extern uint64_t g_aot_sum_methods_us;
            extern uint64_t g_aot_dm_alloc_us;
            extern uint64_t g_aot_dm_sig_us;
            extern uint64_t g_aot_dm_add_us;
            extern uint64_t g_aot_dm_variants;
            fprintf(stderr, "[aot-timing]   (of which: deserializeFunctions   %8lu us)\n",
                (unsigned long)g_aot_sum_funcs_us);
            fprintf(stderr, "[aot-timing]   (of which: deserializeMethods     %8lu us)\n",
                (unsigned long)g_aot_sum_methods_us);
            fprintf(stderr, "[aot-timing]      deserializeMethods variants: %lu\n",
                (unsigned long)g_aot_dm_variants);
            fprintf(stderr, "[aot-timing]      (variant alloc + dynamic_cast  %8lu us)\n",
                (unsigned long)g_aot_dm_alloc_us);
            fprintf(stderr, "[aot-timing]      (readAndSetupVariantSignature  %8lu us)\n",
                (unsigned long)g_aot_dm_sig_us);
            fprintf(stderr, "[aot-timing]      (addUserMethod                 %8lu us)\n",
                (unsigned long)g_aot_dm_add_us);
            extern uint64_t g_aot_dm_sig_setup_us;
            fprintf(stderr, "[aot-timing]         (of signature: setupFromAOTMetadata %8lu us)\n",
                (unsigned long)g_aot_dm_sig_setup_us);
            fflush(stderr);
        }
    }

    //! Phase 1: open a new blob and create its shells (namespaces,
    //! class declarations, hashdecl/enum/typedef stubs) in the
    //! target program.  Does NOT run resolution passes — call
    //! resolveAll() after every desired blob has been added.
    /** @return true on success; false on blob-open or shell-phase
                failure (error populated) */
    bool addBlob(const uint8_t* data, uint32_t size, std::string& error) {
        uint64_t t0 = timingEnabled() ? nowMicros() : 0;
        // Apply this blob's %prepend-module-path / %append-module-path lists to
        // the target Program BEFORE shell deserialization runs — so any module
        // dependency loads triggered during shell creation see the expected
        // search surface.  Merge is dedup-on-append, matching the semantics of
        // applyModulePathDirective.
        {
            std::vector<std::string> prepended, appended;
            std::string mp_err;
            if (readModulePathLists(data, size, prepended, appended, mp_err)) {
                applyModulePathListsToProgram(pgm, prepended, appended);
            }
        }
        auto deser = std::make_unique<QoreAOTBinaryDeserializer>();
        if (!deser->openAndDeserializeShells(pgm, data, size, error)) {
            return false;
        }
        // Install the shared type-resolver cache so this session's
        // lookups share results with sibling sessions in the batch.
        deser->setSharedTypeCache(&shared_type_cache_);
        sessions.push_back(std::move(deser));
        if (timingEnabled()) {
            timings_[0].us_total += nowMicros() - t0;
        }
        return true;
    }

    //! Phase 2: run all resolution passes for every blob added since
    //! construction.  Must be called exactly once after all
    //! addBlob()s, before any user code depends on the deserialized
    //! declarations being complete.
    /** Cross-blob references are handled by the existing
        `pgm->findClass` / `runtimeFindClass` lookup paths — each
        resolution pass walks the shared program namespace tree, so
        blob A's class finding blob B's base class "just works" as
        long as both shells have been created.

        The phase-2 sub-steps are interleaved across sessions so
        every class's method map is populated before any session
        commits its classes.  Without the interleave, a sibling
        session that owns a derived class could trigger a recursive
        parseCommit on its base class (owned by a LATER session)
        before that base's methods are deserialized — silently
        committing the base as empty, losing its constructor, and
        breaking base-class-constructor-argument delegation at
        runtime. */
    bool resolveAll(std::string& error) {
        // Phase-sync invariants documented at each sub-phase.
        // Timing (via QORE_AOT_PHASE_TIMING) validates where
        // load-time is actually spent.  Reported in the dtor.
        const bool time_on = timingEnabled();
#define AOT_PHASE_TIME(idx, body)                                    \
        do {                                                         \
            uint64_t _t0 = time_on ? nowMicros() : 0;                \
            body;                                                    \
            if (time_on) {                                           \
                timings_[idx].us_total += nowMicros() - _t0;         \
            }                                                        \
        } while (0)

        auto runSessionPhase = [this, &error](const char* context, auto&& fn) -> bool {
            size_t i = 0;
            for (auto& sess : sessions) {
                if (i && !(i % 100) && qore_check_cancel(nullptr, context)) {
                    error = std::string("operation cancelled during ") + context;
                    return false;
                }
                if (!fn(*sess)) {
                    return false;
                }
                ++i;
            }
            return true;
        };

        std::unordered_map<std::string, QoreClass*> batch_class_lookup_map;
        for (auto& sess : sessions) {
            sess->appendClassesToLookupMap(batch_class_lookup_map);
        }
        for (auto& sess : sessions) {
            sess->setBatchClassLookupMap(&batch_class_lookup_map);
        }
        struct BatchClassLookupScope {
            std::vector<std::unique_ptr<QoreAOTBinaryDeserializer>>& sessions;
            ~BatchClassLookupScope() {
                for (auto& sess : sessions) {
                    sess->setBatchClassLookupMap(nullptr);
                }
            }
        } batch_class_lookup_scope{sessions};

        // Shell creation may add namespace/class/hashdecl declarations before
        // the root lookup indexes are rebuilt.  Cross-blob type and base-class
        // resolution below uses those indexes, so publish the full shell set
        // before any session resolves names.
        if (!sessions.empty()) {
            rebuildRootIndexesOnce();
        }

        // 2a: types/bases first, then constants, methods, static members,
        // and each session's OWN instance members.  Methods must be added
        // before static/default resolution can observe or initialize class
        // state; otherwise abstract declarations can arrive after an AOT
        // class was already marked initialized.  The barriers are required
        // for member defaults like Class::Defaults.Key, Class::staticVar,
        // Class::staticMethod(), and signature defaults that reference
        // globals declared in sibling script fragments.
        AOT_PHASE_TIME(1, {
            if (!runSessionPhase("AOT type resolution",
                    [&error](QoreAOTBinaryDeserializer& sess) {
                        return sess.resolveTypes(error);
                    })) return false;
            if (!runSessionPhase("AOT class constant shell registration",
                    [&error](QoreAOTBinaryDeserializer& sess) {
                        return sess.registerClassConstantShellsPhase(error);
                    })) return false;
            if (!runSessionPhase("AOT namespace constant resolution",
                    [&error](QoreAOTBinaryDeserializer& sess) {
                        return sess.resolveNamespaceConstantsPhase(error);
                    })) return false;
            if (!runSessionPhase("AOT class constant value resolution",
                    [&error](QoreAOTBinaryDeserializer& sess) {
                        return sess.resolveClassConstantValuesPhase(error);
                    })) return false;
            if (!runSessionPhase("AOT global resolution",
                    [&error](QoreAOTBinaryDeserializer& sess) {
                        return sess.resolveStaticsAndConstants(error);
                    })) return false;
            if (!sessions.empty()) {
                rebuildRootIndexesOnce();
            }
            if (!runSessionPhase("AOT function and method deserialization",
                    [&error](QoreAOTBinaryDeserializer& sess) {
                        return sess.deserializeFunctionsAndMethods(error);
                    })) return false;
            if (!sessions.empty()) {
                rebuildRootIndexesOnce();
            }
            if (!runSessionPhase("AOT static member resolution",
                    [&error](QoreAOTBinaryDeserializer& sess) {
                        return sess.resolveStaticMembersPhase(error);
                    })) return false;
            if (!runSessionPhase("AOT member resolution",
                    [&error](QoreAOTBinaryDeserializer& sess) {
                        return sess.resolveMembers(error);
                    })) return false;
        });
        // 2a-sml: re-propagate super-class map lists now that
        // every session has attached its direct bases.
        AOT_PHASE_TIME(2, {
            if (!runSessionPhase("AOT base class map rebuild",
                    [&error](QoreAOTBinaryDeserializer& sess) {
                        return sess.rebuildBaseClassSmlPhase(error);
                    })) return false;
        });
        // 2a-import: copy base-class members into derived classes
        // only AFTER every session has finished resolveTypesAndMembers.
        AOT_PHASE_TIME(3, {
            if (!runSessionPhase("AOT inherited member import",
                    [&error](QoreAOTBinaryDeserializer& sess) {
                        return sess.importInheritedMembersPhase(error);
                    })) return false;
        });
        // 2a-post: top-level globals.  Already performed before
        // function/method deserialization; keep this timing bucket reserved
        // so existing phase timing output stays stable.
        AOT_PHASE_TIME(4, {
        });
        // 2b: function and method deserialization used to live here.  It now
        // runs before resolveMembers() so member default expression trees can
        // resolve static method calls.  Keep this timing bucket reserved so
        // existing phase timing output stays stable.
        AOT_PHASE_TIME(5, {
        });
        // 2c: commit classes — 4 sub-phases interleaved across
        // sessions so a session's parseCommit walk can find
        // sibling sessions' classes prepared.
        AOT_PHASE_TIME(6, {
            if (!runSessionPhase("AOT class commit prepare",
                    [&error](QoreAOTBinaryDeserializer& sess) {
                        return sess.commitClassesPrepare(error);
                    })) return false;
        });
        AOT_PHASE_TIME(7, {
            if (!runSessionPhase("AOT class commit",
                    [&error](QoreAOTBinaryDeserializer& sess) {
                        return sess.commitClassesDoCommit(error);
                    })) return false;
        });
        AOT_PHASE_TIME(8, {
            if (!runSessionPhase("AOT abstract import",
                    [&error](QoreAOTBinaryDeserializer& sess) {
                        return sess.commitClassesImportAbstract(error);
                    })) return false;
        });
        AOT_PHASE_TIME(9, {
            if (!runSessionPhase("AOT abstract resolution",
                    [&error](QoreAOTBinaryDeserializer& sess) {
                        return sess.commitClassesResolveAbstract(error);
                    })) return false;
        });
        AOT_PHASE_TIME(10, {
            if (!runSessionPhase("AOT class validation",
                    [&error](QoreAOTBinaryDeserializer& sess) {
                        return sess.commitClassesValidate(error);
                    })) return false;
        });
        // 2d: finalize — pending static-method defaults, fallback
        // sources, single cross-session index rebuild, BCA resolution.
        //
        // The per-session single-blob `finalize()` used to rebuild the
        // entire root namespace index each time; with N sessions that
        // was O(N * tree_size) — dominant in batch mode.  Split into
        // pre-index + post-index halves so a single rebuild covers the
        // whole batch.
        AOT_PHASE_TIME(11, {
            if (!runSessionPhase("AOT pre-index finalization",
                    [&error](QoreAOTBinaryDeserializer& sess) {
                        return sess.finalizePreIndex(error);
                    })) return false;
            if (!sessions.empty()) {
                rebuildRootIndexesOnce();
            }
            if (!runSessionPhase("AOT post-index finalization",
                    [&error](QoreAOTBinaryDeserializer& sess) {
                        return sess.finalizePostIndex(error);
                    })) return false;
        });
#undef AOT_PHASE_TIME
        return true;
    }

    //! Resolve sibling blobs far enough that a following source parseCommit()
    //! can commit the combined source/stub/AOT program in one normal pass.
    bool resolveForSourceParse(std::string& error) {
        auto runSessionPhase = [this, &error](const char* context, auto&& fn) -> bool {
            size_t i = 0;
            for (auto& sess : sessions) {
                if (i && !(i % 100) && qore_check_cancel(nullptr, context)) {
                    error = std::string("operation cancelled during ") + context;
                    return false;
                }
                if (!fn(*sess)) {
                    return false;
                }
                ++i;
            }
            return true;
        };

        std::unordered_map<std::string, QoreClass*> batch_class_lookup_map;
        for (auto& sess : sessions) {
            sess->appendClassesToLookupMap(batch_class_lookup_map);
        }
        for (auto& sess : sessions) {
            sess->setBatchClassLookupMap(&batch_class_lookup_map);
        }
        struct BatchClassLookupScope {
            std::vector<std::unique_ptr<QoreAOTBinaryDeserializer>>& sessions;
            ~BatchClassLookupScope() {
                for (auto& sess : sessions) {
                    sess->setBatchClassLookupMap(nullptr);
                }
            }
        } batch_class_lookup_scope{sessions};

        // Publish all preloaded shell declarations before resolving sibling
        // types and bases.  The source parser also needs this when it runs
        // before resolveForSourceParse(), so compileScriptFile() calls
        // rebuildShellIndexes() after the preload loop.
        if (!sessions.empty()) {
            rebuildRootIndexesOnce();
        }

        if (!runSessionPhase("AOT type resolution",
                [&error](QoreAOTBinaryDeserializer& sess) {
                    return sess.resolveTypes(error);
                })) {
            return false;
        }
        if (!runSessionPhase("AOT class constant shell registration",
                [&error](QoreAOTBinaryDeserializer& sess) {
                    return sess.registerClassConstantShellsPhase(error);
                })) {
            return false;
        }
        if (!runSessionPhase("AOT namespace constant resolution",
                [&error](QoreAOTBinaryDeserializer& sess) {
                    return sess.resolveNamespaceConstantsPhase(error);
                })) {
            return false;
        }
        if (!runSessionPhase("AOT class constant value resolution",
                [&error](QoreAOTBinaryDeserializer& sess) {
                    return sess.resolveClassConstantValuesPhase(error);
                })) {
            return false;
        }
        if (!runSessionPhase("AOT global resolution",
                [&error](QoreAOTBinaryDeserializer& sess) {
                    return sess.resolveStaticsAndConstants(error);
                })) {
            return false;
        }
        if (!sessions.empty()) {
            rebuildRootIndexesOnce();
        }
        if (!runSessionPhase("AOT function and method deserialization",
                [&error](QoreAOTBinaryDeserializer& sess) {
                    return sess.deserializeFunctionsAndMethods(error);
                })) {
            return false;
        }
        if (!sessions.empty()) {
            rebuildRootIndexesOnce();
        }
        if (!runSessionPhase("AOT static member resolution",
                [&error](QoreAOTBinaryDeserializer& sess) {
                    return sess.resolveStaticMembersPhase(error);
                })) {
            return false;
        }
        if (!runSessionPhase("AOT member resolution",
                [&error](QoreAOTBinaryDeserializer& sess) {
                    return sess.resolveMembers(error);
                })) {
            return false;
        }
        if (!runSessionPhase("AOT base class map rebuild",
                [&error](QoreAOTBinaryDeserializer& sess) {
                    return sess.rebuildBaseClassSmlPhase(error);
                })) {
            return false;
        }
        if (!runSessionPhase("AOT inherited member import",
                [&error](QoreAOTBinaryDeserializer& sess) {
                    return sess.importInheritedMembersPhase(error);
                })) {
            return false;
        }
        if (!runSessionPhase("AOT class commit prepare",
                [&error](QoreAOTBinaryDeserializer& sess) {
                    return sess.commitClassesPrepare(error);
                })) {
            return false;
        }
        if (!sessions.empty()) {
            rebuildRootIndexesOnce();
        }

        // Install BCA expression blobs before the following source parseCommit().
        // Source constants may instantiate preloaded sibling classes during
        // that commit, so their explicit base-constructor calls must already
        // be present on the pending constructor variants.
        if (!runSessionPhase("AOT BCA expression resolution",
                [&error](QoreAOTBinaryDeserializer& sess) {
                    return sess.resolveBCAExpressionsPhase(error);
                })) {
            return false;
        }
        return runSessionPhase("AOT source-parse IR fallback installation",
            [&error](QoreAOTBinaryDeserializer& sess) {
                return sess.installSourceParseIRFallbacks(error);
            });
    }

    //! Finish AOT-only post-commit work after source parseCommit().
    bool finalizeAfterSourceParse(std::string& error) {
        auto runSessionPhase = [this, &error](const char* context, auto&& fn) -> bool {
            size_t i = 0;
            for (auto& sess : sessions) {
                if (i && !(i % 100) && qore_check_cancel(nullptr, context)) {
                    error = std::string("operation cancelled during ") + context;
                    return false;
                }
                if (!fn(*sess)) {
                    return false;
                }
                ++i;
            }
            return true;
        };

        std::unordered_map<std::string, QoreClass*> batch_class_lookup_map;
        for (auto& sess : sessions) {
            sess->appendClassesToLookupMap(batch_class_lookup_map);
        }
        for (auto& sess : sessions) {
            sess->setBatchClassLookupMap(&batch_class_lookup_map);
        }
        struct BatchClassLookupScope {
            std::vector<std::unique_ptr<QoreAOTBinaryDeserializer>>& sessions;
            ~BatchClassLookupScope() {
                for (auto& sess : sessions) {
                    sess->setBatchClassLookupMap(nullptr);
                }
            }
        } batch_class_lookup_scope{sessions};

        if (!runSessionPhase("AOT abstract import",
                [&error](QoreAOTBinaryDeserializer& sess) {
                    return sess.commitClassesImportAbstract(error);
                })) {
            return false;
        }
        if (!runSessionPhase("AOT abstract resolution",
                [&error](QoreAOTBinaryDeserializer& sess) {
                    return sess.commitClassesResolveAbstract(error);
                })) {
            return false;
        }
        if (!runSessionPhase("AOT class validation",
                [&error](QoreAOTBinaryDeserializer& sess) {
                    return sess.commitClassesValidate(error);
                })) {
            return false;
        }
        if (!runSessionPhase("AOT pre-index finalization",
                [&error](QoreAOTBinaryDeserializer& sess) {
                    return sess.finalizePreIndex(error);
                })) {
            return false;
        }
        if (!sessions.empty()) {
            rebuildRootIndexesOnce();
        }
        return runSessionPhase("AOT post-index finalization",
            [&error](QoreAOTBinaryDeserializer& sess) {
                return sess.finalizePostIndex(error);
            });
    }

    //! Number of blobs currently in-session.
    size_t sessionCount() const { return sessions.size(); }

    //! Rebuild root namespace indexes after shell preload so source parsing can
    //! resolve sibling `.qo` declarations before the later resolution pass.
    void rebuildShellIndexes() {
        if (!sessions.empty()) {
            rebuildRootIndexesOnce();
        }
    }

    //! Access a specific session by insertion index (slice 10g).
    //! Used by the batch-register end path to pull each session's
    //! reader for per-blob registerAOTFunctionsFromSlotMaps + init
    //! execution.  Index must be < sessionCount().
    QoreAOTBinaryDeserializer& session(size_t i) { return *sessions[i]; }

private:
    //! Rebuild the target program's root namespace indexes once for
    //! the entire batch (replaces the per-session rebuild that ran in
    //! each finalize()).  Implemented in QoreAOTBinary.cpp to keep
    //! qore_program_private and qore_root_ns_private out of this header.
    void rebuildRootIndexesOnce();
};

// ---- IR Function Serialization (Phase 5) ----

//! Instruction group tag for IR function serialization
/** Identifies the instruction subclass for correct serialization/deserialization
    of subclass-specific fields. Stored as a u8 in the binary format.
*/
enum class QoreIRInstGroup : uint8_t {
    Base = 0,               //!< QoreIRInstruction — no extra fields
    Const = 1,              //!< QoreIRConstInstruction
    Branch = 2,             //!< QoreIRBranchInstruction
    BranchIf = 3,           //!< QoreIRBranchIfInstruction
    Return = 4,             //!< QoreIRReturnInstruction
    Throw = 5,              //!< QoreIRThrowInstruction
    Local = 6,              //!< QoreIRLocalInstruction
    Var = 7,                //!< QoreIRVarInstruction
    LValue = 8,             //!< QoreIRLValueInstruction
    Expr = 9,               //!< QoreIRExprInstruction
    CallDirect = 10,        //!< QoreIRCallDirectInstruction
    CallMethodDirect = 11,  //!< QoreIRCallMethodDirectInstruction
    InvokeMethodDirect = 12,//!< QoreIRInvokeMethodDirectInstruction
    CallStaticDirect = 13,  //!< QoreIRCallStaticDirectInstruction
    DotEvalMethodDirect = 14, //!< QoreIRDotEvalMethodDirectInstruction
    InvokeDotEvalMethodDirect = 15, //!< QoreIRInvokeDotEvalMethodDirectInstruction
    Invoke = 16,            //!< QoreIRInvokeInstruction
    ScopeEnter = 17,        //!< QoreIRScopeEnterInstruction
    ScopeExit = 18,         //!< QoreIRScopeExitInstruction
    LandingPad = 19,        //!< QoreIRLandingPadInstruction
    SwitchInt = 20,         //!< QoreIRSwitchIntInstruction
    SwitchString = 21,      //!< QoreIRSwitchStringInstruction
    Phi = 22,               //!< QoreIRPhiInstruction
    Guard = 23,             //!< QoreIRGuardInstruction
    ImplicitArg = 24,       //!< QoreIRImplicitArgInstruction
    HashKeyAccess = 25,     //!< QoreIRHashKeyAccessInstruction
    SelfMember = 26,        //!< QoreIRSelfMemberInstruction
    StaticVar = 27,         //!< QoreIRStaticVarInstruction
    NewObject = 28,         //!< QoreIRNewObjectInstruction
    LoadConst = 29,         //!< QoreIRLoadConstantInstruction
    CreateClosure = 30,     //!< QoreIRCreateClosureInstruction
    CreateCallRef = 31,     //!< QoreIRCreateCallRefInstruction
    CreateMethodRef = 32,   //!< QoreIRCreateMethodRefInstruction
    CreateParseRef = 33,    //!< QoreIRCreateParseRefInstruction
    NewHashDecl = 34,       //!< QoreIRNewHashDeclInstruction
    NewComplexHash = 35,    //!< QoreIRNewComplexHashInstruction
    NewComplexList = 36,    //!< QoreIRNewComplexListInstruction
    VrnConstruct = 37,      //!< QoreIRVrnConstructInstruction
    HashKeyStore = 38,      //!< QoreIRHashKeyStoreInstruction
    ListIndexStore = 39,    //!< QoreIRListIndexStoreInstruction
    FusedAddLocal = 40,     //!< QoreIRAddAssignLocalIntInstruction
    FusedIncLocal = 41,     //!< QoreIRIncrementLocalIntInstruction
    FusedBrLtLocal = 42,    //!< QoreIRBranchIfLtLocalIntInstruction
    MapHashKey = 43,        //!< QoreIRMapHashKeyInstruction
    OnBlockExit = 44,       //!< QoreIROnBlockExitInstruction
    IteratorCreate = 45,    //!< QoreIRIteratorCreateInstruction
    IteratorNext = 46,      //!< QoreIRIteratorNextInstruction
    SwitchRegexMatch = 47,  //!< QoreIRSwitchRegexMatchInstruction
    RefForeachInit = 48,    //!< QoreIRRefForeachInitInstruction
    MakeHashConstKeys = 49, //!< QoreIRMakeHashConstKeysInstruction
    SwitchCaseMatch = 50,   //!< QoreIRSwitchCaseMatchInstruction
    Context = 51,           //!< QoreIRContextInstruction
    Summarize = 52,         //!< QoreIRSummarizeInstruction
    ListIndexAccess = 53,   //!< QoreIRListIndexAccessInstruction
    NewHashDeclFromHash = 54, //!< QoreIRNewHashDeclFromHashInstruction
    HashKeyStoreDynamic = 55, //!< QoreIRHashKeyStoreDynamicInstruction
    LValuePath = 56,        //!< QoreIRLValuePathInstruction
    MakeList = 57,          //!< QoreIRMakeListInstruction
    MakeHash = 58,          //!< QoreIRMakeHashInstruction
    CallClosureDirect = 59, //!< Native closure/call-reference invocation
    Backquote = 60,         //!< QoreIRBackquoteInstruction
    Find = 61,              //!< QoreIRFindInstruction
    Background = 62,        //!< QoreIRBackgroundInstruction
    ContextRef = 63,        //!< QoreIRContextRefInstruction
    TypedBase = 64,         //!< QoreIRInstruction with element_type metadata
    NewComplexBuffer = 65,  //!< QoreIRNewComplexBufferInstruction
    Plugin = 66,            //!< QoreIRPluginInstruction
    Unsupported = 0xFF,     //!< Instruction cannot be serialized
};

//! Callback for serializing AST expressions embedded in IR instructions
/** Called for each QoreValue expr/lvalue field that needs serialization.
    Must write the expression data in AOTExprKind format (u8 kind + kind-specific refs).
    @param writer binary writer
    @param expr the AST expression to serialize
    @return true on success, false if expression cannot be serialized
*/
typedef std::function<bool(QoreAOTBinaryWriter& writer, const QoreValue& expr)> AOTExprWriteFunc;

//! Callback for deserializing AST expressions embedded in IR instructions
/** Reads expression data in AOTExprKind format and reconstructs the AST expression.
    @param reader binary reader (for string pool access)
    @param ptr data pointer (advanced past the expression data)
    @param end pointer past end of valid data
    @param error receives error message on failure
    @return the reconstructed expression, or NOTHING on failure
*/
typedef std::function<QoreValue(const QoreAOTBinaryReader& reader, const uint8_t*& ptr,
    const uint8_t* end, std::string& error)> AOTExprReadFunc;

//! Serialize a QoreIRFunction to binary format
/** Writes a compact binary representation of the IR function that can be
    deserialized at runtime to reconstruct the function for IR interpreter execution.

    Binary format:
    - Function header: name, max_value_id, max_local_slot_id, num_guards, return_type, block/local counts
    - Local variable slot table: name, type_path, slot_id for each local
    - Body locals list: name, type_path for each body local
    - Blocks: for each block, name, is_loop_header, instructions
    - Instructions: opcode, group_tag, result, operands, exception_target, group-specific fields

    @param writer binary writer (uses string pool and buffer)
    @param func the IR function to serialize
    @param writeExpr callback for serializing AST expression fields
    @return true on success, false if any instruction cannot be serialized
*/
bool serializeIRFunction(QoreAOTBinaryWriter& writer, const QoreIRFunction& func,
    const AOTExprWriteFunc& writeExpr);

//! Deserialize a QoreIRFunction from binary data
/** Reads binary data written by serializeIRFunction() and reconstructs a
    QoreIRFunction suitable for IR interpreter execution.

    @param reader binary reader (for string pool access)
    @param ptr data pointer (advanced past the function data on success)
    @param end pointer past end of valid data
    @param pgm the QoreProgram for namespace/type resolution
    @param readExpr callback for deserializing AST expression fields
    @param enclosing_locals optional name→LocalVar* map from the enclosing function's scope
    @param error receives error message on failure
    @return reconstructed function, or nullptr on failure
*/
std::unique_ptr<QoreIRFunction> deserializeIRFunction(
    const QoreAOTBinaryReader& reader,
    const uint8_t*& ptr,
    const uint8_t* end,
    QoreProgram* pgm,
    const AOTExprReadFunc& readExpr,
    const std::unordered_map<std::string, LocalVar*>* enclosing_locals,
    std::string& error,
    LocalVar** parent_locals_arr = nullptr,
    int num_parent_locals = 0,
    //! If non-null, after reading the IR header's body_locals section,
    //! this vector is extended with func->all_body_locals in the same
    //! order they were written.  Used by closure deserialization so that
    //! EXPR_TREE blobs in the closure body can resolve LocalVar slot
    //! indices that reference body locals (see CLOSURE_CREATE handlers
    //! in lib/QoreAOTRuntime.cpp and lib/QoreAOTExprHandlers.cpp for
    //! the matching writer-side extension in lib/QoreAOTBinary.cpp
    //! classifyAndWriteExpr's closure branch and in
    //! lib/QoreAOTExprSlotHandlers.cpp write_slot_CLOSURE_CREATE).
    std::vector<LocalVar*>* extended_closure_locals = nullptr,
    //! If true, parent_locals_arr is the function's own slot-id-indexed local
    //! table, not just a parent prefix. Used for full source-stripped debug IR.
    bool use_parent_locals_for_all_slots = false,
    //! Optional body-local table already resolved by the surrounding AOT context.
    //! Indexed in serialized body-local order.
    const std::vector<LocalVar*>* direct_body_locals = nullptr,
    //! Optional Program that owns newly created LocalVars. Namespace and type
    //! resolution still use pgm.
    QoreProgram* local_owner_pgm = nullptr,
    //! If true, reconstruct only the function header and local tables, then
    //! advance ptr to end without deserializing blocks or instructions.
    bool metadata_only = false,
    //! Optional sink used for cooperative cancellation during lazy runtime materialization.
    ExceptionSink* cancel_xsink = nullptr);

//! Compress metadata blob using zlib
/** Compresses the serialized metadata blob to reduce size and LLVM compilation overhead.
    The compressed data includes the original size as a 4-byte little-endian prefix.

    @param input the uncompressed metadata blob
    @param output receives the compressed data
    @param error receives error message on failure
    @return true on success, false on compression failure
*/
bool compressMetadata(const std::vector<uint8_t>& input,
    std::vector<uint8_t>& output,
    std::string& error);

//! Compress metadata using Zstandard
bool compressMetadataZstd(const std::vector<uint8_t>& input,
    std::vector<uint8_t>& output,
    std::string& error);

//! Decompress metadata blob using zlib
/** Decompresses metadata previously compressed by compressMetadata().

    @param input pointer to compressed data
    @param input_len length of compressed data
    @param output receives the decompressed metadata
    @param error receives error message on failure
    @return true on success, false on decompression failure
*/
bool decompressMetadata(const uint8_t* input, size_t input_len,
    std::vector<uint8_t>& output,
    std::string& error);

//! Decompress metadata encoded by compressMetadataZstd()
bool decompressMetadataZstd(const uint8_t* input, size_t input_len,
    std::vector<uint8_t>& output,
    std::string& error);

// ============================================================================
// Internal Expression Handler Helpers (Phase 3.2+)
// ============================================================================
// These functions are used by expression registry handlers for recursive
// serialization/deserialization of nested expressions.

//! Classify and write a QoreValue expression in AOTExprKind format
//! @param writer binary writer to write to
//! @param expr the expression to serialize
//! @param parent_locals parent function's local slot metadata
//! @param parent_globals parent function's global slot metadata
//! @param const_reverse_map reverse map for constant lookup (optional)
//! @return true if expression was successfully serialized, false otherwise
bool classifyAndWriteExpr(QoreAOTBinaryWriter& writer, const QoreValue& expr,
        const std::vector<AOTLocalSlotId>& parent_locals,
        const std::vector<AOTGlobalSlotId>& parent_globals,
        const AOTConstantReverseMap* const_reverse_map = nullptr);

//! Write a default-argument value payload for AOT metadata without a fallback marker
/** Writes either a direct QoreAOTValueTag payload or a VT_EXPR_NATIVE payload.
    Unsupported defaults are fatal: the function returns false and sets a
    detailed diagnostic that names the default owner and parameter.
    The caller is responsible for writing any surrounding has-default marker.
    @param writer binary writer to write to
    @param v default argument value or expression to serialize
    @param owner_kind human-readable callable kind for diagnostics
    @param owner_name callable name for diagnostics
    @param param_name parameter name for diagnostics
    @param error receives a detailed failure reason when serialization fails
    @param parent_locals optional parent local-slot metadata for closure defaults
    @return true if the payload was serialized without fallback, false otherwise
*/
bool qoreAOTWriteDefaultArgValuePayload(QoreAOTBinaryWriter& writer, const QoreValue& v,
        const char* owner_kind, const char* owner_name, const char* param_name,
        std::string* error = nullptr,
        const std::vector<AOTLocalSlotId>* parent_locals = nullptr);

//! Read one expression from inline closure/handler IR binary data
//! @param rdr binary reader for reading data
//! @param p current read pointer (advanced by reading)
//! @param e end of valid data
//! @param err set to error message on failure
//! @param pgm the Qore program for symbol resolution
//! @param locals LocalVar* array for LOCAL_VARREF resolution (may be null)
//! @param num_locals number of entries in locals
//! @param globals Var* array for GLOBAL_VARREF resolution (may be null)
//! @param num_globals number of entries in globals
//! @return reconstructed expression, or NOTHING on failure
QoreValue readOneExpr(
        const QoreAOTBinaryReader& rdr, const uint8_t*& p, const uint8_t* e,
        std::string& err, QoreProgram* pgm,
        LocalVar** locals, int num_locals,
        Var** globals, int num_globals,
        QoreProgram* local_owner_pgm = nullptr);

//! Read one top-level serialized IR instruction expression field.
//!
//! Legacy GENERIC_EVAL is allowed only at this top level when reading old AOT
//! objects: it is the no-payload sentinel for opcodes whose native IR operands
//! are sufficient and whose AST expression field was intentionally omitted.
//! New AOT output must not emit GENERIC_EVAL.
QoreValue readOneTopLevelIRExpr(
        const QoreAOTBinaryReader& rdr, const uint8_t*& p, const uint8_t* e,
        std::string& err, QoreProgram* pgm,
        LocalVar** locals, int num_locals,
        Var** globals, int num_globals,
        QoreProgram* local_owner_pgm = nullptr);

#endif
