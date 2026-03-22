/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file ProtobufHelper.h protobuf <-> Qore hash conversion helpers */
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

#ifndef _QORE_PROTOBUF_HELPER_H
#define _QORE_PROTOBUF_HELPER_H

#include <qore/Qore.h>

#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>

//! Helper class for converting between Qore hashes and protobuf messages
class ProtobufHelper {
public:
    //! Convert a protobuf Message to a Qore hash
    DLLLOCAL static QoreHashNode* messageToHash(const google::protobuf::Message& msg,
        ExceptionSink* xsink);

    //! Convert a Qore hash to a protobuf Message
    DLLLOCAL static bool hashToMessage(const QoreHashNode* hash,
        google::protobuf::Message* msg, ExceptionSink* xsink);

private:
    DLLLOCAL static QoreValue fieldToQore(const google::protobuf::Message& msg,
        const google::protobuf::FieldDescriptor* field,
        const google::protobuf::Reflection* ref, ExceptionSink* xsink);

    DLLLOCAL static QoreListNode* repeatedFieldToList(const google::protobuf::Message& msg,
        const google::protobuf::FieldDescriptor* field,
        const google::protobuf::Reflection* ref, ExceptionSink* xsink);

    DLLLOCAL static QoreValue repeatedElementToQore(const google::protobuf::Message& msg,
        const google::protobuf::FieldDescriptor* field,
        const google::protobuf::Reflection* ref, int index, ExceptionSink* xsink);

    DLLLOCAL static bool setFieldFromQore(google::protobuf::Message* msg,
        const google::protobuf::FieldDescriptor* field,
        const google::protobuf::Reflection* ref,
        QoreValue val, ExceptionSink* xsink);

    DLLLOCAL static bool setRepeatedFieldFromList(google::protobuf::Message* msg,
        const google::protobuf::FieldDescriptor* field,
        const google::protobuf::Reflection* ref,
        const QoreListNode* list, ExceptionSink* xsink);

    DLLLOCAL static QoreHashNode* mapFieldToHash(const google::protobuf::Message& msg,
        const google::protobuf::FieldDescriptor* field,
        const google::protobuf::Reflection* ref, ExceptionSink* xsink);

    DLLLOCAL static bool setMapFieldFromHash(google::protobuf::Message* msg,
        const google::protobuf::FieldDescriptor* field,
        const google::protobuf::Reflection* ref,
        const QoreHashNode* hash, ExceptionSink* xsink);
};

#endif // _QORE_PROTOBUF_HELPER_H
