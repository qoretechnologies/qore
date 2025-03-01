/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreSerializable.qpp

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

#include "qore/Qore.h"
#include "qore/intern/QoreSerializable.h"
#include "qore/intern/QC_Serializable.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/typed_hash_decl_private.h"
#include "qore/intern/QoreObjectIntern.h"
#include "qore/intern/ModuleInfo.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/qore_list_private.h"

#include <string>
#include <set>
#include <cstdint>
#include <cctype>
#include <cstdlib>
#include <vector>
#include <memory>

static QoreString QoreSerializationTypeString("QS");
static QoreString QoreSerializationVersionString("1.1");

static std::vector<std::string> serialization_versions = {
    "1.0",
    "1.1",
};

typedef std::set<std::string> strset_t;

// maximum string length for error message for string matching errors
#define QORE_SERIALIZATION_STRING_ERROR_MAX_LEN 80

namespace {
constexpr bool code_is_int(qore_stream_type t) {
    return t == qore_stream_type::INT1
        || t == qore_stream_type::INT2
        || t == qore_stream_type::INT4
        || t == qore_stream_type::INT8;
}

constexpr bool code_is_string(qore_stream_type t) {
    return t == qore_stream_type::STRING
        || t == qore_stream_type::UTF8_STRING;
}
}

ObjectIndexMap::~ObjectIndexMap() {
    for (auto& i : *this) {
        if (*xs) {
            // in case of an exception, we need to obliterate the object before dereferencing
            if (i.second.getType() == NT_OBJECT) {
                qore_object_private::get(*i.second.get<QoreObject>())->obliterate(xs);
            }
        } else {
            // otherwise we just need to dereference
            i.second.discard(xs);
        }
    }
}

static int stream_read_string(ExceptionSink* xsink, InputStream& stream, QoreString& str, int max_size = -1) {
    assert(max_size);
    str.clear();

    unsigned char c;
    while (true) {
        int64 rc = stream.read(&c, 1, xsink);
        if (*xsink) {
            return -1;
        }
        if (!rc) {
            xsink->raiseException("DESERIALIZATION-ERROR", "end of stream found while reading string value");
            return -1;
        }

        if (!c) {
            break;
        }

        if (max_size > 0 && str.size() == (size_t)max_size) {
            xsink->raiseException("DESERIALIZATION-ERROR", "string exceeded maximum size of %d while reading string value", max_size);
            return -1;
        }

        str.concat(c);
    }

    return 0;
}

static bool has_bin(const QoreString& str) {
    // see if string has binary data in it
    for (const char* p = str.c_str(), * e = p + str.size(); p < e; ++p) {
        if (*p < 32 || *p > 126) {
            return true;
        }
    }

    return false;
}

static int binary_string_deserialization_error(const char* type, const QoreString& expected, ExceptionSink* xsink) {
    xsink->raiseException("DESERIALIZATION-ERROR", "stream data does not match expected %s string value '%s'; read " \
        "binary (non-string) data instead", type, expected.c_str());
    return -1;
}

static int check_deserialization_string(const QoreString& str, const QoreString& expected, const char* type,
    ExceptionSink* xsink) {
    if (str != expected) {
        bool bin = has_bin(str);

        if (bin) {
            return binary_string_deserialization_error(type, expected, xsink);
        } else {
            if (str.size() > QORE_SERIALIZATION_STRING_ERROR_MAX_LEN) {
                ExceptionSink xsink2;
                std::unique_ptr<QoreString> tmp(str.substr(0, QORE_SERIALIZATION_STRING_ERROR_MAX_LEN, &xsink2));
                if (xsink2) {
                    // in case of an encoding error generating the error message, return a binary error instead
                    xsink2.clear();
                    return binary_string_deserialization_error(type, expected, xsink);
                }
                xsink->raiseException("DESERIALIZATION-ERROR", "expecting %s string value '%s' from " \
                    "stream; got '%s...' (string length %ld truncated to %d characters for display) instead", type,
                    expected.c_str(), tmp->c_str(), str.size(), QORE_SERIALIZATION_STRING_ERROR_MAX_LEN);
            } else {
                xsink->raiseException("DESERIALIZATION-ERROR", "expecting %s string value '%s' from " \
                    "stream; got '%s' instead", type, expected.c_str(), str.c_str());
            }
        }
        return -1;
    }
    return 0;
}

static int binary_string_deserialization_error_list(const char* type, const QoreString& expected, ExceptionSink* xsink) {
    xsink->raiseException("DESERIALIZATION-ERROR", "stream data does not match expected one of %s string values %s; " \
        "read binary (non-string) data instead", type, expected.c_str());
    return -1;
}

static int check_deserialization_string(const QoreString& str, const std::vector<std::string>& versions,
    const char* type, ExceptionSink* xsink) {
    for (auto& i : versions) {
        if (str == i) {
            return 0;
        }
    }

    bool bin = has_bin(str);

    // create descriptive string for exception
    QoreString desc;
    for (auto& i : versions) {
        desc.sprintf("'%s', ", i.c_str());
    }
    desc.terminate(desc.size() - 2);

    if (bin) {
            return binary_string_deserialization_error_list(type, desc, xsink);
    } else {
        if (str.size() > QORE_SERIALIZATION_STRING_ERROR_MAX_LEN) {
            ExceptionSink xsink2;
            std::unique_ptr<QoreString> tmp(str.substr(0, QORE_SERIALIZATION_STRING_ERROR_MAX_LEN, &xsink2));
            if (xsink2) {
                // in case of an encoding error generating the error message, return a binary error instead
                xsink2.clear();
                return binary_string_deserialization_error_list(type, desc, xsink);
            }
            xsink->raiseException("DESERIALIZATION-ERROR", "expecting one of %s string values %s from "
                "stream; got '%s...' (string length %ld truncated to %d characters for display) instead", type,
                desc.c_str(), tmp->c_str(), str.size(), QORE_SERIALIZATION_STRING_ERROR_MAX_LEN);
        } else {
            xsink->raiseException("DESERIALIZATION-ERROR", "expecting one of %s string values %s from stream; got "
                "'%s' instead", type, desc.c_str(), str.c_str());
        }
    }
    return -1;
}

int QoreInternalSerializationContext::serializeObject(const QoreObject& obj, std::string& index_str,
        ExceptionSink* xsink) {
    imap_t::iterator i = QoreSerializable::serializeObjectToIndex(obj, *this, xsink);
    if (*xsink) {
        return -1;
    }
    index_str = i->first;
    return 0;
}

int QoreInternalSerializationContext::serializeStaticVars(const QoreClass& cls, ExceptionSink* xsink) {
    cset_t::iterator i = cset.lower_bound(&cls);
    if (i != cset.end() && *i == &cls) {
        return 0;
    }
    cset.insert(i, &cls);
    ReferenceHolder<QoreHashNode> h(xsink);
    QoreClassStaticMemberIterator mi(cls);
    while (mi.next()) {
        const QoreExternalStaticMember& m = mi.getMember();
        QoreValue v = m.getValue();
        if (!v) {
            continue;
        }
        if (!h) {
            h = new QoreHashNode(autoTypeInfo);
        }
        h->setKeyValue(mi.getName(), serializeValue(v, xsink), xsink);
        if (*xsink) {
            return -1;
        }
    }
    if (h) {
        std::string str = cls.getNamespacePath();
        setSVars(str.c_str(), h.release(), xsink);
    }

    return 0;
}

int QoreInternalSerializationContext::serializeHash(const QoreHashNode& h, std::string& index_str,
        ExceptionSink* xsink) {
    imap_t::iterator i = QoreSerializable::serializeHashToIndex(h, *this, xsink);
    if (*xsink) {
        return -1;
    }
    index_str = i->first;
    return 0;
}

int QoreInternalSerializationContext::serializeList(const QoreListNode& l, std::string& index_str,
        ExceptionSink* xsink) {
    imap_t::iterator i = QoreSerializable::serializeListToIndex(l, *this, xsink);
    if (*xsink) {
        return -1;
    }
    index_str = i->first;
    return 0;
}

QoreValue QoreInternalSerializationContext::serializeValue(const QoreValue val, ExceptionSink* xsink) {
    return QoreSerializable::serializeValue(val, *this, xsink);
}

QoreValue QoreInternalDeserializationContext::deserializeContainer(const char* index_str, ExceptionSink* xsink) {
    return QoreSerializable::deserializeIndexedContainer(index_str, *this, xsink);
}

QoreValue QoreInternalDeserializationContext::deserializeValue(const QoreValue val, ExceptionSink* xsink) {
    return QoreSerializable::deserializeData(val, *this, xsink);
}

int QoreSerializationContext::serializeObject(const QoreObject& obj, std::string& index, ExceptionSink* xsink) {
    return reinterpret_cast<QoreInternalSerializationContext*>(this)->serializeObject(obj, index, xsink);
}

int QoreSerializationContext::serializeHash(const QoreHashNode& h, std::string& index, ExceptionSink* xsink) {
    return reinterpret_cast<QoreInternalSerializationContext*>(this)->serializeHash(h, index, xsink);
}

int QoreSerializationContext::serializeList(const QoreListNode& h, std::string& index, ExceptionSink* xsink) {
    return reinterpret_cast<QoreInternalSerializationContext*>(this)->serializeList(h, index, xsink);
}

