/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ExceptionSink.h

    Qore Programming Language ExceptionSink class definition

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

#ifndef _QORE_EXCEPTIONSINK_H

#define _QORE_EXCEPTIONSINK_H

#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

// forward references
class QoreException;
class ExceptionSink;
class QoreXSinkException;
struct QoreProgramLocation;
struct QoreCallStack;
class AbstractStatement;

//! container for holding Qore-language exception information and also for registering a "thread_exit" call
class ExceptionSink {
    friend struct qore_es_private;
    friend QoreXSinkException;

public:
    //! creates an empty ExceptionSink object
    DLLEXPORT ExceptionSink();

    //! calls ExceptionSink::defaultExceptionHandler() on all exceptions still present in the object and then deletes the exception list
    DLLEXPORT ~ExceptionSink();

    //! calls ExceptionSink::defaultExceptionHandler() on all exceptions still present in the object and then deletes the exception list
    DLLEXPORT void handleExceptions();

    //! calls ExceptionSink::defaultWarningHandler() on all exceptions still present in the object and then deletes the exception list
    DLLEXPORT void handleWarnings();

    //! returns true if at least one exception is present or thread_exit has been triggered
    DLLEXPORT bool isEvent() const;

    //! returns true if thread_exit has been triggered
    DLLEXPORT bool isThreadExit() const;

    //! returns true if at least one exception is present
    DLLEXPORT bool isException() const;

    //! returns true if at least one exception is present or thread_exit has been triggered
    /** Intended as a alternative to isEvent()
        @code
        ExceptionSink xsink;
        if (xsink) { .. }
        @endcode
    */
    DLLEXPORT operator bool () const;

    //! appends a Qore-language exception to the list
    /** The AbstractQoreNode pointer returned is always 0; used to simplify error handling code.
        @param err the exception code string
        @param fmt the format string for the description for the exception
        @return always returns nullptr
    */
    DLLEXPORT AbstractQoreNode* raiseException(const char *err, const char *fmt, ...);

    //! appends a Qore-language exception to the list and appends the result of strerror(errno) to the description
    /** The AbstractQoreNode pointer returned is always 0; used to simplify error handling code.
        @param err the exception code string
        @param en the error number (normally "errno")
        @param fmt the format string for the description for the exception
        @return always returns nullptr
    */
    DLLEXPORT AbstractQoreNode* raiseErrnoException(const char *err, int en, const char *fmt, ...);

    //! appends a Qore-language exception to the list and appends the result of strerror(errno) to the description
    /** The AbstractQoreNode pointer returned is always 0; used to simplify error handling code.
        @param err the exception code string
        @param en the error number (normally "errno")
        @param desc the error description (the ExceptionSink object takes over ownership of the reference count)
        @return always returns nullptr
    */
    DLLEXPORT AbstractQoreNode* raiseErrnoException(const char *err, int en, QoreStringNode* desc);

    //! appends a Qore-language exception to the list, and sets the 'arg' member (this object takes over the reference count of 'arg')
    /** The AbstractQoreNode pointer returned is always 0; used to simplify error handling code.
        @param err the exception code string
        @param arg the 'arg' member of the Qore-language exception object; will be dereferenced when the QoreException object is destroyed
        @param fmt the format string for the description for the exception
        @return always returns nullptr
    */
    DLLEXPORT AbstractQoreNode* raiseExceptionArg(const char* err, QoreValue arg, const char* fmt, ...);

    //! appends a Qore-language exception to the list, and sets the 'arg' member (this object takes over the reference counts of 'arg' and 'desc')
    /** The AbstractQoreNode pointer returned is always 0; used to simplify error handling code.
        @param err the exception code string
        @param arg the 'arg' member of the Qore-language exception object; will be dereferenced when the QoreException object is destroyed
        @param desc the description string for the exception; the ExceptionSink object takes ownership of the reference count
        @return always returns nullptr
    */
    DLLEXPORT AbstractQoreNode* raiseExceptionArg(const char* err, QoreValue arg, QoreStringNode* desc);

