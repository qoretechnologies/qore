/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreReferenceBind.h

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_INTERN_QOREREFERENCEBIND_H
#define _QORE_INTERN_QOREREFERENCEBIND_H

// included after <qore/Qore.h>, which declares QoreValue
#include "qore/intern/LexicalSemanticPolicy.h"

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <optional>
#include <type_traits>
#include <vector>

/*  Binding reference elements outside the lvalue locks of a store (qore#7391)

    A value checked against a type that contains reference<T> binds each reference element whose variable has a wider
    declared type: the referenced lvalue is resolved and locked, its value is checked or converted to T and can be
    written back.  A store checks its value with the lock of its target held, so binding there could lock a second
    lvalue under the first one (two threads that do it in the opposite order deadlock) or the same one again.

    A store whose target's type can bind a reference (QoreTypeInfo::mayBindReference(), a cached byte) and whose
    helper holds an lvalue lock checks its value with a QoreReferenceBindProbe installed: the check records each
    reference that it would bind with the reference type instead of binding it.  If anything was recorded, nothing is
    stored: the helper rolls back the entries created for the store and hands the value and the references to the
    QoreBindRetry of the store, and returns -1 with no exception.  The store site then leaves its helper, which
    releases every lock, and QoreBindRetry::again() binds the references with no lock held and has the store made
    again.  The second check accepts the references bound already without binding them.

    The normal path of a store costs the cached byte of the type: a store site whose helper can be made again enables
    the retry of the helper (LValueHelper::enableBindRetry()), whose QoreBindRetry is a null pointer until a store
    records references, and tests LValueHelper::isBindPending() only when its store failed.  A pending store is made
    again out of line: LValueHelper::releaseForBindRetry() takes the retry and releases the helper with its locks, as
    its destructor does, and the retries are made with helpers of their own (qore_store_bind_retry_rounds()).

    The keys and indices of the path of an AST store are evaluated once.  The steps of the first round keep the keys
    that they evaluate in the helper (LValueHelper::retainBindKey()): a key that the step holds a reference to is
    handed to the values that the helper releases after the lvalue locks (a move: no reference is taken, as the step
    would have released its key), and a borrowed key is referenced only before a check that can record references
    (LValueHelper::holdBindKeys()).  When the store is made again, the keys are given to a QoreBindKeys with references
    of its own (LValueHelper::transferBindKeys(): by deferBinds() for a store site that has one, by
    releaseForBindRetry() for one that has none), and a retry resolves the path with them instead of evaluating the key
    expressions again.  They cannot be kept only when the store is made again: the steps release their keys before the
    store checks its value.  The keys of the path of an IR or JIT/AOT instruction are evaluated before the store, which
    resolves the path with them for each round.

    A reference at the root of the path (a variable that holds a reference, or a reference given as the target) is
    resolved again by a retry, as by any other use of the reference.  The keys of the path of a reference are evaluated
    when the reference is made, so its resolution evaluates no key expression; what runs again is the code that the
    resolution of its path runs, such as the write gate of a member, and only when the store is made again: a store
    that records no reference resolves its target once.

    An assignment (its helper is made with for_assignment true, which keeps the keys and enables the retry; a store
    that is not an assignment uses LValueHelper::BindRetryStore), whose normal path is that of a store with no retry
    and the test of a byte after a failed store:
        LValueHelper h(exp, xsink, false, true);
        if (!h) {
            return ...;
        }
        if (h.assign(<value>, ...)) {
            if (h.isBindPending()) [[unlikely]] {
                return <cold out-of-line function: h.releaseForBindRetry(retry, &keys), then the retries>;
            }
            return ...;
        }
        return ...;

    A store site that makes each round in a loop (an operation of its own that is not an assignment) uses a
    QoreBindRetry of its own:
        QoreBindKeys keys(xsink);
        QoreBindRetry retry;
        do {
            LValueHelper h(exp, xsink, true, retry, keys);
            if (!h) {
                return ...;
            }
            if (h.assign(retry.hasValue() ? retry.takeValue() : <value>, ...)) {
                continue;
            }
            return ...;
        } while (retry.again());
        return ...;

    A store whose target is resolved with no key expression to evaluate (a variable, a member, a reference, or the path
    of an IR or JIT/AOT instruction, whose keys are evaluated already) uses qore_store_with_bind_retry(),
    qore_store_with_bind_retry_lazy() or qore_reference_store_with_bind_retry() (Variable.h), which make the first round
    inline and the retries out of line.
    A merge into an object binds the references of its values before it takes the object's lock
    (qore_object_private::mergeBindReferences()).

    Qore code that a check made with a probe runs (the member initializers of a hashdecl that a hash is converted to)
    runs with the probe suspended and with binds refused (QoreReferenceBindCodeScope): a reference that it binds would
    be bound under the lvalue lock of the outer store, so it raises REFERENCE-BIND-ERROR, and the code records nothing in
    the probe of the outer store.

    A store made with a helper that has no QoreBindRetry and that records a reference fails closed: it raises
    REFERENCE-BIND-ERROR and leaves its target unchanged.
*/

