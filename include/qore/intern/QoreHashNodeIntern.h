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
#include <iterator>
#include <limits>

#include <unordered_map>
#include "qore/intern/xxhash.h"
#include "qore/intern/qore_container_free.h"
#include "qore/intern/QoreKeyCompare.h"

//! A key to look up in the key map of a hash: the key, its length and its hash (qore#7194)
/** The hash is the member hash of the key (hashBytes()), so that a hash computed in advance (ex: by the JIT or qcc
    for a constant key) finds the key; a hardened key map hashes the key bytes with its keyed hash instead
    (qore_hash_member_map, qore#7557).
*/
struct DLLLOCAL qore_hash_lookup_key {
    const char* str;
    size_t len;
    size_t hash;

    //! the tag of the constructor that takes the hash that a member of a key map keeps (HashMember::hash)
    struct stored_hash_t {
    };

    //! returns the member hash of the key of \a len bytes at \a str: the hash of the key in a key map that is not
    //! hardened, and the hash of the constant keys that the JIT and qcc compute in advance
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

    //! a key with the hash that a member of a key map keeps: the member hash, or the keyed hash of a hardened map
    /** for a search of the key map that the member is in, or of another map of the same kind (see
        qore_hash_member_map::sameKind())
    */
    DLLLOCAL qore_hash_lookup_key(const char* str, size_t len, size_t hash, stored_hash_t) : str(str), len(len),
            hash(hash) {
        assert(strlen(str) == len);
    }
};

//! returns the keyed hash of the key of \a len bytes at \a str, the hash of a key in a hardened key map (qore#7557)
/** SipHash-1-3 (qore_siphash13()) with the secret key of the process; only called once the key exists (a key map is
    hardened only after qore_hash_member_map::harden() has created it)
*/
DLLLOCAL size_t qore_hash_keyed_bytes(const char* str, size_t len);

//! A member of a hash: its value, key and the links of the member list and of the key map, in one allocation
/** The member keeps the hash of its key in the key map of its hash (qore_hash_member_map::hashOf()), so that the key
    map does not hash it again, and so that a copy of the hash inserts its keys without hashing them again
    (qore#7265).

    The member is the node of the member list (\c prev, \c next, in insertion order) and of the key map
    (\c bucket_next): a member costs one allocation, where a member, a node of a std::list and a node of a
    std::unordered_map cost three (qore#7272); a member keeps its address while it is in the hash, so that a pointer
    to it is a stable iterator of the hash.
*/
class DLLLOCAL HashMember {
public:
    //! the next member in the bucket of the key map (see qore_hash_member_map)
    HashMember* bucket_next = nullptr;
    //! the hash of the key in the key map of the hash (qore_hash_member_map::hashOf())
    size_t hash;
    QoreValue val{};
    std::string key;
    //! the previous and the next members in the order of the hash (see qhlist_t)
    HashMember* prev = nullptr;
    HashMember* next = nullptr;

    //! creates a member with the key and hash of \a k
    DLLLOCAL explicit HashMember(const qore_hash_lookup_key& k) : hash(k.hash), key(k.str, k.len) {
    }

    //! creates a member with the key \a n_key, whose hash is \a n_hash (ex: the key and hash of another member)
    /** the key map that takes the member checks that the hash is the hash of the key in that map
    */
    DLLLOCAL HashMember(const std::string& n_key, size_t n_hash) : hash(n_hash), key(n_key) {
    }

    DLLLOCAL HashMember(const HashMember&) = delete;
    DLLLOCAL HashMember& operator=(const HashMember&) = delete;

    //! returns true if the member has the key \a k
    DLLLOCAL bool matches(const qore_hash_lookup_key& k) const {
        return hash == k.hash && qore_key_equal(key.data(), key.size(), k.str, k.len);
    }
};

//! a function of the key map of a hash that is not inlined into its callers (qore#7272)
#if defined(__GNUC__) || defined(__clang__)
#define QORE_HASH_NOINLINE __attribute__((noinline))
#else
#define QORE_HASH_NOINLINE
#endif

//! The members of a hash in their order: a list linked through the members themselves (qore#7272)
/** The list does not own its members; it allocates nothing, and none of its operations can fail.  An iterator is a
    pointer to a member, which stays valid until that member is removed from the list.
*/
class DLLLOCAL qhlist_t {
public:
    //! a forward iterator; end() is the null member
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = HashMember*;
        using difference_type = std::ptrdiff_t;
        using pointer = HashMember* const*;
        using reference = HashMember* const&;

