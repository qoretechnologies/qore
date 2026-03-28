/* -*- indent-tabs-mode: nil -*- */
/*
    QoreIRLowering.cpp

    Qore Programming Language
*/


#include "qore/intern/QoreJITIncludes.h"
#include <qore/intern/QoreIRLowering.h>
#include <qore/intern/QoreOpcodeRegistry.h>

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
#include <qore/intern/QoreIRVerifier.h>
#include <qore/intern/QoreIRExprRegistry.h>
#include <qore/intern/QoreHashNodeIntern.h>
#include <qore/intern/qore_list_private.h>

#include <atomic>

// Forward declaration from Function.cpp - collects all locals from a statement tree
extern void collectAllStatementLocals(const StatementBlock* block, std::vector<LocalVar*>& locals);

static bool isTerminatorOpcode(QoreIROpcode op) {
    switch (op) {
        case QoreIROpcode::Br:
        case QoreIROpcode::BrIf:
        case QoreIROpcode::BranchIfLtLocalInt:
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
    }
    return val.getTypeInfo();
}

QoreIRLowering::QoreIRLowering(QoreIRBuilder& n_builder, QoreParseContext* n_parse_context)
        : builder(n_builder), parse_context(n_parse_context) {
}

void QoreIRLowering::setParseContext(QoreParseContext* n_parse_context) {
    parse_context = n_parse_context;
}

