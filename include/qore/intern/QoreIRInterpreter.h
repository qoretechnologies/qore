/* -*- indent-tabs-mode: nil -*- */
/*
    QoreIRInterpreter.h

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

#ifndef _QORE_INTERN_QOREIRINTERPRETER_H
#define _QORE_INTERN_QOREIRINTERPRETER_H

#include <qore/intern/QoreIR.h>

#include <string>
#include <unordered_set>
#include <vector>

class ExceptionSink;
class LocalVar;
class QoreProgram;
class QoreValue;
class AbstractStatement;
class StatementBlock;

class QoreIRInterpreter {
public:
    static QoreValue evalComparison(QoreIROpcode op, const QoreValue& left, const QoreValue& right,
            ExceptionSink* xsink);
    static QoreValue evalExpr(QoreIROpcode op, const QoreValue& expr, ExceptionSink* xsink);
    static QoreValue evalUnary(QoreIROpcode op, const QoreValue& value, ExceptionSink* xsink);
    static QoreValue evalBinary(QoreIROpcode op, const QoreValue& left, const QoreValue& right,
            ExceptionSink* xsink);
    static QoreValue evalTernary(QoreIROpcode op, const QoreValue& first, const QoreValue& second,
            const QoreValue& third, ExceptionSink* xsink);
    static QoreValue evalQuaternary(QoreIROpcode op, const QoreValue& first, const QoreValue& second,
            const QoreValue& third, const QoreValue& fourth, ExceptionSink* xsink);
    static QoreValue evalLValueLoad(const QoreValue& lvalue, ExceptionSink* xsink);
    static QoreValue evalLValueStore(const QoreValue& lvalue, const QoreValue& value, ExceptionSink* xsink,
            bool weak = false);
    static QoreValue evalLValueUnary(QoreIROpcode op, const QoreValue& lvalue, ExceptionSink* xsink);
    static QoreValue evalLValueBinary(QoreIROpcode op, const QoreValue& lvalue, const QoreValue& right,
            ExceptionSink* xsink);
    static QoreValue evalLValueTernary(QoreIROpcode op, const QoreValue& lvalue, const QoreValue& first,
            const QoreValue& second, const QoreValue& third, ExceptionSink* xsink);
    static bool simulateInvoke(QoreIROpcode op, const QoreValue& expr, ExceptionSink* xsink);
    static int execStatement(QoreIROpcode op, const AbstractStatement* stmt, QoreValue& return_value,
            ExceptionSink* xsink);
    static bool execute(const QoreIRFunction& func, QoreValue& return_value, ExceptionSink* xsink,
            std::vector<std::string>* cleanup_log = nullptr, const std::vector<QoreValue>* args = nullptr,
            const std::vector<QoreValue>* closure = nullptr,
            const std::unordered_set<const LocalVar*>* pre_instantiated = nullptr,
            const LocalVar* excluded_selfid = nullptr,
            const StatementBlock* statements = nullptr, QoreProgram* pgm = nullptr,
            bool suppress_guard_deopt = false);
};

#endif
