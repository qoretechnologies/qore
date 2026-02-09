/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreProgram.h

    Program Object Definition

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

#ifndef _QORE_QOREPROGRAM_H

#define _QORE_QOREPROGRAM_H

#include <qore/common.h>
#include <qore/AbstractPrivateData.h>
#include <qore/Restrictions.h>
#include <qore/OutputStream.h>
#include <qore/InputStream.h>

// warnings - must correspond with the string order in QoreProgram.cpp
// new warnings must also be added as constants
#define QP_WARN_NONE                     0
#define QP_WARN_WARNING_MASK_UNCHANGED   (1 << 0)   //!< when the warning mask is attempted to be changed but it's locked
#define QP_WARN_DUPLICATE_LOCAL_VARS     (1 << 1)   //!< duplicate local variable name reachable in the same scope
#define QP_WARN_UNKNOWN_WARNING          (1 << 2)   //!< when an unknown warning is encountered
#define QP_WARN_UNDECLARED_VAR           (1 << 3)   //!< when a variable is not declared
#define QP_WARN_DUPLICATE_GLOBAL_VARS    (1 << 4)   //!< when a global variable is declared more than once
#define QP_WARN_UNREACHABLE_CODE         (1 << 5)   //!< when unreachable code is encountered
#define QP_WARN_NONEXISTENT_METHOD_CALL  (1 << 6)   //!< when a non-existent method call is encountered
#define QP_WARN_INVALID_OPERATION        (1 << 7)   //!< when an expression always returns NOTHING, for example
#define QP_WARN_CALL_WITH_TYPE_ERRORS    (1 << 8)   //!< when a function or method call always returns a fixed value due to type errors
#define QP_WARN_RETURN_VALUE_IGNORED     (1 << 9)   //!< when a function or method call has no side effects and the return value is ignored
#define QP_WARN_DEPRECATED               (1 << 10)  //!< when deprecated functionality is accessed
#define QP_WARN_EXCESS_ARGS              (1 << 11)  //!< when excess arguments are given to a function that does not access them
#define QP_WARN_DUPLICATE_HASH_KEY       (1 << 12)  //!< when a hash key has been defined more than once in a literal hash
#define QP_WARN_UNREFERENCED_VARIABLE    (1 << 13)  //!< when a variable is declared but not referenced
#define QP_WARN_DUPLICATE_BLOCK_VARS     (1 << 14)  //!< when a variable is declared more than once in the same block; this would normally be an error, but for backwards-compatibility it's just a warning unless parse option 'assume-local' is set, in which case it's an error
#define QP_WARN_MODULE_ONLY              (1 << 15)  //!< when qualifiers that are only valid in modules are given when not parsing a module
#define QP_WARN_BROKEN_LOGIC_PRECEDENCE  (1 << 16)  //!< warning about expressions that are affected by broken-logic-precedence
#define QP_WARN_INVALID_CATCH            (1 << 17)  //!< when a catch block is missing a type declaration when %require-types is in effect
#define QP_WARN_AMBIGUOUS_CALL_RESOLUTION (1 << 18) //!< ambiguous call resolution at parse time
#define QP_WARN_AMBIGUOUS_OVERLOAD       (1 << 19)  //!< ambiguous overload definition or inheritance at parse time
#define QP_WARN_STRICT (QP_WARN_AMBIGUOUS_CALL_RESOLUTION|QP_WARN_AMBIGUOUS_OVERLOAD) //!< strict warnings excluded from QP_WARN_ALL
#define QP_WARN_ALL (QP_WARN_WARNING_MASK_UNCHANGED|QP_WARN_DUPLICATE_LOCAL_VARS|QP_WARN_UNKNOWN_WARNING|QP_WARN_UNDECLARED_VAR|QP_WARN_DUPLICATE_GLOBAL_VARS|QP_WARN_UNREACHABLE_CODE|QP_WARN_NONEXISTENT_METHOD_CALL|QP_WARN_INVALID_OPERATION|QP_WARN_CALL_WITH_TYPE_ERRORS|QP_WARN_RETURN_VALUE_IGNORED|QP_WARN_DEPRECATED|QP_WARN_EXCESS_ARGS|QP_WARN_DUPLICATE_HASH_KEY|QP_WARN_UNREFERENCED_VARIABLE|QP_WARN_DUPLICATE_BLOCK_VARS|QP_WARN_MODULE_ONLY|QP_WARN_BROKEN_LOGIC_PRECEDENCE|QP_WARN_INVALID_CATCH) //!< all non-strict warnings

#define QP_WARN_MODULES (QP_WARN_UNREACHABLE_CODE|QP_WARN_NONEXISTENT_METHOD_CALL|QP_WARN_INVALID_OPERATION|QP_WARN_CALL_WITH_TYPE_ERRORS|QP_WARN_RETURN_VALUE_IGNORED|QP_WARN_DUPLICATE_HASH_KEY|QP_WARN_DUPLICATE_BLOCK_VARS|QP_WARN_BROKEN_LOGIC_PRECEDENCE|QP_WARN_INVALID_CATCH)

#define QP_WARN_DEFAULT (QP_WARN_UNKNOWN_WARNING|QP_WARN_MODULES|QP_WARN_DEPRECATED)

enum BreakpointPolicy : unsigned char {
   BKP_PO_NONE = 0,
   BKP_PO_ACCEPT = 1,
   BKP_PO_REJECT = 2,
};

//! list of strings of warning codes
DLLEXPORT extern const char** qore_warnings;

//! number of warning codes
DLLEXPORT extern unsigned qore_num_warnings;

//! returns the warning code corresponding to the string passed (0 if no match was made)
DLLEXPORT int get_warning_code(const char* str);

// forward references
class AbstractCallReferenceNode;
class LocalVar;
class ExceptionSink;
class QoreListNode;
class QoreNamespace;
class RootQoreNamespace;
class QoreStringNode;
class QoreHashNode;
class FunctionCallNode;
class AbstractStatement;
class UnresolvedProgramCallReferenceNode;
class Var;
class LVList;
class UserFunctionVariant;
class QoreParseTypeInfo;
class ParamList;
class AbstractQoreZoneInfo;
class qore_program_private;
class AbstractQoreProgramExternalData;
class QoreBreakpoint;
class AbstractQoreFunctionVariant;
class QoreRWLock;
class QoreExternalFunction;
class QoreExternalGlobalVar;
class QoreExternalConstant;
class QoreSandboxManager;
class QoreEnumDecl;

typedef std::list<QoreBreakpoint*> bkp_list_t;

