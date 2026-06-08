/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreHashIterator.h

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

#ifndef _QORE_QOREHASHITERATOR_H

#define _QORE_QOREHASHITERATOR_H

extern QoreClass* QC_HASHITERATOR;
extern QoreClass* QC_HASHKEYITERATOR;
extern QoreClass* QC_HASHPAIRITERATOR;
extern QoreClass* QC_HASHREVERSEITERATOR;

DLLLOCAL QoreObject* qore_new_hash_iterator_object(QoreProgram* pgm, const QoreHashNode* h);
DLLLOCAL QoreObject* qore_new_hash_pair_iterator_object(QoreProgram* pgm, const QoreHashNode* h);
DLLLOCAL const QoreTypeInfo* qore_get_hash_iterator_value_type(const QoreTypeInfo* iterator_type_info);
DLLLOCAL const TypedHashDecl* qore_get_key_value_info_hashdecl(const QoreTypeInfo* value_type);

// the c++ object
class QoreHashIterator : public QoreIteratorBase, public ConstHashIterator {
public:
    //! Selects what nativeGetValue() yields for AbstractIteratorHelper-driven loops.
    /** Three Qore-visible classes share this priv (HashIterator,
        HashKeyIterator, HashPairIterator) and yield different things from
        getValue().  The QPP ctor for each variant calls setView() so the
        fast path returns the right shape.

        @since %Qore 3.0
    */
    enum HashView {
        VIEW_VALUE = 0,  ///< default; matches HashIterator::getValue()
        VIEW_KEY   = 1,  ///< matches HashKeyIterator::getValue()
        VIEW_PAIR  = 2,  ///< matches HashPairIterator::getValue()
    };

protected:
    // reusable hash for pair iterator performance enhancement; provides an approx 70% speed improvement
    mutable QoreHashNode* pairHash;

    const QoreTypeInfo* value_type;

    // What nativeGetValue() yields; set by the QPP ctors for Key/Pair variants.
    HashView view = VIEW_VALUE;

    DLLLOCAL virtual ~QoreHashIterator() {
        assert(!pairHash);
        assert(!h);
    }

    DLLLOCAL int checkPtr(ExceptionSink* xsink) const {
        if (!valid()) {
            xsink->raiseException("ITERATOR-ERROR", "the %s is not pointing at a valid element; make sure %s::next() returns True before calling this method", getName(), getName());
            return -1;
        }
        return 0;
    }

    DLLLOCAL static const QoreTypeInfo* resolveValueType(const QoreHashNode* h,
            const QoreTypeInfo* value_type = nullptr) {
        const QoreTypeInfo* rv = value_type ? value_type : (h ? h->getValueTypeInfo() : autoTypeInfo);
        return QoreTypeInfo::hasType(rv) ? rv : autoTypeInfo;
    }

    DLLLOCAL QoreHashIterator(QoreHashNode* h, const QoreTypeInfo* value_type = nullptr)
            : ConstHashIterator(h), pairHash(0), value_type(resolveValueType(h, value_type)) {
    }

public:
    DLLLOCAL QoreHashIterator(const QoreHashNode* h, const QoreTypeInfo* value_type = nullptr)
            : ConstHashIterator(h->hashRefSelf()), pairHash(0), value_type(resolveValueType(h, value_type)) {
    }

    DLLLOCAL QoreHashIterator(const QoreTypeInfo* value_type = autoTypeInfo)
            : ConstHashIterator(0), pairHash(0), value_type(resolveValueType(nullptr, value_type)) {
    }

    DLLLOCAL QoreHashIterator(const QoreHashIterator& old)
            : ConstHashIterator(old), pairHash(0), value_type(old.value_type) {
    }

