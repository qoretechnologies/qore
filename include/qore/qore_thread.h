/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_thread.h

    POSIX thread library for Qore

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

#ifndef _QORE_QORE_THREAD_H
#define _QORE_QORE_THREAD_H

/** @file qore_thread.h
    Provides definitions relative to threading in Qore.
 */

#include <cstdio>
#include <pthread.h>

#include <cstdint>
#include <functional>

class QoreProgram;
class AbstractQoreZoneInfo;
class ThreadCleanupNode;
class AbstractThreadResource;
struct QoreProgramLocation;
class AbstractStatement;
class QoreStackLocation;

//! pointer to a qore thread destructor function
typedef void (*qtdest_t)(void *);

//! pointer to a qore thread resource destructor function
typedef void (*qtrdest_t)(void *, ExceptionSink *);

//! pointer to a function that can be started with q_start_thread()
typedef void (*q_thread_t)(ExceptionSink* xsink, void* arg);

//! snapshot of the current runtime context (TLS-backed)
/** This structure provides a stable, read-only view of runtime context for
    binary modules. The values are populated from thread-local storage.

    @note Fields may be nullptr during initialization or parsing.
 */
struct QoreRuntimeContext {
    QoreProgram* pgm;
    const QoreProgramLocation* loc;
    const AbstractStatement* stmt;
    int64_t po;
    const QoreStackLocation* stack_loc;
    int element;
};

//! fills a QoreRuntimeContext snapshot from thread-local storage
/** @param rc output pointer for the runtime context snapshot
 */
DLLEXPORT void qore_get_runtime_context(QoreRuntimeContext* rc);

//! returns true if the current thread is a valid qore thread; it is not safe to call most Qore functions unless the thread is registered with Qore
/**
    @see
    - QoreForeignThreadHelper
    - q_register_foreign_thread()
    - q_deregister_foreign_thread()
 */
DLLEXPORT bool is_valid_qore_thread();

//! returns the current TID number
/** @since %Qore 0.9.5.1
*/
DLLEXPORT int q_gettid() noexcept;

//! returns the current QoreProgram
DLLEXPORT QoreProgram* getProgram();

//! returns the current local time zone, note that if 0 = UTC
DLLEXPORT const AbstractQoreZoneInfo* currentTZ();

//! save a resource against a thread for thread resource handling
/** @param atr a pointer to the thread resource to save
 */
DLLEXPORT void set_thread_resource(AbstractThreadResource* atr);

//! remove the resource from the thread resource list for the current thread
/** @param atr a pointer to the thread resource to remove
    @return 0 if successful (resource was found and removed), -1 if the resource was not found
 */
DLLEXPORT int remove_thread_resource(AbstractThreadResource* atr);

//! check if a thread resouce has been saved
/** @param atr thread resource id to check
    @return true if saved, false if not saved
*/
DLLEXPORT bool check_thread_resource(AbstractThreadResource* atr);

//! save a callable resource against a thread for thread resource handling
/** @param rcr a pointer to a callable node for thread resource handling
 *  @param arg an argument to use when calling the thread resource handler
 */
DLLEXPORT void set_thread_resource(const ResolvedCallReferenceNode* rcr, const QoreValue arg);

//! remove the callable resource from the thread resource list for the current thread
/** @param rcr a pointer to the thread resource to remove
    @param xsink any Qore-language exceptions raised when dereferencing the callable object will be saved here

    @return 0 if successful (resource was found and removed), -1 if the resource was not found
 */
DLLEXPORT int remove_thread_resource(const ResolvedCallReferenceNode* rcr, ExceptionSink* xsink);

//! Returns a CallStackInfo hash for the caller's location, if available, otherwise returns nullptr
/** @param offset the call stack entry offset
*/
DLLEXPORT QoreHashNode* qore_get_parent_caller_location(size_t offset = 1);

#if 0
//! save a resource against a thread for thread resource handling using the thread resource id
/** by using the thread resource id, you can quickly check if the resource has already been saved for the thread
    @param trid thread resource id
    @param atr a pointer to the thread resource to save
    @see qore_get_trid()
    @see remove_thread_resource_id()
 */
