/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QoreProtobufSchema.cpp QoreProtobufSchema implementation */
/*
    Qore protobuf module

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

#include "protobuf-module.h"
#include "QoreProtobufSchema.h"
#include "ProtobufHelper.h"

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/util/json_util.h>

#include <map>
#include <set>

// ErrorCollector implementation
#ifdef QORE_PROTOBUF_V22_PLUS
void QoreProtobufSchema::ErrorCollector::RecordError(absl::string_view filename, int line,
        int column, absl::string_view message) {
    errors.push_back(std::string(filename) + ":" + std::to_string(line + 1) + ":" +
        std::to_string(column + 1) + ": " + std::string(message));
}

void QoreProtobufSchema::ErrorCollector::RecordWarning(absl::string_view filename, int line,
        int column, absl::string_view message) {
    // Ignore warnings
}
#else
void QoreProtobufSchema::ErrorCollector::AddError(const std::string& filename, int line,
        int column, const std::string& message) {
    errors.push_back(filename + ":" + std::to_string(line + 1) + ":" +
        std::to_string(column + 1) + ": " + message);
}

void QoreProtobufSchema::ErrorCollector::AddWarning(const std::string& filename, int line,
        int column, const std::string& message) {
    // Ignore warnings
}
#endif

std::string QoreProtobufSchema::ErrorCollector::getErrors() const {
    std::string result;
    for (const auto& err : errors) {
        if (!result.empty()) {
            result += "\n";
        }
        result += err;
    }
    return result;
}

// StringSourceTree implementation
void QoreProtobufSchema::StringSourceTree::addFile(const std::string& filename,
        const std::string& content) {
    files[filename] = content;
}

#ifdef QORE_PROTOBUF_V22_PLUS
google::protobuf::io::ZeroCopyInputStream* QoreProtobufSchema::StringSourceTree::Open(
        absl::string_view filename) {
    std::string fname(filename);
    auto it = files.find(fname);
    if (it == files.end()) {
        last_error = "file not found: " + fname;
        return nullptr;
    }
    return new google::protobuf::io::ArrayInputStream(it->second.data(), it->second.size());
}
#else
google::protobuf::io::ZeroCopyInputStream* QoreProtobufSchema::StringSourceTree::Open(
        const std::string& filename) {
    auto it = files.find(filename);
    if (it == files.end()) {
        last_error = "file not found: " + filename;
        return nullptr;
    }
    return new google::protobuf::io::ArrayInputStream(it->second.data(), it->second.size());
}
#endif

std::string QoreProtobufSchema::StringSourceTree::GetLastErrorMessage() {
    return last_error;
}

// DescriptorErrorCollector implementation
#ifdef QORE_PROTOBUF_V22_PLUS
void QoreProtobufSchema::DescriptorErrorCollector::RecordError(absl::string_view filename,
        absl::string_view element_name,
        const google::protobuf::Message* descriptor, ErrorLocation location,
        absl::string_view message) {
    errors.push_back(std::string(filename) + ": " + std::string(element_name) + ": "
        + std::string(message));
}
#else
void QoreProtobufSchema::DescriptorErrorCollector::AddError(const std::string& filename,
        const std::string& element_name,
        const google::protobuf::Message* descriptor, ErrorLocation location,
        const std::string& message) {
    errors.push_back(filename + ": " + element_name + ": " + message);
}
#endif

std::string QoreProtobufSchema::DescriptorErrorCollector::getErrors() const {
    std::string result;
    for (const auto& err : errors) {
        if (!result.empty()) {
            result += "\n";
        }
        result += err;
    }
    return result;
}

// QoreProtobufSchema constructors
QoreProtobufSchema::QoreProtobufSchema(const char* path, const char* proto_file,
        ExceptionSink* xsink) {
    // Check for thread cancellation / program interrupt before I/O
    if (qore_check_cancel(xsink, "loading .proto file")) {
        return;
    }

    // Check sandbox filesystem restrictions before accessing disk
    QoreSandboxManagerHelper smh;
    if (smh) {
        if (!smh->checkFilesystemAccess(path, QSEC_READ, xsink)) {
            return;
        }
    }

    error_collector = std::make_unique<ErrorCollector>();
    disk_source_tree = std::make_unique<google::protobuf::compiler::DiskSourceTree>();
    disk_source_tree->MapPath("", path);

    importer = std::make_unique<google::protobuf::compiler::Importer>(
        disk_source_tree.get(), error_collector.get());

    file_desc = importer->Import(proto_file);
    if (!file_desc) {
        xsink->raiseException("PROTOBUF-SCHEMA-ERROR", "failed to load proto file '%s': %s",
            proto_file, error_collector->getErrors().c_str());
        return;
    }

    factory = std::make_unique<google::protobuf::DynamicMessageFactory>(importer->pool());
}

QoreProtobufSchema::QoreProtobufSchema(const QoreString& proto_content, const char* filename,
        ExceptionSink* xsink) {
    error_collector = std::make_unique<ErrorCollector>();
    string_source_tree = std::make_unique<StringSourceTree>();

    const char* fname = filename && filename[0] ? filename : "input.proto";
    string_source_tree->addFile(fname, std::string(proto_content.c_str(), proto_content.size()));

    importer = std::make_unique<google::protobuf::compiler::Importer>(
        string_source_tree.get(), error_collector.get());

    file_desc = importer->Import(fname);
    if (!file_desc) {
        xsink->raiseException("PROTOBUF-SCHEMA-ERROR", "failed to parse proto content: %s",
            error_collector->getErrors().c_str());
        return;
    }

    factory = std::make_unique<google::protobuf::DynamicMessageFactory>(importer->pool());
}

QoreProtobufSchema::QoreProtobufSchema(const QoreListNode* serialized_fds, ExceptionSink* xsink) {
    if (!serialized_fds || !serialized_fds->size()) {
        xsink->raiseException("PROTOBUF-SCHEMA-ERROR",
            "serialized descriptor list is empty");
        return;
    }

    desc_error_collector = std::make_unique<DescriptorErrorCollector>();
    standalone_pool = std::make_unique<google::protobuf::DescriptorPool>(
        google::protobuf::DescriptorPool::generated_pool());

    // Deserialize all FileDescriptorProto messages first
    std::vector<google::protobuf::FileDescriptorProto> protos;
    protos.reserve(serialized_fds->size());

    for (size_t i = 0; i < serialized_fds->size(); ++i) {
        QoreValue val = serialized_fds->retrieveEntry(i);
        const BinaryNode* bin = val.get<BinaryNode>();
        if (!bin) {
            xsink->raiseException("PROTOBUF-SCHEMA-ERROR",
                "element %d in descriptor list is not binary (got %s)",
                (int)i, val.getFullTypeName());
            return;
        }

        google::protobuf::FileDescriptorProto fdp;
        if (!fdp.ParseFromArray(bin->getPtr(), bin->size())) {
            xsink->raiseException("PROTOBUF-SCHEMA-ERROR",
                "failed to deserialize FileDescriptorProto at index %d", (int)i);
            return;
        }
        protos.push_back(std::move(fdp));
    }

    // Resolve duplicate file names by appending a numeric suffix.
    // This happens when combining serialized descriptors from multiple ProtobufSchema
    // instances that were created from strings (all default to "input.proto").
    {
        std::map<std::string, int> name_counts;
        for (auto& p : protos) {
            int count = ++name_counts[p.name()];
            if (count > 1) {
                // Rename duplicate: "input.proto" -> "input_2.proto"
                std::string orig = p.name();
                std::string base = orig;
                std::string ext;
                size_t dot = orig.rfind('.');
                if (dot != std::string::npos) {
                    base = orig.substr(0, dot);
                    ext = orig.substr(dot);
                }
                p.set_name(base + "_" + std::to_string(count) + ext);
            }
        }
    }

    // Build files in dependency order: try to build each proto; if it fails because
    // a dependency hasn't been built yet, retry after building others.
    // This handles arbitrary dependency ordering in the input list.
    std::set<std::string> built;

    while (built.size() < protos.size()) {
        bool progress = false;
        for (size_t i = 0; i < protos.size(); ++i) {
            const std::string& name = protos[i].name();
            if (built.count(name)) {
                continue;
            }

            // Check if all dependencies are built
            bool deps_ready = true;
            for (int j = 0; j < protos[i].dependency_size(); ++j) {
                const std::string& dep = protos[i].dependency(j);
                // Dependencies in the generated pool (e.g., google/protobuf/*.proto) are always ready
                if (!built.count(dep)
                        && !google::protobuf::DescriptorPool::generated_pool()->FindFileByName(dep)) {
                    deps_ready = false;
                    break;
                }
            }

            if (!deps_ready) {
                continue;
            }

            const google::protobuf::FileDescriptor* fd = standalone_pool->BuildFileCollectingErrors(
                protos[i], desc_error_collector.get());
            if (!fd) {
                xsink->raiseException("PROTOBUF-SCHEMA-ERROR",
                    "failed to build descriptor for '%s': %s",
                    name.c_str(), desc_error_collector->getErrors().c_str());
                return;
            }

            file_descs.push_back(fd);
            built.insert(name);
            progress = true;
        }

        if (!progress) {
            // No progress means there's a circular or unresolvable dependency
            std::string missing;
            for (size_t i = 0; i < protos.size(); ++i) {
                if (!built.count(protos[i].name())) {
                    if (!missing.empty()) {
                        missing += ", ";
                    }
                    missing += protos[i].name();
                }
            }
            xsink->raiseException("PROTOBUF-SCHEMA-ERROR",
                "unresolvable dependencies for: %s", missing.c_str());
            return;
        }
    }

    // Use the last file descriptor as the primary one
    if (!file_descs.empty()) {
        file_desc = file_descs.back();
    }

    factory = std::make_unique<google::protobuf::DynamicMessageFactory>(standalone_pool.get());
}

QoreProtobufSchema::~QoreProtobufSchema() {
}

const google::protobuf::DescriptorPool* QoreProtobufSchema::getPool() const {
    if (standalone_pool) {
        return standalone_pool.get();
    }
    assert(importer);
    return importer->pool();
}

const google::protobuf::Descriptor* QoreProtobufSchema::findMessageDescriptor(const char* type,
        ExceptionSink* xsink) const {
    assert(file_desc);
    const google::protobuf::DescriptorPool* pool = getPool();
    const google::protobuf::Descriptor* desc = pool->FindMessageTypeByName(type);
    if (!desc) {
        // Try prepending the package name
        std::string pkg(file_desc->package());
        if (!pkg.empty()) {
            desc = pool->FindMessageTypeByName(pkg + "." + type);
        }
    }
    if (!desc) {
        xsink->raiseException("PROTOBUF-TYPE-ERROR", "message type '%s' not found in schema", type);
    }
    return desc;
}

const google::protobuf::Message* QoreProtobufSchema::getPrototype(
        const google::protobuf::Descriptor* desc, ExceptionSink* xsink) const {
    assert(factory);
    const google::protobuf::Message* proto = factory->GetPrototype(desc);
    if (!proto) {
        xsink->raiseException("PROTOBUF-ERROR", "failed to create prototype for message type '%s'",
            std::string(desc->full_name()).c_str());
    }
    return proto;
}

QoreHashNode* QoreProtobufSchema::buildServiceInfo(
        const google::protobuf::ServiceDescriptor* svc, ExceptionSink* xsink) const {
    ReferenceHolder<QoreHashNode> svc_hash(new QoreHashNode(hashdeclProtobufServiceInfo, xsink), xsink);

    svc_hash->setKeyValue("name", new QoreStringNode(std::string(svc->name())), xsink);

    ReferenceHolder<QoreListNode> methods(
        new QoreListNode(hashdeclProtobufMethodInfo->getTypeInfo()), xsink);
    for (int j = 0; j < svc->method_count(); ++j) {
        const google::protobuf::MethodDescriptor* method = svc->method(j);
        ReferenceHolder<QoreHashNode> method_hash(
            new QoreHashNode(hashdeclProtobufMethodInfo, xsink), xsink);

        method_hash->setKeyValue("name", new QoreStringNode(std::string(method->name())), xsink);
        method_hash->setKeyValue("full_path",
            new QoreStringNode(std::string("/") + std::string(svc->full_name()) + "/"
                + std::string(method->name())),
            xsink);
        method_hash->setKeyValue("input_type",
            new QoreStringNode(std::string(method->input_type()->full_name())), xsink);
        method_hash->setKeyValue("output_type",
            new QoreStringNode(std::string(method->output_type()->full_name())), xsink);
        method_hash->setKeyValue("client_streaming", method->client_streaming(), xsink);
        method_hash->setKeyValue("server_streaming", method->server_streaming(), xsink);

        methods->push(method_hash.release(), xsink);
    }

    svc_hash->setKeyValue("methods", methods.release(), xsink);
    return svc_hash.release();
}

QoreListNode* QoreProtobufSchema::getServices(ExceptionSink* xsink) const {
    assert(file_desc);
    ReferenceHolder<QoreListNode> list(new QoreListNode(hashdeclProtobufServiceInfo->getTypeInfo()), xsink);

    if (!file_descs.empty()) {
        // Multi-file descriptor constructor: iterate all files
        for (const auto* fd : file_descs) {
            for (int i = 0; i < fd->service_count(); ++i) {
                QoreHashNode* svc_hash = buildServiceInfo(fd->service(i), xsink);
                if (*xsink) {
                    return nullptr;
                }
                list->push(svc_hash, xsink);
            }
        }
    } else {
        // Single file descriptor
        for (int i = 0; i < file_desc->service_count(); ++i) {
            QoreHashNode* svc_hash = buildServiceInfo(file_desc->service(i), xsink);
            if (*xsink) {
                return nullptr;
            }
            list->push(svc_hash, xsink);
        }
    }

    return list.release();
}

QoreListNode* QoreProtobufSchema::getMessageTypes(ExceptionSink* xsink) const {
    assert(file_desc);
    ReferenceHolder<QoreListNode> list(new QoreListNode(stringTypeInfo), xsink);

    if (!file_descs.empty()) {
        // Multi-file descriptor constructor: iterate all files
        for (const auto* fd : file_descs) {
            for (int i = 0; i < fd->message_type_count(); ++i) {
                list->push(new QoreStringNode(std::string(fd->message_type(i)->full_name())), xsink);
            }
        }
    } else {
        for (int i = 0; i < file_desc->message_type_count(); ++i) {
            list->push(new QoreStringNode(std::string(file_desc->message_type(i)->full_name())), xsink);
        }
    }

    return list.release();
}

QoreHashNode* QoreProtobufSchema::getDefaultMessage(const char* type, ExceptionSink* xsink) const {
    const google::protobuf::Descriptor* desc = findMessageDescriptor(type, xsink);
    if (!desc) {
        return nullptr;
    }

    const google::protobuf::Message* proto = getPrototype(desc, xsink);
    if (!proto) {
        return nullptr;
    }

    std::unique_ptr<google::protobuf::Message> msg(proto->New());
    return ProtobufHelper::messageToHash(*msg, xsink);
}

BinaryNode* QoreProtobufSchema::encode(const char* type, const QoreHashNode* data,
        ExceptionSink* xsink) const {
    const google::protobuf::Descriptor* desc = findMessageDescriptor(type, xsink);
    if (!desc) {
        return nullptr;
    }

    const google::protobuf::Message* proto = getPrototype(desc, xsink);
    if (!proto) {
        return nullptr;
    }

    std::unique_ptr<google::protobuf::Message> msg(proto->New());
    if (!ProtobufHelper::hashToMessage(data, msg.get(), xsink)) {
        return nullptr;
    }

    std::string serialized;
    if (!msg->SerializeToString(&serialized)) {
        xsink->raiseException("PROTOBUF-ENCODE-ERROR", "failed to serialize message of type '%s'",
            type);
        return nullptr;
    }

    SimpleRefHolder<BinaryNode> bin(new BinaryNode);
    bin->append(serialized.data(), serialized.size());
    return bin.release();
}

QoreHashNode* QoreProtobufSchema::decode(const char* type, const BinaryNode* data,
        ExceptionSink* xsink) const {
    const google::protobuf::Descriptor* desc = findMessageDescriptor(type, xsink);
    if (!desc) {
        return nullptr;
    }

    const google::protobuf::Message* proto = getPrototype(desc, xsink);
    if (!proto) {
        return nullptr;
    }

    std::unique_ptr<google::protobuf::Message> msg(proto->New());
    if (!msg->ParseFromArray(data->getPtr(), data->size())) {
        xsink->raiseException("PROTOBUF-DECODE-ERROR",
            "failed to parse binary data as message type '%s'", type);
        return nullptr;
    }

    return ProtobufHelper::messageToHash(*msg, xsink);
}

QoreStringNode* QoreProtobufSchema::toJson(const char* type, const QoreHashNode* data,
        ExceptionSink* xsink) const {
    const google::protobuf::Descriptor* desc = findMessageDescriptor(type, xsink);
    if (!desc) {
        return nullptr;
    }

    const google::protobuf::Message* proto = getPrototype(desc, xsink);
    if (!proto) {
        return nullptr;
    }

    std::unique_ptr<google::protobuf::Message> msg(proto->New());
    if (!ProtobufHelper::hashToMessage(data, msg.get(), xsink)) {
        return nullptr;
    }

    std::string json;
    google::protobuf::util::JsonPrintOptions opts;
    opts.add_whitespace = false;
#ifdef QORE_PROTOBUF_V22_PLUS
    opts.always_print_fields_with_no_presence = true;
#else
    opts.always_print_primitive_fields = true;
#endif

    auto status = google::protobuf::util::MessageToJsonString(*msg, &json, opts);
    if (!status.ok()) {
        xsink->raiseException("PROTOBUF-JSON-ERROR", "failed to convert message to JSON: %s",
            status.ToString().c_str());
        return nullptr;
    }

    return new QoreStringNode(json);
}

QoreHashNode* QoreProtobufSchema::fromJson(const char* type, const QoreString& json,
        ExceptionSink* xsink) const {
    const google::protobuf::Descriptor* desc = findMessageDescriptor(type, xsink);
    if (!desc) {
        return nullptr;
    }

    const google::protobuf::Message* proto = getPrototype(desc, xsink);
    if (!proto) {
        return nullptr;
    }

    std::unique_ptr<google::protobuf::Message> msg(proto->New());

    google::protobuf::util::JsonParseOptions opts;
    opts.ignore_unknown_fields = false;

    auto status = google::protobuf::util::JsonStringToMessage(
        std::string(json.c_str(), json.size()), msg.get(), opts);
    if (!status.ok()) {
        xsink->raiseException("PROTOBUF-JSON-ERROR", "failed to parse JSON as message type '%s': %s",
            type, status.ToString().c_str());
        return nullptr;
    }

    return ProtobufHelper::messageToHash(*msg, xsink);
}

QoreListNode* QoreProtobufSchema::serializeFileDescriptors(ExceptionSink* xsink) const {
    assert(file_desc);
    ReferenceHolder<QoreListNode> list(new QoreListNode(binaryTypeInfo), xsink);

    // Collect all file descriptors
    std::set<const google::protobuf::FileDescriptor*> all_fds;
    if (!file_descs.empty()) {
        for (auto* fd : file_descs) {
            all_fds.insert(fd);
        }
    } else {
        all_fds.insert(file_desc);
    }

    // Add transitive dependencies
    std::vector<const google::protobuf::FileDescriptor*> work(all_fds.begin(), all_fds.end());
    for (size_t i = 0; i < work.size(); ++i) {
        for (int j = 0; j < work[i]->dependency_count(); ++j) {
            auto* dep = work[i]->dependency(j);
            if (all_fds.insert(dep).second) {
                work.push_back(dep);
            }
        }
    }

    // Serialize each descriptor
    for (auto* fd : all_fds) {
        google::protobuf::FileDescriptorProto fdp;
        fd->CopyTo(&fdp);
        std::string data;
        if (!fdp.SerializeToString(&data)) {
            xsink->raiseException("PROTOBUF-SCHEMA-ERROR",
                "failed to serialize FileDescriptorProto for '%s'", std::string(fd->name()).c_str());
            return nullptr;
        }
        SimpleRefHolder<BinaryNode> bin(new BinaryNode);
        bin->append(data.data(), data.size());
        list->push(bin.release(), xsink);
    }

    return list.release();
}

QoreListNode* QoreProtobufSchema::serializeFileWithDeps(
        const google::protobuf::FileDescriptor* fd, ExceptionSink* xsink) const {
    ReferenceHolder<QoreListNode> list(new QoreListNode(binaryTypeInfo), xsink);

    // Collect the file and its transitive dependencies via BFS
    std::set<const google::protobuf::FileDescriptor*> seen;
    std::vector<const google::protobuf::FileDescriptor*> work;
    seen.insert(fd);
    work.push_back(fd);

    for (size_t i = 0; i < work.size(); ++i) {
        for (int j = 0; j < work[i]->dependency_count(); ++j) {
            auto* dep = work[i]->dependency(j);
            if (seen.insert(dep).second) {
                work.push_back(dep);
            }
        }
    }

    // Serialize in reverse BFS order so dependencies appear before dependents;
    // this allows receivers to build descriptors in list order
    for (auto it = work.rbegin(); it != work.rend(); ++it) {
        google::protobuf::FileDescriptorProto fdp;
        (*it)->CopyTo(&fdp);
        std::string data;
        if (!fdp.SerializeToString(&data)) {
            xsink->raiseException("PROTOBUF-SCHEMA-ERROR",
                "failed to serialize FileDescriptorProto for '%s'",
                std::string((*it)->name()).c_str());
            return nullptr;
        }
        SimpleRefHolder<BinaryNode> bin(new BinaryNode);
        bin->append(data.data(), data.size());
        list->push(bin.release(), xsink);
    }

    return list.release();
}

QoreListNode* QoreProtobufSchema::serializeFileDescriptorForSymbol(const char* symbol,
        ExceptionSink* xsink) const {
    const google::protobuf::DescriptorPool* pool = getPool();

    // FindFileContainingSymbol handles services, messages, enums, methods, etc.
    const google::protobuf::FileDescriptor* fd = pool->FindFileContainingSymbol(symbol);
    if (!fd) {
        xsink->raiseException("PROTOBUF-SYMBOL-NOT-FOUND",
            "symbol '%s' not found in schema", symbol);
        return nullptr;
    }

    return serializeFileWithDeps(fd, xsink);
}

QoreListNode* QoreProtobufSchema::serializeFileDescriptorByName(const char* filename,
        ExceptionSink* xsink) const {
    const google::protobuf::DescriptorPool* pool = getPool();
    const google::protobuf::FileDescriptor* fd = pool->FindFileByName(filename);
    if (!fd) {
        xsink->raiseException("PROTOBUF-FILE-NOT-FOUND",
            "file '%s' not found in schema", filename);
        return nullptr;
    }

    return serializeFileWithDeps(fd, xsink);
}

const char* QoreProtobufSchema::fieldTypeName(google::protobuf::FieldDescriptor::Type type) {
    using FD = google::protobuf::FieldDescriptor;
    switch (type) {
        case FD::TYPE_DOUBLE:   return "double";
        case FD::TYPE_FLOAT:    return "float";
        case FD::TYPE_INT64:    return "int64";
        case FD::TYPE_UINT64:   return "uint64";
        case FD::TYPE_INT32:    return "int32";
        case FD::TYPE_FIXED64:  return "fixed64";
        case FD::TYPE_FIXED32:  return "fixed32";
        case FD::TYPE_BOOL:     return "bool";
        case FD::TYPE_STRING:   return "string";
        case FD::TYPE_GROUP:    return "group";
        case FD::TYPE_MESSAGE:  return "message";
        case FD::TYPE_BYTES:    return "bytes";
        case FD::TYPE_UINT32:   return "uint32";
        case FD::TYPE_ENUM:     return "enum";
        case FD::TYPE_SFIXED32: return "sfixed32";
        case FD::TYPE_SFIXED64: return "sfixed64";
        case FD::TYPE_SINT32:   return "sint32";
        case FD::TYPE_SINT64:   return "sint64";
        default:                return "unknown";
    }
}

#ifndef QORE_PROTOBUF_V22_PLUS
const char* QoreProtobufSchema::fieldLabelName(google::protobuf::FieldDescriptor::Label label) {
    using FD = google::protobuf::FieldDescriptor;
    switch (label) {
        case FD::LABEL_OPTIONAL: return "optional";
        case FD::LABEL_REQUIRED: return "required";
        case FD::LABEL_REPEATED: return "repeated";
        default:                 return "unknown";
    }
}
#endif

QoreHashNode* QoreProtobufSchema::buildFieldInfo(const google::protobuf::FieldDescriptor* field,
        ExceptionSink* xsink) const {
    ReferenceHolder<QoreHashNode> info(new QoreHashNode(autoTypeInfo), xsink);

    if (field->is_map()) {
        info->setKeyValue("type", new QoreStringNode("map"), xsink);
        info->setKeyValue("number", (int64)field->number(), xsink);

        // Map key type
        const google::protobuf::FieldDescriptor* key_field = field->message_type()->map_key();
        info->setKeyValue("key_type", new QoreStringNode(fieldTypeName(key_field->type())), xsink);

        // Map value type
        const google::protobuf::FieldDescriptor* value_field = field->message_type()->map_value();
        info->setKeyValue("value_type", new QoreStringNode(fieldTypeName(value_field->type())), xsink);

        if (value_field->type() == google::protobuf::FieldDescriptor::TYPE_MESSAGE) {
            info->setKeyValue("value_message_type",
                new QoreStringNode(std::string(value_field->message_type()->full_name())), xsink);
        }
        if (value_field->type() == google::protobuf::FieldDescriptor::TYPE_ENUM) {
            info->setKeyValue("value_enum_type",
                new QoreStringNode(std::string(value_field->enum_type()->full_name())), xsink);
        }
    } else {
        info->setKeyValue("type", new QoreStringNode(fieldTypeName(field->type())), xsink);
        info->setKeyValue("number", (int64)field->number(), xsink);
#ifdef QORE_PROTOBUF_V22_PLUS
        const char* label_str = field->is_repeated() ? "repeated"
            : field->is_required() ? "required" : "optional";
        info->setKeyValue("label", new QoreStringNode(label_str), xsink);
#else
        info->setKeyValue("label", new QoreStringNode(fieldLabelName(field->label())), xsink);
#endif

        if (field->type() == google::protobuf::FieldDescriptor::TYPE_MESSAGE) {
            info->setKeyValue("message_type",
                new QoreStringNode(std::string(field->message_type()->full_name())), xsink);
        }

        if (field->type() == google::protobuf::FieldDescriptor::TYPE_ENUM) {
            info->setKeyValue("enum_type",
                new QoreStringNode(std::string(field->enum_type()->full_name())), xsink);

            // Include enum values inline
            const google::protobuf::EnumDescriptor* edesc = field->enum_type();
            ReferenceHolder<QoreHashNode> enum_vals(new QoreHashNode(autoTypeInfo), xsink);
            for (int i = 0; i < edesc->value_count(); ++i) {
                const google::protobuf::EnumValueDescriptor* ev = edesc->value(i);
                enum_vals->setKeyValue(std::string(ev->name()), (int64)ev->number(), xsink);
            }
            info->setKeyValue("enum_values", enum_vals.release(), xsink);
        }

        // oneof info
        if (field->containing_oneof()) {
            info->setKeyValue("oneof",
                new QoreStringNode(std::string(field->containing_oneof()->name())), xsink);
        }
    }

    return info.release();
}

QoreHashNode* QoreProtobufSchema::getMessageSchema(const char* type, ExceptionSink* xsink) const {
    const google::protobuf::Descriptor* desc = findMessageDescriptor(type, xsink);
    if (!desc) {
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), xsink);
    result->setKeyValue("name", new QoreStringNode(std::string(desc->full_name())), xsink);

    ReferenceHolder<QoreHashNode> fields(new QoreHashNode(autoTypeInfo), xsink);
    for (int i = 0; i < desc->field_count(); ++i) {
        const google::protobuf::FieldDescriptor* field = desc->field(i);
        QoreHashNode* field_info = buildFieldInfo(field, xsink);
        if (*xsink) {
            return nullptr;
        }
        fields->setKeyValue(std::string(field->name()), field_info, xsink);
    }

    result->setKeyValue("fields", fields.release(), xsink);
    return result.release();
}

QoreHashNode* QoreProtobufSchema::getEnumValues(const char* enum_type, ExceptionSink* xsink) const {
    assert(file_desc);
    const google::protobuf::DescriptorPool* pool = getPool();
    const google::protobuf::EnumDescriptor* edesc = pool->FindEnumTypeByName(enum_type);
    if (!edesc) {
        // Try prepending the package name
        std::string pkg(file_desc->package());
        if (!pkg.empty()) {
            edesc = pool->FindEnumTypeByName(pkg + "." + enum_type);
        }
    }
    if (!edesc) {
        xsink->raiseException("PROTOBUF-TYPE-ERROR", "enum type '%s' not found in schema", enum_type);
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), xsink);
    for (int i = 0; i < edesc->value_count(); ++i) {
        const google::protobuf::EnumValueDescriptor* ev = edesc->value(i);
        result->setKeyValue(std::string(ev->name()), (int64)ev->number(), xsink);
    }

    return result.release();
}

// Programmatic schema definition constructor
QoreProtobufSchema::QoreProtobufSchema(const QoreHashNode* schema_def, ExceptionSink* xsink) {
    buildFromHash(schema_def, xsink);
}

// Scalar type names recognized by the programmatic constructor
static const std::map<std::string, google::protobuf::FieldDescriptorProto::Type> scalar_type_map = {
    {"double",   google::protobuf::FieldDescriptorProto::TYPE_DOUBLE},
    {"float",    google::protobuf::FieldDescriptorProto::TYPE_FLOAT},
    {"int64",    google::protobuf::FieldDescriptorProto::TYPE_INT64},
    {"uint64",   google::protobuf::FieldDescriptorProto::TYPE_UINT64},
    {"int32",    google::protobuf::FieldDescriptorProto::TYPE_INT32},
    {"fixed64",  google::protobuf::FieldDescriptorProto::TYPE_FIXED64},
    {"fixed32",  google::protobuf::FieldDescriptorProto::TYPE_FIXED32},
    {"bool",     google::protobuf::FieldDescriptorProto::TYPE_BOOL},
    {"string",   google::protobuf::FieldDescriptorProto::TYPE_STRING},
    {"bytes",    google::protobuf::FieldDescriptorProto::TYPE_BYTES},
    {"uint32",   google::protobuf::FieldDescriptorProto::TYPE_UINT32},
    {"sfixed32", google::protobuf::FieldDescriptorProto::TYPE_SFIXED32},
    {"sfixed64", google::protobuf::FieldDescriptorProto::TYPE_SFIXED64},
    {"sint32",   google::protobuf::FieldDescriptorProto::TYPE_SINT32},
    {"sint64",   google::protobuf::FieldDescriptorProto::TYPE_SINT64},
};

//! Resolve a field type string to a FieldDescriptorProto::Type
/** @param type_str the type name string
    @param is_reference set to true if the type is a message/enum reference (not a scalar)
    @return the protobuf field type
*/
static google::protobuf::FieldDescriptorProto::Type resolveFieldType(const char* type_str,
        bool& is_reference) {
    auto it = scalar_type_map.find(type_str);
    if (it != scalar_type_map.end()) {
        is_reference = false;
        return it->second;
    }
    // Not a scalar type — treat as a message or enum type reference
    is_reference = true;
    return google::protobuf::FieldDescriptorProto::TYPE_MESSAGE;
}

