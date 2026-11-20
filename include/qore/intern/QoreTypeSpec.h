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

#include <atomic>
#include <cstddef>
#include <functional>
#include <iterator>
#include <vector>
#include <qore/QoreType.h>
#include <qore/QoreClass.h>
#include <qore/TypedHashDecl.h>
#include <qore/QoreEnumDecl.h>
#include <qore/intern/LexicalSemanticPolicy.h>

// Forward declarations
class QoreTypeInfo;
class QoreParameterizedClassTypeInfo;
class QoreTypeParameterTypeInfo;
class QoreWildcardTypeInfo;
class QoreComplexBufferTypeInfo;
class LValueHelper;
class QoreHashNode;
class QoreListNode;
struct QoreAcceptSpec;

#define NO_TYPE_INFO "any"

//! A pointer from a derived type to a type or declaration that can be orphaned (issue #6419)
/** When a declaration that a derived type names is retired, the pointer is repointed to the declaration's tombstone
    (orphan()) while other threads can be reading it; the declaration is freed once every thread has passed a quiescent
    point since (see design/derived-type-retirement.md).  A read is an acquire load, which is a plain load on x86-64
    and AArch64 (\c ldar); the store is a release store.  Copies read the pointer once.
*/
template <typename T>
class QoreOrphanablePtr {
public:
    DLLLOCAL QoreOrphanablePtr(T* p = nullptr) : p(p) {
    }

    DLLLOCAL QoreOrphanablePtr(const QoreOrphanablePtr& old) : p(old.get()) {
    }

    DLLLOCAL QoreOrphanablePtr& operator=(const QoreOrphanablePtr& old) {
        p.store(old.get(), std::memory_order_relaxed);
        return *this;
    }

    DLLLOCAL T* get() const {
        return p.load(std::memory_order_acquire);
    }

    DLLLOCAL operator T*() const {
        return get();
    }

    DLLLOCAL T* operator->() const {
        return get();
    }

    //! Repoints the pointer to the tombstone of what it pointed to; the tombstone is immortal
    DLLLOCAL void orphan(T* tombstone) const {
        p.store(tombstone, std::memory_order_release);
    }

private:
    mutable std::atomic<T*> p;
};

//! A read-only view of a vector of orphanable pointers, which reads each element with an acquire load
template <typename T>
class QoreOrphanablePtrVecView {
public:
    typedef std::vector<QoreOrphanablePtr<T>> vec_t;

    class const_iterator {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef T* value_type;
        typedef std::ptrdiff_t difference_type;
        typedef T* const* pointer;
        typedef T* reference;

        DLLLOCAL const_iterator(typename vec_t::const_iterator i) : i(i) {
        }

        DLLLOCAL T* operator*() const {
            return i->get();
        }

        DLLLOCAL const_iterator& operator++() {
            ++i;
            return *this;
        }

        DLLLOCAL const_iterator operator++(int) {
            const_iterator rv = *this;
            ++i;
            return rv;
        }

        DLLLOCAL bool operator==(const const_iterator& other) const {
            return i == other.i;
        }

        DLLLOCAL bool operator!=(const const_iterator& other) const {
            return i != other.i;
        }

    private:
        typename vec_t::const_iterator i;
    };

    DLLLOCAL QoreOrphanablePtrVecView(const vec_t& v) : v(v) {
    }

    DLLLOCAL size_t size() const {
        return v.size();
    }

    DLLLOCAL bool empty() const {
        return v.empty();
    }

    DLLLOCAL T* operator[](size_t i) const {
        return v[i].get();
    }

    DLLLOCAL const_iterator begin() const {
        return const_iterator(v.begin());
    }

    DLLLOCAL const_iterator end() const {
        return const_iterator(v.end());
    }

    //! Returns a copy of the pointers, where a vector is expected
    DLLLOCAL operator std::vector<T*>() const {
        return toVector();
    }

    //! Returns a copy of the pointers
    DLLLOCAL std::vector<T*> toVector() const {
        std::vector<T*> rv;
        rv.reserve(v.size());
        for (const QoreOrphanablePtr<T>& p : v) {
            rv.push_back(p.get());
        }
        return rv;
    }