class ReferenceNode;
class QoreTypeInfo;
class ExceptionSink;
class LValueHelper;
class QoreBindRetry;

//! A reference element and the type that a bind applies to its lvalue (qore#7391)
struct QoreReferenceBindPair {
    //! the reference; the owner of the pair holds a reference to it
    ReferenceNode* ref;
    //! the type of the reference type that the check accepted the reference as (T of reference<T>)
    const QoreTypeInfo* type;
    //! the lexical semantic policy of the check that found the reference
    LexicalSemanticPolicy policy;
    //! true if the check used the lexical policy, false for the default policy
    bool lexical;
};

//! A list of reference elements, each held with a reference of the list (qore#7391)
class QoreReferenceBindPairList {
public:
    DLLLOCAL QoreReferenceBindPairList() = default;

    //! releases the references left; release() releases them with an exception sink on the normal paths
    DLLLOCAL ~QoreReferenceBindPairList();

    QoreReferenceBindPairList(const QoreReferenceBindPairList&) = delete;
    QoreReferenceBindPairList& operator=(const QoreReferenceBindPairList&) = delete;

    //! adds the reference and the type, taking a reference to \a r
    DLLLOCAL void add(ReferenceNode* r, const QoreTypeInfo* ti, LexicalSemanticPolicy policy, bool lexical);

    //! returns true if the list has the reference with the type
    DLLLOCAL bool contains(const ReferenceNode* r, const QoreTypeInfo* ti) const;

    //! moves the pairs of this list to the end of \a other
    DLLLOCAL void moveTo(QoreReferenceBindPairList& other);

    //! releases the references of the list and empties it; called with no lvalue lock held
    DLLLOCAL void release(ExceptionSink* xsink);

    //! hands the references of the list to \a lvh, which releases them after its locks, and empties the list
    DLLLOCAL void releaseWith(LValueHelper& lvh);

    DLLLOCAL bool empty() const {
        return pairs.empty();
    }

    DLLLOCAL size_t size() const {
        return pairs.size();
    }

    DLLLOCAL const QoreReferenceBindPair& operator[](size_t i) const {
        return pairs[i];
    }

private:
    std::vector<QoreReferenceBindPair> pairs;
};

//! The state of a store whose check recorded reference elements to bind (qore#7391)
/** Allocated by the first round that records references (QoreBindRetry::setPending())
*/
struct QoreBindRetryState {
    //! the exception sink of the store
    ExceptionSink* xsink;
    //! the references that the last round recorded, which are bound before the store is made again
    QoreReferenceBindPairList pending;
    //! the references bound by the earlier rounds, which the check accepts without binding them again
    QoreReferenceBindPairList bound;
    //! the exception that the check raised after it recorded references, reported after they have been bound
    ExceptionSink error;
    //! the value of the last round, if has_value is set
    QoreValue value;
    //! the number of rounds that bound references
    unsigned rounds = 0;
    //! true if value holds the value of the last round
    bool has_value = false;
    //! true if the last round recorded references
    bool is_pending = false;

    DLLLOCAL explicit QoreBindRetryState(ExceptionSink* xsink) : xsink(xsink) {
    }
};

//! The retry of a store whose check recorded references to bind (qore#7391)
/** A null pointer until a round of the store records references: constructing it and testing it costs nothing more
    on the normal path.  Created by a store site that makes each round in a loop before the LValueHelper of the store,
    which it is given to, or allocated by a helper whose store site enabled the retry when its store records references
    (LValueHelper::enableBindRetry()); again() is called after the helper has been destroyed or released, with no
    lvalue lock held.
*/
class QoreBindRetry {
public:
    //! the number of rounds after which a store whose check keeps recording references fails
    static constexpr unsigned MaxRounds = 16;

