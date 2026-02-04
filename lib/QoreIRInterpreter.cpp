/* -*- indent-tabs-mode: nil -*- */
/*
    QoreIRInterpreter.cpp

    Qore Programming Language
*/

#include <qore/intern/QoreIRInterpreter.h>

#include <cmath>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <qore/ExceptionSink.h>
#include <qore/QoreValue.h>
#include <qore/QoreStringNode.h>
#include <qore/DateTimeNode.h>
#include <qore/intern/AbstractStatement.h>
#include <qore/intern/DebugStatement.h>
#include <qore/intern/AssertStatement.h>
#include <qore/intern/ContextStatement.h>
#include <qore/intern/SummarizeStatement.h>
#include <qore/intern/ForEachStatement.h>
#include <qore/intern/FunctionalOperatorInterface.h>
#include <qore/intern/FunctionCallNode.h>
#include <qore/intern/CallReferenceCallNode.h>
#include <qore/intern/OnBlockExitStatement.h>
#include <qore/intern/QoreRegex.h>
#include <qore/intern/QoreRegexMatchOperatorNode.h>
#include <qore/intern/QoreRegexNMatchOperatorNode.h>
#include <qore/intern/QoreRegexExtractOperatorNode.h>
#include <qore/intern/StatementBlock.h>
#include <qore/intern/QoreException.h>
#include <qore/intern/qore_thread_intern.h>
#include <qore/intern/Variable.h>
#include <qore/intern/QoreTypeInfo.h>
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
#include <qore/intern/QoreIR.h>
#include <qore/intern/QoreJIT.h>
#include <qore/intern/QoreOperatorNode.h>
#include <qore/intern/QoreHashObjectDereferenceOperatorNode.h>
#include <qore/intern/QoreDotEvalOperatorNode.h>

static bool guardPredicate(QoreIROpcode opcode, const QoreValue& value, const QoreTypeInfo* type_info) {
    switch (opcode) {
        case QoreIROpcode::GuardInt:
            return value.isInt();
        case QoreIROpcode::GuardFloat:
            return value.isFloat();
        case QoreIROpcode::GuardType:
            if (!type_info) {
                return true;
            }
            return QoreTypeInfo::isType(type_info, value.getType());
        case QoreIROpcode::GuardNotNothing:
            return !value.isNothing();
        default:
            return true;
    }
}
#include <qore/intern/QoreMapOperatorNode.h>
#include <qore/intern/QoreSelectOperatorNode.h>
#include <qore/intern/QoreMapSelectOperatorNode.h>
#include <qore/intern/QoreHashMapOperatorNode.h>
#include <qore/intern/QoreHashMapSelectOperatorNode.h>
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
#include <qore/intern/LocalVar.h>
#include <qore/intern/VarRefNode.h>
#include <qore/QoreHashNode.h>
#include <qore/QoreListNode.h>
#include <qore/QoreStringNode.h>
#include <qore/intern/QoreHashNodeIntern.h>
#include <qore/intern/qore_list_private.h>

// Helper: evaluate a node and ensure the caller owns a reference to the result.
// Some operator eval() implementations return with needs_deref=false (borrowed reference).
// This function ensures we always return an owned reference.
static QoreValue evalAndRef(AbstractQoreNode* node, ExceptionSink* xsink) {
    bool needs_deref = true;
    QoreValue result = node->eval(needs_deref, xsink);
    if (!needs_deref && result.hasNode()) {
        result = result.refSelf();
    }
    return result;
}

// Overload for QoreValue (used with ValueHolder::operator->())
static QoreValue evalAndRef(QoreValue& val, ExceptionSink* xsink) {
    if (!val.hasNode()) {
        return val;
    }
    return evalAndRef(val.getInternalNode(), xsink);
}

static QoreValue evalExprNode(const QoreValue& expr, ExceptionSink* xsink) {
    if (!expr.hasNode()) {
        if (xsink) {
            xsink->raiseException("IR-INTERPRETER-ERROR", "expression opcode requires a parse node");
        }
        return QoreValue();
    }
    bool needs_deref = true;
    ValueHolder node(expr.refSelf(), xsink);
    if (xsink && *xsink) {
        return QoreValue();
    }
    QoreValue result = node->eval(needs_deref, xsink);
    // Ensure the caller always owns a reference to the result
    if (!needs_deref && result.hasNode()) {
        result = result.refSelf();
    }
    return result;
}

QoreValue QoreIRInterpreter::evalComparison(QoreIROpcode op, const QoreValue& left, const QoreValue& right,
        ExceptionSink* xsink) {
    switch (op) {
        case QoreIROpcode::EqInt:
            return QoreValue(left.getAsBigInt() == right.getAsBigInt());
        case QoreIROpcode::EqFloat:
            return QoreValue(left.getAsFloat() == right.getAsFloat());
        case QoreIROpcode::EqAny:
            return QoreValue(QoreLogicalEqualsOperatorNode::softEqual(left, right, xsink));
        case QoreIROpcode::NeInt:
            return QoreValue(left.getAsBigInt() != right.getAsBigInt());
        case QoreIROpcode::NeFloat:
            return QoreValue(left.getAsFloat() != right.getAsFloat());
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
        case QoreIROpcode::ExtractList:
        case QoreIROpcode::ExtractString:
        case QoreIROpcode::ExtractBinary:
        case QoreIROpcode::RemoveAny:
        case QoreIROpcode::RemoveList:
        case QoreIROpcode::RemoveHash:
        case QoreIROpcode::RemoveObject:
        case QoreIROpcode::RemoveString:
        case QoreIROpcode::RemoveBinary:
        case QoreIROpcode::KeysAny:
        case QoreIROpcode::KeysList:
        case QoreIROpcode::KeysHash:
        case QoreIROpcode::RegexMatchAny:
        case QoreIROpcode::RegexMatchBool:
        case QoreIROpcode::RegexNMatchBool:
        case QoreIROpcode::RegexExtractAny:
        case QoreIROpcode::RegexExtractList:
        case QoreIROpcode::RegexSubstAny:
        case QoreIROpcode::RegexSubstString:
        case QoreIROpcode::InstanceOfBool:
        case QoreIROpcode::TrimAny:
        case QoreIROpcode::TrimString:
        case QoreIROpcode::ChompAny:
        case QoreIROpcode::ChompString:
        case QoreIROpcode::TransliterateAny:
        case QoreIROpcode::TransliterateString:
        case QoreIROpcode::BackgroundInt:
        case QoreIROpcode::ListAssignAny:
        case QoreIROpcode::PopAny:
        case QoreIROpcode::PushAny:
        case QoreIROpcode::ExistsAny:
        case QoreIROpcode::ExistsBool:
        case QoreIROpcode::ElementsAny:
        case QoreIROpcode::ElementsInt:
        case QoreIROpcode::DotEvalAny:
        case QoreIROpcode::DotEvalInt:
        case QoreIROpcode::DotEvalFloat:
        case QoreIROpcode::DotEvalString:
        case QoreIROpcode::DotEvalDate:
        case QoreIROpcode::DotEvalList:
        case QoreIROpcode::DotEvalHash:
        case QoreIROpcode::DotEvalObject:
        case QoreIROpcode::MapSelectAny:
        case QoreIROpcode::MapSelectList:
        case QoreIROpcode::HashMapAny:
        case QoreIROpcode::HashMap:
        case QoreIROpcode::HashMapSelectAny:
        case QoreIROpcode::HashMapSelect:
        case QoreIROpcode::CastList:
        case QoreIROpcode::CastHash:
        case QoreIROpcode::CastObject:
        case QoreIROpcode::CastEnum:
            return evalExprNode(expr, xsink);
        case QoreIROpcode::InvokeSimError: {
            if (xsink) {
                xsink->raiseException("IR-INVOKE-SIM-ERROR", "invoke simulated error");
            }
            return QoreValue();
        }
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
            // LValue opcodes (StoreLValue, AddAssignLValue, ShlAssignLValue, etc.)
            // and any other opcodes not explicitly handled above
            // are evaluated via the original AST expression
            return evalExprNode(expr, xsink);
    }
}

bool QoreIRInterpreter::simulateInvoke(QoreIROpcode op, const QoreValue& expr, ExceptionSink* xsink) {
    evalExpr(op, expr, xsink);
    return xsink && *xsink;
}

int QoreIRInterpreter::execStatement(QoreIROpcode op, const AbstractStatement* stmt, QoreValue& return_value,
        ExceptionSink* xsink) {
    switch (op) {
        case QoreIROpcode::Debug:
            if (!stmt) {
                if (xsink) {
                    xsink->raiseException("IR-INTERPRETER-ERROR", "debug statement requires a statement");
                }
                return -1;
            }
            return const_cast<AbstractStatement*>(stmt)->exec(return_value, xsink);
        case QoreIROpcode::Assert:
            if (!stmt) {
                if (xsink) {
                    xsink->raiseException("IR-INTERPRETER-ERROR", "assert statement requires a statement");
                }
                return -1;
            }
            return const_cast<AbstractStatement*>(stmt)->exec(return_value, xsink);
        case QoreIROpcode::Context:
            if (!stmt) {
                if (xsink) {
                    xsink->raiseException("IR-INTERPRETER-ERROR", "context statement requires a statement");
                }
                return -1;
            }
            return const_cast<AbstractStatement*>(stmt)->exec(return_value, xsink);
        case QoreIROpcode::Summarize:
            if (!stmt) {
                if (xsink) {
                    xsink->raiseException("IR-INTERPRETER-ERROR", "summarize statement requires a statement");
                }
                return -1;
            }
            return const_cast<AbstractStatement*>(stmt)->exec(return_value, xsink);
        default:
            break;
    }
    if (xsink) {
        xsink->raiseException("IR-INTERPRETER-ERROR", "unsupported statement opcode");
    }
    return -1;
}

static QoreValue getIRValue(const std::unordered_map<uint32_t, QoreValue>& values, QoreIRValue id) {
    if (!id.isValid()) {
        return QoreValue();
    }
    auto it = values.find(id.id);
    if (it == values.end()) {
        return QoreValue();
    }
    return it->second;
}

static void removeCleanupEntry(std::vector<uint32_t>& cleanup, uint32_t id) {
    for (auto it = cleanup.rbegin(); it != cleanup.rend(); ++it) {
        if (*it == id) {
            cleanup.erase(std::next(it).base());
            return;
        }
    }
}

static void cleanupValues(const std::unordered_map<uint32_t, QoreValue>& values, std::vector<uint32_t>& cleanup,
        ExceptionSink* xsink, bool no_throw, std::vector<std::string>* cleanup_log) {
    for (auto it = cleanup.rbegin(); it != cleanup.rend(); ++it) {
        auto val_it = values.find(*it);
        if (val_it == values.end()) {
            continue;
        }
        QoreValue temp = val_it->second;
        if (cleanup_log && temp.hasNode()) {
            if (temp.getType() == NT_STRING) {
                auto* str = temp.get<QoreStringNode>();
                if (str) {
                    cleanup_log->push_back(str->getBuffer());
                }
            } else {
                cleanup_log->push_back(temp.getTypeName());
            }
        }
        temp.discard(no_throw ? nullptr : xsink);
    }
    cleanup.clear();
}

static void cleanupStoredValues(std::unordered_map<const void*, QoreValue>& values, ExceptionSink* xsink) {
    for (auto& entry : values) {
        entry.second.discard(xsink);
    }
    values.clear();
}

static void storeValue(std::unordered_map<const void*, QoreValue>& values, const void* key, const QoreValue& value,
        ExceptionSink* xsink) {
    auto it = values.find(key);
    if (it != values.end()) {
        it->second.discard(xsink);
    }
    QoreValue stored = value.hasNode() ? value.refSelf() : value;
    values[key] = stored;
}

static void ensureLocalInstantiated(LocalVar* var, std::unordered_set<const LocalVar*>& locals,
        const std::unordered_set<const LocalVar*>* pre_instantiated = nullptr) {
    if (!var) {
        return;
    }
    if (locals.insert(var).second) {
        // Skip instantiation for locals that are already instantiated by the caller (e.g. top-level)
        if (!pre_instantiated || pre_instantiated->find(var) == pre_instantiated->end()) {
            var->instantiate(0);
        }
    }
}

static void cleanupInstantiatedLocals(const std::unordered_set<const LocalVar*>& locals, ExceptionSink* xsink,
        const std::unordered_set<const LocalVar*>* pre_instantiated = nullptr) {
    for (auto* var : locals) {
        if (var) {
            // Skip uninstantiation for locals managed by the caller
            if (pre_instantiated && pre_instantiated->find(var) != pre_instantiated->end()) {
                continue;
            }
            var->uninstantiate(xsink);
        }
    }
}

static void assignLocalVarValue(LocalVar* var, const QoreValue& value, ExceptionSink* xsink) {
    if (!var) {
        return;
    }
    LValueHelper helper(xsink);
    if (var->getLValue(helper, false, true)) {
        return;
    }
    // refSelf before assign — assign takes ownership of the reference
    QoreValue stored = value.hasNode() ? value.refSelf() : value;
    helper.assign(stored);
}

// Write-through for closure variable stores: writes the value to the actual
// ClosureVarValue so that changes are visible outside the IR interpreter.
static void assignClosureVarValue(LocalVar* var, const QoreValue& value, ExceptionSink* xsink) {
    if (!var) {
        return;
    }
    ClosureVarValue* cv = thread_get_runtime_closure_var(var);
    if (!cv) {
        return;
    }
    LValueHelper helper(xsink);
    if (cv->getLValue(helper, false)) {
        return;
    }
    QoreValue stored = value.hasNode() ? value.refSelf() : value;
    helper.assign(stored);
}

// Write-through for global variable stores: writes the value to the actual
// global Var so that changes are visible outside the IR interpreter.
static void assignGlobalVarValue(Var* var, const QoreValue& value, ExceptionSink* xsink) {
    if (!var) {
        return;
    }
    LValueHelper helper(xsink);
    if (var->getLValue(helper, false)) {
        return;
    }
    QoreValue stored = value.hasNode() ? value.refSelf() : value;
    helper.assign(stored);
}