struct QoreBreakpointList_t : public bkp_list_t {
    DLLEXPORT QoreBreakpointList_t();
    // dereferences all breakpoints and clears the list
    DLLEXPORT ~QoreBreakpointList_t();
};

//! Callback type for program cleanup (called before namespace data is cleared)
/** Used by the reflection module to break reference cycles for Type objects (issue #4816)
    @param pgm the program being destroyed

    @since %Qore 2.3
*/
typedef void (*qore_program_cleanup_callback_t)(QoreProgram* pgm);

//! Registers a callback to be called before program namespace data is cleared
/** @param callback the callback function to register

    @since %Qore 2.3
*/
DLLEXPORT void qore_register_program_cleanup_callback(qore_program_cleanup_callback_t callback);

//! Calls the registered program cleanup callback for the given program
/** This should be called before dereferencing a program to break potential reference cycles.

    @param pgm the program to clean up

    @since %Qore 2.3
*/
DLLEXPORT void qore_call_program_cleanup_callback(QoreProgram* pgm);

//! supports parsing and executing Qore-language code, reference counted, dynamically-allocated only
/** This class implements a transaction and thread-safe container for qore-language code
    This class implements two-layered reference counting to address problems with circular references.
    When a program has a global variable that contains an object that references the program...
    objects now reference the dependency counter, so when the object's counter reaches zero and
    the global variable list is deleted, then the variables will in turn dereference the program
    so it can be deleted.
*/
class QoreProgram : public AbstractPrivateData {
    friend class qore_program_private_base;
    friend class qore_program_private;
    friend class qore_debug_program_private;
    friend struct ThreadLocalProgramData;

public:
    //! creates the object
    DLLEXPORT QoreProgram();

    //! creates the object and sets the parse options
    /**
        @param parse_options the parse options mask for the QoreProgram object
    */
    DLLEXPORT QoreProgram(int64 parse_options);

    //! calls a function from the function name and returns the return value
    /** if the function does not exist, an exception is added to "xsink"
        @param name the name of the function to call
        @param args the argument to the function (can be nullptr)
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return the value returned by the function (if any)
    */
    DLLEXPORT QoreValue callFunction(const char* name, const QoreListNode* args, ExceptionSink* xsink);

    //! runs the program (instantiates the program class if a program class has been set) and returns the return value (if any)
    /** @note if the program is run as a class it's not possible to return a value
        @note any threads started by this call will continue running in the background,
        to wait for them to terminate, call QoreProgram::waitForTermination() or
        QoreProgram::waitForTerminationAndDeref()
        @see QoreProgram::setExecClass()
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return the value returned by the final return statement (if any)
    */
    DLLEXPORT QoreValue run(ExceptionSink* xsink);

    //! tuns the top level code and returns any return value
    /**
        @note any threads started by this call will continue running in the background,
        to wait for them to terminate, call QoreProgram::waitForTermination() or
        QoreProgram::waitForTerminationAndDeref()
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return the value returned by the final return statement (if any)
    */
    DLLEXPORT QoreValue runTopLevel(ExceptionSink* xsink);

    //! parses the given filename and runs the file
    /** any errors opening the file are added as Qore-language exceptions
        the default exception handler is run on any Qore-language exceptions
        raised during opening, parsing, and executing the file.
        @note any threads started by this call will continue running in the background,
        to wait for them to terminate, call QoreProgram::waitForTermination() or
        QoreProgram::waitForTerminationAndDeref().
        Also sets the script path.
        @note will also commit any pending changes added with QoreProgram::parsePending()
        @param filename the filename to run
    */
    DLLEXPORT void parseFileAndRun(const char* filename);

    //! parses the given file and runs the file
    /** the default exception handler is run on any Qore-language exceptions
        raised while parsing and executing the file.
        @note any threads started by this call will continue running in the background,
        to wait for them to terminate, call QoreProgram::waitForTermination() or
        QoreProgram::waitForTerminationAndDeref()
        @note will also commit any pending changes added with QoreProgram::parsePending()
        @param fp the filename to run
        @param name the name of the file being parsed and run
    */
    DLLEXPORT void parseAndRun(FILE *fp, const char* name);

    //! parses the given string and runs the code
    /** The default exception handler is run on any Qore-language exceptions
        raised while parsing and executing the code.
        @note any threads started by this call will continue running in the background,
        to wait for them to terminate, call QoreProgram::waitForTermination() or
        QoreProgram::waitForTerminationAndDeref()
        @note will also commit any pending changes added with QoreProgram::parsePending()
        @param str the Qore-language code to parse and run
        @param name the label of the code being parsed and run (used as the file name)
    */
    DLLEXPORT void parseAndRun(const char* str, const char* name);

    //! instantiates the class given and runs its constructor
    /** @note any threads started by this call will continue running in the background,
        to wait for them to terminate, call QoreProgram::waitForTermination() or
        QoreProgram::waitForTerminationAndDeref()
        @note will also commit any pending changes added with QoreProgram::parsePending()
        @param classname the name of the class to instantiate
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT void runClass(const char* classname, ExceptionSink* xsink);

    //! parses the given filename and runs the program by instantiating the class given
    /** Any errors opening the file are added as Qore-language exceptions.
        The default exception handler is run on any Qore-language exceptions
        raised during opening, parsing, and executing the file.
        @note any threads started by this call will continue running in the background,
        to wait for them to terminate, call QoreProgram::waitForTermination() or
        QoreProgram::waitForTerminationAndDeref().
        Also sets the script path.
        @note will also commit any pending changes added with QoreProgram::parsePending()
        @param filename the filename to run
        @param classname the name of the class to instantiate
    */
    DLLEXPORT void parseFileAndRunClass(const char* filename, const char* classname);

    //! parses the given file and runs the code by instantiating the class given
    /** The default exception handler is run on any Qore-language exceptions
        raised while parsing and executing the file.
        @note any threads started by this call will continue running in the background,
        to wait for them to terminate, call QoreProgram::waitForTermination() or
        QoreProgram::waitForTerminationAndDeref()
        @note will also commit any pending changes added with QoreProgram::parsePending()
        @param fp the filename to run
        @param name the name of the file being parsed and run
        @param classname the name of the class to instantiate
    */
    DLLEXPORT void parseAndRunClass(FILE *fp, const char* name, const char* classname);

    //! parses the given string and runs the code by instantiating the class given
    /** The default exception handler is run on any Qore-language exceptions
        raised while parsing and executing the code.
        @note any threads started by this call will continue running in the background,
        to wait for them to terminate, call QoreProgram::waitForTermination() or
        QoreProgram::waitForTerminationAndDeref()
        @note will also commit any pending changes added with QoreProgram::parsePending()
        @param str the Qore-language code to parse and run
        @param name the label of the code being parsed and run (used as the file name)
        @param classname the name of the class to instantiate
    */
    DLLEXPORT void parseAndRunClass(const char* str, const char* name, const char* classname);

