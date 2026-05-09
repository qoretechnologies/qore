/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreClosureNode.h

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

#ifndef _QORE_QORECLOSURENODE_H

#define _QORE_QORECLOSURENODE_H

#include "qore/intern/QoreObjectIntern.h"

#include <map>

class QoreClosureSelfContextHelper {
private:
    const QoreObject* prev;

public:
    DLLLOCAL QoreClosureSelfContextHelper(const QoreObject* obj);
    DLLLOCAL ~QoreClosureSelfContextHelper();
};

DLLLOCAL bool qore_closure_self_context(const QoreObject* obj);

class CVecInstantiator {
protected:
    cvv_vec_t* cvec;
    ExceptionSink* xsink;

public:
    DLLLOCAL CVecInstantiator(cvv_vec_t* cv, ExceptionSink* xs) : cvec(cv), xsink(xs) {
        if (!cvec) {
            return;
        }
        for (cvv_vec_t::iterator i = cvec->begin(), e = cvec->end(); i != e; ++i) {
            thread_instantiate_closure_var((*i)->refSelf());
        }
    }

    DLLLOCAL ~CVecInstantiator() {
        if (!cvec) {
            return;
        }
        // elements are dereferenced when uninstantiated
        for (cvv_vec_t::iterator i = cvec->begin(), e = cvec->end(); i != e; ++i) {
            thread_uninstantiate_closure_var(xsink);
        }
    }
};

class QoreClosureBase : public ResolvedCallReferenceNode {
protected:
    const QoreClosureParseNode* closure;
    mutable ThreadSafeLocalVarRuntimeEnvironment closure_env;
    cvv_vec_t* cvec;
    const qore_class_private* class_ctx;

    DLLLOCAL void del(ExceptionSink* xsink) {
        // cvec and closure_env both reference ClosureVarValues captured by the closure;
        // cmap (closure_env) is a strict subset of cvec (same-thread capture). To avoid
        // double-holding refs on CVVs in both containers — which breaks DGC cycle
        // detection because the rsection scan walks only cmap and undercounts rcount —
        // cvec drops its ref on CVVs that are in cmap at construction time. Here, match
        // that by derefing only cvec-only entries; cmap entries are derefed by
        // closure_env.del().
        if (cvec) {
            for (cvv_vec_t::iterator i = cvec->begin(), e = cvec->end(); i != e; ++i) {
                if (!closure_env.hasVar(*i)) {
                    (*i)->deref(xsink);
                }
            }
            delete cvec;
#ifdef DEBUG
            cvec = nullptr;
#endif
        }
        closure_env.del(xsink);
    }

public:
   //! constructor is not exported outside the library
    DLLLOCAL QoreClosureBase(const QoreClosureParseNode* n_closure, cvv_vec_t* cv,
            const qore_class_private* class_ctx)
            : ResolvedCallReferenceNode(false, NT_RUNTIME_CLOSURE), closure(n_closure),
            closure_env(n_closure->getVList()), cvec(cv), class_ctx(class_ctx) {
        //printd(5, "QoreClosureBase::QoreClosureBase() this: %p closure: %p\n", this, closure);
        closure->ref();
        // thread_get_all_closure_vars() ref'd every CVV in cv; closure_env
        // additionally ref'd each CVV that's in its cmap (subset of cv).
        // That duplicate ref on cmap-members masks cycles from the rsection
        // scan (which walks only cmap), so drop it here. The remaining cvec
        // refs cover CVVs captured for lexical scope but not referenced by
        // the closure body.
        if (cvec) {
            for (cvv_vec_t::iterator i = cvec->begin(), e = cvec->end(); i != e; ++i) {
                if (closure_env.hasVar(*i)) {
                    (*i)->deref(nullptr);
                }
            }
        }
    }

    DLLLOCAL ~QoreClosureBase() {
        //printd(5, "QoreClosureBase::~QoreClosureBase() this: %p closure: %p\n", this, closure);
        const_cast<QoreClosureParseNode*>(closure)->deref();
        assert(!cvec);
    }

    DLLLOCAL ClosureVarValue* find(const LocalVar* id) const {
        return closure_env.find(id);
    }

    DLLLOCAL bool hasVar(ClosureVarValue* cvv) const {
        return closure_env.hasVar(cvv);
    }

    DLLLOCAL const cvar_map_t& getMap() const {
        return closure_env.getMap();
    }

