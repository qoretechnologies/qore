/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QoreProtobufSchema.h QoreProtobufSchema class definition */
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

#ifndef _QORE_PROTOBUF_SCHEMA_H
#define _QORE_PROTOBUF_SCHEMA_H

#include "protobuf-module.h"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/compiler/importer.h>

#include <string>
#include <memory>
#include <vector>

//! Wraps protobuf Importer + DynamicMessageFactory for dynamic proto schema handling
class QoreProtobufSchema : public AbstractPrivateData {
public:
    //! Constructor: load .proto file from the filesystem
    DLLLOCAL QoreProtobufSchema(const char* path, const char* proto_file, ExceptionSink* xsink);

    //! Constructor: parse .proto content from a string
    DLLLOCAL QoreProtobufSchema(const QoreString& proto_content, const char* filename,
        ExceptionSink* xsink);

    //! Constructor: build from serialized FileDescriptorProto data
    DLLLOCAL QoreProtobufSchema(const QoreListNode* serialized_fds, ExceptionSink* xsink);

    //! Constructor: build from a programmatic schema definition hash
    /** @param schema_def a hash defining messages, enums, and package info
        @param xsink exception sink
    */
    DLLLOCAL QoreProtobufSchema(const QoreHashNode* schema_def, ExceptionSink* xsink);

    DLLLOCAL ~QoreProtobufSchema();

    //! Get list of service definitions
    DLLLOCAL QoreListNode* getServices(ExceptionSink* xsink) const;

    //! Get list of message type names
    DLLLOCAL QoreListNode* getMessageTypes(ExceptionSink* xsink) const;

    //! Get a default (empty) message as a Qore hash
    DLLLOCAL QoreHashNode* getDefaultMessage(const char* type, ExceptionSink* xsink) const;

    //! Encode a Qore hash to protobuf binary
    DLLLOCAL BinaryNode* encode(const char* type, const QoreHashNode* data, ExceptionSink* xsink) const;

    //! Decode protobuf binary to a Qore hash
    DLLLOCAL QoreHashNode* decode(const char* type, const BinaryNode* data, ExceptionSink* xsink) const;

    //! Convert a Qore hash to JSON string representation (via protobuf)
    DLLLOCAL QoreStringNode* toJson(const char* type, const QoreHashNode* data, ExceptionSink* xsink) const;

    //! Parse JSON string to a Qore hash (via protobuf)
    DLLLOCAL QoreHashNode* fromJson(const char* type, const QoreString& json, ExceptionSink* xsink) const;

    //! Returns field-level metadata for a protobuf message type
    DLLLOCAL QoreHashNode* getMessageSchema(const char* type, ExceptionSink* xsink) const;

    //! Returns enum value names and numbers
    DLLLOCAL QoreHashNode* getEnumValues(const char* enum_type, ExceptionSink* xsink) const;

    //! Serializes all file descriptors to a list of binary data
    DLLLOCAL QoreListNode* serializeFileDescriptors(ExceptionSink* xsink) const;

    //! Serializes the file descriptor containing a given symbol and its transitive dependencies
    DLLLOCAL QoreListNode* serializeFileDescriptorForSymbol(const char* symbol,
        ExceptionSink* xsink) const;

    //! Serializes the file descriptor with the given filename and its transitive dependencies
    DLLLOCAL QoreListNode* serializeFileDescriptorByName(const char* filename,
        ExceptionSink* xsink) const;

    //! Returns the type name string for a protobuf field type
    DLLLOCAL static const char* fieldTypeName(google::protobuf::FieldDescriptor::Type type);

    //! Returns the label string for a protobuf field label
    DLLLOCAL static const char* fieldLabelName(google::protobuf::FieldDescriptor::Label label);

private:
    //! Find a message descriptor by fully-qualified name
    DLLLOCAL const google::protobuf::Descriptor* findMessageDescriptor(const char* type,
        ExceptionSink* xsink) const;

    //! Create a prototype message for a descriptor
    DLLLOCAL const google::protobuf::Message* getPrototype(
        const google::protobuf::Descriptor* desc, ExceptionSink* xsink) const;