    //! parses code from the file given and commits changes to the QoreProgram
    /**
        @note will also commit any pending changes added with QoreProgram::parsePending()
        @param fp the filename to parse
        @param name the name of the file being parsed and run
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param warn_sink if a warning is raised, the warning information will be added here
        @param warn_mask the warning mask to set (-1 sets all possible warnings)
    */
    DLLEXPORT void parse(FILE *fp, const char* name, ExceptionSink* xsink, ExceptionSink* warn_sink = 0, int warn_mask = QP_WARN_ALL);

    //! parses code from the given string and commits changes to the QoreProgram
    /**
        @note will also commit any pending changes added with QoreProgram::parsePending()
        @param str the code to parse
        @param lstr the label of the code being parsed to be used as a file name
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param warn_sink if a warning is raised, the warning information will be added here
        @param warn_mask the warning mask to set (-1 sets all possible warnings)
    */
    DLLEXPORT void parse(const QoreString* str, const QoreString* lstr, ExceptionSink* xsink, ExceptionSink* warn_sink = 0, int warn_mask = QP_WARN_ALL);

    //! parses code from the given string and commits changes to the QoreProgram
    /**
        @note will also commit any pending changes added with QoreProgram::parsePending()
        @param str the code to parse
        @param lstr the label of the code being parsed to be used as a file name
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param warn_sink if a warning is raised, the warning information will be added here (0 = no warnings)
        @param warn_mask the warning mask to set (-1 sets all possible warnings)
        @param source the source file name (if lstr is a label representing a section of a file for example)
        @param offset the line offset from the label to the file
    */
    DLLEXPORT void parse(const QoreString* str, const QoreString* lstr, ExceptionSink* xsink, ExceptionSink* warn_sink, int warn_mask, const QoreString* source, int offset);

    //! parses code from the given string and commits changes to the QoreProgram
    /**
        @note will also commit any pending changes added with QoreProgram::parsePending()
        @param str the code to parse; the encoding of the string is assumed to be QCS_DEFAULT
        @param lstr the label of the code being parsed to be used as a file name
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param warn_sink if a warning is raised, the warning information will be added here (0 = no warnings)
        @param warn_mask the warning mask to set (-1 sets all possible warnings)
    */
    DLLEXPORT void parse(const char* str, const char* lstr, ExceptionSink* xsink, ExceptionSink* warn_sink = 0, int warn_mask = QP_WARN_ALL);

    //! parses code from the given string and commits changes to the QoreProgram
    /**
        @note will also commit any pending changes added with QoreProgram::parsePending()
        @param str the code to parse; the encoding of the string is assumed to be QCS_DEFAULT
        @param lstr the label of the code being parsed to be used as a file name
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param warn_sink if a warning is raised, the warning information will be added here (0 = no warnings)
        @param warn_mask the warning mask to set (-1 sets all possible warnings)
        @param source the source file name (if lstr is a label representing a section of a file for example)
        @param offset the line offset from the label to the file
    */
    DLLEXPORT void parse(const char* str, const char* lstr, ExceptionSink* xsink, ExceptionSink* warn_sink, int warn_mask, const char* source, int offset);

    //! parses code from the file given and commits changes to the QoreProgram
    /** Also sets the script path.
        @note will also commit any pending changes added with QoreProgram::parsePending()
        @param filename the filename to open and parse
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param warn_sink if a warning is raised, the warning information will be added here (0 = no warnings)
        @param warn_mask the warning mask to set (-1 sets all possible warnings)
        @param only_first_except is flag to stop parsing exceptions printing after 1st exception
    */
    DLLEXPORT void parseFile(const char* filename, ExceptionSink* xsink, ExceptionSink* warn_sink = 0, int warn_mask = QP_WARN_ALL, bool only_first_except = false);

    //! parses code from the given string but does not commit changes to the QoreProgram
    /**
        @param code the code to parse; the encoding of the string is assumed to be QCS_DEFAULT
        @param label the label of the code being parsed to be used as a file name
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param warn_sink if a warning is raised, the warning information will be added here (0 = no warnings)
        @param warn_mask the warning mask to set (-1 sets all possible warnings)
        @see QoreProgram::parseCommit()
        @see QoreProgram::parseRollback()
    */
    DLLEXPORT void parsePending(const char* code, const char* label, ExceptionSink* xsink, ExceptionSink* warn_sink = 0, int warn_mask = QP_WARN_ALL);

    //! parses code from the given string but does not commit changes to the QoreProgram
    /**
        @param code the code to parse; the encoding of the string is assumed to be QCS_DEFAULT
        @param label the label of the code being parsed to be used as a file name
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param warn_sink if a warning is raised, the warning information will be added here (0 = no warnings)
        @param warn_mask the warning mask to set (-1 sets all possible warnings)
        @param source the source file name (if lstr is a label representing a section of a file for example)
        @param offset the line offset from the label to the file

        @see QoreProgram::parseCommit()
        @see QoreProgram::parseRollback()
    */
    DLLEXPORT void parsePending(const char* code, const char* label, ExceptionSink* xsink, ExceptionSink* warn_sink, int warn_mask, const char* source, int offset);

    //! parses code from the given string but does not commit changes to the QoreProgram
    /**
        @param str the code to parse
        @param lstr the label of the code being parsed to be used as a file name
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param warn_sink if a warning is raised, the warning information will be added here (0 = no warnings)
        @param warn_mask the warning mask to set (-1 sets all possible warnings)

        @see QoreProgram::parseCommit()
        @see QoreProgram::parseRollback()
    */
    DLLEXPORT void parsePending(const QoreString* str, const QoreString* lstr, ExceptionSink* xsink, ExceptionSink* warn_sink = 0, int warn_mask = QP_WARN_ALL);

    //! parses code from the given string but does not commit changes to the QoreProgram
    /**
        @param str the code to parse
        @param lstr the label of the code being parsed to be used as a file name
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param warn_sink if a warning is raised, the warning information will be added here (0 = no warnings)
        @param warn_mask the warning mask to set (-1 sets all possible warnings)
        @param source the source file name (if lstr is a label representing a section of a file for example)
        @param offset the line offset from the label to the file

        @see QoreProgram::parseCommit()
        @see QoreProgram::parseRollback()
    */
    DLLEXPORT void parsePending(const QoreString* str, const QoreString* lstr, ExceptionSink* xsink, ExceptionSink* warn_sink, int warn_mask, const QoreString* source, int offset);

