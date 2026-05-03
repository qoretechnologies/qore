/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_DebugProgram.h

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

#ifndef _QORE_CLASS_DEBUGPROGRAM_H

#define _QORE_CLASS_DEBUGPROGRAM_H

DLLEXPORT extern qore_classid_t CID_DEBUGPROGRAM;
DLLLOCAL extern QoreClass* QC_DEBUGPROGRAM;
DLLLOCAL QoreClass *initDebugProgramClass(QoreNamespace& ns);

#include <qore/QoreDebugProgram.h>
#include <qore/ReferenceArgumentHelper.h>

// class needed to handle calls from C++ to Qore script instance
class QoreDebugProgramWithQoreObject: public QoreDebugProgram {
private:
   QoreObject* qo;

   DLLLOCAL void callMethod(const char* name, QoreProgram *pgm, int paramCount, QoreValue* params, DebugRunStateEnum &rs, const AbstractStatement* &rts, ExceptionSink* xsink, ExceptionSink &xsink2) {
      ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink2);
      args->push(QoreProgram::getQoreObject(pgm), nullptr);
      for (int i=0; i<paramCount; i++) {
         //printd(5, "QoreDebugProgramWithCoreObject::callMethod(%s) this: %p, param: %d/%d, type: %s\n", name, this, i, paramCount, params[i]?params[i]->getTypeName():"n/a");
         args->push(params[i], nullptr);
      }
      // LocalVar will sanitize and discard non-node values so we cannot use the ReferenceHolder
      ReferenceArgumentHelper rsah(rs, &xsink2);
      args->push(rsah.getArg(), nullptr); // caller owns ref
      int sid = rts ? pgm->getStatementId(rts) : 0;
      ReferenceArgumentHelper rtsah(sid, &xsink2);
      args->push(rtsah.getArg(), nullptr); // caller owns ref
      printd(5, "QoreDebugProgramWithCoreObject::callMethod(%s) this: %p, pgm: %p, param#: %d, rs: %d, rts: %d, xsink2: %d\n", name, this, pgm, paramCount, rs, sid, xsink2.isEvent());
      qo->evalMethod(name, *args, &xsink2);
      QoreValue rsv(rsah.getOutputValue());
      rs = (DebugRunStateEnum) rsv.getAsBigInt();
      rsv.discard(&xsink2);

      QoreValue rtsv(rtsah.getOutputValue());
      sid = rtsv.getAsBigInt();
      rtsv.discard(&xsink2);
      if (sid <= 0) {
         rts = nullptr;
      } else {
         rts = pgm->resolveStatementId(sid);
      }

      //printd(5, "QoreDebugProgramWithCoreObject::callMethod(%s) this: %p, pgm: %p, rs: %d\n", name, this, pgm, rs);
      /* catch all exceptions from debug code, optionally we could assimilate on demand or create exception handler
       * but developer can try/catch by himself to handle it
       */
      // xsink->assimilate(xsink2);
   }