DLLEXPORT void set_thread_resource_id(q_trid_t trid, AbstractThreadResource *atr);

//! remove the resource from the thread resource list for the current thread using the thread resource ID
/** @param trid thread resource id of the resource to remove, must have been set with set_thread_resource_id()
    @return 0 if successful (resource was found and removed), -1 if the resource was not found
    @see qore_get_trid()
    @see set_thread_resource_id()
 */
DLLEXPORT int remove_thread_resource_id(q_trid_t trid);

//! check if a thread resouce has been saved with the given resource id
/** @param trid thread resource id to check
    @return true if saved, false if not saved
*/
DLLEXPORT bool check_thread_resource_id(q_trid_t trid);
#endif

//! get a thread resource ID
DLLEXPORT q_trid_t qore_get_trid();

//! list of functions to be run when a thread ends; required for some external libraries that require explicit cleanup when a thread terminates
/** this list is not locked and therefore the ThreadCleanupList::push() and
    ThreadCleanupList::pop() functions must only be called in module initialization
    and module deletion.  However this list is implemented in such a way that thread
    cleanup list execution may be safely called interally while push() is being
    executed in a module initialization function, for example.
    @note this is a global object and not an attribute of a thread
 */
class ThreadCleanupList {
private:
   static ThreadCleanupNode *head;

public:
   DLLLOCAL ThreadCleanupList();
   DLLLOCAL ~ThreadCleanupList();
   DLLLOCAL void exec();

   //! must only be called in the module initialization function
   /** @param func the cleanup function to be run whenever a thread ends
       @param arg the argument to the function (can be 0)
   */
   DLLEXPORT void push(qtdest_t func, void *arg);

   //! must only be called in the module destructor/deletion function
   /** @param exec if true the cleanup function will be executed immediately, if false it will not
    */
   DLLEXPORT void pop(bool exec = true);
};

//! the interface to the thread cleanup list
DLLEXPORT extern ThreadCleanupList tclist;

/** @defgroup q_register_foreign_thread_rv Return Value Codes for q_register_foreign_thread()
 */
///@{
#define QFT_OK          0  //!< OK response when calling q_register_foreign_thread()
#define QFT_ERROR      -1  //!< error when calling q_register_foreign_thread()
#define QFT_REGISTERED -2  //!< thread already registered when calling q_register_foreign_thread()
///@}

//! registers the current thread as a Qore thread
/** call q_deregister_foreign_thread() when Qore functionality is no longer required; not calling
    q_deregister_foreign_thread() will cause a memory leak

    @return QFT_OK (0) if the thread was successfully registered, QFT_ERROR (-1) if an error occurred, or
    QFT_REGISTERED (-2) if it was already registered; see @ref q_register_foreign_thread_rv for more info

    @see
    - QoreForeignThreadHelper
    - q_deregister_foreign_thread()
    - is_valid_qore_thread();

    @since
    - %Qore 0.8.7: introduced
    - %QOre 1.0.10: TIDs are reassigned if possible to supoport %Qore thread affinity in foreign language modules
 */
DLLEXPORT int q_register_foreign_thread();

//! deregisters the current thread as a foreign thread
/** @return 0 if the foreign thread was deregistered, -1 if it was not registered or was not a foreign thread

    @see
    - QoreForeignThreadHelper
    - q_register_foreign_thread()
    - is_valid_qore_thread();

    @since %Qore 0.8.7
 */
DLLEXPORT int q_deregister_foreign_thread();

//! reserves a thread ID for later registration and returns the TID reserved; use q_release_reserved_foreign_thread_id() to release the reservation
/** @return the TID reserved or -1 if an error occurred (thread table full)

    @since %Qore 0.8.7
 */
DLLEXPORT int q_reserve_foreign_thread_id();

//! releases a TID reserved with q_reserve_foreign_thread_id()
/** @param tid the TID reserved with q_reserve_foreign_thread_id()

    @return 0 for OK, -1 for error (TID invalid or not reserved)

    @since %Qore 0.8.7
 */
DLLEXPORT int q_release_reserved_foreign_thread_id(int tid);

