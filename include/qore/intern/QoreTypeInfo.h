/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreTypeInfo.h

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

#ifndef _QORE_QORETYPEINFO_H

#define _QORE_QORETYPEINFO_H

#include <map>
#include "qore/intern/qore_string_private.h"
#include <vector>
#include <utility>
#include <functional>

#define NO_TYPE_INFO "any"

// forward references
class LValueHelper;
class QoreEnumDecl;

DLLLOCAL QoreParseOptions parse_get_parse_options();

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

class QoreTypeInfo;
typedef std::function<void (QoreValue&, ExceptionSink*)> q_type_map_t;

static q_type_map_t null_to_nothing = [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); };

static inline void validate_date_variant(QoreValue& n, ExceptionSink* xsink, bool require_relative,
        const char* type_name) {
    if (!xsink) {
        return;
    }

    if (n.getType() != NT_DATE) {
        return;
    }

    bool is_relative = n.get<const DateTimeNode>()->isRelative();
    if (is_relative != require_relative) {
        xsink->raiseException("RUNTIME-TYPE-ERROR", "expected %s date/time value for type '%s'",
            require_relative ? "relative" : "absolute", type_name);
    }
}

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

    DLLLOCAL qore_type_result_e checkMatchType(const QoreTypeSpec& t, bool& may_not_match,
            qore_type_result_e& max_result) const;

    DLLLOCAL qore_type_result_e tryMatchReferenceType(const QoreTypeSpec& t, bool& may_not_match) const;

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
    // ex: this = class, t = NT_OBJECT, result = AMBIGU`OUS
    // ex: this = NT_OBJECT, t = class, result = IDENT
    DLLLOCAL qore_type_result_e match(const QoreTypeSpec& t) const {
        bool may_not_match = false;
        bool may_need_filter = false;
        qore_type_result_e max_result = QTI_NOT_EQUAL;
        return match(t, may_not_match, may_need_filter, max_result, false);
    }

    // this is the "expecting" type, t is the type to match
    // ex: this = class, t = NT_OBJECT, result = AMBIGU`OUS
    // ex: this = NT_OBJECT, t = class, result = IDENT
    DLLLOCAL qore_type_result_e match(const QoreTypeSpec& t, bool& may_not_match) const {
        bool may_need_filter = false;
        qore_type_result_e max_result = QTI_NOT_EQUAL;
        return match(t, may_not_match, may_need_filter, max_result, false);
    }

    // this is the "expecting" type, t is the type to match
    // ex: this = class, t = NT_OBJECT, result = AMBIGU`OUS
    // ex: this = NT_OBJECT, t = class, result = IDENT
    DLLLOCAL qore_type_result_e match(const QoreTypeSpec& t, bool& may_not_match, bool& may_need_filter) const {
        qore_type_result_e max_result = QTI_NOT_EQUAL;
        return match(t, may_not_match, may_need_filter, max_result, false);
    }

    // this is the "expecting" type, t is the type to match
    // ex: this = class, t = NT_OBJECT, result = AMBIGU`OUS
    // ex: this = NT_OBJECT, t = class, result = IDENT
    DLLLOCAL qore_type_result_e match(const QoreTypeSpec& t, bool& may_not_match, bool& may_need_filter,
            qore_type_result_e& max_result, bool known_initial_assignment) const;

    DLLLOCAL qore_type_result_e runtimeAcceptsValue(const QoreValue& n, bool exact) const;

    // returns true if there is a match or if an error has been raised
    DLLLOCAL bool acceptInput(ExceptionSink* xsink, const QoreTypeInfo& typeInfo, q_type_map_t map,
            const char* arg_type, bool obj, int param_num, const char* param_name, QoreValue& n,
            LValueHelper* lvhelper = nullptr) const;

    DLLLOCAL bool operator==(const QoreTypeSpec& other) const;
    DLLLOCAL bool operator!=(const QoreTypeSpec& other) const;

protected:
    DLLLOCAL QoreTypeSpec(const QoreTypeInfo* ti, q_typespec_t t) : typespec(t) {
        u.ti = ti;
    }

    DLLLOCAL QoreTypeSpec(qore_type_t t, q_typespec_t ts) : typespec(ts) {
        u.t = t;
    }

    DLLLOCAL bool acceptInputComplexHash(ExceptionSink* xsink, const QoreTypeInfo& typeInfo, const char* arg_type,
            bool obj, int param_num, const char* param_name, QoreValue& n, LValueHelper* lvhelper, QoreHashNode* h,
            bool& err) const;

    DLLLOCAL bool acceptInputComplexList(ExceptionSink* xsink, const QoreTypeInfo& typeInfo, const char* arg_type,
            bool obj, int param_num, const char* param_name, QoreValue& n, LValueHelper* lvhelper, QoreListNode* l,
            bool& err) const;

private:
    union {
        qore_type_t t;
        const QoreClass* qc;
        const TypedHashDecl* hd;
        const QoreEnumDecl* ed;
        const QoreTypeInfo* ti;
    } u;
    q_typespec_t typespec;
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

class QoreTypeInfo {
public:
    const q_return_vec_t return_vec;

    DLLLOCAL QoreTypeInfo(const char* name, const q_accept_vec_t&& a_vec, const q_return_vec_t&& r_vec)
            : accept_vec(a_vec), return_vec(r_vec), tname(name) {
        setSimpleName();
    }

    DLLLOCAL virtual ~QoreTypeInfo() = default;

    // Accessors for accept_vec encapsulation
    DLLLOCAL bool isAcceptVecEmpty() const {
        return accept_vec.empty();
    }

    DLLLOCAL size_t getAcceptVecSize() const {
        return accept_vec.size();
    }

    DLLLOCAL bool hasOneAcceptSpec() const {
        return accept_vec.size() == 1;
    }

    DLLLOCAL const QoreAcceptSpec& getFirstAcceptSpec() const {
        assert(!accept_vec.empty());
        return accept_vec[0];
    }

    DLLLOCAL const QoreAcceptSpec& getAcceptSpec(size_t index) const {
        assert(index < accept_vec.size());
        return accept_vec[index];
    }

    DLLLOCAL const q_accept_vec_t& getAcceptSpecs() const {
        return accept_vec;
    }

    DLLLOCAL static const QoreTypeInfo* getHardReference(const QoreTypeInfo* ti);

#if 0
    DLLLOCAL static const QoreTypeInfo* getRuntimeType(const QoreTypeInfo* ti);
#endif

    // static version of method, checking for null pointer
    DLLLOCAL static QoreHashNode* getAcceptTypes(const QoreTypeInfo* ti, bool simple = false) {
        ReferenceHolder<QoreHashNode> rv(new QoreHashNode(boolTypeInfo), nullptr);
        if (ti && hasType(ti)) {
            ti->getAcceptTypes(rv, simple);
        } else {
            rv->setKeyValue("any", true, nullptr);
        }
        return rv.release();
    }

    // static version of method, checking for null pointer
    DLLLOCAL static QoreHashNode* getReturnTypes(const QoreTypeInfo* ti, bool simple = false) {
        ReferenceHolder<QoreHashNode> rv(new QoreHashNode(boolTypeInfo), nullptr);
        if (ti && hasType(ti)) {
            ti->getReturnTypes(rv, simple);
        } else {
            rv->setKeyValue("any", true, nullptr);
        }
        return rv.release();
    }

    // static version of method, checking for null pointer
    DLLLOCAL static qore_type_t getSingleType(const QoreTypeInfo* ti) {
        return ti && hasType(ti) ? ti->getSingleType() : NT_ALL;
    }

    // static version of method, checking for null pointer
    DLLLOCAL static qore_type_t getBaseType(const QoreTypeInfo* ti) {
        return ti && hasType(ti) ? ti->getBaseType() : NT_ALL;
    }

    // static version of method, checking for null pointer
    DLLLOCAL static bool parseAcceptsReturns(const QoreTypeInfo* ti, qore_type_t t) {
        return ti && hasType(ti) ? ti->parseAcceptsReturns(t) : true;
    }

    // static version of method, checking for null pointer
    DLLLOCAL static qore_type_result_e parseReturns(const QoreTypeInfo* ti, QoreTypeSpec t) {
        return ti && hasType(ti) ? ti->parseReturns(t) : QTI_WILDCARD;
    }

    // static version of method, checking for null pointer
    DLLLOCAL static bool isReference(const QoreTypeInfo* ti) {
        return ti && ti->return_vec[0].spec.getType() == NT_REFERENCE;
    }

    // static version of method, checking for null pointer
    DLLLOCAL static const QoreTypeInfo* getReferenceTarget(const QoreTypeInfo* ti) {
        if (isReference(ti)) {
            const QoreTypeInfo* rv = ti->return_vec[0].spec.getComplexReference();
            return rv ? rv : anyTypeInfo;
        }
        return nullptr;
    }

    // static version of method, checking for null pointer
    // returns true if this type only returns the type given
    DLLLOCAL static bool isType(const QoreTypeInfo* ti, qore_type_t t) {
        return ti ? ti->isType(t) : false;
    }

    // static version of method, checking for null pointer
    DLLLOCAL static qore_type_result_e runtimeAcceptsValue(const QoreTypeInfo* ti, const QoreValue n) {
        return ti && hasType(ti) ? ti->runtimeAcceptsValue(n) : QTI_WILDCARD;
    }

    // static version of method, checking for null pointer
    DLLLOCAL static qore_type_result_e parseAccepts(const QoreTypeInfo* first, const QoreTypeInfo* second) {
        if (first == second) {
            return QTI_IDENT;
        }
        bool may_not_match = false;
        bool may_need_filter = false;
        qore_type_result_e max_result = QTI_NOT_EQUAL;
        return parseAccepts(first, second, may_not_match, may_need_filter, max_result);
    }

    // static version of method, checking for null pointer
    DLLLOCAL static qore_type_result_e parseAccepts(const QoreTypeInfo* first, const QoreTypeInfo* second,
            bool& may_not_match) {
        if (first == second) {
            return QTI_IDENT;
        }
        bool may_need_filter = false;
        qore_type_result_e max_result = QTI_NOT_EQUAL;
        return parseAccepts(first, second, may_not_match, may_need_filter, max_result);
    }

    DLLLOCAL static qore_type_result_e parseAccepts(const QoreTypeInfo* first, const QoreTypeInfo* second,
            bool& may_not_match, bool& may_need_filter) {
        if (first == second) {
            return QTI_IDENT;
        }
        qore_type_result_e max_result = QTI_NOT_EQUAL;
        return parseAccepts(first, second, may_not_match, may_need_filter, max_result);
    }

    // static version of method, checking for null pointer
    DLLLOCAL static qore_type_result_e parseAccepts(const QoreTypeInfo* first, const QoreTypeInfo* second,
            bool& may_not_match, bool& may_need_filter, qore_type_result_e& max_result,
            bool known_initial_assignment = false) {
        /*
        if (first == second) {
            // issue
            max_result = QTI_IDENT;
            return QTI_IDENT;
        }
        */
        if (first == autoTypeInfo) {
            max_result = QTI_WILDCARD;
            return QTI_WILDCARD;
        }
        if (!hasType(first)) {
            if (!may_need_filter && isComplex(second))
                may_need_filter = true;
            max_result = QTI_WILDCARD;
            return QTI_WILDCARD;
        }
        if (!hasType(second)) {
            if (!may_need_filter) {
                // check if we could need a runtime filter
                for (auto& i : first->getAcceptSpecs()) {
                    if (i.map) {
                        may_need_filter = true;
                        break;
                    }
                }
            }
            max_result = QTI_IDENT;
            may_not_match = true;
            return QTI_AMBIGUOUS;
        }
        return first->parseAccepts(second, may_not_match, may_need_filter, max_result, known_initial_assignment);
    }

    // static version of method, checking for null pointer
    DLLLOCAL static qore_type_result_e runtimeTypeMatch(const QoreTypeInfo* first, const QoreTypeInfo* second) {
        //printd(5, "QoreTypeInfo::runtimeTypeMatch() %p '%s' (ht: %d) <=> %p '%s' (ht: %d)\n", first,
        //    QoreTypeInfo::getName(first), QoreTypeInfo::hasType(first), second, QoreTypeInfo::getName(second),
        //    QoreTypeInfo::hasType(second));
        if (first == second) {
            return QTI_IDENT;
        }
        if (!hasType(second)) {
            return !hasType(first) ? QTI_NEAR : QTI_AMBIGUOUS;
        }
        return QoreTypeInfo::hasType(first) ? first->runtimeTypeMatch(second) : QTI_AMBIGUOUS;
    }

    // static version of method, checking for null pointer
    DLLLOCAL static qore_type_t getSingleReturnType(const QoreTypeInfo* ti) {
        if (!hasType(ti) || ti->return_vec.size() > 1)
            return NT_ALL;
        return ti->return_vec[0].spec.getType();
    }

    // static version of method, checking for null pointer
    DLLLOCAL static bool returnsSingle(const QoreTypeInfo* ti) {
        return ti && hasType(ti) ? ti->return_vec.size() == 1 : false;
    }

    // static version of method, checking for null pointer
    DLLLOCAL static bool acceptsSingle(const QoreTypeInfo* ti) {
        return ti && hasType(ti) ? ti->getAcceptVecSize() == 1 : false;
    }

    // static version of method, checking for null pointer
    DLLLOCAL static const QoreClass* getUniqueReturnClass(const QoreTypeInfo* ti) {
        if (!ti || ti->return_vec.size() > 1 || !hasType(ti))
            return nullptr;
        return ti->return_vec[0].spec.getClass();
    }

    // static version of method, checking for null pointer
    DLLLOCAL static const TypedHashDecl* getUniqueReturnHashDecl(const QoreTypeInfo* ti) {
        if (!ti || ti->return_vec.size() > 1 || !hasType(ti))
            return nullptr;
        return ti->return_vec[0].spec.getHashDecl();
    }

    // static version of method, checking for null pointer
    DLLLOCAL static const QoreEnumDecl* getUniqueReturnEnum(const QoreTypeInfo* ti) {
        if (!ti || ti->return_vec.size() > 1 || !hasType(ti)) {
            return nullptr;
        }
        return ti->return_vec[0].spec.getEnum();
    }

    // static version of method, checking for null pointer
    DLLLOCAL static const QoreTypeInfo* getUniqueReturnComplexHash(const QoreTypeInfo* ti) {
        if (!ti || ti->return_vec.size() > 1 || !hasType(ti))
            return nullptr;
        return ti == autoHashTypeInfo ? autoTypeInfo : ti->return_vec[0].spec.getComplexHash();
    }

    // static version of method, checking for null pointer
    DLLLOCAL static const QoreTypeInfo* getUniqueReturnComplexList(const QoreTypeInfo* ti) {
        if (!ti || ti->return_vec.size() > 1 || !hasType(ti))
            return nullptr;
        return ti == autoListTypeInfo || ti == softAutoListTypeInfo
            ? autoTypeInfo
            : ti->return_vec[0].spec.getComplexList();
    }

    //! Returns the complex code type info if this is a typed callable type
    /** @param ti the type to check
        @return the QoreComplexCodeTypeInfo pointer if this is a typed callable, nullptr otherwise
    */
    DLLLOCAL static const class QoreComplexCodeTypeInfo* getComplexCodeType(const QoreTypeInfo* ti);

    //! Returns the union type info if this is a union type
    /** @param ti the type to check
        @return the QoreUnionTypeInfo pointer if this is a union type, nullptr otherwise
    */
    DLLLOCAL static const class QoreUnionTypeInfo* getUnionType(const QoreTypeInfo* ti);

