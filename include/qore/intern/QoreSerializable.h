/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreSerializable.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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
#include <utility>
#include <vector>

// maps from index strings to containers
typedef std::map<std::string, QoreValue> oimap_t;

//! Reserves the address of a source object, hash or list identified by a serialization index entry
/** Index entries identify source nodes by address, but a source node can be released before serialization
    completes; for example a serializeMembers() method or a builtin serializer can return or serialize newly created
    containers that are released as soon as they have been serialized.  If the node's memory were then reused for a
    new node, the new node would be taken for the old one and would be serialized as a reference to the old node's
    serialized data.

    This holds a weak reference (an existence reference for objects) to the node.  The reference keeps the node's
    memory allocated, and therefore its address unique, until the serialization context is destroyed; it does not
    keep the node's value alive, change its strong reference count or delay its destruction, except for a list or
    hash that was freed and whose other weak holders released their references: the weak holders of such a node hold
    a strong reference to it (see QoreContainerWeakRefs).  Releasing the reference cannot raise an exception.
*/
class QoreSerializationIdentity {
public:
    DLLLOCAL explicit QoreSerializationIdentity(const QoreObject& obj) : node(&obj), type(NT_OBJECT) {
        obj.tRef();
    }

    DLLLOCAL explicit QoreSerializationIdentity(const QoreHashNode& h) : node(&h), type(NT_HASH) {
        const_cast<QoreHashNode&>(h).weakRef();
    }

    DLLLOCAL explicit QoreSerializationIdentity(const QoreListNode& l) : node(&l), type(NT_LIST) {
        const_cast<QoreListNode&>(l).weakRef();
    }

    DLLLOCAL QoreSerializationIdentity(QoreSerializationIdentity&& old) noexcept : node(old.node), type(old.type) {
        old.node = nullptr;
    }

    DLLLOCAL ~QoreSerializationIdentity() {
        if (!node) {
            return;
        }
        switch (type) {
            case NT_OBJECT:
                const_cast<QoreObject*>(static_cast<const QoreObject*>(node))->tDeref();
                break;
            case NT_HASH:
                const_cast<QoreHashNode*>(static_cast<const QoreHashNode*>(node))->weakDeref();
                break;
            default:
                assert(type == NT_LIST);
                const_cast<QoreListNode*>(static_cast<const QoreListNode*>(node))->weakDeref();
                break;
        }
    }

    DLLLOCAL QoreSerializationIdentity(const QoreSerializationIdentity&) = delete;
    DLLLOCAL QoreSerializationIdentity& operator=(const QoreSerializationIdentity&) = delete;
    DLLLOCAL QoreSerializationIdentity& operator=(QoreSerializationIdentity&&) = delete;

private:
    const AbstractQoreNode* node;
    qore_type_t type;
};

//! a serialization index entry for a source node
struct QoreSerializationIndexEntry {
    //! the serialization index string of the node's serialized data
    std::string index;
    //! reserves the source node's address while it identifies this entry
    QoreSerializationIdentity source;

    DLLLOCAL QoreSerializationIndexEntry(std::string&& index, QoreSerializationIdentity&& source)
            : index(std::move(index)), source(std::move(source)) {
    }
};

// maps from object hashes to index entries
typedef std::map<std::string, QoreSerializationIndexEntry> imap_t;

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
    ENUM = 18,
};
}

class qore_class_private;

//! Returns the class context of the code that serializes, deserializes or formats (qore#7146)
/** In the frame of a builtin (Serializable::serialize(), sprintf(), ...), the class context of the code that called it;
    otherwise the class context of the running code.  Only a class that trusted code declared is returned, as only
    such a class's own objects are given with their state (see QoreSerializationCaller)
*/
DLLLOCAL const qore_class_private* qore_serialization_caller_class();

//! Returns true if the calling code itself is trusted, for the trusted serialization APIs (Program::deserialize(),
//! Program::serialize(), qore#7146)
/** The code of a class that trusted code declared, or the code of a trusted Program outside a class that is not a
    library module's; library code outside a class acts for its caller, which is trusted if no sandbox governs it
*/
DLLLOCAL bool qore_serialization_calling_code_trusted();

