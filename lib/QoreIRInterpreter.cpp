/* -*- indent-tabs-mode: nil -*- */
/*
    QoreIRInterpreter.cpp

    Qore Programming Language
*/

#include <qore/intern/QoreIRInterpreter.h>

#include <cmath>

#include <qore/ExceptionSink.h>
#include <qore/QoreValue.h>
#include <qore/intern/Variable.h>
#include <qore/intern/QoreDivisionOperatorNode.h>
#include <qore/intern/QoreBinaryAndOperatorNode.h>
#include <qore/intern/QoreBinaryOrOperatorNode.h>
#include <qore/intern/QoreBinaryXorOperatorNode.h>
#include <qore/intern/QorePlusEqualsOperatorNode.h>
#include <qore/intern/QoreMinusEqualsOperatorNode.h>
#include <qore/intern/QoreMultiplyEqualsOperatorNode.h>
#include <qore/intern/QoreDivideEqualsOperatorNode.h>
#include <qore/intern/QoreModuloEqualsOperatorNode.h>
#include <qore/intern/QoreAndEqualsOperatorNode.h>
#include <qore/intern/QoreOrEqualsOperatorNode.h>
#include <qore/intern/QoreXorEqualsOperatorNode.h>
#include <qore/intern/QoreFoldlOperatorNode.h>
#include <qore/intern/QoreLogicalComparisonOperatorNode.h>
#include <qore/intern/QoreLogicalEqualsOperatorNode.h>
#include <qore/intern/QoreLogicalGreaterThanOperatorNode.h>
#include <qore/intern/QoreLogicalGreaterThanOrEqualsOperatorNode.h>
#include <qore/intern/QoreLogicalLessThanOperatorNode.h>
#include <qore/intern/QoreLogicalLessThanOrEqualsOperatorNode.h>
#include <qore/intern/QoreMapOperatorNode.h>
#include <qore/intern/QoreModuloOperatorNode.h>
#include <qore/intern/QoreMinusOperatorNode.h>
#include <qore/intern/QoreMultiplicationOperatorNode.h>
#include <qore/intern/QorePlusOperatorNode.h>
#include <qore/intern/QoreRangeOperatorNode.h>
#include <qore/intern/QoreSquareBracketsRangeOperatorNode.h>
#include <qore/intern/QoreShiftOperatorNode.h>
#include <qore/intern/QoreShiftLeftOperatorNode.h>
#include <qore/intern/QoreShiftLeftEqualsOperatorNode.h>
#include <qore/intern/QoreShiftRightOperatorNode.h>
#include <qore/intern/QoreShiftRightEqualsOperatorNode.h>
#include <qore/intern/QorePreIncrementOperatorNode.h>
#include <qore/intern/QorePostIncrementOperatorNode.h>
#include <qore/intern/QorePreDecrementOperatorNode.h>
#include <qore/intern/QorePostDecrementOperatorNode.h>
#include <qore/intern/QoreSpliceOperatorNode.h>
#include <qore/intern/QoreUnshiftOperatorNode.h>
#include <qore/intern/QoreUnaryMinusOperatorNode.h>
#include <qore/intern/QoreUnaryPlusOperatorNode.h>
#include <qore/intern/QoreCastOperatorNode.h>

static QoreValue evalExprNode(const QoreValue& expr, ExceptionSink* xsink) {
    if (!expr.hasNode()) {
        if (xsink) {
            xsink->raiseException("IR-INTERPRETER-ERROR", "expression opcode requires a parse node");
        }
        return QoreValue();
    }
    bool needs_deref = false;
    ValueHolder node(expr.refSelf(), xsink);
    if (xsink && *xsink) {
        return QoreValue();
    }
    return node->eval(needs_deref, xsink);
}

