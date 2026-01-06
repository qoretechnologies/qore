/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  ContinueStatement.h

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

#ifndef _QORE_CONTINUESTATEMENT_H

#define _QORE_CONTINUESTATEMENT_H

#include "qore/intern/AbstractStatement.h"

class ContinueStatement : public AbstractStatement {
public:
    DLLLOCAL ContinueStatement(int start_line, int end_line) : AbstractStatement(start_line, end_line) {
    }

    DLLLOCAL virtual ~ContinueStatement() {
    }

    DLLLOCAL virtual bool endsBlock() const {
        return true;
    }

private:
    DLLLOCAL virtual int execImpl(QoreValue& return_value, ExceptionSink *xsink) {
        return RC_CONTINUE;
    }

    DLLLOCAL virtual int parseInitImpl(QoreParseContext& parse_context) {
        if (!(parse_context.pflag & PF_CONTINUE_OK)) {
            if (!(parse_context.pgm->getParseOptions64() & PO_BROKEN_LOOP_STATEMENT)) {
                parseException(*loc, "CONTINUE-NOT-ALLOWED", "continue statements are only allowed in loop " \
                    "statements");
                return -1;
            }
        }
        return 0;
    }
};

#endif