//! Decides for each object whether the code that serializes, deserializes or formats it gets the non-public state that
//! the classes declared by trusted code declare (qore#7146)
/** The code of a class that trusted code declared gets the state of the objects of the classes that it has private
    access to (its own hierarchy); for any other object, the state is withheld if sandboxed code is in the chain of
    callers (qore_caller_is_sandboxed(), which fails closed), so that trusted code that formats or serializes a value
    for a sandboxed caller does not give it the state of another class's object.  The trusted APIs give all state.
*/
class QoreSerializationCaller {
public:
    //! the calling code (see qore_serialization_caller_class())
    DLLLOCAL QoreSerializationCaller() : class_ctx(qore_serialization_caller_class()) {
    }

    //! for a trusted API: all state is given
    DLLLOCAL explicit QoreSerializationCaller(bool unrestricted) : unrestricted(unrestricted) {
    }

    //! returns true if the non-public declarations of class \a decl are withheld from the calling code
    /** The code of a class that trusted code declared gets the \c private:internal declarations of its own class and
        the \c private declarations of the classes that it has private access to (its parent and child classes), not
        those of other classes, such as the sibling base classes of an object's class; any other code does not get them
        if sandboxed code is in the chain of callers

        @param decl the class that declares the member or static variable
        @param internal true for a \c private:internal declaration
    */
    DLLLOCAL bool restricts(const qore_class_private& decl, bool internal);

    //! returns true if the non-public declarations of \a decl are withheld: \a decl is protected (it was declared by
    //! trusted code and does not inherit Qore::SandboxSerializable) and restricts() is true
    DLLLOCAL bool withholds(const qore_class_private& decl, bool internal);

    //! returns true if all state is given (a trusted API)
    DLLLOCAL bool isUnrestricted() const {
        return unrestricted;
    }

private:
    //! the class context of the calling code, if it is a class that trusted code declared
    const qore_class_private* class_ctx = nullptr;
    //! qore_caller_is_sandboxed(), when it is needed: -1 = not known yet
    int8_t chain_sandboxed = -1;
    //! true for a trusted API
    bool unrestricted = false;
};

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
    //! the code that serializes, which decides for each object whether it gets the non-public state of the classes
    //! that trusted code declared (qore#7146)
    QoreSerializationCaller caller;

    DLLLOCAL QoreInternalSerializationContext(ExceptionSink* xsink, int64 flags) : index(xsink), svars(xsink),
            flags(flags) {
    }

    //! for a trusted API (Program::serialize()): all state is given
    DLLLOCAL QoreInternalSerializationContext(ExceptionSink* xsink, int64 flags, bool unrestricted) : index(xsink),
            svars(xsink), flags(flags), caller(unrestricted) {
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

    //! serializes the static variables of \a cls; a non-public static variable that is withheld from the calling code
    //! and that has a value raises SERIALIZATION-ERROR (qore#7146)
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
    //! the code that deserializes, which decides for each object whether it may set the non-public state of the
    //! classes that trusted code declared (qore#7146)
    QoreSerializationCaller caller;
    //! for Program::deserialize(): the Programs of the trusted calling code (the Program of its class, such as a library
    //! module's, and the Programs of the thread's stack, such as the application's that it runs for), whose classes
    //! the names of the data must not name in the target Program as classes of sandboxed code (qore#7146)
    std::vector<QoreProgram*> caller_pgms;

    DLLLOCAL QoreInternalDeserializationContext(ExceptionSink* xsink, int64 flags) : oimap(xsink), flags(flags) {
    }

    //! for Program::deserialize(): all state is set
    DLLLOCAL QoreInternalDeserializationContext(ExceptionSink* xsink, int64 flags,
            std::vector<QoreProgram*>&& caller_pgms) : oimap(xsink), flags(flags), caller(true),
            caller_pgms(std::move(caller_pgms)) {
    }

    //! returns true if the names of the data are checked against the classes of the calling code (Program::deserialize())
    DLLLOCAL bool checksCallerClasses() const {
        return !caller_pgms.empty();
    }

    DLLLOCAL QoreValue deserializeContainer(const char* index_str, ExceptionSink* xsink);

    DLLLOCAL QoreValue deserializeValue(const QoreValue val, ExceptionSink* xsink);
};

class QoreSerializable : public AbstractPrivateData {
friend class QoreInternalSerializationContext;
friend class QoreInternalDeserializationContext;

public:
    DLLLOCAL static QoreHashNode* serializeToData(QoreValue val, int64 flags, ExceptionSink* xsink);

    //! Serializes \a val for trusted code with all state (Program::serializeToData(), qore#7146)
    DLLLOCAL static QoreHashNode* serializeToDataTrusted(QoreValue val, int64 flags, ExceptionSink* xsink);

