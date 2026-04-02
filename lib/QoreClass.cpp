/* -*- indent-tabs-mode: nil -*- */
/*
    QoreClass.cpp

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

#include <qore/Qore.h>
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/ConstantList.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/ql_crypto.h"
#include "qore/intern/QoreObjectIntern.h"
#include "qore/intern/QoreParseClass.h"
#include "qore/intern/RuntimeConfig.h"

#include <algorithm>
#include <set>
#include <vector>
#include <unordered_set>

#include <cassert>
#include <cstdlib>
#include <cstring>

// global class ID sequence
DLLLOCAL Sequence classIDSeq(1);

AbstractQoreClassUserData::~AbstractQoreClassUserData() {
}

QoreValue qore_method_private::evalNormalVariant(QoreObject* self, RuntimeConfig& rc,
        const QoreExternalMethodVariant* ev, const QoreListNode* args, ExceptionSink* xsink) const {
    const AbstractQoreFunctionVariant* variant = reinterpret_cast<const AbstractQoreFunctionVariant*>(ev);
    CodeEvaluationHelper ceh(xsink, rc, getFunction(), variant, getName(), args, self, parent_class->priv);
    if (*xsink) return QoreValue();

    return METHV_const(variant)->evalMethod(self, ceh, xsink);
}

int qore_method_private::parseInit() {
    assert(!static_flag);

    //printd(5, "qore_method_private::parseInit() this: %p %s::%s() func: %p\n", this, parent_class->getName(),
    //  func->getName(), func);
    int err = func->parseInit();

    const char* name = func->getName();
    if (strcmp(name, "constructor")
        && strcmp(name, "destructor")
        && strcmp(name, "copy")) {

        if ((!strcmp(name, "methodGate")
            || !strcmp(name, "memberGate")
            || !strcmp(name, "memberNotification"))) {

            if (!func->pendingEmpty()) {
                // ensure that there is no more than one parameter declared, and if it
                // has a type, it must be a string
                UserSignature* sig = UMV(func->first())->getUserSignature();
                const QoreTypeInfo* t = sig->getParamTypeInfo(0);
                if (!QoreTypeInfo::parseAccepts(stringTypeInfo, t)) {
                    QoreStringNode* desc = new QoreStringNode;
                    desc->sprintf("%s::%s(%s) has an invalid signature; the first argument declared as ",
                        parent_class->getName(), func->getName(), sig->getSignatureText());
                    QoreTypeInfo::getThisType(t, *desc);
                    desc->concat(" is not compatible with 'string'");
                    qore_program_private::makeParseException(getProgram(), *sig->getParseLocation(),
                        "PARSE-TYPE-ERROR", desc);
                    if (!err) {
                        err = -1;
                    }
                }
            }
        } else {
            // make sure the method doesn't override a "final" method in a base class
            if (func->checkFinal() && !err) {
                err = -1;
            }
        }
    }

    return err;
}

ClassAccess qore_method_private::getAccess() const {
    return func->getAccess();
}

void SignatureHash::set(const QoreString& str) {
    DigestHelper dh(str.c_str(), str.size());
#ifdef DEBUG
    assert(!dh.doDigest(0, EVP_sha1()));
#else
    dh.doDigest(0, EVP_sha1());
#endif
    assert(dh.size() == SH_SIZE);
    memcpy(buf, dh.c_str(), dh.size());
}

void SignatureHash::update(const QoreString& str) {
    if (!is_set) {
        set(str);
        is_set = true;
    } else {
        // make copy of old buffer
        unsigned char cbuf[SH_SIZE];
        memcpy(cbuf, buf, SH_SIZE);
        // set hash for new addition
        set(str);
        // xor old hash with new hash
        for (unsigned i = 0; i < SH_SIZE; ++i)
            buf[i] ^= cbuf[i];
    }

#ifdef DEBUG
    //QoreString dbg;
    //toString(dbg);
    //printd(5, "class hash %p set to: %s\n", this, dbg.getBuffer());
#endif
}

AbstractMethod::AbstractMethod(const AbstractMethod& old) : relaxed_match(old.relaxed_match) {
    assert(!old.vlist.empty());
    for (auto& i : old.vlist) {
        assert(vlist.find(i.first) == vlist.end());
        i.second->ref();
        vlist.insert(vmap_t::value_type(i.first, i.second));
    }
}

AbstractMethod::~AbstractMethod() {
    for (auto& i : vlist)
        i.second->deref();
    for (auto& i : pending_save)
        i.second->deref();
}

int AbstractMethod::parseCommit() {
    if (check_parse) {
        check_parse = false;
    }
    for (auto& i : pending_save) {
        i.second->deref();
    }
    pending_save.clear();
    return vlist.empty() ? -1 : 0;
}

// merge changes from parent class method of the same name during parse initialization
void AbstractMethod::parseMergeBase(AbstractMethod& m, bool committed) {
    //printd(5, "AbstractMethod::parseMergeBase(m: %p) this: %p m.pending_save: %d m.vlist: %d\n", &m, this,
    //    !m.pending_save.empty(), !m.vlist.empty());
    // move pending committed variants from our vlist that are in parent's pending_save list to our pending_save
    for (auto& i : m.pending_save) {
        const char* sig = i.second->getAbstractSignature();
        vmap_t::iterator vi = vlist.find(sig);
        if (vi != vlist.end()) {
            /*
            i.second->ref();
            pending_save.insert(vmap_t::value_type(sig, i.second));
            vi.second->deref();
            */

            pending_save.insert(vmap_t::value_type(sig, vi->second));
            vlist.erase(vi);
        }
    }

    // add new abstract methods from parent to our list - if they are not already in our vlist or in our pending_save list
    for (auto& i : m.vlist) {
        const char* sig = i.second->getAbstractSignature();
        //printd(5, "AbstractMethod::parseMergeBase(m: %p) this: %p checking parent: '%s'\n", &m, this, sig);
        if (pending_save.find(sig) != pending_save.end()) {
            continue;
        }
        if (vlist.find(sig) != vlist.end()) {
            continue;
        }
        //printd(5, "AbstractMethod::parseMergeBase(m: %p) this: %p adding to vlist from parent: '%s'\n", &m, this, sig);
        i.second->ref();
        vlist.insert(vmap_t::value_type(sig, i.second));
    }
}

// merge changes from parent class method of the same name during parse initialization
void AbstractMethod::parseMergeBase(AbstractMethod& m, MethodFunctionBase* f, bool committed) {
    //printd(5, "AbstractMethod::parseMergeBase(m: %p, f: %p %s::%s) this: %p m.pending_save: %d m.vlist: %d\n", &m,
    //    f, f ? f->getClassName() : "n/a", f ? f->getName() : "n/a", this, !m.pending_save.empty(),
    //    !m.vlist.empty());
    // move pending committed variants from our vlist that are in parent's pending_save list to our pending_save
    for (auto& i : m.pending_save) {
        const char* sig = i.second->getAbstractSignature();
        vmap_t::iterator vi = vlist.find(sig);
        if (vi != vlist.end()) {
            pending_save.insert(vmap_t::value_type(sig, vi->second));
            vlist.erase(vi);
        }
    }

    // add new abstract methods from parent to our list - if they are not already in our vlist or in our pending_save
    // list
    for (auto& i : m.vlist) {
        const char* sig = i.second->getAbstractSignature();
        //printd(5, "AbstractMethod::parseMergeBase(m: %p, f: %p %s::%s) this: %p checking parent: '%s' (f: %p: %d "
        //    "rm: %d) '%s'\n", &m, f, f ? f->getClassName() : "n/a", f ? f->getName() : "n/a", this, sig, f,
        //    f && f->parseHasVariantWithSignature(i.second, relaxed_match), relaxed_match, sig);

        if (f && f->parseHasVariantWithSignature(i.second, relaxed_match)) {
            // add to our pending_save
            i.second->ref();
            pending_save.insert(vmap_t::value_type(sig, i.second));
            continue;
        }

        if (pending_save.find(sig) != pending_save.end()) {
            continue;
        }
        if (vlist.find(sig) != vlist.end()) {
            continue;
        }
        //printd(5, "AbstractMethod::parseMergeBase(m: %p, f: %p %s::%s) this: %p adding to vlist from parent: "
        //    '%s'\n", &m, f, f ? f->getClassName() : "n/a", f ? f->getName() : "n/a", this, sig);
        i.second->ref();
        vlist.insert(vmap_t::value_type(sig, i.second));
    }
}

void AbstractMethod::parseAdd(MethodVariantBase* v) {
    // see if there is already an committed variant matching this signature
    // in this case it must be inherited
    const char* sig = v->getAbstractSignature();
    if (vlist.find(sig) != vlist.end())
        return;
    //printd(5, "AbstractMethod::parseAdd(v: %p) this: %p (%s) new\n", v, this, sig);

    // already referenced for "normal" insertion, ref again for abstract method insertion
    v->ref();
    vlist.insert(vmap_t::value_type(sig, v));

    if (!check_parse) {
        check_parse = true;
    }
}

void AbstractMethod::parseOverride(MethodVariantBase* v) {
    // see if there is already an committed variant matching this signature
    // in this case it must be inherited
    const char* sig = v->getAbstractSignature();
    vmap_t::iterator vi = vlist.find(sig);
    if (vi != vlist.end()) {
        pending_save.insert(vmap_t::value_type(sig, vi->second));

        // move from vlist to pending_save
        vlist.erase(vi);
        // if override is true, then we know we have a variant in a base class, so we can do nothing here
        return;
    }
}

void AbstractMethod::add(MethodVariantBase* v) {
    // see if there is already an committed variant matching this signature
    // in this case it must be inherited
    const char* sig = v->getAbstractSignature();
    if (vlist.find(sig) != vlist.end())
        return;
    // already referenced for "normal" insertion, ref again for abstract method insertion
    v->ref();
    vlist.insert(vmap_t::value_type(sig, v));
    //printd(5, "AbstractMethod::add() adding xxx::xxx(%s)\n", sig);
}

void AbstractMethod::override(MethodVariantBase* v) {
    // see if there is already a committed variant matching this signature
    // in this case it must be inherited
    // issue #3934: allow for overrides even if the signatures don't match
    if (v->getFlags() & QCF_ABSTRACT_OVERRIDE_ALL) {
        for (auto& i : vlist) {
            i.second->deref();
        }
        vlist.clear();
        return;
    }
    const char* sig = v->getAbstractSignature();
    vmap_t::iterator vi = vlist.find(sig);
    if (vi != vlist.end()) {
        vi->second->deref();

        vlist.erase(vi);
    }
}

void AbstractMethod::checkAbstract(const char* cname, const char* mname, vmap_t& vlist, QoreStringNode*& desc) {
    //printd(5, "AbstractMethod::checkAbstract() checking %s::%s() vlist: %d\n", cname, mname, !vlist.empty());
    if (!vlist.empty()) {
        if (!desc) {
            desc = new QoreStringNodeMaker("class '%s' cannot be instantiated because it has the following " \
                "unimplemented abstract variants:", cname);
        }
        for (auto& vi : vlist) {
            MethodVariantBase* v = vi.second;
            desc->sprintf("\n * abstract %s %s::%s(%s)", QoreTypeInfo::getName(v->getReturnTypeInfo()), cname, mname,
                v->getSignature()->getSignatureText());
        }
    }
}

// try to find match non-abstract variants in base classes (allows concrete variants to be inherited from another
// parent class)
void AbstractMethodMap::parseInit(qore_class_private& qc, BCList* scl) {
    //printd(5, "AbstractMethodMap::parseInit() this: %p cname: %s scl: %p ae: %d\n", this, qc.name.c_str(), scl,
    //    empty());
    for (amap_t::iterator i = begin(), e = end(); i != e;) {
        for (vmap_t::iterator vi = i->second->vlist.begin(), ve = i->second->vlist.end(); vi != ve;) {
            // if there is a matching non-abstract variant in any parent class, then move the variant from vlist to
            // pending_save
            MethodVariantBase* v = scl->matchNonAbstractVariant(i->first, vi->second);
            if (v) {
                const char* sig = vi->second->getAbstractSignature();
                i->second->pending_save.insert(vmap_t::value_type(sig, vi->second));
                vmap_t::iterator ti = vi++;

                i->second->vlist.erase(ti);
                // replace abstract variant
                QoreMethod* m = qc.parseFindLocalMethod(i->first);
                if (!m) {
                    m = new QoreMethod(qc.cls, new NormalUserMethod(qc.cls, i->first.c_str()), false);
                    qc.hm[m->getName()] = m;
                }
                qore_method_private::get(*m)->getFunction()->replaceAbstractVariant(v);
                continue;
            }
            ++vi;
        }
        // issue #2741: remove the abstract method if there are no more abstract variants
        if (i->second->vlist.empty()) {
            delete i->second;
            erase(i++);
        } else {
            ++i;
        }
    }
}

// runtime version of parseInit() for classes created dynamically (e.g., JNI classes)
// issue #5056: allows abstract method resolution for classes created at runtime
void AbstractMethodMap::runtimeInit(qore_class_private& qc, BCList* scl) {
    //printd(5, "AbstractMethodMap::runtimeInit() this: %p cname: %s scl: %p ae: %d\n", this, qc.name.c_str(), scl,
    //    empty());
    for (amap_t::iterator i = begin(), e = end(); i != e;) {
        for (vmap_t::iterator vi = i->second->vlist.begin(), ve = i->second->vlist.end(); vi != ve;) {
            // if there is a matching non-abstract variant in any parent class, then move the variant from vlist to
            // pending_save
            MethodVariantBase* v = scl->runtimeMatchNonAbstractVariant(i->first, vi->second, relaxed_match);
            if (!v) {
                // also check the class's own local methods for a matching non-abstract variant;
                // this handles the case where initializeBuiltin() merged abstract variants from
                // parents before local methods were added (e.g. JNI classes where methods are
                // populated after class initialization to support recursive class loading)
                QoreMethod* m = qc.findLocalCommittedMethod(i->first.c_str());
                if (m) {
                    MethodFunctionBase* f = qore_method_private::get(*m)->getFunction();
                    v = f->runtimeHasVariantWithSignature(vi->second, relaxed_match);
                    if (v && v->isAbstract()) {
                        v = nullptr;
                    }
                }
            }
            if (v) {
                const char* sig = vi->second->getAbstractSignature();
                i->second->pending_save.insert(vmap_t::value_type(sig, vi->second));
                vmap_t::iterator ti = vi++;

                i->second->vlist.erase(ti);
                // replace abstract variant - create local method if needed
                QoreMethod* m = qc.findLocalCommittedMethod(i->first.c_str());
                if (!m) {
                    m = new QoreMethod(qc.cls, new NormalUserMethod(qc.cls, i->first.c_str()), false);
                    qc.hm[m->getName()] = m;
                }
                qore_method_private::get(*m)->getFunction()->replaceAbstractVariant(v);
                continue;
            }
            ++vi;
        }
        // issue #2741: remove the abstract method if there are no more abstract variants
        if (i->second->vlist.empty()) {
            delete i->second;
            erase(i++);
        } else {
            ++i;
        }
    }
}

void AbstractMethodMap::parseAddAbstractVariant(const char* name, MethodVariantBase* f) {
    amap_t::iterator i = amap_t::find(name);
    if (i == end()) {
        AbstractMethod* m = new AbstractMethod(relaxed_match);
        // already referenced for "normal" insertion, ref again for abstract method insertion
        f->ref();
        const char* sig = f->getAbstractSignature();
        m->vlist.insert(vmap_t::value_type(sig, f));
        printd(5, "AbstractMethodMap::parseAddAbstractVariant(name: '%s', v: %p) this: %p first (%s)\n", name, f,
            this, sig);
        insert(amap_t::value_type(name, m));
        return;
    }
    printd(5, "AbstractMethodMap::parseAddAbstractVariant(name: '%s', v: %p) this: %p additional\n", name, f, this);
    i->second->parseAdd(f);
}

void AbstractMethodMap::parseOverrideAbstractVariant(const char* name, MethodVariantBase* f) {
    amap_t::iterator i = amap_t::find(name);
    if (i == end())
        return;
    i->second->parseOverride(f);
}

void AbstractMethodMap::addAbstractVariant(const char* name, MethodVariantBase* f) {
    amap_t::iterator i = amap_t::find(name);
    if (i == end()) {
        AbstractMethod* m = new AbstractMethod(relaxed_match);
        // already referenced for "normal" insertion, ref again for abstract method insertion
        f->ref();
        m->vlist.insert(vmap_t::value_type(f->getAbstractSignature(), f));
        //printd(5, "AbstractMethodMap::addAbstractVariant(name: xxx::%s asig: %s, v: %p) this: %p (new)\n", name,
        //    f->getAbstractSignature(), f, this);
        insert(amap_t::value_type(name, m));
        return;
    }
    //printd(5, "AbstractMethodMap::addAbstractVariant(name: xxx::%s asig: %s, v: %p) this: %p\n", name,
    //    f->getAbstractSignature(), f, this);
    i->second->add(f);
}

void AbstractMethodMap::overrideAbstractVariant(const char* name, MethodVariantBase* f) {
    amap_t::iterator i = amap_t::find(name);
    if (i == end()) {
        return;
    }
    i->second->override(f);
    if (i->second->empty()) {
        delete i->second;
        erase(i);
    }
}

DLLLOCAL QoreStringNode* AbstractMethodMap::checkAbstract(const char* name) const {
    if (empty())
        return nullptr;

    QoreStringNode* desc = nullptr;
    for (auto& i : *this) {
        AbstractMethod::checkAbstract(name, i.first.c_str(), i.second->vlist, desc);
    }

    //printd(5, "AbstractMethodMap::parseCheckAbstract() class: %s desc: %p (%s)\n", name, desc, desc ? desc->getBuffer() : "n/a");
    return desc;
}

int AbstractMethodMap::runtimeCheckInstantiateClass(const char* name, ExceptionSink* xsink) const {
    QoreStringNode* desc = checkAbstract(name);
    if (desc) {
        xsink->raiseException("ABSTRACT-CLASS-ERROR", desc);
        return -1;
    }
    return 0;
}

// we check if there are any abstract method variants still in the committed lists
void AbstractMethodMap::parseCheckAbstractNew(const QoreProgramLocation* loc, const char* name) const {
    QoreStringNode* desc = checkAbstract(name);
    if (desc)
        parseException(*loc, "ABSTRACT-CLASS-ERROR", desc);
}

// FIXME: check private method variant access at runtime

struct SelfLocalVarParseHelper {
    DLLLOCAL SelfLocalVarParseHelper(LocalVar* selfid) { push_local_var(selfid, &loc_builtin); }
    DLLLOCAL ~SelfLocalVarParseHelper() { pop_local_var(); }
};

void raise_nonexistent_method_call_warning(const QoreProgramLocation* loc, const QoreClass* qc, const char* method) {
    qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_NONEXISTENT_METHOD_CALL,
        "NON-EXISTENT-METHOD-CALL", "call to non-existent method '%s::%s()'; this call will be evaluated at "
        "run-time, so if the method is called on an object of a subclass that implements this method, then it could "
        "be a valid call, however in any other case it will result in a run-time exception.  To avoid seeing this "
        "warning, use the cast<> operator (note that if the cast is invalid at run-time, a run-time exception will "
        "be raised) or turn off the warning by using '%%disable-warning non-existent-method-call' in your code",
        qc->getName(), method);
}

class VRMutexHelper {
private:
    VRMutex* m;

public:
    DLLLOCAL VRMutexHelper(VRMutex* n_m, ExceptionSink* xsink) : m(n_m) {
        if (m && m->enter(xsink))
            m = nullptr;
    }
    DLLLOCAL ~VRMutexHelper() {
        if (m)
            m->exit();
    }
    DLLLOCAL operator bool() const { return m != nullptr; }
};

qore_class_private::qore_class_private(QoreClass* n_cls, std::string&& nme, std::string&& path, int64 dom,
        QoreClassTypeInfo* n_typeInfo)
        : name(nme),
        path(path),
        cls(n_cls),
        constlist(this),        // constants
        classID(classIDSeq.next()),
        methodID(classID),
        sys(false),
        initialized(false),
        static_init(false),
        parse_init_called(false),
        parse_init_partial_called(false),
        parse_init_constants_called(false),
        has_public_memdecl(false),
        pending_has_public_memdecl(false),
        owns_typeinfo(n_typeInfo ? false : true),
        resolve_copy_done(false),
        has_new_user_changes(false),
        has_sig_changes(false),
        owns_ornothingtypeinfo(false),
        pub(false),
        final(false),
        inject(false),
        gate_access(false),
        committed(false),
        parse_resolve_hierarchy(false),
        parse_resolve_class_members(false),
        parse_resolve_abstract(false),
        has_transient_member(false),
        domain(dom),
        num_methods(0),
        num_user_methods(0),
        num_static_methods(0),
        num_static_user_methods(0),
        typeInfo(n_typeInfo ? n_typeInfo : new QoreClassTypeInfo(cls, name.c_str(), path.c_str())),
        orNothingTypeInfo(nullptr),
        selfid("self", typeInfo),
        spgm(getProgram()),
        deref_source_program(false) {
    assert(methodID == classID);
    assert(!name.empty());
    assert(n_cls);
    selfid.setSelf();

    if (!n_typeInfo) {
        orNothingTypeInfo = new QoreClassOrNothingTypeInfo(cls, name.c_str(), path.c_str());
        owns_ornothingtypeinfo = true;
    } else {
        // see if typeinfo already accepts NOTHING
        if (QoreTypeInfo::parseAcceptsReturns(typeInfo, NT_NOTHING)) {
            orNothingTypeInfo = reinterpret_cast<QoreClassOrNothingTypeInfo*>(typeInfo);
        } else {
            orNothingTypeInfo = new QoreClassOrNothingTypeInfo(cls, name.c_str(), path.c_str());
            owns_ornothingtypeinfo = true;
        }
    }

    const char* mod_name = get_module_context_name();
    if (mod_name) {
        from_module = mod_name;
    }

    // special handling for pseudo-classes
    if (name[0] == '<') {
        initialized = true;
    }

    //printd(5, "qore_class_private::qore_class_private() this: %p creating '%s' ID:%d cls: %p pub: %d sys: %d\n", this, name.c_str(), classID, cls, pub, sys);
}

#if 0
// classes are immutable & should never be copied
qore_class_private::qore_class_private(const qore_class_private& old, qore_ns_private* ns, QoreClass* new_cls) :
        loc(old.loc),
        name(old.name),
        cls(new_cls),
        ns(ns),
        ahm(old.ahm),
        constlist(old.constlist, 0, this),
        serializer(old.serializer),
        deserializer(old.deserializer),
        classID(old.classID),
        methodID(old.methodID),
        sys(old.sys),
        initialized(old.initialized),
        static_init(old.static_init),
        parse_init_called(old.parse_init_called),
        parse_init_partial_called(old.parse_init_partial_called),
        parse_init_constants_called(old.parse_init_constants_called),
        has_public_memdecl(old.has_public_memdecl),
        pending_has_public_memdecl(old.pending_has_public_memdecl),
        owns_typeinfo(false),
        resolve_copy_done(old.resolve_copy_done),
        has_new_user_changes(old.has_new_user_changes),
        has_sig_changes(old.has_sig_changes),
        owns_ornothingtypeinfo(false),
        pub(old.pub),
        final(old.final),
        inject(old.inject),
        gate_access(old.gate_access),
        committed(old.committed),
        parse_resolve_hierarchy(old.parse_resolve_hierarchy),
        parse_resolve_class_members(old.parse_resolve_class_members),
        parse_resolve_abstract(old.parse_resolve_abstract),
        has_transient_member(old.has_transient_member),
        domain(old.domain),
        num_methods(old.num_methods),
        num_user_methods(old.num_user_methods),
        num_static_methods(old.num_static_methods),
        num_static_user_methods(old.num_static_user_methods),
        typeInfo(old.typeInfo),
        orNothingTypeInfo(old.orNothingTypeInfo),
        injectedClass(old.injectedClass),
        selfid(old.selfid),
        hash(old.hash),
        ptr(old.ptr),
        mud(old.mud ? old.mud->copy() : nullptr),
        spgm(old.spgm && old.deref_source_program ? old.spgm->programRefSelf() : old.spgm),
        deref_source_program(old.deref_source_program),
        from_module(old.from_module),
        lang(old.lang) {
    system_constructor = old.system_constructor ? old.system_constructor->copy(cls) : nullptr;

    // set pointer to new copy
    old.new_copy = cls;

    // copy parent class list, if any, after new_copy is set in old
    scl = old.scl ? new BCList(*old.scl) : nullptr;

    //printd(5, "qore_class_private::qore_class_private() old name: %s (%p) new name: %s (%p)\n", old.name.c_str(), old.name.c_str(), name.c_str(), name.c_str());

    // copy methods and maintain method pointers
    for (auto& i : old.hm) {
        QoreMethod* nf = i.second->copy(cls);

        hm[nf->getName()] = nf;
        if (i.second == old.constructor)
            constructor  = nf;
        else if (i.second == old.destructor)
            destructor   = nf;
        else if (i.second == old.copyMethod)
            copyMethod   = nf;
        else if (i.second == old.methodGate)
            methodGate   = nf;
        else if (i.second == old.memberGate)
            memberGate   = nf;
        else if (i.second == old.memberNotification)
            memberNotification = nf;
    }

    // copy static methods
    for (auto& i : old.shm) {
        QoreMethod* nf = i.second->copy(cls);
        shm[nf->getName()] = nf;
    }

    // copy member list
    for (auto& i : old.members.member_list) {
        members.addNoCheck(strdup(i.first), i.second ? new QoreMemberInfo(*i.second, this) : nullptr);
    }

    // copy static var list
    for (auto& i : old.vars.member_list) {
        vars.addNoCheck(strdup(i.first), i.second ? new QoreVarInfo(*i.second, i.second->getAccess()) : nullptr);
    }

    if (old.committed || old.sys) {
        // setup initialization order
        if (scl) {
            scl->sml.processMemberInitializationList(members, member_init_list);
        }
        // issue #3368: must add local members to initialization list after parent class members
        addLocalMembersForInit();
    }
}
#endif

// only called while the parse lock for the QoreProgram owning "old" is held
qore_class_private::qore_class_private(const qore_class_private& old, qore_ns_private* ns, QoreProgram* spgm,
        const char* new_name, bool inject, const qore_class_private* injectedClass, q_setpub_t set_pub)
        // issue #3179: we force a deep copy of "name" to work around COW issues with std::string with GNU libstdc++ 6+
        : name(new_name ? new_name : old.name.c_str()),
        path(old.path),
        ns(ns),
        ahm(old.ahm),
        constlist(old.constlist, 0, this),    // committed constants
        serializer(old.serializer),
        deserializer(old.deserializer),
        classID(old.classID),
        methodID(old.methodID),
        sys(old.sys),
        initialized(true),
        static_init(true),
        parse_init_called(false),
        parse_init_partial_called(false),
        parse_init_constants_called(false),
        has_public_memdecl(old.has_public_memdecl),
        pending_has_public_memdecl(false),
        owns_typeinfo(false),
        resolve_copy_done(false),
        has_new_user_changes(false),
        has_sig_changes(false),
        owns_ornothingtypeinfo(false),
        pub(set_pub == CSP_UNCHANGED ? old.pub : (set_pub == CSP_SETPUB)),
        final(old.final),
        inject(inject),
        gate_access(old.gate_access),
        committed(true),
        parse_resolve_hierarchy(true),
        parse_resolve_class_members(true),
        parse_resolve_abstract(true),
        has_transient_member(old.has_transient_member),
        domain(old.domain),
        num_methods(old.num_methods),
        num_user_methods(old.num_user_methods),
        num_static_methods(old.num_static_methods),
        num_static_user_methods(old.num_static_user_methods),
        typeInfo(nullptr),
        orNothingTypeInfo(nullptr),
        injectedClass(injectedClass),
        selfid(old.selfid),
        hash(old.hash),
        ptr(old.ptr),
        mud(old.mud ? old.mud->copy() : nullptr),
        spgm(old.spgm && old.deref_source_program ? old.spgm->programRefSelf() : old.spgm),
        deref_source_program(old.deref_source_program),
        from_module(old.from_module),
        lang(old.lang) {
    QORE_TRACE("qore_class_private::qore_class_private(const qore_class_private& old)");
    if (!old.initialized)
        const_cast<qore_class_private&>(old).initialize();

    if (new_name) {
        size_t i = path.rfind(old.name);
        assert(i != std::string::npos);
        path.replace(i, old.name.length(), new_name);
    }

    // create new class object
    cls = old.cls->copyImport();
    cls->priv = this;

    printd(5, "qore_class_private::qore_class_private() this: %p creating copy of '%s' (old: '%s') ID:%d cls: %p " \
        "old: %p sys: %d pgm: %p -> %p\n", this, name.c_str(), old.name.c_str(), classID, cls, old.cls, sys, old.spgm,
        spgm);

    // issue #3368: create new type info objects as the class ptr is derived from the typeInfo object in some cases
    typeInfo = new QoreClassTypeInfo(cls, name.c_str(), path.c_str());
    owns_typeinfo = true;
    orNothingTypeInfo = new QoreClassOrNothingTypeInfo(cls, name.c_str(), path.c_str());
    owns_ornothingtypeinfo = true;

    system_constructor = old.system_constructor ? old.system_constructor->copy(cls) : nullptr;

    // set pointer to new copy
    old.new_copy = cls;

    // copy parent class list, if any, after new_copy is set in old
    scl = old.scl ? new BCList(*old.scl) : nullptr;

    //printd(5, "qore_class_private::qore_class_private() old name: %s (%p) new name: %s (%p)\n", old.name.c_str(), old.name.c_str(), name.c_str(), name.c_str());

    // copy methods and maintain method pointers
    for (auto& i : old.hm) {
        QoreMethod* nf = i.second->copy(cls);

        hm[nf->getName()] = nf;
        if (i.second == old.constructor)
            constructor  = nf;
        else if (i.second == old.destructor)
            destructor   = nf;
        else if (i.second == old.copyMethod)
            copyMethod   = nf;
        else if (i.second == old.methodGate)
            methodGate   = nf;
        else if (i.second == old.memberGate)
            memberGate   = nf;
        else if (i.second == old.memberNotification)
            memberNotification = nf;
    }

    // copy static methods
    for (auto& i : old.shm) {
        QoreMethod* nf = i.second->copy(cls);
        shm[nf->getName()] = nf;
    }

    // copy member list
    for (auto& i : old.members.member_list) {
        members.addNoCheck(strdup(i.first), i.second ? new QoreMemberInfo(*i.second, this) : nullptr);
    }

    // copy static var list
    for (auto& i : old.vars.member_list) {
        i.second->parseInit(i.first);
        vars.addNoCheck(strdup(i.first), i.second ? new QoreVarInfo(*i.second) : nullptr);
    }

    // setup initialization order
    if (scl) {
        scl->sml.processMemberInitializationList(members, member_init_list);
    }
    // issue #3368: must add local members to initialization list after parent class members
    addLocalMembersForInit();
}

