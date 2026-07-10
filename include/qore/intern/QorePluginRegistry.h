/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QorePluginRegistry.h

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.
*/

#ifndef _QORE_INTERN_QOREPLUGINREGISTRY_H
#define _QORE_INTERN_QOREPLUGINREGISTRY_H

#include <qore/QorePluginType.h>
#include <qore/QoreParseOptions.h>
#include <qore/QoreValue.h>

#include <cstdint>
#include <string>
#include <vector>

struct QorePluginResolvedTypeInfo {
    std::string module_name;
    uint16_t local_type_id = 0;
    std::string type_name;
    const QoreTypeInfo* type_info = nullptr;
    QorePluginValueOps value_ops = {};
    QorePluginSerializeCallback serialize = nullptr;
    QorePluginDeserializeCallback deserialize = nullptr;
    uint16_t serializer_format_version = 0;
};

struct QorePluginSerializedValueInfo {
    std::string module_name;
    uint16_t local_type_id = 0;
    uint16_t serializer_format_version = 0;
    std::vector<uint8_t> payload;
};

struct QorePluginLLVMCodegenContext;
namespace llvm {
class Value;
}
typedef llvm::Value* (*QorePluginLLVMCodegenCallback)(QorePluginLLVMCodegenContext* ctx);

struct QorePluginLLVMCodegenInfo {
    std::string module_name;
    uint16_t local_operation_id = 0;
    std::string operation_name;
    QorePluginOperationSignature signature = {};
    QorePluginOpcodeInfoExtended info = {};
    QorePluginLLVMCodegenCallback codegen = nullptr;
};

struct QorePluginLoweringInfo {
    std::string module_name;
    uint16_t local_operation_id = 0;
    std::string operation_name;
    QorePluginOpcodeInfoExtended info = {};
    uint64_t claimed_node_kinds = 0;
    QorePluginLoweringCallback lowering = nullptr;
};

struct QorePluginResolvedOperationInfo {
    uint32_t global_operation_id = 0;
    std::string module_name;
    uint16_t local_operation_id = 0;
    std::string operation_name;
    QorePluginOperationSignature signature = {};
    QorePluginOpcodeInfoExtended info = {};
    uint8_t canonical_signature_version = QORE_PLUGIN_CANONICAL_SIGNATURE_VERSION_V1;
    uint64_t signature_hash = 0;
};

//! Returns true when both the Program and operation descriptor allow IEEE-unsafe FP reassociation.
DLLLOCAL inline bool qore_plugin_allows_fp_reassociation(const QorePluginResolvedOperationInfo& info,
        const QoreParseOptions& parse_options) {
    return info.info.fp_reassociation_allowed && parse_options.hasAll(QoreParseOptions::FP_FAST_MATH);
}

struct QorePluginModuleHandle {
    static constexpr uint64_t Magic = 0x516f7265506c6731ULL; // "QorePlg1"

    uint64_t magic = Magic;
    std::string module_name;
    std::string module_path;
    void* dl_handle = nullptr;
    uint64_t generation = 0;
    bool active = false;
    bool committed = false;

    DLLLOCAL QorePluginModuleHandle(const char* name, const char* path, void* dl_handle);
};

struct QorePluginAOTTypeInfo {
    uint16_t local_type_id = 0;
    std::string type_name;
    std::string type_path;
    uint16_t serializer_format_version = 0;
};

struct QorePluginAOTOperationInfo {
    uint16_t local_id = 0;
    std::string operation_name;
    QorePluginOperationSignature signature = {};
    uint8_t canonical_signature_version = QORE_PLUGIN_CANONICAL_SIGNATURE_VERSION_V1;
    uint64_t signature_hash = 0;
};

struct QorePluginAOTModuleInfo {
    std::string module_name;
    std::string plugin_abi_version;
    std::string operation_set_version;
    std::vector<QorePluginAOTTypeInfo> types;
    std::vector<QorePluginAOTOperationInfo> operations;
};

