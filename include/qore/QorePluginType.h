/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QorePluginType.h

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

#ifndef _QORE_QOREPLUGINTYPE_H
#define _QORE_QOREPLUGINTYPE_H

#include <qore/common.h>

#include <cstdint>

class AbstractQoreNode;
class ExceptionSink;
class QoreHashNode;
class QoreIRBuilder;
class QoreIRLoweringContext;
class QoreListNode;
class QoreParseContext;
class QoreProgram;

struct QorePluginModuleHandle;

#define QORE_PLUGIN_ABI_VERSION_V1 "1.0"
#define QORE_PLUGIN_CANONICAL_SIGNATURE_VERSION_V1 1

enum class QorePluginHelperAbi : uint8_t {
    UnaryValue        = 0,
    BinaryValue       = 1,
    CallValueList     = 2,
    SubscriptValue    = 3,
    Construct         = 4,
    DenseBufferUnary  = 5,
    DenseBufferBinary = 6,
};

enum class QorePluginValueAccess : uint8_t {
    ReadOnly       = 0,
    MutatesLhs     = 1,
    MutatesRhs     = 2,
    MutatesBoth    = 3,
};

enum class QorePluginResultAlias : uint8_t {
    Unknown            = 0,
    MayAliasInputs     = 1,
    FreshNoAliasInputs = 2,
    ReturnsLhs         = 3,
    ReturnsRhs         = 4,
};

struct QorePluginValueOps {
    void (*incref)(uint64_t value_bits) noexcept;
    void (*decref)(uint64_t value_bits) noexcept;
    uint64_t (*clone)(uint64_t value_bits, ExceptionSink* xsink);
    bool (*equal)(uint64_t lhs_bits, uint64_t rhs_bits, ExceptionSink* xsink);
    int64_t (*hash)(uint64_t value_bits, ExceptionSink* xsink);
    void (*cleanup_slot)(uint64_t value_bits) noexcept;
};

enum class QorePluginLoweringResult : uint8_t {
    Lowered       = 0,
    NotApplicable = 1,
    Erroneous     = 2,
};

typedef QorePluginLoweringResult (*QorePluginLoweringCallback)(
    QoreIRLoweringContext* ctx,
    const AbstractQoreNode* ast_node,
    const QoreParseContext* parse_ctx,
    QoreIRBuilder* builder);

typedef const QoreTypeInfo* (*QorePluginTypePromotionCallback)(
    const QoreTypeInfo* lhs,
    const QoreTypeInfo* rhs);

typedef int (*QorePluginByteWriteCallback)(
    const void* data,
    uint32_t len,
    void* user_data,
    ExceptionSink* xsink);

typedef int (*QorePluginByteReadCallback)(
    void* data,
    uint32_t len,
    void* user_data,
    ExceptionSink* xsink);

typedef int (*QorePluginSerializeCallback)(
    uint64_t value_bits,
    QorePluginByteWriteCallback write,
    void* write_user_data,
    ExceptionSink* xsink);

typedef uint64_t (*QorePluginDeserializeCallback)(
    QorePluginByteReadCallback read,
    uint32_t payload_len,
    void* read_user_data,
    ExceptionSink* xsink);

struct QorePluginExtension {
    const char* extension_id;
    const void* extension_data;
    bool required;
};

enum class QorePluginOpcodeTypePromotion : uint8_t {
    Exact          = 0,
    WideningLattice = 1,
    IdentityLHS    = 2,
    Custom         = 3,
};

struct QorePluginOpcodeInfoExtended {
    bool may_have_side_effects;
    bool may_throw_exception;
    bool can_return_nothing;
    bool never_returns_nothing;
    bool is_commutative;
    bool is_associative;
    bool is_idempotent;
    bool annihilator_zero;
    bool has_identity;
    uint8_t identity_kind;
    uint64_t (*make_identity)(const QoreTypeInfo* result_type, ExceptionSink* xsink);
    bool is_pure_modulo_xsink;
    bool can_vectorize;
    QorePluginOpcodeTypePromotion type_promotion_kind;
    QorePluginTypePromotionCallback type_promotion_callback;
    bool is_simd_friendly;
    uint8_t cost_class;
};

struct QorePluginOperationSignature {
    uint8_t arity;
    const QoreTypeInfo* primary_type;
    const QoreTypeInfo* secondary_type;
    const QoreTypeInfo* return_type;
    bool primary_nullable;
    bool secondary_nullable;
    bool return_nullable;
    QorePluginValueAccess access;
    QorePluginResultAlias result_alias;
    QorePluginHelperAbi helper_abi;
};

struct QorePluginOperation {
    uint16_t local_id;
    const char* operation_name;
    QorePluginOperationSignature signature;
    QorePluginOpcodeInfoExtended info;
    void (*runtime_helper)();
    const char* runtime_helper_symbol;
    QorePluginLoweringCallback lowering_pattern;
    uint64_t lowering_claimed_node_kinds;
    int64_t qdom_domains;
    const QorePluginExtension* extensions;
    int num_extensions;
};

