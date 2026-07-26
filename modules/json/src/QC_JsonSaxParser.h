/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QC_JsonSaxParser.h JSON SAX Parser class definition */
/*
    Qore Programming Language - JSON Module

    Copyright (C) 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_QC_JSONSAXPARSER_H
#define _QORE_QC_JSONSAXPARSER_H

#include "qore-json-module.h"
#include "JsonStreamReadHandler.h"

// SAX Event Types
#define JSON_SAX_OBJECT_START   1
#define JSON_SAX_OBJECT_END     2
#define JSON_SAX_ARRAY_START    3
#define JSON_SAX_ARRAY_END      4
#define JSON_SAX_KEY            5
#define JSON_SAX_STRING         6
#define JSON_SAX_NUMBER         7
#define JSON_SAX_BOOLEAN        8
#define JSON_SAX_NULL           9

DLLEXPORT extern qore_classid_t CID_JSONSAXPARSER;
DLLEXPORT extern QoreClass* QC_JSONSAXPARSER;

DLLLOCAL QoreClass* initJsonSaxParserClass(QoreNamespace& ns);

class JsonStreamReader;

class JsonSaxParser : public AbstractPrivateData {
public:
    DLLLOCAL JsonSaxParser(ExceptionSink* xsink);
    DLLLOCAL virtual ~JsonSaxParser();

    //! Parse a JSON string with SAX callbacks
    /** @param json_str the JSON string to parse
        @param callback the Qore callback code to call for each event
        @param xsink exception sink
        @return 0 on success, -1 on error
    */
    DLLLOCAL int parse(const QoreStringNode* json_str, ResolvedCallReferenceNode* callback, ExceptionSink* xsink);

    //! Parse JSON from an input stream with SAX callbacks
    /** @param is the input stream to read from
        @param callback the Qore callback code to call for each event
        @param encoding the character encoding (default UTF-8)
        @param xsink exception sink
        @return 0 on success, -1 on error
    */
    DLLLOCAL int parseStream(QoreObject* stream, ResolvedCallReferenceNode* callback,
        const QoreEncoding* encoding, ExceptionSink* xsink);

private:
    //! Internal parsing state
    struct ParseState {
        int line_number;
        int column;
        int depth;
        int iteration;              //!< counter for interrupt checks
        QoreSandboxManagerHelper smh; //!< cached sandbox manager helper (RAII ref-counted)
        class JsonStreamReader* reader;
        ResolvedCallReferenceNode* callback;
        ExceptionSink* xsink;
        const QoreEncoding* encoding;
    };

    //! Check sandbox depth limit - no-op if no sandbox
    DLLLOCAL bool checkSandboxLimits(ParseState& state);

    //! Check for sandbox interrupt (periodic) - no-op if no sandbox
    DLLLOCAL bool checkInterrupt(ParseState& state);

    //! Emit a SAX event to the callback
    DLLLOCAL bool emitEvent(ParseState& state, int type, const QoreStringNode* key, QoreValue value);

    //! Skip whitespace
    DLLLOCAL void skipWhitespace(ParseState& state);

    //! Peek next character
    DLLLOCAL int peek(ParseState& state);

    //! Get next character
    DLLLOCAL int get(ParseState& state);

    //! Skip UTF-8 BOM if present
    DLLLOCAL void skipBom(ParseState& state);

    //! Parse a JSON value with SAX events
    DLLLOCAL bool parseValue(ParseState& state);

    //! Parse a JSON object with SAX events
    DLLLOCAL bool parseObject(ParseState& state);

    //! Parse a JSON array with SAX events
    DLLLOCAL bool parseArray(ParseState& state);

    //! Parse a JSON string
    DLLLOCAL QoreStringNode* parseString(ParseState& state);

    //! Parse a JSON number
    DLLLOCAL bool parseNumber(ParseState& state);

    //! Compare rest of token
    DLLLOCAL bool cmpRestToken(ParseState& state, const char* tok);
};

#endif // _QORE_QC_JSONSAXPARSER_H