    //! appends a Qore-language exception to the list, and sets the 'arg' member (this object takes over the reference counts of 'arg' and 'desc')
    /** The AbstractQoreNode pointer returned is always 0; used to simplify error handling code.
        @param err the exception code string
        @param arg the 'arg' member of the Qore-language exception object; will be dereferenced when the QoreException object is destroyed
        @param desc the description string for the exception; the ExceptionSink object takes ownership of the reference count
        @param stack a call stack to prepend to the Qore call stack

        @return always returns nullptr

        @since %Qore 0.8.13
    */
    DLLEXPORT AbstractQoreNode* raiseExceptionArg(const char* err, QoreValue arg, QoreStringNode* desc, const QoreCallStack& stack);

    //! appends a Qore-language exception to the list, and sets the 'arg' member (this object takes over the reference counts of 'arg' and 'desc')
    /** The AbstractQoreNode pointer returned is always 0; used to simplify error handling code.
        @param loc the source location for the exception
        @param err the exception code string
        @param arg the 'arg' member of the Qore-language exception object; will be dereferenced when the QoreException object is destroyed
        @param desc the description string for the exception; the ExceptionSink object takes ownership of the reference count
        @param stack a call stack to prepend to the Qore call stack

        @return always returns nullptr

        @since %Qore 0.9
    */
    DLLEXPORT AbstractQoreNode* raiseExceptionArg(const QoreProgramLocation& loc, const char* err, QoreValue arg, QoreStringNode* desc, const QoreCallStack& stack);

    //! appends a Qore-language exception to the list, and sets the 'arg' member (this object takes over the reference counts of 'arg' and 'desc')
    /** The AbstractQoreNode pointer returned is always 0; used to simplify error handling code.
        @param loc the source location for the exception
        @param err the exception code string
        @param arg the 'arg' member of the Qore-language exception object; will be dereferenced when the QoreException object is destroyed
        @param desc the description string for the exception; the ExceptionSink object takes ownership of the reference count

        @return always returns nullptr

        @since %Qore 0.9
    */
    DLLEXPORT AbstractQoreNode* raiseExceptionArg(const QoreProgramLocation& loc, const char* err, QoreValue arg, QoreStringNode* desc);

    //! appends a Qore-language exception to the list; takes owenership of the "desc" argument reference
    /** The AbstractQoreNode pointer returned is always 0; used to simplify error handling code.
        @param err the exception code string
        @param desc the description string for the exception; the ExceptionSink object takes ownership of the reference count
        @return always returns nullptr
    */
    DLLEXPORT AbstractQoreNode* raiseException(const char *err, QoreStringNode* desc);

    //! appends a Qore-language exception to the list; takes owenership of the "desc" argument reference
    /** The AbstractQoreNode pointer returned is always 0; used to simplify error handling code.
        @param err the exception code string for the exception; the ExceptionSink object takes ownership of the reference count
        @param desc the description string for the exception; the ExceptionSink object takes ownership of the reference count
        @return always returns nullptr
    */
    DLLEXPORT AbstractQoreNode* raiseException(QoreStringNode *err, QoreStringNode* desc);

    //! appends a Qore-language exception to the list; takes owenership of the "desc" argument reference
    /** The AbstractQoreNode pointer returned is always 0; used to simplify error handling code.

        @param err the exception code string for the exception; the ExceptionSink object takes ownership of the reference count
        @param desc the description string for the exception; the ExceptionSink object takes ownership of the reference count
        @param arg the exception argument

        @return always returns nullptr

        @since %Qore 0.9.5
    */
    DLLEXPORT AbstractQoreNode* raiseException(QoreStringNode *err, QoreStringNode* desc, QoreValue arg);

    //! appends a Qore-language exception to the list; takes owenership of the "desc" argument reference
    /**
        @param loc the source location for the exception
        @param err the exception code string for the exception
        @param arg the 'arg' member of the Qore-language exception object; will be dereferenced when the QoreException object is destroyed
        @param desc the description string for the exception; the ExceptionSink object takes ownership of the reference count

        @since %Qore 0.9.0
    */
    DLLEXPORT void raiseException(const QoreProgramLocation& loc, const char* err, QoreValue arg, QoreValue desc);

    //! appends a Qore-language exception to the list; takes owenership of the "desc" argument reference
    /**
        @param loc the source location for the exception
        @param err the exception code string for the exception
        @param arg the 'arg' member of the Qore-language exception object; will be dereferenced when the QoreException object is destroyed
        @param fmt the format string for the description for the exception

        @since %Qore 0.9.0
    */
    DLLEXPORT void raiseException(const QoreProgramLocation& loc, const char* err, QoreValue arg, const char* fmt, ...);

