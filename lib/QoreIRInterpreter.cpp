/* -*- indent-tabs-mode: nil -*- */
/*
    QoreIRInterpreter.cpp

    Qore Programming Language
*/

#include <qore/intern/QoreIRInterpreter.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <qore/ExceptionSink.h>
#include <qore/QoreValue.h>
#include <qore/QoreStringNode.h>
#include <qore/DateTimeNode.h>
#include <qore/intern/AbstractStatement.h>
#include <qore/intern/qore_program_private.h>
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
#include <qore/intern/CaseNodeRegex.h>
#include <qore/intern/StatementBlock.h>
#include <qore/intern/QoreException.h>
#include <qore/intern/qore_thread_intern.h>
#include <qore/intern/Variable.h>
#include <qore/intern/WeakReferenceNode.h>
#include <qore/intern/WeakHashReferenceNode.h>
#include <qore/intern/WeakListReferenceNode.h>
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
#include <qore/intern/QoreClassIntern.h>
#include <qore/intern/QoreDotEvalOperatorNode.h>
#include <qore/intern/ConstantList.h>
#include <qore/intern/QoreClosureParseNode.h>
#include <qore/intern/QoreClosureNode.h>
#include <qore/intern/NewComplexTypeNode.h>
#include <qore/intern/typed_hash_decl_private.h>
#include <qore/intern/qore_list_private.h>
#include <qore/intern/QoreHashNodeIntern.h>
#include <qore/intern/QoreObjectIntern.h>
#include <qore/intern/ParseReferenceNode.h>
#include <qore/intern/QoreSquareBracketsOperatorNode.h>
#include <qore/intern/QoreBinaryLValueOperatorNode.h>

//! Extract the base VarRefNode from a (possibly complex) lvalue expression tree.
/** Walks the tree by following the "left" / "base" operand of operator nodes that
    can serve as lvalue wrappers (square brackets, hash deref, shift, splice, etc.).
    Returns nullptr when the tree cannot be resolved to a simple variable reference.
*/
static const VarRefNode* extractLValueBaseVarRef(const QoreValue& lvalue) {
    if (!lvalue.hasNode()) {
        return nullptr;
    }
    const AbstractQoreNode* node = lvalue.getInternalNode();
    if (auto* var = dynamic_cast<const VarRefNode*>(node)) {
        return var;
    }
    if (auto* op = dynamic_cast<const QoreBinaryLValueOperatorNode*>(node)) {
        return extractLValueBaseVarRef(op->getLeft());
    }
    if (auto* op = dynamic_cast<const QoreBinaryIntLValueOperatorNode*>(node)) {
        return extractLValueBaseVarRef(op->getLeft());
    }
    if (auto* op = dynamic_cast<const QoreSquareBracketsOperatorNode*>(node)) {
        return extractLValueBaseVarRef(op->getLeft());
    }
    if (auto* op = dynamic_cast<const QoreSquareBracketsRangeOperatorNode*>(node)) {
        return extractLValueBaseVarRef(op->get(0));
    }
    if (auto* op = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(node)) {
        return extractLValueBaseVarRef(op->getLeft());
    }
    if (auto* op = dynamic_cast<const QoreShiftOperatorNode*>(node)) {
        return extractLValueBaseVarRef(op->getExp());
    }
    if (auto* op = dynamic_cast<const QoreUnshiftOperatorNode*>(node)) {
        return extractLValueBaseVarRef(op->getLeft());
    }
    if (auto* op = dynamic_cast<const QoreSpliceOperatorNode*>(node)) {
        return extractLValueBaseVarRef(op->getLValue());
    }
    if (auto* op = dynamic_cast<const QorePreIncrementOperatorNode*>(node)) {
        return extractLValueBaseVarRef(op->getExp());
    }
    if (auto* op = dynamic_cast<const QorePostIncrementOperatorNode*>(node)) {
        return extractLValueBaseVarRef(op->getExp());
    }
    if (auto* op = dynamic_cast<const QorePreDecrementOperatorNode*>(node)) {
        return extractLValueBaseVarRef(op->getExp());
    }
    if (auto* op = dynamic_cast<const QorePostDecrementOperatorNode*>(node)) {
        return extractLValueBaseVarRef(op->getExp());
    }
    return nullptr;
}

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

// Helper to check if a weak reference's target object is still valid
// Returns the value if valid, or NOTHING if the weak reference target was deleted
static QoreValue validateWeakRef(const QoreValue& val) {
    switch (val.getType()) {
        case NT_WEAKREF: {
            QoreObject* o = val.get<const WeakReferenceNode>()->get();
            if (!o->isValid()) {
                return QoreValue();
            }
            return val;
        }
        case NT_WEAKREF_HASH: {
            QoreHashNode* h = val.get<const WeakHashReferenceNode>()->get();
            // Hashes and lists don't have an "invalid" state, so return as-is
            return val;
        }
        case NT_WEAKREF_LIST: {
            QoreListNode* l = val.get<const WeakListReferenceNode>()->get();
            // Hashes and lists don't have an "invalid" state, so return as-is
            return val;
        }
        default:
            return val;
    }
}