    //! Returns the orphanable elements, for orphaning
    DLLLOCAL const vec_t& getOrphanable() const {
        return v;
    }

private:
    const vec_t& v;
};

//! Makes a vector of orphanable pointers from a vector of pointers
template <typename T>
DLLLOCAL std::vector<QoreOrphanablePtr<T>> qore_orphanable_vec(const std::vector<T*>& v) {
    return std::vector<QoreOrphanablePtr<T>>(v.begin(), v.end());
}

// adds external types to global type map
DLLLOCAL void add_to_type_map(qore_type_t t, const QoreTypeInfo* typeInfo);

// internal use only
DLLLOCAL const QoreTypeInfo* qore_get_complex_hard_reference_type(const QoreTypeInfo* valueTypeInfo);
DLLLOCAL const QoreTypeInfo* qore_get_parameterized_class_type(const QoreClass* qc,
        const std::vector<const QoreTypeInfo*>& args, bool or_nothing = false);
DLLLOCAL const QoreTypeInfo* qore_get_complex_buffer_value_type(const QoreTypeInfo* typeInfo);

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
    QTS_PARAMCLASS = 12,
    QTS_TYPEPARAM = 13,
    QTS_WILDCARD = 14,
    QTS_COMPLEXBUFFER = 15,
    // Code-owned conversion descriptor; never a declared/value type or serialized AOT type.
    QTS_INITIALIZER_FILTER,
};

static constexpr unsigned QORE_TYPE_SPEC_COUNT = static_cast<unsigned>(QTS_INITIALIZER_FILTER) + 1;

typedef std::function<void (QoreValue&, ExceptionSink*)> q_type_map_t;
typedef std::function<void (QoreValue&, ExceptionSink*, LexicalSemanticPolicy)> q_type_lexical_map_t;

// returns type info for base types
DLLLOCAL const QoreTypeInfo* getTypeInfoForType(qore_type_t t);
// returns type info information for parse types (values)
DLLLOCAL const QoreTypeInfo* getTypeInfoForValue(const AbstractQoreNode* n);
// returns an "or nothing" type for the given non-or-nothing type or nullptr if not possible
DLLLOCAL const QoreTypeInfo* get_or_nothing_type(const QoreTypeInfo* typeInfo);
// returns a value (i.e. not "or nothing") type for the given type
DLLLOCAL const QoreTypeInfo* get_value_type(const QoreTypeInfo* typeInfo);
// returns T for an or-nothing type *T, otherwise the type given
DLLLOCAL const QoreTypeInfo* get_or_nothing_base_type(const QoreTypeInfo* typeInfo);