QoreValue QoreSerializationContext::serializeValue(const QoreValue val, ExceptionSink* xsink) {
    return reinterpret_cast<QoreInternalSerializationContext*>(this)->serializeValue(val, xsink);
}

void QoreSerializationContext::addModule(const char* module) {
    reinterpret_cast<QoreInternalSerializationContext*>(this)->addModule(module);
}

int64 QoreSerializationContext::getFlags() const {
    return reinterpret_cast<const QoreInternalSerializationContext*>(this)->flags;
}

QoreValue QoreDeserializationContext::deserializeContainer(const char* index, ExceptionSink* xsink) {
    return reinterpret_cast<QoreInternalDeserializationContext*>(this)->deserializeContainer(index, xsink);
}

QoreValue QoreDeserializationContext::deserializeValue(const QoreValue val, ExceptionSink* xsink) {
    return reinterpret_cast<QoreInternalDeserializationContext*>(this)->deserializeValue(val, xsink);
}

int64 QoreDeserializationContext::getFlags() const {
    return reinterpret_cast<const QoreInternalDeserializationContext*>(this)->flags;
}

static QoreHashNode* serialization_get_index(imap_t::iterator i, bool weak) {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclIndexedObjectSerializationInfo, nullptr), nullptr);
    qore_hash_private* h = qore_hash_private::get(**rv);
    h->setKeyValueIntern(weak ? "_weak" : "_index", new QoreStringNode(i->second.c_str()));
    return rv.release();
}

QoreValue QoreSerializable::serializeValue(const QoreValue val, QoreInternalSerializationContext& context,
        ExceptionSink* xsink) {
    switch (val.getType()) {
        case NT_INT:
        case NT_STRING:
        case NT_BOOLEAN:
        case NT_FLOAT:
        case NT_NUMBER:
        case NT_NOTHING:
        case NT_NULL:
        case NT_BINARY:
        case NT_DATE:
            return val.refSelf();

        case NT_OBJECT: {
            ValueHolder rv(serializeObjectToData(*val.get<const QoreObject>(), false, context, xsink), xsink);
            return *xsink ? QoreValue() : rv.release();
        }

        case NT_WEAKREF: {
            ValueHolder rv(serializeObjectToData(*val.get<WeakReferenceNode>()->get(),
                context.flags & QSF_ALLOW_WEAKREFS ? true : false, context, xsink), xsink);
            return *xsink ? QoreValue() : rv.release();
        }

        case NT_WEAKREF_HASH: {
            ValueHolder rv(serializeHashToData(*val.get<WeakHashReferenceNode>()->get(),
                context.flags & QSF_ALLOW_WEAKREFS ? true : false, context, xsink), xsink);
            return *xsink ? QoreValue() : rv.release();
        }

        case NT_WEAKREF_LIST: {
            ValueHolder rv(serializeListToData(*val.get<WeakListReferenceNode>()->get(),
                context.flags & QSF_ALLOW_WEAKREFS ? true : false, context, xsink), xsink);
            return *xsink ? QoreValue() : rv.release();
        }

        case NT_HASH: {
            ValueHolder rv(serializeHashToData(*val.get<const QoreHashNode>(), false, context, xsink), xsink);
            return *xsink ? QoreValue() : rv.release();
        }

        case NT_LIST: {
            ValueHolder rv(serializeListToData(*val.get<const QoreListNode>(), false, context, xsink), xsink);
            return *xsink ? QoreValue() : rv.release();
        }

        default:
            break;
    }

    xsink->raiseException("SERIALIZATION-ERROR", "Cannot serialize type '%s'; type is not supported for "
        "serialization", val.getTypeName());
    return QoreValue();
}

QoreHashNode* QoreSerializable::serializeToData(const QoreValue val, int64 flags, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclSerializationInfo, xsink), xsink);
    QoreInternalSerializationContext context(xsink, flags);

    ValueHolder data(serializeValue(val, context, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }

    if (!context.mset.empty()) {
        ReferenceHolder<QoreListNode> l(new QoreListNode(autoTypeInfo), xsink);
        for (auto& i : context.mset) {
            l->push(new QoreStringNode(i), xsink);
        }
        rv->setKeyValue("_modules", l.release(), xsink);
    }

    if (context.index) {
        rv->setKeyValue("_index", context.index.release(), xsink);
    }

    if (context.svars) {
        rv->setKeyValue("_sdata", context.svars.release(), xsink);
    }

    rv->setKeyValue("_data", data.release(), xsink);

    return rv.release();
}

imap_t::iterator QoreSerializable::serializeObjectToIndex(const QoreObject& obj,
        QoreInternalSerializationContext& context, ExceptionSink* xsink) {
    // see if object is in index
    QoreString str;
    qore_get_ptr_hash(str, &obj);

    imap_t::iterator i = context.imap.lower_bound(str.c_str());

    return i != context.imap.end() && i->first == str.c_str()
        ? i
        : serializeObjectToIndexIntern(obj, context, str, i, xsink);
}

QoreValue QoreSerializable::serializeObjectToData(const QoreObject& obj, bool weak,
        QoreInternalSerializationContext& context, ExceptionSink* xsink) {
    imap_t::iterator i = serializeObjectToIndex(obj, context, xsink);
    return *xsink ? QoreValue() : serialization_get_index(i, weak);
}

imap_t::iterator QoreSerializable::serializeObjectToIndexIntern(const QoreObject& self,
        QoreInternalSerializationContext& context, const QoreString& str, imap_t::iterator hint,
        ExceptionSink* xsink) {
    const QoreClass& cls = *self.getClass();

    if (context.flags & QSF_INCLUDE_STATIC_CLASS_VARS) {
        QoreClassHierarchyIterator i(*self.getClass());
        while (i.next()) {
            if (context.serializeStaticVars(i.get(), xsink)) {
                return context.imap.end();
            }
        }
        imap_t::iterator i0 = context.imap.find(str.c_str());
        if (i0 != context.imap.end()) {
            return i0;
        }
    }

    // first write object to index
    assert(context.imap.find(str.c_str()) == context.imap.end());
    std::string index_str = std::to_string(context.imap.size());
    imap_t::iterator i = context.imap.insert(hint, imap_t::value_type(str.c_str(), index_str));

    ReferenceHolder<QoreHashNode> h(new QoreHashNode(hashdeclObjectSerializationInfo, xsink), xsink);
    h->setKeyValue("_class", new QoreStringNode(cls.getNamespacePath()), xsink);

    ReferenceHolder<QoreHashNode> class_data(xsink);

    QoreClassHierarchyIterator ci(cls);
    while (ci.next()) {
        // do not process virtual classes
        if (ci.isVirtual()) {
            continue;
        }

        const QoreClass& current_cls = ci.get();

        // insert module if necessary
        {
            const char* module_name = current_cls.getModuleName();
            if (module_name) {
                context.mset.insert(module_name);
            }
        }

        // check if the class inherits Serializable and throw an exception if not
        {
            bool priv = false;
            if (!current_cls.getClass(*QC_SERIALIZABLE, priv)) {
                SimpleRefHolder<QoreStringNode> desc(new QoreStringNodeMaker("Cannot serialize class '%s' as it does "
                    "not inherit 'Serializable' and therefore is not eligible for serialization",
                    current_cls.getName()));
                if (!current_cls.isSystem()) {
                    desc->sprintf("; to correct this error, declare Serializable as a parent class of '%s'",
                        current_cls.getName());
                }
                if (&cls != &current_cls) {
                    desc->sprintf(" (while serializing an object of class '%s')", cls.getName());
                }
                xsink->raiseException("SERIALIZATION-ERROR", desc.release());
                return context.imap.end();
            }
        }

        // serialize class members for each member of the hierarchy separately
        ReferenceHolder<QoreHashNode> class_members(xsink);

        if (current_cls.isSystem()) {
            q_serializer_t serializer = current_cls.getSerializer();
            assert(serializer);

            // get class private data for serialization call
            ReferenceHolder<AbstractPrivateData> private_data(self.getReferencedPrivateData(current_cls.getID(),
                xsink), xsink);
            if (*xsink) {
                return context.imap.end();
            }

            class_members = serializer(self, **private_data, reinterpret_cast<QoreSerializationContext&>(context),
                xsink);
            if (*xsink) {
                return context.imap.end();
            }
        } else {
            // see if the local class has a serializeMembers() method defined
            const QoreMethod* serializeMembers = current_cls.findLocalMethod("serializeMembers");

            // iterate all nornal members in the class
            QoreClassMemberIterator mi(ci.get());
            while (mi.next()) {
                const QoreExternalNormalMember& m = mi.getMember();
                // skip members marked as transient
                if (m.isTransient()) {
                    continue;
                }
                const char* mname = mi.getName();

                ValueHolder val(self.getReferencedMemberNoMethod(mname, &current_cls, xsink), xsink);
                if (*xsink) {
                    return context.imap.end();
                }

                // skip members with no value
                if (!val) {
                    continue;
                }

                ValueHolder new_val(serializeValue(*val, context, xsink), xsink);
                if (*xsink) {
                    xsink->appendLastDescription(" (while serializing object member '%s::%s')", current_cls.getName(), mname);
                    return context.imap.end();
                }

                if (!class_members) {
                    class_members = new QoreHashNode(autoTypeInfo);
                }
                class_members->setKeyValue(mname, new_val.release(), xsink);
            }

            if (serializeMembers) {
                ReferenceHolder<QoreListNode> call_args(xsink);
                if (class_members) {
                    call_args = new QoreListNode(autoTypeInfo);
                    call_args->push(class_members.release(), xsink);
                }

                ValueHolder val(xsink);
                {
                    ObjectSubstitutionHelper osh(const_cast<QoreObject*>(&self), qore_class_private::get(current_cls));
                    val = const_cast<QoreObject&>(self).evalMethod(*serializeMembers, *call_args, xsink);
                }
                if (*xsink) {
                    return context.imap.end();
                }
                if (val) {
                    if (val->getType() != NT_HASH) {
                        xsink->raiseException("SERIALIZATION-ERROR", "%s::serializeMembers() returned type '%s'; "
                            "expecting 'hash' or 'nothing'",
                            current_cls.getName(), val->getFullTypeName());
                        return context.imap.end();
                    }
                    // serialize data returned
                    ReferenceHolder<QoreHashNode> serialized_member_data(new QoreHashNode(autoTypeInfo), xsink);
                    ConstHashIterator mhi(val->get<QoreHashNode>());
                    while (mhi.next()) {
                        ValueHolder new_val(serializeValue(mhi.get(), context, xsink), xsink);
                        if (*xsink) {
                            xsink->appendLastDescription(" (while serializing hash key '%s')", mhi.getKey());
                            return context.imap.end();
                        }
                        serialized_member_data->setKeyValue(mhi.getKey(), new_val.release(), xsink);
                    }
                    class_members = serialized_member_data.release();
                } else {
                    class_members = nullptr;
                }
            }
        }

        if (class_members) {
            if (!class_data) {
                class_data = new QoreHashNode(autoHashTypeInfo);
            }
            std::string class_path = current_cls.getNamespacePath();
            class_data->setKeyValue(class_path.c_str(), class_members.release(), xsink);
        }
    }

    if (class_data) {
        h->setKeyValue("_class_data", class_data.release(), xsink);
    }

    context.setIndex(index_str.c_str(), h.release(), xsink);

    return i;
}

