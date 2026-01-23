/* -*- indent-tabs-mode: nil -*- */
/*
    QoreIRLowering.cpp

    Qore Programming Language
*/

#include <qore/intern/QoreIRLowering.h>

#include <qore/DateTimeNode.h>
#include <qore/QoreValue.h>
#include <qore/intern/QoreLibIntern.h>
#include <qore/intern/ParseNode.h>
#include <qore/intern/QoreLogicalAbsoluteEqualsOperatorNode.h>
#include <qore/intern/QoreLogicalAbsoluteNotEqualsOperatorNode.h>
#include <qore/intern/QoreLogicalAndOperatorNode.h>
#include <qore/intern/QoreLogicalComparisonOperatorNode.h>
#include <qore/intern/QoreLogicalEqualsOperatorNode.h>
#include <qore/intern/QoreLogicalNotOperatorNode.h>
#include <qore/intern/QoreLogicalOrOperatorNode.h>
#include <qore/intern/QoreLogicalLessThanOperatorNode.h>
#include <qore/intern/QoreLogicalLessThanOrEqualsOperatorNode.h>
#include <qore/intern/QoreLogicalNotEqualsOperatorNode.h>
#include <qore/intern/QoreLogicalGreaterThanOperatorNode.h>
#include <qore/intern/QoreLogicalGreaterThanOrEqualsOperatorNode.h>
#include <qore/intern/QoreModuloOperatorNode.h>
#include <qore/intern/QoreMinusOperatorNode.h>
#include <qore/intern/QoreMultiplicationOperatorNode.h>
#include <qore/intern/QoreNullCoalescingOperatorNode.h>
#include <qore/intern/QorePlusOperatorNode.h>
#include <qore/intern/QoreQuestionMarkOperatorNode.h>
#include <qore/intern/QoreDivisionOperatorNode.h>
#include <qore/intern/QoreFoldlOperatorNode.h>
#include <qore/intern/QoreAssignmentOperatorNode.h>
#include <qore/intern/QoreBinaryAndOperatorNode.h>
#include <qore/intern/QoreBinaryOrOperatorNode.h>
#include <qore/intern/QoreBinaryXorOperatorNode.h>
#include <qore/intern/QoreCastOperatorNode.h>
#include <qore/intern/QoreMapOperatorNode.h>
#include <qore/intern/QorePlusEqualsOperatorNode.h>
#include <qore/intern/QoreMinusEqualsOperatorNode.h>
#include <qore/intern/QoreMultiplyEqualsOperatorNode.h>
#include <qore/intern/QoreDivideEqualsOperatorNode.h>
#include <qore/intern/QoreModuloEqualsOperatorNode.h>
#include <qore/intern/QoreAndEqualsOperatorNode.h>
#include <qore/intern/QoreOrEqualsOperatorNode.h>
#include <qore/intern/QoreXorEqualsOperatorNode.h>
#include <qore/intern/QorePreIncrementOperatorNode.h>
#include <qore/intern/QorePostIncrementOperatorNode.h>
#include <qore/intern/QorePreDecrementOperatorNode.h>
#include <qore/intern/QorePostDecrementOperatorNode.h>
#include <qore/intern/QoreRangeOperatorNode.h>
#include <qore/intern/QoreShiftOperatorNode.h>
#include <qore/intern/QoreShiftLeftOperatorNode.h>
#include <qore/intern/QoreShiftLeftEqualsOperatorNode.h>
#include <qore/intern/QoreShiftRightOperatorNode.h>
#include <qore/intern/QoreShiftRightEqualsOperatorNode.h>
#include <qore/intern/QoreSpliceOperatorNode.h>
#include <qore/intern/QoreUnshiftOperatorNode.h>
#include <qore/intern/QoreUnaryMinusOperatorNode.h>
#include <qore/intern/QoreUnaryPlusOperatorNode.h>
#include <qore/intern/QoreValueCoalescingOperatorNode.h>
#include <qore/intern/QoreExtractOperatorNode.h>
#include <qore/intern/QoreRemoveOperatorNode.h>
#include <qore/intern/QoreKeysOperatorNode.h>
#include <qore/intern/QoreHashObjectDereferenceOperatorNode.h>
#include <qore/intern/QoreSquareBracketsOperatorNode.h>
#include <qore/intern/FunctionCallNode.h>
#include <qore/intern/CallReferenceCallNode.h>
#include <qore/intern/QoreSquareBracketsRangeOperatorNode.h>
#include <qore/intern/VarRefNode.h>
#include <qore/intern/Variable.h>
#include <qore/QoreStringNode.h>

QoreIRLowering::QoreIRLowering(QoreIRBuilder& n_builder, QoreParseContext* n_parse_context)
        : builder(n_builder), parse_context(n_parse_context) {
}

void QoreIRLowering::setParseContext(QoreParseContext* n_parse_context) {
    parse_context = n_parse_context;
}

static bool isIntConstant(const QoreValue& value) {
    return value.isInt();
}

static bool isFloatConstant(const QoreValue& value) {
    return value.isFloat();
}

static bool isRangeLValue(const QoreValue& value) {
    const AbstractQoreNode* node = value.getInternalNode();
    return node && dynamic_cast<const QoreSquareBracketsRangeOperatorNode*>(node);
}