qore_class_private::~qore_class_private() {
    printd(5, "qore_class_private::~qore_class_private() this: %p %s\n", this, name.c_str());

    if (spgm) {
        if (deref_source_program) {
            spgm->deref(nullptr);
        }
        spgm = nullptr;
    }
    //assert(!spgm);

    assert(!refs.reference_count());
    assert(!var_refs.reference_count());
    assert(!const_refs.reference_count());

    if (!vars.empty()) {
        vars.del();
    }

    // delete normal methods
    for (auto& i : hm) {
        //printd(5, "qore_class_private::~qore_class_private() deleting method %p %s::%s()\n", m, name, m->getName());
        delete i.second;
    }

    // delete static methods
    for (auto& i : shm) {
        //printd(5, "qore_class_private::~qore_class_private() deleting static method %p %s::%s()\n", m, name, m->getName());
        delete i.second;
    }

    delete scl;
    delete system_constructor;

    if (owns_typeinfo)
        delete typeInfo;

    if (owns_ornothingtypeinfo)
        delete orNothingTypeInfo;

    if (mud) {
        try {
            mud->doDeref();
        } catch (AbstractException& e) {
            // there is currently no way to handle these exceptions
#ifdef DEBUG
            // this will result in the exception being printed out on the console
            ExceptionSink xsink;
            e.convert(&xsink);
#endif
        }
    }
}

void qore_class_private::addBuiltinStaticVar(const char* vname, QoreValue value, ClassAccess access, const QoreTypeInfo* vTypeInfo) {
    assert(!vars.inList(vname));

    if (!sys) {
        sys = committed = true;
    }
    if (!has_sig_changes) {
        has_sig_changes = true;
    }

    vars.addNoCheck(strdup(vname), new QoreVarInfo(&loc_builtin, vTypeInfo, 0, value, access));
}

const QoreMethod* qore_class_private::doParseMethodAccess(const QoreMethod* m, const qore_class_private* class_ctx) {
   assert(m);
   ClassAccess ma = qore_method_private::getAccess(*m);
   return ((ma == Public) || ((ma == Private && class_ctx))) ? m : 0;
}

int qore_class_private::initialize() {
    //printd(5, "qore_class_private::initialize() this: %p '%s' initialized: %d scl: %p\n", this, name.c_str(), initialized, scl);
    if (initialized || sys) {
        return 0;
    }

    return initializeIntern();
}

// issue #2657: initialize class hierarchy first before initializing code and members
int qore_class_private::initializeHierarchy(qcp_set_t& qcp_set) {
    assert(scl);
    if (scl->initializeHierarchy(cls, qcp_set)) {
        initialized = true;
        return -1;
    }

    // setup inheritance list for new methods
    for (auto& i : hm) {
        bool is_new = i.second->priv->func->committedEmpty();

        //printd(5, "class_private::initializeHierarchy() this: %p %s::%s is_new: %d cs: %d (%s)\n",
        //    this, name.c_str(), i->first.c_str(), is_new, checkSpecial(i->second->getName()), i->second->getName());

        if (is_new && !checkSpecial(i.second->getName())) {
            parseAddAncestors(i.second);
        }
    }

    // setup inheritance list for new static methods
    for (auto& i : shm) {
        bool is_new = i.second->priv->func->committedEmpty();
        if (is_new) {
            parseAddStaticAncestors(i.second);
        }
    }

    return 0;
}

int qore_class_private::initializeMembers() {
    if (!parse_resolve_class_members) {
        parse_resolve_class_members = true;

        if (scl) {
            return scl->initializeMembers(cls);
        }
    }
    return 0;
}

// process signature entries for base classes
static void do_sig(QoreString& csig, BCNode& n) {
    qore_class_private* qc = qore_class_private::get(*n.sclass);
    csig.sprintf("inherits %s %s ", privpub(n.getAccess()), qc->name.c_str());
    SignatureHash& h = qc->hash;
    if (h) {
        csig.concat('[');
        h.toString(csig);
        csig.concat("]\n");
    } else {
        csig.sprintf("{%d}\n", qc->classID);
    }
}

// process signature entries for class members
static void do_sig(QoreString& csig, const QoreMemberMap::member_list_element_t& i) {
    if (!i.second->local()) {
        return;
    }

    if (i.second) {
        csig.sprintf("%s mem %s %s %s\n", privpub(i.second->access), QoreTypeInfo::getName(i.second->getTypeInfo()), i.first, i.second->exp.getTypeName());
    } else {
        csig.sprintf("%s mem %s\n", privpub(i.second->access), i.first);
    }
}

// process signature entries for class static vars
static void do_sig(QoreString& csig, const QoreVarMap::member_list_element_t& i) {
    if (i.second) {
        csig.sprintf("%s var %s %s %s\n", privpub(i.second->access), QoreTypeInfo::getName(i.second->getTypeInfo()), i.first, i.second->exp.getTypeName());
    } else {
        csig.sprintf("%s var %s\n", privpub(i.second->access), i.first);
    }
}

// process signature entries for class constants
static void do_sig(QoreString& csig, ConstantList& clist) {
    ConstantListIterator cli(clist);
    while (cli.next())
        csig.sprintf("%s const %s %s\n", privpub(cli.getAccess()), cli.getName().c_str(), cli.getValue().getTypeName());
}

int qore_class_private::initializeIntern() {
    //printd(5, "qore_class_private::initializeIntern() this: %p %s class: %p scl: %p initialized: %d\n", this, name.c_str(), cls, scl, initialized);
    if (initialized) {
        return 0;
    }

    initialized = true;

    assert(!name.empty());
    //printd(5, "qore_class_private::initializeIntern() %s class: %p scl: %p\n", name.c_str(), cls, scl);

    // initialize parent classes
    if (scl) {
        int rc = scl->initialize(cls);
        if (rc) {
            return -1;
        }
    }

    QoreParseClassHelper qpch(cls);

    // issue #3242 ensure that any current local vars not related to this class are not resolved when
    // initializing the class in out of order parse initialization; this will ensure that the local var stack
    // is empty when initializing classes (which can be done at any time during parsing)
    LVarStackBreakHelper lvar_stack_helper;

    // first resolve types in pending variants in all method signatures (incl. return types)
    // since abstract method functions are copied by reference from the normal list; this resolves all pending
    // method function signatures as well
    for (auto& i : hm) {
        i.second->priv->func->resolvePendingSignatures();
    }
    for (auto& i : shm) {
        i.second->priv->func->resolvePendingSignatures();
    }

    int err = 0;

    QoreProgram* pgm = getProgram();
    if (pgm && !sys && (qore_program_private::parseAddDomain(pgm, domain))) {
        parseException(*loc, "ILLEGAL-CLASS-DEFINITION", "class '%s' inherits functionality from base classes that "
            "is restricted by current parse options", name.c_str());
        err = -1;
    }

    // signature string - also processed in parseCommit()
    QoreString csig;

    // initialize parent classes
    if (scl) {
        mergeAbstract();
        // add base classes to signature
        if (has_sig_changes) {
            for (auto& i : *scl) {
                // there could have been a parse failure and a pending rollback here
                // so BCNode::sclass could be null here
                if ((*i).sclass) {
                    assert((*i).sclass->priv->initialized);
                    do_sig(csig, *i);
                }
            }
        }
    }

    if (has_sig_changes) {
        // add methods to class signature
        // pending "normal" (non-static) method variants
        for (auto& i : hm)
            i.second->priv->func->parseSignatures(csig, nullptr);
        // pending static method variants
        for (auto& i : shm)
            i.second->priv->func->parseSignatures(csig, "static");
    }

    // add committed vars to signature first before members
    if (!vars.empty()) {
        for (auto& i : vars.member_list) {
            do_sig(csig, i);
        }

        if (!sys) {
            VariableBlockHelper vbh;

            // initialize new static vars
            for (auto& i : vars.member_list) {
                if (i.second) {
                    if (i.second->parseInit(i.first) && !err) {
                        err = -1;
                    }
                }
            }
        }
    }

    if (!members.empty()) {
        // add committed members to signature
        for (auto& i : members.member_list) {
            do_sig(csig, i);
        }

        if (!sys) {
            SelfLocalVarParseHelper slvph(&selfid);

            // add committed members to signature
            for (auto& i : members.member_list) {
                // check new members for conflicts in base classes
                if (parseCheckMemberInBaseClasses(i.first, i.second) && !err) {
                    err = -1;
                }
            }

            // initialize new members
            //printd(5, "qore_class_private::initializeIntern() this: %p '%s' initialing members: %p\n", this, name.c_str(), &members);
            if (members.parseInit(selfid) && !err) {
                err = -1;
            }
        }
    }

    if (has_sig_changes) {
        // process constants for class signature, private first, then public
        do_sig(csig, constlist);

        if (!csig.empty()) {
            printd(5, "qore_class_private::initializeIntern() this: %p '%s' sig:\n%s", this, name.c_str(), csig.getBuffer());
            hash.update(csig);
        }

        has_sig_changes = false;
    } else {
        assert(csig.empty());
    }

    return err;
}

void qore_class_private::mergeAbstract() {
    assert(scl);
    // merge direct base class abstract method lists to ourselves
    for (auto& i : *scl) {
        if ((*i).sclass) {
            qore_class_private* cls = (*i).sclass->priv;
            if (!cls->initialized) {
                if (cls->sys) {
                    cls->initializeBuiltin();
                } else {
                    cls->initialize();
                }
            }
            assert(cls->initialized);

            // called during class initialization to copy committed abstract variants to our variant lists
            AbstractMethodMap& mm = cls->ahm;
            //printd(5, "qore_class_private::initializeIntern() this: %p '%s' parent: %p '%s' mm empty: %d\n", this, name.c_str(), (*i).sclass, (*i).sclass->getName(), (int)mm.empty());
            for (auto& j : mm) {
                // skip if vlists are empty
                if (j.second->vlist.empty()) {
                    //printd(5, "qore_class_private::initializeIntern() this: %p '%s' skipping %s::%s(): vlist empty\n", this, name.c_str(), (*i).sclass->getName(), j.first.c_str());
                    continue;
                }
                amap_t::iterator vi = ahm.find(j.first);
                if (vi != ahm.end()) {
                    vi->second->parseMergeBase(*(j.second), true);
                    continue;
                }
                // now we import the abstract method to our class
                std::unique_ptr<AbstractMethod> m(new AbstractMethod(ahm.relaxed_match));
                // see if there are pending normal variants...
                hm_method_t::iterator mi = hm.find(j.first);

                // merge committed parent abstract variants with any pending local variants
                m->parseMergeBase((*j.second), mi == hm.end()
                    ? nullptr
                    : qore_method_private::get(*mi->second)->getFunction(), true);
                if (!m->empty()) {
                    ahm.insert(amap_t::value_type(j.first, m.release()));
                    //printd(5, "qore_class_private::initializeIntern() this: %p '%s' insert abstract method variant "
                    //    "%s::%s()\n", this, name.c_str(), (*i).sclass->getName(), j.first.c_str());
                }
            }
        }
    }
}

void qore_class_private::finalizeBuiltin(const char* nspath) {
    initializeBuiltin();
    initializeMembers();
    generateBuiltinSignature(nspath);
}

void qore_class_private::initializeBuiltin() {
    assert(sys);
    if (!initialized) {
        initialized = true;
        if (scl) {
            // initialize builtin parent classes first
            scl->initializeBuiltin();
            // merge abstract variants from parent classes to this class
            mergeAbstract();
        }
    }
}

void qore_class_private::generateBuiltinSignature(const char* nspath) {
    // signature string - also processed in parseCommit()
    QoreStringMaker csig("class %s::%s ", nspath, name.c_str());

    // add base classes to signature
    if (scl) {
        for (auto& i : *scl) {
            assert((*i).sclass);
            assert((*i).sclass->priv->initialized);
            do_sig(csig, *i);
        }
    }

    for (auto& i : hm)
        i.second->priv->func->parseSignatures(csig, nullptr);
    for (auto& i : shm)
        i.second->priv->func->parseSignatures(csig, "static");

    // add committed vars to signature first before members
    for (auto& i : vars.member_list) {
        do_sig(csig, i);
    }

    for (auto& i : members.member_list) {
        do_sig(csig, i);
    }

    do_sig(csig, constlist);
    hash.update(csig);
}

// returns a non-static method if it exists in the local class and has been committed to the class
QoreMethod* qore_class_private::findLocalCommittedMethod(const char* nme) {
    QoreMethod* m = parseFindLocalMethod(nme);
    return m && !m->priv->func->committedEmpty() ? m : nullptr;
}

// returns a non-static method if it exists in the local class and has been committed to the class
const QoreMethod* qore_class_private::findLocalCommittedMethod(const char* nme) const {
    const QoreMethod* m = parseFindLocalMethod(nme);
    return m && !m->priv->func->committedEmpty() ? m : nullptr;
}

// returns a static method if it exists in the local class and has been committed to the class
QoreMethod* qore_class_private::findLocalCommittedStaticMethod(const char* nme) {
    QoreMethod* m = parseFindLocalStaticMethod(nme);
    return m && !m->priv->func->committedEmpty() ? m : nullptr;
}

// returns a static method if it exists in the local class and has been committed to the class
const QoreMethod* qore_class_private::findLocalCommittedStaticMethod(const char* nme) const {
    const QoreMethod* m = parseFindLocalStaticMethod(nme);
    return m && !m->priv->func->committedEmpty() ? m : nullptr;
}

int qore_class_private::initMembers(QoreObject& o, bool& need_scan, ExceptionSink* xsink) const {
    assert(xsink);
    if (members.empty() && !scl)
        return 0;

#ifdef QORE_MANAGE_STACK
    if (check_stack(xsink))
        return -1;
#endif

    // make sure the object context is set before evaluating members
    CodeContextHelperBase cch("constructor", &o, this, xsink, false);
    SelfInstantiatorHelper sih(&selfid, &o);

    // issue #2970: initializes members once and save member info in the appropriate location in the object
    for (auto& i : member_init_list) {
        printd(5, "qore_class_private::initMembers() this: %p %s '%s::%s' ctx %p '%s' (access '%s') has parent " \
            "members: %d\n", this, name.c_str(), i.info->getClass()->name.c_str(), i.name, i.member_class_ctx,
            i.member_class_ctx ? i.member_class_ctx->name.c_str() : "n/a", privpub(i.info->access),
            i.info->numParentMembers());

        if (initMember(o, need_scan, i.name, *i.info, i.member_class_ctx, xsink)) {
            assert(*xsink);
            return -1;
        }
    }

    return 0;
}

int qore_class_private::initMember(QoreObject& o, bool& need_scan, const char* member_name,
        const QoreMemberInfo& info, const qore_class_private* member_class_ctx, ExceptionSink* xsink) const {
    printd(5, "qore_class_private::initMember() this: %p '%s::%s' initializing '%s::%s' member_class_ctx: %p'\n",
        this, name.c_str(), member_name, member_class_ctx ? member_class_ctx->name.c_str() : "<self>", member_name,
        member_class_ctx);
    QoreValue& v = qore_object_private::get(o)->getMemberValueRefForInitialization(member_name, member_class_ctx);
    assert(v.isNothing());
    if (!info.exp.isNothing()) {
        // set runtime location
        QoreProgramLocationHelper l(info.loc);
        ValueEvalOptimizedRefHolder val(info.exp, xsink);
        if (*xsink) {
            return -1;
        }
        printd(5, "qore_class_private::initMember() this: %p '%s::%s' type %s val: %s filter: %d\n", this, name.c_str(),
            member_name, QoreTypeInfo::getPath(info.getTypeInfo()),
            val->getFullTypeName(true),
            QoreTypeInfo::mayRequireFilter(info.getTypeInfo(), *val));
        if (QoreTypeInfo::mayRequireFilter(info.getTypeInfo(), *val)) {
            val.ensureReferencedValue();
            QoreTypeInfo::acceptInputMember(info.getTypeInfo(), member_name, *val, xsink);
            if (*xsink) {
                return -1;
            }
        }
        v = val.takeReferencedValue();
        //printd(5, " + val: %d (%s)\n", v.getType() == NT_INT ? (int)v.getAsBigInt() : 0, v.getTypeName());
        if (needs_scan(v)) {
            qore_object_private::incScanCount(o, 1);
            if (!need_scan) {
                need_scan = true;
            }
        }
    }
    return 0;
}

void qore_class_private::execBaseClassConstructor(QoreObject* self, BCEAList* bceal, RuntimeConfig& rc,
        ExceptionSink* xsink) const {
    //printd(5, "qore_class_private::execBaseClassConstructor() '%s' constructor: %p\n", name.c_str(), constructor);
    // if there is no constructor, execute the superclass constructors directly
    if (!constructor){
        if (scl) { // execute base class constructors if any
            scl->execConstructors(self, bceal, rc, xsink);
        }

        return;
    }
    // no lock is sent with constructor, because no variable has been assigned yet
    bool already_executed;
    const AbstractQoreFunctionVariant* variant;
    const QoreProgramLocation* aloc = nullptr;
    QoreListNode* args = bceal->findArgs(cls->getID(), &already_executed, variant, aloc);
    if (!already_executed && !qore_object_private::get(*self)->checkData(cls->getID())) {
        QoreProgramOptionalLocationHelper plh(aloc);
        constructor->priv->evalConstructor(variant, self, args, bceal, rc, xsink);
    }
}

QoreObject* qore_class_private::execConstructor(ExceptionSink* xsink, RuntimeConfig& rc,
        const AbstractQoreFunctionVariant* variant, const QoreListNode* args, const QoreClass* obj_cls,
        bool allow_abstract) const {
#ifdef DEBUG
    if (!allow_abstract) {
        // instantiation checks have to be made at parse time
        for (auto& i : ahm) {
            printd(0, "qore_class_private::execConstructor() %s::constructor() abstract error '%s':\n",
                name.c_str(), i.first.c_str());
            vmap_t& v = i.second->vlist;
            for (auto& vi : v) {
                printd(0, " + vlist: %s\n", vi.first);
            }
            v = i.second->pending_save;
            for (auto& vi : v) {
                printd(0, " + pending_save: %s\n", vi.first);
            }
        }
        assert(ahm.empty());
    }
#endif

    // check if the class has pending changes and is used in a constant initialization expression
    if (parseHasPendingChanges()) {
        xsink->raiseException("ILLEGAL-CLASS-INSTANTIATION", new QoreStringNodeMaker("cannot instantiate '%s' class "
            "for assignment in a constant expression in the parse initialization phase when the class has "
            "uncommitted changes", name.c_str()));
        return nullptr;
    }

    // create new object
    QoreObject* self = new QoreObject(obj_cls ? obj_cls : cls, getProgram());

    ReferenceHolder<BCEAList> bceal(scl ? new BCEAList : nullptr, xsink);

    printd(5, "qore_class_private::execConstructor() class: %p %s::constructor() o: %p variant: %p\n", cls,
        name.c_str(), self, variant);

    // if we made at least one assignment, then scan the object for recursive references after all assignments
    bool need_scan = false;

    // instantiate members first
    initMembers(*self, need_scan, xsink);

    // scan object for recursive references after all member assignments
    if (need_scan) {
        LValueHelper lvh(*self, xsink);
    }

    if (!*xsink) {
        // it's possible for constructor = 0 and variant != 0, when a class is instantiated to initialize a constant
        // and the matched variant is pending
        if (!constructor && !variant) {
            if (scl) { // execute superconstructors if any
                CodeContextHelper cch(xsink, CT_BUILTIN, "constructor", self, this);

                scl->execConstructors(self, *bceal, rc, xsink);
            }
        } else {
            if (!constructor) {
                hm_method_t::const_iterator i = hm.find("constructor");
                assert(i != hm.end());
                i->second->priv->evalConstructor(variant, self, args, *bceal, rc, xsink);
            } else
                constructor->priv->evalConstructor(variant, self, args, *bceal, rc, xsink);
            printd(5, "qore_class_private::execConstructor() class: %p %s done\n", cls, name.c_str());
        }
    }

    if (*xsink) {
        // instead of executing the destructors for the superclasses that were already executed we call QoreObject::obliterate()
        // which will clear out all the private data by running their dereference methods which must be OK
        self->obliterate(xsink);
        printd(5, "qore_class_private::execConstructor() this: %p %s::constructor() o: %p, exception in constructor, obliterating QoreObject and returning 0\n", this, name.c_str(), self);
        return nullptr;
    }

    printd(5, "qore_class_private::execConstructor() this: %p %s::constructor() returning o: %p\n", this, name.c_str(), self);
    return self;
}

void qore_class_private::parseCommit() {
    //printd(5, "qore_class_private::parseCommit() %s this: %p cls: %p hm.size: %d sys: %d committed: %d\n", name.c_str(), this, cls, hm.size(), sys, committed);
    if (committed) {
        return;
    }

    if (!sys) {
        committed = true;

        if (parse_init_called) {
            parse_init_called = false;
        }

        if (parse_init_partial_called) {
            parse_init_partial_called = false;
        }

        if (has_new_user_changes) {
            // signature string: note the signature is updated in two places, here and in initializeIntern()
            QoreString csig;

            // add parent classes to signature if creating for the first time
            if (has_sig_changes && scl) {
                for (auto& i : *scl) {
                    assert((*i).sclass);
                    (*i).sclass->priv->parseCommit();
                    do_sig(csig, *i);
                }
            }

            // commit pending "normal" (non-static) method variants
            for (auto& i : hm) {
                bool is_new = i.second->priv->func->committedEmpty();
                if (has_sig_changes)
                    i.second->priv->func->parseCommitMethod(csig, 0);
                else
                    i.second->priv->func->parseCommitMethod();
                if (is_new) {
                    checkAssignSpecial(i.second);
                    ++num_methods;
                    ++num_user_methods;
                }
            }

            // commit pending static method variants
            for (auto& i : shm) {
                bool is_new = i.second->priv->func->committedEmpty();
                if (has_sig_changes)
                    i.second->priv->func->parseCommitMethod(csig, "static");
                else
                    i.second->priv->func->parseCommitMethod();
                if (is_new) {
                    ++num_static_methods;
                    ++num_static_user_methods;
                }
            }

            // commit abstract method variant list changes
            ahm.parseCommit();

            if (has_sig_changes) {
                // add all static vars to signature
                for (auto& i : vars.member_list) {
                    do_sig(csig, i);
                }

                for (auto& i : members.member_list) {
                    do_sig(csig, i);
                }
            }

            // set flags
            if (pending_has_public_memdecl) {
                if (!has_public_memdecl)
                    has_public_memdecl = true;
                pending_has_public_memdecl = false;
            }

            // process constants for signature
            if (has_sig_changes) {
                do_sig(csig, constlist);
            }

            // if there are any signature changes, then change the class' signature
            if (has_sig_changes) {
                if (!csig.empty()) {
                    printd(5, "qore_class_private::parseCommit() this:%p '%s' sig:\n%s", this, name.c_str(), csig.getBuffer());
                    hash.update(csig);
                }
                has_sig_changes = false;
            } else {
                assert(csig.empty());
            }

            // issue #2970: make member initialization list in correct order
            if (scl) {
                scl->sml.processMemberInitializationList(members, member_init_list);
            }
            // now add local members last
            addLocalMembersForInit();

            has_new_user_changes = false;
        } else {
#ifdef DEBUG
            for (auto& i : hm)
                assert(i.second->priv->func->pendingEmpty());
            for (auto& i : shm)
                assert(i.second->priv->func->pendingEmpty());
#endif
            assert(!pending_has_public_memdecl);
        }
    } else {
        assert(committed);
        assert(!pending_has_public_memdecl);
    }

    if (!hash) {
        hash.updateEmpty();
    }

    // we check base classes if they have public members if we don't have any
    // it's safe to call parseHasPublicMembersInHierarchy() because the 2nd stage
    // of parsing has completed without any errors (or we wouldn't be
    // running parseCommit())
    if (!has_public_memdecl && (scl ? scl->parseHasPublicMembersInHierarchy() : false)) {
        has_public_memdecl = true;
    }
}

void qore_class_private::addLocalMembersForInit() {
    for (auto& i : members.member_list) {
        // skip imported members
        if (!i.second->local()) {
            continue;
        }

        const qore_class_private* member_class_ctx = i.second->getClassContext(this);
        // local members can only be stored in the standard object hash or in the private:internal hash for this class
        assert(!member_class_ctx || member_class_ctx == this);
        member_init_list.push_back(member_init_entry_t(i.first, i.second, member_class_ctx));
    }
}

void qore_class_private::parseCommitRuntimeInit(ExceptionSink* xsink) {
    assert(initialized);
    // finalize constant initialization
    constlist.parseCommitRuntimeInit();

    // issue #2885: ensure that static class initialization is only performed once
    if (!static_init) {
        static_init = true;
        // add all pending static vars to real list and initialize them
        vars.parseCommitRuntimeInit(xsink);
    }
}

void qore_class_private::addBuiltinMethod(const char* mname, MethodVariantBase* variant) {
    assert(strcmp(mname, "constructor"));
    assert(strcmp(mname, "destructor"));
    assert(strcmp(mname, "copy"));

    if (!sys) {
        sys = committed = true;
    }

    hm_method_t::iterator i = hm.find(mname);
    QoreMethod* nm;
    if (i == hm.end()) {
        MethodFunctionBase* m = new BuiltinNormalMethod(cls, mname);
        nm = new QoreMethod(cls, m, false);
        insertBuiltinMethod(nm);
    } else {
        nm = i->second;
    }

    // set the pointer from the variant back to the owning method
    variant->setMethod(nm);

    nm->priv->addBuiltinVariant(variant);

    if (variant->isAbstract()) {
        ahm.addAbstractVariant(mname, variant);
    } else {
        ahm.overrideAbstractVariant(mname, variant);
    }
}

void qore_class_private::addBuiltinStaticMethod(const char* mname, MethodVariantBase* variant) {
    assert(strcmp(mname, "constructor"));
    assert(strcmp(mname, "destructor"));

    if (!sys) {
        sys = committed = true;
    }

    hm_method_t::iterator i = shm.find(mname);
    QoreMethod* nm;
    if (i == shm.end()) {
        MethodFunctionBase* m = new BuiltinStaticMethod(cls, mname);
        nm = new QoreMethod(cls, m, true);
        insertBuiltinStaticMethod(nm);
    } else {
        nm = i->second;
    }

    // set the pointer from the variant back to the owning method
    variant->setMethod(nm);

    nm->priv->addBuiltinVariant(variant);
}

void qore_class_private::addBuiltinConstructor(BuiltinConstructorVariantBase* variant) {
    if (!sys) {
        sys = committed = true;
    }

    QoreMethod* nm;
    if (!constructor) {
        MethodFunctionBase* m = new ConstructorMethodFunction(cls);
        nm = new QoreMethod(cls, m, false);
        constructor = nm;
        insertBuiltinMethod(nm, true);
    } else {
        nm = const_cast<QoreMethod*>(constructor);
    }

    // set the pointer from the variant back to the owning method
    variant->setMethod(nm);

    nm->priv->addBuiltinVariant(variant);
}

void qore_class_private::addBuiltinDestructor(BuiltinDestructorVariantBase* variant) {
    if (!sys) {
        sys = committed = true;
    }

    assert(!destructor);
    DestructorMethodFunction *m = new DestructorMethodFunction(cls);
    QoreMethod* qm = new QoreMethod(cls, m, false);
    destructor = qm;
    insertBuiltinMethod(qm, true);
    // set the pointer from the variant back to the owning method
    variant->setMethod(qm);

    qm->priv->addBuiltinVariant(variant);
}

void qore_class_private::addBuiltinCopyMethod(BuiltinCopyVariantBase* variant) {
    if (!sys) {
        sys = committed = true;
    }

    assert(!copyMethod);
    CopyMethodFunction *m = new CopyMethodFunction(cls);
    QoreMethod* qm = new QoreMethod(cls, m, false);
    copyMethod = qm;
    insertBuiltinMethod(qm, true);
    // set the pointer from the variant back to the owning method
    variant->setMethod(qm);

    qm->priv->addBuiltinVariant(variant);
}

void qore_class_private::setBuiltinSystemConstructor(BuiltinSystemConstructorBase* m) {
    if (!sys) {
        sys = committed = true;
    }

    assert(!system_constructor);
    QoreMethod* qm = new QoreMethod(cls, m, false);
    qm->priv->setBuiltin();
    system_constructor = qm;
}

void qore_class_private::setPublic() {
    assert(!pub);
    pub = true;
}

