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

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

class QoreTypeInfo;
class QoreClass;
class TypedHashDecl;
class QoreEnumDecl;

class qore_ns_private;
struct QoreTypeOwnerGroup;

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
    - The declarations of a Program that survive its deletion together and name each other form a group
      (QoreTypeOwnerGroup), in which they name each other without references, so that they do not keep each other
      alive: a member whose last reference is released holds a reference to itself for the group while a live (not
      yet released) or referenced member names it, directly or through other members, and the members that no such
      member reaches anymore are released; when the last live member is released, the group is collected, unless a
      member is referenced again (ex: by the reflection API): it is then live again.
    - When the last reference to a declaration is released (qore_type_owner_released()) and a derived type or a
      generic cache entry names its types, it is retired and kept until library cleanup (derived types are never
      freed before that), and so is a declaration that a retired one names (a retired hashdecl keeps its members);
      otherwise it is freed.

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
#ifdef DEBUG
    //! registers the declaration as live, so that the bookkeeping of the declarations that name each other can
    //! assert that it never reads one that was freed (qore_type_owner_is_live())
    DLLLOCAL QoreTypeOwner();
    DLLLOCAL QoreTypeOwner(const QoreTypeOwner&) = delete;
    DLLLOCAL QoreTypeOwner& operator=(const QoreTypeOwner&) = delete;
    DLLLOCAL virtual ~QoreTypeOwner();
#else
    DLLLOCAL virtual ~QoreTypeOwner() = default;