class QoreTypeSpec {
public:
    DLLLOCAL QoreTypeSpec(qore_type_t t) : typespec(QTS_TYPE) {
        // the whole union is set: the getters load its pointer before they test the kind (see getComplexList())
        u.ti = nullptr;
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

    //! returns true if this enum type (QTS_ENUM) accepts the values of the given enum: the same declaration or an
    //! equal one
    DLLLOCAL bool acceptsEnumDecl(const QoreEnumDecl* ed) const;

    DLLLOCAL bool isRuntimeBaseType(qore_type_t t) const {
        return typespec == QTS_TYPE && u.t == t;
    }

    DLLLOCAL const char* getName() const;

    DLLLOCAL const char* getTypeName() const;
    DLLLOCAL const char* getSimpleTypeName() const;

    // returns the base qore_type_t for the type specification
    DLLLOCAL qore_type_t getType() const;

    DLLLOCAL const QoreClass* getClass() const;

    DLLLOCAL const QoreParameterizedClassTypeInfo* getParameterizedClassTypeInfo() const;

    // the pointer of a spec is an atomic load, which the compiler neither moves before the test of the kind nor
    // removes: a getter whose callers usually find another kind loads it only after the test; getComplexHash() and
    // getComplexList(), whose callers usually ask hash and list types, load it first and select it without a branch
    // (issue #6419)
    DLLLOCAL const QoreTypeParameterTypeInfo* getTypeParameterTypeInfo() const {
        return typespec == QTS_TYPEPARAM ? reinterpret_cast<const QoreTypeParameterTypeInfo*>(uTi()) : nullptr;
    }

    DLLLOCAL const QoreWildcardTypeInfo* getWildcardTypeInfo() const {
        return typespec == QTS_WILDCARD ? reinterpret_cast<const QoreWildcardTypeInfo*>(uTi()) : nullptr;
    }

    DLLLOCAL const TypedHashDecl* getHashDecl() const {
        return typespec == QTS_HASHDECL ? uHd() : nullptr;
    }

    DLLLOCAL const QoreEnumDecl* getEnum() const {
        return typespec == QTS_ENUM ? uEd() : nullptr;
    }

    DLLLOCAL const QoreTypeInfo* getComplexHash() const {
        // the union is always wholly set (see QoreTypeSpec(qore_type_t))
        const QoreTypeInfo* ti = uTi();
        return typespec == QTS_COMPLEXHASH ? ti : nullptr;
    }

    DLLLOCAL const QoreTypeInfo* getComplexList() const {
        // the union is always wholly set (see QoreTypeSpec(qore_type_t))
        const QoreTypeInfo* ti = uTi();
        return typespec == QTS_COMPLEXLIST || typespec == QTS_COMPLEXSOFTLIST ? ti : nullptr;
    }

    DLLLOCAL const QoreTypeInfo* getComplexSoftList() const {
        return typespec == QTS_COMPLEXSOFTLIST ? uTi() : nullptr;
    }

    DLLLOCAL const QoreTypeInfo* getComplexReference() const {
        return (typespec == QTS_COMPLEXREF || typespec == QTS_COMPLEXHARDREF) ? uTi() : nullptr;
    }

    DLLLOCAL const QoreTypeInfo* getComplexBuffer() const {
        return typespec == QTS_COMPLEXBUFFER ? uTi() : nullptr;
    }

    //! returns the element type, if any (nullptr if not applicable)
    DLLLOCAL const QoreTypeInfo* getElementType() const {
        switch (typespec) {
            case QTS_COMPLEXHASH:
            case QTS_COMPLEXLIST:
            case QTS_COMPLEXSOFTLIST:
                return uTi();
            case QTS_COMPLEXBUFFER:
                return qore_get_complex_buffer_value_type(uTi());
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
                || typespec == QTS_COMPLEXBUFFER
                || typespec == QTS_ENUM
                || typespec == QTS_PARAMCLASS
                || typespec == QTS_TYPEPARAM
                || typespec == QTS_WILDCARD
            ;
    }

    DLLLOCAL const QoreTypeInfo* getTypeInfo() const {
        switch (typespec) {
            case QTS_HASHDECL:
                return uHd()->getTypeInfo();

            case QTS_ENUM:
                return uEd()->getTypeInfo();

            case QTS_COMPLEXLIST:
            case QTS_COMPLEXSOFTLIST:
                return qore_get_complex_list_type(uTi());

            case QTS_COMPLEXHASH:
                return qore_get_complex_hash_type(uTi());

            case QTS_COMPLEXHARDREF:
                return qore_get_complex_hard_reference_type(uTi());

            case QTS_COMPLEXREF:
                return qore_get_complex_reference_type(uTi());

            case QTS_COMPLEXBUFFER:
                return uTi();

            case QTS_CLASS:
                return uQc()->getTypeInfo();

            case QTS_PARAMCLASS:
            case QTS_TYPEPARAM:
            case QTS_WILDCARD:
            case QTS_INITIALIZER_FILTER:
                return uTi();

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
            case QTS_COMPLEXBUFFER:
                return bufferTypeInfo;
            case QTS_CLASS:
            case QTS_PARAMCLASS:
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
                return uEd()->getBaseTypeInfo();
            case QTS_TYPEPARAM:
            case QTS_WILDCARD:
            case QTS_INITIALIZER_FILTER:
                return uTi();
        }
        assert(false);
        return nullptr;
    }

    // matches a base type against this spec as an accept spec; a soft list also matches the types its element type
    // accepts, since such values are converted to a list
    DLLLOCAL qore_type_result_e matchType(qore_type_t t) const;

    // matches a base type against this spec as a return spec; a soft list always returns a list, so a nullable or
    // scalar element type does not make it return the element's base type
    DLLLOCAL qore_type_result_e matchReturnType(qore_type_t t) const;

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

    //! matches a value at runtime
    /** @param n the value
        @param exact true if a match of this type is an exact match
        @param fold if true, a typed container value whose element type this complex container type does not
        accept, but whose elements the fold loops in acceptInputComplexHash()/acceptInputComplexList() may convert
        (nested containers), is a QTI_AMBIGUOUS match; otherwise only a value that has this type matches
    */
    DLLLOCAL qore_type_result_e runtimeAcceptsValue(const QoreValue& n, bool exact, bool fold = false) const;

    //! Matches with the conversion operation's explicit source policy, including soft-list elements.
    DLLLOCAL qore_type_result_e runtimeAcceptsValue(const QoreValue& n, bool exact, bool fold,
            LexicalSemanticPolicy policy) const;

    template <typename MatchPolicy>
    DLLLOCAL qore_type_result_e runtimeAcceptsValueImpl(const QoreValue& n, bool exact, bool fold,
            MatchPolicy policy) const;

    DLLLOCAL qore_type_result_e checkMatchType(const QoreTypeSpec& t, bool& may_not_match,
            qore_type_result_e& max_result) const;

    DLLLOCAL qore_type_result_e tryMatchReferenceType(const QoreTypeSpec& t, bool& may_not_match) const;

    // returns true if there is a match or if an error has been raised
    DLLLOCAL bool acceptInput(ExceptionSink* xsink, const QoreTypeInfo& typeInfo, const QoreAcceptSpec& accept,
            const char* arg_type, bool obj, int param_num, const char* param_name, QoreValue& n,
            LValueHelper* lvhelper = nullptr) const;

    template <typename FilterPolicy>
    DLLLOCAL bool acceptInputImpl(ExceptionSink* xsink, const QoreTypeInfo& typeInfo, const QoreAcceptSpec& accept,
            const char* arg_type, bool obj, int param_num, const char* param_name, QoreValue& n,
            LValueHelper* lvhelper, FilterPolicy policy) const;

    template <typename FilterPolicy>
    DLLLOCAL bool acceptInputComplexHashImpl(ExceptionSink* xsink, const QoreTypeInfo& typeInfo,
            const char* arg_type, bool obj, int param_num, const char* param_name, QoreValue& n,
            LValueHelper* lvhelper, QoreHashNode* h, bool& err, FilterPolicy policy) const;

    template <typename FilterPolicy>
    DLLLOCAL bool acceptInputComplexListImpl(ExceptionSink* xsink, const QoreTypeInfo& typeInfo,
            const char* arg_type, bool obj, int param_num, const char* param_name, QoreValue& n,
            LValueHelper* lvhelper, QoreListNode* l, bool& err, FilterPolicy policy) const;

    DLLLOCAL bool operator==(const QoreTypeSpec& other) const;
    DLLLOCAL bool operator!=(const QoreTypeSpec& other) const;

    DLLLOCAL bool acceptInputComplexHash(ExceptionSink* xsink, const QoreTypeInfo& typeInfo, const char* arg_type,
            bool obj, int param_num, const char* param_name, QoreValue& n, LValueHelper* lvhelper, QoreHashNode* h,
            bool& err) const;

    DLLLOCAL bool acceptInputComplexList(ExceptionSink* xsink, const QoreTypeInfo& typeInfo, const char* arg_type,
            bool obj, int param_num, const char* param_name, QoreValue& n, LValueHelper* lvhelper, QoreListNode* l,
            bool& err) const;

    //! Returns the type pointer of a spec that holds one (a complex type's element type), for orphaning
    DLLLOCAL const QoreTypeInfo* getOrphanableTypeInfo() const {
        return uTi();
    }

    //! Repoints the type or declaration pointer of the spec to its tombstone (issue #6419; see QoreOrphanablePtr)
    /** Only the pointer of the spec's own kind is changed; the tombstone is immortal.  Readers read it with
        uTi(), uQc(), uHd() and uEd()
    */
    DLLLOCAL void orphanTypeInfo(const QoreTypeInfo* tombstone) const {
        __atomic_store_n(&u.ti, tombstone, __ATOMIC_RELEASE);
    }

    DLLLOCAL void orphanClass(const QoreClass* tombstone) const {
        __atomic_store_n(&u.qc, tombstone, __ATOMIC_RELEASE);
    }

    DLLLOCAL void orphanHashDecl(const TypedHashDecl* tombstone) const {
        __atomic_store_n(&u.hd, tombstone, __ATOMIC_RELEASE);
    }

    DLLLOCAL void orphanEnum(const QoreEnumDecl* tombstone) const {
        __atomic_store_n(&u.ed, tombstone, __ATOMIC_RELEASE);
    }

protected:
    q_typespec_t typespec;
    //! the type or declaration; a pointer is read with an acquire load (uTi(), uQc(), uHd(), uEd()), as a derived
    //! type's spec can be orphaned while it is read (issue #6419)
    mutable union {
        qore_type_t t;
        const QoreClass* qc;
        const TypedHashDecl* hd;
        const QoreTypeInfo* ti;
        const QoreEnumDecl* ed;
    } u;

    DLLLOCAL const QoreTypeInfo* uTi() const {
        return __atomic_load_n(&u.ti, __ATOMIC_ACQUIRE);
    }

    DLLLOCAL const QoreClass* uQc() const {
        return __atomic_load_n(&u.qc, __ATOMIC_ACQUIRE);
    }

    DLLLOCAL const TypedHashDecl* uHd() const {
        return __atomic_load_n(&u.hd, __ATOMIC_ACQUIRE);
    }

    DLLLOCAL const QoreEnumDecl* uEd() const {
        return __atomic_load_n(&u.ed, __ATOMIC_ACQUIRE);
    }

    DLLLOCAL QoreTypeSpec(const QoreTypeInfo* ti, q_typespec_t t) : typespec(t) {
        u.ti = ti;
    }

    DLLLOCAL QoreTypeSpec(qore_type_t t, q_typespec_t ts) : typespec(ts) {
        // the whole union is set (see QoreTypeSpec(qore_type_t))
        u.ti = nullptr;
        u.t = t;
    }
};

//! Internal conversion dispatch for a lexical initializer; not used for type matching.
class QoreInitializerFilterTypeSpec : public QoreTypeSpec {
public:
    DLLLOCAL explicit QoreInitializerFilterTypeSpec(const QoreTypeInfo* descriptor)
            : QoreTypeSpec(descriptor, QTS_INITIALIZER_FILTER) {
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

class QoreComplexBufferTypeSpec : public QoreTypeSpec {
public:
    DLLLOCAL QoreComplexBufferTypeSpec(const QoreTypeInfo* ti) : QoreTypeSpec(ti, QTS_COMPLEXBUFFER) {
    }
};

class QoreParameterizedClassTypeSpec : public QoreTypeSpec {
public:
    DLLLOCAL QoreParameterizedClassTypeSpec(const QoreParameterizedClassTypeInfo* ti);
};

class QoreTypeParameterTypeSpec : public QoreTypeSpec {
public:
    DLLLOCAL QoreTypeParameterTypeSpec(const QoreTypeParameterTypeInfo* ti);
};

class QoreWildcardTypeSpec : public QoreTypeSpec {
public:
    DLLLOCAL QoreWildcardTypeSpec(const QoreWildcardTypeInfo* ti);
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
    //! Optional explicit-policy conversion; the shared type never stores a particular invocation's policy.
    const q_type_lexical_map_t lexical_map;

    DLLLOCAL QoreAcceptSpec(const QoreTypeSpec&& spec, const q_type_map_t&& map, bool exact = false,
            const q_type_lexical_map_t&& lexical_map = {})
            : spec(spec), map(map), exact(exact), lexical_map(lexical_map) {
    }
};
typedef std::vector<QoreAcceptSpec> q_accept_vec_t;

template <typename T>
DLLLOCAL bool typespec_vec_compare(const T& a, const T& b);

DLLLOCAL bool accept_vec_compare(const q_accept_vec_t& a, const q_accept_vec_t& b);
DLLLOCAL bool return_vec_compare(const q_return_vec_t& a, const q_return_vec_t& b);

#endif
