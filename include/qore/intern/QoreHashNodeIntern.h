/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreHashNodeIntern.h

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

#ifndef _QORE_QOREHASHNODEINTERN_H

#define _QORE_QOREHASHNODEINTERN_H

#include <list>

// to maintain the order of inserts
class HashMember {
public:
    QoreValue val{};
    std::string key;

    DLLLOCAL HashMember(const char* n_key) : key(n_key) {
    }

    DLLLOCAL ~HashMember() {
    }
};

typedef std::list<HashMember*> qhlist_t;

#ifdef HAVE_QORE_HASH_MAP
//#warning compiling with hash_map
#include <qore/hash_map_include.h>
#include "qore/intern/xxhash.h"

typedef HASH_MAP<const char*, qhlist_t::iterator, qore_hash_str, eqstr> hm_hm_t;
#else
typedef std::map<const char*, qhlist_t::iterator, ltstr> hm_hm_t;
#endif

// QoreHashIterator private class
class qhi_priv {
public:
    qhlist_t::iterator i;
    bool val;

    DLLLOCAL qhi_priv() : val(false) {
    }

    DLLLOCAL qhi_priv(const qhi_priv& old) : i(old.i), val(old.val) {
    }

    DLLLOCAL bool valid() const {
        return val;
    }

    DLLLOCAL bool next(qhlist_t& ml) {
        //printd(0, "qhi_priv::next() this: %p val: %d\n", this, val);
        if (!val) {
            if (ml.begin() != ml.end()) {
                i = ml.begin();
                val = true;
            }
        } else {
            ++i;
            if (i == ml.end())
                val = false;
        }
        return val;
    }

    DLLLOCAL bool prev(qhlist_t& ml) {
        if (!val) {
            if (ml.begin() != ml.end()) {
                i = ml.end();
                --i;
                val = true;
            }
        } else {
            if (i == ml.begin())
                val = false;
            else
                --i;
        }
        return val;
    }

    DLLLOCAL void reset() {
        val = false;
    }

    DLLLOCAL static qhi_priv* get(HashIterator& i) {
        return i.priv;
    }
};

class qore_hash_private {
public:
    qhlist_t member_list;
    hm_hm_t hm;
    // either hashdecl or complexTypeInfo can be set, but not both
    const TypedHashDecl* hashdecl = nullptr;
    const QoreTypeInfo* complexTypeInfo = nullptr;
    unsigned obj_count = 0;
    QoreReferenceCounter weakRefs;
#ifdef DEBUG
    bool is_obj = false;
#endif
    bool valid = true;

    DLLLOCAL qore_hash_private() {
    }

    // hashes should always be empty by the time they are deleted
    // because object destructors need to be run...
    DLLLOCAL ~qore_hash_private();

    DLLLOCAL static QoreHashNode* newHash(bool needs_eval) {
        QoreHashNode* h = new QoreHashNode;
        if (needs_eval) {
            h->value = false;
            h->needs_eval_flag = true;
        }
        return h;
    }

    DLLLOCAL static void clearNeedsEval(QoreHashNode& h) {
        h.value = true;
        h.needs_eval_flag = false;
    }

    DLLLOCAL static void setNeedsEval(QoreHashNode& h) {
        h.value = false;
        h.needs_eval_flag = true;
    }

    DLLLOCAL static void clear(QoreHashNode& h, ExceptionSink* xsink, bool reverse = false) {
        assert(h.is_unique());
        h.priv->clear(xsink, reverse);
    }

    DLLLOCAL int checkValid(ExceptionSink* xsink) {
        if (!valid) {
            xsink->raiseException("HASH-ERROR", "Cannot modify a hash that has already gone out of scope");
            return -1;
        }
        return 0;
    }

    DLLLOCAL QoreValue getKeyValueIntern(const char* key) const;

    DLLLOCAL QoreValue getKeyValueExistence(const char* key, bool& exists, ExceptionSink* xsink) const;

    DLLLOCAL QoreValue getKeyValueExistenceIntern(const char* key, bool& exists) const;

    DLLLOCAL int checkKey(const char* key, ExceptionSink* xsink) const;

    DLLLOCAL QoreValue getReferencedKeyValueIntern(const char* key) const {
        bool exists;
        return getReferencedKeyValueIntern(key, exists);
    }

