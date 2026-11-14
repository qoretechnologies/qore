/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ReturnStatement.h

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

#ifndef _QORE_RETURNSTATEMENT_H

#define _QORE_RETURNSTATEMENT_H

#include "qore/intern/AbstractStatement.h"

class RuntimeConfig;

class ReturnStatement : public AbstractStatement {
public:
    //! no lexical sub-scope of its own
    DLLLOCAL virtual bool mayHaveNestedScopeLocals(unsigned depth) const {
        return false;
    }
    DLLLOCAL ReturnStatement(int start_line, int end_line, QoreValue v = QoreValue())
            : AbstractStatement(start_line, end_line), exp(v) {
    }

    DLLLOCAL ~ReturnStatement() {
        exp.discard(nullptr);
    }

    DLLLOCAL virtual bool endsBlock() const {
        return true;
    }

    DLLLOCAL virtual bool hasFinalReturn() const {
        return true;
    }

    DLLLOCAL QoreValue getExpression() const {
        return exp;
    }

    //! Replaces only a compatibility return, before parse initialization or debugger registration.
    DLLLOCAL static AbstractStatement* bindDeclarationPolicy(AbstractStatement* statement);

protected:
    QoreValue exp{};

    //! Transfers a parse-only node; the expression's ownership and source provenance are preserved exactly.
    DLLLOCAL explicit ReturnStatement(ReturnStatement& source) : AbstractStatement(source.loc), exp(source.exp) {
        assert(!source.breakpoints && !source.scope_known);
        source.exp.clear();
        pwo = source.pwo;
        semantic_policy = source.semantic_policy;
        semantic_policy_bound = source.semantic_policy_bound;
    }

private:

    DLLLOCAL virtual int execImpl(QoreValue& return_value, ExceptionSink *xsink);
    DLLLOCAL virtual int execImpl(RuntimeConfig& rc, QoreValue& return_value, ExceptionSink* xsink);
    DLLLOCAL virtual int parseInitImpl(QoreParseContext& parse_context);
};

//! Selected during parsing only for a declaration with compatibility return coercion.
class LexicalReturnStatement : public ReturnStatement {
public:
    DLLLOCAL LexicalReturnStatement(ReturnStatement& source, LexicalSemanticPolicy policy)
            : ReturnStatement(source), return_policy(policy) {
    }

private:
    LexicalSemanticPolicy return_policy;
    DLLLOCAL int execImpl(QoreValue& return_value, ExceptionSink* xsink) override;
    DLLLOCAL int execImpl(RuntimeConfig& rc, QoreValue& return_value, ExceptionSink* xsink) override;
};

#endif