    //! Checks if two typed callable types have compatible signatures
    /** This is used for additional type checking when assigning closures/call references
        to typed code variables. Standard variance rules apply:
        - Return types: covariant (source can be more specific)
        - Parameter types: contravariant (source can be more general)

        @param target the target type (typed code variable)
        @param source the source type (closure/call reference)
        @return true if compatible, false otherwise
    */
    DLLLOCAL static bool checkComplexCodeCompatibility(const QoreTypeInfo* target, const QoreTypeInfo* source);

    //! Returns the element type info for an iterator class based on source type
    /** For HashPairIterator with source hash<string, int>: returns the type info for
        hash<HashPairInfo> where HashPairInfo has key: string and value: int
        For HashKeyIterator with source hash<string, int>: returns the type info for string
        For HashIterator with source hash<string, int>: returns the type info for int (value type)
        For ListIterator with source list<T>: returns the type info for T

        @param iteratorClass the iterator class (e.g., HashPairIterator)
        @param sourceTypeInfo the type of the source expression (e.g., hash<string, int>)
        @return the element type info (QoreTypeInfo pointer), or nullptr if unknown

        @since Qore 2.1
    */
    DLLLOCAL static const QoreTypeInfo* getIteratorElementType(const QoreClass* iteratorClass,
        const QoreTypeInfo* sourceTypeInfo);

    //! Returns the implicit argument type for a functional operator's iterator expression
    /** This helper function extracts the implicit argument type from an iterator expression,
        checking both list element types and iterator class element types.

        @param iteratorExpr the iterator expression value (e.g., h.pairIterator())
        @param iteratorTypeInfo the type info of the iterator expression
        @return the implicit argument type for the iterator, or nullptr if unknown

        @since Qore 2.1
    */
    DLLLOCAL static const QoreTypeInfo* getImplicitArgTypeForIterator(const QoreValue& iteratorExpr,
        const QoreTypeInfo* iteratorTypeInfo);

    //! Returns or creates a HashPairInfo type with the given value type
    /** @param valueType the type for the "value" member
        @return the HashPairInfo type with the specified value type

        @since Qore 2.1
    */
    DLLLOCAL static const QoreTypeInfo* getHashPairType(const QoreTypeInfo* valueType);

    // static version of method, checking for null pointer
    DLLLOCAL static const QoreTypeInfo* getUniqueReturnComplexSoftList(const QoreTypeInfo* ti) {
        if (!ti || ti->return_vec.size() > 1 || !hasType(ti))
            return nullptr;
        return ti == softAutoListTypeInfo ? autoTypeInfo : ti->return_vec[0].spec.getComplexSoftList();
    }

    // static version of method, checking for null pointer
    DLLLOCAL static const QoreTypeInfo* getUniqueReturnComplexReference(const QoreTypeInfo* ti) {
        if (!ti || ti->return_vec.size() > 1 || !hasType(ti))
            return nullptr;
        return ti->return_vec[0].spec.getComplexReference();
    }

    // static version of method, checking for null pointer
    DLLLOCAL static const QoreTypeInfo* getReturnComplexHashOrNothing(const QoreTypeInfo* ti);

    // static version of method, checking for null pointer
    DLLLOCAL static const QoreTypeInfo* getReturnComplexListOrNothing(const QoreTypeInfo* ti) {
        if (!ti || !hasType(ti))
            return nullptr;
        if (ti->return_vec.size() > 1) {
            if (ti->return_vec.size() != 2 || (ti->return_vec[1].spec.match(NT_NOTHING) != QTI_IDENT))
                return nullptr;
        }
        return ti == autoListTypeInfo
            || ti == autoListOrNothingTypeInfo
            || ti == softAutoListTypeInfo
            || ti == softAutoListOrNothingTypeInfo
            ? autoTypeInfo
            : ti->return_vec[0].spec.getComplexList();
    }

    // static version of method, checking for null pointer
    DLLLOCAL static bool hasComplexType(const QoreTypeInfo* ti) {
        if (!ti) {
            return false;
        }
        return ti == autoTypeInfo || isComplex(ti);
    }

    // static version of method, checking for null pointer
    DLLLOCAL static bool hasType(const QoreTypeInfo* ti) {
        if (!ti) {
            return false;
        }
        assert(!ti->isAcceptVecEmpty());
        if (ti->getFirstAcceptSpec().spec.getType() == NT_ALL) {
            return false;
        }
        return true;
    }

    // static version of method, checking for null pointer
    DLLLOCAL static bool needsScan(const QoreTypeInfo* ti) {
        return ti && hasType(ti) ? ti->needsScanImpl() : true;
    }

    // static version of method, checking for null pointer
    DLLLOCAL static const char* getName(const QoreTypeInfo* ti) {
        return ti ? ti->tname.c_str() : NO_TYPE_INFO;
    }

    // static version of method, checking for null pointer
    DLLLOCAL static const char* getSimpleName(const QoreTypeInfo* ti) {
        return ti ? ti->getSimpleName() : NO_TYPE_INFO;
    }

    // static version of method, checking for null pointer
    DLLLOCAL static const char* getPath(const QoreTypeInfo* ti) {
        if (!ti) {
            return NO_TYPE_INFO;
        }
        return ti->getPathImpl();
    }

    // static version of method, checking for null pointer
    DLLLOCAL static void getThisType(const QoreTypeInfo* ti, QoreString& str) {
        if (ti)
            ti->getThisTypeImpl(str);
        else
            str.sprintf("no value");
    }

    // static version of method, checking for null pointer
    DLLLOCAL static void acceptInputParam(const QoreTypeInfo* ti, int param_num, const char* param_name, QoreValue& n,
            ExceptionSink* xsink) {
        if (hasType(ti)) {
            ti->acceptInputIntern(xsink, "parameter", false, param_num, param_name, n);
        } else if (ti != autoTypeInfo) {
            stripTypeInfo(n, xsink);
        }
    }

    // static version of method, checking for null pointer
    DLLLOCAL static void acceptInputMember(const QoreTypeInfo* ti, const char* member_name, QoreValue& n,
            ExceptionSink* xsink) {
        if (hasType(ti)) {
            ti->acceptInputIntern(xsink, "member", true, -1, member_name, n);
        } else if (ti != autoTypeInfo) {
            stripTypeInfo(n, xsink);
        }
    }

    // static version of method, checking for null pointer
    DLLLOCAL static void acceptInputKey(const QoreTypeInfo* ti, const char* member_name, QoreValue& n,
            ExceptionSink* xsink) {
        if (hasType(ti)) {
            ti->acceptInputIntern(xsink, "key", false, -1, member_name, n);
        } else if (ti != autoTypeInfo) {
            stripTypeInfo(n, xsink);
        }
    }

    // static version of method, checking for null pointer
    DLLLOCAL static void acceptAssignment(const QoreTypeInfo* ti, const char* text, QoreValue& n,
            ExceptionSink* xsink, LValueHelper* lvhelper = nullptr) {
        assert(text && text[0] == '<');
        if (hasType(ti)) {
            ti->acceptInputIntern(xsink, "lvalue", false, -1, text, n);
        } else if (ti != autoTypeInfo) {
            stripTypeInfo(n, xsink, lvhelper);
        }
    }

    // static version of method, checking for null pointer
    DLLLOCAL static bool hasDefaultValue(const QoreTypeInfo* ti) {
        return ti ? ti->hasDefaultValueImpl() : false;
    }

    // static version of method, checking for null pointer
    DLLLOCAL static QoreValue getDefaultQoreValue(const QoreTypeInfo* ti) {
        return ti ? ti->getDefaultQoreValueImpl() : QoreValue();
    }

    // static version of method, checking for null pointer
    DLLLOCAL static bool mayRequireFilter(const QoreTypeInfo* ti, const QoreValue& n) {
        if (!hasType(ti) && ti != autoTypeInfo) {
            return isComplex(n.getTypeInfo());
        }
        assert(ti);
        return ti->mayRequireFilter(n);
    }

    // static version of method, checking for null pointer
    DLLLOCAL static bool equal(const QoreTypeInfo* a, const QoreTypeInfo* b) {
        if (a == b)
            return true;
        bool hta = hasType(a);
        bool htb = hasType(b);
        if (hta && htb)
            return accept_vec_compare(a->getAcceptSpecs(), b->getAcceptSpecs()) && return_vec_compare(a->return_vec, b->return_vec);
        return hta || htb ? false : true;
    }

    // static version of method, checking for null pointer
    DLLLOCAL static bool isInputIdentical(const QoreTypeInfo* a, const QoreTypeInfo* b) {
        if (a == b)
            return true;
        bool hta = hasType(a);
        bool htb = hasType(b);
        if (hta && htb)
            return accept_vec_compare(a->getAcceptSpecs(), b->getAcceptSpecs());
        return hta || htb ? false : true;
    }

    // static version of method, checking for null pointer
    DLLLOCAL static bool isOutputIdentical(const QoreTypeInfo* a, const QoreTypeInfo* b) {
        if (a == b)
            return true;
        bool hta = hasType(a);
        bool htb = hasType(b);
        if (hta && htb)
            return return_vec_compare(a->return_vec, b->return_vec);
        return hta || htb ? false : true;
    }

    // if second's return type is compatible with first's return type
    // static version of method, checking for null pointer
    DLLLOCAL static bool isOutputCompatible(const QoreTypeInfo* first, const QoreTypeInfo* second) {
        if (hasType(first) && hasType(second)) {
            return parseAccepts(first, second);
        }
        return true;
    }

    // if first is a superset of second with a strict interpretation that the order of accept and return declarations must also be the same
    // static version of method, checking for null pointer
    DLLLOCAL static bool superSetOf(const QoreTypeInfo* first, const QoreTypeInfo* second) {
        if (hasType(first)) {
            if (hasType(second))
                return first->superSetOf(second);
            return false;
        }
        return hasType(second);
    }

    // returns true if first's output is a superset of second's output with a strict interpretation that the order of return declarations must also be the same
    DLLLOCAL static bool outputSuperSetOf(const QoreTypeInfo* first, const QoreTypeInfo* second) {
        if (!hasType(first))
            return true;

        if (hasType(second))
            return first == second ? true : first->outputSuperSetOf(second);
        return false;
    }

    // static version of method, checking for null pointer
    DLLLOCAL static bool canConvertToScalar(const QoreTypeInfo* ti) {
        return ti ? ti->canConvertToScalarImpl() : true;
    }

    // static version of method, checking for null pointer
    DLLLOCAL static void checkDoNonNumericWarning(const QoreTypeInfo* ti, const QoreProgramLocation* loc, const char* preface) {
        if (ti && !ti->canConvertToScalarImpl())
            ti->doNonNumericWarning(loc, preface);
    }

    // static version of method, checking for null pointer
    DLLLOCAL static void checkDoNonBooleanWarning(const QoreTypeInfo* ti, const QoreProgramLocation* loc, const char* preface) {
        if (ti && !ti->canConvertToScalarImpl())
            ti->doNonBooleanWarning(loc, preface);
    }

    // static version of method, checking for null pointer
    DLLLOCAL static void checkDoNonStringWarning(const QoreTypeInfo* ti, const QoreProgramLocation* loc, const char* preface) {
        if (ti && !ti->canConvertToScalarImpl())
            ti->doNonStringWarning(loc, preface);
    }

    // static version of method, checking for null pointer
    DLLLOCAL static void concatName(const QoreTypeInfo* ti, std::string& str) {
        if (ti && (hasType(ti) || ti == autoTypeInfo))
            str.append(ti->tname.c_str());
        else
            str.append(NO_TYPE_INFO);
    }

    // check for a common type
    DLLLOCAL static bool matchCommonType(const QoreTypeInfo*& ctype, const QoreTypeInfo* ntype) {
        // issue #3005: if the first element had no type, then there is no common type
        if (!ctype || ctype == anyTypeInfo) {
            ctype = nullptr;
            return false;
        }
        if (ctype == ntype) {
            return true;
        }
        if (!QoreTypeInfo::hasType(ntype)) {
            // issue #2791: when performing type folding, do not set to type "any" but rather use "auto"
            ctype = ntype == anyTypeInfo ? nullptr : ntype;
            return false;
        }

        // ctype |* NOTHING -> *type
        if (!QoreTypeInfo::parseReturns(ctype, NT_NOTHING) && QoreTypeInfo::isType(ntype, NT_NOTHING)) {
            const QoreTypeInfo* ti = get_or_nothing_type(ctype);
            ctype = ti;
            return ctype != autoTypeInfo ? true : false;
        }

        // ctype==NOTHING | type -> *type
        if (QoreTypeInfo::isType(ctype, NT_NOTHING)) {
            ctype = get_or_nothing_type_check(ntype);
            return ctype != autoTypeInfo ? true : false;
        }

        // ctype |* *ctype -> *ctype
        // if the new type is a superset of the existing common type, then use the new type
        if (ntype->superSetOf(ctype)) {
            ctype = ntype;
            return true;
        }

        // try to find a common base type
        // if we're dealing with types that return multiple types, then they are not compatible
        if (ctype->return_vec.size() > 1 || ntype->return_vec.size() > 1) {
            // issue #2791: when performing type folding, do not set to type "any" but rather use "auto"
            ctype = autoTypeInfo;
            return false;
        }

        // see if we have a complex type
        const QoreTypeInfo* bti = ctype->return_vec[0].spec.getBaseTypeInfo();
        if (bti == ntype->return_vec[0].spec.getBaseTypeInfo()) {
            // issue #3429: when performing type folding with complex types, do not use subtype "any" but rather use "auto"
            if (bti == hashTypeInfo) {
                // Check if both are hashdecls before degrading to autoHashTypeInfo
                // Fixes issue where hashdecl type infos from different module load instances are incorrectly
                // degraded to hash<auto>, causing overload resolution failures and type checking errors
                const TypedHashDecl* hd1 = ctype->return_vec[0].spec.getHashDecl();
                const TypedHashDecl* hd2 = ntype->return_vec[0].spec.getHashDecl();
                if (hd1 && hd2) {
                    // Both are hashdecls - preserve the typed structure instead of degrading to auto
                    // Structural equality will be checked at runtime if needed
                    return true;
                }
                ctype = autoHashTypeInfo;
            } else if (bti == listTypeInfo) {
                ctype = autoListTypeInfo;
            } else if (bti == softListTypeInfo) {
                ctype = softAutoListTypeInfo;
            } else if (bti == hashOrNothingTypeInfo) {
                ctype = autoHashOrNothingTypeInfo;
            } else if (bti == listOrNothingTypeInfo) {
                ctype = autoListOrNothingTypeInfo;
            } else if (bti == softListOrNothingTypeInfo) {
                ctype = softAutoListOrNothingTypeInfo;
            } else {
                ctype = bti;
            }
            return true;
        }
        // issue #2791: when performing type folding, do not set to type "any" but rather use "auto"
        ctype = autoTypeInfo;
        return false;
    }

    //! returns true if ti could return a complex type
    DLLLOCAL static bool isComplex(const QoreTypeInfo* ti) {
        if (!ti)
            return false;
        for (auto& i : ti->return_vec) {
            if (i.spec.isComplex())
                return true;
        }
        return false;
    }

    //! returns true if the type is an explicit list type
    DLLLOCAL static bool isListType(const QoreTypeInfo* ti) {
        if (!hasType(ti)) {
            return false;
        }
        return ti->return_vec[0].spec.getType() == NT_LIST;
    }

    //! returns true if the type is an explicit hash type
    DLLLOCAL static bool isHashType(const QoreTypeInfo* ti) {
        if (!hasType(ti)) {
            return false;
        }
        return ti->return_vec[0].spec.getType() == NT_HASH;
    }