QoreValue QoreIRInterpreter::evalComparison(QoreIROpcode op, const QoreValue& left, const QoreValue& right,
        ExceptionSink* xsink) {
    switch (op) {
        case QoreIROpcode::EqInt:
            return QoreValue(left.getAsBigInt() == right.getAsBigInt());
        case QoreIROpcode::EqFloat:
            return QoreValue(left.getAsFloat() == right.getAsFloat());
        case QoreIROpcode::EqString: {
            const QoreStringNode* lstr = left.get<const QoreStringNode>();
            const QoreStringNode* rstr = right.get<const QoreStringNode>();
            return QoreValue(lstr && rstr && lstr->equal(rstr));
        }
        case QoreIROpcode::EqAny:
            return QoreValue(QoreLogicalEqualsOperatorNode::softEqual(left, right, xsink));
        case QoreIROpcode::NeInt:
            return QoreValue(left.getAsBigInt() != right.getAsBigInt());
        case QoreIROpcode::NeFloat:
            return QoreValue(left.getAsFloat() != right.getAsFloat());
        case QoreIROpcode::NeString: {
            const QoreStringNode* lstr = left.get<const QoreStringNode>();
            const QoreStringNode* rstr = right.get<const QoreStringNode>();
            return QoreValue(!lstr || !rstr || !lstr->equal(rstr));
        }
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
        case QoreIROpcode::LtString: {
            const QoreStringNode* lstr = left.get<const QoreStringNode>();
            const QoreStringNode* rstr = right.get<const QoreStringNode>();
            return QoreValue(lstr && rstr && (lstr->compare(rstr) < 0));
        }
        case QoreIROpcode::LtAny:
            return QoreValue(QoreLogicalLessThanOperatorNode::doLessThan(left, right, xsink));
        case QoreIROpcode::LeInt:
            return QoreValue(left.getAsBigInt() <= right.getAsBigInt());
        case QoreIROpcode::LeFloat:
            return QoreValue(left.getAsFloat() <= right.getAsFloat());
        case QoreIROpcode::LeString: {
            const QoreStringNode* lstr = left.get<const QoreStringNode>();
            const QoreStringNode* rstr = right.get<const QoreStringNode>();
            return QoreValue(lstr && rstr && (lstr->compare(rstr) <= 0));
        }
        case QoreIROpcode::LeAny:
            return QoreValue(QoreLogicalLessThanOrEqualsOperatorNode::doLessThanOrEquals(left, right, xsink));
        case QoreIROpcode::GtInt:
            return QoreValue(left.getAsBigInt() > right.getAsBigInt());
        case QoreIROpcode::GtFloat:
            return QoreValue(left.getAsFloat() > right.getAsFloat());
        case QoreIROpcode::GtString: {
            const QoreStringNode* lstr = left.get<const QoreStringNode>();
            const QoreStringNode* rstr = right.get<const QoreStringNode>();
            return QoreValue(lstr && rstr && (lstr->compare(rstr) > 0));
        }
        case QoreIROpcode::GtAny:
            return QoreValue(QoreLogicalGreaterThanOperatorNode::doGreaterThan(left, right, xsink));
        case QoreIROpcode::GeInt:
            return QoreValue(left.getAsBigInt() >= right.getAsBigInt());
        case QoreIROpcode::GeFloat:
            return QoreValue(left.getAsFloat() >= right.getAsFloat());
        case QoreIROpcode::GeString: {
            const QoreStringNode* lstr = left.get<const QoreStringNode>();
            const QoreStringNode* rstr = right.get<const QoreStringNode>();
            return QoreValue(lstr && rstr && (lstr->compare(rstr) >= 0));
        }
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
        case QoreIROpcode::CmpString: {
            const QoreStringNode* lstr = left.get<const QoreStringNode>();
            const QoreStringNode* rstr = right.get<const QoreStringNode>();
            int64_t result = 0;
            if (lstr && rstr) {
                int cmp = lstr->compare(rstr);
                result = cmp < 0 ? -1 : (cmp > 0 ? 1 : 0);  // normalize to -1/0/1
            }
            return QoreValue(result);
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
        case QoreIROpcode::CallDirect:
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
        case QoreIROpcode::MapAny:
        case QoreIROpcode::MapInt:
        case QoreIROpcode::MapFloat:
        case QoreIROpcode::SelectAny:
        case QoreIROpcode::SelectInt:
        case QoreIROpcode::SelectFloat:
        case QoreIROpcode::FoldlAny:
        case QoreIROpcode::FoldlInt:
        case QoreIROpcode::FoldlFloat:
        case QoreIROpcode::FoldrAny:
        case QoreIROpcode::FoldrInt:
        case QoreIROpcode::FoldrFloat:
        case QoreIROpcode::FoldrSumInt:
        case QoreIROpcode::FoldrSumFloat:
        case QoreIROpcode::FoldrProdInt:
        case QoreIROpcode::FoldrProdFloat:
        case QoreIROpcode::FoldrDiffInt:
        case QoreIROpcode::FoldrDiffFloat:
        case QoreIROpcode::FoldrMinInt:
        case QoreIROpcode::FoldrMinFloat:
        case QoreIROpcode::FoldrMaxInt:
        case QoreIROpcode::FoldrMaxFloat:
        case QoreIROpcode::MapSelectAny:
        case QoreIROpcode::MapSelectList:
        case QoreIROpcode::HashMapAny:
        case QoreIROpcode::HashMap:
        case QoreIROpcode::HashMapSelectAny:
        case QoreIROpcode::HashMapSelect:
            return evalExprNode(expr, xsink);
        case QoreIROpcode::InvokeSimError: {
            if (xsink) {
                xsink->raiseException("IR-INVOKE-SIM-ERROR", "invoke simulated error");
            }
            return QoreValue();
        }
        // Cast opcodes are handled natively in the main execution loop
        // using operand[0] — they should not reach evalExpr
        case QoreIROpcode::CastList:
        case QoreIROpcode::CastHash:
        case QoreIROpcode::CastObject:
        case QoreIROpcode::CastEnum:
        case QoreIROpcode::CastAny:
            assert(false);
            return QoreValue();
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
        case QoreIROpcode::Foreach:
            if (!stmt) {
                if (xsink) {
                    xsink->raiseException("IR-INTERPRETER-ERROR", "foreach statement requires a statement");
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

static QoreValue getIRValue(const std::vector<QoreValue>& values, QoreIRValue id) {
    if (!id.isValid() || id.id >= values.size()) {
        return QoreValue();
    }
    return values[id.id];
}

static void removeCleanupEntry(std::vector<uint32_t>& cleanup, uint32_t id) {
    for (auto it = cleanup.rbegin(); it != cleanup.rend(); ++it) {
        if (*it == id) {
            cleanup.erase(std::next(it).base());
            return;
        }
    }
}

// Ensures the values vector is large enough to hold the given slot ID
static inline void ensureValueSlotSize(std::vector<QoreValue>& values, uint32_t id) {
    if (id >= values.size()) {
        // Grow vector exponentially to amortize allocation cost
        size_t new_size = std::max((size_t)(id + 1), values.size() * 2);
        values.resize(new_size);
    }
}

// Sets a value slot without discarding the previous value (for simple assignments like Const)
static inline void setValueSlotDirect(std::vector<QoreValue>& values, uint32_t id, QoreValue new_val) {
    ensureValueSlotSize(values, id);
    values[id] = new_val;
}

// Sets a value slot, discarding any previous value to prevent leaks in loops.
// Each slot holds a +1 reference (from new, refSelf, or call result), so
// discarding on overwrite is safe — SSA guarantees the old value is from a
// prior iteration and no longer needed by the current computation.
static void setValueSlot(std::vector<QoreValue>& values,
        uint32_t id, QoreValue new_val, ExceptionSink* xsink) {
    ensureValueSlotSize(values, id);
    values[id].discard(xsink);
    values[id] = new_val;
}

static void cleanupValues(std::vector<QoreValue>& values, std::vector<uint32_t>& cleanup,
        ExceptionSink* xsink, bool no_throw, std::vector<std::string>* cleanup_log) {
    // Track cleaned value slot IDs to handle duplicates from loop iterations.
    // When an instruction executes multiple times, it may push its result ID multiple
    // times, but values[id] only holds the final value. We clean each slot once.
    std::unordered_set<uint32_t> cleaned_ids;
    for (auto it = cleanup.rbegin(); it != cleanup.rend(); ++it) {
        if (cleaned_ids.count(*it)) {
            continue;  // Already cleaned this slot
        }
        cleaned_ids.insert(*it);
        if (*it >= values.size()) {
            continue;
        }
        QoreValue temp = values[*it];
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
        // Clear the entry to prevent stale pointer access if execution
        // continues (e.g., after an exception is caught in a loop).
        // Set to NOTHING (bits=0) instead of erasing.
        values[*it] = QoreValue();
    }
    cleanup.clear();
}

static void cleanupStoredValues(std::unordered_map<const void*, QoreValue>& values, ExceptionSink* xsink) {
    for (auto& entry : values) {
        entry.second.discard(xsink);
    }
    values.clear();
}

// Diagnostic tracking for cache clearing operations (debug only)
// Can be enabled with QORE_DIAG_CACHE_CLEAR environment variable
// This uses thread-local storage, so each thread tracks its own statistics
// At program exit, the main thread's diagnostics are printed
struct CacheClearDiagnostics {
    int64_t cleanup_count = 0;
    int64_t total_locals_cleared = 0;
    int64_t total_globals_cleared = 0;
    int64_t total_threadlocals_cleared = 0;
    int64_t total_closures_cleared = 0;
    int64_t total_slot_cache_cleared = 0;

    static CacheClearDiagnostics& instance() {
        static thread_local CacheClearDiagnostics inst;
        return inst;
    }

    static void printAtExit() {
        CacheClearDiagnostics& inst = instance();
        const char* diag_env = getenv("QORE_DIAG_CACHE_CLEAR");
        if (!diag_env || !*diag_env) {
            return;
        }
        if (inst.cleanup_count == 0) {
            return;
        }
        fprintf(stderr, "\n=== Cache Clearing Diagnostics (main thread) ===\n");
        fprintf(stderr, "Total cleanup calls: %ld\n", inst.cleanup_count);
        fprintf(stderr, "Avg locals per cleanup: %.1f\n", (double)inst.total_locals_cleared / inst.cleanup_count);
        fprintf(stderr, "Avg globals per cleanup: %.1f\n", (double)inst.total_globals_cleared / inst.cleanup_count);
        fprintf(stderr, "Avg threadlocals per cleanup: %.1f\n", (double)inst.total_threadlocals_cleared / inst.cleanup_count);
        fprintf(stderr, "Avg closures per cleanup: %.1f\n", (double)inst.total_closures_cleared / inst.cleanup_count);
        fprintf(stderr, "Avg slot cache entries per cleanup: %.1f\n", (double)inst.total_slot_cache_cleared / inst.cleanup_count);
        int64_t total_items = inst.total_locals_cleared + inst.total_globals_cleared + inst.total_threadlocals_cleared +
            inst.total_closures_cleared + inst.total_slot_cache_cleared;
        fprintf(stderr, "Total items cleared: %ld\n", total_items);
    }

    void recordCleanup(int locals_sz, int globals_sz, int threadlocals_sz, int closures_sz, int slot_cache_sz) {
        cleanup_count++;
        total_locals_cleared += locals_sz;
        total_globals_cleared += globals_sz;
        total_threadlocals_cleared += threadlocals_sz;
        total_closures_cleared += closures_sz;
        total_slot_cache_cleared += slot_cache_sz;
    }
};

// Register diagnostics printer at program exit
namespace {
    struct CacheClearDiagInit {
        CacheClearDiagInit() {
            atexit(CacheClearDiagnostics::printAtExit);
        }
    } cache_clear_diag_init;
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
        const std::unordered_set<const LocalVar*>* pre_instantiated = nullptr,
        const std::unordered_set<const void*>* function_own_locals = nullptr,
        std::unordered_set<const LocalVar*>* locally_uninstantiated = nullptr) {
    if (!var) {
        return;
    }
    // When function_own_locals is provided, variables NOT in the set are outer-scope
    // variables (e.g. top-level locals accessed from a sub, or enclosing-function
    // params accessed from an on_exit handler body).  These are already on the
    // thread-local stack from the calling scope and must NOT be re-instantiated
    // (which would shadow the existing value with NOTHING) or tracked for cleanup.
    if (function_own_locals
            && !function_own_locals->count(reinterpret_cast<const void*>(var))) {
        return;
    }
    if (locals.insert(var).second) {
        // Pre-instantiated variables (params, argvid, selfid, ast_visible_body_locals)
        // are already on the thread-local stack from evalTiered and must never be
        // re-instantiated by the IR interpreter.  When UninstantiateLocal processes a
        // pre-instantiated variable (e.g., at the end of a loop iteration), it only
        // clears the value (clearValue/del) without popping the stack entry, so the
        // entry remains on the stack for reuse on the next iteration.
        bool skip_instantiation = pre_instantiated
            && pre_instantiated->find(var) != pre_instantiated->end();

        if (!skip_instantiation) {
            var->instantiate(QoreParseOptions());
        }
        // Clear the "locally uninstantiated" flag if present
        if (locally_uninstantiated) {
            locally_uninstantiated->erase(var);
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
    // Use the thread-local cvstack lookup (by name) to find the topmost closure
    // variable, which is the current function's own variable.  This is critical
    // when a function with closure-use variables is called from within a closure:
    // thread_get_runtime_closure_var() would return the calling closure's variable
    // (wrong scope), while thread_find_closure_var() returns the current function's
    // own cvstack variable (correct scope).
    ClosureVarValue* cv = thread_find_closure_var(var->getName());
    if (!cv) {
        cv = thread_get_runtime_closure_var(var);
    }
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


static void updateLocalVarFromLvalue(std::unordered_map<const void*, QoreValue>& locals,
        std::unordered_set<const LocalVar*>& instantiated_locals, const QoreValue& lvalue,
        const QoreValue& value, ExceptionSink* xsink,
        const std::unordered_set<const LocalVar*>* pre_instantiated = nullptr,
        const std::unordered_set<const void*>* function_own_locals = nullptr,
        std::unordered_set<const LocalVar*>* locally_uninstantiated = nullptr,
        const std::unordered_map<const LocalVar*, uint32_t>* local_var_slots = nullptr,
        std::vector<QoreValue>* locals_slot_cache_ptr = nullptr) {
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
        ensureLocalInstantiated(var_ref->ref.id, instantiated_locals, pre_instantiated,
                function_own_locals, locally_uninstantiated);
        // Invalidate the cache entry (don't pre-populate) because
        // assignLocalVarValue() → acceptAssignment() may coerce the value type.
        // The next LoadLocal cache miss will read the actual coerced value.
        auto cache_it = locals.find(var_ref->ref.id);
        if (cache_it != locals.end()) {
            cache_it->second.discard(xsink);
            locals.erase(cache_it);
        }
        if (local_var_slots && locals_slot_cache_ptr) {
            const LocalVar* lv = reinterpret_cast<const LocalVar*>(var_ref->ref.id);
            auto slot_it = local_var_slots->find(lv);
            if (slot_it != local_var_slots->end()
                    && slot_it->second < locals_slot_cache_ptr->size()) {
                (*locals_slot_cache_ptr)[slot_it->second].discard(xsink);
                (*locals_slot_cache_ptr)[slot_it->second] = QoreValue();
            }
        }
        assignLocalVarValue(var_ref->ref.id, value, xsink);
    }
}

static QoreListNode* buildArgList(const std::vector<QoreValue>& values,
        const std::vector<QoreIRValue>& operands, size_t start_index, ExceptionSink* xsink) {
    QoreListNode* args = new QoreListNode(autoTypeInfo);
    qore_list_private* priv = qore_list_private::get(*args);
    priv->reserve(operands.size() - start_index);
    for (size_t i = start_index; i < operands.size(); ++i) {
        QoreValue val = getIRValue(values, operands[i]);
        QoreValue stored = val.hasNode() ? val.refSelf() : val;
        // Use pushIntern() to bypass checkVal/stripVal which strips complex types
        // (e.g., hash<string, bool> -> hash<auto>) from arguments
        priv->pushIntern(stored);
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
        const std::vector<QoreValue>& values, ExceptionSink* xsink) {
    QoreIROpcode op = inv->invoke_opcode;
    switch (op) {
        // Unary computation opcodes
        case QoreIROpcode::ToBool:
        case QoreIROpcode::Not:
        case QoreIROpcode::IsNullOrNothing:
        case QoreIROpcode::UnaryPlusAny:
        case QoreIROpcode::UnaryMinusInt:
        case QoreIROpcode::UnaryMinusFloat:
        case QoreIROpcode::UnaryMinusAny:
        case QoreIROpcode::ExistsAny:
        case QoreIROpcode::ExistsBool: {
            QoreValue val = inv->operands.empty() ? QoreValue() : getIRValue(values, inv->operands[0]);
            return QoreIRInterpreter::evalUnary(op, val, xsink);
        }
        // Binary computation opcodes (arithmetic, bitwise, compound assignments, comparisons, etc.)
        case QoreIROpcode::AddInt:
        case QoreIROpcode::AddFloat:
        case QoreIROpcode::AddAny:
        case QoreIROpcode::AddString:
        case QoreIROpcode::StringConcat:
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
        case QoreIROpcode::EqString:
        case QoreIROpcode::EqAny:
        case QoreIROpcode::NeInt:
        case QoreIROpcode::NeFloat:
        case QoreIROpcode::NeString:
        case QoreIROpcode::NeAny:
        case QoreIROpcode::EqHard:
        case QoreIROpcode::NeHard:
        case QoreIROpcode::LtInt:
        case QoreIROpcode::LtFloat:
        case QoreIROpcode::LtString:
        case QoreIROpcode::LtAny:
        case QoreIROpcode::LeInt:
        case QoreIROpcode::LeFloat:
        case QoreIROpcode::LeString:
        case QoreIROpcode::LeAny:
        case QoreIROpcode::GtInt:
        case QoreIROpcode::GtFloat:
        case QoreIROpcode::GtString:
        case QoreIROpcode::GtAny:
        case QoreIROpcode::GeInt:
        case QoreIROpcode::GeFloat:
        case QoreIROpcode::GeString:
        case QoreIROpcode::GeAny:
        case QoreIROpcode::CmpInt:
        case QoreIROpcode::CmpFloat:
        case QoreIROpcode::CmpString:
        case QoreIROpcode::CmpAny:
        case QoreIROpcode::FoldlAny:
        case QoreIROpcode::FoldlInt:
        case QoreIROpcode::FoldlFloat:
        case QoreIROpcode::FoldrAny:
        case QoreIROpcode::FoldrInt:
        case QoreIROpcode::FoldrFloat:
        case QoreIROpcode::FoldrSumInt:
        case QoreIROpcode::FoldrSumFloat:
        case QoreIROpcode::FoldrProdInt:
        case QoreIROpcode::FoldrProdFloat:
        case QoreIROpcode::FoldrDiffInt:
        case QoreIROpcode::FoldrDiffFloat:
        case QoreIROpcode::FoldrMinInt:
        case QoreIROpcode::FoldrMinFloat:
        case QoreIROpcode::FoldrMaxInt:
        case QoreIROpcode::FoldrMaxFloat:
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
        case QoreIROpcode::FusedMapFoldlSumScaleInt:
        case QoreIROpcode::FusedMapFoldlSumScaleFloat:
        case QoreIROpcode::FusedMapFoldlSumSquareInt:
        case QoreIROpcode::FusedMapFoldlSumSquareFloat:
        case QoreIROpcode::FusedMapFoldlProdScaleInt:
        case QoreIROpcode::FusedMapFoldlProdScaleFloat:
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
        case QoreIROpcode::CallDirect:
        case QoreIROpcode::CallIndirect:
        case QoreIROpcode::CallMethod:
        case QoreIROpcode::CallStatic:
        case QoreIROpcode::CallStaticDirect: {
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
                if (op == QoreIROpcode::Call || op == QoreIROpcode::CallDirect) {
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
                } else if (op == QoreIROpcode::CallStatic || op == QoreIROpcode::CallStaticDirect) {
                    if (auto* call = dynamic_cast<const StaticMethodCallNode*>(
                            inv->expr.getInternalNode())) {
                        QoreValue call_expr(new StaticMethodCallNode(*call, arg_list));
                        ValueHolder call_holder(call_expr, nullptr);
                        res = QoreIRInterpreter::evalExpr(QoreIROpcode::CallStatic, call_expr, xsink);
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

        // NewObject: construct object directly without VarRefNewObjectNode assignment
        // VarRefNewObjectNode::evalImpl assigns to the local variable internally, but
        // the IR lowering emits a separate StoreLocal for that. Going through evalExprNode
        // would cause a double-assignment, leaking one reference.
        case QoreIROpcode::NewObject: {
            if (inv->expr.hasNode()) {
                auto* vrn = dynamic_cast<const VarRefNewObjectNode*>(inv->expr.getInternalNode());
                if (vrn) {
                    const QoreClass* qc = QoreTypeInfo::getUniqueReturnClass(vrn->getTypeInfo());
                    if (qc) {
                        RuntimeConfig& rc = rc_get_current_ref();
                        return qore_class_private::execConstructor(*qc, rc,
                            vrn->getVariant(), vrn->getArgs(), xsink);
                    }
                }
            }
            return QoreIRInterpreter::evalExpr(op, inv->expr, xsink);
        }

        // VrnConstruct: construct hashdecl/complex types without local variable assignment
        case QoreIROpcode::VrnConstruct: {
            if (inv->expr.hasNode()) {
                auto* vrn = dynamic_cast<const VarRefNewObjectNode*>(inv->expr.getInternalNode());
                if (vrn) {
                    return vrn->constructValue(xsink);
                }
            }
            return QoreIRInterpreter::evalExpr(op, inv->expr, xsink);
        }

        // ListPush invoke: native list push with pre-evaluated operands
        case QoreIROpcode::ListPush: {
            QoreValue list_val = inv->operands.size() > 0 ? getIRValue(values, inv->operands[0]) : QoreValue();
            QoreValue push_val = inv->operands.size() > 1 ? getIRValue(values, inv->operands[1]) : QoreValue();
            if (list_val.getType() == NT_LIST) {
                QoreListNode* l = list_val.get<QoreListNode>();
                l->push(push_val.refSelf(), xsink);
                // refSelf: result shares same list pointer as operand; both are in
                // cleanup, so both need their own reference
                return list_val.refSelf();
            }
            if (list_val.isNothing()) {
                QoreListNode* l = new QoreListNode(autoTypeInfo);
                l->push(push_val.refSelf(), xsink);
                return QoreValue(l);
            }
            if (xsink) {
                xsink->raiseException("PUSH-ERROR",
                    "the lvalue argument to push is type \"%s\"; expecting \"list\"",
                    list_val.getTypeName());
            }
            return QoreValue();
        }

        // Cast opcodes: native cast with pre-evaluated inner value (operand[0])
        case QoreIROpcode::CastList:
        case QoreIROpcode::CastHash:
        case QoreIROpcode::CastObject:
        case QoreIROpcode::CastEnum:
        case QoreIROpcode::CastAny: {
            QoreValue inner = inv->operands.empty() ? QoreValue() : getIRValue(values, inv->operands[0]);
            auto* cast_node = dynamic_cast<const QoreCastOperatorNode*>(inv->expr.getInternalNode());
            if (cast_node) {
                return cast_node->castValue(inner, xsink);
            }
            // Fallback for unresolved CastAny
            return QoreIRInterpreter::evalExpr(op, inv->expr, xsink);
        }

        // CallClosureDirect: call closure/callref with pre-evaluated operands
        // Without this, CallClosureDirect falls through to AST eval which ignores
        // the pre-evaluated callee and args from lowerCallReference()
        case QoreIROpcode::CallClosureDirect: {
            if (!inv->operands.empty()) {
                QoreValue ref_val = getIRValue(values, inv->operands[0]);
                if (!ref_val.hasNode()) {
                    if (xsink) {
                        xsink->raiseException("CALL-REFERENCE-ERROR",
                            "cannot call a NOTHING value as a closure/call reference");
                    }
                    return QoreValue();
                }
                ResolvedCallReferenceNode* callref = dynamic_cast<ResolvedCallReferenceNode*>(
                    const_cast<AbstractQoreNode*>(ref_val.getInternalNode()));
                if (!callref) {
                    if (xsink) {
                        xsink->raiseException("CALL-REFERENCE-ERROR",
                            "value is not a call reference or closure");
                    }
                    return QoreValue();
                }
                int nargs = static_cast<int>(inv->operands.size()) - 1;
                QoreListNode* arg_list = nargs > 0 ? new QoreListNode(autoTypeInfo) : nullptr;
                if (arg_list) {
                    for (int i = 0; i < nargs; ++i) {
                        QoreValue val = getIRValue(values, inv->operands[i + 1]);
                        if (val.hasNode()) {
                            val.refSelf();
                        }
                        arg_list->push(val, xsink);
                    }
                }
                // execValue borrows args (const QoreListNode*) — does NOT take ownership
                QoreValue result = callref->execValue(arg_list, xsink);
                if (arg_list) {
                    arg_list->deref(xsink);
                }
                return result;
            }
            return QoreIRInterpreter::evalExpr(op, inv->expr, xsink);
        }

        // StoreLValue invoke: use pre-evaluated RHS operand and weak flag
        // instead of delegating to AST (which would re-evaluate the RHS).
        // inv->expr is the full assignment expression; extract the lvalue from it.
        case QoreIROpcode::StoreLValue: {
            if (!inv->operands.empty() && inv->expr.hasNode()) {
                auto* assign = dynamic_cast<const QoreAssignmentOperatorNode*>(
                    inv->expr.getInternalNode());
                if (assign) {
                    QoreValue val = getIRValue(values, inv->operands[0]);
                    return QoreIRInterpreter::evalLValueStore(assign->getLeft(), val, xsink,
                        inv->weak);
                }
            }
            return QoreIRInterpreter::evalExpr(op, inv->expr, xsink);
        }

        // Binary compound assignment lvalue opcodes: extract the lvalue from the
        // AST expression and use the pre-evaluated RHS operand.  This avoids
        // re-evaluating the RHS through the full AST path and ensures correct
        // lvalue semantics via evalLValueBinary (which uses LValueHelper).
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
            if (!inv->operands.empty() && inv->expr.hasNode()) {
                auto* binop = dynamic_cast<const QoreBinaryOperatorNode<LValueOperatorNode>*>(
                    inv->expr.getInternalNode());
                if (binop) {
                    QoreValue right = getIRValue(values, inv->operands[0]);
                    return QoreIRInterpreter::evalLValueBinary(op, binop->getLeft(), right, xsink);
                }
            }
            return QoreIRInterpreter::evalExpr(op, inv->expr, xsink);
        }

        // Unary lvalue opcodes: extract the lvalue from the AST and delegate
        // to evalLValueUnary which handles shift, pre/post increment/decrement.
        case QoreIROpcode::ShiftLValue:
        case QoreIROpcode::PreIncLValue:
        case QoreIROpcode::PreDecLValue:
        case QoreIROpcode::PostIncLValue:
        case QoreIROpcode::PostDecLValue: {
            if (inv->expr.hasNode()) {
                auto* unaryop = dynamic_cast<const QoreSingleExpressionOperatorNode<LValueOperatorNode>*>(
                    inv->expr.getInternalNode());
                if (unaryop) {
                    return QoreIRInterpreter::evalLValueUnary(op, unaryop->getExp(), xsink);
                }
            }
            return QoreIRInterpreter::evalExpr(op, inv->expr, xsink);
        }

        // Ternary lvalue opcodes: extract the lvalue and use pre-evaluated operands.
        case QoreIROpcode::SpliceLValue: {
            if (inv->operands.size() >= 3 && inv->expr.hasNode()) {
                auto* spliceop = dynamic_cast<const QoreSpliceOperatorNode*>(
                    inv->expr.getInternalNode());
                if (spliceop) {
                    QoreValue offset = getIRValue(values, inv->operands[0]);
                    QoreValue length = getIRValue(values, inv->operands[1]);
                    QoreValue replacement = getIRValue(values, inv->operands[2]);
                    return QoreIRInterpreter::evalLValueTernary(op, spliceop->getLValue(), offset,
                        length, replacement, xsink);
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
    const QoreIRFunction* handler_ir = nullptr;  //!< compiled handler (nullptr = AST fallback)
};

// Execute a single on_block_exit handler body, using compiled IR if available, otherwise AST.
// Returns true if execution encountered an error.
static void executeHandlerBody(const IROnBlockExitHandler& handler, ExceptionSink* obe_xsink) {
    if (handler.handler_ir) {
        // Execute compiled handler via IR interpreter
        QoreValue rv;
        QoreIRInterpreter::execute(*handler.handler_ir, rv, obe_xsink);
        rv.discard(obe_xsink);
    } else if (handler.code) {
        // AST fallback
        QoreValue rv;
        handler.code->exec(rv, obe_xsink);
    }
}

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
            if (handlers[i].code || handlers[i].handler_ir) {
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
                    executeHandlerBody(handlers[i], &obe_xsink);
                    // Restore td->catchException BEFORE clearing xsink to avoid
                    // a use-after-free window where td->catchException points to
                    // the freed exception chain
                    ex_helper.reset();
                    argv_helper.reset();
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
    // Clear handlers to prevent double-firing from RAII scope_exit_guard
    handlers.clear();
    return nrc;
}

// Placeholder class - QoreIRStackLocation was removed because inheriting from
// QoreProgramStackLocationHelper caused IR frames to appear in exception callstacks,
// resulting in wrong line numbers compared to AST mode.
// Exception callstacks should only contain frames at exception propagation points,
// which in normal AST execution doesn't include IR internal frames.
//
// The correct approach is to update runtime_loc via per-instruction location updates
// in the main execute() loop for use by AST evaluation code (CodeEvaluationHelper),
// without pushing frames onto the stack location chain.
//
// Issue: Normal mode showed frame 0 line 561 (call site), IR mode showed line 552 (throw site)
// because QoreIRStackLocation was in the stack location chain at the time the exception was
// created, capturing the throw statement's location instead of letting the AST evaluator
// handle the exception location correctly.
class QoreIRStackLocation {
    // Unused stub - kept to avoid breaking anything that might reference this class
};

bool QoreIRInterpreter::execute(const QoreIRFunction& func, QoreValue& return_value, ExceptionSink* xsink,
        std::vector<std::string>* cleanup_log, const std::vector<QoreValue>* args,
        const std::vector<QoreValue>* closure, const std::unordered_set<const LocalVar*>* pre_instantiated,
        const LocalVar* excluded_selfid, const StatementBlock* statements, QoreProgram* pgm, bool suppress_guard_deopt) {
#ifdef QORE_MANAGE_STACK
    if (check_stack(xsink)) {
        return false;
    }
#endif
    // Use vector for IR value slots for O(1) direct index access instead of O(1) hash lookup
    // QoreIRFunction assigns IDs sequentially starting from 1, so vector[id] is direct access
    std::vector<QoreValue> values;
    // Right-size the reservation based on actual max_value_id from the function
    // to avoid unnecessary allocations for small functions while allowing growth for large ones
    size_t reserve_size = func.max_value_id > 0 ? func.max_value_id + 1 : 128;
    values.reserve(reserve_size);
    std::vector<uint32_t> cleanup;
    std::unordered_map<const void*, QoreValue> locals;

    // Phase 3 optimization: Flat array for fast local variable cache access
    // Maps slot IDs to cached values for O(1) direct array access (vs O(1) hash lookup)
    // This cache supplements the traditional locals map for hot paths (LoadLocal/StoreLocal)
    std::vector<QoreValue> locals_slot_cache;
    if (func.max_local_slot_id > 0) {
        locals_slot_cache.resize(func.max_local_slot_id + 1);
    }

    std::unordered_set<const LocalVar*> instantiated_locals;
    // Track pre_instantiated variables that have been explicitly uninstantiated by
    // UninstantiateLocal during execution (e.g., loop-scope variables).
    // These must be re-instantiated by ensureLocalInstantiated on next use, even though
    // they appear in pre_instantiated (the caller won't re-instantiate them per-iteration).
    std::unordered_set<const LocalVar*> locally_uninstantiated;
    // Track which value slots hold VarRefNewObjectNode results for each LocalVar
    // Used to cleanup references when UninstantiateLocal is processed
    std::unordered_map<const LocalVar*, uint32_t> local_init_slots;
    // Track value slots from LoadLocal instructions for each LocalVar.
    // When UninstantiateLocal fires, these slots must be cleaned up so the
    // block-scoped object's refcount drops to zero and its destructor runs
    // immediately — not deferred to function exit via cleanupValues().
    std::unordered_map<const LocalVar*, std::unordered_set<uint32_t>> local_load_slots;
    std::unordered_map<const void*, QoreValue> globals;
    std::unordered_map<const void*, QoreValue> threadlocals;
    std::unordered_map<const void*, QoreValue> closures;
    // Track active iterators (from IteratorCreate/IteratorCreateReverse) for cleanup
    // on non-normal exit paths. IteratorCreate stores the iterator pointer as int64_t
    // in the values map; cleanupValues() treats int64_t as a plain number (no delete).
    std::unordered_set<FunctionalOperatorInterface*> active_iterators;
    // on_block_exit handlers collected during IR execution
    std::vector<IROnBlockExitHandler> on_block_exit_handlers;
    // Catch exception stack for rethrow support
    struct CatchEntry {
        QoreException* caught;  // the caught exception
        QoreException* saved;   // the previous td->catchException value
    };
    std::vector<CatchEntry> catch_exception_stack;
    // RAII cleanup for catch_exception_stack — ensures proper cleanup on all exit paths
    struct CatchStackCleanup {
        std::vector<CatchEntry>& stack;
        ExceptionSink* xsink;
        ~CatchStackCleanup() {
            while (!stack.empty()) {
                auto entry = stack.back();
                stack.pop_back();
                if (entry.caught) {
                    catch_swap_exception(entry.saved);
                    entry.caught->del(xsink);
                }
            }
        }
    } catch_stack_cleanup{catch_exception_stack, xsink};
    // Scope stack: tracks handler list indices at each ScopeEnter
    // Used to know which handlers to execute on ScopeExit
    std::vector<size_t> scope_stack;

    // Helper to fire on_block_exit handlers for all scopes from current depth down to target_depth.
    // Used by Throw/Rethrow handlers (no-exception-target case) to fire scope exits after
    // the exception is raised on xsink, ensuring on_error handlers see the active exception.
    auto fireScopeExits = [&](size_t target_depth = 0) {
        while (scope_stack.size() > target_depth) {
            size_t scope_start = scope_stack.back();
            scope_stack.pop_back();
            if (on_block_exit_handlers.size() > scope_start) {
                bool error = xsink && xsink->isException();
                ExceptionSink obe_xsink;
                for (size_t i = on_block_exit_handlers.size(); i > scope_start; --i) {
                    obe_type_e type = on_block_exit_handlers[i - 1].type;
                    if (type == OBE_Unconditional || (error && type == OBE_Error)) {
                        if (on_block_exit_handlers[i - 1].code || on_block_exit_handlers[i - 1].handler_ir) {
                            std::unique_ptr<SingleArgvContextHelper> argv_helper;
                            std::unique_ptr<CatchExceptionHelper> ex_helper;
                            if (type == OBE_Error && xsink) {
                                QoreException* except = xsink->getException();
                                if (except) {
                                    ex_helper.reset(new CatchExceptionHelper(except));
                                    argv_helper.reset(new SingleArgvContextHelper(
                                        except->makeExceptionObject(), xsink));
                                }
                            }
                            executeHandlerBody(on_block_exit_handlers[i - 1], &obe_xsink);
                            // Restore td->catchException BEFORE clearing xsink to avoid
                            // a use-after-free window where td->catchException points to
                            // the freed exception chain
                            ex_helper.reset();
                            argv_helper.reset();
                            if (type == OBE_Error) {
                                if (qore_es_private::get(obe_xsink)->rethrown) {
                                    if (xsink) {
                                        xsink->clear();
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
                on_block_exit_handlers.resize(scope_start);
                // Invalidate caches after handler execution (both AST and compiled handlers
                // can modify any variable type on the thread-local variable stack)
                cleanupStoredValues(locals, xsink);
                cleanupStoredValues(globals, xsink);
                cleanupStoredValues(threadlocals, xsink);
                cleanupStoredValues(closures, xsink);
                // Also clear slot cache without a separate lambda call
                for (size_t i = 0; i < locals_slot_cache.size(); ++i) {
                    locals_slot_cache[i].discard(xsink);
                    locals_slot_cache[i] = QoreValue();
                }
            }
        }
    };

    // Helper to clear all variable caches (locals, globals, threadlocals, closures, and slot cache)
    // after AST function/method calls. The calls may have modified any of these variable types,
    // so all caches must be invalidated to force re-read from the runtime stack on next access.
    auto cleanupLocalCaches = [&]() {
        // Record diagnostics before clearing (for accurate cache size measurement)
        int locals_sz = locals.size();
        int globals_sz = globals.size();
        int threadlocals_sz = threadlocals.size();
        int closures_sz = closures.size();
        int slot_cache_sz = locals_slot_cache.size();

        cleanupStoredValues(locals, xsink);
        cleanupStoredValues(globals, xsink);
        cleanupStoredValues(threadlocals, xsink);
        cleanupStoredValues(closures, xsink);
        // Clear the fast-path slot cache as well — without this, stale slot cache hits
        // would return pre-call values instead of live values modified by the call.
        for (size_t i = 0; i < locals_slot_cache.size(); ++i) {
            locals_slot_cache[i].discard(xsink);
            locals_slot_cache[i] = QoreValue();
        }

        // Record diagnostics if enabled
        CacheClearDiagnostics::instance().recordCleanup(locals_sz, globals_sz, threadlocals_sz, closures_sz, slot_cache_sz);
    };

    struct LocalInstantiationCleanup {
        std::unordered_set<const LocalVar*>& locals;
        ExceptionSink* xsink;
        const std::unordered_set<const LocalVar*>* pre_instantiated;
        ~LocalInstantiationCleanup() {
            cleanupInstantiatedLocals(locals, xsink, pre_instantiated);
        }
    } local_cleanup{instantiated_locals, xsink, pre_instantiated};

    // RAII guard: discard all slot cache entries on any function exit path.
    // The slot cache holds refSelf() references to local variable values for O(1) access.
    // std::vector<QoreValue> destructor does NOT call discard() on elements, so without
    // this guard, every IR function execution leaks all cached references.
    // Constructed AFTER local_cleanup so it's destroyed BEFORE it — slot cache refs are
    // released while the underlying locals are still valid on the thread-local stack.
    struct SlotCacheCleanup {
        std::vector<QoreValue>& cache;
        ExceptionSink* xsink;
        ~SlotCacheCleanup() {
            for (auto& v : cache) {
                v.discard(xsink);
            }
        }
    } slot_cache_cleanup{locals_slot_cache, xsink};

    // RAII guard: delete active iterators on any function exit path.
    // IteratorNext deletes iterators on the normal done path, but return/exception
    // can exit without reaching done, leaking the iterator stored as raw int64_t.
    struct IteratorCleanupGuard {
        std::unordered_set<FunctionalOperatorInterface*>& iters;
        ~IteratorCleanupGuard() {
            for (auto* iter : iters) {
                delete iter;
            }
        }
    } iter_cleanup{active_iterators};

    // Pointer to the function's own locals set (pre_instantiated_locals from QoreIRFunction).
    // Used by ensureLocalInstantiated() to distinguish function-own locals from outer-scope
    // variables (e.g. top-level locals accessed from a sub, enclosing-function params
    // accessed from on_exit handler bodies).  Outer-scope variables are already on the
    // thread-local stack and must NOT be re-instantiated.
    // Always point to pre_instantiated_locals — even when empty, it correctly
    // indicates that ALL LoadLocal targets are outer-scope variables (e.g. handler
    // bodies with no own locals that reference enclosing-function params).
    const std::unordered_set<const void*>* function_own_locals = &func.pre_instantiated_locals;

    // RAII guard: fire remaining on_block_exit handlers on ANY function exit path.
    // Normal exits fire handlers via ScopeExit instructions (which clear scope_stack
    // and on_block_exit_handlers). Error exits (exception from Call/ExprOp/etc.)
    // skip ScopeExit and return false directly, leaving handlers unfired.
    // This guard catches those cases by calling fireScopeExits(0) before return,
    // ensuring on_exit/on_error/on_success handlers always fire.
    // Constructed AFTER local_cleanup so it's destroyed BEFORE it — handlers fire
    // while runtime locals are still valid on the thread-local variable stack.
    struct ScopeExitGuard {
        decltype(fireScopeExits)& fire;
        ~ScopeExitGuard() { fire(0); }
    } scope_exit_guard{fireScopeExits};

    // Debug hook support
    ThreadLocalProgramData* tlpd = get_thread_local_program_data();
    // can_debug: invariant within this function — tlpd, statements, pgm won't change.
    // Only runtimeCheck() can change (debugger attaches mid-execution).
    bool can_debug = tlpd && statements && pgm;
    bool debug_active = can_debug && tlpd->runtimeCheck();
    int last_debug_line = -1;  // track line changes for dbgStep

    // Call dbgFunctionEnter if debug is active, then fire block-entry event.
    // AST mode fires dbgStep(block, nullptr) at block entry (StatementBlock.cpp:239)
    // to signal the debugger that a new block scope is being entered.
    if (debug_active) {
        tlpd->dbgFunctionEnter(statements, xsink);
        if (*xsink) {
            return false;  // local_cleanup RAII handles cleanup
        }
        int dbg_rc = tlpd->dbgStep(statements, nullptr, xsink);
        if (dbg_rc || *xsink) {
            tlpd->dbgFunctionExit(statements, return_value, xsink);
            return false;  // local_cleanup RAII handles cleanup
        }
    }


    if (func.blocks.empty()) {
        if (xsink) {
            xsink->raiseException("IR-EXEC-ERROR", "function has no basic blocks");
        }
        return false;
    }
    QoreIRBasicBlock* block = func.blocks.front().get();
    QoreIRBasicBlock* prev_block = nullptr;
    size_t ip = 0;

    // NOTE: QoreIRStackLocation was removed - it was adding extra frames to exception
    // callstacks with the wrong line numbers.  Instead, runtime_loc is updated
    // per-instruction in the main loop below for use by AST evaluation code.

    // OSR: loop iteration counter for loop-aware JIT promotion
    uint32_t loop_iterations = 0;
    const uint32_t osr_threshold = static_cast<uint32_t>(QoreJIT::getJITThreshold());

    // Track values[] slots holding strong refs from weak reference evaluation.
    // When LoadSelfMember (or LoadStaticVar) evaluates a needsEval() value
    // (e.g., WeakReferenceNode), the resulting strong ref is stored in values[].
    // In AST mode, such refs are temporary within each expression evaluation.
    // In IR mode, they persist until function return — for long-running functions
    // (e.g., PipelineQueue::run() blocking in cond.wait()), this prevents object
    // destruction and causes shutdown hangs.
    // Fix: discard these "ephemeral" values at statement boundaries (line changes).
    // Uses a vector (not set) — duplicates are harmless (discard on NOTHING is a no-op)
    // and 0-2 entries per statement makes linear scan faster than hash table overhead.
    std::vector<uint32_t> ephemeral_weak_ref_slots;
    int last_ephemeral_line = -1;

    while (block) {
        if (ip >= block->instructions.size()) {
            if (xsink) {
                xsink->raiseException("IR-EXEC-ERROR", "fell off end of basic block");
            }
            cleanupValues(values, cleanup, xsink, true, cleanup_log);
            cleanupStoredValues(locals, xsink);
            cleanupStoredValues(globals, xsink);
            cleanupStoredValues(threadlocals, xsink);
            cleanupStoredValues(closures, xsink);
            return false;
        }
        while (ip < block->instructions.size() && block->instructions[ip]->opcode == QoreIROpcode::Phi) {
            auto* phi = dynamic_cast<QoreIRPhiInstruction*>(block->instructions[ip].get());
            if (!phi) {
                if (xsink) {
                    xsink->raiseException("IR-EXEC-ERROR", "phi instruction cast failed");
                }
                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                cleanupStoredValues(locals, xsink);
                cleanupStoredValues(globals, xsink);
                cleanupStoredValues(threadlocals, xsink);
                cleanupStoredValues(closures, xsink);
                return false;
            }
            if (!prev_block) {
                if (xsink) {
                    xsink->raiseException("IR-EXEC-ERROR", "phi has no predecessor");
                }
                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                cleanupStoredValues(locals, xsink);
                cleanupStoredValues(globals, xsink);
                cleanupStoredValues(threadlocals, xsink);
                cleanupStoredValues(closures, xsink);
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
                cleanupStoredValues(locals, xsink);
                cleanupStoredValues(globals, xsink);
                cleanupStoredValues(threadlocals, xsink);
                cleanupStoredValues(closures, xsink);
                return false;
            }
            QoreValue val = getIRValue(values, incoming_value);
            QoreValue stored = val.hasNode() ? val.refSelf() : val;
            setValueSlot(values, phi->result.id, stored, xsink);
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
            cleanupStoredValues(locals, xsink);
            cleanupStoredValues(globals, xsink);
            cleanupStoredValues(threadlocals, xsink);
            cleanupStoredValues(closures, xsink);
            return false;
        }
        QoreIRInstruction* inst = block->instructions[ip].get();

        // Update runtime_loc from the current instruction's source location.
        // This is needed because AST evaluation code (CodeEvaluationHelper, exception handling)
        // reads runtime_loc to determine the current source position. Without this, builtin
        // function calls and exceptions would report <builtin>:-1 during IR execution.
        // This is a single pointer write per instruction — negligible overhead.
        if (inst->loc) {
            update_runtime_statement_location(nullptr, inst->loc);
        }

        // Clean up ephemeral weak-ref values at statement boundaries.
        // When the source line changes, discard strong refs from needsEval() member
        // loads (e.g., WeakReferenceNode evaluations) to match AST temporary lifetime.
        // Instructions with null loc (synthetic/internal) are transparent — the cleanup
        // is deferred until the next instruction with a real source location.
        if (inst->loc && inst->loc->start_line != last_ephemeral_line) {
            if (!ephemeral_weak_ref_slots.empty()) {
                for (uint32_t slot : ephemeral_weak_ref_slots) {
                    if (slot < values.size()) {
                        values[slot].discard(xsink);
                        values[slot] = QoreValue();
                    }
                }
                ephemeral_weak_ref_slots.clear();
            }
            last_ephemeral_line = inst->loc->start_line;
        }

        // Debug: fire dbgStep on source line changes.
        // Re-check debug state: matches AST mode where runtimeCheck() gates every dbgStep.
        // Handles both mid-execution attachment (addProgram) and detachment (removeProgram).
        if (can_debug) {
            bool runtime_check = tlpd->runtimeCheck();
            if (!debug_active && runtime_check) {
                // Debugger attached mid-execution: start generating step events
                // from the current position — no retroactive dbgFunctionEnter since the function
                // was already entered before the debugger attached (matching AST mode behavior).
                debug_active = true;
                last_debug_line = -1;
            } else if (debug_active && !runtime_check) {
                // Debugger detached mid-execution: stop generating debug events
                debug_active = false;
            }
        }
        if (debug_active) {
            if (inst->loc && inst->loc->start_line != last_debug_line) {
                last_debug_line = inst->loc->start_line;
                // start_line + offset: statements are indexed by this combined value
                // (see QoreProgram.cpp addStatementToIndexIntern); start_line is the
                // absolute line in the file, offset adjusts for embedded sources.
                AbstractStatement* dbg_stmt = qore_program_private::get(*pgm)->getStatementFromIndex(
                    inst->loc->getFile(), inst->loc->start_line + inst->loc->offset);
                if (dbg_stmt) {
                    int dbg_rc = tlpd->dbgStep(statements, dbg_stmt, xsink);
                    if (dbg_rc || *xsink) {
                        if (dbg_rc == RC_RETURN || *xsink) {
                            if (debug_active) {
                                tlpd->dbgFunctionExit(statements, return_value, xsink);
                            }
                            executeOnBlockExitHandlers(on_block_exit_handlers, xsink);
                            cleanupValues(values, cleanup, xsink, true, cleanup_log);
                            cleanupStoredValues(locals, xsink);
                            cleanupStoredValues(globals, xsink);
                            cleanupStoredValues(threadlocals, xsink);
                            cleanupStoredValues(closures, xsink);
                            return dbg_rc == RC_RETURN;
                        }
                        // RC_BREAK/RC_CONTINUE: not easily translatable to IR control flow
                        // Fall through for now
                    }
                }
            }
        }

        switch (inst->opcode) {
            case QoreIROpcode::ConstInt: {
                auto* cinst = static_cast<QoreIRConstInstruction*>(inst);
                setValueSlotDirect(values, cinst->result.id, QoreValue(cinst->constant.int_value));
                ++ip;
                break;
            }
            case QoreIROpcode::ConstFloat: {
                auto* cinst = static_cast<QoreIRConstInstruction*>(inst);
                setValueSlotDirect(values, cinst->result.id, QoreValue(cinst->constant.float_value));
                ++ip;
                break;
            }
            case QoreIROpcode::ConstBool: {
                auto* cinst = static_cast<QoreIRConstInstruction*>(inst);
                setValueSlotDirect(values, cinst->result.id, QoreValue(cinst->constant.bool_value));
                ++ip;
                break;
            }
            case QoreIROpcode::ConstNothing: {
                auto* cinst = static_cast<QoreIRConstInstruction*>(inst);
                setValueSlotDirect(values, cinst->result.id, QoreValue());
                ++ip;
                break;
            }
            case QoreIROpcode::ConstNull: {
                auto* cinst = static_cast<QoreIRConstInstruction*>(inst);
                setValueSlotDirect(values, cinst->result.id, QoreValue::makeNull());
                ++ip;
                break;
            }
            case QoreIROpcode::ConstString: {
                auto* cinst = static_cast<QoreIRConstInstruction*>(inst);
                QoreStringNode* str = new QoreStringNode(cinst->constant.string_value);
                setValueSlot(values, cinst->result.id, QoreValue(str), xsink);
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
                setValueSlot(values, cinst->result.id, QoreValue(dt), xsink);
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
                        cleanupStoredValues(locals, xsink);
                        cleanupStoredValues(globals, xsink);
                        cleanupStoredValues(threadlocals, xsink);
                        cleanupStoredValues(closures, xsink);
                        return false;
                    }
                }
                if (!vtype || vtype == anyTypeInfo || !vcommon) {
                    vtype = autoTypeInfo;
                }
                qore_list_private::get(*list)->complexTypeInfo = qore_get_complex_list_type(vtype);
                QoreListNode* raw_list = list.release();
                setValueSlot(values, inst->result.id, QoreValue(raw_list), xsink);
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
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
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
                        cleanupStoredValues(locals, xsink);
                        cleanupStoredValues(globals, xsink);
                        cleanupStoredValues(threadlocals, xsink);
                        cleanupStoredValues(closures, xsink);
                        return false;
                    }
                }
                if (!vtype || vtype == anyTypeInfo) {
                    vtype = autoTypeInfo;
                }
                qore_hash_private::get(*hash)->complexTypeInfo = qore_get_complex_hash_type(vtype);
                setValueSlot(values, inst->result.id, QoreValue(hash.release()), xsink);
                cleanup.push_back(inst->result.id);
                ++ip;
                break;
            }
            case QoreIROpcode::CreateEmptyList: {
                QoreListNode* list = new QoreListNode(autoTypeInfo);
                setValueSlot(values, inst->result.id, QoreValue(list), xsink);
                cleanup.push_back(inst->result.id);
                ++ip;
                break;
            }
            case QoreIROpcode::ListAppend: {
                QoreValue list_val = getIRValue(values, inst->operands[0]);
                QoreValue value = getIRValue(values, inst->operands[1]);
                QoreListNode* list = list_val.get<QoreListNode>();
                if (list) {
                    list->push(value.hasNode() ? value.refSelf() : value, xsink);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::ListPush: {
                QoreValue list_val = getIRValue(values, inst->operands[0]);
                QoreValue push_val = getIRValue(values, inst->operands[1]);
                QoreValue result;
                if (list_val.getType() == NT_LIST) {
                    QoreListNode* l = list_val.get<QoreListNode>();
                    l->push(push_val.refSelf(), xsink);
                    // refSelf: result shares same list pointer as operand; both are in
                    // cleanup, so both need their own reference
                    result = list_val.refSelf();
                } else if (list_val.isNothing()) {
                    QoreListNode* l = new QoreListNode(autoTypeInfo);
                    l->push(push_val.refSelf(), xsink);
                    result = QoreValue(l);
                } else {
                    if (xsink) {
                        xsink->raiseException("PUSH-ERROR",
                            "the lvalue argument to push is type \"%s\"; expecting \"list\"",
                            list_val.getTypeName());
                    }
                }
                if (xsink && *xsink) {
                    result.discard(xsink);
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                setValueSlot(values, inst->result.id, result, xsink);
                if (result.hasNode()) {
                    cleanup.push_back(inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::CreateSizedList: {
                QoreValue cap_val = getIRValue(values, inst->operands[0]);
                int64_t capacity = cap_val.getAsBigInt();
                QoreListNode* list = new QoreListNode(autoTypeInfo);
                if (capacity > 0) {
                    qore_list_private::get(*list)->reserve(static_cast<size_t>(capacity));
                }
                setValueSlot(values, inst->result.id, QoreValue(list), xsink);
                cleanup.push_back(inst->result.id);
                ++ip;
                break;
            }
            case QoreIROpcode::ListSize: {
                QoreValue list_val = getIRValue(values, inst->operands[0]);
                int64_t size = 0;
                if (list_val.getType() == NT_LIST) {
                    size = static_cast<int64_t>(list_val.get<const QoreListNode>()->size());
                }
                setValueSlot(values, inst->result.id, QoreValue(size), xsink);
                ++ip;
                break;
            }
            case QoreIROpcode::ListGetInt: {
                QoreValue list_val = getIRValue(values, inst->operands[0]);
                QoreValue idx_val = getIRValue(values, inst->operands[1]);
                int64_t index = idx_val.getAsBigInt();
                int64_t result = 0;
                if (list_val.getType() == NT_LIST) {
                    const QoreListNode* l = list_val.get<const QoreListNode>();
                    if (index >= 0 && static_cast<size_t>(index) < l->size()) {
                        result = l->retrieveEntry(index).getAsBigInt();
                    }
                }
                setValueSlot(values, inst->result.id, QoreValue(result), xsink);
                ++ip;
                break;
            }
            case QoreIROpcode::ListGetFloat: {
                QoreValue list_val = getIRValue(values, inst->operands[0]);
                QoreValue idx_val = getIRValue(values, inst->operands[1]);
                int64_t index = idx_val.getAsBigInt();
                double result = 0.0;
                if (list_val.getType() == NT_LIST) {
                    const QoreListNode* l = list_val.get<const QoreListNode>();
                    if (index >= 0 && static_cast<size_t>(index) < l->size()) {
                        result = l->retrieveEntry(index).getAsFloat();
                    }
                }
                setValueSlot(values, inst->result.id, QoreValue(result), xsink);
                ++ip;
                break;
            }
            case QoreIROpcode::ListGetValue: {
                QoreValue list_val = getIRValue(values, inst->operands[0]);
                QoreValue idx_val = getIRValue(values, inst->operands[1]);
                int64_t index = idx_val.getAsBigInt();
                QoreValue result;
                if (list_val.getType() == NT_LIST) {
                    const QoreListNode* l = list_val.get<const QoreListNode>();
                    if (index >= 0 && static_cast<size_t>(index) < l->size()) {
                        result = l->getReferencedEntry(static_cast<size_t>(index));
                    }
                }
                setValueSlot(values, inst->result.id, result, xsink);
                if (result.hasNode()) {
                    cleanup.push_back(inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::ListSetInt: {
                QoreValue list_val = getIRValue(values, inst->operands[0]);
                QoreValue idx_val = getIRValue(values, inst->operands[1]);
                QoreValue val = getIRValue(values, inst->operands[2]);
                if (list_val.getType() == NT_LIST) {
                    QoreListNode* l = list_val.get<QoreListNode>();
                    int64_t index = idx_val.getAsBigInt();
                    qore_list_private* priv = qore_list_private::get(*l);
                    priv->getEntryReference(static_cast<size_t>(index)) = QoreValue(val.getAsBigInt());
                    if (static_cast<size_t>(index) >= priv->length) {
                        priv->length = static_cast<size_t>(index) + 1;
                    }
                }
                ++ip;
                break;
            }
            case QoreIROpcode::ListSetFloat: {
                QoreValue list_val = getIRValue(values, inst->operands[0]);
                QoreValue idx_val = getIRValue(values, inst->operands[1]);
                QoreValue val = getIRValue(values, inst->operands[2]);
                if (list_val.getType() == NT_LIST) {
                    QoreListNode* l = list_val.get<QoreListNode>();
                    int64_t index = idx_val.getAsBigInt();
                    qore_list_private* priv = qore_list_private::get(*l);
                    priv->getEntryReference(static_cast<size_t>(index)) = QoreValue(val.getAsFloat());
                    if (static_cast<size_t>(index) >= priv->length) {
                        priv->length = static_cast<size_t>(index) + 1;
                    }
                }
                ++ip;
                break;
            }
            case QoreIROpcode::ListSetValue: {
                QoreValue list_val = getIRValue(values, inst->operands[0]);
                QoreValue idx_val = getIRValue(values, inst->operands[1]);
                QoreValue val = getIRValue(values, inst->operands[2]);
                if (list_val.getType() == NT_LIST) {
                    QoreListNode* l = list_val.get<QoreListNode>();
                    int64_t index = idx_val.getAsBigInt();
                    QoreValue stored = val.hasNode() ? val.refSelf() : val;
                    qore_list_private* priv = qore_list_private::get(*l);
                    priv->getEntryReference(static_cast<size_t>(index)) = stored;
                    if (static_cast<size_t>(index) >= priv->length) {
                        priv->length = static_cast<size_t>(index) + 1;
                    }
                    if (needs_scan(stored)) {
                        priv->incScanCount(1);
                    }
                }
                ++ip;
                break;
            }
            case QoreIROpcode::GetObjectClass: {
                QoreValue obj_val = getIRValue(values, inst->operands[0]);
                int64_t class_ptr = 0;
                if (obj_val.getType() == NT_OBJECT) {
                    class_ptr = reinterpret_cast<int64_t>(obj_val.get<const QoreObject>()->getClass());
                }
                setValueSlot(values, inst->result.id, QoreValue(class_ptr), xsink);
                ++ip;
                break;
            }
            case QoreIROpcode::CallClosureDirect: {
                // operands[0] = closure/callref value, operands[1..n] = args
                QoreValue ref_val = getIRValue(values, inst->operands[0]);
                if (!ref_val.hasNode()) {
                    xsink->raiseException("CALL-REFERENCE-ERROR",
                        "cannot call a NOTHING value as a closure/call reference");
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }

                ResolvedCallReferenceNode* callref = dynamic_cast<ResolvedCallReferenceNode*>(
                    const_cast<AbstractQoreNode*>(ref_val.getInternalNode()));
                if (!callref) {
                    xsink->raiseException("CALL-REFERENCE-ERROR",
                        "value is not a call reference or closure");
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }

                // Build args list from operands[1..]
                int nargs = static_cast<int>(inst->operands.size()) - 1;
                QoreListNode* arg_list =
                    nargs > 0 ? new QoreListNode(autoTypeInfo) : nullptr;
                if (arg_list) {
                    for (int i = 0; i < nargs; ++i) {
                        QoreValue val = getIRValue(values, inst->operands[i + 1]);
                        if (val.hasNode()) {
                            val.refSelf();
                        }
                        arg_list->push(val, xsink);
                    }
                }

                // execValue borrows args (const QoreListNode*) — it does NOT take
                // ownership.  We must deref the arg list after the call to avoid
                // leaking the refSelf'd entries.
                QoreValue result = callref->execValue(arg_list, xsink);
                // Deref the arg list now — entries were refSelf'd for the call
                if (arg_list) {
                    arg_list->deref(xsink);
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                // Closure/callref execution can run arbitrary code that may modify
                // globals, thread-locals, or other cached state, so clear all caches
                cleanupLocalCaches();
                setValueSlot(values, inst->result.id, result, xsink);
                if (result.hasNode()) {
                    cleanup.push_back(inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::StringConcat: {
                // Multi-string concatenation - a + b + c + d in single pass
                if (inst->operands.empty()) {
                    setValueSlot(values, inst->result.id, QoreValue(new QoreStringNode()), xsink);
                    cleanup.push_back(inst->result.id);
                    ++ip;
                    break;
                }
                // Get first string to determine encoding
                QoreValue first = getIRValue(values, inst->operands[0]);
                const QoreEncoding* enc = QCS_DEFAULT;
                if (first.getType() == NT_STRING) {
                    enc = first.get<const QoreStringNode>()->getEncoding();
                }
                QoreStringNode* result = new QoreStringNode(enc);
                // Concatenate all operands
                for (const auto& operand : inst->operands) {
                    QoreValue v = getIRValue(values, operand);
                    if (v.getType() == NT_STRING) {
                        const QoreStringNode* s = v.get<const QoreStringNode>();
                        result->concat(s, xsink);
                        if (xsink && *xsink) {
                            result->deref();
                            cleanupValues(values, cleanup, xsink, true, cleanup_log);
                            cleanupStoredValues(locals, xsink);
                            cleanupStoredValues(globals, xsink);
                            cleanupStoredValues(threadlocals, xsink);
                            cleanupStoredValues(closures, xsink);
                            return false;
                        }
                    }
                    // NOTHING values are skipped (treated as empty string)
                }
                setValueSlot(values, inst->result.id, QoreValue(result), xsink);
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
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
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
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
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
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                // For lvalue-modifying invoke opcodes, invalidate the slot cache BEFORE
                // the call.  evalInvoke handles these natively via evalLValueBinary/
                // evalLValueUnary/evalLValueTernary, which use LValueHelper internally.
                // The slot cache holds refSelf() references that inflate refcounts,
                // causing ensureUnique() to trigger COW unnecessarily — the modification
                // would be applied to the copy while the cache retains the stale original.
                // See design/lvalue-loads-in-ir.md.
                switch (inv->invoke_opcode) {
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
                    case QoreIROpcode::UnshiftLValue:
                    case QoreIROpcode::ShiftLValue:
                    case QoreIROpcode::SpliceLValue:
                    case QoreIROpcode::PreIncLValue:
                    case QoreIROpcode::PreDecLValue:
                    case QoreIROpcode::PostIncLValue:
                    case QoreIROpcode::PostDecLValue:
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
                    case QoreIROpcode::StoreLValue: {
                        cleanupStoredValues(locals, xsink);
                        cleanupStoredValues(globals, xsink);
                        cleanupStoredValues(threadlocals, xsink);
                        cleanupStoredValues(closures, xsink);
                        for (size_t i = 0; i < locals_slot_cache.size(); ++i) {
                            locals_slot_cache[i].discard(xsink);
                            locals_slot_cache[i] = QoreValue();
                        }
                        // Also clean values[] entries for the lvalue target variable
                        // to ensure ensureUnique() sees the natural refcount.
                        // inv->expr is the full assignment expression (e.g.,
                        // QoreAssignmentOperatorNode) — extractLValueBaseVarRef
                        // recurses through operator nodes to find the base VarRefNode.
                        const VarRefNode* inv_base_var = extractLValueBaseVarRef(inv->expr);
                        if (inv_base_var && inv_base_var->ref.id) {
                            auto inv_llslot_it = local_load_slots.find(inv_base_var->ref.id);
                            if (inv_llslot_it != local_load_slots.end()) {
                                for (uint32_t slot_id : inv_llslot_it->second) {
                                    if (slot_id < values.size()) {
                                        values[slot_id].discard(xsink);
                                        values[slot_id] = QoreValue();
                                    }
                                }
                            }
                        }
                        break;
                    }
                    default:
                        break;
                }
                QoreValue res = evalInvoke(inv, values, xsink);
                // Invalidate all variable caches after Invoke - the AST call may have modified
                // globals, thread-locals, or closure variables, and the slot cache must be cleared
                cleanupLocalCaches();
                if (xsink && *xsink) {
                    if (debug_active) {
                        tlpd->dbgException(nullptr, xsink);
                        // dbgException may dismiss the exception
                        if (!*xsink) {
                            // Exception was dismissed by debugger — continue normal flow.
                            // NOTE: the invoke's result slot (values[inv->result]) contains
                            // NOTHING since the call raised an exception before producing a
                            // result. Code on the normal path may read this slot expecting a
                            // valid value. This is inherent to debugger exception dismissal;
                            // the debugger is responsible for understanding this risk.
                            prev_block = block;
                            block = inv->normal_target;
                            ip = 0;
                            break;
                        }
                    }
                    if (!inv->exception_target) {
                        // No exception target (no try/catch): propagate exception to caller.
                        // Fire debug exit, on_block_exit handlers, and full cleanup.
                        if (debug_active) {
                            tlpd->dbgFunctionExit(statements, return_value, xsink);
                        }
                        executeOnBlockExitHandlers(on_block_exit_handlers, xsink);
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupStoredValues(locals, xsink);
                        cleanupStoredValues(globals, xsink);
                        cleanupStoredValues(threadlocals, xsink);
                        cleanupStoredValues(closures, xsink);
                        return false;
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    prev_block = block;
                    block = inv->exception_target;
                    ip = 0;
                    break;
                }
                setValueSlot(values, inv->result.id, res, xsink);
                if (res.hasNode()) {
                    cleanup.push_back(inv->result.id);
                }
                prev_block = block;
                block = inv->normal_target;
                ip = 0;
                break;
            }
            case QoreIROpcode::LandingPad: {
                // Execute on_error/on_exit handlers for scopes entered within the try body
                // that were not exited due to an invoke exception jumping directly here.
                // Uses the same handler execution pattern as fireScopeExits() and ScopeExit:
                // CatchExceptionHelper for rethrow support, rethrown check + xsink->clear(),
                // executeHandlerBody() for IR-compiled handlers, and error flag update.
                auto* lp_inst = static_cast<QoreIRLandingPadInstruction*>(inst);
                while (scope_stack.size() > lp_inst->scope_depth) {
                    size_t scope_start = scope_stack.back();
                    scope_stack.pop_back();
                    // Execute on_error/on_exit handlers in reverse order (LIFO)
                    if (on_block_exit_handlers.size() > scope_start) {
                        bool error = xsink && xsink->isException();
                        ExceptionSink obe_xsink;
                        for (size_t i = on_block_exit_handlers.size(); i > scope_start; --i) {
                            obe_type_e type = on_block_exit_handlers[i - 1].type;
                            if (type == OBE_Unconditional || (error && type == OBE_Error)) {
                                if (on_block_exit_handlers[i - 1].code || on_block_exit_handlers[i - 1].handler_ir) {
                                    std::unique_ptr<SingleArgvContextHelper> argv_helper;
                                    std::unique_ptr<CatchExceptionHelper> ex_helper;
                                    if (type == OBE_Error && xsink) {
                                        QoreException* except = xsink->getException();
                                        if (except) {
                                            ex_helper.reset(new CatchExceptionHelper(except));
                                            argv_helper.reset(new SingleArgvContextHelper(
                                                except->makeExceptionObject(), xsink));
                                        }
                                    }
                                    executeHandlerBody(on_block_exit_handlers[i - 1], &obe_xsink);
                                    // Restore td->catchException BEFORE clearing xsink to avoid
                                    // a use-after-free window where td->catchException points to
                                    // the freed exception chain
                                    ex_helper.reset();
                                    argv_helper.reset();
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
                        on_block_exit_handlers.resize(scope_start);
                        // Invalidate caches after handler execution (both AST and compiled
                        // handlers can modify locals via the thread-local variable stack)
                        cleanupStoredValues(locals, xsink);
                    }
                }
                ++ip;
                break;
            }
            case QoreIROpcode::CatchException: {
                if (!xsink || !*xsink) {
                    setValueSlot(values, inst->result.id, QoreValue(), xsink);
                    // Push empty entry for consistent push/pop with CatchCleanup
                    catch_exception_stack.push_back({nullptr, nullptr});
                    ++ip;
                    break;
                }
                QoreException* caught = xsink->catchException();
                QoreException* saved = catch_swap_exception(caught);
                catch_exception_stack.push_back({caught, saved});
                QoreHashNode* info = caught->makeExceptionObject();
                setValueSlot(values, inst->result.id, QoreValue(info), xsink);
                cleanup.push_back(inst->result.id);
                ++ip;
                break;
            }
            case QoreIROpcode::CatchCleanup: {
                if (!catch_exception_stack.empty()) {
                    auto entry = catch_exception_stack.back();
                    catch_exception_stack.pop_back();
                    if (entry.caught) {
                        catch_swap_exception(entry.saved);
                        entry.caught->del(xsink);
                    }
                }
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
                    // Check for thread cancellation or program interrupt at loop headers
                    if (qore_check_cancel(xsink, "IR loop")) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupStoredValues(locals, xsink);
                        cleanupStoredValues(globals, xsink);
                        cleanupStoredValues(threadlocals, xsink);
                        cleanupStoredValues(closures, xsink);
                        return false;
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
                    // Check for thread cancellation or program interrupt at loop headers
                    if (qore_check_cancel(xsink, "IR loop")) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupStoredValues(locals, xsink);
                        cleanupStoredValues(globals, xsink);
                        cleanupStoredValues(threadlocals, xsink);
                        cleanupStoredValues(closures, xsink);
                        return false;
                    }
                }
                break;
            }
            case QoreIROpcode::SwitchInt: {
                auto* sw = static_cast<QoreIRSwitchIntInstruction*>(inst);
                QoreValue switch_val = getIRValue(values, sw->switch_val);
                int64_t val = switch_val.getAsBigInt();
                prev_block = block;
                // Search for matching case
                QoreIRBasicBlock* target = sw->default_target;
                for (const auto& c : sw->cases) {
                    if (c.value == val) {
                        target = c.target;
                        break;
                    }
                }
                block = target;
                ip = 0;
                break;
            }
            case QoreIROpcode::SwitchString: {
                auto* sw = static_cast<QoreIRSwitchStringInstruction*>(inst);
                QoreValue switch_val = getIRValue(values, sw->switch_val);
                prev_block = block;
                // Search for matching case
                QoreIRBasicBlock* target = sw->default_target;
                const QoreStringNode* str = switch_val.get<const QoreStringNode>();
                if (str) {
                    for (const auto& c : sw->cases) {
                        if (str->equal(c.value.c_str())) {
                            target = c.target;
                            break;
                        }
                    }
                }
                block = target;
                ip = 0;
                break;
            }
            case QoreIROpcode::LoadLocal: {
                auto* local_inst = static_cast<QoreIRLocalInstruction*>(inst);
                ensureLocalInstantiated(local_inst->local, instantiated_locals, pre_instantiated,
                        function_own_locals, &locally_uninstantiated);
                QoreValue out;
                // Closure-bound locals must always be read from the runtime stack
                // because closures can modify the value between IR instructions
                if (local_inst->local && local_inst->local->closureUse()) {
                    bool needs_deref = true;
                    out = local_inst->local->eval(needs_deref, xsink);
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupStoredValues(locals, xsink);
                        cleanupStoredValues(globals, xsink);
                        cleanupStoredValues(threadlocals, xsink);
                        cleanupStoredValues(closures, xsink);
                        return false;
                    }
                    // eval() returns a referenced value; use it directly for the value slot.
                    // When auto_ref=false (lvalue operand), the common cleanup below won't
                    // fire, so we must track the owned ref here to avoid a leak.
                    if (!local_inst->auto_ref && needs_deref && out.hasNode()) {
                        cleanup.push_back(local_inst->result.id);
                        local_load_slots[local_inst->local].insert(local_inst->result.id);
                    }
                } else if (local_inst->auto_ref) {
                    // Normal path: caching with refcount inflation (auto_ref=true)
                    // Phase 3 optimization: Try fast path with slot cache first
                    QoreValue cached_val;
                    bool found_in_slot_cache = false;
                    if (local_inst->local) {
                        auto slot_it = func.local_var_slots.find(local_inst->local);
                        if (slot_it != func.local_var_slots.end() && slot_it->second < locals_slot_cache.size()) {
                            cached_val = locals_slot_cache[slot_it->second];
                            if (!cached_val.isNothing() && cached_val.getType() != NT_REFERENCE) {
                                found_in_slot_cache = true;
                                out = cached_val.hasNode() ? cached_val.refSelf() : cached_val;
                            }
                        }
                    }

                    if (!found_in_slot_cache) {
                        // Fallback to traditional map-based cache lookup
                        auto it = locals.find(local_inst->local);
                        if (it != locals.end() && it->second.getType() != NT_REFERENCE) {
                            QoreValue val = it->second;
                            out = val.hasNode() ? val.refSelf() : val;
                        } else if (local_inst->local) {
                            bool needs_deref = true;
                            QoreValue val = local_inst->local->eval(needs_deref, xsink);
                            if (xsink && *xsink) {
                                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                                cleanupStoredValues(locals, xsink);
                                cleanupStoredValues(globals, xsink);
                                cleanupStoredValues(threadlocals, xsink);
                                cleanupStoredValues(closures, xsink);
                                return false;
                            }
                            storeValue(locals, local_inst->local, val, xsink);
                            // Also store in slot cache for fast access on next load
                            auto slot_it = func.local_var_slots.find(local_inst->local);
                            if (slot_it != func.local_var_slots.end() && slot_it->second < locals_slot_cache.size()) {
                                // Discard old slot cache value before overwriting to prevent
                                // refcount leak.  On a cache miss with eval() (e.g. closure
                                // variable), the slot may already hold a stale +1 ref from a
                                // prior load.  Without this discard, overwriting the slot
                                // silently drops that reference, causing a permanent leak.
                                locals_slot_cache[slot_it->second].discard(xsink);
                                locals_slot_cache[slot_it->second] = val.hasNode() ? val.refSelf() : val;
                            }
                            out = val.hasNode() ? val.refSelf() : val;
                            // When eval() returned an owned reference (needs_deref=true,
                            // e.g. ClosureVarValue for VT_IMMEDIATE reference params),
                            // storeValue/slot-cache/out each did refSelf() to create
                            // independent +1 refs.  The original owned ref in `val` is
                            // now surplus — release it to avoid a permanent refcount leak.
                            if (needs_deref && val.hasNode()) {
                                val.getInternalNode()->deref(xsink);
                            }
                        }
                    }
                } else {
                    // Lvalue path: load without refcount inflation (auto_ref=false)
                    // Do NOT cache to avoid interfering with COW logic
                    if (local_inst->local) {
                        bool needs_deref = true;
                        out = local_inst->local->eval(needs_deref, xsink);
                        if (xsink && *xsink) {
                            cleanupValues(values, cleanup, xsink, true, cleanup_log);
                            cleanupStoredValues(locals, xsink);
                            cleanupStoredValues(globals, xsink);
                            cleanupStoredValues(threadlocals, xsink);
                            cleanupStoredValues(closures, xsink);
                            return false;
                        }
                        // ClosureVarValue::eval() always returns an owned ref
                        // (needs_deref=true) for thread safety, even on the
                        // auto_ref=false lvalue path.  We must track it for
                        // cleanup so the owned ref is released on function exit.
                        if (needs_deref && out.hasNode()) {
                            cleanup.push_back(local_inst->result.id);
                            local_load_slots[local_inst->local].insert(local_inst->result.id);
                        }
                    }
                }
                setValueSlot(values, local_inst->result.id, out, xsink);
                if (out.hasNode() && local_inst->auto_ref) {
                    // Only owned references need cleanup — auto_ref=false borrowed
                    // views are handled above when needs_deref is true
                    cleanup.push_back(local_inst->result.id);
                }
                // Track this LoadLocal result slot for cleanup in UninstantiateLocal.
                // Without this, the last loop iteration's LoadLocal holds an extra
                // reference to block-scoped objects, deferring their destructor to
                // function exit instead of firing at block scope exit.
                if (local_inst->local && local_inst->auto_ref) {
                    local_load_slots[local_inst->local].insert(local_inst->result.id);
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
                setValueSlot(values, inst->result.id, out, xsink);
                if (out.hasNode()) {
                    cleanup.push_back(inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::LoadClosure: {
                auto* local_inst = static_cast<QoreIRLocalInstruction*>(inst);
                QoreValue out;
                if (!inst->operands.empty() && closure) {
                    QoreValue idx_val = getIRValue(values, inst->operands[0]);
                    int64 idx = idx_val.getAsBigInt();
                    QoreValue val;
                    if (idx >= 0 && static_cast<size_t>(idx) < closure->size()) {
                        val = (*closure)[static_cast<size_t>(idx)];
                    }
                    // Closure arg: val is shared reference, needs refSelf for value slot
                    out = val.hasNode() ? val.refSelf() : val;
                } else {
                    auto it = closures.find(local_inst->local);
                    if (it != closures.end()) {
                        // Cache hit: val is shared with cache, needs refSelf for value slot
                        QoreValue val = it->second;
                        out = val.hasNode() ? val.refSelf() : val;
                    } else if (local_inst->local) {
                        // Read closure variable: prefer cvstack (topmost = current
                        // function's own variable) over runtime closure env (which
                        // may point to the calling closure's variable in recursive
                        // scenarios).
                        ClosureVarValue* cv = thread_find_closure_var(local_inst->local->getName());
                        if (!cv) {
                            cv = thread_get_runtime_closure_var(local_inst->local);
                        }
                        if (cv) {
                            QoreValue val = cv->eval(xsink);
                            if (xsink && *xsink) {
                                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                                cleanupStoredValues(locals, xsink);
                                cleanupStoredValues(globals, xsink);
                                cleanupStoredValues(threadlocals, xsink);
                                cleanupStoredValues(closures, xsink);
                                return false;
                            }
                            // cv->eval() returns a referenced value (+1); store a separate
                            // reference in the cache and use eval's reference for the value
                            // slot.  No additional refSelf for out — eval's +1 transfers to it.
                            storeValue(closures, local_inst->local, val, nullptr);
                            out = val;
                        }
                    }
                }
                setValueSlot(values, local_inst->result.id, out, xsink);
                if (out.hasNode() && local_inst->auto_ref) {
                    cleanup.push_back(local_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::HashKeyAccess: {
                auto* hka_inst = static_cast<QoreIRHashKeyAccessInstruction*>(inst);
                QoreValue base = getIRValue(values, hka_inst->operands[0]);
                QoreValue out;

                // Handle weak references by unwrapping them
                if (base.getType() == NT_WEAKREF) {
                    QoreObject* o = base.get<const WeakReferenceNode>()->get();
                    if (o && o->isValid()) {
                        out = o->evalMember(hka_inst->key_name.c_str(), xsink);
                    }
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupStoredValues(locals, xsink);
                        cleanupStoredValues(globals, xsink);
                        cleanupStoredValues(threadlocals, xsink);
                        cleanupStoredValues(closures, xsink);
                        return false;
                    }
                } else if (base.getType() == NT_HASH) {
                    const QoreHashNode* h = base.get<const QoreHashNode>();
                    out = h->getKeyValue(hka_inst->key_name.c_str(), xsink);
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupStoredValues(locals, xsink);
                        cleanupStoredValues(globals, xsink);
                        cleanupStoredValues(threadlocals, xsink);
                        cleanupStoredValues(closures, xsink);
                        return false;
                    }
                    out.refSelf();
                } else if (base.getType() == NT_OBJECT) {
                    QoreObject* o = const_cast<QoreObject*>(base.get<const QoreObject>());
                    out = o->evalMember(hka_inst->key_name.c_str(), xsink);
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupStoredValues(locals, xsink);
                        cleanupStoredValues(globals, xsink);
                        cleanupStoredValues(threadlocals, xsink);
                        cleanupStoredValues(closures, xsink);
                        return false;
                    }
                }
                // else: NOTHING for non-hash/non-object values
                setValueSlot(values, hka_inst->result.id, out, xsink);
                if (out.hasNode()) {
                    cleanup.push_back(hka_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::HashKeyAccessInt: {
                auto* hka_inst = static_cast<QoreIRHashKeyAccessInstruction*>(inst);
                QoreValue base = getIRValue(values, hka_inst->operands[0]);
                int64 out = 0;
                if (base.getType() == NT_HASH) {
                    const QoreHashNode* h = base.get<const QoreHashNode>();
                    out = h->getKeyValue(hka_inst->key_name.c_str()).getAsBigInt();
                }
                setValueSlot(values, hka_inst->result.id, out, xsink);
                ++ip;
                break;
            }
            case QoreIROpcode::HashKeyStore: {
                auto* hks_inst = static_cast<QoreIRHashKeyStoreInstruction*>(inst);
                QoreValue hash_val = getIRValue(values, hks_inst->operands[0]);
                QoreValue val      = getIRValue(values, hks_inst->operands[1]);
                if (hash_val.getType() == NT_HASH) {
                    QoreHashNode* h = hash_val.get<QoreHashNode>();
                    // The hash was loaded with auto_ref=false for lvalue operations,
                    // so is_unique() accurately reflects whether COW is needed
                    if (!h->is_unique()) {
                        // COW: create unique copy and update the local variable
                        QoreHashNode* new_h = h->copy();
                        LocalVar* lv = const_cast<LocalVar*>(
                            reinterpret_cast<const LocalVar*>(hks_inst->container->ref.id));
                        // Invalidate caches (matches StoreLocal pattern)
                        auto cache_it = locals.find(lv);
                        if (cache_it != locals.end()) {
                            cache_it->second.discard(xsink);
                            locals.erase(cache_it);
                        }
                        auto slot_it = func.local_var_slots.find(lv);
                        if (slot_it != func.local_var_slots.end()
                                && slot_it->second < locals_slot_cache.size()) {
                            locals_slot_cache[slot_it->second].discard(xsink);
                            locals_slot_cache[slot_it->second] = QoreValue();
                        }
                        // Write new_h to thread-local stack (follows references)
                        assignLocalVarValue(lv, QoreValue(new_h), xsink);
                        if (xsink && *xsink) {
                            new_h->deref(xsink);
                            cleanupValues(values, cleanup, xsink, true, cleanup_log);
                            cleanupStoredValues(locals, xsink);
                            cleanupStoredValues(globals, xsink);
                            cleanupStoredValues(threadlocals, xsink);
                            cleanupStoredValues(closures, xsink);
                            return false;
                        }
                        // Release the original copy() reference; assignLocalVarValue()
                        // took its own ref via refSelf() internally.  Without this deref,
                        // every COW copy leaks.  The object is still alive because the
                        // variable stack holds a reference (refcount >= 1).
                        new_h->deref(xsink);
                        h = new_h;
                        // CRITICAL FIX: Update IR values[] array with new COW copy.
                        // Without this, subsequent IR instructions reading operands[0]
                        // will use the stale hash and lose modifications.
                        // The hash was originally loaded with auto_ref=false (for lvalue),
                        // so values[] contains a borrowed reference. We update it to point
                        // to new_h (still borrowed), without taking an extra reference.
                        // The LocalVar owns the reference to new_h.
                        values[hks_inst->operands[0].id] = QoreValue(new_h);
                        if (xsink && *xsink) {
                            cleanupValues(values, cleanup, xsink, true, cleanup_log);
                            cleanupStoredValues(locals, xsink);
                            cleanupStoredValues(globals, xsink);
                            cleanupStoredValues(threadlocals, xsink);
                            cleanupStoredValues(closures, xsink);
                            return false;
                        }
                    }
                    h->setKeyValue(hks_inst->key_name.c_str(), val.refSelf(), xsink);
                } else if (hash_val.getType() == NT_OBJECT) {
                    const_cast<QoreObject*>(hash_val.get<const QoreObject>())->setValue(
                        hks_inst->key_name.c_str(), val.refSelf(), xsink);
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                if (hks_inst->result.isValid()) {
                    setValueSlot(values, hks_inst->result.id, val, xsink);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::ListIndexStore: {
                auto* lis_inst = static_cast<QoreIRListIndexStoreInstruction*>(inst);
                QoreValue list_val = getIRValue(values, lis_inst->operands[0]);
                QoreValue val      = getIRValue(values, lis_inst->operands[1]);
                QoreValue idx_val  = getIRValue(values, lis_inst->operands[2]);
                int64_t index = idx_val.getAsBigInt();
                if (list_val.getType() == NT_LIST) {
                    QoreListNode* l = list_val.get<QoreListNode>();
                    // The list was loaded with auto_ref=false for lvalue operations,
                    // so is_unique() accurately reflects whether COW is needed
                    if (!l->is_unique()) {
                        // COW: create unique copy and update the local variable
                        QoreListNode* new_l = l->copy();
                        LocalVar* lv = const_cast<LocalVar*>(
                            reinterpret_cast<const LocalVar*>(lis_inst->container->ref.id));
                        // Invalidate caches (matches StoreLocal pattern)
                        auto cache_it = locals.find(lv);
                        if (cache_it != locals.end()) {
                            cache_it->second.discard(xsink);
                            locals.erase(cache_it);
                        }
                        auto slot_it = func.local_var_slots.find(lv);
                        if (slot_it != func.local_var_slots.end()
                                && slot_it->second < locals_slot_cache.size()) {
                            locals_slot_cache[slot_it->second].discard(xsink);
                            locals_slot_cache[slot_it->second] = QoreValue();
                        }
                        // Write new_l to thread-local stack (follows references)
                        assignLocalVarValue(lv, QoreValue(new_l), xsink);
                        if (xsink && *xsink) {
                            new_l->deref(xsink);
                            cleanupValues(values, cleanup, xsink, true, cleanup_log);
                            cleanupStoredValues(locals, xsink);
                            cleanupStoredValues(globals, xsink);
                            cleanupStoredValues(threadlocals, xsink);
                            cleanupStoredValues(closures, xsink);
                            return false;
                        }
                        // Release the original copy() reference; assignLocalVarValue()
                        // took its own ref via refSelf() internally.  Without this deref,
                        // every COW copy leaks.  The object is still alive because the
                        // variable stack holds a reference (refcount >= 1).
                        new_l->deref(xsink);
                        l = new_l;
                        // CRITICAL FIX: Update IR values[] array with new COW copy.
                        // Use direct assignment (not setValueSlot) to avoid discarding
                        // the un-ref'd old slot.
                        values[lis_inst->operands[0].id] = QoreValue(new_l->refSelf());
                        cleanup.push_back(lis_inst->operands[0].id);
                        if (xsink && *xsink) {
                            cleanupValues(values, cleanup, xsink, true, cleanup_log);
                            cleanupStoredValues(locals, xsink);
                            cleanupStoredValues(globals, xsink);
                            cleanupStoredValues(threadlocals, xsink);
                            cleanupStoredValues(closures, xsink);
                            return false;
                        }
                    }
                    l->setEntry(index, val.refSelf(), xsink);
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                if (lis_inst->result.isValid()) {
                    setValueSlot(values, lis_inst->result.id, val, xsink);
                }
                ++ip;
                break;
            }
            // Fully specialized hash-key map operations
            case QoreIROpcode::MapHashKeyValue: {
                const auto* mhk = static_cast<const QoreIRMapHashKeyInstruction*>(inst);
                QoreValue list_val = getIRValue(values, mhk->operands[0]);
                QoreValue out;
                if (list_val.getType() == NT_LIST) {
                    const QoreListNode* l = list_val.get<const QoreListNode>();
                    size_t sz = l->size();
                    ReferenceHolder<QoreListNode> result(new QoreListNode(autoTypeInfo), xsink);
                    for (size_t i = 0; i < sz; ++i) {
                        QoreValue elem = l->retrieveEntry(i);
                        if (elem.getType() == NT_HASH) {
                            QoreValue val = elem.get<const QoreHashNode>()->getKeyValue(mhk->key1.c_str());
                            result->push(val.refSelf(), xsink);
                        } else {
                            result->push(QoreValue(), xsink);
                        }
                    }
                    out = result.release();
                }
                setValueSlot(values, mhk->result.id, out, xsink);
                if (out.hasNode()) {
                    cleanup.push_back(mhk->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::MapHashKeyInt: {
                const auto* mhk = static_cast<const QoreIRMapHashKeyInstruction*>(inst);
                QoreValue list_val = getIRValue(values, mhk->operands[0]);
                QoreValue out;
                if (list_val.getType() == NT_LIST) {
                    const QoreListNode* l = list_val.get<const QoreListNode>();
                    size_t sz = l->size();
                    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), xsink);
                    for (size_t i = 0; i < sz; ++i) {
                        QoreValue elem = l->retrieveEntry(i);
                        if (elem.getType() == NT_HASH) {
                            result->push(
                                elem.get<const QoreHashNode>()->getKeyValue(mhk->key1.c_str()).getAsBigInt(), xsink);
                        } else {
                            result->push(0ll, xsink);
                        }
                    }
                    out = result.release();
                }
                setValueSlot(values, mhk->result.id, out, xsink);
                if (out.hasNode()) {
                    cleanup.push_back(mhk->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::MapHashKeyOffsetInt: {
                const auto* mhk = static_cast<const QoreIRMapHashKeyInstruction*>(inst);
                QoreValue list_val = getIRValue(values, mhk->operands[0]);
                QoreValue offset_val = getIRValue(values, mhk->operands[1]);
                int64_t offset = offset_val.getAsBigInt();
                QoreValue out;
                if (list_val.getType() == NT_LIST) {
                    const QoreListNode* l = list_val.get<const QoreListNode>();
                    size_t sz = l->size();
                    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), xsink);
                    for (size_t i = 0; i < sz; ++i) {
                        QoreValue elem = l->retrieveEntry(i);
                        if (elem.getType() == NT_HASH) {
                            result->push(
                                elem.get<const QoreHashNode>()->getKeyValue(mhk->key1.c_str()).getAsBigInt()
                                    + offset,
                                xsink);
                        } else {
                            result->push(offset, xsink);
                        }
                    }
                    out = result.release();
                }
                setValueSlot(values, mhk->result.id, out, xsink);
                if (out.hasNode()) {
                    cleanup.push_back(mhk->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::MapHashKeyScaleInt: {
                const auto* mhk = static_cast<const QoreIRMapHashKeyInstruction*>(inst);
                QoreValue list_val = getIRValue(values, mhk->operands[0]);
                QoreValue scale_val = getIRValue(values, mhk->operands[1]);
                int64_t scale = scale_val.getAsBigInt();
                QoreValue out;
                if (list_val.getType() == NT_LIST) {
                    const QoreListNode* l = list_val.get<const QoreListNode>();
                    size_t sz = l->size();
                    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), xsink);
                    for (size_t i = 0; i < sz; ++i) {
                        QoreValue elem = l->retrieveEntry(i);
                        if (elem.getType() == NT_HASH) {
                            result->push(
                                elem.get<const QoreHashNode>()->getKeyValue(mhk->key1.c_str()).getAsBigInt()
                                    * scale,
                                xsink);
                        } else {
                            result->push(0ll, xsink);
                        }
                    }
                    out = result.release();
                }
                setValueSlot(values, mhk->result.id, out, xsink);
                if (out.hasNode()) {
                    cleanup.push_back(mhk->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::HashMapTwoKeys: {
                const auto* mhk = static_cast<const QoreIRMapHashKeyInstruction*>(inst);
                QoreValue list_val = getIRValue(values, mhk->operands[0]);
                QoreValue out;
                if (list_val.getType() == NT_LIST) {
                    const QoreListNode* l = list_val.get<const QoreListNode>();
                    size_t sz = l->size();
                    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoHashTypeInfo), nullptr);
                    for (size_t i = 0; i < sz; ++i) {
                        QoreValue elem = l->retrieveEntry(i);
                        if (elem.getType() == NT_HASH) {
                            const QoreHashNode* h = elem.get<const QoreHashNode>();
                            QoreValue k = h->getKeyValue(mhk->key1.c_str());
                            QoreValue val = h->getKeyValue(mhk->key2.c_str());
                            QoreStringValueHelper key_str(k);
                            result->setKeyValue(key_str->c_str(), val.refSelf(), nullptr);
                        }
                    }
                    out = result.release();
                }
                setValueSlot(values, mhk->result.id, out, xsink);
                if (out.hasNode()) {
                    cleanup.push_back(mhk->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::LoadSelfMember: {
                auto* sm_inst = static_cast<QoreIRSelfMemberInstruction*>(inst);
                QoreObject* obj = runtime_get_stack_object();
                assert(obj);
                // issue 3523: evaluate in case the value is a reference
                ValueHolder val(obj->getReferencedMemberNoMethod(sm_inst->member_name.c_str(), xsink), xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                bool needs_eval = val->needsEval();
                QoreValue out = needs_eval ? val->eval(xsink) : val.release();
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                setValueSlot(values, sm_inst->result.id, out, xsink);
                if (out.hasNode()) {
                    cleanup.push_back(sm_inst->result.id);
                    if (needs_eval) {
                        // Strong ref from evaluating a reference-type member
                        // (e.g., WeakReferenceNode → strong ref). Mark ephemeral
                        // so it's cleaned at the next statement boundary, matching
                        // AST temporary lifetime semantics.
                        ephemeral_weak_ref_slots.push_back(sm_inst->result.id);
                    }
                }
                ++ip;
                break;
            }
            case QoreIROpcode::LoadStaticVar: {
                auto* sv_inst = static_cast<QoreIRStaticVarInstruction*>(inst);
                // issue 3523: evaluate in case the value is a reference
                ValueHolder val(sv_inst->vi->getReferencedValue(sv_inst->var_name.c_str(), xsink),
                        xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                bool needs_eval = val->needsEval();
                QoreValue out = needs_eval ? val->eval(xsink) : val.release();
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                setValueSlot(values, sv_inst->result.id, out, xsink);
                if (out.hasNode()) {
                    cleanup.push_back(sv_inst->result.id);
                    if (needs_eval) {
                        // Strong ref from evaluating a reference-type static var
                        // (e.g., WeakReferenceNode). Mark ephemeral for statement-
                        // boundary cleanup (see LoadSelfMember comment).
                        ephemeral_weak_ref_slots.push_back(sv_inst->result.id);
                    }
                }
                ++ip;
                break;
            }
            case QoreIROpcode::NewObject: {
                auto* no_inst = static_cast<QoreIRNewObjectInstruction*>(inst);
                RuntimeConfig& rc = rc_get_current_ref();
                QoreValue out = qore_class_private::execConstructor(*no_inst->qc, rc,
                        no_inst->variant, no_inst->args, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                setValueSlot(values, no_inst->result.id, out, xsink);
                if (out.hasNode()) {
                    cleanup.push_back(no_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::LoadConstant: {
                auto* lc_inst = static_cast<QoreIRLoadConstantInstruction*>(inst);
                bool needs_deref = true;
                QoreValue out = const_cast<RuntimeConstantRefNode*>(lc_inst->node)->eval(
                        needs_deref, xsink);
                if (!needs_deref && out.hasNode()) {
                    out = out.refSelf();
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                setValueSlot(values, lc_inst->result.id, out, xsink);
                if (out.hasNode()) {
                    cleanup.push_back(lc_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::CreateClosure: {
                auto* cc_inst = static_cast<QoreIRCreateClosureInstruction*>(inst);
                bool needs_deref = true;
                QoreValue out = const_cast<QoreClosureParseNode*>(cc_inst->closure_node)->eval(
                        needs_deref, xsink);
                if (!needs_deref && out.hasNode()) {
                    out = out.refSelf();
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                setValueSlot(values, cc_inst->result.id, out, xsink);
                if (out.hasNode()) {
                    cleanup.push_back(cc_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::CreateCallRef: {
                auto* cr_inst = static_cast<QoreIRCreateCallRefInstruction*>(inst);
                bool needs_deref = true;
                QoreValue ref_expr = cr_inst->expr.refSelf();
                QoreValue out = ref_expr.getInternalNode()->eval(needs_deref, xsink);
                if (!needs_deref && out.hasNode()) {
                    out = out.refSelf();
                }
                ref_expr.discard(xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                setValueSlot(values, cr_inst->result.id, out, xsink);
                if (out.hasNode()) {
                    cleanup.push_back(cr_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::CreateMethodRef: {
                auto* mr_inst = static_cast<QoreIRCreateMethodRefInstruction*>(inst);
                bool needs_deref = true;
                QoreValue ref_expr = mr_inst->expr.refSelf();
                QoreValue out = ref_expr.getInternalNode()->eval(needs_deref, xsink);
                if (!needs_deref && out.hasNode()) {
                    out = out.refSelf();
                }
                ref_expr.discard(xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                setValueSlot(values, mr_inst->result.id, out, xsink);
                if (out.hasNode()) {
                    cleanup.push_back(mr_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::CreateParseRef: {
                auto* pr_inst = static_cast<QoreIRCreateParseRefInstruction*>(inst);
                bool needs_deref = true;
                QoreValue out = const_cast<ParseReferenceNode*>(pr_inst->node)->eval(
                        needs_deref, xsink);
                if (!needs_deref && out.hasNode()) {
                    out = out.refSelf();
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                setValueSlot(values, pr_inst->result.id, out, xsink);
                if (out.hasNode()) {
                    cleanup.push_back(pr_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::NewHashDecl: {
                auto* nhd_inst = static_cast<QoreIRNewHashDeclInstruction*>(inst);
                bool needs_deref = true;
                QoreValue out = const_cast<NewHashDeclNode*>(nhd_inst->node)->eval(needs_deref, xsink);
                if (!needs_deref && out.hasNode()) {
                    out = out.refSelf();
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                setValueSlot(values, nhd_inst->result.id, out, xsink);
                if (out.hasNode()) {
                    cleanup.push_back(nhd_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::NewComplexHash: {
                auto* nch_inst = static_cast<QoreIRNewComplexHashInstruction*>(inst);
                bool needs_deref = true;
                QoreValue out = const_cast<NewComplexHashNode*>(nch_inst->node)->eval(needs_deref, xsink);
                if (!needs_deref && out.hasNode()) {
                    out = out.refSelf();
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                setValueSlot(values, nch_inst->result.id, out, xsink);
                if (out.hasNode()) {
                    cleanup.push_back(nch_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::NewComplexList: {
                auto* ncl_inst = static_cast<QoreIRNewComplexListInstruction*>(inst);
                bool needs_deref = true;
                QoreValue out = const_cast<NewComplexListNode*>(ncl_inst->node)->eval(needs_deref, xsink);
                if (!needs_deref && out.hasNode()) {
                    out = out.refSelf();
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                setValueSlot(values, ncl_inst->result.id, out, xsink);
                if (out.hasNode()) {
                    cleanup.push_back(ncl_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::VrnConstruct: {
                auto* vrn_inst = static_cast<QoreIRVrnConstructInstruction*>(inst);
                uint64_t result_bits = qore_rt_vrn_construct(vrn_inst->vrn, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                QoreValue out = fromBits(result_bits);
                setValueSlot(values, vrn_inst->result.id, out, xsink);
                if (out.hasNode()) {
                    cleanup.push_back(vrn_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::HashSetKeyValue: {
                QoreValue hash_val = getIRValue(values, inst->operands[0]);
                QoreValue key_val = getIRValue(values, inst->operands[1]);
                QoreValue value_val = getIRValue(values, inst->operands[2]);
                QoreStringValueHelper key_str(key_val);
                QoreHashNode* hash = hash_val.get<QoreHashNode>();
                if (value_val.hasNode()) {
                    value_val.refSelf();
                }
                hash->setKeyValue(key_str->c_str(), value_val, xsink);
                // Do NOT discard key_val here - it's managed by the IR value map cleanup
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                ++ip;
                break;
            }
            case QoreIROpcode::IteratorCreateReverse: {
                QoreValue iterable = getIRValue(values, inst->operands[0]);
                FunctionalOperator::FunctionalValueType value_type;
                FunctionalOperatorInterface* iter = FunctionalOperatorInterface::getFunctionalIterator(
                    value_type, iterable, false, "foldr operator", xsink);
                if ((xsink && *xsink) || value_type == FunctionalOperator::nothing) {
                    delete iter;
                    setValueSlot(values, inst->result.id, QoreValue(), xsink);
                    ++ip;
                    break;
                }
                // Store as int (pointer) — same pattern as IteratorCreate
                if (iter) {
                    active_iterators.insert(iter);
                }
                setValueSlot(values, inst->result.id,
                        QoreValue(reinterpret_cast<int64_t>(iter)), xsink);
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
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                ensureLocalInstantiated(local_inst->local, instantiated_locals, pre_instantiated,
                        function_own_locals, &locally_uninstantiated);
                QoreIRValue operand = local_inst->operands.front();
                QoreValue val = getIRValue(values, operand);

                // Handle weak assignment by wrapping in WeakReferenceNode at runtime
                if (local_inst->weak && val.hasNode()) {
                    qore_type_t type = val.getType();
                    if (type == NT_OBJECT) {
                        QoreObject* o = val.get<QoreObject>();
                        val = new WeakReferenceNode(o);
                    } else if (type == NT_HASH) {
                        QoreHashNode* h = val.get<QoreHashNode>();
                        val = new WeakHashReferenceNode(h);
                    } else if (type == NT_LIST) {
                        QoreListNode* l = val.get<QoreListNode>();
                        val = new WeakListReferenceNode(l);
                    }
                }

                // Don't cache closure-bound locals — closures can modify the value.
                // We invalidate (rather than pre-populate) the cache here because
                // assignLocalVarValue() → acceptAssignment() may coerce the value
                // (e.g., list → list<hash<auto>>).  When the list has refcount > 1
                // (due to the cache holding a ref), acceptInputComplexList() creates
                // a COPY with the correct value type info.  The local variable gets
                // the copy; if we cached the original, it would be stale.  The next
                // LoadLocal cache miss will read the actual coerced value.
                if (!local_inst->local || !local_inst->local->closureUse()) {
                    auto cache_it = locals.find(local_inst->local);
                    if (cache_it != locals.end()) {
                        cache_it->second.discard(xsink);
                        locals.erase(cache_it);
                    }
                    // Also invalidate slot cache (Phase 3 optimization)
                    if (local_inst->local) {
                        auto slot_it = func.local_var_slots.find(local_inst->local);
                        if (slot_it != func.local_var_slots.end() && slot_it->second < locals_slot_cache.size()) {
                            locals_slot_cache[slot_it->second].discard(xsink);
                            locals_slot_cache[slot_it->second] = QoreValue();
                        }
                    }
                }
                assignLocalVarValue(local_inst->local, val, xsink);

                // For weak assignments, cache the weak-wrapped value so LoadLocal
                // returns the WeakReferenceNode instead of calling eval() which unwraps it
                if (local_inst->weak && val.getType() >= NT_WEAKREF && val.getType() <= NT_WEAKREF_LIST) {
                    storeValue(locals, local_inst->local, val.hasNode() ? val.refSelf() : val, nullptr);
                }

                // If the variable holds a reference, assignLocalVarValue wrote through
                // the reference to another variable.  Clear the locals cache to prevent
                // stale reads from that target variable.
                if (local_inst->local
                        && QoreTypeInfo::isReference(local_inst->local->getTypeInfo())) {
                    cleanupStoredValues(locals, xsink);
                }
                // Track the operand slot for cleanup when this local is uninstantiated
                if (operand.isValid()) {
                    local_init_slots[local_inst->local] = operand.id;
                }
                if (xsink && *xsink) {
                    if (inst->exception_target) {
                        prev_block = block;
                        block = inst->exception_target;
                        ip = 0;
                        break;
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                ++ip;
                break;
            }
            case QoreIROpcode::UninstantiateLocal: {
                auto* local_inst = static_cast<QoreIRLocalInstruction*>(inst);
                // Uninstantiate the local variable (calls destructor for objects)
                if (local_inst->local) {
                    // Skip outer-scope variables: these were never instantiated by
                    // this function (ensureLocalInstantiated skips them) and must
                    // not be uninstantiated here — doing so would pop an entry
                    // from the calling function's stack frame, corrupting the
                    // variable stack.
                    if (function_own_locals
                            && !function_own_locals->count(
                                reinterpret_cast<const void*>(local_inst->local))) {
                        ++ip;
                        break;
                    }
                    bool is_pre = pre_instantiated && pre_instantiated->find(local_inst->local) != pre_instantiated->end();
                    // Check if this variable was actually instantiated by the IR
                    // interpreter. For non-pre-instantiated (IR-only) variables,
                    // ensureLocalInstantiated() pushes them on first use. If a
                    // variable was never used (e.g. in a code path not taken),
                    // it was never pushed, so we must not pop it here.
                    bool was_instantiated = instantiated_locals.count(local_inst->local) > 0;
                    // Remove from instantiated_locals since we're explicitly cleaning it up
                    instantiated_locals.erase(local_inst->local);
                    if (!is_pre && !was_instantiated) {
                        // Variable was never instantiated by this function —
                        // skip uninstantiation to avoid popping an unrelated
                        // stack entry.
                        ++ip;
                        break;
                    }

                    // Track any variable that we're explicitly uninstantiating mid-execution
                    // (both pre-instantiated loop variables AND block-local variables).
                    // These must be re-instantiated on next use by ensureLocalInstantiated.
                    // For pre-instantiated: the caller won't re-instantiate them per-iteration.
                    // For non-pre-instantiated: we need to ensure they're instantiated before next use.
                    locally_uninstantiated.insert(local_inst->local);

                    // Drop the locals cache reference FIRST (before lvar->del) so that
                    // lvar->del() is the final deref and triggers the destructor.
                    // This is critical because destructors run AST code that can modify
                    // globals — if lvar->del() isn't the last deref, the destructor
                    // fires during locals cache discard which is too late for proper
                    // globals cache invalidation.
                    auto locals_it = locals.find(local_inst->local);
                    if (locals_it != locals.end()) {
                        locals_it->second.discard(xsink);
                        locals.erase(locals_it);
                    }
                    // Also drop from slot cache (Phase 3 optimization)
                    auto slot_it = func.local_var_slots.find(local_inst->local);
                    if (slot_it != func.local_var_slots.end() && slot_it->second < locals_slot_cache.size()) {
                        locals_slot_cache[slot_it->second].discard(xsink);
                        locals_slot_cache[slot_it->second] = QoreValue();
                    }

                    // Helper lambda: drop all value slots associated with this local
                    // (init slot from StoreLocal + load slots from LoadLocal) BEFORE
                    // lvar->del()/uninstantiate() so that lvar->del() is the true
                    // final deref and triggers the destructor immediately.
                    auto cleanupLocalSlots = [&](const LocalVar* lv) {
                        // Clean up the init slot (from StoreLocal / VarRefNewObjectNode)
                        auto slot_it = local_init_slots.find(lv);
                        if (slot_it != local_init_slots.end()) {
                            uint32_t slot_id = slot_it->second;
                            if (slot_id < values.size()) {
                                values[slot_id].discard(xsink);
                                values[slot_id] = QoreValue();  // Set to NOTHING instead of erase
                            }
                            cleanup.erase(std::remove(cleanup.begin(), cleanup.end(), slot_id), cleanup.end());
                            local_init_slots.erase(slot_it);
                        }
                        // Clean up all LoadLocal result slots for this local.
                        // On the last loop iteration, these slots are never overwritten
                        // by the next iteration's LoadLocal, so they hold an extra
                        // reference that defers the object's destructor to function exit.
                        auto load_it = local_load_slots.find(lv);
                        if (load_it != local_load_slots.end()) {
                            for (uint32_t slot_id : load_it->second) {
                                if (slot_id < values.size()) {
                                    values[slot_id].discard(xsink);
                                    values[slot_id] = QoreValue();  // Set to NOTHING instead of erase
                                }
                                cleanup.erase(std::remove(cleanup.begin(), cleanup.end(), slot_id),
                                    cleanup.end());
                            }
                            local_load_slots.erase(load_it);
                        }
                    };

                    if (is_pre) {
                        // Pre-instantiated local: clear the value on the runtime stack
                        // to trigger destructors at block scope exit.  The entry stays
                        // on the stack so the caller's cleanup can pop it later.

                        // Drop value slot references (init + load slots)
                        cleanupLocalSlots(local_inst->local);

                        // lvar->del() / cvv->clearValue() is the FINAL deref that
                        // triggers the destructor.  After this call, invalidate the
                        // globals cache since the destructor runs AST code.
                        // For closure-captured locals: only clear the value if no
                        // closures still hold references (references == 1 means only
                        // the cvstack entry remains).  When references > 1, closures
                        // may still need the value (e.g., submitted to a thread pool).
                        if (local_inst->local->closureUse()) {
                            ClosureVarValue* cvv = thread_find_closure_var(
                                local_inst->local->getName());
                            if (cvv && cvv->references.load(std::memory_order_acquire) == 1) {
                                cvv->clearValue(xsink);
                            }
                        } else {
                            LocalVarValue* lvar = thread_find_lvar(local_inst->local->getName());
                            if (lvar) {
                                lvar->del(xsink);
                            }
                        }
                        // Destructor may have modified globals via AST code
                        cleanupStoredValues(globals, xsink);
                    } else {
                        // Non-pre-instantiated: full uninstantiate (pop + destructor)
                        // Clean up value slots (init + load) BEFORE uninstantiating
                        cleanupLocalSlots(local_inst->local);
                        local_inst->local->uninstantiate(xsink);
                        // Destructor may have modified globals via AST code
                        cleanupStoredValues(globals, xsink);
                    }
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
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                QoreValue val = getIRValue(values, local_inst->operands.front());

                // Handle weak assignment by wrapping in WeakReferenceNode at runtime
                if (local_inst->weak && val.hasNode()) {
                    qore_type_t type = val.getType();
                    if (type == NT_OBJECT) {
                        QoreObject* o = val.get<QoreObject>();
                        val = new WeakReferenceNode(o);
                    } else if (type == NT_HASH) {
                        QoreHashNode* h = val.get<QoreHashNode>();
                        val = new WeakHashReferenceNode(h);
                    } else if (type == NT_LIST) {
                        QoreListNode* l = val.get<QoreListNode>();
                        val = new WeakListReferenceNode(l);
                    }
                }

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
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
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
                    QoreValue val = validateWeakRef(it->second);
                    out = val.hasNode() ? val.refSelf() : val;
                } else {
                    // Read from the actual global variable when not in the local cache
                    out = var_inst->var->eval();
                }
                setValueSlot(values, var_inst->result.id, out, xsink);
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
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                QoreValue val = getIRValue(values, var_inst->operands.front());

                // Handle weak assignment by wrapping in WeakReferenceNode at runtime
                if (var_inst->weak && val.hasNode()) {
                    qore_type_t type = val.getType();
                    if (type == NT_OBJECT) {
                        QoreObject* o = val.get<QoreObject>();
                        val = new WeakReferenceNode(o);
                    } else if (type == NT_HASH) {
                        QoreHashNode* h = val.get<QoreHashNode>();
                        val = new WeakHashReferenceNode(h);
                    } else if (type == NT_LIST) {
                        QoreListNode* l = val.get<QoreListNode>();
                        val = new WeakListReferenceNode(l);
                    }
                }

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
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
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
                setValueSlot(values, var_inst->result.id, out, xsink);
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
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                QoreValue val = getIRValue(values, var_inst->operands.front());

                // Handle weak assignment by wrapping in WeakReferenceNode at runtime
                if (var_inst->weak && val.hasNode()) {
                    qore_type_t type = val.getType();
                    if (type == NT_OBJECT) {
                        QoreObject* o = val.get<QoreObject>();
                        val = new WeakReferenceNode(o);
                    } else if (type == NT_HASH) {
                        QoreHashNode* h = val.get<QoreHashNode>();
                        val = new WeakHashReferenceNode(h);
                    } else if (type == NT_LIST) {
                        QoreListNode* l = val.get<QoreListNode>();
                        val = new WeakListReferenceNode(l);
                    }
                }

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
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                ++ip;
                break;
            }
            case QoreIROpcode::LoadImplicitArg: {
                auto* impl_arg_inst = static_cast<QoreIRImplicitArgInstruction*>(inst);
                const QoreListNode* argv = thread_get_implicit_args();
                QoreValue out;
                if (argv) {
                    out = argv->retrieveEntry(impl_arg_inst->offset);
                    if (out.hasNode()) {
                        out = out.refSelf();
                    }
                }
                setValueSlot(values, impl_arg_inst->result.id, out, xsink);
                if (out.hasNode()) {
                    cleanup.push_back(impl_arg_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::LoadImplicitArgv: {
                const QoreListNode* argv = thread_get_implicit_args();
                QoreValue out;
                if (argv) {
                    out = const_cast<QoreListNode*>(argv);
                    out.refSelf();
                }
                setValueSlot(values, inst->result.id, out, xsink);
                if (out.hasNode()) {
                    cleanup.push_back(inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::LoadImplicitElement: {
                int64 element = get_implicit_element();
                setValueSlot(values, inst->result.id, element, xsink);
                ++ip;
                break;
            }
            case QoreIROpcode::PushImplicitArg: {
                QoreValue val = getIRValue(values, inst->operands[0]);
                // Save old context
                const QoreListNode* old_argv = thread_get_implicit_args();
                if (old_argv) {
                    const_cast<QoreListNode*>(old_argv)->ref();
                }
                // Create new argv with val as $1
                QoreListNode* new_argv = nullptr;
                if (!val.isNothing()) {
                    new_argv = new QoreListNode(autoTypeInfo);
                    new_argv->push(val.refSelf(), xsink);
                }
                thread_set_implicit_args(new_argv);
                // Store old context as result for later restoration
                setValueSlot(values, inst->result.id, QoreValue(const_cast<QoreListNode*>(old_argv)), xsink);
                if (old_argv) {
                    cleanup.push_back(inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::SetImplicitArgv: {
                QoreValue argv_val = getIRValue(values, inst->operands[0]);
                // Save old context
                const QoreListNode* old_argv = thread_get_implicit_args();
                if (old_argv) {
                    const_cast<QoreListNode*>(old_argv)->ref();
                }
                // Set the list directly as implicit args (used for foldl $1/$2)
                QoreListNode* new_argv = argv_val.get<QoreListNode>();
                if (new_argv) {
                    new_argv->ref();  // Take a reference since we're using it directly
                }
                thread_set_implicit_args(new_argv);
                // Store old context as result for later restoration
                setValueSlot(values, inst->result.id, QoreValue(const_cast<QoreListNode*>(old_argv)), xsink);
                if (old_argv) {
                    cleanup.push_back(inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::PopImplicitArg: {
                QoreValue old_val = getIRValue(values, inst->operands[0]);
                QoreListNode* old_argv = old_val.get<QoreListNode>();
                // Deref current argv
                const QoreListNode* current = thread_get_implicit_args();
                if (current) {
                    const_cast<QoreListNode*>(current)->deref(xsink);
                }
                // Restore old context
                thread_set_implicit_args(old_argv);
                ++ip;
                break;
            }
            case QoreIROpcode::PushImplicitElement: {
                QoreValue idx = getIRValue(values, inst->operands[0]);
                int64 old_element = save_implicit_element(static_cast<int>(idx.getAsBigInt()));
                setValueSlot(values, inst->result.id, old_element, xsink);
                ++ip;
                break;
            }
            case QoreIROpcode::PopImplicitElement: {
                QoreValue old_val = getIRValue(values, inst->operands[0]);
                save_implicit_element(static_cast<int>(old_val.getAsBigInt()));
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
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                cleanupStoredValues(locals, xsink);
                cleanupStoredValues(globals, xsink);
                cleanupStoredValues(threadlocals, xsink);
                cleanupStoredValues(closures, xsink);
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
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                cleanupStoredValues(locals, xsink);
                cleanupStoredValues(globals, xsink);
                cleanupStoredValues(threadlocals, xsink);
                cleanupStoredValues(closures, xsink);
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
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                cleanupStoredValues(locals, xsink);
                cleanupStoredValues(globals, xsink);
                cleanupStoredValues(threadlocals, xsink);
                cleanupStoredValues(closures, xsink);
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
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                cleanupStoredValues(locals, xsink);
                cleanupStoredValues(globals, xsink);
                cleanupStoredValues(threadlocals, xsink);
                cleanupStoredValues(closures, xsink);
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
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
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
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                cleanupStoredValues(locals, xsink);
                cleanupStoredValues(globals, xsink);
                cleanupStoredValues(threadlocals, xsink);
                cleanupStoredValues(closures, xsink);
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
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                // Store iterator pointer as int64_t in result
                // A nullptr means the iterable was empty (nothing)
                if (iter) {
                    active_iterators.insert(iter);
                }
                setValueSlot(values, iter_inst->result.id, QoreValue(reinterpret_cast<int64_t>(iter)), xsink);
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
                    active_iterators.erase(iter);
                    delete iter;
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                if (done) {
                    // Iterator exhausted - clean up and branch to done
                    active_iterators.erase(iter);
                    delete iter;
                    // Clear the iterator value so we don't double-delete
                    setValueSlot(values, iter_inst->iterator.id, QoreValue(static_cast<int64_t>(0)), xsink);
                    prev_block = block;
                    block = iter_inst->done_target;
                    ip = 0;
                } else {
                    // Store current value in result and branch to continue
                    // Use setValueSlot since this executes in a loop
                    setValueSlot(values, iter_inst->result.id, val.takeReferencedValue(), xsink);
                    cleanup.push_back(iter_inst->result.id);
                    prev_block = block;
                    block = iter_inst->continue_target;
                    ip = 0;
                }
                break;
            }

            // --- Reference foreach opcodes ---
            case QoreIROpcode::RefForeachInit: {
                auto* ref_init = static_cast<QoreIRRefForeachInitInstruction*>(inst);
                uint64_t state = qore_rt_ref_foreach_init(toBits(ref_init->expr), xsink);
                if (xsink && *xsink) {
                    if (inst->exception_target) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        prev_block = block;
                        block = inst->exception_target;
                        ip = 0;
                        break;
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                setValueSlot(values, inst->result.id,
                    QoreValue(static_cast<int64_t>(state)), xsink);
                ++ip;
                break;
            }
            case QoreIROpcode::RefForeachSize: {
                QoreValue state_val = getIRValue(values, inst->operands[0]);
                int64_t size = qore_rt_ref_foreach_size(
                    static_cast<uint64_t>(state_val.getAsBigInt()));
                setValueSlot(values, inst->result.id, QoreValue(size), xsink);
                ++ip;
                break;
            }
            case QoreIROpcode::RefForeachGetEntry: {
                QoreValue state_val = getIRValue(values, inst->operands[0]);
                QoreValue index_val = getIRValue(values, inst->operands[1]);
                uint64_t entry = qore_rt_ref_foreach_get_entry(
                    static_cast<uint64_t>(state_val.getAsBigInt()),
                    index_val.getAsBigInt(), xsink);
                if (xsink && *xsink) {
                    if (inst->exception_target) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        prev_block = block;
                        block = inst->exception_target;
                        ip = 0;
                        break;
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                QoreValue result = fromBits(entry);
                setValueSlot(values, inst->result.id, result, xsink);
                if (result.hasNode()) {
                    cleanup.push_back(inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::RefForeachRecord: {
                QoreValue state_val = getIRValue(values, inst->operands[0]);
                QoreValue value_val = getIRValue(values, inst->operands[1]);
                qore_rt_ref_foreach_record(
                    static_cast<uint64_t>(state_val.getAsBigInt()),
                    toBits(value_val), xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                ++ip;
                break;
            }
            case QoreIROpcode::RefForeachFinalize: {
                QoreValue state_val = getIRValue(values, inst->operands[0]);
                QoreValue fill_val = getIRValue(values, inst->operands[1]);
                qore_rt_ref_foreach_finalize(
                    static_cast<uint64_t>(state_val.getAsBigInt()),
                    fill_val.getAsBigInt(), xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                ++ip;
                break;
            }
            case QoreIROpcode::RefForeachCleanup: {
                QoreValue state_val = getIRValue(values, inst->operands[0]);
                qore_rt_ref_foreach_cleanup(
                    static_cast<uint64_t>(state_val.getAsBigInt()), xsink);
                ++ip;
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
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                // Record the handler for deferred execution at block/function exit.
                // Don't call exec() here - the AST's exec() calls advance_on_block_exit()
                // which requires the thread on_block_exit stack to be set up by StatementBlock,
                // but the IR interpreter doesn't go through StatementBlock::execIntern().
                on_block_exit_handlers.push_back({obe_inst->stmt->getType(),
                    obe_inst->stmt->getCode(),
                    obe_inst->handler_ir ? obe_inst->handler_ir.get() : nullptr});
                ++ip;
                break;
            }
            case QoreIROpcode::ScopeEnter: {
                // Record current handler list size so ScopeExit knows which handlers to execute
                scope_stack.push_back(on_block_exit_handlers.size());
                ++ip;
                break;
            }
            case QoreIROpcode::ScopeExit: {
                auto* scope_inst = static_cast<QoreIRScopeExitInstruction*>(inst);
                // Execute handlers registered since matching ScopeEnter
                if (!scope_stack.empty()) {
                    size_t scope_start = scope_stack.back();
                    scope_stack.pop_back();
                    // Execute handlers in reverse order (LIFO) from current to scope_start
                    if (on_block_exit_handlers.size() > scope_start) {
                        ExceptionSink obe_xsink;
                        bool error = scope_inst->is_error || (xsink && xsink->isException());
                        for (size_t i = on_block_exit_handlers.size(); i > scope_start; --i) {
                            obe_type_e type = on_block_exit_handlers[i - 1].type;
                            if (type == OBE_Unconditional || (!error && type == OBE_Success) || (error && type == OBE_Error)) {
                                if (on_block_exit_handlers[i - 1].code || on_block_exit_handlers[i - 1].handler_ir) {
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
                                    executeHandlerBody(on_block_exit_handlers[i - 1], &obe_xsink);
                                    // Restore td->catchException BEFORE clearing xsink to avoid
                                    // a use-after-free window where td->catchException points to
                                    // the freed exception chain
                                    ex_helper.reset();
                                    argv_helper.reset();
                                    if (type == OBE_Error) {
                                        if (qore_es_private::get(obe_xsink)->rethrown) {
                                            if (xsink) {
                                                xsink->clear();
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
                        // Remove executed handlers
                        on_block_exit_handlers.resize(scope_start);
                        // Handler execution (both AST and compiled IR) can modify any local,
                        // global, thread-local, or closure variable on the thread-local stack.
                        // Clear all caches (including slot cache) so subsequent variable reads
                        // re-fetch from the runtime.
                        cleanupLocalCaches();
                    }
                }
                ++ip;
                break;
            }
            case QoreIROpcode::ThreadExit:
                if (xsink) {
                    xsink->raiseThreadExit();
                }
                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                cleanupStoredValues(locals, xsink);
                cleanupStoredValues(globals, xsink);
                cleanupStoredValues(threadlocals, xsink);
                cleanupStoredValues(closures, xsink);
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
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                QoreValue value = getIRValue(values, guard_inst->operands.front());
                // Record type profile for this guard point
                if (guard_inst->guard_id < func.guard_profile_count) {
                    func.guard_profiles[guard_inst->guard_id].record(value);
                }
                if (!guardPredicate(inst->opcode, value, guard_inst->type_info)) {
                    if (guard_inst->deopt_target && !suppress_guard_deopt) {
                        // Guard type check failed — fall back to AST execution.
                        // Don't raise an exception; returning false without setting
                        // xsink tells evalTiered() to re-execute via AST.
                        printd(2, "QoreIRInterpreter::execute() guard failed for '%s' "
                            "— falling back to AST\n", func.name.c_str());
                        // Fire on_block_exit handlers before cleanup — guards may fire
                        // after side-effecting code (e.g., Mutex::lock() + on_exit
                        // Mutex::unlock()); without this, on_exit handlers are orphaned
                        // and resources like mutexes remain locked.
                        executeOnBlockExitHandlers(on_block_exit_handlers, xsink);
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupStoredValues(locals, xsink);
                        cleanupStoredValues(globals, xsink);
                        cleanupStoredValues(threadlocals, xsink);
                        cleanupStoredValues(closures, xsink);
                        return false;
                    }
                    // No deopt target, or top-level code (suppress_guard_deopt) —
                    // guard failure is handled silently; continue execution with the
                    // current value. For top-level code, deopt would re-execute the
                    // entire block, duplicating side effects (I/O, mutations).
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
            case QoreIROpcode::UnaryMinusAny:
            case QoreIROpcode::ExistsAny:
            case QoreIROpcode::ExistsBool: {
                if (inst->operands.size() < 1) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "unary op missing operand");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                QoreValue val = getIRValue(values, inst->operands[0]);
                QoreValue res = QoreIRInterpreter::evalUnary(inst->opcode, val, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                setValueSlot(values, inst->result.id, res, xsink);
                if (res.hasNode()) {
                    cleanup.push_back(inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::AddInt:
            case QoreIROpcode::AddFloat:
            case QoreIROpcode::AddAny:
            case QoreIROpcode::AddString:
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
            case QoreIROpcode::FoldrSumInt:
            case QoreIROpcode::FoldrSumFloat:
            case QoreIROpcode::FoldrProdInt:
            case QoreIROpcode::FoldrProdFloat:
            case QoreIROpcode::FoldrDiffInt:
            case QoreIROpcode::FoldrDiffFloat:
            case QoreIROpcode::FoldrMinInt:
            case QoreIROpcode::FoldrMinFloat:
            case QoreIROpcode::FoldrMaxInt:
            case QoreIROpcode::FoldrMaxFloat:
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
            case QoreIROpcode::FusedMapFoldlSumScaleInt:
            case QoreIROpcode::FusedMapFoldlSumScaleFloat:
            case QoreIROpcode::FusedMapFoldlSumSquareInt:
            case QoreIROpcode::FusedMapFoldlSumSquareFloat:
            case QoreIROpcode::FusedMapFoldlProdScaleInt:
            case QoreIROpcode::FusedMapFoldlProdScaleFloat:
            case QoreIROpcode::RangeAny:
            case QoreIROpcode::RangeInt:
            case QoreIROpcode::RangeFloat:
            case QoreIROpcode::RangeDate:
            case QoreIROpcode::EqInt:
            case QoreIROpcode::EqFloat:
            case QoreIROpcode::EqString:
            case QoreIROpcode::EqAny:
            case QoreIROpcode::NeInt:
            case QoreIROpcode::NeFloat:
            case QoreIROpcode::NeString:
            case QoreIROpcode::NeAny:
            case QoreIROpcode::EqHard:
            case QoreIROpcode::NeHard:
            case QoreIROpcode::LtInt:
            case QoreIROpcode::LtFloat:
            case QoreIROpcode::LtString:
            case QoreIROpcode::LtAny:
            case QoreIROpcode::LeInt:
            case QoreIROpcode::LeFloat:
            case QoreIROpcode::LeString:
            case QoreIROpcode::LeAny:
            case QoreIROpcode::GtInt:
            case QoreIROpcode::GtFloat:
            case QoreIROpcode::GtString:
            case QoreIROpcode::GtAny:
            case QoreIROpcode::GeInt:
            case QoreIROpcode::GeFloat:
            case QoreIROpcode::GeString:
            case QoreIROpcode::GeAny:
            case QoreIROpcode::CmpInt:
            case QoreIROpcode::CmpFloat:
            case QoreIROpcode::CmpString:
            case QoreIROpcode::CmpAny: {
                if (inst->operands.size() < 2) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "binary op missing operands");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                QoreValue left = getIRValue(values, inst->operands[0]);
                QoreValue right = getIRValue(values, inst->operands[1]);
                QoreValue res = QoreIRInterpreter::evalBinary(inst->opcode, left, right, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                setValueSlot(values, inst->result.id, res, xsink);
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
                QoreValue res;
                if (inst->operands.empty()) {
                    // Delegate-to-AST: operands are empty, expression stored in inst->expr
                    auto* expr_inst = static_cast<QoreIRExprInstruction*>(inst);
                    res = QoreIRInterpreter::evalExpr(inst->opcode, expr_inst->expr, xsink);
                } else if (inst->operands.size() < 3) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "ternary op missing operands");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                } else {
                    QoreValue first = getIRValue(values, inst->operands[0]);
                    QoreValue second = getIRValue(values, inst->operands[1]);
                    QoreValue third = getIRValue(values, inst->operands[2]);
                    res = QoreIRInterpreter::evalTernary(inst->opcode, first, second, third, xsink);
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                setValueSlot(values, inst->result.id, res, xsink);
                if (res.hasNode()) {
                    cleanup.push_back(inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::HashMapSelectAny:
            case QoreIROpcode::HashMapSelect: {
                QoreValue res;
                if (inst->operands.empty()) {
                    // Delegate-to-AST: operands are empty, expression stored in inst->expr
                    auto* expr_inst = static_cast<QoreIRExprInstruction*>(inst);
                    res = QoreIRInterpreter::evalExpr(inst->opcode, expr_inst->expr, xsink);
                } else if (inst->operands.size() < 4) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "quaternary op missing operands");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                } else {
                    QoreValue first = getIRValue(values, inst->operands[0]);
                    QoreValue second = getIRValue(values, inst->operands[1]);
                    QoreValue third = getIRValue(values, inst->operands[2]);
                    QoreValue fourth = getIRValue(values, inst->operands[3]);
                    res = QoreIRInterpreter::evalQuaternary(inst->opcode, first, second, third, fourth, xsink);
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                setValueSlot(values, inst->result.id, res, xsink);
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
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                setValueSlot(values, lval_inst->result.id, res, xsink);
                if (res.hasNode()) {
                    cleanup.push_back(lval_inst->result.id);
                }
                updateLocalVarFromLvalue(locals, instantiated_locals, lval_inst->lvalue, res, xsink, pre_instantiated, function_own_locals, &locally_uninstantiated,
                    &func.local_var_slots, &locals_slot_cache);
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
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                // Extract the base variable from the (possibly complex) lvalue
                // expression — e.g. for `lst[0] = val`, extract the VarRefNode
                // for `lst`.  Used for ensureLocalInstantiated and for cleaning
                // LoadLocal values[] entries to prevent refcount inflation.
                const VarRefNode* base_var = extractLValueBaseVarRef(lval_inst->lvalue);
                if (base_var) {
                    qore_var_t type = base_var->getType();
                    if ((type == VT_LOCAL || type == VT_LOCAL_TS) && base_var->ref.id) {
                        ensureLocalInstantiated(base_var->ref.id, instantiated_locals, pre_instantiated,
                                function_own_locals, &locally_uninstantiated);
                    }
                }
                QoreValue val = getIRValue(values, lval_inst->operands[0]);
                // Invalidate all caches BEFORE the lvalue operation so that
                // LValueHelper::ensureUnique() sees the variable's natural
                // refcount and only triggers COW when truly necessary.
                // See design/lvalue-loads-in-ir.md for the full invariant.
                cleanupStoredValues(locals, xsink);
                cleanupStoredValues(globals, xsink);
                cleanupStoredValues(threadlocals, xsink);
                cleanupStoredValues(closures, xsink);
                for (size_t i = 0; i < locals_slot_cache.size(); ++i) {
                    locals_slot_cache[i].discard(xsink);
                    locals_slot_cache[i] = QoreValue();
                }
                // Also discard values[] entries from LoadLocal results for the
                // target variable.  These hold +1 references (from guard loads
                // or prior reads) that inflate the refcount beyond the caches.
                // Setting values[slot] to NOTHING is safe: the guard BrIf has
                // already consumed the value; cleanupValues() will no-op on
                // NOTHING entries still in the cleanup vector.
                if (base_var && base_var->ref.id) {
                    auto llslot_it = local_load_slots.find(base_var->ref.id);
                    if (llslot_it != local_load_slots.end()) {
                        for (uint32_t slot_id : llslot_it->second) {
                            if (slot_id < values.size()) {
                                values[slot_id].discard(xsink);
                                values[slot_id] = QoreValue();
                            }
                        }
                    }
                }
                QoreValue res = QoreIRInterpreter::evalLValueStore(lval_inst->lvalue, val, xsink,
                    lval_inst->weak);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                // StoreLValue has no result register (result.id == 0); discard
                // the returned reference to avoid storing into values[0] which
                // causes double-free when multiple stores accumulate in cleanup
                if (lval_inst->result.isValid()) {
                    setValueSlot(values, lval_inst->result.id, res, xsink);
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
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                cleanupStoredValues(locals, xsink);
                cleanupStoredValues(globals, xsink);
                cleanupStoredValues(threadlocals, xsink);
                cleanupStoredValues(closures, xsink);
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
                        cleanupStoredValues(locals, xsink);
                        cleanupStoredValues(globals, xsink);
                        cleanupStoredValues(threadlocals, xsink);
                        cleanupStoredValues(closures, xsink);
                        return false;
                    }
                }
                setValueSlot(values, lval_inst->result.id, result_val, xsink);
                if (result_val.hasNode()) {
                    cleanup.push_back(lval_inst->result.id);
                }
                // Always reload the lvalue to update local var cache with new value
                QoreValue updated = QoreIRInterpreter::evalLValueLoad(lval_inst->lvalue, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                updateLocalVarFromLvalue(locals, instantiated_locals, lval_inst->lvalue, updated, xsink, pre_instantiated, function_own_locals, &locally_uninstantiated,
                    &func.local_var_slots, &locals_slot_cache);
                // discard the reload value if not used as result (for post ops, it's only for cache update)
                if (is_post) {
                    updated.discard(xsink);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::ShiftLValue: {
                auto* lval_inst = static_cast<QoreIRLValueInstruction*>(inst);
                // Invalidate all caches BEFORE the lvalue operation (see design/lvalue-loads-in-ir.md)
                cleanupStoredValues(locals, xsink);
                cleanupStoredValues(globals, xsink);
                cleanupStoredValues(threadlocals, xsink);
                cleanupStoredValues(closures, xsink);
                for (size_t i = 0; i < locals_slot_cache.size(); ++i) {
                    locals_slot_cache[i].discard(xsink);
                    locals_slot_cache[i] = QoreValue();
                }
                QoreValue res = QoreIRInterpreter::evalLValueUnary(inst->opcode, lval_inst->lvalue, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                // shift returns the removed element, not the updated list
                setValueSlot(values, lval_inst->result.id, res, xsink);
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
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                QoreValue right = getIRValue(values, lval_inst->operands[0]);
                // Invalidate all caches BEFORE the lvalue operation.
                // The slot cache holds refSelf() references that inflate refcounts,
                // causing LValueHelper::ensureUnique() to trigger COW unnecessarily.
                // The modification would be applied to the COW copy while the cache
                // retains the stale original.  See design/lvalue-loads-in-ir.md.
                cleanupStoredValues(locals, xsink);
                cleanupStoredValues(globals, xsink);
                cleanupStoredValues(threadlocals, xsink);
                cleanupStoredValues(closures, xsink);
                for (size_t i = 0; i < locals_slot_cache.size(); ++i) {
                    locals_slot_cache[i].discard(xsink);
                    locals_slot_cache[i] = QoreValue();
                }
                QoreValue res = QoreIRInterpreter::evalLValueBinary(inst->opcode, lval_inst->lvalue, right, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                setValueSlot(values, lval_inst->result.id, res, xsink);
                if (res.hasNode()) {
                    cleanup.push_back(lval_inst->result.id);
                }
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
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                QoreValue first = getIRValue(values, lval_inst->operands[0]);
                QoreValue second = getIRValue(values, lval_inst->operands[1]);
                QoreValue third = getIRValue(values, lval_inst->operands[2]);
                // Invalidate all caches BEFORE the lvalue operation (see design/lvalue-loads-in-ir.md)
                cleanupStoredValues(locals, xsink);
                cleanupStoredValues(globals, xsink);
                cleanupStoredValues(threadlocals, xsink);
                cleanupStoredValues(closures, xsink);
                for (size_t i = 0; i < locals_slot_cache.size(); ++i) {
                    locals_slot_cache[i].discard(xsink);
                    locals_slot_cache[i] = QoreValue();
                }
                QoreValue res = QoreIRInterpreter::evalLValueTernary(inst->opcode, lval_inst->lvalue, first, second,
                    third, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                setValueSlot(values, lval_inst->result.id, res, xsink);
                if (res.hasNode()) {
                    cleanup.push_back(lval_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::CallDirect:
            case QoreIROpcode::Call:
            case QoreIROpcode::CallIndirect:
            case QoreIROpcode::CallMethod:
            case QoreIROpcode::CallStatic:
            case QoreIROpcode::CallStaticDirect: {
                // CallDirect/CallStaticDirect use their own instruction classes (with extra
                // fields for LLVM lowering), but in the interpreter they behave identically
                // to Call/CallStatic. Extract expr from the appropriate instruction type.
                QoreValue call_expr;
                QoreIROpcode effective_opcode;
                if (inst->opcode == QoreIROpcode::CallDirect) {
                    call_expr = static_cast<QoreIRCallDirectInstruction*>(inst)->expr;
                    effective_opcode = QoreIROpcode::Call;
                } else if (inst->opcode == QoreIROpcode::CallStaticDirect) {
                    call_expr = static_cast<QoreIRCallStaticDirectInstruction*>(inst)->expr;
                    effective_opcode = QoreIROpcode::CallStatic;
                } else {
                    call_expr = static_cast<QoreIRExprInstruction*>(inst)->expr;
                    effective_opcode = inst->opcode;
                }
                QoreValue res;
                bool used_operands = false;
                if (!inst->operands.empty()) {
                    const ParseNode* parse_node = nullptr;
                    if (call_expr.hasNode()) {
                        parse_node = dynamic_cast<const ParseNode*>(call_expr.getInternalNode());
                    }
                    const QoreProgramLocation* loc = parse_node ? parse_node->loc : nullptr;
                    // For CallIndirect, operand[0] is the callee — skip it when building args
                    size_t arg_start = (effective_opcode == QoreIROpcode::CallIndirect) ? 1 : 0;
                    QoreListNode* arg_list = buildArgList(values, inst->operands, arg_start, xsink);
                    if (xsink && *xsink) {
                        if (arg_list) {
                            arg_list->deref(xsink);
                        }
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupStoredValues(locals, xsink);
                        cleanupStoredValues(globals, xsink);
                        cleanupStoredValues(threadlocals, xsink);
                        cleanupStoredValues(closures, xsink);
                        return false;
                    }
                    if (effective_opcode == QoreIROpcode::Call) {
                        if (auto* call = dynamic_cast<const FunctionCallNode*>(call_expr.getInternalNode())) {
                            QoreValue ce(new FunctionCallNode(*call, arg_list));
                            ValueHolder call_holder(ce, nullptr);
                            res = QoreIRInterpreter::evalExpr(effective_opcode, ce, xsink);
                            used_operands = true;
                        }
                    } else if (effective_opcode == QoreIROpcode::CallMethod) {
                        if (auto* call = dynamic_cast<const SelfFunctionCallNode*>(call_expr.getInternalNode())) {
                            QoreValue ce(new SelfFunctionCallNode(*call, arg_list));
                            ValueHolder call_holder(ce, nullptr);
                            res = QoreIRInterpreter::evalExpr(effective_opcode, ce, xsink);
                            used_operands = true;
                        }
                    } else if (effective_opcode == QoreIROpcode::CallStatic) {
                        if (auto* call = dynamic_cast<const StaticMethodCallNode*>(call_expr.getInternalNode())) {
                            QoreValue ce(new StaticMethodCallNode(*call, arg_list));
                            ValueHolder call_holder(ce, nullptr);
                            res = QoreIRInterpreter::evalExpr(effective_opcode, ce, xsink);
                            used_operands = true;
                        }
                    } else {
                        if (auto* call = dynamic_cast<const CallReferenceCallNode*>(
                            call_expr.getInternalNode())) {
                            QoreValue exp = call->getExp();
                            if (exp.hasNode()) {
                                exp = exp.refSelf();
                            }
                            QoreValue ce(new CallReferenceCallNode(loc, exp, arg_list));
                            ValueHolder call_holder(ce, nullptr);
                            res = QoreIRInterpreter::evalExpr(effective_opcode, ce, xsink);
                            used_operands = true;
                        }
                    }
                    if (!used_operands && arg_list) {
                        arg_list->deref(xsink);
                    }
                }
                if (!used_operands) {
                    // For VarRefNewObjectNode, instantiate the local variable before AST evaluation
                    // so that thread_find_lvar can find it during lvalue assignment
                    if (auto* var_new_obj = dynamic_cast<const VarRefNewObjectNode*>(
                            call_expr.getInternalNode())) {
                        qore_var_t vtype = var_new_obj->getType();
                        // Check all local variable types (including closure-use variants)
                        if ((vtype == VT_LOCAL || vtype == VT_CLOSURE || vtype == VT_LOCAL_TS)
                                && var_new_obj->ref.id) {
                            ensureLocalInstantiated(var_new_obj->ref.id, instantiated_locals, pre_instantiated,
                                    function_own_locals, &locally_uninstantiated);
                            // Track the result slot for this local's initialization
                            // so we can clean it up when UninstantiateLocal is processed
                            local_init_slots[var_new_obj->ref.id] = inst->result.id;
                        }
                    }
                    res = QoreIRInterpreter::evalExpr(effective_opcode, call_expr, xsink);
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                cleanupLocalCaches();
                setValueSlot(values, inst->result.id, res, xsink);
                if (res.hasNode()) {
                    cleanup.push_back(inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::CallMethodDirect: {
                // Direct method call for devirtualized final class methods
                auto* direct_inst = static_cast<QoreIRCallMethodDirectInstruction*>(inst);
                const QoreMethod* method = direct_inst->method;

                // Get self object from runtime stack
                QoreObject* self = runtime_get_stack_object();
                if (!self) {
                    if (xsink) {
                        xsink->raiseException("IR-INTERPRETER-ERROR",
                            "no self object in direct method call");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }

                // Build argument list from operands
                ReferenceHolder<QoreListNode> arg_list(
                    direct_inst->operands.empty() ? nullptr
                        : new QoreListNode(autoTypeInfo), xsink);
                for (const auto& operand : direct_inst->operands) {
                    QoreValue arg_val = getIRValue(values, operand);
                    if (arg_val.hasNode()) {
                        arg_val.refSelf();
                    }
                    arg_list->push(arg_val, xsink);
                }

                // Get runtime config and call the method directly
                RuntimeConfig& rc = rc_get_current_ref();
                QoreValue res = qore_method_private::eval(*method, xsink, rc, self, *arg_list);

                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                setValueSlot(values, direct_inst->result.id, res, xsink);
                if (res.hasNode()) {
                    cleanup.push_back(direct_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::InvokeMethodDirect: {
                // Direct method call with exception routing for devirtualized final class methods
                auto* invoke_inst = static_cast<QoreIRInvokeMethodDirectInstruction*>(inst);
                const QoreMethod* method = invoke_inst->method;

                // Get self object from runtime stack
                QoreObject* self = runtime_get_stack_object();
                if (!self) {
                    if (xsink) {
                        xsink->raiseException("IR-INTERPRETER-ERROR",
                            "no self object in invoke method direct");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }

                // Build argument list from operands
                ReferenceHolder<QoreListNode> arg_list(
                    invoke_inst->operands.empty() ? nullptr
                        : new QoreListNode(autoTypeInfo), xsink);
                for (const auto& operand : invoke_inst->operands) {
                    QoreValue arg_val = getIRValue(values, operand);
                    if (arg_val.hasNode()) {
                        arg_val.refSelf();
                    }
                    arg_list->push(arg_val, xsink);
                }

                // Get runtime config and call the method directly
                RuntimeConfig& rc = rc_get_current_ref();
                QoreValue res = qore_method_private::eval(*method, xsink, rc, self, *arg_list);

                // Invalidate all variable caches after method call - the call may have modified
                // globals, thread-locals, or closure variables, and the slot cache must be cleared
                cleanupLocalCaches();

                if (xsink && *xsink) {
                    // On exception, branch to exception target
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    prev_block = block;
                    block = invoke_inst->exception_target;
                    ip = 0;
                    break;
                }
                setValueSlot(values, invoke_inst->result.id, res, xsink);
                if (res.hasNode()) {
                    cleanup.push_back(invoke_inst->result.id);
                }
                // On success, branch to normal target
                prev_block = block;
                block = invoke_inst->normal_target;
                ip = 0;
                break;
            }
            case QoreIROpcode::DotEvalMethodDirect: {
                // Direct dot-eval method call with pre-evaluated base + args
                auto* direct_inst = static_cast<QoreIRDotEvalMethodDirectInstruction*>(inst);

                // operands[0] is the base expression, operands[1..n-1] are arguments
                QoreValue base = getIRValue(values, direct_inst->operands[0]);

                // Build NaN-boxed args array from operands[1..n-1]
                int nargs = static_cast<int>(direct_inst->operands.size()) - 1;
                std::vector<uint64_t> nanboxed_args(nargs > 0 ? nargs : 0);
                for (int i = 0; i < nargs; ++i) {
                    nanboxed_args[i] = toBits(getIRValue(values, direct_inst->operands[i + 1]));
                }

                QoreValue res;
                if (direct_inst->pseudo) {
                    res = fromBits(qore_rt_dot_eval_pseudo_method_direct(
                        toBits(base), direct_inst->method, direct_inst->qc, direct_inst->variant,
                        nanboxed_args.data(), nargs, xsink));
                } else {
                    res = fromBits(qore_rt_dot_eval_method_direct(
                        toBits(base), direct_inst->method, direct_inst->qc, direct_inst->variant,
                        nanboxed_args.data(), nargs, xsink));
                }

                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                cleanupLocalCaches();
                setValueSlot(values, direct_inst->result.id, res, xsink);
                if (res.hasNode()) {
                    cleanup.push_back(direct_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::InvokeDotEvalMethodDirect: {
                // Direct dot-eval method call with exception routing
                auto* de_invoke_inst = static_cast<QoreIRInvokeDotEvalMethodDirectInstruction*>(inst);

                // operands[0] is the base expression, operands[1..n-1] are arguments
                QoreValue base = getIRValue(values, de_invoke_inst->operands[0]);

                // Build NaN-boxed args array from operands[1..n-1]
                int nargs = static_cast<int>(de_invoke_inst->operands.size()) - 1;
                std::vector<uint64_t> nanboxed_args(nargs > 0 ? nargs : 0);
                for (int i = 0; i < nargs; ++i) {
                    nanboxed_args[i] = toBits(getIRValue(values, de_invoke_inst->operands[i + 1]));
                }

                QoreValue res;
                if (de_invoke_inst->pseudo) {
                    res = fromBits(qore_rt_dot_eval_pseudo_method_direct(
                        toBits(base), de_invoke_inst->method, de_invoke_inst->qc, de_invoke_inst->variant,
                        nanboxed_args.data(), nargs, xsink));
                } else {
                    res = fromBits(qore_rt_dot_eval_method_direct(
                        toBits(base), de_invoke_inst->method, de_invoke_inst->qc, de_invoke_inst->variant,
                        nanboxed_args.data(), nargs, xsink));
                }

                // Invalidate all variable caches after method call - must include slot cache
                cleanupLocalCaches();

                if (xsink && *xsink) {
                    // On exception, branch to exception target
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    prev_block = block;
                    block = de_invoke_inst->exception_target;
                    ip = 0;
                    break;
                }
                setValueSlot(values, de_invoke_inst->result.id, res, xsink);
                if (res.hasNode()) {
                    cleanup.push_back(de_invoke_inst->result.id);
                }
                // On success, branch to normal target
                prev_block = block;
                block = de_invoke_inst->normal_target;
                ip = 0;
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
                cleanupStoredValues(locals, xsink);
                cleanupStoredValues(globals, xsink);
                cleanupStoredValues(threadlocals, xsink);
                cleanupStoredValues(closures, xsink);
                return false;
            }
            setValueSlot(values, expr_inst->result.id, res, xsink);
            if (res.hasNode()) {
                cleanup.push_back(expr_inst->result.id);
            }
            ++ip;
            break;
        }
        case QoreIROpcode::SwitchRegexMatch: {
            // Handle switch regex case match using CaseNodeRegex::matches()
            auto* regex_inst = static_cast<QoreIRSwitchRegexMatchInstruction*>(inst);
            QoreValue res;
            if (!regex_inst->operands.empty() && regex_inst->regex_case) {
                QoreValue switch_val = getIRValue(values, regex_inst->operands[0]);
                bool match = regex_inst->regex_case->matches(switch_val, xsink);
                res = QoreValue(match);
            } else {
                res = QoreValue(false);
            }
            if (xsink && *xsink) {
                executeOnBlockExitHandlers(on_block_exit_handlers, xsink);
                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                cleanupStoredValues(locals, xsink);
                cleanupStoredValues(globals, xsink);
                cleanupStoredValues(threadlocals, xsink);
                cleanupStoredValues(closures, xsink);
                return false;
            }
            setValueSlot(values, regex_inst->result.id, res, xsink);
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
                cleanupStoredValues(locals, xsink);
                cleanupStoredValues(globals, xsink);
                cleanupStoredValues(threadlocals, xsink);
                cleanupStoredValues(closures, xsink);
                return false;
            }
            setValueSlot(values, expr_inst->result.id, res, xsink);
            if (res.hasNode()) {
                cleanup.push_back(expr_inst->result.id);
            }
            ++ip;
            break;
        }
        // Lvalue-modifying AST opcodes: invalidate all caches BEFORE evalExpr()
        // because these operations modify variables in-place via LValueHelper.
        // The extra reference from the slot cache inflates refcounts, causing
        // ensureUnique() to trigger COW — the modification is applied to the
        // copy while the cache retains the stale original.
        // See design/lvalue-loads-in-ir.md "Slot Cache Invalidation Rule".
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
        case QoreIROpcode::ListAssignAny: {
                auto* expr_inst = static_cast<QoreIRExprInstruction*>(inst);
                // Invalidate all caches BEFORE the lvalue operation
                cleanupStoredValues(locals, xsink);
                cleanupStoredValues(globals, xsink);
                cleanupStoredValues(threadlocals, xsink);
                cleanupStoredValues(closures, xsink);
                for (size_t i = 0; i < locals_slot_cache.size(); ++i) {
                    locals_slot_cache[i].discard(xsink);
                    locals_slot_cache[i] = QoreValue();
                }
                QoreValue res = QoreIRInterpreter::evalExpr(inst->opcode, expr_inst->expr, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                setValueSlot(values, inst->result.id, res, xsink);
                if (res.hasNode()) {
                    cleanup.push_back(inst->result.id);
                }
                ++ip;
                break;
            }
        // Non-modifying AST opcodes: no cache invalidation needed
        case QoreIROpcode::KeysAny:
        case QoreIROpcode::KeysList:
        case QoreIROpcode::KeysHash:
        case QoreIROpcode::InstanceOfBool:
        case QoreIROpcode::BackgroundInt:
        case QoreIROpcode::ElementsAny:
        case QoreIROpcode::ElementsInt: {
                auto* expr_inst = static_cast<QoreIRExprInstruction*>(inst);
                QoreValue res = QoreIRInterpreter::evalExpr(inst->opcode, expr_inst->expr, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    return false;
                }
                setValueSlot(values, inst->result.id, res, xsink);
                if (res.hasNode()) {
                    cleanup.push_back(inst->result.id);
                }
                ++ip;
                break;
            }
        // InvokeSimError: error propagation call that can modify variables via evalExpr()
        // Must invalidate all caches after the call, both on success and error paths
        case QoreIROpcode::InvokeSimError: {
                auto* expr_inst = static_cast<QoreIRExprInstruction*>(inst);
                QoreValue res = QoreIRInterpreter::evalExpr(inst->opcode, expr_inst->expr, xsink);

                // Invalidate all variable caches after the AST call - it may have modified
                // locals, globals, thread-locals, or closure variables. Must include slot cache.
                cleanupLocalCaches();

                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    return false;
                }
                setValueSlot(values, inst->result.id, res, xsink);
                if (res.hasNode()) {
                    cleanup.push_back(inst->result.id);
                }
                ++ip;
                break;
            }
        // Cast opcodes: native cast with pre-evaluated inner value (operand[0])
        case QoreIROpcode::CastAny:
        case QoreIROpcode::CastList:
        case QoreIROpcode::CastHash:
        case QoreIROpcode::CastObject:
        case QoreIROpcode::CastEnum: {
                auto* expr_inst = static_cast<QoreIRExprInstruction*>(inst);
                QoreValue inner = getIRValue(values, inst->operands[0]);
                auto* cast_node = dynamic_cast<const QoreCastOperatorNode*>(
                    expr_inst->expr.getInternalNode());
                QoreValue res;
                if (cast_node) {
                    res = cast_node->castValue(inner, xsink);
                } else {
                    // Fallback for unresolved CastAny (QoreParseCastOperatorNode)
                    res = evalExprNode(expr_inst->expr, xsink);
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                // Cast opcodes are now native — no cache invalidation needed
                setValueSlot(values, inst->result.id, res, xsink);
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
                    cleanupLocalCaches();
                    return false;
                }
                // DotEval can execute AST code (method calls) that modifies variables, so cache invalidation is needed
                cleanupLocalCaches();
                setValueSlot(values, expr_inst->result.id, res, xsink);
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
                    // Set the runtime location to the throw statement's location so
                    // that the exception gets the correct source file/line info
                    QoreProgramOptionalLocationHelper loc_helper(inst->loc);
                    // throw args are always a list: (err, desc[, arg])
                    // use the same raiseException(list) API as the AST path
                    if (arg.getType() == NT_LIST) {
                        xsink->raiseException(arg.get<const QoreListNode>());
                    } else {
                        QoreValue owned_arg = arg.hasNode() ? arg.refSelf() : arg;
                        xsink->raiseExceptionArg("IR-EXEC-THROW", owned_arg, "throw");
                    }
                }
                if (debug_active && xsink && *xsink) {
                    tlpd->dbgException(nullptr, xsink);
                }
                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                if (throw_inst->exception_target) {
                    prev_block = block;
                    block = throw_inst->exception_target;
                    ip = 0;
                    break;
                }
                // No exception target: fire all scope exits after raising the exception.
                // The exception is now on xsink, so on_error handlers can access it
                // via CatchExceptionHelper for rethrow support.
                if (debug_active) {
                    tlpd->dbgFunctionExit(statements, return_value, xsink);
                }
                fireScopeExits();
                cleanupStoredValues(locals, xsink);
                cleanupStoredValues(globals, xsink);
                cleanupStoredValues(threadlocals, xsink);
                cleanupStoredValues(closures, xsink);
                return false;
            }
            case QoreIROpcode::Rethrow: {
                auto* rethrow_inst = static_cast<QoreIRThrowInstruction*>(inst);
                if (!xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, xsink);
                    cleanupStoredValues(globals, xsink);
                    cleanupStoredValues(threadlocals, xsink);
                    cleanupStoredValues(closures, xsink);
                    return false;
                }
                // Only access catch context for real rethrows (from Qore rethrow
                // statements inside catch/on_error blocks).  Synthetic rethrows
                // (e.g. foreach reference cleanup) just propagate the exception
                // already on xsink without touching td->catchException.
                if (!rethrow_inst->synthetic) {
                    QoreException* ex = catch_get_exception();
                    if (!ex) {
                        xsink->raiseException("IR-EXEC-ERROR", "rethrow without exception");
                    } else {
                        if (!inst->operands.empty()) {
                            // Rethrow with args: modify the exception's err/desc/arg
                            // before rethrowing (matches AST RethrowStatement::execImpl)
                            QoreValue arg = getIRValue(values, inst->operands[0]);
                            if (arg.getType() == NT_LIST) {
                                ex = ex->replaceTop(*arg.get<const QoreListNode>(), *xsink);
                            }
                        }
                        qore_es_private::get(*xsink)->rethrow(ex);
                    }
                }
                if (debug_active && *xsink) {
                    tlpd->dbgException(nullptr, xsink);
                }
                // Clean up ALL active catch scopes (catch_depth levels)
                for (int i = 0; i < rethrow_inst->catch_depth; ++i) {
                    if (!catch_exception_stack.empty()) {
                        auto entry = catch_exception_stack.back();
                        catch_exception_stack.pop_back();
                        if (entry.caught) {
                            catch_swap_exception(entry.saved);
                            entry.caught->del(xsink);
                        }
                    }
                }
                // Branch to outer landing pad if inside nested try/catch
                if (rethrow_inst->exception_target) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    prev_block = block;
                    block = rethrow_inst->exception_target;
                    ip = 0;
                    break;
                }
                // No exception target: fire all scope exits after rethrowing.
                // The rethrown exception is now on xsink, so on_error handlers
                // can access it via CatchExceptionHelper.
                if (debug_active) {
                    tlpd->dbgFunctionExit(statements, return_value, xsink);
                }
                fireScopeExits();
                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                cleanupStoredValues(locals, xsink);
                cleanupStoredValues(globals, xsink);
                cleanupStoredValues(threadlocals, xsink);
                cleanupStoredValues(closures, xsink);
                return false;
            }
            case QoreIROpcode::Return: {
                auto* ret = static_cast<QoreIRReturnInstruction*>(inst);
                if (ret->has_value) {
                    QoreValue val = getIRValue(values, ret->value);
                    removeCleanupEntry(cleanup, ret->value.id);
                    if (val.hasNode()) {
                        return_value = val.refSelf();
                        if (ret->value.id < values.size()) {
                            values[ret->value.id].discard(xsink);
                            values[ret->value.id] = QoreValue();  // Set to NOTHING instead of erase
                        }
                    } else {
                        return_value = val;
                    }
                } else {
                    return_value = QoreValue();
                }
                if (debug_active) {
                    tlpd->dbgFunctionExit(statements, return_value, xsink);
                }
                executeOnBlockExitHandlers(on_block_exit_handlers, xsink);
                cleanupValues(values, cleanup, xsink, false, cleanup_log);
                cleanupStoredValues(locals, xsink);
                cleanupStoredValues(globals, xsink);
                cleanupStoredValues(threadlocals, xsink);
                cleanupStoredValues(closures, xsink);
                return true;
            }
            case QoreIROpcode::ReturnNothing: {
                return_value = QoreValue();
                if (debug_active) {
                    tlpd->dbgFunctionExit(statements, return_value, xsink);
                }
                executeOnBlockExitHandlers(on_block_exit_handlers, xsink);
                cleanupValues(values, cleanup, xsink, false, cleanup_log);
                cleanupStoredValues(locals, xsink);
                cleanupStoredValues(globals, xsink);
                cleanupStoredValues(threadlocals, xsink);
                cleanupStoredValues(closures, xsink);
                return true;
            }
            default:
                if (xsink) {
                    std::string msg = "unsupported opcode in executor: ";
                    msg += std::to_string(static_cast<int>(inst->opcode));
                    xsink->raiseException("IR-EXEC-ERROR", msg.c_str());
                }
                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                cleanupStoredValues(locals, xsink);
                cleanupStoredValues(globals, xsink);
                cleanupStoredValues(threadlocals, xsink);
                cleanupStoredValues(closures, xsink);
                return false;
        }

    }

    if (xsink) {
        xsink->raiseException("IR-EXEC-ERROR", "executor reached invalid state");
    }
    cleanupValues(values, cleanup, xsink, true, cleanup_log);
    cleanupStoredValues(locals, xsink);
    cleanupStoredValues(globals, xsink);
    cleanupStoredValues(threadlocals, xsink);
    cleanupStoredValues(closures, xsink);
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
        case QoreIROpcode::ExistsAny:
        case QoreIROpcode::ExistsBool:
            return QoreValue(!value.isNothing());
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
        case QoreIROpcode::AddString: {
            // Typed string concatenation - both operands are known to be strings
            const QoreStringNode* ls = left.getType() == NT_STRING
                ? left.get<const QoreStringNode>() : nullptr;
            const QoreStringNode* rs = right.getType() == NT_STRING
                ? right.get<const QoreStringNode>() : nullptr;
            if (!ls && !rs) {
                return QoreValue();  // Both NOTHING
            }
            if (!ls) {
                return QoreValue(rs->stringRefSelf());  // Copy right
            }
            if (!rs) {
                return QoreValue(ls->stringRefSelf());  // Copy left
            }
            QoreStringNode* result = new QoreStringNode(*ls);
            result->concat(rs, xsink);
            if (*xsink) {
                result->deref();
                return QoreValue();
            }
            return QoreValue(result);
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
        case QoreIROpcode::AddNumber: {
            const QoreNumberNode* ln = left.get<const QoreNumberNode>();
            const QoreNumberNode* rn = right.get<const QoreNumberNode>();
            return ln && rn ? QoreValue(ln->doPlus(*rn)) : QoreValue();
        }
        case QoreIROpcode::SubNumber: {
            const QoreNumberNode* ln = left.get<const QoreNumberNode>();
            const QoreNumberNode* rn = right.get<const QoreNumberNode>();
            return ln && rn ? QoreValue(ln->doMinus(*rn)) : QoreValue();
        }
        case QoreIROpcode::MulNumber: {
            const QoreNumberNode* ln = left.get<const QoreNumberNode>();
            const QoreNumberNode* rn = right.get<const QoreNumberNode>();
            return ln && rn ? QoreValue(ln->doMultiply(*rn)) : QoreValue();
        }
        case QoreIROpcode::DivNumber: {
            const QoreNumberNode* ln = left.get<const QoreNumberNode>();
            const QoreNumberNode* rn = right.get<const QoreNumberNode>();
            return ln && rn ? QoreValue(ln->doDivideBy(*rn, xsink)) : QoreValue();
        }
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
            ValueHolder node(QoreValue(new QoreBinaryAndOperatorNode(get_runtime_location(), left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::OrInt:
            return QoreValue(left.getAsBigInt() | right.getAsBigInt());
        case QoreIROpcode::OrAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreBinaryOrOperatorNode(get_runtime_location(), left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::XorInt:
            return QoreValue(left.getAsBigInt() ^ right.getAsBigInt());
        case QoreIROpcode::XorAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreBinaryXorOperatorNode(get_runtime_location(), left.refSelf(), right.refSelf())), xsink);
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
            ValueHolder node(QoreValue(new QoreBinaryAndOperatorNode(get_runtime_location(), left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::OrAssignInt:
            return QoreValue(left.getAsBigInt() | right.getAsBigInt());
        case QoreIROpcode::OrAssignAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreBinaryOrOperatorNode(get_runtime_location(), left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::XorAssignInt:
            return QoreValue(left.getAsBigInt() ^ right.getAsBigInt());
        case QoreIROpcode::XorAssignAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreBinaryXorOperatorNode(get_runtime_location(), left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::ShlInt:
            return QoreValue(left.getAsBigInt() << right.getAsBigInt());
        case QoreIROpcode::ShlAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreShiftLeftOperatorNode(get_runtime_location(), left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::ShrInt:
            return QoreValue(left.getAsBigInt() >> right.getAsBigInt());
        case QoreIROpcode::ShrAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreShiftRightOperatorNode(get_runtime_location(), left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::ShlAssignInt:
            return QoreValue(left.getAsBigInt() << right.getAsBigInt());
        case QoreIROpcode::ShlAssignAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreShiftLeftEqualsOperatorNode(get_runtime_location(), left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::ShrAssignInt:
            return QoreValue(left.getAsBigInt() >> right.getAsBigInt());
        case QoreIROpcode::ShrAssignAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreShiftRightEqualsOperatorNode(get_runtime_location(), left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::FoldlAny:
        case QoreIROpcode::FoldlInt:
        case QoreIROpcode::FoldlFloat: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreFoldlOperatorNode(get_runtime_location(), left.refSelf(), right.refSelf())), xsink);
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
        // Specialized foldr operations — sum/prod are commutative so same as foldl
        // diff uses reverse iteration: list[n-1] - list[n-2] - ... - list[0]
        case QoreIROpcode::FoldrSumInt: {
            if (left.getType() != NT_LIST) {
                return right.isNothing() ? QoreValue(0ll) : QoreValue(right.getAsBigInt());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return right.isNothing() ? QoreValue(0ll) : QoreValue(right.getAsBigInt());
            }
            int64_t result = right.isNothing() ? 0ll : right.getAsBigInt();
            for (size_t i = 0; i < sz; ++i) {
                result += l->retrieveEntry(i).getAsBigInt();
            }
            return QoreValue(result);
        }
        case QoreIROpcode::FoldrSumFloat: {
            if (left.getType() != NT_LIST) {
                return right.isNothing() ? QoreValue(0.0) : QoreValue(right.getAsFloat());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return right.isNothing() ? QoreValue(0.0) : QoreValue(right.getAsFloat());
            }
            double result = right.isNothing() ? 0.0 : right.getAsFloat();
            for (size_t i = 0; i < sz; ++i) {
                result += l->retrieveEntry(i).getAsFloat();
            }
            return QoreValue(result);
        }
        case QoreIROpcode::FoldrProdInt: {
            if (left.getType() != NT_LIST) {
                return right.isNothing() ? QoreValue(1ll) : QoreValue(right.getAsBigInt());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return right.isNothing() ? QoreValue(1ll) : QoreValue(right.getAsBigInt());
            }
            int64_t result = right.isNothing() ? 1ll : right.getAsBigInt();
            for (size_t i = 0; i < sz; ++i) {
                result *= l->retrieveEntry(i).getAsBigInt();
            }
            return QoreValue(result);
        }
        case QoreIROpcode::FoldrProdFloat: {
            if (left.getType() != NT_LIST) {
                return right.isNothing() ? QoreValue(1.0) : QoreValue(right.getAsFloat());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return right.isNothing() ? QoreValue(1.0) : QoreValue(right.getAsFloat());
            }
            double result = right.isNothing() ? 1.0 : right.getAsFloat();
            for (size_t i = 0; i < sz; ++i) {
                result *= l->retrieveEntry(i).getAsFloat();
            }
            return QoreValue(result);
        }
        case QoreIROpcode::FoldrDiffInt: {
            // foldr $1 - $2: list[n-1] - list[n-2] - ... - list[0]
            if (left.getType() != NT_LIST) {
                return right.isNothing() ? QoreValue(0ll) : QoreValue(right.getAsBigInt());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return right.isNothing() ? QoreValue(0ll) : QoreValue(right.getAsBigInt());
            }
            // Start from last element and subtract backwards
            int64_t result = l->retrieveEntry(sz - 1).getAsBigInt();
            for (size_t i = sz - 1; i > 0; --i) {
                result -= l->retrieveEntry(i - 1).getAsBigInt();
            }
            return QoreValue(result);
        }
        case QoreIROpcode::FoldrDiffFloat: {
            // foldr $1 - $2: list[n-1] - list[n-2] - ... - list[0]
            if (left.getType() != NT_LIST) {
                return right.isNothing() ? QoreValue(0.0) : QoreValue(right.getAsFloat());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return right.isNothing() ? QoreValue(0.0) : QoreValue(right.getAsFloat());
            }
            // Start from last element and subtract backwards
            double result = l->retrieveEntry(sz - 1).getAsFloat();
            for (size_t i = sz - 1; i > 0; --i) {
                result -= l->retrieveEntry(i - 1).getAsFloat();
            }
            return QoreValue(result);
        }
        case QoreIROpcode::FoldrMinInt: {
            if (left.getType() != NT_LIST) {
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsBigInt());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsBigInt());
            }
            int64_t result = l->retrieveEntry(0).getAsBigInt();
            for (size_t i = 1; i < sz; ++i) {
                int64_t val = l->retrieveEntry(i).getAsBigInt();
                if (val < result) {
                    result = val;
                }
            }
            return QoreValue(result);
        }
        case QoreIROpcode::FoldrMinFloat: {
            if (left.getType() != NT_LIST) {
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsFloat());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsFloat());
            }
            double result = l->retrieveEntry(0).getAsFloat();
            for (size_t i = 1; i < sz; ++i) {
                double val = l->retrieveEntry(i).getAsFloat();
                if (val < result) {
                    result = val;
                }
            }
            return QoreValue(result);
        }
        case QoreIROpcode::FoldrMaxInt: {
            if (left.getType() != NT_LIST) {
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsBigInt());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsBigInt());
            }
            int64_t result = l->retrieveEntry(0).getAsBigInt();
            for (size_t i = 1; i < sz; ++i) {
                int64_t val = l->retrieveEntry(i).getAsBigInt();
                if (val > result) {
                    result = val;
                }
            }
            return QoreValue(result);
        }
        case QoreIROpcode::FoldrMaxFloat: {
            if (left.getType() != NT_LIST) {
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsFloat());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsFloat());
            }
            double result = l->retrieveEntry(0).getAsFloat();
            for (size_t i = 1; i < sz; ++i) {
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
            ValueHolder node(QoreValue(new QoreFoldrOperatorNode(get_runtime_location(), left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::MapAny:
        case QoreIROpcode::MapInt:
        case QoreIROpcode::MapFloat: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreMapOperatorNode(get_runtime_location(), left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        // Optimized map operations with native loops
        case QoreIROpcode::MapScaleInt: {
            // left = list, right = scale factor
            if (left.getType() != NT_LIST) {
                return QoreValue();
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
                return QoreValue();
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
                return QoreValue();
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
                return QoreValue();
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
                return QoreValue();
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
                return QoreValue();
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
            ValueHolder node(QoreValue(new QoreSelectOperatorNode(get_runtime_location(), left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        // Optimized select operations with native loops
        case QoreIROpcode::SelectPositiveInt: {
            // left = list (filter $1 > 0)
            if (left.getType() != NT_LIST) {
                return QoreValue();
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
                return QoreValue();
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
                return QoreValue();
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
                return QoreValue();
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
                return QoreValue();
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
                return QoreValue();
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
                return QoreValue();
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
                return QoreValue();
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
                return QoreValue();
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
                return QoreValue();
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
        // Fused map+foldl operations (single pass, no intermediate list)
        case QoreIROpcode::FusedMapFoldlSumScaleInt: {
            // left = list, right = scale factor
            // Sum of (list[i] * scale)
            if (left.getType() != NT_LIST) {
                return QoreValue((int64_t)0);
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return QoreValue((int64_t)0);
            }
            int64_t scale = right.getAsBigInt();
            int64_t result = 0;
            for (size_t i = 0; i < sz; ++i) {
                result += l->retrieveEntry(i).getAsBigInt() * scale;
            }
            return QoreValue(result);
        }
        case QoreIROpcode::FusedMapFoldlSumScaleFloat: {
            if (left.getType() != NT_LIST) {
                return QoreValue(0.0);
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return QoreValue(0.0);
            }
            double scale = right.getAsFloat();
            double result = 0.0;
            for (size_t i = 0; i < sz; ++i) {
                result += l->retrieveEntry(i).getAsFloat() * scale;
            }
            return QoreValue(result);
        }
        case QoreIROpcode::FusedMapFoldlSumSquareInt: {
            // left = list, right = unused
            // Sum of (list[i]^2)
            if (left.getType() != NT_LIST) {
                return QoreValue((int64_t)0);
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return QoreValue((int64_t)0);
            }
            int64_t result = 0;
            for (size_t i = 0; i < sz; ++i) {
                int64_t val = l->retrieveEntry(i).getAsBigInt();
                result += val * val;
            }
            return QoreValue(result);
        }
        case QoreIROpcode::FusedMapFoldlSumSquareFloat: {
            if (left.getType() != NT_LIST) {
                return QoreValue(0.0);
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return QoreValue(0.0);
            }
            double result = 0.0;
            for (size_t i = 0; i < sz; ++i) {
                double val = l->retrieveEntry(i).getAsFloat();
                result += val * val;
            }
            return QoreValue(result);
        }
        case QoreIROpcode::FusedMapFoldlProdScaleInt: {
            // left = list, right = scale factor
            // Product of (list[i] * scale)
            if (left.getType() != NT_LIST) {
                return QoreValue((int64_t)1);  // Identity for product
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return QoreValue((int64_t)1);  // Identity for product
            }
            int64_t scale = right.getAsBigInt();
            int64_t result = 1;
            for (size_t i = 0; i < sz; ++i) {
                result *= l->retrieveEntry(i).getAsBigInt() * scale;
            }
            return QoreValue(result);
        }
        case QoreIROpcode::FusedMapFoldlProdScaleFloat: {
            if (left.getType() != NT_LIST) {
                return QoreValue(1.0);  // Identity for product
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return QoreValue(1.0);  // Identity for product
            }
            double scale = right.getAsFloat();
            double result = 1.0;
            for (size_t i = 0; i < sz; ++i) {
                result *= l->retrieveEntry(i).getAsFloat() * scale;
            }
            return QoreValue(result);
        }
        case QoreIROpcode::RangeAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreRangeOperatorNode(get_runtime_location(), left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::RangeInt:
        case QoreIROpcode::RangeFloat:
        case QoreIROpcode::RangeDate: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreRangeOperatorNode(get_runtime_location(), left.refSelf(), right.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::EqInt:
        case QoreIROpcode::EqFloat:
        case QoreIROpcode::EqString:
        case QoreIROpcode::EqAny:
        case QoreIROpcode::NeInt:
        case QoreIROpcode::NeFloat:
        case QoreIROpcode::NeString:
        case QoreIROpcode::NeAny:
        case QoreIROpcode::EqHard:
        case QoreIROpcode::NeHard:
        case QoreIROpcode::LtInt:
        case QoreIROpcode::LtFloat:
        case QoreIROpcode::LtString:
        case QoreIROpcode::LtAny:
        case QoreIROpcode::LeInt:
        case QoreIROpcode::LeFloat:
        case QoreIROpcode::LeString:
        case QoreIROpcode::LeAny:
        case QoreIROpcode::GtInt:
        case QoreIROpcode::GtFloat:
        case QoreIROpcode::GtString:
        case QoreIROpcode::GtAny:
        case QoreIROpcode::GeInt:
        case QoreIROpcode::GeFloat:
        case QoreIROpcode::GeString:
        case QoreIROpcode::GeAny:
        case QoreIROpcode::CmpInt:
        case QoreIROpcode::CmpFloat:
        case QoreIROpcode::CmpString:
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
            ValueHolder node(QoreValue(new QoreSquareBracketsRangeOperatorNode(get_runtime_location(),
                first.refSelf(), second.refSelf(), third.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::MapSelectAny:
        case QoreIROpcode::MapSelectList: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreMapSelectOperatorNode(get_runtime_location(),
                first.refSelf(), second.refSelf(), third.refSelf())), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::HashMapAny:
        case QoreIROpcode::HashMap: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreHashMapOperatorNode(get_runtime_location(),
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
            ValueHolder node(QoreValue(new QoreHashMapSelectOperatorNode(get_runtime_location(),
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

QoreValue QoreIRInterpreter::evalLValueStore(const QoreValue& lvalue, const QoreValue& value, ExceptionSink* xsink,
        bool weak) {
    LValueHelper helper(lvalue, xsink);
    if (!helper) {
        return QoreValue();
    }
    // refSelf() before passing to assign() - assign() takes ownership via
    // assignAssume()/takeNode(), but value is a borrowed reference from the
    // caller's values map; without the extra ref, both the variable and the
    // values map would think they own the same single reference
    if (helper.assign(value.refSelf(), "<lvalue>", true, weak)) {
        return QoreValue();
    }
    return value.refSelf();
}

QoreValue QoreIRInterpreter::evalLValueUnary(QoreIROpcode op, const QoreValue& lvalue, ExceptionSink* xsink) {
    bool needs_deref = true;
    QoreValue lvalue_ref = lvalue.refSelf();
    switch (op) {
        case QoreIROpcode::PreIncLValue: {
            ValueHolder node(QoreValue(new QorePreIncrementOperatorNode(get_runtime_location(), lvalue_ref)), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::PreDecLValue: {
            ValueHolder node(QoreValue(new QorePreDecrementOperatorNode(get_runtime_location(), lvalue_ref)), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::PostIncLValue: {
            ValueHolder node(QoreValue(new QorePostIncrementOperatorNode(get_runtime_location(), lvalue_ref)), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::PostDecLValue: {
            ValueHolder node(QoreValue(new QorePostDecrementOperatorNode(get_runtime_location(), lvalue_ref)), xsink);
            return evalAndRef(*node, xsink);
        }
        case QoreIROpcode::ShiftLValue: {
            ValueHolder node(QoreValue(new QoreShiftOperatorNode(get_runtime_location(), lvalue_ref)), xsink);
            return evalAndRef(*node, xsink);
        }
        default:
            break;
    }
    if (xsink) {
        xsink->raiseException("IR-INTERPRETER-ERROR", "unsupported lvalue unary opcode: %d", (int)op);
    }
    return QoreValue();
}

// Direct compound assignment helpers — avoid allocating temporary AST operator nodes
static QoreValue evalPlusEquals(const QoreValue& lvalue, const QoreValue& right, ExceptionSink* xsink) {
    // values requiring dereferencing must be dereferenced outside the lock
    SafeDerefHelper sdh(xsink);

    // get ptr to current value (lvalue is locked for the scope of the LValueHelper object)
    LValueHelper v(lvalue, xsink);
    if (!v) {
        return QoreValue();
    }

    qore_type_t vtype = v.getType();

    if (vtype == NT_NOTHING) {
        const QoreTypeInfo* typeInfo = v.getTypeInfo();
        if (QoreTypeInfo::hasDefaultValue(typeInfo)) {
            if (v.assign(QoreTypeInfo::getDefaultQoreValue(typeInfo), "<lvalue for += operator>")) {
                return QoreValue();
            }
            vtype = v.getType();
        } else if (QoreTypeInfo::isListType(typeInfo)) {
            if (v.assign(new QoreListNode(QoreTypeInfo::getReturnComplexListOrNothing(typeInfo)),
                    "<lvalue for += operator>")) {
                return QoreValue();
            }
            vtype = v.getType();
        } else if (right.isNothing()) {
            return QoreValue();
        } else if (QoreTypeInfo::isHashType(typeInfo)) {
            if (v.assign(new QoreHashNode(QoreTypeInfo::getReturnComplexHashOrNothing(typeInfo)),
                    "<lvalue for += operator>")) {
                return QoreValue();
            }
            vtype = v.getType();
        } else {
            // assign rhs to lhs
            if (v.assign(right.refSelf(), "<lvalue for += operator>")) {
                return QoreValue();
            }
            return v.getReferencedValue();
        }
    }

    if (vtype == NT_LIST) {
        v.ensureUnique();
        QoreListNode* l = v.getValue().get<QoreListNode>();
        if (right.getType() == NT_LIST) {
            l->merge(right.get<const QoreListNode>(), xsink);
        } else {
            l->push(right.refSelf(), xsink);
        }
    } else if (vtype == NT_HASH) {
        if (right.getType() == NT_HASH) {
            v.ensureUnique();
            qore_hash_private::get(*v.getValue().get<QoreHashNode>())
                ->merge(*qore_hash_private::get(*right.get<const QoreHashNode>()), sdh, xsink);
        } else if (right.getType() == NT_OBJECT) {
            v.ensureUnique();
            qore_object_private::get(*right.get<QoreObject>())->mergeDataToHash(
                v.getValue().get<QoreHashNode>(), sdh, xsink);
        }
    } else if (vtype == NT_OBJECT) {
        QoreObject* o = v.getValue().get<QoreObject>();
        qore_object_private::get(*o)->plusEquals(right.getInternalNode(), v.getAutoVLock(), sdh, xsink);
    } else if (vtype == NT_STRING) {
        if (!right.isNullOrNothing()) {
            QoreStringValueHelper str(right);
            v.ensureUnique();
            QoreStringNode* vs = v.getValue().get<QoreStringNode>();
            vs->concat(*str, xsink);
        }
    } else if (vtype == NT_NUMBER) {
        v.plusEqualsNumber(right, "<+= operator>");
    } else if (vtype == NT_FLOAT) {
        v.plusEqualsFloat(right.getAsFloat());
    } else if (vtype == NT_DATE) {
        if (!right.isNullOrNothing()) {
            DateTime date(right);
            v.assign(v.getValue().get<DateTimeNode>()->add(date), "<lvalue for += operator>");
        }
    } else if (vtype == NT_BINARY) {
        if (!right.isNullOrNothing()) {
            v.ensureUnique();
            BinaryNode* b = v.getValue().get<BinaryNode>();
            if (right.getType() == NT_BINARY) {
                const BinaryNode* arg = right.get<const BinaryNode>();
                b->append(arg);
            } else {
                QoreStringNodeValueHelper str(right);
                if (str->strlen()) {
                    b->append(str->getBuffer(), str->strlen());
                }
            }
        }
    } else {
        // if the lvalue is a timeout, then convert any date/time value as if it were a timeout
        if (right.getType() == NT_DATE && QoreTypeInfo::equal(v.getTypeInfo(), timeoutTypeInfo)) {
            int64 ms = right.get<const DateTimeNode>()->getRelativeMilliseconds();
            return v.plusEqualsBigInt(ms);
        }
        // do integer plus-equals
        v.plusEqualsBigInt(right.getAsBigInt());
    }

    if (*xsink) {
        return QoreValue();
    }
    return v.getReferencedValue();
}

static QoreValue evalMinusEquals(const QoreValue& lvalue, const QoreValue& right, ExceptionSink* xsink) {
    LValueHelper v(lvalue, xsink);
    if (!v) {
        return QoreValue();
    }

    if (right.isNothing()) {
        return v.getReferencedValue();
    }

    qore_type_t vtype = v.getType();

    if (vtype == NT_NOTHING) {
        const QoreTypeInfo* typeInfo = v.getTypeInfo();
        if (QoreTypeInfo::isHashType(typeInfo)) {
            return QoreValue();
        } else if (QoreTypeInfo::hasDefaultValue(typeInfo)) {
            if (v.assign(QoreTypeInfo::getDefaultQoreValue(typeInfo))) {
                return QoreValue();
            }
            vtype = v.getType();
        } else {
            if (right.getType() == NT_FLOAT) {
                v.assign(-right.getAsFloat());
            } else if (right.getType() == NT_NUMBER) {
                const QoreNumberNode* num = right.get<const QoreNumberNode>();
                v.assign(num->negate());
            } else {
                v.assign(-right.getAsBigInt());
            }
            if (*xsink) {
                return QoreValue();
            }
            return v.getReferencedValue();
        }
    }

    if (vtype == NT_FLOAT) {
        return v.minusEqualsFloat(right.getAsFloat());
    } else if (vtype == NT_NUMBER) {
        v.minusEqualsNumber(right, "<-= operator>");
    } else if (vtype == NT_DATE) {
        if (right.getType() == NT_DATE) {
            v.assign(v.getValue().get<DateTimeNode>()->subtractBy(*right.get<const DateTimeNode>()));
        } else {
            DateTime date(right);
            v.assign(v.getValue().get<DateTimeNode>()->subtractBy(date));
        }
    } else if (vtype == NT_HASH) {
        if (right.getType() != NT_HASH && right.getType() != NT_OBJECT) {
            v.ensureUnique();
            QoreHashNode* vh = v.getValue().get<QoreHashNode>();

            const QoreListNode* nrl = (right.getType() == NT_LIST)
                ? right.get<const QoreListNode>()
                : nullptr;
            if (nrl && nrl->size()) {
                ConstListIterator li(nrl);
                while (li.next()) {
                    QoreStringValueHelper val(li.getValue());
                    vh->removeKey(*val, xsink);
                    if (*xsink) {
                        return QoreValue();
                    }
                }
            } else {
                QoreStringValueHelper str(right);
                vh->removeKey(*str, xsink);
            }
        }
    } else if (vtype == NT_OBJECT) {
        if (right.getType() != NT_HASH && right.getType() != NT_OBJECT) {
            QoreObject* o = v.getValue().get<QoreObject>();

            const QoreListNode* nrl = (right.getType() == NT_LIST)
                ? right.get<const QoreListNode>()
                : nullptr;
            if (nrl && nrl->size()) {
                ConstListIterator li(nrl);
                while (li.next()) {
                    QoreStringValueHelper val(li.getValue());
                    o->removeMember(*val, xsink);
                    if (*xsink) {
                        return QoreValue();
                    }
                }
            } else {
                QoreStringValueHelper str(right);
                o->removeMember(*str, xsink);
            }
        }
    } else {
        // if the lvalue is a timeout, convert date/time value as timeout
        if (right.getType() == NT_DATE && QoreTypeInfo::equal(v.getTypeInfo(), timeoutTypeInfo)) {
            int64 ms = right.get<const DateTimeNode>()->getRelativeMilliseconds();
            return v.minusEqualsBigInt(ms);
        }
        // do integer minus-equals
        return v.minusEqualsBigInt(right.getAsBigInt());
    }

    if (*xsink) {
        return QoreValue();
    }
    return v.getReferencedValue();
}

static QoreValue evalMultiplyEquals(const QoreValue& lvalue, const QoreValue& right, ExceptionSink* xsink) {
    LValueHelper v(lvalue, xsink);
    if (!v) {
        return QoreValue();
    }

    if (v.getType() == NT_NUMBER || right.getType() == NT_NUMBER) {
        v.multiplyEqualsNumber(right, "<*= operator>");
        if (!*xsink) {
            return v.getReferencedValue();
        }
        return QoreValue();
    }

    if (v.getType() == NT_FLOAT || right.getType() == NT_FLOAT) {
        return v.multiplyEqualsFloat(right.getAsFloat(), "<*= operator>");
    }

    int64 y = right.getAsBigInt();
    if (!v.getAsBigInt() || !y) {
        v.assign(0ll);
        return 0ll;
    }

    return v.multiplyEqualsBigInt(y, "<*= operator>");
}

static QoreValue evalDivideEquals(const QoreValue& lvalue, const QoreValue& right, ExceptionSink* xsink) {
    LValueHelper v(lvalue, xsink);
    if (!v) {
        return QoreValue();
    }

    if (right.getType() == NT_NUMBER || v.getType() == NT_NUMBER) {
        if (right.getAsFloat() == 0.0) {
            xsink->raiseException("DIVISION-BY-ZERO",
                "division by zero in arbitrary-precision numeric expression");
            return QoreValue();
        }
        v.divideEqualsNumber(right, "</= operator>");
    } else if (right.getType() == NT_FLOAT || v.getType() == NT_FLOAT) {
        double val = right.getAsFloat();
        if (val == 0.0) {
            xsink->raiseException("DIVISION-BY-ZERO", "division by zero in floating-point expression");
            return QoreValue();
        }
        return v.divideEqualsFloat(val, "</= operator>");
    } else {
        int64 val = right.getAsBigInt();
        if (!val) {
            xsink->raiseException("DIVISION-BY-ZERO", "division by zero in integer expression");
            return QoreValue();
        }
        return v.divideEqualsBigInt(val, "</= operator>");
    }

    if (*xsink) {
        return QoreValue();
    }
    return v.getReferencedValue();
}

static QoreValue evalUnshift(const QoreValue& lvalue, const QoreValue& right, ExceptionSink* xsink) {
    LValueHelper val(lvalue, xsink);
    if (!val) {
        return QoreValue();
    }

    // assign to a blank list if the lvalue has no value yet but is typed as a list or a softlist
    if (val.getType() == NT_NOTHING) {
        const QoreTypeInfo* vti = val.getTypeInfo();
        if (QoreTypeInfo::parseAcceptsReturns(vti, NT_LIST)) {
            const QoreTypeInfo* lti = vti == autoTypeInfo
                ? autoTypeInfo
                : QoreTypeInfo::getReturnComplexListOrNothing(vti);
            if (val.assign(new QoreListNode(lti))) {
                assert(*xsink);
                return QoreValue();
            }
        }
    }

    // value is not a list, so throw exception
    if (val.getType() != NT_LIST) {
        xsink->raiseException("UNSHIFT-ERROR", "the lvalue argument to unshift is type \"%s\"; "
            "expecting \"list\"", val.getTypeName());
        return QoreValue();
    }

    val.ensureUnique();
    QoreListNode* l = val.getValue().get<QoreListNode>();
    l->insert(right.refSelf(), xsink);

    if (*xsink) {
        return QoreValue();
    }
    return l->refSelf();
}

QoreValue QoreIRInterpreter::evalLValueBinary(QoreIROpcode op, const QoreValue& lvalue, const QoreValue& right,
        ExceptionSink* xsink) {
    switch (op) {
        case QoreIROpcode::AddAssignLValue:
            return evalPlusEquals(lvalue, right, xsink);
        case QoreIROpcode::SubAssignLValue:
            return evalMinusEquals(lvalue, right, xsink);
        case QoreIROpcode::MulAssignLValue:
            return evalMultiplyEquals(lvalue, right, xsink);
        case QoreIROpcode::DivAssignLValue:
            return evalDivideEquals(lvalue, right, xsink);
        case QoreIROpcode::ModAssignLValue: {
            int64 val = right.getAsBigInt();
            LValueHelper v(lvalue, xsink);
            if (!v) {
                return QoreValue();
            }
            if (!val) {
                v.assign(0ll, "<%= operator>");
                return 0ll;
            }
            return v.modulaEqualsBigInt(val, "<%= operator>");
        }
        case QoreIROpcode::AndAssignLValue: {
            int64 val = right.getAsBigInt();
            LValueHelper v(lvalue, xsink);
            if (!v) {
                return QoreValue();
            }
            return v.andEqualsBigInt(val, "<&= operator>");
        }
        case QoreIROpcode::OrAssignLValue: {
            int64 val = right.getAsBigInt();
            LValueHelper v(lvalue, xsink);
            if (!v) {
                return QoreValue();
            }
            return v.orEqualsBigInt(val, "<|= operator>");
        }
        case QoreIROpcode::XorAssignLValue: {
            int64 val = right.getAsBigInt();
            LValueHelper v(lvalue, xsink);
            if (!v) {
                return QoreValue();
            }
            return v.xorEqualsBigInt(val, "<^= operator>");
        }
        case QoreIROpcode::ShlAssignLValue: {
            int64 val = right.getAsBigInt();
            LValueHelper v(lvalue, xsink);
            if (!v) {
                return QoreValue();
            }
            return v.shiftLeftEqualsBigInt(val, "<<= operator>");
        }
        case QoreIROpcode::ShrAssignLValue: {
            int64 val = right.getAsBigInt();
            LValueHelper v(lvalue, xsink);
            if (!v) {
                return QoreValue();
            }
            return v.shiftRightEqualsBigInt(val, ">>= operator>");
        }
        case QoreIROpcode::UnshiftLValue:
            return evalUnshift(lvalue, right, xsink);
        default:
            break;
    }
    if (xsink) {
        xsink->raiseException("IR-INTERPRETER-ERROR", "unsupported lvalue binary opcode: %d", (int)op);
    }
    return QoreValue();
}

QoreValue QoreIRInterpreter::evalLValueTernary(QoreIROpcode op, const QoreValue& lvalue, const QoreValue& first,
        const QoreValue& second, const QoreValue& third, ExceptionSink* xsink) {
    bool needs_deref = true;
    QoreValue lvalue_ref = lvalue.refSelf();
    switch (op) {
        case QoreIROpcode::SpliceLValue: {
            ValueHolder node(QoreValue(new QoreSpliceOperatorNode(get_runtime_location(), lvalue_ref,
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
