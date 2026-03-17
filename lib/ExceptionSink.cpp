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
#include "qore/intern/QoreException.h"

#include <cstdlib>

#define Q_MAX_EXCEPTIONS 10

void qore_es_private::assimilate(qore_es_private& xs) {
    if (xs.thread_exit) {
        thread_exit = xs.thread_exit;
        xs.thread_exit = false;
    }
    if (xs.tail) {
        assert(xs.head);
        if (tail) {
            tail->next = xs.head;
        } else {
            head = xs.head;
        }
        tail = xs.tail;

        xs.head = xs.tail = nullptr;
        // NOTE: xs.count can be 0 if the object is externally managed
        if (!xs.externally_managed) {
            assert(xs.count);
            count += xs.count;
            xs.count = 0;
        } else {
            assert(!xs.count);
        }
    } else {
        assert(!xs.head);
        assert(!xs.count);
    }
}

void qore_es_private::insert(QoreException *e) {
    // append exception to the list
    if (!head) {
        head = e;
    } else {
        tail->next = e;
    }
    tail = e;

    // increment exception counts
    if (!externally_managed) {
        ++count;
        inc_active_exceptions(1);
    }
}

ExceptionSink::ExceptionSink() : priv(new qore_es_private) {
}

ExceptionSink::~ExceptionSink() {
    if (priv) {
        handleExceptions();
        delete priv;
    }
}

void ExceptionSink::raiseThreadExit() {
    priv->thread_exit = true;
}

bool ExceptionSink::isEvent() const {
    return priv->head || priv->thread_exit;
}

bool ExceptionSink::isThreadExit() const {
    return priv->thread_exit;
}

bool ExceptionSink::isException() const {
    return priv->head;
}

// Intended as a alternative to isException():
// ExceptionSink xsink;
// if (xsink) { .. }
ExceptionSink::operator bool () const {
    assert(this);
    return priv->head || priv->thread_exit;
}

void ExceptionSink::overrideLocation(const QoreProgramLocation& loc) {
    //printd(5, "ExceptionSink::overrideLocation() loc: %p: %s:%d\n", &loc, loc.getFileValue(), loc.start_line);
    QoreException* w = priv->head;
    while (w) {
        w->set(loc);
        w = w->next;
    }
}

void ExceptionSink::setLastLocation(const QoreProgramLocation& loc) {
    if (priv->tail) {
        priv->tail->set(loc);
    }
}

QoreException* ExceptionSink::catchException() {
    QoreException* e = priv->head;
    priv->head = priv->tail = nullptr;
    if (priv->count) {
        assert(!priv->externally_managed);
        inc_active_exceptions(-priv->count);
        priv->count = 0;
    }
    return e;
}

QoreException* ExceptionSink::getException() {
    return priv->head;
}

void ExceptionSink::handleExceptions() {
    if (priv->head) {
        defaultExceptionHandler(priv->head);
        clear();
    } else {
        priv->thread_exit = false;
    }
}

void ExceptionSink::handleWarnings() {
    if (priv->head) {
        defaultWarningHandler(priv->head);
        clear();
    }
}

void ExceptionSink::markExternallyManaged() {
    if (!priv->externally_managed) {
        priv->externally_managed = true;
        if (priv->count) {
            inc_active_exceptions(-priv->count);
            priv->count = 0;
        }
    }
}

QoreHashNode* ExceptionSink::getExceptionInfo() {
    QoreException* except = catchException();
    if (!except) {
        return nullptr;
    }
    ExceptionSink xsink2;
    ReferenceHolder<QoreHashNode> rv(except->makeExceptionObject(), &xsink2);
    except->del(&xsink2);
    xsink2.clear();
    return rv.release();
}

void ExceptionSink::clear() {
    if (priv->count) {
        assert(!priv->externally_managed);
        inc_active_exceptions(-priv->count);
        priv->count = 0;
    }
    priv->clearIntern();
    priv->head = priv->tail = nullptr;
    priv->thread_exit = false;
}

const QoreValue ExceptionSink::getExceptionErr() {
    return priv->head ? priv->head->err : QoreValue();
}

const QoreValue ExceptionSink::getExceptionDesc() {
    return priv->head ? priv->head->desc : QoreValue();
}