    //! commits pending changes to the program
    /**
        @see QoreProgram::parsePending()
        @see QoreProgram::parseRollback()
    */
    DLLEXPORT void parseCommit(ExceptionSink* xsink, ExceptionSink* warn_sink = 0, int warn_mask = QP_WARN_ALL);

    //! rolls back changes to the program object that were added with QoreProgram::parsePending()
    /** When used with PO_ALLOW_REPARSE, this method performs an atomic rollback of only the pending parse
        transaction, preserving all previously committed program state. This enables REPL-style interactive
        programming where parse errors don't destroy the Program object.

        Without PO_ALLOW_REPARSE, this method clears all program state including committed items.

        A Qore-language exception could be raised if the parse lock could not be acquired (Program has running threads)

        @see QoreProgram::parsePending()
        @see QoreProgram::parseCommit()

        @since Qore 2.3 provides atomic rollback preserving committed state when used with PO_ALLOW_REPARSE
    */
    DLLEXPORT int parseRollback(ExceptionSink* xsink);

    //! returns true if the given function exists as a user function, false if not
    DLLEXPORT bool existsFunction(const char* name);

    using AbstractPrivateData::deref;
    //! dereferences the object and deletes it if the reference count reaches zero
    /** do not use this function if the program may be running, use QoreProgram::waitForTerminationAndDeref() instead
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @see QoreProgram::waitForTerminationAndDeref()
    */
    DLLEXPORT virtual void deref(ExceptionSink* xsink);

    //! references "this" and returns a non-const pointer to itself
    DLLEXPORT QoreProgram* programRefSelf() const;

    //! locks parse options so they may not be changed
    DLLEXPORT void lockOptions();

    //! sets the name of the application class to be executed (instantiated) instead of top-level code
    /** normally parse option PO_NO_TOP_LEVEL_STATEMENTS should be set as well
        @note the string passed here will copied
        @param ecn the name of the class to be executed as the program class
        @see QoreProgram::parseSetParseOptions()
    */
    DLLEXPORT void setExecClass(const char* ecn = 0);

    //! adds the parse options given to the parse option mask
    DLLEXPORT void parseSetParseOptions(int64 po);

    //! disables the parse options given to the parse option mask
    DLLEXPORT void parseDisableParseOptions(int64 po);

    //! this call blocks until the program's last thread terminates
    DLLEXPORT void waitForTermination();

    //! this call blocks until the program's last thread terminates, and then calls QoreProgram::deref()
    /**
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT void waitForTerminationAndDeref(ExceptionSink* xsink);

    //! returns a pointer to the "Qore" namespace
    DLLEXPORT QoreNamespace* getQoreNS() const;

    //! returns a pointer to the root namespace
    DLLEXPORT RootQoreNamespace* getRootNS() const;

    //! sets the warning mask
    /**
        @param wm the new warning mask
        @return 0 for success, -1 for error
    */
    DLLEXPORT int setWarningMask(int wm);

    //! enables a warning by its code
    /**
        @param code the warning code to enable
        @return 0 for success, -1 for error
    */
    DLLEXPORT int enableWarning(int code);

    //! disables a warning by its code
    /**
        @param code the warning code to disable
        @return 0 for success, -1 for error
    */
    DLLEXPORT int disableWarning(int code);

    //! returns the parse options currently set for this program; DEPRECATED; use getParseOptions64() instead
    DLLEXPORT int getParseOptions() const;

    //! returns the parse options currently set for this program
    DLLEXPORT int64 getParseOptions64() const;

    //! sets the parse options and adds Qore-language exception information if an error occurs
    /**
        @param po the parse options to add to the parse option mask
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT void setParseOptions(int64 po, ExceptionSink* xsink);

    //! turns off the parse options given in the passed mask and adds Qore-language exception information if an error occurs
    /**
        @param po the parse options to subtract from the parse option mask
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT void disableParseOptions(int64 po, ExceptionSink* xsink);

    //! replaces the parse options in the program with those given by the argument; adds Qore-language exception information if an error occurs
    /** An exception will be raised if the calling program does not have PO_NO_CHILD_PO_RESTRICTIONS set
        @param po the parse options to add to the parse option mask
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT void replaceParseOptions(int64 po, ExceptionSink* xsink);

    //! returns a list of all user functions in this program
    /**
        @return a list of all user functions in this program; returns 0 only if the Program is being destroyed, otherwise returns an empty list
    */
    DLLEXPORT QoreListNode* getUserFunctionList();

    //! returns true if the warning code is set
    DLLEXPORT bool checkWarning(int code) const;

    //! returns the warning mask
    DLLEXPORT int getWarningMask() const;

    //! returns true if the given feature is present in the program object
    DLLEXPORT bool checkFeature(const char* f) const;

    //! returns a list of features in the program object
    DLLEXPORT QoreListNode* getFeatureList() const;

    //! returns the script file name, if known (0 if not)
    /** @return the script file name, if known (0 if not)
    */
    DLLEXPORT QoreStringNode* getScriptName() const;

    //! returns the script path (directory and name), if known (0 if not)
    /** @return the script path (directory and name), if known (0 if not)
    */
    DLLEXPORT QoreStringNode* getScriptPath() const;

    //! returns the script directory, if known (0 if not)
    /** @return the script directory, if known (0 if not)
    */
    DLLEXPORT QoreStringNode* getScriptDir() const;

    //! sets the script path
    /** @param path the directory and filename of the script (set to 0 to clear)
    */
    DLLEXPORT void setScriptPath(const char* path);

    //! returns the value of the global variable given (do not include the "$" symbol), the caller owns the reference count returned
    /** @param var the variable name to return (do not include the "$" symbol)
        @param found returns true if the variable exists, false if not
        @return the value of the global variable given; if a non-zero pointer is returned, the caller owns the reference count returned
    */
    DLLEXPORT QoreValue getGlobalVariableValue(const char* var, bool &found) const;

    //! returns the value of the global variable given (do not include the "$" symbol), the caller owns the reference count returned
    /** @param var the variable name to return (do not include the "$" symbol)
        @param found returns true if the variable exists, false if not
        @return the value of the global variable given; if a non-zero pointer is returned, the caller owns the reference count returned
    */
    DLLEXPORT QoreValue getGlobalVariableVal(const char* var, bool &found) const;

    // retrieves the time zone setting for the program
    DLLEXPORT const AbstractQoreZoneInfo *currentTZ() const;

    // sets the program's time zone
    DLLEXPORT void setTZ(const AbstractQoreZoneInfo *n_TZ);

    //! manually add the feature to the program
    /** useful for use with %module-cmd when manually merging in namespace changes to the program
        @param name the name of the feature to add to the QoreProgram's feature list
    */
    DLLEXPORT void addFeature(const char* name);

