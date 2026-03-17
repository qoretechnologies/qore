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
#include <qore/intern/ContextStatement.h>
#include <qore/intern/SummarizeStatement.h>
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
// Compile-time guard: forces review of interpreter dispatch when opcodes change.
// Update this value after verifying the new opcode is handled (or deliberately
// falls through to the default case).
static_assert(QORE_IR_MAX_OPCODE == 348,
    "New IR opcode added — review QoreIRInterpreter.cpp dispatch switch "
    "and update this assertion.  Also check QoreIRToLLVM.cpp.");
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
const VarRefNode* extractLValueBaseVarRef(const QoreValue& lvalue) {
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
#include <qore/intern/QoreInstanceOfOperatorNode.h>
#include <qore/intern/ScopedObjectCallNode.h>
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

// Overload for const QoreValue& (used in evalInvoke with const instruction)
static QoreValue evalAndRef(const QoreValue& val, ExceptionSink* xsink) {
    if (!val.hasNode()) {
        return val;
    }
    return evalAndRef(const_cast<AbstractQoreNode*>(val.getInternalNode()), xsink);
}

// Instrumentation: track AST node types evaluated via evalExprNode
// #define QORE_IR_PROFILE_AST_FALLBACK
#ifdef QORE_IR_PROFILE_AST_FALLBACK
#include <atomic>
#include <map>
#include <mutex>
static std::mutex ast_fallback_mutex;
static std::map<std::string, uint64_t> ast_fallback_counts;
static std::atomic<uint64_t> total_ast_fallback_count{0};

static void record_ast_fallback(const QoreValue& expr) {
    const char* type_name = "unknown";
    if (expr.hasNode()) {
        type_name = expr.getInternalNode()->getTypeName();
    }
    std::lock_guard<std::mutex> lock(ast_fallback_mutex);
    ast_fallback_counts[type_name]++;
    total_ast_fallback_count.fetch_add(1, std::memory_order_relaxed);
}

__attribute__((destructor))
static void dump_ast_fallback_stats() {
    std::lock_guard<std::mutex> lock(ast_fallback_mutex);
    if (ast_fallback_counts.empty()) {
        return;
    }
    // Sort by count descending
    std::vector<std::pair<std::string, uint64_t>> sorted(
        ast_fallback_counts.begin(), ast_fallback_counts.end());
    std::sort(sorted.begin(), sorted.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });
    fprintf(stderr, "\n=== IR AST Fallback Profile ===\n");
    fprintf(stderr, "Total evalExprNode calls: %llu\n", total_ast_fallback_count.load());
    for (const auto& [name, count] : sorted) {
        fprintf(stderr, "  %-40s %8llu (%5.1f%%)\n", name.c_str(), count,
            100.0 * count / total_ast_fallback_count.load());
    }
    fprintf(stderr, "===============================\n");
}
#endif

