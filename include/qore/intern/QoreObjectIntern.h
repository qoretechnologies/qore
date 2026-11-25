/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreObjectIntern.h

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

#ifndef _QORE_QOREOBJECTINTERN_H

#define _QORE_QOREOBJECTINTERN_H

#include "qore/intern/VRMutex.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <vector>

// Qore DCG / cycle check defines are in QoreLibIntern.h

#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/RSection.h"
#include "qore/intern/RSet.h"
#include "qore/intern/QoreProgramObjectLockOps.h"
#include "qore/intern/QoreMiscMembersOps.h"

#define OS_OK            0
#define OS_DELETED      -1

// object access constants
#define QOA_OK           0
#define QOA_PRIV_ERROR   1
#define QOA_PUB_ERROR    2

class LValueHelper;

//! the private:internal members of an object, per class, keyed by the class ID of the class that declares them
/** The class ID is the identity of the class: the copies of a class imported into other Programs share it, and a class
    with the same name and signature as another one (whose signature hash is the same) has its own (qore#7036)
*/
typedef std::map<qore_classid_t, QoreHashNode*> cdmap_t;

/*
  Qore internal class data is stored against the object with this data structure
  against its qore_classid_t (class ID).  In a class hierarchy, for private data
  that is actually a C++ subclass of Qore parent classes, then we save the same
  private data against the qore class IDs of the parent classes, but we set the
  flag to true, meaning that we will not delete the private data when
  the parent classes' destructors are run, but rather only when the subclass that
  actually owns data has its turn to destroy private object data.

  So basically, the second boolean flag just means - does this class ID actually
  own the private data or not - if it's false, then it does not actually own the data,
  but is compatible with the data, so parent class builtin (C++) methods will get
  passed this private data as if it belonged to this class and as if it were saved
  directly to the object in the class' constructor.

  please note that this flag is called the "virtual" flag elsewhere in the code here,
  meaning that the data only "virtually" belongs to the class
 */
typedef std::pair<AbstractPrivateData*, bool> private_pair_t;

// mapping from qore class ID to the object data
typedef std::map<qore_classid_t, private_pair_t> keymap_t;

//! called when an object releases private data or hands it out, before it can be dereferenced
/** A Program reports its user code to the garbage collector through the object that holds it; the object stops
    reporting it here, on every path that releases the Program (issue #5590; see
    qore_program_private::clearScanHolder()).
*/
DLLLOCAL void qore_object_private_data_released(qore_classid_t key, AbstractPrivateData* pd);

// for objects with multiple classes, private data has to be keyed
class KeyList {
private:
    keymap_t keymap;

public:
    DLLLOCAL AbstractPrivateData* getReferencedPrivateData(qore_classid_t key) const {
        keymap_t::const_iterator i = keymap.find(key);
        if (i == keymap.end()) {
            return nullptr;
        }

        AbstractPrivateData* apd = i->second.first;
        if (!apd) {
            return nullptr;
        }
        apd->ref();
        return apd;
    }

    DLLLOCAL bool checkData(qore_classid_t key) const {
        return keymap.find(key) != keymap.end();
    }

    DLLLOCAL void addToString(QoreString* str) const {
        for (keymap_t::const_iterator i = keymap.begin(), e = keymap.end(); i != e; ++i)
            str->sprintf("%d=<%p>, ", i->first, i->second.first);
    }

    DLLLOCAL void derefAll(ExceptionSink* xsink) const {
        for (keymap_t::const_iterator i = keymap.begin(), e = keymap.end(); i != e; ++i) {
            if (!i->second.second && i->second.first) {
                qore_object_private_data_released(i->first, i->second.first);
                i->second.first->deref(xsink);
            }
        }
    }

    DLLLOCAL AbstractPrivateData* getAndClearPtr(qore_classid_t key) {
        keymap_t::iterator i = keymap.find(key);
        //printd(5, "KeyList::getAndClearPtr this: %p, key: %d, end: %d\n", this, key, i == keymap.end());
        if (i == keymap.end() || i->second.second) {
            //printd(5, "KeyList::getAndClearPtr second: %d\n", i->second.second);
            return nullptr;
        }
        //printd(5, "KeyList::getAndClearPtr first: %p\n", i->second.first);
        // we must clear the private data when this function is called or a crash will result
        AbstractPrivateData* rv = i->second.first;
        if (rv) {
            qore_object_private_data_released(key, rv);
        }
        keymap.erase(i);
        // also erase virtual mappings that alias the same private data, so that
        // tryGetReferencedPrivateData() via a parent class ID cannot find a
        // dangling pointer after the caller derefs the returned pointer
        for (auto it = keymap.begin(); it != keymap.end(); ) {
            if (it->second.second && it->second.first == rv) {
                it = keymap.erase(it);
            } else {
                ++it;
            }
        }
        return rv;
    }

    DLLLOCAL AbstractPrivateData* getAndRemovePtr(qore_classid_t key) {
        keymap_t::iterator i = keymap.find(key);
        //printd(5, "KeyList::getAndRemovePtr this: %p, key: %d, end: %d\n", this, key, i == keymap.end());
        if (i == keymap.end() || i->second.second) {
            //printd(5, "KeyList::getAndRemovePtr second: %d\n", i->second.second);
            return 0;
        }
        AbstractPrivateData* rv = i->second.first;
        //printd(5, "KeyList::getAndRemovePtr first: %p\n", i->second.first);
        if (rv) {
            qore_object_private_data_released(key, rv);
        }
        i->second.first = 0;
        // also clear virtual mappings that alias the same private data, so that
        // tryGetReferencedPrivateData() via a parent class ID cannot find a
        // dangling pointer after the caller derefs the returned pointer
        for (auto& entry : keymap) {
            if (entry.second.second && entry.second.first == rv) {
                entry.second.first = nullptr;
            }
        }
        return rv;
    }

    DLLLOCAL void insert(qore_classid_t key, AbstractPrivateData* pd) {
        assert(pd);
        assert(keymap.find(key) == keymap.end());
        //printd(5, "KeyList::insert this: %p, key: %d, pd: %p\n", this, key, pd);
        keymap.insert(std::make_pair(key, std::make_pair(pd, false)));
    }

    DLLLOCAL void insertVirtual(qore_classid_t key, AbstractPrivateData* pd) {
        assert(pd);
        //printd(5, "KeyList::insertVirtual this: %p, key: %d, pd: %p, test: %d\n", this, key, pd,
        //    keymap.find(key) == keymap.end());
        if (keymap.find(key) == keymap.end())
            keymap.insert(std::make_pair(key, std::make_pair(pd, true)));
    }
};

class VRMutex;

//! scans the objects in a recursive set that a value removed from an object holds, directly or in its lists and hashes
/** Called after a write that removed the value from an object in a recursive set, and after the scan of that object,
    which does not reach them when the removed edge was its only path to them (#6335).
*/
DLLLOCAL void qore_scan_removed_value_objects(QoreValue v, RObject* root, ExceptionSink* xsink);