    //! returns the element type, if any (nullptr if not applicable)
    DLLLOCAL static const QoreTypeInfo* getElementType(const QoreTypeInfo* ti) {
        if (!hasType(ti)) {
            return nullptr;
        }
        assert(!ti->return_vec.empty());
        if (ti == autoListTypeInfo
            || ti == autoListOrNothingTypeInfo
            || ti == softAutoListTypeInfo
            || ti == softAutoListOrNothingTypeInfo
            || ti == autoHashTypeInfo
            || ti == autoHashOrNothingTypeInfo) {
            return autoTypeInfo;
        }
        return ti->return_vec[0].spec.getElementType();
    }

    //! returns the typed hash ptr, if applicable
    DLLLOCAL static const TypedHashDecl* getTypedHash(const QoreTypeInfo* ti) {
        if (!hasType(ti)) {
            return nullptr;
        }
        assert(!ti->return_vec.empty());
        // CRITICAL FIX Phase 2: Don't extract hashdecl from optional complex hash types
        // For optional types like *hash<string, hash<DataProviderExpressionInfo>>,
        // return_vec has 2 specs: [actual_type, NOTHING].
        // Only reject if the base type is a complex hash (not a hashdecl itself)
        if (ti->return_vec.size() > 1) {
            // For optional types, check if base type is a complex hash by examining typespec
            if (ti->return_vec[0].spec.getTypeSpec() == QTS_COMPLEXHASH) {
                // It's an optional complex hash type - don't extract hashdecl from it
                // This prevents the inner hashdecl from being incorrectly applied to the outer hash
                return nullptr;
            }
            // It's an optional type but not a complex hash (e.g., *hashdecl)
            // Fall through to extract the hashdecl
        }
        return ti->return_vec[0].spec.getHashDecl();
    }

    // static version of method, checking for null pointer
    DLLLOCAL static const QoreClass* getReturnClass(const QoreTypeInfo* ti) {
        if (!hasType(ti)) {
            return nullptr;
        }
        assert(!ti->return_vec.empty());
        return ti->return_vec[0].spec.getClass();
    }

    //! returns the enum ptr, if applicable
    DLLLOCAL static const QoreEnumDecl* getReturnEnum(const QoreTypeInfo* ti) {
        if (!hasType(ti)) {
            return nullptr;
        }
        assert(!ti->return_vec.empty());
        return ti->return_vec[0].spec.getEnum();
    }

    // static version of method, checking for null pointer
    DLLLOCAL static const QoreTypeInfo* getComplexHashValueType(const QoreTypeInfo* ti) {
        if (!hasType(ti)) {
            return nullptr;
        }
        assert(!ti->return_vec.empty());
        if (ti == autoHashTypeInfo || ti == autoHashOrNothingTypeInfo) {
            return autoTypeInfo;
        }
        // Handle or_nothing types which have 2 return specs (actual type + NOTHING)
        // CRITICAL FIX: Validate structure for optional types
        if (ti->return_vec.size() > 1) {
            // Optional type must have exactly 2 specs: [actual_type, NOTHING]
            if (ti->return_vec.size() != 2 || (ti->return_vec[1].spec.match(NT_NOTHING) != QTI_IDENT)) {
                // Malformed optional type - reject it to prevent type corruption
                return nullptr;
            }
        }
        const QoreTypeInfo* result = ti->return_vec[0].spec.getComplexHash();

        // CRITICAL FIX for hash<HashdeclType> cast operations:
        // When parsing `cast<hash<string, hash<DataProviderExpressionInfo>>>()`,
        // the parser creates a type where:
        //   - getName() = "hash<string, hash<DataProviderExpressionInfo>>"
        //   - getComplexHash() = nullptr (not a QoreComplexHashTypeSpec)
        //   - getHashDecl() = DataProviderExpressionInfo hashdecl
        //
        // The cast operator selection logic uses getComplexHashValueType() to determine
        // which handler to use. Without this fix, it returns nullptr and selects the
        // QoreHashDeclCastOperatorNode handler, which incorrectly sets the hashdecl
        // binding on the cast result. This causes "INVALID-MEMBER" errors when accessing
        // hash keys later (the hash thinks it's a typed-hash, not a complex hash).
        //
        // Solution: Return the hashdecl's type info (which represents the hashdecl-typed
        // hash, i.e., hash<string, HashdeclType>), so the complex hash handler is selected.
        //
        // Note: This is semantically correct because:
        // - For `hash<string, DataProviderExpressionInfo>`, the "value type" is really
        //   "DataProviderExpressionInfo as a typed-hash type", which is what hd->getTypeInfo()
        //   represents in the context of complex hashes.
        // - The cast operator uses this to determine HOW to cast, not what the final type is.
        // - The final type is still the full complex hash, set via parse_context.typeInfo.
        if (!result) {
            const TypedHashDecl* hd = ti->return_vec[0].spec.getHashDecl();
            if (hd) {
                const char* type_name = getName(ti);
                if (type_name && strstr(type_name, "hash<")) {
                    // This is hash<HashdeclType> notation - return the hashdecl's type info
                    // to signal that complex hash handler should be used
                    return hd->getTypeInfo();
                }
            }
        }
        return result;
    }

    // static version of method, checking for null pointer
    DLLLOCAL static const QoreTypeInfo* getComplexListValueType(const QoreTypeInfo* ti) {
        if (!hasType(ti)) {
            return nullptr;
        }
        assert(!ti->return_vec.empty());
        if (ti == autoListTypeInfo || ti == autoListOrNothingTypeInfo) {
            return autoTypeInfo;
        }
        // Handle or_nothing types which have 2 return specs (actual type + NOTHING)
        // CRITICAL FIX: Validate structure for optional types
        if (ti->return_vec.size() > 1) {
            // Optional type must have exactly 2 specs: [actual_type, NOTHING]
            if (ti->return_vec.size() != 2 || (ti->return_vec[1].spec.match(NT_NOTHING) != QTI_IDENT)) {
                // Malformed optional type - reject it to prevent type corruption
                return nullptr;
            }
        }
        return ti->return_vec[0].spec.getComplexList();
    }

    DLLLOCAL void getAcceptTypes(ReferenceHolder<QoreHashNode>& h, bool simple = false) const {
        for (auto& i : getAcceptSpecs()) {
            const char* type_name = simple ? i.spec.getSimpleTypeName() : i.spec.getTypeName();
            h->setKeyValue(type_name, true, nullptr);
            if (!strcmp(type_name, "int")) {
                h->setKeyValue("integer", true, nullptr);
            }
        }
    }

    DLLLOCAL void getReturnTypes(ReferenceHolder<QoreHashNode>& h, bool simple = false) const {
        for (auto& i : return_vec) {
            const char* type_name = simple ? i.spec.getSimpleTypeName() : i.spec.getTypeName();
            h->setKeyValue(type_name, true, nullptr);
            if (!strcmp(type_name, "int")) {
                h->setKeyValue("integer", true, nullptr);
            }
        }
    }

    DLLLOCAL int doAcceptError(bool priv_error, const char* arg_type, bool obj, int param_num, const char* param_name,
            const QoreValue& n, ExceptionSink* xsink) const;

    DLLLOCAL int doTypeException(const char* arg_type, int param_num, const char* param_name, const QoreValue& n,
            ExceptionSink* xsink) const;

    //! Returns true if this type looks like it could have come from type narrowing
    /** Simple scalar types like int, string, float, etc. that appear in hash value positions
        are likely to have come from type narrowing of hash<auto>.

        @note We only check hard types (not soft types) since type narrowing typically produces
        hard types. We don't include nothing/null since those represent absence of value rather
        than narrowed types.

        @since %Qore 2.1
    */
    DLLLOCAL bool isSimpleTypeNarrowed() const {
        // Check if this is a simple type that could be the result of hash<auto> narrowing
        return this == bigIntTypeInfo || this == stringTypeInfo || this == floatTypeInfo
            || this == boolTypeInfo || this == dateTypeInfo || this == binaryTypeInfo
            || this == numberTypeInfo;
    }

    DLLLOCAL void acceptInputIntern(ExceptionSink* xsink, const char* arg_type, bool obj, int param_num,
            const char* param_name, QoreValue& n, LValueHelper* lvhelper = nullptr) const {
        // CRITICAL FIX Phase 2: Prioritize complex hash/list handlers for optional types
        // For optional types (return_vec.size() > 1), we need to ensure complex hash/list
        // handlers are tried BEFORE hashdecl handlers to prevent incorrect routing.
        // This prevents inner hashdecls from being extracted and applied to outer containers.

        if (return_vec.size() > 1 && return_vec[1].spec.match(NT_NOTHING) == QTI_IDENT) {
            // This is a proper optional type. For optional complex hashes/lists, try those first.
            // Check if return_vec[0] indicates a complex hash or list
            q_typespec_t base_typespec = return_vec[0].spec.getTypeSpec();
            if (base_typespec == QTS_COMPLEXHASH || base_typespec == QTS_COMPLEXLIST ||
                base_typespec == QTS_COMPLEXSOFTLIST) {
                // Try complex hash/list specs first
                for (auto& t : getAcceptSpecs()) {
                    q_typespec_t spec_type = t.spec.getTypeSpec();
                    if (spec_type == QTS_COMPLEXHASH || spec_type == QTS_COMPLEXLIST ||
                        spec_type == QTS_COMPLEXSOFTLIST) {
                        if (t.spec.acceptInput(xsink, *this, t.map, arg_type, obj, param_num, param_name, n, lvhelper)) {
                            return;
                        }
                    }
                }
                // If complex handlers didn't work, try hashdecl/other handlers
                for (auto& t : getAcceptSpecs()) {
                    q_typespec_t spec_type = t.spec.getTypeSpec();
                    if (spec_type != QTS_COMPLEXHASH && spec_type != QTS_COMPLEXLIST &&
                        spec_type != QTS_COMPLEXSOFTLIST) {
                        if (t.spec.acceptInput(xsink, *this, t.map, arg_type, obj, param_num, param_name, n, lvhelper)) {
                            return;
                        }
                    }
                }
                doAcceptError(false, arg_type, obj, param_num, param_name, n, xsink);
                return;
            }
        }

        // Normal routing for non-optional or non-complex types
        for (auto& t : getAcceptSpecs()) {
            if (t.spec.acceptInput(xsink, *this, t.map, arg_type, obj, param_num, param_name, n, lvhelper)) {
                return;
            }
        }
        doAcceptError(false, arg_type, obj, param_num, param_name, n, xsink);
    }

    DLLLOCAL void doNonNumericWarning(const QoreProgramLocation* loc, const char* preface) const;
    DLLLOCAL void doNonBooleanWarning(const QoreProgramLocation* loc, const char* preface) const;
    DLLLOCAL void doNonStringWarning(const QoreProgramLocation* loc, const char* preface) const;
    DLLLOCAL void doNonStringError(const QoreProgramLocation* loc, const char* preface) const;

    DLLLOCAL const char* getSimpleName() const {
        return sname.empty() ? tname.c_str() : sname.c_str();
    }

protected:
    QoreString tname;
    QoreString sname;

    DLLLOCAL QoreTypeInfo(const q_accept_vec_t&& a_vec, const q_return_vec_t&& r_vec, const QoreString& tname)
            : accept_vec(a_vec), return_vec(r_vec), tname(tname) {
        setSimpleName();
    }

    DLLLOCAL void setSimpleName() {
        ssize_t i = tname.find('<');
        if (i == -1) {
            return;
        }
        sname.set(tname.c_str(), i);
    }

    DLLLOCAL int doObjectPrivateClassException(const char* param_name, ExceptionSink* xsink) const {
        assert(xsink);
        QoreStringNode* desc = new QoreStringNode;
        desc->sprintf("member '%s' expects type '%s', but got an object where this class is privately inherited instead", param_name, tname.c_str());
        xsink->raiseException("RUNTIME-TYPE-ERROR", desc);
        return -1;
    }

    DLLLOCAL int doPrivateClassException(const char* arg_type, int param_num, const char* param_name, ExceptionSink* xsink) const {
        // xsink may be null in case that parse exceptions have been disabled in the QoreProgram object
        // for example if there was a "requires" error
        if (!xsink)
            return -1;

        QoreStringNode* desc = new QoreStringNode;
        QoreTypeInfo::ptext(*desc, arg_type, param_num, param_name);
        desc->sprintf("expects type '%s', but got an object where this class is privately inherited instead", tname.c_str());
        xsink->raiseException("RUNTIME-TYPE-ERROR", desc);
        return -1;
    }

    DLLLOCAL int doObjectHashDeclTypeException(const char* arg_type, const char* param_name, const QoreValue& n, ExceptionSink* xsink) const {
        assert(xsink);
        QoreStringNode* desc = new QoreStringNode;
        desc->sprintf("%s '%s' expects type '%s', but got ", arg_type, param_name, tname.c_str());
        QoreTypeInfo::getNodeType(*desc, n);
        desc->concat(" instead");
        xsink->raiseException("RUNTIME-TYPE-ERROR", desc);
        return -1;
    }

    // returns true if "this" is a superset of the argument with a strict interpretation that the order of accept and return declarations must also be the same
    DLLLOCAL bool superSetOf(const QoreTypeInfo* t) const {
        if (accept_vec.size() < t->getAcceptVecSize() || return_vec.size() < t->return_vec.size())
            return false;

        for (unsigned i = 0; i < t->getAcceptVecSize(); ++i) {
            if (t->getAcceptSpec(i).spec != getAcceptSpec(i).spec)
                return false;
        }

        for (unsigned i = 0; i < t->return_vec.size(); ++i) {
            if (t->return_vec[i].spec != return_vec[i].spec)
                return false;
        }

        return true;
    }

    // returns true if "this"'s output is a superset of the argument's output with a strict interpretation that the order of return declarations must also be the same
    DLLLOCAL bool outputSuperSetOf(const QoreTypeInfo* t) const {
        if (return_vec.size() < t->return_vec.size())
            return false;

        for (unsigned i = 0; i < t->return_vec.size(); ++i) {
            bool may_not_match = false;
            bool may_need_filter = false;
            if (!return_vec[i].spec.match(t->return_vec[i].spec, may_not_match, may_need_filter))
                return false;
            if (may_not_match)
                return false;
        }

        return true;
    }

    DLLLOCAL qore_type_t getSingleType() const {
        if (accept_vec.size() == 1 && return_vec.size() == 1) {
            qore_type_t qt = getFirstAcceptSpec().spec.getType();
            if (qt == return_vec[0].spec.getType())
                return qt;
        }
        return NT_ALL;
    }

    DLLLOCAL qore_type_t getBaseType() const {
        assert(!return_vec.empty());
        return return_vec[0].spec.getType();
    }

    DLLLOCAL bool parseAcceptsReturns(qore_type_t t) const {
        bool ok = false;
        for (auto& i : getAcceptSpecs()) {
            if (i.spec.matchType(t) != QTI_NOT_EQUAL) {
                ok = true;
                break;
            }
        }
        if (!ok)
            return false;
        for (auto& i : return_vec) {
            if (i.spec.matchType(t) != QTI_NOT_EQUAL)
                return true;
        }
        return false;
    }

    DLLLOCAL qore_type_result_e parseReturns(QoreTypeSpec t) const {
        if (return_vec.size() == 1)
            return t.match(return_vec[0].spec);
        for (auto& i : return_vec) {
            qore_type_result_e rv = t.match(i.spec);
            if (rv != QTI_NOT_EQUAL)
                return i.exact ? QTI_IDENT : QTI_AMBIGUOUS;
        }
        return QTI_NOT_EQUAL;
    }

    // returns true if this type only returns the type given
    DLLLOCAL bool isType(qore_type_t t) const {
        if (return_vec.size() > 1)
            return false;
        return t == return_vec[0].spec.getType();
    }

    // returns true if the type matches an accept type with a filter (type only checked)
    DLLLOCAL bool mayRequireFilter(const QoreValue& n) const {
        for (auto& at : getAcceptSpecs()) {
            if (at.map && at.spec.matchType(n.getType()) != QTI_NOT_EQUAL)
                return true;
        }
        return false;
    }