const QoreValue ExceptionSink::getExceptionArg() {
    return priv->head ? priv->head->arg : QoreValue();
}

int ExceptionSink::appendLastDescription(const char* fmt, ...) {
    if (!priv->head || priv->head->desc.getType() != NT_STRING) {
        return -1;
    }

    SimpleRefHolder<QoreStringNode> new_desc(new QoreStringNode);

    va_list args;

    while (true) {
        va_start(args, fmt);
        int rc = new_desc->vsprintf(fmt, args);
        va_end(args);
        if (!rc) {
            break;
        }
    }

    QoreStringNode* old_desc = priv->head->desc.get<QoreStringNode>();
    old_desc->concat(new_desc->c_str(), new_desc->size());
    return 0;
}

int ExceptionSink::renamePrependLastException(const char* err, const char* desc_fmt, ...) {
    if (!priv->head || priv->head->desc.getType() != NT_STRING || priv->head->err.getType() != NT_STRING) {
        return -1;
    }

    SimpleRefHolder<QoreStringNode> new_desc(new QoreStringNode);
    va_list args;
    while (true) {
        va_start(args, desc_fmt);
        int rc = new_desc->vsprintf(desc_fmt, args);
        va_end(args);
        if (!rc) {
            break;
        }
    }

    QoreStringNode* old_desc = priv->head->desc.get<QoreStringNode>();
    old_desc->prepend(new_desc->c_str(), new_desc->size());

    priv->head->err.discard(nullptr);
    priv->head->err = new QoreStringNode(err);
    return 0;
}

AbstractQoreNode* ExceptionSink::raiseException(const char *err, const char *fmt, ...) {
    QoreStringNode* desc = new QoreStringNode;

    va_list args;

    while (true) {
        va_start(args, fmt);
        int rc = desc->vsprintf(fmt, args);
        va_end(args);
        if (!rc) {
            break;
        }
    }
    printd(5, "ExceptionSink::raiseException(%s, %s)\n", err, desc->getBuffer());
    priv->insert(new QoreException(err, desc));
    return nullptr;
}

AbstractQoreNode* ExceptionSink::raiseErrnoException(const char *err, int en, QoreStringNode* desc) {
    // append strerror(en) to description
    desc->concat(": ");
    q_strerror(*desc, en);

    printd(5, "ExceptionSink::raiseException(%s, %s)\n", err, desc->getBuffer());
    priv->insert(new QoreException(err, desc, en));
    return nullptr;
}

AbstractQoreNode* ExceptionSink::raiseErrnoException(const char *err, int en, const char *fmt, ...) {
    QoreStringNode* desc = new QoreStringNode;

    va_list args;

    while (true) {
        va_start(args, fmt);
        int rc = desc->vsprintf(fmt, args);
        va_end(args);
        if (!rc) {
            break;
        }
    }

    return raiseErrnoException(err, en, desc);
}

// returns nullptr, takes ownership of the "desc" argument
AbstractQoreNode* ExceptionSink::raiseException(const char *err, QoreStringNode* desc) {
    printd(5, "ExceptionSink::raiseException(%s, %s)\n", err, desc->getBuffer());
    priv->insert(new QoreException(err, desc));
    return nullptr;
}

// returns nullptr, takes ownership of the "desc" argument
AbstractQoreNode* ExceptionSink::raiseException(QoreStringNode *err, QoreStringNode *desc) {
    printd(5, "ExceptionSink::raiseException(%s, %s)\n", err->c_str(), desc->c_str());
    priv->insert(new QoreException(err, desc));
    return nullptr;
}

// returns nullptr, takes ownership of the "desc" argument
AbstractQoreNode* ExceptionSink::raiseException(QoreStringNode *err, QoreStringNode *desc, QoreValue arg) {
    printd(5, "ExceptionSink::raiseException(%s, %s)\n", err->c_str(), desc->c_str());
    priv->insert(new QoreException(err, desc, arg));
    return nullptr;
}

AbstractQoreNode* ExceptionSink::raiseExceptionArg(const char* err, QoreValue arg, QoreStringNode *desc) {
    printd(5, "ExceptionSink::raiseExceptionArg(%s, %s)\n", err, desc->getBuffer());
    QoreException* exc = new QoreException(err, desc);
    exc->arg = arg;
    priv->insert(exc);
    return nullptr;
}

