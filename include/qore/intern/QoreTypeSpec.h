/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreTypeSpec.h

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

#ifndef _QORE_QORETYPESPEC_H
#define _QORE_QORETYPESPEC_H

#include <functional>
#include <vector>
#include <qore/QoreType.h>
#include <qore/QoreClass.h>
#include <qore/TypedHashDecl.h>
#include <qore/QoreEnumDecl.h>

// Forward declarations
class QoreTypeInfo;
class LValueHelper;
class QoreHashNode;
class QoreListNode;

#define NO_TYPE_INFO "any"

// adds external types to global type map
DLLLOCAL void add_to_type_map(qore_type_t t, const QoreTypeInfo* typeInfo);

// internal use only
DLLLOCAL const QoreTypeInfo* qore_get_complex_hard_reference_type(const QoreTypeInfo* valueTypeInfo);

enum q_typespec_t : unsigned char {
    QTS_TYPE = 0,
    QTS_CLASS = 1,
    QTS_HASHDECL = 2,
    QTS_HARDREF = 3,
    QTS_COMPLEXHASH = 4,
    QTS_COMPLEXLIST = 5,
    QTS_COMPLEXHARDREF = 6,
    QTS_COMPLEXREF = 7,
    QTS_COMPLEXSOFTLIST = 8,
    QTS_EMPTYLIST = 9,
    QTS_EMPTYHASH = 10,
    QTS_ENUM = 11,
};

typedef std::function<void (QoreValue&, ExceptionSink*)> q_type_map_t;

// returns type info for base types
DLLLOCAL const QoreTypeInfo* getTypeInfoForType(qore_type_t t);
// returns type info information for parse types (values)
DLLLOCAL const QoreTypeInfo* getTypeInfoForValue(const AbstractQoreNode* n);
// returns an "or nothing" type for the given non-or-nothing type or nullptr if not possible
DLLLOCAL const QoreTypeInfo* get_or_nothing_type(const QoreTypeInfo* typeInfo);
// returns a value (i.e. not "or nothing") type for the given type
DLLLOCAL const QoreTypeInfo* get_value_type(const QoreTypeInfo* typeInfo);

class QoreTypeSpec {
public:
    DLLLOCAL QoreTypeSpec(qore_type_t t) : typespec(QTS_TYPE) {
        u.t = t;
    }

    DLLLOCAL QoreTypeSpec(const QoreClass* qc) : typespec(QTS_CLASS) {
        u.qc = qc;
    }

    DLLLOCAL QoreTypeSpec(const TypedHashDecl* hd) : typespec(QTS_HASHDECL) {
        u.hd = hd;
    }

    DLLLOCAL QoreTypeSpec(const QoreEnumDecl* ed) : typespec(QTS_ENUM) {
        u.ed = ed;
    }

    DLLLOCAL q_typespec_t getTypeSpec() const {
        return typespec;
    }

    DLLLOCAL const char* getName() const;

    DLLLOCAL const char* getTypeName() const;
    DLLLOCAL const char* getSimpleTypeName() const;

    // returns the base qore_type_t for the type specification
    DLLLOCAL qore_type_t getType() const;

    DLLLOCAL const QoreClass* getClass() const {
        return typespec == QTS_CLASS ? u.qc : nullptr;
    }

    DLLLOCAL const TypedHashDecl* getHashDecl() const {
        return typespec == QTS_HASHDECL ? u.hd : nullptr;
    }

    DLLLOCAL const QoreEnumDecl* getEnum() const {
        return typespec == QTS_ENUM ? u.ed : nullptr;
    }

    DLLLOCAL const QoreTypeInfo* getComplexHash() const {
        return typespec == QTS_COMPLEXHASH ? u.ti : nullptr;
    }

    DLLLOCAL const QoreTypeInfo* getComplexList() const {
        return typespec == QTS_COMPLEXLIST || typespec == QTS_COMPLEXSOFTLIST ? u.ti : nullptr;
    }

    DLLLOCAL const QoreTypeInfo* getComplexSoftList() const {
        return typespec == QTS_COMPLEXSOFTLIST ? u.ti : nullptr;
    }

    DLLLOCAL const QoreTypeInfo* getComplexReference() const {
        return (typespec == QTS_COMPLEXREF || typespec == QTS_COMPLEXHARDREF) ? u.ti : nullptr;
    }

    //! returns the element type, if any (nullptr if not applicable)
    DLLLOCAL const QoreTypeInfo* getElementType() const {
        switch (typespec) {
            case QTS_COMPLEXHASH:
            case QTS_COMPLEXLIST:
            case QTS_COMPLEXSOFTLIST:
                return u.ti;
            default:
                break;
        }
        return nullptr;
    }