QoreListNode* BCEAList::findArgs(qore_classid_t classid, bool* aexeced, const AbstractQoreFunctionVariant*& variant, const QoreProgramLocation*& loc) {
    bceamap_t::iterator i = lower_bound(classid);
    // not found
    if (i == end() || i->first != classid) {
        insert(i, bceamap_t::value_type(classid, new BCEANode));
        *aexeced = false;
        variant = nullptr;
        return nullptr;
    }

    // already executed
    if (i->second->execed) {
        *aexeced = true;
        variant = nullptr;
        return nullptr;
    }

    // found and not yet executed
    *aexeced = false;
    i->second->execed = true;
    variant = i->second->variant;
    loc = i->second->loc;
    return i->second->args;
}

int BCEAList::add(qore_classid_t classid, const QoreListNode* arg, const AbstractQoreFunctionVariant* variant, const QoreProgramLocation* loc, ExceptionSink* xsink) {
    // see if class already exists in the list
    bceamap_t::iterator i = lower_bound(classid);
    bool n = ((i == end() || i->first != classid));
    if (!n && i->second->execed)
        return 0;

    // save arguments for evaluation in the constructor
    if (n)
        insert(i, bceamap_t::value_type(classid, new BCEANode(loc, arg ? arg->listRefSelf() : 0, variant)));
    else {
        assert(!i->second->args);
        assert(!i->second->variant);
        assert(!i->second->execed);
        i->second->args = arg ? arg->listRefSelf() : 0;
        i->second->variant = reinterpret_cast<const MethodVariant*>(variant);
    }
    return 0;
}

void BCEAList::deref(ExceptionSink* xsink) {
    bceamap_t::iterator i;
    while ((i = begin()) != end()) {
        BCEANode* n = i->second;
        erase(i);

        if (n->args)
            n->args->deref(xsink);
        delete n;
    }
    delete this;
}

// resolves classes, parses arguments, and attempts to find constructor variant
int BCANode::parseInit(BCList* bcl, const char* classname) {
    QoreClass* sclass = nullptr;
    if (ns) {
        sclass = qore_root_ns_private::parseFindScopedClass(loc, *ns);
        assert(!sclass || sclass->priv);
        printd(5, "BCANode::parseInit() this: %p resolved named scoped %s -> %p\n", this, ns->ostr, sclass);
        delete ns;
        ns = nullptr;
    } else {
        sclass = qore_root_ns_private::parseFindClass(loc, name);
        assert(!sclass || sclass->priv);
        printd(5, "BCANode::parseInit() this: %p resolved %s -> %p\n", this, name, sclass);
        free(name);
        name = nullptr;
    }

    int err = 0;

    if (sclass) {
        if (!bcl->match(sclass)) {
            parse_error(*loc, "%s in base constructor argument list is not a base class of %s", sclass->getName(),
                classname);
            err = -1;
        } else {
            classid = sclass->getID();

            // find constructor variant
            const QoreMethod* m = sclass->getConstructor();
            QoreParseContext parse_context;
            parse_context.oflag = qore_class_private::getSelfId(*sclass);
            if (m) {
                err = parseArgsVariant(loc, parse_context, qore_method_private::get(*m)->getFunction(), nullptr);
            } else {
                if (parse_args) {
                    type_vec_t argTypeInfo;
                    err = parse_args->initArgs(parse_context, argTypeInfo, args);
                    parse_args = nullptr;
                }
            }
            if (parse_context.lvids) {
                parse_error(*loc, "illegal local variable declaration in base class constructor argument");
                while (parse_context.lvids--) {
                    pop_local_var();
                }
                if (!err) {
                    err = -1;
                }
            }
        }
    }
    return err;
}

int BCNode::tryResolveClass(QoreClass* cls, bool raise_error) {
    if (!sclass) {
        if (cname) {
            // if the class cannot be found, RootQoreNamespace::parseFindScopedClass() will throw the appropriate exception
            sclass = qore_root_ns_private::parseFindScopedClass(loc, *cname, raise_error);
            if (sclass) {
                assert(sclass->priv);
                printd(5, "BCNode::tryResolveClass() %s inheriting %s (%p)\n", cls->getName(), cname->ostr, sclass);
                delete cname;
                cname = nullptr;
            } else {
                printd(5, "BCNode::tryResolveClass() %s cannot resolve %s\n", cls->getName(), cname->ostr);
            }
        } else {
            // issue #3005: cstr may be nullptr in case of a previous parse error
            if (cstr) {
                // if the class cannot be found, qore_root_ns_private::parseFindClass() will throw the appropriate exception
                sclass = qore_root_ns_private::parseFindClass(loc, cstr, raise_error);
                if (sclass) {
                    assert(sclass->priv);
                    printd(5, "BCNode::tryResolveClass() %s inheriting %s (%p)\n", cls->getName(), cstr, sclass);
                    free(cstr);
                    cstr = nullptr;
                } else {
                    printd(5, "BCNode::tryResolveClass() %s cannot resolve %s\n", cls->getName(), cstr);
                }
            }
        }
        if (cls == sclass) {
            parse_error(*cls->priv->loc, "class '%s' cannot inherit itself", cls->getName());
            assert(cls->priv->scl);
            cls->priv->scl->valid = false;
            sclass = nullptr;
        }
        //printd(5, "BCNode::tryResolveClass() cls: %p '%s' inherits %p '%s' final: %d\n", cls, cls->getName(),
        //  sclass, sclass ? sclass->getName() : "n/a", sclass ? sclass->priv->final : 0);
    }
    return sclass ? 0 : -1;
}

int BCNode::initializeHierarchy(QoreClass* cls, qcp_set_t& qcp_set) {
    tryResolveClass(cls, true);
    int rc = 0;
    // recursively add base classes to special method list
    if (sclass) {
        if (!qcp_set.insert(sclass->priv).second) {
            // issue #2317: ensure that the class is really recursive in the inheritance list before throwing an
            // exception
            if (sclass->priv->scl && sclass->priv->scl->findInHierarchy(*sclass->priv)) {
                parse_error(*sclass->priv->loc, "circular reference in class hierarchy, '%s' is an ancestor of " \
                    "itself", sclass->getName());
                if (sclass->priv->scl) {
                    sclass->priv->scl->valid = false;
                }
                return -1;
            }
            return 0;
        }
        if (sclass->priv->final) {
            parse_error(*cls->priv->loc, "class '%s' cannot inherit 'final' class '%s'", cls->getName(),
                sclass->getName());
            rc = -1;
        }

        if (sclass->priv->scl && sclass->priv->initializeHierarchy(qcp_set) && !rc) {
            rc = -1;
        }
    }
    return rc;
}

int BCNode::initializeMembers(QoreClass* cls) {
    if (!sclass) {
        return 0;
    }
    int err = sclass->priv->initializeMembers();
    // import all base class member definitions into this class
    cls->priv->parseImportMembers(*sclass->priv, access);
    return err;
}

int BCNode::initialize(QoreClass* cls) {
    if (!sclass) {
        return 0;
    }
    assert(sclass);
    int rc = 0;

    rc = sclass->priv->initializeIntern();
    // include all base class domains in this class's domain
    if (!sclass->priv->addBaseClassesToSubclass(cls, is_virtual)) {
        cls->priv->domain |= sclass->priv->domain;
        // import all base class member definitions into this class
        //cls->priv->parseImportMembers(*sclass->priv, access);
    }
    if (sclass->priv->final) {
        parse_error(*cls->priv->loc, "class '%s' cannot inherit 'final' class '%s'", cls->getName(),
            sclass->getName());
        if (!rc) {
            rc = -1;
        }
    }

    return rc;
}

bool BCNode::isBaseClass(QoreClass* qc, bool toplevel) const {
    assert(sclass);

    if (!toplevel && access >= Internal)
        return false;

    //printd(5, "BCNode::isBaseClass() %p %s (%d) == %s (%d)\n", this, qc->getName(), qc->getID(), sclass->getName(), sclass->getID());
    if (qc->getID() == sclass->getID() || (sclass->priv->scl && sclass->priv->scl->isBaseClass(qc, false))) {
        //printd(5, "BCNode::isBaseClass() %p %s (%d) TRUE\n", this, qc->getName(), qc->getID());
        return true;
    }
    return false;
}

const QoreMethod* BCNode::runtimeFindCommittedMethod(const char* name, ClassAccess& n_access, const qore_class_private* class_ctx, bool allow_internal) const {
    assert(sclass);

    if (access >= Internal && !allow_internal)
        return nullptr;

    const QoreMethod* m = sclass->priv->runtimeFindCommittedMethodIntern(name, n_access, class_ctx);
    if (m && n_access < access)
        n_access = access;

    return m;
}

const QoreMethod* BCNode::runtimeFindCommittedStaticMethod(const char* name, ClassAccess& n_access, const qore_class_private* class_ctx, bool allow_internal) const {
    assert(sclass);

    if (access >= Internal && !allow_internal)
        return nullptr;

    const QoreMethod* m = sclass->priv->runtimeFindCommittedStaticMethodIntern(name, n_access, class_ctx);
    if (m && n_access < access)
        n_access = access;

    return m;
}

const QoreMethod* BCNode::parseFindNormalMethod(const char* name, const qore_class_private* class_ctx, bool allow_internal) const {
    // sclass can be 0 if the class could not be found during parse initialization
    if (!sclass)
        return nullptr;

    if (access > Public && (!class_ctx || (access == Internal && !allow_internal)))
        return nullptr;

    return sclass->priv->parseFindNormalMethodIntern(name, class_ctx);
}

const QoreMethod* BCNode::parseFindStaticMethod(const char* name, const qore_class_private* class_ctx, bool allow_internal) const {
    // sclass can be 0 if the class could not be found during parse initialization
    if (!sclass)
        return nullptr;

    if (access > Public && (!class_ctx || (access == Internal && !allow_internal)))
        return nullptr;

    return sclass->priv->parseFindStaticMethodIntern(name, class_ctx);
}

const QoreMethod* BCNode::parseResolveSelfMethod(const QoreProgramLocation* loc, const char* name, const qore_class_private* class_ctx, bool allow_internal) const {
    // sclass can be 0 if the class could not be found during parse initialization
    if (!sclass)
        return nullptr;

    if (access >= Internal && !allow_internal)
        return nullptr;

    sclass->priv->initialize();
    return sclass->priv->parseResolveSelfMethodIntern(loc, name, class_ctx);
}

const QoreMemberInfo* BCNode::parseFindMember(const char* mem, const qore_class_private*& qc, ClassAccess& n_access, bool toplevel) const {
    // sclass can be 0 if the class could not be found during parse initialization
    if (!sclass)
        return nullptr;

    if (access >= Internal && !toplevel)
        return nullptr;

    const QoreMemberInfo* rv = sclass->priv->parseFindMemberNoInit(mem, qc, n_access, false);
    if (rv && n_access < access)
        n_access = access;
    return rv;
}

const QoreVarInfo* BCNode::parseFindVar(const char* name, const qore_class_private*& qc, ClassAccess& n_access, bool toplevel) const {
    // sclass can be 0 if the class could not be found during parse initialization
    if (!sclass)
        return nullptr;

    if (access >= Internal && !toplevel)
        return nullptr;

    const QoreVarInfo* vi = sclass->priv->parseFindVar(name, qc, n_access, false);
    if (vi && n_access < access)
        n_access = access;
    return vi;
}

const QoreClass* BCNode::findInHierarchy(const qore_class_private& qc) {
   // sclass can be 0 if the class could not be found during parse initialization
   return sclass ? sclass->priv->findInHierarchy(qc) : nullptr;
}

const QoreClass* BCNode::getClass(qore_classid_t cid, ClassAccess& n_access, bool toplevel) const {
    // sclass can be 0 if the class could not be found during parse initialization
    if (!sclass)
        return nullptr;

    if (access >= Internal && !toplevel)
        return nullptr;

    const QoreClass* qc = (sclass->getID() == cid) ? sclass : sclass->priv->getClassIntern(cid, n_access, false);
    if (qc && n_access < access)
        n_access = access;

    return qc;
}

const QoreClass* BCNode::getClass(const qore_class_private& qc, ClassAccess& n_access, bool toplevel) const {
    // sclass can be 0 if the class could not be found during parse initialization
    if (!sclass)
        return nullptr;

    if (access >= Internal && !toplevel)
        return nullptr;

    const QoreClass* rv = sclass->priv->getClassIntern(qc, n_access, false);

    if (rv && n_access < access)
        n_access = access;

    return rv;
}

const QoreClass *BCNode::parseGetClass(const qore_class_private& qc, ClassAccess& n_access, bool toplevel) const {
    // sclass can be 0 if the class could not be found during parse initialization
    if (!sclass)
        return nullptr;

    if (access >= Internal && !toplevel)
        return nullptr;

    const QoreClass* rv = sclass->priv->parseGetClassIntern(qc, n_access, false);

    if (rv && n_access < access)
        n_access = access;

    return rv;
}

bool BCNode::inHierarchy(const qore_class_private& qc, ClassAccess& n_access) const {
    // sclass can be 0 if the class could not be found during parse initialization
    if (!sclass) {
        return false;
    }

    bool rv = sclass->priv->inHierarchy(qc, n_access);

    if (rv && n_access < access) {
        n_access = access;
    }

    return rv;
}

bool BCNode::inHierarchyStrict(const qore_class_private& qc, ClassAccess& n_access) const {
    // sclass can be 0 if the class could not be found during parse initialization
    if (!sclass) {
        return false;
    }

    bool rv = sclass->priv->inHierarchyStrict(qc, n_access);

    if (rv && n_access < access) {
        n_access = access;
    }

    return rv;
}

bool BCNode::runtimeIsPrivateMember(const char* str, bool toplevel) const {
    assert(sclass);

    if (access >= Internal && !toplevel)
        return false;

    return sclass->priv->runtimeIsPrivateMemberIntern(str, false);
}

QoreValue BCNode::parseFindConstantValue(const char* cname, const QoreTypeInfo*& typeInfo, bool& found, const qore_class_private* class_ctx, bool allow_internal) const {
    // sclass can be 0 if the class could not be found during parse initialization
    if (!sclass)
        return QoreValue();

    if (access >= Internal && !allow_internal)
        return QoreValue();

    return sclass->priv->parseFindConstantValueIntern(cname, typeInfo, found, class_ctx);
}

bool BCNode::parseCheckHierarchy(const QoreClass* cls, ClassAccess& n_access, bool toplevel) const {
    // sclass can be 0 if the class could not be found during parse initialization
    if (!sclass)
        return false;

    if (access >= Internal && !toplevel)
        return false;

    if (sclass->priv->parseCheckHierarchyIntern(cls, n_access, false)) {
        //printd(5, "BCNode::parseCheckHierarchy() '%s' '%s' access: %s n_access: %s\n", sclass->getName(), cls->getName(), privpub(access), privpub(n_access));
        if (n_access < access)
            n_access = access;
        return true;
    }

    return false;
}

QoreVarInfo* BCNode::parseFindStaticVar(const char* vname, const QoreClass*& qc, ClassAccess& n_access, bool check, bool toplevel) const {
    // sclass can be 0 if the class could not be found during parse initialization
    if (!sclass)
        return nullptr;

    if (access >= Internal && !toplevel)
        return nullptr;

    QoreVarInfo* vi = sclass->priv->parseFindStaticVarIntern(vname, qc, n_access, check, false);
    if (vi && n_access < access)
        n_access = access;

    return vi;
}

void BCNode::execConstructors(QoreObject* o, BCEAList* bceal, RuntimeConfig& rc, ExceptionSink* xsink) const {
    //printd(5, "BCNode::execConstructors() %s::constructor() o: %p (for subclass %s) virtual: %d\n", sclass->getName(), o, o->getClass()->getName(), is_virtual);

    // do not execute constructors for virtual base classes
    if (is_virtual)
        return;
    sclass->priv->execBaseClassConstructor(o, bceal, rc, xsink);
}

int BCNode::addBaseClassesToSubclass(QoreClass* child, bool is_virtual) {
    // issue #2318 must check for duplicate base classes here
    // sclass may be 0 in case of a parse exception
    if (!sclass) {
        return 0;
    }

    if (sclass->priv->scl && sclass->priv->scl->findInHierarchy(*child->priv)) {
        parse_error(*loc, "OOPS %s", child->getName());
        return -1;
    }

    return sclass->priv->addBaseClassesToSubclass(child, is_virtual || this->is_virtual);
}

void BCNode::initializeBuiltin() {
    assert(sclass);
    if (sclass->priv->sys) {
        sclass->priv->initializeBuiltin();
    }
}

const ConstantEntry* BCNode::findConstantEntry(const char* name) const {
    assert(sclass);
    return sclass->priv->findConstantEntry(name);
}

void BCList::parseResolveAbstract() {
    for (auto& i : *this) {
        if (i->sclass) {
            i->sclass->priv->parseResolveAbstract();
        }
    }
}

void BCList::rescanParents(QoreClass* cls) {
    if (rescanned) {
        return;
    }
    rescanned = true;
    // iterate sml for all virtual parent classes; must iterate with offsets;
    // the vector can be reallocated during this operation
    for (unsigned i = 0; i < sml.size(); ++i) {
        if (sml[i].second && sml[i].first->priv->scl) {
            sml[i].first->priv->scl->rescanParents(sml[i].first);
            sml[i].first->priv->scl->sml.alignBaseClassesInSubclass(sml[i].first, cls, true);
        }
    }
}

bool BCList::isBaseClass(QoreClass* qc, bool toplevel) const {
    for (auto& i : *this) {
        if ((*i).isBaseClass(qc, toplevel))
            return true;
    }
    //printd(5, "BCList::isBaseClass() %p %s (%d) FALSE\n", this, qc->getName(), qc->getID());
    return false;
}

int BCList::initializeHierarchy(QoreClass* cls, qcp_set_t& qcp_set) {
    for (auto& i : *this) {
        if ((*i).initializeHierarchy(cls, qcp_set)) {
            if (valid) {
                valid = false;
            }
        }
    }

    // compare each class in the list to ensure that there are no duplicates
    for (bclist_t::iterator i = begin(), e = end(); i != e; ++i) {
        if ((*i)->sclass) {
            bclist_t::iterator j = i;
            while (++j != e) {
                if (!(*j)->sclass) {
                    continue;
                }
                if ((*i)->sclass->getID() == (*j)->sclass->getID()) {
                    parse_error(*cls->priv->loc, "class '%s' cannot inherit '%s' more than once", cls->getName(),
                        (*i)->sclass->getName());
                    if (valid) {
                        valid = false;
                    }
                }
            }
        }
    }

    return valid ? 0 : -1;
}

int BCList::initializeMembers(QoreClass* cls) {
    int err = 0;
    for (auto& i : *this) {
        if (i->initializeMembers(cls) && !err) {
            err = -1;
        }
    }
    return err;
}

int BCList::initialize(QoreClass* cls) {
    printd(5, "BCList::initialize(%s) this: %p empty: %d\n", cls->getName(), this, empty());
    for (bclist_t::iterator i = begin(), e = end(); i != e; ++i) {
        if ((*i)->initialize(cls)) {
            if (valid) {
                valid = false;
            }
        }
    }

    return valid ? 0 : -1;
}

bool BCList::parseHasPublicMembersInHierarchy() const {
    for (auto& i : *this) {
        if ((*i).sclass && qore_class_private::get(*(*i).sclass)->parseHasPublicMembersInHierarchy()) {
            return true;
        }
    }
    return false;
}

const QoreMemberInfo* BCList::parseFindMember(const char* mem, const qore_class_private*& qc, ClassAccess& access, bool toplevel) const {
    if (!valid) {
        return nullptr;
    }

    for (auto& i : *this) {
        const QoreMemberInfo* mi = (*i).parseFindMember(mem, qc, access, toplevel);
        if (mi) {
            return mi;
        }
    }
    return nullptr;
}

const QoreVarInfo* BCList::parseFindVar(const char* name, const qore_class_private*& qc, ClassAccess& n_access, bool toplevel) const {
    if (!valid) {
        return nullptr;
    }

    for (auto& i : *this) {
        const QoreVarInfo* rv = (*i).parseFindVar(name, qc, n_access, toplevel);
        if (rv)
            return rv;
    }
    return nullptr;
}

// called at run time
const QoreMethod* BCList::runtimeFindCommittedMethod(const char* name, ClassAccess& access, const qore_class_private* class_ctx, bool allow_internal) const {
    for (auto& i : *this) {
        const QoreMethod* m = (*i).runtimeFindCommittedMethod(name, access, class_ctx, allow_internal);
        if (m)
            return m;
    }
    return nullptr;
}

// called at run time
const QoreMethod* BCList::runtimeFindCommittedStaticMethod(const char* name, ClassAccess& access, const qore_class_private* class_ctx, bool allow_internal) const {
    for (auto& i : *this) {
        const QoreMethod* m = (*i).runtimeFindCommittedStaticMethod(name, access, class_ctx, allow_internal);
        if (m)
            return m;
    }
    return nullptr;
}

const QoreMethod* BCList::parseFindNormalMethod(const char* name, const qore_class_private* class_ctx, bool allow_internal) {
    if (!valid) {
        return nullptr;
    }

    for (auto& i : *this) {
        const QoreMethod* m = (*i).parseFindNormalMethod(name, class_ctx, allow_internal);
        if (m)
            return m;
    }
    return nullptr;
}

const QoreMethod* BCList::parseFindStaticMethod(const char* name, const qore_class_private* class_ctx, bool allow_internal) {
    if (!valid) {
        return nullptr;
    }

    for (auto& i : *this) {
        const QoreMethod* m = (*i).parseFindStaticMethod(name, class_ctx, allow_internal);
        if (m)
            return m;
    }
    return nullptr;
}

const QoreMethod* BCList::parseResolveSelfMethod(const QoreProgramLocation* loc, const char* name, const qore_class_private* class_ctx, bool allow_internal) {
    for (auto& i : *this) {
        const QoreMethod* m = (*i).parseResolveSelfMethod(loc, name, class_ctx, allow_internal);
        if (m)
            return m;
    }
    return nullptr;
}

bool BCList::match(const QoreClass* cls) {
    for (auto& i : *this) {
        if (cls == (*i).sclass) {
            return true;
        }
    }
    return false;
}

bool BCList::runtimeIsPrivateMember(const char* str, bool toplevel) const {
    for (auto& i : *this) {
        if ((*i).runtimeIsPrivateMember(str, toplevel))
            return true;
    }
    return false;
}

void BCList::execConstructors(QoreObject* o, BCEAList* bceal, RuntimeConfig& rc, ExceptionSink* xsink) const {
    for (auto& i : *this) {
        (*i).execConstructors(o, bceal, rc, xsink);
        if (*xsink)
            break;
    }
}

bool BCList::parseCheckHierarchy(const QoreClass* cls, ClassAccess& access, bool toplevel) const {
    for (auto& i : *this) {
        if ((*i).parseCheckHierarchy(cls, access, toplevel))
            return true;
    }
    return false;
}

void BCList::addNewAncestors(QoreMethod* m) {
    QoreFunction *f = qore_method_private::get(*m)->getFunction();
    const char* name = m->getName();
    for (auto& i : *this) {
        QoreClass* qc = (*i).sclass;
        // should be only called from builtin classes, therefore qc != NULL
        assert(qc);
        assert((*i).access != Internal);
        const QoreMethod* w = qc->priv->findLocalCommittedMethod(name);
        if (w)
            f->addNewAncestor(qore_method_private::get(*w)->getFunction(), (*i).access);
        qc->priv->addNewAncestors(m);
    }
}

void BCList::addNewStaticAncestors(QoreMethod* m) {
    QoreFunction *f = qore_method_private::get(*m)->getFunction();
    const char* name = m->getName();
    for (auto& i : *this) {
        QoreClass* qc = (*i).sclass;
        // should be only called from builtin classes, therefore qc != NULL
        assert(qc);
        assert((*i).access != Internal);
        const QoreMethod* w = qc->priv->findLocalCommittedStaticMethod(name);
        if (w)
            f->addNewAncestor(qore_method_private::get(*w)->getFunction(), (*i).access);
        qc->priv->addNewStaticAncestors(m);
    }
}

void BCList::addAncestors(QoreMethod* m) {
    const char* name = m->getName();
    for (auto& i : *this) {
        QoreClass* qc = (*i).sclass;
        assert(qc);

        const QoreMethod* w = qc->priv->findLocalCommittedMethod(name);
        if (w)
            qore_method_private::get(*m)->getFunction()->addAncestor(qore_method_private::get(*w)->getFunction(),
                (*i).access);

        qc->priv->addAncestors(m);
    }
}

void BCList::addStaticAncestors(QoreMethod* m) {
    const char* name = m->getName();
    for (auto& i : *this) {
        QoreClass* qc = (*i).sclass;
        assert(qc);

        const QoreMethod* w = qc->priv->findLocalCommittedStaticMethod(name);
        if (w)
            qore_method_private::get(*m)->getFunction()->addAncestor(qore_method_private::get(*w)->getFunction(),
                (*i).access);
        qc->priv->addStaticAncestors(m);
    }
}

void BCList::parseAddAncestors(QoreMethod* m) {
    const char* name = m->getName();

    //printd(5, "BCList::parseAddAncestors(%p %s) this: %p size: %d\n", m, name, this, size());

    for (auto& i : *this) {
        // if there was a parse error finding the base class, then skip
        QoreClass* qc = (*i).sclass;
        if (!qc)
            continue;

        const QoreMethod* w = qc->priv->parseFindLocalMethod(name);
        //printd(5, "BCList::parseAddAncestors(%p %s) this: %p qc: %p w: %p\n", m, name, this, qc, w);

        if (w) {
            qore_method_private::get(*m)->getFunction()->addAncestor(qore_method_private::get(*w)->getFunction(),
                (*i).access);
        }

        qc->priv->parseAddAncestors(m);
    }
}

void BCList::parseAddStaticAncestors(QoreMethod* m) {
    const char* name = m->getName();
    for (auto& i : *this) {
        QoreClass* qc = (*i).sclass;
        // qc may be 0 if there were a parse error with an unknown class earlier
        if (!qc)
            continue;

        const QoreMethod* w = qc->priv->parseFindLocalStaticMethod(name);
        if (w)
            qore_method_private::get(*m)->getFunction()->addAncestor(qore_method_private::get(*w)->getFunction(),
                (*i).access);

        qc->priv->parseAddStaticAncestors(m);
    }
}

void BCList::resolveCopy() {
    for (auto& i : *this) {
        assert((*i).sclass->priv->new_copy);
        (*i).sclass = (*i).sclass->priv->new_copy;
        (*i).sclass->priv->resolveCopy();
    }

    sml.resolveCopy();
}

QoreValue BCList::parseFindConstantValue(const char* cname, const QoreTypeInfo*& typeInfo, bool& found, const qore_class_private* class_ctx, bool allow_internal) const {
    if (!valid)
        return QoreValue();

    for (auto& i : *this) {
       QoreValue rv = (*i).parseFindConstantValue(cname, typeInfo, found, class_ctx, allow_internal);
       if (found)
           return rv;
    }
    return QoreValue();
}

QoreVarInfo* BCList::parseFindStaticVar(const char* vname, const QoreClass*& qc, ClassAccess& access, bool check, bool toplevel) const {
    if (!valid)
        return nullptr;

    for (auto& i : *this) {
        QoreVarInfo* vi = (*i).parseFindStaticVar(vname, qc, access, check, toplevel);
        if (vi)
            return vi;
    }
    return nullptr;
}

const QoreClass* BCList::findInHierarchy(const qore_class_private& qc) {
    for (auto& i : *this) {
        const QoreClass* rv = (*i).findInHierarchy(qc);
        if (rv)
            return rv;
    }

    return nullptr;
}

const QoreClass* BCList::getClass(qore_classid_t cid, ClassAccess& n_access, bool toplevel) const {
    for (auto& i : *this) {
        const QoreClass* qc = (*i).getClass(cid, n_access, toplevel);
        if (qc)
            return qc;
    }

    return nullptr;
}

const QoreClass* BCList::getClass(const qore_class_private& qc, ClassAccess& n_access, bool toplevel) const {
    for (auto& i : *this) {
        const QoreClass* rv = (*i).getClass(qc, n_access, toplevel);
        if (rv)
            return rv;
    }

    return nullptr;
}

const QoreClass* BCList::parseGetClass(const qore_class_private& qc, ClassAccess& n_access, bool toplevel) const {
    for (auto& i : *this) {
        const QoreClass* rv = (*i).parseGetClass(qc, n_access, toplevel);
        if (rv)
            return rv;
    }

    return nullptr;
}

bool BCList::inHierarchy(const qore_class_private& qc, ClassAccess& n_access) const {
    for (auto& i : *this) {
        bool b = (*i).inHierarchy(qc, n_access);
        if (b) {
            return b;
        }
    }

    return false;
}

bool BCList::inHierarchyStrict(const qore_class_private& qc, ClassAccess& n_access) const {
    for (auto& i : *this) {
        bool b = (*i).inHierarchyStrict(qc, n_access);
        if (b) {
            return b;
        }
    }

    return false;
}

MethodVariantBase* BCList::matchNonAbstractVariant(const std::string& name, MethodVariantBase* v) const {
    for (auto& i : *this) {
        const QoreClass* nqc = (*i).sclass;
        //printd(5, "BCList::matchNonAbstractVariant() this: %p %s::%s %p (%s) ncq: %p\n", this, nqc ? nqc->getName() : "n/a", name.c_str(), v, v->getAbstractSignature(), nqc);

        // nqc may be 0 if there were a parse error with an unknown class earlier
        // also if the original abstract variant comes from the class being searched, then skip it
        if (!nqc || v->getClass() == nqc)
            continue;

        QoreMethod* m = nqc->priv->parseFindLocalMethod(name);
        if (m) {
            MethodFunctionBase* f = qore_method_private::get(*m)->getFunction();
            MethodVariantBase* ov = f->parseHasVariantWithSignature(v, nqc->priv->ahm.relaxed_match);
            if (ov && !ov->isAbstract())
                return ov;
        }
        if (nqc->priv->scl) {
            MethodVariantBase* ov = nqc->priv->scl->matchNonAbstractVariant(name, v);
            if (ov)
                return ov;
        }
    }

    return nullptr;
}