    //! Serializes \a val for trusted code with all state to \a stream (Program::serialize(), qore#7146)
    DLLLOCAL static void serializeTrusted(const QoreValue val, OutputStream& stream, int64 flags,
            ExceptionSink* xsink);

    DLLLOCAL static void serialize(const QoreObject& self, OutputStream& stream, int64 flags, ExceptionSink* xsink);

    DLLLOCAL static void serialize(const QoreValue val, OutputStream& stream, int64 flags, ExceptionSink* xsink);

    DLLLOCAL static QoreHashNode* deserializeToData(ExceptionSink* xsink, InputStream& stream, int64 flags = 0);

    DLLLOCAL static QoreValue deserialize(ExceptionSink* xsink, InputStream& stream, int64 flags = 0);

    DLLLOCAL static QoreValue deserialize(ExceptionSink* xsink, const QoreHashNode& h, int64 flags = 0);

    //! Deserializes \a h for trusted code into the context of \a pgm (Program::deserialize(), qore#7146)
    /** Classes and type declarations are resolved in \a pgm, the objects get the Program of their class (or \a pgm),
        and the non-public members of the classes that trusted code declared are set as in the data, for the trusted
        calling code; the code of the classes (deserializeMembers(), transient member initializers) runs in \a pgm
    */
    DLLLOCAL static QoreValue deserializeForProgram(ExceptionSink* xsink, QoreProgram* pgm, const QoreHashNode& h,
            int64 flags = 0);

    //! as deserializeForProgram(), reading the data from \a stream
    DLLLOCAL static QoreValue deserializeForProgram(ExceptionSink* xsink, QoreProgram* pgm, InputStream& stream,
            int64 flags = 0);

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

    DLLLOCAL static QoreValue deserializeIntern(ExceptionSink* xsink, const QoreHashNode& h,
            QoreInternalDeserializationContext& context);

    DLLLOCAL static QoreHashNode* serializeToDataIntern(const QoreValue val, QoreInternalSerializationContext& context,
            ExceptionSink* xsink);

    //! Raises DESERIALIZATION-ERROR if \a cname names a class with protected non-public state in the Program of the
    //! code that calls Program::deserialize() (qore#7146)
    /** Called for a name that names a class that sandboxed code declared in the target Program: such a class would get
        the state that the data has for the caller's class

        @return 0 if OK, -1 if an exception was raised
    */
    DLLLOCAL static int checkProgramDeserializeClass(QoreInternalDeserializationContext& context, const char* cname,
            ExceptionSink* xsink);

    //! Raises DESERIALIZATION-ERROR if the hierarchy of \a cls has a non-transient non-public member of a protected
    //! class that is withheld from the calling code (qore#7146); returns -1 if an exception was raised
    DLLLOCAL static int checkDeserializationRefused(QoreInternalDeserializationContext& context, const QoreClass& cls,
            ExceptionSink* xsink);

    //! Raises SERIALIZATION-ERROR for a non-public member or static variable of class \a cls with a value that is
    //! withheld from the calling code (qore#7146)
    DLLLOCAL static void raiseWithheldError(const QoreClass& cls, const char* kind, const char* name,
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
            int64 flags, ExceptionSink* xsink);

    DLLLOCAL static int64 deserializeIntFromStream(StreamReader& reader, qore_stream_type code, ExceptionSink* xsink);

    DLLLOCAL static QoreListNode* deserializeListFromStream(ExceptionSink* xsink, StreamReader& reader, int64 flags);

    DLLLOCAL static double deserializeFloatFromStream(StreamReader& reader, ExceptionSink* xsink);

    DLLLOCAL static QoreNumberNode* deserializeNumberFromStream(StreamReader& reader, ExceptionSink* xsink);

    DLLLOCAL static DateTimeNode* deserializeAbsDateFromStream(StreamReader& reader, ExceptionSink* xsink);

    DLLLOCAL static DateTimeNode* deserializeRelDateFromStream(StreamReader& reader, ExceptionSink* xsink);

    DLLLOCAL static BinaryNode* deserializeBinaryFromStream(StreamReader& reader, ExceptionSink* xsink);

    DLLLOCAL static QoreValue deserializeEnumData(const QoreStringNode& enum_path,
        const QoreStringNode& member_name, ExceptionSink* xsink);
    DLLLOCAL static QoreValue deserializeEnumFromStream(ExceptionSink* xsink, StreamReader& reader);
};

#endif // _QORE_CLASS_INTERN_QORESERIALIZABLE_H