    //! Get the descriptor pool (works for all constructor types)
    DLLLOCAL const google::protobuf::DescriptorPool* getPool() const;

    //! Build a field info hash for getMessageSchema()
    DLLLOCAL QoreHashNode* buildFieldInfo(const google::protobuf::FieldDescriptor* field,
        ExceptionSink* xsink) const;

    //! Build service info for a service descriptor
    DLLLOCAL QoreHashNode* buildServiceInfo(const google::protobuf::ServiceDescriptor* svc,
        ExceptionSink* xsink) const;

    //! Serialize a single file descriptor and its transitive dependencies
    DLLLOCAL QoreListNode* serializeFileWithDeps(const google::protobuf::FileDescriptor* fd,
        ExceptionSink* xsink) const;

    //! Build a FileDescriptorProto from a Qore hash schema definition
    DLLLOCAL void buildFromHash(const QoreHashNode* schema_def, ExceptionSink* xsink);

    //! Build a DescriptorProto (message) from a Qore hash
    DLLLOCAL void buildMessage(google::protobuf::DescriptorProto* msg,
        const QoreHashNode* msg_def, ExceptionSink* xsink);

    //! Error collector for protobuf parser errors
    class ErrorCollector : public google::protobuf::compiler::MultiFileErrorCollector {
    public:
#ifdef QORE_PROTOBUF_V22_PLUS
        void RecordError(absl::string_view filename, int line, int column,
            absl::string_view message) override;
        void RecordWarning(absl::string_view filename, int line, int column,
            absl::string_view message) override;
#else
        void AddError(const std::string& filename, int line, int column,
            const std::string& message) override;
        void AddWarning(const std::string& filename, int line, int column,
            const std::string& message) override;
#endif

        std::string getErrors() const;
        bool hasErrors() const { return !errors.empty(); }

    private:
        std::vector<std::string> errors;
    };

    //! Source tree for string-based proto loading
    class StringSourceTree : public google::protobuf::compiler::SourceTree {
    public:
        void addFile(const std::string& filename, const std::string& content);
#ifdef QORE_PROTOBUF_V22_PLUS
        google::protobuf::io::ZeroCopyInputStream* Open(absl::string_view filename) override;
#else
        google::protobuf::io::ZeroCopyInputStream* Open(const std::string& filename) override;
#endif
        std::string GetLastErrorMessage() override;

    private:
        std::map<std::string, std::string> files;
        std::string last_error;
    };

    //! Descriptor database error collector for BuildFile
    class DescriptorErrorCollector : public google::protobuf::DescriptorPool::ErrorCollector {
    public:
#ifdef QORE_PROTOBUF_V22_PLUS
        void RecordError(absl::string_view filename, absl::string_view element_name,
            const google::protobuf::Message* descriptor, ErrorLocation location,
            absl::string_view message) override;
#else
        void AddError(const std::string& filename, const std::string& element_name,
            const google::protobuf::Message* descriptor, ErrorLocation location,
            const std::string& message) override;
#endif
        std::string getErrors() const;
        bool hasErrors() const { return !errors.empty(); }

    private:
        std::vector<std::string> errors;
    };

    std::unique_ptr<google::protobuf::compiler::DiskSourceTree> disk_source_tree;
    std::unique_ptr<StringSourceTree> string_source_tree;
    std::unique_ptr<ErrorCollector> error_collector;
    std::unique_ptr<google::protobuf::compiler::Importer> importer;

    //! Standalone descriptor pool for the descriptor-based and programmatic constructors
    /** @note Must be declared before \c factory so that the factory is destroyed
        first -- the factory's cached DynamicMessage objects reference descriptors
        in the pool.
    */
    std::unique_ptr<google::protobuf::DescriptorPool> standalone_pool;
    std::unique_ptr<DescriptorErrorCollector> desc_error_collector;

    std::unique_ptr<google::protobuf::DynamicMessageFactory> factory;

    //! All file descriptors loaded (for multi-file descriptor constructor)
    std::vector<const google::protobuf::FileDescriptor*> file_descs;

    const google::protobuf::FileDescriptor* file_desc = nullptr;
};

#endif // _QORE_PROTOBUF_SCHEMA_H