// Walk a compound lvalue expression (e.g., rv.body, obj.member) to find the root
// variable reference, then invalidate it from the stored values cache.  This is
// necessary because LValueHelper may trigger copy-on-write (COW) on the container,
// replacing the object on the thread-local variable stack with a new copy.  The cached
// version in locals/globals/closures maps would then be stale.
static void invalidateLvalueRoot(const QoreValue& lvalue,
        std::unordered_map<const void*, QoreValue>& locals,
        std::unordered_map<const void*, QoreValue>& globals,
        std::unordered_map<const void*, QoreValue>& threadlocals,
        std::unordered_map<const void*, QoreValue>& closures) {
    if (!lvalue.hasNode()) {
        return;
    }
    const AbstractQoreNode* node = lvalue.getInternalNode();
    // Walk through binary operator nodes (like . or {} dereference) to find
    // the root VarRefNode
    while (node) {
        if (auto* var_ref = dynamic_cast<const VarRefNode*>(node)) {
            qore_var_t type = var_ref->getType();
            if (type == VT_LOCAL || type == VT_LOCAL_TS) {
                auto it = locals.find(var_ref->ref.id);
                if (it != locals.end()) {
                    it->second.discard(nullptr);
                    locals.erase(it);
                }
            } else if (type == VT_GLOBAL) {
                auto it = globals.find(var_ref->ref.id);
                if (it != globals.end()) {
                    it->second.discard(nullptr);
                    globals.erase(it);
                }
            } else if (type == VT_THREAD_LOCAL) {
                auto it = threadlocals.find(var_ref->ref.id);
                if (it != threadlocals.end()) {
                    it->second.discard(nullptr);
                    threadlocals.erase(it);
                }
            } else if (type == VT_CLOSURE || type == VT_LOCAL_TS) {
                auto it = closures.find(var_ref->ref.id);
                if (it != closures.end()) {
                    it->second.discard(nullptr);
                    closures.erase(it);
                }
            }
            return;
        }
        // Binary operators (., {}, [], etc.) have left and right operands.
        // The root variable is on the left side.
        if (auto* bin_op = dynamic_cast<const QoreBinaryOperatorNode<>*>(node)) {
            QoreValue left_val = bin_op->getLeft();
            node = left_val.hasNode() ? left_val.getInternalNode() : nullptr;
        } else {
            // Can't identify the root variable — fall back to full invalidation
            cleanupStoredValues(locals, nullptr);
            cleanupStoredValues(globals, nullptr);
            cleanupStoredValues(threadlocals, nullptr);
            cleanupStoredValues(closures, nullptr);
            return;
        }
    }
}

static void updateLocalVarFromLvalue(std::unordered_map<const void*, QoreValue>& locals,
        std::unordered_set<const LocalVar*>& instantiated_locals, const QoreValue& lvalue,
        const QoreValue& value, ExceptionSink* xsink,
        const std::unordered_set<const LocalVar*>* pre_instantiated = nullptr) {
    if (!lvalue.hasNode()) {
        return;
    }
    const AbstractQoreNode* node = lvalue.getInternalNode();
    const VarRefNode* var_ref = dynamic_cast<const VarRefNode*>(node);
    if (!var_ref) {
        return;
    }
    qore_var_t type = var_ref->getType();
    if ((type == VT_LOCAL || type == VT_LOCAL_TS) && var_ref->ref.id) {
        ensureLocalInstantiated(var_ref->ref.id, instantiated_locals, pre_instantiated);
        QoreValue stored = value.hasNode() ? value.refSelf() : value;
        storeValue(locals, var_ref->ref.id, stored, nullptr);
        assignLocalVarValue(var_ref->ref.id, stored, xsink);
    }
}

static QoreListNode* buildArgList(const std::unordered_map<uint32_t, QoreValue>& values,
        const std::vector<QoreIRValue>& operands, size_t start_index, ExceptionSink* xsink) {
    QoreListNode* args = new QoreListNode(autoTypeInfo);
    for (size_t i = start_index; i < operands.size(); ++i) {
        QoreValue val = getIRValue(values, operands[i]);
        QoreValue stored = val.hasNode() ? val.refSelf() : val;
        if (args->push(stored, xsink)) {
            return args;
        }
    }
    return args;
}

// Evaluate an invoke instruction by dispatching to the appropriate eval method.
// Unary/binary computation opcodes use evalUnary/evalBinary with the IR operand
// values.  Expression opcodes (Call, DotEval, LoadLValue, etc.) use evalExpr
// which evaluates the original AST expression.
//
// This distinction matters because compound assignment opcodes (AddAssignAny, etc.)
// return NOTHING when evaluated as full AST statements, but the IR expects the
// computed value as the result.  Using evalBinary with the operand values returns
// the correct computed value.
static QoreValue evalInvoke(const QoreIRInvokeInstruction* inv,
        const std::unordered_map<uint32_t, QoreValue>& values, ExceptionSink* xsink) {
    QoreIROpcode op = inv->invoke_opcode;
    switch (op) {
        // Unary computation opcodes
        case QoreIROpcode::ToBool:
        case QoreIROpcode::Not:
        case QoreIROpcode::IsNullOrNothing:
        case QoreIROpcode::UnaryPlusAny:
        case QoreIROpcode::UnaryMinusInt:
        case QoreIROpcode::UnaryMinusFloat:
        case QoreIROpcode::UnaryMinusAny: {
            QoreValue val = inv->operands.empty() ? QoreValue() : getIRValue(values, inv->operands[0]);
            return QoreIRInterpreter::evalUnary(op, val, xsink);
        }
        // Binary computation opcodes (arithmetic, bitwise, compound assignments, comparisons, etc.)
        case QoreIROpcode::AddInt:
        case QoreIROpcode::AddFloat:
        case QoreIROpcode::AddAny:
        case QoreIROpcode::SubInt:
        case QoreIROpcode::SubFloat:
        case QoreIROpcode::SubAny:
        case QoreIROpcode::MulInt:
        case QoreIROpcode::MulFloat:
        case QoreIROpcode::MulAny:
        case QoreIROpcode::DivInt:
        case QoreIROpcode::DivFloat:
        case QoreIROpcode::DivAny:
        case QoreIROpcode::ModInt:
        case QoreIROpcode::ModAny:
        case QoreIROpcode::AndInt:
        case QoreIROpcode::AndAny:
        case QoreIROpcode::OrInt:
        case QoreIROpcode::OrAny:
        case QoreIROpcode::XorInt:
        case QoreIROpcode::XorAny:
        case QoreIROpcode::ShlInt:
        case QoreIROpcode::ShlAny:
        case QoreIROpcode::ShrInt:
        case QoreIROpcode::ShrAny:
        case QoreIROpcode::ShlAssignInt:
        case QoreIROpcode::ShlAssignAny:
        case QoreIROpcode::ShrAssignInt:
        case QoreIROpcode::ShrAssignAny:
        case QoreIROpcode::AddAssignInt:
        case QoreIROpcode::AddAssignFloat:
        case QoreIROpcode::AddAssignAny:
        case QoreIROpcode::SubAssignInt:
        case QoreIROpcode::SubAssignFloat:
        case QoreIROpcode::SubAssignAny:
        case QoreIROpcode::MulAssignInt:
        case QoreIROpcode::MulAssignFloat:
        case QoreIROpcode::MulAssignAny:
        case QoreIROpcode::DivAssignInt:
        case QoreIROpcode::DivAssignFloat:
        case QoreIROpcode::DivAssignAny:
        case QoreIROpcode::ModAssignInt:
        case QoreIROpcode::ModAssignAny:
        case QoreIROpcode::AndAssignInt:
        case QoreIROpcode::AndAssignAny:
        case QoreIROpcode::OrAssignInt:
        case QoreIROpcode::OrAssignAny:
        case QoreIROpcode::XorAssignInt:
        case QoreIROpcode::XorAssignAny:
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
        case QoreIROpcode::FoldlAny:
        case QoreIROpcode::FoldlInt:
        case QoreIROpcode::FoldlFloat:
        case QoreIROpcode::FoldrAny:
        case QoreIROpcode::FoldrInt:
        case QoreIROpcode::FoldrFloat:
        case QoreIROpcode::FoldlSumInt:
        case QoreIROpcode::FoldlSumFloat:
        case QoreIROpcode::FoldlProdInt:
        case QoreIROpcode::FoldlProdFloat:
        case QoreIROpcode::FoldlDiffInt:
        case QoreIROpcode::FoldlDiffFloat:
        case QoreIROpcode::FoldlMinInt:
        case QoreIROpcode::FoldlMinFloat:
        case QoreIROpcode::FoldlMaxInt:
        case QoreIROpcode::FoldlMaxFloat:
        case QoreIROpcode::MapAny:
        case QoreIROpcode::MapInt:
        case QoreIROpcode::MapFloat:
        case QoreIROpcode::MapScaleInt:
        case QoreIROpcode::MapScaleFloat:
        case QoreIROpcode::MapOffsetInt:
        case QoreIROpcode::MapOffsetFloat:
        case QoreIROpcode::MapSquareInt:
        case QoreIROpcode::MapSquareFloat:
        case QoreIROpcode::SelectAny:
        case QoreIROpcode::SelectInt:
        case QoreIROpcode::SelectFloat:
        case QoreIROpcode::SelectPositiveInt:
        case QoreIROpcode::SelectPositiveFloat:
        case QoreIROpcode::SelectNonZeroInt:
        case QoreIROpcode::SelectNonZeroFloat:
        case QoreIROpcode::FusedMapSelectScalePositiveInt:
        case QoreIROpcode::FusedMapSelectScalePositiveFloat:
        case QoreIROpcode::FusedMapSelectOffsetPositiveInt:
        case QoreIROpcode::FusedMapSelectOffsetPositiveFloat:
        case QoreIROpcode::FusedMapSelectSquarePositiveInt:
        case QoreIROpcode::FusedMapSelectSquarePositiveFloat:
        case QoreIROpcode::RangeAny:
        case QoreIROpcode::RangeInt:
        case QoreIROpcode::RangeFloat:
        case QoreIROpcode::RangeDate: {
            QoreValue left = inv->operands.size() > 0 ? getIRValue(values, inv->operands[0]) : QoreValue();
            QoreValue right = inv->operands.size() > 1 ? getIRValue(values, inv->operands[1]) : QoreValue();
            return QoreIRInterpreter::evalBinary(op, left, right, xsink);
        }
        // Regex match/nmatch: use operand value instead of AST expression's left operand
        case QoreIROpcode::RegexMatchAny:
        case QoreIROpcode::RegexMatchBool:
        case QoreIROpcode::RegexNMatchBool: {
            if (!inv->operands.empty()) {
                QoreValue str_val = getIRValue(values, inv->operands[0]);
                QoreRegex* regex = nullptr;
                if (auto* match_node = dynamic_cast<const QoreRegexMatchOperatorNode*>(
                        inv->expr.getInternalNode())) {
                    regex = match_node->getRegex();
                } else if (auto* nmatch_node = dynamic_cast<const QoreRegexNMatchOperatorNode*>(
                        inv->expr.getInternalNode())) {
                    regex = nmatch_node->getRegex();
                }
                if (regex) {
                    QoreStringNodeValueHelper str(str_val);
                    bool match = regex->exec(*str, xsink);
                    if (op == QoreIROpcode::RegexNMatchBool) {
                        match = !match;
                    }
                    return QoreValue(match);
                }
            }
            return QoreIRInterpreter::evalExpr(op, inv->expr, xsink);
        }
        // Regex extract: use operand value instead of re-evaluating subject expression
        case QoreIROpcode::RegexExtractAny:
        case QoreIROpcode::RegexExtractList: {
            if (!inv->operands.empty()) {
                QoreValue str_val = getIRValue(values, inv->operands[0]);
                if (auto* extract_node = dynamic_cast<const QoreRegexExtractOperatorNode*>(
                        inv->expr.getInternalNode())) {
                    QoreRegex* regex = extract_node->getRegex();
                    if (regex) {
                        QoreStringNodeValueHelper str(str_val);
                        return QoreValue(regex->extractSubstrings(*str, xsink));
                    }
                }
            }
            return QoreIRInterpreter::evalExpr(op, inv->expr, xsink);
        }
        // Call-type opcodes: use pre-evaluated operands to avoid double-evaluation
        case QoreIROpcode::Call:
        case QoreIROpcode::CallIndirect:
        case QoreIROpcode::CallMethod:
        case QoreIROpcode::CallStatic: {
            if (!inv->operands.empty()) {
                const ParseNode* parse_node = nullptr;
                if (inv->expr.hasNode()) {
                    parse_node = dynamic_cast<const ParseNode*>(inv->expr.getInternalNode());
                }
                const QoreProgramLocation* loc = parse_node ? parse_node->loc : nullptr;
                size_t arg_start = (op == QoreIROpcode::CallIndirect) ? 1 : 0;
                QoreListNode* arg_list = buildArgList(values, inv->operands, arg_start, xsink);
                if (xsink && *xsink) {
                    if (arg_list) {
                        arg_list->deref(xsink);
                    }
                    return QoreValue();
                }
                bool used_operands = false;
                QoreValue res;
                if (op == QoreIROpcode::Call) {
                    if (auto* call = dynamic_cast<const FunctionCallNode*>(
                            inv->expr.getInternalNode())) {
                        QoreValue call_expr(new FunctionCallNode(*call, arg_list));
                        ValueHolder call_holder(call_expr, nullptr);
                        res = QoreIRInterpreter::evalExpr(op, call_expr, xsink);
                        used_operands = true;
                    }
                } else if (op == QoreIROpcode::CallMethod) {
                    if (auto* call = dynamic_cast<const SelfFunctionCallNode*>(
                            inv->expr.getInternalNode())) {
                        QoreValue call_expr(new SelfFunctionCallNode(*call, arg_list));
                        ValueHolder call_holder(call_expr, nullptr);
                        res = QoreIRInterpreter::evalExpr(op, call_expr, xsink);
                        used_operands = true;
                    }
                } else if (op == QoreIROpcode::CallStatic) {
                    if (auto* call = dynamic_cast<const StaticMethodCallNode*>(
                            inv->expr.getInternalNode())) {
                        QoreValue call_expr(new StaticMethodCallNode(*call, arg_list));
                        ValueHolder call_holder(call_expr, nullptr);
                        res = QoreIRInterpreter::evalExpr(op, call_expr, xsink);
                        used_operands = true;
                    }
                } else {
                    // CallIndirect
                    if (auto* call = dynamic_cast<const CallReferenceCallNode*>(
                            inv->expr.getInternalNode())) {
                        QoreValue exp = call->getExp();
                        if (exp.hasNode()) {
                            exp = exp.refSelf();
                        }
                        QoreValue call_expr(new CallReferenceCallNode(loc, exp, arg_list));
                        ValueHolder call_holder(call_expr, nullptr);
                        res = QoreIRInterpreter::evalExpr(op, call_expr, xsink);
                        used_operands = true;
                    }
                }
                if (!used_operands && arg_list) {
                    arg_list->deref(xsink);
                }
                if (used_operands) {
                    return res;
                }
            }
            // Fall through to evalExpr if no operands or cast failed
            return QoreIRInterpreter::evalExpr(op, inv->expr, xsink);
        }

        // DotEval opcodes: use pre-evaluated base to avoid double-evaluation
        case QoreIROpcode::DotEvalAny:
        case QoreIROpcode::DotEvalInt:
        case QoreIROpcode::DotEvalFloat:
        case QoreIROpcode::DotEvalString:
        case QoreIROpcode::DotEvalDate:
        case QoreIROpcode::DotEvalList:
        case QoreIROpcode::DotEvalHash:
        case QoreIROpcode::DotEvalObject: {
            if (!inv->operands.empty()) {
                QoreValue base = getIRValue(values, inv->operands[0]);
                auto* dot_eval = dynamic_cast<const QoreDotEvalOperatorNode*>(
                    inv->expr.getInternalNode());
                if (dot_eval) {
                    return dot_eval->evalWithBase(base, xsink);
                }
            }
            return QoreIRInterpreter::evalExpr(op, inv->expr, xsink);
        }

        // Everything else (LoadLValue, expression ops, etc.)
        // evaluated through the original AST expression
        default:
            return QoreIRInterpreter::evalExpr(op, inv->expr, xsink);
    }
}