    //! sets the "thread_exit" flag; will cause the current thread to terminate
    DLLEXPORT void raiseThreadExit();

    //! assimilates all entries of the "xs" argument by appending them to the internal list and deletes the "xs" argument
    DLLEXPORT void assimilate(ExceptionSink *xs);

    //! assimilates all entries of the "xs" argument by appending them to the internal list and clears the "xs" argument
    DLLEXPORT void assimilate(ExceptionSink &xs);

    //! intended to be used to handle out of memory errors
    /** @note Currently causes the program to exit immedaitely
     */
    DLLEXPORT void outOfMemory();

    //! deletes the exception list immediately
    DLLEXPORT void clear();

    //! returns the error of the top exception
    DLLEXPORT const QoreValue getExceptionErr();

    //! returns the description of the top exception
    DLLEXPORT const QoreValue getExceptionDesc();

    //! returns the argument of the top exception
    DLLEXPORT const QoreValue getExceptionArg();

    //! appends a formatted string to the top exception description if the desc value is a string
    /** @return -1 for error, not added, or 0 = OK

        @since %Qore 0.9
    */
    DLLEXPORT int appendLastDescription(const char* fmt, ...);

    // Renames the "err" key of the top exception and prepends a string description to any desc value
    /** @return -1 for error, not added, or 0 = OK

        @since %Qore 1.13
    */
    DLLEXPORT int renamePrependLastException(const char* err, const char* desc_fmt, ...);

    //! Call if the ExceptionSink object will be managed outside of Qore
    DLLEXPORT void markExternallyManaged();

    //! Consumes the exception and returns an ExceptionInfo hash for it
    /** The caller owns the reference returned
    */
    DLLEXPORT QoreHashNode* getExceptionInfo();

    DLLLOCAL void raiseException(QoreException* e);
    DLLLOCAL void raiseException(const QoreListNode* n);
    DLLLOCAL QoreException* catchException();
    DLLLOCAL QoreException* getException();
    DLLLOCAL void overrideLocation(const QoreProgramLocation& loc);
    DLLLOCAL void setLastLocation(const QoreProgramLocation& loc);
    DLLLOCAL void rethrow(QoreException* old);

    DLLLOCAL static void defaultExceptionHandler(QoreException* e);
    DLLLOCAL static void defaultWarningHandler(QoreException* e);

    DLLLOCAL static void outputExceptionLocation(const char* fns, int start_line, int end_line, const char* srcs,
        int offset, const char* langs, const char* types);

private:
    //! private implementation of the class
    struct qore_es_private* priv;

    ExceptionSink(const ExceptionSink&) = delete;
    ExceptionSink& operator=(const ExceptionSink&) = delete;
};

//! call stack element type
enum qore_call_t : signed char {
    CT_UNUSED     = -1,
    CT_USER       =  0,
    CT_BUILTIN    =  1,
    CT_NEWTHREAD  =  2,
    CT_RETHROW    =  3
};

//! Qore source location; strings must be in the default encoding for the Qore process
/** @since %Qore 0.8.13
 */
struct QoreSourceLocation {
    std::string label;     //!< the code label name (source file if source not present)
    int start_line,        //!< the start line
        end_line;          //!< the end line
    std::string source;    //!< optional additional source file
    unsigned offset = 0;   //!< offset in source file (only used if source is not empty)
    std::string code;      //!< the function or method call name; method calls in format class::name
    std::string lang;      //!< the source language

    DLLLOCAL QoreSourceLocation(const char* label, int start, int end, const char* code, const char* lang = "Qore") :
        label(label), start_line(start), end_line(end), code(code), lang(lang) {
    }

    DLLLOCAL QoreSourceLocation(const char* label, int start, int end, const char* source, unsigned offset,
        const char* code, const char* lang = "Qore") :
        label(label), start_line(start), end_line(end), source(source), offset(offset), code(code), lang(lang) {
    }
};

//! call stack element; strings must be in the default encoding for the Qore process
/** @since %Qore 0.8.13
 */
