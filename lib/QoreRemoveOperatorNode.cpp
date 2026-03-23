/*
    QoreRemoveOperatorNode.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Techologies s.r.o.

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
#include "qore/intern/qore_string_private.h"

QoreString QoreRemoveOperatorNode::remove_str("remove operator expression");

// if del is true, then the returned QoreString * should be removed, if false, then it must not be
QoreString* QoreRemoveOperatorNode::getAsString(bool &del, int foff, ExceptionSink *xsink) const {
    del = false;
    return &remove_str;
}

int QoreRemoveOperatorNode::getAsString(QoreString &str, int foff, ExceptionSink *xsink) const {
    qore_string_private::get(str)->concat(&remove_str);
    return 0;
}

QoreValue QoreRemoveOperatorNode::evalImpl(bool& needs_deref, ExceptionSink *xsink) const {
    LValueRemoveHelper lvrh(exp, xsink, false);
    if (!lvrh)
        return QoreValue();

    bool static_assignment = false;
    QoreValue rv = lvrh.remove(static_assignment);
    assert(needs_deref);
    if (static_assignment)
        needs_deref = false;
    return rv;
}

int QoreRemoveOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    assert(!parse_context.typeInfo);
    QoreParseAnalysis operand_analysis;
    int err = 0;
    {
        QoreParseContextAnalysisHelper ah(parse_context);
        err = parse_init_value(exp, parse_context);
        operand_analysis = parse_context.analysis;
    }
    if (exp && !err) {
        err = checkLValue(exp, parse_context.pflag);
    }

    // If remove is applied to a simple variable reference, reset its assignment tracking
    // since remove unassigns the variable (sets it to NOTHING)
    if (exp && exp.getType() == NT_VARREF && !err) {
        VarRefNode* vrn = exp.get<VarRefNode>();
        qore_var_t vtype = vrn->getType();
        if (vtype == VT_LOCAL || vtype == VT_CLOSURE || vtype == VT_LOCAL_TS) {
            LocalVar* lvar = vrn->ref.id;
            if (lvar) {
                lvar->parseUnassigned();
            }
        }
        // TODO: handle global and thread-local variables if needed
    }

    returnTypeInfo = parse_context.typeInfo;
    parse_context.analysis.clear();
    if (returnTypeInfo) {
        parse_context.analysis.setFlag(QoreParseAnalysis::KnownTypeInfo);
        parse_context.analysis.known_type = returnTypeInfo;
        if (operand_analysis.hasFlag(QoreParseAnalysis::NeverNothing)) {
            parse_context.analysis.setFlag(QoreParseAnalysis::NeverNothing);
        }
    }
    if (operand_analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned)) {
        parse_context.analysis.setFlag(QoreParseAnalysis::DefinitelyAssigned);
    }
    return err;
}
