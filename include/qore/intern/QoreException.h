/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreException.h

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
    //! the ID of the exception for a debugger, given when a debugger's exception event first reports it, or 0; a
    //! rethrow keeps it (see QoreException::getDebugId())
    int64 debug_id = 0;
    //! the ID of the Program whose parse recorded the exception as a diagnostic (when it was raised, or by
    //! qore_program_private::addExceptionDiagnostics()), or 0; copies keep it, and an exception that another Program
    //! recorded (ex: one that a Program parsed by code run in the parse of this Program raised) is not a diagnostic of
    //! this Program (issue #6516)
    qore_program_id_t diagnostic_pgm_id = 0;
    //! the index of the diagnostic of the exception in the diagnostics of that Program, or -1; the record is kept
    //! while the Program accumulates its diagnostics (issue #6555)
    int32_t diagnostic_index = -1;
    //! the generation of the diagnostics of that Program that the index refers to (issue #6555)
    uint32_t diagnostic_gen = 0;
    //! the ID of the sandboxed Program whose code raised the exception or for which it was raised (see
    //! qore_caller_is_sandboxed()), or 0 if it is not known (see \a sandboxed)
    qore_program_id_t sandboxed_pgm_id = 0;
    //! true if code that is sandboxed or acts for sandboxed code raised the exception; copies (rethrows) keep it, and
    //! every report of the exception is then marked and has the text of the code's choosing escaped (qore#7139)
    bool sandboxed = false;

    DLLLOCAL QoreExceptionBase(QoreValue n_err, QoreValue n_desc, QoreValue n_arg = QoreValue(),
        qore_call_t n_type = CT_BUILTIN);

    DLLLOCAL QoreExceptionBase(const QoreExceptionBase& old) :
        type(old.type), callStack(old.callStack->copy()),
        err(old.err.refSelf()), desc(old.desc.refSelf()),
        arg(old.arg.refSelf()), debug_id(old.debug_id), diagnostic_pgm_id(old.diagnostic_pgm_id),
        diagnostic_index(old.diagnostic_index), diagnostic_gen(old.diagnostic_gen),
        sandboxed_pgm_id(old.sandboxed_pgm_id), sandboxed(old.sandboxed) {
    }

    //! returns true if code that is sandboxed or acts for sandboxed code raised the exception (qore#7139)
    DLLLOCAL bool isSandboxed() const {
        return sandboxed;
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

    //! Selects the constructor that copies an exception without the exceptions chained to it
    enum CopyOneTag { CopyOne };

    DLLLOCAL QoreException(const QoreException& old, CopyOneTag) : QoreExceptionBase(old),
            QoreExceptionLocation(old) {
    }

public:
    QoreException* next = nullptr;

    // called for generic exceptions
    DLLLOCAL QoreHashNode* makeExceptionObjectAndDelete(ExceptionSink *xsink);
    //! Returns a hash<ExceptionInfo> of this exception for the code running on the thread
    /** The code running on the thread receives the exception (a catch block, for example): code restricted from
        external information (see runtime_no_external_info()) is given the locations with the file names of the
        sources only (qore#7507)
    */
    DLLLOCAL QoreHashNode* makeExceptionObject(int level = 0) const;
    //! Returns a hash<ExceptionInfo> of this exception for code restricted from external information or not
    /** @param no_external_info if true, the locations (\c file, \c source, the \c callstack, those of the chained
        exceptions) are given with the file names of the sources only (qore#7507)
        @param level the level of the exception in the chain
    */
    DLLLOCAL QoreHashNode* makeExceptionObjectFor(bool no_external_info, int level = 0) const;
    //! Returns a hash<ExceptionInfo> of this exception, for code restricted from external information or not
    template <bool no_external_info>
    DLLLOCAL QoreHashNode* makeExceptionObjectImpl(int level) const;
    //! Returns a hash<ExceptionInfo> of this exception without the exceptions chained to it
    DLLLOCAL QoreHashNode* makeExceptionObjectWithoutChain() const;

    // called for runtime exceptions
    DLLLOCAL QoreException(const char *n_err, QoreValue n_desc, QoreValue n_arg = QoreValue())
        : QoreExceptionBase(new QoreStringNode(n_err), n_desc, n_arg),
          QoreExceptionLocation(get_runtime_location_safe()) {
    }

    DLLLOCAL QoreException(QoreStringNode *n_err, QoreValue n_desc, QoreValue n_arg = QoreValue())
        : QoreExceptionBase(n_err, n_desc, n_arg),
          QoreExceptionLocation(get_runtime_location_safe()) {
    }

    //! Copies an exception and the exceptions chained to it
    /** The chain is copied without recursion, as it can be long, for example after many parse errors.
    */
    DLLLOCAL QoreException(const QoreException& old) : QoreExceptionBase(old), QoreExceptionLocation(old) {
        QoreException* tail = this;
        try {
            for (const QoreException* w = old.next; w; w = w->next) {
                tail->next = new QoreException(*w, CopyOne);
                tail = tail->next;
            }
        } catch (...) {
            // releases the copied chain and this exception's own values, as no destructor releases them
            if (next) {
                next->del(nullptr);
                next = nullptr;
            }
            if (callStack) {
                callStack->deref(nullptr);
                callStack = nullptr;
            }
            err.discard(nullptr);
            desc.discard(nullptr);
            arg.discard(nullptr);
            throw;
        }
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
                    QoreStringValueHelper str(kv);
                    if (strcmp(str->c_str(), "<builtin>")) {
                        loc.file = str->c_str();
                        kv = v.get<const QoreHashNode>()->getKeyValue("source");
                        if (kv.getType() == NT_STRING) {
                            QoreStringValueHelper source(kv);
                            loc.source = source->c_str();
                        }
                        kv = v.get<const QoreHashNode>()->getKeyValue("lang");
                        if (kv.getType() == NT_STRING) {
                            QoreStringValueHelper lang(kv);
                            loc.lang = lang->c_str();
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

    //! Returns the ID of the exception for a debugger, which is given when it is first called (#5906)
    /** The ID is unique in the process; a rethrow of the exception keeps it, as the rethrown exception is a copy.  It
        is given only when a debugger's exception event reports the exception, so raising an exception costs nothing.
    */
    DLLLOCAL int64 getDebugId();

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
    //! True while an event is pending in the sink: an exception (head) or a thread exit (qore#6370)
    /** Kept up to date by every change of head and thread_exit (see syncPending()), so that a check of the sink, in
        libqore and in the code that the JIT and qcc generate (QoreIRToLLVM::emitPendingExceptionTest()), is one load
        and one compare.  It takes the byte after externally_managed that was padding, so the offsets of the other
        members and the size, which code compiled before it was added reads, are unchanged (see the static_asserts
        after the struct)
    */
    bool pending = false;

    DLLLOCAL qore_es_private() {
    }

    DLLLOCAL ~qore_es_private() {
    }

    //! sets pending from head and thread_exit; called after any change of them
    DLLLOCAL void syncPending() {
        pending = head || thread_exit;
    }

    //! returns true if an exception or a thread exit is pending
    DLLLOCAL bool isPending() const {
        assert(pending == (head || thread_exit));
        return pending;
    }

    DLLLOCAL void clearIntern() {
        // delete all exceptions
        ExceptionSink xs;
        if (head) {
            head->del(&xs);
            head = tail = nullptr;
            syncPending();
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

    //! clears the exceptions of a sink whose exception an on_error handler rethrew, which replaces it (#5904)
    /** A thread exit that is pending in the sink, which another handler of the block raised, stays pending, as
        ExceptionSink::clear() would also clear it.
    */
    DLLLOCAL static void clearForRethrow(ExceptionSink& xsink) {
        bool thread_exit = xsink.priv->thread_exit;
        xsink.clear();
        xsink.priv->thread_exit = thread_exit;
        xsink.priv->syncPending();
    }

    DLLLOCAL static void addStackInfo(ExceptionSink& xsink, qore_call_t type, const char* class_name,
        const char* code, const QoreProgramLocation& loc) {
        xsink.priv->addStackInfo(type, class_name, code, loc);
    }

    DLLLOCAL static void appendList(ExceptionSink& xsink, QoreString& str) {
        xsink.priv->appendListIntern(str);
    }
};

// the members that code compiled before pending was added reads (qcc-compiled code tests head and thread_exit) keep
// their offsets, and the struct its size (see qore_es_private::pending)
static_assert(offsetof(qore_es_private, head) == 0, "qore_es_private::head moved");
static_assert(offsetof(qore_es_private, thread_exit) == sizeof(void*) * 2 + sizeof(unsigned),
    "qore_es_private::thread_exit moved");
static_assert(offsetof(qore_es_private, pending) == offsetof(qore_es_private, externally_managed) + 1,
    "qore_es_private::pending is not in the padding after externally_managed");
static_assert(sizeof(qore_es_private) == (offsetof(qore_es_private, thread_exit) + 4 + alignof(void*) - 1)
    / alignof(void*) * alignof(void*), "the size of qore_es_private changed");

// The checks of a sink, inline in libqore (qore#6369): every "if (*xsink)" in the library would otherwise be a call
// through the PLT.  Modules, which see the declarations without "inline", call the copies that the library exports
// (ExceptionSink.cpp makes the compiler emit them), so the API and the ABI are unchanged.
inline bool ExceptionSink::isEvent() const {
    return priv->isPending();
}

inline ExceptionSink::operator bool() const {
    return priv->isPending();
}

//! Returns the value that code gives, or no value if the code raised an exception
/** Code that raises an exception gives no value to its caller: a return statement whose value the declared return
    type rejects raises the error with the value still held, which is released here, so that a caller that takes no
    value with an exception does not keep it; an immediate value (other than an enum value, which holds a reference
    to its declaration) holds nothing to release

    @param rv the value that the code gave
    @param xsink the sink of the code's exception, if any

    @return @p rv, or no value if @p rv holds a reference and @p xsink holds an exception, after releasing @p rv
*/
DLLLOCAL QoreValue qore_release_value_with_exception_intern(QoreValue rv, ExceptionSink* xsink);

DLLLOCAL QORE_INTERN_ALWAYS_INLINE QoreValue qore_release_value_with_exception(QoreValue rv, ExceptionSink* xsink) {
    // the value and the sink are tested inline (ExceptionSink::operator bool() is not), as every call of a user
    // variant passes its value here; the value is released out of line
    if (rv.needsRef()) {
        const qore_es_private* es = qore_es_private::get(*xsink);
        if (__builtin_expect(es->isPending(), 0)) {
            return qore_release_value_with_exception_intern(rv, xsink);
        }
    }
    return rv;
}

//! Returns the number of unhandled exceptions that the default exception handler has reported in the calling thread
/** A program's entry point takes the count before the program and the library are torn down, and makes its exit
    status non-zero if the teardown reported any (see qore_teardown_exit_status())
*/
DLLEXPORT uint64_t qore_get_thread_unhandled_exception_count();

//! Returns the exit status of a program whose teardown has finished
/** @param rc the exit status of the program
    @param count the number of unhandled exceptions reported in the calling thread before the teardown, as returned by
    qore_get_thread_unhandled_exception_count()

    @return \a rc if it is not 0 or if the teardown reported no unhandled exception in the calling thread, otherwise 3,
    the exit status of a program ending with an unhandled exception
*/
DLLEXPORT int qore_teardown_exit_status(int rc, uint64_t count);

class ParseExceptionSink {
protected:
    ExceptionSink xsink;

public:
    DLLLOCAL ~ParseExceptionSink();

    DLLLOCAL ExceptionSink *operator*() {
        return &xsink;
    }
};

//! Replaces the paths of the location hash \a h (\c file and \c source), which the caller owns alone, with the file
//! names of the sources, for code restricted from external information (qore#7507)
DLLLOCAL void qore_restrict_location_paths(QoreHashNode& h);

//! Returns a copy of the call stack \a stack with the file names of the sources only, for code restricted from
//! external information (qore#7507)
DLLLOCAL QoreListNode* qore_restrict_call_stack_paths(const QoreListNode& stack);

//! Replaces the paths of the call stack \a stack, whose list and hashes the caller owns alone, with the file names of
//! the sources, for code restricted from external information (qore#7507)
DLLLOCAL void qore_restrict_call_stack_paths_in_place(QoreListNode& stack);

#endif