    DLLLOCAL QoreBindRetry() = default;

    DLLLOCAL ~QoreBindRetry() {
        if (state) [[unlikely]] {
            release();
        }
    }

    QoreBindRetry(const QoreBindRetry&) = delete;
    QoreBindRetry& operator=(const QoreBindRetry&) = delete;

    //! returns true if the store has to be made again; binds the references recorded with no lvalue lock held
    /** Called after the helper of the store has been destroyed.  Returns false if the store did not record a
        reference (it succeeded or failed, and the caller reports that), or if a bind raised an exception, or if the
        check raised an exception after it recorded a reference: the exception is in the exception sink of the store
        then.
    */
    DLLLOCAL bool again() {
        return state ? againIntern() : false;
    }

    //! returns true if the last round recorded references and has to be made again (see again())
    /** For a store site whose operation can fail with no exception for this reason only, such as += through a
        default value
    */
    DLLLOCAL bool isPending() const {
        return state && state->is_pending;
    }

    //! returns true if the value of the last round is kept for the store (see takeValue())
    DLLLOCAL bool hasValue() const {
        return state && state->has_value;
    }

    //! returns the value of the last round, which the caller owns
    DLLLOCAL QoreValue takeValue();

    //! returns true if a reference recorded by an earlier round of this store was bound to \a ti
    DLLLOCAL bool isBound(const ReferenceNode* r, const QoreTypeInfo* ti) const {
        return state && state->bound.contains(r, ti);
    }

    //! records that the store has to be made again (LValueHelper)
    /** @param value the value of the store, which the retry takes; a value of an earlier round that the store site did
        not take is handed to \a lvh, which releases it after its locks
        @param pairs the references to bind, which the retry takes
        @param error the exception that the check raised after it recorded the references, which the retry takes
        @param lvh the helper of the store, which holds an lvalue lock
    */
    DLLLOCAL void setPending(QoreValue& value, QoreReferenceBindPairList& pairs, ExceptionSink& error,
            LValueHelper& lvh);

    //! takes the state of \a other, which has none afterwards; this retry must have none
    DLLLOCAL void takeFrom(QoreBindRetry& other) {
        assert(!state);
        state = other.state;
        other.state = nullptr;
    }

private:
    //! the state of a store that recorded references, allocated when needed
    QoreBindRetryState* state = nullptr;

    //! binds the references recorded and returns true if the store has to be made again
    DLLLOCAL bool againIntern();

    //! releases the value and the references held
    DLLLOCAL void release();
};

//! A key or an index of the path of an AST store that the store site keeps for a retry (qore#7391)
struct QoreBindKey {
    QoreValue value;
    //! true if the store site holds a reference to the value
    bool owned;
};

//! The keys and indices of the path of an AST store, kept for a retry of the store (qore#7391)
/** Declared by a store site that makes the store in rounds before its helper, and given to the LValueHelper of each
    round, or by the cold function that makes the retries of an assignment.  It is given the keys that the steps of
    the path of the first round kept in their helper, in the order that the steps took them, until a reference was
    followed (the path of a reference is resolved again: see the top of this file), with a reference of its own to
    each, when the store is made again (LValueHelper::transferBindKeys()).  A retry resolves the path with them.  The
    keys are released when the store site leaves, after the lvalue locks.

    Constructing it costs one store, and destroying it the test of one byte: the exception sink and the keys after the
    first BufSize are set only when they are used.
*/
class QoreBindKeys {
public:
    DLLLOCAL QoreBindKeys() = default;

    //! for a store site that has an exception sink at hand; the keys are released with the sink of the helper
    DLLLOCAL explicit QoreBindKeys(ExceptionSink* /* xsink */) {
    }

    DLLLOCAL ~QoreBindKeys() {
        // only keys that the store site holds a reference to are released; the keys of most paths are borrowed (the
        // constants of the expression, the values of variables) or need no reference (ints)
        if (release_bits) [[unlikely]] {
            release();
        }
    }

    QoreBindKeys(const QoreBindKeys&) = delete;
    QoreBindKeys& operator=(const QoreBindKeys&) = delete;

