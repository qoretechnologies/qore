/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    Restrictions.h

    QORE programming language

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

#ifndef _QORE_RESTRICTIONS_H

#define _QORE_RESTRICTIONS_H

/** @file Restrictions.h
    defines parse restrictions and functional domains for builtin functions and qore class methods
*/

#define PO_NO_GLOBAL_VARS                   (1 <<  0)    //!< cannot define new global variables
#define PO_NO_SUBROUTINE_DEFS               (1 <<  1)    //!< cannot define new user subroutines
#define PO_NO_THREAD_CONTROL                (1 <<  2)    //!< cannot launch new threads) use thread_exit) or access thread data
#define PO_NO_THREAD_CLASSES                (1 <<  3)    //!< no access to thread classes
#define PO_NO_TOP_LEVEL_STATEMENTS          (1 <<  4)    //!< cannot define new top-level statements (outside of sub or class defs)
#define PO_NO_CLASS_DEFS                    (1 <<  5)    //!< cannot define new object classes
#define PO_NO_NAMESPACE_DEFS                (1 <<  6)    //!< cannot define new namespaces
#define PO_NO_CONSTANT_DEFS                 (1 <<  7)    //!< cannot define new constants
#define PO_NO_NEW                           (1 <<  8)    //!< cannot use the new operator (DEPRECATED: this option is not useful anymore because objects can be declared and created without "new")
#define PO_NO_INHERIT_SYSTEM_CLASSES        (1 <<  9)    //!< do not inherit system classes into this program space
#define PO_NO_INHERIT_USER_CLASSES          (1 << 10)    //!< do not inherit public user classes into this program space
#define PO_NO_CHILD_PO_RESTRICTIONS         (1 << 11)    //!< turn off parse option inheritance restrictions
#define PO_NO_EXTERNAL_PROCESS              (1 << 12)    //!< do not allow access to functionality that calls external processes: backquote op, system(), exec(), etc
#define PO_REQUIRE_OUR                      (1 << 13)    //!< require "our" for global var declaration
#define PO_NO_PROCESS_CONTROL               (1 << 14)    //!< do not allow access to functionality that can affect the current process: fork(), exec(), abort(), etc
#define PO_NO_NETWORK                       (1 << 15)    //!< do not allow any network access (objs & subroutines)
#define PO_NO_FILESYSTEM                    (1 << 16)    //!< do not allow any filesystem access (objects & subroutines)
#define PO_LOCK_WARNINGS                    (1 << 17)    //!< do not allow programs to change the warning mask
#define PO_NO_DATABASE                      (1 << 18)    //!< do not allow database access
#define PO_NO_GUI                           (1 << 19)    //!< do not allow any GUI-relevant actions to be performed
#define PO_NO_TERMINAL_IO                   (1 << 20)    //!< do not allow any terminal I/O to be performed
#define PO_REQUIRE_TYPES                    (1 << 21)    //!< require type information for all declarations
#define PO_NO_EXTERNAL_INFO                 (1 << 22)    //!< do not allow any access to host, process, etc information
#define PO_NO_THREAD_INFO                   (1 << 23)    //!< do not allow any access to thread information
#define PO_NO_LOCALE_CONTROL                (1 << 24)    //!< do not allow changes to program locale
#define PO_REQUIRE_PROTOTYPES               (1 << 25)    //!< require types in method and function declarations
#define PO_STRICT_ARGS                      (1 << 26)    //!< do not allow access to RT_NOOP code or excess args
#define PO_REQUIRE_BARE_REFS                (1 << 27)    //!< do not allow '$' for vars and '$.' for class member refs
#define PO_ASSUME_LOCAL                     (1 << 28)    //!< assume local variable scope if not declared (implicit "my")
#define PO_NO_MODULES                       (1 << 29)    //!< do not allow external modules to be loaded
#define PO_NO_INHERIT_USER_FUNC_VARIANTS    (1 << 30)    //!< do not inherit public user function variants from the parent into the new program's space
#define PO_NO_INHERIT_SYSTEM_FUNC_VARIANTS  (1LL << 31)  //!< do not inherit any builtin function variants to the new program's space
#define PO_NO_INHERIT_GLOBAL_VARS           (1LL << 32)  //!< do not inherit public global variables from the parent into the new program's space
#define PO_IN_MODULE                        (1LL << 33)  //!< do not use directly, this is set automatically in user module programs
#define PO_NO_EMBEDDED_LOGIC                (1LL << 34)  //!< do not allow embedded logic or runtime parsing
#define PO_STRICT_BOOLEAN_EVAL              (1LL << 35)  //!< do non-intuitive strict mathematical boolean evaluations (the Qore default prior to v0.8.6)
#define PO_ALLOW_INJECTION                  (1LL << 36)  //!< allow code injection
#define PO_NO_INHERIT_USER_CONSTANTS        (1LL << 37)  //!< do not inherit user constants from the parent into the new program's space
#define PO_NO_INHERIT_SYSTEM_CONSTANTS      (1LL << 38)  //!< do not inherit system constants from the parent into the new program's space
#define PO_BROKEN_LIST_PARSING              (1LL << 39)  //!< allow for old pre-%Qore 0.8.12 broken list rewriting in the parser
#define PO_BROKEN_LOGIC_PRECEDENCE          (1LL << 40)  //!< allow for old pre-%Qore 0.8.12 precedence of logical and bitwise operators
#define PO_BROKEN_INT_ASSIGNMENTS           (1LL << 41)  //!< allow for old pre-%Qore 0.8.12 "int" restrictions to be ignored at runtime
#define PO_BROKEN_OPERATORS                 (1LL << 42)  //!< allow for old pre-%Qore 0.8.12 parsing of multi-character operators with spaces
#define PO_BROKEN_LOOP_STATEMENT            (1LL << 43)  //!< allow for old pre-%Qore 0.8.13 handling of break and continue
#define PO_STRONG_ENCAPSULATION             (1LL << 44)  //!< disallow out-of-line class and namespace declarations
#define PO_NO_UNCONTROLLED_APIS             (1LL << 45)  //!< disallow access to "uncontrolled APIs" like external language bindings or direct generic system call APIs that could bypass sandboxing controls
#define PO_NO_DEBUGGING                     (1LL << 46)  //!< disallows debugging actions that could be insecure such as reading the thread local variable stack
#define PO_BROKEN_REFERENCES                (1LL << 47)  //!< allow for old pre-%Qore 0.8.13 "reference" and "*reference" type restriction behavior where they accepted any type
#define PO_NO_INHERIT_USER_HASHDECLS        (1LL << 48)  //!< do not inherit user hashdecls from the parent into the new program's space
#define PO_NO_INHERIT_SYSTEM_HASHDECLS      (1LL << 49)  //!< do not inherit system hashdecls from the parent into the new program's space
#define PO_ALLOW_WEAK_REFERENCES            (1LL << 50)  //!< allow the use of the weak reference assignment operator ':='
#define PO_ALLOW_DEBUGGER                   (1LL << 51)  //!< allow the use debugger stuff
#define PO_ALLOW_STATEMENT_NO_EFFECT        (1LL << 52)  //!< allow for old pre-%Qore 0.9 top-level statements having no effect
#define PO_NO_REFLECTION                    (1LL << 53)  //!< disallow the use of reflection
#define PO_NO_TRANSIENT                     (1LL << 54)  //!< disables the transient keyword
#define PO_BROKEN_SPRINTF                   (1LL << 55)  //!< enables pre-0.9 broken sprintf handling
#define PO_BROKEN_CAST                      (1LL << 56)  //!< enables pre-0.9.4 broken cast<> operator handling
#define PO_ALLOW_RETURNS                    (1LL << 57)  //!< allows the use of the deprecated "returns" keyword
#define PO_STRICT_TYPES                     (1LL << 58)  //!< enforce strict type checking and setting default values
#define PO_BROKEN_RANGE                     (1LL << 59)  //!< allow for old pre-%Qore 0.9.5 "range()" and "xrange()" behavior where the upper limit was included in the result
#define PO_NO_INHERIT_PROGRAM_DATA          (1LL << 60)  //!< do not inherit module-specific Program data from the parent
#define PO_BROKEN_VARARGS                   (1LL << 61)  //!< allow for old pre-%Qore 1.17 vararg handling with implicit ellipses
#define PO_BROKEN_LIST_RANGE                (1LL << 62)  //!< allow for old pre-%Qore 2.0 list range behavior
#define PO_ENABLE_DEBUG                     (1LL << 63)  //!< enable \@assert and \@debug statements (otherwise ignored at parse time)