imap_t::iterator QoreSerializable::serializeHashToIndex(const QoreHashNode& h,
        QoreInternalSerializationContext& context, ExceptionSink* xsink) {
    // see if object is in index
    QoreString str;
    qore_get_ptr_hash(str, &h);

    imap_t::iterator i = context.imap.lower_bound(str.c_str());

    return i != context.imap.end() && i->first == str.c_str()
        ? i
        : serializeHashToIndexIntern(h, context, str, i, xsink);
}

QoreValue QoreSerializable::serializeHashToData(const QoreHashNode& h, bool weak,
        QoreInternalSerializationContext& context, ExceptionSink* xsink) {
    imap_t::iterator i = serializeHashToIndex(h, context, xsink);
    return *xsink ? QoreValue() : serialization_get_index(i, weak);
}

imap_t::iterator QoreSerializable::serializeHashToIndexIntern(const QoreHashNode& h,
        QoreInternalSerializationContext& context, const QoreString& str,
        imap_t::iterator hint, ExceptionSink* xsink) {
    // first write to imap
    assert(context.imap.find(str.c_str()) == context.imap.end());
    std::string index_str = std::to_string(context.imap.size());
    imap_t::iterator i = context.imap.insert(hint, imap_t::value_type(str.c_str(), index_str));

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclHashSerializationInfo, xsink), xsink);

    const TypedHashDecl* thd = h.getHashDecl();
    if (thd) {
        const char* module_name = thd->getModuleName();
        //printd(5, "hashdecl '%s' mod: '%s'\n", thd->getNamespacePath().c_str(), module_name);
        if (module_name) {
            context.mset.insert(module_name);
        }
        rv->setKeyValue("_hash", new QoreStringNode(thd->getNamespacePath()), xsink);
    } else {
        // issue #3318: write complex type to stream, if any
        const QoreTypeInfo* vti = h.getValueTypeInfo();
        //printd(5, "QoreSerializable::serializeHashToData() vti: '%s'\n", QoreTypeInfo::getName(vti));
        rv->setKeyValue("_hash", new QoreStringNodeMaker("^%s", QoreTypeInfo::getName(vti)), xsink);
    }

    // serialize hash members
    ReferenceHolder<QoreHashNode> hash_members(xsink);

    ConstHashIterator hi(h);
    while (hi.next()) {
        ValueHolder new_val(serializeValue(hi.get(), context, xsink), xsink);
        if (*xsink) {
            xsink->appendLastDescription(" (while serializing hash key '%s')", hi.getKey());
            return context.imap.end();
        }

        if (!hash_members) {
            hash_members = new QoreHashNode(autoTypeInfo);
        }
        // always set the hash key even if it has no value
        hash_members->setKeyValue(hi.getKey(), new_val.release(), xsink);
    }

    if (hash_members) {
        rv->setKeyValue("_members", hash_members.release(), xsink);
    }

    context.setIndex(index_str.c_str(), rv.release(), xsink);

    return i;
}

imap_t::iterator QoreSerializable::serializeListToIndex(const QoreListNode& l,
        QoreInternalSerializationContext& context, ExceptionSink* xsink) {
    // see if object is in index
    QoreString str;
    qore_get_ptr_hash(str, &l);

    imap_t::iterator i = context.imap.lower_bound(str.c_str());

    return i != context.imap.end() && i->first == str.c_str()
        ? i
        : serializeListToIndexIntern(l, context, str, i, xsink);
}

QoreValue QoreSerializable::serializeListToData(const QoreListNode& l, bool weak,
        QoreInternalSerializationContext& context, ExceptionSink* xsink) {
    imap_t::iterator i = serializeListToIndex(l, context, xsink);
    return *xsink ? QoreValue() : serialization_get_index(i, weak);
}

imap_t::iterator QoreSerializable::serializeListToIndexIntern(const QoreListNode& l,
        QoreInternalSerializationContext& context, const QoreString& str,
        imap_t::iterator hint, ExceptionSink* xsink) {
    // first write to imap
    assert(context.imap.find(str.c_str()) == context.imap.end());
    std::string index_str = std::to_string(context.imap.size());
    imap_t::iterator i = context.imap.insert(hint, imap_t::value_type(str.c_str(), index_str));

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclListSerializationInfo, xsink), xsink);

    // issue #3318: write complex type to stream, if any
    const QoreTypeInfo* vti = l.getValueTypeInfo();
    //printd(5, "QoreSerializable::serializeHashToData() vti: '%s'\n", QoreTypeInfo::getName(vti));
    rv->setKeyValue("_list", new QoreStringNode(QoreTypeInfo::getName(vti)), xsink);

    ReferenceHolder<QoreListNode> serialized_list(xsink);

    ConstListIterator li(l);
    while (li.next()) {
        ValueHolder new_val(serializeValue(li.getValue(), context, xsink), xsink);
        if (*xsink) {
            xsink->appendLastDescription(" (while serializing list element %lu)", li.index() + 1);
            return context.imap.end();
        }

        if (!serialized_list) {
            serialized_list = new QoreListNode(autoTypeInfo);
        }

        serialized_list->push(new_val.release(), xsink);
    }

    if (serialized_list) {
        rv->setKeyValue("_elements", serialized_list.release(), xsink);
    }

    context.setIndex(index_str.c_str(), rv.release(), xsink);

    return i;
}