AbstractQoreNode* ExceptionSink::raiseExceptionArg(const char* err, QoreValue arg, QoreStringNode *desc,
        const QoreCallStack& stack) {
    printd(5, "ExceptionSink::raiseExceptionArg(%s, %s, %p)\n", err, desc->getBuffer(), &stack);
    QoreException* exc = new QoreException(err, desc);
    exc->arg = arg;
    priv->insert(exc);
    priv->addStackInfo(stack);
    return nullptr;
}

AbstractQoreNode* ExceptionSink::raiseExceptionArg(const QoreProgramLocation& loc, const char* err, QoreValue arg,
        QoreStringNode *desc) {
    printd(5, "ExceptionSink::raiseExceptionArg(loc, %s, %s)\n", err, desc->getBuffer());
    priv->insert(new QoreException(loc, err, desc, arg));
    return nullptr;
}

AbstractQoreNode* ExceptionSink::raiseExceptionArg(const QoreProgramLocation& loc, const char* err, QoreValue arg,
        QoreStringNode *desc, const QoreCallStack& stack) {
    printd(5, "ExceptionSink::raiseExceptionArg(loc, %s, %s, %p)\n", err, desc->getBuffer(), &stack);
    priv->insert(new QoreException(loc, err, desc, arg));
    priv->addStackInfo(stack);
    return nullptr;
}

AbstractQoreNode* ExceptionSink::raiseExceptionArg(const char* err, QoreValue arg, const char* fmt, ...) {
    QoreStringNode *desc = new QoreStringNode;

    va_list args;

    while (true) {
        va_start(args, fmt);
        int rc = desc->vsprintf(fmt, args);
        va_end(args);
        if (!rc) {
            break;
        }
    }
    printd(5, "ExceptionSink::raiseExceptionArg(%s, %s)\n", err, desc->getBuffer());
    QoreException* exc = new QoreException(err, desc);
    exc->arg = arg;
    priv->insert(exc);
    return nullptr;
}

void ExceptionSink::raiseException(QoreException* e) {
    priv->insert(e);
}

void ExceptionSink::raiseException(const QoreListNode* n) {
    priv->insert(new QoreException(n));
}

void ExceptionSink::raiseException(const QoreProgramLocation& loc, const char* err, QoreValue arg, QoreValue desc) {
    printd(5, "ExceptionSink::raiseExceptionArg(%s, %s)\n", err,
        desc.getType() == NT_STRING ? desc.get<QoreStringNode>()->c_str() : desc.getTypeName());
    priv->insert(new QoreException(loc, err, desc, arg));
}

void ExceptionSink::raiseException(const QoreProgramLocation &loc, const char *err, QoreValue arg,
        const char *fmt, ...) {

    QoreStringNodeHolder desc(new QoreStringNode);

    va_list args;

    while (true) {
        va_start(args, fmt);
        int rc = desc->vsprintf(fmt, args);
        va_end(args);
        if (!rc) {
            break;
        }
    }

    raiseException(loc, err, arg, desc.release());
}

void ExceptionSink::assimilate(ExceptionSink* xs) {
    assimilate(*xs);
    delete xs;
}

void ExceptionSink::assimilate(ExceptionSink& xs) {
    priv->assimilate(*xs.priv);
}

void ExceptionSink::outOfMemory() {
    printf("OUT OF MEMORY: aborting\n");
    _Exit(1);
}