//! Marks the objects in \a v as constants of the whole process, whose members no code can change (qore#6749)
/** Called for the value of each builtin constant of the library and of binary modules (QoreNamespace::addConstant(),
    QoreClass::addBuiltinConstant()) and for the process-wide singleton objects of the library, before the objects are
    shared: one such object serves every Program and thread, so a member that one caller gave it would be read by
    every other caller, also by the code of other sandboxes.  The objects in the lists and hashes of \a v are marked
    too; only system objects (no Program owns them) are marked, as an object that a Program owns, such as one that a
    language bridge imports as a constant of a Program, is that Program's.  Adding a member to a marked object raises
    \c SYSTEM-OBJECT-ERROR (a class's member write gate, which owns the storage of its undeclared members, is
    evaluated first).
*/
DLLLOCAL void qore_object_mark_process_constant(QoreValue v);

//! Returns true if an object that a write removed from \a root needs a scan of its own after the root's (#6335)
/** Not if it is in no recursive set, nor if the removed value holds its only reference (it is freed when the value is
    released), nor if the root's scan reached it (it is in the root's valid set).

    The sets are compared by their pointers with no lock; a set is read only under the root's rsection, which keeps
    it alive (RObject::hasInValidSet()): another thread can recompute the root's set and free the one loaded here
    (#6792).
*/
DLLLOCAL bool qore_removed_object_needs_scan(RObject* o, RObject* root);

class qore_object_private : public RObject {
public:
    const QoreClass* theclass;
    const QoreTypeInfo* instantiated_type = nullptr;
    //! OS_OK, the TID of the thread running the destructor, or OS_DELETED
    /** Changed under the object's write lock, but read without it where a stale value only defers a decision: by
        the dereference fast paths, and by RSet::canDelete() and the scan for the other objects of a recursive set,
        whose locks it does not hold while another thread can be deleting them.  Atomic for those reads.
    */
    std::atomic<int> status{OS_OK};

    KeyList* privateData = nullptr;
    // member data
    QoreHashNode* data;
    QoreProgram* pgm;
    //! Immutable creation context; does not retain a Program or any Qore object
    QoreSandboxContext* sandbox_provenance = nullptr;
    //! true if the object is a capability (qore#7052): an object of module code that the host's code created without
    //! restrictions; a call of one of its methods that enters the Program of its code marks the context that it
    //! enters as a grant (see ProgramThreadCountContextHelper::set() and ThreadData::CapabilityFrame); read only when
    //! a call enters another Program, never by calls within one
    bool capability_origin = false;
    cdmap_t* cdmap = nullptr;

    // used for garbage collection
    mutable unsigned obj_count = 0;

    mutable VRMutex gate;

    //! The number of private data containers whose values the scanner has to report for this object
    /** Changed under rlck, in the critical section of the container's own lock in which its count changes; read
        without rlck by the scan (needsScan(), scanMembers(), valuesCanChangeWithoutScan()), which only needs a value
        that was current at some point, so it is atomic and read with relaxed ordering.  A container that a scan
        misses because of a stale read has marked the edges it added (qore_dgc_value_stored()).
    */
    std::atomic<int> scan_private_data{0};

    //! A class ID registered with QoreObject::addPrivateDataScan() and the number of registrations for it
    typedef std::vector<std::pair<qore_classid_t, int>> private_data_scan_list_t;

    //! The private data that the scanner reports with AbstractScannablePrivateData::scanValues(), or nullptr
    /** Allocated by the first registration, as most objects never have one; changed and read under rlck.  Each
        registration is also counted in scan_private_data.
    */
    private_data_scan_list_t* private_data_scans = nullptr;

    bool system_object;
    //! set when the object is a constant of the whole process: its members cannot be changed (qore#6749)
    /** An object that a builtin constant of the library or of a binary module holds, and a process-wide singleton
        object of the library, is one object for every Program and thread: a member that any code gave it would be
        seen by all other code, also by the code of other sandboxes.  Set before the object is shared (see
        qore_object_mark_process_constant()) and never cleared, so it is read without a lock.
    */
    bool process_constant = false;
    //! set when the object's destruction has started; read without the object's locks as status is
    std::atomic<bool> in_destructor;
    bool recursive_ref_found;

    QoreObject* obj;

#ifdef DEBUG
    //! The weak references that the dereferences taking the count to zero may release
    /** The object's base weak reference, plus one for each reference taken when the count was zero (a destructor
        making its object reachable again, or the deletion's own reference when others released the count to zero
        first; see takeDeleteReference()).  Each dereference that takes the count to zero once the object is being
        destroyed releases one of them, and they must never run out: a release beyond them frees the object while
        other weak references (the self reference of a poll operation's private data, for one) still use it.
    */
    std::atomic<int> dbg_zero_count_weak_refs{1};

    //! Accounts for the release of a weak reference owed to a dereference that took the count to zero
    DLLLOCAL void dbgReleaseZeroCountWeakRef() {
        int rv = --dbg_zero_count_weak_refs;
        if (rv < 0) {
            printd(0, "qore_object_private::dbgReleaseZeroCountWeakRef() this: %p '%s': weak reference released "
                "for a dereference to zero that had none\n", this, getName());
            assert(false);
        }
    }
#endif

    //! Unique hash computed lazily on first getUniqueHash() call — stable for
    //! the object's lifetime and globally unique even when malloc reuses
    //! addresses. Deferred to keep the constructor cheap (the SHA1+sprintf
    //! chain consumes stack that can push deep recursion past the stack guard).
    mutable std::atomic<QoreStringNode*> unique_hash{nullptr};

    DLLLOCAL qore_object_private(QoreObject* n_obj, const QoreClass *oc, QoreProgram* p, QoreHashNode* n_data,
            const QoreTypeInfo* n_instantiated_type = nullptr);

    DLLLOCAL ~qore_object_private();

    DLLLOCAL void incScanPrivateData();
    DLLLOCAL void decScanPrivateData();

    //! Registers private data for AbstractScannablePrivateData::scanValues(); see QoreObject::addPrivateDataScan()
    DLLLOCAL void addPrivateDataScan(qore_classid_t key);
    //! Undoes one registration of addPrivateDataScan(); see QoreObject::removePrivateDataScan()
    DLLLOCAL void removePrivateDataScan(qore_classid_t key);

    DLLLOCAL void plusEquals(const AbstractQoreNode* v, AutoVLock& vl, SafeDerefHelper& sdh, ExceptionSink* xsink) {
        if (!v) {
            return;
        }

        // do not need ensure_unique() for objects
        if (v->getType() == NT_OBJECT) {
            merge(*const_cast<QoreObject*>(reinterpret_cast<const QoreObject*>(v))->priv, vl, sdh, xsink);
        } else if (v->getType() == NT_HASH) {
            merge(reinterpret_cast<const QoreHashNode*>(v), vl, sdh, xsink);
        }
    }