QoreValue QoreSerializable::deserialize(ExceptionSink* xsink, const QoreHashNode& h, int64 flags) {
    assert(hashdeclSerializationInfo->equal(h.getHashDecl()));

    QoreInternalDeserializationContext context(xsink, flags);

    QoreProgram* pgm = getProgram();

    // load any required modules
    QoreValue val = h.getKeyValue("_modules");
    if (val) {
        if (val.getType() != NT_LIST) {
            xsink->raiseException("DESERIALIZATION-ERROR", "invalid '_modules' type; expecting 'list', got '%s'",
                val.getFullTypeName());
            return QoreValue();
        }

        ConstListIterator li(val.get<const QoreListNode>());
        while (li.next()) {
            val = li.getValue();
            assert(val.getType() == NT_STRING);
            QMM.runTimeLoadModule(*xsink, *xsink, val.get<const QoreStringNode>()->c_str(), pgm);
            if (*xsink) {
                return QoreValue();
            }
        }
    }

    val = h.getKeyValue("_index");
    if (val) {
        if (val.getType() != NT_HASH) {
            xsink->raiseException("DESERIALIZATION-ERROR", "invalid '_index' type; expecting 'hash', got '%s'",
                val.getFullTypeName());
            return QoreValue();
        }
        const QoreHashNode* index = val.get<const QoreHashNode>();
        // first create the objects and their indices
        ConstHashIterator hi(index);
        while (hi.next()) {
            const char* key = hi.getKey();
            const QoreHashNode* oh = hi.get().get<const QoreHashNode>();
            if (hashdeclObjectSerializationInfo->equal(oh->getHashDecl())) {
                QoreValue v = oh->getKeyValue("_class");
                assert(v.getType() == NT_STRING);
                const char* cname = v.get<const QoreStringNode>()->c_str();
                const QoreClass* cls = pgm->findClass(cname, xsink);
                if (!cls) {
                    if (!*xsink) {
                        xsink->raiseException("DESERIALIZATION-ERROR", "Cannot find class '%s' required for "
                            "deserialization", cname);
                    }
                    return QoreValue();
                }
                QoreObject* obj = new QoreObject(cls, pgm);
                assert(context.oimap.find(key) == context.oimap.end());
                context.oimap.insert(oimap_t::value_type(key, obj));
            } else if (hashdeclHashSerializationInfo->equal(oh->getHashDecl())) {
                ValueHolder holder(new QoreHashNode(autoTypeInfo), xsink);
                assert(context.oimap.find(key) == context.oimap.end());
                context.oimap.insert(oimap_t::value_type(key, holder.release()));
            } else if (hashdeclListSerializationInfo->equal(oh->getHashDecl())) {
                ValueHolder holder(new QoreListNode(autoTypeInfo), xsink);
                assert(context.oimap.find(key) == context.oimap.end());
                context.oimap.insert(oimap_t::value_type(key, holder.release()));
            } else {
                if (!*xsink) {
                    xsink->raiseException("DESERIALIZATION-ERROR", "Invalid data in container index in serialized "
                        "data");
                }
                return QoreValue();
            }
        }

        // iterate the hash again
        while (hi.next()) {
            const char* key = hi.getKey();
            const QoreHashNode* oh = hi.get().get<const QoreHashNode>();

            if (hashdeclHashSerializationInfo->equal(oh->getHashDecl())) {
                QoreValue v = oh->getKeyValue("_hash");
                if (v.getType() != NT_STRING) {
                    xsink->raiseException("DESERIALIZATION-ERROR", "'_hash' key has invalid type '%s'; expecting "
                        "'string'", v.getTypeName());
                    return QoreValue();
                }

                QoreHashNode* val = context.oimap.find(key)->second.get<QoreHashNode>();
                // deserialize in place
                deserializeHashData(*v.get<const QoreStringNode>(), *oh, context, xsink, val);
                if (*xsink) {
                    return QoreValue();
                }
                continue;
            } else if (hashdeclListSerializationInfo->equal(oh->getHashDecl())) {
                QoreValue v = oh->getKeyValue("_list");
                QoreListNode* val = context.oimap.find(key)->second.get<QoreListNode>();
                // deserialize in place
                deserializeListData(v, *oh, context, xsink, val);
                if (*xsink) {
                    return QoreValue();
                }
                continue;
            }

            assert(hashdeclObjectSerializationInfo->equal(oh->getHashDecl()));
            QoreObject* obj = context.oimap.find(key)->second.get<QoreObject>();

            const QoreClass& cls = *obj->getClass();

            // deserialize each class in the hierarchy separately
            QoreValue v = oh->getKeyValue("_class_data");
            assert(!v || v.getType() == NT_HASH);
            const QoreHashNode* mh = v ? v.get<const QoreHashNode>() : nullptr;
            // make sure we use all the keys in the hash
            size_t found = 0;

            // ensure that the serialization hash includes all classes necessary for the parsent class
            QoreClassHierarchyIterator chi(cls);

            while (chi.next()) {
                // do not process virtual classes
                if (chi.isVirtual()) {
                    continue;
                }

                const QoreClass& mcls = chi.get();

                //printd(5, "iterating %p '%s' GOT CHILD %p '%s'\n", cls, cls->getName(), mcls,
                //    mcls.getNamespacePath().c_str());

                // check if the class inherits Serializable and throw an exception if not
                {
                    bool priv = false;
                    if (!mcls.getClass(*QC_SERIALIZABLE, priv)) {
                        xsink->raiseException("DESERIALIZATION-ERROR", "cannot deserialize class '%s' as it does not "
                            "inherit 'Serializable' and therefore is not eligible for deserialization'",
                            mcls.getName());
                        return QoreValue();
                    }
                }

                std::string class_path = mcls.getNamespacePath();

                QoreHashNode* cmh = nullptr;

                QoreValue cv;
                if (mh) {
                    bool exists;
                    cv = mh->getKeyValueExistence(class_path.c_str(), exists);
                    if (exists) {
                        ++found;
                        if (cv) {
                            if (cv.getType() != NT_HASH) {
                                xsink->raiseException("DESERIALIZATION-ERROR", "serialized data for class '%s' has "
                                    "type '%s'; expecting 'hash' or 'nothing'",
                                    mcls.getName(), cv.getTypeName());
                                return QoreValue();
                            }
                            cmh = cv.get<QoreHashNode>();
                        }
                    }
                }

                if (mcls.isSystem()) {
                    q_deserializer_t deserializer = mcls.getDeserializer();
                    assert(deserializer);

                    deserializer(*obj, cmh, reinterpret_cast<QoreDeserializationContext&>(context), xsink);
                    if (*xsink) {
                        return QoreValue();
                    }
                } else {
                    // see if the local class has a deserializeMembers() method defined
                    const QoreMethod* deserializeMembers = mcls.findLocalMethod("deserializeMembers");

                    // build deserialized meember hash for deserializeMembers() method if it exists
                    ReferenceHolder<QoreHashNode> dmh(deserializeMembers ? new QoreHashNode(autoTypeInfo) : nullptr,
                        xsink);

                    if (cmh) {
                        // deserialize members
                        ConstHashIterator cmhi(cmh);
                        while (cmhi.next()) {
                            ValueHolder vh(deserializeData(cmhi.get(), context, xsink), xsink);
                            if (*xsink) {
                                return QoreValue();
                            }

                            if (!deserializeMembers) {
                                obj->setMemberValue(cmhi.getKey(), &mcls, *vh, xsink);
                            } else {
                                dmh->setKeyValue(cmhi.getKey(), vh.release(), xsink);
                            }
                            if (*xsink) {
                                return QoreValue();
                            }
                        }
                    }

                    if (deserializeMembers) {
                        ReferenceHolder<QoreListNode> call_args(new QoreListNode(autoTypeInfo), xsink);
                        call_args->push(dmh.release(), xsink);
                        ObjectSubstitutionHelper osh(obj, qore_class_private::get(mcls));
                        ValueHolder val(obj->evalMethod(*deserializeMembers, *call_args, xsink), xsink);
                        if (*xsink) {
                            return QoreValue();
                        }
                    } else {
                        // initialize transient members
                        if (mcls.hasTransientMember()) {
                            QoreClassMemberIterator mi(mcls);
                            while (mi.next()) {
                                const QoreExternalNormalMember& mem = mi.getMember();
                                if (!mem.isTransient()) {
                                    continue;
                                }
                                ValueHolder val(mem.getDefaultValue(xsink), xsink);
                                if (*xsink) {
                                    return QoreValue();
                                }
                                // skip transient member initialization if there is no expression
                                if (!val) {
                                    //printd(5, "DESERIALIZE transient member %s::%s has no value\n", mcls.getName(),
                                    //    mi.getName());
                                    continue;
                                }
                                // assign value to member
                                obj->setMemberValue(mi.getName(), &mcls, *val, xsink);
                                if (*xsink) {
                                    return QoreValue();
                                }
                            }
                        }
                    }
                }
            }

            // get hierarchy size in terms of serialized data
            size_t hsize = mh ? mh->size() : 0;
            // throw an exception if we did not use all the keys in the serialization hash
            if (found < hsize) {
                // get list of "extra" classes in serialization hash
                strset_t strset;

                // first get a set of all classes in the serialization hash
                ConstHashIterator chi2(mh);
                while (chi2.next()) {
                    strset.insert(chi2.getKey());
                }

                // remove all matching classes in the hierarchy
                QoreClassHierarchyIterator chi3(cls);
                while (chi3.next()) {
                    strset_t::iterator i = strset.find(chi3.get().getNamespacePath());
                    if (i != strset.end()) {
                        strset.erase(i);
                    }
                }

                // create the error string
                SimpleRefHolder<QoreStringNode> desc(new QoreStringNodeMaker("incompatible class hierarchy; %d "
                    "class%s in serialization data, but %d used for deserialization; unmatched classes: ",
                    static_cast<int>(hsize), hsize == 1 ? "" : "es", static_cast<int>(found)));

                for (auto& i : strset) {
                    desc->sprintf("'%s', ", i.c_str());
                }

                desc->terminate(desc->size() - 2);

                xsink->raiseException("DESERIALIZATION-ERROR", desc.release());
                return QoreValue();
            }
        }
    }

    if (flags & QDF_STATIC_CLASS_VARS) {
        val = h.getKeyValue("_sdata");
        if (val) {
            if (val.getType() != NT_HASH) {
                xsink->raiseException("DESERIALIZATION-ERROR", "invalid '_sdata' type; expecting 'hash', got '%s'",
                    val.getFullTypeName());
                return QoreValue();
            }
            const QoreHashNode* svars = val.get<const QoreHashNode>();
            if (deserializeStaticClassVars(xsink, *svars, context)) {
                return QoreValue();
            }
        }
    }

    return deserializeData(h.getKeyValue("_data"), context, xsink);
}