// static member function
void ExceptionSink::defaultExceptionHandler(QoreException* e) {
    ExceptionSink xsink;

    QoreString nstr;
    {
        DateTime now;
        now.setNow();
        now.format(nstr, "YYYY-MM-DD HH:mm:SS.xx Dy Z (z)");
    }

    unsigned ecnt = 0;

    while (e) {
        //printd(5, "ExceptionSink::defaultExceptionHandler() cs size=%d\n", cs->size());
        printe("unhandled QORE %s exception thrown in TID %d at %s",
            e->type == CT_USER ? "User" : "System", q_gettid(), nstr.getBuffer());

        QoreListNode* cs = e->callStack;
        bool found = false;
        if (cs->size()) {
            // find first non-rethrow element
            unsigned i = 0;

            QoreHashNode *h;
            while (true) {
                h = cs->retrieveEntry(i).get<QoreHashNode>();
                assert(h);
                if (h->getKeyValue("typecode").getAsBigInt() != CT_RETHROW) {
                    break;
                }
                i++;
                if (i == cs->size()) {
                    break;
                }
            }

            if (i < cs->size()) {
                found = true;
                QoreStringNode* func = h->getKeyValue("function").get<QoreStringNode>();
                QoreStringNode* type = h->getKeyValue("type").get<QoreStringNode>();
                QoreExceptionLocation loc;
                e->getLocation(loc);

                printe(" in %s() (", func->c_str());
                const char* fns = !loc.file.empty() ? loc.file.c_str() : nullptr;
                const char* srcs = !loc.source.empty() ? loc.source.c_str() : nullptr;
                const char* langs = !loc.lang.empty() ? loc.lang.c_str() : nullptr;
                outputExceptionLocation(fns, loc.start_line, loc.end_line, srcs, loc.offset, langs, type->c_str());
                printe(")\n");
            }
        }

        if (!found) {
            printe(" at ");

            const char* fns = !e->file.empty() ? e->file.c_str() : nullptr;
            const char* srcs = !e->source.empty() ? e->source.c_str() : nullptr;
            const char* langs = !e->lang.empty() ? e->lang.c_str() : nullptr;
            outputExceptionLocation(fns, e->start_line, e->end_line, srcs, e->offset, langs,
                e->type == CT_USER ? "user" : "builtin");
            printe("\n");
        }

        bool hdr = false;
        if (e->type == CT_BUILTIN) {
            QoreStringNode* err = e->err.get<QoreStringNode>();
            assert(!err->empty());
            QoreStringNode* desc = e->desc.get<QoreStringNode>();
            assert(!desc->empty());
            printe("%s: %s", err->c_str(), desc->c_str());
            hdr = true;
        } else {
            if (!e->err.isNothing()) {
                if (e->err.getType() == NT_STRING) {
                    QoreStringNode *err = e->err.get<QoreStringNode>();
                    printe("%s", err->c_str());
                } else {
                    QoreNodeAsStringHelper str(e->err, FMT_NORMAL, &xsink);
                    printe("EXCEPTION: %s", str->c_str());
                    hdr = true;
                }
            } else {
                printe("EXCEPTION");
            }

            if (!e->desc.isNothing()) {
                if (e->desc.getType() == NT_STRING) {
                    QoreStringNode *desc = e->desc.get<QoreStringNode>();
                    printe("%s%s", hdr ? ", desc: " : ": ", desc->c_str());
                } else {
                    QoreNodeAsStringHelper str(e->desc, FMT_NORMAL, &xsink);
                    printe(", desc: %s", str->c_str());
                    if (!hdr) {
                        hdr = true;
                    }
                }
            }
        }

        // issue #3768: show "arg" unconditionally if present
        if (!e->arg.isNothing()) {
            if (e->arg.getType() == NT_STRING) {
                QoreStringNode *arg = e->arg.get<QoreStringNode>();
                printe("%s%s", hdr ? ", arg: " : ": ", arg->c_str());
            } else {
                QoreNodeAsStringHelper str(e->arg, FMT_NORMAL, &xsink);
                printe(", arg: %s", str->c_str());
            }
        }
        printe("\n");

        if (cs->size()) {
            printe("call stack:\n");
            for (unsigned i = 0; i < cs->size(); i++) {
                int pos = cs->size() - i;
                QoreHashNode* h = cs->retrieveEntry(i).get<QoreHashNode>();
                QoreStringNode* strtype = h->getKeyValue("type").get<QoreStringNode>();
                const char* type = strtype->c_str();
                int typecode = (int)h->getKeyValue("typecode").getAsBigInt();
                if (!strcmp(type, "new-thread"))
                    printe(" %2d: *thread start*\n", pos);
                else {
                    QoreStringNode* fn = h->getKeyValue("file").get<QoreStringNode>();
                    const char* fns = fn && !fn->empty() ? fn->c_str() : nullptr;
                    int start_line = (int)h->getKeyValue("line").getAsBigInt();
                    int end_line = (int)h->getKeyValue("endline").getAsBigInt();

                    QoreStringNode* src = h->getKeyValue("source").get<QoreStringNode>();
                    const char* srcs = src && !src->empty() ? src->c_str() : nullptr;
                    int offset = (int)h->getKeyValue("offset").getAsBigInt();

                    QoreStringNode* lang = h->getKeyValue("lang").get<QoreStringNode>();
                    const char* langs = lang && !lang->empty() ? lang->c_str() : nullptr;

                    printe(" %2d: ", pos);

                    if (typecode == CT_RETHROW) {
                        printe("RETHROW at ");
                        if (fn) {
                            printe("%s:", fn->getBuffer());
                        } else {
                            printe("line");
                        }
                        printe("%d", start_line);
                        if (srcs) {
                            printe(" (source %s:%d)", srcs, offset + start_line);
                        }
                    } else {
                        QoreStringNode* fs = h->getKeyValue("function").get<QoreStringNode>();
                        printe("%s() (", fs->getBuffer());
                        outputExceptionLocation(fns, start_line, end_line, srcs, offset, langs, type);
                    }
                    printe(")\n");
                }
            }
        }
        e = e->next;
        if (e) {
            ++ecnt;
            if (ecnt == Q_MAX_EXCEPTIONS) {
                printe("*** maximum exception count reached (%d); suppressing further output\n", ecnt);
                break;
            }
            printe("chained exception:\n");
        }
    }
}

