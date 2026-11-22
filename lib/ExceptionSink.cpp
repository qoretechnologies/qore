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
#include "qore/intern/QoreException.h"
#include "qore/intern/QoreRuntimeDiagnostic.h"
#include "qore/intern/qore_list_private.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/QoreValueExceptionOps.h"
#include "qore/intern/qore_thread_intern.h"

#include <cstdlib>

#define Q_MAX_EXCEPTIONS 10

void qore_es_private::assimilate(qore_es_private& xs) {
    if (xs.thread_exit) {
        thread_exit = xs.thread_exit;
        xs.thread_exit = false;
    }
    // The source sink is always left empty, so its `rethrown` flag must be reset along with its
    // exception list, exactly as clear() and catchException() do.  The flag describes the
    // exception held by the sink; once that exception has been handed off it describes nothing.
    // A stale flag is destructive: the on_block_exit loops (StatementBlock::execIntern() and the
    // IR/JIT/AOT runtime equivalents) reuse one handler sink for every handler they fire and
    // clear the in-flight exception whenever that sink reports `rethrown` after an on_error
    // handler.  A leftover flag from an earlier `on_error rethrow` handler therefore made a later
    // on_error handler that did NOT rethrow discard the rethrown exception, and the function
    // returned normally with no exception.  The flag is deliberately not transferred to this
    // sink: the rethrow was already applied to it by the caller clearing the replaced exception.
    xs.rethrown = false;
    if (xs.tail) {
        assert(xs.head);
        if (tail) {
            tail->next = xs.head;
        } else {
            head = xs.head;
        }
        tail = xs.tail;

        QoreException* adopted = xs.head;
        xs.head = xs.tail = nullptr;
        // An exception is counted as active on the thread that holds it in a sink that Qore manages, and is not
        // counted while a sink managed outside of Qore (see markExternallyManaged()) holds it; a module uses such a
        // sink to keep exceptions beyond the call that raised them, or to hand them to another thread.  The counts
        // therefore move with the exceptions: assimilated into a sink managed outside of Qore, they leave the count
        // of the current thread; assimilated from such a sink, they are counted on the current thread as if raised
        // in it, so that the sink can be handled or assimilated in turn like any other.
        if (!xs.externally_managed) {
            assert(xs.count);
            if (externally_managed) {
                inc_active_exceptions(-static_cast<int>(xs.count));
            } else {
                count += xs.count;
            }
            xs.count = 0;
        } else {
            assert(!xs.count);
            if (!externally_managed) {
                for (QoreException* e = adopted; e; e = e->next) {
                    ++count;
                    inc_active_exceptions(1);
                }
            }
        }
    } else {
        assert(!xs.head);
        assert(!xs.count);
    }
    syncPending();
    xs.syncPending();
}

void qore_es_private::insert(QoreException *e) {
    // append exception to the list
    if (!head) {
        head = e;
    } else {
        tail->next = e;
    }
    // A rethrow brings the whole caught chain. A subsequent exception must be
    // appended after its last node or the remaining exceptions are orphaned.
    for (;;) {
        if (!externally_managed) {
            ++count;
            inc_active_exceptions(1);
        }
        if (!e->next) {
            break;
        }
        e = e->next;
    }
    tail = e;
    pending = true;
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
    priv->pending = true;
}

bool ExceptionSink::isThreadExit() const {
    return priv->thread_exit;
}

bool ExceptionSink::isException() const {
    return priv->head;
}

// ExceptionSink::isEvent() and ExceptionSink::operator bool() are inline in libqore (see qore/intern/QoreException.h,
// qore#6369); taking their addresses here makes the compiler emit the copies that the library exports for modules
static bool (ExceptionSink::* const qore_es_is_event_export)() const __attribute__((used)) = &ExceptionSink::isEvent;
static bool (ExceptionSink::* const qore_es_operator_bool_export)() const __attribute__((used))
    = &ExceptionSink::operator bool;

QoreValue qore_release_value_with_exception_intern(QoreValue rv, ExceptionSink* xsink) {
    rv.discard(xsink);
    return QoreValue();
}