    DLLLOCAL void plusEquals(const AbstractQoreNode* v, AutoVLock& vl, SafeDerefHelper& sdh, ExceptionSink* xsink,
            LexicalSemanticPolicy policy) {
        if (!v) {
            return;
        }
        if (v->getType() == NT_OBJECT) {
            merge(*const_cast<QoreObject*>(static_cast<const QoreObject*>(v))->priv, vl, sdh, xsink, policy);
        } else if (v->getType() == NT_HASH) {
            merge(static_cast<const QoreHashNode*>(v), vl, sdh, xsink, policy);
        }
    }

    DLLLOCAL void merge(qore_object_private& o, AutoVLock& vl, SafeDerefHelper& sdh, ExceptionSink* xsink);

    DLLLOCAL void merge(qore_object_private& o, AutoVLock& vl, SafeDerefHelper& sdh, ExceptionSink* xsink,
            LexicalSemanticPolicy policy);

    template <bool Lexical>
    DLLLOCAL void mergeImpl(qore_object_private& o, AutoVLock& vl, SafeDerefHelper& sdh, ExceptionSink* xsink,
            LexicalSemanticPolicy policy);

    DLLLOCAL void merge(const QoreHashNode* h, AutoVLock& vl, SafeDerefHelper& sdh, ExceptionSink* xsink,
            LexicalSemanticPolicy policy);

    template <bool Lexical>
    DLLLOCAL void mergeImpl(const QoreHashNode* h, AutoVLock& vl, SafeDerefHelper& sdh, ExceptionSink* xsink,
            LexicalSemanticPolicy policy);

    DLLLOCAL int mergeAccessorMembers(const QoreHashNode* ext, const qore_class_private* class_ctx,
        AutoVLock& vl, ExceptionSink* xsink,
            LexicalSemanticPolicy policy);

    template <bool Lexical>
    DLLLOCAL int mergeAccessorMembersImpl(const QoreHashNode* ext, const qore_class_private* class_ctx,
        AutoVLock& vl, ExceptionSink* xsink,
            LexicalSemanticPolicy policy);

    DLLLOCAL void mergeIntern(ExceptionSink* xsink, const QoreHashNode* h, bool& check_recursive,
        const qore_class_private* class_ctx, SafeDerefHelper& sdh, const QoreHashNode* new_internal_data,
            LexicalSemanticPolicy policy);

    template <bool Lexical>
    DLLLOCAL void mergeInternImpl(ExceptionSink* xsink, const QoreHashNode* h, bool& check_recursive,
        const qore_class_private* class_ctx, SafeDerefHelper& sdh, const QoreHashNode* new_internal_data,
            LexicalSemanticPolicy policy);

    //! merges the members of the object that the current class context can use into \a hash
    DLLLOCAL void mergeDataToHash(QoreHashNode* hash, SafeDerefHelper& sdh, ExceptionSink* xsink,
            LexicalSemanticPolicy policy) const;

    //! merges the members of the object that the current class context can use into \a hash when this thread holds
    //! the object's lock for writing already (the lvalue of the merge is a member of this object; qore#7107)
    DLLLOCAL void mergeDataToHashHeld(QoreHashNode* hash, SafeDerefHelper& sdh, ExceptionSink* xsink,
            LexicalSemanticPolicy policy) const;

    template <bool Lexical, bool Lock>
    DLLLOCAL void mergeDataToHashImpl(QoreHashNode* hash, SafeDerefHelper& sdh, ExceptionSink* xsink,
            LexicalSemanticPolicy policy) const;

    template <bool Lexical>
    DLLLOCAL void mergeDataToHashIntern(QoreHashNode* hash, SafeDerefHelper& sdh, ExceptionSink* xsink,
            LexicalSemanticPolicy policy, const qore_class_private* class_ctx,
            const qore_class_private* storage_ctx) const;

    //! a declared member that a class context with no private access to the object's class uses on the object, and the
    //! storage that holds its value (qore#7180)
    struct ContextStorageMember {
        const char* key;
        const QoreHashNode* store;
    };

    //! returns the declared members that a class context with no private access to the object's class uses on the
    //! object, in the order in which they were declared, with the storage of their values; the object's lock must be held
    /** A class that is on the path of a private:internal inheritance to a member (the class that declares it, and the
        classes between it and the class that inherits it with private:internal) uses the member on the objects of the
        subclasses of that class, as through self, and the member's value is in the storage that the class context has
        for it, which is not the object's data hash (issue #6800): the enumerations of the members list it as the
        access of the member finds it.  The keys are valid as long as the object's class lives; the storages while the
        object's lock is held.

        The members are found in the internal data of the object, so an object with none has no such member, and
        nothing is done for a class context that is not in the object's hierarchy.
    */
    DLLLOCAL void getContextStorageMembers(const qore_class_private* storage_ctx,
            std::vector<ContextStorageMember>& out) const;

    DLLLOCAL void merge(const QoreHashNode* h, AutoVLock& vl, SafeDerefHelper& sdh, ExceptionSink* xsink);

    DLLLOCAL void mergeIntern(ExceptionSink* xsink, const QoreHashNode* h, bool& check_recursive,
        const qore_class_private* class_ctx, SafeDerefHelper& sdh, const QoreHashNode* new_internal_data = nullptr);

    //! moves the values of declared members held in external storage from a hash to be merged into \a ext
    /** @param h the hash to merge, which is modified only if it has such members; the caller owns it
        @param ext receives a hash of the values for such members, if there are any
    */
    DLLLOCAL void takeAccessorMembersForMerge(QoreHashNode* h, ReferenceHolder<QoreHashNode>& ext) const;

    //! writes the values of declared members held in external storage that are merged into the object
    /** called without any lock on the object; the lvalue lock in \a vl is released first, since the setters can
        run any code; access and the declared types are checked as for an assignment

        @return 0 for OK, -1 if an exception was raised
    */
    DLLLOCAL int mergeAccessorMembers(const QoreHashNode* ext, const qore_class_private* class_ctx, AutoVLock& vl,
        ExceptionSink* xsink);

    DLLLOCAL QoreHashNode* copyData(ExceptionSink* xsink, bool throw_exception = true) const;

    DLLLOCAL int getLValue(const char* key, LValueHelper& lvh, const qore_class_private* class_ctx, bool for_remove,
            ExceptionSink* xsink);

    //! Raises \c SYSTEM-OBJECT-ERROR and returns -1 if the member write is refused (qore#6749)
    /** see checkProcessConstantWrite() and checkForeignBuiltinMemberWrite(); called only where a member is created
        (the miss path of a member write) and for merges, so that a write to a member that exists, such as a declared
        member, costs nothing more

        @param mem the member written, or nullptr for a merge
        @param h the members merged, or nullptr for a member write
        @param class_ctx the class context of the write
    */
    DLLLOCAL int checkMemberWrite(const char* mem, const QoreHashNode* h, const qore_class_private* class_ctx,
            ExceptionSink* xsink) const {
        // only a constant of the whole process or an object of a builtin class
        if (!process_constant && !qore_class_private::get(*theclass)->sys) [[likely]] {
            return 0;
        }
        return checkProcessConstantWrite(mem, xsink) || checkForeignBuiltinMemberWrite(mem, h, class_ctx, xsink)
            ? -1 : 0;
    }