int QoreSerializable::deserializeStaticClassVars(ExceptionSink* xsink, const QoreHashNode& svars,
        QoreInternalDeserializationContext& context) {
    ConstHashIterator i(svars);
    QoreProgram* pgm = getProgram();
    while (i.next()) {
        const char* cname = i.getKey();
        const QoreClass* cls = pgm->findClass(cname, xsink);
        if (*xsink) {
            return -1;
        }
        if (!cls) {
            xsink->raiseException("DESERIALIZATION-ERROR", "Cannot find class '%s' to set static class vars; failed "
                "to deserialize data", cname);
            return -1;
        }
        QoreValue v = i.get();
        if (v.getType() != NT_HASH) {
            xsink->raiseException("DESERIALIZATION-ERROR", "Class '%s' static var value expected with type 'hash'; "
                "got type '%s' instead", cname, v.getFullTypeName());
            return -1;
        }
        const QoreHashNode& mh = *v.get<const QoreHashNode>();
        ConstHashIterator ci(mh);
        while (ci.next()) {
            const QoreExternalStaticMember* m = cls->findLocalStaticMember(ci.getKey());
            if (!m) {
                xsink->raiseException("DESERIALIZATION-ERROR", "Class '%s' has no static var '%s'", cname, ci.getKey());
                return -1;
            }
            QoreValue v = deserializeData(ci.get(), context, xsink);
            if (*xsink) {
                return -1;
            }
            ReferenceHolder<QoreHashNode> h(xsink);
            if (context.flags & QDF_MERGE_STATIC_CLASS_VARS && (m->getValue().getType() == NT_HASH)
                && (v.getType() == NT_HASH)) {
                h = m->getValue().get<const QoreHashNode>()->realCopy();
                h->merge(v.get<const QoreHashNode>(), xsink);
                if (*xsink) {
                    return -1;
                }
                v = *h;
            }
            if (m->setValue(v, xsink)) {
                xsink->appendLastDescription(" (while setting static var '%s::%s')", cname, ci.getKey());
                return -1;
            }
        }
    }
    return 0;
}

QoreValue QoreSerializable::deserializeIndexedContainer(const char* key, QoreInternalDeserializationContext& context,
        ExceptionSink* xsink) {
    oimap_t::const_iterator i = context.oimap.find(key);
    if (i == context.oimap.end()) {
        xsink->raiseException("DESERIALIZATION-ERROR", "'_index' value '%s' is invalid; no such index exists", key);
        return QoreValue();
    }

    return i->second.refSelf();
}

QoreValue QoreSerializable::deserializeIndexedWeakReference(const char* key,
        QoreInternalDeserializationContext& context, ExceptionSink* xsink) {
    oimap_t::const_iterator i = context.oimap.find(key);
    if (i == context.oimap.end()) {
        xsink->raiseException("DESERIALIZATION-ERROR", "'_weak' value '%s' is invalid; no such index exists", key);
        return QoreValue();
    }
    if (i->second.getType() == NT_OBJECT) {
        return new WeakReferenceNode(const_cast<QoreObject*>(i->second.get<QoreObject>()));
    } else if (i->second.getType() == NT_HASH) {
        return new WeakHashReferenceNode(const_cast<QoreHashNode*>(i->second.get<QoreHashNode>()));
    }

    assert(i->second.getType() == NT_LIST);
    return new WeakListReferenceNode(const_cast<QoreListNode*>(i->second.get<QoreListNode>()));
}

QoreValue QoreSerializable::deserializeData(const QoreValue val, QoreInternalDeserializationContext& context,
        ExceptionSink* xsink) {
    if (val.getType() == NT_HASH) {
        const QoreHashNode* h = val.get<const QoreHashNode>();
        QoreValue v = h->getKeyValue("_hash");
        if (v) {
            if (v.getType() != NT_STRING) {
                xsink->raiseException("DESERIALIZATION-ERROR", "'_hash' key has invalid type '%s'; expecting "
                    "'string'", v.getTypeName());
                return QoreValue();
            }

            return deserializeHashData(*v.get<const QoreStringNode>(), *h, context, xsink);
        }
        v = h->getKeyValue("_index");
        if (v) {
            if (v.getType() != NT_STRING) {
                xsink->raiseException("DESERIALIZATION-ERROR", "'_index' key has invalid type '%s'; expecting 'string'",
                    v.getTypeName());
                return QoreValue();
            }
            const char* key = v.get<const QoreStringNode>()->c_str();
            return deserializeIndexedContainer(key, context, xsink);
        }
        v = h->getKeyValue("_weak");
        if (v) {
            if (v.getType() != NT_STRING) {
                xsink->raiseException("DESERIALIZATION-ERROR", "'_index' key has invalid type '%s'; expecting 'string'",
                    v.getTypeName());
                return QoreValue();
            }
            const char* key = v.get<const QoreStringNode>()->c_str();
            return deserializeIndexedWeakReference(key, context, xsink);
        }
        v = h->getKeyValue("_list");
        if (v) {
            return deserializeListData(v, *h, context, xsink);
        }

        xsink->raiseException("DESERIALIZATION-ERROR", "hash hash no type information for deserialization; expecting "
            "either '_hash' or '_index' keys; neither was found");
        return QoreValue();
    }

    // ffor backwards compatibility with data serialized by serializer 1.0
    if (val.getType() == NT_LIST) {
        return deserializeListData(*val.get<const QoreListNode>(), context, xsink);
    }

    return val.refSelf();
}

QoreValue QoreSerializable::deserializeHashData(const QoreStringNode& type, const QoreHashNode& h,
        QoreInternalDeserializationContext& context, ExceptionSink* xsink, QoreHashNode* rval) {
    // get members, if any
    const QoreValue members = h.getKeyValue("_members");

    if (members && members.getType() != NT_HASH) {
        xsink->raiseException("DESERIALIZATION-ERROR", "'_members' key has invalid type '%s'; expecting 'hash' or "
            "'nothing'", members.getTypeName());
        return QoreValue();
    }

    // deserialize members first, then return hash
    ReferenceHolder<QoreHashNode> rv(xsink);
    if (rval && (type[0] == '^')) {
        rv = rval;
    } else {
        rv = new QoreHashNode(autoTypeInfo);
    }
    qore_hash_private* ph = qore_hash_private::get(**rv);
    if (members) {
        // deserialize hash members
        ConstHashIterator mi(members.get<const QoreHashNode>());
        while (mi.next()) {
            ValueHolder val(deserializeData(mi.get(), context, xsink), xsink);
            if (*xsink) {
                return QoreValue();
            }
            ph->setKeyValueIntern(mi.getKey(), val.release());
        }
    }

    if (type[0] == '^') {
        // make sure we can still deserialize v1.0 data where the type value is '^hash^'
        if (type != "^hash^") {
            const char* value_type = type.c_str() + 1;
            const QoreTypeInfo* vti = qore_get_type_from_string_intern(value_type);
            //printd(5, "QoreSerializable::deserializeHashData() vti: '%s'\n", value_type);
            if (!vti) {
                xsink->raiseException("DESERIALIZATION-ERROR", "'hash has value type '%s' which cannot be matched to a "
                    "known type", value_type);
                return QoreValue();
            }
            ph->complexTypeInfo = (vti == anyTypeInfo ? nullptr : qore_get_complex_hash_type(vti));
        }
        return rv.release();
    }

    const QoreNamespace* pns = nullptr;
    const TypedHashDecl* hd = getProgram()->findHashDecl(type.c_str(), pns);
    if (!hd) {
        xsink->raiseException("DESERIALIZATION-ERROR", "'_hash' key indicates that a '%s' typed hash should be "
            "deserialized, but no such typed hash (hashdecl) could be found in the current Program object",
            type.c_str());
        return QoreValue();
    }

    // do the runtime cast
    return typed_hash_decl_private::get(*hd)->newHash(*rv, true, xsink, rval);
}

QoreValue QoreSerializable::deserializeListData(QoreValue v, const QoreHashNode& h,
        QoreInternalDeserializationContext& context, ExceptionSink* xsink, QoreListNode* rval) {
    if (v.getType() != NT_STRING) {
        xsink->raiseException("DESERIALIZATION-ERROR", "'_list' key has invalid type '%s'; expecting 'string'",
            v.getTypeName());
        return QoreValue();
    }

    // get element type
    const char* value_type = v.get<QoreStringNode>()->c_str();
    const QoreTypeInfo* vti = qore_get_type_from_string_intern(value_type);
    if (!vti) {
        xsink->raiseException("DESERIALIZATION-ERROR", "'list has value type '%s' which cannot be matched to a "
            "known type", value_type);
        return QoreValue();
    }

    // get elements, if any
    const QoreValue elements = h.getKeyValue("_elements");

    if (elements && elements.getType() != NT_LIST) {
        xsink->raiseException("DESERIALIZATION-ERROR", "'_elements' key has invalid type '%s'; expecting 'list' "
            "or 'nothing'", elements.getTypeName());
        return QoreValue();
    }

    if (elements.isNothing()) {
        return rval ? rval : new QoreListNode(vti);
    }
    ValueHolder rv(deserializeListData(*elements.get<const QoreListNode>(), context, xsink, rval), xsink);
    if (*xsink) {
        return QoreValue();
    }
    qore_list_private* pl = qore_list_private::get(*rv->get<QoreListNode>());
    pl->complexTypeInfo = (vti == anyTypeInfo ? nullptr : qore_get_complex_list_type(vti));
    return rv.release();
}

QoreValue QoreSerializable::deserializeListData(const QoreListNode& l, QoreInternalDeserializationContext& context,
        ExceptionSink* xsink, QoreListNode* rval) {
    ReferenceHolder<QoreListNode> rv(rval ? rval : new QoreListNode(autoTypeInfo), xsink);

    ConstListIterator li(l);
    while (li.next()) {
        ValueHolder val(deserializeData(li.getValue(), context, xsink), xsink);
        if (*xsink) {
            return QoreValue();
        }
        rv->push(val.release(), xsink);
    }

    return rv.release();
}