class QorePluginModuleInitScope {
public:
    DLLLOCAL explicit QorePluginModuleInitScope(QorePluginModuleHandle& handle);
    DLLLOCAL ~QorePluginModuleInitScope();

    DLLLOCAL QorePluginModuleInitScope(const QorePluginModuleInitScope&) = delete;
    DLLLOCAL QorePluginModuleInitScope& operator=(const QorePluginModuleInitScope&) = delete;

    DLLLOCAL void commit(ExceptionSink* xsink);
    DLLLOCAL const QorePluginModuleHandle* getHandle() const {
        return &handle;
    }

private:
    QorePluginModuleHandle& handle;
    // Previous thread-local current_plugin_module_handle; saved on entry,
    // restored on exit so binary-module init callbacks can recursively load
    // other binary modules (each push/pops its own scope LIFO).
    const QorePluginModuleHandle* prev_handle = nullptr;
    bool committed = false;
};

DLLLOCAL bool qore_plugin_is_current_module_handle(const QorePluginModuleHandle* handle);
DLLLOCAL void* qore_plugin_resolve_module_symbol(const QorePluginModuleHandle* handle, const char* symbol);
DLLLOCAL int qore_plugin_commit_module_init_registration(const QorePluginModuleHandle& handle, ExceptionSink* xsink);
DLLLOCAL void qore_plugin_rollback_module_init_registration(const QorePluginModuleHandle& handle);

DLLLOCAL int qore_plugin_get_process_operation_id(const char* module_name, uint16_t local_operation_id,
    uint32_t* global_operation_id, ExceptionSink* xsink);
DLLLOCAL int qore_plugin_get_process_operation_id_checked(const char* module_name, uint16_t local_operation_id,
    uint8_t canonical_signature_version, uint64_t signature_hash, uint32_t* global_operation_id,
    ExceptionSink* xsink);

DLLLOCAL uint64_t qore_plugin_compute_signature_hash_v1(const QorePluginOperationSignature& signature);
DLLLOCAL int qore_plugin_get_aot_module_info(const char* module_name, QorePluginAOTModuleInfo& info,
    ExceptionSink* xsink);
DLLLOCAL int qore_plugin_get_type_info(const char* module_name, uint16_t local_type_id,
    QorePluginResolvedTypeInfo& info, ExceptionSink* xsink);
DLLLOCAL int qore_plugin_get_llvm_codegen_info(uint32_t global_operation_id,
    QorePluginLLVMCodegenInfo& info, ExceptionSink* xsink);
DLLLOCAL bool qore_plugin_has_registered_operations();
DLLLOCAL int qore_plugin_get_lowering_infos(QoreProgram* pgm, qore_type_t node_type,
    std::vector<QorePluginLoweringInfo>& infos, ExceptionSink* xsink);
DLLLOCAL int qore_plugin_resolve_program_operation_info(QoreProgram* pgm, const QoreTypeInfo* lhs_type,
    const QoreTypeInfo* rhs_type, const char* operation_name, QorePluginHelperAbi helper_abi,
    QorePluginResolvedOperationInfo& info, ExceptionSink* xsink);
DLLLOCAL QoreValue qore_plugin_try_dispatch_unary(QoreProgram* pgm, const char* operation_name,
    QorePluginHelperAbi helper_abi, QoreValue value, bool& matched, ExceptionSink* xsink);
DLLLOCAL QoreValue qore_plugin_try_dispatch_binary(QoreProgram* pgm, const char* operation_name,
    QorePluginHelperAbi helper_abi, QoreValue lhs, QoreValue rhs, bool& matched, ExceptionSink* xsink);
DLLLOCAL QoreValue qore_plugin_try_dispatch_call(QoreProgram* pgm, const char* operation_name,
    QoreValue self, const QoreListNode* args, bool& matched, ExceptionSink* xsink);
