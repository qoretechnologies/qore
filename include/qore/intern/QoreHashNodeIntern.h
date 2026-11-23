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

#include "qore/intern/QoreSandboxMemory.h"
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

#include <unordered_map>
#include "qore/intern/xxhash.h"
#include "qore/intern/qore_container_free.h"
#include "qore/intern/QoreKeyCompare.h"

//! A key to look up in the key map of a hash: the key, its length and its hash (qore#7194)
/** The hash is the one that qore_hash_str gives for the key, so that a hash computed in advance with qore_hash_str
    (ex: by the JIT for a constant key) finds the key.
*/
struct qore_hash_lookup_key {
    const char* str;
    size_t len;
    size_t hash;

    //! returns the hash of the key of \a len bytes at \a str; the same as qore_hash_str for the key
    DLLLOCAL static size_t hashBytes(const char* str, size_t len) {
#if TARGET_BITS == 64
        return XXH64(str, len, 0);
#else
        return XXH32(str, len, 0);
#endif
    }

    DLLLOCAL explicit qore_hash_lookup_key(const char* str) : str(str), len(strlen(str)), hash(hashBytes(str, len)) {
    }

    DLLLOCAL qore_hash_lookup_key(const char* str, size_t len) : str(str), len(len), hash(hashBytes(str, len)) {
        assert(strlen(str) == len);
    }

    DLLLOCAL qore_hash_lookup_key(const char* str, size_t len, size_t hash) : str(str), len(len), hash(hash) {
        assert(strlen(str) == len);
        assert(hash == hashBytes(str, len));
    }
};

//! The hash function of the key map of a hash (qore#7194)
/** Not noexcept: libstdc++ then keeps the hash of each key in its node and compares it before the keys, so that the
    keys of a lookup are compared only when their hashes are equal (see the static_assert after hm_hm_t)
*/
struct qore_hash_member_key_hash {
    using is_transparent = void;

    DLLLOCAL size_t operator()(const std::string* key) const {
        return qore_hash_lookup_key::hashBytes(key->data(), key->size());
    }

    DLLLOCAL size_t operator()(const qore_hash_lookup_key& key) const {
        return key.hash;
    }
};

//! The key comparison of the key map of a hash: by length, then inline (qore#7194)
struct qore_hash_member_key_eq {
    using is_transparent = void;

    DLLLOCAL bool operator()(const std::string* a, const std::string* b) const {
        return a == b || qore_key_equal(a->data(), a->size(), b->data(), b->size());
    }

    DLLLOCAL bool operator()(const qore_hash_lookup_key& a, const std::string* b) const {
        return qore_key_equal(a.str, a.len, b->data(), b->size());
    }

    DLLLOCAL bool operator()(const std::string* a, const qore_hash_lookup_key& b) const {
        return qore_key_equal(a->data(), a->size(), b.str, b.len);
    }
};

//! The key map of a hash: the key of each member (the key string of its HashMember) to its place in the member list
/** The map refers to the key of the member (HashMember::key), which is not changed while the member is in the hash,
    so that a key comparison knows the length of the key (qore#7194)
*/
typedef std::unordered_map<const std::string*, qhlist_t::iterator, qore_hash_member_key_hash,
    qore_hash_member_key_eq> hm_hm_t;
#ifdef __GLIBCXX__
// the hash of each key is kept in its node and compared before the keys (see qore_hash_member_key_hash)
static_assert(std::__cache_default<const std::string*, qore_hash_member_key_hash>::value,
    "the key map of a hash must keep the hash of each key");
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
    QoreContainerWeakRefs weakRefs;
#ifdef DEBUG
    bool is_obj = false;
