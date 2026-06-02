/*
    QoreBinaryXorOperatorNode.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software or associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    or/or sell copies of the Software, or to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice or this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#include <qore/Qore.h>
#include <qore/intern/QorePluginRegistry.h>
#include "qore/intern/qore_program_private.h"

QoreString QoreBinaryXorOperatorNode::op_str("^ (binary xor) operator expression");

static void set_binary_xor_analysis_bitwise(QoreParseContext& parse_context,
        const QoreParseAnalysis& left_analysis, const QoreParseAnalysis& right_analysis,
        const QorePluginResolvedOperationInfo* plugin_op = nullptr) {
    parse_context.analysis.clear();
    parse_context.analysis.setFlag(QoreParseAnalysis::KnownTypeInfo);
    if (!plugin_op || (!plugin_op->signature.return_nullable && !plugin_op->info.can_return_nothing)) {
        parse_context.analysis.setFlag(QoreParseAnalysis::NeverNothing);
    }
    parse_context.analysis.known_type = parse_context.typeInfo;
    if (left_analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned)
            && right_analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned)) {
        parse_context.analysis.setFlag(QoreParseAnalysis::DefinitelyAssigned);
    }
}

QoreValue QoreBinaryXorOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    ValueEvalOptimizedRefHolder lh(left, xsink);
    if (*xsink) return QoreValue();
    ValueEvalOptimizedRefHolder rh(right, xsink);
    if (*xsink) return QoreValue();

    if (qore_plugin_value_may_have_operation(*lh) || qore_plugin_value_may_have_operation(*rh)) {
        bool plugin_matched = false;
        QoreValue plugin_result = qore_plugin_try_dispatch_binary(getProgram(), "bit_xor",
            QorePluginHelperAbi::BinaryValue, *lh, *rh, plugin_matched, xsink);
        if (*xsink || plugin_matched) {
            return plugin_result;
        }
    }

    return lh->getAsBigInt() ^ rh->getAsBigInt();
}

int QoreBinaryXorOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    // turn off "return value ignored" flags
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    parse_context.typeInfo = nullptr;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    int err = 0;
    {
        QoreParseContextAnalysisHelper ah(parse_context);
        err = parse_init_value(left, parse_context);
        left_analysis = parse_context.analysis;
    }
    const QoreTypeInfo* lti = parse_context.typeInfo;
    parse_context.typeInfo = nullptr;
    {
        QoreParseContextAnalysisHelper ah(parse_context);
        if (parse_init_value(right, parse_context) && !err) {
            err = -1;
        }
        right_analysis = parse_context.analysis;
    }

    if (!err) {
        QorePluginResolvedOperationInfo plugin_op;
        ExceptionSink* parse_xsink = parse_context.pgm ? parse_context.pgm->getParseExceptionSink() : nullptr;
        int plugin_rc = qore_plugin_resolve_program_operation_info(parse_context.pgm, lti, parse_context.typeInfo,
            "bit_xor", QorePluginHelperAbi::BinaryValue, plugin_op, parse_xsink);
        if (plugin_rc < 0) {
            err = -1;
        } else if (!plugin_rc) {
            parse_context.typeInfo = plugin_op.signature.return_type;
            set_binary_xor_analysis_bitwise(parse_context, left_analysis, right_analysis, &plugin_op);
            return err;
        }
    }

    // see if any of the arguments cannot be converted to an integer, if so generate a warning
    if (!QoreTypeInfo::canConvertToScalar(lti)) {
        if (!QoreTypeInfo::canConvertToScalar(parse_context.typeInfo)) {
            QoreStringNode* desc = new QoreStringNode("neither side of the binary xor (^) expression can be " \
                "converted to an integer (left hand side is ");
            QoreTypeInfo::getThisType(lti, *desc);
            desc->concat("; right hand side is ");
            QoreTypeInfo::getThisType(parse_context.typeInfo, *desc);
            desc->concat("), therefore the entire expression will always return a constant 0");
            qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION, "INVALID-OPERATION",
                desc);
            if (!err) {
                err = -1;
            }
        } else {
            QoreStringNode* desc = new QoreStringNode("the left hand side of the binary xor (^) expression is ");
            QoreTypeInfo::getThisType(lti, *desc);
            desc->concat(", which cannot be converted to an integer, therefore the entire expression will always " \
                "return the integer value of the right hand side");
            qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION, "INVALID-OPERATION",
                desc);
            if (!err) {
                err = -1;
            }
        }
    } else if (!QoreTypeInfo::canConvertToScalar(parse_context.typeInfo)) {
        QoreStringNode* desc = new QoreStringNode("the right hand side of the binary xor (^) expression is ");
        QoreTypeInfo::getThisType(parse_context.typeInfo, *desc);
        desc->concat(", which cannot be converted to an integer, therefore the entire expression will always " \
            "return the integer value of the left hand side");
        qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION, "INVALID-OPERATION",
            desc);
        if (!err) {
            err = -1;
        }
    }

    // see if both arguments are constant values, then eval immediately or substitute this node with the result
    if (!err && left.isValue() && right.isValue()) {
        SimpleRefHolder<QoreBinaryXorOperatorNode> del(this);
        ParseExceptionSink xsink;
        ValueEvalOptimizedRefHolder v(this, *xsink);
        assert(!**xsink);
        QoreValue result = v.takeReferencedValue();
        // only use parse-time folding if we got a valid result
        // (constants may not be fully resolved at parse time, resulting in NOTHING)
        if (!result.isNothing()) {
            val = result;
        } else {
            // constants not resolved - skip parse-time folding, let runtime handle it
            del.release();
        }
    }

    parse_context.typeInfo = bigIntTypeInfo;
    set_binary_xor_analysis_bitwise(parse_context, left_analysis, right_analysis);
    return err;
}