    DLLLOCAL QoreValue getReferencedKeyValueIntern(const char* key, bool& exists) const {
        assert(key);

        hm_hm_t::const_iterator i = hm.find(key);
        if (i != hm.end()) {
            exists = true;
            return (*(i->second))->val.refSelf();
        }

        exists = false;
        return QoreValue();
    }

    DLLLOCAL int64 getKeyAsBigInt(const char* key, bool &found) const {
        assert(key);
        hm_hm_t::const_iterator i = hm.find(key);

        if (i != hm.end()) {
            found = true;
            return (*(i->second))->val.getAsBigInt();
        }

        found = false;
        return 0;
    }

    DLLLOCAL bool getKeyAsBool(const char* key, bool& found) const {
        assert(key);
        hm_hm_t::const_iterator i = hm.find(key);

        if (i != hm.end()) {
            found = true;
            return (*(i->second))->val.getAsBool();
        }

        found = false;
        return false;
    }

    DLLLOCAL bool existsKey(const char* key) const {
        assert(key);
        return hm.find(key) != hm.end();
    }

    DLLLOCAL bool existsKeyValue(const char* key) const {
        assert(key);
        hm_hm_t::const_iterator i = hm.find(key);
        if (i == hm.end())
            return false;
        return !(*(i->second))->val.isNothing();
    }

    DLLLOCAL HashMember* findMember(const char* key) {
        assert(key);
        hm_hm_t::iterator i = hm.find(key);
        return i != hm.end() ? (*(i->second)) : nullptr;
    }

   DLLLOCAL HashMember* findCreateMember(const char* key) {
        // otherwise create the new hash entry
        HashMember* om = findMember(key);
        if (om)
            return om;

        om = new HashMember(key);
        assert(om->val.isNothing());
        member_list.push_back(om);

        // add to the map
        qhlist_t::iterator i = member_list.end();
        --i;
        hm[om->key.c_str()] = i;

        // return the new member
        return om;
    }

    DLLLOCAL QoreValue& getValueRef(const char* key) {
        return findCreateMember(key)->val;
    }

    // NOTE: does not delete the value, this must be done by the caller before this call
    // also does not delete map entry; must be done outside this call
    DLLLOCAL void internDeleteKey(qhlist_t::iterator i) {
        HashMember* om = *i;

        member_list.erase(i);

        // free om memory
        delete om;
    }

    DLLLOCAL void deleteKey(const char* key, ExceptionSink *xsink) {
        assert(key);

        hm_hm_t::iterator i = hm.find(key);

        if (i == hm.end())
            return;

        qhlist_t::iterator li = i->second;
        hm.erase(i);

        // dereference node if present
        AbstractQoreNode* n = (*li)->val.assignNothing();
        if (n) {
            if (needs_scan(n))
                incScanCount(-1);

            if (n->getType() == NT_OBJECT)
                reinterpret_cast<QoreObject*>(n)->doDelete(xsink);
            n->deref(xsink);
        }

        internDeleteKey(li);
    }

    // removes the value and dereferences it, without performing a delete on it
    DLLLOCAL void removeKey(const char* key, ExceptionSink *xsink) {
        takeKeyValueIntern(key).discard(xsink);
    }

    DLLLOCAL QoreValue takeKeyValueIntern(const char* key, qore_object_private* obj = nullptr);

    DLLLOCAL QoreValue takeKeyValueIntern(const char* key, bool& exists) {
        assert(key);

        hm_hm_t::iterator i = hm.find(key);

        if (i == hm.end()) {
            exists = false;
            return QoreValue();
        }
        exists = true;

        qhlist_t::iterator li = i->second;
        hm.erase(i);

        QoreValue rv = (*li)->val;
        internDeleteKey(li);

        if (needs_scan(rv))
            incScanCount(-1);

        return rv;
    }

    DLLLOCAL const char* getFirstKey() const  {
        return member_list.empty() ? nullptr : member_list.front()->key.c_str();
    }

    DLLLOCAL const char* getLastKey() const {
        return member_list.empty() ? nullptr : member_list.back()->key.c_str();
    }

    DLLLOCAL QoreListNode* getKeys() const;

    DLLLOCAL void merge(const qore_hash_private& h0, ExceptionSink* xsink);

