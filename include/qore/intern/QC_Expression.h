/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QC_Expression.h

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

#ifndef _QORE_CLASS_EXPRESSION_H
#define _QORE_CLASS_EXPRESSION_H

#include "qore/intern/qore_program_private.h"

class QoreExpression : public AbstractPrivateData {
public:
    DLLLOCAL QoreExpression(QoreProgram& qore_pgm, const QoreStringNode& source, const QoreStringNode& label, ExceptionSink* xsink)
        : pgm(qore_program_private::get(qore_pgm)) {
        pgm->depRef();
        // the destructor does not run if the constructor throws: the reference is released here then
        try {
            exp = pgm->createExpression(source, label, xsink);
        } catch (...) {
            pgm->depDeref();
            throw;
        }
    }

    DLLLOCAL ~QoreExpression() {
        if (exp) {
            pgm->deleteExpression(exp);
        }
        pgm->depDeref();
    }

    DLLLOCAL QoreValue eval(ExceptionSink* xsink) const {
        return pgm->evalExpression(exp, xsink);
    }

protected:
    qore_program_private* pgm;
    q_exp_t exp;
};

DLLEXPORT extern qore_classid_t CID_EXPRESSION;
DLLEXPORT extern QoreClass* QC_EXPRESSION;
DLLLOCAL QoreClass *initExpressionClass(QoreNamespace& ns);

#endif