// runtime version for dynamically-created classes (e.g., JNI classes)
MethodVariantBase* BCList::runtimeMatchNonAbstractVariant(const std::string& name, MethodVariantBase* v,
        bool relaxed_match) const {
    for (auto& i : *this) {
        const QoreClass* nqc = (*i).sclass;
        // nqc may be nullptr if there were errors earlier
        // also if the original abstract variant comes from the class being searched, then skip it
        if (!nqc || v->getClass() == nqc)
            continue;

        QoreMethod* m = nqc->priv->findLocalCommittedMethod(name.c_str());
        if (m) {
            MethodFunctionBase* f = qore_method_private::get(*m)->getFunction();
            MethodVariantBase* ov = f->runtimeHasVariantWithSignature(v, relaxed_match);
            if (ov && !ov->isAbstract())
                return ov;
        }
        if (nqc->priv->scl) {
            MethodVariantBase* ov = nqc->priv->scl->runtimeMatchNonAbstractVariant(name, v, relaxed_match);
            if (ov)
                return ov;
        }
    }

    return nullptr;
}

int BCAList::execBaseClassConstructorArgs(BCEAList* bceal, ExceptionSink* xsink) const {
    for (auto& i : *this) {
        if (bceal->add(i->classid, i->getArgs(), i->getVariant(), i->loc, xsink))
            return -1;
    }
    return 0;
}

QoreProgram* QoreClass::getProgram() const {
    return priv->ns->getProgram();
}

const QoreMethod* QoreClass::findStaticMethod(const char* nme) const {
    const qore_class_private* class_ctx = runtime_get_class();
    if (class_ctx && !priv->runtimeCheckPrivateClassAccess(class_ctx)) {
        class_ctx = nullptr;
    }

    CurrentProgramRuntimeParseContextHelper pch;
    ClassAccess access;
    return priv->runtimeFindCommittedStaticMethod(nme, access, class_ctx);
}

const QoreMethod* QoreClass::findStaticMethod(const char* nme, ClassAccess& access) const {
    const qore_class_private* class_ctx = runtime_get_class();
    if (class_ctx && !priv->runtimeCheckPrivateClassAccess(class_ctx))
        class_ctx = nullptr;
    CurrentProgramRuntimeParseContextHelper pch;
    return priv->runtimeFindCommittedStaticMethod(nme, access, class_ctx);
}

const QoreMethod* QoreClass::findMethod(const char* nme) const {
    const qore_class_private* class_ctx = runtime_get_class();
    if (class_ctx && !priv->runtimeCheckPrivateClassAccess(class_ctx)) {
        class_ctx = nullptr;
    }
    ClassAccess access = Public;
    return priv->runtimeFindCommittedMethod(nme, access, class_ctx);
}

const QoreMethod* QoreClass::findMethod(const char* nme, ClassAccess& access) const {
    const qore_class_private* class_ctx = runtime_get_class();
    if (class_ctx && !priv->runtimeCheckPrivateClassAccess(class_ctx)) {
        class_ctx = nullptr;
    }
    return priv->runtimeFindCommittedMethod(nme, access, class_ctx);
}

const QoreMethod* QoreClass::findLocalStaticMethod(const char* nme) const {
    return priv->findLocalCommittedStaticMethod(nme);
}

const QoreMethod* QoreClass::findLocalMethod(const char* nme) const {
    return priv->findLocalCommittedMethod(nme);
}

bool QoreClass::hasCopy() const {
   return priv->copyMethod ? true : false;
}

qore_classid_t QoreClass::getID() const {
   return priv->classID;
}

bool QoreClass::isSystem() const {
   return priv->sys;
}

bool QoreClass::isModulePublic() const {
    return priv->pub;
}

bool QoreClass::isAbstract() const {
    return priv->hasAbstract();
}

bool QoreClass::isFinal() const {
    return priv->final;
}

bool QoreClass::isInjected() const {
    return priv->inject;
}

bool QoreClass::isPseudoClass() const {
    return priv->name[0] == '<';
}

const QoreClass* QoreClass::getInjectedAsClass() const {
    if (priv->injectedClass) {
        return priv->injectedClass->cls;
    }
    return nullptr;
}

QoreClass* QoreClass::getInjectedAsClass() {
    if (priv->injectedClass) {
        return priv->injectedClass->cls;
    }
    return nullptr;
}

qore_type_t QoreClass::getPseudoClassType() const {
    if (!isPseudoClass()) {
        return -1;
    }

    if (priv->name == "<value>") {
        return -1;
    }

    if (priv->name == "<binary>") {
        return NT_BINARY;
    }

    if (priv->name == "<bool>") {
        return NT_BOOLEAN;
    }

    if (priv->name == "<callref>") {
        return NT_FUNCREF;
    }

    if (priv->name == "<closure>") {
        return NT_RUNTIME_CLOSURE;
    }

    if (priv->name == "<date>") {
        return NT_DATE;
    }

    if (priv->name == "<float>") {
        return NT_FLOAT;
    }

    if (priv->name == "<hash>") {
        return NT_HASH;
    }

    if (priv->name == "<int>") {
        return NT_INT;
    }

    if (priv->name == "<int>") {
        return NT_INT;
    }

    if (priv->name == "<list>") {
        return NT_LIST;
    }

    if (priv->name == "<nothing>") {
        return NT_NOTHING;
    }

    if (priv->name == "<number>") {
        return NT_NUMBER;
    }

    if (priv->name == "<object>") {
        return NT_OBJECT;
    }

    assert(priv->name == "<string>");
    return NT_STRING;
}

QoreValue QoreClass::evalPseudoMethod(const QoreValue n, const char* nme, const QoreListNode* args,
        ExceptionSink* xsink) const {
    assert(isPseudoClass());
    RuntimeConfig& rc = rc_get_current_ref();
    return priv->evalPseudoMethod(n, nme, args, rc, xsink);
}

QoreValue QoreClass::evalPseudoMethod(const QoreMethod* m, const QoreExternalMethodVariant* variant,
        const QoreValue n, const QoreListNode* args, ExceptionSink* xsink) const {
    assert(isPseudoClass());
    RuntimeConfig& rc = rc_get_current_ref();
    return priv->evalPseudoMethod(m, reinterpret_cast<const AbstractQoreFunctionVariant*>(variant), n, args, rc,
        xsink);
}

void QoreClass::setSystem() {
    priv->sys = true;
    priv->committed = true;
    priv->loc = &loc_builtin;
}

bool QoreClass::hasMemberGate() const {
    return priv->memberGate != nullptr;
}

bool QoreClass::hasMethodGate() const {
    return priv->methodGate != nullptr;
}

bool QoreClass::hasMemberNotification() const {
    return priv->memberNotification != nullptr;
}

int64 QoreClass::getDomain() const {
    return priv->domain;
}

const char* QoreClass::getName() const {
    return priv->name.c_str();
}

int QoreClass::numMethods() const {
    return priv->num_methods;
}

int QoreClass::numStaticMethods() const {
    return priv->num_static_methods;
}

int QoreClass::numUserMethods() const {
    return priv->num_user_methods;
}

int QoreClass::numStaticUserMethods() const {
    return priv->num_static_user_methods;
}

void QoreClass::addBuiltinBaseClass(QoreClass* qc) {
    if (!priv->scl) {
        priv->scl = new BCList;
    }
    priv->scl->push_back(new BCNode(&loc_builtin, qc));
    priv->scl->sml.add(this, qc, false);
}

void QoreClass::addDefaultBuiltinBaseClass(QoreClass* qc) {
    addBuiltinBaseClass(qc);
    // make sure no methodID has already been assigned
    assert(priv->methodID == priv->classID);
    priv->methodID = qc->priv->classID;
}

void QoreClass::addBuiltinVirtualBaseClass(QoreClass* qc) {
    priv->addBaseClass(qc, true);
}

void QoreClass::addBaseClass(QoreClass* qc, bool virt) {
    priv->addBaseClass(qc, virt);
}

void QoreClass::initializeBuiltin() {
    priv->initializeBuiltin();
}

int QoreClass::runtimeCheckInstantiateClass(ExceptionSink* xsink) const {
    return priv->runtimeCheckInstantiateClass(xsink);
}

void qore_class_private::addBaseClass(QoreClass* qc, bool virt) {
    assert(qc);
    //printd(5, "adding %s as base class to %s (virt: %d)\n", qc->priv->name.c_str(), priv->name.c_str(), virt);
    if (!scl) {
        scl = new BCList;
    }
    scl->push_back(new BCNode(&loc_builtin, qc, virt));

    // add parent classes of new base class
    if (qc->priv->scl && qc->priv->scl->valid) {
        qc->priv->scl->addBaseClassesToSubclass(qc, cls, virt);
    }
    scl->sml.add(cls, qc, virt);
}

void qore_class_private::parseCheckAbstractNew(const QoreProgramLocation* loc) const {
    // we must defer the check until all classes are completely resolved
    qore_root_ns_private::get(*getRootNS())->deferParseCheckAbstractNew(this, loc);
}

void qore_class_private::parseDoCheckAbstractNew(const QoreProgramLocation* loc) const {
    assert(initialized);
    ahm.parseCheckAbstractNew(loc, name.c_str());
}

const QoreMethod* qore_class_private::parseFindNormalMethod(const char* nme, const qore_class_private* class_ctx) {
    // if we have a class context, first we have to check here for an internal method
    if (class_ctx) {
        const QoreMethod* m = class_ctx->parseFindLocalMethod(nme);
        if (m && qore_method_private::getAccess(*m) == Internal)
            return m;
    }

    return parseFindNormalMethodIntern(nme, class_ctx);
}

const QoreMethod* qore_class_private::parseFindStaticMethod(const char* nme, const qore_class_private* class_ctx) {
    // if we have a class context, first we have to check here for an internal method
    if (class_ctx) {
        const QoreMethod* m = class_ctx->parseFindLocalStaticMethod(nme);
        if (m && qore_method_private::getAccess(*m) == Internal)
            return m;
    }

    return parseFindStaticMethodIntern(nme, class_ctx);
}

const QoreMethod* qore_class_private::parseFindAnyMethod(const char* nme, const qore_class_private* class_ctx) {
    const QoreMethod* m = nullptr;

    // if we have a class context, first we have to check here for an internal method
    if (class_ctx) {
        m = class_ctx->parseFindAnyLocalMethod(nme);
        if (m && qore_method_private::getAccess(*m) != Internal)
            m = nullptr;
    }

    if (!m) {
        m = parseFindNormalMethodIntern(nme, class_ctx);
    }

    //printd(5, "qore_class_private::parseFindAnyMethod() cls: '%s' nme: '%s' class_ctx: %p m: %p (%s::%s) %s\n",
    //  name.c_str(), nme, class_ctx, m, m ? m->getClass()->getName() : "n/a", m ? m->getName() : "n/a",
    //  m ? privpub(qore_method_private::getAccess(*m)) : "n/a");

    if (m && strcmp(nme, "constructor") && strcmp(nme, "destructor") && strcmp(nme, "copy")
        && ((qore_method_private::getAccess(*m) == Public) || class_ctx)) {
        return m;
    }

    m = parseFindStaticMethodIntern(nme, class_ctx);
    return m && ((qore_method_private::getAccess(*m) == Public) || class_ctx) ? m : nullptr;
}

const QoreMethod* qore_class_private::parseFindAnyMethodStaticFirst(const char* nme,
        const qore_class_private* class_ctx) {
    const QoreMethod* m = nullptr;

    // if we have a class context, first we have to check here for an internal method
    if (class_ctx) {
        m = class_ctx->parseFindLocalStaticMethod(nme);
        if (m && qore_method_private::getAccess(*m) != Internal) {
            m = class_ctx->parseFindLocalMethod(nme);
            if (m && qore_method_private::getAccess(*m) != Internal) {
                m = nullptr;
            }
        }
    }

    if (!m)
        m = parseFindStaticMethodIntern(nme, class_ctx);

    if (m && ((qore_method_private::getAccess(*m) == Public) || class_ctx))
        return m;

    m = parseFindNormalMethodIntern(nme, class_ctx);

    if (m && (!strcmp(nme, "constructor") || !strcmp(nme, "destructor") || !strcmp(nme, "copy"))) {
        m = nullptr;
    }

    return m && ((qore_method_private::getAccess(*m) == Public) || class_ctx) ? m : nullptr;
}

// searches all methods, both pending and comitted
const QoreMethod* qore_class_private::parseResolveSelfMethod(const QoreProgramLocation* loc, const char* nme,
        const qore_class_private* class_ctx) {
    initialize();

    const QoreMethod* m = nullptr;

    if (class_ctx) {
        const QoreMethod* m = class_ctx->parseFindAnyLocalMethod(nme);
        if (m && qore_method_private::getAccess(*m) != Internal)
            m = nullptr;
    }
    if (!m)
        m = parseResolveSelfMethodIntern(loc, nme, class_ctx);

    if (!m) {
        parse_error(*loc, "no method %s::%s() has been defined; if you want to make a call to a method that will " \
            "be defined in an inherited class, then use 'self.%s()' instead", name.c_str(), nme, nme);
        return nullptr;
    }
    printd(5, "qore_class_private::parseResolveSelfMethod(%s) resolved to %s::%s() %p (static: %d)\n", nme,
        name.c_str(), nme, m, m->isStatic());

    const char* mname = m->getName();
    // make sure we're not calling a method that cannot be called directly
    if (!m->isStatic() && (!strcmp(mname, "constructor") || !strcmp(mname, "destructor") || !strcmp(mname, "copy"))) {
        parse_error(*loc, "explicit calls to %s() methods are not allowed", nme);
        return nullptr;
    }

    return m;
}

const QoreMethod* qore_class_private::parseFindSelfMethod(const char* nme) {
    initialize();
    return parseFindAnyMethod(nme, this);
}

const QoreMethod* qore_class_private::parseResolveSelfMethod(const QoreProgramLocation* loc, NamedScope* nme) {
    // first find class
    QoreClass* qc = qore_root_ns_private::parseFindScopedClassWithMethod(loc, *nme, true);
    if (!qc)
        return nullptr;

    // see if class is base class of this class
    if (qc != cls && (!scl || !scl->isBaseClass(qc, true))) {
        parse_error(*loc, "'%s' is not a base class of '%s'", qc->getName(), name.c_str());
        return nullptr;
    }

    return qc->priv->parseResolveSelfMethod(loc, nme->getIdentifier(), this);
}

// finds a non-static method in the class hierarchy at parse time, optionally initializes classes
const QoreMethod* qore_class_private::parseFindNormalMethodIntern(const char* mname, const qore_class_private* class_ctx) {
    initialize();
    const QoreMethod* m = parseFindLocalMethod(mname);
    if (m && doParseMethodAccess(m, class_ctx)) {
        //printd(5, "qore_class_private::parseFindNormalMethodIntern() this: %p '%s' mname: '%s' m: %p\n", this, name.c_str(), mname, m);
        return m;
    }
    //printd(5, "qore_class_private::parseFindNormalMethodIntern() this: %p '%s' mname: '%s' NOT FOUND\n", this, name.c_str(), mname);
    if (scl) {
        m = scl->parseFindNormalMethod(mname, class_ctx, class_ctx == this);
        if (m && doParseMethodAccess(m, class_ctx)) {
            return m;
        }
    }
    return nullptr;
}

// finds a static method in the class hierarchy at parse time, optionally initializes classes
const QoreMethod* qore_class_private::parseFindStaticMethodIntern(const char* mname, const qore_class_private* class_ctx) {
    initialize();
    const QoreMethod* m = parseFindLocalStaticMethod(mname);
    if (m && doParseMethodAccess(m, class_ctx))
        return m;
    if (scl) {
        m = scl->parseFindStaticMethod(mname, class_ctx, class_ctx == this);
        if (m && doParseMethodAccess(m, class_ctx))
            return m;
    }
    return nullptr;
}

const QoreMethod* qore_class_private::parseResolveSelfMethodIntern(const QoreProgramLocation* loc, const char* nme,
        const qore_class_private* class_ctx) {
    const QoreMethod* m = parseFindLocalMethod(nme);
    if (m && doMethodAccess(m, qore_method_private::getAccess(*m), class_ctx))
        return m;
    m = parseFindLocalStaticMethod(nme);
    if (m && doMethodAccess(m, qore_method_private::getAccess(*m), class_ctx))
        return m;
    if (scl) {
        m = scl->parseResolveSelfMethod(loc, nme, class_ctx, this == class_ctx);
        if (m && doMethodAccess(m, qore_method_private::getAccess(*m), class_ctx))
            return m;
    }
    return nullptr;
}