#endif
    //! false if the hash cannot be changed: it was freed, or it is a copy of a freed hash (see copyFreedState())
    bool valid = true;
    //! true once the hash has been freed (see QoreHashNode::derefImpl())
    bool freed = false;
    //! true if the weak holders of the hash hold one reference of their own since its free started (see
    //! QoreContainerWeakRefs)
    bool weak_hold = false;
    //! the estimated memory of the members (see sbMemberSize()); always kept, so that it is right when charged
    size_t sb_bytes = 0;
    //! the sandbox memory charge of the members (see design/sandbox-resource-limits.md); synced with sb_bytes
    QoreSandboxChargeRecord sb;

    DLLLOCAL qore_hash_private() {
    }

    //! the estimated memory of a member: the member, its key and its node in the member list and in the key map
    DLLLOCAL static size_t sbMemberSize(const HashMember* m) {
        return sizeof(HashMember) + m->key.size() + 4 * sizeof(void*) + 2 * sizeof(void*) + sizeof(qhlist_t::iterator);
    }

    //! Accounts a member that was added
    DLLLOCAL void sbAdd(const HashMember* m) {
        sb_bytes += sbMemberSize(m);
        if (sb.charge || qore_sb_mem.charge) {
            sb.sync(sb_bytes);
        }
    }

    //! Accounts a member that is removed
    DLLLOCAL void sbRemove(const HashMember* m) {
        size_t size = sbMemberSize(m);
        sb_bytes = size > sb_bytes ? 0 : sb_bytes - size;
        if (sb.charge) {
            sb.sync(sb_bytes);
        }
    }

    //! Accounts the removal of all members
    DLLLOCAL void sbClear() {
        sb_bytes = 0;
        sb.release();
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
            // as qore_list_private::checkValid(): a caller can have no exception sink (the merge of the public API)
            if (xsink) {
                xsink->raiseException("HASH-ERROR", "Cannot modify a hash that has already gone out of scope");
            }
            return -1;
        }
        return 0;
    }

    //! returns the key map entry of \a key, or hm.end()
    /** An empty hash is not searched, so that its key is not hashed (qore#7194)
    */
    DLLLOCAL hm_hm_t::const_iterator findIter(const char* key) const {
        return hm.empty() ? hm.end() : hm.find(qore_hash_lookup_key(key));
    }

    DLLLOCAL hm_hm_t::iterator findIter(const char* key) {
        return hm.empty() ? hm.end() : hm.find(qore_hash_lookup_key(key));
    }

    //! returns the key map entry of \a key of \a len bytes, or hm.end()
    DLLLOCAL hm_hm_t::const_iterator findIter(const char* key, size_t len) const {
        return hm.empty() ? hm.end() : hm.find(qore_hash_lookup_key(key, len));
    }

    DLLLOCAL hm_hm_t::iterator findIter(const char* key, size_t len) {
        return hm.empty() ? hm.end() : hm.find(qore_hash_lookup_key(key, len));
    }

    //! returns the key map entry of \a key, or hm.end()
    DLLLOCAL hm_hm_t::const_iterator findIter(const qore_hash_lookup_key& key) const {
        return hm.empty() ? hm.end() : hm.find(key);
    }

    DLLLOCAL hm_hm_t::iterator findIter(const qore_hash_lookup_key& key) {
        return hm.empty() ? hm.end() : hm.find(key);
    }

    //! returns the key map entry of \a key, whose hash is \a hash, or hm.end()
    DLLLOCAL hm_hm_t::const_iterator findIterPrehashed(const char* key, size_t hash) const {
        return hm.empty() ? hm.end() : hm.find(qore_hash_lookup_key(key, strlen(key), hash));
    }

    DLLLOCAL hm_hm_t::iterator findIterPrehashed(const char* key, size_t hash) {
        return hm.empty() ? hm.end() : hm.find(qore_hash_lookup_key(key, strlen(key), hash));
    }

    //! Returns true if each member of this hash has a member of \a other with the same key whose value is equal to
    //! its value by \a value_equal (qore#7266)
    /** Compares the members of two hashes of the same size for the equality operators.  It walks both member lists
        together while the keys of the members are the same (by length and inline, without hashing the key); from
        the first pair of members whose keys differ on, it looks each remaining key of this hash up in the key map of
        \a other.  The values are compared in the order of the members of this hash, with the value of this hash as
        the first argument of \a value_equal, as a lookup of every key in \a other does, so that the result, the
        exceptions raised and the order of the comparisons are the same.

        @param other the other hash; the caller has checked that it has as many members as this hash
        @param value_equal a function taking the value of a member of this hash and the value of the member of
        \a other with the same key, returning true if they are equal; the comparison ends with false as soon as it
        returns false

        @return true if every member of this hash has a member of \a other with the same key and an equal value
    */
    template <typename F>
    DLLLOCAL bool membersEqual(const qore_hash_private& other, F&& value_equal) const {
        assert(member_list.size() == other.member_list.size());
        qhlist_t::const_iterator i = member_list.begin();
        const qhlist_t::const_iterator e = member_list.end();
        // the other list's end is tested too, so that the walk cannot pass it whatever the sizes are
        for (qhlist_t::const_iterator j = other.member_list.begin(), oe = other.member_list.end(); i != e && j != oe;
                ++i, ++j) {
            const HashMember* m = *i;
            const HashMember* om = *j;
            if (!qore_key_equal(m->key.data(), m->key.size(), om->key.data(), om->key.size())) {
                break;
            }
            // the keys are unique, so the member with the same key is the member at the same place
            if (!value_equal(m->val, om->val)) {
                return false;
            }
        }
        for (; i != e; ++i) {
            const HashMember* m = *i;
            hm_hm_t::const_iterator found = other.findIter(m->key.data(), m->key.size());
            if (found == other.hm.end() || !value_equal(m->val, (*found->second)->val)) {
                return false;
            }
        }
        return true;
    }

    DLLLOCAL QoreValue getKeyValueIntern(const char* key) const;

    DLLLOCAL QoreValue getKeyValuePrehashed(const char* key, size_t hash, ExceptionSink* xsink) const;
    DLLLOCAL QoreValue getKeyValueExistencePrehashed(const char* key, size_t hash,
            bool& exists, ExceptionSink* xsink) const;
    DLLLOCAL QoreValue getKeyValueExistencePrehashedIntern(const char* key, size_t hash,
            bool& exists) const;

    DLLLOCAL QoreValue getKeyValueExistence(const char* key, bool& exists, ExceptionSink* xsink) const;

    DLLLOCAL QoreValue getKeyValueExistenceIntern(const char* key, bool& exists) const;

    DLLLOCAL int checkKey(const char* key, ExceptionSink* xsink) const;

    // raises an INVALID-MEMBER exception if the key cannot be assigned in a hashdecl hash
    /** @return 0 for OK, -1 if an exception was raised
    */
    DLLLOCAL int checkLValueKey(const char* key, ExceptionSink* xsink) const;

    DLLLOCAL QoreValue getReferencedKeyValueIntern(const char* key) const {
        bool exists;
        return getReferencedKeyValueIntern(key, exists);
    }

    DLLLOCAL QoreValue getReferencedKeyValueIntern(const char* key, bool& exists) const {
        assert(key);
        hm_hm_t::const_iterator i = findIter(key);
        if (i != hm.end()) {
            exists = true;
            return (*(i->second))->val.refSelf();
        }

        exists = false;
        return QoreValue();
    }

    //! returns the value of the key \a key of \a len bytes with a reference, or no value; \a exists is set if the
    //! key exists
    DLLLOCAL QoreValue getReferencedKeyValueIntern(const char* key, size_t len, bool& exists) const {
        hm_hm_t::const_iterator i = findIter(key, len);
        if (i != hm.end()) {
            exists = true;
            return (*(i->second))->val.refSelf();
        }

        exists = false;
        return QoreValue();
    }

    //! returns the value of the key with a reference, or no value; \a exists is set if the key exists
    DLLLOCAL QoreValue getReferencedKeyValueIntern(const qore_hash_lookup_key& key, bool& exists) const {
        hm_hm_t::const_iterator i = findIter(key);
        if (i != hm.end()) {
            exists = true;
            return (*(i->second))->val.refSelf();
        }

        exists = false;
        return QoreValue();
    }

    DLLLOCAL QoreValue getReferencedKeyValuePrehashedIntern(
            const char* key, size_t hash) const {
        assert(key);
        hm_hm_t::const_iterator i = findIterPrehashed(key, hash);
        return i != hm.end() ? (*(i->second))->val.refSelf() : QoreValue();
    }

    DLLLOCAL int64 getKeyAsBigInt(const char* key, bool &found) const {
        assert(key);
        hm_hm_t::const_iterator i = findIter(key);

        if (i != hm.end()) {
            found = true;
            return (*(i->second))->val.getAsBigInt();
        }

        found = false;
        return 0;
    }

    DLLLOCAL bool getKeyAsBool(const char* key, bool& found) const {
        assert(key);
        hm_hm_t::const_iterator i = findIter(key);

        if (i != hm.end()) {
            found = true;
            return (*(i->second))->val.getAsBool();
        }

        found = false;
        return false;
    }

    DLLLOCAL bool existsKey(const char* key) const {
        assert(key);
        return findIter(key) != hm.end();
    }

    DLLLOCAL bool existsKeyValue(const char* key) const {
        assert(key);
        hm_hm_t::const_iterator i = findIter(key);
        if (i == hm.end())
            return false;
        return !(*(i->second))->val.isNothing();
    }

    DLLLOCAL HashMember* findMember(const char* key) {
        assert(key);
        hm_hm_t::iterator i = findIter(key);
        return i != hm.end() ? (*(i->second)) : nullptr;
    }

    //! returns the member with the key \a key of \a len bytes, or nullptr
    DLLLOCAL HashMember* findMember(const char* key, size_t len) {
        hm_hm_t::iterator i = findIter(key, len);
        return i != hm.end() ? (*(i->second)) : nullptr;
    }

    DLLLOCAL HashMember* findMember(const qore_hash_lookup_key& key) {
        hm_hm_t::iterator i = findIter(key);
        return i != hm.end() ? (*(i->second)) : nullptr;
    }

    DLLLOCAL HashMember* findMemberPrehashed(const char* key, size_t hash) {
        assert(key);
        hm_hm_t::iterator i = findIterPrehashed(key, hash);
        return i != hm.end() ? (*(i->second)) : nullptr;
    }

    DLLLOCAL HashMember* findCreateMember(const char* key) {
        // otherwise create the new hash entry
        HashMember* om = findMember(key);
        if (om)
            return om;

        return createMemberKnownAbsent(key);
    }

    // as above; "created" is set to true if the member did not exist and was created here
    DLLLOCAL HashMember* findCreateMember(const char* key, bool& created) {
        HashMember* om = findMember(key);
        if (om) {
            created = false;
            return om;
        }

        created = true;
        return createMemberKnownAbsent(key);
    }

    DLLLOCAL HashMember* findCreateMemberPrehashed(const char* key, size_t hash) {
        HashMember* om = findMemberPrehashed(key, hash);
        return om ? om : createMemberKnownAbsent(key);
    }

    DLLLOCAL HashMember* createMemberKnownAbsent(const char* key) {
        HashMember* om = new HashMember(key);
        assert(om->val.isNothing());
        member_list.push_back(om);
        sbAdd(om);

        // add to the map
        qhlist_t::iterator i = member_list.end();
        --i;
        hm[&om->key] = i;

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
        sbRemove(om);

        // free om memory
        delete om;
    }

    DLLLOCAL void deleteKey(const char* key, ExceptionSink *xsink) {
        assert(key);

        hm_hm_t::iterator i = findIter(key);

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

    //! Takes the value of a key for delete or remove (qore#7306)
    /** a hash freed under a weak reference and a copy of one (qore#7163) are empty: the key is not found, and
        HASH-ERROR is raised then, which costs nothing when the key is removed; the hash can be shared (the hash of a
        weak reference), so its uniqueness is not asserted

        @return the value of the key, or NOTHING if it does not exist or an exception was raised
    */
    DLLLOCAL QoreValue takeRemovedKey(const char* key, ExceptionSink* xsink);

    //! Raises HASH-ERROR for delete or remove that removes nothing from a hash freed under a weak reference or from a
    //! copy of one (qore#7163), which is empty; the removers call it only when they remove nothing (qore#7306)
    /** @return -1 if the hash cannot be changed and HASH-ERROR was raised, 0 if not
    */
    DLLLOCAL int checkRemovalValid(ExceptionSink* xsink) {
        // a hash whose last reference was released is empty, and so is a copy of it
        assert(valid || hm.empty());
        return checkValid(xsink);
    }

    DLLLOCAL QoreValue takeKeyValueIntern(const char* key, bool& exists) {
        assert(key);

        hm_hm_t::iterator i = findIter(key);

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
    DLLLOCAL void mergeNative(const qore_hash_private& h, ExceptionSink* xsink);
    DLLLOCAL static int setKeyValueCanonical(QoreHashNode& hash, const char* key, QoreValue value,
        ExceptionSink* xsink);

    DLLLOCAL static int setKeyValueLexical(QoreHashNode& hash, const char* key, QoreValue value,
        ExceptionSink* xsink, LexicalSemanticPolicy policy);

    // to be called when a lock is held to avoid dereferencing in the lock
    DLLLOCAL void merge(const qore_hash_private& h, SafeDerefHelper& sdh, ExceptionSink* xsink);

    DLLLOCAL void merge(const qore_hash_private& h, SafeDerefHelper& sdh, ExceptionSink* xsink,
            LexicalSemanticPolicy policy);

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

    //! returns the hashdecl of the two that inherits the other, or nullptr if neither inherits the other
    DLLLOCAL static const TypedHashDecl* getInheritingHashDecl(const TypedHashDecl* a, const TypedHashDecl* b);

    //! returns a copy of the hash with the type of its sum with \a other (ex: <tt>h + other</tt>)
    DLLLOCAL QoreHashNode* copyCheckNewType(const qore_hash_private& other) const {
        // a new value, which can be changed also when this hash was freed (see copyFreedState())
        QoreHashNode* rv = getCopy();
        copyIntern(*rv->priv);
        if (hashdecl || other.hashdecl) {
            if (!hashdecl || !other.hashdecl || !hashdecl->equal(other.hashdecl)) {
                // issue #6023: if one hashdecl inherits the other, the result is a hash of the child, which declares
                // every key of both
                const TypedHashDecl* hd = hashdecl && other.hashdecl
                    ? getInheritingHashDecl(hashdecl, other.hashdecl)
                    : nullptr;
                if (!hd) {
                    rv->priv->setHashDecl(nullptr);
                    rv->priv->complexTypeInfo = autoHashTypeInfo;
                } else if (hd != hashdecl) {
                    rv->priv->setHashDecl(hd);
                }
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
            copyFreedState(*h->priv);
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
        copyFreedState(*h->priv);
        return h;
    }

    //! A copy of a freed hash cannot be changed either (qore#7163)
    /** A hash freed under weak references is held by its weak holders (see QoreContainerWeakRefs), so a new
        reference to it is never its only one, and changing the hash through that reference copies it; the copy reads
        as empty, as the hash does, and changing it raises \c HASH-ERROR, as changing the hash itself does.  A value
        computed from it (its sum with another hash) is a new hash.

        The copy is not freed: it is freed as any other hash when its last reference is released.
    */
    DLLLOCAL void copyFreedState(qore_hash_private& h) const {
        if (!valid) {
            h.valid = false;
        }
    }

    //! returns a new hash with the members of this one, which can be changed also when this hash was freed
    /** For a value computed from the hash (ex: <tt>h - "key"</tt>, <tt>h + obj</tt>), which is a new hash, as the sum
        with another hash is (copyCheckNewType()); copy() keeps the freed state, for a copy that stands for the hash
        itself (qore#7199)

        @param strip true to copy without type information, as copy() does
    */
    DLLLOCAL QoreHashNode* copyValue(bool strip = false) const {
        QoreHashNode* h = copy(strip);
        h->priv->valid = true;
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
        size_t count = 0;
        for (qhlist_t::const_iterator i = member_list.begin(), e = member_list.end(); i != e; ++i) {
            if (++count % 100 == 0 && qore_check_cancel(xsink, "hash expression evaluation")) {
                return nullptr;
            }
            ValueEvalOptimizedRefHolder value((*i)->val, xsink);
            if (*xsink) {
                return nullptr;
            }
            h->priv->setKeyValue((*i)->key, value.takeReferencedValue(), xsink);
            if (*xsink) {
                return nullptr;
            }
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

    DLLLOCAL void setKeyValue(const char* key, QoreValue val, SafeDerefHelper& sdh, ExceptionSink* xsink,
            LexicalSemanticPolicy policy) {
        hash_assignment_priv ha(*this, key);
        ha.assign(val, sdh, xsink, policy);
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

    DLLLOCAL void setKeyValueKnownAbsent(const char* key, QoreValue val, ExceptionSink* xsink) {
        // the member is created here, so a rejected assignment removes it again
        hash_assignment_priv ha(*this, createMemberKnownAbsent(key), true);
        ha.assign(val, xsink);
    }

    DLLLOCAL void setKeyValueKnownAbsentIntern(const char* key, QoreValue val) {
        HashMember* member = createMemberKnownAbsent(key);
        member->val = val;
        if (needs_scan(val)) {
            incScanCount(1);
        }
    }

    DLLLOCAL void setKeyValue(const std::string& key, QoreValue val, ExceptionSink* xsink) {
        hash_assignment_priv ha(*this, key.c_str());
        ha.assign(val, xsink);
    }

    DLLLOCAL void setKeyValue(const char* key, QoreValue val, qore_object_private* o, ExceptionSink* xsink) {
        hash_assignment_priv ha(*this, key, false, o);
        ha.assign(val, xsink);
    }

    //! Frees all members; the hash itself stays valid (see clear())
    DLLLOCAL void freeMembers(ExceptionSink* xsink, bool reverse = false) {
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
        sbClear();
    }

    //! Frees all members of a hash whose last reference was released, and marks it as freed
    /** A weak reference can keep the node allocated after this; \c freed then tells that it was freed.
    */
    DLLLOCAL bool derefImpl(ExceptionSink* xsink, bool reverse = false) {
        freeMembers(xsink, reverse);
        valid = false;
        freed = true;
        return true;
    }

    //! Frees the next member of a hash whose reference count has reached zero
    /** The member is removed from the member list before its value is freed.

        @return false if all members have been freed
    */
    DLLLOCAL static bool freeNextMember(QoreHashNode& h, ExceptionSink* xsink) {
        qore_hash_private* p = h.priv;
        if (p->member_list.empty()) {
            return false;
        }
        HashMember* member = p->member_list.front();
        p->member_list.pop_front();
        qore_container_free_helper::freeEntry(member->val, xsink);
        delete member;
        return true;
    }

    //! Finishes freeing a hash after its members have been freed
    DLLLOCAL static void finishFree(QoreHashNode& h) {
        qore_hash_private* p = h.priv;
        assert(p->member_list.empty());
        p->hm.clear();
        p->obj_count = 0;
        p->sbClear();
        finishFreeMembers(h);
    }

    //! Finishes freeing a hash after its members have been freed and its member index cleared
    DLLLOCAL static void finishFreeMembers(QoreHashNode& h) {
        qore_hash_private* p = h.priv;
        if (!p->weak_hold) {
            // no weak holder can take a reference to the hash (see QoreContainerWeakRefs)
            delete &h;
            return;
        }
        // its weak holders keep the node allocated, and read it as an empty hash
        p->valid = false;
        p->freed = true;
        h.weakDeref();
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

    //! Removes the key and returns its value, if the key exists; \a o is the object whose members this hash holds
    DLLLOCAL QoreValue takeKeyValueIfExists(const char* key, qore_object_private* o, bool& exists);

    DLLLOCAL QoreValue swapKeyValue(const char* key, QoreValue val, qore_object_private* o) {
        //printd(0, "qore_hash_private::swapKeyValue() this: %p key: %s val: %p (%s) deprecated API called\n", this, key, val, get_node_type(val));
        //assert(false);
        hash_assignment_priv ha(*this, key, false, o);
        return ha.swap(val);
    }

    //! Empties a hash that stays in use (an object's members before it is released, a thread's data)
    /** Unlike derefImpl(), the hash is not marked as freed: marking it made a hash that is used again, such as the
        thread-local data of a pooled thread, reject changes, and made its eventual release look like the release of
        a freed hash.
    */
    DLLLOCAL void clear(ExceptionSink* xsink, bool reverse) {
        freeMembers(xsink, reverse);
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

    //! returns the type of the values of a typed hash; a hashdecl gives no single type to its members
    DLLLOCAL const QoreTypeInfo* getValueTypeInfo() const {
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

    DLLLOCAL bool weakDeref() {
        if (weakRefs.ROdereference()) {
            return true;
        }
        return false;
    }

    DLLLOCAL void setHashDecl(const TypedHashDecl* hd);

    //! Returns a new reference to a hash that a weak reference holds, or a new empty hash that cannot be changed if
    //! its last reference was released (qore#7198; see WeakHashReferenceNode::lock() and qore_list_private::lockWeak())
    DLLLOCAL static QoreHashNode* lockWeak(QoreHashNode& h) {
        // the reference is taken as QoreReferenceCounter::optRef() takes it, inline
        int count = h.references.load();
        while (count > 0) {
            if (h.references.compare_exchange_weak(count, count + 1)) {
                return !h.priv->weak_hold ? &h : lockWeakFreeing(h);
            }
        }
        return getFreedStandIn(h);
    }

    //! Releases the reference that lockWeak() took to a hash whose free has started, and returns a new empty hash
    //! (see qore_list_private::lockWeakFreeing())
    DLLLOCAL static __attribute__((noinline)) QoreHashNode* lockWeakFreeing(QoreHashNode& h) {
#ifdef DEBUG
        bool last =
#endif
        h.ROdereference();
        assert(!last);
        return getFreedStandIn(h);
    }

    //! Returns a new empty hash with the type of a hash whose last reference was released, which cannot be changed
    //! (qore#7198; see lockWeak())
    DLLLOCAL static __attribute__((noinline)) QoreHashNode* getFreedStandIn(const QoreHashNode& h) {
        QoreHashNode* rv = h.priv->getCopy();
        rv->priv->valid = false;
        return rv;
    }

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
        ExceptionSink* xsink, LexicalSemanticPolicy policy);

    DLLLOCAL static QoreHashNode* newComplexHash(const QoreTypeInfo* typeInfo, const QoreParseListNode* args,
            ExceptionSink* xsink);

    DLLLOCAL static QoreHashNode* newComplexHashFromHash(const QoreTypeInfo* typeInfo, QoreHashNode* init,
            ExceptionSink* xsink, bool coerce_values = false);

    DLLLOCAL static QoreHashNode* newComplexHashFromHash(const QoreTypeInfo* typeInfo, QoreHashNode* init,
            ExceptionSink* xsink, bool coerce_values, LexicalSemanticPolicy policy);

    template <bool Lexical>
    DLLLOCAL static QoreHashNode* newComplexHashFromHashImpl(const QoreTypeInfo* typeInfo, QoreHashNode* init,
            ExceptionSink* xsink, bool coerce_values, LexicalSemanticPolicy policy);

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