void QoreSerializable::serialize(const QoreValue val, OutputStream& stream, int64 flags, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> h(serializeToData(val, flags, xsink), xsink);
    if (*xsink) {
        return;
    }

    serializeToStream(**h, stream, xsink);
}


void QoreSerializable::serialize(const QoreObject& self, OutputStream& stream, int64 flags, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> h(serializeToData(&self, flags, xsink), xsink);
    if (*xsink) {
        return;
    }

    serializeToStream(**h, stream, xsink);
}

void QoreSerializable::serializeToStream(const QoreHashNode& h, OutputStream& stream, ExceptionSink* xsink) {
    assert(hashdeclSerializationInfo->equal(h.getHashDecl()));

    serializeValueToStream(&h, stream, xsink);
    //xsink->raiseException("SERIALIZATION-ERROR", "unimplemented");
}

int QoreSerializable::serializeValueToStream(const QoreValue val, OutputStream& stream, ExceptionSink* xsink) {
    if (!stream.check(xsink)) {
        return -1;
    }

    // write header to output stream
    stream.write(QoreSerializationTypeString.c_str(), QoreSerializationTypeString.size() + 1, xsink);
    if (*xsink) {
        return -1;
    }
    stream.write(QoreSerializationVersionString.c_str(), QoreSerializationVersionString.size() + 1, xsink);
    if (*xsink) {
        return -1;
    }

    // must reference the output stream for the assignment to StreamWriter
    stream.ref();
    ReferenceHolder<StreamWriter> writer(new StreamWriter(xsink, &stream, QCS_UTF8), xsink);

    return serializeValueToStream(val, **writer, xsink);
}

int QoreSerializable::serializeValueToStream(const QoreValue val, StreamWriter& writer, ExceptionSink* xsink) {
    switch (val.getType()) {
        case NT_HASH:
            return serializeHashToStream(*val.get<QoreHashNode>(), writer, xsink);

        case NT_STRING: {
            return serializeStringToStream(*val.get<QoreStringNode>(), writer, xsink);
        }

        case NT_INT: {
            return serializeIntToStream(val.v.i, writer, xsink);
        }

        case NT_BOOLEAN: {
            return serializeBoolToStream(val.v.b, writer, xsink);
        }

        case NT_LIST: {
            return serializeListToStream(*val.get<QoreListNode>(), writer, xsink);
        }

        case NT_FLOAT: {
            return serializeFloatToStream(val.v.f, writer, xsink);
        }

        case NT_NUMBER: {
            return serializeNumberToStream(*val.get<QoreNumberNode>(), writer, xsink);
        }

        case NT_DATE: {
            return serializeDateToStream(*val.get<DateTimeNode>(), writer, xsink);
        }

        case NT_BINARY: {
            return serializeBinaryToStream(*val.get<BinaryNode>(), writer, xsink);
        }

        case NT_NULL: {
            // write data type code to stream
            return writer.writei1(qore_stream_type::SQLNULL, xsink);
        }

        case NT_NOTHING: {
            // write data type code to stream
            return writer.writei1(qore_stream_type::NOTHING, xsink);
        }

        default:
            break;
    }

    xsink->raiseException("SERIALIZATION-ERROR", "cannot serialize type '%s'; type is not supported for serialization",
        val.getTypeName());
    return -1;
}

int QoreSerializable::serializeHashToStream(const QoreHashNode& h, StreamWriter& writer, ExceptionSink* xsink) {
    // write hashdecl type to stream, if any, otherwise -1
    {
        const TypedHashDecl* thd = h.getHashDecl();
        if (thd) {
            // write data type code to stream
            if (writer.writei1(qore_stream_type::HASHDECL, xsink)) {
                return -1;
            }

            std::string path = thd->getNamespacePath();

            // write hashdecl string size to stream
            if (serializeIntToStream(path.size(), writer, xsink)) {
                return -1;
            }

            // write string
            if (writer.write(path.c_str(), path.size(), xsink)) {
                return -1;
            }
        } else {
            // write data type code to stream
            if (writer.writei1(qore_stream_type::HASH, xsink)) {
                return -1;
            }
        }
    }

    // write hash size to stream
    if (serializeIntToStream(h.size(), writer, xsink)) {
        return -1;
    }

    ConstHashIterator hi(h);
    while (hi.next()) {
        // write key string to stream
        if (serializeStringToStream(writer, hi.getKey(), strlen(hi.getKey()), QCS_DEFAULT, xsink)) {
            return -1;
        }

        if (serializeValueToStream(hi.get(), writer, xsink)) {
            return -1;
        }
    }

    return 0;
}

int QoreSerializable::serializeStringToStream(const QoreStringNode& str, StreamWriter& writer, ExceptionSink* xsink) {
    return serializeStringToStream(writer, str.c_str(), str.size(), str.getEncoding(), xsink);
}

int QoreSerializable::serializeStringToStream(StreamWriter& writer, const char* str, size_t len, const QoreEncoding* enc, ExceptionSink* xsink) {
    // write string encoding if not UTF-8
    if (enc && enc != QCS_UTF8) {
        // write data type code to stream
        if (writer.writei1(qore_stream_type::STRING, xsink)) {
            return -1;
        }
        // get encoding string
        const char* enc_str = enc->getCode();
        size_t enc_len = strlen(enc_str);

        // write encoding string size to stream
        if (QoreSerializable::serializeIntToStream(enc_len, writer, xsink)) {
            return -1;
        }

        // write string
        if (writer.write(enc_str, enc_len, xsink)) {
            return -1;
        }
    } else {
        // write data type code to stream
        if (writer.writei1(qore_stream_type::UTF8_STRING, xsink)) {
            return -1;
        }
    }

    // write string size
    if (QoreSerializable::serializeIntToStream(len, writer, xsink)) {
        return -1;
    }

    // write string
    return writer.write(str, len, xsink);
}

int QoreSerializable::serializeIntToStream(int64 i, StreamWriter& writer, ExceptionSink* xsink) {
    if ((i >= -128) & (i <= 127)) {
        // write data type code to stream
        if (!writer.writei1(qore_stream_type::INT1, xsink)) {
            // write integer to stream
            return writer.writei1((signed char)i, xsink);
        }
    } else if ((i >= -32768) && (i <= 32767)) {
        // write data type code to stream
        if (!writer.writei1(qore_stream_type::INT2, xsink)) {
            // write integer to stream
            return writer.writei2((int16_t)i, xsink);
        }
    } else if ((i >= -2147483648) && (i <= 2147483647)) {
        // write data type code to stream
        if (!writer.writei1(qore_stream_type::INT4, xsink)) {
            // write integer to stream
            return writer.writei4((int32_t)i, xsink);
        }
    } else {
        // write data type code to stream
        if (!writer.writei1(qore_stream_type::INT8, xsink)) {
            // write integer to stream
            return writer.writei8((int64_t)i, xsink);
        }
    }

    return -1;
}

int QoreSerializable::serializeBoolToStream(bool b, StreamWriter& writer, ExceptionSink* xsink) {
    // write data type code to stream
    return writer.writei1(b ? qore_stream_type::BOOLEAN_TRUE : qore_stream_type::BOOLEAN_FALSE, xsink);
}

int QoreSerializable::serializeListToStream(const QoreListNode& l, StreamWriter& writer, ExceptionSink* xsink) {
    // write data type code to stream
    if (writer.writei1(qore_stream_type::LIST, xsink)) {
        return -1;
    }

    // write list size to stream
    if (serializeIntToStream(l.size(), writer, xsink)) {
        return -1;
    }

    ConstListIterator i(l);
    while (i.next()) {
        if (serializeValueToStream(i.getValue(), writer, xsink)) {
            return -1;
        }
    }

    return 0;
}

int QoreSerializable::serializeFloatToStream(double f, StreamWriter& writer, ExceptionSink* xsink) {
    // write data type code to stream
    if (!writer.writei1(qore_stream_type::FLOAT, xsink)) {
        // write value to stream
        int64* i = reinterpret_cast<int64*>(&f);
        return writer.writei8(*i, xsink);
    }

    return -1;
}

int QoreSerializable::serializeNumberToStream(const QoreNumberNode& n, StreamWriter& writer, ExceptionSink* xsink) {
    // write data type code to stream
    if (writer.writei1(qore_stream_type::QORENUMBER, xsink)) {
        return -1;
    }

    QoreString tmp;
    n.toString(tmp, QORE_NF_SCIENTIFIC|QORE_NF_RAW);
    if (tmp == "inf") {
        tmp.set("@inf@n");
    } else if (tmp == "-inf") {
        tmp.set("-@inf@n");
    } else if (tmp == "nan") {
        tmp.set("@nan@n");
    } else {
        tmp.concat('n');
    }
    // append precision
    tmp.sprintf("{%u}", n.getPrec());

    // write number string size
    if (QoreSerializable::serializeIntToStream(tmp.size(), writer, xsink)) {
        return -1;
    }

    // write number string
    return writer.write(tmp.c_str(), tmp.size(), xsink);
}

