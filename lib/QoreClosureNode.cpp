/*
    QoreClosureNode.cpp

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
#include "qore/intern/QoreTypeInfo.h"

const QoreTypeInfo* QoreClosureBase::getCallTypeInfo() const {
    UserClosureFunction* f = closure->getFunction();
    if (f) {
        AbstractFunctionSignature* sig = f->getUniqueSignature();
        if (sig) {
            return qore_get_complex_code_type_from_signature(sig);
        }
    }
    return nullptr;
}

ThreadSafeLocalVarRuntimeEnvironmentHelper::ThreadSafeLocalVarRuntimeEnvironmentHelper(
        const QoreClosureBase* current
    ) : prev(thread_set_runtime_closure_env(current)) {
    //printd(5, "ThreadSafeLocalVarRuntimeEnvironmentHelper::ThreadSafeLocalVarRuntimeEnvironmentHelper() prev: %p "
    //    "current: %p\n", prev, current);
}

ThreadSafeLocalVarRuntimeEnvironmentHelper::~ThreadSafeLocalVarRuntimeEnvironmentHelper() {
    //printd(5, "ThreadSafeLocalVarRuntimeEnvironmentHelper::~ThreadSafeLocalVarRuntimeEnvironmentHelper() "
    //    "prev: %p\n", prev);
    thread_set_runtime_closure_env(prev);
}

ThreadSafeLocalVarRuntimeEnvironment::ThreadSafeLocalVarRuntimeEnvironment(const lvar_set_t* vlist) {
    //printd(5, "ThreadSafeLocalVarRuntimeEnvironment::ThreadSafeLocalVarRuntimeEnvironment() this: %p vlist: %p "
    //    "size: %d\n", this, vlist, vlist->size());
    for (lvar_set_t::const_iterator i = vlist->begin(), e = vlist->end(); i != e; ++i) {
        ClosureVarValue* cvar = thread_find_closure_var((*i)->getName());
        //printd(5, "ThreadSafeLocalVarRuntimeEnvironment::ThreadSafeLocalVarRuntimeEnvironment() this: %p '%s' "
        //    "i: %p cvar: %p val: %s\n", this, (*i)->getName(), *i, cvar, cvar->val.getTypeName());
        cmap[*i] = cvar;
        cvvset.insert(cvar);
        cvar->ref();
    }
}

ThreadSafeLocalVarRuntimeEnvironment::~ThreadSafeLocalVarRuntimeEnvironment() {
    //printd(5, "ThreadSafeLocalVarRuntimeEnvironment::~ThreadSafeLocalVarRuntimeEnvironment() this: %p\n", this);
    assert(cmap.empty());
    assert(cvvset.empty());
}

ClosureVarValue* ThreadSafeLocalVarRuntimeEnvironment::find(const LocalVar* id) const {
    //printd(5, "ThreadSafeLocalVarRuntimeEnvironment::find(%p '%s') this: %p\n", id, id->getName(), this);
    cvar_map_t::const_iterator i = cmap.find(id);
    // Return nullptr if not found — the caller (LocalVar::eval(), isRef(), getLValue(), remove())
    // falls back to thread_find_closure_var(name) which searches the cvstack by name.
    // This happens when a function with closure_use locals is called from within a
    // different closure's execution context (closure_rt_env points to the calling closure).
    return i != cmap.end() ? i->second : nullptr;
}

bool ThreadSafeLocalVarRuntimeEnvironment::hasVar(ClosureVarValue* cvv) const {
    return cvvset.find(cvv) != cvvset.end();
}

void ThreadSafeLocalVarRuntimeEnvironment::del(ExceptionSink* xsink) {
    //printd(5, "ThreadSafeLocalVarRuntimeEnvironment::del() this: %p\n", this);
    for (cvar_map_t::iterator i = cmap.begin(), e = cmap.end(); i != e; ++i)
        i->second->deref(xsink);

#ifdef DEBUG
    cmap.clear();
    cvvset.clear();
#endif
}

bool QoreClosureNode::derefImpl(ExceptionSink* xsink) {
    del(xsink);
    pgm->depDeref();
    return ResolvedCallReferenceNode::derefImpl(xsink);
}

QoreValue QoreClosureNode::execValue(const QoreListNode* args, ExceptionSink* xsink) const {
    CVecInstantiator cvi(cvec, xsink);
    return closure->exec(*this, pgm, args, 0, class_ctx, xsink);
}

bool QoreClosureNode::getAsBoolImpl() const {
    // check if we should do perl-style boolean evaluation
    QoreProgram* pgm = getProgram();
    if (pgm && runtime_check_parse_option(PO_STRICT_BOOLEAN_EVAL))
        return false;
    return true;
}

bool QoreObjectClosureNode::derefImpl(ExceptionSink* xsink) {
    del(xsink);
    obj->tDeref();
#ifdef DEBUG
    obj = nullptr;
#endif
    return ResolvedCallReferenceNode::derefImpl(xsink);
}

QoreValue QoreObjectClosureNode::execValue(const QoreListNode* args, ExceptionSink* xsink) const {
    CVecInstantiator cvi(cvec, xsink);
    return closure->exec(*this, 0, args, obj, class_ctx, xsink);
}