static QoreValue evalExprNode(const QoreValue& expr, ExceptionSink* xsink) {
    if (!expr.hasNode()) {
        if (xsink) {
            xsink->raiseException("IR-INTERPRETER-ERROR", "expression opcode requires a parse node");
        }
        return QoreValue();
    }
#ifdef QORE_IR_PROFILE_AST_FALLBACK
    record_ast_fallback(expr);
#endif
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
        case QoreIROpcode::CastComplexHash:
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

// Thread-local call frame pool — eliminates per-call heap allocation by
// on_block_exit handler record (defined here so IRCallFrame can pool it)
struct IROnBlockExitHandler {
    obe_type_e type;
    StatementBlock* code;
    const QoreIRFunction* handler_ir = nullptr;  //!< compiled handler (nullptr = AST fallback)
};

// recycling all per-call data structures across function calls.
// After warm-up (first visit to each recursion depth), subsequent calls at
// that depth reuse the same vectors/sets with retained capacity → zero malloc.
// For recursive functions like fibonacci, this eliminates millions of
// allocations that otherwise dominate execution time.
struct IRCallFrame {
    std::vector<QoreValue> values;
    std::vector<uint32_t> cleanup;
    std::vector<QoreValue> locals_slot_cache;
    std::vector<LocalVarValue*> locals_lvar_cache;
    std::unordered_set<const LocalVar*> instantiated_locals;
    std::unordered_set<const LocalVar*> locally_uninstantiated;
    std::vector<bool> locals_ir_only;
    std::vector<bool> locals_instantiated;
    std::vector<uint32_t> local_init_slots;
    std::vector<std::vector<uint32_t>> local_load_slots;
    std::vector<bool> load_slot_registered;
    // Ephemeral weak ref slots: value slot IDs holding LoadSelfMember/LoadStaticVar
    // results that must be discarded at statement boundaries to prevent long-running
    // threads from holding references that block object destruction.
    std::vector<uint32_t> ephemeral_weak_ref_slots;

    // Per-call containers pooled here to avoid per-call heap allocation.
    // After warm-up, clear() retains bucket arrays / capacity.
    std::unordered_map<const void*, QoreValue> globals;
    std::unordered_map<const void*, QoreValue> threadlocals;
    std::unordered_map<const void*, QoreValue> closures;
    std::unordered_set<FunctionalOperatorInterface*> active_iterators;
    std::vector<IROnBlockExitHandler> on_block_exit_handlers;

    // Reset all fields for reuse.  clear() retains capacity, so no heap
    // allocation after the first call at each recursion depth.
    void reset(size_t reserve_size, size_t local_slot_count) {
        values.clear();
        values.resize(reserve_size);
        cleanup.clear();
        if (local_slot_count > 0) {
            locals_slot_cache.clear();
            locals_slot_cache.resize(local_slot_count);
            locals_lvar_cache.assign(local_slot_count, nullptr);
            locals_ir_only.assign(local_slot_count, false);
            locals_instantiated.assign(local_slot_count, false);
            local_init_slots.assign(local_slot_count, UINT32_MAX);
            local_load_slots.clear();
            local_load_slots.resize(local_slot_count);
        } else {
            locals_slot_cache.clear();
            locals_lvar_cache.clear();
            locals_ir_only.clear();
            locals_instantiated.clear();
            local_init_slots.clear();
            local_load_slots.clear();
        }
        load_slot_registered.assign(reserve_size, false);
        instantiated_locals.clear();
        locally_uninstantiated.clear();
        ephemeral_weak_ref_slots.clear();
        globals.clear();
        threadlocals.clear();
        closures.clear();
        active_iterators.clear();
        on_block_exit_handlers.clear();
    }
};

struct IRCallFramePool {
    std::vector<std::unique_ptr<IRCallFrame>> frames;
    size_t top = 0;

    IRCallFrame& push(size_t reserve_size, size_t local_slot_count) {
        if (top >= frames.size()) {
            frames.push_back(std::make_unique<IRCallFrame>());
        }
        IRCallFrame& frame = *frames[top++];
        frame.reset(reserve_size, local_slot_count);
        return frame;
    }

    void pop() {
        assert(top > 0);
        --top;
    }
};

static thread_local IRCallFramePool tl_frame_pool;

// Lightweight wrapper around a std::vector<QoreValue> that provides operator[]
// and size(), allowing it to be used as a drop-in replacement for
// std::vector<QoreValue> in function signatures without changing call sites.
struct IRValueSlots {
    std::vector<QoreValue>& vec;

    QoreValue& operator[](size_t i) { return vec[i]; }
    const QoreValue& operator[](size_t i) const { return vec[i]; }
    size_t size() const { return vec.size(); }
};

static inline QoreValue getIRValue(const IRValueSlots& values, QoreIRValue id) {
    // values array is pre-sized to max_value_id + 1 via arena push, so all valid
    // IR value IDs are guaranteed in-bounds.  Skip bounds check on the hot path.
    assert(id.isValid() && id.id < values.size());
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

// Sets a value slot without discarding the previous value (for simple assignments like Const)
static inline void setValueSlotDirect(IRValueSlots& values, uint32_t id, QoreValue new_val) {
    // values array is pre-sized via arena push; assert in-bounds
    assert(id < values.size());
    values[id] = new_val;
}

// Sets a value slot, discarding any previous value to prevent leaks in loops.
// Each slot holds a +1 reference (from new, refSelf, or call result), so
// discarding on overwrite is safe — SSA guarantees the old value is from a
// prior iteration and no longer needed by the current computation.
static inline void setValueSlot(IRValueSlots& values,
        uint32_t id, QoreValue new_val, ExceptionSink* xsink) {
    // values array is pre-sized via arena push; assert in-bounds
    assert(id < values.size());
    values[id].discard(xsink);
    values[id] = new_val;
}

static void cleanupValues(IRValueSlots& values, std::vector<uint32_t>& cleanup,
        ExceptionSink* xsink, bool no_throw, std::vector<std::string>* cleanup_log) {
    // Iterate in reverse (LIFO) to clean up the latest values first.
    // Duplicate IDs in cleanup are safe: after the first discard, values[id]
    // is set to NOTHING, so subsequent discards are no-ops (no hash set needed).
    ExceptionSink* eff_xsink = no_throw ? nullptr : xsink;
    for (auto it = cleanup.rbegin(); it != cleanup.rend(); ++it) {
        uint32_t id = *it;
        if (id >= values.size()) {
            continue;
        }
        QoreValue& slot = values[id];
        if (cleanup_log && slot.hasNode()) {
            if (slot.getType() == NT_STRING) {
                auto* str = slot.get<QoreStringNode>();
                if (str) {
                    cleanup_log->push_back(str->getBuffer());
                }
            } else {
                cleanup_log->push_back(slot.getTypeName());
            }
        }
        slot.discard(eff_xsink);
        slot = QoreValue();
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
        // are already on the thread-local stack from evalTiered.  We skip re-instantiation
        // UNLESS the variable was explicitly uninstantiated mid-execution (e.g., a
        // closure-use loop-body variable whose CVV was popped by UninstantiateLocal at
        // the end of the previous loop iteration).  In that case we MUST re-instantiate
        // so the current iteration has a fresh CVV to capture.
        bool locally_uninst = locally_uninstantiated && locally_uninstantiated->count(var);
        bool is_pre = pre_instantiated && pre_instantiated->count(var) > 0;
        // For pre-instantiated closure-use vars explicitly uninstantiated: need new CVV
        // (the old CVV was popped by UninstantiateLocal, so we must push a fresh one for
        //  the next loop iteration to capture a unique binding).
        // For pre-instantiated non-closure vars explicitly uninstantiated: the LVV stays
        //  on the lvstack (UninstantiateLocal only del()'d the value), so do NOT push
        //  another one — that would create a double-push and make locals_lvar_cache stale.
        bool skip_instantiation = is_pre && !(locally_uninst && var->closureUse());

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

// Forward declaration — defined after execute() with other evalXxxEquals helpers
static QoreValue evalPlusEquals(const QoreValue& lvalue, const QoreValue& right, ExceptionSink* xsink);

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


static void updateLocalVarFromLvalue(
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
        // Invalidate the slot cache entry (don't pre-populate) because
        // assignLocalVarValue() → acceptAssignment() may coerce the value type.
        // The next LoadLocal cache miss will read the actual coerced value.
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

static QoreListNode* buildArgList(const IRValueSlots& values,
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
        const IRValueSlots& values, ExceptionSink* xsink) {
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
        case QoreIROpcode::ExistsBool:
        case QoreIROpcode::IsCollectionType: {
            QoreValue val = inv->operands.empty() ? QoreValue() : getIRValue(values, inv->operands[0]);
            return QoreIRInterpreter::evalUnary(op, val, xsink);
        }
        case QoreIROpcode::ToString: {
            QoreValue val = inv->operands.empty() ? QoreValue() : getIRValue(values, inv->operands[0]);
            switch (val.getType()) {
                case NT_STRING:
                    return QoreValue(val.get<const QoreStringNode>()->stringRefSelf());
                case NT_INT:
                    return QoreValue(new QoreStringNodeMaker(QLLD, val.getAsBigInt()));
                case NT_FLOAT:
                    return QoreValue(q_fix_decimal(new QoreStringNodeMaker("%.9g", val.getAsFloat()), 0));
                case NT_BOOLEAN:
                    return QoreValue(new QoreStringNodeMaker(QLLD, val.getAsBigInt()));
                case NT_NOTHING:
                case NT_NULL:
                    return QoreValue(new QoreStringNode());
                default: {
                    QoreStringValueHelper sv(val);
                    return QoreValue(new QoreStringNode(*sv));
                }
            }
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
            return evalAndRef(inv->expr, xsink);
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
            return evalAndRef(inv->expr, xsink);
        }
        // Call-type opcodes: use pre-evaluated operands to avoid double-evaluation
        case QoreIROpcode::Call:
        case QoreIROpcode::CallDirect:
        case QoreIROpcode::CallIndirect:
        case QoreIROpcode::CallMethod:
        case QoreIROpcode::CallStatic:
        case QoreIROpcode::CallStaticDirect: {
            // Native operator handling: [] and .{} with pre-evaluated operands
            if (!inv->operands.empty() && (op == QoreIROpcode::Call || op == QoreIROpcode::CallDirect)
                    && inv->operands.size() == 2 && inv->expr.hasNode()) {
                if (auto* sq = dynamic_cast<const QoreSquareBracketsOperatorNode*>(
                        inv->expr.getInternalNode())) {
                    QoreValue lhs_val = getIRValue(values, inv->operands[0]);
                    QoreValue rhs_val = getIRValue(values, inv->operands[1]);
                    return QoreSquareBracketsOperatorNode::doSquareBrackets(
                        lhs_val, rhs_val, true, xsink);
                }
                if (auto* hod = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(
                        inv->expr.getInternalNode())) {
                    QoreValue lhs_val = getIRValue(values, inv->operands[0]);
                    QoreValue rhs_val = getIRValue(values, inv->operands[1]);
                    qore_type_t lt = lhs_val.getType();
                    if (lt == NT_HASH) {
                        const QoreHashNode* h = lhs_val.get<const QoreHashNode>();
                        if (rhs_val.getType() == NT_LIST) {
                            return qore_hash_private::get(*h)->getSlice(
                                rhs_val.get<const QoreListNode>(), xsink);
                        }
                        QoreStringNodeValueHelper key(rhs_val);
                        QoreValue v = h->getKeyValue(**key, xsink);
                        return (xsink && *xsink) ? QoreValue() : v.refSelf();
                    }
                    if (lt == NT_OBJECT) {
                        QoreObject* o = const_cast<QoreObject*>(lhs_val.get<const QoreObject>());
                        if (rhs_val.getType() == NT_LIST) {
                            return o->getSlice(rhs_val.get<const QoreListNode>(), xsink);
                        }
                        QoreStringNodeValueHelper key(rhs_val);
                        return o->evalMember(*key, xsink);
                    }
                    return QoreValue();
                }
            }
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
                        // Direct evalImpl() — avoids evalExprNode() overhead
                        FunctionCallNode clone(*call, arg_list);
                        res = evalAndRef(&clone, xsink);
                        used_operands = true;
                    }
                } else if (op == QoreIROpcode::CallMethod) {
                    if (auto* call = dynamic_cast<const SelfFunctionCallNode*>(
                            inv->expr.getInternalNode())) {
                        // Direct evalImpl() — avoids evalExprNode() overhead
                        SelfFunctionCallNode clone(*call, arg_list);
                        res = evalAndRef(&clone, xsink);
                        used_operands = true;
                    }
                } else if (op == QoreIROpcode::CallStatic || op == QoreIROpcode::CallStaticDirect) {
                    if (auto* call = dynamic_cast<const StaticMethodCallNode*>(
                            inv->expr.getInternalNode())) {
                        // Direct evalImpl() — avoids evalExprNode() overhead
                        StaticMethodCallNode clone(*call, arg_list);
                        res = evalAndRef(&clone, xsink);
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
                        // Direct evalImpl() — avoids evalExprNode() overhead
                        CallReferenceCallNode clone(loc, exp, arg_list);
                        res = evalAndRef(&clone, xsink);
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
            // Fall through: no operands or cast failed — direct eval avoids evalExprNode overhead
            // Handle ScopedObjectCallNode (bare "new") directly
            if (inv->expr.hasNode()) {
                auto* scoped = dynamic_cast<const ScopedObjectCallNode*>(
                    inv->expr.getInternalNode());
                if (scoped && scoped->oc) {
                    RuntimeConfig& rc = rc_get_current_ref();
                    return qore_class_private::execConstructor(*scoped->oc, rc,
                        scoped->getVariant(), scoped->getArgs(), xsink);
                }
            }
            return evalAndRef(inv->expr, xsink);
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
            // Direct eval — avoids evalExprNode() overhead
            return evalAndRef(inv->expr, xsink);
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
                // ScopedObjectCallNode: bare "new ClassName(args)" not in a var decl
                auto* scoped = dynamic_cast<const ScopedObjectCallNode*>(inv->expr.getInternalNode());
                if (scoped && scoped->oc) {
                    RuntimeConfig& rc = rc_get_current_ref();
                    return qore_class_private::execConstructor(*scoped->oc, rc,
                        scoped->getVariant(), scoped->getArgs(), xsink);
                }
            }
            // Direct eval — avoids evalExprNode() overhead
            return evalAndRef(inv->expr, xsink);
        }

        // VrnConstruct: construct hashdecl/complex types without local variable assignment
        case QoreIROpcode::VrnConstruct: {
            if (inv->expr.hasNode()) {
                auto* vrn = dynamic_cast<const VarRefNewObjectNode*>(inv->expr.getInternalNode());
                if (vrn) {
                    return vrn->constructValue(xsink);
                }
            }
            // Direct eval — avoids evalExprNode() overhead
            return evalAndRef(inv->expr, xsink);
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
        case QoreIROpcode::CastComplexHash:
        case QoreIROpcode::CastObject:
        case QoreIROpcode::CastEnum:
        case QoreIROpcode::CastAny: {
            QoreValue inner = inv->operands.empty() ? QoreValue() : getIRValue(values, inv->operands[0]);
            auto* cast_node = dynamic_cast<const QoreCastOperatorNode*>(inv->expr.getInternalNode());
            if (cast_node) {
                return cast_node->castValue(inner, xsink);
            }
            // Fallback for unresolved CastAny
            return evalAndRef(inv->expr, xsink);
        }

        // CallClosureDirect: call closure/callref with pre-evaluated operands
        // Without this, CallClosureDirect falls through to AST eval which ignores
        // the pre-evaluated callee and args from lowerCallReference()
        case QoreIROpcode::CallClosureDirect: {
            if (!inv->operands.empty()) {
                QoreValue ref_val = getIRValue(values, inv->operands[0]);
                int nargs = static_cast<int>(inv->operands.size()) - 1;
                // Fast path: call through qore_rt_call_closure_* which uses
                // static_cast + execClosureDirect (no dynamic_cast, no QoreListNode)
                uint64_t ref_bits = toBits(ref_val);
                if (nargs == 0) {
                    return fromBits(qore_rt_call_closure_0(ref_bits, xsink));
                }
                if (nargs == 1) {
                    uint64_t arg0 = toBits(getIRValue(values, inv->operands[1]));
                    return fromBits(qore_rt_call_closure_1(ref_bits, arg0, xsink));
                }
                // N-arg path: use stack buffer for small arg counts
                constexpr int SMALL_BUF = 8;
                uint64_t small_buf[SMALL_BUF];
                uint64_t* args = nargs <= SMALL_BUF ? small_buf : new uint64_t[nargs];
                for (int i = 0; i < nargs; ++i) {
                    args[i] = toBits(getIRValue(values, inv->operands[i + 1]));
                }
                uint64_t result_bits = qore_rt_call_closure_fast(ref_bits, args, nargs, xsink);
                if (nargs > SMALL_BUF) {
                    delete[] args;
                }
                return fromBits(result_bits);
            }
            return evalAndRef(inv->expr, xsink);
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
            return evalAndRef(inv->expr, xsink);
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
            return evalAndRef(inv->expr, xsink);
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
            return evalAndRef(inv->expr, xsink);
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
            return evalAndRef(inv->expr, xsink);
        }

        // InstanceOf: native type check with pre-evaluated operand
        case QoreIROpcode::InstanceOfBool: {
            if (!inv->operands.empty()) {
                auto* io_node = static_cast<const QoreInstanceOfOperatorNode*>(
                    inv->expr.getInternalNode());
                const QoreTypeInfo* ti = io_node->getInstanceTypeInfo();
                QoreValue val = getIRValue(values, inv->operands[0]);
                qore_type_t t = val.getType();
                switch (t) {
                    case NT_WEAKREF:
                        return QoreTypeInfo::runtimeAcceptsValue(ti,
                            **val.get<const WeakReferenceNode>()) ? true : false;
                    case NT_WEAKREF_HASH:
                        return QoreTypeInfo::runtimeAcceptsValue(ti,
                            **val.get<const WeakHashReferenceNode>()) ? true : false;
                    case NT_WEAKREF_LIST:
                        return QoreTypeInfo::runtimeAcceptsValue(ti,
                            **val.get<const WeakListReferenceNode>()) ? true : false;
                    default:
                        return QoreTypeInfo::runtimeAcceptsValue(ti, val) ? true : false;
                }
            }
            return evalAndRef(inv->expr, xsink);
        }
        // Keys: native hash/object key retrieval with pre-evaluated operand
        case QoreIROpcode::KeysAny:
        case QoreIROpcode::KeysList:
        case QoreIROpcode::KeysHash: {
            if (!inv->operands.empty()) {
                QoreValue val = getIRValue(values, inv->operands[0]);
                qore_type_t t = val.getType();
                if (t == NT_HASH) {
                    return val.get<const QoreHashNode>()->getKeys();
                }
                if (t == NT_OBJECT) {
                    QoreObject* o = const_cast<QoreObject*>(val.get<const QoreObject>());
                    AutoVLock vl(xsink);
                    QoreValue members = qore_object_private::get(*o)->getRuntimeMemberHash(xsink);
                    if (xsink && *xsink) {
                        return QoreValue();
                    }
                    if (members.getType() == NT_HASH) {
                        QoreValue keys = members.get<const QoreHashNode>()->getKeys();
                        members.discard(xsink);
                        return keys;
                    }
                    members.discard(xsink);
                }
                return QoreValue();
            }
            return evalAndRef(inv->expr, xsink);
        }

        // Lvalue-modifying opcodes: direct eval() — avoids evalExprNode() overhead
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
            return evalAndRef(inv->expr, xsink);

        // Everything else (LoadLValue, expression ops, etc.)
        // evaluated through the original AST expression
        default:
            return evalAndRef(inv->expr, xsink);
    }
}

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
        const LocalVar* excluded_selfid, const StatementBlock* statements, QoreProgram* pgm, bool suppress_guard_deopt,
        const IRDirectParams* direct_params) {
#ifdef QORE_MANAGE_STACK
    if (check_stack(xsink)) {
        return false;
    }
#endif
    // Grab a pooled call frame from the thread-local pool.  After warm-up, this
    // reuses an existing frame (clear+resize only, no heap allocation) — critical
    // for recursive functions like fibonacci where millions of calls would otherwise
    // malloc/free a dozen vectors per invocation.
    size_t reserve_size = func.max_value_id > 0 ? func.max_value_id + 1 : 128;
    size_t local_slot_count = func.local_var_slots.size();
    IRCallFrame& frame = tl_frame_pool.push(reserve_size, local_slot_count);
    // RAII guard: return the frame to the pool on any exit path
    struct FrameGuard {
        ~FrameGuard() { tl_frame_pool.pop(); }
    } frame_guard;

    IRValueSlots values{frame.values};
    auto& cleanup = frame.cleanup;
    auto& locals_slot_cache = frame.locals_slot_cache;
    auto& locals_lvar_cache = frame.locals_lvar_cache;
    auto& instantiated_locals = frame.instantiated_locals;
    auto& locally_uninstantiated = frame.locally_uninstantiated;
    auto& locals_ir_only = frame.locals_ir_only;
    auto& locals_instantiated = frame.locals_instantiated;
    auto& local_init_slots = frame.local_init_slots;
    auto& local_load_slots = frame.local_load_slots;
    auto& load_slot_registered = frame.load_slot_registered;

    // Precompute bitmask of IR-only local slots.  After function calls (without
    // reference args), only non-IR-only slots need cache invalidation because callees
    // can access non-IR-only locals through the TLS variable stack (Qore's scoping
    // allows inner functions to access outer locals).  IR-only locals exist only in
    // the slot cache and cannot be accessed through TLS, so they stay valid.
    bool has_non_ir_only_locals = false;
    for (auto& [lvar, sid] : func.local_var_slots) {
        if (lvar && sid < local_slot_count) {
            if (func.ir_only_locals.count(reinterpret_cast<const void*>(lvar))) {
                locals_ir_only[sid] = true;
            } else {
                has_non_ir_only_locals = true;
            }
        }
    }
    // Direct params: pre-populate slot cache from caller-provided values,
    // bypassing TLS instantiate/eval/uninstantiate round-trip entirely.
    if (direct_params && direct_params->nargs > 0) {
        for (int i = 0; i < direct_params->nargs; ++i) {
            // Find the slot_id for this param's LocalVar
            auto it = func.param_slot_ids.find(i);
            if (it != func.param_slot_ids.end()) {
                uint32_t sid = it->second;
                if (sid < local_slot_count) {
                    QoreValue val = fromBits(direct_params->args[i]);
                    if (val.hasNode()) {
                        val.refSelf();
                    }
                    locals_slot_cache[sid] = val;
                    locals_instantiated[sid] = true;
                }
            }
        }
    }

    // Use pooled containers from the frame (already cleared in reset())
    auto& globals = frame.globals;
    auto& threadlocals = frame.threadlocals;
    auto& closures = frame.closures;
    auto& active_iterators = frame.active_iterators;
    auto& on_block_exit_handlers = frame.on_block_exit_handlers;
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
                cleanupStoredValues(globals, xsink);
                cleanupStoredValues(threadlocals, xsink);
                cleanupStoredValues(closures, xsink);
                for (size_t i = 0; i < locals_slot_cache.size(); ++i) {
                    locals_slot_cache[i].discard(xsink);
                    locals_slot_cache[i] = QoreValue();
                }
                std::fill(locals_lvar_cache.begin(), locals_lvar_cache.end(), nullptr);
            }
        }
    };

    // Helper to clear all variable caches (slot cache, globals, threadlocals, closures)
    // after AST function/method calls. The calls may have modified any of these variable types,
    // so all caches must be invalidated to force re-read from the runtime stack on next access.
    // Also re-instantiates pre-instantiated closure vars that were popped mid-execution
    // (e.g. loop-body closure-capture vars whose CVV was popped by UninstantiateLocal) so that
    // evalTiered's cleanup can pop exactly one CVV per ast_visible_body_locals var.
    auto cleanupLocalCaches = [&]() {
        // Re-instantiate pre-instantiated closure vars still in locally_uninstantiated.
        // evalTiered pushes exactly one CVV per ast_visible_body_locals var and pops exactly
        // one on cleanup — we must leave one CVV on the cvstack at execute() exit.
        if (pre_instantiated && !locally_uninstantiated.empty()) {
            for (const LocalVar* lv : locally_uninstantiated) {
                if (pre_instantiated->count(lv) && lv->closureUse()) {
                    const_cast<LocalVar*>(lv)->instantiate(QoreParseOptions());
                }
            }
            // Clear to prevent double-instantiation if cleanupLocalCaches is called again.
            locally_uninstantiated.clear();
        }
        for (size_t i = 0; i < locals_slot_cache.size(); ++i) {
            locals_slot_cache[i].discard(xsink);
            locals_slot_cache[i] = QoreValue();
        }
        std::fill(locals_lvar_cache.begin(), locals_lvar_cache.end(), nullptr);
        cleanupStoredValues(globals, xsink);
        cleanupStoredValues(threadlocals, xsink);
        cleanupStoredValues(closures, xsink);
    };

    // Lightweight cache invalidation for after external calls (function/method calls).
    // Called functions run in their own frame and cannot modify the caller's local
    // variables directly.  Only globals, thread-locals, and closure variables might
    // have been modified by the called code.  Additionally, locals that are
    // closure-bound must be invalidated since the callee may have captured and
    // accessed through the TLS variable stack.  IR-only locals are safe since they
    // exist only in the slot cache and cannot be reached by callees.
    auto invalidateExternalCaches = [&]() {
        // Fast path: if no external values were cached, nothing to invalidate
        if (globals.empty() && threadlocals.empty() && closures.empty() && !has_non_ir_only_locals) {
            return;
        }
        cleanupStoredValues(globals, xsink);
        cleanupStoredValues(threadlocals, xsink);
        cleanupStoredValues(closures, xsink);
        // Invalidate non-IR-only local slots: callees can modify these through TLS
        if (has_non_ir_only_locals) {
            for (size_t i = 0; i < locals_ir_only.size(); ++i) {
                if (!locals_ir_only[i]) {
                    locals_slot_cache[i].discard(xsink);
                    locals_slot_cache[i] = QoreValue();
                }
            }
        }
    };

    // Helper: discard all LoadLocal result slots for a given local variable,
    // clear the tracking vector, and reset the registration bitmap entries.
    auto clearLoadSlots = [&](uint32_t slot_id) {
        if (slot_id >= local_load_slots.size() || local_load_slots[slot_id].empty()) {
            return;
        }
        for (uint32_t vid : local_load_slots[slot_id]) {
            if (vid < values.size()) {
                values[vid].discard(xsink);
                values[vid] = QoreValue();
            }
            if (vid < load_slot_registered.size()) {
                load_slot_registered[vid] = false;
            }
        }
        local_load_slots[slot_id].clear();
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

    // Cache pointers to thread-local runtime location fields.
    // Avoids repeated TLS lookups (pthread_getspecific + std::map::find) per instruction.
    // These pointers remain valid for the duration of this function (same thread).
    RuntimeLocationCache rl_cache = get_runtime_location_cache();

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
    auto& ephemeral_weak_ref_slots = frame.ephemeral_weak_ref_slots;
    int last_ephemeral_line = -1;

    while (block) {
        if (ip >= block->instructions.size()) {
            if (xsink) {
                xsink->raiseException("IR-EXEC-ERROR", "fell off end of basic block");
            }
            cleanupValues(values, cleanup, xsink, true, cleanup_log);
            cleanupLocalCaches();
            return false;
        }
        // Fast path: skip phi loop entirely if block has no phi nodes
        if (block->has_phi_nodes) {
            while (ip < block->instructions.size() && block->instructions[ip]->opcode == QoreIROpcode::Phi) {
                // Opcode check above guarantees this is a Phi instruction; static_cast avoids RTTI
                auto* phi = static_cast<QoreIRPhiInstruction*>(block->instructions[ip].get());
                assert(phi);
                if (!prev_block) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "phi has no predecessor");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
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
                    cleanupLocalCaches();
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
        }
        if (ip >= block->instructions.size()) {
            if (xsink) {
                xsink->raiseException("IR-EXEC-ERROR", "fell off end of basic block after phi");
            }
            cleanupValues(values, cleanup, xsink, true, cleanup_log);
            cleanupLocalCaches();
            return false;
        }
        {
        // Save current block pointer to detect block changes (branches) after switch
        QoreIRBasicBlock* current_block = block;
next_instruction:
        QoreIRInstruction* inst = block->instructions[ip].get();
        // Unified per-line overhead: only update runtime_loc, check ephemeral refs,
        // and fire debug events when the source line changes.  Multiple IR instructions
        // on the same source line share the same location, so we only need one check
        // per line transition.  Same-line instructions skip the update entirely —
        // the runtime_loc was set on line entry and remains valid (same line number).
        // Uses cached ThreadData pointers (rl_cache) to avoid repeated TLS lookups
        // (pthread_getspecific + std::map::find per call).
        // Uses cached_start_line (-1 = no loc, >=0 = loc->start_line) to avoid
        // pointer dereferences on the hot path.
        if (inst->cached_start_line >= 0 && inst->cached_start_line != last_ephemeral_line) {
            // Update runtime_loc for exception/callstack reporting via cached pointers
            *rl_cache.stmt_ptr = nullptr;
            *rl_cache.loc_ptr = inst->loc;
            // Discard ephemeral weak-ref values at statement boundaries
            if (!ephemeral_weak_ref_slots.empty()) {
                for (uint32_t slot : ephemeral_weak_ref_slots) {
                    if (slot < values.size()) {
                        values[slot].discard(xsink);
                        values[slot] = QoreValue();
                    }
                }
                ephemeral_weak_ref_slots.clear();
            }
            last_ephemeral_line = inst->cached_start_line;
            // Debug: check for debugger attach/detach and fire dbgStep on line changes
            if (can_debug) {
                bool runtime_check = tlpd->runtimeCheck();
                if (!debug_active && runtime_check) {
                    debug_active = true;
                    last_debug_line = -1;
                } else if (debug_active && !runtime_check) {
                    debug_active = false;
                }
            }
            if (debug_active && inst->cached_start_line != last_debug_line) {
                last_debug_line = inst->cached_start_line;
                AbstractStatement* dbg_stmt = qore_program_private::get(*pgm)->getStatementFromIndex(
                    inst->loc->getFile(), inst->cached_start_line + inst->loc->offset);
                if (dbg_stmt) {
                    int dbg_rc = tlpd->dbgStep(statements, dbg_stmt, xsink);
                    if (dbg_rc || *xsink) {
                        if (dbg_rc == RC_RETURN || *xsink) {
                            if (debug_active) {
                                tlpd->dbgFunctionExit(statements, return_value, xsink);
                            }
                            executeOnBlockExitHandlers(on_block_exit_handlers, xsink);
                            cleanupValues(values, cleanup, xsink, true, cleanup_log);
                            cleanupLocalCaches();
                            return dbg_rc == RC_RETURN;
                        }
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
                setValueSlotDirect(values, cinst->result.id, QoreValue(str));
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
                    // date_microseconds is a UTC epoch from getEpochMicrosecondsUTC(); use
                    // makeAbsolute() which stores the epoch directly without local-to-UTC
                    // conversion (unlike DateTimeNode(s, ms) which goes through setLocalDate)
                    int64_t epoch_seconds = cinst->constant.date_microseconds / 1000000;
                    int us = static_cast<int>(cinst->constant.date_microseconds % 1000000);
                    dt = DateTimeNode::makeAbsolute(currentTZ(), epoch_seconds, us);
                }
                setValueSlotDirect(values, cinst->result.id, QoreValue(dt));
                cleanup.push_back(cinst->result.id);
                ++ip;
                break;
            }
            case QoreIROpcode::ConstEnum: {
                auto* cinst = static_cast<QoreIRConstInstruction*>(inst);
                // TAG_ENUM values are zero-allocation, zero-refcount inline values
                setValueSlot(values, cinst->result.id,
                    QoreValue::makeEnum(cinst->constant.enum_member), xsink);
                ++ip;
                break;
            }
            case QoreIROpcode::MakeList: {
                const auto* ml = static_cast<const QoreIRMakeListInstruction*>(inst);
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
                        cleanupLocalCaches();
                        return false;
                    }
                }
                if (ml->typeInfo) {
                    qore_list_private::get(*list)->complexTypeInfo = ml->typeInfo;
                } else {
                    if (!vtype || vtype == anyTypeInfo || !vcommon) {
                        vtype = autoTypeInfo;
                    }
                    qore_list_private::get(*list)->complexTypeInfo = qore_get_complex_list_type(vtype);
                }
                QoreListNode* raw_list = list.release();
                setValueSlotDirect(values, inst->result.id, QoreValue(raw_list));
                cleanup.push_back(inst->result.id);
                ++ip;
                break;
            }
            case QoreIROpcode::MakeHash: {
                const auto* mh = static_cast<const QoreIRMakeHashInstruction*>(inst);
                ReferenceHolder<QoreHashNode> hash(new QoreHashNode(autoTypeInfo), xsink);
                const QoreTypeInfo* vtype = nullptr;
                bool vcommon = false;
                if (inst->operands.size() % 2 != 0) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "make.hash requires even operands");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                for (size_t i = 0; i < inst->operands.size(); i += 2) {
                    QoreValue key_val = getIRValue(values, inst->operands[i]);
                    QoreValue value = getIRValue(values, inst->operands[i + 1]);
                    QoreValue stored = value.hasNode() ? value.refSelf() : value;
                    const QoreTypeInfo* vt = stored.getFullTypeInfo();
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
                        cleanupLocalCaches();
                        return false;
                    }
                }
                if (mh->typeInfo) {
                    qore_hash_private::get(*hash)->complexTypeInfo = mh->typeInfo;
                } else {
                    if (!vtype || vtype == anyTypeInfo) {
                        vtype = autoTypeInfo;
                    }
                    qore_hash_private::get(*hash)->complexTypeInfo = qore_get_complex_hash_type(vtype);
                }
                setValueSlotDirect(values, inst->result.id, QoreValue(hash.release()));
                cleanup.push_back(inst->result.id);
                ++ip;
                break;
            }
            case QoreIROpcode::MakeHashConstKeys: {
                const auto* mhck = static_cast<const QoreIRMakeHashConstKeysInstruction*>(inst);
                const auto& ckeys = mhck->keys;
                size_t n = ckeys.size();
                assert(n == inst->operands.size());
                ReferenceHolder<QoreHashNode> hash(new QoreHashNode(autoTypeInfo), xsink);
                qore_hash_private* hp = qore_hash_private::get(*hash);
                hp->hm.reserve(n);
                const QoreTypeInfo* vtype = nullptr;
                bool vcommon = false;
                for (size_t i = 0; i < n; ++i) {
                    QoreValue value = getIRValue(values, inst->operands[i]);
                    QoreValue stored = value.hasNode() ? value.refSelf() : value;
                    const QoreTypeInfo* vt = stored.getFullTypeInfo();
                    if (!i) {
                        vtype = vt;
                        vcommon = true;
                    } else if (vcommon && !QoreTypeInfo::matchCommonType(vtype, vt)) {
                        vcommon = false;
                    }
                    hash->setKeyValue(ckeys[i].c_str(), stored, xsink);
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                }
                if (mhck->typeInfo) {
                    hp->complexTypeInfo = mhck->typeInfo;
                } else {
                    if (!vtype || vtype == anyTypeInfo) {
                        vtype = autoTypeInfo;
                    }
                    hp->complexTypeInfo = qore_get_complex_hash_type(vtype);
                }
                setValueSlotDirect(values, inst->result.id, QoreValue(hash.release()));
                cleanup.push_back(inst->result.id);
                ++ip;
                break;
            }
            case QoreIROpcode::CreateEmptyList: {
                QoreListNode* list = new QoreListNode(autoTypeInfo);
                setValueSlotDirect(values, inst->result.id, QoreValue(list));
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
                    cleanupLocalCaches();
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
                setValueSlotDirect(values, inst->result.id, QoreValue(list));
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
                setValueSlotDirect(values, inst->result.id, QoreValue(size));
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
                setValueSlotDirect(values, inst->result.id, QoreValue(result));
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
                setValueSlotDirect(values, inst->result.id, QoreValue(result));
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
            case QoreIROpcode::ListGetValueNoRef: {
                // Read-only list element access — borrowed reference (no refSelf)
                // Safe when the list outlives the use of the returned element
                QoreValue list_val = getIRValue(values, inst->operands[0]);
                QoreValue idx_val = getIRValue(values, inst->operands[1]);
                int64_t index = idx_val.getAsBigInt();
                QoreValue result;
                if (list_val.getType() == NT_LIST) {
                    const QoreListNode* l = list_val.get<const QoreListNode>();
                    if (index >= 0 && static_cast<size_t>(index) < l->size()) {
                        result = l->retrieveEntry(static_cast<size_t>(index));
                    }
                }
                setValueSlot(values, inst->result.id, result, xsink);
                // Do NOT add to cleanup — no owned reference (borrowed from list)
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
                setValueSlotDirect(values, inst->result.id, QoreValue(class_ptr));
                ++ip;
                break;
            }
            case QoreIROpcode::CallClosureDirect: {
                // operands[0] = closure/callref value, operands[1..n] = args
                // Fast path: call through qore_rt_call_closure_* which uses
                // static_cast + execClosureDirect (no dynamic_cast, no QoreListNode)
                QoreValue ref_val = getIRValue(values, inst->operands[0]);
                int nargs = static_cast<int>(inst->operands.size()) - 1;
                uint64_t ref_bits = toBits(ref_val);
                QoreValue result;
                if (nargs == 0) {
                    result = fromBits(qore_rt_call_closure_0(ref_bits, xsink));
                } else if (nargs == 1) {
                    uint64_t arg0 = toBits(getIRValue(values, inst->operands[1]));
                    result = fromBits(qore_rt_call_closure_1(ref_bits, arg0, xsink));
                } else {
                    constexpr int SMALL_BUF = 8;
                    uint64_t small_buf[SMALL_BUF];
                    uint64_t* args = nargs <= SMALL_BUF ? small_buf : new uint64_t[nargs];
                    for (int i = 0; i < nargs; ++i) {
                        args[i] = toBits(getIRValue(values, inst->operands[i + 1]));
                    }
                    result = fromBits(qore_rt_call_closure_fast(ref_bits, args, nargs, xsink));
                    if (nargs > SMALL_BUF) {
                        delete[] args;
                    }
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                // Closure/callref execution runs in its own frame and cannot modify
                // the caller's local variables, so only invalidate external caches
                invalidateExternalCaches();
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
                    setValueSlotDirect(values, inst->result.id, QoreValue(new QoreStringNode()));
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
                            cleanupLocalCaches();
                            return false;
                        }
                    }
                    // NOTHING values are skipped (treated as empty string)
                }
                setValueSlotDirect(values, inst->result.id, QoreValue(result));
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
                    cleanupLocalCaches();
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
                    cleanupLocalCaches();
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
                // Opcode guarantees instruction type; static_cast avoids RTTI overhead
                auto* inv = static_cast<QoreIRInvokeInstruction*>(inst);
                assert(inv);
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
                        // Targeted invalidation: only clear the lvalue target's slot
                        // cache entry and values[] entries to prevent COW from inflated
                        // refcounts. Non-local targets (members, globals) don't need
                        // slot cache invalidation.
                        // inv->expr is the full QoreAssignmentOperatorNode; extract the lvalue first.
                        const VarRefNode* inv_base_var = nullptr;
                        if (inv->expr.hasNode()) {
                            auto* assign = dynamic_cast<const QoreAssignmentOperatorNode*>(
                                    inv->expr.getInternalNode());
                            if (assign) {
                                inv_base_var = extractLValueBaseVarRef(assign->getLeft());
                            }
                        }
                        if (inv_base_var && inv_base_var->ref.id) {
                            auto inv_slot_it = func.local_var_slots.find(
                                reinterpret_cast<const LocalVar*>(inv_base_var->ref.id));
                            if (inv_slot_it != func.local_var_slots.end()) {
                                uint32_t sid = inv_slot_it->second;
                                if (sid < locals_slot_cache.size()) {
                                    locals_slot_cache[sid].discard(xsink);
                                    locals_slot_cache[sid] = QoreValue();
                                }
                                clearLoadSlots(sid);
                            }
                        }
                        break;
                    }
                    default:
                        break;
                }
                QoreValue res = evalInvoke(inv, values, xsink);
                // Invalidate external caches after Invoke — the AST call may have
                // modified globals, thread-locals, or closure variables. The locals
                // slot cache remains valid: called code runs in its own frame and
                // cannot modify the current function's local variables.
                // Fast path: skip if no external values were cached (checked in lambda)
                invalidateExternalCaches();

                // For reference write-through assignments via StoreLValue invoke, flush all local
                // caches — writing through a reference can modify any arbitrary local variable.
                if (inv->invoke_opcode == QoreIROpcode::StoreLValue) {
                    if (inv->expr.hasNode()) {
                        auto* assign = dynamic_cast<const QoreAssignmentOperatorNode*>(
                                inv->expr.getInternalNode());
                        if (assign) {
                            const VarRefNode* base = extractLValueBaseVarRef(assign->getLeft());
                            if (base && base->getTypeInfo()
                                    && QoreTypeInfo::isReference(base->getTypeInfo())) {
                                cleanupLocalCaches();
                            }
                        }
                    }
                }
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
                        cleanupLocalCaches();
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
                        // Handler execution can modify globals, threadlocals, closures, and
                        // non-IR-only locals through TLS.  IR-only locals are unreachable
                        // by handlers and stay valid in the slot cache.
                        invalidateExternalCaches();
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
                        cleanupLocalCaches();
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
                        cleanupLocalCaches();
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
                QoreValue out;

                if (local_inst->is_closure) {
                    // Closure path: always read from runtime stack (closures can
                    // modify the value between IR instructions)
                    if (local_inst->slot_id >= locals_instantiated.size()
                            || !locals_instantiated[local_inst->slot_id]) {
                        ensureLocalInstantiated(local_inst->local, instantiated_locals,
                            pre_instantiated, function_own_locals, &locally_uninstantiated);
                        if (local_inst->slot_id < locals_instantiated.size()) {
                            locals_instantiated[local_inst->slot_id] = true;
                        }
                    }
                    bool needs_deref = true;
                    out = local_inst->local->eval(needs_deref, xsink);
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    if (!local_inst->auto_ref && needs_deref && out.hasNode()) {
                        cleanup.push_back(local_inst->result.id);
                        if (local_inst->slot_id != UINT32_MAX
                                && local_inst->slot_id < local_load_slots.size()) {
                            uint32_t rid = local_inst->result.id;
                            if (rid < load_slot_registered.size() && !load_slot_registered[rid]) {
                                local_load_slots[local_inst->slot_id].push_back(rid);
                                load_slot_registered[rid] = true;
                            }
                        }
                    }
                } else if (local_inst->auto_ref) {
                    // FAST PATH: direct slot cache access (most common case)
                    uint32_t sid = local_inst->slot_id;
                    if (sid != UINT32_MAX && sid < locals_slot_cache.size()) {
                        QoreValue cached_val = locals_slot_cache[sid];
                        if (!cached_val.isNothing() && cached_val.getType() != NT_REFERENCE) {
                            // Cache hit: one refSelf, no hash lookups, no instantiation check
                            out = cached_val.hasNode() ? cached_val.refSelf() : cached_val;
                            goto load_local_done;
                        }
                    }
                    // Cache miss: instantiate if needed, eval, populate cache
                    if (local_inst->local) {
                        if (local_inst->slot_id >= locals_instantiated.size()
                                || !locals_instantiated[local_inst->slot_id]) {
                            ensureLocalInstantiated(local_inst->local, instantiated_locals,
                                pre_instantiated, function_own_locals, &locally_uninstantiated);
                            if (local_inst->slot_id < locals_instantiated.size()) {
                                locals_instantiated[local_inst->slot_id] = true;
                            }
                        }
                        bool needs_deref = true;
                        QoreValue val = local_inst->local->eval(needs_deref, xsink);
                        if (xsink && *xsink) {
                            cleanupValues(values, cleanup, xsink, true, cleanup_log);
                            cleanupLocalCaches();
                            return false;
                        }
                        // Store in slot cache for fast access on next load
                        if (sid != UINT32_MAX && sid < locals_slot_cache.size()) {
                            locals_slot_cache[sid].discard(xsink);
                            locals_slot_cache[sid] = val.hasNode() ? val.refSelf() : val;
                        }
                        out = val.hasNode() ? val.refSelf() : val;
                        // Release eval()'s owned ref — slot-cache and out each hold
                        // their own +1 refs via refSelf()
                        if (needs_deref && val.hasNode()) {
                            val.getInternalNode()->deref(xsink);
                        }
                    }
                } else {
                    // Lvalue path: load without refcount inflation (auto_ref=false)
                    // Do NOT cache to avoid interfering with COW logic
                    if (local_inst->local) {
                        if (local_inst->slot_id >= locals_instantiated.size()
                                || !locals_instantiated[local_inst->slot_id]) {
                            ensureLocalInstantiated(local_inst->local, instantiated_locals,
                                pre_instantiated, function_own_locals, &locally_uninstantiated);
                            if (local_inst->slot_id < locals_instantiated.size()) {
                                locals_instantiated[local_inst->slot_id] = true;
                            }
                        }
                        bool needs_deref = true;
                        out = local_inst->local->eval(needs_deref, xsink);
                        if (xsink && *xsink) {
                            cleanupValues(values, cleanup, xsink, true, cleanup_log);
                            cleanupLocalCaches();
                            return false;
                        }
                        if (needs_deref && out.hasNode()) {
                            cleanup.push_back(local_inst->result.id);
                            if (local_inst->slot_id != UINT32_MAX
                                    && local_inst->slot_id < local_load_slots.size()) {
                                local_load_slots[local_inst->slot_id].push_back(
                                    local_inst->result.id);
                            }
                        }
                    }
                }