        DLLLOCAL iterator() = default;
        DLLLOCAL explicit iterator(HashMember* m) : m(m) {
        }

        DLLLOCAL HashMember* const& operator*() const {
            return m;
        }

        DLLLOCAL iterator& operator++() {
            m = m->next;
            return *this;
        }

        DLLLOCAL bool operator==(const iterator& o) const {
            return m == o.m;
        }

        DLLLOCAL bool operator!=(const iterator& o) const {
            return m != o.m;
        }

    private:
        HashMember* m = nullptr;
    };
    using const_iterator = iterator;

    DLLLOCAL qhlist_t() = default;
    DLLLOCAL qhlist_t(const qhlist_t&) = delete;
    DLLLOCAL qhlist_t& operator=(const qhlist_t&) = delete;

    DLLLOCAL iterator begin() const {
        return iterator(head);
    }

    DLLLOCAL iterator end() const {
        return iterator();
    }

    DLLLOCAL bool empty() const {
        return !head;
    }

    //! the first member, or nullptr
    DLLLOCAL HashMember* front() const {
        return head;
    }

    //! the last member, or nullptr
    DLLLOCAL HashMember* back() const {
        return tail;
    }

    //! adds \a m, which is in no list, at the end
    DLLLOCAL void push_back(HashMember* m) {
        m->prev = tail;
        m->next = nullptr;
        if (tail) {
            tail->next = m;
        } else {
            head = m;
        }
        tail = m;
    }

    //! removes \a m from the list; its links are left as they were
    DLLLOCAL void unlink(HashMember* m) {
        if (m->prev) {
            m->prev->next = m->next;
        } else {
            assert(head == m);
            head = m->next;
        }
        if (m->next) {
            m->next->prev = m->prev;
        } else {
            assert(tail == m);
            tail = m->prev;
        }
    }

    //! moves \a m, which is in the list, to its end
    DLLLOCAL void moveToBack(HashMember* m) {
        if (m != tail) {
            unlink(m);
            push_back(m);
        }
    }

    //! forgets all members, which the caller deletes
    DLLLOCAL void reset() {
        head = tail = nullptr;
    }

private:
    HashMember* head = nullptr;
    HashMember* tail = nullptr;
};

//! The key map of a hash: a hash table of its members, chained through the members themselves (qore#7272)
/** The table is an array of buckets, each the first member of a chain linked by HashMember::bucket_next; the bucket
    of a member is the hash of its key masked to the number of buckets, a power of 2 that is at least the number of
    members.  A search compares the hashes of the members of the bucket before their keys.

    A map hashes its keys with the member hash (qore_hash_lookup_key::hashBytes()), which the JIT and qcc also
    compute in advance for constant keys.  Keys chosen to share that hash would otherwise make the chain of their
    bucket, and so every search of it, as long as the number of keys; an insert that would make a chain longer than
    MaxChain therefore hardens the map instead (qore#7557): it hashes all of its keys again with a keyed hash
    (qore_hash_keyed_bytes()) whose key is secret and created once per process, and keeps doing so for every key that
    it is given, ignoring a hash computed in advance.  A search of a map that is not hardened never changes it, so
    that a hash can be searched by several threads at once; only an insert, which has the hash to itself, hardens it.

    The map does not own its members; only its bucket array is allocated, so inserting a member can fail
    (std::bad_alloc) only when the array grows, before anything is changed, and removing one cannot fail; a
    hardening that cannot allocate its array changes nothing, and the next insert that would make a chain longer than
    MaxChain tries it again.  The map reads the hash of the members of a bucket when it searches it, so a member is
    removed from the map before it is deleted while code can search the map; when no code can (the release of a
    hash), the map is cleared after the members are deleted, without reading them (qore#7194, qore#7265).
*/
class DLLLOCAL qore_hash_member_map {
public:
    //! the number of buckets that a map gets when its first member is inserted
    static constexpr size_t MinBuckets = 8;

    //! the longest chain of a bucket that an insert makes in a map that is not hardened (qore#7557)
    /** with keys that do not share hashes, a chain of this length is practically impossible at the load of the map
        (at most one member per bucket on average), so only such keys harden a map
    */
    static constexpr size_t MaxChain = 16;

    DLLLOCAL qore_hash_member_map() = default;
    DLLLOCAL qore_hash_member_map(const qore_hash_member_map&) = delete;
    DLLLOCAL qore_hash_member_map& operator=(const qore_hash_member_map&) = delete;

