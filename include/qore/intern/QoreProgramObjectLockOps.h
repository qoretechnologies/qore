/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreProgramObjectLockOps.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef _QORE_INTERN_QOREPROGRAMOBJECTLOCKOPS_H
#define _QORE_INTERN_QOREPROGRAMOBJECTLOCKOPS_H

#include <qore/QoreObject.h>
#include <qore/QoreProgram.h>
#include <qore/QoreCounter.h>
#include <qore/QoreCondition.h>
#include <qore/QoreRWLock.h>
#include <qore/RWLock.h>

#include <utility>

/** The non-virtual DLLLOCAL member functions of QoreObject, QoreProgram, QoreBreakpoint, QoreCounter, QoreCondition,
    QoreRWLock, QoreVarRWLock and RWLock were declared in the public headers although no module can call them (they are
    not exported); they are members of the classes below, in the implementation of the library.  Each class is a friend
    of the class it serves; call an operation through a temporary object:

    @code
    qore_object_ops(obj).getStatus();
    @endcode

    A function that stays out of line is a static fooImpl(Cls* b, ...) with an inline wrapper, so that the temporary is
    optimized away and the call costs what the member function cost (a temporary whose address a call takes makes GCC
    add a stack-protector canary to the caller).
*/

//! The library-internal operations of QoreObject (qore#4380)
class qore_object_ops {
public:
    //! the object
    QoreObject* b;