QoreValue QoreIRInterpreter::evalComparison(QoreIROpcode op, const QoreValue& left, const QoreValue& right,
        ExceptionSink* xsink) {
    switch (op) {
        case QoreIROpcode::EqInt:
            return QoreValue(left.getAsBigInt() == right.getAsBigInt());
        case QoreIROpcode::EqAny:
            return QoreValue(QoreLogicalEqualsOperatorNode::softEqual(left, right, xsink));
        case QoreIROpcode::NeInt:
            return QoreValue(left.getAsBigInt() != right.getAsBigInt());
        case QoreIROpcode::NeAny:
            return QoreValue(!QoreLogicalEqualsOperatorNode::softEqual(left, right, xsink));
        case QoreIROpcode::EqHard:
            return QoreValue(left.isEqualHard(right));
        case QoreIROpcode::NeHard:
            return QoreValue(!left.isEqualHard(right));
        case QoreIROpcode::LtInt:
            return QoreValue(left.getAsBigInt() < right.getAsBigInt());
        case QoreIROpcode::LtFloat:
            return QoreValue(left.getAsFloat() < right.getAsFloat());
        case QoreIROpcode::LtAny:
            return QoreValue(QoreLogicalLessThanOperatorNode::doLessThan(left, right, xsink));
        case QoreIROpcode::LeInt:
            return QoreValue(left.getAsBigInt() <= right.getAsBigInt());
        case QoreIROpcode::LeFloat:
            return QoreValue(left.getAsFloat() <= right.getAsFloat());
        case QoreIROpcode::LeAny:
            return QoreValue(QoreLogicalLessThanOrEqualsOperatorNode::doLessThanOrEquals(left, right, xsink));
        case QoreIROpcode::GtInt:
            return QoreValue(left.getAsBigInt() > right.getAsBigInt());
        case QoreIROpcode::GtFloat:
            return QoreValue(left.getAsFloat() > right.getAsFloat());
        case QoreIROpcode::GtAny:
            return QoreValue(QoreLogicalGreaterThanOperatorNode::doGreaterThan(left, right, xsink));
        case QoreIROpcode::GeInt:
            return QoreValue(left.getAsBigInt() >= right.getAsBigInt());
        case QoreIROpcode::GeFloat:
            return QoreValue(left.getAsFloat() >= right.getAsFloat());
        case QoreIROpcode::GeAny:
            return QoreValue(QoreLogicalGreaterThanOrEqualsOperatorNode::doGreaterThanOrEquals(left, right, xsink));
        case QoreIROpcode::CmpInt: {
            int64_t l = left.getAsBigInt();
            int64_t r = right.getAsBigInt();
            return QoreValue(l < r ? -1 : (l > r ? 1 : 0));
        }
        case QoreIROpcode::CmpFloat: {
            double l = left.getAsFloat();
            double r = right.getAsFloat();
            if (std::isnan(l) || std::isnan(r)) {
                if (xsink) {
                    xsink->raiseException("NAN-COMPARE-ERROR",
                        "NaN in floating-point comparison for logical comparison operator");
                }
                return QoreValue();
            }
            return QoreValue(l < r ? -1 : (l > r ? 1 : 0));
        }
        case QoreIROpcode::CmpAny:
            return QoreValue(QoreLogicalComparisonOperatorNode::doComparison(left, right, xsink));
        default:
            break;
    }
    if (xsink) {
        xsink->raiseException("IR-INTERPRETER-ERROR", "unsupported comparison opcode");
    }
    return QoreValue();
}

QoreValue QoreIRInterpreter::evalExpr(QoreIROpcode op, const QoreValue& expr, ExceptionSink* xsink) {
    switch (op) {
        case QoreIROpcode::Call:
        case QoreIROpcode::CallIndirect:
        case QoreIROpcode::CallMethod:
        case QoreIROpcode::CallStatic:
        case QoreIROpcode::Invoke:
        case QoreIROpcode::ExtractAny:
        case QoreIROpcode::RemoveAny:
        case QoreIROpcode::KeysAny:
            return evalExprNode(expr, xsink);
        case QoreIROpcode::CastAny: {
            if (expr.hasNode()) {
                auto* parse_cast = dynamic_cast<const QoreParseCastOperatorNode*>(expr.getInternalNode());
                if (parse_cast) {
                    if (xsink) {
                        xsink->raiseException("IR-INTERPRETER-ERROR",
                            "cast opcode requires parse initialization");
                    }
                    return QoreValue();
                }
            }
            return evalExprNode(expr, xsink);
        }
        default:
            break;
    }
    if (xsink) {
        xsink->raiseException("IR-INTERPRETER-ERROR", "unsupported expression opcode");
    }
    return QoreValue();
}

