/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreException.h

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

#ifndef _QORE_QOREEXCEPTION_H

#define _QORE_QOREEXCEPTION_H

#include <cstdarg>
#include <string>

// Forward declaration for loc_builtin fallback
DLLLOCAL extern const class QoreProgramLocation loc_builtin;

// Helper to safely get runtime location with fallback to loc_builtin
static inline const QoreProgramLocation& get_runtime_location_safe() {
    const QoreProgramLocation* loc = get_runtime_location();
    return loc ? *loc : loc_builtin;
}

struct QoreExceptionBase {
    qore_call_t type;
    QoreListNode* callStack = new QoreListNode(autoTypeInfo);
    QoreValue err, desc, arg;

    DLLLOCAL QoreExceptionBase(QoreValue n_err, QoreValue n_desc, QoreValue n_arg = QoreValue(),
        qore_call_t n_type = CT_BUILTIN);

    DLLLOCAL QoreExceptionBase(const QoreExceptionBase& old) :
        type(old.type), callStack(old.callStack->copy()),
        err(old.err.refSelf()), desc(old.desc.refSelf()),
        arg(old.arg.refSelf()) {
    }

    DLLLOCAL ~QoreExceptionBase() {
        assert(!callStack);
    }
};

struct QoreExceptionLocation : QoreProgramLineLocation {
    std::string file;
    std::string source;
    std::string lang;
    int offset = 0;

    DLLLOCAL QoreExceptionLocation() {
    }

    DLLLOCAL QoreExceptionLocation(const QoreProgramLocation& loc) : QoreProgramLineLocation(loc),
        file(loc.getFileValue()), source(loc.getSourceValue()), lang(loc.getLanguageValue()), offset(loc.offset) {
    }

    DLLLOCAL QoreExceptionLocation(const QoreExceptionLocation& old) : QoreProgramLineLocation(old),
        file(old.file), source(old.source), lang(old.lang), offset(old.offset) {
    }

    DLLLOCAL QoreExceptionLocation(QoreExceptionLocation&& old) = default;

    DLLLOCAL QoreExceptionLocation& operator=(const QoreExceptionLocation& other) {
        start_line = other.start_line;
        end_line = other.end_line;
        file = other.file;
        source = other.source;
        lang = other.lang;
        offset = other.offset;
        return *this;
    }

    DLLLOCAL void set(const QoreProgramLocation& loc) {
        start_line = loc.start_line;
        end_line = loc.end_line;
        file = loc.getFileValue();
        source = loc.getSourceValue();
        lang = loc.getLanguageValue();
        offset = loc.offset;
    }

    DLLLOCAL bool isBuiltin() const {
        return file == "<builtin>" && (start_line == end_line) && (start_line == -1);
    }
};

class QoreException : public QoreExceptionBase, public QoreExceptionLocation {
    friend class ExceptionSink;
    friend struct qore_es_private;

public:
    QoreException* next = nullptr;

    // called for generic exceptions
    DLLLOCAL QoreHashNode* makeExceptionObjectAndDelete(ExceptionSink *xsink);
    DLLLOCAL QoreHashNode* makeExceptionObject(int level = 0) const;

    // called for runtime exceptions
    DLLLOCAL QoreException(const char *n_err, QoreValue n_desc, QoreValue n_arg = QoreValue())
        : QoreExceptionBase(new QoreStringNode(n_err), n_desc, n_arg),
          QoreExceptionLocation(get_runtime_location_safe()) {
    }

    DLLLOCAL QoreException(QoreStringNode *n_err, QoreValue n_desc, QoreValue n_arg = QoreValue())
        : QoreExceptionBase(n_err, n_desc, n_arg),
          QoreExceptionLocation(get_runtime_location_safe()) {
    }

    DLLLOCAL QoreException(const QoreException& old) : QoreExceptionBase(old),
        QoreExceptionLocation(old), next(old.next ? new QoreException(*old.next) : nullptr) {
    }

    // called for user exceptions
    DLLLOCAL QoreException(const QoreListNode* n) : QoreExceptionBase(0, 0, 0, CT_USER),
        QoreExceptionLocation(get_runtime_location_safe()) {
        if (n) {
            err = n->getReferencedEntry(0);
            desc = n->getReferencedEntry(1);
            arg = n->size() > 3 ? n->copyListFrom(2) : n->getReferencedEntry(2);
        }
    }

    DLLLOCAL QoreException(const QoreProgramLocation& n_loc, const char *n_err, QoreValue n_desc,
        QoreValue n_arg = QoreValue(), qore_call_t n_type = CT_BUILTIN) :
        QoreExceptionBase(new QoreStringNode(n_err), n_desc, n_arg, n_type), QoreExceptionLocation(n_loc) {
    }

    // replaces the top exception
    /** @param new_ex must be of type "hash<ExceptionInfo>"
        @param xsink qore exceptions dereferencing current exception args are put here

        @since %Qore 1.1
    */
    DLLLOCAL QoreException* replaceTop(const QoreListNode& new_ex, ExceptionSink& xsink);