    // to be called when a lock is held to avoid dereferencing in the lock
    DLLLOCAL void merge(const qore_hash_private& h, SafeDerefHelper& sdh, ExceptionSink* xsink);

    DLLLOCAL int getLValue(const char* key, LValueHelper& lvh, bool for_remove, ExceptionSink* xsink);

    DLLLOCAL void getTypeName(QoreString& str) const {
        if (hashdecl)
            str.sprintf("hash<%s>", hashdecl->getName());
        else if (complexTypeInfo)
            str.concat(QoreTypeInfo::getName(complexTypeInfo));
        else
            str.concat("hash");
    }

    // issue #3877: returns a typed list
    DLLLOCAL QoreListNode* getValues() const;

    DLLLOCAL QoreHashNode* getSlice(const QoreListNode* value_list, ExceptionSink* xsink) const;

    DLLLOCAL QoreHashNode* getCopy() const {
        QoreHashNode* h = new QoreHashNode;
        if (hashdecl) {
            h->priv->setHashDecl(hashdecl);
        } else if (complexTypeInfo) {
            h->priv->complexTypeInfo = complexTypeInfo;
        }
        return h;
    }

    DLLLOCAL QoreHashNode* getEmptyCopy(bool is_value) const {
        QoreHashNode* h = newHash(!is_value);
        if (hashdecl) {
            h->priv->setHashDecl(hashdecl);
        } else if (complexTypeInfo) {
            h->priv->complexTypeInfo = complexTypeInfo;
        }
        return h;
    }

    DLLLOCAL QoreHashNode* copyCheckNewType(const qore_hash_private& other) const {
        QoreHashNode* rv = copy();
        if (hashdecl || other.hashdecl) {
            if (!hashdecl || !other.hashdecl || !hashdecl->equal(other.hashdecl)) {
                rv->priv->setHashDecl(nullptr);
                rv->priv->complexTypeInfo = autoHashTypeInfo;
            }
        } else {
            const QoreTypeInfo* orig_ctype, * ctype;
            orig_ctype = ctype = QoreTypeInfo::getUniqueReturnComplexHash(complexTypeInfo);
            const QoreTypeInfo* newElementType = QoreTypeInfo::getUniqueReturnComplexHash(other.complexTypeInfo);
            if ((!ctype || ctype == anyTypeInfo) && (!newElementType || newElementType == anyTypeInfo)) {
                rv->priv->complexTypeInfo = nullptr;
            } else if (QoreTypeInfo::matchCommonType(ctype, newElementType)) {
                rv->priv->complexTypeInfo = ctype == orig_ctype ? complexTypeInfo : qore_get_complex_hash_type(ctype);
            } else {
                rv->priv->complexTypeInfo = autoHashTypeInfo;
            }
        }
        return rv;
    }

    DLLLOCAL QoreHashNode* copy(const QoreTypeInfo* newComplexTypeInfo) const {
        QoreHashNode* h = new QoreHashNode;
        h->priv->complexTypeInfo = newComplexTypeInfo;
        copyIntern(*h->priv);
        return h;
    }

    // strip = copy without type information
    DLLLOCAL QoreHashNode* copy(bool strip = false) const {
        // issue #2791: perform type stripping at the source
        if (!strip || (!complexTypeInfo && !hashdecl)) {
            QoreHashNode* h = getCopy();
            copyIntern(*h->priv);
            return h;
        }
        QoreHashNode* h = new QoreHashNode;
        // copy all members to new object
        for (auto& i : member_list) {
            hash_assignment_priv ha(*h, i->key.c_str());
            QoreValue v = copy_strip_complex_types(i->val);
#ifdef DEBUG
            assert(ha.swap(v).isNothing());
#else
            ha.swap(v);
#endif
        }

        return h;
    }

    DLLLOCAL void copyIntern(qore_hash_private& h) const {
        // copy all members to new object
        for (auto& i : member_list) {
            hash_assignment_priv ha(h, i->key.c_str());
#ifdef DEBUG
            assert(ha.swap(i->val.refSelf()).isNothing());
#else
            ha.swap(i->val.refSelf());
#endif
        }
    }