//! registers a foreign qore thread as a Qore thread with a reserved TID
/** @param tid the TID reserved with q_reserve_foreign_thread_id()

    @return 0 for OK, -1 for error (TID invalid or not reserved)

    @since %Qore 0.8.7
 */
DLLEXPORT int q_register_reserved_foreign_thread(int tid);

//! deregisters a foreign qore thread but retains the TID as reserved
/** @returns 0 for OK, -1 for error (TID invalid or not foreign)

    @since %Qore 0.8.7
 */
DLLEXPORT int q_deregister_reserved_foreign_thread();

//! starts a new thread with the given arguments, when the thread terminates, it deregisters itself
/** @param arg the argument to the function
    @param f the function to start in the new thread
    @param xsink any errors starting the new thread will be raised here and cause -1 to be returned

    @return the new TID or -1 if an error occurred

    @since %Qore 0.8.8
 */
int q_start_thread(ExceptionSink* xsink, q_thread_t f, void* arg = nullptr);

//! starts a new thread with a custom stack size and no Qore stack guard, when the thread terminates, it deregisters itself
/** The Qore stack guard is disabled for threads created with this overload, allowing the full
    stack size to be used. This is intended for lightweight C++ I/O threads that do not execute
    Qore-language code directly.

    @param xsink any errors starting the new thread will be raised here and cause -1 to be returned
    @param f the function to start in the new thread
    @param arg the argument to the function
    @param stack_size the stack size for the new thread in bytes

    @return the new TID or -1 if an error occurred

    @since %Qore 2.3
 */
int q_start_thread(ExceptionSink* xsink, q_thread_t f, void* arg, size_t stack_size);

//! starts a new thread with lifecycle flags
/** Threads started with QTF_EXTERNAL_LIFECYCLE are counted in tp_thread_counter instead of
    thread_counter, so they don't block QoreProgramHelper shutdown. This is used for ThreadPool
    threads whose lifecycle is managed explicitly.

    @param xsink any errors starting the new thread will be raised here and cause -1 to be returned
    @param f the function to start in the new thread
    @param arg the argument to the function
    @param flags thread flags (e.g. QTF_EXTERNAL_LIFECYCLE)

    @return the new TID or -1 if an error occurred

    @since %Qore 2.3
 */
DLLEXPORT int q_start_thread(ExceptionSink* xsink, q_thread_t f, void* arg, int flags);

//! use this class to temporarily register and deregister a foreign thread to allow Qore code to be executed and the Qore library to be used from threads not created by the Qore library
/** @since %Qore 0.8.7
 */
class QoreForeignThreadHelper {
public:
    //! registers the current thread as a foreign thread
    /** the thread will be deregistered with q_deregister_foreign_thread() in the destructor

        @since %Qore 1.0.10 TID are reassigned if possible in order to support %Qore thread affinity in language
        modules
    */
    DLLEXPORT QoreForeignThreadHelper();

    //! registers the current thread as a foreign thread
    /** The tid given must be already reserved with q_reserve_foreign_thread_id(); the thread will be deregistered
        with q_deregister_reserved_foreign_thread() in the destructor
    */
    DLLEXPORT explicit QoreForeignThreadHelper(int tid);

    //! deregisters the current thread
    /** deregistration is only performed if the original registration in the constructor was successful.
        In this case either q_deregister_foreign_thread() or q_reserve_foreign_thread_id() will be called, depending
        on the action taken in the constructor
    */
    DLLEXPORT ~QoreForeignThreadHelper();

    //! returns true if the object is valid, false if not
    DLLEXPORT operator bool() const;

protected:
    class qore_foreign_thread_priv* priv;

private:
    DLLLOCAL QoreForeignThreadHelper& operator=(const QoreForeignThreadHelper&) = delete;
    DLLLOCAL QoreForeignThreadHelper(const QoreForeignThreadHelper&) = delete;
    DLLLOCAL void* operator new(size_t) = delete;
};

typedef std::function<void(void*)> q_thread_local_destructor;