    DLLLOCAL void getLocation(QoreExceptionLocation& loc) const {
        if (isBuiltin() && callStack) {
            ConstListIterator i(callStack);
            while (i.next()) {
                QoreValue v = i.getValue();
                assert(v.getType() == NT_HASH);
                QoreValue kv = v.get<const QoreHashNode>()->getKeyValue("file");
                if (kv.getType() == NT_STRING) {
                    const QoreStringNode* str = kv.get<const QoreStringNode>();
                    if (*str != "<builtin>") {
                        loc.file = str->c_str();
                        kv = v.get<const QoreHashNode>()->getKeyValue("source");
                        if (kv.getType() == NT_STRING) {
                            loc.source = kv.get<const QoreStringNode>()->c_str();
                        }
                        kv = v.get<const QoreHashNode>()->getKeyValue("lang");
                        if (kv.getType() == NT_STRING) {
                            loc.lang = kv.get<const QoreStringNode>()->c_str();
                        }
                        kv = v.get<const QoreHashNode>()->getKeyValue("line");
                        loc.start_line = kv.getAsBigInt();
                        kv = v.get<const QoreHashNode>()->getKeyValue("endline");
                        loc.end_line = kv.getAsBigInt();
                        kv = v.get<const QoreHashNode>()->getKeyValue("offset");
                        loc.offset = kv.getAsBigInt();
                        return;
                    }
                }
            }
        }
        loc = *this;
    }

    DLLLOCAL void del(ExceptionSink *xsink);

    DLLLOCAL QoreException* rethrow();

protected:
    DLLLOCAL ~QoreException() {
        assert(!callStack);
        assert(!err.hasNode());
        assert(!desc.hasNode());
        assert(!arg.hasNode());
    }

    DLLLOCAL void addStackInfo(QoreHashNode* n);

    DLLLOCAL static const char* getType(qore_call_t type);

    DLLLOCAL static QoreHashNode* getStackHash(const QoreCallStackElement& cse);

private:
    DLLLOCAL QoreException& operator=(const QoreException&) = delete;
};

class ParseException : public QoreException {
public:
    // called for parse exceptions
    DLLLOCAL ParseException(const QoreProgramLocation& loc, const char* err, QoreStringNode* desc) : QoreException(loc, err, desc) {
    }
};

struct qore_es_private {
    QoreException* head = nullptr, * tail = nullptr;
    // exception count
    unsigned count = 0;
    bool thread_exit = false;
    bool rethrown = false;
    bool externally_managed = false;

    DLLLOCAL qore_es_private() {
    }

    DLLLOCAL ~qore_es_private() {
    }

    DLLLOCAL void clearIntern() {
        // delete all exceptions
        ExceptionSink xs;
        if (head) {
            head->del(&xs);
            head = tail = nullptr;
        }
    }

    DLLLOCAL void insert(QoreException *e);

    DLLLOCAL void appendListIntern(QoreString& str) const {
        QoreException* w = head;
        while (w) {
            QoreStringNodeValueHelper err(w->err);
            QoreStringNodeValueHelper desc(w->desc);

            str.concat(" * ");
            if (!w->file.empty())
                str.sprintf("%s:", w->file.c_str());
            if (w->start_line) {
                str.sprintf("%d", w->start_line);
                if (w->end_line && w->end_line != w->start_line)
                    str.sprintf("-%d", w->end_line);
                str.concat(": ");
            }
            str.sprintf("%s: %s", err->getBuffer(), desc->getBuffer());
            if (w != tail)
                str.concat('\n');

            w = w->next;
        }
    }

    // creates a stack trace node and adds it to all exceptions in this sink
    DLLLOCAL void addStackInfo(qore_call_t type, const char *class_name, const char *code,
        const QoreProgramLocation& loc);

    DLLLOCAL void addStackInfo(const QoreCallStackElement& cse) {
        assert(head);
        QoreHashNode* n = QoreException::getStackHash(cse);

        assert(head);
        QoreException* w = head;
        while (w) {
            w->addStackInfo(n);
            w = w->next;
            if (w)
                n->ref();
        }
    }

    DLLLOCAL void addStackInfo(const QoreCallStack& stack) {
        for (auto& i : stack)
            addStackInfo(i);
    }

    DLLLOCAL void assimilate(qore_es_private& es);

    DLLLOCAL void rethrow(QoreException* old) {
        insert(old->rethrow());
        rethrown = true;
    }

    DLLLOCAL static qore_es_private* get(ExceptionSink& xsink) {
        return xsink.priv;
    }

    DLLLOCAL static const qore_es_private* get(const ExceptionSink& xsink) {
        return xsink.priv;
    }

    DLLLOCAL static void addStackInfo(ExceptionSink& xsink, qore_call_t type, const char* class_name,
        const char* code, const QoreProgramLocation& loc) {
        xsink.priv->addStackInfo(type, class_name, code, loc);
    }

    DLLLOCAL static void appendList(ExceptionSink& xsink, QoreString& str) {
        xsink.priv->appendListIntern(str);
    }
};

class ParseExceptionSink {
protected:
    ExceptionSink xsink;

public:
    DLLLOCAL ~ParseExceptionSink();

    DLLLOCAL ExceptionSink *operator*() {
        return &xsink;
    }
};

#endif