    DLLLOCAL QoreHashNode* plusEquals(const QoreHashNode* h, ExceptionSink* xsink) const {
        // issue #2791: perform type stripping at the source
        // issue #3429: maintain types unless we have a plain hash; convert to hash<auto> if the types are not compatible
        ReferenceHolder<QoreHashNode> rv(copyCheckNewType(*h->priv), xsink);
        rv->priv->merge(*h->priv, xsink);
        return *xsink ? nullptr : rv.release();
    }

    DLLLOCAL QoreHashNode* evalImpl(ExceptionSink* xsink) const {
        QoreHashNodeHolder h(getCopy(), xsink);

        for (qhlist_t::const_iterator i = member_list.begin(), e = member_list.end(); i != e; ++i) {
            h->priv->setKeyValue((*i)->key, (*i)->val.refSelf(), xsink);
            if (*xsink)
                return nullptr;
        }

        return h.release();
    }

    DLLLOCAL void setKeyValue(const char* key, QoreValue val, SafeDerefHelper& sdh, ExceptionSink* xsink) {
        hash_assignment_priv ha(*this, key);
        // in case of assigning keys to an initialized hashdecl, the key may already have a value
        ha.assign(val, sdh, xsink);
    }

    DLLLOCAL void setKeyValue(const std::string& key, QoreValue val, SafeDerefHelper& sdh, ExceptionSink* xsink) {
        hash_assignment_priv ha(*this, key.c_str());
        // in case of assigning keys to an initialized hashdecl, the key may already have a value
        ha.assign(val, sdh, xsink);
    }

    DLLLOCAL void setKeyValueIntern(const char* key, QoreValue v) {
        hash_assignment_priv ha(*this, key);
        // in case of assigning keys to an initialized hashdecl, the key may already have a value
        // if the value is an object of a class that throws an exception in the destructor, then a crash will result
        ValueHolder old(ha.swap(v), nullptr);
    }

    DLLLOCAL void setKeyValue(const char* key, QoreValue val, ExceptionSink* xsink) {
        hash_assignment_priv ha(*this, key);
        ha.assign(val, xsink);
    }

    DLLLOCAL void setKeyValue(const std::string& key, QoreValue val, ExceptionSink* xsink) {
        hash_assignment_priv ha(*this, key.c_str());
        ha.assign(val, xsink);
    }

    DLLLOCAL void setKeyValue(const char* key, QoreValue val, qore_object_private* o, ExceptionSink* xsink) {
        hash_assignment_priv ha(*this, key, false, o);
        ha.assign(val, xsink);
    }

    DLLLOCAL bool derefImpl(ExceptionSink* xsink, bool reverse = false) {
        if (reverse) {
            for (qhlist_t::reverse_iterator i = member_list.rbegin(), e = member_list.rend(); i != e; ++i) {
                (*i)->val.discard(xsink);
                delete *i;
            }
        } else {
            for (qhlist_t::iterator i = member_list.begin(), e = member_list.end(); i != e; ++i) {
                (*i)->val.discard(xsink);
                delete *i;
            }
        }

        member_list.clear();
        hm.clear();
        obj_count = 0;
        valid = false;
        return true;
    }

    DLLLOCAL QoreValue swapKeyValueIfExists(const char* key, QoreValue val, qore_object_private* o, bool& exists) {
        hash_assignment_priv ha(*this, key, true, o);
        if (!ha.exists()) {
            exists = false;
            return QoreValue();
        }
        exists = true;
        return ha.swap(val);
    }

    DLLLOCAL QoreValue swapKeyValue(const char* key, QoreValue val, qore_object_private* o) {
        //printd(0, "qore_hash_private::swapKeyValue() this: %p key: %s val: %p (%s) deprecated API called\n", this, key, val, get_node_type(val));
        //assert(false);
        hash_assignment_priv ha(*this, key, false, o);
        return ha.swap(val);
    }

    DLLLOCAL void clear(ExceptionSink* xsink, bool reverse) {
        derefImpl(xsink, reverse);
    }

    DLLLOCAL size_t size() const {
        return member_list.size();
    }

    DLLLOCAL bool empty() const {
        return member_list.empty();
    }