#endif

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

    //! Returns true for an original builtin declaration of a binary module (isTypeOwnerSystem() and not marked as
    //! belonging to no binary module: not_module_decl, set on the declarations of the static system namespace, the
    //! pseudo-classes and the tombstones, qore#7533)
    /** A module's namespace can be freed before a declaration of a Program that names its declarations (at the end of
        the process, the modules are deleted before a Program that holds itself is cleared), so a hashdecl copy, which
        shares the types of its source's members, references the module declarations that they name from when it is
        made, an enum copy references its original if it is a module's, and every other declaration of a Program
        references the module declarations that it names from its first QoreTypeOwnerBatch (type_owner_module_refs),
        until it is destroyed.  The declarations of the system namespace (Qore::) are freed after every Program and
        need no reference (qore#7533)
    */
    DLLLOCAL virtual bool isTypeOwnerModule() const = 0;

    //! Returns true if the declaration keeps its members when it is retired, and so what their types name
    DLLLOCAL virtual bool keepsTypeOwnerMembers() const = 0;

    //! Releases what the type checks of the declaration's types do not read; called once, when it is retired
    DLLLOCAL virtual void retireTypeOwner() = 0;

    //! Frees the declaration, which has no reference left
    DLLLOCAL virtual void freeTypeOwner() = 0;

    //! Adds the declarations that the declaration's member, static variable and signature types name directly, and
    //! the binary module declarations that they name directly or through compound types (qore_type_owner_add_name())
    DLLLOCAL virtual void getTypeOwnerNames(std::vector<QoreTypeOwner*>& names) const = 0;

    //! Adds the declarations that this one holds a reference to beyond the names (type_owner_refs): the base classes
    //! of a class, the source of an imported class, the parent and the source of a hashdecl
    /** Every reference is added (once per reference held), so that the references that only declarations of a cycle
        hold can be told from the others at library cleanup (delete_qore_types()); a reference that is not listed
        counts as held from outside the cycle, which keeps the cycle
    */
    DLLLOCAL virtual void getTypeOwnerHeldRefs(std::vector<QoreTypeOwner*>& refs) const = 0;

    //! Adds the binary module declarations that a binary module's class names in its own types and in those of its
    //! module base classes (qore#7533)
    /** Called for a binary module declaration (isTypeOwnerModule()) that a declaration of a Program or a hashdecl copy
        names: a class's type is shared with its copies, so a Program's code calls the module's original, whose
        member, static variable and signature types are read then (by the type checks of its calls, for example),
        and the module declarations that they name are kept with it (see
        qore_type_owner_add_module_class_names()).  Only the declarations that its types name are added, not what
        they name in turn.  Nothing is added for a hashdecl or an enum: a Program names a copy of a module's hashdecl,
        which keeps what its members name itself, and an enum names no declaration
    */
    DLLLOCAL virtual void getTypeOwnerModuleSignatureNames(std::vector<QoreTypeOwner*>& names) const = 0;

    //! Adds the declarations that the declaration's caches hold (the concrete declarations of a generic hashdecl),
    //! referenced; the caller releases the references (issue #6419)
    DLLLOCAL virtual void getTypeOwnerCachedDecls(std::vector<QoreTypeOwner*>& owners) const {
    }

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
    //! true if the names of the declaration are counted: it references the ones in type_owner_refs
    bool type_owner_names_counted = false;
    //! the declarations that this one names directly
    std::vector<QoreTypeOwner*> type_owner_names;
    //! the declarations that this one holds a reference to for its names
    std::vector<QoreTypeOwner*> type_owner_refs;
    //! true once type_owner_module_refs holds the module declarations that this one names (QoreTypeOwnerBatch)
    bool type_owner_module_refs_taken = false;
    //! the binary module declarations that this one names, directly or through compound types, referenced until it
    //! is destroyed (qore#7533)
    /** Builtin declarations are not among its names (type_owner_names), as they are not decided with the
        declarations of Programs; the module ones are referenced when the first Program that the declaration belongs to
        is deleted, while its types are valid, as the module that they belong to can be deleted before the declaration
        (a declaration of a user module's Program that a Program referenced at the end of the process keeps, whose
        types are read when that Program is cleared).  Empty for a hashdecl copy, which references them itself
        (typed_hash_decl_private::copy_module_names)
    */
    std::vector<QoreTypeOwner*> type_owner_module_refs;
    //! the group of survivors of a Program that the declaration is a member of, if any
    QoreTypeOwnerGroup* type_owner_group = nullptr;
    //! true while the declaration is a live member of its group: its last reference has not been released
    bool type_owner_group_live = false;
    //! true if the declaration holds a reference to itself for its group
    bool type_owner_group_hold = false;
    //! true if a retired hashdecl names it: it is retired, not freed, until no such hashdecl is left
    /** see type_owner_kept_by
    */
    bool type_owner_forever_named = false;
    //! the number of retired or concrete declarations that name it without a reference (issue #6419)
    /** A retired hashdecl, which keeps its members (type_owner_keeps), and a concrete declaration of a generic hashdecl
        whose type arguments name it (HD<Foo>); a retired declaration is freed only once this is zero
    */
    size_t type_owner_kept_by = 0;
    //! the declarations that this one keeps (their type_owner_kept_by counts it), released when it is freed
    std::vector<QoreTypeOwner*> type_owner_keeps;
    //! the epoch of the retirement once every derived type that named the declaration was orphaned (issue #6419)
    /** 0 while it is retired and its derived types are not orphaned yet
    */
    uint64_t type_owner_retire_epoch = 0;
    //! true if a derived type could not be orphaned from the declaration: it is kept until library cleanup
    bool type_owner_unfreeable = false;
    //! true once the declaration is being freed after its retirement (qore_type_owner_released() returns false)
    bool type_owner_freeing = false;
    //! the tombstone of the declaration once it has been made, guarded by the tombstone registry's lock
    /** Made while the declaration and what it names are alive (QoreTypeOwnerBatch), so that a later lookup does not
        read what can have been freed since
    */
    const void* type_owner_tombstone = nullptr;
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

    DLLLOCAL virtual bool isTypeOwnerModule() const {
        return owner->isTypeOwnerModule();
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

    DLLLOCAL virtual void getTypeOwnerModuleSignatureNames(std::vector<QoreTypeOwner*>& names) const {
        owner->getTypeOwnerModuleSignatureNames(names);
    }

    DLLLOCAL virtual void getTypeOwnerHeldRefs(std::vector<QoreTypeOwner*>& refs) const {
        owner->getTypeOwnerHeldRefs(refs);
    }

    DLLLOCAL virtual void getTypeOwnerCachedDecls(std::vector<QoreTypeOwner*>& owners) const {
        owner->getTypeOwnerCachedDecls(owners);
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

#ifdef DEBUG
//! Returns true if \a owner is a declaration that has not been freed (debug builds)
DLLLOCAL bool qore_type_owner_is_live(const QoreTypeOwner* owner);

//! Returns the number of declarations of Programs (not builtin ones) that have not been freed (debug builds)
/** Called at the end of qore_cleanup(): a declaration that is still alive then was leaked, unless its owner keeps it
    on purpose (a Program held at static scope)
*/
DLLLOCAL size_t qore_count_live_user_type_owners();

//! Returns the process-wide lock of the type retirement machinery with the given name, or nullptr (debug builds)
/** The names are those of the locks' fork handlers: \c "type_wait_entry", \c "type_epoch_holds",
    \c "type_tombstones", \c "generic_cache_registry", \c "type_owner_registry" and \c "type_info_registry"; for
    dbg_fork_while_type_lock_held() (qore#7549)
*/
DLLLOCAL std::mutex* qore_type_dbg_fork_lock(const char* name);
//! Returns the lock of qore_type_dbg_fork_lock() that thread.cpp owns, or nullptr (debug builds)
DLLLOCAL std::mutex* qore_thread_type_dbg_fork_lock(const char* name);
//! Returns the lock of the registry of tombstones (debug builds)
DLLLOCAL std::mutex* qore_type_tombstone_dbg_fork_lock();
//! Returns the number of armed epoch holds (QoreTypeEpochHold) of type retirement (debug builds, qore#7549)
DLLLOCAL size_t qore_type_epoch_hold_count();
#endif

//! Called by a declaration whose last reference was released; see QoreTypeOwner
/** @return true if the declaration was retired (or is kept as already retired): the caller must not free it; false if
    the caller frees it
*/
DLLLOCAL bool qore_type_owner_released(QoreTypeOwner* owner);

//! Called by the destructor of a declaration: releases the names that it counted, which can free retired ones
DLLLOCAL void qore_type_owner_destroyed(QoreTypeOwner* owner);

//! Adds the declaration of a type to \a names if the type is the type of a user class, hashdecl or enum other than
//! \a self, or its or-nothing type; otherwise, if \a module_names is true, the binary module declarations that the type
//! names, directly or through the types that it is made of (see qore_type_owner_add_module_names())
/** QoreTypeOwnerBatch separates the module declarations (QoreTypeOwner::type_owner_module_refs); \a module_names is
    false for a hashdecl copy, which references them itself (qore#7533)
*/
DLLLOCAL void qore_type_owner_add_name(std::vector<QoreTypeOwner*>& names, const QoreTypeInfo* ti,
        const QoreTypeOwner* self, bool module_names = true);

//! Adds the binary module declarations (QoreTypeOwner::isTypeOwnerModule()) that a type names, directly or through
//! the types that it is made of (element, member, argument, parameter and return types), once each
/** A hashdecl copy, which shares the types of its source's members, references them (qore#7533)
*/
DLLLOCAL void qore_type_owner_add_module_names(std::vector<QoreTypeOwner*>& names, const QoreTypeInfo* ti);

//! The binary module declarations that each binary module class names in its own types, computed once per caller
//! (QoreTypeOwner::getTypeOwnerModuleSignatureNames())
typedef std::map<const QoreTypeOwner*, std::vector<QoreTypeOwner*>> qore_type_owner_module_names_cache_t;

//! Adds the binary module declarations that the binary module classes in \a names name in their own types (qore#7533)
/** \a names lists binary module declarations (QoreTypeOwner::isTypeOwnerModule()); for each that is in it when called,
    the declarations of QoreTypeOwner::getTypeOwnerModuleSignatureNames() are added once each, but not walked in
    turn.  \a cache, if given, keeps what each declaration names for the next call (a QoreTypeOwnerBatch walks each
    module class once)
*/
DLLLOCAL void qore_type_owner_add_module_class_names(std::vector<QoreTypeOwner*>& names,
        qore_type_owner_module_names_cache_t* cache = nullptr);

//! Decides what happens to the declarations of a Program whose namespace is deleted (see QoreTypeOwner)
/** Holds a reference to each declaration of the namespace while the namespace is deleted; afterwards, the declarations
    that something else still references survive, and count the declarations that they name; the others are retired
    if derived types or surviving or retired declarations name them, and freed otherwise
*/
class QoreTypeOwnerBatch {
public:
    //! Takes a reference to each user declaration of the namespace and of its subnamespaces
    DLLLOCAL QoreTypeOwnerBatch(qore_ns_private& ns);

    //! Takes a reference to each of the given user declarations, which are destroyed together (the declarations that
    //! a rollback of a parse removes)
    DLLLOCAL QoreTypeOwnerBatch(const std::vector<QoreTypeOwner*>& all);

    //! Decides the fate of the declarations and releases the references
    DLLLOCAL ~QoreTypeOwnerBatch();

private:
    std::vector<QoreTypeOwner*> owners;

    DLLLOCAL void init(const std::vector<QoreTypeOwner*>& all);
};

//! Returns the number of declarations retired and not freed yet (see QoreTypeOwner)
DLLLOCAL size_t qore_retired_type_owner_count();

//! Counts the declarations and derived types kept until library cleanup, by kind, and the cache entries, for debug
//! checks (issue #6419)
DLLLOCAL void qore_type_retention_breakdown(std::map<std::string, int64_t>& out);

//! Returns the number of groups of declarations that survived their Program and are not collected yet
DLLLOCAL size_t qore_type_owner_group_count();

//! Returns the number of survivors of deleted Programs that only each other keep (see delete_qore_types()), which the
//! library cleanup frees; nothing is freed
DLLLOCAL size_t qore_cyclic_type_owner_count();

//! The global epoch of type retirement (issue #6419; see design/derived-type-retirement.md)
/** Incremented once for each retirement of declarations or derived types; each retired item records the epoch after
    the increment.  A thread publishes the epoch when it enters Qore code (its Program context is set from none) and
    clears it when it leaves (see qore_type_retirement_scan()): an item retired at epoch E can be freed once no thread
    runs Qore code since an epoch before E.  Nothing is freed before library cleanup yet; the epochs are bookkeeping,
    reported by qore_type_retirement_info().
*/
DLLLOCAL extern std::atomic<uint64_t> qore_type_retire_epoch;

//! The bookkeeping of the declarations and derived types retired and not freed yet (see qore_type_retire_epoch)
struct QoreTypeRetirementInfo {
    //! the current epoch
    uint64_t epoch = 0;
    //! the number of declarations retired and not freed
    size_t pending_declarations = 0;
    //! the number of derived types retired and not freed
    size_t pending_derived_types = 0;
    //! the number of those declarations whose grace period has passed: no thread runs Qore code since before them
    size_t freeable_declarations = 0;
    //! the number of those derived types whose grace period has passed
    size_t freeable_derived_types = 0;
    //! the epoch of the oldest item retired and not freed, or 0 if there is none
    uint64_t oldest_pending_epoch = 0;
    //! the epoch of the newest item retired and not freed, or 0 if there is none
    uint64_t newest_pending_epoch = 0;
    //! the lowest epoch of the threads that run Qore code, or 0 if no thread does
    uint64_t min_active_epoch = 0;
    //! the number of retired declarations freed before library cleanup
    int64_t freed_declarations = 0;
    //! the numbers of tombstones made (see qore_class_tombstone())
    size_t tombstone_classes = 0;
    size_t tombstone_hashdecls = 0;
    size_t tombstone_enums = 0;
};

//! Returns the bookkeeping of the retired declarations and derived types; the threads are scanned after the items are
//! counted, so an item counted as freeable is one that no thread can read
DLLLOCAL void qore_type_retirement_info(QoreTypeRetirementInfo& info);

//! Frees the retired declarations that no thread can read and nothing leads to anymore (issue #6419)
/** Every thread has passed a quiescent point since the declaration's retirement epoch, no derived type or declaration
    names it, and nothing but its retirement holds a reference to it.  Cheap when nothing is pending; never waits.
    Called after retirements, at the outermost exit of a thread from Qore code, at the end of a quiescent wait and by
    the debug hooks; a call made while one runs in the same thread does nothing.

    @return the number of declarations freed
*/
DLLLOCAL size_t qore_type_retirement_free();

//! Called when library cleanup starts (every thread has ended): from then on, the declarations retired are kept until
//! library cleanup frees them, with their derived types, without tombstones (see qore_type_retirement_free())
DLLLOCAL void qore_type_retirement_shutdown();

//! true while retired declarations wait to be freed (see qore_type_retirement_free())
DLLLOCAL extern std::atomic<bool> qore_type_retirement_pending;

//! Returns the tombstone of a class: an empty, immortal class equal to it (see lib/QoreTypeTombstone.cpp), the class
//! itself if it is builtin, or nullptr if it cannot be made; the caller holds no type lock
DLLLOCAL const QoreClass* qore_class_tombstone(const QoreClass* qc);
//! Returns the tombstone of a hashdecl; see qore_class_tombstone()
DLLLOCAL const TypedHashDecl* qore_hashdecl_tombstone(const TypedHashDecl* hd);
//! Returns the tombstone of an enum; see qore_class_tombstone()
DLLLOCAL const QoreEnumDecl* qore_enum_tombstone(const QoreEnumDecl* ed);
//! Returns the type built on the tombstones of the declarations that a type names (the type itself if it names none);
//! \a ok is false if one cannot be made
DLLLOCAL const QoreTypeInfo* qore_type_tombstone(const QoreTypeInfo* ti, bool& ok);
//! Returns true if the declaration is a tombstone
DLLLOCAL bool qore_is_type_tombstone(const void* decl);
//! Returns the numbers of tombstones made
DLLLOCAL void qore_type_tombstone_counts(size_t& classes, size_t& hashdecls, size_t& enums);
//! Frees the tombstones at library cleanup
DLLLOCAL void qore_free_type_tombstones();
//! Makes the tombstone of a declaration whose types a derived type names, while it is alive (QoreTypeOwnerBatch)
DLLLOCAL void qore_type_owner_make_tombstone(QoreTypeOwner* owner);

//! Holds back the free of what is retired after the epoch of the thread that armed it, while it lives (issue #6419)
/** For work that a thread hands to a thread that has no epoch of its own (a work item of the JIT's background compiler,
    whose IR the handing thread read): the scan of the threads counts each armed hold as a thread with that epoch.
    Movable with the work; empty until it is armed.
*/
class QoreTypeEpochHold {
public:
    DLLLOCAL QoreTypeEpochHold() = default;
    DLLLOCAL QoreTypeEpochHold(QoreTypeEpochHold&& old) : id(old.id) {
        old.id = 0;
    }
    DLLLOCAL QoreTypeEpochHold& operator=(QoreTypeEpochHold&& old) {
        if (this != &old) {
            release();
            id = old.id;
            old.id = 0;
        }
        return *this;
    }
    DLLLOCAL QoreTypeEpochHold(const QoreTypeEpochHold&) = delete;
    DLLLOCAL QoreTypeEpochHold& operator=(const QoreTypeEpochHold&) = delete;

    DLLLOCAL ~QoreTypeEpochHold() {
        release();
    }

    //! Records the epoch of the current thread (the current epoch if it runs no Qore code)
    DLLLOCAL void arm();

    //! Releases the hold
    DLLLOCAL void release();

    //! Releases the hold of work that a child process created with fork() lost with the thread that ran it
    /** Called by a fork handler in the child (see QoreJIT::forkChild()): nothing else would ever release it there, and
        it would keep everything retired since its epoch from being freed in the child (qore#7549)
    */
    DLLLOCAL void abandonAfterFork() noexcept;

private:
    //! the hold's key in the registry of holds, or 0
    uint64_t id = 0;
};

//! Returns the lowest epoch of the armed holds, or 0 if there is none (see QoreTypeEpochHold)
DLLLOCAL uint64_t qore_type_epoch_hold_min();

//! A concrete declaration of a generic hashdecl whose type arguments name user declarations keeps them (issue #6419)
/** They are retired, not freed, while it lives, as its members name their types; released with
    qore_type_owner_release_kept()
*/
DLLLOCAL void qore_type_owner_keep_type_args(QoreTypeOwner* keeper, const std::vector<const QoreTypeInfo*>& args);
//! Releases what qore_type_owner_keep_type_args() kept
DLLLOCAL void qore_type_owner_release_kept(QoreTypeOwner* keeper);

#endif