QoreValue qore_class_private::parseFindConstantValueIntern(const char* cname, const QoreTypeInfo*& cTypeInfo,
        bool& found, const qore_class_private* class_ctx) {
    // issue #4967: initializing all class constants here would result in recursive constant definition exceptions
    // being raised
    // check constant list
    ClassAccess access = Public;
    // NOTE: the following function call will initialize any constant found
    QoreValue rv = constlist.find(cname, cTypeInfo, access, found);

    // check for accessibility to private constants
    if (found) {
        if (access == Internal) {
            if (class_ctx == this) {
                return rv;
            } else {
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

int qore_class_private::parseCheckClassHierarchyMembers(const char* mname, const QoreMemberInfo& b_mi,
        const QoreMemberInfo& l_mi) const {
    // if both classes are system classes, then ignore
    if (sys && l_mi.getClass()->sys) {
        return 0;
    }
    if (l_mi.access != b_mi.access || l_mi.parseHasTypeInfo() || b_mi.parseHasTypeInfo()) {
        // raise an exception only if parse exceptions are enabled
        if (getProgram()->getParseExceptionSink()) {
            qore_program_private::makeParseException(getProgram(), *l_mi.loc, "PARSE-ERROR",
                new QoreStringNodeMaker("class '%s' cannot be combined with class '%s' in the same hierarchy because "
                    "member '%s' is accessible from class '%s' from more than one declaration in the hierarchy",
                    l_mi.getClass()->name.c_str(), b_mi.getClass()->name.c_str(), mname, name.c_str()));
        }
        return -1;
    }
    return 0;
}

// imports members from qc -> this
void qore_class_private::parseImportMembers(qore_class_private& qc, ClassAccess access) {
#ifdef DEBUG
    if (ns && qc.cls->priv->ns) {
        std::string nspath0;
        std::string nspath1;
        ns->getPath(nspath0);
        qc.cls->priv->ns->getPath(nspath1);
        assert(qc.name != name || nspath0 != nspath1);
    }
#endif

    printd(5, "qore_class_private::parseImportMembers() this: %p '%s' members: %p init qc: %p '%s' qc.members: %p\n",
        this, name.c_str(), &members, &qc, qc.name.c_str(), &qc.members);
    // set parse class context before initializing members
    QoreParseClassHelper cph(qc.cls);

    // issue #2657: ensure that parent class members are initialized before merging
    qc.members.parseInit(qc.selfid);
    for (auto& i : qc.members.member_list) {
        QoreMemberInfo* mi = members.find(i.first);
        if (mi) {
            // issue #2970: if the member is declared as private:internal in this class
            // or if the current or new member are inaccessible, add contextual access information
            // so that the member can be properly accessed from base classes
            if (mi->access == Inaccessible && i.second->access < Internal) {
                printd(5, "qore_class_private::parseImportMembers() this: %p swapping '%s' <- '%s::%s' this: '%s' " \
                    "other: '%s' context access\n", this, name.c_str(), qc.name.c_str(), i.first, privpub(mi->access),
                    privpub(i.second->access));
                i.second->parseInit(i.first, selfid);
                QoreMemberInfo* nmi = new QoreMemberInfo(*i.second, this, access);
                std::unique_ptr<QoreMemberInfo> old = std::unique_ptr<QoreMemberInfo>(members.replace(i.first, nmi));
                nmi->addContextAccess(*old->getClass()->members.find(i.first));
                //nmi->addContextAccess(*old.get());
                continue;
            } else if (mi->isLocalInternal() || i.second->access == Internal || mi->access == Inaccessible) {
                printd(5, "qore_class_private::parseImportMembers() this: %p importing '%s' <- '%s::%s' this: '%s' " \
                    "other: '%s' context access\n", this, name.c_str(), qc.name.c_str(), i.first, privpub(mi->access),
                    privpub(i.second->access));
                mi->addContextAccess(*i.second);
                continue;
            }

            printd(5, "qore_class_private::parseImportMembers() this: %p importing '%s' <- '%s::%s' ('%s') parent " \
                "access: '%s' this access: '%s'\n", this, name.c_str(), qc.name.c_str(), i.first,
                i.second->exp.getTypeName(), privpub(access), privpub(mi->access));

            if (!mi->getClass()->equal(*i.second->getClass())) {
                mi->getClass()->parseCheckClassHierarchyMembers(i.first, *(i.second), *mi);
            }
            continue;
        }
        i.second->parseInit(i.first, selfid);
        QoreMemberInfo* nmi = new QoreMemberInfo(*i.second, this, access);
        printd(5, "qore_class_private::parseImportMembers() this: %p '%s' importing <- '%s::%s' ('%s') new " \
            "access: '%s' old: '%s'\n", this, name.c_str(), qc.name.c_str(), i.first, i.second->exp.getTypeName(),
            privpub(nmi->access), privpub(i.second->access));
        members.addInheritedNoCheck(strdup(i.first), nmi);
    }
}

void qore_class_private::parseRollback() {
    if (parse_init_called)
        parse_init_called = false;

    if (parse_init_partial_called)
        parse_init_partial_called = false;

    if (has_sig_changes)
        has_sig_changes = false;

    if (!has_new_user_changes) {
#ifdef DEBUG
        // verify status
        for (hm_method_t::iterator i = hm.begin(), e = hm.end(); i != e; ++i)
            assert(i->second->priv->func->pendingEmpty());
        for (hm_method_t::iterator i = shm.begin(), e = shm.end(); i != e; ++i)
            assert(i->second->priv->func->pendingEmpty());
#endif
        assert(!pending_has_public_memdecl);
        return;
    }

    // rollback pending "normal" (non-static) method variants
    for (hm_method_t::iterator i = hm.begin(), e = hm.end(); i != e;) {
        // if there are no committed variants, then the method must be deleted
        if (i->second->priv->func->committedEmpty()) {
            delete i->second;
            hm.erase(i++);
            continue;
        }

        i->second->priv->func->parseRollbackMethod();
        ++i;
    }

    // rollback pending static method variants
    for (hm_method_t::iterator i = shm.begin(), e = shm.end(); i != e;) {
        // if there are no committed variants, then the method must be deleted
        if (i->second->priv->func->committedEmpty()) {
            delete i->second;
            shm.erase(i++);
            continue;
        }

        i->second->priv->func->parseRollbackMethod();
        ++i;
    }

    // rollback pending abstract method variant changes
    ahm.parseRollback();

    // delete all static vars
    if (!vars.empty()) {
        vars.del();
        //printd(5, "qore_class_private::parseRollback() '%s' deleted all vars: %d\n", name.c_str(), vars.empty());
    }

    // set flags
    if (pending_has_public_memdecl)
        pending_has_public_memdecl = false;

    has_new_user_changes = false;
}

QoreMethod::QoreMethod(const QoreClass* n_parent_class, MethodFunctionBase* n_func, bool n_static)
        : priv(new qore_method_private(n_parent_class, n_func, n_static)) {
}

QoreMethod::~QoreMethod() {
    delete priv;
}

bool QoreMethod::isUser() const {
    return priv->isUniquelyUser();
}

bool QoreMethod::isBuiltin() const {
    return !priv->isUniquelyUser();
}

bool QoreMethod::isPrivate() const {
    return priv->func->isUniquelyPrivate();
}

ClassAccess QoreMethod::getAccess() const {
    return priv->func->getAccess();
}

bool QoreMethod::isStatic() const {
    return priv->static_flag;
}

const char* QoreMethod::getName() const {
    return priv->getName();
}

const std::string& QoreMethod::getNameStr() const {
    return priv->getNameStr();
}

const QoreClass* QoreMethod::getClass() const {
    return priv->parent_class;
}

const char* QoreMethod::getClassName() const {
    return priv->parent_class->getName();
}

void QoreMethod::assign_class(const QoreClass* p_class) {
    assert(!priv->parent_class);
    priv->parent_class = p_class;
}

QoreValue QoreMethod::execManaged(QoreObject* self, const QoreListNode* args, ExceptionSink* xsink) const {
    // to ensure the object does not get referenced for the call
    ObjectSubstitutionHelper osh(self, qore_class_private::get(*priv->parent_class));
    RuntimeConfig& rc = rc_get_current_ref();
    return priv->eval(xsink, rc, self, args);
}

method_type_e QoreMethod::getMethodType() const {
    // set type
    if (isStatic()) {
        return MT_Static;
    }
    if (dynamic_cast<const ConstructorMethodFunction*>(priv->func)) {
        return MT_Constructor;
    }
    if (dynamic_cast<const DestructorMethodFunction*>(priv->func)) {
        return MT_Destructor;
    }
    if (dynamic_cast<const CopyMethodFunction*>(priv->func)) {
        return MT_Copy;
    }
    return MT_Normal;
}

// only called for ::methodGate() and ::memberGate() which cannot be overloaded
bool QoreMethod::inMethod(const QoreObject* self) const {
    return ::runtime_in_object_method(priv->func->getName(), self);
}

QoreMethod* QoreMethod::copy(const QoreClass* p_class) const {
    return new QoreMethod(p_class, priv->func->copy(p_class), priv->static_flag);
}

const QoreTypeInfo* QoreMethod::getUniqueReturnTypeInfo() const {
    return priv->getUniqueReturnTypeInfo();
}

static const QoreClass* getStackClass() {
    const qore_class_private* qc = runtime_get_class();
    return qc ? qc->cls : nullptr;
}

void QoreClass::addMember(const char* mname, ClassAccess access, const QoreTypeInfo* n_typeInfo,
        QoreValue initial_value) {
    priv->addMember(mname, access, n_typeInfo, initial_value);
}

BCSMList::BCSMList(const BCSMList& old) {
    reserve(old.size());
    for (auto& i : old) {
        push_back(i);
        i.first->priv->ref();
    }
}

BCSMList::~BCSMList() {
    for (auto& i : *this) {
        i.first->priv->deref(false, false);
    }
}

void BCSMList::processMemberInitializationList(const QoreMemberMap& members, member_init_list_t& member_init_list) {
    for (auto& i : *this) {
        assert(i.first);

        //printd(5, "BCSMList::processMemberInitializationList() processing %p '%s'\n", i.first->priv,
        //    i.first->getName());
        for (auto& mi : i.first->priv->members.member_list) {
            // skip imported members
            if (!mi.second->local()) {
                //printd(5, "BCSMList::processMemberInitializationList() %p '%s::%s' NOT LOCAL\n", i.first->priv,
                //    i.first->getName(), mi.first);
                continue;
            }
            // find corresponding member in derived class
            const QoreMemberInfo* info = members.find(mi.first);
            if (!info) {
                //printd(5, "BCSMList::processMemberInitializationList() %p '%s::%s' NOT FOUND\n", i.first->priv,
                //    i.first->getName(), mi.first);
                // in case of dependency injections, the member may not be found in the class
                continue;
            }

            const qore_class_private* member_class_ctx = info->getClassContext(i.first->priv);
            //printd(5, "BCSMList::processMemberInitializationList() adding '%s::%s' ctx: %p '%s'\n",
            //    i.first->getName(), mi.first, member_class_ctx,
            //    member_class_ctx ? member_class_ctx->name.c_str() : "n/a");
            // insert this entry with the class context for saving against the object
            member_init_list.push_back(member_init_entry_t(mi.first, mi.second, member_class_ctx));
        }
        //printd(5, "BCSMList::processMemberInitializationList() done processing %p '%s'\n", i.first->priv,
        //    i.first->getName());
    }
}

void BCSMList::alignBaseClassesInSubclass(QoreClass* thisclass, QoreClass* child, bool is_virtual) {
    //printd(5, "BCSMList::alignBaseClassesInSubclass(this: %s, sc: %s) size: %d\n", thisclass->getName(),
    //    sc->getName());
    // we must iterate with offsets, because the vector can be reallocated during this iteration
    for (unsigned i = 0; i < (*this).size(); ++i) {
        bool virt = is_virtual || (*this)[i].second;
        //printd(5, "BCSMList::alignBaseClassesInSubclass() %s child: %s virt: %d\n", thisclass->getName(),
        //    sc->getName(), virt);
        child->priv->scl->sml.align(child, (*this)[i].first, virt);
    }
}

void BCSMList::align(QoreClass* thisclass, QoreClass* qc, bool is_virtual) {
    assert(thisclass->getID() != qc->getID());

    // see if class already exists in vector
    for (auto& i : *this) {
        if (i.first->getID() == qc->getID()) {
            return;
        }
        assert(i.first->getID() != thisclass->getID());
    }
    qc->priv->ref();

    // append to the end of the vector
    //printd(5, "BCSMList::align() adding %p '%s' (virt: %d) as a base class of %p '%s'\n", qc, qc->getName(),
    //    is_virtual, thisclass, thisclass->getName());
    push_back(std::make_pair(qc, is_virtual));
}

int BCSMList::addBaseClassesToSubclass(QoreClass* thisclass, QoreClass* sc, bool is_virtual) {
    //printd(5, "BCSMList::addBaseClassesToSubclass(this: %s, sc: %s)\n", thisclass->getName(), sc->getName());
    for (class_list_t::const_iterator i = begin(), e = end(); i != e; ++i) {
        //printd(5, "BCSMList::addBaseClassesToSubclass() %s sc: %s is_virt: %d\n", thisclass->getName(), sc->getName(), is_virtual);
        if (sc->priv->scl->sml.add(thisclass, (*i).first, is_virtual || (*i).second)) {
            //printd(5, "BCSMList::addBaseClassesToSubclass() %s sc: %s is_virt: %d FAILED\n", thisclass->getName(), sc->getName(), is_virtual);
            return -1;
        }
    }
    return 0;
}

int BCSMList::add(QoreClass* thisclass, QoreClass* qc, bool is_virtual) {
    if (thisclass->getID() == qc->getID()) {
        thisclass->priv->scl->valid = false;
        parse_error(*thisclass->priv->loc, "class '%s' cannot inherit itself", thisclass->getName());
        return -1;
    }

    // see if class already exists in list
    class_list_t::const_iterator i = begin();
    while (i != end()) {
        if (i->first->getID() == qc->getID())
            return 0;
        if (i->first->getID() == thisclass->getID()) {
            thisclass->priv->scl->valid = false;
            parse_error(*thisclass->priv->loc, "circular reference in class hierarchy, '%s' is an ancestor of itself",
                thisclass->getName());
            return -1;
        }
        ++i;
    }
    qc->priv->ref();

    // append to the end of the list
    push_back(std::make_pair(qc, is_virtual));

    return 0;
}

void BCSMList::execDestructors(QoreObject* o, RuntimeConfig& rc, ExceptionSink* xsink) const {
    for (class_list_t::const_reverse_iterator i = rbegin(), e = rend(); i != e; ++i) {
        //printd(5, "BCSMList::execDestructors() %s::destructor() o: %p virt: %s (subclass %s)\n", i->first->getName(), o, i->second ? "true" : "false", o->getClass()->getName());
        if (!i->second)
            i->first->priv->execBaseClassDestructor(o, rc, xsink);
    }
}

void BCSMList::execSystemDestructors(QoreObject* o, RuntimeConfig& rc, ExceptionSink* xsink) const {
    for (class_list_t::const_reverse_iterator i = rbegin(), e = rend(); i != e; ++i) {
        //printd(5, "BCSMList::execSystemDestructors() %s::destructor() o: %p virt: %s (subclass %s)\n", i->first->getName(), o, i->second ? "true" : "false", o->getClass()->getName());
        if (!i->second)
            i->first->priv->execBaseClassSystemDestructor(o, rc, xsink);
    }
}

void BCSMList::execCopyMethods(QoreObject* self, QoreObject* old, RuntimeConfig& rc, ExceptionSink* xsink) const {
    for (class_list_t::const_iterator i = begin(), e = end(); i != e; ++i) {
        if (!i->second) {
            i->first->priv->execBaseClassCopy(self, old, rc, xsink);
            if (xsink->isEvent())
                break;
        }
    }
}

const QoreClass* BCSMList::getClass(qore_classid_t cid) const {
    for (class_list_t::const_iterator i = begin(), e = end(); i != e; ++i) {
        if (i->first->getID() == cid)
            return i->first;
    }
    return nullptr;
}

void BCSMList::resolveCopy() {
    for (class_list_t::iterator i = begin(), e = end(); i != e; ++i) {
        assert(i->first->priv->new_copy);
        i->first = i->first->priv->new_copy;
    }
}

QoreClass::QoreClass() : priv(nullptr) {
}

QoreClass::QoreClass(const QoreClass& old) : priv(old.priv) {
    assert(priv->typeInfo);
    assert(priv->orNothingTypeInfo);

    priv->pgmRef();

    // ensure atomicity when writing to qcset
    AutoLocker al(priv->gate.asl_lock);
    priv->qcset.insert(this);
}

QoreClass::QoreClass(std::string&& nme, std::string&& ns_path, int64 dom)
        : priv(new qore_class_private(this, std::move(nme), std::move(ns_path), dom)) {
    assert(priv->typeInfo);
    assert(priv->orNothingTypeInfo);
}

QoreClass::QoreClass(const char* nme, const char* ns_path, int64 dom)
        : priv(new qore_class_private(this, std::string(nme), std::string(ns_path), dom)) {
    assert(priv->typeInfo);
    assert(priv->orNothingTypeInfo);
}

QoreClass::QoreClass(const char* nme, const char* ns_path, int64 dom, const QoreTypeInfo* typeInfo)
        : priv(new qore_class_private(this, std::string(nme), std::string(ns_path), dom,
        const_cast<QoreClassTypeInfo*>(reinterpret_cast<const QoreClassTypeInfo*>(typeInfo)))) {
    assert(priv->typeInfo);
    assert(priv->orNothingTypeInfo);
}

QoreClass* QoreClass::copyImport() {
    return new QoreClass;
}

QoreClass* QoreClass::copy() {
    return new QoreClass(*this);
}

QoreClass::~QoreClass() {
    // dereference the private data if still present
    if (priv) {
        {
            // ensure atomicity when writing to qcset
            AutoLocker al(priv->gate.asl_lock);
            qc_set_t::iterator i = priv->qcset.find(this);
            if (i != priv->qcset.end()) {
                priv->qcset.erase(i);
            }
        }

        priv->deref(true, true, true);
    }
}

void QoreClass::setUserData(const void *n_ptr) {
   priv->setUserData(n_ptr);
}

const void *QoreClass::getUserData() const {
   return priv->getUserData();
}

void QoreClass::setManagedUserData(AbstractQoreClassUserData *n_mud) {
   priv->setManagedUserData(n_mud);
}

AbstractQoreClassUserData* QoreClass::getManagedUserData() const {
   return priv->getManagedUserData();
}

const QoreClass* QoreClass::getClass(qore_classid_t cid) const {
    if (cid == priv->classID)
        return this;
    return priv->scl ? priv->scl->sml.getClass(cid) : nullptr;
}

const QoreClass* QoreClass::getClass(qore_classid_t cid, bool& cpriv) const {
    ClassAccess access = Public;
    const QoreClass* qc = priv->getClassIntern(cid, access, true);
    cpriv = (access > Public);
    return qc;
}

const QoreClass* QoreClass::getClass(const QoreClass& qc, bool& cpriv) const {
    ClassAccess access = Public;
    const QoreClass* rv = priv->getClassIntern(*qc.priv, access, true);
    cpriv = (access > Public);
    return rv;
}

bool QoreClass::inHierarchy(const QoreClass& cls, ClassAccess& n_access) const {
    n_access = Public;
    return priv->inHierarchy(*cls.priv, n_access);
}

bool QoreClass::inHierarchyStrict(const QoreClass& cls, ClassAccess& n_access) const {
    n_access = Public;
    return priv->inHierarchyStrict(*cls.priv, n_access);
}

bool QoreClass::hasTransientMember() const {
    return priv->has_transient_member;
}

const char* QoreClass::getModuleName() const {
    return priv->getModuleName();
}

bool QoreMethod::existsVariant(const type_vec_t &paramTypeInfo) const {
    return priv->func->existsVariant(paramTypeInfo);
}

const QoreExternalMethodFunction* QoreMethod::getFunction() const {
    return reinterpret_cast<const QoreExternalMethodFunction*>(priv->func);
}

const QoreClass* qore_class_private::parseGetClass(const qore_class_private& qc, ClassAccess& n_access) const {
    n_access = Public;
    const_cast<qore_class_private*>(this)->parseResolveHierarchy();
    if (parseEqual(qc))
        return (QoreClass*)cls;
    return scl ? scl->parseGetClass(qc, n_access, true) : nullptr;
}

bool qore_class_private::runtimeHasCallableMethod(const char* m, int mask) const {
    if (!initialized) {
        return false;
    }
    const qore_class_private* class_ctx = runtime_get_class();
    if (class_ctx && !runtimeCheckPrivateClassAccess(class_ctx)) {
        class_ctx = nullptr;
    }

    const QoreMethod* w = nullptr;
    ClassAccess access;
    CurrentProgramRuntimeParseContextHelper pch;

    if (mask & QCCM_NORMAL) {
        w = runtimeFindCommittedMethod(m, access, class_ctx);
    }

    if (!w && (mask & QCCM_STATIC)) {
        w = runtimeFindCommittedStaticMethod(m, access, class_ctx);
    }

    return !w || (!class_ctx && (access > Public)) ? false : true;
}

const QoreMethod* qore_class_private::runtimeFindCommittedStaticMethod(const char* nme, ClassAccess& access,
        const qore_class_private* class_ctx) const {
    access = Public;
    if (!initialized) {
        return nullptr;
    }
    if (class_ctx && class_ctx != this) {
        const QoreMethod* m = class_ctx->findLocalCommittedStaticMethod(nme);
        if (m && qore_method_private::get(*m)->getFunction()->hasPrivateInternalVariants()) {
            return m;
        }
    }
    return runtimeFindCommittedStaticMethodIntern(nme, access, class_ctx);
}

const QoreMethod* qore_class_private::runtimeFindCommittedMethod(const char* nme, ClassAccess& access,
        const qore_class_private* class_ctx) const {
    access = Public;
    if (!initialized) {
        return nullptr;
    }
    if (class_ctx && class_ctx != this) {
        const QoreMethod* m = class_ctx->findLocalCommittedMethod(nme);
        if (m && qore_method_private::get(*m)->getFunction()->hasPrivateInternalVariants()) {
            return m;
        }
    }
    return runtimeFindCommittedMethodIntern(nme, access, class_ctx);
}

const QoreMethod* qore_class_private::runtimeFindCommittedMethodForEval(const char* nme, ClassAccess& access,
        const qore_class_private* class_ctx) const {
    access = Public;
    if (!initialized) {
        return nullptr;
    }
    if (class_ctx && class_ctx != this) {
        const QoreMethod* m = class_ctx->findLocalCommittedMethod(nme);
        if (m) {
            const qore_method_private* meth = qore_method_private::get(*m);
            //printd(5, "qore_class_private::runtimeFindCommittedMethodForEval() '%s::%s': %p (pi: %d a: %d)\n", name.c_str(), nme, meth, meth->getFunction()->hasPrivateInternalVariants(), meth->isAbstract());
            if (meth->getFunction()->hasPrivateInternalVariants() && !meth->isAbstract()) {
                return m;
            }
        }
    }
    return runtimeFindCommittedMethodIntern(nme, access, class_ctx);
}

const QoreMethod* qore_class_private::getMethodForEval(const char* nme, QoreProgram* pgm,
        const qore_class_private* class_ctx, ExceptionSink* xsink) const {
    if (!initialized) {
        return nullptr;
    }

    //printd(5, "qore_class_private::getMethodForEval() %s::%s() %s call attempted\n", name.c_str(), nme, runtimeCheckPrivateClassAccess() ? "external" : "internal" );

    const QoreMethod* w;
    ClassAccess access;

    //printd(5, "qore_class_private::getMethodForEval() %s::%s() class_ctx: %p %s\n", name.c_str(), nme, class_ctx, class_ctx ? class_ctx->name.c_str() : "n/a");

    {
        ProgramRuntimeParseContextHelper pch(xsink, pgm);
        if (*xsink) {
            return nullptr;
        }

        if (!(w = runtimeFindCommittedMethodForEval(nme, access, class_ctx))
            && !(w = runtimeFindCommittedStaticMethod(nme, access, class_ctx))) {
            return nullptr;
        }
    }

    //printd(5, "QoreClass::getMethodForEval() %s::%s() found method %p class %s\n", name.c_str(), nme, w, w->getClassName());

    // check for illegal explicit call
    if (w == constructor || w == destructor) {
        xsink->raiseException("ILLEGAL-EXPLICIT-METHOD-CALL", "explicit calls to ::%s() methods are not allowed", nme);
        return nullptr;
    }

    if (w->isPrivate() && !class_ctx) {
        xsink->raiseException("METHOD-IS-PRIVATE", "%s::%s() is private and cannot be accessed externally", name.c_str(), nme);
        return nullptr;
    } else if ((access > Public) && !class_ctx) {
        xsink->raiseException("BASE-CLASS-IS-PRIVATE", "%s() is a method of a privately-inherited class %s", nme, name.c_str());
        return nullptr;
    }

    return w;
}

bool qore_class_private::runtimeIsPrivateMemberIntern(const char* str, bool toplevel) const {
    if (!initialized) {
        return false;
    }

    QoreMemberInfo* info = members.find(str);
    if (info && (toplevel || !info->isLocalInternal())) {
        ClassAccess ma = info->getAccess();
        return ma > Public;
    }

    return !scl ? false : scl->runtimeIsPrivateMember(str, toplevel);
}

QoreValue qore_class_private::evalMethodGate(QoreObject* self, const char* nme, const QoreListNode* args, ExceptionSink* xsink) const {
    printd(5, "qore_class_private::evalMethodGate() method: %s args: %p\n", nme, args);

    ReferenceHolder<QoreListNode> args_holder(xsink);

    // build new argument list
    if (args) {
        if (args->needs_eval()) {
            args_holder = args->evalList(xsink);
        } else {
            args_holder = args->copy();
        }
        if (*xsink) {
            return QoreValue();
        }
    } else {
        args_holder = new QoreListNode(autoTypeInfo);
    }

    if (gate_access) {
        args_holder->insert(runtimeCheckPrivateClassAccess() ? true : false, nullptr);
    }

    args_holder->insert(new QoreStringNode(nme), nullptr);

    return self->evalMethod(*methodGate, *args_holder, xsink);
}

void qore_class_private::execMemberNotification(QoreObject* self, const char* mem, ExceptionSink* xsink) const {
    // cannot run this method when executing from within the class
    assert((cls != getStackClass()));
    //printd(5, "qore_class_private::execMemberNotification() member: %s\n", mem);
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
    args->push(new QoreStringNode(mem), nullptr);
    self->evalMethod(*memberNotification, *args, xsink).discard(xsink);
}

bool QoreClass::isPrivateMember(const char* str) const {
    return priv->runtimeIsPrivateMemberIntern(str, true);
}

bool QoreClass::hasPublicMembersInHierarchy() const {
    return priv->has_public_memdecl;
}

QoreObject* QoreClass::execConstructor(const QoreListNode* args, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return priv->execConstructor(xsink, rc, nullptr, args);
}

QoreObject* QoreClass::execConstructor(const QoreClass& obj_cls, const QoreListNode* args, bool allow_abstract,
        ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return priv->execConstructor(xsink, rc, nullptr, args, &obj_cls, allow_abstract);
}

QoreObject* QoreClass::execConstructorVariant(const QoreExternalMethodVariant* mv, const QoreListNode *args,
        ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return priv->execConstructor(xsink, rc, reinterpret_cast<const ConstructorMethodVariant*>(mv), args);
}

QoreValue qore_class_private::evalMemberGate(QoreObject* self, const char* nme, ExceptionSink* xsink) const {
    assert(nme);
    printd(5, "qore_class_private::evalMemberGate() member: %s\n", nme);
    // do not run memberGate method if we are already in it...
    if (!memberGate || memberGate->inMethod(self))
        return QoreValue();

    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
    args->push(new QoreStringNode(nme), nullptr);
    if (gate_access)
        args->push(runtimeCheckPrivateClassAccess() ? true : false, nullptr);

    return self->evalMethod(*memberGate, *args, xsink);
}

QoreValue qore_class_private::evalMethod(QoreObject* self, const char* nme, const QoreListNode* args,
        const qore_class_private* class_ctx, RuntimeConfig& rc, ExceptionSink* xsink) const {
    QORE_TRACE("qore_class_private::evalMethod()");
    assert(self);

    if (!strcmp(nme, "copy")) {
        if (args) {
            xsink->raiseException("COPY-ERROR", "while calling %s::copy(): it is illegal to pass arguments to copy methods", self->getClassName());
            return QoreValue();
        }
        return execCopy(self, xsink);
    }

    const QoreMethod* w = getMethodForEval(nme, self->getProgram(), class_ctx, xsink);
    if (*xsink) {
        return QoreValue();
    }

    if (w) {
        return qore_method_private::eval(*w, xsink, rc, self, args, class_ctx);
    }

    const QoreMemberInfo* mi = runtimeGetMemberInfo(nme, class_ctx);
    if (mi) {
        const qore_class_private* member_class_ctx = mi->getClassContext(class_ctx);
        qore_object_private* obj_priv = qore_object_private::get(*self);
        QoreValue mv{};
        bool exists = false;
        {
            QoreAutoVarRWReadLocker al(obj_priv->rml);
            if (obj_priv->status == OS_DELETED) {
                return QoreValue();
            }
            const QoreHashNode* odata = member_class_ctx ? obj_priv->getInternalData(member_class_ctx)
                : obj_priv->data;
            if (odata) {
                mv = qore_hash_private::get(*odata)->getReferencedKeyValueIntern(nme, exists);
            }
        }

        if (exists && (mv.getType() == NT_FUNCREF || mv.getType() == NT_RUNTIME_CLOSURE)) {
            ValueHolder mvh(mv, xsink);
            if (*xsink) {
                return QoreValue();
            }
            const ResolvedCallReferenceNode* ref =
                dynamic_cast<const ResolvedCallReferenceNode*>(mv.getInternalNode());
            if (ref) {
                return ref->execValue(args, xsink);
            }
        }
    }

    // first see if there is a pseudo-method for this
    QoreClass* qc = nullptr;
    w = pseudo_classes_find_method(NT_OBJECT, nme, qc);
    if (w) {
        return qore_method_private::evalPseudoMethod(*w, xsink, rc, nullptr, self, args);
    } else if (methodGate && !methodGate->inMethod(self)) { // call methodGate with unknown method name and arguments
        return evalMethodGate(self, nme, args, xsink);
    }

    xsink->raiseException("METHOD-DOES-NOT-EXIST", "no method %s::%s() has been defined and no pseudo-method " \
        "<object>::%s() is available (class lang: %s)", self->getClassName(), nme, nme,
        self->getClass()->getLanguage());
    return QoreValue();
}

QoreObject* qore_class_private::execSystemConstructor(QoreObject* self, int code, va_list args) const {
    assert(system_constructor);
    const_cast<qore_class_private*>(this)->initialize();
    // no lock is sent with constructor, because no variable has been assigned yet
    system_constructor->priv->evalSystemConstructor(self, code, args);
    return self;
}

QoreObject* QoreClass::execSystemConstructor(int code, ...) const {
    va_list args;

    // create new object
    QoreObject* o = new QoreObject(this, nullptr);

    va_start(args, code);
    priv->execSystemConstructor(o, code, args);
    va_end(args);

    printd(5, "QoreClass::execSystemConstructor() %s::execSystemConstructor() returning %p\n", priv->name.c_str(), o);
    return o;
}

void qore_class_private::execDestructor(QoreObject* self, RuntimeConfig& rc, ExceptionSink* xsink) const {
    //printd(5, "qore_class_private::execDestructor() %s::destructor() o: %p scl: %p sml: %p, self: %p, "
    //  destructor: %p, isSystemObject: %d\n", name.c_str(), self, scl, scl ? &scl->sml : 0, self, destructor,
    //  self->isSystemObject());

    // we use a new, blank exception sink to ensure all destructor code gets executed
    // in case there were already exceptions in the current exceptionsink
    ExceptionSink de;

    if (self->isSystemObject()) {
        if (destructor) {
            destructor->priv->evalSystemDestructor(self, rc, &de);
        } else {
            self->defaultSystemDestructor(classID, &de);
        }
        // execute superclass destructors
        if (scl) {
            scl->sml.execSystemDestructors(self, rc, &de);
        }
    } else {
        if (destructor) {
            destructor->priv->evalDestructor(self, rc, &de);
        } else if (sys) {
            self->defaultSystemDestructor(classID, &de);
        }
        // execute superclass destructors
        if (scl) {
            scl->sml.execDestructors(self, rc, &de);
        }
    }

    assert(xsink || !de);
    if (xsink) {
        xsink->assimilate(de);
    }
}

void qore_class_private::execBaseClassDestructor(QoreObject* self, RuntimeConfig& rc, ExceptionSink* xsink) const {
    // we use a new, blank exception sink to ensure all destructor code gets executed
    // in case there were already exceptions in the current exceptionsink
    ExceptionSink de;
    //printd(5, "qore_class_private::execBaseClassDestructor() %s::destructor(), destructor: %p, sys: %d\n",
    //  name.c_str(), destructor, sys);
    if (destructor)
        destructor->priv->evalDestructor(self, rc, &de);
    else if (sys)
        self->defaultSystemDestructor(classID, &de);

    assert(xsink || !de);
    if (xsink) {
        xsink->assimilate(de);
    }
}

void qore_class_private::execBaseClassSystemDestructor(QoreObject* self, RuntimeConfig& rc,
        ExceptionSink* xsink) const {
    // we use a new, blank exception sink to ensure all destructor code gets executed
    // in case there were already exceptions in the current exceptionsink
    ExceptionSink de;
    if (destructor)
        destructor->priv->evalSystemDestructor(self, rc, &de);
    else if (sys)
        self->defaultSystemDestructor(classID, &de);

    assert(xsink || !de);
    if (xsink) {
        xsink->assimilate(de);
    }
}

void qore_class_private::execBaseClassCopy(QoreObject* self, QoreObject* old, RuntimeConfig& rc,
        ExceptionSink* xsink) const {
    if (copyMethod) {
        copyMethod->priv->evalBaseClassCopy(self, old, rc, xsink);
    }
}

QoreObject* QoreClass::execCopy(QoreObject* old, ExceptionSink* xsink) const {
    return priv->execCopy(old, xsink);
}

QoreObject* qore_class_private::execCopy(QoreObject* old, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    // check for illegal private calls
    if (copyMethod && copyMethod->isPrivate() && cls != getStackClass()) {
        xsink->raiseException("METHOD-IS-PRIVATE", "%s::copy() is private and cannot be accessed externally",
            name.c_str());
        return nullptr;
    }

    ReferenceHolder<QoreObject> self(new QoreObject(cls, old->getProgram(), (QoreHashNode*)nullptr), xsink);
    qore_object_private* self_priv = qore_object_private::get(**self);
    qore_object_private* old_priv = qore_object_private::get(*old);
    if (self_priv->copyData(xsink, *old_priv)) {
        return nullptr;
    }

    if (copyMethod) {
        copyMethod->priv->evalCopy(*self, old, rc, xsink);
    } else if (scl) { // execute superclass copy methods
        scl->sml.execCopyMethods(*self, old, rc, xsink);
    }

    return *xsink ? nullptr : self.release();
}

int qore_class_private::addBaseClassesToSubclass(QoreClass* child, bool is_virtual) {
    //printd(5, "qore_class_private::addBaseClassesToSubclass() this: %p '%s' sc: %p '%s' is_virtual: %d scl: %p\n",
    //  this, name.c_str(), sc, sc->getName(), is_virtual, scl);
    if (scl && scl->addBaseClassesToSubclass(cls, child, is_virtual)) {
        return -1;
    }
    assert(child->priv->scl);
    return child->priv->scl->sml.add(child, cls, is_virtual);
}

int qore_class_private::addUserMethod(const char* mname, MethodVariantBase* f, bool n_static) {
    // FIXME: set class name at parse time
    const char* tname = name.c_str();
    printd(5, "QoreClass::addUserMethod(%s, umv: %p, priv: %d, static: %d) this: %p %s\n", mname, f, f->isPrivate(),
        n_static, this, tname);

    std::unique_ptr<MethodVariantBase> func(f);

    if (sys || committed) {
        parseCheckSystemCommitted(static_cast<UserSignature*>(f->getSignature())->getParseLocation());
        return -1;
    }

    if (f->isAbstract()) {
        if (initialized) {
            parseException(*static_cast<UserSignature*>(f->getSignature())->getParseLocation(),
                "ILLEGAL-ABSTRACT-METHOD", "abstract %s::%s(): abstract methods cannot be added to a class once " \
                "the class has been committed", name.c_str(), mname);
            return -1;
        }
        if (n_static) {
            parseException(*static_cast<UserSignature*>(f->getSignature())->getParseLocation(),
                "ILLEGAL-ABSTRACT-METHOD", "abstract %s::%s(): abstract methods cannot be static", name.c_str(),
                mname);
            return -1;
        }
    }

    bool dst = !strcmp(mname, "destructor");
    bool con = dst ? false : !strcmp(mname, "constructor");

    // check for illegal static method
    if (n_static) {
        if ((con || dst || checkSpecialStaticIntern(mname))) {
            parseException(*static_cast<UserSignature*>(f->getSignature())->getParseLocation(),
                "ILLEGAL-STATIC-METHOD", "%s methods cannot be static", mname);
            return -1;
        }
    }

    bool cpy = dst || con ? false : !strcmp(mname, "copy");
    // check for illegal method overloads
    if (sys && (con || cpy)) {
        parseException(*static_cast<UserSignature*>(f->getSignature())->getParseLocation(), "ILLEGAL-METHOD-OVERLOAD",
            "class %s is builtin; %s methods in builtin classes cannot be overloaded; create a subclass instead",
            name.c_str(), mname);
        return -1;
    }

    // set flags for other special methods
    bool methGate, memGate, hasMemberNotification;
    if (dst || con || cpy) {
        methGate = memGate = hasMemberNotification = false;
        // issue #3126: cannot add abstract variants of special methods
        if (f->isAbstract() && (con || dst || cpy)) {
            parseException(*static_cast<UserSignature*>(f->getSignature())->getParseLocation(),
                "ILLEGAL-ABSTRACT-METHOD", "in class %s: %s() methods cannot be abstract", tname, mname);
            return -1;
        }
    } else {
        methGate = !strcmp(mname, "methodGate");
        memGate = methGate ? false : !strcmp(mname, "memberGate");
        hasMemberNotification = methGate || memGate ? false : !strcmp(mname, "memberNotification");
    }

    // we cannot initialize the class here
    QoreMethod* m = const_cast<QoreMethod*>(!n_static ? parseFindLocalMethod(mname) : parseFindLocalStaticMethod(mname));
    if (!n_static && m && (dst || cpy || methGate || memGate || hasMemberNotification)) {
        parseException(*static_cast<UserSignature*>(f->getSignature())->getParseLocation(), "ILLEGAL-METHOD-OVERLOAD",
            "a %s::%s() method has already been defined; cannot overload %s methods", tname, mname, mname);
        return -1;
    }

    // now we add the new variant to a method, creating the method if necessary

    if (!has_new_user_changes) {
        has_new_user_changes = true;
    }
    if (!has_sig_changes) {
        has_sig_changes = true;
    }

    bool is_new = false;
    // if the method does not exist, then create it
    if (!m) {
        is_new = true;
        MethodFunctionBase* mfb;
        if (con) {
            mfb = new ConstructorMethodFunction(cls);
            // selfid is set below for all non-static methods
            // set selfid immediately if adding a constructor variant
            //reinterpret_cast<UserConstructorVariant*>(f)->getUserSignature()->setSelfId(&selfid);
            assert(!f->isAbstract());
        } else if (dst) {
            mfb = new DestructorMethodFunction(cls);
            assert(!f->isAbstract());
        } else if (cpy) {
            mfb = new CopyMethodFunction(cls);
            assert(!f->isAbstract());
        } else if (n_static) {
            mfb = new StaticUserMethod(cls, mname);
        } else {
            mfb = new NormalUserMethod(cls, mname);
        }

        m = new QoreMethod(cls, mfb, n_static);
    }

    // add this variant to the method
    if (m->priv->addUserVariant(func.release())) {
        if (is_new)
            delete m;
        return -1;
    }

    //printd(5, "qore_class_private::addUserMethod() %s %s::%s(%s) f: %p (%d) new: %d\n", privpub(f->getAccess()),
    //  tname, mname, f->getSignature()->getSignatureText(), f, ((QoreReferenceCounter*)f)->reference_count(),
    //  is_new);

    // set the pointer from the variant back to the owning method
    if (n_static) {
        f->setMethod(m);
    } else {
        f->setNormalUserMethod(m, &selfid);
    }

    // add the new method to the class if it's a new method
    if (is_new) {
        if (n_static) {
            shm[m->getName()] = m;
        } else {
            hm[m->getName()] = m;
        }
    }

    // add this variant to the abstract map if it's an abstract variant
    if (f->isAbstract()) {
        assert(!n_static);
        if (!f->hasBody()) {
            //printd(5, "qore_class_private::addUserMethod() this: %p adding abstract method variant %s::%s()\n",
            //  this, name.c_str(), mname);
            ahm.parseAddAbstractVariant(mname, f);
        } else {
            // this is a concrete method variant that must have an abstract implementation in a parent class
            f->clearAbstract();
            ahm.parseOverrideAbstractVariant(mname, f);
        }
    } else if (!n_static && !con && !dst)
        ahm.parseOverrideAbstractVariant(mname, f);

    return 0;
}

void QoreClass::addMethod(const char* nme, q_method_t m, ClassAccess access, int64 flags, int64 domain,
        const QoreTypeInfo* returnTypeInfo, unsigned num_params, ...) {
    type_vec_t typeList;
    arg_vec_t defaultArgList;
    name_vec_t nameList;
    if (num_params) {
        va_list args;
        va_start(args, num_params);
        qore_process_params(num_params, typeList, defaultArgList, nameList, args);
        va_end(args);
    }

    priv->addBuiltinMethod(nme, new BuiltinNormalMethodValueVariant(m, access, false, flags, domain, returnTypeInfo,
        typeList, defaultArgList, nameList));
}

void QoreClass::addMethod(const void* ptr, const char* nme, q_external_method_t m, ClassAccess access, int64 flags,
        int64 domain, const QoreTypeInfo* returnTypeInfo, const type_vec_t& typeList, const arg_vec_t& defaultArgList,
        const name_vec_t& nameList) {
    priv->addBuiltinMethod(nme, new BuiltinExternalNormalMethodValueVariant(ptr, m, access, false, flags, domain,
        returnTypeInfo, typeList, defaultArgList, nameList));
}

void QoreClass::addStaticMethod(const char* nme, q_func_t m, ClassAccess access, int64 flags, int64 domain,
        const QoreTypeInfo* returnTypeInfo, unsigned num_params, ...) {
    type_vec_t typeList;
    arg_vec_t defaultArgList;
    name_vec_t nameList;
    if (num_params) {
        va_list args;
        va_start(args, num_params);
        qore_process_params(num_params, typeList, defaultArgList, nameList, args);
        va_end(args);
    }

    priv->addBuiltinStaticMethod(nme, new BuiltinStaticMethodValueVariant(m, access, false, flags, domain,
        returnTypeInfo, typeList, defaultArgList, nameList));
}

void QoreClass::addStaticMethod(const void* ptr, const char* nme, q_external_static_method_t m, ClassAccess access,
        int64 flags, int64 domain, const QoreTypeInfo* returnTypeInfo, const type_vec_t& typeList,
        const arg_vec_t& defaultArgList, const name_vec_t& nameList) {
    priv->addBuiltinStaticMethod(nme, new BuiltinExternalStaticMethodValueVariant(ptr, m, access, false, flags,
        domain, returnTypeInfo, typeList, defaultArgList, nameList));
}

void QoreClass::addConstructor(q_constructor_t m, ClassAccess access, int64 n_flags, int64 n_domain,
        unsigned num_params, ...) {
    type_vec_t typeList;
    arg_vec_t defaultArgList;
    name_vec_t nameList;
    if (num_params) {
        va_list args;
        va_start(args, num_params);
        qore_process_params(num_params, typeList, defaultArgList, nameList, args);
        va_end(args);
    }
    priv->addBuiltinConstructor(new BuiltinConstructorValueVariant(m, access, n_flags, n_domain, typeList,
        defaultArgList, nameList));
}

void QoreClass::addConstructor(const void* ptr, q_external_constructor_t m, ClassAccess access, int64 n_flags,
        int64 n_domain, const type_vec_t& typeList, const arg_vec_t& defaultArgList, const name_vec_t& nameList) {
    priv->addBuiltinConstructor(new BuiltinExternalConstructorValueVariant(ptr, m, access, n_flags, n_domain,
        typeList, defaultArgList, nameList));
}

void QoreClass::addAbstractMethod(const char *n_name, ClassAccess access, int64 n_flags,
        const QoreTypeInfo* returnTypeInfo, unsigned num_params, ...) {
    type_vec_t typeList;
    arg_vec_t defaultArgList;
    name_vec_t nameList;
    if (num_params) {
        va_list args;
        va_start(args, num_params);
        qore_process_params(num_params, typeList, defaultArgList, nameList, args);
        va_end(args);
    }
    //printd(5, "QoreClass::addAbstractMethodVariantExtended3() %s::%s() num_params: %d\n", getName(), n_name,
    //  num_params);

    priv->addBuiltinMethod(n_name, new BuiltinAbstractMethodVariant(access, n_flags, returnTypeInfo, typeList,
        defaultArgList, nameList));
}

void QoreClass::addAbstractMethod(const char* n_name, ClassAccess access, int64 n_flags,
        const QoreTypeInfo* returnTypeInfo, const type_vec_t& typeList, const arg_vec_t& defaultArgList,
        const name_vec_t& nameList) {
    priv->addBuiltinMethod(n_name, new BuiltinAbstractMethodVariant(access, n_flags, returnTypeInfo, typeList,
        defaultArgList, nameList));
}

// sets a builtin function as class destructor - no duplicate checking is made
void QoreClass::setDestructor(q_destructor_t m) {
    priv->addBuiltinDestructor(new BuiltinDestructorVariant(m));
}

// sets a builtin function as class destructor - no duplicate checking is made
void QoreClass::setDestructor(const void *ptr, q_external_destructor_t m) {
    priv->addBuiltinDestructor(new BuiltinExternalDestructorVariant(ptr, m));
}

// sets a builtin function as class copy constructor - no duplicate checking is made
void QoreClass::setCopy(q_copy_t m) {
    priv->addBuiltinCopyMethod(new BuiltinCopyVariant(this, m));
}

// sets a builtin function as class copy constructor - no duplicate checking is made
void QoreClass::setCopy(const void* ptr, q_external_copy_t m) {
    priv->addBuiltinCopyMethod(new BuiltinExternalCopyVariant(ptr, this, m));
}

// sets the final flag
void QoreClass::setFinal() {
    qore_class_private::setFinal(*this);
}

void QoreClass::setSerializer(q_serializer_t m) {
    priv->setSerializer(m);
}

void QoreClass::setDeserializer(q_deserializer_t m) {
    priv->setDeserializer(m);
}

q_serializer_t QoreClass::getSerializer() const {
    return priv->serializer;
}

q_deserializer_t QoreClass::getDeserializer() const {
    return priv->deserializer;
}

void QoreClass::setSystemConstructor(q_system_constructor_t m) {
    priv->setBuiltinSystemConstructor(new BuiltinSystemConstructor(this, m));
}

QoreListNode* QoreClass::getMethodList() const {
    QoreListNode* l = new QoreListNode(stringTypeInfo);

    for (hm_method_t::const_iterator i = priv->hm.begin(), e = priv->hm.end(); i != e; ++i)
        l->push(new QoreStringNode(i->first), nullptr);
    return l;
}

QoreListNode* QoreClass::getStaticMethodList() const {
    QoreListNode* l = new QoreListNode(stringTypeInfo);

    for (hm_method_t::const_iterator i = priv->shm.begin(), e = priv->shm.end(); i != e; ++i)
        l->push(new QoreStringNode(i->first), nullptr);
    return l;
}

int qore_class_private::parseInitPartial() {
    if (parse_init_partial_called || sys) {
        return 0;
    }

    int err = parseResolveHierarchy();

    if (initialize() && !err) {
        err = -1;
    }

    if (initializeMembers() && !err) {
        err = -1;
    }

    // the class could be initialized out of line during initialize9)
    if (parse_init_partial_called) {
        return err;
    }

    NamespaceParseContextHelper nspch(cls->priv->ns);
    QoreParseClassHelper qpch(cls);
    if (parseInitPartialIntern() && !err) {
        err = -1;
    }

    return err;
}

int qore_class_private::parseInitPartialIntern() {
    assert(!parse_init_partial_called);
    parse_init_partial_called = true;

    //printd(5, "class_private::parseInitPartialIntern() this: %p '%s' scl: %p user_changes: %d\n", this,
    //  name.c_str(), scl, has_new_user_changes);

    int err = 0;

    // initialize parents first for abstract method handling
    if (scl) {
        for (bclist_t::iterator i = scl->begin(), e = scl->end(); i != e; ++i) {
            if ((*i)->sclass) {
                if ((*i)->sclass->priv->parseInitPartial() && !err) {
                    err = -1;
                }

                //printd(5, "qore_class_private::parseInitPartialIntern() this: %p '%s' merging base class abstract
                //  methods from %p '%s'\n", this, name.c_str(), (*i)->sclass, (*i)->sclass->getName());

                // copy pending abstract changes from parent classes to the local class

                // get parent's abstract method map
                AbstractMethodMap& mm = (*i)->sclass->priv->ahm;

                // iterate parent's method map and merge parent changes to our method map
                for (amap_t::iterator ai = mm.begin(), ae = mm.end(); ai != ae; ++ai) {
                    amap_t::iterator vi = ahm.find(ai->first);
                    printd(5, "qore_class_private::parseInitPartialIntern() this: %p '%s' checking '%s::%s()' "
                        "found: %d\n", this, name.c_str(), (*i)->sclass->getName(), ai->first.c_str(), vi != ahm.end());
                    if (vi != ahm.end()) {
                        vi->second->parseMergeBase(*(ai->second));
                        continue;
                    }
                    std::unique_ptr<AbstractMethod> m(new AbstractMethod(ahm.relaxed_match));
                    // see if there are pending normal variants...
                    hm_method_t::iterator mi = hm.find(ai->first);
                    printd(5, "qore_class_private::parseInitPartialIntern() this: %p '%s' looking for local "
                        "'%s': %d\n", this, name.c_str(), ai->first.c_str(), mi != hm.end());
                    m->parseMergeBase(*(ai->second), mi == hm.end()
                        ? nullptr
                        : qore_method_private::get(*mi->second)->getFunction());
                    if (!m->empty()) {
                        ahm.insert(amap_t::value_type(ai->first, m.release()));
                    }
                    printd(5, "qore_class_private::parseInitPartialIntern() this: %p '%s' insert abstract method "
                        "variant %s::%s()\n", this, name.c_str(), name.c_str(), ai->first.c_str());
                }
            }
        }
    }

    return err;
}

int qore_class_private::parseInitConstants() {
    if (parse_init_constants_called || sys) {
        return 0;
    }
    parse_init_constants_called = true;

    if (has_new_user_changes) {
        // initialize constants
        if (constlist.parseInit()) {
            return -1;
        }
    }

    return 0;
}

int qore_class_private::parseInit() {
    // make sure initialize() is called first
    int err = initialize();

    //printd(5, "qore_class_private::parseInit() this: %p '%s' parse_init_called: %d parse_init_partial_called: %d\n",
    //  this, name.c_str(), parse_init_called, parse_init_partial_called);
    if (parse_init_called || sys) {
        return err;
    }

    parse_init_called = true;

    if (has_new_user_changes) {
        NamespaceParseContextHelper nspch(cls->priv->ns);
        QoreParseClassHelper qpch(cls);

        //printd(5, "qore_class_private::parseInit() this: %p '%s' set cls ctx\n", this, name.c_str());

        if (!parse_init_partial_called && parseInitPartialIntern() && !err) {
            err = -1;
        }

        // initialize methods
        for (hm_method_t::iterator i = hm.begin(), e = hm.end(); i != e; ++i) {
            if (i->second->priv->parseInit() && !err) {
                err = -1;
            }
        }

        // initialize static methods
        for (hm_method_t::iterator i = shm.begin(), e = shm.end(); i != e; ++i) {
            if (i->second->priv->parseInitStatic() && !err) {
                err = -1;
            }
        }
    }

    if (parseInitConstants() && !err) {
        err = -1;
    }

    if (!err) {
        parseWarnAmbiguousOverloads();
    }

    //printd(5, "qore_class_private::parseInit() this: %p cls: %p %s scl: %p\n", this, cls, name.c_str(), scl);
    return err;
}

namespace {

bool is_special_method_name(const char* name) {
    return !strcmp(name, "constructor") || !strcmp(name, "destructor") || !strcmp(name, "copy")
        || !strcmp(name, "methodGate") || !strcmp(name, "memberGate") || !strcmp(name, "memberNotification");
}

void collect_base_method_names(const qore_class_private& qc, bool is_static, std::set<std::string>& names,
        std::unordered_set<qore_classid_t>& visited) {
    if (!visited.insert(qc.classID).second) {
        return;
    }

    const hm_method_t& map = is_static ? qc.shm : qc.hm;
    for (const auto& i : map) {
        if (is_special_method_name(i.second->getName())) {
            continue;
        }
        names.insert(i.first);
    }

    if (!qc.scl) {
        return;
    }

    for (auto& i : *qc.scl) {
        if (i->sclass) {
            collect_base_method_names(*qore_class_private::get(*i->sclass), is_static, names, visited);
        }
    }
}

bool local_overrides_all_base_variants(MethodFunctionBase& local_func, MethodFunctionBase& base_func,
        bool relaxed_match) {
    QoreFunctionIterator fiter(base_func);
    while (fiter.next()) {
        MethodVariantBase* base_variant = const_cast<MethodVariantBase*>(METHVB_const(fiter.getVariant()));
        if (local_func.parseHasVariantWithSignature(base_variant, relaxed_match)) {
            continue;
        }
        base_variant->parseResolveUserSignature();
        const AbstractFunctionSignature* base_sig = base_variant->getSignature();
        const char* base_sig_text = base_sig ? base_sig->getSignatureText() : nullptr;
        if (base_sig_text && *base_sig_text) {
            QoreFunctionIterator lifter(local_func);
            while (lifter.next()) {
                const AbstractQoreFunctionVariant* local_variant = lifter.getVariant();
                if (!local_variant) {
                    continue;
                }
                const_cast<AbstractQoreFunctionVariant*>(local_variant)->parseResolveUserSignature();
                const AbstractFunctionSignature* local_sig = local_variant->getSignature();
                const char* local_sig_text = local_sig ? local_sig->getSignatureText() : nullptr;
                if (local_sig_text && !strcmp(local_sig_text, base_sig_text)) {
                    base_sig_text = nullptr;
                    break;
                }
            }
            if (!base_sig_text) {
                continue;
            }
        }
        return false;
    }
    return true;
}

const QoreProgramLocation* get_method_warning_loc(const qore_class_private& qc, MethodFunctionBase& func) {
    if (func.numVariants()) {
        const AbstractQoreFunctionVariant* variant = func.first();
        if (variant) {
            const UserVariantBase* uvb = variant->getUserVariantBase();
            if (uvb && uvb->getUserSignature() && uvb->getUserSignature()->getParseLocation()) {
                return uvb->getUserSignature()->getParseLocation();
            }
        }
    }
    return qc.loc;
}

QoreString build_match_list(const std::vector<std::string>& matches) {
    QoreString desc;
    for (size_t i = 0; i < matches.size(); ++i) {
        if (i) {
            desc.concat(", ");
        }
        desc.concat("'");
        desc.concat(matches[i].c_str());
        desc.concat("'");
    }
    return desc;
}

std::vector<std::string> get_method_signature_list(const QoreMethod& method) {
    std::set<std::string> matches;
    MethodFunctionBase* func = qore_method_private::get(method)->getFunction();
    const QoreClass* mclass = method.getClass();
    const char* class_name = mclass ? mclass->getName() : "<unknown>";
    const char* method_name = method.getName();

    size_t added = 0;
    QoreFunctionIterator fiter(*func);
    while (fiter.next()) {
        const AbstractQoreFunctionVariant* variant = fiter.getVariant();
        const AbstractFunctionSignature* sig = variant ? variant->getSignature() : nullptr;
        if (sig && sig->getSignatureText()) {
            matches.insert(std::string(class_name) + "::" + method_name + "(" + sig->getSignatureText() + ")");
        } else {
            matches.insert(std::string(class_name) + "::" + method_name + "()");
        }
        ++added;
    }

    if (!added) {
        matches.insert(std::string(class_name) + "::" + method_name + "()");
    }

    return std::vector<std::string>(matches.begin(), matches.end());
}

void append_method_signatures(const QoreMethod& method, std::set<std::string>& matches) {
    const std::vector<std::string> sigs = get_method_signature_list(method);
    matches.insert(sigs.begin(), sigs.end());
}

QoreString build_method_overload_list(const QoreMethod& method) {
    const std::vector<std::string> sigs = get_method_signature_list(method);
    return build_match_list(sigs);
}

bool signatures_ambiguous(const AbstractFunctionSignature& a, const AbstractFunctionSignature& b, bool relaxed_match) {
    if (a.hasVarargs() != b.hasVarargs()) {
        return false;
    }

    if (a.numParams() != b.numParams()) {
        return false;
    }

    for (unsigned i = 0; i < a.numParams(); ++i) {
        const QoreTypeInfo* at = a.getParamTypeInfo(i);
        const QoreTypeInfo* bt = b.getParamTypeInfo(i);
        if (!at || !bt) {
            return true;
        }
        if (at == anyTypeInfo || bt == anyTypeInfo || at == autoTypeInfo || bt == autoTypeInfo) {
            return true;
        }

        bool match = relaxed_match
            ? (QoreTypeInfo::parseAccepts(at, bt) >= QTI_WILDCARD
                || QoreTypeInfo::parseAccepts(bt, at) >= QTI_WILDCARD)
            : (QoreTypeInfo::runtimeTypeMatch(at, bt) >= QTI_NEAR
                || QoreTypeInfo::runtimeTypeMatch(bt, at) >= QTI_NEAR);

        if (!match) {
            return false;
        }
    }

    return true;
}

QoreString build_base_class_list(const std::set<std::string>& bases) {
    QoreString desc;
    size_t i = 0;
    for (const auto& base_name : bases) {
        if (i) {
            desc.concat(", ");
        }
        desc.concat("'");
        desc.concat(base_name.c_str());
        desc.concat("'");
        ++i;
    }
    return desc;
}

} // namespace

void qore_class_private::parseWarnAmbiguousOverloads() {
    if (sys || !scl || !parse_check_parse_option(PO_REQUIRE_TYPES) || parse_check_parse_option(PO_IN_MODULE)) {
        return;
    }

    bool relaxed_match = ahm.relaxed_match;

    // warn when a local method hides base class overloads
    for (const auto& i : hm) {
        QoreMethod* local_method = i.second;
        if (is_special_method_name(local_method->getName())) {
            continue;
        }

        MethodFunctionBase* local_func = qore_method_private::get(*local_method)->getFunction();
        std::set<std::string> hidden_base_names;
        std::set<std::string> match_set;
        bool hides_variants = false;
        bool ambiguous_signatures = false;

        append_method_signatures(*local_method, match_set);

        for (auto& bci : *scl) {
            if (!bci->sclass) {
                continue;
            }
            const QoreMethod* base_method = bci->parseFindNormalMethod(local_method->getName(), this, true);
            if (!base_method) {
                continue;
            }

            MethodFunctionBase* base_func = qore_method_private::get(*base_method)->getFunction();
            if (base_func->numVariants() > 1
                && !local_overrides_all_base_variants(*local_func, *base_func, relaxed_match)) {
                if (local_func->parseHasAmbiguousSignature(*base_func, relaxed_match)) {
                    ambiguous_signatures = true;
                }
                const QoreClass* base_class = base_method->getClass();
                if (base_class) {
                    hidden_base_names.insert(base_class->getName());
                }
                append_method_signatures(*base_method, match_set);
                hides_variants = true;
            }
        }

        if (hides_variants && ambiguous_signatures) {
            std::vector<std::string> matches(match_set.begin(), match_set.end());
            QoreString match_list = build_match_list(matches);
            QoreString base_list = build_base_class_list(hidden_base_names);
            QoreString local_list = build_method_overload_list(*local_method);
            QoreStringNode* desc = new QoreStringNode;
            if (local_func->numVariants() > 1) {
                desc->sprintf("method overloads %s hide overloaded variants from base class(es) %s; matches: %s; "
                    "calls will resolve to overloads %s; use a fully-qualified base class path in the call or "
                    "rename the object to remove ambiguity; consider adjusting one or more signatures to "
                    "disambiguate the overloads (for example, add distinct parameter types or remove default "
                    "arguments)", local_list.c_str(), base_list.c_str(), match_list.c_str(), local_list.c_str());
            } else {
                desc->sprintf("method %s hides overloaded variants from base class(es) %s; matches: %s; calls will "
                    "resolve to %s; use a fully-qualified base class path in the call or rename the object to "
                    "remove ambiguity; consider adjusting one or more signatures to disambiguate the overloads (for "
                    "example, add distinct parameter types or remove default arguments)", local_list.c_str(),
                    base_list.c_str(), match_list.c_str(), local_list.c_str());
            }
            qore_program_private::makeParseWarning(getProgram(), *get_method_warning_loc(*this, *local_func),
                QP_WARN_AMBIGUOUS_OVERLOAD, "AMBIGUOUS-OVERLOAD", desc);
        }
    }

    // warn when multiple base classes supply a method with the same name
    std::set<std::string> base_names;
    std::unordered_set<qore_classid_t> visited;
    collect_base_method_names(*this, false, base_names, visited);

    for (const auto& name_it : base_names) {
        if (hm.find(name_it) != hm.end()) {
            continue;
        }

        const QoreMethod* resolved_method = nullptr;
        std::set<std::string> provider_names;
        std::set<std::string> match_set;
        for (auto& bci : *scl) {
            if (!bci->sclass) {
                continue;
            }
            const QoreMethod* base_method = bci->parseFindNormalMethod(name_it.c_str(), this, true);
            if (!base_method) {
                continue;
            }
            if (!resolved_method) {
                resolved_method = base_method;
            }
            const QoreClass* method_class = base_method->getClass();
            if (method_class) {
                provider_names.insert(method_class->getName());
            }
            append_method_signatures(*base_method, match_set);
        }

        if (provider_names.size() > 1 && resolved_method) {
            std::vector<std::string> matches(match_set.begin(), match_set.end());
            QoreString match_list = build_match_list(matches);
            QoreString resolved_list = build_method_overload_list(*resolved_method);
            QoreStringNode* desc = new QoreStringNode;
            if (qore_method_private::get(*resolved_method)->getFunction()->numVariants() > 1) {
                desc->sprintf("method name '%s()' in class '%s' is ambiguous in calls; matches: %s; calls will "
                    "resolve to overloads %s; use a fully-qualified base class path in the call or rename the "
                    "object to remove ambiguity", name_it.c_str(), name.c_str(), match_list.c_str(),
                    resolved_list.c_str());
            } else {
                desc->sprintf("method name '%s()' in class '%s' is ambiguous in calls; matches: %s; calls will "
                    "resolve to %s; use a fully-qualified base class path in the call or rename the object to "
                    "remove ambiguity", name_it.c_str(), name.c_str(), match_list.c_str(), resolved_list.c_str());
            }
            qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_AMBIGUOUS_OVERLOAD,
                "AMBIGUOUS-OVERLOAD", desc);
        }
    }

    // no warnings for static methods; they are not inherited
}

int qore_class_private::parseResolveHierarchy() {
    if (!parse_resolve_hierarchy) {
        parse_resolve_hierarchy = true;

        if (!scl) {
            return 0;
        }

        qcp_set_t qcp_set = {this};
        // issue #2657: initialize class hierarchy first before initializing code and members
        return initializeHierarchy(qcp_set);
    }

    return 0;
}

void qore_class_private::parseResolveAbstract() {
    if (!parse_resolve_abstract) {
        parse_resolve_abstract = true;

        if (!scl)
            return;

        scl->parseResolveAbstract();

        // search for new concrete variants of abstract variants last
        ahm.parseInit(*this, scl);
    }
}

void qore_class_private::recheckBuiltinMethodHierarchy() {
    initialize();

    if (!scl)
        return;

    for (hm_method_t::iterator i = hm.begin(), e = hm.end(); i != e; ++i)
        scl->addNewAncestors(i->second);

    for (hm_method_t::iterator i = shm.begin(), e = shm.end(); i != e; ++i)
        scl->addNewStaticAncestors(i->second);
}

void qore_class_private::resolveCopy() {
    if (resolve_copy_done)
        return;

    resolve_copy_done = true;

    // resolve inheritance lists in methods
    for (hm_method_t::iterator i = hm.begin(), e = hm.end(); i != e; ++i)
        i->second->priv->func->resolveCopy();

    for (hm_method_t::iterator i = shm.begin(), e = shm.end(); i != e; ++i)
        i->second->priv->func->resolveCopy();

    if (scl)
        scl->resolveCopy();
}

int qore_class_private::checkExistingVarMember(const char* dname, const QoreMemberInfoBaseAccess* mi,
        const QoreMemberInfoBaseAccess* omi, const qore_class_private* qc, ClassAccess oaccess, bool var) const {
    //printd(5, "qore_class_private::checkExistingVarMember() name: %s priv: %d is_priv: %d sclass: %s\n", name.c_str(), priv, is_priv, sclass->getName());

    // here we know that the member or var already exists, so either it will be a
    // duplicate declaration, in which case it is ignored if it has no type info, or it is a
    // contradictory declaration, in which case a parse exception is raised

    // if the var was previously declared public
    if (mi->access != oaccess) {
        // raise an exception only if parse exceptions are enabled
        if (getProgram()->getParseExceptionSink()) {
            QoreStringNode* desc = new QoreStringNode;
            desc->sprintf("class '%s' ", name.c_str());
            desc->concat("cannot declare ");
            desc->sprintf("%s %s ", privpub(mi->access), var ? "static variable" : "member");
            desc->sprintf("'%s' when ", dname);
            if (qc == this)
                desc->concat("this class");
            else
                desc->sprintf("base class '%s'", qc->name.c_str());
            desc->sprintf(" already declared this %s as %s", var ? "variable" : "member", privpub(oaccess));
            qore_program_private::makeParseException(getProgram(), *mi->loc, "PARSE-ERROR", desc);
        }
        return -1;
    } else if ((mi && mi->parseHasTypeInfo()) || (omi && omi->parseHasTypeInfo())) {
        if (getProgram()->getParseExceptionSink()) {
            QoreStringNode* desc = new QoreStringNode;
            desc->sprintf("%s %s ", privpub(mi->access), var ? "static variable" : "member");
            desc->sprintf("'%s' was already declared in ", dname);
            if (qc == this)
                desc->concat("this class");
            else
                desc->sprintf("base class '%s'", qc->name.c_str());
            if (mi && mi->parseHasTypeInfo())
                desc->sprintf(" with a type definition");
            desc->concat(" and cannot be declared again");
            desc->sprintf(" in class '%s'", name.c_str());
            desc->concat(" if the declaration has a type definition");

            qore_program_private::makeParseException(getProgram(), *mi->loc, "PARSE-TYPE-ERROR", desc);
        }
        return -1;
    }

    return 0;
}

QoreValue qore_class_private::evalPseudoMethod(const QoreValue n, const char* nme, const QoreListNode* args,
        RuntimeConfig& rc, ExceptionSink* xsink) const {
   QORE_TRACE("qore_class_private::evalPseudoMethod()");

   const QoreMethod* m = runtimeFindPseudoMethod(n, nme, xsink);
   if (!m)
      return QoreValue();

   //printd(5, "qore_class_private::evalPseudoMethod() %s::%s() found method %p class %s\n", priv->name, nme, w,
   //   w->getClassName());

   return qore_method_private::evalPseudoMethod(*m, xsink, rc, 0, n, args);
}

QoreValue qore_class_private::evalPseudoMethod(const QoreMethod* m, const AbstractQoreFunctionVariant* variant,
        const QoreValue n, const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) const {
    return qore_method_private::evalPseudoMethod(*m, xsink, rc, variant, n, args);
}

bool qore_class_private::parseCheckPrivateClassAccess(const qore_class_private* opc) const {
    // see if shouldBeClass is a parent class of the class currently being parsed
    //ClassAccess access1 = Public;
    //printd(5, "qore_class_private::parseCheckPrivateClassAccess(%p '%s') pc: %p '%s' found: %p\n", this,
    //  name.c_str(), opc, opc ? opc->name.c_str() : "n/a", opc ? opc->getClass(*this, access1) : nullptr);

    if (!opc)
        return false;

    ClassAccess access = Public;
    return opc->parseGetClass(*this, access) || (scl && scl->parseGetClass(*opc, access, true));
}

bool qore_class_private::runtimeCheckPrivateClassAccess(const qore_class_private* qc) const {
    if (!qc) {
        //printd(5, "runtimeCheckPrivateClassAccess() this: %p '%s' no runtime class context: failed\n", this, name.c_str());
        return false;
    }

    ClassAccess access = Public;
    //printd(5, "runtimeCheckPrivateClassAccess() qc: %p '%s' test: %p '%s' okl: %d okr: %d\n", qc, qc->name.c_str(), this, name.c_str(), qc->getClassIntern(*this, access, true), (scl && scl->getClass(*qc, access, true)));
    return qc->getClassIntern(*this, access, true) || (scl && scl->getClass(*qc, access, true));
}

qore_type_result_e qore_class_private::parseCheckCompatibleClass(const qore_class_private& oc, bool& may_not_match) const {
    qore_type_result_e rv = parseCheckCompatibleClassIntern(oc, may_not_match);
    if (rv != QTI_NOT_EQUAL)
        return rv;
    if (injectedClass) {
        rv = injectedClass->parseCheckCompatibleClass(oc, may_not_match);
        if (rv != QTI_NOT_EQUAL)
            return rv;
    }
    if (oc.injectedClass) {
        qore_type_result_e rv = oc.injectedClass->parseCheckCompatibleClass(*this, may_not_match);
        if (!may_not_match && rv != QTI_NOT_EQUAL)
            may_not_match = true;
        return rv;
    }
    return QTI_NOT_EQUAL;
}

qore_type_result_e qore_class_private::parseCheckCompatibleClassIntern(const qore_class_private& oc, bool& may_not_match) const {
    // make sure both classes are initialized
    const_cast<qore_class_private*>(this)->initialize();
    const_cast<qore_class_private&>(oc).initialize();

#ifdef DEBUG_SKIP
    QoreString h1, h2;
    hash.toString(h1);
    oc.hash.toString(h2);
    printd(5, "qore_class_private::parseCheckCompatibleClass() %p '%s' (%d %s) == %p '%s' (%d %s)\n", this, name.c_str(), classID, h1.getBuffer(), &oc, oc.name.c_str(), oc.classID, h2.getBuffer());
#endif

    if (parseEqual(oc))
        return QTI_IDENT;

    ClassAccess access;
    if (!parseGetClass(oc, access)) {
        if (!oc.parseGetClass(*this, access))
            return QTI_NOT_EQUAL;
        may_not_match = true;
    }

    if (access == Public)
        return QTI_AMBIGUOUS;

    if (parseCheckPrivateClassAccess())
        return QTI_AMBIGUOUS;

    if (!parseCheckPrivateClassAccess()) {
        if (may_not_match)
            may_not_match = false;
        return QTI_NOT_EQUAL;
    }

    return QTI_AMBIGUOUS;
}

qore_type_result_e qore_class_private::runtimeCheckCompatibleClass(const qore_class_private& oc) const {
    qore_type_result_e rv = runtimeCheckCompatibleClassIntern(oc);
    if (rv != QTI_NOT_EQUAL)
        return rv;
    if (injectedClass) {
        rv = injectedClass->runtimeCheckCompatibleClassIntern(oc);
        if (rv != QTI_NOT_EQUAL)
            return rv;
    }
    return QTI_NOT_EQUAL;
}

qore_type_result_e qore_class_private::runtimeCheckCompatibleClassIntern(const qore_class_private& oc) const {
    if (equal(oc)) {
        return QTI_IDENT;
    }

    ClassAccess access = Public;
    if (!oc.scl || !oc.scl->getClass(*this, access, true)) {
        return QTI_NOT_EQUAL;
    }

    if (access == Public) {
        return QTI_AMBIGUOUS;
    }

    return runtimeCheckPrivateClassAccess() ? QTI_AMBIGUOUS : QTI_NOT_EQUAL;
}

QoreValue qore_class_private::setKeyValue(const std::string& key, QoreValue val) {
    // ensure atomicity when reading from or writing to kvmap
    AutoLocker al(gate.asl_lock);

    kvmap_t::iterator i = kvmap.lower_bound(key);
    if (i != kvmap.end() && i->first == key) {
        QoreValue rv = i->second;
        i->second = val;
        return rv;
    }
    kvmap.insert(i, kvmap_t::value_type(key, val));
    return QoreValue();
}

QoreValue qore_class_private::setKeyValueIfNotSet(const std::string& key, QoreValue val) {
    // ensure atomicity when reading from or writing to kvmap
    AutoLocker al(gate.asl_lock);

    kvmap_t::iterator i = kvmap.lower_bound(key);
    if (i != kvmap.end() && i->first == key) {
        if (i->second) {
            return val;
        }
        i->second = val;
        return QoreValue();
    }
    kvmap.insert(i, kvmap_t::value_type(key, val));
    return QoreValue();
}

bool qore_class_private::setKeyValueIfNotSet(const std::string& key, const char* val) {
    // ensure atomicity when reading from or writing to kvmap
    AutoLocker al(gate.asl_lock);

    kvmap_t::iterator i = kvmap.lower_bound(key);
    if (i != kvmap.end() && i->first == key) {
        if (!i->second) {
            i->second = new QoreStringNode(val);
            return true;
        }
        return false;
    }
    kvmap.insert(i, kvmap_t::value_type(key, new QoreStringNode(val)));
    return true;
}

QoreValue qore_class_private::getReferencedKeyValue(const std::string& key) const {
    // ensure atomicity when reading from or writing to kvmap
    AutoLocker al(gate.asl_lock);

    kvmap_t::const_iterator i = kvmap.find(key);
    if (i == kvmap.end()) {
        return QoreValue();
    }
    return i->second.refSelf();
}

QoreValue qore_class_private::getReferencedKeyValue(const char* key) const {
    // ensure atomicity when reading from or writing to kvmap
    AutoLocker al(gate.asl_lock);

    kvmap_t::const_iterator i = kvmap.find(key);
    if (i == kvmap.end()) {
        return QoreValue();
    }
    return i->second.refSelf();
}

const ConstantEntry* qore_class_private::findConstantEntry(const char* name) const {
    const ConstantEntry* ce = constlist.findEntry(name);
    if (ce) {
        return ce;
    }
    return scl ? scl->findConstantEntry(name) : nullptr;
}

bool QoreClass::hasParentClass() const {
    return (bool)priv->scl;
}

const QoreMethod* QoreClass::getConstructor() const {
    return priv->constructor;
}

const QoreMethod* QoreClass::getSystemConstructor() const {
    return priv->system_constructor;
}

const QoreMethod* QoreClass::getDestructor() const {
    return priv->destructor;
}

const QoreMethod* QoreClass::getCopyMethod() const {
    return priv->copyMethod;
}

const QoreMethod* QoreClass::getMemberGateMethod() const {
    return priv->memberGate;
}

const QoreMethod* QoreClass::getMethodGate() const {
    return priv->methodGate;
}

const QoreMethod* QoreClass::getMemberNotificationMethod() const {
    return priv->memberNotification;
}

const QoreTypeInfo* QoreClass::getTypeInfo() const {
    return priv->getTypeInfo();
}

const QoreTypeInfo* QoreClass::getOrNothingTypeInfo() const {
    return priv->getOrNothingTypeInfo();
}

bool QoreClass::isPublicOrPrivateMember(const char* str, bool& priv_member) const {
    const qore_class_private* class_ctx = runtime_get_class();
    if (class_ctx && !priv->runtimeCheckPrivateClassAccess(class_ctx)) {
        class_ctx = nullptr;
    }
    const QoreMemberInfo* mi = priv->runtimeGetMemberInfo(str, class_ctx);
    if (mi) {
        priv_member = mi->access > Public;
    }
    return mi;
}

void QoreClass::recheckBuiltinMethodHierarchy() {
    priv->recheckBuiltinMethodHierarchy();
}

void QoreClass::addBuiltinConstant(const char* name, QoreValue value, ClassAccess access,
        const QoreTypeInfo* typeInfo) {
    priv->addBuiltinConstant(name, value, access, typeInfo);
}

void QoreClass::addBuiltinStaticVar(const char* name, QoreValue value, ClassAccess access,
        const QoreTypeInfo* typeInfo) {
    priv->addBuiltinStaticVar(name, value, access, typeInfo);
}

void QoreClass::rescanParents() {
    // rebuild parent class data
    if (priv->scl) {
        priv->scl->rescanParents(this);
    }
}

void QoreClass::setPublicMemberFlag() {
    priv->has_public_memdecl = true;
}

void QoreClass::setGateAccessFlag() {
    priv->gate_access = true;
}

void QoreClass::setLanguage(const char* lang) {
    priv->lang = lang;
}

const char* QoreClass::getLanguage() const {
    return priv->lang.c_str();
}

void QoreClass::setRelaxedAbstractMatch() {
    priv->ahm.relaxed_match = true;
}

bool QoreClass::getRelaxedAbstractMatch() const {
    return priv->ahm.relaxed_match;
}

void QoreClass::runtimeResolveAbstractMethods() {
    if (priv->scl) {
        priv->ahm.runtimeInit(*priv, priv->scl);
    }
    // mark as resolved so that parseResolveAbstract() in parseCommit() is a no-op;
    // this prevents concurrent modification of shared parent class data when
    // multiple Programs share JNI parent class pointers via BCNode::sclass
    priv->parse_resolve_abstract = true;
}

const QoreExternalNormalMember* QoreClass::findLocalMember(const char* name) const {
    return reinterpret_cast<const QoreExternalNormalMember*>(priv->members.find(name));
}

const QoreExternalStaticMember* QoreClass::findLocalStaticMember(const char* name) const {
    return reinterpret_cast<const QoreExternalStaticMember*>(priv->vars.find(name));
}

std::string QoreClass::getNamespacePath(bool anchored) const {
    std::string path;
    if (priv && priv->ns) {
        priv->ns->getPath(path);
        if (!path.empty()) {
            path += "::";
        }
        if (anchored) {
            path.insert(0, "::");
        }
        path += getName();
    }
    return path;
}

const char* QoreClass::getPath() const {
    return priv->path.c_str();
}

bool QoreClass::isEqual(const QoreClass& cls) const {
    return priv->equal(*cls.priv);
}

BinaryNode* QoreClass::getBinaryHash() const {
    SimpleRefHolder<BinaryNode> b(new BinaryNode);
    char* hash = priv->getHash();
    assert(hash);
    b->append(hash, SH_SIZE);
    return b.release();
}

const QoreExternalConstant* QoreClass::findConstant(const char* name) const {
    return reinterpret_cast<const QoreExternalConstant*>(priv->findConstantEntry(name));
}

const QoreNamespace* QoreClass::getNamespace() const {
    return priv->ns->ns;
}

QoreValue QoreClass::setKeyValue(const std::string& key, QoreValue val) {
    return priv->setKeyValue(key, val);
}

QoreValue QoreClass::setKeyValueIfNotSet(const std::string& key, QoreValue val) {
    return priv->setKeyValueIfNotSet(key, val);
}

bool QoreClass::setKeyValueIfNotSet(const std::string& key, const char* val) {
    return priv->setKeyValueIfNotSet(key, val);
}

QoreValue QoreClass::getReferencedKeyValue(const std::string& key) const {
    return priv->getReferencedKeyValue(key);
}

AbstractQoreNode* QoreClass::getNewObjectExpression(QoreListNode* args) const {
    return new NewObjectCallNode(this, args);
}

int MethodFunctionBase::parseInit() {
    return QoreFunction::parseInit(qore_class_private::get(*qc)->ns);
}

void MethodFunctionBase::parseCommit() {
    QoreFunction::parseCommit();
}

void MethodFunctionBase::parseRollback() {
    QoreFunction::parseRollback();
}

int MethodFunctionBase::checkFinalVariant(const MethodFunctionBase* m, const MethodVariantBase* v) const {
    if (!v->isFinal()) {
        return 0;
    }

    const AbstractFunctionSignature* sig = v->getSignature();
    const AbstractFunctionSignature* vs = nullptr;
    int rc = parseCompareResolvedSignature(vlist, sig, vs);
    if (rc == QTI_NOT_EQUAL) {
        return 0;
    }

    const char* stat = isStatic() ? "static " : "";
    // can only be overridden with a user variant
    assert(dynamic_cast<const UserSignature*>(vs));
    parse_error(*static_cast<const UserSignature*>(vs)->getParseLocation(), "child class method %s%s::%s(%s) " \
        "cannot override parent class method final %s%s::%s(%s)", stat, qc->getName(), getName(),
        vs->getSignatureText(), stat, m->qc->getName(), getName(), sig->getSignatureText());
    return -1;
}

int MethodFunctionBase::checkFinal() const {
    // only check if we have new pending variants in this method
    if (vlist.empty()) {
        //printd(5, "MethodFunctionBase::checkFinal() %s::%s() pending list is empty\n", qc->getName(), getName());
        return 0;
    }

    ilist_t::const_iterator i = ilist.begin(), e = ilist.end();
    ++i;
    for (; i != e; ++i) {
        const MethodFunctionBase* m = METHFB_const((*i).func);
        //printd(5, "MethodFunctionBase::checkFinal() base method %s::%s() has_final: %d against child %s::%s()\n",
        //  m->qc->getName(), getName(), m->has_final, qc->getName(), getName());
        if (m->has_final) {
            for (vlist_t::const_iterator i = m->vlist.begin(), e = m->vlist.end(); i != e; ++i) {
                if (checkFinalVariant(m, METHVB_const(*i)))
                    return -1;
            }
        }
    }
    return 0;
}

void MethodFunctionBase::addBuiltinMethodVariant(MethodVariantBase* variant) {
    ClassAccess ma = variant->getAccess();
    if (access > ma) {
        access = ma;
    }
    if (!has_final && variant->isFinal()) {
        has_final = true;
    }
    if (is_abstract && !variant->isAbstract()) {
        is_abstract = false;
    }
    if (!has_private_internal_variants && ma == Internal) {
        has_private_internal_variants = true;
    }
    addBuiltinVariant(variant);
}

int MethodFunctionBase::parseAddUserMethodVariant(MethodVariantBase* variant) {
    int rc = addPendingVariant(variant);
    if (!rc) {
        ClassAccess ma = variant->getAccess();
        if (access > ma) {
            access = ma;
        }
        if (!has_final && variant->isFinal()) {
            has_final = true;
        }
        if (is_abstract && !variant->isAbstract()) {
            is_abstract = false;
        }
        if (!has_private_internal_variants && ma == Internal) {
            has_private_internal_variants = true;
        }
    }
    return rc;
}

static void do_variant_sig(QoreString& csig, const std::string& name, const MethodVariantBase* v, const char* mod) {
    if (v->isAbstract()) {
        csig.concat("abstract ");
    }
    switch (v->getAccess()) {
        case Public: csig.concat("pub "); break;
        case Private: csig.concat("priv "); break;
        case Internal: csig.concat("int "); break;
        case Inaccessible: assert(false); break;
    }
    if (mod) {
        csig.concat(mod);
        csig.concat(' ');
    }
    csig.concat(name);
    csig.concat('(');
    csig.concat(v->getSignature()->getSignatureText());
    csig.concat(')');
    csig.concat('\n');
}

void MethodFunctionBase::parseSignatures(QoreString& csig, const char* mod) const {
    for (vlist_t::const_iterator i = vlist.begin(), e = vlist.end(); i != e; ++i) {
        const MethodVariantBase* v = METHVB_const(*i);
        do_variant_sig(csig, name, v, mod);
    }
}

void MethodFunctionBase::parseCommitMethod(QoreString& csig, const char* mod) {
    parseSignatures(csig, mod);
    parseCommitMethod();
}

void MethodFunctionBase::parseCommitMethod() {
    parseCommit();
}

void MethodFunctionBase::parseRollbackMethod() {
    parseRollback();
}

void MethodFunctionBase::replaceAbstractVariantIntern(MethodVariantBase* variant) {
    assert(!variant->isAbstract());
    variant->ref();
    AbstractFunctionSignature& sig = *(variant->getSignature());
    bool relaxed_match = qore_class_private::get(*getClass())->ahm.relaxed_match;

    for (vlist_t::iterator i = vlist.begin(), e = vlist.end(); i != e; ++i) {
        (*i)->parseResolveUserSignature();
        if ((*i)->isSignatureIdentical(sig, relaxed_match)) {
            pending_save.push_back(*i);
            vlist.erase(i);
            vlist.push_back(variant);
            //printd(5, "MethodFunctionBase::replaceAbstractVariantIntern() this: %p replacing %p ::%s%s in vlist\n",
            //    this, variant, getName(), variant->getAbstractSignature());
            return;
        }
    }
    //printd(5, "MethodFunctionBase::replaceAbstractVariantIntern() this: %p adding %p ::%s%s to vlist\n", this,
    //    variant, getName(), variant->getAbstractSignature());
    vlist.push_back(variant);
    if (is_abstract) {
        is_abstract = false;
    }
}

void MethodFunctionBase::replaceAbstractVariant(MethodVariantBase* variant) {
    replaceAbstractVariantIntern(variant);

    ClassAccess ma = variant->getAccess();
    if (access > ma) {
        access = ma;
    }

    if (!has_final && variant->isFinal()) {
        has_final = true;
    }
    if (!has_private_internal_variants && ma == Internal) {
        has_private_internal_variants = true;
    }
}

// if an identical signature is found to the passed variant, then it is removed from the abstract list
MethodVariantBase* MethodFunctionBase::parseHasVariantWithSignature(MethodVariantBase* v, bool relaxed_match) const {
    v->parseResolveUserSignature();
    AbstractFunctionSignature& sig = *(v->getSignature());
    for (vlist_t::const_iterator i = vlist.begin(), e = vlist.end(); i != e; ++i) {
        (*i)->parseResolveUserSignature();
        if ((*i)->isSignatureIdentical(sig, relaxed_match)) {
            return reinterpret_cast<MethodVariantBase*>(*i);
        }
    }
    return nullptr;
}

bool MethodFunctionBase::parseHasAmbiguousSignature(const MethodFunctionBase& other, bool relaxed_match) const {
    for (vlist_t::const_iterator li = vlist.begin(), le = vlist.end(); li != le; ++li) {
        (*li)->parseResolveUserSignature();
        const AbstractFunctionSignature* lsig = (*li)->getSignature();
        if (!lsig) {
            return true;
        }

        int compat_count = 0;
        for (vlist_t::const_iterator oi = other.vlist.begin(), oe = other.vlist.end(); oi != oe; ++oi) {
            (*oi)->parseResolveUserSignature();
            const AbstractFunctionSignature* osig = (*oi)->getSignature();
            if (!osig) {
                return true;
            }
            if (signatures_ambiguous(*lsig, *osig, relaxed_match)) {
                if (++compat_count > 1) {
                    return true;
                }
            }
        }
    }
    return false;
}

// runtime version - assumes signatures are already resolved
MethodVariantBase* MethodFunctionBase::runtimeHasVariantWithSignature(MethodVariantBase* v, bool relaxed_match) const {
    AbstractFunctionSignature& sig = *(v->getSignature());
    for (vlist_t::const_iterator i = vlist.begin(), e = vlist.end(); i != e; ++i) {
        if ((*i)->isSignatureIdentical(sig, relaxed_match)) {
            return reinterpret_cast<MethodVariantBase*>(*i);
        }
    }
    return nullptr;
}

QoreValue UserMethodVariant::evalMethod(QoreObject* self, CodeEvaluationHelper& ceh, ExceptionSink* xsink) const {
    //QORE_TRACE("UserMethodVariant::evalMethod()");
    VRMutexOptionalLockHelper vrmolh(synchronized ? (
        self
            ? qore_object_private::get(*self)->getGate()
            : ceh.getClass()->getGate()
        ) : nullptr, xsink);
    if (*xsink)
        return QoreValue();
    //printd(5, "UserMethodVariant::evalMethod() this: %p %s::%s() self: %p cctx: %p (%s)\n", this,
    //    getClassPriv()->name.c_str(), qmethod->getName(), self, runtime_get_class(),
    //    runtime_get_class() ? runtime_get_class()->name.c_str() : "n/a");
    return eval(qmethod->getName(), &ceh, self, xsink, getClassPriv());
}

void BuiltinConstructorValueVariant::evalConstructor(const QoreClass& thisclass, QoreObject* self,
        CodeEvaluationHelper& ceh, BCList* bcl, BCEAList* bceal, ExceptionSink* xsink) const {
    CodeContextHelper cch(xsink, CT_BUILTIN, "constructor", self, qore_class_private::get(thisclass));

    if (constructorPrelude(thisclass, ceh, self, bcl, bceal, xsink))
        return;

    constructor(self, ceh.getArgs(), ceh.getRuntimeConfig(), xsink);
}

void BuiltinExternalConstructorValueVariant::evalConstructor(const QoreClass& thisclass, QoreObject* self,
        CodeEvaluationHelper& ceh, BCList* bcl, BCEAList* bceal, ExceptionSink* xsink) const {
    CodeContextHelper cch(xsink, CT_BUILTIN, "constructor", self, qore_class_private::get(thisclass));

    if (constructorPrelude(thisclass, ceh, self, bcl, bceal, xsink))
        return;

    constructor(*qmethod, ptr, self, ceh.getArgs(), ceh.getRuntimeConfig(), xsink);
}

int ConstructorMethodVariant::constructorPrelude(const QoreClass& thisclass, CodeEvaluationHelper& ceh,
        QoreObject* self, BCList* bcl, BCEAList* bceal, ExceptionSink* xsink) const {
    if (bcl) {
        const BCAList* bcal = getBaseClassArgumentList();
        if (bcal) {
            bcal->execBaseClassConstructorArgs(bceal, xsink);
            if (*xsink) {
                return -1;
            }
        }
        bcl->execConstructors(self, bceal, ceh.getRuntimeConfig(), xsink);
        if (*xsink) {
            return -1;
        }
    }

    return 0;
}

UserConstructorVariant::~UserConstructorVariant() {
    delete bcal;
}

void UserConstructorVariant::evalConstructor(const QoreClass &thisclass, QoreObject* self,
        CodeEvaluationHelper& ceh, BCList* bcl, BCEAList* bceal, ExceptionSink* xsink) const {
    UserVariantExecHelper uveh(this, &ceh, xsink);
    if (!uveh)
        return;

    CodeContextHelper cch(xsink, CT_USER, "constructor", self, qore_class_private::get(thisclass), false);

    // instantiate "self" before executing base class constructors in case base class constructor arguments reference
    // "self"
    assert(signature.selfid);
    signature.selfid->instantiateSelf(self);

    // instantiate argv and push id on stack for base class constructors
    if (bcl) {
        ReferenceHolder<QoreListNode>& argv = uveh.getArgv();
        signature.argvid->instantiate(argv ? argv->refSelf() : nullptr);
        ArgvContextHelper argv_helper(argv ? argv->listRefSelf() : nullptr, xsink);
    }

    if (!constructorPrelude(thisclass, ceh, self, bcl, bceal, xsink)) {
        evalIntern(uveh.getArgv(), 0, xsink).discard(xsink);
    }

    // uninstantiate argv
    if (bcl)
        signature.argvid->uninstantiate(xsink);

    // if self then uninstantiate
    signature.selfid->uninstantiateSelf();
}

int UserConstructorVariant::parseInit(QoreFunction* f) {
    MethodFunctionBase* mf = static_cast<MethodFunctionBase*>(f);
    const QoreClass& parent_class = *(mf->MethodFunctionBase::getClass());

    signature.resolve();
    assert(!signature.getReturnTypeInfo() || signature.getReturnTypeInfo() == nothingTypeInfo);

    // push return type on stack (no return value can be used)
    ParseCodeInfoHelper rtih("constructor", nothingTypeInfo);

    int err = 0;

    if (bcal && !parent_class.hasParentClass()) {
        parse_error(*signature.getParseLocation(), "base constructor arguments given for class '%s' that has no " \
            "parent classes", parent_class.getName());
        delete bcal;
        bcal = nullptr;
        err = -1;
    }

    //printd(5, "UserConstructorVariant::parseInitConstructor() this: %p %s::constructor() params: %d\n", this,
    //    parent_class.getName(), signature.numParams());
    // must be called even if statements is NULL
    if (statements->parseInitConstructor(parent_class.getTypeInfo(), this, bcal, parent_class) && !err) {
        err = -1;
    }

    // recheck types against committed types if necessary
    if (recheck) {
        f->parseCheckDuplicateSignatureCommitted(&signature);
    }

    return err;
}

void BuiltinDestructorVariant::evalDestructor(const QoreClass &thisclass, QoreObject* self, RuntimeConfig& rc,
        ExceptionSink* xsink) const {
    CodeContextHelper cch(xsink, CT_BUILTIN, "destructor", self, qore_class_private::get(thisclass));

    AbstractPrivateData* private_data = self->getAndClearPrivateData(thisclass.getID(), xsink);
    //printd(5, "BuiltinDestructorVariant::evalDestructor() o: %p, v: %d, classid: %d, private: %p\n", self,
    //    self->isValid(), thisclass.getID(), private_data);
    if (!private_data)
        return;
    destructor(self, private_data, rc, xsink);
}

void BuiltinExternalDestructorVariant::evalDestructor(const QoreClass &thisclass, QoreObject* self,
        RuntimeConfig& rc, ExceptionSink* xsink) const {
    CodeContextHelper cch(xsink, CT_BUILTIN, "destructor", self, qore_class_private::get(thisclass));

    AbstractPrivateData* private_data = self->getAndClearPrivateData(thisclass.getID(), xsink);
    //printd(5, "BuiltinExternalDestructorVariant::evalDestructor() o: %p, v: %d, private: %p\n", self,
    //    self->isValid(), private_data);
    if (!private_data)
        return;
    destructor(thisclass, ptr, self, private_data, rc, xsink);
}

void UserCopyVariant::evalCopy(const QoreClass& thisclass, QoreObject* self, QoreObject* old,
        CodeEvaluationHelper& ceh, BCList* scl, ExceptionSink* xsink) const {
    // there can only be max 1 param
    assert(signature.numParams() <= 1);

    QoreListNode* args = new QoreListNode(autoTypeInfo);
    args->push(old->refSelf(), nullptr);
    ceh.setArgs(args);

    UserVariantExecHelper uveh(this, &ceh, xsink);
    if (!uveh) {
        return;
    }

    CodeContextHelper cch(xsink, CT_USER, "copy", self, qore_class_private::get(thisclass));

    if (scl) {
        scl->sml.execCopyMethods(self, old, ceh.getRuntimeConfig(), xsink);
        if (*xsink) {
            return;
        }
    }

    evalIntern(uveh.getArgv(), self, xsink).discard(xsink);
}

int UserCopyVariant::parseInit(QoreFunction* f) {
    MethodFunctionBase* mf = static_cast<MethodFunctionBase*>(f);
    const QoreClass& parent_class = *(mf->MethodFunctionBase::getClass());

    int err = signature.resolve();

    // make sure there is max one parameter in the copy method
    if (signature.numParams() > 1) {
        parse_error(*signature.getParseLocation(), "maximum of one parameter may be defined in class copy methods " \
            "(%d defined); this parameter will be assigned to the old object when the method is executed",
            signature.numParams());
        if (!err) {
            err = -1;
        }
    }

    // push return type on stack (no return value can be used)
    ParseCodeInfoHelper rtih("copy", nothingTypeInfo);

    // must be called even if statements is NULL
    if (statements->parseInitMethod(parent_class.getTypeInfo(), this) && !err) {
        err = -1;
    }

    // see if there is a type specification for the sole parameter and make sure it matches the class if there is
    if (signature.numParams()) {
        const QoreTypeInfo* typeInfo = signature.getParamTypeInfo(0);
        if (typeInfo) {
            if (QoreTypeInfo::parseReturns(typeInfo, &parent_class) == QTI_NOT_EQUAL) {
                // raise parse exception if parse exceptions have not been suppressed
                if (getProgram()->getParseExceptionSink()) {
                    QoreStringNode* desc = new QoreStringNode("the copy constructor will be passed ");
                    QoreTypeInfo::getThisType(parent_class.getTypeInfo(), *desc);
                    desc->concat(", but the object's parameter was defined expecting ");
                    QoreTypeInfo::getThisType(typeInfo, *desc);
                    desc->concat(" instead");
                    qore_program_private::makeParseException(getProgram(), *signature.getParseLocation(),
                        "PARSE-TYPE-ERROR", desc);
                }
                if (!err) {
                    err = -1;
                }
            }
        } else { // set to class' type
            signature.setFirstParamType(parent_class.getTypeInfo());
        }
    }

    // only 1 variant is possible, no need to recheck types
    return err;
}

void BuiltinCopyVariantBase::evalCopy(const QoreClass& thisclass, QoreObject* self, QoreObject* old,
        CodeEvaluationHelper& ceh, BCList* scl, ExceptionSink* xsink) const {
    CodeContextHelper cch(xsink, CT_BUILTIN, "copy", self, qore_class_private::get(thisclass));

    if (scl) {
        scl->sml.execCopyMethods(self, old, ceh.getRuntimeConfig(), xsink);
        if (*xsink) {
            return;
        }
    }

    old->evalCopyMethodWithPrivateData(thisclass, this, self, ceh.getRuntimeConfig(), xsink);
}

void ConstructorMethodFunction::evalConstructor(const AbstractQoreFunctionVariant* variant,
        const QoreClass& thisclass, QoreObject* self, const QoreListNode* args, BCList* bcl, BCEAList* bceal,
        RuntimeConfig& rc, ExceptionSink* xsink) const {
    // setup call, save runtime position, and evaluate arguments
    CodeEvaluationHelper ceh(xsink, rc, this, variant, "constructor", args, self,
        qore_class_private::get(thisclass));
    if (*xsink)
        return;

    CONMV_const(variant)->evalConstructor(thisclass, self, ceh, bcl, bceal, xsink);
}

void CopyMethodFunction::evalCopy(const QoreClass& thisclass, QoreObject* self, QoreObject* old, BCList* scl,
        RuntimeConfig& rc, ExceptionSink* xsink) const {
    assert(vlist.singular());

    const AbstractQoreFunctionVariant* variant = first();
    qore_call_t ct = variant->getCallType();

    // setup call, save runtime position
    CodeEvaluationHelper ceh(xsink, rc, this, variant, "copy", (QoreListNode*)nullptr, self,
        qore_class_private::get(thisclass), ct, true);
    if (*xsink) return;

    COPYMV_const(variant)->evalCopy(thisclass, self, old, ceh, scl, xsink);
}

void DestructorMethodFunction::evalDestructor(const QoreClass& thisclass, QoreObject* self, RuntimeConfig& rc,
        ExceptionSink* xsink) const {
    assert(vlist.singular());

    const AbstractQoreFunctionVariant* variant = first();
    qore_call_t ct = variant->getCallType();

    // setup call, save runtime position
    //printd(5, "DestructorMethodFunction::evalDestructor() %s::%s() o: %p, v: %d, ct: %d\n", getClassName(),
    //  getName(), self, self->isValid(),ct);
    CodeEvaluationHelper ceh(xsink, rc, this, variant, "destructor", (QoreListNode*)nullptr, self,
        qore_class_private::get(thisclass), ct);
    if (*xsink) return;

    DESMV_const(variant)->evalDestructor(thisclass, self, rc, xsink);
}

// if the variant was identified at parse time, then variant will not be NULL, otherwise if NULL then it is identified at run time
QoreValue NormalMethodFunction::evalMethod(ExceptionSink* xsink, RuntimeConfig& rc,
        const AbstractQoreFunctionVariant* variant, QoreObject* self, const QoreListNode* args,
        const qore_class_private* cctx, QoreProgram* pgm_ctx) const {
    //QORE_TRACE("NormalMethodFunction::evalMethod()");
    const char* cname = getClassName();
    const char* mname = getName();
    //printd(5, "NormalMethodFunction::evalMethod() %s::%s() v: %d\n", cname, mname, self->isValid());
    CodeEvaluationHelper ceh(xsink, rc, this, variant, mname, args, self, qore_class_private::get(*qc), CT_UNUSED,
        false,
        cctx, pgm_ctx);
    if (*xsink)
        return QoreValue();

    const MethodVariant* mv = METHV_const(variant);
    if (mv->isAbstract()) {
        xsink->raiseException("ABSTRACT-VARIANT-ERROR", "cannot call abstract variant %s::%s(%s) directly", cname,
            mname, mv->getSignature()->getSignatureText());
        return QoreValue();
    }
    //printd(5, "NormalMethodFunction::evalMethod() %s::%s(%s) (self: %s, v: %s) variant: %p, mv: %p priv: %d "
    //  "access: %d (%p %s)\n", getClassName(), mname, mv->getSignature()->getSignatureText(),
    //  self->getClass()->getName(), variant->getClass()->getName(), variant, mv, mv->isPrivate(),
    //  qore_class_private::runtimeCheckPrivateClassAccess(*mv->getClass()), runtime_get_class(),
    //  runtime_get_class() ? runtime_get_class()->name.c_str() : "n/a");

    return mv->evalMethod(self, ceh, xsink);
}

// if the variant was identified at parse time, then variant will not be NULL, otherwise if NULL then it is identified
// at run time
QoreValue NormalMethodFunction::evalMethodTmpArgs(ExceptionSink* xsink, RuntimeConfig& rc,
        const AbstractQoreFunctionVariant* variant, QoreObject* self, QoreListNode* args,
        const qore_class_private* cctx) const {
    //QORE_TRACE("NormalMethodFunction::evalMethodTmpArgs()");
    const char* cname = getClassName();
    const char* mname = getName();
    //printd(5, "NormalMethodFunction::evalMethod() %s::%s() v: %d\n", cname, mname, self->isValid());

    CodeEvaluationHelper ceh(xsink, rc, this, variant, mname, args, self, qore_class_private::get(*qc), CT_UNUSED,
        false, cctx);
    if (*xsink)
        return QoreValue();

    const MethodVariant* mv = METHV_const(variant);
    if (mv->isAbstract()) {
        xsink->raiseException("ABSTRACT-VARIANT-ERROR", "cannot call abstract variant %s::%s(%s) directly", cname,
            mname, mv->getSignature()->getSignatureText());
        return QoreValue();
    }
    //printd(5, "NormalMethodFunction::evalMethod() %s::%s(%s) (self: %s) variant: %p, mv: %p priv: %d access: %d "
    //  "(%p %s)\n",getClassName(), mname, mv->getSignature()->getSignatureText(), self->getClass()->getName(),
    //  variant, mv, mv->isPrivate(), qore_class_private::runtimeCheckPrivateClassAccess(*mv->getClass()),
    //  runtime_get_class(), runtime_get_class() ? runtime_get_class()->name.c_str() : "n/a");

    return mv->evalMethod(self, ceh, xsink);
}

// if the variant was identified at parse time, then variant will not be NULL, otherwise if NULL then it is identified
// at run time
QoreValue NormalMethodFunction::evalPseudoMethod(ExceptionSink* xsink, RuntimeConfig& rc,
        const AbstractQoreFunctionVariant* variant, const QoreValue n, const QoreListNode* args,
        const qore_class_private* cctx) const {
    //QORE_TRACE("NormalMethodFunction::evalPseudoMethod()");
    const char* mname = getName();
    //printd(5, "NormalMethodFunction::evalPseudoMethod() '%s' cctx: '%s'\n", mname,
    //    cctx ? cctx->name.c_str() : "n/a");
    CodeEvaluationHelper ceh(xsink, rc, this, variant, mname, args, nullptr, qore_class_private::get(*qc),
        CT_UNUSED, false, cctx);
    if (*xsink)
        return QoreValue();

    return METHV_const(variant)->evalPseudoMethod(n, ceh, xsink);
}

// if the variant was identified at parse time, then variant will not be NULL, otherwise if NULL then it is identified
// at run time
QoreValue StaticMethodFunction::evalMethod(ExceptionSink* xsink, RuntimeConfig& rc,
        const AbstractQoreFunctionVariant* variant, const QoreListNode* args, const qore_class_private* cctx,
        QoreProgram* pgm_ctx) const {
    const char* mname = getName();
    CodeEvaluationHelper ceh(xsink, rc, this, variant, mname, args, nullptr, qore_class_private::get(*qc),
        CT_UNUSED, false, cctx, pgm_ctx);
    if (*xsink) {
        return QoreValue();
    }
    // issue #4964: set class ctx
    ObjectSubstitutionHelper osh(nullptr, cctx);
    return METHV_const(variant)->evalMethod(nullptr, ceh, xsink);
}

// if the variant was identified at parse time, then variant will not be NULL, otherwise if NULL then it is identified
// at run time
QoreValue StaticMethodFunction::evalMethodTmpArgs(ExceptionSink* xsink, RuntimeConfig& rc,
        const AbstractQoreFunctionVariant* variant, QoreListNode* args, const qore_class_private* cctx) const {
    const char* mname = getName();
    CodeEvaluationHelper ceh(xsink, rc, this, variant, mname, args, nullptr, qore_class_private::get(*qc),
        CT_UNUSED, false, cctx);
    if (*xsink)
        return QoreValue();

    return METHV_const(variant)->evalMethod(nullptr, ceh, xsink);
}

const qore_class_private* MethodVariantBase::getClassPriv() const {
    return qore_class_private::get(*(qmethod->getClass()));
}

const char* MethodVariantBase::getAbstractSignature() {
    if (asig.empty())
        getSignature()->addAbstractParameterSignature(asig);
    return asig.c_str();
}

QoreValue BuiltinNormalMethodVariantBase::evalMethod(QoreObject* self, CodeEvaluationHelper& ceh,
        ExceptionSink* xsink) const {
    CodeContextHelper cch(xsink, CT_BUILTIN, qmethod->getName(), self, qore_class_private::get(*qmethod->getClass()));
    return qore_object_private::evalBuiltinMethodWithPrivateData(*self, *qmethod, this, ceh.getArgs(),
        ceh.getRuntimeConfig(), xsink);
}

QoreValue BuiltinNormalMethodVariantBase::evalPseudoMethod(const QoreValue n, CodeEvaluationHelper& ceh,
        ExceptionSink* xsink) const {
    CodeContextHelper cch(xsink, CT_BUILTIN, qmethod->getName(), nullptr, runtime_get_class());
    return evalImpl(nullptr, (AbstractPrivateData*)&n, ceh.getArgs(), ceh.getRuntimeConfig(), xsink);
}

class qmi_priv {
public:
    hm_method_t& m;
    hm_method_t::iterator i;

    DLLLOCAL qmi_priv(hm_method_t& n_m) : m(n_m) {
        i = m.end();
    }

    DLLLOCAL bool next() {
        if (i == m.end()) {
            i = m.begin();
        } else {
            ++i;
        }
        return i != m.end();
    }

    DLLLOCAL const QoreMethod* getMethod() const {
        assert(i != m.end());
        return i->second;
    }
};
#define HMI_CAST(p) (reinterpret_cast<qmi_priv*>(p))

QoreMethodIterator::QoreMethodIterator(const QoreClass& qc) : priv(new qmi_priv(qc.priv->hm)) {
}

QoreMethodIterator::~QoreMethodIterator() {
    delete HMI_CAST(priv);
}

bool QoreMethodIterator::next() {
    return HMI_CAST(priv)->next();
}

const QoreMethod* QoreMethodIterator::getMethod() const {
    return HMI_CAST(priv)->getMethod();
}

QoreStaticMethodIterator::QoreStaticMethodIterator(const QoreClass& qc) : priv(new qmi_priv(qc.priv->shm)) {
}

QoreStaticMethodIterator::~QoreStaticMethodIterator() {
    delete HMI_CAST(priv);
}

bool QoreStaticMethodIterator::next() {
    return HMI_CAST(priv)->next();
}

const QoreMethod* QoreStaticMethodIterator::getMethod() const {
    return HMI_CAST(priv)->getMethod();
}

// FIXME: eliminate this method; move injected fields to local, always copy classes with priv by reference
QoreMemberInfo::QoreMemberInfo(const QoreMemberInfo& old, const qore_class_private* cls) :
    QoreMemberInfoBaseAccess(old, old.access),
    cls_vec(old.cls_vec),
    cls_context_map(old.cls_context_map ? new cls_context_map_t(*old.cls_context_map) : nullptr),
    member_info_list(old.member_info_list ? new member_info_list_t(*old.member_info_list) : nullptr), is_local(old.is_local) {
    // write new class owner in initial position
    cls_vec[0] = cls;
}

QoreMemberInfo::QoreMemberInfo(const QoreMemberInfo& old, const qore_class_private* cls, ClassAccess cls_access) :
    QoreMemberInfoBaseAccess(old, (old.access == Internal) ? Inaccessible : QORE_MAX(old.access, cls_access)),
    cls_vec(old.cls_vec),
    cls_context_map(old.cls_context_map ? new cls_context_map_t(*old.cls_context_map) : nullptr),
    member_info_list(old.member_info_list ? new member_info_list_t(*old.member_info_list) : nullptr), is_local(false) {
    //printd(5, "QoreMemberInfo::QoreMemberInfo() copy cls: %p '%s' member_info_list: %p old.access: '%s' "
    //    "new.access: '%s' context_map: %p (%d) member_info_list: %p (%d)\n", cls, cls->name.c_str(),
    //    member_info_list, privpub(old.access), privpub(access), cls_context_map,
    //    cls_context_map ? cls_context_map->size() : nullptr, member_info_list,
    //    member_info_list ? member_info_list->size() : nullptr);
    // add inheriting class to class vector if accessible
    if (access < Inaccessible) {
        cls_vec.push_back(cls);
        //printd(5, "QoreMemberInfo::QoreMemberInfo() added class %p '%s' to inheritance list\n", cls,
        //    cls->name.c_str());
    } else {
        // when importing internal members, add context access entries for the child classes
        if (!member_info_list) {
            //printd(5, "QoreMemberInfo::QoreMemberInfo() adding context access for class %p '%s'\n",
            //    old.cls_vec.back(), old.cls_vec.back()->name.c_str());
            // add context to last class
            addContextAccess(old, old.cls_vec.back());
        } else {
            //printd(5, "QoreMemberInfo::QoreMemberInfo() copy member_info_list: %p old.access: '%s'\n",
            //    member_info_list, privpub(old.access));
        }
    }
}

void QoreMemberInfo::addContextAccess(const QoreMemberInfo& mi) {
    //printd(5, "QoreMemberInfo::addContextAccess() this: %p cls_context_map: %p (%d) mi.cls_context_map: %p (%d) "
    //    local: %d mi.local: %d\n", this, cls_context_map, cls_context_map ? cls_context_map->size() : nullptr,
    //    mi.cls_context_map, mi.cls_context_map ? mi.cls_context_map->size() : nullptr, local(), mi.local());
    if (!cls_context_map) {
        cls_context_map = new cls_context_map_t;
        assert(!member_info_list);
        member_info_list = new member_info_list_t;
    } else if (member_info_list->size()) {
        // see if we have already inherited this member
        if (getClass() == mi.getClass()) {
            //printd(5, "QoreMemberInfo::addContextAccess() this: %p already inherited member from class %p '%s'\n",
            //    this, mi.getClass(), mi.getClass()->name.c_str());
            return;
        }
    }

    // copy context map
    if (mi.cls_context_map) {
        for (auto& i : *mi.cls_context_map) {
            cls_context_map->insert(cls_context_map_t::value_type(i.first, i.second));
        }
        member_info_list->insert(member_info_list->begin(), mi.member_info_list->begin(), mi.member_info_list->end());
    }

    // add a mapping for imported local members
    addContextAccess(mi, mi.getClass());
}

void QoreMemberInfo::addContextAccess(const QoreMemberInfo& mi, const qore_class_private* qc) {
    //printd(5, "QoreMemberInfo::addContextAccess() this: %p cls_context_map: %p (%d) qc: %p '%s'\n", this,
    //    cls_context_map, cls_context_map ? cls_context_map->size() : 0, qc, qc->name.c_str());

    if (!cls_context_map) {
        cls_context_map = new cls_context_map_t;
        assert(!member_info_list);
        member_info_list = new member_info_list_t;
    }

#ifdef DEBUG
    bool inserted = false;
#endif
    for (auto& i : mi.cls_vec) {
        // the class can be in our cls_context_map already with a class that's inherited multiple times in the hierarchy
        cls_context_map_t::iterator ci = cls_context_map->lower_bound(i);
        if (ci == cls_context_map->end() || ci->first != i) {
            //printd(5, "QoreMemberInfo::addContextAccess() this: %p inserting cls %p '%s' -> %p '%s'\n", this, i,
            //    i->name.c_str(), qc, qc->name.c_str());
            cls_context_map->insert(ci, cls_context_map_t::value_type(i, qc));
#ifdef DEBUG
            inserted = true;
#endif
        }
#ifdef DEBUG
        else {
            //printd(5, "QoreMemberInfo::addContextAccess() this: %p skipping cls %p '%s' (-> %p '%s'); already inserted\n", this, i, i->name.c_str(), qc, qc->name.c_str());
            if (ci != cls_context_map->end()) {
                assert(ci->second == qc);
            }
        }
#endif
    }
#ifdef DEBUG
    if (inserted) {
        //printd(5, "QoreMemberInfo::addContextAccess() this: %p adding link to inherited member from class %p '%s'\n", this, qc, qc->name.c_str());
        member_info_list->insert(member_info_list->begin(), &mi);
    }
#endif
}

int QoreMemberInfo::parseInit(const char* name, LocalVar& selfid) {
    if (init) {
        return 0;
    }
    init = true;

    int err = 0;
    if (!typeInfo) {
        typeInfo = QoreParseTypeInfo::resolveAndDelete(parseTypeInfo, loc, err);
        parseTypeInfo = nullptr;
    }
#ifdef DEBUG
    else assert(!parseTypeInfo);
#endif

    if (exp) {
        printd(5, "QoreMemberInfo::parseInit() this: %p '%s' s'%s' %d\n", this, name, exp.getFullTypeName(),
            exp.getType());
        QoreParseContext parse_context(&selfid);
        if (parse_init_value(exp, parse_context) && !err) {
            err = -1;
        }
        const QoreTypeInfo* argTypeInfo = parse_context.typeInfo;
        if (parse_context.lvids) {
            parse_error(*loc, "illegal local variable declaration in member initialization expression");
            while (parse_context.lvids--) {
                pop_local_var();
            }
            if (!err) {
                err = -1;
            }
        }
        // get hard reference for assignment if applicable
        const QoreTypeInfo* assignmentTypeInfo = QoreTypeInfo::getHardReference(typeInfo);

        // throw a type exception only if parse exceptions are enabled
        if (!QoreTypeInfo::parseAccepts(assignmentTypeInfo, argTypeInfo)) {
            if (getProgram()->getParseExceptionSink()) {
                QoreStringNode* desc = new QoreStringNode("initialization expression for ");
                desc->sprintf("%s member '%s' returns ", privpub(access), name);
                QoreTypeInfo::getThisType(argTypeInfo, *desc);
                desc->concat(", but the member was declared as ");
                QoreTypeInfo::getThisType(typeInfo, *desc);
                qore_program_private::makeParseException(getProgram(), *loc, "PARSE-TYPE-ERROR", desc);
            }
            if (!err) {
                err = -1;
            }
        }
    }
    return err;
}

int QoreVarInfo::parseInit(const char* name) {
    if (QoreMemberInfoBaseAccess::init) {
        return 0;
    }
    QoreMemberInfoBaseAccess::init = true;

    int err = 0;
    if (!typeInfo) {
        typeInfo = QoreParseTypeInfo::resolveAndDelete(parseTypeInfo, loc, err);
        parseTypeInfo = nullptr;
    }
#ifdef DEBUG
    else assert(!parseTypeInfo);
#endif

    val.set(typeInfo);

    if (exp) {
        QoreParseContext parse_context;
        if (parse_init_value(exp, parse_context) && !err) {
            err = -1;
        }
        const QoreTypeInfo* argTypeInfo = parse_context.typeInfo;
        if (parse_context.lvids) {
            parse_error(*loc, "illegal local variable declaration in class static variable initialization " \
                "expression");
            while (parse_context.lvids--) {
                pop_local_var();
            }
            if (!err) {
                err = -1;
            }
        }
        // get hard reference for assignment if applicable
        const QoreTypeInfo* assignmentTypeInfo = QoreTypeInfo::getHardReference(typeInfo);

        // throw a type exception only if parse exceptions are enabled
        if (!QoreTypeInfo::parseAccepts(assignmentTypeInfo, argTypeInfo)) {
            if (getProgram()->getParseExceptionSink()) {
                QoreStringNode* desc = new QoreStringNode("initialization expression for ");
                desc->sprintf("%s class static variable '%s' returns ", privpub(access), name);
                QoreTypeInfo::getThisType(argTypeInfo, *desc);
                desc->concat(", but the variable was declared as ");
                QoreTypeInfo::getThisType(typeInfo, *desc);
                qore_program_private::makeParseException(getProgram(), *loc, "PARSE-TYPE-ERROR", desc);
            }
            if (!err) {
                err = -1;
            }
        }
    }
    return err;
}

int QoreVarInfo::evalInit(const char* name, ExceptionSink* xsink) {
    printd(5, "QoreVarInfo::evalInit() %s committing %s var (exp: %s)\n", name, privpub(access), exp.getFullTypeName());

    if (eval_init) {
        return 0;
    }
    eval_init = true;

    if (exp) {
        // evaluate expression
        ValueEvalOptimizedRefHolder val(exp, xsink);
        if (*xsink) {
            return -1;
        }
        if (QoreTypeInfo::mayRequireFilter(getTypeInfo(), *val)) {
            val.ensureReferencedValue();
            QoreTypeInfo::acceptInputMember(getTypeInfo(), name, *val, xsink);
            if (*xsink) {
                return -1;
            }
        }

        discard(assignInit(val.takeReferencedValue()), xsink);
    } else {
        init();
    }

    return 0;
}

QoreParseClassHelper::QoreParseClassHelper(QoreClass* new_cls, qore_ns_private* new_ns) {
    assert(!(new_ns && new_cls));
    qore_class_private* new_cls_priv = new_cls ? qore_class_private::get(*new_cls) : nullptr;
    if (!new_ns) {
        assert(new_cls_priv);
        new_ns = new_cls_priv->ns;
    }
    thread_set_class_and_ns(new_cls_priv, new_ns, cls, ns);
    restore = (cls != new_cls_priv || ns != new_ns);
    //printd(5, "QoreParseClassHelper() this: %p '%s' nc: %p nn: %p oc: %p on: %p\n", this,
    //    new_cls ? new_cls->getName() : "n/a", new_cls_priv, new_ns, cls, ns);
}

QoreParseClassHelper::~QoreParseClassHelper() {
    if (restore) {
        thread_set_class_and_ns(cls, ns);
        //printd(5, "~QoreParseClassHelper() this: %p restoring c: %p '%s' n: %p\n", this, cls,
        //    cls ? cls->name.c_str() : "n/a", ns);
    } else {
        //printd(5, "~QoreParseClassHelper() this: %p no restore necessary\n", this);
    }
}

int QoreMemberMap::parseInit(LocalVar& selfid) {
    //printd(5, "QoreMemberMap::parseInit() this: %p init: %d\n", this, init);
    if (init) {
        return 0;
    }
    init = true;
    int err = 0;
    for (auto& i : member_list) {
        printd(5, "QoreMemberMap::parseInit() this: %p mem: '%s' (%p) type: %s (%d)\n", this, i.first,
            i.second, i.second->exp.getTypeName(), i.second->exp.getType());
        if (i.second) {
            if (i.second->parseInit(i.first, selfid) && !err) {
                err = -1;
            }
        }
    }
    return err;
}

void QoreMemberMap::moveAllTo(QoreClass* qc, ClassAccess access) {
    if (empty() && access == Public) {
        qore_class_private::get(*qc)->parseSetEmptyPublicMemberDeclaration();
        return;
    }
    for (auto& i : member_list) {
        qore_class_private::parseAddMember(*qc, i.first, access, i.second);
        i.second = nullptr;
    }
    member_list.clear();
}

void QoreVarMap::parseCommitRuntimeInit(ExceptionSink* xsink) {
    if (init) {
        return;
    }
    init = true;
    assert(xsink);
    for (auto& i : member_list) {
        // initialize variable
        if (i.second->evalInit(i.first, xsink)) {
            continue;
        }
    }
}

void QoreVarMap::moveAllTo(QoreClass* qc, ClassAccess access) {
    if (empty() && access == Public) {
        qore_class_private::get(*qc)->parseSetEmptyPublicMemberDeclaration();
        return;
    }
    for (auto& i : member_list) {
        qore_class_private::parseAddStaticVar(qc, i.first, access, i.second);
        i.second = nullptr;
    }
    member_list.clear();
}

QoreClassHolder::~QoreClassHolder() {
    if (c) {
        qore_class_private::get(*c)->deref(true, true);
    }
}

QoreBuiltinClass::QoreBuiltinClass(QoreProgram* pgm, const char* name, const char* path, int64 n_domain)
        : QoreClass(name, path, n_domain) {
    setSystem();
    priv->spgm = pgm;
    priv->deref_source_program = false;
}

QoreBuiltinClass::QoreBuiltinClass(const char* name, const char* path, int64 n_domain)
        : QoreClass(name, path, n_domain) {
    setSystem();
}

QoreBuiltinClass::QoreBuiltinClass(const QoreBuiltinClass& old) : QoreClass(old) {
}

QoreBuiltinClass::QoreBuiltinClass() {
}

void QoreBuiltinClass::setModulePublic(bool v) {
    priv->pub = v;
}

QoreParseClass::QoreParseClass(const QoreProgramLocation* loc) {
    std::string path;
    std::string name = parse_pop_name(path);

    priv = new qore_class_private(this, std::move(name), std::move(path), QDOM_DEFAULT);
    priv->loc = loc;
}

class qore_parent_class_iterator_private {
public:
    DLLLOCAL qore_parent_class_iterator_private(const qore_class_private* qc) : qc(qc) {
        if (qc->scl) {
            i = qc->scl->end();
        }
    }

    DLLLOCAL bool next() {
        if (!qc->scl) {
            return false;
        }
        if (i == qc->scl->end()) {
            i = qc->scl->begin();
        } else {
            ++i;
        }
        if (i != qc->scl->end() && !(*i)->sclass && (*i)->tryResolveClass(qc->cls, false)) {
            i = qc->scl->end();
        }
        return i != qc->scl->end();
    }

    DLLLOCAL const QoreClass* getParentClass() const {
        assert(valid());
        return (*i)->sclass;
    }

    DLLLOCAL ClassAccess getAccess() const {
        assert(valid());
        return (*i)->access;
    }

    DLLLOCAL bool valid() const {
        return qc->scl && i != qc->scl->end();
    }

private:
    const qore_class_private* qc;
    BCList::const_iterator i;
};

QoreParentClassIterator::QoreParentClassIterator(const QoreClass& cls)
        : priv(new qore_parent_class_iterator_private(qore_class_private::get(cls))) {
}

QoreParentClassIterator::~QoreParentClassIterator() = default;

bool QoreParentClassIterator::next() {
    return priv->next();
}

bool QoreParentClassIterator::valid() const {
    return priv->valid();
}

const QoreClass& QoreParentClassIterator::getParentClass() const {
    return *priv->getParentClass();
}

ClassAccess QoreParentClassIterator::getAccess() const {
    return priv->getAccess();
}

class qore_class_constant_iterator : public PrivateIteratorBase<cnemap_t> {
public:
    DLLLOCAL qore_class_constant_iterator(const qore_class_private& qc)
            : PrivateIteratorBase<cnemap_t>(qc.constlist.cnemap) {
    }

    //! returns the constant
    DLLLOCAL const QoreExternalConstant* get() const {
        assert(valid());
        return reinterpret_cast<const QoreExternalConstant*>(i->second);
    }
};

QoreClassConstantIterator::QoreClassConstantIterator(const QoreClass& cls)
    : priv(new qore_class_constant_iterator(*qore_class_private::get(cls))) {
}

QoreClassConstantIterator::~QoreClassConstantIterator() = default;

bool QoreClassConstantIterator::next() {
    return priv->next();
}

bool QoreClassConstantIterator::valid() const {
    return priv->valid();
}

const QoreExternalConstant& QoreClassConstantIterator::get() const {
    return *priv->get();
}

class qore_class_member_iterator_private : public PrivateMemberIteratorBase<QoreMemberMap, QoreExternalNormalMember> {
public:
    DLLLOCAL qore_class_member_iterator_private(const qore_class_private& obj)
            : PrivateMemberIteratorBase<QoreMemberMap, QoreExternalNormalMember>(obj.members.member_list) {
    }

    DLLLOCAL bool next() {
        if (i == obj.end()) {
            i = obj.begin();
        } else {
            ++i;
        }
        while (i != obj.end() && !i->second->local()) {
            ++i;
        }
        return (i != obj.end());
    }
};

QoreClassMemberIterator::QoreClassMemberIterator(const QoreClass& cls) :
    priv(new qore_class_member_iterator_private(*qore_class_private::get(cls))) {
}

QoreClassMemberIterator::~QoreClassMemberIterator() = default;

bool QoreClassMemberIterator::next() {
    return priv->next();
}

bool QoreClassMemberIterator::valid() const {
    return priv->valid();
}

const QoreExternalNormalMember& QoreClassMemberIterator::getMember() const {
    return priv->getMember();
}

const char* QoreClassMemberIterator::getName() const {
    return priv->getName();
}

class qore_class_static_member_iterator_private
        : public PrivateMemberIteratorBase<QoreVarMap, QoreExternalStaticMember> {
public:
    DLLLOCAL qore_class_static_member_iterator_private(const qore_class_private& obj)
            : PrivateMemberIteratorBase<QoreVarMap, QoreExternalStaticMember>(obj.vars.member_list) {
    }
};

QoreClassStaticMemberIterator::QoreClassStaticMemberIterator(const QoreClass& cls) :
    priv(new qore_class_static_member_iterator_private(*qore_class_private::get(cls))) {
}

QoreClassStaticMemberIterator::~QoreClassStaticMemberIterator() = default;

bool QoreClassStaticMemberIterator::next() {
    return priv->next();
}

bool QoreClassStaticMemberIterator::valid() const {
    return priv->valid();
}

const QoreExternalStaticMember& QoreClassStaticMemberIterator::getMember() const {
    return priv->getMember();
}

const char* QoreClassStaticMemberIterator::getName() const {
    return priv->getName();
}

class qore_class_hierarchy_iterator {
public:
    DLLLOCAL qore_class_hierarchy_iterator(const qore_class_private& qc) : qc(qc) {
        if (qc.scl) {
            i = qc.scl->sml.end();
        }
    }

    DLLLOCAL bool next() {
        if (!qc.scl) {
            if (do_class) {
                do_class = false;
                return false;
            }
            do_class = true;
            return true;
        }
        if (i == qc.scl->sml.end()) {
            if (do_class) {
                do_class = false;
                return false;
            }
            i = qc.scl->sml.begin();
        } else {
            ++i;
            if (i == qc.scl->sml.end()) {
                if (!do_class) {
                    do_class = true;
                    return true;
                }
            }

        }
        return i != qc.scl->sml.end();
    }

    DLLLOCAL const QoreClass& get() const {
        if (do_class) {
            return *qc.cls;
        }
        assert(valid());
        return *i->first;
    }

    DLLLOCAL bool valid() const {
        return do_class || (qc.scl && i != qc.scl->sml.end());
    }

    DLLLOCAL bool isVirtual() const {
        if (do_class) {
            return false;
        }
        assert(valid());
        return i->second;
    }

private:
    const qore_class_private& qc;
    BCSMList::const_iterator i;
    bool do_class = false;
};

QoreClassHierarchyIterator::QoreClassHierarchyIterator(const QoreClass& cls)
        : priv(new qore_class_hierarchy_iterator(*qore_class_private::get(cls))) {
}

QoreClassHierarchyIterator::~QoreClassHierarchyIterator() = default;

bool QoreClassHierarchyIterator::next() {
    return priv->next();
}

bool QoreClassHierarchyIterator::valid() const {
    return priv->valid();
}

const QoreClass& QoreClassHierarchyIterator::get() const {
    return priv->get();
}

bool QoreClassHierarchyIterator::isVirtual() const {
    return priv->isVirtual();
}

class qore_class_destructor_hierarchy_iterator {
public:
    DLLLOCAL qore_class_destructor_hierarchy_iterator(const qore_class_private* qc) : qc(qc) {
        if (qc->scl) {
            i = qc->scl->sml.rend();
        }
    }

    DLLLOCAL bool next() {
        if (!do_class && i == qc->scl->sml.rend()) {
            do_class = true;
            return true;
        }
        if (do_class) {
            do_class = false;
        }
        if (!qc->scl) {
            return false;
        }

        if (i == qc->scl->sml.rend()) {
            i = qc->scl->sml.rbegin();
        } else {
            ++i;
        }
        return i != qc->scl->sml.rend();
    }

    DLLLOCAL const QoreClass* get() const {
        if (do_class) {
            return qc->cls;
        }
        assert(valid());
        return i->first;
    }

    DLLLOCAL bool valid() const {
        return do_class || (qc->scl && i != qc->scl->sml.rend());
    }

    DLLLOCAL bool isVirtual() const {
        if (do_class) {
            return false;
        }
        assert(valid());
        return i->second;
    }

private:
    const qore_class_private* qc;
    BCSMList::const_reverse_iterator i;
    bool do_class = false;
};

QoreClassDestructorHierarchyIterator::QoreClassDestructorHierarchyIterator(const QoreClass* cls) : priv(new qore_class_destructor_hierarchy_iterator(qore_class_private::get(*cls))) {
}

QoreClassDestructorHierarchyIterator::~QoreClassDestructorHierarchyIterator() {
    delete priv;
}

bool QoreClassDestructorHierarchyIterator::next() {
    return priv->next();
}

bool QoreClassDestructorHierarchyIterator::valid() const {
    return priv->valid();
}

const QoreClass* QoreClassDestructorHierarchyIterator::get() const {
    return priv->get();
}

bool QoreClassDestructorHierarchyIterator::isVirtual() const {
    return priv->isVirtual();
}