    DLLLOCAL void incScanCount(int dt) {
        assert(!is_obj);
        assert(dt);
        assert(obj_count || dt > 0);
        //printd(5, "qore_hash_private::incScanCount() this: %p dt: %d: %d -> %d\n", this, dt, obj_count, obj_count + dt);
        obj_count += dt;
    }

    DLLLOCAL const QoreTypeInfo* getValueTypeInfo() const {
        if (hashdecl) {
            // For hashdecl-typed hashes, the value type is the hashdecl's type itself
            return hashdecl->getTypeInfo();
        }
        return complexTypeInfo ? QoreTypeInfo::getComplexHashValueType(complexTypeInfo) : nullptr;
    }

    DLLLOCAL const QoreTypeInfo* getTypeInfo() const {
        if (hashdecl)
            return hashdecl->getTypeInfo();
        if (complexTypeInfo)
            return complexTypeInfo;
        return hashTypeInfo;
    }

    DLLLOCAL const TypedHashDecl* getHashDecl() const {
        return hashdecl;
    }

    DLLLOCAL void weakRef() {
        weakRefs.ROreference();
    }

    DLLLOCAL bool weakDeref() {
        if (weakRefs.ROdereference()) {
            return true;
        }
        return false;
    }

    DLLLOCAL void setHashDecl(const TypedHashDecl* hd);

    DLLLOCAL static QoreHashNode* getPlainHash(QoreHashNode* h) {
        if (!h->priv->hashdecl && !h->priv->complexTypeInfo) {
            return h;
        }
        // no exception is possible
        ReferenceHolder<QoreHashNode> holder(h, nullptr);
        return h->priv->copy(true);
    }

    DLLLOCAL static QoreHashNode* newHashDecl(const TypedHashDecl* hd) {
        QoreHashNode* rv = new QoreHashNode;
        rv->priv->setHashDecl(hd);
        return rv;
    }

    DLLLOCAL static qore_hash_private* get(QoreHashNode& h) {
        return h.priv;
    }

    DLLLOCAL static qore_hash_private* get(QoreHashNode* h) {
        return h ? h->priv : nullptr;
    }

    DLLLOCAL static const qore_hash_private* get(const QoreHashNode& h) {
        return h.priv;
    }

    DLLLOCAL static const qore_hash_private* get(const QoreHashNode* h) {
        return h ? h->priv : nullptr;
    }

    // returns -1 if no checks are needed or if an error is raised, 0 if OK to check
    DLLLOCAL static int parseInitHashInitialization(const QoreProgramLocation* loc, QoreParseContext& parse_context,
            QoreParseListNode* args, QoreValue& arg, int& err);

    DLLLOCAL static int parseInitComplexHashInitialization(const QoreProgramLocation* loc, QoreParseContext& parse_context,
            QoreParseListNode* args);

    DLLLOCAL static int parseCheckComplexHashInitialization(const QoreProgramLocation* loc,
            const QoreTypeInfo* valueTypeInfo, const QoreTypeInfo* argTypeInfo, QoreValue exp,
            const char* context_action, bool strict_check = true);

    DLLLOCAL static int parseCheckTypedAssignment(const QoreProgramLocation* loc, QoreValue arg,
            const QoreTypeInfo* vti, const char* context_action, bool strict_check = true);

    DLLLOCAL static QoreHashNode* newComplexHash(const QoreTypeInfo* typeInfo, const QoreParseListNode* args,
            ExceptionSink* xsink);

    DLLLOCAL static QoreHashNode* newComplexHashFromHash(const QoreTypeInfo* typeInfo, QoreHashNode* init,
            ExceptionSink* xsink, bool coerce_values = false);

    DLLLOCAL static unsigned getScanCount(const QoreHashNode& h) {
        assert(!h.priv->is_obj);
        return h.priv->obj_count;
    }

    DLLLOCAL static void incScanCount(const QoreHashNode& h, int dt) {
        assert(!h.priv->is_obj);
        h.priv->incScanCount(dt);
    }

    DLLLOCAL static QoreValue getFirstKeyValue(const QoreHashNode* h) {
        return h->priv->member_list.empty() ? QoreValue() : h->priv->member_list.front()->val;
    }

    DLLLOCAL static QoreValue getLastKeyValue(const QoreHashNode* h) {
        return h->priv->member_list.empty() ? QoreValue() : h->priv->member_list.back()->val;
    }
};

#endif