    DLLLOCAL ~qore_hash_member_map() {
        delete[] buckets;
    }

    DLLLOCAL size_t size() const {
        return count;
    }

    DLLLOCAL bool empty() const {
        return !count;
    }

    //! the number of buckets (0 before the first member is inserted)
    DLLLOCAL size_t bucket_count() const {
        return buckets ? mask + 1 : 0;
    }

    //! true if the map hashes its keys with the keyed hash (qore#7557)
    DLLLOCAL bool isHardened() const {
        return hardened;
    }

    //! true if the members of both maps keep hashes of the same kind, so that the hash that a member of one keeps
    //! finds its key in the other
    DLLLOCAL bool sameKind(const qore_hash_member_map& other) const {
        return hardened == other.hardened;
    }

    //! returns the hash of the key of \a len bytes at \a str in this map: the hash that its member keeps
    DLLLOCAL size_t hashOf(const char* str, size_t len) const {
        return hardened ? qore_hash_keyed_bytes(str, len) : qore_hash_lookup_key::hashBytes(str, len);
    }

    //! returns the member with the key \a k, or nullptr
    /** The bucket of the hash of \a k is searched first: it is the bucket of the key in a map that is not hardened.
        In a hardened map it is another bucket, whose members cannot match unless one has the key (it then is the
        member of the key), and the key is searched with its keyed hash after it; the search of a map that is not
        hardened is therefore the same as before qore#7557 but for one test when the key is not found.
    */
    DLLLOCAL HashMember* find(const qore_hash_lookup_key& k) const {
        if (!count) {
            return nullptr;
        }
        for (HashMember* m = buckets[k.hash & mask]; m; m = m->bucket_next) {
            if (m->matches(k)) {
                return m;
            }
        }
        if (hardened) [[unlikely]] {
            return findKeyed(k.str, k.len);
        }
        return nullptr;
    }

    //! inserts \a m, whose key the map does not have and whose hash is the member hash of its key (or, in a map that
    //! is not hardened, any hash: debug functions simulate shared hashes so); throws std::bad_alloc, with nothing
    //! changed, if the bucket array cannot grow
    /** The map is hardened if the insert would make the chain of the bucket of \a m longer than MaxChain; the hash
        of \a m is the keyed hash of its key then, as in any hardened map.
    */
    DLLLOCAL void insert(HashMember* m) {
        if (count >= bucket_count()) {
            grow();
        }
        if (hardened) [[unlikely]] {
            m->hash = qore_hash_keyed_bytes(m->key.data(), m->key.size());
        }
        HashMember** b = &buckets[m->hash & mask];
        if (*b && !hardened && chainFull(*b)) [[unlikely]] {
            if (tryHarden()) {
                m->hash = qore_hash_keyed_bytes(m->key.data(), m->key.size());
                b = &buckets[m->hash & mask];
            }
        }
        m->bucket_next = *b;
        *b = m;
        ++count;
    }

    //! inserts \a m, a copy of a member of a map of the same kind with no fewer buckets than this map, whose key
    //! this map does not have, without checking the length of its chain: the chain is not longer than the chain of
    //! the member in that map (qore#7557); throws std::bad_alloc, with nothing changed, if the bucket array cannot
    //! grow
    DLLLOCAL void insertCopy(HashMember* m) {
        if (count >= bucket_count()) {
            grow();
        }
        HashMember*& b = buckets[m->hash & mask];
        m->bucket_next = b;
        b = m;
        ++count;
    }

    //! removes \a m, which is in the map
    DLLLOCAL void erase(HashMember* m) {
        assert(count);
        HashMember** p = &buckets[m->hash & mask];
        while (*p != m) {
            assert(*p);
            p = &(*p)->bucket_next;
        }
        *p = m->bucket_next;
        --count;
    }

    //! forgets all members and frees the bucket array; reads no member; the map is not hardened after it
    DLLLOCAL void clear() {
        delete[] buckets;
        buckets = nullptr;
        mask = 0;
        count = 0;
        hardened = false;
    }

    //! makes room for \a n members without a rehash; throws std::bad_alloc, with nothing changed, if the bucket array
    //! cannot be allocated
    /** not inline, so that the code of its callers (ex: the IR interpreter's hash literals) stays small
    */
    QORE_HASH_NOINLINE DLLLOCAL void reserve(size_t n);

    //! makes this map, which has no members, hash its keys as \a src does, for a copy of the members of \a src
    /** a copy of a hardened map is hardened, so that it takes the keyed hashes of the members
    */
    DLLLOCAL void copyKind(const qore_hash_member_map& src) {
        assert(!count);
        hardened = src.hardened;
    }