    //! sets the time zone during parsing
    /** @param zone can be either a region name (ex: 'Europe/Prague') or a UTC offset in the format SDD[:DD[:DD]] where S is + or - and D is an integer 0 - 9; the ':' characters are optional

        @note do not call this function if there are any running threads; a crash could result
    */
    DLLEXPORT void parseSetTimeZone(const char* zone);

    //! defines a parse-time variable; call only at parse time (or before parsing)
    /** @param str the name of the variable
        @param val the value of the variable; may be 0; if non-0, then the QoreProgram object assumes ownership of the reference

        @note if this function is called at runtime it could cause a crash
    */
    DLLEXPORT void parseDefine(const char* str, QoreValue val);

    //! defines a parse-time variable; call only at parse time (or before parsing)
    /** @param str the name of the variable
        @param val a string value that will be parsed and converted to a qore value

        @note if this function is called at runtime it could cause a crash
    */
    DLLEXPORT void parseDefine(const char* str, const char* val);

    // @deprecated use parseCmdLineDefines(ExceptionSink& xs, ExceptionSink& ws, int w, const std::map<std::string, std::string>& defmap) instead
    DLLEXPORT void parseCmdLineDefines(const std::map<std::string, std::string> defmap, ExceptionSink& xs, ExceptionSink& ws, int w);

    //! defines parse-time variables
    /** @param xs exception sink for errors
        @param ws exception sink for warnings
        @param w warning mask
        @param defmap a map of variable names to values
    */
    DLLEXPORT void parseCmdLineDefines(ExceptionSink& xs, ExceptionSink& ws, int w, const std::map<std::string, std::string>& defmap);

    //! sets a pointer to external data in the Program
    /** @param owner a unique string identifying the owner of the data; for modules this should be the module name
        @param pud the external data

        @since %Qore 0.8.13
    */
    DLLEXPORT void setExternalData(const char* owner, AbstractQoreProgramExternalData* pud);

    //! retrieves the external data pointer
    /** @param owner a unique string identifying the owner of the data; for modules this should be the module name

        @return the data if set otherwise nullptr is returned

        @since %Qore 0.8.13
    */
    DLLEXPORT AbstractQoreProgramExternalData* getExternalData(const char* owner) const;

    //! removes a pointer to external data in the Program; does not dereference the data
    /** @param owner a unique string identifying the owner of the data; for modules this should be the module name

        @since %Qore 0.9.5
    */
    DLLEXPORT AbstractQoreProgramExternalData* removeExternalData(const char* owner);

    //! retrieves a hash of global variables and their values
    /** @return a hash of global variable information; keys are namespace-justified global variable names, values are the values

        @since %Qore 0.8.13
    */
    DLLEXPORT QoreHashNode* getGlobalVars() const;

    //! sets the value of the given global variable
    /** @param name the name of the variable
        @param val the value to assign; the value must be already referenced for the assignment and will be dereferenced if the assignment fails
        @param xsink for Qore-language exceptions

        @return 0 for OK, -1 if an exception was raised

        @since %Qore 0.8.13
    */
    DLLEXPORT int setGlobalVarValue(const char* name, QoreValue val, ExceptionSink* xsink);

    // finds a function or class method variant if possible
    /** @param name the function or class method name; may also be namespace-justified
        @param params a list of string parameters giving the types to match
        @param xsink any errors finding the code are stored as Qore-language exceptions here; possible errors are as follows:
        - \c FIND-CALL-ERROR: \a name cannot be resolved, invalid parameter (member of \a params has a non-string value, string cannot be resolved to a valid type)
        - \c INVALID-FUNCTION-ACCESS: the resolved code is not accessible in the current QoreProgram's context

        @since %Qore 0.8.13
    */
    DLLEXPORT const AbstractQoreFunctionVariant* runtimeFindCall(const char* name, const QoreListNode* params, ExceptionSink* xsink) const;

    // finds all variants of a function or class method and returns a list of the results
    /** @param name the function or class method name; may also be namespace-justified
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return a list of hashes or nullptr if the name cannot be resolved; when matched, each hash element has the following keys:
        - \c desc: a string description of the call which includes the name and the full text call signature
        - \c params: a QoreListNode object that gives the params in a format that can be used by runtimeFindCall()

        @note the caller owns the reference count returned for non-nullptr values

        @since %Qore 0.8.13
    */
    DLLEXPORT QoreListNode* runtimeFindCallVariants(const char* name, ExceptionSink* xsink) const;

    //! returns a list of threads active in this Program object
    /** @return a list of threads active in this Program object

        @since %Qore 0.8.13
    */
    DLLEXPORT QoreListNode* getThreadList() const;

    //! returns the number of threads currently active in this Program object
    /** @return the number of threads currently active in this Program object

        @since %Qore 2.3
    */
    DLLEXPORT unsigned getThreadCount() const;

    //! search for the given class in the program; can be a simple class name or a namespace-prefixed path (ex: "NamespaceName::ClassName")
    /** @since %Qore 0.9
    */
    DLLEXPORT const QoreClass* findClass(const char* path, ExceptionSink* xsink) const;

    //! returns a list of all classes that match the pattern
    /** @since %Qore 0.9
    */
    DLLEXPORT class_vec_t findAllClassesRegex(const QoreString& pattern, int re_opts, ExceptionSink* xsink) const;

    //! returns a list of all typed hashes (hashdecls) that match the pattern
    /** @since %Qore 0.9
    */
    DLLEXPORT hashdecl_vec_t findAllHashDeclsRegex(const QoreString& pattern, int re_opts, ExceptionSink* xsink) const;

    //! returns a list of all functions that match the pattern
    /** @since %Qore 0.9
    */
    DLLEXPORT func_vec_t findAllFunctionsRegex(const QoreString& pattern, int re_opts, ExceptionSink* xsink) const;

    //! returns a list of all namespaces that match the pattern
    /** @since %Qore 0.9
    */
    DLLEXPORT ns_vec_t findAllNamespacesRegex(const QoreString& pattern, int re_opts, ExceptionSink* xsink) const;

    //! returns a list of all global variables that match the pattern
    /** @since %Qore 0.9
    */
    DLLEXPORT gvar_vec_t findAllGlobalVarsRegex(const QoreString& pattern, int re_opts, ExceptionSink* xsink) const;

    //! returns a list of all namespace constants that match the pattern
    /** @since %Qore 0.9
    */
    DLLEXPORT const_vec_t findAllNamespaceConstantsRegex(const QoreString& pattern, int re_opts, ExceptionSink* xsink) const;