static google::protobuf::FieldDescriptorProto::Label resolveFieldLabel(const char* label_str) {
    using FDP = google::protobuf::FieldDescriptorProto;
    if (!strcmp(label_str, "repeated")) return FDP::LABEL_REPEATED;
    if (!strcmp(label_str, "required")) return FDP::LABEL_REQUIRED;
    return FDP::LABEL_OPTIONAL;
}

// Check if a type name matches a defined enum in the message or top-level schema
static bool isEnumType(const QoreHashNode* msg_def, const char* type_name) {
    // Check nested enums in this message
    QoreValue enums_val = msg_def->getKeyValue("enums");
    if (!enums_val.isNullOrNothing()) {
        const QoreHashNode* enums = enums_val.get<QoreHashNode>();
        if (enums && enums->existsKey(type_name)) {
            return true;
        }
    }
    return false;
}

void QoreProtobufSchema::buildMessage(google::protobuf::DescriptorProto* msg,
        const QoreHashNode* msg_def, ExceptionSink* xsink) {
    // Process fields
    QoreValue fields_val = msg_def->getKeyValue("fields");
    if (!fields_val.isNullOrNothing()) {
        const QoreListNode* fields = fields_val.get<QoreListNode>();
        if (!fields) {
            xsink->raiseException("PROTOBUF-SCHEMA-ERROR",
                "'fields' must be a list of field definition hashes");
            return;
        }
        for (size_t i = 0; i < fields->size(); ++i) {
            QoreValue fval = fields->retrieveEntry(i);
            const QoreHashNode* field_def = fval.get<QoreHashNode>();
            if (!field_def) {
                xsink->raiseException("PROTOBUF-SCHEMA-ERROR",
                    "field definition at index %d must be a hash", (int)i);
                return;
            }

            google::protobuf::FieldDescriptorProto* fdp = msg->add_field();

            // name (required)
            QoreValue name_val = field_def->getKeyValue("name");
            if (name_val.isNullOrNothing()) {
                xsink->raiseException("PROTOBUF-SCHEMA-ERROR",
                    "field definition at index %d missing required 'name'", (int)i);
                return;
            }
            QoreStringValueHelper name_str(name_val);
            fdp->set_name(name_str->c_str());

            // number (required)
            QoreValue number_val = field_def->getKeyValue("number");
            if (number_val.isNullOrNothing()) {
                xsink->raiseException("PROTOBUF-SCHEMA-ERROR",
                    "field '%s' missing required 'number'", name_str->c_str());
                return;
            }
            fdp->set_number((int)number_val.getAsBigInt());

            // type (required)
            QoreValue type_val = field_def->getKeyValue("type");
            if (type_val.isNullOrNothing()) {
                xsink->raiseException("PROTOBUF-SCHEMA-ERROR",
                    "field '%s' missing required 'type'", name_str->c_str());
                return;
            }
            QoreStringValueHelper type_str(type_val);
            bool is_reference = false;
            auto ftype = resolveFieldType(type_str->c_str(), is_reference);
            if (is_reference && isEnumType(msg_def, type_str->c_str())) {
                ftype = google::protobuf::FieldDescriptorProto::TYPE_ENUM;
            }
            fdp->set_type(ftype);
            if (is_reference) {
                // Message or enum reference — set type_name for protobuf to resolve
                fdp->set_type_name(type_str->c_str());
            }

            // label (optional, defaults to "optional")
            QoreValue label_val = field_def->getKeyValue("label");
            if (!label_val.isNullOrNothing()) {
                QoreStringValueHelper label_str(label_val);
                fdp->set_label(resolveFieldLabel(label_str->c_str()));
            } else {
                fdp->set_label(google::protobuf::FieldDescriptorProto::LABEL_OPTIONAL);
            }

            // Check for map field: "map_key_type" and "map_value_type" present
            QoreValue map_key_val = field_def->getKeyValue("map_key_type");
            QoreValue map_val_val = field_def->getKeyValue("map_value_type");
            if (!map_key_val.isNullOrNothing() && !map_val_val.isNullOrNothing()) {
                // Map fields are implemented as repeated fields of a synthetic MapEntry message
                QoreStringValueHelper map_key_str(map_key_val);
                QoreStringValueHelper map_val_str(map_val_val);

                // Build the synthetic MapEntry nested message
                std::string entry_name = std::string(name_str->c_str()) + "Entry";
                google::protobuf::DescriptorProto* entry = msg->add_nested_type();
                entry->set_name(entry_name);

                // Key field (always field number 1)
                google::protobuf::FieldDescriptorProto* key_fdp = entry->add_field();
                key_fdp->set_name("key");
                key_fdp->set_number(1);
                key_fdp->set_label(google::protobuf::FieldDescriptorProto::LABEL_OPTIONAL);
                bool key_is_ref = false;
                key_fdp->set_type(resolveFieldType(map_key_str->c_str(), key_is_ref));

                // Value field (always field number 2)
                google::protobuf::FieldDescriptorProto* val_fdp = entry->add_field();
                val_fdp->set_name("value");
                val_fdp->set_number(2);
                val_fdp->set_label(google::protobuf::FieldDescriptorProto::LABEL_OPTIONAL);
                bool val_is_ref = false;
                val_fdp->set_type(resolveFieldType(map_val_str->c_str(), val_is_ref));
                if (val_is_ref) {
                    val_fdp->set_type_name(map_val_str->c_str());
                }

                // The map field itself is a repeated field of the entry type
                fdp->set_type(google::protobuf::FieldDescriptorProto::TYPE_MESSAGE);
                fdp->set_type_name(entry_name);
                fdp->set_label(google::protobuf::FieldDescriptorProto::LABEL_REPEATED);
            }

            // oneof_index (optional — set by oneof processing below)
            QoreValue oneof_idx_val = field_def->getKeyValue("oneof_index");
            if (!oneof_idx_val.isNullOrNothing()) {
                fdp->set_oneof_index((int)oneof_idx_val.getAsBigInt());
            }

            // options (optional)
            QoreValue opts_val = field_def->getKeyValue("options");
            if (!opts_val.isNullOrNothing()) {
                const QoreHashNode* opts = opts_val.get<QoreHashNode>();
                if (opts) {
                    QoreValue packed_val = opts->getKeyValue("packed");
                    if (!packed_val.isNullOrNothing() && packed_val.getAsBool()) {
                        fdp->mutable_options()->set_packed(true);
                    }
                }
            }
        }
    }

    // Process oneofs
    QoreValue oneofs_val = msg_def->getKeyValue("oneofs");
    if (!oneofs_val.isNullOrNothing()) {
        const QoreHashNode* oneofs = oneofs_val.get<QoreHashNode>();
        if (!oneofs) {
            xsink->raiseException("PROTOBUF-SCHEMA-ERROR",
                "'oneofs' must be a hash of oneof definitions");
            return;
        }
        ConstHashIterator ohi(oneofs);
        while (ohi.next()) {
            google::protobuf::OneofDescriptorProto* odp = msg->add_oneof_decl();
            odp->set_name(ohi.getKey());

            // Process oneof member fields
            const QoreHashNode* oneof_def = ohi.get().get<QoreHashNode>();
            if (!oneof_def) {
                continue;
            }
            QoreValue oneof_fields_val = oneof_def->getKeyValue("fields");
            if (oneof_fields_val.isNullOrNothing()) {
                continue;
            }
            const QoreListNode* oneof_fields = oneof_fields_val.get<QoreListNode>();
            if (!oneof_fields) {
                continue;
            }

            int oneof_index = msg->oneof_decl_size() - 1;
            for (size_t j = 0; j < oneof_fields->size(); ++j) {
                QoreValue ofval = oneof_fields->retrieveEntry(j);
                const QoreHashNode* ofield_def = ofval.get<QoreHashNode>();
                if (!ofield_def) {
                    continue;
                }

                google::protobuf::FieldDescriptorProto* ofdp = msg->add_field();

                QoreValue oname = ofield_def->getKeyValue("name");
                if (!oname.isNullOrNothing()) {
                    QoreStringValueHelper oname_str(oname);
                    ofdp->set_name(oname_str->c_str());
                }

                QoreValue onumber = ofield_def->getKeyValue("number");
                if (!onumber.isNullOrNothing()) {
                    ofdp->set_number((int)onumber.getAsBigInt());
                }

                QoreValue otype = ofield_def->getKeyValue("type");
                if (!otype.isNullOrNothing()) {
                    QoreStringValueHelper otype_str(otype);
                    bool ois_ref = false;
                    auto oftype = resolveFieldType(otype_str->c_str(), ois_ref);
                    ofdp->set_type(oftype);
                    if (ois_ref) {
                        ofdp->set_type_name(otype_str->c_str());
                    }
                }

                ofdp->set_label(google::protobuf::FieldDescriptorProto::LABEL_OPTIONAL);
                ofdp->set_oneof_index(oneof_index);
            }
        }
    }

    // Process nested enums
    QoreValue enums_val = msg_def->getKeyValue("enums");
    if (!enums_val.isNullOrNothing()) {
        const QoreHashNode* enums = enums_val.get<QoreHashNode>();
        if (!enums) {
            xsink->raiseException("PROTOBUF-SCHEMA-ERROR",
                "'enums' must be a hash of enum definitions");
            return;
        }
        ConstHashIterator ehi(enums);
        while (ehi.next()) {
            google::protobuf::EnumDescriptorProto* edp = msg->add_enum_type();
            edp->set_name(ehi.getKey());

            const QoreHashNode* enum_def = ehi.get().get<QoreHashNode>();
            if (!enum_def) {
                continue;
            }
            QoreValue values_val = enum_def->getKeyValue("values");
            if (values_val.isNullOrNothing()) {
                continue;
            }
            const QoreHashNode* values = values_val.get<QoreHashNode>();
            if (!values) {
                continue;
            }
            ConstHashIterator vhi(values);
            while (vhi.next()) {
                google::protobuf::EnumValueDescriptorProto* evdp = edp->add_value();
                evdp->set_name(vhi.getKey());
                evdp->set_number((int)vhi.get().getAsBigInt());
            }
        }
    }

    // Process nested messages
    QoreValue nested_val = msg_def->getKeyValue("nested_messages");
    if (!nested_val.isNullOrNothing()) {
        const QoreHashNode* nested = nested_val.get<QoreHashNode>();
        if (nested) {
            ConstHashIterator nhi(nested);
            while (nhi.next()) {
                google::protobuf::DescriptorProto* nested_msg = msg->add_nested_type();
                nested_msg->set_name(nhi.getKey());
                const QoreHashNode* nested_def = nhi.get().get<QoreHashNode>();
                if (nested_def) {
                    buildMessage(nested_msg, nested_def, xsink);
                    if (*xsink) {
                        return;
                    }
                }
            }
        }
    }
}