    //! returns the index of the bucket of \a m, which is in the map (for debug functions)
    DLLLOCAL size_t bucketIndex(const HashMember* m) const {
        assert(count);
        return m->hash & mask;
    }

    //! returns the length of the longest chain of a bucket (for debug functions)
    DLLLOCAL size_t maxChain() const;

private:
    HashMember** buckets = nullptr;
    //! the number of buckets - 1, if the array is allocated
    size_t mask = 0;
    size_t count = 0;
    //! true if the keys are hashed with the keyed hash (qore#7557)
    bool hardened = false;

    //! the number of buckets for \a n members: the smallest power of 2 that is at least \a n and MinBuckets
    DLLLOCAL static size_t bucketsFor(size_t n) {
        // a power of 2 of at least n exists
        assert(n <= (std::numeric_limits<size_t>::max() >> 1) + 1);
        size_t b = MinBuckets;
        while (b < n) {
            b <<= 1;
        }
        return b;
    }

    //! true if the chain that starts with \a m has MaxChain members, so that another one would make it too long
    DLLLOCAL static bool chainFull(const HashMember* m) {
        size_t n = 1;
        for (m = m->bucket_next; m; m = m->bucket_next) {
            if (++n == MaxChain) {
                return true;
            }
        }
        return false;
    }

    //! doubles the buckets, or allocates the first ones; not inline, so that insert() stays small
    QORE_HASH_NOINLINE DLLLOCAL void grow();

    //! moves the members to a new bucket array of \a n buckets, a power of 2 that is at least the number of members
    QORE_HASH_NOINLINE DLLLOCAL void rehash(size_t n);

    //! returns the member with the key of \a len bytes at \a str in a hardened map, or nullptr
    QORE_HASH_NOINLINE DLLLOCAL HashMember* findKeyed(const char* str, size_t len) const;

    //! hardens the map (harden()); returns false, with nothing changed, if it cannot, so that a later insert tries
    //! again
    QORE_HASH_NOINLINE DLLLOCAL bool tryHarden();

    //! moves the members to a new bucket array of as many buckets with the keyed hashes of their keys; throws an
    //! exception, with nothing changed, if the array cannot be allocated or the secret key cannot be created
    DLLLOCAL void harden();
};

//! the searches of the key map of a hash are functions of their own with the map's search inlined (qore#7265)
/** The key to look up is passed to the map's search in registers, not in memory, and its hash and bytes are compared
    inline; the searches are not inlined into their callers, so that the code of their callers stays small
*/
#if defined(__GNUC__) || defined(__clang__)
#define QORE_HASH_FIND_INLINE __attribute__((noinline, flatten))
#else
#define QORE_HASH_FIND_INLINE
#endif

// QoreHashIterator private class
class DLLLOCAL qhi_priv {
public:
    //! the current member, if val is true
    HashMember* i = nullptr;
    bool val;

    DLLLOCAL qhi_priv() : val(false) {
    }

    DLLLOCAL qhi_priv(const qhi_priv& old) : i(old.i), val(old.val) {
    }

    DLLLOCAL bool valid() const {
        return val;
    }

    DLLLOCAL bool next(const qhlist_t& ml) {
        //printd(0, "qhi_priv::next() this: %p val: %d\n", this, val);
        i = val ? i->next : ml.front();
        val = i;
        return val;
    }

    DLLLOCAL bool prev(const qhlist_t& ml) {
        i = val ? i->prev : ml.back();
        val = i;
        return val;
    }

    DLLLOCAL void reset() {
        val = false;
    }

    DLLLOCAL static qhi_priv* get(HashIterator& i) {
        return i.priv;
    }
};

class DLLLOCAL qore_hash_private {
public:
    //! the members in their order
    qhlist_t member_list;
    //! the members by key
    qore_hash_member_map hm;
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
    //! true while the values that were saved in a hash of thread-local data during its release are freed: nothing can
    //! be saved in it then (see clear() and checkClearingSave())
    bool refuse_saves = false;
    //! the estimated memory of the members (see sbMemberSize()); always kept, so that it is right when charged
    size_t sb_bytes = 0;
    //! the sandbox memory charge of the members (see design/sandbox-resource-limits.md); synced with sb_bytes
    QoreSandboxChargeRecord sb;

    DLLLOCAL qore_hash_private() {
    }