    //! search for the given function in the program; can be a simple function name or a namespace-prefixed path (ex: "NamespaceName::function_name")
    /** @since %Qore 0.9
    */
    DLLEXPORT const QoreExternalFunction* findFunction(const char* path) const;

    //! search for the given typed hash (hashdecl) in the program; can be a simple function name or a namespace-prefixed path (ex: "NamespaceName::TypedHashName")
    /** @since %Qore 0.9
    */
    DLLEXPORT const TypedHashDecl* findHashDecl(const char* path, const QoreNamespace*& pns) const;

    //! search for the given enum in the program; can be a simple enum name or a namespace-prefixed path (ex: "NamespaceName::EnumName")
    /** @since %Qore 2.1
    */
    DLLEXPORT const QoreEnumDecl* findEnum(const char* path, const QoreNamespace*& pns) const;

    //! search for the given namespace in the program; can be a simple namespace name or a namespace-prefixed path (ex: "NamespaceName::Namespace")
    /** @note this function is safe to call during module initialization for the current Program (as returned by
        getProgram())

        @since %Qore 0.9
    */
    DLLEXPORT QoreNamespace* findNamespace(const QoreString& path);

    //! search for the given namespace in the program; can be a simple namespace name or a namespace-prefixed path (ex: "NamespaceName::Namespace")
    /** @since %Qore 0.9
    */
    DLLEXPORT const QoreNamespace* findNamespace(const QoreString& path) const;

    //! search for the given global variable in the program; can be a simple function name or a namespace-prefixed path (ex: "NamespaceName::global_var")
    /** @since %Qore 0.9
    */
    DLLEXPORT const QoreExternalGlobalVar* findGlobalVar(const char* path, const QoreNamespace*& pns) const;

    //! search for the given namespace constant in the program; can be a simple function name or a namespace-prefixed path (ex: "NamespaceName::MyConstant")
    /** @since %Qore 0.9
    */
    DLLEXPORT const QoreExternalConstant* findNamespaceConstant(const char* path, const QoreNamespace*& pns) const;

    //! incremements the weak reference count for the program object
    /** @since %Qore 0.9
    */
    DLLEXPORT void depRef();

    //! dereferences a weak reference for the program object
    /** @since %Qore 0.9 public
    */
    DLLEXPORT void depDeref();

    //! returns the thread-local data for the current thread and the Program object
    /** will always be non-nullptr if the thread is registered

        @since %Qore 0.9
    */
    DLLEXPORT QoreHashNode* getThreadData();

    //! sets the execution mode for this program
    DLLEXPORT void setExecMode(qore_exec_mode_t mode);

    //! gets the current execution mode for this program
    DLLEXPORT qore_exec_mode_t getExecMode() const;

    //! enables or disables IR dump before execution
    DLLEXPORT void setIRDump(bool dump);

    //! returns true if IR dump is enabled
    DLLEXPORT bool getIRDump() const;

    //! enables or disables IR fallback warnings to stderr
    DLLEXPORT void setIRFallbackWarn(bool warn);

    //! returns true if IR fallback warnings are enabled
    DLLEXPORT bool getIRFallbackWarn() const;

    //! enables or disables IR fallback reporting (counts by category at exit)
    DLLEXPORT void setIRFallbackReport(bool report);

    //! returns true if IR fallback reporting is enabled
    DLLEXPORT bool getIRFallbackReport() const;

    //! records an IR fallback event for later reporting
    DLLEXPORT void recordIRFallback(const char* reason) const;

    //! prints the IR fallback report to stderr (called at program exit)
    DLLEXPORT void printIRFallbackReport() const;

    //! issues a module command for the given module; the module is loaded into the current %Program object if it is not already present
    /** @param module the module name; must be in the default character encoding (normally UTF-8)
        @param cmd the command string to execute to be parsed by the module (normally UTF-8)
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return -1 if an error occurred (in which case the error information is in \a xsink), 0 if not

        @note command errors can result in other module- and command-specific exceptions being thrown; see
        documentation for the module and command in question for more details

        @since %Qore 0.9.5
    */
    DLLEXPORT int issueModuleCmd(const char* module, const char* cmd, ExceptionSink* xsink);

    DLLLOCAL QoreProgram(QoreProgram* pgm, int64 po, bool ec = false, const char* ecn = nullptr);

    DLLLOCAL LocalVar *createLocalVar(const char* name, const QoreTypeInfo *typeInfo);

    // returns 0 if a "requires" exception has already occurred
    DLLLOCAL ExceptionSink* getParseExceptionSink();

    DLLLOCAL void addFile(char* f);
    DLLLOCAL QoreListNode* getVarList();
    DLLLOCAL void parseSetIncludePath(const char* path);
    DLLLOCAL const char* parseGetIncludePath() const;

    /* for run-time module loading; the parse lock must be grabbed
        before loading new modules - note this should only be assigned
        to an AutoLock or SafeLocker object!
    */
    DLLLOCAL QoreThreadLock *getParseLock();
    DLLLOCAL const LVList* getTopLevelLVList() const;

    //! returns the script directory, if known (0 if not), does not grab the parse lock, only to be called while parsing
    /** @return the script directory, if known (0 if not)
    */
    DLLLOCAL const char* parseGetScriptDir() const;

    // can only be called while parsing from the same thread doing the parsing
    DLLLOCAL bool parseExceptionRaised() const;

    // TODO: implement !
    /** returns the value of the local variable given (do not include the "$" symbol), the caller owns the reference count returned
        The variable is related to current frame. This function can be executed only when program is stopped

        @param var the variable name to return (do not include the "$" symbol)
        @param found returns true if the variable exists, false if not
        @return the value of the global variable given; if a non-zero pointer is returned, the caller owns the reference count returned
    */
    DLLEXPORT QoreValue getLocalVariableVal(const char* var, bool &found) const;

    /** Assign @ref QoreBreakpoint instance to @ref QoreProgram. If breakpoint has been assigned to an program then is unassigned in the first step.
    */
    DLLEXPORT void assignBreakpoint(QoreBreakpoint *bkpt, ExceptionSink *xsink);

    /** delete all breakpoints from instance
    */
    DLLEXPORT void deleteAllBreakpoints();

    /** get list of breakpoint assigned to program.
    */
    DLLEXPORT void getBreakpoints(QoreBreakpointList_t &bkptList);

    /** get list of breakpoint assigned to statement.
    */
    DLLEXPORT void getStatementBreakpoints(const AbstractStatement* statement, QoreBreakpointList_t &bkptList);

    /** find statement related to particular line in a source file
    */
    DLLEXPORT AbstractStatement* findStatement(const char* fileName, int line) const;

