/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreTypeOwner.h

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

#ifndef _QORE_INTERN_QORETYPEOWNER_H
#define _QORE_INTERN_QORETYPEOWNER_H

#include <qore/common.h>

#include <cstddef>
#include <vector>

class QoreTypeInfo;

class qore_ns_private;

//! A class, hashdecl or enum declaration, whose types can be named after it is destroyed (issue #5790)
/** A value or an object refers to the declaration of its own type with a reference, but its types are also named
    with plain pointers: by a derived type built on them (\c list<*Foo>, \c Box<*Foo>, \c hash<string, Foo>,
    \c hash<HD<*Foo>>, ...), which a value can have as its type, and by the member, static variable and signature
    types of another declaration (a hashdecl with a member of type \c *Foo), which can outlive the Program of both;
    every type check against such a type reads the declaration.

    - A declaration that survives the deletion of its Program (QoreTypeOwnerBatch), or that is retired and keeps its
      members, holds a reference to each declaration that it names directly, released when it is destroyed (or
      retired, for a class): a declaration named so is never released before what names it.  The declarations of a
      Program destroyed together do not keep each other.
    - When the last reference to a declaration is released (qore_type_owner_released()) and a derived type or a
      generic cache entry names its types, it is retired and kept until library cleanup (derived types are never
      freed before that); otherwise it is freed.

    A retired declaration releases what the type checks do not read (a class its methods, members and static
    variables; a hashdecl and an enum keep their members, as the checks compare them), and its types match no value
    that is not of an equal declaration, as no value of the declaration exists (unless the reflection API creates
    one), and NOTHING where the type accepts it.  As every pointer stays valid and nothing that the type checks read is
    changed, a type check in another thread is not affected.  It holds a reference to itself, so the references that
    other code takes to it and releases never bring its count to zero again.

    The decisions are made under the type lock, which guards the state of the class; a declaration that is referenced
    when it would be decided is decided when that reference is released.  So a reference taken to a declaration whose
    count is zero (by the reflection API, through a type that names it) is safe: only a declaration that nothing can
    reach is freed.
*/
class QoreTypeOwner {
public:
    DLLLOCAL virtual ~QoreTypeOwner() = default;

    //! The key of the declaration in the parameterized-class caches: its class or hashdecl object; nullptr for an enum
    DLLLOCAL virtual const void* getTypeOwnerKey() const = 0;

    //! The type of the declaration
    DLLLOCAL virtual const QoreTypeInfo* getTypeOwnerType() const = 0;

    //! The or-nothing type of the declaration
    DLLLOCAL virtual const QoreTypeInfo* getTypeOwnerOrNothingType() const = 0;

    //! Returns true if something holds a reference to the declaration
    DLLLOCAL bool hasTypeOwnerReferences() const {
        return getTypeOwnerReferenceCount() > 0;
    }

    //! Returns the number of references to the declaration
    DLLLOCAL virtual int getTypeOwnerReferenceCount() const = 0;

    //! Returns true for an original builtin (system or module) declaration, which is never retired
    /** Builtin declarations are freed with their namespace at library cleanup (or when their module is unloaded), in
        an order of their own, and name each other directly; the copies of builtin hashdecls and enums that a Program's
        namespace holds are freed with the Program, as user declarations are
    */
    DLLLOCAL virtual bool isTypeOwnerSystem() const = 0;

    //! Returns true if the declaration keeps its members when it is retired, and so what their types name
    DLLLOCAL virtual bool keepsTypeOwnerMembers() const = 0;

    //! Releases what the type checks of the declaration's types do not read; called once, when it is retired
    DLLLOCAL virtual void retireTypeOwner() = 0;

    //! Frees the declaration, which has no reference left
    DLLLOCAL virtual void freeTypeOwner() = 0;

    //! Adds the declarations that the declaration's member, static variable and signature types name directly
    DLLLOCAL virtual void getTypeOwnerNames(std::vector<QoreTypeOwner*>& names) const = 0;

    //! Takes a reference to the declaration (QoreTypeOwnerBatch)
    DLLLOCAL virtual void refTypeOwner() = 0;

    //! Releases a reference taken with refTypeOwner()
    DLLLOCAL virtual void derefTypeOwner() = 0;

