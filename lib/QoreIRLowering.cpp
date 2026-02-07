/* -*- indent-tabs-mode: nil -*- */
/*
    QoreIRLowering.cpp

    Qore Programming Language
*/


#include <qore/intern/QoreIRLowering.h>

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
#include <qore/intern/ConstantList.h>
#include <qore/intern/ContextrefNode.h>
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
#include <qore/intern/SummarizeStatement.h>
#include <qore/intern/QoreOperatorNode.h>
#include <qore/intern/ObjectMethodReferenceNode.h>
#include <qore/intern/QoreTypeInfo.h>
#include <qore/intern/QoreClassIntern.h>

static bool isTerminatorOpcode(QoreIROpcode op) {
    switch (op) {
        case QoreIROpcode::Br:
        case QoreIROpcode::BrIf:
        case QoreIROpcode::Invoke:
        case QoreIROpcode::IteratorNext:
        case QoreIROpcode::Return:
        case QoreIROpcode::ReturnNothing:
        case QoreIROpcode::Throw:
        case QoreIROpcode::Rethrow:
        case QoreIROpcode::ThreadExit:
            return true;
        default:
            return false;
    }
}

static bool blockHasTerminator(const QoreIRBasicBlock* block) {
    if (!block || block->instructions.empty()) {
        return false;
    }
    return isTerminatorOpcode(block->instructions.back()->opcode);
}
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