DLLLOCAL inline bool qore_plugin_value_may_have_operation(QoreValue value) {
    qore_type_t type = value.getType();
    return type == NT_OBJECT || type == NT_PLUGIN_VALUE;
}
DLLLOCAL bool qore_plugin_is_value_node(const AbstractQoreNode* node);
DLLLOCAL const QoreTypeInfo* qore_plugin_get_value_type_info(const AbstractQoreNode* node);
DLLLOCAL bool qore_plugin_get_value_profile_info(const AbstractQoreNode* node, std::string& module_name,
    uint16_t& local_type_id, const QoreTypeInfo*& type_info);
DLLLOCAL int qore_plugin_serialize_value_node(const AbstractQoreNode* node,
    QorePluginSerializedValueInfo& info, ExceptionSink* xsink);
DLLLOCAL QoreValue qore_plugin_deserialize_value(const char* module_name, uint16_t local_type_id,
    uint16_t serializer_format_version, const uint8_t* payload, uint32_t payload_len, ExceptionSink* xsink);
DLLLOCAL void qore_plugin_record_fallback_site(QoreProgram* pgm, const char* file, int line,
    const char* operation_name, const QoreTypeInfo* lhs_type, const QoreTypeInfo* rhs_type, const char* reason);

extern "C" {
DLLEXPORT uint64_t qore_rt_plugin_unary(uint32_t global_operation_id, uint64_t value_bits,
    ExceptionSink* xsink);
DLLEXPORT uint64_t qore_rt_plugin_binary(uint32_t global_operation_id, uint64_t lhs_bits,
    uint64_t rhs_bits, ExceptionSink* xsink);
DLLEXPORT uint64_t qore_rt_plugin_call(uint32_t global_operation_id, uint64_t self_bits,
    uint64_t args_list_bits, ExceptionSink* xsink);
DLLEXPORT uint64_t qore_rt_plugin_call_args(uint32_t global_operation_id, uint64_t self_bits,
    const uint64_t* arg_bits, int32_t nargs, ExceptionSink* xsink);
DLLEXPORT uint64_t qore_rt_plugin_subscript(uint32_t global_operation_id, uint64_t container_bits,
    uint64_t key_bits, ExceptionSink* xsink);
DLLEXPORT uint64_t qore_rt_plugin_construct(uint32_t global_operation_id, uint64_t args_list_bits,
    ExceptionSink* xsink);
DLLEXPORT uint64_t qore_rt_plugin_construct_args(uint32_t global_operation_id, const uint64_t* arg_bits,
    int32_t nargs, ExceptionSink* xsink);
DLLEXPORT uint64_t qore_rt_plugin_dense_buffer_unary(uint32_t global_operation_id, void* result_buffer_data,
    int64_t result_size, const void* value_data, int64_t value_size, int64_t value_stride, ExceptionSink* xsink);
DLLEXPORT uint64_t qore_rt_plugin_dense_buffer_binary(uint32_t global_operation_id, void* result_buffer_data,
    int64_t result_size, const void* lhs_data, int64_t lhs_size, int64_t lhs_stride, const void* rhs_data,
    int64_t rhs_size, int64_t rhs_stride, ExceptionSink* xsink);
DLLEXPORT uint64_t qore_rt_plugin_dense_buffer_unary_values(uint32_t global_operation_id,
    uint64_t result_buffer_bits, uint64_t value_bits, ExceptionSink* xsink);
DLLEXPORT uint64_t qore_rt_plugin_dense_buffer_binary_values(uint32_t global_operation_id,
    uint64_t result_buffer_bits, uint64_t lhs_bits, uint64_t rhs_bits, ExceptionSink* xsink);
DLLEXPORT int64_t qore_rt_guard_plugin_type_profiled(uint64_t value_bits, const QoreTypeInfo* type_info,
    const char* module_name, uint32_t local_type_id);
}

#endif
