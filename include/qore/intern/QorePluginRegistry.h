/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QorePluginRegistry.h

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.
*/

#ifndef _QORE_INTERN_QOREPLUGINREGISTRY_H
#define _QORE_INTERN_QOREPLUGINREGISTRY_H

#include <qore/QorePluginType.h>

#include <cstdint>
#include <string>
#include <vector>

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
}

#endif
