/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreClassIntern.h

    Qore Programming Language

    Copyright (C) 2003 - 2023 Qore Technologies, s.r.o.

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

#ifndef _QORE_QORECLASSINTERN_H

#define _QORE_QORECLASSINTERN_H

#include "qore/safe_dslist"
#include "qore/intern/ConstantList.h"
#include "qore/intern/QoreLValue.h"
#include "qore/intern/qore_var_rwlock_priv.h"
#include "qore/intern/VRMutex.h"
#include "qore/vector_map"
#include "qore/vector_set"

#include <algorithm>
#include <cstring>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#define OTF_USER    CT_USER
#define OTF_BUILTIN CT_BUILTIN

// cannot use vector_map here for performance reasons with method lookups
#ifdef HAVE_QORE_HASH_MAP
#include <qore/hash_map_include.h>
#include "qore/intern/xxhash.h"

typedef HASH_MAP<std::string, QoreMethod*> hm_method_t;
#else
typedef std::map<std::string, QoreMethod*> hm_method_t;
#endif

// forward reference to private class implementation
class qore_class_private;

// map from abstract signature to variant for fast tracking of abstract variants
//typedef vector_map_t<const char*, MethodVariantBase*> vmap_t;
// must be a map to support deletion while iterating
typedef std::map<const char*, MethodVariantBase*, ltstr> vmap_t;

struct AbstractMethod {
    // committed abstract methods from this class and parent classes
    vmap_t vlist;
    // abstract methods to be removed
    vmap_t pending_save;
    // flag if there are new entries in vlist to check
    bool check_parse = false;
    // use relaxed method signature matching
    bool relaxed_match;

    DLLLOCAL AbstractMethod(bool relaxed_match) : relaxed_match(relaxed_match) {
    }

    DLLLOCAL AbstractMethod(const AbstractMethod& old);

    DLLLOCAL ~AbstractMethod();

    // merge changes from parent class method of the same name during parse initialization
    DLLLOCAL void parseMergeBase(AbstractMethod& m, bool committed = false);

    // merge changes from parent class method of the same name during parse initialization
    DLLLOCAL void parseMergeBase(AbstractMethod& m, MethodFunctionBase* f, bool committed = false);

    DLLLOCAL void parseAdd(MethodVariantBase* v);

    DLLLOCAL void parseOverride(MethodVariantBase* v);

    // delete/purge all saved variants in the pending_save list, returns 0 if the AbstractMethod still has abstract
    // variants, -1 if not and therefore can be removed from the map
    DLLLOCAL int parseCommit();

    // noop: parsing can happen once and either succeeds or fails
    DLLLOCAL void parseRollback() {
    }

    DLLLOCAL static void checkAbstract(const char* cname, const char* mname, vmap_t& vlist, QoreStringNode*& desc);

    DLLLOCAL void add(MethodVariantBase* v);
    DLLLOCAL void override(MethodVariantBase* v);

    DLLLOCAL bool empty() const {
        return vlist.empty();
    }
};

// cannot be a vector map to support deletion while iterating with the map API
#ifdef HAVE_QORE_HASH_MAP
typedef HASH_MAP<std::string, AbstractMethod*> amap_t;
#else
typedef std::map<std::string, AbstractMethod*> amap_t;
#endif

struct AbstractMethodMap : amap_t {
    DLLLOCAL AbstractMethodMap(const AbstractMethodMap& old) : relaxed_match(old.relaxed_match) {
        for (auto& i : old) {
            assert(!i.second->vlist.empty());
            insert(amap_t::value_type(i.first, new AbstractMethod(*(i.second))));
        }
    }

    DLLLOCAL AbstractMethodMap() {
    }

    DLLLOCAL ~AbstractMethodMap() {
        for (auto& i : *this) {
            delete i.second;
        }
    }

    DLLLOCAL AbstractMethod* findMethod(const std::string& name) {
        amap_t::iterator i = find(name);
        return i == end() ? nullptr : i->second;
    }

    // adds a pending abstract variant if it doesn't already exist
    DLLLOCAL void parseAddAbstractVariant(const char* name, MethodVariantBase* f);

    DLLLOCAL void parseOverrideAbstractVariant(const char* name, MethodVariantBase* f);

    // adds a committed abstract variant if it doesn't already exist
    DLLLOCAL void addAbstractVariant(const char* name, MethodVariantBase* f);

    // adds a committed non-abstract variant
    DLLLOCAL void overrideAbstractVariant(const char* name, MethodVariantBase* f);

    DLLLOCAL void parseCommit() {
        for (amap_t::iterator i = begin(), e = end(); i != e;) {
            if (i->second->parseCommit()) {
                //printd(5, "AbstractMethodMap::parseCommit() removing abstract ???::%s()\n", i->first.c_str());
                delete i->second;
                erase(i++);
                continue;
            }
            ++i;
        }
    }

    DLLLOCAL void parseRollback() {
        for (auto& i : *this)
            i.second->parseRollback();
    }

    DLLLOCAL void parseInit(qore_class_private& qc, BCList* scl);

    DLLLOCAL QoreStringNode* checkAbstract(const char* cname) const;

    // we check if there are any abstract method variants still in the committed lists
    DLLLOCAL void parseCheckAbstractNew(const QoreProgramLocation* loc, const char* name) const;

    // we check if there are any abstract method variants in the class at runtime (for use with exec-class)
    DLLLOCAL int runtimeCheckInstantiateClass(const char* name, ExceptionSink* xsink) const;

    // use relaxed matching
    bool relaxed_match = false;
};

class SignatureHash;

static inline const char* privpub(ClassAccess access) {
   return access == Public
      ? "public"
      : (access == Private
        ? "private"
        : (access == Internal ? "private:internal" : "inaccessible"));
}

// forward reference for base class (constructor) argument list
class BCAList;
// forward reference for base class list
class BCList;
// forward reference for base class (constructor) evaluated argument list
class BCEAList;

class MethodVariantBase : public AbstractQoreFunctionVariant {
protected:
    const QoreMethod* qmethod = nullptr;    // pointer to method that owns the variant
    ClassAccess access;                     // variant access code
    bool final;                             // is the variant final or not
    bool abstract;                          // is the variant abstract or not
    std::string asig;                       // abstract signature, only set for abstract method variants

public:
    // if %broken-varargs is set, add QCF_USES_EXTRA_ARGS to abstract methods by default as derived methods could use
    // extra arguments
    DLLLOCAL MethodVariantBase(ClassAccess n_access, bool n_final, int64 n_flags, bool n_is_user = false,
            bool n_is_abstract = false) :
            AbstractQoreFunctionVariant(n_flags |
                (n_is_abstract && (getProgram()->getParseOptions64() & PO_BROKEN_VARARGS)
                    ? QCF_USES_EXTRA_ARGS
                    : 0
                ), n_is_user), access(n_access), final(n_final), abstract(n_is_abstract) {
    }

    DLLLOCAL bool isAbstract() const {
        return abstract;
    }

    DLLLOCAL bool isPrivate() const {
        return access > Public;
    }

    DLLLOCAL ClassAccess getAccess() const {
        return access;
    }

    DLLLOCAL bool isFinal() const {
        return final;
    }

    DLLLOCAL void clearAbstract() {
        assert(abstract);
        abstract = false;
    }

    DLLLOCAL void setMethod(QoreMethod* n_qm) {
        qmethod = n_qm;
    }

    DLLLOCAL void setNormalUserMethod(QoreMethod* n_qm, LocalVar* selfid) {
        setMethod(n_qm);
        getUserVariantBase()->setSelfId(selfid);
        assert(selfid);
        assert(selfid->getTypeInfo());
        assert(QoreTypeInfo::getUniqueReturnClass(selfid->getTypeInfo()));
    }

    DLLLOCAL const QoreMethod* method() const {
        assert(qmethod);
        return qmethod;
    }

    DLLLOCAL const QoreClass* getClass() const {
        return qmethod->getClass();
    }

    DLLLOCAL const char* getAbstractSignature();

    DLLLOCAL const qore_class_private* getClassPriv() const;

    DLLLOCAL MethodVariantBase* ref() {
        ROreference();
        return this;
    }

    DLLLOCAL void deref() {
        if (ROdereference()) {
            delete this;
        }
    }
};

#define METHVB(f) (reinterpret_cast<MethodVariantBase*>(f))
#define METHVB_const(f) (reinterpret_cast<const MethodVariantBase*>(f))

class MethodVariant : public MethodVariantBase {
public:
    DLLLOCAL MethodVariant(ClassAccess n_access, bool n_final, int64 n_flags, bool n_is_user = false,
            bool is_abstract = false) : MethodVariantBase(n_access, n_final, n_flags, n_is_user, is_abstract) {
    }
    DLLLOCAL virtual QoreValue evalMethod(QoreObject* self, CodeEvaluationHelper& ceh, ExceptionSink* xsink) const = 0;
    DLLLOCAL virtual QoreValue evalPseudoMethod(const QoreValue n, CodeEvaluationHelper& ceh, ExceptionSink* xsink) const {
        assert(false);
        return QoreValue();
    }
};

#define METHV(f) (reinterpret_cast<MethodVariant*>(f))
#define METHV_const(f) (reinterpret_cast<const MethodVariant*>(f))

class ConstructorMethodVariant : public MethodVariantBase {
protected:
    // evaluates base class constructors and initializes members
    DLLLOCAL int constructorPrelude(const QoreClass &thisclass, CodeEvaluationHelper& ceh, QoreObject* self, BCList* bcl, BCEAList* bceal, ExceptionSink* xsink) const;

public:
    DLLLOCAL ConstructorMethodVariant(ClassAccess n_access, int64 n_flags, bool n_is_user = false) : MethodVariantBase(n_access, false, n_flags, n_is_user) {
    }
    DLLLOCAL virtual const BCAList* getBaseClassArgumentList() const = 0;
    DLLLOCAL virtual void evalConstructor(const QoreClass &thisclass, QoreObject* self, CodeEvaluationHelper& ceh, BCList* bcl, BCEAList* bceal, ExceptionSink* xsink) const = 0;
};

#define CONMV(f) (reinterpret_cast<ConstructorMethodVariant*>(f))
#define CONMV_const(f) (reinterpret_cast<const ConstructorMethodVariant*>(f))

class DestructorMethodVariant : public MethodVariantBase {
protected:
public:
    DLLLOCAL DestructorMethodVariant(bool n_is_user = false) : MethodVariantBase(Public, false, QCF_NO_FLAGS, n_is_user) {
    }

    DLLLOCAL virtual void evalDestructor(const QoreClass &thisclass, QoreObject* self, ExceptionSink* xsink) const = 0;
};

#define DESMV(f) (reinterpret_cast<DestructorMethodVariant*>(f))
#define DESMV_const(f) (reinterpret_cast<const DestructorMethodVariant*>(f))

class CopyMethodVariant : public MethodVariantBase {
protected:
public:
    DLLLOCAL CopyMethodVariant(ClassAccess n_access, bool n_is_user = false)
            : MethodVariantBase(n_access, false, QCF_NO_FLAGS, n_is_user) {
    }

    DLLLOCAL virtual void evalCopy(const QoreClass &thisclass, QoreObject* self, QoreObject* old,
            CodeEvaluationHelper& ceh, BCList* scl, ExceptionSink* xsink) const = 0;
};

#define COPYMV(f) (reinterpret_cast<CopyMethodVariant*>(f))
#define COPYMV_const(f) (reinterpret_cast<const CopyMethodVariant*>(f))

class UserMethodVariant : public MethodVariant, public UserVariantBase {
protected:
    bool synchronized;

public:
    DLLLOCAL UserMethodVariant(ClassAccess n_access, bool n_final, StatementBlock* b, int n_sig_first_line,
            int n_sig_last_line, QoreValue params, RetTypeInfo* rv, bool synced, int64 n_flags, bool is_abstract)
            : MethodVariant(n_access, n_final, n_flags, true, is_abstract),
            UserVariantBase(b, n_sig_first_line, n_sig_last_line, params, rv, false), synchronized(synced) {
         if (signature.hasVarargs()) {
            flags |= QCF_USES_EXTRA_ARGS;
        }
    }

    DLLLOCAL ~UserMethodVariant() {
    }

    // the following defines the pure virtual functions that are common to all user variants
    COMMON_USER_VARIANT_FUNCTIONS

    DLLLOCAL virtual int parseInit(QoreFunction* f) {
        MethodFunctionBase* mf = static_cast<MethodFunctionBase*>(f);

        signature.resolve();
        // parseResolve and push current return type on stack
        ParseCodeInfoHelper rtih(mf->getName(), signature.getReturnTypeInfo());

        int err = 0;

        // must be called even if "statements" is NULL
        if (!mf->isStatic()) {
            if (!isAbstract()) {
                err = statements->parseInitMethod(mf->MethodFunctionBase::getClass()->getTypeInfo(), this);
            }
        } else {
            err = statements->parseInit(this);
        }

        // recheck types against committed types if necessary
        if (recheck) {
            f->parseCheckDuplicateSignatureCommitted(&signature);
        }

        return err;
    }

    DLLLOCAL virtual QoreValue evalMethod(QoreObject* self, CodeEvaluationHelper& ceh, ExceptionSink* xsink) const;
};

#define UMV(f) (reinterpret_cast<UserMethodVariant*>(f))
#define UMV_const(f) (reinterpret_cast<const UserMethodVariant*>(f))

class UserConstructorVariant : public ConstructorMethodVariant, public UserVariantBase {
protected:
    // base class argument list for constructors
    BCAList* bcal;

    DLLLOCAL virtual ~UserConstructorVariant();

public:
    DLLLOCAL UserConstructorVariant(ClassAccess n_access, StatementBlock* b, int n_sig_first_line, int n_sig_last_line,
            QoreValue params, BCAList* n_bcal, int64 n_flags = QCF_NO_FLAGS)
            : ConstructorMethodVariant(n_access, n_flags, true), UserVariantBase(b, n_sig_first_line, n_sig_last_line,
                params, nullptr, false), bcal(n_bcal) {
        if (signature.hasVarargs()) {
            flags |= QCF_USES_EXTRA_ARGS;
        }
    }

    // the following defines the pure virtual functions that are common to all user variants
    COMMON_USER_VARIANT_FUNCTIONS

    DLLLOCAL virtual const BCAList* getBaseClassArgumentList() const {
        return bcal;
    }

    DLLLOCAL virtual void evalConstructor(const QoreClass& thisclass, QoreObject* self, CodeEvaluationHelper& ceh,
        BCList* bcl, BCEAList* bceal, ExceptionSink* xsink) const;

    DLLLOCAL virtual int parseInit(QoreFunction* f);
};

#define UCONV(f) (reinterpret_cast<UserConstructorVariant*>(f))
#define UCONV_const(f) (reinterpret_cast<const UserConstructorVariant*>(f))

class UserDestructorVariant : public DestructorMethodVariant, public UserVariantBase {
protected:
public:
    DLLLOCAL UserDestructorVariant(StatementBlock* b, int n_sig_first_line, int n_sig_last_line)
            : DestructorMethodVariant(true), UserVariantBase(b, n_sig_first_line, n_sig_last_line, QoreValue(),
                nullptr, false) {
    }

    // the following defines the pure virtual functions that are common to all user variants
    COMMON_USER_VARIANT_FUNCTIONS

    DLLLOCAL virtual int parseInit(QoreFunction* f) {
        MethodFunctionBase* mf = static_cast<MethodFunctionBase*>(f);

        int err = signature.resolve();
        assert(!signature.getReturnTypeInfo() || signature.getReturnTypeInfo() == nothingTypeInfo);

        // push return type on stack (no return value can be used)
        ParseCodeInfoHelper rtih("destructor", nothingTypeInfo);

        // must be called even if statements is NULL
        if (statements->parseInitMethod(mf->MethodFunctionBase::getClass()->getTypeInfo(), this) && !err) {
            err = -1;
        }

        // only 1 variant is possible, no need to recheck types
        return err;
    }

    DLLLOCAL virtual void evalDestructor(const QoreClass &thisclass, QoreObject* self, ExceptionSink* xsink) const {
        // there cannot be any params
        assert(!signature.numParams());
        assert(!signature.getReturnTypeInfo() || signature.getReturnTypeInfo() == nothingTypeInfo);
        eval("destructor", 0, self, xsink, getClassPriv()).discard(xsink);
    }
};

#define UDESV(f) (reinterpret_cast<UserDestructorVariant*>(f))
#define UDESV_const(f) (reinterpret_cast<const UserDestructorVariant*>(f))

class UserCopyVariant : public CopyMethodVariant, public UserVariantBase {
protected:
public:
   DLLLOCAL UserCopyVariant(ClassAccess n_access, StatementBlock* b, int n_sig_first_line, int n_sig_last_line, QoreValue params, RetTypeInfo* rv, bool synced) : CopyMethodVariant(n_access, true), UserVariantBase(b, n_sig_first_line, n_sig_last_line, params, rv, synced) {
   }

   // the following defines the pure virtual functions that are common to all user variants
   COMMON_USER_VARIANT_FUNCTIONS

   DLLLOCAL virtual int parseInit(QoreFunction* f);

   DLLLOCAL virtual void evalCopy(const QoreClass &thisclass, QoreObject* self, QoreObject* old, CodeEvaluationHelper& ceh, BCList* scl, ExceptionSink* xsink) const;
};

#define UCOPYV(f) (reinterpret_cast<UserCopyVariant*>(f))

class BuiltinMethodVariant : public MethodVariant, public BuiltinFunctionVariantBase {
public:
   DLLLOCAL BuiltinMethodVariant(ClassAccess n_access, bool n_final, int64 n_flags, int64 n_functionality,
        const QoreTypeInfo* n_returnTypeInfo, const type_vec_t &n_typeList = type_vec_t(),
        const arg_vec_t &n_defaultArgList = arg_vec_t(), const name_vec_t& n_names = name_vec_t())
        : MethodVariant(n_access, n_final, n_flags),
            BuiltinFunctionVariantBase(n_flags, n_functionality, n_returnTypeInfo, n_typeList, n_defaultArgList, n_names) {}

   // the following defines the pure virtual functions that are common to all builtin variants
   COMMON_BUILTIN_VARIANT_FUNCTIONS
};

