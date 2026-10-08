/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreBinaryLValueOperatorNode.h

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

#ifndef _QORE_QOREBINARYLVALUEOPERATORNODE_H

#define _QORE_QOREBINARYLVALUEOPERATORNODE_H

class QoreBinaryLValueOperatorNode : public QoreBinaryOperatorNode<LValueOperatorNode> {
protected:
    DLLLOCAL virtual const QoreTypeInfo *getTypeInfo() const {
        return ti;
    }

public:
    const QoreTypeInfo *ti; // typeinfo of lhs

    DLLLOCAL QoreBinaryLValueOperatorNode(const QoreProgramLocation* loc, QoreValue left, QoreValue right)
            : QoreBinaryOperatorNode<LValueOperatorNode>(loc, left, right), ti(0) {
    }

    DLLLOCAL virtual bool hasEffect() const {
        return true;
    }
};

// for operators that try to change the lvalue to an int
class QoreBinaryIntLValueOperatorNode : public QoreBinaryOperatorNode<LValueOperatorNode> {
protected:
    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return bigIntTypeInfo;
    }

public:
    DLLLOCAL QoreBinaryIntLValueOperatorNode(const QoreProgramLocation* loc, QoreValue left, QoreValue right)
            : QoreBinaryOperatorNode<LValueOperatorNode>(loc, left, right) {
    }

    DLLLOCAL int parseInitIntLValue(const char* name, QoreParseContext& parse_context) {
        // turn off "reference ok" and "return value ignored" flags
        QoreParseContextFlagHelper fh(parse_context);
        parse_context.unsetFlags(PF_RETURN_VALUE_IGNORED);

        parse_context.typeInfo = nullptr;
        int err;
        {
            QoreParseContextFlagHelper fh0(parse_context);
            fh0.setFlags(PF_FOR_ASSIGNMENT);
            err = parse_init_value(left, parse_context);
        }
        if (checkLValue(left, parse_context.pflag) && !err) {
            err = -1;
        }

        // make sure left side can take an integer value
        if (check_lvalue_int(loc, parse_context.typeInfo, name) && !err) {
            err = -1;
        }

        parse_context.typeInfo = nullptr;
        // FIXME: check for invalid operation - type cannot be converted to integer
        if (parse_init_value(right, parse_context) && !err) {
            err = -1;
        }

        parse_context.typeInfo = bigIntTypeInfo;
        // the analysis left by the right operand describes that operand, not the result
        qore_set_result_parse_analysis(parse_context, bigIntTypeInfo);
        return err;
    }

    DLLLOCAL virtual bool hasEffect() const {
        return true;
    }
};
#endif
