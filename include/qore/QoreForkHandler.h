/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreForkHandler.h

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

#ifndef _QORE_QOREFORKHANDLER_H
#define _QORE_QOREFORKHANDLER_H

#include <qore/common.h>

/** @file QoreForkHandler.h
    Fork handlers for the process-wide locks of binary modules; see @ref qore_module_fork_handlers
*/

class QoreThreadLock;
class QoreRecursiveThreadLock;

//! the lock order ranks of the fork handlers of module locks
/** Before \c fork(), the library acquires its own process-wide locks and the locks of the registered module
    handlers in increasing rank order; a thread that holds a lock may only acquire locks of a higher rank.  A module
    lock therefore ranks below every registered lock that its holders acquire, and above every registered lock that
    is held when it is acquired.  See @ref qore_module_fork_handlers.

    @since %Qore 3.0
*/
enum QoreForkRank : int {
    //! a module lock that is held while user code runs or while a thread waits for input: it is not acquired before
    //! \c fork(), only initialized again in the child
    QFR_MODULE_RESET = 60,
    //! a module lock that is acquired while no other registered lock is held, and whose holders can check for
    //! cancellation (qore_check_cancel(), which takes the library's thread list lock) and call library functions that
    //! only take its leaf locks (memory, strings, containers, exceptions), but do not start or wait for threads
    QFR_MODULE_CANCELLABLE = 450,
    //! a module lock whose holders take no lock but the library's leaf locks (memory allocation)
    QFR_MODULE_LEAF = 1000,
};

//! a fork handler; called with the argument given to qore_register_fork_handler()
typedef void (*q_fork_handler_t)(void* arg);

//! a registration of fork handlers; see qore_register_fork_handler()
struct QoreForkHandlerRegistration;

//! registers the fork handlers of a process-wide lock of a module
/** A child process created with \c fork() runs only the thread that called \c fork(): a lock that another thread
    held at that moment stays held in the child forever.  The library runs registered fork handlers for every
    \c fork() in the process, whoever calls it:
    - @p prepare before \c fork(), in increasing rank order with the library's own handlers; normally it acquires the
      lock, so that no other thread holds it and the data it protects is consistent when the process is copied
    - @p parent after \c fork() in the parent, in decreasing rank order; normally it releases the lock
    - @p child after \c fork() in the child, in decreasing rank order; normally it initializes the lock (and any
      condition used with it) again, since the forking thread is the only thread left; a lock is initialized again
      rather than released, since a lock held by another thread of the parent cannot be released in the child

    The handlers run in the thread that calls \c fork(): they must not throw, register or remove fork handlers,
    acquire any lock but their own, or allocate memory in the child.

    Register the handlers in the module's initialization function and remove them in its delete function, not
    while static objects are constructed or destroyed: the dynamic loader holds its lock then, which a thread that
    the fork handlers wait for can need.

    @param rank the lock order rank, normally a @ref QoreForkRank value
    @param name the name of the lock, for debugging; it must stay valid until the registration is removed (a string
    literal)
    @param prepare the handler called before \c fork(); may be nullptr
    @param parent the handler called in the parent after \c fork(); may be nullptr
    @param child the handler called in the child after \c fork(); may be nullptr
    @param arg the argument passed to the handlers

    @return the registration, to be removed with qore_unregister_fork_handler(), or nullptr if there was no memory
    for it

    @par Example:
    @code{.cpp}
// a registry of the module, protected by a process-wide lock whose holders take no other lock
static QoreThreadLock registry_lock;
static QoreForkHandlerRegistration* registry_fork_reg = nullptr;

static void mymod_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    registry_fork_reg = qore_register_fork_handler(QFR_MODULE_LEAF, "mymod_registry", qore_fork_prepare_thread_lock,
        qore_fork_parent_thread_lock, qore_fork_child_thread_lock, &registry_lock);
    // ...
}

static void mymod_module_delete() {
    qore_unregister_fork_handler(registry_fork_reg);
    // ...
}
    @endcode

    @since %Qore 3.0
*/
DLLEXPORT QoreForkHandlerRegistration* qore_register_fork_handler(int rank, const char* name,
        q_fork_handler_t prepare, q_fork_handler_t parent, q_fork_handler_t child, void* arg);

//! removes a registration made with qore_register_fork_handler(); does nothing if @p reg is nullptr
/** @since %Qore 3.0
*/
DLLEXPORT void qore_unregister_fork_handler(QoreForkHandlerRegistration* reg);

//! a prepare handler for a QoreThreadLock (not a QoreRecursiveThreadLock): acquires the lock passed as @p lock
/** @since %Qore 3.0
*/
DLLEXPORT void qore_fork_prepare_thread_lock(void* lock);

//! a parent handler for a QoreThreadLock: releases the lock passed as @p lock
/** @since %Qore 3.0
*/
DLLEXPORT void qore_fork_parent_thread_lock(void* lock);

//! a child handler for a QoreThreadLock: initializes the lock passed as @p lock again, unlocked
/** @since %Qore 3.0
*/
DLLEXPORT void qore_fork_child_thread_lock(void* lock);

//! initializes a QoreRecursiveThreadLock again, unlocked, in a child process created with \c fork()
/** For the child handler of a recursive lock: after this, the handler takes the lock again as often as the forking
    thread held it, which the module counts itself (see @ref qore_module_fork_handlers)

    @since %Qore 3.0
*/
DLLEXPORT void qore_fork_reinit_recursive_thread_lock(QoreRecursiveThreadLock& lock);

#endif
