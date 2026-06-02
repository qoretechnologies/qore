/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    AbstractStatement.h

    Qore Programming Language

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

#ifndef _QORE_ABSTRACTSTATEMENT_H

#define _QORE_ABSTRACTSTATEMENT_H

#include <qore/common.h>
#include "qore/intern/ParseWarnOptions.h"

#define RC_RETURN       1
#define RC_BREAK        2
#define RC_CONTINUE     3

#define PF_RETURN_VALUE_IGNORED  (1 << 0)
#define PF_BACKGROUND            (1 << 1)
#define PF_RETHROW_OK            (1 << 2)
#define PF_FOR_ASSIGNMENT        (1 << 3)
#define PF_CONST_EXPRESSION      (1 << 4)
#define PF_TOP_LEVEL             (1 << 5) //!< parsing at the top-level of the program
#define PF_BREAK_OK              (1 << 6)
#define PF_CONTINUE_OK           (1 << 7)
#define PF_NO_TOP_LEVEL_LVARS    (1 << 8)
#define PF_NARROWED_TYPE         (1 << 9) //!< current expression involves a narrowed auto type

// all definitions in this file are private to the library and subject to change
// forward references
class LVList;
class StatementBlock;
class QoreBreakpoint;
class RuntimeConfig;

// forward declaration
class qore_program_private_base;

class AbstractStatement {
public:
    const QoreProgramLocation* loc;
    struct ParseWarnOptions pwo;

    DLLLOCAL AbstractStatement(qore_program_private_base* p);

    DLLLOCAL AbstractStatement(const QoreProgramLocation* loc);
    DLLLOCAL AbstractStatement(int sline, int eline);
    DLLLOCAL virtual ~AbstractStatement();

    DLLLOCAL int exec(QoreValue& return_value, ExceptionSink* xsink);
    DLLLOCAL int exec(RuntimeConfig& rc, QoreValue& return_value, ExceptionSink* xsink);
    DLLLOCAL int parseInit(QoreParseContext& parse_context);

    DLLLOCAL void finalizeBlock(int sline, int eline);

    DLLLOCAL bool getBreakpointFlag() const {
        return breakpointFlag;
    }

    // statement should return true if it ends a block (break, continue, return, throw, etc)
    // meaning that any subsequent statements will be unconditionally skipped
    DLLLOCAL virtual bool endsBlock() const {
        return false;
    }

    // should return true if the statement is a declaration processed at parse time and should not go into the parse
    // tree
    DLLLOCAL virtual bool isParseDeclaration() const {
        return false;
    }

    // should return true if the statement is a declaration and does not represent an executable statement
    DLLLOCAL virtual bool isDeclaration() const {
        return false;
    }

    DLLLOCAL virtual bool hasFinalReturn() const {
        return false;
    }

    DLLLOCAL virtual void parseCommit(QoreProgram* pgm);

protected:
    QoreBreakpointList_t* breakpoints = nullptr;
    volatile bool breakpointFlag = false;  // fast access to check if breakpoints are non-empty

    DLLLOCAL virtual int execImpl(QoreValue& return_value, ExceptionSink* xsink) = 0;
    DLLLOCAL virtual int execImpl(RuntimeConfig& rc, QoreValue& return_value, ExceptionSink* xsink);

    //! Returns -1 = parse error raised, 0 = OK
    DLLLOCAL virtual int parseInitImpl(QoreParseContext& parse_context) = 0;

    friend class qore_program_private;
    // executed when qore_program_private::lck_breakpoint lock is acquired
    DLLLOCAL QoreBreakpoint* getBreakpoint() const;

    friend class QoreBreakpoint;
    DLLLOCAL void assignBreakpoint(QoreBreakpoint *bkpt);
    DLLLOCAL void unassignBreakpoint(QoreBreakpoint *bkpt);
};

DLLLOCAL void push_cvar(const char* name);
DLLLOCAL void pop_cvar();
DLLLOCAL LocalVar* pop_local_var(bool set_unassigned = false);
DLLLOCAL int pop_local_var_get_id();
// used for constructor methods sharing a common "self" local variable and for top-level local variables
DLLLOCAL void push_local_var(LocalVar* lv, const QoreProgramLocation* loc);

// push a local variable on the stack at parse time
/** @param name the name of the var
    @param loc the location of the declaration
    @param typeInfo the declared type of the variable
    @param is_auto true if it's an automatic variable (already assigned - ex: parameter var or "self"/"argv")
    @param n_refs reference count
    @param pflag parse flags

    @return the LocalVar ptr (caller owns the pointer returned)
*/
DLLLOCAL LocalVar* push_local_var(const char* name, const QoreProgramLocation* loc, const QoreTypeInfo* typeInfo,
        int& err, bool is_auto = true, int n_refs = 0, int pflag = 0);

DLLLOCAL LocalVar* find_local_var(const char* name, bool &in_closure);

#endif // _QORE_ABSTRACTSTATEMENT_H
