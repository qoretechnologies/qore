/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_YamlStreamWriter.h

    Qore Programming Language

    Copyright 2003 - 2026 Qore Technologies, s.r.o.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef _QORE_YAML_STREAM_WRITER_H
#define _QORE_YAML_STREAM_WRITER_H

#include <qore/Qore.h>
#include <yaml.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

// Error string
#define QY_STREAM_EMIT_ERR "YAML-STREAM-EMIT-ERROR"

// Writer states
enum YamlWriterState {
    YWS_INITIAL,        // Before any output
    YWS_STREAM_STARTED, // Stream started
    YWS_IN_DOCUMENT,    // Inside a document
    YWS_IN_MAPPING,     // Inside a mapping
    YWS_IN_SEQUENCE,    // Inside a sequence
    YWS_EXPECT_VALUE,   // After a key, expecting value
    YWS_CLOSED          // Writer closed
};

class QoreYamlStreamWriteHandler;

class QoreYamlStreamWriter : public AbstractPrivateData {
public:
    DLLLOCAL QoreYamlStreamWriter(QoreObject* stream, int flags, int width, int indent,
                                   ExceptionSink* xsink);
    DLLLOCAL virtual ~QoreYamlStreamWriter();

    DLLLOCAL int startDocument(ExceptionSink* xsink);
    DLLLOCAL int endDocument(ExceptionSink* xsink);
    DLLLOCAL int startSequence(const char* anchor, ExceptionSink* xsink);
    DLLLOCAL int endSequence(ExceptionSink* xsink);
    DLLLOCAL int startMapping(const char* anchor, ExceptionSink* xsink);
    DLLLOCAL int endMapping(ExceptionSink* xsink);
    DLLLOCAL int writeKey(const QoreStringNode* key, ExceptionSink* xsink);
    DLLLOCAL int writeScalar(QoreValue value, const QoreStringNode* tag,
                              const QoreStringNode* anchor, ExceptionSink* xsink);
    DLLLOCAL int writeAlias(const char* anchor, ExceptionSink* xsink);
    DLLLOCAL int write(QoreValue value, ExceptionSink* xsink);
    DLLLOCAL int flush(ExceptionSink* xsink);
    DLLLOCAL int close(ExceptionSink* xsink);

    DLLLOCAL int64 getDocumentCount() const { return document_count; }

private:
    QoreObject* output_stream;
    yaml_emitter_t emitter;
    yaml_event_t event;
    bool emitter_initialized;
    YamlWriterState state;
    int emit_flags;
    bool block_style;
    bool explicit_start_doc;
    bool explicit_end_doc;
    int64 document_count;
    std::set<std::string> anchors;
    std::vector<YamlWriterState> state_stack;  // Stack for nested container states

    // Output buffer
    std::unique_ptr<QoreYamlStreamWriteHandler> write_handler;

    DLLLOCAL int emitEvent(const char* event_name, ExceptionSink* xsink);
    DLLLOCAL int writeScalarValue(const QoreStringNode* value, const char* tag,
                                   const char* anchor, yaml_scalar_style_t style,
                                   ExceptionSink* xsink);
    DLLLOCAL int writeValueRecursive(QoreValue value, ExceptionSink* xsink);

    // Static callback for libyaml
    static int yaml_write_handler(void* data, unsigned char* buffer, size_t size);
};

// Handler for writing to output stream - streams data directly
class QoreYamlStreamWriteHandler {
public:
    DLLLOCAL QoreYamlStreamWriteHandler(QoreObject* stream);
    DLLLOCAL virtual ~QoreYamlStreamWriteHandler();

    DLLLOCAL int write(unsigned char* buffer, size_t size);
    DLLLOCAL bool hasError() const { return has_error; }
    DLLLOCAL const char* getErrorMessage() const { return error_message.c_str(); }

private:
    QoreObject* output_stream;
    bool has_error;
    std::string error_message;
};

// Class ID for the QoreYamlStreamWriter class
DLLLOCAL extern qore_classid_t CID_YAMLSTREAMWRITER;

// Qore class pointer
DLLLOCAL extern QoreClass* QC_YAMLSTREAMWRITER;

// hashdecl pointer for YamlStreamWriterOptions
DLLLOCAL extern const TypedHashDecl* hashdeclYamlStreamWriterOptions;

// Initialization functions
DLLLOCAL QoreClass* initYamlStreamWriterClass(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_YamlStreamWriterOptions(QoreNamespace& ns);

#endif
