/*
    QoreException.cpp

    Qore programming language exception handling support

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
#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/QoreThreadList.h"
#include "qore/intern/qore_thread_intern.h"


#include <qore/safe_dslist>

#include <atomic>
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
    // populate call stack
    const QoreStackLocation* w = get_runtime_stack_location();
    while (w) {
        callStack->push(QoreThreadList::getCallStackHash(*w), nullptr);
        w = w->getNext();
    }
    // an exception that code that is sandboxed or acts for sandboxed code raises is reported marked, wherever and in
    // whichever thread it is reported, so that the text of its choosing cannot pass for the host's own (qore#7139)
    sandboxed = qore_code_origin_sandboxed(&sandboxed_pgm_id);
}

void QoreException::del(ExceptionSink* xsink) {
    // the chained exceptions are deleted in a loop, as a chain can be long, for example after many parse errors
    QoreException* e = this;
    while (e) {
        QoreException* n = e->next;
        if (e->callStack) {
            //printd(5, "QoreException::del() this: %p callStack: %p (r: %d)\n", e, e->callStack, e->callStack->reference_count());
            e->callStack->deref(xsink);
#ifdef DEBUG
            e->callStack = nullptr;
#endif
        }
        e->err.discard(xsink);
        e->desc.discard(xsink);
        e->arg.discard(xsink);
        delete e;
        e = n;
    }
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
    // a rethrow with new values by code that is sandboxed or acts for sandboxed code gives the exception text of its
    // choosing: it is reported marked from then on (qore#7139)
    if (new_ex.size() > 0) {
        qore_program_id_t pgm_id = 0;
        if (qore_code_origin_sandboxed(&pgm_id)) {
            sandboxed = true;
            sandboxed_pgm_id = pgm_id;
        }
    }
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

int64 QoreException::getDebugId() {
    static std::atomic<int64> qore_exception_debug_id_seq{0};
    if (!debug_id) {
        debug_id = ++qore_exception_debug_id_seq;
    }
    return debug_id;
}

QoreException* QoreException::rethrow() {
    QoreExceptionHolder e(new QoreException(*this));

    // insert current position as a rethrow entry in the new callstack
    QoreListNode* l = e->callStack;
    const char *fn = nullptr;
    QoreHashNode* n = l->retrieveEntry(0).get<QoreHashNode>();
    // get function name
    std::string fn_storage;
    if (n) {
        QoreStringValueHelper fn_str(n->getKeyValue("function"));
        fn_storage = fn_str->c_str();
        fn = fn_storage.c_str();
    } else {
        fn = "<unknown>";
    }

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

// Gives a location's path to code restricted from external information: the file name of the source, as the
// directories of the host are external information (qore#7507); out of line, as only such code calls it
static QoreStringNode* q_restricted_location_path(const std::string& path) {
    return new QoreStringNode(qore_path_file_name(path.c_str()));
}

void qore_restrict_location_paths(QoreHashNode& h) {
    assert(h.is_unique());
    qore_hash_private* ph = qore_hash_private::get(h);
    for (const char* key : {"file", "source"}) {
        // the value can be an inline short string
        QoreStringDataHelper str(h.getKeyValue(key));
        if (str) {
            ph->setKeyValueIntern(key, new QoreStringNode(qore_path_file_name(str.c_str()), str.getEncoding()));
        }
    }
}

QoreListNode* qore_restrict_call_stack_paths(const QoreListNode& stack) {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(stack.getValueTypeInfo()), nullptr);
    ConstListIterator i(&stack);
    while (i.next()) {
        QoreValue v = i.getValue();
        if (v.getType() == NT_HASH) {
            ReferenceHolder<QoreHashNode> h(v.get<const QoreHashNode>()->copy(), nullptr);
            qore_restrict_location_paths(**h);
            rv->push(h.release(), nullptr);
        } else {
            rv->push(v.refSelf(), nullptr);
        }
    }
    return rv.release();
}

void qore_restrict_call_stack_paths_in_place(QoreListNode& stack) {
    assert(stack.is_unique());
    ListIterator i(&stack);
    while (i.next()) {
        QoreValue v = i.getValue();
        if (v.getType() == NT_HASH) {
            assert(v.get<QoreHashNode>()->is_unique());
            qore_restrict_location_paths(*v.get<QoreHashNode>());
        }
    }
}

QoreHashNode* QoreException::makeExceptionObject(int level) const {
    // the code running on the thread is the code that receives the exception (a catch block, for example)
    return makeExceptionObjectFor(runtime_no_external_info(), level);
}

QoreHashNode* QoreException::makeExceptionObjectFor(bool no_external_info, int level) const {
    QORE_TRACE("makeExceptionObject()");

    QoreHashNode* h = new QoreHashNode(hashdeclExceptionInfo, nullptr);
    auto ph = qore_hash_private::get(*h);

    QoreExceptionLocation loc;
    getLocation(loc);

    ph->setKeyValueIntern("type", new QoreStringNode(type == CT_USER ? "User" : "System"));
    ph->setKeyValueIntern("file", no_external_info ? q_restricted_location_path(loc.file)
        : new QoreStringNode(loc.file));
    ph->setKeyValueIntern("line", loc.start_line);
    ph->setKeyValueIntern("endline", loc.end_line);
    ph->setKeyValueIntern("source", no_external_info ? q_restricted_location_path(loc.source)
        : new QoreStringNode(loc.source));
    ph->setKeyValueIntern("offset", loc.offset);
    ph->setKeyValueIntern("lang", new QoreStringNode(loc.lang));
    // the call stack is shared with the exception, except for code restricted from external information, which is
    // given a copy with the file names of the sources only
    ph->setKeyValueIntern("callstack", no_external_info ? qore_restrict_call_stack_paths(*callStack)
        : callStack->refSelf());
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
             ph->setKeyValueIntern("next", next->makeExceptionObjectFor(no_external_info, level + 1));
         }
    }

    return h;
}

QoreHashNode* QoreException::makeExceptionObjectWithoutChain() const {
    // the chain is added only below the maximum level
    return makeExceptionObject(QORE_MAX_EXCEPTIONS);
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