struct QoreCallStackElement : public QoreSourceLocation {
    qore_call_t type;        //!< the call stack element type

    DLLLOCAL QoreCallStackElement(qore_call_t type, const char* label, int start, int end, const char* code,
        const char* lang = "Qore") :
        QoreSourceLocation(label, start, end, code, lang), type(type) {
    }

    DLLLOCAL QoreCallStackElement(qore_call_t type, const char* label, int start, int end, const char* source,
        unsigned offset, const char* code, const char* lang = "Qore") :
        QoreSourceLocation(label, start, end, source, offset, code, lang), type(type) {
    }
};

typedef std::vector<QoreCallStackElement> callstack_vec_t;

//! Qore call stack
/** @since %Qore 0.8.13
 */
struct QoreCallStack : public callstack_vec_t {
    //! add an element to the end of the stack trace
    DLLEXPORT void add(qore_call_t type, const char* label, int start, int end, const char* code,
        const char* lang = "Qore");

    //! add an element to the end of the stack trace
    DLLEXPORT void add(qore_call_t type, const char* label, int start, int end, const char* source,
        unsigned offset, const char* code, const char* lang = "Qore");
};

static inline void alreadyDeleted(ExceptionSink *xsink, const char *cmeth) {
    xsink->raiseException("OBJECT-ALREADY-DELETED", "the method %s() cannot be executed because the object has already been deleted", cmeth);
}

static inline void makeAccessDeletedObjectException(ExceptionSink *xsink, const char *mem, const char *cname) {
    xsink->raiseException("OBJECT-ALREADY-DELETED", "attempt to access member '%s' of an already-deleted object of class '%s'", mem, cname);
}

static inline void makeAccessDeletedObjectException(ExceptionSink *xsink, const char *cname) {
    xsink->raiseException("OBJECT-ALREADY-DELETED", "attempt to access an already-deleted object of class '%s'", cname);
}

static inline void makeObjectIncompatibleException(ExceptionSink* xsink, const char* cname) {
    xsink->raiseException("OBJECT-INCOMPATIBLE",
        "object of class '%s' is not compatible with the expected builtin class for this operation",
        cname);
}

static inline void makeObjectIncompatibleException(ExceptionSink* xsink, const char* cname,
        const char* expected_cname) {
    xsink->raiseException("OBJECT-INCOMPATIBLE",
        "object of class '%s' is not compatible with the expected builtin class '%s'",
        cname, expected_cname);
}

//! returns a custom Qore program location for external modules to generate runtime exceptions with the source location
class QoreExternalProgramLocationWrapper {
public:
    //! empty constructor; use set() to set the location
    DLLEXPORT QoreExternalProgramLocationWrapper();

    //! copy ctor
    DLLEXPORT QoreExternalProgramLocationWrapper(const QoreExternalProgramLocationWrapper&);

    //! move ctor
    DLLEXPORT QoreExternalProgramLocationWrapper(QoreExternalProgramLocationWrapper&&);

    //! constructor setting the source location
    DLLEXPORT QoreExternalProgramLocationWrapper(const char* file, int start_line, int end_line,
        const char* source = nullptr, int offset = 0, const char* lang = nullptr);

    //! destructor; frees memory
    DLLEXPORT ~QoreExternalProgramLocationWrapper();

    //! sets the program source location
    DLLEXPORT void set(const char* file, int start_line, int end_line,
        const char* source = nullptr, int offset = 0, const char* lang = nullptr);

    //! returns the source location
    DLLLOCAL const QoreProgramLocation& get() const {
        return *loc;
    }

    //! returns the file name
    DLLLOCAL const std::string& getFile() const {
        return file_str;
    }

    //! returns the source
    DLLLOCAL const std::string& getSource() const {
        return source_str;
    }

    //! returns the language
    DLLLOCAL const std::string& getLanguage() const {
        return lang_str;
    }

    //! returns the start line
    DLLEXPORT int getStartLine() const;

    //! returns the start line
    DLLEXPORT int getEndLine() const;

private:
    // save strings for exceptions in case they are epheremal when this object is created
    std::string file_str;
    std::string source_str;
    std::string lang_str;

    // actual exception location
    QoreProgramLocation* loc;
};