QoreValue QoreIRInterpreter::evalUnary(QoreIROpcode op, const QoreValue& value, ExceptionSink* xsink) {
    switch (op) {
        case QoreIROpcode::ToBool:
            return QoreValue(value.getAsBool());
        case QoreIROpcode::Not:
            return QoreValue(!value.getAsBool());
        case QoreIROpcode::IsNullOrNothing:
            return QoreValue(value.isNullOrNothing());
        case QoreIROpcode::UnaryPlusAny: {
            bool needs_deref = false;
            QoreUnaryPlusOperatorNode node(nullptr, value);
            return node.eval(needs_deref, xsink);
        }
        case QoreIROpcode::UnaryMinusInt:
            return QoreValue(-value.getAsBigInt());
        case QoreIROpcode::UnaryMinusFloat:
            return QoreValue(-value.getAsFloat());
        case QoreIROpcode::UnaryMinusAny: {
            bool needs_deref = false;
            QoreUnaryMinusOperatorNode node(nullptr, value);
            return node.eval(needs_deref, xsink);
        }
        default:
            break;
    }
    if (xsink) {
        xsink->raiseException("IR-INTERPRETER-ERROR", "unsupported unary opcode");
    }
    return QoreValue();
}

QoreValue QoreIRInterpreter::evalBinary(QoreIROpcode op, const QoreValue& left, const QoreValue& right,
        ExceptionSink* xsink) {
    switch (op) {
        case QoreIROpcode::AddInt:
            return QoreValue(left.getAsBigInt() + right.getAsBigInt());
        case QoreIROpcode::AddFloat:
            return QoreValue(left.getAsFloat() + right.getAsFloat());
        case QoreIROpcode::AddAny: {
            bool needs_deref = false;
            QorePlusOperatorNode node(nullptr, left, right);
            return node.eval(needs_deref, xsink);
        }
        case QoreIROpcode::SubInt:
            return QoreValue(left.getAsBigInt() - right.getAsBigInt());
        case QoreIROpcode::SubFloat:
            return QoreValue(left.getAsFloat() - right.getAsFloat());
        case QoreIROpcode::SubAny: {
            bool needs_deref = false;
            QoreMinusOperatorNode node(nullptr, left, right);
            return node.eval(needs_deref, xsink);
        }
        case QoreIROpcode::MulInt:
            return QoreValue(left.getAsBigInt() * right.getAsBigInt());
        case QoreIROpcode::MulFloat:
            return QoreValue(left.getAsFloat() * right.getAsFloat());
        case QoreIROpcode::MulAny: {
            bool needs_deref = false;
            QoreMultiplicationOperatorNode node(nullptr, left, right);
            return node.eval(needs_deref, xsink);
        }
        case QoreIROpcode::DivInt: {
            int64_t divisor = right.getAsBigInt();
            if (!divisor) {
                if (xsink) {
                    xsink->raiseException("DIVISION-BY-ZERO", "division by zero found in integer expression");
                }
                return QoreValue();
            }
            return QoreValue(left.getAsBigInt() / divisor);
        }
        case QoreIROpcode::DivFloat: {
            double divisor = right.getAsFloat();
            if (!divisor) {
                if (xsink) {
                    xsink->raiseException("DIVISION-BY-ZERO", "division by zero found in floating-point expression");
                }
                return QoreValue();
            }
            return QoreValue(left.getAsFloat() / divisor);
        }
        case QoreIROpcode::DivAny:
            return QoreDivisionOperatorNode::doDivision(left, right, xsink);
        case QoreIROpcode::ModInt:
        case QoreIROpcode::ModAny: {
            int64_t divisor = right.getAsBigInt();
            if (!divisor) {
                if (xsink) {
                    xsink->raiseException("DIVISION-BY-ZERO", "modula operand cannot be zero");
                }
                return QoreValue();
            }
            return QoreValue(left.getAsBigInt() % divisor);
        }
        case QoreIROpcode::AndInt:
            return QoreValue(left.getAsBigInt() & right.getAsBigInt());
        case QoreIROpcode::AndAny: {
            bool needs_deref = false;
            ValueHolder node(QoreValue(new QoreBinaryAndOperatorNode(nullptr, left, right)), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::OrInt:
            return QoreValue(left.getAsBigInt() | right.getAsBigInt());
        case QoreIROpcode::OrAny: {
            bool needs_deref = false;
            ValueHolder node(QoreValue(new QoreBinaryOrOperatorNode(nullptr, left, right)), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::XorInt:
            return QoreValue(left.getAsBigInt() ^ right.getAsBigInt());
        case QoreIROpcode::XorAny: {
            bool needs_deref = false;
            ValueHolder node(QoreValue(new QoreBinaryXorOperatorNode(nullptr, left, right)), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::AddAssignInt:
            return QoreValue(left.getAsBigInt() + right.getAsBigInt());
        case QoreIROpcode::AddAssignAny: {
            bool needs_deref = false;
            QorePlusOperatorNode node(nullptr, left, right);
            return node.eval(needs_deref, xsink);
        }
        case QoreIROpcode::SubAssignInt:
            return QoreValue(left.getAsBigInt() - right.getAsBigInt());
        case QoreIROpcode::SubAssignAny: {
            bool needs_deref = false;
            QoreMinusOperatorNode node(nullptr, left, right);
            return node.eval(needs_deref, xsink);
        }
        case QoreIROpcode::MulAssignInt:
            return QoreValue(left.getAsBigInt() * right.getAsBigInt());
        case QoreIROpcode::MulAssignAny: {
            bool needs_deref = false;
            QoreMultiplicationOperatorNode node(nullptr, left, right);
            return node.eval(needs_deref, xsink);
        }
        case QoreIROpcode::DivAssignInt: {
            int64_t divisor = right.getAsBigInt();
            if (!divisor) {
                if (xsink) {
                    xsink->raiseException("DIVISION-BY-ZERO", "division by zero found in integer expression");
                }
                return QoreValue();
            }
            return QoreValue(left.getAsBigInt() / divisor);
        }
        case QoreIROpcode::DivAssignAny:
            return QoreDivisionOperatorNode::doDivision(left, right, xsink);
        case QoreIROpcode::ModAssignInt:
        case QoreIROpcode::ModAssignAny: {
            int64_t divisor = right.getAsBigInt();
            if (!divisor) {
                if (xsink) {
                    xsink->raiseException("DIVISION-BY-ZERO", "modula operand cannot be zero");
                }
                return QoreValue();
            }
            return QoreValue(left.getAsBigInt() % divisor);
        }
        case QoreIROpcode::AndAssignInt:
            return QoreValue(left.getAsBigInt() & right.getAsBigInt());
        case QoreIROpcode::AndAssignAny: {
            bool needs_deref = false;
            ValueHolder node(QoreValue(new QoreBinaryAndOperatorNode(nullptr, left, right)), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::OrAssignInt:
            return QoreValue(left.getAsBigInt() | right.getAsBigInt());
        case QoreIROpcode::OrAssignAny: {
            bool needs_deref = false;
            ValueHolder node(QoreValue(new QoreBinaryOrOperatorNode(nullptr, left, right)), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::XorAssignInt:
            return QoreValue(left.getAsBigInt() ^ right.getAsBigInt());
        case QoreIROpcode::XorAssignAny: {
            bool needs_deref = false;
            ValueHolder node(QoreValue(new QoreBinaryXorOperatorNode(nullptr, left, right)), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::ShlInt:
            return QoreValue(left.getAsBigInt() << right.getAsBigInt());
        case QoreIROpcode::ShlAny: {
            bool needs_deref = false;
            ValueHolder node(QoreValue(new QoreShiftLeftOperatorNode(nullptr, left, right)), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::ShrInt:
            return QoreValue(left.getAsBigInt() >> right.getAsBigInt());
        case QoreIROpcode::ShrAny: {
            bool needs_deref = false;
            ValueHolder node(QoreValue(new QoreShiftRightOperatorNode(nullptr, left, right)), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::ShlAssignInt:
            return QoreValue(left.getAsBigInt() << right.getAsBigInt());
        case QoreIROpcode::ShlAssignAny: {
            bool needs_deref = false;
            ValueHolder node(QoreValue(new QoreShiftLeftEqualsOperatorNode(nullptr, left, right)), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::ShrAssignInt:
            return QoreValue(left.getAsBigInt() >> right.getAsBigInt());
        case QoreIROpcode::ShrAssignAny: {
            bool needs_deref = false;
            ValueHolder node(QoreValue(new QoreShiftRightEqualsOperatorNode(nullptr, left, right)), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::FoldlAny: {
            bool needs_deref = false;
            ValueHolder node(QoreValue(new QoreFoldlOperatorNode(nullptr, left, right)), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::FoldrAny: {
            bool needs_deref = false;
            ValueHolder node(QoreValue(new QoreFoldrOperatorNode(nullptr, left, right)), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::MapAny: {
            bool needs_deref = false;
            ValueHolder node(QoreValue(new QoreMapOperatorNode(nullptr, left, right)), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::RangeAny: {
            bool needs_deref = false;
            ValueHolder node(QoreValue(new QoreRangeOperatorNode(nullptr, left, right)), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::EqInt:
        case QoreIROpcode::EqAny:
        case QoreIROpcode::NeInt:
        case QoreIROpcode::NeAny:
        case QoreIROpcode::EqHard:
        case QoreIROpcode::NeHard:
        case QoreIROpcode::LtInt:
        case QoreIROpcode::LtFloat:
        case QoreIROpcode::LtAny:
        case QoreIROpcode::LeInt:
        case QoreIROpcode::LeFloat:
        case QoreIROpcode::LeAny:
        case QoreIROpcode::GtInt:
        case QoreIROpcode::GtFloat:
        case QoreIROpcode::GtAny:
        case QoreIROpcode::GeInt:
        case QoreIROpcode::GeFloat:
        case QoreIROpcode::GeAny:
        case QoreIROpcode::CmpInt:
        case QoreIROpcode::CmpFloat:
        case QoreIROpcode::CmpAny:
            return evalComparison(op, left, right, xsink);
        default:
            break;
    }
    if (xsink) {
        xsink->raiseException("IR-INTERPRETER-ERROR", "unsupported binary opcode");
    }
    return QoreValue();
}

QoreValue QoreIRInterpreter::evalTernary(QoreIROpcode op, const QoreValue& first, const QoreValue& second,
        const QoreValue& third, ExceptionSink* xsink) {
    switch (op) {
        case QoreIROpcode::RangeSliceAny: {
            bool needs_deref = false;
            ValueHolder node(QoreValue(new QoreSquareBracketsRangeOperatorNode(nullptr, first, second, third)),
                xsink);
            return node->eval(needs_deref, xsink);
        }
        default:
            break;
    }
    if (xsink) {
        xsink->raiseException("IR-INTERPRETER-ERROR", "unsupported ternary opcode");
    }
    return QoreValue();
}

QoreValue QoreIRInterpreter::evalLValueLoad(const QoreValue& lvalue, ExceptionSink* xsink) {
    LValueHelper helper(lvalue, xsink);
    if (!helper) {
        return QoreValue();
    }
    return helper.getReferencedValue();
}

QoreValue QoreIRInterpreter::evalLValueStore(const QoreValue& lvalue, const QoreValue& value, ExceptionSink* xsink) {
    LValueHelper helper(lvalue, xsink);
    if (!helper) {
        return QoreValue();
    }
    if (helper.assign(value, "<lvalue>")) {
        return QoreValue();
    }
    return value.refSelf();
}

QoreValue QoreIRInterpreter::evalLValueUnary(QoreIROpcode op, const QoreValue& lvalue, ExceptionSink* xsink) {
    bool needs_deref = false;
    QoreValue lvalue_ref = lvalue.refSelf();
    switch (op) {
        case QoreIROpcode::PreIncLValue: {
            ValueHolder node(QoreValue(new QorePreIncrementOperatorNode(nullptr, lvalue_ref)), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::PreDecLValue: {
            ValueHolder node(QoreValue(new QorePreDecrementOperatorNode(nullptr, lvalue_ref)), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::PostIncLValue: {
            ValueHolder node(QoreValue(new QorePostIncrementOperatorNode(nullptr, lvalue_ref)), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::PostDecLValue: {
            ValueHolder node(QoreValue(new QorePostDecrementOperatorNode(nullptr, lvalue_ref)), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::ShiftLValue: {
            ValueHolder node(QoreValue(new QoreShiftOperatorNode(nullptr, lvalue_ref)), xsink);
            return node->eval(needs_deref, xsink);
        }
        default:
            break;
    }
    if (xsink) {
        xsink->raiseException("IR-INTERPRETER-ERROR", "unsupported lvalue unary opcode");
    }
    return QoreValue();
}

QoreValue QoreIRInterpreter::evalLValueBinary(QoreIROpcode op, const QoreValue& lvalue, const QoreValue& right,
        ExceptionSink* xsink) {
    bool needs_deref = false;
    QoreValue lvalue_ref = lvalue.refSelf();
    switch (op) {
        case QoreIROpcode::AddAssignLValue: {
            ValueHolder node(QoreValue(new QorePlusEqualsOperatorNode(nullptr, lvalue_ref, right)), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::SubAssignLValue: {
            ValueHolder node(QoreValue(new QoreMinusEqualsOperatorNode(nullptr, lvalue_ref, right)), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::MulAssignLValue: {
            ValueHolder node(QoreValue(new QoreMultiplyEqualsOperatorNode(nullptr, lvalue_ref, right)), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::DivAssignLValue: {
            ValueHolder node(QoreValue(new QoreDivideEqualsOperatorNode(nullptr, lvalue_ref, right)), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::ModAssignLValue: {
            ValueHolder node(QoreValue(new QoreModuloEqualsOperatorNode(nullptr, lvalue_ref, right)), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::AndAssignLValue: {
            ValueHolder node(QoreValue(new QoreAndEqualsOperatorNode(nullptr, lvalue_ref, right)), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::OrAssignLValue: {
            ValueHolder node(QoreValue(new QoreOrEqualsOperatorNode(nullptr, lvalue_ref, right)), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::XorAssignLValue: {
            ValueHolder node(QoreValue(new QoreXorEqualsOperatorNode(nullptr, lvalue_ref, right)), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::ShlAssignLValue: {
            ValueHolder node(QoreValue(new QoreShiftLeftEqualsOperatorNode(nullptr, lvalue_ref, right)), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::ShrAssignLValue: {
            ValueHolder node(QoreValue(new QoreShiftRightEqualsOperatorNode(nullptr, lvalue_ref, right)), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::UnshiftLValue: {
            ValueHolder node(QoreValue(new QoreUnshiftOperatorNode(nullptr, lvalue_ref, right)), xsink);
            return node->eval(needs_deref, xsink);
        }
        default:
            break;
    }
    if (xsink) {
        xsink->raiseException("IR-INTERPRETER-ERROR", "unsupported lvalue binary opcode");
    }
    return QoreValue();
}

QoreValue QoreIRInterpreter::evalLValueTernary(QoreIROpcode op, const QoreValue& lvalue, const QoreValue& first,
        const QoreValue& second, const QoreValue& third, ExceptionSink* xsink) {
    bool needs_deref = false;
    QoreValue lvalue_ref = lvalue.refSelf();
    switch (op) {
        case QoreIROpcode::SpliceLValue: {
            ValueHolder node(QoreValue(new QoreSpliceOperatorNode(nullptr, lvalue_ref, first, second, third)), xsink);
            return node->eval(needs_deref, xsink);
        }
        default:
            break;
    }
    if (xsink) {
        xsink->raiseException("IR-INTERPRETER-ERROR", "unsupported lvalue ternary opcode");
    }
    return QoreValue();
}