QoreIRValue QoreIRLowering::lowerConditionValue(const QoreValue& cond, std::string& error) {
    // BrIf calls getAsBool() on its operand, so ToBool is redundant here.
    // Skip the ToBool emission to reduce instruction count.
    return lowerExpression(cond, error);
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
    if (left_var->getType() != VT_LOCAL || !left_var->ref.id) {
        return false;
    }
    if (right_var->getType() != VT_LOCAL || !right_var->ref.id) {
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
                    if (auto* cast = dynamic_cast<const QoreCastOperatorNode*>(node)) {
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
            // CF_SKIP_LVARS: pre-instantiated locals mechanism handles local cleanup
            // at function exit. The break/continue paths need explicit UninstantiateLocal
            // because execution continues after the loop, but on return the function
            // exits immediately and the caller handles cleanup.
            // Also handles RefForeach cleanup (record + finalize without fill remaining).
            if (!emitBlockCleanups(0, error, false, CF_SKIP_LVARS)) {
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
        // Emit block cleanups for all active scopes (fires on_exit handlers).
        // Same as ReturnNothing — CF_SKIP_LVARS, and handles RefForeach cleanup.
        if (!emitBlockCleanups(0, error, false, CF_SKIP_LVARS)) {
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
        if (!blockHasTerminator(builder.getBlock())) {
            builder.createBranch(cond_block);
        }

        builder.setBlock(cond_block);
        // Try fused BranchIfLtLocalInt for int local < int local conditions
        if (!tryEmitFusedBranchIfLtLocalInt(while_stmt->getCond(), body_block, exit_block)) {
            QoreIRValue cond = lowerConditionValue(while_stmt->getCond(), error);
            if (!cond.isValid()) {
                return false;
            }
            builder.createBranchIf(cond, body_block, exit_block);
        }

        builder.setBlock(body_block);
        flow_stack.push_back({exit_block, cond_block, false, catch_cleanup_depth, cleanup_stack.size(), QoreIRValue()});
        if (!lowerStatementBlock(while_stmt->getCode(), error)) {
            flow_stack.pop_back();
            return false;
        }
        if (!blockHasTerminator(builder.getBlock())) {
            builder.createBranch(cond_block);
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
        // Try fused BranchIfLtLocalInt for int local < int local conditions
        if (cond_expr && !cond_expr.isNothing()
                && tryEmitFusedBranchIfLtLocalInt(cond_expr, body_block, exit_block)) {
            // Fused condition+branch emitted
        } else {
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
        }

        builder.setBlock(body_block);
        flow_stack.push_back({exit_block, iter_block, false, catch_cleanup_depth, cleanup_stack.size(), QoreIRValue()});
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
            builder.createBranch(header_block, stmt->loc);

            // Header: PHI for index, compare with size
            builder.setBlock(header_block);
            auto* index_phi = builder.createPhi({}, stmt->loc);
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
                if (!lowerStatementBlock(body, error)) {
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
            builder.createBranch(header_block, stmt->loc);

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

            // Catch: landing pad, cleanup without write-back, rethrow
            builder.setBlock(catch_block);
            builder.createLandingPad(try_scope_depth, try_scope_id, stmt->loc);
            builder.createRefForeachCleanup(state, stmt->loc);
            {
                auto* rethrow_inst = builder.createRethrow(nullptr, stmt->loc);
                rethrow_inst->synthetic = true;
            }

            // Move merge block to end (after invoke.cont blocks from body lowering)
            builder.getFunction()->moveBlockToEnd(merge_block);
            builder.setBlock(merge_block);
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
        builder.createBranch(header_block, stmt->loc);

        // Header block: PHI for index, check for next value and branch
        builder.setBlock(header_block);

        // Create PHI for iteration index ($#) - will be completed after body
        auto* index_phi = builder.createPhi({}, stmt->loc);
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
            // Use StoreLValue for the assignment
            auto* store_inst = builder.createStoreLValue(var_expr, value_val, stmt->loc);
            if (!exception_stack.empty()) {
                store_inst->exception_target = exception_stack.back();
            }
        }

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
            if (!lowerStatementBlock(body, error)) {
                flow_stack.pop_back();
                return false;
            }
        }
        flow_stack.pop_back();

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
        builder.createBranch(header_block, stmt->loc);

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
                // Single message expression
                msg = lowerExpression(message, error);
                if (!msg.isValid()) {
                    return false;
                }
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
        builder.createBranch(target);
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
                // Use SwitchCaseMatch which calls CaseNode::matches() —
                // this unwraps TAG_ENUM from both sides before isEqualHard(),
                // matching the AST switch statement behavior
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
        if (LocalVar* catch_var = try_stmt->getCatchVar()) {
            maybeInsertNotNothingGuard(catch_inst->result, nullptr, catch_inst->loc, catch_var->getTypeInfo());
            builder.createStoreLocal(catch_var, catch_inst->result, stmt->loc);
            if (parse_context) {
                parse_context->markLocalAssignment(catch_var, true, catch_var->getTypeInfo());
            }
        }
        ++catch_cleanup_depth;
        if (try_stmt->getCatchBlock() && !lowerStatementBlock(try_stmt->getCatchBlock(), error)) {
            --catch_cleanup_depth;
            return false;
        }
        --catch_cleanup_depth;
        if (!blockHasTerminator(builder.getBlock())) {
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

    // Push handler stack to track which handlers are registered in this block
    size_t block_handler_start = block_handlers.size();
    handler_stack.push(block_handler_start);

    // Get the block's local variables for cleanup
    const LVList* lvars = block->getLVList();

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

    bool terminated = false;
    for (auto it = block->getStatements().begin(); it != block->getStatements().end(); ++it) {
        if (!*it) {
            continue;
        }
        if (!lowerStatement(*it, error)) {
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
            terminated = true;
            break;
        }
    }

    // Pop handler stack
    handler_stack.pop();

    // At fall-through (normal exit), inline handlers
    if (has_on_block_exit) {
        if (!terminated) {
            // Phase 1: Inline handlers at fall-through exit
            if (!lowerHandlersAtExit(false, error, block_handler_start)) {
                return false;
            }
            // Phase 2a: Emit ScopeExit for fall-through path (inline_lowered=true means handlers already inlined)
            builder.createScopeExit(scope_id, false, nullptr, /*inline_lowered=*/true);
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
        // This prevents handler accumulation and keeps CFGs manageable
        int compiled_count = compileBlockHandlerIRs(block_level_handlers, builder.getFunction(), error);
        if (compiled_count < 0 && !error.empty()) {
            // Handler compilation failure is non-fatal for block-level handlers
            // Log but continue - handler will use AST fallback
            if (getenv("QORE_DEBUG_HANDLERS")) {
                fprintf(stderr, "Block-level handler IR compilation warning: %s\n", error.c_str());
            }
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
            builder.createUninstantiateLocal(lvars->lv[i], block->loc);
        }
    }
    if (lvars) {
        cleanup_stack.pop_back();
    }

    return true;
}

bool QoreIRLowering::emitBlockCleanups(size_t target_depth, std::string& error, bool is_error, unsigned flags) {
    // Emit interleaved cleanup instructions from innermost to outermost block until
    // we reach the target depth.  Handles: ScopeExit (on_exit handlers), Lvars
    // (block-scoped locals), RefForeachRecord (pop $#, load var, record), and
    // RefForeach (finalize/write-back).
    for (size_t i = cleanup_stack.size(); i > target_depth; --i) {
        const BlockCleanupEntry& entry = cleanup_stack[i - 1];
        switch (entry.type) {
            case BlockCleanupEntry::Scope: {
                // Phase 1: Inline handlers on break/continue/return cleanup
                if (!lowerHandlersAtExit(is_error, error, entry.handler_start)) {
                    return false;
                }
                // Phase 2a: Pop scope_stack without re-executing handlers (inline_lowered=true)
                builder.createScopeExit(entry.scope_id, is_error, entry.loc, /*inline_lowered=*/true);
                break;
            }
            case BlockCleanupEntry::Lvars:
                if (!(flags & CF_SKIP_LVARS)) {
                    assert(entry.lvars);
                    for (int j = static_cast<int>(entry.lvars->size()) - 1; j >= 0; --j) {
                        builder.createUninstantiateLocal(entry.lvars->lv[j], entry.loc);
                    }
                }
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
    if (ntype == NT_STRING || ntype == NT_INT || ntype == NT_FLOAT || ntype == NT_BOOLEAN
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
    auto handler_func = std::make_unique<QoreIRFunction>("handler");

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
    // This prevents pathological CFGs that occur when many handlers are compiled together
    int compiled_count = 0;

    if (!parent_func) {
        error = "no parent function available for handler compilation";
        return -1;
    }

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
        auto handler_func = std::make_unique<QoreIRFunction>("handler");

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

        // Lower the handler body into the handler IR function
        std::string handler_error;
        if (!handler_lowering.lowerStatementBlock(handler.code, handler_error)) {
            // Log failure but continue (non-fatal) - handler will use AST fallback
            if (!error.empty()) {
                error += "; ";
            }
            error += "handler body lowering failed: " + handler_error;
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
                !isTerminatorOpcode(final_block->instructions.back()->opcode)) {
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

    return compiled_count;
}

int QoreIRLowering::compileAllHandlerIRs(std::string& error) {
    // Phase B2: Handler IR compilation with parent slot inheritance
    // QoreIRVerifier has been updated to handle pre-seeded parent slots
    // Handler functions can now be compiled with parent scope access
    int compiled_count = 0;
    QoreIRFunction* parent_func = builder.getFunction();

    if (!parent_func) {
        error = "no parent function available for handler compilation";
        return 0;
    }

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
        auto handler_func = std::make_unique<QoreIRFunction>("handler");

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

        // Lower the handler body into the handler IR function
        std::string handler_error;
        if (!handler_lowering.lowerStatementBlock(handler.code, handler_error)) {
            // Log failure but continue (non-fatal) - handler will use AST fallback
            if (!error.empty()) {
                error += "; ";
            }
            error += "handler body lowering failed: " + handler_error;
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
                !isTerminatorOpcode(final_block->instructions.back()->opcode)) {
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

    return compiled_count;
}

bool QoreIRLowering::lowerHandlersAtExit(bool is_error, std::string& error, size_t start_index) {
    // Lower all applicable handlers for current block in LIFO order
    // Handlers are executed in reverse registration order (innermost to outermost)
    if (block_handlers.empty() || start_index >= block_handlers.size()) {
        return true;
    }

    // Process handlers in reverse order (LIFO), starting from the end
    for (int i = static_cast<int>(block_handlers.size()) - 1; i >= static_cast<int>(start_index); --i) {
        const InlineHandler& handler = block_handlers[i];
        if (!shouldRunHandler(handler, is_error)) {
            continue;
        }

        // Lower the handler code block inline using the current parse context
        // This gives the handler natural access to parent block's scope
        if (!lowerStatementBlock(handler.code, error)) {
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
    key_name = right.get<const QoreStringNode>()->c_str();
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

// Check if a variable is a local (not global) and not captured by a closure.
// Used for fused integer operations (++, --, +=) that can only be applied to lvstack locals.
static bool isLocalNonClosureVar(const VarRefNode* var) {
    return var && var->getType() == VT_LOCAL && var->ref.id && !var->ref.id->closureUse();
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
    // Dispatch through registry of 80 expression handlers
    for (size_t i = 0; i < QORE_IR_EXPR_REGISTRY_SIZE; ++i) {
        const QoreIRExprHandlerInfo& info = QORE_IR_EXPR_REGISTRY[i];
        QoreIRExprCtx ctx{*this, expr, error};
        QoreIRValue result = info.handler(ctx);
        if (result.isValid() || !error.empty()) {
            return result;
        }
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
    // Delegate unsupported expression types to AST evaluation via ExprOp.
    // The interpreter's evalExpr() default case calls evalExprNode() for any opcode,
    // so we use QoreIROpcode::Call as a generic expression evaluation opcode.
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
    if (auto* parse_ref = dynamic_cast<const ParseReferenceNode*>(node)) {
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {}, normal_block, handler, parse_ref->loc);
            inst->invoke_opcode = QoreIROpcode::CreateParseRef;
            builder.setBlock(normal_block);
            return inst->result;
        }
        return builder.createCreateParseRef(parse_ref, expr, parse_ref->loc)->result;
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
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {}, normal_block, handler, new_hd->loc);
            inst->invoke_opcode = QoreIROpcode::NewHashDecl;
            builder.setBlock(normal_block);
            return inst->result;
        }
        return builder.createNewHashDecl(new_hd, expr, new_hd->loc)->result;
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
    // ParseNewComplexTypeNode and ParseNoEvalNode are parse-time-only nodes whose evalImpl()
    // asserts false — they cannot be delegated to AST evaluation
    if (dynamic_cast<const ParseNewComplexTypeNode*>(node) || dynamic_cast<const ParseNoEvalNode*>(node)) {
        error = "parse-only node not supported for IR lowering";
        return QoreIRValue();
    }
    if (auto* new_obj = dynamic_cast<const NewObjectCallNode*>(node)) {
        // NewObject evaluates constructor args through AST at runtime;
        // track as AST-delegated so map body push/pop implicit args for $1/$#
        ++ast_delegate_count;
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
        // NewObject evaluates constructor args through AST at runtime;
        // track as AST-delegated so map body push/pop implicit args for $1/$#
        ++ast_delegate_count;
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
    // Parse-time constant values: use LoadConstant (returns expr.refSelf() at runtime)
    if (dynamic_cast<const QoreObject*>(node)
            || dynamic_cast<const QoreNumberNode*>(node)
            || dynamic_cast<const BinaryNode*>(node)) {
        return builder.createLoadConstant(nullptr, expr, nullptr)->result;
    }
    // Object method references (e.g., \methodName())
    if (auto* mref = dynamic_cast<const AbstractParseObjectMethodReferenceNode*>(node)) {
        if (!exception_stack.empty()) {
            QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
            if (!normal_block) {
                error = "IR builder failed to create invoke continuation block";
                return QoreIRValue();
            }
            QoreIRBasicBlock* handler = exception_stack.back();
            auto* inst = builder.createInvoke(expr, {}, normal_block, handler, mref->loc);
            inst->invoke_opcode = QoreIROpcode::CreateMethodRef;
            builder.setBlock(normal_block);
            return inst->result;
        }
        return builder.createCreateMethodRef(expr, mref->loc)->result;
    }
    // Pre-evaluated hash/list constants (e.g., const hashes/lists containing runtime objects)
    if (dynamic_cast<const QoreHashNode*>(node) || dynamic_cast<const QoreListNode*>(node)) {
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
    return func->createBlock(name);
}

QoreIRValue QoreIRLowering::lowerConstant(const QoreValue& expr, std::string& error) {
    // TAG_ENUM must be checked first: getType() returns the base type (e.g., NT_INT),
    // so base-type-specific paths below would strip enum identity
    if (expr.isEnum()) {
        return builder.createConstEnum(expr.getEnumMember())->result;
    }
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
            // Extract complexTypeInfo from constant list
            const QoreTypeInfo* cti = qore_list_private::get(*list)->complexTypeInfo;
            return builder.createMakeList(values, nullptr, cti)->result;
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
    // with implicit constructor call. Split into construction + assignment.
    if (auto* vrn = dynamic_cast<const VarRefNewObjectNode*>(node)) {
        // Constructor args are evaluated through AST at runtime;
        // track as AST-delegated so map body push/pop implicit args for $1/$#
        ++ast_delegate_count;
        // Check if this is a class constructor (VRN_OBJECT)
        const QoreClass* qc = QoreTypeInfo::getUniqueReturnClass(vrn->getTypeInfo());
        if (qc) {
            // VRN_OBJECT: construct object using NewObject opcode, then store to variable
            QoreIRValue obj_val;
            if (!exception_stack.empty()) {
                QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
                if (!normal_block) {
                    error = "IR builder failed to create invoke continuation block";
                    return QoreIRValue();
                }
                QoreIRBasicBlock* handler = exception_stack.back();
                auto* inst = builder.createInvoke(expr, {}, normal_block, handler, var->loc);
                inst->invoke_opcode = QoreIROpcode::NewObject;
                builder.setBlock(normal_block);
                obj_val = inst->result;
            } else {
                obj_val = builder.createNewObject(qc, vrn->getVariant(),
                    vrn->getArgs(), expr, var->loc)->result;
            }
            // Store the constructed object to the variable
            if (!storeVarRef(var, obj_val, error, "VarRefNewObjectNode", &expr, var->loc)) {
                return QoreIRValue();
            }
            return obj_val;
        }
        // Non-VRN_OBJECT types (hashdecl, complex hash/list): construct + store
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
        const char* context, const QoreValue* expr, const QoreProgramLocation* guard_loc, bool weak) {
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
                auto* store_inst = builder.createStoreLocal(var->ref.id, value, var->loc, weak);
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
    // Force lvalue path for reference-typed variables.
    // Same pattern used by all compound operators (+=, -=, etc.).
    // StoreLValue delegates to AST's LValueHelper which correctly handles both
    // initial reference binding and subsequent write-through assignment.
    if (left_var && left_var->getTypeInfo() && QoreTypeInfo::isReference(left_var->getTypeInfo())) {
        left_var = nullptr;
    }
    if (left_var) {
        if (!storeVarRef(left_var, right, error, "assignment", &right_expr, nullptr, is_weak)) {
            return QoreIRValue();
        }
    } else if (assign->getLeft().hasNode()) {
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
        } else {
            builder.createStoreLValue(assign->getLeft(), right, assign->loc, is_weak);
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
        if (right_var && right_var->getType() == VT_LOCAL && right_var->ref.id) {
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
        // Fast path: constant-key hash subscript compound assignment
        const VarRefNode* container_var = nullptr;
        std::string key_name;
        QoreValue key_expr;
        if (isConstKeyHashSubscript(op->getLeft(), container_var, key_name, key_expr)) {
            QoreIROpcode arith_op = force_int ? QoreIROpcode::AddAssignInt : QoreIROpcode::AddAssignAny;
            return emitHashKeyCompoundOp(container_var, key_name, key_expr,
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
        // Fast path: constant-key hash subscript compound assignment
        const VarRefNode* container_var = nullptr;
        std::string key_name;
        QoreValue key_expr;
        if (isConstKeyHashSubscript(op->getLeft(), container_var, key_name, key_expr)) {
            QoreIROpcode arith_op = force_int ? QoreIROpcode::SubAssignInt : QoreIROpcode::SubAssignAny;
            return emitHashKeyCompoundOp(container_var, key_name, key_expr,
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
        // Fast path: constant-key hash subscript compound assignment
        const VarRefNode* container_var = nullptr;
        std::string key_name;
        QoreValue key_expr;
        if (isConstKeyHashSubscript(op->getLeft(), container_var, key_name, key_expr)) {
            return emitHashKeyCompoundOp(container_var, key_name, key_expr,
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
        // Fast path: constant-key hash subscript compound assignment
        const VarRefNode* container_var = nullptr;
        std::string key_name;
        QoreValue key_expr;
        if (isConstKeyHashSubscript(op->getLeft(), container_var, key_name, key_expr)) {
            return emitHashKeyCompoundOp(container_var, key_name, key_expr,
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
            // Fused path: emit single IncrementLocalInt for VT_LOCAL (but not closure-use vars)
            // Closure-use variables are instantiated on cvstack, not lvstack, so they need the fallback path
            if (isLocalNonClosureVar(var)) {
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
            QoreIRValue one = builder.createConstInt(1, base_op->loc)->result;
            QoreIRValue new_value = lowerBinaryOpOrInvoke(
                QoreIROpcode::AddAssignInt, expr, old_value, one, base_op->loc, error);
            if (!new_value.isValid()) {
                return QoreIRValue();
            }
            if (!storeVarRef(var, new_value, error, "post-increment-int")) {
                return QoreIRValue();
            }
            return old_value;  // post-increment returns old value
        }
    }
    // Range lvalue (e.g., list[0..2]++) - delegate entire expression to AST
    if (isRangeLValue(lvexp)) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, base_op->loc, error);
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
            // Fused path: emit single IncrementLocalInt(delta=-1) for VT_LOCAL (but not closure-use vars)
            // Closure-use variables are instantiated on cvstack, not lvstack, so they need the fallback path
            if (isLocalNonClosureVar(var)) {
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
            QoreIRValue one = builder.createConstInt(1, base_op->loc)->result;
            QoreIRValue new_value = lowerBinaryOpOrInvoke(
                QoreIROpcode::SubAssignInt, expr, old_value, one, base_op->loc, error);
            if (!new_value.isValid()) {
                return QoreIRValue();
            }
            if (!storeVarRef(var, new_value, error, "post-decrement-int")) {
                return QoreIRValue();
            }
            return old_value;  // post-decrement returns old value
        }
    }
    // Range lvalue (e.g., list[0..2]--) - delegate entire expression to AST
    if (isRangeLValue(lvexp)) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, base_op->loc, error);
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

    // For rhs_list_range (e.g., list[1..3]), the RHS contains unevaluated AST
    // nodes with range operators that cannot be pre-evaluated — fall back to AST
    if (op->hasRhsListRange()) {
        std::vector<QoreIRValue> operands;
        return lowerExprOpOrInvoke(QoreIROpcode::Call, expr, operands, op->loc, error);
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

    // Try native hash/object key access: h.key or h{"key"} with constant string key.
    // QoreHashObjectDereferenceOperatorNode handles both hash key access and object
    // member access via {"key"} syntax. qore_rt_hash_key_access handles both types.
    QoreValue right_val = op->getRight();
    if (right_val.hasNode() && right_val.getType() == NT_STRING) {
        const char* key_str = right_val.get<const QoreStringNode>()->c_str();
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

    // Native ListPush path: when lvalue is a simple local variable, emit
    // LoadLocal + ListPush + StoreLocal to avoid AST delegation (which causes
    // reload trackers → extra refcount → copy-on-write → O(n²) in loops)
    if (left_expr.hasNode()) {
        auto* var = dynamic_cast<const VarRefNode*>(left_expr.getInternalNode());
        if (var && var->getType() == VT_LOCAL && var->ref.id) {
            // Lower the value to push first
            QoreIRValue push_val = lowerExpression(op->getRight(), error);
            if (!push_val.isValid()) {
                return QoreIRValue();
            }

            // Load current list from local
            QoreIRValue list_val = builder.createLoadLocal(var->ref.id, op->loc)->result;

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
                builder.setBlock(normal_block);

                // Store result back to local
                auto* store_inst = builder.createStoreLocal(var->ref.id, inst->result, op->loc);
                store_inst->exception_target = exception_stack.back();
                return inst->result;
            }

            // Normal path
            QoreIRValue result = builder.createListPush(list_val, push_val, op->loc)->result;

            // Store result back to local (may be new list if auto-vivified from NOTHING)
            builder.createStoreLocal(var->ref.id, result, op->loc);
            return result;
        }
    }

    // Fallback: delegate to AST for non-local lvalues (global, closure, complex lvalues)
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

    // Check if all keys are constant strings — use optimized MakeHashConstKeys
    bool all_const_keys = true;
    std::vector<std::string> const_keys;
    const_keys.reserve(keys.size());
    for (size_t i = 0; i < keys.size(); ++i) {
        if (keys[i].getType() == NT_STRING) {
            const_keys.push_back(keys[i].get<const QoreStringNode>()->c_str());
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
            QoreIRValue value = lowerExpression(values_vec[i], error);
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
        QoreIRValue value = lowerExpression(values_vec[i], error);
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

QoreIRValue QoreIRLowering::lowerDotEval(const QoreValue& expr, std::string& error) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* op = dynamic_cast<const QoreDotEvalOperatorNode*>(node);
    if (!op) {
        return QoreIRValue();
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
    // Exclude copy() calls (getRawName() == nullptr means it's a copy call).
    MethodCallNode* m = op->getMethodCall();
    if (m->getRawName()) {
        // Lower arguments
        std::vector<QoreIRValue> lowered_args;
        if (lowerCallArgs(m->getParseArgs(), m->getArgs(), lowered_args, error)) {
            // Build operands = [base, arg0, arg1, ...]
            std::vector<QoreIRValue> operands;
            operands.push_back(base_val);
            operands.insert(operands.end(), lowered_args.begin(), lowered_args.end());

            QoreIRValue result;
            bool should_invoke = !exception_stack.empty();
            if (should_invoke) {
                QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
                if (!normal_block) {
                    error = "IR builder failed to create invoke continuation block";
                    return QoreIRValue();
                }
                QoreIRBasicBlock* handler = exception_stack.back();
                auto* inst = builder.createInvokeDotEvalMethodDirect(m->getMethod(), m->getClass(),
                    m->getVariant(), expr, m->isPseudo(), operands, normal_block, handler, op->loc);
                builder.setBlock(normal_block);
                result = inst->result;
            } else {
                auto* inst = builder.createDotEvalMethodDirect(m->getMethod(), m->getClass(),
                    m->getVariant(), expr, m->isPseudo(), operands, op->loc);
                result = inst->result;
            }
            return result;
        }
        // lowerCallArgs failed — fall through to generic path
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
    // Track AST-delegated instructions for map body optimization
    ++ast_delegate_count;

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
    store_inst->operands.push_back(hash_val);
    store_inst->operands.push_back(new_val);

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
    if (!func_name || strcmp(func_name, "string") != 0) {
        return QoreIRValue();
    }
    // Match string(expr) with exactly 1 argument and no encoding parameter
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
    // ToString never throws — always emit as a plain ExprOp, not Invoke.
    // The argument's call (if any) is already lowered as Invoke by lowerCallArgs.
    QoreIRValue result = builder.createExprOp(QoreIROpcode::ToString, expr, operands, call->loc)->result;
    // ToString always returns a string, never NOTHING
    never_nothing_values.insert(result.id);
    return result;
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

    // If the function is resolved at parse time, use CallDirect to skip the AST round-trip
    const QoreFunction* func = call->getFunction();
    if (func) {
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
    // Use CallClosureDirect for fast closure/callref invocation
    // This calls qore_rt_call_closure_fast() which directly calls callref->execValue()
    // instead of going through AST node copy and dynamic_cast chain
    return lowerExprOpOrInvoke(QoreIROpcode::CallClosureDirect, expr, operands, call->loc, error);
}

// Helper function for Phase 3: Check if a callee is eligible for inlining and cache its IR
static void tryCacheCalleeIRForInlining(const AbstractQoreFunctionVariant* variant,
        QoreIRCallMethodDirectInstruction* inst) {
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

    // Check inlining thresholds: ≤20 instructions, ≤2 blocks
    // Count basic blocks and instructions
    unsigned block_count = 0;
    unsigned total_instructions = 0;

    for (const auto& block_ptr : callee_ir->blocks) {
        block_count++;
        if (block_count > 2) {
            // Too many blocks, don't inline
            inst->inline_ir_state = -1;
            return;
        }
        for (const auto& instr : block_ptr->instructions) {
            total_instructions++;
            if (total_instructions > 20) {
                // Too many instructions, don't inline
                inst->inline_ir_state = -1;
                return;
            }
        }
    }

    // Callee is eligible for inlining
    inst->cached_callee_ir = callee_ir;
    inst->inline_ir_state = 1;
}

// Helper function for static method calls
static void tryCacheCalleeIRForInlining(const AbstractQoreFunctionVariant* variant,
        QoreIRCallStaticDirectInstruction* inst) {
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

    // Check inlining thresholds: ≤20 instructions, ≤2 blocks
    unsigned block_count = 0;
    unsigned total_instructions = 0;

    for (const auto& block_ptr : callee_ir->blocks) {
        block_count++;
        if (block_count > 2) {
            // Too many blocks, don't inline
            inst->inline_ir_state = -1;
            return;
        }
        for (const auto& instr : block_ptr->instructions) {
            total_instructions++;
            if (total_instructions > 20) {
                // Too many instructions, don't inline
                inst->inline_ir_state = -1;
                return;
            }
        }
    }

    // Callee is eligible for inlining
    inst->cached_callee_ir = callee_ir;
    inst->inline_ir_state = 1;
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
            auto* call_inst = dynamic_cast<QoreIRCallMethodDirectInstruction*>(invoke_inst);
            if (call_inst) {
                tryCacheCalleeIRForInlining(variant, call_inst);
            }
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

    // Only use CallStaticDirect if AST conclusively determined the variant at parse time.
    // If getVariant() returns nullptr, AST set runtime_match=true, meaning parse-time variant
    // selection was inconclusive (due to missing type information), and runtime dispatch is required.
    const AbstractQoreFunctionVariant* variant = call->getVariant();
    if (call->getMethod() && variant) {
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
// Returns the key name if matched, nullptr otherwise
static const char* getImplicitHashKeyAccess(const QoreValue& expr) {
    const AbstractQoreNode* node = expr.getInternalNode();
    auto* deref = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(node);
    if (!deref) {
        return nullptr;
    }
    // Check left is $1
    QoreValue left_val = deref->getLeft();
    auto* impl_arg = dynamic_cast<const QoreImplicitArgumentNode*>(left_val.getInternalNode());
    if (!impl_arg || impl_arg->getOffset() != 0) {
        return nullptr;
    }
    // Check right is constant string
    QoreValue right_val = deref->getRight();
    if (!right_val.hasNode() || right_val.getType() != NT_STRING) {
        return nullptr;
    }
    return right_val.get<const QoreStringNode>()->c_str();
}

// Pattern analysis for hash-key map operations
// Detects: map $1.key, list / map ($1.key + N), list / map ($1.key * N), list
// Returns optimized opcode or MapAny for fallback
// Sets key_name to the key name and constant_val for offset/scale patterns
static QoreIROpcode analyzeMapHashKeyPattern(const QoreValue& map_expr, const char*& key_name,
        QoreValue& constant_val) {
    // Direct $1.key pattern
    const char* key = getImplicitHashKeyAccess(map_expr);
    if (key) {
        key_name = key;
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
        const char* k = getImplicitHashKeyAccess(left);
        if (k && !right.hasNode() && (right.getType() == NT_INT)) {
            // Check result type is int
            const QoreTypeInfo* rtype = plus_op->getTypeInfo();
            if (QoreTypeInfo::parseReturns(rtype, NT_INT) == QTI_IDENT) {
                key_name = k;
                constant_val = right;
                return QoreIROpcode::MapHashKeyOffsetInt;
            }
        }

        // Pattern: const + $1.key
        k = getImplicitHashKeyAccess(right);
        if (k && !left.hasNode() && (left.getType() == NT_INT)) {
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
        const char* k = getImplicitHashKeyAccess(left);
        if (k && !right.hasNode() && (right.getType() == NT_INT)) {
            const QoreTypeInfo* rtype = mul_op->getTypeInfo();
            if (QoreTypeInfo::parseReturns(rtype, NT_INT) == QTI_IDENT) {
                key_name = k;
                constant_val = right;
                return QoreIROpcode::MapHashKeyScaleInt;
            }
        }

        // Pattern: const * $1.key
        k = getImplicitHashKeyAccess(right);
        if (k && !left.hasNode() && (left.getType() == NT_INT)) {
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
        const char*& key1_name, const char*& key2_name) {
    const char* k1 = getImplicitHashKeyAccess(key_expr);
    const char* k2 = getImplicitHashKeyAccess(val_expr);
    if (k1 && k2) {
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
            const char* key_name = nullptr;
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
                auto* inst = builder.createMapHashKey(hk_opcode, key_name, nullptr, map->loc);
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
            const char* key1_name = nullptr;
            const char* key2_name = nullptr;
            QoreIROpcode hk_opcode = analyzeHashMapTwoKeysPattern(map->get(0), map->get(1),
                key1_name, key2_name);
            if (hk_opcode == QoreIROpcode::HashMapTwoKeys) {
                // Lower the input list
                QoreIRValue list_val = lowerExpression(map->get(2), error);
                if (!list_val.isValid()) {
                    return QoreIRValue();
                }

                auto* inst = builder.createMapHashKey(QoreIROpcode::HashMapTwoKeys,
                    key1_name, key2_name, map->loc);
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

    // Only enter single-value path when we have positive evidence it's NOT a list:
    // - elem_type must be null (no list element type extracted)
    // - list_type must be non-null (we have type information)
    // - parseReturns must confirm it's NOT a list
    // This guard prevents entering single-value path for unknown types (like method calls)
    if (!elem_type && list_type && !QoreTypeInfo::parseReturns(list_type, NT_LIST)) {
        QoreIRValue input_val = lowerExpression(map->getRight(), error);
        if (!input_val.isValid()) {
            return QoreIRValue();
        }
        VirtualImplicitContext saved = virtual_implicit;
        virtual_implicit.arg0 = input_val;
        virtual_implicit.arg1 = QoreIRValue();
        virtual_implicit.element = builder.createConstInt(0, map->loc)->result;
        virtual_implicit.active = true;
        QoreIRValue expr_result = lowerExpression(map->getLeft(), error);
        virtual_implicit = saved;
        return expr_result;
    }

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
    QoreIRValue input_list = lowerExpression(map->getRight(), error);
    if (!input_list.isValid()) {
        return QoreIRValue();
    }

    if (use_direct_index) {
        // Direct-index loop: avoid iterator overhead for typed lists
        // Get list size
        QoreIRValue list_size = builder.createListSize(input_list, map->loc)->result;

        // Create blocks AFTER evaluating the input expression
        // No empty check needed: createSizedList(0) produces an empty list,
        // and the header condition (0 >= 0) immediately exits the loop.
        QoreIRBasicBlock* preheader_block = createBlock("map.preheader");
        QoreIRBasicBlock* header_block = createBlock("map.header");
        QoreIRBasicBlock* body_block = createBlock("map.body");
        QoreIRBasicBlock* exit_block = createBlock("map.exit");
        if (!preheader_block || !header_block || !body_block || !exit_block) {
            error = "IR builder failed to create blocks for map";
            return QoreIRValue();
        }
        header_block->is_loop_header = true;

        builder.createBranch(preheader_block, map->loc);

        // Preheader: create pre-sized result list (size 0 for empty input is fine)
        builder.setBlock(preheader_block);
        QoreIRValue zero = builder.createConstInt(0, map->loc)->result;
        QoreIRValue result_list = builder.createSizedList(list_size, map->loc, expTypeInfo)->result;
        builder.createBranch(header_block, map->loc);

        // Header block: check if index < size
        builder.setBlock(header_block);

        auto* index_phi = builder.createPhi({}, map->loc);
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

        // Store result directly at index position in pre-sized list
        builder.createListSetValue(result_list, index_val, expr_result, map->loc);

        // Increment index
        QoreIRValue one = builder.createConstInt(1, map->loc)->result;
        QoreIRValue next_index = builder.createBinaryOp(QoreIROpcode::AddInt, index_val, one, map->loc)->result;

        // Record body exit block
        QoreIRBasicBlock* body_exit_block = builder.getBlock();

        // Branch back to header
        builder.createBranch(header_block, map->loc);

        // Complete PHI nodes
        index_phi->incoming.push_back({zero, preheader_block});
        index_phi->incoming.push_back({next_index, body_exit_block});
        index_phi->operands.push_back(zero);
        index_phi->operands.push_back(next_index);

        // Exit block: result_list is the result (empty for size 0, filled for size > 0)
        builder.setBlock(exit_block);

        return result_list;
    }

    // Fallback: iterator-based loop for untyped lists

    // Create iterator from input list
    auto* iter_inst = builder.createIteratorCreate(input_list, nullptr, map->loc);
    QoreIRValue iter_val = iter_inst->result;

    // Create basic blocks for the loop structure AFTER evaluating the input
    // expression and creating the iterator
    QoreIRBasicBlock* preheader_block = createBlock("map.preheader");
    QoreIRBasicBlock* header_block = createBlock("map.header");
    QoreIRBasicBlock* body_block = createBlock("map.body");
    QoreIRBasicBlock* exit_block = createBlock("map.exit");
    QoreIRBasicBlock* nothing_block = createBlock("map.nothing");
    if (!preheader_block || !header_block || !body_block || !exit_block || !nothing_block) {
        error = "IR builder failed to create blocks for map";
        return QoreIRValue();
    }
    header_block->is_loop_header = true;

    // Check if iterator is null (input was NOTHING) — return NOTHING in that case
    QoreIRValue zero = builder.createConstInt(0, map->loc)->result;
    QoreIRValue iter_as_int = iter_val;
    QoreIRValue is_null = builder.createBinaryOp(QoreIROpcode::EqInt, iter_as_int, zero, map->loc)->result;
    builder.createBranchIf(is_null, nothing_block, preheader_block, map->loc);

    // Preheader: create empty result list and proceed to loop
    builder.setBlock(preheader_block);
    QoreIRValue result_list = builder.createEmptyList(map->loc, expTypeInfo)->result;
    QoreIRValue init_index = builder.createConstInt(0, map->loc)->result;
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

    // Set virtual implicit context: $1 = element, $# = index (fast path for IR-lowered refs)
    VirtualImplicitContext saved = virtual_implicit;
    virtual_implicit.arg0 = element_val;
    virtual_implicit.arg1 = QoreIRValue();
    virtual_implicit.element = index_val;
    virtual_implicit.active = true;

    // Lower the map expression first - check if any AST delegation occurs
    int saved_ast_count = ast_delegate_count;
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
    index_phi->incoming.push_back({init_index, preheader_block});
    index_phi->incoming.push_back({next_index, body_exit_block});
    index_phi->operands.push_back(init_index);
    index_phi->operands.push_back(next_index);

    // Nothing block: input was NOTHING → return NOTHING
    builder.setBlock(nothing_block);
    QoreIRValue nothing_val = builder.createConstNothing(map->loc)->result;
    builder.createBranch(exit_block, map->loc);

    // Exit block: PHI between result_list (from loop) and NOTHING (from null check)
    builder.setBlock(exit_block);

    std::vector<QoreIRPhiIncoming> result_incoming;
    result_incoming.push_back({result_list, header_block});   // Normal loop exit
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
        QoreIRValue list_size = builder.createListSize(input_list, select->loc)->result;

        // Create blocks AFTER evaluating the input expression
        // No empty check needed: createEmptyList produces an empty list for empty input,
        // and the header condition (0 >= 0) immediately exits the loop.
        QoreIRBasicBlock* preheader_block = createBlock("select.preheader");
        QoreIRBasicBlock* header_block = createBlock("select.header");
        QoreIRBasicBlock* body_block = createBlock("select.body");
        QoreIRBasicBlock* append_block = createBlock("select.append");
        QoreIRBasicBlock* cont_block = createBlock("select.cont");
        QoreIRBasicBlock* exit_block = createBlock("select.exit");
        if (!preheader_block || !header_block || !body_block
                || !append_block || !cont_block || !exit_block) {
            error = "IR builder failed to create blocks for select";
            return QoreIRValue();
        }
        header_block->is_loop_header = true;

        builder.createBranch(preheader_block, select->loc);

        // Preheader: create empty result list (filtered, size unknown)
        builder.setBlock(preheader_block);
        QoreIRValue zero = builder.createConstInt(0, select->loc)->result;
        QoreIRValue result_list = builder.createEmptyList(select->loc)->result;
        builder.createBranch(header_block, select->loc);

        // Header block: check if index < size
        builder.setBlock(header_block);

        auto* index_phi = builder.createPhi({}, select->loc);
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

        builder.createBranch(header_block, select->loc);

        // Complete PHI nodes
        index_phi->incoming.push_back({zero, preheader_block});
        index_phi->incoming.push_back({next_index, cont_exit_block});
        index_phi->operands.push_back(zero);
        index_phi->operands.push_back(next_index);

        // Exit block: result_list is the result (empty for size 0)
        builder.setBlock(exit_block);

        return result_list;
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
    builder.createBranch(header_block, select->loc);

    // Header block: create phi for index and check for next value
    builder.setBlock(header_block);

    auto* index_phi = builder.createPhi({}, select->loc);
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

    builder.createBranch(header_block, select->loc);

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
        builder.createBranch(header_block, foldl->loc);

        // Header block: check if index < size
        builder.setBlock(header_block);

        // PHI for index
        auto* index_phi = builder.createPhi({}, foldl->loc);
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
        builder.createBranch(header_block, foldl->loc);

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

QoreIRValue QoreIRLowering::lowerFoldrNative(const QoreFoldrOperatorNode* foldr, const QoreValue& expr,
        std::string& error) {
    if (!ensureBuilderContext(error)) {
        return QoreIRValue();
    }

    // foldr is identical to foldl except with reverse iteration

    // Evaluate the input list (right operand of foldr)
    QoreIRValue input_list = lowerExpression(foldr->getRight(), error);
    if (!input_list.isValid()) {
        return QoreIRValue();
    }

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
    builder.createBranch(header_block, foldr->loc);

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

    // e[0] = map expression, e[1] = iterator/input, e[2] = select predicate

    // Check if the input list has a known element type for direct-index optimization
    const QoreTypeInfo* list_type = getExprTypeInfo(ms->get(1));
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

    // Evaluate the input list (operand 1)
    QoreIRValue input_list = lowerExpression(ms->get(1), error);
    if (!input_list.isValid()) {
        return QoreIRValue();
    }

    if (use_direct_index) {
        // Direct-index loop: avoid iterator overhead for typed lists
        QoreIRValue list_size = builder.createListSize(input_list, ms->loc)->result;

        // Create blocks AFTER evaluating the input expression
        // No empty check needed: createEmptyList produces an empty list,
        // and the header condition (0 >= 0) immediately exits the loop.
        QoreIRBasicBlock* preheader_block = createBlock("mapselect.preheader");
        QoreIRBasicBlock* header_block = createBlock("mapselect.header");
        QoreIRBasicBlock* body_block = createBlock("mapselect.body");
        QoreIRBasicBlock* append_block = createBlock("mapselect.append");
        QoreIRBasicBlock* cont_block = createBlock("mapselect.cont");
        QoreIRBasicBlock* exit_block = createBlock("mapselect.exit");
        if (!preheader_block || !header_block || !body_block
                || !append_block || !cont_block || !exit_block) {
            error = "IR builder failed to create blocks for map+select";
            return QoreIRValue();
        }
        header_block->is_loop_header = true;

        builder.createBranch(preheader_block, ms->loc);

        // Preheader: create empty result list (filtered, size unknown)
        builder.setBlock(preheader_block);
        QoreIRValue zero = builder.createConstInt(0, ms->loc)->result;
        QoreIRValue result_list = builder.createEmptyList(ms->loc)->result;
        builder.createBranch(header_block, ms->loc);

        // Header block: check if index < size
        builder.setBlock(header_block);

        auto* index_phi = builder.createPhi({}, ms->loc);
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

        QoreIRValue map_result = lowerExpression(ms->get(0), error);
        if (!map_result.isValid()) {
            virtual_implicit = saved;
            return QoreIRValue();
        }

        // Restore virtual context
        virtual_implicit = saved;

        bool needs_implicit_push = (ast_delegate_count > saved_ast_count);
        if (needs_implicit_push) {
            QoreIRFunction* func = builder.getFunction();

            auto push_elem = std::make_unique<QoreIRInstruction>(QoreIROpcode::PushImplicitElement);
            push_elem->result = func->createValue();
            push_elem->operands.push_back(index_val);
            push_elem->loc = ms->loc;
            QoreIRValue old_element = push_elem->result;

            auto push_argv = std::make_unique<QoreIRInstruction>(QoreIROpcode::PushImplicitArg);
            push_argv->result = func->createValue();
            push_argv->operands.push_back(element_val);
            push_argv->loc = ms->loc;
            QoreIRValue old_argv = push_argv->result;

            size_t insert_pos = 1;  // After element load
            body_block->instructions.insert(body_block->instructions.begin() + insert_pos,
                std::move(push_elem));
            body_block->instructions.insert(body_block->instructions.begin() + insert_pos + 1,
                std::move(push_argv));

            builder.createPopImplicitArg(old_argv, ms->loc);
            builder.createPopImplicitElement(old_element, ms->loc);
        }

        builder.createListAppend(result_list, map_result, ms->loc);
        builder.createBranch(cont_block, ms->loc);

        // Continue block: increment index, loop back
        builder.setBlock(cont_block);

        QoreIRValue one = builder.createConstInt(1, ms->loc)->result;
        QoreIRValue next_index = builder.createBinaryOp(QoreIROpcode::AddInt, index_val, one,
            ms->loc)->result;

        QoreIRBasicBlock* cont_exit_block = builder.getBlock();

        builder.createBranch(header_block, ms->loc);

        // Complete PHI nodes
        index_phi->incoming.push_back({zero, preheader_block});
        index_phi->incoming.push_back({next_index, cont_exit_block});
        index_phi->operands.push_back(zero);
        index_phi->operands.push_back(next_index);

        // Exit block: result_list is the result (empty for size 0)
        builder.setBlock(exit_block);

        return result_list;
    }

    // Fallback: iterator-based loop for untyped lists

    // Create iterator from input list
    auto* iter_inst = builder.createIteratorCreate(input_list, nullptr, ms->loc);
    QoreIRValue iter_val = iter_inst->result;

    // Create loop blocks AFTER expression evaluation
    QoreIRBasicBlock* preheader_block = createBlock("mapselect.preheader");
    QoreIRBasicBlock* header_block = createBlock("mapselect.header");
    QoreIRBasicBlock* body_block = createBlock("mapselect.body");
    QoreIRBasicBlock* append_block = createBlock("mapselect.append");
    QoreIRBasicBlock* cont_block = createBlock("mapselect.cont");
    QoreIRBasicBlock* exit_block = createBlock("mapselect.exit");
    QoreIRBasicBlock* nothing_block = createBlock("mapselect.nothing");
    if (!preheader_block || !header_block || !body_block || !append_block
            || !cont_block || !exit_block || !nothing_block) {
        error = "IR builder failed to create blocks for map+select";
        return QoreIRValue();
    }
    header_block->is_loop_header = true;

    // Check if iterator is null (input was NOTHING) → return NOTHING
    QoreIRValue zero = builder.createConstInt(0, ms->loc)->result;
    QoreIRValue is_null = builder.createBinaryOp(QoreIROpcode::EqInt, iter_val, zero, ms->loc)->result;
    builder.createBranchIf(is_null, nothing_block, preheader_block, ms->loc);

    // Preheader: create empty result list and proceed to loop
    builder.setBlock(preheader_block);
    QoreIRValue result_list = builder.createEmptyList(ms->loc)->result;
    QoreIRValue init_index = builder.createConstInt(0, ms->loc)->result;
    builder.createBranch(header_block, ms->loc);

    // Header block: create phi for index and check for next value
    builder.setBlock(header_block);

    auto* index_phi = builder.createPhi({}, ms->loc);
    QoreIRValue index_val = index_phi->result;

    // Get next element from iterator
    auto* next_inst = builder.createIteratorNext(iter_val, exit_block, body_block, ms->loc);
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

    QoreIRValue map_result = lowerExpression(ms->get(0), error);
    if (!map_result.isValid()) {
        virtual_implicit = saved;
        return QoreIRValue();
    }

    builder.createListAppend(result_list, map_result, ms->loc);
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

    builder.createBranch(header_block, ms->loc);

    // Complete the phi node
    index_phi->incoming.push_back({init_index, preheader_block});
    index_phi->incoming.push_back({next_index, cont_exit_block});
    index_phi->operands.push_back(init_index);
    index_phi->operands.push_back(next_index);

    // Nothing block: input was NOTHING → return NOTHING
    builder.setBlock(nothing_block);
    QoreIRValue nothing_val = builder.createConstNothing(ms->loc)->result;
    builder.createBranch(exit_block, ms->loc);

    // Exit block: PHI between result_list and NOTHING
    builder.setBlock(exit_block);

    std::vector<QoreIRPhiIncoming> result_incoming;
    result_incoming.push_back({result_list, header_block});
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
        QoreIRValue list_size = builder.createListSize(input_list, hm->loc)->result;

        // Create blocks AFTER evaluating the input expression
        // No empty check needed: createMakeHash produces an empty hash,
        // and the header condition (0 >= 0) immediately exits the loop.
        QoreIRBasicBlock* preheader_block = createBlock("hashmap.preheader");
        QoreIRBasicBlock* header_block = createBlock("hashmap.header");
        QoreIRBasicBlock* body_block = createBlock("hashmap.body");
        QoreIRBasicBlock* exit_block = createBlock("hashmap.exit");
        if (!preheader_block || !header_block || !body_block || !exit_block) {
            error = "IR builder failed to create blocks for hash map";
            return QoreIRValue();
        }
        header_block->is_loop_header = true;

        builder.createBranch(preheader_block, hm->loc);

        // Preheader: create empty result hash and proceed to loop
        builder.setBlock(preheader_block);
        QoreIRValue zero = builder.createConstInt(0, hm->loc)->result;
        QoreIRValue result_hash = builder.createMakeHash({}, hm->loc, hash_result_type)->result;
        builder.createBranch(header_block, hm->loc);

        // Header block: check if index < size
        builder.setBlock(header_block);

        auto* index_phi = builder.createPhi({}, hm->loc);
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
        builder.createBranch(header_block, hm->loc);

        // Complete PHI nodes
        index_phi->incoming.push_back({zero, preheader_block});
        index_phi->incoming.push_back({next_index, body_exit_block});
        index_phi->operands.push_back(zero);
        index_phi->operands.push_back(next_index);

        // Exit block: result_hash is the result (empty for size 0)
        builder.setBlock(exit_block);

        return result_hash;
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
    builder.createBranch(header_block, hm->loc);

    // Header block
    builder.setBlock(header_block);

    auto* index_phi = builder.createPhi({}, hm->loc);
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
    builder.createBranch(header_block, hm->loc);

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
        QoreIRValue list_size = builder.createListSize(input_list, hms->loc)->result;

        // Create blocks AFTER evaluating the input expression
        // No empty check needed: createMakeHash produces an empty hash,
        // and the header condition (0 >= 0) immediately exits the loop.
        QoreIRBasicBlock* preheader_block = createBlock("hashmapselect.preheader");
        QoreIRBasicBlock* header_block = createBlock("hashmapselect.header");
        QoreIRBasicBlock* body_block = createBlock("hashmapselect.body");
        QoreIRBasicBlock* insert_block = createBlock("hashmapselect.insert");
        QoreIRBasicBlock* cont_block = createBlock("hashmapselect.cont");
        QoreIRBasicBlock* exit_block = createBlock("hashmapselect.exit");
        if (!preheader_block || !header_block || !body_block
                || !insert_block || !cont_block || !exit_block) {
            error = "IR builder failed to create blocks for hash map+select";
            return QoreIRValue();
        }
        header_block->is_loop_header = true;

        builder.createBranch(preheader_block, hms->loc);

        // Preheader: create empty result hash and proceed to loop
        builder.setBlock(preheader_block);
        QoreIRValue zero = builder.createConstInt(0, hms->loc)->result;
        QoreIRValue result_hash = builder.createMakeHash({}, hms->loc, hash_result_type)->result;
        builder.createBranch(header_block, hms->loc);

        // Header block: check if index < size
        builder.setBlock(header_block);

        auto* index_phi = builder.createPhi({}, hms->loc);
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
        builder.createBranch(header_block, hms->loc);

        // Complete PHI nodes
        index_phi->incoming.push_back({zero, preheader_block});
        index_phi->incoming.push_back({next_index, cont_exit_block});
        index_phi->operands.push_back(zero);
        index_phi->operands.push_back(next_index);

        // Exit block: result_hash is the result (empty for size 0)
        builder.setBlock(exit_block);

        return result_hash;
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
    builder.createBranch(header_block, hms->loc);

    // Header block
    builder.setBlock(header_block);

    auto* index_phi = builder.createPhi({}, hms->loc);
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
    builder.createBranch(header_block, hms->loc);

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