    using AbstractPrivateData::deref;
    DLLLOCAL virtual void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (h) {
                const_cast<QoreHashNode*>(h)->deref(xsink);
#ifdef DEBUG
                h = nullptr;
#endif
            }
            if (pairHash) {
                pairHash->deref(xsink);
#ifdef DEBUG
                pairHash = nullptr;
#endif
            }
            delete this;
        }
    }

    DLLLOCAL QoreValue getReferencedValue(ExceptionSink* xsink) const {
        if (checkPtr(xsink))
            return QoreValue();
        return ConstHashIterator::getReferenced();
    }

    DLLLOCAL QoreValue getReferencedKeyValue(ExceptionSink* xsink) const {
        if (checkPtr(xsink))
            return QoreValue();
        return ConstHashIterator::getReferenced();
    }

    DLLLOCAL QoreHashNode* getReferencedValuePair(ExceptionSink* xsink) const {
        if (checkPtr(xsink))
            return nullptr;
        // create or re-use the pair hash if possible
        if (!pairHash) {
            pairHash = new QoreHashNode(qore_get_key_value_info_hashdecl(value_type), xsink);
            if (*xsink) {
                pairHash->deref(xsink);
                pairHash = nullptr;
                return nullptr;
            }
        } else if (!pairHash->is_unique()) {
            pairHash->deref(xsink);
            pairHash = new QoreHashNode(qore_get_key_value_info_hashdecl(value_type), xsink);
            if (*xsink) {
                pairHash->deref(xsink);
                pairHash = nullptr;
                return nullptr;
            }
        }
        pairHash->setKeyValue("key", new QoreStringNode(ConstHashIterator::getKey()), xsink);
        pairHash->setKeyValue("value", ConstHashIterator::getReferenced(), xsink);
        return pairHash->hashRefSelf();
    }

    DLLLOCAL QoreStringNode* getKey(ExceptionSink* xsink) const {
        if (checkPtr(xsink))
            return nullptr;
        return new QoreStringNode(ConstHashIterator::getKey());
    }

    DLLLOCAL bool empty() const {
        return !h || h->empty();
    }

    DLLLOCAL bool next() {
        if (!h)
            return false;
        return ConstHashIterator::next();
    }

    DLLLOCAL bool prev() {
        if (!h)
            return false;
        return ConstHashIterator::prev();
    }

    DLLLOCAL virtual const char* getName() const { return "HashIterator"; }

    DLLLOCAL virtual const QoreTypeInfo* getElementType() const {
        switch (view) {
            case VIEW_KEY:
                return stringTypeInfo;
            case VIEW_PAIR:
                return qore_get_key_value_info_hashdecl(value_type)->getTypeInfo();
            case VIEW_VALUE:
            default:
                return value_type;
        }
    }

    //! Sets the value shape yielded by nativeGetValue(); see @ref HashView.
    DLLLOCAL void setView(HashView v) { view = v; }

    //! Returns the value type carried by the iterator source.
    DLLLOCAL const QoreTypeInfo* getValueTypeInfo() const { return value_type; }

    // Native fast-path: branch on view to match HashIterator/HashKeyIterator/
    // HashPairIterator semantics.  All three share this priv class.
    DLLLOCAL bool supportsNativeIteration() const override { return true; }

    DLLLOCAL bool nativeNext(ExceptionSink* xsink) override {
        if (check(xsink)) {
            return false;
        }
        return next();
    }

    DLLLOCAL QoreValue nativeGetValue(ExceptionSink* xsink) override {
        if (check(xsink)) {
            return QoreValue();
        }
        switch (view) {
            case VIEW_KEY:
                return getKey(xsink);
            case VIEW_PAIR:
                return getReferencedValuePair(xsink);
            case VIEW_VALUE:
            default:
                return getReferencedValue(xsink);
        }
    }
};

// internal reverse iterator class implementation only for the getName() function - the iterators remain
// forwards and are used in the reverse sense by the Qore language class implementation below
class QoreHashReverseIterator : public QoreHashIterator {
public:
    DLLLOCAL QoreHashReverseIterator(const QoreHashNode* h, const QoreTypeInfo* value_type = nullptr)
            : QoreHashIterator(h, value_type) {
    }

    DLLLOCAL QoreHashReverseIterator(const QoreTypeInfo* value_type = autoTypeInfo) : QoreHashIterator(value_type) {
    }

    DLLLOCAL QoreHashReverseIterator(const QoreHashReverseIterator& old) : QoreHashIterator(old) {
    }

    DLLLOCAL virtual const char* getName() const {
        return "HashReverseIterator";
    }

    // Reverse fast path: nativeNext routes to prev() (the priv's backwards
    // walker).  Yield shape (VIEW_VALUE / KEY / PAIR) is inherited.
    DLLLOCAL bool nativeNext(ExceptionSink* xsink) override {
        if (check(xsink)) {
            return false;
        }
        return prev();
    }
};

#endif // _QORE_QOREHASHITERATOR_H