    //! Raises \c SYSTEM-OBJECT-ERROR and returns -1 if the object is a constant of the whole process (qore#6749)
    /** @param mem the member written, or nullptr for a merge
    */
    DLLLOCAL int checkProcessConstantWrite(const char* mem, ExceptionSink* xsink) const {
        if (!process_constant) [[likely]] {
            return 0;
        }
        return raiseProcessConstantWrite(mem, xsink);
    }

    //! Raises \c SYSTEM-OBJECT-ERROR for a member write to a constant of the whole process; returns -1
    DLLLOCAL int raiseProcessConstantWrite(const char* mem, ExceptionSink* xsink) const;

    //! Raises \c SYSTEM-OBJECT-ERROR and returns -1 if sandboxed code gives a member to a foreign builtin object
    /** The code of a sandboxed Program cannot give an object of a builtin class without a public member list,
        which another Program or the library created, a member that the class does not declare (qore#6749): the
        object can be shared, as one that a module keeps in a static member is, and the member would be read by other
        callers, also by the code of other sandboxes.  The members that the class declares (the private and internal
        members of a builtin class, which its C++ code writes) and the objects that the Program created are not
        affected.

        @param mem the member written, or nullptr for a merge
        @param h the members merged, or nullptr for a member write
        @param class_ctx the class context of the write
    */
    DLLLOCAL int checkForeignBuiltinMemberWrite(const char* mem, const QoreHashNode* h,
            const qore_class_private* class_ctx, ExceptionSink* xsink) const;

    DLLLOCAL QoreStringNode* firstKey(ExceptionSink* xsink) {
        // get the current class context
        const qore_class_private* class_ctx = runtime_get_class();
        if (class_ctx && !qore_class_private::runtimeCheckPrivateClassAccess(*theclass, class_ctx))
            class_ctx = nullptr;

        QoreAutoVarRWReadLocker al(rml);

        if (status == OS_DELETED) {
            makeAccessDeletedObjectException(xsink, theclass->getName());
            return nullptr;
        }

        // all the members only in the object's class: another class context sees the members that it can use (issue
        // #6760)
        if (class_ctx == qore_class_private::get(*theclass)) {
            const char* str = data->getFirstKey();
            //printd(5, "qore_object_private::firstKey() got %p (%s)\n", str, str ? str : "<null>");
            return !str ? nullptr : new QoreStringNode(str);
        }

        // get first accessible non-internal member
        ConstHashIterator hi(data);
        while (hi.next()) {
            //printd(5, "qore_object_private::firstKey() checking '%s'\n", hi.getKey());
            const qore_class_private* member_class_ctx = nullptr;
            if (!checkMemberAccessIntern(hi.getKey(), false, class_ctx, member_class_ctx))
                return new QoreStringNode(hi.getKey());
            //printd(5, "qore_object_private::firstKey() skipping '%s' (private)\n", hi.getKey());
        }

        return nullptr;
    }

    DLLLOCAL QoreStringNode* lastKey(ExceptionSink* xsink) {
        // get the current class context
        const qore_class_private* class_ctx = runtime_get_class();
        if (class_ctx && !qore_class_private::runtimeCheckPrivateClassAccess(*theclass, class_ctx))
            class_ctx = nullptr;

        QoreAutoVarRWReadLocker al(rml);

        if (status == OS_DELETED) {
            makeAccessDeletedObjectException(xsink, theclass->getName());
            return nullptr;
        }

        // all the members only in the object's class (issue #6760)
        if (class_ctx == qore_class_private::get(*theclass)) {
            const char* str = data->getLastKey();
            return !str ? nullptr : new QoreStringNode(str);
        }

        // get last accessible non-internal member
        ReverseConstHashIterator hi(data);
        while (hi.next()) {
            const qore_class_private* member_class_ctx = nullptr;
            if (!checkMemberAccessIntern(hi.getKey(), false, class_ctx, member_class_ctx))
                return new QoreStringNode(hi.getKey());
        }

        return nullptr;
    }

    DLLLOCAL QoreHashNode* getSlice(const QoreListNode* l, ExceptionSink* xsink) const;

    DLLLOCAL int checkMemberAccessIntern(const char* mem, bool has_public_members,
            const qore_class_private* class_ctx, const qore_class_private*& member_class_ctx) const {
        assert(!member_class_ctx);

        const qore_class_private* theclass_priv = qore_class_private::get(*theclass);
        const QoreMemberInfo* info = theclass_priv->runtimeGetMemberInfo(mem, class_ctx);
        if (!info) {
            return has_public_members ? QOA_PUB_ERROR : QOA_OK;
        }
        member_class_ctx = info->getClassContext(class_ctx);
        return theclass_priv->runtimeCheckMemberAccess(info, class_ctx) ? QOA_OK : QOA_PRIV_ERROR;
    }

    //! raises OBJECT-ALREADY-DELETED and returns true if the object is deleted
    /** takes the object's read lock unless \a vl holds the object's lock: the lvalue is then a member of the object
        itself, whose lock this thread holds for writing
    */
    DLLLOCAL bool raiseIfDeleted(const AutoVLock& vl, ExceptionSink* xsink) const {
        QoreSafeVarRWReadLocker sl(rml, qore_auto_vlock_ops(&vl).getRWL() != &rml);
        if (status == OS_DELETED) {
            makeAccessDeletedObjectException(xsink, theclass->getName());
            return true;
        }
        return false;
    }

    // must be called in the object read lock
    DLLLOCAL const QoreHashNode* getInternalData(const qore_class_private* class_ctx) const {
        if (!cdmap)
            return nullptr;
        cdmap_t::const_iterator i = cdmap->find(class_ctx->classID);
        return i != cdmap->end() ? i->second : nullptr;
    }

    // must be called in the object read lock
    DLLLOCAL QoreHashNode* getInternalData(const qore_class_private* class_ctx) {
        if (!cdmap)
            return nullptr;
        cdmap_t::iterator i = cdmap->find(class_ctx->classID);
        return i != cdmap->end() ? i->second : nullptr;
    }

    // must be called in the object write lock
    DLLLOCAL QoreHashNode* getCreateInternalData(const qore_class_private* class_ctx);

    // perform a shallow copy of all object data when executing a copy method
    /** @return -1 for error, 0 for OK
    */
    DLLLOCAL int copyData(ExceptionSink* xsink, const qore_object_private& old);

    DLLLOCAL void setValue(const char* key, QoreValue val, ExceptionSink* xsink);

    DLLLOCAL void setValueIntern(const qore_class_private* class_ctx, const char* key, QoreValue val,
            ExceptionSink* xsink);

    DLLLOCAL int checkMemberAccess(const char* mem, const qore_class_private* class_ctx,
            const qore_class_private*& member_class_ctx) const;