    // returns true if at least one variable in the set of closure-bound local variables could contain an object or a
    // closure (also through a container)
    DLLLOCAL bool needsScan() const {
        return closure->needsScan();
    }

    DLLLOCAL static const char* getStaticTypeName() {
        return "closure";
    }

    DLLLOCAL virtual QoreFunction* getFunction() const {
        return closure->getFunction();
    }

    DLLLOCAL virtual QoreObject* getObject() const {
        return nullptr;
    }

    DLLLOCAL virtual QoreObject* refSelfForBackground(ExceptionSink* xsink) const {
        return nullptr;
    }

    //! Returns the full typed code type info (e.g., code<int(string)>) from the closure's signature
    DLLLOCAL const QoreTypeInfo* getCallTypeInfo() const;
};

class QoreClosureNode : public QoreClosureBase {
private:
    QoreProgram* pgm;

    DLLLOCAL QoreClosureNode(const QoreClosureNode&); // not implemented
    DLLLOCAL QoreClosureNode& operator=(const QoreClosureNode&); // not implemented

protected:
    DLLLOCAL virtual bool derefImpl(ExceptionSink* xsink);

public:
    DLLLOCAL QoreClosureNode(const QoreClosureParseNode* n_closure, cvv_vec_t* cv = nullptr,
            const qore_class_private* class_ctx = nullptr) : QoreClosureBase(n_closure, cv, class_ctx),
            pgm(::getProgram()) {
        pgm->depRef();
    }

    DLLLOCAL virtual ~QoreClosureNode() {
    }

    DLLLOCAL virtual QoreValue execValue(const QoreListNode* args, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreProgram* getProgram() const {
        return pgm;
    }

    //! returns false unless perl-boolean-evaluation is enabled, in which case it returns true
    /** @return false unless perl-boolean-evaluation is enabled, in which case it returns true
     */
    DLLEXPORT virtual bool getAsBoolImpl() const;

    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
        str.sprintf("function closure (%slambda, %p)", closure->isLambda() ? "" : "non-", this);
        return 0;
    }

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const {
        del = true;
        QoreString* rv = new QoreString;
        getAsString(*rv, foff, xsink);
        return rv;
    }

    DLLLOCAL virtual const char* getTypeName() const {
        return getStaticTypeName();
    }

    DLLLOCAL bool isLambda() const { return closure->isLambda(); }

    DLLLOCAL virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
        return QoreClosureNode::is_equal_hard(v, xsink);
    }

    DLLLOCAL virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
        return v == this;
    }
};

class QoreObjectClosureNode : public QoreClosureBase {
public:
    DLLLOCAL QoreObjectClosureNode(QoreObject* n_obj, const qore_class_private* c_ctx,
            const QoreClosureParseNode* n_closure, cvv_vec_t* cv = nullptr)
            : QoreClosureBase(n_closure, cv, c_ctx),
                obj(n_obj) {
        obj->tRef();
    }

    DLLLOCAL ~QoreObjectClosureNode() {
        assert(!obj);
    }

    DLLLOCAL virtual QoreValue execValue(const QoreListNode* args, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreProgram* getProgram() const {
        return obj->getProgram();
    }

    DLLLOCAL virtual QoreObject* refSelfForBackground(ExceptionSink* xsink) const;

    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
        str.sprintf("function closure (%slambda, in object of class '%s', %p)",
            closure->isLambda() ? "" : "non-", obj->getClassName(), this);
        return 0;
    }

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const {
        del = true;
        QoreString* rv = new QoreString;
        getAsString(*rv, foff, xsink);
        return rv;
    }

    DLLLOCAL virtual const char* getTypeName() const {
        return getStaticTypeName();
    }

    DLLLOCAL bool isLambda() const { return closure->isLambda(); }

    DLLLOCAL virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
        return QoreObjectClosureNode::is_equal_hard(v, xsink);
    }

    DLLLOCAL virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
        return v == this;
    }

    DLLLOCAL virtual QoreObject* getObject() const {
        return obj;
    }

protected:
    DLLLOCAL virtual bool derefImpl(ExceptionSink* xsink);

private:
    QoreObject* obj;

    QoreObjectClosureNode(const QoreObjectClosureNode&) = delete;
    QoreObjectClosureNode& operator=(const QoreObjectClosureNode&) = delete;
};

#endif