    //! the estimated memory of a member: the member (which is its node in the member list and in the key map), its
    //! key and its bucket in the key map (qore#7272)
    DLLLOCAL static size_t sbMemberSize(const HashMember* m) {
        return sizeof(HashMember) + m->key.size() + sizeof(HashMember*);
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

    //! frees the members of a hash that has no other reference and that no code can reach (an object's members when
    //! the object is released)
    DLLLOCAL static void clear(QoreHashNode& h, ExceptionSink* xsink, bool reverse = false) {
        assert(h.is_unique());
        h.priv->freeMembers(xsink, reverse);
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

    //! returns the member with the key \a key, or nullptr
    /** An empty hash is not searched, so that its key is not hashed (qore#7194)
    */
    QORE_HASH_FIND_INLINE DLLLOCAL HashMember* findMember(const char* key) const {
        return hm.empty() ? nullptr : hm.find(qore_hash_lookup_key(key));
    }

    //! returns the member with the key \a key of \a len bytes, or nullptr
    QORE_HASH_FIND_INLINE DLLLOCAL HashMember* findMember(const char* key, size_t len) const {
        return hm.empty() ? nullptr : hm.find(qore_hash_lookup_key(key, len));
    }

    //! returns the member with the key \a key, or nullptr
    QORE_HASH_FIND_INLINE DLLLOCAL HashMember* findMember(const qore_hash_lookup_key& key) const {
        return hm.empty() ? nullptr : hm.find(key);
    }

    //! returns the member with the key \a key, whose hash is \a hash, or nullptr
    QORE_HASH_FIND_INLINE DLLLOCAL HashMember* findMemberPrehashed(const char* key, size_t hash) const {
        return hm.empty() ? nullptr : hm.find(qore_hash_lookup_key(key, strlen(key), hash));
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
        assert(size() == other.size());
        const HashMember* m = member_list.front();
        // the hashes have as many members (the callers check it), so the walk ends at the end of both lists; it counts
        // the members instead of testing both lists' ends, which costs a test less per member (qore#7272)
        const HashMember* om = other.member_list.front();
        for (size_t n = size(); n; --n, m = m->next, om = om->next) {
            if (!qore_key_equal(m->key.data(), m->key.size(), om->key.data(), om->key.size())) [[unlikely]] {
                break;
            }
            // the keys are unique, so the member with the same key is the member at the same place
            if (!value_equal(m->val, om->val)) [[unlikely]] {
                return false;
            }
        }
        // the key is searched with the hash that the member keeps if the key maps hash their keys the same way
        // (qore#7265); else it is hashed again (one of the maps is hardened, qore#7557)
        bool same_kind = hm.sameKind(other.hm);
        for (; m; m = m->next) {
            const HashMember* found = same_kind
                ? other.findMember(qore_hash_lookup_key(m->key.data(), m->key.size(), m->hash,
                    qore_hash_lookup_key::stored_hash_t()))
                : other.findMember(qore_hash_lookup_key(m->key.data(), m->key.size()));
            if (!found || !value_equal(m->val, found->val)) {
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
        const HashMember* m = findMember(key);
        if (m) {
            exists = true;
            return m->val.refSelf();
        }

        exists = false;
        return QoreValue();
    }

    //! returns the value of the key \a key of \a len bytes with a reference, or no value; \a exists is set if the
    //! key exists
    DLLLOCAL QoreValue getReferencedKeyValueIntern(const char* key, size_t len, bool& exists) const {
        const HashMember* m = findMember(key, len);
        if (m) {
            exists = true;
            return m->val.refSelf();
        }

        exists = false;
        return QoreValue();
    }

    //! returns the value of the key with a reference, or no value; \a exists is set if the key exists
    DLLLOCAL QoreValue getReferencedKeyValueIntern(const qore_hash_lookup_key& key, bool& exists) const {
        const HashMember* m = findMember(key);
        if (m) {
            exists = true;
            return m->val.refSelf();
        }

        exists = false;
        return QoreValue();
    }

    DLLLOCAL QoreValue getReferencedKeyValuePrehashedIntern(
            const char* key, size_t hash) const {
        assert(key);
        const HashMember* m = findMemberPrehashed(key, hash);
        return m ? m->val.refSelf() : QoreValue();
    }

    DLLLOCAL int64 getKeyAsBigInt(const char* key, bool &found) const {
        assert(key);
        const HashMember* m = findMember(key);

        if (m) {
            found = true;
            return m->val.getAsBigInt();
        }

        found = false;
        return 0;
    }

    DLLLOCAL bool getKeyAsBool(const char* key, bool& found) const {
        assert(key);
        const HashMember* m = findMember(key);

        if (m) {
            found = true;
            return m->val.getAsBool();
        }

        found = false;
        return false;
    }

    DLLLOCAL bool existsKey(const char* key) const {
        assert(key);
        return findMember(key) != nullptr;
    }

    DLLLOCAL bool existsKeyValue(const char* key) const {
        assert(key);
        const HashMember* m = findMember(key);
        return m && !m->val.isNothing();
    }

    //! returns the member of \a key, which is created if the hash does not have it
    /** The key is hashed once, for the search and for a new member; not inline, so that the key and its hash are
        not on the stack frames of its inline callers, such as the IR interpreter's (qore#7265)
    */
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((noinline))
#endif
    DLLLOCAL HashMember* findCreateMember(const char* key);

    // as above; "created" is set to true if the member did not exist and was created here
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((noinline))
#endif
    DLLLOCAL HashMember* findCreateMember(const char* key, bool& created);

    //! returns the member of \a key, whose hash is \a hash, which is created if the hash does not have it
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((noinline))
#endif
    DLLLOCAL HashMember* findCreateMemberPrehashed(const char* key, size_t hash);

    //! creates the member of the key \a key, which the hash does not have
    /** The functions that create members are not inline, so that their code is not copied into the inline code of
        their callers (qore#7265)
    */
    // not inline: the key and its hash would otherwise be on the stack frames of its inline callers, such as the IR
    // interpreter's (via setKeyValueKnownAbsent())
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((noinline))
#endif
    DLLLOCAL HashMember* createMemberKnownAbsent(const char* key);

    //! creates the member of the key \a key, which the hash does not have
    DLLLOCAL HashMember* createMemberKnownAbsent(const qore_hash_lookup_key& key);

    //! creates a member with the key of the member \a old of another hash, which this hash does not have, without
    //! hashing the key (qore#7265)
    /** @param old the member of the other hash, whose key map is of the same kind as the key map of this hash (see
        reserveForCopy())
        @param copy_layout the value that reserveForCopy() returned: true if the key map of this hash has no fewer
        buckets than the key map of the other hash, so that the chains of its buckets need no check (qore#7557)
    */
    DLLLOCAL HashMember* createMemberKnownAbsent(const HashMember& old, bool copy_layout);

    //! adds the new member \a m, whose key the hash does not have, at the end of the hash; takes over \a m
    DLLLOCAL HashMember* addMemberKnownAbsent(HashMember* m);

    //! sets the value of a new member \a om, which has no value, without type checks; takes over the reference of \a v
    DLLLOCAL void setNewMemberValue(HashMember* om, QoreValue v) {
        assert(om->val.isNothing());
        om->val = v;
        if (needs_scan(v)) {
            incScanCount(1);
        }
    }

    DLLLOCAL QoreValue& getValueRef(const char* key) {
        return findCreateMember(key)->val;
    }

    //! removes the member \a om from the key map and the member list, and deletes it
    /** The value is not released: the caller takes it first and releases it after this call, as releasing it can
        run code that changes the hash
    */
    DLLLOCAL void internDeleteKey(HashMember* om) {
        hm.erase(om);
        member_list.unlink(om);
        sbRemove(om);
        delete om;
    }

    DLLLOCAL void deleteKey(const char* key, ExceptionSink *xsink) {
        assert(key);

        HashMember* om = findMember(key);
        if (!om) {
            return;
        }

        // the value is taken and the member deleted before the value is released, as releasing it can run code
        // that changes the hash
        AbstractQoreNode* n = om->val.assignNothing();
        internDeleteKey(om);

        // dereference node if present
        if (n) {
            if (needs_scan(n))
                incScanCount(-1);

            if (n->getType() == NT_OBJECT)
                reinterpret_cast<QoreObject*>(n)->doDelete(xsink);
            n->deref(xsink);
        }
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

        HashMember* om = findMember(key);
        if (!om) {
            exists = false;
            return QoreValue();
        }
        exists = true;

        QoreValue rv = om->val;
        internDeleteKey(om);

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

    //! returns a copy of the hash converted to another type: a new hash, which can be changed also when this hash was
    //! freed under a weak reference (qore#7199; see copyFreedState())
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
        qore_hash_private& hp = *h->priv;
        bool copy_layout = hp.reserveForCopy(*this);
        // copy all members to new object
        for (const HashMember* i : member_list) {
            HashMember* om = hp.createMemberKnownAbsent(*i, copy_layout);
            hp.setNewMemberValue(om, copy_strip_complex_types(i->val));
        }
        copyFreedState(hp);
        return h;
    }

    //! A copy of a freed hash cannot be changed either (qore#7163)
    /** A hash freed under weak references is held by its weak holders (see QoreContainerWeakRefs), so a new
        reference to it is never its only one, and changing the hash through that reference copies it; the copy reads
        as empty, as the hash does, and changing it raises \c HASH-ERROR, as changing the hash itself does.  A value
        computed from it (its sum with another hash) is a new hash, and so are its conversion to another hash type (a
        cast or the hash given to a variable or parameter of another type: getPlainHash(), newComplexHashFromHash())
        and a new expression made from it (makeNewValid()), which can be changed (qore#7199).

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

    //! copies the members to the new hash \a h, which has none
    DLLLOCAL void copyIntern(qore_hash_private& h) const {
        bool copy_layout = h.reserveForCopy(*this);
        // copy all members to new object
        for (const HashMember* i : member_list) {
            h.setNewMemberValue(h.createMemberKnownAbsent(*i, copy_layout), i->val.refSelf());
        }
    }

    //! prepares this new hash, which has no members, for the members of \a src: the key map takes them without a
    //! rehash (qore#7265)
    /** the key map of a hash that grows from empty doubles its buckets from qore_hash_member_map::MinBuckets;
        reserved, it has as many buckets as the next power of 2 from the number of keys, which is not more; the key
        map hashes its keys as the key map of \a src does (it is hardened if that one is, qore#7557), so that it takes
        the hashes of the members of \a src

        @return true if the chains of the buckets of the copy need no check (createMemberKnownAbsent()): its key map
        is hardened, or it has no fewer buckets than the key map of \a src, so that the chain of a member is not
        longer than in \a src
    */
    DLLLOCAL bool reserveForCopy(const qore_hash_private& src) {
        assert(member_list.empty());
        assert(hm.empty());
        if (src.hm.empty()) {
            return true;
        }
        hm.reserve(src.hm.size());
        hm.copyKind(src.hm);
        return hm.isHardened() || hm.bucket_count() >= src.hm.bucket_count();
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
        qore_hash_private& hp = *h->priv;
        bool copy_layout = hp.reserveForCopy(*this);
        size_t count = 0;
        for (const HashMember* i : member_list) {
            if (++count % 100 == 0 && qore_check_cancel(xsink, "hash expression evaluation")) {
                return nullptr;
            }
            ValueEvalOptimizedRefHolder value(i->val, xsink);
            if (*xsink) {
                return nullptr;
            }
            // the keys of this hash are unique, and the new hash has none of them; the member is created here, so a
            // rejected assignment removes it again
            hash_assignment_priv ha(hp, hp.createMemberKnownAbsent(*i, copy_layout), true);
            ha.assign(value.takeReferencedValue(), xsink);
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
    /** Only for a hash that no code can reach any more, or whose values' release runs no code (see clear()): no code
        searches the key map while the members are deleted, so it is cleared after them, in one pass
    */
    // always inline, as before qore#7265, in the release of a hash and of an object's members
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((always_inline))
#endif
    DLLLOCAL void freeMembers(ExceptionSink* xsink, bool reverse = false) {
        if (reverse) {
            for (HashMember* m = member_list.back(); m;) {
                HashMember* p = m->prev;
                m->val.discard(xsink);
                delete m;
                m = p;
            }
        } else {
            for (HashMember* m = member_list.front(); m;) {
                HashMember* n = m->next;
                m->val.discard(xsink);
                delete m;
                m = n;
            }
        }

        member_list.reset();
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
        // removed from the key map too, so that the size of the hash stays that of its member list (qore#7272)
        p->hm.erase(member);
        p->member_list.unlink(member);
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

    //! Empties a hash that stays in use (a thread's data)
    /** Unlike derefImpl(), the hash is not marked as freed: marking it made a hash that is used again, such as the
        thread-local data of a pooled thread, reject changes, and made its eventual release look like the release of
        a freed hash.

        Code that runs when a value is freed (a destructor) can read and change the hash (ex: thread-local data), in
        two phases (qore#7265):
        - first the values that the hash has when the clearing starts are freed in order: their destructors can read
          the values not freed yet and save values;
        - then the values saved in the first phase are freed in the same way, while nothing can be saved in the hash
          (checkClearingSave()): the clearing ends also when destructors save values again and again, as the release
          of thread_local variables does (see thread_local_vars_finalizing()).

        Each value is taken, and its member deleted (from the key map first), before the value is freed, as
        deleteKey() does, so that no member or iterator is held while code runs, and each member is visited once;
        nothing is allocated.  If no value's release can run code (freeRunsNoCode()), the members are deleted as
        when the hash is freed.

        Values saved through the C++ API (QoreProgram::getThreadData()) during the release are not refused: a module
        must not save thread-local data from code that runs when thread-local data is released.
    */
    DLLLOCAL void clear(ExceptionSink* xsink);

    //! raises \c DESTRUCTOR-ERROR and returns -1 if nothing can be saved in this hash of thread-local data now (see
    //! clear())
    DLLLOCAL int checkClearingSave(ExceptionSink* xsink) const;

    //! returns true if freeing \a v runs no code (a destructor): a value without a node, or a string, date, binary,
    //! number or null (see clear())
    DLLLOCAL static bool freeRunsNoCode(const QoreValue& v);

    //! freeMembers() for clear(), not inline, so that freeMembers() stays inline in its other callers (the release
    //! of a hash and of an object's members)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((noinline))
#endif
    DLLLOCAL void freeMembersOutOfLine(ExceptionSink* xsink);


    //! takes the value of the first member, deletes the member and frees the value, which can run code that changes
    //! the hash (see clear())
    /** The member is deleted at once, before its value is freed, with every allocator (qore#7272).

        Before qore#7272, a member had a node of its own in the member list, which was left between the memory of one
        value and that of the next when the member was deleted; with glibc, freeing those nodes at once or in their
        order merged the memory freed into chunks that grew until glibc trimmed the heap at almost every free, so
        qore#7265 freed them at the end of the release in a scattered order (with glibc only).  A member is now its
        node in the list and in the key map, so nothing is left to free after the values.  Measured (Ir or
        instructions per value, delete_all_thread_data() of N objects (qo), of N objects whose destructors save a
        value each (qd) and of N strings and ints (qp)):
        - glibc 2.43 (canoe, callgrind, ast/jit/aot3): qd -2296/-2394/-3008, qo -1019/-1086/-1031, qp -850/-786/-859
          against the release of qore#7265; deleting the members at once measured qd -55/-189/+151 and qo
          -12/+39/-12 (ast/ir/jit) against deleting them in the scattered order of qore#7265 at the end, within the
          differences between the layouts of the heap;
        - musl (Alpine, callgrind, ast/ir/jit): qd -2167/-2195/-2420, qo -698/-748/-706, qp -598/-583/-610;
        - macOS (instructions retired, minimum of 9 runs, ast): qd -2852, qo -733, qp -761.
        Do not defer or reorder the deletes without measuring again on each allocator.
    */
    DLLLOCAL void takeFreeMemberForClear(ExceptionSink* xsink);

    //! the number of members: the members in the key map, which are those of the member list (except in
    //! freeMembers(), which empties both at its end)
    DLLLOCAL size_t size() const {
        assert(hm.empty() == member_list.empty());
        return hm.size();
    }

    DLLLOCAL bool empty() const {
        assert(hm.empty() == member_list.empty());
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

    //! Returns the hash that a new expression gives (new hash<...>(h)): a new hash, which can be changed also when the
    //! hash it is made from was freed under a weak reference (qore#7199)
    /** @param h the hash made by the new expression, which the caller owns, or nullptr if an exception was raised;
        a hash that cannot be changed is the hash made from or a copy of it: it is copied if another holder shares it
    */
    DLLLOCAL static QoreHashNode* makeNewValid(QoreHashNode* h) {
        if (!h || h->priv->valid) [[likely]] {
            return h;
        }
        if (!h->is_unique()) {
            ReferenceHolder<QoreHashNode> holder(h, nullptr);
            h = h->priv->copy();
        }
        h->priv->valid = true;
        return h;
    }

    DLLLOCAL static QoreHashNode* getPlainHash(QoreHashNode* h) {
        if (!h->priv->hashdecl && !h->priv->complexTypeInfo) {
            return h;
        }
        // no exception is possible
        ReferenceHolder<QoreHashNode> holder(h, nullptr);
        // the hash converted to a plain hash is a new hash, which can be changed also when the hash was freed under
        // a weak reference (qore#7199; see copyFreedState())
        QoreHashNode* rv = h->priv->copy(true);
        rv->priv->valid = true;
        return rv;
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
