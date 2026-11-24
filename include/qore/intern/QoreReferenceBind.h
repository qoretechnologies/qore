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

#include <cstddef>
#include <optional>
#include <type_traits>
#include <vector>

/*  Binding reference elements outside the lvalue locks of a store (qore#7391)

    A value checked against a type that contains reference<T> binds each reference element whose variable has a wider
    declared type: the referenced lvalue is resolved and locked, its value is checked or converted to T and can be
    written back.  A store checks its value with the lock of its target held, so binding there could lock a second
    lvalue under the first one (two threads that do it in the opposite order deadlock) or the same one again.

    A store whose target's type can bind a reference (QoreTypeInfo::mayBindReference()) and whose helper holds an
    lvalue lock checks its value with a QoreReferenceBindProbe installed: the check records each reference that it
    would bind with the reference type instead of binding it.  If anything was recorded, nothing is stored: the
    helper rolls back the entries created for the store and hands the value and the references to the QoreBindRetry
    of the store, and returns -1 with no exception.  The store site then leaves its helper, which releases every lock,
    and QoreBindRetry::again() binds the references with no lock held and has the store made again.  The second check
    accepts the references bound already without binding them.

    A store site:
        QoreBindRetry retry(xsink);
        do {
            LValueHelper h(exp, xsink, true, retry);
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
    of an IR or JIT/AOT instruction, whose keys are evaluated already) uses qore_store_with_bind_retry() or
    qore_reference_store_with_bind_retry() (Variable.h), which make this loop.  A merge into an object binds the
    references of its values before it takes the object's lock (qore_object_private::mergeBindReferences()).

    A store made with a helper that has no QoreBindRetry and that records a reference fails closed: it raises
    REFERENCE-BIND-ERROR and leaves its target unchanged.
*/

class ReferenceNode;
class QoreTypeInfo;
class ExceptionSink;
class LValueHelper;
class QoreBindRetry;
struct QoreBindRetryState;

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

//! The probe of the type check of a store that holds an lvalue lock (qore#7391)
/** While installed, QTS_COMPLEXREF in QoreTypeSpec::acceptInputImpl() records the references that it would bind
    instead of binding them.  The probe is installed for the current thread and restores the probe that it replaced
    when it is destroyed.
*/
class QoreReferenceBindProbe {
public:
    //! installs the probe; \a retry gives the references that were bound already, if any
    DLLLOCAL explicit QoreReferenceBindProbe(const QoreBindRetry* retry);

    //! installs the probe; \a bound gives the references that were bound already, if any (qore#7391)
    /** For a store that binds the references of its values before it takes its lock, such as a merge into an object
    */
    DLLLOCAL QoreReferenceBindProbe(const QoreBindRetry* retry, const QoreReferenceBindPairList* bound);

    //! uninstalls the probe if it is still installed; the pairs left are released with no exception sink
    DLLLOCAL ~QoreReferenceBindProbe();

    QoreReferenceBindProbe(const QoreReferenceBindProbe&) = delete;
    QoreReferenceBindProbe& operator=(const QoreReferenceBindProbe&) = delete;

    //! uninstalls the probe; the pairs that it recorded are kept
    DLLLOCAL void uninstall();

    //! records that the check accepted \a r for the reference type of \a ti; accepted with no record if bound already
    DLLLOCAL void record(ReferenceNode* r, const QoreTypeInfo* ti, LexicalSemanticPolicy policy, bool lexical);

    //! returns true if a reference was recorded
    DLLLOCAL bool recorded() const {
        return !pairs.empty();
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
};

//! the probe installed for the current thread, or nullptr
DLLLOCAL extern thread_local QoreReferenceBindProbe* qore_reference_bind_probe;

//! Uninstalls the probe of the current thread for a check that binds inline, and installs it again (qore#7391)
/** For a check made with no lvalue lock held in code that runs while a probe is installed, such as a store to a local
    variable, or a bind itself
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

//! Checks the values that an operation stores into the container that \a lvh holds before it changes it (qore#7391)
/** For a container for which qore_container_store_may_bind() returns true.  The values are checked as the operation
    checks them, on references of their own, with a probe installed: if the probe records references, nothing is
    changed and the store is made again after they have been bound (LValueHelper::deferBinds()).  Otherwise the probe
    is left installed in \a probe for the operation itself, which then records nothing, as the references of its
    values need no bind; the caller calls qore_container_store_probe_end() after the operation.  An exception that
    the check raises here is dropped: the operation raises it again.

    @param lvh the helper of the operation, which holds the lvalue lock
    @param operand the value that the operation stores
    @param merge true if the operation stores the elements of a list operand (into a list) or the members of a hash
    operand (into a hash), false if it stores \a operand itself
    @param policy the lexical semantic policy of the operation, if any
    @param probe the probe, installed here if the operation is checked with it

    @return -1 if the operation must not be made: the store is made again, or an exception was raised; 0 if not
*/
DLLLOCAL int qore_container_store_probe(LValueHelper& lvh, const QoreValue& operand, bool merge,
        const LexicalSemanticPolicy* policy, std::optional<QoreReferenceBindProbe>& probe);

//! Uninstalls the probe that qore_container_store_probe() left for an operation (qore#7391)
/** A reference recorded by the operation itself, which its check found to need no bind, raises REFERENCE-BIND-ERROR

    @return -1 if an exception was raised, 0 if not
*/
DLLLOCAL int qore_container_store_probe_end(LValueHelper& lvh, std::optional<QoreReferenceBindProbe>& probe);

//! Binds a reference to the reference type of \a pair.type with no lvalue lock held (lib/QoreTypeInfo.cpp)
/** The bind of QTS_COMPLEXREF in QoreTypeSpec::acceptInputImpl(): the value of the referenced lvalue is checked or
    converted to the type and written back if it was converted (qore#7391)
*/
DLLLOCAL void qore_reference_bind_pair(const QoreReferenceBindPair& pair, ExceptionSink* xsink);

//! The retry of a store whose check recorded references to bind (qore#7391)
/** Created by the store site before the LValueHelper of the store, which it is given to; again() is called after the
    helper has been destroyed, with no lvalue lock held.

    The keys and indices of the path of the target are evaluated once: the helper of the first round records them in
    the order that its steps take them, and the helper of a retry resolves the path with them instead of evaluating
    the key expressions again.
*/
class QoreBindRetry {
public:
    //! the number of rounds after which a store whose check keeps recording references fails
    static constexpr unsigned MaxRounds = 16;

