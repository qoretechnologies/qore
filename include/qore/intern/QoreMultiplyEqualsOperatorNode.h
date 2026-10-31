/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreMultiplyEqualsOperatorNode.h

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

#ifndef _QORE_QOREMULTIPLYEQUALSOPERATORNODE_H
#define _QORE_QOREMULTIPLYEQUALSOPERATORNODE_H

class QoreMultiplyEqualsOperatorNode : public QoreBinaryLValueOperatorNode {
OP_COMMON
protected:
    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

    //! how the operator treats an lvalue type at parse time
    enum class lvalue_e {
        //! the lvalue keeps its type
        KEEP,
        //! the lvalue is assigned a number value, which its type must accept
        NUMBER,
        //! the lvalue is assigned a float value, which its type must accept
        FLOAT,
        //! the lvalue is assigned an integer value, which its type must accept
        INT,
        //! the type is not known well enough; the operation is decided at runtime
        GENERIC,
    };

    //! returns how the operator treats the given lvalue type with an operand of the given type
    DLLLOCAL static lvalue_e getLValueKind(const QoreTypeInfo* lti, const QoreTypeInfo* rti) {
        if (QoreTypeInfo::isType(lti, NT_NUMBER)) {
            return lvalue_e::KEEP;
        }
        if (QoreTypeInfo::isType(rti, NT_NUMBER)) {
            return lvalue_e::NUMBER;
        }
        if (QoreTypeInfo::isType(lti, NT_FLOAT)) {
            return lvalue_e::KEEP;
        }
        if (QoreTypeInfo::isType(rti, NT_FLOAT)) {
            return lvalue_e::FLOAT;
        }
        if (QoreTypeInfo::returnsSingle(lti)) {
            return lvalue_e::INT;
        }
        return lvalue_e::GENERIC;
    }

public:
    DLLLOCAL QoreMultiplyEqualsOperatorNode(const QoreProgramLocation* loc, QoreValue left, QoreValue right)
            : QoreBinaryLValueOperatorNode(loc, left, right) {
    }

    DLLLOCAL virtual QoreOperatorNode* copyBackground(ExceptionSink *xsink) const {
        return copyBackgroundExplicit<QoreMultiplyEqualsOperatorNode>(xsink);
    }

    DLLLOCAL int parseInitIntern(const char* name, QoreParseContext& parse_context) {
        assert(!parse_context.typeInfo);
        int err;
        {
            QoreParseContextFlagHelper fh(parse_context);
            fh.setFlags(PF_FOR_ASSIGNMENT);
            err = parse_init_value(left, parse_context);
            ti = parse_context.typeInfo;
        }
        if (!err) {
            err = checkLValue(left, parse_context.pflag);
        }

        parse_context.typeInfo = nullptr;
        if (parse_init_value(right, parse_context) && !err) {
            err = -1;
        }
        const QoreTypeInfo* rightTypeInfo = parse_context.typeInfo;

        // the parse-time checks are made on the plain type T of an or-nothing lvalue (*T): an operation that T
        // refuses can never succeed on *T; the error names the declared type, and the result type and the runtime
        // operation follow the declared type
        const QoreTypeInfo* plain_ti = get_or_nothing_base_type(ti);
        lvalue_e kind = getLValueKind(ti, rightTypeInfo);
        lvalue_e check_kind = plain_ti == ti ? kind : getLValueKind(plain_ti, rightTypeInfo);

        switch (check_kind) {
            case lvalue_e::NUMBER:
                if (check_lvalue_number(loc, ti, name) && !err) {
                    err = -1;
                }
                break;
            case lvalue_e::FLOAT:
                if (check_lvalue_float(loc, ti, name) && !err) {
                    err = -1;
                }
                break;
            case lvalue_e::INT:
                if (check_lvalue_int(loc, ti, name) && !err) {
                    err = -1;
                }
                break;
            case lvalue_e::KEEP:
            case lvalue_e::GENERIC:
                break;
        }

        switch (kind) {
            case lvalue_e::NUMBER:
                ti = numberTypeInfo;
                break;
            case lvalue_e::FLOAT:
                ti = floatTypeInfo;
                break;
            case lvalue_e::INT:
                ti = bigIntTypeInfo;
                break;
            case lvalue_e::GENERIC:
                ti = nullptr;
                break;
            case lvalue_e::KEEP:
                break;
        }

        parse_context.typeInfo = ti;
        // the analysis left by the right operand describes that operand, not the result
        qore_set_result_parse_analysis(parse_context, ti);
        return err;
    }
};

#endif
