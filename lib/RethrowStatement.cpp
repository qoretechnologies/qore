/*
  RethrowStatement.cpp

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
#include "qore/intern/RethrowStatement.h"
#include "qore/intern/QoreException.h"

int RethrowStatement::execImpl(QoreValue& return_value, ExceptionSink* xsink) {
    if (args) {
        ValueEvalOptimizedRefHolder v(args, xsink);
        if (!*xsink) {
            QoreException* ex = catch_get_exception()->replaceTop(*v->get<QoreListNode>(), *xsink);
            qore_es_private::get(*xsink)->rethrow(ex);
        }
    } else {
        qore_es_private::get(*xsink)->rethrow(catch_get_exception());
    }
    return 0;
}

int RethrowStatement::parseInitImpl(QoreParseContext& parse_context) {
    int err;
    if (args) {
        // turn off top-level flag for statement vars
        QoreParseContextFlagHelper fh(parse_context);
        fh.unsetFlags(PF_TOP_LEVEL);

        parse_context.typeInfo = nullptr;
        err = parse_init_value(args, parse_context);

        switch (args.getType()) {
            case NT_LIST:
            case NT_PARSE_LIST:
                break;
            default: {
                //printd(5, "ThrowStatement::parseInitImpl() v: %p '%s' e: %d\n", args, get_type_name(args),
                //  args->needs_eval());
                QoreListNode* l = new QoreListNode(args.needsEval());
                l->push(args, nullptr);
                args = l;
                break;
            }
        }
    } else {
        err = 0;
    }
    if (!(parse_context.pflag & PF_RETHROW_OK)) {
        parseException(*loc, "RETHROW-NOT-IN-CATCH-BLOCK", "rethrow statements are only allowed in catch blocks");
        return -1;
    }
    return err;
}