    DLLLOCAL explicit QoreBindRetry(ExceptionSink* xsink) : xsink(xsink) {
    }

    DLLLOCAL ~QoreBindRetry() {
        if (state || owned_keys) [[unlikely]] {
            release();
        }
    }

    QoreBindRetry(const QoreBindRetry&) = delete;
    QoreBindRetry& operator=(const QoreBindRetry&) = delete;

    //! returns true if the store has to be made again; binds the references recorded with no lvalue lock held
    /** Called after the helper of the store has been destroyed.  Returns false if the store did not record a
        reference (it succeeded or failed, and the caller reports that), or if a bind raised an exception, or if the
        check raised an exception after it recorded a reference: the exception is in the exception sink then.
    */
    DLLLOCAL bool again() {
        return state ? againIntern() : false;
    }

    //! returns true if the last round recorded references and has to be made again (see again())
    /** For a store site whose operation can fail with no exception for this reason only, such as += through a
        default value
    */
    DLLLOCAL bool isPending() const {
        return pending;
    }

    //! returns true if the value of the last round is kept for the store (see takeValue())
    DLLLOCAL bool hasValue() const {
        return value_kept;
    }

    //! returns the value of the last round, which the caller owns
    DLLLOCAL QoreValue takeValue();

    //! returns true if a reference recorded by an earlier round of this store was bound to \a ti
    DLLLOCAL bool isBound(const ReferenceNode* r, const QoreTypeInfo* ti) const;

    //! records that the store has to be made again (LValueHelper)
    /** @param value the value of the store, which the retry takes; a value of an earlier round that the store site did
        not take is handed to \a lvh, which releases it after its locks
        @param pairs the references to bind, which the retry takes
        @param error the exception that the check raised after it recorded the references, which the retry takes
        @param lvh the helper of the store, which holds an lvalue lock
    */
    DLLLOCAL void setPending(QoreValue& value, QoreReferenceBindPairList& pairs, ExceptionSink& error,
            LValueHelper& lvh);

    //! returns true if the keys of the path of the target have been recorded
    DLLLOCAL bool keysJournaled() const {
        return keys_journaled;
    }

    //! records that the helper of the first round has resolved the path, whose keys are recorded then
    DLLLOCAL void setKeysJournaled() {
        keys_journaled = true;
    }

    //! records a key or an index of the path of the target
    /** @param key the key
        @param evaluated true if the key was evaluated, false if it is the constant of the expression, which lives as
        long as the expression and is recorded without a reference
    */
    DLLLOCAL void recordKey(const QoreValue& key, bool evaluated);

    //! returns the number of keys recorded
    DLLLOCAL size_t keyCount() const {
        return key_count;
    }

    //! returns the key recorded at position \a i (0 = the key that the first step took)
    DLLLOCAL QoreValue getKey(size_t i) const;

private:
    //! the number of keys held without an allocation
    static constexpr size_t KeyBufSize = 8;

    //! the exception sink of the store
    ExceptionSink* xsink;
    //! the state of a store that recorded references, allocated when needed
    QoreBindRetryState* state = nullptr;
    //! the first KeyBufSize keys recorded (the first key_count are valid); the others are in the state
    alignas(QoreValue) unsigned char key_buf[KeyBufSize * sizeof(QoreValue)];
    //! the number of keys recorded
    size_t key_count = 0;
    //! the number of keys recorded with a reference of the retry
    size_t owned_keys = 0;
    //! the keys of key_buf recorded with a reference of the retry, one bit each
    unsigned owned_key_mask = 0;
    //! true once the keys of the path have been recorded
    bool keys_journaled = false;
    //! true if the state holds the value of the last round (see hasValue())
    bool value_kept = false;
    //! true if the last round recorded references (see isPending())
    bool pending = false;

    //! binds the references recorded and returns true if the store has to be made again
    DLLLOCAL bool againIntern();

    //! releases the value, the keys and the references held
    DLLLOCAL void release();

    //! returns the state, allocating it if needed
    DLLLOCAL QoreBindRetryState& getState();
};

#endif