QoreIRValue QoreIRLowering::lowerConditionValue(const QoreValue& cond, std::string& error) {
    QoreIRValue lowered = lowerExpression(cond, error);
    if (!lowered.isValid()) {
        return QoreIRValue();
    }
    if (cond.isBool()) {
        return lowered;
    }
    QoreParseAnalysis analysis;
    if (getAnalysis(cond, analysis)) {
        if (analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
            && analysis.hasFlag(QoreParseAnalysis::NeverNothing)
            && QoreTypeInfo::isType(analysis.known_type, NT_BOOLEAN)) {
            return lowered;
        }
    }
    return lowerUnaryOpOrInvoke(QoreIROpcode::ToBool, cond, lowered, nullptr, error);
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
                    if (auto* call = dynamic_cast<const FunctionCallNode*>(node)) {
                        if (!lowerCallArgs(call->getParseArgs(), call->getArgs(), operands, error)) {
                            return false;
                        }
                        loc = call->loc;
                        invoked = true;
                    } else if (auto* call = dynamic_cast<const CallReferenceCallNode*>(node)) {
                        QoreIRValue callee = lowerExpression(call->getExp(), error);
                        if (!callee.isValid()) {
                            return false;
                        }
                        operands.push_back(callee);
                        if (!lowerCallArgs(call->getParseArgs(), call->getArgs(), operands, error)) {
                            return false;
                        }
                        loc = call->loc;
                        invoked = true;
                    } else if (auto* call = dynamic_cast<const SelfFunctionCallNode*>(node)) {
                        if (!lowerCallArgs(call->getParseArgs(), call->getArgs(), operands, error)) {
                            return false;
                        }
                        loc = call->loc;
                        invoked = true;
                    } else if (auto* call = dynamic_cast<const StaticMethodCallNode*>(node)) {
                        if (!lowerCallArgs(call->getParseArgs(), call->getArgs(), operands, error)) {
                            return false;
                        }
                        loc = call->loc;
                        invoked = true;
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
                    } else if (auto* remove = dynamic_cast<const QoreRemoveOperatorNode*>(node)) {
                        QoreIRValue operand = lowerExpression(remove->getExp(), error);
                        if (!operand.isValid()) {
                            return false;
                        }
                        operands.push_back(operand);
                        loc = remove->loc;
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
                    } else if (auto* dot = dynamic_cast<const QoreDotEvalOperatorNode*>(node)) {
                        QoreIRValue operand = lowerExpression(dot->getExpression(), error);
                        if (!operand.isValid()) {
                            return false;
                        }
                        operands.push_back(operand);
                        loc = dot->loc;
                        invoked = true;
                    } else {
                        if (auto* ternary = dynamic_cast<const QoreQuestionMarkOperatorNode*>(node)) {
                            QoreIRValue cond = lowerExpression(ternary->get(0), error);
                            if (!cond.isValid()) {
                                return false;
                            }
                            QoreIRValue left = lowerExpression(ternary->get(1), error);
                            if (!left.isValid()) {
                                return false;
                            }
                            QoreIRValue right = lowerExpression(ternary->get(2), error);
                            if (!right.isValid()) {
                                return false;
                            }
                            operands.push_back(cond);
                            operands.push_back(left);
                            operands.push_back(right);
                            loc = ternary->loc;
                            invoked = true;
                        } else if (auto* binary = dynamic_cast<const QoreBinaryOperatorNode<>*>(node)) {
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
                        } else if (auto* unary = dynamic_cast<const QoreSingleExpressionOperatorNode<>*>(node)) {
                            QoreIRValue value = lowerExpression(unary->getExp(), error);
                            if (!value.isValid()) {
                                return false;
                            }
                            operands.push_back(value);
                            loc = unary->loc;
                            invoked = true;
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
        // Emit ScopeExit for all active scopes before returning
        emitScopeExits(0, false);
        if (!expr || expr.isNothing()) {
            builder.createReturnNothing();
            return true;
        }
        QoreIRValue lowered = lowerExpression(expr, error);
        if (!lowered.isValid()) {
            return false;
        }
        builder.createReturn(lowered);
        return true;
    }
    if (auto* if_stmt = dynamic_cast<const IfStatement*>(stmt)) {
        QoreIRValue cond = lowerConditionValue(if_stmt->getCond(), error);
        if (!cond.isValid()) {
            return false;
        }
        QoreIRBasicBlock* then_block = createBlock("if.then");
        QoreIRBasicBlock* merge_block = createBlock("if.merge");
        QoreIRBasicBlock* else_block = if_stmt->getElseCode() ? createBlock("if.else") : merge_block;
        if (!then_block || !merge_block || !else_block) {
            error = "IR builder failed to create blocks for if";
            return false;
        }
        builder.createBranchIf(cond, then_block, else_block);

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
        flow_stack.push_back({exit_block, cond_block, false, scope_stack.size()});
        if (!lowerStatementBlock(do_stmt->getCode(), error)) {
            flow_stack.pop_back();
            return false;
        }
        if (!blockHasTerminator(builder.getBlock())) {
            builder.createBranch(cond_block);
        }
        flow_stack.pop_back();

        builder.setBlock(cond_block);
        QoreIRValue cond = lowerConditionValue(do_stmt->getCond(), error);
        if (!cond.isValid()) {
            return false;
        }
        builder.createBranchIf(cond, body_block, exit_block);

        builder.setBlock(exit_block);
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
        if (!blockHasTerminator(builder.getBlock())) {
            builder.createBranch(cond_block);
        }

        builder.setBlock(cond_block);
        QoreIRValue cond = lowerConditionValue(while_stmt->getCond(), error);
        if (!cond.isValid()) {
            return false;
        }
        builder.createBranchIf(cond, body_block, exit_block);

        builder.setBlock(body_block);
        flow_stack.push_back({exit_block, cond_block, false, scope_stack.size()});
        if (!lowerStatementBlock(while_stmt->getCode(), error)) {
            flow_stack.pop_back();
            return false;
        }
        if (!blockHasTerminator(builder.getBlock())) {
            builder.createBranch(cond_block);
        }
        flow_stack.pop_back();

        builder.setBlock(exit_block);
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
        if (!blockHasTerminator(builder.getBlock())) {
            builder.createBranch(cond_block);
        }

        builder.setBlock(cond_block);
        QoreValue cond_expr = for_stmt->getCond();
        QoreIRValue cond_value;
        if (!cond_expr || cond_expr.isNothing()) {
            cond_value = builder.createConstBool(true)->result;
        } else {
            cond_value = lowerConditionValue(cond_expr, error);
            if (!cond_value.isValid()) {
                return false;
            }
        }
        builder.createBranchIf(cond_value, body_block, exit_block);

        builder.setBlock(body_block);
        flow_stack.push_back({exit_block, iter_block, false, scope_stack.size()});
        if (!lowerStatementBlock(for_stmt->getCode(), error)) {
            flow_stack.pop_back();
            return false;
        }
        if (!blockHasTerminator(builder.getBlock())) {
            builder.createBranch(iter_block);
        }
        flow_stack.pop_back();

        builder.setBlock(iter_block);
        QoreValue iter_expr = for_stmt->getIterator();
        if (iter_expr && !iter_expr.isNothing()) {
            QoreIRValue lowered = lowerExpression(iter_expr, error);
            if (!lowered.isValid()) {
                return false;
            }
        }
        if (!blockHasTerminator(builder.getBlock())) {
            builder.createBranch(cond_block);
        }

        builder.setBlock(exit_block);
        return true;
    }
    if (auto* foreach_stmt = dynamic_cast<const ForEachStatement*>(stmt)) {
        // For reference iteration (foreach x in \list), fall back to AST execution
        // due to complex semantics of modifying the list during iteration
        if (foreach_stmt->isRef()) {
            auto* inst = builder.createForeach(foreach_stmt, stmt->loc);
            if (!exception_stack.empty()) {
                inst->exception_target = exception_stack.back();
            }
            return true;
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

        // Create the iterator
        auto* iter_inst = builder.createIteratorCreate(list_val, foreach_stmt->getIteratorFunc(), stmt->loc);
        QoreIRValue iter_val = iter_inst->result;

        // Create basic blocks for the loop structure AFTER evaluating the list
        // expression and creating the iterator
        QoreIRBasicBlock* header_block = createBlock("foreach.header");
        QoreIRBasicBlock* body_block = createBlock("foreach.body");
        QoreIRBasicBlock* exit_block = createBlock("foreach.exit");
        if (!header_block || !body_block || !exit_block) {
            error = "IR builder failed to create blocks for foreach";
            return false;
        }
        // Mark header block as loop header for OSR detection
        header_block->is_loop_header = true;

        // Branch to header
        builder.createBranch(header_block, stmt->loc);

        // Header block: check for next value and branch
        builder.setBlock(header_block);
        auto* next_inst = builder.createIteratorNext(iter_val, exit_block, body_block, stmt->loc);
        QoreIRValue value_val = next_inst->result;

        // Body block: assign value to loop variable and execute body
        builder.setBlock(body_block);

        // Assign the value to the loop variable
        QoreValue var_expr = foreach_stmt->getVar();
        if (var_expr && !var_expr.isNothing()) {
            // Use StoreLValue for the assignment
            auto* store_inst = builder.createStoreLValue(var_expr, value_val, stmt->loc);
            if (!exception_stack.empty()) {
                store_inst->exception_target = exception_stack.back();
            }
        }

        // Lower the loop body with proper break/continue targets
        flow_stack.push_back({exit_block, header_block, false, scope_stack.size()});
        StatementBlock* body = foreach_stmt->getCode();
        if (body) {
            if (!lowerStatementBlock(body, error)) {
                flow_stack.pop_back();
                return false;
            }
        }
        flow_stack.pop_back();

        // Branch back to header
        if (!blockHasTerminator(builder.getBlock())) {
            builder.createBranch(header_block, stmt->loc);
        }

        // Exit block
        builder.setBlock(exit_block);
        return true;
    }
    if (auto* on_block_exit_stmt = dynamic_cast<const OnBlockExitStatement*>(stmt)) {
        builder.createOnBlockExit(on_block_exit_stmt, stmt->loc);
        return true;
    }
    if (auto* debug_stmt = dynamic_cast<const DebugStatement*>(stmt)) {
        auto* inst = builder.createDebug(debug_stmt, stmt->loc);
        if (!exception_stack.empty()) {
            inst->exception_target = exception_stack.back();
        }
        return true;
    }
    if (auto* assert_stmt = dynamic_cast<const AssertStatement*>(stmt)) {
        auto* inst = builder.createAssert(assert_stmt, stmt->loc);
        if (!exception_stack.empty()) {
            inst->exception_target = exception_stack.back();
        }
        return true;
    }
    if (auto* summarize_stmt = dynamic_cast<const SummarizeStatement*>(stmt)) {
        auto* inst = builder.createSummarize(summarize_stmt, stmt->loc);
        if (!exception_stack.empty()) {
            inst->exception_target = exception_stack.back();
        }
        return true;
    }
    if (auto* context_stmt = dynamic_cast<const ContextStatement*>(stmt)) {
        auto* inst = builder.createContext(context_stmt, stmt->loc);
        if (!exception_stack.empty()) {
            inst->exception_target = exception_stack.back();
        }
        return true;
    }
    if (auto* thread_exit_stmt = dynamic_cast<const ThreadExitStatement*>(stmt)) {
        builder.createThreadExit(thread_exit_stmt->loc);
        return true;
    }
    if (auto* break_stmt = dynamic_cast<const BreakStatement*>(stmt)) {
        QoreIRBasicBlock* target = nullptr;
        size_t target_scope_depth = 0;
        for (auto it = flow_stack.rbegin(); it != flow_stack.rend(); ++it) {
            if (it->break_target) {
                target = it->break_target;
                target_scope_depth = it->scope_stack_depth;
                break;
            }
        }
        if (!target) {
            error = "break statement has no active target for IR lowering";
            return false;
        }
        // Emit ScopeExit for scopes entered since the loop started
        emitScopeExits(target_scope_depth, false);
        builder.createBranch(target);
        return true;
    }
    if (auto* cont_stmt = dynamic_cast<const ContinueStatement*>(stmt)) {
        QoreIRBasicBlock* target = nullptr;
        size_t target_scope_depth = 0;
        for (auto it = flow_stack.rbegin(); it != flow_stack.rend(); ++it) {
            if (it->continue_target) {
                target = it->continue_target;
                target_scope_depth = it->scope_stack_depth;
                break;
            }
        }
        if (!target) {
            error = "continue statement has no active target for IR lowering";
            return false;
        }
        // Emit ScopeExit for scopes entered since the loop started
        emitScopeExits(target_scope_depth, false);
        builder.createBranch(target);
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
            flow_stack.push_back({end_block, nullptr, true, scope_stack.size()});
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
            const QoreStringNode* str = node->val.get<const QoreStringNode>();
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
            flow_stack.push_back({end_block, nullptr, true, scope_stack.size()});
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
                QoreIRValue case_val = lowerExpression(node->val, error);
                if (!case_val.isValid()) {
                    return false;
                }
                QoreValue cmp_expr(new QoreLogicalAbsoluteEqualsOperatorNode(node->loc, switch_expr.refSelf(),
                    node->val.refSelf()));
                ValueHolder cmp_holder(cmp_expr, nullptr);
                match_value = lowerBinaryOpOrInvoke(QoreIROpcode::EqHard, cmp_expr, switch_val, case_val, node->loc,
                    error);
                if (!match_value.isValid()) {
                    return false;
                }
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

        flow_stack.push_back({end_block, nullptr, true, scope_stack.size()});
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
        if (try_stmt->getTryBlock() && !lowerStatementBlock(try_stmt->getTryBlock(), error)) {
            exception_stack.pop_back();
            guard_exception_target_override = prev_guard_override;
            return false;
        }
        exception_stack.pop_back();
        guard_exception_target_override = prev_guard_override;
        if (!blockHasTerminator(builder.getBlock())) {
            builder.createBranch(merge_block);
        }

        builder.setBlock(catch_block);
        builder.createLandingPad(stmt->loc);
        QoreIRInstruction* catch_inst = builder.createCatchException(stmt->loc);
        if (LocalVar* catch_var = try_stmt->getCatchVar()) {
            maybeInsertNotNothingGuard(catch_inst->result, nullptr, catch_inst->loc, catch_var->getTypeInfo());
            builder.createStoreLocal(catch_var, catch_inst->result, stmt->loc);
            if (parse_context) {
                parse_context->markLocalAssignment(catch_var, true, catch_var->getTypeInfo());
            }
        }
        if (try_stmt->getCatchBlock() && !lowerStatementBlock(try_stmt->getCatchBlock(), error)) {
            return false;
        }
        if (!blockHasTerminator(builder.getBlock())) {
            builder.createBranch(merge_block);
        }

        builder.setBlock(merge_block);
        return true;
    }
    if (auto* throw_stmt = dynamic_cast<const ThrowStatement*>(stmt)) {
        QoreIRValue value = lowerExpression(throw_stmt->getArgs(), error);
        if (!value.isValid()) {
            return false;
        }
        // Emit ScopeExit for all active scopes with is_error=true before throwing
        // This ensures on_error handlers are executed
        emitScopeExits(0, true);
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
            // Emit ScopeExit for all active scopes with is_error=true before throwing
            emitScopeExits(0, true);
            QoreIRBasicBlock* handler = exception_stack.empty() ? nullptr : exception_stack.back();
            builder.createThrow(value, handler, stmt->loc);
        } else {
            // Emit ScopeExit for all active scopes with is_error=true before rethrowing
            emitScopeExits(0, true);
            builder.createRethrow(stmt->loc);
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

    // Check if this block has on_exit/on_success/on_error handlers
    bool has_on_block_exit = block->hasOnBlockExit();
    uint32_t scope_id = 0;

    if (has_on_block_exit) {
        // Allocate a unique scope ID and emit ScopeEnter
        scope_id = ++scope_counter;
        scope_stack.push_back(scope_id);
        builder.createScopeEnter(scope_id);
    }

    // Get the block's local variables for cleanup
    const LVList* lvars = block->getLVList();

    bool terminated = false;
    for (auto it = block->getStatements().begin(); it != block->getStatements().end(); ++it) {
        if (!*it) {
            continue;
        }
        if (!lowerStatement(*it, error)) {
            if (has_on_block_exit) {
                scope_stack.pop_back();
            }
            return false;
        }
        if (blockHasTerminator(builder.getBlock())) {
            terminated = true;
            break;
        }
    }

    // Emit ScopeExit if we have on_exit handlers and didn't terminate early
    // (early termination like return/break/continue will handle ScopeExit themselves)
    if (has_on_block_exit) {
        if (!terminated) {
            builder.createScopeExit(scope_id, false);
        }
        scope_stack.pop_back();
    }

    // Emit UninstantiateLocal for block-scoped local variables in reverse order
    // (reverse order ensures destructors are called in LIFO order like in AST mode)
    if (lvars && !terminated) {
        for (int i = static_cast<int>(lvars->size()) - 1; i >= 0; --i) {
            builder.createUninstantiateLocal(lvars->lv[i], block->loc);
        }
    }

    return true;
}

void QoreIRLowering::emitScopeExits(size_t target_depth, bool is_error) {
    // Emit ScopeExit instructions from innermost to outermost scope
    // until we reach the target depth
    for (size_t i = scope_stack.size(); i > target_depth; --i) {
        uint32_t scope_id = scope_stack[i - 1];
        builder.createScopeExit(scope_id, is_error);
    }
}

static bool isIntConstant(const QoreValue& value) {
    return value.isInt();
}

static bool isFloatConstant(const QoreValue& value) {
    return value.isFloat();
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

QoreIRValue QoreIRLowering::lowerExpression(const QoreValue& expr, std::string& error) {
    QoreIRValue constant = lowerConstant(expr, error);
    if (constant.isValid()) {
        return constant;
    }
    if (!error.empty()) {
        return QoreIRValue();
    }
    QoreIRValue list = lowerParseList(expr, error);
    if (list.isValid() || !error.empty()) {
        return list;
    }
    QoreIRValue hash = lowerParseHash(expr, error);
    if (hash.isValid() || !error.empty()) {
        return hash;
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
    result = lowerBinaryNot(expr, error);
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
    result = lowerPop(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerShift(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerPush(expr, error);
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
    result = lowerDelete(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerKeys(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerRegexExtract(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerRegexNMatch(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerRegexMatch(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerRegexSubst(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerInstanceOf(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerTrim(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerChomp(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerTransliteration(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerBackground(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerListAssignment(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerExists(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerElements(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerDotEval(expr, error);
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
    result = lowerSelect(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerMapSelect(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerHashMap(expr, error);
    if (result.isValid() || !error.empty()) {
        return result;
    }
    result = lowerHashMapSelect(expr, error);
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
    const AbstractQoreNode* node = expr.getInternalNode();
    if (auto* impl_arg = dynamic_cast<const QoreImplicitArgumentNode*>(node)) {
        int offset = impl_arg->getOffset();
        if (offset == -1) {
            // $argv - entire argument list
            return builder.createLoadImplicitArgv(impl_arg->loc)->result;
        }
        // $1, $2, etc. - specific argument (offset is 0 for $1, 1 for $2, etc.)
        return builder.createLoadImplicitArg(offset, impl_arg->loc)->result;
    }
    if (auto* impl_elem = dynamic_cast<const QoreImplicitElementNode*>(node)) {
        return builder.createLoadImplicitElement(impl_elem->loc)->result;
    }
    // Delegate unsupported expression types to AST evaluation via ExprOp.
    // The interpreter's evalExpr() default case calls evalExprNode() for any opcode,
    // so we use QoreIROpcode::Call as a generic expression evaluation opcode.
    if (auto* closure = dynamic_cast<const QoreClosureParseNode*>(node)) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, closure->loc, error);
    }
    if (auto* parse_ref = dynamic_cast<const ParseReferenceNode*>(node)) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, parse_ref->loc, error);
    }
    if (dynamic_cast<const AbstractCallReferenceNode*>(node)) {
        std::vector<QoreIRValue> operands;
        auto* parse_node = dynamic_cast<const ParseNode*>(node);
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands,
            parse_node ? parse_node->loc : nullptr, error);
    }
    // Context references are a legacy feature - delegate to AST evaluation
    if (auto* ctx_ref = dynamic_cast<const ContextrefNode*>(node)) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, ctx_ref->loc, error);
    }
    if (auto* complex_ctx_ref = dynamic_cast<const ComplexContextrefNode*>(node)) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, complex_ctx_ref->loc, error);
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
    if (auto* backquote = dynamic_cast<const BackquoteNode*>(node)) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, backquote->loc, error);
    }
    if (auto* find_node = dynamic_cast<const FindNode*>(node)) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, find_node->loc, error);
    }
    if (auto* rt_const = dynamic_cast<const RuntimeConstantRefNode*>(node)) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, rt_const->loc, error);
    }
    if (auto* new_hd = dynamic_cast<const NewHashDeclNode*>(node)) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, new_hd->loc, error);
    }
    if (auto* new_ch = dynamic_cast<const NewComplexHashNode*>(node)) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, new_ch->loc, error);
    }
    if (auto* new_cl = dynamic_cast<const NewComplexListNode*>(node)) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, new_cl->loc, error);
    }
    // ParseNewComplexTypeNode and ParseNoEvalNode are parse-time-only nodes whose evalImpl()
    // asserts false — they cannot be delegated to AST evaluation
    if (dynamic_cast<const ParseNewComplexTypeNode*>(node) || dynamic_cast<const ParseNoEvalNode*>(node)) {
        error = "parse-only node not supported for IR lowering";
        return QoreIRValue();
    }
    if (auto* new_obj = dynamic_cast<const NewObjectCallNode*>(node)) {
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {}, normal_block, handler, nullptr);
            inst->invoke_opcode = QoreIROpcode::NewObject;
            builder.setBlock(normal_block);
            return inst->result;
        }
        return builder.createNewObject(new_obj->getClass(), new_obj->getVariant(),
                new_obj->getArgs(), expr, nullptr)->result;
    }
    if (auto* scoped_obj = dynamic_cast<const ScopedObjectCallNode*>(node)) {
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {}, normal_block, handler, scoped_obj->loc);
            inst->invoke_opcode = QoreIROpcode::NewObject;
            builder.setBlock(normal_block);
            return inst->result;
        }
        return builder.createNewObject(scoped_obj->oc, scoped_obj->getVariant(),
                scoped_obj->getArgs(), expr, scoped_obj->loc)->result;
    }
    // QoreObject values (e.g., Type constants like AutoListOrNothingType)
    // These are already evaluated to objects at parse time, delegate to AST evaluation
    if (dynamic_cast<const QoreObject*>(node)) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, nullptr, error);
    }
    // QoreNumberNode arbitrary-precision number values
    if (dynamic_cast<const QoreNumberNode*>(node)) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, nullptr, error);
    }
    // BinaryNode literals (e.g., <abcd>)
    if (dynamic_cast<const BinaryNode*>(node)) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, nullptr, error);
    }
    // Object method references (e.g., \methodName())
    if (auto* mref = dynamic_cast<const AbstractParseObjectMethodReferenceNode*>(node)) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, mref->loc, error);
    }
    // Non-value hash/list nodes (e.g., const hashes containing runtime objects)
    if (dynamic_cast<const QoreHashNode*>(node) || dynamic_cast<const QoreListNode*>(node)) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, nullptr, error);
    }
    error = std::string("unsupported expression node for IR lowering: ") + node->getTypeName();
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
                QoreIRValue lowered = lowerConstant(entry, error);
                if (!lowered.isValid()) {
                    return QoreIRValue();
                }
                values.push_back(lowered);
            }
            return builder.createMakeList(values, nullptr)->result;
        }
    }
    if (expr.getType() == NT_HASH && expr.isValue()) {
        const QoreHashNode* hash = expr.get<const QoreHashNode>();
        if (hash) {
            std::vector<QoreIRValue> values;
            ConstHashIterator it(hash);
            while (it.next()) {
                const char* key = it.getKey();
                values.push_back(builder.createConstString(key ? key : "")->result);
                QoreValue entry = it.get();
                QoreIRValue lowered = lowerConstant(entry, error);
                if (!lowered.isValid()) {
                    return QoreIRValue();
                }
                values.push_back(lowered);
            }
            return builder.createMakeHash(values, nullptr)->result;
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
        return builder.createConstDate(micros, is_relative)->result;
    }
    if (expr.isNull()) {
        return builder.createConstNull()->result;
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
    // with implicit constructor call. eval() on this node both constructs the object
    // and assigns it to the variable.
    if (dynamic_cast<const VarRefNewObjectNode*>(node)) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, var->loc, error);
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
            result = builder.createLoadLocal(var->ref.id, var->loc)->result;
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
    // NOTE: no guard here — consuming operators (lowerRange, storeVarRef, guardVarLValue, etc.)
    // emit their own guards with proper exception scope context
    return result;
}