struct QorePluginTypeDescriptor {
    uint16_t local_type_id;
    const char* type_name;
    const QoreTypeInfo* type_info;
    QorePluginValueOps value_ops;
    QorePluginSerializeCallback serialize;
    QorePluginDeserializeCallback deserialize;
    uint16_t serializer_format_version;
    int64_t baseline_qdom_domains;
};

struct QorePluginDependency {
    const char* module_name;
    const char* min_plugin_abi_version;
    const char* min_operation_set_version;
};

struct QorePluginRegistrationContextV1 {
    uint32_t struct_size;
    uint32_t flags;
    const char* module_path;
    const QorePluginModuleHandle* module_handle;
};

struct QorePluginTypeRegistration {
    const char* module_name;
    const char* plugin_abi_version;
    const char* operation_set_version;
    const QorePluginTypeDescriptor* types;
    int num_types;
    const QorePluginOperation* operations;
    int num_operations;
    const QorePluginDependency* dependencies;
    int num_dependencies;
};

enum QorePluginValidationContextFlags : uint32_t {
    QORE_PLUGIN_VALIDATE_NO_FLAGS = 0,
    QORE_PLUGIN_VALIDATE_RESERVED_MASK = 0xFFFFFFFFu,
};

struct QorePluginValidationContext {
    uint32_t struct_size;
    uint32_t flags;
    const QorePluginModuleHandle* module_handle;
    const QoreProgram* program;
};

extern "C" {
DLLEXPORT int qore_validate_plugin_types_v1(
    const QorePluginTypeRegistration* reg,
    const QorePluginValidationContext* ctx,
    bool collect_all,
    ExceptionSink* xsink);

DLLEXPORT int qore_register_plugin_types_v1(
    const QorePluginRegistrationContextV1* ctx,
    const QorePluginTypeRegistration* reg,
    ExceptionSink* xsink);
}

DLLEXPORT QoreListNode* qore_plugin_get_process_modules(ExceptionSink* xsink);
DLLEXPORT QoreListNode* qore_plugin_get_process_types(const char* module_name, ExceptionSink* xsink);
DLLEXPORT QoreListNode* qore_plugin_get_process_operations(const char* module_name, ExceptionSink* xsink);
DLLEXPORT QoreHashNode* qore_plugin_resolve_process_operation(const QoreTypeInfo* lhs_type,
    const QoreTypeInfo* rhs_type, const char* operation_name, ExceptionSink* xsink);

//! Returns process-loaded plugin modules active in a Program
/** @param pgm the Program to inspect; nullptr returns an empty list
    @param xsink the exception sink
    @return a list of active plugin module names
 */
DLLEXPORT QoreListNode* qore_plugin_get_program_modules(QoreProgram* pgm, ExceptionSink* xsink);
//! Returns plugin type descriptors for a Program-active module
/** @param pgm the Program to inspect; nullptr returns an empty list for loaded modules
    @param module_name the plugin module name
    @param xsink the exception sink
    @return a list of plugin type descriptor hashes
    @throw PLUGIN-REGISTRY-MODULE-NOT-LOADED the module is not loaded in the process
 */
DLLEXPORT QoreListNode* qore_plugin_get_program_types(QoreProgram* pgm, const char* module_name,
    ExceptionSink* xsink);
//! Returns plugin operation descriptors for a Program-active module
/** @param pgm the Program to inspect; nullptr returns an empty list for loaded modules
    @param module_name the plugin module name
    @param xsink the exception sink
    @return a list of plugin operation descriptor hashes
    @throw PLUGIN-REGISTRY-MODULE-NOT-LOADED the module is not loaded in the process
 */
DLLEXPORT QoreListNode* qore_plugin_get_program_operations(QoreProgram* pgm, const char* module_name,
    ExceptionSink* xsink);
//! Resolves a plugin operation visible in a Program
/** @param pgm the Program to inspect; nullptr behaves like a Program with no active plugin modules
    @param lhs_type the left-hand operand type
    @param rhs_type the optional right-hand operand type
    @param operation_name the registered operation name
    @param xsink the exception sink
    @return the selected plugin operation descriptor hash or nullptr when no visible operation matches
 */
DLLEXPORT QoreHashNode* qore_plugin_resolve_program_operation(QoreProgram* pgm, const QoreTypeInfo* lhs_type,
    const QoreTypeInfo* rhs_type, const char* operation_name, ExceptionSink* xsink);
//! Returns recently recorded plugin fallback sites for a Program
/** @param pgm the Program to inspect; nullptr returns an empty list
    @param xsink the exception sink
    @return a list of fallback-site descriptor hashes
 */
DLLEXPORT QoreListNode* qore_plugin_get_recent_fallback_sites(QoreProgram* pgm, ExceptionSink* xsink);
//! Clears recorded plugin fallback sites for a Program
/** @param pgm the Program to update; nullptr is ignored
 */
DLLEXPORT void qore_plugin_clear_fallback_sites(QoreProgram* pgm);

#endif
