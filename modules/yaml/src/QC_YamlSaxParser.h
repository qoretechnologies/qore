/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_YamlSaxParser.h

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

#ifndef _QORE_YAML_SAX_PARSER_H
#define _QORE_YAML_SAX_PARSER_H

#include <qore/Qore.h>
#include <yaml.h>

#include "YamlStreamReadHandler.h"

// Event type constants
#define YAML_SAX_STREAM_START       1
#define YAML_SAX_STREAM_END         2
#define YAML_SAX_DOCUMENT_START     3
#define YAML_SAX_DOCUMENT_END       4
#define YAML_SAX_ALIAS              5
#define YAML_SAX_SCALAR             6
#define YAML_SAX_SEQUENCE_START     7
#define YAML_SAX_SEQUENCE_END       8
#define YAML_SAX_MAPPING_START      9
#define YAML_SAX_MAPPING_END        10

// Error strings
#define QY_SAX_PARSE_ERR "YAML-SAX-PARSE-ERROR"
#define QY_SAX_CALLBACK_ERR "YAML-SAX-CALLBACK-ERROR"
#define QY_SAX_STREAM_ERR "STREAM-READ-ERROR"

class QoreYamlSaxParser : public AbstractPrivateData {
public:
    DLLLOCAL QoreYamlSaxParser();
    DLLLOCAL virtual ~QoreYamlSaxParser();

    DLLLOCAL int parse(const QoreStringNode* yaml_str,
                       ResolvedCallReferenceNode* callback,
                       ExceptionSink* xsink);

    DLLLOCAL int parseStream(QoreObject* stream,
                             ResolvedCallReferenceNode* callback,
                             const QoreEncoding* encoding,
                             ExceptionSink* xsink);

private:
    struct ParseState {
        yaml_parser_t parser;
        int depth;
        ResolvedCallReferenceNode* callback;
        ExceptionSink* xsink;
    };

    DLLLOCAL bool emitEvent(ParseState& state, yaml_event_t& event);
    DLLLOCAL bool processEvents(ParseState& state);
    DLLLOCAL QoreHashNode* createEventHash(yaml_event_t& event, int depth,
                                            ExceptionSink* xsink);
};

// Class ID for the QoreYamlSaxParser class
DLLLOCAL extern qore_classid_t CID_YAMLSAXPARSER;

// Qore class pointer
DLLLOCAL extern QoreClass* QC_YAMLSAXPARSER;

// hashdecl pointer for YamlSaxEvent
DLLLOCAL extern const TypedHashDecl* hashdeclYamlSaxEvent;

// enum pointers (used by hashdecl initialization)
DLLLOCAL extern QoreEnumDecl* enumYamlSaxEventType;
DLLLOCAL extern QoreEnumDecl* enumYamlScalarStyle;

// Initialization functions
DLLLOCAL QoreClass* initYamlSaxParserClass(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_YamlSaxEvent(QoreNamespace& ns);
DLLLOCAL QoreEnumDecl* init_enum_YamlSaxEventType(QoreNamespace& ns);
DLLLOCAL QoreEnumDecl* init_enum_YamlScalarStyle(QoreNamespace& ns);

#endif