    //! keeps \a key; \a owned is true if the reference that the caller held for it is handed over
    /** @param xsink the exception sink that the keys held are released with
    */
    DLLLOCAL void add(const QoreValue& key, bool owned, ExceptionSink* xsink) {
        if (count < BufSize) [[likely]] {
            new (&reinterpret_cast<QoreValue*>(buf)[count]) QoreValue(key);
            // a value that needs no reference is not released
            if (owned && key.needsRef()) {
                this->xsink = xsink;
                release_bits |= static_cast<uint8_t>(1u << count);
            }
            ++count;
            return;
        }
        addMore(key, owned, xsink);
    }

    //! takes a reference to each key that is only borrowed
    /** Called when the keys are given to the retries of a store (LValueHelper::transferBindKeys(), the cold path): the
        helper that kept them releases its own references
    */
    DLLLOCAL void hold(ExceptionSink* xsink);

    //! returns the number of keys kept
    DLLLOCAL size_t size() const {
        return count;
    }

    //! returns the key at position \a i (0 = the key that the first step took), borrowed from this object
    DLLLOCAL QoreValue get(size_t i) const {
        return i < BufSize ? reinterpret_cast<const QoreValue*>(buf)[i] : (*more)[i - BufSize].value;
    }

    //! returns true once the first round has resolved the path, whose keys are kept here then
    DLLLOCAL bool isRecorded() const {
        return recorded;
    }

    //! records that the first round has resolved the path
    DLLLOCAL void setRecorded() {
        recorded = true;
    }

private:
    //! the number of keys held without an allocation
    static constexpr size_t BufSize = 4;
    //! the bit of release_bits set when the keys after the first BufSize are allocated
    static constexpr uint8_t MoreBit = 1u << BufSize;

    // the first three members are initialized together, with the stores merged
    //! the number of keys kept
    uint32_t count = 0;
    //! the keys of buf that the store site holds a reference to, one bit each, and MoreBit; 0 if nothing is released
    uint8_t release_bits = 0;
    //! true once the first round has resolved the path
    bool recorded = false;
    //! the exception sink that the keys held are released with; set when the first one is held
    ExceptionSink* xsink;
    //! the keys after the first BufSize, allocated for a long path only; valid if MoreBit is set in release_bits
    std::vector<QoreBindKey>* more;
    //! the first BufSize keys (the first count are valid)
    alignas(QoreValue) unsigned char buf[BufSize * sizeof(QoreValue)];

    //! keeps a key after the first BufSize
    DLLLOCAL void addMore(const QoreValue& key, bool owned, ExceptionSink* xsink);

    //! releases the keys that the store site holds and the keys allocated
    DLLLOCAL void release();
};

//! The probe of the type check of a store that holds an lvalue lock (qore#7391)
/** While installed, QTS_COMPLEXREF in QoreTypeSpec::acceptInputImpl() records the references that it would bind
    instead of binding them.  The probe is installed for the current thread and restores the probe that it replaced
    when it is destroyed.
*/
class QoreReferenceBindProbe {
public:
    //! installs the probe; \a retry gives the references that were bound already, if any
    /** @param verify true for the probe of a container operation whose values were checked before it (see
        qore_container_store_probe()): a reference that needs a bind and was not bound already is refused, and the
        check of the operation raises REFERENCE-BIND-ERROR before the operation stores the value
    */
    DLLLOCAL explicit QoreReferenceBindProbe(const QoreBindRetry* retry, bool verify = false);

    //! installs the probe; \a bound gives the references that were bound already, if any (qore#7391)
    /** For a store that binds the references of its values before it takes its lock, such as a merge into an object

        @param locked true if the thread holds an lvalue lock while the probe is installed: Qore code that the check
        runs then refuses to bind references (QoreReferenceBindCodeScope)
    */
    DLLLOCAL QoreReferenceBindProbe(const QoreBindRetry* retry, const QoreReferenceBindPairList* bound, bool locked);

    //! uninstalls the probe if it is still installed; the pairs left are released with no exception sink
    DLLLOCAL ~QoreReferenceBindProbe();

    QoreReferenceBindProbe(const QoreReferenceBindProbe&) = delete;
    QoreReferenceBindProbe& operator=(const QoreReferenceBindProbe&) = delete;

    //! uninstalls the probe; the pairs that it recorded are kept
    DLLLOCAL void uninstall();