void qore_exception_sink_ops::overrideLocationImpl(ExceptionSink* b, const QoreProgramLocation& loc) {
    //printd(5, "ExceptionSink::overrideLocation() loc: %p: %s:%d\n", &loc, loc.getFileValue(), loc.start_line);
    QoreException* w = b->priv->head;
    while (w) {
        w->set(loc);
        w = w->next;
    }
}

void qore_exception_sink_ops::setLastLocationImpl(ExceptionSink* b, const QoreProgramLocation& loc) {
    if (b->priv->tail) {
        b->priv->tail->set(loc);
    }
}

QoreException* qore_exception_sink_ops::catchExceptionImpl(ExceptionSink* b) {
    QoreException* e = b->priv->head;
    b->priv->head = b->priv->tail = nullptr;
    b->priv->syncPending();
    if (b->priv->count) {
        assert(!b->priv->externally_managed);
        inc_active_exceptions(-b->priv->count);
        b->priv->count = 0;
    }
    // Reset the `rethrown` flag along with the exception itself.  Callers
    // (e.g. @ref StatementBlock::execIntern's on_error handling) use
    // `rethrown` as a signal that the in-flight exception was replaced via
    // `rethrow` inside the extracted block.  Leaving the flag set after the
    // exception has been handed off would cause a subsequent `clear()` to
    // wipe an unrelated outer exception — e.g. a `try { rethrow; } catch {}`
    // nested inside an `on_error` body should not discard the original
    // exception that triggered the block.
    b->priv->rethrown = false;
    return e;
}

QoreException* qore_exception_sink_ops::getExceptionImpl(ExceptionSink* b) {
    return b->priv->head;
}

void ExceptionSink::handleExceptions() {
    if (priv->head) {
        qore_exception_sink_ops(this).defaultExceptionHandler(priv->head);
        clear();
    } else {
        priv->thread_exit = false;
        priv->pending = false;
    }
}