    DLLLOCAL qore_type_result_e parseAccepts(const QoreTypeInfo* typeInfo, bool& may_not_match,
            bool& may_need_filter, qore_type_result_e& max_result, bool known_initial_assignment = false) const {
        //printd(5, "QoreTypeInfo::parseAccepts() '%s' <- '%s'\n", tname.c_str(), typeInfo->tname.c_str());
        if (typeInfo->return_vec.size() > accept_vec.size()) {
            may_not_match = true;
        }

        bool ok = false;
        for (auto& rt : typeInfo->return_vec) {
            bool t_no_match = true;
            for (auto& at : getAcceptSpecs()) {
                qore_type_result_e t_max_result = QTI_NOT_EQUAL;
                qore_type_result_e res = parseAcceptsIntern(at, rt, may_not_match, may_need_filter, t_no_match, ok,
                    t_max_result, known_initial_assignment);
                if (res == QTI_IDENT) {
                    max_result = t_max_result;
                    return res;
                } else if (res == QTI_AMBIGUOUS || res == QTI_NEAR || res == QTI_WILDCARD) {
                    max_result = t_max_result;
                    assert(ok);
                    if (may_not_match) {
                        return res;
                    }
                    break;
                }
            }
            if (t_no_match) {
                if (!may_not_match) {
                    may_not_match = true;
                    if (ok) {
                        return QTI_AMBIGUOUS;
                    }
                }
            }
        }
        if (ok) {
            return QTI_AMBIGUOUS;
        }
        may_not_match = false;
        return QTI_NOT_EQUAL;
    }

    // checks that types are near identical at runtime
    /** accept and return types match in the same order and with no match worse than QTI_NEAR
    */
    DLLLOCAL qore_type_result_e runtimeTypeMatch(const QoreTypeInfo* typeInfo) const {
        //printd(5, "QoreTypeInfo::runtimeTypeMatch() '%s' <=> '%s'\n", tname.c_str(), typeInfo->tname.c_str());
        if (typeInfo->return_vec.size() != return_vec.size() || typeInfo->getAcceptVecSize() != accept_vec.size()) {
            return QTI_NOT_EQUAL;
        }

        // check accept types
        qore_type_result_e rc = QTI_IDENT;
        for (size_t i = 0; i < accept_vec.size(); ++i) {
            bool may_not_match = false;
            bool may_need_filter = false;
            qore_type_result_e res = getAcceptSpec(i).spec.match(typeInfo->getAcceptSpec(i).spec, may_not_match, may_need_filter);
            //printd(5, " + accept: %s %d <=> %s %d: %d\n", QoreTypeInfo::getName(getAcceptSpec(i).spec.getBaseTypeInfo()), getAcceptSpec(i).spec.getTypeSpec(), QoreTypeInfo::getName(typeInfo->getAcceptSpec(i).spec.getBaseTypeInfo()), typeInfo->getAcceptSpec(i).spec.getTypeSpec(), res);
            if (res < QTI_NEAR) {
                return QTI_NOT_EQUAL;
            }
            if (res < rc) {
                rc = res;
            }
        }

        // check return types
        for (size_t i = 0; i < return_vec.size(); ++i) {
            bool may_not_match = false;
            bool may_need_filter = false;
            qore_type_result_e res = return_vec[i].spec.match(typeInfo->return_vec[i].spec, may_not_match, may_need_filter);
            //printd(5, " + return: %s %d <=> %s %d: %d\n", QoreTypeInfo::getName(return_vec[i].spec.getBaseTypeInfo()), return_vec[i].spec.getTypeSpec(), QoreTypeInfo::getName(typeInfo->return_vec[i].spec.getBaseTypeInfo()), typeInfo->return_vec[i].spec.getTypeSpec(), res);
            if (res < QTI_NEAR) {
                return QTI_NOT_EQUAL;
            }
            if (res < rc) {
                rc = res;
            }
        }

        return rc;
    }

    DLLLOCAL qore_type_result_e runtimeAcceptsValue(const QoreValue& n) const;

    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return true;
    }

    DLLLOCAL virtual void getThisTypeImpl(QoreString& str) const {
        str.sprintf("type '%s'", tname.c_str());
    }

    DLLLOCAL virtual const char* getPathImpl() const {
        return tname.c_str();
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return false;
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
        return QoreValue();
    }

    // returns true if there is no type or if the type can be converted to a scalar (numeric, bool, int, string, etc) value, false true if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const = 0;

    DLLLOCAL static qore_type_result_e parseAcceptsIntern(const QoreAcceptSpec& at, const QoreReturnSpec& rt,
            bool& may_not_match, bool& may_need_filter, bool& t_no_match, bool& ok, qore_type_result_e& max_result,
            bool known_initial_assignment) {
        //printd(5, "QoreTypeInfo::parseAcceptsIntern() at: %d rt: %d rc: %d\n", (int)at.spec.getTypeSpec(),
        //    (int)rt.spec.getTypeSpec(), at.spec.match(rt.spec, may_not_match, may_need_filter));
        qore_type_result_e res = at.spec.match(rt.spec, may_not_match, may_need_filter, max_result,
            known_initial_assignment);
        switch (res) {
            case QTI_IDENT:
                if (at.exact && rt.exact) {
                    if (at.map && !may_need_filter) {
                        may_need_filter = true;
                    }
                    return QTI_IDENT;
                }
            // fall down to next case
            case QTI_NEAR:
            case QTI_AMBIGUOUS:
            case QTI_WILDCARD:
                if (at.map && !may_need_filter) {
                    may_need_filter = true;
                }
                if (t_no_match) {
                    t_no_match = false;
                    if (!ok) {
                        ok = true;
                        if (may_not_match) {
                            return res;
                        }
                    }
                }

            // fall down to default
            default:
                break;
        }
        return QTI_NOT_EQUAL;
    }

    DLLLOCAL static void getNodeType(QoreString& str, const QoreValue& n);

    DLLLOCAL static void ptext(QoreString& str, const char* arg_type, int param_num, const char* param_name);

    DLLLOCAL static void stripTypeInfo(QoreValue& n, ExceptionSink* xsink, LValueHelper* lvhelper = nullptr);

private:
    const q_accept_vec_t accept_vec;
};

class QoreAnyTypeInfo : public QoreTypeInfo {
public:
   DLLLOCAL QoreAnyTypeInfo() : QoreTypeInfo("any", q_accept_vec_t {{NT_ALL, nullptr}}, q_return_vec_t {{NT_ALL}}) {
   }

protected:
   DLLLOCAL virtual void getThisTypeImpl(QoreString& str) const {
      str.concat("no value");
   }

   // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
   DLLLOCAL virtual bool canConvertToScalarImpl() const {
      return true;
   }
};

class QoreAutoTypeInfo : public QoreTypeInfo {
public:
   DLLLOCAL QoreAutoTypeInfo() : QoreTypeInfo("auto", q_accept_vec_t {{NT_ALL, nullptr}}, q_return_vec_t {{NT_ALL}}) {
   }

protected:
   DLLLOCAL virtual void getThisTypeImpl(QoreString& str) const {
      str.concat("no value");
   }

   // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
   DLLLOCAL virtual bool canConvertToScalarImpl() const {
      return true;
   }
};

// Marker type for auto! - same behavior as auto but signals no type narrowing
// This type is only used during parsing to detect the auto! syntax
class QoreAutoNoNarrowTypeInfo : public QoreAutoTypeInfo {
public:
    DLLLOCAL QoreAutoNoNarrowTypeInfo() : QoreAutoTypeInfo() {
    }
};

class QoreClassTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreClassTypeInfo(const QoreClass* qc, const char* name, const char* path)
        : QoreTypeInfo(q_accept_vec_t {{qc, nullptr, true}}, q_return_vec_t {{qc, true}},
            QoreStringMaker("object<%s>", name)) {
        assert(path && *path);
        pname = "object<";
        pname.append(path);
        pname.append(">");
    }

protected:
    std::string pname;

    DLLLOCAL QoreClassTypeInfo(const q_accept_vec_t&& a_vec, const q_return_vec_t&& r_vec, const QoreString& tname)
        : QoreTypeInfo(std::move(a_vec), std::move(r_vec), tname) {
    }

    DLLLOCAL virtual void getThisTypeImpl(QoreString& str) const {
        qore_string_private::get(str)->concat(&tname);
    }

    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return false;
    }

    DLLLOCAL const char* getPathImpl() const {
        return pname.c_str();
    }
};

class QoreClassOrNothingTypeInfo : public QoreClassTypeInfo {
public:
    DLLLOCAL QoreClassOrNothingTypeInfo(const QoreClass* qc, const char* name, const char* path)
            : QoreClassTypeInfo(q_accept_vec_t {
                {qc, nullptr},
                {NT_NOTHING, nullptr},
                {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }},
            }, q_return_vec_t {{qc}, {NT_NOTHING}}, QoreStringMaker("*object<%s>", name)) {
        assert(path && *path);
        pname = "*object<";
        pname.append(path);
        pname.append(">");
    }

protected:
    DLLLOCAL virtual void getThisTypeImpl(QoreString& str) const {
        str.sprintf("an object of class '%s' or no value (NOTHING)", getFirstAcceptSpec().spec.getClass()->getName());
    }

    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return false;
    }
};

class QoreHashDeclTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreHashDeclTypeInfo(const TypedHashDecl* hd, const char* name, const char* path)
            : QoreTypeInfo(q_accept_vec_t {{hd, nullptr, true}}, q_return_vec_t {{hd, true}},
            QoreStringMaker("hash<%s>", name)) {
        assert(path && *path);
        pname = "hash<";
        pname.append(path);
        pname.append(">");
    }

protected:
    std::string pname;

    DLLLOCAL QoreHashDeclTypeInfo(const q_accept_vec_t&& a_vec, const q_return_vec_t&& r_vec, const QoreString& tname)
        : QoreTypeInfo(std::move(a_vec), std::move(r_vec), tname) {
    }

    DLLLOCAL virtual void getThisTypeImpl(QoreString& str) const {
        qore_string_private::get(str)->concat(&tname);
    }

    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return false;
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return true;
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const;

    DLLLOCAL const char* getPathImpl() const {
        return pname.c_str();
    }
};

class QoreHashDeclOrNothingTypeInfo : public QoreHashDeclTypeInfo {
public:
    DLLLOCAL QoreHashDeclOrNothingTypeInfo(const TypedHashDecl* hd, const char* name, const char* path)
            : QoreHashDeclTypeInfo(q_accept_vec_t {
                {hd, nullptr},
                {NT_NOTHING, nullptr},
                {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }},
            }, q_return_vec_t {{hd}, {NT_NOTHING}}, QoreStringMaker("*hash<%s>", name)) {
        assert(path && *path);
        pname = "*hash<";
        pname.append(path);
        pname.append(">");
    }

protected:
    DLLLOCAL virtual void getThisTypeImpl(QoreString& str) const {
        str.sprintf("hash<%s> or no value (NOTHING)", getFirstAcceptSpec().spec.getHashDecl()->getName());
    }

    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return false;
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return false;
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
        return QoreValue();
    }
};

class QoreEnumTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreEnumTypeInfo(const QoreEnumDecl* ed, const char* name, const char* path);

    //! Constructor with explicit accept/return vectors for or-nothing variant
    DLLLOCAL QoreEnumTypeInfo(const q_accept_vec_t&& a_vec, const q_return_vec_t&& r_vec, const QoreString& tname,
            const char* path)
            : QoreTypeInfo(std::move(a_vec), std::move(r_vec), tname) {
        assert(path && *path);
        pname = path;
    }

protected:
    std::string pname;

    DLLLOCAL virtual void getThisTypeImpl(QoreString& str) const {
        qore_string_private::get(str)->concat(&tname);
    }

    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        // Enum values can be converted to scalar since they are based on scalar types
        return true;
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return true;
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const;

    DLLLOCAL const char* getPathImpl() const {
        return pname.c_str();
    }
};

class QoreEnumOrNothingTypeInfo : public QoreEnumTypeInfo {
public:
    DLLLOCAL QoreEnumOrNothingTypeInfo(const QoreEnumDecl* ed, const char* name, const char* path);

protected:
    DLLLOCAL virtual void getThisTypeImpl(QoreString& str) const {
        str.sprintf("enum<%s> or no value (NOTHING)", getFirstAcceptSpec().spec.getEnum()->getName());
    }

    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return false;
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return false;
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
        return QoreValue();
    }
};

class QoreComplexHashTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreComplexHashTypeInfo(const QoreTypeInfo* vti)
        : QoreTypeInfo(q_accept_vec_t {{QoreComplexHashTypeSpec(vti), nullptr, true}},
            q_return_vec_t {{QoreComplexHashTypeSpec(vti), true}},
            vti == autoTypeInfo
                ? QoreString("hash<auto>")
                : QoreStringMaker("hash<string, %s>", QoreTypeInfo::getName(vti))) {
        if (vti == autoTypeInfo) {
            pname = tname;
        } else {
            pname = QoreStringMaker("hash<string, %s>", QoreTypeInfo::getPath(vti));
        }
    }

protected:
    QoreString pname;

    DLLLOCAL QoreComplexHashTypeInfo(const q_accept_vec_t&& a_vec, const q_return_vec_t&& r_vec,
            const QoreString& tname) : QoreTypeInfo(std::move(a_vec), std::move(r_vec), tname) {
    }

    DLLLOCAL virtual void getThisTypeImpl(QoreString& str) const {
        qore_string_private::get(str)->concat(&tname);
    }

    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return false;
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return true;
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
        return new QoreHashNode(getFirstAcceptSpec().spec.getComplexHash());
    }

    DLLLOCAL const char* getPathImpl() const {
        assert(!pname.empty());
        return pname.c_str();
    }
};

class QoreComplexHashOrNothingTypeInfo : public QoreComplexHashTypeInfo {
public:
    DLLLOCAL QoreComplexHashOrNothingTypeInfo(const QoreTypeInfo* vti) : QoreComplexHashTypeInfo(q_accept_vec_t {
            {QoreComplexHashTypeSpec(vti), nullptr, true},
            {NT_NOTHING, nullptr},
            {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }},
            }, q_return_vec_t {{QoreComplexHashTypeSpec(vti)}, {NT_NOTHING}},
            vti == autoTypeInfo
                ? QoreString("*hash<auto>")
                : QoreStringMaker("*hash<string, %s>", QoreTypeInfo::getName(vti))) {
        if (vti == autoTypeInfo) {
            pname = tname;
        } else {
            pname = QoreStringMaker("*hash<string, %s>", QoreTypeInfo::getPath(vti));
        }
    }

protected:
    DLLLOCAL virtual void getThisTypeImpl(QoreString& str) const {
        str.sprintf("hash<string, %s> or no value (NOTHING)", QoreTypeInfo::getName(getFirstAcceptSpec().spec.getComplexHash()));
    }

    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return false;
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return false;
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
        return QoreValue();
    }
};

class QoreAutoHashTypeInfo : public QoreComplexHashTypeInfo {
public:
   DLLLOCAL QoreAutoHashTypeInfo() : QoreComplexHashTypeInfo(autoTypeInfo) {
   }
};

class QoreAutoHashOrNothingTypeInfo : public QoreComplexHashOrNothingTypeInfo {
public:
   DLLLOCAL QoreAutoHashOrNothingTypeInfo() : QoreComplexHashOrNothingTypeInfo(autoTypeInfo) {
   }
};

// Marker types for hash<auto!> - same as hash<auto> but signals no type narrowing
class QoreAutoNoNarrowHashTypeInfo : public QoreComplexHashTypeInfo {
public:
   DLLLOCAL QoreAutoNoNarrowHashTypeInfo() : QoreComplexHashTypeInfo(autoNoNarrowTypeInfo) {
   }
};