    DLLLOCAL explicit qore_object_ops(QoreObject* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_object_ops(H& holder) : qore_object_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_object_ops(const QoreObject* n_b) : b(const_cast<QoreObject*>(n_b)) {
    }

    DLLLOCAL explicit qore_object_ops(QoreObject& r) : b(&r) {
    }

    DLLLOCAL explicit qore_object_ops(const QoreObject& r) : b(const_cast<QoreObject*>(&r)) {
    }

    // returns a new hash consisting of just the members of value_list
    DLLLOCAL static QoreHashNode* getSliceImpl(QoreObject* b, const QoreListNode* value_list, ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* getSlice(const QoreListNode* value_list, ExceptionSink* xsink) {
        return getSliceImpl(b, value_list, xsink);
    }

    //! called to evaluate a builtin method when private data is available
    /**
        @param method a constant reference to the QoreMethod object
        @param meth the name of the method to evalute
        @param args the arguments for the method
        @param rc runtime configuration for the call
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLLOCAL static QoreValue evalBuiltinMethodWithPrivateDataImpl(QoreObject* b, const QoreMethod& method, const BuiltinNormalMethodVariantBase* meth, const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink);
    DLLLOCAL QoreValue evalBuiltinMethodWithPrivateData(const QoreMethod& method, const BuiltinNormalMethodVariantBase* meth, const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
        return evalBuiltinMethodWithPrivateDataImpl(b, method, meth, args, rc, xsink);
    }

    //! Internal canonical boolean conversion, with the same lifetime lock as the public value API.
    DLLLOCAL static bool getAsBoolDefaultImpl(QoreObject* b);
    DLLLOCAL bool getAsBoolDefault() {
        return getAsBoolDefaultImpl(b);
    }

    //! returns true if the class has a memberNotification method
    DLLLOCAL static bool hasMemberNotificationImpl(QoreObject* b);
    DLLLOCAL bool hasMemberNotification() {
        return hasMemberNotificationImpl(b);
    }

    DLLLOCAL static int getStatusImpl(QoreObject* b);
    DLLLOCAL int getStatus() {
        return getStatusImpl(b);
    }

    //! called at parse time
    DLLLOCAL static int parseInitImpl(QoreObject* b, QoreValue& val, QoreParseContext& parse_context);
    DLLLOCAL int parseInit(QoreValue& val, QoreParseContext& parse_context) {
        return parseInitImpl(b, val, parse_context);
    }

    //! concatenates info about private data to a string
    /**
        @param str the string to concatenate to
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLLOCAL static void addPrivateDataToStringImpl(QoreObject* b, QoreString* str, ExceptionSink* xsink);
    DLLLOCAL void addPrivateDataToString(QoreString* str, ExceptionSink* xsink) {
        addPrivateDataToStringImpl(b, str, xsink);
    }

    //! runs the destructor for system objects
    /**
        @param classID the ID of the class
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLLOCAL static void defaultSystemDestructorImpl(QoreObject* b, qore_classid_t classID, ExceptionSink* xsink);
    DLLLOCAL void defaultSystemDestructor(qore_classid_t classID, ExceptionSink* xsink) {
        defaultSystemDestructorImpl(b, classID, xsink);
    }

    //! called on the old object (this) to acquire private data, copy method called with pointer to "self" (new copy)
    DLLLOCAL static void evalCopyMethodWithPrivateDataImpl(QoreObject* b, const QoreClass &thisclass, const BuiltinCopyVariantBase* meth, QoreObject* self, RuntimeConfig& rc, ExceptionSink* xsink);
    DLLLOCAL void evalCopyMethodWithPrivateData(const QoreClass &thisclass, const BuiltinCopyVariantBase* meth, QoreObject* self, RuntimeConfig& rc, ExceptionSink* xsink) {
        evalCopyMethodWithPrivateDataImpl(b, thisclass, meth, self, rc, xsink);
    }

    //! executes the member notification on the object the given member
    DLLLOCAL static void execMemberNotificationImpl(QoreObject* b, const char* member, ExceptionSink* xsink);
    DLLLOCAL void execMemberNotification(const char* member, ExceptionSink* xsink) {
        execMemberNotificationImpl(b, member, xsink);
    }

    //! Destroys all members and dereferences all private data structures
    /**
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLLOCAL static void obliterateImpl(QoreObject* b, ExceptionSink* xsink);
    DLLLOCAL void obliterate(ExceptionSink* xsink) {
        obliterateImpl(b, xsink);
    }

};

//! The library-internal operations of QoreProgram (qore#4380)
class qore_program_ops {
public:
    //! the object
    QoreProgram* b;

    DLLLOCAL explicit qore_program_ops(QoreProgram* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_program_ops(H& holder) : qore_program_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_program_ops(const QoreProgram* n_b) : b(const_cast<QoreProgram*>(n_b)) {
    }

    DLLLOCAL explicit qore_program_ops(QoreProgram& r) : b(&r) {
    }

    DLLLOCAL explicit qore_program_ops(const QoreProgram& r) : b(const_cast<QoreProgram*>(&r)) {
    }

    // returns 0 if a "requires" exception has already occurred
    DLLLOCAL static ExceptionSink* getParseExceptionSinkImpl(QoreProgram* b);
    DLLLOCAL ExceptionSink* getParseExceptionSink() {
        return getParseExceptionSinkImpl(b);
    }

    DLLLOCAL static QoreListNode* getVarListImpl(QoreProgram* b);
    DLLLOCAL QoreListNode* getVarList() {
        return getVarListImpl(b);
    }

    /* for run-time module loading; the parse lock must be grabbed
        before loading new modules - note this should only be assigned
        to an AutoLock or SafeLocker object!
    */
    DLLLOCAL static QoreThreadLock * getParseLockImpl(QoreProgram* b);
    DLLLOCAL QoreThreadLock * getParseLock() {
        return getParseLockImpl(b);
    }

    // can only be called while parsing from the same thread doing the parsing
    DLLLOCAL static bool parseExceptionRaisedImpl(QoreProgram* b);
    DLLLOCAL bool parseExceptionRaised() {
        return parseExceptionRaisedImpl(b);
    }

    DLLLOCAL static const LVList* getTopLevelLVListImpl(QoreProgram* b);
    DLLLOCAL const LVList* getTopLevelLVList() {
        return getTopLevelLVListImpl(b);
    }

    DLLLOCAL static const char* parseGetIncludePathImpl(QoreProgram* b);
    DLLLOCAL const char* parseGetIncludePath() {
        return parseGetIncludePathImpl(b);
    }

    //! returns the script directory, if known (0 if not), does not grab the parse lock, only to be called while parsing
    /** @return the script directory, if known (0 if not)
    */
    DLLLOCAL static const char* parseGetScriptDirImpl(QoreProgram* b);
    DLLLOCAL const char* parseGetScriptDir() {
        return parseGetScriptDirImpl(b);
    }

    DLLLOCAL static void parseSetIncludePathImpl(QoreProgram* b, const char* path);
    DLLLOCAL void parseSetIncludePath(const char* path) {
        parseSetIncludePathImpl(b, path);
    }

};

//! The library-internal operations of QoreBreakpoint (qore#4380)
class qore_breakpoint_ops {
public:
    //! the object
    QoreBreakpoint* b;

    DLLLOCAL explicit qore_breakpoint_ops(QoreBreakpoint* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_breakpoint_ops(H& holder) : qore_breakpoint_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_breakpoint_ops(const QoreBreakpoint* n_b) : b(const_cast<QoreBreakpoint*>(n_b)) {
    }

    DLLLOCAL explicit qore_breakpoint_ops(QoreBreakpoint& r) : b(&r) {
    }

    DLLLOCAL explicit qore_breakpoint_ops(const QoreBreakpoint& r) : b(const_cast<QoreBreakpoint*>(&r)) {
    }

    //! returns the Program of the breakpoint with a reference for the caller, or nullptr
    /** nullptr is returned if the breakpoint is not assigned to a Program or if the last reference of its Program has
        been released
    */
    DLLLOCAL static QoreProgram* getReferencedProgramImpl(QoreBreakpoint* b);
    DLLLOCAL QoreProgram* getReferencedProgram() {
        return getReferencedProgramImpl(b);
    }

    DLLLOCAL static bool checkPgmImpl(QoreBreakpoint* b, ExceptionSink* xsink);
    DLLLOCAL bool checkPgm(ExceptionSink* xsink) {
        return checkPgmImpl(b, xsink);
    }

    DLLLOCAL static bool isStatementAssignedImpl(QoreBreakpoint* b, const AbstractStatement *statement);
    DLLLOCAL bool isStatementAssigned(const AbstractStatement *statement) {
        return isStatementAssignedImpl(b, statement);
    }

    //! returns the breakpoint with the given ID with a reference for the caller, or nullptr
    DLLLOCAL static QoreBreakpoint* getReferencedBreakpoint(unsigned breakpointId);

    DLLLOCAL static void unassignAllStatementsImpl(QoreBreakpoint* b);
    DLLLOCAL void unassignAllStatements() {
        unassignAllStatementsImpl(b);
    }

};

//! The library-internal operations of QoreCounter (qore#4380)
class qore_counter_ops {
public:
    //! the object
    QoreCounter* b;

    DLLLOCAL explicit qore_counter_ops(QoreCounter* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_counter_ops(H& holder) : qore_counter_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_counter_ops(const QoreCounter* n_b) : b(const_cast<QoreCounter*>(n_b)) {
    }

    DLLLOCAL explicit qore_counter_ops(QoreCounter& r) : b(&r) {
    }

    DLLLOCAL explicit qore_counter_ops(const QoreCounter& r) : b(const_cast<QoreCounter*>(&r)) {
    }

    //! waits until the count is at most \a n; returns 0, or ETIMEDOUT if it is still higher after \a timeout_ms
    DLLLOCAL static int waitForCountAtMostImpl(QoreCounter* b, int n, int timeout_ms);
    DLLLOCAL int waitForCountAtMost(int n, int timeout_ms) {
        return waitForCountAtMostImpl(b, n, timeout_ms);
    }

    DLLLOCAL static void decImpl(QoreCounter* b);
    DLLLOCAL void dec() {
        decImpl(b);
    }

    // internal use only - the fork handlers of process-wide counters (see QoreForkCounter)
    //! acquires the counter's lock before fork()
    DLLLOCAL static void lockForForkImpl(QoreCounter* b);
    DLLLOCAL void lockForFork() {
        lockForForkImpl(b);
    }

    //! initializes the counter's lock and condition again in the child after fork()
    /** @param count the count in the child, or -1 to keep the count
    */
    DLLLOCAL static void reinitAfterForkImpl(QoreCounter* b, int count);
    DLLLOCAL void reinitAfterFork(int count = -1) {
        reinitAfterForkImpl(b, count);
    }

    //! releases the counter's lock in the parent after fork()
    DLLLOCAL static void unlockForForkImpl(QoreCounter* b);
    DLLLOCAL void unlockForFork() {
        unlockForForkImpl(b);
    }

    // internal use only - for internal counters
    DLLLOCAL static void waitForZeroImpl(QoreCounter* b);
    DLLLOCAL void waitForZero() {
        waitForZeroImpl(b);
    }

};

//! The library-internal operations of QoreCondition (qore#4380)
class qore_condition_ops {
public:
    //! the object
    QoreCondition* b;

    DLLLOCAL explicit qore_condition_ops(QoreCondition* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_condition_ops(H& holder) : qore_condition_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_condition_ops(const QoreCondition* n_b) : b(const_cast<QoreCondition*>(n_b)) {
    }

    DLLLOCAL explicit qore_condition_ops(QoreCondition& r) : b(&r) {
    }

    DLLLOCAL explicit qore_condition_ops(const QoreCondition& r) : b(const_cast<QoreCondition*>(&r)) {
    }

    //! blocks a thread on a lock until the condition is signaled, with interrupt support and a check before it blocks
    /** As waitWithInterrupt(QoreThreadLock&, ExceptionSink*), but \a before_block is called with the lock held after
        the wait is registered for a cancellation wakeup and before the thread blocks; if it returns non-zero, the
        thread does not block, and QORE_COND_RESULT_SUCCESS is returned.  A thread that requests the end of the wait
        by setting what \a before_block reads and then waking the waiter (QoreThreadList::wakeCondWaiter()) is
        therefore never lost; see qore_cond_wait_cancellable()

        @param l the QoreThreadLock to wait on
        @param xsink exception sink for interrupt errors
        @param operation the operation named in the cancellation exception
        @param before_block the check; called with \a arg
        @param arg the argument of \a before_block

        @note internal: for the wait for a Program's parse lock, whose mutex is a QoreThreadLock (see
        qore_program_private::waitForParseLock()); not available to modules
    */
    DLLLOCAL static int waitWithInterruptImpl(QoreCondition* b, QoreThreadLock& l, ExceptionSink* xsink, const char* operation, int (*before_block)(void*), void* arg);
    DLLLOCAL int waitWithInterrupt(QoreThreadLock& l, ExceptionSink* xsink, const char* operation, int (*before_block)(void*), void* arg) {
        return waitWithInterruptImpl(b, l, xsink, operation, before_block, arg);
    }

    //! initializes the condition again in a child process created with fork(), where its waiters do not exist
    /** internal
    */
    DLLLOCAL static void reinitAfterForkImpl(QoreCondition* b);
    DLLLOCAL void reinitAfterFork() {
        reinitAfterForkImpl(b);
    }

};

//! The library-internal operations of QoreRWLock (qore#4380)
class qore_rwlock_ops {
public:
    //! the object
    QoreRWLock* b;

    DLLLOCAL explicit qore_rwlock_ops(QoreRWLock* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_rwlock_ops(H& holder) : qore_rwlock_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_rwlock_ops(const QoreRWLock* n_b) : b(const_cast<QoreRWLock*>(n_b)) {
    }

    DLLLOCAL explicit qore_rwlock_ops(QoreRWLock& r) : b(&r) {
    }

    DLLLOCAL explicit qore_rwlock_ops(const QoreRWLock& r) : b(const_cast<QoreRWLock*>(&r)) {
    }

    //! initializes the lock again, unlocked and without waiters, in a child process created with fork() (internal)
    /** see QoreForkRWLock
    */
    DLLLOCAL static void reinitAfterForkImpl(QoreRWLock* b);
    DLLLOCAL void reinitAfterFork() {
        reinitAfterForkImpl(b);
    }

};

//! The library-internal operations of QoreVarRWLock (qore#4380)
class qore_var_rwlock_ops {
public:
    //! the object
    QoreVarRWLock* b;

    DLLLOCAL explicit qore_var_rwlock_ops(QoreVarRWLock* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_var_rwlock_ops(H& holder) : qore_var_rwlock_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_var_rwlock_ops(const QoreVarRWLock* n_b) : b(const_cast<QoreVarRWLock*>(n_b)) {
    }

    DLLLOCAL explicit qore_var_rwlock_ops(QoreVarRWLock& r) : b(&r) {
    }

    DLLLOCAL explicit qore_var_rwlock_ops(const QoreVarRWLock& r) : b(const_cast<QoreVarRWLock*>(&r)) {
    }

    //! tries to grab the read lock; does not block if unsuccessful; returns 0 if successful
    DLLLOCAL static int tryrdlockImpl(QoreVarRWLock* b);
    DLLLOCAL int tryrdlock() {
        return tryrdlockImpl(b);
    }

    //! tries to grab the write lock; does not block if unsuccessful; returns 0 if successful
    DLLLOCAL static int trywrlockImpl(QoreVarRWLock* b);
    DLLLOCAL int trywrlock() {
        return trywrlockImpl(b);
    }

    //! grabs the read lock
    DLLLOCAL static void rdlockImpl(QoreVarRWLock* b);
    DLLLOCAL void rdlock() {
        rdlockImpl(b);
    }

    //! grabs the write lock
    DLLLOCAL static void wrlockImpl(QoreVarRWLock* b);
    DLLLOCAL void wrlock() {
        wrlockImpl(b);
    }

};

//! The library-internal operations of RWLock (qore#4380)
class qore_rwlock_intern_ops {
public:
    //! the object
    RWLock* b;

    DLLLOCAL explicit qore_rwlock_intern_ops(RWLock* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_rwlock_intern_ops(H& holder) : qore_rwlock_intern_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_rwlock_intern_ops(const RWLock* n_b) : b(const_cast<RWLock*>(n_b)) {
    }

    DLLLOCAL explicit qore_rwlock_intern_ops(RWLock& r) : b(&r) {
    }

    DLLLOCAL explicit qore_rwlock_intern_ops(const RWLock& r) : b(const_cast<RWLock*>(&r)) {
    }

    // 0 = last read lock in this thread released
    DLLLOCAL static int cleanup_read_lock_internImpl(RWLock* b, tid_map_t::iterator i);
    DLLLOCAL int cleanup_read_lock_intern(tid_map_t::iterator i) {
        return cleanup_read_lock_internImpl(b, i);
    }

    DLLLOCAL static int grab_read_lock_internImpl(RWLock* b, int mtid, VLock *nvl, int64 timeout_ms, ExceptionSink *xsink);
    DLLLOCAL int grab_read_lock_intern(int mtid, VLock *nvl, int64 timeout_ms, ExceptionSink *xsink) {
        return grab_read_lock_internImpl(b, mtid, nvl, timeout_ms, xsink);
    }

    DLLLOCAL static void mark_read_lock_internImpl(RWLock* b, int mtid, VLock *nvl);
    DLLLOCAL void mark_read_lock_intern(int mtid, VLock *nvl) {
        mark_read_lock_internImpl(b, mtid, nvl);
    }

    DLLLOCAL static void release_read_lock_internImpl(RWLock* b, tid_map_t::iterator i);
    DLLLOCAL void release_read_lock_intern(tid_map_t::iterator i) {
        release_read_lock_internImpl(b, i);
    }

    DLLLOCAL static void set_initial_read_lock_internImpl(RWLock* b, int mtid, VLock *nvl);
    DLLLOCAL void set_initial_read_lock_intern(int mtid, VLock *nvl) {
        set_initial_read_lock_internImpl(b, mtid, nvl);
    }

};

#endif // _QORE_INTERN_QOREPROGRAMOBJECTLOCKOPS_H