    DLLLOCAL bool isComplex() const {
        return typespec == QTS_HASHDECL
                || typespec == QTS_COMPLEXHASH
                || typespec == QTS_COMPLEXLIST
                || typespec == QTS_COMPLEXSOFTLIST
                || typespec == QTS_COMPLEXHARDREF
                || typespec == QTS_COMPLEXREF
                || typespec == QTS_ENUM
            ;
    }

    DLLLOCAL const QoreTypeInfo* getTypeInfo() const {
        switch (typespec) {
            case QTS_HASHDECL:
                return u.hd->getTypeInfo();

            case QTS_ENUM:
                return u.ed->getTypeInfo();

            case QTS_COMPLEXLIST:
            case QTS_COMPLEXSOFTLIST:
                return qore_get_complex_list_type(u.ti);

            case QTS_COMPLEXHASH:
                return qore_get_complex_hash_type(u.ti);

            case QTS_COMPLEXHARDREF:
                return qore_get_complex_hard_reference_type(u.ti);

            case QTS_COMPLEXREF:
                return qore_get_complex_reference_type(u.ti);

            case QTS_CLASS:
                return u.qc->getTypeInfo();

            case QTS_TYPE:
                return getTypeInfoForType(u.t);

            case QTS_HARDREF:
                return referenceTypeInfo;

            case QTS_EMPTYLIST:
                return emptyListTypeInfo;

            case QTS_EMPTYHASH:
                return emptyHashTypeInfo;
        }
        assert(false);
        return nullptr;
    }

    DLLLOCAL const QoreTypeInfo* getBaseTypeInfo() const {
        switch (typespec) {
            case QTS_HASHDECL:
            case QTS_COMPLEXHASH:
                return hashTypeInfo;
            case QTS_COMPLEXLIST:
            case QTS_COMPLEXSOFTLIST:
                return listTypeInfo;
            case QTS_CLASS:
                return objectTypeInfo;
            case QTS_HARDREF:
            case QTS_COMPLEXHARDREF:
            case QTS_COMPLEXREF:
                return referenceTypeInfo;
            case QTS_TYPE:
                return getTypeInfoForType(u.t);
            case QTS_EMPTYLIST:
                return emptyListTypeInfo;
            case QTS_EMPTYHASH:
                return emptyHashTypeInfo;
            case QTS_ENUM:
                return u.ed->getBaseTypeInfo();
        }
        assert(false);
        return nullptr;
    }

    DLLLOCAL qore_type_result_e matchType(qore_type_t t) const;

    // this is the "expecting" type, t is the type to match
    // ex: this = class, t = NT_OBJECT, result = AMBIGUOUS
    // ex: this = NT_OBJECT, t = class, result = IDENT
    DLLLOCAL qore_type_result_e match(const QoreTypeSpec& t) const {
        bool may_not_match = false;
        bool may_need_filter = false;
        qore_type_result_e max_result = QTI_NOT_EQUAL;
        return match(t, may_not_match, may_need_filter, max_result, false);
    }

    // this is the "expecting" type, t is the type to match
    // ex: this = class, t = NT_OBJECT, result = AMBIGUOUS
    // ex: this = NT_OBJECT, t = class, result = IDENT
    DLLLOCAL qore_type_result_e match(const QoreTypeSpec& t, bool& may_not_match) const {
        bool may_need_filter = false;
        qore_type_result_e max_result = QTI_NOT_EQUAL;
        return match(t, may_not_match, may_need_filter, max_result, false);
    }

    // this is the "expecting" type, t is the type to match
    // ex: this = class, t = NT_OBJECT, result = AMBIGUOUS
    // ex: this = NT_OBJECT, t = class, result = IDENT
    DLLLOCAL qore_type_result_e match(const QoreTypeSpec& t, bool& may_not_match, bool& may_need_filter) const {
        qore_type_result_e max_result = QTI_NOT_EQUAL;
        return match(t, may_not_match, may_need_filter, max_result, false);
    }

    // this is the "expecting" type, t is the type to match
    // ex: this = class, t = NT_OBJECT, result = AMBIGUOUS
    // ex: this = NT_OBJECT, t = class, result = IDENT
    DLLLOCAL qore_type_result_e match(const QoreTypeSpec& t, bool& may_not_match, bool& may_need_filter,
            qore_type_result_e& max_result, bool known_initial_assignment) const;

    DLLLOCAL qore_type_result_e runtimeAcceptsValue(const QoreValue& n, bool exact) const;

    DLLLOCAL qore_type_result_e checkMatchType(const QoreTypeSpec& t, bool& may_not_match,
            qore_type_result_e& max_result) const;

    DLLLOCAL qore_type_result_e tryMatchReferenceType(const QoreTypeSpec& t, bool& may_not_match) const;