bool QoreIRLowering::storeVarRef(const VarRefNode* var, QoreIRValue value, std::string& error,
        const char* context, const QoreValue* expr, const QoreProgramLocation* guard_loc) {
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
            {
                auto* store_inst = builder.createStoreLocal(var->ref.id, value, var->loc);
                if (!exception_stack.empty()) {
                    store_inst->exception_target = exception_stack.back();
                }
            }
            if (parse_context) {
                parse_context->markLocalAssignment(var->ref.id, true, target_type);
            }
            return true;
        case VT_CLOSURE:
        case VT_LOCAL_TS:
            if (!var->ref.id) {
                error = std::string("unresolved closure variable reference in IR lowering (") + context + ")";
                return false;
            }
            {
                auto* store_inst = builder.createStoreClosure(var->ref.id, value, var->loc);
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
                auto* store_inst = builder.createStoreGlobal(var->ref.var, value, var->loc);
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
                auto* store_inst = builder.createStoreThreadLocal(var->ref.var, value, var->loc);
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
    if (!var || var->getType() != VT_LOCAL) {
        return nullptr;
    }
    return var->ref.id;
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

// Returns true if the given opcode is guaranteed to produce a non-NOTHING result
static bool opcodeNeverReturnsNothing(QoreIROpcode op) {
    switch (op) {
        // Typed arithmetic always produces int/float values
        case QoreIROpcode::AddInt:
        case QoreIROpcode::AddFloat:
        case QoreIROpcode::SubInt:
        case QoreIROpcode::SubFloat:
        case QoreIROpcode::MulInt:
        case QoreIROpcode::MulFloat:
        case QoreIROpcode::DivInt:
        case QoreIROpcode::DivFloat:
        case QoreIROpcode::ModInt:
        case QoreIROpcode::AndInt:
        case QoreIROpcode::OrInt:
        case QoreIROpcode::XorInt:
        case QoreIROpcode::ShlInt:
        case QoreIROpcode::ShrInt:
        // Typed unary always produces values
        case QoreIROpcode::UnaryMinusInt:
        case QoreIROpcode::UnaryMinusFloat:
        // All comparisons always produce bool/int values
        case QoreIROpcode::EqInt:
        case QoreIROpcode::EqFloat:
        case QoreIROpcode::EqAny:
        case QoreIROpcode::NeInt:
        case QoreIROpcode::NeFloat:
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
        // Boolean operations always produce bool
        case QoreIROpcode::ToBool:
        case QoreIROpcode::Not:
        case QoreIROpcode::IsNullOrNothing:
        case QoreIROpcode::InstanceOfBool:
        case QoreIROpcode::ExistsBool:
        case QoreIROpcode::RegexMatchBool:
        case QoreIROpcode::RegexNMatchBool:
        // Typed results from method/member evaluation
        case QoreIROpcode::DotEvalInt:
        case QoreIROpcode::DotEvalFloat:
        case QoreIROpcode::DotEvalString:
        case QoreIROpcode::DotEvalDate:
        case QoreIROpcode::DotEvalList:
        case QoreIROpcode::DotEvalHash:
        case QoreIROpcode::DotEvalObject:
        // Integer-typed operations
        case QoreIROpcode::ElementsInt:
        case QoreIROpcode::BackgroundInt:
        // String-typed operations
        case QoreIROpcode::RegexSubstString:
        case QoreIROpcode::TrimString:
        case QoreIROpcode::ChompString:
        case QoreIROpcode::TransliterateString:
            return true;
        default:
            return false;
    }
}

bool QoreIRLowering::needsNotNothingGuard(const QoreValue* expr, const QoreTypeInfo* target_type,
        bool allow_maybe_nothing) const {
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
    const QoreTypeInfo* type = getGuaranteedTypeForValue(expr, target_type);
    if (type && QoreTypeInfo::parseReturns(type, NT_NOTHING) == QTI_NOT_EQUAL) {
        if (expr && expr->hasNode()) {
            if (LocalVar* local = getLocalVarFromValue(*expr)) {
                if (allow_maybe_nothing) {
                    return false;
                }
                return parse_context ? parse_context->needsGuardForLocal(local) : true;
            }
            QoreParseAnalysis analysis;
            bool got_analysis = false;
            try {
                got_analysis = getAnalysis(*expr, analysis);
            } catch (...) {
                return true;
            }
            if (got_analysis && analysis.hasFlag(QoreParseAnalysis::NeverNothing)) {
                return false;
            }
        }
        if (LocalVar* local = expr ? getLocalVarFromValue(*expr) : nullptr) {
            return parse_context ? parse_context->needsGuardForLocal(local) : true;
        }
        return true;
    }
    if (!expr) {
        return false;
    }
    return needsNotNothingGuard(*expr);
}

void QoreIRLowering::maybeInsertNotNothingGuard(QoreIRValue value, const QoreValue* expr,
        const QoreProgramLocation* loc, const QoreTypeInfo* target_type, bool allow_maybe_nothing) {
    if (!value.isValid() || !needsNotNothingGuard(expr, target_type, allow_maybe_nothing)) {
        return;
    }
    // Check the last instruction in the current block
    QoreIRBasicBlock* current_block = builder.getBlock();
    if (current_block && !current_block->instructions.empty()) {
        const auto& last = current_block->instructions.back();
        // Skip duplicate guard on the same value
        if (last->opcode == QoreIROpcode::GuardNotNothing
                && !last->operands.empty() && last->operands[0].id == value.id) {
            return;
        }
        // Skip guard if the value was produced by an opcode that never returns NOTHING
        if (last->result.id == value.id && opcodeNeverReturnsNothing(last->opcode)) {
            return;
        }
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
    if (parse_context) {
        if (LocalVar* local = getLocalVarFromValue(expr)) {
            return parse_context->needsGuardForLocal(local);
        }
    }
    QoreParseAnalysis analysis;
    bool got_analysis = false;
    try {
        got_analysis = getAnalysis(expr, analysis);
    } catch (...) {
        return true;
    }
    if (got_analysis) {
        if (analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)
                && !analysis.hasFlag(QoreParseAnalysis::NeverNothing)) {
            if (analysis.known_type && QoreTypeInfo::parseReturns(analysis.known_type, NT_NOTHING) == QTI_NOT_EQUAL) {
                return true;
            }
        }
    }
    if (parse_context) {
        if (LocalVar* local = getLocalVarFromValue(expr)) {
            return parse_context->needsGuardForLocal(local);
        }
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
    auto* assign = dynamic_cast<const QoreAssignmentOperatorNode*>(node);
    if (!assign) {
        auto* weak = dynamic_cast<const QoreWeakAssignmentOperatorNode*>(node);
        if (!weak) {
            return QoreIRValue();
        }
        assign = weak;
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
        if (!storeVarRef(left_var, right, error, "assignment", &right_expr)) {
            return QoreIRValue();
        }
    } else if (assign->getLeft().hasNode()) {
        if (!guardLValueBase(assign->getLeft(), error)) {
            return QoreIRValue();
        }
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {right}, normal_block, handler, assign->loc);
            inst->invoke_opcode = QoreIROpcode::StoreLValue;
            builder.setBlock(normal_block);
        } else {
            builder.createStoreLValue(assign->getLeft(), right, assign->loc);
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
    // Object += hash requires in-place member merge. Treat object-typed variables like
    // complex lvalues and use AddAssignLValue (which uses QorePlusEqualsOperatorNode
    // with proper lvalue semantics). This is because object + hash = hash (not object),
    // so the load-compute-store decomposition doesn't work for objects.
    if (left_var && left_var->getTypeInfo()
            && QoreTypeInfo::getUniqueReturnClass(left_var->getTypeInfo())) {
        left_var = nullptr;  // Force lvalue path
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
            error = "unsupported lvalue for plus-equals IR lowering";
            return QoreIRValue();
        }
        if (!guardLValueBase(op->getLeft(), error)) {
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
            error = "unsupported lvalue for minus-equals IR lowering";
            return QoreIRValue();
        }
        if (!guardLValueBase(op->getLeft(), error)) {
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
            error = "unsupported lvalue for multiply-equals IR lowering";
            return QoreIRValue();
        }
        if (!guardLValueBase(op->getLeft(), error)) {
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
            error = "unsupported lvalue for divide-equals IR lowering";
            return QoreIRValue();
        }
        if (!guardLValueBase(op->getLeft(), error)) {
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
            error = "unsupported lvalue for modulo-equals IR lowering";
            return QoreIRValue();
        }
        if (!guardLValueBase(op->getLeft(), error)) {
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
            error = "unsupported lvalue for and-equals IR lowering";
            return QoreIRValue();
        }
        if (!guardLValueBase(op->getLeft(), error)) {
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
            error = "unsupported lvalue for or-equals IR lowering";
            return QoreIRValue();
        }
        if (!guardLValueBase(op->getLeft(), error)) {
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
            error = "unsupported lvalue for xor-equals IR lowering";
            return QoreIRValue();
        }
        if (!guardLValueBase(op->getLeft(), error)) {
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
    if (dynamic_cast<const QoreIntPreIncrementOperatorNode*>(node)) {
        // int-specific node uses the same lowering path
    }
    QoreValue lvexp = op->getExp();
    if (!lvexp.hasNode()) {
        error = "unsupported lvalue for pre-increment IR lowering";
        return QoreIRValue();
    }
    // Range lvalue (e.g., ++list[0..2]) - delegate entire expression to AST
    if (isRangeLValue(lvexp)) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, op->loc, error);
    }
    if (!guardLValueBase(lvexp, error)) {
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
    // Range lvalue (e.g., list[0..2]++) - delegate entire expression to AST
    if (isRangeLValue(lvexp)) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, base_op->loc, error);
    }
    if (!guardLValueBase(lvexp, error)) {
        return QoreIRValue();
    }
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
    if (dynamic_cast<const QoreIntPreDecrementOperatorNode*>(node)) {
        // int-specific node uses the same lowering path
    }
    QoreValue lvexp = op->getExp();
    if (!lvexp.hasNode()) {
        error = "unsupported lvalue for pre-decrement IR lowering";
        return QoreIRValue();
    }
    // Range lvalue (e.g., --list[0..2]) - delegate entire expression to AST
    if (isRangeLValue(lvexp)) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, op->loc, error);
    }
    if (!guardLValueBase(lvexp, error)) {
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
    // Range lvalue (e.g., list[0..2]--) - delegate entire expression to AST
    if (isRangeLValue(lvexp)) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, base_op->loc, error);
    }
    if (!guardLValueBase(lvexp, error)) {
        return QoreIRValue();
    }
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
    QoreIROpcode op = selectNumericOpcode(left_expr, right_expr,
        QoreIROpcode::AddInt, QoreIROpcode::AddFloat, QoreIROpcode::AddAny);
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
        QoreIROpcode::SubInt, QoreIROpcode::SubFloat, QoreIROpcode::SubAny);
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
        QoreIROpcode::MulInt, QoreIROpcode::MulFloat, QoreIROpcode::MulAny);
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
        QoreIROpcode::DivInt, QoreIROpcode::DivFloat, QoreIROpcode::DivAny);
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
    std::vector<QoreIRValue> operands;
    return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, op->loc, error);
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
    if (!guardLValueBase(lvalue, error)) {
        return QoreIRValue();
    }
    // Use expression evaluation (Call) instead of LoadLValue so that both
    // single-key access (h{"x"}) and multi-key slicing (h{("x","z")}) are
    // handled correctly; LValueHelper only supports single string keys.
    std::vector<QoreIRValue> operands;
    return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, op->loc, error);
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
    if (!guardLValueBase(lvalue, error)) {
        return QoreIRValue();
    }
    QoreIRValue right = lowerExpression(op->getRight(), error);
    if (!right.isValid()) {
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
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, op->loc, error);
    }
    if (!guardLValueBase(lvalue, error)) {
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
    if (!exception_stack.empty()) {
        QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
        if (!normal_block) {
            error = "IR builder failed to create invoke continuation block";
            return QoreIRValue();
        }
        QoreIRBasicBlock* handler = exception_stack.back();
        auto* inst = builder.createInvoke(expr, {offset, length, replacement}, normal_block, handler, op->loc);
        inst->invoke_opcode = QoreIROpcode::SpliceLValue;
        builder.setBlock(normal_block);
        return inst->result;
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
    std::vector<QoreIRValue> operands;
    return lowerExprOpOrInvoke(QoreIROpcode::InstanceOfBool, expr, operands, op->loc, error);
}

QoreIRValue QoreIRLowering::lowerTrim(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreTrimOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
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
    std::vector<QoreIRValue> operands;
    return lowerExprOpOrInvoke(QoreIROpcode::BackgroundInt, expr, operands, op->loc, error);
}

QoreIRValue QoreIRLowering::lowerListAssignment(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreListAssignmentOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    std::vector<QoreIRValue> operands;
    return lowerExprOpOrInvoke(QoreIROpcode::ListAssignAny, expr, operands, op->loc, error);
}

QoreIRValue QoreIRLowering::lowerRegexSubst(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreRegexSubstOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
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
    std::vector<QoreIRValue> operands;
    operands.reserve(keys.size() * 2);
    for (size_t i = 0; i < keys.size(); ++i) {
        QoreIRValue key = lowerExpression(keys[i], error);
        if (!key.isValid()) {
            return QoreIRValue();
        }
        QoreIRValue value = lowerExpression(values_vec[i], error);
        if (!value.isValid()) {
            return QoreIRValue();
        }
        operands.push_back(key);
        operands.push_back(value);
    }
    return builder.createMakeHash(operands, hash->loc)->result;
}

QoreIRValue QoreIRLowering::lowerParseList(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* list = dynamic_cast<const QoreParseListNode*>(node);
    if (!list) {
        return QoreIRValue();
    }
    std::vector<QoreIRValue> values;
    values.reserve(list->size());
    for (size_t i = 0; i < list->size(); ++i) {
        QoreIRValue value = lowerExpression(list->get(i), error);
        if (!value.isValid()) {
            return QoreIRValue();
        }
        values.push_back(value);
    }
    return builder.createMakeList(values, list->loc)->result;
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

QoreIRValue QoreIRLowering::lowerDotEval(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreDotEvalOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
    }
    QoreIRValue operand = lowerExpression(op->getExpression(), error);
    if (!operand.isValid()) {
        return QoreIRValue();
    }
    std::vector<QoreIRValue> operands{operand};
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
        const std::vector<QoreIRValue>& operands, const QoreProgramLocation* loc, std::string& error) {
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
        builder.setBlock(normal_block);
        result = inst->result;
    } else {
        result = builder.createExprOp(op, expr, operands, loc)->result;
    }
    if (!opcodeNeverReturnsNothing(op)) {
        maybeInsertNotNothingGuard(result, &expr, loc, nullptr);
    }
    return result;
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
    if (!opcodeNeverReturnsNothing(op)) {
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
    if (!opcodeNeverReturnsNothing(op)) {
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
    QoreIRValue value = lowerExpression(cast_node->getExp(), error);
    if (!value.isValid()) {
        return QoreIRValue();
    }
    QoreIROpcode opcode = QoreIROpcode::CastAny;
    if (cast) {
        if (dynamic_cast<const QoreComplexListCastOperatorNode*>(node)) {
            opcode = QoreIROpcode::CastList;
        } else if (dynamic_cast<const QoreComplexHashCastOperatorNode*>(node)
                || dynamic_cast<const QoreHashDeclCastOperatorNode*>(node)) {
            opcode = QoreIROpcode::CastHash;
        } else if (dynamic_cast<const QoreEnumCastOperatorNode*>(node)) {
            opcode = QoreIROpcode::CastEnum;
        } else if (dynamic_cast<const QoreClassCastOperatorNode*>(node)) {
            opcode = QoreIROpcode::CastObject;
        }
    }
    std::vector<QoreIRValue> operands;
    operands.push_back(value);
    return lowerExprOpOrInvoke(opcode, expr, operands, cast_node->loc, error);
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
    return lowerExprOpOrInvoke(QoreIROpcode::CallIndirect, expr, operands, call->loc, error);
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

    // Check for devirtualization opportunities
    // We can bypass virtual dispatch if:
    // 1. The method is resolved at parse time, AND
    // 2. The class is final (cannot have subclasses, so no override possible)
    const QoreMethod* method = call->getMethod();
    const QoreClass* qc = call->getClass();
    if (method && qc && qc->isFinal()) {
        // Safe devirtualization - the class is final, so no subclass can override
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
            auto* inst = builder.createInvokeMethodDirect(method, qc, operands,
                    normal_block, handler, call->loc);
            builder.setBlock(normal_block);
            result = inst->result;
        } else {
            result = builder.createCallMethodDirect(method, qc, operands, call->loc)->result;
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
    std::vector<QoreIRValue> operands;
    if (!lowerCallArgs(call->getParseArgs(), call->getArgs(), operands, error)) {
        return QoreIRValue();
    }
    return lowerExprOpOrInvoke(QoreIROpcode::CallStatic, expr, operands, call->loc, error);
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
                    result_type = arg0.getTypeInfo();
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

    // Try to detect optimizable pattern first
    const QoreTypeInfo* result_type = nullptr;
    // Get list type info for fallback type detection
    const QoreTypeInfo* list_type = foldl->getRight().getTypeInfo();
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

QoreIRValue QoreIRLowering::lowerFoldr(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* foldr = dynamic_cast<const QoreFoldrOperatorNode*>(node);
    if (!foldr) {
        return QoreIRValue();
    }

    // Delegate entire operation to AST evaluation
    // AST handles implicit argument context ($1, $2, $#) correctly
    std::vector<QoreIRValue> operands;
    return lowerExprOpOrInvoke(QoreIROpcode::FoldrAny, expr, operands, foldr->loc, error);
}

QoreIRValue QoreIRLowering::lowerMap(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* map = dynamic_cast<const QoreMapOperatorNode*>(node);
    if (!map) {
        return QoreIRValue();
    }

    // Try pattern analysis for optimized opcodes
    const QoreTypeInfo* result_type = nullptr;
    QoreValue constant_val;
    QoreIROpcode opt_opcode = analyzeMapPattern(map->getLeft(), result_type, constant_val);

    // For optimized patterns, check if we can fuse with a select operand
    if (opt_opcode != QoreIROpcode::MapAny) {
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

    // Delegate entire operation to AST evaluation
    // AST handles implicit argument context ($1, $#) correctly
    std::vector<QoreIRValue> operands;
    return lowerExprOpOrInvoke(QoreIROpcode::MapSelectAny, expr, operands, map_select->loc, error);
}

QoreIRValue QoreIRLowering::lowerHashMap(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* map = dynamic_cast<const QoreHashMapOperatorNode*>(node);
    if (!map) {
        return QoreIRValue();
    }

    // Delegate entire operation to AST evaluation
    // AST handles implicit argument context ($1, $#) correctly
    std::vector<QoreIRValue> operands;
    return lowerExprOpOrInvoke(QoreIROpcode::HashMapAny, expr, operands, map->loc, error);
}

QoreIRValue QoreIRLowering::lowerHashMapSelect(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* map_select = dynamic_cast<const QoreHashMapSelectOperatorNode*>(node);
    if (!map_select) {
        return QoreIRValue();
    }

    // Delegate entire operation to AST evaluation
    // AST handles implicit argument context ($1, $#) correctly
    std::vector<QoreIRValue> operands;
    return lowerExprOpOrInvoke(QoreIROpcode::HashMapSelectAny, expr, operands, map_select->loc, error);
}

QoreIRValue QoreIRLowering::lowerMapNative(const QoreMapOperatorNode* map, const QoreValue& expr,
        std::string& error) {
    if (!ensureBuilderContext(error)) {
        return QoreIRValue();
    }

    // Evaluate the input list and create the iterator BEFORE creating loop blocks,
    // so that any blocks created during expression evaluation (e.g., invoke.cont
    // blocks from guarded calls in try-catch) appear before the loop header in the
    // block list.

    // Create empty result list
    QoreIRValue result_list = builder.createEmptyList(map->loc)->result;

    // Evaluate the input list (right operand of map)
    QoreIRValue input_list = lowerExpression(map->getRight(), error);
    if (!input_list.isValid()) {
        return QoreIRValue();
    }

    // Create iterator from input list
    auto* iter_inst = builder.createIteratorCreate(input_list, nullptr, map->loc);
    QoreIRValue iter_val = iter_inst->result;

    // Initial index value (0)
    QoreIRValue init_index = builder.createConstInt(0, map->loc)->result;

    // Capture entry block AFTER list evaluation and iterator creation, since
    // expression evaluation may change the current block (e.g., invoke.cont blocks
    // in try-catch). This block will branch to the header, so it must be the
    // PHI predecessor.
    QoreIRBasicBlock* entry_block = builder.getBlock();

    // Create basic blocks for the loop structure AFTER evaluating the input
    // expression and creating the iterator
    QoreIRBasicBlock* header_block = createBlock("map.header");
    QoreIRBasicBlock* body_block = createBlock("map.body");
    QoreIRBasicBlock* exit_block = createBlock("map.exit");
    if (!header_block || !body_block || !exit_block) {
        error = "IR builder failed to create blocks for map";
        return QoreIRValue();
    }
    header_block->is_loop_header = true;

    // Branch to header
    builder.createBranch(header_block, map->loc);

    // Header block: create phi for index and check for next value
    builder.setBlock(header_block);

    // Create phi for index - will be completed after body block
    auto* index_phi = builder.createPhi({}, map->loc);
    QoreIRValue index_val = index_phi->result;

    // Get next element from iterator (branches to exit if done, body if has element)
    auto* next_inst = builder.createIteratorNext(iter_val, exit_block, body_block, map->loc);
    QoreIRValue element_val = next_inst->result;

    // Body block: set up context, evaluate expression, append result
    builder.setBlock(body_block);

    // Push implicit element ($#) - save old value for restoration
    QoreIRValue old_element = builder.createPushImplicitElement(index_val, map->loc)->result;

    // Push implicit argument ($1 = current element) - save old context
    QoreIRValue old_argv = builder.createPushImplicitArg(element_val, map->loc)->result;

    // Lower the map expression - now $1 and $# are available in thread-local context
    QoreIRValue expr_result = lowerExpression(map->getLeft(), error);

    // Always restore contexts, even if expression lowering failed
    // Restore in reverse order: pop $1, then $#
    builder.createPopImplicitArg(old_argv, map->loc);
    builder.createPopImplicitElement(old_element, map->loc);

    if (!expr_result.isValid()) {
        return QoreIRValue();
    }

    // Append result to output list
    builder.createListAppend(result_list, expr_result, map->loc);

    // Increment index for next iteration
    QoreIRValue one = builder.createConstInt(1, map->loc)->result;
    QoreIRValue next_index = builder.createBinaryOp(QoreIROpcode::AddInt, index_val, one, map->loc)->result;

    // Record the body exit block before branching
    QoreIRBasicBlock* body_exit_block = builder.getBlock();

    // Branch back to header
    builder.createBranch(header_block, map->loc);

    // Complete the phi node with incoming values
    index_phi->incoming.push_back({init_index, entry_block});
    index_phi->incoming.push_back({next_index, body_exit_block});
    index_phi->operands.push_back(init_index);
    index_phi->operands.push_back(next_index);

    // Exit block: return result list
    builder.setBlock(exit_block);

    return result_list;
}

QoreIRValue QoreIRLowering::lowerSelectNative(const QoreSelectOperatorNode* select, const QoreValue& expr,
        std::string& error) {
    if (!ensureBuilderContext(error)) {
        return QoreIRValue();
    }

    // Evaluate the input list and create the iterator BEFORE creating loop blocks,
    // so that any blocks created during expression evaluation (e.g., invoke.cont
    // blocks from guarded calls in try-catch) appear before the loop header in the
    // block list.

    // Create empty result list
    QoreIRValue result_list = builder.createEmptyList(select->loc)->result;

    // Evaluate the input list (left operand of select)
    QoreIRValue input_list = lowerExpression(select->getLeft(), error);
    if (!input_list.isValid()) {
        return QoreIRValue();
    }

    // Create iterator from input list
    auto* iter_inst = builder.createIteratorCreate(input_list, nullptr, select->loc);
    QoreIRValue iter_val = iter_inst->result;

    // Initial index value (0)
    QoreIRValue init_index = builder.createConstInt(0, select->loc)->result;

    // Capture entry block AFTER list evaluation and iterator creation, since
    // expression evaluation may change the current block (e.g., invoke.cont blocks
    // in try-catch). This block will branch to the header, so it must be the
    // PHI predecessor.
    QoreIRBasicBlock* entry_block = builder.getBlock();

    // Create basic blocks for the loop structure AFTER evaluating the input
    // expression and creating the iterator
    QoreIRBasicBlock* header_block = createBlock("select.header");
    QoreIRBasicBlock* body_block = createBlock("select.body");
    QoreIRBasicBlock* append_block = createBlock("select.append");
    QoreIRBasicBlock* cont_block = createBlock("select.cont");
    QoreIRBasicBlock* exit_block = createBlock("select.exit");
    if (!header_block || !body_block || !append_block || !cont_block || !exit_block) {
        error = "IR builder failed to create blocks for select";
        return QoreIRValue();
    }
    header_block->is_loop_header = true;

    // Branch to header
    builder.createBranch(header_block, select->loc);

    // Header block: create phi for index and check for next value
    builder.setBlock(header_block);

    // Create phi for index
    auto* index_phi = builder.createPhi({}, select->loc);
    QoreIRValue index_val = index_phi->result;

    // Get next element from iterator
    auto* next_inst = builder.createIteratorNext(iter_val, exit_block, body_block, select->loc);
    QoreIRValue element_val = next_inst->result;

    // Body block: set up context, evaluate predicate
    builder.setBlock(body_block);

    // Push implicit contexts
    QoreIRValue old_element = builder.createPushImplicitElement(index_val, select->loc)->result;
    QoreIRValue old_argv = builder.createPushImplicitArg(element_val, select->loc)->result;

    // Lower the predicate expression (right operand of select)
    QoreIRValue predicate_result = lowerExpression(select->getRight(), error);

    // Restore contexts
    builder.createPopImplicitArg(old_argv, select->loc);
    builder.createPopImplicitElement(old_element, select->loc);

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

    // Increment index
    QoreIRValue one = builder.createConstInt(1, select->loc)->result;
    QoreIRValue next_index = builder.createBinaryOp(QoreIROpcode::AddInt, index_val, one, select->loc)->result;

    QoreIRBasicBlock* cont_exit_block = builder.getBlock();

    // Branch back to header
    builder.createBranch(header_block, select->loc);

    // Complete the phi node
    index_phi->incoming.push_back({init_index, entry_block});
    index_phi->incoming.push_back({next_index, cont_exit_block});
    index_phi->operands.push_back(init_index);
    index_phi->operands.push_back(next_index);

    // Exit block
    builder.setBlock(exit_block);

    return result_list;
}

QoreIRValue QoreIRLowering::lowerFoldlNative(const QoreFoldlOperatorNode* foldl, const QoreValue& expr,
        std::string& error) {
    if (!ensureBuilderContext(error)) {
        return QoreIRValue();
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

    // Create a 2-element list for argv: [accumulator, element]
    // $1 = accumulator (index 0), $2 = element (index 1)
    QoreIRValue argv_list = builder.createEmptyList(foldl->loc)->result;
    builder.createListAppend(argv_list, accum_val, foldl->loc);
    builder.createListAppend(argv_list, element_val, foldl->loc);

    // Set the list directly as implicit args using SetImplicitArgv
    // This allows LoadImplicitArg(0) to return $1 and LoadImplicitArg(1) to return $2
    QoreIRValue old_argv = builder.createSetImplicitArgv(argv_list, foldl->loc)->result;

    // Lower the fold expression - now $1 and $2 are available
    QoreIRValue fold_result = lowerExpression(foldl->getLeft(), error);

    // Restore context
    builder.createPopImplicitArg(old_argv, foldl->loc);

    if (!fold_result.isValid()) {
        return QoreIRValue();
    }

    // Record body exit block
    QoreIRBasicBlock* body_exit_block = builder.getBlock();

    // Branch back to header with new accumulator
    builder.createBranch(header_block, foldl->loc);

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