    //! records that the check accepted \a r for the reference type of \a ti; accepted with no record if bound already
    /** @return false if the reference needs a bind and the probe verifies (no record is made), true if not
    */
    DLLLOCAL bool record(ReferenceNode* r, const QoreTypeInfo* ti, LexicalSemanticPolicy policy, bool lexical);

    //! returns true if a reference was recorded
    DLLLOCAL bool recorded() const {
        return !pairs.empty();
    }

    //! returns true if the thread holds an lvalue lock while the probe is installed
    DLLLOCAL bool isLocked() const {
        return locked;
    }

    //! the references recorded, which have to be bound before the store can be made
    QoreReferenceBindPairList pairs;

private:
    //! the retry of the store, which holds the references bound already, or nullptr
    const QoreBindRetry* retry;
    //! the references bound already by a store that binds them before it takes its lock, or nullptr
    const QoreReferenceBindPairList* bound = nullptr;
    //! the probe that this one replaced
    QoreReferenceBindProbe* prev;
    //! true while the probe is installed
    bool installed = true;
    //! true if a reference that needs a bind is refused instead of recorded
    bool verify = false;
    //! true if the thread holds an lvalue lock while the probe is installed
    bool locked = true;
};

//! the probe installed for the current thread, or nullptr
DLLLOCAL extern thread_local QoreReferenceBindProbe* qore_reference_bind_probe;

//! true while Qore code runs that the check of a store holding an lvalue lock runs (see QoreReferenceBindCodeScope)
DLLLOCAL extern thread_local bool qore_reference_bind_refused;

//! Uninstalls the probe of the current thread for a check that binds inline, and installs it again (qore#7391)
/** For a check made with no lvalue lock held in code that runs while a probe is installed, such as the write-back of
    a bind
*/
class QoreReferenceBindProbeSuspend {
public:
    DLLLOCAL QoreReferenceBindProbeSuspend() : prev(qore_reference_bind_probe) {
        qore_reference_bind_probe = nullptr;
    }

    DLLLOCAL ~QoreReferenceBindProbeSuspend() {
        qore_reference_bind_probe = prev;
    }

    QoreReferenceBindProbeSuspend(const QoreReferenceBindProbeSuspend&) = delete;
    QoreReferenceBindProbeSuspend& operator=(const QoreReferenceBindProbeSuspend&) = delete;

private:
    QoreReferenceBindProbe* prev;
};

//! Runs Qore code from a type check that can be made with a probe installed (qore#7391)
/** The only Qore code that a type check runs is the member initializers of a hashdecl that a hash is converted to
    (typed_hash_decl_private): this scope is around their evaluation.  With no probe installed, it does nothing.  With a
    probe installed, the code runs with the probe suspended, so that its own checks record nothing in the probe of the
    outer store, and, if the outer store holds an lvalue lock, with binds refused: a reference that the code binds (an
    argument for a reference<T> parameter, a store to a local variable) would be bound under that lock, which can
    deadlock, so the bind raises REFERENCE-BIND-ERROR instead (qore_reference_bind_unlocked()).
*/
class QoreReferenceBindCodeScope {
public:
    DLLLOCAL QoreReferenceBindCodeScope() : prev(qore_reference_bind_probe) {
        if (prev) [[unlikely]] {
            enter();
        }
    }

    DLLLOCAL ~QoreReferenceBindCodeScope() {
        if (prev) [[unlikely]] {
            qore_reference_bind_probe = prev;
            qore_reference_bind_refused = prev_refused;
        }
    }

    QoreReferenceBindCodeScope(const QoreReferenceBindCodeScope&) = delete;
    QoreReferenceBindCodeScope& operator=(const QoreReferenceBindCodeScope&) = delete;

private:
    QoreReferenceBindProbe* prev;
    bool prev_refused = false;

    DLLLOCAL void enter() {
        prev_refused = qore_reference_bind_refused;
        qore_reference_bind_probe = nullptr;
        if (prev->isLocked()) {
            qore_reference_bind_refused = true;
        }
    }
};

//! A bind of a reference in progress in the current thread (qore#7391)
/** A reference that an outer bind of the same thread is binding to the same type is accepted without binding it again,
    so that a cycle of references (a variable that holds a reference to another one that holds a reference to the
    first one) ends.
*/
class QoreReferenceBindFrame {
public:
    DLLLOCAL QoreReferenceBindFrame(const ReferenceNode* r, const QoreTypeInfo* ti);

