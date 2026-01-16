/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreObjectIntern.h

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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

#define OS_OK            0
#define OS_DELETED      -1

// object access constants
#define QOA_OK           0
#define QOA_PRIV_ERROR   1
#define QOA_PUB_ERROR    2

class LValueHelper;

class lthash {
public:
    DLLLOCAL bool operator()(char* s1, char* s2) const {
        return memcmp(s1, s2, SH_SIZE) < 0;
    }
};

// per-class internal data
typedef std::map<char*, QoreHashNode*, lthash> cdmap_t;

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

// for objects with multiple classes, private data has to be keyed
class KeyList {
private:
    keymap_t keymap;

public:
    DLLLOCAL AbstractPrivateData* getReferencedPrivateData(qore_classid_t key) const {
        keymap_t::const_iterator i = keymap.find(key);
        if (i == keymap.end())
            return 0;

        AbstractPrivateData* apd = i->second.first;
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
        for (keymap_t::const_iterator i = keymap.begin(), e = keymap.end(); i != e; ++i)
            if (!i->second.second)
                i->second.first->deref(xsink);
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
        keymap.erase(i);
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
        i->second.first = 0;
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

class qore_object_private : public RObject {
public:
    const QoreClass* theclass;
    int status = OS_OK;

    KeyList* privateData = nullptr;
    // member data
    QoreHashNode* data;
    QoreProgram* pgm;
    cdmap_t* cdmap = nullptr;

    // used for garbage collection
    mutable unsigned obj_count = 0;

    mutable VRMutex gate;

    int scan_private_data = 0;

    bool system_object, in_destructor;
    bool recursive_ref_found;

    QoreObject* obj;

    DLLLOCAL qore_object_private(QoreObject* n_obj, const QoreClass *oc, QoreProgram* p, QoreHashNode* n_data);

    DLLLOCAL ~qore_object_private();

    DLLLOCAL void incScanPrivateData();
    DLLLOCAL void decScanPrivateData();

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

    DLLLOCAL void merge(qore_object_private& o, AutoVLock& vl, SafeDerefHelper& sdh, ExceptionSink* xsink);

    DLLLOCAL void merge(const QoreHashNode* h, AutoVLock& vl, SafeDerefHelper& sdh, ExceptionSink* xsink);

    DLLLOCAL void mergeIntern(ExceptionSink* xsink, const QoreHashNode* h, bool& check_recursive,
        const qore_class_private* class_ctx, SafeDerefHelper& sdh, const QoreHashNode* new_internal_data = nullptr);

    DLLLOCAL QoreHashNode* copyData(ExceptionSink* xsink, bool throw_exception = true) const;

    DLLLOCAL int getLValue(const char* key, LValueHelper& lvh, const qore_class_private* class_ctx, bool for_remove,
            ExceptionSink* xsink);

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

        if (class_ctx) {
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

        if (class_ctx) {
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
        return ((info->access > Public) && !class_ctx) ? QOA_PRIV_ERROR : QOA_OK;
    }

    // must be called in the object read lock
    DLLLOCAL const QoreHashNode* getInternalData(const qore_class_private* class_ctx) const {
        if (!cdmap)
            return nullptr;
        cdmap_t::const_iterator i = cdmap->find(class_ctx->getHash());
        return i != cdmap->end() ? i->second : nullptr;
    }

    // must be called in the object read lock
    DLLLOCAL QoreHashNode* getInternalData(const qore_class_private* class_ctx) {
        if (!cdmap)
            return nullptr;
        cdmap_t::iterator i = cdmap->find(class_ctx->getHash());
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

    DLLLOCAL int checkMemberAccessGetTypeInfo(ExceptionSink* xsink, const char* mem,
            const qore_class_private* class_ctx, const qore_class_private*& member_class_ctx,
            const QoreTypeInfo*& typeInfo) const {
        assert(!member_class_ctx);
        const qore_class_private* theclass_priv = qore_class_private::get(*theclass);
        const QoreMemberInfo* mi = theclass_priv->runtimeGetMemberInfo(mem, class_ctx);
        if (mi) {
            if (mi->access > Public && !class_ctx) {
                doPrivateException(mem, xsink);
                return -1;
            }

            member_class_ctx = mi->getClassContext(class_ctx);
            typeInfo = mi->getTypeInfo();
            return 0;
        }

        // member is not declared
        if (theclass->hasPublicMembersInHierarchy()) {
            doPublicException(mem, xsink);
            return -1;
        }
        return 0;
    }

    DLLLOCAL QoreValue takeMember(ExceptionSink* xsink, const char* mem, bool check_access = true);

    DLLLOCAL QoreValue takeMember(LValueHelper& lvh, const char* mem);

    DLLLOCAL void takeMembers(QoreLValueGeneric& rv, LValueHelper& lvh, const QoreListNode* l);

    DLLLOCAL QoreValue getReferencedMemberNoMethod(const char* mem, ExceptionSink* xsink) const;

    // lock not held on entry
    DLLLOCAL void doDeleteIntern(ExceptionSink* xsink);

    DLLLOCAL void cleanup(ExceptionSink* xsink, QoreHashNode* td, cdmap_t* cdm) {
        if (privateData) {
            printd(5, "qore_object_private::cleanup() this: %p privateData: %p\n", this, privateData);
            privateData->derefAll(xsink);
            delete privateData;
#ifdef DEBUG
            privateData = nullptr;
#endif
        }

        if (td) {
            td->clear(xsink, true);
            td->deref(xsink);
        }

        if (cdm) {
            for (auto& i : *cdm) {
                i.second->clear(xsink, true);
                i.second->deref(xsink);
            }
            delete cdm;
        }
    }

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
            if (--obj->references)
                return;
        }

        {
            QoreSafeVarRWWriteLocker sl(rml);

            if (in_destructor || status != OS_OK) {
                printd(5, "qore_object_private::obliterate() obj: %p data: %p in_destructor: %d status: %d\n", obj,
                    data, in_destructor, status);
                //printd(5, "Object lock %p unlocked (safe)\n", &rml);
                sl.unlock();
                tDeref();
                return;
            }

            //printd(5, "Object lock %p locked   (safe)\n", &rml);
            printd(5, "qore_object_private::obliterate() obj: %p class: %s\n", obj, theclass->getName());

            status = OS_DELETED;
            cdmap_t* cdm = cdmap;
            cdmap = nullptr;
            QoreHashNode* td = data;
            data = nullptr;

            removeInvalidateRSetIntern();

            //printd(5, "Object lock %p unlocked (safe)\n", &rml);
            sl.unlock();

            cleanup(xsink, td, cdm);
        }
        tDeref();
    }

    DLLLOCAL void doPrivateException(const char* mem, ExceptionSink* xsink) const {
        xsink->raiseException("PRIVATE-MEMBER", "'%s' is a private member of class '%s'", mem, theclass->getName());
    }

    DLLLOCAL void doPublicException(const char* mem, ExceptionSink* xsink) const {
        xsink->raiseException("INVALID-MEMBER", "'%s' is not a registered member of class '%s'", mem,
            theclass->getName());
    }

    DLLLOCAL virtual const char* getName() const {
        return theclass->getName();
    }

    DLLLOCAL virtual void deleteObject() {
        delete obj;
    }

    DLLLOCAL virtual bool isValidImpl() const {
        if (status != OS_OK || in_destructor) {
            printd(QRO_LVL, "qore_object_intern::isValidImpl() this: %p cannot delete graph obj status: %d "
                "in_destructor: %d\n", this, status, in_destructor);
            return false;
        }
        return true;
    }

    DLLLOCAL virtual bool scanMembersIntern(RSetHelper& rsh, QoreHashNode* odata);

    DLLLOCAL virtual bool scanMembers(RSetHelper& rsh);

    // always called in the rsection lock
    DLLLOCAL virtual bool needsScan(bool scan_now) {
        assert(rml.hasRSectionLock());
        printd(5, "qore_object_private::needsScan() scan_count: %d scan_private_data: %d scan_now: %d\n",
            getScanCount(), scan_private_data, scan_now);

        // the status cannot change while this lock is held
        if ((!getScanCount() && !scan_private_data) || status != OS_OK) {
            return false;
        }

        {
            AutoLocker al(rlck);
            if (deferred_scan) {
                if (!rrefs && scan_now) {
                    deferred_scan = false;
                    return true;
                }
                return false;
            }
            if (!rrefs) {
                return true;
            }
            deferred_scan = true;
            // if there is no rset, our job is done
            if (!rset && !scan_private_data) {
                return false;
            }
            // if we have an rset, then we need to invalidate it and ensure that
            // rrefs does not go to zero until this is done
            rref_wait = true;
        }

        removeInvalidateRSetIntern();
        AutoLocker al(rlck);
        rref_wait = false;
        if (rref_waiting) {
            rcond.broadcast();
        }

        return false;
    }

    DLLLOCAL void mergeDataToHash(QoreHashNode* hash, SafeDerefHelper& sdh, ExceptionSink* xsink) const;

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

    DLLLOCAL QoreValue evalBuiltinMethodWithPrivateData(const QoreMethod& method,
            const BuiltinNormalMethodVariantBase* meth, const QoreListNode* args, q_rt_flags_t rtflags,
            ExceptionSink* xsink);

    // no locking necessary; if class_ctx is non-null, an internal member is being initialized
    DLLLOCAL QoreValue& getMemberValueRefForInitialization(const char* member, const qore_class_private* class_ctx);

    //! returns member data of the object (or 0 if there's an exception)
    /** private members are excluded if called outside the class, caller owns the QoreHashNode reference returned

        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return member data of the object
    */
    DLLLOCAL QoreHashNode* getRuntimeMemberHash(ExceptionSink* xsink) const;

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

    DLLLOCAL void fastDeref();

    DLLLOCAL int startCall(const char* mname, ExceptionSink* xsink);

    DLLLOCAL void endCall(ExceptionSink* xsink);

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
            const BuiltinNormalMethodVariantBase* meth, const QoreListNode* args, q_rt_flags_t rtflags,
                ExceptionSink* xsink) {
        return obj.priv->evalBuiltinMethodWithPrivateData(method, meth, args, rtflags, xsink);
    }

    DLLLOCAL static qore_object_private* get(QoreObject& obj) {
        return obj.priv;
    }

    DLLLOCAL static const qore_object_private* get(const QoreObject& obj) {
        return obj.priv;
    }

    DLLLOCAL static QoreValue takeMember(QoreObject& obj, ExceptionSink* xsink, const char* mem,
            bool check_access = true) {
        return obj.priv->takeMember(xsink, mem, check_access);
    }

    DLLLOCAL static QoreValue takeMember(QoreObject& obj, LValueHelper& lvh, const char* mem) {
        return obj.priv->takeMember(lvh, mem);
    }

    DLLLOCAL static void takeMembers(QoreObject& o, QoreLValueGeneric& rv, LValueHelper& lvh, const QoreListNode* l) {
        o.priv->takeMembers(rv, lvh, l);
    }

    DLLLOCAL static int getLValue(const QoreObject& obj, const char* key, LValueHelper& lvh,
            const qore_class_private* class_ctx, bool for_remove, ExceptionSink* xsink) {
        return obj.priv->getLValue(key, lvh, class_ctx, for_remove, xsink);
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
        if (pobj->obj == vl.getObject()) {
            assert(vl.getRWL() == &pobj->rml);
            vl.clear();
            return;
        }

        // reference current object
        pobj->obj->tRef();

        // unlock previous lock and release from AutoVLock structure
        vl.del();

        // lock current object
        pobj->rml.wrlock();
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
        vl.set(pobj->obj, &pobj->rml);
        pobj = nullptr;
    }
};

#endif