    /** find statement related to particular function
    */
    DLLEXPORT AbstractStatement* findFunctionStatement(const char* functionName, const QoreListNode* params, ExceptionSink* xsink) const;

    //! get the statement id
    /**
        @param statement MUST be statement of this Program instance!
        @return the statement id which consist of pointer to both program and statement instances
    */
    DLLEXPORT unsigned long getStatementId(const AbstractStatement* statement) const;

    //! get the statement from statement id
    /**
        @param statementId created by @ref QoreProgram::getStatementId

        @return the original statement or null if statement cannot be resolved
    */
    DLLEXPORT AbstractStatement* resolveStatementId(unsigned long statementId) const;

    //! get list of files which appears in a statement
    /**
        @return hash where the key is file name and value is hash where the key value is label and the value is label's section offset in the file
    */
    DLLEXPORT QoreHashNode* getSourceFileNames(ExceptionSink* xsink) const;
    //! get list of labels which appears in a statement
    /**
        @return hash where the key is label name and value is hash where the key is file and the value is label's section offset in the file
    */
    DLLEXPORT QoreHashNode* getSourceLabels(ExceptionSink* xsink) const;

    //! get the program id
    /**
        @return the program id
    */
    DLLEXPORT unsigned getProgramId() const;

    //! get the program from program id
    /**
        @param programId provided by @ref QoreProgram::getProgramId

        @return the original program or null if program cannot be resolved
    */
    DLLEXPORT static QoreProgram* resolveProgramId(unsigned programId);

    //! register link to Qore script object
    DLLEXPORT void registerQoreObject(QoreObject *o, ExceptionSink* xsink) const;

    //! unregister link to Qore script object
    DLLEXPORT void unregisterQoreObject(QoreObject *o, ExceptionSink* xsink) const;

    //! find Qore script object related to QoreProgram instance
    DLLEXPORT QoreObject* findQoreObject() const;

    //! get QoreObject of QoreProgram
    DLLEXPORT static QoreObject* getQoreObject(QoreProgram* pgm);

    //! list all programs as QoreObject list
    DLLEXPORT static QoreListNode* getAllQoreObjects(ExceptionSink* xsink);

    //! check if program can provide debugging stuff
    DLLEXPORT bool checkAllowDebugging(ExceptionSink* xsink);

    //! sets the sandbox manager for this program
    /** @param sm the sandbox manager to set; the program takes a strong reference
        (calls ref()), and releases it when the program is destroyed or a new
        sandbox manager is set

        @since %Qore 2.1
    */
    DLLEXPORT void setSandboxManager(QoreSandboxManager* sm);

protected:
    //! the destructor is private in order to prohibit the object from being allocated on the stack
    /** the destructor is run when the reference count reaches 0
    */
    DLLLOCAL virtual ~QoreProgram();

private:
    //! private implementation
    qore_program_private* priv;

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL QoreProgram(const QoreProgram&);

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL QoreProgram& operator=(const QoreProgram&);
};

//! safely manages QoreProgram objects; note the the destructor will block until all background threads in the qore library terminate and until the current QoreProgram terminates
/** not useful in embedded code due to the fact that the destructor blocks until all background threads in the entire qore library terminate
 */
class QoreProgramHelper {
private:
   QoreProgram* pgm;
   ExceptionSink& xsink;

public:
   //! creates the QoreProgram object: DEPRECATED: use QoreProgramHelper(int64, ExceptionSink&) instead
   DLLEXPORT QoreProgramHelper(ExceptionSink& xs);

   //! creates the QoreProgram object and sets the parse options
   DLLEXPORT QoreProgramHelper(int64 parse_options, ExceptionSink& xs);

   //! waits until all background threads in the Qore library have terminated and until the QoreProgram object is done executing and then dereferences the object
   /** QoreProgram objects are deleted when there reference count reaches 0.
    */
   DLLEXPORT ~QoreProgramHelper();

   //! returns the QoreProgram object being managed
   DLLEXPORT QoreProgram* operator->();

   //! returns the QoreProgram object being managed
   DLLEXPORT QoreProgram* operator*();
};

//! allows for the parse lock for the current program to be acquired by binary modules
/** @since %Qore 0.8.13
 */
class CurrentProgramRuntimeExternalParseContextHelper {
public:
    //! acquires the parse lock; if already acquired by another thread, then this call blocks until the lock can be acquired
    DLLEXPORT CurrentProgramRuntimeExternalParseContextHelper();

    //! releases the parse lock for the current program
    DLLEXPORT ~CurrentProgramRuntimeExternalParseContextHelper();

    //! returns true if the object is valid (lock acquired), false if not (program already deleted)
    DLLEXPORT operator bool() const;

private:
    bool valid = true;

    // not implemented
    CurrentProgramRuntimeExternalParseContextHelper(const CurrentProgramRuntimeExternalParseContextHelper&) = delete;
    void* operator new(size_t) = delete;
};

//! allows for the parse lock for the current program to be acquired by binary modules
/** @since %Qore 1.0.13
 */
class ProgramRuntimeExternalParseContextHelper {
public:
    //! acquires the parse lock; if already acquired by another thread, then this call blocks until the lock can be acquired
    DLLEXPORT ProgramRuntimeExternalParseContextHelper(QoreProgram* pgm);

    //! releases the parse lock for the current program
    DLLEXPORT ~ProgramRuntimeExternalParseContextHelper();

    //! returns true if the object is valid (lock acquired), false if not (program already deleted)
    DLLEXPORT operator bool() const;

private:
    QoreProgram* pgm;

    // not implemented
    ProgramRuntimeExternalParseContextHelper(const ProgramRuntimeExternalParseContextHelper&) = delete;
    void* operator new(size_t) = delete;
};

//! allows for external modules to set the current Program context explicitly
/** @since %Qore 0.8.13
 */
class QoreProgramContextHelper {
public:
    //! sets the current Program context
    DLLEXPORT QoreProgramContextHelper(QoreProgram* pgm);
    //! restores the previous Program context
    DLLEXPORT ~QoreProgramContextHelper();

private:
    QoreProgram* old_pgm;

    // not implemented
    QoreProgramContextHelper(const QoreProgramContextHelper&) = delete;
    void* operator new(size_t) = delete;
};

//! an abstract class for program-specific external data
/** This class can be used by binary modules to store custom data in a QoreProgram object

    @see
    - QoreProgram::getExternalData()
    - QoreProgram::setExternalData()

    @since %Qore 0.8.13
 */
class AbstractQoreProgramExternalData {
public:
    //! Destroys the object
    /** The base class implementation is empty
     */
    DLLEXPORT virtual ~AbstractQoreProgramExternalData();

