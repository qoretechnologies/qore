/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreSerializable.h

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef _QORE_CLASS_INTERN_QORESERIALIZABLE_H

#define _QORE_CLASS_INTERN_QORESERIALIZABLE_H

#include <qore/Qore.h>
#include "qore/intern/StreamReader.h"
#include "qore/intern/StreamWriter.h"

#include <map>
#include <string>
#include <set>

// maps from index strings to containers
typedef std::map<std::string, QoreValue> oimap_t;

// maps from object hashes to index strings
typedef std::map<std::string, std::string> imap_t;

// set of modules to load
typedef std::set<std::string> mset_t;

namespace {
// qore serialization stream data type constants
enum qore_stream_type : unsigned char {
    HASH = 0,
    HASHDECL = 1,
    STRING = 2,
    UTF8_STRING = 3,
    LIST = 4,
    BOOLEAN_TRUE = 5,
    BOOLEAN_FALSE = 6,
    INT1 = 7,
    INT2 = 8,
    INT4 = 9,
    INT8 = 10,
    FLOAT = 11,
    QORENUMBER = 12,
    QOREBINARY = 13,
    ABSDATE = 14,
    RELDATE = 15,
    SQLNULL = 16,
    NOTHING = 17,
};
}

class QoreInternalSerializationContext {
public:
    ReferenceHolder<QoreHashNode> index;
    ReferenceHolder<QoreHashNode> svars;
    imap_t imap;
    mset_t mset;

    // set of classes to serialize static vars
    typedef std::set<const QoreClass*> cset_t;
    cset_t cset;

    int64 flags;

    DLLLOCAL QoreInternalSerializationContext(ExceptionSink* xsink, int64 flags) : flags(flags), index(xsink),
            svars(xsink) {
    }

    DLLLOCAL int serializeObject(const QoreObject& obj, std::string& index_str, ExceptionSink* xsink);

    DLLLOCAL int serializeHash(const QoreHashNode& h, std::string& index_str, ExceptionSink* xsink);

    DLLLOCAL int serializeList(const QoreListNode& l, std::string& index_str, ExceptionSink* xsink);

    DLLLOCAL QoreValue serializeValue(const QoreValue val, ExceptionSink* xsink);

    DLLLOCAL void addModule(const char* module) {
        mset.insert(module);
    }

    DLLLOCAL void setIndex(const char* str, QoreValue v, ExceptionSink* xsink) {
        if (!index) {
            index = new QoreHashNode(autoTypeInfo);
        }
        index->setKeyValue(str, v, xsink);
    }

    DLLLOCAL void setSVars(const char* str, QoreValue v, ExceptionSink* xsink) {
        if (!svars) {
            svars = new QoreHashNode(autoTypeInfo);
        }
        svars->setKeyValue(str, v, xsink);
    }

    DLLLOCAL int serializeStaticVars(const QoreClass& cls, ExceptionSink* xsink);
};

class ObjectIndexMap : public oimap_t {
public:
    DLLLOCAL ObjectIndexMap(ExceptionSink* xs) : xs(xs) {
    }

    DLLLOCAL ~ObjectIndexMap();

protected:
    ExceptionSink* xs;
};

class QoreInternalDeserializationContext {
public:
    ObjectIndexMap oimap;
    int64 flags;

    DLLLOCAL QoreInternalDeserializationContext(ExceptionSink* xsink, int64 flags) : oimap(xsink), flags(flags) {
    }

    DLLLOCAL QoreValue deserializeContainer(const char* index_str, ExceptionSink* xsink);

    DLLLOCAL QoreValue deserializeValue(const QoreValue val, ExceptionSink* xsink);
};

class QoreSerializable : public AbstractPrivateData {
friend class QoreInternalSerializationContext;
friend class QoreInternalDeserializationContext;

public:
    DLLLOCAL static QoreHashNode* serializeToData(QoreValue val, int64 flags, ExceptionSink* xsink);

    DLLLOCAL static void serialize(const QoreObject& self, OutputStream& stream, int64 flags, ExceptionSink* xsink);

    DLLLOCAL static void serialize(const QoreValue val, OutputStream& stream, int64 flags, ExceptionSink* xsink);

    DLLLOCAL static QoreHashNode* deserializeToData(ExceptionSink* xsink, InputStream& stream, int64 flags = 0);

    DLLLOCAL static QoreValue deserialize(ExceptionSink* xsink, InputStream& stream, int64 flags = 0);

    DLLLOCAL static QoreValue deserialize(ExceptionSink* xsink, const QoreHashNode& h, int64 flags = 0);

    DLLLOCAL static int serializeValueToStream(const QoreValue val, OutputStream& stream, ExceptionSink* xsink);

    // note: does not serialiaze the stream header; just the value
    DLLLOCAL static int serializeValueToStream(const QoreValue val, StreamWriter& writer, ExceptionSink* xsink);

    // note: reads a raw value; assumes the stream header has already been read
    DLLLOCAL static QoreValue deserializeValueFromStream(ExceptionSink* xsink, StreamReader& reader, int64 flags = 0);

protected:
    DLLLOCAL virtual ~QoreSerializable() {}

    DLLLOCAL static QoreValue serializeObjectToData(const QoreObject& obj, bool weak,
            QoreInternalSerializationContext& context, ExceptionSink* xsink);

    DLLLOCAL static imap_t::iterator serializeObjectToIndex(const QoreObject& obj,
            QoreInternalSerializationContext& context, ExceptionSink* xsink);

    DLLLOCAL static imap_t::iterator serializeObjectToIndexIntern(const QoreObject& self,
            QoreInternalSerializationContext& context, const QoreString& str,
            imap_t::iterator hint, ExceptionSink* xsink);

