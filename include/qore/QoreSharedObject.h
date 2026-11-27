/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreSharedObject.h

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

#ifndef _QORE_QORE_SHARED_OBJECT_H
#define _QORE_QORE_SHARED_OBJECT_H

/** @file QoreSharedObject.h
    The C++ API of the objects shared by every Program of the process (qore#7669)

    The C++ counterpart of SandboxManager::createsSharedObjects(), SandboxManager::canShareObjects(),
    SandboxManager::shareObject(), SandboxManager::isSharedObject(), SandboxManager::checkSharedObjectChange(),
    SandboxManager::checkSharedObjectCallback() and SandboxManager::updateSharedObject(), with the same semantics,
    for binary modules that create objects that every Program can reach (a registry, a cache, a client shared between
    Programs).  Included by qore/Qore.h; available if \c _QORE_HAS_SHARED_OBJECT_API is defined.

    Restricted code is code that a restricting sandbox governs (one that does not trust its code: it refuses
    uncontrolled APIs, see QoreSandboxManager::trustsUncontrolledApis()), the code of a Program sandboxed without a
    manager, and code that acts for such code; code that a trusted (governed) sandbox governs is sandboxed but not
    restricted, and creates and changes shared objects as trusted code does (qore#7737).

    No C++ exception leaves these functions: a failure is raised in the ExceptionSink given, and the predicates fail
    closed.

    @since %Qore 3.0
*/

#include <qore/common.h>

class QoreObject;
class ResolvedCallReferenceNode;
class ExceptionSink;
class QoreValue;

//! Returns true if what the calling code creates may be shared by every Program of the process
/** As SandboxManager::createsSharedObjects(): true if the calling code is not restricted; fails closed in the other
    direction than the sandbox checks: true if the sandbox that governs the code cannot be resolved, so that the object
    is treated as shared and restricted code cannot change it.  It reveals only whether the calling code is restricted.

    A module calls it where it creates an object for its caller (for example in the constructor of a builtin class),
    and marks the object with qore_share_object() if it returns true.

    @since %Qore 3.0
*/
DLLEXPORT bool qore_caller_creates_shared_objects();

//! Returns true if the calling code may mark objects as shared with qore_share_object()
/** As SandboxManager::canShareObjects(): code that no restricting sandbox governs may, and so may code in an update
    scope of a shared object (see qore_update_shared_object() and SandboxManager::updateSharedObject()), as long as it
    does not run for restricted code that the scope's code called.  Fails closed: false if it cannot be decided

    @since %Qore 3.0
*/
DLLEXPORT bool qore_caller_can_share_objects();

//! Marks an object as shared by every Program of the process, so that restricted code cannot change or delete it
/** As SandboxManager::shareObject(): code that is restricted or acts for restricted code then cannot change the
    object's members or delete it (\c SHARED-OBJECT-ERROR); the C++ methods of its class that change its private
    data check the caller with qore_check_shared_object_change() first.  The mark is permanent; only code that may
    share objects (qore_caller_can_share_objects()) can set it.

    @param obj the object to mark
    @param xsink raises \c SHARED-OBJECT-ERROR if \a obj is null or the calling code cannot share objects

    @return 0 if the object is marked, -1 if an exception was raised

    @since %Qore 3.0
*/
DLLEXPORT int qore_share_object(QoreObject* obj, ExceptionSink* xsink);

//! Returns true if the object is marked as shared by every Program of the process
/** As SandboxManager::isSharedObject()

    @param obj the object to check; null gives false

    @since %Qore 3.0
*/
DLLEXPORT bool qore_object_is_shared(const QoreObject* obj);

//! Raises \c SHARED-OBJECT-ERROR if the calling code is restricted and the object is shared by every Program
/** As SandboxManager::checkSharedObjectChange(): a C++ method of a builtin class that changes the private data of a
    shared object on behalf of its caller calls it before it changes anything.  It decides by the calling code only,
    also in an update scope (see qore_update_shared_object()), and fails closed: the change is refused if the sandbox
    that governs the code cannot be resolved.

    @param obj the object that the caller changes; nothing is checked for null
    @param what what is changed, for the exception (ex: \c "set the base path")
    @param xsink for the exception

    @return 0 if the change is allowed, -1 if an exception was raised

    @since %Qore 3.0
*/
DLLEXPORT int qore_check_shared_object_change(const QoreObject* obj, const char* what, ExceptionSink* xsink);

