/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    DbiForkRegistry.h

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

#ifndef _QORE_INTERN_DBIFORKREGISTRY_H
#define _QORE_INTERN_DBIFORKREGISTRY_H

/** @file DbiForkRegistry.h
    The DBI objects whose locks are initialized again in a child process created with fork(), and the connections
    that such a child gives up; see design/dbi-fork.md.
*/

class ExceptionSink;
class Datasource;

//! an object of the DBI layer whose locks are initialized again in a child process created with fork()
/** A thread of the parent can hold the lock of a datasource, of a pool, of a connection's statement set or of a
    statement while the process is copied: the lock would stay held in the child forever.  These locks are held across
    driver calls (opening and closing connections, closing statements), so they are not acquired before fork(); the
    child handler of the registry initializes them again (as for a QFLM_RESET lock), and the child gives up the
    parent's connections without using what the locks protect (see qore_ds_private::abandon()).

    An object registers itself with forkRegister() once it is completely constructed and removes itself with
    forkUnregister() before it is destroyed.  forkChild() runs in the child, where only the forking thread exists:
    it must take no lock and allocate nothing.
*/
class QoreDbiForkObject {
public:
    DLLLOCAL QoreDbiForkObject() = default;

    DLLLOCAL virtual ~QoreDbiForkObject();

    QoreDbiForkObject(const QoreDbiForkObject&) = delete;
    QoreDbiForkObject& operator=(const QoreDbiForkObject&) = delete;

    //! initializes the object's locks and conditions again in a child process; takes no lock and allocates nothing
    DLLLOCAL virtual void forkChild() noexcept = 0;

protected:
    //! registers the object; called when it is completely constructed
    DLLLOCAL void forkRegister();

    //! removes the registration; called first by the destructor of the class that registered the object
    DLLLOCAL void forkUnregister();

private:
    friend class QoreDbiForkRegistry;

    //! the neighbors in the registry's list, protected by the registry's lock
    QoreDbiForkObject* fork_prev = nullptr;
    QoreDbiForkObject* fork_next = nullptr;
    bool fork_registered = false;
};

//! how a check for the connections of the parent of a child process created with fork() treats a transaction
/** A child gives up every connection that it inherits (see qore_ds_private::abandon()); a transaction in progress on
    one of them belongs to the parent, and the action of the child that would continue it raises
    \c DATASOURCE-FORK-ERROR instead (see qore_dbi_raise_fork_error()).
*/
enum class QoreDbiForkCheck : unsigned char {
    //! the action uses the connection, so it would continue the transaction: the error is raised
    Raise,
    //! the action does not use the connection (options, configuration, closing a statement): the error is raised by
    //! the next action that does
    Defer,
    //! the connection is closed or destroyed: the transaction is given up without an error
    Discard,
};

//! the reason for \c DATASOURCE-FORK-ERROR for a connection that was in a transaction
constexpr const char* QoreDbiForkReasonTransaction = "was in a transaction, which belongs to the parent";
//! the reason for \c DATASOURCE-FORK-ERROR for a transaction lock that another thread of the parent held
constexpr const char* QoreDbiForkReasonLock = "another thread of the parent held its transaction lock";
//! the reason for \c DATASOURCE-FORK-ERROR for a connection of a DatasourcePool that the thread held in a transaction
constexpr const char* QoreDbiForkReasonPoolTransaction = "the connection that this thread held was in a transaction, "
    "which belongs to the parent";

//! keeps memory given up by a child process created with fork() reachable until the process ends
/** The driver data of an inherited connection and of its statements, and the containers that a thread of the parent
    can have been changing when the process was copied, are never freed or traversed in the child; keeping a pointer
    to them makes them reachable memory rather than leaks for memory checkers.  Thread-safe.
*/
DLLLOCAL void qore_dbi_keep_abandoned(void* p);

//! raises DATASOURCE-FORK-ERROR for an action on a connection that a child process created with fork() gave up
/** @param ds the datasource (for the connection description)
    @param reason why the action cannot use a new connection
*/
DLLLOCAL void qore_dbi_raise_fork_error(ExceptionSink* xsink, const Datasource& ds, const char* reason);

#endif
