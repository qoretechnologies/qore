/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QoreDebugProgram.h

  Program Debug Object Definition

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

#ifndef INCLUDE_QORE_QOREDEBUGPROGRAM_H_
#define INCLUDE_QORE_QOREDEBUGPROGRAM_H_

#include <qore/AbstractPrivateData.h>
#include <qore/Restrictions.h>
#include <qore/support.h>

class QoreProgram;
class ExceptionSink;
class AbstractStatement;
class StatementBlock;
class QoreBreakpoint;
class qore_program_private;
class qore_debug_program_private;

enum DebugRunStateEnum : unsigned char {
   DBG_RS_DETACH = 0,
   DBG_RS_RUN = 1,
   DBG_RS_STEP = 2,
   DBG_RS_STEP_OVER = 3,
   DBG_RS_UNTIL_RETURN = 4,
   DBG_RS_STOPPED = 5,    // last one, see assert in setRunState
};

//! supports parsing and executing Qore-language code, reference counted, dynamically-allocated only
/** This class implements a transaction and thread-safe container for qore-language code
    This class implements two-layered reference counting to address problems with circular references.
    When a program has a global variable that contains an object that references the program...
    objects now reference the dependency counter, so when the object's counter reaches zero and
    the global variable list is deleted, then the variables will in turn dereference the program
    so it can be deleted.
*/
class QoreDebugProgram : public AbstractPrivateData {
   friend class qore_program_private;
   friend class qore_debug_program_private;
private:
   //! private implementation
   qore_debug_program_private* priv;

   //! this function is not implemented; it is here as a private function in order to prohibit it from being used
   DLLLOCAL QoreDebugProgram(const QoreDebugProgram&);

   //! this function is not implemented; it is here as a private function in order to prohibit it from being used
   DLLLOCAL QoreDebugProgram& operator=(const QoreDebugProgram&);

protected:
   //! the destructor is private in order to prohibit the object from being allocated on the stack
   /** the destructor will wait till all debug calls are terminated
    */
   DLLLOCAL virtual ~QoreDebugProgram();

public:
   //! creates the object
   DLLEXPORT QoreDebugProgram();

   DLLLOCAL virtual void refForCallback();

   DLLLOCAL virtual void derefForCallback(ExceptionSink* xsink);

   DLLEXPORT void addProgram(QoreProgram *pgm, ExceptionSink* xsink);
   DLLEXPORT void removeProgram(QoreProgram *pgm);
   DLLEXPORT QoreListNode* getAllProgramObjects();

   DLLEXPORT virtual void onAttach(QoreProgram *pgm, DebugRunStateEnum &rs, const AbstractStatement* &rts, ExceptionSink* xsink);

   DLLEXPORT virtual void onDetach(QoreProgram *pgm, DebugRunStateEnum &rs, const AbstractStatement* &rts, ExceptionSink* xsink);

   /**
    * Executed on every step of StatementBlock.
    * @param pgm
    * @param blockStatement
    * @param bkptId breakpoint id if hit otherwise 0
    * @param statement current AbstractStatement of blockStatement being processed. Executed also when blockStatement is entered with value of NULL
    * @param flow loop flow
    * @param rs run state
    * @param rts "run to" statement
    * @param xsink if an error occurs, the Qore-language exception information will be added here
    */
   DLLEXPORT virtual void onStep(QoreProgram *pgm, const StatementBlock *blockStatement, const AbstractStatement *statement, unsigned bkptId, int &flow, DebugRunStateEnum &rs, const AbstractStatement* &rts, ExceptionSink* xsink);

   /**
    * Executed when a function is entered.
    */
   DLLEXPORT virtual void onFunctionEnter(QoreProgram *pgm, const StatementBlock *statement, DebugRunStateEnum &rs, const AbstractStatement* &rts, ExceptionSink* xsink);

   /**
    * Executed when a function is exited.
    */
   DLLEXPORT virtual void onFunctionExit(QoreProgram *pgm, const StatementBlock *statement, QoreValue& returnValue, DebugRunStateEnum &rs, const AbstractStatement* &rts, ExceptionSink* xsink);

   /**
    * Executed when an exception is raised.
    */
   DLLEXPORT virtual void onException(QoreProgram *pgm, const AbstractStatement *statement, DebugRunStateEnum &rs, const AbstractStatement* &rts, ExceptionSink* xsink);

   /**
    * Executed when a thread/program is exited.
    */
   DLLEXPORT virtual void onExit(QoreProgram *pgm, const StatementBlock *statement, QoreValue& returnValue, DebugRunStateEnum &rs, const AbstractStatement* &rts, ExceptionSink* xsink);

   /**
    * Break specific program thread
    * @return 0 if the operation was successful, -1 if the target program does not allow debugging, -2 if xxx
    */
   DLLEXPORT int breakProgramThread(QoreProgram *pgm, int tid) const;

   /**
    * Break program, i.e. all threads
    * @return 0 if the operation was successful, -1 if the target program does not allow debugging, -2 if xxx
    */
   DLLEXPORT int breakProgram(QoreProgram *pgm) const;

   //! remove programs and wait till the program's last thread terminates
   /**
      @param xsink if an error occurs, the Qore-language exception information will be added here
   */
   DLLEXPORT void waitForTerminationAndClear(ExceptionSink* xsink);

   //! get number of program currently interrupted
   /**
   */
   DLLEXPORT int getInterruptedCount();
};

#endif /* INCLUDE_QORE_QOREDEBUGPROGRAM_H_ */