// static
void ExceptionSink::outputExceptionLocation(const char* fns, int start_line, int end_line, const char* srcs, int offset,
    const char* langs, const char* types) {
    if (fns) {
        printe("%s:", fns);
        if (!start_line) {
            printe("<init>");
        } else if (start_line == end_line) {
            printe("%d", start_line);
        } else {
            printe("%d-%d", start_line, end_line);
        }
    } else {
        if (!start_line) {
            printe("<init>");
        } else if (start_line == end_line) {
            printe("line %d", start_line);
        } else {
            printe("line %d - %d", start_line, end_line);
        }
    }

    bool openparen = false;
    if (langs) {
        printe(" (%s", langs);
        openparen = true;
    }
    if (srcs) {
        printe(" ");
        if (!openparen) {
            printe("(");
            openparen = true;
        }
        if (!start_line) {
            printe("source %s", srcs);
        } else if (start_line == end_line) {
            printe("source %s:%d", srcs, start_line + offset);
        } else {
            printe("source %s:%d-%d", srcs, start_line + offset, end_line + offset);
        }
    }
    if (types) {
        printe(" ");
        if (!openparen) {
            printe("(");
            openparen = true;
        }
        printe("%s code", types);
    }
    if (openparen) {
        printe(")");
    }
}

// static member function
void ExceptionSink::defaultWarningHandler(QoreException* e) {
    ExceptionSink xsink;

    while (e) {
        printe("warning encountered at ");
        const char* fns = !e->file.empty() ? e->file.c_str() : nullptr;
        const char* srcs = !e->source.empty() ? e->source.c_str() : nullptr;
        const char* langs = !e->lang.empty() ? e->lang.c_str() : nullptr;
        outputExceptionLocation(fns, e->start_line, e->end_line, srcs, e->offset, langs,
            e->type == CT_USER ? "user" : "builtin");
        printe("\n");

        QoreStringNode* err  = e->err.get<QoreStringNode>();
        QoreStringNode* desc = e->desc.get<QoreStringNode>();

        printe("%s: %s\n", err->c_str(), desc->c_str());

        e = e->next;
        if (e) {
            printe("next warning:\n");
        }
    }
}

QoreStackLocation::QoreStackLocation() {
}

QoreExternalProgramLocationWrapper::QoreExternalProgramLocationWrapper() : loc(new QoreProgramLocation) {
}

QoreExternalProgramLocationWrapper::QoreExternalProgramLocationWrapper(const QoreExternalProgramLocationWrapper& old)
        : file_str(old.file_str), source_str(old.source_str), lang_str(old.lang_str),
            loc(new QoreProgramLocation(file_str.c_str(), old.loc->start_line, old.loc->end_line,
            source_str.empty() ? nullptr : source_str.c_str(), old.loc->offset,
            lang_str.empty() ? nullptr : lang_str.c_str())) {
}

QoreExternalProgramLocationWrapper::QoreExternalProgramLocationWrapper(QoreExternalProgramLocationWrapper&& old)
        : file_str(old.file_str), source_str(old.source_str), lang_str(old.lang_str), loc(old.loc) {
    old.loc = nullptr;
}