    DLLLOCAL ~QoreReferenceBindFrame();

    QoreReferenceBindFrame(const QoreReferenceBindFrame&) = delete;
    QoreReferenceBindFrame& operator=(const QoreReferenceBindFrame&) = delete;

    //! returns true if a bind of the current thread is binding \a r to \a ti
    DLLLOCAL static bool active(const ReferenceNode* r, const QoreTypeInfo* ti);

private:
    const ReferenceNode* r;
    const QoreTypeInfo* ti;
    QoreReferenceBindFrame* prev;
};

//! returns true if a value stored into the list or the hash \a container can bind a reference element (qore#7391)
/** For an operation that stores values into the container that its lvalue holds (push, unshift, splice, +=) with the
    lock of the lvalue held: its element type, its value type or the member types of its hashdecl can bind a reference
*/
DLLLOCAL bool qore_container_store_may_bind(const QoreValue& container);

//! returns true if push or unshift into the lvalue of \a lvh, which has no value, can bind a reference (qore#7391)
/** The operation assigns the lvalue a new list of the element type of its list type first: true if that type can
    bind a reference.  Out of line, for an lvalue with no value; the caller tests a list's own element type inline
    (qore_list_private::elementMayBindReference()).
*/
DLLLOCAL bool qore_list_push_nothing_may_bind(const LValueHelper& lvh);

//! Checks the values that an operation stores into the container that \a lvh holds before it changes it (qore#7391)
/** For a container for which qore_container_store_may_bind() returns true, or an lvalue with no value whose type can
    bind a reference, before the operation stores the container that it makes for it.  The values are checked as the
    operation checks them, on references of their own, with a probe installed; the members of an object operand of +=
    on a hash are copied under the object's read lock first.  If the probe records references, nothing is changed and
    the store is made again after they have been bound (LValueHelper::deferBinds()).  Otherwise a probe that verifies
    is left installed in \a probe for the operation itself: its check finds only references that need no bind or that
    were bound already, and a reference that needs a bind anyway (the values changed meanwhile) is refused by the
    check of the operation, as a value of the wrong type is, before the operation stores it.  An exception that the
    check raises here is dropped: the operation raises it again.

    @param lvh the helper of the operation, which holds the lvalue lock
    @param operand the value that the operation stores
    @param merge true if the operation stores the elements of a list operand (into a list) or the members of a hash
    or an object operand (into a hash), false if it stores \a operand itself
    @param policy the lexical semantic policy of the operation, if any
    @param probe the probe, installed here if the operation is checked with it; the caller resets it after the
    operation

    @return -1 if the operation must not be made: the store is made again; 0 if not
*/
DLLLOCAL int qore_container_store_probe(LValueHelper& lvh, const QoreValue& operand, bool merge,
        const LexicalSemanticPolicy* policy, std::optional<QoreReferenceBindProbe>& probe);

//! Binds a reference to the reference type of \a pair.type with no lvalue lock held (lib/QoreTypeInfo.cpp)
/** The bind of QTS_COMPLEXREF in QoreTypeSpec::acceptInputImpl(): the value of the referenced lvalue is checked or
    converted to the type and written back if it was converted (qore#7391)
*/
DLLLOCAL void qore_reference_bind_pair(const QoreReferenceBindPair& pair, ExceptionSink* xsink);

#ifdef DEBUG
//! called by the bind of a reference element before it resolves the referenced lvalue (qore#7391)
/** Called with the number of lvalue write locks that the thread holds (qore_lvalue_write_locks_held), which is 0 for
    a bind made outside the locks of the store; returns true if the bind must fail with DBG-REFERENCE-BIND-HELD.  Set
    by dbg_reference_bind_rendezvous() (lib/ql_debug.cpp).
*/
typedef bool (*qore_dbg_reference_bind_hook_t)(int held);
DLLLOCAL extern std::atomic<qore_dbg_reference_bind_hook_t> qore_dbg_reference_bind_hook;

//! true if a bind made with an lvalue write lock held raises DBG-REFERENCE-BIND-HELD instead of aborting (qore#7391)
/** Set by dbg_reference_bind_strict() (lib/ql_debug.cpp): a test arms it so that a store path that still binds under
    a lock fails the test with an exception; otherwise such a bind fails an assertion
*/
DLLLOCAL extern std::atomic<bool> qore_dbg_reference_bind_strict;
#endif

#endif