class BuiltinAbstractMethodVariant : public BuiltinMethodVariant {
public:
   DLLLOCAL BuiltinAbstractMethodVariant(ClassAccess n_access, int64 n_flags, const QoreTypeInfo* n_returnTypeInfo, const type_vec_t &n_typeList = type_vec_t(), const arg_vec_t &n_defaultArgList = arg_vec_t(), const name_vec_t& n_names = name_vec_t()) : BuiltinMethodVariant(n_access, false, n_flags, QDOM_DEFAULT, n_returnTypeInfo, n_typeList, n_defaultArgList, n_names) {
      abstract = true;
   }

   DLLLOCAL virtual QoreValue evalMethod(QoreObject* self, CodeEvaluationHelper& ceh, ExceptionSink* xsink) const {
      assert(false);
      return QoreValue();
   }
};

class BuiltinNormalMethodVariantBase : public BuiltinMethodVariant {
public:
    DLLLOCAL BuiltinNormalMethodVariantBase(ClassAccess n_access, bool n_final, int64 n_flags, int64 n_functionality, const QoreTypeInfo* n_returnTypeInfo, const type_vec_t &n_typeList = type_vec_t(), const arg_vec_t &n_defaultArgList = arg_vec_t(), const name_vec_t& n_names = name_vec_t()) : BuiltinMethodVariant(n_access, n_final, n_flags, n_functionality, n_returnTypeInfo, n_typeList, n_defaultArgList, n_names) {}