    //! For reference-counted classes, returns the same object with the reference count incremented
    /** This function is called for external data when a new program object is created that inherits the configuration
        of the parent.

        The call is made after the child program has been completely set up.

        @param pgm the new (child) QoreProgram object after setup

        @note this function is called with the program parse lock held; use init() for any operations that need to be
        called unlocked
    */
    virtual AbstractQoreProgramExternalData* copy(QoreProgram* pgm) const = 0;

    //! Called after copy() on the new object
    /** The base class implementation is empty

        @since %Qore 1.0
     */
    virtual void init();

    //! For non-reference counted classes, deletes the object immediately
    virtual void doDeref() = 0;
};

typedef std::list<AbstractStatement*> AbstractStatementList_t;
typedef std::list<int> TidList_t;

//! Class implementing breakpoint for debugging
/** Breakpoint is assigned to one or more statements. When such a statement is executed then
 *  breakpoint is probed using @ref checkBreak and program is potentially interrupted and
 *  callback @ref QoreDebugProgram is triggered. The instance must be assigned to a @ref QoreProgram
 */
class QoreBreakpoint : public AbstractPrivateData {
private:
    qore_program_private* pgm;
    AbstractStatementList_t statementList;
    typedef std::map<int/*tid*/, int/*count*/> TidMap_t;
    TidMap_t tidMap;
    QoreObject* qo; // reference to Qore script object, it's private object but we cannot
    static QoreRWLock lck_breakpoint; // to protect breakpoint manipulation
    static QoreBreakpointList_t breakpointList;
    static volatile unsigned breakpointIdCounter;   // to generate breakpointId
    unsigned breakpointId;

    DLLLOCAL void unassignAllStatements();
    DLLLOCAL bool isStatementAssigned(const AbstractStatement *statement) const;
    DLLLOCAL bool checkPgm(ExceptionSink* xsink) const;

    friend class qore_program_private;
    friend class AbstractStatement;

protected:
    DLLLOCAL virtual ~QoreBreakpoint();
    //! check if program flow should be interrupted
    DLLLOCAL virtual bool checkBreak() const;

public:
    bool enabled;
    /** Defines policy how thread list is evaluated. In the case of ACCEPT policy are considered all TIDs in list.
        In case of REJECT policy are considered all TIDs not in the list.
    */
    BreakpointPolicy policy;

    DLLEXPORT QoreBreakpoint();
    /** Copy all props but object reference
    */
    DLLEXPORT QoreBreakpoint& operator=(const QoreBreakpoint& other);
    /** Assign @ref QoreProgram to breakpoint.

        @param new_pgm QoreProgram to be assigned, when NULL then unassigns Program and deletes all statement references
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT void assignProgram(QoreProgram *new_pgm, ExceptionSink* xsink);
    /* Get assigned program to breakpoint
    */
    DLLEXPORT QoreProgram* getProgram() const;
    /** Assign breakpoint to a statement.
    */
    DLLEXPORT void assignStatement(AbstractStatement* statement, ExceptionSink* xsink);
    /** Unassign breakpoint from statement
    */
    DLLEXPORT void unassignStatement(AbstractStatement* statement, ExceptionSink* xsink);
    /** Get list of statements
    */
    DLLEXPORT void getStatements(AbstractStatementList_t &statList, ExceptionSink* xsink);
    /** Get list of statement ids
    */
    DLLEXPORT QoreListNode* getStatementIds(ExceptionSink* xsink);
    /** Resolve statement from statement id
    */
    DLLEXPORT AbstractStatement* resolveStatementId(unsigned long statementId, ExceptionSink* xsink) const;
    /** Get list of the thread IDs
    */
    DLLEXPORT void getThreadIds(TidList_t &tidList, ExceptionSink* xsink);
    /** Set list of the thread IDs
    */
    DLLEXPORT void setThreadIds(TidList_t tidList, ExceptionSink* xsink);
    /** Add thread ID to the list
    */
    DLLEXPORT void addThreadId(int tid, ExceptionSink* xsink);
    /** Remove thread ID from the list
    */
    DLLEXPORT void removeThreadId(int tid, ExceptionSink* xsink);
    /** Check if thread is ID in list
    */
    DLLEXPORT bool isThreadId(int tid, ExceptionSink* xsink);
    /** Clear list of the thread IDs
    */
    DLLEXPORT void clearThreadIds(ExceptionSink* xsink);

    //! get the breakpoint id
    /**
        @return the breakpoint id which is unique number
    */
    DLLEXPORT unsigned getBreakpointId() const;

    //! get the breakpoint from breakpoint id
    /**
        @param breakpointId provided by @ref getBreakpointId

        @return the original breakpoint or null if object cannot be resolved
    */
    DLLEXPORT static QoreBreakpoint* resolveBreakpointId(unsigned breakpointId);

    DLLEXPORT void setQoreObject(QoreObject* n_qo);

    DLLEXPORT QoreObject* getQoreObject();
};

//! allows a program to be used and guarantees that it will stay valid until the destructor is run if successfully acquired in the constructor
/** @since %Qore 0.9
*/
class QoreExternalProgramContextHelper {
public:
    //! try to attach to the given program, if not possible, then a Qore-language exception is thrown
    DLLEXPORT QoreExternalProgramContextHelper(ExceptionSink* xsink, QoreProgram* pgm);

    //! destroys the object and releases the program to be destroyed if it was successfully acquired in the constructor
    DLLEXPORT ~QoreExternalProgramContextHelper();

private:
    class ProgramThreadCountContextHelper* priv;
};

//! allows the program call context to be set by external modules
/** @since %Qore 0.9.4
*/
class QoreExternalProgramCallContextHelper {
public:
    //! sets the call context to the given program
    DLLEXPORT QoreExternalProgramCallContextHelper(QoreProgram* pgm);

    //! resets the call context to the original state
    DLLEXPORT ~QoreExternalProgramCallContextHelper();

private:
    class QoreExternalProgramCallContextHelperPriv* priv;
};

//! Allows binary code to set the current object and class context
class QoreObjectContextHelper {
public:
    DLLEXPORT QoreObjectContextHelper(QoreObject* obj, QoreClass* cls);
    DLLEXPORT ~QoreObjectContextHelper();

private:
    class qore_object_context_helper* priv;

    DLLLOCAL QoreObjectContextHelper(const QoreObjectContextHelper&) = delete;
    DLLLOCAL QoreObjectContextHelper& operator=(const QoreObjectContextHelper&) = delete;
    DLLLOCAL void* operator new(size_t) = delete;
};

#endif  // _QORE_QOREPROGRAM_H