class QoreAutoNoNarrowHashOrNothingTypeInfo : public QoreComplexHashOrNothingTypeInfo {
public:
   DLLLOCAL QoreAutoNoNarrowHashOrNothingTypeInfo() : QoreComplexHashOrNothingTypeInfo(autoNoNarrowTypeInfo) {
   }
};

class QoreComplexListTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreComplexListTypeInfo(const QoreTypeInfo* vti)
        : QoreTypeInfo(q_accept_vec_t {{QoreComplexListTypeSpec(vti), nullptr, true}},
            q_return_vec_t {{QoreComplexListTypeSpec(vti), true}},
            QoreStringMaker("list<%s>", QoreTypeInfo::getName(vti))) {
        assert(vti);
        pname = QoreStringMaker("list<%s>", QoreTypeInfo::getPath(vti));
    }

protected:
    QoreString pname;

    DLLLOCAL QoreComplexListTypeInfo(const q_accept_vec_t&& a_vec, const q_return_vec_t&& r_vec,
            const QoreString& tname) : QoreTypeInfo(std::move(a_vec), std::move(r_vec), tname) {
    }

    DLLLOCAL virtual void getThisTypeImpl(QoreString& str) const {
        qore_string_private::get(str)->concat(&tname);
    }

    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return false;
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return true;
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
        return new QoreListNode(getFirstAcceptSpec().spec.getComplexList());
    }

    DLLLOCAL const char* getPathImpl() const {
        assert(!pname.empty());
        return pname.c_str();
    }
};

class QoreComplexListOrNothingTypeInfo : public QoreComplexListTypeInfo {
public:
    DLLLOCAL QoreComplexListOrNothingTypeInfo(const QoreTypeInfo* vti) : QoreComplexListTypeInfo(q_accept_vec_t {
            {QoreComplexListTypeSpec(vti), nullptr},
            {NT_NOTHING, nullptr},
            {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }},
            }, q_return_vec_t {{QoreComplexListTypeSpec(vti)}, {NT_NOTHING}},
            QoreStringMaker("*list<%s>", QoreTypeInfo::getName(vti))) {
        assert(vti);
        pname = QoreStringMaker("*list<%s>", QoreTypeInfo::getPath(vti));
    }

protected:
    DLLLOCAL QoreComplexListOrNothingTypeInfo(const q_accept_vec_t&& a_vec, const q_return_vec_t&& r_vec,
            const QoreString& tname) : QoreComplexListTypeInfo(std::move(a_vec), std::move(r_vec), tname) {
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return false;
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
        return QoreValue();
    }
};

class QoreAutoListTypeInfo : public QoreComplexListTypeInfo {
public:
    DLLLOCAL QoreAutoListTypeInfo() : QoreComplexListTypeInfo(autoTypeInfo) {
    }
};

class QoreAutoListOrNothingTypeInfo : public QoreComplexListOrNothingTypeInfo {
public:
    DLLLOCAL QoreAutoListOrNothingTypeInfo() : QoreComplexListOrNothingTypeInfo(autoTypeInfo) {
    }
};

// Marker types for list<auto!> - same as list<auto> but signals no type narrowing
class QoreAutoNoNarrowListTypeInfo : public QoreComplexListTypeInfo {
public:
    DLLLOCAL QoreAutoNoNarrowListTypeInfo() : QoreComplexListTypeInfo(autoNoNarrowTypeInfo) {
    }
};

class QoreAutoNoNarrowListOrNothingTypeInfo : public QoreComplexListOrNothingTypeInfo {
public:
    DLLLOCAL QoreAutoNoNarrowListOrNothingTypeInfo() : QoreComplexListOrNothingTypeInfo(autoNoNarrowTypeInfo) {
    }
};

class QoreComplexSoftListTypeInfo : public QoreComplexListTypeInfo {
public:
    DLLLOCAL QoreComplexSoftListTypeInfo(const QoreTypeInfo* vti);

protected:
    DLLLOCAL QoreComplexSoftListTypeInfo(const q_accept_vec_t&& a_vec, const q_return_vec_t&& r_vec,
            const QoreString& tname)
            : QoreComplexListTypeInfo(std::move(a_vec), std::move(r_vec), tname) {
    }
};

class QoreComplexSoftListOrNothingTypeInfo : public QoreComplexListOrNothingTypeInfo {
public:
    DLLLOCAL QoreComplexSoftListOrNothingTypeInfo(const QoreTypeInfo* vti);

protected:
    DLLLOCAL QoreComplexSoftListOrNothingTypeInfo(const q_accept_vec_t&& a_vec, const q_return_vec_t&& r_vec,
            const QoreString& tname)
            : QoreComplexListOrNothingTypeInfo(std::move(a_vec), std::move(r_vec), tname) {
    }
};

class QoreHardReferenceTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreHardReferenceTypeInfo()
            : QoreTypeInfo(q_accept_vec_t {
                    {QoreHardReferenceTypeSpec(), nullptr, true},
            },
            q_return_vec_t {
                {QoreHardReferenceTypeSpec(), true},
            },
            "reference") {
    }

protected:
    DLLLOCAL virtual void getThisTypeImpl(QoreString& str) const {
        qore_string_private::get(str)->concat(&tname);
    }

    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return false;
    }
};

class QoreHardReferenceOrNothingTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreHardReferenceOrNothingTypeInfo()
        : QoreTypeInfo(q_accept_vec_t {
                {QoreHardReferenceTypeSpec(), nullptr, true},
                {NT_NOTHING, nullptr},
                {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }},
            },
            q_return_vec_t {
                {QoreHardReferenceTypeSpec(), true},
                {NT_NOTHING},
            },
            "*reference") {
    }

protected:
    DLLLOCAL virtual void getThisTypeImpl(QoreString& str) const {
        qore_string_private::get(str)->concat(&tname);
    }

    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return false;
    }
};

class QoreComplexHardReferenceTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreComplexHardReferenceTypeInfo(const QoreTypeInfo* vti)
        : QoreTypeInfo(q_accept_vec_t {
                {QoreComplexHardReferenceTypeSpec(vti), nullptr, true},
            },
            q_return_vec_t {
                {QoreComplexHardReferenceTypeSpec(vti), true},
            },
            QoreStringMaker("reference<%s>", QoreTypeInfo::getName(vti))) {
        assert(vti);
        pname = QoreStringMaker("reference<%s>", QoreTypeInfo::getPath(vti));
    }

protected:
    QoreString pname;

    DLLLOCAL virtual void getThisTypeImpl(QoreString& str) const {
        qore_string_private::get(str)->concat(&tname);
    }

    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return false;
    }

    DLLLOCAL const char* getPathImpl() const {
        assert(!pname.empty());
        return pname.c_str();
    }
};

class QoreComplexHardReferenceOrNothingTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreComplexHardReferenceOrNothingTypeInfo(const QoreTypeInfo* vti)
        : QoreTypeInfo(q_accept_vec_t {
                {QoreComplexHardReferenceTypeSpec(vti), nullptr, true},
                {NT_NOTHING, nullptr},
                {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }},
            },
            q_return_vec_t {
                {QoreComplexHardReferenceTypeSpec(vti), true},
                {NT_NOTHING},
            },
            QoreStringMaker("*reference<%s>", QoreTypeInfo::getName(vti))) {
        assert(vti);
        pname = QoreStringMaker("*reference<%s>", QoreTypeInfo::getPath(vti));
    }

protected:
    QoreString pname;

    DLLLOCAL virtual void getThisTypeImpl(QoreString& str) const {
        qore_string_private::get(str)->concat(&tname);
    }

    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return false;
    }

    DLLLOCAL const char* getPathImpl() const {
        assert(!pname.empty());
        return pname.c_str();
    }
};

class QoreComplexReferenceTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreComplexReferenceTypeInfo(const QoreTypeInfo* vti)
        : QoreTypeInfo(q_accept_vec_t {{QoreComplexReferenceTypeSpec(vti), nullptr, true}},
            q_return_vec_t {{QoreComplexReferenceTypeSpec(vti), true}},
            QoreStringMaker("reference<%s>", QoreTypeInfo::getName(vti))) {
        assert(vti);
        pname = QoreStringMaker("reference<%s>", QoreTypeInfo::getPath(vti));
    }

    DLLLOCAL ~QoreComplexReferenceTypeInfo() {
        delete ref_type;
    }

    DLLLOCAL const QoreTypeInfo* getHardReference() const {
        if (ref_type) {
            return ref_type;
        }

        AutoLocker al(ref_lock);
        // check again in the lock
        if (ref_type) {
            return ref_type;
        }

        return ref_type = getHardReferenceImpl();
    }

protected:
    QoreString pname;
    mutable QoreThreadLock ref_lock;
    mutable const QoreTypeInfo* ref_type = nullptr;

    DLLLOCAL QoreComplexReferenceTypeInfo(const q_accept_vec_t&& a_vec, const q_return_vec_t&& r_vec,
        const QoreString& tname) : QoreTypeInfo(std::move(a_vec), std::move(r_vec), tname) {
    }

    DLLLOCAL virtual void getThisTypeImpl(QoreString& str) const {
        qore_string_private::get(str)->concat(&tname);
    }

    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return false;
    }

    DLLLOCAL const char* getPathImpl() const {
        assert(!pname.empty());
        return pname.c_str();
    }

    DLLLOCAL virtual const QoreTypeInfo* getHardReferenceImpl() const {
        return new QoreComplexHardReferenceTypeInfo(getFirstAcceptSpec().spec.getComplexReference());
    }
};

class QoreComplexReferenceOrNothingTypeInfo : public QoreComplexReferenceTypeInfo {
public:
    DLLLOCAL QoreComplexReferenceOrNothingTypeInfo(const QoreTypeInfo* vti)
        : QoreComplexReferenceTypeInfo(q_accept_vec_t {
                {QoreComplexReferenceTypeSpec(vti), nullptr, true},
                {NT_NOTHING, nullptr},
                {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }},
            },
            q_return_vec_t {{QoreComplexReferenceTypeSpec(vti)}, {NT_NOTHING}},
            QoreStringMaker("*reference<%s>", QoreTypeInfo::getName(vti))) {
        assert(vti);
        pname = QoreStringMaker("*reference<%s>", QoreTypeInfo::getPath(vti));
    }

protected:
    DLLLOCAL virtual void getThisTypeImpl(QoreString& str) const {
        str.sprintf("reference<%s> or no value (NOTHING)", QoreTypeInfo::getName(getFirstAcceptSpec().spec.getComplexReference()));
    }

    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return false;
    }

    DLLLOCAL virtual const QoreTypeInfo* getHardReferenceImpl() const {
        return new QoreComplexHardReferenceOrNothingTypeInfo(getFirstAcceptSpec().spec.getComplexReference());
    }
};

class QoreBaseTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreBaseTypeInfo(const char* name, qore_type_t t)
            : QoreTypeInfo(name, q_accept_vec_t {{t, nullptr, true}}, q_return_vec_t {{t, true}}) {
    }

protected:
    DLLLOCAL QoreBaseTypeInfo(const char* name, q_accept_vec_t&& a_vec, q_return_vec_t&& r_vec)
            : QoreTypeInfo(name, std::move(a_vec), std::move(r_vec)) {
    }
};

class QoreBaseOrNothingTypeInfo : public QoreBaseTypeInfo {
public:
    DLLLOCAL QoreBaseOrNothingTypeInfo(const char* name, qore_type_t t) : QoreBaseTypeInfo(name, q_accept_vec_t {
            {t, nullptr},
            {NT_NOTHING, nullptr},
            {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }},
        },
        q_return_vec_t {{t}, {NT_NOTHING}}) {
    }

protected:
    DLLLOCAL QoreBaseOrNothingTypeInfo(const char* name, q_accept_vec_t&& a_vec, q_return_vec_t&& r_vec)
            : QoreBaseTypeInfo(name, std::move(a_vec), std::move(r_vec)) {
    }
};

class QoreBaseConvertTypeInfo : public QoreBaseTypeInfo {
public:
    DLLLOCAL QoreBaseConvertTypeInfo(const char* name, qore_type_t qt) : QoreBaseTypeInfo(name, qt) {
    }

protected:
    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return true;
    }
};

class QoreBaseOrNothingConvertTypeInfo : public QoreBaseOrNothingTypeInfo {
public:
    DLLLOCAL QoreBaseOrNothingConvertTypeInfo(const char* name, qore_type_t qt) : QoreBaseOrNothingTypeInfo(name, qt) {
    }

protected:
    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return true;
    }
};

class QoreBaseNoConvertTypeInfo : public QoreBaseTypeInfo {
public:
    DLLLOCAL QoreBaseNoConvertTypeInfo(const char* name, qore_type_t qt) : QoreBaseTypeInfo(name, qt) {
    }

protected:
    DLLLOCAL QoreBaseNoConvertTypeInfo(const char* name, q_accept_vec_t&& a_vec, q_return_vec_t&& r_vec)
            : QoreBaseTypeInfo(name, std::move(a_vec), std::move(r_vec)) {
    }

    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return false;
    }
};

class QoreObjectTypeInfo : public QoreBaseTypeInfo {
public:
    DLLLOCAL QoreObjectTypeInfo() : QoreBaseTypeInfo("object", NT_OBJECT) {
    }

protected:
    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return false;
    }
};

class QoreBaseOrNothingNoConvertTypeInfo : public QoreBaseOrNothingTypeInfo {
public:
    DLLLOCAL QoreBaseOrNothingNoConvertTypeInfo(const char* name, qore_type_t qt) : QoreBaseOrNothingTypeInfo(name, qt) {
    }

protected:
    DLLLOCAL QoreBaseOrNothingNoConvertTypeInfo(const char* name, q_accept_vec_t&& a_vec, q_return_vec_t&& r_vec)
            : QoreBaseOrNothingTypeInfo(name, std::move(a_vec), std::move(r_vec)) {
    }

    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return false;
    }
};

class QoreBigIntTypeInfo : public QoreBaseConvertTypeInfo {
public:
    DLLLOCAL QoreBigIntTypeInfo() : QoreBaseConvertTypeInfo("int", NT_INT) {
    }

protected:
    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return true;
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
        return 0LL;
    }
};

class QoreBigIntOrNothingTypeInfo : public QoreBaseOrNothingConvertTypeInfo {
public:
    DLLLOCAL QoreBigIntOrNothingTypeInfo() : QoreBaseOrNothingConvertTypeInfo("*int", NT_INT) {
    }

protected:
    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }
};

class QoreStringTypeInfo : public QoreBaseConvertTypeInfo {
public:
    DLLLOCAL QoreStringTypeInfo() : QoreBaseConvertTypeInfo("string", NT_STRING) {
    }

protected:
    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return true;
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
        return NullString->refSelf();
    }
};

class QoreStringOrNothingTypeInfo : public QoreBaseOrNothingConvertTypeInfo {
public:
   DLLLOCAL QoreStringOrNothingTypeInfo() : QoreBaseOrNothingConvertTypeInfo("*string", NT_STRING) {
   }

protected:
   // returns true if this type could contain an object or a closure
   DLLLOCAL virtual bool needsScanImpl() const {
      return false;
   }
};

class QoreBoolTypeInfo : public QoreBaseConvertTypeInfo {
public:
    DLLLOCAL QoreBoolTypeInfo() : QoreBaseConvertTypeInfo("bool", NT_BOOLEAN) {
    }

protected:
    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return true;
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
        return false;
    }
};

class QoreBoolOrNothingTypeInfo : public QoreBaseOrNothingConvertTypeInfo {
public:
    DLLLOCAL QoreBoolOrNothingTypeInfo() : QoreBaseOrNothingConvertTypeInfo("*bool", NT_BOOLEAN) {
    }

protected:
    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }
};