// aliases for old defines
#define PO_NO_SYSTEM_FUNC_VARIANTS          PO_NO_INHERIT_SYSTEM_FUNC_VARIANTS
#define PO_NO_SYSTEM_CLASSES                PO_NO_INHERIT_SYSTEM_CLASSES
#define PO_NO_USER_CLASSES                  PO_NO_INHERIT_USER_CLASSES

#define PO_DEFAULT 0            //!< no parse options set by default

//! all options that are set by the system
#define PO_SYSTEM_OPS (PO_IN_MODULE)

//! synonym for PO_REQUIRE_BARE_REFS
#define PO_ALLOW_BARE_REFS PO_REQUIRE_BARE_REFS

// combination options
//! cannot access any thread functionality
#define PO_NO_THREADS                 (PO_NO_THREAD_CONTROL|PO_NO_THREAD_CLASSES|PO_NO_THREAD_INFO)

//! prohibits any external access
#define PO_NO_EXTERNAL_ACCESS         (PO_NO_PROCESS_CONTROL|PO_NO_NETWORK|PO_NO_FILESYSTEM|PO_NO_DATABASE \
    |PO_NO_EXTERNAL_INFO|PO_NO_EXTERNAL_PROCESS|PO_NO_MODULES|PO_NO_UNCONTROLLED_APIS)