    //! as checkMemberAccess() for the member name \a mem of \a len bytes (qore#7194)
    DLLLOCAL int checkMemberAccess(const char* mem, size_t len, const qore_class_private* class_ctx,
            const qore_class_private*& member_class_ctx) const;

    //! returns the value of the member \a mem of \a len bytes as QoreObject::evalMember() (qore#7194)
    DLLLOCAL QoreValue evalMember(const char* mem, size_t len, ExceptionSink* xsink);

    //! Reads a unique container from an ordinary stored member under the object lock (qore#7301).
    DLLLOCAL bool tryReadUniqueWeakMember(const std::string& name, QoreValue& result);

    //! returns the value of the member \a mem of \a obj as QoreObject::evalMember() (qore#7194)
    /** Not inline, so that a call from the IR interpreter's dispatch loop is as small as a call of
        QoreObject::evalMember(); \a mem is a key without a NUL character (made from a C string)
    */
    DLLLOCAL static QORE_INTERN_NOINLINE QoreValue evalMember(QoreObject* obj, const std::string& mem,
            ExceptionSink* xsink);

    DLLLOCAL int checkMemberAccess(const char* mem, const qore_class_private* class_ctx,
            const qore_class_private*& member_class_ctx, ExceptionSink* xsink) const {
        int rc = checkMemberAccess(mem, class_ctx, member_class_ctx);
        if (!rc) {
            return 0;
        }

        if (rc == QOA_PRIV_ERROR) {
            doPrivateException(mem, xsink);
        } else {
            doPublicException(mem, xsink);
        }
        return -1;
    }

    /** @param declared if not null, set to true if the member is declared by the class, false if not (a member that
        can be added to and removed from the object)
    */
    DLLLOCAL int checkMemberAccessGetTypeInfo(ExceptionSink* xsink, const char* mem,
            const qore_class_private* class_ctx, const qore_class_private*& member_class_ctx,
            const QoreTypeInfo*& typeInfo, bool* declared = nullptr) const {
        return checkMemberAccessGetTypeInfo(xsink, mem, strlen(mem), class_ctx, member_class_ctx, typeInfo,
            declared);
    }

    //! as above for the member name \a mem of \a len bytes (qore#7194)
    DLLLOCAL int checkMemberAccessGetTypeInfo(ExceptionSink* xsink, const char* mem, size_t len,
            const qore_class_private* class_ctx, const qore_class_private*& member_class_ctx,
            const QoreTypeInfo*& typeInfo, bool* declared = nullptr) const {
        assert(!member_class_ctx);
        const qore_class_private* theclass_priv = qore_class_private::get(*theclass);
        const QoreMemberInfo* mi = theclass_priv->runtimeGetMemberInfo(mem, len, class_ctx);
        if (declared) {
            *declared = mi != nullptr;
        }
        if (mi) {
            if (!theclass_priv->runtimeCheckMemberAccess(mi, class_ctx)) {
                doPrivateException(mem, xsink);
                return -1;
            }

            member_class_ctx = mi->getClassContext(class_ctx);
            // the type of the declaration whose storage the access uses (issue #6670)
            typeInfo = qore_substitute_type_params_if_needed(mi->getStorageTypeInfo(mem, member_class_ctx),
                instantiated_type ? instantiated_type : theclass->getTypeInfo());
            return 0;
        }

        // member is not declared
        if (theclass->hasPublicMembersInHierarchy()) {
            doPublicException(mem, xsink);
            return -1;
        }
        return 0;
    }

    DLLLOCAL QoreValue takeMember(ExceptionSink* xsink, const char* mem, bool check_access = true,
        bool for_delete = false, bool* exists = nullptr);

    //! Takes the value of a member out of the object's member hash \a odata, which must be locked for writing
    /** A declared member keeps its slot, with no value; the key of a member that the class does not declare is removed,
        as from a hash, so that the object's keys no longer report it (qore#7179).  A missing member is not created.
    */
    DLLLOCAL QoreValue takeMemberValue(QoreHashNode* odata, const char* key, bool declared, bool& exists);

    //! Removes a mixed native/ordinary member slice after detaching its parent lvalue
    DLLLOCAL __attribute__((noinline)) void takeExternalMembers(QoreLValueGeneric& rv, LValueHelper& lvh,
        const QoreListNode* l, bool for_delete);

    DLLLOCAL QoreValue takeMember(LValueHelper& lvh, const char* mem, bool for_delete = false, bool* exists = nullptr);

    DLLLOCAL void takeMembers(QoreLValueGeneric& rv, LValueHelper& lvh, const QoreListNode* l, bool for_delete = false);

    // Self member access can use external storage for undeclared members of proxy classes.
    DLLLOCAL QoreValue getReferencedMemberForSelf(const char* mem, ExceptionSink* xsink) const;

    DLLLOCAL QoreValue getReferencedMemberNoMethod(const char* mem, ExceptionSink* xsink) const;

    DLLLOCAL QoreValue getReferencedMemberNoMethodResolved(const char* mem,
            const qore_class_private* member_class_ctx,
            ExceptionSink* xsink) const;

    //! as getReferencedMemberNoMethodResolved(), for a member that the caller found to have no accessors
    DLLLOCAL QoreValue getReferencedMemberNoMethodResolvedNoAccessor(const char* mem,
            const qore_class_private* member_class_ctx,
            ExceptionSink* xsink) const;

    DLLLOCAL QoreValue getReferencedMemberNoMethodResolvedPrehashed(
            const char* mem, size_t hash,
            const qore_class_private* member_class_ctx,
            ExceptionSink* xsink) const;

    DLLLOCAL int getLValueResolved(const char* key,
            const QoreMemberInfo& info,
            const qore_class_private* class_ctx, LValueHelper& lvh,
            bool for_remove, ExceptionSink* xsink);

    DLLLOCAL int getLValueResolvedPrehashed(const char* key, size_t hash,
            const QoreMemberInfo& info,
            const qore_class_private* class_ctx, LValueHelper& lvh,
            bool for_remove, ExceptionSink* xsink);

    //! Assigns an exact scalar member without generic lvalue bookkeeping.
    /** @param key member name
        @param hash precomputed member-name hash, or 0
        @param info resolved member declaration
        @param member_class_ctx object storage context for the member
        @param value scalar value to assign
        @param expected_type exact scalar type required for both member and value
        @param xsink exception sink
        @return 1 when assigned, 0 when the generic path is required, -1 on error
    */
    DLLLOCAL int tryAssignScalarMemberResolvedPrehashed(const char* key,
            size_t hash, const QoreMemberInfo& info,
            const qore_class_private* member_class_ctx, QoreValue value,
            qore_type_t expected_type, ExceptionSink* xsink);

    //! Runs the destructor and releases the object's data
    /** Lock not held on entry.

        @param ref_taken true if the reference that the deletion holds while the destructor runs has been taken
        already (takeDeleteReference(), when a dereference decided to delete the object), false to take it here (an
        explicit deletion, whose caller holds a reference of its own)
    */
    DLLLOCAL void doDeleteIntern(ExceptionSink* xsink, bool ref_taken = false);

