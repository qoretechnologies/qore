/*
  OnBlockExitStatement.cpp

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

#include <qore/Qore.h>
#include "qore/intern/OnBlockExitStatement.h"
#include "qore/intern/StatementBlock.h"

OnBlockExitStatement::OnBlockExitStatement(int start_line, int end_line, StatementBlock* n_code,
        enum obe_type_e n_type) : AbstractStatement(start_line, end_line), code(n_code), type(n_type) {
}

OnBlockExitStatement::~OnBlockExitStatement() {
    delete code;
}

int OnBlockExitStatement::execImpl(RuntimeConfig& rconfig, QoreValue& return_value, ExceptionSink* xsink) {
    // "activate" this block when the block exits in the thread "on block exit" stack
    advance_on_block_exit();
    return 0;
}

int OnBlockExitStatement::parseInitImpl(QoreParseContext& parse_context) {
    if (!code) {
        return 0;
    }

    // turn off top-level flag for statement vars
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_TOP_LEVEL | PF_BREAK_OK | PF_CONTINUE_OK);
    if (type == OBE_Error) {
        fh.setFlags(PF_RETHROW_OK);
    }
    // issue #4961: set implicit arg type
    ParseImplicitArgTypeHelper pia(type == OBE_Error ? hashdeclExceptionInfo->getTypeInfo() : nothingTypeInfo);

    return code->parseInitImpl(parse_context);
}

void OnBlockExitStatement::parseCommit(QoreProgram* pgm) {
    AbstractStatement::parseCommit(pgm);
    if (code) {
        code->parseCommit(pgm);
    }
}