void ExceptionSink::handleWarnings() {
    if (priv->head) {
        qore_exception_sink_ops(this).defaultWarningHandler(priv->head);
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
    QoreException* except = qore_exception_sink_ops(this).catchException();
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
    priv->pending = false;
    priv->rethrown = false;
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

// appends text to the description of an exception in the encoding of the description (qore#6741): text that the
// description's encoding cannot represent makes the description UTF-8, which represents both
static void qore_append_exception_description(QoreException& ex, const QoreString& text) {
    QoreStringNodeValueHelper old_desc(ex.desc);
    QoreStringNodeHolder desc(old_desc->copy());
    if (text.getEncoding() == desc->getEncoding()) {
        desc->concat(text.c_str(), text.size());
    } else {
        ExceptionSink xsink;
        desc->concat(&text, &xsink);
        if (xsink) {
            xsink.clear();
            desc = old_desc->convertEncoding(QCS_UTF8, &xsink);
            if (*desc) {
                desc->concat(&text, &xsink);
            }
            if (!*desc || xsink) {
                // text that is not valid in its encoding is left out
                xsink.clear();
                return;
            }
        }
    }
    ex.desc.discard(nullptr);
    ex.desc = desc.release();
}

int qore_exception_sink_ops::appendLastDescription(const QoreString& text) {
    QoreException* ex = getException();
    if (!ex || ex->desc.getType() != NT_STRING) {
        return -1;
    }
    qore_append_exception_description(*ex, text);
    return 0;
}

int ExceptionSink::appendLastDescription(const char* fmt, ...) {
    if (!priv->head || priv->head->desc.getType() != NT_STRING) {
        return -1;
    }

    // the text is formatted in the default encoding
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

    qore_append_exception_description(*priv->head, *new_desc);
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

    QoreStringNodeValueHelper old_desc(priv->head->desc);
    QoreStringNodeHolder desc(old_desc->copy());
    desc->prepend(new_desc->c_str(), new_desc->size());
    priv->head->desc.discard(nullptr);
    priv->head->desc = desc.release();

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

void qore_exception_sink_ops::raiseExceptionImpl(ExceptionSink* b, QoreException* e) {
    b->priv->insert(e);
}

void qore_exception_sink_ops::raiseExceptionImpl(ExceptionSink* b, const QoreListNode* n) {
    b->priv->insert(new QoreException(n));
}

void ExceptionSink::raiseException(const QoreProgramLocation& loc, const char* err, QoreValue arg, QoreValue desc) {
    QoreStringValueHelper desc_str(desc);
    printd(5, "ExceptionSink::raiseExceptionArg(%s, %s)\n", err,
        desc.getType() == NT_STRING ? desc_str->c_str() : desc.getTypeName());
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

//! the number of unhandled exceptions reported in this thread by ExceptionSink::defaultExceptionHandler()
static thread_local uint64_t thread_unhandled_exception_count = 0;

uint64_t qore_get_thread_unhandled_exception_count() {
    return thread_unhandled_exception_count;
}

int qore_teardown_exit_status(int rc, uint64_t count) {
    return (!rc && thread_unhandled_exception_count != count) ? 3 : rc;
}

//! writes the report of an unhandled exception or of a warning to standard error
/** The report is written with the stream's lock held (qore_stdio_write()), so that output that other threads write
    to the stream meanwhile, such as the traces of native compilation (QORE_JIT_TIMING), comes before or after it and
    not between the parts of its lines
*/
static void write_report(const QoreString& out) {
    if (!out.empty()) {
        qore_stdio_write(stderr, out.c_str(), out.size(), nullptr, "printe");
    }
}

//! the mark of the first exception of the chain that code that is sandboxed or acts for sandboxed code raised
static bool qore_exception_chain_mark(const QoreException* e, QoreString& mark) {
    for (; e; e = e->next) {
        if (e->isSandboxed()) {
            mark.clear();
            if (e->sandboxed_pgm_id > 0) {
                mark.sprintf("[caller Program " QLLD "] ", (int64)e->sandboxed_pgm_id);
            } else {
                mark.concat("[caller Program unknown] ");
            }
            return true;
        }
    }
    return false;
}

void qore_mark_last_exception_sandboxed(ExceptionSink& xsink, qore_program_id_t pgm_id) {
    QoreException* e = qore_exception_sink_ops(xsink).getException();
    if (!e) {
        return;
    }
    while (e->next) {
        e = e->next;
    }
    e->sandboxed = true;
    e->sandboxed_pgm_id = pgm_id;
}

void qore_log_exception_text(ExceptionSink& xsink, QoreString& mark, QoreString& err, QoreString& desc) {
    qore_exception_sandboxed_mark(xsink, mark);
    QoreStringValueHelper err_str(xsink.getExceptionErr());
    QoreStringValueHelper desc_str(xsink.getExceptionDesc());
    if (err_str->empty()) {
        err.concat('?');
    } else {
        qore_append_escaped_report_text(err, *err_str);
    }
    if (desc_str->empty()) {
        desc.concat('?');
    } else {
        qore_append_escaped_report_text(desc, *desc_str);
    }
}

bool qore_exception_sandboxed_origin(const ExceptionSink& xsink, qore_program_id_t& pgm_id) {
    for (const QoreException* e = qore_exception_sink_ops(const_cast<ExceptionSink&>(xsink)).getException(); e;
            e = e->next) {
        if (e->isSandboxed()) {
            pgm_id = e->sandboxed_pgm_id;
            return true;
        }
    }
    return false;
}

bool qore_exception_sandboxed_mark(const ExceptionSink& xsink, QoreString& mark) {
    return qore_exception_chain_mark(qore_exception_sink_ops(const_cast<ExceptionSink&>(xsink)).getException(), mark);
}

//! appends the escape of code point \a cp and returns true, or returns false if it is not escaped
//! with \a line_only, only the characters that can end or rewrite the line are escaped (not backslash or tab)
static bool qore_report_escape_code_point(QoreString& out, unsigned cp, bool line_only = false) {
    switch (cp) {
        case '\n': out.concat("\\n"); return true;
        case '\r': out.concat("\\r"); return true;
        case '\t':
            if (line_only) {
                return false;
            }
            out.concat("\\t");
            return true;
        case '\\':
            if (line_only) {
                return false;
            }
            out.concat("\\\\");
            return true;
        default:
            break;
    }
    if (cp < 0x20 || cp == 0x7f) {
        out.sprintf("\\x%02x", cp);
        return true;
    }
    // C1 controls (NEL, CSI, ...), the line and paragraph separators, and the bidirectional formatting characters
    if ((cp >= 0x80 && cp <= 0x9f) || cp == 0x2028 || cp == 0x2029 || cp == 0x061c || cp == 0x200e || cp == 0x200f
            || (cp >= 0x202a && cp <= 0x202e) || (cp >= 0x2066 && cp <= 0x2069)) {
        out.sprintf("\\u{%04x}", cp);
        return true;
    }
    return false;
}

//! appends UTF-8 text escaped; the bytes of invalid UTF-8 are escaped as \xNN
static void qore_append_escaped_utf8(QoreString& out, const char* text, size_t len, bool line_only = false,
        bool double_percent = false) {
    const unsigned char* s = reinterpret_cast<const unsigned char*>(text);
    size_t i = 0;
    while (i < len) {
        unsigned c = s[i];
        unsigned cp;
        size_t n;
        if (c < 0x80) {
            cp = c;
            n = 1;
        } else if (c >= 0xc2 && c <= 0xdf) {
            cp = c & 0x1f;
            n = 2;
        } else if (c >= 0xe0 && c <= 0xef) {
            cp = c & 0x0f;
            n = 3;
        } else if (c >= 0xf0 && c <= 0xf4) {
            cp = c & 0x07;
            n = 4;
        } else {
            out.sprintf("\\x%02x", c);
            ++i;
            continue;
        }
        bool ok = i + n <= len;
        for (size_t k = 1; ok && k < n; ++k) {
            if ((s[i + k] & 0xc0) != 0x80) {
                ok = false;
            } else {
                cp = (cp << 6) | (s[i + k] & 0x3f);
            }
        }
        // overlong forms, surrogates and code points past U+10FFFF are not valid UTF-8
        if (ok && ((n == 3 && cp < 0x800) || (n == 4 && (cp < 0x10000 || cp > 0x10ffff))
                || (cp >= 0xd800 && cp <= 0xdfff))) {
            ok = false;
        }
        if (!ok) {
            out.sprintf("\\x%02x", c);
            ++i;
            continue;
        }
        if (double_percent && cp == '%') {
            out.concat("%%");
        } else if (!qore_report_escape_code_point(out, cp, line_only)) {
            out.concat(text + i, n);
        }
        i += n;
    }
}

void qore_append_line_safe_text(QoreString& out, const char* text, size_t len, bool double_percent) {
    qore_append_escaped_utf8(out, text, len, true, double_percent);
}

void qore_append_escaped_report_text(QoreString& out, const char* text) {
    if (text) {
        qore_append_escaped_utf8(out, text, strlen(text));
    }
}

void qore_append_escaped_report_text(QoreString& out, const QoreString* text) {
    if (!text) {
        return;
    }
    const QoreEncoding* enc = text->getEncoding();
    if (enc == QCS_UTF8 || enc == QCS_USASCII) {
        qore_append_escaped_utf8(out, text->c_str(), text->size());
        return;
    }
    ExceptionSink xsink;
    TempEncodingHelper utf8(text, QCS_UTF8, &xsink);
    if (xsink || !utf8) {
        // text that cannot be converted: its bytes that are not ASCII are escaped
        xsink.clear();
        const unsigned char* s = reinterpret_cast<const unsigned char*>(text->c_str());
        for (size_t i = 0, e = text->size(); i < e; ++i) {
            if (s[i] >= 0x80 || !qore_report_escape_code_point(out, s[i])) {
                if (s[i] >= 0x80) {
                    out.sprintf("\\x%02x", (unsigned)s[i]);
                } else {
                    out.concat((char)s[i]);
                }
            }
        }
        return;
    }
    qore_append_escaped_utf8(out, utf8->c_str(), utf8->size());
}

//! writes the report of an exception chain that code that is sandboxed or acts for sandboxed code raised (qore#7139)
/** Each line starts with \a mark, and the text of the code's choosing (the error, description and argument, the names
    of functions, files, sources and languages) is escaped, so that it stays on its marked line and cannot pass for
    the host's own output
*/
static void qore_write_sandboxed_report(const QoreException* e, const QoreString& mark, const QoreString& now) {
    ExceptionSink xsink;
    QoreString out;
    auto value = [&] (QoreString& line, const QoreValue v) {
        if (v.getType() == NT_STRING) {
            // also a short string, which has no node
            QoreStringValueHelper str(v);
            qore_append_escaped_report_text(line, *str);
        } else {
            QoreNodeAsStringHelper str(v, FMT_NORMAL, &xsink);
            qore_append_escaped_report_text(line, *str);
        }
    };
    auto location = [&] (QoreString& line, const char* func, const char* file, int64 start_line, int64 end_line,
            const char* source, int64 offset) {
        if (func) {
            line.concat(" in ");
            qore_append_escaped_report_text(line, func);
            line.concat("()");
        }
        line.concat(" at ");
        if (file && *file) {
            qore_append_escaped_report_text(line, file);
            line.concat(':');
        } else {
            line.concat("line ");
        }
        line.sprintf(QLLD, start_line);
        if (end_line != start_line) {
            line.sprintf("-" QLLD, end_line);
        }
        if (source && *source) {
            line.concat(" (source ");
            qore_append_escaped_report_text(line, source);
            line.sprintf(":" QLLD ")", start_line + offset);
        }
    };
    unsigned ecnt = 0;
    for (; e; e = e->next) {
        QoreString line(mark);
        line.sprintf("unhandled QORE %s exception thrown in TID %d at %s", e->type == CT_USER ? "User" : "System",
            q_gettid(), now.c_str());
        location(line, nullptr, e->file.c_str(), e->start_line, e->end_line, e->source.c_str(), e->offset);
        out.concat(line.c_str(), line.size());
        out.concat('\n');

        line = mark;
        if (e->err.isNothing()) {
            line.concat("EXCEPTION");
        } else {
            value(line, e->err);
        }
        if (!e->desc.isNothing()) {
            line.concat(": ");
            value(line, e->desc);
        }
        if (!e->arg.isNothing()) {
            line.concat(", arg: ");
            value(line, e->arg);
        }
        out.concat(line.c_str(), line.size());
        out.concat('\n');

        const QoreListNode* cs = e->callStack;
        if (cs->size()) {
            out.concat(mark.c_str(), mark.size());
            out.concat("call stack:\n");
            for (size_t i = 0; i < cs->size(); ++i) {
                const QoreHashNode* h = cs->retrieveEntry(i).get<const QoreHashNode>();
                line = mark;
                line.sprintf(" %2d: ", (int)(cs->size() - i));
                QoreStringValueHelper type(h->getKeyValue("type"));
                if (!strcmp(type->c_str(), "new-thread")) {
                    line.concat("*thread start*");
                } else {
                    QoreStringValueHelper func(h->getKeyValue("function"));
                    QoreStringValueHelper file(h->getKeyValue("file"));
                    QoreStringValueHelper source(h->getKeyValue("source"));
                    location(line, func->c_str(), file->c_str(), h->getKeyValue("line").getAsBigInt(),
                        h->getKeyValue("endline").getAsBigInt(), source->c_str(),
                        h->getKeyValue("offset").getAsBigInt());
                }
                out.concat(line.c_str(), line.size());
                out.concat('\n');
            }
        }
        if (e->next) {
            if (++ecnt == Q_MAX_EXCEPTIONS) {
                out.concat(mark.c_str(), mark.size());
                out.sprintf("*** maximum exception count reached (%d); suppressing further output\n", ecnt);
                break;
            }
            out.concat(mark.c_str(), mark.size());
            out.concat("chained exception:\n");
        }
    }
    write_report(out);
}

// static member function
void qore_exception_sink_ops::defaultExceptionHandler(QoreException* e) {
    ExceptionSink xsink;
    // the report is written whole (write_report()), so that the output of other threads is not mixed into it
    QoreString out;

    if (e) {
        ++thread_unhandled_exception_count;
    }

    QoreString nstr;
    {
        DateTime now;
        now.setNow();
        now.format(nstr, "YYYY-MM-DD HH:mm:SS.xx Dy Z (z)");
    }

    // the exceptions of code that is sandboxed or acts for sandboxed code are reported marked (qore#7139)
    {
        QoreString mark;
        if (qore_exception_chain_mark(e, mark)) {
            qore_write_sandboxed_report(e, mark, nstr);
            return;
        }
    }

    unsigned ecnt = 0;

    while (e) {
        //printd(5, "ExceptionSink::defaultExceptionHandler() cs size=%d\n", cs->size());
        out.sprintf("unhandled QORE %s exception thrown in TID %d at %s",
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
                QoreStringValueHelper func(h->getKeyValue("function"));
                QoreStringValueHelper type(h->getKeyValue("type"));
                QoreExceptionLocation loc;
                e->getLocation(loc);

                out.sprintf(" in %s() (", func->c_str());
                const char* fns = !loc.file.empty() ? loc.file.c_str() : nullptr;
                const char* srcs = !loc.source.empty() ? loc.source.c_str() : nullptr;
                const char* langs = !loc.lang.empty() ? loc.lang.c_str() : nullptr;
                outputExceptionLocation(out, fns, loc.start_line, loc.end_line, srcs, loc.offset, langs, type->c_str());
                out.sprintf(")\n");
            }
        }

        if (!found) {
            out.sprintf(" at ");

            const char* fns = !e->file.empty() ? e->file.c_str() : nullptr;
            const char* srcs = !e->source.empty() ? e->source.c_str() : nullptr;
            const char* langs = !e->lang.empty() ? e->lang.c_str() : nullptr;
            outputExceptionLocation(out, fns, e->start_line, e->end_line, srcs, e->offset, langs,
                e->type == CT_USER ? "user" : "builtin");
            out.sprintf("\n");
        }

        bool hdr = false;
        if (e->type == CT_BUILTIN) {
            QoreStringValueHelper err(e->err);
            assert(!err->empty());
            QoreStringValueHelper desc(e->desc);
            assert(!desc->empty());
            out.sprintf("%s: %s", err->c_str(), desc->c_str());
            hdr = true;
        } else {
            if (!e->err.isNothing()) {
                if (e->err.getType() == NT_STRING) {
                    QoreStringValueHelper err(e->err);
                    out.sprintf("%s", err->c_str());
                } else {
                    QoreNodeAsStringHelper str(e->err, FMT_NORMAL, &xsink);
                    out.sprintf("EXCEPTION: %s", str->c_str());
                    hdr = true;
                }
            } else {
                out.sprintf("EXCEPTION");
            }

            if (!e->desc.isNothing()) {
                if (e->desc.getType() == NT_STRING) {
                    QoreStringValueHelper desc(e->desc);
                    out.sprintf("%s%s", hdr ? ", desc: " : ": ", desc->c_str());
                } else {
                    QoreNodeAsStringHelper str(e->desc, FMT_NORMAL, &xsink);
                    out.sprintf(", desc: %s", str->c_str());
                    if (!hdr) {
                        hdr = true;
                    }
                }
            }
        }

        // issue #3768: show "arg" unconditionally if present
        if (!e->arg.isNothing()) {
            const QoreHashNode* diag = e->arg.getType() == NT_HASH ? e->arg.get<const QoreHashNode>() : nullptr;
            if (diag && diag->getHashDecl() == hashdeclRuntimeDiagnosticInfo) {
                // a runtime diagnostic is shown as a hint with its suggestions
                QoreStringValueHelper id(diag->getKeyValue("id"));
                QoreStringValueHelper hint(diag->getKeyValue("hint"));
                out.sprintf("\nhint (%s): %s", id->c_str(), hint->c_str());
                const QoreListNode* sl = diag->getKeyValue("suggestions").getType() == NT_LIST
                    ? diag->getKeyValue("suggestions").get<const QoreListNode>()
                    : nullptr;
                if (sl) {
                    ConstListIterator li(sl);
                    while (li.next()) {
                        QoreStringValueHelper suggestion(li.getValue());
                        out.sprintf("\n  - %s", suggestion->c_str());
                    }
                }
            } else if (e->arg.getType() == NT_STRING) {
                QoreStringValueHelper arg(e->arg);
                out.sprintf("%s%s", hdr ? ", arg: " : ": ", arg->c_str());
            } else {
                QoreNodeAsStringHelper str(e->arg, FMT_NORMAL, &xsink);
                out.sprintf(", arg: %s", str->c_str());
            }
        }
        out.sprintf("\n");

        if (cs->size()) {
            out.sprintf("call stack:\n");
            for (unsigned i = 0; i < cs->size(); i++) {
                int pos = cs->size() - i;
                QoreHashNode* h = cs->retrieveEntry(i).get<QoreHashNode>();
                QoreStringValueHelper strtype(h->getKeyValue("type"));
                const char* type = strtype->c_str();
                int typecode = (int)h->getKeyValue("typecode").getAsBigInt();
                if (!strcmp(type, "new-thread")) {
                    out.sprintf(" %2d: *thread start*\n", pos);
                } else {
                    QoreStringValueHelper fn(h->getKeyValue("file"));
                    const char* fns = !fn->empty() ? fn->c_str() : nullptr;
                    int start_line = (int)h->getKeyValue("line").getAsBigInt();
                    int end_line = (int)h->getKeyValue("endline").getAsBigInt();

                    QoreStringValueHelper src(h->getKeyValue("source"));
                    const char* srcs = !src->empty() ? src->c_str() : nullptr;
                    int offset = (int)h->getKeyValue("offset").getAsBigInt();

                    QoreStringValueHelper lang(h->getKeyValue("lang"));
                    const char* langs = !lang->empty() ? lang->c_str() : nullptr;

                    out.sprintf(" %2d: ", pos);

                    if (typecode == CT_RETHROW) {
                        out.sprintf("RETHROW at ");
                        if (!fn->empty()) {
                            out.sprintf("%s:", fn->getBuffer());
                        } else {
                            out.sprintf("line");
                        }
                        out.sprintf("%d", start_line);
                        if (srcs) {
                            out.sprintf(" (source %s:%d)", srcs, offset + start_line);
                        }
                    } else {
                        QoreStringValueHelper fs(h->getKeyValue("function"));
                        out.sprintf("%s() (", fs->getBuffer());
                        outputExceptionLocation(out, fns, start_line, end_line, srcs, offset, langs, type);
                    }
                    out.sprintf(")\n");
                }
            }
        }
        e = e->next;
        if (e) {
            ++ecnt;
            if (ecnt == Q_MAX_EXCEPTIONS) {
                out.sprintf("*** maximum exception count reached (%d); suppressing further output\n", ecnt);
                break;
            }
            out.sprintf("chained exception:\n");
        }
    }
    write_report(out);
}

// static
void qore_exception_sink_ops::outputExceptionLocation(QoreString& out, const char* fns, int start_line, int end_line,
        const char* srcs, int offset, const char* langs, const char* types) {
    if (fns) {
        out.sprintf("%s:", fns);
        if (!start_line) {
            out.sprintf("<init>");
        } else if (start_line == end_line) {
            out.sprintf("%d", start_line);
        } else {
            out.sprintf("%d-%d", start_line, end_line);
        }
    } else {
        if (!start_line) {
            out.sprintf("<init>");
        } else if (start_line == end_line) {
            out.sprintf("line %d", start_line);
        } else {
            out.sprintf("line %d - %d", start_line, end_line);
        }
    }

    bool openparen = false;
    if (langs) {
        out.sprintf(" (%s", langs);
        openparen = true;
    }
    if (srcs) {
        out.sprintf(" ");
        if (!openparen) {
            out.sprintf("(");
            openparen = true;
        }
        if (!start_line) {
            out.sprintf("source %s", srcs);
        } else if (start_line == end_line) {
            out.sprintf("source %s:%d", srcs, start_line + offset);
        } else {
            out.sprintf("source %s:%d-%d", srcs, start_line + offset, end_line + offset);
        }
    }
    if (types) {
        out.sprintf(" ");
        if (!openparen) {
            out.sprintf("(");
            openparen = true;
        }
        out.sprintf("%s code", types);
    }
    if (openparen) {
        out.sprintf(")");
    }
}

// static member function
void qore_exception_sink_ops::defaultWarningHandler(QoreException* e) {
    ExceptionSink xsink;
    // the report is written whole (write_report()), as that of an exception
    QoreString out;

    // the warnings of the parse of code that is sandboxed or acts for sandboxed code are reported marked, with the text
    // of its choosing escaped (qore#7139)
    {
        QoreString mark;
        if (qore_exception_chain_mark(e, mark)) {
            for (; e; e = e->next) {
                QoreString line(mark);
                line.concat("warning encountered at ");
                if (!e->file.empty()) {
                    qore_append_escaped_report_text(line, e->file.c_str());
                    line.concat(':');
                } else {
                    line.concat("line ");
                }
                line.sprintf("%d", e->start_line);
                if (!e->source.empty()) {
                    line.concat(" (source ");
                    qore_append_escaped_report_text(line, e->source.c_str());
                    line.sprintf(":%d)", e->start_line + e->offset);
                }
                line.concat('\n');
                line.concat(mark.c_str(), mark.size());
                QoreStringValueHelper err(e->err);
                QoreStringValueHelper desc(e->desc);
                qore_append_escaped_report_text(line, *err);
                line.concat(": ");
                qore_append_escaped_report_text(line, *desc);
                line.concat('\n');
                out.concat(line.c_str(), line.size());
            }
            write_report(out);
            return;
        }
    }

    while (e) {
        out.sprintf("warning encountered at ");
        const char* fns = !e->file.empty() ? e->file.c_str() : nullptr;
        const char* srcs = !e->source.empty() ? e->source.c_str() : nullptr;
        const char* langs = !e->lang.empty() ? e->lang.c_str() : nullptr;
        outputExceptionLocation(out, fns, e->start_line, e->end_line, srcs, e->offset, langs,
            e->type == CT_USER ? "user" : "builtin");
        out.sprintf("\n");

        QoreStringValueHelper err(e->err);
        QoreStringValueHelper desc(e->desc);

        out.sprintf("%s: %s\n", err->c_str(), desc->c_str());

        e = e->next;
        if (e) {
            out.sprintf("next warning:\n");
        }
    }
    write_report(out);
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
    // -1 is the "unknown location" sentinel
    assert(start_line >= -1);
    loc->start_line = start_line;
    assert(end_line >= -1);
    loc->end_line = end_line;
    assert(offset >= -1);
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

QoreHashNode* qore_make_runtime_diagnostic(const char* id, const char* hint, const char* doc,
        const std::vector<std::pair<std::string, std::string>>& facts, const std::vector<std::string>& suggestions) {
    assert(hashdeclRuntimeDiagnosticInfo);
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(hashdeclRuntimeDiagnosticInfo, nullptr), nullptr);
    qore_hash_private* ph = qore_hash_private::get(**h);
    ph->setKeyValueIntern("id", new QoreStringNode(id));
    ph->setKeyValueIntern("hint", new QoreStringNode(hint));
    ReferenceHolder<QoreListNode> sl(new QoreListNode(stringTypeInfo), nullptr);
    for (const auto& suggestion : suggestions) {
        sl->push(new QoreStringNode(suggestion), nullptr);
    }
    ph->setKeyValueIntern("suggestions", sl.release());
    ReferenceHolder<QoreHashNode> fh(new QoreHashNode(stringTypeInfo), nullptr);
    qore_hash_private* fph = qore_hash_private::get(**fh);
    for (const auto& fact : facts) {
        fph->setKeyValueIntern(fact.first.c_str(), new QoreStringNode(fact.second));
    }
    ph->setKeyValueIntern("facts", fh.release());
    ph->setKeyValueIntern("doc", doc ? QoreValue(new QoreStringNode(doc)) : QoreValue());
    return h.release();
}

void qore_raise_deleted_object(ExceptionSink* xsink, const char* fmt, ...) {
    if (!xsink) {
        return;
    }
    SimpleRefHolder<QoreStringNode> desc(new QoreStringNode);
    while (true) {
        va_list args;
        va_start(args, fmt);
        int rc = desc->vsprintf(fmt, args);
        va_end(args);
        if (!rc) {
            break;
        }
    }
    ReferenceHolder<QoreHashNode> diag(qore_make_runtime_diagnostic(QORE_RTD_DELETED_OBJECT,
        "the object was deleted before this call or member access: its last reference went away, or it was "
        "deleted explicitly; a method call reference (\\method() or \\obj.method()) does not keep its object "
        "alive, so keep a reference to the object for as long as the call reference can be called",
        "pitfalls_background_callref", {},
        {"keep a reference to the object, as in a member or a local variable, while the call can happen",
         "call the method directly, as in \"background obj.method()\", which keeps the object alive"}), nullptr);
    xsink->raiseExceptionArg("OBJECT-ALREADY-DELETED", diag.release(), desc.release());
}