    DLLLOCAL static QoreValue serializeValue(const QoreValue val, QoreInternalSerializationContext& context,
            ExceptionSink* xsink);

    DLLLOCAL static QoreValue serializeHashToData(const QoreHashNode& h, bool weak,
            QoreInternalSerializationContext& context, ExceptionSink* xsink);

    DLLLOCAL static imap_t::iterator serializeHashToIndex(const QoreHashNode& h,
            QoreInternalSerializationContext& context, ExceptionSink* xsink);

    DLLLOCAL static imap_t::iterator serializeHashToIndexIntern(const QoreHashNode& h,
            QoreInternalSerializationContext& context, const QoreString& str,
            imap_t::iterator hint, ExceptionSink* xsink);

    DLLLOCAL static QoreValue serializeListToData(const QoreListNode& l, bool weak,
            QoreInternalSerializationContext& context, ExceptionSink* xsink);

    DLLLOCAL static imap_t::iterator serializeListToIndex(const QoreListNode& l,
            QoreInternalSerializationContext& context, ExceptionSink* xsink);

    DLLLOCAL static imap_t::iterator serializeListToIndexIntern(const QoreListNode& h,
            QoreInternalSerializationContext& context, const QoreString& str,
            imap_t::iterator hint, ExceptionSink* xsink);

    DLLLOCAL static int readStringFromStream(StreamReader& reader, QoreString& str, const char* type,
            ExceptionSink* xsink);
    DLLLOCAL static int64 readIntFromStream(ExceptionSink* xsink, StreamReader& reader, const char* type,
            bool can_be_negative = false);

    DLLLOCAL static int serializeHashToStream(const QoreHashNode& h, StreamWriter& writer, ExceptionSink* xsink);
    DLLLOCAL static int serializeStringToStream(const QoreStringNode& str, StreamWriter& writer,
            ExceptionSink* xsink);
    DLLLOCAL static int serializeStringToStream(StreamWriter& writer, const char* key, size_t len,
            const QoreEncoding* enc, ExceptionSink* xsink);
    DLLLOCAL static int serializeIntToStream(int64 i, StreamWriter& writer, ExceptionSink* xsink);
    DLLLOCAL static int serializeBoolToStream(bool b, StreamWriter& writer, ExceptionSink* xsink);
    DLLLOCAL static int serializeListToStream(const QoreListNode& l, StreamWriter& writer, ExceptionSink* xsink);
    DLLLOCAL static int serializeFloatToStream(double f, StreamWriter& writer, ExceptionSink* xsink);
    DLLLOCAL static int serializeNumberToStream(const QoreNumberNode& n, StreamWriter& writer, ExceptionSink* xsink);
    DLLLOCAL static int serializeDateToStream(const DateTimeNode& n, StreamWriter& writer, ExceptionSink* xsink);
    DLLLOCAL static int serializeBinaryToStream(const BinaryNode& n, StreamWriter& writer, ExceptionSink* xsink);

    DLLLOCAL static void serializeToStream(const QoreHashNode& h, OutputStream& stream, ExceptionSink* xsink);

    DLLLOCAL static QoreValue deserializeData(const QoreValue val, QoreInternalDeserializationContext& context,
            ExceptionSink* xsink);

    DLLLOCAL static int deserializeStaticClassVars(ExceptionSink* xsink, const QoreHashNode& svars,
            QoreInternalDeserializationContext& context);

    DLLLOCAL static QoreValue deserializeHashData(const QoreStringNode& type, const QoreHashNode& h,
            QoreInternalDeserializationContext& context, ExceptionSink* xsink, QoreHashNode* rv = nullptr);

    DLLLOCAL static QoreValue deserializeListData(QoreValue v, const QoreHashNode& h,
            QoreInternalDeserializationContext& context, ExceptionSink* xsink, QoreListNode* rv = nullptr);

    DLLLOCAL static QoreValue deserializeListData(const QoreListNode& l, QoreInternalDeserializationContext& context,
            ExceptionSink* xsink, QoreListNode* rv = nullptr);

    DLLLOCAL static QoreValue deserializeIndexedContainer(const char* key,
            QoreInternalDeserializationContext& context, ExceptionSink* xsink);
    DLLLOCAL static QoreValue deserializeIndexedWeakReference(const char* key,
            QoreInternalDeserializationContext& context, ExceptionSink* xsink);

    DLLLOCAL static QoreHashNode* deserializeHashFromStream(ExceptionSink* xsink, StreamReader& reader,
            qore_stream_type code, int64 flags);

    DLLLOCAL static QoreStringNode* deserializeStringFromStream(StreamReader& reader, qore_stream_type code,
            ExceptionSink* xsink);

    DLLLOCAL static int64 deserializeIntFromStream(StreamReader& reader, qore_stream_type code, ExceptionSink* xsink);

    DLLLOCAL static QoreListNode* deserializeListFromStream(ExceptionSink* xsink, StreamReader& reader, int64 flags);

    DLLLOCAL static double deserializeFloatFromStream(StreamReader& reader, ExceptionSink* xsink);

    DLLLOCAL static QoreNumberNode* deserializeNumberFromStream(StreamReader& reader, ExceptionSink* xsink);

    DLLLOCAL static DateTimeNode* deserializeAbsDateFromStream(StreamReader& reader, ExceptionSink* xsink);

    DLLLOCAL static DateTimeNode* deserializeRelDateFromStream(StreamReader& reader, ExceptionSink* xsink);

    DLLLOCAL static BinaryNode* deserializeBinaryFromStream(StreamReader& reader, ExceptionSink* xsink);
};

#endif // _QORE_CLASS_INTERN_QORESERIALIZABLE_H