class QoreBinaryTypeInfo : public QoreBaseNoConvertTypeInfo {
public:
    DLLLOCAL QoreBinaryTypeInfo() : QoreBaseNoConvertTypeInfo("binary", NT_BINARY) {
    }

protected:
    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return true;
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
        return new BinaryNode;
    }
};

class QoreBinaryOrNothingTypeInfo : public QoreBaseOrNothingNoConvertTypeInfo {
public:
    DLLLOCAL QoreBinaryOrNothingTypeInfo() : QoreBaseOrNothingNoConvertTypeInfo("*binary", NT_BINARY) {
    }

protected:
    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }
};

class QoreHexBinaryTypeInfo : public QoreBaseNoConvertTypeInfo {
public:
    DLLLOCAL QoreHexBinaryTypeInfo() : QoreBaseNoConvertTypeInfo("hexbinary", q_accept_vec_t {
            {NT_BINARY, nullptr, true},
            {NT_STRING, [] (QoreValue& n, ExceptionSink* xsink) {
                    discard(n.assign(n.get<const QoreStringNode>()->parseHex(xsink)), xsink);
                }
            },
            }, q_return_vec_t {{NT_BINARY, true}}) {
    }

protected:
    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return true;
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
        return new BinaryNode();
    }
};

class QoreHexBinaryOrNothingTypeInfo : public QoreBaseOrNothingNoConvertTypeInfo {
public:
    DLLLOCAL QoreHexBinaryOrNothingTypeInfo() : QoreBaseOrNothingNoConvertTypeInfo("*hexbinary", q_accept_vec_t {
            {NT_BINARY, nullptr, true},
            {NT_STRING, [] (QoreValue& n, ExceptionSink* xsink) {
                    discard(n.assign(n.get<const QoreStringNode>()->parseHex(xsink)), xsink);
                }
            },
            {NT_NOTHING, nullptr},
            {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }},
            }, q_return_vec_t {{NT_HASH}, {NT_NOTHING}}) {
    }

protected:
    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }
};

class QoreBase64BinaryTypeInfo : public QoreBaseNoConvertTypeInfo {
public:
    DLLLOCAL QoreBase64BinaryTypeInfo() : QoreBaseNoConvertTypeInfo("base64binary", q_accept_vec_t {
            {NT_BINARY, nullptr, true},
            {NT_STRING, [] (QoreValue& n, ExceptionSink* xsink) {
                    discard(n.assign(n.get<const QoreStringNode>()->parseBase64(xsink)), xsink);
                }
            },
            }, q_return_vec_t {{NT_BINARY, true}}) {
    }

protected:
    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return true;
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
        return new BinaryNode();
    }
};

class QoreBase64BinaryOrNothingTypeInfo : public QoreBaseOrNothingNoConvertTypeInfo {
public:
    DLLLOCAL QoreBase64BinaryOrNothingTypeInfo() : QoreBaseOrNothingNoConvertTypeInfo("*base64binary", q_accept_vec_t {
            {NT_BINARY, nullptr, true},
            {NT_STRING, [] (QoreValue& n, ExceptionSink* xsink) {
                    discard(n.assign(n.get<const QoreStringNode>()->parseBase64(xsink)), xsink);
                }
            },
            {NT_NOTHING, nullptr},
            {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }},
            }, q_return_vec_t {{NT_HASH}, {NT_NOTHING}}) {
    }

protected:
    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }
};

class QoreBase64UrlBinaryTypeInfo : public QoreBaseNoConvertTypeInfo {
public:
    DLLLOCAL QoreBase64UrlBinaryTypeInfo() : QoreBaseNoConvertTypeInfo("base64urlbinary", q_accept_vec_t {
            {NT_BINARY, nullptr, true},
            {NT_STRING, [] (QoreValue& n, ExceptionSink* xsink) {
                    discard(n.assign(n.get<const QoreStringNode>()->parseBase64Url(xsink)), xsink);
                }
            },
            }, q_return_vec_t {{NT_BINARY, true}}) {
    }

protected:
    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return true;
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
        return new BinaryNode();
    }
};

class QoreBase64UrlBinaryOrNothingTypeInfo : public QoreBaseOrNothingNoConvertTypeInfo {
public:
    DLLLOCAL QoreBase64UrlBinaryOrNothingTypeInfo() : QoreBaseOrNothingNoConvertTypeInfo("*base64urlbinary", q_accept_vec_t {
            {NT_BINARY, nullptr, true},
            {NT_STRING, [] (QoreValue& n, ExceptionSink* xsink) {
                    discard(n.assign(n.get<const QoreStringNode>()->parseBase64Url(xsink)), xsink);
                }
            },
            {NT_NOTHING, nullptr},
            {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }},
            }, q_return_vec_t {{NT_HASH}, {NT_NOTHING}}) {
    }

protected:
    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }
};

class QoreObjectOrNothingTypeInfo : public QoreBaseOrNothingNoConvertTypeInfo {
public:
    DLLLOCAL QoreObjectOrNothingTypeInfo() : QoreBaseOrNothingNoConvertTypeInfo("*object", NT_OBJECT) {
    }
};

class QoreDateTypeInfo : public QoreBaseConvertTypeInfo {
public:
    DLLLOCAL QoreDateTypeInfo() : QoreBaseConvertTypeInfo("date", NT_DATE) {
    }

protected:
    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return true;
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
        return ZeroDate->refSelf();
    }
};

class QoreDateOrNothingTypeInfo : public QoreBaseOrNothingConvertTypeInfo {
public:
    DLLLOCAL QoreDateOrNothingTypeInfo() : QoreBaseOrNothingConvertTypeInfo("*date", NT_DATE) {
    }

protected:
    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }
};

class QoreAbsoluteDateTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreAbsoluteDateTypeInfo() : QoreTypeInfo("date<absolute>", q_accept_vec_t {
            {NT_DATE, [] (QoreValue& n, ExceptionSink* xsink) { validate_date_variant(n, xsink, false, "date<absolute>"); }, true},
        }, q_return_vec_t {{NT_DATE, true}}) {
    }

protected:
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return true;
    }

    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return true;
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
        return ZeroDate->refSelf();
    }
};

class QoreAbsoluteDateOrNothingTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreAbsoluteDateOrNothingTypeInfo() : QoreTypeInfo("*date<absolute>", q_accept_vec_t {
            {NT_DATE, [] (QoreValue& n, ExceptionSink* xsink) { validate_date_variant(n, xsink, false, "date<absolute>"); }},
            {NT_NOTHING, nullptr},
            {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }},
        }, q_return_vec_t {{NT_DATE}, {NT_NOTHING}}) {
    }

protected:
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return true;
    }

    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }
};

class QoreRelativeDateTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreRelativeDateTypeInfo() : QoreTypeInfo("date<relative>", q_accept_vec_t {
            {NT_DATE, [] (QoreValue& n, ExceptionSink* xsink) { validate_date_variant(n, xsink, true, "date<relative>"); }, true},
        }, q_return_vec_t {{NT_DATE, true}}) {
    }

protected:
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return true;
    }

    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return true;
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
        return new DateTimeNode(true);
    }
};

class QoreRelativeDateOrNothingTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreRelativeDateOrNothingTypeInfo() : QoreTypeInfo("*date<relative>", q_accept_vec_t {
            {NT_DATE, [] (QoreValue& n, ExceptionSink* xsink) { validate_date_variant(n, xsink, true, "date<relative>"); }},
            {NT_NOTHING, nullptr},
            {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }},
        }, q_return_vec_t {{NT_DATE}, {NT_NOTHING}}) {
    }

protected:
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return true;
    }

    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }
};

DLLLOCAL void map_get_plain_hash(QoreValue&, ExceptionSink*);
DLLLOCAL void map_get_plain_hash_lvalue(QoreValue&, ExceptionSink*, LValueHelper*);

class QoreHashTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreHashTypeInfo() : QoreTypeInfo("hash", q_accept_vec_t {
            {NT_HASH, map_get_plain_hash, true}},
        q_return_vec_t {{NT_HASH, true}}) {
    }

protected:
    DLLLOCAL QoreHashTypeInfo(const char* name, const q_accept_vec_t&& a_vec, const q_return_vec_t&& r_vec) :
        QoreTypeInfo(name, std::move(a_vec), std::move(r_vec)) {
    }

    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return false;
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return true;
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
        return emptyHash->hashRefSelf();
    }
};

class QoreEmptyHashTypeInfo : public QoreHashTypeInfo {
public:
    DLLLOCAL QoreEmptyHashTypeInfo() : QoreHashTypeInfo("hash", q_accept_vec_t {
            {NT_HASH, map_get_plain_hash, true},
        },
        q_return_vec_t {{QoreEmptyHashTypeSpec(), true}}) {
    }
};

class QoreHashOrNothingTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreHashOrNothingTypeInfo() : QoreTypeInfo("*hash", q_accept_vec_t {
            {NT_HASH, map_get_plain_hash},
            {NT_NOTHING, nullptr},
            {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }},
        },
        q_return_vec_t {{NT_HASH}, {NT_NOTHING}}) {
    }

protected:
    DLLLOCAL QoreHashOrNothingTypeInfo(const char* name, const q_accept_vec_t&& a_vec, const q_return_vec_t&& r_vec) :
        QoreTypeInfo(name, std::move(a_vec), std::move(r_vec)) {
    }

    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return false;
    }
};

DLLLOCAL void map_get_plain_list(QoreValue&, ExceptionSink*);
DLLLOCAL void map_get_plain_list_lvalue(QoreValue&, ExceptionSink*, LValueHelper*);

class QoreListTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreListTypeInfo() : QoreTypeInfo("list",
        q_accept_vec_t {
            {NT_LIST, map_get_plain_list, true},
        },
        q_return_vec_t {{NT_LIST, true}}) {
    }

protected:
    DLLLOCAL QoreListTypeInfo(const char* name, const q_accept_vec_t&& a_vec, const q_return_vec_t&& r_vec) :
        QoreTypeInfo(name, std::move(a_vec), std::move(r_vec)) {
    }

    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return false;
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return true;
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
        return emptyList->listRefSelf();
    }
};

class QoreEmptyListTypeInfo : public QoreListTypeInfo {
public:
    DLLLOCAL QoreEmptyListTypeInfo() : QoreListTypeInfo("list", q_accept_vec_t {
            {NT_LIST, nullptr, true},
        },
        q_return_vec_t {{QoreEmptyListTypeSpec(), true}}) {
    }
};

class QoreListOrNothingTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreListOrNothingTypeInfo() : QoreTypeInfo("*list",
        q_accept_vec_t {
            {NT_LIST, map_get_plain_list},
            {NT_NOTHING, nullptr},
            {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }},
        },
        q_return_vec_t {{NT_LIST}, {NT_NOTHING}}) {
    }

protected:
    DLLLOCAL QoreListOrNothingTypeInfo(const char* name, const q_accept_vec_t&& a_vec, const q_return_vec_t&& r_vec) :
        QoreTypeInfo(name, std::move(a_vec), std::move(r_vec)) {
    }

    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return false;
    }
};

class QoreNothingTypeInfo : public QoreBaseNoConvertTypeInfo {
public:
    DLLLOCAL QoreNothingTypeInfo() : QoreBaseNoConvertTypeInfo("nothing", NT_NOTHING) {
    }
};

class QoreNullTypeInfo : public QoreBaseNoConvertTypeInfo {
public:
    DLLLOCAL QoreNullTypeInfo() : QoreBaseNoConvertTypeInfo("null", NT_NULL) {
    }

protected:
    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return true;
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
        return &Null;
    }
};

class QoreNullOrNothingTypeInfo : public QoreBaseOrNothingNoConvertTypeInfo {
public:
    DLLLOCAL QoreNullOrNothingTypeInfo() : QoreBaseOrNothingNoConvertTypeInfo("*null", NT_NULL) {
    }

protected:
    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }
};

class QoreClosureTypeInfo : public QoreBaseNoConvertTypeInfo {
public:
    DLLLOCAL QoreClosureTypeInfo() : QoreBaseNoConvertTypeInfo("closure", NT_RUNTIME_CLOSURE) {
    }
};

class QoreClosureOrNothingTypeInfo : public QoreBaseOrNothingNoConvertTypeInfo {
public:
    DLLLOCAL QoreClosureOrNothingTypeInfo() : QoreBaseOrNothingNoConvertTypeInfo("*closure", NT_RUNTIME_CLOSURE) {
    }
};

class QoreCallReferenceTypeInfo : public QoreBaseNoConvertTypeInfo {
public:
    DLLLOCAL QoreCallReferenceTypeInfo() : QoreBaseNoConvertTypeInfo("callref", NT_FUNCREF) {
    }

protected:
    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }
};

class QoreCallReferenceOrNothingTypeInfo : public QoreBaseOrNothingNoConvertTypeInfo {
public:
    DLLLOCAL QoreCallReferenceOrNothingTypeInfo() : QoreBaseOrNothingNoConvertTypeInfo("*callref", NT_FUNCREF) {
    }

protected:
    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }
};

class QoreReferenceTypeInfo : public QoreBaseNoConvertTypeInfo {
public:
    DLLLOCAL QoreReferenceTypeInfo() : QoreBaseNoConvertTypeInfo("reference", NT_REFERENCE) {
    }

protected:
    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }
};

class QoreReferenceOrNothingTypeInfo : public QoreBaseOrNothingNoConvertTypeInfo {
public:
    DLLLOCAL QoreReferenceOrNothingTypeInfo() : QoreBaseOrNothingNoConvertTypeInfo("*reference", NT_REFERENCE) {
    }

protected:
    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }
};

class QoreNumberTypeInfo : public QoreBaseTypeInfo {
public:
    DLLLOCAL QoreNumberTypeInfo() : QoreBaseTypeInfo("number", q_accept_vec_t {
            {NT_NUMBER, nullptr, true},
            {NT_FLOAT, [] (QoreValue& n, ExceptionSink* xsink) {
                discard(n.assign(new QoreNumberNode(n.getAsFloat())), xsink);
            }},
            {NT_INT, [] (QoreValue& n, ExceptionSink* xsink) {
                discard(n.assign(new QoreNumberNode(n.getAsBigInt())), xsink);
            }},
        },
        q_return_vec_t {{NT_NUMBER, true}}) {
    }

protected:
    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return true;
    }

    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return true;
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
        return ZeroNumber->refSelf();
    }
};

class QoreNumberOrNothingTypeInfo : public QoreBaseTypeInfo {
public:
    DLLLOCAL QoreNumberOrNothingTypeInfo() :
        QoreBaseTypeInfo("*number", q_accept_vec_t {
            {NT_NUMBER, nullptr},
            {NT_FLOAT, [] (QoreValue& n, ExceptionSink* xsink) {
                    discard(n.assign(new QoreNumberNode(n.getAsFloat())), xsink);
                }
            },
            {NT_INT, [] (QoreValue& n, ExceptionSink* xsink) {
                    discard(n.assign(new QoreNumberNode(n.getAsBigInt())), xsink);
                }
            },
            {NT_NOTHING, nullptr},
            {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) {
                    n.assignNothing();
                }
            },
        }, q_return_vec_t {{NT_NUMBER}, {NT_NOTHING}}) {
    }

protected:
    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return true;
    }

    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }
};

class QoreFloatTypeInfo : public QoreBaseTypeInfo {
public:
    DLLLOCAL QoreFloatTypeInfo() : QoreBaseTypeInfo("float", q_accept_vec_t {
            {NT_FLOAT, nullptr, true},
            {NT_INT, [] (QoreValue& n, ExceptionSink* xsink) {
                    discard(n.assign((double)n.getAsBigInt()), xsink);
                }
            },
        }, q_return_vec_t {{NT_FLOAT, true}}) {
    }

protected:
    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return true;
    }

    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return true;
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
        return 0.0;
    }
};