    //! Takes the reference that the deletion holds while the destructor runs
    /** Called by the dereference that decided to delete the object, with the object's lock held exclusively, in
        the critical section that marks the object as being destroyed: from then on, a dereference that takes the
        count to zero finds the object being destroyed, and releases a weak reference of the object (see
        robject_dereference_helper::finalDeref()).

        When the deciding dereference released the last reference itself, that release is the deletion's: the count
        is zero, and the release of the deletion's reference is the one that releases the object's base weak
        reference.  When the dereference decided to collect the object with references remaining, which happens when
        all of them are held by other members of its recursive set, they can be released before this, by threads
        that reach the members through weak references; the dereference that took the count to zero then releases
        the base weak reference, so the deletion's reference, taken when the count is zero, takes a weak reference of
        its own for its release, as any reference taken when the count is zero does (customRefIntern()).  Taking the
        reference after the object had been marked, and the deciding dereference had completed, let the count reach
        zero twice: the base weak reference was released twice, which freed the object while weak references to it
        were still held.

        @param refs_remained true if references remained when the dereference decided to delete the object
    */
    DLLLOCAL void takeDeleteReference(bool refs_remained) {
        // no rlck: the count is atomic, as in customRefIntern(); the object's lock is held exclusively
        if (!references.fetch_add(1, std::memory_order_acq_rel) && refs_remained) {
            tRef();
#ifdef DEBUG
            ++dbg_zero_count_weak_refs;
#endif
        }
    }

    DLLLOCAL void cleanup(ExceptionSink* xsink, QoreHashNode* td, cdmap_t* cdm);

    //! Invalidate an incomplete object while its owner keeps its reference alive.
    /** Clears member and native storage without calling user destructors or
        consuming the caller's reference. Failed graph deserialization uses this
        before releasing any indexed owners, so cycles cannot retain partial objects.
    */
    DLLLOCAL void obliterateMembers(ExceptionSink* xsink);

    // this method is called when there is an exception in a constructor and the object should be deleted
    DLLLOCAL void obliterate(ExceptionSink* xsink) {
        printd(5, "qore_object_private::obliterate() obj: %p class: %s %d->%d\n", obj, theclass->getName(),
            obj->references.load(), obj->references.load() - 1);

#ifdef QORE_DEBUG_OBJ_REFS
        printd(QORE_DEBUG_OBJ_REFS, "qore_object_private::obliterate() this: %p class: %s: references %d->%d\n", this,
            theclass->getName(), obj->references.load(), obj->references.load() - 1);
#endif

        {
            AutoLocker slr(rlck);
            if (--obj->references) {
                return;
            }
        }

        obliterateMembers(xsink);
#ifdef DEBUG
        dbgReleaseZeroCountWeakRef();
#endif
        tDeref();
    }

    DLLLOCAL void doPrivateException(const char* mem, ExceptionSink* xsink) const {
        xsink->raiseException("PRIVATE-MEMBER", "'%s' is a private member of class '%s'", mem,
            qore_class_private::get(*theclass)->memberDiagnosticName());
    }

    DLLLOCAL void doPublicException(const char* mem, ExceptionSink* xsink) const {
        xsink->raiseException("INVALID-MEMBER", "'%s' is not a registered member of class '%s'", mem,
            qore_class_private::get(*theclass)->memberDiagnosticName());
    }

    DLLLOCAL virtual const char* getName() const {
        return theclass->getName();
    }

    DLLLOCAL virtual void deleteObject() {
        // Defense in depth: firing deleteObject() while the object is still inside its own user
        // destructor (data/cdmap/rset not yet cleared in doDeleteIntern) means a tDeref somewhere
        // took tRefs to zero too early — almost always a self-weak-ref pattern in private data
        // (the SocketPollOperationBase case).  Catch this specifically; the default
        // ~qore_object_private asserts would also trip but with a less informative message.
        assert(!(in_destructor && data) && "reentrant deleteObject during execDestructor — "
            "a tDeref dropped tRefs to zero while the user destructor was still running; "
            "suspect a self-weak-ref in private data (e.g. SocketPollOperationBase)");
        delete obj;
    }

    DLLLOCAL virtual void releaseCycleReference(ExceptionSink* xsink) {
        customDeref(xsink, false);
    }

    DLLLOCAL virtual bool isValidImpl() const {
        if (status != OS_OK || in_destructor) {
            printd(QRO_LVL, "qore_object_intern::isValidImpl() this: %p cannot delete graph obj status: %d "
                "in_destructor: %d\n", this, status.load(), (int)in_destructor.load());
            return false;
        }
        return true;
    }

    //! Private data containers hold values that their own methods change without scanning this object
    DLLLOCAL virtual bool valuesCanChangeWithoutScan() const {
        return scan_private_data.load(std::memory_order_relaxed) != 0;
    }

    DLLLOCAL virtual bool scanMembersIntern(RSetHelper& rsh, QoreHashNode* odata);

    DLLLOCAL virtual bool scanMembers(RSetHelper& rsh);

    // always called in the rsection lock, which a scan that changes nothing holds in shared mode
    DLLLOCAL virtual bool needsScan(bool scan_now) {
        assert(rml.checkRSectionHeld());
        printd(5, "qore_object_private::needsScan() scan_count: %d scan_private_data: %d scan_now: %d\n",
            getScanCount(), scan_private_data.load(std::memory_order_relaxed), scan_now);

        // the status cannot change while this lock is held
        if ((!getScanCount() && !scan_private_data.load(std::memory_order_relaxed)) || status != OS_OK) {
            return false;
        }

        // Starting a scan at an object with real references is still deferred by checkDeferScan().
        // Once a scan has reached this object, however, its outgoing edges are needed to establish the
        // complete cycle. Skipping them can omit a live owner of a shared container from the rset,
        // allowing the objects behind that container to be collected while the owner is still in use.
        // The r-section protects the member walk; real references remain external to the resulting rset.
        AutoLocker al(rlck);
        if (rrefs) {
            deferred_scan = true;
        } else if (scan_now) {
            deferred_scan = false;
        }
        return true;
    }

    //! merges the members of the object that the current class context can use into \a hash
    DLLLOCAL void mergeDataToHash(QoreHashNode* hash, SafeDerefHelper& sdh, ExceptionSink* xsink) const;

    //! merges the members of the object that the current class context can use into \a hash when this thread holds
    //! the object's lock for writing already (the lvalue of the merge is a member of this object; qore#7107)
    DLLLOCAL void mergeDataToHashHeld(QoreHashNode* hash, SafeDerefHelper& sdh, ExceptionSink* xsink) const;

    DLLLOCAL bool checkData(qore_classid_t key) {
        if (!privateData) {
            return false;
        }
        return privateData->checkData(key);
    }