public:
   DLLLOCAL QoreDebugProgramWithQoreObject(QoreObject* n_qo): qo(n_qo) {
      //printd(5, "QoreDebugProgramWithCoreObject::QoreDebugProgramWithCoreObject() this: %p, qo: %p\n", this, n_qo);
   }
   DLLLOCAL virtual void refForCallback() override {
      qo->ref();
   }
   DLLLOCAL virtual void derefForCallback(ExceptionSink* xsink) override {
      qo->deref(xsink);
   }
   DLLLOCAL virtual void onAttach(QoreProgram *pgm, DebugRunStateEnum &rs, const AbstractStatement* &rts, ExceptionSink* xsink) {
      //printd(5, "QoreDebugProgramWithCoreObject::onAttach() this: %p, pgm: %p\n", this, pgm);
      ExceptionSink xsink2;
      callMethod("onAttach", pgm, 0, 0, rs, rts, xsink, xsink2);
   }
   DLLLOCAL virtual void onDetach(QoreProgram *pgm, DebugRunStateEnum &rs, const AbstractStatement* &rts, ExceptionSink* xsink) {
      printd(5, "QoreDebugProgramWithCoreObject::onDetach() this: %p, pgm: %p\n", this, pgm);
      ExceptionSink xsink2;
      callMethod("onDetach", pgm, 0, 0, rs, rts, xsink, xsink2);
   }
   DLLLOCAL virtual void onStep(QoreProgram *pgm, const StatementBlock *blockStatement, const AbstractStatement *statement, unsigned bkptId, int &flow, DebugRunStateEnum &rs, const AbstractStatement* &rts, ExceptionSink* xsink) {
      ExceptionSink xsink2;
      QoreValue params[4];
      params[0] = pgm->getStatementId(blockStatement);
      params[1] = statement ? QoreValue(pgm->getStatementId(statement)) : QoreValue();
      /*
      if (!params[0]->getAsInt())
         printd(5, "QoreDebugProgramWithCoreObject::onStep::blockStatement:%s:%d-%d:%s\n", blockStatement->loc.getFile(), blockStatement->loc.start_line, blockStatement->loc.end_line, typeid(blockStatement).name());
      if (statement && !params[1]->getAsInt()) {
         printd(5, "QoreDebugProgramWithCoreObject::onStep::statement:%s:%d-%d:%s\n", statement->loc.getFile(), statement->loc.start_line, statement->loc.end_line, typeid(statement).name());
      }*/
      params[2] = bkptId > 0 ? QoreValue(bkptId) : QoreValue();
      // LocalVar will sanitize and discard non-node values so we cannot use the ReferenceHolder
      ReferenceArgumentHelper rah(flow, &xsink2);
      params[3] = rah.getArg(); // caller owns ref
      callMethod("onStep", pgm, 4, params, rs, rts, xsink, xsink2);
      QoreValue v(rah.getOutputValue());
      flow = v.getAsBigInt();
      v.discard(&xsink2);
   }
   DLLLOCAL virtual void onFunctionEnter(QoreProgram *pgm, const StatementBlock *blockStatement, DebugRunStateEnum &rs, const AbstractStatement* &rts, ExceptionSink* xsink) {
      ExceptionSink xsink2;
      QoreValue params[1];
      params[0] = pgm->getStatementId(blockStatement);
      /*
      if (!params[0]->getAsInt())
         printd(5, "QoreDebugProgramWithCoreObject::onFunctionEnter::blockStatement:%s:%d-%d:%s\n", blockStatement->loc.getFile(), blockStatement->loc.start_line, blockStatement->loc.end_line, typeid(blockStatement).name());
      */
      callMethod("onFunctionEnter", pgm, 1, params, rs, rts, xsink, xsink2);
   }
   DLLLOCAL virtual void onFunctionExit(QoreProgram *pgm, const StatementBlock *blockStatement, QoreValue& returnValue, DebugRunStateEnum &rs, const AbstractStatement* &rts, ExceptionSink* xsink) {
      ExceptionSink xsink2;
      QoreValue params[2];
      params[0] = pgm->getStatementId(blockStatement);
      /*
      if (!params[0]->getAsInt())
         printd(5, "QoreDebugProgramWithCoreObject::onFunctionExit::blockStatement:%s:%d-%d:%s\n", blockStatement->loc.getFile(), blockStatement->loc.start_line, blockStatement->loc.end_line, typeid(blockStatement).name());
      */
      //printd(5, "QoreDebugProgramWithCoreObject::onFunctionExit() getRetValue#0: type: %d, in: %p\n", returnValue.type, returnValue.getInternalNode());
      ReferenceArgumentHelper rah(returnValue, &xsink2);
      params[1] = rah.getArg(); // caller owns ref
      callMethod("onFunctionExit", pgm, 2, params, rs, rts, xsink, xsink2);
      returnValue = rah.getOutputValue(); // caller owns ref
      //printd(5, "QoreDebugProgramWithCoreObject::onFunctionExit() getRetValue#3: type: %d, in: %p\n", returnValue.type, returnValue.getInternalNode());
   }
   DLLLOCAL virtual void onException(QoreProgram *pgm, const AbstractStatement *statement, DebugRunStateEnum &rs, const AbstractStatement* &rts, ExceptionSink* xsink) {
      ExceptionSink xsink2;
      QoreValue params[3];
      params[0] = pgm->getStatementId(statement);
      /*
      if (!params[0]->getAsInt())
         printd(5, "QoreDebugProgramWithCoreObject::onException::statement:%s:%d-%d:%s\n", statement->loc.getFile(), statement->loc.start_line, statement->loc.end_line, typeid(statement).name());
      */
      QoreException* except = xsink->getException();
      params[1] = except->makeExceptionObject();
      // LocalVar will sanitize and discard non-node values so we cannot use the ReferenceHolder
      ReferenceArgumentHelper rah(QoreValue(false), &xsink2);
      params[2] = rah.getArg(); // caller owns ref
      callMethod("onException", pgm, 3, params, rs, rts, xsink, xsink2);
      QoreValue v(rah.getOutputValue());
      if (v.getAsBool()) {
         xsink->clear();  // dismiss exception
      }
      v.discard(&xsink2);
   }
   DLLLOCAL virtual void onExit(QoreProgram *pgm, const StatementBlock *blockStatement, QoreValue& returnValue, DebugRunStateEnum &rs, const AbstractStatement* &rts, ExceptionSink* xsink) {
      ExceptionSink xsink2;
      QoreValue params[2];
      params[0] = pgm->getStatementId(blockStatement);
      /*
      if (!params[0]->getAsInt())
         printd(5, "QoreDebugProgramWithCoreObject::onExit::blockStatement:%s:%d-%d:%s\n", blockStatement->loc.getFile(), blockStatement->loc.start_line, blockStatement->loc.end_line, typeid(blockStatement).name());
      */
      //printd(5, "QoreDebugProgramWithCoreObject::onExit() getRetValue#0: type: %d, in: %p\n", returnValue.type, returnValue.getInternalNode());
      ReferenceArgumentHelper rah(returnValue, &xsink2);
      params[1] = rah.getArg(); // caller owns ref
      callMethod("onExit", pgm, 2, params, rs, rts, xsink, xsink2);
      returnValue = rah.getOutputValue(); // caller owns ref
      //printd(5, "QoreDebugProgramWithCoreObject::onExit() getRetValue#3: type: %d, in: %p\n", returnValue.type, returnValue.getInternalNode());
   }
};


#endif // _QORE_CLASS_DEBUGPROGRAM_H