//! data structure for user thread-local data
/** @since %Qore 0.9.5
*/
struct q_user_tld {
    void* data;
    q_thread_local_destructor destructor;

    DLLLOCAL q_user_tld(void* data, q_thread_local_destructor destructor);
    DLLLOCAL q_user_tld(q_user_tld&& old) = default;
    DLLLOCAL q_user_tld(const q_user_tld& old) = default;
    DLLLOCAL q_user_tld& operator=(const q_user_tld& other) = default;
};

//! returns a unique ID to use with thread_local_data APIs
/** @since %Qore 0.9.5
*/
DLLEXPORT int q_get_unique_thread_local_data_key();

//! saves thread-local data
/** can only be called in a registered %Qore thread

    @since %Qore 0.9.5
*/
DLLEXPORT void q_save_thread_local_data(int key, void* data, q_thread_local_destructor destructor = nullptr);

//! swaps thread-local data
/** can only be called in a registered %Qore thread

    @since %Qore 0.9.5
*/
DLLEXPORT void* q_swap_thread_local_data(int key, void* new_data, q_thread_local_destructor destructor = nullptr, bool run_destructor = true);

//! returns thread-local data
/** @note can only be called in a registered %Qore thread

    @since %Qore 0.9.5
*/
DLLEXPORT void* q_get_thread_local_data(int key);

//! returns thread-local data
/** @param key the key used to save the data

    @return nullptr if the data was not found, otherwise a pointer to the data structure holding the data

    @note can only be called in a registered %Qore thread

    @since %Qore 0.9.5
*/
DLLEXPORT q_user_tld* q_get_thread_local_data_all(int key);

//! returns and removes thread-local data
/** @param key the key used to save the data
    @param data the data structure, if found (output variable)
    @param run_destructor if true then the destructor is run when the data is removed; if false, it is not

    @return -1 if the data was not found, 0 if found and returned

    @note can only be called in a registered %Qore thread

    @since %Qore 0.9.5
*/
DLLEXPORT int q_remove_thread_local_data(int key, q_user_tld& data, bool run_destructor = true);

/** @defgroup thread_cancel_api Thread Cancellation API
    Functions for cooperative per-thread cancellation.

    Thread cancellation allows one thread to request that another thread stop execution.
    The target thread receives a \c THREAD-CANCELLED exception at the next cancellation point
    (loop iteration, blocking wait, sleep, I/O operation).

    @since %Qore 2.2
*/
///@{

//! Checks for both thread cancellation and program interrupt
/** This is the primary cancellation check function. Call it at cancellation points in
    binary modules. It checks the per-thread cancellation flag first (cheap atomic load),
    then the program-level interrupt via the sandbox manager.

    @param xsink exception sink — receives \c THREAD-CANCELLED or \c PROGRAM-INTERRUPTED
    @param operation description of the operation being checked (for exception messages)

    @return true if cancelled/interrupted (exception raised), false otherwise

    @since %Qore 2.2
*/
DLLEXPORT bool qore_check_cancel(ExceptionSink* xsink, const char* operation = "operation");

//! Requests cooperative cancellation of a specific thread
/** The target thread will receive a \c THREAD-CANCELLED exception at the next cancellation
    point. Only threads in the same program context can be cancelled.

    @param tid the Qore thread ID to cancel
    @param reason optional reason string (will be included in the exception message)

    @return 0 on success, -1 if the thread was not found, not active, or in a different program

    @since %Qore 2.2
*/
DLLEXPORT int qore_cancel_thread(int tid, const char* reason = nullptr);

//! Clears the cancellation flag for the current thread
/** Call this after catching a \c THREAD-CANCELLED exception if the thread should continue
    running (e.g., to complete cleanup operations without re-triggering at the next
    cancellation point).

    @since %Qore 2.2
*/
DLLEXPORT void qore_clear_thread_cancel();

//! Returns true if cancellation has been requested for the current thread
/** Non-throwing check — allows code to detect cancellation without triggering an exception.

    @return true if cancellation has been requested

    @since %Qore 2.2
*/
DLLEXPORT bool qore_is_thread_cancel_requested();

///@}

#endif  // ifndef _QORE_THREAD_H
