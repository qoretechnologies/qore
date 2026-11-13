/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreArithmeticParseType.h

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_INTERN_QOREARITHMETICPARSETYPE_H
#define _QORE_INTERN_QOREARITHMETICPARSETYPE_H

//! the binary arithmetic operators whose parse result type qore_arith_parse_result_type() gives
enum class QoreArithParseOp {
    Plus,
    Minus,
    Multiply,
    Divide,
};

//! the kind of result of qore_arith_parse_result_type()
enum class QoreArithParseResult {
    //! every combination of the values that the operand types allow gives a scalar value or no value
    Scalar,
    //! some combination gives a list, a hash or an object: when each operand has one value type, the operator's own
    //! rules give the container type more precisely (ex: its element type)
    Container,
    //! some combination gives a value without a parse type here (a buffer, SQL NULL): the result type is not known
    Untyped,
};

//! Returns the parse result type of a binary arithmetic operator from the runtime result of every combination of the
//! values that the operand types allow (qore#5663)
/** An operand type that is not known allows a value of any type.  The result type is the union of the results (or
    one type, or-nothing if a combination gives no value).

    @param op the operator
    @param lti the type of the left operand
    @param rti the type of the right operand
    @param result the result type, unless QoreArithParseResult::Untyped is returned
    @param single true on return if each operand has one value type (NOTHING counts as a value type)
*/
DLLLOCAL QoreArithParseResult qore_arith_parse_result_type(QoreArithParseOp op, const QoreTypeInfo* lti,
        const QoreTypeInfo* rti, const QoreTypeInfo*& result, bool& single);

//! Returns true if the container type that an operator's own rules give (\a rule_ti) has the base type of the result
//! that qore_arith_parse_result_type() computes (\a arith_ti): a list, a hash or an object
/** The rules of an operator for operands of one value type each give a container type more precisely (ex: its element
    type), but some do not give the value that the operator returns (ex: \c nothing for <tt>bool + hash</tt>)
*/
DLLLOCAL bool qore_arith_container_rule_matches(const QoreTypeInfo* rule_ti, const QoreTypeInfo* arith_ti);

#endif