    DLLLOCAL void setPrivate(qore_classid_t key, AbstractPrivateData* pd) {
        if (!privateData) {
            privateData = new KeyList;
        }
        printd(5, "qore_object_private::setPrivate() this: %p 2:privateData: %p (%s) key: %d pd: %p\n", this,
            privateData, theclass->getName(), key, pd);
        privateData->insert(key, pd);
        addVirtualPrivateData(key, pd);
    }

    // add virtual IDs for private data to class list
    DLLLOCAL void addVirtualPrivateData(qore_classid_t key, AbstractPrivateData* apd);

    DLLLOCAL AbstractPrivateData* getAndRemovePrivateData(qore_classid_t key, ExceptionSink* xsink) {
        QoreSafeVarRWWriteLocker sl(rml);
        return privateData ? privateData->getAndRemovePtr(key) : nullptr;
    }

    DLLLOCAL AbstractPrivateData* getReferencedPrivateData(qore_classid_t key, ExceptionSink* xsink) const;

    DLLLOCAL AbstractPrivateData* tryGetReferencedPrivateData(qore_classid_t key, ExceptionSink* xsink) const;

    //! Returns referenced private data for a deterministic garbage collection scan, or nullptr
    /** Never raises an exception.  A scan probes every scannable object for each private data type that can hold
        references, so an object of another class (or one already deleted) is the normal case, not an error.

        Raising here would also capture a call stack, and call stack capture calls external language stack
        location helpers - the Python helper acquires the GIL - while the scan holds r-sections that other threads
        wait for.  A thread holding the GIL and waiting for such an object then deadlocks with the scan.

        @param key the class ID of the private data to return

        @return the referenced private data (the caller must dereference it), or nullptr if the object is deleted
        or has no private data for \a key
    */
    DLLLOCAL AbstractPrivateData* getScanPrivateData(qore_classid_t key) const;

    //! Reports the values of the private data registered with addPrivateDataScan() to a scan
    /** @param rsh the scan
        @param xsink receives private data dereferences only; see scanMembers()

        @return true if the scan must end
    */
    DLLLOCAL bool scanRegisteredPrivateData(RSetHelper& rsh, ExceptionSink& xsink);

    DLLLOCAL QoreValue evalBuiltinMethodWithPrivateData(const QoreMethod& method,
            const BuiltinNormalMethodVariantBase* meth, const QoreListNode* args, RuntimeConfig& rc,
            ExceptionSink* xsink);

    // no locking necessary; if class_ctx is non-null, an internal member is being initialized
    DLLLOCAL QoreValue& getMemberValueRefForInitialization(const char* member, const qore_class_private* class_ctx);

    //! returns member data of the object (or 0 if there's an exception)
    /** private members are excluded if called outside the class, caller owns the QoreHashNode reference returned

        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param class_ctx class scope of the code requesting the members
        @return member data of the object
    */
    DLLLOCAL QoreHashNode* getRuntimeMemberHash(ExceptionSink* xsink,
        const qore_class_private* class_ctx = runtime_get_class()) const;

    //! returns the names of the members that the calling code can see, as the keys of getRuntimeMemberHash() (#7048)
    /** raises an exception and returns nullptr if the object has been deleted

        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return the names of the members, caller owns the reference returned
    */
    DLLLOCAL QoreListNode* getRuntimeMemberList(ExceptionSink* xsink) const;

    //! returns the number of the members that the calling code can see, as the size of getRuntimeMemberHash() (#7048)
    /** returns 0 if the object has been deleted

        @return the number of the members
    */
    DLLLOCAL int64 getRuntimeMemberCount() const;

    //! getRuntimeMemberList() for a class with non-public members or an object with internal data
    DLLLOCAL QORE_INTERN_NOINLINE QoreListNode* getRuntimeMemberListSlow(ExceptionSink* xsink) const;

    //! getRuntimeMemberCount() for a class with non-public members or an object with internal data
    DLLLOCAL QORE_INTERN_NOINLINE int64 getRuntimeMemberCountSlow() const;

    //! returns @ref true if the calling code can see a member of the object; stops at the first one (#7048)
    DLLLOCAL bool hasRuntimeMembers() const;

    //! returns the first name of getRuntimeMemberList() for the class context; the object's lock must be held
    /** the name is valid while the lock is held
    */
    DLLLOCAL const char* getFirstRuntimeMemberKeyIntern(const qore_class_private* class_ctx) const;

    DLLLOCAL void incScanCount(int dt) {
        assert(dt);
#ifdef QORE_DEBUG_OBJ_REFS
        printd(QORE_DEBUG_OBJ_REFS, "qore_object_private::incScanCount() this: %p dt: %d: %d -> %d\n", this, dt,
            obj_count, obj_count + dt);
#endif
        assert(obj_count || dt > 0);
        obj_count += dt;
    }

    DLLLOCAL unsigned getScanCount() const {
        return obj_count;
    }

    DLLLOCAL VRMutex* getGate() const {
        return &gate;
    }

    /*
    DLLLOCAL static bool hackId(const QoreObject& obj) {
        if (!obj.priv->data)
            return false;
        const AbstractQoreNode* n = obj.priv->data->getKeyValue("name");
        if (n && n->getType() == NT_STRING && strstr(reinterpret_cast<const QoreStringNode*>(n)->getBuffer(),
            "http-test"))
            return true;
        return false;
    }
    */

    // custom reference handler - unlocked
    /** @param real if the reference is "real" (i.e. cannot be part of a recursive cycle) or not
    */
    DLLLOCAL void customRefIntern(bool real);

    // custom dereference handler - unlocked
    /** @param real if the dereference is "real" (i.e. cannot be part of a recursive cycle) or not
    */
    DLLLOCAL void customDeref(ExceptionSink* xsink, bool real);

    //! Runs the destructor and releases the object's data; see RObject::deleteOrDefer()
    DLLLOCAL virtual void deleteNow(ExceptionSink* xsink) {
        // only a dereference that decided to delete the object defers or makes its deletion, and it has taken the
        // deletion's reference (takeDeleteReference())
        doDeleteIntern(xsink, true);
    }

    DLLLOCAL void fastDeref();

    //! starts a call of the method @a mname of the class @a cls on the object
    /** raises "cannot call <class>::<method>() on an object that has already been deleted" if the object has been
        deleted (issue #5824), naming @a cls, or the object's class if there is none
    */
    DLLLOCAL int startCall(const qore_class_private* cls, const char* mname, ExceptionSink* xsink);

    //! checks a call of the method @a mname of the class @a cls on the current object, which takes no reference
    /** raises the error of startCall() if the object has been deleted (by the arguments of the call: the call itself
        checks it before them); the status changes to OS_DELETED only after the destructor has run, so the destructor's
        calls on the object are not affected (issue #5824)

        @return -1 if the error was raised, 0 if not
    */
    DLLLOCAL int checkCurrentObjectCall(const qore_class_private* cls, const char* mname, ExceptionSink* xsink) {
        if (status == OS_DELETED) {
            raiseDeletedCall(cls, mname, xsink);
            return -1;
        }
        return 0;
    }