    // returns true if there is a match or if an error has been raised
    DLLLOCAL bool acceptInput(ExceptionSink* xsink, const QoreTypeInfo& typeInfo, q_type_map_t map,
            const char* arg_type, bool obj, int param_num, const char* param_name, QoreValue& n,
            LValueHelper* lvhelper = nullptr) const;

    DLLLOCAL bool operator==(const QoreTypeSpec& other) const;
    DLLLOCAL bool operator!=(const QoreTypeSpec& other) const;

    DLLLOCAL bool acceptInputComplexHash(ExceptionSink* xsink, const QoreTypeInfo& typeInfo, const char* arg_type,
            bool obj, int param_num, const char* param_name, QoreValue& n, LValueHelper* lvhelper, QoreHashNode* h,
            bool& err) const;

    DLLLOCAL bool acceptInputComplexList(ExceptionSink* xsink, const QoreTypeInfo& typeInfo, const char* arg_type,
            bool obj, int param_num, const char* param_name, QoreValue& n, LValueHelper* lvhelper, QoreListNode* l,
            bool& err) const;

protected:
    q_typespec_t typespec;
    union {
        qore_type_t t;
        const QoreClass* qc;
        const TypedHashDecl* hd;
        const QoreTypeInfo* ti;
        const QoreEnumDecl* ed;
    } u;

    DLLLOCAL QoreTypeSpec(const QoreTypeInfo* ti, q_typespec_t t) : typespec(t) {
        u.ti = ti;
    }

    DLLLOCAL QoreTypeSpec(qore_type_t t, q_typespec_t ts) : typespec(ts) {
        u.t = t;
    }
};

class QoreHardReferenceTypeSpec : public QoreTypeSpec {
public:
    DLLLOCAL QoreHardReferenceTypeSpec() : QoreTypeSpec(NT_REFERENCE, QTS_HARDREF) {
    }
};

class QoreComplexHashTypeSpec : public QoreTypeSpec {
public:
    DLLLOCAL QoreComplexHashTypeSpec(const QoreTypeInfo* ti) : QoreTypeSpec(ti, QTS_COMPLEXHASH) {
    }
};

class QoreComplexListTypeSpec : public QoreTypeSpec {
public:
    DLLLOCAL QoreComplexListTypeSpec(const QoreTypeInfo* ti) : QoreTypeSpec(ti, QTS_COMPLEXLIST) {
    }
};

class QoreComplexSoftListTypeSpec : public QoreTypeSpec {
public:
    DLLLOCAL QoreComplexSoftListTypeSpec(const QoreTypeInfo* ti) : QoreTypeSpec(ti, QTS_COMPLEXSOFTLIST) {
    }
};

class QoreComplexHardReferenceTypeSpec : public QoreTypeSpec {
public:
    DLLLOCAL QoreComplexHardReferenceTypeSpec(const QoreTypeInfo* ti) : QoreTypeSpec(ti, QTS_COMPLEXHARDREF) {
    }
};

class QoreComplexReferenceTypeSpec : public QoreTypeSpec {
public:
    DLLLOCAL QoreComplexReferenceTypeSpec(const QoreTypeInfo* ti) : QoreTypeSpec(ti, QTS_COMPLEXREF) {
    }
};

class QoreEmptyListTypeSpec : public QoreTypeSpec {
public:
    DLLLOCAL QoreEmptyListTypeSpec() : QoreTypeSpec(NT_LIST, QTS_EMPTYLIST) {
    }
};

class QoreEmptyHashTypeSpec : public QoreTypeSpec {
public:
    DLLLOCAL QoreEmptyHashTypeSpec() : QoreTypeSpec(NT_HASH, QTS_EMPTYHASH) {
    }
};

struct QoreReturnSpec {
    const QoreTypeSpec spec;
    bool exact = false;

    DLLLOCAL QoreReturnSpec(const QoreTypeSpec&& spec, bool exact = false) : spec(spec), exact(exact) {
    }
};

typedef std::vector<QoreReturnSpec> q_return_vec_t;

struct QoreAcceptSpec {
    const QoreTypeSpec spec;
    const q_type_map_t map;
    bool exact = false;

    DLLLOCAL QoreAcceptSpec(const QoreTypeSpec&& spec, const q_type_map_t&& map, bool exact = false) : spec(spec), map(map), exact(exact) {
    }
};
typedef std::vector<QoreAcceptSpec> q_accept_vec_t;

template <typename T>
DLLLOCAL bool typespec_vec_compare(const T& a, const T& b);

DLLLOCAL bool accept_vec_compare(const q_accept_vec_t& a, const q_accept_vec_t& b);
DLLLOCAL bool return_vec_compare(const q_return_vec_t& a, const q_return_vec_t& b);

#endif