class QoreFloatOrNothingTypeInfo : public QoreBaseTypeInfo {
public:
    DLLLOCAL QoreFloatOrNothingTypeInfo() : QoreBaseTypeInfo("*float", q_accept_vec_t {
            {NT_FLOAT, nullptr},
            {NT_INT, [] (QoreValue& n, ExceptionSink* xsink) {
                    discard(n.assign((double)n.getAsBigInt()), xsink);
                }
            },
            {NT_NOTHING, nullptr},
            {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) {
                    n.assignNothing();
                }
            },
        }, q_return_vec_t {{NT_FLOAT}, {NT_NOTHING}}) {
    }

protected:
    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return true;
    }

    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }
};

class QoreCodeTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreCodeTypeInfo() : QoreTypeInfo("code", q_accept_vec_t {
            {NT_RUNTIME_CLOSURE, nullptr},
            {NT_FUNCREF, nullptr},
        }, q_return_vec_t {{NT_RUNTIME_CLOSURE}, {NT_FUNCREF}}) {
    }

protected:
    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return false;
    }

    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return true;
    }
};

class QoreCodeOrNothingTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreCodeOrNothingTypeInfo() : QoreTypeInfo("*code", q_accept_vec_t {
            {NT_RUNTIME_CLOSURE, nullptr},
            {NT_FUNCREF, nullptr},
            {NT_NOTHING, nullptr},
            {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }},
        }, q_return_vec_t {{NT_RUNTIME_CLOSURE}, {NT_FUNCREF}, {NT_NOTHING}}) {
    }

protected:
    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return false;
    }

    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return true;
    }
};

class QoreDataTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreDataTypeInfo() : QoreTypeInfo("data", q_accept_vec_t {
            {NT_STRING, nullptr},
            {NT_BINARY, nullptr},
        }, q_return_vec_t {{NT_STRING}, {NT_BINARY}}) {
    }

protected:
    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return true;
    }

    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }
};

class QoreDataOrNothingTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreDataOrNothingTypeInfo() : QoreTypeInfo("*data", q_accept_vec_t {
            {NT_STRING, nullptr},
            {NT_BINARY, nullptr},
            {NT_NOTHING, nullptr},
            {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }},
        }, q_return_vec_t {{NT_STRING}, {NT_BINARY}, {NT_NOTHING}}) {
    }

protected:
    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return true;
    }

    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }
};

class QoreSoftBigIntTypeInfo : public QoreTypeInfo {
public:
   DLLLOCAL QoreSoftBigIntTypeInfo() : QoreTypeInfo("softint", q_accept_vec_t {
         {NT_INT, nullptr, true},
         {NT_FLOAT, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsBigInt()), xsink);
            }
         },
         {NT_STRING, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsBigInt()), xsink);
            }
         },
         {NT_DATE, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsBigInt()), xsink);
            }
         },
         {NT_BOOLEAN, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsBigInt()), xsink);
            }
         },
         {NT_NUMBER, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsBigInt()), xsink);
            }
         },
      }, q_return_vec_t {{NT_INT, true}}) {
   }

protected:
   // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
   DLLLOCAL virtual bool canConvertToScalarImpl() const {
      return true;
   }

   // returns true if this type could contain an object or a closure
   DLLLOCAL virtual bool needsScanImpl() const {
      return false;
   }

   DLLLOCAL virtual bool hasDefaultValueImpl() const {
      return true;
   }

   DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
      return 0ll;
   }
};

class QoreSoftBigIntOrNothingTypeInfo : public QoreTypeInfo {
public:
   DLLLOCAL QoreSoftBigIntOrNothingTypeInfo() : QoreTypeInfo("*softint", q_accept_vec_t {
         {NT_INT, nullptr},
         {NT_FLOAT, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsBigInt()), xsink);
            }
         },
         {NT_STRING, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsBigInt()), xsink);
            }
         },
         {NT_DATE, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsBigInt()), xsink);
            }
         },
         {NT_BOOLEAN, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsBigInt()), xsink);
            }
         },
         {NT_NUMBER, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsBigInt()), xsink);
            }
         },
         {NT_NOTHING, nullptr},
         {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }},
      }, q_return_vec_t {{NT_INT}, {NT_NOTHING}}) {
   }

protected:
   // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
   DLLLOCAL virtual bool canConvertToScalarImpl() const {
      return true;
   }

   // returns true if this type could contain an object or a closure
   DLLLOCAL virtual bool needsScanImpl() const {
      return false;
   }
};

class QoreSoftFloatTypeInfo : public QoreTypeInfo {
public:
   DLLLOCAL QoreSoftFloatTypeInfo() : QoreTypeInfo("softfloat", q_accept_vec_t {
         {NT_FLOAT, nullptr, true},
         {NT_INT, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsFloat()), xsink);
            }
         },
         {NT_STRING, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsFloat()), xsink);
            }
         },
         {NT_DATE, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsFloat()), xsink);
            }
         },
         {NT_BOOLEAN, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsFloat()), xsink);
            }
         },
         {NT_NUMBER, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsFloat()), xsink);
            }
         },
      }, q_return_vec_t {{NT_FLOAT, true}}) {
   }

protected:
   // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
   DLLLOCAL virtual bool canConvertToScalarImpl() const {
      return true;
   }

   // returns true if this type could contain an object or a closure
   DLLLOCAL virtual bool needsScanImpl() const {
      return false;
   }

   DLLLOCAL virtual bool hasDefaultValueImpl() const {
      return true;
   }

   DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
      return 0.0;
   }
};

class QoreSoftFloatOrNothingTypeInfo : public QoreTypeInfo {
public:
   DLLLOCAL QoreSoftFloatOrNothingTypeInfo() : QoreTypeInfo("*softfloat", q_accept_vec_t {
         {NT_FLOAT, nullptr},
         {NT_INT, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsFloat()), xsink);
            }
         },
         {NT_STRING, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsFloat()), xsink);
            }
         },
         {NT_DATE, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsFloat()), xsink);
            }
         },
         {NT_BOOLEAN, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsFloat()), xsink);
            }
         },
         {NT_NUMBER, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsFloat()), xsink);
            }
         },
         {NT_NOTHING, nullptr},
         {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }},
      }, q_return_vec_t {{NT_FLOAT}, {NT_NOTHING}}) {
   }

protected:
   // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
   DLLLOCAL virtual bool canConvertToScalarImpl() const {
      return true;
   }

   // returns true if this type could contain an object or a closure
   DLLLOCAL virtual bool needsScanImpl() const {
      return false;
   }
};

class QoreSoftNumberTypeInfo : public QoreTypeInfo {
public:
   DLLLOCAL QoreSoftNumberTypeInfo() : QoreTypeInfo("softnumber", q_accept_vec_t {
         {NT_NUMBER, nullptr, true},
         {NT_FLOAT, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(new QoreNumberNode(n.getAsFloat())), xsink);
            }
         },
         {NT_INT, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(new QoreNumberNode(n.getAsBigInt())), xsink);
            }
         },
         {NT_STRING, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(new QoreNumberNode(n.get<const QoreStringNode>()->c_str())), xsink);
            }
         },
         {NT_DATE, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(new QoreNumberNode(n.getAsFloat())), xsink);
            }
         },
         {NT_BOOLEAN, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(new QoreNumberNode(n.getAsFloat())), xsink);
            }
         },
      }, q_return_vec_t {{NT_NUMBER, true}}) {
   }

protected:
   // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
   DLLLOCAL virtual bool canConvertToScalarImpl() const {
      return true;
   }

   // returns true if this type could contain an object or a closure
   DLLLOCAL virtual bool needsScanImpl() const {
      return false;
   }

   DLLLOCAL virtual bool hasDefaultValueImpl() const {
      return true;
   }

   DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
      return ZeroNumber->refSelf();
   }
};

class QoreSoftNumberOrNothingTypeInfo : public QoreTypeInfo {
public:
   DLLLOCAL QoreSoftNumberOrNothingTypeInfo() : QoreTypeInfo("*softnumber", q_accept_vec_t {
         {NT_NUMBER, nullptr},
         {NT_FLOAT, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(new QoreNumberNode(n.getAsFloat())), xsink);
            }
         },
         {NT_INT, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(new QoreNumberNode(n.getAsBigInt())), xsink);
            }
         },
         {NT_STRING, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(new QoreNumberNode(n.get<const QoreStringNode>()->c_str())), xsink);
            }
         },
         {NT_DATE, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(new QoreNumberNode(n.getAsFloat())), xsink);
            }
         },
         {NT_BOOLEAN, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(new QoreNumberNode(n.getAsFloat())), xsink);
            }
         },
         {NT_NOTHING, nullptr},
         {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }},
      }, q_return_vec_t {{NT_NUMBER}, {NT_NOTHING}}) {
   }

protected:
   // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
   DLLLOCAL virtual bool canConvertToScalarImpl() const {
      return true;
   }

   // returns true if this type could contain an object or a closure
   DLLLOCAL virtual bool needsScanImpl() const {
      return false;
   }
};

class QoreSoftBinaryTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreSoftBinaryTypeInfo() : QoreTypeInfo("softbinary",
        q_accept_vec_t {
            {NT_BINARY, nullptr, true},
            {NT_STRING,
                [] (QoreValue& n, ExceptionSink* xsink) {
                    const QoreStringNode* str = n.get<const QoreStringNode>();
                    SimpleRefHolder<BinaryNode> bn(new BinaryNode);
                    bn->append((const void*)str->c_str(), str->size());
                    discard(n.assign(bn.release()), xsink);
                }
            },
        }, q_return_vec_t {{NT_BINARY, true}}) {
    }

protected:
    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return false;
    }

    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return true;
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
        return new BinaryNode();
    }
};

class QoreSoftBinaryOrNothingTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreSoftBinaryOrNothingTypeInfo() : QoreTypeInfo("*softbinary",
        q_accept_vec_t {
            {NT_BINARY, nullptr, true},
            {NT_STRING,
                [] (QoreValue& n, ExceptionSink* xsink) {
                    const QoreStringNode* str = n.get<const QoreStringNode>();
                    SimpleRefHolder<BinaryNode> bn(new BinaryNode);
                    bn->append((const void*)str->c_str(), str->size());
                    discard(n.assign(bn.release()), xsink);
                }
            },
            {NT_NOTHING, nullptr},
            {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }},
        }, q_return_vec_t {{NT_BINARY}, {NT_NOTHING}}) {
    }

protected:
    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return false;
    }

    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }
};

class QoreSoftBoolTypeInfo : public QoreTypeInfo {
public:
   DLLLOCAL QoreSoftBoolTypeInfo() : QoreTypeInfo("softbool", q_accept_vec_t {
         {NT_BOOLEAN, nullptr, true},
         {NT_FLOAT, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsBool()), xsink);
            }
         },
         {NT_INT, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsBool()), xsink);
            }
         },
         {NT_STRING, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsBool()), xsink);
            }
         },
         {NT_DATE, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsBool()), xsink);
            }
         },
         {NT_NUMBER, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsBool()), xsink);
            }
         },
      }, q_return_vec_t {{NT_BOOLEAN, true}}) {
   }

protected:
   // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
   DLLLOCAL virtual bool canConvertToScalarImpl() const {
      return true;
   }

   // returns true if this type could contain an object or a closure
   DLLLOCAL virtual bool needsScanImpl() const {
      return false;
   }

   DLLLOCAL virtual bool hasDefaultValueImpl() const {
      return true;
   }

   DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
      return false;
   }
};

class QoreSoftBoolOrNothingTypeInfo : public QoreTypeInfo {
public:
   DLLLOCAL QoreSoftBoolOrNothingTypeInfo() : QoreTypeInfo("*softbool", q_accept_vec_t {
         {NT_BOOLEAN, nullptr},
         {NT_FLOAT, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsBool()), xsink);
            }
         },
         {NT_INT, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsBool()), xsink);
            }
         },
         {NT_STRING, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsBool()), xsink);
            }
         },
         {NT_DATE, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsBool()), xsink);
            }
         },
         {NT_NUMBER, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(n.getAsBool()), xsink);
            }
         },
         {NT_NOTHING, nullptr},
         {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }},
      }, q_return_vec_t {{NT_BOOLEAN}, {NT_NOTHING}}) {
   }

protected:
   // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
   DLLLOCAL virtual bool canConvertToScalarImpl() const {
      return true;
   }

   // returns true if this type could contain an object or a closure
   DLLLOCAL virtual bool needsScanImpl() const {
      return false;
   }
};

class QoreSoftStringTypeInfo : public QoreTypeInfo {
public:
   DLLLOCAL QoreSoftStringTypeInfo() : QoreTypeInfo("softstring", q_accept_vec_t {
         {NT_STRING, nullptr, true},
         {NT_BOOLEAN, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(new QoreStringNodeMaker(QLLD, n.getAsBigInt())), xsink);
            }
         },
         {NT_FLOAT, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(q_fix_decimal(new QoreStringNodeMaker("%.9g", n.getAsFloat()), 0)), xsink);
            }
         },
         {NT_INT, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(new QoreStringNodeMaker(QLLD, n.getAsBigInt())), xsink);
            }
         },
         {NT_DATE, [] (QoreValue& n, ExceptionSink* xsink) {
               QoreStringNodeValueHelper str(n.getInternalNode());
               discard(n.assign(str.getReferencedValue()), xsink);
            }
         },
         {NT_NUMBER, [] (QoreValue& n, ExceptionSink* xsink) {
               QoreStringNodeValueHelper str(n.getInternalNode());
               discard(n.assign(str.getReferencedValue()), xsink);
            }
         },
      }, q_return_vec_t {{NT_STRING, true}}) {
   }

protected:
   // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
   DLLLOCAL virtual bool canConvertToScalarImpl() const {
      return true;
   }

   // returns true if this type could contain an object or a closure
   DLLLOCAL virtual bool needsScanImpl() const {
      return false;
   }

   DLLLOCAL virtual bool hasDefaultValueImpl() const {
      return true;
   }

   DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
      return NullString->refSelf();
   }
};

class QoreSoftStringOrNothingTypeInfo : public QoreTypeInfo {
public:
   DLLLOCAL QoreSoftStringOrNothingTypeInfo() : QoreTypeInfo("*softstring", q_accept_vec_t {
         {NT_STRING, nullptr},
         {NT_BOOLEAN, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(new QoreStringNodeMaker(QLLD, n.getAsBigInt())), xsink);
            }
         },
         {NT_FLOAT, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(q_fix_decimal(new QoreStringNodeMaker("%.9g", n.getAsFloat()), 0)), xsink);
            }
         },
         {NT_INT, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(new QoreStringNodeMaker(QLLD, n.getAsBigInt())), xsink);
            }
         },
         {NT_DATE, [] (QoreValue& n, ExceptionSink* xsink) {
               QoreStringNodeValueHelper str(n.getInternalNode());
               discard(n.assign(str.getReferencedValue()), xsink);
            }
         },
         {NT_NUMBER, [] (QoreValue& n, ExceptionSink* xsink) {
               QoreStringNodeValueHelper str(n.getInternalNode());
               discard(n.assign(str.getReferencedValue()), xsink);
            }
         },
         {NT_NOTHING, nullptr},
         {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }},
      }, q_return_vec_t {{NT_STRING}, {NT_NOTHING}}) {
   }

protected:
   // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
   DLLLOCAL virtual bool canConvertToScalarImpl() const {
      return true;
   }

   // returns true if this type could contain an object or a closure
   DLLLOCAL virtual bool needsScanImpl() const {
      return false;
   }
};