// absolute dates:
//   utc zones:    int epoch | int us | int offset
//   region zones: int epoch | int us | string region
// relative dates:
//   int size "P..."
int QoreSerializable::serializeDateToStream(const DateTimeNode& n, StreamWriter& writer, ExceptionSink* xsink) {
    if (n.isAbsolute()) {
        // write data type code to stream
        if (writer.writei1(qore_stream_type::ABSDATE, xsink)) {
            return -1;
        }

        // write epoch to stream
        int64 epoch = n.getEpochSecondsUTC();
        if (serializeIntToStream(epoch, writer, xsink)) {
            return -1;
        }

        // write microseconds to stream
        int us = n.getMicrosecond();
        if (serializeIntToStream(us, writer, xsink)) {
            return -1;
        }

        // write zone type to stream
        const AbstractQoreZoneInfo* zone = n.getZone();
        //printd(5, "QoreSerializable::serializeDateToStream() epoch: " QLLD " us: %d zone: '%s'\n", epoch, us, AbstractQoreZoneInfo::getRegionName(zone));
        if (dynamic_cast<const QoreOffsetZoneInfo*>(zone)) {
            int utc_offset = AbstractQoreZoneInfo::getUTCOffset(zone);

            // write UTC offset to stream
            if (serializeIntToStream(utc_offset, writer, xsink)) {
                return -1;
            }
        } else {
            const char* region = AbstractQoreZoneInfo::getRegionName(zone);
            assert(region);

            if (serializeStringToStream(writer, region, strlen(region), nullptr, xsink)) {
                return -1;
            }
        }

        return 0;
    }

    // relative dates are written as strings to save serialization space on average
    // write relative date type code
    if (writer.writei1(qore_stream_type::RELDATE, xsink)) {
        return -1;
    }

    QoreString str;

    if (n.hasValue()) {
        qore_tm info;
        n.getInfo(info);

        if (info.year) {
            str.sprintf("%dY", info.year);
        }
        if (info.month) {
            str.sprintf("%dM", info.month);
        }
        if (info.day) {
            str.sprintf("%dD", info.day);
        }

        bool has_t = false;

        if (info.hour) {
            str.sprintf("T%dH", info.hour);
            has_t = true;
        }
        if (info.minute) {
            if (!has_t) {
                str.concat('T');
                has_t = true;
            }
            str.sprintf("%dM", info.minute);
        }
        if (info.second) {
            if (!has_t) {
                str.concat('T');
                has_t = true;
            }
            str.sprintf("%dS", info.second);
        }
        if (info.us) {
            if (!has_t) {
                str.concat('T');
                has_t = true;
            }
            str.sprintf("%du", info.us);
        }
    } else {
        str.concat("0D");
    }

    // write string size
    if (QoreSerializable::serializeIntToStream(str.size(), writer, xsink)) {
        return -1;
    }

    // write string
    return writer.write(str.c_str(), str.size(), xsink);
}

int QoreSerializable::serializeBinaryToStream(const BinaryNode& n, StreamWriter& writer, ExceptionSink* xsink) {
    // write data type code to stream
    if (writer.writei1(qore_stream_type::QOREBINARY, xsink)) {
        return -1;
    }

    // write size to stream
    if (serializeIntToStream(n.size(), writer, xsink)) {
        return -1;
    }

    return writer.write(n.getPtr(), n.size(), xsink);
}

QoreHashNode* QoreSerializable::deserializeToData(ExceptionSink* xsink, InputStream& stream, int64 flags) {
    if (!stream.check(xsink)) {
        return nullptr;
    }

    // read serialization stream type
    QoreString str;
    if (stream_read_string(xsink, stream, str, 50)) {
        return nullptr;
    }

    if (check_deserialization_string(str, QoreSerializationTypeString, "header type", xsink)) {
        return nullptr;
    }

    // read serialization stream version
    if (stream_read_string(xsink, stream, str, 50)) {
        return nullptr;
    }

    if (check_deserialization_string(str, serialization_versions, "header version", xsink)) {
        return nullptr;
    }

    // must reference the input stream for the assignment to StreamReader
    stream.ref();
    ReferenceHolder<StreamReader> reader(new StreamReader(xsink, &stream, QCS_UTF8), xsink);

    ValueHolder val(deserializeValueFromStream(xsink, **reader, flags), xsink);
    if (*xsink) {
        return nullptr;
    }
    QoreHashNode* h = val->getType() == NT_HASH ? val->get<QoreHashNode>() : nullptr;

    //printd(5, "QoreSerializable::deserialize() h: %p val: '%s' hd: %p '%s' (%p %d)\n", h, val->getFullTypeName(),
    //    h->getHashDecl(), h->getHashDecl() ? h->getHashDecl()->getName() : "n/a", hashdeclSerializationInfo,
    //    h->getHashDecl() == hashdeclSerializationInfo);
    if (!h || !hashdeclSerializationInfo->equal(h->getHashDecl())) {
        xsink->raiseException("DESERIALIZATION-ERROR", "expecting a SerializationInfo hash from the serialization "
            "stream; got type '%s' instead", val->getFullTypeName());
        return nullptr;
    }

    val.release();
    return h;
}

QoreValue QoreSerializable::deserialize(ExceptionSink* xsink, InputStream& stream, int64 flags) {
    ReferenceHolder<QoreHashNode> h(deserializeToData(xsink, stream, flags), xsink);
    if (*xsink) {
        return QoreValue();
    }
    return deserialize(xsink, **h, flags);
}

QoreValue QoreSerializable::deserializeValueFromStream(ExceptionSink* xsink, StreamReader& reader, int64 flags) {
    // read data type code
    qore_stream_type code = static_cast<qore_stream_type>(reader.readi1(xsink));
    if (*xsink) {
        return -1;
    }

    switch (code) {
        case qore_stream_type::HASH:
        case qore_stream_type::HASHDECL:
            return deserializeHashFromStream(xsink, reader, code, flags);

        case qore_stream_type::STRING:
        case qore_stream_type::UTF8_STRING:
            return deserializeStringFromStream(reader, code, xsink);

        case qore_stream_type::INT1:
        case qore_stream_type::INT2:
        case qore_stream_type::INT4:
        case qore_stream_type::INT8:
            return deserializeIntFromStream(reader, code, xsink);

        case qore_stream_type::BOOLEAN_TRUE:
            return true;

        case qore_stream_type::BOOLEAN_FALSE:
            return false;

        case qore_stream_type::LIST:
            return deserializeListFromStream(xsink, reader, flags);

        case qore_stream_type::FLOAT:
            return deserializeFloatFromStream(reader, xsink);

        case qore_stream_type::QORENUMBER:
            return deserializeNumberFromStream(reader, xsink);

        case qore_stream_type::ABSDATE:
            return deserializeAbsDateFromStream(reader, xsink);

        case qore_stream_type::RELDATE:
            return deserializeRelDateFromStream(reader, xsink);

        case qore_stream_type::QOREBINARY:
            return deserializeBinaryFromStream(reader, xsink);

        case qore_stream_type::SQLNULL:
            return &Null;

        case qore_stream_type::NOTHING:
            return QoreValue();

        default:
            break;
    }

    xsink->raiseException("DESERIALIZATION-ERROR", "invalid serialization type code %d", (int)code);
    return QoreValue();
}

int64 QoreSerializable::readIntFromStream(ExceptionSink* xsink, StreamReader& reader, const char* type, bool can_be_negative) {
    // read int code
    qore_stream_type code = static_cast<qore_stream_type>(reader.readi1(xsink));
    if (*xsink) {
        return 0;
    }

    if (!code_is_int(code)) {
        xsink->raiseException("DESERIALIZATION-ERROR", "expecting integer code for the %s value; got invalid code %d instead", type, code);
        return 0;
    }

    int64 value = deserializeIntFromStream(reader, code, xsink);
    if (*xsink) {
        return 0;
    }

    if (!can_be_negative && value < 0) {
        xsink->raiseException("DESERIALIZATION-ERROR", "stream data gives an invalid %s value (" QLLD ")", type, value);
        return 0;
    }

    return value;
}

QoreHashNode* QoreSerializable::deserializeHashFromStream(ExceptionSink* xsink, StreamReader& reader,
        qore_stream_type code, int64 flags) {
    const TypedHashDecl* thd;
    int64 size;
    if (code == qore_stream_type::HASHDECL) {
        // read hashdecl path string
        QoreString path_str;
        if (readStringFromStream(reader, path_str, "hashdecl tag", xsink)) {
            return nullptr;
        }

        const QoreNamespace* pns = nullptr;
        thd = getProgram()->findHashDecl(path_str.c_str(), pns);
        if (!thd) {
            xsink->raiseException("DESERIALIZATION-ERROR", "stream data indicates that a '%s' typed hash should be "
                "deserialized, but no such typed hash (hashdecl) could be found in the current Program object",
                path_str.c_str());
            return nullptr;
        }
    } else {
        assert(code == qore_stream_type::HASH);
        thd = nullptr;
    }

    // now read in the number of hash keys
    size = readIntFromStream(xsink, reader, "hash size");
    if (*xsink) {
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);

    // read in hash keys
    for (int64 i = 0; i < size; ++i) {
        // read key type (must be string)
        code = static_cast<qore_stream_type>(reader.readi1(xsink));
        if (*xsink) {
            return nullptr;
        }
        if (!code_is_string(code)) {
            xsink->raiseException("DESERIALIZATION-ERROR", "expecting string type for hash key; got type %d instead",
                static_cast<int>(code));
            return nullptr;
        }

        SimpleRefHolder<QoreStringNode> key(deserializeStringFromStream(reader, code, xsink));
        if (*xsink) {
            return nullptr;
        }

        // convert key string to QCS_DEFAULT if necessary
        TempEncodingHelper tstr(**key, QCS_DEFAULT, xsink);
        if (*xsink) {
            return nullptr;
        }

        ValueHolder val(deserializeValueFromStream(xsink, reader, flags), xsink);
        if (*xsink) {
            if (thd) {
                xsink->appendLastDescription(" (while deserializing hashdecl '%s')", thd->getName());
            }
            return nullptr;
        }

        h->setKeyValue(tstr->c_str(), val.release(), xsink);
    }

    return thd ? typed_hash_decl_private::get(*thd)->newHash(*h, true, xsink) : h.release();
}

