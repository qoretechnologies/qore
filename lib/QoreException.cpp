/*
    QoreException.cpp

    Qore programming language exception handling support

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

#include <qore/Qore.h>
#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/QoreThreadList.h"

// Declare JIT stack flag from QoreJIT.cpp
extern bool is_jit_cleared_stack();

#include <qore/safe_dslist>

#include <cassert>
#include <cstdio>

//! add an element to the end of the stack trace
void QoreCallStack::add(qore_call_t type, const char* label, int start, int end, const char* code,
        const char* lang) {
    // issue #4030 insert at the beginning of the call stack, since later the order will be reversed
    insert(begin(), QoreCallStackElement(type, label, start, end, code, lang));
}

void QoreCallStack::add(qore_call_t type, const char* label, int start, int end, const char* source,
        unsigned offset, const char* code, const char* lang) {
    // issue #4030 insert at the beginning of the call stack, since later the order will be reversed
    insert(begin(), QoreCallStackElement(type, label, start, end, source, offset, code, lang));
}

QoreExceptionBase::QoreExceptionBase(QoreValue n_err, QoreValue n_desc, QoreValue n_arg, qore_call_t n_type)
        : type(n_type), err(n_err), desc(n_desc), arg(n_arg) {
    // Skip call stack building if JIT execution cleared the stack location.
    // When JIT clears the stack to prevent dangling pointers, builtin functions
    // may create their own stack location objects that become invalid during exception
    // unwinding. Skipping the call stack avoids crashes in getCallStackHash().
    if (is_jit_cleared_stack()) {
        return;
    }

    // populate call stack normally
    const QoreStackLocation* w = get_runtime_stack_location();
    while (w) {
        callStack->push(QoreThreadList::getCallStackHash(*w), nullptr);
        w = w->getNext();
    }
}

void QoreException::del(ExceptionSink* xsink) {
    if (callStack) {
        //printd(5, "QoreException::del() this: %p callStack: %p (r: %d)\n", this, callStack, callStack->reference_count());
        callStack->deref(xsink);
#ifdef DEBUG
        callStack = nullptr;
#endif
    }
    err.discard(xsink);
    desc.discard(xsink);
    arg.discard(xsink);
    if (next) {
        next->del(xsink);
    }

    delete this;
}

class QoreExceptionHolder {
public:
    DLLLOCAL QoreExceptionHolder(QoreException* e) : e(e) {
    }

    DLLLOCAL ~QoreExceptionHolder() {
        if (e) {
            e->del(nullptr);
        }
    }

    DLLLOCAL QoreException* release() {
        QoreException* rv = e;
        e = nullptr;
        return rv;
    }

    DLLLOCAL QoreException* operator->() {
        return e;
    }

private:
    QoreException * e;
};

QoreException* QoreException::replaceTop(const QoreListNode& new_ex, ExceptionSink& xsink) {
    if (new_ex.size() > 0) {
        err.discard(&xsink);
        err = new_ex.getReferencedEntry(0);
        if (new_ex.size() > 1) {
            desc.discard(&xsink);
            desc = new_ex.getReferencedEntry(1);
            if (new_ex.size() > 2) {
                arg.discard(&xsink);
                arg = new_ex.getReferencedEntry(2);
            }
        }
    }
    return this;
}

QoreException* QoreException::rethrow() {
    QoreExceptionHolder e(new QoreException(*this));

    // insert current position as a rethrow entry in the new callstack
    QoreListNode* l = e->callStack;
    const char *fn = nullptr;
    QoreHashNode* n = l->retrieveEntry(0).get<QoreHashNode>();
    // get function name
    fn = !n ? "<unknown>" : n->getKeyValue("function").get<QoreStringNode>()->c_str();

    const QoreProgramLocation* loc = get_runtime_location();
    if (loc) {
        l->insert(QoreThreadList::getCallStackHash(CT_RETHROW, fn, *loc), nullptr);
    } else {
        static QoreProgramLocation default_loc;
        l->insert(QoreThreadList::getCallStackHash(CT_RETHROW, fn, default_loc), nullptr);
    }
    return e.release();
}

int QORE_MAX_EXCEPTIONS = 20;

QoreHashNode* QoreException::makeExceptionObject(int level) const {
    QORE_TRACE("makeExceptionObject()");

    QoreHashNode* h = new QoreHashNode(hashdeclExceptionInfo, nullptr);
    auto ph = qore_hash_private::get(*h);

    QoreExceptionLocation loc;
    getLocation(loc);

    ph->setKeyValueIntern("type", new QoreStringNode(type == CT_USER ? "User" : "System"));
    ph->setKeyValueIntern("file", new QoreStringNode(loc.file));
    ph->setKeyValueIntern("line", loc.start_line);
    ph->setKeyValueIntern("endline", loc.end_line);
    ph->setKeyValueIntern("source", new QoreStringNode(loc.source));
    ph->setKeyValueIntern("offset", loc.offset);
    ph->setKeyValueIntern("lang", new QoreStringNode(loc.lang));
    ph->setKeyValueIntern("callstack", callStack->refSelf());
    if (err) {
        ph->setKeyValueIntern("err", err.refSelf());
    }
    if (desc) {
        ph->setKeyValueIntern("desc", desc.refSelf());
    }
    if (arg) {
        ph->setKeyValueIntern("arg", arg.refSelf());
    }

    // add chained exceptions with this "chain reaction" call
    if (next) {
         if (level < QORE_MAX_EXCEPTIONS) {
             ph->setKeyValueIntern("next", next->makeExceptionObject(level + 1));
         }
    }

    return h;
}

QoreHashNode *QoreException::makeExceptionObjectAndDelete(ExceptionSink *xsink) {
    QORE_TRACE("makeExceptionObjectAndDelete()");
    QoreHashNode* rv = makeExceptionObject();
    del(xsink);

    return rv;
}

void QoreException::addStackInfo(QoreHashNode* n) {
    //printd(5, "QoreException::addStackInfo() this: %p callStack: %p (r: %d) n: %p (nr: %d)\n", this, callStack, callStack->reference_count(), n, n->reference_count());
    callStack->insert(n, nullptr);
}

const char* QoreException::getType(qore_call_t type) {
    switch (type) {
        case CT_USER:
            return "user";
        case CT_BUILTIN:
            return "builtin";
        case CT_RETHROW:
            return "rethrow";
        case CT_NEWTHREAD:
            return "new-thread";
        default:
            break;
    }
    assert(false);
    return 0;
}

// static function
QoreHashNode* QoreException::getStackHash(const QoreCallStackElement& cse) {
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(hashdeclCallStackInfo, nullptr), nullptr);

    qore_hash_private* ph = qore_hash_private::get(**h);

    assert(!cse.code.empty());
    ph->setKeyValueIntern("function", new QoreStringNode(cse.code));
    ph->setKeyValueIntern("line",     cse.start_line);
    ph->setKeyValueIntern("endline",  cse.end_line);
    ph->setKeyValueIntern("file",     new QoreStringNode(cse.label));
    // do not set "source" to NOTHING, as it must have a value according to the CallStackInfo hashdecl
    if (!cse.source.empty()) {
        ph->setKeyValueIntern("source", new QoreStringNode(cse.source));
    }
    ph->setKeyValueIntern("offset",   cse.offset);
    ph->setKeyValueIntern("lang",     new QoreStringNode(cse.lang));
    ph->setKeyValueIntern("typecode", cse.type);
    ph->setKeyValueIntern("type",     new QoreStringNode(getType(cse.type)));

    return h.release();
}

DLLLOCAL ParseExceptionSink::~ParseExceptionSink() {
    if (xsink) {
        qore_program_private::addParseException(getProgram(), xsink);
    }
}

// creates a stack trace node and adds it to all exceptions in this sink
void qore_es_private::addStackInfo(qore_call_t type, const char *class_name, const char *code,
    const QoreProgramLocation& loc) {
    assert(head);
    QoreString str;
    if (class_name) {
        str.sprintf("%s::", class_name);
    }
    str.concat(code);
    QoreHashNode* n = QoreThreadList::getCallStackHash(type, str.c_str(), loc);

    assert(head);
    QoreException* w = head;
    while (w) {
        w->addStackInfo(n);
        w = w->next;
        if (w)
            n->ref();
    }
}