QoreExternalProgramLocationWrapper::QoreExternalProgramLocationWrapper(const char* file, int start_line, int end_line,
        const char* source, int offset, const char* lang) : file_str(file ? file : ""), source_str(source ? source : ""),
        lang_str(lang ? lang : ""), loc(new QoreProgramLocation(file_str.c_str(), start_line, end_line,
        source_str.empty() ? nullptr : source_str.c_str(), offset, lang_str.empty() ? nullptr : lang_str.c_str())) {
    assert(file);
}

QoreExternalProgramLocationWrapper::~QoreExternalProgramLocationWrapper() {
    delete loc;
}

void QoreExternalProgramLocationWrapper::set(const char* file, int start_line, int end_line,
    const char* source, int offset, const char* lang) {
    // we need to save strings in case they are epheremal when this object is created
    if (!file) {
        file_str.clear();
    } else {
        file_str = file;
    }
    loc->setFile(file_str.c_str());
    if (!source) {
        source_str.clear();
    } else {
        source_str = source;
    }
    loc->setSource(source_str.c_str());
    if (!lang) {
        lang_str.clear();
    } else {
        lang_str = lang;
    }
    loc->setLanguage(lang_str.c_str());
    // the internal storage for start_line is currently 16 bits
    assert(start_line <= 0xffff);
    loc->start_line = start_line;
    // the internal storage for end_line is currently 16 bits
    assert(end_line <= 0xffff);
    loc->end_line = end_line;
    assert(offset <= 0xffff);
    loc->offset = offset;
}

int QoreExternalProgramLocationWrapper::getStartLine() const {
    return loc->start_line;
}

int QoreExternalProgramLocationWrapper::getEndLine() const {
    return loc->end_line;
}

class qore_external_stack_location_priv {
public:
    const AbstractStatement* stmt = nullptr;
    QoreProgram* pgm = nullptr;

    DLLLOCAL qore_external_stack_location_priv() = default;

    DLLLOCAL qore_external_stack_location_priv(const qore_external_stack_location_priv& old) = default;

    DLLLOCAL qore_external_stack_location_priv(qore_external_stack_location_priv&& old) = default;
};

QoreExternalStackLocation::QoreExternalStackLocation() : priv(new qore_external_stack_location_priv) {
}

QoreExternalStackLocation::QoreExternalStackLocation(const QoreExternalStackLocation& old)
    : priv(new qore_external_stack_location_priv(*old.priv)) {
}

QoreExternalStackLocation::QoreExternalStackLocation(QoreExternalStackLocation&& old) : priv(old.priv) {
    old.priv = nullptr;
}

QoreExternalStackLocation::~QoreExternalStackLocation() {
    delete priv;
}

QoreProgram* QoreExternalStackLocation::getProgram() const {
    return priv->pgm;
}

const AbstractStatement* QoreExternalStackLocation::getStatement() const {
    return priv->stmt;
}

class qore_external_runtime_stack_location_helper_priv : public QoreProgramStackLocationHelper {
public:
    DLLLOCAL qore_external_runtime_stack_location_helper_priv(QoreExternalStackLocation& stack_loc)
            : QoreProgramStackLocationHelper(&stack_loc, stack_loc.priv->stmt, stack_loc.priv->pgm) {
    }

    DLLLOCAL qore_external_runtime_stack_location_helper_priv(
        const qore_external_runtime_stack_location_helper_priv& old
    ) = default;
};

QoreExternalRuntimeStackLocationHelper::QoreExternalRuntimeStackLocationHelper()
        : priv(new qore_external_runtime_stack_location_helper_priv(*this)) {
}

QoreExternalRuntimeStackLocationHelper::QoreExternalRuntimeStackLocationHelper(
            const QoreExternalRuntimeStackLocationHelper& old
        ) : priv(new qore_external_runtime_stack_location_helper_priv(*old.priv)) {
}

QoreExternalRuntimeStackLocationHelper::QoreExternalRuntimeStackLocationHelper(
            QoreExternalRuntimeStackLocationHelper&& old
        ) : priv(old.priv) {
    old.priv = nullptr;
}

QoreExternalRuntimeStackLocationHelper::~QoreExternalRuntimeStackLocationHelper() {
    delete priv;
}