//! prohibits all terminal and file I/O and GUI operations
#define PO_NO_IO                      (PO_NO_GUI|PO_NO_TERMINAL_IO|PO_NO_FILESYSTEM|PO_NO_NETWORK|PO_NO_DATABASE \
    |PO_NO_UNCONTROLLED_APIS)

//! most restrictive access - can just execute logic, no I/O, no threading, no external access, no reflection
#define PO_LOCKDOWN                   (PO_NO_EXTERNAL_ACCESS|PO_NO_THREADS|PO_NO_IO|PO_NO_REFLECTION)

//! new Qore style: no more '$' and with assumed variable scope
#define PO_NEW_STYLE                  (PO_ALLOW_BARE_REFS|PO_ASSUME_LOCAL)

//! mask of all options allowing for more freedom (instead of less)
#define PO_POSITIVE_OPTIONS           (PO_NO_CHILD_PO_RESTRICTIONS|PO_ALLOW_INJECTION|PO_ALLOW_WEAK_REFERENCES \
    |PO_ALLOW_DEBUGGER|PO_ENABLE_DEBUG)

//! mask of options that have no effect on code access or code safety
#define PO_FREE_OPTIONS               (PO_ALLOW_BARE_REFS|PO_ASSUME_LOCAL|PO_STRICT_BOOLEAN_EVAL \
    |PO_BROKEN_LIST_PARSING|PO_BROKEN_LOGIC_PRECEDENCE|PO_BROKEN_INT_ASSIGNMENTS|PO_BROKEN_OPERATORS \
    |PO_BROKEN_LOOP_STATEMENT|PO_BROKEN_REFERENCES|PO_BROKEN_SPRINTF|PO_BROKEN_RANGE|PO_BROKEN_VARARGS \
    |PO_BROKEN_LIST_RANGE)

//! mask of options related to style but not capabilities
#define PO_STYLE_OPTIONS              (PO_NO_TOP_LEVEL_STATEMENTS|PO_REQUIRE_PROTOTYPES|PO_REQUIRE_TYPES \
    |PO_STRICT_ARGS)

//! mask of free and style options
#define PO_FREE_STYLE_OPTIONS         (PO_FREE_OPTIONS|PO_STYLE_OPTIONS)

