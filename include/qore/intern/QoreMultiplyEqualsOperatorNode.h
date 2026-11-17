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

class LValueHelper;

//! *= with a date lvalue or operand: assigns the product that the * operator gives, a relative date (qore#6109)
/** @param v the lvalue
    @param rhs the operand
    @param xsink for an \c INVALID-DATE exception if the product is out of range, and for an exception if the
    lvalue's type does not accept a date

    @return 0 for success, -1 if an exception was raised
*/
DLLLOCAL int qore_date_multiply_equals(LValueHelper& v, const QoreValue& rhs, ExceptionSink* xsink);

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
        //! *= with a date operand: the lvalue is assigned a date value, which its type must accept; the result
        //! type is decided at runtime (a softint lvalue converts the date)
        DATE,
        //! /= on a date lvalue: the lvalue is assigned the quotient, an integer, float or number value and never a
        //! date, which its type must accept; the result has the lvalue's type (a softdate lvalue converts it)
        DATE_QUOTIENT,
        //! the type is not known well enough; the operation is decided at runtime
        GENERIC,
    };

    //! returns how the operator treats the given lvalue type with an operand of the given type
    /** @param divide true for /=, false for *=
    */
    DLLLOCAL static lvalue_e getLValueKind(const QoreTypeInfo* lti, const QoreTypeInfo* rti, bool divide) {
        // a value of a type parameter has its bound, or a type known only at run time (qore#5774)
        if (const QoreTypeInfo* src = QoreTypeInfo::getTypeParameterSourceType(lti)) {
            lti = src;
        }
        if (const QoreTypeInfo* src = QoreTypeInfo::getTypeParameterSourceType(rti)) {
            rti = src;
        }
        // a date operand of *= gives a relative date, as the * operator does (qore#6109): a date lvalue keeps its
        // type, and the product is assigned to any other lvalue, whose type must accept a date; /= has no date case:
        // the quotient of a date lvalue is an integer, float or number value (qore#6103)
        if (QoreTypeInfo::isType(lti, NT_DATE)) {
            return divide ? lvalue_e::DATE_QUOTIENT : lvalue_e::KEEP;
        }
        if (QoreTypeInfo::isType(rti, NT_DATE)) {
            return divide ? lvalue_e::GENERIC : lvalue_e::DATE;
        }
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

    //! raises a parse error if the lvalue's type accepts none of the values that /= can give a date lvalue
    /** the quotient is a number for a number operand, a float for a float operand, an integer for an operand of
        another single type, and one of these for an operand whose type is not known (qore#6103)
    */
    DLLLOCAL int checkDateQuotient(const QoreTypeInfo* rti, const char* name) const {
        if (const QoreTypeInfo* src = QoreTypeInfo::getTypeParameterSourceType(rti)) {
            rti = src;
        }
        if (QoreTypeInfo::isType(rti, NT_NUMBER)) {
            return check_lvalue_accepts_result(loc, ti, name, {numberTypeInfo}, "a number value");
        }
        if (QoreTypeInfo::isType(rti, NT_FLOAT)) {
            return check_lvalue_accepts_result(loc, ti, name, {floatTypeInfo}, "a float value");
        }
        if (QoreTypeInfo::returnsSingle(rti)) {
            return check_lvalue_accepts_result(loc, ti, name, {bigIntTypeInfo}, "an integer value");
        }
        return check_lvalue_accepts_result(loc, ti, name, {bigIntTypeInfo, floatTypeInfo, numberTypeInfo},
            "an integer, float, or number value");
    }

    //! raises a parse error if the lvalue's type accepts none of the values that the operator can assign it
    /** for an lvalue whose type is not a single type (ex: a union): /= assigns an integer, float or number value,
        and *= one of these or a date (qore#6103)
    */
    DLLLOCAL int checkGenericResult(bool divide, const char* name) const {
        if (divide) {
            return check_lvalue_accepts_result(loc, ti, name, {bigIntTypeInfo, floatTypeInfo, numberTypeInfo},
                "an integer, float, or number value");
        }
        return check_lvalue_accepts_result(loc, ti, name,
            {bigIntTypeInfo, floatTypeInfo, numberTypeInfo, dateTypeInfo}, "an integer, float, number, or date value");
    }

    DLLLOCAL int parseInitIntern(const char* name, QoreParseContext& parse_context) {
        assert(!parse_context.typeInfo);
        // the operands' values are used: turn off the "return value ignored" flag of the statement, as += does
        // (qore#6140)
        QoreParseContextFlagHelper fh_rvi(parse_context);
        fh_rvi.unsetFlags(PF_RETURN_VALUE_IGNORED);
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
        const bool divide = name[0] == '/';
        lvalue_e kind = getLValueKind(ti, rightTypeInfo, divide);
        // /= divides by a date operand as by an integer: the lvalue's type is checked as for an integer operand,
        // while the result type stays as it was (qore#6103)
        const bool date_divisor = divide && QoreTypeInfo::isType(rightTypeInfo, NT_DATE);
        const QoreTypeInfo* check_rti = date_divisor ? bigIntTypeInfo : rightTypeInfo;
        lvalue_e check_kind = (plain_ti == ti && check_rti == rightTypeInfo)
            ? kind
            : getLValueKind(plain_ti, check_rti, divide);

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
                // the integer quotient of a date divisor is refused only if the lvalue's type refuses it both as
                // check_lvalue_int() does for an integer divisor and as an assignment does, which a soft type
                // accepts by converting it (ex: softstring); the result type is not changed for it
                if ((date_divisor
                        ? (!QoreTypeInfo::parseAcceptsReturns(ti, NT_INT)
                            && check_lvalue_accepts_result(loc, ti, name, {bigIntTypeInfo}, "an integer value"))
                        : check_lvalue_int(loc, ti, name))
                        && !err) {
                    err = -1;
                }
                break;
            // a result that the lvalue's type can never hold is a parse error, as for an assignment (qore#6103)
            case lvalue_e::DATE:
                if (check_lvalue_accepts_result(loc, ti, name, {dateTypeInfo}, "a date value") && !err) {
                    err = -1;
                }
                break;
            case lvalue_e::DATE_QUOTIENT:
                if (checkDateQuotient(check_rti, name) && !err) {
                    err = -1;
                }
                break;
            case lvalue_e::GENERIC:
                if (checkGenericResult(divide, name) && !err) {
                    err = -1;
                }
                break;
            case lvalue_e::KEEP:
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
            case lvalue_e::DATE:
            case lvalue_e::GENERIC:
                ti = nullptr;
                break;
            case lvalue_e::DATE_QUOTIENT:
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