class QoreSoftDateTypeInfo : public QoreTypeInfo {
public:
   DLLLOCAL QoreSoftDateTypeInfo() : QoreTypeInfo("softdate", q_accept_vec_t {
         {NT_DATE, nullptr, true},
         {NT_STRING, [] (QoreValue& n, ExceptionSink* xsink) {
               DateTimeNodeValueHelper dt(n.getInternalNode());
               discard(n.assign(dt.getReferencedValue()), xsink);
            }
         },
         {NT_BOOLEAN, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(new DateTimeNode(n.getAsBigInt())), xsink);
            }
         },
         {NT_FLOAT, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(new DateTimeNode(n.getAsBigInt())), xsink);
            }
         },
         {NT_INT, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(new DateTimeNode(n.getAsBigInt())), xsink);
            }
         },
         {NT_NUMBER, [] (QoreValue& n, ExceptionSink* xsink) {
               DateTimeNodeValueHelper dt(n.getInternalNode());
               discard(n.assign(dt.getReferencedValue()), xsink);
            }
         },
      }, q_return_vec_t {{NT_DATE, true}}) {
   }

protected:
   // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
   DLLLOCAL virtual bool canConvertToScalarImpl() const {
      return true;
   }

   // returns true if this type could contain an object or a closure
   DLLLOCAL virtual bool needsScanImpl() const {
      return false;
   }

   DLLLOCAL virtual bool hasDefaultValueImpl() const {
      return true;
   }

   DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
      return ZeroDate->refSelf();
   }
};

class QoreSoftDateOrNothingTypeInfo : public QoreTypeInfo {
public:
   DLLLOCAL QoreSoftDateOrNothingTypeInfo() : QoreTypeInfo("*softdate", q_accept_vec_t {
         {NT_DATE, nullptr},
         {NT_STRING, [] (QoreValue& n, ExceptionSink* xsink) {
               DateTimeNodeValueHelper dt(n.getInternalNode());
               discard(n.assign(dt.getReferencedValue()), xsink);
            }
         },
         {NT_BOOLEAN, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(new DateTimeNode(n.getAsBigInt())), xsink);
            }
         },
         {NT_FLOAT, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(new DateTimeNode(n.getAsBigInt())), xsink);
            }
         },
         {NT_INT, [] (QoreValue& n, ExceptionSink* xsink) {
               discard(n.assign(new DateTimeNode(n.getAsBigInt())), xsink);
            }
         },
         {NT_NUMBER, [] (QoreValue& n, ExceptionSink* xsink) {
               DateTimeNodeValueHelper dt(n.getInternalNode());
               discard(n.assign(dt.getReferencedValue()), xsink);
            }
         },
         {NT_NOTHING, nullptr},
         {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }},
      }, q_return_vec_t {{NT_DATE}, {NT_NOTHING}}) {
   }

protected:
   // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
   DLLLOCAL virtual bool canConvertToScalarImpl() const {
      return true;
   }

   // returns true if this type could contain an object or a closure
   DLLLOCAL virtual bool needsScanImpl() const {
      return false;
   }
};

class QoreSoftListTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreSoftListTypeInfo() : QoreTypeInfo("softlist", q_accept_vec_t {
            {NT_LIST, map_get_plain_list, true},
            {NT_NOTHING, [] (QoreValue& n, ExceptionSink* xsink) {
                    QoreListNode* l = new QoreListNode;
                    n.assign(l);
                }
            },
            {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) {
                    xsink->raiseException("RUNTIME-TYPE-ERROR", "soft list types do not accept NULL");
                }
            },
            {NT_ALL, [] (QoreValue& n, ExceptionSink* xsink) {
                    QoreListNode* l = new QoreListNode;
                    l->push(n, nullptr);
                    n.assign(l);
                }
            },
        }, q_return_vec_t {{NT_LIST, true}}) {
    }

protected:
    DLLLOCAL QoreSoftListTypeInfo(const char* name, const q_accept_vec_t&& a_vec, const q_return_vec_t&& r_vec) : QoreTypeInfo(name, std::move(a_vec), std::move(r_vec)) {
    }

    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return false;
    }

    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return true;
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return true;
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
        return emptyList->listRefSelf();
    }
};

class QoreSoftListOrNothingTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreSoftListOrNothingTypeInfo() : QoreTypeInfo("*softlist", q_accept_vec_t {
            {NT_LIST, map_get_plain_list},
            {NT_NOTHING, nullptr},
            {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }},
            {NT_ALL, [] (QoreValue& n, ExceptionSink* xsink) {
                    QoreListNode* l = new QoreListNode;
                    l->push(n, nullptr);
                    n.assign(l);
                }
            },
        }, q_return_vec_t {{NT_LIST}, {NT_NOTHING}}) {
    }

protected:
    DLLLOCAL QoreSoftListOrNothingTypeInfo(const char* name, const q_accept_vec_t&& a_vec, const q_return_vec_t&& r_vec) : QoreTypeInfo(name, std::move(a_vec), std::move(r_vec)) {
    }

    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return false;
    }

    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return true;
    }
};

class QoreSoftAutoListTypeInfo : public QoreComplexSoftListTypeInfo {
public:
    DLLLOCAL QoreSoftAutoListTypeInfo() : QoreComplexSoftListTypeInfo(q_accept_vec_t {
            {QoreComplexListTypeSpec(autoTypeInfo), nullptr, true},
            {NT_NOTHING, [] (QoreValue& n, ExceptionSink* xsink) {
                    QoreListNode* l = new QoreListNode(autoTypeInfo);
                    n.assign(l);
                }
            },
            {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) {
                    xsink->raiseException("RUNTIME-TYPE-ERROR", "soft list types do not accept NULL");
                }
            },
            {NT_ALL, [] (QoreValue& n, ExceptionSink* xsink) {
                    QoreListNode* l = new QoreListNode(autoTypeInfo);
                    l->push(n, nullptr);
                    n.assign(l);
                }
            },
        }, q_return_vec_t {{QoreComplexListTypeSpec(autoTypeInfo), true}}, QoreString("softlist<auto>")) {
        pname = tname;
   }
};

class QoreSoftAutoListOrNothingTypeInfo : public QoreComplexSoftListTypeInfo {
public:
    DLLLOCAL QoreSoftAutoListOrNothingTypeInfo() : QoreComplexSoftListTypeInfo(q_accept_vec_t {
            {QoreComplexListTypeSpec(autoTypeInfo), nullptr},
            {NT_NOTHING, nullptr},
            {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }},
            {NT_ALL, [] (QoreValue& n, ExceptionSink* xsink) {
                    QoreListNode* l = new QoreListNode(autoTypeInfo);
                    l->push(n, nullptr);
                    n.assign(l);
                }
            },
        }, q_return_vec_t {{QoreComplexListTypeSpec(autoTypeInfo)}, {NT_NOTHING}}, QoreString("*softlist<auto>")) {
        pname = tname;
    }
};

class QoreTimeoutTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreTimeoutTypeInfo() : QoreTypeInfo("timeout", q_accept_vec_t {
            {NT_INT, nullptr},
            {NT_DATE, [] (QoreValue& n, ExceptionSink* xsink) {
                    int64 ms = n.get<const DateTimeNode>()->getRelativeMilliseconds();
                    discard(n.assign(ms), xsink);
                }
            },
        }, q_return_vec_t {{NT_INT, true}}) {
    }

protected:
    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return true;
    }

    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return true;
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
        return 0ll;
    }
};

class QoreTimeoutOrNothingTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreTimeoutOrNothingTypeInfo() : QoreTypeInfo("*timeout", q_accept_vec_t {
            {NT_INT, nullptr},
            {NT_DATE, [] (QoreValue& n, ExceptionSink* xsink) {
                    int64 ms = n.get<const DateTimeNode>()->getRelativeMilliseconds();
                    discard(n.assign(ms), xsink);
                }
            },
            {NT_NOTHING, nullptr},
            {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) {
                    n.assignNothing();
                }
            },
        }, q_return_vec_t {{NT_INT}, {NT_NOTHING}}) {
    }

protected:
    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return true;
    }

    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }
};

class QoreIntOrFloatTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreIntOrFloatTypeInfo() : QoreTypeInfo("int|float", q_accept_vec_t {
            {NT_INT, nullptr},
            {NT_FLOAT, nullptr},
        }, q_return_vec_t {{NT_INT}, {NT_FLOAT}}) {
    }

protected:
    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return true;
    }

    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }
};

class QoreIntFloatOrNumberTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreIntFloatOrNumberTypeInfo() : QoreTypeInfo("int|float|number", q_accept_vec_t {
            {NT_INT, nullptr},
            {NT_FLOAT, nullptr},
            {NT_NUMBER, nullptr},
        }, q_return_vec_t {{NT_INT}, {NT_FLOAT}, {NT_NUMBER}}) {
    }

protected:
    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return true;
    }

    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }
};

class QoreFloatOrNumberTypeInfo : public QoreTypeInfo {
public:
    DLLLOCAL QoreFloatOrNumberTypeInfo() : QoreTypeInfo("float|number", q_accept_vec_t {
            {NT_FLOAT, nullptr},
            {NT_NUMBER, nullptr},
        }, q_return_vec_t {{NT_FLOAT}, {NT_NUMBER}}) {
    }

protected:
    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return true;
    }

    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return false;
    }
};

//! Maximum number of types allowed in a union type
constexpr size_t QORE_MAX_UNION_MEMBERS = 100;

//! Type info for explicitly declared union types (union<T1, T2, ...>)
class QoreUnionTypeInfo : public QoreTypeInfo {
public:
    //! Constructor for explicit union types
    DLLLOCAL QoreUnionTypeInfo(const q_accept_vec_t&& a_vec, const q_return_vec_t&& r_vec,
            const QoreString& name, bool or_nothing = false)
            : QoreTypeInfo(std::move(a_vec), std::move(r_vec), name), orNothing(or_nothing) {
    }

    //! Returns true if this is an or-nothing union type
    DLLLOCAL bool isOrNothing() const {
        return orNothing;
    }

    //! Returns the member types of this union
    DLLLOCAL type_vec_t getMemberTypes() const {
        type_vec_t result;
        for (const auto& rt : return_vec) {
            const QoreTypeInfo* ti = rt.spec.getBaseTypeInfo();
            // Skip NOTHING type for or-nothing unions
            if (ti && ti != nothingTypeInfo) {
                result.push_back(ti);
            }
        }
        return result;
    }

protected:
    bool orNothing;  //!< true if this union type accepts NOTHING

    DLLLOCAL virtual void getThisTypeImpl(QoreString& str) const {
        qore_string_private::get(str)->concat(&tname);
    }

    // returns true if there is no type or if the type can be converted to a scalar value, false if otherwise
    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        // a union can convert to scalar if any of its members can; at runtime, the operation
        // succeeds when the actual value is a scalar-convertible type (e.g. string in a
        // union<string, binary, InputStream>)
        for (const auto& rt : return_vec) {
            const QoreTypeInfo* ti = rt.spec.getBaseTypeInfo();
            if (ti && ti != nothingTypeInfo && QoreTypeInfo::canConvertToScalar(ti)) {
                return true;
            }
        }
        return false;
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return orNothing;  // *union<...> has a default value of NOTHING
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
        // Only *union<...> has a default value (NOTHING); regular union<...> has no default
        return QoreValue();
    }

    // returns true if this type could contain an object or a closure
    DLLLOCAL virtual bool needsScanImpl() const {
        return true;  // conservatively assume union could contain scannable types
    }
};

//! Vector of type infos for union member types
typedef std::vector<const QoreTypeInfo*> type_vec_t;

//! Creates or retrieves a cached union type for the given member types
/** @param member_types vector of member types
    @param or_nothing if true, the union type also accepts NOTHING
    @return the union type info, or nullptr on error
*/
DLLLOCAL const QoreTypeInfo* qore_get_union_type(const type_vec_t& member_types, bool or_nothing = false);

//! Creates or retrieves a cached or-nothing union type for the given member types
DLLLOCAL const QoreTypeInfo* qore_get_union_or_nothing_type(const type_vec_t& member_types);

//! Type info for typed callable types: code<ReturnType(ParamTypes...)>
/** This class represents a callable type with specified return type and parameter types.
    It supports:
    - Return type specification
    - Parameter type specification (zero or more)
    - Varargs support via "..." syntax
    - Standard variance: covariant returns, contravariant parameters
*/
class QoreComplexCodeTypeInfo : public QoreTypeInfo {
public:
    //! Constructor for typed callable types
    /** @param ret_type the return type (nullptr for nothing/void)
        @param param_types vector of parameter types
        @param varargs if true, accepts variable arguments
        @param or_nothing if true, the code type also accepts NOTHING
    */
    DLLLOCAL QoreComplexCodeTypeInfo(const QoreTypeInfo* ret_type, type_vec_t&& param_types,
            bool varargs = false, bool or_nothing = false);

    //! Returns the return type of this callable
    DLLLOCAL const QoreTypeInfo* getReturnType() const {
        return returnType;
    }

    //! Returns the parameter types of this callable
    DLLLOCAL const type_vec_t& getParamTypes() const {
        return paramTypes;
    }

    //! Returns true if this callable accepts variable arguments
    DLLLOCAL bool hasVarArgs() const {
        return varargs;
    }

    //! Returns true if this is an or-nothing callable type
    DLLLOCAL bool isOrNothing() const {
        return orNothing;
    }

    //! Checks if the given signature is compatible with this callable type
    /** Uses standard variance rules:
        - Return type: covariant (assigned callable can return more specific type)
        - Parameter types: contravariant (assigned callable can accept more general type)
        @param sig the function signature to check
        @return true if the signature is compatible, false otherwise
    */
    DLLLOCAL bool isSignatureCompatible(const class AbstractFunctionSignature* sig) const;

protected:
    const QoreTypeInfo* returnType;  //!< return type (nullptr = nothing)
    type_vec_t paramTypes;           //!< parameter types
    bool varargs;                    //!< true if accepts variable arguments
    bool orNothing;                  //!< true if accepts NOTHING
    QoreString pname;                //!< path name for type

    DLLLOCAL virtual void getThisTypeImpl(QoreString& str) const {
        qore_string_private::get(str)->concat(&tname);
    }

    DLLLOCAL virtual bool canConvertToScalarImpl() const {
        return false;
    }

    DLLLOCAL virtual bool hasDefaultValueImpl() const {
        return orNothing;
    }

    DLLLOCAL virtual QoreValue getDefaultQoreValueImpl() const {
        return QoreValue();  // NOTHING
    }

    DLLLOCAL virtual bool needsScanImpl() const {
        return true;  // closures need scanning
    }

    DLLLOCAL const char* getPathImpl() const {
        return pname.empty() ? tname.c_str() : pname.c_str();
    }
};

//! Creates or retrieves a cached typed callable type
/** @param return_type the return type (nullptr for nothing)
    @param param_types vector of parameter types
    @param varargs if true, accepts variable arguments
    @param or_nothing if true, also accepts NOTHING
    @return the typed callable type info
*/
DLLLOCAL const QoreTypeInfo* qore_get_complex_code_type(const QoreTypeInfo* return_type,
    const type_vec_t& param_types, bool varargs = false, bool or_nothing = false);

//! Creates or retrieves a cached or-nothing typed callable type
DLLLOCAL const QoreTypeInfo* qore_get_complex_code_or_nothing_type(const QoreTypeInfo* return_type,
    const type_vec_t& param_types, bool varargs = false);

//! Creates a typed callable type from a function signature
/** @param sig the function signature to create the type from
    @param or_nothing if true, also accepts NOTHING
    @return the typed callable type info
*/
DLLLOCAL const QoreTypeInfo* qore_get_complex_code_type_from_signature(const class AbstractFunctionSignature* sig,
    bool or_nothing = false);

#include "qore/intern/QoreParseTypeInfo.h"

#endif // _QORE_QORETYPEINFO_H
