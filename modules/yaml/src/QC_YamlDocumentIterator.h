/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_YamlDocumentIterator.h

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

#ifndef _QORE_YAML_DOCUMENT_ITERATOR_H
#define _QORE_YAML_DOCUMENT_ITERATOR_H

#include <qore/Qore.h>
#include <yaml.h>
#include <memory>

#include "YamlStreamReadHandler.h"

#include <string>
#include <map>

// Error strings
#define QY_STREAM_ERR "STREAM-READ-ERROR"

class QoreYamlDocumentIterator : public AbstractPrivateData {
public:
    // Alias map type for anchor/alias support
    typedef std::map<std::string, QoreValue> alias_map_t;
    // Constructor for string input
    DLLLOCAL QoreYamlDocumentIterator(const QoreStringNode* yaml_str,
                                       bool skip_empty, int max_docs,
                                       ExceptionSink* xsink);

    // Constructor for stream input
    DLLLOCAL QoreYamlDocumentIterator(QoreObject* stream, const QoreEncoding* encoding,
                                       bool skip_empty, int max_docs,
                                       ExceptionSink* xsink);

    DLLLOCAL virtual ~QoreYamlDocumentIterator();

    // Returns true if there is another document
    DLLLOCAL bool next(ExceptionSink* xsink);

    // Returns the current document value
    DLLLOCAL QoreValue getValue(ExceptionSink* xsink);

    // Returns true if positioned on a valid document
    DLLLOCAL bool valid() const { return is_valid; }

    // Returns the current document number (1-based)
    DLLLOCAL int64 getDocumentNumber() const { return doc_number; }

    // Reset to beginning (only works for string input)
    DLLLOCAL int reset(ExceptionSink* xsink);

private:
    std::string yaml_data;
    yaml_parser_t parser;
    yaml_event_t event;
    bool parser_initialized;
    bool is_valid;
    bool skip_empty_docs;
    int max_documents;
    int64 doc_number;
    bool has_event;
    bool from_stream;  // True if initialized from stream (can't reset)

    // Stream read handler for true streaming input (null for string input)
    std::unique_ptr<YamlStreamReadHandler> read_handler;

    // Alias map for anchor/alias support
    alias_map_t alias_map;

    // Values outlive individual calls; do not retain a call's ExceptionSink.
    QoreValue current_value;

    DLLLOCAL int initParser(ExceptionSink* xsink);
    DLLLOCAL void cleanupParser();
    DLLLOCAL void discardEvent();
    DLLLOCAL int getEvent(ExceptionSink* xsink);
    DLLLOCAL QoreValue parseDocument(ExceptionSink* xsink);
    DLLLOCAL QoreValue parseNode(ExceptionSink* xsink);
    DLLLOCAL QoreListNode* parseSequence(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* parseMapping(ExceptionSink* xsink);
    DLLLOCAL QoreValue parseScalar(ExceptionSink* xsink);
    DLLLOCAL void clearAliasMap(ExceptionSink* xsink);
    DLLLOCAL void storeAnchor(const std::string& anchor, QoreValue val, ExceptionSink* xsink);
};

// Class ID for the QoreYamlDocumentIterator class
DLLLOCAL extern qore_classid_t CID_YAMLDOCUMENTITERATOR;

// Qore class pointer
DLLLOCAL extern QoreClass* QC_YAMLDOCUMENTITERATOR;

// hashdecl pointer for YamlDocumentIteratorOptions
DLLLOCAL extern const TypedHashDecl* hashdeclYamlDocumentIteratorOptions;

// Initialization functions
DLLLOCAL QoreClass* initYamlDocumentIteratorClass(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_YamlDocumentIteratorOptions(QoreNamespace& ns);

// Function to parse all documents from a string
DLLLOCAL QoreListNode* parse_yaml_documents_intern(const QoreStringNode* yaml, ExceptionSink* xsink);

#endif