// IR on_block_exit handler entry: records the type and code block for deferred execution
struct IROnBlockExitHandler {
    obe_type_e type;
    StatementBlock* code;
};

// Execute on_block_exit handlers in reverse order (LIFO), matching the AST's
// StatementBlock::execIntern() semantics.  Returns the last non-zero return code, if any.
static int executeOnBlockExitHandlers(std::vector<IROnBlockExitHandler>& handlers, ExceptionSink* xsink) {
    if (handlers.empty()) {
        return 0;
    }
    ExceptionSink obe_xsink;
    int nrc = 0;
    bool error = xsink && xsink->isException();
    // Execute in reverse order (most recently registered first)
    for (int i = (int)handlers.size() - 1; i >= 0; --i) {
        obe_type_e type = handlers[i].type;
        if (type == OBE_Unconditional || (!error && type == OBE_Success) || (error && type == OBE_Error)) {
            if (handlers[i].code) {
                {
                    // Instantiate exception for on_error blocks as an implicit arg
                    std::unique_ptr<SingleArgvContextHelper> argv_helper;
                    std::unique_ptr<CatchExceptionHelper> ex_helper;
                    if (type == OBE_Error && xsink) {
                        QoreException* except = xsink->getException();
                        if (except) {
                            ex_helper.reset(new CatchExceptionHelper(except));
                            argv_helper.reset(new SingleArgvContextHelper(except->makeExceptionObject(), xsink));
                        }
                    }
                    QoreValue rv;
                    nrc = handlers[i].code->exec(rv, &obe_xsink);
                    if (type == OBE_Error) {
                        if (qore_es_private::get(obe_xsink)->rethrown) {
                            if (xsink) {
                                xsink->clear();
                            }
                        }
                    }
                }
                if (obe_xsink) {
                    if (xsink) {
                        xsink->assimilate(obe_xsink);
                    }
                    if (!error) {
                        error = true;
                    }
                }
            }
        }
    }
    return nrc;
}