void QoreProtobufSchema::buildFromHash(const QoreHashNode* schema_def, ExceptionSink* xsink) {
    google::protobuf::FileDescriptorProto fdp;
    fdp.set_name("programmatic.proto");

    // Set syntax
    QoreValue syntax_val = schema_def->getKeyValue("syntax");
    fdp.set_syntax(syntax_val.isNullOrNothing()
        ? "proto3" : QoreStringValueHelper(syntax_val)->c_str());

    // Set package
    QoreValue pkg_val = schema_def->getKeyValue("package");
    if (!pkg_val.isNullOrNothing()) {
        QoreStringValueHelper pkg_str(pkg_val);
        fdp.set_package(pkg_str->c_str());
    }

    // Process top-level enums
    QoreValue enums_val = schema_def->getKeyValue("enums");
    if (!enums_val.isNullOrNothing()) {
        const QoreHashNode* enums = enums_val.get<QoreHashNode>();
        if (enums) {
            ConstHashIterator ehi(enums);
            while (ehi.next()) {
                google::protobuf::EnumDescriptorProto* edp = fdp.add_enum_type();
                edp->set_name(ehi.getKey());
                const QoreHashNode* enum_def = ehi.get().get<QoreHashNode>();
                if (!enum_def) {
                    continue;
                }
                QoreValue values_val = enum_def->getKeyValue("values");
                if (values_val.isNullOrNothing()) {
                    continue;
                }
                const QoreHashNode* values = values_val.get<QoreHashNode>();
                if (!values) {
                    continue;
                }
                ConstHashIterator vhi(values);
                while (vhi.next()) {
                    google::protobuf::EnumValueDescriptorProto* evdp = edp->add_value();
                    evdp->set_name(vhi.getKey());
                    evdp->set_number((int)vhi.get().getAsBigInt());
                }
            }
        }
    }

    // Process messages
    QoreValue msgs_val = schema_def->getKeyValue("messages");
    if (!msgs_val.isNullOrNothing()) {
        const QoreHashNode* messages = msgs_val.get<QoreHashNode>();
        if (!messages) {
            xsink->raiseException("PROTOBUF-SCHEMA-ERROR",
                "'messages' must be a hash of message definitions");
            return;
        }
        ConstHashIterator hi(messages);
        while (hi.next()) {
            google::protobuf::DescriptorProto* msg = fdp.add_message_type();
            msg->set_name(hi.getKey());
            const QoreHashNode* msg_def = hi.get().get<QoreHashNode>();
            if (msg_def) {
                buildMessage(msg, msg_def, xsink);
                if (*xsink) {
                    return;
                }
            }
        }
    }

    // Process services
    QoreValue svcs_val = schema_def->getKeyValue("services");
    if (!svcs_val.isNullOrNothing()) {
        const QoreHashNode* services = svcs_val.get<QoreHashNode>();
        if (!services) {
            xsink->raiseException("PROTOBUF-SCHEMA-ERROR",
                "'services' must be a hash of service definitions");
            return;
        }
        ConstHashIterator shi(services);
        while (shi.next()) {
            google::protobuf::ServiceDescriptorProto* sdp = fdp.add_service();
            sdp->set_name(shi.getKey());
            const QoreHashNode* svc_def = shi.get().get<QoreHashNode>();
            if (!svc_def) {
                continue;
            }
            QoreValue methods_val = svc_def->getKeyValue("methods");
            if (methods_val.isNullOrNothing()) {
                continue;
            }
            const QoreListNode* methods = methods_val.get<QoreListNode>();
            if (!methods) {
                continue;
            }
            for (size_t i = 0; i < methods->size(); ++i) {
                QoreValue mval = methods->retrieveEntry(i);
                const QoreHashNode* method_def = mval.get<QoreHashNode>();
                if (!method_def) {
                    continue;
                }
                google::protobuf::MethodDescriptorProto* mdp = sdp->add_method();
                QoreValue mname = method_def->getKeyValue("name");
                if (!mname.isNullOrNothing()) {
                    QoreStringValueHelper mname_str(mname);
                    mdp->set_name(mname_str->c_str());
                }
                QoreValue input = method_def->getKeyValue("input_type");
                if (!input.isNullOrNothing()) {
                    QoreStringValueHelper input_str(input);
                    mdp->set_input_type(input_str->c_str());
                }
                QoreValue output = method_def->getKeyValue("output_type");
                if (!output.isNullOrNothing()) {
                    QoreStringValueHelper output_str(output);
                    mdp->set_output_type(output_str->c_str());
                }
                QoreValue cs = method_def->getKeyValue("client_streaming");
                if (!cs.isNullOrNothing()) {
                    mdp->set_client_streaming(cs.getAsBool());
                }
                QoreValue ss = method_def->getKeyValue("server_streaming");
                if (!ss.isNullOrNothing()) {
                    mdp->set_server_streaming(ss.getAsBool());
                }
            }
        }
    }

    // Build into descriptor pool
    desc_error_collector = std::make_unique<DescriptorErrorCollector>();
    standalone_pool = std::make_unique<google::protobuf::DescriptorPool>(
        google::protobuf::DescriptorPool::generated_pool());

    file_desc = standalone_pool->BuildFileCollectingErrors(fdp, desc_error_collector.get());
    if (!file_desc) {
        xsink->raiseException("PROTOBUF-SCHEMA-ERROR",
            "failed to build schema from definition: %s",
            desc_error_collector->getErrors().c_str());
        return;
    }

    file_descs.push_back(file_desc);
    factory = std::make_unique<google::protobuf::DynamicMessageFactory>(standalone_pool.get());
}