    //! true once the declaration has been retired
    bool type_owner_retired = false;
    //! true while the thread retiring the declaration works without the lock: no other thread frees it then
    bool type_owner_retiring = false;
    //! true while the declaration holds a reference to itself as a retired declaration
    /** So that the references that other code takes and releases (ex: the reflection API) never bring its count to
        zero; released at library cleanup
    */
    bool type_owner_retirement_ref = false;
    //! true if type_owner_names holds the declarations that this one names (QoreTypeOwnerBatch)
    /** The names are taken when the Program's namespace is deleted, before any of its declarations is freed: the
        types that a declaration names can be freed afterwards, when nothing references them
    */
    bool type_owner_names_known = false;
    //! true if the declaration holds a reference to each declaration in type_owner_names
    bool type_owner_names_counted = false;
    //! the declarations that this one names directly
    std::vector<QoreTypeOwner*> type_owner_names;
};

//! The QoreTypeOwner of a class, hashdecl or enum declaration's private data \a T
/** A member of the declaration (its last one), so that the layout of the declaration's other members, which the
    code of the type checks reads, is not changed; each call is the method of the same name of the declaration
*/
template <class T>
class QoreTypeOwnerOf : public QoreTypeOwner {
public:
    DLLLOCAL QoreTypeOwnerOf(T* owner) : owner(owner) {
    }

    DLLLOCAL virtual const void* getTypeOwnerKey() const {
        return owner->getTypeOwnerKey();
    }

    DLLLOCAL virtual const QoreTypeInfo* getTypeOwnerType() const {
        return owner->getTypeOwnerType();
    }

    DLLLOCAL virtual const QoreTypeInfo* getTypeOwnerOrNothingType() const {
        return owner->getTypeOwnerOrNothingType();
    }

    DLLLOCAL virtual int getTypeOwnerReferenceCount() const {
        return owner->getTypeOwnerReferenceCount();
    }

    DLLLOCAL virtual bool isTypeOwnerSystem() const {
        return owner->isTypeOwnerSystem();
    }

    DLLLOCAL virtual bool keepsTypeOwnerMembers() const {
        return owner->keepsTypeOwnerMembers();
    }

    DLLLOCAL virtual void retireTypeOwner() {
        owner->retireTypeOwner();
    }

    DLLLOCAL virtual void freeTypeOwner() {
        owner->freeTypeOwner();
    }

    DLLLOCAL virtual void getTypeOwnerNames(std::vector<QoreTypeOwner*>& names) const {
        owner->getTypeOwnerNames(names);
    }

    DLLLOCAL virtual void refTypeOwner() {
        owner->refTypeOwner();
    }

    DLLLOCAL virtual void derefTypeOwner() {
        owner->derefTypeOwner();
    }

private:
    T* owner;
};

//! Called by a declaration whose last reference was released; see QoreTypeOwner
/** @return true if the declaration was retired (or is kept as already retired): the caller must not free it; false if
    the caller frees it
*/
DLLLOCAL bool qore_type_owner_released(QoreTypeOwner* owner);

//! Called by the destructor of a declaration: releases the names that it counted, which can free retired ones
DLLLOCAL void qore_type_owner_destroyed(QoreTypeOwner* owner);

//! Adds the declaration of a type to \a names if the type is the type of a user class, hashdecl or enum other than
//! \a self, or its or-nothing type
DLLLOCAL void qore_type_owner_add_name(std::vector<QoreTypeOwner*>& names, const QoreTypeInfo* ti,
        const QoreTypeOwner* self);

//! Decides what happens to the declarations of a Program whose namespace is deleted (see QoreTypeOwner)
/** Holds a reference to each declaration of the namespace while the namespace is deleted; afterwards, the declarations
    that something else still references survive, and count the declarations that they name; the others are retired
    if derived types or surviving or retired declarations name them, and freed otherwise
*/
class QoreTypeOwnerBatch {
public:
    //! Takes a reference to each user declaration of the namespace and of its subnamespaces
    DLLLOCAL QoreTypeOwnerBatch(qore_ns_private& ns);

    //! Decides the fate of the declarations and releases the references
    DLLLOCAL ~QoreTypeOwnerBatch();

private:
    std::vector<QoreTypeOwner*> owners;
};

//! Returns the number of declarations retired and not freed yet (see QoreTypeOwner)
DLLLOCAL size_t qore_retired_type_owner_count();

#endif