bool QoreIRInterpreter::execute(const QoreIRFunction& func, QoreValue& return_value, ExceptionSink* xsink,
        std::vector<std::string>* cleanup_log, const std::vector<QoreValue>* args,
        const std::vector<QoreValue>* closure, const std::unordered_set<const LocalVar*>* pre_instantiated) {
    std::unordered_map<uint32_t, QoreValue> values;
    std::vector<uint32_t> cleanup;
    std::unordered_map<const void*, QoreValue> locals;
    std::unordered_set<const LocalVar*> instantiated_locals;
    std::unordered_map<const void*, QoreValue> globals;
    std::unordered_map<const void*, QoreValue> threadlocals;
    std::unordered_map<const void*, QoreValue> closures;
    // on_block_exit handlers collected during IR execution
    std::vector<IROnBlockExitHandler> on_block_exit_handlers;
    struct LocalInstantiationCleanup {
        std::unordered_set<const LocalVar*>& locals;
        ExceptionSink* xsink;
        const std::unordered_set<const LocalVar*>* pre_instantiated;
        ~LocalInstantiationCleanup() {
            cleanupInstantiatedLocals(locals, xsink, pre_instantiated);
        }
    } local_cleanup{instantiated_locals, xsink, pre_instantiated};
    if (func.blocks.empty()) {
        if (xsink) {
            xsink->raiseException("IR-EXEC-ERROR", "function has no basic blocks");
        }
        return false;
    }
    QoreIRBasicBlock* block = func.blocks.front().get();
    QoreIRBasicBlock* prev_block = nullptr;
    size_t ip = 0;

    // OSR: loop iteration counter for loop-aware JIT promotion
    uint32_t loop_iterations = 0;
    const uint32_t osr_threshold = static_cast<uint32_t>(QoreJIT::getJITThreshold());

    while (block) {
        if (ip >= block->instructions.size()) {
            if (xsink) {
                xsink->raiseException("IR-EXEC-ERROR", "fell off end of basic block");
            }
            cleanupValues(values, cleanup, xsink, true, cleanup_log);
            return false;
        }
        while (ip < block->instructions.size() && block->instructions[ip]->opcode == QoreIROpcode::Phi) {
            auto* phi = dynamic_cast<QoreIRPhiInstruction*>(block->instructions[ip].get());
            if (!phi) {
                if (xsink) {
                    xsink->raiseException("IR-EXEC-ERROR", "phi instruction cast failed");
                }
                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                cleanupStoredValues(locals, nullptr);
                cleanupStoredValues(globals, nullptr);
                cleanupStoredValues(threadlocals, nullptr);
                cleanupStoredValues(closures, nullptr);
                return false;
            }
            if (!prev_block) {
                if (xsink) {
                    xsink->raiseException("IR-EXEC-ERROR", "phi has no predecessor");
                }
                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                cleanupStoredValues(locals, nullptr);
                cleanupStoredValues(globals, nullptr);
                cleanupStoredValues(threadlocals, nullptr);
                cleanupStoredValues(closures, nullptr);
                return false;
            }
            QoreIRValue incoming_value;
            bool found = false;
            for (const auto& inc : phi->incoming) {
                if (inc.block == prev_block) {
                    incoming_value = inc.value;
                    found = true;
                    break;
                }
            }
            if (!found) {
                if (xsink) {
                    xsink->raiseException("IR-EXEC-ERROR", "phi missing predecessor incoming");
                }
                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                cleanupStoredValues(locals, nullptr);
                cleanupStoredValues(globals, nullptr);
                cleanupStoredValues(threadlocals, nullptr);
                cleanupStoredValues(closures, nullptr);
                return false;
            }
            QoreValue val = getIRValue(values, incoming_value);
            QoreValue stored = val.hasNode() ? val.refSelf() : val;
            values[phi->result.id] = stored;
            if (stored.hasNode()) {
                cleanup.push_back(phi->result.id);
            }
            ++ip;
        }
        if (ip >= block->instructions.size()) {
            if (xsink) {
                xsink->raiseException("IR-EXEC-ERROR", "fell off end of basic block after phi");
            }
            cleanupValues(values, cleanup, xsink, true, cleanup_log);
            cleanupStoredValues(locals, nullptr);
            cleanupStoredValues(globals, nullptr);
            cleanupStoredValues(threadlocals, nullptr);
            cleanupStoredValues(closures, nullptr);
            return false;
        }
        QoreIRInstruction* inst = block->instructions[ip].get();
        switch (inst->opcode) {
            case QoreIROpcode::ConstInt: {
                auto* cinst = static_cast<QoreIRConstInstruction*>(inst);
                values[cinst->result.id] = QoreValue(cinst->constant.int_value);
                ++ip;
                break;
            }
            case QoreIROpcode::ConstFloat: {
                auto* cinst = static_cast<QoreIRConstInstruction*>(inst);
                values[cinst->result.id] = QoreValue(cinst->constant.float_value);
                ++ip;
                break;
            }
            case QoreIROpcode::ConstBool: {
                auto* cinst = static_cast<QoreIRConstInstruction*>(inst);
                values[cinst->result.id] = QoreValue(cinst->constant.bool_value);
                ++ip;
                break;
            }
            case QoreIROpcode::ConstNothing: {
                auto* cinst = static_cast<QoreIRConstInstruction*>(inst);
                values[cinst->result.id] = QoreValue();
                ++ip;
                break;
            }
            case QoreIROpcode::ConstNull: {
                auto* cinst = static_cast<QoreIRConstInstruction*>(inst);
                values[cinst->result.id] = QoreValue::makeNull();
                ++ip;
                break;
            }
            case QoreIROpcode::ConstString: {
                auto* cinst = static_cast<QoreIRConstInstruction*>(inst);
                QoreStringNode* str = new QoreStringNode(cinst->constant.string_value);
                values[cinst->result.id] = QoreValue(str);
                cleanup.push_back(cinst->result.id);
                ++ip;
                break;
            }
            case QoreIROpcode::ConstDate: {
                auto* cinst = static_cast<QoreIRConstInstruction*>(inst);
                DateTimeNode* dt;
                if (cinst->constant.date_is_relative) {
                    dt = new DateTimeNode(true);
                    dt->setRelativeDateSeconds(cinst->constant.date_microseconds / 1000000,
                        static_cast<int>(cinst->constant.date_microseconds % 1000000));
                } else {
                    int64_t epoch_seconds = cinst->constant.date_microseconds / 1000000;
                    int ms = static_cast<int>((cinst->constant.date_microseconds % 1000000) / 1000);
                    dt = new DateTimeNode(epoch_seconds, ms);
                }
                values[cinst->result.id] = QoreValue(dt);
                cleanup.push_back(cinst->result.id);
                ++ip;
                break;
            }
            case QoreIROpcode::MakeList: {
                ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), xsink);
                const QoreTypeInfo* vtype = nullptr;
                bool vcommon = false;
                for (const auto& operand : inst->operands) {
                    QoreValue value = getIRValue(values, operand);
                    QoreValue stored = value.hasNode() ? value.refSelf() : value;
                    const QoreTypeInfo* vt = stored.getTypeInfo();
                    if (!vtype) {
                        vtype = vt;
                        vcommon = true;
                    } else if (vcommon && !QoreTypeInfo::matchCommonType(vtype, vt)) {
                        vcommon = false;
                    }
                    if (list->push(stored, xsink)) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupStoredValues(locals, nullptr);
                        cleanupStoredValues(globals, nullptr);
                        cleanupStoredValues(threadlocals, nullptr);
                        cleanupStoredValues(closures, nullptr);
                        return false;
                    }
                }
                if (!vtype || vtype == anyTypeInfo || !vcommon) {
                    vtype = autoTypeInfo;
                }
                qore_list_private::get(*list)->complexTypeInfo = qore_get_complex_list_type(vtype);
                values[inst->result.id] = QoreValue(list.release());
                cleanup.push_back(inst->result.id);
                ++ip;
                break;
            }
            case QoreIROpcode::MakeHash: {
                ReferenceHolder<QoreHashNode> hash(new QoreHashNode(autoTypeInfo), xsink);
                const QoreTypeInfo* vtype = nullptr;
                bool vcommon = false;
                if (inst->operands.size() % 2 != 0) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "make.hash requires even operands");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                for (size_t i = 0; i < inst->operands.size(); i += 2) {
                    QoreValue key_val = getIRValue(values, inst->operands[i]);
                    QoreValue value = getIRValue(values, inst->operands[i + 1]);
                    QoreValue stored = value.hasNode() ? value.refSelf() : value;
                    const QoreTypeInfo* vt = stored.getTypeInfo();
                    if (!i) {
                        vtype = vt;
                        vcommon = true;
                    } else if (vcommon && !QoreTypeInfo::matchCommonType(vtype, vt)) {
                        vcommon = false;
                    }
                    QoreStringValueHelper key(key_val);
                    hash->setKeyValue(key->c_str(), stored, xsink);
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupStoredValues(locals, nullptr);
                        cleanupStoredValues(globals, nullptr);
                        cleanupStoredValues(threadlocals, nullptr);
                        cleanupStoredValues(closures, nullptr);
                        return false;
                    }
                }
                if (!vtype || vtype == anyTypeInfo) {
                    vtype = autoTypeInfo;
                }
                qore_hash_private::get(*hash)->complexTypeInfo = qore_get_complex_hash_type(vtype);
                values[inst->result.id] = QoreValue(hash.release());
                cleanup.push_back(inst->result.id);
                ++ip;
                break;
            }
            case QoreIROpcode::Incref: {
                if (inst->operands.empty()) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "incref missing operand");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                QoreIRValue id = inst->operands.front();
                QoreValue val = getIRValue(values, id);
                val.ref();
                cleanup.push_back(id.id);
                ++ip;
                break;
            }
            case QoreIROpcode::Decref:
            case QoreIROpcode::DecrefNoThrow: {
                if (inst->operands.empty()) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "decref missing operand");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                QoreIRValue id = inst->operands.front();
                removeCleanupEntry(cleanup, id.id);
                QoreValue val = getIRValue(values, id);
                QoreValue temp = val;
                temp.discard(inst->opcode == QoreIROpcode::DecrefNoThrow ? nullptr : xsink);
                ++ip;
                break;
            }
            case QoreIROpcode::Invoke: {
                auto* inv = dynamic_cast<QoreIRInvokeInstruction*>(inst);
                if (!inv) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "invoke instruction cast failed");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                QoreValue res = evalInvoke(inv, values, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    prev_block = block;
                    block = inv->exception_target;
                    ip = 0;
                    break;
                }
                cleanupStoredValues(locals, nullptr);
                cleanupStoredValues(globals, nullptr);
                cleanupStoredValues(threadlocals, nullptr);
                cleanupStoredValues(closures, nullptr);
                values[inv->result.id] = res;
                if (res.hasNode()) {
                    cleanup.push_back(inv->result.id);
                }
                prev_block = block;
                block = inv->normal_target;
                ip = 0;
                break;
            }
            case QoreIROpcode::LandingPad:
                ++ip;
                break;
            case QoreIROpcode::CatchException: {
                if (!xsink || !*xsink) {
                    values[inst->result.id] = QoreValue();
                    ++ip;
                    break;
                }
                QoreHashNode* info = xsink->getExceptionInfo();
                values[inst->result.id] = QoreValue(info);
                cleanup.push_back(inst->result.id);
                xsink->clear();
                ++ip;
                break;
            }
            case QoreIROpcode::Br: {
                auto* br = static_cast<QoreIRBranchInstruction*>(inst);
                prev_block = block;
                block = br->target;
                ip = 0;
                // OSR: count back-edges to loop headers
                if (block->is_loop_header) {
                    ++loop_iterations;
                    if (!func.osr_jit_requested && loop_iterations >= osr_threshold) {
                        func.osr_jit_requested = true;
                        printd(2, "QoreIRInterpreter: OSR triggered for '%s' "
                            "(loop iterations=%u)\n", func.name.c_str(), loop_iterations);
                    }
                }
                break;
            }
            case QoreIROpcode::BrIf: {
                auto* br = static_cast<QoreIRBranchIfInstruction*>(inst);
                QoreValue cond = getIRValue(values, br->condition);
                prev_block = block;
                block = cond.getAsBool() ? br->true_target : br->false_target;
                ip = 0;
                // OSR: count back-edges to loop headers
                if (block->is_loop_header) {
                    ++loop_iterations;
                    if (!func.osr_jit_requested && loop_iterations >= osr_threshold) {
                        func.osr_jit_requested = true;
                        printd(2, "QoreIRInterpreter: OSR triggered for '%s' "
                            "(loop iterations=%u)\n", func.name.c_str(), loop_iterations);
                    }
                }
                break;
            }
            case QoreIROpcode::LoadLocal: {
                auto* local_inst = static_cast<QoreIRLocalInstruction*>(inst);
                ensureLocalInstantiated(local_inst->local, instantiated_locals, pre_instantiated);
                auto it = locals.find(local_inst->local);
                QoreValue val;
                if (it != locals.end()) {
                    val = it->second;
                } else if (local_inst->local) {
                    bool needs_deref = true;
                    val = local_inst->local->eval(needs_deref, xsink);
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupStoredValues(locals, nullptr);
                        cleanupStoredValues(globals, nullptr);
                        cleanupStoredValues(threadlocals, nullptr);
                        cleanupStoredValues(closures, nullptr);
                        return false;
                    }
                    storeValue(locals, local_inst->local, val, nullptr);
                }
                QoreValue out = val.hasNode() ? val.refSelf() : val;
                values[local_inst->result.id] = out;
                if (out.hasNode()) {
                    cleanup.push_back(local_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::LoadArg: {
                int64 idx = 0;
                if (!inst->operands.empty()) {
                    QoreValue idx_val = getIRValue(values, inst->operands[0]);
                    idx = idx_val.getAsBigInt();
                }
                QoreValue val;
                if (args && idx >= 0 && static_cast<size_t>(idx) < args->size()) {
                    val = (*args)[static_cast<size_t>(idx)];
                }
                QoreValue out = val.hasNode() ? val.refSelf() : val;
                values[inst->result.id] = out;
                if (out.hasNode()) {
                    cleanup.push_back(inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::LoadClosure: {
                auto* local_inst = static_cast<QoreIRLocalInstruction*>(inst);
                QoreValue val;
                if (!inst->operands.empty() && closure) {
                    QoreValue idx_val = getIRValue(values, inst->operands[0]);
                    int64 idx = idx_val.getAsBigInt();
                    if (idx >= 0 && static_cast<size_t>(idx) < closure->size()) {
                        val = (*closure)[static_cast<size_t>(idx)];
                    }
                } else {
                    auto it = closures.find(local_inst->local);
                    if (it != closures.end()) {
                        val = it->second;
                    } else if (local_inst->local) {
                        // Read from the runtime closure environment (set by
                        // ThreadSafeLocalVarRuntimeEnvironmentHelper in
                        // UserClosureFunction::evalClosure)
                        ClosureVarValue* cv = thread_get_runtime_closure_var(local_inst->local);
                        if (cv) {
                            val = cv->eval(xsink);
                            if (xsink && *xsink) {
                                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                                cleanupStoredValues(locals, nullptr);
                                cleanupStoredValues(globals, nullptr);
                                cleanupStoredValues(threadlocals, nullptr);
                                cleanupStoredValues(closures, nullptr);
                                return false;
                            }
                            storeValue(closures, local_inst->local, val, nullptr);
                        }
                    }
                }
                QoreValue out = val.hasNode() ? val.refSelf() : val;
                values[local_inst->result.id] = out;
                if (out.hasNode()) {
                    cleanup.push_back(local_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::StoreLocal: {
                auto* local_inst = static_cast<QoreIRLocalInstruction*>(inst);
                if (local_inst->operands.empty()) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "store.local missing operand");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                ensureLocalInstantiated(local_inst->local, instantiated_locals, pre_instantiated);
                QoreValue val = getIRValue(values, local_inst->operands.front());
                storeValue(locals, local_inst->local, val, xsink);
                assignLocalVarValue(local_inst->local, val, xsink);
                if (xsink && *xsink) {
                    if (inst->exception_target) {
                        prev_block = block;
                        block = inst->exception_target;
                        ip = 0;
                        break;
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                ++ip;
                break;
            }
            case QoreIROpcode::StoreClosure: {
                auto* local_inst = static_cast<QoreIRLocalInstruction*>(inst);
                if (local_inst->operands.empty()) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "store.closure missing operand");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                QoreValue val = getIRValue(values, local_inst->operands.front());
                storeValue(closures, local_inst->local, val, xsink);
                // Write-through: update the actual closure variable so changes
                // are visible outside the IR interpreter's local cache.
                assignClosureVarValue(local_inst->local, val, xsink);
                if (xsink && *xsink) {
                    if (inst->exception_target) {
                        prev_block = block;
                        block = inst->exception_target;
                        ip = 0;
                        break;
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                ++ip;
                break;
            }
            case QoreIROpcode::LoadGlobal: {
                auto* var_inst = static_cast<QoreIRVarInstruction*>(inst);
                QoreValue out;
                auto it = globals.find(var_inst->var);
                if (it != globals.end()) {
                    QoreValue val = it->second;
                    out = val.hasNode() ? val.refSelf() : val;
                } else {
                    // Read from the actual global variable when not in the local cache
                    out = var_inst->var->eval();
                }
                values[var_inst->result.id] = out;
                if (out.hasNode()) {
                    cleanup.push_back(var_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::StoreGlobal: {
                auto* var_inst = static_cast<QoreIRVarInstruction*>(inst);
                if (var_inst->operands.empty()) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "store.global missing operand");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                QoreValue val = getIRValue(values, var_inst->operands.front());
                storeValue(globals, var_inst->var, val, xsink);
                // Write-through: update the actual global variable so changes
                // are visible outside the IR interpreter's local cache.
                assignGlobalVarValue(var_inst->var, val, xsink);
                if (xsink && *xsink) {
                    if (inst->exception_target) {
                        prev_block = block;
                        block = inst->exception_target;
                        ip = 0;
                        break;
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                ++ip;
                break;
            }
            case QoreIROpcode::LoadThreadLocal: {
                auto* var_inst = static_cast<QoreIRVarInstruction*>(inst);
                QoreValue out;
                auto it = threadlocals.find(var_inst->var);
                if (it != threadlocals.end()) {
                    QoreValue val = it->second;
                    out = val.hasNode() ? val.refSelf() : val;
                } else {
                    // Read from the actual thread-local variable when not in the local cache
                    out = var_inst->var->eval();
                }
                values[var_inst->result.id] = out;
                if (out.hasNode()) {
                    cleanup.push_back(var_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::StoreThreadLocal: {
                auto* var_inst = static_cast<QoreIRVarInstruction*>(inst);
                if (var_inst->operands.empty()) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "store.threadlocal missing operand");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                QoreValue val = getIRValue(values, var_inst->operands.front());
                storeValue(threadlocals, var_inst->var, val, xsink);
                // Write-through: update the actual thread-local variable.
                assignGlobalVarValue(var_inst->var, val, xsink);
                if (xsink && *xsink) {
                    if (inst->exception_target) {
                        prev_block = block;
                        block = inst->exception_target;
                        ip = 0;
                        break;
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                ++ip;
                break;
            }
            case QoreIROpcode::Debug: {
                auto* debug_inst = static_cast<QoreIRDebugInstruction*>(inst);
                QoreValue stmt_return;
                int rc = QoreIRInterpreter::execStatement(QoreIROpcode::Debug, debug_inst->stmt,
                    stmt_return, xsink);
                if (rc || (xsink && *xsink)) {
                    if (inst->exception_target && xsink && *xsink) {
                        prev_block = block;
                        block = inst->exception_target;
                        ip = 0;
                        break;
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                cleanupStoredValues(locals, nullptr);
                cleanupStoredValues(globals, nullptr);
                cleanupStoredValues(threadlocals, nullptr);
                cleanupStoredValues(closures, nullptr);
                ++ip;
                break;
            }
            case QoreIROpcode::Assert: {
                auto* assert_inst = static_cast<QoreIRAssertInstruction*>(inst);
                QoreValue stmt_return;
                int rc = QoreIRInterpreter::execStatement(QoreIROpcode::Assert, assert_inst->stmt,
                    stmt_return, xsink);
                if (rc || (xsink && *xsink)) {
                    if (inst->exception_target && xsink && *xsink) {
                        prev_block = block;
                        block = inst->exception_target;
                        ip = 0;
                        break;
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                cleanupStoredValues(locals, nullptr);
                cleanupStoredValues(globals, nullptr);
                cleanupStoredValues(threadlocals, nullptr);
                cleanupStoredValues(closures, nullptr);
                ++ip;
                break;
            }
            case QoreIROpcode::Context: {
                auto* context_inst = static_cast<QoreIRContextInstruction*>(inst);
                QoreValue stmt_return;
                int rc = QoreIRInterpreter::execStatement(QoreIROpcode::Context, context_inst->stmt,
                    stmt_return, xsink);
                if (rc || (xsink && *xsink)) {
                    if (inst->exception_target && xsink && *xsink) {
                        prev_block = block;
                        block = inst->exception_target;
                        ip = 0;
                        break;
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                cleanupStoredValues(locals, nullptr);
                cleanupStoredValues(globals, nullptr);
                cleanupStoredValues(threadlocals, nullptr);
                cleanupStoredValues(closures, nullptr);
                ++ip;
                break;
            }
            case QoreIROpcode::Summarize: {
                auto* summarize_inst = static_cast<QoreIRSummarizeInstruction*>(inst);
                QoreValue stmt_return;
                int rc = QoreIRInterpreter::execStatement(QoreIROpcode::Summarize, summarize_inst->stmt,
                    stmt_return, xsink);
                if (rc || (xsink && *xsink)) {
                    if (inst->exception_target && xsink && *xsink) {
                        prev_block = block;
                        block = inst->exception_target;
                        ip = 0;
                        break;
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                cleanupStoredValues(locals, nullptr);
                cleanupStoredValues(globals, nullptr);
                cleanupStoredValues(threadlocals, nullptr);
                cleanupStoredValues(closures, nullptr);
                ++ip;
                break;
            }
            case QoreIROpcode::Foreach: {
                auto* foreach_inst = static_cast<QoreIRForeachInstruction*>(inst);
                if (!foreach_inst->stmt) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "foreach requires a statement");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                QoreValue stmt_return;
                int rc = const_cast<ForEachStatement*>(foreach_inst->stmt)->exec(stmt_return, xsink);
                if (rc || (xsink && *xsink)) {
                    if (inst->exception_target && xsink && *xsink) {
                        prev_block = block;
                        block = inst->exception_target;
                        ip = 0;
                        break;
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                cleanupStoredValues(locals, nullptr);
                cleanupStoredValues(globals, nullptr);
                cleanupStoredValues(threadlocals, nullptr);
                cleanupStoredValues(closures, nullptr);
                ++ip;
                break;
            }
            case QoreIROpcode::IteratorCreate: {
                auto* iter_inst = static_cast<QoreIRIteratorCreateInstruction*>(inst);
                QoreValue iterable = getIRValue(values, iter_inst->iterable);
                FunctionalOperator::FunctionalValueType value_type;
                FunctionalOperatorInterface* iter = nullptr;
                if (iter_inst->iterator_func) {
                    iter = iter_inst->iterator_func->getFunctionalIterator(value_type, xsink);
                } else {
                    iter = FunctionalOperatorInterface::getFunctionalIterator(value_type, iterable, true,
                        "foreach statement", xsink);
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                // Store iterator pointer as int64_t in result
                // A nullptr means the iterable was empty (nothing)
                values[iter_inst->result.id] = QoreValue(reinterpret_cast<int64_t>(iter));
                ++ip;
                break;
            }
            case QoreIROpcode::IteratorNext: {
                auto* iter_inst = static_cast<QoreIRIteratorNextInstruction*>(inst);
                QoreValue iter_val = getIRValue(values, iter_inst->iterator);
                FunctionalOperatorInterface* iter = reinterpret_cast<FunctionalOperatorInterface*>(
                    iter_val.getAsBigInt());
                if (!iter) {
                    // Empty iterator - branch to done
                    prev_block = block;
                    block = iter_inst->done_target;
                    ip = 0;
                    break;
                }
                ValueOptionalRefHolder val(xsink);
                bool done = iter->getNext(val, xsink);
                if (xsink && *xsink) {
                    delete iter;
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                if (done) {
                    // Iterator exhausted - clean up and branch to done
                    delete iter;
                    // Clear the iterator value so we don't double-delete
                    values[iter_inst->iterator.id] = QoreValue(static_cast<int64_t>(0));
                    prev_block = block;
                    block = iter_inst->done_target;
                    ip = 0;
                } else {
                    // Store current value in result and branch to continue
                    values[iter_inst->result.id] = val.takeReferencedValue();
                    prev_block = block;
                    block = iter_inst->continue_target;
                    ip = 0;
                }
                break;
            }
            case QoreIROpcode::OnBlockExit: {
                auto* obe_inst = static_cast<QoreIROnBlockExitInstruction*>(inst);
                if (!obe_inst->stmt) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "on-block-exit requires a statement");
                    }
                    executeOnBlockExitHandlers(on_block_exit_handlers, xsink);
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                // Record the handler for deferred execution at block/function exit.
                // Don't call exec() here - the AST's exec() calls advance_on_block_exit()
                // which requires the thread on_block_exit stack to be set up by StatementBlock,
                // but the IR interpreter doesn't go through StatementBlock::execIntern().
                on_block_exit_handlers.push_back({obe_inst->stmt->getType(), obe_inst->stmt->getCode()});
                ++ip;
                break;
            }
            case QoreIROpcode::ThreadExit:
                if (xsink) {
                    xsink->raiseThreadExit();
                }
                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                cleanupStoredValues(locals, nullptr);
                cleanupStoredValues(globals, nullptr);
                cleanupStoredValues(threadlocals, nullptr);
                cleanupStoredValues(closures, nullptr);
                return false;
            case QoreIROpcode::GuardInt:
            case QoreIROpcode::GuardFloat:
            case QoreIROpcode::GuardType:
            case QoreIROpcode::GuardNotNothing: {
                auto* guard_inst = static_cast<QoreIRGuardInstruction*>(inst);
                if (guard_inst->operands.empty()) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "guard missing operand");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                QoreValue value = getIRValue(values, guard_inst->operands.front());
                // Record type profile for this guard point
                if (guard_inst->guard_id < func.guard_profile_count) {
                    func.guard_profiles[guard_inst->guard_id].record(value);
                }
                if (!guardPredicate(inst->opcode, value, guard_inst->type_info)) {
                    if (guard_inst->deopt_target) {
                        // Guard has a deopt target (e.g. catch block) — route to it
                        if (xsink) {
                            xsink->raiseException("IR-EXEC-GUARD-FAIL",
                                "guard type check failed — deoptimizing");
                        }
                        prev_block = block;
                        block = guard_inst->deopt_target;
                        ip = 0;
                        break;
                    }
                    // No deopt target — guard failure is speculative, continue silently
                }
                ++ip;
                break;
            }
            case QoreIROpcode::ToBool:
            case QoreIROpcode::Not:
            case QoreIROpcode::IsNullOrNothing:
            case QoreIROpcode::UnaryPlusAny:
            case QoreIROpcode::UnaryMinusInt:
            case QoreIROpcode::UnaryMinusFloat:
            case QoreIROpcode::UnaryMinusAny: {
                if (inst->operands.size() < 1) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "unary op missing operand");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                QoreValue val = getIRValue(values, inst->operands[0]);
                QoreValue res = QoreIRInterpreter::evalUnary(inst->opcode, val, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                values[inst->result.id] = res;
                if (res.hasNode()) {
                    cleanup.push_back(inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::AddInt:
            case QoreIROpcode::AddFloat:
            case QoreIROpcode::AddAny:
            case QoreIROpcode::SubInt:
            case QoreIROpcode::SubFloat:
            case QoreIROpcode::SubAny:
            case QoreIROpcode::MulInt:
            case QoreIROpcode::MulFloat:
            case QoreIROpcode::MulAny:
            case QoreIROpcode::DivInt:
            case QoreIROpcode::DivFloat:
            case QoreIROpcode::DivAny:
            case QoreIROpcode::ModInt:
            case QoreIROpcode::ModAny:
            case QoreIROpcode::AndInt:
            case QoreIROpcode::AndAny:
            case QoreIROpcode::OrInt:
            case QoreIROpcode::OrAny:
            case QoreIROpcode::XorInt:
            case QoreIROpcode::XorAny:
            case QoreIROpcode::ShlInt:
            case QoreIROpcode::ShlAny:
            case QoreIROpcode::ShrInt:
            case QoreIROpcode::ShrAny:
            case QoreIROpcode::ShlAssignInt:
            case QoreIROpcode::ShlAssignAny:
            case QoreIROpcode::ShrAssignInt:
            case QoreIROpcode::ShrAssignAny:
            case QoreIROpcode::AddAssignInt:
            case QoreIROpcode::AddAssignFloat:
            case QoreIROpcode::AddAssignAny:
            case QoreIROpcode::SubAssignInt:
            case QoreIROpcode::SubAssignFloat:
            case QoreIROpcode::SubAssignAny:
            case QoreIROpcode::MulAssignInt:
            case QoreIROpcode::MulAssignFloat:
            case QoreIROpcode::MulAssignAny:
            case QoreIROpcode::DivAssignInt:
            case QoreIROpcode::DivAssignFloat:
            case QoreIROpcode::DivAssignAny:
            case QoreIROpcode::ModAssignInt:
            case QoreIROpcode::ModAssignAny:
            case QoreIROpcode::AndAssignInt:
            case QoreIROpcode::AndAssignAny:
            case QoreIROpcode::OrAssignInt:
            case QoreIROpcode::OrAssignAny:
            case QoreIROpcode::XorAssignInt:
            case QoreIROpcode::XorAssignAny:
            case QoreIROpcode::FoldlAny:
            case QoreIROpcode::FoldlInt:
            case QoreIROpcode::FoldlFloat:
            case QoreIROpcode::FoldrAny:
            case QoreIROpcode::FoldrInt:
            case QoreIROpcode::FoldrFloat:
            case QoreIROpcode::FoldlSumInt:
            case QoreIROpcode::FoldlSumFloat:
            case QoreIROpcode::FoldlProdInt:
            case QoreIROpcode::FoldlProdFloat:
            case QoreIROpcode::FoldlDiffInt:
            case QoreIROpcode::FoldlDiffFloat:
            case QoreIROpcode::FoldlMinInt:
            case QoreIROpcode::FoldlMinFloat:
            case QoreIROpcode::FoldlMaxInt:
            case QoreIROpcode::FoldlMaxFloat:
            case QoreIROpcode::MapAny:
            case QoreIROpcode::MapInt:
            case QoreIROpcode::MapFloat:
            case QoreIROpcode::MapScaleInt:
            case QoreIROpcode::MapScaleFloat:
            case QoreIROpcode::MapOffsetInt:
            case QoreIROpcode::MapOffsetFloat:
            case QoreIROpcode::MapSquareInt:
            case QoreIROpcode::MapSquareFloat:
            case QoreIROpcode::SelectAny:
            case QoreIROpcode::SelectInt:
            case QoreIROpcode::SelectFloat:
            case QoreIROpcode::SelectPositiveInt:
            case QoreIROpcode::SelectPositiveFloat:
            case QoreIROpcode::SelectNonZeroInt:
            case QoreIROpcode::SelectNonZeroFloat:
            case QoreIROpcode::FusedMapSelectScalePositiveInt:
            case QoreIROpcode::FusedMapSelectScalePositiveFloat:
            case QoreIROpcode::FusedMapSelectOffsetPositiveInt:
            case QoreIROpcode::FusedMapSelectOffsetPositiveFloat:
            case QoreIROpcode::FusedMapSelectSquarePositiveInt:
            case QoreIROpcode::FusedMapSelectSquarePositiveFloat:
            case QoreIROpcode::RangeAny:
            case QoreIROpcode::RangeInt:
            case QoreIROpcode::RangeFloat:
            case QoreIROpcode::RangeDate:
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
            case QoreIROpcode::CmpAny: {
                if (inst->operands.size() < 2) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "binary op missing operands");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                QoreValue left = getIRValue(values, inst->operands[0]);
                QoreValue right = getIRValue(values, inst->operands[1]);
                QoreValue res = QoreIRInterpreter::evalBinary(inst->opcode, left, right, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                values[inst->result.id] = res;
                if (res.hasNode()) {
                    cleanup.push_back(inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::RangeSliceAny:
            case QoreIROpcode::RangeSliceInt:
            case QoreIROpcode::RangeSliceFloat:
            case QoreIROpcode::MapSelectAny:
            case QoreIROpcode::MapSelectList:
            case QoreIROpcode::HashMapAny:
            case QoreIROpcode::HashMap: {
                if (inst->operands.size() < 3) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "ternary op missing operands");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                QoreValue first = getIRValue(values, inst->operands[0]);
                QoreValue second = getIRValue(values, inst->operands[1]);
                QoreValue third = getIRValue(values, inst->operands[2]);
                QoreValue res = QoreIRInterpreter::evalTernary(inst->opcode, first, second, third, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                values[inst->result.id] = res;
                if (res.hasNode()) {
                    cleanup.push_back(inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::HashMapSelectAny:
            case QoreIROpcode::HashMapSelect: {
                if (inst->operands.size() < 4) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "quaternary op missing operands");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                QoreValue first = getIRValue(values, inst->operands[0]);
                QoreValue second = getIRValue(values, inst->operands[1]);
                QoreValue third = getIRValue(values, inst->operands[2]);
                QoreValue fourth = getIRValue(values, inst->operands[3]);
                QoreValue res = QoreIRInterpreter::evalQuaternary(inst->opcode, first, second, third, fourth, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                values[inst->result.id] = res;
                if (res.hasNode()) {
                    cleanup.push_back(inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::LoadLValue: {
                auto* lval_inst = static_cast<QoreIRLValueInstruction*>(inst);
                QoreValue res = QoreIRInterpreter::evalLValueLoad(lval_inst->lvalue, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                values[lval_inst->result.id] = res;
                if (res.hasNode()) {
                    cleanup.push_back(lval_inst->result.id);
                }
                updateLocalVarFromLvalue(locals, instantiated_locals, lval_inst->lvalue, res, xsink, pre_instantiated);
                ++ip;
                break;
            }
            case QoreIROpcode::StoreLValue: {
                auto* lval_inst = static_cast<QoreIRLValueInstruction*>(inst);
                if (lval_inst->operands.empty()) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "store.lvalue missing operand");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                QoreValue val = getIRValue(values, lval_inst->operands[0]);
                QoreValue res = QoreIRInterpreter::evalLValueStore(lval_inst->lvalue, val, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                // Invalidate the root variable's cached value — evalLValueStore
                // may trigger COW on the thread-local stack, making the cached
                // value stale (the next LoadLocal will re-read from the stack)
                invalidateLvalueRoot(lval_inst->lvalue, locals, globals, threadlocals, closures);
                // StoreLValue has no result register (result.id == 0); discard
                // the returned reference to avoid storing into values[0] which
                // causes double-free when multiple stores accumulate in cleanup
                if (lval_inst->result.isValid()) {
                    values[lval_inst->result.id] = res;
                    if (res.hasNode()) {
                        cleanup.push_back(lval_inst->result.id);
                    }
                } else {
                    res.discard(xsink);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::PreIncLValue:
            case QoreIROpcode::PreDecLValue:
            case QoreIROpcode::PostIncLValue:
            case QoreIROpcode::PostDecLValue: {
                auto* lval_inst = static_cast<QoreIRLValueInstruction*>(inst);
                // evalLValueUnary returns the old value for post-inc/dec, new value for pre-inc/dec
                QoreValue res = QoreIRInterpreter::evalLValueUnary(inst->opcode, lval_inst->lvalue, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                cleanupStoredValues(locals, nullptr);
                cleanupStoredValues(globals, nullptr);
                cleanupStoredValues(threadlocals, nullptr);
                cleanupStoredValues(closures, nullptr);
                // For post-increment/decrement, use the old value (res) as the result;
                // for pre-increment/decrement, reload the updated value
                bool is_post = (inst->opcode == QoreIROpcode::PostIncLValue
                    || inst->opcode == QoreIROpcode::PostDecLValue);
                QoreValue result_val;
                if (is_post) {
                    result_val = res;
                } else {
                    res.discard(xsink);
                    result_val = QoreIRInterpreter::evalLValueLoad(lval_inst->lvalue, xsink);
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupStoredValues(locals, nullptr);
                        cleanupStoredValues(globals, nullptr);
                        cleanupStoredValues(threadlocals, nullptr);
                        cleanupStoredValues(closures, nullptr);
                        return false;
                    }
                }
                values[lval_inst->result.id] = result_val;
                if (result_val.hasNode()) {
                    cleanup.push_back(lval_inst->result.id);
                }
                // Always reload the lvalue to update local var cache with new value
                QoreValue updated = QoreIRInterpreter::evalLValueLoad(lval_inst->lvalue, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                updateLocalVarFromLvalue(locals, instantiated_locals, lval_inst->lvalue, updated, xsink, pre_instantiated);
                // discard the reload value if not used as result (for post ops, it's only for cache update)
                if (is_post) {
                    updated.discard(xsink);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::ShiftLValue: {
                auto* lval_inst = static_cast<QoreIRLValueInstruction*>(inst);
                QoreValue res = QoreIRInterpreter::evalLValueUnary(inst->opcode, lval_inst->lvalue, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                cleanupStoredValues(locals, nullptr);
                cleanupStoredValues(globals, nullptr);
                cleanupStoredValues(threadlocals, nullptr);
                cleanupStoredValues(closures, nullptr);
                // shift returns the removed element, not the updated list
                values[lval_inst->result.id] = res;
                if (res.hasNode()) {
                    cleanup.push_back(lval_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::AddAssignLValue:
            case QoreIROpcode::SubAssignLValue:
            case QoreIROpcode::MulAssignLValue:
            case QoreIROpcode::DivAssignLValue:
            case QoreIROpcode::ModAssignLValue:
            case QoreIROpcode::AndAssignLValue:
            case QoreIROpcode::OrAssignLValue:
            case QoreIROpcode::XorAssignLValue:
            case QoreIROpcode::ShlAssignLValue:
            case QoreIROpcode::ShrAssignLValue:
            case QoreIROpcode::UnshiftLValue: {
                auto* lval_inst = static_cast<QoreIRLValueInstruction*>(inst);
                if (lval_inst->operands.empty()) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "lvalue binary op missing operand");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                QoreValue right = getIRValue(values, lval_inst->operands[0]);
                QoreValue res = QoreIRInterpreter::evalLValueBinary(inst->opcode, lval_inst->lvalue, right, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                cleanupStoredValues(locals, nullptr);
                cleanupStoredValues(globals, nullptr);
                cleanupStoredValues(threadlocals, nullptr);
                cleanupStoredValues(closures, nullptr);
                values[lval_inst->result.id] = res;
                if (res.hasNode()) {
                    cleanup.push_back(lval_inst->result.id);
                }
                updateLocalVarFromLvalue(locals, instantiated_locals, lval_inst->lvalue, res, xsink, pre_instantiated);
                ++ip;
                break;
            }
            case QoreIROpcode::SpliceLValue: {
                auto* lval_inst = static_cast<QoreIRLValueInstruction*>(inst);
                if (lval_inst->operands.size() < 3) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "lvalue ternary op missing operands");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                QoreValue first = getIRValue(values, lval_inst->operands[0]);
                QoreValue second = getIRValue(values, lval_inst->operands[1]);
                QoreValue third = getIRValue(values, lval_inst->operands[2]);
                QoreValue res = QoreIRInterpreter::evalLValueTernary(inst->opcode, lval_inst->lvalue, first, second,
                    third, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                cleanupStoredValues(locals, nullptr);
                cleanupStoredValues(globals, nullptr);
                cleanupStoredValues(threadlocals, nullptr);
                cleanupStoredValues(closures, nullptr);
                values[lval_inst->result.id] = res;
                if (res.hasNode()) {
                    cleanup.push_back(lval_inst->result.id);
                }
                updateLocalVarFromLvalue(locals, instantiated_locals, lval_inst->lvalue, res, xsink, pre_instantiated);
                ++ip;
                break;
            }
            case QoreIROpcode::Call:
            case QoreIROpcode::CallIndirect:
            case QoreIROpcode::CallMethod:
            case QoreIROpcode::CallStatic: {
                auto* expr_inst = static_cast<QoreIRExprInstruction*>(inst);
                QoreValue res;
                bool used_operands = false;
                if (!expr_inst->operands.empty()) {
                    const ParseNode* parse_node = nullptr;
                    if (expr_inst->expr.hasNode()) {
                        parse_node = dynamic_cast<const ParseNode*>(expr_inst->expr.getInternalNode());
                    }
                    const QoreProgramLocation* loc = parse_node ? parse_node->loc : nullptr;
                    // For CallIndirect, operand[0] is the callee — skip it when building args
                    size_t arg_start = (inst->opcode == QoreIROpcode::CallIndirect) ? 1 : 0;
                    QoreListNode* arg_list = buildArgList(values, expr_inst->operands, arg_start, xsink);
                    if (xsink && *xsink) {
                        if (arg_list) {
                            arg_list->deref(xsink);
                        }
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupStoredValues(locals, nullptr);
                        cleanupStoredValues(globals, nullptr);
                        cleanupStoredValues(threadlocals, nullptr);
                        cleanupStoredValues(closures, nullptr);
                        return false;
                    }
                    if (inst->opcode == QoreIROpcode::Call) {
                        if (auto* call = dynamic_cast<const FunctionCallNode*>(expr_inst->expr.getInternalNode())) {
                            QoreValue call_expr(new FunctionCallNode(*call, arg_list));
                            ValueHolder call_holder(call_expr, nullptr);
                            res = QoreIRInterpreter::evalExpr(inst->opcode, call_expr, xsink);
                            used_operands = true;
                        }
                    } else if (inst->opcode == QoreIROpcode::CallMethod) {
                        if (auto* call = dynamic_cast<const SelfFunctionCallNode*>(expr_inst->expr.getInternalNode())) {
                            QoreValue call_expr(new SelfFunctionCallNode(*call, arg_list));
                            ValueHolder call_holder(call_expr, nullptr);
                            res = QoreIRInterpreter::evalExpr(inst->opcode, call_expr, xsink);
                            used_operands = true;
                        }
                    } else if (inst->opcode == QoreIROpcode::CallStatic) {
                        if (auto* call = dynamic_cast<const StaticMethodCallNode*>(expr_inst->expr.getInternalNode())) {
                            QoreValue call_expr(new StaticMethodCallNode(*call, arg_list));
                            ValueHolder call_holder(call_expr, nullptr);
                            res = QoreIRInterpreter::evalExpr(inst->opcode, call_expr, xsink);
                            used_operands = true;
                        }
                    } else {
                        if (auto* call = dynamic_cast<const CallReferenceCallNode*>(
                            expr_inst->expr.getInternalNode())) {
                            QoreValue exp = call->getExp();
                            if (exp.hasNode()) {
                                exp = exp.refSelf();
                            }
                            QoreValue call_expr(new CallReferenceCallNode(loc, exp, arg_list));
                            ValueHolder call_holder(call_expr, nullptr);
                            res = QoreIRInterpreter::evalExpr(inst->opcode, call_expr, xsink);
                            used_operands = true;
                        }
                    }
                    if (!used_operands && arg_list) {
                        arg_list->deref(xsink);
                    }
                }
                if (!used_operands) {
                    res = QoreIRInterpreter::evalExpr(inst->opcode, expr_inst->expr, xsink);
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                cleanupStoredValues(locals, nullptr);
                cleanupStoredValues(globals, nullptr);
                cleanupStoredValues(threadlocals, nullptr);
                cleanupStoredValues(closures, nullptr);
                values[expr_inst->result.id] = res;
                if (res.hasNode()) {
                    cleanup.push_back(expr_inst->result.id);
                }
                ++ip;
                break;
            }
        case QoreIROpcode::RegexMatchBool:
        case QoreIROpcode::RegexNMatchBool: {
            // Handle regex match/nmatch using the IR operand (the actual runtime value)
            // rather than falling back to evalExprNode, which would evaluate the AST
            // expression's left operand (which may be NOTHING for switch-generated regex cases).
            auto* expr_inst = static_cast<QoreIRExprInstruction*>(inst);
            QoreValue res;
            if (!expr_inst->operands.empty()) {
                QoreValue str_val = getIRValue(values, expr_inst->operands[0]);
                QoreRegex* regex = nullptr;
                if (auto* match_node = dynamic_cast<const QoreRegexMatchOperatorNode*>(
                        expr_inst->expr.getInternalNode())) {
                    regex = match_node->getRegex();
                } else if (auto* nmatch_node = dynamic_cast<const QoreRegexNMatchOperatorNode*>(
                        expr_inst->expr.getInternalNode())) {
                    regex = nmatch_node->getRegex();
                }
                if (regex) {
                    QoreStringNodeValueHelper str(str_val);
                    bool match = regex->exec(*str, xsink);
                    if (inst->opcode == QoreIROpcode::RegexNMatchBool) {
                        match = !match;
                    }
                    res = QoreValue(match);
                } else {
                    res = QoreIRInterpreter::evalExpr(inst->opcode, expr_inst->expr, xsink);
                }
            } else {
                res = QoreIRInterpreter::evalExpr(inst->opcode, expr_inst->expr, xsink);
            }
            if (xsink && *xsink) {
                executeOnBlockExitHandlers(on_block_exit_handlers, xsink);
                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                cleanupStoredValues(locals, nullptr);
                cleanupStoredValues(globals, nullptr);
                cleanupStoredValues(threadlocals, nullptr);
                cleanupStoredValues(closures, nullptr);
                return false;
            }
            values[expr_inst->result.id] = res;
            if (res.hasNode()) {
                cleanup.push_back(expr_inst->result.id);
            }
            ++ip;
            break;
        }
        case QoreIROpcode::RegexMatchAny:
        case QoreIROpcode::RegexExtractAny:
        case QoreIROpcode::RegexExtractList: {
            auto* expr_inst = static_cast<QoreIRExprInstruction*>(inst);
            QoreValue res;
            if (!inst->operands.empty()) {
                QoreValue str_val = getIRValue(values, expr_inst->operands[0]);
                if (auto* match_node = dynamic_cast<const QoreRegexMatchOperatorNode*>(
                        expr_inst->expr.getInternalNode())) {
                    QoreRegex* regex = match_node->getRegex();
                    if (regex) {
                        QoreStringNodeValueHelper str(str_val);
                        if (inst->opcode == QoreIROpcode::RegexMatchAny) {
                            res = QoreValue(regex->exec(*str, xsink));
                        } else {
                            res = QoreValue(regex->extractSubstrings(*str, xsink));
                        }
                    } else {
                        res = QoreIRInterpreter::evalExpr(inst->opcode, expr_inst->expr, xsink);
                    }
                } else if (auto* extract_node = dynamic_cast<const QoreRegexExtractOperatorNode*>(
                        expr_inst->expr.getInternalNode())) {
                    QoreRegex* regex = extract_node->getRegex();
                    if (regex) {
                        QoreStringNodeValueHelper str(str_val);
                        res = QoreValue(regex->extractSubstrings(*str, xsink));
                    } else {
                        res = QoreIRInterpreter::evalExpr(inst->opcode, expr_inst->expr, xsink);
                    }
                } else {
                    res = QoreIRInterpreter::evalExpr(inst->opcode, expr_inst->expr, xsink);
                }
            } else {
                res = QoreIRInterpreter::evalExpr(inst->opcode, expr_inst->expr, xsink);
            }
            if (xsink && *xsink) {
                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                cleanupStoredValues(locals, nullptr);
                cleanupStoredValues(globals, nullptr);
                cleanupStoredValues(threadlocals, nullptr);
                cleanupStoredValues(closures, nullptr);
                return false;
            }
            values[expr_inst->result.id] = res;
            if (res.hasNode()) {
                cleanup.push_back(expr_inst->result.id);
            }
            ++ip;
            break;
        }
        case QoreIROpcode::ExtractAny:
        case QoreIROpcode::ExtractList:
        case QoreIROpcode::ExtractString:
        case QoreIROpcode::ExtractBinary:
        case QoreIROpcode::RemoveAny:
        case QoreIROpcode::RemoveList:
        case QoreIROpcode::RemoveHash:
        case QoreIROpcode::RemoveObject:
        case QoreIROpcode::RemoveString:
        case QoreIROpcode::RemoveBinary:
        case QoreIROpcode::KeysAny:
        case QoreIROpcode::KeysList:
        case QoreIROpcode::KeysHash:
            case QoreIROpcode::RegexSubstAny:
            case QoreIROpcode::RegexSubstString:
            case QoreIROpcode::InstanceOfBool:
            case QoreIROpcode::TrimAny:
            case QoreIROpcode::TrimString:
            case QoreIROpcode::ChompAny:
            case QoreIROpcode::ChompString:
            case QoreIROpcode::TransliterateAny:
            case QoreIROpcode::TransliterateString:
            case QoreIROpcode::BackgroundInt:
            case QoreIROpcode::ListAssignAny:
            case QoreIROpcode::PopAny:
            case QoreIROpcode::PushAny:
            case QoreIROpcode::ExistsAny:
            case QoreIROpcode::ExistsBool:
        case QoreIROpcode::ElementsAny:
        case QoreIROpcode::ElementsInt:
        case QoreIROpcode::CastAny:
            case QoreIROpcode::CastList:
            case QoreIROpcode::CastHash:
            case QoreIROpcode::CastObject:
            case QoreIROpcode::CastEnum:
            case QoreIROpcode::InvokeSimError: {
                auto* expr_inst = static_cast<QoreIRExprInstruction*>(inst);
                QoreValue res = QoreIRInterpreter::evalExpr(inst->opcode, expr_inst->expr, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                // Only invalidate caches for opcodes that modify variables via AST
                // Pure expression opcodes (Cast, Exists, Elements, etc.)
                // don't modify variables and don't need cache invalidation
                switch (inst->opcode) {
                    case QoreIROpcode::ExtractAny:
                    case QoreIROpcode::ExtractList:
                    case QoreIROpcode::ExtractString:
                    case QoreIROpcode::ExtractBinary:
                    case QoreIROpcode::RemoveAny:
                    case QoreIROpcode::RemoveList:
                    case QoreIROpcode::RemoveHash:
                    case QoreIROpcode::RemoveObject:
                    case QoreIROpcode::RemoveString:
                    case QoreIROpcode::RemoveBinary:
                    case QoreIROpcode::RegexSubstAny:
                    case QoreIROpcode::RegexSubstString:
                    case QoreIROpcode::TrimAny:
                    case QoreIROpcode::TrimString:
                    case QoreIROpcode::ChompAny:
                    case QoreIROpcode::ChompString:
                    case QoreIROpcode::TransliterateAny:
                    case QoreIROpcode::TransliterateString:
                    case QoreIROpcode::PopAny:
                    case QoreIROpcode::PushAny:
                    case QoreIROpcode::ListAssignAny:
                        cleanupStoredValues(locals, nullptr);
                        cleanupStoredValues(globals, nullptr);
                        cleanupStoredValues(threadlocals, nullptr);
                        cleanupStoredValues(closures, nullptr);
                        break;
                    default:
                        break;
                }
                values[inst->result.id] = res;
                if (res.hasNode()) {
                    cleanup.push_back(inst->result.id);
                }
                ++ip;
                break;
            }
        // DotEval opcodes: use pre-evaluated base to avoid double-evaluation
        case QoreIROpcode::DotEvalAny:
        case QoreIROpcode::DotEvalInt:
        case QoreIROpcode::DotEvalFloat:
        case QoreIROpcode::DotEvalString:
        case QoreIROpcode::DotEvalDate:
        case QoreIROpcode::DotEvalList:
        case QoreIROpcode::DotEvalHash:
        case QoreIROpcode::DotEvalObject: {
                auto* expr_inst = static_cast<QoreIRExprInstruction*>(inst);
                QoreValue res;
                if (!inst->operands.empty()) {
                    QoreValue base = getIRValue(values, inst->operands[0]);
                    auto* dot_eval = dynamic_cast<const QoreDotEvalOperatorNode*>(
                        expr_inst->expr.getInternalNode());
                    if (dot_eval) {
                        res = dot_eval->evalWithBase(base, xsink);
                    } else {
                        res = QoreIRInterpreter::evalExpr(inst->opcode, expr_inst->expr, xsink);
                    }
                } else {
                    res = QoreIRInterpreter::evalExpr(inst->opcode, expr_inst->expr, xsink);
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                // DotEval opcodes are pure expression opcodes — no cache invalidation needed
                values[expr_inst->result.id] = res;
                if (res.hasNode()) {
                    cleanup.push_back(expr_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::Throw: {
                auto* throw_inst = static_cast<QoreIRThrowInstruction*>(inst);
                if (inst->operands.empty()) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "throw missing operand");
                    }
                } else if (xsink) {
                    QoreValue arg = getIRValue(values, inst->operands[0]);
                    // throw args are always a list: (err, desc[, arg])
                    // use the same raiseException(list) API as the AST path
                    if (arg.getType() == NT_LIST) {
                        xsink->raiseException(arg.get<const QoreListNode>());
                    } else {
                        QoreValue owned_arg = arg.hasNode() ? arg.refSelf() : arg;
                        xsink->raiseExceptionArg("IR-EXEC-THROW", owned_arg, "throw");
                    }
                }
                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                if (throw_inst->exception_target) {
                    prev_block = block;
                    block = throw_inst->exception_target;
                    ip = 0;
                    break;
                }
                cleanupStoredValues(locals, nullptr);
                cleanupStoredValues(globals, nullptr);
                cleanupStoredValues(threadlocals, nullptr);
                cleanupStoredValues(closures, nullptr);
                return false;
            }
            case QoreIROpcode::Rethrow: {
                if (!xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                QoreException* ex = catch_get_exception();
                if (!ex) {
                    xsink->raiseException("IR-EXEC-ERROR", "rethrow without exception");
                } else {
                    xsink->raiseException(ex);
                }
                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                cleanupStoredValues(locals, nullptr);
                cleanupStoredValues(globals, nullptr);
                cleanupStoredValues(threadlocals, nullptr);
                cleanupStoredValues(closures, nullptr);
                return false;
            }
            case QoreIROpcode::Return: {
                auto* ret = static_cast<QoreIRReturnInstruction*>(inst);
                if (ret->has_value) {
                    QoreValue val = getIRValue(values, ret->value);
                    removeCleanupEntry(cleanup, ret->value.id);
                    if (val.hasNode()) {
                        return_value = val.refSelf();
                        auto it = values.find(ret->value.id);
                        if (it != values.end()) {
                            it->second.discard(xsink);
                            values.erase(it);
                        }
                    } else {
                        return_value = val;
                    }
                } else {
                    return_value = QoreValue();
                }
                executeOnBlockExitHandlers(on_block_exit_handlers, xsink);
                cleanupValues(values, cleanup, xsink, false, cleanup_log);
                cleanupStoredValues(locals, xsink);
                cleanupStoredValues(globals, xsink);
                cleanupStoredValues(threadlocals, xsink);
                cleanupStoredValues(closures, xsink);
                return true;
            }
            case QoreIROpcode::ReturnNothing:
                return_value = QoreValue();
                executeOnBlockExitHandlers(on_block_exit_handlers, xsink);
                cleanupValues(values, cleanup, xsink, false, cleanup_log);
                cleanupStoredValues(locals, xsink);
                cleanupStoredValues(globals, xsink);
                cleanupStoredValues(threadlocals, xsink);
                cleanupStoredValues(closures, xsink);
                return true;
            default:
                if (xsink) {
                    std::string msg = "unsupported opcode in executor: ";
                    msg += std::to_string(static_cast<int>(inst->opcode));
                    xsink->raiseException("IR-EXEC-ERROR", msg.c_str());
                }
                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                cleanupStoredValues(locals, nullptr);
                cleanupStoredValues(globals, nullptr);
                cleanupStoredValues(threadlocals, nullptr);
                cleanupStoredValues(closures, nullptr);
                return false;
        }
    }

    if (xsink) {
        xsink->raiseException("IR-EXEC-ERROR", "executor reached invalid state");
    }
    cleanupValues(values, cleanup, xsink, true, cleanup_log);
    cleanupStoredValues(locals, nullptr);
    cleanupStoredValues(globals, nullptr);
    cleanupStoredValues(threadlocals, nullptr);
    cleanupStoredValues(closures, nullptr);
    return false;
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
            bool needs_deref = true;
            QoreUnaryPlusOperatorNode node(nullptr, value);
            return evalAndRef(&node, xsink);
        }
        case QoreIROpcode::UnaryMinusInt:
            return QoreValue(-value.getAsBigInt());
        case QoreIROpcode::UnaryMinusFloat:
            return QoreValue(-value.getAsFloat());
        case QoreIROpcode::UnaryMinusAny: {
            bool needs_deref = true;
            QoreUnaryMinusOperatorNode node(nullptr, value);
            return evalAndRef(&node, xsink);
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
            bool needs_deref = true;
            QorePlusOperatorNode node(nullptr, left.refSelf(), right.refSelf());
            return evalAndRef(&node, xsink);
        }
        case QoreIROpcode::SubInt:
            return QoreValue(left.getAsBigInt() - right.getAsBigInt());
        case QoreIROpcode::SubFloat:
            return QoreValue(left.getAsFloat() - right.getAsFloat());
        case QoreIROpcode::SubAny: {
            bool needs_deref = true;
            QoreMinusOperatorNode node(nullptr, left.refSelf(), right.refSelf());
            return evalAndRef(&node, xsink);
        }
        case QoreIROpcode::MulInt:
            return QoreValue(left.getAsBigInt() * right.getAsBigInt());
        case QoreIROpcode::MulFloat:
            return QoreValue(left.getAsFloat() * right.getAsFloat());
        case QoreIROpcode::MulAny: {
            bool needs_deref = true;
            QoreMultiplicationOperatorNode node(nullptr, left.refSelf(), right.refSelf());
            return evalAndRef(&node, xsink);
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
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreBinaryAndOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::OrInt:
            return QoreValue(left.getAsBigInt() | right.getAsBigInt());
        case QoreIROpcode::OrAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreBinaryOrOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::XorInt:
            return QoreValue(left.getAsBigInt() ^ right.getAsBigInt());
        case QoreIROpcode::XorAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreBinaryXorOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::AddAssignInt:
            return QoreValue(left.getAsBigInt() + right.getAsBigInt());
        case QoreIROpcode::AddAssignFloat:
            return QoreValue(left.getAsFloat() + right.getAsFloat());
        case QoreIROpcode::AddAssignAny: {
            // If the left operand is NOTHING, return the right operand directly
            // to avoid NOTHING being treated as a list element in concatenation
            if (left.isNothing()) {
                return right.refSelf();
            }
            bool needs_deref = true;
            QorePlusOperatorNode node(nullptr, left.refSelf(), right.refSelf());
            return evalAndRef(&node, xsink);
        }
        case QoreIROpcode::SubAssignInt:
            return QoreValue(left.getAsBigInt() - right.getAsBigInt());
        case QoreIROpcode::SubAssignFloat:
            return QoreValue(left.getAsFloat() - right.getAsFloat());
        case QoreIROpcode::SubAssignAny: {
            bool needs_deref = true;
            QoreMinusOperatorNode node(nullptr, left.refSelf(), right.refSelf());
            return evalAndRef(&node, xsink);
        }
        case QoreIROpcode::MulAssignInt:
            return QoreValue(left.getAsBigInt() * right.getAsBigInt());
        case QoreIROpcode::MulAssignFloat:
            return QoreValue(left.getAsFloat() * right.getAsFloat());
        case QoreIROpcode::MulAssignAny: {
            bool needs_deref = true;
            QoreMultiplicationOperatorNode node(nullptr, left.refSelf(), right.refSelf());
            return evalAndRef(&node, xsink);
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
        case QoreIROpcode::DivAssignFloat: {
            double divisor = right.getAsFloat();
            if (!divisor) {
                if (xsink) {
                    xsink->raiseException("DIVISION-BY-ZERO", "division by zero found in floating-point expression");
                }
                return QoreValue();
            }
            return QoreValue(left.getAsFloat() / divisor);
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
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreBinaryAndOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::OrAssignInt:
            return QoreValue(left.getAsBigInt() | right.getAsBigInt());
        case QoreIROpcode::OrAssignAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreBinaryOrOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::XorAssignInt:
            return QoreValue(left.getAsBigInt() ^ right.getAsBigInt());
        case QoreIROpcode::XorAssignAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreBinaryXorOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::ShlInt:
            return QoreValue(left.getAsBigInt() << right.getAsBigInt());
        case QoreIROpcode::ShlAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreShiftLeftOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::ShrInt:
            return QoreValue(left.getAsBigInt() >> right.getAsBigInt());
        case QoreIROpcode::ShrAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreShiftRightOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::ShlAssignInt:
            return QoreValue(left.getAsBigInt() << right.getAsBigInt());
        case QoreIROpcode::ShlAssignAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreShiftLeftEqualsOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::ShrAssignInt:
            return QoreValue(left.getAsBigInt() >> right.getAsBigInt());
        case QoreIROpcode::ShrAssignAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreShiftRightEqualsOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::FoldlAny:
        case QoreIROpcode::FoldlInt:
        case QoreIROpcode::FoldlFloat: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreFoldlOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        // Optimized fold operations with native loops
        case QoreIROpcode::FoldlSumInt: {
            // left = list, right = initial value (NOTHING means use first element)
            if (left.getType() != NT_LIST) {
                return right.isNothing() ? QoreValue(0ll) : QoreValue(right.getAsBigInt());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            size_t start_idx = 0;
            int64_t result;
            if (right.isNothing()) {
                if (sz == 0) {
                    return QoreValue(0ll);
                }
                result = l->retrieveEntry(0).getAsBigInt();
                start_idx = 1;
            } else {
                result = right.getAsBigInt();
            }
            for (size_t i = start_idx; i < sz; ++i) {
                result += l->retrieveEntry(i).getAsBigInt();
            }
            return QoreValue(result);
        }
        case QoreIROpcode::FoldlSumFloat: {
            // left = list, right = initial value (NOTHING means use first element)
            if (left.getType() != NT_LIST) {
                return right.isNothing() ? QoreValue(0.0) : QoreValue(right.getAsFloat());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            size_t start_idx = 0;
            double result;
            if (right.isNothing()) {
                if (sz == 0) {
                    return QoreValue(0.0);
                }
                result = l->retrieveEntry(0).getAsFloat();
                start_idx = 1;
            } else {
                result = right.getAsFloat();
            }
            for (size_t i = start_idx; i < sz; ++i) {
                result += l->retrieveEntry(i).getAsFloat();
            }
            return QoreValue(result);
        }
        case QoreIROpcode::FoldlProdInt: {
            // left = list, right = initial value (NOTHING means use first element)
            if (left.getType() != NT_LIST) {
                return right.isNothing() ? QoreValue(1ll) : QoreValue(right.getAsBigInt());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            size_t start_idx = 0;
            int64_t result;
            if (right.isNothing()) {
                if (sz == 0) {
                    return QoreValue(1ll);
                }
                result = l->retrieveEntry(0).getAsBigInt();
                start_idx = 1;
            } else {
                result = right.getAsBigInt();
            }
            for (size_t i = start_idx; i < sz; ++i) {
                result *= l->retrieveEntry(i).getAsBigInt();
            }
            return QoreValue(result);
        }
        case QoreIROpcode::FoldlProdFloat: {
            // left = list, right = initial value (NOTHING means use first element)
            if (left.getType() != NT_LIST) {
                return right.isNothing() ? QoreValue(1.0) : QoreValue(right.getAsFloat());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            size_t start_idx = 0;
            double result;
            if (right.isNothing()) {
                if (sz == 0) {
                    return QoreValue(1.0);
                }
                result = l->retrieveEntry(0).getAsFloat();
                start_idx = 1;
            } else {
                result = right.getAsFloat();
            }
            for (size_t i = start_idx; i < sz; ++i) {
                result *= l->retrieveEntry(i).getAsFloat();
            }
            return QoreValue(result);
        }
        case QoreIROpcode::FoldlDiffInt: {
            // left = list, right = initial value (NOTHING means use first element)
            if (left.getType() != NT_LIST) {
                return right.isNothing() ? QoreValue(0ll) : QoreValue(right.getAsBigInt());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            size_t start_idx = 0;
            int64_t result;
            if (right.isNothing()) {
                if (sz == 0) {
                    return QoreValue(0ll);
                }
                result = l->retrieveEntry(0).getAsBigInt();
                start_idx = 1;
            } else {
                result = right.getAsBigInt();
            }
            for (size_t i = start_idx; i < sz; ++i) {
                result -= l->retrieveEntry(i).getAsBigInt();
            }
            return QoreValue(result);
        }
        case QoreIROpcode::FoldlDiffFloat: {
            // left = list, right = initial value (NOTHING means use first element)
            if (left.getType() != NT_LIST) {
                return right.isNothing() ? QoreValue(0.0) : QoreValue(right.getAsFloat());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            size_t start_idx = 0;
            double result;
            if (right.isNothing()) {
                if (sz == 0) {
                    return QoreValue(0.0);
                }
                result = l->retrieveEntry(0).getAsFloat();
                start_idx = 1;
            } else {
                result = right.getAsFloat();
            }
            for (size_t i = start_idx; i < sz; ++i) {
                result -= l->retrieveEntry(i).getAsFloat();
            }
            return QoreValue(result);
        }
        case QoreIROpcode::FoldlMinInt: {
            // left = list, right = initial value (NOTHING means use first element)
            if (left.getType() != NT_LIST) {
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsBigInt());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsBigInt());
            }
            size_t start_idx = 0;
            int64_t result;
            if (right.isNothing()) {
                result = l->retrieveEntry(0).getAsBigInt();
                start_idx = 1;
            } else {
                result = right.getAsBigInt();
            }
            for (size_t i = start_idx; i < sz; ++i) {
                int64_t val = l->retrieveEntry(i).getAsBigInt();
                if (val < result) {
                    result = val;
                }
            }
            return QoreValue(result);
        }
        case QoreIROpcode::FoldlMinFloat: {
            // left = list, right = initial value (NOTHING means use first element)
            if (left.getType() != NT_LIST) {
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsFloat());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsFloat());
            }
            size_t start_idx = 0;
            double result;
            if (right.isNothing()) {
                result = l->retrieveEntry(0).getAsFloat();
                start_idx = 1;
            } else {
                result = right.getAsFloat();
            }
            for (size_t i = start_idx; i < sz; ++i) {
                double val = l->retrieveEntry(i).getAsFloat();
                if (val < result) {
                    result = val;
                }
            }
            return QoreValue(result);
        }
        case QoreIROpcode::FoldlMaxInt: {
            // left = list, right = initial value (NOTHING means use first element)
            if (left.getType() != NT_LIST) {
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsBigInt());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsBigInt());
            }
            size_t start_idx = 0;
            int64_t result;
            if (right.isNothing()) {
                result = l->retrieveEntry(0).getAsBigInt();
                start_idx = 1;
            } else {
                result = right.getAsBigInt();
            }
            for (size_t i = start_idx; i < sz; ++i) {
                int64_t val = l->retrieveEntry(i).getAsBigInt();
                if (val > result) {
                    result = val;
                }
            }
            return QoreValue(result);
        }
        case QoreIROpcode::FoldlMaxFloat: {
            // left = list, right = initial value (NOTHING means use first element)
            if (left.getType() != NT_LIST) {
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsFloat());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsFloat());
            }
            size_t start_idx = 0;
            double result;
            if (right.isNothing()) {
                result = l->retrieveEntry(0).getAsFloat();
                start_idx = 1;
            } else {
                result = right.getAsFloat();
            }
            for (size_t i = start_idx; i < sz; ++i) {
                double val = l->retrieveEntry(i).getAsFloat();
                if (val > result) {
                    result = val;
                }
            }
            return QoreValue(result);
        }
        case QoreIROpcode::FoldrAny:
        case QoreIROpcode::FoldrInt:
        case QoreIROpcode::FoldrFloat: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreFoldrOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::MapAny:
        case QoreIROpcode::MapInt:
        case QoreIROpcode::MapFloat: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreMapOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        // Optimized map operations with native loops
        case QoreIROpcode::MapScaleInt: {
            // left = list, right = scale factor
            if (left.getType() != NT_LIST) {
                return QoreValue(new QoreListNode(autoTypeInfo));
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            int64_t scale = right.getAsBigInt();
            ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), xsink);
            for (size_t i = 0; i < sz; ++i) {
                result->push(l->retrieveEntry(i).getAsBigInt() * scale, xsink);
            }
            return result.release();
        }
        case QoreIROpcode::MapScaleFloat: {
            if (left.getType() != NT_LIST) {
                return QoreValue(new QoreListNode(autoTypeInfo));
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            double scale = right.getAsFloat();
            ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), xsink);
            for (size_t i = 0; i < sz; ++i) {
                result->push(l->retrieveEntry(i).getAsFloat() * scale, xsink);
            }
            return result.release();
        }
        case QoreIROpcode::MapOffsetInt: {
            // left = list, right = offset
            if (left.getType() != NT_LIST) {
                return QoreValue(new QoreListNode(autoTypeInfo));
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            int64_t offset = right.getAsBigInt();
            ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), xsink);
            for (size_t i = 0; i < sz; ++i) {
                result->push(l->retrieveEntry(i).getAsBigInt() + offset, xsink);
            }
            return result.release();
        }
        case QoreIROpcode::MapOffsetFloat: {
            if (left.getType() != NT_LIST) {
                return QoreValue(new QoreListNode(autoTypeInfo));
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            double offset = right.getAsFloat();
            ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), xsink);
            for (size_t i = 0; i < sz; ++i) {
                result->push(l->retrieveEntry(i).getAsFloat() + offset, xsink);
            }
            return result.release();
        }
        case QoreIROpcode::MapSquareInt: {
            // left = list, right = unused
            if (left.getType() != NT_LIST) {
                return QoreValue(new QoreListNode(autoTypeInfo));
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), xsink);
            for (size_t i = 0; i < sz; ++i) {
                int64_t val = l->retrieveEntry(i).getAsBigInt();
                result->push(val * val, xsink);
            }
            return result.release();
        }
        case QoreIROpcode::MapSquareFloat: {
            if (left.getType() != NT_LIST) {
                return QoreValue(new QoreListNode(autoTypeInfo));
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), xsink);
            for (size_t i = 0; i < sz; ++i) {
                double val = l->retrieveEntry(i).getAsFloat();
                result->push(val * val, xsink);
            }
            return result.release();
        }
        case QoreIROpcode::SelectAny:
        case QoreIROpcode::SelectInt:
        case QoreIROpcode::SelectFloat: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreSelectOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        // Optimized select operations with native loops
        case QoreIROpcode::SelectPositiveInt: {
            // left = list (filter $1 > 0)
            if (left.getType() != NT_LIST) {
                return QoreValue(new QoreListNode(autoTypeInfo));
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), xsink);
            for (size_t i = 0; i < sz; ++i) {
                int64_t val = l->retrieveEntry(i).getAsBigInt();
                if (val > 0) {
                    result->push(val, xsink);
                }
            }
            return result.release();
        }
        case QoreIROpcode::SelectPositiveFloat: {
            if (left.getType() != NT_LIST) {
                return QoreValue(new QoreListNode(autoTypeInfo));
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), xsink);
            for (size_t i = 0; i < sz; ++i) {
                double val = l->retrieveEntry(i).getAsFloat();
                if (val > 0.0) {
                    result->push(val, xsink);
                }
            }
            return result.release();
        }
        case QoreIROpcode::SelectNonZeroInt: {
            // left = list (filter $1 != 0)
            if (left.getType() != NT_LIST) {
                return QoreValue(new QoreListNode(autoTypeInfo));
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), xsink);
            for (size_t i = 0; i < sz; ++i) {
                int64_t val = l->retrieveEntry(i).getAsBigInt();
                if (val != 0) {
                    result->push(val, xsink);
                }
            }
            return result.release();
        }
        case QoreIROpcode::SelectNonZeroFloat: {
            if (left.getType() != NT_LIST) {
                return QoreValue(new QoreListNode(autoTypeInfo));
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), xsink);
            for (size_t i = 0; i < sz; ++i) {
                double val = l->retrieveEntry(i).getAsFloat();
                if (val != 0.0) {
                    result->push(val, xsink);
                }
            }
            return result.release();
        }
        // Fused map+select operations (single pass, no intermediate list)
        case QoreIROpcode::FusedMapSelectScalePositiveInt: {
            // left = list, right = scale factor
            // Filter $1 > 0, then multiply by scale
            if (left.getType() != NT_LIST) {
                return QoreValue(new QoreListNode(autoTypeInfo));
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            int64_t scale = right.getAsBigInt();
            ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), xsink);
            for (size_t i = 0; i < sz; ++i) {
                int64_t val = l->retrieveEntry(i).getAsBigInt();
                if (val > 0) {
                    result->push(val * scale, xsink);
                }
            }
            return result.release();
        }
        case QoreIROpcode::FusedMapSelectScalePositiveFloat: {
            if (left.getType() != NT_LIST) {
                return QoreValue(new QoreListNode(autoTypeInfo));
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            double scale = right.getAsFloat();
            ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), xsink);
            for (size_t i = 0; i < sz; ++i) {
                double val = l->retrieveEntry(i).getAsFloat();
                if (val > 0.0) {
                    result->push(val * scale, xsink);
                }
            }
            return result.release();
        }
        case QoreIROpcode::FusedMapSelectOffsetPositiveInt: {
            // left = list, right = offset
            // Filter $1 > 0, then add offset
            if (left.getType() != NT_LIST) {
                return QoreValue(new QoreListNode(autoTypeInfo));
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            int64_t offset = right.getAsBigInt();
            ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), xsink);
            for (size_t i = 0; i < sz; ++i) {
                int64_t val = l->retrieveEntry(i).getAsBigInt();
                if (val > 0) {
                    result->push(val + offset, xsink);
                }
            }
            return result.release();
        }
        case QoreIROpcode::FusedMapSelectOffsetPositiveFloat: {
            if (left.getType() != NT_LIST) {
                return QoreValue(new QoreListNode(autoTypeInfo));
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            double offset = right.getAsFloat();
            ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), xsink);
            for (size_t i = 0; i < sz; ++i) {
                double val = l->retrieveEntry(i).getAsFloat();
                if (val > 0.0) {
                    result->push(val + offset, xsink);
                }
            }
            return result.release();
        }
        case QoreIROpcode::FusedMapSelectSquarePositiveInt: {
            // left = list, right = unused
            // Filter $1 > 0, then square
            if (left.getType() != NT_LIST) {
                return QoreValue(new QoreListNode(autoTypeInfo));
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), xsink);
            for (size_t i = 0; i < sz; ++i) {
                int64_t val = l->retrieveEntry(i).getAsBigInt();
                if (val > 0) {
                    result->push(val * val, xsink);
                }
            }
            return result.release();
        }
        case QoreIROpcode::FusedMapSelectSquarePositiveFloat: {
            if (left.getType() != NT_LIST) {
                return QoreValue(new QoreListNode(autoTypeInfo));
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), xsink);
            for (size_t i = 0; i < sz; ++i) {
                double val = l->retrieveEntry(i).getAsFloat();
                if (val > 0.0) {
                    result->push(val * val, xsink);
                }
            }
            return result.release();
        }
        case QoreIROpcode::RangeAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreRangeOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::RangeInt:
        case QoreIROpcode::RangeFloat:
        case QoreIROpcode::RangeDate: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreRangeOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
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
        case QoreIROpcode::RangeSliceAny:
        case QoreIROpcode::RangeSliceInt:
        case QoreIROpcode::RangeSliceFloat: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreSquareBracketsRangeOperatorNode(nullptr,
                first.refSelf(), second.refSelf(), third.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::MapSelectAny:
        case QoreIROpcode::MapSelectList: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreMapSelectOperatorNode(nullptr,
                first.refSelf(), second.refSelf(), third.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::HashMapAny:
        case QoreIROpcode::HashMap: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreHashMapOperatorNode(nullptr,
                first.refSelf(), second.refSelf(), third.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        default:
            break;
    }
    if (xsink) {
        xsink->raiseException("IR-INTERPRETER-ERROR", "unsupported ternary opcode");
    }
    return QoreValue();
}

QoreValue QoreIRInterpreter::evalQuaternary(QoreIROpcode op, const QoreValue& first, const QoreValue& second,
        const QoreValue& third, const QoreValue& fourth, ExceptionSink* xsink) {
    switch (op) {
        case QoreIROpcode::HashMapSelectAny:
        case QoreIROpcode::HashMapSelect: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreHashMapSelectOperatorNode(nullptr,
                first.refSelf(), second.refSelf(), third.refSelf(), fourth.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        default:
            break;
    }
    if (xsink) {
        xsink->raiseException("IR-INTERPRETER-ERROR", "unsupported quaternary opcode");
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
    // refSelf() before passing to assign() - assign() takes ownership via
    // assignAssume()/takeNode(), but value is a borrowed reference from the
    // caller's values map; without the extra ref, both the variable and the
    // values map would think they own the same single reference
    if (helper.assign(value.refSelf(), "<lvalue>")) {
        return QoreValue();
    }
    return value.refSelf();
}

QoreValue QoreIRInterpreter::evalLValueUnary(QoreIROpcode op, const QoreValue& lvalue, ExceptionSink* xsink) {
    bool needs_deref = true;
    QoreValue lvalue_ref = lvalue.refSelf();
    switch (op) {
        case QoreIROpcode::PreIncLValue: {
            ValueHolder node(QoreValue(new QorePreIncrementOperatorNode(nullptr, lvalue_ref)), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::PreDecLValue: {
            ValueHolder node(QoreValue(new QorePreDecrementOperatorNode(nullptr, lvalue_ref)), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::PostIncLValue: {
            ValueHolder node(QoreValue(new QorePostIncrementOperatorNode(nullptr, lvalue_ref)), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::PostDecLValue: {
            ValueHolder node(QoreValue(new QorePostDecrementOperatorNode(nullptr, lvalue_ref)), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::ShiftLValue: {
            ValueHolder node(QoreValue(new QoreShiftOperatorNode(nullptr, lvalue_ref)), xsink);
            return evalAndRef(*node, xsink);
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
    bool needs_deref = true;
    QoreValue lvalue_ref = lvalue.refSelf();
    switch (op) {
        case QoreIROpcode::AddAssignLValue: {
            // Check if lvalue is currently NOTHING; if so, assign right value directly
            // to avoid NOTHING being treated as an element in list/hash concatenation
            {
                QoreValue current = evalLValueLoad(lvalue, xsink);
                if (xsink && *xsink) {
                    lvalue_ref.discard(nullptr);
                    return QoreValue();
                }
                if (current.isNothing()) {
                    current.discard(nullptr);
                    QoreValue result = evalLValueStore(lvalue, right, xsink);
                    lvalue_ref.discard(nullptr);
                    return result;
                }
                current.discard(nullptr);
            }
            ValueHolder node(QoreValue(new QorePlusEqualsOperatorNode(nullptr, lvalue_ref, right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::SubAssignLValue: {
            ValueHolder node(QoreValue(new QoreMinusEqualsOperatorNode(nullptr, lvalue_ref, right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::MulAssignLValue: {
            ValueHolder node(QoreValue(new QoreMultiplyEqualsOperatorNode(nullptr, lvalue_ref, right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::DivAssignLValue: {
            ValueHolder node(QoreValue(new QoreDivideEqualsOperatorNode(nullptr, lvalue_ref, right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::ModAssignLValue: {
            ValueHolder node(QoreValue(new QoreModuloEqualsOperatorNode(nullptr, lvalue_ref, right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::AndAssignLValue: {
            ValueHolder node(QoreValue(new QoreAndEqualsOperatorNode(nullptr, lvalue_ref, right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::OrAssignLValue: {
            ValueHolder node(QoreValue(new QoreOrEqualsOperatorNode(nullptr, lvalue_ref, right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::XorAssignLValue: {
            ValueHolder node(QoreValue(new QoreXorEqualsOperatorNode(nullptr, lvalue_ref, right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::ShlAssignLValue: {
            ValueHolder node(QoreValue(new QoreShiftLeftEqualsOperatorNode(nullptr, lvalue_ref, right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::ShrAssignLValue: {
            ValueHolder node(QoreValue(new QoreShiftRightEqualsOperatorNode(nullptr, lvalue_ref, right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::UnshiftLValue: {
            ValueHolder node(QoreValue(new QoreUnshiftOperatorNode(nullptr, lvalue_ref, right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
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
    bool needs_deref = true;
    QoreValue lvalue_ref = lvalue.refSelf();
    switch (op) {
        case QoreIROpcode::SpliceLValue: {
            ValueHolder node(QoreValue(new QoreSpliceOperatorNode(nullptr, lvalue_ref,
                first.refSelf(), second.refSelf(), third.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        default:
            break;
    }
    if (xsink) {
        xsink->raiseException("IR-INTERPRETER-ERROR", "unsupported lvalue ternary opcode");
    }
    return QoreValue();
}