    DLLLOCAL virtual QoreValue evalMethod(QoreObject* self, CodeEvaluationHelper& ceh, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreValue evalPseudoMethod(const QoreValue n, CodeEvaluationHelper& ceh, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreValue evalImpl(QoreObject* self, AbstractPrivateData* private_data, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) const = 0;
};

class BuiltinNormalMethodValueVariant : public BuiltinNormalMethodVariantBase {
protected:
    q_method_n_t method;

public:
    DLLLOCAL BuiltinNormalMethodValueVariant(q_method_n_t m, ClassAccess n_access, bool n_final = false, int64 n_flags = QCF_USES_EXTRA_ARGS, int64 n_functionality = QDOM_DEFAULT, const QoreTypeInfo* n_returnTypeInfo = 0, const type_vec_t &n_typeList = type_vec_t(), const arg_vec_t &n_defaultArgList = arg_vec_t(), const name_vec_t& n_names = name_vec_t()) : BuiltinNormalMethodVariantBase(n_access, n_final, n_flags, n_functionality, n_returnTypeInfo, n_typeList, n_defaultArgList, n_names), method(m) {
    }
    DLLLOCAL virtual QoreValue evalImpl(QoreObject* self, AbstractPrivateData* private_data, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) const {
        return method(self, private_data, args, rtflags, xsink);
    }
};

class BuiltinExternalNormalMethodValueVariant : public BuiltinNormalMethodVariantBase {
protected:
    q_external_method_t method;
    const void* ptr;

public:
    DLLLOCAL BuiltinExternalNormalMethodValueVariant(const void* n_ptr, q_external_method_t m, ClassAccess n_access, bool n_final = false, int64 n_flags = QCF_USES_EXTRA_ARGS, int64 n_functionality = QDOM_DEFAULT, const QoreTypeInfo* n_returnTypeInfo = 0, const type_vec_t &n_typeList = type_vec_t(), const arg_vec_t &n_defaultArgList = arg_vec_t(), const name_vec_t& n_names = name_vec_t()) : BuiltinNormalMethodVariantBase(n_access, n_final, n_flags, n_functionality, n_returnTypeInfo, n_typeList, n_defaultArgList, n_names), method(m), ptr(n_ptr) {
    }
    DLLLOCAL virtual QoreValue evalImpl(QoreObject* self, AbstractPrivateData* private_data, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) const {
        return method(*qmethod, ptr, self, private_data, args, rtflags, xsink);
    }
};

class BuiltinStaticMethodValueVariant : public BuiltinMethodVariant {
protected:
    q_func_n_t static_method;

public:
    DLLLOCAL BuiltinStaticMethodValueVariant(q_func_n_t m, ClassAccess n_access, bool n_final = false, int64 n_flags = QCF_USES_EXTRA_ARGS, int64 n_functionality = QDOM_DEFAULT, const QoreTypeInfo* n_returnTypeInfo = 0, const type_vec_t &n_typeList = type_vec_t(), const arg_vec_t &n_defaultArgList = arg_vec_t(), const name_vec_t& n_names = name_vec_t()) : BuiltinMethodVariant(n_access, n_final, n_flags, n_functionality, n_returnTypeInfo, n_typeList, n_defaultArgList, n_names), static_method(m) {
    }

    DLLLOCAL virtual QoreValue evalMethod(QoreObject* self, CodeEvaluationHelper& ceh, ExceptionSink* xsink) const {
        CodeContextHelper cch(xsink, CT_BUILTIN, qmethod->getName(), 0, getClassPriv());

        return static_method(ceh.getArgs(), ceh.getRuntimeFlags(), xsink);
    }
};

class BuiltinExternalStaticMethodValueVariant : public BuiltinMethodVariant {
protected:
    q_external_static_method_t static_method;
    const void* ptr;

public:
    DLLLOCAL BuiltinExternalStaticMethodValueVariant(const void* n_ptr, q_external_static_method_t m, ClassAccess n_access, bool n_final = false, int64 n_flags = QCF_USES_EXTRA_ARGS, int64 n_functionality = QDOM_DEFAULT, const QoreTypeInfo* n_returnTypeInfo = 0, const type_vec_t &n_typeList = type_vec_t(), const arg_vec_t &n_defaultArgList = arg_vec_t(), const name_vec_t& n_names = name_vec_t()) : BuiltinMethodVariant(n_access, n_final, n_flags, n_functionality, n_returnTypeInfo, n_typeList, n_defaultArgList, n_names), static_method(m), ptr(n_ptr) {
    }

    DLLLOCAL virtual QoreValue evalMethod(QoreObject* self, CodeEvaluationHelper& ceh, ExceptionSink* xsink) const {
        CodeContextHelper cch(xsink, CT_BUILTIN, qmethod->getName(), 0, getClassPriv());

        return static_method(*qmethod, ptr, ceh.getArgs(), ceh.getRuntimeFlags(), xsink);
    }
};

class BuiltinConstructorVariantBase : public ConstructorMethodVariant, public BuiltinFunctionVariantBase {
public:
    // return type info is set to 0 because the new operator actually returns the new object, not the constructor
    DLLLOCAL BuiltinConstructorVariantBase(ClassAccess n_access, int64 n_flags = QCF_USES_EXTRA_ARGS,
            int64 n_functionality = QDOM_DEFAULT, const type_vec_t &n_typeList = type_vec_t(),
            const arg_vec_t &n_defaultArgList = arg_vec_t(), const name_vec_t& n_names = name_vec_t())
            : ConstructorMethodVariant(n_access, n_flags),
            BuiltinFunctionVariantBase(n_flags, n_functionality, 0, n_typeList, n_defaultArgList, n_names) {
    }

    // the following defines the pure virtual functions that are common to all builtin variants
    COMMON_BUILTIN_VARIANT_FUNCTIONS

    DLLLOCAL virtual const BCAList* getBaseClassArgumentList() const {
        return nullptr;
    }
};

class BuiltinConstructorValueVariant : public BuiltinConstructorVariantBase {
protected:
    q_constructor_n_t constructor;

public:
    DLLLOCAL BuiltinConstructorValueVariant(q_constructor_n_t m, ClassAccess n_access, int64 n_flags = QCF_USES_EXTRA_ARGS, int64 n_functionality = QDOM_DEFAULT, const type_vec_t &n_typeList = type_vec_t(), const arg_vec_t &n_defaultArgList = arg_vec_t(), const name_vec_t& n_names = name_vec_t()) : BuiltinConstructorVariantBase(n_access, n_flags, n_functionality, n_typeList, n_defaultArgList, n_names), constructor(m) {
    }

    DLLLOCAL virtual void evalConstructor(const QoreClass& thisclass, QoreObject* self, CodeEvaluationHelper& ceh, BCList* bcl, BCEAList* bceal, ExceptionSink* xsink) const;
};

class BuiltinExternalConstructorValueVariant : public BuiltinConstructorVariantBase {
protected:
    q_external_constructor_t constructor;
    const void* ptr;

public:
    DLLLOCAL BuiltinExternalConstructorValueVariant(const void* n_ptr, q_external_constructor_t m, ClassAccess n_access, int64 n_flags = QCF_USES_EXTRA_ARGS, int64 n_functionality = QDOM_DEFAULT, const type_vec_t &n_typeList = type_vec_t(), const arg_vec_t &n_defaultArgList = arg_vec_t(), const name_vec_t& n_names = name_vec_t()) : BuiltinConstructorVariantBase(n_access, n_flags, n_functionality, n_typeList, n_defaultArgList, n_names), constructor(m), ptr(n_ptr) {
    }

    DLLLOCAL virtual void evalConstructor(const QoreClass& thisclass, QoreObject* self, CodeEvaluationHelper& ceh, BCList* bcl, BCEAList* bceal, ExceptionSink* xsink) const;
};

class BuiltinDestructorVariantBase : public DestructorMethodVariant, public BuiltinFunctionVariantBase {
public:
    // the following defines the pure virtual functions that are common to all builtin variants
    COMMON_BUILTIN_VARIANT_FUNCTIONS
};

class BuiltinDestructorVariant : public BuiltinDestructorVariantBase {
protected:
    q_destructor_t destructor;

public:
    DLLLOCAL BuiltinDestructorVariant(q_destructor_t n_destructor) : destructor(n_destructor) {
    }

    DLLLOCAL virtual void evalDestructor(const QoreClass &thisclass, QoreObject* self, ExceptionSink* xsink) const;
};

class BuiltinExternalDestructorVariant : public BuiltinDestructorVariantBase {
protected:
    q_external_destructor_t destructor;
    const void* ptr;

public:
    DLLLOCAL BuiltinExternalDestructorVariant(const void* ptr, q_external_destructor_t destructor) : destructor(destructor), ptr(ptr) {
    }

    DLLLOCAL virtual void evalDestructor(const QoreClass &thisclass, QoreObject* self, ExceptionSink* xsink) const;
};

class BuiltinCopyVariantBase : public CopyMethodVariant, public BuiltinFunctionVariantBase {
protected:
public:
    DLLLOCAL BuiltinCopyVariantBase(const QoreClass* c)
        : CopyMethodVariant(Public),
        BuiltinFunctionVariantBase(0, QDOM_DEFAULT, c->getTypeInfo()) {
    }

    // the following defines the pure virtual functions that are common to all builtin variants
    COMMON_BUILTIN_VARIANT_FUNCTIONS

    DLLLOCAL virtual void evalCopy(const QoreClass &thisclass, QoreObject* self, QoreObject* old, CodeEvaluationHelper& ceh, BCList* scl, ExceptionSink* xsink) const;
    DLLLOCAL virtual void evalImpl(const QoreClass &thisclass, QoreObject* self, QoreObject* old, AbstractPrivateData* private_data, ExceptionSink* xsink) const = 0;
};

class BuiltinCopyVariant : public BuiltinCopyVariantBase {
protected:
    q_copy_t copy;

public:
    DLLLOCAL BuiltinCopyVariant(QoreClass* c, q_copy_t m) : BuiltinCopyVariantBase(c), copy(m) {
    }
    DLLLOCAL virtual void evalImpl(const QoreClass &thisclass, QoreObject* self, QoreObject* old,
            AbstractPrivateData* private_data, ExceptionSink* xsink) const {
        copy(self, old, private_data, xsink);
    }
};

class BuiltinExternalCopyVariant : public BuiltinCopyVariantBase {
protected:
    q_external_copy_t copy;
    const void* ptr;

public:
    DLLLOCAL BuiltinExternalCopyVariant(const void* n_ptr, QoreClass* c, q_external_copy_t m)
            : BuiltinCopyVariantBase(c), copy(m), ptr(n_ptr) {
    }

    DLLLOCAL virtual void evalImpl(const QoreClass &thisclass, QoreObject* self, QoreObject* old,
            AbstractPrivateData* private_data, ExceptionSink* xsink) const {
        copy(thisclass, ptr, self, old, private_data, xsink);
    }
};

// abstract class for method functions (static and non-static)
class NormalMethodFunction : public MethodFunctionBase {
public:
    DLLLOCAL NormalMethodFunction(const char* nme, const QoreClass* n_qc) : MethodFunctionBase(nme, n_qc, false) {
    }

    DLLLOCAL NormalMethodFunction(const NormalMethodFunction &old, const QoreClass* n_qc)
            : MethodFunctionBase(old, n_qc) {
    }

    DLLLOCAL virtual ~NormalMethodFunction() {
    }

    // if the variant was identified at parse time, then variant will not be NULL, otherwise if NULL then it is identified at run time
    DLLLOCAL QoreValue evalMethod(ExceptionSink* xsink, const AbstractQoreFunctionVariant* variant, QoreObject* self,
            const QoreListNode* args, const qore_class_private* cctx = nullptr, QoreProgram* pgm_ctx = nullptr) const;

    // if the variant was identified at parse time, then variant will not be NULL, otherwise if NULL then it is identified at run time
    DLLLOCAL QoreValue evalMethodTmpArgs(ExceptionSink* xsink, const AbstractQoreFunctionVariant* variant,
            QoreObject* self, QoreListNode* args, const qore_class_private* cctx = nullptr) const;

    // if the variant was identified at parse time, then variant will not be NULL, otherwise if NULL then it is identified at run time
    DLLLOCAL QoreValue evalPseudoMethod(ExceptionSink* xsink, const AbstractQoreFunctionVariant* variant,
        const QoreValue n, const QoreListNode* args, const qore_class_private* cctx = runtime_get_class()) const;
};

#define NMETHF(f) (reinterpret_cast<NormalMethodFunction*>(f))

// abstract class for method functions (static and non-static)
class StaticMethodFunction : public MethodFunctionBase {
public:
    DLLLOCAL StaticMethodFunction(const char* nme, const QoreClass* n_qc) : MethodFunctionBase(nme, n_qc, true) {
    }
    DLLLOCAL StaticMethodFunction(const StaticMethodFunction &old, const QoreClass* n_qc)
            : MethodFunctionBase(old, n_qc) {
    }
    DLLLOCAL virtual ~StaticMethodFunction() {
    }

    // if the variant was identified at parse time, then variant will not be NULL, otherwise if NULL then it is identified at run time
    DLLLOCAL QoreValue evalMethod(ExceptionSink* xsink, const AbstractQoreFunctionVariant* variant,
            const QoreListNode* args, const qore_class_private* cctx = nullptr, QoreProgram* pgm_ctx = nullptr) const;

    // if the variant was identified at parse time, then variant will not be NULL, otherwise if NULL then it is identified at run time
    DLLLOCAL QoreValue evalMethodTmpArgs(ExceptionSink* xsink, const AbstractQoreFunctionVariant* variant,
            QoreListNode* args, const qore_class_private* cctx = nullptr) const;
};

#define SMETHF(f) (reinterpret_cast<StaticMethodFunction*>(f))

// abstract class for constructor method functions
class ConstructorMethodFunction : public MethodFunctionBase {
public:
    DLLLOCAL ConstructorMethodFunction(const QoreClass* n_qc) : MethodFunctionBase("constructor", n_qc, false) {
    }
    DLLLOCAL ConstructorMethodFunction(const ConstructorMethodFunction &old, const QoreClass* n_qc)
            : MethodFunctionBase(old, n_qc) {
    }

    // if the variant was identified at parse time, then variant will not be NULL, otherwise if NULL then it is identified at run time
    DLLLOCAL void evalConstructor(const AbstractQoreFunctionVariant* variant, const QoreClass &thisclass,
            QoreObject* self, const QoreListNode* args, BCList* bcl, BCEAList* bceal, ExceptionSink* xsink) const;

    DLLLOCAL virtual MethodFunctionBase* copy(const QoreClass* n_qc) const {
        return new ConstructorMethodFunction(*this, n_qc);
    }
};

#define CONMF(f) (reinterpret_cast<ConstructorMethodFunction*>(f))

// abstract class for destructor method functions
class DestructorMethodFunction : public MethodFunctionBase {
public:
    DLLLOCAL DestructorMethodFunction(const QoreClass* n_qc) : MethodFunctionBase("destructor", n_qc, false) {
    }
    DLLLOCAL DestructorMethodFunction(const DestructorMethodFunction &old, const QoreClass* n_qc)
            : MethodFunctionBase(old, n_qc) {
    }
    DLLLOCAL void evalDestructor(const QoreClass &thisclass, QoreObject* self, ExceptionSink* xsink) const;

    DLLLOCAL virtual MethodFunctionBase* copy(const QoreClass* n_qc) const {
        return new DestructorMethodFunction(*this, n_qc);
    }
};

#define DESMF(f) (reinterpret_cast<DestructorMethodFunction*>(f))

// abstract class for copy method functions
class CopyMethodFunction : public MethodFunctionBase {
public:
    DLLLOCAL CopyMethodFunction(const QoreClass* n_qc) : MethodFunctionBase("copy", n_qc, false) {
    }
    DLLLOCAL CopyMethodFunction(const CopyMethodFunction &old, const QoreClass* n_qc)
            : MethodFunctionBase(old, n_qc) {
    }
    DLLLOCAL void evalCopy(const QoreClass &thisclass, QoreObject* self, QoreObject* old, BCList* scl,
            ExceptionSink* xsink) const;

    DLLLOCAL virtual MethodFunctionBase* copy(const QoreClass* n_qc) const {
        return new CopyMethodFunction(*this, n_qc);
    }
};

#define COPYMF(f) (reinterpret_cast<CopyMethodFunction*>(f))

class BuiltinSystemConstructorBase : public MethodFunctionBase {
public:
    DLLLOCAL BuiltinSystemConstructorBase(const QoreClass* n_qc) : MethodFunctionBase("constructor", n_qc, false) {
    }
    DLLLOCAL BuiltinSystemConstructorBase(const BuiltinSystemConstructorBase &old, const QoreClass* n_qc) : MethodFunctionBase(old, n_qc) {
    }
    DLLLOCAL virtual void eval(const QoreClass &thisclass, QoreObject* self, int code, va_list args) const = 0;

    DLLLOCAL virtual MethodFunctionBase* copy(const QoreClass* n_qc) const = 0;
};

#define BSYSCONB(f) (reinterpret_cast<BuiltinSystemConstructorBase* >(f))

// system constructors are not accessed from userspace so we don't need to conform
// to the abstract class structure
class BuiltinSystemConstructor : public BuiltinSystemConstructorBase {
protected:
    q_system_constructor_t system_constructor;

public:
    DLLLOCAL BuiltinSystemConstructor(const QoreClass* n_qc, q_system_constructor_t m) : BuiltinSystemConstructorBase(n_qc), system_constructor(m) {
    }

    DLLLOCAL BuiltinSystemConstructor(const BuiltinSystemConstructor &old, const QoreClass* n_qc) : BuiltinSystemConstructorBase(old, n_qc), system_constructor(old.system_constructor) {
    }

    DLLLOCAL virtual void eval(const QoreClass &thisclass, QoreObject* self, int code, va_list args) const {
        system_constructor(self, code, args);
    }

    DLLLOCAL virtual MethodFunctionBase* copy(const QoreClass* n_qc) const {
        return new BuiltinSystemConstructor(*this, n_qc);
    }
};

class BuiltinNormalMethod : public NormalMethodFunction {
public:
    DLLLOCAL BuiltinNormalMethod(const QoreClass* n_qc, const char* mname) : NormalMethodFunction(mname, n_qc) {
    }

    DLLLOCAL BuiltinNormalMethod(const BuiltinNormalMethod &old, const QoreClass* n_qc) : NormalMethodFunction(old, n_qc) {
    }

    DLLLOCAL virtual MethodFunctionBase* copy(const QoreClass* n_qc) const {
        return new BuiltinNormalMethod(*this, n_qc);
    }
};

class BuiltinStaticMethod : public StaticMethodFunction {
public:
    DLLLOCAL BuiltinStaticMethod(const QoreClass* n_qc, const char* mname) : StaticMethodFunction(mname, n_qc) {
    }

    DLLLOCAL BuiltinStaticMethod(const BuiltinStaticMethod &old, const QoreClass* n_qc) : StaticMethodFunction(old, n_qc) {
    }

    DLLLOCAL virtual MethodFunctionBase* copy(const QoreClass* n_qc) const {
        return new BuiltinStaticMethod(*this, n_qc);
    }
};

class NormalUserMethod : public NormalMethodFunction {
public:
    DLLLOCAL NormalUserMethod(const QoreClass* n_qc, const char* mname) : NormalMethodFunction(mname, n_qc) {
    }
    DLLLOCAL NormalUserMethod(const NormalUserMethod &old, const QoreClass* n_qc) : NormalMethodFunction(old, n_qc) {
    }
    DLLLOCAL virtual MethodFunctionBase* copy(const QoreClass* n_qc) const {
        return new NormalUserMethod(*this, n_qc);
    }
};

class StaticUserMethod : public StaticMethodFunction {
public:
    DLLLOCAL StaticUserMethod(const QoreClass* n_qc, const char* mname) : StaticMethodFunction(mname, n_qc) {
    }
    DLLLOCAL StaticUserMethod(const StaticUserMethod &old, const QoreClass* n_qc) : StaticMethodFunction(old, n_qc) {
    }
    DLLLOCAL virtual MethodFunctionBase* copy(const QoreClass* n_qc) const {
        return new StaticUserMethod(*this, n_qc);
    }
};

class QoreMemberInfoBase {
protected:
    const QoreTypeInfo* typeInfo;

    DLLLOCAL QoreMemberInfoBase(const QoreMemberInfoBase& old) : typeInfo(old.typeInfo), exp(old.exp.refSelf()),
            loc(old.loc), parseTypeInfo(old.parseTypeInfo ? new QoreParseTypeInfo(*old.parseTypeInfo) : nullptr) {
    }

public:
    // initialization expression
    QoreValue exp;

    // store parse location in case of errors
    const QoreProgramLocation* loc;
    QoreParseTypeInfo* parseTypeInfo;

    DLLLOCAL QoreMemberInfoBase(const QoreProgramLocation* loc, const QoreTypeInfo* n_typeinfo = nullptr,
            QoreParseTypeInfo* n_parseTypeInfo = nullptr, QoreValue e = QoreValue())
            : typeInfo(n_typeinfo), exp(e), loc(loc), parseTypeInfo(n_parseTypeInfo) {
    }

    DLLLOCAL ~QoreMemberInfoBase() {
        del();
    }

    DLLLOCAL void del() {
        exp.discard(nullptr);
        if (parseTypeInfo) {
            delete parseTypeInfo;
            parseTypeInfo = nullptr;
        }
    }

    DLLLOCAL const QoreTypeInfo* getTypeInfo() const {
        return typeInfo;
    }

    DLLLOCAL const QoreTypeInfo* parseGetTypeInfo() const {
        // we cannot tell if the member has been initialized, so we return anyTypeInfo here for potential references
        return QoreTypeInfo::isReference(typeInfo) ? anyTypeInfo : typeInfo;
        //return typeInfo;
    }

    DLLLOCAL bool parseHasTypeInfo() const {
        return (typeInfo || parseTypeInfo);
    }
};

class QoreMemberInfoBaseAccess : public QoreMemberInfoBase {
public:
    ClassAccess access;

    DLLLOCAL QoreMemberInfoBaseAccess(const QoreProgramLocation* loc, const QoreTypeInfo* n_typeinfo = nullptr,
            QoreParseTypeInfo* n_parseTypeInfo = nullptr, QoreValue e = QoreValue(), ClassAccess n_access = Public) :
        QoreMemberInfoBase(loc, n_typeinfo, n_parseTypeInfo, e), access(n_access) {
    }

    DLLLOCAL ClassAccess getAccess() const {
        return access;
    }

protected:
    DLLLOCAL QoreMemberInfoBaseAccess(const QoreMemberInfoBaseAccess& old, ClassAccess n_access)
            : QoreMemberInfoBase(old), access(old.access >= n_access ? old.access : n_access), init(old.init) {
    }

    bool init = false;
};

// for the inheritance list for each member
typedef std::vector<const qore_class_private*> cls_vec_t;

// maps from an accessing context to the object storage context
typedef std::map<const qore_class_private*, const qore_class_private*> cls_context_map_t;

// list of inherited member info dsta structures for initialization
typedef std::deque<const QoreMemberInfo*> member_info_list_t;

// the access stored here is a composite access for the member; the maximum of the class inheritance access
// (for accessible members imported from base classes) and the member's access in the class where it's
// defined
class QoreMemberInfo : public QoreMemberInfoBaseAccess {
    friend class qore_class_private;
public:
    DLLLOCAL QoreMemberInfo(const QoreProgramLocation* loc, const QoreTypeInfo* n_typeInfo = nullptr,
            QoreParseTypeInfo* n_parseTypeInfo = nullptr, QoreValue e = QoreValue(), ClassAccess n_access = Public,
            const qore_class_private* qc = nullptr)
            : QoreMemberInfoBaseAccess(loc, n_typeInfo, n_parseTypeInfo, e, n_access), is_local(true) {
        if (qc) {
            cls_vec.push_back(qc);
        }
    }

    // copy to a new class - same member, new class
    DLLLOCAL QoreMemberInfo(const QoreMemberInfo& old, const qore_class_private* cls);

    DLLLOCAL ~QoreMemberInfo() {
        delete cls_context_map;
        delete member_info_list;
    }

    DLLLOCAL void setDeclaringClass(const qore_class_private* qc) {
        assert(cls_vec.empty());
        cls_vec.push_back(qc);
        assert(is_local);
    }

    // returns true if the member is a locally-defined member with private:internal member access
    DLLLOCAL bool isLocalInternal() const {
        return is_local && access == Internal;
    }

    // returns true if the member is locally defined
    DLLLOCAL bool local() const {
        return is_local;
    }

    // returns the class where the member was declared
    DLLLOCAL const qore_class_private* getClass() const {
        return cls_vec[0];
    }

    // issue #2970: returns the class ptr to be used to access the member data in the object; nullptr means to access in the standard object hash
    DLLLOCAL const qore_class_private* getClassContext(const qore_class_private* class_ctx) const {
        if (local() && class_ctx == getClass()) {
            if (access == Internal) {
                return class_ctx;
            }
            return nullptr;
        }
        if (cls_context_map) {
            cls_context_map_t::const_iterator i = cls_context_map->find(class_ctx);
            if (i != cls_context_map->end()) {
                return i->second;
            }
        }

        return nullptr;
    }

    DLLLOCAL size_t getContextSize() const {
        return cls_context_map ? cls_context_map->size() : 0;
    }

    // issue #2970: marks the member to be accessed with the classes in the argument when
    // accessing the object's data
    DLLLOCAL void addContextAccess(const QoreMemberInfo& mi);

    // issue #2970: marks the member to be accessed with the classes in the argument when
    // accessing the object's data
    DLLLOCAL void addContextAccess(const QoreMemberInfo& mi, const qore_class_private* qc);

    // issue #2970: returns the number of parent class members to initialize
    DLLLOCAL size_t numParentMembers() const {
        return member_info_list ? member_info_list->size() : 0;
    }

    // issue #2970: initialization begin
    DLLLOCAL member_info_list_t::const_iterator initializationBegin() const {
        assert(member_info_list);
        return member_info_list->begin();
    }

    // issue #2970: initialization end
    DLLLOCAL member_info_list_t::const_iterator initializationEnd() const {
        assert(member_info_list);
        return member_info_list->end();
    }

    // initializes the member
    DLLLOCAL int parseInit(const char* name, LocalVar& selfid);

    // sets the transient flag
    DLLLOCAL void setTransient() {
        assert(!is_transient);
        is_transient = true;
    }

    // returns the transient flag
    DLLLOCAL bool getTransient() const {
        return is_transient;
    }

private:
    // the classes where this member is accessible; the first class is the class where the member was defined
    cls_vec_t cls_vec;
    // maps class contexts to class pointers in object storage for private members
    cls_context_map_t* cls_context_map = nullptr;
    // list of inherited members for initialization
    member_info_list_t* member_info_list = nullptr;

    // local flag
    bool is_local,
        // transient flag
        is_transient = false;

    /**
        @param old the old object
        @param cls the new class inheriting the member
        @param cls_access the inheritance access modifier for qc relative to this class

        issue #2970:
        1) ensure that inherited members get marked with the correct access information: max of declared member access
            and class inheritance access
        2) mark members imported from "private:internal" class inheritance with the class context
    */
    DLLLOCAL QoreMemberInfo(const QoreMemberInfo& old, const qore_class_private* cls, ClassAccess cls_access);
};

class QoreVarInfo : public QoreMemberInfoBaseAccess {
public:
    mutable QoreVarRWLock rwl;
    QoreLValueGeneric val;
    bool finalized;
    // flag telling if the static var has been evaluated
    bool eval_init = false;

    DLLLOCAL QoreVarInfo(const QoreProgramLocation* loc, const QoreTypeInfo* n_typeinfo = nullptr,
            QoreParseTypeInfo* n_parseTypeInfo = nullptr, QoreValue e = QoreValue(), ClassAccess n_access = Public) :
        QoreMemberInfoBaseAccess(loc, n_typeinfo, n_parseTypeInfo, e, n_access), finalized(false) {
    }

    DLLLOCAL QoreVarInfo(const QoreVarInfo& old, ClassAccess n_access = Public)
            : QoreMemberInfoBaseAccess(old, n_access), val(old.val), finalized(old.finalized), eval_init(old.eval_init) {
    }

    DLLLOCAL ~QoreVarInfo() {
        assert(!val.hasValue());
    }

    DLLLOCAL int evalInit(const char* name, ExceptionSink* xsink);

#ifdef DEBUG
    DLLLOCAL void del() {
        assert(!val.hasValue());
        QoreMemberInfoBaseAccess::del();
    }
#endif

    DLLLOCAL void clear(ExceptionSink* xsink) {
        ValueHolder tmp(xsink);
        QoreAutoVarRWWriteLocker al(rwl);
        if (!finalized)
            finalized = true;
        tmp = val.removeValue(true);
    }

    DLLLOCAL void delVar(ExceptionSink* xsink) {
#ifdef DEBUG
        QoreMemberInfoBaseAccess::del();
#else
        del();
#endif
        val.removeValue(true).discard(xsink);
    }

    DLLLOCAL AbstractQoreNode* assignInit(QoreValue v) {
        // try to set an optimized value type for the value holder if possible
        val.set(getTypeInfo());
        return val.assignInitial(v);
    }

    DLLLOCAL void getLValue(LValueHelper& lvh) {
        lvh.setAndLock(rwl);
        if (checkFinalized(lvh.vl.xsink))
            return;
        lvh.setValue(val, getTypeInfo());
    }

    DLLLOCAL void init() {
        val.set(getTypeInfo());
        if (getProgram()->getParseOptions64() & PO_STRICT_TYPES) {
            // try to set an optimized value type for the value holder if possible
            discard(val.assignInitial(QoreTypeInfo::getDefaultQoreValue(typeInfo)), nullptr);
        }
    }

    // can be called during parse initialization, in which case the variable must be initialized first
    DLLLOCAL QoreValue getReferencedValue(const char* name, ExceptionSink* xsink) {
        if (!eval_init && evalInit(name, xsink)) {
            return QoreValue();
        }
        return getRuntimeReferencedValue();
    }

    DLLLOCAL QoreValue getRuntimeReferencedValue() const {
        QoreAutoVarRWReadLocker al(rwl);
        return val.getReferencedValue();
    }

    DLLLOCAL int64 getAsBigInt() const {
        QoreAutoVarRWReadLocker al(rwl);
        return val.getAsBigInt();
    }

    DLLLOCAL double getAsFloat() const {
        QoreAutoVarRWReadLocker al(rwl);
        return val.getAsFloat();
    }

    DLLLOCAL bool getAsBool() const {
        QoreAutoVarRWReadLocker al(rwl);
        return val.getAsBool();
    }

    DLLLOCAL int parseInit(const char* name);

protected:
    DLLLOCAL int checkFinalized(ExceptionSink* xsink) const {
        if (finalized) {
            xsink->raiseException("DESTRUCTOR-ERROR", "illegal class static variable assignment after second phase "
                "of variable destruction");
            return -1;
        }
        return 0;
    }
};

template <typename T>
class QoreMemberMapBase {
public:
    typedef std::pair<char*, std::unique_ptr<T>> member_list_element_t;
    typedef std::deque<member_list_element_t> member_list_t;
    typedef typename member_list_t::iterator iterator;
    typedef typename member_list_t::const_iterator const_iterator;
    member_list_t member_list;

    DLLLOCAL ~QoreMemberMapBase() {
        for (auto& i : member_list) {
            //printd(5, "QoreMemberMap::~QoreMemberMap() this: %p freeing member %p '%s'\n", this, i->second, i->first);
            // the key is allocated normally in the scanner, and must be freed manually here
            free(i.first);
        }
        member_list.clear();
    }

    DLLLOCAL bool inList(const char* name) const {
        return (bool)find(name);
    }

    DLLLOCAL T* find(const char* name) const {
        typename member_list_t::const_iterator i =
            std::find_if(member_list.begin(), member_list.end(), [name](const member_list_element_t& e)
                -> bool {return !strcmp(e.first, name); });
        return i == member_list.end() ? nullptr : i->second.get();
    }

    DLLLOCAL T* replace(const char* name, T* info) {
        typename member_list_t::iterator i =
            std::find_if(member_list.begin(), member_list.end(), [name](const member_list_element_t& e)
                -> bool {return !strcmp(e.first, name); });
        assert(i != member_list.end());
        T* rv = i->second.release();
        i->second = std::unique_ptr<T>(info);
        return rv;
    }

    DLLLOCAL bool empty() const {
        return member_list.empty();
    }

    DLLLOCAL void addNoCheck(char* name, T* info) {
        assert(name);
        assert(info);
        assert(!inList(name));
        member_list.push_back(std::make_pair(name, std::unique_ptr<T>(info)));
    }

    DLLLOCAL void addNoCheck(std::pair<char*, T*> pair) {
        addNoCheck(pair.first, pair.second);
    }

    DLLLOCAL void moveAllTo(QoreMemberMapBase<T>& dest) {
        dest.member_list.insert(dest.member_list.end(), member_list.begin(), member_list.end());
        member_list.clear();
    }

    DLLLOCAL size_t size() const {
        return member_list.size();
    }
};

class QoreMemberMap : public QoreMemberMapBase<QoreMemberInfo> {
public:
    using QoreMemberMapBase<QoreMemberInfo>::moveAllTo;
    DLLLOCAL void moveAllTo(QoreClass* qc, ClassAccess access);

    using QoreMemberMapBase<QoreMemberInfo>::addNoCheck;
    DLLLOCAL void addNoCheck(char* name, QoreMemberInfo* info) {
        assert(info->getClass());
        QoreMemberMapBase<QoreMemberInfo>::addNoCheck(name, info);
    }

    DLLLOCAL void addInheritedNoCheck(char* name, QoreMemberInfo* info) {
        assert(name);
        assert(info);
        assert(!inList(name));
        member_list.insert(member_list.begin(), std::make_pair(name, std::unique_ptr<QoreMemberInfo>(info)));
    }

    DLLLOCAL int parseInit(LocalVar& selfid);

private:
    bool init = false;
};

class QoreVarMap : public QoreMemberMapBase<QoreVarInfo> {
public:
    DLLLOCAL void clear(ExceptionSink* xsink) {
        for (member_list_t::reverse_iterator i = member_list.rbegin(), e = member_list.rend(); i != e; ++i) {
            i->second->clear(xsink);
        }
    }

    DLLLOCAL void del(ExceptionSink* xsink) {
        for (member_list_t::reverse_iterator i = member_list.rbegin(), e = member_list.rend(); i != e; ++i) {
            i->second->delVar(xsink);
            // the key is allocated normally in the scanner, and must be freed manually here
            free(i->first);
        }
        member_list.clear();
    }

    DLLLOCAL void del() {
        for (member_list_t::reverse_iterator i = member_list.rbegin(), e = member_list.rend(); i != e; ++i) {
            assert(!i->second->val.hasValue());
            /*
            // when rolling back a failed parse, vars may have values, but no exception can happen, so xsink can be nullptr
            i->second->delVar(nullptr);
            */
            // the key is allocated normally in the scanner, and must be freed manually here
            free(i->first);
        }
        member_list.clear();
    }

    DLLLOCAL void clearNoFree() {
        member_list.clear();
    }

    DLLLOCAL void moveAllTo(QoreClass* qc, ClassAccess access);

    DLLLOCAL void parseCommitRuntimeInit(ExceptionSink* xsink);

private:
    bool init = false;
};

/*
  BCANode
  base class constructor argument node
*/
class BCANode : public FunctionCallBase {
public:
    // set automatically when created
    const QoreProgramLocation* loc;
    qore_classid_t classid = 0;
    //QoreClass* sclass;
    NamedScope* ns;
    char* name;

    // this function takes ownership of n and arg
    DLLLOCAL BCANode(NamedScope* n, QoreParseListNode* n_args, const QoreProgramLocation* loc)
            : FunctionCallBase(n_args), loc(loc), ns(n), name(nullptr) {
        assert(loc->start_line > 0);
    }

    // this function takes ownership of n and arg
    DLLLOCAL BCANode(char* n, QoreParseListNode* n_args, const QoreProgramLocation* loc)
            : FunctionCallBase(n_args), loc(loc), ns(nullptr), name(n) {
        assert(loc->start_line > 0);
    }

    DLLLOCAL ~BCANode() {
        delete ns;
        if (name)
            free(name);
    }

    // resolves classes, parses arguments, and attempts to find constructor variant
    DLLLOCAL int parseInit(BCList* bcl, const char* classname);
};

typedef std::vector<BCANode*> bcalist_t;

// BCAList
// base class constructor argument list
// this data structure will not be modified even if the class is copied
// to a subprogram object
class BCAList : public bcalist_t {
public:
   DLLLOCAL BCAList(BCANode* n) {
      push_back(n);
   }

   DLLLOCAL ~BCAList() {
      for (bcalist_t::iterator i = begin(), e = end(); i != e; ++i)
         delete *i;
   }

   // returns 0 for no errors, -1 for exception raised
   DLLLOCAL int execBaseClassConstructorArgs(BCEAList* bceal, ExceptionSink* xsink) const;
};

typedef std::pair<QoreClass*, bool> class_virt_pair_t;
//typedef std::list<class_virt_pair_t> class_list_t;
typedef std::vector<class_virt_pair_t> class_list_t;

// member initialization list entry
struct member_init_entry_t {
    const char* name;
    const QoreMemberInfo* info;
    const qore_class_private* member_class_ctx;

    DLLLOCAL member_init_entry_t(const char* name, const QoreMemberInfo* info, const qore_class_private* member_class_ctx) :
        name(name), info(info), member_class_ctx(member_class_ctx) {
    }
};

// list of members in initialization order
typedef std::vector<member_init_entry_t> member_init_list_t;

// BCSMList: Base Class Special Method List
// unique list of base classes for a class hierarchy to ensure that "special" methods, constructor(), destructor(), copy() - are executed only once
// this class also tracks virtual classes to ensure that they are not inserted into the list in a complex tree and executed here
class BCSMList : public class_list_t {
public:
    DLLLOCAL BCSMList() {
    }

    DLLLOCAL BCSMList(const BCSMList &old);

    DLLLOCAL ~BCSMList();

    DLLLOCAL void processMemberInitializationList(const QoreMemberMap& members, member_init_list_t& member_init_list);

    DLLLOCAL int add(QoreClass* thisclass, QoreClass* qc, bool is_virtual);
    DLLLOCAL int addBaseClassesToSubclass(QoreClass* thisclass, QoreClass* sc, bool is_virtual);

    DLLLOCAL void alignBaseClassesInSubclass(QoreClass* thisclass, QoreClass* child, bool is_virtual);

    // returns 0 = can add, non-0 = cannot add
    DLLLOCAL void align(QoreClass* thisclass, QoreClass* qc, bool is_virtual);

    DLLLOCAL const QoreClass* getClass(qore_classid_t cid) const;
    //DLLLOCAL void execConstructors(QoreObject* o, BCEAList* bceal, ExceptionSink* xsink) const;
    DLLLOCAL void execDestructors(QoreObject* o, ExceptionSink* xsink) const;
    DLLLOCAL void execSystemDestructors(QoreObject* o, ExceptionSink* xsink) const;
    DLLLOCAL void execCopyMethods(QoreObject* self, QoreObject* old, ExceptionSink* xsink) const;

    // parseResolve classes to the new class pointer after all namespaces and classes have been copied
    DLLLOCAL void resolveCopy();
};

// set of private class pointers; used when checking for recursive class inheritance lists
typedef vector_set_t<qore_class_private*> qcp_set_t;
//typedef std::set<qore_class_private*> qcp_set_t;

// BCNode
// base class pointer
class BCNode {
public:
    // populated automatically on creation
    const QoreProgramLocation* loc;
    NamedScope* cname = nullptr;
    char* cstr = nullptr;
    QoreClass* sclass = nullptr;
    ClassAccess access;
    bool is_virtual : 1;

    DLLLOCAL BCNode(const QoreProgramLocation* loc, NamedScope* c, ClassAccess a) : loc(loc), cname(c), access(a),
            is_virtual(false) {
    }

    // this method takes ownership of *str
    DLLLOCAL BCNode(const QoreProgramLocation* loc, char* str, ClassAccess a) : loc(loc), cstr(str), access(a),
            is_virtual(false) {
    }

    // for builtin base classes
    DLLLOCAL BCNode(const QoreProgramLocation* loc, QoreClass* qc, bool n_virtual = false) : loc(loc), sclass(qc),
            access(Public), is_virtual(n_virtual) {
    }

    // called at runtime with committed classes
    DLLLOCAL BCNode(const BCNode &old) : loc(old.loc), sclass(old.sclass), access(old.access),
            is_virtual(old.is_virtual) {
        assert(!old.cname);
        assert(!old.cstr);
        assert(old.sclass);
    }

    DLLLOCAL ~BCNode() {
        delete cname;
        if (cstr)
            free(cstr);
    }

    DLLLOCAL int tryResolveClass(QoreClass* cls, bool raise_error);

    DLLLOCAL ClassAccess getAccess() const { return access; }

    // returns -1 if a recursive reference is found, 0 if not
    DLLLOCAL int initializeHierarchy(QoreClass* cls, qcp_set_t& qcp_set);

    DLLLOCAL int initializeMembers(QoreClass* cls);

    // returns -1 if a recursive reference is found, 0 if not
    DLLLOCAL int initialize(QoreClass* cls);

    DLLLOCAL bool isBaseClass(QoreClass* qc, bool toplevel) const;

    DLLLOCAL const QoreMethod* runtimeFindCommittedMethod(const char* name, ClassAccess& n_access,
            const qore_class_private* class_ctx, bool allow_internal) const;
    DLLLOCAL const QoreMethod* runtimeFindCommittedStaticMethod(const char* name, ClassAccess& n_access,
            const qore_class_private* class_ctx, bool allow_internal) const;

    DLLLOCAL bool runtimeIsPrivateMember(const char* str, bool toplevel) const;

    DLLLOCAL void execConstructors(QoreObject* o, BCEAList* bceal, ExceptionSink* xsink) const;

    DLLLOCAL const QoreMemberInfo* parseFindMember(const char* mem, const qore_class_private*& qc,
            ClassAccess& n_access, bool toplevel) const;
    DLLLOCAL const QoreVarInfo* parseFindVar(const char* name, const qore_class_private*& qc, ClassAccess& n_access,
            bool toplevel) const;

    DLLLOCAL const QoreClass* findInHierarchy(const qore_class_private& qc);

    DLLLOCAL const QoreClass* getClass(qore_classid_t cid, ClassAccess& n_access, bool toplevel) const;
    DLLLOCAL const QoreClass* getClass(const qore_class_private& qc, ClassAccess& n_access, bool toplevel) const;
    DLLLOCAL const QoreClass* parseGetClass(const qore_class_private& qc, ClassAccess& n_access, bool toplevel) const;
    DLLLOCAL bool inHierarchy(const qore_class_private& qc, ClassAccess& n_access) const;
    DLLLOCAL bool inHierarchyStrict(const qore_class_private& qc, ClassAccess& n_access) const;

    // inaccessible methods are ignored
    DLLLOCAL const QoreMethod* parseFindNormalMethod(const char* name, const qore_class_private* class_ctx,
            bool allow_internal) const;

    // inaccessible methods are ignored
    DLLLOCAL const QoreMethod* parseFindStaticMethod(const char* name, const qore_class_private* class_ctx,
            bool allow_internal) const;

    // inaccessible methods are ignored
    DLLLOCAL const QoreMethod* parseResolveSelfMethod(const QoreProgramLocation* loc, const char* name,
            const qore_class_private* class_ctx, bool allow_internal) const;

    DLLLOCAL bool parseCheckHierarchy(const QoreClass* cls, ClassAccess& n_access, bool toplevel) const;

    DLLLOCAL QoreVarInfo* parseFindStaticVar(const char* vname, const QoreClass*& qc, ClassAccess& n_access,
            bool check, bool toplevel) const;

    DLLLOCAL QoreValue parseFindConstantValue(const char* cname, const QoreTypeInfo*& typeInfo, bool &found,
            const qore_class_private* class_ctx, bool allow_internal) const;

    DLLLOCAL int addBaseClassesToSubclass(QoreClass* child, bool is_virtual);

    DLLLOCAL void initializeBuiltin();
};

typedef std::vector<BCNode*> bclist_t;

//  BCList
//  linked list of base classes, constructors called head->tail,
//  destructors called in reverse order (tail->head) (stored in BCSMList)
//  note that this data structure cannot be modified even if the class is
//  copied to a subprogram object and extended
//  this class is a QoreReferenceCounter so it won't be copied when the class is copied
class BCList : public bclist_t {
protected:
public:
    // special method (constructor, destructor, copy) list for superclasses
    BCSMList sml;
    bool valid = true;
    bool rescanned = false;

    DLLLOCAL BCList(BCNode* n) {
        push_back(n);
    }

    DLLLOCAL BCList() {
    }

    DLLLOCAL BCList(const BCList& old) : sml(old.sml) {
        assert(old.valid);
        reserve(old.size());
        for (bclist_t::const_iterator i = old.begin(), e = old.end(); i != e; ++i)
            push_back(new BCNode(*(*i)));
    }

    DLLLOCAL ~BCList() {
        for (bclist_t::iterator i = begin(), e = end(); i != e; ++i)
            delete *i;
    }

    DLLLOCAL int initializeHierarchy(QoreClass* thisclass, qcp_set_t& qcp_set);

    DLLLOCAL int initializeMembers(QoreClass* thisclass);

    DLLLOCAL int initialize(QoreClass* thisclass);

    // inaccessible methods are ignored
    DLLLOCAL const QoreMethod* parseResolveSelfMethod(const QoreProgramLocation* loc, const char* name,
            const qore_class_private* class_ctx, bool allow_internal);

    // inaccessible methods are ignored
    DLLLOCAL const QoreMethod* parseFindNormalMethod(const char* name, const qore_class_private* class_ctx,
            bool allow_internal);
    // inaccessible methods are ignored
    DLLLOCAL const QoreMethod* parseFindStaticMethod(const char* name, const qore_class_private* class_ctx,
            bool allow_internal);

    DLLLOCAL const QoreMethod* runtimeFindCommittedMethod(const char* name, ClassAccess& access,
            const qore_class_private* class_ctx, bool allow_internal) const;
    DLLLOCAL const QoreMethod* runtimeFindCommittedStaticMethod(const char* name, ClassAccess& access,
            const qore_class_private* class_ctx, bool allow_internal) const;

    DLLLOCAL bool match(const QoreClass* cls);
    DLLLOCAL void execConstructors(QoreObject* o, BCEAList* bceal, ExceptionSink* xsink) const;

    DLLLOCAL bool runtimeIsPrivateMember(const char* str, bool toplevel) const;

    DLLLOCAL bool parseCheckHierarchy(const QoreClass* cls, ClassAccess& access, bool toplevel) const;

    DLLLOCAL const QoreMemberInfo* parseFindMember(const char* mem, const qore_class_private*& qc,
            ClassAccess& n_access, bool toplevel) const;

    DLLLOCAL const QoreVarInfo* parseFindVar(const char* vname, const qore_class_private*& qc, ClassAccess& access,
            bool toplevel) const;

    DLLLOCAL bool parseHasPublicMembersInHierarchy() const;

    DLLLOCAL const QoreClass* findInHierarchy(const qore_class_private& qc);

    DLLLOCAL const QoreClass* getClass(qore_classid_t cid, ClassAccess& n_access, bool toplevel) const;
    DLLLOCAL const QoreClass* getClass(const qore_class_private& qc, ClassAccess& n_access, bool toplevel) const;

    DLLLOCAL const QoreClass* parseGetClass(const qore_class_private& qc, ClassAccess& n_access, bool toplevel) const;
    DLLLOCAL bool inHierarchy(const qore_class_private& qc, ClassAccess& n_access) const;
    DLLLOCAL bool inHierarchyStrict(const qore_class_private& qc, ClassAccess& n_access) const;

    DLLLOCAL void addNewAncestors(QoreMethod* m);
    DLLLOCAL void addAncestors(QoreMethod* m);
    DLLLOCAL void addNewStaticAncestors(QoreMethod* m);
    DLLLOCAL void addStaticAncestors(QoreMethod* m);
    DLLLOCAL void parseAddAncestors(QoreMethod* m);
    DLLLOCAL void parseAddStaticAncestors(QoreMethod* m);

    DLLLOCAL void parseResolveAbstract();

    DLLLOCAL QoreValue parseFindConstantValue(const char* cname, const QoreTypeInfo*& typeInfo, bool& found,
            const qore_class_private* class_ctx, bool allow_internal) const;

    DLLLOCAL QoreVarInfo* parseFindStaticVar(const char* vname, const QoreClass*& qc, ClassAccess& access, bool check,
            bool toplevel) const;

    DLLLOCAL void resolveCopy();

    DLLLOCAL MethodVariantBase* matchNonAbstractVariant(const std::string& name, MethodVariantBase* v) const;

    DLLLOCAL bool isBaseClass(QoreClass* qc, bool toplevel) const;

    DLLLOCAL int addBaseClassesToSubclass(QoreClass* thisparent, QoreClass* child, bool is_virtual) {
        for (auto& i : *this) {
            if ((*i).addBaseClassesToSubclass(child, is_virtual))
                return -1;
        }
        return sml.addBaseClassesToSubclass(thisparent, child, is_virtual);
    }

    DLLLOCAL void rescanParents(QoreClass* cls);

    DLLLOCAL void initializeBuiltin() {
        for (auto& i : *this) {
            (*i).initializeBuiltin();
        }
    }
};

// BCEANode
// base constructor evaluated argument node; created locally at run time
class BCEANode {
public:
   const QoreProgramLocation* loc;
   QoreListNode* args = nullptr;
   const AbstractQoreFunctionVariant* variant = nullptr;
   bool execed = false;
   bool member_init_done = false;

   DLLLOCAL BCEANode(const QoreProgramLocation* loc, QoreListNode* args, const AbstractQoreFunctionVariant* variant) : loc(loc), args(args), variant(reinterpret_cast<const MethodVariant*>(variant)) {
   }

   DLLLOCAL BCEANode(bool n_execed = true, bool mid = true) : execed(n_execed), member_init_done(mid) {
   }
};

/*
struct ltqc {
   bool operator()(const QoreClass* qc1, const QoreClass* qc2) const {
      return qc1 < qc2;
   }
};
*/

//typedef std::map<qore_classid_t, BCEANode*> bceamap_t;
typedef vector_map_t<qore_classid_t, BCEANode*> bceamap_t;

/*
  BCEAList
  base constructor evaluated argument list
*/
class BCEAList : public bceamap_t {
protected:
    DLLLOCAL ~BCEAList() {
        assert(empty());
    }

public:
    DLLLOCAL void deref(ExceptionSink* xsink);
    // evaluates arguments, returns -1 if an exception was thrown
    DLLLOCAL int add(qore_classid_t classid, const QoreListNode* arg, const AbstractQoreFunctionVariant* variant, const QoreProgramLocation* loc, ExceptionSink* xsink);
    DLLLOCAL QoreListNode* findArgs(qore_classid_t classid, bool* aexeced, const AbstractQoreFunctionVariant*& variant, const QoreProgramLocation*& loc);
    /*
    DLLLOCAL bool initMembers(qore_classid_t classid) {
        bceamap_t::iterator i = lower_bound(classid);
        if (i == end() || i->first != classid) {
            insert(i, bceamap_t::value_type(classid, new BCEANode(false, true)));
            return false;
        }
        if (!i->second->member_init_done) {
            i->second->member_init_done = true;
            return false;
        }
        return true;
    }
    */
};

struct SelfInstantiatorHelper {
    LocalVar* selfid;
    DLLLOCAL SelfInstantiatorHelper(LocalVar* n_selfid, QoreObject* self) : selfid(n_selfid) {
        selfid->instantiateSelf(self);
    }
    DLLLOCAL ~SelfInstantiatorHelper() {
        selfid->uninstantiateSelf();
    }
};

// signature hash size - we use SHA1 for performance reasons (and because we don't necessarily need the best cryptographic security)
#define SH_SIZE 20

// class "signature" hash for comparing classes with the same name from different program objects at runtime
class SignatureHash {
protected:
    unsigned char buf[SH_SIZE];
    bool is_set;

    DLLLOCAL void set(const QoreString& str);

    DLLLOCAL void clearHash() {
        memset(buf, 0, SH_SIZE);
    }

    DLLLOCAL void copyHash(const SignatureHash& other) {
        memcpy(buf, other.buf, SH_SIZE);
    }

public:
    DLLLOCAL SignatureHash() : is_set(false) {
        clearHash();
    }

    DLLLOCAL SignatureHash(const SignatureHash& old) : is_set(old.is_set) {
        if (is_set)
            copyHash(old);
    }

    DLLLOCAL void update(const QoreString& str);

    DLLLOCAL void updateEmpty() {
        assert(!is_set);
        clearHash();
        is_set = true;
    }

    DLLLOCAL bool operator==(const SignatureHash& other) const {
        // if either one of the hashes is not set, then the comparison always fails
        if (!is_set || !other.is_set)
            return false;
        return !memcmp(buf, other.buf, SH_SIZE);
    }

    DLLLOCAL SignatureHash& operator=(const SignatureHash& other) {
        if (!other.is_set) {
            assert(false);
            clear();
        } else {
            if (!is_set)
                is_set = true;
            copyHash(other);
        }
        return *this;
    }

    DLLLOCAL operator bool() const {
        return is_set;
    }

    // appends the hash to the string
    DLLLOCAL void toString(QoreString& str) const {
        for (unsigned i = 0; i < SH_SIZE; ++i) {
            str.sprintf("%02x", buf[i]);
        }
    }

#ifdef DEBUG
    // for debugging only; uses a static buffer
    DLLLOCAL const char* debugToString() const {
        static QoreString str;
        str.clear();
        for (unsigned i = 0; i < SH_SIZE; ++i) {
            str.sprintf("%02x", buf[i]);
        }
        return str.c_str();
    }
#endif

    DLLLOCAL char* getHash() const {
        assert(is_set);
        return (char*)buf;
    }

    DLLLOCAL void clear() {
        if (is_set) {
            is_set = false;
            clearHash();
        }
    }
};

#define QCCM_NORMAL (1 << 0)
#define QCCM_STATIC (1 << 1)

// set of QoreClass pointers associated to a qore_class_private object
typedef vector_set_t<QoreClass*> qc_set_t;
//typedef std::set<QoreClass*> qc_set_t;

// private QoreClass implementation
// only dynamically allocated; reference counter managed in "refs"
class qore_class_private {
public:
    const QoreProgramLocation* loc; // location of declaration
    std::string name;               // the name of the class
    std::string path;               // the full namespace path for the class
    QoreClass* cls;                 // parent class
    qore_ns_private* ns = nullptr;  // parent namespace
    BCList* scl = nullptr;          // base class list
    qc_set_t qcset;                 // set of QoreClass pointers associated with this private object (besides cls)

    mutable VRMutex gate;           // for synchronized static methods

    hm_method_t hm,                 // "normal" (non-static) method map
        shm;                        // static method map

    AbstractMethodMap ahm;          // holds abstract variants with no implementation in the current class

    ConstantList constlist;         // class constants

    // member list (map)
    QoreMemberMap members;
    // member initialization list in hierarchy nitialization order
    member_init_list_t member_init_list;

    // static var list (map)
    QoreVarMap vars;

    const QoreMethod* system_constructor = nullptr,
        * constructor = nullptr,
        * destructor = nullptr,
        * copyMethod = nullptr,
        * methodGate = nullptr,
        * memberGate = nullptr,
        * memberNotification = nullptr;

    q_serializer_t serializer = nullptr;
    q_deserializer_t deserializer = nullptr;

    qore_classid_t classID,          // class ID
        methodID;                    // for subclasses of builtin classes that will not have their own private data,
                                     // instead they will get the private data from this class

    bool sys : 1,                         // system/builtin class?
        initialized : 1,                  // is initialized? (only performed once)
        static_init : 1,                  // has static initialization been called for the class?
        parse_init_called : 1,            // has parseInit() been called? (performed once for each parseCommit())
        parse_init_partial_called : 1,    // has parseInitPartial() been called? (performed once for each parseCommit())
        has_public_memdecl : 1,           // has a public member declaration somewhere in the hierarchy?
        pending_has_public_memdecl : 1,   // has a pending public member declaration in this class?
        owns_typeinfo : 1,                // do we own the typeInfo data or not?
        resolve_copy_done : 1,            // has the copy already been resolved
        has_new_user_changes : 1,         // does the class have new user code that needs to be processed?
        has_sig_changes : 1,              // does the class have code affecting the signature to be processed?
        owns_ornothingtypeinfo : 1,       // do we own the "or nothing" type info
        pub : 1,                          // is a public class (modules only)
        final : 1,                        // is the class "final" (cannot be inherited)
        inject : 1,                       // has the class been injected
        gate_access : 1,                  // if the methodGate and memberGate methods should be called with a class access boolean
        committed : 1,                    // can only parse to a class once
        parse_resolve_hierarchy : 1,      // class hierarchy resolved
        parse_resolve_class_members : 1,  // class members resolved
        parse_resolve_abstract : 1,       // abstract methods resolved
        has_transient_member : 1          // has at least one transient member
        ;

    int64 domain;                    // capabilities of builtin class to use in the context of parse restrictions
    mutable QoreReferenceCounter refs;  // existence references
    mutable QoreReferenceCounter const_refs; // constant references
    mutable QoreReferenceCounter var_refs;   // static var references

    unsigned num_methods, num_user_methods, num_static_methods, num_static_user_methods;

    // type information for the class, may not have a pointer to the same QoreClass
    // as the actual owning class in case of a copy
    QoreClassTypeInfo* typeInfo = nullptr;
    QoreClassOrNothingTypeInfo* orNothingTypeInfo = nullptr;

    const qore_class_private* injectedClass = nullptr;

    // common "self" local variable for all constructors
    mutable LocalVar selfid;

    // class "signature" hash for comparing classes with the same name from different program objects at runtime
    SignatureHash hash;

    // user-specific data
    const void* ptr = nullptr;

    // managed user-specific data
    AbstractQoreClassUserData* mud = nullptr;

    // pointer to new class when copying
    mutable QoreClass* new_copy = nullptr;

    // pointer to owning program for imported classes
    QoreProgram* spgm = nullptr;

    // if the souorce program should be dereference when the class is destroyed
    bool deref_source_program = true;

    // the module that defined this class, if any
    std::string from_module;

    // class language
    std::string lang = "Qore";

    // class key-value store
    typedef std::map<std::string, QoreValue> kvmap_t;
    kvmap_t kvmap;

    DLLLOCAL qore_class_private(QoreClass* n_cls, std::string&& nme, std::string&& path, int64 dom = QDOM_DEFAULT,
            QoreClassTypeInfo* n_typeinfo = nullptr);

    // only called while the parse lock for the QoreProgram owning "old" is held
    // called for injected classes only
    DLLLOCAL qore_class_private(const qore_class_private& old, qore_ns_private* ns, QoreProgram* spgm,
            const char* nme, bool inject, const qore_class_private* injectedClass, q_setpub_t set_pub);

    //! Sets a key value in the class's key-value store
    /** @param key the key to store
        @param value the value to store

        @return any value previously stored in that key; must be dereferenced by the caller

        @since %Qore 1.0
    */
    DLLLOCAL QoreValue setKeyValue(const std::string& key, QoreValue val);

    //! Sets a key value in the class's key-value store only if no value exists for the given key
    /** @param key the key to store
        @param value the value to store; must be already referenced for storage

        @return returns \a value if another value already exists for that key, otherwise returns no value

        @note if \a value is returned, the caller must dereference it

        @since %Qore 1.0
    */
    DLLLOCAL QoreValue setKeyValueIfNotSet(const std::string& key, QoreValue val);

    //! Sets a key value in the class's key-value store only if no value exists for the given key
    /** @param key the key to store
        @param value the string to store; will be converted to a QoreStringNode if stored

        @param returns true if the value was set, false if not (a value is already in place)

        @note All class key-value operations are atomic

        @since %Qore 1.0
    */
    DLLLOCAL bool setKeyValueIfNotSet(const std::string& key, const char* str);

    //! Returns a referenced key value from the class's key-value store
    /** @param key the key to check

        @return the value corersponding to the key; the caller is responsible for dereferencing the value returned

        @since %Qore 1.0
    */
    DLLLOCAL QoreValue getReferencedKeyValue(const std::string& key) const;

    //! Returns a referenced key value from the class's key-value store
    /** @param key the key to check

        @return the value corersponding to the key; the caller is responsible for dereferencing the value returned

        @since %Qore 1.0.13
    */
    DLLLOCAL QoreValue getReferencedKeyValue(const char* key) const;

    // add a base class to this class
    DLLLOCAL void addBaseClass(QoreClass* qc, bool virt);

    // This function must only be called from QoreObject
    DLLLOCAL QoreValue evalMemberGate(QoreObject* self, const char* nme, ExceptionSink* xsink) const;

    DLLLOCAL const char* getModuleName() const {
        return from_module.empty() ? nullptr : from_module.c_str();
    }

    // get base class list to add virtual class indexes for private data
    DLLLOCAL BCSMList* getBCSMList() const {
        return scl ? &scl->sml : nullptr;
    }

    DLLLOCAL void pgmRef() const {
        refs.ROreference();
        var_refs.ROreference();
        const_refs.ROreference();
    }

    DLLLOCAL void ref() const {
        refs.ROreference();
    }

    DLLLOCAL bool deref(bool ns_const, bool ns_vars, bool in_del = false) {
        if (ns_const && const_refs.ROdereference()) {
            // constants may not be empty when deleting an uninitialized user class
            if (!constlist.empty()) {
                constlist.deleteAll(nullptr);
            }
        }
        if (ns_vars && var_refs.ROdereference()) {
            // vars may not be empty when deleting an uninitialized user class
            if (!vars.empty()) {
                vars.del(nullptr);
            }
        }

        if (refs.ROdereference()) {
            // remove the private data pointer, delete the class object, then delete ourselves
            cls->priv = nullptr;
            if (!in_del) {
                delete cls;
            }

            // delete all linked QoreClass objects
            for (auto& i : qcset) {
                i->priv = nullptr;
                delete i;
            }

            delete this;
            return true;
        }
        return false;
    }

    DLLLOCAL bool hasAbstract() const {
        return !ahm.empty();
    }

    DLLLOCAL void parseSetEmptyPublicMemberDeclaration() {
        pending_has_public_memdecl = true;
        if (!has_new_user_changes) {
            has_new_user_changes = true;
        }
    }

    DLLLOCAL int runtimeCheckInstantiateClass(ExceptionSink* xsink) const {
        return ahm.runtimeCheckInstantiateClass(name.c_str(), xsink);
    }

    DLLLOCAL void parseCheckAbstractNew(const QoreProgramLocation* loc) const;

    DLLLOCAL void parseDoCheckAbstractNew(const QoreProgramLocation* loc) const;

    DLLLOCAL void setNamespace(qore_ns_private* n) {
        assert(!ns);
        ns = n;
    }

    // returns true if the namespace was assigned
    DLLLOCAL bool setNamespaceConditional(qore_ns_private* n) {
        // only assign the namespace if it hasn't already been assiogned
        if (!ns) {
            ns = n;
            return true;
        }
        return false;
    }

    // used when assimilating a namespace at parse time
    DLLLOCAL void updateNamespace(qore_ns_private* n) {
        assert(ns);
        assert(ns != n);
        ns = n;
    }

    DLLLOCAL void resolveCopy();

    DLLLOCAL void setUserData(const void* n_ptr) {
        assert(!ptr);
        ptr = n_ptr;
    }

    DLLLOCAL const void* getUserData() const {
        return ptr;
    }

    DLLLOCAL void setManagedUserData(AbstractQoreClassUserData* n_mud) {
        assert(!mud);
        mud = n_mud;
    }

    DLLLOCAL AbstractQoreClassUserData* getManagedUserData() const {
        return mud;
    }

    DLLLOCAL const QoreTypeInfo* getTypeInfo() const {
        return typeInfo;
    }

    DLLLOCAL const QoreTypeInfo* getOrNothingTypeInfo() const {
        return orNothingTypeInfo;
    }

    DLLLOCAL bool runtimeIsPrivateMemberIntern(const char* str, bool toplevel) const;

    DLLLOCAL void parseImportMembers(qore_class_private& qc, ClassAccess access);

    DLLLOCAL bool parseHasMemberGate() const {
        return memberGate || hm.find("memberGate") != hm.end() || ahm.find("memberGate") != ahm.end();
    }

    DLLLOCAL bool parseHasMethodGate() const {
        return methodGate || hm.find("methodGate") != hm.end() || ahm.find("methodGate") != ahm.end();
    }

    // checks for all special methods except constructor & destructor
    DLLLOCAL bool checkAssignSpecialIntern(const QoreMethod* m) {
        // set quick pointers
        if (!methodGate && !strcmp(m->getName(), "methodGate")) {
            methodGate = m;
            return true;
        }

        if (!memberGate && !strcmp(m->getName(), "memberGate")) {
            memberGate = m;
            //printd(5, "qore_class_private::checkAssignSpecialIntern() this: %p got %s::%s()\n", this, name.c_str(), m->getName());
            return true;
        }

        if (!memberNotification && !strcmp(m->getName(), "memberNotification")) {
            memberNotification = m;
            return true;
        }

        return false;
    }

    // checks for all special methods except constructor, destructor, and copy
    DLLLOCAL bool checkSpecialStaticIntern(const char* mname) {
        // set quick pointers
        if ((!methodGate && !strcmp(mname, "methodGate"))
            || (!memberGate && !strcmp(mname, "memberGate"))
            || (!memberNotification && !strcmp(mname, "memberNotification")))
            return true;
        return false;
    }

    // checks for all special methods
    DLLLOCAL bool checkSpecial(const char* mname) {
        // set quick pointers
        if ((!methodGate && !strcmp(mname, "methodGate"))
            || (!memberGate && !strcmp(mname, "memberGate"))
            || (!memberNotification && !strcmp(mname, "memberNotification"))
            || (!constructor && !strcmp(mname, "constructor"))
            || (!destructor && !strcmp(mname, "destructor"))
            || (!copyMethod && !strcmp(mname, "copy")))
            return true;
        return false;
    }

    // checks for all special methods
    DLLLOCAL bool checkAssignSpecial(const QoreMethod* m) {
        // set quick pointers
        if (!constructor && !strcmp(m->getName(), "constructor")) {
            constructor = m;
            return true;
        }

        if (!destructor && !strcmp(m->getName(), "destructor")) {
            destructor = m;
            return true;
        }

        if (!copyMethod && !strcmp(m->getName(), "copy")) {
            copyMethod = m;
            return true;
        }

        return checkAssignSpecialIntern(m);
    }

    // merge abstract variants from base classes to child class
    DLLLOCAL void mergeAbstract();

    // returns -1 if a recursive inheritance list was found, 0 if not
    DLLLOCAL int initializeIntern();
    DLLLOCAL int initializeHierarchy(qcp_set_t& qcp_set);
    DLLLOCAL int initializeMembers();
    DLLLOCAL int initialize();

    DLLLOCAL int parseInitPartial();
    DLLLOCAL int parseInitPartialIntern();

    DLLLOCAL int parseCheckMemberAccess(const QoreProgramLocation* loc, const char* mem,
            const QoreTypeInfo*& memberTypeInfo, int pflag) const {
        const_cast<qore_class_private*>(this)->parseInitPartial();

        const qore_class_private* qc = nullptr;
        ClassAccess access;
        const QoreMemberInfo* omi = parseFindMember(mem, qc, access);

        if (!omi) {
            int rc = 0;
            if (!parseHasMemberGate() || (pflag & PF_FOR_ASSIGNMENT)) {
                if (parse_check_parse_option(PO_REQUIRE_TYPES)) {
                    parse_error(*loc, "member '%s' of class '%s' referenced has no type information because it was " \
                        "not declared in a public or private member list, but parse options require type " \
                        "information for all declarations",
                        mem, name.c_str());
                    rc = -1;
                }
                if (parseHasPublicMembersInHierarchy()) {
                    //printd(5, "qore_class_private::parseCheckMemberAccess() %s %%.%s memberGate: %d pflag: %d\n",
                    //  name.c_str(), mem, parseHasMemberGate(), pflag);
                    parse_error(*loc, "illegal access to unknown member '%s' in class '%s' which has a public " \
                        "member list (or inherited public member list)", mem, name.c_str());
                    rc = -1;
                }
            }
            return rc;
        }

        memberTypeInfo = omi->getTypeInfo();

        // only raise a parse error for illegal access to private members if there is not memberGate function
        if ((access > Public) && !parseHasMemberGate() && !parseCheckPrivateClassAccess()) {
            memberTypeInfo = nullptr;
            parse_error(*loc, "illegal access to private member '%s' of class '%s'", mem, name.c_str());
            return -1;
        }
        return 0;
    }

    DLLLOCAL int parseResolveInternalMemberAccess(const char* mem, const QoreTypeInfo*& memberTypeInfo) const {
        const_cast<qore_class_private*>(this)->parseInitPartial();

        const qore_class_private* qc = nullptr;
        ClassAccess access;
        const QoreMemberInfo* omi = parseFindMember(mem, qc, access);
        if (omi) {
            memberTypeInfo = omi->getTypeInfo();
        }

        return omi ? 0 : -1;
    }

    DLLLOCAL int parseCheckInternalMemberAccess(const char* mem, const QoreTypeInfo*& memberTypeInfo,
            const QoreProgramLocation* loc) const {
        const_cast<qore_class_private*>(this)->parseInitPartial();

        // throws a parse exception if there are public members and the name is not valid
        const qore_class_private* qc = nullptr;
        ClassAccess access;
        const QoreMemberInfo* omi = parseFindMember(mem, qc, access);
        int rc = 0;
        if (omi) {
            // issue #3355: to handle out of order initialization properly, we need to ensure that the member is
            // initialized before accessing its type.  We cannot rely on calling members.parseInit() here because
            // we may have already been called by this call
            rc = const_cast<QoreMemberInfo*>(omi)->parseInit(mem, selfid);
            memberTypeInfo = omi->parseGetTypeInfo();
        }

        if (!omi) {
            if (parse_check_parse_option(PO_REQUIRE_TYPES)) {
                parse_error(*loc, "member '%s' of class '%s' referenced has no type information because it was not " \
                    "declared in a public or private member list, but parse options require type information for " \
                    "all declarations", mem, name.c_str());
                if (!rc) {
                    rc = -1;
                }
            }
            if (parseHasPublicMembersInHierarchy()) {
                parse_error(*loc, "illegal access to unknown member '%s' in class '%s' which has a public member " \
                    "list (or inherited public member list)", mem, name.c_str());
                if (!rc) {
                    rc = -1;
                }
            }
        }
        return rc;
    }

    DLLLOCAL bool parseHasPublicMembersInHierarchy() const {
        if (has_public_memdecl || pending_has_public_memdecl)
            return true;

        return scl ? scl->parseHasPublicMembersInHierarchy() : false;
    }

    /** this = the object's class

        @param mem the member name
        @param class_ctx the current class context

        @return the class context for the member or nullptr if it should be stored in the standard object data hash
    */
    DLLLOCAL const qore_class_private* runtimeGetMemberContext(const char* mem, const qore_class_private* class_ctx) const {
        const QoreMemberInfo* info = runtimeGetMemberInfo(mem, class_ctx);
        //printd(5, "qore_class_private::runtimeGetMemberContext() this: %p '%s' mem: '%s' info: %p member class: %p '%s' ctx(%p): %p (size: %d)\n", this, name.c_str(), mem, info, info ? info->getClass() : nullptr, info ? info->getClass()->name.c_str() : "n/a", class_ctx, info ? info->getClassContext(class_ctx) : nullptr, info ? info->getContextSize() : 0);
        return info ? info->getClassContext(class_ctx) : nullptr;
    }

    DLLLOCAL bool runtimeIsMemberInternal(const char* mem) const {
        QoreMemberInfo* info = members.find(mem);
        return info && info->isLocalInternal() ? true : false;
    }

    /**
        this = the object's class

        @param mem the member name
        @param access output variable for the access of the member
        @param class_ctx the current class context; class_ctx is only set if it is present and accessible, so we only need to check for internal access here

        @return the member found or nullptr if not found
    */
    DLLLOCAL const QoreMemberInfo* runtimeGetMemberInfo(const char* mem, const qore_class_private* class_ctx) const {
        QoreMemberInfo* info;
        if (class_ctx) {
            info = class_ctx->members.find(mem);
            if (info && info->isLocalInternal()) {
                return info;
            }
        } else {
            info = nullptr;
        }

        if (class_ctx != this) {
            info = members.find(mem);
        }

        if (info && info->access == Inaccessible && !info->getClassContext(class_ctx)) {
            info = nullptr;
        }

        return info;
    }

    DLLLOCAL const QoreMemberInfo* parseFindMember(const char* mem, const qore_class_private*& qc, ClassAccess& access) const {
        access = Public;
        const_cast<qore_class_private*>(this)->initialize();
        return parseFindMemberNoInit(mem, qc, access, true);
    }

    // returns the member if it's defined and reachable from the class and cannot be declared again in the top-level class
    /** if the member is reachable through private inheritance and can be declared again, then nullptr is returned
    */
    DLLLOCAL const QoreMemberInfo* parseFindMemberNoInit(const char* mem, const qore_class_private*& qc, ClassAccess& access, bool toplevel) const {
        const QoreMemberInfo* mi = members.find(mem);
        if (mi) {
            ClassAccess ma = mi->getAccess();
            if (toplevel || ma < Internal) {
                if (access < ma) {
                    access = ma;
                }
                qc = mi->getClass();
                return mi;
            }
        }

        return scl ? scl->parseFindMember(mem, qc, access, true) : 0;
    }

    DLLLOCAL const QoreVarInfo* parseFindVar(const char* vname, const qore_class_private*& qc, ClassAccess& access, bool toplevel) const {
        //printd(5, "parseFindVar() this: %p cls: %p (%s) scl: %p\n", this, cls, cls->getName(), scl);

        QoreVarInfo* vi = vars.find(const_cast<char*>(vname));

        if (vi) {
            qc = this;
            access = vi->getAccess();
            return vi;
        }

        return scl ? scl->parseFindVar(vname, qc, access, toplevel) : nullptr;
    }

    DLLLOCAL int parseCheckClassHierarchyMembers(const char* mname, const QoreMemberInfo& l_mi, const QoreMemberInfo& b_mi) const;

    DLLLOCAL int checkExistingVarMember(const char* dname, const QoreMemberInfoBaseAccess* mi, const QoreMemberInfoBaseAccess* omi, const qore_class_private* qc, ClassAccess oaccess, bool var = false) const;

    DLLLOCAL int parseCheckVar(const char* dname, const QoreVarInfo* vi) const {
        const qore_class_private* qc = 0;
        ClassAccess access;
        const QoreVarInfo* ovi = parseFindVar(dname, qc, access, true);
        //printd(5, "parseCheckVar() %s cls: %p (%s)\n", dname, sclass, sclass ? sclass->getName() : "n/a");
        if (!ovi) {
            if (parseHasConstant(dname)) {
                parse_error(*vi->loc, "'%s' has already been declared as a constant in class '%s' and therefore cannot be also declared as a static class variable in the same class with the same name", dname, name.c_str());
                return -1;
            }
            return 0;
        }

        return checkExistingVarMember(dname, vi, ovi, qc, access, true);
    }

    DLLLOCAL int parseCheckMember(const char* mem, const QoreMemberInfo* mi) const {
        const qore_class_private* qc = nullptr;
        ClassAccess access = Public;
        const QoreMemberInfo* omi = parseFindMemberNoInit(mem, qc, access, true);
        if (!omi) {
            return 0;
        }

        return checkExistingVarMember(mem, mi, omi, qc, omi->access);
    }

    DLLLOCAL int parseCheckMemberInBaseClasses(const char* mem, const QoreMemberInfo* mi) const {
        const qore_class_private* qc = nullptr;
        ClassAccess access = Public;
        // issue #2970: do not check classes inherited by direct parents with private:internal inheritance
        // as these members cannot cause a conflict
        const QoreMemberInfo* omi = scl ? scl->parseFindMember(mem, qc, access, false) : nullptr;
        if (!omi || (omi->getClass() == mi->getClass())) {
            return 0;
        }

        return checkExistingVarMember(mem, mi, omi, qc, omi->access);
    }

    DLLLOCAL int parseCheckSystemCommitted(const QoreProgramLocation* loc) {
        if (sys) {
            parse_error(*loc, "cannot modify system class '%s'", name.c_str());
            return -1;
        }
        if (committed) {
            parse_error(*loc, "cannot modify user class '%s' once it's been committed", name.c_str());
            return -1;
        }
        return 0;
    }

    DLLLOCAL void parseAddMember(char* mem, ClassAccess access, QoreMemberInfo* memberInfo) {
        memberInfo->access = access;
        if (!parseCheckSystemCommitted(memberInfo->loc) && !parseCheckMember(mem, memberInfo)) {
            if (!has_new_user_changes) {
                has_new_user_changes = true;
            }
            if (!has_sig_changes) {
                has_sig_changes = true;
            }
            memberInfo->setDeclaringClass(this);
            if (!has_transient_member && memberInfo->getTransient()) {
                has_transient_member = true;
            }
            //printd(5, "qore_class_private::parseAddMember() this: %p %s adding %s %p %s\n", this, name.c_str(), privpub(access), mem, mem);
            members.addNoCheck(mem, memberInfo);
            return;
        }

        free(mem);
        delete memberInfo;
    }

    DLLLOCAL void parseAddStaticVar(char* dname, ClassAccess access, QoreVarInfo* VarInfo) {
        VarInfo->access = access;
        if (!parseCheckSystemCommitted(VarInfo->loc) && !parseCheckVar(dname, VarInfo)) {
            if (!has_new_user_changes) {
                has_new_user_changes = true;
            }
            if (!has_sig_changes) {
                has_sig_changes = true;
            }

            //printd(5, "qore_class_private::parseAddStaticVar() this: %p %s adding %p %s\n", this, name.c_str(), mem, mem);
            vars.addNoCheck(dname, VarInfo);
            return;
        }

        free(dname);
        delete VarInfo;
    }

    DLLLOCAL void addBuiltinConstant(const char* cname, QoreValue value, ClassAccess access = Public, const QoreTypeInfo* cTypeInfo = nullptr) {
        assert(!constlist.inList(cname));
        if (!sys) {
            sys = committed = true;
        }
        constlist.add(cname, value, cTypeInfo, access);
    }

    DLLLOCAL void addBuiltinStaticVar(const char* vname, QoreValue value, ClassAccess access = Public, const QoreTypeInfo* vTypeInfo = nullptr);

    DLLLOCAL void parseAssimilateConstants(ConstantList &cmap, ClassAccess access) {
        assert(!sys && !committed);
        if (!has_new_user_changes)
            has_new_user_changes = true;
        if (!has_sig_changes)
            has_sig_changes = true;

        // set access if necessary
        cmap.setAccess(access);
        constlist.assimilate(cmap, "class", name.c_str());
    }

    DLLLOCAL void parseAddConstant(const QoreProgramLocation* loc, const std::string &cname, QoreValue val, ClassAccess access) {
        ValueHolder val_holder(val, nullptr);
        if (parseCheckSystemCommitted(loc)) {
            return;
        }
        if (parseHasVar(cname.c_str())) {
            parse_error(*loc, "'%s' has already been declared as a static variable in class '%s' and therefore cannot be also declared as a constant in the same class with the same name", cname.c_str(), name.c_str());
            return;
        }
        if (!has_new_user_changes)
            has_new_user_changes = true;
        if (!has_sig_changes)
            has_sig_changes = true;

        //printd(5, "parseAddConstant() this: %p cls: %p const: %s access: %d\n", this, cls, cname.c_str(), access);

        constlist.parseAdd(loc, cname, val_holder.release(), access, name.c_str());
    }

    DLLLOCAL bool parseHasVar(const char* vn) {
        return vars.inList(vn);
    }

    DLLLOCAL bool parseHasConstant(const std::string &cname) const {
        return constlist.inList(cname);
    }

    DLLLOCAL QoreValue parseFindLocalConstantValue(const char* cname, const QoreTypeInfo*& cTypeInfo, bool& found) {
        parseInitPartial();

        // first check committed constants
        ClassAccess access = Public;
        QoreValue rv = constlist.find(cname, cTypeInfo, access, found);

        // check for accessibility to private constants
        if (found && (access > Public)) {
            qore_class_private* class_ctx = parse_get_class_priv();
            if ((access >= Internal && class_ctx != this) || !parseCheckPrivateClassAccess(class_ctx)) {
                rv.clear();
                cTypeInfo = nullptr;
                found = false;
            }
        }

        //printd(5, "qore_class_private::parseFindLocalConstantValue(%s) this: %p (cls: %p %s) rv: %p\n", cname, this,
        //  cls, name.c_str(), rv);
        return rv;
    }

    DLLLOCAL QoreValue parseFindConstantValue(const char* cname, const QoreTypeInfo*& cTypeInfo, bool& found,
            const qore_class_private* class_ctx) {
        found = false;
        return parseFindConstantValueIntern(cname, cTypeInfo, found, class_ctx);
    }

    DLLLOCAL QoreValue parseFindConstantValueIntern(const char* cname, const QoreTypeInfo*& cTypeInfo, bool& found,
            const qore_class_private* class_ctx) {
        parseInitPartial();

        // check constant list
        ClassAccess access = Public;
        QoreValue rv = constlist.find(cname, cTypeInfo, access, found);

        // check for accessibility to private constants
        if (found) {
            if (access == Internal) {
                if (class_ctx == this)
                    return rv;
                else {
                    cTypeInfo = nullptr;
                    found = false;
                }
            } else if (access == Private && !parseCheckPrivateClassAccess(class_ctx)) {
                cTypeInfo = nullptr;
                found = false;
            } else {
                return rv;
            }
        }

        return scl ? scl->parseFindConstantValue(cname, cTypeInfo, found, class_ctx, class_ctx == this) : QoreValue();
    }

    DLLLOCAL QoreVarInfo* parseFindLocalStaticVar(const char* vname) const {
        QoreVarInfo* vi = vars.find(vname);

        if (vi && (vi->access > Public) && !parseCheckPrivateClassAccess())
            vi = nullptr;

        return vi;
    }

    DLLLOCAL QoreVarInfo* parseFindStaticVar(const char* vname, const QoreClass*& qc, ClassAccess& access, bool check = false) const {
        access = Public;
        return parseFindStaticVarIntern(vname, qc, access, check, true);
    }

    DLLLOCAL QoreVarInfo* parseFindStaticVarIntern(const char* vname, const QoreClass*& qc, ClassAccess& access, bool check, bool toplevel) const {
        QoreVarInfo* vi = vars.find(vname);

        if (vi) {
            ClassAccess va = vi->getAccess();
            if (toplevel || va != Internal) {
                if (access < va) {
                    access = va;
                }

                // return null and stop searching in this class if we should verify access, and the var is not accessible
                if (check && (access > Public) && !parseCheckPrivateClassAccess()) {
                    return nullptr;
                }

                qc = cls;
                return vi;
            }
        }

        return scl ? scl->parseFindStaticVar(vname, qc, access, check, toplevel) : nullptr;
    }

    DLLLOCAL void addMember(const char* mem, ClassAccess access, const QoreTypeInfo* n_typeinfo, QoreValue initial_value) {
        assert(!members.inList(mem));
        if (!has_sig_changes) {
            has_sig_changes = true;
        }
        members.addNoCheck(strdup(mem), new QoreMemberInfo(&loc_builtin, n_typeinfo, nullptr, initial_value, access, this));
        if (access == Public && !has_public_memdecl) {
            has_public_memdecl = true;
        }
    }

    DLLLOCAL void insertBuiltinStaticMethod(QoreMethod* m) {
        assert(m->isStatic());
        //printd(5, "QoreClass::insertBuiltinStaticMethod() %s::%s() size: %d\n", name.c_str(), m->getName(), numMethods());
        shm[m->getName()] = m;
        // maintain method counts (safely inside parse lock)
        ++num_static_methods;
        if (!sys) {
            sys = committed = true;
        }
        // check for special methods (except constructor and destructor) and abort if found
        assert(!checkSpecialStaticIntern(m->getName()));
        // add ancestors
        addStaticAncestors(m);
    }

    DLLLOCAL void insertBuiltinMethod(QoreMethod* m, bool special_method = false) {
        assert(!m->isStatic());
        //printd(5, "QoreClass::insertBuiltinMethod() %s::%s() size: %d\n", name.c_str(), m->getName(), numMethods());
        hm[m->getName()] = m;
        // maintain method counts (safely inside parse lock)
        ++num_methods;
        if (!sys) {
            sys = committed = true;
        }
        // check for special methods (except constructor and destructor)
        if (!special_method && !checkAssignSpecialIntern(m))
            // add ancestors
            addAncestors(m);
    }

    DLLLOCAL void recheckBuiltinMethodHierarchy();

    DLLLOCAL void addNewAncestors(QoreMethod* m) {
        if (!scl) {
            return;
        }

        scl->addNewAncestors(m);
    }

    DLLLOCAL void addNewStaticAncestors(QoreMethod* m) {
        if (!scl) {
            return;
        }

        scl->addNewStaticAncestors(m);
    }

    DLLLOCAL void addStaticAncestors(QoreMethod* m) {
        if (!scl) {
            return;
        }

        scl->addStaticAncestors(m);
    }

    DLLLOCAL void addAncestors(QoreMethod* m) {
        assert(strcmp(m->getName(), "constructor"));

        if (!scl) {
            return;
        }

        scl->addAncestors(m);
    }

    DLLLOCAL void parseAddStaticAncestors(QoreMethod* m) {
        if (!scl) {
            return;
        }

        scl->parseAddStaticAncestors(m);
    }

    DLLLOCAL void parseAddAncestors(QoreMethod* m) {
        //printd(5, "qore_class_private::parseAddAncestors(%p %s) this: %p cls: %p %s scl: %p\n", m, m->getName(), this, cls, name.c_str(), scl);
        assert(strcmp(m->getName(), "constructor"));

        if (!scl)
            return;

        scl->parseAddAncestors(m);
    }

    DLLLOCAL int initMembers(QoreObject& o, bool& need_scan, ExceptionSink* xsink) const;

    DLLLOCAL int initMember(QoreObject& o, bool& need_scan, const char* member_name, const QoreMemberInfo& info, const qore_class_private* member_class_ctx, ExceptionSink* xsink) const;

    DLLLOCAL void clearConstants(QoreListNode& l) {
        if (const_refs.ROdereference()) {
            constlist.clear(l);
        }
    }

    DLLLOCAL void clearConstants(ExceptionSink* xsink) {
        if (const_refs.ROdereference()) {
            constlist.deleteAll(xsink);
        }
    }

    DLLLOCAL void clear(ExceptionSink* xsink) {
        //printd(5, "qore_class_private::clear() this: %p '%s' %d -> %d\n", this, name.c_str(), var_refs.reference_count(), var_refs.reference_count() - 1);

        if (var_refs.ROdereference()) {
            // issue #3521: only clear vars here; do not delete
            vars.clear(xsink);
        }
    }

    DLLLOCAL void deleteClassData(bool deref_vars, ExceptionSink* xsink) {
        if (deref_vars && var_refs.ROdereference()) {
            vars.clear(xsink);
            vars.del(xsink);
        } else if (!var_refs.reference_count()) {
            // delete vars again if possible
            vars.del(xsink);
        }

        // delete key-value data
        for (auto& i : kvmap) {
            i.second.discard(xsink);
        }
        kvmap.clear();

        /*
        if (!const_refs.reference_count()) {
            constlist.deleteAll(xsink);
        }
        */
        if (spgm) {
            if (deref_source_program) {
                spgm->deref(xsink);
            }
            spgm = nullptr;
        }
    }

    /*
    DLLLOCAL int initMembers(QoreObject& o, BCEAList* bceal, ExceptionSink* xsink) const {
        if (scl && scl->initMembers(o, bceal, xsink))
            return -1;

        if (members.empty())
            return 0;

        // ensure class is only initialized once
        if (bceal && bceal->initMembers(cls->getID()))
            return 0;

        SelfInstantiatorHelper sih(&selfid, &o, xsink);

        if (initMembers(o, members.begin(), members.end(), xsink))
            return -1;
        return 0;
    }
    */

    DLLLOCAL const QoreMethod* getMethodForEval(const char* nme, QoreProgram* pgm,
        const qore_class_private* class_ctx, ExceptionSink* xsink) const;

    // allow_abstract = true if only for other languages that have inherited this class and have implemented
    // the abstract methods
    DLLLOCAL QoreObject* execConstructor(ExceptionSink* xsink, const AbstractQoreFunctionVariant* variant,
        const QoreListNode* args, const QoreClass* obj_cls = nullptr, bool allow_abstract = false) const;

    DLLLOCAL void addBuiltinMethod(const char* mname, MethodVariantBase* variant);
    DLLLOCAL void addBuiltinStaticMethod(const char* mname, MethodVariantBase* variant);
    DLLLOCAL void addBuiltinConstructor(BuiltinConstructorVariantBase* variant);
    DLLLOCAL void addBuiltinDestructor(BuiltinDestructorVariantBase* variant);
    DLLLOCAL void addBuiltinCopyMethod(BuiltinCopyVariantBase* variant);
    DLLLOCAL void setBuiltinSystemConstructor(BuiltinSystemConstructorBase* m);

    DLLLOCAL void execBaseClassConstructor(QoreObject* self, BCEAList* bceal, ExceptionSink* xsink) const;
    DLLLOCAL QoreObject* execSystemConstructor(QoreObject* self, int code, va_list args) const;
    DLLLOCAL QoreObject* execCopy(QoreObject* old, ExceptionSink* xsink) const;

    // returns a non-static method if it exists in the local class
    DLLLOCAL QoreMethod* parseFindLocalMethod(const char* nme) {
        hm_method_t::iterator i = hm.find(nme);
        return (i != hm.end()) ? i->second : nullptr;
    }
    // returns a non-static method if it exists in the local class
    DLLLOCAL const QoreMethod* parseFindLocalMethod(const char* nme) const {
        hm_method_t::const_iterator i = hm.find(nme);
        return (i != hm.end()) ? i->second : nullptr;
    }

    DLLLOCAL QoreMethod* parseFindLocalMethod(const std::string& nme) {
        hm_method_t::iterator i = hm.find(nme);
        return (i != hm.end()) ? i->second : nullptr;
    }
    // returns a non-static method if it exists in the local class
    DLLLOCAL const QoreMethod* parseFindLocalMethod(const std::string& nme) const {
        hm_method_t::const_iterator i = hm.find(nme);
        return (i != hm.end()) ? i->second : nullptr;
    }

    // returns any method if it exists in the local class
    DLLLOCAL const QoreMethod* parseFindAnyLocalMethod(const char* nme) const {
        const QoreMethod* m = parseFindLocalMethod(nme);
        return m ? m : parseFindLocalStaticMethod(nme);
    }

    // returns a static method if it exists in the local class
    DLLLOCAL QoreMethod* parseFindLocalStaticMethod(const char* nme) {
        hm_method_t::iterator i = shm.find(nme);
        return (i != shm.end()) ? i->second : nullptr;
    }
    // returns a static method if it exists in the local class
    DLLLOCAL const QoreMethod* parseFindLocalStaticMethod(const char* nme) const {
        hm_method_t::const_iterator i = shm.find(nme);
        return (i != shm.end()) ? i->second : nullptr;
    }

    DLLLOCAL const QoreMethod* parseGetConstructor() const {
        const_cast<qore_class_private*>(this)->initialize();
        if (constructor) {
            return constructor;
        }
        return parseFindLocalMethod("constructor");
    }

    DLLLOCAL void unsetPublicMemberFlag() {
        assert(has_public_memdecl);
        has_public_memdecl = false;
    }

    // This function must only be called from QoreObject
    DLLLOCAL void execMemberNotification(QoreObject* self, const char* mem, ExceptionSink* xsink) const;

    // returns a non-static method if it exists in the local class and has been committed to the class
    DLLLOCAL QoreMethod* findLocalCommittedMethod(const char* nme);
    // returns a non-static method if it exists in the local class and has been committed to the class
    DLLLOCAL const QoreMethod* findLocalCommittedMethod(const char* nme) const;

    // returns a static method if it exists in the local class and has been committed to the class
    DLLLOCAL QoreMethod* findLocalCommittedStaticMethod(const char* nme);
    // returns a static method if it exists in the local class and has been committed to the class
    DLLLOCAL const QoreMethod* findLocalCommittedStaticMethod(const char* nme) const;

    DLLLOCAL void finalizeBuiltin(const char* nspath);
    DLLLOCAL void generateBuiltinSignature(const char* nspath);
    DLLLOCAL void initializeBuiltin();

    DLLLOCAL QoreValue evalMethod(QoreObject* self, const char* nme, const QoreListNode* args, const qore_class_private* class_ctx, ExceptionSink* xsink) const;

    DLLLOCAL QoreValue evalMethodGate(QoreObject* self, const char* nme, const QoreListNode* args, ExceptionSink* xsink) const;

    DLLLOCAL static const QoreMethod* doParseMethodAccess(const QoreMethod* m, const qore_class_private* class_ctx);

    DLLLOCAL static const QoreMethod* doMethodAccess(const QoreMethod* m, ClassAccess ma, const qore_class_private* class_ctx) {
        assert(m);
        return ((ma == Public) || ((ma == Private && class_ctx))) ? m : nullptr;
    }

    DLLLOCAL static const QoreMethod* doMethodAccess(const QoreMethod* m, ClassAccess& access, ClassAccess ma) {
        assert(m);

        if (ma >= Internal)
            m = nullptr;
        else if (access < ma)
            access = ma;

        return m;
    }

    DLLLOCAL const QoreMethod* doRuntimeMethodAccess(const QoreMethod* m, ClassAccess& access, ClassAccess ma, const qore_class_private* class_ctx) const {
        assert(m);

        if (ma >= Internal && (!class_ctx || !equal(*class_ctx)))
            m = nullptr;
        else if (access < ma)
            access = ma;

        return m;
    }

    // performs class initialization; find a static method in the class hierarchy at parse time; inaccessible methods are ignored
    DLLLOCAL const QoreMethod* parseFindNormalMethod(const char* mname, const qore_class_private* class_ctx);

    // performs class initialization; find a static method in the class hierarchy at parse time; inaccessible methods are ignored
    DLLLOCAL const QoreMethod* parseFindStaticMethod(const char* mname, const qore_class_private* class_ctx);

    // performs class initialization; finds a non-static method in the class hierarchy at parse time, optionally initializes classes
    DLLLOCAL const QoreMethod* parseFindNormalMethodIntern(const char* mname, const qore_class_private* class_ctx);

    // performs class initialization; finds a static method in the class hierarchy at parse time, optionally initializes classes
    DLLLOCAL const QoreMethod* parseFindStaticMethodIntern(const char* mname, const qore_class_private* class_ctx);

    DLLLOCAL const QoreMethod* parseResolveSelfMethodIntern(const QoreProgramLocation* loc, const char* nme,
            const qore_class_private* class_ctx);

    // returns a non-static method if it exists in class hierarchy and has been committed to the class
    // class_ctx is only set if it is present and accessible, so we only need to check for internal access here
    DLLLOCAL const QoreMethod* runtimeFindCommittedStaticMethodIntern(const char* nme, ClassAccess& access,
            const qore_class_private* class_ctx) const {
        const QoreMethod* m = findLocalCommittedStaticMethod(nme);
        if (m &&
            (class_ctx == this || doRuntimeMethodAccess(m, access, m->getAccess(), class_ctx))) {
            return m;
        }
        if (!scl) {
            return nullptr;
        }
        // access already checked in subclasses, do not need to check again
        return scl->runtimeFindCommittedStaticMethod(nme, access, class_ctx, class_ctx == this);
    }

    // returns a non-static method if it exists in class hierarchy and has been committed to the class
    // class_ctx is only set if it is present and accessible, so we only need to check for internal access here
    DLLLOCAL const QoreMethod* runtimeFindCommittedMethodIntern(const char* nme, ClassAccess& access,
            const qore_class_private* class_ctx) const {
        const QoreMethod* m = findLocalCommittedMethod(nme);
        //printd(5, "qore_class_private::runtimeFindCommittedMethodIntern(%s) '%s' class_ctx: %p '%s' FIRST m: %p\n",
        //    nme, name.c_str(), class_ctx, class_ctx ? class_ctx.name.c_str() : "n/a", m);
        if (m &&
            (class_ctx == this || doRuntimeMethodAccess(m, access, m->getAccess(), class_ctx))) {
            return m;
        }
        if (!scl) {
            return nullptr;
        }
        // access already checked in subclasses, do not need to check again
        return scl->runtimeFindCommittedMethod(nme, access, class_ctx, class_ctx == this);
    }

    DLLLOCAL const QoreMethod* runtimeFindCommittedStaticMethod(const char* nme, ClassAccess& access,
        const qore_class_private* class_ctx) const;

    DLLLOCAL const QoreMethod* runtimeFindCommittedMethod(const char* nme, ClassAccess& access,
        const qore_class_private* class_ctx) const;

    DLLLOCAL const QoreMethod* runtimeFindCommittedMethodForEval(const char* nme, ClassAccess& access,
        const qore_class_private* class_ctx) const;

    DLLLOCAL const QoreMethod* runtimeFindAnyCommittedMethod(const char* nme) const {
        ClassAccess access = Public;
        const qore_class_private* class_ctx = this;
        const QoreMethod* m = runtimeFindCommittedMethodIntern(nme, access, class_ctx);
        if (!m) {
            // check for special methods
            if (!strcmp(nme, "constructor"))
                return constructor;
            if (!strcmp(nme, "destructor"))
                return destructor;
            if (!strcmp(nme, "copy"))
                return copyMethod;
            if (!strcmp(nme, "methodGate"))
                return copyMethod;
            if (!strcmp(nme, "memberGate"))
                return memberGate;
            if (!strcmp(nme, "memberNotification"))
                return memberNotification;
        }
        return m;
    }

    DLLLOCAL const QoreMethod* findMethod(const char* nme, ClassAccess& access) const {
        CurrentProgramRuntimeParseContextHelper pch;
        const qore_class_private* class_ctx = runtime_get_class();
        if (class_ctx && !runtimeCheckPrivateClassAccess(class_ctx))
            class_ctx = nullptr;
        return runtimeFindCommittedMethod(nme, access, class_ctx);
    }

    DLLLOCAL bool runtimeHasCallableMethod(const char* m, int mask) const;

    DLLLOCAL void execDestructor(QoreObject* self, ExceptionSink* xsink) const;

    DLLLOCAL void execBaseClassDestructor(QoreObject* self, ExceptionSink* xsink) const;

    DLLLOCAL void execBaseClassSystemDestructor(QoreObject* self, ExceptionSink* xsink) const;

    DLLLOCAL void execBaseClassCopy(QoreObject* self, QoreObject* old, ExceptionSink* xsink) const;

    DLLLOCAL int parseInit();
    DLLLOCAL int parseResolveHierarchy();
    DLLLOCAL void parseResolveClassMembers();
    DLLLOCAL void parseResolveAbstract();
    DLLLOCAL void parseCommit();
    DLLLOCAL void parseCommitRuntimeInit(ExceptionSink* xsink);
    DLLLOCAL void parseRollback();
    DLLLOCAL int addUserMethod(const char* mname, MethodVariantBase* f, bool n_static);
    DLLLOCAL void addLocalMembersForInit();

    DLLLOCAL QoreValue evalPseudoMethod(const QoreValue n, const char* name, const QoreListNode* args,
            ExceptionSink* xsink) const;

    DLLLOCAL QoreValue evalPseudoMethod(const QoreMethod* m, const AbstractQoreFunctionVariant* variant,
            const QoreValue n, const QoreListNode* args, ExceptionSink* xsink) const;

    DLLLOCAL const QoreMethod* runtimeFindPseudoMethod(const QoreValue n, const char* nme,
            ExceptionSink* xsink) const {
        const QoreMethod* w;

        const qore_class_private* class_ctx = runtime_get_class();
        if (class_ctx && !runtimeCheckPrivateClassAccess(class_ctx)) {
            class_ctx = nullptr;
        }
        ClassAccess access;
        if (!(w = runtimeFindCommittedMethod(nme, access, class_ctx))) {
            qore_type_t t = n.getType();
            // throw an exception
            if (t == NT_OBJECT) {
                const char* cname = n.get<const QoreObject>()->getClassName();
                xsink->raiseException("METHOD-DOES-NOT-EXIST", "no method %s::%s() or pseudo-method %s::%s() is " \
                    "available", cname, nme, name.c_str(), nme);
            } else {
                xsink->raiseException("PSEUDO-METHOD-DOES-NOT-EXIST", "no pseudo method <%s>::%s() has been defined",
                    n.getTypeName(), nme);
            }
            return nullptr;
        }

        return w;
    }

    DLLLOCAL bool parseCheckPrivateClassAccess(const qore_class_private* qc = parse_get_class_priv()) const;
    DLLLOCAL bool runtimeCheckPrivateClassAccess(const qore_class_private* qc = runtime_get_class()) const;

    // this = class to find in "oc"
    DLLLOCAL qore_type_result_e parseCheckCompatibleClass(const qore_class_private& oc) const {
        bool may_not_match = false;
        qore_type_result_e rv = parseCheckCompatibleClass(oc, may_not_match);
        // if the type may not match at runtime, then return no match with %strict-types
        if (may_not_match && (getProgram()->getParseOptions64() & PO_STRICT_TYPES)) {
            return QTI_NOT_EQUAL;
        }
        return rv;
    }
    DLLLOCAL qore_type_result_e parseCheckCompatibleClass(const qore_class_private& oc, bool& may_not_match) const;
    DLLLOCAL qore_type_result_e parseCheckCompatibleClassIntern(const qore_class_private& oc, bool& may_not_match) const;
    // this = class to find in "oc"
    DLLLOCAL qore_type_result_e runtimeCheckCompatibleClass(const qore_class_private& oc) const;
    DLLLOCAL qore_type_result_e runtimeCheckCompatibleClassIntern(const qore_class_private& oc) const;

    // find the given class anywhere in the hierarchy regardless of access permissions or location
    DLLLOCAL const QoreClass* findInHierarchy(const qore_class_private& qc) {
        if (equal(qc))
            return cls;
        return scl ? scl->findInHierarchy(qc) : nullptr;
    }

    DLLLOCAL const QoreClass* getClassIntern(qore_classid_t cid, ClassAccess& n_access, bool toplevel) const {
        if (cid == classID)
            return cls;
        return scl ? scl->getClass(cid, n_access, toplevel) : nullptr;
    }

    DLLLOCAL const QoreClass* getClass(const qore_class_private& qc, ClassAccess& n_access) const {
        n_access = Public;
        return getClassIntern(qc, n_access, true);
    }

    DLLLOCAL const QoreClass* getClassIntern(const qore_class_private& qc, ClassAccess& n_access,
            bool toplevel) const {
        if (equal(qc)) {
            return cls;
        }

#ifdef DEBUG_1
        if (qc.name == name) {
            QoreString lh, rh;
            hash.toString(lh);
            qc.hash.toString(rh);
            printd(5, "qore_class_private::getClassIntern() this: %p '%s' != '%s' scl: %p (hash: %s qc.hash: %s)\n",
                this, name.c_str(), qc.name.c_str(), scl, lh.getBuffer(), rh.getBuffer());
        }
#endif

        return scl ? scl->getClass(qc, n_access, toplevel) : nullptr;
    }

    DLLLOCAL const QoreClass* parseGetClassIntern(const qore_class_private& qc, ClassAccess& n_access,
            bool toplevel) const {
        // check hashes if names are the same
        // FIXME: check fully-qualified namespace name
        if (parseEqual(qc))
            return cls;

#ifdef DEBUG_SKIP
        if (qc.name == name) {
            printd(5, "qore_class_private::parseGetClassIntern() this: %p '%s' != '%s' scl: %p\n", this, name.c_str(),
                qc.name.c_str(), scl);
            parseShowHashes();
            qc.parseShowHashes();
        }
#endif

        return scl ? scl->parseGetClass(qc, n_access, toplevel) : nullptr;
    }

    DLLLOCAL bool inHierarchy(const qore_class_private& qc, ClassAccess& n_access) const {
        if (equal(qc)) {
            return cls;
        }

        return scl ? scl->inHierarchy(qc, n_access) : false;
    }

    DLLLOCAL bool inHierarchyStrict(const qore_class_private& qc, ClassAccess& n_access) const {
        if (strictEqual(qc)) {
            return cls;
        }

        return scl ? scl->inHierarchyStrict(qc, n_access) : false;
    }

#ifdef DEBUG_SKIP
    DLLLOCAL void parseShowHash() const {
        QoreString ch, ph;
        hash.toString(ch);
        printd(5, " + %p %s committed: %s\n", this, name.c_str(), ch.getBuffer());
    }
#endif

    DLLLOCAL bool parseCheckEqualHash(const qore_class_private& qc) const {
#ifdef DEBUG_SKIP
        printd(5, "qore_class_private::parseCheckEqualHash() %s == %s\n", name.c_str(), qc.name.c_str());
        parseShowHash();
        qc.parseShowHash();
#endif
        return hash == qc.hash;
    }

    DLLLOCAL bool strictEqual(const qore_class_private& qc) const {
        if (&qc == this) {
            return true;
        }

        if (qc.classID == classID || (qc.name == name && qc.hash == hash)) {
            return true;
        }

        return false;
    }

    DLLLOCAL bool equal(const qore_class_private& qc) const {
        if (&qc == this)
            return true;

        if (qc.classID == classID || (qc.name == name && qc.hash == hash))
            return true;

        if (injectedClass && injectedClass->equal(qc))
            return true;

        if (qc.injectedClass && equal(*qc.injectedClass))
            return true;

        return false;
    }

    DLLLOCAL bool parseEqual(const qore_class_private& qc) const {
        if (&qc == this)
            return true;

        if (qc.classID == classID || (qc.name == name && parseCheckEqualHash(qc)))
            return true;

        if (injectedClass && injectedClass->parseEqual(qc))
            return true;

        if (qc.injectedClass && parseEqual(*qc.injectedClass))
            return true;

        return false;
    }

    DLLLOCAL const QoreClass* parseGetClass(const qore_class_private& qc, ClassAccess& n_access) const;

    DLLLOCAL int addBaseClassesToSubclass(QoreClass* sc, bool is_virtual);

    DLLLOCAL void setPublic();

    DLLLOCAL void parseSetBaseClassList(BCList* bcl) {
        assert(!scl);
        if (bcl) {
            scl = bcl;
            if (!has_new_user_changes)
                has_new_user_changes = true;
            if (!has_sig_changes)
                has_sig_changes = true;
        }
    }

    DLLLOCAL bool parseHasPendingChanges() const {
        return has_new_user_changes;
    }

    DLLLOCAL bool parseCheckHierarchy(const QoreClass* n_cls, ClassAccess& access) const {
        access = Public;
        return parseCheckHierarchyIntern(n_cls, access, true);
    }

    DLLLOCAL bool parseCheckHierarchyIntern(const QoreClass* n_cls, ClassAccess& access, bool toplevel) const {
        if (parseEqual(*n_cls->priv))
            return true;

        return scl ? scl->parseCheckHierarchy(n_cls, access, toplevel) : false;
    }

    DLLLOCAL VRMutex* getGate() const {
        return &gate;
    }

    // class initialization; inaccessible methods are ignored
    DLLLOCAL const QoreMethod* parseFindAnyMethod(const char* nme, const qore_class_private* class_ctx);

    // class initialization; inaccessible methods are ignored
    DLLLOCAL const QoreMethod* parseFindAnyMethodStaticFirst(const char* nme, const qore_class_private* class_ctx);

    // class initialization
    DLLLOCAL const QoreMethod* parseResolveSelfMethod(const QoreProgramLocation* loc, const char* nme,
            const qore_class_private* class_ctx);
    DLLLOCAL const QoreMethod* parseResolveSelfMethod(const QoreProgramLocation* loc, NamedScope* nme);

    // class initialization
    DLLLOCAL const QoreMethod* parseFindSelfMethod(const char* nme);

    DLLLOCAL char* getHash() const {
        return hash.getHash();
    }

    DLLLOCAL void setSerializer(q_serializer_t m) {
        assert(!serializer);
        serializer = m;
    }

    DLLLOCAL void setDeserializer(q_deserializer_t m) {
        assert(!deserializer);
        deserializer = m;
    }

    // static methods
    //DLLLOCAL static

    DLLLOCAL static char* getHash(const QoreClass& qc) {
        return qc.priv->getHash();
    }

    DLLLOCAL static void parseAddConstant(QoreClass& qc, const QoreProgramLocation* loc, const std::string &cname,
            QoreValue val, ClassAccess access) {
        qc.priv->parseAddConstant(loc, cname, val, access);
    }

    DLLLOCAL static LocalVar* getSelfId(const QoreClass& qc) {
        return &qc.priv->selfid;
    }

    DLLLOCAL static QoreObject* execConstructor(const QoreClass& qc, const AbstractQoreFunctionVariant* variant,
            const QoreListNode* args, ExceptionSink* xsink) {
        return qc.priv->execConstructor(xsink, variant, args);
    }

    DLLLOCAL static bool injected(const QoreClass& qc) {
        return qc.priv->inject;
    }

    DLLLOCAL static QoreClass* makeImportClass(const QoreClass& qc, QoreProgram* spgm, const char* nme, bool inject,
            const qore_class_private* injectedClass, qore_ns_private* ns, q_setpub_t set_pub) {
        qore_class_private* priv = new qore_class_private(*qc.priv, ns, spgm, nme, inject, injectedClass, set_pub);
        //printd(5, "qore_program_private::makeImportClass() name: '%s' as '%s' inject: %d rv: %p\n", qc.getName(),
        //    priv->name.c_str(), inject, priv->cls);
        return priv->cls;
    }

    DLLLOCAL static const QoreMethod* runtimeFindCommittedStaticMethod(const QoreClass& qc, const char* nme,
            ClassAccess& access, const qore_class_private* class_ctx) {
        return qc.priv->runtimeFindCommittedStaticMethod(nme, access, class_ctx);
    }

    DLLLOCAL static const QoreMethod* parseFindLocalMethod(const QoreClass& qc, const char* mname) {
        return qc.priv->parseFindLocalMethod(mname);
    }

    DLLLOCAL static bool parseHasPendingChanges(const QoreClass& qc) {
        return qc.priv->parseHasPendingChanges();
    }

    DLLLOCAL static int parseCheckMemberAccess(const QoreClass& qc, const QoreProgramLocation* loc, const char* mem,
            const QoreTypeInfo*& memberTypeInfo, int pflag) {
        return qc.priv->parseCheckMemberAccess(loc, mem, memberTypeInfo, pflag);
    }

    DLLLOCAL static bool runtimeHasCallableMethod(const QoreClass& qc, const char* m) {
        return qc.priv->runtimeHasCallableMethod(m, QCCM_NORMAL | QCCM_STATIC);
    }

    DLLLOCAL static bool runtimeHasCallableNormalMethod(const QoreClass& qc, const char* m) {
        return qc.priv->runtimeHasCallableMethod(m, QCCM_NORMAL);
    }

    DLLLOCAL static bool runtimeHasCallableStaticMethod(const QoreClass& qc, const char* m) {
        return qc.priv->runtimeHasCallableMethod(m, QCCM_STATIC);
    }

    DLLLOCAL static int runtimeCheckInstantiateClass(const QoreClass& qc, ExceptionSink* xsink) {
        return qc.priv->runtimeCheckInstantiateClass(xsink);
    }

    DLLLOCAL static int parseInitPartial(QoreClass& qc) {
        return qc.priv->parseInitPartial();
    }

    DLLLOCAL static void parseCommit(QoreClass& qc) {
        qc.priv->parseCommit();
    }

    DLLLOCAL static void parseCommitRuntimeInit(QoreClass& qc, ExceptionSink* xsink) {
        qc.priv->parseCommitRuntimeInit(xsink);
    }

    DLLLOCAL static void parseRollback(QoreClass& qc) {
        qc.priv->parseRollback();
    }

    DLLLOCAL static void resolveCopy(QoreClass& qc) {
        qc.priv->resolveCopy();
    }

    DLLLOCAL static int addUserMethod(QoreClass& qc, const char* mname, MethodVariantBase* f, bool n_static) {
        return qc.priv->addUserMethod(mname, f, n_static);
    }

    DLLLOCAL static void initialize(QoreClass& qc) {
        qc.priv->initialize();
    }

    DLLLOCAL static void parseSetBaseClassList(QoreClass& qc, BCList* bcl) {
        qc.priv->parseSetBaseClassList(bcl);
    }

    DLLLOCAL static BCList* getBaseClassList(const QoreClass& qc) {
        return qc.priv->scl;
    }

    DLLLOCAL static void parseAddStaticVar(QoreClass* qc, char* dname, ClassAccess access, QoreVarInfo* VarInfo) {
        qc->priv->parseAddStaticVar(dname, access, VarInfo);
    }

    // searches only the current class, returns 0 if private found and not accessible in the current parse context
    DLLLOCAL static QoreValue parseFindLocalConstantValue(QoreClass* qc, const char* cname,
            const QoreTypeInfo*& typeInfo, bool& found) {
        return qc->priv->parseFindLocalConstantValue(cname, typeInfo, found);
    }

    // searches only the current class, returns 0 if private found and not accessible in the current parse context
    DLLLOCAL static QoreVarInfo* parseFindLocalStaticVar(const QoreClass* qc, const char* vname) {
        return qc->priv->parseFindLocalStaticVar(vname);
    }

    // searches this class and all superclasses
    DLLLOCAL static QoreValue parseFindConstantValue(QoreClass* qc, const char* cname, const QoreTypeInfo*& typeInfo,
            bool& found, const qore_class_private* class_ctx) {
        return qc->priv->parseFindConstantValue(cname, typeInfo, found, class_ctx);
    }

    // searches this class and all superclasses, if check = false, then assumes parsing from within the class
    // (parse_get_class() == this class)
    DLLLOCAL static QoreVarInfo* parseFindStaticVar(const QoreClass* qc, const char* vname, const QoreClass*& nqc,
            ClassAccess& access, bool check = false) {
        return qc->priv->parseFindStaticVar(vname, nqc, access, check);
    }

    DLLLOCAL static int parseCheckInternalMemberAccess(const QoreClass* qc, const char* mem,
            const QoreTypeInfo*& memberTypeInfo, const QoreProgramLocation* loc) {
        return qc->priv->parseCheckInternalMemberAccess(mem, memberTypeInfo, loc);
    }

    DLLLOCAL static int parseResolveInternalMemberAccess(const QoreClass* qc, const char* mem,
            const QoreTypeInfo*& memberTypeInfo) {
        return qc->priv->parseResolveInternalMemberAccess(mem, memberTypeInfo);
    }

    DLLLOCAL static const QoreMethod* parseFindSelfMethod(QoreClass* qc, const char* mname) {
        return qc->priv->parseFindSelfMethod(mname);
    }

    DLLLOCAL static void parseAddMember(QoreClass& qc, char* nme, ClassAccess access, QoreMemberInfo* mInfo) {
        qc.priv->parseAddMember(nme, access, mInfo);
    }

    DLLLOCAL static QoreValue evalPseudoMethod(const QoreClass* qc, const QoreValue n, const char* name,
            const QoreListNode* args, ExceptionSink* xsink) {
        return qc->priv->evalPseudoMethod(n, name, args, xsink);
    }

    DLLLOCAL static QoreValue evalPseudoMethod(const QoreClass* qc, const QoreMethod* m,
            const AbstractQoreFunctionVariant* variant, const QoreValue n, const QoreListNode* args,
            ExceptionSink* xsink) {
        return qc->priv->evalPseudoMethod(m, variant, n, args, xsink);
    }

    DLLLOCAL static bool parseCheckPrivateClassAccess(const QoreClass& qc,
            const qore_class_private* oqc = parse_get_class_priv()) {
        return qc.priv->parseCheckPrivateClassAccess(oqc);
    }

    DLLLOCAL static bool runtimeCheckPrivateClassAccess(const QoreClass& qc,
            const qore_class_private* oqc = runtime_get_class()) {
        return qc.priv->runtimeCheckPrivateClassAccess(oqc);
    }

    DLLLOCAL static qore_type_result_e parseCheckCompatibleClass(const QoreClass* qc, const QoreClass* oc) {
        if (!oc)
            return QTI_NOT_EQUAL;
        return qc->priv->parseCheckCompatibleClass(*(oc->priv));
    }

    DLLLOCAL static qore_type_result_e runtimeCheckCompatibleClass(const QoreClass& qc, const QoreClass& oc) {
        return qc.priv->runtimeCheckCompatibleClass(*oc.priv);
    }

    DLLLOCAL static qore_class_private* get(QoreClass& qc) {
        return qc.priv;
    }

    DLLLOCAL static const qore_class_private* get(const QoreClass& qc) {
        return qc.priv;
    }

    DLLLOCAL static bool isPublic(const QoreClass& qc) {
        return qc.priv->pub;
    }

    DLLLOCAL static bool isUserPublic(const QoreClass& qc) {
        return qc.priv->pub && !qc.priv->sys;
    }

    DLLLOCAL static bool isFinal(const QoreClass& qc) {
        return qc.priv->final;
    }

    DLLLOCAL static void setPublic(QoreClass& qc) {
        qc.priv->setPublic();
    }

    DLLLOCAL static void setFinal(QoreClass& qc) {
        assert(!qc.priv->final);
        qc.priv->final = true;
        //printd(5, "class '%s' set to final\n", qc.getName());
    }

protected:
        DLLLOCAL ~qore_class_private();
};

class qore_class_private_holder {
    qore_class_private* c;

public:
    DLLLOCAL qore_class_private_holder(QoreClass* n_c) : c(qore_class_private::get(*n_c)) {
    }

    DLLLOCAL qore_class_private_holder(qore_class_private* n_c) : c(n_c) {
    }

    DLLLOCAL ~qore_class_private_holder() {
        if (c) {
            c->deref(true, true);
        }
    }

    DLLLOCAL qore_class_private* operator*() {
        return c;
    }

    DLLLOCAL QoreClass* release() {
        if (c) {
            QoreClass* rv = c->cls;
            c = nullptr;
            return rv;
        }
        return nullptr;
    }
};

class qore_method_private {
public:
    const QoreClass* parent_class;
    MethodFunctionBase* func;
    bool static_flag, all_user;

    DLLLOCAL qore_method_private(const QoreClass* n_parent_class, MethodFunctionBase* n_func, bool n_static)
            : parent_class(n_parent_class), func(n_func), static_flag(n_static), all_user(true) {
        assert(parent_class == n_func->getClass());
    }

    DLLLOCAL ~qore_method_private() {
        func->deref();
    }

    DLLLOCAL void setBuiltin() {
        if (all_user)
            all_user = false;
    }

    DLLLOCAL bool isUniquelyUser() const {
        return all_user;
    }

    DLLLOCAL bool isAbstract() const {
        return func->isAbstract();
    }

    DLLLOCAL int addUserVariant(MethodVariantBase* variant) {
        return func->parseAddUserMethodVariant(variant);
    }

    DLLLOCAL void addBuiltinVariant(MethodVariantBase* variant) {
        setBuiltin();
        func->addBuiltinMethodVariant(variant);
    }

    DLLLOCAL MethodFunctionBase* getFunction() const {
        return const_cast<MethodFunctionBase*>(func);
    }

    DLLLOCAL const char* getName() const {
        return func->getName();
    }

    DLLLOCAL const std::string& getNameStr() const {
        return func->getNameStr();
    }

    DLLLOCAL int parseInit();

    DLLLOCAL int parseInitStatic() {
        assert(static_flag);
        int err = func->parseInit();
        // make sure the method doesn't override a "final" method in a base class
        if (func->checkFinal() && !err) {
            err = -1;
        }
        return err;
    }

    DLLLOCAL const QoreTypeInfo* getUniqueReturnTypeInfo() const {
        return func->getUniqueReturnTypeInfo();
    }

    DLLLOCAL void evalConstructor(const AbstractQoreFunctionVariant* variant, QoreObject* self,
            const QoreListNode* args, BCEAList* bceal, ExceptionSink* xsink) {
        CONMF(func)->evalConstructor(variant, *parent_class, self, args, parent_class->priv->scl, bceal, xsink);
    }

    DLLLOCAL void evalCopy(QoreObject* self, QoreObject* old, ExceptionSink* xsink) const {
        COPYMF(func)->evalCopy(*parent_class, self, old, parent_class->priv->scl, xsink);
    }

    DLLLOCAL void evalBaseClassCopy(QoreObject* self, QoreObject* old, ExceptionSink* xsink) const {
        COPYMF(func)->evalCopy(*parent_class, self, old, nullptr, xsink);
    }

    DLLLOCAL void evalDestructor(QoreObject* self, ExceptionSink* xsink) const {
        DESMF(func)->evalDestructor(*parent_class, self, xsink);
    }

    DLLLOCAL void evalSystemDestructor(QoreObject* self, ExceptionSink* xsink) const {
        // execute function directly
        DESMF(func)->evalDestructor(*parent_class, self, xsink);
    }

    DLLLOCAL void evalSystemConstructor(QoreObject* self, int code, va_list args) const {
        BSYSCONB(func)->eval(*parent_class, self, code, args);
    }

    DLLLOCAL QoreValue eval(ExceptionSink* xsink, QoreObject* self, const QoreListNode* args,
            const qore_class_private* cctx = nullptr, QoreProgram* pgm_ctx = nullptr) const {
        if (!static_flag) {
            assert(self);
            return NMETHF(func)->evalMethod(xsink, 0, self, args, cctx, pgm_ctx);
        }
        return SMETHF(func)->evalMethod(xsink, 0, args, cctx, pgm_ctx);
    }

    DLLLOCAL QoreValue evalTmpArgs(ExceptionSink* xsink, QoreObject* self, QoreListNode* args,
            const qore_class_private* cctx = nullptr) const {
        if (!static_flag) {
            assert(self);
            return NMETHF(func)->evalMethodTmpArgs(xsink, nullptr, self, args, cctx);
        }
        return SMETHF(func)->evalMethodTmpArgs(xsink, nullptr, args, cctx);
    }

    DLLLOCAL QoreValue evalPseudoMethod(const AbstractQoreFunctionVariant* variant, const QoreValue n,
            const QoreListNode* args, ExceptionSink* xsink) const {
        //QORE_TRACE("qore_method_private::evalPseudoMethod()");

        assert(!static_flag);

        QoreValue rv = NMETHF(func)->evalPseudoMethod(xsink, variant, n, args);
        printd(5, "qore_method_private::evalPseudoMethod() %s::%s() returning type: %s\n", parent_class->getName(),
            getName(), rv.getTypeName());
        return rv;
    }

    DLLLOCAL QoreValue evalNormalVariant(QoreObject* self, const QoreExternalMethodVariant* ev,
            const QoreListNode* args, ExceptionSink* xsink) const;

    // returns the lowest access code for all variants
    DLLLOCAL ClassAccess getAccess() const;

    // returns the lowest access code for all variants including uncommitted variants
    DLLLOCAL static ClassAccess getAccess(const QoreMethod& m) {
        return m.priv->getAccess();
    }

    DLLLOCAL static QoreValue evalNormalVariant(const QoreMethod& m, ExceptionSink* xsink, QoreObject* self,
            const QoreExternalMethodVariant* ev, const QoreListNode* args) {
        return m.priv->evalNormalVariant(self, ev, args, xsink);
    }

    DLLLOCAL static QoreValue evalPseudoMethod(const QoreMethod& m, ExceptionSink* xsink,
            const AbstractQoreFunctionVariant* variant, const QoreValue n, const QoreListNode* args) {
        return m.priv->evalPseudoMethod(variant, n, args, xsink);
    }

    DLLLOCAL static QoreValue eval(const QoreMethod& m, ExceptionSink* xsink, QoreObject* self,
            const QoreListNode* args, const qore_class_private* cctx = nullptr, QoreProgram* pgm_ctx = nullptr) {
        return m.priv->eval(xsink, self, args, cctx, pgm_ctx);
    }

    DLLLOCAL static QoreValue evalTmpArgs(const QoreMethod& m, ExceptionSink* xsink, QoreObject* self,
            QoreListNode* args, const qore_class_private* cctx = nullptr) {
        return m.priv->evalTmpArgs(xsink, self, args, cctx);
    }

    DLLLOCAL static qore_method_private* get(QoreMethod& m) {
        return m.priv;
    }

    DLLLOCAL static const qore_method_private* get(const QoreMethod& m) {
        return m.priv;
    }
};

template <class T>
class PrivateIteratorBase {
public:
    DLLLOCAL PrivateIteratorBase(const T& obj) : obj(obj), i(obj.end()) {
    }

    DLLLOCAL bool next() {
        if (i == obj.end()) {
            i = obj.begin();
        } else {
            ++i;
        }
        return (i != obj.end());
    }

    //! returns true if the iterator is pointing at a valid element
    DLLLOCAL bool valid() const {
        return i != obj.end();
    }

protected:
    const T& obj;
    typename T::const_iterator i;
};

template <class T, class U>
class PrivateMemberIteratorBase : public PrivateIteratorBase<typename T::member_list_t> {
public:
    DLLLOCAL PrivateMemberIteratorBase(const typename T::member_list_t& obj)
            : PrivateIteratorBase<typename T::member_list_t>(obj) {
    }

    DLLLOCAL const U& getMember() const {
        assert(this->valid());
        return *reinterpret_cast<const U*>(this->i->second.get());
    }

    DLLLOCAL const char* getName() const {
        assert(this->valid());
        return this->i->first;
    }
};

#endif