int QoreSerializable::readStringFromStream(StreamReader& reader, QoreString& str, const char* type, ExceptionSink* xsink) {
    // read string size
    int64 size = readIntFromStream(xsink, reader, "string size");
    if (*xsink) {
        return -1;
    }
    if (size) {
        // read in string data
        str.reserve(size);
        if (reader.read(xsink, const_cast<char*>(str.c_str()), size) == -1) {
            return -1;
        }
    }
    str.terminate(size);
    return 0;
}

QoreStringNode* QoreSerializable::deserializeStringFromStream(StreamReader& reader, qore_stream_type code, ExceptionSink* xsink) {
    SimpleRefHolder<QoreStringNode> str(new QoreStringNode);

    const QoreEncoding* enc;
    if (code == qore_stream_type::STRING) {
        // read encoding string
        QoreString enc_str;
        if (readStringFromStream(reader, enc_str, "encoding", xsink)) {
            return nullptr;
        }
        enc = QEM.findCreate(enc_str.c_str());
    } else {
        assert(code == qore_stream_type::UTF8_STRING);
        enc = QCS_UTF8;
    }

    if (readStringFromStream(reader, **str, "data", xsink)) {
        return nullptr;
    }

    str->setEncoding(enc);
    return str.release();
}

int64 QoreSerializable::deserializeIntFromStream(StreamReader& reader, qore_stream_type code, ExceptionSink* xsink) {
    int64 i;
    switch (code) {
        case qore_stream_type::INT1: i = reader.readi1(xsink); break;
        case qore_stream_type::INT2: i = reader.readi2(xsink); break;
        case qore_stream_type::INT4: i = reader.readi4(xsink); break;
        case qore_stream_type::INT8: i = reader.readi8(xsink); break;
        default:
            assert(false);
            break;
    }

    return *xsink ? 0 : i;
}

QoreListNode* QoreSerializable::deserializeListFromStream(ExceptionSink* xsink, StreamReader& reader, int64 flags) {
    // read list size from stream
    int64 size = readIntFromStream(xsink, reader, "list size");
    if (*xsink) {
        return nullptr;
    }
    //printd(5, "QoreSerializable::deserializeListFromStream() list size: " QLLD "\n", size);

    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);

    for (int64 i = 0; i < size; ++i) {
        ValueHolder val(deserializeValueFromStream(xsink, reader, flags), xsink);
        if (*xsink) {
            return nullptr;
        }
        rv->push(val.release(), xsink);
    }

    return rv.release();
}

double QoreSerializable::deserializeFloatFromStream(StreamReader& reader, ExceptionSink* xsink) {
    int64 i = reader.readi8(xsink);
    double* f = reinterpret_cast<double*>(&i);
    return *f;
}

static unsigned is_prec(const char* str, size_t len) {
    //printd(5, "is_prec str: '%s' len: %d strlen: %d\n", str, len, strlen(str));
    if (*str != '{') {
        return 0;
    }

    const char* p = str + 1;
    while (isdigit(*p)) {
        ++p;
    }
    if (p == (str + 1) || *p != '}' || static_cast<size_t>(p - str + 1) != len) {
        return 0;
    }

    return static_cast<unsigned>(atoi(str + 1));
}

QoreNumberNode* QoreSerializable::deserializeNumberFromStream(StreamReader& reader, ExceptionSink* xsink) {
    QoreString tmp;
    if (readStringFromStream(reader, tmp, "arbitrary-precision number", xsink)) {
        return nullptr;
    }

    //printd(5, "QoreSerializable::deserializeNumberFromStream() tmp: '%s'\n", tmp.c_str());

    const char* val = tmp.c_str();
    bool sign = (*val == '-' || *val == '+');

    // check for @inf@ and @nan@
    if (!strncasecmp(val + sign, "@nan@", 5) || !strncasecmp(val + sign, "@inf@", 5)) {
        if (val[5 + sign] == 'n') {
            if (tmp.size() == static_cast<size_t>(6 + sign)) {
                return new QoreNumberNode(val);
            } else {
                unsigned prec = is_prec(val + sign + 6, tmp.size() - sign - 6);
                if (prec) {
                    return new QoreNumberNode(val, prec);
                }
            }
        }
        xsink->raiseException("DESERIALIZATION-ERROR", "invalid number string '%s' read from stream", tmp.c_str());
        return nullptr;
    }

    const char* p = strchr(val, '{');
    if (p) {
        unsigned prec = is_prec(p, tmp.size() - (p - val));
        //printd(5, "%s prec %d\n", val, prec);
        if (prec) {
            return new QoreNumberNode(val, prec);
        } else {
            xsink->raiseException("DESERIALIZATION-ERROR", "invalid number string '%s' read from stream", tmp.c_str());
            return nullptr;
        }
    }

    return new QoreNumberNode(val);
}

// absolute dates:
//   utc zones:    int epoch | int us | int offset
//   region zones: int epoch | int us | string region
DateTimeNode* QoreSerializable::deserializeAbsDateFromStream(StreamReader& reader, ExceptionSink* xsink) {
    // read in epoch
    int64 epoch = readIntFromStream(xsink, reader, "absolute date epoch", true);
    if (*xsink) {
        return nullptr;
    }

    int64 us = readIntFromStream(xsink, reader, "absolute date microseconds", true);
    if (*xsink) {
        return nullptr;
    }

    // read key type (must be string)
    qore_stream_type zt = static_cast<qore_stream_type>(reader.peekCheck(xsink));
    if (*xsink) {
        return nullptr;
    }

    if (!code_is_int(zt) && zt != qore_stream_type::UTF8_STRING) {
        xsink->raiseException("DESERIALIZATION-ERROR", "invalid absolute date zone type %d read from stream; "
            "expecting an integer or UTF-8 string type", (int)zt);
        return nullptr;
    }

    const AbstractQoreZoneInfo* zone;
    if (code_is_int(zt)) {
        // read UTC offset
        int64 seconds_east = readIntFromStream(xsink, reader, "absolute date UTC offset", true);
        if (*xsink) {
            return nullptr;
        }

        zone = QTZM.findCreateOffsetZone(seconds_east);
    } else {
        // read string data type code
#ifdef DEBUG
        qore_stream_type code = static_cast<qore_stream_type>(reader.readi1(xsink));
#else
        reader.readi1(xsink);
#endif
        if (*xsink) {
            return nullptr;
        }
#ifdef DEBUG
        assert(code == qore_stream_type::UTF8_STRING);
#endif

        QoreString region;
        if (readStringFromStream(reader, region, "absolute date region", xsink)) {
            return nullptr;
        }

        bool is_path = (!region.empty() && region.c_str()[0] == '.') || q_absolute_path(region.c_str());
        if (is_path && runtime_check_parse_option(PO_NO_FILESYSTEM)) {
            xsink->raiseException("ILLEGAL-FILESYSTEM-ACCESS", "cannot create a TimeZone object from absolute path "
                "'%s' when sandboxing restriction PO_NO_FILESYSTEM is set", region.c_str());
        }

        zone = is_path
            ? QTZM.findLoadRegionFromPath(region.c_str(), xsink)
            : QTZM.findLoadRegion(region.c_str(), xsink);

        if (*xsink) {
            return nullptr;
        }
    }

    //printd(5, "QoreSerializable::deserializeDateFromStream() epoch: " QLLD " us: %d zone: '%s'\n", epoch, us,
    //    AbstractQoreZoneInfo::getRegionName(zone));

    return DateTimeNode::makeAbsolute(zone, epoch, us);
}

// relative dates:
//   int size "P..."
DateTimeNode* QoreSerializable::deserializeRelDateFromStream(StreamReader& reader, ExceptionSink* xsink) {
    QoreString str;
    if (readStringFromStream(reader, str, "relative date", xsink)) {
        return nullptr;
    }
    str.prepend("P");

    return new DateTimeNode(str.c_str());
}

BinaryNode* QoreSerializable::deserializeBinaryFromStream(StreamReader& reader, ExceptionSink* xsink) {
    // read size
    int64 size = readIntFromStream(xsink, reader, "binary object size");
    if (*xsink) {
        return nullptr;
    }

    SimpleRefHolder<BinaryNode> rv(new BinaryNode);
    if (size) {
        rv->preallocate(size);

        if (reader.read(xsink, const_cast<void*>(rv->getPtr()), rv->size()) == -1) {
            assert(*xsink);
            return nullptr;
        }
    }
    return rv.release();
}
