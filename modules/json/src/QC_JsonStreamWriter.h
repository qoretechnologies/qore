/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QC_JsonStreamWriter.h JSON Stream Writer class definition */
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

#ifndef _QORE_QC_JSONSTREAMWRITER_H
#define _QORE_QC_JSONSTREAMWRITER_H

#include "qore-json-module.h"

#include <vector>

DLLEXPORT extern qore_classid_t CID_JSONSTREAMWRITER;
DLLEXPORT extern QoreClass* QC_JSONSTREAMWRITER;

DLLLOCAL QoreClass* initJsonStreamWriterClass(QoreNamespace& ns);

class JsonStreamWriter : public AbstractPrivateData {
public:
    //! Constructor
    /** @param os the output stream to write to
        @param format formatting option (JGF_NONE or JGF_ADD_FORMATTING)
        @param xsink exception sink
    */
    DLLLOCAL JsonStreamWriter(OutputStream* os, int format, ExceptionSink* xsink);

    DLLLOCAL virtual ~JsonStreamWriter();

    //! Write the start of a JSON object '{'
    DLLLOCAL int startObject(ExceptionSink* xsink);

    //! Write the end of a JSON object '}'
    DLLLOCAL int endObject(ExceptionSink* xsink);

    //! Write the start of a JSON array '['
    DLLLOCAL int startArray(ExceptionSink* xsink);

    //! Write the end of a JSON array ']'
    DLLLOCAL int endArray(ExceptionSink* xsink);

    //! Write an object key
    DLLLOCAL int key(const QoreStringNode* k, ExceptionSink* xsink);

    //! Write a string value
    DLLLOCAL int stringValue(const QoreStringNode* str, ExceptionSink* xsink);

    //! Write a number value (integer)
    DLLLOCAL int numberValue(int64 n, ExceptionSink* xsink);

    //! Write a number value (float)
    DLLLOCAL int numberValue(double n, ExceptionSink* xsink);

    //! Write a boolean value
    DLLLOCAL int boolValue(bool b, ExceptionSink* xsink);

    //! Write a null value
    DLLLOCAL int nullValue(ExceptionSink* xsink);

    //! Write any Qore value as JSON
    DLLLOCAL int value(QoreValue v, ExceptionSink* xsink);

    //! Flush the stream
    DLLLOCAL int flush(ExceptionSink* xsink);

private:
    //! Container type for tracking state
    enum ContainerType {
        CT_NONE,
        CT_OBJECT,
        CT_ARRAY
    };

    //! State for each nesting level
    struct ContainerState {
        ContainerType type;
        bool needComma;     //!< Whether next element needs a leading comma
        bool expectValue;   //!< In object, true after key (expecting value)

        ContainerState(ContainerType t) : type(t), needComma(false), expectValue(false) {}
    };

    OutputStream* os;                       //!< Output stream (weak reference)
    std::vector<ContainerState> stack;      //!< Container state stack
    int format;                             //!< Formatting option
    QoreString buffer;                      //!< Write buffer

    //! Write raw string to stream
    DLLLOCAL int writeRaw(const char* str, size_t len, ExceptionSink* xsink);
    DLLLOCAL int writeRaw(const char* str, ExceptionSink* xsink);
    DLLLOCAL int writeRaw(const QoreString& str, ExceptionSink* xsink);

    //! Write comma if needed
    DLLLOCAL int writeCommaIfNeeded(ExceptionSink* xsink);

    //! Write newline and indentation if formatting
    DLLLOCAL int writeNewlineIndent(ExceptionSink* xsink);

    //! Write a JSON-escaped string (with quotes)
    DLLLOCAL int writeJsonString(const QoreStringNode* str, ExceptionSink* xsink);

    //! Check sandbox limits
    DLLLOCAL bool checkSandboxLimits(ExceptionSink* xsink);

    //! Ensure key/value sequencing is valid
    DLLLOCAL bool ensureKeyAllowed(ExceptionSink* xsink);
    DLLLOCAL bool ensureValueAllowed(ExceptionSink* xsink);
    DLLLOCAL void markValueWritten();

    //! Current depth
    DLLLOCAL int depth() const { return (int)stack.size(); }

    //! Iteration counter for interrupt checks
    int iteration;

    //! Cached sandbox manager
    QoreSandboxManagerHelper smh;
};

#endif // _QORE_QC_JSONSTREAMWRITER_H
