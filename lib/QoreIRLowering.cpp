/* -*- indent-tabs-mode: nil -*- */
/*
    QoreIRLowering.cpp

    Qore Programming Language
*/

#include <algorithm>

#include "qore/intern/QoreJITIncludes.h"
#include <qore/intern/QoreIRLowering.h>
#include <qore/intern/QoreOpcodeRegistry.h>
#include <qore/intern/QorePluginRegistry.h>

#include <qore/DateTimeNode.h>
#include <qore/QoreObject.h>
#include <qore/QoreValue.h>
#include <qore/QoreHashNode.h>
#include <qore/QoreListNode.h>
#include <qore/QoreNumberNode.h>
#include <qore/BinaryNode.h>
#include <qore/intern/QoreLibIntern.h>
#include <qore/intern/LocalVar.h>
#include <qore/intern/QoreTypeInfo.h>
#include <qore/intern/ParseNode.h>
#include <qore/QoreType.h>
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
#include <qore/intern/QoreIntMinusEqualsOperatorNode.h>
#include <qore/intern/QoreIntPreIncrementOperatorNode.h>
#include <qore/intern/QoreIntPostIncrementOperatorNode.h>
#include <qore/intern/QoreIntPreDecrementOperatorNode.h>
#include <qore/intern/QoreIntPostDecrementOperatorNode.h>
#include <qore/intern/QoreMultiplicationOperatorNode.h>
#include <qore/intern/QoreNullCoalescingOperatorNode.h>
#include <qore/intern/QorePlusOperatorNode.h>
#include <qore/intern/QoreIntPlusEqualsOperatorNode.h>
#include <qore/intern/QoreQuestionMarkOperatorNode.h>
#include <qore/intern/QoreDivisionOperatorNode.h>
#include <qore/intern/QoreFoldlOperatorNode.h>
#include <qore/intern/QoreParseHashNode.h>
#include <qore/intern/QoreParseListNode.h>
#include <qore/intern/QoreImplicitArgumentNode.h>
#include <qore/intern/QoreImplicitElementNode.h>
#include <qore/intern/QoreRegexNMatchOperatorNode.h>
#include <qore/intern/QoreInstanceOfOperatorNode.h>
#include <qore/intern/QoreTrimOperatorNode.h>
#include <qore/intern/QoreChompOperatorNode.h>
#include <qore/intern/QoreTransliterationOperatorNode.h>
#include <qore/intern/QoreBackgroundOperatorNode.h>
#include <qore/intern/QoreListAssignmentOperatorNode.h>
#include <qore/intern/QoreSelectOperatorNode.h>
#include <qore/intern/QoreMapSelectOperatorNode.h>
#include <qore/intern/QoreHashMapOperatorNode.h>
#include <qore/intern/QoreHashMapSelectOperatorNode.h>
#include <qore/intern/QoreAssignmentOperatorNode.h>
#include <qore/intern/QoreBinaryAndOperatorNode.h>
#include <qore/intern/QoreBinaryOrOperatorNode.h>
#include <qore/intern/QoreBinaryNotOperatorNode.h>
#include <qore/intern/QoreBinaryXorOperatorNode.h>
#include <qore/intern/QoreCastOperatorNode.h>
#include <qore/intern/QoreMapOperatorNode.h>
#include <qore/intern/QoreIterateOperatorNode.h>
#include <qore/intern/QoreStreamingOperatorNode.h>
#include <qore/intern/QoreBinaryLValueOperatorNode.h>
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
#include <qore/intern/QorePopOperatorNode.h>
#include <qore/intern/QorePushOperatorNode.h>
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
#include <qore/intern/QoreDeleteOperatorNode.h>
#include <qore/intern/QoreKeysOperatorNode.h>
#include <qore/intern/QoreRegexMatchOperatorNode.h>
#include <qore/intern/QoreRegexExtractOperatorNode.h>
#include <qore/intern/QoreRegexSubstOperatorNode.h>
#include <qore/intern/QoreExistsOperatorNode.h>
#include <qore/intern/QoreElementsOperatorNode.h>
#include <qore/intern/QoreDotEvalOperatorNode.h>
#include <qore/intern/BackquoteNode.h>
#include <qore/intern/CallReferenceNode.h>
#include <qore/intern/ComplexContextrefNode.h>
#include <qore/intern/ContextRowNode.h>
#include <qore/intern/ConstantList.h>
#include <qore/intern/ContextrefNode.h>
#include <qore/intern/QoreClosureNode.h>
#include <qore/intern/FindNode.h>
#include <qore/intern/NewComplexTypeNode.h>
#include <qore/intern/ParseReferenceNode.h>
#include <qore/intern/SelfVarrefNode.h>
#include <qore/intern/StaticClassVarRefNode.h>
#include <qore/intern/ExpressionStatement.h>
#include <qore/intern/ForStatement.h>
#include <qore/intern/IfStatement.h>
#include <qore/intern/ReturnStatement.h>
#include <qore/intern/DoWhileStatement.h>
#include <qore/intern/StatementBlock.h>
#include <qore/intern/WhileStatement.h>
#include <qore/intern/BreakStatement.h>
#include <qore/intern/ContinueStatement.h>
#include <qore/intern/SwitchStatement.h>
#include <qore/intern/CaseNodeWithOperator.h>
#include <qore/intern/CaseNodeRegex.h>
#include <qore/intern/TryStatement.h>
#include <qore/intern/ThrowStatement.h>
#include <qore/intern/RethrowStatement.h>
#include <qore/intern/ForEachStatement.h>
#include <qore/intern/ThreadExitStatement.h>
#include <qore/intern/OnBlockExitStatement.h>
#include <qore/intern/DebugStatement.h>
#include <qore/intern/AssertStatement.h>
#include <qore/intern/ContextStatement.h>
#include <qore/intern/Context.h>  // for CM_SORT_* constants
#include <qore/intern/SummarizeStatement.h>
#include <qore/intern/QoreOperatorNode.h>
#include <qore/intern/ObjectMethodReferenceNode.h>
#include <qore/intern/QoreTypeInfo.h>
#include <qore/intern/QoreClassIntern.h>
#include <qore/intern/QoreIRVerifier.h>
#include <qore/intern/QoreIRExprRegistry.h>
#include <qore/intern/QoreHashNodeIntern.h>
#include <qore/intern/qore_list_private.h>

#include <atomic>

// Forward declaration from Function.cpp - collects all locals from a statement tree
extern void collectAllStatementLocals(const StatementBlock* block, std::vector<LocalVar*>& locals);

static bool blockHasTerminator(const QoreIRBasicBlock* block) {
    if (!block || block->instructions.empty()) {
        return false;
    }
    return isTerminator(block->instructions.back()->opcode);
}

void QoreIRLoweringContext::setError(const std::string& message) {
    error = message;
}

void QoreIRLoweringContext::setError(const char* message) {
    error = message ? message : "";
}

const std::string& QoreIRLoweringContext::getError() const {
    return error;
}

void QoreIRLoweringContext::setResult(QoreIRValue value) {
    result = value;
}

QoreIRValue QoreIRLoweringContext::getResult() const {
    return result;
}

QoreIRLowering& QoreIRLoweringContext::getLowering() const {
    return lowering;
}

static bool pluginLoweringClaimsNode(uint64_t claimed_node_kinds, qore_type_t node_type) {
    return node_type >= 0 && node_type < 64 && (claimed_node_kinds & (1ULL << node_type));
}

static void setPluginLoweringError(std::string& error, const QorePluginLoweringInfo& info,
        const char* subreason, const char* detail) {
    error = "PLUGIN-LOWERING-CLAIM-VIOLATED: plugin lowering callback for module=\"";
    error += info.module_name;
    error += "\", operation=\"";
    error += info.operation_name.empty() ? "<unknown>" : info.operation_name;
    error += "\", local_operation_id=\"";
    error += std::to_string(info.local_operation_id);
    error += "\" ";
    error += detail ? detail : "failed";
    error += " (subreason=\"";
    error += subreason ? subreason : "lowering_failed";
    error += "\", section=3.7)";
}
#include <qore/intern/QoreHashObjectDereferenceOperatorNode.h>
#include <qore/intern/QoreSquareBracketsOperatorNode.h>
#include <qore/intern/FunctionCallNode.h>
#include <qore/intern/CallReferenceCallNode.h>
#include <qore/intern/QorePseudoMethods.h>
#include <qore/intern/QoreSquareBracketsRangeOperatorNode.h>
#include <qore/intern/VarRefNode.h>
#include <qore/intern/Variable.h>
#include <qore/QoreStringNode.h>

// Get parse-time type info from a QoreValue expression.
// QoreValue::getTypeInfo() returns type based on runtime value, which returns null for
// parse nodes like VarRefNode. This helper checks for specific node types that have
// parse-time type info, which is needed for type-aware optimizations in map/select/fold etc.
static const QoreTypeInfo* getExprTypeInfo(const QoreValue& val) {
    const AbstractQoreNode* node = val.getInternalNode();
    if (node) {
        if (auto* var_ref = dynamic_cast<const VarRefNode*>(node)) {
            return var_ref->getTypeInfo();
        }
        if (auto* cast_op = dynamic_cast<const QoreCastOperatorNode*>(node)) {
            // Cast operators should return their target type, not the generic 'auto' type
            return cast_op->getCastTypeInfo();
        }
        if (auto* rt_const = dynamic_cast<const RuntimeConstantRefNode*>(node)) {
            // RuntimeConstantRefNode holds the type of the constant (e.g., list<string>)
            if (auto* ce = rt_const->getConstantEntry()) {
                return ce->typeInfo;
            }
        }
        // Generic operator type info: any operator node with parse-time result type.
        // This covers Map, Select, Keys, Plus, SquareBrackets, Range, and all other
        // operators that override getTypeInfo() — no need for per-type dynamic_casts.
        if (node->getType() == NT_OPERATOR) {
            const QoreTypeInfo* ti = static_cast<const QoreOperatorNode*>(node)->getTypeInfo();
            if (ti) {
                return ti;
            }
        }
        if (auto* parse_node = dynamic_cast<const ParseNode*>(node)) {
            const QoreParseAnalysis& analysis = parse_node->getParseAnalysis();
            if (analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)) {
                return analysis.known_type;
            }
        }
    }
    return val.getTypeInfo();
}

static bool isImmediateCleanupFreeType(const QoreTypeInfo* type_info) {
    return QoreTypeInfo::isType(type_info, NT_BOOLEAN)
        || QoreTypeInfo::isType(type_info, NT_INT)
        || QoreTypeInfo::isType(type_info, NT_FLOAT)
        || QoreTypeInfo::isType(type_info, NT_CHAR)
        || QoreTypeInfo::isType(type_info, NT_NOTHING)
        || QoreTypeInfo::isType(type_info, NT_NULL);
}

static bool isImmediateCleanupFreeValue(const QoreValue& val) {
    const QoreTypeInfo* type_info = getExprTypeInfo(val);
    if (isImmediateCleanupFreeType(type_info)) {
        return true;
    }
    if (val.hasNode()) {
        return false;
    }
    qore_type_t type = val.getType();
    return type == NT_BOOLEAN || type == NT_INT || type == NT_FLOAT || type == NT_CHAR
        || type == NT_NOTHING || type == NT_NULL;
}

static bool expressionMayCreateNodeTemp(const QoreValue& val) {
    if (!val) {
        return false;
    }
    const AbstractQoreNode* node = val.getInternalNode();
    if (!node) {
        return !isImmediateCleanupFreeValue(val);
    }

    if (auto* var_ref = dynamic_cast<const VarRefNode*>(node)) {
        return !isImmediateCleanupFreeType(var_ref->getTypeInfo());
    }
    if (auto* rt_const = dynamic_cast<const RuntimeConstantRefNode*>(node)) {
        if (auto* ce = rt_const->getConstantEntry()) {
            return !isImmediateCleanupFreeType(ce->typeInfo);
        }
        return true;
    }
    if (val.isValue()) {
        return !isImmediateCleanupFreeValue(val);
    }

    auto argsMayCreateNodeTemp = [](const QoreParseListNode* parse_args, const QoreListNode* args) {
        if (parse_args) {
            for (size_t i = 0; i < parse_args->size(); ++i) {
                if (expressionMayCreateNodeTemp(parse_args->get(i))) {
                    return true;
                }
            }
        }
        if (args) {
            ConstListIterator li(args);
            while (li.next()) {
                if (expressionMayCreateNodeTemp(li.getValue())) {
                    return true;
                }
            }
        }
        return false;
    };

    if (auto* call = dynamic_cast<const FunctionCallBase*>(node)) {
        return argsMayCreateNodeTemp(call->getParseArgs(), call->getArgs())
            || !isImmediateCleanupFreeType(getExprTypeInfo(val));
    }
    if (auto* call = dynamic_cast<const CallReferenceCallNode*>(node)) {
        return expressionMayCreateNodeTemp(call->getExp())
            || argsMayCreateNodeTemp(call->getParseArgs(), call->getArgs())
            || !isImmediateCleanupFreeType(getExprTypeInfo(val));
    }
    if (auto* binop = dynamic_cast<const QoreBinaryOperatorNode<>*>(node)) {
        return expressionMayCreateNodeTemp(binop->getLeft())
            || expressionMayCreateNodeTemp(binop->getRight())
            || !isImmediateCleanupFreeType(getExprTypeInfo(val));
    }
    if (auto* binop = dynamic_cast<const QoreBinaryOperatorNode<LValueOperatorNode>*>(node)) {
        return expressionMayCreateNodeTemp(binop->getLeft())
            || expressionMayCreateNodeTemp(binop->getRight())
            || !isImmediateCleanupFreeType(getExprTypeInfo(val));
    }
    if (auto* unop = dynamic_cast<const QoreSingleExpressionOperatorNode<>*>(node)) {
        return expressionMayCreateNodeTemp(unop->getExp())
            || !isImmediateCleanupFreeType(getExprTypeInfo(val));
    }
    if (auto* unop = dynamic_cast<const QoreSingleExpressionOperatorNode<LValueOperatorNode>*>(node)) {
        return expressionMayCreateNodeTemp(unop->getExp())
            || !isImmediateCleanupFreeType(getExprTypeInfo(val));
    }
    if (auto* sub = dynamic_cast<const QoreSquareBracketsOperatorNode*>(node)) {
        return expressionMayCreateNodeTemp(sub->getLeft())
            || expressionMayCreateNodeTemp(sub->getRight())
            || !isImmediateCleanupFreeType(getExprTypeInfo(val));
    }
    if (auto* hd = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(node)) {
        return expressionMayCreateNodeTemp(hd->getLeft())
            || !isImmediateCleanupFreeType(getExprTypeInfo(val));
    }

    return true;
}

static bool statementMayCreateNodeTemp(const AbstractStatement* stmt) {
    if (auto* expr_stmt = dynamic_cast<const ExpressionStatement*>(stmt)) {
        return expressionMayCreateNodeTemp(expr_stmt->getExpression());
    }
    if (auto* return_stmt = dynamic_cast<const ReturnStatement*>(stmt)) {
        return expressionMayCreateNodeTemp(return_stmt->getExpression());
    }
    if (auto* for_stmt = dynamic_cast<const ForStatement*>(stmt)) {
        return expressionMayCreateNodeTemp(for_stmt->getAssignment())
            || expressionMayCreateNodeTemp(for_stmt->getIterator());
    }
    if (dynamic_cast<const IfStatement*>(stmt) || dynamic_cast<const WhileStatement*>(stmt)
            || dynamic_cast<const DoWhileStatement*>(stmt)) {
        return false;
    }
    return true;
}

static bool conditionValueSurvivesDiscardTemps(const QoreValue& val) {
    // A condition value survives the discard.temps emitted just before the
    // branch only if it can never be a refcounted heap temporary that
    // discard.temps would free.  Integers are deliberately excluded here even
    // though isImmediateCleanupFreeType() accepts NT_INT: an integer outside the
    // inline INT48 range — e.g. the result of `x & (1 << 53)` lowered via the
    // boxing AndAny path — is materialised as a refcounted QoreBigIntNode, and
    // that temporary is released by the discard.temps that precedes the branch.
    // Treating such a value as a survivor skips the ToBool conversion and lets
    // BrIf's slow path dereference the freed node — a use-after-free (crash or
    // wrong branch).  Converting with ToBool first reads the value while it is
    // still live, so only the genuinely never-boxed immediate types qualify:
    // bool, float (always a native double), char, NOTHING and NULL.
    const QoreTypeInfo* type_info = getExprTypeInfo(val);
    return QoreTypeInfo::isType(type_info, NT_BOOLEAN)
        || QoreTypeInfo::isType(type_info, NT_FLOAT)
        || QoreTypeInfo::isType(type_info, NT_CHAR)
        || QoreTypeInfo::isType(type_info, NT_NOTHING)
        || QoreTypeInfo::isType(type_info, NT_NULL);
}

static QoreIRPluginOperationRef pluginOperationRefFromInfo(const QorePluginResolvedOperationInfo& info,
        QoreProgram* pgm) {
    QoreIRPluginOperationRef ref;
    ref.global_operation_id = info.global_operation_id;
    ref.module_name = info.module_name;
    ref.local_operation_id = info.local_operation_id;
    ref.canonical_signature_version = info.canonical_signature_version;
    ref.signature_hash = info.signature_hash;
    ref.fp_reassociation_enabled = pgm && qore_plugin_allows_fp_reassociation(info, pgm->getParseOptions());
    return ref;
}

static QoreIRValue tryDescriptorPluginSubscriptLowering(QoreIRLowering& lowering, QoreIRBuilder& builder,
        QoreParseContext* parse_context, const QoreValue&, const QoreSquareBracketsOperatorNode* op,
        std::string& error) {
    QorePluginResolvedOperationInfo info;
    int rc = qore_plugin_resolve_program_operation_info(parse_context ? parse_context->pgm : nullptr,
        getExprTypeInfo(op->getLeft()), getExprTypeInfo(op->getRight()), "subscript",
        QorePluginHelperAbi::SubscriptValue, info, nullptr);
    if (rc > 0) {
        return QoreIRValue();
    }
    if (rc < 0) {
        error = "failed to resolve descriptor-based plugin subscript operation";
        return QoreIRValue();
    }

    QoreIRValue lhs = lowering.lowerExpression(op->getLeft(), error);
    if (!lhs.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue rhs = lowering.lowerExpression(op->getRight(), error);
    if (!rhs.isValid()) {
        return QoreIRValue();
    }
    return builder.createPluginOp(QoreIROpcode::PluginSubscript,
        pluginOperationRefFromInfo(info, parse_context ? parse_context->pgm : nullptr), {lhs, rhs}, op->loc)->result;
}

static QoreIRValue tryDescriptorPluginSliceLowering(QoreIRLowering& lowering, QoreIRBuilder& builder,
        QoreParseContext* parse_context, const QoreValue&, const QoreSquareBracketsRangeOperatorNode* op,
        std::string& error) {
    QorePluginResolvedOperationInfo info;
    int rc = qore_plugin_resolve_program_operation_info(parse_context ? parse_context->pgm : nullptr,
        getExprTypeInfo(op->get(0)), nullptr, "slice", QorePluginHelperAbi::CallValueList, info, nullptr);
    if (rc > 0) {
        return QoreIRValue();
    }
    if (rc < 0) {
        error = "failed to resolve descriptor-based plugin slice operation";
        return QoreIRValue();
    }
    if (info.signature.arity != 0xff) {
        return QoreIRValue();
    }

    QoreIRValue seq = lowering.lowerExpression(op->get(0), error);
    if (!seq.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue start = lowering.lowerExpression(op->get(1), error);
    if (!start.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue end = lowering.lowerExpression(op->get(2), error);
    if (!end.isValid()) {
        return QoreIRValue();
    }
    return builder.createPluginOp(QoreIROpcode::PluginCall,
        pluginOperationRefFromInfo(info, parse_context ? parse_context->pgm : nullptr), {seq, start, end},
        op->loc)->result;
}

static QoreIRValue tryDescriptorPluginBinaryLowering(QoreIRLowering& lowering, QoreIRBuilder& builder,
        QoreParseContext* parse_context, const QoreBinaryOperatorNode<>* op, const char* operation_name,
        std::string& error) {
    QorePluginResolvedOperationInfo info;
    int rc = qore_plugin_resolve_program_operation_info(parse_context ? parse_context->pgm : nullptr,
        getExprTypeInfo(op->getLeft()), getExprTypeInfo(op->getRight()), operation_name,
        QorePluginHelperAbi::BinaryValue, info, nullptr);
    if (rc > 0) {
        return QoreIRValue();
    }
    if (rc < 0) {
        error = "failed to resolve descriptor-based plugin binary operation '";
        error += operation_name;
        error += "'";
        return QoreIRValue();
    }

    QoreIRValue lhs = lowering.lowerExpression(op->getLeft(), error);
    if (!lhs.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue rhs = lowering.lowerExpression(op->getRight(), error);
    if (!rhs.isValid()) {
        return QoreIRValue();
    }
    return builder.createPluginOp(QoreIROpcode::PluginBinary,
        pluginOperationRefFromInfo(info, parse_context ? parse_context->pgm : nullptr), {lhs, rhs}, op->loc)->result;
}

static QoreIRValue tryDescriptorPluginUnaryLowering(QoreIRLowering& lowering, QoreIRBuilder& builder,
        QoreParseContext* parse_context, const QoreBinaryNotOperatorNode* op, const char* operation_name,
        std::string& error) {
    QorePluginResolvedOperationInfo info;
    int rc = qore_plugin_resolve_program_operation_info(parse_context ? parse_context->pgm : nullptr,
        getExprTypeInfo(op->getExp()), nullptr, operation_name, QorePluginHelperAbi::UnaryValue, info, nullptr);
    if (rc > 0) {
        return QoreIRValue();
    }
    if (rc < 0) {
        error = "failed to resolve descriptor-based plugin unary operation '";
        error += operation_name;
        error += "'";
        return QoreIRValue();
    }

    QoreIRValue value = lowering.lowerExpression(op->getExp(), error);
    if (!value.isValid()) {
        return QoreIRValue();
    }
    return builder.createPluginOp(QoreIROpcode::PluginUnary,
        pluginOperationRefFromInfo(info, parse_context ? parse_context->pgm : nullptr), {value}, op->loc)->result;
}

static QoreIRValue tryDescriptorPluginLowering(QoreIRLowering& lowering, QoreIRBuilder& builder,
        QoreParseContext* parse_context, const QoreValue& expr, const AbstractQoreNode* node,
        std::string& error) {
    if (auto* subscript = dynamic_cast<const QoreSquareBracketsOperatorNode*>(node)) {
        return tryDescriptorPluginSubscriptLowering(lowering, builder, parse_context, expr, subscript, error);
    }
    if (auto* slice = dynamic_cast<const QoreSquareBracketsRangeOperatorNode*>(node)) {
        return tryDescriptorPluginSliceLowering(lowering, builder, parse_context, expr, slice, error);
    }
    if (auto* bit_not = dynamic_cast<const QoreBinaryNotOperatorNode*>(node)) {
        return tryDescriptorPluginUnaryLowering(lowering, builder, parse_context, bit_not, "bit_not", error);
    }
    if (auto* bit_and = dynamic_cast<const QoreBinaryAndOperatorNode*>(node)) {
        return tryDescriptorPluginBinaryLowering(lowering, builder, parse_context, bit_and, "bit_and", error);
    }
    if (auto* bit_or = dynamic_cast<const QoreBinaryOrOperatorNode*>(node)) {
        return tryDescriptorPluginBinaryLowering(lowering, builder, parse_context, bit_or, "bit_or", error);
    }
    if (auto* bit_xor = dynamic_cast<const QoreBinaryXorOperatorNode*>(node)) {
        return tryDescriptorPluginBinaryLowering(lowering, builder, parse_context, bit_xor, "bit_xor", error);
    }
    if (auto* ne = dynamic_cast<const QoreLogicalNotEqualsOperatorNode*>(node)) {
        return tryDescriptorPluginBinaryLowering(lowering, builder, parse_context, ne, "ne", error);
    }
    if (auto* eq = dynamic_cast<const QoreLogicalEqualsOperatorNode*>(node)) {
        return tryDescriptorPluginBinaryLowering(lowering, builder, parse_context, eq, "eq", error);
    }
    if (auto* le = dynamic_cast<const QoreLogicalLessThanOrEqualsOperatorNode*>(node)) {
        return tryDescriptorPluginBinaryLowering(lowering, builder, parse_context, le, "le", error);
    }
    if (auto* lt = dynamic_cast<const QoreLogicalLessThanOperatorNode*>(node)) {
        return tryDescriptorPluginBinaryLowering(lowering, builder, parse_context, lt, "lt", error);
    }
    if (auto* ge = dynamic_cast<const QoreLogicalGreaterThanOrEqualsOperatorNode*>(node)) {
        return tryDescriptorPluginBinaryLowering(lowering, builder, parse_context, ge, "ge", error);
    }
    if (auto* gt = dynamic_cast<const QoreLogicalGreaterThanOperatorNode*>(node)) {
        return tryDescriptorPluginBinaryLowering(lowering, builder, parse_context, gt, "gt", error);
    }
    return QoreIRValue();
}

QoreIRLowering::QoreIRLowering(QoreIRBuilder& n_builder, QoreParseContext* n_parse_context)
        : builder(n_builder), parse_context(n_parse_context) {
}

// Check if a variable is a local (not global) and not captured by a closure.
// Used for fused integer operations that can only be applied to lvstack locals.
static bool isLocalNonClosureVar(const VarRefNode* var) {
    return var && var->getType() == VT_LOCAL && var->ref.id && !var->ref.id->closureUse();
}

// Check if a variable has a LocalVar backing slot, including closure-use locals.
// Fused interpreter opcodes handle closure write-through explicitly.
static bool isLocalOrClosureVar(const VarRefNode* var) {
    return var && (var->getType() == VT_LOCAL || var->getType() == VT_CLOSURE) && var->ref.id;
}

static bool qoreIrCallHasNoArgs(const FunctionCallBase* call) {
    if (!call) {
        return true;
    }
    const QoreParseListNode* parse_args = call->getParseArgs();
    if (parse_args && parse_args->size()) {
        return false;
    }
    const QoreListNode* args = call->getArgs();
    return !args || !args->size();
}

static const VarRefNode* qoreIrGetDirectLocalVarRef(const QoreValue& expr) {
    if (!expr.hasNode()) {
        return nullptr;
    }
    auto* var = dynamic_cast<const VarRefNode*>(expr.getInternalNode());
    return isLocalNonClosureVar(var) ? var : nullptr;
}

static bool qoreIrGetSinglePositionalCallArgNoHoles(const FunctionCallBase* call, QoreValue& arg) {
    if (!call) {
        return false;
    }
    if (const QoreParseListNode* parse_args = call->getParseArgs()) {
        if (parse_args->size() != 1) {
            return false;
        }
        arg = parse_args->get(0);
        return true;
    }
    const QoreListNode* args = call->getArgs();
    if (!args) {
        return false;
    }
    const qore_list_private* args_priv = qore_list_private::get(args);
    if (args_priv && args_priv->hasCallArgEvalMap()) {
        const std::vector<size_t>* pos_map = args_priv->getCallArgEvalMap();
        if (!pos_map || pos_map->size() != 1 || args_priv->getCallArgEvalResultSize() != 1
                || args_priv->callArgEvalMapHasHoles() || (*pos_map)[0] != 0) {
            return false;
        }
        arg = args->retrieveEntry(0);
        return true;
    }
    if (args->size() != 1) {
        return false;
    }
    arg = args->retrieveEntry(0);
    return true;
}

static bool qoreIrIsAssignedPlainListLocal(LocalVar* local) {
    if (!local || local->closureUse() || !local->isAssigned()) {
        return false;
    }
    const QoreTypeInfo* type_info = local->parseGetTypeInfo();
    return type_info && QoreTypeInfo::isType(type_info, NT_LIST);
}

static bool qoreIrIsAssignedPlainStringLocal(LocalVar* local) {
    if (!local || local->closureUse() || !local->isAssigned()) {
        return false;
    }
    const QoreTypeInfo* type_info = local->parseGetTypeInfo();
    return type_info && QoreTypeInfo::isType(type_info, NT_STRING);
}

static LocalVar* qoreIrGetHoistableListSizeLocal(const QoreValue& expr) {
    if (!expr.hasNode()) {
        return nullptr;
    }
    auto* dot = dynamic_cast<const QoreDotEvalOperatorNode*>(expr.getInternalNode());
    if (!dot) {
        return nullptr;
    }
    MethodCallNode* method_call = dot->getMethodCall();
    if (!method_call || !method_call->isPseudo() || !qoreIrCallHasNoArgs(method_call)) {
        return nullptr;
    }
    const char* method_name = method_call->getName();
    if (!method_name || strcmp(method_name, "size")) {
        return nullptr;
    }
    const QoreMethod* method = method_call->getMethod();
    if (method && method->getClass() && strcmp(method->getClass()->getName(), "<list>")) {
        return nullptr;
    }
    const VarRefNode* base_var = qoreIrGetDirectLocalVarRef(dot->getExpression());
    if (!base_var) {
        return nullptr;
    }
    LocalVar* local = base_var->ref.id;
    return qoreIrIsAssignedPlainListLocal(local) ? local : nullptr;
}

static bool qoreIrIsHoistableStringNoArgMethodName(const char* name) {
    return name && (!strcmp(name, "size") || !strcmp(name, "strlen") || !strcmp(name, "length")
        || !strcmp(name, "empty") || !strcmp(name, "val") || !strcmp(name, "sizep")
        || !strcmp(name, "strp") || !strcmp(name, "intp"));
}

static bool qoreIrIsGlobalStringLengthName(const char* name) {
    return name && (!strcmp(name, "strlen") || !strcmp(name, "length"));
}

static LocalVar* qoreIrGetHoistableStringNoArgLocal(const QoreValue& expr) {
    if (!expr.hasNode()) {
        return nullptr;
    }
    const AbstractQoreNode* node = expr.getInternalNode();
    if (!node) {
        return nullptr;
    }
    if (auto* dot = dynamic_cast<const QoreDotEvalOperatorNode*>(node)) {
        MethodCallNode* method_call = dot->getMethodCall();
        if (!method_call || !method_call->isPseudo() || !qoreIrCallHasNoArgs(method_call)
                || !qoreIrIsHoistableStringNoArgMethodName(method_call->getName())) {
            return nullptr;
        }
        const QoreMethod* method = method_call->getMethod();
        if (method && method->getClass() && strcmp(method->getClass()->getName(), "<string>")) {
            return nullptr;
        }
        const VarRefNode* base_var = qoreIrGetDirectLocalVarRef(dot->getExpression());
        if (!base_var) {
            return nullptr;
        }
        LocalVar* local = base_var->ref.id;
        return qoreIrIsAssignedPlainStringLocal(local) ? local : nullptr;
    }
    return nullptr;
}

static LocalVar* qoreIrGetHoistableGlobalStringLengthLocal(const QoreValue& expr) {
    if (!expr.hasNode()) {
        return nullptr;
    }
    auto* call = dynamic_cast<const FunctionCallNode*>(expr.getInternalNode());
    if (!call || call->hasExplicitTypeArgs() || !qoreIrIsGlobalStringLengthName(call->getName())) {
        return nullptr;
    }
    QoreValue arg_expr;
    if (!qoreIrGetSinglePositionalCallArgNoHoles(call, arg_expr)) {
        return nullptr;
    }
    const VarRefNode* arg_var = qoreIrGetDirectLocalVarRef(arg_expr);
    if (!arg_var) {
        return nullptr;
    }
    LocalVar* local = arg_var->ref.id;
    return qoreIrIsAssignedPlainStringLocal(local) ? local : nullptr;
}

static bool qoreIrStatementBlockInvalidatesListSizeHoist(const StatementBlock* block,
        const std::unordered_set<LocalVar*>& candidates);

static bool qoreIrListSizeHoistAnalysisCancelled(size_t count) {
    return count && count % 100 == 0 && qore_check_cancel(nullptr, "IR list-size hoist analysis");
}

static bool qoreIrIsDirectNonCandidateLocalLValue(const QoreValue& expr,
        const std::unordered_set<LocalVar*>& candidates) {
    const VarRefNode* var = qoreIrGetDirectLocalVarRef(expr);
    return var && candidates.find(var->ref.id) == candidates.end();
}

static bool qoreIrExpressionInvalidatesListSizeHoist(const QoreValue& expr,
        const std::unordered_set<LocalVar*>& candidates) {
    if (!expr.hasNode()) {
        return false;
    }
    const AbstractQoreNode* node = expr.getInternalNode();
    if (!node) {
        return false;
    }
    if (qoreIrGetHoistableListSizeLocal(expr)
            || qoreIrGetHoistableStringNoArgLocal(expr)
            || qoreIrGetHoistableGlobalStringLengthLocal(expr)) {
        return false;
    }
    if (auto* dot = dynamic_cast<const QoreDotEvalOperatorNode*>(node)) {
        (void)dot;
        return true;
    }
    if (dynamic_cast<const FunctionCallBase*>(node)
            || dynamic_cast<const CallReferenceCallNode*>(node)) {
        return true;
    }
    if (auto* assign = dynamic_cast<const QoreAssignmentOperatorNode*>(node)) {
        if (!qoreIrIsDirectNonCandidateLocalLValue(assign->getLeft(), candidates)) {
            return true;
        }
        return qoreIrExpressionInvalidatesListSizeHoist(assign->getRight(), candidates);
    }
    if (auto* bin_lvalue = dynamic_cast<const QoreBinaryOperatorNode<LValueOperatorNode>*>(node)) {
        if (!qoreIrIsDirectNonCandidateLocalLValue(bin_lvalue->getLeft(), candidates)) {
            return true;
        }
        return qoreIrExpressionInvalidatesListSizeHoist(bin_lvalue->getRight(), candidates);
    }
    if (auto* un_lvalue = dynamic_cast<const QoreSingleExpressionOperatorNode<LValueOperatorNode>*>(node)) {
        return !qoreIrIsDirectNonCandidateLocalLValue(un_lvalue->getExp(), candidates);
    }
    if (auto* binop = dynamic_cast<const QoreBinaryOperatorNode<>*>(node)) {
        return qoreIrExpressionInvalidatesListSizeHoist(binop->getLeft(), candidates)
            || qoreIrExpressionInvalidatesListSizeHoist(binop->getRight(), candidates);
    }
    if (auto* unop = dynamic_cast<const QoreSingleExpressionOperatorNode<>*>(node)) {
        return qoreIrExpressionInvalidatesListSizeHoist(unop->getExp(), candidates);
    }
    if (auto* sub = dynamic_cast<const QoreSquareBracketsOperatorNode*>(node)) {
        return qoreIrExpressionInvalidatesListSizeHoist(sub->getLeft(), candidates)
            || qoreIrExpressionInvalidatesListSizeHoist(sub->getRight(), candidates);
    }
    if (auto* hd = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(node)) {
        return qoreIrExpressionInvalidatesListSizeHoist(hd->getLeft(), candidates);
    }
    return false;
}

static void qoreIrCollectListSizeHoistCandidates(const QoreValue& expr,
        std::unordered_set<LocalVar*>& list_candidates, std::unordered_set<LocalVar*>& invalidation_candidates,
        std::vector<QoreValue>& value_candidates, bool& cancelled) {
    if (cancelled || !expr.hasNode()) {
        return;
    }
    if (LocalVar* local = qoreIrGetHoistableListSizeLocal(expr)) {
        list_candidates.insert(local);
        invalidation_candidates.insert(local);
        return;
    }
    if (LocalVar* local = qoreIrGetHoistableStringNoArgLocal(expr)) {
        invalidation_candidates.insert(local);
        value_candidates.push_back(expr);
        return;
    }
    if (LocalVar* local = qoreIrGetHoistableGlobalStringLengthLocal(expr)) {
        invalidation_candidates.insert(local);
        value_candidates.push_back(expr);
        return;
    }
    const AbstractQoreNode* node = expr.getInternalNode();
    if (!node) {
        return;
    }
    if (auto* dot = dynamic_cast<const QoreDotEvalOperatorNode*>(node)) {
        qoreIrCollectListSizeHoistCandidates(dot->getExpression(), list_candidates, invalidation_candidates,
            value_candidates, cancelled);
        if (cancelled) {
            return;
        }
        if (MethodCallNode* method_call = dot->getMethodCall()) {
            if (const QoreParseListNode* parse_args = method_call->getParseArgs()) {
                for (size_t i = 0; i < parse_args->size(); ++i) {
                    if (qoreIrListSizeHoistAnalysisCancelled(i)) {
                        cancelled = true;
                        return;
                    }
                    qoreIrCollectListSizeHoistCandidates(parse_args->get(i), list_candidates,
                        invalidation_candidates, value_candidates, cancelled);
                    if (cancelled) {
                        return;
                    }
                }
            }
            if (const QoreListNode* args = method_call->getArgs()) {
                ConstListIterator li(args);
                size_t count = 0;
                while (li.next()) {
                    if (qoreIrListSizeHoistAnalysisCancelled(count++)) {
                        cancelled = true;
                        return;
                    }
                    qoreIrCollectListSizeHoistCandidates(li.getValue(), list_candidates, invalidation_candidates,
                        value_candidates, cancelled);
                    if (cancelled) {
                        return;
                    }
                }
            }
        }
        return;
    }
    if (auto* call = dynamic_cast<const FunctionCallBase*>(node)) {
        if (const QoreParseListNode* parse_args = call->getParseArgs()) {
            for (size_t i = 0; i < parse_args->size(); ++i) {
                if (qoreIrListSizeHoistAnalysisCancelled(i)) {
                    cancelled = true;
                    return;
                }
                qoreIrCollectListSizeHoistCandidates(parse_args->get(i), list_candidates, invalidation_candidates,
                    value_candidates, cancelled);
                if (cancelled) {
                    return;
                }
            }
        }
        if (const QoreListNode* args = call->getArgs()) {
            ConstListIterator li(args);
            size_t count = 0;
            while (li.next()) {
                if (qoreIrListSizeHoistAnalysisCancelled(count++)) {
                    cancelled = true;
                    return;
                }
                qoreIrCollectListSizeHoistCandidates(li.getValue(), list_candidates, invalidation_candidates,
                    value_candidates, cancelled);
                if (cancelled) {
                    return;
                }
            }
        }
        return;
    }
    if (auto* call = dynamic_cast<const CallReferenceCallNode*>(node)) {
        qoreIrCollectListSizeHoistCandidates(call->getExp(), list_candidates, invalidation_candidates,
            value_candidates, cancelled);
        if (cancelled) {
            return;
        }
        if (const QoreParseListNode* parse_args = call->getParseArgs()) {
            for (size_t i = 0; i < parse_args->size(); ++i) {
                if (qoreIrListSizeHoistAnalysisCancelled(i)) {
                    cancelled = true;
                    return;
                }
                qoreIrCollectListSizeHoistCandidates(parse_args->get(i), list_candidates, invalidation_candidates,
                    value_candidates, cancelled);
                if (cancelled) {
                    return;
                }
            }
        }
        if (const QoreListNode* args = call->getArgs()) {
            ConstListIterator li(args);
            size_t count = 0;
            while (li.next()) {
                if (qoreIrListSizeHoistAnalysisCancelled(count++)) {
                    cancelled = true;
                    return;
                }
                qoreIrCollectListSizeHoistCandidates(li.getValue(), list_candidates, invalidation_candidates,
                    value_candidates, cancelled);
                if (cancelled) {
                    return;
                }
            }
        }
        return;
    }
    if (auto* binop = dynamic_cast<const QoreBinaryOperatorNode<>*>(node)) {
        qoreIrCollectListSizeHoistCandidates(binop->getLeft(), list_candidates, invalidation_candidates,
            value_candidates, cancelled);
        qoreIrCollectListSizeHoistCandidates(binop->getRight(), list_candidates, invalidation_candidates,
            value_candidates, cancelled);
        return;
    }
    if (auto* binop = dynamic_cast<const QoreBinaryOperatorNode<LValueOperatorNode>*>(node)) {
        qoreIrCollectListSizeHoistCandidates(binop->getLeft(), list_candidates, invalidation_candidates,
            value_candidates, cancelled);
        qoreIrCollectListSizeHoistCandidates(binop->getRight(), list_candidates, invalidation_candidates,
            value_candidates, cancelled);
        return;
    }
    if (auto* unop = dynamic_cast<const QoreSingleExpressionOperatorNode<>*>(node)) {
        qoreIrCollectListSizeHoistCandidates(unop->getExp(), list_candidates, invalidation_candidates,
            value_candidates, cancelled);
        return;
    }
    if (auto* unop = dynamic_cast<const QoreSingleExpressionOperatorNode<LValueOperatorNode>*>(node)) {
        qoreIrCollectListSizeHoistCandidates(unop->getExp(), list_candidates, invalidation_candidates,
            value_candidates, cancelled);
        return;
    }
    if (auto* sub = dynamic_cast<const QoreSquareBracketsOperatorNode*>(node)) {
        qoreIrCollectListSizeHoistCandidates(sub->getLeft(), list_candidates, invalidation_candidates,
            value_candidates, cancelled);
        qoreIrCollectListSizeHoistCandidates(sub->getRight(), list_candidates, invalidation_candidates,
            value_candidates, cancelled);
        return;
    }
    if (auto* hd = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(node)) {
        qoreIrCollectListSizeHoistCandidates(hd->getLeft(), list_candidates, invalidation_candidates,
            value_candidates, cancelled);
    }
}

static bool qoreIrStatementInvalidatesListSizeHoist(const AbstractStatement* stmt,
        const std::unordered_set<LocalVar*>& candidates) {
    if (!stmt) {
        return false;
    }
    if (auto* expr_stmt = dynamic_cast<const ExpressionStatement*>(stmt)) {
        return qoreIrExpressionInvalidatesListSizeHoist(expr_stmt->getExpression(), candidates);
    }
    if (auto* return_stmt = dynamic_cast<const ReturnStatement*>(stmt)) {
        return qoreIrExpressionInvalidatesListSizeHoist(return_stmt->getExpression(), candidates);
    }
    if (auto* if_stmt = dynamic_cast<const IfStatement*>(stmt)) {
        return qoreIrExpressionInvalidatesListSizeHoist(if_stmt->getCond(), candidates)
            || qoreIrStatementBlockInvalidatesListSizeHoist(if_stmt->getIfCode(), candidates)
            || qoreIrStatementBlockInvalidatesListSizeHoist(if_stmt->getElseCode(), candidates);
    }
    if (dynamic_cast<const WhileStatement*>(stmt) || dynamic_cast<const DoWhileStatement*>(stmt)
            || dynamic_cast<const ForStatement*>(stmt) || dynamic_cast<const ForEachStatement*>(stmt)
            || dynamic_cast<const TryStatement*>(stmt)) {
        return true;
    }
    return false;
}

static bool qoreIrStatementBlockInvalidatesListSizeHoist(const StatementBlock* block,
        const std::unordered_set<LocalVar*>& candidates) {
    if (!block) {
        return false;
    }
    size_t count = 0;
    for (const AbstractStatement* stmt : block->getStatements()) {
        if (qoreIrListSizeHoistAnalysisCancelled(count++)) {
            return true;
        }
        if (qoreIrStatementInvalidatesListSizeHoist(stmt, candidates)) {
            return true;
        }
    }
    return false;
}

static void qoreIrCollectListSizeHoistCandidatesFromBlock(const StatementBlock* block,
        std::unordered_set<LocalVar*>& list_candidates, std::unordered_set<LocalVar*>& invalidation_candidates,
        std::vector<QoreValue>& value_candidates, bool& cancelled) {
    if (cancelled || !block) {
        return;
    }
    size_t count = 0;
    for (const AbstractStatement* stmt : block->getStatements()) {
        if (qoreIrListSizeHoistAnalysisCancelled(count++)) {
            cancelled = true;
            return;
        }
        if (auto* expr_stmt = dynamic_cast<const ExpressionStatement*>(stmt)) {
            qoreIrCollectListSizeHoistCandidates(expr_stmt->getExpression(), list_candidates,
                invalidation_candidates, value_candidates, cancelled);
        } else if (auto* return_stmt = dynamic_cast<const ReturnStatement*>(stmt)) {
            qoreIrCollectListSizeHoistCandidates(return_stmt->getExpression(), list_candidates,
                invalidation_candidates, value_candidates, cancelled);
        } else if (auto* if_stmt = dynamic_cast<const IfStatement*>(stmt)) {
            qoreIrCollectListSizeHoistCandidates(if_stmt->getCond(), list_candidates, invalidation_candidates,
                value_candidates, cancelled);
            qoreIrCollectListSizeHoistCandidatesFromBlock(if_stmt->getIfCode(), list_candidates,
                invalidation_candidates, value_candidates, cancelled);
            qoreIrCollectListSizeHoistCandidatesFromBlock(if_stmt->getElseCode(), list_candidates,
                invalidation_candidates, value_candidates, cancelled);
        }
        if (cancelled) {
            return;
        }
    }
}

struct QoreIRLoopInvariantHoists {
    std::unordered_map<LocalVar*, QoreIRValue> list_sizes;
    std::unordered_map<const AbstractQoreNode*, QoreIRValue> values;

    bool empty() const {
        return list_sizes.empty() && values.empty();
    }
};

static QoreIRLoopInvariantHoists qoreIrHoistLoopInvariantValues(QoreIRLowering& lowering,
        QoreIRBuilder& builder, const QoreValue& cond, const StatementBlock* body,
        const QoreValue& iter, const QoreProgramLocation* loc, std::string& error) {
    std::unordered_set<LocalVar*> list_candidates;
    std::unordered_set<LocalVar*> invalidation_candidates;
    std::vector<QoreValue> value_candidates;
    bool cancelled = false;
    qoreIrCollectListSizeHoistCandidates(cond, list_candidates, invalidation_candidates, value_candidates,
        cancelled);
    qoreIrCollectListSizeHoistCandidates(iter, list_candidates, invalidation_candidates, value_candidates,
        cancelled);
    qoreIrCollectListSizeHoistCandidatesFromBlock(body, list_candidates, invalidation_candidates,
        value_candidates, cancelled);
    if (cancelled || (list_candidates.empty() && value_candidates.empty())) {
        return {};
    }
    if (qoreIrExpressionInvalidatesListSizeHoist(cond, invalidation_candidates)
            || qoreIrExpressionInvalidatesListSizeHoist(iter, invalidation_candidates)
            || qoreIrStatementBlockInvalidatesListSizeHoist(body, invalidation_candidates)) {
        return {};
    }

    std::vector<LocalVar*> ordered(list_candidates.begin(), list_candidates.end());
    std::sort(ordered.begin(), ordered.end(), [](LocalVar* a, LocalVar* b) {
        const char* an = a ? a->getName() : "";
        const char* bn = b ? b->getName() : "";
        int cmp = strcmp(an, bn);
        return cmp ? cmp < 0 : a < b;
    });
    for (size_t i = 0; i < ordered.size(); ++i) {
        if (qoreIrListSizeHoistAnalysisCancelled(i)) {
            return {};
        }
    }

    std::sort(value_candidates.begin(), value_candidates.end(), [](const QoreValue& a, const QoreValue& b) {
        return a.getInternalNode() < b.getInternalNode();
    });
    value_candidates.erase(std::unique(value_candidates.begin(), value_candidates.end(),
        [](const QoreValue& a, const QoreValue& b) {
            return a.getInternalNode() == b.getInternalNode();
        }), value_candidates.end());
    for (size_t i = 0; i < value_candidates.size(); ++i) {
        if (qoreIrListSizeHoistAnalysisCancelled(i)) {
            return {};
        }
    }

    QoreIRLoopInvariantHoists hoisted;
    builder.createPushTempMark(loc);
    size_t local_count = 0;
    for (LocalVar* local : ordered) {
        if (qoreIrListSizeHoistAnalysisCancelled(local_count++)) {
            builder.createDiscardTemps(loc);
            return {};
        }
        QoreIRValue list_value = builder.createLoadLocal(local, loc)->result;
        hoisted.list_sizes.emplace(local, builder.createListSize(list_value, loc)->result);
    }
    size_t value_count = 0;
    for (const QoreValue& value_expr : value_candidates) {
        if (qoreIrListSizeHoistAnalysisCancelled(value_count++)) {
            builder.createDiscardTemps(loc);
            return {};
        }
        QoreIRValue value = lowering.lowerExpression(value_expr, error);
        if (!value.isValid()) {
            builder.createDiscardTemps(loc);
            return {};
        }
        hoisted.values.emplace(value_expr.getInternalNode(), value);
    }
    builder.createDiscardTemps(loc);
    return hoisted;
}

void QoreIRLowering::setParseContext(QoreParseContext* n_parse_context) {
    parse_context = n_parse_context;
}

QoreIRValue QoreIRLowering::lowerConditionValue(const QoreValue& cond, std::string& error) {
    // BrIf calls getAsBool() on its operand, so ToBool is redundant here.
    // Skip the ToBool emission to reduce instruction count.
    return lowerExpression(cond, error);
}

QoreIRValue QoreIRLowering::tryPluginLowering(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    if (!node) {
        return QoreIRValue();
    }
    static const bool empty_fast_path_enabled =
        std::getenv("QORE_DISABLE_IR_PLUGIN_EMPTY_FAST_PATH") == nullptr;
    if (empty_fast_path_enabled && !qore_plugin_has_registered_operations()) {
        return QoreIRValue();
    }

    std::vector<QorePluginLoweringInfo> lowerers;
    if (qore_plugin_get_lowering_infos(parse_context ? parse_context->pgm : nullptr, node->getType(), lowerers,
            nullptr)) {
        error = "failed to query plugin lowering callbacks";
        return QoreIRValue();
    }

    QoreIRLoweringContext ctx(*this, error);
    size_t callback_count = 0;
    for (const QorePluginLoweringInfo& info : lowerers) {
        if (++callback_count % 100 == 0 && qore_check_cancel(nullptr, "plugin lowering callbacks")) {
            error = "plugin lowering callback dispatch cancelled";
            return QoreIRValue();
        }
        ctx.setResult(QoreIRValue());
        QorePluginLoweringResult result = info.lowering(&ctx, node, parse_context, &builder);
        if (!error.empty()) {
            return QoreIRValue();
        }

        switch (result) {
            case QorePluginLoweringResult::Lowered: {
                QoreIRValue lowered = ctx.getResult();
                if (lowered.isValid()) {
                    return lowered;
                }
                QoreIRBasicBlock* block = builder.getBlock();
                if (block && !block->instructions.empty() && block->instructions.back()->result.isValid()) {
                    return block->instructions.back()->result;
                }
                setPluginLoweringError(error, info, "lowered_without_result",
                    "returned Lowered but did not set a result or emit a result-producing instruction");
                return QoreIRValue();
            }
            case QorePluginLoweringResult::NotApplicable:
                if (pluginLoweringClaimsNode(info.claimed_node_kinds, node->getType())) {
                    setPluginLoweringError(error, info, "claimed_not_applicable",
                        "claimed the AST node kind but returned NotApplicable");
                    return QoreIRValue();
                }
                break;
            case QorePluginLoweringResult::Erroneous:
                if (error.empty()) {
                    setPluginLoweringError(error, info, "erroneous_without_diagnostic",
                        "returned Erroneous without setting a diagnostic");
                }
                return QoreIRValue();
            default:
                setPluginLoweringError(error, info, "invalid_result",
                    "returned an invalid QorePluginLoweringResult value");
                return QoreIRValue();
        }
    }

    return tryDescriptorPluginLowering(*this, builder, parse_context, expr, node, error);
}

bool QoreIRLowering::tryEmitFusedBranchIfLtLocalInt(const QoreValue& cond,
        QoreIRBasicBlock* true_target, QoreIRBasicBlock* false_target) {
    if (!cond.hasNode()) {
        return false;
    }
    auto* lt = dynamic_cast<const QoreLogicalLessThanOperatorNode*>(cond.getInternalNode());
    if (!lt) {
        return false;
    }
    // Both sides must be typed int local variables
    QoreIROpcode op = selectComparisonOpcode(lt->getLeft(), lt->getRight(),
        QoreIROpcode::LtInt, QoreIROpcode::LtFloat, QoreIROpcode::LtAny);
    if (op != QoreIROpcode::LtInt) {
        return false;
    }
    const AbstractQoreNode* left_node = lt->getLeft().getInternalNode();
    const AbstractQoreNode* right_node = lt->getRight().getInternalNode();
    auto* left_var = dynamic_cast<const VarRefNode*>(left_node);
    auto* right_var = dynamic_cast<const VarRefNode*>(right_node);
    if (!left_var || !right_var) {
        return false;
    }
    if (!isLocalNonClosureVar(left_var) || !isLocalNonClosureVar(right_var)) {
        return false;
    }
    builder.createBranchIfLtLocalInt(left_var->ref.id, right_var->ref.id,
        true_target, false_target, lt->loc);
    return true;
}

bool QoreIRLowering::lowerStatement(const AbstractStatement* stmt, std::string& error) {
    if (!stmt) {
        error = "null statement for IR lowering";
        return false;
    }
    if (!ensureBuilderContext(error)) {
        return false;
    }

    if (auto* block = dynamic_cast<const StatementBlock*>(stmt)) {
        return lowerStatementBlock(block, error);
    }
    if (auto* expr_stmt = dynamic_cast<const ExpressionStatement*>(stmt)) {
        QoreValue expr = expr_stmt->getExpression();
        if (expr) {
            if (!exception_stack.empty()) {
                QoreIRBasicBlock* handler = exception_stack.back();
                const AbstractQoreNode* node = expr.getInternalNode();
                if (node) {
                    std::vector<QoreIRValue> operands;
                    const QoreProgramLocation* loc = nullptr;
                    bool invoked = false;
                    // FunctionCallNode, SelfFunctionCallNode, StaticMethodCallNode, and
                    // CallReferenceCallNode are NOT handled here — they fall through to
                    // lowerExpression() which dispatches to lowerFunctionCall(),
                    // lowerSelfCall(), lowerStaticCall(), and lowerCallReference()
                    // respectively. Those functions correctly set invoke_opcode on the
                    // Invoke instruction, while this early path would create an Invoke
                    // with default invoke_opcode, causing the IR interpreter to
                    // re-evaluate the full AST expression — double-executing side effects
                    // in arguments (e.g. assertTrue(obj.doWork()) would call doWork()
                    // twice).
                    if (dynamic_cast<const LValueOperatorNode*>(node)) {
                        // Lvalue operators have dedicated lowering that preserves
                        // mutation semantics and emits LValuePath instructions
                        // where possible. Pre-evaluating their operands here
                        // would force a generic Invoke of the original AST node.
                    } else if (auto* cast = dynamic_cast<const QoreCastOperatorNode*>(node)) {
                        const QoreSingleExpressionOperatorNode<>* cast_node = cast;
                        QoreIRValue value = lowerExpression(cast_node->getExp(), error);
                        if (!value.isValid()) {
                            return false;
                        }
                        operands.push_back(value);
                        loc = cast_node->loc;
                        invoked = true;
                    } else if (auto* parse_cast = dynamic_cast<const QoreParseCastOperatorNode*>(node)) {
                        const QoreSingleExpressionOperatorNode<>* cast_node = parse_cast;
                        QoreIRValue value = lowerExpression(cast_node->getExp(), error);
                        if (!value.isValid()) {
                            return false;
                        }
                        operands.push_back(value);
                        loc = cast_node->loc;
                        invoked = true;
                    } else if (auto* extract = dynamic_cast<const QoreExtractOperatorNode*>(node)) {
                        QoreIRValue lvalue = lowerExpression(extract->getLValue(), error);
                        if (!lvalue.isValid()) {
                            return false;
                        }
                        QoreIRValue offset = lowerExpression(extract->getOffset(), error);
                        if (!offset.isValid()) {
                            return false;
                        }
                        QoreIRValue length = extract->getLength().isNothing()
                            ? builder.createConstNothing(extract->loc)->result
                            : lowerExpression(extract->getLength(), error);
                        if (!length.isValid()) {
                            return false;
                        }
                        QoreIRValue replacement = extract->getNewValue().isNothing()
                            ? builder.createConstNothing(extract->loc)->result
                            : lowerExpression(extract->getNewValue(), error);
                        if (!replacement.isValid()) {
                            return false;
                        }
                        operands.push_back(lvalue);
                        operands.push_back(offset);
                        operands.push_back(length);
                        operands.push_back(replacement);
                        loc = extract->loc;
                        invoked = true;
                    } else if (auto* keys = dynamic_cast<const QoreKeysOperatorNode*>(node)) {
                        QoreIRValue operand = lowerExpression(keys->getExp(), error);
                        if (!operand.isValid()) {
                            return false;
                        }
                        operands.push_back(operand);
                        loc = keys->loc;
                        invoked = true;
                    } else if (auto* regex = dynamic_cast<const QoreRegexMatchOperatorNode*>(node)) {
                        QoreIRValue operand = lowerExpression(regex->getExp(), error);
                        if (!operand.isValid()) {
                            return false;
                        }
                        operands.push_back(operand);
                        loc = regex->loc;
                        invoked = true;
                    } else if (auto* regex = dynamic_cast<const QoreRegexExtractOperatorNode*>(node)) {
                        QoreIRValue operand = lowerExpression(regex->getExp(), error);
                        if (!operand.isValid()) {
                            return false;
                        }
                        operands.push_back(operand);
                        loc = regex->loc;
                        invoked = true;
                    } else if (auto* regex = dynamic_cast<const QoreRegexSubstOperatorNode*>(node)) {
                        QoreIRValue operand = lowerExpression(regex->getExp(), error);
                        if (!operand.isValid()) {
                            return false;
                        }
                        operands.push_back(operand);
                        loc = regex->loc;
                        invoked = true;
                    } else if (auto* exists = dynamic_cast<const QoreExistsOperatorNode*>(node)) {
                        QoreIRValue operand = lowerExpression(exists->getExp(), error);
                        if (!operand.isValid()) {
                            return false;
                        }
                        operands.push_back(operand);
                        loc = exists->loc;
                        invoked = true;
                    } else if (auto* elements = dynamic_cast<const QoreElementsOperatorNode*>(node)) {
                        QoreIRValue operand = lowerExpression(elements->getExp(), error);
                        if (!operand.isValid()) {
                            return false;
                        }
                        operands.push_back(operand);
                        loc = elements->loc;
                        invoked = true;
                    } else {
                        // NOTE: QoreDotEvalOperatorNode is intentionally NOT handled here.
                        // It falls through to lowerExpression() → lowerDotEval() which creates
                        // InvokeDotEvalMethodDirect with proper exception handling. The generic
                        // Invoke path here would set invoke_opcode=Invoke with the full AST
                        // expression, which works for IR execution but fails in AOT mode because
                        // QoreDotEvalOperatorNode cannot be serialized — the expr becomes null
                        // after AOT deserialization, silently dropping the method call.
                        // NOTE: QoreQuestionMarkOperatorNode (ternary ?:) is intentionally
                        // NOT pre-evaluated here.  Pre-evaluating all three operands eagerly
                        // breaks short-circuit semantics — both the "then" and "else" branches
                        // would be executed regardless of the condition, causing side effects
                        // (e.g. calling a NOTHING closure reference).  Ternary expressions
                        // are correctly lowered by lowerQuestionMark() via lowerExpression(),
                        // which creates proper branching with BranchIf so only the selected
                        // branch is evaluated.  Sub-expressions inside each branch will still
                        // get their own Invoke instructions for exception handling.
                        if (auto* binary = dynamic_cast<const QoreBinaryOperatorNode<>*>(node)) {
                            // map, select, foldl, foldr operators use implicit arguments
                            // ($1, $#) set up during operator evaluation — their child
                            // expressions must NOT be pre-evaluated outside that context.
                            //
                            // Skip pre-evaluation for:
                            // - Map/select/foldl operators: use implicit context ($1, $#)
                            //   that is only valid inside the operator's body.
                            // - Lvalue operators (assignments, +=, -=, etc.): have their own
                            //   lowering in lowerExpression() that properly creates Invoke
                            //   instructions for sub-expressions; pre-evaluating here would
                            //   create an Invoke that re-evaluates the full expression via AST.
                            if (!dynamic_cast<const QoreMapOperatorNode*>(node)
                                && !dynamic_cast<const QoreSelectOperatorNode*>(node)
                                && !dynamic_cast<const QoreFoldlOperatorNode*>(node)
                                && !dynamic_cast<const QoreBinaryLValueOperatorNode*>(node)
                                && !dynamic_cast<const QoreBinaryIntLValueOperatorNode*>(node)) {
                                QoreIRValue left = lowerExpression(binary->getLeft(), error);
                                if (!left.isValid()) {
                                    return false;
                                }
                                QoreIRValue right = lowerExpression(binary->getRight(), error);
                                if (!right.isValid()) {
                                    return false;
                                }
                                operands.push_back(left);
                                operands.push_back(right);
                                loc = binary->loc;
                                invoked = true;
                            }
                        } else if (auto* unary = dynamic_cast<const QoreSingleExpressionOperatorNode<>*>(node)) {
                            // BackgroundOperatorNode inherits from QoreSingleExpressionOperatorNode
                            // but must NOT be handled here — lowering the inner expression would
                            // execute the function call directly, then the Invoke would also
                            // execute background t(...) via AST delegation, causing double execution
                            if (!dynamic_cast<const QoreBackgroundOperatorNode*>(node)) {
                                QoreIRValue value = lowerExpression(unary->getExp(), error);
                                if (!value.isValid()) {
                                    return false;
                                }
                                operands.push_back(value);
                                loc = unary->loc;
                                invoked = true;
                            }
                        }
                    }
                    bool use_invoke = handler && (!parse_context || parse_context->expressionCanThrow());
                    if (invoked && use_invoke) {
                        QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
                        if (!normal_block) {
                            error = "IR builder failed to create invoke continuation block";
                            return false;
                        }
                        builder.createInvoke(expr, operands, normal_block, handler, loc);
                        builder.setBlock(normal_block);
                        return true;
                    }
                }
            }
            QoreIRValue lowered = lowerExpression(expr, error);
            if (!lowered.isValid()) {
                return false;
            }
        }
        return true;
    }
    if (auto* ret_stmt = dynamic_cast<const ReturnStatement*>(stmt)) {
        QoreValue expr = ret_stmt->getExpression();
        if (!expr || expr.isNothing()) {
            // Emit block cleanups for all active scopes before returning.
            // Include lvar cleanup here so object destructors run before the
            // source-level caller resumes.  The function wrapper still pops
            // pre-instantiated local slots later; UninstantiateLocal clears the
            // value without corrupting that stack ownership contract.
            // Also handles RefForeach cleanup (record + finalize without fill remaining).
            if (!emitBlockCleanups(0, error, false)) {
                return false;
            }
            // Emit CatchCleanup for all active catch scopes before returning
            for (int i = 0; i < catch_cleanup_depth; ++i) {
                builder.createCatchCleanup(stmt->loc);
            }
            builder.createReturnNothing();
            return true;
        }
        // Evaluate the return expression BEFORE firing on_exit handlers.
        // In AST mode, ReturnStatement::execImpl evaluates the expression first,
        // then the enclosing StatementBlock fires on_block_exit handlers after
        // the statement loop breaks on RC_RETURN.  The return expression may
        // reference resources (files, connections) that on_exit handlers clean up,
        // so we must preserve this ordering in the IR path.
        QoreIRValue lowered = lowerExpression(expr, error);
        if (!lowered.isValid()) {
            return false;
        }
        bool cleanup_before_return = catch_cleanup_depth > 0;
        if (!cleanup_before_return) {
            for (const auto& entry : cleanup_stack) {
                if (entry.type == BlockCleanupEntry::Scope
                        || entry.type == BlockCleanupEntry::Lvars
                        || entry.type == BlockCleanupEntry::CatchVar
                        || entry.type == BlockCleanupEntry::RefForeachRecord
                        || entry.type == BlockCleanupEntry::RefForeach
                        || entry.type == BlockCleanupEntry::Context) {
                    cleanup_before_return = true;
                    break;
                }
            }
        }
        if (cleanup_before_return) {
            lowered = builder.createRefSelf(lowered, stmt->loc)->result;
        }
        // Emit block cleanups for all active scopes (fires on_exit handlers,
        // clears lvar values, and handles RefForeach cleanup).
        if (!emitBlockCleanups(0, error, false)) {
            return false;
        }
        // Emit CatchCleanup for all active catch scopes before returning
        for (int i = 0; i < catch_cleanup_depth; ++i) {
            builder.createCatchCleanup(stmt->loc);
        }
        builder.createReturn(lowered);
        return true;
    }
    if (auto* if_stmt = dynamic_cast<const IfStatement*>(stmt)) {
        // Match AST-mode condition lifetime: temporaries created while
        // evaluating the condition must be released before either branch body
        // mutates state.  Otherwise a condition such as `if (!h{k})` can keep
        // `h` referenced while the then-block assigns `h{k}`, forcing COW of
        // the whole hash on every insertion.
        bool condition_needs_cleanup = expressionMayCreateNodeTemp(if_stmt->getCond());
        if (condition_needs_cleanup) {
            builder.createPushTempMark(if_stmt->loc);
        }
        QoreIRValue cond = lowerConditionValue(if_stmt->getCond(), error);
        if (!cond.isValid()) {
            return false;
        }
        QoreIRValue cond_bool = !condition_needs_cleanup || conditionValueSurvivesDiscardTemps(if_stmt->getCond())
            ? cond
            : builder.createUnaryOp(QoreIROpcode::ToBool, cond, if_stmt->loc)->result;
        if (condition_needs_cleanup) {
            builder.createDiscardTemps(if_stmt->loc);
        }
        QoreIRBasicBlock* then_block = createBlock("if.then");
        QoreIRBasicBlock* merge_block = createBlock("if.merge");
        QoreIRBasicBlock* else_block = if_stmt->getElseCode() ? createBlock("if.else") : merge_block;
        if (!then_block || !merge_block || !else_block) {
            error = "IR builder failed to create blocks for if";
            return false;
        }
        builder.createBranchIf(cond_bool, then_block, else_block);

        builder.setBlock(then_block);
        if (!lowerStatementBlock(if_stmt->getIfCode(), error)) {
            return false;
        }
        if (!blockHasTerminator(builder.getBlock())) {
            builder.createBranch(merge_block);
        }

        if (if_stmt->getElseCode()) {
            builder.setBlock(else_block);
            if (!lowerStatementBlock(if_stmt->getElseCode(), error)) {
                return false;
            }
            if (!blockHasTerminator(builder.getBlock())) {
                builder.createBranch(merge_block);
            }
        }

        // Move merge_block to end so LLVM lowering processes it after any
        // invoke.cont blocks created during the then/else body lowering.
        builder.getFunction()->moveBlockToEnd(merge_block);
        builder.setBlock(merge_block);
        return true;
    }
    if (auto* do_stmt = dynamic_cast<const DoWhileStatement*>(stmt)) {
        QoreIRBasicBlock* body_block = createBlock("do.body");
        QoreIRBasicBlock* cond_block = createBlock("do.cond");
        QoreIRBasicBlock* exit_block = createBlock("do.exit");
        if (!body_block || !cond_block || !exit_block) {
            error = "IR builder failed to create blocks for do-while";
            return false;
        }
        // Mark condition block as loop header for OSR detection
        cond_block->is_loop_header = true;
        if (!blockHasTerminator(builder.getBlock())) {
            builder.createBranch(body_block);
        }

        builder.setBlock(body_block);
        flow_stack.push_back({exit_block, cond_block, false, catch_cleanup_depth, cleanup_stack.size(), QoreIRValue()});
        // Active enclosing-loop exit for debugger DebugFlowBreak resolution (see while case).
        QoreIRBasicBlock* prev_loop_exit = current_loop_exit;
        current_loop_exit = exit_block;
        body_block->enclosing_loop_exit = exit_block;
        ++loop_depth;
        if (!lowerStatementBlock(do_stmt->getCode(), error)) {
            --loop_depth;
            current_loop_exit = prev_loop_exit;
            flow_stack.pop_back();
            return false;
        }
        --loop_depth;
        current_loop_exit = prev_loop_exit;
        if (!blockHasTerminator(builder.getBlock())) {
            auto* br = builder.createBranch(cond_block);
            setLoopCheckpointExceptionTarget(br, cond_block);
        }
        flow_stack.pop_back();

        builder.setBlock(cond_block);
        // Bracket the condition with PushTempMark/DiscardTemps — see the
        // WhileStatement case below for the rationale (per-iteration node
        // temps from weak-ref LoadLocal in the condition must not persist).
        bool condition_needs_cleanup = expressionMayCreateNodeTemp(do_stmt->getCond());
        if (condition_needs_cleanup) {
            builder.createPushTempMark(stmt->loc);
        }
        QoreIRValue cond = lowerConditionValue(do_stmt->getCond(), error);
        if (!cond.isValid()) {
            return false;
        }
        // Convert node values to scalar bool so the branch operand survives DiscardTemps.
        QoreIRValue cond_bool = !condition_needs_cleanup || conditionValueSurvivesDiscardTemps(do_stmt->getCond())
            ? cond
            : builder.createUnaryOp(QoreIROpcode::ToBool, cond, stmt->loc)->result;
        if (condition_needs_cleanup) {
            builder.createDiscardTemps(stmt->loc);
        }
        builder.createBranchIf(cond_bool, body_block, exit_block);

        // Move exit_block to end so LLVM lowering processes it after any
        // invoke.cont blocks created during the do-while body lowering.
        builder.getFunction()->moveBlockToEnd(exit_block);
        builder.setBlock(exit_block);

        // Emit UninstantiateLocal for DoWhileStatement loop variables in reverse order
        if (const LVList* loop_lvars = do_stmt->getLVList()) {
            for (int i = static_cast<int>(loop_lvars->size()) - 1; i >= 0; --i) {
                builder.createUninstantiateLocal(loop_lvars->lv[i], stmt->loc);
            }
        }

        return true;
    }
    if (auto* while_stmt = dynamic_cast<const WhileStatement*>(stmt)) {
        QoreIRBasicBlock* cond_block = createBlock("while.cond");
        QoreIRBasicBlock* body_block = createBlock("while.body");
        QoreIRBasicBlock* exit_block = createBlock("while.exit");
        if (!cond_block || !body_block || !exit_block) {
            error = "IR builder failed to create blocks for while";
            return false;
        }
        // Mark condition block as loop header for OSR detection
        cond_block->is_loop_header = true;
        QoreIRLoopInvariantHoists invariant_hoists;
        if (!blockHasTerminator(builder.getBlock())) {
            invariant_hoists = qoreIrHoistLoopInvariantValues(*this, builder, while_stmt->getCond(),
                while_stmt->getCode(), QoreValue(), while_stmt->loc, error);
            if (!error.empty()) {
                return false;
            }
        }
        if (!blockHasTerminator(builder.getBlock())) {
            auto* br = builder.createBranch(cond_block);
            setLoopCheckpointExceptionTarget(br, cond_block);
        }
        bool pushed_invariant_list_sizes = !invariant_hoists.list_sizes.empty();
        bool pushed_invariant_values = !invariant_hoists.values.empty();
        if (pushed_invariant_list_sizes) {
            loop_invariant_list_sizes.push_back(std::move(invariant_hoists.list_sizes));
        }
        if (pushed_invariant_values) {
            loop_invariant_values.push_back(std::move(invariant_hoists.values));
        }
        auto pop_invariant_hoists = [&]() {
            if (pushed_invariant_values) {
                loop_invariant_values.pop_back();
            }
            if (pushed_invariant_list_sizes) {
                loop_invariant_list_sizes.pop_back();
            }
        };

        builder.setBlock(cond_block);
        // Bracket the condition with PushTempMark/DiscardTemps so per-iteration
        // node temps (e.g., LoadLocal of a weak-ref local in the condition) get
        // cleaned up every iteration, matching AST-mode ValueEvalRefHolder timing.
        // Without this, condition temps accumulate in the cleanup vector across
        // iterations and any refSelf'd node values hold the object alive
        // indefinitely (thread-object.qtest transparent thread pattern: worker
        // loops on `self.stop` where self holds a weak ref; each iteration's
        // LoadLocal refSelfs the unwrapped object, preventing the strong ref
        // from dropping to zero when the outer scope releases its ref).
        //
        // The cond value may itself be a node (e.g. `while (data)` where data
        // is a string — BranchIf calls getAsBool() on whatever type), so we
        // convert to a scalar bool BEFORE DiscardTemps to prevent the cond
        // value from being dropped.  The ToBool result is a scalar not in the
        // cleanup vector, so it survives DiscardTemps.
        // Try fused BranchIfLtLocalInt for int local < int local conditions
        if (!tryEmitFusedBranchIfLtLocalInt(while_stmt->getCond(), body_block, exit_block)) {
            bool condition_needs_cleanup = expressionMayCreateNodeTemp(while_stmt->getCond());
            if (condition_needs_cleanup) {
                builder.createPushTempMark(while_stmt->loc);
            }
            QoreIRValue cond = lowerConditionValue(while_stmt->getCond(), error);
            if (!cond.isValid()) {
                pop_invariant_hoists();
                return false;
            }
            // Convert node values to scalar bool so the branch operand survives DiscardTemps.
            QoreIRValue cond_bool = !condition_needs_cleanup
                    || conditionValueSurvivesDiscardTemps(while_stmt->getCond())
                ? cond
                : builder.createUnaryOp(QoreIROpcode::ToBool, cond, while_stmt->loc)->result;
            if (condition_needs_cleanup) {
                builder.createDiscardTemps(while_stmt->loc);
            }
            builder.createBranchIf(cond_bool, body_block, exit_block);
        }
        // Fused path: int comparisons don't create node temps, no cleanup needed.

        builder.setBlock(body_block);
        flow_stack.push_back({exit_block, cond_block, false, catch_cleanup_depth, cleanup_stack.size(), QoreIRValue()});
        // Mark this loop's exit as the active enclosing-loop exit for blocks lowered in
        // the body (body_block itself was created before this point, so stamp it too).
        QoreIRBasicBlock* prev_loop_exit = current_loop_exit;
        current_loop_exit = exit_block;
        body_block->enclosing_loop_exit = exit_block;
        ++loop_depth;
        if (!lowerStatementBlock(while_stmt->getCode(), error)) {
            --loop_depth;
            current_loop_exit = prev_loop_exit;
            flow_stack.pop_back();
            pop_invariant_hoists();
            return false;
        }
        --loop_depth;
        current_loop_exit = prev_loop_exit;
        if (!blockHasTerminator(builder.getBlock())) {
            auto* br = builder.createBranch(cond_block);
            setLoopCheckpointExceptionTarget(br, cond_block);
        }
        flow_stack.pop_back();

        // Move exit_block to end so LLVM lowering processes it after any
        // invoke.cont blocks created during the while body lowering.
        builder.getFunction()->moveBlockToEnd(exit_block);
        builder.setBlock(exit_block);

        // Emit UninstantiateLocal for WhileStatement loop variables in reverse order
        if (const LVList* loop_lvars = while_stmt->getLVList()) {
            for (int i = static_cast<int>(loop_lvars->size()) - 1; i >= 0; --i) {
                builder.createUninstantiateLocal(loop_lvars->lv[i], stmt->loc);
            }
        }
        pop_invariant_hoists();

        return true;
    }
    if (auto* for_stmt = dynamic_cast<const ForStatement*>(stmt)) {
        QoreIRBasicBlock* cond_block = createBlock("for.cond");
        QoreIRBasicBlock* body_block = createBlock("for.body");
        QoreIRBasicBlock* iter_block = createBlock("for.iter");
        QoreIRBasicBlock* exit_block = createBlock("for.exit");
        if (!cond_block || !body_block || !iter_block || !exit_block) {
            error = "IR builder failed to create blocks for for";
            return false;
        }
        // Mark condition block as loop header for OSR detection
        cond_block->is_loop_header = true;
        QoreValue init = for_stmt->getAssignment();
        if (init && !init.isNothing()) {
            QoreIRValue lowered = lowerExpression(init, error);
            if (!lowered.isValid()) {
                return false;
            }
        }
        QoreValue cond_expr = for_stmt->getCond();
        QoreValue iter_expr = for_stmt->getIterator();
        QoreIRLoopInvariantHoists invariant_hoists;
        if (!blockHasTerminator(builder.getBlock())) {
            invariant_hoists = qoreIrHoistLoopInvariantValues(*this, builder, cond_expr,
                for_stmt->getCode(), iter_expr, for_stmt->loc, error);
            if (!error.empty()) {
                return false;
            }
        }
        if (!blockHasTerminator(builder.getBlock())) {
            auto* br = builder.createBranch(cond_block);
            setLoopCheckpointExceptionTarget(br, cond_block);
        }
        bool pushed_invariant_list_sizes = !invariant_hoists.list_sizes.empty();
        bool pushed_invariant_values = !invariant_hoists.values.empty();
        if (pushed_invariant_list_sizes) {
            loop_invariant_list_sizes.push_back(std::move(invariant_hoists.list_sizes));
        }
        if (pushed_invariant_values) {
            loop_invariant_values.push_back(std::move(invariant_hoists.values));
        }
        auto pop_invariant_hoists = [&]() {
            if (pushed_invariant_values) {
                loop_invariant_values.pop_back();
            }
            if (pushed_invariant_list_sizes) {
                loop_invariant_list_sizes.pop_back();
            }
        };

        builder.setBlock(cond_block);
        // Try fused BranchIfLtLocalInt for int local < int local conditions
        if (cond_expr && !cond_expr.isNothing()
                && tryEmitFusedBranchIfLtLocalInt(cond_expr, body_block, exit_block)) {
            // Fused condition+branch emitted — no node temps, no cleanup needed.
        } else {
            // Bracket the condition with PushTempMark/DiscardTemps — see the
            // WhileStatement case above for the rationale (per-iteration node
            // temps from weak-ref LoadLocal in the condition must not persist).
            bool condition_needs_cleanup = cond_expr && !cond_expr.isNothing()
                && expressionMayCreateNodeTemp(cond_expr);
            if (condition_needs_cleanup) {
                builder.createPushTempMark(stmt->loc);
            }
            QoreIRValue cond_value;
            if (!cond_expr || cond_expr.isNothing()) {
                cond_value = builder.createConstBool(true)->result;
            } else {
                cond_value = lowerConditionValue(cond_expr, error);
                if (!cond_value.isValid()) {
                    pop_invariant_hoists();
                    return false;
                }
            }
            // Convert node values to scalar bool so the branch operand survives DiscardTemps.
            QoreIRValue cond_bool = (!condition_needs_cleanup
                    || !cond_expr || cond_expr.isNothing() || conditionValueSurvivesDiscardTemps(cond_expr))
                ? cond_value
                : builder.createUnaryOp(QoreIROpcode::ToBool, cond_value, stmt->loc)->result;
            if (condition_needs_cleanup) {
                builder.createDiscardTemps(stmt->loc);
            }
            builder.createBranchIf(cond_bool, body_block, exit_block);
        }

        builder.setBlock(body_block);
        flow_stack.push_back({exit_block, iter_block, false, catch_cleanup_depth, cleanup_stack.size(), QoreIRValue()});
        // Active enclosing-loop exit for debugger DebugFlowBreak resolution (see while case).
        QoreIRBasicBlock* prev_loop_exit = current_loop_exit;
        current_loop_exit = exit_block;
        body_block->enclosing_loop_exit = exit_block;
        iter_block->enclosing_loop_exit = exit_block;
        ++loop_depth;
        if (!lowerStatementBlock(for_stmt->getCode(), error)) {
            --loop_depth;
            current_loop_exit = prev_loop_exit;
            flow_stack.pop_back();
            pop_invariant_hoists();
            return false;
        }
        --loop_depth;
        if (!blockHasTerminator(builder.getBlock())) {
            builder.createBranch(iter_block);
        }
        flow_stack.pop_back();

        builder.setBlock(iter_block);
        if (iter_expr && !iter_expr.isNothing()) {
            QoreIRValue lowered = lowerExpression(iter_expr, error);
            if (!lowered.isValid()) {
                current_loop_exit = prev_loop_exit;
                pop_invariant_hoists();
                return false;
            }
        }
        current_loop_exit = prev_loop_exit;
        if (!blockHasTerminator(builder.getBlock())) {
            auto* br = builder.createBranch(cond_block);
            setLoopCheckpointExceptionTarget(br, cond_block);
        }

        // Move exit_block to end so LLVM lowering processes it after any
        // invoke.cont blocks created during the for body lowering.
        builder.getFunction()->moveBlockToEnd(exit_block);
        builder.setBlock(exit_block);

        // Emit UninstantiateLocal for ForStatement loop variables in reverse order
        // (reverse order ensures destructors are called in LIFO order like in AST mode)
        // The loop variables are declared in the for statement's initialization (e.g., "int j = 2")
        // and must be cleaned up after the loop exits.
        if (const LVList* loop_lvars = for_stmt->getLVList()) {
            for (int i = static_cast<int>(loop_lvars->size()) - 1; i >= 0; --i) {
                builder.createUninstantiateLocal(loop_lvars->lv[i], stmt->loc);
            }
        }
        pop_invariant_hoists();

        return true;
    }
    if (auto* foreach_stmt = dynamic_cast<const ForEachStatement*>(stmt)) {
        // Reference iteration (foreach x in \list): natively compile with try/catch
        // wrapping for exception safety.  The runtime helpers manage the opaque
        // RefForeachState (init, get entry, record modified value, finalize/cleanup).
        if (foreach_stmt->isRef()) {
            // Phase 0: Initialize ref foreach state from ParseReferenceNode
            QoreValue list_expr = foreach_stmt->getList();
            auto* init_inst = builder.createRefForeachInit(list_expr, stmt->loc);
            if (!exception_stack.empty()) {
                init_inst->exception_target = exception_stack.back();
            }
            QoreIRValue state = init_inst->result;

            // Check state != 0 (init failed → exception already set, skip loop)
            QoreIRBasicBlock* check_size_block = createBlock("ref_foreach.check_size");
            QoreIRBasicBlock* merge_block = createBlock("ref_foreach.merge");
            if (!check_size_block || !merge_block) {
                error = "IR builder failed to create blocks for ref foreach";
                return false;
            }
            QoreIRValue zero = builder.createConstInt(0, stmt->loc)->result;
            QoreIRValue state_ok = builder.createBinaryOp(QoreIROpcode::NeInt,
                state, zero, stmt->loc)->result;
            builder.createBranchIf(state_ok, check_size_block, merge_block);

            // Phase 1: Check iteration count > 0
            builder.setBlock(check_size_block);
            QoreIRValue size = builder.createRefForeachSize(state, stmt->loc)->result;
            QoreIRBasicBlock* cleanup_empty_block = createBlock("ref_foreach.cleanup_empty");
            QoreIRBasicBlock* try_entry_block = createBlock("ref_foreach.try_entry");
            if (!cleanup_empty_block || !try_entry_block) {
                error = "IR builder failed to create blocks for ref foreach";
                return false;
            }
            QoreIRValue size_gt_0 = builder.createBinaryOp(QoreIROpcode::GtInt,
                size, zero, stmt->loc)->result;
            builder.createBranchIf(size_gt_0, try_entry_block, cleanup_empty_block);

            // Empty case: cleanup without write-back
            builder.setBlock(cleanup_empty_block);
            builder.createRefForeachCleanup(state, stmt->loc);
            builder.createBranch(merge_block);

            // Phase 2: Try/catch wrapped loop
            builder.setBlock(try_entry_block);

            // Push RefForeach entry to cleanup stack (for return path finalize)
            BlockCleanupEntry ref_foreach_entry;
            ref_foreach_entry.type = BlockCleanupEntry::RefForeach;
            ref_foreach_entry.ref_foreach_state = state;
            ref_foreach_entry.loc = stmt->loc;
            cleanup_stack.push_back(ref_foreach_entry);

            // Set up try scope for exception safety
            uint32_t try_scope_id = ++scope_counter;
            scope_stack.push_back(try_scope_id);
            {
                BlockCleanupEntry scope_entry;
                scope_entry.type = BlockCleanupEntry::Scope;
                scope_entry.scope_id = try_scope_id;
                // Try-level scope registers no handlers itself; anchor
                // handler_start to the current block_handlers size so
                // emitBlockCleanups() on return unwinding doesn't re-inline
                // enclosing-block handlers (see identical guard in the
                // TryStatement lowering path).
                scope_entry.handler_start = block_handlers.size();
                cleanup_stack.push_back(scope_entry);
            }
            builder.createScopeEnter(try_scope_id);

            // Create loop structure blocks
            QoreIRBasicBlock* preheader_block = createBlock("ref_foreach.preheader");
            QoreIRBasicBlock* header_block = createBlock("ref_foreach.header");
            QoreIRBasicBlock* body_block = createBlock("ref_foreach.body");
            QoreIRBasicBlock* latch_block = createBlock("ref_foreach.latch");
            QoreIRBasicBlock* break_handler_block = createBlock("ref_foreach.break_handler");
            QoreIRBasicBlock* exit_normal_block = createBlock("ref_foreach.exit_normal");
            QoreIRBasicBlock* catch_block = createBlock("ref_foreach.catch");
            if (!preheader_block || !header_block || !body_block || !latch_block
                    || !break_handler_block || !exit_normal_block || !catch_block) {
                error = "IR builder failed to create blocks for ref foreach loop";
                scope_stack.pop_back();
                cleanup_stack.pop_back();  // Scope
                cleanup_stack.pop_back();  // RefForeach
                return false;
            }
            header_block->is_loop_header = true;

            // Preheader: initialize index counter
            builder.createBranch(preheader_block, stmt->loc);
            builder.setBlock(preheader_block);
            QoreIRValue init_index = builder.createConstInt(0, stmt->loc)->result;
            {
                auto* br = builder.createBranch(header_block, stmt->loc);
                setLoopCheckpointExceptionTarget(br, header_block, catch_block);
            }

            // Header: PHI for index, compare with size
            builder.setBlock(header_block);
            auto* index_phi = builder.createPhi({}, stmt->loc, QoreIRPhiValueKind::NativeInt);
            QoreIRValue index_val = index_phi->result;
            QoreIRValue cmp_val = builder.createBinaryOp(QoreIROpcode::LtInt,
                index_val, size, stmt->loc)->result;
            builder.createBranchIf(cmp_val, body_block, exit_normal_block);

            // Body: get entry, assign to loop var, push $#, execute body
            builder.setBlock(body_block);

            // Push exception handler for body instructions
            QoreIRBasicBlock* prev_guard_override = guard_exception_target_override;
            guard_exception_target_override = catch_block;
            exception_stack.push_back(catch_block);
            size_t try_scope_depth = scope_stack.size() - 1;  // depth before try scope
            exception_scope_depth_stack.push_back(try_scope_depth);

            // Get entry and assign to loop variable
            auto* get_entry_inst = builder.createRefForeachGetEntry(state, index_val, stmt->loc);
            get_entry_inst->exception_target = catch_block;
            QoreIRValue entry_val = get_entry_inst->result;

            QoreValue var_expr = foreach_stmt->getVar();
            if (var_expr && !var_expr.isNothing()) {
                auto* store_inst = builder.createStoreLValue(var_expr, entry_val, stmt->loc);
                store_inst->exception_target = catch_block;
            }

            // Push implicit element ($#) for the current iteration
            QoreIRValue old_element = builder.createPushImplicitElement(index_val, stmt->loc)->result;

            // Push RefForeachRecord entry to cleanup stack (for return path record)
            BlockCleanupEntry record_entry;
            record_entry.type = BlockCleanupEntry::RefForeachRecord;
            record_entry.ref_foreach_state = state;
            record_entry.old_implicit_element = old_element;
            record_entry.var_expr = var_expr;
            record_entry.loc = stmt->loc;
            cleanup_stack.push_back(record_entry);

            // Set up flow target: break → break_handler, continue → latch
            // old_implicit_element is INVALID — the break_handler block handles $# pop
            FlowTarget ft;
            ft.break_target = break_handler_block;
            ft.continue_target = latch_block;
            ft.is_switch = false;
            ft.catch_cleanup_depth = catch_cleanup_depth;
            ft.cleanup_stack_depth = cleanup_stack.size();
            flow_stack.push_back(ft);

            // Lower the loop body
            StatementBlock* body = foreach_stmt->getCode();
            if (body) {
                ++loop_depth;
                if (!lowerStatementBlock(body, error)) {
                    --loop_depth;
                    flow_stack.pop_back();
                    cleanup_stack.pop_back();  // RefForeachRecord
                    exception_stack.pop_back();
                    exception_scope_depth_stack.pop_back();
                    guard_exception_target_override = prev_guard_override;
                    scope_stack.pop_back();
                    cleanup_stack.pop_back();  // Scope
                    cleanup_stack.pop_back();  // RefForeach
                    return false;
                }
                --loop_depth;
            }
            flow_stack.pop_back();

            // Pop RefForeachRecord from cleanup stack (only needed during body lowering)
            cleanup_stack.pop_back();

            // Normal body exit falls through to latch block
            if (!blockHasTerminator(builder.getBlock())) {
                builder.createBranch(latch_block, stmt->loc);
            }

            // Latch: pop $#, read back modified var, record, increment, branch to header
            builder.setBlock(latch_block);
            builder.createPopImplicitElement(old_element, stmt->loc);
            QoreIRValue modified = lowerExpression(var_expr, error);
            if (!modified.isValid()) {
                exception_stack.pop_back();
                exception_scope_depth_stack.pop_back();
                guard_exception_target_override = prev_guard_override;
                scope_stack.pop_back();
                cleanup_stack.pop_back();  // Scope
                cleanup_stack.pop_back();  // RefForeach
                return false;
            }
            builder.createRefForeachRecord(state, modified, stmt->loc);
            QoreIRValue one = builder.createConstInt(1, stmt->loc)->result;
            QoreIRValue next_index = builder.createBinaryOp(QoreIROpcode::AddInt,
                index_val, one, stmt->loc)->result;
            {
                auto* br = builder.createBranch(header_block, stmt->loc);
                setLoopCheckpointExceptionTarget(br, header_block, catch_block);
            }

            // Complete PHI node
            index_phi->incoming.push_back({init_index, preheader_block});
            index_phi->incoming.push_back({next_index, latch_block});
            index_phi->operands.push_back(init_index);
            index_phi->operands.push_back(next_index);

            // Pop exception handler
            exception_stack.pop_back();
            exception_scope_depth_stack.pop_back();
            guard_exception_target_override = prev_guard_override;

            // Pop try scope and RefForeach from cleanup stack
            scope_stack.pop_back();
            cleanup_stack.pop_back();  // Scope
            cleanup_stack.pop_back();  // RefForeach

            // Break handler: pop $#, record, scope exit, finalize with fill remaining
            builder.setBlock(break_handler_block);
            builder.createPopImplicitElement(old_element, stmt->loc);
            QoreIRValue break_modified = lowerExpression(var_expr, error);
            if (!break_modified.isValid()) {
                return false;
            }
            builder.createRefForeachRecord(state, break_modified, stmt->loc);
            builder.createScopeExit(try_scope_id, false);
            QoreIRValue fill_true = builder.createConstInt(1, stmt->loc)->result;
            builder.createRefForeachFinalize(state, fill_true, stmt->loc);
            builder.createBranch(merge_block);

            // Exit normal: scope exit, finalize without fill remaining
            builder.setBlock(exit_normal_block);
            builder.createScopeExit(try_scope_id, false);
            QoreIRValue fill_false = builder.createConstInt(0, stmt->loc)->result;
            builder.createRefForeachFinalize(state, fill_false, stmt->loc);
            builder.createBranch(merge_block);

            // Catch: landing pad, cleanup without write-back, rethrow to outer handler
            builder.setBlock(catch_block);
            builder.createLandingPad(try_scope_depth, try_scope_id, stmt->loc);
            builder.createRefForeachCleanup(state, stmt->loc);
            {
                // Route the synthetic rethrow to the outer exception handler (if any)
                // so try/catch blocks around the ref foreach can catch the exception
                QoreIRBasicBlock* outer_handler = exception_stack.empty()
                    ? nullptr : exception_stack.back();
                auto* rethrow_inst = builder.createRethrow(outer_handler, stmt->loc);
                rethrow_inst->synthetic = true;
            }

            // Move merge block to end (after invoke.cont blocks from body lowering)
            builder.getFunction()->moveBlockToEnd(merge_block);
            builder.setBlock(merge_block);

            if (const LVList* loop_lvars = foreach_stmt->getLVList()) {
                for (int i = static_cast<int>(loop_lvars->size()) - 1; i >= 0; --i) {
                    builder.createUninstantiateLocal(loop_lvars->lv[i], stmt->loc);
                }
            }
            return true;
        }

        {
            QoreValue list_expr = foreach_stmt->getList();
            std::vector<LazyPipelineStage> source_stages;
            QoreValue base_source;
            if (!collectLazyPipelineStages(list_expr, base_source, source_stages, error)) {
                return false;
            }
            if (!source_stages.empty()) {
                return lowerForeachLazyPipelineFused(foreach_stmt, base_source, source_stages, error);
            }
        }

        // Evaluate the list expression BEFORE creating loop blocks, so that any
        // blocks created during expression evaluation (e.g., invoke.cont blocks
        // from guarded calls in try-catch) appear before the loop header in the
        // block list. This ensures IteratorCreate's block is processed before
        // IteratorNext's block during LLVM lowering.
        QoreValue list_expr = foreach_stmt->getList();
        QoreIRValue list_val;
        if (list_expr && !list_expr.isNothing()) {
            list_val = lowerExpression(list_expr, error);
            if (!list_val.isValid()) {
                return false;
            }
        } else {
            // Empty list - just create a nothing constant
            list_val = builder.createConstNothing(stmt->loc)->result;
        }

        // Create the iterator.  Pass nullptr for iterator_func to use the generic
        // runtime path.  The parse-tree iterator_func pointer is a compile-time artifact
        // that doesn't exist at runtime in AOT-compiled binaries.
        auto* iter_inst = builder.createIteratorCreate(list_val, nullptr, stmt->loc);
        QoreIRValue iter_val = iter_inst->result;

        // Create basic blocks for the loop structure AFTER evaluating the list
        // expression and creating the iterator
        QoreIRBasicBlock* preheader_block = createBlock("foreach.preheader");
        QoreIRBasicBlock* header_block = createBlock("foreach.header");
        QoreIRBasicBlock* body_block = createBlock("foreach.body");
        QoreIRBasicBlock* latch_block = createBlock("foreach.latch");
        QoreIRBasicBlock* exit_block = createBlock("foreach.exit");
        if (!preheader_block || !header_block || !body_block || !latch_block || !exit_block) {
            error = "IR builder failed to create blocks for foreach";
            return false;
        }
        // Mark header block as loop header for OSR detection
        header_block->is_loop_header = true;

        // Branch to preheader
        builder.createBranch(preheader_block, stmt->loc);

        // Preheader: initialize index counter and branch to header
        builder.setBlock(preheader_block);
        QoreIRValue init_index = builder.createConstInt(0, stmt->loc)->result;
        {
            auto* br = builder.createBranch(header_block, stmt->loc);
            setLoopCheckpointExceptionTarget(br, header_block);
        }

        // Header block: PHI for index, check for next value and branch
        builder.setBlock(header_block);

        // Create PHI for iteration index ($#) - will be completed after body
        auto* index_phi = builder.createPhi({}, stmt->loc, QoreIRPhiValueKind::NativeInt);
        QoreIRValue index_val = index_phi->result;

        auto* next_inst = builder.createIteratorNext(iter_val, exit_block, body_block, stmt->loc);
        QoreIRValue value_val = next_inst->result;

        // Body block: set $# via PushImplicitElement, assign value to loop variable, execute body
        builder.setBlock(body_block);

        // Push the implicit element ($#) for the current iteration
        QoreIRValue old_element = builder.createPushImplicitElement(index_val, stmt->loc)->result;

        // Assign the value to the loop variable
        QoreValue var_expr = foreach_stmt->getVar();
        if (var_expr && !var_expr.isNothing()) {
            // Simple variable targets can use normal assignment lowering; complex
            // lvalues and reference-typed vars need StoreLValue write-through semantics.
            const auto* var_node = dynamic_cast<const VarRefNode*>(var_expr.getInternalNode());
            const QoreTypeInfo* var_type = var_node ? getVarRefTypeInfo(var_node) : nullptr;
            if (var_node && var_node->getType() != VT_IMMEDIATE
                    && !QoreTypeInfo::isReference(var_type)) {
                if (!storeVarRef(var_node, value_val, error, "foreach assignment")) {
                    return false;
                }
            } else {
                auto* store_inst = builder.createStoreLValue(var_expr, value_val, stmt->loc);
                if (!exception_stack.empty()) {
                    store_inst->exception_target = exception_stack.back();
                }
            }
        }

        // Register a cleanup entry so a non-local exit (return / thread_exit) from
        // inside the body restores $# to the caller's value.  Break and continue
        // are handled on their own paths (break pops via ft.old_implicit_element
        // below; continue falls through to the latch which pops), so this entry is
        // pushed *before* ft.cleanup_stack_depth is captured: emitBlockCleanups()
        // for break/continue stops above it and will not double-pop, while a return
        // walks the whole cleanup stack and fires it.  Matches the reference foreach,
        // which pops $# from its RefForeachRecord cleanup entry.
        BlockCleanupEntry elem_entry;
        elem_entry.type = BlockCleanupEntry::ForeachElement;
        elem_entry.old_implicit_element = old_element;
        elem_entry.loc = stmt->loc;
        cleanup_stack.push_back(elem_entry);

        // Lower the loop body with proper break/continue targets
        // break → exit_block (break handler pops $# via old_implicit_element)
        // continue → latch_block (pops $# and increments index)
        FlowTarget ft;
        ft.break_target = exit_block;
        ft.continue_target = latch_block;
        ft.is_switch = false;
        ft.catch_cleanup_depth = catch_cleanup_depth;
        ft.cleanup_stack_depth = cleanup_stack.size();
        ft.old_implicit_element = old_element;
        flow_stack.push_back(ft);
        StatementBlock* body = foreach_stmt->getCode();
        if (body) {
            ++loop_depth;
            if (!lowerStatementBlock(body, error)) {
                --loop_depth;
                flow_stack.pop_back();
                cleanup_stack.pop_back();  // ForeachElement
                return false;
            }
            --loop_depth;
        }
        flow_stack.pop_back();
        cleanup_stack.pop_back();  // ForeachElement

        // Normal body exit falls through to latch block
        if (!blockHasTerminator(builder.getBlock())) {
            builder.createBranch(latch_block, stmt->loc);
        }

        // Latch block: pop implicit element, increment index, branch to header
        builder.setBlock(latch_block);
        builder.createPopImplicitElement(old_element, stmt->loc);

        // Increment index for next iteration
        QoreIRValue one = builder.createConstInt(1, stmt->loc)->result;
        QoreIRValue next_index = builder.createBinaryOp(QoreIROpcode::AddInt, index_val, one,
            stmt->loc)->result;
        {
            auto* br = builder.createBranch(header_block, stmt->loc);
            setLoopCheckpointExceptionTarget(br, header_block);
        }

        // Complete the PHI node with incoming values
        index_phi->incoming.push_back({init_index, preheader_block});
        index_phi->incoming.push_back({next_index, latch_block});
        index_phi->operands.push_back(init_index);
        index_phi->operands.push_back(next_index);

        // Move exit_block to end so LLVM lowering processes it after any
        // invoke.cont blocks created during the foreach body lowering.
        builder.getFunction()->moveBlockToEnd(exit_block);
        // Exit block
        builder.setBlock(exit_block);

        // Foreach loop variables live for the duration of the foreach
        // statement. Clean them up at the loop exit so nested block locals
        // declared before an inner foreach do not pop the inner loop variable
        // out of LIFO order.
        if (const LVList* loop_lvars = foreach_stmt->getLVList()) {
            for (int i = static_cast<int>(loop_lvars->size()) - 1; i >= 0; --i) {
                builder.createUninstantiateLocal(loop_lvars->lv[i], stmt->loc);
            }
        }
        return true;
    }
    if (auto* on_block_exit_stmt = dynamic_cast<const OnBlockExitStatement*>(stmt)) {
        // Phase 1: Inline handler lowering on normal paths
        //
        // Register handler in block_handlers for inline lowering at normal exit points.
        // Handler code is lowered directly into parent IR context via lowerStatementBlock(),
        // which correctly accesses outer-scope variables (same value frame, same slot cache).

        StatementBlock* handler_code = on_block_exit_stmt->getCode();
        if (handler_code) {
            // Phase 2a: Emit OnBlockExit instruction so handlers are registered for exception-path execution
            auto* obe_inst = builder.createOnBlockExit(on_block_exit_stmt, stmt->loc);

            // Register for inline lowering at exit points and handler IR compilation
            block_handlers.emplace_back(InlineHandler{
                on_block_exit_stmt->getType(),
                handler_code,
                stmt->loc,
                obe_inst  // enable compileAllHandlerIRs() to compile this handler
            });
        }

        return true;
    }
    if (auto* debug_stmt = dynamic_cast<const DebugStatement*>(stmt)) {
        // Lower debug statements inline rather than delegating to AST.
        // Expression form (@debug(expr)): lower the expression and discard result.
        // Block form (@debug { ... }): recursively lower the statement block.
        if (StatementBlock* block = debug_stmt->getBlock()) {
            return lowerStatementBlock(block, error);
        }
        QoreValue expr = debug_stmt->getExpression();
        if (expr) {
            QoreIRValue lowered = lowerExpression(expr, error);
            if (!lowered.isValid()) {
                return false;
            }
        }
        return true;
    }
    if (auto* assert_stmt = dynamic_cast<const AssertStatement*>(stmt)) {
        // Fully lower assert: inline the condition check and emit a throw on the
        // failure path.  No AST delegation — the entire assert is compiled to IR.
        QoreValue cond = assert_stmt->getCondition();
        if (!cond) {
            // No condition — unconditional assert failure (rare, but handle it)
            QoreIRValue err = builder.createConstString("ASSERTION-ERROR", stmt->loc)->result;
            QoreIRValue msg = builder.createConstString("assertion failed", stmt->loc)->result;
            QoreIRValue throw_list = builder.createMakeList({err, msg}, stmt->loc)->result;
            QoreIRBasicBlock* handler = exception_stack.empty() ? nullptr : exception_stack.back();
            builder.createThrow(throw_list, handler, stmt->loc);
            return true;
        }
        QoreIRValue cond_val = lowerConditionValue(cond, error);
        if (!cond_val.isValid()) {
            return false;
        }
        QoreIRBasicBlock* fail_block = createBlock("assert.fail");
        QoreIRBasicBlock* merge_block = createBlock("assert.merge");
        if (!fail_block || !merge_block) {
            error = "IR builder failed to create blocks for assert";
            return false;
        }
        builder.createBranchIf(cond_val, merge_block, fail_block);

        // Failure path: construct and throw ASSERTION-ERROR
        builder.setBlock(fail_block);
        QoreIRValue err = builder.createConstString("ASSERTION-ERROR", stmt->loc)->result;
        QoreValue message = assert_stmt->getMessage();
        QoreIRValue msg;
        if (message && !message.isNothing()) {
            if (message.getType() == NT_PARSE_LIST || message.getType() == NT_LIST) {
                // sprintf format case: @assert(cond, "fmt %d", x)
                // May be NT_PARSE_LIST (runtime args) or NT_LIST (constant-folded at parse time)
                QoreIRValue list_val = lowerExpression(message, error);
                if (!list_val.isValid()) {
                    return false;
                }
                msg = builder.createExprOp(QoreIROpcode::Sprintf, message, {list_val},
                    stmt->loc)->result;
            } else {
                // Single message expression — convert to string for the error desc
                QoreIRValue raw_msg = lowerExpression(message, error);
                if (!raw_msg.isValid()) {
                    return false;
                }
                // Wrap in sprintf("%s", val) to ensure string conversion
                QoreIRValue fmt = builder.createConstString("%s", stmt->loc)->result;
                QoreIRValue sprintf_list = builder.createMakeList({fmt, raw_msg}, stmt->loc)->result;
                msg = builder.createExprOp(QoreIROpcode::Sprintf, QoreValue(),
                    {sprintf_list}, stmt->loc)->result;
            }
        } else {
            msg = builder.createConstString("assertion failed", stmt->loc)->result;
        }
        QoreIRValue throw_list = builder.createMakeList({err, msg}, stmt->loc)->result;
        // Emit CatchCleanup for all active catch scopes before throwing
        for (int i = 0; i < catch_cleanup_depth; ++i) {
            builder.createCatchCleanup(stmt->loc);
        }
        QoreIRBasicBlock* handler = exception_stack.empty() ? nullptr : exception_stack.back();
        builder.createThrow(throw_list, handler, stmt->loc);
        if (!blockHasTerminator(builder.getBlock())) {
            builder.createBranch(merge_block);
        }

        builder.setBlock(merge_block);
        return true;
    }
    if (auto* summarize_stmt = dynamic_cast<const SummarizeStatement*>(stmt)) {
        error = "unsupported IR statement lowering: legacy summarize statement has no native lowering";
        if (stmt->loc) {
            error += " at ";
            error += stmt->loc->getFile() ? stmt->loc->getFile() : "<unknown>";
            error += ":";
            error += std::to_string(stmt->loc->start_line);
        }
        error += "; AST statement fallback is disabled";
        return false;
    }
    if (auto* context_stmt = dynamic_cast<const ContextStatement*>(stmt)) {
        // Native IR lowering of `context` — mirrors the non-ref Foreach shape.
        // ContextInit evaluates the context data hash + per-row where/sort
        // filters inside the runtime Context ctor and returns an opaque
        // Context* state handle.  The loop body is inlined here; control
        // flow (break/continue/return) is native IR.
        std::string name = context_stmt->name ? context_stmt->name : "";
        QoreValue sort_exp;
        int sort_type = -1;
        if (context_stmt->sort_ascending) {
            sort_exp = context_stmt->sort_ascending;
            sort_type = CM_SORT_ASCENDING;
        } else if (context_stmt->sort_descending) {
            sort_exp = context_stmt->sort_descending;
            sort_type = CM_SORT_DESCENDING;
        }
        auto* init_inst = builder.createContext(name, context_stmt->exp,
            context_stmt->where_exp, sort_exp, sort_type, stmt->loc);
        if (!exception_stack.empty()) {
            init_inst->exception_target = exception_stack.back();
        }
        QoreIRValue state = init_inst->result;

        StatementBlock* code = context_stmt->code;
        if (!code) {
            // No body: still destroy to pop the thread-local stack frame
            // (Context ctor pushed itself even when there's nothing to run).
            builder.createContextDestroy(state, stmt->loc);
            return true;
        }

        // Get iteration count from the state handle (no exceptions).
        QoreIRValue max_pos = builder.createContextMaxPos(state, stmt->loc)->result;

        // Create loop blocks.
        QoreIRBasicBlock* preheader_block = createBlock("context.preheader");
        QoreIRBasicBlock* header_block = createBlock("context.header");
        QoreIRBasicBlock* body_block = createBlock("context.body");
        QoreIRBasicBlock* latch_block = createBlock("context.latch");
        QoreIRBasicBlock* break_handler_block = createBlock("context.break_handler");
        QoreIRBasicBlock* exit_normal_block = createBlock("context.exit_normal");
        QoreIRBasicBlock* catch_block = createBlock("context.catch");
        QoreIRBasicBlock* merge_block = createBlock("context.merge");
        if (!preheader_block || !header_block || !body_block || !latch_block
                || !break_handler_block || !exit_normal_block || !catch_block
                || !merge_block) {
            error = "IR builder failed to create blocks for context loop";
            return false;
        }
        header_block->is_loop_header = true;

        // Push Context entry to cleanup_stack so return-from-body destroys
        // the frame before unwinding.  Must sit BELOW the try Scope entry
        // so the reverse-walk order on return is: body locals → Scope → Context.
        BlockCleanupEntry context_entry;
        context_entry.type = BlockCleanupEntry::Context;
        context_entry.context_state = state;
        context_entry.loc = stmt->loc;
        cleanup_stack.push_back(context_entry);

        // Set up try scope so exceptions from the body still run ContextDestroy.
        uint32_t try_scope_id = ++scope_counter;
        scope_stack.push_back(try_scope_id);
        {
            BlockCleanupEntry scope_entry;
            scope_entry.type = BlockCleanupEntry::Scope;
            scope_entry.scope_id = try_scope_id;
            scope_entry.handler_start = block_handlers.size();
            cleanup_stack.push_back(scope_entry);
        }
        builder.createScopeEnter(try_scope_id);

        // Install exception handler override for the body.
        QoreIRBasicBlock* prev_guard_override = guard_exception_target_override;
        guard_exception_target_override = catch_block;
        exception_stack.push_back(catch_block);
        size_t try_scope_depth = scope_stack.size() - 1;
        exception_scope_depth_stack.push_back(try_scope_depth);

        // Branch to preheader.
        builder.createBranch(preheader_block, stmt->loc);

        // Preheader: init index counter.
        builder.setBlock(preheader_block);
        QoreIRValue init_index = builder.createConstInt(0, stmt->loc)->result;
        {
            auto* br = builder.createBranch(header_block, stmt->loc);
            setLoopCheckpointExceptionTarget(br, header_block);
        }

        // Header: PHI for index, compare with max_pos.
        builder.setBlock(header_block);
        auto* index_phi = builder.createPhi({}, stmt->loc, QoreIRPhiValueKind::NativeInt);
        QoreIRValue index_val = index_phi->result;
        QoreIRValue cmp = builder.createBinaryOp(QoreIROpcode::LtInt,
            index_val, max_pos, stmt->loc)->result;
        builder.createBranchIf(cmp, body_block, exit_normal_block);

        // Body: set the current row on the Context handle, then execute body.
        builder.setBlock(body_block);
        builder.createContextSetPos(state, index_val, stmt->loc);

        // Install break/continue targets.
        FlowTarget ft;
        ft.break_target = break_handler_block;
        ft.continue_target = latch_block;
        ft.is_switch = false;
        ft.catch_cleanup_depth = catch_cleanup_depth;
        ft.cleanup_stack_depth = cleanup_stack.size();
        flow_stack.push_back(ft);

        ++loop_depth;
        if (!lowerStatementBlock(code, error)) {
            --loop_depth;
            flow_stack.pop_back();
            exception_stack.pop_back();
            exception_scope_depth_stack.pop_back();
            guard_exception_target_override = prev_guard_override;
            scope_stack.pop_back();
            cleanup_stack.pop_back();  // Scope
            cleanup_stack.pop_back();  // Context
            return false;
        }
        --loop_depth;
        flow_stack.pop_back();

        // Normal body exit falls through to latch.
        if (!blockHasTerminator(builder.getBlock())) {
            builder.createBranch(latch_block, stmt->loc);
        }

        // Latch: increment index, branch back to header.
        builder.setBlock(latch_block);
        QoreIRValue one = builder.createConstInt(1, stmt->loc)->result;
        QoreIRValue next_index = builder.createBinaryOp(QoreIROpcode::AddInt,
            index_val, one, stmt->loc)->result;
        {
            auto* br = builder.createBranch(header_block, stmt->loc);
            setLoopCheckpointExceptionTarget(br, header_block);
        }

        // Complete the PHI.
        index_phi->incoming.push_back({init_index, preheader_block});
        index_phi->incoming.push_back({next_index, latch_block});
        index_phi->operands.push_back(init_index);
        index_phi->operands.push_back(next_index);

        // Pop exception handler.
        exception_stack.pop_back();
        exception_scope_depth_stack.pop_back();
        guard_exception_target_override = prev_guard_override;

        // Pop try Scope + Context cleanup entries (rest of the lowering
        // emits them explicitly on each exit block).
        scope_stack.pop_back();
        cleanup_stack.pop_back();  // Scope
        cleanup_stack.pop_back();  // Context

        // Break handler: exit try scope, destroy, merge.
        builder.setBlock(break_handler_block);
        builder.createScopeExit(try_scope_id, false);
        builder.createContextDestroy(state, stmt->loc);
        builder.createBranch(merge_block);

        // Normal exit: exit try scope, destroy, merge.
        builder.setBlock(exit_normal_block);
        builder.createScopeExit(try_scope_id, false);
        builder.createContextDestroy(state, stmt->loc);
        builder.createBranch(merge_block);

        // Catch: landingpad, destroy, rethrow.
        builder.setBlock(catch_block);
        builder.createLandingPad(try_scope_depth, try_scope_id, stmt->loc);
        builder.createContextDestroy(state, stmt->loc);
        {
            QoreIRBasicBlock* outer_handler = exception_stack.empty()
                ? nullptr : exception_stack.back();
            auto* rethrow_inst = builder.createRethrow(outer_handler, stmt->loc);
            rethrow_inst->synthetic = true;
        }

        // Move merge block to end so LLVM lowering processes it after any
        // invoke.cont blocks created during the body lowering.
        builder.getFunction()->moveBlockToEnd(merge_block);
        builder.setBlock(merge_block);
        return true;
    }
    if (auto* thread_exit_stmt = dynamic_cast<const ThreadExitStatement*>(stmt)) {
        builder.createThreadExit(thread_exit_stmt->loc);
        return true;
    }
    if (auto* break_stmt = dynamic_cast<const BreakStatement*>(stmt)) {
        QoreIRBasicBlock* target = nullptr;
        size_t target_cleanup_depth = 0;
        int target_catch_depth = 0;
        QoreIRValue old_elem;
        for (auto it = flow_stack.rbegin(); it != flow_stack.rend(); ++it) {
            if (it->break_target) {
                target = it->break_target;
                target_cleanup_depth = it->cleanup_stack_depth;
                target_catch_depth = it->catch_cleanup_depth;
                old_elem = it->old_implicit_element;
                break;
            }
        }
        if (!target) {
            error = "break statement has no active target for IR lowering";
            return false;
        }
        // Emit interleaved ScopeExit and UninstantiateLocal for blocks entered
        // since the loop started (matches AST mode's destruction ordering)
        if (!emitBlockCleanups(target_cleanup_depth, error, false)) {
            return false;
        }
        // Emit CatchCleanup for catch scopes entered since the loop started
        for (int i = 0; i < catch_cleanup_depth - target_catch_depth; ++i) {
            builder.createCatchCleanup(stmt->loc);
        }
        // Restore $# for foreach loops before breaking out
        if (old_elem.isValid()) {
            builder.createPopImplicitElement(old_elem, stmt->loc);
        }
        {
            auto* br = builder.createBranch(target);
            setLoopCheckpointExceptionTarget(br, target);
        }
        return true;
    }
    if (auto* cont_stmt = dynamic_cast<const ContinueStatement*>(stmt)) {
        QoreIRBasicBlock* target = nullptr;
        size_t target_cleanup_depth = 0;
        int target_catch_depth = 0;
        for (auto it = flow_stack.rbegin(); it != flow_stack.rend(); ++it) {
            if (it->continue_target) {
                target = it->continue_target;
                target_cleanup_depth = it->cleanup_stack_depth;
                target_catch_depth = it->catch_cleanup_depth;
                break;
            }
        }
        if (!target) {
            error = "continue statement has no active target for IR lowering";
            return false;
        }
        // Emit interleaved ScopeExit and UninstantiateLocal for blocks entered
        // since the loop started (matches AST mode's destruction ordering)
        if (!emitBlockCleanups(target_cleanup_depth, error, false)) {
            return false;
        }
        // Emit CatchCleanup for catch scopes entered since the loop started
        for (int i = 0; i < catch_cleanup_depth - target_catch_depth; ++i) {
            builder.createCatchCleanup(stmt->loc);
        }
        // Note: no need to pop $# here for foreach loops — the continue target
        // (latch_block) already calls PopImplicitElement before incrementing the
        // index and branching back to the header.
        {
            auto* br = builder.createBranch(target);
            setLoopCheckpointExceptionTarget(br, target);
        }
        return true;
    }
    if (auto* switch_stmt = dynamic_cast<const SwitchStatement*>(stmt)) {
        QoreValue switch_expr = switch_stmt->getSwitchExp();
        QoreIRValue switch_val = lowerExpression(switch_expr, error);
        if (!switch_val.isValid()) {
            return false;
        }

        QoreIRBasicBlock* match_block = createBlock("switch.match");
        QoreIRBasicBlock* end_block = createBlock("switch.end");
        if (!match_block || !end_block) {
            error = "IR builder failed to create blocks for switch";
            return false;
        }
        if (!blockHasTerminator(builder.getBlock())) {
            builder.createBranch(match_block);
        }

        struct CaseInfo {
            const CaseNode* node = nullptr;
            QoreIRBasicBlock* block = nullptr;
        };
        std::vector<CaseInfo> cases;
        const CaseNode* def_case = nullptr;
        for (const CaseNode* cur = switch_stmt->getCases(); cur; cur = cur->next) {
            QoreIRBasicBlock* case_block = createBlock("switch.case");
            if (!case_block) {
                error = "IR builder failed to create switch case block";
                return false;
            }
            cases.push_back({cur, case_block});
            if (cur->isDefault()) {
                def_case = cur;
            }
        }

        // Check if we can use optimized SwitchInt (all cases are integer constants with equality)
        // Requirements:
        // 1. Switch expression must be guaranteed int type (not float, string, NOTHING, etc.)
        // 2. All non-default cases must be integer constants with soft equality
        bool use_switch_int = guaranteedIntType(&switch_expr);
        std::vector<std::pair<int64_t, QoreIRBasicBlock*>> int_cases;
        QoreIRBasicBlock* default_block_for_switch = nullptr;

        // First pass: check if all non-default cases are integer equality checks
        for (size_t i = 0; i < cases.size() && use_switch_int; ++i) {
            const CaseNode* node = cases[i].node;
            if (node->isDefault()) {
                default_block_for_switch = cases[i].block;
                continue;
            }
            // Check for regex cases - not supported
            if (dynamic_cast<const CaseNodeRegex*>(node)) {
                use_switch_int = false;
                break;
            }
            // Check for range operators - not supported for SwitchInt
            if (auto* op_case = dynamic_cast<const CaseNodeWithOperator*>(node)) {
                op_log_func_t op_func = op_case->getOpFunc();
                // Only soft equality is compatible with SwitchInt
                if (op_func != QoreLogicalEqualsOperatorNode::softEqual) {
                    use_switch_int = false;
                    break;
                }
            }
            // Check if case value is an integer constant
            if (node->val.getType() != NT_INT) {
                use_switch_int = false;
                break;
            }
            int_cases.push_back({node->val.getAsBigInt(), cases[i].block});
        }

        // Use SwitchInt if all conditions are met
        if (use_switch_int && !int_cases.empty()) {
            builder.setBlock(match_block);

            // If no default case, use end_block
            if (!default_block_for_switch) {
                default_block_for_switch = end_block;
            }

            // Convert to QoreIRSwitchCase format
            std::vector<QoreIRSwitchCase> switch_cases;
            for (const auto& ic : int_cases) {
                switch_cases.push_back({ic.first, ic.second});
            }

            builder.createSwitchInt(switch_val, default_block_for_switch, switch_cases);

            // Lower case bodies (same as before)
            flow_stack.push_back({end_block, nullptr, true, catch_cleanup_depth, cleanup_stack.size(), QoreIRValue()});
            for (size_t i = 0; i < cases.size(); ++i) {
                builder.setBlock(cases[i].block);
                if (cases[i].node->code) {
                    if (!lowerStatementBlock(cases[i].node->code, error)) {
                        flow_stack.pop_back();
                        return false;
                    }
                }
                if (!blockHasTerminator(builder.getBlock())) {
                    if (i + 1 < cases.size()) {
                        builder.createBranch(cases[i + 1].block);
                    } else {
                        builder.createBranch(end_block);
                    }
                }
            }
            flow_stack.pop_back();
            builder.setBlock(end_block);
            return true;
        }

        // Check if we can use optimized SwitchString (all cases are string constants with equality)
        // Requirements:
        // 1. Switch expression must be guaranteed string type
        // 2. All non-default cases must be string constants with soft equality
        bool use_switch_string = guaranteedStringType(&switch_expr);
        std::vector<std::pair<std::string, QoreIRBasicBlock*>> string_cases;
        QoreIRBasicBlock* default_block_for_str_switch = nullptr;

        // First pass: check if all non-default cases are string equality checks
        for (size_t i = 0; i < cases.size() && use_switch_string; ++i) {
            const CaseNode* node = cases[i].node;
            if (node->isDefault()) {
                default_block_for_str_switch = cases[i].block;
                continue;
            }
            // Check for regex cases - not supported
            if (dynamic_cast<const CaseNodeRegex*>(node)) {
                use_switch_string = false;
                break;
            }
            // Check for range operators - not supported for SwitchString
            if (auto* op_case = dynamic_cast<const CaseNodeWithOperator*>(node)) {
                op_log_func_t op_func = op_case->getOpFunc();
                // Only soft equality is compatible with SwitchString
                if (op_func != QoreLogicalEqualsOperatorNode::softEqual) {
                    use_switch_string = false;
                    break;
                }
            }
            // Check if case value is a string constant
            if (node->val.getType() != NT_STRING) {
                use_switch_string = false;
                break;
            }
            QoreStringValueHelper str(node->val);
            string_cases.push_back({str->c_str(), cases[i].block});
        }

        // Use SwitchString if all conditions are met
        if (use_switch_string && !string_cases.empty()) {
            builder.setBlock(match_block);

            // If no default case, use end_block
            if (!default_block_for_str_switch) {
                default_block_for_str_switch = end_block;
            }

            // Convert to QoreIRSwitchStringCase format
            std::vector<QoreIRSwitchStringCase> switch_cases;
            for (const auto& sc : string_cases) {
                switch_cases.push_back({sc.first, sc.second});
            }

            builder.createSwitchString(switch_val, default_block_for_str_switch, switch_cases);

            // Lower case bodies (same as before)
            flow_stack.push_back({end_block, nullptr, true, catch_cleanup_depth, cleanup_stack.size(), QoreIRValue()});
            for (size_t i = 0; i < cases.size(); ++i) {
                builder.setBlock(cases[i].block);
                if (cases[i].node->code) {
                    if (!lowerStatementBlock(cases[i].node->code, error)) {
                        flow_stack.pop_back();
                        return false;
                    }
                }
                if (!blockHasTerminator(builder.getBlock())) {
                    if (i + 1 < cases.size()) {
                        builder.createBranch(cases[i + 1].block);
                    } else {
                        builder.createBranch(end_block);
                    }
                }
            }
            flow_stack.pop_back();
            builder.setBlock(end_block);
            return true;
        }

        // Fall through to comparison chain approach
        builder.setBlock(match_block);
        QoreIRBasicBlock* check_block = match_block;
        QoreIRBasicBlock* default_block = nullptr;
        for (size_t i = 0; i < cases.size(); ++i) {
            const CaseNode* node = cases[i].node;
            if (node->isDefault()) {
                default_block = cases[i].block;
                continue;
            }

            builder.setBlock(check_block);
            QoreIRBasicBlock* next_check = createBlock("switch.check");
            if (!next_check) {
                error = "IR builder failed to create switch check block";
                return false;
            }

            QoreIRValue match_value;
            if (auto* regex_case = dynamic_cast<const CaseNodeRegex*>(node)) {
                // Use SwitchRegexMatch instruction which directly calls CaseNodeRegex::matches()
                // without creating temporary AST nodes that would corrupt the switch statement
                auto* inst = builder.createSwitchRegexMatch(regex_case, switch_val, node->loc);
                if (!exception_stack.empty()) {
                    inst->exception_target = exception_stack.back();
                }
                match_value = inst->result;
            } else if (auto* op_case = dynamic_cast<const CaseNodeWithOperator*>(node)) {
                QoreIRValue case_val = lowerExpression(node->val, error);
                if (!case_val.isValid()) {
                    return false;
                }
                op_log_func_t op_func = op_case->getOpFunc();
                QoreIROpcode cmp_op = QoreIROpcode::EqAny;
                QoreValue cmp_expr;
                if (op_func == QoreLogicalGreaterThanOrEqualsOperatorNode::doGreaterThanOrEquals) {
                    cmp_op = selectComparisonOpcode(switch_expr, node->val, QoreIROpcode::GeInt,
                        QoreIROpcode::GeFloat, QoreIROpcode::GeAny);
                    cmp_expr = QoreValue(new QoreLogicalGreaterThanOrEqualsOperatorNode(node->loc,
                        switch_expr.refSelf(), node->val.refSelf()));
                } else if (op_func == QoreLogicalLessThanOrEqualsOperatorNode::doLessThanOrEquals) {
                    cmp_op = selectComparisonOpcode(switch_expr, node->val, QoreIROpcode::LeInt,
                        QoreIROpcode::LeFloat, QoreIROpcode::LeAny);
                    cmp_expr = QoreValue(new QoreLogicalLessThanOrEqualsOperatorNode(node->loc,
                        switch_expr.refSelf(), node->val.refSelf()));
                } else if (op_func == QoreLogicalLessThanOperatorNode::doLessThan) {
                    cmp_op = selectComparisonOpcode(switch_expr, node->val, QoreIROpcode::LtInt,
                        QoreIROpcode::LtFloat, QoreIROpcode::LtAny);
                    cmp_expr = QoreValue(new QoreLogicalLessThanOperatorNode(node->loc, switch_expr.refSelf(),
                        node->val.refSelf()));
                } else if (op_func == QoreLogicalGreaterThanOperatorNode::doGreaterThan) {
                    cmp_op = selectComparisonOpcode(switch_expr, node->val, QoreIROpcode::GtInt,
                        QoreIROpcode::GtFloat, QoreIROpcode::GtAny);
                    cmp_expr = QoreValue(new QoreLogicalGreaterThanOperatorNode(node->loc, switch_expr.refSelf(),
                        node->val.refSelf()));
                } else if (op_func == QoreLogicalEqualsOperatorNode::softEqual) {
                    cmp_op = selectComparisonOpcode(switch_expr, node->val, QoreIROpcode::EqInt,
                        QoreIROpcode::EqAny, QoreIROpcode::EqAny);
                    cmp_expr = QoreValue(new QoreLogicalEqualsOperatorNode(node->loc, switch_expr.refSelf(),
                        node->val.refSelf()));
                } else {
                    error = "unsupported switch case operator for IR lowering";
                    return false;
                }
                ValueHolder cmp_holder(cmp_expr, nullptr);
                match_value = lowerBinaryOpOrInvoke(cmp_op, cmp_expr, switch_val, case_val, node->loc, error);
                if (!match_value.isValid()) {
                    return false;
                }
            } else {
                // Use SwitchCaseMatch which calls CaseNode::matches(),
                // matching the AST switch statement behavior.
                auto* inst = builder.createSwitchCaseMatch(node, switch_val, node->loc);
                if (!exception_stack.empty()) {
                    inst->exception_target = exception_stack.back();
                }
                match_value = inst->result;
            }

            builder.createBranchIf(match_value, cases[i].block, next_check);
            check_block = next_check;
        }

        builder.setBlock(check_block);
        if (!default_block && def_case) {
            for (const auto& info : cases) {
                if (info.node == def_case) {
                    default_block = info.block;
                    break;
                }
            }
        }
        if (default_block) {
            builder.createBranch(default_block);
        } else {
            builder.createBranch(end_block);
        }

        flow_stack.push_back({end_block, nullptr, true, catch_cleanup_depth, cleanup_stack.size(), QoreIRValue()});
        for (size_t i = 0; i < cases.size(); ++i) {
            builder.setBlock(cases[i].block);
            if (cases[i].node->code) {
                if (!lowerStatementBlock(cases[i].node->code, error)) {
                    flow_stack.pop_back();
                    return false;
                }
            }
            if (!blockHasTerminator(builder.getBlock())) {
                if (i + 1 < cases.size()) {
                    builder.createBranch(cases[i + 1].block);
                } else {
                    builder.createBranch(end_block);
                }
            }
        }
        flow_stack.pop_back();

        builder.setBlock(end_block);
        return true;
    }
    if (auto* try_stmt = dynamic_cast<const TryStatement*>(stmt)) {
        QoreIRBasicBlock* try_block = createBlock("try.body");
        QoreIRBasicBlock* catch_block = createBlock("try.catch");
        QoreIRBasicBlock* merge_block = createBlock("try.merge");
        if (!try_block || !catch_block || !merge_block) {
            error = "IR builder failed to create blocks for try";
            return false;
        }
        if (!blockHasTerminator(builder.getBlock())) {
            builder.createBranch(try_block);
        }

        builder.setBlock(try_block);
        QoreIRBasicBlock* prev_guard_override = guard_exception_target_override;
        guard_exception_target_override = catch_block;
        exception_stack.push_back(catch_block);
        size_t try_scope_depth = scope_stack.size();
        exception_scope_depth_stack.push_back(try_scope_depth);

        // Allocate a try-level scope ID and emit ScopeEnter to save the OBE count
        // at try entry. This is used by the LandingPad to execute on_error/on_exit
        // handlers for scopes entered within the try body when an invoke exception
        // jumps directly to the catch block (bypassing normal ScopeExit).
        uint32_t try_scope_id = ++scope_counter;
        scope_stack.push_back(try_scope_id);
        {
            BlockCleanupEntry scope_entry;
            scope_entry.type = BlockCleanupEntry::Scope;
            scope_entry.scope_id = try_scope_id;
            // The try scope itself never registers on_exit/on_success/on_error
            // handlers — any such handlers inside the try body belong to inner
            // StatementBlocks and are cleaned up by their own Scope entries.
            // Anchor handler_start to the current block_handlers size so that
            // emitBlockCleanups() on break/continue/return unwinding past this
            // try scope does NOT re-inline handlers belonging to ENCLOSING
            // blocks (which would fire them twice).
            scope_entry.handler_start = block_handlers.size();
            cleanup_stack.push_back(scope_entry);
        }
        builder.createScopeEnter(try_scope_id);

        if (try_stmt->getTryBlock() && !lowerStatementBlock(try_stmt->getTryBlock(), error)) {
            scope_stack.pop_back();
            cleanup_stack.pop_back();
            exception_stack.pop_back();
            exception_scope_depth_stack.pop_back();
            guard_exception_target_override = prev_guard_override;
            return false;
        }
        exception_stack.pop_back();
        exception_scope_depth_stack.pop_back();
        guard_exception_target_override = prev_guard_override;
        if (!blockHasTerminator(builder.getBlock())) {
            // On normal path, emit ScopeExit for the try-level scope (no-op since
            // inner scopes already cleaned up their handlers)
            builder.createScopeExit(try_scope_id, false);
            scope_stack.pop_back();
            cleanup_stack.pop_back();
            builder.createBranch(merge_block);
        } else {
            scope_stack.pop_back();
            cleanup_stack.pop_back();
        }

        builder.setBlock(catch_block);
        // Pass the scope_stack depth and try_scope_id so the LandingPad can
        // execute on_error/on_exit handlers for scopes entered within the try body
        builder.createLandingPad(try_scope_depth, try_scope_id, stmt->loc);
        QoreIRInstruction* catch_inst = builder.createCatchException(stmt->loc);
        LocalVar* catch_var = try_stmt->getCatchVar();
        if (catch_var) {
            maybeInsertNotNothingGuard(catch_inst->result, nullptr, catch_inst->loc, catch_var->getTypeInfo());
            builder.createStoreLocal(catch_var, catch_inst->result, stmt->loc);
            if (parse_context) {
                parse_context->markLocalAssignment(catch_var, true, catch_var->getTypeInfo());
            }
            // Push a CatchVar cleanup entry so non-local exits (break, continue,
            // return, throw) within the catch block uninstantiate the catch_var
            // before unwinding.  The thread local-var stack is strictly LIFO —
            // if we leave the catch_var on the stack, the enclosing block's
            // UninstantiateLocal for its own lvars will pop the catch_var
            // instead, leading to thread_find_lvar asserts when the catch
            // fires again (identity mismatch between expected and actual top).
            BlockCleanupEntry catch_var_entry;
            catch_var_entry.type = BlockCleanupEntry::CatchVar;
            catch_var_entry.catch_var = catch_var;
            catch_var_entry.loc = stmt->loc;
            cleanup_stack.push_back(catch_var_entry);
        }
        ++catch_cleanup_depth;
        if (try_stmt->getCatchBlock() && !lowerStatementBlock(try_stmt->getCatchBlock(), error)) {
            --catch_cleanup_depth;
            if (catch_var) {
                cleanup_stack.pop_back();
            }
            return false;
        }
        --catch_cleanup_depth;
        if (catch_var) {
            cleanup_stack.pop_back();
        }
        if (!blockHasTerminator(builder.getBlock())) {
            // Fallthrough path: uninstantiate catch_var before CatchCleanup so
            // the stack pop order matches the push order (LIFO invariant).
            if (catch_var) {
                auto* ui = builder.createUninstantiateLocal(catch_var, stmt->loc);
                ui->is_block_exit = true;
            }
            // Clean up catch scope before merging: restore previous td->catchException
            // and delete the caught exception.  This only runs on the catch path's
            // fallthrough to merge.  Rethrow handles its own cleanup.
            // Return/throw/break/continue from catch emit their own CatchCleanup.
            builder.createCatchCleanup(stmt->loc);
            builder.createBranch(merge_block);
        }

        // Move merge_block to the end of the block list so it is processed
        // AFTER all invoke.cont blocks created during the try body lowering.
        // LLVM lowering processes blocks in list order — emitInvokeCleanup in
        // ReturnNothing must see all trackResultForCleanup allocas from the body.
        builder.getFunction()->moveBlockToEnd(merge_block);
        builder.setBlock(merge_block);
        return true;
    }
    if (auto* throw_stmt = dynamic_cast<const ThrowStatement*>(stmt)) {
        QoreIRValue value = lowerExpression(throw_stmt->getArgs(), error);
        if (!value.isValid()) {
            return false;
        }
        // NOTE: do NOT emit ScopeExits before Throw. The exception must be on xsink
        // before on_error handlers fire (for CatchExceptionHelper/rethrow support).
        // For exception-target case: the LandingPad fires scope exits after the invoke.
        // For no-exception-target case: the Throw handler fires scope exits after raising.
        // Emit CatchCleanup for all active catch scopes before throwing
        for (int i = 0; i < catch_cleanup_depth; ++i) {
            builder.createCatchCleanup(stmt->loc);
        }
        QoreIRBasicBlock* handler = exception_stack.empty() ? nullptr : exception_stack.back();
        builder.createThrow(value, handler, stmt->loc);
        return true;
    }
    if (auto* rethrow_stmt = dynamic_cast<const RethrowStatement*>(stmt)) {
        QoreValue args = rethrow_stmt->getArgs();
        if (args && !args.isNothing()) {
            QoreIRValue value = lowerExpression(args, error);
            if (!value.isValid()) {
                return false;
            }
            // NOTE: do NOT emit ScopeExits or CatchCleanup before Rethrow —
            // rethrow needs td->catchException intact to get the exception to
            // modify via replaceTop().  Store catch_cleanup_depth in the
            // instruction so the rethrow handler can clean up all catch scopes
            // AFTER rethrowing.
            QoreIRBasicBlock* handler = exception_stack.empty() ? nullptr : exception_stack.back();
            auto* rethrow_inst = builder.createRethrow(handler, stmt->loc);
            rethrow_inst->catch_depth = catch_cleanup_depth;
            // Attach args to the Rethrow instruction so the interpreter/JIT can
            // call catch_get_exception()->replaceTop() with the new err/desc/arg.
            rethrow_inst->operands.push_back(value);
        } else {
            // NOTE: do NOT emit ScopeExits before Rethrow — same reason as Throw above.
            // Don't emit CatchCleanup before rethrow — rethrow needs td->catchException
            // intact to get the exception to rethrow.  Instead, store catch_cleanup_depth
            // in the instruction so the rethrow handler can clean up all catch scopes
            // AFTER rethrowing.
            QoreIRBasicBlock* handler = exception_stack.empty() ? nullptr : exception_stack.back();
            auto* rethrow_inst = builder.createRethrow(handler, stmt->loc);
            rethrow_inst->catch_depth = catch_cleanup_depth;
        }
        return true;
    }

    error = "unsupported statement for IR lowering";
    return false;
}

bool QoreIRLowering::lowerStatementBlock(const StatementBlock* block, std::string& error) {
    if (!block) {
        error = "null statement block for IR lowering";
        return false;
    }
    if (!ensureBuilderContext(error)) {
        return false;
    }

    const bool root_statement_block = statement_block_depth++ == 0;
    struct StatementBlockDepthGuard {
        int& depth;
        ~StatementBlockDepthGuard() {
            --depth;
        }
    } statement_block_depth_guard{statement_block_depth};

    // Push handler stack to track which handlers are registered in this block
    size_t block_handler_start = block_handlers.size();
    handler_stack.push(block_handler_start);

    // Top-level locals are owned by QoreProgram and instantiated for each
    // thread before the top-level block runs.  Treating the root top-level
    // block as an ordinary lexical scope pops program-scope CVVs too early.
    const bool root_top_level = dynamic_cast<const TopLevelStatementBlock*>(block) != nullptr;

    // Get the block's local variables for cleanup.
    const LVList* lvars = root_top_level ? nullptr : block->getLVList();

    // Push lvars to cleanup_stack BEFORE scope entry.
    // This ensures correct reverse ordering on cleanup: scope exit fires before
    // lvars cleanup, matching AST mode where on_exit handlers run inside execIntern()
    // before LVListInstantiator destructor fires in execImpl().
    if (lvars) {
        BlockCleanupEntry lvars_entry;
        lvars_entry.type = BlockCleanupEntry::Lvars;
        lvars_entry.lvars = lvars;
        lvars_entry.loc = block->loc;
        cleanup_stack.push_back(lvars_entry);

        // Emit InstantiateLocal for each closure-use variable at block entry.
        // This ensures the variable is on the cvstack before any code in the
        // block (including CreateClosure) accesses it. Pairs with
        // UninstantiateLocal emitted at block exit.
        for (unsigned i = 0; i < lvars->size(); ++i) {
            if (lvars->lv[i]->closureUse()) {
                builder.createInstantiateLocal(lvars->lv[i], block->loc);
            }
        }
    }

    // Check if this block has on_exit/on_success/on_error handlers
    bool has_on_block_exit = block->hasOnBlockExit();
    uint32_t scope_id = 0;

    if (has_on_block_exit) {
        // Allocate a unique scope ID (for compatibility with Phase 2/3)
        scope_id = ++scope_counter;
        scope_stack.push_back(scope_id);
        {
            BlockCleanupEntry scope_entry;
            scope_entry.type = BlockCleanupEntry::Scope;
            scope_entry.scope_id = scope_id;
            scope_entry.handler_start = block_handler_start;  // Record handler start for inline lowering
            cleanup_stack.push_back(scope_entry);
        }
        // Phase 2a: Emit ScopeEnter so scope_stack has watermark for exception-path handler execution
        builder.createScopeEnter(scope_id);
    }

    QoreIRBasicBlock* lvar_exception_cleanup_block = nullptr;
    QoreIRBasicBlock* lvar_exception_cleanup_continue_block = nullptr;
    QoreIRBasicBlock* saved_exception_target = nullptr;
    bool pushed_lvar_exception_target = false;
    if (lvars && getCurrentExceptionTarget()) {
        // Exceptions raised inside this lexical block must destroy its block-scoped
        // locals before control reaches the enclosing catch.  Normal fall-through
        // cleanup below only handles non-exception exits.
        saved_exception_target = getCurrentExceptionTarget();
        lvar_exception_cleanup_block = createBlock("block.lvars.exception.cleanup");
        if (!lvar_exception_cleanup_block) {
            error = "IR builder failed to create block local exception cleanup block";
            if (has_on_block_exit) {
                scope_stack.pop_back();
                cleanup_stack.pop_back();
            }
            cleanup_stack.pop_back();
            return false;
        }
        if (has_on_block_exit) {
            lvar_exception_cleanup_continue_block = createBlock("block.lvars.exception.cleanup.cont");
            if (!lvar_exception_cleanup_continue_block) {
                error = "IR builder failed to create block local exception cleanup continuation block";
                scope_stack.pop_back();
                cleanup_stack.pop_back();
                cleanup_stack.pop_back();
                return false;
            }
        }
        exception_stack.push_back(lvar_exception_cleanup_block);
        pushed_lvar_exception_target = true;
    }

    if (!root_statement_block) {
        builder.createDebugBlock(block->loc);
    }

    bool terminated = false;
    for (auto it = block->getStatements().begin(); it != block->getStatements().end(); ++it) {
        if (!*it) {
            continue;
        }
        // Bracket each statement with PushTempMark / DiscardTemps so expression
        // temps created during the statement destruct at end of statement —
        // matching AST-mode ValueEvalRefHolder destructor timing (fixes
        // pipe.qtest pattern: `InputStream is = new StreamPipe().getInputStream();`
        // where the temp StreamPipe must destruct at statement end so its
        // internal PipeOutputStream dtor sets pipe->broken).  The marker lets
        // DiscardTemps stop at the statement's cleanup boundary rather than
        // draining the entire cleanup vector, preserving OUTER-scope temps —
        // critical for e.g. a `foreach` list expression's iterator temp, which
        // lives in the cleanup vector across every body-statement boundary.
        bool statement_needs_cleanup = statementMayCreateNodeTemp(*it);
        if (statement_needs_cleanup) {
            builder.createPushTempMark((*it)->loc);
        }
        if (!lowerStatement(*it, error)) {
            if (pushed_lvar_exception_target) {
                exception_stack.pop_back();
            }
            if (has_on_block_exit) {
                scope_stack.pop_back();
                cleanup_stack.pop_back();
            }
            if (lvars) {
                cleanup_stack.pop_back();
            }
            return false;
        }
        if (blockHasTerminator(builder.getBlock())) {
            // Terminator (Return/Throw/Branch) handles full cleanup including
            // any markers; skip DiscardTemps emission.
            terminated = true;
            break;
        }
        if (statement_needs_cleanup) {
            builder.createDiscardTemps((*it)->loc);
        }
    }

    if (pushed_lvar_exception_target) {
        exception_stack.pop_back();
    }

    // Pop handler stack
    handler_stack.pop();

    // At fall-through (normal exit), inline handlers
    if (has_on_block_exit) {
        if (!terminated) {
            // Check if the block has BOTH on_exit and on_error handlers.
            // When on_exit handler code throws, the LandingPad re-fires handlers,
            // causing on_exit to execute twice. Don't inline when mixed — let the
            // runtime ScopeExit handler manage re-entrancy properly.
            bool has_unconditional = false;
            bool has_error = false;
            for (size_t hi = block_handler_start; hi < block_handlers.size(); ++hi) {
                if (block_handlers[hi].type == OBE_Unconditional) has_unconditional = true;
                if (block_handlers[hi].type == OBE_Error) has_error = true;
            }
            bool skip_inline = has_unconditional && has_error;

            if (!skip_inline) {
                // Clear the runtime OBE registration before executing inline
                // handler code.  Handler bodies may contain return/break/etc.;
                // if such a non-local exit happens before ScopeExit runs, the
                // interpreter's return cleanup would see the handler still
                // registered and fire it a second time.
                builder.createScopeExit(scope_id, false, nullptr, /*inline_lowered=*/true);

                // Phase 1: Inline handlers at fall-through exit.  The
                // Scope entry that owns these handlers is at
                // cleanup_stack.size()-1 (it gets popped below); pass
                // barrier_depth = cleanup_stack.size() so a non-local
                // exit inside a handler body clamps target_depth past
                // the firing Scope entry — without the barrier,
                // `return`/`break`/`continue` inside an on_exit body
                // triggers infinite re-inlining.
                if (!lowerHandlersAtExit(false, error, block_handler_start,
                        /*end_index=*/SIZE_MAX,
                        /*barrier_depth=*/cleanup_stack.size())) {
                    return false;
                }
            } else {
                // Phase 2a: Emit ScopeExit to execute runtime handlers.
                auto* se_inst = builder.createScopeExit(scope_id, false, nullptr, /*inline_lowered=*/false);
                // When handlers execute at runtime, they may throw.  Set
                // exception_target so the interpreter routes to the try/catch
                // landing pad.
                if (!exception_stack.empty()) {
                    se_inst->exception_target = exception_stack.back();
                }
            }
        }
        // Always pop scope/cleanup stacks - they're tracking compile-time state, not IR instructions
        // When terminated=true, emitBlockCleanups already emitted the ScopeExit IR, but we still need to pop
        scope_stack.pop_back();
        cleanup_stack.pop_back();
    }

    // Compile handlers registered in THIS block level IMMEDIATELY as we exit
    // This avoids accumulating handlers from all nesting levels, which creates pathological CFGs for LLVM
    // Each block's handlers are compiled independently before the parent block resumes
    if (!block_handlers.empty() && block_handler_start < block_handlers.size()) {
        // Extract handlers registered in this block level
        std::vector<InlineHandler> block_level_handlers(
            block_handlers.begin() + block_handler_start, block_handlers.end());

        // Compile only this block's handlers (not nested ones - they compile themselves)
        // This prevents handler accumulation and keeps CFGs manageable.
        // On failure (-1) propagate as an outer-block lowering failure — the
        // runtime no longer tolerates null handler_ir (see executeHandlerBody
        // assert).  The enclosing caller will then fall back to AST for the
        // whole function / top-level block.
        int compiled_count = compileBlockHandlerIRs(block_level_handlers, builder.getFunction(), error);
        if (compiled_count < 0) {
            if (getenv("QORE_DEBUG_HANDLERS")) {
                fprintf(stderr, "Block-level handler IR compilation failed: %s\n", error.c_str());
            }
            return false;
        }
    }

    // Remove handlers registered in this block (restore to previous size)
    if (block_handler_start < block_handlers.size()) {
        block_handlers.erase(block_handlers.begin() + block_handler_start, block_handlers.end());
    }

    // Emit UninstantiateLocal for block-scoped local variables in reverse order
    // (reverse order ensures destructors are called in LIFO order like in AST mode)
    // When terminated, the break/continue/return handler has already emitted these
    // via emitBlockCleanups
    if (lvars && !terminated) {
        for (int i = static_cast<int>(lvars->size()) - 1; i >= 0; --i) {
            auto* ui = builder.createUninstantiateLocal(lvars->lv[i], block->loc);
            // Set is_block_exit only when NOT inside a loop body.
            // Loop body blocks re-use variables each iteration — closures
            // may still need the captured value. Non-loop blocks are permanent
            // scope exits where CVV values should be cleared unconditionally.
            ui->is_block_exit = (loop_depth == 0);
        }
        auto* check_inst = builder.createCheckException(block->loc);
        check_inst->exception_target = getCurrentExceptionTarget();
    }
    if (lvars) {
        cleanup_stack.pop_back();
    }

    if (lvar_exception_cleanup_block) {
        QoreIRBasicBlock* after_block = builder.getBlock();
        builder.getFunction()->moveBlockToEnd(lvar_exception_cleanup_block);
        builder.setBlock(lvar_exception_cleanup_block);
        if (has_on_block_exit) {
            // Exception unwinds through this synthetic block instead of the
            // try LandingPad, so fire this lexical scope's on_exit/on_error
            // handlers here while its locals are still alive.
            auto* se_inst = builder.createScopeExit(scope_id, true, block->loc, /*inline_lowered=*/false);
            se_inst->exception_target = lvar_exception_cleanup_continue_block;
            builder.createBranch(lvar_exception_cleanup_continue_block, block->loc);
            builder.setBlock(lvar_exception_cleanup_continue_block);
        }
        for (int i = static_cast<int>(lvars->size()) - 1; i >= 0; --i) {
            auto* ui = builder.createUninstantiateLocal(lvars->lv[i], block->loc);
            ui->is_block_exit = true;
        }
        auto* check_inst = builder.createCheckException(block->loc);
        check_inst->exception_target = saved_exception_target;
        builder.createBranch(saved_exception_target, block->loc);
        builder.setBlock(after_block);
    }

    return true;
}

bool QoreIRLowering::emitBlockCleanups(size_t target_depth, std::string& error, bool is_error, unsigned flags) {
    // Emit interleaved cleanup instructions from innermost to outermost block until
    // we reach the target depth.  Handles: ScopeExit (on_exit handlers), Lvars
    // (block-scoped locals), RefForeachRecord (pop $#, load var, record), and
    // RefForeach (finalize/write-back).
    //
    // A HandlerBarrier entry (pushed by lowerHandlersAtExit around a
    // handler body being inlined) marks the Scope entry that fired the
    // handler as protected.  Non-local exits from inside the handler must
    // not revisit that Scope entry, or the same handler range would be
    // inlined recursively.  Other cleanup entries below the protected Scope
    // still have to run, especially Lvars, so object destructors fire before
    // the source-level caller resumes.
    // Without the protected-scope skip, a non-local exit inside the handler body would
    // recursively re-enter `lowerHandlersAtExit` on the same handler
    // range and infinite-recurse (see 8fb555ac1 for the symmetric fix
    // on TryStatement / RefForeach).  The barrier's own `handler_start`
    // records the depth just above the Scope entry to protect.
    size_t protected_scope_index = SIZE_MAX;
    for (size_t j = cleanup_stack.size(); j > target_depth; --j) {
        if (cleanup_stack[j - 1].type == BlockCleanupEntry::HandlerBarrier) {
            const size_t clamp_to = cleanup_stack[j - 1].handler_start;
            if (clamp_to > target_depth && clamp_to != 0) {
                protected_scope_index = clamp_to - 1;
            }
            break;  // only the innermost barrier matters
        }
    }
    for (size_t i = cleanup_stack.size(); i > target_depth; --i) {
        // Copy by value: lowerHandlersAtExit() can trigger cleanup_stack reallocation
        // (when inlining handler bodies that push to cleanup_stack), which would
        // invalidate a reference into the vector.
        const BlockCleanupEntry entry = cleanup_stack[i - 1];
        switch (entry.type) {
            case BlockCleanupEntry::Scope: {
                if ((i - 1) == protected_scope_index) {
                    break;
                }
                // Clear the runtime OBE registration before executing inline
                // handler code.  A handler can contain a non-local exit; if the
                // ScopeExit ran after the inline body, that exit path would
                // leave the handler registered and fire it again during return
                // cleanup.
                builder.createScopeExit(entry.scope_id, is_error, entry.loc, /*inline_lowered=*/true);

                // Phase 1: Inline handlers on break/continue/return cleanup.
                // Each Scope's handlers occupy the range [entry.handler_start,
                // next_inner_scope.handler_start) in block_handlers — handlers
                // pushed AFTER this scope's entry but BEFORE any inner scope
                // belong to this scope.  Scan the cleanup_stack above i-1 for
                // the innermost Scope that is still pending/being processed
                // and use its handler_start as our upper bound, so outer
                // scopes don't re-inline handlers already fired by inner
                // scopes on the same unwind.  Without this bound,
                // lowerHandlersAtExit() would use block_handlers.size() and
                // refire inner-scope handlers — each handler must fire
                // exactly once per unwind (matching AST semantics).  We
                // cannot mutate block_handlers here because compileBlockHandlerIRs
                // (invoked after the enclosing lowerStatementBlock returns)
                // still needs to walk the registered handler vector.
                size_t handler_end = block_handlers.size();
                for (size_t j = i; j < cleanup_stack.size(); ++j) {
                    if (cleanup_stack[j].type == BlockCleanupEntry::Scope) {
                        handler_end = cleanup_stack[j].handler_start;
                        break;
                    }
                }
                // barrier_depth = i means: while the handler body is
                // inlined, any `emitBlockCleanups` invoked from a
                // non-local exit inside that body must clamp its
                // target_depth up to `i` — so the walk excludes
                // cleanup_stack[i-1] (this firing Scope entry) and
                // everything below it.  Remaining outer entries are
                // fired by the current outer emitBlockCleanups when
                // this iteration returns.  This is what prevents the
                // infinite re-inlining cycle.
                if (!lowerHandlersAtExit(is_error, error, entry.handler_start, handler_end,
                        /*barrier_depth=*/i)) {
                    return false;
                }
                break;
            }
            case BlockCleanupEntry::Lvars:
                if (!(flags & CF_SKIP_LVARS)) {
                    assert(entry.lvars);
                    for (int j = static_cast<int>(entry.lvars->size()) - 1; j >= 0; --j) {
                        auto* ui = builder.createUninstantiateLocal(entry.lvars->lv[j], entry.loc);
                        // break/continue/return always exit the scope permanently
                        ui->is_block_exit = true;
                    }
                }
                break;
            case BlockCleanupEntry::CatchVar:
                if (!(flags & CF_SKIP_LVARS)) {
                    assert(entry.catch_var);
                    auto* ui = builder.createUninstantiateLocal(entry.catch_var, entry.loc);
                    ui->is_block_exit = true;
                }
                break;
            case BlockCleanupEntry::ForeachElement:
                // Value (iterator-based) foreach: restore $# to the caller's value
                // on a non-local exit (return / thread_exit) out of the loop body.
                builder.createPopImplicitElement(entry.old_implicit_element, entry.loc);
                break;
            case BlockCleanupEntry::RefForeachRecord: {
                // Pop implicit element ($#) and record modified loop variable value
                builder.createPopImplicitElement(entry.old_implicit_element, entry.loc);
                std::string err;
                QoreIRValue modified = lowerExpression(entry.var_expr, err);
                if (modified.isValid()) {
                    builder.createRefForeachRecord(entry.ref_foreach_state, modified, entry.loc);
                }
                break;
            }
            case BlockCleanupEntry::RefForeach: {
                // Finalize ref foreach: write back to reference (with or without fill remaining)
                QoreIRValue fill = builder.createConstInt(
                    (flags & CF_FILL_REMAINING) ? 1 : 0, entry.loc)->result;
                builder.createRefForeachFinalize(entry.ref_foreach_state, fill, entry.loc);
                break;
            }
            case BlockCleanupEntry::Context: {
                // Destroy the Context frame (pops thread-local stack + frees).
                // Fires on return-through-body for native `context` loops.
                builder.createContextDestroy(entry.context_state, entry.loc);
                break;
            }
            case BlockCleanupEntry::HandlerBarrier:
                // Sentinel — already handled by the pre-walk clamp above;
                // the main loop's target_depth got raised past any barrier
                // so we should never visit one here.  Guard anyway.
                break;
        }
    }
    return true;
}

bool QoreIRLowering::shouldRunHandler(const InlineHandler& handler, bool is_error) {
    // Determine if handler should execute based on exit type and handler type
    switch (handler.type) {
        case OBE_Unconditional:
            // Always executes
            return true;
        case OBE_Success:
            // Only executes on normal (non-error) exits
            return !is_error;
        case OBE_Error:
            // Only executes on error/exception exits
            return is_error;
    }
    return false;
}

//! Helper function to recursively collect local variable references from an expression
/** Walks the AST to find VarRefNode instances and stores their LocalVar references.
 */
static void collectLocalVarsFromExpr(const QoreValue& expr,
        std::unordered_set<LocalVar*>& local_vars) {
    if (!expr.hasNode()) {
        return;
    }
    const AbstractQoreNode* node = expr.getInternalNode();
    if (!node) {
        return;
    }

    qore_type_t ntype = expr.getType();

    // VarRefNode — leaf node, check if it's a local variable
    if (ntype == NT_VARREF) {
        auto* var_ref = reinterpret_cast<const VarRefNode*>(node);
        qore_var_t type = var_ref->getType();
        if ((type == VT_LOCAL || type == VT_LOCAL_TS || type == VT_CLOSURE) && var_ref->ref.id) {
            local_vars.insert(var_ref->ref.id);
        }
        return;
    }

    // Constants and literals — leaf nodes
    if (ntype == NT_STRING || ntype == NT_INT || ntype == NT_FLOAT || ntype == NT_BOOLEAN || ntype == NT_CHAR
            || ntype == NT_NOTHING || ntype == NT_NULL || ntype == NT_NUMBER
            || ntype == NT_DATE || ntype == NT_BINARY || ntype == NT_HASH
            || ntype == NT_LIST || ntype == NT_BACKQUOTE) {
        return;
    }

    // Binary operators: recurse left and right
    if (auto* binop = dynamic_cast<const QoreBinaryOperatorNode<>*>(node)) {
        collectLocalVarsFromExpr(binop->getLeft(), local_vars);
        collectLocalVarsFromExpr(binop->getRight(), local_vars);
        return;
    }

    // Unary/single expression operators: recurse expression
    if (auto* unop = dynamic_cast<const QoreSingleExpressionOperatorNode<>*>(node)) {
        collectLocalVarsFromExpr(unop->getExp(), local_vars);
        return;
    }

    // LValue single expression operators
    if (auto* unop = dynamic_cast<const QoreSingleExpressionOperatorNode<LValueOperatorNode>*>(node)) {
        collectLocalVarsFromExpr(unop->getExp(), local_vars);
        return;
    }

    // Function calls (FunctionCallNode, SelfFunctionCallNode, StaticMethodCallNode, etc.)
    if (auto* call = dynamic_cast<const FunctionCallBase*>(node)) {
        if (const QoreParseListNode* args = call->getParseArgs()) {
            for (size_t i = 0; i < args->size(); ++i) {
                collectLocalVarsFromExpr(args->get(i), local_vars);
            }
        }
        if (const QoreListNode* args = call->getArgs()) {
            ConstListIterator li(args);
            while (li.next()) {
                collectLocalVarsFromExpr(li.getValue(), local_vars);
            }
        }
        return;
    }

    // Subscript operators: recurse into container and index
    if (auto* sub = dynamic_cast<const QoreSquareBracketsOperatorNode*>(node)) {
        collectLocalVarsFromExpr(sub->getLeft(), local_vars);
        collectLocalVarsFromExpr(sub->getRight(), local_vars);
        return;
    }

    // Hash dereference operator: recurse into left side
    if (auto* hd = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(node)) {
        collectLocalVarsFromExpr(hd->getLeft(), local_vars);
        return;
    }
}

//! Helper function to recursively collect local variables from statements
/** Walks the AST statement tree to find all VarRefNode instances in expressions.
 */
static void collectLocalVarsFromStatements(const StatementBlock* block,
        std::unordered_set<LocalVar*>& local_vars) {
    if (!block) {
        return;
    }
    for (const auto* stmt : block->getStatements()) {
        if (!stmt) {
            continue;
        }
        // ExpressionStatement: walk expression
        if (auto* expr_stmt = dynamic_cast<const ExpressionStatement*>(stmt)) {
            collectLocalVarsFromExpr(expr_stmt->getExpression(), local_vars);
            continue;
        }
        // IfStatement: walk condition, then-block, else-block
        if (auto* if_stmt = dynamic_cast<const IfStatement*>(stmt)) {
            collectLocalVarsFromExpr(if_stmt->getCond(), local_vars);
            collectLocalVarsFromStatements(if_stmt->getIfCode(), local_vars);
            collectLocalVarsFromStatements(if_stmt->getElseCode(), local_vars);
            continue;
        }
        // WhileStatement / DoWhileStatement: walk condition, body
        if (auto* while_stmt = dynamic_cast<const WhileStatement*>(stmt)) {
            collectLocalVarsFromExpr(while_stmt->getCond(), local_vars);
            collectLocalVarsFromStatements(while_stmt->getCode(), local_vars);
            continue;
        }
        if (auto* dowhile_stmt = dynamic_cast<const DoWhileStatement*>(stmt)) {
            collectLocalVarsFromExpr(dowhile_stmt->getCond(), local_vars);
            collectLocalVarsFromStatements(dowhile_stmt->getCode(), local_vars);
            continue;
        }
        // ForStatement: walk assignment, condition, iterator, body
        if (auto* for_stmt = dynamic_cast<const ForStatement*>(stmt)) {
            collectLocalVarsFromExpr(for_stmt->getAssignment(), local_vars);
            collectLocalVarsFromExpr(for_stmt->getCond(), local_vars);
            collectLocalVarsFromExpr(for_stmt->getIterator(), local_vars);
            collectLocalVarsFromStatements(for_stmt->getCode(), local_vars);
            continue;
        }
        // TryStatement: walk try-block, catch block
        if (auto* try_stmt = dynamic_cast<const TryStatement*>(stmt)) {
            collectLocalVarsFromStatements(try_stmt->getTryBlock(), local_vars);
            collectLocalVarsFromStatements(try_stmt->getCatchBlock(), local_vars);
            continue;
        }
    }
}

QoreIRLowering::HandlerVariableCapture QoreIRLowering::analyzeHandlerVariables(
        const StatementBlock* handler_code) {
    HandlerVariableCapture capture;
    if (!handler_code) {
        return capture;
    }

    // Collect all local variables referenced in handler code
    std::unordered_set<LocalVar*> all_locals;
    collectLocalVarsFromStatements(handler_code, all_locals);

    // Convert to vector, avoiding duplicates
    for (LocalVar* lv : all_locals) {
        if (capture.referenced_set.find(lv) == capture.referenced_set.end()) {
            capture.referenced_locals.push_back(lv);
            capture.referenced_set.insert(lv);
        }
    }

    return capture;
}

// Each on_block_exit handler body is compiled to its own QoreIRFunction.  The
// JIT resolves a handler's compiled LLVM function by name (module.getFunction),
// so every handler MUST have a unique name — otherwise multiple handlers in one
// function (e.g. on_exit + on_error + on_success) all collide to a single LLVM
// function and run the same body (the IR interpreter is unaffected: it uses the
// handler_ir pointer directly).  A process-global counter assigned at
// construction gives each handler_ir a stable, unique name for its lifetime.
static std::string qore_ir_next_handler_name() {
    static std::atomic<uint64_t> handler_counter{0};
    return "handler_" + std::to_string(handler_counter.fetch_add(1, std::memory_order_relaxed));
}

QoreIRFunction* QoreIRLowering::compileHandlerToIR(
        const StatementBlock* handler_code,
        const HandlerVariableCapture& capture,
        std::string& error) {
    if (!handler_code) {
        error = "handler code is null";
        return nullptr;
    }

    // Phase 3b: Compile handler to IR function with captured variables as parameters
    // Create a new IR function for the handler
    auto handler_func = std::make_unique<QoreIRFunction>(qore_ir_next_handler_name());

    // Populate pre_instantiated_locals with handler body locals
    // so the IR interpreter knows which variables need instantiation
    // (rather than treating all variables as outer-scope)
    std::vector<LocalVar*> handler_locals;

    // Collect locals from handler's block and all nested statements
    if (handler_code->getLVList()) {
        const LVList* lvars = handler_code->getLVList();
        for (unsigned i = 0; i < lvars->size(); ++i) {
            handler_locals.push_back(lvars->lv[i]);
        }
    }

    // Helper lambda to recursively collect locals from statements
    std::function<void(const AbstractStatement*)> collectStmtLocals =
        [&](const AbstractStatement* stmt) {
            if (!stmt) return;

            // Dispatch based on statement type
            // (Only recurse into types that are fully lowered to IR)
            if (auto* block = dynamic_cast<const StatementBlock*>(stmt)) {
                if (block->getLVList()) {
                    const LVList* lvars = block->getLVList();
                    for (unsigned i = 0; i < lvars->size(); ++i) {
                        handler_locals.push_back(lvars->lv[i]);
                    }
                }
                const auto& stmts = block->getStatements();
                for (auto it = stmts.begin(); it != stmts.end(); ++it) {
                    collectStmtLocals(*it);
                }
            } else if (auto* if_stmt = dynamic_cast<const IfStatement*>(stmt)) {
                if (if_stmt->getLVList()) {
                    const LVList* lvars = if_stmt->getLVList();
                    for (unsigned i = 0; i < lvars->size(); ++i) {
                        handler_locals.push_back(lvars->lv[i]);
                    }
                }
                // Recurse into if/else code
                if (auto* if_code = if_stmt->getIfCode()) {
                    if (if_code->getLVList()) {
                        const LVList* lvars = if_code->getLVList();
                        for (unsigned i = 0; i < lvars->size(); ++i) {
                            handler_locals.push_back(lvars->lv[i]);
                        }
                    }
                    const auto& stmts = if_code->getStatements();
                    for (auto it = stmts.begin(); it != stmts.end(); ++it) {
                        collectStmtLocals(*it);
                    }
                }
                if (auto* else_code = if_stmt->getElseCode()) {
                    if (else_code->getLVList()) {
                        const LVList* lvars = else_code->getLVList();
                        for (unsigned i = 0; i < lvars->size(); ++i) {
                            handler_locals.push_back(lvars->lv[i]);
                        }
                    }
                    const auto& stmts = else_code->getStatements();
                    for (auto it = stmts.begin(); it != stmts.end(); ++it) {
                        collectStmtLocals(*it);
                    }
                }
            } else if (auto* for_stmt = dynamic_cast<const ForStatement*>(stmt)) {
                if (for_stmt->getLVList()) {
                    const LVList* lvars = for_stmt->getLVList();
                    for (unsigned i = 0; i < lvars->size(); ++i) {
                        handler_locals.push_back(lvars->lv[i]);
                    }
                }
                if (auto* code = for_stmt->getCode()) {
                    if (code->getLVList()) {
                        const LVList* lvars = code->getLVList();
                        for (unsigned i = 0; i < lvars->size(); ++i) {
                            handler_locals.push_back(lvars->lv[i]);
                        }
                    }
                    const auto& stmts = code->getStatements();
                    for (auto it = stmts.begin(); it != stmts.end(); ++it) {
                        collectStmtLocals(*it);
                    }
                }
            } else if (auto* while_stmt = dynamic_cast<const WhileStatement*>(stmt)) {
                if (while_stmt->getLVList()) {
                    const LVList* lvars = while_stmt->getLVList();
                    for (unsigned i = 0; i < lvars->size(); ++i) {
                        handler_locals.push_back(lvars->lv[i]);
                    }
                }
                if (auto* code = while_stmt->getCode()) {
                    if (code->getLVList()) {
                        const LVList* lvars = code->getLVList();
                        for (unsigned i = 0; i < lvars->size(); ++i) {
                            handler_locals.push_back(lvars->lv[i]);
                        }
                    }
                    const auto& stmts = code->getStatements();
                    for (auto it = stmts.begin(); it != stmts.end(); ++it) {
                        collectStmtLocals(*it);
                    }
                }
            } else if (auto* try_stmt = dynamic_cast<const TryStatement*>(stmt)) {
                if (auto* catch_var = try_stmt->getCatchVar()) {
                    handler_locals.push_back(catch_var);
                }
                if (auto* try_block = try_stmt->getTryBlock()) {
                    if (try_block->getLVList()) {
                        const LVList* lvars = try_block->getLVList();
                        for (unsigned i = 0; i < lvars->size(); ++i) {
                            handler_locals.push_back(lvars->lv[i]);
                        }
                    }
                    const auto& stmts = try_block->getStatements();
                    for (auto it = stmts.begin(); it != stmts.end(); ++it) {
                        collectStmtLocals(*it);
                    }
                }
                if (auto* catch_block = try_stmt->getCatchBlock()) {
                    if (catch_block->getLVList()) {
                        const LVList* lvars = catch_block->getLVList();
                        for (unsigned i = 0; i < lvars->size(); ++i) {
                            handler_locals.push_back(lvars->lv[i]);
                        }
                    }
                    const auto& stmts = catch_block->getStatements();
                    for (auto it = stmts.begin(); it != stmts.end(); ++it) {
                        collectStmtLocals(*it);
                    }
                }
            }
        };

    // Recurse into all handler statements
    const auto& stmts = handler_code->getStatements();
    for (auto it = stmts.begin(); it != stmts.end(); ++it) {
        collectStmtLocals(*it);
    }

    // Insert all collected locals into pre_instantiated_locals
    for (LocalVar* lv : handler_locals) {
        handler_func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(lv));
    }

    // Create entry block
    QoreIRBasicBlock* entry = handler_func->createBlock("entry");

    // Create a builder for the handler function
    QoreIRBuilder handler_builder(handler_func.get());
    handler_builder.setBlock(entry);

    // Create temporary lowering context for the handler body
    QoreIRLowering handler_lowering(handler_builder, parse_context);

    // Lower the handler body into the handler IR function
    if (!handler_lowering.lowerStatementBlock(handler_code, error)) {
        error = "handler body lowering failed: " + error;
        return nullptr;
    }

    // If handler doesn't end with a terminator (return, throw, branch), add return nothing
    // Note: the current block might be different from entry (e.g., after switch lowering),
    // so we check the actual current block
    QoreIRBasicBlock* final_block = handler_builder.getBlock();
    if (!final_block || final_block->instructions.empty() ||
            !isTerminator(final_block->instructions.back()->opcode)) {
        handler_builder.createReturnNothing();
    }

    // Update function metadata (value IDs from builder operations)
    handler_func->max_value_id = handler_builder.getFunction()->max_value_id;
    handler_func->max_local_slot_id = handler_builder.getFunction()->max_local_slot_id;

    // Transfer ownership to caller
    return handler_func.release();
}

int QoreIRLowering::compileBlockHandlerIRs(const std::vector<InlineHandler>& handlers,
        QoreIRFunction* parent_func, std::string& error) {
    // Compile a specific set of handlers (for a block level) without accumulation
    // This prevents pathological CFGs that occur when many handlers are compiled together.
    //
    // Returns the number of handlers successfully compiled on success, or -1
    // if any handler failed to lower.  Callers must treat -1 as an
    // outer-function lowering failure — there's no longer an AST fallback
    // at the handler level (see executeHandlerBody's assert).
    int compiled_count = 0;
    bool had_failure = false;

    if (!parent_func) {
        error = "no parent function available for handler compilation";
        return -1;
    }

    // Block-level handlers are compiled during statement-block lowering, before
    // the owning function's normal post-lowering slot pass.  Refresh parent slots
    // here so handler IR inherits the same slot IDs that the parent runtime frame
    // will use when the deferred handler executes.
    parent_func->computeSlotIdsAndEmbed();

    // Iterate through the handlers provided for this block level
    for (const InlineHandler& handler : handlers) {
        // Skip if already compiled or invalid
        if (!handler.obe_inst || !handler.code) {
            continue;
        }

        // Skip if handler already has IR attached (already compiled)
        if (handler.obe_inst->handler_ir) {
            continue;
        }

        // Create handler IR function
        auto handler_func = std::make_unique<QoreIRFunction>(qore_ir_next_handler_name());

        // Phase B1: Pre-seed handler's local_var_slots with parent's entries
        uint32_t parent_slot_count = parent_func->local_var_slots.size();
        handler_func->local_var_slots = parent_func->local_var_slots;
        handler_func->parent_slot_count = parent_slot_count;

        // Mark parent locals as pre-instantiated in the handler
        for (auto& [lvar, slot_id] : handler_func->local_var_slots) {
            if (lvar && slot_id < parent_slot_count) {
                handler_func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(lvar));
                handler_func->pre_instantiated_cache.insert(lvar);
            }
        }

        // Create entry block for handler
        QoreIRBasicBlock* entry = handler_func->createBlock("entry");

        // Create a builder for the handler function
        QoreIRBuilder handler_builder(handler_func.get());
        handler_builder.setBlock(entry);

        // Create temporary lowering context for the handler body
        QoreIRLowering handler_lowering(handler_builder, parse_context);

        // Lower the handler body into the handler IR function.  Any failure
        // here is an outer-function lowering failure — we no longer keep an
        // AST fallback for OBE handlers.  Continue compiling the remaining
        // handlers (so we surface every error in one pass) then report -1.
        std::string handler_error;
        if (!handler_lowering.lowerStatementBlock(handler.code, handler_error)) {
            if (getenv("QORE_IR_TRACE_OBE_FALLBACK")) {
                const QoreProgramLocation* loc = handler.code ? handler.code->loc : nullptr;
                fprintf(stderr, "[obe-compile-fail:block] %s:%d: %s\n",
                    loc ? (loc->getFile() ? loc->getFile() : "<unknown>") : "<no-loc>",
                    loc ? loc->start_line : 0, handler_error.c_str());
                fflush(stderr);
            }
            if (!error.empty()) {
                error += "; ";
            }
            error += "handler body lowering failed: " + handler_error;
            had_failure = true;
            continue;
        }

        // Recursively compile any nested handlers inside this handler body
        std::string nested_error;
        handler_lowering.compileAllHandlerIRs(nested_error);
        if (!nested_error.empty()) {
            if (!error.empty()) {
                error += "; ";
            }
            error += "nested handler compilation: " + nested_error;
        }

        // If handler doesn't end with a terminator, add return nothing
        QoreIRBasicBlock* final_block = handler_builder.getBlock();
        if (!final_block || final_block->instructions.empty() ||
                !isTerminator(final_block->instructions.back()->opcode)) {
            handler_builder.createReturnNothing();
        }

        // Update function metadata
        handler_func->max_value_id = handler_builder.getFunction()->max_value_id;
        handler_func->max_local_slot_id = handler_builder.getFunction()->max_local_slot_id;

        // Compute slot IDs for handler-specific locals (parent slots already pre-seeded)
        handler_func->computeSlotIdsAndEmbed();

        // Attach compiled handler IR to instruction
        handler.obe_inst->handler_ir = std::move(handler_func);
        compiled_count++;
    }

    return had_failure ? -1 : compiled_count;
}

int QoreIRLowering::compileAllHandlerIRs(std::string& error) {
    // Phase B2: Handler IR compilation with parent slot inheritance
    // QoreIRVerifier has been updated to handle pre-seeded parent slots
    // Handler functions can now be compiled with parent scope access.
    //
    // Returns compiled count on success, or -1 if any handler fails to
    // lower.  Callers must treat -1 as a function-level IR lowering
    // failure (the runtime `executeHandlerBody` asserts handler_ir is
    // populated — no AST fallback for OBE handlers).
    int compiled_count = 0;
    bool had_failure = false;
    QoreIRFunction* parent_func = builder.getFunction();

    if (!parent_func) {
        error = "no parent function available for handler compilation";
        return -1;
    }

    // Keep this invariant local to handler compilation as well.  Most callers
    // already run the slot pass before compileAllHandlerIRs(), but closure and
    // nested-handler paths can reach this point first.
    parent_func->computeSlotIdsAndEmbed();

    // Iterate through all registered handlers for this lowering context
    for (InlineHandler& handler : saved_top_level_handlers) {
        // Skip if already compiled or invalid
        if (!handler.obe_inst || !handler.code) {
            continue;
        }

        // Skip if handler already has IR attached (already compiled)
        if (handler.obe_inst->handler_ir) {
            continue;
        }

        // Create handler IR function
        auto handler_func = std::make_unique<QoreIRFunction>(qore_ir_next_handler_name());

        // Phase B1: Pre-seed handler's local_var_slots with parent's entries
        // This allows handler code to reference parent-scope variables by their parent slot IDs
        uint32_t parent_slot_count = parent_func->local_var_slots.size();
        handler_func->local_var_slots = parent_func->local_var_slots;
        handler_func->parent_slot_count = parent_slot_count;

        // Mark parent locals as pre-instantiated in the handler
        for (auto& [lvar, slot_id] : handler_func->local_var_slots) {
            if (lvar && slot_id < parent_slot_count) {
                handler_func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(lvar));
                handler_func->pre_instantiated_cache.insert(lvar);
            }
        }

        // Create entry block for handler
        QoreIRBasicBlock* entry = handler_func->createBlock("entry");

        // Create a builder for the handler function
        QoreIRBuilder handler_builder(handler_func.get());
        handler_builder.setBlock(entry);

        // Create temporary lowering context for the handler body
        // This will use the pre-seeded local_var_slots for scope access
        QoreIRLowering handler_lowering(handler_builder, parse_context);

        // Lower the handler body into the handler IR function.  Any failure
        // is now reported as an outer-function lowering failure — the
        // runtime assert in executeHandlerBody forbids AST fallback mid-IR.
        std::string handler_error;
        if (!handler_lowering.lowerStatementBlock(handler.code, handler_error)) {
            if (getenv("QORE_IR_TRACE_OBE_FALLBACK")) {
                const QoreProgramLocation* loc = handler.code ? handler.code->loc : nullptr;
                fprintf(stderr, "[obe-compile-fail] %s:%d: %s\n",
                    loc ? (loc->getFile() ? loc->getFile() : "<unknown>") : "<no-loc>",
                    loc ? loc->start_line : 0, handler_error.c_str());
                fflush(stderr);
            }
            if (!error.empty()) {
                error += "; ";
            }
            error += "handler body lowering failed: " + handler_error;
            had_failure = true;
            continue;
        }

        // Recursively compile any nested on_exit handlers inside this handler body
        std::string nested_error;
        handler_lowering.compileAllHandlerIRs(nested_error);
        // nested_error is informational only — append to outer error if non-empty
        if (!nested_error.empty()) {
            if (!error.empty()) {
                error += "; ";
            }
            error += "nested handler compilation: " + nested_error;
        }

        // If handler doesn't end with a terminator, add return nothing
        QoreIRBasicBlock* final_block = handler_builder.getBlock();
        if (!final_block || final_block->instructions.empty() ||
                !isTerminator(final_block->instructions.back()->opcode)) {
            handler_builder.createReturnNothing();
        }

        // Update function metadata
        handler_func->max_value_id = handler_builder.getFunction()->max_value_id;
        handler_func->max_local_slot_id = handler_builder.getFunction()->max_local_slot_id;

        // Compute slot IDs for handler-specific locals (parent slots already pre-seeded)
        handler_func->computeSlotIdsAndEmbed();

        // Attach compiled handler IR to instruction
        handler.obe_inst->handler_ir = std::move(handler_func);
        compiled_count++;
    }

    // Clear saved handlers after compilation to avoid polluting subsequent compilations
    saved_top_level_handlers.clear();

    return had_failure ? -1 : compiled_count;
}

bool QoreIRLowering::lowerHandlersAtExit(bool is_error, std::string& error, size_t start_index,
        size_t end_index, size_t barrier_depth) {
    // Debug-only sanity assert: a correct fix puts natural depth well
    // under this.  Real-world deeply-nested handlers in the qwf corpus
    // stay under 8 once the HandlerBarrier mechanism prevents the
    // re-entry cycle; tripping it signals a regression that must be fixed.
    static thread_local int handler_depth = 0;
    struct DepthGuard {
        int& d;
        DepthGuard(int& d) : d(d) { ++d; }
        ~DepthGuard() { --d; }
    } guard(handler_depth);
    assert(handler_depth <= 32 && "IR handler-inlining depth exploded — "
            "likely cleanup-emit cycle; see BlockCleanupEntry::HandlerBarrier");

    // Lower handlers in [start_index, end_index) in LIFO order.
    // Handlers are executed in reverse registration order (innermost to outermost).
    // end_index defaults to SIZE_MAX meaning "through the end of block_handlers"
    // (used by the normal fall-through path which has no concurrently-live
    // inner Scope entries to exclude).
    if (end_index > block_handlers.size()) {
        end_index = block_handlers.size();
    }
    if (block_handlers.empty() || start_index >= end_index) {
        return true;
    }

    // Process handlers in reverse order (LIFO), starting from the end
    for (int i = static_cast<int>(end_index) - 1; i >= static_cast<int>(start_index); --i) {
        const InlineHandler& handler = block_handlers[i];
        if (!shouldRunHandler(handler, is_error)) {
            continue;
        }

        // Push a HandlerBarrier sentinel on cleanup_stack at `barrier_depth`
        // so any `emitBlockCleanups` invoked from a non-local exit
        // (return/break/continue) *inside* the handler body's lowering
        // clamps its walk at the barrier — preventing the firing Scope
        // entry (which still lives on cleanup_stack at/above this depth
        // and still lists this handler in its handler_start range) from
        // being re-entered, which would re-fire this same handler and
        // recurse unboundedly.  Symmetric to the TryStatement /
        // RefForeach anchors added in 8fb555ac1 (that fix kept outer
        // Scope entries from claiming inner-scope handlers; this one
        // keeps an in-flight handler from claiming its own Scope).
        //
        // SIZE_MAX means "caller didn't compute a barrier" — e.g.
        // compileAllHandlerIRs's isolated instance with an empty
        // cleanup_stack where there's nothing to protect against.  In
        // that case we skip the barrier entirely.
        const bool have_barrier = (barrier_depth != SIZE_MAX);
        if (have_barrier) {
            BlockCleanupEntry barrier;
            barrier.type = BlockCleanupEntry::HandlerBarrier;
            barrier.handler_start = barrier_depth;
            barrier.loc = handler.code ? handler.code->loc : nullptr;
            cleanup_stack.push_back(barrier);
        }

        // Lower the handler code block inline using the current parse context
        // This gives the handler natural access to parent block's scope
        bool ok = lowerStatementBlock(handler.code, error);

        if (have_barrier) {
            // The barrier must still be on top — handler bodies push
            // their own entries above it and are responsible for popping
            // them before returning.  Assert loudly if someone violated
            // that.
            assert(!cleanup_stack.empty()
                && cleanup_stack.back().type == BlockCleanupEntry::HandlerBarrier
                && "HandlerBarrier disturbed by handler body");
            cleanup_stack.pop_back();
        }
        if (!ok) {
            return false;
        }
    }

    return true;
}

static bool isIntConstant(const QoreValue& value) {
    return value.isInt();
}

static bool isFloatConstant(const QoreValue& value) {
    return value.isFloat();
}

static bool isNumberConstant(const QoreValue& value) {
    return value.getInternalNode() && dynamic_cast<const QoreNumberNode*>(value.getInternalNode());
}

QoreIROpcode QoreIRLowering::selectComparisonOpcode(const QoreValue& left, const QoreValue& right,
        QoreIROpcode int_op, QoreIROpcode float_op, QoreIROpcode any_op) {
    if (isIntConstant(left) && isIntConstant(right)) {
        return int_op;
    }
    if (isFloatConstant(left) && isFloatConstant(right)) {
        return float_op;
    }
    return selectNumericOpcode(left, right, int_op, float_op, any_op);
}

const QoreTypeInfo* QoreIRLowering::selectAnalysisType(const QoreParseAnalysis& analysis) const {
    return analysis.narrowed_type ? analysis.narrowed_type : analysis.known_type;
}

bool QoreIRLowering::analysisIndicatesInt(const QoreParseAnalysis& analysis) const {
    const QoreTypeInfo* type = selectAnalysisType(analysis);
    return type && QoreTypeInfo::isType(type, NT_INT)
        && analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo);
}

bool QoreIRLowering::analysisIndicatesFloat(const QoreParseAnalysis& analysis) const {
    const QoreTypeInfo* type = selectAnalysisType(analysis);
    return type && QoreTypeInfo::isType(type, NT_FLOAT)
        && analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo);
}

bool QoreIRLowering::analysisIndicatesDate(const QoreParseAnalysis& analysis) const {
    const QoreTypeInfo* type = selectAnalysisType(analysis);
    return type && QoreTypeInfo::isType(type, NT_DATE)
        && analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo);
}

bool QoreIRLowering::guaranteedIntType(const QoreValue* expr) const {
    if (!expr) {
        return false;
    }
    const QoreTypeInfo* type = getGuaranteedTypeForValue(expr, nullptr);
    if (!type || !QoreTypeInfo::isType(type, NT_INT)) {
        return false;
    }
    if (QoreTypeInfo::parseReturns(type, NT_NOTHING) == QTI_IDENT) {
        return false;
    }
    if (expr->hasNode()) {
        QoreParseAnalysis analysis;
        if (getAnalysis(*expr, analysis) && analysis.hasFlag(QoreParseAnalysis::NeverNothing)) {
            return true;
        }
    }
    return true;
}

bool QoreIRLowering::guaranteedFloatType(const QoreValue* expr) const {
    if (!expr) {
        return false;
    }
    const QoreTypeInfo* type = getGuaranteedTypeForValue(expr, nullptr);
    if (!type || !QoreTypeInfo::isType(type, NT_FLOAT)) {
        return false;
    }
    if (QoreTypeInfo::parseReturns(type, NT_NOTHING) == QTI_IDENT) {
        return false;
    }
    if (expr->hasNode()) {
        QoreParseAnalysis analysis;
        if (getAnalysis(*expr, analysis) && analysis.hasFlag(QoreParseAnalysis::NeverNothing)) {
            return true;
        }
    }
    return true;
}

// Returns true if expr is $hash{"const_key"} where $hash is a local variable.
// Sets container_var, key_name, and key_expr if true.
static bool isConstKeyHashSubscript(const QoreValue& expr,
        const VarRefNode*& container_var, std::string& key_name, QoreValue& key_expr) {
    const AbstractQoreNode* node = expr.getInternalNode();
    if (!node) return false;
    const auto* hd = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(node);
    if (!hd) return false;
    const QoreValue right = hd->getRight();
    if (!right.hasNode() || right.getType() != NT_STRING) return false;
    QoreStringValueHelper key(right);
    key_name = key->c_str();
    key_expr = right;  // Also return the QoreValue for lowerExpression
    const auto* vr = dynamic_cast<const VarRefNode*>(hd->getLeft().getInternalNode());
    if (!vr) return false;
    qore_var_t vtype = vr->getType();
    // Allow local, local_ts, and closure variables
    if (vtype != VT_LOCAL && vtype != VT_LOCAL_TS && vtype != VT_CLOSURE) return false;
    // Reference-type variables must use the lvalue path to write through the reference
    // binding to the original variable.  The HashKeyStore optimization bypasses references.
    if (vr->ref.id && QoreTypeInfo::isReference(
            reinterpret_cast<const LocalVar*>(vr->ref.id)->getTypeInfo())) {
        return false;
    }
    // Object-typed containers (e.g., `self`) require the lvalue path for correct
    // member type-aware initialization (NOTHING → typed list/hash) via evalPlusEquals.
    // The load-compute-store pattern in emitHashKeyCompoundOp cannot handle this.
    if (vr->ref.id && QoreTypeInfo::getUniqueReturnClass(
            reinterpret_cast<const LocalVar*>(vr->ref.id)->getTypeInfo()) != nullptr) {
        return false;
    }
    container_var = vr;
    return true;
}

// Returns true if expr is $hash{dynamic_key} where $hash is a non-reference, non-object
// local variable and key is any lowerable expression (not just constant string).
// Sets container_var and key_expr (the dynamic key expression to lower).
static bool isDynamicKeyHashSubscript(const QoreValue& expr,
        const VarRefNode*& container_var, QoreValue& key_expr) {
    const AbstractQoreNode* node = expr.getInternalNode();
    if (!node) return false;
    const auto* hd = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(node);
    if (!hd) return false;
    key_expr = hd->getRight();
    const auto* vr = dynamic_cast<const VarRefNode*>(hd->getLeft().getInternalNode());
    if (!vr) return false;
    qore_var_t vtype = vr->getType();
    if (vtype != VT_LOCAL && vtype != VT_LOCAL_TS && vtype != VT_CLOSURE) return false;
    if (vr->ref.id && QoreTypeInfo::isReference(
            reinterpret_cast<const LocalVar*>(vr->ref.id)->getTypeInfo())) {
        return false;
    }
    if (vr->ref.id && QoreTypeInfo::getUniqueReturnClass(
            reinterpret_cast<const LocalVar*>(vr->ref.id)->getTypeInfo()) != nullptr) {
        return false;
    }
    container_var = vr;
    return true;
}

// Extract a structured lvalue path from an AST lvalue expression.
// Returns true if the expression can be represented as a sequence of LVPathSteps.
// On success, populates `path` and `dynamic_operands` (expressions to lower for dynamic keys/indices).
// This handles ALL variable types (local, closure, global, thread-local, self member, static var)
// and ALL navigation types (hash key const, hash key dynamic, list index).
//
// `allow_slice`: when true, multi-key hash slice (h{"a","b"}) and
// multi-index list slice (l[1,3,5]) lvalues are accepted and encoded as
// HashKeySlice / ListIndexSlice / ListRangeSlice terminal steps.  Callers that don't
// implement slice semantics at runtime (e.g. LValuePathAssign /
// LValuePathCompound / LValuePathBinaryMut helpers) must pass false so
// these shapes fall through to the existing AST-eval path.
static bool extractLValuePath(const QoreValue& expr,
        std::vector<LVPathStep>& path, std::vector<QoreValue>& dynamic_operands,
        bool allow_slice = false) {
    const AbstractQoreNode* node = expr.getInternalNode();
    if (!node) {
        return false;
    }

    // Root: VarRefNode (local, closure, global, thread-local, immediate)
    if (auto* vr = dynamic_cast<const VarRefNode*>(node)) {
        LVPathStep step;
        qore_var_t vtype = vr->getType();
        switch (vtype) {
            case VT_LOCAL:
            case VT_LOCAL_TS:
                step.kind = LVPathStepKind::LocalVar;
                step.ref_ptr = vr->ref.id;
                // Record the name so AOT deserialization can resolve the
                // variable via enclosing_locals name lookup when the closure
                // body captures a var that doesn't live in the closure's own
                // local_var_slots (captured vars from enclosing scopes).
                step.name = vr->getName() ? vr->getName() : "";
                step.type_info = vr->getTypeInfo();
                break;
            case VT_CLOSURE:
                step.kind = LVPathStepKind::ClosureVar;
                step.ref_ptr = vr->ref.id;
                step.name = vr->getName();
                step.type_info = vr->getTypeInfo();
                break;
            case VT_GLOBAL:
                step.kind = LVPathStepKind::GlobalVar;
                step.ref_ptr = vr->ref.var;
                step.name = vr->getName();
                step.type_info = vr->getTypeInfo();
                break;
            case VT_THREAD_LOCAL:
                step.kind = LVPathStepKind::ThreadLocalVar;
                step.ref_ptr = vr->ref.var;
                step.name = vr->getName();
                step.type_info = vr->getTypeInfo();
                break;
            case VT_IMMEDIATE:
                // Immediate closures — treat as closure var
                step.kind = LVPathStepKind::ClosureVar;
                step.ref_ptr = nullptr;
                step.name = vr->getName();
                step.type_info = vr->getTypeInfo();
                break;
            default:
                return false;
        }
        path.push_back(step);
        return true;
    }

    // Root: SelfVarrefNode (self.member)
    if (auto* sv = dynamic_cast<const SelfVarrefNode*>(node)) {
        LVPathStep step;
        step.kind = LVPathStepKind::SelfMember;
        step.name = sv->str;
        path.push_back(step);
        return true;
    }

    // Root: StaticClassVarRefNode (ClassName::var)
    if (auto* scv = dynamic_cast<const StaticClassVarRefNode*>(node)) {
        LVPathStep step;
        step.kind = LVPathStepKind::StaticVar;
        step.ref_ptr = scv;
        // Store "ClassPath::varName" so AOT deserialization can resolve the static var.
        // Use the runtime class path form (no leading "::") used by LoadStaticVar.
        step.name = std::string(scv->qc.getNamespacePath()) + "::" + scv->str;
        path.push_back(step);
        return true;
    }

    if (auto* dsv = dynamic_cast<const DeferredStaticClassMemberRefNode*>(node)) {
        LVPathStep step;
        if (dsv->class_path.empty()) {
            // AOT source parsing uses DeferredStaticClassMemberRefNode for
            // unresolved bare symbols.  In an lvalue root position, such a
            // symbol can only be a deferred global/thread-local variable.
            step.kind = LVPathStepKind::GlobalVar;
            step.name = dsv->member_name;
        } else {
            step.kind = LVPathStepKind::StaticVar;
            step.name = dsv->class_path + "::" + dsv->member_name;
        }
        step.ref_ptr = nullptr;
        path.push_back(step);
        return true;
    }

    // Navigation: QoreSquareBracketsRangeOperatorNode (container[start..stop])
    if (auto* sbr = dynamic_cast<const QoreSquareBracketsRangeOperatorNode*>(node)) {
        if (!allow_slice) {
            return false;
        }
        if (!extractLValuePath(sbr->get(0), path, dynamic_operands, allow_slice)) {
            return false;
        }
        LVPathStep step;
        step.kind = LVPathStepKind::ListRangeSlice;
        step.slice_operand_ids.reserve(2);
        dynamic_operands.push_back(sbr->get(1));
        step.slice_operand_ids.push_back(UINT32_MAX);
        dynamic_operands.push_back(sbr->get(2));
        step.slice_operand_ids.push_back(UINT32_MAX);
        path.push_back(std::move(step));
        return true;
    }

    // Navigation: QoreHashObjectDereferenceOperatorNode (container{key})
    if (auto* hd = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(node)) {
        const QoreValue right = hd->getRight();
        // Multi-key hash slice (e.g. h{"a","b"}): lower each sub-expression as a
        // dynamic operand and emit a HashKeySlice terminal step that carries the
        // list of SSA ids.  The runtime (patchLVPath + LValuePathUnary executor)
        // iterates the slice operands in declared order, mirroring the AST's
        // LValueRemoveHelper::doRemove path over a hash list selector.
        if (right.hasNode() && (right.getType() == NT_LIST || right.getType() == NT_PARSE_LIST)) {
            if (!allow_slice) {
                // Callers that don't support slice semantics (e.g. compound /
                // binary-mut ops) fall back to the AST-eval path.
                return false;
            }
            if (!extractLValuePath(hd->getLeft(), path, dynamic_operands, allow_slice)) {
                return false;
            }
            LVPathStep step;
            step.kind = LVPathStepKind::HashKeySlice;
            if (right.getType() == NT_PARSE_LIST) {
                const QoreParseListNode* pln = right.get<const QoreParseListNode>();
                const QoreParseListNode::nvec_t& vl = pln->getValues();
                step.slice_operand_ids.reserve(vl.size());
                for (const QoreValue& sub : vl) {
                    dynamic_operands.push_back(sub);
                    step.slice_operand_ids.push_back(UINT32_MAX);  // filled by caller
                }
            } else {
                const QoreListNode* l = right.get<const QoreListNode>();
                size_t n = l->size();
                step.slice_operand_ids.reserve(n);
                for (size_t i = 0; i < n; ++i) {
                    dynamic_operands.push_back(l->retrieveEntry(i));
                    step.slice_operand_ids.push_back(UINT32_MAX);
                }
            }
            path.push_back(std::move(step));
            return true;
        }
        // Recurse on left side (container)
        if (!extractLValuePath(hd->getLeft(), path, dynamic_operands, allow_slice)) {
            return false;
        }
        // Add hash key step
        LVPathStep step;
        if (right.hasNode() && right.getType() == NT_STRING) {
            step.kind = LVPathStepKind::HashKeyConst;
            QoreStringValueHelper key(right);
            step.name = key->c_str();
        } else {
            step.kind = LVPathStepKind::HashKey;
            // Store the expression to lower as a dynamic operand
            dynamic_operands.push_back(right);
            // operand_idx will be set after lowering
        }
        path.push_back(step);
        return true;
    }

    // Navigation: QoreSquareBracketsOperatorNode (container[index])
    if (auto* sb = dynamic_cast<const QoreSquareBracketsOperatorNode*>(node)) {
        const QoreValue right = sb->getRight();
        // Multi-index slice (e.g. l[1,3,5] or str[1,45,3..4,x,5]): lower each
        // sub-expression as a dynamic operand and emit a ListIndexSlice
        // terminal step.  Range operators inside the list are lowered to
        // full expressions and expand to int lists at runtime — the
        // LValuePathUnary executor iterates each operand, expanding nested
        // lists, and collects indices into a reverse-sorted set before
        // spliceSingle'ing each one (mirrors LValueRemoveHelper atomicity).
        if (right.hasNode() && (right.getType() == NT_LIST || right.getType() == NT_PARSE_LIST)) {
            if (!allow_slice) {
                return false;
            }
            if (!extractLValuePath(sb->getLeft(), path, dynamic_operands, allow_slice)) {
                return false;
            }
            LVPathStep step;
            step.kind = LVPathStepKind::ListIndexSlice;
            if (right.getType() == NT_PARSE_LIST) {
                const QoreParseListNode* pln = right.get<const QoreParseListNode>();
                const QoreParseListNode::nvec_t& vl = pln->getValues();
                step.slice_operand_ids.reserve(vl.size());
                for (const QoreValue& sub : vl) {
                    dynamic_operands.push_back(sub);
                    step.slice_operand_ids.push_back(UINT32_MAX);
                }
            } else {
                const QoreListNode* l = right.get<const QoreListNode>();
                size_t n = l->size();
                step.slice_operand_ids.reserve(n);
                for (size_t i = 0; i < n; ++i) {
                    dynamic_operands.push_back(l->retrieveEntry(i));
                    step.slice_operand_ids.push_back(UINT32_MAX);
                }
            }
            path.push_back(std::move(step));
            return true;
        }
        // Recurse on left side (container)
        if (!extractLValuePath(sb->getLeft(), path, dynamic_operands, allow_slice)) {
            return false;
        }
        // Add list index step
        LVPathStep step;
        step.kind = LVPathStepKind::ListIndex;
        dynamic_operands.push_back(right);
        path.push_back(step);
        return true;
    }

    // Unsupported expression type for path extraction
    return false;
}

// Returns true if expr is $list[index] where $list is a local variable and index is compile-time constant.
// Sets container_var and index_expr if true.
static bool isConstIndexListSubscript(const QoreValue& expr,
        const VarRefNode*& container_var, QoreValue& index_expr) {
    const AbstractQoreNode* node = expr.getInternalNode();
    if (!node) return false;

    // Check if this is a square brackets operator (list subscript)
    const auto* sb = dynamic_cast<const QoreSquareBracketsOperatorNode*>(node);
    if (!sb) return false;

    // Get the index expression - must be a compile-time constant value (literal or const expression)
    const QoreValue index = sb->getRight();
    if (!index.isValue()) {
        return false;
    }

    // Get the container - must be a local variable reference
    const auto* vr = dynamic_cast<const VarRefNode*>(sb->getLeft().getInternalNode());
    if (!vr) return false;

    // Verify it's a local, local_ts, or closure variable
    qore_var_t vtype = vr->getType();
    if (vtype != VT_LOCAL && vtype != VT_LOCAL_TS && vtype != VT_CLOSURE) return false;
    // Reference-type variables must use the lvalue path to write through the reference
    // binding to the original variable.  The ListIndexStore optimization bypasses references.
    if (vr->ref.id && QoreTypeInfo::isReference(
            reinterpret_cast<const LocalVar*>(vr->ref.id)->getTypeInfo())) {
        return false;
    }
    if (vr->ref.id) {
        const QoreTypeInfo* container_ti = reinterpret_cast<const LocalVar*>(vr->ref.id)->getTypeInfo();
        if (QoreTypeInfo::isType(container_ti, NT_BUFFER)
                || QoreTypeInfo::getReturnComplexBufferOrNothing(container_ti)) {
            return false;
        }
    }

    // Set output parameters and succeed
    container_var = vr;
    index_expr = index;
    return true;
}

bool QoreIRLowering::guaranteedNumberType(const QoreValue* expr) const {
    if (!expr) {
        return false;
    }
    const QoreTypeInfo* type = getGuaranteedTypeForValue(expr, nullptr);
    if (!type || !QoreTypeInfo::isType(type, NT_NUMBER)) {
        return false;
    }
    if (QoreTypeInfo::parseReturns(type, NT_NOTHING) == QTI_IDENT) {
        return false;
    }
    if (expr->hasNode()) {
        QoreParseAnalysis analysis;
        if (getAnalysis(*expr, analysis) && analysis.hasFlag(QoreParseAnalysis::NeverNothing)) {
            return true;
        }
    }
    return true;
}

bool QoreIRLowering::guaranteedStringType(const QoreValue* expr) const {
    if (!expr) {
        return false;
    }
    // Check for string literal
    if (expr->getType() == NT_STRING) {
        return true;
    }
    const QoreTypeInfo* type = getGuaranteedTypeForValue(expr, nullptr);
    if (!type || !QoreTypeInfo::isType(type, NT_STRING)) {
        return false;
    }
    if (QoreTypeInfo::parseReturns(type, NT_NOTHING) == QTI_IDENT) {
        return false;
    }
    if (expr->hasNode()) {
        QoreParseAnalysis analysis;
        if (getAnalysis(*expr, analysis) && analysis.hasFlag(QoreParseAnalysis::NeverNothing)) {
            return true;
        }
    }
    return true;
}

QoreIROpcode QoreIRLowering::selectNumericOpcode(const QoreValue& left, const QoreValue& right,
        QoreIROpcode int_op, QoreIROpcode float_op, QoreIROpcode any_op) {
    if (isIntConstant(left) && isIntConstant(right)) {
        return int_op;
    }
    if (isFloatConstant(left) && isFloatConstant(right)) {
        return float_op;
    }
    // Mixed int/float constants → promote to float (int * 0.1 → float)
    if ((isIntConstant(left) && isFloatConstant(right))
            || (isFloatConstant(left) && isIntConstant(right))) {
        return float_op;
    }
    if (guaranteedIntType(&left) && guaranteedIntType(&right)) {
        return int_op;
    }
    if (guaranteedFloatType(&left) && guaranteedFloatType(&right)) {
        return float_op;
    }
    // Mixed int/float guaranteed types → promote to float
    if ((guaranteedIntType(&left) && guaranteedFloatType(&right))
            || (guaranteedFloatType(&left) && guaranteedIntType(&right))) {
        return float_op;
    }
    if (parse_context) {
        QoreParseAnalysis left_analysis;
        QoreParseAnalysis right_analysis;
        if (getAnalysis(left, left_analysis) && getAnalysis(right, right_analysis)) {
            if (analysisIndicatesInt(left_analysis) && analysisIndicatesInt(right_analysis)) {
                return int_op;
            }
            if (analysisIndicatesFloat(left_analysis) && analysisIndicatesFloat(right_analysis)) {
                return float_op;
            }
            // Mixed int/float analysis → promote to float
            if ((analysisIndicatesInt(left_analysis) && analysisIndicatesFloat(right_analysis))
                    || (analysisIndicatesFloat(left_analysis) && analysisIndicatesInt(right_analysis))) {
                return float_op;
            }
            const QoreTypeInfo* left_known = selectAnalysisType(left_analysis);
            const QoreTypeInfo* right_known = selectAnalysisType(right_analysis);
            if (left_known && right_known && QoreTypeInfo::isType(left_known, NT_INT)
                    && QoreTypeInfo::isType(right_known, NT_INT)) {
                return int_op;
            }
            if (left_known && right_known && QoreTypeInfo::isType(left_known, NT_FLOAT)
                    && QoreTypeInfo::isType(right_known, NT_FLOAT)) {
                return float_op;
            }
            // Mixed int/float known types → promote to float
            if (left_known && right_known) {
                bool left_is_int = QoreTypeInfo::isType(left_known, NT_INT);
                bool left_is_float = QoreTypeInfo::isType(left_known, NT_FLOAT);
                bool right_is_int = QoreTypeInfo::isType(right_known, NT_INT);
                bool right_is_float = QoreTypeInfo::isType(right_known, NT_FLOAT);
                if ((left_is_int && right_is_float) || (left_is_float && right_is_int)) {
                    return float_op;
                }
            }
        }
    }
    return any_op;
}

QoreIROpcode QoreIRLowering::selectNumericOpcode(const QoreValue& left, const QoreValue& right,
        QoreIROpcode int_op, QoreIROpcode float_op, QoreIROpcode any_op,
        QoreIROpcode number_op) {
    // Use number opcode for number literal constants (confirmed to work)
    // Variable type detection requires further investigation of type info availability
    if (isNumberConstant(left) && isNumberConstant(right)) {
        return number_op;
    }

    // Delegate to existing selectNumericOpcode for int/float/any
    return selectNumericOpcode(left, right, int_op, float_op, any_op);
}

QoreIROpcode QoreIRLowering::selectFoldOpcode(const QoreParseAnalysis& analysis,
        QoreIROpcode any_op, QoreIROpcode int_op, QoreIROpcode float_op) const {
    if (analysisIndicatesInt(analysis)) {
        return int_op;
    }
    if (analysisIndicatesFloat(analysis)) {
        return float_op;
    }
    return any_op;
}

static bool isRangeLValue(const QoreValue& value) {
    const AbstractQoreNode* node = value.getInternalNode();
    return node && dynamic_cast<const QoreSquareBracketsRangeOperatorNode*>(node);
}

static bool getLValueBaseValue(const QoreValue& value, QoreValue& base) {
    const AbstractQoreNode* node = value.getInternalNode();
    if (!node) {
        return false;
    }
    if (dynamic_cast<const VarRefNode*>(node)) {
        base = value;
        return true;
    }
    if (auto* op = dynamic_cast<const QoreBinaryLValueOperatorNode*>(node)) {
        return getLValueBaseValue(op->getLeft(), base);
    }
    if (auto* op = dynamic_cast<const QoreBinaryIntLValueOperatorNode*>(node)) {
        return getLValueBaseValue(op->getLeft(), base);
    }
    if (auto* op = dynamic_cast<const QoreSquareBracketsOperatorNode*>(node)) {
        return getLValueBaseValue(op->getLeft(), base);
    }
    if (auto* op = dynamic_cast<const QoreSquareBracketsRangeOperatorNode*>(node)) {
        return getLValueBaseValue(op->get(0), base);
    }
    if (auto* op = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(node)) {
        return getLValueBaseValue(op->getLeft(), base);
    }
    if (auto* op = dynamic_cast<const QoreShiftOperatorNode*>(node)) {
        return getLValueBaseValue(op->getExp(), base);
    }
    if (auto* op = dynamic_cast<const QoreUnshiftOperatorNode*>(node)) {
        return getLValueBaseValue(op->getLeft(), base);
    }
    if (auto* op = dynamic_cast<const QoreSpliceOperatorNode*>(node)) {
        return getLValueBaseValue(op->getLValue(), base);
    }
    if (auto* op = dynamic_cast<const QorePreIncrementOperatorNode*>(node)) {
        return getLValueBaseValue(op->getExp(), base);
    }
    if (auto* op = dynamic_cast<const QorePostIncrementOperatorNode*>(node)) {
        return getLValueBaseValue(op->getExp(), base);
    }
    if (auto* op = dynamic_cast<const QorePreDecrementOperatorNode*>(node)) {
        return getLValueBaseValue(op->getExp(), base);
    }
    if (auto* op = dynamic_cast<const QorePostDecrementOperatorNode*>(node)) {
        return getLValueBaseValue(op->getExp(), base);
    }
    return false;
}

static bool isInvokeLValueNode(const AbstractQoreNode* node) {
    return dynamic_cast<const QoreBinaryLValueOperatorNode*>(node)
        || dynamic_cast<const QoreBinaryIntLValueOperatorNode*>(node)
        || dynamic_cast<const QorePreIncrementOperatorNode*>(node)
        || dynamic_cast<const QorePostIncrementOperatorNode*>(node)
        || dynamic_cast<const QorePreDecrementOperatorNode*>(node)
        || dynamic_cast<const QorePostDecrementOperatorNode*>(node)
        || dynamic_cast<const QoreShiftOperatorNode*>(node)
        || dynamic_cast<const QoreUnshiftOperatorNode*>(node)
        || dynamic_cast<const QoreSpliceOperatorNode*>(node);
}

static std::string describeIRLoweringExpr(const QoreValue& expr) {
    std::string desc = "QoreValue type '";
    desc += expr.getTypeName();
    desc += "'";

    const AbstractQoreNode* node = expr.hasNode() ? expr.getInternalNode() : nullptr;
    if (!node) {
        return desc;
    }

    desc += ", node '";
    desc += node->getTypeName();
    desc += "'";

    const auto* parse_node = dynamic_cast<const ParseNode*>(node);
    if (parse_node && parse_node->loc) {
        desc += " at ";
        desc += parse_node->loc->getFile() ? parse_node->loc->getFile() : "<unknown>";
        desc += ":";
        desc += std::to_string(parse_node->loc->start_line);
    }
    return desc;
}

static const std::string& getIRExprRegistryValidationError() {
    static const std::string registry_error = []() {
        std::string error;
        if (!qore_ir_validate_expr_registry(error)) {
            return "IR expression registry validation failed: " + error;
        }
        return std::string();
    }();
    return registry_error;
}

QoreIRValue QoreIRLowering::lowerExpression(const QoreValue& expr, std::string& error) {
    const std::string& registry_error = getIRExprRegistryValidationError();
    if (!registry_error.empty()) {
        error = registry_error;
        return QoreIRValue();
    }

    QoreIRValue plugin_lowered = tryPluginLowering(expr, error);
    if (plugin_lowered.isValid() || !error.empty()) {
        return plugin_lowered;
    }

    // Dispatch through explicitly claimed expression handlers. Once a handler
    // claims an expression shape, silent NotApplicable is an error: this keeps
    // future handlers/plugins from relying on ordering fallthrough.
    if (const QoreIRExprHandlerInfo* claimed = qore_ir_find_expr_handler(expr)) {
        const QoreIRExprHandlerInfo& info = *claimed;
        QoreIRExprCtx ctx{*this, expr, error};
        QoreIRValue result = info.handler(ctx);
        if (result.isValid() || !error.empty()) {
            return result;
        }

        error = "IR expression handler '";
        error += info.name ? info.name : "<unnamed>";
        error += "' claimed ";
        error += describeIRLoweringExpr(expr);
        error += " but returned no IR value and no diagnostic; fix the handler claim predicate or report why "
            "the expression cannot be lowered";
        return QoreIRValue();
    }

    const AbstractQoreNode* node = expr.getInternalNode();
    if (auto* impl_arg = dynamic_cast<const QoreImplicitArgumentNode*>(node)) {
        int offset = impl_arg->getOffset();
        // Check virtual implicit context first - avoids runtime push/pop/load overhead
        if (virtual_implicit.active) {
            if (offset == 0 && virtual_implicit.arg0.isValid()) {
                return virtual_implicit.arg0;
            }
            if (offset == 1 && virtual_implicit.arg1.isValid()) {
                return virtual_implicit.arg1;
            }
            if (offset == -1) {
                // $argv - build list from virtual values on the fly
                QoreIRValue argv_list = builder.createEmptyList(impl_arg->loc)->result;
                if (virtual_implicit.arg0.isValid()) {
                    builder.createListAppend(argv_list, virtual_implicit.arg0, impl_arg->loc);
                }
                if (virtual_implicit.arg1.isValid()) {
                    builder.createListAppend(argv_list, virtual_implicit.arg1, impl_arg->loc);
                }
                return argv_list;
            }
        }
        if (offset == -1) {
            // $argv - entire argument list
            return builder.createLoadImplicitArgv(impl_arg->loc)->result;
        }
        // $1, $2, etc. - specific argument (offset is 0 for $1, 1 for $2, etc.)
        return builder.createLoadImplicitArg(offset, impl_arg->loc)->result;
    }
    if (auto* impl_elem = dynamic_cast<const QoreImplicitElementNode*>(node)) {
        // Check virtual implicit context first
        if (virtual_implicit.active && virtual_implicit.element.isValid()) {
            return virtual_implicit.element;
        }
        return builder.createLoadImplicitElement(impl_elem->loc)->result;
    }
    // Handle expression node shapes that still carry AST metadata but execute through
    // dedicated native IR/JIT/AOT helpers.  Unsupported shapes must report an error
    // below instead of emitting a generic AST-evaluation fallback.
    if (auto* closure = dynamic_cast<const QoreClosureParseNode*>(node)) {
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {}, normal_block, handler, closure->loc);
            inst->invoke_opcode = QoreIROpcode::CreateClosure;
            builder.setBlock(normal_block);
            return inst->result;
        }
        return builder.createCreateClosure(closure, expr, closure->loc)->result;
    }
    if (auto* runtime_closure = dynamic_cast<const QoreClosureBase*>(node)) {
        cvv_vec_t* cvec = runtime_closure->getCvec();
        if (cvec && !cvec->empty()) {
            error = "unsupported runtime closure lowering: captured-variable runtime closures in constant containers "
                "cannot be reconstructed safely";
            return QoreIRValue();
        }
        if (runtime_closure->getObject()) {
            error = "unsupported runtime closure lowering: object-bound runtime closures in constant containers "
                "cannot be reconstructed safely";
            return QoreIRValue();
        }

        const QoreClosureParseNode* closure = runtime_closure->getClosureParseNode();
        QoreValue closure_expr(closure->refSelf());
        QoreIRValue rv;
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                closure_expr.discard(nullptr);
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(closure_expr, {}, normal_block, handler,
                closure ? closure->loc : nullptr);
            inst->invoke_opcode = QoreIROpcode::CreateClosure;
            builder.setBlock(normal_block);
            rv = inst->result;
        } else {
            rv = builder.createCreateClosure(closure, closure_expr, closure ? closure->loc : nullptr)->result;
        }
        closure_expr.discard(nullptr);
        return rv;
    }
    if (auto* parse_ref = dynamic_cast<const ParseReferenceNode*>(node)) {
        // Pre-evaluate dynamic top-level lvalue selectors in parse references.
        // This keeps virtual implicit values such as $# bound to the current IR
        // map/select context instead of letting the AST reference evaluator read
        // stale thread-local implicit state at runtime.
        std::vector<QoreIRValue> selector_operands;
        const QoreValue& lv_expr = parse_ref->getLVExp();
        if (lv_expr.hasNode()) {
            auto* hd = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(
                lv_expr.getInternalNode());
            if (hd) {
                QoreIRValue key_val = lowerExpression(hd->getRight(), error);
                if (key_val.isValid()) {
                    selector_operands.push_back(key_val);
                }
            } else if (auto* sb = dynamic_cast<const QoreSquareBracketsOperatorNode*>(
                    lv_expr.getInternalNode())) {
                QoreIRValue index_val = lowerExpression(sb->getRight(), error);
                if (index_val.isValid()) {
                    selector_operands.push_back(index_val);
                }
            }
        }
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, selector_operands, normal_block, handler, parse_ref->loc);
            inst->invoke_opcode = QoreIROpcode::CreateParseRef;
            builder.setBlock(normal_block);
            return inst->result;
        }
        // Non-exception path: use the specialized CreateParseRef instruction
        auto* cpr_inst = builder.createCreateParseRef(parse_ref, expr, parse_ref->loc);
        // Add selector operands for complex hash/list member access.
        for (auto& op : selector_operands) {
            cpr_inst->operands.push_back(op);
        }
        return cpr_inst->result;
    }
    if (dynamic_cast<const AbstractCallReferenceNode*>(node)) {
        auto* parse_node = dynamic_cast<const ParseNode*>(node);
        const QoreProgramLocation* loc = parse_node ? parse_node->loc : nullptr;
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {}, normal_block, handler, loc);
            inst->invoke_opcode = QoreIROpcode::CreateCallRef;
            builder.setBlock(normal_block);
            return inst->result;
        }
        return builder.createCreateCallRef(expr, loc)->result;
    }
    if (auto* ctx_ref = dynamic_cast<const ContextrefNode*>(node)) {
        return builder.createContextRef(ctx_ref->str, 0, ctx_ref->loc)->result;
    }
    if (auto* complex_ctx_ref = dynamic_cast<const ComplexContextrefNode*>(node)) {
        return builder.createContextRef(complex_ctx_ref->member,
            complex_ctx_ref->stack_offset, complex_ctx_ref->loc)->result;
    }
    if (auto* ctx_row = dynamic_cast<const ContextRowNode*>(node)) {
        return builder.createContextRow(ctx_row->loc)->result;
    }
    if (auto* self_ref = dynamic_cast<const SelfVarrefNode*>(node)) {
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {}, normal_block, handler, self_ref->loc);
            inst->invoke_opcode = QoreIROpcode::LoadSelfMember;
            builder.setBlock(normal_block);
            return inst->result;
        }
        return builder.createLoadSelfMember(self_ref->str, self_ref->loc)->result;
    }
    // Static class variable references (e.g., AbstractDataProviderType::anyDataType)
    if (auto* static_var = dynamic_cast<const StaticClassVarRefNode*>(node)) {
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {}, normal_block, handler, static_var->loc);
            inst->invoke_opcode = QoreIROpcode::LoadStaticVar;
            builder.setBlock(normal_block);
            return inst->result;
        }
        return builder.createLoadStaticVar(&static_var->vi, static_var->str.c_str(),
                expr, static_var->loc)->result;
    }
    if (auto* deferred_static = dynamic_cast<const DeferredStaticClassMemberRefNode*>(node)) {
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {}, normal_block, handler, deferred_static->loc);
            inst->invoke_opcode = QoreIROpcode::LoadStaticVar;
            builder.setBlock(normal_block);
            return inst->result;
        }
        return builder.createLoadStaticVar(nullptr, deferred_static->member_name.c_str(),
                expr, deferred_static->loc)->result;
    }
    if (auto* backquote = dynamic_cast<const BackquoteNode*>(node)) {
        auto* inst = builder.createBackquote(backquote->str, backquote->loc);
        if (!exception_stack.empty()) {
            inst->exception_target = exception_stack.back();
        }
        return inst->result;
    }
    if (auto* find_node = dynamic_cast<const FindNode*>(node)) {
        auto* inst = builder.createFind(find_node->exp, find_node->find_exp, find_node->where,
                static_cast<int32_t>(find_node->getMode()), find_node->loc);
        if (!exception_stack.empty()) {
            inst->exception_target = exception_stack.back();
        }
        return inst->result;
    }
    if (auto* rt_const = dynamic_cast<const RuntimeConstantRefNode*>(node)) {
        // Delayed constants are evaluated once during parse commit.  Lower
        // resolved scalar/literal values directly so AOT does not need an
        // unserializable RuntimeConstantRefNode for primitive folded constants
        // such as `foldl $1 | $2, ConstList`.  Keep containers and objects as
        // named constants; the AOT reverse map preserves their identity and
        // avoids inlining large or partially non-serializable graphs.
        ConstantEntry* ce = rt_const->getConstantEntry();
        if (ce) {
            QoreValue resolved = ce->getReferencedValue();
            if (!ce->aot_shell_pending && ce->hasValue() && !resolved.needsEval()) {
                qore_type_t resolved_type = resolved.getType();
                if (resolved.isEnum() || resolved_type == NT_INT || resolved_type == NT_FLOAT
                        || resolved_type == NT_BOOLEAN || resolved_type == NT_STRING
                        || resolved_type == NT_DATE || resolved_type == NT_NUMBER
                        || resolved_type == NT_BINARY || resolved_type == NT_NULL
                        || resolved_type == NT_NOTHING) {
                    auto* inst = builder.createLoadConstant(nullptr, resolved, rt_const->loc);
                    resolved.discard(nullptr);
                    return inst->result;
                }
            }
            resolved.discard(nullptr);
        }
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {}, normal_block, handler, rt_const->loc);
            inst->invoke_opcode = QoreIROpcode::LoadConstant;
            builder.setBlock(normal_block);
            return inst->result;
        }
        return builder.createLoadConstant(rt_const, expr, rt_const->loc)->result;
    }
    if (auto* new_hd = dynamic_cast<const NewHashDeclNode*>(node)) {
        const QoreParseListNode* pargs = new_hd->args;
        QoreIRValue hash_val;
        if (pargs && !pargs->empty()) {
            if (pargs->size() != 1) {
                error = "hashdecl constructor argument count mismatch";
                return QoreIRValue();
            }
            hash_val = lowerExpression(pargs->get(0), error);
            if (!hash_val.isValid()) {
                return QoreIRValue();
            }
        } else {
            std::vector<std::string> empty_keys;
            std::vector<QoreIRValue> empty_vals;
            hash_val = builder.createMakeHashConstKeys(std::move(empty_keys), empty_vals, new_hd->loc, nullptr)
                ->result;
        }
        const QoreTypeInfo* hd_type_info = new_hd->hd
            ? qore_substitute_type_params_if_needed(new_hd->hd->getTypeInfo()) : nullptr;
        const TypedHashDecl* hd = QoreTypeInfo::getUniqueReturnHashDecl(hd_type_info);
        const char* hd_path = nullptr;
        if (!hd && new_hd->isDynamicHashDeclConstruct()) {
            hd_path = new_hd->getDynamicHashDeclName().c_str();
        }
        if (!hd && (!hd_path || !*hd_path)) {
            error = "hashdecl construction target could not be resolved after generic type substitution";
            return QoreIRValue();
        }
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {hash_val}, normal_block, handler, new_hd->loc);
            inst->invoke_opcode = QoreIROpcode::NewHashDeclFromHash;
            builder.setBlock(normal_block);
            return inst->result;
        }
        return hd
            ? builder.createNewHashDeclFromHash(hd, new_hd->runtime_check, hash_val, new_hd->loc)->result
            : builder.createNewHashDeclFromHash(hd_path, nullptr, new_hd->runtime_check,
                hash_val, new_hd->loc)->result;
    }
    if (auto* new_ch = dynamic_cast<const NewComplexHashNode*>(node)) {
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {}, normal_block, handler, new_ch->loc);
            inst->invoke_opcode = QoreIROpcode::NewComplexHash;
            builder.setBlock(normal_block);
            return inst->result;
        }
        return builder.createNewComplexHash(new_ch, expr, new_ch->loc)->result;
    }
    if (auto* new_cl = dynamic_cast<const NewComplexListNode*>(node)) {
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {}, normal_block, handler, new_cl->loc);
            inst->invoke_opcode = QoreIROpcode::NewComplexList;
            builder.setBlock(normal_block);
            return inst->result;
        }
        return builder.createNewComplexList(new_cl, expr, new_cl->loc)->result;
    }
    if (auto* new_cb = dynamic_cast<const NewComplexBufferNode*>(node)) {
        std::vector<QoreIRValue> operands;
        if (!new_cb->args.isNothing() && !new_cb->shouldEvaluateWithNode()) {
            QoreIRValue arg = lowerExpression(new_cb->args, error);
            if (!arg.isValid()) {
                return QoreIRValue();
            }
            operands.push_back(arg);
        }
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, operands, normal_block, handler, new_cb->loc);
            inst->invoke_opcode = QoreIROpcode::NewComplexBuffer;
            builder.setBlock(normal_block);
            return inst->result;
        }
        auto* inst = builder.createNewComplexBuffer(new_cb, expr, new_cb->loc);
        inst->operands = std::move(operands);
        return inst->result;
    }
    // ParseNewComplexTypeNode and ParseNoEvalNode are parse-time-only nodes whose evalImpl()
    // asserts false — they cannot be delegated to AST evaluation
    if (dynamic_cast<const ParseNewComplexTypeNode*>(node) || dynamic_cast<const ParseNoEvalNode*>(node)) {
        error = "parse-only node not supported for IR lowering";
        return QoreIRValue();
    }
    if (auto* new_obj = dynamic_cast<const NewObjectCallNode*>(node)) {
        // No-AST path: lower each constructor arg as a separate IR instruction,
        // then pass the computed operand values to the NewObject instruction.
        std::vector<QoreIRValue> operands;
        if (!lowerCallArgs(new_obj->getParseArgs(), new_obj->getArgs(), operands, error)) {
            return QoreIRValue();
        }
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, operands, normal_block, handler, nullptr);
            inst->invoke_opcode = QoreIROpcode::NewObject;
            builder.setBlock(normal_block);
            return inst->result;
        }
        return builder.createNewObject(new_obj->getClass(), new_obj->getVariant(),
                operands, expr, new_obj->getObjectTypeInfo(), nullptr)->result;
    }
    if (auto* scoped_obj = dynamic_cast<const ScopedObjectCallNode*>(node)) {
        // No-AST path: lower each constructor arg as a separate IR instruction.
        std::vector<QoreIRValue> operands;
        if (!lowerCallArgs(scoped_obj->getParseArgs(), scoped_obj->getArgs(), operands, error)) {
            return QoreIRValue();
        }
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, operands, normal_block, handler, scoped_obj->loc);
            inst->invoke_opcode = QoreIROpcode::NewObject;
            builder.setBlock(normal_block);
            return inst->result;
        }
        return builder.createNewObject(scoped_obj->oc, scoped_obj->getVariant(),
                operands, expr, scoped_obj->getObjectTypeInfo(), scoped_obj->loc)->result;
    }
    // Parse-time constant values: use LoadConstant (returns expr.refSelf() at runtime)
    if (dynamic_cast<const QoreObject*>(node)
            || dynamic_cast<const QoreNumberNode*>(node)
            || dynamic_cast<const BinaryNode*>(node)) {
        return builder.createLoadConstant(nullptr, expr, nullptr)->result;
    }
    // Object method references (e.g., \methodName())
    if (auto* mref = dynamic_cast<const AbstractParseObjectMethodReferenceNode*>(node)) {
        std::vector<QoreIRValue> operands;
        if (auto* omref = dynamic_cast<const ParseObjectMethodReferenceNode*>(node)) {
            QoreIRValue obj = lowerExpression(omref->getExp(), error);
            if (!obj.isValid()) {
                return QoreIRValue();
            }
            operands.push_back(obj);
        }
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, operands, normal_block, handler, mref->loc);
            inst->invoke_opcode = QoreIROpcode::CreateMethodRef;
            builder.setBlock(normal_block);
            return inst->result;
        }
        auto* inst = builder.createCreateMethodRef(expr, mref->loc);
        inst->operands = operands;
        return inst->result;
    }
    // QoreListNode/QoreHashNode can still contain unevaluated entries, for
    // example single-argument `rethrow expr` wraps expr in a needs-eval list.
    // Lower container elements natively first so AOT serialization does not
    // treat evaluable entries as unsupported constant payloads.
    if (dynamic_cast<const QoreHashNode*>(node) || dynamic_cast<const QoreListNode*>(node)) {
        QoreIRValue value = lowerContainerLiteral(expr, error);
        if (value.isValid() || !error.empty()) {
            return value;
        }
        // Preserve the old path for concrete constants that cannot be rebuilt
        // from native operands, such as constant objects in containers.
        return builder.createLoadConstant(nullptr, expr, nullptr)->result;
    }
    error = std::string("unsupported expression node for IR lowering: ")
        + (node ? node->getTypeName() : expr.getTypeName());
    return QoreIRValue();
}

bool QoreIRLowering::getAnalysis(const QoreValue& expr, QoreParseAnalysis& analysis) const {
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
        try {
            if (parse_init_value(temp, *parse_context)) {
                parse_context->typeInfo = saved_type;
                return false;
            }
        } catch (...) {
            parse_context->typeInfo = saved_type;
            return false;
        }
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

bool QoreIRLowering::guardVarLValue(const QoreValue& exp, std::string& error, bool allow_maybe_nothing) {
    const AbstractQoreNode* node = exp.getInternalNode();
    auto* var = dynamic_cast<const VarRefNode*>(node);
    if (!var) {
        return true;
    }

    std::string guard_error;
    QoreIRValue value = loadVarRef(var, guard_error, "lvalue guard", exp);
    if (!value.isValid()) {
        if (!guard_error.empty()) {
            error = guard_error;
        }
        return false;
    }
    maybeInsertNotNothingGuard(value, &exp, getVarRefLocation(var), getVarRefTypeInfo(var), allow_maybe_nothing);
    return true;
}

bool QoreIRLowering::guardLValueBase(const QoreValue& exp, std::string& error, bool allow_maybe_nothing) {
    QoreValue base;
    if (!getLValueBaseValue(exp, base)) {
        return true;
    }
    return guardVarLValue(base, error, allow_maybe_nothing);
}

void QoreIRLowering::markLocalAssignmentFromExpression(const QoreValue& exp) {
    if (!parse_context || !exp.hasNode()) {
        return;
    }
    const AbstractQoreNode* node = exp.getInternalNode();
    auto* var = dynamic_cast<const VarRefNode*>(node);
    if (!var || var->getType() != VT_LOCAL) {
        return;
    }
    LocalVar* local = var->ref.id;
    if (!local) {
        return;
    }
    parse_context->markLocalAssignment(local, true, local->parseGetTypeInfo());
}

void QoreIRLowering::markLocalUnassignmentFromExpression(const QoreValue& exp) {
    if (!parse_context || !exp.hasNode()) {
        return;
    }
    if (LocalVar* local = getLocalVarFromValue(exp)) {
        parse_context->markLocalAssignment(local, false, nullptr);
    }
}

bool QoreIRLowering::expressionCanThrow(const QoreValue& expr) const {
    if (!expr.hasNode()) {
        // Simple values (int, float, bool, NOTHING, etc.) can never throw
        return false;
    }
    auto* node = dynamic_cast<const ParseNode*>(expr.getInternalNode());
    if (!node) {
        return true;
    }
    return !node->getParseAnalysis().hasFlag(QoreParseAnalysis::NeverThrows);
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
    QoreIRBasicBlock* b = func->createBlock(name);
    // Stamp the innermost enclosing loop's exit so a debugger DebugFlowBreak firing
    // in this block can be resolved to the correct loop exit by the IR interpreter.
    if (b) {
        b->enclosing_loop_exit = current_loop_exit;
    }
    return b;
}

static bool isContainerLiteral(const QoreValue& v) {
    if (!v.hasNode()) {
        return false;
    }
    const AbstractQoreNode* node = v.getInternalNode();
    return dynamic_cast<const QoreParseHashNode*>(node) || dynamic_cast<const QoreParseListNode*>(node)
        || dynamic_cast<const QoreHashNode*>(node) || dynamic_cast<const QoreListNode*>(node);
}

QoreIRValue QoreIRLowering::lowerConstant(const QoreValue& expr, std::string& error) {
    // TAG_ENUM must be checked first: getType() returns the base type (e.g., NT_INT),
    // so base-type-specific paths below would strip enum identity
    if (expr.isEnum()) {
        return builder.createConstEnum(expr.getEnumMember())->result;
    }
    // QoreNullNode satisfies isNothing() through the generic node predicate;
    // check NULL first so IR preserves NULL instead of lowering it as NOTHING.
    if (expr.isNull()) {
        return builder.createConstNull()->result;
    }
    if (expr.isNothing()) {
        return builder.createConstNothing()->result;
    }
    if (expr.isBool()) {
        return builder.createConstBool(expr.getAsBool())->result;
    }
    if (expr.isChar()) {
        return builder.createConstChar(expr.getChar())->result;
    }
    if (expr.isInt()) {
        return builder.createConstInt(expr.getAsBigInt())->result;
    }
    if (expr.isFloat()) {
        return builder.createConstFloat(expr.getAsFloat())->result;
    }
    // Handle QoreIntNode (heap-allocated integer outside 48-bit range)
    if (expr.getType() == NT_INT && expr.hasNode()) {
        return builder.createConstInt(expr.getAsBigInt())->result;
    }
    // Handle QoreFloatNode (heap-allocated float, e.g., negative NaN)
    if (expr.getType() == NT_FLOAT && expr.hasNode()) {
        return builder.createConstFloat(expr.getAsFloat())->result;
    }
    if (expr.getType() == NT_LIST && expr.isValue()) {
        const QoreListNode* list = expr.get<const QoreListNode>();
        if (list) {
            std::vector<QoreIRValue> values;
            values.reserve(list->size());
            for (size_t i = 0; i < list->size(); ++i) {
                QoreValue entry = list->retrieveEntry(i);
                QoreIRValue lowered = lowerContainerElement(entry, error);
                if (!lowered.isValid()) {
                    return QoreIRValue();
                }
                values.push_back(lowered);
            }
            // Extract complexTypeInfo from constant list
            const QoreTypeInfo* cti = qore_list_private::get(*list)->complexTypeInfo;
            return builder.createMakeList(values, nullptr, cti)->result;
        }
    }
    if (expr.getType() == NT_HASH && expr.isValue()) {
        const QoreHashNode* hash = expr.get<const QoreHashNode>();
        if (hash) {
            if (hash->getHashDecl()) {
                return builder.createLoadConstant(nullptr, expr, nullptr)->result;
            }
            std::vector<QoreIRValue> values;
            ConstHashIterator it(hash);
            while (it.next()) {
                const char* key = it.getKey();
                values.push_back(builder.createConstString(key ? key : "")->result);
                QoreValue entry = it.get();
                QoreIRValue lowered = lowerContainerElement(entry, error);
                if (!lowered.isValid()) {
                    return QoreIRValue();
                }
                values.push_back(lowered);
            }
            // Extract complexTypeInfo from constant hash
            const QoreTypeInfo* cti = qore_hash_private::get(*hash)->complexTypeInfo;
            return builder.createMakeHash(values, nullptr, cti)->result;
        }
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
        auto* inst = builder.createConstDate(micros, is_relative);
        if (!is_relative) {
            // Preserve the original timezone for absolute dates so UTC "Z"
            // dates are reconstructed with UTC zone, not the current local zone
            inst->constant.date_zone = dt->getZone();
            inst->constant.date_zone_set = true;
        }
        if (is_relative) {
            // Always preserve full relative date components to avoid lossy
            // conversions (e.g., -1D → -24h loses the day semantics, 5Y → hours
            // loses year precision across leap years)
            qore_tm info;
            dt->getInfo(info);
            inst->constant.rel_years = info.year;
            inst->constant.rel_months = info.month;
            inst->constant.rel_days = info.day;
            inst->constant.rel_hours = info.hour;
            inst->constant.rel_minutes = info.minute;
            inst->constant.rel_seconds = info.second;
            inst->constant.rel_us = info.us;
        }
        return inst->result;
    }
    return QoreIRValue();
}

QoreIRValue QoreIRLowering::lowerVarRef(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* var = dynamic_cast<const VarRefNode*>(node);
    if (!var) {
        return QoreIRValue();
    }
    // Handle VarRefNewObjectNode (e.g., "Foo f("hello")") — a variable declaration
    // with implicit constructor call. Split into construction + assignment.
    if (auto* vrn = dynamic_cast<const VarRefNewObjectNode*>(node)) {
        // Check if this is a class constructor (VRN_OBJECT)
        const QoreClass* qc = QoreTypeInfo::getUniqueReturnClass(vrn->getTypeInfo());
        if (qc) {
            // VRN_OBJECT: construct object using NewObject opcode, then store to variable.
            // No-AST path: lower each constructor arg as a separate IR instruction.
            std::vector<QoreIRValue> operands;
            if (!lowerCallArgs(vrn->getParseArgs(), vrn->getArgs(), operands, error)) {
                return QoreIRValue();
            }
            QoreIRValue obj_val;
            if (!exception_stack.empty()) {
                QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
                if (!normal_block) {
                    error = "IR builder failed to create invoke continuation block";
                    return QoreIRValue();
                }
                QoreIRBasicBlock* handler = exception_stack.back();
                auto* inst = builder.createInvoke(expr, operands, normal_block, handler, var->loc);
                inst->invoke_opcode = QoreIROpcode::NewObject;
                builder.setBlock(normal_block);
                obj_val = inst->result;
            } else {
                obj_val = builder.createNewObject(qc, vrn->getVariant(),
                    operands, expr, vrn->getTypeInfo(), var->loc)->result;
            }
            // Store the constructed object to the variable
            if (!storeVarRef(var, obj_val, error, "VarRefNewObjectNode", &expr, var->loc)) {
                return QoreIRValue();
            }
            return obj_val;
        }
        // Hashdecl construction: decompose into hash lowering + NewHashDeclFromHash
        // This ensures local variable references in the hash initializer are properly
        // lowered as individual IR instructions (LoadLocal etc.), enabling correct AOT
        // serialization instead of baking pre-evaluated values into the AST.
        const QoreTypeInfo* runtime_type_info = qore_substitute_type_params_if_needed(vrn->getTypeInfo());
        const TypedHashDecl* hd = QoreTypeInfo::getUniqueReturnHashDecl(runtime_type_info);
        if (hd && vrn->isHashDeclConstruct()) {
            // Undo ast_delegate_count: hashdecl args are fully lowered via IR,
            // not delegated to AST evaluation
            --ast_delegate_count;
            const QoreParseListNode* pargs = vrn->getParseArgs();
            QoreIRValue hash_val;
            if (pargs && !pargs->empty()) {
                // Lower the hash initializer expression (single arg) via lowerExpression
                // which properly handles QoreParseHashNode → MakeHashConstKeys with
                // individual IR instructions for each value
                hash_val = lowerExpression(pargs->get(0), error);
                if (!hash_val.isValid()) {
                    return QoreIRValue();
                }
            } else {
                // No args: create an empty hash
                std::vector<std::string> empty_keys;
                std::vector<QoreIRValue> empty_vals;
                hash_val = builder.createMakeHashConstKeys(std::move(empty_keys), empty_vals,
                    var->loc, nullptr)->result;
            }

            QoreIRValue construct_val;
            if (!exception_stack.empty()) {
                QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
                if (!normal_block) {
                    error = "IR builder failed to create invoke continuation block";
                    return QoreIRValue();
                }
                QoreIRBasicBlock* handler = exception_stack.back();
                auto* inst = builder.createInvoke(expr, {hash_val}, normal_block, handler, var->loc);
                inst->invoke_opcode = QoreIROpcode::NewHashDeclFromHash;
                builder.setBlock(normal_block);
                construct_val = inst->result;
            } else {
                construct_val = builder.createNewHashDeclFromHash(hd,
                    vrn->getRuntimeCheck(), hash_val, var->loc)->result;
            }
            // Store the constructed value to the variable
            if (!storeVarRef(var, construct_val, error, "VarRefNewObjectNode", &expr, var->loc)) {
                return QoreIRValue();
            }
            return construct_val;
        }

        if (vrn->isComplexHashConstruct()) {
            const QoreParseListNode* pargs = vrn->getParseArgs();
            QoreIRValue hash_val;
            if (pargs && !pargs->empty()) {
                if (pargs->size() != 1) {
                    error = "complex hash constructor argument count mismatch";
                    return QoreIRValue();
                }
                hash_val = lowerExpression(pargs->get(0), error);
                if (!hash_val.isValid()) {
                    return QoreIRValue();
                }
            } else {
                std::vector<std::string> empty_keys;
                std::vector<QoreIRValue> empty_vals;
                hash_val = builder.createMakeHashConstKeys(std::move(empty_keys), empty_vals,
                    var->loc, nullptr)->result;
            }

            QoreIRValue construct_val;
            if (!exception_stack.empty()) {
                QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
                if (!normal_block) {
                    error = "IR builder failed to create invoke continuation block";
                    return QoreIRValue();
                }
                QoreIRBasicBlock* handler = exception_stack.back();
                auto* inst = builder.createInvoke(expr, {hash_val}, normal_block, handler, var->loc);
                inst->invoke_opcode = QoreIROpcode::VrnConstruct;
                builder.setBlock(normal_block);
                construct_val = inst->result;
            } else {
                auto* inst = builder.createVrnConstruct(vrn, expr, var->loc);
                inst->operands.push_back(hash_val);
                construct_val = inst->result;
            }
            if (!storeVarRef(var, construct_val, error, "VarRefNewObjectNode", &expr, var->loc)) {
                return QoreIRValue();
            }
            return construct_val;
        }

        if (vrn->isComplexListConstruct()) {
            const QoreValue& new_args = vrn->getNewArgs();
            QoreIRValue value = new_args.isNothing()
                ? builder.createConstNothing(var->loc)->result
                : lowerExpression(new_args, error);
            if (!value.isValid()) {
                return QoreIRValue();
            }

            QoreIRValue construct_val;
            if (!exception_stack.empty()) {
                QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
                if (!normal_block) {
                    error = "IR builder failed to create invoke continuation block";
                    return QoreIRValue();
                }
                QoreIRBasicBlock* handler = exception_stack.back();
                auto* inst = builder.createInvoke(expr, {value}, normal_block, handler, var->loc);
                inst->invoke_opcode = QoreIROpcode::VrnConstruct;
                builder.setBlock(normal_block);
                construct_val = inst->result;
            } else {
                auto* inst = builder.createVrnConstruct(vrn, expr, var->loc);
                inst->operands.push_back(value);
                construct_val = inst->result;
            }
            if (!storeVarRef(var, construct_val, error, "VarRefNewObjectNode", &expr, var->loc)) {
                return QoreIRValue();
            }
            return construct_val;
        }

        // Non-hashdecl types (complex hash/list): construct + store via VrnConstruct
        QoreIRValue construct_val;
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {}, normal_block, handler, var->loc);
            inst->invoke_opcode = QoreIROpcode::VrnConstruct;
            builder.setBlock(normal_block);
            construct_val = inst->result;
        } else {
            construct_val = builder.createVrnConstruct(vrn, expr, var->loc)->result;
        }
        // Store the constructed value to the variable
        if (!storeVarRef(var, construct_val, error, "VarRefNewObjectNode", &expr, var->loc)) {
            return QoreIRValue();
        }
        return construct_val;
    }
    return loadVarRef(var, error, "variable reference", expr);
}

 QoreIRValue QoreIRLowering::loadVarRef(const VarRefNode* var, std::string& error, const char* context,
         const QoreValue& expr) {
    if (!var) {
        error = std::string("null lvalue in IR lowering (") + context + ")";
        return QoreIRValue();
    }
    QoreIRValue result;
    switch (var->getType()) {
        case VT_LOCAL:
            if (!var->ref.id) {
                error = std::string("unresolved local variable reference in IR lowering (") + context + ")";
                return QoreIRValue();
            }
            // Route closure-use VT_LOCAL through LoadClosure so the value is
            // always read from the cvstack (not a local alloca). The parser
            // sets closureUse() on the LocalVar after the VarRefNode is created,
            // so the VarRefNode may still have VT_LOCAL even though the variable
            // is captured by a closure.
            if (var->ref.id->closureUse()) {
                result = builder.createLoadClosure(var->ref.id, var->loc)->result;
            } else {
                result = builder.createLoadLocal(var->ref.id, var->loc)->result;
            }
            break;
        case VT_CLOSURE:
        case VT_LOCAL_TS:
            if (!var->ref.id) {
                error = std::string("unresolved closure variable reference in IR lowering (") + context + ")";
                return QoreIRValue();
            }
            result = builder.createLoadClosure(var->ref.id, var->loc)->result;
            break;
        case VT_GLOBAL:
            if (!var->ref.var) {
                error = std::string("unresolved global variable reference in IR lowering (") + context + ")";
                return QoreIRValue();
            }
            result = builder.createLoadGlobal(var->ref.var, var->loc)->result;
            break;
        case VT_THREAD_LOCAL:
            if (!var->ref.var) {
                error = std::string("unresolved thread-local variable reference in IR lowering (") + context + ")";
                return QoreIRValue();
            }
            result = builder.createLoadThreadLocal(var->ref.var, var->loc)->result;
            break;
        case VT_IMMEDIATE:
            result = builder.createLoadLValue(expr, var->loc)->result;
            break;
        default:
            error = std::string("unsupported variable reference for IR lowering (") + context + ")";
            return QoreIRValue();
    }
    QoreParseAnalysis analysis;
    if (getAnalysis(expr, analysis)) {
        QoreIRValueFacts facts;
        const QoreTypeInfo* declared_type = var->getType() == VT_LOCAL && var->ref.id
            ? var->ref.id->getTypeInfo() : nullptr;
        facts.type_info = analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
            ? analysis.known_type : declared_type;
        bool local_definitely_assigned = parse_context && var->getType() == VT_LOCAL && var->ref.id
            && parse_context->isLocalDefinitelyAssigned(var->ref.id);
        bool plain_local = declared_type && !QoreTypeInfo::getReferenceTarget(declared_type);
        bool definitely_assigned = analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned)
            || (local_definitely_assigned && plain_local);
        facts.assigned_state = definitely_assigned
            ? QoreIRAssignedState::Assigned : QoreIRAssignedState::MaybeAssigned;
        facts.never_nothing = analysis.hasFlag(QoreParseAnalysis::NeverNothing)
            || (definitely_assigned && facts.type_info
                && QoreTypeInfo::parseReturns(facts.type_info, NT_NOTHING) == QTI_NOT_EQUAL);
        if (facts.never_nothing && facts.type_info) {
            if (QoreTypeInfo::isType(facts.type_info, NT_INT)) {
                facts.representation = QoreIRValueRepresentation::NativeInt;
            } else if (QoreTypeInfo::isType(facts.type_info, NT_FLOAT)) {
                facts.representation = QoreIRValueRepresentation::NativeFloat;
            } else if (QoreTypeInfo::isType(facts.type_info, NT_BOOLEAN)) {
                facts.representation = QoreIRValueRepresentation::NativeBool;
            } else {
                facts.representation = QoreIRValueRepresentation::Boxed;
            }
        } else {
            facts.representation = QoreIRValueRepresentation::Boxed;
        }
        builder.getFunction()->setValueFacts(result, facts);
    }
    // NOTE: no guard here — consuming operators (lowerRange, storeVarRef, guardVarLValue, etc.)
    // emit their own guards with proper exception scope context
    return result;
}

bool QoreIRLowering::storeVarRef(const VarRefNode* var, QoreIRValue value, std::string& error,
        const char* context, const QoreValue* expr, const QoreProgramLocation* guard_loc, bool weak,
        QoreIRValue* store_result) {
    if (!var) {
        error = std::string("null lvalue in IR lowering (") + context + ")";
        return false;
    }
    const QoreTypeInfo* target_type = getVarRefTypeInfo(var);
    const QoreProgramLocation* loc = guard_loc ? guard_loc : getVarRefLocation(var);
    bool allow_maybe_nothing = !expr || expr->isNothing();
    maybeInsertNotNothingGuard(value, expr, loc, target_type, allow_maybe_nothing);
    switch (var->getType()) {
        case VT_LOCAL:
            if (!var->ref.id) {
                error = std::string("unresolved local variable reference in IR lowering (") + context + ")";
                return false;
            }
            // Route closure-use VT_LOCAL through StoreClosure so the value is
            // always written to the cvstack (not a local alloca). See loadVarRef
            // comment for why closureUse() may be true even for VT_LOCAL.
            if (var->ref.id->closureUse()) {
                auto* store_inst = builder.createStoreClosure(var->ref.id, value, var->loc, weak);
                store_inst->initial_assignment = var->isDecl();
                if (store_result) {
                    store_inst->result = builder.getFunction()->createValue();
                    *store_result = store_inst->result;
                }
                if (!exception_stack.empty()) {
                    store_inst->exception_target = exception_stack.back();
                }
                if (parse_context) {
                    parse_context->markLocalAssignment(var->ref.id, true, target_type);
                }
                return true;
            }
            {
                auto* store_inst = builder.createStoreLocal(var->ref.id, value, var->loc, weak);
                store_inst->initial_assignment = var->isDecl();
                if (store_result) {
                    store_inst->result = builder.getFunction()->createValue();
                    *store_result = store_inst->result;
                }
                if (!exception_stack.empty()) {
                    store_inst->exception_target = exception_stack.back();
                }
            }
            if (parse_context) {
                parse_context->markLocalAssignment(var->ref.id, true, target_type);
            }
            return true;
        case VT_CLOSURE:
            if (!var->ref.id) {
                error = std::string("unresolved closure variable reference in IR lowering (") + context + ")";
                return false;
            }
            {
                auto* store_inst = builder.createStoreClosure(var->ref.id, value, var->loc, weak);
                store_inst->initial_assignment = var->isDecl();
                if (store_result) {
                    store_inst->result = builder.getFunction()->createValue();
                    *store_result = store_inst->result;
                }
                if (!exception_stack.empty()) {
                    store_inst->exception_target = exception_stack.back();
                }
            }
            // PHASE 3: Track closure variable assignment for guard suppression
            if (parse_context) {
                parse_context->markLocalAssignment(var->ref.id, true, target_type);
            }
            return true;
        case VT_LOCAL_TS:
            if (!var->ref.id) {
                error = std::string("unresolved closure variable reference in IR lowering (") + context + ")";
                return false;
            }
            {
                auto* store_inst = builder.createStoreClosure(var->ref.id, value, var->loc, weak);
                store_inst->initial_assignment = var->isDecl();
                if (store_result) {
                    store_inst->result = builder.getFunction()->createValue();
                    *store_result = store_inst->result;
                }
                if (!exception_stack.empty()) {
                    store_inst->exception_target = exception_stack.back();
                }
            }
            return true;
        case VT_GLOBAL:
            if (!var->ref.var) {
                error = std::string("unresolved global variable reference in IR lowering (") + context + ")";
                return false;
            }
            {
                auto* store_inst = builder.createStoreGlobal(var->ref.var, value, var->loc, weak);
                if (store_result) {
                    store_inst->result = builder.getFunction()->createValue();
                    *store_result = store_inst->result;
                }
                if (!exception_stack.empty()) {
                    store_inst->exception_target = exception_stack.back();
                }
            }
            return true;
        case VT_THREAD_LOCAL:
            if (!var->ref.var) {
                error = std::string("unresolved thread-local variable reference in IR lowering (") + context + ")";
                return false;
            }
            {
                auto* store_inst = builder.createStoreThreadLocal(var->ref.var, value, var->loc, weak);
                if (store_result) {
                    store_inst->result = builder.getFunction()->createValue();
                    *store_result = store_inst->result;
                }
                if (!exception_stack.empty()) {
                    store_inst->exception_target = exception_stack.back();
                }
            }
            return true;
        default:
            break;
    }
    error = std::string("unsupported variable reference for IR lowering (") + context + ")";
    return false;
}

const QoreTypeInfo* QoreIRLowering::getVarRefTypeInfo(const VarRefNode* var) const {
    if (!var) {
        return nullptr;
    }
    switch (var->getType()) {
        case VT_LOCAL:
        case VT_LOCAL_TS:
        case VT_CLOSURE:
            return var->ref.id ? var->ref.id->getTypeInfo() : nullptr;
        case VT_GLOBAL:
        case VT_THREAD_LOCAL:
            return var->ref.var ? var->ref.var->getTypeInfo() : nullptr;
        default:
            break;
    }
    return nullptr;
}

const QoreProgramLocation* QoreIRLowering::getVarRefLocation(const VarRefNode* var) const {
    return var ? var->loc : nullptr;
}

LocalVar* QoreIRLowering::getLocalVarFromValue(const QoreValue& expr) const {
    if (!expr.hasNode()) {
        return nullptr;
    }
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* var = dynamic_cast<const VarRefNode*>(node);
    // Accept both local (VT_LOCAL) and closure-captured (VT_CLOSURE) variables —
    // both use var->ref.id to identify the LocalVar, which carries the declared
    // type info needed for type-specialized switch/arithmetic lowering.
    if (!var || (var->getType() != VT_LOCAL && var->getType() != VT_CLOSURE)) {
        return nullptr;
    }
    return var->ref.id;
}

QoreIRValue QoreIRLowering::findLoopInvariantListSize(LocalVar* local) const {
    if (!local) {
        return QoreIRValue();
    }
    for (auto it = loop_invariant_list_sizes.rbegin(); it != loop_invariant_list_sizes.rend(); ++it) {
        auto found = it->find(local);
        if (found != it->end()) {
            return found->second;
        }
    }
    return QoreIRValue();
}

QoreIRValue QoreIRLowering::findLoopInvariantValue(const QoreValue& expr) const {
    if (!expr.hasNode()) {
        return QoreIRValue();
    }
    const AbstractQoreNode* node = expr.getInternalNode();
    if (!node) {
        return QoreIRValue();
    }
    for (auto it = loop_invariant_values.rbegin(); it != loop_invariant_values.rend(); ++it) {
        auto found = it->find(node);
        if (found != it->end()) {
            return found->second;
        }
    }
    return QoreIRValue();
}

const QoreTypeInfo* QoreIRLowering::getGuaranteedTypeForValue(const QoreValue* expr,
        const QoreTypeInfo* fallback) const {
    if (fallback) {
        return fallback;
    }
    if (!expr || !parse_context) {
        return nullptr;
    }
    if (LocalVar* local = getLocalVarFromValue(*expr)) {
        return parse_context->guaranteedType(local);
    }
    return nullptr;
}

// PHASE 3: Delegate to centralized opcode registry instead of maintaining separate switch statement
//! Returns true if the given opcode can legitimately produce a NOTHING result
//! Queries the centralized registry (QoreOpcodeRegistry.h) for opcode properties
static bool opcodeCanReturnNothing(QoreIROpcode op) {
    return getOpcodeCanReturnNothing(static_cast<int>(op));
}

// PHASE 3: Delegate to centralized opcode registry instead of maintaining separate switch statement
//! Returns true if the given opcode is guaranteed to never return NOTHING
//! Queries the centralized registry (QoreOpcodeRegistry.h) for opcode properties
static bool opcodeNeverReturnsNothing(QoreIROpcode op) {
    return getOpcodeNeverReturnsNothing(static_cast<int>(op));
}

bool QoreIRLowering::needsNotNothingGuard(const QoreValue* expr, const QoreTypeInfo* target_type,
        bool allow_maybe_nothing) const {

    static bool debug_guard_entry = [] {
        const char* debug_env = getenv("QORE_IR_DEBUG");
        return debug_env && strstr(debug_env, "guard");
    }();

    if (debug_guard_entry) {
        fprintf(stderr, "[GUARD-NEED-CHECK-ENTRY] expr=%p has_node=%d, target_type=%p, allow_maybe=%d\n",
                expr, expr && expr->hasNode(), target_type, allow_maybe_nothing);
        fflush(stderr);
    }

    // Constants are never NOTHING - no guard needed
    if (expr && !expr->isNothing() && !expr->hasNode()) {
        return false;
    }
    // Non-ParseNode runtime values (QoreBigIntNode, QoreStringNode, etc.) are never NOTHING
    if (expr && expr->hasNode()) {
        const AbstractQoreNode* node = expr->getInternalNode();
        if (node && !dynamic_cast<const ParseNode*>(node)) {
            return false;
        }
    }

    // CRITICAL FIX: Prevent deopt-causing guards
    // When allow_maybe_nothing is true, skip guard even if target expects non-nothing
    // This handles optional returns that are being assigned to non-optional variables
    // and prevents IR->AST deopt that causes side effect re-execution
    if (allow_maybe_nothing) {
        static bool debug_guard = [] {
            const char* debug_env = getenv("QORE_IR_DEBUG");
            return debug_env && strstr(debug_env, "guard");
        }();
        if (debug_guard) {
            fprintf(stderr, "[GUARD-SKIP-DEOPT-RISK] Skipping guard to prevent deopt: allow_maybe_nothing=true\n");
            fflush(stderr);
        }
        return false;
    }

    // PHASE 1: Check VarRefNode for optional type BEFORE any getAnalysis calls
    // This catches optional-typed variables early and prevents deopt on legitimate NOTHING values
    // But only return early if we DEFINITELY don't need a guard; let other cases fall through
    if (expr && expr->hasNode()) {
        const AbstractQoreNode* node = expr->getInternalNode();
        if (node && dynamic_cast<const ParseNode*>(node) && node->getType() == NT_VARREF) {
            const VarRefNode* var_ref = static_cast<const VarRefNode*>(node);
            const QoreTypeInfo* var_type = var_ref->getTypeInfo();
            // If variable has optional type (*Type), it can legitimately return NOTHING
            if (var_type && QoreTypeInfo::parseReturns(var_type, NT_NOTHING) != QTI_NOT_EQUAL) {
                if (debug_guard_entry) {
                    fprintf(stderr, "[GUARD-SKIP-OPTIONAL-VAR-PHASE1] Variable is optional type, skipping guard\n");
                    fflush(stderr);
                }
                return false;  // Variable can be nothing - no guard needed, return early
            }
        }
    }

    // Check if the expression itself can return nothing (e.g., optional return type)
    // If so, skip the guard - optional types are allowed to return nothing
    if (expr && expr->hasNode()) {
        const AbstractQoreNode* node = expr->getInternalNode();
        if (node && dynamic_cast<const ParseNode*>(node)) {
            QoreParseAnalysis expr_analysis;
            bool got_analysis = false;
            try {
                got_analysis = getAnalysis(*expr, expr_analysis);
            } catch (...) {
                got_analysis = false;
            }

            static bool debug_guard = [] {
                const char* debug_env = getenv("QORE_IR_DEBUG");
                return debug_env && strstr(debug_env, "guard");
            }();

            if (got_analysis) {
                // Use actual type information from analysis if available
                if (expr_analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo) && expr_analysis.known_type) {
                    const QoreTypeInfo* expr_type = expr_analysis.known_type;
                    // Check if the expression's type allows nothing (is optional like *string)
                    if (QoreTypeInfo::parseReturns(expr_type, NT_NOTHING) != QTI_NOT_EQUAL) {
                        if (debug_guard) {
                            fprintf(stderr, "[GUARD-SKIP-OPTIONAL-TYPE] Expression type is optional\n");
                            fflush(stderr);
                        }
                        return false;  // Optional types can return nothing - no guard
                    }
                    // Expression type is known and non-optional
                    // BUT: must still check target_type before deciding to insert guard
                    // If target_type allows NOTHING, we don't need a guard
                    // Store type for target_type check below, don't return yet
                    if (debug_guard) {
                        fprintf(stderr, "[GUARD-EXPR-TYPE-NON-OPTIONAL] Expression type is non-optional, checking target_type\n");
                        fflush(stderr);
                    }
                    // Fall through to target_type check at line 3463
                } else {
                    // Analysis succeeded but no KnownTypeInfo
                    // Be conservative: don't insert guard for operations we can't fully understand
                    if (debug_guard) {
                        fprintf(stderr, "[GUARD-SKIP-INCOMPLETE-ANALYSIS] Analysis present but no type info - skip guard\n");
                        fflush(stderr);
                    }
                    return false;
                }
            }
        }
    }

    // CRITICAL: Check if the expression can legitimately return nothing
    // Operations like "remove" can return nothing even on non-optional types.
    // Only require non-nothing when we can prove the expression type never allows it.

    // If expr is not available, we can't reliably determine if guard is needed
    if (!expr || !expr->hasNode()) {
        // Without expression information, don't insert guard speculatively
        return false;
    }

    const QoreTypeInfo* type = nullptr;
    const AbstractQoreNode* node = expr->getInternalNode();
    if (!node || !dynamic_cast<const ParseNode*>(node)) {
        return false;  // Not a parse node - not a guard candidate
    }

    // Note: VarRefNode optional type check already handled in Phase 1 above
    // This fallback section deals with other ParseNode types

    QoreParseAnalysis expr_analysis;
    bool got_analysis = false;
    try {
        got_analysis = getAnalysis(*expr, expr_analysis);
    } catch (...) {
        got_analysis = false;
    }

    if (!got_analysis) {
        // No analysis available - can't prove guard is needed
        return false;
    }

    // Check if expression can return nothing
    if (expr_analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo) && expr_analysis.known_type) {
        type = expr_analysis.known_type;
        // If expression type allows nothing, no guard needed
        if (QoreTypeInfo::parseReturns(type, NT_NOTHING) != QTI_NOT_EQUAL) {
            return false;  // Optional type - can return nothing
        }
        // Expression type is known and doesn't allow nothing
        // Continue to check target type as well
    } else if (expr_analysis.hasFlag(QoreParseAnalysis::NeverNothing)) {
        // Expression is guaranteed to never return nothing
        return false;
    } else {
        // Analysis available but no definitive type information
        // Be conservative - don't insert guard for operations we can't fully understand
        return false;
    }

    // At this point: expression type is known, doesn't allow nothing
    // Check target type for additional constraint
    type = target_type;

    if (type && QoreTypeInfo::parseReturns(type, NT_NOTHING) == QTI_NOT_EQUAL) {
        if (debug_guard_entry) {
            fprintf(stderr, "[GUARD-TARGET-TYPE-CHECK] target_type doesn't allow nothing\n");
            fflush(stderr);
        }
        if (expr && expr->hasNode()) {
            if (LocalVar* local = getLocalVarFromValue(*expr)) {
                if (allow_maybe_nothing) {
                    return false;
                }
                bool result = parse_context ? parse_context->needsGuardForLocal(local) : true;
                if (debug_guard_entry) {
                    fprintf(stderr, "[GUARD-RETURN-TRUE-LVAR] needsGuardForLocal returned %d\n", result);
                    fflush(stderr);
                }
                return result;
            }
            QoreParseAnalysis analysis;
            bool got_analysis = false;
            try {
                got_analysis = getAnalysis(*expr, analysis);
            } catch (...) {
                if (debug_guard_entry) {
                    fprintf(stderr, "[GUARD-SKIP-EXCEPTION] Exception in getAnalysis, skip guard to avoid deopt risk\n");
                    fflush(stderr);
                }
                // If analysis fails, don't insert guard speculatively to avoid deopt-causing guard failures
                return false;
            }
            if (got_analysis) {
                // Use analysis to check expression's actual type and behavior
                if (analysis.hasFlag(QoreParseAnalysis::NeverNothing)) {
                    if (debug_guard_entry) {
                        fprintf(stderr, "[GUARD-SKIP-NEVER-NOTHING-ANALYSIS] Analysis shows NeverNothing\n");
                        fflush(stderr);
                    }
                    return false;
                }
                // Check if analysis has type info showing expression allows NOTHING
                if (analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo) && analysis.known_type) {
                    const QoreTypeInfo* expr_type = analysis.known_type;
                    if (QoreTypeInfo::parseReturns(expr_type, NT_NOTHING) != QTI_NOT_EQUAL) {
                        if (debug_guard_entry) {
                            fprintf(stderr, "[GUARD-SKIP-ANALYSIS-SHOWS-OPTIONAL] Analysis shows expression type is optional\n");
                            fflush(stderr);
                        }
                        return false;  // Expression type allows NOTHING - no guard needed
                    }
                }
            }
        }
        if (LocalVar* local = expr ? getLocalVarFromValue(*expr) : nullptr) {
            bool result = parse_context ? parse_context->needsGuardForLocal(local) : true;
            if (debug_guard_entry) {
                fprintf(stderr, "[GUARD-RETURN-TRUE-LVAR2] needsGuardForLocal returned %d\n", result);
                fflush(stderr);
            }
            return result;
        }
        if (debug_guard_entry) {
            fprintf(stderr, "[GUARD-SKIP-CANNOT-PROVE-GUARD-NEEDED] Cannot prove guard is needed; skip to avoid deopt risk\n");
            fflush(stderr);
        }
        // Be conservative: if we can't prove a guard is needed (can't find LocalVar, can't determine
        // that target value is definitely assigned), don't insert a guard that might fail at runtime
        // and cause unnecessary IR->AST deopt causing side effect re-execution.
        return false;
    }
    // If the type is known and explicitly allows NOTHING, don't generate a speculative
    // guard based on profiling.  NOTHING is a valid value for such types, and a guard
    // deopt would re-execute the entire function, causing double side effects.
    if (type) {
        return false;
    }
    if (!expr) {
        return false;
    }
    return needsNotNothingGuard(*expr);
}

void QoreIRLowering::maybeInsertNotNothingGuard(QoreIRValue value, const QoreValue* expr,
        const QoreProgramLocation* loc, const QoreTypeInfo* target_type, bool allow_maybe_nothing) {
    static bool debug_guard = [] {
        const char* debug_env = getenv("QORE_IR_DEBUG");
        return debug_env && strstr(debug_env, "guard");
    }();

    if (!value.isValid() || !needsNotNothingGuard(expr, target_type, allow_maybe_nothing)) {
        return;
    }

    if (debug_guard) {
        fprintf(stderr, "[GUARD-INSERT] About to insert GuardNotNothing for value slot %d, expr=%s, target_type=%s\n",
                value.id, expr ? "present" : "nullptr", target_type ? "present" : "nullptr");
        fflush(stderr);
    }

    // Check expression's analysis for assignment state and never-nothing guarantees
    // Don't insert guard if: (1) expr never returns nothing, OR (2) target wasn't definitely assigned
    if (expr && expr->hasNode()) {
        const AbstractQoreNode* node = expr->getInternalNode();
        if (node && dynamic_cast<const ParseNode*>(node)) {
            QoreParseAnalysis expr_analysis;
            bool got_analysis = false;
            try {
                got_analysis = getAnalysis(*expr, expr_analysis);
            } catch (...) {
                got_analysis = false;
            }

            if (got_analysis) {
                static bool debug_guard_type = [] {
                    const char* debug_env = getenv("QORE_IR_DEBUG");
                    return debug_env && strstr(debug_env, "guard");
                }();

                // Note: These checks are handled in needsNotNothingGuard()
                // which returns bool, not here in maybeInsertNotNothingGuard() which is void
                // The actual logic is checked before calling this function
            }
        }
    }

    // Skip guard if the value is known to never be NOTHING (tracks across block boundaries)
    if (never_nothing_values.count(value.id)) {
        if (debug_guard) {
            fprintf(stderr, "[GUARD-SKIP-NEVER-NOTHING-TRACKED] Skipping guard - value known to never be nothing\n");
            fflush(stderr);
        }
        return;
    }
    // Find the instruction that produced this value and check its properties
    QoreIRBasicBlock* current_block = builder.getBlock();
    if (current_block && !current_block->instructions.empty()) {
        // Search for the instruction that produces this value
        QoreIRInstruction* producer = nullptr;
        for (auto it = current_block->instructions.rbegin(); it != current_block->instructions.rend(); ++it) {
            if ((*it)->result.id == value.id) {
                producer = it->get();
                break;
            }
        }

        if (!producer) {
            // Value wasn't produced in this block (maybe from a previous block or argument)
            // Don't insert a guard in this case to be safe
            if (debug_guard) {
                fprintf(stderr, "[GUARD-SKIP-NOT-PRODUCED] Value slot %d not produced in current block - skipping guard\n", value.id);
                fflush(stderr);
            }
            return;
        }

        if (debug_guard) {
            fprintf(stderr, "[GUARD-CHECK-PRODUCER] Value slot %d produced by opcode=%d\n",
                    value.id, static_cast<int>(producer->opcode));
            fflush(stderr);
        }

        // Skip duplicate guard on the same value
        if (producer->opcode == QoreIROpcode::GuardNotNothing
                && !producer->operands.empty() && producer->operands[0].id == value.id) {
            if (debug_guard) {
                fprintf(stderr, "[GUARD-SKIP-DUPLICATE] Skipping duplicate guard on value slot %d\n", value.id);
                fflush(stderr);
            }
            return;
        }

        // Skip guard if the value was produced by an opcode that never returns NOTHING
        if (opcodeNeverReturnsNothing(producer->opcode)) {
            if (debug_guard) {
                fprintf(stderr, "[GUARD-SKIP-NEVER-NOTHING] Skipping guard - opcode never returns nothing\n");
                fflush(stderr);
            }
            return;
        }

        // Skip guard if the value was produced by an operation that CAN return nothing
        // (e.g., function calls, hash member access, variable loads). Optional return types should be
        // allowed to return nothing without triggering IR->AST deopt.
        if (opcodeCanReturnNothing(producer->opcode)) {
            if (debug_guard) {
                fprintf(stderr, "[GUARD-SKIP-CAN-RETURN-NOTHING] Skipping guard on value slot %d from opcode that can return nothing (opcode=%d)\n", value.id, static_cast<int>(producer->opcode));
                fflush(stderr);
            }
            return;
        }

        if (debug_guard) {
            fprintf(stderr, "[GUARD-WILL-INSERT] Value slot %d produced by opcode=%d (can_return=%d, never_return=%d)\n",
                    value.id, static_cast<int>(producer->opcode), opcodeCanReturnNothing(producer->opcode),
                    opcodeNeverReturnsNothing(producer->opcode));
            fflush(stderr);
        }
    } else {
        if (debug_guard) {
            fprintf(stderr, "[GUARD-SKIP-NO-BLOCK] Value slot %d, no current block or empty instructions - NOT inserting guard\n", value.id);
            fflush(stderr);
        }
        // No current block or empty instructions - can't verify the opcode, so don't insert
        // a guard that might fail. This is conservative but correct.
        return;
    }
    const QoreProgramLocation* guard_loc = loc;
    if (!guard_loc && expr) {
        guard_loc = getExpressionLocation(*expr);
    }
    QoreIRBasicBlock* handler = getGuardExceptionTarget();
    if (!handler && !exception_stack.empty()) {
        handler = exception_stack.back();
    }
    builder.createGuardNotNothing(value, handler, guard_loc);
}

QoreIRBasicBlock* QoreIRLowering::getCurrentExceptionTarget() const {
    return exception_stack.empty() ? nullptr : exception_stack.back();
}

QoreIRBasicBlock* QoreIRLowering::getGuardExceptionTarget() const {
    return guard_exception_target_override ? guard_exception_target_override : getCurrentExceptionTarget();
}

void QoreIRLowering::setLoopCheckpointExceptionTarget(QoreIRInstruction* inst, QoreIRBasicBlock* target,
        QoreIRBasicBlock* handler) {
    if (inst && target && target->is_loop_header) {
        inst->exception_target = handler ? handler : getCurrentExceptionTarget();
    }
}

QoreIRLowering::GuardExceptionTargetOverrideScope::GuardExceptionTargetOverrideScope(QoreIRLowering& lowering,
        QoreIRBasicBlock* target)
        : lowering(lowering), previous(lowering.guard_exception_target_override) {
    if (target) {
        lowering.guard_exception_target_override = target;
    }
}

QoreIRLowering::GuardExceptionTargetOverrideScope::~GuardExceptionTargetOverrideScope() {
    lowering.guard_exception_target_override = previous;
}

bool QoreIRLowering::needsNotNothingGuard(const QoreValue& expr) const {
    static bool debug_guard = [] {
        const char* debug_env = getenv("QORE_IR_DEBUG");
        return debug_env && strstr(debug_env, "guard");
    }();

    if (debug_guard) {
        fprintf(stderr, "[GUARD-SINGLE-ARG] Entry: expr type=%s, has_node=%d, parse_context=%p\n",
                expr.getTypeName(), expr.hasNode(), (void*)parse_context);
        fflush(stderr);
    }

    if (parse_context) {
        LocalVar* local = getLocalVarFromValue(expr);
        if (debug_guard) {
            fprintf(stderr, "[GUARD-SINGLE-ARG-GETLVAR] getLocalVarFromValue returned %p\n", (void*)local);
            fflush(stderr);
        }
        if (local) {
            bool result = parse_context->needsGuardForLocal(local);
            if (debug_guard) {
                fprintf(stderr, "[GUARD-SINGLE-ARG-LVAR-RESULT] needsGuardForLocal returned %d\n", result);
                fflush(stderr);
            }
            return result;
        }
    }

    if (debug_guard) {
        fprintf(stderr, "[GUARD-SINGLE-ARG-NO-LVAR] No LocalVar found, attempting analysis\n");
        fflush(stderr);
    }

    QoreParseAnalysis analysis;
    bool got_analysis = false;
    try {
        got_analysis = getAnalysis(expr, analysis);
    } catch (...) {
        if (debug_guard) {
            fprintf(stderr, "[GUARD-SINGLE-ARG-EXCEPTION] Exception in getAnalysis, returning true\n");
            fflush(stderr);
        }
        return true;
    }

    if (got_analysis) {
        if (debug_guard) {
            fprintf(stderr, "[GUARD-SINGLE-ARG-ANALYSIS] Got analysis: KnownTypeInfo=%d, NeverNothing=%d, known_type=%p\n",
                    analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo),
                    analysis.hasFlag(QoreParseAnalysis::NeverNothing),
                    (void*)analysis.known_type);
            fflush(stderr);
        }

        if (analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
                && !analysis.hasFlag(QoreParseAnalysis::NeverNothing)) {
            if (analysis.known_type && QoreTypeInfo::parseReturns(analysis.known_type, NT_NOTHING) == QTI_NOT_EQUAL) {
                if (debug_guard) {
                    fprintf(stderr, "[GUARD-SINGLE-ARG-RETURNING-TRUE] Type is non-optional and not NeverNothing\n");
                    fflush(stderr);
                }
                return true;
            }
        }
    }

    if (parse_context) {
        LocalVar* local = getLocalVarFromValue(expr);
        if (debug_guard) {
            fprintf(stderr, "[GUARD-SINGLE-ARG-FINAL-LVAR] getLocalVarFromValue returned %p (2nd try)\n", (void*)local);
            fflush(stderr);
        }
        if (local) {
            bool result = parse_context->needsGuardForLocal(local);
            if (debug_guard) {
                fprintf(stderr, "[GUARD-SINGLE-ARG-FINAL-RESULT] needsGuardForLocal returned %d (2nd try)\n", result);
                fflush(stderr);
            }
            return result;
        }
    }

    if (debug_guard) {
        fprintf(stderr, "[GUARD-SINGLE-ARG-RETURNING-FALSE] Default case\n");
        fflush(stderr);
    }
    return false;
}

void QoreIRLowering::maybeInsertNotNothingGuard(QoreIRValue value, const QoreValue& expr) {
    maybeInsertNotNothingGuard(value, &expr, getExpressionLocation(expr), nullptr, false);
}

const QoreProgramLocation* QoreIRLowering::getExpressionLocation(const QoreValue& expr) const {
    if (!expr.hasNode()) {
        return nullptr;
    }
    auto* parse_node = dynamic_cast<const ParseNode*>(expr.getInternalNode());
    return parse_node ? parse_node->loc : nullptr;
}

QoreIRValue QoreIRLowering::lowerAssignment(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    // Check for weak assignment first since QoreWeakAssignmentOperatorNode inherits from
    // QoreAssignmentOperatorNode
    bool is_weak = dynamic_cast<const QoreWeakAssignmentOperatorNode*>(node) != nullptr;
    auto* assign = dynamic_cast<const QoreAssignmentOperatorNode*>(node);
    if (!assign) {
        return QoreIRValue();
    }
    if (getenv("QORE_AOT_TRACE_LOWER_ASSIGN")) {
        fprintf(stderr, "[aot-trace] lowerAssignment file=%s line=%d\n",
            assign->loc ? assign->loc->getFile() : "?",
            assign->loc ? assign->loc->start_line : 0);
        fflush(stderr);
    }

    // Range lvalue (e.g., list[0..2] = x) - delegate entire expression to AST before lowering RHS
    if (isRangeLValue(assign->getLeft())) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, assign->loc, error);
    }

    const AbstractQoreNode* left_node = assign->getLeft().getInternalNode();
    auto* left_var = dynamic_cast<const VarRefNode*>(left_node);
    QoreValue right_expr(assign->getRight());
    QoreIRValue right = lowerExpression(right_expr, error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    if (left_var && left_var->getType() == VT_IMMEDIATE) {
        left_var = nullptr;
    }
    if (left_var) {
        QoreIRValue store_result;
        if (!storeVarRef(left_var, right, error, "assignment", &right_expr, nullptr, is_weak,
                is_weak ? &store_result : nullptr)) {
            return QoreIRValue();
        }
        if (is_weak && store_result.isValid()) {
            return store_result;
        }
        if (assign->needsReturnValue()) {
            return loadVarRef(left_var, error, "assignment result", assign->getLeft());
        }
    } else if (assign->getLeft().hasNode()) {
        // Fast path: const-key hash subscript → LoadLocal + HashKeyStore (no EXPR_TREE)
        const VarRefNode* container_var = nullptr;
        std::string key_name;
        QoreValue key_expr;
        if (!is_weak && isConstKeyHashSubscript(assign->getLeft(), container_var, key_name, key_expr)) {
            return emitHashKeyDirectStore(container_var, key_name, key_expr, right, expr,
                assign->loc, error);
        }
        // Fast path: dynamic-key hash subscript → LoadLocal + HashKeyStoreDynamic (no EXPR_TREE)
        if (!is_weak && isDynamicKeyHashSubscript(assign->getLeft(), container_var, key_expr)) {
            return emitHashKeyDynamicStore(container_var, key_expr, right, expr,
                assign->loc, error);
        }
        // Fast path: const-index list subscript → LoadLocal + ListIndexStore (no EXPR_TREE)
        QoreValue index_expr;
        if (!is_weak && isConstIndexListSubscript(assign->getLeft(), container_var, index_expr)) {
            return emitListIndexDirectStore(container_var, index_expr, right, expr,
                assign->loc, error);
        }
        // LValuePath: structured path for all remaining lvalue cases (reference vars,
        // object vars, non-VarRef containers, nested member access, etc.)
        {
            std::vector<LVPathStep> lv_path;
            std::vector<QoreValue> dynamic_operands;
            if (extractLValuePath(assign->getLeft(), lv_path, dynamic_operands)
                    && !lv_path.empty()
                    ) {
                // Lower dynamic key/index operands
                std::vector<QoreIRValue> dyn_vals;
                for (auto& dop : dynamic_operands) {
                    QoreIRValue dv = lowerExpression(dop, error);
                    if (!dv.isValid()) {
                        return QoreIRValue();
                    }
                    dyn_vals.push_back(dv);
                }
                // Assign operand indices to dynamic path steps
                uint32_t dyn_idx = 0;
                for (auto& step : lv_path) {
                    if ((step.kind == LVPathStepKind::HashKey || step.kind == LVPathStepKind::ListIndex)
                            && step.operand_idx == UINT32_MAX) {
                        if (dyn_idx < dyn_vals.size()) {
                            step.operand_idx = dyn_vals[dyn_idx].id;
                            ++dyn_idx;
                        }
                    }
                }
                // Create LValuePathAssign instruction (no result value — assignment
                auto* path_inst = builder.getBlock()->appendInstruction<QoreIRLValuePathInstruction>(
                    QoreIROpcode::LValuePathAssign);
                path_inst->result = builder.getFunction()->createValue();
                path_inst->path = std::move(lv_path);
                path_inst->weak = is_weak;
                path_inst->loc = assign->loc;
                path_inst->operands.push_back(right);
                // Add dynamic operands so they're tracked for cleanup
                for (auto& dv : dyn_vals) {
                    path_inst->operands.push_back(dv);
                }
                return path_inst->result;
            }
        }
        if (!guardLValueBase(assign->getLeft(), error)) {
            return QoreIRValue();
        }
        // StoreLValue delegates lvalue evaluation to AST at runtime;
        // track as AST delegation so map/select body emits PushImplicitArg
        // when the lvalue contains $1/$# references (e.g., hash{$1} = val)
        ++ast_delegate_count;
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {right}, normal_block, handler, assign->loc);
            inst->invoke_opcode = QoreIROpcode::StoreLValue;
            inst->weak = is_weak;
            builder.setBlock(normal_block);
            if (is_weak) {
                return inst->result;
            }
        } else {
            auto* store_inst = builder.createStoreLValue(assign->getLeft(), right, assign->loc, is_weak);
            if (is_weak) {
                store_inst->result = builder.getFunction()->createValue();
                return store_inst->result;
            }
        }
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
    // Range lvalue (e.g., list[0..2] += x) - delegate entire expression to AST before lowering RHS
    if (isRangeLValue(op->getLeft())) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, op->loc, error);
    }

    bool force_int = dynamic_cast<const QoreIntPlusEqualsOperatorNode*>(node) != nullptr;
    const AbstractQoreNode* left_node = op->getLeft().getInternalNode();
    auto* left_var = dynamic_cast<const VarRefNode*>(left_node);
    // Force lvalue path for types where load-compute-store produces wrong types or O(n) copies:
    // - Objects: object + hash = hash (not object), needs in-place member merge
    // - Lists: list + element copies the entire list, making loops O(n^2)
    // - Hashes: hash + hash copies the entire hash
    // - Binary: binary + string = string (not binary), needs in-place append
    // Using AddAssignLValue goes through QorePlusEqualsOperatorNode which does
    // proper in-place modification via lvalue semantics.
    if (left_var && left_var->getTypeInfo()) {
        const QoreTypeInfo* ti = left_var->getTypeInfo();
        if (QoreTypeInfo::getUniqueReturnClass(ti) != nullptr
                || QoreTypeInfo::isListType(ti)
                || QoreTypeInfo::isHashType(ti)
                || QoreTypeInfo::getBaseType(ti) == NT_BINARY
                || QoreTypeInfo::getBaseType(ti) == NT_FLOAT
                || QoreTypeInfo::getBaseType(ti) == NT_NUMBER
                || QoreTypeInfo::getBaseType(ti) == NT_DATE
                || QoreTypeInfo::isReference(ti)) {
            left_var = nullptr;  // Force lvalue path
        }
    }
    QoreValue right_expr(op->getRight());

    // Fused local int operations: emit single instruction instead of LoadLocal+op+StoreLocal.
    // Exclude reference-typed locals: they need lvalue semantics to write through to the
    // target variable. Also exclude closure-use variables: they are instantiated on cvstack,
    // not lvstack, so they need the fallback path.
    if (force_int && isLocalNonClosureVar(left_var)
            && !QoreTypeInfo::isReference(left_var->getTypeInfo())) {
        const AbstractQoreNode* right_node = right_expr.getInternalNode();
        auto* right_var = dynamic_cast<const VarRefNode*>(right_node);
        if (isLocalNonClosureVar(right_var)) {
            // target += source (both typed int locals) → AddAssignLocalInt
            auto* inst = builder.createAddAssignLocalInt(
                left_var->ref.id, right_var->ref.id, op->loc);
            if (parse_context) {
                parse_context->markLocalAssignment(left_var->ref.id, true,
                    left_var->getTypeInfo());
            }
            return inst->result;
        }
        if (right_expr.getType() == NT_INT) {
            // local += const → IncrementLocalInt
            auto* inst = builder.createIncrementLocalInt(
                left_var->ref.id, right_expr.getAsBigInt(), op->loc);
            if (parse_context) {
                parse_context->markLocalAssignment(left_var->ref.id, true,
                    left_var->getTypeInfo());
            }
            return inst->result;
        }
    }

    QoreIRValue right = lowerExpression(right_expr, error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    if (left_var && left_var->getType() == VT_IMMEDIATE) {
        left_var = nullptr;
    }
    if (!left_var) {
        // Prefer true lvalue semantics for member/subscript compound assignments.
        // The hash/list load-compute-store fast paths cannot preserve hashdecl
        // member types (ex: list<auto> fields) or in-place container semantics.
        {
            QoreIRValue path_result = tryEmitLValuePathOp(QoreIROpcode::LValuePathCompound,
                op->getLeft(), &right, op->loc, error, false, LVCompoundOp::AddAssign);
            if (path_result.isValid()) {
                return path_result;
            }
        }

        // Fast path: constant-key hash subscript compound assignment
        const VarRefNode* container_var = nullptr;
        std::string key_name;
        QoreValue key_expr;
        if (isConstKeyHashSubscript(op->getLeft(), container_var, key_name, key_expr)) {
            QoreIROpcode arith_op = force_int ? QoreIROpcode::AddAssignInt : QoreIROpcode::AddAssignAny;
            return emitHashKeyCompoundOp(container_var, key_name, key_expr,
                arith_op, right, expr, op->loc, error);
        }
        if (isDynamicKeyHashSubscript(op->getLeft(), container_var, key_expr)) {
            QoreIROpcode arith_op = force_int ? QoreIROpcode::AddAssignInt : QoreIROpcode::AddAssignAny;
            return emitHashKeyDynamicCompoundOp(container_var, key_expr,
                arith_op, right, expr, op->loc, error);
        }

        // Fast path: list index subscript compound assignment
        QoreValue index_expr;
        if (isConstIndexListSubscript(op->getLeft(), container_var, index_expr)) {
            QoreIROpcode arith_op = force_int ? QoreIROpcode::AddAssignInt : QoreIROpcode::AddAssignAny;
            return emitListKeyCompoundOp(container_var, index_expr,
                arith_op, right, expr, op->loc, error);
        }

        if (!op->getLeft().hasNode()) {
            error = "unsupported lvalue for plus-equals IR lowering";
            return QoreIRValue();
        }
        if (!guardLValueBase(op->getLeft(), error)) {
            return QoreIRValue();
        }
        // lvalue evaluation delegates to AST at runtime; track for implicit arg push
        ++ast_delegate_count;
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {right}, normal_block, handler, op->loc);
            inst->invoke_opcode = QoreIROpcode::AddAssignLValue;
            builder.setBlock(normal_block);
            return inst->result;
        }
        return builder.createLValueBinaryOp(QoreIROpcode::AddAssignLValue, op->getLeft(), right, op->loc)->result;
    }
    QoreIRValue left_value = loadVarRef(left_var, error, "plus-equals", op->getLeft());
    if (!left_value.isValid()) {
        return QoreIRValue();
    }

    // Generic NOTHING→default coercion for typed variables: if the variable's type has
    // a non-NOTHING default value (e.g., date→ZeroDate, hash→{}, list→()), emit a
    // conditional store of the default value before arithmetic.  This matches the AST
    // PlusEquals semantics (QorePlusEqualsOperatorNode::evalImpl lines 155-187) via
    // proper IR lowering.
    const QoreTypeInfo* lvar_ti = left_var->getTypeInfo();
    bool left_local_known_assigned = parse_context
        && left_var->getType() == VT_LOCAL
        && left_var->ref.id
        && parse_context->isLocalDefinitelyAssigned(left_var->ref.id)
        && lvar_ti
        && QoreTypeInfo::parseReturns(lvar_ti, NT_NOTHING) == QTI_NOT_EQUAL;
    if (lvar_ti && QoreTypeInfo::hasDefaultValue(lvar_ti) && !left_local_known_assigned) {
        QoreIRBasicBlock* has_value_bb = createBlock("pe.has_value");
        QoreIRBasicBlock* nothing_bb = createBlock("pe.nothing");
        QoreIRBasicBlock* merge_bb = createBlock("pe.merge");
        if (!has_value_bb || !nothing_bb || !merge_bb) {
            error = "IR builder failed to create blocks for NOTHING→default coercion";
            return QoreIRValue();
        }
        // Use EqHard comparison with NOTHING constant (not BrIf, which uses getAsBool
        // and would treat 0, "", false as NOTHING)
        QoreIRValue nothing_val = builder.createConstNothing(op->loc)->result;
        QoreIRValue is_nothing = builder.createBinaryOp(QoreIROpcode::EqHard,
            left_value, nothing_val, op->loc)->result;
        builder.createBranchIf(is_nothing, nothing_bb, has_value_bb);

        // Nothing path: store the type's default value and branch to merge
        builder.setBlock(nothing_bb);
        QoreValue default_val = QoreTypeInfo::getDefaultQoreValue(lvar_ti);
        QoreIRValue default_ir = lowerConstant(default_val, error);
        if (!default_ir.isValid()) {
            default_val.discard(nullptr);
            return QoreIRValue();
        }
        if (!storeVarRef(left_var, default_ir, error, "pe.default-init", nullptr)) {
            return QoreIRValue();
        }
        builder.createBranch(merge_bb);

        // Has-value path: just branch to merge
        builder.setBlock(has_value_bb);
        builder.createBranch(merge_bb);

        // Merge: reload the variable (now has either original or default value)
        builder.setBlock(merge_bb);
        left_value = loadVarRef(left_var, error, "pe.reload", op->getLeft());
        if (!left_value.isValid()) {
            return QoreIRValue();
        }
    }
    QoreIROpcode opcode = QoreIROpcode::AddAssignAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if (force_int
        || (isIntConstant(op->getLeft()) && isIntConstant(right_expr))
        || (getAnalysis(op->getLeft(), left_analysis)
            && getAnalysis(right_expr, right_analysis)
            && isNeverNothingInt(left_analysis)
            && isNeverNothingInt(right_analysis))) {
        opcode = QoreIROpcode::AddAssignInt;
    } else if ((isFloatConstant(op->getLeft()) && isFloatConstant(right_expr))
        || (getAnalysis(op->getLeft(), left_analysis)
            && getAnalysis(right_expr, right_analysis)
            && isNeverNothingFloat(left_analysis)
            && isNeverNothingFloat(right_analysis))) {
        opcode = QoreIROpcode::AddAssignFloat;
    }
    QoreIRValue result = lowerBinaryOpOrInvoke(opcode, expr, left_value, right, op->loc, error);
    if (!result.isValid()) {
        return QoreIRValue();
    }
    if (!storeVarRef(left_var, result, error, "plus-equals", &right_expr)) {
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
    // Range lvalue (e.g., list[0..2] -= x) - delegate entire expression to AST before lowering RHS
    if (isRangeLValue(op->getLeft())) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, op->loc, error);
    }

    bool force_int = dynamic_cast<const QoreIntMinusEqualsOperatorNode*>(node) != nullptr;
    const AbstractQoreNode* left_node = op->getLeft().getInternalNode();
    auto* left_var = dynamic_cast<const VarRefNode*>(left_node);
    // Force lvalue path for types where load-compute-store produces wrong result types:
    // - float/number: NOTHING - int = int (needs coercion to float/number)
    // - References: need lvalue semantics for write-through
    // - Hash/Object: -= has special semantics (key removal), not arithmetic
    if (left_var && left_var->getTypeInfo()) {
        const QoreTypeInfo* ti = left_var->getTypeInfo();
        if (QoreTypeInfo::getBaseType(ti) == NT_FLOAT
                || QoreTypeInfo::getBaseType(ti) == NT_NUMBER
                || QoreTypeInfo::getBaseType(ti) == NT_DATE
                || QoreTypeInfo::isReference(ti)
                || QoreTypeInfo::isHashType(ti)
                || QoreTypeInfo::getBaseType(ti) == NT_OBJECT) {
            left_var = nullptr;
        }
    }
    QoreValue right_expr(op->getRight());
    QoreIRValue right = lowerExpression(right_expr, error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    if (left_var && left_var->getType() == VT_IMMEDIATE) {
        left_var = nullptr;
    }
    if (!left_var) {
        // Prefer true lvalue semantics for member/subscript compound assignments.
        {
            QoreIRValue path_result = tryEmitLValuePathOp(QoreIROpcode::LValuePathCompound,
                op->getLeft(), &right, op->loc, error, false, LVCompoundOp::SubAssign);
            if (path_result.isValid()) {
                return path_result;
            }
        }

        // Fast path: constant-key hash subscript compound assignment
        const VarRefNode* container_var = nullptr;
        std::string key_name;
        QoreValue key_expr;
        if (isConstKeyHashSubscript(op->getLeft(), container_var, key_name, key_expr)) {
            QoreIROpcode arith_op = force_int ? QoreIROpcode::SubAssignInt : QoreIROpcode::SubAssignAny;
            return emitHashKeyCompoundOp(container_var, key_name, key_expr,
                arith_op, right, expr, op->loc, error);
        }
        if (isDynamicKeyHashSubscript(op->getLeft(), container_var, key_expr)) {
            QoreIROpcode arith_op = force_int ? QoreIROpcode::SubAssignInt : QoreIROpcode::SubAssignAny;
            return emitHashKeyDynamicCompoundOp(container_var, key_expr,
                arith_op, right, expr, op->loc, error);
        }

        // Fast path: list index subscript compound assignment
        QoreValue index_expr;
        if (isConstIndexListSubscript(op->getLeft(), container_var, index_expr)) {
            QoreIROpcode arith_op = force_int ? QoreIROpcode::SubAssignInt : QoreIROpcode::SubAssignAny;
            return emitListKeyCompoundOp(container_var, index_expr,
                arith_op, right, expr, op->loc, error);
        }

        if (!op->getLeft().hasNode()) {
            error = "unsupported lvalue for minus-equals IR lowering";
            return QoreIRValue();
        }
        if (!guardLValueBase(op->getLeft(), error)) {
            return QoreIRValue();
        }
        // lvalue evaluation delegates to AST at runtime; track for implicit arg push
        ++ast_delegate_count;
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {right}, normal_block, handler, op->loc);
            inst->invoke_opcode = QoreIROpcode::SubAssignLValue;
            builder.setBlock(normal_block);
            return inst->result;
        }
        return builder.createLValueBinaryOp(QoreIROpcode::SubAssignLValue, op->getLeft(), right, op->loc)->result;
    }
    QoreIRValue left_value = loadVarRef(left_var, error, "minus-equals", op->getLeft());
    if (!left_value.isValid()) {
        return QoreIRValue();
    }

    // Generic NOTHING→default coercion for typed variables (see lowerPlusEquals)
    const QoreTypeInfo* lvar_ti = left_var->getTypeInfo();
    bool left_local_known_assigned = parse_context
        && left_var->getType() == VT_LOCAL
        && left_var->ref.id
        && parse_context->isLocalDefinitelyAssigned(left_var->ref.id)
        && lvar_ti
        && QoreTypeInfo::parseReturns(lvar_ti, NT_NOTHING) == QTI_NOT_EQUAL;
    if (lvar_ti && QoreTypeInfo::hasDefaultValue(lvar_ti) && !left_local_known_assigned) {
        QoreIRBasicBlock* has_value_bb = createBlock("me.has_value");
        QoreIRBasicBlock* nothing_bb = createBlock("me.nothing");
        QoreIRBasicBlock* merge_bb = createBlock("me.merge");
        if (!has_value_bb || !nothing_bb || !merge_bb) {
            error = "IR builder failed to create blocks for NOTHING→default coercion";
            return QoreIRValue();
        }
        QoreIRValue nothing_val = builder.createConstNothing(op->loc)->result;
        QoreIRValue is_nothing = builder.createBinaryOp(QoreIROpcode::EqHard,
            left_value, nothing_val, op->loc)->result;
        builder.createBranchIf(is_nothing, nothing_bb, has_value_bb);

        builder.setBlock(nothing_bb);
        QoreValue default_val = QoreTypeInfo::getDefaultQoreValue(lvar_ti);
        QoreIRValue default_ir = lowerConstant(default_val, error);
        if (!default_ir.isValid()) {
            default_val.discard(nullptr);
            return QoreIRValue();
        }
        if (!storeVarRef(left_var, default_ir, error, "me.default-init", nullptr)) {
            return QoreIRValue();
        }
        builder.createBranch(merge_bb);

        builder.setBlock(has_value_bb);
        builder.createBranch(merge_bb);

        builder.setBlock(merge_bb);
        left_value = loadVarRef(left_var, error, "me.reload", op->getLeft());
        if (!left_value.isValid()) {
            return QoreIRValue();
        }
    }

    QoreIROpcode opcode = QoreIROpcode::SubAssignAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if (force_int
        || (isIntConstant(op->getLeft()) && isIntConstant(right_expr))
        || (getAnalysis(op->getLeft(), left_analysis)
            && getAnalysis(right_expr, right_analysis)
            && isNeverNothingInt(left_analysis)
            && isNeverNothingInt(right_analysis))) {
        opcode = QoreIROpcode::SubAssignInt;
    } else if ((isFloatConstant(op->getLeft()) && isFloatConstant(right_expr))
        || (getAnalysis(op->getLeft(), left_analysis)
            && getAnalysis(right_expr, right_analysis)
            && isNeverNothingFloat(left_analysis)
            && isNeverNothingFloat(right_analysis))) {
        opcode = QoreIROpcode::SubAssignFloat;
    }
    QoreIRValue result = lowerBinaryOpOrInvoke(opcode, expr, left_value, right, op->loc, error);
    if (!result.isValid()) {
        return QoreIRValue();
    }
    if (!storeVarRef(left_var, result, error, "minus-equals", &right_expr)) {
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
    // Range lvalue (e.g., list[0..2] *= x) - delegate entire expression to AST before lowering RHS
    if (isRangeLValue(op->getLeft())) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, op->loc, error);
    }

    const AbstractQoreNode* left_node = op->getLeft().getInternalNode();
    auto* left_var = dynamic_cast<const VarRefNode*>(left_node);
    // Force lvalue path for types where load-compute-store produces wrong result types
    if (left_var && left_var->getTypeInfo()) {
        const QoreTypeInfo* ti = left_var->getTypeInfo();
        if (QoreTypeInfo::getBaseType(ti) == NT_FLOAT
                || QoreTypeInfo::getBaseType(ti) == NT_NUMBER
                || QoreTypeInfo::isReference(ti)) {
            left_var = nullptr;
        }
    }
    QoreValue right_expr(op->getRight());
    QoreIRValue right = lowerExpression(right_expr, error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    if (left_var && left_var->getType() == VT_IMMEDIATE) {
        left_var = nullptr;
    }
    if (!left_var) {
        // Prefer true lvalue semantics for member/subscript compound assignments.
        {
            QoreIRValue path_result = tryEmitLValuePathOp(QoreIROpcode::LValuePathCompound,
                op->getLeft(), &right, op->loc, error, false, LVCompoundOp::MulAssign);
            if (path_result.isValid()) {
                return path_result;
            }
        }

        // Fast path: constant-key hash subscript compound assignment
        const VarRefNode* container_var = nullptr;
        std::string key_name;
        QoreValue key_expr;
        if (isConstKeyHashSubscript(op->getLeft(), container_var, key_name, key_expr)) {
            return emitHashKeyCompoundOp(container_var, key_name, key_expr,
                QoreIROpcode::MulAssignAny, right, expr, op->loc, error);
        }
        if (isDynamicKeyHashSubscript(op->getLeft(), container_var, key_expr)) {
            return emitHashKeyDynamicCompoundOp(container_var, key_expr,
                QoreIROpcode::MulAssignAny, right, expr, op->loc, error);
        }

        // Fast path: list index subscript compound assignment
        QoreValue index_expr;
        if (isConstIndexListSubscript(op->getLeft(), container_var, index_expr)) {
            return emitListKeyCompoundOp(container_var, index_expr,
                QoreIROpcode::MulAssignAny, right, expr, op->loc, error);
        }

        if (!op->getLeft().hasNode()) {
            error = "unsupported lvalue for multiply-equals IR lowering";
            return QoreIRValue();
        }
        if (!guardLValueBase(op->getLeft(), error)) {
            return QoreIRValue();
        }
        // lvalue evaluation delegates to AST at runtime; track for implicit arg push
        ++ast_delegate_count;
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {right}, normal_block, handler, op->loc);
            inst->invoke_opcode = QoreIROpcode::MulAssignLValue;
            builder.setBlock(normal_block);
            return inst->result;
        }
        return builder.createLValueBinaryOp(QoreIROpcode::MulAssignLValue, op->getLeft(), right, op->loc)->result;
    }
    QoreIRValue left_value = loadVarRef(left_var, error, "multiply-equals", op->getLeft());
    if (!left_value.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = QoreIROpcode::MulAssignAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if ((isIntConstant(op->getLeft()) && isIntConstant(right_expr))
        || (getAnalysis(op->getLeft(), left_analysis)
            && getAnalysis(right_expr, right_analysis)
            && isNeverNothingInt(left_analysis)
            && isNeverNothingInt(right_analysis))) {
        opcode = QoreIROpcode::MulAssignInt;
    } else if ((isFloatConstant(op->getLeft()) && isFloatConstant(right_expr))
        || (getAnalysis(op->getLeft(), left_analysis)
            && getAnalysis(right_expr, right_analysis)
            && isNeverNothingFloat(left_analysis)
            && isNeverNothingFloat(right_analysis))) {
        opcode = QoreIROpcode::MulAssignFloat;
    }
    QoreIRValue result = lowerBinaryOpOrInvoke(opcode, expr, left_value, right, op->loc, error);
    if (!result.isValid()) {
        return QoreIRValue();
    }
    if (!storeVarRef(left_var, result, error, "multiply-equals", &right_expr)) {
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
    // Range lvalue (e.g., list[0..2] /= x) - delegate entire expression to AST before lowering RHS
    if (isRangeLValue(op->getLeft())) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, op->loc, error);
    }

    const AbstractQoreNode* left_node = op->getLeft().getInternalNode();
    auto* left_var = dynamic_cast<const VarRefNode*>(left_node);
    // Force lvalue path for types where load-compute-store produces wrong result types
    if (left_var && left_var->getTypeInfo()) {
        const QoreTypeInfo* ti = left_var->getTypeInfo();
        if (QoreTypeInfo::getBaseType(ti) == NT_FLOAT
                || QoreTypeInfo::getBaseType(ti) == NT_NUMBER
                || QoreTypeInfo::isReference(ti)) {
            left_var = nullptr;
        }
    }
    QoreValue right_expr(op->getRight());
    QoreIRValue right = lowerExpression(right_expr, error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    if (left_var && left_var->getType() == VT_IMMEDIATE) {
        left_var = nullptr;
    }
    if (!left_var) {
        // Prefer true lvalue semantics for member/subscript compound assignments.
        {
            QoreIRValue path_result = tryEmitLValuePathOp(QoreIROpcode::LValuePathCompound,
                op->getLeft(), &right, op->loc, error, false, LVCompoundOp::DivAssign);
            if (path_result.isValid()) {
                return path_result;
            }
        }

        // Fast path: constant-key hash subscript compound assignment
        const VarRefNode* container_var = nullptr;
        std::string key_name;
        QoreValue key_expr;
        if (isConstKeyHashSubscript(op->getLeft(), container_var, key_name, key_expr)) {
            return emitHashKeyCompoundOp(container_var, key_name, key_expr,
                QoreIROpcode::DivAssignAny, right, expr, op->loc, error);
        }
        if (isDynamicKeyHashSubscript(op->getLeft(), container_var, key_expr)) {
            return emitHashKeyDynamicCompoundOp(container_var, key_expr,
                QoreIROpcode::DivAssignAny, right, expr, op->loc, error);
        }

        // Fast path: list index subscript compound assignment
        QoreValue index_expr;
        if (isConstIndexListSubscript(op->getLeft(), container_var, index_expr)) {
            return emitListKeyCompoundOp(container_var, index_expr,
                QoreIROpcode::DivAssignAny, right, expr, op->loc, error);
        }

        if (!op->getLeft().hasNode()) {
            error = "unsupported lvalue for divide-equals IR lowering";
            return QoreIRValue();
        }
        if (!guardLValueBase(op->getLeft(), error)) {
            return QoreIRValue();
        }
        // lvalue evaluation delegates to AST at runtime; track for implicit arg push
        ++ast_delegate_count;
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {right}, normal_block, handler, op->loc);
            inst->invoke_opcode = QoreIROpcode::DivAssignLValue;
            builder.setBlock(normal_block);
            return inst->result;
        }
        return builder.createLValueBinaryOp(QoreIROpcode::DivAssignLValue, op->getLeft(), right, op->loc)->result;
    }
    QoreIRValue left_value = loadVarRef(left_var, error, "divide-equals", op->getLeft());
    if (!left_value.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = QoreIROpcode::DivAssignAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if ((isIntConstant(op->getLeft()) && isIntConstant(right_expr))
        || (getAnalysis(op->getLeft(), left_analysis)
            && getAnalysis(right_expr, right_analysis)
            && isNeverNothingInt(left_analysis)
            && isNeverNothingInt(right_analysis))) {
        opcode = QoreIROpcode::DivAssignInt;
    } else if ((isFloatConstant(op->getLeft()) && isFloatConstant(right_expr))
        || (getAnalysis(op->getLeft(), left_analysis)
            && getAnalysis(right_expr, right_analysis)
            && isNeverNothingFloat(left_analysis)
            && isNeverNothingFloat(right_analysis))) {
        opcode = QoreIROpcode::DivAssignFloat;
    }
    QoreIRValue result = lowerBinaryOpOrInvoke(opcode, expr, left_value, right, op->loc, error);
    if (!result.isValid()) {
        return QoreIRValue();
    }
    if (!storeVarRef(left_var, result, error, "divide-equals", &right_expr)) {
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
    // Range lvalue (e.g., list[0..2] %= x) - delegate entire expression to AST before lowering RHS
    if (isRangeLValue(op->getLeft())) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, op->loc, error);
    }

    bool force_int = dynamic_cast<const QoreBinaryIntLValueOperatorNode*>(node) != nullptr;
    const AbstractQoreNode* left_node = op->getLeft().getInternalNode();
    auto* left_var = dynamic_cast<const VarRefNode*>(left_node);
    // Reference-typed locals need lvalue semantics for write-through
    if (left_var && left_var->getTypeInfo()
            && QoreTypeInfo::isReference(left_var->getTypeInfo())) {
        left_var = nullptr;
    }
    QoreValue right_expr(op->getRight());
    QoreIRValue right = lowerExpression(right_expr, error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    if (left_var && left_var->getType() == VT_IMMEDIATE) {
        left_var = nullptr;
    }
    if (!left_var) {
        // Path-based compound assignment for complex lvalues
        {
            QoreIRValue path_result = tryEmitLValuePathOp(QoreIROpcode::LValuePathCompound,
                op->getLeft(), &right, op->loc, error, false, LVCompoundOp::ModAssign);
            if (path_result.isValid()) {
                return path_result;
            }
        }

        if (!op->getLeft().hasNode()) {
            error = "unsupported lvalue for modulo-equals IR lowering";
            return QoreIRValue();
        }
        if (!guardLValueBase(op->getLeft(), error)) {
            return QoreIRValue();
        }
        // lvalue evaluation delegates to AST at runtime; track for implicit arg push
        ++ast_delegate_count;
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {right}, normal_block, handler, op->loc);
            inst->invoke_opcode = QoreIROpcode::ModAssignLValue;
            builder.setBlock(normal_block);
            return inst->result;
        }
        return builder.createLValueBinaryOp(QoreIROpcode::ModAssignLValue, op->getLeft(), right, op->loc)->result;
    }
    QoreIRValue left_value = loadVarRef(left_var, error, "modulo-equals", op->getLeft());
    if (!left_value.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = QoreIROpcode::ModAssignAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if (force_int
        || (isIntConstant(op->getLeft()) && isIntConstant(right_expr))
        || (getAnalysis(op->getLeft(), left_analysis)
            && getAnalysis(right_expr, right_analysis)
            && isNeverNothingInt(left_analysis)
            && isNeverNothingInt(right_analysis))) {
        opcode = QoreIROpcode::ModAssignInt;
    }
    QoreIRValue result = lowerBinaryOpOrInvoke(opcode, expr, left_value, right, op->loc, error);
    if (!result.isValid()) {
        return QoreIRValue();
    }
    if (!storeVarRef(left_var, result, error, "modulo-equals", &right_expr)) {
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
    // Range lvalue (e.g., list[0..2] &= x) - delegate entire expression to AST before lowering RHS
    if (isRangeLValue(op->getLeft())) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, op->loc, error);
    }

    bool force_int = dynamic_cast<const QoreBinaryIntLValueOperatorNode*>(node) != nullptr;
    const AbstractQoreNode* left_node = op->getLeft().getInternalNode();
    auto* left_var = dynamic_cast<const VarRefNode*>(left_node);
    // Reference-typed locals need lvalue semantics for write-through
    if (left_var && left_var->getTypeInfo()
            && QoreTypeInfo::isReference(left_var->getTypeInfo())) {
        left_var = nullptr;
    }
    QoreValue right_expr(op->getRight());
    QoreIRValue right = lowerExpression(right_expr, error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    if (left_var && left_var->getType() == VT_IMMEDIATE) {
        left_var = nullptr;
    }
    if (!left_var) {
        // Path-based compound assignment for complex lvalues
        {
            QoreIRValue path_result = tryEmitLValuePathOp(QoreIROpcode::LValuePathCompound,
                op->getLeft(), &right, op->loc, error, false, LVCompoundOp::AndAssign);
            if (path_result.isValid()) {
                return path_result;
            }
        }

        if (!op->getLeft().hasNode()) {
            error = "unsupported lvalue for and-equals IR lowering";
            return QoreIRValue();
        }
        if (!guardLValueBase(op->getLeft(), error)) {
            return QoreIRValue();
        }
        // lvalue evaluation delegates to AST at runtime; track for implicit arg push
        ++ast_delegate_count;
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {right}, normal_block, handler, op->loc);
            inst->invoke_opcode = QoreIROpcode::AndAssignLValue;
            builder.setBlock(normal_block);
            return inst->result;
        }
        return builder.createLValueBinaryOp(QoreIROpcode::AndAssignLValue, op->getLeft(), right, op->loc)->result;
    }
    QoreIRValue left_value = loadVarRef(left_var, error, "and-equals", op->getLeft());
    if (!left_value.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = QoreIROpcode::AndAssignAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if (force_int
        || (isIntConstant(op->getLeft()) && isIntConstant(right_expr))
        || (getAnalysis(op->getLeft(), left_analysis)
            && getAnalysis(right_expr, right_analysis)
            && isNeverNothingInt(left_analysis)
            && isNeverNothingInt(right_analysis))) {
        opcode = QoreIROpcode::AndAssignInt;
    }
    QoreIRValue result = lowerBinaryOpOrInvoke(opcode, expr, left_value, right, op->loc, error);
    if (!result.isValid()) {
        return QoreIRValue();
    }
    if (!storeVarRef(left_var, result, error, "and-equals", &right_expr)) {
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
    // Range lvalue (e.g., list[0..2] |= x) - delegate entire expression to AST before lowering RHS
    if (isRangeLValue(op->getLeft())) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, op->loc, error);
    }

    bool force_int = dynamic_cast<const QoreBinaryIntLValueOperatorNode*>(node) != nullptr;
    const AbstractQoreNode* left_node = op->getLeft().getInternalNode();
    auto* left_var = dynamic_cast<const VarRefNode*>(left_node);
    // Reference-typed locals need lvalue semantics for write-through
    if (left_var && left_var->getTypeInfo()
            && QoreTypeInfo::isReference(left_var->getTypeInfo())) {
        left_var = nullptr;
    }
    QoreValue right_expr(op->getRight());
    QoreIRValue right = lowerExpression(right_expr, error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    if (left_var && left_var->getType() == VT_IMMEDIATE) {
        left_var = nullptr;
    }
    if (!left_var) {
        // Path-based compound assignment for complex lvalues
        {
            QoreIRValue path_result = tryEmitLValuePathOp(QoreIROpcode::LValuePathCompound,
                op->getLeft(), &right, op->loc, error, false, LVCompoundOp::OrAssign);
            if (path_result.isValid()) {
                return path_result;
            }
        }

        if (!op->getLeft().hasNode()) {
            error = "unsupported lvalue for or-equals IR lowering";
            return QoreIRValue();
        }
        if (!guardLValueBase(op->getLeft(), error)) {
            return QoreIRValue();
        }
        // lvalue evaluation delegates to AST at runtime; track for implicit arg push
        ++ast_delegate_count;
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {right}, normal_block, handler, op->loc);
            inst->invoke_opcode = QoreIROpcode::OrAssignLValue;
            builder.setBlock(normal_block);
            return inst->result;
        }
        return builder.createLValueBinaryOp(QoreIROpcode::OrAssignLValue, op->getLeft(), right, op->loc)->result;
    }
    QoreIRValue left_value = loadVarRef(left_var, error, "or-equals", op->getLeft());
    if (!left_value.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = QoreIROpcode::OrAssignAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if (force_int
        || (isIntConstant(op->getLeft()) && isIntConstant(right_expr))
        || (getAnalysis(op->getLeft(), left_analysis)
            && getAnalysis(right_expr, right_analysis)
            && isNeverNothingInt(left_analysis)
            && isNeverNothingInt(right_analysis))) {
        opcode = QoreIROpcode::OrAssignInt;
    }
    QoreIRValue result = lowerBinaryOpOrInvoke(opcode, expr, left_value, right, op->loc, error);
    if (!result.isValid()) {
        return QoreIRValue();
    }
    if (!storeVarRef(left_var, result, error, "or-equals", &right_expr)) {
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
    // Range lvalue (e.g., list[0..2] ^= x) - delegate entire expression to AST before lowering RHS
    if (isRangeLValue(op->getLeft())) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, op->loc, error);
    }

    bool force_int = dynamic_cast<const QoreBinaryIntLValueOperatorNode*>(node) != nullptr;
    const AbstractQoreNode* left_node = op->getLeft().getInternalNode();
    auto* left_var = dynamic_cast<const VarRefNode*>(left_node);
    // Reference-typed locals need lvalue semantics for write-through
    if (left_var && left_var->getTypeInfo()
            && QoreTypeInfo::isReference(left_var->getTypeInfo())) {
        left_var = nullptr;
    }
    QoreValue right_expr(op->getRight());
    QoreIRValue right = lowerExpression(right_expr, error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    if (left_var && left_var->getType() == VT_IMMEDIATE) {
        left_var = nullptr;
    }
    if (!left_var) {
        // Path-based compound assignment for complex lvalues
        {
            QoreIRValue path_result = tryEmitLValuePathOp(QoreIROpcode::LValuePathCompound,
                op->getLeft(), &right, op->loc, error, false, LVCompoundOp::XorAssign);
            if (path_result.isValid()) {
                return path_result;
            }
        }

        if (!op->getLeft().hasNode()) {
            error = "unsupported lvalue for xor-equals IR lowering";
            return QoreIRValue();
        }
        if (!guardLValueBase(op->getLeft(), error)) {
            return QoreIRValue();
        }
        // lvalue evaluation delegates to AST at runtime; track for implicit arg push
        ++ast_delegate_count;
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {right}, normal_block, handler, op->loc);
            inst->invoke_opcode = QoreIROpcode::XorAssignLValue;
            builder.setBlock(normal_block);
            return inst->result;
        }
        return builder.createLValueBinaryOp(QoreIROpcode::XorAssignLValue, op->getLeft(), right, op->loc)->result;
    }
    QoreIRValue left_value = loadVarRef(left_var, error, "xor-equals", op->getLeft());
    if (!left_value.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = QoreIROpcode::XorAssignAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if (force_int
        || (isIntConstant(op->getLeft()) && isIntConstant(right_expr))
        || (getAnalysis(op->getLeft(), left_analysis)
            && getAnalysis(right_expr, right_analysis)
            && isNeverNothingInt(left_analysis)
            && isNeverNothingInt(right_analysis))) {
        opcode = QoreIROpcode::XorAssignInt;
    }
    QoreIRValue result = lowerBinaryOpOrInvoke(opcode, expr, left_value, right, op->loc, error);
    if (!result.isValid()) {
        return QoreIRValue();
    }
    if (!storeVarRef(left_var, result, error, "xor-equals", &right_expr)) {
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
    // Typed int pre-increment on simple VarRef (exclude references — need lvalue write-through)
    if (dynamic_cast<const QoreIntPreIncrementOperatorNode*>(node)) {
        auto* var = dynamic_cast<const VarRefNode*>(lvexp.getInternalNode());
        if (var && var->getType() != VT_IMMEDIATE && !isRangeLValue(lvexp)
                && !QoreTypeInfo::isReference(var->getTypeInfo())) {
            // Fused path: emit single IncrementLocalInt for lvstack and closure-use locals.
            if (isLocalOrClosureVar(var)) {
                auto* inst = builder.createIncrementLocalInt(var->ref.id, 1, op->loc);
                if (parse_context) {
                    parse_context->markLocalAssignment(var->ref.id, true,
                        var->getTypeInfo());
                }
                return inst->result;
            }
            // Fallback: LoadLocal + AddAssignInt + StoreLocal for closures/globals
            QoreIRValue loaded = loadVarRef(var, error, "pre-increment-int", lvexp);
            if (!loaded.isValid()) {
                return QoreIRValue();
            }
            QoreIRValue one = builder.createConstInt(1, op->loc)->result;
            QoreIRValue result = lowerBinaryOpOrInvoke(
                QoreIROpcode::AddAssignInt, expr, loaded, one, op->loc, error);
            if (!result.isValid()) {
                return QoreIRValue();
            }
            if (!storeVarRef(var, result, error, "pre-increment-int")) {
                return QoreIRValue();
            }
            return result;
        }
    }
    // Range lvalue (e.g., ++list[0..2]) - delegate entire expression to AST
    if (isRangeLValue(lvexp)) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, op->loc, error);
    }
    // Fast path: ++h.key for const-key hash subscript → LoadLocal + HashKeyAccess + Add + HashKeyStore
    {
        const VarRefNode* container_var = nullptr;
        std::string key_name;
        QoreValue key_expr;
        if (isConstKeyHashSubscript(lvexp, container_var, key_name, key_expr)) {
            QoreIRValue one = builder.createConstInt(1, op->loc)->result;
            return emitHashKeyCompoundOp(container_var, key_name, key_expr,
                QoreIROpcode::AddAssignInt, one, expr, op->loc, error);
        }
        if (isDynamicKeyHashSubscript(lvexp, container_var, key_expr)) {
            QoreIRValue one = builder.createConstInt(1, op->loc)->result;
            return emitHashKeyDynamicCompoundOp(container_var, key_expr,
                QoreIROpcode::AddAssignInt, one, expr, op->loc, error);
        }
    }
    // Path-based unary for complex lvalues (member chains, nested subscripts)
    {
        QoreIRValue path_result = tryEmitLValuePathOp(QoreIROpcode::LValuePathUnary,
            lvexp, nullptr, op->loc, error, false, LVCompoundOp::AddAssign, LVUnaryOp::PreInc);
        if (path_result.isValid()) {
            return path_result;
        }
    }
    if (!guardLValueBase(lvexp, error)) {
        return QoreIRValue();
    }
    // lvalue evaluation delegates to AST at runtime; track for implicit arg push
    ++ast_delegate_count;
    if (!exception_stack.empty()) {
        QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
        if (!normal_block) {
            error = "IR builder failed to create invoke continuation block";
            return QoreIRValue();
        }
        QoreIRBasicBlock* handler = exception_stack.back();
        auto* inst = builder.createInvoke(expr, {}, normal_block, handler, op->loc);
        inst->invoke_opcode = QoreIROpcode::PreIncLValue;
        builder.setBlock(normal_block);
        markLocalAssignmentFromExpression(lvexp);
        return inst->result;
    }
    QoreIRValue result = builder.createLValueUnaryOp(QoreIROpcode::PreIncLValue, lvexp, op->loc)->result;
    markLocalAssignmentFromExpression(lvexp);
    return result;
}

QoreIRValue QoreIRLowering::lowerPostIncrement(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    // QoreIntPostIncrementOperatorNode inherits from QoreSingleExpressionOperatorNode<LValueOperatorNode>
    // (NOT from QorePostIncrementOperatorNode), so we must try both casts
    const QoreSingleExpressionOperatorNode<LValueOperatorNode>* base_op =
        dynamic_cast<const QorePostIncrementOperatorNode*>(node);
    if (!base_op) {
        base_op = dynamic_cast<const QoreIntPostIncrementOperatorNode*>(node);
    }
    if (!base_op) {
        return QoreIRValue();
    }
    QoreValue lvexp = base_op->getExp();
    if (!lvexp.hasNode()) {
        error = "unsupported lvalue for post-increment IR lowering";
        return QoreIRValue();
    }
    // Typed int post-increment on simple VarRef (exclude references — need lvalue write-through)
    if (dynamic_cast<const QoreIntPostIncrementOperatorNode*>(node)) {
        auto* var = dynamic_cast<const VarRefNode*>(lvexp.getInternalNode());
        if (var && var->getType() != VT_IMMEDIATE && !isRangeLValue(lvexp)
                && !QoreTypeInfo::isReference(var->getTypeInfo())) {
            QoreIRValue old_value = loadVarRef(var, error, "post-increment-int", lvexp);
            if (!old_value.isValid()) {
                return QoreIRValue();
            }
            QoreIRValue zero = builder.createConstInt(0, base_op->loc)->result;
            QoreIRValue old_result = lowerBinaryOpOrInvoke(
                QoreIROpcode::AddInt, expr, old_value, zero, base_op->loc, error);
            if (!old_result.isValid()) {
                return QoreIRValue();
            }
            QoreIRValue one = builder.createConstInt(1, base_op->loc)->result;
            QoreIRValue new_value = lowerBinaryOpOrInvoke(
                QoreIROpcode::AddAssignInt, expr, old_result, one, base_op->loc, error);
            if (!new_value.isValid()) {
                return QoreIRValue();
            }
            if (!storeVarRef(var, new_value, error, "post-increment-int")) {
                return QoreIRValue();
            }
            return old_result;  // post-increment returns old value coerced to int
        }
    }
    // Range lvalue (e.g., list[0..2]++) - delegate entire expression to AST
    if (isRangeLValue(lvexp)) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, base_op->loc, error);
    }
    // Post-inc/dec on hash keys deferred — needs old value which emitHashKeyCompoundOp
    // doesn't currently expose. The pre-inc/dec case is handled in lowerPreIncrement/Decrement.
    // Path-based unary for complex lvalues
    {
        QoreIRValue path_result = tryEmitLValuePathOp(QoreIROpcode::LValuePathUnary,
            lvexp, nullptr, base_op->loc, error, false, LVCompoundOp::AddAssign, LVUnaryOp::PostInc);
        if (path_result.isValid()) {
            return path_result;
        }
    }
    if (!guardLValueBase(lvexp, error)) {
        return QoreIRValue();
    }
    // lvalue evaluation delegates to AST at runtime; track for implicit arg push
    ++ast_delegate_count;
    if (!exception_stack.empty()) {
        QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
        if (!normal_block) {
            error = "IR builder failed to create invoke continuation block";
            return QoreIRValue();
        }
        QoreIRBasicBlock* handler = exception_stack.back();
        auto* inst = builder.createInvoke(expr, {}, normal_block, handler, base_op->loc);
        inst->invoke_opcode = QoreIROpcode::PostIncLValue;
        builder.setBlock(normal_block);
        markLocalAssignmentFromExpression(lvexp);
        return inst->result;
    }
    QoreIRValue result = builder.createLValueUnaryOp(QoreIROpcode::PostIncLValue, lvexp, base_op->loc)->result;
    markLocalAssignmentFromExpression(lvexp);
    return result;
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
    // Typed int pre-decrement on simple VarRef
    // Exclude references — need lvalue write-through
    if (dynamic_cast<const QoreIntPreDecrementOperatorNode*>(node)) {
        auto* var = dynamic_cast<const VarRefNode*>(lvexp.getInternalNode());
        if (var && var->getType() != VT_IMMEDIATE && !isRangeLValue(lvexp)
                && !QoreTypeInfo::isReference(var->getTypeInfo())) {
            // Fused path: emit single IncrementLocalInt(delta=-1) for lvstack and closure-use locals.
            if (isLocalOrClosureVar(var)) {
                auto* inst = builder.createIncrementLocalInt(var->ref.id, -1, op->loc);
                if (parse_context) {
                    parse_context->markLocalAssignment(var->ref.id, true,
                        var->getTypeInfo());
                }
                return inst->result;
            }
            // Fallback: LoadLocal + SubAssignInt + StoreLocal for closures/globals
            QoreIRValue loaded = loadVarRef(var, error, "pre-decrement-int", lvexp);
            if (!loaded.isValid()) {
                return QoreIRValue();
            }
            QoreIRValue one = builder.createConstInt(1, op->loc)->result;
            QoreIRValue result = lowerBinaryOpOrInvoke(
                QoreIROpcode::SubAssignInt, expr, loaded, one, op->loc, error);
            if (!result.isValid()) {
                return QoreIRValue();
            }
            if (!storeVarRef(var, result, error, "pre-decrement-int")) {
                return QoreIRValue();
            }
            return result;
        }
    }
    // Range lvalue (e.g., --list[0..2]) - delegate entire expression to AST
    if (isRangeLValue(lvexp)) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, op->loc, error);
    }
    // Fast path: --h.key for const-key hash subscript → LoadLocal + HashKeyAccess + Sub + HashKeyStore
    {
        const VarRefNode* container_var = nullptr;
        std::string key_name;
        QoreValue key_expr;
        if (isConstKeyHashSubscript(lvexp, container_var, key_name, key_expr)) {
            QoreIRValue one = builder.createConstInt(1, op->loc)->result;
            return emitHashKeyCompoundOp(container_var, key_name, key_expr,
                QoreIROpcode::SubAssignInt, one, expr, op->loc, error);
        }
        if (isDynamicKeyHashSubscript(lvexp, container_var, key_expr)) {
            QoreIRValue one = builder.createConstInt(1, op->loc)->result;
            return emitHashKeyDynamicCompoundOp(container_var, key_expr,
                QoreIROpcode::SubAssignInt, one, expr, op->loc, error);
        }
    }
    // Path-based unary for complex lvalues
    {
        QoreIRValue path_result = tryEmitLValuePathOp(QoreIROpcode::LValuePathUnary,
            lvexp, nullptr, op->loc, error, false, LVCompoundOp::AddAssign, LVUnaryOp::PreDec);
        if (path_result.isValid()) {
            return path_result;
        }
    }
    if (!guardLValueBase(lvexp, error)) {
        return QoreIRValue();
    }
    // lvalue evaluation delegates to AST at runtime; track for implicit arg push
    ++ast_delegate_count;
    if (!exception_stack.empty()) {
        QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
        if (!normal_block) {
            error = "IR builder failed to create invoke continuation block";
            return QoreIRValue();
        }
        QoreIRBasicBlock* handler = exception_stack.back();
        auto* inst = builder.createInvoke(expr, {}, normal_block, handler, op->loc);
        inst->invoke_opcode = QoreIROpcode::PreDecLValue;
        builder.setBlock(normal_block);
        markLocalAssignmentFromExpression(lvexp);
        return inst->result;
    }
    QoreIRValue result = builder.createLValueUnaryOp(QoreIROpcode::PreDecLValue, lvexp, op->loc)->result;
    markLocalAssignmentFromExpression(lvexp);
    return result;
}

QoreIRValue QoreIRLowering::lowerPostDecrement(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    // QoreIntPostDecrementOperatorNode inherits from QoreIntPostIncrementOperatorNode
    // (NOT from QorePostDecrementOperatorNode), so we must try both casts
    const QoreSingleExpressionOperatorNode<LValueOperatorNode>* base_op =
        dynamic_cast<const QorePostDecrementOperatorNode*>(node);
    if (!base_op) {
        base_op = dynamic_cast<const QoreIntPostDecrementOperatorNode*>(node);
    }
    if (!base_op) {
        return QoreIRValue();
    }
    QoreValue lvexp = base_op->getExp();
    if (!lvexp.hasNode()) {
        error = "unsupported lvalue for post-decrement IR lowering";
        return QoreIRValue();
    }
    // Typed int post-decrement on simple VarRef (exclude references — need lvalue write-through)
    if (dynamic_cast<const QoreIntPostDecrementOperatorNode*>(node)) {
        auto* var = dynamic_cast<const VarRefNode*>(lvexp.getInternalNode());
        if (var && var->getType() != VT_IMMEDIATE && !isRangeLValue(lvexp)
                && !QoreTypeInfo::isReference(var->getTypeInfo())) {
            QoreIRValue old_value = loadVarRef(var, error, "post-decrement-int", lvexp);
            if (!old_value.isValid()) {
                return QoreIRValue();
            }
            QoreIRValue zero = builder.createConstInt(0, base_op->loc)->result;
            QoreIRValue old_result = lowerBinaryOpOrInvoke(
                QoreIROpcode::AddInt, expr, old_value, zero, base_op->loc, error);
            if (!old_result.isValid()) {
                return QoreIRValue();
            }
            QoreIRValue one = builder.createConstInt(1, base_op->loc)->result;
            QoreIRValue new_value = lowerBinaryOpOrInvoke(
                QoreIROpcode::SubAssignInt, expr, old_result, one, base_op->loc, error);
            if (!new_value.isValid()) {
                return QoreIRValue();
            }
            if (!storeVarRef(var, new_value, error, "post-decrement-int")) {
                return QoreIRValue();
            }
            return old_result;  // post-decrement returns old value coerced to int
        }
    }
    // Range lvalue (e.g., list[0..2]--) - delegate entire expression to AST
    if (isRangeLValue(lvexp)) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, base_op->loc, error);
    }
    // Path-based unary for complex lvalues
    {
        QoreIRValue path_result = tryEmitLValuePathOp(QoreIROpcode::LValuePathUnary,
            lvexp, nullptr, base_op->loc, error, false, LVCompoundOp::AddAssign, LVUnaryOp::PostDec);
        if (path_result.isValid()) {
            return path_result;
        }
    }
    if (!guardLValueBase(lvexp, error)) {
        return QoreIRValue();
    }
    // lvalue evaluation delegates to AST at runtime; track for implicit arg push
    ++ast_delegate_count;
    if (!exception_stack.empty()) {
        QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
        if (!normal_block) {
            error = "IR builder failed to create invoke continuation block";
            return QoreIRValue();
        }
        QoreIRBasicBlock* handler = exception_stack.back();
        auto* inst = builder.createInvoke(expr, {}, normal_block, handler, base_op->loc);
        inst->invoke_opcode = QoreIROpcode::PostDecLValue;
        builder.setBlock(normal_block);
        markLocalAssignmentFromExpression(lvexp);
        return inst->result;
    }
    QoreIRValue result = builder.createLValueUnaryOp(QoreIROpcode::PostDecLValue, lvexp, base_op->loc)->result;
    markLocalAssignmentFromExpression(lvexp);
    return result;
}

QoreIRValue QoreIRLowering::lowerPlus(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* plus = dynamic_cast<const QorePlusOperatorNode*>(node);
    if (!plus) {
        return QoreIRValue();
    }

    QoreValue left_expr = plus->getLeft();
    QoreValue right_expr = plus->getRight();

    // Lambda to check if a leaf expression has string type using AST type info
    // Note: We check VarRefNode specifically because ParseNode::getTypeInfo() is protected
    auto isStringTypedExpr = [](QoreValue e) -> bool {
        // Check for string literal
        if (e.getType() == NT_STRING) {
            return true;
        }
        // Check VarRefNode's type info (public method)
        auto* var_node = dynamic_cast<const VarRefNode*>(e.getInternalNode());
        if (var_node) {
            const QoreTypeInfo* type_info = var_node->getTypeInfo();
            if (type_info && QoreTypeInfo::isType(type_info, NT_STRING)) {
                return true;
            }
        }
        return false;
    };

    // Lambda to recursively check if expression is a string plus chain (all leaves are strings)
    std::function<bool(QoreValue)> isStringPlusChain = [&](QoreValue e) -> bool {
        auto* p = dynamic_cast<const QorePlusOperatorNode*>(e.getInternalNode());
        if (p) {
            // It's a plus - check if its return type is string (public method on QorePlusOperatorNode)
            const QoreTypeInfo* type_info = p->getTypeInfo();
            if (!type_info || !QoreTypeInfo::isType(type_info, NT_STRING)) {
                return false;
            }
            // Also recursively check operands
            return isStringPlusChain(p->getLeft()) && isStringPlusChain(p->getRight());
        }
        // It's a leaf - check if it's a string type
        return isStringTypedExpr(e);
    };

    // Check for typed string concatenation chain (a + b + c + d)
    // First check if the whole expression is a string plus chain
    if (isStringPlusChain(expr)) {
        // Collect all operands from the chain
        std::vector<QoreValue> ast_operands;

        // Lambda to recursively collect leaf operands
        std::function<void(QoreValue)> collectOperands = [&](QoreValue e) {
            auto* p = dynamic_cast<const QorePlusOperatorNode*>(e.getInternalNode());
            if (p) {
                collectOperands(p->getLeft());
                collectOperands(p->getRight());
            } else {
                ast_operands.push_back(e);
            }
        };
        collectOperands(expr);

        // Use StringConcat for 3+ operands (chains), AddString for 2
        if (ast_operands.size() >= 3) {
            // Lower all operands
            std::vector<QoreIRValue> ir_operands;
            for (const auto& op : ast_operands) {
                QoreIRValue lowered = lowerExpression(op, error);
                if (!lowered.isValid()) {
                    return QoreIRValue();
                }
                ir_operands.push_back(lowered);
            }

            // Create StringConcat instruction using createBinaryOp for first two operands
            QoreIRInstruction* inst = builder.createBinaryOp(
                    QoreIROpcode::StringConcat, ir_operands[0], ir_operands[1], plus->loc);
            // Add remaining operands
            for (size_t i = 2; i < ir_operands.size(); ++i) {
                inst->operands.push_back(ir_operands[i]);
            }
            return inst->result;
        }

        // Regular two-operand string concatenation
        QoreIRValue left = lowerExpression(plus->getLeft(), error);
        if (!left.isValid()) {
            return QoreIRValue();
        }
        QoreIRValue right = lowerExpression(plus->getRight(), error);
        if (!right.isValid()) {
            return QoreIRValue();
        }
        return lowerBinaryOpOrInvoke(QoreIROpcode::AddString, expr, left, right, plus->loc, error);
    }

    // Non-string addition
    QoreIRValue left = lowerExpression(plus->getLeft(), error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue right = lowerExpression(plus->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode op = selectNumericOpcode(plus->getLeft(), plus->getRight(),
        QoreIROpcode::AddInt, QoreIROpcode::AddFloat, QoreIROpcode::AddAny,
        QoreIROpcode::AddNumber);
    // Use timeout-aware opcode when one operand is typed as timeout (int ms convention)
    if (op == QoreIROpcode::AddAny) {
        const QoreTypeInfo* lti = getExprTypeInfo(plus->getLeft());
        const QoreTypeInfo* rti = getExprTypeInfo(plus->getRight());
        if ((lti && QoreTypeInfo::equal(lti, timeoutTypeInfo))
                || (rti && QoreTypeInfo::equal(rti, timeoutTypeInfo))) {
            op = QoreIROpcode::AddTimeout;
        }
    }
    return lowerBinaryOpOrInvoke(op, expr, left, right, plus->loc, error);
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
    QoreIROpcode op = selectNumericOpcode(minus->getLeft(), minus->getRight(),
        QoreIROpcode::SubInt, QoreIROpcode::SubFloat, QoreIROpcode::SubAny,
        QoreIROpcode::SubNumber);
    // Use timeout-aware opcode when one operand is typed as timeout (int ms convention)
    if (op == QoreIROpcode::SubAny) {
        const QoreTypeInfo* lti = getExprTypeInfo(minus->getLeft());
        const QoreTypeInfo* rti = getExprTypeInfo(minus->getRight());
        if ((lti && QoreTypeInfo::equal(lti, timeoutTypeInfo))
                || (rti && QoreTypeInfo::equal(rti, timeoutTypeInfo))) {
            op = QoreIROpcode::SubTimeout;
        }
    }
    return lowerBinaryOpOrInvoke(op, expr, left, right, minus->loc, error);
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
    // Check for typed string equality first
    QoreValue left_expr = eq->getLeft();
    QoreValue right_expr = eq->getRight();
    QoreIROpcode op;
    if (guaranteedStringType(&left_expr) && guaranteedStringType(&right_expr)) {
        op = QoreIROpcode::EqString;
    } else {
        op = selectComparisonOpcode(eq->getLeft(), eq->getRight(),
            QoreIROpcode::EqInt, QoreIROpcode::EqFloat, QoreIROpcode::EqAny);
    }
    return lowerBinaryOpOrInvoke(op, expr, left, right, eq->loc, error);
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
    // Check for typed string inequality first
    QoreValue left_expr = ne->getLeft();
    QoreValue right_expr = ne->getRight();
    QoreIROpcode op;
    if (guaranteedStringType(&left_expr) && guaranteedStringType(&right_expr)) {
        op = QoreIROpcode::NeString;
    } else {
        op = selectComparisonOpcode(ne->getLeft(), ne->getRight(),
            QoreIROpcode::NeInt, QoreIROpcode::NeFloat, QoreIROpcode::NeAny);
    }
    return lowerBinaryOpOrInvoke(op, expr, left, right, ne->loc, error);
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
    return lowerBinaryOpOrInvoke(QoreIROpcode::EqHard, expr, left, right, eq->loc, error);
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
    return lowerBinaryOpOrInvoke(QoreIROpcode::NeHard, expr, left, right, ne->loc, error);
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
    // Check for typed string comparison first
    QoreValue left_expr = lt->getLeft();
    QoreValue right_expr = lt->getRight();
    QoreIROpcode op;
    if (guaranteedStringType(&left_expr) && guaranteedStringType(&right_expr)) {
        op = QoreIROpcode::LtString;
    } else {
        op = selectComparisonOpcode(lt->getLeft(), lt->getRight(),
            QoreIROpcode::LtInt, QoreIROpcode::LtFloat, QoreIROpcode::LtAny);
    }
    return lowerBinaryOpOrInvoke(op, expr, left, right, lt->loc, error);
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
    // Check for typed string comparison first
    QoreValue left_expr = le->getLeft();
    QoreValue right_expr = le->getRight();
    QoreIROpcode op;
    if (guaranteedStringType(&left_expr) && guaranteedStringType(&right_expr)) {
        op = QoreIROpcode::LeString;
    } else {
        op = selectComparisonOpcode(le->getLeft(), le->getRight(),
            QoreIROpcode::LeInt, QoreIROpcode::LeFloat, QoreIROpcode::LeAny);
    }
    return lowerBinaryOpOrInvoke(op, expr, left, right, le->loc, error);
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
    // Check for typed string comparison first
    QoreValue left_expr = gt->getLeft();
    QoreValue right_expr = gt->getRight();
    QoreIROpcode op;
    if (guaranteedStringType(&left_expr) && guaranteedStringType(&right_expr)) {
        op = QoreIROpcode::GtString;
    } else {
        op = selectComparisonOpcode(gt->getLeft(), gt->getRight(),
            QoreIROpcode::GtInt, QoreIROpcode::GtFloat, QoreIROpcode::GtAny);
    }
    return lowerBinaryOpOrInvoke(op, expr, left, right, gt->loc, error);
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
    // Check for typed string comparison first
    QoreValue left_expr = ge->getLeft();
    QoreValue right_expr = ge->getRight();
    QoreIROpcode op;
    if (guaranteedStringType(&left_expr) && guaranteedStringType(&right_expr)) {
        op = QoreIROpcode::GeString;
    } else {
        op = selectComparisonOpcode(ge->getLeft(), ge->getRight(),
            QoreIROpcode::GeInt, QoreIROpcode::GeFloat, QoreIROpcode::GeAny);
    }
    return lowerBinaryOpOrInvoke(op, expr, left, right, ge->loc, error);
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
    // Check for typed string comparison first
    QoreValue left_expr = cmp->getLeft();
    QoreValue right_expr = cmp->getRight();
    QoreIROpcode op;
    if (guaranteedStringType(&left_expr) && guaranteedStringType(&right_expr)) {
        op = QoreIROpcode::CmpString;
    } else {
        op = selectComparisonOpcode(cmp->getLeft(), cmp->getRight(),
            QoreIROpcode::CmpInt, QoreIROpcode::CmpFloat, QoreIROpcode::CmpAny);
    }
    return lowerBinaryOpOrInvoke(op, expr, left, right, cmp->loc, error);
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
    return lowerUnaryOpOrInvoke(QoreIROpcode::UnaryPlusAny, expr, value, plus->loc, error);
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
    return lowerUnaryOpOrInvoke(op, expr, value, minus->loc, error);
}

QoreIRValue QoreIRLowering::lowerBinaryNot(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* binary_not = dynamic_cast<const QoreBinaryNotOperatorNode*>(node);
    if (!binary_not) {
        return QoreIRValue();
    }
    if (!ensureBuilderContext(error)) {
        return QoreIRValue();
    }

    QoreIRValue value = lowerExpression(binary_not->getExp(), error);
    if (!value.isValid()) {
        return QoreIRValue();
    }

    QoreIROpcode op = QoreIROpcode::XorAny;
    QoreParseAnalysis analysis;
    if (getAnalysis(binary_not->getExp(), analysis) && isNeverNothingInt(analysis)) {
        op = QoreIROpcode::XorInt;
    }

    QoreIRValue all_bits = builder.createConstInt(-1, binary_not->loc)->result;
    return lowerBinaryOpOrInvoke(op, expr, value, all_bits, binary_not->loc, error);
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
    QoreIROpcode op = selectNumericOpcode(mul->getLeft(), mul->getRight(),
        QoreIROpcode::MulInt, QoreIROpcode::MulFloat, QoreIROpcode::MulAny,
        QoreIROpcode::MulNumber);
    return lowerBinaryOpOrInvoke(op, expr, left, right, mul->loc, error);
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
    QoreIROpcode op = selectNumericOpcode(div->getLeft(), div->getRight(),
        QoreIROpcode::DivInt, QoreIROpcode::DivFloat, QoreIROpcode::DivAny,
        QoreIROpcode::DivNumber);
    return lowerBinaryOpOrInvoke(op, expr, left, right, div->loc, error);
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
    QoreIROpcode op = selectNumericOpcode(mod->getLeft(), mod->getRight(),
        QoreIROpcode::ModInt, QoreIROpcode::ModAny, QoreIROpcode::ModAny);
    return lowerBinaryOpOrInvoke(op, expr, left, right, mod->loc, error);
}

QoreIRValue QoreIRLowering::lowerBinaryAnd(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreBinaryAndOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }

    GuardExceptionTargetOverrideScope guard_scope(*this, getGuardExceptionTarget());
    QoreValue left_expr = op->getLeft();
    QoreIRValue left = lowerExpression(left_expr, error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreValue right_expr = op->getRight();
    QoreIRValue right = lowerExpression(right_expr, error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = selectNumericOpcode(op->getLeft(), op->getRight(),
        QoreIROpcode::AndInt, QoreIROpcode::AndAny, QoreIROpcode::AndAny);
    return lowerBinaryOpOrInvoke(opcode, expr, left, right, op->loc, error);
}

QoreIRValue QoreIRLowering::lowerBinaryOr(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreBinaryOrOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }

    GuardExceptionTargetOverrideScope guard_scope(*this, getGuardExceptionTarget());

    QoreValue left_expr = op->getLeft();
    QoreIRValue left = lowerExpression(left_expr, error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreValue right_expr = op->getRight();
    QoreIRValue right = lowerExpression(right_expr, error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = selectNumericOpcode(op->getLeft(), op->getRight(),
        QoreIROpcode::OrInt, QoreIROpcode::OrAny, QoreIROpcode::OrAny);
    return lowerBinaryOpOrInvoke(opcode, expr, left, right, op->loc, error);
}

QoreIRValue QoreIRLowering::lowerBinaryXor(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreBinaryXorOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }

    QoreValue left_expr = op->getLeft();
    QoreIRValue left = lowerExpression(left_expr, error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreValue right_expr = op->getRight();
    QoreIRValue right = lowerExpression(right_expr, error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = selectNumericOpcode(op->getLeft(), op->getRight(),
        QoreIROpcode::XorInt, QoreIROpcode::XorAny, QoreIROpcode::XorAny);
    return lowerBinaryOpOrInvoke(opcode, expr, left, right, op->loc, error);
}

QoreIRValue QoreIRLowering::lowerShiftLeft(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreShiftLeftOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }

    QoreValue left_expr = op->getLeft();
    QoreIRValue left = lowerExpression(left_expr, error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreValue right_expr = op->getRight();
    QoreIRValue right = lowerExpression(right_expr, error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = selectNumericOpcode(op->getLeft(), op->getRight(),
        QoreIROpcode::ShlInt, QoreIROpcode::ShlAny, QoreIROpcode::ShlAny);
    return lowerBinaryOpOrInvoke(opcode, expr, left, right, op->loc, error);
}

QoreIRValue QoreIRLowering::lowerShiftRight(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreShiftRightOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }

    QoreValue left_expr = op->getLeft();
    QoreIRValue left = lowerExpression(left_expr, error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreValue right_expr = op->getRight();
    QoreIRValue right = lowerExpression(right_expr, error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = selectNumericOpcode(op->getLeft(), op->getRight(),
        QoreIROpcode::ShrInt, QoreIROpcode::ShrAny, QoreIROpcode::ShrAny);
    return lowerBinaryOpOrInvoke(opcode, expr, left, right, op->loc, error);
}

QoreIRValue QoreIRLowering::lowerShiftLeftEquals(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreShiftLeftEqualsOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    // Range lvalue (e.g., list[0..2] <<= x) - delegate entire expression to AST before lowering RHS
    if (isRangeLValue(op->getLeft())) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, op->loc, error);
    }

    bool force_int = dynamic_cast<const QoreBinaryIntLValueOperatorNode*>(node) != nullptr;

    const AbstractQoreNode* left_node = op->getLeft().getInternalNode();
    auto* left_var = dynamic_cast<const VarRefNode*>(left_node);
    // Reference-typed locals need lvalue semantics for write-through
    if (left_var && left_var->getTypeInfo()
            && QoreTypeInfo::isReference(left_var->getTypeInfo())) {
        left_var = nullptr;
    }
    QoreValue right_expr(op->getRight());
    QoreIRValue right = lowerExpression(right_expr, error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    if (left_var && left_var->getType() == VT_IMMEDIATE) {
        left_var = nullptr;
    }
    if (!left_var) {
        if (!op->getLeft().hasNode()) {
            error = "unsupported lvalue for shift-left-assign IR lowering";
            return QoreIRValue();
        }
        if (!guardLValueBase(op->getLeft(), error)) {
            return QoreIRValue();
        }
        // lvalue evaluation delegates to AST at runtime; track for implicit arg push
        ++ast_delegate_count;
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {right}, normal_block, handler, op->loc);
            inst->invoke_opcode = QoreIROpcode::ShlAssignLValue;
            builder.setBlock(normal_block);
            return inst->result;
        }
        return builder.createLValueBinaryOp(QoreIROpcode::ShlAssignLValue, op->getLeft(), right, op->loc)->result;
    }
    QoreIRValue left_value = loadVarRef(left_var, error, "shift-left-assign", op->getLeft());
    if (!left_value.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = force_int
        ? QoreIROpcode::ShlAssignInt
        : selectNumericOpcode(op->getLeft(), right_expr,
            QoreIROpcode::ShlAssignInt, QoreIROpcode::ShlAssignAny, QoreIROpcode::ShlAssignAny);
    QoreIRValue result = lowerBinaryOpOrInvoke(opcode, expr, left_value, right, op->loc, error);
    if (!result.isValid()) {
        return QoreIRValue();
    }
    if (!storeVarRef(left_var, result, error, "shift-left-assign", &right_expr)) {
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
    // Range lvalue (e.g., list[0..2] >>= x) - delegate entire expression to AST before lowering RHS
    if (isRangeLValue(op->getLeft())) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, op->loc, error);
    }

    bool force_int = dynamic_cast<const QoreBinaryIntLValueOperatorNode*>(node) != nullptr;

    const AbstractQoreNode* left_node = op->getLeft().getInternalNode();
    auto* left_var = dynamic_cast<const VarRefNode*>(left_node);
    // Reference-typed locals need lvalue semantics for write-through
    if (left_var && left_var->getTypeInfo()
            && QoreTypeInfo::isReference(left_var->getTypeInfo())) {
        left_var = nullptr;
    }
    QoreValue right_expr(op->getRight());
    QoreIRValue right = lowerExpression(right_expr, error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    if (left_var && left_var->getType() == VT_IMMEDIATE) {
        left_var = nullptr;
    }
    if (!left_var) {
        if (!op->getLeft().hasNode()) {
            error = "unsupported lvalue for shift-right-assign IR lowering";
            return QoreIRValue();
        }
        if (!guardLValueBase(op->getLeft(), error)) {
            return QoreIRValue();
        }
        // lvalue evaluation delegates to AST at runtime; track for implicit arg push
        ++ast_delegate_count;
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {right}, normal_block, handler, op->loc);
            inst->invoke_opcode = QoreIROpcode::ShrAssignLValue;
            builder.setBlock(normal_block);
            return inst->result;
        }
        return builder.createLValueBinaryOp(QoreIROpcode::ShrAssignLValue, op->getLeft(), right, op->loc)->result;
    }
    QoreIRValue left_value = loadVarRef(left_var, error, "shift-right-assign", op->getLeft());
    if (!left_value.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = force_int
        ? QoreIROpcode::ShrAssignInt
        : selectNumericOpcode(op->getLeft(), right_expr,
            QoreIROpcode::ShrAssignInt, QoreIROpcode::ShrAssignAny, QoreIROpcode::ShrAssignAny);
    QoreIRValue result = lowerBinaryOpOrInvoke(opcode, expr, left_value, right, op->loc, error);
    if (!result.isValid()) {
        return QoreIRValue();
    }
    if (!storeVarRef(left_var, result, error, "shift-right-assign", &right_expr)) {
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

    GuardExceptionTargetOverrideScope guard_scope(*this, getGuardExceptionTarget());

    QoreValue left_expr = op->getLeft();
    QoreIRValue left = lowerExpression(left_expr, error);
    if (!left.isValid()) {
        return QoreIRValue();
    }
    QoreValue right_expr = op->getRight();
    QoreIRValue right = lowerExpression(right_expr, error);
    if (!right.isValid()) {
        return QoreIRValue();
    }
    const QoreTypeInfo* left_type = getGuaranteedTypeForValue(&left_expr, nullptr);
    const QoreTypeInfo* right_type = getGuaranteedTypeForValue(&right_expr, nullptr);
    maybeInsertNotNothingGuard(left, &left_expr, nullptr, left_type);
    maybeInsertNotNothingGuard(right, &right_expr, nullptr, right_type);
    QoreIROpcode opcode = QoreIROpcode::RangeAny;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    if (getAnalysis(op->getLeft(), left_analysis)
        && getAnalysis(op->getRight(), right_analysis)
        && analysisIndicatesDate(left_analysis)
        && analysisIndicatesDate(right_analysis)) {
        opcode = QoreIROpcode::RangeDate;
    } else {
        QoreParseAnalysis expr_analysis;
        if (getAnalysis(expr, expr_analysis)) {
            opcode = selectFoldOpcode(expr_analysis, QoreIROpcode::RangeAny,
                QoreIROpcode::RangeInt, QoreIROpcode::RangeFloat);
        } else {
            opcode = selectNumericOpcode(op->getLeft(), op->getRight(),
                QoreIROpcode::RangeInt, QoreIROpcode::RangeFloat, QoreIROpcode::RangeAny);
        }
    }
    QoreIRValue result;
    if (!exception_stack.empty()) {
        QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
        if (!normal_block) {
            error = "IR builder failed to create invoke continuation block";
            return QoreIRValue();
        }
        QoreIRBasicBlock* handler = exception_stack.back();
        auto* inst = builder.createInvoke(expr, {left, right}, normal_block, handler, op->loc);
        inst->invoke_opcode = opcode;
        builder.setBlock(normal_block);
        result = inst->result;
    } else {
        result = builder.createBinaryOp(opcode, left, right, op->loc)->result;
    }
    maybeInsertNotNothingGuard(result, &expr, op->loc, nullptr);
    return result;
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
    maybeInsertNotNothingGuard(seq, op->get(0));
    maybeInsertNotNothingGuard(start, op->get(1));
    maybeInsertNotNothingGuard(end, op->get(2));
    QoreIROpcode opcode = QoreIROpcode::RangeSliceAny;
    QoreParseAnalysis expr_analysis;
    if (getAnalysis(expr, expr_analysis)) {
        opcode = selectFoldOpcode(expr_analysis, QoreIROpcode::RangeSliceAny,
            QoreIROpcode::RangeSliceInt, QoreIROpcode::RangeSliceFloat);
        if (opcode == QoreIROpcode::RangeSliceAny) {
            opcode = selectNumericOpcode(op->get(1), op->get(2),
                QoreIROpcode::RangeSliceInt, QoreIROpcode::RangeSliceFloat, QoreIROpcode::RangeSliceAny);
        }
    } else {
        opcode = selectNumericOpcode(op->get(1), op->get(2),
            QoreIROpcode::RangeSliceInt, QoreIROpcode::RangeSliceFloat, QoreIROpcode::RangeSliceAny);
    }
    QoreIRValue result;
    if (!exception_stack.empty()) {
        QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
        if (!normal_block) {
            error = "IR builder failed to create invoke continuation block";
            return QoreIRValue();
        }
        QoreIRBasicBlock* handler = exception_stack.back();
        auto* inst = builder.createInvoke(expr, {seq, start, end}, normal_block, handler, op->loc);
        inst->invoke_opcode = opcode;
        builder.setBlock(normal_block);
        result = inst->result;
    } else {
        result = builder.createTernaryOp(opcode, seq, start, end, op->loc)->result;
    }
    maybeInsertNotNothingGuard(result, &expr, op->loc, nullptr);
    return result;
}

static bool collectListIndexRangeSelectors(const QoreValue& rhs, std::vector<QoreValue>& selectors) {
    bool has_range = false;
    if (auto* pln = dynamic_cast<const QoreParseListNode*>(rhs.getInternalNode())) {
        selectors.reserve(pln->size());
        for (size_t i = 0; i < pln->size(); ++i) {
            QoreValue v = pln->get(i);
            if (dynamic_cast<const QoreRangeOperatorNode*>(v.getInternalNode())) {
                has_range = true;
            }
            selectors.push_back(v);
        }
        return has_range;
    }
    if (auto* qln = dynamic_cast<const QoreListNode*>(rhs.getInternalNode())) {
        selectors.reserve(qln->size());
        for (size_t i = 0; i < qln->size(); ++i) {
            QoreValue v = qln->retrieveEntry(i);
            if (dynamic_cast<const QoreRangeOperatorNode*>(v.getInternalNode())) {
                has_range = true;
            }
            selectors.push_back(v);
        }
        return has_range;
    }
    return false;
}

QoreIRValue QoreIRLowering::lowerSquareBrackets(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreSquareBracketsOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    // Use expression evaluation (Call) instead of LoadLValue so that
    // reading from non-list types (binary, string, hash) works correctly;
    // LValueHelper only supports list-type lvalue access and would reject
    // binary[index] or string[index] with a RUNTIME-TYPE-ERROR.

    // Multi-selector slices can contain range entries (for example
    // str[0, 3..4, 5]). Evaluating the whole RHS as a list loses which entry
    // was a range, so lower each selector separately and carry compact
    // selector metadata on the native ListIndexDynamic instruction.
    std::vector<QoreValue> range_selectors;
    if (collectListIndexRangeSelectors(op->getRight(), range_selectors)) {
        QoreIRValue lhs = lowerExpression(op->getLeft(), error);
        if (!lhs.isValid()) {
            return QoreIRValue();
        }
        std::vector<QoreIRValue> operands{lhs};
        std::vector<uint8_t> selector_kinds;
        selector_kinds.reserve(range_selectors.size());
        for (QoreValue selector : range_selectors) {
            if (auto* range = dynamic_cast<const QoreRangeOperatorNode*>(selector.getInternalNode())) {
                QoreIRValue start = lowerExpression(range->getLeft(), error);
                if (!start.isValid()) {
                    return QoreIRValue();
                }
                QoreIRValue stop = lowerExpression(range->getRight(), error);
                if (!stop.isValid()) {
                    return QoreIRValue();
                }
                selector_kinds.push_back(1);
                operands.push_back(start);
                operands.push_back(stop);
            } else {
                QoreIRValue index = lowerExpression(selector, error);
                if (!index.isValid()) {
                    return QoreIRValue();
                }
                selector_kinds.push_back(0);
                operands.push_back(index);
            }
        }
        auto* inst = builder.createExprOp(QoreIROpcode::ListIndexDynamic, expr, operands, op->loc);
        inst->list_selector_kinds = std::move(selector_kinds);
        return inst->result;
    }

    // Lower both operands (container and index) for native execution
    QoreIRValue lhs = lowerExpression(op->getLeft(), error);
    if (!lhs.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue rhs = lowerExpression(op->getRight(), error);
    if (!rhs.isValid()) {
        return QoreIRValue();
    }
    std::vector<QoreIRValue> operands{lhs, rhs};
    return lowerExprOpOrInvoke(QoreIROpcode::ListIndexDynamic, expr, operands, op->loc, error);
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

    // Try native hash/object key access: h.key or h{"key"} with constant string key.
    // QoreHashObjectDereferenceOperatorNode handles both hash key access and object
    // member access via {"key"} syntax. qore_rt_hash_key_access handles both types.
    QoreValue right_val = op->getRight();
    if (right_val.hasNode() && right_val.getType() == NT_STRING) {
        QoreStringValueHelper key(right_val);
        const char* key_str = key->c_str();
        // Lower the base expression
        QoreIRValue base_val = lowerExpression(op->getLeft(), error);
        if (!base_val.isValid()) {
            return QoreIRValue();
        }
        std::vector<QoreIRValue> operands{base_val};
        QoreIRValue result;
        bool should_invoke = !exception_stack.empty() && expressionCanThrow(expr);
        // Plain hash<auto> (no hashdecl, not object) never throws on key access
        if (should_invoke) {
            const QoreTypeInfo* base_type = getExprTypeInfo(op->getLeft());
            if (QoreTypeInfo::parseReturns(base_type, NT_HASH) == QTI_IDENT
                    && !QoreTypeInfo::getUniqueReturnHashDecl(base_type)
                    && !QoreTypeInfo::getUniqueReturnClass(base_type)) {
                should_invoke = false;
            }
        }
        if (should_invoke) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvokeHashKeyAccess(key_str, expr, operands,
                normal_block, handler, op->loc);
            builder.setBlock(normal_block);
            result = inst->result;
        } else {
            // Check if value type is known int for HashKeyAccessInt optimization
            const QoreTypeInfo* base_type = getExprTypeInfo(op->getLeft());
            const QoreTypeInfo* hash_val_type = QoreTypeInfo::getUniqueReturnComplexHash(base_type);
            if (hash_val_type
                    && QoreTypeInfo::parseReturns(hash_val_type, NT_INT) == QTI_IDENT) {
                // Native int return - no refcounting needed
                auto* hka_inst = builder.createHashKeyAccessInt(key_str, op->loc);
                hka_inst->operands = operands;
                result = hka_inst->result;
            } else {
                auto* hka_inst = builder.createHashKeyAccess(key_str, op->loc);
                hka_inst->operands = operands;
                result = hka_inst->result;
            }
        }
        return result;
    }

    if (!guardLValueBase(lvalue, error)) {
        return QoreIRValue();
    }
    // Dynamic key: lower both base and key expressions for native execution.
    // Handles both single-key access (h{var}) and multi-key slicing (h{list}).
    QoreIRValue base_val = lowerExpression(op->getLeft(), error);
    if (!base_val.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue key_val = lowerExpression(op->getRight(), error);
    if (!key_val.isValid()) {
        return QoreIRValue();
    }
    std::vector<QoreIRValue> operands{base_val, key_val};
    return lowerExprOpOrInvoke(QoreIROpcode::HashDerefDynamic, expr, operands, op->loc, error);
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
    // Range lvalue (e.g., shift list[0..2]) - delegate entire expression to AST
    if (isRangeLValue(lvalue)) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, op->loc, error);
    }
    // Path-based shift for complex lvalues (must be before guardLValueBase)
    {
        QoreIRValue path_result = tryEmitLValuePathOp(QoreIROpcode::LValuePathUnary,
            lvalue, nullptr, op->loc, error, false, LVCompoundOp::AddAssign, LVUnaryOp::Shift);
        if (path_result.isValid()) {
            return path_result;
        }
    }
    if (!guardLValueBase(lvalue, error)) {
        return QoreIRValue();
    }
    if (!exception_stack.empty()) {
        QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
        if (!normal_block) {
            error = "IR builder failed to create invoke continuation block";
            return QoreIRValue();
        }
        QoreIRBasicBlock* handler = exception_stack.back();
        auto* inst = builder.createInvoke(expr, {}, normal_block, handler, op->loc);
        inst->invoke_opcode = QoreIROpcode::ShiftLValue;
        builder.setBlock(normal_block);
        return inst->result;
    }
    return builder.createLValueUnaryOp(QoreIROpcode::ShiftLValue, lvalue, op->loc)->result;
}

QoreIRValue QoreIRLowering::lowerPop(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QorePopOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    // Path-based pop for complex lvalues (must be before guardLValueBase)
    {
        QoreIRValue path_result = tryEmitLValuePathOp(QoreIROpcode::LValuePathUnary,
            op->getExp(), nullptr, op->loc, error, false, LVCompoundOp::AddAssign, LVUnaryOp::Pop);
        if (path_result.isValid()) {
            return path_result;
        }
    }
    if (!guardLValueBase(op->getExp(), error)) {
        return QoreIRValue();
    }
    std::vector<QoreIRValue> operands;
    return lowerExprOpOrInvoke(QoreIROpcode::PopAny, expr, operands, op->loc, error);
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
    // Range lvalue (e.g., unshift list[0..2], val) - delegate entire expression to AST
    if (isRangeLValue(lvalue)) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, op->loc, error);
    }
    QoreIRValue right = lowerExpression(op->getRight(), error);
    if (!right.isValid()) {
        return QoreIRValue();
    }

    // Path-based unshift for native IR/AOT lvalues, including try/catch bodies
    // and nested hash/list member chains.  This avoids emitting
    // Invoke(UnshiftLValue), which has no source-free AOT lowering.
    QoreIRValue path_result = tryEmitLValuePathOp(QoreIROpcode::LValuePathBinaryMut,
        lvalue, &right, op->loc, error, false, LVCompoundOp::AddAssign,
        LVUnaryOp::PreInc, LVBinaryMutOp::Unshift);
    if (path_result.isValid()) {
        return path_result;
    }

    if (!guardLValueBase(lvalue, error)) {
        return QoreIRValue();
    }
    if (!exception_stack.empty()) {
        QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
        if (!normal_block) {
            error = "IR builder failed to create invoke continuation block";
            return QoreIRValue();
        }
        QoreIRBasicBlock* handler = exception_stack.back();
        auto* inst = builder.createInvoke(expr, {right}, normal_block, handler, op->loc);
        inst->invoke_opcode = QoreIROpcode::UnshiftLValue;
        builder.setBlock(normal_block);
        return inst->result;
    }
    return builder.createLValueBinaryOp(QoreIROpcode::UnshiftLValue, lvalue, right, op->loc)->result;
}

QoreIRValue QoreIRLowering::lowerPush(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QorePushOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    QoreValue left_expr = op->getLeft();

    // Native ListPush path: emit Load + ListPush + Store for local and closure
    // variables to avoid AST delegation (which relies on AST nodes stripped in AOT)
    if (left_expr.hasNode()) {
        auto* var = dynamic_cast<const VarRefNode*>(left_expr.getInternalNode());
        if (var && var->ref.id
                && (var->getType() == VT_LOCAL || var->getType() == VT_CLOSURE
                    || var->getType() == VT_LOCAL_TS)) {
            bool is_closure = var->ref.id->closureUse()
                || var->getType() == VT_CLOSURE || var->getType() == VT_LOCAL_TS;

            // Lower the value to push first
            QoreIRValue push_val = lowerExpression(op->getRight(), error);
            if (!push_val.isValid()) {
                return QoreIRValue();
            }

            // Load current list
            QoreIRValue list_val = lowerExpression(left_expr, error);
            if (!list_val.isValid()) {
                return QoreIRValue();
            }

            if (!exception_stack.empty()) {
                // Invoke path: ListPush can throw (type errors, etc.)
                QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
                if (!normal_block) {
                    error = "IR builder failed to create invoke continuation block";
                    return QoreIRValue();
                }
                QoreIRBasicBlock* handler = exception_stack.back();
                auto* inst = builder.createInvoke(expr, {list_val, push_val}, normal_block, handler, op->loc);
                inst->invoke_opcode = QoreIROpcode::ListPush;
                // Set element type for proper coercion on auto-vivification
                const QoreTypeInfo* var_type = var->ref.id->getTypeInfo();
                inst->element_type = QoreTypeInfo::getUniqueReturnComplexList(var_type);
                if (!inst->element_type) {
                    inst->element_type = QoreTypeInfo::getUniqueReturnComplexSoftList(var_type);
                }
                if (!inst->element_type) {
                    inst->element_type = QoreTypeInfo::getReturnComplexListOrNothing(var_type);
                }
                builder.setBlock(normal_block);

                // Store result back
                if (is_closure) {
                    auto* store_inst = builder.createStoreClosure(var->ref.id, inst->result, op->loc);
                    store_inst->exception_target = exception_stack.back();
                } else {
                    auto* store_inst = builder.createStoreLocal(var->ref.id, inst->result, op->loc);
                    store_inst->exception_target = exception_stack.back();
                }
                return inst->result;
            }

            // Normal path
            auto* push_inst = builder.createListPush(list_val, push_val, op->loc);
            // Set element type from the variable's list type for proper coercion
            // when auto-vivifying from NOTHING (e.g., list<softint> l; push l, "3")
            const QoreTypeInfo* var_type = var->ref.id->getTypeInfo();
            push_inst->element_type = QoreTypeInfo::getUniqueReturnComplexList(var_type);
            if (!push_inst->element_type) {
                push_inst->element_type = QoreTypeInfo::getUniqueReturnComplexSoftList(var_type);
            }
            if (!push_inst->element_type) {
                push_inst->element_type = QoreTypeInfo::getReturnComplexListOrNothing(var_type);
            }
            QoreIRValue result = push_inst->result;

            // Store result back (may be new list if auto-vivified from NOTHING)
            if (is_closure) {
                auto* store_inst = builder.createStoreClosure(var->ref.id, result, op->loc);
                if (!exception_stack.empty()) {
                    store_inst->exception_target = exception_stack.back();
                }
            } else {
                builder.createStoreLocal(var->ref.id, result, op->loc);
            }
            return result;
        }
    }

    // Path-based push for complex lvalues (member chains, nested subscripts)
    {
        QoreIRValue push_val = lowerExpression(op->getRight(), error);
        if (!push_val.isValid()) {
            return QoreIRValue();
        }
        QoreIRValue path_result = tryEmitLValuePathOp(QoreIROpcode::LValuePathBinaryMut,
            left_expr, &push_val, op->loc, error, false, LVCompoundOp::AddAssign,
            LVUnaryOp::PreInc, LVBinaryMutOp::Push);
        if (path_result.isValid()) {
            return path_result;
        }
    }

    // Fallback: delegate to AST for non-local lvalues (global, complex lvalues)
    if (!guardLValueBase(left_expr, error, true)) {
        return QoreIRValue();
    }
    std::vector<QoreIRValue> operands;
    return lowerExprOpOrInvoke(QoreIROpcode::PushAny, expr, operands, op->loc, error);
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
    // Range lvalue (e.g., splice list[0..2], offset, len) - delegate entire expression to AST
    if (isRangeLValue(lvalue)) {
        error = "unsupported range lvalue for native splice lowering";
        return QoreIRValue();
    }

    std::vector<LVPathStep> lv_path;
    std::vector<QoreValue> dynamic_operands;
    if (!extractLValuePath(lvalue, lv_path, dynamic_operands, /*allow_slice=*/false)
            || lv_path.empty()) {
        error = "unsupported lvalue for native splice lowering";
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

    std::vector<QoreIRValue> dyn_vals;
    size_t lowered_dyn = 0;
    for (auto& dop : dynamic_operands) {
        if (++lowered_dyn % 100 == 0 && qore_check_cancel(nullptr, "IR splice lvalue operand lowering")) {
            error = "IR splice lvalue operand lowering cancelled";
            return QoreIRValue();
        }
        QoreIRValue dv = lowerExpression(dop, error);
        if (!dv.isValid()) {
            return QoreIRValue();
        }
        dyn_vals.push_back(dv);
    }
    uint32_t dyn_idx = 0;
    size_t walked_steps = 0;
    for (auto& step : lv_path) {
        if (++walked_steps % 100 == 0 && qore_check_cancel(nullptr, "IR splice lvalue path lowering")) {
            error = "IR splice lvalue path lowering cancelled";
            return QoreIRValue();
        }
        if ((step.kind == LVPathStepKind::HashKey || step.kind == LVPathStepKind::ListIndex)
                && step.operand_idx == UINT32_MAX) {
            if (dyn_idx < dyn_vals.size()) {
                step.operand_idx = dyn_vals[dyn_idx].id;
                ++dyn_idx;
            }
        }
    }

    auto* path_inst = builder.getBlock()->appendInstruction<QoreIRLValuePathInstruction>(
        QoreIROpcode::LValuePathTernary);
    path_inst->result = builder.getFunction()->createValue();
    path_inst->path = std::move(lv_path);
    path_inst->ternary_op = LVTernaryOp::Splice;
    path_inst->loc = op->loc;
    path_inst->ref_rv = op->needsReturnValue();
    if (QoreIRBasicBlock* handler = getCurrentExceptionTarget()) {
        path_inst->exception_target = handler;
    }
    path_inst->operands.push_back(offset);
    path_inst->operands.push_back(length);
    path_inst->operands.push_back(replacement);
    for (size_t i = 0; i < dyn_vals.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "IR splice operand attachment")) {
            error = "IR splice operand attachment cancelled";
            return QoreIRValue();
        }
        auto& dv = dyn_vals[i];
        path_inst->operands.push_back(dv);
    }
    return path_inst->result;
}

QoreIRValue QoreIRLowering::lowerExtract(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreExtractOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    // Try LValuePathTernary for complex lvalues (hash member, self member, etc.).
    // Extract does not support slice lvalues — the parser already rejects them
    // at parse time, but we also gate at extractLValuePath with allow_slice=false
    // to prevent any future relaxation from silently corrupting runtime state.
    {
        std::vector<LVPathStep> lv_path;
        std::vector<QoreValue> dynamic_operands;
        if (extractLValuePath(op->getLValue(), lv_path, dynamic_operands, /*allow_slice=*/false)
                && !lv_path.empty()) {
            // Lower the ternary operands: offset, length, replacement
            QoreIRValue offset_val = lowerExpression(op->getOffset(), error);
            if (!offset_val.isValid()) {
                return QoreIRValue();
            }
            QoreIRValue length_val = op->getLength().isNothing()
                ? builder.createConstNothing(op->loc)->result
                : lowerExpression(op->getLength(), error);
            if (!length_val.isValid()) {
                return QoreIRValue();
            }
            QoreIRValue replacement_val = op->getNewValue().isNothing()
                ? builder.createConstNothing(op->loc)->result
                : lowerExpression(op->getNewValue(), error);
            if (!replacement_val.isValid()) {
                return QoreIRValue();
            }
            // Lower dynamic key/index operands from the lvalue path
            std::vector<QoreIRValue> dyn_vals;
            for (auto& dop : dynamic_operands) {
                QoreIRValue dv = lowerExpression(dop, error);
                if (!dv.isValid()) {
                    return QoreIRValue();
                }
                dyn_vals.push_back(dv);
            }
            // Assign operand indices to dynamic path steps
            uint32_t dyn_idx = 0;
            for (auto& step : lv_path) {
                if ((step.kind == LVPathStepKind::HashKey || step.kind == LVPathStepKind::ListIndex)
                        && step.operand_idx == UINT32_MAX) {
                    if (dyn_idx < dyn_vals.size()) {
                        step.operand_idx = dyn_vals[dyn_idx].id;
                        ++dyn_idx;
                    }
                } else if (step.kind == LVPathStepKind::HashKeySlice
                        || step.kind == LVPathStepKind::ListIndexSlice
                        || step.kind == LVPathStepKind::ListRangeSlice) {
                    for (uint32_t& sid : step.slice_operand_ids) {
                        if (dyn_idx < dyn_vals.size()) {
                            sid = dyn_vals[dyn_idx].id;
                            ++dyn_idx;
                        }
                    }
                }
            }
            // Create LValuePathTernary instruction
            auto* path_inst = builder.getBlock()->appendInstruction<QoreIRLValuePathInstruction>(
                QoreIROpcode::LValuePathTernary);
            path_inst->result = builder.getFunction()->createValue();
            path_inst->path = std::move(lv_path);
            path_inst->ternary_op = LVTernaryOp::Extract;
            path_inst->loc = op->loc;
            // Copy ref_rv flag
            path_inst->ref_rv = op->needsReturnValue();
            // Operands: [0]=offset, [1]=length, [2]=replacement, then dynamic key/index operands
            path_inst->operands.push_back(offset_val);
            path_inst->operands.push_back(length_val);
            path_inst->operands.push_back(replacement_val);
            for (auto& dv : dyn_vals) {
                path_inst->operands.push_back(dv);
            }
            return path_inst->result;
        }
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
    QoreIROpcode opcode = QoreIROpcode::ExtractAny;
    QoreParseAnalysis analysis;
    if (getAnalysis(op->getLValue(), analysis)
        && analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
        && analysis.known_type) {
        if (QoreTypeInfo::isType(analysis.known_type, NT_LIST)) {
            opcode = QoreIROpcode::ExtractList;
        } else if (QoreTypeInfo::isType(analysis.known_type, NT_STRING)) {
            opcode = QoreIROpcode::ExtractString;
        } else if (QoreTypeInfo::isType(analysis.known_type, NT_BINARY)) {
            opcode = QoreIROpcode::ExtractBinary;
        }
    }
    return lowerExprOpOrInvoke(opcode, expr, operands, op->loc, error);
}

QoreIRValue QoreIRLowering::lowerRemove(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreRemoveOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    // Path-based remove for all lvalue types — avoids EXPR_TREE serialization
    {
        QoreIRValue path_result = tryEmitLValuePathOp(QoreIROpcode::LValuePathUnary,
            op->getExp(), nullptr, op->loc, error, false, LVCompoundOp::AddAssign, LVUnaryOp::Remove);
        if (path_result.isValid()) {
            markLocalUnassignmentFromExpression(op->getExp());
            // When the return value is not used (ExpressionStatement), invalidate the
            // result slot so the IR interpreter discards the removed value immediately
            // instead of deferring to DiscardTemps.  This is critical for weak-reference
            // patterns where the destructor must fire as soon as the last strong ref is
            // dropped — even a one-instruction delay to DiscardTemps can cause hangs
            // (thread-object.qtest transparent thread test pattern).
            if (!op->needsReturnValue()) {
                auto& insts = builder.getBlock()->instructions;
                if (!insts.empty()) {
                    auto* path_inst = dynamic_cast<QoreIRLValuePathInstruction*>(insts.back().get());
                    if (path_inst) {
                        path_inst->result = QoreIRValue();  // invalid → IR interpreter discards immediately
                    }
                }
            }
            return path_result;
        }
    }
    QoreIRValue operand = lowerExpression(op->getExp(), error);
    if (!operand.isValid()) {
        return QoreIRValue();
    }
    std::vector<QoreIRValue> operands{operand};
    QoreIROpcode opcode = QoreIROpcode::RemoveAny;
    QoreParseAnalysis analysis;
    if (getAnalysis(op->getExp(), analysis)
        && analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
        && analysis.known_type) {
        if (QoreTypeInfo::isType(analysis.known_type, NT_LIST)) {
            opcode = QoreIROpcode::RemoveList;
        } else if (QoreTypeInfo::isType(analysis.known_type, NT_HASH)) {
            opcode = QoreIROpcode::RemoveHash;
        } else if (QoreTypeInfo::isType(analysis.known_type, NT_OBJECT)) {
            opcode = QoreIROpcode::RemoveObject;
        } else if (QoreTypeInfo::isType(analysis.known_type, NT_STRING)) {
            opcode = QoreIROpcode::RemoveString;
        } else if (QoreTypeInfo::isType(analysis.known_type, NT_BINARY)) {
            opcode = QoreIROpcode::RemoveBinary;
        }
    }
    QoreIRValue result = lowerExprOpOrInvoke(opcode, expr, operands, op->loc, error);
    markLocalUnassignmentFromExpression(op->getExp());
    return result;
}

QoreIRValue QoreIRLowering::lowerDelete(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreDeleteOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    // Path-based delete for complex lvalues (must be before lowerExpression)
    {
        QoreIRValue path_result = tryEmitLValuePathOp(QoreIROpcode::LValuePathUnary,
            op->getExp(), nullptr, op->loc, error, false, LVCompoundOp::AddAssign, LVUnaryOp::Delete);
        if (path_result.isValid()) {
            markLocalUnassignmentFromExpression(op->getExp());
            return path_result;
        }
    }
    QoreIRValue operand = lowerExpression(op->getExp(), error);
    if (!operand.isValid()) {
        return QoreIRValue();
    }
    std::vector<QoreIRValue> operands{operand};
    QoreIROpcode opcode = QoreIROpcode::RemoveAny;
    QoreParseAnalysis analysis;
    if (getAnalysis(op->getExp(), analysis)
        && analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
        && analysis.known_type) {
        if (QoreTypeInfo::isType(analysis.known_type, NT_LIST)) {
            opcode = QoreIROpcode::RemoveList;
        } else if (QoreTypeInfo::isType(analysis.known_type, NT_HASH)) {
            opcode = QoreIROpcode::RemoveHash;
        } else if (QoreTypeInfo::isType(analysis.known_type, NT_OBJECT)) {
            opcode = QoreIROpcode::RemoveObject;
        } else if (QoreTypeInfo::isType(analysis.known_type, NT_STRING)) {
            opcode = QoreIROpcode::RemoveString;
        } else if (QoreTypeInfo::isType(analysis.known_type, NT_BINARY)) {
            opcode = QoreIROpcode::RemoveBinary;
        }
    }
    QoreIRValue result = lowerExprOpOrInvoke(opcode, expr, operands, op->loc, error);
    markLocalUnassignmentFromExpression(op->getExp());
    if (!result.isValid()) {
        return result;
    }
    return builder.createConstNothing(op->loc)->result;
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
    QoreIROpcode opcode = QoreIROpcode::KeysAny;
    QoreParseAnalysis analysis;
    if (getAnalysis(op->getExp(), analysis)
        && analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
        && analysis.known_type) {
        if (QoreTypeInfo::isType(analysis.known_type, NT_LIST)) {
            opcode = QoreIROpcode::KeysList;
        } else if (QoreTypeInfo::isType(analysis.known_type, NT_HASH)) {
            opcode = QoreIROpcode::KeysHash;
        }
    }
    return lowerExprOpOrInvoke(opcode, expr, operands, op->loc, error);
}

QoreIRValue QoreIRLowering::lowerRegexNMatch(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreRegexNMatchOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    QoreIRValue operand = lowerExpression(op->getExp(), error);
    if (!operand.isValid()) {
        return QoreIRValue();
    }
    std::vector<QoreIRValue> operands{operand};
    return lowerExprOpOrInvoke(QoreIROpcode::RegexNMatchBool, expr, operands, op->loc, error);
}

QoreIRValue QoreIRLowering::lowerRegexMatch(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreRegexMatchOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    QoreIRValue operand = lowerExpression(op->getExp(), error);
    if (!operand.isValid()) {
        return QoreIRValue();
    }
    std::vector<QoreIRValue> operands{operand};
    QoreIROpcode opcode = QoreIROpcode::RegexMatchAny;
    QoreParseAnalysis analysis;
    if (getAnalysis(expr, analysis)
        && analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
        && analysis.hasFlag(QoreParseAnalysis::NeverNothing)
        && analysis.known_type
        && QoreTypeInfo::isType(analysis.known_type, NT_BOOLEAN)) {
        opcode = QoreIROpcode::RegexMatchBool;
    }
    return lowerExprOpOrInvoke(opcode, expr, operands, op->loc, error);
}

QoreIRValue QoreIRLowering::lowerRegexExtract(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreRegexExtractOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    QoreIRValue operand = lowerExpression(op->getExp(), error);
    if (!operand.isValid()) {
        return QoreIRValue();
    }
    std::vector<QoreIRValue> operands{operand};
    QoreIROpcode opcode = QoreIROpcode::RegexExtractAny;
    QoreParseAnalysis analysis;
    if (getAnalysis(expr, analysis)
        && analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
        && analysis.known_type
        && QoreTypeInfo::parseReturns(analysis.known_type, NT_LIST) != QTI_NOT_EQUAL) {
        opcode = QoreIROpcode::RegexExtractList;
    }
    return lowerExprOpOrInvoke(opcode, expr, operands, op->loc, error);
}

QoreIRValue QoreIRLowering::lowerInstanceOf(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreInstanceOfOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    QoreIRValue operand = lowerExpression(op->getExp(), error);
    if (!operand.isValid()) {
        return QoreIRValue();
    }
    std::vector<QoreIRValue> operands{operand};
    return lowerExprOpOrInvoke(QoreIROpcode::InstanceOfBool, expr, operands, op->loc, error);
}

QoreIRValue QoreIRLowering::lowerTrim(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreTrimOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    // Path-based trim for complex lvalues — avoids EXPR_TREE serialization
    {
        QoreIRValue path_result = tryEmitLValuePathOp(QoreIROpcode::LValuePathUnary,
            op->getExp(), nullptr, op->loc, error, false, LVCompoundOp::AddAssign, LVUnaryOp::Trim);
        if (path_result.isValid()) {
            return path_result;
        }
    }
    std::vector<QoreIRValue> operands;
    QoreIROpcode opcode = QoreIROpcode::TrimAny;
    QoreParseAnalysis analysis;
    if (getAnalysis(op->getExp(), analysis)
        && analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
        && analysis.hasFlag(QoreParseAnalysis::NeverNothing)
        && analysis.known_type
        && QoreTypeInfo::isType(analysis.known_type, NT_STRING)) {
        opcode = QoreIROpcode::TrimString;
    }
    return lowerExprOpOrInvoke(opcode, expr, operands, op->loc, error);
}

QoreIRValue QoreIRLowering::lowerChomp(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreChompOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    // Path-based chomp for complex lvalues (mirrors lowerTrim) — avoids the
    // AST-eval fallback in the IR interpreter / JIT runtime on simple lvalues.
    {
        QoreIRValue path_result = tryEmitLValuePathOp(QoreIROpcode::LValuePathUnary,
            op->getExp(), nullptr, op->loc, error, false, LVCompoundOp::AddAssign, LVUnaryOp::Chomp);
        if (path_result.isValid()) {
            return path_result;
        }
    }
    std::vector<QoreIRValue> operands;
    QoreIROpcode opcode = QoreIROpcode::ChompAny;
    QoreParseAnalysis analysis;
    if (getAnalysis(op->getExp(), analysis)
        && analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
        && analysis.hasFlag(QoreParseAnalysis::NeverNothing)
        && analysis.known_type
        && QoreTypeInfo::isType(analysis.known_type, NT_STRING)) {
        opcode = QoreIROpcode::ChompString;
    }
    return lowerExprOpOrInvoke(opcode, expr, operands, op->loc, error);
}

QoreIRValue QoreIRLowering::lowerTransliteration(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreTransliterationOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    // Path-based transliteration for complex lvalues — avoids EXPR_TREE serialization
    {
        QoreIRValue path_result = tryEmitLValuePathOp(QoreIROpcode::LValuePathBinaryMut,
            op->getExp(), nullptr, op->loc, error, false, LVCompoundOp::AddAssign,
            LVUnaryOp::PreInc, LVBinaryMutOp::Transliterate, expr);
        if (path_result.isValid()) {
            return path_result;
        }
    }
    std::vector<QoreIRValue> operands;
    QoreIROpcode opcode = QoreIROpcode::TransliterateAny;
    QoreParseAnalysis analysis;
    if (getAnalysis(op->getExp(), analysis)
        && analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
        && analysis.hasFlag(QoreParseAnalysis::NeverNothing)
        && analysis.known_type
        && QoreTypeInfo::isType(analysis.known_type, NT_STRING)) {
        opcode = QoreIROpcode::TransliterateString;
    } else if (op->getExp().isValue()) {
        const QoreValue& value = op->getExp();
        if (value.getType() == NT_STRING) {
            opcode = QoreIROpcode::TransliterateString;
        }
    }
    return lowerExprOpOrInvoke(opcode, expr, operands, op->loc, error);
}

QoreIRValue QoreIRLowering::lowerBackground(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreBackgroundOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }

    // Decomposed paths — pre-evaluate args (and receiver/callref where applicable)
    // so the spawned thread's code runs with IR-evaluated operands instead of
    // depending on a raw ParseNode*.
    //
    // Operand layouts by inner-expression type:
    //   SelfFunctionCallNode       → [arg...] or [ConstNothing] sentinel for zero args
    //   FunctionCallNode           → [arg...] or [ConstNothing] sentinel for zero args
    //   StaticMethodCallNode       → [arg...] or [ConstNothing] sentinel for zero args
    //   QoreDotEvalOperatorNode    → [receiver, arg...]  (always ≥ 1 operand)
    //   CallReferenceCallNode      → [callref, arg...]   (always ≥ 1 operand)
    //
    // An empty operand list is rejected by LLVM/AOT lowering; no AST fallback is emitted.
    QoreValue inner_expr = op->getExp();
    if (inner_expr.hasNode()) {
        const AbstractQoreNode* inner = inner_expr.getInternalNode();

        // background self.method(args)
        if (auto* sfcn = dynamic_cast<const SelfFunctionCallNode*>(inner)) {
            if (sfcn->getMethod() && !sfcn->getMethod()->isStatic()) {
                std::vector<QoreIRValue> operands;
                if (lowerCallArgs(sfcn->getParseArgs(), sfcn->getArgs(), operands, error)) {
                    if (operands.empty()) {
                        operands.push_back(builder.createConstNothing(op->loc)->result);
                    }
                    return lowerExprOpOrInvoke(QoreIROpcode::BackgroundInt, expr,
                        operands, op->loc, error);
                }
                error.clear();
            }
        }
        // background foo(args) — free function call
        else if (auto* fcn = dynamic_cast<const FunctionCallNode*>(inner)) {
            if (fcn->getFunction()) {
                std::vector<QoreIRValue> operands;
                if (lowerCallArgs(fcn->getParseArgs(), fcn->getArgs(), operands, error)) {
                    if (operands.empty()) {
                        operands.push_back(builder.createConstNothing(op->loc)->result);
                    }
                    return lowerExprOpOrInvoke(QoreIROpcode::BackgroundInt, expr,
                        operands, op->loc, error);
                }
                error.clear();
            }
        }
        // background Class::staticMethod(args)
        else if (auto* smcn = dynamic_cast<const StaticMethodCallNode*>(inner)) {
            std::string class_path = smcn->getClassPath();
            if (smcn->getMethod() || !class_path.empty()) {
                std::vector<QoreIRValue> operands;
                if (lowerCallArgs(smcn->getParseArgs(), smcn->getArgs(), operands, error)) {
                    if (!smcn->getMethod()) {
                        std::string qualified_name = class_path;
                        qualified_name += "::";
                        qualified_name += smcn->getName();
                        auto* inst = builder.createBackground(QoreIRBackgroundKind::StaticMethod,
                            qualified_name, expr, operands, op->loc);
                        if (!exception_stack.empty()) {
                            inst->exception_target = exception_stack.back();
                        }
                        maybeInsertNotNothingGuard(inst->result, &expr, op->loc, nullptr);
                        return inst->result;
                    }
                    if (operands.empty()) {
                        operands.push_back(builder.createConstNothing(op->loc)->result);
                    }
                    return lowerExprOpOrInvoke(QoreIROpcode::BackgroundInt, expr,
                        operands, op->loc, error);
                }
                error.clear();
            }
        }
        // background obj.method(args)
        else if (auto* devn = dynamic_cast<const QoreDotEvalOperatorNode*>(inner)) {
            MethodCallNode* m = devn->getMethodCall();
            if (m && m->getName()) {
                QoreIRValue receiver_val = lowerExpression(devn->getExpression(), error);
                if (receiver_val.isValid()) {
                    std::vector<QoreIRValue> operands;
                    operands.push_back(receiver_val);
                    if (lowerCallArgs(m->getParseArgs(), m->getArgs(), operands, error)) {
                        auto* inst = builder.createBackground(QoreIRBackgroundKind::DotEval,
                            m->getName(), expr, operands, op->loc);
                        if (!exception_stack.empty()) {
                            inst->exception_target = exception_stack.back();
                        }
                        maybeInsertNotNothingGuard(inst->result, &expr, op->loc, nullptr);
                        return inst->result;
                    }
                    error.clear();
                } else {
                    error.clear();
                }
            }
        }
        // background callref(args) — closure / call-ref / method-ref invocation
        else if (auto* crcn = dynamic_cast<const CallReferenceCallNode*>(inner)) {
            QoreIRValue callee_val = lowerExpression(crcn->getExp(), error);
            if (callee_val.isValid()) {
                std::vector<QoreIRValue> operands;
                operands.push_back(callee_val);
                if (lowerCallArgs(crcn->getParseArgs(), crcn->getArgs(), operands, error)) {
                    return lowerExprOpOrInvoke(QoreIROpcode::BackgroundInt, expr,
                        operands, op->loc, error);
                }
                error.clear();
            } else {
                error.clear();
            }
        }
    }

    // Fallback: full AST evaluation path
    std::vector<QoreIRValue> operands;
    return lowerExprOpOrInvoke(QoreIROpcode::BackgroundInt, expr, operands, op->loc, error);
}

QoreIRValue QoreIRLowering::lowerListAssignment(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreListAssignmentOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    // Decompose list assignment into per-entry extraction + lvalue stores.
    // The RHS is evaluated exactly once; qore_rt_list_assignment_value preserves
    // AST semantics for non-list RHS values (first LHS receives the value, the
    // remaining LHS entries receive NOTHING).
    QoreValue lhs = op->getLeft();
    if (lhs.getType() != NT_PARSE_LIST) {
        error = "unsupported list assignment lowering: left-hand side is not a parse list";
        return QoreIRValue();
    }
    const auto* parse_list = lhs.get<const QoreParseListNode>();

    // Lower RHS expression
    QoreIRValue rhs_val = lowerExpression(op->getRight(), error);
    if (!rhs_val.isValid()) {
        return QoreIRValue();
    }
    // For each LHS variable, extract element from list and store
    for (size_t i = 0; i < parse_list->size(); ++i) {
        QoreValue entry = parse_list->get(i);
        // Create constant index
        QoreIRValue idx = builder.createConstInt(static_cast<int64_t>(i), op->loc)->result;
        QoreIRValue element_val = builder.createExprOp(QoreIROpcode::ListAssignAny, expr,
            {rhs_val, idx}, op->loc)->result;

        const auto* var = entry.getType() == NT_VARREF ? entry.get<const VarRefNode>() : nullptr;
        if (var && var->getType() != VT_IMMEDIATE
                && !(var->getTypeInfo() && QoreTypeInfo::isReference(var->getTypeInfo()))) {
            if (!storeVarRef(var, element_val, error, "list-assignment")) {
                return QoreIRValue();
            }
        } else if (entry.hasNode()) {
            QoreIRValue path_result = tryEmitLValuePathOp(QoreIROpcode::LValuePathAssign,
                entry, &element_val, op->loc, error, false);
            if (!path_result.isValid()) {
                if (error.empty()) {
                    error = "unsupported list assignment lvalue for native lowering: ";
                    error += entry.getInternalNode()->getTypeName();
                }
                return QoreIRValue();
            }
        } else {
            error = "unsupported list assignment lvalue for native lowering: ";
            error += entry.getTypeName();
            return QoreIRValue();
        }
    }
    // Return RHS if return value is needed, otherwise NOTHING
    if (op->needsReturnValue()) {
        return rhs_val;
    }
    return builder.createConstNothing(op->loc)->result;
}

QoreIRValue QoreIRLowering::lowerRegexSubst(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreRegexSubstOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    // Path-based regex subst for complex lvalues
    {
        QoreIRValue path_result = tryEmitLValuePathOp(QoreIROpcode::LValuePathBinaryMut,
            op->getExp(), nullptr, op->loc, error, false, LVCompoundOp::AddAssign,
            LVUnaryOp::PreInc, LVBinaryMutOp::RegexSubst, expr);
        if (path_result.isValid()) {
            return path_result;
        }
    }
    QoreIRValue operand = lowerExpression(op->getExp(), error);
    if (!operand.isValid()) {
        return QoreIRValue();
    }
    std::vector<QoreIRValue> operands{operand};
    QoreIROpcode opcode = QoreIROpcode::RegexSubstAny;
    QoreParseAnalysis analysis;
    if (getAnalysis(expr, analysis)
        && analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
        && analysis.known_type
        && QoreTypeInfo::parseReturns(analysis.known_type, NT_STRING) != QTI_NOT_EQUAL) {
        opcode = QoreIROpcode::RegexSubstString;
    }
    return lowerExprOpOrInvoke(opcode, expr, operands, op->loc, error);
}

QoreIRValue QoreIRLowering::lowerContainerLiteral(const QoreValue& expr, std::string& error) {
    if (!expr.hasNode()) {
        return QoreIRValue();
    }

    const AbstractQoreNode* node = expr.getInternalNode();
    if (dynamic_cast<const QoreParseHashNode*>(node)) {
        return lowerParseHash(expr, error);
    }
    if (dynamic_cast<const QoreParseListNode*>(node)) {
        return lowerParseList(expr, error);
    }

    if (auto* hash = dynamic_cast<const QoreHashNode*>(node)) {
        if (hash->getHashDecl()) {
            return builder.createLoadConstant(nullptr, expr, nullptr)->result;
        }

        std::vector<std::string> keys;
        std::vector<QoreIRValue> values;
        keys.reserve(hash->size());
        values.reserve(hash->size());
        ConstHashIterator it(hash);
        while (it.next()) {
            const char* key = it.getKey();
            keys.push_back(key ? key : "");
            QoreIRValue value = lowerContainerElement(it.get(), error);
            if (!value.isValid()) {
                return QoreIRValue();
            }
            values.push_back(value);
        }

        const QoreTypeInfo* cti = qore_hash_private::get(*hash)->complexTypeInfo;
        if (cti == autoHashTypeInfo) {
            cti = nullptr;
        }
        return builder.createMakeHashConstKeys(std::move(keys), values, nullptr, cti)->result;
    }

    if (auto* list = dynamic_cast<const QoreListNode*>(node)) {
        std::vector<QoreIRValue> values;
        values.reserve(list->size());
        for (size_t i = 0; i < list->size(); ++i) {
            QoreIRValue value = lowerContainerElement(list->retrieveEntry(i), error);
            if (!value.isValid()) {
                return QoreIRValue();
            }
            values.push_back(value);
        }

        const QoreTypeInfo* cti = qore_list_private::get(*list)->complexTypeInfo;
        if (cti == autoListTypeInfo) {
            cti = nullptr;
        }
        return builder.createMakeList(values, nullptr, cti)->result;
    }

    return QoreIRValue();
}

QoreIRValue QoreIRLowering::lowerContainerElement(const QoreValue& expr, std::string& error) {
    QoreIRValue value = lowerContainerLiteral(expr, error);
    if (value.isValid() || !error.empty()) {
        return value;
    }

    value = lowerConstant(expr, error);
    if (value.isValid() || !error.empty()) {
        return value;
    }

    return lowerExpression(expr, error);
}

QoreIRValue QoreIRLowering::lowerParseHash(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* hash = dynamic_cast<const QoreParseHashNode*>(node);
    if (!hash) {
        return QoreIRValue();
    }
    const auto& keys = hash->getKeys();
    const auto& values_vec = hash->getValues();
    if (keys.size() != values_vec.size()) {
        error = "parse hash node key/value size mismatch";
        return QoreIRValue();
    }

    // Check if all keys are constant strings — use optimized MakeHashConstKeys
    bool all_const_keys = true;
    std::vector<std::string> const_keys;
    const_keys.reserve(keys.size());
    for (size_t i = 0; i < keys.size(); ++i) {
        if (keys[i].getType() == NT_STRING) {
            QoreStringValueHelper key(keys[i]);
            const_keys.push_back(key->c_str());
        } else {
            all_const_keys = false;
            break;
        }
    }

    // Get parse-time type info from the QoreParseHashNode
    // Pass nullptr for auto types so the IR interpreter computes from runtime values
    // (lvalues can return NOTHING despite their declared type)
    const QoreTypeInfo* parse_ti = hash->getParseTypeInfo();
    if (parse_ti == autoHashTypeInfo) {
        parse_ti = nullptr;
    }

    if (all_const_keys) {
        // Optimized path: only value operands, key names embedded in instruction
        std::vector<QoreIRValue> value_operands;
        value_operands.reserve(keys.size());
        for (size_t i = 0; i < values_vec.size(); ++i) {
            QoreIRValue value = lowerContainerElement(values_vec[i], error);
            if (!value.isValid()) {
                return QoreIRValue();
            }
            value_operands.push_back(value);
        }
        return builder.createMakeHashConstKeys(std::move(const_keys), value_operands, hash->loc, parse_ti)->result;
    }

    // General path: alternating key-value operands
    std::vector<QoreIRValue> operands;
    operands.reserve(keys.size() * 2);
    for (size_t i = 0; i < keys.size(); ++i) {
        QoreIRValue key = lowerExpression(keys[i], error);
        if (!key.isValid()) {
            return QoreIRValue();
        }
        QoreIRValue value = lowerContainerElement(values_vec[i], error);
        if (!value.isValid()) {
            return QoreIRValue();
        }
        operands.push_back(key);
        operands.push_back(value);
    }
    return builder.createMakeHash(operands, hash->loc, parse_ti)->result;
}

QoreIRValue QoreIRLowering::lowerParseList(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    // Phase 3b diag: trace the pre-dynamic_cast pointer so a crashing
    // input at this site reveals the file + line of the triggering
    // expression instead of a bare SIGSEGV.  Gated on
    // QORE_AOT_TRACE_LOWER_PARSELIST.
    if (node && getenv("QORE_AOT_TRACE_LOWER_PARSELIST")) {
        fprintf(stderr, "[aot-trace] lowerParseList node=%p type=%d\n",
            (const void*)node, (int)expr.getType());
        fflush(stderr);
    }
    auto* list = dynamic_cast<const QoreParseListNode*>(node);
    if (!list) {
        return QoreIRValue();
    }
    std::vector<QoreIRValue> values;
    values.reserve(list->size());
    for (size_t i = 0; i < list->size(); ++i) {
        QoreIRValue value = lowerContainerElement(list->get(i), error);
        if (!value.isValid()) {
            return QoreIRValue();
        }
        values.push_back(value);
    }
    // Get parse-time type info from the QoreParseListNode
    // Pass nullptr for auto/unspecific types so the IR interpreter computes from runtime values
    // (lvalues can return NOTHING despite their declared type)
    const QoreTypeInfo* parse_ti = list->getParseTypeInfo();
    if (parse_ti == autoListTypeInfo) {
        parse_ti = nullptr;
    } else if (parse_ti) {
        // Also clear for list types with auto element types (e.g., list<hash<auto>>)
        const QoreTypeInfo* elem_type = QoreTypeInfo::getUniqueReturnComplexList(parse_ti);
        if (elem_type && (elem_type == autoTypeInfo || elem_type == autoHashTypeInfo
            || elem_type == autoHashOrNothingTypeInfo)) {
            parse_ti = nullptr;
        }
    }
    return builder.createMakeList(values, list->loc, parse_ti)->result;
}

QoreIRValue QoreIRLowering::lowerExists(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreExistsOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    QoreIRValue operand = lowerExpression(op->getExp(), error);
    if (!operand.isValid()) {
        return QoreIRValue();
    }
    std::vector<QoreIRValue> operands{operand};
    QoreIROpcode opcode = QoreIROpcode::ExistsAny;
    QoreParseAnalysis analysis;
    if (getAnalysis(expr, analysis)
        && analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
        && analysis.hasFlag(QoreParseAnalysis::NeverNothing)
        && analysis.known_type
        && QoreTypeInfo::isType(analysis.known_type, NT_BOOLEAN)) {
        opcode = QoreIROpcode::ExistsBool;
    }
    return lowerExprOpOrInvoke(opcode, expr, operands, op->loc, error);
}

QoreIRValue QoreIRLowering::lowerElements(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreElementsOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    QoreIRValue operand = lowerExpression(op->getExp(), error);
    if (!operand.isValid()) {
        return QoreIRValue();
    }
    std::vector<QoreIRValue> operands{operand};
    QoreIROpcode opcode = QoreIROpcode::ElementsAny;
    QoreParseAnalysis analysis;
    if (getAnalysis(expr, analysis)
        && analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
        && analysis.hasFlag(QoreParseAnalysis::NeverNothing)
        && analysis.known_type
        && QoreTypeInfo::isType(analysis.known_type, NT_INT)) {
        opcode = QoreIROpcode::ElementsInt;
    }
    return lowerExprOpOrInvoke(opcode, expr, operands, op->loc, error);
}

static bool qore_ir_type_may_require_dot_eval_name_dispatch(const QoreTypeInfo* type_info) {
    if (!type_info || !QoreTypeInfo::hasType(type_info)) {
        return true;
    }
    for (const QoreReturnSpec& spec : type_info->return_vec) {
        switch (spec.spec.getType()) {
            case NT_OBJECT:
            case NT_HASH:
            case NT_WEAKREF:
            case NT_WEAKREF_HASH:
            case NT_REFERENCE:
            case NT_ALL:
                return true;
            default:
                break;
        }
    }
    return false;
}

QoreIRValue QoreIRLowering::lowerDotEval(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreDotEvalOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    if (QoreIRValue hoisted = findLoopInvariantValue(expr); hoisted.isValid()) {
        return hoisted;
    }
    MethodCallNode* m = op->getMethodCall();
    const QoreValue& base_expr = op->getExpression();
    LocalVar* base_local = getLocalVarFromValue(base_expr);
    bool self_base = base_local && !strcmp(base_local->getName(), "self");
    if (m && m->getName() && self_base) {
        const QoreMethod* method = m->getMethod();
        const QoreClass* qc = m->getClass();
        const AbstractQoreFunctionVariant* variant = m->getVariant();
        if (method && qc && qc->isFinal()
                && !overloadedDirectCallNeedsRuntimeDispatch(qore_method_private::get(*method)->getFunction(),
                    variant, m->getParseArgs(), m->getArgs())) {
            std::vector<QoreIRValue> lowered_args;
            if (lowerCallArgs(m->getParseArgs(), m->getArgs(), lowered_args, error)) {
                if (!exception_stack.empty()) {
                    QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
                    if (!normal_block) {
                        error = "IR builder failed to create invoke continuation block";
                        return QoreIRValue();
                    }
                    QoreIRBasicBlock* handler = exception_stack.back();
                    auto* invoke_inst = builder.createInvokeMethodDirect(method, qc, variant, lowered_args,
                        normal_block, handler, expr, op->loc);
                    builder.setBlock(normal_block);
                    return invoke_inst->result;
                }
                return builder.createCallMethodDirect(method, qc, variant, lowered_args, expr, op->loc)->result;
            }
            error.clear();
        }
    }
    if (m && m->isPseudo() && qoreIrCallHasNoArgs(m)) {
        const char* method_name = m->getName();
        if (method_name && !strcmp(method_name, "size")) {
            if (LocalVar* list_local = qoreIrGetHoistableListSizeLocal(expr)) {
                QoreIRValue hoisted_size = findLoopInvariantListSize(list_local);
                if (hoisted_size.isValid()) {
                    return hoisted_size;
                }
            }
        }
    }
    QoreIRValue base_val = lowerExpression(op->getExpression(), error);
    if (!base_val.isValid()) {
        return QoreIRValue();
    }

    // Pre-evaluate args and use DotEvalMethodDirect to avoid AST round-trip at runtime.
    // This handles both resolved methods (class+method set) and abstract/unresolved methods
    // (null class/method). For unresolved methods, the runtime dispatches by name using the
    // embedded expression. This is essential for AOT correctness: DotEvalAny evaluates args
    // from the EXPR_TREE which reads locals from TLS, but IR-only locals aren't on TLS.
    // copy() calls have getRawName() == nullptr but DO have method+class resolved.
    // Use getName() which returns "copy" when getRawName() is null, ensuring copy()
    // calls also produce DotEvalMethodDirect instead of falling back to DotEvalAny.
    if (m->getName()) {
        // Lower arguments
        std::vector<QoreIRValue> lowered_args;
        if (lowerCallArgs(m->getParseArgs(), m->getArgs(), lowered_args, error)) {
            if (m->isPseudo() && lowered_args.empty()) {
                const QoreMethod* pseudo_method = m->getMethod();
                const char* method_name = pseudo_method ? pseudo_method->getName() : m->getName();
                QoreParseAnalysis base_analysis;
                bool have_base_analysis = getAnalysis(base_expr, base_analysis);
                const QoreTypeInfo* base_analysis_type = have_base_analysis
                    && base_analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
                    ? selectAnalysisType(base_analysis) : nullptr;
                bool safe_to_bypass_name_dispatch = base_analysis_type
                    && base_analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
                    && !qore_ir_type_may_require_dot_eval_name_dispatch(base_analysis_type);
                if (pseudo_method && pseudo_method->getClass()
                        && !strcmp(pseudo_method->getClass()->getName(), "<list>")
                        && !strcmp(method_name, "size")) {
                    return builder.createListSize(base_val, op->loc)->result;
                }
                if (pseudo_method && pseudo_method->getClass() == QC_PSEUDOVALUE
                        && method_name && safe_to_bypass_name_dispatch) {
                    QoreIROpcode conversion_op = QoreIROpcode::ToBool;
                    bool recognized_conversion = true;
                    if (!strcmp(method_name, "toInt")) {
                        conversion_op = QoreIROpcode::ToInt;
                    } else if (!strcmp(method_name, "toFloat")) {
                        conversion_op = QoreIROpcode::ToFloat;
                    } else if (!strcmp(method_name, "toBool")) {
                        conversion_op = QoreIROpcode::ToBool;
                    } else if (!strcmp(method_name, "toString")) {
                        conversion_op = QoreIROpcode::ToString;
                    } else {
                        recognized_conversion = false;
                    }
                    if (recognized_conversion) {
                        QoreIRValue result = builder.createUnaryOp(conversion_op, base_val, op->loc)->result;
                        never_nothing_values.insert(result.id);
                        return result;
                    }
                }
            }

            // Build operands = [base, arg0, arg1, ...]
            std::vector<QoreIRValue> operands;
            operands.push_back(base_val);
            operands.insert(operands.end(), lowered_args.begin(), lowered_args.end());

            QoreIRValue result;
            bool should_invoke = !exception_stack.empty();
            QoreParseAnalysis base_analysis;
            bool have_base_analysis = m->isPseudo() && getAnalysis(base_expr, base_analysis);
            bool pseudo_base_known_string = have_base_analysis
                && base_analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
                && QoreTypeInfo::isType(selectAnalysisType(base_analysis), NT_STRING);
            bool pseudo_base_known_assigned_string = pseudo_base_known_string
                && (base_analysis.hasFlag(QoreParseAnalysis::NeverNothing)
                    || base_analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned));
            bool pseudo_arg0_known_string = false;
            bool pseudo_arg0_known_assigned_string = false;
            bool pseudo_arg0_known_assigned_int = false;
            bool pseudo_arg1_known_assigned_int = false;
            if (m->isPseudo() && !lowered_args.empty()) {
                QoreValue pseudo_args[2];
                bool have_pseudo_arg[2] = {false, false};
                const QoreParseListNode* parse_args = m->getParseArgs();
                const QoreListNode* args = m->getArgs();
                if (parse_args && parse_args->size()) {
                    size_t max_args = std::min<size_t>(parse_args->size(), 2);
                    for (size_t ai = 0; ai < max_args; ++ai) {
                        pseudo_args[ai] = parse_args->get(ai);
                        have_pseudo_arg[ai] = true;
                    }
                } else if (args && args->size()) {
                    const qore_list_private* args_priv = qore_list_private::get(args);
                    if (args_priv && args_priv->hasCallArgEvalMap()) {
                        const std::vector<size_t>* pos_map = args_priv->getCallArgEvalMap();
                        for (size_t si = 0; pos_map && si < pos_map->size(); ++si) {
                            if (si && !(si % 100) && qore_check_cancel(nullptr,
                                    "IR named pseudo call argument analysis")) {
                                error = "IR named pseudo call argument analysis cancelled or interrupted";
                                return QoreIRValue();
                            }
                            size_t logical_pos = (*pos_map)[si];
                            if (logical_pos < 2) {
                                pseudo_args[logical_pos] = args->retrieveEntry(si);
                                have_pseudo_arg[logical_pos] = true;
                            }
                        }
                    } else {
                        size_t max_args = std::min<size_t>(args->size(), 2);
                        for (size_t ai = 0; ai < max_args; ++ai) {
                            pseudo_args[ai] = args->retrieveEntry(ai);
                            have_pseudo_arg[ai] = true;
                        }
                    }
                }
                for (int ai = 0; ai < 2; ++ai) {
                    QoreParseAnalysis arg_analysis;
                    bool have_analysis = have_pseudo_arg[ai] && getAnalysis(pseudo_args[ai], arg_analysis)
                        && arg_analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo);
                    const QoreTypeInfo* arg_type = have_analysis ? selectAnalysisType(arg_analysis) : nullptr;
                    bool assigned = have_analysis
                        && (arg_analysis.hasFlag(QoreParseAnalysis::NeverNothing)
                            || arg_analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned));
                    bool known_assigned_int = assigned && QoreTypeInfo::isType(arg_type, NT_INT);
                    if (!ai) {
                        pseudo_arg0_known_string = have_analysis
                            && QoreTypeInfo::isType(arg_type, NT_STRING);
                        pseudo_arg0_known_assigned_string = assigned && pseudo_arg0_known_string;
                        pseudo_arg0_known_assigned_int = known_assigned_int;
                    } else {
                        pseudo_arg1_known_assigned_int = known_assigned_int;
                    }
                }
            }
            const QoreTypeInfo* pseudo_base_type = have_base_analysis
                && base_analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
                ? selectAnalysisType(base_analysis) : nullptr;
            bool pseudo_base_safe_value_dispatch = pseudo_base_type
                && !qore_ir_type_may_require_dot_eval_name_dispatch(pseudo_base_type);
            // Ordinary object dot-eval must use runtime name dispatch: the parse-time
            // method pointer can be a containing method object whose overload set does
            // not match the runtime overload lookup exactly after AOT deserialization.
            // Pseudo-methods remain direct so the existing fast paths are preserved.
            const QoreMethod* method = m->isPseudo() ? m->getMethod() : nullptr;
            const QoreClass* qc = m->isPseudo() ? m->getClass() : nullptr;
            const AbstractQoreFunctionVariant* variant = m->isPseudo() ? m->getVariant() : nullptr;
            // Always set fallback_method_name so consumers (LLVM codegen, IR interpreter,
            // AOT deserialization) don't need to extract it from the AST expr field.
            // The expr is still stored for the LLVM AOT slot system (call target resolution).
            if (should_invoke) {
                QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
                if (!normal_block) {
                    error = "IR builder failed to create invoke continuation block";
                    return QoreIRValue();
                }
                QoreIRBasicBlock* handler = exception_stack.back();
                auto* inst = builder.createInvokeDotEvalMethodDirect(method, qc, variant, expr, m->isPseudo(),
                    operands, normal_block, handler, op->loc);
                inst->fallback_method_name = strdup(m->getName());
                inst->explicit_type_param_inst = m->getExplicitTypeParamInstantiation();
                inst->pseudo_base_known_string = pseudo_base_known_string;
                inst->pseudo_base_known_assigned_string = pseudo_base_known_assigned_string;
                inst->pseudo_arg0_known_string = pseudo_arg0_known_string;
                inst->pseudo_arg0_known_assigned_string = pseudo_arg0_known_assigned_string;
                inst->pseudo_arg0_known_assigned_int = pseudo_arg0_known_assigned_int;
                inst->pseudo_arg1_known_assigned_int = pseudo_arg1_known_assigned_int;
                inst->pseudo_base_safe_value_dispatch = pseudo_base_safe_value_dispatch;
                builder.setBlock(normal_block);
                result = inst->result;
            } else {
                auto* inst = builder.createDotEvalMethodDirect(method, qc, variant, expr, m->isPseudo(),
                    operands, op->loc);
                inst->fallback_method_name = strdup(m->getName());
                inst->explicit_type_param_inst = m->getExplicitTypeParamInstantiation();
                inst->pseudo_base_known_string = pseudo_base_known_string;
                inst->pseudo_base_known_assigned_string = pseudo_base_known_assigned_string;
                inst->pseudo_arg0_known_string = pseudo_arg0_known_string;
                inst->pseudo_arg0_known_assigned_string = pseudo_arg0_known_assigned_string;
                inst->pseudo_arg0_known_assigned_int = pseudo_arg0_known_assigned_int;
                inst->pseudo_arg1_known_assigned_int = pseudo_arg1_known_assigned_int;
                inst->pseudo_base_safe_value_dispatch = pseudo_base_safe_value_dispatch;
                result = inst->result;
            }
            return result;
        }
        // lowerCallArgs failed — fall through to generic path
        error.clear();
    }

    std::vector<QoreIRValue> operands{base_val};
    QoreIROpcode opcode = QoreIROpcode::DotEvalAny;
    QoreParseAnalysis analysis;
    if (getAnalysis(expr, analysis)
        && analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
        && analysis.known_type) {
        if (QoreTypeInfo::isType(analysis.known_type, NT_INT)) {
            opcode = QoreIROpcode::DotEvalInt;
        } else if (QoreTypeInfo::isType(analysis.known_type, NT_FLOAT)) {
            opcode = QoreIROpcode::DotEvalFloat;
        } else if (QoreTypeInfo::isType(analysis.known_type, NT_STRING)) {
            opcode = QoreIROpcode::DotEvalString;
        } else if (QoreTypeInfo::isType(analysis.known_type, NT_DATE)) {
            opcode = QoreIROpcode::DotEvalDate;
        } else if (QoreTypeInfo::isType(analysis.known_type, NT_LIST)) {
            opcode = QoreIROpcode::DotEvalList;
        } else if (QoreTypeInfo::isType(analysis.known_type, NT_HASH)) {
            opcode = QoreIROpcode::DotEvalHash;
        } else if (QoreTypeInfo::isType(analysis.known_type, NT_OBJECT)) {
            opcode = QoreIROpcode::DotEvalObject;
        }
    }
    return lowerExprOpOrInvoke(opcode, expr, operands, op->loc, error);
}

bool QoreIRLowering::lowerCallArgs(const QoreParseListNode* parse_args, const QoreListNode* args,
        std::vector<QoreIRValue>& lowered, std::string& error) {
    if (!parse_args && !args) {
        return true;  // zero-argument call — valid, no args to lower
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
    const qore_list_private* args_priv = qore_list_private::get(args);
    if (args_priv && args_priv->hasCallArgEvalMap()) {
        const std::vector<size_t>* pos_map = args_priv->getCallArgEvalMap();
        size_t result_size = args_priv->getCallArgEvalResultSize();
        assert(pos_map && pos_map->size() == args->size());
        std::vector<QoreIRValue> positional(result_size);
        for (size_t i = 0; i < args->size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(nullptr, "IR named call argument lowering")) {
                error = "IR named call argument lowering cancelled or interrupted";
                return false;
            }
            QoreIRValue arg = lowerExpression(args->retrieveEntry(i), error);
            if (!arg.isValid()) {
                return false;
            }
            size_t pos = (*pos_map)[i];
            assert(pos < result_size);
            positional[pos] = arg;
        }
        for (size_t i = 0; i < result_size; ++i) {
            if (!positional[i].isValid()) {
                positional[i] = builder.createConstNothing()->result;
            }
            lowered.push_back(positional[i]);
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

QoreIRValue QoreIRLowering::lowerExprOpOrInvoke(QoreIROpcode op, const QoreValue& expr,
        const std::vector<QoreIRValue>& operands, const QoreProgramLocation* loc, std::string& error,
        bool has_ref_args) {
    // Track AST-delegated instructions for map body optimization
    ++ast_delegate_count;

    bool caller_local_ref_args = has_ref_args;
    switch (op) {
        case QoreIROpcode::Call:
        case QoreIROpcode::CallDirect:
        case QoreIROpcode::CallMethod:
        case QoreIROpcode::CallStatic:
        case QoreIROpcode::CallStaticDirect:
        case QoreIROpcode::CallIndirect:
        case QoreIROpcode::CallClosureDirect:
            if (!caller_local_ref_args && expr.hasNode()) {
                const AbstractQoreNode* node = expr.getInternalNode();
                if (auto* call = dynamic_cast<const AbstractFunctionCallNode*>(node)) {
                    caller_local_ref_args = callArgsMayPassReferences(call->getParseArgs(), call->getArgs());
                } else if (auto* call = dynamic_cast<const CallReferenceCallNode*>(node)) {
                    caller_local_ref_args = callArgsMayPassReferences(call->getParseArgs(), call->getArgs());
                } else if (op != QoreIROpcode::CallClosureDirect) {
                    caller_local_ref_args = true;
                }
            } else if (!caller_local_ref_args && op != QoreIROpcode::CallClosureDirect) {
                caller_local_ref_args = true;
            }
            break;
        default:
            break;
    }

    QoreIRValue result;
    bool should_invoke = !exception_stack.empty() && expressionCanThrow(expr);
    if (should_invoke) {
        QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
        if (!normal_block) {
            error = "IR builder failed to create invoke continuation block";
            return QoreIRValue();
        }
        QoreIRBasicBlock* handler = exception_stack.back();
        auto* inst = builder.createInvoke(expr, operands, normal_block, handler, loc);
        inst->invoke_opcode = op;
        inst->has_ref_args = caller_local_ref_args;
        builder.setBlock(normal_block);
        result = inst->result;
    } else {
        auto* inst = builder.createExprOp(op, expr, operands, loc);
        inst->has_ref_args = caller_local_ref_args;
        result = inst->result;
    }
    if (opcodeNeverReturnsNothing(op)) {
        never_nothing_values.insert(result.id);
    } else {
        maybeInsertNotNothingGuard(result, &expr, loc, nullptr);
    }
    return result;
}

// Emit LoadLocal → HashKeyAccess → op → HashKeyStore sequence for $hash{key} OP= val.
// arith_op must be one of AddAssignInt/Float/Any, SubAssignInt/Float/Any, etc.
QoreIRValue QoreIRLowering::emitHashKeyCompoundOp(
        const VarRefNode* container_var, const std::string& key_name, const QoreValue& key_expr,
        QoreIROpcode arith_op, const QoreIRValue& right,
        const QoreValue& full_expr, const QoreProgramLocation* loc, std::string& error) {
    // TRACE: emitHashKeyCompoundOp entry

    // Load the hash container without refcount inflation (auto_ref=false)
    // This allows HashKeyStore's is_unique() check to accurately detect if COW is needed
    // based on whether the hash is shared with other code, not just with LoadLocal's cache
    QoreIRValue hash_val;
    if (container_var->getType() == VT_LOCAL && container_var->ref.id) {
        LocalVar* lv = const_cast<LocalVar*>(reinterpret_cast<const LocalVar*>(container_var->ref.id));
        hash_val = builder.createLoadLocal(lv, loc, false)->result;
    } else {
        hash_val = loadVarRef(container_var, error, "hash-compound-op", full_expr);
    }
    if (!hash_val.isValid()) return QoreIRValue();

    // Lower the key expression (required for consistent AST behavior with top-level evaluation)
    QoreIRValue key_val = lowerExpression(key_expr, error);
    if (!key_val.isValid()) return QoreIRValue();

    // Load current element value
    auto load_inst = builder.getBlock()->appendInstruction<QoreIRHashKeyAccessInstruction>(key_name.c_str());
    load_inst->loc = loc;
    load_inst->result = builder.getFunction()->createValue();
    load_inst->operands.push_back(hash_val);
    QoreIRValue old_val = load_inst->result;

    // Apply arithmetic
    QoreIRValue new_val = lowerBinaryOpOrInvoke(arith_op, full_expr, old_val, right, loc, error);
    if (!new_val.isValid()) return QoreIRValue();

    // Store result back to hash element (COW-safe via QoreIRHashKeyStoreInstruction)
    auto store_inst = builder.getBlock()->appendInstruction<QoreIRHashKeyStoreInstruction>(
        container_var, key_name.c_str());
    store_inst->loc = loc;
    if (!exception_stack.empty()) {
        store_inst->exception_target = exception_stack.back();
    }
    store_inst->operands.push_back(hash_val);
    store_inst->operands.push_back(new_val);

    return new_val;
}

// Emit LoadLocal → HashDerefDynamic → op → HashKeyStoreDynamic for $hash{dyn_key} OP= val.
// Like emitHashKeyCompoundOp but key is a dynamic expression.
QoreIRValue QoreIRLowering::emitHashKeyDynamicCompoundOp(
        const VarRefNode* container_var, const QoreValue& key_expr,
        QoreIROpcode arith_op, const QoreIRValue& right,
        const QoreValue& full_expr, const QoreProgramLocation* loc, std::string& error) {
    // Load the hash container without refcount inflation (auto_ref=false)
    QoreIRValue hash_val;
    if (container_var->getType() == VT_LOCAL && container_var->ref.id) {
        LocalVar* lv = const_cast<LocalVar*>(reinterpret_cast<const LocalVar*>(container_var->ref.id));
        hash_val = builder.createLoadLocal(lv, loc, false)->result;
    } else {
        hash_val = loadVarRef(container_var, error, "hash-dynamic-compound-op", full_expr);
    }
    if (!hash_val.isValid()) {
        return QoreIRValue();
    }

    // Lower the key expression
    QoreIRValue key_val = lowerExpression(key_expr, error);
    if (!key_val.isValid()) {
        return QoreIRValue();
    }

    // Load current element value via HashDerefDynamic
    std::vector<QoreIRValue> deref_operands{hash_val, key_val};
    QoreIRValue old_val = lowerExprOpOrInvoke(QoreIROpcode::HashDerefDynamic, full_expr,
        deref_operands, loc, error);
    if (!old_val.isValid()) {
        return QoreIRValue();
    }

    // Apply arithmetic
    QoreIRValue new_val = lowerBinaryOpOrInvoke(arith_op, full_expr, old_val, right, loc, error);
    if (!new_val.isValid()) {
        return QoreIRValue();
    }

    // Store result back with dynamic key
    auto* store_inst = builder.getBlock()->appendInstruction<QoreIRHashKeyStoreDynamicInstruction>(
        container_var);
    store_inst->loc = loc;
    if (!exception_stack.empty()) {
        store_inst->exception_target = exception_stack.back();
    }
    store_inst->operands.push_back(hash_val);
    store_inst->operands.push_back(new_val);
    store_inst->operands.push_back(key_val);

    return new_val;
}

// Emit LoadLocal → ListIndexAccess → op → ListIndexStore sequence for $list[index] OP= val.
// arith_op must be one of AddAssignInt/Float/Any, SubAssignInt/Float/Any, etc.
QoreIRValue QoreIRLowering::emitListKeyCompoundOp(
        const VarRefNode* container_var, const QoreValue& index_expr,
        QoreIROpcode arith_op, const QoreIRValue& right,
        const QoreValue& full_expr, const QoreProgramLocation* loc, std::string& error) {
    // Load the list container without refcount inflation (auto_ref=false)
    // This allows ListIndexStore's is_unique() check to accurately detect if COW is needed
    QoreIRValue list_val;
    if (container_var->getType() == VT_LOCAL && container_var->ref.id) {
        LocalVar* lv = const_cast<LocalVar*>(reinterpret_cast<const LocalVar*>(container_var->ref.id));
        list_val = builder.createLoadLocal(lv, loc, false)->result;
    } else {
        list_val = loadVarRef(container_var, error, "list-compound-op", full_expr);
    }
    if (!list_val.isValid()) return QoreIRValue();

    // Lower the index expression
    QoreIRValue index_val = lowerExpression(index_expr, error);
    if (!index_val.isValid()) return QoreIRValue();

    // Load current element value via ListIndexAccess
    auto load_inst = builder.getBlock()->appendInstruction<QoreIRListIndexAccessInstruction>();
    load_inst->loc = loc;
    load_inst->result = builder.getFunction()->createValue();
    load_inst->operands.push_back(list_val);
    load_inst->operands.push_back(index_val);
    QoreIRValue old_val = load_inst->result;

    // Apply arithmetic
    QoreIRValue new_val = lowerBinaryOpOrInvoke(arith_op, full_expr, old_val, right, loc, error);
    if (!new_val.isValid()) return QoreIRValue();

    // Store result back to list element (COW-safe via QoreIRListIndexStoreInstruction)
    auto store_inst = builder.getBlock()->appendInstruction<QoreIRListIndexStoreInstruction>(container_var);
    store_inst->loc = loc;
    store_inst->operands.push_back(list_val);
    store_inst->operands.push_back(new_val);
    store_inst->operands.push_back(index_val);

    return new_val;
}

// Emit LoadLocal(hash, auto_ref=false) → HashKeyStore(hash, value, key) for h.key = val.
// Simplified version of emitHashKeyCompoundOp without the load-compute cycle.
QoreIRValue QoreIRLowering::emitHashKeyDirectStore(
        const VarRefNode* container_var, const std::string& key_name, const QoreValue& key_expr,
        const QoreIRValue& value, const QoreValue& full_expr,
        const QoreProgramLocation* loc, std::string& error) {
    // Load the hash container without refcount inflation (auto_ref=false)
    QoreIRValue hash_val;
    if (container_var->getType() == VT_LOCAL && container_var->ref.id) {
        LocalVar* lv = const_cast<LocalVar*>(reinterpret_cast<const LocalVar*>(container_var->ref.id));
        hash_val = builder.createLoadLocal(lv, loc, false)->result;
    } else {
        hash_val = loadVarRef(container_var, error, "hash-direct-store", full_expr);
    }
    if (!hash_val.isValid()) {
        return QoreIRValue();
    }

    // Store value to hash element (COW-safe via QoreIRHashKeyStoreInstruction)
    auto* store_inst = builder.getBlock()->appendInstruction<QoreIRHashKeyStoreInstruction>(
        container_var, key_name.c_str());
    store_inst->loc = loc;
    if (!exception_stack.empty()) {
        store_inst->exception_target = exception_stack.back();
    }
    store_inst->operands.push_back(hash_val);
    store_inst->operands.push_back(value);

    return value;
}

// Emit LoadLocal(hash, auto_ref=false) → HashKeyStoreDynamic(hash, value, key) for h{dyn_key} = val.
// Like emitHashKeyDirectStore but key is a dynamic expression lowered to IR.
QoreIRValue QoreIRLowering::emitHashKeyDynamicStore(
        const VarRefNode* container_var, const QoreValue& key_expr,
        const QoreIRValue& value, const QoreValue& full_expr,
        const QoreProgramLocation* loc, std::string& error) {
    // Load the hash container without refcount inflation (auto_ref=false)
    QoreIRValue hash_val;
    if (container_var->getType() == VT_LOCAL && container_var->ref.id) {
        LocalVar* lv = const_cast<LocalVar*>(reinterpret_cast<const LocalVar*>(container_var->ref.id));
        hash_val = builder.createLoadLocal(lv, loc, false)->result;
    } else {
        hash_val = loadVarRef(container_var, error, "hash-dynamic-store", full_expr);
    }
    if (!hash_val.isValid()) {
        return QoreIRValue();
    }

    // Lower the key expression to IR
    QoreIRValue key_val = lowerExpression(key_expr, error);
    if (!key_val.isValid()) {
        return QoreIRValue();
    }

    // Store value to hash element with dynamic key
    auto* store_inst = builder.getBlock()->appendInstruction<QoreIRHashKeyStoreDynamicInstruction>(
        container_var);
    store_inst->loc = loc;
    if (!exception_stack.empty()) {
        store_inst->exception_target = exception_stack.back();
    }
    store_inst->operands.push_back(hash_val);
    store_inst->operands.push_back(value);
    store_inst->operands.push_back(key_val);

    return value;
}

// Try to emit a LValuePath instruction for any lvalue expression that can be path-encoded.
// Returns valid QoreIRValue on success, invalid on failure (lvalue not path-encodable).
// Only applies to lvalues that benefit from path-based access: member variable chains
// (SelfVarref root) or multi-step paths. Simple local variable subscripts are handled
// by existing fast paths (emitHashKeyCompoundOp, etc.) or guardLValueBase fallback.
QoreIRValue QoreIRLowering::tryEmitLValuePathOp(QoreIROpcode opcode, const QoreValue& lvalue,
        const QoreIRValue* rhs, const QoreProgramLocation* loc, std::string& error,
        bool weak, LVCompoundOp compound_op, LVUnaryOp unary_op,
        LVBinaryMutOp binary_mut_op, const QoreValue& pattern_expr) {
    std::vector<LVPathStep> lv_path;
    std::vector<QoreValue> dynamic_operands;
    // Slice lvalues (HashKeySlice / ListIndexSlice / ListRangeSlice) are currently only
    // supported at the runtime by LValuePathUnary's Remove/Delete paths.
    // Any other opcode/unary-op combination must not accept slice lvalues —
    // extractLValuePath will fall back to AST-eval for those shapes.
    const bool allow_slice = (opcode == QoreIROpcode::LValuePathUnary
            && (unary_op == LVUnaryOp::Remove
                    || unary_op == LVUnaryOp::Delete));
    if (!extractLValuePath(lvalue, lv_path, dynamic_operands, allow_slice)) {
        return QoreIRValue();
    }
    if (lv_path.empty()) {
        return QoreIRValue();
    }
    // Reference-typed roots are allowed — navigatePath's LocalVarValue::getLValue()
    // detects NT_REFERENCE and follows it via doLValue(ref, for_remove).
    // Lower dynamic key/index operands
    std::vector<QoreIRValue> dyn_vals;
    for (auto& dop : dynamic_operands) {
        QoreIRValue dv = lowerExpression(dop, error);
        if (!dv.isValid()) {
            return QoreIRValue();
        }
        dyn_vals.push_back(dv);
    }
    // Assign operand indices to dynamic path steps
    uint32_t dyn_idx = 0;
    for (auto& step : lv_path) {
        if ((step.kind == LVPathStepKind::HashKey || step.kind == LVPathStepKind::ListIndex)
                && step.operand_idx == UINT32_MAX) {
            if (dyn_idx < dyn_vals.size()) {
                step.operand_idx = dyn_vals[dyn_idx].id;
                ++dyn_idx;
            }
        } else if (step.kind == LVPathStepKind::HashKeySlice
                || step.kind == LVPathStepKind::ListIndexSlice
                || step.kind == LVPathStepKind::ListRangeSlice) {
            for (uint32_t& sid : step.slice_operand_ids) {
                if (dyn_idx < dyn_vals.size()) {
                    sid = dyn_vals[dyn_idx].id;
                    ++dyn_idx;
                }
            }
        }
    }
    // Create the instruction with a properly allocated result value ID
    auto* path_inst = builder.getBlock()->appendInstruction<QoreIRLValuePathInstruction>(opcode);
    path_inst->result = builder.getFunction()->createValue();
    path_inst->path = std::move(lv_path);
    path_inst->weak = weak;
    path_inst->compound_op = compound_op;
    path_inst->unary_op = unary_op;
    path_inst->binary_mut_op = binary_mut_op;
    if (pattern_expr.hasNode()) {
        const_cast<QoreValue&>(path_inst->pattern_expr) = pattern_expr;
        // Copy ref_rv flag from the operator node (determines if return value is used)
        auto* op_node = dynamic_cast<const QoreOperatorNode*>(pattern_expr.getInternalNode());
        if (op_node) {
            path_inst->ref_rv = op_node->needsReturnValue();
        }
    }
    path_inst->loc = loc;
    if (QoreIRBasicBlock* handler = getCurrentExceptionTarget()) {
        path_inst->exception_target = handler;
    }
    // Add RHS operand if provided
    if (rhs) {
        path_inst->operands.push_back(*rhs);
    }
    // Add dynamic operands
    for (auto& dv : dyn_vals) {
        path_inst->operands.push_back(dv);
    }
    // Compound ops compute a new value (e.g. +=, -=): return the instruction's result
    // so the caller uses the actual computed value, not the RHS.
    // Compound and binary-mut ops compute a new value; return the instruction result so
    // expression users see the modified target value instead of the RHS operand.
    // Assignment returns the RHS; unary returns path_inst->result.
    if (opcode == QoreIROpcode::LValuePathCompound
            || opcode == QoreIROpcode::LValuePathBinaryMut) {
        return path_inst->result;
    }
    return rhs ? *rhs : path_inst->result;
}

// Emit LoadLocal(list, auto_ref=false) → ListIndexStore(list, value, index) for l[i] = val.
QoreIRValue QoreIRLowering::emitListIndexDirectStore(
        const VarRefNode* container_var, const QoreValue& index_expr,
        const QoreIRValue& value, const QoreValue& full_expr,
        const QoreProgramLocation* loc, std::string& error) {
    // Load the list container without refcount inflation (auto_ref=false)
    QoreIRValue list_val;
    if (container_var->getType() == VT_LOCAL && container_var->ref.id) {
        LocalVar* lv = const_cast<LocalVar*>(reinterpret_cast<const LocalVar*>(container_var->ref.id));
        list_val = builder.createLoadLocal(lv, loc, false)->result;
    } else {
        list_val = loadVarRef(container_var, error, "list-direct-store", full_expr);
    }
    if (!list_val.isValid()) {
        return QoreIRValue();
    }

    // Lower the index expression
    QoreIRValue index_val = lowerExpression(index_expr, error);
    if (!index_val.isValid()) {
        return QoreIRValue();
    }

    // Store value to list element (COW-safe via QoreIRListIndexStoreInstruction)
    auto* store_inst = builder.getBlock()->appendInstruction<QoreIRListIndexStoreInstruction>(container_var);
    store_inst->loc = loc;
    store_inst->operands.push_back(list_val);
    store_inst->operands.push_back(value);
    store_inst->operands.push_back(index_val);

    return value;
}

QoreIRValue QoreIRLowering::lowerBinaryOpOrInvoke(QoreIROpcode op, const QoreValue& expr, QoreIRValue left,
        QoreIRValue right, const QoreProgramLocation* loc, std::string& error) {
    QoreIRValue result;
    bool should_invoke = !exception_stack.empty() && expressionCanThrow(expr);
    if (should_invoke) {
        QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
        if (!normal_block) {
            error = "IR builder failed to create invoke continuation block";
            return QoreIRValue();
        }
        QoreIRBasicBlock* handler = exception_stack.back();
        auto* inst = builder.createInvoke(expr, {left, right}, normal_block, handler, loc);
        inst->invoke_opcode = op;
        builder.setBlock(normal_block);
        result = inst->result;
    } else {
        result = builder.createBinaryOp(op, left, right, loc)->result;
    }
    if (opcodeNeverReturnsNothing(op)) {
        never_nothing_values.insert(result.id);
    } else {
        maybeInsertNotNothingGuard(result, &expr, loc, nullptr);
    }
    return result;
}

QoreIRValue QoreIRLowering::lowerUnaryOpOrInvoke(QoreIROpcode op, const QoreValue& expr, QoreIRValue value,
        const QoreProgramLocation* loc, std::string& error) {
    QoreIRValue result;
    bool should_invoke = !exception_stack.empty() && expressionCanThrow(expr);
    if (should_invoke) {
        QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
        if (!normal_block) {
            error = "IR builder failed to create invoke continuation block";
            return QoreIRValue();
        }
        QoreIRBasicBlock* handler = exception_stack.back();
        auto* inst = builder.createInvoke(expr, {value}, normal_block, handler, loc);
        inst->invoke_opcode = op;
        builder.setBlock(normal_block);
        result = inst->result;
    } else {
        result = builder.createUnaryOp(op, value, loc)->result;
    }
    if (opcodeNeverReturnsNothing(op)) {
        never_nothing_values.insert(result.id);
    } else {
        maybeInsertNotNothingGuard(result, &expr, loc, nullptr);
    }
    return result;
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

    QoreIROpcode opcode = QoreIROpcode::CastAny;
    if (cast) {
        if (dynamic_cast<const QoreComplexListCastOperatorNode*>(node)) {
            opcode = QoreIROpcode::CastList;
        } else if (dynamic_cast<const QoreComplexHashCastOperatorNode*>(node)) {
            opcode = QoreIROpcode::CastComplexHash;
        } else if (dynamic_cast<const QoreHashDeclCastOperatorNode*>(node)) {
            opcode = QoreIROpcode::CastHash;
        } else if (dynamic_cast<const QoreEnumCastOperatorNode*>(node)) {
            opcode = QoreIROpcode::CastEnum;
        } else if (dynamic_cast<const QoreClassCastOperatorNode*>(node)) {
            opcode = QoreIROpcode::CastObject;
        }
    }

    // Pre-evaluate the inner expression as operand[0].  The cast runtime helpers
    // perform only the type check/coercion on the pre-evaluated value, avoiding
    // double-evaluation of side-effecting sub-expressions.
    QoreIRValue inner = lowerExpression(cast_node->getExp(), error);
    if (!inner.isValid()) {
        return QoreIRValue();
    }
    std::vector<QoreIRValue> operands;
    operands.push_back(inner);

    // Use lowerExprOpOrInvoke for instruction creation (handles try/catch properly),
    // but undo the ast_delegate_count increment since cast opcodes are now native.
    auto result = lowerExprOpOrInvoke(opcode, expr, operands, cast_node->loc, error);
    --ast_delegate_count;
    return result;
}

QoreIRValue QoreIRLowering::lowerBuiltinTypeConversion(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* call = dynamic_cast<const FunctionCallNode*>(node);
    if (!call) {
        return QoreIRValue();
    }
    const char* func_name = call->getName();
    if (!func_name) {
        return QoreIRValue();
    }
    QoreIROpcode opcode;
    if (!strcmp(func_name, "string")) {
        opcode = QoreIROpcode::ToString;
    } else if (!strcmp(func_name, "int")) {
        opcode = QoreIROpcode::ToInt;
    } else if (!strcmp(func_name, "float")) {
        opcode = QoreIROpcode::ToFloat;
    } else if (!strcmp(func_name, "boolean")) {
        opcode = QoreIROpcode::ToBool;
    } else {
        return QoreIRValue();
    }
    // Match one-argument builtin conversions only. string(expr, enc) still
    // uses the normal builtin implementation because the encoding parameter
    // affects semantics.
    const QoreListNode* args = call->getArgs();
    const QoreParseListNode* parse_args = call->getParseArgs();
    size_t nargs = args ? args->size() : (parse_args ? parse_args->size() : 0);
    if (nargs != 1) {
        return QoreIRValue();
    }
    // Lower the single argument
    std::vector<QoreIRValue> operands;
    if (!lowerCallArgs(parse_args, args, operands, error)) {
        return QoreIRValue();
    }
    // These conversions never throw; always emit as plain ExprOps, not Invoke.
    // The argument's call (if any) is already lowered as Invoke by lowerCallArgs.
    QoreIRValue result = builder.createExprOp(opcode, expr, operands, call->loc)->result;
    never_nothing_values.insert(result.id);
    return result;
}

static size_t qore_ir_get_call_arg_count(const QoreParseListNode* parse_args, const QoreListNode* args) {
    if (parse_args) {
        return parse_args->size();
    }
    if (!args) {
        return 0;
    }
    const qore_list_private* args_priv = qore_list_private::get(args);
    return args_priv && args_priv->hasCallArgEvalMap() ? args_priv->getCallArgEvalResultSize() : args->size();
}

static QoreValue qore_ir_get_call_arg(const QoreParseListNode* parse_args, const QoreListNode* args, size_t i) {
    if (parse_args) {
        return parse_args->get(i);
    }
    const qore_list_private* args_priv = qore_list_private::get(args);
    if (args_priv && args_priv->hasCallArgEvalMap()) {
        assert(false && "mapped call arguments must be handled by callers that can build a positional view");
        return QoreValue();
    }
    return args->retrieveEntry(i);
}

static bool qore_ir_get_single_positional_call_arg(const QoreParseListNode* parse_args,
        const QoreListNode* args, QoreValue& arg) {
    if (parse_args) {
        if (parse_args->size() != 1) {
            return false;
        }
        arg = parse_args->get(0);
        return true;
    }
    if (!args) {
        return false;
    }
    const qore_list_private* args_priv = qore_list_private::get(args);
    if (args_priv && args_priv->hasCallArgEvalMap()) {
        const std::vector<size_t>* pos_map = args_priv->getCallArgEvalMap();
        if (!pos_map || pos_map->size() != 1 || args_priv->getCallArgEvalResultSize() != 1
                || args_priv->callArgEvalMapHasHoles() || (*pos_map)[0] != 0) {
            return false;
        }
        arg = args->retrieveEntry(0);
        return true;
    }
    if (args->size() != 1) {
        return false;
    }
    arg = args->retrieveEntry(0);
    return true;
}

static bool qore_ir_get_positional_call_args_no_holes(const QoreParseListNode* parse_args,
        const QoreListNode* args, size_t nargs, std::vector<QoreValue>& positional_args) {
    positional_args.clear();
    if (parse_args) {
        if (parse_args->size() != nargs) {
            return false;
        }
        positional_args.reserve(nargs);
        for (size_t i = 0; i < nargs; ++i) {
            positional_args.push_back(parse_args->get(i));
        }
        return true;
    }
    if (!args) {
        return nargs == 0;
    }
    const qore_list_private* args_priv = qore_list_private::get(args);
    if (args_priv && args_priv->hasCallArgEvalMap()) {
        const std::vector<size_t>* pos_map = args_priv->getCallArgEvalMap();
        if (!pos_map || args_priv->getCallArgEvalResultSize() != nargs || args_priv->callArgEvalMapHasHoles()) {
            return false;
        }
        assert(pos_map->size() == args->size());
        positional_args.resize(nargs);
        std::vector<bool> seen(nargs, false);
        for (size_t i = 0; i < args->size(); ++i) {
            size_t pos = (*pos_map)[i];
            if (pos >= nargs || seen[pos]) {
                return false;
            }
            positional_args[pos] = args->retrieveEntry(i);
            seen[pos] = true;
        }
        for (bool s : seen) {
            if (!s) {
                return false;
            }
        }
        return true;
    }
    if (args->size() != nargs) {
        return false;
    }
    positional_args.reserve(nargs);
    for (size_t i = 0; i < nargs; ++i) {
        positional_args.push_back(args->retrieveEntry(i));
    }
    return true;
}

static bool qore_ir_call_args_have_named_holes(const QoreListNode* args) {
    const qore_list_private* args_priv = qore_list_private::get(args);
    return args_priv && args_priv->callArgEvalMapHasHoles();
}

bool QoreIRLowering::callArgumentMayPassReference(const QoreValue& arg) const {
    if (arg.getType() == NT_REFERENCE || arg.getType() == NT_PARSEREFERENCE) {
        return true;
    }
    if (!arg.hasNode()) {
        return false;
    }

    const AbstractQoreNode* node = arg.getInternalNode();
    if (!node || !dynamic_cast<const ParseNode*>(node)) {
        return false;
    }
    if (dynamic_cast<const ParseReferenceNode*>(node)) {
        return true;
    }

    if (LocalVar* local = getLocalVarFromValue(arg)) {
        const QoreTypeInfo* ti = local->getTypeInfo();
        if (ti && QoreTypeInfo::parseReturns(ti, NT_REFERENCE) != QTI_NOT_EQUAL) {
            return true;
        }
    }

    QoreParseAnalysis analysis;
    bool got_analysis = false;
    try {
        got_analysis = getAnalysis(arg, analysis);
    } catch (...) {
        got_analysis = false;
    }
    if (got_analysis && analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo) && analysis.known_type) {
        return QoreTypeInfo::parseReturns(analysis.known_type, NT_REFERENCE) != QTI_NOT_EQUAL;
    }

    return true;
}

bool QoreIRLowering::callArgsMayPassReferences(const QoreParseListNode* parse_args,
        const QoreListNode* args) const {
    if (parse_args) {
        for (size_t i = 0; i < parse_args->size(); ++i) {
            if (callArgumentMayPassReference(parse_args->get(i))) {
                return true;
            }
        }
        return false;
    }
    if (!args) {
        return false;
    }
    for (size_t i = 0; i < args->size(); ++i) {
        if (callArgumentMayPassReference(args->retrieveEntry(i))) {
            return true;
        }
    }
    return false;
}

bool QoreIRLowering::callArgumentMayBeRuntimeNothing(const QoreValue& arg) const {
    if (arg.isNothing()) {
        return true;
    }
    if (!arg.hasNode()) {
        return false;
    }

    const AbstractQoreNode* node = arg.getInternalNode();
    if (!node || !dynamic_cast<const ParseNode*>(node)) {
        return false;
    }

    QoreParseAnalysis analysis;
    bool got_analysis = false;
    try {
        got_analysis = getAnalysis(arg, analysis);
    } catch (...) {
        got_analysis = false;
    }
    if (LocalVar* local = getLocalVarFromValue(arg)) {
        // Guard insertion treats unassigned locals as valid NOTHING values, but
        // overload dispatch must fall back when a selected non-NOTHING variant
        // could reject that runtime value.
        return !local->isAssigned()
            || !got_analysis
            || !analysis.hasFlag(QoreParseAnalysis::NeverNothing);
    }

    if (got_analysis && analysis.hasFlag(QoreParseAnalysis::NeverNothing)) {
        return false;
    }

    if (got_analysis && analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo) && analysis.known_type) {
        return true;
    }

    return true;
}

bool QoreIRLowering::directCallVariantMayRejectRuntimeNothing(const AbstractQoreFunctionVariant* variant,
        const QoreParseListNode* parse_args, const QoreListNode* args) const {
    if (!variant) {
        return false;
    }

    const AbstractFunctionSignature* sig = variant->getSignature();
    if (!sig) {
        return false;
    }

    size_t nargs = qore_ir_get_call_arg_count(parse_args, args);
    unsigned nparams = sig->numParams();
    size_t ncheck = std::min(nargs, static_cast<size_t>(nparams));

    std::vector<QoreValue> mapped_args;
    const qore_list_private* args_priv = !parse_args && args ? qore_list_private::get(args) : nullptr;
    if (args_priv && args_priv->hasCallArgEvalMap()) {
        const std::vector<size_t>* pos_map = args_priv->getCallArgEvalMap();
        if (args_priv->callArgEvalMapHasHoles()) {
            return true;
        }
        mapped_args.resize(ncheck);
        for (size_t si = 0; si < pos_map->size(); ++si) {
            size_t pos = (*pos_map)[si];
            if (pos < ncheck) {
                mapped_args[pos] = args->retrieveEntry(si);
            }
        }
    }

    for (size_t i = 0; i < ncheck; ++i) {
        const QoreTypeInfo* param_ti = sig->getParamTypeInfo(static_cast<unsigned>(i));
        if (QoreTypeInfo::parseReturns(param_ti, NT_NOTHING) != QTI_NOT_EQUAL) {
            continue;
        }
        QoreValue arg = mapped_args.empty() ? qore_ir_get_call_arg(parse_args, args, i) : mapped_args[i];
        if (callArgumentMayBeRuntimeNothing(arg)) {
            return true;
        }
    }

    return false;
}

bool QoreIRLowering::overloadedDirectCallNeedsRuntimeDispatch(const QoreFunction* func,
        const AbstractQoreFunctionVariant* variant, const QoreParseListNode* parse_args,
        const QoreListNode* args) const {
    if (qore_ir_call_args_have_named_holes(args)) {
        return true;
    }
    return func && func->numVariants() > 1
        && directCallVariantMayRejectRuntimeNothing(variant, parse_args, args);
}

QoreIRValue QoreIRLowering::lowerFunctionCall(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* call = dynamic_cast<const FunctionCallNode*>(node);
    if (!call) {
        return QoreIRValue();
    }
    if (QoreIRValue hoisted = findLoopInvariantValue(expr); hoisted.isValid()) {
        return hoisted;
    }
    const char* func_name = call->getName();
    if (func_name && (!strcmp(func_name, "length") || !strcmp(func_name, "strlen"))
            && !call->hasExplicitTypeArgs()) {
        const QoreParseListNode* parse_args = call->getParseArgs();
        const QoreListNode* args = call->getArgs();
        QoreValue arg_expr;
        QoreParseAnalysis arg_analysis;
        if (qore_ir_get_single_positional_call_arg(parse_args, args, arg_expr)
                && getAnalysis(arg_expr, arg_analysis)
                && arg_analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
                && (arg_analysis.hasFlag(QoreParseAnalysis::NeverNothing)
                    || arg_analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned))
                && QoreTypeInfo::isType(selectAnalysisType(arg_analysis), NT_STRING)) {
            std::vector<QoreIRValue> operands;
            if (!lowerCallArgs(parse_args, args, operands, error)) {
                return QoreIRValue();
            }
            if (operands.size() == 1) {
                QoreClass* qc = nullptr;
                const QoreMethod* method = pseudo_classes_find_method(NT_STRING, func_name, qc);
                if (method && qc) {
                    auto* inst = builder.createDotEvalMethodDirect(method, qc, nullptr, expr, true,
                        operands, call->loc);
                    inst->fallback_method_name = strdup(func_name);
                    inst->pseudo_base_known_string = true;
                    inst->pseudo_base_known_assigned_string = true;
                    inst->pseudo_base_safe_value_dispatch = true;
                    never_nothing_values.insert(inst->result.id);
                    return inst->result;
                }
            }
        }
    }
    const char* string_case_method_name = nullptr;
    if (func_name && !call->hasExplicitTypeArgs()) {
        if (!strcmp(func_name, "tolower")) {
            string_case_method_name = "lwr";
        } else if (!strcmp(func_name, "toupper")) {
            string_case_method_name = "upr";
        }
    }
    if (string_case_method_name) {
        const QoreParseListNode* parse_args = call->getParseArgs();
        const QoreListNode* args = call->getArgs();
        QoreValue arg_expr;
        QoreParseAnalysis arg_analysis;
        if (qore_ir_get_single_positional_call_arg(parse_args, args, arg_expr)
                && getAnalysis(arg_expr, arg_analysis)
                && arg_analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
                && QoreTypeInfo::isType(selectAnalysisType(arg_analysis), NT_STRING)) {
            LocalVar* arg_local = getLocalVarFromValue(arg_expr);
            bool arg_known_assigned = arg_local
                && parse_context
                && parse_context->isLocalDefinitelyAssigned(arg_local);
            if (arg_known_assigned) {
                std::vector<QoreIRValue> operands;
                if (!lowerCallArgs(parse_args, args, operands, error)) {
                    return QoreIRValue();
                }
                if (operands.size() == 1) {
                    QoreClass* qc = nullptr;
                    const QoreMethod* method = pseudo_classes_find_method(NT_STRING, string_case_method_name, qc);
                    if (method && qc) {
                        QoreIRDotEvalMethodDirectInstruction* direct_inst = nullptr;
                        QoreIRInvokeDotEvalMethodDirectInstruction* invoke_inst = nullptr;
                        if (!exception_stack.empty()) {
                            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
                            if (!normal_block) {
                                error = "IR builder failed to create invoke continuation block";
                                return QoreIRValue();
                            }
                            QoreIRBasicBlock* handler = exception_stack.back();
                            invoke_inst = builder.createInvokeDotEvalMethodDirect(method, qc, nullptr, expr, true,
                                operands, normal_block, handler, call->loc);
                            builder.setBlock(normal_block);
                        } else {
                            direct_inst = builder.createDotEvalMethodDirect(method, qc, nullptr, expr, true,
                                operands, call->loc);
                        }

                        auto set_pseudo_case_flags = [string_case_method_name](auto* inst) {
                            inst->fallback_method_name = strdup(string_case_method_name);
                            inst->pseudo_base_known_string = true;
                            inst->pseudo_base_known_assigned_string = true;
                            inst->pseudo_base_safe_value_dispatch = true;
                        };
                        if (direct_inst) {
                            set_pseudo_case_flags(direct_inst);
                            return direct_inst->result;
                        }
                        assert(invoke_inst);
                        set_pseudo_case_flags(invoke_inst);
                        return invoke_inst->result;
                    }
                }
            }
        }
    }
    if (func_name && !strcmp(func_name, "substr") && !call->hasExplicitTypeArgs()) {
        const QoreParseListNode* parse_args = call->getParseArgs();
        const QoreListNode* args = call->getArgs();
        size_t nargs = qore_ir_get_call_arg_count(parse_args, args);
        if (nargs == 2 || nargs == 3) {
            std::vector<QoreValue> positional_args;
            if (qore_ir_get_positional_call_args_no_holes(parse_args, args, nargs, positional_args)) {
                QoreParseAnalysis base_analysis;
                QoreParseAnalysis start_analysis;
                QoreParseAnalysis length_analysis;
                bool base_ok = getAnalysis(positional_args[0], base_analysis)
                    && base_analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
                    && (base_analysis.hasFlag(QoreParseAnalysis::NeverNothing)
                        || base_analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned))
                    && QoreTypeInfo::isType(selectAnalysisType(base_analysis), NT_STRING);
                bool start_ok = getAnalysis(positional_args[1], start_analysis)
                    && start_analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
                    && (start_analysis.hasFlag(QoreParseAnalysis::NeverNothing)
                        || start_analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned))
                    && QoreTypeInfo::isType(selectAnalysisType(start_analysis), NT_INT);
                bool length_ok = nargs == 2
                    || (getAnalysis(positional_args[2], length_analysis)
                        && length_analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
                        && (length_analysis.hasFlag(QoreParseAnalysis::NeverNothing)
                            || length_analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned))
                        && QoreTypeInfo::isType(selectAnalysisType(length_analysis), NT_INT));
                if (base_ok && start_ok && length_ok) {
                    std::vector<QoreIRValue> operands;
                    if (!lowerCallArgs(parse_args, args, operands, error)) {
                        return QoreIRValue();
                    }
                    if (operands.size() == nargs) {
                        QoreClass* qc = nullptr;
                        const QoreMethod* method = pseudo_classes_find_method(NT_STRING, func_name, qc);
                        if (method && qc) {
                            QoreIRDotEvalMethodDirectInstruction* direct_inst = nullptr;
                            QoreIRInvokeDotEvalMethodDirectInstruction* invoke_inst = nullptr;
                            if (!exception_stack.empty()) {
                                QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
                                if (!normal_block) {
                                    error = "IR builder failed to create invoke continuation block";
                                    return QoreIRValue();
                                }
                                QoreIRBasicBlock* handler = exception_stack.back();
                                invoke_inst = builder.createInvokeDotEvalMethodDirect(method, qc, nullptr, expr, true,
                                    operands, normal_block, handler, call->loc);
                                builder.setBlock(normal_block);
                            } else {
                                direct_inst = builder.createDotEvalMethodDirect(method, qc, nullptr, expr, true,
                                    operands, call->loc);
                            }

                            auto set_pseudo_substr_flags = [func_name, nargs](auto* inst) {
                                inst->fallback_method_name = strdup(func_name);
                                inst->pseudo_base_known_string = true;
                                inst->pseudo_base_known_assigned_string = true;
                                inst->pseudo_arg0_known_assigned_int = true;
                                inst->pseudo_arg1_known_assigned_int = nargs == 3;
                                inst->pseudo_base_safe_value_dispatch = true;
                            };
                            if (direct_inst) {
                                set_pseudo_substr_flags(direct_inst);
                                never_nothing_values.insert(direct_inst->result.id);
                                return direct_inst->result;
                            }
                            assert(invoke_inst);
                            set_pseudo_substr_flags(invoke_inst);
                            never_nothing_values.insert(invoke_inst->result.id);
                            return invoke_inst->result;
                        }
                    }
                }
            }
        }
    }
    const char* string_find_method_name = nullptr;
    if (func_name && !call->hasExplicitTypeArgs()) {
        if (!strcmp(func_name, "index")) {
            string_find_method_name = "find";
        } else if (!strcmp(func_name, "rindex")) {
            string_find_method_name = "rfind";
        }
    }
    if (string_find_method_name) {
        const QoreParseListNode* parse_args = call->getParseArgs();
        const QoreListNode* args = call->getArgs();
        size_t nargs = qore_ir_get_call_arg_count(parse_args, args);
        if (nargs == 2 || nargs == 3) {
            std::vector<QoreValue> positional_args;
            if (qore_ir_get_positional_call_args_no_holes(parse_args, args, nargs, positional_args)) {
                QoreParseAnalysis base_analysis;
                QoreParseAnalysis substring_analysis;
                QoreParseAnalysis offset_analysis;
                bool base_ok = getAnalysis(positional_args[0], base_analysis)
                    && base_analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
                    && (base_analysis.hasFlag(QoreParseAnalysis::NeverNothing)
                        || base_analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned))
                    && QoreTypeInfo::isType(selectAnalysisType(base_analysis), NT_STRING);
                bool substring_ok = getAnalysis(positional_args[1], substring_analysis)
                    && substring_analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
                    && (substring_analysis.hasFlag(QoreParseAnalysis::NeverNothing)
                        || substring_analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned))
                    && QoreTypeInfo::isType(selectAnalysisType(substring_analysis), NT_STRING);
                bool offset_ok = nargs == 2
                    || (getAnalysis(positional_args[2], offset_analysis)
                        && offset_analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
                        && (offset_analysis.hasFlag(QoreParseAnalysis::NeverNothing)
                            || offset_analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned))
                        && QoreTypeInfo::isType(selectAnalysisType(offset_analysis), NT_INT));
                if (base_ok && substring_ok && offset_ok) {
                    std::vector<QoreIRValue> operands;
                    if (!lowerCallArgs(parse_args, args, operands, error)) {
                        return QoreIRValue();
                    }
                    if (operands.size() == nargs) {
                        QoreClass* qc = nullptr;
                        const QoreMethod* method = pseudo_classes_find_method(NT_STRING, string_find_method_name, qc);
                        if (method && qc) {
                            QoreIRDotEvalMethodDirectInstruction* direct_inst = nullptr;
                            QoreIRInvokeDotEvalMethodDirectInstruction* invoke_inst = nullptr;
                            if (!exception_stack.empty()) {
                                QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
                                if (!normal_block) {
                                    error = "IR builder failed to create invoke continuation block";
                                    return QoreIRValue();
                                }
                                QoreIRBasicBlock* handler = exception_stack.back();
                                invoke_inst = builder.createInvokeDotEvalMethodDirect(method, qc, nullptr, expr, true,
                                    operands, normal_block, handler, call->loc);
                                builder.setBlock(normal_block);
                            } else {
                                direct_inst = builder.createDotEvalMethodDirect(method, qc, nullptr, expr, true,
                                    operands, call->loc);
                            }

                            auto set_pseudo_find_flags = [string_find_method_name, nargs](auto* inst) {
                                inst->fallback_method_name = strdup(string_find_method_name);
                                inst->pseudo_base_known_string = true;
                                inst->pseudo_base_known_assigned_string = true;
                                inst->pseudo_arg0_known_string = true;
                                inst->pseudo_arg0_known_assigned_string = true;
                                inst->pseudo_arg1_known_assigned_int = nargs == 3;
                                inst->pseudo_base_safe_value_dispatch = true;
                            };
                            if (direct_inst) {
                                set_pseudo_find_flags(direct_inst);
                                never_nothing_values.insert(direct_inst->result.id);
                                return direct_inst->result;
                            }
                            assert(invoke_inst);
                            set_pseudo_find_flags(invoke_inst);
                            never_nothing_values.insert(invoke_inst->result.id);
                            return invoke_inst->result;
                        }
                    }
                }
            }
        }
    }
    if (func_name && !strcmp(func_name, "number") && !call->hasExplicitTypeArgs()) {
        const QoreParseListNode* parse_args = call->getParseArgs();
        const QoreListNode* args = call->getArgs();
        QoreValue arg_expr;
        QoreParseAnalysis arg_analysis;
        if (qore_ir_get_single_positional_call_arg(parse_args, args, arg_expr)
                && getAnalysis(arg_expr, arg_analysis)
                && arg_analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)) {
            const QoreTypeInfo* arg_type = selectAnalysisType(arg_analysis);
            if (arg_type && !qore_ir_type_may_require_dot_eval_name_dispatch(arg_type)) {
                std::vector<QoreIRValue> operands;
                if (!lowerCallArgs(parse_args, args, operands, error)) {
                    return QoreIRValue();
                }
                if (operands.size() == 1) {
                    QoreClass* qc = nullptr;
                    const QoreMethod* method = pseudo_classes_find_method(NT_NOTHING, "toNumber", qc);
                    if (method && qc) {
                        QoreIRDotEvalMethodDirectInstruction* direct_inst = nullptr;
                        QoreIRInvokeDotEvalMethodDirectInstruction* invoke_inst = nullptr;
                        if (!exception_stack.empty()) {
                            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
                            if (!normal_block) {
                                error = "IR builder failed to create invoke continuation block";
                                return QoreIRValue();
                            }
                            QoreIRBasicBlock* handler = exception_stack.back();
                            invoke_inst = builder.createInvokeDotEvalMethodDirect(method, qc, nullptr, expr, true,
                                operands, normal_block, handler, call->loc);
                            builder.setBlock(normal_block);
                        } else {
                            direct_inst = builder.createDotEvalMethodDirect(method, qc, nullptr, expr, true,
                                operands, call->loc);
                        }

                        auto set_pseudo_to_number_flags = [](auto* inst) {
                            inst->fallback_method_name = strdup("toNumber");
                            inst->pseudo_base_safe_value_dispatch = true;
                        };
                        if (direct_inst) {
                            set_pseudo_to_number_flags(direct_inst);
                            never_nothing_values.insert(direct_inst->result.id);
                            return direct_inst->result;
                        }
                        assert(invoke_inst);
                        set_pseudo_to_number_flags(invoke_inst);
                        never_nothing_values.insert(invoke_inst->result.id);
                        return invoke_inst->result;
                    }
                }
            }
        }
    }
    std::vector<QoreIRValue> operands;
    if (!lowerCallArgs(call->getParseArgs(), call->getArgs(), operands, error)) {
        return QoreIRValue();
    }
    if (call->hasExplicitTypeArgs()) {
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, call->loc, error);
    }

    // If the function is resolved at parse time, use CallDirect to skip the AST round-trip
    const QoreFunction* func = call->getFunction();
    if (func && !overloadedDirectCallNeedsRuntimeDispatch(func, call->getVariant(),
            call->getParseArgs(), call->getArgs())) {
        if (!exception_stack.empty()) {
            // In try/catch: use Invoke with invoke_opcode = CallDirect
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, operands, normal_block, handler, call->loc);
            inst->invoke_opcode = QoreIROpcode::CallDirect;
            // Capture the resolved func now (guaranteed non-null by the enclosing guard):
            // codegen reads inst->func instead of re-resolving getFunction() on the AST node,
            // which can be cleared by codegen time for runtime-created closures (#null-func-crash).
            assert(func);
            inst->func = func;
            inst->has_ref_args = callArgsMayPassReferences(call->getParseArgs(), call->getArgs());
            builder.setBlock(normal_block);
            return inst->result;
        }
        return builder.createCallDirect(func, call->getVariant(),
                call->getProgram(), expr, operands, call->loc)->result;
    }

    return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, call->loc, error);
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
    bool has_ref_args = callArgsMayPassReferences(call->getParseArgs(), call->getArgs());
    // Use CallClosureDirect for fast closure/callref invocation
    // This calls qore_rt_call_closure_fast() which directly calls callref->execValue()
    // instead of going through AST node copy and dynamic_cast chain
    return lowerExprOpOrInvoke(QoreIROpcode::CallClosureDirect, expr, operands, call->loc, error, has_ref_args);
}

// Helper function for Phase 3: cache direct-call target metadata. Runtime
// eligibility still depends on the caller program, argument count, and whether
// the callee was promoted to native code, so the interpreter validates before use.
template <typename DirectCallInst>
static void tryCacheCalleeIRForInlining(const AbstractQoreFunctionVariant* variant,
        DirectCallInst* inst) {
    if (!variant || !inst) {
        return;
    }

    const UserVariantBase* uvb = variant->getUserVariantBase();
    if (!uvb) {
        return;
    }

    // Get the cached IR function from the variant
    const QoreIRFunction* callee_ir = uvb->getCachedIR();
    if (!callee_ir) {
        return;
    }

    const UserSignature* sig = uvb->getUserSignature();
    inst->cached_callee_ir = callee_ir;
    inst->cached_uvb = uvb;
    inst->cached_return_type = sig ? sig->getReturnTypeInfo() : nullptr;
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
    if (call->hasExplicitTypeArgs()) {
        return lowerExprOpOrInvoke(QoreIROpcode::CallMethod, expr, operands, call->loc, error);
    }

    // Check for devirtualization opportunities
    // We can bypass virtual dispatch if:
    // 1. The method is resolved at parse time, AND
    // 2. The class is final (cannot have subclasses, so no override possible)
    const QoreMethod* method = call->getMethod();
    const QoreClass* qc = call->getClass();
    if (method && qc && qc->isFinal()
            && !overloadedDirectCallNeedsRuntimeDispatch(qore_method_private::get(*method)->getFunction(),
                call->getVariant(), call->getParseArgs(), call->getArgs())) {
        // Safe devirtualization - the class is final, so no subclass can override
        const AbstractQoreFunctionVariant* variant = call->getVariant();
        QoreIRValue result;
        bool should_invoke = !exception_stack.empty();  // method calls can always throw
        if (should_invoke) {
            // Use InvokeMethodDirect for devirtualized calls in try/catch.
            // This avoids AST evaluation overhead while maintaining proper exception routing.
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* invoke_inst = builder.createInvokeMethodDirect(method, qc, variant, operands,
                    normal_block, handler, expr, call->loc);
            // Phase 3: Try to cache the callee IR for inlining
            tryCacheCalleeIRForInlining(variant, invoke_inst);
            builder.setBlock(normal_block);
            result = invoke_inst->result;
        } else {
            auto* call_inst = builder.createCallMethodDirect(method, qc, variant, operands, expr, call->loc);
            // Phase 3: Try to cache the callee IR for inlining
            tryCacheCalleeIRForInlining(variant, call_inst);
            result = call_inst->result;
        }
        return result;
    }

    return lowerExprOpOrInvoke(QoreIROpcode::CallMethod, expr, operands, call->loc, error);
}

QoreIRValue QoreIRLowering::lowerStaticCall(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* call = dynamic_cast<const StaticMethodCallNode*>(node);
    if (!call) {
        return QoreIRValue();
    }
    std::vector<QoreIRValue> lowered_args;
    if (!lowerCallArgs(call->getParseArgs(), call->getArgs(), lowered_args, error)) {
        return QoreIRValue();
    }
    if (call->hasExplicitTypeArgs()) {
        return lowerExprOpOrInvoke(QoreIROpcode::CallStatic, expr, lowered_args, call->loc, error);
    }

    // Only use CallStaticDirect if AST conclusively determined the variant at parse time.
    // If getVariant() returns nullptr, AST set runtime_match=true, meaning parse-time variant
    // selection was inconclusive (due to missing type information), and runtime dispatch is required.
    const AbstractQoreFunctionVariant* variant = call->getVariant();
    const QoreMethod* method = call->getMethod();
    if (method && variant && !call->getReceiverTypeInfo()
            && !overloadedDirectCallNeedsRuntimeDispatch(qore_method_private::get(*method)->getFunction(), variant,
                call->getParseArgs(), call->getArgs())) {
        QoreIRValue result;
        bool should_invoke = !exception_stack.empty();
        if (should_invoke) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* invoke_inst = builder.createInvoke(expr, lowered_args, normal_block, handler, call->loc);
            invoke_inst->invoke_opcode = QoreIROpcode::CallStaticDirect;
            invoke_inst->has_ref_args = callArgsMayPassReferences(call->getParseArgs(), call->getArgs());
            // Phase 3: Try to cache the callee IR for inlining
            auto* call_static_inst = dynamic_cast<QoreIRCallStaticDirectInstruction*>(invoke_inst);
            if (call_static_inst) {
                tryCacheCalleeIRForInlining(variant, call_static_inst);
            }
            builder.setBlock(normal_block);
            result = invoke_inst->result;
        } else {
            auto* call_static_inst = builder.createCallStaticDirect(call->getMethod(), variant, expr,
                lowered_args, call->loc);
            // Phase 3: Try to cache the callee IR for inlining
            tryCacheCalleeIRForInlining(variant, call_static_inst);
            result = call_static_inst->result;
        }
        return result;
    }

    return lowerExprOpOrInvoke(QoreIROpcode::CallStatic, expr, lowered_args, call->loc, error);
}

// Pattern analysis for optimized foldl operations
// Returns optimized opcode if pattern is detected, or FoldlAny for fallback
// list_type is the type info of the list being folded, used as fallback for type detection
static QoreIROpcode analyzeFoldPattern(const QoreValue& fold_expr, const QoreTypeInfo*& result_type,
        const QoreTypeInfo* list_type = nullptr) {
    const AbstractQoreNode* node = fold_expr.getInternalNode();
    if (!node) {
        return QoreIROpcode::FoldlAny;
    }

    // Specialized opcodes (FoldlSumInt, FoldlDiffInt, FoldlMinInt, ...)
    // assume the source is a list — they iterate the list in one shot
    // from the runtime helper.  When the source is an object (typically
    // `.iterator()` call) or any non-list, the specialized opcode
    // misinterprets the input and produces garbage (zero for int ops).
    //
    // Guard here: only allow specialization when `list_type` is actually
    // a list (complex or bare-list return).  Otherwise fall through to
    // native lowering which handles iterators via the proper loop.
    //
    // Regression history: surfaced by the Option B parser change
    // (Qore commit 282da15f1 + follow-up) that started narrowing $1
    // for `list.iterator()` sources to the list element type — the
    // resulting `int - int` body matched the FoldlDiffInt pattern
    // but the source object was an AbstractIterator, causing
    // `foldr $1 - $2, (2,3,4).iterator()` to return 0 instead of -1.
    if (list_type) {
        if (QoreTypeInfo::parseReturns(list_type, NT_LIST) == QTI_NOT_EQUAL) {
            return QoreIROpcode::FoldlAny;
        }
    }

    // Check for Plus operator: $1 + $2
    if (auto* plus_op = dynamic_cast<const QorePlusOperatorNode*>(node)) {
        QoreValue left = plus_op->getLeft();
        QoreValue right = plus_op->getRight();

        const auto* arg1 = dynamic_cast<const QoreImplicitArgumentNode*>(left.getInternalNode());
        const auto* arg2 = dynamic_cast<const QoreImplicitArgumentNode*>(right.getInternalNode());

        // Check if operands are $1 and $2 (offsets 0 and 1)
        if (arg1 && arg2 && arg1->getOffset() == 0 && arg2->getOffset() == 1) {
            // Determine result type based on expression type
            // Only use optimized opcodes for numeric types; strings use native lowering
            result_type = plus_op->getTypeInfo();
            if (QoreTypeInfo::parseReturns(result_type, NT_INT) == QTI_IDENT) {
                return QoreIROpcode::FoldlSumInt;
            } else if (QoreTypeInfo::parseReturns(result_type, NT_FLOAT) == QTI_IDENT) {
                return QoreIROpcode::FoldlSumFloat;
            }
            // For strings and other types, use native lowering (FoldlAny)
            // This ensures string concatenation works correctly
        }
    }

    // Check for Multiply operator: $1 * $2
    if (auto* mul_op = dynamic_cast<const QoreMultiplicationOperatorNode*>(node)) {
        QoreValue left = mul_op->getLeft();
        QoreValue right = mul_op->getRight();

        const auto* arg1 = dynamic_cast<const QoreImplicitArgumentNode*>(left.getInternalNode());
        const auto* arg2 = dynamic_cast<const QoreImplicitArgumentNode*>(right.getInternalNode());

        // Check if operands are $1 and $2 (offsets 0 and 1)
        if (arg1 && arg2 && arg1->getOffset() == 0 && arg2->getOffset() == 1) {
            // Determine result type based on expression type
            // Only use optimized opcodes for numeric types
            result_type = mul_op->getTypeInfo();
            if (QoreTypeInfo::parseReturns(result_type, NT_INT) == QTI_IDENT) {
                return QoreIROpcode::FoldlProdInt;
            } else if (QoreTypeInfo::parseReturns(result_type, NT_FLOAT) == QTI_IDENT) {
                return QoreIROpcode::FoldlProdFloat;
            }
            // For non-numeric types, use native lowering
        }
    }

    // Check for Minus operator: $1 - $2
    if (auto* minus_op = dynamic_cast<const QoreMinusOperatorNode*>(node)) {
        QoreValue left = minus_op->getLeft();
        QoreValue right = minus_op->getRight();

        const auto* arg1 = dynamic_cast<const QoreImplicitArgumentNode*>(left.getInternalNode());
        const auto* arg2 = dynamic_cast<const QoreImplicitArgumentNode*>(right.getInternalNode());

        // Check if operands are $1 and $2 (offsets 0 and 1)
        if (arg1 && arg2 && arg1->getOffset() == 0 && arg2->getOffset() == 1) {
            // Determine result type based on expression type
            // Only use optimized opcodes for numeric types
            result_type = minus_op->getTypeInfo();
            if (QoreTypeInfo::parseReturns(result_type, NT_INT) == QTI_IDENT) {
                return QoreIROpcode::FoldlDiffInt;
            } else if (QoreTypeInfo::parseReturns(result_type, NT_FLOAT) == QTI_IDENT) {
                return QoreIROpcode::FoldlDiffFloat;
            }
            // For non-numeric types, use native lowering
        }
    }

    // Check for min/max function calls: min($1, $2) or max($1, $2)
    if (auto* call = dynamic_cast<const FunctionCallNode*>(node)) {
        const char* func_name = call->getName();
        if (func_name && (strcmp(func_name, "min") == 0 || strcmp(func_name, "max") == 0)) {
            bool is_min = strcmp(func_name, "min") == 0;
            const QoreListNode* args = call->getArgs();
            if (args && args->size() == 2) {
                QoreValue arg0 = args->retrieveEntry(0);
                QoreValue arg1 = args->retrieveEntry(1);
                const auto* impl_arg0 = dynamic_cast<const QoreImplicitArgumentNode*>(arg0.getInternalNode());
                const auto* impl_arg1 = dynamic_cast<const QoreImplicitArgumentNode*>(arg1.getInternalNode());

                // Check if args are $1 and $2 (offsets 0 and 1)
                if (impl_arg0 && impl_arg1 && impl_arg0->getOffset() == 0 && impl_arg1->getOffset() == 1) {
                    // min/max return the same type as their arguments
                    // Try the argument type first, then fall back to list element type
                    result_type = getExprTypeInfo(arg0);
                    if (QoreTypeInfo::parseReturns(result_type, NT_INT) == QTI_IDENT) {
                        return is_min ? QoreIROpcode::FoldlMinInt : QoreIROpcode::FoldlMaxInt;
                    } else if (QoreTypeInfo::parseReturns(result_type, NT_FLOAT) == QTI_IDENT) {
                        return is_min ? QoreIROpcode::FoldlMinFloat : QoreIROpcode::FoldlMaxFloat;
                    }
                    // Fall back to list element type if available
                    if (list_type) {
                        const QoreTypeInfo* elem_type = QoreTypeInfo::getUniqueReturnComplexList(list_type);
                        if (elem_type) {
                            if (QoreTypeInfo::parseReturns(elem_type, NT_INT) == QTI_IDENT) {
                                return is_min ? QoreIROpcode::FoldlMinInt : QoreIROpcode::FoldlMaxInt;
                            } else if (QoreTypeInfo::parseReturns(elem_type, NT_FLOAT) == QTI_IDENT) {
                                return is_min ? QoreIROpcode::FoldlMinFloat : QoreIROpcode::FoldlMaxFloat;
                            }
                        }
                    }
                    // Fall back to generic foldl if type cannot be determined
                    // This ensures correctness over optimization
                    return QoreIROpcode::FoldlAny;
                }
            }
        }
    }

    return QoreIROpcode::FoldlAny;
}

// Pattern analysis for optimized map operations
// Returns optimized opcode if pattern detected, or MapAny for fallback
// For scale/offset patterns, constant_val is set to the constant operand
static QoreIROpcode analyzeMapPattern(const QoreValue& map_expr, const QoreTypeInfo*& result_type,
        QoreValue& constant_val) {
    const AbstractQoreNode* node = map_expr.getInternalNode();
    if (!node) {
        return QoreIROpcode::MapAny;
    }

    // Check for Multiply operator: $1 * const or const * $1
    if (auto* mul_op = dynamic_cast<const QoreMultiplicationOperatorNode*>(node)) {
        QoreValue left = mul_op->getLeft();
        QoreValue right = mul_op->getRight();

        const auto* arg_left = dynamic_cast<const QoreImplicitArgumentNode*>(left.getInternalNode());
        const auto* arg_right = dynamic_cast<const QoreImplicitArgumentNode*>(right.getInternalNode());

        // Pattern: $1 * $1 (square)
        if (arg_left && arg_right && arg_left->getOffset() == 0 && arg_right->getOffset() == 0) {
            result_type = mul_op->getTypeInfo();
            if (QoreTypeInfo::parseReturns(result_type, NT_INT) == QTI_IDENT) {
                return QoreIROpcode::MapSquareInt;
            } else if (QoreTypeInfo::parseReturns(result_type, NT_FLOAT) == QTI_IDENT) {
                return QoreIROpcode::MapSquareFloat;
            }
            return QoreIROpcode::MapSquareInt;
        }

        // Pattern: $1 * const
        if (arg_left && arg_left->getOffset() == 0 && !arg_right) {
            // Check if right is a constant
            if (right.getType() == NT_INT || right.getType() == NT_FLOAT || !right.hasNode()) {
                constant_val = right;
                result_type = mul_op->getTypeInfo();
                if (QoreTypeInfo::parseReturns(result_type, NT_INT) == QTI_IDENT) {
                    return QoreIROpcode::MapScaleInt;
                } else if (QoreTypeInfo::parseReturns(result_type, NT_FLOAT) == QTI_IDENT) {
                    return QoreIROpcode::MapScaleFloat;
                }
                return QoreIROpcode::MapScaleInt;
            }
        }

        // Pattern: const * $1
        if (arg_right && arg_right->getOffset() == 0 && !arg_left) {
            if (left.getType() == NT_INT || left.getType() == NT_FLOAT || !left.hasNode()) {
                constant_val = left;
                result_type = mul_op->getTypeInfo();
                if (QoreTypeInfo::parseReturns(result_type, NT_INT) == QTI_IDENT) {
                    return QoreIROpcode::MapScaleInt;
                } else if (QoreTypeInfo::parseReturns(result_type, NT_FLOAT) == QTI_IDENT) {
                    return QoreIROpcode::MapScaleFloat;
                }
                return QoreIROpcode::MapScaleInt;
            }
        }
    }

    // Check for Plus operator: $1 + const or const + $1
    if (auto* plus_op = dynamic_cast<const QorePlusOperatorNode*>(node)) {
        QoreValue left = plus_op->getLeft();
        QoreValue right = plus_op->getRight();

        const auto* arg_left = dynamic_cast<const QoreImplicitArgumentNode*>(left.getInternalNode());
        const auto* arg_right = dynamic_cast<const QoreImplicitArgumentNode*>(right.getInternalNode());

        // Pattern: $1 + const
        if (arg_left && arg_left->getOffset() == 0 && !arg_right) {
            if (right.getType() == NT_INT || right.getType() == NT_FLOAT || !right.hasNode()) {
                constant_val = right;
                result_type = plus_op->getTypeInfo();
                if (QoreTypeInfo::parseReturns(result_type, NT_INT) == QTI_IDENT) {
                    return QoreIROpcode::MapOffsetInt;
                } else if (QoreTypeInfo::parseReturns(result_type, NT_FLOAT) == QTI_IDENT) {
                    return QoreIROpcode::MapOffsetFloat;
                }
                return QoreIROpcode::MapOffsetInt;
            }
        }

        // Pattern: const + $1
        if (arg_right && arg_right->getOffset() == 0 && !arg_left) {
            if (left.getType() == NT_INT || left.getType() == NT_FLOAT || !left.hasNode()) {
                constant_val = left;
                result_type = plus_op->getTypeInfo();
                if (QoreTypeInfo::parseReturns(result_type, NT_INT) == QTI_IDENT) {
                    return QoreIROpcode::MapOffsetInt;
                } else if (QoreTypeInfo::parseReturns(result_type, NT_FLOAT) == QTI_IDENT) {
                    return QoreIROpcode::MapOffsetFloat;
                }
                return QoreIROpcode::MapOffsetInt;
            }
        }
    }

    return QoreIROpcode::MapAny;
}

// Helper: check if an expression is $1.key (hash object dereference with implicit arg and constant string key)
static bool getImplicitHashKeyAccess(const QoreValue& expr, std::string& key_name) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* deref = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(node);
    if (!deref) {
        return false;
    }
    // Check left is $1
    QoreValue left_val = deref->getLeft();
    auto* impl_arg = dynamic_cast<const QoreImplicitArgumentNode*>(left_val.getInternalNode());
    if (!impl_arg || impl_arg->getOffset() != 0) {
        return false;
    }
    // Check right is constant string
    QoreValue right_val = deref->getRight();
    if (!right_val.hasNode() || right_val.getType() != NT_STRING) {
        return false;
    }
    QoreStringValueHelper key(right_val);
    key_name = key->c_str();
    return true;
}

// Pattern analysis for hash-key map operations
// Detects: map $1.key, list / map ($1.key + N), list / map ($1.key * N), list
// Returns optimized opcode or MapAny for fallback
// Sets key_name to the key name and constant_val for offset/scale patterns
static QoreIROpcode analyzeMapHashKeyPattern(const QoreValue& map_expr, std::string& key_name,
        QoreValue& constant_val) {
    // Direct $1.key pattern
    if (getImplicitHashKeyAccess(map_expr, key_name)) {
        return QoreIROpcode::MapHashKeyValue;
    }

    const AbstractQoreNode* node = map_expr.getInternalNode();
    if (!node) {
        return QoreIROpcode::MapAny;
    }

    // Check for Plus operator: $1.key + const
    if (auto* plus_op = dynamic_cast<const QorePlusOperatorNode*>(node)) {
        QoreValue left = plus_op->getLeft();
        QoreValue right = plus_op->getRight();

        // Pattern: $1.key + const
        std::string k;
        if (getImplicitHashKeyAccess(left, k) && !right.hasNode() && (right.getType() == NT_INT)) {
            // Check result type is int
            const QoreTypeInfo* rtype = plus_op->getTypeInfo();
            if (QoreTypeInfo::parseReturns(rtype, NT_INT) == QTI_IDENT) {
                key_name = k;
                constant_val = right;
                return QoreIROpcode::MapHashKeyOffsetInt;
            }
        }

        // Pattern: const + $1.key
        if (getImplicitHashKeyAccess(right, k) && !left.hasNode() && (left.getType() == NT_INT)) {
            const QoreTypeInfo* rtype = plus_op->getTypeInfo();
            if (QoreTypeInfo::parseReturns(rtype, NT_INT) == QTI_IDENT) {
                key_name = k;
                constant_val = left;
                return QoreIROpcode::MapHashKeyOffsetInt;
            }
        }
    }

    // Check for Multiply operator: $1.key * const
    if (auto* mul_op = dynamic_cast<const QoreMultiplicationOperatorNode*>(node)) {
        QoreValue left = mul_op->getLeft();
        QoreValue right = mul_op->getRight();

        // Pattern: $1.key * const
        std::string k;
        if (getImplicitHashKeyAccess(left, k) && !right.hasNode() && (right.getType() == NT_INT)) {
            const QoreTypeInfo* rtype = mul_op->getTypeInfo();
            if (QoreTypeInfo::parseReturns(rtype, NT_INT) == QTI_IDENT) {
                key_name = k;
                constant_val = right;
                return QoreIROpcode::MapHashKeyScaleInt;
            }
        }

        // Pattern: const * $1.key
        if (getImplicitHashKeyAccess(right, k) && !left.hasNode() && (left.getType() == NT_INT)) {
            const QoreTypeInfo* rtype = mul_op->getTypeInfo();
            if (QoreTypeInfo::parseReturns(rtype, NT_INT) == QTI_IDENT) {
                key_name = k;
                constant_val = left;
                return QoreIROpcode::MapHashKeyScaleInt;
            }
        }
    }

    return QoreIROpcode::MapAny;
}

// Pattern analysis for hash map two-keys: map {$1.k1: $1.k2}, list
// Returns HashMapTwoKeys opcode if both key and value are $1.key patterns
static QoreIROpcode analyzeHashMapTwoKeysPattern(const QoreValue& key_expr, const QoreValue& val_expr,
        std::string& key1_name, std::string& key2_name) {
    std::string k1;
    std::string k2;
    if (getImplicitHashKeyAccess(key_expr, k1) && getImplicitHashKeyAccess(val_expr, k2)) {
        key1_name = k1;
        key2_name = k2;
        return QoreIROpcode::HashMapTwoKeys;
    }
    return QoreIROpcode::MapAny;
}

// Pattern analysis for optimized select operations
// Returns optimized opcode if pattern detected, or SelectAny for fallback
static QoreIROpcode analyzeSelectPattern(const QoreValue& select_expr, const QoreTypeInfo*& result_type) {
    const AbstractQoreNode* node = select_expr.getInternalNode();
    if (!node) {
        return QoreIROpcode::SelectAny;
    }

    // Check for Greater Than operator: $1 > 0
    if (auto* gt_op = dynamic_cast<const QoreLogicalGreaterThanOperatorNode*>(node)) {
        QoreValue left = gt_op->getLeft();
        QoreValue right = gt_op->getRight();

        const auto* arg_left = dynamic_cast<const QoreImplicitArgumentNode*>(left.getInternalNode());

        // Pattern: $1 > 0
        if (arg_left && arg_left->getOffset() == 0 && !right.hasNode()) {
            if (right.getAsBigInt() == 0) {
                result_type = gt_op->getTypeInfo();
                // Determine type from expression analysis or default to int
                return QoreIROpcode::SelectPositiveInt;
            }
        }
    }

    // Check for Not Equals operator: $1 != 0
    if (auto* ne_op = dynamic_cast<const QoreLogicalNotEqualsOperatorNode*>(node)) {
        QoreValue left = ne_op->getLeft();
        QoreValue right = ne_op->getRight();

        const auto* arg_left = dynamic_cast<const QoreImplicitArgumentNode*>(left.getInternalNode());

        // Pattern: $1 != 0
        if (arg_left && arg_left->getOffset() == 0 && !right.hasNode()) {
            if (right.getAsBigInt() == 0) {
                result_type = nullptr;  // Type will be inferred from list
                return QoreIROpcode::SelectNonZeroInt;
            }
        }
    }

    return QoreIROpcode::SelectAny;
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

    {
        std::vector<LazyPipelineStage> source_stages;
        QoreValue base_source;
        if (!collectLazyPipelineStages(foldl->getIteratorExpr(), base_source, source_stages, error)) {
            return QoreIRValue();
        }
        if (!source_stages.empty()) {
            LazyPipelineRoot root;
            root.kind = LazyPipelineRoot::Foldl;
            root.fold_expr = &foldl->getFoldExpression();
            root.loc = foldl->loc;
            return lowerLazyPipelineFused(base_source, source_stages, root, error);
        }
    }

    // Try to detect optimizable pattern first
    const QoreTypeInfo* result_type = nullptr;
    // Get list type info for fallback type detection
    const QoreTypeInfo* list_type = getExprTypeInfo(foldl->getRight());
    QoreIROpcode opt_opcode = analyzeFoldPattern(foldl->getLeft(), result_type, list_type);

    if (opt_opcode != QoreIROpcode::FoldlAny) {
        // Check if we can fuse foldl with a map operand
        // Pattern: foldl $1 + $2, (map $1 * c, list) or foldl $1 * $2, (map $1 * c, list)
        const AbstractQoreNode* right_node = foldl->getRight().getInternalNode();
        auto* inner_map = dynamic_cast<const QoreMapOperatorNode*>(right_node);

        if (inner_map && (opt_opcode == QoreIROpcode::FoldlSumInt || opt_opcode == QoreIROpcode::FoldlSumFloat ||
                         opt_opcode == QoreIROpcode::FoldlProdInt || opt_opcode == QoreIROpcode::FoldlProdFloat)) {
            // Analyze the map pattern
            const QoreTypeInfo* map_result_type = nullptr;
            QoreValue map_constant_val;
            QoreIROpcode map_opcode = analyzeMapPattern(inner_map->getLeft(), map_result_type, map_constant_val);

            // Only fuse for scale ($1 * c) or square ($1 * $1) patterns with sum
            QoreIROpcode fused_opcode = QoreIROpcode::FoldlAny;
            bool needs_constant = true;

            // Determine if we need float fused opcodes based on:
            // 1. The fold opcode type (FoldlSumFloat, FoldlProdFloat)
            // 2. The map opcode type (MapScaleFloat, MapSquareFloat)
            // 3. The element type of the underlying list (as fallback)
            bool is_float = (opt_opcode == QoreIROpcode::FoldlSumFloat ||
                            opt_opcode == QoreIROpcode::FoldlProdFloat ||
                            map_opcode == QoreIROpcode::MapScaleFloat ||
                            map_opcode == QoreIROpcode::MapSquareFloat);

            // If opcodes don't indicate float, check element type as additional validation
            if (!is_float) {
                QoreParseAnalysis inner_list_analysis;
                const QoreTypeInfo* inner_list_type = nullptr;
                if (getAnalysis(inner_map->getRight(), inner_list_analysis) &&
                        inner_list_analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)) {
                    inner_list_type = inner_list_analysis.known_type;
                }
                const QoreTypeInfo* elem_type = QoreTypeInfo::getUniqueReturnComplexList(inner_list_type);
                if (QoreTypeInfo::isType(elem_type, NT_FLOAT)) {
                    is_float = true;
                }
            }

            if (opt_opcode == QoreIROpcode::FoldlSumInt || opt_opcode == QoreIROpcode::FoldlSumFloat) {
                // foldl $1 + $2, (map ..., list)
                if (map_opcode == QoreIROpcode::MapScaleInt || map_opcode == QoreIROpcode::MapScaleFloat) {
                    fused_opcode = is_float ? QoreIROpcode::FusedMapFoldlSumScaleFloat
                                           : QoreIROpcode::FusedMapFoldlSumScaleInt;
                } else if (map_opcode == QoreIROpcode::MapSquareInt || map_opcode == QoreIROpcode::MapSquareFloat) {
                    fused_opcode = is_float ? QoreIROpcode::FusedMapFoldlSumSquareFloat
                                           : QoreIROpcode::FusedMapFoldlSumSquareInt;
                    needs_constant = false;
                }
            } else if (opt_opcode == QoreIROpcode::FoldlProdInt || opt_opcode == QoreIROpcode::FoldlProdFloat) {
                // foldl $1 * $2, (map ..., list)
                if (map_opcode == QoreIROpcode::MapScaleInt || map_opcode == QoreIROpcode::MapScaleFloat) {
                    fused_opcode = is_float ? QoreIROpcode::FusedMapFoldlProdScaleFloat
                                           : QoreIROpcode::FusedMapFoldlProdScaleInt;
                }
                // Note: We don't have fused square+prod patterns currently
            }

            if (fused_opcode != QoreIROpcode::FoldlAny) {
                // Lower the underlying list (right operand of map)
                QoreIRValue list_val = lowerExpression(inner_map->getRight(), error);
                if (!list_val.isValid()) {
                    return QoreIRValue();
                }

                QoreIRValue const_ir;
                if (needs_constant) {
                    const_ir = lowerConstant(map_constant_val, error);
                    if (!const_ir.isValid()) {
                        return QoreIRValue();
                    }
                } else {
                    // Square doesn't need a constant, use NOTHING as placeholder
                    const_ir = builder.createConstNothing(foldl->loc)->result;
                }

                QoreIRValue result;
                if (!exception_stack.empty()) {
                    QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
                    if (!normal_block) {
                        error = "IR builder failed to create invoke continuation block";
                        return QoreIRValue();
                    }
                    QoreIRBasicBlock* handler = exception_stack.back();
                    auto* inst = builder.createInvoke(expr, {list_val, const_ir}, normal_block, handler, foldl->loc);
                    inst->invoke_opcode = fused_opcode;
                    builder.setBlock(normal_block);
                    result = inst->result;
                } else {
                    result = builder.createBinaryOp(fused_opcode, list_val, const_ir, foldl->loc)->result;
                }
                maybeInsertNotNothingGuard(result, &expr, foldl->loc, nullptr);
                return result;
            }
        }

        // No fusion possible, emit regular optimized foldl opcode
        // Optimized path: emit specialized opcode with just the list operand
        QoreIRValue list = lowerExpression(foldl->getRight(), error);
        if (!list.isValid()) {
            return QoreIRValue();
        }

        // Create a dummy NOTHING value for the second operand (initial value is first list element)
        QoreIRValue init = builder.createConstNothing(foldl->loc)->result;

        QoreIRValue result;
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {list, init}, normal_block, handler, foldl->loc);
            inst->invoke_opcode = opt_opcode;
            builder.setBlock(normal_block);
            result = inst->result;
        } else {
            result = builder.createBinaryOp(opt_opcode, list, init, foldl->loc)->result;
        }
        maybeInsertNotNothingGuard(result, &expr, foldl->loc, nullptr);
        return result;
    }

    // Native IR lowering with implicit argument context ($1, $2)
    return lowerFoldlNative(foldl, expr, error);
}

// Map foldl opcode to corresponding foldr opcode
static QoreIROpcode foldlToFoldrOpcode(QoreIROpcode foldl_op) {
    switch (foldl_op) {
        case QoreIROpcode::FoldlSumInt: return QoreIROpcode::FoldrSumInt;
        case QoreIROpcode::FoldlSumFloat: return QoreIROpcode::FoldrSumFloat;
        case QoreIROpcode::FoldlProdInt: return QoreIROpcode::FoldrProdInt;
        case QoreIROpcode::FoldlProdFloat: return QoreIROpcode::FoldrProdFloat;
        case QoreIROpcode::FoldlDiffInt: return QoreIROpcode::FoldrDiffInt;
        case QoreIROpcode::FoldlDiffFloat: return QoreIROpcode::FoldrDiffFloat;
        case QoreIROpcode::FoldlMinInt: return QoreIROpcode::FoldrMinInt;
        case QoreIROpcode::FoldlMinFloat: return QoreIROpcode::FoldrMinFloat;
        case QoreIROpcode::FoldlMaxInt: return QoreIROpcode::FoldrMaxInt;
        case QoreIROpcode::FoldlMaxFloat: return QoreIROpcode::FoldrMaxFloat;
        default: return QoreIROpcode::FoldrAny;
    }
}

QoreIRValue QoreIRLowering::lowerFoldr(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* foldr = dynamic_cast<const QoreFoldrOperatorNode*>(node);
    if (!foldr) {
        return QoreIRValue();
    }

    {
        std::vector<LazyPipelineStage> source_stages;
        QoreValue base_source;
        if (!collectLazyPipelineStages(foldr->getIteratorExpr(), base_source, source_stages, error)) {
            return QoreIRValue();
        }
        if (!source_stages.empty()) {
            LazyPipelineRoot root;
            root.kind = LazyPipelineRoot::List;
            root.list_element_type = QoreTypeInfo::getUniqueReturnComplexList(getExprTypeInfo(foldr->getIteratorExpr()));
            root.loc = foldr->loc;
            root.need_result = true;
            QoreIRValue list = lowerLazyPipelineFused(base_source, source_stages, root, error);
            if (!list.isValid()) {
                return QoreIRValue();
            }
            return lowerFoldrNativeValue(foldr, list, error);
        }
    }

    // Try to detect optimizable pattern (reuse analyzeFoldPattern from foldl)
    const QoreTypeInfo* result_type = nullptr;
    const QoreTypeInfo* list_type = getExprTypeInfo(foldr->getRight());
    QoreIROpcode foldl_opcode = analyzeFoldPattern(foldr->getLeft(), result_type, list_type);

    if (foldl_opcode != QoreIROpcode::FoldlAny) {
        // Map foldl opcode to foldr equivalent
        QoreIROpcode opt_opcode = foldlToFoldrOpcode(foldl_opcode);

        // Emit specialized opcode with just the list operand
        QoreIRValue list = lowerExpression(foldr->getRight(), error);
        if (!list.isValid()) {
            return QoreIRValue();
        }

        // Create a dummy NOTHING value for the second operand (initial value is last list element)
        QoreIRValue init = builder.createConstNothing(foldr->loc)->result;

        QoreIRValue result;
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {list, init}, normal_block, handler, foldr->loc);
            inst->invoke_opcode = opt_opcode;
            builder.setBlock(normal_block);
            result = inst->result;
        } else {
            result = builder.createBinaryOp(opt_opcode, list, init, foldr->loc)->result;
        }
        maybeInsertNotNothingGuard(result, &expr, foldr->loc, nullptr);
        return result;
    }

    // Native IR lowering with reverse iteration
    return lowerFoldrNative(foldr, expr, error);
}

QoreIRValue QoreIRLowering::lowerMap(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* map = dynamic_cast<const QoreMapOperatorNode*>(node);
    if (!map) {
        return QoreIRValue();
    }

    // If the iterator expression is known to be nothing, return NOTHING directly
    {
        const QoreTypeInfo* iter_type = getExprTypeInfo(map->getRight());
        if (QoreTypeInfo::isType(iter_type, NT_NOTHING)) {
            return builder.createConstNothing(map->loc)->result;
        }
    }

    {
        std::vector<LazyPipelineStage> source_stages;
        QoreValue base_source;
        if (!collectLazyPipelineStages(map->getIteratorExpr(), base_source, source_stages, error)) {
            return QoreIRValue();
        }
        if (!source_stages.empty()) {
            source_stages.push_back({LazyPipelineStage::Map, &map->getMapExpression(), nullptr, map->loc});
            LazyPipelineRoot root;
            root.kind = LazyPipelineRoot::List;
            root.list_element_type = QoreTypeInfo::getUniqueReturnComplexList(getExprTypeInfo(expr));
            root.loc = map->loc;
            root.need_result = map->needsReturnValue();
            return lowerLazyPipelineFused(base_source, source_stages, root, error);
        }
    }

    // For typed lists with known element types, prefer native LLVM IR lowering
    // which generates direct-index loops that LLVM can optimize (vectorize, unroll, etc.)
    // This is faster than pattern-matched opcodes which delegate to opaque C++ runtime calls.
    {
        const QoreTypeInfo* list_type = getExprTypeInfo(map->getRight());
        const QoreTypeInfo* elem_type = QoreTypeInfo::getUniqueReturnComplexList(list_type);
        if (elem_type) {
            if (QoreTypeInfo::parseReturns(elem_type, NT_INT) == QTI_IDENT
                    || QoreTypeInfo::parseReturns(elem_type, NT_FLOAT) == QTI_IDENT) {
                return lowerMapNative(map, expr, error);
            }
        }
    }

    // Try hash-key specialized opcodes for hash-typed lists
    {
        const QoreTypeInfo* list_type = getExprTypeInfo(map->getRight());
        const QoreTypeInfo* elem_type = QoreTypeInfo::getUniqueReturnComplexList(list_type);
        if (elem_type && QoreTypeInfo::parseReturns(elem_type, NT_HASH) != QTI_NOT_EQUAL) {
            std::string key_name;
            QoreValue hk_constant;
            QoreIROpcode hk_opcode = analyzeMapHashKeyPattern(map->getLeft(), key_name, hk_constant);

            if (hk_opcode != QoreIROpcode::MapAny) {
                // Check if value type is known int for MapHashKeyInt detection
                if (hk_opcode == QoreIROpcode::MapHashKeyValue) {
                    // Check if the value type for this key is known to be int
                    const QoreTypeInfo* hash_val_type = QoreTypeInfo::getUniqueReturnComplexHash(elem_type);
                    if (hash_val_type
                            && QoreTypeInfo::parseReturns(hash_val_type, NT_INT) == QTI_IDENT) {
                        hk_opcode = QoreIROpcode::MapHashKeyInt;
                    }
                }

                // Lower the input list
                QoreIRValue list_val = lowerExpression(map->getRight(), error);
                if (!list_val.isValid()) {
                    return QoreIRValue();
                }

                // Create the specialized instruction
                auto* inst = builder.createMapHashKey(hk_opcode, key_name.c_str(), nullptr, map->loc);
                inst->operands.push_back(list_val);

                // For offset/scale patterns, add the constant operand
                if (hk_opcode == QoreIROpcode::MapHashKeyOffsetInt
                        || hk_opcode == QoreIROpcode::MapHashKeyScaleInt) {
                    QoreIRValue const_ir = lowerConstant(hk_constant, error);
                    if (!const_ir.isValid()) {
                        return QoreIRValue();
                    }
                    inst->operands.push_back(const_ir);
                }

                return inst->result;
            }
        }
    }

    // Try pattern analysis for optimized opcodes (untyped lists only)
    const QoreTypeInfo* result_type = nullptr;
    QoreValue constant_val;
    QoreIROpcode opt_opcode = analyzeMapPattern(map->getLeft(), result_type, constant_val);

    // For optimized patterns, check if we can fuse with a select operand
    if (opt_opcode != QoreIROpcode::MapAny) {
        // Skip optimized opcode if the right operand is definitely an iterator object.
        // Optimized opcodes handle NT_LIST, NOTHING, and single values, but not NT_OBJECT iterators.
        {
            // Try to get type info from both getExprTypeInfo and getAnalysis
            const QoreTypeInfo* right_type = getExprTypeInfo(map->getRight());
            QoreParseAnalysis analysis;
            if (!right_type && getAnalysis(map->getRight(), analysis)
                    && analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)) {
                right_type = analysis.known_type;
            }

            // Check if the right operand is definitely a class/object type (could be an iterator)
            if (right_type) {
                // Check if it's a specific class type (not a list)
                const QoreClass* obj_class = QoreTypeInfo::getUniqueReturnClass(right_type);
                if (obj_class) {
                    // Definitely an object/iterator — use native lowering
                    return lowerMapNative(map, expr, error);
                }
            }
        }

        // Check if the right operand (list) is a select expression with positive filter
        const AbstractQoreNode* right_node = map->getRight().getInternalNode();
        auto* inner_select = dynamic_cast<const QoreSelectOperatorNode*>(right_node);

        if (inner_select) {
            // Analyze the select pattern
            const QoreTypeInfo* select_result_type = nullptr;
            QoreIROpcode select_opcode = analyzeSelectPattern(inner_select->getRight(), select_result_type);

            // Only fuse if select pattern is SelectPositiveInt (i.e., $1 > 0)
            if (select_opcode == QoreIROpcode::SelectPositiveInt) {
                // Determine element type from the underlying list
                QoreParseAnalysis list_analysis;
                const QoreTypeInfo* list_type = nullptr;
                if (getAnalysis(inner_select->getLeft(), list_analysis) &&
                        list_analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)) {
                    list_type = list_analysis.known_type;
                }
                const QoreTypeInfo* elem_type = QoreTypeInfo::getUniqueReturnComplexList(list_type);
                bool is_float = QoreTypeInfo::isType(elem_type, NT_FLOAT);

                // Select the fused opcode based on map pattern and element type
                QoreIROpcode fused_opcode = QoreIROpcode::MapAny;
                if (opt_opcode == QoreIROpcode::MapScaleInt || opt_opcode == QoreIROpcode::MapScaleFloat) {
                    fused_opcode = is_float ? QoreIROpcode::FusedMapSelectScalePositiveFloat
                                           : QoreIROpcode::FusedMapSelectScalePositiveInt;
                } else if (opt_opcode == QoreIROpcode::MapOffsetInt || opt_opcode == QoreIROpcode::MapOffsetFloat) {
                    fused_opcode = is_float ? QoreIROpcode::FusedMapSelectOffsetPositiveFloat
                                           : QoreIROpcode::FusedMapSelectOffsetPositiveInt;
                } else if (opt_opcode == QoreIROpcode::MapSquareInt || opt_opcode == QoreIROpcode::MapSquareFloat) {
                    fused_opcode = is_float ? QoreIROpcode::FusedMapSelectSquarePositiveFloat
                                           : QoreIROpcode::FusedMapSelectSquarePositiveInt;
                }

                if (fused_opcode != QoreIROpcode::MapAny) {
                    // Lower the underlying list (left operand of select)
                    QoreIRValue list_val = lowerExpression(inner_select->getLeft(), error);
                    if (!list_val.isValid()) {
                        return QoreIRValue();
                    }

                    QoreIRValue const_ir;
                    if (fused_opcode == QoreIROpcode::FusedMapSelectSquarePositiveInt ||
                            fused_opcode == QoreIROpcode::FusedMapSelectSquarePositiveFloat) {
                        // Square doesn't need a constant, use NOTHING as placeholder
                        const_ir = builder.createConstNothing(map->loc)->result;
                    } else {
                        // Lower the constant value
                        const_ir = lowerConstant(constant_val, error);
                        if (!const_ir.isValid()) {
                            return QoreIRValue();
                        }
                    }

                    QoreIRValue result;
                    if (!exception_stack.empty()) {
                        QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
                        if (!normal_block) {
                            error = "IR builder failed to create invoke continuation block";
                            return QoreIRValue();
                        }
                        QoreIRBasicBlock* handler = exception_stack.back();
                        auto* inst = builder.createInvoke(expr, {list_val, const_ir}, normal_block, handler, map->loc);
                        inst->invoke_opcode = fused_opcode;
                        builder.setBlock(normal_block);
                        result = inst->result;
                    } else {
                        result = builder.createBinaryOp(fused_opcode, list_val, const_ir, map->loc)->result;
                    }
                    maybeInsertNotNothingGuard(result, &expr, map->loc, nullptr);
                    return result;
                }
            }
        }

        // No fusion possible, emit regular optimized map opcode
        // Lower the list (right operand of map)
        QoreIRValue list_val = lowerExpression(map->getRight(), error);
        if (!list_val.isValid()) {
            return QoreIRValue();
        }

        QoreIRValue const_ir;
        if (opt_opcode == QoreIROpcode::MapSquareInt || opt_opcode == QoreIROpcode::MapSquareFloat) {
            // Square doesn't need a constant, use NOTHING as placeholder
            const_ir = builder.createConstNothing(map->loc)->result;
        } else {
            // Lower the constant value
            const_ir = lowerConstant(constant_val, error);
            if (!const_ir.isValid()) {
                return QoreIRValue();
            }
        }

        QoreIRValue result;
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {list_val, const_ir}, normal_block, handler, map->loc);
            inst->invoke_opcode = opt_opcode;
            builder.setBlock(normal_block);
            result = inst->result;
        } else {
            result = builder.createBinaryOp(opt_opcode, list_val, const_ir, map->loc)->result;
        }
        maybeInsertNotNothingGuard(result, &expr, map->loc, nullptr);
        return result;
    }

    // Native IR lowering with implicit argument context ($1, $#)
    return lowerMapNative(map, expr, error);
}

QoreIRValue QoreIRLowering::lowerSelect(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* select = dynamic_cast<const QoreSelectOperatorNode*>(node);
    if (!select) {
        return QoreIRValue();
    }

    {
        std::vector<LazyPipelineStage> source_stages;
        QoreValue base_source;
        if (!collectLazyPipelineStages(select->getSourceExpression(), base_source, source_stages, error)) {
            return QoreIRValue();
        }
        if (!source_stages.empty()) {
            source_stages.push_back({LazyPipelineStage::Select, &select->getPredicateExpression(), nullptr,
                select->loc});
            LazyPipelineRoot root;
            root.kind = LazyPipelineRoot::List;
            root.list_element_type = QoreTypeInfo::getUniqueReturnComplexList(getExprTypeInfo(expr));
            root.loc = select->loc;
            root.need_result = select->needsReturnValue();
            return lowerLazyPipelineFused(base_source, source_stages, root, error);
        }
    }

    // Try pattern analysis for optimized opcodes
    // Syntax: (select list, condition) - getLeft() = list, getRight() = condition
    const QoreTypeInfo* result_type = nullptr;
    QoreIROpcode opt_opcode = analyzeSelectPattern(select->getRight(), result_type);

    // For optimized patterns, emit optimized opcode with just the list
    if (opt_opcode != QoreIROpcode::SelectAny) {
        // Lower the list (left operand of select)
        QoreIRValue list_val = lowerExpression(select->getLeft(), error);
        if (!list_val.isValid()) {
            return QoreIRValue();
        }

        // Determine if list has float elements to choose Int vs Float variant
        QoreParseAnalysis list_analysis;
        const QoreTypeInfo* list_type = nullptr;
        if (getAnalysis(select->getLeft(), list_analysis) &&
                list_analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)) {
            list_type = list_analysis.known_type;
        }
        const QoreTypeInfo* elem_type = QoreTypeInfo::getUniqueReturnComplexList(list_type);
        bool is_float = QoreTypeInfo::isType(elem_type, NT_FLOAT);

        // Upgrade to Float variant if needed
        if (is_float) {
            if (opt_opcode == QoreIROpcode::SelectPositiveInt) {
                opt_opcode = QoreIROpcode::SelectPositiveFloat;
            } else if (opt_opcode == QoreIROpcode::SelectNonZeroInt) {
                opt_opcode = QoreIROpcode::SelectNonZeroFloat;
            }
        }

        // Use NOTHING as placeholder for second operand (not used by optimized select)
        QoreIRValue placeholder = builder.createConstNothing(select->loc)->result;

        QoreIRValue result;
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {list_val, placeholder}, normal_block, handler, select->loc);
            inst->invoke_opcode = opt_opcode;
            builder.setBlock(normal_block);
            result = inst->result;
        } else {
            result = builder.createBinaryOp(opt_opcode, list_val, placeholder, select->loc)->result;
        }
        maybeInsertNotNothingGuard(result, &expr, select->loc, nullptr);
        return result;
    }

    // Native IR lowering with implicit argument context ($1, $#)
    return lowerSelectNative(select, expr, error);
}

QoreIRValue QoreIRLowering::lowerMapSelect(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* map_select = dynamic_cast<const QoreMapSelectOperatorNode*>(node);
    if (!map_select) {
        return QoreIRValue();
    }

    {
        std::vector<LazyPipelineStage> source_stages;
        QoreValue base_source;
        if (!collectLazyPipelineStages(map_select->getIteratorExpr(), base_source, source_stages, error)) {
            return QoreIRValue();
        }
        if (!source_stages.empty()) {
            source_stages.push_back({LazyPipelineStage::MapSelect, &map_select->getMapExpression(),
                &map_select->getSelectExpression(), map_select->loc});
            LazyPipelineRoot root;
            root.kind = LazyPipelineRoot::List;
            root.list_element_type = QoreTypeInfo::getUniqueReturnComplexList(getExprTypeInfo(expr));
            root.loc = map_select->loc;
            root.need_result = map_select->needsReturnValue();
            return lowerLazyPipelineFused(base_source, source_stages, root, error);
        }
    }

    // Native IR lowering with implicit argument context
    return lowerMapSelectNative(map_select, expr, error);
}

QoreIRValue QoreIRLowering::lowerHashMap(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* map = dynamic_cast<const QoreHashMapOperatorNode*>(node);
    if (!map) {
        return QoreIRValue();
    }

    // Try HashMapTwoKeys specialized opcode: map {$1.k1: $1.k2}, list
    {
        const QoreTypeInfo* list_type = getExprTypeInfo(map->get(2));
        const QoreTypeInfo* elem_type = QoreTypeInfo::getUniqueReturnComplexList(list_type);
        if (elem_type && QoreTypeInfo::parseReturns(elem_type, NT_HASH) != QTI_NOT_EQUAL) {
            std::string key1_name;
            std::string key2_name;
            QoreIROpcode hk_opcode = analyzeHashMapTwoKeysPattern(map->get(0), map->get(1),
                key1_name, key2_name);
            if (hk_opcode == QoreIROpcode::HashMapTwoKeys) {
                // Lower the input list
                QoreIRValue list_val = lowerExpression(map->get(2), error);
                if (!list_val.isValid()) {
                    return QoreIRValue();
                }

                auto* inst = builder.createMapHashKey(QoreIROpcode::HashMapTwoKeys,
                    key1_name.c_str(), key2_name.c_str(), map->loc);
                inst->operands.push_back(list_val);
                return inst->result;
            }
        }
    }

    // Native IR lowering with hash building
    return lowerHashMapNative(map, expr, error);
}

QoreIRValue QoreIRLowering::lowerHashMapSelect(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* map_select = dynamic_cast<const QoreHashMapSelectOperatorNode*>(node);
    if (!map_select) {
        return QoreIRValue();
    }

    // Native IR lowering with hash building + filtering
    return lowerHashMapSelectNative(map_select, expr, error);
}

QoreIRValue QoreIRLowering::lowerMapNative(const QoreMapOperatorNode* map, const QoreValue& expr,
        std::string& error) {
    if (!ensureBuilderContext(error)) {
        return QoreIRValue();
    }
    bool need_result = map->needsReturnValue();

    // Get the element type of the map result list directly from the map expression type
    const QoreTypeInfo* expTypeInfo = map->getMapExpType();
    // Fallback: extract from return type if expression type not available
    if (!expTypeInfo) {
        const QoreTypeInfo* map_return_type = map->getMapReturnType();
        if (map_return_type) {
            expTypeInfo = QoreTypeInfo::getUniqueReturnComplexList(map_return_type);
        }
    }

    // Check if the input is actually a list or a single value
    const QoreTypeInfo* list_type = getExprTypeInfo(map->getRight());
    const QoreTypeInfo* elem_type = QoreTypeInfo::getUniqueReturnComplexList(list_type);
    const QoreIterateOperatorNode* iterate_right =
        dynamic_cast<const QoreIterateOperatorNode*>(map->getRight().getInternalNode());

    // For nested map/select operators, use returnTypeInfo which provides the definitive
    // return type (set to iteratorTypeInfo for scalar inputs). This is more precise than
    // getExprTypeInfo() which may return autoTypeInfo for the body expression.
    // For nested map/select operators, use returnTypeInfo (via getMapReturnType) which
    // provides the definitive return type — set to iteratorTypeInfo for scalar inputs.
    // This is more precise than getExprTypeInfo() which may return autoTypeInfo.
    // Walk the chain of nested maps to find the innermost concrete return type.
    if (!list_type || list_type == autoTypeInfo) {
        const AbstractQoreNode* rhs = map->getRight().getInternalNode();
        while (rhs) {
            if (auto* inner_map = dynamic_cast<const QoreMapOperatorNode*>(rhs)) {
                const QoreTypeInfo* rti = inner_map->getMapReturnType();
                if (rti && rti != autoTypeInfo) {
                    list_type = rti;
                    break;
                }
                // Continue walking into the inner map's right side
                rhs = inner_map->getRight().getInternalNode();
            } else if (auto* inner_sel = dynamic_cast<const QoreMapSelectOperatorNode*>(rhs)) {
                const QoreTypeInfo* rti = inner_sel->getMapReturnType();
                if (rti && rti != autoTypeInfo) {
                    list_type = rti;
                    break;
                }
                // Continue walking into the inner select's iterator
                rhs = inner_sel->getIteratorExpr().getInternalNode();
            } else {
                break;
            }
        }
    }

    // Only enter single-value path when we have positive evidence it's NOT a list or iterator:
    // - elem_type must be null (no list element type extracted)
    // - list_type must be non-null (we have type information)
    // - parseReturns must confirm it's NOT a list
    // - type must not be an iterator class (iterator objects produce lists at runtime)
    // This guard prevents entering single-value path for unknown types (like method calls)
    bool is_iterator = false;
    if (list_type) {
        const QoreClass* obj_class = QoreTypeInfo::getUniqueReturnClass(list_type);
        if (obj_class && qore_class_private::parseCheckCompatibleClass(obj_class, QC_ABSTRACTITERATOR)) {
            is_iterator = true;
        }
    }
    if (!elem_type && list_type && !is_iterator && !QoreTypeInfo::parseReturns(list_type, NT_LIST)) {
        QoreIRValue input_val = lowerExpression(map->getRight(), error);
        if (!input_val.isValid()) {
            return QoreIRValue();
        }
        VirtualImplicitContext saved = virtual_implicit;
        virtual_implicit.arg0 = input_val;
        virtual_implicit.arg1 = QoreIRValue();
        virtual_implicit.element = builder.createConstInt(0, map->loc)->result;
        virtual_implicit.active = true;
        if (!need_result) {
            builder.createPushTempMark(map->loc);
        }
        QoreIRValue expr_result = lowerExpression(map->getLeft(), error);
        virtual_implicit = saved;
        if (!need_result) {
            builder.createDiscardTemps(map->loc);
            return builder.createConstNothing(map->loc)->result;
        }
        return expr_result;
    }

    bool use_direct_index = false;
    bool elem_is_int = false;
    bool elem_is_float = false;
    if (elem_type && !iterate_right) {
        if (QoreTypeInfo::parseReturns(elem_type, NT_INT) == QTI_IDENT) {
            use_direct_index = true;
            elem_is_int = true;
        } else if (QoreTypeInfo::parseReturns(elem_type, NT_FLOAT) == QTI_IDENT) {
            use_direct_index = true;
            elem_is_float = true;
        } else {
            // Generic typed list — use ListGetValue for any known element type
            use_direct_index = true;
        }
    }

    // Evaluate the input list and create the iterator BEFORE creating loop blocks,
    // so that any blocks created during expression evaluation (e.g., invoke.cont
    // blocks from guarded calls in try-catch) appear before the loop header in the
    // block list.

    // Evaluate the input list (right operand of map)
    QoreIRValue input_list = iterate_right
        ? lowerExpression(iterate_right->getExp(), error)
        : lowerExpression(map->getRight(), error);
    if (!input_list.isValid()) {
        return QoreIRValue();
    }

    if (use_direct_index) {
        // Direct-index loop: avoid iterator overhead for typed lists
        // Create blocks AFTER evaluating the input expression
        // No empty-list check needed: createSizedList(0) produces an empty list,
        // and the header condition (0 >= 0) immediately exits the loop.
        QoreIRBasicBlock* nothing_block = createBlock("map.direct.nothing");
        QoreIRBasicBlock* preheader_block = createBlock("map.preheader");
        QoreIRBasicBlock* header_block = createBlock("map.header");
        QoreIRBasicBlock* body_block = createBlock("map.body");
        QoreIRBasicBlock* exit_block = createBlock("map.exit");
        QoreIRBasicBlock* final_block = createBlock("map.direct.final");
        if (!nothing_block || !preheader_block || !header_block || !body_block || !exit_block || !final_block) {
            error = "IR builder failed to create blocks for map";
            return QoreIRValue();
        }
        header_block->is_loop_header = true;

        QoreIRValue nothing_check_val = builder.createConstNothing(map->loc)->result;
        QoreIRValue is_nothing = builder.createBinaryOp(QoreIROpcode::EqHard, input_list, nothing_check_val,
            map->loc)->result;
        builder.createBranchIf(is_nothing, nothing_block, preheader_block, map->loc);

        // Preheader: create pre-sized result list (size 0 for empty input is fine)
        builder.setBlock(preheader_block);
        QoreIRValue list_size = builder.createListSize(input_list, map->loc)->result;
        QoreIRValue zero = builder.createConstInt(0, map->loc)->result;
        QoreIRValue result_list;
        if (need_result) {
            result_list = builder.createSizedList(list_size, map->loc, expTypeInfo)->result;
        }
        {
            auto* br = builder.createBranch(header_block, map->loc);
            setLoopCheckpointExceptionTarget(br, header_block);
        }

        // Header block: check if index < size
        builder.setBlock(header_block);

        auto* index_phi = builder.createPhi({}, map->loc, QoreIRPhiValueKind::NativeInt);
        QoreIRValue index_val = index_phi->result;

        QoreIRValue at_end = builder.createBinaryOp(QoreIROpcode::GeInt, index_val, list_size,
            map->loc)->result;
        builder.createBranchIf(at_end, exit_block, body_block, map->loc);

        // Body block: load element, evaluate expression, store result
        builder.setBlock(body_block);

        // Load element at current index
        QoreIRValue element_val;
        if (elem_is_int) {
            element_val = builder.createListGetInt(input_list, index_val, map->loc)->result;
        } else if (elem_is_float) {
            element_val = builder.createListGetFloat(input_list, index_val, map->loc)->result;
        } else {
            element_val = builder.createListGetValue(input_list, index_val, map->loc)->result;
        }

        // Set virtual implicit context: $1 = element, $# = index (fast path for IR-lowered refs)
        VirtualImplicitContext saved = virtual_implicit;
        virtual_implicit.arg0 = element_val;
        virtual_implicit.arg1 = QoreIRValue();
        virtual_implicit.element = index_val;
        virtual_implicit.active = true;

        // Lower the map expression first - check if any AST delegation occurs
        int saved_ast_count = ast_delegate_count;
        if (!need_result) {
            builder.createPushTempMark(map->loc);
        }
        QoreIRValue expr_result = lowerExpression(map->getLeft(), error);

        // Restore virtual context
        virtual_implicit = saved;

        if (!expr_result.isValid()) {
            return QoreIRValue();
        }

        bool needs_implicit_push = (ast_delegate_count > saved_ast_count);
        if (needs_implicit_push) {
            // Body contains AST-delegated calls: push/pop implicit args to thread-local stack
            // Insert push instructions at the start of body block (after element load)
            // We need to create values in the current function
            QoreIRFunction* func = builder.getFunction();

            // Create push instructions and insert them at the right position in body_block
            // Position: after element load (first instruction(s)), before body expression
            auto push_elem = std::make_unique<QoreIRInstruction>(QoreIROpcode::PushImplicitElement);
            push_elem->result = func->createValue();
            push_elem->operands.push_back(index_val);
            push_elem->loc = map->loc;
            QoreIRValue old_element = push_elem->result;

            auto push_argv = std::make_unique<QoreIRInstruction>(QoreIROpcode::PushImplicitArg);
            push_argv->result = func->createValue();
            push_argv->operands.push_back(element_val);
            push_argv->loc = map->loc;
            QoreIRValue old_argv = push_argv->result;

            // Find insertion point: after element load instruction(s) in body_block
            // Element load is the first instruction in body_block
            size_t insert_pos = 1;  // After the element load instruction
            body_block->instructions.insert(body_block->instructions.begin() + insert_pos,
                std::move(push_elem));
            body_block->instructions.insert(body_block->instructions.begin() + insert_pos + 1,
                std::move(push_argv));

            // Emit pop after body expression (in current block which may be invoke.cont)
            builder.createPopImplicitArg(old_argv, map->loc);
            builder.createPopImplicitElement(old_element, map->loc);
        }

        if (need_result) {
            // Store result directly at index position in pre-sized list
            builder.createListSetValue(result_list, index_val, expr_result, map->loc);
        } else {
            builder.createDiscardTemps(map->loc);
        }

        // Increment index
        QoreIRValue one = builder.createConstInt(1, map->loc)->result;
        QoreIRValue next_index = builder.createBinaryOp(QoreIROpcode::AddInt, index_val, one, map->loc)->result;

        // Record body exit block
        QoreIRBasicBlock* body_exit_block = builder.getBlock();

        // Branch back to header
        {
            auto* br = builder.createBranch(header_block, map->loc);
            setLoopCheckpointExceptionTarget(br, header_block);
        }

        // Complete PHI nodes
        index_phi->incoming.push_back({zero, preheader_block});
        index_phi->incoming.push_back({next_index, body_exit_block});
        index_phi->operands.push_back(zero);
        index_phi->operands.push_back(next_index);

        // Exit block: result_list is the result (empty for size 0, filled for size > 0)
        builder.setBlock(exit_block);
        builder.createBranch(final_block, map->loc);

        builder.setBlock(nothing_block);
        QoreIRValue nothing_val = builder.createConstNothing(map->loc)->result;
        builder.createBranch(final_block, map->loc);

        builder.setBlock(final_block);
        if (!need_result) {
            return builder.createConstNothing(map->loc)->result;
        }

        std::vector<QoreIRPhiIncoming> result_incoming;
        result_incoming.push_back({result_list, exit_block});
        result_incoming.push_back({nothing_val, nothing_block});
        auto* result_phi = builder.createPhi(result_incoming, map->loc);
        return result_phi->result;
    }

    // Fallback: iterator-based loop for untyped lists

    // If the source type is unknown (for example an auto-typed local), runtime
    // scalar inputs must preserve AST semantics: map returns the mapped scalar,
    // not a single-element list. Lists and iterator objects still return lists.
    bool is_known_collection = list_type
        && (QoreTypeInfo::isListType(list_type)
            || QoreTypeInfo::getUniqueReturnClass(list_type) != nullptr);
    bool needs_runtime_unwrap_check = need_result && !is_known_collection;

    QoreIRValue is_collection_val;
    if (needs_runtime_unwrap_check) {
        is_collection_val = builder.createUnaryOp(QoreIROpcode::IsCollectionType,
                input_list, map->loc)->result;
    }

    // Create iterator from input list
    auto* iter_inst = iterate_right
        ? builder.createIteratorCreateIterate(input_list, map->loc)
        : builder.createIteratorCreate(input_list, nullptr, map->loc);
    QoreIRValue iter_val = iter_inst->result;

    // Create basic blocks for the loop structure AFTER evaluating the input
    // expression and creating the iterator
    QoreIRBasicBlock* preheader_block = createBlock("map.preheader");
    QoreIRBasicBlock* header_block = createBlock("map.header");
    QoreIRBasicBlock* body_block = createBlock("map.body");
    QoreIRBasicBlock* loop_exit_block = createBlock("map.loop_exit");
    QoreIRBasicBlock* nothing_block = createBlock("map.nothing");
    QoreIRBasicBlock* final_block = createBlock("map.final");
    if (!preheader_block || !header_block || !body_block || !loop_exit_block || !nothing_block || !final_block) {
        error = "IR builder failed to create blocks for map";
        return QoreIRValue();
    }
    header_block->is_loop_header = true;

    QoreIRBasicBlock* unwrap_check_block = nullptr;
    QoreIRBasicBlock* unwrap_get_block = nullptr;
    QoreIRBasicBlock* unwrap_nothing_block = nullptr;
    if (needs_runtime_unwrap_check) {
        unwrap_check_block = createBlock("map.unwrap_check");
        unwrap_get_block = createBlock("map.unwrap_get");
        unwrap_nothing_block = createBlock("map.unwrap_nothing");
        if (!unwrap_check_block || !unwrap_get_block || !unwrap_nothing_block) {
            error = "IR builder failed to create blocks for map unwrap";
            return QoreIRValue();
        }
    }

    // Check if iterator is null (input was NOTHING) — return NOTHING in that case
    QoreIRValue zero = builder.createConstInt(0, map->loc)->result;
    QoreIRValue iter_as_int = iter_val;
    QoreIRValue is_null = builder.createBinaryOp(QoreIROpcode::EqInt, iter_as_int, zero, map->loc)->result;
    builder.createBranchIf(is_null, nothing_block, preheader_block, map->loc);

    // Preheader: create empty result list and proceed to loop
    builder.setBlock(preheader_block);
    QoreIRValue result_list;
    if (need_result) {
        result_list = builder.createEmptyList(map->loc, expTypeInfo)->result;
    }
    QoreIRValue init_index = builder.createConstInt(0, map->loc)->result;
    {
        auto* br = builder.createBranch(header_block, map->loc);
        setLoopCheckpointExceptionTarget(br, header_block);
    }

    // Header block: create phi for index and check for next value
    builder.setBlock(header_block);

    // Create phi for index - will be completed after body block
    auto* index_phi = builder.createPhi({}, map->loc, QoreIRPhiValueKind::NativeInt);
    QoreIRValue index_val = index_phi->result;

    // Get next element from iterator (branches to exit if done, body if has element)
    auto* next_inst = builder.createIteratorNext(iter_val, loop_exit_block, body_block, map->loc);
    QoreIRValue element_val = next_inst->result;

    // Body block: set up context, evaluate expression, append result
    builder.setBlock(body_block);

    // Set virtual implicit context: $1 = element, $# = index (fast path for IR-lowered refs)
    VirtualImplicitContext saved = virtual_implicit;
    virtual_implicit.arg0 = element_val;
    virtual_implicit.arg1 = QoreIRValue();
    virtual_implicit.element = index_val;
    virtual_implicit.active = true;

    // Lower the map expression first - check if any AST delegation occurs
    int saved_ast_count = ast_delegate_count;
    if (!need_result) {
        builder.createPushTempMark(map->loc);
    }
    QoreIRValue expr_result = lowerExpression(map->getLeft(), error);

    // Restore virtual context
    virtual_implicit = saved;

    if (!expr_result.isValid()) {
        return QoreIRValue();
    }

    bool needs_implicit_push = (ast_delegate_count > saved_ast_count);
    if (needs_implicit_push) {
        // Body contains AST-delegated calls: push/pop implicit args to thread-local stack
        QoreIRFunction* func = builder.getFunction();

        auto push_elem = std::make_unique<QoreIRInstruction>(QoreIROpcode::PushImplicitElement);
        push_elem->result = func->createValue();
        push_elem->operands.push_back(index_val);
        push_elem->loc = map->loc;
        QoreIRValue old_element_iter = push_elem->result;

        auto push_argv = std::make_unique<QoreIRInstruction>(QoreIROpcode::PushImplicitArg);
        push_argv->result = func->createValue();
        push_argv->operands.push_back(element_val);
        push_argv->loc = map->loc;
        QoreIRValue old_argv_iter = push_argv->result;

        // Insert push instructions at the start of body_block
        body_block->instructions.insert(body_block->instructions.begin(),
            std::move(push_argv));
        body_block->instructions.insert(body_block->instructions.begin(),
            std::move(push_elem));

        // Emit pop after body expression (in current block which may be invoke.cont)
        builder.createPopImplicitArg(old_argv_iter, map->loc);
        builder.createPopImplicitElement(old_element_iter, map->loc);
    }

    if (need_result) {
        // Append result to output list
        builder.createListAppend(result_list, expr_result, map->loc);
    } else {
        builder.createDiscardTemps(map->loc);
    }

    // Increment index for next iteration
    QoreIRValue one = builder.createConstInt(1, map->loc)->result;
    QoreIRValue next_index = builder.createBinaryOp(QoreIROpcode::AddInt, index_val, one, map->loc)->result;

    // Record the body exit block before branching
    QoreIRBasicBlock* body_exit_block = builder.getBlock();

    // Branch back to header
    {
        auto* br = builder.createBranch(header_block, map->loc);
        setLoopCheckpointExceptionTarget(br, header_block);
    }

    // Complete the phi node with incoming values
    index_phi->incoming.push_back({init_index, preheader_block});
    index_phi->incoming.push_back({next_index, body_exit_block});
    index_phi->operands.push_back(init_index);
    index_phi->operands.push_back(next_index);

    // Nothing block: input was NOTHING → return NOTHING
    builder.setBlock(nothing_block);
    QoreIRValue nothing_val = builder.createConstNothing(map->loc)->result;
    builder.createBranch(final_block, map->loc);

    builder.setBlock(loop_exit_block);
    if (needs_runtime_unwrap_check) {
        builder.createBranchIf(is_collection_val, final_block, unwrap_check_block, map->loc);

        builder.setBlock(unwrap_check_block);
        QoreIRValue list_size = builder.createListSize(result_list, map->loc)->result;
        builder.createBranchIf(list_size, unwrap_get_block, unwrap_nothing_block, map->loc);

        builder.setBlock(unwrap_get_block);
        QoreIRValue zero_idx = builder.createConstInt(0, map->loc)->result;
        QoreIRValue unwrapped = builder.createBinaryOp(QoreIROpcode::ListGetValue,
                result_list, zero_idx, map->loc)->result;
        builder.createBranch(final_block, map->loc);

        builder.setBlock(unwrap_nothing_block);
        QoreIRValue unwrap_nothing_val = builder.createConstNothing(map->loc)->result;
        builder.createBranch(final_block, map->loc);

        builder.setBlock(final_block);
        std::vector<QoreIRPhiIncoming> result_incoming;
        result_incoming.push_back({result_list, loop_exit_block});
        result_incoming.push_back({unwrapped, unwrap_get_block});
        result_incoming.push_back({unwrap_nothing_val, unwrap_nothing_block});
        result_incoming.push_back({nothing_val, nothing_block});
        auto* result_phi = builder.createPhi(result_incoming, map->loc);

        return result_phi->result;
    }

    builder.createBranch(final_block, map->loc);

    // Final block: PHI between result_list (from loop) and NOTHING (from null check)
    builder.setBlock(final_block);
    if (!need_result) {
        return builder.createConstNothing(map->loc)->result;
    }

    std::vector<QoreIRPhiIncoming> result_incoming;
    result_incoming.push_back({result_list, loop_exit_block}); // Normal loop exit
    result_incoming.push_back({nothing_val, nothing_block});  // NOTHING input
    auto* result_phi = builder.createPhi(result_incoming, map->loc);

    return result_phi->result;
}

QoreIRValue QoreIRLowering::lowerSelectNative(const QoreSelectOperatorNode* select, const QoreValue& expr,
        std::string& error) {
    if (!ensureBuilderContext(error)) {
        return QoreIRValue();
    }

    // Check if the input list has a known element type for direct-index optimization
    const QoreTypeInfo* list_type = getExprTypeInfo(select->getLeft());
    const QoreTypeInfo* elem_type = QoreTypeInfo::getUniqueReturnComplexList(list_type);
    bool use_direct_index = (elem_type != nullptr);
    bool elem_is_int = false;
    bool elem_is_float = false;
    if (elem_type) {
        if (QoreTypeInfo::parseReturns(elem_type, NT_INT) == QTI_IDENT) {
            elem_is_int = true;
        } else if (QoreTypeInfo::parseReturns(elem_type, NT_FLOAT) == QTI_IDENT) {
            elem_is_float = true;
        }
    }

    // Evaluate the input list (left operand of select)
    QoreIRValue input_list = lowerExpression(select->getLeft(), error);
    if (!input_list.isValid()) {
        return QoreIRValue();
    }

    if (use_direct_index) {
        // Direct-index loop: avoid iterator overhead for typed lists
        // Create blocks AFTER evaluating the input expression
        // No empty check needed: createEmptyList produces an empty list for empty input,
        // and the header condition (0 >= 0) immediately exits the loop.
        QoreIRBasicBlock* nothing_block = createBlock("select.direct.nothing");
        QoreIRBasicBlock* preheader_block = createBlock("select.preheader");
        QoreIRBasicBlock* header_block = createBlock("select.header");
        QoreIRBasicBlock* body_block = createBlock("select.body");
        QoreIRBasicBlock* append_block = createBlock("select.append");
        QoreIRBasicBlock* cont_block = createBlock("select.cont");
        QoreIRBasicBlock* exit_block = createBlock("select.exit");
        QoreIRBasicBlock* final_block = createBlock("select.direct.final");
        if (!nothing_block || !preheader_block || !header_block || !body_block
                || !append_block || !cont_block || !exit_block || !final_block) {
            error = "IR builder failed to create blocks for select";
            return QoreIRValue();
        }
        header_block->is_loop_header = true;

        QoreIRValue nothing_check_val = builder.createConstNothing(select->loc)->result;
        QoreIRValue is_nothing = builder.createBinaryOp(QoreIROpcode::EqHard, input_list, nothing_check_val,
            select->loc)->result;
        builder.createBranchIf(is_nothing, nothing_block, preheader_block, select->loc);

        // Preheader: create empty result list (filtered, size unknown)
        // Preserve input element type so the result has correct type info
        builder.setBlock(preheader_block);
        QoreIRValue list_size = builder.createListSize(input_list, select->loc)->result;
        QoreIRValue zero = builder.createConstInt(0, select->loc)->result;
        QoreIRValue result_list = builder.createEmptyList(select->loc, elem_type)->result;
        {
            auto* br = builder.createBranch(header_block, select->loc);
            setLoopCheckpointExceptionTarget(br, header_block);
        }

        // Header block: check if index < size
        builder.setBlock(header_block);

        auto* index_phi = builder.createPhi({}, select->loc, QoreIRPhiValueKind::NativeInt);
        QoreIRValue index_val = index_phi->result;

        QoreIRValue at_end = builder.createBinaryOp(QoreIROpcode::GeInt, index_val, list_size,
            select->loc)->result;
        builder.createBranchIf(at_end, exit_block, body_block, select->loc);

        // Body block: load element, evaluate predicate
        builder.setBlock(body_block);

        // Load element at current index
        QoreIRValue element_val;
        if (elem_is_int) {
            element_val = builder.createListGetInt(input_list, index_val, select->loc)->result;
        } else if (elem_is_float) {
            element_val = builder.createListGetFloat(input_list, index_val, select->loc)->result;
        } else {
            element_val = builder.createListGetValue(input_list, index_val, select->loc)->result;
        }

        // Set virtual implicit context
        VirtualImplicitContext saved = virtual_implicit;
        virtual_implicit.arg0 = element_val;
        virtual_implicit.arg1 = QoreIRValue();
        virtual_implicit.element = index_val;
        virtual_implicit.active = true;

        // Lower predicate - track AST delegation
        int saved_ast_count = ast_delegate_count;
        QoreIRValue predicate_result = lowerExpression(select->getRight(), error);

        // Restore virtual context
        virtual_implicit = saved;

        if (!predicate_result.isValid()) {
            return QoreIRValue();
        }

        bool needs_implicit_push = (ast_delegate_count > saved_ast_count);
        if (needs_implicit_push) {
            QoreIRFunction* func = builder.getFunction();

            auto push_elem = std::make_unique<QoreIRInstruction>(QoreIROpcode::PushImplicitElement);
            push_elem->result = func->createValue();
            push_elem->operands.push_back(index_val);
            push_elem->loc = select->loc;
            QoreIRValue old_element = push_elem->result;

            auto push_argv = std::make_unique<QoreIRInstruction>(QoreIROpcode::PushImplicitArg);
            push_argv->result = func->createValue();
            push_argv->operands.push_back(element_val);
            push_argv->loc = select->loc;
            QoreIRValue old_argv = push_argv->result;

            size_t insert_pos = 1;  // After element load
            body_block->instructions.insert(body_block->instructions.begin() + insert_pos,
                std::move(push_elem));
            body_block->instructions.insert(body_block->instructions.begin() + insert_pos + 1,
                std::move(push_argv));

            builder.createPopImplicitArg(old_argv, select->loc);
            builder.createPopImplicitElement(old_element, select->loc);
        }

        // Convert predicate to bool
        QoreIRValue predicate_bool = builder.createUnaryOp(QoreIROpcode::ToBool, predicate_result,
            select->loc)->result;

        // Branch: if true append, else skip
        builder.createBranchIf(predicate_bool, append_block, cont_block, select->loc);

        // Append block: add element to result list
        builder.setBlock(append_block);
        builder.createListAppend(result_list, element_val, select->loc);
        builder.createBranch(cont_block, select->loc);

        // Continue block: increment index and loop back
        builder.setBlock(cont_block);

        QoreIRValue one = builder.createConstInt(1, select->loc)->result;
        QoreIRValue next_index = builder.createBinaryOp(QoreIROpcode::AddInt, index_val, one,
            select->loc)->result;

        QoreIRBasicBlock* cont_exit_block = builder.getBlock();

        {
            auto* br = builder.createBranch(header_block, select->loc);
            setLoopCheckpointExceptionTarget(br, header_block);
        }

        // Complete PHI nodes
        index_phi->incoming.push_back({zero, preheader_block});
        index_phi->incoming.push_back({next_index, cont_exit_block});
        index_phi->operands.push_back(zero);
        index_phi->operands.push_back(next_index);

        // Exit block: result_list is the result (empty for size 0)
        builder.setBlock(exit_block);
        builder.createBranch(final_block, select->loc);

        builder.setBlock(nothing_block);
        QoreIRValue nothing_val = builder.createConstNothing(select->loc)->result;
        builder.createBranch(final_block, select->loc);

        builder.setBlock(final_block);
        std::vector<QoreIRPhiIncoming> result_incoming;
        result_incoming.push_back({result_list, exit_block});
        result_incoming.push_back({nothing_val, nothing_block});
        auto* result_phi = builder.createPhi(result_incoming, select->loc);

        return result_phi->result;
    }

    // Fallback: iterator-based loop for untyped lists

    // Check at compile time if we know whether the input is a collection (list/object)
    // or a scalar.  For unknown (auto) types, we need a runtime check.
    bool is_known_collection = list_type
        && (QoreTypeInfo::isListType(list_type)
            || QoreTypeInfo::getUniqueReturnClass(list_type) != nullptr);
    bool needs_runtime_unwrap_check = !is_known_collection;

    // For runtime unwrap check: determine if input is a collection before creating
    // the iterator (type check on the original value)
    QoreIRValue is_collection_val;
    if (needs_runtime_unwrap_check) {
        is_collection_val = builder.createUnaryOp(QoreIROpcode::IsCollectionType,
                input_list, select->loc)->result;
    }

    // Create iterator from input list
    auto* iter_inst = builder.createIteratorCreate(input_list, nullptr, select->loc);
    QoreIRValue iter_val = iter_inst->result;

    // Create basic blocks AFTER evaluating the input expression
    QoreIRBasicBlock* preheader_block = createBlock("select.preheader");
    QoreIRBasicBlock* header_block = createBlock("select.header");
    QoreIRBasicBlock* body_block = createBlock("select.body");
    QoreIRBasicBlock* append_block = createBlock("select.append");
    QoreIRBasicBlock* cont_block = createBlock("select.cont");
    QoreIRBasicBlock* loop_exit_block = createBlock("select.loop_exit");
    QoreIRBasicBlock* nothing_block = createBlock("select.nothing");
    QoreIRBasicBlock* final_block = createBlock("select.final");
    if (!preheader_block || !header_block || !body_block || !append_block
            || !cont_block || !loop_exit_block || !nothing_block || !final_block) {
        error = "IR builder failed to create blocks for select";
        return QoreIRValue();
    }
    header_block->is_loop_header = true;

    // Additional blocks for scalar unwrapping (only when needed)
    QoreIRBasicBlock* unwrap_check_block = nullptr;
    QoreIRBasicBlock* unwrap_get_block = nullptr;
    QoreIRBasicBlock* unwrap_nothing_block = nullptr;
    if (needs_runtime_unwrap_check) {
        unwrap_check_block = createBlock("select.unwrap_check");
        unwrap_get_block = createBlock("select.unwrap_get");
        unwrap_nothing_block = createBlock("select.unwrap_nothing");
        if (!unwrap_check_block || !unwrap_get_block || !unwrap_nothing_block) {
            error = "IR builder failed to create blocks for select unwrap";
            return QoreIRValue();
        }
    }

    // Check if iterator is null (input was NOTHING) → return NOTHING
    QoreIRValue zero = builder.createConstInt(0, select->loc)->result;
    QoreIRValue is_null = builder.createBinaryOp(QoreIROpcode::EqInt, iter_val, zero, select->loc)->result;
    builder.createBranchIf(is_null, nothing_block, preheader_block, select->loc);

    // Preheader: create empty result list and proceed to loop
    builder.setBlock(preheader_block);
    QoreIRValue result_list = builder.createEmptyList(select->loc)->result;
    QoreIRValue init_index = builder.createConstInt(0, select->loc)->result;
    {
        auto* br = builder.createBranch(header_block, select->loc);
        setLoopCheckpointExceptionTarget(br, header_block);
    }

    // Header block: create phi for index and check for next value
    builder.setBlock(header_block);

    auto* index_phi = builder.createPhi({}, select->loc, QoreIRPhiValueKind::NativeInt);
    QoreIRValue index_val = index_phi->result;

    // Get next element from iterator
    auto* next_inst = builder.createIteratorNext(iter_val, loop_exit_block, body_block, select->loc);
    QoreIRValue element_val = next_inst->result;

    // Body block: set up context, evaluate predicate
    builder.setBlock(body_block);

    // Push implicit args to thread-local stack (needed for AST-delegated sub-expressions)
    QoreIRValue old_element_sel = builder.createPushImplicitElement(index_val, select->loc)->result;
    QoreIRValue old_argv_sel = builder.createPushImplicitArg(element_val, select->loc)->result;

    // Set virtual implicit context: $1 = element, $# = index (fast path for IR-lowered refs)
    VirtualImplicitContext saved = virtual_implicit;
    virtual_implicit.arg0 = element_val;
    virtual_implicit.arg1 = QoreIRValue();
    virtual_implicit.element = index_val;
    virtual_implicit.active = true;

    // Lower the predicate expression (right operand of select)
    QoreIRValue predicate_result = lowerExpression(select->getRight(), error);

    // Restore virtual context and thread-local stack
    virtual_implicit = saved;
    builder.createPopImplicitArg(old_argv_sel, select->loc);
    builder.createPopImplicitElement(old_element_sel, select->loc);

    if (!predicate_result.isValid()) {
        return QoreIRValue();
    }

    // Convert predicate to bool
    QoreIRValue predicate_bool = builder.createUnaryOp(QoreIROpcode::ToBool, predicate_result, select->loc)->result;

    // Branch based on predicate: if true append, else skip
    builder.createBranchIf(predicate_bool, append_block, cont_block, select->loc);

    // Append block: add element to result list
    builder.setBlock(append_block);
    builder.createListAppend(result_list, element_val, select->loc);
    builder.createBranch(cont_block, select->loc);

    // Continue block: increment index and loop back
    builder.setBlock(cont_block);

    QoreIRValue one = builder.createConstInt(1, select->loc)->result;
    QoreIRValue next_index = builder.createBinaryOp(QoreIROpcode::AddInt, index_val, one, select->loc)->result;

    QoreIRBasicBlock* cont_exit_block = builder.getBlock();

    {
        auto* br = builder.createBranch(header_block, select->loc);
        setLoopCheckpointExceptionTarget(br, header_block);
    }

    // Complete the phi node
    index_phi->incoming.push_back({init_index, preheader_block});
    index_phi->incoming.push_back({next_index, cont_exit_block});
    index_phi->operands.push_back(init_index);
    index_phi->operands.push_back(next_index);

    // Nothing block: input was NOTHING → return NOTHING
    builder.setBlock(nothing_block);
    QoreIRValue nothing_val = builder.createConstNothing(select->loc)->result;
    builder.createBranch(final_block, select->loc);

    // Loop exit block: iterator exhausted → decide whether to return list or unwrap
    builder.setBlock(loop_exit_block);

    if (needs_runtime_unwrap_check) {
        // Runtime check: if input was a collection (list/object), return result_list as-is
        builder.createBranchIf(is_collection_val, final_block, unwrap_check_block, select->loc);

        // Unwrap check: scalar input — check if result_list has any elements
        builder.setBlock(unwrap_check_block);
        QoreIRValue list_size = builder.createListSize(result_list, select->loc)->result;
        builder.createBranchIf(list_size, unwrap_get_block, unwrap_nothing_block, select->loc);

        // Unwrap get: return result_list[0] (scalar matched predicate)
        builder.setBlock(unwrap_get_block);
        QoreIRValue zero_idx = builder.createConstInt(0, select->loc)->result;
        QoreIRValue unwrapped = builder.createBinaryOp(QoreIROpcode::ListGetValue,
                result_list, zero_idx, select->loc)->result;
        builder.createBranch(final_block, select->loc);

        // Unwrap nothing: scalar didn't match predicate → return NOTHING
        builder.setBlock(unwrap_nothing_block);
        QoreIRValue unwrap_nothing_val = builder.createConstNothing(select->loc)->result;
        builder.createBranch(final_block, select->loc);

        // Final block: PHI merging all paths
        builder.setBlock(final_block);
        std::vector<QoreIRPhiIncoming> result_incoming;
        result_incoming.push_back({result_list, loop_exit_block});      // collection → return list
        result_incoming.push_back({unwrapped, unwrap_get_block});        // scalar matched → return scalar
        result_incoming.push_back({unwrap_nothing_val, unwrap_nothing_block}); // scalar didn't match → NOTHING
        result_incoming.push_back({nothing_val, nothing_block});         // NOTHING input → NOTHING
        auto* result_phi = builder.createPhi(result_incoming, select->loc);

        return result_phi->result;
    } else {
        // Compile-time known collection: return result_list directly
        builder.createBranch(final_block, select->loc);

        // Final block: PHI between result_list and NOTHING
        builder.setBlock(final_block);
        std::vector<QoreIRPhiIncoming> result_incoming;
        result_incoming.push_back({result_list, loop_exit_block});
        result_incoming.push_back({nothing_val, nothing_block});
        auto* result_phi = builder.createPhi(result_incoming, select->loc);

        return result_phi->result;
    }
}

QoreIRValue QoreIRLowering::lowerFoldlNative(const QoreFoldlOperatorNode* foldl, const QoreValue& expr,
        std::string& error) {
    if (!ensureBuilderContext(error)) {
        return QoreIRValue();
    }

    // Check if the input list has a known element type for direct-index optimization
    const QoreTypeInfo* list_type = getExprTypeInfo(foldl->getRight());
    const QoreTypeInfo* elem_type = QoreTypeInfo::getUniqueReturnComplexList(list_type);
    bool use_direct_index = false;
    bool elem_is_int = false;
    bool elem_is_float = false;
    if (elem_type) {
        if (QoreTypeInfo::parseReturns(elem_type, NT_INT) == QTI_IDENT) {
            use_direct_index = true;
            elem_is_int = true;
        } else if (QoreTypeInfo::parseReturns(elem_type, NT_FLOAT) == QTI_IDENT) {
            use_direct_index = true;
            elem_is_float = true;
        }
    }

    // Evaluate the input list and create the iterator BEFORE creating loop blocks,
    // so that any blocks created during expression evaluation (e.g., invoke.cont
    // blocks from guarded calls in try-catch) appear before the loop header in the
    // block list.

    // Evaluate the input list (right operand of foldl)
    QoreIRValue input_list = lowerExpression(foldl->getRight(), error);
    if (!input_list.isValid()) {
        return QoreIRValue();
    }

    if (use_direct_index) {
        // Direct-index loop: avoid iterator overhead for typed lists
        // Get list size
        QoreIRValue list_size = builder.createListSize(input_list, foldl->loc)->result;

        // Create blocks AFTER evaluating the input expression
        QoreIRBasicBlock* empty_check_block = createBlock("foldl.empty.check");
        QoreIRBasicBlock* init_block = createBlock("foldl.init");
        QoreIRBasicBlock* header_block = createBlock("foldl.header");
        QoreIRBasicBlock* body_block = createBlock("foldl.body");
        QoreIRBasicBlock* exit_block = createBlock("foldl.exit");
        if (!empty_check_block || !init_block || !header_block || !body_block || !exit_block) {
            error = "IR builder failed to create blocks for foldl";
            return QoreIRValue();
        }
        header_block->is_loop_header = true;

        builder.createBranch(empty_check_block, foldl->loc);

        // Empty check: if size == 0, return NOTHING
        builder.setBlock(empty_check_block);
        QoreIRValue zero = builder.createConstInt(0, foldl->loc)->result;
        QoreIRValue is_empty = builder.createBinaryOp(QoreIROpcode::EqInt, list_size, zero, foldl->loc)->result;
        builder.createBranchIf(is_empty, exit_block, init_block, foldl->loc);

        // Init block: load first element as accumulator, start index at 1
        builder.setBlock(init_block);
        QoreIRValue first_val;
        if (elem_is_int) {
            first_val = builder.createListGetInt(input_list, zero, foldl->loc)->result;
        } else {
            first_val = builder.createListGetFloat(input_list, zero, foldl->loc)->result;
        }
        QoreIRValue one = builder.createConstInt(1, foldl->loc)->result;
        {
            auto* br = builder.createBranch(header_block, foldl->loc);
            setLoopCheckpointExceptionTarget(br, header_block);
        }

        // Header block: check if index < size
        builder.setBlock(header_block);

        // PHI for index
        auto* index_phi = builder.createPhi({}, foldl->loc, QoreIRPhiValueKind::NativeInt);
        QoreIRValue index_val = index_phi->result;

        // PHI for accumulator
        auto* accum_phi = builder.createPhi({}, foldl->loc);
        QoreIRValue accum_val = accum_phi->result;

        // Check: index < size
        QoreIRValue at_end = builder.createBinaryOp(QoreIROpcode::GeInt, index_val, list_size,
            foldl->loc)->result;
        builder.createBranchIf(at_end, exit_block, body_block, foldl->loc);

        // Body block: load element, evaluate fold expression
        builder.setBlock(body_block);

        // Load element at current index
        QoreIRValue element_val;
        if (elem_is_int) {
            element_val = builder.createListGetInt(input_list, index_val, foldl->loc)->result;
        } else {
            element_val = builder.createListGetFloat(input_list, index_val, foldl->loc)->result;
        }

        // Push implicit args to thread-local stack (needed for AST-delegated sub-expressions)
        QoreIRValue argv_list_typed = builder.createEmptyList(foldl->loc)->result;
        builder.createListAppend(argv_list_typed, accum_val, foldl->loc);
        builder.createListAppend(argv_list_typed, element_val, foldl->loc);
        QoreIRValue old_argv_typed = builder.createSetImplicitArgv(argv_list_typed, foldl->loc)->result;

        // Set virtual implicit context: $1 = accumulator, $2 = element (fast path for IR-lowered refs)
        VirtualImplicitContext saved = virtual_implicit;
        virtual_implicit.arg0 = accum_val;
        virtual_implicit.arg1 = element_val;
        virtual_implicit.element = QoreIRValue();
        virtual_implicit.active = true;

        // Lower the fold expression - $1 and $2 resolved via virtual context (IR) or thread-local stack (AST)
        QoreIRValue fold_result = lowerExpression(foldl->getLeft(), error);

        // Restore virtual context and thread-local stack
        virtual_implicit = saved;
        builder.createPopImplicitArg(old_argv_typed, foldl->loc);

        if (!fold_result.isValid()) {
            return QoreIRValue();
        }

        // Increment index
        QoreIRValue next_index = builder.createBinaryOp(QoreIROpcode::AddInt, index_val, one, foldl->loc)->result;

        // Record body exit block (expression lowering may create new blocks)
        QoreIRBasicBlock* body_exit_block = builder.getBlock();

        // Branch back to header
        {
            auto* br = builder.createBranch(header_block, foldl->loc);
            setLoopCheckpointExceptionTarget(br, header_block);
        }

        // Complete PHI nodes
        index_phi->incoming.push_back({one, init_block});
        index_phi->incoming.push_back({next_index, body_exit_block});
        index_phi->operands.push_back(one);
        index_phi->operands.push_back(next_index);

        accum_phi->incoming.push_back({first_val, init_block});
        accum_phi->incoming.push_back({fold_result, body_exit_block});
        accum_phi->operands.push_back(first_val);
        accum_phi->operands.push_back(fold_result);

        // Exit block: PHI between identity value (empty) and accumulator
        builder.setBlock(exit_block);

        QoreIRValue identity_val = elem_is_int
            ? builder.createConstInt(0, foldl->loc)->result
            : builder.createConstFloat(0.0, foldl->loc)->result;

        std::vector<QoreIRPhiIncoming> result_incoming;
        result_incoming.push_back({identity_val, empty_check_block});  // Empty list case
        result_incoming.push_back({accum_val, header_block});          // Normal case after iterations

        auto* result_phi = builder.createPhi(result_incoming, foldl->loc);

        return result_phi->result;
    }

    // Fallback: iterator-based loop for untyped lists

    // Create iterator from input list
    auto* iter_inst = builder.createIteratorCreate(input_list, nullptr, foldl->loc);
    QoreIRValue iter_val = iter_inst->result;

    // Create basic blocks for the loop structure AFTER evaluating the input
    // expression and creating the iterator
    QoreIRBasicBlock* init_block = createBlock("foldl.init");
    QoreIRBasicBlock* header_block = createBlock("foldl.header");
    QoreIRBasicBlock* body_block = createBlock("foldl.body");
    QoreIRBasicBlock* exit_block = createBlock("foldl.exit");
    if (!init_block || !header_block || !body_block || !exit_block) {
        error = "IR builder failed to create blocks for foldl";
        return QoreIRValue();
    }
    header_block->is_loop_header = true;

    // Branch to init block to get first element
    builder.createBranch(init_block, foldl->loc);

    // Init block: get first element as initial accumulator
    builder.setBlock(init_block);
    auto* first_inst = builder.createIteratorNext(iter_val, exit_block, header_block, foldl->loc);
    QoreIRValue first_val = first_inst->result;

    // Header block: check for next element
    builder.setBlock(header_block);

    // Create phi for accumulator - will be completed after body block
    auto* accum_phi = builder.createPhi({}, foldl->loc);
    QoreIRValue accum_val = accum_phi->result;

    // Get next element from iterator
    auto* next_inst = builder.createIteratorNext(iter_val, exit_block, body_block, foldl->loc);
    QoreIRValue element_val = next_inst->result;

    // Body block: set up context with both $1 (accumulator) and $2 (element)
    builder.setBlock(body_block);

    // Push implicit args to thread-local stack (needed for AST-delegated sub-expressions)
    QoreIRValue argv_list_iter = builder.createEmptyList(foldl->loc)->result;
    builder.createListAppend(argv_list_iter, accum_val, foldl->loc);
    builder.createListAppend(argv_list_iter, element_val, foldl->loc);
    QoreIRValue old_argv_iter = builder.createSetImplicitArgv(argv_list_iter, foldl->loc)->result;

    // Set virtual implicit context: $1 = accumulator, $2 = element (fast path for IR-lowered refs)
    VirtualImplicitContext saved = virtual_implicit;
    virtual_implicit.arg0 = accum_val;
    virtual_implicit.arg1 = element_val;
    virtual_implicit.element = QoreIRValue();
    virtual_implicit.active = true;

    // Lower the fold expression - $1 and $2 resolved via virtual context (IR) or thread-local stack (AST)
    QoreIRValue fold_result = lowerExpression(foldl->getLeft(), error);

    // Restore virtual context and thread-local stack
    virtual_implicit = saved;
    builder.createPopImplicitArg(old_argv_iter, foldl->loc);

    if (!fold_result.isValid()) {
        return QoreIRValue();
    }

    // Record body exit block
    QoreIRBasicBlock* body_exit_block = builder.getBlock();

    // Branch back to header with new accumulator
    {
        auto* br = builder.createBranch(header_block, foldl->loc);
        setLoopCheckpointExceptionTarget(br, header_block);
    }

    // Complete the accumulator phi node
    accum_phi->incoming.push_back({first_val, init_block});
    accum_phi->incoming.push_back({fold_result, body_exit_block});
    accum_phi->operands.push_back(first_val);
    accum_phi->operands.push_back(fold_result);

    // Exit block: return accumulator (via phi from different sources)
    builder.setBlock(exit_block);

    // Create phi for final result
    // - If empty list, exit from init_block with NOTHING (first_inst goes to exit)
    // - If single element, exit from init_block with that element
    // - If multiple elements, exit from header with accumulator
    // The IteratorNext handles this: done_target is exit_block
    // So we get: NOTHING from init (empty list) or accum_val from header

    // For empty list case, we need to return NOTHING
    QoreIRValue nothing_val = builder.createConstNothing(foldl->loc)->result;

    std::vector<QoreIRPhiIncoming> result_incoming;
    result_incoming.push_back({nothing_val, init_block});  // Empty list case
    result_incoming.push_back({accum_val, header_block});  // Normal case after iterations

    auto* result_phi = builder.createPhi(result_incoming, foldl->loc);

    return result_phi->result;
}

QoreIRValue QoreIRLowering::lowerFoldrNative(const QoreFoldrOperatorNode* foldr, const QoreValue& expr,
        std::string& error) {
    if (!ensureBuilderContext(error)) {
        return QoreIRValue();
    }

    // Evaluate the input list (right operand of foldr)
    QoreIRValue input_list = lowerExpression(foldr->getRight(), error);
    if (!input_list.isValid()) {
        return QoreIRValue();
    }

    return lowerFoldrNativeValue(foldr, input_list, error);
}

QoreIRValue QoreIRLowering::lowerFoldrNativeValue(const QoreFoldrOperatorNode* foldr, QoreIRValue input_list,
        std::string& error) {
    if (!ensureBuilderContext(error)) {
        return QoreIRValue();
    }

    // foldr is identical to foldl except with reverse iteration

    // Create reverse iterator from input list
    auto* iter_inst = builder.createIteratorCreateReverse(input_list, foldr->loc);
    QoreIRValue iter_val = iter_inst->result;

    // Create basic blocks for the loop structure AFTER evaluating the input
    // expression and creating the iterator
    QoreIRBasicBlock* init_block = createBlock("foldr.init");
    QoreIRBasicBlock* header_block = createBlock("foldr.header");
    QoreIRBasicBlock* body_block = createBlock("foldr.body");
    QoreIRBasicBlock* exit_block = createBlock("foldr.exit");
    if (!init_block || !header_block || !body_block || !exit_block) {
        error = "IR builder failed to create blocks for foldr";
        return QoreIRValue();
    }
    header_block->is_loop_header = true;

    // Branch to init block to get first element
    builder.createBranch(init_block, foldr->loc);

    // Init block: get first element as initial accumulator
    builder.setBlock(init_block);
    auto* first_inst = builder.createIteratorNext(iter_val, exit_block, header_block, foldr->loc);
    QoreIRValue first_val = first_inst->result;

    // Header block: check for next element
    builder.setBlock(header_block);

    // Create phi for accumulator
    auto* accum_phi = builder.createPhi({}, foldr->loc);
    QoreIRValue accum_val = accum_phi->result;

    // Get next element from iterator
    auto* next_inst = builder.createIteratorNext(iter_val, exit_block, body_block, foldr->loc);
    QoreIRValue element_val = next_inst->result;

    // Body block: set up context with both $1 (accumulator) and $2 (element)
    builder.setBlock(body_block);

    // Push implicit args to thread-local stack (needed for AST-delegated sub-expressions)
    QoreIRValue argv_list_foldr = builder.createEmptyList(foldr->loc)->result;
    builder.createListAppend(argv_list_foldr, accum_val, foldr->loc);
    builder.createListAppend(argv_list_foldr, element_val, foldr->loc);
    QoreIRValue old_argv_foldr = builder.createSetImplicitArgv(argv_list_foldr, foldr->loc)->result;

    // Set virtual implicit context: $1 = accumulator, $2 = element (fast path for IR-lowered refs)
    VirtualImplicitContext saved = virtual_implicit;
    virtual_implicit.arg0 = accum_val;
    virtual_implicit.arg1 = element_val;
    virtual_implicit.element = QoreIRValue();
    virtual_implicit.active = true;

    // Lower the fold expression - $1 and $2 resolved via virtual context (IR) or thread-local stack (AST)
    QoreIRValue fold_result = lowerExpression(foldr->getLeft(), error);

    // Restore virtual context and thread-local stack
    virtual_implicit = saved;
    builder.createPopImplicitArg(old_argv_foldr, foldr->loc);

    if (!fold_result.isValid()) {
        return QoreIRValue();
    }

    // Record body exit block
    QoreIRBasicBlock* body_exit_block = builder.getBlock();

    // Branch back to header with new accumulator
    {
        auto* br = builder.createBranch(header_block, foldr->loc);
        setLoopCheckpointExceptionTarget(br, header_block);
    }

    // Complete the accumulator phi node
    accum_phi->incoming.push_back({first_val, init_block});
    accum_phi->incoming.push_back({fold_result, body_exit_block});
    accum_phi->operands.push_back(first_val);
    accum_phi->operands.push_back(fold_result);

    // Exit block: return accumulator (via phi from different sources)
    builder.setBlock(exit_block);

    // For empty list case, return NOTHING
    QoreIRValue nothing_val = builder.createConstNothing(foldr->loc)->result;

    std::vector<QoreIRPhiIncoming> result_incoming;
    result_incoming.push_back({nothing_val, init_block});  // Empty list case
    result_incoming.push_back({accum_val, header_block});  // Normal case after iterations

    auto* result_phi = builder.createPhi(result_incoming, foldr->loc);

    return result_phi->result;
}

QoreIRValue QoreIRLowering::lowerMapSelectNative(const QoreMapSelectOperatorNode* ms, const QoreValue& expr,
        std::string& error) {
    if (!ensureBuilderContext(error)) {
        return QoreIRValue();
    }
    bool need_result = ms->needsReturnValue();

    // e[0] = map expression, e[1] = iterator/input, e[2] = select predicate

    // Check if the input list has a known element type for direct-index optimization
    const QoreTypeInfo* list_type = getExprTypeInfo(ms->get(1));
    const QoreTypeInfo* elem_type = QoreTypeInfo::getUniqueReturnComplexList(list_type);
    const QoreIterateOperatorNode* iterate_source =
        dynamic_cast<const QoreIterateOperatorNode*>(ms->get(1).getInternalNode());
    bool use_direct_index = elem_type && !iterate_source;
    bool elem_is_int = false;
    bool elem_is_float = false;
    if (elem_type) {
        if (QoreTypeInfo::parseReturns(elem_type, NT_INT) == QTI_IDENT) {
            elem_is_int = true;
        } else if (QoreTypeInfo::parseReturns(elem_type, NT_FLOAT) == QTI_IDENT) {
            elem_is_float = true;
        }
    }

    // Evaluate the input list (operand 1)
    QoreIRValue input_list = iterate_source
        ? lowerExpression(iterate_source->getExp(), error)
        : lowerExpression(ms->get(1), error);
    if (!input_list.isValid()) {
        return QoreIRValue();
    }

    if (use_direct_index) {
        // Direct-index loop: avoid iterator overhead for typed lists
        // Create blocks AFTER evaluating the input expression
        // No empty check needed: createEmptyList produces an empty list,
        // and the header condition (0 >= 0) immediately exits the loop.
        QoreIRBasicBlock* nothing_block = createBlock("mapselect.direct.nothing");
        QoreIRBasicBlock* preheader_block = createBlock("mapselect.preheader");
        QoreIRBasicBlock* header_block = createBlock("mapselect.header");
        QoreIRBasicBlock* body_block = createBlock("mapselect.body");
        QoreIRBasicBlock* append_block = createBlock("mapselect.append");
        QoreIRBasicBlock* cont_block = createBlock("mapselect.cont");
        QoreIRBasicBlock* exit_block = createBlock("mapselect.exit");
        QoreIRBasicBlock* final_block = createBlock("mapselect.direct.final");
        if (!nothing_block || !preheader_block || !header_block || !body_block
                || !append_block || !cont_block || !exit_block || !final_block) {
            error = "IR builder failed to create blocks for map+select";
            return QoreIRValue();
        }
        header_block->is_loop_header = true;

        QoreIRValue nothing_check_val = builder.createConstNothing(ms->loc)->result;
        QoreIRValue is_nothing = builder.createBinaryOp(QoreIROpcode::EqHard, input_list, nothing_check_val,
            ms->loc)->result;
        builder.createBranchIf(is_nothing, nothing_block, preheader_block, ms->loc);

        // Preheader: create empty result list (filtered, size unknown)
        builder.setBlock(preheader_block);
        QoreIRValue list_size = builder.createListSize(input_list, ms->loc)->result;
        QoreIRValue zero = builder.createConstInt(0, ms->loc)->result;
        QoreIRValue result_list;
        if (need_result) {
            result_list = builder.createEmptyList(ms->loc)->result;
        }
        {
            auto* br = builder.createBranch(header_block, ms->loc);
            setLoopCheckpointExceptionTarget(br, header_block);
        }

        // Header block: check if index < size
        builder.setBlock(header_block);

        auto* index_phi = builder.createPhi({}, ms->loc, QoreIRPhiValueKind::NativeInt);
        QoreIRValue index_val = index_phi->result;

        QoreIRValue at_end = builder.createBinaryOp(QoreIROpcode::GeInt, index_val, list_size,
            ms->loc)->result;
        builder.createBranchIf(at_end, exit_block, body_block, ms->loc);

        // Body block: load element, evaluate predicate
        builder.setBlock(body_block);

        // Load element at current index
        QoreIRValue element_val;
        if (elem_is_int) {
            element_val = builder.createListGetInt(input_list, index_val, ms->loc)->result;
        } else if (elem_is_float) {
            element_val = builder.createListGetFloat(input_list, index_val, ms->loc)->result;
        } else {
            element_val = builder.createListGetValue(input_list, index_val, ms->loc)->result;
        }

        // Set virtual implicit context
        VirtualImplicitContext saved = virtual_implicit;
        virtual_implicit.arg0 = element_val;
        virtual_implicit.arg1 = QoreIRValue();
        virtual_implicit.element = index_val;
        virtual_implicit.active = true;

        // Track AST delegation
        int saved_ast_count = ast_delegate_count;

        // Lower the select predicate (operand 2)
        QoreIRValue predicate_result = lowerExpression(ms->get(2), error);
        if (!predicate_result.isValid()) {
            virtual_implicit = saved;
            return QoreIRValue();
        }

        QoreIRValue predicate_bool = builder.createUnaryOp(QoreIROpcode::ToBool, predicate_result,
            ms->loc)->result;
        builder.createBranchIf(predicate_bool, append_block, cont_block, ms->loc);

        // Append block: evaluate map expression and append to result
        builder.setBlock(append_block);

        if (!need_result) {
            builder.createPushTempMark(ms->loc);
        }
        QoreIRValue map_result = lowerExpression(ms->get(0), error);
        if (!map_result.isValid()) {
            virtual_implicit = saved;
            return QoreIRValue();
        }

        // Restore virtual context
        virtual_implicit = saved;

        bool needs_implicit_push = (ast_delegate_count > saved_ast_count);
        QoreIRValue old_element;
        QoreIRValue old_argv;
        if (needs_implicit_push) {
            QoreIRFunction* func = builder.getFunction();

            auto push_elem = std::make_unique<QoreIRInstruction>(QoreIROpcode::PushImplicitElement);
            push_elem->result = func->createValue();
            push_elem->operands.push_back(index_val);
            push_elem->loc = ms->loc;
            old_element = push_elem->result;

            auto push_argv = std::make_unique<QoreIRInstruction>(QoreIROpcode::PushImplicitArg);
            push_argv->result = func->createValue();
            push_argv->operands.push_back(element_val);
            push_argv->loc = ms->loc;
            old_argv = push_argv->result;

            size_t insert_pos = 1;  // After element load
            body_block->instructions.insert(body_block->instructions.begin() + insert_pos,
                std::move(push_elem));
            body_block->instructions.insert(body_block->instructions.begin() + insert_pos + 1,
                std::move(push_argv));
        }

        if (need_result) {
            builder.createListAppend(result_list, map_result, ms->loc);
        } else {
            builder.createDiscardTemps(ms->loc);
        }
        builder.createBranch(cont_block, ms->loc);

        // Continue block: increment index, loop back
        builder.setBlock(cont_block);

        if (needs_implicit_push) {
            builder.createPopImplicitArg(old_argv, ms->loc);
            builder.createPopImplicitElement(old_element, ms->loc);
        }

        QoreIRValue one = builder.createConstInt(1, ms->loc)->result;
        QoreIRValue next_index = builder.createBinaryOp(QoreIROpcode::AddInt, index_val, one,
            ms->loc)->result;

        QoreIRBasicBlock* cont_exit_block = builder.getBlock();

        {
            auto* br = builder.createBranch(header_block, ms->loc);
            setLoopCheckpointExceptionTarget(br, header_block);
        }

        // Complete PHI nodes
        index_phi->incoming.push_back({zero, preheader_block});
        index_phi->incoming.push_back({next_index, cont_exit_block});
        index_phi->operands.push_back(zero);
        index_phi->operands.push_back(next_index);

        // Exit block: result_list is the result (empty for size 0)
        builder.setBlock(exit_block);
        builder.createBranch(final_block, ms->loc);

        builder.setBlock(nothing_block);
        QoreIRValue nothing_val = builder.createConstNothing(ms->loc)->result;
        builder.createBranch(final_block, ms->loc);

        builder.setBlock(final_block);
        if (!need_result) {
            return builder.createConstNothing(ms->loc)->result;
        }

        std::vector<QoreIRPhiIncoming> result_incoming;
        result_incoming.push_back({result_list, exit_block});
        result_incoming.push_back({nothing_val, nothing_block});
        auto* result_phi = builder.createPhi(result_incoming, ms->loc);
        return result_phi->result;
    }

    // Fallback: iterator-based loop for untyped lists

    // Match AST semantics for scalar sources with unknown static type: a
    // map-select over a scalar returns the mapped scalar when selected, or
    // NOTHING when filtered out. Collections still return lists.
    bool is_known_collection = list_type
        && (QoreTypeInfo::isListType(list_type)
            || QoreTypeInfo::getUniqueReturnClass(list_type) != nullptr);
    bool needs_runtime_unwrap_check = need_result && !is_known_collection;

    QoreIRValue is_collection_val;
    if (needs_runtime_unwrap_check) {
        is_collection_val = builder.createUnaryOp(QoreIROpcode::IsCollectionType,
                input_list, ms->loc)->result;
    }

    // Create iterator from input list
    auto* iter_inst = iterate_source
        ? builder.createIteratorCreateIterate(input_list, ms->loc)
        : builder.createIteratorCreate(input_list, nullptr, ms->loc);
    QoreIRValue iter_val = iter_inst->result;

    // Create loop blocks AFTER expression evaluation
    QoreIRBasicBlock* preheader_block = createBlock("mapselect.preheader");
    QoreIRBasicBlock* header_block = createBlock("mapselect.header");
    QoreIRBasicBlock* body_block = createBlock("mapselect.body");
    QoreIRBasicBlock* append_block = createBlock("mapselect.append");
    QoreIRBasicBlock* cont_block = createBlock("mapselect.cont");
    QoreIRBasicBlock* loop_exit_block = createBlock("mapselect.loop_exit");
    QoreIRBasicBlock* nothing_block = createBlock("mapselect.nothing");
    QoreIRBasicBlock* final_block = createBlock("mapselect.final");
    if (!preheader_block || !header_block || !body_block || !append_block
            || !cont_block || !loop_exit_block || !nothing_block || !final_block) {
        error = "IR builder failed to create blocks for map+select";
        return QoreIRValue();
    }
    header_block->is_loop_header = true;

    QoreIRBasicBlock* unwrap_check_block = nullptr;
    QoreIRBasicBlock* unwrap_get_block = nullptr;
    QoreIRBasicBlock* unwrap_nothing_block = nullptr;
    if (needs_runtime_unwrap_check) {
        unwrap_check_block = createBlock("mapselect.unwrap_check");
        unwrap_get_block = createBlock("mapselect.unwrap_get");
        unwrap_nothing_block = createBlock("mapselect.unwrap_nothing");
        if (!unwrap_check_block || !unwrap_get_block || !unwrap_nothing_block) {
            error = "IR builder failed to create blocks for map+select unwrap";
            return QoreIRValue();
        }
    }

    // Check if iterator is null (input was NOTHING) → return NOTHING
    QoreIRValue zero = builder.createConstInt(0, ms->loc)->result;
    QoreIRValue is_null = builder.createBinaryOp(QoreIROpcode::EqInt, iter_val, zero, ms->loc)->result;
    builder.createBranchIf(is_null, nothing_block, preheader_block, ms->loc);

    // Preheader: create empty result list and proceed to loop
    builder.setBlock(preheader_block);
    QoreIRValue result_list;
    if (need_result) {
        result_list = builder.createEmptyList(ms->loc)->result;
    }
    QoreIRValue init_index = builder.createConstInt(0, ms->loc)->result;
    {
        auto* br = builder.createBranch(header_block, ms->loc);
        setLoopCheckpointExceptionTarget(br, header_block);
    }

    // Header block: create phi for index and check for next value
    builder.setBlock(header_block);

    auto* index_phi = builder.createPhi({}, ms->loc, QoreIRPhiValueKind::NativeInt);
    QoreIRValue index_val = index_phi->result;

    // Get next element from iterator
    auto* next_inst = builder.createIteratorNext(iter_val, loop_exit_block, body_block, ms->loc);
    QoreIRValue element_val = next_inst->result;

    // Body block: push implicit args, evaluate select predicate
    builder.setBlock(body_block);

    // Push implicit args to thread-local stack (needed for AST-delegated sub-expressions)
    QoreIRValue old_element_ms = builder.createPushImplicitElement(index_val, ms->loc)->result;
    QoreIRValue old_argv_ms = builder.createPushImplicitArg(element_val, ms->loc)->result;

    // Set virtual implicit context: $1 = element, $# = index (fast path for IR-lowered refs)
    VirtualImplicitContext saved = virtual_implicit;
    virtual_implicit.arg0 = element_val;
    virtual_implicit.arg1 = QoreIRValue();
    virtual_implicit.element = index_val;
    virtual_implicit.active = true;

    // Lower the select predicate (operand 2)
    QoreIRValue predicate_result = lowerExpression(ms->get(2), error);
    if (!predicate_result.isValid()) {
        virtual_implicit = saved;
        return QoreIRValue();
    }

    QoreIRValue predicate_bool = builder.createUnaryOp(QoreIROpcode::ToBool, predicate_result, ms->loc)->result;
    builder.createBranchIf(predicate_bool, append_block, cont_block, ms->loc);

    // Append block: evaluate map expression and append to result
    builder.setBlock(append_block);

    if (!need_result) {
        builder.createPushTempMark(ms->loc);
    }
    QoreIRValue map_result = lowerExpression(ms->get(0), error);
    if (!map_result.isValid()) {
        virtual_implicit = saved;
        return QoreIRValue();
    }

    if (need_result) {
        builder.createListAppend(result_list, map_result, ms->loc);
    } else {
        builder.createDiscardTemps(ms->loc);
    }
    builder.createBranch(cont_block, ms->loc);

    // Continue block: increment index, loop back
    builder.setBlock(cont_block);

    // Restore virtual context and thread-local stack
    virtual_implicit = saved;
    builder.createPopImplicitArg(old_argv_ms, ms->loc);
    builder.createPopImplicitElement(old_element_ms, ms->loc);

    QoreIRValue one = builder.createConstInt(1, ms->loc)->result;
    QoreIRValue next_index = builder.createBinaryOp(QoreIROpcode::AddInt, index_val, one, ms->loc)->result;

    QoreIRBasicBlock* cont_exit_block = builder.getBlock();

    {
        auto* br = builder.createBranch(header_block, ms->loc);
        setLoopCheckpointExceptionTarget(br, header_block);
    }

    // Complete the phi node
    index_phi->incoming.push_back({init_index, preheader_block});
    index_phi->incoming.push_back({next_index, cont_exit_block});
    index_phi->operands.push_back(init_index);
    index_phi->operands.push_back(next_index);

    // Nothing block: input was NOTHING → return NOTHING
    builder.setBlock(nothing_block);
    QoreIRValue nothing_val = builder.createConstNothing(ms->loc)->result;
    builder.createBranch(final_block, ms->loc);

    builder.setBlock(loop_exit_block);
    if (needs_runtime_unwrap_check) {
        builder.createBranchIf(is_collection_val, final_block, unwrap_check_block, ms->loc);

        builder.setBlock(unwrap_check_block);
        QoreIRValue list_size = builder.createListSize(result_list, ms->loc)->result;
        builder.createBranchIf(list_size, unwrap_get_block, unwrap_nothing_block, ms->loc);

        builder.setBlock(unwrap_get_block);
        QoreIRValue zero_idx = builder.createConstInt(0, ms->loc)->result;
        QoreIRValue unwrapped = builder.createBinaryOp(QoreIROpcode::ListGetValue,
                result_list, zero_idx, ms->loc)->result;
        builder.createBranch(final_block, ms->loc);

        builder.setBlock(unwrap_nothing_block);
        QoreIRValue unwrap_nothing_val = builder.createConstNothing(ms->loc)->result;
        builder.createBranch(final_block, ms->loc);

        builder.setBlock(final_block);
        std::vector<QoreIRPhiIncoming> result_incoming;
        result_incoming.push_back({result_list, loop_exit_block});
        result_incoming.push_back({unwrapped, unwrap_get_block});
        result_incoming.push_back({unwrap_nothing_val, unwrap_nothing_block});
        result_incoming.push_back({nothing_val, nothing_block});
        auto* result_phi = builder.createPhi(result_incoming, ms->loc);

        return result_phi->result;
    }

    builder.createBranch(final_block, ms->loc);

    // Final block: PHI between result_list and NOTHING
    builder.setBlock(final_block);
    if (!need_result) {
        return builder.createConstNothing(ms->loc)->result;
    }

    std::vector<QoreIRPhiIncoming> result_incoming;
    result_incoming.push_back({result_list, loop_exit_block});
    result_incoming.push_back({nothing_val, nothing_block});
    auto* result_phi = builder.createPhi(result_incoming, ms->loc);

    return result_phi->result;
}

QoreIRValue QoreIRLowering::lowerHashMapNative(const QoreHashMapOperatorNode* hm, const QoreValue& expr,
        std::string& error) {
    if (!ensureBuilderContext(error)) {
        return QoreIRValue();
    }

    // e[0] = key expression, e[1] = value expression, e[2] = iterator/input

    // Check if the input list has a known element type for direct-index optimization
    const QoreTypeInfo* list_type = getExprTypeInfo(hm->get(2));
    const QoreTypeInfo* elem_type = QoreTypeInfo::getUniqueReturnComplexList(list_type);
    bool use_direct_index = (elem_type != nullptr);

    // Get or infer hash map result type
    const QoreTypeInfo* hash_result_type = hm->getTypeInfo();
    if (!hash_result_type) {
        // If returnTypeInfo wasn't set during parsing, try to infer from value expression
        const QoreTypeInfo* value_type = getExprTypeInfo(hm->get(1));
        if (value_type && QoreTypeInfo::hasType(value_type)) {
            hash_result_type = qore_get_complex_hash_type(value_type);
        }
    }

    // Evaluate the input (operand 2)
    QoreIRValue input_list = lowerExpression(hm->get(2), error);
    if (!input_list.isValid()) {
        return QoreIRValue();
    }

    if (use_direct_index) {
        // Direct-index loop: avoid iterator overhead for typed lists
        // Create blocks AFTER evaluating the input expression
        // No empty check needed: createMakeHash produces an empty hash,
        // and the header condition (0 >= 0) immediately exits the loop.
        QoreIRBasicBlock* nothing_block = createBlock("hashmap.direct.nothing");
        QoreIRBasicBlock* preheader_block = createBlock("hashmap.preheader");
        QoreIRBasicBlock* header_block = createBlock("hashmap.header");
        QoreIRBasicBlock* body_block = createBlock("hashmap.body");
        QoreIRBasicBlock* exit_block = createBlock("hashmap.exit");
        QoreIRBasicBlock* final_block = createBlock("hashmap.direct.final");
        if (!nothing_block || !preheader_block || !header_block || !body_block || !exit_block || !final_block) {
            error = "IR builder failed to create blocks for hash map";
            return QoreIRValue();
        }
        header_block->is_loop_header = true;

        QoreIRValue nothing_check_val = builder.createConstNothing(hm->loc)->result;
        QoreIRValue is_nothing = builder.createBinaryOp(QoreIROpcode::EqHard, input_list, nothing_check_val,
            hm->loc)->result;
        builder.createBranchIf(is_nothing, nothing_block, preheader_block, hm->loc);

        // Preheader: create empty result hash and proceed to loop
        builder.setBlock(preheader_block);
        QoreIRValue list_size = builder.createListSize(input_list, hm->loc)->result;
        QoreIRValue zero = builder.createConstInt(0, hm->loc)->result;
        QoreIRValue result_hash = builder.createMakeHash({}, hm->loc, hash_result_type)->result;
        {
            auto* br = builder.createBranch(header_block, hm->loc);
            setLoopCheckpointExceptionTarget(br, header_block);
        }

        // Header block: check if index < size
        builder.setBlock(header_block);

        auto* index_phi = builder.createPhi({}, hm->loc, QoreIRPhiValueKind::NativeInt);
        QoreIRValue index_val = index_phi->result;

        QoreIRValue at_end = builder.createBinaryOp(QoreIROpcode::GeInt, index_val, list_size,
            hm->loc)->result;
        builder.createBranchIf(at_end, exit_block, body_block, hm->loc);

        // Body block: load element, evaluate key and value, insert into hash
        builder.setBlock(body_block);

        // Load element at current index
        QoreIRValue element_val = builder.createListGetValue(input_list, index_val, hm->loc)->result;

        // Set virtual implicit context: $1 = element, $# = index
        VirtualImplicitContext saved = virtual_implicit;
        virtual_implicit.arg0 = element_val;
        virtual_implicit.arg1 = QoreIRValue();
        virtual_implicit.element = index_val;
        virtual_implicit.active = true;

        // Lower key and value expressions — track AST delegation
        int saved_ast_count = ast_delegate_count;

        QoreIRValue key_result = lowerExpression(hm->get(0), error);
        if (!key_result.isValid()) {
            virtual_implicit = saved;
            return QoreIRValue();
        }

        QoreIRValue value_result = lowerExpression(hm->get(1), error);
        if (!value_result.isValid()) {
            virtual_implicit = saved;
            return QoreIRValue();
        }

        // Restore virtual context
        virtual_implicit = saved;

        bool needs_implicit_push = (ast_delegate_count > saved_ast_count);
        if (needs_implicit_push) {
            // Body contains AST-delegated calls: insert push/pop implicit args
            QoreIRFunction* func = builder.getFunction();

            auto push_elem = std::make_unique<QoreIRInstruction>(QoreIROpcode::PushImplicitElement);
            push_elem->result = func->createValue();
            push_elem->operands.push_back(index_val);
            push_elem->loc = hm->loc;
            QoreIRValue old_element = push_elem->result;

            auto push_argv = std::make_unique<QoreIRInstruction>(QoreIROpcode::PushImplicitArg);
            push_argv->result = func->createValue();
            push_argv->operands.push_back(element_val);
            push_argv->loc = hm->loc;
            QoreIRValue old_argv = push_argv->result;

            // Insert after element load instruction in body_block
            size_t insert_pos = 1;
            body_block->instructions.insert(body_block->instructions.begin() + insert_pos,
                std::move(push_elem));
            body_block->instructions.insert(body_block->instructions.begin() + insert_pos + 1,
                std::move(push_argv));

            // Pop after body expression (in current block which may be invoke.cont)
            builder.createPopImplicitArg(old_argv, hm->loc);
            builder.createPopImplicitElement(old_element, hm->loc);
        }

        // Set key-value in hash
        builder.createHashSetKeyValue(result_hash, key_result, value_result, hm->loc);

        // Increment index
        QoreIRValue one = builder.createConstInt(1, hm->loc)->result;
        QoreIRValue next_index = builder.createBinaryOp(QoreIROpcode::AddInt, index_val, one, hm->loc)->result;

        QoreIRBasicBlock* body_exit_block = builder.getBlock();

        // Branch back to header
        {
            auto* br = builder.createBranch(header_block, hm->loc);
            setLoopCheckpointExceptionTarget(br, header_block);
        }

        // Complete PHI nodes
        index_phi->incoming.push_back({zero, preheader_block});
        index_phi->incoming.push_back({next_index, body_exit_block});
        index_phi->operands.push_back(zero);
        index_phi->operands.push_back(next_index);

        // Exit block: result_hash is the result (empty for size 0)
        builder.setBlock(exit_block);
        builder.createBranch(final_block, hm->loc);

        builder.setBlock(nothing_block);
        QoreIRValue nothing_val = builder.createConstNothing(hm->loc)->result;
        builder.createBranch(final_block, hm->loc);

        builder.setBlock(final_block);
        std::vector<QoreIRPhiIncoming> result_incoming;
        result_incoming.push_back({result_hash, exit_block});
        result_incoming.push_back({nothing_val, nothing_block});
        auto* result_phi = builder.createPhi(result_incoming, hm->loc);

        return result_phi->result;
    }

    // Fallback: iterator-based loop for untyped lists

    // Create iterator from input
    auto* iter_inst = builder.createIteratorCreate(input_list, nullptr, hm->loc);
    QoreIRValue iter_val = iter_inst->result;

    // Create loop blocks AFTER expression evaluation
    QoreIRBasicBlock* preheader_block = createBlock("hashmap.preheader");
    QoreIRBasicBlock* header_block = createBlock("hashmap.header");
    QoreIRBasicBlock* body_block = createBlock("hashmap.body");
    QoreIRBasicBlock* exit_block = createBlock("hashmap.exit");
    QoreIRBasicBlock* nothing_block = createBlock("hashmap.nothing");
    if (!preheader_block || !header_block || !body_block || !exit_block || !nothing_block) {
        error = "IR builder failed to create blocks for hash map";
        return QoreIRValue();
    }
    header_block->is_loop_header = true;

    // Check if iterator is null (input was NOTHING) — return NOTHING in that case
    QoreIRValue zero = builder.createConstInt(0, hm->loc)->result;
    QoreIRValue is_null = builder.createBinaryOp(QoreIROpcode::EqInt, iter_val, zero, hm->loc)->result;
    builder.createBranchIf(is_null, nothing_block, preheader_block, hm->loc);

    // Preheader: create empty result hash and proceed to loop
    builder.setBlock(preheader_block);
    QoreIRValue result_hash = builder.createMakeHash({}, hm->loc, hash_result_type)->result;
    QoreIRValue init_index = builder.createConstInt(0, hm->loc)->result;
    {
        auto* br = builder.createBranch(header_block, hm->loc);
        setLoopCheckpointExceptionTarget(br, header_block);
    }

    // Header block
    builder.setBlock(header_block);

    auto* index_phi = builder.createPhi({}, hm->loc, QoreIRPhiValueKind::NativeInt);
    QoreIRValue index_val = index_phi->result;

    // Get next element from iterator
    auto* next_inst = builder.createIteratorNext(iter_val, exit_block, body_block, hm->loc);
    QoreIRValue element_val = next_inst->result;

    // Body block: evaluate key and value expressions
    builder.setBlock(body_block);

    // Set virtual implicit context: $1 = element, $# = index (fast path for IR-lowered refs)
    VirtualImplicitContext saved = virtual_implicit;
    virtual_implicit.arg0 = element_val;
    virtual_implicit.arg1 = QoreIRValue();
    virtual_implicit.element = index_val;
    virtual_implicit.active = true;

    // Lower key and value expressions — track AST delegation
    int saved_ast_count = ast_delegate_count;

    QoreIRValue key_result = lowerExpression(hm->get(0), error);
    if (!key_result.isValid()) {
        virtual_implicit = saved;
        return QoreIRValue();
    }

    QoreIRValue value_result = lowerExpression(hm->get(1), error);
    if (!value_result.isValid()) {
        virtual_implicit = saved;
        return QoreIRValue();
    }

    // Restore virtual context
    virtual_implicit = saved;

    bool needs_implicit_push = (ast_delegate_count > saved_ast_count);
    if (needs_implicit_push) {
        // Body contains AST-delegated calls: insert push/pop
        QoreIRFunction* func = builder.getFunction();

        auto push_elem = std::make_unique<QoreIRInstruction>(QoreIROpcode::PushImplicitElement);
        push_elem->result = func->createValue();
        push_elem->operands.push_back(index_val);
        push_elem->loc = hm->loc;
        QoreIRValue old_element = push_elem->result;

        auto push_argv = std::make_unique<QoreIRInstruction>(QoreIROpcode::PushImplicitArg);
        push_argv->result = func->createValue();
        push_argv->operands.push_back(element_val);
        push_argv->loc = hm->loc;
        QoreIRValue old_argv = push_argv->result;

        // Insert after start of body_block (which follows IteratorNext)
        size_t insert_pos = 0;
        body_block->instructions.insert(body_block->instructions.begin() + insert_pos,
            std::move(push_elem));
        body_block->instructions.insert(body_block->instructions.begin() + insert_pos + 1,
            std::move(push_argv));

        builder.createPopImplicitArg(old_argv, hm->loc);
        builder.createPopImplicitElement(old_element, hm->loc);
    }

    // Set key-value in hash
    builder.createHashSetKeyValue(result_hash, key_result, value_result, hm->loc);

    // Increment index
    QoreIRValue one = builder.createConstInt(1, hm->loc)->result;
    QoreIRValue next_index = builder.createBinaryOp(QoreIROpcode::AddInt, index_val, one, hm->loc)->result;

    QoreIRBasicBlock* body_exit_block = builder.getBlock();

    // Branch back to header
    {
        auto* br = builder.createBranch(header_block, hm->loc);
        setLoopCheckpointExceptionTarget(br, header_block);
    }

    // Complete the phi node
    index_phi->incoming.push_back({init_index, preheader_block});
    index_phi->incoming.push_back({next_index, body_exit_block});
    index_phi->operands.push_back(init_index);
    index_phi->operands.push_back(next_index);

    // Nothing block: input was NOTHING → return NOTHING
    builder.setBlock(nothing_block);
    QoreIRValue nothing_val = builder.createConstNothing(hm->loc)->result;
    builder.createBranch(exit_block, hm->loc);

    // Exit block: PHI between result_hash (from loop) and NOTHING (from null check)
    builder.setBlock(exit_block);

    std::vector<QoreIRPhiIncoming> result_incoming;
    result_incoming.push_back({result_hash, header_block});    // Normal loop exit
    result_incoming.push_back({nothing_val, nothing_block});   // NOTHING input
    auto* result_phi = builder.createPhi(result_incoming, hm->loc);

    return result_phi->result;
}

QoreIRValue QoreIRLowering::lowerHashMapSelectNative(const QoreHashMapSelectOperatorNode* hms,
        const QoreValue& expr, std::string& error) {
    if (!ensureBuilderContext(error)) {
        return QoreIRValue();
    }

    // e[0] = key expression, e[1] = value expression, e[2] = iterator/input, e[3] = select predicate

    // Check if the input list has a known element type for direct-index optimization
    const QoreTypeInfo* list_type = getExprTypeInfo(hms->get(2));
    const QoreTypeInfo* elem_type = QoreTypeInfo::getUniqueReturnComplexList(list_type);
    bool use_direct_index = (elem_type != nullptr);

    // Get or infer hash map+select result type
    const QoreTypeInfo* hash_result_type = hms->getTypeInfo();
    if (!hash_result_type) {
        // If returnTypeInfo wasn't set during parsing, try to infer from value expression
        const QoreTypeInfo* value_type = getExprTypeInfo(hms->get(1));
        if (value_type && QoreTypeInfo::hasType(value_type)) {
            hash_result_type = qore_get_complex_hash_type(value_type);
        }
    }

    // Evaluate the input (operand 2)
    QoreIRValue input_list = lowerExpression(hms->get(2), error);
    if (!input_list.isValid()) {
        return QoreIRValue();
    }

    if (use_direct_index) {
        // Direct-index loop: avoid iterator overhead for typed lists
        // Create blocks AFTER evaluating the input expression
        // No empty check needed: createMakeHash produces an empty hash,
        // and the header condition (0 >= 0) immediately exits the loop.
        QoreIRBasicBlock* nothing_block = createBlock("hashmapselect.direct.nothing");
        QoreIRBasicBlock* preheader_block = createBlock("hashmapselect.preheader");
        QoreIRBasicBlock* header_block = createBlock("hashmapselect.header");
        QoreIRBasicBlock* body_block = createBlock("hashmapselect.body");
        QoreIRBasicBlock* insert_block = createBlock("hashmapselect.insert");
        QoreIRBasicBlock* cont_block = createBlock("hashmapselect.cont");
        QoreIRBasicBlock* exit_block = createBlock("hashmapselect.exit");
        QoreIRBasicBlock* final_block = createBlock("hashmapselect.direct.final");
        if (!nothing_block || !preheader_block || !header_block || !body_block
                || !insert_block || !cont_block || !exit_block || !final_block) {
            error = "IR builder failed to create blocks for hash map+select";
            return QoreIRValue();
        }
        header_block->is_loop_header = true;

        QoreIRValue nothing_check_val = builder.createConstNothing(hms->loc)->result;
        QoreIRValue is_nothing = builder.createBinaryOp(QoreIROpcode::EqHard, input_list, nothing_check_val,
            hms->loc)->result;
        builder.createBranchIf(is_nothing, nothing_block, preheader_block, hms->loc);

        // Preheader: create empty result hash and proceed to loop
        builder.setBlock(preheader_block);
        QoreIRValue list_size = builder.createListSize(input_list, hms->loc)->result;
        QoreIRValue zero = builder.createConstInt(0, hms->loc)->result;
        QoreIRValue result_hash = builder.createMakeHash({}, hms->loc, hash_result_type)->result;
        {
            auto* br = builder.createBranch(header_block, hms->loc);
            setLoopCheckpointExceptionTarget(br, header_block);
        }

        // Header block: check if index < size
        builder.setBlock(header_block);

        auto* index_phi = builder.createPhi({}, hms->loc, QoreIRPhiValueKind::NativeInt);
        QoreIRValue index_val = index_phi->result;

        QoreIRValue at_end = builder.createBinaryOp(QoreIROpcode::GeInt, index_val, list_size,
            hms->loc)->result;
        builder.createBranchIf(at_end, exit_block, body_block, hms->loc);

        // Body block: load element, evaluate predicate
        builder.setBlock(body_block);

        // Load element at current index
        QoreIRValue element_val = builder.createListGetValue(input_list, index_val, hms->loc)->result;

        // Set virtual implicit context: $1 = element, $# = index
        VirtualImplicitContext saved = virtual_implicit;
        virtual_implicit.arg0 = element_val;
        virtual_implicit.arg1 = QoreIRValue();
        virtual_implicit.element = index_val;
        virtual_implicit.active = true;

        // Lower all body expressions — track AST delegation
        int saved_ast_count = ast_delegate_count;

        // Lower the select predicate (operand 3)
        QoreIRValue predicate_result = lowerExpression(hms->get(3), error);
        if (!predicate_result.isValid()) {
            virtual_implicit = saved;
            return QoreIRValue();
        }

        // Convert predicate to bool
        QoreIRValue predicate_bool = builder.createUnaryOp(QoreIROpcode::ToBool, predicate_result,
            hms->loc)->result;

        // Branch based on predicate
        builder.createBranchIf(predicate_bool, insert_block, cont_block, hms->loc);

        // Insert block: evaluate key and value, insert into hash
        builder.setBlock(insert_block);

        QoreIRValue key_result = lowerExpression(hms->get(0), error);
        if (!key_result.isValid()) {
            virtual_implicit = saved;
            return QoreIRValue();
        }

        QoreIRValue value_result = lowerExpression(hms->get(1), error);
        if (!value_result.isValid()) {
            virtual_implicit = saved;
            return QoreIRValue();
        }

        // Set key-value in hash
        builder.createHashSetKeyValue(result_hash, key_result, value_result, hms->loc);
        builder.createBranch(cont_block, hms->loc);

        // Continue block: restore virtual context
        builder.setBlock(cont_block);
        virtual_implicit = saved;

        bool needs_implicit_push = (ast_delegate_count > saved_ast_count);
        if (needs_implicit_push) {
            // Body contains AST-delegated calls: insert push/pop implicit args
            QoreIRFunction* func = builder.getFunction();

            auto push_elem = std::make_unique<QoreIRInstruction>(QoreIROpcode::PushImplicitElement);
            push_elem->result = func->createValue();
            push_elem->operands.push_back(index_val);
            push_elem->loc = hms->loc;
            QoreIRValue old_element = push_elem->result;

            auto push_argv = std::make_unique<QoreIRInstruction>(QoreIROpcode::PushImplicitArg);
            push_argv->result = func->createValue();
            push_argv->operands.push_back(element_val);
            push_argv->loc = hms->loc;
            QoreIRValue old_argv = push_argv->result;

            // Insert after element load instruction in body_block
            size_t insert_pos = 1;
            body_block->instructions.insert(body_block->instructions.begin() + insert_pos,
                std::move(push_elem));
            body_block->instructions.insert(body_block->instructions.begin() + insert_pos + 1,
                std::move(push_argv));

            // Pop in cont block (reached from both insert and skip paths)
            builder.createPopImplicitArg(old_argv, hms->loc);
            builder.createPopImplicitElement(old_element, hms->loc);
        }

        // Increment index
        QoreIRValue one = builder.createConstInt(1, hms->loc)->result;
        QoreIRValue next_index = builder.createBinaryOp(QoreIROpcode::AddInt, index_val, one,
            hms->loc)->result;

        QoreIRBasicBlock* cont_exit_block = builder.getBlock();

        // Branch back to header
        {
            auto* br = builder.createBranch(header_block, hms->loc);
            setLoopCheckpointExceptionTarget(br, header_block);
        }

        // Complete PHI nodes
        index_phi->incoming.push_back({zero, preheader_block});
        index_phi->incoming.push_back({next_index, cont_exit_block});
        index_phi->operands.push_back(zero);
        index_phi->operands.push_back(next_index);

        // Exit block: result_hash is the result (empty for size 0)
        builder.setBlock(exit_block);
        builder.createBranch(final_block, hms->loc);

        builder.setBlock(nothing_block);
        QoreIRValue nothing_val = builder.createConstNothing(hms->loc)->result;
        builder.createBranch(final_block, hms->loc);

        builder.setBlock(final_block);
        std::vector<QoreIRPhiIncoming> result_incoming;
        result_incoming.push_back({result_hash, exit_block});
        result_incoming.push_back({nothing_val, nothing_block});
        auto* result_phi = builder.createPhi(result_incoming, hms->loc);

        return result_phi->result;
    }

    // Fallback: iterator-based loop for untyped lists

    // Create iterator from input
    auto* iter_inst = builder.createIteratorCreate(input_list, nullptr, hms->loc);
    QoreIRValue iter_val = iter_inst->result;

    // Create loop blocks AFTER expression evaluation
    QoreIRBasicBlock* preheader_block = createBlock("hashmapselect.preheader");
    QoreIRBasicBlock* header_block = createBlock("hashmapselect.header");
    QoreIRBasicBlock* body_block = createBlock("hashmapselect.body");
    QoreIRBasicBlock* insert_block = createBlock("hashmapselect.insert");
    QoreIRBasicBlock* cont_block = createBlock("hashmapselect.cont");
    QoreIRBasicBlock* exit_block = createBlock("hashmapselect.exit");
    QoreIRBasicBlock* nothing_block = createBlock("hashmapselect.nothing");
    if (!preheader_block || !header_block || !body_block || !insert_block || !cont_block || !exit_block
            || !nothing_block) {
        error = "IR builder failed to create blocks for hash map+select";
        return QoreIRValue();
    }
    header_block->is_loop_header = true;

    // Check if iterator is null (input was NOTHING) — return NOTHING in that case
    QoreIRValue zero = builder.createConstInt(0, hms->loc)->result;
    QoreIRValue is_null = builder.createBinaryOp(QoreIROpcode::EqInt, iter_val, zero, hms->loc)->result;
    builder.createBranchIf(is_null, nothing_block, preheader_block, hms->loc);

    // Preheader: create empty result hash and proceed to loop
    builder.setBlock(preheader_block);
    QoreIRValue result_hash = builder.createMakeHash({}, hms->loc, hash_result_type)->result;
    QoreIRValue init_index = builder.createConstInt(0, hms->loc)->result;
    {
        auto* br = builder.createBranch(header_block, hms->loc);
        setLoopCheckpointExceptionTarget(br, header_block);
    }

    // Header block
    builder.setBlock(header_block);

    auto* index_phi = builder.createPhi({}, hms->loc, QoreIRPhiValueKind::NativeInt);
    QoreIRValue index_val = index_phi->result;

    // Get next element from iterator
    auto* next_inst = builder.createIteratorNext(iter_val, exit_block, body_block, hms->loc);
    QoreIRValue element_val = next_inst->result;

    // Body block: evaluate select predicate
    builder.setBlock(body_block);

    // Set virtual implicit context: $1 = element, $# = index
    VirtualImplicitContext saved = virtual_implicit;
    virtual_implicit.arg0 = element_val;
    virtual_implicit.arg1 = QoreIRValue();
    virtual_implicit.element = index_val;
    virtual_implicit.active = true;

    // Lower all body expressions — track AST delegation
    int saved_ast_count = ast_delegate_count;

    // Lower the select predicate (operand 3)
    QoreIRValue predicate_result = lowerExpression(hms->get(3), error);
    if (!predicate_result.isValid()) {
        virtual_implicit = saved;
        return QoreIRValue();
    }

    // Convert predicate to bool
    QoreIRValue predicate_bool = builder.createUnaryOp(QoreIROpcode::ToBool, predicate_result, hms->loc)->result;

    // Branch based on predicate
    builder.createBranchIf(predicate_bool, insert_block, cont_block, hms->loc);

    // Insert block: evaluate key and value, insert into hash
    builder.setBlock(insert_block);

    QoreIRValue key_result = lowerExpression(hms->get(0), error);
    if (!key_result.isValid()) {
        virtual_implicit = saved;
        return QoreIRValue();
    }

    QoreIRValue value_result = lowerExpression(hms->get(1), error);
    if (!value_result.isValid()) {
        virtual_implicit = saved;
        return QoreIRValue();
    }

    // Set key-value in hash
    builder.createHashSetKeyValue(result_hash, key_result, value_result, hms->loc);
    builder.createBranch(cont_block, hms->loc);

    // Continue block: restore virtual context
    builder.setBlock(cont_block);
    virtual_implicit = saved;

    bool needs_implicit_push = (ast_delegate_count > saved_ast_count);
    if (needs_implicit_push) {
        // Body contains AST-delegated calls: insert push/pop
        QoreIRFunction* func = builder.getFunction();

        auto push_elem = std::make_unique<QoreIRInstruction>(QoreIROpcode::PushImplicitElement);
        push_elem->result = func->createValue();
        push_elem->operands.push_back(index_val);
        push_elem->loc = hms->loc;
        QoreIRValue old_element = push_elem->result;

        auto push_argv = std::make_unique<QoreIRInstruction>(QoreIROpcode::PushImplicitArg);
        push_argv->result = func->createValue();
        push_argv->operands.push_back(element_val);
        push_argv->loc = hms->loc;
        QoreIRValue old_argv = push_argv->result;

        size_t insert_pos = 0;
        body_block->instructions.insert(body_block->instructions.begin() + insert_pos,
            std::move(push_elem));
        body_block->instructions.insert(body_block->instructions.begin() + insert_pos + 1,
            std::move(push_argv));

        builder.createPopImplicitArg(old_argv, hms->loc);
        builder.createPopImplicitElement(old_element, hms->loc);
    }

    // Increment index
    QoreIRValue one = builder.createConstInt(1, hms->loc)->result;
    QoreIRValue next_index = builder.createBinaryOp(QoreIROpcode::AddInt, index_val, one, hms->loc)->result;

    QoreIRBasicBlock* cont_exit_block = builder.getBlock();

    // Branch back to header
    {
        auto* br = builder.createBranch(header_block, hms->loc);
        setLoopCheckpointExceptionTarget(br, header_block);
    }

    // Complete the phi node
    index_phi->incoming.push_back({init_index, preheader_block});
    index_phi->incoming.push_back({next_index, cont_exit_block});
    index_phi->operands.push_back(init_index);
    index_phi->operands.push_back(next_index);

    // Nothing block: input was NOTHING → return NOTHING
    builder.setBlock(nothing_block);
    QoreIRValue nothing_val = builder.createConstNothing(hms->loc)->result;
    builder.createBranch(exit_block, hms->loc);

    // Exit block: PHI between result_hash (from loop) and NOTHING (from null check)
    builder.setBlock(exit_block);

    std::vector<QoreIRPhiIncoming> result_incoming;
    result_incoming.push_back({result_hash, header_block});    // Normal loop exit
    result_incoming.push_back({nothing_val, nothing_block});   // NOTHING input
    auto* result_phi = builder.createPhi(result_incoming, hms->loc);

    return result_phi->result;
}

QoreIRValue QoreIRLowering::lowerIterate(const QoreValue& expr, std::string& error) {
    auto* op = dynamic_cast<const QoreIterateOperatorNode*>(expr.getInternalNode());
    if (!op) {
        return QoreIRValue();
    }
    if (!ensureBuilderContext(error)) {
        return QoreIRValue();
    }

    QoreIRValue source = lowerExpression(op->getExp(), error);
    if (!source.isValid()) {
        return QoreIRValue();
    }
    return lowerExprOpOrInvoke(QoreIROpcode::IterateValue, expr, {source}, op->loc, error);
}

QoreIRValue QoreIRLowering::lowerStreaming(const QoreValue& expr, std::string& error) {
    auto* op = dynamic_cast<const QoreStreamingOperatorNode*>(expr.getInternalNode());
    if (!op) {
        return QoreIRValue();
    }
    return lowerStreamingNative(op, expr, error);
}

namespace {
bool qore_streaming_kind_is_terminal(QoreStreamingOperatorNode::Kind kind) {
    return kind == QoreStreamingOperatorNode::First || kind == QoreStreamingOperatorNode::Any
        || kind == QoreStreamingOperatorNode::All || kind == QoreStreamingOperatorNode::Count;
}

}

QoreIRValue QoreIRLowering::lowerLazyPipelineFused(const QoreValue& base_source,
        const std::vector<LazyPipelineStage>& source_stages, const LazyPipelineRoot& root, std::string& error) {
    if (!ensureBuilderContext(error)) {
        return QoreIRValue();
    }

    const QoreProgramLocation* loc = root.loc;
    const QoreStreamingOperatorNode* op = root.streaming;
    bool root_streaming = root.kind == LazyPipelineRoot::Streaming;
    bool root_terminal = root_streaming && qore_streaming_kind_is_terminal(op->getKind());
    bool root_list = root.kind == LazyPipelineRoot::List || (root_streaming && !root_terminal);
    bool need_result = root.need_result;
    bool build_result_list = root_list && need_result;
    bool root_foldl = root.kind == LazyPipelineRoot::Foldl;

    std::vector<LazyPipelineStage> stages = source_stages;
    if (root_streaming && !root_terminal) {
        LazyPipelineStage::Kind kind;
        switch (op->getKind()) {
            case QoreStreamingOperatorNode::Take:
                kind = LazyPipelineStage::StreamTake;
                break;
            case QoreStreamingOperatorNode::Drop:
                kind = LazyPipelineStage::StreamDrop;
                break;
            case QoreStreamingOperatorNode::TakeWhile:
                kind = LazyPipelineStage::StreamTakeWhile;
                break;
            case QoreStreamingOperatorNode::TakeUntil:
                kind = LazyPipelineStage::StreamTakeUntil;
                break;
            case QoreStreamingOperatorNode::First:
            case QoreStreamingOperatorNode::Any:
            case QoreStreamingOperatorNode::All:
            case QoreStreamingOperatorNode::Count:
                assert(false);
                return QoreIRValue();
        }
        stages.push_back({kind, &op->getPredicate(), nullptr, op->loc});
    }

    QoreIRValue source = lowerExpression(base_source, error);
    if (!source.isValid()) {
        return QoreIRValue();
    }

    bool source_uses_iterate = !stages.empty()
        && (stages.front().kind == LazyPipelineStage::StreamTake
            || stages.front().kind == LazyPipelineStage::StreamDrop
            || stages.front().kind == LazyPipelineStage::StreamTakeWhile
            || stages.front().kind == LazyPipelineStage::StreamTakeUntil);
    const QoreTypeInfo* base_source_type = getExprTypeInfo(base_source);
    bool base_source_known_collection = base_source_type
        && (QoreTypeInfo::isListType(base_source_type)
            || QoreTypeInfo::getUniqueReturnClass(base_source_type) != nullptr);
    bool needs_runtime_unwrap_check = root.kind == LazyPipelineRoot::List
        && need_result
        && !source_uses_iterate
        && !base_source_known_collection;
    QoreIRValue is_collection_val;
    if (needs_runtime_unwrap_check) {
        is_collection_val = builder.createUnaryOp(QoreIROpcode::IsCollectionType, source, loc)->result;
    }
    auto* iter_inst = source_uses_iterate
        ? builder.createIteratorCreateIterate(source, loc)
        : builder.createIteratorCreate(source, nullptr, loc);
    QoreIRValue iter_val = iter_inst->result;

    auto lower_with_implicit = [&](const QoreValue& stage_expr, const QoreProgramLocation* stage_loc,
            QoreIRValue element_val, QoreIRValue index_val) -> QoreIRValue {
        QoreIRValue old_element = builder.createPushImplicitElement(index_val, stage_loc)->result;
        QoreIRValue old_argv = builder.createPushImplicitArg(element_val, stage_loc)->result;

        VirtualImplicitContext saved = virtual_implicit;
        virtual_implicit.arg0 = element_val;
        virtual_implicit.arg1 = QoreIRValue();
        virtual_implicit.element = index_val;
        virtual_implicit.active = true;

        QoreIRValue result = lowerExpression(stage_expr, error);

        virtual_implicit = saved;
        builder.createPopImplicitArg(old_argv, stage_loc);
        builder.createPopImplicitElement(old_element, stage_loc);

        return result;
    };

    auto lower_predicate = [&](const QoreValue* pred, const QoreProgramLocation* pred_loc, QoreIRValue element_val,
            QoreIRValue index_val) -> QoreIRValue {
        if (!pred || !static_cast<bool>(*pred)) {
            return builder.createConstBool(true, pred_loc)->result;
        }

        QoreIRValue pred_result = lower_with_implicit(*pred, pred_loc, element_val, index_val);
        if (!pred_result.isValid()) {
            return QoreIRValue();
        }
        return builder.createUnaryOp(QoreIROpcode::ToBool, pred_result, pred_loc)->result;
    };

    auto lower_fold_expr = [&](QoreIRValue accum_val, QoreIRValue element_val) -> QoreIRValue {
        assert(root.fold_expr);

        QoreIRValue argv_list = builder.createEmptyList(loc)->result;
        builder.createListAppend(argv_list, accum_val, loc);
        builder.createListAppend(argv_list, element_val, loc);
        QoreIRValue old_argv = builder.createSetImplicitArgv(argv_list, loc)->result;

        VirtualImplicitContext saved = virtual_implicit;
        virtual_implicit.arg0 = accum_val;
        virtual_implicit.arg1 = element_val;
        virtual_implicit.element = QoreIRValue();
        virtual_implicit.active = true;

        QoreIRValue fold_result = lowerExpression(*root.fold_expr, error);

        virtual_implicit = saved;
        builder.createPopImplicitArg(old_argv, loc);

        return fold_result;
    };

    auto emit_negative_limit_throw = [&](const char* name, const QoreProgramLocation* stage_loc) {
        QoreIRValue err = builder.createConstString("STREAMING-OPERATOR-ERROR", stage_loc)->result;
        QoreIRValue msg = builder.createConstString(
            std::string(name) + " requires a non-negative count", stage_loc)->result;
        QoreIRValue throw_list = builder.createMakeList({err, msg}, stage_loc)->result;
        QoreIRBasicBlock* handler = exception_stack.empty() ? nullptr : exception_stack.back();
        builder.createThrow(throw_list, handler, stage_loc);
    };

    std::vector<QoreIRValue> limits(stages.size());
    for (size_t i = 0; i < stages.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "fused streaming limit lowering")) {
            error = "fused streaming limit lowering cancelled";
            return QoreIRValue();
        }
        if (stages[i].kind != LazyPipelineStage::StreamTake && stages[i].kind != LazyPipelineStage::StreamDrop) {
            continue;
        }

        assert(stages[i].primary);
        limits[i] = lowerExpression(*stages[i].primary, error);
        if (!limits[i].isValid()) {
            return QoreIRValue();
        }

        QoreIRBasicBlock* limit_err_block = createBlock("stream.fused.limit.error");
        QoreIRBasicBlock* limit_ok_block = createBlock("stream.fused.limit.ok");
        if (!limit_err_block || !limit_ok_block) {
            error = "IR builder failed to create blocks for fused streaming limit check";
            return QoreIRValue();
        }

        QoreIRValue zero = builder.createConstInt(0, stages[i].loc)->result;
        QoreIRValue is_negative = builder.createBinaryOp(QoreIROpcode::LtInt, limits[i], zero,
            stages[i].loc)->result;
        builder.createBranchIf(is_negative, limit_err_block, limit_ok_block, stages[i].loc);

        builder.setBlock(limit_err_block);
        emit_negative_limit_throw(stages[i].kind == LazyPipelineStage::StreamTake ? "take" : "drop", stages[i].loc);

        builder.setBlock(limit_ok_block);
    }

    QoreIRBasicBlock* null_block = createBlock("stream.fused.null");
    QoreIRBasicBlock* preheader_block = createBlock("stream.fused.preheader");
    QoreIRBasicBlock* header_block = createBlock("stream.fused.header");
    QoreIRBasicBlock* next_block = createBlock("stream.fused.next");
    QoreIRBasicBlock* body_block = createBlock("stream.fused.body");
    QoreIRBasicBlock* exit_block = createBlock("stream.fused.exit");
    QoreIRBasicBlock* final_block = createBlock("stream.fused.final");
    if (!null_block || !preheader_block || !header_block || !next_block || !body_block || !exit_block
            || !final_block) {
        error = "IR builder failed to create blocks for fused streaming operator";
        return QoreIRValue();
    }
    header_block->is_loop_header = true;

    QoreIRValue zero = builder.createConstInt(0, loc)->result;
    QoreIRValue is_null = builder.createBinaryOp(QoreIROpcode::EqInt, iter_val, zero, loc)->result;
    builder.createBranchIf(is_null, null_block, preheader_block, loc);

    QoreIRValue fold_initial_accum;
    QoreIRValue fold_initial_has_accum;

    builder.setBlock(preheader_block);
    QoreIRValue result_list;
    if (build_result_list) {
        const QoreTypeInfo* elem_type = root.list_element_type;
        if (!elem_type && root_streaming) {
            const QoreTypeInfo* source_type = getExprTypeInfo(op->getSource());
            elem_type = QoreTypeInfo::getUniqueReturnComplexList(source_type);
            if (!elem_type) {
                elem_type = QoreIterateOperatorNode::getElementTypeInfo(op->getSource(), source_type);
            }
        }
        result_list = builder.createEmptyList(loc, elem_type)->result;
    }
    if (root_foldl) {
        fold_initial_accum = builder.createConstNothing(loc)->result;
        fold_initial_has_accum = builder.createConstBool(false, loc)->result;
    }
    {
        auto* br = builder.createBranch(header_block, loc);
        setLoopCheckpointExceptionTarget(br, header_block);
    }

    struct FusedLoopState {
        std::vector<QoreIRValue> stage_indices;
        QoreIRValue root_index;
        QoreIRValue count;
        QoreIRValue fold_accum;
        QoreIRValue fold_has_accum;
    };

    struct FusedContinuePath {
        QoreIRBasicBlock* block;
        FusedLoopState state;
    };

    std::vector<FusedContinuePath> continue_paths;
    std::vector<QoreIRPhiIncoming> final_incoming;

    auto add_final = [&](QoreIRValue value, const QoreProgramLocation* branch_loc) {
        QoreIRBasicBlock* block = builder.getBlock();
        builder.createBranch(final_block, branch_loc);
        final_incoming.push_back({value, block});
    };

    auto add_continue = [&](const FusedLoopState& state, const QoreProgramLocation* branch_loc) {
        QoreIRBasicBlock* block = builder.getBlock();
        auto* br = builder.createBranch(header_block, branch_loc);
        setLoopCheckpointExceptionTarget(br, header_block);
        continue_paths.push_back({block, state});
    };

    builder.setBlock(header_block);
    std::vector<QoreIRPhiInstruction*> stage_phis;
    std::vector<QoreIRValue> stage_indices;
    stage_phis.reserve(stages.size());
    stage_indices.reserve(stages.size());
    for (size_t i = 0; i < stages.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "fused streaming PHI lowering")) {
            error = "fused streaming PHI lowering cancelled";
            return QoreIRValue();
        }
        auto* phi = builder.createPhi({}, stages[i].loc, QoreIRPhiValueKind::NativeInt);
        stage_phis.push_back(phi);
        stage_indices.push_back(phi->result);
    }

    QoreIRPhiInstruction* root_index_phi = nullptr;
    QoreIRPhiInstruction* count_phi = nullptr;
    QoreIRPhiInstruction* fold_accum_phi = nullptr;
    QoreIRPhiInstruction* fold_has_accum_phi = nullptr;
    QoreIRValue root_index;
    QoreIRValue count_value;
    if (root_terminal) {
        root_index_phi = builder.createPhi({}, loc, QoreIRPhiValueKind::NativeInt);
        root_index = root_index_phi->result;
        if (op->getKind() == QoreStreamingOperatorNode::Count) {
            count_phi = builder.createPhi({}, loc, QoreIRPhiValueKind::NativeInt);
            count_value = count_phi->result;
        }
    }
    QoreIRValue fold_accum;
    QoreIRValue fold_has_accum;
    if (root_foldl) {
        fold_accum_phi = builder.createPhi({}, loc);
        fold_accum = fold_accum_phi->result;
        fold_has_accum_phi = builder.createPhi({}, loc);
        fold_has_accum = fold_has_accum_phi->result;
    }

    for (size_t i = 0; i < stages.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "fused streaming take lowering")) {
            error = "fused streaming take lowering cancelled";
            return QoreIRValue();
        }
        if (stages[i].kind != LazyPipelineStage::StreamTake) {
            continue;
        }

        QoreIRBasicBlock* check_next_block = createBlock("stream.fused.take.check.next");
        if (!check_next_block) {
            error = "IR builder failed to create fused streaming take check block";
            return QoreIRValue();
        }
        QoreIRValue at_limit = builder.createBinaryOp(QoreIROpcode::GeInt, stage_indices[i], limits[i],
            stages[i].loc)->result;
        builder.createBranchIf(at_limit, exit_block, check_next_block, stages[i].loc);
        builder.setBlock(check_next_block);
    }

    builder.createBranch(next_block, loc);

    builder.setBlock(next_block);
    auto* next_inst = builder.createIteratorNext(iter_val, exit_block, body_block, loc);
    QoreIRValue element_val = next_inst->result;

    builder.setBlock(body_block);
    FusedLoopState state{stage_indices, root_index, count_value, fold_accum, fold_has_accum};
    QoreIRValue one = builder.createConstInt(1, loc)->result;

    for (size_t i = 0; i < stages.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "fused streaming body lowering")) {
            error = "fused streaming body lowering cancelled";
            return QoreIRValue();
        }
        const LazyPipelineStage& stage = stages[i];
        switch (stage.kind) {
            case LazyPipelineStage::StreamTake: {
                state.stage_indices[i] = builder.createBinaryOp(QoreIROpcode::AddInt, state.stage_indices[i],
                    one, stage.loc)->result;
                break;
            }
            case LazyPipelineStage::StreamDrop: {
                QoreIRValue next_stage_index = builder.createBinaryOp(QoreIROpcode::AddInt,
                    state.stage_indices[i], one, stage.loc)->result;
                FusedLoopState updated = state;
                updated.stage_indices[i] = next_stage_index;

                QoreIRBasicBlock* keep_block = createBlock("stream.fused.drop.keep");
                QoreIRBasicBlock* drop_block = createBlock("stream.fused.drop.skip");
                if (!keep_block || !drop_block) {
                    error = "IR builder failed to create fused streaming drop blocks";
                    return QoreIRValue();
                }
                QoreIRValue keep = builder.createBinaryOp(QoreIROpcode::GeInt, state.stage_indices[i], limits[i],
                    stage.loc)->result;
                builder.createBranchIf(keep, keep_block, drop_block, stage.loc);

                builder.setBlock(drop_block);
                add_continue(updated, stage.loc);

                builder.setBlock(keep_block);
                state = updated;
                break;
            }
            case LazyPipelineStage::StreamTakeWhile:
            case LazyPipelineStage::StreamTakeUntil: {
                QoreIRValue pred_bool = lower_predicate(stage.primary, stage.loc, element_val, state.stage_indices[i]);
                if (!pred_bool.isValid()) {
                    return QoreIRValue();
                }

                QoreIRBasicBlock* include_block = createBlock("stream.fused.boundary.include");
                QoreIRBasicBlock* stop_block = createBlock("stream.fused.boundary.stop");
                if (!include_block || !stop_block) {
                    error = "IR builder failed to create fused streaming boundary blocks";
                    return QoreIRValue();
                }
                if (stage.kind == LazyPipelineStage::StreamTakeWhile) {
                    builder.createBranchIf(pred_bool, include_block, stop_block, stage.loc);
                } else {
                    builder.createBranchIf(pred_bool, stop_block, include_block, stage.loc);
                }

                builder.setBlock(stop_block);
                builder.createBranch(exit_block, stage.loc);

                builder.setBlock(include_block);
                state.stage_indices[i] = builder.createBinaryOp(QoreIROpcode::AddInt, state.stage_indices[i],
                    one, stage.loc)->result;
                break;
            }
            case LazyPipelineStage::Select: {
                QoreIRValue pred_bool = lower_predicate(stage.primary, stage.loc, element_val, state.stage_indices[i]);
                if (!pred_bool.isValid()) {
                    return QoreIRValue();
                }

                QoreIRValue next_stage_index = builder.createBinaryOp(QoreIROpcode::AddInt,
                    state.stage_indices[i], one, stage.loc)->result;
                FusedLoopState updated = state;
                updated.stage_indices[i] = next_stage_index;

                QoreIRBasicBlock* pass_block = createBlock("stream.fused.select.pass");
                QoreIRBasicBlock* skip_block = createBlock("stream.fused.select.skip");
                if (!pass_block || !skip_block) {
                    error = "IR builder failed to create fused select blocks";
                    return QoreIRValue();
                }
                builder.createBranchIf(pred_bool, pass_block, skip_block, stage.loc);

                builder.setBlock(skip_block);
                add_continue(updated, stage.loc);

                builder.setBlock(pass_block);
                state = updated;
                break;
            }
            case LazyPipelineStage::Map: {
                assert(stage.primary);
                QoreIRValue map_result = lower_with_implicit(*stage.primary, stage.loc, element_val,
                    state.stage_indices[i]);
                if (!map_result.isValid()) {
                    return QoreIRValue();
                }
                element_val = map_result;
                state.stage_indices[i] = builder.createBinaryOp(QoreIROpcode::AddInt, state.stage_indices[i],
                    one, stage.loc)->result;
                break;
            }
            case LazyPipelineStage::MapSelect: {
                assert(stage.primary);
                QoreIRValue pred_bool = lower_predicate(stage.secondary, stage.loc, element_val,
                    state.stage_indices[i]);
                if (!pred_bool.isValid()) {
                    return QoreIRValue();
                }

                QoreIRValue next_stage_index = builder.createBinaryOp(QoreIROpcode::AddInt,
                    state.stage_indices[i], one, stage.loc)->result;
                FusedLoopState updated = state;
                updated.stage_indices[i] = next_stage_index;

                QoreIRBasicBlock* map_block = createBlock("stream.fused.mapselect.map");
                QoreIRBasicBlock* skip_block = createBlock("stream.fused.mapselect.skip");
                if (!map_block || !skip_block) {
                    error = "IR builder failed to create fused map-select blocks";
                    return QoreIRValue();
                }
                builder.createBranchIf(pred_bool, map_block, skip_block, stage.loc);

                builder.setBlock(skip_block);
                add_continue(updated, stage.loc);

                builder.setBlock(map_block);
                QoreIRValue map_result = lower_with_implicit(*stage.primary, stage.loc, element_val,
                    state.stage_indices[i]);
                if (!map_result.isValid()) {
                    return QoreIRValue();
                }
                element_val = map_result;
                state = updated;
                break;
            }
        }
    }

    if (root_list) {
        if (!need_result) {
            add_continue(state, loc);
        } else {
            builder.createListAppend(result_list, element_val, loc);
            add_continue(state, loc);
        }
    } else if (root_foldl) {
        QoreIRBasicBlock* init_accum_block = createBlock("stream.fused.fold.init");
        QoreIRBasicBlock* fold_block = createBlock("stream.fused.fold.body");
        QoreIRBasicBlock* fold_cont_block = createBlock("stream.fused.fold.cont");
        if (!init_accum_block || !fold_block || !fold_cont_block) {
            error = "IR builder failed to create fused fold blocks";
            return QoreIRValue();
        }
        builder.createBranchIf(state.fold_has_accum, fold_block, init_accum_block, loc);

        builder.setBlock(init_accum_block);
        QoreIRValue true_val = builder.createConstBool(true, loc)->result;
        QoreIRValue init_accum_val = builder.createRefSelf(element_val, loc)->result;
        QoreIRBasicBlock* init_accum_exit_block = builder.getBlock();
        builder.createBranch(fold_cont_block, loc);

        builder.setBlock(fold_block);
        QoreIRValue fold_result = lower_fold_expr(state.fold_accum, element_val);
        if (!fold_result.isValid()) {
            return QoreIRValue();
        }
        QoreIRBasicBlock* fold_exit_block = builder.getBlock();
        builder.createBranch(fold_cont_block, loc);

        builder.setBlock(fold_cont_block);
        auto* next_accum_phi = builder.createPhi(
            {{init_accum_val, init_accum_exit_block}, {fold_result, fold_exit_block}}, loc);
        auto* next_has_phi = builder.createPhi({{true_val, init_accum_exit_block}, {true_val, fold_exit_block}},
            loc);
        FusedLoopState next_state = state;
        next_state.fold_accum = next_accum_phi->result;
        next_state.fold_has_accum = next_has_phi->result;
        add_continue(next_state, loc);
    } else if (op->getKind() == QoreStreamingOperatorNode::Count) {
        QoreIRValue pred_bool = lower_predicate(op->hasPredicate() ? &op->getPredicate() : nullptr, loc,
            element_val, state.root_index);
        if (!pred_bool.isValid()) {
            return QoreIRValue();
        }

        QoreIRBasicBlock* inc_block = createBlock("stream.fused.count.inc");
        QoreIRBasicBlock* cont_block = createBlock("stream.fused.count.cont");
        if (!inc_block || !cont_block) {
            error = "IR builder failed to create fused streaming count blocks";
            return QoreIRValue();
        }
        QoreIRBasicBlock* predicate_block = builder.getBlock();
        builder.createBranchIf(pred_bool, inc_block, cont_block, loc);

        builder.setBlock(inc_block);
        QoreIRValue inc_count = builder.createBinaryOp(QoreIROpcode::AddInt, state.count, one, loc)->result;
        builder.createBranch(cont_block, loc);

        builder.setBlock(cont_block);
        auto* next_count_phi = builder.createPhi({{inc_count, inc_block}, {state.count, predicate_block}}, loc,
            QoreIRPhiValueKind::NativeInt);
        FusedLoopState next_state = state;
        next_state.count = next_count_phi->result;
        next_state.root_index = builder.createBinaryOp(QoreIROpcode::AddInt, state.root_index, one, loc)->result;
        add_continue(next_state, loc);
    } else {
        QoreIRValue pred_bool = lower_predicate(op->hasPredicate() ? &op->getPredicate() : nullptr, loc,
            element_val, state.root_index);
        if (!pred_bool.isValid()) {
            return QoreIRValue();
        }

        QoreIRBasicBlock* match_block = createBlock("stream.fused.terminal.match");
        QoreIRBasicBlock* cont_block = createBlock("stream.fused.terminal.cont");
        if (!match_block || !cont_block) {
            error = "IR builder failed to create fused streaming terminal blocks";
            return QoreIRValue();
        }
        if (op->getKind() == QoreStreamingOperatorNode::All) {
            builder.createBranchIf(pred_bool, cont_block, match_block, loc);
        } else {
            builder.createBranchIf(pred_bool, match_block, cont_block, loc);
        }

        builder.setBlock(match_block);
        QoreIRValue match_result;
        if (op->getKind() == QoreStreamingOperatorNode::First) {
            match_result = element_val;
        } else if (op->getKind() == QoreStreamingOperatorNode::Any) {
            match_result = builder.createConstBool(true, loc)->result;
        } else {
            match_result = builder.createConstBool(false, loc)->result;
        }
        add_final(match_result, loc);

        builder.setBlock(cont_block);
        FusedLoopState next_state = state;
        next_state.root_index = builder.createBinaryOp(QoreIROpcode::AddInt, state.root_index, one, loc)->result;
        add_continue(next_state, loc);
    }

    for (size_t i = 0; i < stage_phis.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "fused streaming incoming lowering")) {
            error = "fused streaming incoming lowering cancelled";
            return QoreIRValue();
        }
        stage_phis[i]->incoming.push_back({zero, preheader_block});
        stage_phis[i]->operands.push_back(zero);
        size_t path_count = 0;
        for (const FusedContinuePath& path : continue_paths) {
            if (++path_count % 100 == 0 && qore_check_cancel(nullptr, "fused streaming PHI incoming lowering")) {
                error = "fused streaming PHI incoming lowering cancelled";
                return QoreIRValue();
            }
            stage_phis[i]->incoming.push_back({path.state.stage_indices[i], path.block});
            stage_phis[i]->operands.push_back(path.state.stage_indices[i]);
        }
    }
    if (root_index_phi) {
        root_index_phi->incoming.push_back({zero, preheader_block});
        root_index_phi->operands.push_back(zero);
        size_t path_count = 0;
        for (const FusedContinuePath& path : continue_paths) {
            if (++path_count % 100 == 0 && qore_check_cancel(nullptr, "fused streaming root incoming lowering")) {
                error = "fused streaming root incoming lowering cancelled";
                return QoreIRValue();
            }
            root_index_phi->incoming.push_back({path.state.root_index, path.block});
            root_index_phi->operands.push_back(path.state.root_index);
        }
    }
    if (count_phi) {
        count_phi->incoming.push_back({zero, preheader_block});
        count_phi->operands.push_back(zero);
        size_t path_count = 0;
        for (const FusedContinuePath& path : continue_paths) {
            if (++path_count % 100 == 0 && qore_check_cancel(nullptr, "fused streaming count incoming lowering")) {
                error = "fused streaming count incoming lowering cancelled";
                return QoreIRValue();
            }
            count_phi->incoming.push_back({path.state.count, path.block});
            count_phi->operands.push_back(path.state.count);
        }
    }
    if (fold_accum_phi) {
        fold_accum_phi->incoming.push_back({fold_initial_accum, preheader_block});
        fold_accum_phi->operands.push_back(fold_initial_accum);
        size_t path_count = 0;
        for (const FusedContinuePath& path : continue_paths) {
            if (++path_count % 100 == 0 && qore_check_cancel(nullptr, "fused fold accumulator incoming lowering")) {
                error = "fused fold accumulator incoming lowering cancelled";
                return QoreIRValue();
            }
            fold_accum_phi->incoming.push_back({path.state.fold_accum, path.block});
            fold_accum_phi->operands.push_back(path.state.fold_accum);
        }
    }
    if (fold_has_accum_phi) {
        fold_has_accum_phi->incoming.push_back({fold_initial_has_accum, preheader_block});
        fold_has_accum_phi->operands.push_back(fold_initial_has_accum);
        size_t path_count = 0;
        for (const FusedContinuePath& path : continue_paths) {
            if (++path_count % 100 == 0 && qore_check_cancel(nullptr, "fused fold state incoming lowering")) {
                error = "fused fold state incoming lowering cancelled";
                return QoreIRValue();
            }
            fold_has_accum_phi->incoming.push_back({path.state.fold_has_accum, path.block});
            fold_has_accum_phi->operands.push_back(path.state.fold_has_accum);
        }
    }

    builder.setBlock(null_block);
    QoreIRValue null_result;
    if (!root_streaming) {
        null_result = builder.createConstNothing(loc)->result;
    } else {
        switch (op->getKind()) {
            case QoreStreamingOperatorNode::First:
                null_result = builder.createConstNothing(loc)->result;
                break;
            case QoreStreamingOperatorNode::Any:
                null_result = builder.createConstBool(false, loc)->result;
                break;
            case QoreStreamingOperatorNode::All:
                null_result = builder.createConstBool(true, loc)->result;
                break;
            case QoreStreamingOperatorNode::Count:
                null_result = zero;
                break;
            case QoreStreamingOperatorNode::Take:
            case QoreStreamingOperatorNode::Drop:
            case QoreStreamingOperatorNode::TakeWhile:
            case QoreStreamingOperatorNode::TakeUntil:
                null_result = builder.createConstNothing(loc)->result;
                break;
        }
    }
    add_final(null_result, loc);

    builder.setBlock(exit_block);
    if (root_foldl) {
        QoreIRBasicBlock* fold_result_block = createBlock("stream.fused.fold.result");
        QoreIRBasicBlock* fold_empty_block = createBlock("stream.fused.fold.empty");
        if (!fold_result_block || !fold_empty_block) {
            error = "IR builder failed to create fused fold exit blocks";
            return QoreIRValue();
        }
        builder.createBranchIf(fold_has_accum_phi->result, fold_result_block, fold_empty_block, loc);

        builder.setBlock(fold_result_block);
        add_final(fold_accum_phi->result, loc);

        builder.setBlock(fold_empty_block);
        add_final(builder.createConstNothing(loc)->result, loc);
    } else if (root_list && needs_runtime_unwrap_check) {
        QoreIRBasicBlock* list_result_block = createBlock("stream.fused.list.result");
        QoreIRBasicBlock* unwrap_check_block = createBlock("stream.fused.unwrap.check");
        QoreIRBasicBlock* unwrap_get_block = createBlock("stream.fused.unwrap.get");
        QoreIRBasicBlock* unwrap_nothing_block = createBlock("stream.fused.unwrap.nothing");
        if (!list_result_block || !unwrap_check_block || !unwrap_get_block || !unwrap_nothing_block) {
            error = "IR builder failed to create fused list unwrap blocks";
            return QoreIRValue();
        }
        builder.createBranchIf(is_collection_val, list_result_block, unwrap_check_block, loc);

        builder.setBlock(list_result_block);
        add_final(result_list, loc);

        builder.setBlock(unwrap_check_block);
        QoreIRValue list_size = builder.createListSize(result_list, loc)->result;
        builder.createBranchIf(list_size, unwrap_get_block, unwrap_nothing_block, loc);

        builder.setBlock(unwrap_get_block);
        QoreIRValue zero_idx = builder.createConstInt(0, loc)->result;
        QoreIRValue unwrapped = builder.createBinaryOp(QoreIROpcode::ListGetValue, result_list, zero_idx,
            loc)->result;
        add_final(unwrapped, loc);

        builder.setBlock(unwrap_nothing_block);
        add_final(builder.createConstNothing(loc)->result, loc);
    } else {
        QoreIRValue exit_result;
        if (root_list) {
            exit_result = need_result ? result_list : builder.createConstNothing(loc)->result;
        } else {
            switch (op->getKind()) {
                case QoreStreamingOperatorNode::First:
                    exit_result = builder.createConstNothing(loc)->result;
                    break;
                case QoreStreamingOperatorNode::Any:
                    exit_result = builder.createConstBool(false, loc)->result;
                    break;
                case QoreStreamingOperatorNode::All:
                    exit_result = builder.createConstBool(true, loc)->result;
                    break;
                case QoreStreamingOperatorNode::Count:
                    exit_result = count_phi ? count_phi->result : zero;
                    break;
                case QoreStreamingOperatorNode::Take:
                case QoreStreamingOperatorNode::Drop:
                case QoreStreamingOperatorNode::TakeWhile:
                case QoreStreamingOperatorNode::TakeUntil:
                    exit_result = result_list;
                    break;
            }
        }
        add_final(exit_result, loc);
    }

    builder.setBlock(final_block);
    auto* result_phi = builder.createPhi(final_incoming, loc);
    return result_phi->result;
}

bool QoreIRLowering::collectLazyPipelineStages(const QoreValue& source, QoreValue& base_source,
        std::vector<LazyPipelineStage>& source_stages, std::string& error) {
    base_source = source;
    size_t source_stage_count = 0;
    while (true) {
        const AbstractQoreNode* source_node = base_source.getInternalNode();
        if (auto* source_op = dynamic_cast<const QoreStreamingOperatorNode*>(source_node)) {
            if (qore_streaming_kind_is_terminal(source_op->getKind())) {
                break;
            }
            LazyPipelineStage::Kind kind;
            switch (source_op->getKind()) {
                case QoreStreamingOperatorNode::Take:
                    kind = LazyPipelineStage::StreamTake;
                    break;
                case QoreStreamingOperatorNode::Drop:
                    kind = LazyPipelineStage::StreamDrop;
                    break;
                case QoreStreamingOperatorNode::TakeWhile:
                    kind = LazyPipelineStage::StreamTakeWhile;
                    break;
                case QoreStreamingOperatorNode::TakeUntil:
                    kind = LazyPipelineStage::StreamTakeUntil;
                    break;
                case QoreStreamingOperatorNode::First:
                case QoreStreamingOperatorNode::Any:
                case QoreStreamingOperatorNode::All:
                case QoreStreamingOperatorNode::Count:
                    assert(false);
                    return false;
            }
            if (++source_stage_count % 100 == 0 && qore_check_cancel(nullptr, "lazy source-stage lowering")) {
                error = "lazy source-stage lowering cancelled";
                return false;
            }
            source_stages.push_back({kind, &source_op->getPredicate(), nullptr, source_op->loc});
            base_source = source_op->getSource();
            continue;
        }

        if (auto* map_select = dynamic_cast<const QoreMapSelectOperatorNode*>(source_node)) {
            if (++source_stage_count % 100 == 0 && qore_check_cancel(nullptr, "lazy source-stage lowering")) {
                error = "lazy source-stage lowering cancelled";
                return false;
            }
            source_stages.push_back({LazyPipelineStage::MapSelect, &map_select->getMapExpression(),
                &map_select->getSelectExpression(), map_select->loc});
            base_source = map_select->getIteratorExpr();
            continue;
        }

        if (auto* map = dynamic_cast<const QoreMapOperatorNode*>(source_node)) {
            if (++source_stage_count % 100 == 0 && qore_check_cancel(nullptr, "lazy source-stage lowering")) {
                error = "lazy source-stage lowering cancelled";
                return false;
            }
            source_stages.push_back({LazyPipelineStage::Map, &map->getMapExpression(), nullptr, map->loc});
            base_source = map->getIteratorExpr();
            continue;
        }

        if (auto* select = dynamic_cast<const QoreSelectOperatorNode*>(source_node)) {
            if (++source_stage_count % 100 == 0 && qore_check_cancel(nullptr, "lazy source-stage lowering")) {
                error = "lazy source-stage lowering cancelled";
                return false;
            }
            source_stages.push_back({LazyPipelineStage::Select, &select->getPredicateExpression(), nullptr,
                select->loc});
            base_source = select->getSourceExpression();
            continue;
        }

        break;
    }

    std::reverse(source_stages.begin(), source_stages.end());
    return true;
}

QoreIRValue QoreIRLowering::lowerStreamingFused(const QoreStreamingOperatorNode* op,
        const QoreValue& base_source, const std::vector<LazyPipelineStage>& source_stages, std::string& error) {
    LazyPipelineRoot root;
    root.kind = LazyPipelineRoot::Streaming;
    root.streaming = op;
    root.loc = op->loc;
    root.need_result = op->needsReturnValue();
    return lowerLazyPipelineFused(base_source, source_stages, root, error);
}

bool QoreIRLowering::lowerForeachLazyPipelineFused(const ForEachStatement* foreach_stmt,
        const QoreValue& base_source, const std::vector<LazyPipelineStage>& source_stages, std::string& error) {
    if (!ensureBuilderContext(error)) {
        return false;
    }

    const QoreProgramLocation* loc = foreach_stmt->loc;
    std::vector<LazyPipelineStage> stages = source_stages;

    QoreIRValue source = lowerExpression(base_source, error);
    if (!source.isValid()) {
        return false;
    }

    bool source_uses_iterate = !stages.empty()
        && (stages.front().kind == LazyPipelineStage::StreamTake
            || stages.front().kind == LazyPipelineStage::StreamDrop
            || stages.front().kind == LazyPipelineStage::StreamTakeWhile
            || stages.front().kind == LazyPipelineStage::StreamTakeUntil);
    auto* iter_inst = source_uses_iterate
        ? builder.createIteratorCreateIterate(source, loc)
        : builder.createIteratorCreate(source, nullptr, loc);
    QoreIRValue iter_val = iter_inst->result;

    auto lower_with_implicit = [&](const QoreValue& stage_expr, const QoreProgramLocation* stage_loc,
            QoreIRValue element_val, QoreIRValue index_val) -> QoreIRValue {
        QoreIRValue old_element = builder.createPushImplicitElement(index_val, stage_loc)->result;
        QoreIRValue old_argv = builder.createPushImplicitArg(element_val, stage_loc)->result;

        VirtualImplicitContext saved = virtual_implicit;
        virtual_implicit.arg0 = element_val;
        virtual_implicit.arg1 = QoreIRValue();
        virtual_implicit.element = index_val;
        virtual_implicit.active = true;

        QoreIRValue result = lowerExpression(stage_expr, error);

        virtual_implicit = saved;
        builder.createPopImplicitArg(old_argv, stage_loc);
        builder.createPopImplicitElement(old_element, stage_loc);

        return result;
    };

    auto lower_predicate = [&](const QoreValue* pred, const QoreProgramLocation* pred_loc, QoreIRValue element_val,
            QoreIRValue index_val) -> QoreIRValue {
        if (!pred || !static_cast<bool>(*pred)) {
            return builder.createConstBool(true, pred_loc)->result;
        }

        QoreIRValue pred_result = lower_with_implicit(*pred, pred_loc, element_val, index_val);
        if (!pred_result.isValid()) {
            return QoreIRValue();
        }
        return builder.createUnaryOp(QoreIROpcode::ToBool, pred_result, pred_loc)->result;
    };

    auto emit_negative_limit_throw = [&](const char* name, const QoreProgramLocation* stage_loc) {
        QoreIRValue err = builder.createConstString("STREAMING-OPERATOR-ERROR", stage_loc)->result;
        QoreIRValue msg = builder.createConstString(
            std::string(name) + " requires a non-negative count", stage_loc)->result;
        QoreIRValue throw_list = builder.createMakeList({err, msg}, stage_loc)->result;
        QoreIRBasicBlock* handler = exception_stack.empty() ? nullptr : exception_stack.back();
        builder.createThrow(throw_list, handler, stage_loc);
    };

    std::vector<QoreIRValue> limits(stages.size());
    for (size_t i = 0; i < stages.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "fused foreach limit lowering")) {
            error = "fused foreach limit lowering cancelled";
            return false;
        }
        if (stages[i].kind != LazyPipelineStage::StreamTake && stages[i].kind != LazyPipelineStage::StreamDrop) {
            continue;
        }

        assert(stages[i].primary);
        limits[i] = lowerExpression(*stages[i].primary, error);
        if (!limits[i].isValid()) {
            return false;
        }

        QoreIRBasicBlock* limit_err_block = createBlock("foreach.fused.limit.error");
        QoreIRBasicBlock* limit_ok_block = createBlock("foreach.fused.limit.ok");
        if (!limit_err_block || !limit_ok_block) {
            error = "IR builder failed to create blocks for fused foreach limit check";
            return false;
        }

        QoreIRValue zero = builder.createConstInt(0, stages[i].loc)->result;
        QoreIRValue is_negative = builder.createBinaryOp(QoreIROpcode::LtInt, limits[i], zero,
            stages[i].loc)->result;
        builder.createBranchIf(is_negative, limit_err_block, limit_ok_block, stages[i].loc);

        builder.setBlock(limit_err_block);
        emit_negative_limit_throw(stages[i].kind == LazyPipelineStage::StreamTake ? "take" : "drop", stages[i].loc);

        builder.setBlock(limit_ok_block);
    }

    QoreIRBasicBlock* exit_block = createBlock("foreach.fused.exit");
    QoreIRBasicBlock* null_block = createBlock("foreach.fused.null");
    QoreIRBasicBlock* preheader_block = createBlock("foreach.fused.preheader");
    QoreIRBasicBlock* header_block = createBlock("foreach.fused.header");
    QoreIRBasicBlock* next_block = createBlock("foreach.fused.next");
    QoreIRBasicBlock* body_block = createBlock("foreach.fused.body");
    QoreIRBasicBlock* latch_block = createBlock("foreach.fused.latch");
    if (!exit_block || !null_block || !preheader_block || !header_block || !next_block || !body_block
            || !latch_block) {
        error = "IR builder failed to create blocks for fused foreach";
        return false;
    }
    header_block->is_loop_header = true;

    QoreIRValue zero = builder.createConstInt(0, loc)->result;
    QoreIRValue is_null = builder.createBinaryOp(QoreIROpcode::EqInt, iter_val, zero, loc)->result;
    builder.createBranchIf(is_null, null_block, preheader_block, loc);

    builder.setBlock(null_block);
    builder.createBranch(exit_block, loc);

    builder.setBlock(preheader_block);
    {
        auto* br = builder.createBranch(header_block, loc);
        setLoopCheckpointExceptionTarget(br, header_block);
    }

    struct FusedForeachState {
        std::vector<QoreIRValue> stage_indices;
        QoreIRValue foreach_index;
    };

    struct FusedForeachContinuePath {
        QoreIRBasicBlock* block;
        FusedForeachState state;
    };

    std::vector<FusedForeachContinuePath> continue_paths;

    auto add_continue = [&](const FusedForeachState& state, const QoreProgramLocation* branch_loc) {
        QoreIRBasicBlock* block = builder.getBlock();
        auto* br = builder.createBranch(header_block, branch_loc);
        setLoopCheckpointExceptionTarget(br, header_block);
        continue_paths.push_back({block, state});
    };

    builder.setBlock(header_block);
    std::vector<QoreIRPhiInstruction*> stage_phis;
    std::vector<QoreIRValue> stage_indices;
    stage_phis.reserve(stages.size());
    stage_indices.reserve(stages.size());
    for (size_t i = 0; i < stages.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "fused foreach PHI lowering")) {
            error = "fused foreach PHI lowering cancelled";
            return false;
        }
        auto* phi = builder.createPhi({}, stages[i].loc, QoreIRPhiValueKind::NativeInt);
        stage_phis.push_back(phi);
        stage_indices.push_back(phi->result);
    }
    auto* foreach_index_phi = builder.createPhi({}, loc, QoreIRPhiValueKind::NativeInt);
    QoreIRValue foreach_index = foreach_index_phi->result;

    for (size_t i = 0; i < stages.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "fused foreach take lowering")) {
            error = "fused foreach take lowering cancelled";
            return false;
        }
        if (stages[i].kind != LazyPipelineStage::StreamTake) {
            continue;
        }

        QoreIRBasicBlock* check_next_block = createBlock("foreach.fused.take.check.next");
        if (!check_next_block) {
            error = "IR builder failed to create fused foreach take check block";
            return false;
        }
        QoreIRValue at_limit = builder.createBinaryOp(QoreIROpcode::GeInt, stage_indices[i], limits[i],
            stages[i].loc)->result;
        builder.createBranchIf(at_limit, exit_block, check_next_block, stages[i].loc);
        builder.setBlock(check_next_block);
    }

    builder.createBranch(next_block, loc);

    builder.setBlock(next_block);
    auto* next_inst = builder.createIteratorNext(iter_val, exit_block, body_block, loc);
    QoreIRValue element_val = next_inst->result;

    builder.setBlock(body_block);
    FusedForeachState state{stage_indices, foreach_index};
    QoreIRValue one = builder.createConstInt(1, loc)->result;

    for (size_t i = 0; i < stages.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "fused foreach body lowering")) {
            error = "fused foreach body lowering cancelled";
            return false;
        }
        const LazyPipelineStage& stage = stages[i];
        switch (stage.kind) {
            case LazyPipelineStage::StreamTake: {
                state.stage_indices[i] = builder.createBinaryOp(QoreIROpcode::AddInt, state.stage_indices[i],
                    one, stage.loc)->result;
                break;
            }
            case LazyPipelineStage::StreamDrop: {
                QoreIRValue next_stage_index = builder.createBinaryOp(QoreIROpcode::AddInt,
                    state.stage_indices[i], one, stage.loc)->result;
                FusedForeachState updated = state;
                updated.stage_indices[i] = next_stage_index;

                QoreIRBasicBlock* keep_block = createBlock("foreach.fused.drop.keep");
                QoreIRBasicBlock* drop_block = createBlock("foreach.fused.drop.skip");
                if (!keep_block || !drop_block) {
                    error = "IR builder failed to create fused foreach drop blocks";
                    return false;
                }
                QoreIRValue keep = builder.createBinaryOp(QoreIROpcode::GeInt, state.stage_indices[i], limits[i],
                    stage.loc)->result;
                builder.createBranchIf(keep, keep_block, drop_block, stage.loc);

                builder.setBlock(drop_block);
                add_continue(updated, stage.loc);

                builder.setBlock(keep_block);
                state = updated;
                break;
            }
            case LazyPipelineStage::StreamTakeWhile:
            case LazyPipelineStage::StreamTakeUntil: {
                QoreIRValue pred_bool = lower_predicate(stage.primary, stage.loc, element_val, state.stage_indices[i]);
                if (!pred_bool.isValid()) {
                    return false;
                }

                QoreIRBasicBlock* include_block = createBlock("foreach.fused.boundary.include");
                QoreIRBasicBlock* stop_block = createBlock("foreach.fused.boundary.stop");
                if (!include_block || !stop_block) {
                    error = "IR builder failed to create fused foreach boundary blocks";
                    return false;
                }
                if (stage.kind == LazyPipelineStage::StreamTakeWhile) {
                    builder.createBranchIf(pred_bool, include_block, stop_block, stage.loc);
                } else {
                    builder.createBranchIf(pred_bool, stop_block, include_block, stage.loc);
                }

                builder.setBlock(stop_block);
                builder.createBranch(exit_block, stage.loc);

                builder.setBlock(include_block);
                state.stage_indices[i] = builder.createBinaryOp(QoreIROpcode::AddInt, state.stage_indices[i],
                    one, stage.loc)->result;
                break;
            }
            case LazyPipelineStage::Select: {
                QoreIRValue pred_bool = lower_predicate(stage.primary, stage.loc, element_val, state.stage_indices[i]);
                if (!pred_bool.isValid()) {
                    return false;
                }

                QoreIRValue next_stage_index = builder.createBinaryOp(QoreIROpcode::AddInt,
                    state.stage_indices[i], one, stage.loc)->result;
                FusedForeachState updated = state;
                updated.stage_indices[i] = next_stage_index;

                QoreIRBasicBlock* pass_block = createBlock("foreach.fused.select.pass");
                QoreIRBasicBlock* skip_block = createBlock("foreach.fused.select.skip");
                if (!pass_block || !skip_block) {
                    error = "IR builder failed to create fused foreach select blocks";
                    return false;
                }
                builder.createBranchIf(pred_bool, pass_block, skip_block, stage.loc);

                builder.setBlock(skip_block);
                add_continue(updated, stage.loc);

                builder.setBlock(pass_block);
                state = updated;
                break;
            }
            case LazyPipelineStage::Map: {
                assert(stage.primary);
                QoreIRValue map_result = lower_with_implicit(*stage.primary, stage.loc, element_val,
                    state.stage_indices[i]);
                if (!map_result.isValid()) {
                    return false;
                }
                element_val = map_result;
                state.stage_indices[i] = builder.createBinaryOp(QoreIROpcode::AddInt, state.stage_indices[i],
                    one, stage.loc)->result;
                break;
            }
            case LazyPipelineStage::MapSelect: {
                assert(stage.primary);
                QoreIRValue pred_bool = lower_predicate(stage.secondary, stage.loc, element_val,
                    state.stage_indices[i]);
                if (!pred_bool.isValid()) {
                    return false;
                }

                QoreIRValue next_stage_index = builder.createBinaryOp(QoreIROpcode::AddInt,
                    state.stage_indices[i], one, stage.loc)->result;
                FusedForeachState updated = state;
                updated.stage_indices[i] = next_stage_index;

                QoreIRBasicBlock* map_block = createBlock("foreach.fused.mapselect.map");
                QoreIRBasicBlock* skip_block = createBlock("foreach.fused.mapselect.skip");
                if (!map_block || !skip_block) {
                    error = "IR builder failed to create fused foreach map-select blocks";
                    return false;
                }
                builder.createBranchIf(pred_bool, map_block, skip_block, stage.loc);

                builder.setBlock(skip_block);
                add_continue(updated, stage.loc);

                builder.setBlock(map_block);
                QoreIRValue map_result = lower_with_implicit(*stage.primary, stage.loc, element_val,
                    state.stage_indices[i]);
                if (!map_result.isValid()) {
                    return false;
                }
                element_val = map_result;
                state = updated;
                break;
            }
        }
    }

    QoreIRValue old_element = builder.createPushImplicitElement(state.foreach_index, loc)->result;

    QoreValue var_expr = foreach_stmt->getVar();
    if (var_expr && !var_expr.isNothing()) {
        const auto* var_node = dynamic_cast<const VarRefNode*>(var_expr.getInternalNode());
        const QoreTypeInfo* var_type = var_node ? getVarRefTypeInfo(var_node) : nullptr;
        if (var_node && var_node->getType() != VT_IMMEDIATE
                && !QoreTypeInfo::isReference(var_type)) {
            if (!storeVarRef(var_node, element_val, error, "foreach assignment")) {
                return false;
            }
        } else {
            auto* store_inst = builder.createStoreLValue(var_expr, element_val, loc);
            if (!exception_stack.empty()) {
                store_inst->exception_target = exception_stack.back();
            }
        }
    }

    FlowTarget ft;
    ft.break_target = exit_block;
    ft.continue_target = latch_block;
    ft.is_switch = false;
    ft.catch_cleanup_depth = catch_cleanup_depth;
    ft.cleanup_stack_depth = cleanup_stack.size();
    ft.old_implicit_element = old_element;
    flow_stack.push_back(ft);
    StatementBlock* body = foreach_stmt->getCode();
    if (body) {
        ++loop_depth;
        if (!lowerStatementBlock(body, error)) {
            --loop_depth;
            flow_stack.pop_back();
            return false;
        }
        --loop_depth;
    }
    flow_stack.pop_back();

    if (!blockHasTerminator(builder.getBlock())) {
        builder.createBranch(latch_block, loc);
    }

    // Stage and body lowering can create pass/skip/continuation blocks after the
    // placeholder latch block.  Emit the latch after those blocks so LLVM lowering
    // sees the PushImplicitElement result before the latch PopImplicitElement.
    builder.getFunction()->moveBlockToEnd(latch_block);
    builder.setBlock(latch_block);
    builder.createPopImplicitElement(old_element, loc);
    QoreIRValue next_foreach_index = builder.createBinaryOp(QoreIROpcode::AddInt, state.foreach_index, one,
        loc)->result;
    FusedForeachState next_state = state;
    next_state.foreach_index = next_foreach_index;
    QoreIRBasicBlock* latch_exit_block = builder.getBlock();
    {
        auto* br = builder.createBranch(header_block, loc);
        setLoopCheckpointExceptionTarget(br, header_block);
    }
    continue_paths.push_back({latch_exit_block, next_state});

    for (size_t i = 0; i < stage_phis.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "fused foreach incoming lowering")) {
            error = "fused foreach incoming lowering cancelled";
            return false;
        }
        stage_phis[i]->incoming.push_back({zero, preheader_block});
        stage_phis[i]->operands.push_back(zero);
        size_t path_count = 0;
        for (const FusedForeachContinuePath& path : continue_paths) {
            if (++path_count % 100 == 0 && qore_check_cancel(nullptr, "fused foreach PHI incoming lowering")) {
                error = "fused foreach PHI incoming lowering cancelled";
                return false;
            }
            stage_phis[i]->incoming.push_back({path.state.stage_indices[i], path.block});
            stage_phis[i]->operands.push_back(path.state.stage_indices[i]);
        }
    }

    foreach_index_phi->incoming.push_back({zero, preheader_block});
    foreach_index_phi->operands.push_back(zero);
    size_t path_count = 0;
    for (const FusedForeachContinuePath& path : continue_paths) {
        if (++path_count % 100 == 0 && qore_check_cancel(nullptr, "fused foreach index incoming lowering")) {
            error = "fused foreach index incoming lowering cancelled";
            return false;
        }
        foreach_index_phi->incoming.push_back({path.state.foreach_index, path.block});
        foreach_index_phi->operands.push_back(path.state.foreach_index);
    }

    builder.getFunction()->moveBlockToEnd(exit_block);
    builder.setBlock(exit_block);

    if (const LVList* loop_lvars = foreach_stmt->getLVList()) {
        for (int i = static_cast<int>(loop_lvars->size()) - 1; i >= 0; --i) {
            builder.createUninstantiateLocal(loop_lvars->lv[i], loc);
        }
    }
    return true;
}

QoreIRValue QoreIRLowering::lowerStreamingNative(const QoreStreamingOperatorNode* op, const QoreValue& expr,
        std::string& error) {
    if (!ensureBuilderContext(error)) {
        return QoreIRValue();
    }

    const QoreProgramLocation* loc = op->loc;
    std::vector<LazyPipelineStage> source_stages;
    QoreValue base_source;
    if (!collectLazyPipelineStages(op->getSource(), base_source, source_stages, error)) {
        return QoreIRValue();
    }
    if (!source_stages.empty()) {
        return lowerStreamingFused(op, base_source, source_stages, error);
    }

    QoreIRValue source = lowerExpression(op->getSource(), error);
    if (!source.isValid()) {
        return QoreIRValue();
    }

    auto* iter_inst = builder.createIteratorCreateIterate(source, loc);
    QoreIRValue iter_val = iter_inst->result;

    auto lower_predicate = [&](QoreIRValue element_val, QoreIRValue index_val) -> QoreIRValue {
        if (!op->hasPredicate()) {
            return builder.createConstBool(true, loc)->result;
        }

        QoreIRValue old_element = builder.createPushImplicitElement(index_val, loc)->result;
        QoreIRValue old_argv = builder.createPushImplicitArg(element_val, loc)->result;

        VirtualImplicitContext saved = virtual_implicit;
        virtual_implicit.arg0 = element_val;
        virtual_implicit.arg1 = QoreIRValue();
        virtual_implicit.element = index_val;
        virtual_implicit.active = true;

        QoreIRValue pred = lowerExpression(op->getPredicate(), error);

        virtual_implicit = saved;
        builder.createPopImplicitArg(old_argv, loc);
        builder.createPopImplicitElement(old_element, loc);

        if (!pred.isValid()) {
            return QoreIRValue();
        }
        return builder.createUnaryOp(QoreIROpcode::ToBool, pred, loc)->result;
    };

    auto emit_negative_limit_throw = [&](const char* name) {
        QoreIRValue err = builder.createConstString("STREAMING-OPERATOR-ERROR", loc)->result;
        QoreIRValue msg = builder.createConstString(
            std::string(name) + " requires a non-negative count", loc)->result;
        QoreIRValue throw_list = builder.createMakeList({err, msg}, loc)->result;
        QoreIRBasicBlock* handler = exception_stack.empty() ? nullptr : exception_stack.back();
        builder.createThrow(throw_list, handler, loc);
    };

    switch (op->getKind()) {
        case QoreStreamingOperatorNode::First:
        case QoreStreamingOperatorNode::Any:
        case QoreStreamingOperatorNode::All: {
            QoreIRBasicBlock* preheader_block = createBlock("stream.terminal.preheader");
            QoreIRBasicBlock* header_block = createBlock("stream.terminal.header");
            QoreIRBasicBlock* body_block = createBlock("stream.terminal.body");
            QoreIRBasicBlock* match_block = createBlock("stream.terminal.match");
            QoreIRBasicBlock* cont_block = createBlock("stream.terminal.cont");
            QoreIRBasicBlock* exit_block = createBlock("stream.terminal.exit");
            QoreIRBasicBlock* final_block = createBlock("stream.terminal.final");
            if (!preheader_block || !header_block || !body_block || !match_block || !cont_block
                    || !exit_block || !final_block) {
                error = "IR builder failed to create blocks for streaming terminal operator";
                return QoreIRValue();
            }
            header_block->is_loop_header = true;

            QoreIRValue zero = builder.createConstInt(0, loc)->result;
            QoreIRValue is_null = builder.createBinaryOp(QoreIROpcode::EqInt, iter_val, zero, loc)->result;
            builder.createBranchIf(is_null, exit_block, preheader_block, loc);

            builder.setBlock(preheader_block);
            QoreIRValue init_index = builder.createConstInt(0, loc)->result;
            {
                auto* br = builder.createBranch(header_block, loc);
                setLoopCheckpointExceptionTarget(br, header_block);
            }

            builder.setBlock(header_block);
            auto* index_phi = builder.createPhi({}, loc, QoreIRPhiValueKind::NativeInt);
            QoreIRValue index_val = index_phi->result;
            auto* next_inst = builder.createIteratorNext(iter_val, exit_block, body_block, loc);
            QoreIRValue element_val = next_inst->result;

            builder.setBlock(body_block);
            QoreIRValue pred_bool = lower_predicate(element_val, index_val);
            if (!pred_bool.isValid()) {
                return QoreIRValue();
            }
            if (op->getKind() == QoreStreamingOperatorNode::All) {
                builder.createBranchIf(pred_bool, cont_block, match_block, loc);
            } else {
                builder.createBranchIf(pred_bool, match_block, cont_block, loc);
            }

            builder.setBlock(cont_block);
            QoreIRValue one = builder.createConstInt(1, loc)->result;
            QoreIRValue next_index = builder.createBinaryOp(QoreIROpcode::AddInt, index_val, one, loc)->result;
            QoreIRBasicBlock* cont_exit_block = builder.getBlock();
            {
                auto* br = builder.createBranch(header_block, loc);
                setLoopCheckpointExceptionTarget(br, header_block);
            }

            index_phi->incoming.push_back({init_index, preheader_block});
            index_phi->incoming.push_back({next_index, cont_exit_block});
            index_phi->operands.push_back(init_index);
            index_phi->operands.push_back(next_index);

            builder.setBlock(match_block);
            QoreIRValue match_result;
            if (op->getKind() == QoreStreamingOperatorNode::First) {
                match_result = element_val;
            } else if (op->getKind() == QoreStreamingOperatorNode::Any) {
                match_result = builder.createConstBool(true, loc)->result;
            } else {
                match_result = builder.createConstBool(false, loc)->result;
            }
            builder.createBranch(final_block, loc);

            builder.setBlock(exit_block);
            QoreIRValue exit_result;
            if (op->getKind() == QoreStreamingOperatorNode::First) {
                exit_result = builder.createConstNothing(loc)->result;
            } else if (op->getKind() == QoreStreamingOperatorNode::Any) {
                exit_result = builder.createConstBool(false, loc)->result;
            } else {
                exit_result = builder.createConstBool(true, loc)->result;
            }
            builder.createBranch(final_block, loc);

            builder.setBlock(final_block);
            auto* result_phi = builder.createPhi({{match_result, match_block}, {exit_result, exit_block}}, loc);
            return result_phi->result;
        }

        case QoreStreamingOperatorNode::Count: {
            QoreIRBasicBlock* preheader_block = createBlock("stream.count.preheader");
            QoreIRBasicBlock* header_block = createBlock("stream.count.header");
            QoreIRBasicBlock* body_block = createBlock("stream.count.body");
            QoreIRBasicBlock* inc_block = createBlock("stream.count.inc");
            QoreIRBasicBlock* cont_block = createBlock("stream.count.cont");
            QoreIRBasicBlock* exit_block = createBlock("stream.count.exit");
            if (!preheader_block || !header_block || !body_block || !inc_block || !cont_block || !exit_block) {
                error = "IR builder failed to create blocks for streaming count";
                return QoreIRValue();
            }
            header_block->is_loop_header = true;

            QoreIRValue zero = builder.createConstInt(0, loc)->result;
            QoreIRValue is_null = builder.createBinaryOp(QoreIROpcode::EqInt, iter_val, zero, loc)->result;
            QoreIRBasicBlock* iter_check_block = builder.getBlock();
            builder.createBranchIf(is_null, exit_block, preheader_block, loc);

            builder.setBlock(preheader_block);
            QoreIRValue one = builder.createConstInt(1, loc)->result;
            {
                auto* br = builder.createBranch(header_block, loc);
                setLoopCheckpointExceptionTarget(br, header_block);
            }

            builder.setBlock(header_block);
            auto* index_phi = builder.createPhi({}, loc, QoreIRPhiValueKind::NativeInt);
            auto* count_phi = builder.createPhi({}, loc, QoreIRPhiValueKind::NativeInt);
            QoreIRValue index_val = index_phi->result;
            QoreIRValue count_val = count_phi->result;
            auto* next_inst = builder.createIteratorNext(iter_val, exit_block, body_block, loc);
            QoreIRValue element_val = next_inst->result;

            builder.setBlock(body_block);
            QoreIRValue pred_bool = lower_predicate(element_val, index_val);
            if (!pred_bool.isValid()) {
                return QoreIRValue();
            }
            QoreIRBasicBlock* predicate_block = builder.getBlock();
            builder.createBranchIf(pred_bool, inc_block, cont_block, loc);

            builder.setBlock(inc_block);
            QoreIRValue inc_count = builder.createBinaryOp(QoreIROpcode::AddInt, count_val, one, loc)->result;
            builder.createBranch(cont_block, loc);

            builder.setBlock(cont_block);
            auto* next_count_phi = builder.createPhi({{inc_count, inc_block}, {count_val, predicate_block}}, loc,
                QoreIRPhiValueKind::NativeInt);
            QoreIRValue next_index = builder.createBinaryOp(QoreIROpcode::AddInt, index_val, one, loc)->result;
            QoreIRBasicBlock* cont_exit_block = builder.getBlock();
            {
                auto* br = builder.createBranch(header_block, loc);
                setLoopCheckpointExceptionTarget(br, header_block);
            }

            index_phi->incoming.push_back({zero, preheader_block});
            index_phi->incoming.push_back({next_index, cont_exit_block});
            index_phi->operands.push_back(zero);
            index_phi->operands.push_back(next_index);

            count_phi->incoming.push_back({zero, preheader_block});
            count_phi->incoming.push_back({next_count_phi->result, cont_exit_block});
            count_phi->operands.push_back(zero);
            count_phi->operands.push_back(next_count_phi->result);

            builder.setBlock(exit_block);
            auto* result_phi = builder.createPhi({{zero, iter_check_block}, {count_val, header_block}}, loc,
                QoreIRPhiValueKind::NativeInt);
            return result_phi->result;
        }

        case QoreStreamingOperatorNode::Take:
        case QoreStreamingOperatorNode::Drop:
        case QoreStreamingOperatorNode::TakeWhile:
        case QoreStreamingOperatorNode::TakeUntil:
            break;
    }

    QoreIRValue limit_val;
    QoreIRBasicBlock* limit_ok_block = nullptr;
    if (op->getKind() == QoreStreamingOperatorNode::Take || op->getKind() == QoreStreamingOperatorNode::Drop) {
        limit_val = lowerExpression(op->getPredicate(), error);
        if (!limit_val.isValid()) {
            return QoreIRValue();
        }
        QoreIRBasicBlock* limit_err_block = createBlock("stream.limit.error");
        limit_ok_block = createBlock("stream.limit.ok");
        if (!limit_err_block || !limit_ok_block) {
            error = "IR builder failed to create blocks for streaming limit check";
            return QoreIRValue();
        }
        QoreIRValue zero = builder.createConstInt(0, loc)->result;
        QoreIRValue is_negative = builder.createBinaryOp(QoreIROpcode::LtInt, limit_val, zero, loc)->result;
        builder.createBranchIf(is_negative, limit_err_block, limit_ok_block, loc);

        builder.setBlock(limit_err_block);
        emit_negative_limit_throw(op->getKind() == QoreStreamingOperatorNode::Take ? "take" : "drop");

        builder.setBlock(limit_ok_block);
    }

    QoreIRBasicBlock* preheader_block = createBlock("stream.list.preheader");
    QoreIRBasicBlock* header_block = createBlock("stream.list.header");
    QoreIRBasicBlock* next_block = createBlock("stream.list.next");
    QoreIRBasicBlock* body_block = createBlock("stream.list.body");
    QoreIRBasicBlock* append_block = createBlock("stream.list.append");
    QoreIRBasicBlock* cont_block = createBlock("stream.list.cont");
    QoreIRBasicBlock* nothing_block = createBlock("stream.list.nothing");
    QoreIRBasicBlock* exit_block = createBlock("stream.list.exit");
    QoreIRBasicBlock* final_block = createBlock("stream.list.final");
    if (!preheader_block || !header_block || !next_block || !body_block || !append_block || !cont_block
            || !nothing_block || !exit_block || !final_block) {
        error = "IR builder failed to create blocks for streaming list operator";
        return QoreIRValue();
    }
    header_block->is_loop_header = true;

    QoreIRValue zero = builder.createConstInt(0, loc)->result;
    QoreIRValue is_null = builder.createBinaryOp(QoreIROpcode::EqInt, iter_val, zero, loc)->result;
    builder.createBranchIf(is_null, nothing_block, preheader_block, loc);

    builder.setBlock(preheader_block);
    const QoreTypeInfo* elem_type = QoreIterateOperatorNode::getElementTypeInfo(op->getSource(),
        getExprTypeInfo(op->getSource()));
    QoreIRValue result_list = builder.createEmptyList(loc, elem_type)->result;
    {
        auto* br = builder.createBranch(header_block, loc);
        setLoopCheckpointExceptionTarget(br, header_block);
    }

    builder.setBlock(header_block);
    auto* index_phi = builder.createPhi({}, loc, QoreIRPhiValueKind::NativeInt);
    QoreIRValue index_val = index_phi->result;
    QoreIRBasicBlock* take_done_block = nullptr;
    if (op->getKind() == QoreStreamingOperatorNode::Take) {
        take_done_block = createBlock("stream.take.done");
        if (!take_done_block) {
            error = "IR builder failed to create take done block";
            return QoreIRValue();
        }
        QoreIRValue at_limit = builder.createBinaryOp(QoreIROpcode::GeInt, index_val, limit_val, loc)->result;
        builder.createBranchIf(at_limit, take_done_block, next_block, loc);
        builder.setBlock(take_done_block);
        builder.createBranch(exit_block, loc);
    } else {
        builder.createBranch(next_block, loc);
    }

    builder.setBlock(next_block);
    auto* next_inst = builder.createIteratorNext(iter_val, exit_block, body_block, loc);
    QoreIRValue element_val = next_inst->result;

    builder.setBlock(body_block);
    if (op->getKind() == QoreStreamingOperatorNode::Take) {
        builder.createBranch(append_block, loc);
    } else if (op->getKind() == QoreStreamingOperatorNode::Drop) {
        QoreIRValue keep = builder.createBinaryOp(QoreIROpcode::GeInt, index_val, limit_val, loc)->result;
        builder.createBranchIf(keep, append_block, cont_block, loc);
    } else {
        QoreIRValue pred_bool = lower_predicate(element_val, index_val);
        if (!pred_bool.isValid()) {
            return QoreIRValue();
        }
        if (op->getKind() == QoreStreamingOperatorNode::TakeWhile) {
            builder.createBranchIf(pred_bool, append_block, exit_block, loc);
        } else {
            builder.createBranchIf(pred_bool, exit_block, append_block, loc);
        }
    }

    builder.setBlock(append_block);
    builder.createListAppend(result_list, element_val, loc);
    builder.createBranch(cont_block, loc);

    builder.setBlock(cont_block);
    QoreIRValue one = builder.createConstInt(1, loc)->result;
    QoreIRValue next_index = builder.createBinaryOp(QoreIROpcode::AddInt, index_val, one, loc)->result;
    QoreIRBasicBlock* cont_exit_block = builder.getBlock();
    {
        auto* br = builder.createBranch(header_block, loc);
        setLoopCheckpointExceptionTarget(br, header_block);
    }

    index_phi->incoming.push_back({zero, preheader_block});
    index_phi->incoming.push_back({next_index, cont_exit_block});
    index_phi->operands.push_back(zero);
    index_phi->operands.push_back(next_index);

    builder.setBlock(nothing_block);
    QoreIRValue nothing_val = builder.createConstNothing(loc)->result;
    builder.createBranch(final_block, loc);

    builder.setBlock(exit_block);
    builder.createBranch(final_block, loc);

    builder.setBlock(final_block);
    auto* result_phi = builder.createPhi({{result_list, exit_block}, {nothing_val, nothing_block}}, loc);
    return result_phi->result;
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

    QoreIRValue left_value = lowerExpression(and_node->getLeft(), error);
    if (!left_value.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue left_bool = lowerUnaryOpOrInvoke(QoreIROpcode::ToBool, and_node->getLeft(), left_value,
        and_node->loc, error);
    if (!left_bool.isValid()) {
        return QoreIRValue();
    }
    QoreIRBasicBlock* left_block = builder.getBlock();
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
    QoreIRValue right_bool = lowerUnaryOpOrInvoke(QoreIROpcode::ToBool, and_node->getRight(), right_value,
        and_node->loc, error);
    if (!right_bool.isValid()) {
        return QoreIRValue();
    }
    QoreIRBasicBlock* rhs_exit_block = builder.getBlock();
    builder.createBranch(merge_block);

    builder.setBlock(merge_block);
    std::vector<QoreIRPhiIncoming> incoming = {
        {false_value, left_block},
        {right_bool, rhs_exit_block},
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

    QoreIRValue left_value = lowerExpression(or_node->getLeft(), error);
    if (!left_value.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue left_bool = lowerUnaryOpOrInvoke(QoreIROpcode::ToBool, or_node->getLeft(), left_value,
        or_node->loc, error);
    if (!left_bool.isValid()) {
        return QoreIRValue();
    }
    QoreIRBasicBlock* left_block = builder.getBlock();
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
    QoreIRValue right_bool = lowerUnaryOpOrInvoke(QoreIROpcode::ToBool, or_node->getRight(), right_value,
        or_node->loc, error);
    if (!right_bool.isValid()) {
        return QoreIRValue();
    }
    QoreIRBasicBlock* rhs_exit_block = builder.getBlock();
    builder.createBranch(merge_block);

    builder.setBlock(merge_block);
    std::vector<QoreIRPhiIncoming> incoming = {
        {true_value, left_block},
        {right_bool, rhs_exit_block},
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
    QoreIRValue bool_value = lowerUnaryOpOrInvoke(QoreIROpcode::ToBool, not_node->getExp(), value,
        not_node->loc, error);
    if (!bool_value.isValid()) {
        return QoreIRValue();
    }
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

    QoreIRValue left_value = lowerExpression(coalesce->getLeft(), error);
    if (!left_value.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue is_null = builder.createUnaryOp(QoreIROpcode::IsNullOrNothing, left_value)->result;
    QoreIRBasicBlock* left_block = builder.getBlock();

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
    QoreIRBasicBlock* rhs_exit_block = builder.getBlock();
    builder.createBranch(merge_block);

    builder.setBlock(merge_block);
    std::vector<QoreIRPhiIncoming> incoming = {
        {left_value, left_block},
        {right_value, rhs_exit_block},
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

    QoreIRValue left_value = lowerExpression(coalesce->getLeft(), error);
    if (!left_value.isValid()) {
        return QoreIRValue();
    }
    QoreIRValue left_bool = lowerUnaryOpOrInvoke(QoreIROpcode::ToBool, coalesce->getLeft(), left_value,
        coalesce->loc, error);
    if (!left_bool.isValid()) {
        return QoreIRValue();
    }
    QoreIRBasicBlock* left_block = builder.getBlock();

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
    QoreIRBasicBlock* rhs_exit_block = builder.getBlock();
    builder.createBranch(merge_block);

    builder.setBlock(merge_block);
    std::vector<QoreIRPhiIncoming> incoming = {
        {left_value, left_block},
        {right_value, rhs_exit_block},
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
    QoreIRValue cond_bool = lowerUnaryOpOrInvoke(QoreIROpcode::ToBool, ternary->get(0), cond_value,
        ternary->loc, error);
    if (!cond_bool.isValid()) {
        return QoreIRValue();
    }

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
    QoreIRBasicBlock* left_exit_block = builder.getBlock();
    builder.createBranch(merge_block);

    builder.setBlock(right_block);
    QoreIRValue right_value = lowerExpression(ternary->get(2), error);
    if (!right_value.isValid()) {
        return QoreIRValue();
    }
    QoreIRBasicBlock* right_exit_block = builder.getBlock();
    builder.createBranch(merge_block);

    builder.setBlock(merge_block);
    std::vector<QoreIRPhiIncoming> incoming = {
        {left_value, left_exit_block},
        {right_value, right_exit_block},
    };
    return builder.createPhi(incoming)->result;
}