//! Stack location element abstract class
/** @since %Qore 0.9
*/
class QoreStackLocation {
public:
    //! constructor
    DLLLOCAL QoreStackLocation();

    //! copy ctor
    DLLLOCAL QoreStackLocation(const QoreStackLocation&) = default;

    //! move ctor
    DLLLOCAL QoreStackLocation(QoreStackLocation&&) = default;

    //! virtual destructor
    DLLLOCAL virtual ~QoreStackLocation() = default;

    //! default assignment operator
    DLLLOCAL QoreStackLocation& operator=(const QoreStackLocation&) = default;

    //! default move assignment operator
    DLLLOCAL QoreStackLocation& operator=(QoreStackLocation&&) = default;

    //! called when pushed on the stack to set the next location
    /** @param next a pointer to the existing next stack element that must exist and must stay on the stack while
        this object exists or nullptr in case this is the first element on the stack

        @note it is an error and will cause a segfault if the next object is destroyed before this object is
        destroyed.  Under no circumstances, and under the very real threat of at least 700 lashings with a wet noodle,
        should the next object be deleted before this object.
    */
    DLLLOCAL void setNext(const QoreStackLocation* next) {
        stack_next = next;
    }

    //! returns the next location in the stack or nullptr if there is none
    DLLLOCAL virtual const QoreStackLocation* getNext() const {
        return stack_next;
    }

    //! returns the QoreProgram container
    DLLLOCAL virtual QoreProgram* getProgram() const = 0;

    //! returns the statement for the call for internal Qore code
    DLLLOCAL virtual const AbstractStatement* getStatement() const = 0;

    //! returns the name of the function or method call
    DLLLOCAL virtual const std::string& getCallName() const = 0;

    //! returns the call type
    DLLLOCAL virtual qore_call_t getCallType() const = 0;

    //! returns the source location of the element
    DLLLOCAL virtual const QoreProgramLocation& getLocation() const = 0;

protected:
    const QoreStackLocation* stack_next = nullptr;
};

//! Stack location element abstract class for external binary modules
/** @since %Qore 0.9
*/
class QoreExternalStackLocation : public QoreStackLocation {
    friend class qore_external_runtime_stack_location_helper_priv;
public:
    //! create the object
    DLLEXPORT QoreExternalStackLocation();

    //! copy ctor
    DLLEXPORT QoreExternalStackLocation(const QoreExternalStackLocation&);

    //! move ctor
    DLLEXPORT QoreExternalStackLocation(QoreExternalStackLocation&&);

    //! destroys the object
    DLLEXPORT virtual ~QoreExternalStackLocation();

    //! no assignment operator
    DLLLOCAL QoreExternalStackLocation& operator=(const QoreExternalStackLocation&) = delete;

    //! no move assignment operator
    DLLLOCAL QoreExternalStackLocation& operator=(QoreExternalStackLocation&&) = delete;

    //! returns the QoreProgram container
    DLLEXPORT virtual QoreProgram* getProgram() const;

    //! returns the statement for the call for internal Qore code
    DLLEXPORT virtual const AbstractStatement* getStatement() const;

private:
    class qore_external_stack_location_priv* priv;
};

//! Sets the stack location for external modules providing language support
/** @since %Qore 0.9
*/
class QoreExternalRuntimeStackLocationHelper : public QoreExternalStackLocation {
public:
    //! Sets the current runtime location
    DLLEXPORT QoreExternalRuntimeStackLocationHelper();

    //! copy ctor
    DLLEXPORT QoreExternalRuntimeStackLocationHelper(const QoreExternalRuntimeStackLocationHelper&);

    //! move ctor
    DLLEXPORT QoreExternalRuntimeStackLocationHelper(QoreExternalRuntimeStackLocationHelper&&);

    //! Restores the old runtime location
    DLLEXPORT ~QoreExternalRuntimeStackLocationHelper();

    //! no assignment operator
    DLLLOCAL QoreExternalRuntimeStackLocationHelper& operator=(const QoreExternalRuntimeStackLocationHelper&) = delete;

    //! no move assignment operator
    DLLLOCAL QoreExternalRuntimeStackLocationHelper& operator=(QoreExternalRuntimeStackLocationHelper&&) = delete;

private:
    class qore_external_runtime_stack_location_helper_priv* priv;
};

#endif