load_local_done:
                setValueSlot(values, local_inst->result.id, out, xsink);
                if (out.hasNode() && local_inst->auto_ref) {
                    cleanup.push_back(local_inst->result.id);
                }
                // Track LoadLocal result slot for cleanup in UninstantiateLocal
                // Only track node values that need reference cleanup; simple types
                // (int/float/bool) don't need tracking
                if (out.hasNode()
                        && local_inst->slot_id != UINT32_MAX
                        && local_inst->slot_id < local_load_slots.size()
                        && local_inst->auto_ref) {
                    uint32_t rid = local_inst->result.id;
                    if (rid < load_slot_registered.size() && !load_slot_registered[rid]) {
                        local_load_slots[local_inst->slot_id].push_back(rid);
                        load_slot_registered[rid] = true;
                    }
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
                                cleanupLocalCaches();
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
                        cleanupLocalCaches();
                        return false;
                    }
                } else if (base.getType() == NT_HASH) {
                    const QoreHashNode* h = base.get<const QoreHashNode>();
                    out = h->getKeyValue(hka_inst->key_name.c_str(), xsink);
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    out.refSelf();
                } else if (base.getType() == NT_OBJECT) {
                    QoreObject* o = const_cast<QoreObject*>(base.get<const QoreObject>());
                    out = o->evalMember(hka_inst->key_name.c_str(), xsink);
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
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
                QoreValue out;
                if (base.getType() == NT_HASH) {
                    const QoreHashNode* h = base.get<const QoreHashNode>();
                    bool exists = false;
                    QoreValue v = h->getKeyValueExistence(hka_inst->key_name.c_str(), exists);
                    if (exists) {
                        out = v.getAsBigInt();
                    }
                    // else: key missing → NOTHING
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

                    // At this point, refcount = TLS (1) only (no artificial refs held).
                    // Trigger COW if there are additional external references beyond TLS.
                    if (h->reference_count() > 1) {
                        // COW: create unique copy and update the local variable
                        QoreHashNode* new_h = h->copy();
                        LocalVar* lv = const_cast<LocalVar*>(
                            reinterpret_cast<const LocalVar*>(hks_inst->container->ref.id));
                        // VT_CLOSURE containers require assignClosureVarValue() for correct
                        // scope lookup via thread_find_closure_var()
                        if (hks_inst->container->getType() == VT_CLOSURE) {
                            assignClosureVarValue(lv, QoreValue(new_h), xsink);
                        } else {
                            assignLocalVarValue(lv, QoreValue(new_h), xsink);
                        }
                        if (xsink && *xsink) {
                            new_h->deref(xsink);
                            cleanupValues(values, cleanup, xsink, true, cleanup_log);
                            cleanupLocalCaches();
                            return false;
                        }
                        // Release copy()'s ref; TLS now owns the new_h
                        new_h->deref(xsink);
                        h = new_h;
                    }

                    // Make the update with already-referenced value.
                    // (hash is already in TLS and will be cleaned up normally)
                    h->setKeyValue(hks_inst->key_name.c_str(), val.refSelf(), xsink);

                    // Cleanup must happen AFTER modification completes and locks are released.
                    // Defer clearing refs that may trigger destructors.
                    // Release any auto_ref=true LoadLocal result slots for this container variable.
                    clearLoadSlots(hks_inst->container_slot_id);

                    // Release the slot cache ref.
                    uint32_t csid = hks_inst->container_slot_id;
                    if (csid != UINT32_MAX && csid < locals_slot_cache.size()) {
                        locals_slot_cache[csid].discard(xsink);
                    }

                    // Clear the container slot so cleanup doesn't try to discard it
                    // (it's held by TLS and managed separately)
                    values[hks_inst->operands[0].id] = QoreValue();
                } else if (hash_val.getType() == NT_OBJECT) {
                    const_cast<QoreObject*>(hash_val.get<const QoreObject>())->setValue(
                        hks_inst->key_name.c_str(), val.refSelf(), xsink);
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                if (hks_inst->result.isValid()) {
                    setValueSlot(values, hks_inst->result.id, val, xsink);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::ListIndexAccess: {
                QoreValue list_val = getIRValue(values, inst->operands[0]);
                QoreValue idx_val = getIRValue(values, inst->operands[1]);
                int64_t index = idx_val.getAsBigInt();
                QoreValue out;
                if (list_val.getType() == NT_LIST) {
                    const QoreListNode* l = list_val.get<const QoreListNode>();
                    if (index >= 0 && static_cast<size_t>(index) < l->size()) {
                        out = l->getReferencedEntry(static_cast<size_t>(index));
                    }
                }
                setValueSlot(values, inst->result.id, out, xsink);
                if (out.hasNode()) {
                    cleanup.push_back(inst->result.id);
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

                    // At this point, refcount = TLS (1) only (no artificial refs held).
                    // Trigger COW if there are additional external references beyond TLS.
                    if (l->reference_count() > 1) {
                        // COW: create unique copy and update the local variable
                        QoreListNode* new_l = l->copy();
                        LocalVar* lv = const_cast<LocalVar*>(
                            reinterpret_cast<const LocalVar*>(lis_inst->container->ref.id));
                        // VT_CLOSURE containers require assignClosureVarValue() for correct
                        // scope lookup via thread_find_closure_var()
                        if (lis_inst->container->getType() == VT_CLOSURE) {
                            assignClosureVarValue(lv, QoreValue(new_l), xsink);
                        } else {
                            assignLocalVarValue(lv, QoreValue(new_l), xsink);
                        }
                        if (xsink && *xsink) {
                            new_l->deref(xsink);
                            cleanupValues(values, cleanup, xsink, true, cleanup_log);
                            cleanupLocalCaches();
                            return false;
                        }
                        // Release copy()'s ref; TLS now owns the new_l
                        new_l->deref(xsink);
                        l = new_l;
                    }

                    // Make the update with already-referenced value.
                    // (list is already in TLS and will be cleaned up normally)
                    l->setEntry(index, val.refSelf(), xsink);

                    // Cleanup must happen AFTER modification completes and locks are released.
                    // Defer clearing refs that may trigger destructors.
                    // Release any auto_ref=true LoadLocal result slots for this container variable.
                    clearLoadSlots(lis_inst->container_slot_id);

                    // Release the slot cache ref.
                    uint32_t csid = lis_inst->container_slot_id;
                    if (csid != UINT32_MAX && csid < locals_slot_cache.size()) {
                        locals_slot_cache[csid].discard(xsink);
                    }

                    // Clear the container slot so cleanup doesn't try to discard it
                    // (it's held by TLS and managed separately)
                    values[lis_inst->operands[0].id] = QoreValue();
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                if (lis_inst->result.isValid()) {
                    setValueSlot(values, lis_inst->result.id, val, xsink);
                }
                ++ip;
                break;
            }
            // Fused local int operations — reduce dispatch overhead for tight loops
            case QoreIROpcode::AddAssignLocalInt: {
                auto* fused_inst = static_cast<QoreIRAddAssignLocalIntInstruction*>(inst);
                // Read target from slot cache (fast path) or eval (cold path)
                int64_t target_val = 0;
                if (fused_inst->target_slot_id < locals_slot_cache.size()
                        && !locals_slot_cache[fused_inst->target_slot_id].isNothing()) {
                    target_val = locals_slot_cache[fused_inst->target_slot_id].getAsBigInt();
                } else {
                    ensureLocalInstantiated(fused_inst->target, instantiated_locals,
                        pre_instantiated, function_own_locals, &locally_uninstantiated);
                    if (fused_inst->target_slot_id < locals_instantiated.size()) {
                        locals_instantiated[fused_inst->target_slot_id] = true;
                    }
                    bool nd = true;
                    QoreValue tv = fused_inst->target->eval(nd, xsink);
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    target_val = tv.getAsBigInt();
                    if (nd && tv.hasNode()) {
                        tv.getInternalNode()->deref(xsink);
                    }
                }
                // Read source from slot cache (fast path) or eval (cold path)
                int64_t source_val = 0;
                if (fused_inst->source_slot_id < locals_slot_cache.size()
                        && !locals_slot_cache[fused_inst->source_slot_id].isNothing()) {
                    source_val = locals_slot_cache[fused_inst->source_slot_id].getAsBigInt();
                } else {
                    ensureLocalInstantiated(fused_inst->source, instantiated_locals,
                        pre_instantiated, function_own_locals, &locally_uninstantiated);
                    if (fused_inst->source_slot_id < locals_instantiated.size()) {
                        locals_instantiated[fused_inst->source_slot_id] = true;
                    }
                    bool nd = true;
                    QoreValue sv = fused_inst->source->eval(nd, xsink);
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    source_val = sv.getAsBigInt();
                    if (nd && sv.hasNode()) {
                        sv.getInternalNode()->deref(xsink);
                    }
                }
                int64 result_val = target_val + source_val;
                // Update slot cache (always)
                if (fused_inst->target_slot_id < locals_slot_cache.size()) {
                    locals_slot_cache[fused_inst->target_slot_id] = QoreValue(result_val);
                }
                // Write through to thread-local variable only if not IR-only
                if (!fused_inst->target_ir_only) {
                    if (fused_inst->target_slot_id < locals_lvar_cache.size()) {
                        // Use cached LocalVarValue* for direct write-through (avoids TLS lookup)
                        LocalVarValue*& lvv = locals_lvar_cache[fused_inst->target_slot_id];
                        if (!lvv) {
                            lvv = thread_find_lvar(fused_inst->target->getName());
                        }
                        if (lvv) {
                            discard(lvv->val.assign(result_val), xsink);
                        } else {
                            assignLocalVarValue(fused_inst->target, QoreValue(result_val), xsink);
                        }
                    } else {
                        assignLocalVarValue(fused_inst->target, QoreValue(result_val), xsink);
                    }
                }
                // Set result value
                if (fused_inst->result.isValid()) {
                    setValueSlot(values, fused_inst->result.id, QoreValue(result_val), xsink);
                }
                ++ip;
                break;
            }

            case QoreIROpcode::IncrementLocalInt: {
                auto* fused_inst = static_cast<QoreIRIncrementLocalIntInstruction*>(inst);
                // Read local from slot cache (fast path) or eval (cold path)
                int64_t local_val = 0;
                if (fused_inst->slot_id < locals_slot_cache.size()
                        && !locals_slot_cache[fused_inst->slot_id].isNothing()) {
                    local_val = locals_slot_cache[fused_inst->slot_id].getAsBigInt();
                } else {
                    ensureLocalInstantiated(fused_inst->local, instantiated_locals,
                        pre_instantiated, function_own_locals, &locally_uninstantiated);
                    if (fused_inst->slot_id < locals_instantiated.size()) {
                        locals_instantiated[fused_inst->slot_id] = true;
                    }
                    bool nd = true;
                    QoreValue lv = fused_inst->local->eval(nd, xsink);
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    local_val = lv.getAsBigInt();
                    if (nd && lv.hasNode()) {
                        lv.getInternalNode()->deref(xsink);
                    }
                }
                int64 result_val = local_val + fused_inst->delta;
                // Update slot cache (always)
                if (fused_inst->slot_id < locals_slot_cache.size()) {
                    locals_slot_cache[fused_inst->slot_id] = QoreValue(result_val);
                }
                // Write through to thread-local variable only if not IR-only
                if (!fused_inst->ir_only) {
                    if (fused_inst->slot_id < locals_lvar_cache.size()) {
                        // Use cached LocalVarValue* for direct write-through (avoids TLS lookup)
                        LocalVarValue*& lvv = locals_lvar_cache[fused_inst->slot_id];
                        if (!lvv) {
                            lvv = thread_find_lvar(fused_inst->local->getName());
                        }
                        if (lvv) {
                            discard(lvv->val.assign(result_val), xsink);
                        } else {
                            assignLocalVarValue(fused_inst->local, QoreValue(result_val), xsink);
                        }
                    } else {
                        assignLocalVarValue(fused_inst->local, QoreValue(result_val), xsink);
                    }
                }
                // Set result value
                if (fused_inst->result.isValid()) {
                    setValueSlot(values, fused_inst->result.id, QoreValue(result_val), xsink);
                }
                ++ip;
                break;
            }

            case QoreIROpcode::BranchIfLtLocalInt: {
                auto* fused_inst = static_cast<QoreIRBranchIfLtLocalIntInstruction*>(inst);
                // Read both locals from slot cache (fast path) or eval (cold path)
                int64_t lhs_val = 0;
                if (fused_inst->lhs_slot_id < locals_slot_cache.size()
                        && !locals_slot_cache[fused_inst->lhs_slot_id].isNothing()) {
                    lhs_val = locals_slot_cache[fused_inst->lhs_slot_id].getAsBigInt();
                } else {
                    ensureLocalInstantiated(fused_inst->lhs, instantiated_locals,
                        pre_instantiated, function_own_locals, &locally_uninstantiated);
                    if (fused_inst->lhs_slot_id < locals_instantiated.size()) {
                        locals_instantiated[fused_inst->lhs_slot_id] = true;
                    }
                    bool nd = true;
                    QoreValue lv = fused_inst->lhs->eval(nd, xsink);
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    lhs_val = lv.getAsBigInt();
                    if (nd && lv.hasNode()) {
                        lv.getInternalNode()->deref(xsink);
                    }
                }
                int64_t rhs_val = 0;
                if (fused_inst->rhs_slot_id < locals_slot_cache.size()
                        && !locals_slot_cache[fused_inst->rhs_slot_id].isNothing()) {
                    rhs_val = locals_slot_cache[fused_inst->rhs_slot_id].getAsBigInt();
                } else {
                    ensureLocalInstantiated(fused_inst->rhs, instantiated_locals,
                        pre_instantiated, function_own_locals, &locally_uninstantiated);
                    if (fused_inst->rhs_slot_id < locals_instantiated.size()) {
                        locals_instantiated[fused_inst->rhs_slot_id] = true;
                    }
                    bool nd = true;
                    QoreValue rv = fused_inst->rhs->eval(nd, xsink);
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    rhs_val = rv.getAsBigInt();
                    if (nd && rv.hasNode()) {
                        rv.getInternalNode()->deref(xsink);
                    }
                }
                prev_block = block;
                block = (lhs_val < rhs_val) ? fused_inst->true_target : fused_inst->false_target;
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
                        cleanupLocalCaches();
                        return false;
                    }
                }
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
                    cleanupLocalCaches();
                    return false;
                }
                bool needs_eval = val->needsEval();
                QoreValue out = needs_eval ? val->eval(xsink) : val.release();
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
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
                    cleanupLocalCaches();
                    return false;
                }
                bool needs_eval = val->needsEval();
                QoreValue out = needs_eval ? val->eval(xsink) : val.release();
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
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
                const QoreClass* qc = no_inst->qc;
                const AbstractQoreFunctionVariant* variant = no_inst->variant;
                const QoreListNode* args = no_inst->args;
                if (!qc && no_inst->expr.hasNode()) {
                    // AOT fallback: qc/variant/args not set at compile time;
                    // recover class and args from the deserialized expression node
                    auto* nocn = dynamic_cast<const NewObjectCallNode*>(
                        no_inst->expr.getInternalNode());
                    if (nocn) {
                        qc = nocn->getClass();
                        variant = nocn->getVariant();
                        args = nocn->getArgs();
                    } else {
                        auto* socn = dynamic_cast<const ScopedObjectCallNode*>(
                            no_inst->expr.getInternalNode());
                        if (socn) {
                            qc = socn->oc;
                            variant = socn->getVariant();
                            args = socn->getArgs();
                        }
                    }
                }
                if (!qc) {
                    xsink->raiseException("RUNTIME-ERROR",
                        "cannot construct object: class not resolved in AOT mode");
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                QoreValue out = qore_class_private::execConstructor(*qc, rc,
                        variant, args, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
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
                QoreValue out;
                if (lc_inst->node) {
                    out = const_cast<RuntimeConstantRefNode*>(lc_inst->node)->eval(
                            needs_deref, xsink);
                    if (!needs_deref && out.hasNode()) {
                        out = out.refSelf();
                    }
                    // Set exception location to the constant location if an exception occurred
                    if (xsink && *xsink && lc_inst->node && lc_inst->node->loc) {
                        xsink->setLastLocation(*lc_inst->node->loc);
                    }
                } else {
                    // AOT mode: node is null; expr holds the resolved constant value directly
                    // (set by readOneExpr via resolveExprSlot for RUNTIME_CONST_REF)
                    out = lc_inst->expr.refSelf();
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
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
                QoreValue out;
                if (cc_inst->closure_node) {
                    // Ensure all closure-captured local variables are instantiated on the
                    // thread-local stack before eval() tries to create the closure.
                    // The QoreClosureParseNode::eval() method creates a
                    // ThreadSafeLocalVarRuntimeEnvironment which calls thread_find_closure_var()
                    // for each captured variable — if they're not on the cvstack yet, the lookup
                    // returns nullptr and we get a crash.
                    const LVarSet* vlist = cc_inst->closure_node->getVList();
                    if (vlist) {
                        for (LocalVar* var : *vlist) {
                            ensureLocalInstantiated(var, instantiated_locals, pre_instantiated,
                                function_own_locals, &locally_uninstantiated);
                        }
                    }
                    out = const_cast<QoreClosureParseNode*>(cc_inst->closure_node)->eval(
                            needs_deref, xsink);
                    if (!needs_deref && out.hasNode()) {
                        out = out.refSelf();
                    }
                } else if (cc_inst->expr.hasNode()) {
                    // AOT mode: closure_node is null; expr holds the resolved QoreClosureParseNode
                    // (set by buildContextFromSlotMap/buildContextForVariant for CLOSURE_CREATE)
                    QoreClosureParseNode* closure_node =
                        static_cast<QoreClosureParseNode*>(cc_inst->expr.getInternalNode());
                    // Ensure all closure-captured local variables are instantiated on the
                    // thread-local stack before eval() tries to create the closure.
                    const LVarSet* vlist = closure_node->getVList();
                    if (vlist) {
                        for (LocalVar* var : *vlist) {
                            ensureLocalInstantiated(var, instantiated_locals, pre_instantiated,
                                function_own_locals, &locally_uninstantiated);
                        }
                    }
                    out = closure_node->eval(needs_deref, xsink);
                    if (!needs_deref && out.hasNode()) {
                        out = out.refSelf();
                    }
                } else {
                    xsink->raiseException("RUNTIME-ERROR",
                        "closure expression not resolved in AOT mode");
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
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
                if (!ref_expr.hasNode()) {
                    xsink->raiseException("RUNTIME-ERROR",
                        "call reference expression not resolved in AOT mode");
                    ref_expr.discard(xsink);
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                QoreValue out = ref_expr.getInternalNode()->eval(needs_deref, xsink);
                if (!needs_deref && out.hasNode()) {
                    out = out.refSelf();
                }
                ref_expr.discard(xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
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
                if (!ref_expr.hasNode()) {
                    xsink->raiseException("RUNTIME-ERROR",
                        "method reference expression not resolved in AOT mode");
                    ref_expr.discard(xsink);
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                QoreValue out = ref_expr.getInternalNode()->eval(needs_deref, xsink);
                if (!needs_deref && out.hasNode()) {
                    out = out.refSelf();
                }
                ref_expr.discard(xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
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
                QoreValue out;
                if (pr_inst->node) {
                    out = const_cast<ParseReferenceNode*>(pr_inst->node)->eval(needs_deref, xsink);
                    if (!needs_deref && out.hasNode()) {
                        out = out.refSelf();
                    }
                } else if (pr_inst->expr.hasNode()) {
                    // AOT mode: node is null; expr holds the reconstructed ParseReferenceNode
                    out = pr_inst->expr.getInternalNode()->eval(needs_deref, xsink);
                    if (!needs_deref && out.hasNode()) {
                        out = out.refSelf();
                    }
                } else {
                    xsink->raiseException("RUNTIME-ERROR", "parse reference not resolved in AOT mode");
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
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
                    cleanupLocalCaches();
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
                    cleanupLocalCaches();
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
                    cleanupLocalCaches();
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
                    cleanupLocalCaches();
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
                    cleanupLocalCaches();
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
                    cleanupLocalCaches();
                    return false;
                }
                // Fast path: skip hash lookups if already instantiated (bitset check)
                if (local_inst->slot_id >= locals_instantiated.size()
                        || !locals_instantiated[local_inst->slot_id]) {
                    ensureLocalInstantiated(local_inst->local, instantiated_locals, pre_instantiated,
                            function_own_locals, &locally_uninstantiated);
                    if (local_inst->slot_id < locals_instantiated.size()) {
                        locals_instantiated[local_inst->slot_id] = true;
                    }
                }
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
                // Don't cache reference-type locals — writes must go through
                // LValueHelper to follow the reference binding to the original
                // variable.  Caching would bypass the reference mechanism and
                // either overwrite the ReferenceNode or cache a stale value.
                // We invalidate (rather than pre-populate) the cache here because
                // assignLocalVarValue() → acceptAssignment() may coerce the value
                // (e.g., list → list<hash<auto>>).  When the list has refcount > 1
                // (due to the cache holding a ref), acceptInputComplexList() creates
                // a COPY with the correct value type info.  The local variable gets
                // the copy; if we cached the original, it would be stale.  The next
                // LoadLocal cache miss will read the actual coerced value.
                if (!local_inst->is_closure
                        && !local_inst->is_ref
                        && local_inst->slot_id != UINT32_MAX
                        && local_inst->slot_id < locals_slot_cache.size()) {
                    if (!val.hasNode() && !val.isEnum()) {
                        // Simple type (int/float/bool/nothing): check that the target
                        // variable's declared type accepts this value type before using
                        // the direct write-through fast path (which bypasses runtime
                        // type checking via acceptAssignment())
                        // NOTE: TAG_ENUM values also have hasNode()=false, but must go
                        // through the full path to preserve enum identity and type checking
                        const QoreTypeInfo* target_ti = local_inst->local
                            ? local_inst->local->getTypeInfo() : nullptr;
                        if (!QoreTypeInfo::parseAcceptsReturns(target_ti, val.getType())) {
                            // Type mismatch: invalidate caches and use full
                            // type-checking path (will raise RUNTIME-TYPE-ERROR)
                            locals_slot_cache[local_inst->slot_id].discard(xsink);
                            locals_slot_cache[local_inst->slot_id] = QoreValue();
                            if (local_inst->slot_id < locals_lvar_cache.size()) {
                                locals_lvar_cache[local_inst->slot_id] = nullptr;
                            }
                            assignLocalVarValue(local_inst->local, val, xsink);
                        } else {
                            // Type is compatible: pre-populate the cache to avoid a
                            // cache miss on the next LoadLocal
                            locals_slot_cache[local_inst->slot_id].discard(xsink);
                            locals_slot_cache[local_inst->slot_id] = val;
                            // Use cached LocalVarValue* for direct write-through (avoids TLS lookup)
                            if (local_inst->slot_id < locals_lvar_cache.size()) {
                                LocalVarValue*& lvv = locals_lvar_cache[local_inst->slot_id];
                                if (!lvv) {
                                    lvv = thread_find_lvar(local_inst->local->getName());
                                }
                                if (lvv) {
                                    qore_type_t vt = val.getType();
                                    if (vt == NT_INT) {
                                        discard(lvv->val.assign(val.getAsBigInt()), xsink);
                                    } else if (vt == NT_FLOAT) {
                                        discard(lvv->val.assign(val.getAsFloat()), xsink);
                                    } else if (vt == NT_BOOLEAN) {
                                        discard(lvv->val.assign(val.getAsBool()), xsink);
                                    } else {
                                        assignLocalVarValue(local_inst->local, val, xsink);
                                    }
                                } else {
                                    assignLocalVarValue(local_inst->local, val, xsink);
                                }
                            } else {
                                assignLocalVarValue(local_inst->local, val, xsink);
                            }
                        }
                    } else {
                        // Complex type: invalidate because assignLocalVarValue() →
                        // acceptAssignment() may coerce the value
                        locals_slot_cache[local_inst->slot_id].discard(xsink);
                        locals_slot_cache[local_inst->slot_id] = QoreValue();
                        // Also invalidate lvar cache for complex types
                        if (local_inst->slot_id < locals_lvar_cache.size()) {
                            locals_lvar_cache[local_inst->slot_id] = nullptr;
                        }
                        assignLocalVarValue(local_inst->local, val, xsink);
                    }
                } else {
                    assignLocalVarValue(local_inst->local, val, xsink);
                }

                // For weak assignments, cache the weak-wrapped value in slot cache
                // so LoadLocal returns the WeakReferenceNode instead of calling
                // eval() which unwraps it
                if (local_inst->weak && val.getType() >= NT_WEAKREF
                        && val.getType() <= NT_WEAKREF_LIST
                        && local_inst->slot_id != UINT32_MAX
                        && local_inst->slot_id < locals_slot_cache.size()) {
                    locals_slot_cache[local_inst->slot_id].discard(xsink);
                    locals_slot_cache[local_inst->slot_id] = val.hasNode() ? val.refSelf() : val;
                }

                // If the variable holds a reference, assignLocalVarValue wrote through
                // the reference to another variable.  Clear all caches to prevent
                // stale reads from that target variable and the slot cache.
                if (local_inst->local
                        && QoreTypeInfo::isReference(local_inst->local->getTypeInfo())) {
                    cleanupLocalCaches();
                }
                // Track the operand slot for cleanup when this local is uninstantiated
                if (operand.isValid() && local_inst->slot_id != UINT32_MAX
                        && local_inst->slot_id < local_init_slots.size()) {
                    local_init_slots[local_inst->slot_id] = operand.id;
                }
                if (xsink && *xsink) {
                    if (inst->exception_target) {
                        prev_block = block;
                        block = inst->exception_target;
                        ip = 0;
                        break;
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                ++ip;
                break;
            }
            case QoreIROpcode::UninstantiateLocal: {
                auto* local_inst = static_cast<QoreIRLocalInstruction*>(inst);
                // Uninstantiate the local variable (calls destructor for objects)
                if (local_inst->local) {
                    bool is_pre = pre_instantiated && pre_instantiated->find(local_inst->local) != pre_instantiated->end();
                    // Check if this variable was actually instantiated by the IR
                    // interpreter. For non-pre-instantiated (IR-only) variables,
                    // ensureLocalInstantiated() pushes them on first use. If a
                    // variable was never used (e.g. in a code path not taken),
                    // it was never pushed, so we must not pop it here.
                    // NOTE: outer-scope variables (from a calling function) are
                    // also !is_pre && !was_instantiated -- ensureLocalInstantiated()
                    // skips them (via the function_own_locals check), so they are
                    // never added to instantiated_locals. The !is_pre && !was_instantiated
                    // path below handles them safely: it clears the slot cache but
                    // does NOT call uninstantiate(), so no stack corruption occurs.
                    bool was_instantiated = instantiated_locals.count(local_inst->local) > 0;
                    // Remove from instantiated_locals since we're explicitly cleaning it up
                    instantiated_locals.erase(local_inst->local);
                    // Clear the fast-path instantiation flag
                    if (local_inst->slot_id < locals_instantiated.size()) {
                        locals_instantiated[local_inst->slot_id] = false;
                    }
                    if (!is_pre && !was_instantiated) {
                        // Variable was never on the TLS stack for this function —
                        // skip uninstantiation to avoid popping an unrelated
                        // stack entry. BUT: still clear the slot cache value (if any)
                        // to trigger destructors at block scope exit (e.g., AutoLock).
                        if (local_inst->slot_id != UINT32_MAX
                                && local_inst->slot_id < locals_slot_cache.size()) {
                            locals_slot_cache[local_inst->slot_id].discard(xsink);
                            locals_slot_cache[local_inst->slot_id] = QoreValue();
                        }
                        // Also clear the init slot (from StoreLocal) to drop that reference
                        if (local_inst->slot_id != UINT32_MAX) {
                            if (local_inst->slot_id < local_init_slots.size()
                                    && local_init_slots[local_inst->slot_id] != UINT32_MAX) {
                                uint32_t init_slot = local_init_slots[local_inst->slot_id];
                                if (init_slot < values.size()) {
                                    values[init_slot].discard(xsink);
                                    values[init_slot] = QoreValue();
                                }
                                local_init_slots[local_inst->slot_id] = UINT32_MAX;
                            }
                            clearLoadSlots(local_inst->slot_id);
                        }
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
                    // fires during slot cache discard which is too late for proper
                    // globals cache invalidation.
                    // Drop slot cache reference FIRST (before lvar->del)
                    if (local_inst->slot_id != UINT32_MAX
                            && local_inst->slot_id < locals_slot_cache.size()) {
                        locals_slot_cache[local_inst->slot_id].discard(xsink);
                        locals_slot_cache[local_inst->slot_id] = QoreValue();
                    }

                    // Helper lambda: drop all value slots associated with this local
                    // (init slot from StoreLocal + load slots from LoadLocal) BEFORE
                    // lvar->del()/uninstantiate() so that lvar->del() is the true
                    // final deref and triggers the destructor immediately.
                    auto cleanupLocalSlots = [&](uint32_t var_slot_id) {
                        if (var_slot_id == UINT32_MAX) {
                            return;
                        }
                        // Clean up the init slot (from StoreLocal / VarRefNewObjectNode)
                        if (var_slot_id < local_init_slots.size()
                                && local_init_slots[var_slot_id] != UINT32_MAX) {
                            uint32_t init_slot = local_init_slots[var_slot_id];
                            if (init_slot < values.size()) {
                                values[init_slot].discard(xsink);
                                values[init_slot] = QoreValue();
                            }
                            // Don't remove from cleanup vector — values[slot] is now
                            // NOTHING so cleanupValues() will no-op on it.  Removing
                            // via erase(remove()) is O(n) and causes O(n^2) in loops.
                            local_init_slots[var_slot_id] = UINT32_MAX;
                        }
                        // Clean up all LoadLocal result slots for this local.
                        // On the last loop iteration, these slots are never overwritten
                        // by the next iteration's LoadLocal, so they hold an extra
                        // reference that defers the object's destructor to function exit.
                        clearLoadSlots(var_slot_id);
                    };

                    if (is_pre) {
                        // Pre-instantiated local: clear the value on the runtime stack
                        // to trigger destructors at block scope exit.  The entry stays
                        // on the stack so the caller's cleanup can pop it later.

                        // Drop value slot references (init + load slots)
                        cleanupLocalSlots(local_inst->slot_id);

                        // lvar->del() / cvv uninstantiate is the FINAL deref that
                        // triggers the destructor.  After this call, invalidate the
                        // globals cache since the destructor runs AST code.
                        // For closure-captured pre-instantiated locals (e.g. loop body
                        // vars): pop the old ClosureVarValue (giving each iteration an
                        // independent CVV so closures capture their own binding, not the
                        // shared last value).  We do NOT push a fresh CVV here; the next
                        // iteration's ensureLocalInstantiated will re-push when first
                        // accessed (because locally_uninstantiated is set above).
                        // At function exit, cleanupLocalCaches() re-pushes an empty CVV
                        // for evalTiered's cleanup to pop (maintaining the stack invariant).
                        if (local_inst->is_closure) {
                            local_inst->local->uninstantiate(xsink);
                        } else {
                            LocalVarValue* lvar = thread_find_lvar(local_inst->local->getName());
                            if (lvar) {
                                lvar->del(xsink);
                            }
                        }
                        // Destructor runs in its own scope and cannot modify our locals
                        invalidateExternalCaches();
                    } else {
                        // Non-pre-instantiated: full uninstantiate (pop + destructor)
                        // Clean up value slots (init + load) BEFORE uninstantiating
                        cleanupLocalSlots(local_inst->slot_id);
                        local_inst->local->uninstantiate(xsink);
                        // Destructor runs in its own scope and cannot modify our locals
                        invalidateExternalCaches();
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
                    cleanupLocalCaches();
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
                    cleanupLocalCaches();
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
                    cleanupLocalCaches();
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
                    cleanupLocalCaches();
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
                    cleanupLocalCaches();
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
                    cleanupLocalCaches();
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
                setValueSlotDirect(values, inst->result.id, element);
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
                setValueSlotDirect(values, inst->result.id, old_element);
                ++ip;
                break;
            }
            case QoreIROpcode::PopImplicitElement: {
                QoreValue old_val = getIRValue(values, inst->operands[0]);
                save_implicit_element(static_cast<int>(old_val.getAsBigInt()));
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
                    cleanupLocalCaches();
                    return false;
                }
                cleanupLocalCaches();
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
                    cleanupLocalCaches();
                    return false;
                }
                cleanupLocalCaches();
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
                    cleanupLocalCaches();
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
                    cleanupLocalCaches();
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
                    cleanupLocalCaches();
                    return false;
                }
                setValueSlotDirect(values, inst->result.id,
                    QoreValue(static_cast<int64_t>(state)));
                ++ip;
                break;
            }
            case QoreIROpcode::RefForeachSize: {
                QoreValue state_val = getIRValue(values, inst->operands[0]);
                int64_t size = qore_rt_ref_foreach_size(
                    static_cast<uint64_t>(state_val.getAsBigInt()));
                setValueSlotDirect(values, inst->result.id, QoreValue(size));
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
                    cleanupLocalCaches();
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
                    cleanupLocalCaches();
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
                    cleanupLocalCaches();
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
                if (obe_inst->stmt) {
                    // Normal case: record the handler for deferred execution at block/function exit.
                    // Don't call exec() here - the AST's exec() calls advance_on_block_exit()
                    // which requires the thread on_block_exit stack to be set up by StatementBlock,
                    // but the IR interpreter doesn't go through StatementBlock::execIntern().
                    on_block_exit_handlers.push_back({obe_inst->stmt->getType(),
                        obe_inst->stmt->getCode(),
                        obe_inst->handler_ir ? obe_inst->handler_ir.get() : nullptr});
                } else if (obe_inst->handler_ir) {
                    // Deserialized case: no AST statement, but handler IR is available
                    on_block_exit_handlers.push_back({obe_inst->obe_type,
                        nullptr,
                        obe_inst->handler_ir.get()});
                } else {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "on-block-exit requires a statement or handler IR");
                    }
                    executeOnBlockExitHandlers(on_block_exit_handlers, xsink);
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
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
                        // Handler execution (both AST and compiled IR) can modify globals,
                        // threadlocals, closures, and non-IR-only locals through the TLS
                        // variable stack.  IR-only locals exist only in the slot cache and
                        // are unreachable by handlers, so they stay valid.
                        invalidateExternalCaches();
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
                cleanupLocalCaches();
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
                    cleanupLocalCaches();
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
                        cleanupLocalCaches();
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
            case QoreIROpcode::ExistsBool:
            case QoreIROpcode::IsCollectionType: {
                if (inst->operands.size() < 1) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "unary op missing operand");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                QoreValue val = getIRValue(values, inst->operands[0]);
                QoreValue res = QoreIRInterpreter::evalUnary(inst->opcode, val, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                setValueSlot(values, inst->result.id, res, xsink);
                if (res.hasNode()) {
                    cleanup.push_back(inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::ToString: {
                QoreValue val = getIRValue(values, inst->operands[0]);
                QoreStringNode* str;
                switch (val.getType()) {
                    case NT_STRING:
                        str = val.get<const QoreStringNode>()->stringRefSelf();
                        break;
                    case NT_INT:
                        str = new QoreStringNodeMaker(QLLD, val.getAsBigInt());
                        break;
                    case NT_FLOAT:
                        str = q_fix_decimal(new QoreStringNodeMaker("%.9g", val.getAsFloat()), 0);
                        break;
                    case NT_BOOLEAN:
                        str = new QoreStringNodeMaker(QLLD, val.getAsBigInt());
                        break;
                    case NT_NOTHING:
                    case NT_NULL:
                        str = new QoreStringNode();
                        break;
                    default: {
                        // General fallback: use QoreStringValueHelper
                        QoreStringValueHelper sv(val);
                        str = new QoreStringNode(*sv);
                        break;
                    }
                }
                setValueSlot(values, inst->result.id, QoreValue(str), xsink);
                cleanup.push_back(inst->result.id);
                ++ip;
                break;
            }
            case QoreIROpcode::Sprintf: {
                QoreValue val = getIRValue(values, inst->operands[0]);
                QoreStringNode* str;
                if (val.getType() == NT_LIST) {
                    str = q_sprintf(val.get<const QoreListNode>(), 0, 0, xsink);
                    if (*xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        if (inst->exception_target) {
                            prev_block = block;
                            block = inst->exception_target;
                            ip = 0;
                            break;
                        }
                        if (debug_active) {
                            tlpd->dbgFunctionExit(statements, return_value, xsink);
                        }
                        fireScopeExits();
                        cleanupLocalCaches();
                        return false;
                    }
                } else {
                    // Single value: convert to string
                    QoreStringValueHelper sv(val);
                    str = new QoreStringNode(*sv);
                }
                setValueSlot(values, inst->result.id, QoreValue(str), xsink);
                cleanup.push_back(inst->result.id);
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
                    cleanupLocalCaches();
                    return false;
                }
                QoreValue left = getIRValue(values, inst->operands[0]);
                QoreValue right = getIRValue(values, inst->operands[1]);
                QoreValue res = QoreIRInterpreter::evalBinary(inst->opcode, left, right, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
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
                    res = evalAndRef(expr_inst->expr, xsink);
                } else if (inst->operands.size() < 3) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "ternary op missing operands");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                } else {
                    QoreValue first = getIRValue(values, inst->operands[0]);
                    QoreValue second = getIRValue(values, inst->operands[1]);
                    QoreValue third = getIRValue(values, inst->operands[2]);
                    res = QoreIRInterpreter::evalTernary(inst->opcode, first, second, third, xsink);
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
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
                    res = evalAndRef(expr_inst->expr, xsink);
                } else if (inst->operands.size() < 4) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "quaternary op missing operands");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
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
                    cleanupLocalCaches();
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
                    cleanupLocalCaches();
                    return false;
                }
                setValueSlot(values, lval_inst->result.id, res, xsink);
                if (res.hasNode()) {
                    cleanup.push_back(lval_inst->result.id);
                }
                updateLocalVarFromLvalue(instantiated_locals, lval_inst->lvalue, res, xsink, pre_instantiated, function_own_locals, &locally_uninstantiated,
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
                    cleanupLocalCaches();
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
                // Hold an explicit reference to the RHS value through
                // pre-invalidation.  clearLoadSlots below discards values[]
                // entries for the target variable, which may include the RHS
                // operand if it was loaded from the same variable (e.g.,
                // h.b = h).  Without this extra ref, the hash's refcount
                // drops to 1 before LValueHelper is created, so
                // ensureUnique() thinks it's unique and skips the COW copy,
                // creating a circular self-reference.
                ValueHolder val_holder(val.refSelf(), xsink);
                // Targeted pre-invalidation BEFORE the lvalue operation so that
                // LValueHelper::ensureUnique() sees the variable's natural
                // refcount and only triggers COW when truly necessary.
                // See design/lvalue-loads-in-ir.md for the full invariant.
                // Only invalidate the specific local variable's cache entry and values[]
                // entries — StoreLValue doesn't call external code, so other caches are safe.
                if (lval_inst->hasLocalTarget()) {
                    // Fast path: pre-computed slot_id for the lvalue target
                    if (lval_inst->lvalue_slot_id < locals_slot_cache.size()) {
                        locals_slot_cache[lval_inst->lvalue_slot_id].discard(xsink);
                        locals_slot_cache[lval_inst->lvalue_slot_id] = QoreValue();
                    }
                    // Also discard values[] entries from LoadLocal results for the
                    // target variable.  These hold +1 references (from guard loads
                    // or prior reads) that inflate the refcount beyond the caches.
                    clearLoadSlots(lval_inst->lvalue_slot_id);
                } else if (lval_inst->lvalue_slot_id == UINT32_MAX) {
                    // Unresolved target: try runtime lookup as safety fallback
                    if (base_var && base_var->ref.id) {
                        auto bv_slot_it = func.local_var_slots.find(
                            reinterpret_cast<const LocalVar*>(base_var->ref.id));
                        if (bv_slot_it != func.local_var_slots.end()) {
                            uint32_t sid = bv_slot_it->second;
                            if (sid < locals_slot_cache.size()) {
                                locals_slot_cache[sid].discard(xsink);
                                locals_slot_cache[sid] = QoreValue();
                            }
                            clearLoadSlots(sid);
                        }
                    } else {
                        // Truly unknown lvalue target - full slot cache wipe for safety
                        for (size_t i = 0; i < locals_slot_cache.size(); ++i) {
                            locals_slot_cache[i].discard(xsink);
                            locals_slot_cache[i] = QoreValue();
                        }
                    }
                }
                // else: LVALUE_NON_LOCAL — target is a member/global/static variable,
                // no local cache invalidation needed
                QoreValue res = QoreIRInterpreter::evalLValueStore(lval_inst->lvalue, val, xsink,
                    lval_inst->weak);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }

                // After a reference write-through, the target variable's slot cache may be stale.
                // We cannot know statically which variable was modified, so flush all local caches.
                // Mirrors StoreLocal's cleanupLocalCaches() at lines 4146-4148.
                if (base_var && base_var->getTypeInfo()
                        && QoreTypeInfo::isReference(base_var->getTypeInfo())) {
                    cleanupLocalCaches();
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
                // Targeted cache invalidation before the lvalue operation
                if (lval_inst->hasLocalTarget()) {
                    if (lval_inst->lvalue_slot_id < locals_slot_cache.size()) {
                        locals_slot_cache[lval_inst->lvalue_slot_id].discard(xsink);
                        locals_slot_cache[lval_inst->lvalue_slot_id] = QoreValue();
                    }
                    clearLoadSlots(lval_inst->lvalue_slot_id);
                } else if (lval_inst->lvalue_slot_id == UINT32_MAX) {
                    // Unresolved lvalue target - full slot cache wipe for safety
                    for (size_t i = 0; i < locals_slot_cache.size(); ++i) {
                        locals_slot_cache[i].discard(xsink);
                        locals_slot_cache[i] = QoreValue();
                    }
                }
                // else: LVALUE_NON_LOCAL — no local cache invalidation needed
                // evalLValueUnary returns the old value for post-inc/dec, new value for pre-inc/dec.
                // It also modifies the lvalue in place via LValueHelper, so no reload is needed.
                // The slot cache was already pre-invalidated above; the next LoadLocal will
                // re-populate it from the updated variable.
                QoreValue res = QoreIRInterpreter::evalLValueUnary(inst->opcode, lval_inst->lvalue, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                setValueSlot(values, lval_inst->result.id, res, xsink);
                if (res.hasNode()) {
                    cleanup.push_back(lval_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::ShiftLValue: {
                auto* lval_inst = static_cast<QoreIRLValueInstruction*>(inst);
                // Targeted cache invalidation before the lvalue operation
                if (lval_inst->hasLocalTarget()) {
                    if (lval_inst->lvalue_slot_id < locals_slot_cache.size()) {
                        locals_slot_cache[lval_inst->lvalue_slot_id].discard(xsink);
                        locals_slot_cache[lval_inst->lvalue_slot_id] = QoreValue();
                    }
                    clearLoadSlots(lval_inst->lvalue_slot_id);
                } else if (lval_inst->lvalue_slot_id == UINT32_MAX) {
                    // Unresolved lvalue target - full slot cache wipe for safety
                    for (size_t i = 0; i < locals_slot_cache.size(); ++i) {
                        locals_slot_cache[i].discard(xsink);
                        locals_slot_cache[i] = QoreValue();
                    }
                }
                // else: LVALUE_NON_LOCAL — no local cache invalidation needed
                QoreValue res = QoreIRInterpreter::evalLValueUnary(inst->opcode, lval_inst->lvalue, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
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
            case QoreIROpcode::AddAssignLValue: {
                // Inlined int fast path: skip evalLValueBinary/evalPlusEquals/SafeDerefHelper
                auto* lval_inst = static_cast<QoreIRLValueInstruction*>(inst);
                if (lval_inst->operands.empty()) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "lvalue binary op missing operand");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                QoreValue right = getIRValue(values, lval_inst->operands[0]);
                // Targeted cache invalidation BEFORE the lvalue operation
                if (lval_inst->hasLocalTarget()) {
                    if (lval_inst->lvalue_slot_id < locals_slot_cache.size()) {
                        locals_slot_cache[lval_inst->lvalue_slot_id].discard(xsink);
                        locals_slot_cache[lval_inst->lvalue_slot_id] = QoreValue();
                    }
                    clearLoadSlots(lval_inst->lvalue_slot_id);
                } else if (lval_inst->lvalue_slot_id == UINT32_MAX) {
                    for (size_t i = 0; i < locals_slot_cache.size(); ++i) {
                        locals_slot_cache[i].discard(xsink);
                        locals_slot_cache[i] = QoreValue();
                    }
                }
                // Int fast path: probe type under lock, do direct operation if int
                QoreValue res;
                bool fast_path_done = false;
                {
                    LValueHelper v(lval_inst->lvalue, xsink);
                    if (v && v.getType() == NT_INT) {
                        // Direct int plus-equals — no SafeDerefHelper, no type dispatch
                        v.plusEqualsBigInt(right.getAsBigInt());
                        if (!*xsink) {
                            res = v.getReferencedValue();
                        }
                        fast_path_done = true;
                    }
                }
                // Non-int: fall back to full evalPlusEquals (LValueHelper released above)
                if (!fast_path_done && !*xsink) {
                    res = evalPlusEquals(lval_inst->lvalue, right, xsink);
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                setValueSlot(values, lval_inst->result.id, res, xsink);
                if (res.hasNode()) {
                    cleanup.push_back(lval_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::SubAssignLValue:
            case QoreIROpcode::MulAssignLValue:
            case QoreIROpcode::DivAssignLValue:
            case QoreIROpcode::ModAssignLValue:
            case QoreIROpcode::AndAssignLValue:
            case QoreIROpcode::OrAssignLValue:
            case QoreIROpcode::XorAssignLValue:
            case QoreIROpcode::ShlAssignLValue:
            case QoreIROpcode::ShrAssignLValue: {
                // Inlined int fast path for arithmetic compound assignments
                auto* lval_inst = static_cast<QoreIRLValueInstruction*>(inst);
                if (lval_inst->operands.empty()) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "lvalue binary op missing operand");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                QoreValue right = getIRValue(values, lval_inst->operands[0]);
                // Targeted cache invalidation BEFORE the lvalue operation
                if (lval_inst->hasLocalTarget()) {
                    if (lval_inst->lvalue_slot_id < locals_slot_cache.size()) {
                        locals_slot_cache[lval_inst->lvalue_slot_id].discard(xsink);
                        locals_slot_cache[lval_inst->lvalue_slot_id] = QoreValue();
                    }
                    clearLoadSlots(lval_inst->lvalue_slot_id);
                } else if (lval_inst->lvalue_slot_id == UINT32_MAX) {
                    for (size_t i = 0; i < locals_slot_cache.size(); ++i) {
                        locals_slot_cache[i].discard(xsink);
                        locals_slot_cache[i] = QoreValue();
                    }
                }
                // Int fast path: probe type under lock, do direct operation if int
                QoreValue res;
                bool fast_path_done = false;
                {
                    LValueHelper v(lval_inst->lvalue, xsink);
                    if (v && v.getType() == NT_INT) {
                        int64 rhs = right.getAsBigInt();
                        switch (inst->opcode) {
                            case QoreIROpcode::SubAssignLValue:
                                v.minusEqualsBigInt(rhs);
                                break;
                            case QoreIROpcode::MulAssignLValue:
                                v.multiplyEqualsBigInt(rhs);
                                break;
                            case QoreIROpcode::DivAssignLValue:
                                v.divideEqualsBigInt(rhs);
                                break;
                            case QoreIROpcode::ModAssignLValue:
                                v.modulaEqualsBigInt(rhs);
                                break;
                            case QoreIROpcode::AndAssignLValue:
                                v.andEqualsBigInt(rhs);
                                break;
                            case QoreIROpcode::OrAssignLValue:
                                v.orEqualsBigInt(rhs);
                                break;
                            case QoreIROpcode::XorAssignLValue:
                                v.xorEqualsBigInt(rhs);
                                break;
                            case QoreIROpcode::ShlAssignLValue:
                                v.shiftLeftEqualsBigInt(rhs);
                                break;
                            case QoreIROpcode::ShrAssignLValue:
                                v.shiftRightEqualsBigInt(rhs);
                                break;
                            default:
                                assert(false);
                                break;
                        }
                        if (!*xsink) {
                            res = v.getReferencedValue();
                        }
                        fast_path_done = true;
                    }
                }
                // Non-int: fall back to full evalLValueBinary (LValueHelper released above)
                if (!fast_path_done && !*xsink) {
                    res = evalLValueBinary(inst->opcode, lval_inst->lvalue, right, xsink);
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                setValueSlot(values, lval_inst->result.id, res, xsink);
                if (res.hasNode()) {
                    cleanup.push_back(lval_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::UnshiftLValue: {
                // UnshiftLValue is a list operation — no int fast path
                auto* lval_inst = static_cast<QoreIRLValueInstruction*>(inst);
                if (lval_inst->operands.empty()) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "lvalue binary op missing operand");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                QoreValue right = getIRValue(values, lval_inst->operands[0]);
                // Targeted cache invalidation BEFORE the lvalue operation to prevent
                // COW from inflated refcounts.  See design/lvalue-loads-in-ir.md.
                if (lval_inst->hasLocalTarget()) {
                    // Local variable: only invalidate this variable's slot cache entry
                    // and any values[] entries loaded from this variable
                    if (lval_inst->lvalue_slot_id < locals_slot_cache.size()) {
                        locals_slot_cache[lval_inst->lvalue_slot_id].discard(xsink);
                        locals_slot_cache[lval_inst->lvalue_slot_id] = QoreValue();
                    }
                    clearLoadSlots(lval_inst->lvalue_slot_id);
                } else if (lval_inst->lvalue_slot_id == UINT32_MAX) {
                    // Unresolved lvalue target - full slot cache wipe for safety
                    for (size_t i = 0; i < locals_slot_cache.size(); ++i) {
                        locals_slot_cache[i].discard(xsink);
                        locals_slot_cache[i] = QoreValue();
                    }
                }
                // else: LVALUE_NON_LOCAL — no local cache invalidation needed
                QoreValue res = QoreIRInterpreter::evalLValueBinary(inst->opcode, lval_inst->lvalue, right, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
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
                    cleanupLocalCaches();
                    return false;
                }
                QoreValue first = getIRValue(values, lval_inst->operands[0]);
                QoreValue second = getIRValue(values, lval_inst->operands[1]);
                QoreValue third = getIRValue(values, lval_inst->operands[2]);
                // Targeted pre-invalidation BEFORE the lvalue operation (see design/lvalue-loads-in-ir.md)
                if (lval_inst->hasLocalTarget()) {
                    if (lval_inst->lvalue_slot_id < locals_slot_cache.size()) {
                        locals_slot_cache[lval_inst->lvalue_slot_id].discard(xsink);
                        locals_slot_cache[lval_inst->lvalue_slot_id] = QoreValue();
                    }
                    clearLoadSlots(lval_inst->lvalue_slot_id);
                } else if (lval_inst->lvalue_slot_id == UINT32_MAX) {
                    // Unresolved lvalue target - full slot cache wipe for safety
                    for (size_t i = 0; i < locals_slot_cache.size(); ++i) {
                        locals_slot_cache[i].discard(xsink);
                        locals_slot_cache[i] = QoreValue();
                    }
                }
                // else: LVALUE_NON_LOCAL — no local cache invalidation needed
                QoreValue res = QoreIRInterpreter::evalLValueTernary(inst->opcode, lval_inst->lvalue, first, second,
                    third, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                setValueSlot(values, lval_inst->result.id, res, xsink);
                if (res.hasNode()) {
                    cleanup.push_back(lval_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::CallDirect: {
                // Fast path: bypass AST for resolved user functions with IR/JIT
                auto* direct_inst = static_cast<QoreIRCallDirectInstruction*>(inst);
                if (direct_inst->variant) {
                    int nargs = static_cast<int>(direct_inst->operands.size());
                    // Build NaN-boxed args array from operands
                    constexpr int SMALL_BUF = 8;
                    uint64_t nb_buf[SMALL_BUF];
                    uint64_t* nanboxed_args = nargs <= SMALL_BUF ? nb_buf : new uint64_t[nargs];
                    for (int i = 0; i < nargs; ++i) {
                        nanboxed_args[i] = toBits(getIRValue(values, direct_inst->operands[i]));
                    }

                    // Inline IR-to-IR call: skip qore_rt_call_fast overhead entirely
                    // when the callee has direct_params_eligible IR.
                    // Eliminates: check_stack, ThreadFrameBoundaryHelper, ArgvContextHelper,
                    // param TLS push/pop, execJITWithDeopt wrapper, name string construction.
                    int8_t state = direct_inst->inline_ir_state.load(std::memory_order_acquire);
                    if (state == 0) {
                        // First call: check eligibility and cache result
                        const UserVariantBase* uvb = direct_inst->variant->getUserVariantBase();
                        if (uvb) {
                            const QoreIRFunction* callee_ir = uvb->getCachedIR();
                            const UserSignature* sig = uvb->getUserSignature();
                            // Don't cache inline IR calls to functions from different programs/modules
                            // to avoid stale pointers when modules are unloaded (especially dynamically-loaded ones)
                            // Only inline if caller and callee are in the same program
                            bool same_program = (uvb->pgm == direct_inst->pgm);
                            if (same_program && callee_ir && callee_ir->direct_params_eligible
                                    && !uvb->hasCachedFunction()
                                    && !direct_inst->has_ref_args
                                    && callee_ir->ast_visible_body_locals.empty()
                                    && nargs >= static_cast<int>(sig->numParams())) {
                                direct_inst->cached_callee_ir = callee_ir;
                                direct_inst->cached_uvb = uvb;
                                direct_inst->cached_return_type = sig->getReturnTypeInfo();
                                direct_inst->inline_ir_state.store(1, std::memory_order_release);
                                state = 1;
                            } else {
                                direct_inst->inline_ir_state.store(-1, std::memory_order_release);
                                state = -1;
                            }
                        } else {
                            direct_inst->inline_ir_state.store(-1, std::memory_order_release);
                            state = -1;
                        }
                    }

                    QoreValue res;
                    if (state == 1) {
                        // Ultra-fast inline IR-to-IR call path
                        const QoreIRFunction* callee_ir = direct_inst->cached_callee_ir;
                        const UserVariantBase* uvb = direct_inst->cached_uvb;
                        // Re-check: callee may have been promoted to JIT since caching
                        if (uvb->hasCachedFunction()) {
                            // JIT promotion: invalidate cache and use standard path
                            direct_inst->inline_ir_state.store(-1, std::memory_order_release);
                            res = fromBits(qore_rt_call_fast(
                                direct_inst->func, direct_inst->variant, direct_inst->pgm,
                                nanboxed_args, nargs, xsink));
                        } else {
                            // Instantiate argvid (always NOTHING for exact-arity calls)
                            const UserSignature* sig = uvb->getUserSignature();
                            if (sig->argvid) {
                                sig->argvid->instantiate(QoreValue());
                            }
                            IRDirectParams dp{nanboxed_args, nargs};
                            QoreValue ir_return_value;
                            bool ok = QoreIRInterpreter::execute(*callee_ir, ir_return_value, xsink,
                                nullptr, nullptr, nullptr, callee_ir->cached_pre_instantiated,
                                nullptr, uvb->getStatementBlock(), uvb->pgm, false, &dp);
                            if (sig->argvid) {
                                sig->argvid->uninstantiate(xsink);
                            }
                            if (!ok && !(xsink && *xsink)) {
                                // IR deopt: fall through to standard path
                                direct_inst->inline_ir_state.store(-1, std::memory_order_release);
                                res = fromBits(qore_rt_call_fast(
                                    direct_inst->func, direct_inst->variant, direct_inst->pgm,
                                    nanboxed_args, nargs, xsink));
                            } else {
                                // Apply return type coercion
                                if (!(xsink && *xsink) && direct_inst->cached_return_type) {
                                    QoreTypeInfo::acceptAssignment(direct_inst->cached_return_type,
                                        "<return statement>", ir_return_value, xsink);
                                }
                                res = ir_return_value;
                            }
                        }
                    } else {
                        // Standard path through qore_rt_call_fast
                        res = fromBits(qore_rt_call_fast(
                            direct_inst->func, direct_inst->variant, direct_inst->pgm,
                            nanboxed_args, nargs, xsink));
                    }

                    if (nargs > SMALL_BUF) { delete[] nanboxed_args; }
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    // If call has reference args, callee may have modified caller's locals
                    if (direct_inst->has_ref_args) {
                        cleanupLocalCaches();
                    } else {
                        invalidateExternalCaches();
                    }
                    setValueSlot(values, inst->result.id, res, xsink);
                    if (res.hasNode()) {
                        cleanup.push_back(inst->result.id);
                    }
                    ++ip;
                    break;
                }
                // Fall through to slow path when variant is not resolved
            }
            // fallthrough
            case QoreIROpcode::CallStaticDirect: {
                // Fast path: bypass AST for resolved static method calls with IR/JIT
                auto* static_inst = static_cast<QoreIRCallStaticDirectInstruction*>(inst);
                if (static_inst->variant) {
                    int nargs = static_cast<int>(static_inst->operands.size());
                    constexpr int SMALL_BUF = 8;
                    uint64_t nb_buf[SMALL_BUF];
                    uint64_t* nanboxed_args = nargs <= SMALL_BUF ? nb_buf : new uint64_t[nargs];
                    for (int i = 0; i < nargs; ++i) {
                        nanboxed_args[i] = toBits(getIRValue(values, static_inst->operands[i]));
                    }
                    QoreValue res = fromBits(qore_rt_call_static_method_direct(
                        static_inst->method, static_inst->variant,
                        nanboxed_args, nargs, xsink));
                    if (nargs > SMALL_BUF) { delete[] nanboxed_args; }
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    if (static_inst->has_ref_args) {
                        cleanupLocalCaches();
                    } else {
                        invalidateExternalCaches();
                    }
                    setValueSlot(values, inst->result.id, res, xsink);
                    if (res.hasNode()) {
                        cleanup.push_back(inst->result.id);
                    }
                    ++ip;
                    break;
                }
                // Fall through to slow path when variant is not resolved
            }
            // fallthrough
            case QoreIROpcode::Call:
            case QoreIROpcode::CallIndirect:
            case QoreIROpcode::CallMethod:
            case QoreIROpcode::CallStatic: {
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
                // Native operator handling: [] and .{} with pre-evaluated operands
                // These are not function calls — operands are container+index, not args
                if (!inst->operands.empty() && effective_opcode == QoreIROpcode::Call
                        && inst->operands.size() == 2 && call_expr.hasNode()) {
                    if (auto* sq = dynamic_cast<const QoreSquareBracketsOperatorNode*>(
                            call_expr.getInternalNode())) {
                        QoreValue lhs_val = getIRValue(values, inst->operands[0]);
                        QoreValue rhs_val = getIRValue(values, inst->operands[1]);
                        res = QoreSquareBracketsOperatorNode::doSquareBrackets(
                            lhs_val, rhs_val, true, xsink);
                        used_operands = true;
                    } else if (auto* hod = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(
                            call_expr.getInternalNode())) {
                        QoreValue lhs_val = getIRValue(values, inst->operands[0]);
                        QoreValue rhs_val = getIRValue(values, inst->operands[1]);
                        qore_type_t lt = lhs_val.getType();
                        if (lt == NT_HASH) {
                            const QoreHashNode* h = lhs_val.get<const QoreHashNode>();
                            if (rhs_val.getType() == NT_LIST) {
                                res = qore_hash_private::get(*h)->getSlice(
                                    rhs_val.get<const QoreListNode>(), xsink);
                            } else {
                                QoreStringNodeValueHelper key(rhs_val);
                                res = h->getKeyValue(**key, xsink);
                                if (!(xsink && *xsink)) {
                                    res = res.refSelf();
                                }
                            }
                        } else if (lt == NT_OBJECT) {
                            QoreObject* o = const_cast<QoreObject*>(lhs_val.get<const QoreObject>());
                            if (rhs_val.getType() == NT_LIST) {
                                res = o->getSlice(rhs_val.get<const QoreListNode>(), xsink);
                            } else {
                                QoreStringNodeValueHelper key(rhs_val);
                                res = o->evalMember(*key, xsink);
                            }
                        }
                        used_operands = true;
                    }
                }
                if (!inst->operands.empty() && !used_operands) {
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
                        cleanupLocalCaches();
                        return false;
                    }
                    if (effective_opcode == QoreIROpcode::Call) {
                        if (auto* call = dynamic_cast<const FunctionCallNode*>(call_expr.getInternalNode())) {
                            // Direct evalImpl() — avoids evalExprNode() overhead
                            FunctionCallNode clone(*call, arg_list);
                            res = evalAndRef(&clone, xsink);
                            used_operands = true;
                        }
                    } else if (effective_opcode == QoreIROpcode::CallMethod) {
                        if (auto* call = dynamic_cast<const SelfFunctionCallNode*>(call_expr.getInternalNode())) {
                            // Direct evalImpl() — avoids evalExprNode() overhead
                            SelfFunctionCallNode clone(*call, arg_list);
                            res = evalAndRef(&clone, xsink);
                            used_operands = true;
                        }
                    } else if (effective_opcode == QoreIROpcode::CallStatic) {
                        if (auto* call = dynamic_cast<const StaticMethodCallNode*>(call_expr.getInternalNode())) {
                            // Direct evalImpl() — avoids evalExprNode() overhead
                            StaticMethodCallNode clone(*call, arg_list);
                            res = evalAndRef(&clone, xsink);
                            used_operands = true;
                        }
                    } else {
                        if (auto* call = dynamic_cast<const CallReferenceCallNode*>(
                            call_expr.getInternalNode())) {
                            QoreValue exp = call->getExp();
                            if (exp.hasNode()) {
                                exp = exp.refSelf();
                            }
                            // Direct evalImpl() — avoids evalExprNode() overhead
                            CallReferenceCallNode clone(loc, exp, arg_list);
                            res = evalAndRef(&clone, xsink);
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
                            auto slot_it = func.local_var_slots.find(var_new_obj->ref.id);
                            if (slot_it != func.local_var_slots.end()
                                    && slot_it->second < local_init_slots.size()) {
                                local_init_slots[slot_it->second] = inst->result.id;
                            }
                        }
                    }
                    // ScopedObjectCallNode: bare "new ClassName(args)" — construct directly
                    if (call_expr.hasNode()) {
                        auto* scoped = dynamic_cast<const ScopedObjectCallNode*>(
                            call_expr.getInternalNode());
                        if (scoped && scoped->oc) {
                            RuntimeConfig& rc = rc_get_current_ref();
                            res = qore_class_private::execConstructor(*scoped->oc, rc,
                                scoped->getVariant(), scoped->getArgs(), xsink);
                            used_operands = true;
                        }
                    }
                    if (!used_operands) {
                        // Direct eval — avoids evalExprNode() overhead
                        res = evalAndRef(call_expr, xsink);
                    }
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                // Function call runs in its own frame and cannot modify caller's locals
                invalidateExternalCaches();
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

                // Fast path: bypass QoreListNode when variant is resolved
                if (direct_inst->variant) {
                    int nargs = static_cast<int>(direct_inst->operands.size());
                    constexpr int SMALL_BUF = 8;
                    uint64_t nb_buf[SMALL_BUF];
                    uint64_t* nanboxed_args = nargs <= SMALL_BUF ? nb_buf : new uint64_t[nargs];
                    for (int i = 0; i < nargs; ++i) {
                        nanboxed_args[i] = toBits(getIRValue(values, direct_inst->operands[i]));
                    }
                    QoreValue res = fromBits(qore_rt_call_method_fast(
                        direct_inst->method, direct_inst->variant,
                        nanboxed_args, nargs, xsink));
                    if (nargs > SMALL_BUF) { delete[] nanboxed_args; }
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    if (direct_inst->has_ref_args) {
                        cleanupLocalCaches();
                    } else {
                        // Skip invalidation for built-in methods without reference arguments
                        if (!direct_inst->method->isBuiltin()) {
                            invalidateExternalCaches();
                        }
                    }
                    setValueSlot(values, direct_inst->result.id, res, xsink);
                    if (res.hasNode()) {
                        cleanup.push_back(direct_inst->result.id);
                    }
                    ++ip;
                    break;
                }

                // Slow path: variant not resolved
                const QoreMethod* method = direct_inst->method;

                // Get self object from runtime stack
                QoreObject* self = runtime_get_stack_object();
                if (!self) {
                    if (xsink) {
                        xsink->raiseException("IR-INTERPRETER-ERROR",
                            "no self object in direct method call");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
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
                // Skip invalidation for built-in methods without reference arguments
                if (!method->isBuiltin() || direct_inst->has_ref_args) {
                    invalidateExternalCaches();
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

                // Fast path: bypass QoreListNode when variant is resolved
                if (invoke_inst->variant) {
                    int nargs = static_cast<int>(invoke_inst->operands.size());
                    constexpr int SMALL_BUF = 8;
                    uint64_t nb_buf[SMALL_BUF];
                    uint64_t* nanboxed_args = nargs <= SMALL_BUF ? nb_buf : new uint64_t[nargs];
                    for (int i = 0; i < nargs; ++i) {
                        nanboxed_args[i] = toBits(getIRValue(values, invoke_inst->operands[i]));
                    }
                    QoreValue res = fromBits(qore_rt_call_method_fast(
                        invoke_inst->method, invoke_inst->variant,
                        nanboxed_args, nargs, xsink));
                    if (nargs > SMALL_BUF) { delete[] nanboxed_args; }
                    if (invoke_inst->has_ref_args) {
                        cleanupLocalCaches();
                    } else {
                        // For built-in methods without reference arguments that are known to not
                        // access Qore global state (like Future::isDone(), Counter::dec(), etc.),
                        // skip invalidation to avoid expensive hash map operations on hot paths
                        if (invoke_inst->method->isBuiltin()) {
                            // Built-in methods are pure C++ and don't access Qore globals
                            // No invalidation needed
                        } else {
                            invalidateExternalCaches();
                        }
                    }
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
                    prev_block = block;
                    block = invoke_inst->normal_target;
                    ip = 0;
                    break;
                }

                // Slow path: variant not resolved
                const QoreMethod* method = invoke_inst->method;

                // Get self object from runtime stack
                QoreObject* self = runtime_get_stack_object();
                if (!self) {
                    if (xsink) {
                        xsink->raiseException("IR-INTERPRETER-ERROR",
                            "no self object in invoke method direct");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
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

                // Method call runs in its own frame and cannot modify caller's locals
                // Skip invalidation for built-in methods without reference arguments
                // (built-in methods are pure C++ and don't access Qore global state)
                if (!method->isBuiltin() || invoke_inst->has_ref_args) {
                    invalidateExternalCaches();
                }

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
                constexpr int SMALL_BUF = 8;
                uint64_t nb_buf[SMALL_BUF];
                uint64_t* nanboxed_args = nargs <= SMALL_BUF ? nb_buf : new uint64_t[nargs];
                for (int i = 0; i < nargs; ++i) {
                    nanboxed_args[i] = toBits(getIRValue(values, direct_inst->operands[i + 1]));
                }

                QoreValue res;
                bool called_external = false;  // Track if we called external code
                if (direct_inst->pseudo) {
                    // Fast-path optimizations for common pseudo-methods
                    const char* method_name = direct_inst->method->getName();
                    qore_type_t base_type = base.getType();

                    if (!strcmp(method_name, "typeCode") && nargs == 0) {
                        // Inline: return type code constant
                        res = QoreValue(static_cast<int64_t>(base_type));
                    } else if (!strcmp(method_name, "size") && nargs == 0) {
                        // Inline: size() for lists and strings
                        if (base_type == NT_LIST) {
                            const QoreListNode* l = base.get<const QoreListNode>();
                            res = QoreValue(static_cast<int64_t>(l->size()));
                        } else if (base_type == NT_STRING) {
                            const QoreStringNode* s = base.get<const QoreStringNode>();
                            res = QoreValue(static_cast<int64_t>(s->strlen()));
                        } else if (base_type == NT_HASH) {
                            const QoreHashNode* h = base.get<const QoreHashNode>();
                            res = QoreValue(static_cast<int64_t>(h->size()));
                        } else {
                            // Unsupported type, use runtime dispatch
                            called_external = true;
                            res = fromBits(qore_rt_dot_eval_pseudo_method_direct(
                                toBits(base), direct_inst->method, direct_inst->qc, direct_inst->variant,
                                nanboxed_args, nargs, xsink));
                        }
                    } else if ((!strcmp(method_name, "strlen") || !strcmp(method_name, "length")) && nargs == 0) {
                        // Inline: strlen()/length() for strings
                        if (base_type == NT_STRING) {
                            const QoreStringNode* s = base.get<const QoreStringNode>();
                            res = QoreValue(static_cast<int64_t>(s->strlen()));
                        } else {
                            // Unsupported type, use runtime dispatch
                            called_external = true;
                            res = fromBits(qore_rt_dot_eval_pseudo_method_direct(
                                toBits(base), direct_inst->method, direct_inst->qc, direct_inst->variant,
                                nanboxed_args, nargs, xsink));
                        }
                    } else if (!strcmp(method_name, "empty") && nargs == 0) {
                        // Inline: empty() for lists and strings
                        if (base_type == NT_LIST) {
                            const QoreListNode* l = base.get<const QoreListNode>();
                            res = QoreValue(l->empty() ? true : false);
                        } else if (base_type == NT_STRING) {
                            const QoreStringNode* s = base.get<const QoreStringNode>();
                            res = QoreValue(s->strlen() == 0 ? true : false);
                        } else if (base_type == NT_HASH) {
                            const QoreHashNode* h = base.get<const QoreHashNode>();
                            res = QoreValue(h->empty() ? true : false);
                        } else {
                            // Unsupported type, use runtime dispatch
                            called_external = true;
                            res = fromBits(qore_rt_dot_eval_pseudo_method_direct(
                                toBits(base), direct_inst->method, direct_inst->qc, direct_inst->variant,
                                nanboxed_args, nargs, xsink));
                        }
                    } else if (!strcmp(method_name, "val") && nargs == 0) {
                        // Inline: val() for lists and strings (opposite of empty)
                        if (base_type == NT_LIST) {
                            const QoreListNode* l = base.get<const QoreListNode>();
                            res = QoreValue(l->empty() ? false : true);
                        } else if (base_type == NT_STRING) {
                            const QoreStringNode* s = base.get<const QoreStringNode>();
                            res = QoreValue(s->strlen() == 0 ? false : true);
                        } else if (base_type == NT_HASH) {
                            const QoreHashNode* h = base.get<const QoreHashNode>();
                            res = QoreValue(h->empty() ? false : true);
                        } else {
                            // Unsupported type, use runtime dispatch
                            called_external = true;
                            res = fromBits(qore_rt_dot_eval_pseudo_method_direct(
                                toBits(base), direct_inst->method, direct_inst->qc, direct_inst->variant,
                                nanboxed_args, nargs, xsink));
                        }
                    } else if (!strcmp(method_name, "type") && nargs == 0) {
                        // Inline: type() - return type name string
                        res = QoreValue(new QoreStringNode(base.getTypeName()));
                    } else {
                        // Unsupported pseudo-method, use generic runtime dispatch
                        called_external = true;
                        res = fromBits(qore_rt_dot_eval_pseudo_method_direct(
                            toBits(base), direct_inst->method, direct_inst->qc, direct_inst->variant,
                            nanboxed_args, nargs, xsink));
                    }
                } else {
                    called_external = true;
                    res = fromBits(qore_rt_dot_eval_method_direct(
                        toBits(base), direct_inst->method, direct_inst->qc, direct_inst->variant,
                        nanboxed_args, nargs, xsink));
                }
                if (nargs > SMALL_BUF) { delete[] nanboxed_args; }

                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                // Method call runs in its own frame and cannot modify caller's locals
                // Only invalidate if we actually called external code
                if (called_external) {
                    invalidateExternalCaches();
                }
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
                constexpr int SMALL_BUF = 8;
                uint64_t nb_buf[SMALL_BUF];
                uint64_t* nanboxed_args = nargs <= SMALL_BUF ? nb_buf : new uint64_t[nargs];
                for (int i = 0; i < nargs; ++i) {
                    nanboxed_args[i] = toBits(getIRValue(values, de_invoke_inst->operands[i + 1]));
                }

                QoreValue res;
                bool called_external = false;  // Track if we called external code
                if (de_invoke_inst->pseudo) {
                    // Fast-path optimizations for common pseudo-methods
                    const char* method_name = de_invoke_inst->method->getName();
                    qore_type_t base_type = base.getType();

                    if (!strcmp(method_name, "typeCode") && nargs == 0) {
                        // Inline: return type code constant
                        res = QoreValue(static_cast<int64_t>(base_type));
                    } else if (!strcmp(method_name, "size") && nargs == 0) {
                        // Inline: size() for lists and strings
                        if (base_type == NT_LIST) {
                            const QoreListNode* l = base.get<const QoreListNode>();
                            res = QoreValue(static_cast<int64_t>(l->size()));
                        } else if (base_type == NT_STRING) {
                            const QoreStringNode* s = base.get<const QoreStringNode>();
                            res = QoreValue(static_cast<int64_t>(s->strlen()));
                        } else if (base_type == NT_HASH) {
                            const QoreHashNode* h = base.get<const QoreHashNode>();
                            res = QoreValue(static_cast<int64_t>(h->size()));
                        } else {
                            // Unsupported type, use runtime dispatch
                            called_external = true;
                            res = fromBits(qore_rt_dot_eval_pseudo_method_direct(
                                toBits(base), de_invoke_inst->method, de_invoke_inst->qc, de_invoke_inst->variant,
                                nanboxed_args, nargs, xsink));
                        }
                    } else if ((!strcmp(method_name, "strlen") || !strcmp(method_name, "length")) && nargs == 0) {
                        // Inline: strlen()/length() for strings
                        if (base_type == NT_STRING) {
                            const QoreStringNode* s = base.get<const QoreStringNode>();
                            res = QoreValue(static_cast<int64_t>(s->strlen()));
                        } else {
                            // Unsupported type, use runtime dispatch
                            called_external = true;
                            res = fromBits(qore_rt_dot_eval_pseudo_method_direct(
                                toBits(base), de_invoke_inst->method, de_invoke_inst->qc, de_invoke_inst->variant,
                                nanboxed_args, nargs, xsink));
                        }
                    } else if (!strcmp(method_name, "empty") && nargs == 0) {
                        // Inline: empty() for lists and strings
                        if (base_type == NT_LIST) {
                            const QoreListNode* l = base.get<const QoreListNode>();
                            res = QoreValue(l->empty() ? true : false);
                        } else if (base_type == NT_STRING) {
                            const QoreStringNode* s = base.get<const QoreStringNode>();
                            res = QoreValue(s->strlen() == 0 ? true : false);
                        } else if (base_type == NT_HASH) {
                            const QoreHashNode* h = base.get<const QoreHashNode>();
                            res = QoreValue(h->empty() ? true : false);
                        } else {
                            // Unsupported type, use runtime dispatch
                            called_external = true;
                            res = fromBits(qore_rt_dot_eval_pseudo_method_direct(
                                toBits(base), de_invoke_inst->method, de_invoke_inst->qc, de_invoke_inst->variant,
                                nanboxed_args, nargs, xsink));
                        }
                    } else if (!strcmp(method_name, "val") && nargs == 0) {
                        // Inline: val() for lists and strings (opposite of empty)
                        if (base_type == NT_LIST) {
                            const QoreListNode* l = base.get<const QoreListNode>();
                            res = QoreValue(l->empty() ? false : true);
                        } else if (base_type == NT_STRING) {
                            const QoreStringNode* s = base.get<const QoreStringNode>();
                            res = QoreValue(s->strlen() == 0 ? false : true);
                        } else if (base_type == NT_HASH) {
                            const QoreHashNode* h = base.get<const QoreHashNode>();
                            res = QoreValue(h->empty() ? false : true);
                        } else {
                            // Unsupported type, use runtime dispatch
                            called_external = true;
                            res = fromBits(qore_rt_dot_eval_pseudo_method_direct(
                                toBits(base), de_invoke_inst->method, de_invoke_inst->qc, de_invoke_inst->variant,
                                nanboxed_args, nargs, xsink));
                        }
                    } else if (!strcmp(method_name, "type") && nargs == 0) {
                        // Inline: type() - return type name string
                        res = QoreValue(new QoreStringNode(base.getTypeName()));
                    } else {
                        // Unsupported pseudo-method, use generic runtime dispatch
                        called_external = true;
                        res = fromBits(qore_rt_dot_eval_pseudo_method_direct(
                            toBits(base), de_invoke_inst->method, de_invoke_inst->qc, de_invoke_inst->variant,
                            nanboxed_args, nargs, xsink));
                    }
                } else {
                    called_external = true;
                    res = fromBits(qore_rt_dot_eval_method_direct(
                        toBits(base), de_invoke_inst->method, de_invoke_inst->qc, de_invoke_inst->variant,
                        nanboxed_args, nargs, xsink));
                }
                if (nargs > SMALL_BUF) { delete[] nanboxed_args; }

                // Method call runs in its own frame and cannot modify caller's locals
                // Only invalidate if we actually called external code
                if (called_external) {
                    invalidateExternalCaches();
                }

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
                    res = evalAndRef(expr_inst->expr, xsink);
                }
            } else {
                res = evalAndRef(expr_inst->expr, xsink);
            }
            if (xsink && *xsink) {
                executeOnBlockExitHandlers(on_block_exit_handlers, xsink);
                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                cleanupLocalCaches();
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
                cleanupLocalCaches();
                return false;
            }
            setValueSlot(values, regex_inst->result.id, res, xsink);
            ++ip;
            break;
        }
        case QoreIROpcode::SwitchCaseMatch: {
            // Handle switch case match using CaseNode::matches() which unwraps TAG_ENUM
            auto* case_inst = static_cast<QoreIRSwitchCaseMatchInstruction*>(inst);
            QoreValue res;
            if (!case_inst->operands.empty() && case_inst->case_node) {
                QoreValue switch_val = getIRValue(values, case_inst->operands[0]);
                bool match = case_inst->case_node->matches(switch_val, xsink);
                res = QoreValue(match);
            } else {
                res = QoreValue(false);
            }
            if (xsink && *xsink) {
                executeOnBlockExitHandlers(on_block_exit_handlers, xsink);
                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                cleanupLocalCaches();
                return false;
            }
            setValueSlot(values, case_inst->result.id, res, xsink);
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
                        res = evalAndRef(expr_inst->expr, xsink);
                    }
                } else if (auto* extract_node = dynamic_cast<const QoreRegexExtractOperatorNode*>(
                        expr_inst->expr.getInternalNode())) {
                    QoreRegex* regex = extract_node->getRegex();
                    if (regex) {
                        QoreStringNodeValueHelper str(str_val);
                        res = QoreValue(regex->extractSubstrings(*str, xsink));
                    } else {
                        res = evalAndRef(expr_inst->expr, xsink);
                    }
                } else {
                    res = evalAndRef(expr_inst->expr, xsink);
                }
            } else {
                res = evalAndRef(expr_inst->expr, xsink);
            }
            if (xsink && *xsink) {
                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                cleanupLocalCaches();
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
                // Invalidate all caches BEFORE the lvalue operation to prevent COW inflation
                cleanupLocalCaches();
                // Direct eval() — avoids evalExprNode() overhead (refSelf + ValueHolder)
                QoreValue res = evalAndRef(expr_inst->expr, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                setValueSlot(values, inst->result.id, res, xsink);
                if (res.hasNode()) {
                    cleanup.push_back(inst->result.id);
                }
                ++ip;
                break;
            }
        // InstanceOf: native type check with pre-evaluated operand
        case QoreIROpcode::InstanceOfBool: {
                auto* expr_inst = static_cast<QoreIRExprInstruction*>(inst);
                QoreValue res;
                if (!expr_inst->operands.empty()) {
                    auto* io_node = static_cast<const QoreInstanceOfOperatorNode*>(
                        expr_inst->expr.getInternalNode());
                    const QoreTypeInfo* ti = io_node->getInstanceTypeInfo();
                    QoreValue val = getIRValue(values, expr_inst->operands[0]);
                    // Handle weak reference types like AST evalImpl
                    qore_type_t t = val.getType();
                    switch (t) {
                        case NT_WEAKREF:
                            res = QoreTypeInfo::runtimeAcceptsValue(ti,
                                **val.get<const WeakReferenceNode>()) ? true : false;
                            break;
                        case NT_WEAKREF_HASH:
                            res = QoreTypeInfo::runtimeAcceptsValue(ti,
                                **val.get<const WeakHashReferenceNode>()) ? true : false;
                            break;
                        case NT_WEAKREF_LIST:
                            res = QoreTypeInfo::runtimeAcceptsValue(ti,
                                **val.get<const WeakListReferenceNode>()) ? true : false;
                            break;
                        default:
                            res = QoreTypeInfo::runtimeAcceptsValue(ti, val) ? true : false;
                            break;
                    }
                } else {
                    res = evalAndRef(expr_inst->expr, xsink);
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        return false;
                    }
                }
                setValueSlot(values, inst->result.id, res, xsink);
                // bool result has no node — no cleanup needed
                ++ip;
                break;
            }
        // Keys: native hash/object key retrieval with pre-evaluated operand
        case QoreIROpcode::KeysAny:
        case QoreIROpcode::KeysList:
        case QoreIROpcode::KeysHash: {
                auto* expr_inst = static_cast<QoreIRExprInstruction*>(inst);
                QoreValue res;
                if (!expr_inst->operands.empty()) {
                    QoreValue val = getIRValue(values, expr_inst->operands[0]);
                    qore_type_t t = val.getType();
                    if (t == NT_HASH) {
                        res = val.get<const QoreHashNode>()->getKeys();
                    } else if (t == NT_OBJECT) {
                        QoreObject* o = const_cast<QoreObject*>(val.get<const QoreObject>());
                        AutoVLock vl(xsink);
                        res = qore_object_private::get(*o)->getRuntimeMemberHash(xsink);
                        if (!(xsink && *xsink) && res.getType() == NT_HASH) {
                            QoreValue keys = res.get<const QoreHashNode>()->getKeys();
                            res.discard(xsink);
                            res = keys;
                        }
                    }
                    // For other types, res stays NOTHING
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        return false;
                    }
                } else {
                    res = evalAndRef(expr_inst->expr, xsink);
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        return false;
                    }
                }
                setValueSlot(values, inst->result.id, res, xsink);
                if (res.hasNode()) {
                    cleanup.push_back(inst->result.id);
                }
                ++ip;
                break;
            }
        // Non-modifying AST opcodes: direct eval — no cache invalidation needed
        case QoreIROpcode::BackgroundInt: {
                auto* expr_inst = static_cast<QoreIRExprInstruction*>(inst);
                // BackgroundInt spawns a background thread that will access closure-captured
                // variables, so invalidate external caches (for closure variable access) after
                // spawning the thread
                QoreValue res = evalAndRef(expr_inst->expr, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    return false;
                }
                // Invalidate external caches after spawning background thread
                // The closure may access closure-captured variables cached in CVV
                invalidateExternalCaches();
                setValueSlot(values, inst->result.id, res, xsink);
                if (res.hasNode()) {
                    cleanup.push_back(inst->result.id);
                }
                ++ip;
                break;
            }
        case QoreIROpcode::ElementsAny:
        case QoreIROpcode::ElementsInt: {
                auto* expr_inst = static_cast<QoreIRExprInstruction*>(inst);
                // Direct eval() — avoids evalExprNode() overhead
                QoreValue res = evalAndRef(expr_inst->expr, xsink);
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
        case QoreIROpcode::CastComplexHash:
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
                    cleanupLocalCaches();
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
                        res = evalAndRef(expr_inst->expr, xsink);
                    }
                } else {
                    res = evalAndRef(expr_inst->expr, xsink);
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                // Method calls run in their own frame and cannot modify caller's locals
                invalidateExternalCaches();
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
                cleanupLocalCaches();
                return false;
            }
            case QoreIROpcode::Rethrow: {
                auto* rethrow_inst = static_cast<QoreIRThrowInstruction*>(inst);
                if (!xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
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
                cleanupLocalCaches();
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
                cleanupLocalCaches();
                return true;
            }
            case QoreIROpcode::ReturnNothing: {
                return_value = QoreValue();
                if (debug_active) {
                    tlpd->dbgFunctionExit(statements, return_value, xsink);
                }
                executeOnBlockExitHandlers(on_block_exit_handlers, xsink);
                cleanupValues(values, cleanup, xsink, false, cleanup_log);
                cleanupLocalCaches();
                return true;
            }
            case QoreIROpcode::Phi: {
                // Phi instructions should be processed at block entry, but may appear
                // mid-block in some IR lowering patterns. Process inline: select the
                // incoming value from the predecessor block.
                auto* phi = static_cast<QoreIRPhiInstruction*>(inst);
                QoreIRValue incoming_value;
                bool found = false;
                for (const auto& inc : phi->incoming) {
                    if (inc.block == prev_block) {
                        incoming_value = inc.value;
                        found = true;
                        break;
                    }
                }
                if (found) {
                    QoreValue val = getIRValue(values, incoming_value);
                    QoreValue stored = val.hasNode() ? val.refSelf() : val;
                    setValueSlot(values, phi->result.id, stored, xsink);
                    if (stored.hasNode()) {
                        cleanup.push_back(phi->result.id);
                    }
                }
                ++ip;
                break;
            }
            default:
                if (xsink) {
                    std::string msg = "unsupported opcode in executor: ";
                    msg += std::to_string(static_cast<int>(inst->opcode));
                    xsink->raiseException("IR-EXEC-ERROR", msg.c_str());
                }
                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                cleanupLocalCaches();
                return false;
        }

        // Fast path: if we're still in the same block (no branch occurred),
        // skip phi processing and boundary checks — jump directly to next instruction.
        // Branch instructions set block != current_block, so they fall through to the
        // outer while loop which handles phi processing for the new block.
        // The ip > 0 check handles self-loops (branch to same block with ip = 0):
        // these must go through phi processing in the outer while loop.
        if (block == current_block && ip > 0 && ip < block->instructions.size()) {
            goto next_instruction;
        }
        } // end of current_block scope

    }

    if (xsink) {
        xsink->raiseException("IR-EXEC-ERROR", "executor reached invalid state");
    }
    cleanupValues(values, cleanup, xsink, true, cleanup_log);
    cleanupLocalCaches();
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
        case QoreIROpcode::IsCollectionType: {
            qore_type_t t = value.getType();
            return QoreValue(t == NT_LIST || t == NT_OBJECT);
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
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsBigInt());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            size_t start_idx = 0;
            int64_t result;
            if (right.isNothing()) {
                if (sz == 0) {
                    return QoreValue();
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
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsFloat());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            size_t start_idx = 0;
            double result;
            if (right.isNothing()) {
                if (sz == 0) {
                    return QoreValue();
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
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsBigInt());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            size_t start_idx = 0;
            int64_t result;
            if (right.isNothing()) {
                if (sz == 0) {
                    return QoreValue();
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
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsFloat());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            size_t start_idx = 0;
            double result;
            if (right.isNothing()) {
                if (sz == 0) {
                    return QoreValue();
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
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsBigInt());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            size_t start_idx = 0;
            int64_t result;
            if (right.isNothing()) {
                if (sz == 0) {
                    return QoreValue();
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
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsFloat());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            size_t start_idx = 0;
            double result;
            if (right.isNothing()) {
                if (sz == 0) {
                    return QoreValue();
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
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsBigInt());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsBigInt());
            }
            int64_t result = right.isNothing() ? 0ll : right.getAsBigInt();
            for (size_t i = 0; i < sz; ++i) {
                result += l->retrieveEntry(i).getAsBigInt();
            }
            return QoreValue(result);
        }
        case QoreIROpcode::FoldrSumFloat: {
            if (left.getType() != NT_LIST) {
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsFloat());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsFloat());
            }
            double result = right.isNothing() ? 0.0 : right.getAsFloat();
            for (size_t i = 0; i < sz; ++i) {
                result += l->retrieveEntry(i).getAsFloat();
            }
            return QoreValue(result);
        }
        case QoreIROpcode::FoldrProdInt: {
            if (left.getType() != NT_LIST) {
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsBigInt());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsBigInt());
            }
            int64_t result = right.isNothing() ? 1ll : right.getAsBigInt();
            for (size_t i = 0; i < sz; ++i) {
                result *= l->retrieveEntry(i).getAsBigInt();
            }
            return QoreValue(result);
        }
        case QoreIROpcode::FoldrProdFloat: {
            if (left.getType() != NT_LIST) {
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsFloat());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsFloat());
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
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsBigInt());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsBigInt());
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
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsFloat());
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return right.isNothing() ? QoreValue() : QoreValue(right.getAsFloat());
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
            // left = list or single value, right = scale factor
            if (left.getType() != NT_LIST) {
                // NOTHING returns NOTHING
                if (left.isNothing()) {
                    return QoreValue();
                }
                // Handle iterator objects using abstract iterator protocol
                if (left.getType() == NT_OBJECT) {
                    QoreObject* obj = const_cast<QoreObject*>(left.get<const QoreObject>());
                    AbstractIteratorHelper h(xsink, "map operator", obj);
                    if (h) {
                        int64_t scale = right.getAsBigInt();
                        ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), xsink);
                        while (true) {
                            bool has_next = h.next(xsink);
                            if (*xsink) return QoreValue();
                            if (!has_next) break;
                            ValueHolder iv(h.getValue(xsink), xsink);
                            if (*xsink) return QoreValue();
                            result->push(iv->getAsBigInt() * scale, xsink);
                        }
                        return result.release();
                    }
                    // Fall through to single-value handling if not an iterator
                }
                // Handle single-value input: apply operation and return directly
                int64_t val = left.getAsBigInt();
                int64_t scale = right.getAsBigInt();
                return val * scale;
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
                // NOTHING returns NOTHING
                if (left.isNothing()) {
                    return QoreValue();
                }
                // Handle iterator objects using abstract iterator protocol
                if (left.getType() == NT_OBJECT) {
                    QoreObject* obj = const_cast<QoreObject*>(left.get<const QoreObject>());
                    AbstractIteratorHelper h(xsink, "map operator", obj);
                    if (h) {
                        double scale = right.getAsFloat();
                        ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), xsink);
                        while (true) {
                            bool has_next = h.next(xsink);
                            if (*xsink) return QoreValue();
                            if (!has_next) break;
                            ValueHolder iv(h.getValue(xsink), xsink);
                            if (*xsink) return QoreValue();
                            result->push(iv->getAsFloat() * scale, xsink);
                        }
                        return result.release();
                    }
                    // Fall through to single-value handling if not an iterator
                }
                // Handle single-value input: apply operation and return directly
                double val = left.getAsFloat();
                double scale = right.getAsFloat();
                return val * scale;
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
            // left = list or single value, right = offset
            if (left.getType() != NT_LIST) {
                // NOTHING returns NOTHING
                if (left.isNothing()) {
                    return QoreValue();
                }
                // Handle iterator objects using abstract iterator protocol
                if (left.getType() == NT_OBJECT) {
                    QoreObject* obj = const_cast<QoreObject*>(left.get<const QoreObject>());
                    AbstractIteratorHelper h(xsink, "map operator", obj);
                    if (h) {
                        int64_t offset = right.getAsBigInt();
                        ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), xsink);
                        while (true) {
                            bool has_next = h.next(xsink);
                            if (*xsink) return QoreValue();
                            if (!has_next) break;
                            ValueHolder iv(h.getValue(xsink), xsink);
                            if (*xsink) return QoreValue();
                            result->push(iv->getAsBigInt() + offset, xsink);
                        }
                        return result.release();
                    }
                    // Fall through to single-value handling if not an iterator
                }
                // Handle single-value input: apply operation and return directly
                int64_t val = left.getAsBigInt();
                int64_t offset = right.getAsBigInt();
                return val + offset;
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
                // NOTHING returns NOTHING
                if (left.isNothing()) {
                    return QoreValue();
                }
                // Handle iterator objects using abstract iterator protocol
                if (left.getType() == NT_OBJECT) {
                    QoreObject* obj = const_cast<QoreObject*>(left.get<const QoreObject>());
                    AbstractIteratorHelper h(xsink, "map operator", obj);
                    if (h) {
                        double offset = right.getAsFloat();
                        ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), xsink);
                        while (true) {
                            bool has_next = h.next(xsink);
                            if (*xsink) return QoreValue();
                            if (!has_next) break;
                            ValueHolder iv(h.getValue(xsink), xsink);
                            if (*xsink) return QoreValue();
                            result->push(iv->getAsFloat() + offset, xsink);
                        }
                        return result.release();
                    }
                    // Fall through to single-value handling if not an iterator
                }
                // Handle single-value input: apply operation and return directly
                double val = left.getAsFloat();
                double offset = right.getAsFloat();
                return val + offset;
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
            // left = list or single value, right = unused
            if (left.getType() != NT_LIST) {
                // NOTHING returns NOTHING
                if (left.isNothing()) {
                    return QoreValue();
                }
                // Handle iterator objects using abstract iterator protocol
                if (left.getType() == NT_OBJECT) {
                    QoreObject* obj = const_cast<QoreObject*>(left.get<const QoreObject>());
                    AbstractIteratorHelper h(xsink, "map operator", obj);
                    if (h) {
                        ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), xsink);
                        while (true) {
                            bool has_next = h.next(xsink);
                            if (*xsink) return QoreValue();
                            if (!has_next) break;
                            ValueHolder iv(h.getValue(xsink), xsink);
                            if (*xsink) return QoreValue();
                            int64_t val = iv->getAsBigInt();
                            result->push(val * val, xsink);
                        }
                        return result.release();
                    }
                    // Fall through to single-value handling if not an iterator
                }
                // Handle single-value input: apply operation and return directly
                int64_t val = left.getAsBigInt();
                return val * val;
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
                // NOTHING returns NOTHING
                if (left.isNothing()) {
                    return QoreValue();
                }
                // Handle iterator objects using abstract iterator protocol
                if (left.getType() == NT_OBJECT) {
                    QoreObject* obj = const_cast<QoreObject*>(left.get<const QoreObject>());
                    AbstractIteratorHelper h(xsink, "map operator", obj);
                    if (h) {
                        ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), xsink);
                        while (true) {
                            bool has_next = h.next(xsink);
                            if (*xsink) return QoreValue();
                            if (!has_next) break;
                            ValueHolder iv(h.getValue(xsink), xsink);
                            if (*xsink) return QoreValue();
                            double val = iv->getAsFloat();
                            result->push(val * val, xsink);
                        }
                        return result.release();
                    }
                    // Fall through to single-value handling if not an iterator
                }
                // Handle single-value input: apply operation and return directly
                double val = left.getAsFloat();
                return val * val;
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
                return QoreValue();
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return QoreValue();
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
                return QoreValue();
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return QoreValue();
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
                return QoreValue();
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return QoreValue();
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
                return QoreValue();
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return QoreValue();
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
                return QoreValue();
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return QoreValue();
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
                return QoreValue();
            }
            const QoreListNode* l = left.get<const QoreListNode>();
            size_t sz = l->size();
            if (sz == 0) {
                return QoreValue();
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