//! mask of options that affect the way a child Program inherits user code from the parent
#define PO_USER_INHERITANCE_OPTIONS   (PO_NO_INHERIT_USER_CLASSES|PO_NO_INHERIT_USER_FUNC_VARIANTS \
    |PO_NO_INHERIT_GLOBAL_VARS|PO_NO_INHERIT_USER_CONSTANTS|PO_NO_INHERIT_USER_HASHDECLS)

//! mask of options that affect the way a child Program inherits user code from the parent
#define PO_SYSTEM_INHERITANCE_OPTIONS (PO_NO_INHERIT_SYSTEM_CLASSES|PO_NO_INHERIT_SYSTEM_FUNC_VARIANTS \
    |PO_NO_INHERIT_SYSTEM_CONSTANTS|PO_NO_INHERIT_SYSTEM_HASHDECLS|PO_NO_INHERIT_PROGRAM_DATA)

//! mask of options that affect the way a child Program inherits code from the parent
#define PO_INHERITANCE_OPTIONS        (PO_USER_INHERITANCE_OPTIONS|PO_SYSTEM_INHERITANCE_OPTIONS)

//! an alias of PO_USER_INHERITANCE_OPTIONS
#define PO_NO_USER_API PO_USER_INHERITANCE_OPTIONS

//! an alias of PO_SYSTEM_INHERITANCE_OPTIONS
#define PO_NO_SYSTEM_API PO_SYSTEM_INHERITANCE_OPTIONS

//! an alias of PO_INHERITANCE_OPTIONS
#define PO_NO_API               PO_INHERITANCE_OPTIONS

#define QDOM_DEFAULT            0                         //!< the default domain (no domain)
#define QDOM_PROCESS            PO_NO_PROCESS_CONTROL     //!< provides process control functionality (can affect or stop the current process)
#define QDOM_NETWORK            PO_NO_NETWORK             //!< provides network functionality
#define QDOM_EXTERNAL_PROCESS   PO_NO_EXTERNAL_PROCESS    //!< provides external process control functionality (can affect) start) or stop external processes)
#define QDOM_FILESYSTEM         PO_NO_FILESYSTEM          //!< provides access to the filesystem
#define QDOM_THREAD_CLASS       PO_NO_THREAD_CLASSES      //!< provides thread control functionality
#define QDOM_THREAD_CONTROL     PO_NO_THREAD_CONTROL      //!< provides the ability to check or manipulate threads (including starting new threads)
#define QDOM_DATABASE           PO_NO_DATABASE            //!< provides access to databases
#define QDOM_GUI                PO_NO_GUI                 //!< provides GUI functionality
#define QDOM_TERMINAL_IO        PO_NO_TERMINAL_IO         //!< provides terminal I/O functionality
#define QDOM_EXTERNAL_INFO      PO_NO_EXTERNAL_INFO       //!< provides access to external information (ex: hostname, pid, process uid, etc)
#define QDOM_THREAD_INFO        PO_NO_THREAD_INFO         //!< provides access to information regarding threading (tid, active threads, etc)
#define QDOM_LOCALE_CONTROL     PO_NO_LOCALE_CONTROL      //!< provides access to functionality that changes locale information
#define QDOM_MODULES            PO_NO_MODULES             //!< provides access to external modules
#define QDOM_IN_MODULE          PO_IN_MODULE              //!< tagged with code that is restricted in user modules
#define QDOM_EMBEDDED_LOGIC     PO_NO_EMBEDDED_LOGIC      //!< provides dynamic parsing functionality
#define QDOM_INJECTION          PO_ALLOW_INJECTION        //!< provides functionality related to code / dependency injection
#define QDOM_UNCONTROLLED_API   PO_NO_UNCONTROLLED_APIS   //!< provides unchecked access to system functionality that could bypass Qore's sandboxing controls
#define QDOM_DEBUGGER           PO_ALLOW_DEBUGGER         //!< provides debugger functionality
#define QDOM_REFLECTION         PO_NO_REFLECTION          //!< provides reflection functionality

#endif //_QORE_DOMAIN_H