//! Raises \c SHARED-OBJECT-ERROR if restricted code gives a shared object code to run while the object is changed
/** As SandboxManager::checkSharedObjectCallback(): for a C++ method that runs code that its caller gives while it
    holds the object's state (such as a lock), code that is restricted or acts for restricted code can only give a
    closure that a method of a class that trusted code declared created.  Nothing is checked for an object that is not
    shared or a caller that no restricting sandbox governs; fails closed.

    @param obj the object that the caller changes; nothing is checked for null
    @param callback the code that the caller gives; where the callback is checked (a shared object and a restricted
    caller), null is refused as code that is not a closure of a trusted class; otherwise 0 is returned for null too,
    so the caller still handles a null callback itself
    @param what what is done, for the exception (ex: \c "run code locked")
    @param xsink for the exception

    @return 0 if the callback may run, -1 if an exception was raised

    @since %Qore 3.0
*/
DLLEXPORT int qore_check_shared_object_callback(const QoreObject* obj, const ResolvedCallReferenceNode* callback,
        const char* what, ExceptionSink* xsink);

//! The C++ update of a shared object that qore_update_shared_object() runs in an update scope
/** @param obj the object given to qore_update_shared_object()
    @param arg the argument given to qore_update_shared_object()
    @param xsink for exceptions

    @return the value that qore_update_shared_object() returns; it is returned as it is, also with an exception
*/
typedef QoreValue (*q_shared_object_update_t)(QoreObject* obj, void* arg, ExceptionSink* xsink);

//! Runs a C++ update of a shared object in an update scope for it, such as a cache that the object builds on first use
/** The C++ counterpart of SandboxManager::updateSharedObject(): \a update runs in an update scope for the object,
    as the code of the scope's Program (the thread enters that Program for the update, as a closure of a class of the
    object runs in its own Program), so that:
    - the update may share objects (qore_caller_can_share_objects()): an object that it creates and caches in the
      shared object can be marked with qore_share_object()
    - code of the scope's Program that the update calls, such as a closure of the object's class, can change the
      object's members

    The scope is allowed for a restricted caller with no open scope, so that a builtin method of a shared object can
    build its state on first use in a call for restricted code, as a closure of a trusted class can.  The update must
    compute the new state from the object itself, never from data that its caller gives.

    The checks of SandboxManager::updateSharedObject() that do not concern a closure apply:
    - \a obj must be shared
    - the scope's Program is the object's Program (see QoreObject::getProgram()), which must not be restricted; for an
      object that has no Program (an object of a builtin class that C++ code created), the Program of the calling
      code, if the calling code may share objects (qore_caller_can_share_objects()) and that Program is not
      restricted; else the scope is refused (fails closed)
    - code that restricted code called in an open update scope cannot open a scope

    The update changes the object's private data directly; qore_check_shared_object_change() decides by the calling
    code only and refuses a restricted caller also in the scope.  Code that the update calls is subject to the normal
    rules, as code that a closure calls in an update scope: code that runs in a restricted Program (a callback of a
    restricted caller, a method of a class of a restricted Program), and any code that it calls, acts for restricted
    code, so it cannot change the object, open another update scope or share objects.  Code that runs in a Program
    that no restricting sandbox governs is the scope's code, whoever supplied it: a closure or method reference of a
    trusted class, or a reference to a builtin function or method, that the caller gives runs with the rights of the
    scope, and a name that the caller gives resolves in the scope's Program.  The update therefore never calls code,
    or resolves names, that its caller supplied; the restrictions of SandboxManager::updateSharedObject() on its
    closure (created by a class of the object in the calling code, run once) have no counterpart here, as the update
    is the module's own C++ code.  Use it for the objects of the module's own classes.  The scope ends on every path
    out of this function; a thread that the update starts does not inherit it.

    @param obj the shared object to update
    @param update the update to run
    @param arg the argument given to \a update
    @param xsink raises \c SHARED-OBJECT-ERROR if the scope cannot be opened (and \a update is not run); a C++
    exception that \a update throws is raised here as an exception (\c OUT-OF-MEMORY for \c std::bad_alloc, else
    \c SHARED-OBJECT-ERROR)

    @return the value that \a update returns, or no value if the scope could not be opened

    @par Example:
    @code{.cpp}
static QoreValue build_cache(QoreObject* obj, void* arg, ExceptionSink* xsink) {
    static_cast<MyRegistry*>(arg)->buildCache();
    return QoreValue();
}

// a builtin method that builds the cache on first use, also in a call for restricted code
if (!priv->hasCache()) {
    if (qore_object_is_shared(self)) {
        qore_update_shared_object(self, build_cache, priv, xsink);
    } else {
        priv->buildCache();
    }
}
    @endcode

    @since %Qore 3.0
*/
DLLEXPORT QoreValue qore_update_shared_object(QoreObject* obj, q_shared_object_update_t update, void* arg,
        ExceptionSink* xsink);

#endif