QoreIRValue QoreIRLowering::lowerExpression(const QoreValue& expr, std::string& error) {
    QoreIRValue constant = lowerConstant(expr, error);
    if (constant.isValid()) {
        return constant;
    }
    if (!error.empty()) {
        return QoreIRValue();
    }
    QoreIRValue var_ref = lowerVarRef(expr, error);
    if (var_ref.isValid() || !error.empty()) {
        return var_ref;
    }
    QoreIRValue assign = lowerAssignment(expr, error);
    if (assign.isValid() || !error.empty()) {
        return assign;
    }
    assign = lowerPlusEquals(expr, error);
    if (assign.isValid() || !error.empty()) {
        return assign;
    }
    assign = lowerMinusEquals(expr, error);
    if (assign.isValid() || !error.empty()) {
        return assign;
    }
    assign = lowerDivideEquals(expr, error);
    if (assign.isValid() || !error.empty()) {
        return assign;
    }
    assign = lowerMultiplyEquals(expr, error);
    if (assign.isValid() || !error.empty()) {
        return assign;
    }
    assign = lowerModuloEquals(expr, error);
    if (assign.isValid() || !error.empty()) {
        return assign;
    }
    assign = lowerAndEquals(expr, error);
    if (assign.isValid() || !error.empty()) {
        return assign;
    }
    assign = lowerOrEquals(expr, error);
    if (assign.isValid() || !error.empty()) {
        return assign;
    }
    assign = lowerXorEquals(expr, error);
    if (assign.isValid() || !error.empty()) {
        return assign;
    }
    assign = lowerPreDecrement(expr, error);
    if (assign.isValid() || !error.empty()) {
        return assign;
    }
    assign = lowerPostDecrement(expr, error);
    if (assign.isValid() || !error.empty()) {
        return assign;
    }
    assign = lowerPreIncrement(expr, error);
    if (assign.isValid() || !error.empty()) {
        return assign;
    }
    assign = lowerPostIncrement(expr, error);
    if (assign.isValid() || !error.empty()) {
        return assign;
    }
    QoreIRValue result = lowerPlus(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerMinus(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerLogicalLessThan(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerLogicalNotEquals(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerLogicalEquals(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerLogicalAbsoluteNotEquals(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerLogicalAbsoluteEquals(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerLogicalLessThanOrEquals(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerLogicalGreaterThan(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerLogicalGreaterThanOrEquals(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerLogicalComparison(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerUnaryPlus(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerUnaryMinus(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerMultiplication(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerDivision(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerModulo(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerBinaryAnd(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerBinaryOr(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerBinaryXor(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerShiftLeft(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerShiftRight(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerShiftLeftEquals(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerShiftRightEquals(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerRange(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerSquareBracketsRange(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerSquareBrackets(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerHashObjectDereference(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerShift(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerUnshift(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerSplice(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerExtract(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerRemove(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerKeys(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerFoldr(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerFoldl(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerMap(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerLogicalAnd(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerLogicalOr(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerLogicalNot(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerNullCoalescing(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerValueCoalescing(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerQuestionMark(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerCast(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerFunctionCall(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerCallReference(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerSelfCall(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerStaticCall(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    error = "unsupported expression node for IR lowering";
    return QoreIRValue();
}

bool QoreIRLowering::getAnalysis(const QoreValue& expr, QoreParseAnalysis& analysis) {
    analysis.clear();
    if (expr.hasNode()) {
        const AbstractQoreNode* node = expr.getInternalNode();
        auto* parse_node = dynamic_cast<const ParseNode*>(node);
        if (parse_node) {
            analysis = parse_node->getParseAnalysis();
            return true;
        }
    }
    if (!expr.isNothing()) {
        analysis.setFlag(QoreParseAnalysis::DefinitelyAssigned);
        analysis.setFlag(QoreParseAnalysis::KnownTypeInfo);
        analysis.setFlag(QoreParseAnalysis::NeverNothing);
        analysis.known_type = expr.getFullTypeInfo();
        return true;
    }
    if (parse_context) {
        QoreParseContextAnalysisHelper ah(*parse_context);
        const QoreTypeInfo* saved_type = parse_context->typeInfo;
        parse_context->typeInfo = nullptr;
        QoreValue temp(expr);
        parse_init_value(temp, *parse_context);
        analysis = parse_context->analysis;
        parse_context->typeInfo = saved_type;
        return true;
    }
    return false;
}

bool QoreIRLowering::isNeverNothingInt(const QoreParseAnalysis& analysis) const {
    return analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
        && analysis.hasFlag(QoreParseAnalysis::NeverNothing)
        && QoreTypeInfo::isType(analysis.known_type, NT_INT);
}

bool QoreIRLowering::isNeverNothingFloat(const QoreParseAnalysis& analysis) const {
    return analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
        && analysis.hasFlag(QoreParseAnalysis::NeverNothing)
        && QoreTypeInfo::isType(analysis.known_type, NT_FLOAT);
}

bool QoreIRLowering::ensureBuilderContext(std::string& error) const {
    if (!builder.getFunction()) {
        error = "IR builder has no active function";
        return false;
    }
    if (!builder.getBlock()) {
        error = "IR builder has no active block";
        return false;
    }
    return true;
}

QoreIRBasicBlock* QoreIRLowering::createBlock(const std::string& prefix) {
    QoreIRFunction* func = builder.getFunction();
    if (!func) {
        return nullptr;
    }
    std::string name = prefix + "." + std::to_string(block_counter++);
    return func->createBlock(name);
}

QoreIRValue QoreIRLowering::lowerConstant(const QoreValue& expr, std::string& error) {
    if (expr.isNothing()) {
        return builder.createConstNothing()->result;
    }
    if (expr.isBool()) {
        return builder.createConstBool(expr.getAsBool())->result;
    }
    if (expr.isInt()) {
        return builder.createConstInt(expr.getAsBigInt())->result;
    }
    if (expr.isFloat()) {
        return builder.createConstFloat(expr.getAsFloat())->result;
    }
    if (expr.getType() == NT_STRING && expr.isValue()) {
        QoreStringValueHelper str(expr);
        const char* buf = str->getBuffer();
        return builder.createConstString(std::string(buf ? buf : "", str->size()))->result;
    }
    if (expr.getType() == NT_DATE && expr.isValue()) {
        const DateTimeNode* dt = expr.get<const DateTimeNode>();
        bool is_relative = dt->isRelative();
        int64_t micros = is_relative ? dt->getRelativeMicroseconds() : dt->getEpochMicrosecondsUTC();
        return builder.createConstDate(micros, is_relative)->result;
    }
    if (expr.isNull()) {
        error = "null constant lowering not implemented";
        return QoreIRValue();
    }
    return QoreIRValue();
}

QoreIRValue QoreIRLowering::lowerVarRef(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* var = dynamic_cast<const VarRefNode*>(node);
    if (!var) {
        return QoreIRValue();
    }
    return loadVarRef(var, error, "variable reference");
}

QoreIRValue QoreIRLowering::loadVarRef(const VarRefNode* var, std::string& error, const char* context) {
    if (!var) {
        error = std::string("null lvalue in IR lowering (") + context + ")";
        return QoreIRValue();
    }
    switch (var->getType()) {
        case VT_LOCAL:
            if (!var->ref.id) {
                error = std::string("unresolved local variable reference in IR lowering (") + context + ")";
                return QoreIRValue();
            }
            return builder.createLoadLocal(var->ref.id, var->loc)->result;
        case VT_CLOSURE:
        case VT_LOCAL_TS:
            if (!var->ref.id) {
                error = std::string("unresolved closure variable reference in IR lowering (") + context + ")";
                return QoreIRValue();
            }
            return builder.createLoadClosure(var->ref.id, var->loc)->result;
        case VT_GLOBAL:
            if (!var->ref.var) {
                error = std::string("unresolved global variable reference in IR lowering (") + context + ")";
                return QoreIRValue();
            }
            return builder.createLoadGlobal(var->ref.var, var->loc)->result;
        case VT_THREAD_LOCAL:
            if (!var->ref.var) {
                error = std::string("unresolved thread-local variable reference in IR lowering (") + context + ")";
                return QoreIRValue();
            }
            return builder.createLoadThreadLocal(var->ref.var, var->loc)->result;
        default:
            break;
    }
    error = std::string("unsupported variable reference for IR lowering (") + context + ")";
    return QoreIRValue();
}

bool QoreIRLowering::storeVarRef(const VarRefNode* var, QoreIRValue value, std::string& error,
        const char* context) {
    if (!var) {
        error = std::string("null lvalue in IR lowering (") + context + ")";
        return false;
    }
    switch (var->getType()) {
        case VT_LOCAL:
            if (!var->ref.id) {
                error = std::string("unresolved local variable reference in IR lowering (") + context + ")";
                return false;
            }
            builder.createStoreLocal(var->ref.id, value, var->loc);
            return true;
        case VT_CLOSURE:
        case VT_LOCAL_TS:
            if (!var->ref.id) {
                error = std::string("unresolved closure variable reference in IR lowering (") + context + ")";
                return false;
            }
            builder.createStoreClosure(var->ref.id, value, var->loc);
            return true;
        case VT_GLOBAL:
            if (!var->ref.var) {
                error = std::string("unresolved global variable reference in IR lowering (") + context + ")";
                return false;
            }
            builder.createStoreGlobal(var->ref.var, value, var->loc);
            return true;
        case VT_THREAD_LOCAL:
            if (!var->ref.var) {
                error = std::string("unresolved thread-local variable reference in IR lowering (") + context + ")";
                return false;
            }
            builder.createStoreThreadLocal(var->ref.var, value, var->loc);
            return true;
        default:
            break;
    }
    error = std::string("unsupported variable reference for IR lowering (") + context + ")";
    return false;
}

QoreIRValue QoreIRLowering::lowerAssignment(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* assign = dynamic_cast<const QoreAssignmentOperatorNode*>(node);
    if (!assign) {
        auto* weak = dynamic_cast<const QoreWeakAssignmentOperatorNode*>(node);
        if (!weak) {
            return QoreIRValue();
        }
        assign = weak;
    }

    const AbstractQoreNode* left_node = assign->getLeft().getInternalNode();
    auto* left_var = dynamic_cast<const VarRefNode*>(left_node);
    QoreIRValue right = lowerExpression(assign->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    if (left_var) {
        if (!storeVarRef(left_var, right, error, "assignment")) {
            return QoreIRValue();
        }
    } else if (assign->getLeft().hasNode()) {
        if (isRangeLValue(assign->getLeft())) {
            error = "unsupported lvalue range for assignment IR lowering";
            return QoreIRValue();
        }
        builder.createStoreLValue(assign->getLeft(), right, assign->loc);
    } else {
        error = "unsupported lvalue for assignment IR lowering";
        return QoreIRValue();
    }
    return right;
}

QoreIRValue QoreIRLowering::lowerPlusEquals(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QorePlusEqualsOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    const AbstractQoreNode* left_node = op->getLeft().getInternalNode();
    auto* left_var = dynamic_cast<const VarRefNode*>(left_node);
    QoreIRValue right = lowerExpression(op->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    if (!left_var) {
        if (!op->getLeft().hasNode()) {
            error = "unsupported lvalue for plus-equals IR lowering";
            return QoreIRValue();
        }
        if (isRangeLValue(op->getLeft())) {
            error = "unsupported lvalue range for plus-equals IR lowering";
            return QoreIRValue();
        }
        return builder.createLValueBinaryOp(QoreIROpcode::AddAssignLValue, op->getLeft(), right, op->loc)->result;
    }
    QoreIRValue left_value = loadVarRef(left_var, error, "plus-equals");
    if (!left_value.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = QoreIROpcode::AddAssignAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if ((isIntConstant(op->getLeft()) && isIntConstant(op->getRight()))
        || (getAnalysis(op->getLeft(), left_analysis)
            && getAnalysis(op->getRight(), right_analysis)
            && isNeverNothingInt(left_analysis)
            && isNeverNothingInt(right_analysis))) {
        opcode = QoreIROpcode::AddAssignInt;
    }
    QoreIRValue result = builder.createBinaryOp(opcode, left_value, right)->result;
    if (!storeVarRef(left_var, result, error, "plus-equals")) {
        return QoreIRValue();
    }
    return result;
}

QoreIRValue QoreIRLowering::lowerMinusEquals(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreMinusEqualsOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    const AbstractQoreNode* left_node = op->getLeft().getInternalNode();
    auto* left_var = dynamic_cast<const VarRefNode*>(left_node);
    QoreIRValue right = lowerExpression(op->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    if (!left_var) {
        if (!op->getLeft().hasNode()) {
            error = "unsupported lvalue for minus-equals IR lowering";
            return QoreIRValue();
        }
        if (isRangeLValue(op->getLeft())) {
            error = "unsupported lvalue range for minus-equals IR lowering";
            return QoreIRValue();
        }
        return builder.createLValueBinaryOp(QoreIROpcode::SubAssignLValue, op->getLeft(), right, op->loc)->result;
    }
    QoreIRValue left_value = loadVarRef(left_var, error, "minus-equals");
    if (!left_value.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = QoreIROpcode::SubAssignAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if ((isIntConstant(op->getLeft()) && isIntConstant(op->getRight()))
        || (getAnalysis(op->getLeft(), left_analysis)
            && getAnalysis(op->getRight(), right_analysis)
            && isNeverNothingInt(left_analysis)
            && isNeverNothingInt(right_analysis))) {
        opcode = QoreIROpcode::SubAssignInt;
    }
    QoreIRValue result = builder.createBinaryOp(opcode, left_value, right)->result;
    if (!storeVarRef(left_var, result, error, "minus-equals")) {
        return QoreIRValue();
    }
    return result;
}

QoreIRValue QoreIRLowering::lowerMultiplyEquals(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreMultiplyEqualsOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    const AbstractQoreNode* left_node = op->getLeft().getInternalNode();
    auto* left_var = dynamic_cast<const VarRefNode*>(left_node);
    QoreIRValue right = lowerExpression(op->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    if (!left_var) {
        if (!op->getLeft().hasNode()) {
            error = "unsupported lvalue for multiply-equals IR lowering";
            return QoreIRValue();
        }
        if (isRangeLValue(op->getLeft())) {
            error = "unsupported lvalue range for multiply-equals IR lowering";
            return QoreIRValue();
        }
        return builder.createLValueBinaryOp(QoreIROpcode::MulAssignLValue, op->getLeft(), right, op->loc)->result;
    }
    QoreIRValue left_value = loadVarRef(left_var, error, "multiply-equals");
    if (!left_value.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = QoreIROpcode::MulAssignAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if ((isIntConstant(op->getLeft()) && isIntConstant(op->getRight()))
        || (getAnalysis(op->getLeft(), left_analysis)
            && getAnalysis(op->getRight(), right_analysis)
            && isNeverNothingInt(left_analysis)
            && isNeverNothingInt(right_analysis))) {
        opcode = QoreIROpcode::MulAssignInt;
    }
    QoreIRValue result = builder.createBinaryOp(opcode, left_value, right)->result;
    if (!storeVarRef(left_var, result, error, "multiply-equals")) {
        return QoreIRValue();
    }
    return result;
}

QoreIRValue QoreIRLowering::lowerDivideEquals(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreDivideEqualsOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    const AbstractQoreNode* left_node = op->getLeft().getInternalNode();
    auto* left_var = dynamic_cast<const VarRefNode*>(left_node);
    QoreIRValue right = lowerExpression(op->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    if (!left_var) {
        if (!op->getLeft().hasNode()) {
            error = "unsupported lvalue for divide-equals IR lowering";
            return QoreIRValue();
        }
        if (isRangeLValue(op->getLeft())) {
            error = "unsupported lvalue range for divide-equals IR lowering";
            return QoreIRValue();
        }
        return builder.createLValueBinaryOp(QoreIROpcode::DivAssignLValue, op->getLeft(), right, op->loc)->result;
    }
    QoreIRValue left_value = loadVarRef(left_var, error, "divide-equals");
    if (!left_value.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = QoreIROpcode::DivAssignAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if ((isIntConstant(op->getLeft()) && isIntConstant(op->getRight()))
        || (getAnalysis(op->getLeft(), left_analysis)
            && getAnalysis(op->getRight(), right_analysis)
            && isNeverNothingInt(left_analysis)
            && isNeverNothingInt(right_analysis))) {
        opcode = QoreIROpcode::DivAssignInt;
    }
    QoreIRValue result = builder.createBinaryOp(opcode, left_value, right)->result;
    if (!storeVarRef(left_var, result, error, "divide-equals")) {
        return QoreIRValue();
    }
    return result;
}

QoreIRValue QoreIRLowering::lowerModuloEquals(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreModuloEqualsOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    const AbstractQoreNode* left_node = op->getLeft().getInternalNode();
    auto* left_var = dynamic_cast<const VarRefNode*>(left_node);
    QoreIRValue right = lowerExpression(op->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    if (!left_var) {
        if (!op->getLeft().hasNode()) {
            error = "unsupported lvalue for modulo-equals IR lowering";
            return QoreIRValue();
        }
        if (isRangeLValue(op->getLeft())) {
            error = "unsupported lvalue range for modulo-equals IR lowering";
            return QoreIRValue();
        }
        return builder.createLValueBinaryOp(QoreIROpcode::ModAssignLValue, op->getLeft(), right, op->loc)->result;
    }
    QoreIRValue left_value = loadVarRef(left_var, error, "modulo-equals");
    if (!left_value.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = QoreIROpcode::ModAssignAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if ((isIntConstant(op->getLeft()) && isIntConstant(op->getRight()))
        || (getAnalysis(op->getLeft(), left_analysis)
            && getAnalysis(op->getRight(), right_analysis)
            && isNeverNothingInt(left_analysis)
            && isNeverNothingInt(right_analysis))) {
        opcode = QoreIROpcode::ModAssignInt;
    }
    QoreIRValue result = builder.createBinaryOp(opcode, left_value, right)->result;
    if (!storeVarRef(left_var, result, error, "modulo-equals")) {
        return QoreIRValue();
    }
    return result;
}

QoreIRValue QoreIRLowering::lowerAndEquals(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreAndEqualsOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    const AbstractQoreNode* left_node = op->getLeft().getInternalNode();
    auto* left_var = dynamic_cast<const VarRefNode*>(left_node);
    QoreIRValue right = lowerExpression(op->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    if (!left_var) {
        if (!op->getLeft().hasNode()) {
            error = "unsupported lvalue for and-equals IR lowering";
            return QoreIRValue();
        }
        if (isRangeLValue(op->getLeft())) {
            error = "unsupported lvalue range for and-equals IR lowering";
            return QoreIRValue();
        }
        return builder.createLValueBinaryOp(QoreIROpcode::AndAssignLValue, op->getLeft(), right, op->loc)->result;
    }
    QoreIRValue left_value = loadVarRef(left_var, error, "and-equals");
    if (!left_value.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = QoreIROpcode::AndAssignAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if ((isIntConstant(op->getLeft()) && isIntConstant(op->getRight()))
        || (getAnalysis(op->getLeft(), left_analysis)
            && getAnalysis(op->getRight(), right_analysis)
            && isNeverNothingInt(left_analysis)
            && isNeverNothingInt(right_analysis))) {
        opcode = QoreIROpcode::AndAssignInt;
    }
    QoreIRValue result = builder.createBinaryOp(opcode, left_value, right)->result;
    if (!storeVarRef(left_var, result, error, "and-equals")) {
        return QoreIRValue();
    }
    return result;
}

QoreIRValue QoreIRLowering::lowerOrEquals(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreOrEqualsOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    const AbstractQoreNode* left_node = op->getLeft().getInternalNode();
    auto* left_var = dynamic_cast<const VarRefNode*>(left_node);
    QoreIRValue right = lowerExpression(op->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    if (!left_var) {
        if (!op->getLeft().hasNode()) {
            error = "unsupported lvalue for or-equals IR lowering";
            return QoreIRValue();
        }
        if (isRangeLValue(op->getLeft())) {
            error = "unsupported lvalue range for or-equals IR lowering";
            return QoreIRValue();
        }
        return builder.createLValueBinaryOp(QoreIROpcode::OrAssignLValue, op->getLeft(), right, op->loc)->result;
    }
    QoreIRValue left_value = loadVarRef(left_var, error, "or-equals");
    if (!left_value.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = QoreIROpcode::OrAssignAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if ((isIntConstant(op->getLeft()) && isIntConstant(op->getRight()))
        || (getAnalysis(op->getLeft(), left_analysis)
            && getAnalysis(op->getRight(), right_analysis)
            && isNeverNothingInt(left_analysis)
            && isNeverNothingInt(right_analysis))) {
        opcode = QoreIROpcode::OrAssignInt;
    }
    QoreIRValue result = builder.createBinaryOp(opcode, left_value, right)->result;
    if (!storeVarRef(left_var, result, error, "or-equals")) {
        return QoreIRValue();
    }
    return result;
}

QoreIRValue QoreIRLowering::lowerXorEquals(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreXorEqualsOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    const AbstractQoreNode* left_node = op->getLeft().getInternalNode();
    auto* left_var = dynamic_cast<const VarRefNode*>(left_node);
    QoreIRValue right = lowerExpression(op->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    if (!left_var) {
        if (!op->getLeft().hasNode()) {
            error = "unsupported lvalue for xor-equals IR lowering";
            return QoreIRValue();
        }
        if (isRangeLValue(op->getLeft())) {
            error = "unsupported lvalue range for xor-equals IR lowering";
            return QoreIRValue();
        }
        return builder.createLValueBinaryOp(QoreIROpcode::XorAssignLValue, op->getLeft(), right, op->loc)->result;
    }
    QoreIRValue left_value = loadVarRef(left_var, error, "xor-equals");
    if (!left_value.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = QoreIROpcode::XorAssignAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if ((isIntConstant(op->getLeft()) && isIntConstant(op->getRight()))
        || (getAnalysis(op->getLeft(), left_analysis)
            && getAnalysis(op->getRight(), right_analysis)
            && isNeverNothingInt(left_analysis)
            && isNeverNothingInt(right_analysis))) {
        opcode = QoreIROpcode::XorAssignInt;
    }
    QoreIRValue result = builder.createBinaryOp(opcode, left_value, right)->result;
    if (!storeVarRef(left_var, result, error, "xor-equals")) {
        return QoreIRValue();
    }
    return result;
}

QoreIRValue QoreIRLowering::lowerPreIncrement(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QorePreIncrementOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    QoreValue lvexp = op->getExp();
    if (!lvexp.hasNode()) {
        error = "unsupported lvalue for pre-increment IR lowering";
        return QoreIRValue();
    }
    if (isRangeLValue(lvexp)) {
        error = "unsupported lvalue range for pre-increment IR lowering";
        return QoreIRValue();
    }
    return builder.createLValueUnaryOp(QoreIROpcode::PreIncLValue, lvexp, op->loc)->result;
}

QoreIRValue QoreIRLowering::lowerPostIncrement(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QorePostIncrementOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    QoreValue lvexp = op->getExp();
    if (!lvexp.hasNode()) {
        error = "unsupported lvalue for post-increment IR lowering";
        return QoreIRValue();
    }
    if (isRangeLValue(lvexp)) {
        error = "unsupported lvalue range for post-increment IR lowering";
        return QoreIRValue();
    }
    return builder.createLValueUnaryOp(QoreIROpcode::PostIncLValue, lvexp, op->loc)->result;
}

QoreIRValue QoreIRLowering::lowerPreDecrement(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QorePreDecrementOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    QoreValue lvexp = op->getExp();
    if (!lvexp.hasNode()) {
        error = "unsupported lvalue for pre-decrement IR lowering";
        return QoreIRValue();
    }
    if (isRangeLValue(lvexp)) {
        error = "unsupported lvalue range for pre-decrement IR lowering";
        return QoreIRValue();
    }
    return builder.createLValueUnaryOp(QoreIROpcode::PreDecLValue, lvexp, op->loc)->result;
}

QoreIRValue QoreIRLowering::lowerPostDecrement(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QorePostDecrementOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    QoreValue lvexp = op->getExp();
    if (!lvexp.hasNode()) {
        error = "unsupported lvalue for post-decrement IR lowering";
        return QoreIRValue();
    }
    if (isRangeLValue(lvexp)) {
        error = "unsupported lvalue range for post-decrement IR lowering";
        return QoreIRValue();
    }
    return builder.createLValueUnaryOp(QoreIROpcode::PostDecLValue, lvexp, op->loc)->result;
}

QoreIRValue QoreIRLowering::lowerPlus(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* plus = dynamic_cast<const QorePlusOperatorNode*>(node);
    if (!plus) {
        return QoreIRValue();
    }

    QoreIRValue left = lowerExpression(plus->getLeft(), error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue right = lowerExpression(plus->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode op = QoreIROpcode::AddAny;
    if (isIntConstant(plus->getLeft()) && isIntConstant(plus->getRight())) {
        op = QoreIROpcode::AddInt;
    } else if (isFloatConstant(plus->getLeft()) && isFloatConstant(plus->getRight())) {
        op = QoreIROpcode::AddFloat;
    } else {
        QoreParseAnalysis left_analysis;
        QoreParseAnalysis right_analysis;
        if (getAnalysis(plus->getLeft(), left_analysis)
            && getAnalysis(plus->getRight(), right_analysis)
        ) {
            if (isNeverNothingInt(left_analysis) && isNeverNothingInt(right_analysis)) {
                op = QoreIROpcode::AddInt;
            } else if (isNeverNothingFloat(left_analysis) && isNeverNothingFloat(right_analysis)) {
                op = QoreIROpcode::AddFloat;
            }
        }
    }
    return builder.createBinaryOp(op, left, right)->result;
}

QoreIRValue QoreIRLowering::lowerMinus(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* minus = dynamic_cast<const QoreMinusOperatorNode*>(node);
    if (!minus) {
        return QoreIRValue();
    }

    QoreIRValue left = lowerExpression(minus->getLeft(), error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue right = lowerExpression(minus->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode op = QoreIROpcode::SubAny;
    if (isIntConstant(minus->getLeft()) && isIntConstant(minus->getRight())) {
        op = QoreIROpcode::SubInt;
    } else if (isFloatConstant(minus->getLeft()) && isFloatConstant(minus->getRight())) {
        op = QoreIROpcode::SubFloat;
    } else {
        QoreParseAnalysis left_analysis;
        QoreParseAnalysis right_analysis;
        if (getAnalysis(minus->getLeft(), left_analysis)
            && getAnalysis(minus->getRight(), right_analysis)
        ) {
            if (isNeverNothingInt(left_analysis) && isNeverNothingInt(right_analysis)) {
                op = QoreIROpcode::SubInt;
            } else if (isNeverNothingFloat(left_analysis) && isNeverNothingFloat(right_analysis)) {
                op = QoreIROpcode::SubFloat;
            }
        }
    }
    return builder.createBinaryOp(op, left, right)->result;
}

QoreIRValue QoreIRLowering::lowerLogicalEquals(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* eq = dynamic_cast<const QoreLogicalEqualsOperatorNode*>(node);
    if (!eq) {
        return QoreIRValue();
    }

    QoreIRValue left = lowerExpression(eq->getLeft(), error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue right = lowerExpression(eq->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode op = QoreIROpcode::EqAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if (getAnalysis(eq->getLeft(), left_analysis)
        && getAnalysis(eq->getRight(), right_analysis)
        && isNeverNothingInt(left_analysis)
        && isNeverNothingInt(right_analysis)) {
        op = QoreIROpcode::EqInt;
    }
    return builder.createBinaryOp(op, left, right)->result;
}

QoreIRValue QoreIRLowering::lowerLogicalNotEquals(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* ne = dynamic_cast<const QoreLogicalNotEqualsOperatorNode*>(node);
    if (!ne) {
        return QoreIRValue();
    }

    QoreIRValue left = lowerExpression(ne->getLeft(), error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue right = lowerExpression(ne->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode op = QoreIROpcode::NeAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if (getAnalysis(ne->getLeft(), left_analysis)
        && getAnalysis(ne->getRight(), right_analysis)
        && isNeverNothingInt(left_analysis)
        && isNeverNothingInt(right_analysis)) {
        op = QoreIROpcode::NeInt;
    }
    return builder.createBinaryOp(op, left, right)->result;
}

QoreIRValue QoreIRLowering::lowerLogicalAbsoluteEquals(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* eq = dynamic_cast<const QoreLogicalAbsoluteEqualsOperatorNode*>(node);
    if (!eq) {
        return QoreIRValue();
    }

    QoreIRValue left = lowerExpression(eq->getLeft(), error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue right = lowerExpression(eq->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    return builder.createBinaryOp(QoreIROpcode::EqHard, left, right)->result;
}

QoreIRValue QoreIRLowering::lowerLogicalAbsoluteNotEquals(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* ne = dynamic_cast<const QoreLogicalAbsoluteNotEqualsOperatorNode*>(node);
    if (!ne) {
        return QoreIRValue();
    }

    QoreIRValue left = lowerExpression(ne->getLeft(), error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue right = lowerExpression(ne->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    return builder.createBinaryOp(QoreIROpcode::NeHard, left, right)->result;
}

QoreIRValue QoreIRLowering::lowerLogicalLessThan(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* lt = dynamic_cast<const QoreLogicalLessThanOperatorNode*>(node);
    if (!lt) {
        return QoreIRValue();
    }

    QoreIRValue left = lowerExpression(lt->getLeft(), error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue right = lowerExpression(lt->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode op = QoreIROpcode::LtAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if (getAnalysis(lt->getLeft(), left_analysis)
        && getAnalysis(lt->getRight(), right_analysis)) {
        if (isNeverNothingInt(left_analysis) && isNeverNothingInt(right_analysis)) {
            op = QoreIROpcode::LtInt;
        } else if (isNeverNothingFloat(left_analysis) && isNeverNothingFloat(right_analysis)) {
            op = QoreIROpcode::LtFloat;
        }
    }
    return builder.createBinaryOp(op, left, right)->result;
}

QoreIRValue QoreIRLowering::lowerLogicalLessThanOrEquals(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* le = dynamic_cast<const QoreLogicalLessThanOrEqualsOperatorNode*>(node);
    if (!le) {
        return QoreIRValue();
    }

    QoreIRValue left = lowerExpression(le->getLeft(), error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue right = lowerExpression(le->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode op = QoreIROpcode::LeAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if (getAnalysis(le->getLeft(), left_analysis)
        && getAnalysis(le->getRight(), right_analysis)) {
        if (isNeverNothingInt(left_analysis) && isNeverNothingInt(right_analysis)) {
            op = QoreIROpcode::LeInt;
        } else if (isNeverNothingFloat(left_analysis) && isNeverNothingFloat(right_analysis)) {
            op = QoreIROpcode::LeFloat;
        }
    }
    return builder.createBinaryOp(op, left, right)->result;
}

QoreIRValue QoreIRLowering::lowerLogicalGreaterThan(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* gt = dynamic_cast<const QoreLogicalGreaterThanOperatorNode*>(node);
    if (!gt) {
        return QoreIRValue();
    }

    QoreIRValue left = lowerExpression(gt->getLeft(), error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue right = lowerExpression(gt->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode op = QoreIROpcode::GtAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if (getAnalysis(gt->getLeft(), left_analysis)
        && getAnalysis(gt->getRight(), right_analysis)) {
        if (isNeverNothingInt(left_analysis) && isNeverNothingInt(right_analysis)) {
            op = QoreIROpcode::GtInt;
        } else if (isNeverNothingFloat(left_analysis) && isNeverNothingFloat(right_analysis)) {
            op = QoreIROpcode::GtFloat;
        }
    }
    return builder.createBinaryOp(op, left, right)->result;
}

QoreIRValue QoreIRLowering::lowerLogicalGreaterThanOrEquals(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* ge = dynamic_cast<const QoreLogicalGreaterThanOrEqualsOperatorNode*>(node);
    if (!ge) {
        return QoreIRValue();
    }

    QoreIRValue left = lowerExpression(ge->getLeft(), error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue right = lowerExpression(ge->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode op = QoreIROpcode::GeAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if (getAnalysis(ge->getLeft(), left_analysis)
        && getAnalysis(ge->getRight(), right_analysis)) {
        if (isNeverNothingInt(left_analysis) && isNeverNothingInt(right_analysis)) {
            op = QoreIROpcode::GeInt;
        } else if (isNeverNothingFloat(left_analysis) && isNeverNothingFloat(right_analysis)) {
            op = QoreIROpcode::GeFloat;
        }
    }
    return builder.createBinaryOp(op, left, right)->result;
}

QoreIRValue QoreIRLowering::lowerLogicalComparison(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* cmp = dynamic_cast<const QoreLogicalComparisonOperatorNode*>(node);
    if (!cmp) {
        return QoreIRValue();
    }

    QoreIRValue left = lowerExpression(cmp->getLeft(), error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue right = lowerExpression(cmp->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode op = QoreIROpcode::CmpAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if (getAnalysis(cmp->getLeft(), left_analysis)
        && getAnalysis(cmp->getRight(), right_analysis)) {
        if (isNeverNothingInt(left_analysis) && isNeverNothingInt(right_analysis)) {
            op = QoreIROpcode::CmpInt;
        } else if (isNeverNothingFloat(left_analysis) && isNeverNothingFloat(right_analysis)) {
            op = QoreIROpcode::CmpFloat;
        }
    }
    return builder.createBinaryOp(op, left, right)->result;
}

QoreIRValue QoreIRLowering::lowerUnaryPlus(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* plus = dynamic_cast<const QoreUnaryPlusOperatorNode*>(node);
    if (!plus) {
        return QoreIRValue();
    }
    if (!ensureBuilderContext(error)) {
        return QoreIRValue();
    }

    QoreIRValue value = lowerExpression(plus->getExp(), error);
    if (!value.isValid()) {
        return QoreIRValue();
    }
    return builder.createUnaryOp(QoreIROpcode::UnaryPlusAny, value)->result;
}

QoreIRValue QoreIRLowering::lowerUnaryMinus(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* minus = dynamic_cast<const QoreUnaryMinusOperatorNode*>(node);
    if (!minus) {
        return QoreIRValue();
    }
    if (!ensureBuilderContext(error)) {
        return QoreIRValue();
    }

    QoreIRValue value = lowerExpression(minus->getExp(), error);
    if (!value.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode op = QoreIROpcode::UnaryMinusAny;
    QoreParseAnalysis analysis;
    if (getAnalysis(minus->getExp(), analysis)) {
        if (isNeverNothingInt(analysis)) {
            op = QoreIROpcode::UnaryMinusInt;
        } else if (isNeverNothingFloat(analysis)) {
            op = QoreIROpcode::UnaryMinusFloat;
        }
    }
    return builder.createUnaryOp(op, value)->result;
}

QoreIRValue QoreIRLowering::lowerMultiplication(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* mul = dynamic_cast<const QoreMultiplicationOperatorNode*>(node);
    if (!mul) {
        return QoreIRValue();
    }

    QoreIRValue left = lowerExpression(mul->getLeft(), error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue right = lowerExpression(mul->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode op = QoreIROpcode::MulAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if (getAnalysis(mul->getLeft(), left_analysis)
        && getAnalysis(mul->getRight(), right_analysis)) {
        if (isNeverNothingInt(left_analysis) && isNeverNothingInt(right_analysis)) {
            op = QoreIROpcode::MulInt;
        } else if (isNeverNothingFloat(left_analysis) && isNeverNothingFloat(right_analysis)) {
            op = QoreIROpcode::MulFloat;
        }
    }
    return builder.createBinaryOp(op, left, right)->result;
}

QoreIRValue QoreIRLowering::lowerDivision(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* div = dynamic_cast<const QoreDivisionOperatorNode*>(node);
    if (!div) {
        return QoreIRValue();
    }

    QoreIRValue left = lowerExpression(div->getLeft(), error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue right = lowerExpression(div->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode op = QoreIROpcode::DivAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if (getAnalysis(div->getLeft(), left_analysis)
        && getAnalysis(div->getRight(), right_analysis)) {
        if (isNeverNothingInt(left_analysis) && isNeverNothingInt(right_analysis)) {
            op = QoreIROpcode::DivInt;
        } else if (isNeverNothingFloat(left_analysis) && isNeverNothingFloat(right_analysis)) {
            op = QoreIROpcode::DivFloat;
        }
    }
    return builder.createBinaryOp(op, left, right)->result;
}

QoreIRValue QoreIRLowering::lowerModulo(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* mod = dynamic_cast<const QoreModuloOperatorNode*>(node);
    if (!mod) {
        return QoreIRValue();
    }

    QoreIRValue left = lowerExpression(mod->getLeft(), error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue right = lowerExpression(mod->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode op = QoreIROpcode::ModAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if (getAnalysis(mod->getLeft(), left_analysis)
        && getAnalysis(mod->getRight(), right_analysis)
        && isNeverNothingInt(left_analysis)
        && isNeverNothingInt(right_analysis)) {
        op = QoreIROpcode::ModInt;
    }
    return builder.createBinaryOp(op, left, right)->result;
}

QoreIRValue QoreIRLowering::lowerBinaryAnd(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreBinaryAndOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }

    QoreIRValue left = lowerExpression(op->getLeft(), error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue right = lowerExpression(op->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = QoreIROpcode::AndAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if ((isIntConstant(op->getLeft()) && isIntConstant(op->getRight()))
        || (getAnalysis(op->getLeft(), left_analysis)
            && getAnalysis(op->getRight(), right_analysis)
            && isNeverNothingInt(left_analysis)
            && isNeverNothingInt(right_analysis))) {
        opcode = QoreIROpcode::AndInt;
    }
    return builder.createBinaryOp(opcode, left, right)->result;
}

QoreIRValue QoreIRLowering::lowerBinaryOr(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreBinaryOrOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }

    QoreIRValue left = lowerExpression(op->getLeft(), error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue right = lowerExpression(op->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = QoreIROpcode::OrAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if ((isIntConstant(op->getLeft()) && isIntConstant(op->getRight()))
        || (getAnalysis(op->getLeft(), left_analysis)
            && getAnalysis(op->getRight(), right_analysis)
            && isNeverNothingInt(left_analysis)
            && isNeverNothingInt(right_analysis))) {
        opcode = QoreIROpcode::OrInt;
    }
    return builder.createBinaryOp(opcode, left, right)->result;
}

QoreIRValue QoreIRLowering::lowerBinaryXor(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreBinaryXorOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }

    QoreIRValue left = lowerExpression(op->getLeft(), error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue right = lowerExpression(op->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = QoreIROpcode::XorAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if ((isIntConstant(op->getLeft()) && isIntConstant(op->getRight()))
        || (getAnalysis(op->getLeft(), left_analysis)
            && getAnalysis(op->getRight(), right_analysis)
            && isNeverNothingInt(left_analysis)
            && isNeverNothingInt(right_analysis))) {
        opcode = QoreIROpcode::XorInt;
    }
    return builder.createBinaryOp(opcode, left, right)->result;
}

QoreIRValue QoreIRLowering::lowerShiftLeft(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreShiftLeftOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }

    QoreIRValue left = lowerExpression(op->getLeft(), error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue right = lowerExpression(op->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = QoreIROpcode::ShlAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if ((isIntConstant(op->getLeft()) && isIntConstant(op->getRight()))
        || (getAnalysis(op->getLeft(), left_analysis)
            && getAnalysis(op->getRight(), right_analysis)
            && isNeverNothingInt(left_analysis)
            && isNeverNothingInt(right_analysis))) {
        opcode = QoreIROpcode::ShlInt;
    }
    return builder.createBinaryOp(opcode, left, right)->result;
}

QoreIRValue QoreIRLowering::lowerShiftRight(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreShiftRightOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }

    QoreIRValue left = lowerExpression(op->getLeft(), error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue right = lowerExpression(op->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = QoreIROpcode::ShrAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if ((isIntConstant(op->getLeft()) && isIntConstant(op->getRight()))
        || (getAnalysis(op->getLeft(), left_analysis)
            && getAnalysis(op->getRight(), right_analysis)
            && isNeverNothingInt(left_analysis)
            && isNeverNothingInt(right_analysis))) {
        opcode = QoreIROpcode::ShrInt;
    }
    return builder.createBinaryOp(opcode, left, right)->result;
}

QoreIRValue QoreIRLowering::lowerShiftLeftEquals(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreShiftLeftEqualsOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }

    const AbstractQoreNode* left_node = op->getLeft().getInternalNode();
    auto* left_var = dynamic_cast<const VarRefNode*>(left_node);
    QoreIRValue right = lowerExpression(op->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    if (!left_var) {
        if (!op->getLeft().hasNode()) {
            error = "unsupported lvalue for shift-left-assign IR lowering";
            return QoreIRValue();
        }
        if (isRangeLValue(op->getLeft())) {
            error = "unsupported lvalue range for shift-left-assign IR lowering";
            return QoreIRValue();
        }
        return builder.createLValueBinaryOp(QoreIROpcode::ShlAssignLValue, op->getLeft(), right, op->loc)->result;
    }
    QoreIRValue left_value = loadVarRef(left_var, error, "shift-left-assign");
    if (!left_value.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = QoreIROpcode::ShlAssignAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if ((isIntConstant(op->getLeft()) && isIntConstant(op->getRight()))
        || (getAnalysis(op->getLeft(), left_analysis)
            && getAnalysis(op->getRight(), right_analysis)
            && isNeverNothingInt(left_analysis)
            && isNeverNothingInt(right_analysis))) {
        opcode = QoreIROpcode::ShlAssignInt;
    }
    QoreIRValue result = builder.createBinaryOp(opcode, left_value, right)->result;
    if (!storeVarRef(left_var, result, error, "shift-left-assign")) {
        return QoreIRValue();
    }
    return result;
}

QoreIRValue QoreIRLowering::lowerShiftRightEquals(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreShiftRightEqualsOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }

    const AbstractQoreNode* left_node = op->getLeft().getInternalNode();
    auto* left_var = dynamic_cast<const VarRefNode*>(left_node);
    QoreIRValue right = lowerExpression(op->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    if (!left_var) {
        if (!op->getLeft().hasNode()) {
            error = "unsupported lvalue for shift-right-assign IR lowering";
            return QoreIRValue();
        }
        if (isRangeLValue(op->getLeft())) {
            error = "unsupported lvalue range for shift-right-assign IR lowering";
            return QoreIRValue();
        }
        return builder.createLValueBinaryOp(QoreIROpcode::ShrAssignLValue, op->getLeft(), right, op->loc)->result;
    }
    QoreIRValue left_value = loadVarRef(left_var, error, "shift-right-assign");
    if (!left_value.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = QoreIROpcode::ShrAssignAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if ((isIntConstant(op->getLeft()) && isIntConstant(op->getRight()))
        || (getAnalysis(op->getLeft(), left_analysis)
            && getAnalysis(op->getRight(), right_analysis)
            && isNeverNothingInt(left_analysis)
            && isNeverNothingInt(right_analysis))) {
        opcode = QoreIROpcode::ShrAssignInt;
    }
    QoreIRValue result = builder.createBinaryOp(opcode, left_value, right)->result;
    if (!storeVarRef(left_var, result, error, "shift-right-assign")) {
        return QoreIRValue();
    }
    return result;
}

QoreIRValue QoreIRLowering::lowerRange(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreRangeOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }

    QoreIRValue left = lowerExpression(op->getLeft(), error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue right = lowerExpression(op->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    return builder.createBinaryOp(QoreIROpcode::RangeAny, left, right, op->loc)->result;
}

QoreIRValue QoreIRLowering::lowerSquareBracketsRange(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreSquareBracketsRangeOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }

    QoreIRValue seq = lowerExpression(op->get(0), error);
    if (!seq.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue start = lowerExpression(op->get(1), error);
    if (!start.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue end = lowerExpression(op->get(2), error);
    if (!end.isValid()) {
        return QoreIRValue();
    }
    return builder.createTernaryOp(QoreIROpcode::RangeSliceAny, seq, start, end, op->loc)->result;
}

QoreIRValue QoreIRLowering::lowerSquareBrackets(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreSquareBracketsOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    QoreValue lvalue(expr);
    if (!lvalue.hasNode()) {
        error = "unsupported lvalue for square brackets IR lowering";
        return QoreIRValue();
    }
    if (isRangeLValue(lvalue)) {
        error = "unsupported lvalue range for square brackets IR lowering";
        return QoreIRValue();
    }
    return builder.createLoadLValue(lvalue, op->loc)->result;
}

QoreIRValue QoreIRLowering::lowerHashObjectDereference(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    QoreValue lvalue(expr);
    if (!lvalue.hasNode()) {
        error = "unsupported lvalue for hash dereference IR lowering";
        return QoreIRValue();
    }
    return builder.createLoadLValue(lvalue, op->loc)->result;
}

QoreIRValue QoreIRLowering::lowerShift(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreShiftOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    QoreValue lvalue = op->getExp();
    if (!lvalue.hasNode()) {
        error = "unsupported lvalue for shift IR lowering";
        return QoreIRValue();
    }
    if (isRangeLValue(lvalue)) {
        error = "unsupported lvalue range for shift IR lowering";
        return QoreIRValue();
    }
    return builder.createLValueUnaryOp(QoreIROpcode::ShiftLValue, lvalue, op->loc)->result;
}

QoreIRValue QoreIRLowering::lowerUnshift(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreUnshiftOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    QoreValue lvalue = op->getLeft();
    if (!lvalue.hasNode()) {
        error = "unsupported lvalue for unshift IR lowering";
        return QoreIRValue();
    }
    if (isRangeLValue(lvalue)) {
        error = "unsupported lvalue range for unshift IR lowering";
        return QoreIRValue();
    }
    QoreIRValue right = lowerExpression(op->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    return builder.createLValueBinaryOp(QoreIROpcode::UnshiftLValue, lvalue, right, op->loc)->result;
}

QoreIRValue QoreIRLowering::lowerSplice(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreSpliceOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    QoreValue lvalue = op->getLValue();
    if (!lvalue.hasNode()) {
        error = "unsupported lvalue for splice IR lowering";
        return QoreIRValue();
    }
    if (isRangeLValue(lvalue)) {
        error = "unsupported lvalue range for splice IR lowering";
        return QoreIRValue();
    }
    QoreIRValue offset = lowerExpression(op->getOffset(), error);
    if (!offset.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue length = lowerExpression(op->getLength(), error);
    if (!length.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue replacement = lowerExpression(op->getNewValue(), error);
    if (!replacement.isValid()) {
        return QoreIRValue();
    }
    return builder.createLValueTernaryOp(QoreIROpcode::SpliceLValue, lvalue, offset, length, replacement,
        op->loc)->result;
}

QoreIRValue QoreIRLowering::lowerExtract(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreExtractOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    QoreIRValue lvalue = lowerExpression(op->getLValue(), error);
    if (!lvalue.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue offset = lowerExpression(op->getOffset(), error);
    if (!offset.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue length = op->getLength().isNothing()
        ? builder.createConstNothing(op->loc)->result
        : lowerExpression(op->getLength(), error);
    if (!length.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue replacement = op->getNewValue().isNothing()
        ? builder.createConstNothing(op->loc)->result
        : lowerExpression(op->getNewValue(), error);
    if (!replacement.isValid()) {
        return QoreIRValue();
    }
    std::vector<QoreIRValue> operands{lvalue, offset, length, replacement};
    return builder.createExprOp(QoreIROpcode::ExtractAny, expr, operands, op->loc)->result;
}

QoreIRValue QoreIRLowering::lowerRemove(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreRemoveOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    QoreIRValue operand = lowerExpression(op->getExp(), error);
    if (!operand.isValid()) {
        return QoreIRValue();
    }
    std::vector<QoreIRValue> operands{operand};
    return builder.createExprOp(QoreIROpcode::RemoveAny, expr, operands, op->loc)->result;
}

QoreIRValue QoreIRLowering::lowerKeys(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreKeysOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    QoreIRValue operand = lowerExpression(op->getExp(), error);
    if (!operand.isValid()) {
        return QoreIRValue();
    }
    std::vector<QoreIRValue> operands{operand};
    return builder.createExprOp(QoreIROpcode::KeysAny, expr, operands, op->loc)->result;
}

bool QoreIRLowering::lowerCallArgs(const QoreParseListNode* parse_args, const QoreListNode* args,
        std::vector<QoreIRValue>& lowered, std::string& error) {
    if (!parse_args && !args) {
        error = "call args missing for IR lowering";
        return false;
    }
    if (parse_args) {
        for (size_t i = 0; i < parse_args->size(); ++i) {
            QoreIRValue arg = lowerExpression(parse_args->get(i), error);
            if (!arg.isValid()) {
                return false;
            }
            lowered.push_back(arg);
        }
        return true;
    }
    for (size_t i = 0; i < args->size(); ++i) {
        QoreIRValue arg = lowerExpression(args->retrieveEntry(i), error);
        if (!arg.isValid()) {
            return false;
        }
        lowered.push_back(arg);
    }
    return true;
}

QoreIRValue QoreIRLowering::lowerCast(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* cast = dynamic_cast<const QoreCastOperatorNode*>(node);
    auto* parse_cast = dynamic_cast<const QoreParseCastOperatorNode*>(node);
    if (!cast && !parse_cast) {
        return QoreIRValue();
    }
    const QoreSingleExpressionOperatorNode<>* cast_node = cast
        ? static_cast<const QoreSingleExpressionOperatorNode<>*>(cast)
        : static_cast<const QoreSingleExpressionOperatorNode<>*>(parse_cast);
    QoreIRValue value = lowerExpression(cast_node->getExp(), error);
    if (!value.isValid()) {
        return QoreIRValue();
    }
    std::vector<QoreIRValue> operands;
    operands.push_back(value);
    return builder.createExprOp(QoreIROpcode::CastAny, expr, operands, cast_node->loc)->result;
}

QoreIRValue QoreIRLowering::lowerFunctionCall(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* call = dynamic_cast<const FunctionCallNode*>(node);
    if (!call) {
        return QoreIRValue();
    }
    std::vector<QoreIRValue> operands;
    if (!lowerCallArgs(call->getParseArgs(), call->getArgs(), operands, error)) {
        return QoreIRValue();
    }
    return builder.createExprOp(QoreIROpcode::Call, expr, operands, call->loc)->result;
}

QoreIRValue QoreIRLowering::lowerCallReference(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* call = dynamic_cast<const CallReferenceCallNode*>(node);
    if (!call) {
        return QoreIRValue();
    }
    QoreIRValue callee = lowerExpression(call->getExp(), error);
    if (!callee.isValid()) {
        return QoreIRValue();
    }
    std::vector<QoreIRValue> operands;
    operands.push_back(callee);
    if (!lowerCallArgs(call->getParseArgs(), call->getArgs(), operands, error)) {
        return QoreIRValue();
    }
    return builder.createExprOp(QoreIROpcode::CallIndirect, expr, operands, call->loc)->result;
}

QoreIRValue QoreIRLowering::lowerSelfCall(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* call = dynamic_cast<const SelfFunctionCallNode*>(node);
    if (!call) {
        return QoreIRValue();
    }
    std::vector<QoreIRValue> operands;
    if (!lowerCallArgs(call->getParseArgs(), call->getArgs(), operands, error)) {
        return QoreIRValue();
    }
    return builder.createExprOp(QoreIROpcode::CallMethod, expr, operands, call->loc)->result;
}

QoreIRValue QoreIRLowering::lowerStaticCall(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* call = dynamic_cast<const StaticMethodCallNode*>(node);
    if (!call) {
        return QoreIRValue();
    }
    std::vector<QoreIRValue> operands;
    if (!lowerCallArgs(call->getParseArgs(), call->getArgs(), operands, error)) {
        return QoreIRValue();
    }
    return builder.createExprOp(QoreIROpcode::CallStatic, expr, operands, call->loc)->result;
}

QoreIRValue QoreIRLowering::lowerFoldl(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    if (dynamic_cast<const QoreFoldrOperatorNode*>(node)) {
        return QoreIRValue();
    }
    auto* foldl = dynamic_cast<const QoreFoldlOperatorNode*>(node);
    if (!foldl) {
        return QoreIRValue();
    }

    QoreIRValue left = lowerExpression(foldl->getLeft(), error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue right = lowerExpression(foldl->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    return builder.createBinaryOp(QoreIROpcode::FoldlAny, left, right)->result;
}

QoreIRValue QoreIRLowering::lowerFoldr(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* foldr = dynamic_cast<const QoreFoldrOperatorNode*>(node);
    if (!foldr) {
        return QoreIRValue();
    }

    QoreIRValue left = lowerExpression(foldr->getLeft(), error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue right = lowerExpression(foldr->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    return builder.createBinaryOp(QoreIROpcode::FoldrAny, left, right)->result;
}

QoreIRValue QoreIRLowering::lowerMap(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* map = dynamic_cast<const QoreMapOperatorNode*>(node);
    if (!map) {
        return QoreIRValue();
    }

    QoreIRValue left = lowerExpression(map->getLeft(), error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue right = lowerExpression(map->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    return builder.createBinaryOp(QoreIROpcode::MapAny, left, right)->result;
}

QoreIRValue QoreIRLowering::lowerLogicalAnd(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    if (dynamic_cast<const QoreLogicalOrOperatorNode*>(node)) {
        return QoreIRValue();
    }
    auto* and_node = dynamic_cast<const QoreLogicalAndOperatorNode*>(node);
    if (!and_node) {
        return QoreIRValue();
    }
    if (!ensureBuilderContext(error)) {
        return QoreIRValue();
    }

    QoreIRBasicBlock* left_block = builder.getBlock();
    QoreIRValue left_value = lowerExpression(and_node->getLeft(), error);
    if (!left_value.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue left_bool = builder.createUnaryOp(QoreIROpcode::ToBool, left_value)->result;
    QoreIRValue false_value = builder.createConstBool(false)->result;

    QoreIRBasicBlock* rhs_block = createBlock("and.rhs");
    QoreIRBasicBlock* merge_block = createBlock("and.merge");
    if (!rhs_block || !merge_block) {
        error = "IR builder failed to create blocks for logical and";
        return QoreIRValue();
    }
    builder.createBranchIf(left_bool, rhs_block, merge_block);

    builder.setBlock(rhs_block);
    QoreIRValue right_value = lowerExpression(and_node->getRight(), error);
    if (!right_value.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue right_bool = builder.createUnaryOp(QoreIROpcode::ToBool, right_value)->result;
    builder.createBranch(merge_block);

    builder.setBlock(merge_block);
    std::vector<QoreIRPhiIncoming> incoming = {
        {false_value, left_block},
        {right_bool, rhs_block},
    };
    return builder.createPhi(incoming)->result;
}

QoreIRValue QoreIRLowering::lowerLogicalOr(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* or_node = dynamic_cast<const QoreLogicalOrOperatorNode*>(node);
    if (!or_node) {
        return QoreIRValue();
    }
    if (!ensureBuilderContext(error)) {
        return QoreIRValue();
    }

    QoreIRBasicBlock* left_block = builder.getBlock();
    QoreIRValue left_value = lowerExpression(or_node->getLeft(), error);
    if (!left_value.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue left_bool = builder.createUnaryOp(QoreIROpcode::ToBool, left_value)->result;
    QoreIRValue true_value = builder.createConstBool(true)->result;

    QoreIRBasicBlock* rhs_block = createBlock("or.rhs");
    QoreIRBasicBlock* merge_block = createBlock("or.merge");
    if (!rhs_block || !merge_block) {
        error = "IR builder failed to create blocks for logical or";
        return QoreIRValue();
    }
    builder.createBranchIf(left_bool, merge_block, rhs_block);

    builder.setBlock(rhs_block);
    QoreIRValue right_value = lowerExpression(or_node->getRight(), error);
    if (!right_value.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue right_bool = builder.createUnaryOp(QoreIROpcode::ToBool, right_value)->result;
    builder.createBranch(merge_block);

    builder.setBlock(merge_block);
    std::vector<QoreIRPhiIncoming> incoming = {
        {true_value, left_block},
        {right_bool, rhs_block},
    };
    return builder.createPhi(incoming)->result;
}

QoreIRValue QoreIRLowering::lowerLogicalNot(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* not_node = dynamic_cast<const QoreLogicalNotOperatorNode*>(node);
    if (!not_node) {
        return QoreIRValue();
    }
    if (!ensureBuilderContext(error)) {
        return QoreIRValue();
    }

    QoreIRValue value = lowerExpression(not_node->getExp(), error);
    if (!value.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue bool_value = builder.createUnaryOp(QoreIROpcode::ToBool, value)->result;
    return builder.createUnaryOp(QoreIROpcode::Not, bool_value)->result;
}

QoreIRValue QoreIRLowering::lowerNullCoalescing(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* coalesce = dynamic_cast<const QoreNullCoalescingOperatorNode*>(node);
    if (!coalesce) {
        return QoreIRValue();
    }
    if (!ensureBuilderContext(error)) {
        return QoreIRValue();
    }

    QoreIRBasicBlock* left_block = builder.getBlock();
    QoreIRValue left_value = lowerExpression(coalesce->getLeft(), error);
    if (!left_value.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue is_null = builder.createUnaryOp(QoreIROpcode::IsNullOrNothing, left_value)->result;

    QoreIRBasicBlock* rhs_block = createBlock("coalesce.null.rhs");
    QoreIRBasicBlock* merge_block = createBlock("coalesce.null.merge");
    if (!rhs_block || !merge_block) {
        error = "IR builder failed to create blocks for null coalescing";
        return QoreIRValue();
    }
    builder.createBranchIf(is_null, rhs_block, merge_block);

    builder.setBlock(rhs_block);
    QoreIRValue right_value = lowerExpression(coalesce->getRight(), error);
    if (!right_value.isValid()) {
        return QoreIRValue();
    }
    builder.createBranch(merge_block);

    builder.setBlock(merge_block);
    std::vector<QoreIRPhiIncoming> incoming = {
        {left_value, left_block},
        {right_value, rhs_block},
    };
    return builder.createPhi(incoming)->result;
}

QoreIRValue QoreIRLowering::lowerValueCoalescing(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* coalesce = dynamic_cast<const QoreValueCoalescingOperatorNode*>(node);
    if (!coalesce) {
        return QoreIRValue();
    }
    if (!ensureBuilderContext(error)) {
        return QoreIRValue();
    }

    QoreIRBasicBlock* left_block = builder.getBlock();
    QoreIRValue left_value = lowerExpression(coalesce->getLeft(), error);
    if (!left_value.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue left_bool = builder.createUnaryOp(QoreIROpcode::ToBool, left_value)->result;

    QoreIRBasicBlock* rhs_block = createBlock("coalesce.value.rhs");
    QoreIRBasicBlock* merge_block = createBlock("coalesce.value.merge");
    if (!rhs_block || !merge_block) {
        error = "IR builder failed to create blocks for value coalescing";
        return QoreIRValue();
    }
    builder.createBranchIf(left_bool, merge_block, rhs_block);

    builder.setBlock(rhs_block);
    QoreIRValue right_value = lowerExpression(coalesce->getRight(), error);
    if (!right_value.isValid()) {
        return QoreIRValue();
    }
    builder.createBranch(merge_block);

    builder.setBlock(merge_block);
    std::vector<QoreIRPhiIncoming> incoming = {
        {left_value, left_block},
        {right_value, rhs_block},
    };
    return builder.createPhi(incoming)->result;
}

QoreIRValue QoreIRLowering::lowerQuestionMark(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* ternary = dynamic_cast<const QoreQuestionMarkOperatorNode*>(node);
    if (!ternary) {
        return QoreIRValue();
    }
    if (!ensureBuilderContext(error)) {
        return QoreIRValue();
    }

    QoreIRValue cond_value = lowerExpression(ternary->get(0), error);
    if (!cond_value.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue cond_bool = builder.createUnaryOp(QoreIROpcode::ToBool, cond_value)->result;

    QoreIRBasicBlock* left_block = createBlock("ternary.then");
    QoreIRBasicBlock* right_block = createBlock("ternary.else");
    QoreIRBasicBlock* merge_block = createBlock("ternary.merge");
    if (!left_block || !right_block || !merge_block) {
        error = "IR builder failed to create blocks for ternary";
        return QoreIRValue();
    }
    builder.createBranchIf(cond_bool, left_block, right_block);

    builder.setBlock(left_block);
    QoreIRValue left_value = lowerExpression(ternary->get(1), error);
    if (!left_value.isValid()) {
        return QoreIRValue();
    }
    builder.createBranch(merge_block);

    builder.setBlock(right_block);
    QoreIRValue right_value = lowerExpression(ternary->get(2), error);
    if (!right_value.isValid()) {
        return QoreIRValue();
    }
    builder.createBranch(merge_block);

    builder.setBlock(merge_block);
    std::vector<QoreIRPhiIncoming> incoming = {
        {left_value, left_block},
        {right_value, right_block},
    };
    return builder.createPhi(incoming)->result;
}