    //! raises the error of a call of the method @a mname of the class @a cls on the object, which has been deleted
    // the error path of startCall() and checkCurrentObjectCall(), kept out of the code that every call runs
    DLLLOCAL QORE_INTERN_NOINLINE void raiseDeletedCall(const qore_class_private* cls, const char* mname, ExceptionSink* xsink) const;

    DLLLOCAL void endCall(ExceptionSink* xsink);

    DLLLOCAL int checkClosureSelfValid(ExceptionSink* xsink);

    DLLLOCAL int refClosureSelfForBackground(ExceptionSink* xsink);

    // increments the real reference count without incrementing the actual reference count
    // (i.e. turns the current not real reference into a "real" reference; one that cannot
    // participate in recursive graphs)
    DLLLOCAL void setRealReference();

    // decrements the real reference count without decrementing the actual reference count
    DLLLOCAL void unsetRealReference();

    DLLLOCAL const char* getClassName() const {
        return theclass->getName();
    }

    DLLLOCAL static QoreValue evalBuiltinMethodWithPrivateData(QoreObject& obj, const QoreMethod& method,
            const BuiltinNormalMethodVariantBase* meth, const QoreListNode* args, RuntimeConfig& rc,
                ExceptionSink* xsink) {
        return obj.priv->evalBuiltinMethodWithPrivateData(method, meth, args, rc, xsink);
    }

    DLLLOCAL static qore_object_private* get(QoreObject& obj) {
        return obj.priv;
    }

    DLLLOCAL static const qore_object_private* get(const QoreObject& obj) {
        return obj.priv;
    }

    DLLLOCAL static QoreValue takeMember(QoreObject& obj, ExceptionSink* xsink, const char* mem,
            bool check_access = true, bool for_delete = false) {
        return obj.priv->takeMember(xsink, mem, check_access, for_delete);
    }

    DLLLOCAL static QoreValue takeMember(QoreObject& obj, LValueHelper& lvh, const char* mem, bool for_delete = false) {
        return obj.priv->takeMember(lvh, mem, for_delete);
    }

    DLLLOCAL static void takeMembers(QoreObject& o, QoreLValueGeneric& rv, LValueHelper& lvh, const QoreListNode* l,
            bool for_delete = false) {
        o.priv->takeMembers(rv, lvh, l, for_delete);
    }

    DLLLOCAL static int getLValue(const QoreObject& obj, const char* key, LValueHelper& lvh,
            const qore_class_private* class_ctx, bool for_remove, ExceptionSink* xsink) {
        return obj.priv->getLValue(key, lvh, class_ctx, for_remove, xsink);
    }

    //! Returns true if the object has not been deleted: QoreObject::isValid(), inline for the check of a call's target
    DLLLOCAL QORE_INTERN_ALWAYS_INLINE static bool isValidInline(const QoreObject* obj) {
        return obj->priv->status.load(std::memory_order_relaxed) != OS_DELETED;
    }

    //! Returns the class of the object, inline (it does not change)
    DLLLOCAL static const QoreClass* getClassInline(const QoreObject* obj) {
        return obj->priv->theclass;
    }

    //! Returns true if the object has not been deleted and is of one of the classes given, which a method resolved at
    //! parse time is called on without being looked up by name (AbstractMethodCallNode::exec(), issue #5824)
    //! (always inlined: it is the inline part of every check of a call's target)
    DLLLOCAL QORE_INTERN_ALWAYS_INLINE static bool isValidOfClass(const QoreObject* obj, const QoreClass* qc,
            const QoreClass* method_qc, bool any_class = false) {
        if (any_class) {
            return isValidInline(obj);
        }
        if (!qc) {
            return false;
        }
        const QoreClass* cls = obj->priv->theclass;
        return (cls == qc || cls == method_qc) && isValidInline(obj);
    }

    //! The byte offset of the class pointer in an object's private data, for JIT code (QoreJITObjectLayout)
    DLLLOCAL static uint32_t getClassOffset() {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
        return static_cast<uint32_t>(offsetof(qore_object_private, theclass));
#pragma GCC diagnostic pop
    }

    //! The byte offset of an object's private data pointer, for JIT code (QoreJITObjectLayout)
    DLLLOCAL static uint32_t getPrivOffset() {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
        // QoreObject has single, non-virtual inheritance: offsetof() gives the member's offset
        return static_cast<uint32_t>(offsetof(QoreObject, priv));
#pragma GCC diagnostic pop
    }

    //! The byte offset of the status in an object's private data, for JIT code (QoreJITObjectLayout)
    DLLLOCAL static uint32_t getStatusOffset() {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
        // qore_object_private has single, non-virtual inheritance: offsetof() gives the member's offset
        return static_cast<uint32_t>(offsetof(qore_object_private, status));
#pragma GCC diagnostic pop
    }

    DLLLOCAL static QoreStringNode* firstKey(QoreObject* obj, ExceptionSink* xsink) {
        return obj->priv->firstKey(xsink);
    }

    DLLLOCAL static QoreStringNode* lastKey(QoreObject* obj, ExceptionSink* xsink) {
        return obj->priv->lastKey(xsink);
    }

    DLLLOCAL static unsigned getScanCount(const QoreObject& o) {
        return o.priv->getScanCount();
    }

    DLLLOCAL static void incScanCount(const QoreObject& o, int dt) {
        o.priv->incScanCount(dt);
    }
};

class qore_object_lock_handoff_helper {
private:
    qore_object_private* pobj;
    AutoVLock& vl;

public:
    DLLLOCAL qore_object_lock_handoff_helper(qore_object_private* n_pobj, AutoVLock& n_vl) : pobj(n_pobj), vl(n_vl) {
        if (pobj->obj == qore_auto_vlock_ops(vl).getObject()) {
            assert(qore_auto_vlock_ops(vl).getRWL() == &pobj->rml);
            qore_auto_vlock_ops(vl).clear();
            return;
        }

        // reference current object
        pobj->obj->tRef();

        // unlock previous lock and release from AutoVLock structure
        vl.del();

        // lock current object, unless the assignment of this thread holds its lock while it binds a reference: the lock
        // and the reference to the object stay with that assignment (qore#7380)
        if (!qore_var_rwlock_ops(pobj->rml).wrlockUnlessHeldForBind(&vl)) {
            pobj->obj->tDeref();
            pobj = nullptr;
        }
    }

    DLLLOCAL ~qore_object_lock_handoff_helper() {
        // unlock if lock not saved in AutoVLock structure
        if (pobj) {
            //printd(5, "Object lock %p unlocked (handoff)\n", &pobj->rml);
            pobj->rml.unlock();
            pobj->obj->tDeref();
        }
    }

    DLLLOCAL void stayLocked() {
        if (pobj) {
            qore_auto_vlock_ops(vl).set(pobj->obj, &pobj->rml);
            pobj = nullptr;
        }
    }
};

#endif
