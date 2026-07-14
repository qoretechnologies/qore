/* -*- indent-tabs-mode: nil -*- */
/*
    QoreIRInterpreter.cpp

    Qore Programming Language
*/

#include "qore/intern/QoreJITIncludes.h"
#include <qore/intern/QoreIRInterpreter.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>
#include <typeinfo>
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
#include "qore/intern/QorePseudoMethods.h"
#include <qore/intern/QoreRegex.h>
#include <qore/intern/StaticClassVarRefNode.h>
#include <qore/intern/QoreRegexSubst.h>
#include <qore/intern/QoreRegexSubstOperatorNode.h>
#include <qore/intern/QoreTransliteration.h>
#include <qore/intern/QoreTransliterationOperatorNode.h>
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
#include <qore/intern/QoreBackgroundOperatorNode.h>
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
#include <qore/intern/QoreIterateOperatorNode.h>
#include <qore/intern/QoreLogicalComparisonOperatorNode.h>
#include <qore/intern/QoreLogicalEqualsOperatorNode.h>
#include <qore/intern/QoreLogicalGreaterThanOperatorNode.h>
#include <qore/intern/QoreLogicalGreaterThanOrEqualsOperatorNode.h>
#include <qore/intern/QoreLogicalLessThanOperatorNode.h>
#include <qore/intern/QoreLogicalLessThanOrEqualsOperatorNode.h>
#include <qore/intern/QoreIR.h>
#include <qore/intern/QorePluginRegistry.h>
#include <qore/intern/QoreAOTBinary.h>
// Compile-time guard: forces review of interpreter dispatch when opcodes change.
// Update this value after verifying the new opcode is handled (or deliberately
// falls through to the default case).
static_assert(QORE_IR_MAX_OPCODE == 383,
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
#include <qore/intern/QoreRemoveOperatorNode.h>
#include <qore/intern/QoreDeleteOperatorNode.h>

extern "C" uint64_t qore_rt_list_index_selectors(uint64_t left_bits, const uint8_t* kinds,
        int32_t count, const uint64_t* selector_bits, int32_t string_index_char, ExceptionSink* xsink);

static int qore_ir_check_closure_self_valid(QoreObject* obj, ExceptionSink* xsink) {
    return (obj && qore_closure_self_context(obj))
        ? qore_object_private::get(*obj)->checkClosureSelfValid(xsink)
        : 0;
}

static const TypedHashDecl* resolveNewHashDeclFromHashTarget(
        const QoreIRNewHashDeclFromHashInstruction& inst, ExceptionSink* xsink) {
    if (inst.hd) {
        return inst.hd;
    }
    if (inst.hd_path.empty()) {
        if (xsink) {
            xsink->raiseException("HASHDECL-ERROR",
                "cannot resolve hashdecl for NewHashDeclFromHash: missing serialized path");
        }
        return nullptr;
    }
    QoreProgram* pgm = getProgram();
    if (!pgm) {
        if (xsink) {
            xsink->raiseException("HASHDECL-ERROR",
                "cannot resolve hashdecl '%s': no program context", inst.hd_path.c_str());
        }
        return nullptr;
    }
    const TypedHashDecl* hd = qore_aot_resolve_hashdecl_path(pgm, inst.hd_path.c_str());
    if (!hd) {
        std::string error;
        QoreAOTTypeResolver resolver(pgm);
        const QoreTypeInfo* ti = resolver.resolve(inst.hd_path.c_str(), error);
        ti = qore_substitute_type_params_if_needed(ti);
        hd = QoreTypeInfo::getUniqueReturnHashDecl(ti);
    }
    if (!hd && xsink) {
        xsink->raiseException("HASHDECL-ERROR", "cannot resolve hashdecl '%s'",
            inst.hd_path.c_str());
    }
    return hd;
}

static QoreHashNode* makeImplicitHashForLValueType(const QoreTypeInfo* typeInfo, ExceptionSink* xsink) {
    if (!QoreTypeInfo::parseAcceptsReturns(typeInfo, NT_HASH)) {
        xsink->raiseException("RUNTIME-TYPE-ERROR", "cannot convert lvalue declared as %s to a hash",
            QoreTypeInfo::getName(typeInfo));
        return nullptr;
    }

    if (!typeInfo || typeInfo == anyTypeInfo || typeInfo == hashTypeInfo || typeInfo == hashOrNothingTypeInfo) {
        return new QoreHashNode;
    }

    const QoreTypeInfo* sti = typeInfo == autoTypeInfo
        ? autoTypeInfo
        : QoreTypeInfo::getReturnComplexHashOrNothing(typeInfo);
    if (sti) {
        return new QoreHashNode(sti);
    }

    const TypedHashDecl* thd = QoreTypeInfo::getUniqueReturnHashDecl(typeInfo);
    if (thd) {
        QoreStringNode* desc = new QoreStringNodeMaker("Cannot implicitly create typed hash '%s' "
            "with an assignment; to address this error, declare the typed hash before the assignment",
            thd->getName());
        xsink->raiseException("HASHDECL-IMPLICIT-CONSTRUCTION-ERROR", desc);
        return nullptr;
    }

    return new QoreHashNode(QoreTypeInfo::getElementType(QoreTypeInfo::getReturnComplexHashOrNothing(typeInfo)));
}

static const QoreTypeInfo* substituteRuntimeTypeParams(const QoreTypeInfo* typeInfo) {
    return qore_substitute_type_params_if_needed(typeInfo);
}

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
    if (auto* op = dynamic_cast<const QoreRemoveOperatorNode*>(node)) {
        return extractLValueBaseVarRef(op->getExp());
    }
    if (auto* op = dynamic_cast<const QoreDeleteOperatorNode*>(node)) {
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
#include <qore/intern/BackquoteNode.h>
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

static QoreValue getListAssignmentValue(QoreValue value, int64_t index) {
    if (value.getType() == NT_LIST) {
        const QoreListNode* l = value.get<const QoreListNode>();
        if (index >= 0 && static_cast<size_t>(index) < l->size()) {
            return l->getReferencedEntry(static_cast<size_t>(index));
        }
        return QoreValue();
    }
    return index == 0 ? value.refSelf() : QoreValue();
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

static inline bool isWeakReferenceType(qore_type_t type) {
    return type == NT_WEAKREF || type == NT_WEAKREF_HASH || type == NT_WEAKREF_LIST;
}

static bool normalizeWeakReferenceForAssignment(QoreValue& value, ValueHolder& holder, ExceptionSink* xsink) {
    if (!isWeakReferenceType(value.getType())) {
        return false;
    }
    holder = value.eval(xsink);
    if (xsink && *xsink) {
        return true;
    }
    value = *holder;
    return true;
}

static bool evaluateOwnedWeakReferenceResult(QoreValue& value, ExceptionSink* xsink) {
    if (!isWeakReferenceType(value.getType())) {
        return false;
    }
    ValueHolder old(value, xsink);
    value = old->eval(xsink);
    return true;
}

QoreValue QoreIRInterpreter::evalComparison(QoreIROpcode op, const QoreValue& left, const QoreValue& right,
        ExceptionSink* xsink) {
    switch (op) {
        case QoreIROpcode::EqInt:
            return QoreValue(left.getAsBigInt() == right.getAsBigInt());
        case QoreIROpcode::EqFloat:
            return QoreValue(left.getAsFloat() == right.getAsFloat());
        case QoreIROpcode::EqString: {
            if (left.getType() != NT_STRING || right.getType() != NT_STRING) {
                return QoreValue(false);
            }
            QoreStringNodeValueHelper lstr(left);
            QoreStringNodeValueHelper rstr(right);
            return QoreValue(lstr && rstr && lstr->equalSoft(**rstr, xsink));
        }
        case QoreIROpcode::EqAny:
            if (qore_buffer_binary_op_applies(left, right)) {
                return qore_buffer_binary_op(left, right, QoreBufferBinaryOperation::Equal, xsink);
            }
            return QoreValue(QoreLogicalEqualsOperatorNode::softEqual(left, right, xsink));
        case QoreIROpcode::NeInt:
            return QoreValue(left.getAsBigInt() != right.getAsBigInt());
        case QoreIROpcode::NeFloat:
            return QoreValue(left.getAsFloat() != right.getAsFloat());
        case QoreIROpcode::NeString: {
            if (left.getType() != NT_STRING || right.getType() != NT_STRING) {
                return QoreValue(true);
            }
            QoreStringNodeValueHelper lstr(left);
            QoreStringNodeValueHelper rstr(right);
            return QoreValue(!lstr || !rstr || !lstr->equalSoft(**rstr, xsink));
        }
        case QoreIROpcode::NeAny:
            if (qore_buffer_binary_op_applies(left, right)) {
                return qore_buffer_binary_op(left, right, QoreBufferBinaryOperation::NotEqual, xsink);
            }
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
            if (left.getType() != NT_STRING || right.getType() != NT_STRING) {
                return QoreValue(false);
            }
            QoreStringNodeValueHelper lstr(left);
            QoreStringNodeValueHelper rstr(right);
            return QoreValue(lstr && rstr && (lstr->compare(*rstr) < 0));
        }
        case QoreIROpcode::LtAny:
            if (qore_buffer_binary_op_applies(left, right)) {
                return qore_buffer_binary_op(left, right, QoreBufferBinaryOperation::LessThan, xsink);
            }
            return QoreValue(QoreLogicalLessThanOperatorNode::doLessThan(left, right, xsink));
        case QoreIROpcode::LeInt:
            return QoreValue(left.getAsBigInt() <= right.getAsBigInt());
        case QoreIROpcode::LeFloat:
            return QoreValue(left.getAsFloat() <= right.getAsFloat());
        case QoreIROpcode::LeString: {
            if (left.getType() != NT_STRING || right.getType() != NT_STRING) {
                return QoreValue(false);
            }
            QoreStringNodeValueHelper lstr(left);
            QoreStringNodeValueHelper rstr(right);
            return QoreValue(lstr && rstr && (lstr->compare(*rstr) <= 0));
        }
        case QoreIROpcode::LeAny:
            if (qore_buffer_binary_op_applies(left, right)) {
                return qore_buffer_binary_op(left, right, QoreBufferBinaryOperation::LessThanOrEqual, xsink);
            }
            return QoreValue(QoreLogicalLessThanOrEqualsOperatorNode::doLessThanOrEquals(left, right, xsink));
        case QoreIROpcode::GtInt:
            return QoreValue(left.getAsBigInt() > right.getAsBigInt());
        case QoreIROpcode::GtFloat:
            return QoreValue(left.getAsFloat() > right.getAsFloat());
        case QoreIROpcode::GtString: {
            if (left.getType() != NT_STRING || right.getType() != NT_STRING) {
                return QoreValue(false);
            }
            QoreStringNodeValueHelper lstr(left);
            QoreStringNodeValueHelper rstr(right);
            return QoreValue(lstr && rstr && (lstr->compare(*rstr) > 0));
        }
        case QoreIROpcode::GtAny:
            if (qore_buffer_binary_op_applies(left, right)) {
                return qore_buffer_binary_op(left, right, QoreBufferBinaryOperation::GreaterThan, xsink);
            }
            return QoreValue(QoreLogicalGreaterThanOperatorNode::doGreaterThan(left, right, xsink));
        case QoreIROpcode::GeInt:
            return QoreValue(left.getAsBigInt() >= right.getAsBigInt());
        case QoreIROpcode::GeFloat:
            return QoreValue(left.getAsFloat() >= right.getAsFloat());
        case QoreIROpcode::GeString: {
            if (left.getType() != NT_STRING || right.getType() != NT_STRING) {
                return QoreValue(false);
            }
            QoreStringNodeValueHelper lstr(left);
            QoreStringNodeValueHelper rstr(right);
            return QoreValue(lstr && rstr && (lstr->compare(*rstr) >= 0));
        }
        case QoreIROpcode::GeAny:
            if (qore_buffer_binary_op_applies(left, right)) {
                return qore_buffer_binary_op(left, right, QoreBufferBinaryOperation::GreaterThanOrEqual, xsink);
            }
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
            int64_t result = 0;
            if (left.getType() == NT_STRING && right.getType() == NT_STRING) {
                QoreStringNodeValueHelper lstr(left);
                QoreStringNodeValueHelper rstr(right);
                int cmp = lstr->compare(*rstr);
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

static QoreValue raiseIRInterpreterAstFallback(QoreIROpcode op, const QoreValue& expr, ExceptionSink* xsink) {
    if (xsink) {
        const AbstractQoreNode* node = expr.getInternalNode();
        const ParseNode* parse_node = dynamic_cast<const ParseNode*>(node);
        const QoreProgramLocation* loc = parse_node ? parse_node->loc : nullptr;
        if (loc) {
            xsink->raiseException("IR-AST-FALLBACK-ERROR",
                "IR interpreter executable AST expression fallback is disabled: opcode=%s(%d) "
                "expr_type=%s node_type=%s source=%s:%d; add native IR lowering instead",
                getOpcodeName(static_cast<int>(op)), static_cast<int>(op), expr.getTypeName(),
                node ? typeid(*node).name() : "<null>", loc->getFileValue(), loc->start_line);
        } else {
            xsink->raiseException("IR-AST-FALLBACK-ERROR",
                "IR interpreter executable AST expression fallback is disabled: opcode=%s(%d) "
                "expr_type=%s node_type=%s; add native IR lowering instead",
                getOpcodeName(static_cast<int>(op)), static_cast<int>(op), expr.getTypeName(),
                node ? typeid(*node).name() : "<null>");
        }
    }
    return QoreValue();
}

QoreValue QoreIRInterpreter::evalExpr(QoreIROpcode op, const QoreValue& expr, ExceptionSink* xsink) {
    switch (op) {
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
            return raiseIRInterpreterAstFallback(op, expr, xsink);
    }
}

bool QoreIRInterpreter::simulateInvoke(QoreIROpcode op, const QoreValue& expr, ExceptionSink* xsink) {
    evalExpr(op, expr, xsink);
    return xsink && *xsink;
}

int QoreIRInterpreter::execStatement(QoreIROpcode op, const AbstractStatement* stmt, QoreValue& return_value,
        ExceptionSink* xsink) {
    switch (op) {
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

// Thread-local state for QORE_IR_TRACE_SILENT_FAIL=1 — populated before
// each instruction dispatch in execute()'s main loop, read by
// dumpLastSilentFail() at the caller's fallback path.  No cost when the
// env var is off; ~2 word-stores per instruction when it's on (no
// measurable overhead — the loop already touches these cachelines).
static thread_local QoreIROpcode tls_last_silent_fail_opcode = QoreIROpcode::ConstNothing;
static thread_local const QoreProgramLocation* tls_last_silent_fail_loc = nullptr;

void QoreIRInterpreter::dumpLastSilentFail(const char* tag) {
    const QoreProgramLocation* loc = tls_last_silent_fail_loc;
    fprintf(stderr, "[ir-silent-fail:%s] opcode=%d @ %s:%d\n",
        tag ? tag : "?",
        static_cast<int>(tls_last_silent_fail_opcode),
        loc ? (loc->getFile() ? loc->getFile() : "<unknown>") : "<no-loc>",
        loc ? loc->start_line : 0);
    fflush(stderr);
}

// Thread-local call frame pool — eliminates per-call heap allocation by
// on_block_exit handler record (defined here so IRCallFrame can pool it)
struct IROnBlockExitHandler {
    obe_type_e type;
    StatementBlock* code;
    const QoreIRFunction* handler_ir = nullptr;  //!< compiled handler (required for IR/JIT/AOT execution)
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
    std::vector<const LocalVar*> instantiated_locals_ordered;
    std::unordered_set<const LocalVar*> locally_uninstantiated;
    std::vector<uint8_t> locals_ir_only;
    std::vector<bool> locals_instantiated;
    std::vector<uint32_t> local_init_slots;
    std::vector<std::vector<uint32_t>> local_load_slots;
    std::vector<bool> load_slot_registered;
    // Ephemeral weak ref slots: value slot IDs holding LoadSelfMember/LoadStaticVar
    // results that must be discarded at statement boundaries to prevent long-running
    // threads from holding references that block object destruction.
    std::vector<uint32_t> ephemeral_weak_ref_slots;
    // (Reserved for potential future per-frame diagnostic state; the
    // silent-fail trace uses a thread-local in QoreIRInterpreter.cpp so
    // cross-frame failure propagation remains visible.)

    // Per-call containers pooled here to avoid per-call heap allocation.
    // After warm-up, clear() retains bucket arrays / capacity.
    std::unordered_map<const void*, QoreValue> globals;
    std::unordered_map<const void*, QoreValue> threadlocals;
    std::unordered_map<const void*, QoreValue> closures;
    std::unordered_set<FunctionalOperatorInterface*> active_iterators;
    std::vector<IROnBlockExitHandler> on_block_exit_handlers;
    // Tracks which value slot IDs are associated with local variables (via StoreLocal).
    // Used by UninstantiateLocal's container DGC scan to distinguish temporary expression
    // results from variable-held values — only temporaries are cleared at block exit.
    std::unordered_set<uint32_t> local_owned_slots;

    // Reset all fields for reuse.  clear() retains capacity, so no heap
    // allocation after the first call at each recursion depth.
    // IMPORTANT: QoreValue has no destructor that calls discard(), so we must
    // explicitly discard all reference-counted values before clearing vectors.
    void releaseReferences(ExceptionSink* xsink) {
        for (auto it = cleanup.rbegin(); it != cleanup.rend(); ++it) {
            uint32_t id = *it;
            if (id < values.size()) {
                values[id].discard(xsink);
                values[id] = QoreValue();
            }
        }
        cleanup.clear();
        for (auto& val : locals_slot_cache) {
            val.discard(xsink);
            val = QoreValue();
        }
        for (auto& entry : globals) {
            entry.second.discard(xsink);
        }
        globals.clear();
        for (auto& entry : threadlocals) {
            entry.second.discard(xsink);
        }
        threadlocals.clear();
        for (auto& entry : closures) {
            entry.second.discard(xsink);
        }
        closures.clear();
    }

    void reset(size_t reserve_size, size_t local_slot_count) {
        // Do NOT iterate values[] and discard — slots may hold borrowed (non-owning)
        // references intentionally stored by opcodes like ListGetInt/HashGetKey. These
        // are not added to `cleanup` because the caller doesn't own them; the container
        // (list/hash) owns them. Discarding borrowed references here UAFs when the
        // container has been freed between function exit and pool reuse.
        // Owned references are tracked in `cleanup` and drained by cleanupValues() at
        // every execute() return path; by the time we reach reset(), all owned slots
        // are already NOTHING.
        values.clear();
        values.resize(reserve_size);
        cleanup.clear();
        if (local_slot_count > 0) {
            for (auto& val : locals_slot_cache) {
                val.discard(nullptr);
            }
            locals_slot_cache.clear();
            locals_slot_cache.resize(local_slot_count);
            locals_lvar_cache.assign(local_slot_count, nullptr);
            locals_ir_only.assign(local_slot_count, false);
            locals_instantiated.assign(local_slot_count, false);
            local_init_slots.assign(local_slot_count, UINT32_MAX);
            local_load_slots.clear();
            local_load_slots.resize(local_slot_count);
        } else {
            for (auto& val : locals_slot_cache) {
                val.discard(nullptr);
            }
            locals_slot_cache.clear();
            locals_lvar_cache.clear();
            locals_ir_only.clear();
            locals_instantiated.clear();
            local_init_slots.clear();
            local_load_slots.clear();
        }
        load_slot_registered.assign(reserve_size, false);
        instantiated_locals.clear();
        instantiated_locals_ordered.clear();
        locally_uninstantiated.clear();
        ephemeral_weak_ref_slots.clear();
        for (auto& entry : globals) {
            entry.second.discard(nullptr);
        }
        globals.clear();
        for (auto& entry : threadlocals) {
            entry.second.discard(nullptr);
        }
        threadlocals.clear();
        for (auto& entry : closures) {
            entry.second.discard(nullptr);
        }
        closures.clear();
        active_iterators.clear();
        on_block_exit_handlers.clear();
        local_owned_slots.clear();
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

static uint32_t resolvePluginOperationId(const QoreIRPluginInstruction& inst, ExceptionSink* xsink) {
    if (inst.operation.global_operation_id) {
        return inst.operation.global_operation_id;
    }
    if (inst.operation.module_name.empty()) {
        if (xsink) {
            xsink->raiseException("PLUGIN-HELPER-ABI-MISMATCH",
                "cannot dispatch plugin operation: instruction has no global id or module name "
                "(field=\"operation\", expected=\"global operation id or module/local operation reference\", "
                "actual=\"missing\", subreason=\"operation_not_registered\", section=3.5)");
        }
        return 0;
    }
    uint32_t global_id = 0;
    if (qore_plugin_get_process_operation_id_checked(inst.operation.module_name.c_str(),
            inst.operation.local_operation_id, inst.operation.canonical_signature_version,
            inst.operation.signature_hash, &global_id, xsink)) {
        return 0;
    }
    return global_id;
}

static QoreValue makePluginArgsList(const std::vector<QoreIRValue>& operands,
        size_t start, const IRValueSlots& values, ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
    qore_list_private* priv = qore_list_private::get(**args);
    priv->reserve(operands.size() > start ? operands.size() - start : 0);
    for (size_t i = start; i < operands.size(); ++i) {
        if (i != start && !((i - start) % 100)
                && qore_check_cancel(xsink, "plugin runtime argument list construction")) {
            return QoreValue();
        }
        QoreValue arg = getIRValue(values, operands[i]);
        if (arg.hasNode()) {
            arg.refSelf();
        }
        priv->pushIntern(arg);
    }
    return QoreValue(args.release());
}

// Build a QoreListNode from a range of IR operands, ref'ing each node-typed value
// so the resulting list owns a ref on every element.  Returns nullptr for the
// empty range (same semantics as FunctionCallNode's args: absent list means no
// args, not empty list).
static QoreListNode* buildArgListFromIROperands(
        const std::vector<QoreIRValue>& operands,
        size_t first, size_t end,
        const IRValueSlots& values) {
    if (first >= end) {
        return nullptr;
    }
    QoreListNode* arg_list = new QoreListNode(autoTypeInfo);
    qore_list_private* priv = qore_list_private::get(*arg_list);
    priv->reserve(end - first);
    for (size_t i = first; i < end; ++i) {
        QoreValue val = getIRValue(values, operands[i]);
        if (val.hasNode()) {
            val.refSelf();
        }
        priv->pushIntern(val);
    }
    return arg_list;
}

// Attempt the decomposed-background path for one of the five supported inner
// expression shapes (self.method / foo(args) / Class::sm(args) / obj.method(args) /
// callref(args)).  Returns {true, result} if the shape matched and the spawned
// thread was kicked off; {false, QoreValue()} if the caller should fall through
// to the AST eval path.  Operand layout matches QoreIRLowering::lowerBackground.
static std::pair<bool, QoreValue> runDecomposedBackground(
        const QoreBackgroundOperatorNode* bg_op,
        const std::vector<QoreIRValue>& operands,
        const IRValueSlots& values,
        ExceptionSink* xsink) {
    if (!bg_op || operands.empty()) {
        return {false, QoreValue()};
    }
    const AbstractQoreNode* inner = bg_op->getExp().getInternalNode();
    const size_t nargs_ops = operands.size();

    // background self.method(args)
    if (auto* sfcn = dynamic_cast<const SelfFunctionCallNode*>(inner)) {
        if (!sfcn->getMethod()) {
            return {false, QoreValue()};
        }
        const QoreParseListNode* pargs = sfcn->getParseArgs();
        const QoreListNode* sargs = sfcn->getArgs();
        size_t actual_nargs = pargs ? pargs->size() : (sargs ? sargs->size() : 0);
        QoreListNode* arg_list =
            buildArgListFromIROperands(operands, 0, actual_nargs, values);
        SetSelfFunctionCallNode* call_node =
            new SetSelfFunctionCallNode(*sfcn, arg_list);
        QoreValue result = do_op_background(QoreValue(call_node), xsink);
        call_node->deref(xsink);
        QoreValue(call_node).discard(xsink);
        return {true, result};
    }
    // background foo(args) — free function call
    if (auto* fcn = dynamic_cast<const FunctionCallNode*>(inner)) {
        const QoreParseListNode* pargs = fcn->getParseArgs();
        const QoreListNode* fargs = fcn->getArgs();
        size_t actual_nargs = pargs ? pargs->size() : (fargs ? fargs->size() : 0);
        QoreListNode* arg_list =
            buildArgListFromIROperands(operands, 0, actual_nargs, values);
        FunctionCallNode* call_node = new FunctionCallNode(*fcn, arg_list);
        QoreValue result = do_op_background(QoreValue(call_node), xsink);
        QoreValue(call_node).discard(xsink);
        return {true, result};
    }
    // background Class::staticMethod(args)
    if (auto* smcn = dynamic_cast<const StaticMethodCallNode*>(inner)) {
        const QoreParseListNode* pargs = smcn->getParseArgs();
        const QoreListNode* sargs = smcn->getArgs();
        size_t actual_nargs = pargs ? pargs->size() : (sargs ? sargs->size() : 0);
        QoreListNode* arg_list =
            buildArgListFromIROperands(operands, 0, actual_nargs, values);
        StaticMethodCallNode* call_node = new StaticMethodCallNode(*smcn, arg_list);
        QoreValue result = do_op_background(QoreValue(call_node), xsink);
        QoreValue(call_node).discard(xsink);
        return {true, result};
    }
    // background obj.method(args) — receiver pre-evaluated in operand 0
    if (auto* devn = dynamic_cast<const QoreDotEvalOperatorNode*>(inner)) {
        MethodCallNode* m = devn->getMethodCall();
        if (!m) {
            return {false, QoreValue()};
        }
        QoreValue recv = getIRValue(values, operands[0]);
        if (recv.hasNode()) {
            recv.refSelf();
        }
        QoreListNode* arg_list =
            buildArgListFromIROperands(operands, 1, nargs_ops, values);
        MethodCallNode* new_m = new MethodCallNode(*m, arg_list);
        QoreDotEvalOperatorNode* call_node =
            new QoreDotEvalOperatorNode(devn->loc, recv, new_m);
        QoreValue result = do_op_background(QoreValue(call_node), xsink);
        QoreValue(call_node).discard(xsink);
        return {true, result};
    }
    // background callref(args) — call-ref pre-evaluated in operand 0
    if (auto* crcn = dynamic_cast<const CallReferenceCallNode*>(inner)) {
        QoreValue callee = getIRValue(values, operands[0]);
        if (callee.hasNode()) {
            callee.refSelf();
        }
        QoreListNode* arg_list =
            buildArgListFromIROperands(operands, 1, nargs_ops, values);
        CallReferenceCallNode* call_node =
            new CallReferenceCallNode(crcn->loc, callee, arg_list);
        QoreValue result = do_op_background(QoreValue(call_node), xsink);
        QoreValue(call_node).discard(xsink);
        return {true, result};
    }
    return {false, QoreValue()};
}

static std::pair<bool, QoreValue> runBackgroundMetadata(
        const QoreIRBackgroundInstruction* bg_inst,
        const IRValueSlots& values,
        ExceptionSink* xsink) {
    if (!bg_inst || bg_inst->operands.empty()) {
        return {false, QoreValue()};
    }

    if (bg_inst->kind == QoreIRBackgroundKind::DotEval) {
        QoreValue recv = getIRValue(values, bg_inst->operands[0]);
        if (recv.hasNode()) {
            recv.refSelf();
        }
        QoreListNode* arg_list =
            buildArgListFromIROperands(bg_inst->operands, 1, bg_inst->operands.size(), values);
        MethodCallNode* new_m = new MethodCallNode(&loc_builtin, strdup(bg_inst->name.c_str()), arg_list);
        QoreDotEvalOperatorNode* call_node =
            new QoreDotEvalOperatorNode(&loc_builtin, recv, new_m);
        QoreValue result = do_op_background(QoreValue(call_node), xsink);
        QoreValue(call_node).discard(xsink);
        return {true, result};
    }

    return {false, QoreValue()};
}

static bool removeCleanupEntry(std::vector<uint32_t>& cleanup, uint32_t id) {
    for (auto it = cleanup.rbegin(); it != cleanup.rend(); ++it) {
        if (*it == id) {
            cleanup.erase(std::next(it).base());
            return true;
        }
    }
    return false;
}

static bool removeAllCleanupEntries(std::vector<uint32_t>& cleanup, uint32_t id) {
    bool removed = false;
    for (auto it = cleanup.begin(); it != cleanup.end();) {
        if (*it == id) {
            it = cleanup.erase(it);
            removed = true;
        } else {
            ++it;
        }
    }
    return removed;
}

static bool hasCleanupEntry(const std::vector<uint32_t>& cleanup, uint32_t id) {
    return std::find(cleanup.begin(), cleanup.end(), id) != cleanup.end();
}

// Sets a value slot without discarding the previous value. Only use this for
// scalar or borrowed values; owned node values in loop-reexecuted slots must
// use setValueSlot() so the previous iteration's value is released.
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

static inline void setOwnedValueSlot(IRValueSlots& values, std::vector<uint32_t>& cleanup,
        uint32_t id, QoreValue new_val, ExceptionSink* xsink) {
    setValueSlot(values, id, new_val, xsink);
    if (new_val.hasNode()) {
        cleanup.push_back(id);
    }
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
                QoreStringValueHelper str(slot);
                cleanup_log->push_back(str->getBuffer());
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
        std::vector<const LocalVar*>& locals_ordered,
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
        locals_ordered.push_back(var);
        // Pre-instantiated variables (params, argvid, selfid, ast_visible_body_locals)
        // are already on the thread-local stack from evalTiered.  We skip re-instantiation
        // UNLESS the variable was explicitly uninstantiated mid-execution (e.g., a
        // closure-use loop-body variable whose CVV was popped by UninstantiateLocal at
        // the end of the previous loop iteration).  In that case we MUST re-instantiate
        // so the current iteration has a fresh CVV to capture.
        bool locally_uninst = locally_uninstantiated && locally_uninstantiated->count(var);
        bool is_pre = pre_instantiated && pre_instantiated->count(var) > 0;
        // For pre-instantiated closure-use vars: must instantiate on cvstack so closures can
        // capture them. instantiate() routes to cvstack when closureUse()=true, no lvstack
        // double-push. For pre-instantiated non-closure vars: skip (already on lvstack).
        // For locally-uninstantiated closure vars: need new CVV for next loop iteration.
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

static void cleanupInstantiatedLocals(const std::vector<const LocalVar*>& locals_ordered, ExceptionSink* xsink,
        const std::unordered_set<const LocalVar*>* pre_instantiated = nullptr) {
    // Iterate in REVERSE instantiation order so that closures that capture
    // other closures release their references before the captured CVVs are deref'd.
    // This matches AST mode's reverse-declaration-order finalization and prevents
    // DGC scanner deadlocks (QoreVarRWLock contention in ClosureVarValue::deref).
    for (auto it = locals_ordered.rbegin(); it != locals_ordered.rend(); ++it) {
        const LocalVar* var = *it;
        if (var) {
            if (pre_instantiated && pre_instantiated->find(var) != pre_instantiated->end()) {
                continue;
            }
            var->uninstantiate(xsink);
        }
    }
}

// Forward declarations — defined after execute() with other evalXxxEquals helpers
static QoreValue evalPlusEquals(const QoreValue& lvalue, const QoreValue& right, ExceptionSink* xsink);
QoreValue doPlusEqualsOnLValue(LValueHelper& v, const QoreValue& right, ExceptionSink* xsink);
QoreValue doMinusEqualsOnLValue(LValueHelper& v, const QoreValue& right, ExceptionSink* xsink);

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

// Variant of assignLocalVarValue that TRANSFERS ownership of value's reference
// instead of ref'ing. Caller must not deref value after this call.
// Returns true if the value was consumed (assigned or discarded on failure).
//
// This variant is needed for COW paths where the caller has a freshly-copied
// unique container (refcount 1). Passing via assignLocalVarValue's refSelf
// would bump refcount to 2, causing LValueHelper::assign to take the "non-unique"
// branch for typed lvalues (e.g., hash<auto!>) and create an extra copy —
// leaving the caller's pointer dangling after helper's saveTemp consumes it.
static void assignLocalVarValueTransfer(LocalVar* var, QoreValue value, ExceptionSink* xsink) {
    if (!var) {
        value.discard(xsink);
        return;
    }
    LValueHelper helper(xsink);
    if (var->getLValue(helper, false, true)) {
        value.discard(xsink);
        return;
    }
    // Transfer: no refSelf. helper.assign takes ownership of the passed ref.
    helper.assign(value);
}

static void applyNoNarrowContainerType(const QoreTypeInfo* ti, QoreValue& val, ExceptionSink* xsink) {
    if (ti == anyTypeInfo || ti == autoNoNarrowTypeInfo) {
        if (val.getType() == NT_HASH) {
            map_get_plain_hash(val, xsink);
        } else if (val.getType() == NT_LIST) {
            map_get_plain_list(val, xsink);
        }
    } else if (ti == autoNoNarrowHashTypeInfo || ti == autoNoNarrowHashOrNothingTypeInfo) {
        if (val.getType() != NT_HASH) {
            return;
        }
        QoreHashNode* h = val.get<QoreHashNode>();
        qore_hash_private* hp = qore_hash_private::get(*h);
        if (!hp->getHashDecl() && hp->complexTypeInfo == autoHashTypeInfo) {
            return;
        }
        if (!h->is_unique()) {
            QoreHashNode* copy = h->copy();
            qore_hash_private* cp = qore_hash_private::get(*copy);
            if (cp->getHashDecl()) {
                cp->setHashDecl(nullptr);
            }
            cp->complexTypeInfo = autoHashTypeInfo;
            AbstractQoreNode* old = val.assign(copy);
            discard(old, xsink);
        } else {
            if (hp->getHashDecl()) {
                hp->setHashDecl(nullptr);
            }
            hp->complexTypeInfo = autoHashTypeInfo;
        }
    } else if (ti == autoNoNarrowListTypeInfo || ti == autoNoNarrowListOrNothingTypeInfo) {
        if (val.getType() != NT_LIST) {
            return;
        }
        QoreListNode* l = val.get<QoreListNode>();
        qore_list_private* lp = qore_list_private::get(*l);
        if (lp->complexTypeInfo == autoListTypeInfo) {
            return;
        }
        if (!l->is_unique()) {
            QoreListNode* copy = l->copy();
            qore_list_private::get(*copy)->complexTypeInfo = autoListTypeInfo;
            AbstractQoreNode* old = val.assign(copy);
            discard(old, xsink);
        } else {
            lp->complexTypeInfo = autoListTypeInfo;
        }
    }
}

static QoreValue coerceIRLocalValue(LocalVar* var, const QoreValue& value, ExceptionSink* xsink) {
    QoreValue stored = value.hasNode() ? value.refSelf() : value;
    const QoreTypeInfo* ti = var ? var->getTypeInfoForLValue() : nullptr;
    ti = qore_substitute_type_params_if_needed(ti);
    QoreTypeInfo::acceptAssignment(ti, "<lvalue>", stored, xsink);
    if (xsink && *xsink) {
        stored.discard(xsink);
        return QoreValue();
    }
    applyNoNarrowContainerType(ti, stored, xsink);
    if (xsink && *xsink) {
        stored.discard(xsink);
        return QoreValue();
    }
    return stored;
}

static ClosureVarValue* resolve_closure_var_value(const LocalVar* var) {
    if (!var) {
        return nullptr;
    }

    if (thread_has_runtime_closure_env()) {
        ClosureVarValue* frame_cvv = thread_try_find_closure_var_in_current_frame(var->getName());
        if (frame_cvv) {
            return frame_cvv;
        }

        ClosureVarValue* env_cvv = thread_try_get_runtime_closure_var(var);
        if (env_cvv) {
            return env_cvv;
        }
    }

    ClosureVarValue* cvv = thread_try_find_closure_var(var->getName());
    if (!cvv) {
        cvv = thread_try_get_runtime_closure_var(var);
    }
    return cvv;
}

// Write-through for closure variable stores: writes the value to the actual
// ClosureVarValue so that changes are visible outside the IR interpreter.
static ClosureVarValue* findClosureVarValueForIR(LocalVar* var);

static void assignClosureVarValue(LocalVar* var, const QoreValue& value, ExceptionSink* xsink,
        bool initial_assignment = false) {
    if (!var) {
        return;
    }
    ClosureVarValue* cv = findClosureVarValueForIR(var);
    if (!cv) {
        // Closure variable not found — fall back to local stack assignment.
        // This happens when a function has closureUse() variables but executes
        // in its own body context (not as a closure).
        LValueHelper helper(xsink);
        if (var->getLValue(helper, false, initial_assignment)) {
            return;
        }
        QoreValue stored = value.hasNode() ? value.refSelf() : value;
        helper.assign(stored);
        return;
    }
    LValueHelper helper(xsink);
    if (cv->getLValue(helper, false, initial_assignment)) {
        return;
    }
    QoreValue stored = value.hasNode() ? value.refSelf() : value;
    helper.assign(stored);
}

static ClosureVarValue* findClosureVarValueForIR(LocalVar* var) {
    if (!var) {
        return nullptr;
    }
    // See LocalVar::eval / assignClosureVarValue for lookup priority strategy.
    return resolve_closure_var_value(var);
}

static bool incrementClosureVarIntFast(LocalVar* var, int64_t delta, int64_t& result, ExceptionSink* xsink) {
    ClosureVarValue* cv = findClosureVarValueForIR(var);
    if (!cv || cv->isReadOnly() || cv->finalized || cv->val.getType() != NT_INT) {
        return false;
    }
    result = cv->val.getAsBigInt() + delta;
    discard(cv->val.assign(static_cast<int64>(result)), xsink);
    return !(xsink && *xsink);
}

// Variant of assignClosureVarValue that TRANSFERS ownership of value's reference.
// See assignLocalVarValueTransfer for rationale.
static void assignClosureVarValueTransfer(LocalVar* var, QoreValue value, ExceptionSink* xsink) {
    if (!var) {
        value.discard(xsink);
        return;
    }
    ClosureVarValue* cv = findClosureVarValueForIR(var);
    if (!cv) {
        // Fall back to local stack assignment
        LValueHelper helper(xsink);
        if (var->getLValue(helper, false, false)) {
            value.discard(xsink);
            return;
        }
        helper.assign(value);
        return;
    }
    LValueHelper helper(xsink);
    if (cv->getLValue(helper, false)) {
        value.discard(xsink);
        return;
    }
    helper.assign(value);
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

static void assignObjectMemberValue(QoreObject* obj, const char* key, const QoreValue& value,
        ExceptionSink* xsink) {
    const qore_class_private* class_ctx = runtime_get_class();
    if (class_ctx && !qore_class_private::runtimeCheckPrivateClassAccess(*obj->getClass(), class_ctx)) {
        class_ctx = nullptr;
    }

    LValueHelper helper(xsink);
    if (qore_object_private::getLValue(*obj, key, helper, class_ctx, false, xsink)) {
        return;
    }

    QoreValue stored = value.hasNode() ? value.refSelf() : value;
    helper.assign(stored);
}


static void updateLocalVarFromLvalue(
        std::unordered_set<const LocalVar*>& instantiated_locals,
        std::vector<const LocalVar*>& instantiated_locals_ordered,
        const QoreValue& lvalue,
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
        ensureLocalInstantiated(var_ref->ref.id, instantiated_locals, instantiated_locals_ordered, pre_instantiated,
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

static void countIRValueUse(std::vector<uint32_t>& counts, QoreIRValue value) {
    if (value.isValid() && value.id < counts.size()) {
        ++counts[value.id];
    }
}

static void markIRValueUse(std::vector<uint8_t>& uses, QoreIRValue value) {
    if (value.isValid() && value.id < uses.size()) {
        uses[value.id] = 1;
    }
}

static bool buildValueUseCounts(const QoreIRFunction& func,
        std::vector<uint32_t>& counts, std::vector<uint8_t>& dot_eval_only_bases,
        ExceptionSink* xsink) {
    counts.assign(func.max_value_id + 1, 0);
    std::vector<uint8_t> dot_eval_base_candidates(counts.size(), 0);
    std::vector<uint8_t> non_dot_eval_uses(counts.size(), 0);
    size_t inst_count = 0;
    for (const auto& block : func.blocks) {
        for (const auto& inst_uptr : block->instructions) {
            if (((++inst_count % 100) == 0)
                    && qore_check_cancel(xsink, "IR interpreter analysis")) {
                return false;
            }
            const QoreIRInstruction* inst = inst_uptr.get();
            for (QoreIRValue operand : inst->operands) {
                countIRValueUse(counts, operand);
            }
            switch (inst->opcode) {
                case QoreIROpcode::BrIf: {
                    auto* br = static_cast<const QoreIRBranchIfInstruction*>(inst);
                    countIRValueUse(counts, br->condition);
                    markIRValueUse(non_dot_eval_uses, br->condition);
                    break;
                }
                case QoreIROpcode::SwitchInt: {
                    auto* sw = static_cast<const QoreIRSwitchIntInstruction*>(inst);
                    countIRValueUse(counts, sw->switch_val);
                    markIRValueUse(non_dot_eval_uses, sw->switch_val);
                    break;
                }
                case QoreIROpcode::SwitchString: {
                    auto* sw = static_cast<const QoreIRSwitchStringInstruction*>(inst);
                    countIRValueUse(counts, sw->switch_val);
                    markIRValueUse(non_dot_eval_uses, sw->switch_val);
                    break;
                }
                case QoreIROpcode::Phi: {
                    auto* phi = static_cast<const QoreIRPhiInstruction*>(inst);
                    for (const QoreIRPhiIncoming& incoming : phi->incoming) {
                        countIRValueUse(counts, incoming.value);
                        markIRValueUse(non_dot_eval_uses, incoming.value);
                    }
                    break;
                }
                case QoreIROpcode::Return: {
                    auto* ret = static_cast<const QoreIRReturnInstruction*>(inst);
                    if (ret->has_value) {
                        countIRValueUse(counts, ret->value);
                        markIRValueUse(non_dot_eval_uses, ret->value);
                    }
                    break;
                }
                case QoreIROpcode::IteratorCreate:
                case QoreIROpcode::IteratorCreateIterate: {
                    // IteratorCreateReverse is intentionally absent: it is built as a
                    // plain QoreIRInstruction with its iterable in operands (already
                    // counted above), so casting it to QoreIRIteratorCreateInstruction
                    // here would read a non-existent field.
                    auto* iter = static_cast<const QoreIRIteratorCreateInstruction*>(inst);
                    countIRValueUse(counts, iter->iterable);
                    markIRValueUse(non_dot_eval_uses, iter->iterable);
                    break;
                }
                case QoreIROpcode::IteratorNext: {
                    auto* iter = static_cast<const QoreIRIteratorNextInstruction*>(inst);
                    countIRValueUse(counts, iter->iterator);
                    markIRValueUse(non_dot_eval_uses, iter->iterator);
                    break;
                }
                default:
                    break;
            }

            bool dot_eval = inst->opcode == QoreIROpcode::DotEvalMethodDirect
                || inst->opcode == QoreIROpcode::InvokeDotEvalMethodDirect;
            if (!dot_eval && inst->opcode == QoreIROpcode::Invoke
                    && !inst->operands.empty()) {
                auto* inv = static_cast<const QoreIRInvokeInstruction*>(inst);
                dot_eval = isDotEvalInvokeOpcode(inv->invoke_opcode);
            }

            if (dot_eval && !inst->operands.empty()) {
                markIRValueUse(dot_eval_base_candidates, inst->operands[0]);
                for (size_t i = 1; i < inst->operands.size(); ++i) {
                    markIRValueUse(non_dot_eval_uses, inst->operands[i]);
                }
            } else {
                for (QoreIRValue operand : inst->operands) {
                    markIRValueUse(non_dot_eval_uses, operand);
                }
            }
        }
    }
    dot_eval_only_bases.assign(counts.size(), 0);
    for (size_t i = 0; i < dot_eval_base_candidates.size(); ++i) {
        if (i && ((i % 100) == 0) && qore_check_cancel(xsink, "IR interpreter analysis")) {
            return false;
        }
        if (dot_eval_base_candidates[i] && !non_dot_eval_uses[i]) {
            dot_eval_only_bases[i] = 1;
        }
    }
    return true;
}

static bool localWriteMayInvalidateCallerCaches(const QoreIRFunction& func, const LocalVar* lv) {
    if (!lv) {
        return true;
    }
    if (!func.ir_only_locals.count(reinterpret_cast<const void*>(lv))) {
        return true;
    }
    return lv->closureUse() || QoreTypeInfo::isReference(lv->getTypeInfo());
}

static bool varRefWriteMayInvalidateCallerCaches(const QoreIRFunction& func, const VarRefNode* var) {
    if (!var) {
        return true;
    }
    switch (var->getType()) {
        case VT_GLOBAL:
        case VT_LOCAL_TS:
        case VT_CLOSURE:
        case VT_IMMEDIATE:
            return true;
        case VT_LOCAL:
            return localWriteMayInvalidateCallerCaches(func, var->ref.id);
        default:
            return true;
    }
}

static bool containerWriteMayInvalidateCallerCaches(const QoreIRFunction& func, const VarRefNode* container,
        const LocalVar* container_lv) {
    return container_lv ? localWriteMayInvalidateCallerCaches(func, container_lv)
        : varRefWriteMayInvalidateCallerCaches(func, container);
}

static bool lvaluePathMayInvalidateCallerCaches(const QoreIRFunction& func,
        const QoreIRLValuePathInstruction* inst) {
    if (!inst || inst->path.empty()) {
        return true;
    }
    const LVPathStep& root = inst->path.front();
    switch (root.kind) {
        case LVPathStepKind::LocalVar: {
            auto* lv = reinterpret_cast<const LocalVar*>(root.ref_ptr);
            return localWriteMayInvalidateCallerCaches(func, lv)
                || (root.type_info && QoreTypeInfo::isReference(root.type_info));
        }
        case LVPathStepKind::SelfMember:
            return false;
        case LVPathStepKind::ClosureVar:
        case LVPathStepKind::GlobalVar:
        case LVPathStepKind::ThreadLocalVar:
        case LVPathStepKind::StaticVar:
            return true;
        default:
            return true;
    }
}

static bool instructionMayInvalidateCallerCaches(const QoreIRFunction& func, const QoreIRInstruction* inst) {
    if (!inst) {
        return true;
    }
    switch (inst->opcode) {
        case QoreIROpcode::StoreClosure:
        case QoreIROpcode::StoreGlobal:
        case QoreIROpcode::StoreThreadLocal:
        case QoreIROpcode::NewObject:
        case QoreIROpcode::CreateParseRef:
        case QoreIROpcode::Call:
        case QoreIROpcode::CallIndirect:
        case QoreIROpcode::CallMethod:
        case QoreIROpcode::CallStatic:
        case QoreIROpcode::CallStaticDirect:
        case QoreIROpcode::DotEvalAny:
        case QoreIROpcode::DotEvalInt:
        case QoreIROpcode::DotEvalFloat:
        case QoreIROpcode::DotEvalString:
        case QoreIROpcode::DotEvalDate:
        case QoreIROpcode::DotEvalList:
        case QoreIROpcode::DotEvalHash:
        case QoreIROpcode::DotEvalObject:
        case QoreIROpcode::BackgroundInt:
        case QoreIROpcode::Invoke:
        case QoreIROpcode::InvokeSimError:
        case QoreIROpcode::OnBlockExit:
        case QoreIROpcode::ScopeExit:
        case QoreIROpcode::Backquote:
            return true;

        case QoreIROpcode::StoreLocal: {
            auto* local_inst = static_cast<const QoreIRLocalInstruction*>(inst);
            return localWriteMayInvalidateCallerCaches(func, local_inst->local);
        }
        case QoreIROpcode::AddAssignLocalInt: {
            auto* add_inst = static_cast<const QoreIRAddAssignLocalIntInstruction*>(inst);
            return !add_inst->target_ir_only || localWriteMayInvalidateCallerCaches(func, add_inst->target);
        }
        case QoreIROpcode::IncrementLocalInt: {
            auto* inc_inst = static_cast<const QoreIRIncrementLocalIntInstruction*>(inst);
            return !inc_inst->ir_only || localWriteMayInvalidateCallerCaches(func, inc_inst->local);
        }
        case QoreIROpcode::HashKeyStore: {
            auto* store_inst = static_cast<const QoreIRHashKeyStoreInstruction*>(inst);
            return containerWriteMayInvalidateCallerCaches(func, store_inst->container, store_inst->container_lv);
        }
        case QoreIROpcode::HashKeyStoreDynamic: {
            auto* store_inst = static_cast<const QoreIRHashKeyStoreDynamicInstruction*>(inst);
            return containerWriteMayInvalidateCallerCaches(func, store_inst->container, store_inst->container_lv);
        }
        case QoreIROpcode::ListIndexStore: {
            auto* store_inst = static_cast<const QoreIRListIndexStoreInstruction*>(inst);
            return varRefWriteMayInvalidateCallerCaches(func, store_inst->container);
        }
        case QoreIROpcode::StoreLValue:
        case QoreIROpcode::PreIncLValue:
        case QoreIROpcode::PreDecLValue:
        case QoreIROpcode::PostIncLValue:
        case QoreIROpcode::PostDecLValue:
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
        case QoreIROpcode::ShiftLValue:
        case QoreIROpcode::UnshiftLValue:
        case QoreIROpcode::PopAny:
        case QoreIROpcode::PushAny:
        case QoreIROpcode::SpliceLValue: {
            auto* lval_inst = static_cast<const QoreIRLValueInstruction*>(inst);
            return varRefWriteMayInvalidateCallerCaches(func, extractLValueBaseVarRef(lval_inst->lvalue));
        }
        case QoreIROpcode::LValuePathAssign:
        case QoreIROpcode::LValuePathCompound:
        case QoreIROpcode::LValuePathUnary:
        case QoreIROpcode::LValuePathBinaryMut:
        case QoreIROpcode::LValuePathTernary:
            return lvaluePathMayInvalidateCallerCaches(func, static_cast<const QoreIRLValuePathInstruction*>(inst));

        case QoreIROpcode::CallDirect: {
            auto* direct_inst = static_cast<const QoreIRCallDirectInstruction*>(inst);
            return !direct_inst->is_self_recursive
                && (!direct_inst->func || direct_inst->func != func.source_qf);
        }

        default:
            return false;
    }
}

static bool buildInterpreterAnalysis(const QoreIRFunction& func, ExceptionSink* xsink) {
    std::vector<uint32_t> value_use_counts;
    std::vector<uint8_t> dot_eval_only_bases;
    if (!buildValueUseCounts(func, value_use_counts, dot_eval_only_bases, xsink)) {
        return false;
    }
    std::vector<int32_t> operand_use_counts(func.max_value_id + 1, 0);
    std::vector<uint8_t> return_protected_slots(func.max_value_id + 1, 0);
    std::vector<uint32_t> return_value_slot_ids;
    std::vector<uint32_t> return_preserve_slot_ids;
    bool needs_slot_cache_tls = false;
    bool may_invalidate_external_caches = false;
    size_t inst_count = 0;
    for (const auto& b : func.blocks) {
        for (const auto& inst_ptr : b->instructions) {
            if (((++inst_count % 100) == 0)
                    && qore_check_cancel(xsink, "IR interpreter analysis")) {
                return false;
            }
            if (!inst_ptr) {
                continue;
            }
            if (inst_ptr->opcode == QoreIROpcode::OnBlockExit) {
                needs_slot_cache_tls = true;
            }
            if (inst_ptr->opcode == QoreIROpcode::Return) {
                auto* ret = static_cast<const QoreIRReturnInstruction*>(inst_ptr.get());
                if (ret->has_value && ret->value.isValid() && ret->value.id < return_protected_slots.size()) {
                    return_protected_slots[ret->value.id] = 1;
                    return_value_slot_ids.push_back(ret->value.id);
                }
            } else if (inst_ptr->opcode == QoreIROpcode::RefSelf && inst_ptr->result.isValid()
                    && inst_ptr->result.id < return_protected_slots.size()) {
                return_protected_slots[inst_ptr->result.id] = 1;
                return_preserve_slot_ids.push_back(inst_ptr->result.id);
            }
            if (!may_invalidate_external_caches
                    && instructionMayInvalidateCallerCaches(func, inst_ptr.get())) {
                may_invalidate_external_caches = true;
            }
            auto countOperand = [&operand_use_counts](QoreIRValue op) {
                if (op.isValid() && op.id < operand_use_counts.size()) {
                    ++operand_use_counts[op.id];
                }
            };
            for (const auto& op : inst_ptr->operands) {
                countOperand(op);
            }
            // Some instructions carry value operands in dedicated fields that are
            // not present in inst->operands.  These must be counted here too, or a
            // value whose only consumer is such a field (e.g. a call result used
            // directly as a foreach/map iterable) is seen as having zero uses and
            // is wrongly discarded as dead.  Mirror buildValueUseCounts(), but only
            // for the fields that are *not* also mirrored into operands (Phi and
            // IteratorCreateReverse already push their operands, so are skipped).
            switch (inst_ptr->opcode) {
                case QoreIROpcode::BrIf:
                    countOperand(static_cast<const QoreIRBranchIfInstruction*>(inst_ptr.get())->condition);
                    break;
                case QoreIROpcode::SwitchInt:
                    countOperand(static_cast<const QoreIRSwitchIntInstruction*>(inst_ptr.get())->switch_val);
                    break;
                case QoreIROpcode::SwitchString:
                    countOperand(static_cast<const QoreIRSwitchStringInstruction*>(inst_ptr.get())->switch_val);
                    break;
                case QoreIROpcode::IteratorCreate:
                case QoreIROpcode::IteratorCreateIterate:
                    countOperand(static_cast<const QoreIRIteratorCreateInstruction*>(inst_ptr.get())->iterable);
                    break;
                case QoreIROpcode::IteratorNext:
                    countOperand(static_cast<const QoreIRIteratorNextInstruction*>(inst_ptr.get())->iterator);
                    break;
                case QoreIROpcode::Return: {
                    auto* ret = static_cast<const QoreIRReturnInstruction*>(inst_ptr.get());
                    if (ret->has_value) {
                        countOperand(ret->value);
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }
    int max_param_idx = -1;
    for (const auto& [idx, slot_id] : func.param_slot_ids) {
        (void)slot_id;
        if (idx > max_param_idx) {
            max_param_idx = idx;
        }
    }
    for (const auto& [idx, lv] : func.param_local_vars) {
        (void)lv;
        if (idx > max_param_idx) {
            max_param_idx = idx;
        }
    }
    std::vector<uint32_t> param_slot_ids;
    std::vector<const LocalVar*> param_local_vars;
    if (max_param_idx >= 0) {
        param_slot_ids.assign(static_cast<size_t>(max_param_idx) + 1, UINT32_MAX);
        param_local_vars.assign(static_cast<size_t>(max_param_idx) + 1, nullptr);
        for (const auto& [idx, slot_id] : func.param_slot_ids) {
            if (idx >= 0) {
                param_slot_ids[static_cast<size_t>(idx)] = slot_id;
            }
        }
        for (const auto& [idx, lv] : func.param_local_vars) {
            if (idx >= 0) {
                param_local_vars[static_cast<size_t>(idx)] = lv;
            }
        }
    }
    std::vector<uint8_t> locals_ir_only;
    bool has_non_ir_only_locals = false;
    size_t local_slot_count = func.local_var_slots.empty()
        ? 0 : static_cast<size_t>(func.max_local_slot_id) + 1;
    if (local_slot_count > 0) {
        locals_ir_only.assign(local_slot_count, 0);
        for (auto& [lvar, sid] : func.local_var_slots) {
            if (lvar && sid < local_slot_count) {
                if (func.ir_only_locals.count(reinterpret_cast<const void*>(lvar))) {
                    locals_ir_only[sid] = 1;
                } else {
                    has_non_ir_only_locals = true;
                }
            }
        }
    }
    func.interpreter_value_use_counts = std::move(value_use_counts);
    func.interpreter_dot_eval_only_bases = std::move(dot_eval_only_bases);
    func.interpreter_operand_use_counts = std::move(operand_use_counts);
    func.interpreter_param_slot_ids = std::move(param_slot_ids);
    func.interpreter_param_local_vars = std::move(param_local_vars);
    func.interpreter_locals_ir_only = std::move(locals_ir_only);
    func.interpreter_return_protected_slots = std::move(return_protected_slots);
    func.interpreter_return_value_slot_ids = std::move(return_value_slot_ids);
    func.interpreter_return_preserve_slot_ids = std::move(return_preserve_slot_ids);
    func.interpreter_needs_slot_cache_tls = needs_slot_cache_tls;
    func.interpreter_has_non_ir_only_locals = has_non_ir_only_locals;
    func.interpreter_may_invalidate_external_caches = may_invalidate_external_caches;
    func.interpreter_analysis_ready = true;
    return true;
}

static bool ensureInterpreterAnalysis(const QoreIRFunction& func, ExceptionSink* xsink) {
    if (func.interpreter_analysis_ready) {
        return true;
    }
    std::lock_guard<std::mutex> lock(func.interpreter_analysis_mutex);
    return func.interpreter_analysis_ready || buildInterpreterAnalysis(func, xsink);
}

static LocalVar* findIRSelfLocalForInterpreter(const QoreIRFunction* ir) {
    if (!ir) {
        return nullptr;
    }
    LocalVar* named_self = nullptr;
    for (const auto& [lv, slot_id] : ir->local_var_slots) {
        (void)slot_id;
        if (!lv || !lv->getName()) {
            continue;
        }
        if (lv->isSelf()) {
            return const_cast<LocalVar*>(lv);
        }
        if (!named_self && !strcmp(lv->getName(), "self")) {
            named_self = const_cast<LocalVar*>(lv);
        }
    }
    return named_self;
}

static bool methodVariantFastCallEligibleForInterpreter(const AbstractQoreFunctionVariant* variant) {
    const auto* mvb = dynamic_cast<const MethodVariantBase*>(variant);
    if (mvb) {
        return mvb->isStaticallyFastMethodCallEligible();
    }
    const UserVariantBase* uvb = variant ? variant->getUserVariantBase() : nullptr;
    return uvb && uvb->isStaticallyFastCallEligible();
}

static inline bool exactPrimitiveTypeAcceptsValue(const QoreTypeInfo* type_info, const QoreValue& value) {
    qore_type_t value_type = value.getType();
    return (type_info == bigIntTypeInfo && value_type == NT_INT)
        || (type_info == floatTypeInfo && value_type == NT_FLOAT)
        || (type_info == boolTypeInfo && value_type == NT_BOOLEAN)
        || (type_info == stringTypeInfo && value_type == NT_STRING)
        || (type_info == charTypeInfo && value_type == NT_CHAR);
}

static void acceptInterpreterIRReturnType(const UserSignature* sig, const QoreTypeInfo* return_type,
        const QoreTypeInfo* receiver_type_info, QoreValue& value, ExceptionSink* xsink) {
    const QoreTypeInfo* rt = return_type;
    if (!rt) {
        rt = sig ? sig->getReturnTypeInfo() : nullptr;
        if (receiver_type_info) {
            rt = qore_substitute_type_params_if_needed(rt, receiver_type_info);
        } else if (sig && sig->needsTypeParameterSubstitution()) {
            rt = qore_substitute_type_params_if_needed(rt);
        }
    } else if (receiver_type_info) {
        rt = qore_substitute_type_params_if_needed(rt, receiver_type_info);
    } else if (sig && sig->needsTypeParameterSubstitution()) {
        rt = qore_substitute_type_params_if_needed(rt);
    }

    if (!rt || rt == autoTypeInfo) {
        return;
    }
    if (exactPrimitiveTypeAcceptsValue(rt, value)) {
        return;
    }

    if (value.isNothing() && rt && QoreTypeInfo::hasType(rt)) {
        QoreTypeInfo::acceptAssignment(rt, "<block return>", value, xsink, nullptr);
        if (xsink && *xsink && sig) {
            xsink->overrideLocation(*sig->getParseLocation());
            xsink->appendLastDescription(": block missing return statement");
        }
    } else {
        QoreTypeInfo::acceptAssignment(rt, "<return statement>", value, xsink);
    }
}

static bool preserveInterpreterFastCallArg(QoreValue val) {
    switch (val.getType()) {
        case NT_REFERENCE:
        case NT_WEAKREF:
        case NT_WEAKREF_HASH:
        case NT_WEAKREF_LIST:
            return true;
        default:
            return false;
    }
}

static int instantiateInterpreterFastCallParams(const UserSignature* sig, unsigned num_params,
        const uint64_t* args, ExceptionSink* xsink, const QoreTypeInfo* receiver_type_info = nullptr) {
    for (unsigned i = 0; i < num_params; ++i) {
        QoreValue raw = fromBits(args[i]);
        QoreValue val;
        if (!raw.needsEval() || preserveInterpreterFastCallArg(raw)) {
            val = raw.refSelf();
        } else {
            ValueEvalOptimizedRefHolder eval_arg(raw, xsink);
            if (xsink && *xsink) {
                for (int j = static_cast<int>(i) - 1; j >= 0; --j) {
                    sig->lv[j]->uninstantiate(xsink);
                }
                return -1;
            }
            val = eval_arg.getReferencedValue();
        }

        const QoreTypeInfo* param_type_info = sig->getParamTypeInfo(i);
        if (receiver_type_info) {
            param_type_info = qore_substitute_type_params_if_needed(param_type_info, receiver_type_info);
        }
        if (param_type_info && !exactPrimitiveTypeAcceptsValue(param_type_info, val)
                && (QoreTypeInfo::hasType(param_type_info)
                || QoreTypeInfo::mayRequireFilter(param_type_info, val))) {
            QoreTypeInfo::acceptInputParam(param_type_info, i, sig->getName(i), val, xsink);
            if (xsink && *xsink) {
                val.discard(xsink);
                for (int j = static_cast<int>(i) - 1; j >= 0; --j) {
                    sig->lv[j]->uninstantiate(xsink);
                }
                return -1;
            }
        }

        sig->lv[i]->instantiate(val);
    }
    return 0;
}

static void uninstantiateInterpreterFastCallParams(const UserSignature* sig, unsigned num_params,
        ExceptionSink* xsink) {
    for (int i = static_cast<int>(num_params) - 1; i >= 0; --i) {
        sig->lv[i]->uninstantiate(xsink);
    }
}

class InterpreterFastCallParamScope {
public:
    InterpreterFastCallParamScope(const UserSignature* sig, unsigned num_params, ExceptionSink* xsink)
            : sig(sig), num_params(num_params), xsink(xsink) {
    }

    ~InterpreterFastCallParamScope() {
        cleanup();
    }

    void activateParams() {
        params_instantiated = true;
    }

    void instantiateEmptyArgv() {
        assert(sig && sig->argvid);
        sig->argvid->instantiate(QoreValue());
        argv_instantiated = true;
    }

    void cleanup() {
        if (!params_instantiated) {
            return;
        }
        if (argv_instantiated) {
            sig->argvid->uninstantiate(xsink);
            argv_instantiated = false;
        }
        uninstantiateInterpreterFastCallParams(sig, num_params, xsink);
        params_instantiated = false;
    }

private:
    const UserSignature* sig;
    unsigned num_params;
    ExceptionSink* xsink;
    bool params_instantiated = false;
    bool argv_instantiated = false;
};

template <typename DirectCallInst>
static int8_t ensureInterpreterResolvedInlineIRCallState(DirectCallInst* inst, const QoreMethod* method,
        const AbstractQoreFunctionVariant* variant, QoreProgram* caller_pgm, int nargs, bool reject_copy_method) {
    auto reject = [&]() -> int8_t {
        inst->inline_ir_state.store(-1, std::memory_order_release);
        return -1;
    };
    int8_t state = inst->inline_ir_state.load(std::memory_order_acquire);
    if (state > 0 && inst->cached_callee_ir && inst->cached_uvb) {
        return state;
    }
    if (state == -1) {
        return -1;
    }

    if (!variant || inst->has_ref_args || !method) {
        return reject();
    }
    if (reject_copy_method && !strcmp(method->getName(), "copy")) {
        return reject();
    }

    const UserVariantBase* uvb = inst->cached_uvb ? inst->cached_uvb : variant->getUserVariantBase();
    if (!uvb || uvb->pgm != caller_pgm || uvb->hasCachedFunction()
            || !methodVariantFastCallEligibleForInterpreter(variant)) {
        return reject();
    }

    const QoreIRFunction* callee_ir = inst->cached_callee_ir ? inst->cached_callee_ir : uvb->getCachedIR();
    const UserSignature* sig = uvb->getUserSignature();
    if (!callee_ir || !sig
            || sig->needsTypeParameterSubstitution()
            || !callee_ir->ast_visible_body_locals.empty()
            || callee_ir->hasTypeSpecialization()
            || ((!callee_ir->direct_params_eligible || nargs < static_cast<int>(sig->numParams()))
                && nargs != static_cast<int>(sig->numParams()))) {
        return reject();
    }

    inst->cached_callee_ir = callee_ir;
    inst->cached_uvb = uvb;
    inst->cached_return_type = sig->getReturnTypeInfo();
    int8_t eligible_state = callee_ir->direct_params_eligible
        && nargs >= static_cast<int>(sig->numParams()) ? 1 : 2;
    inst->inline_ir_state.store(eligible_state, std::memory_order_release);
    return eligible_state;
}

static int8_t ensureInterpreterInlineIRFunctionState(QoreIRCallDirectInstruction* inst,
        QoreProgram* caller_pgm, int nargs) {
    auto reject = [&]() -> int8_t {
        inst->inline_ir_state.store(-1, std::memory_order_release);
        return -1;
    };
    int8_t state = inst->inline_ir_state.load(std::memory_order_acquire);
    if (state > 0 && inst->cached_callee_ir && inst->cached_uvb) {
        return state;
    }
    if (state == -1) {
        return -1;
    }

    if (!caller_pgm) {
        caller_pgm = getProgram();
    }
    if (!inst->variant || inst->has_ref_args || !caller_pgm) {
        return reject();
    }
    const UserVariantBase* uvb = inst->cached_uvb ? inst->cached_uvb : inst->variant->getUserVariantBase();
    if (!uvb || uvb->pgm != caller_pgm || uvb->hasCachedFunction()
            || !uvb->isStaticallyFastCallEligible()) {
        return reject();
    }

    const QoreIRFunction* callee_ir = inst->cached_callee_ir ? inst->cached_callee_ir : uvb->getCachedIR();
    const UserSignature* sig = uvb->getUserSignature();
    if (!callee_ir || !sig
            || sig->needsTypeParameterSubstitution()
            || !callee_ir->ast_visible_body_locals.empty()
            || callee_ir->hasTypeSpecialization()
            || ((!callee_ir->direct_params_eligible || nargs < static_cast<int>(sig->numParams()))
                && nargs != static_cast<int>(sig->numParams()))) {
        return reject();
    }

    inst->cached_callee_ir = callee_ir;
    inst->cached_uvb = uvb;
    inst->cached_return_type = sig->getReturnTypeInfo();
    int8_t eligible_state = callee_ir->direct_params_eligible
        && nargs >= static_cast<int>(sig->numParams()) ? 1 : 2;
    inst->inline_ir_state.store(eligible_state, std::memory_order_release);
    return eligible_state;
}

static bool tryExecuteInterpreterInlineIRFunction(QoreIRCallDirectInstruction* inst,
        QoreProgram* caller_pgm, uint64_t* args, int nargs, QoreValue& result, ExceptionSink* xsink,
        bool* may_invalidate_external_caches = nullptr) {
    int8_t inline_state = ensureInterpreterInlineIRFunctionState(inst, caller_pgm, nargs);
    if (inline_state <= 0) {
        return false;
    }

    const UserVariantBase* uvb = inst->cached_uvb;
    if (uvb->hasCachedFunction()) {
        inst->inline_ir_state.store(-1, std::memory_order_release);
        return false;
    }

    const QoreIRFunction* callee_ir = inst->cached_callee_ir;
    const UserSignature* sig = uvb->getUserSignature();
    unsigned num_params = sig->numParams();
    bool use_direct_params = inline_state == 1;
    if (!use_direct_params
            && instantiateInterpreterFastCallParams(sig, num_params, args, xsink) < 0) {
        result = QoreValue();
        return true;
    }
    std::optional<InterpreterFastCallParamScope> param_scope;
    if (!use_direct_params) {
        param_scope.emplace(sig, num_params, xsink);
        param_scope->activateParams();
        if (sig->argvid) {
            param_scope->instantiateEmptyArgv();
        }
    }

    std::optional<ThreadSafeLocalVarRuntimeEnvironmentHelper> closure_env_clear;
    if (thread_has_runtime_closure_env()) {
        closure_env_clear.emplace(nullptr);
    }
    QoreValue ir_return_value;
    IRDirectParams dp{args, nargs};
    const IRDirectParams* direct_params = use_direct_params ? &dp : nullptr;
    bool ok = QoreIRInterpreter::execute(*callee_ir, ir_return_value, xsink,
        nullptr, nullptr, nullptr, callee_ir->cached_pre_instantiated,
        nullptr, uvb->getStatementBlock(), uvb->pgm, false, direct_params);
    if (param_scope) {
        param_scope->cleanup();
    }
    if (!ok && !(xsink && *xsink)) {
        ir_return_value.discard(xsink);
        inst->inline_ir_state.store(-1, std::memory_order_release);
        return false;
    }

    if (!(xsink && *xsink)) {
        acceptInterpreterIRReturnType(sig, inst->cached_return_type, nullptr, ir_return_value, xsink);
    }
    if (xsink && *xsink) {
        ir_return_value.discard(xsink);
        result = QoreValue();
    } else {
        result = ir_return_value;
    }
    if (may_invalidate_external_caches) {
        *may_invalidate_external_caches = callee_ir->interpreter_may_invalidate_external_caches;
    }
    return true;
}

template <typename DirectMethodInst>
static bool executeInterpreterInlineIRMethodTarget(DirectMethodInst* inst, const QoreMethod* method,
        QoreObject* self, const QoreTypeInfo* receiver_type_info, int8_t inline_state,
        uint64_t* args, int nargs, QoreValue& result, ExceptionSink* xsink,
        bool* may_invalidate_external_caches = nullptr) {
    const UserVariantBase* uvb = inst->cached_uvb;
    if (uvb->hasCachedFunction()) {
        inst->inline_ir_state.store(-1, std::memory_order_release);
        return false;
    }

    const QoreIRFunction* callee_ir = inst->cached_callee_ir;
    const UserSignature* sig = uvb->getUserSignature();
    unsigned num_params = sig->numParams();
    ObjectSubstitutionHelper osh(self, qore_class_private::get(*method->getClass()));
    const LocalVar* selfid = sig->selfid ? sig->selfid : findIRSelfLocalForInterpreter(callee_ir);
    SelfInstantiationHelper self_helper(selfid, self);

    bool use_direct_params = inline_state == 1;
    if (!use_direct_params
            && instantiateInterpreterFastCallParams(sig, num_params, args, xsink, receiver_type_info) < 0) {
        result = QoreValue();
        return true;
    }
    std::optional<InterpreterFastCallParamScope> param_scope;
    if (!use_direct_params) {
        param_scope.emplace(sig, num_params, xsink);
        param_scope->activateParams();
        if (sig->argvid) {
            param_scope->instantiateEmptyArgv();
        }
    }

    QoreValue ir_return_value;
    IRDirectParams dp{args, nargs};
    const IRDirectParams* direct_params = use_direct_params ? &dp : nullptr;
    bool ok = QoreIRInterpreter::execute(*callee_ir, ir_return_value, xsink,
        nullptr, nullptr, nullptr, callee_ir->cached_pre_instantiated,
        nullptr, uvb->getStatementBlock(), uvb->pgm, false, direct_params);
    if (param_scope) {
        param_scope->cleanup();
    }
    if (!ok && !(xsink && *xsink)) {
        ir_return_value.discard(xsink);
        inst->inline_ir_state.store(-1, std::memory_order_release);
        return false;
    }

    if (!(xsink && *xsink)) {
        acceptInterpreterIRReturnType(sig, inst->cached_return_type, receiver_type_info, ir_return_value, xsink);
    }
    if (xsink && *xsink) {
        ir_return_value.discard(xsink);
        result = QoreValue();
    } else {
        result = ir_return_value;
    }
    if (may_invalidate_external_caches) {
        *may_invalidate_external_caches = callee_ir->interpreter_may_invalidate_external_caches;
    }
    return true;
}

template <typename DirectMethodInst>
static bool tryExecuteInterpreterInlineIRMethod(DirectMethodInst* inst, QoreObject* self,
        QoreProgram* caller_pgm, uint64_t* args, int nargs, QoreValue& result, ExceptionSink* xsink,
        bool* may_invalidate_external_caches = nullptr) {
    if (!self || !caller_pgm) {
        return false;
    }
    int8_t inline_state = ensureInterpreterResolvedInlineIRCallState(
        inst, inst->method, inst->variant, caller_pgm, nargs, true);
    if (inline_state <= 0) {
        return false;
    }

    return executeInterpreterInlineIRMethodTarget(inst, inst->method, self, nullptr,
        inline_state, args, nargs, result, xsink, may_invalidate_external_caches);
}

static const AbstractQoreFunctionVariant* getSingleInterpreterMethodVariant(const QoreMethod* method) {
    if (!method) {
        return nullptr;
    }
    const qore_method_private* priv = qore_method_private::get(*method);
    const QoreFunction* func = priv ? priv->getFunction() : nullptr;
    return func && func->numVariants() == 1 ? func->first() : nullptr;
}

template <typename DotEvalInst>
static bool tryExecuteInterpreterInlineIRDotEvalMethod(DotEvalInst* inst, QoreValue base,
        const char* method_name, const QoreTypeParamInstantiation* explicit_inst, QoreProgram* caller_pgm,
        uint64_t* args, int nargs, QoreValue& result, ExceptionSink* xsink,
        bool* may_invalidate_external_caches = nullptr) {
    if (!caller_pgm || inst->pseudo || explicit_inst || !method_name) {
        return false;
    }

    QoreObject* self = nullptr;
    if (base.getType() == NT_OBJECT) {
        self = const_cast<QoreObject*>(reinterpret_cast<const QoreObject*>(base.getInternalNode()));
    } else if (base.getType() == NT_WEAKREF) {
        self = base.get<const WeakReferenceNode>()->get();
        if (!self || !self->isValid()) {
            xsink->raiseException("OBJECT-ALREADY-DELETED",
                "cannot call '%s()' on a deleted weak reference", method_name);
            result = QoreValue();
            return true;
        }
    } else {
        return false;
    }

    if (!self->isValid()) {
        xsink->raiseException("OBJECT-ALREADY-DELETED",
            "cannot call '%s()' on an object that has already been deleted", method_name);
        result = QoreValue();
        return true;
    }

    int8_t state = inst->inline_ir_state.load(std::memory_order_acquire);
    const QoreClass* object_class = self->getClass();
    if (state > 0) {
        if (inst->cached_object_class != object_class || !inst->cached_method) {
            return false;
        }
        if (!inst->cached_method->isPrivate()) {
            return executeInterpreterInlineIRMethodTarget(inst, inst->cached_method, self, nullptr,
                state, args, nargs, result, xsink, may_invalidate_external_caches);
        }
    }

    const qore_class_private* class_ctx = runtime_get_class();
    if (class_ctx && !qore_class_private::parseCheckPrivateClassAccess(*object_class, class_ctx)) {
        class_ctx = nullptr;
    }

    if (state > 0) {
        if (inst->cached_class_ctx != class_ctx) {
            return false;
        }
        return executeInterpreterInlineIRMethodTarget(inst, inst->cached_method, self, nullptr,
            state, args, nargs, result, xsink, may_invalidate_external_caches);
    }
    if (state == -1) {
        return false;
    }

    const QoreMethod* method = inst->method;
    const AbstractQoreFunctionVariant* variant = inst->variant;
    if (method) {
        if ((inst->qc && object_class != inst->qc) && object_class != method->getClass()) {
            return false;
        }
    } else {
        if (!strcmp(method_name, "copy")) {
            inst->inline_ir_state.store(-1, std::memory_order_release);
            return false;
        }
        const qore_class_private* priv = qore_class_private::get(*object_class);
        method = priv->getMethodForEval(method_name, self->getProgram(), class_ctx, xsink);
        if (xsink && *xsink) {
            result = QoreValue();
            return true;
        }
        if (!method) {
            return false;
        }
        variant = getSingleInterpreterMethodVariant(method);
        if (!variant) {
            inst->inline_ir_state.store(-1, std::memory_order_release);
            return false;
        }
    }

    inst->cached_object_class = object_class;
    inst->cached_class_ctx = class_ctx;
    inst->cached_method = method;
    state = ensureInterpreterResolvedInlineIRCallState(inst, method, variant, caller_pgm, nargs, true);
    if (state <= 0) {
        return false;
    }

    return executeInterpreterInlineIRMethodTarget(inst, method, self, nullptr,
        state, args, nargs, result, xsink, may_invalidate_external_caches);
}

static bool tryExecuteInterpreterInlineIRStaticMethod(QoreIRCallStaticDirectInstruction* inst,
        QoreProgram* caller_pgm, uint64_t* args, int nargs, QoreValue& result, ExceptionSink* xsink,
        bool* may_invalidate_external_caches = nullptr) {
    int8_t inline_state = caller_pgm
        ? ensureInterpreterResolvedInlineIRCallState(inst, inst->method, inst->variant, caller_pgm, nargs, false) : -1;
    if (inline_state <= 0) {
        return false;
    }

    const UserVariantBase* uvb = inst->cached_uvb;
    if (uvb->hasCachedFunction()) {
        inst->inline_ir_state.store(-1, std::memory_order_release);
        return false;
    }

    const QoreIRFunction* callee_ir = inst->cached_callee_ir;
    const UserSignature* sig = uvb->getUserSignature();
    unsigned num_params = sig->numParams();
    ClassOnlySubstitutionHelper cosh(qore_class_private::get(*inst->method->getClass()));
    bool use_direct_params = inline_state == 1;
    if (!use_direct_params
            && instantiateInterpreterFastCallParams(sig, num_params, args, xsink) < 0) {
        result = QoreValue();
        return true;
    }
    std::optional<InterpreterFastCallParamScope> param_scope;
    if (!use_direct_params) {
        param_scope.emplace(sig, num_params, xsink);
        param_scope->activateParams();
        if (sig->argvid) {
            param_scope->instantiateEmptyArgv();
        }
    }

    QoreValue ir_return_value;
    IRDirectParams dp{args, nargs};
    const IRDirectParams* direct_params = use_direct_params ? &dp : nullptr;
    bool ok = QoreIRInterpreter::execute(*callee_ir, ir_return_value, xsink,
        nullptr, nullptr, nullptr, callee_ir->cached_pre_instantiated,
        nullptr, uvb->getStatementBlock(), uvb->pgm, false, direct_params);
    if (param_scope) {
        param_scope->cleanup();
    }
    if (!ok && !(xsink && *xsink)) {
        ir_return_value.discard(xsink);
        inst->inline_ir_state.store(-1, std::memory_order_release);
        return false;
    }

    if (!(xsink && *xsink)) {
        acceptInterpreterIRReturnType(sig, inst->cached_return_type, nullptr, ir_return_value, xsink);
    }
    if (xsink && *xsink) {
        ir_return_value.discard(xsink);
        result = QoreValue();
    } else {
        result = ir_return_value;
    }
    if (may_invalidate_external_caches) {
        *may_invalidate_external_caches = callee_ir->interpreter_may_invalidate_external_caches;
    }
    return true;
}

static const QoreIRIncrementLocalIntInstruction* getSimpleClosureIncrementInstruction(
        const QoreIRFunction* callee_ir) {
    if (!callee_ir) {
        return nullptr;
    }
    const QoreIRIncrementLocalIntInstruction* inc_inst = nullptr;
    bool has_return_nothing = false;
    for (const auto& block : callee_ir->blocks) {
        if (!block) {
            continue;
        }
        for (const auto& inst_ptr : block->instructions) {
            if (!inst_ptr) {
                continue;
            }
            switch (inst_ptr->opcode) {
                case QoreIROpcode::DebugBlock:
                case QoreIROpcode::PushTempMark:
                case QoreIROpcode::DiscardTemps:
                    break;
                case QoreIROpcode::IncrementLocalInt: {
                    if (inc_inst) {
                        return nullptr;
                    }
                    auto* candidate = static_cast<const QoreIRIncrementLocalIntInstruction*>(inst_ptr.get());
                    if (!candidate->local || !candidate->local->closureUse() || candidate->ir_only) {
                        return nullptr;
                    }
                    inc_inst = candidate;
                    break;
                }
                case QoreIROpcode::ReturnNothing:
                    has_return_nothing = true;
                    break;
                default:
                    return nullptr;
            }
        }
    }
    return inc_inst && has_return_nothing ? inc_inst : nullptr;
}

static bool tryExecuteSimpleClosureIncrement(const QoreClosureBase* cb,
        const QoreIRIncrementLocalIntInstruction* inc_inst, QoreValue& result, ExceptionSink* xsink) {
    if (!cb || !inc_inst || !inc_inst->local) {
        return false;
    }
    ClosureVarValue* stack_cvv = thread_try_find_closure_var(inc_inst->local->getName());
    ClosureVarValue* env_cvv = cb->find(inc_inst->local);
    ClosureVarValue* cv = (stack_cvv && env_cvv && stack_cvv != env_cvv) ? stack_cvv
        : (env_cvv ? env_cvv : stack_cvv);
    if (!cv || cv->isReadOnly() || cv->finalized || cv->val.getType() != NT_INT) {
        return false;
    }
    int64_t result_val = cv->val.getAsBigInt() + inc_inst->delta;
    discard(cv->val.assign(static_cast<int64>(result_val)), xsink);
    result = QoreValue();
    return !(xsink && *xsink);
}

static bool tryExecuteInterpreterInlineIRClosure(QoreValue ref_val, QoreProgram* caller_pgm,
        uint64_t* args, int nargs, QoreValue& result, ExceptionSink* xsink,
        bool* may_invalidate_external_caches = nullptr) {
    if (!ref_val.hasNode()) {
        return false;
    }
    const AbstractQoreNode* node = ref_val.getInternalNode();
    if (node->getType() != NT_RUNTIME_CLOSURE) {
        return false;
    }

    auto* cb = static_cast<const QoreClosureBase*>(node);
    auto* uf = static_cast<UserClosureFunction*>(cb->getFunction());
    if (!uf || !uf->numVariants()) {
        return false;
    }
    if (!caller_pgm) {
        caller_pgm = getProgram();
    }
    if (!caller_pgm) {
        return false;
    }

    struct ClosureInlineCache {
        const AbstractQoreNode* node = nullptr;
        UserClosureFunction* function = nullptr;
        QoreProgram* caller_pgm = nullptr;
        const UserVariantBase* uvb = nullptr;
        const QoreIRFunction* callee_ir = nullptr;
        const UserSignature* sig = nullptr;
        const QoreIRIncrementLocalIntInstruction* simple_increment = nullptr;
        unsigned num_params = 0;
        bool direct_params_eligible = false;
    };
    static thread_local ClosureInlineCache closure_inline_cache;

    const UserVariantBase* uvb = nullptr;
    const QoreIRFunction* callee_ir = nullptr;
    const UserSignature* sig = nullptr;
    const QoreIRIncrementLocalIntInstruction* simple_increment = nullptr;
    unsigned num_params = 0;
    bool use_cached_metadata = closure_inline_cache.node == node
        && closure_inline_cache.function == uf
        && closure_inline_cache.caller_pgm == caller_pgm
        && closure_inline_cache.uvb
        && closure_inline_cache.callee_ir
        && closure_inline_cache.sig
        && ((closure_inline_cache.direct_params_eligible
                && nargs >= static_cast<int>(closure_inline_cache.num_params))
            || nargs == static_cast<int>(closure_inline_cache.num_params));
    if (use_cached_metadata) {
        uvb = closure_inline_cache.uvb;
        if (uvb->hasCachedFunction()) {
            closure_inline_cache = ClosureInlineCache();
            return false;
        }
        callee_ir = closure_inline_cache.callee_ir;
        sig = closure_inline_cache.sig;
        simple_increment = closure_inline_cache.simple_increment;
        num_params = closure_inline_cache.num_params;
    } else {
        const AbstractQoreFunctionVariant* variant = uf->first();
        uvb = variant ? variant->getUserVariantBase() : nullptr;
        if (!uvb || uvb->pgm != caller_pgm || uvb->hasCachedFunction()
                || !uvb->isStaticallyFastCallEligible()) {
            return false;
        }

        callee_ir = uvb->getCachedIR();
        sig = uvb->getUserSignature();
        if (!callee_ir || !sig
                || sig->needsTypeParameterSubstitution()
                || !callee_ir->ast_visible_body_locals.empty()
                || callee_ir->hasTypeSpecialization()
                || ((!callee_ir->direct_params_eligible || nargs < static_cast<int>(sig->numParams()))
                    && nargs != static_cast<int>(sig->numParams()))) {
            return false;
        }
        num_params = sig->numParams();
        simple_increment = nargs == 0 ? getSimpleClosureIncrementInstruction(callee_ir) : nullptr;
        closure_inline_cache = {
            node,
            uf,
            caller_pgm,
            uvb,
            callee_ir,
            sig,
            simple_increment,
            num_params,
            callee_ir->direct_params_eligible,
        };
    }
    if (nargs == 0 && simple_increment
            && tryExecuteSimpleClosureIncrement(cb, simple_increment, result, xsink)) {
        if (may_invalidate_external_caches) {
            *may_invalidate_external_caches = true;
        }
        return true;
    }
    ClosureTlpdEnsureHelper tlpd_helper(xsink, uvb->pgm);
    if (xsink && *xsink) {
        result = QoreValue();
        return true;
    }
    CVecInstantiator cvi(cb->getCvec(), xsink);
    if (xsink && *xsink) {
        result = QoreValue();
        return true;
    }
    ThreadSafeLocalVarRuntimeEnvironmentHelper closure_env(cb);

    QoreObject* self = const_cast<QoreObject*>(cb->getObject());
    std::optional<QoreClosureSelfContextHelper> closure_self_ctx;
    std::optional<ObjectSubstitutionHelper> object_ctx;
    if (self) {
        closure_self_ctx.emplace(self);
        if (qore_ir_check_closure_self_valid(self, xsink)) {
            result = QoreValue();
            return true;
        }
        object_ctx.emplace(self, cb->getClassCtx());
    }

    const LocalVar* selfid = sig->selfid ? sig->selfid : findIRSelfLocalForInterpreter(callee_ir);
    SelfInstantiationHelper self_helper(selfid, self);

    bool use_direct_params = callee_ir->direct_params_eligible
        && nargs >= static_cast<int>(num_params);
    if (!use_direct_params
            && instantiateInterpreterFastCallParams(sig, num_params, args, xsink) < 0) {
        result = QoreValue();
        return true;
    }
    std::optional<InterpreterFastCallParamScope> param_scope;
    if (!use_direct_params) {
        param_scope.emplace(sig, num_params, xsink);
        param_scope->activateParams();
        if (sig->argvid) {
            param_scope->instantiateEmptyArgv();
        }
    }

    QoreValue ir_return_value;
    IRDirectParams dp{args, nargs};
    const IRDirectParams* direct_params = use_direct_params ? &dp : nullptr;
    bool ok = QoreIRInterpreter::execute(*callee_ir, ir_return_value, xsink,
        nullptr, nullptr, nullptr, callee_ir->cached_pre_instantiated,
        nullptr, uvb->getStatementBlock(), uvb->pgm, false, direct_params);
    if (param_scope) {
        param_scope->cleanup();
    }
    if (!ok && !(xsink && *xsink)) {
        ir_return_value.discard(xsink);
        return false;
    }

    if (!(xsink && *xsink)) {
        acceptInterpreterIRReturnType(sig, nullptr, nullptr, ir_return_value, xsink);
    }
    if (xsink && *xsink) {
        ir_return_value.discard(xsink);
        result = QoreValue();
    } else {
        result = ir_return_value;
    }
    if (may_invalidate_external_caches) {
        *may_invalidate_external_caches = callee_ir->interpreter_may_invalidate_external_caches;
    }
    return true;
}

static void countCallArgOccurrences(const IRValueSlots& values,
        const std::vector<QoreIRValue>& operands, size_t start_index,
        const std::vector<uint32_t>& value_use_counts,
        std::unordered_map<uint32_t, uint32_t>& occurrences) {
    occurrences.clear();
    occurrences.reserve(operands.size() - start_index);
    for (size_t i = start_index; i < operands.size(); ++i) {
        QoreIRValue operand = operands[i];
        if (operand.isValid() && operand.id < values.size()
                && operand.id < value_use_counts.size()) {
            ++occurrences[operand.id];
        }
    }
}

static void consumeLastUseCallArgSlots(IRValueSlots& values, std::vector<uint32_t>& cleanup,
        const std::vector<QoreIRValue>& operands, size_t start_index,
        const std::vector<uint32_t>& value_use_counts,
        const std::unordered_map<uint32_t, int32_t>* borrowed_slots, ExceptionSink* xsink) {
    std::unordered_map<uint32_t, uint32_t> occurrences;
    countCallArgOccurrences(values, operands, start_index, value_use_counts, occurrences);
    uint32_t checked = 0;
    for (const auto& [id, occurrences_in_args] : occurrences) {
        if (((++checked % 100) == 0)
                && qore_check_cancel(xsink, "IR call argument cleanup")) {
            return;
        }
        if (value_use_counts[id] != occurrences_in_args) {
            continue;
        }
        QoreValue& slot = values[id];
        if (!slot.hasNode()) {
            continue;
        }
        removeAllCleanupEntries(cleanup, id);
        if (borrowed_slots && borrowed_slots->find(id) != borrowed_slots->end()) {
            slot = QoreValue();
            continue;
        }
        slot.discard(xsink);
        slot = QoreValue();
    }
}

static bool hasLastUseCallArgSlots(const IRValueSlots& values, const std::vector<QoreIRValue>& operands,
        size_t start_index, const std::vector<uint32_t>& value_use_counts) {
    std::unordered_map<uint32_t, uint32_t> occurrences;
    countCallArgOccurrences(values, operands, start_index, value_use_counts, occurrences);
    for (const auto& [id, occurrences_in_args] : occurrences) {
        if (value_use_counts[id] != occurrences_in_args) {
            continue;
        }
        if (values[id].hasNode()) {
            return true;
        }
    }
    return false;
}

static QoreValue raiseIRAstFallback(ExceptionSink* xsink, const char* kind,
        const QoreIRFunction* func, const QoreIRBasicBlock* block, size_t ip,
        const QoreIRInstruction* inst, QoreIROpcode invoke_opcode, const QoreValue& expr) {
    if (xsink) {
        const QoreProgramLocation* loc = inst ? inst->loc : nullptr;
        const AbstractQoreNode* node = expr.getInternalNode();
        int line = loc ? loc->start_line + loc->offset : -1;
        xsink->raiseException("IR-AST-FALLBACK-ERROR",
            "AST expression fallback is disabled: kind='%s' function='%s' block='%s' ip=%zu "
            "opcode=%s(%d) invoke_opcode=%s(%d) result_slot=%d expr_type=%s node_type=%s source=%s:%d",
            kind ? kind : "unknown",
            func ? func->name.c_str() : "",
            block ? block->name.c_str() : "",
            ip,
            inst ? getOpcodeName(static_cast<int>(inst->opcode)) : "<none>",
            inst ? static_cast<int>(inst->opcode) : -1,
            getOpcodeName(static_cast<int>(invoke_opcode)),
            static_cast<int>(invoke_opcode),
            inst && inst->result.isValid() ? static_cast<int>(inst->result.id) : -1,
            expr.getTypeName(),
            node ? typeid(*node).name() : "<null>",
            loc ? loc->getFileValue() : "",
            line);
    }
    return QoreValue();
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
        const IRValueSlots& values, ExceptionSink* xsink,
        const QoreIRFunction* func, const QoreIRBasicBlock* block, size_t ip,
        bool preserve_hash_key_weak_result = false) {
    QoreIROpcode op = inv->invoke_opcode;

    switch (op) {
        // Unary computation opcodes
        case QoreIROpcode::ToBool:
        case QoreIROpcode::ToInt:
        case QoreIROpcode::ToFloat:
        case QoreIROpcode::Not:
        case QoreIROpcode::IsNullOrNothing:
        case QoreIROpcode::UnaryPlusAny:
        case QoreIROpcode::UnaryMinusInt:
        case QoreIROpcode::UnaryMinusFloat:
        case QoreIROpcode::UnaryMinusAny:
        case QoreIROpcode::ExistsAny:
        case QoreIROpcode::ExistsBool:
        case QoreIROpcode::IsCollectionType:
        case QoreIROpcode::ElementsAny:
        case QoreIROpcode::ElementsInt: {
            QoreValue val = inv->operands.empty() ? QoreValue() : getIRValue(values, inv->operands[0]);
            return QoreIRInterpreter::evalUnary(op, val, xsink);
        }
        case QoreIROpcode::ToString: {
                QoreValue val = inv->operands.empty() ? QoreValue() : getIRValue(values, inv->operands[0]);
                switch (val.getType()) {
                case NT_STRING: {
                    QoreStringNodeValueHelper str(val);
                    return QoreValue(str.getReferencedValue());
                }
                case NT_INT: {
                    char buf[64];
                    int len = snprintf(buf, sizeof(buf), QLLD, val.getAsBigInt());
                    return QoreValue::makeStringValue(buf, len > 0 ? static_cast<size_t>(len) : 0);
                }
                case NT_FLOAT:
                    return QoreValue(q_fix_decimal(new QoreStringNodeMaker("%.9g", val.getAsFloat()), 0));
                case NT_BOOLEAN:
                    return val.getAsBool()
                        ? QoreValue::makeStringValue("1", 1)
                        : QoreValue::makeStringValue("0", 1);
                case NT_NOTHING:
                case NT_NULL:
                    return QoreValue::makeStringValue("");
                default: {
                    QoreStringValueHelper sv(val);
                    return QoreValue::makeStringValue(sv->c_str(), sv->size(), sv->getEncoding());
                }
            }
        }
        case QoreIROpcode::IterateValue: {
            QoreValue source = inv->operands.empty() ? QoreValue() : getIRValue(values, inv->operands[0]);
            return QoreIterateOperatorNode::evalIteratorValue(source,
                QoreIterateOperatorNode::getElementTypeInfo(source, source.getTypeInfo()), xsink);
        }
        // Binary computation opcodes (arithmetic, bitwise, compound assignments, comparisons, etc.)
        case QoreIROpcode::AddInt:
        case QoreIROpcode::AddFloat:
        case QoreIROpcode::AddAny:
        case QoreIROpcode::AddTimeout:
        case QoreIROpcode::AddString:
        case QoreIROpcode::StringConcat:
        case QoreIROpcode::SubInt:
        case QoreIROpcode::SubFloat:
        case QoreIROpcode::SubAny:
        case QoreIROpcode::SubTimeout:
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
        case QoreIROpcode::HashDerefDynamic:
        case QoreIROpcode::ListIndexDynamic:
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
        // Ternary computation opcodes.  These can be emitted as Invoke
        // instructions inside try/on_error regions so exceptions branch to the
        // handler, but the operands are already evaluated SSA values.
        case QoreIROpcode::RangeSliceAny:
        case QoreIROpcode::RangeSliceInt:
        case QoreIROpcode::RangeSliceFloat: {
            if (inv->operands.size() < 3) {
                if (xsink) {
                    xsink->raiseException("IR-EXEC-ERROR",
                        "RangeSlice invoke requires three operands; got %zu",
                        inv->operands.size());
                }
                return QoreValue();
            }
            QoreValue first = getIRValue(values, inv->operands[0]);
            QoreValue second = getIRValue(values, inv->operands[1]);
            QoreValue third = getIRValue(values, inv->operands[2]);
            return QoreIRInterpreter::evalTernary(op, first, second, third, xsink);
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
            return raiseIRAstFallback(xsink, "invoke", func, block, ip, inv, op, inv->expr);
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
            return raiseIRAstFallback(xsink, "invoke", func, block, ip, inv, op, inv->expr);
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
                        lhs_val, rhs_val, true, sq->hasStringIndexChar(), sq->hasNegativeOffsets(), xsink);
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
            if (op != QoreIROpcode::CallIndirect) {
                const ParseNode* parse_node = nullptr;
                if (inv->expr.hasNode()) {
                    parse_node = dynamic_cast<const ParseNode*>(inv->expr.getInternalNode());
                }
                const QoreProgramLocation* loc = parse_node ? parse_node->loc : nullptr;
                QoreListNode* arg_list = buildArgListFromIROperands(inv->operands, 0,
                    inv->operands.size(), values);
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
            } else if (!inv->operands.empty()) {
                const ParseNode* parse_node = nullptr;
                if (inv->expr.hasNode()) {
                    parse_node = dynamic_cast<const ParseNode*>(inv->expr.getInternalNode());
                }
                const QoreProgramLocation* loc = parse_node ? parse_node->loc : nullptr;
                QoreListNode* arg_list = buildArgListFromIROperands(inv->operands, 1,
                    inv->operands.size(), values);
                if (xsink && *xsink) {
                    if (arg_list) {
                        arg_list->deref(xsink);
                    }
                    return QoreValue();
                }
                bool used_operands = false;
                QoreValue res;
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
                if (!used_operands && arg_list) {
                    arg_list->deref(xsink);
                }
                if (used_operands) {
                    return res;
                }
            }
            // Fall through: no operands or native call metadata failed — report
            // unsupported lowering instead of evaluating the original AST.
            // Handle ScopedObjectCallNode (bare "new") directly
            if (inv->expr.hasNode()) {
                auto* scoped = dynamic_cast<const ScopedObjectCallNode*>(
                    inv->expr.getInternalNode());
                if (scoped && scoped->oc) {
                    RuntimeConfig& rc = rc_get_current_ref();
                    const QoreTypeInfo* object_type_info
                        = qore_substitute_type_params_if_needed(scoped->getObjectTypeInfo());
                    return qore_class_private::execConstructor(*scoped->oc, rc,
                        scoped->getVariant(), scoped->getArgs(), xsink, object_type_info);
                }
            }
            return raiseIRAstFallback(xsink, "invoke", func, block, ip, inv, op, inv->expr);
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
            // No supported native dot-eval lowering was available.
            return raiseIRAstFallback(xsink, "invoke", func, block, ip, inv, op, inv->expr);
        }

        // NewObject: construct object with pre-computed NaN-boxed operand values.
        // Class and variant are extracted from `inv->expr` metadata (no AST
        // evaluation). Args come from inv->operands, NOT from AST nodes.
        case QoreIROpcode::NewObject: {
            const QoreClass* qc = nullptr;
            const AbstractQoreFunctionVariant* variant = nullptr;
            const QoreTypeInfo* object_type_info = nullptr;
            if (inv->expr.hasNode()) {
                if (auto* vrn = dynamic_cast<const VarRefNewObjectNode*>(
                        inv->expr.getInternalNode())) {
                    qc = QoreTypeInfo::getUniqueReturnClass(vrn->getTypeInfo());
                    variant = vrn->getVariant();
                    object_type_info = vrn->getTypeInfo();
                } else if (auto* scoped = dynamic_cast<const ScopedObjectCallNode*>(
                        inv->expr.getInternalNode())) {
                    qc = scoped->oc;
                    variant = scoped->getVariant();
                    object_type_info = scoped->getObjectTypeInfo();
                } else if (auto* nocn = dynamic_cast<const NewObjectCallNode*>(
                        inv->expr.getInternalNode())) {
                    qc = nocn->getClass();
                    variant = nocn->getVariant();
                    object_type_info = nocn->getObjectTypeInfo();
                }
            }
            if (!qc) {
                xsink->raiseException("RUNTIME-ERROR",
                    "NewObject invoke: class not resolved");
                return QoreValue();
            }
            object_type_info = qore_substitute_type_params_if_needed(object_type_info);
            // Build NaN-boxed arg array from pre-computed IR operands
            int nargs = static_cast<int>(inv->operands.size());
            constexpr int SMALL_BUF = 8;
            uint64_t nb_buf[SMALL_BUF];
            uint64_t* nb_args = nargs <= SMALL_BUF ? nb_buf : new uint64_t[nargs];
            for (int i = 0; i < nargs; ++i) {
                nb_args[i] = toBits(getIRValue(values, inv->operands[i]));
            }
            ReferenceHolder<QoreListNode> arg_list(xsink);
            if (nargs > 0) {
                arg_list = qore_list_private::newList(false);
                qore_list_private* priv = qore_list_private::get(**arg_list);
                priv->reserve(nargs);
                for (int i = 0; i < nargs; ++i) {
                    QoreValue val = fromBits(nb_args[i]);
                    if (val.hasNode()) {
                        val.refSelf();
                    }
                    priv->pushIntern(val);
                }
            }
            if (nargs > SMALL_BUF) delete[] nb_args;
            RuntimeConfig& rc = rc_get_current_ref();
            return qore_class_private::execConstructor(*qc, rc, variant,
                nargs > 0 ? *arg_list : nullptr, xsink, object_type_info);
        }

        // VrnConstruct: construct hashdecl/complex types without local variable assignment
        case QoreIROpcode::VrnConstruct: {
            if (inv->expr.hasNode()) {
                auto* node = inv->expr.getInternalNode();
                auto* vrn = dynamic_cast<const VarRefNewObjectNode*>(node);
                if (!inv->operands.empty()) {
                    QoreValue init = getIRValue(values, inv->operands[0]);
                    if (vrn && vrn->isComplexHashConstruct()) {
                        return fromBits(qore_rt_new_complex_hash_from_hash(vrn->getTypeInfo(),
                            toBits(init), xsink));
                    }
                    if (vrn && vrn->isComplexListConstruct()) {
                        return fromBits(qore_rt_new_complex_list_from_value(vrn->getTypeInfo(),
                            toBits(init), xsink));
                    }
                    if (auto* nch = dynamic_cast<const NewComplexHashNode*>(node)) {
                        return fromBits(qore_rt_new_complex_hash_from_hash(nch->typeInfo,
                            toBits(init), xsink));
                    }
                    if (auto* ncl = dynamic_cast<const NewComplexListNode*>(node)) {
                        return fromBits(qore_rt_new_complex_list_from_value(ncl->typeInfo,
                            toBits(init), xsink));
                    }
                    return raiseIRAstFallback(xsink, "invoke", func, block, ip, inv, op, inv->expr);
                }
                if (vrn) {
                    return vrn->constructValue(xsink);
                }
            }
            // Direct eval — avoids evalExprNode() overhead
            return raiseIRAstFallback(xsink, "invoke", func, block, ip, inv, op, inv->expr);
        }

        // NewHashDeclFromHash: construct hashdecl from pre-lowered hash operand
        case QoreIROpcode::NewHashDeclFromHash: {
            QoreValue hash_val = inv->operands.empty() ? QoreValue() : getIRValue(values, inv->operands[0]);
            const TypedHashDecl* hd = nullptr;
            bool runtime_check = false;
            if (inv->expr.hasNode()) {
                auto* vrn = dynamic_cast<const VarRefNewObjectNode*>(inv->expr.getInternalNode());
                if (vrn) {
                    const QoreTypeInfo* runtime_type_info
                        = qore_substitute_type_params_if_needed(vrn->getTypeInfo());
                    hd = QoreTypeInfo::getUniqueReturnHashDecl(runtime_type_info);
                    runtime_check = vrn->getRuntimeCheck();
                } else if (auto* nhd = dynamic_cast<const NewHashDeclNode*>(inv->expr.getInternalNode())) {
                    const QoreTypeInfo* runtime_type_info
                        = qore_substitute_type_params_if_needed(nhd->hd->getTypeInfo());
                    hd = QoreTypeInfo::getUniqueReturnHashDecl(runtime_type_info);
                    runtime_check = nhd->runtime_check;
                }
            }
            if (hd) {
                const QoreHashNode* init = nullptr;
                if (hash_val.getType() != NT_NOTHING) {
                    if (hash_val.getType() != NT_HASH) {
                        xsink->raiseException("HASHDECL-INIT-ERROR",
                            "hashdecl '%s' hash initializer value must be a hash; got type '%s' instead",
                            hd->getName(), hash_val.getTypeName());
                        return QoreValue();
                    }
                    init = hash_val.get<const QoreHashNode>();
                }
                QoreHashNode* result = typed_hash_decl_private::get(*hd)->newHash(init,
                    runtime_check, xsink);
                return result ? QoreValue(result) : QoreValue();
            }
            return QoreValue();
        }

        case QoreIROpcode::LoadConstant: {
            if (auto* node = dynamic_cast<const RuntimeConstantRefNode*>(
                    inv->expr.getInternalNode())) {
                return fromBits(qore_rt_load_constant(node, xsink));
            }
            if (inv->expr.needsEval()) {
                return inv->expr.eval(xsink);
            }
            return inv->expr.refSelf();
        }

        case QoreIROpcode::LoadSelfMember: {
            if (auto* self_ref = dynamic_cast<const SelfVarrefNode*>(
                    inv->expr.getInternalNode())) {
                uint64_t result_bits = preserve_hash_key_weak_result
                    ? qore_rt_load_self_member_for_call(self_ref->str, xsink)
                    : qore_rt_load_self_member(self_ref->str, xsink);
                return fromBits(result_bits);
            }
            return raiseIRAstFallback(xsink, "invoke", func, block, ip, inv, op, inv->expr);
        }

        case QoreIROpcode::NewHashDecl: {
            if (auto* node = dynamic_cast<const NewHashDeclNode*>(inv->expr.getInternalNode())) {
                return fromBits(qore_rt_new_hash_decl(node, xsink));
            }
            return raiseIRAstFallback(xsink, "invoke", func, block, ip, inv, op, inv->expr);
        }

        case QoreIROpcode::NewComplexHash: {
            if (auto* node = dynamic_cast<const NewComplexHashNode*>(inv->expr.getInternalNode())) {
                return fromBits(qore_rt_new_complex_hash(node, xsink));
            }
            return raiseIRAstFallback(xsink, "invoke", func, block, ip, inv, op, inv->expr);
        }

        case QoreIROpcode::NewComplexList: {
            if (auto* node = dynamic_cast<const NewComplexListNode*>(inv->expr.getInternalNode())) {
                return fromBits(qore_rt_new_complex_list(node, xsink));
            }
            return raiseIRAstFallback(xsink, "invoke", func, block, ip, inv, op, inv->expr);
        }

        case QoreIROpcode::NewComplexBuffer: {
            if (auto* node = dynamic_cast<const NewComplexBufferNode*>(inv->expr.getInternalNode())) {
                if (!inv->operands.empty()) {
                    QoreValue init = getIRValue(values, inv->operands[0]);
                    return fromBits(qore_rt_new_complex_buffer_from_value(node->typeInfo, toBits(init), xsink));
                }
                return fromBits(qore_rt_new_complex_buffer(node, xsink));
            }
            return raiseIRAstFallback(xsink, "invoke", func, block, ip, inv, op, inv->expr);
        }

        case QoreIROpcode::LoadStaticVar: {
            if (auto* static_var = dynamic_cast<const StaticClassVarRefNode*>(
                    inv->expr.getInternalNode())) {
                uint64_t result_bits = preserve_hash_key_weak_result
                    ? qore_rt_load_static_var_for_call(&static_var->vi, static_var->str.c_str(), xsink)
                    : qore_rt_load_static_var(&static_var->vi, static_var->str.c_str(), xsink);
                return fromBits(result_bits);
            }
            return raiseIRAstFallback(xsink, "invoke", func, block, ip, inv, op, inv->expr);
        }

        case QoreIROpcode::CreateClosure: {
            if (auto* closure = dynamic_cast<const QoreClosureParseNode*>(inv->expr.getInternalNode())) {
                return fromBits(qore_rt_create_closure(closure, xsink));
            }
            return raiseIRAstFallback(xsink, "invoke", func, block, ip, inv, op, inv->expr);
        }

        case QoreIROpcode::CreateCallRef:
            return fromBits(qore_rt_create_call_ref(toBits(inv->expr), xsink));

        case QoreIROpcode::CreateMethodRef:
            return fromBits(qore_rt_create_method_ref(toBits(inv->expr), xsink));

        case QoreIROpcode::CreateParseRef: {
            if (auto* parse_ref = dynamic_cast<const ParseReferenceNode*>(inv->expr.getInternalNode())) {
                if (!inv->operands.empty()) {
                    QoreValue key = getIRValue(values, inv->operands[0]);
                    return fromBits(qore_rt_create_parse_ref_resolved_hash_key(parse_ref, toBits(key), xsink));
                }
                return fromBits(qore_rt_create_parse_ref(parse_ref, xsink));
            }
            return raiseIRAstFallback(xsink, "invoke", func, block, ip, inv, op, inv->expr);
        }

        // HashKeyAccess invoke: native hash/object member access with pre-evaluated base.
        case QoreIROpcode::HashKeyAccess: {
            if (!inv->operands.empty()) {
                QoreValue base = getIRValue(values, inv->operands[0]);
                uint64_t base_bits = toBits(base);
                const char* key = inv->invoke_key_name.c_str();
                uint64_t result_bits = preserve_hash_key_weak_result
                    ? qore_rt_hash_key_access_for_call(base_bits, key, xsink)
                    : qore_rt_hash_key_access(base_bits, key, xsink);
                return fromBits(result_bits);
            }
            return raiseIRAstFallback(xsink, "invoke", func, block, ip, inv, op, inv->expr);
        }

        case QoreIROpcode::HashKeyAccessInt: {
            if (!inv->operands.empty()) {
                QoreValue base = getIRValue(values, inv->operands[0]);
                return fromBits(qore_rt_hash_key_access_int(toBits(base), inv->invoke_key_name.c_str()));
            }
            return raiseIRAstFallback(xsink, "invoke", func, block, ip, inv, op, inv->expr);
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
                const QoreTypeInfo* elem_type = substituteRuntimeTypeParams(
                    inv->element_type ? inv->element_type : autoTypeInfo);
                QoreListNode* l = new QoreListNode(elem_type);
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
            return raiseIRAstFallback(xsink, "invoke", func, block, ip, inv, op, inv->expr);
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
                    QoreValue inline_result;
                    if (tryExecuteInterpreterInlineIRClosure(ref_val, getProgram(), nullptr, 0,
                            inline_result, xsink)) {
                        return inline_result;
                    }
                    return fromBits(qore_rt_call_closure_0(ref_bits, xsink));
                }
                if (nargs == 1) {
                    uint64_t arg0 = toBits(getIRValue(values, inv->operands[1]));
                    QoreValue inline_result;
                    if (tryExecuteInterpreterInlineIRClosure(ref_val, getProgram(), &arg0, 1,
                            inline_result, xsink)) {
                        return inline_result;
                    }
                    return fromBits(qore_rt_call_closure_1(ref_bits, arg0, xsink));
                }
                // N-arg path: use stack buffer for small arg counts
                constexpr int SMALL_BUF = 8;
                uint64_t small_buf[SMALL_BUF];
                uint64_t* args = nargs <= SMALL_BUF ? small_buf : new uint64_t[nargs];
                for (int i = 0; i < nargs; ++i) {
                    args[i] = toBits(getIRValue(values, inv->operands[i + 1]));
                }
                QoreValue inline_result;
                bool inline_done = tryExecuteInterpreterInlineIRClosure(ref_val, getProgram(), args, nargs,
                    inline_result, xsink);
                uint64_t result_bits = inline_done ? 0 : qore_rt_call_closure_fast(ref_bits, args, nargs, xsink);
                if (nargs > SMALL_BUF) {
                    delete[] args;
                }
                return inline_done ? inline_result : fromBits(result_bits);
            }
            return raiseIRAstFallback(xsink, "invoke", func, block, ip, inv, op, inv->expr);
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
            return raiseIRAstFallback(xsink, "invoke", func, block, ip, inv, op, inv->expr);
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
            return raiseIRAstFallback(xsink, "invoke", func, block, ip, inv, op, inv->expr);
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
            return raiseIRAstFallback(xsink, "invoke", func, block, ip, inv, op, inv->expr);
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
            return raiseIRAstFallback(xsink, "invoke", func, block, ip, inv, op, inv->expr);
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
            return raiseIRAstFallback(xsink, "invoke", func, block, ip, inv, op, inv->expr);
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
            return raiseIRAstFallback(xsink, "invoke", func, block, ip, inv, op, inv->expr);
        }

        // Lvalue-modifying opcodes without native LValuePath lowering.
        // These fire when tryEmitLValuePathOp rejected the lvalue shape (e.g. an
        // exotic compound lvalue that extractLValuePath doesn't decompose yet).
        // They are errors so remaining shapes are fixed instead of silently
        // running through AST.
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
            if (op == QoreIROpcode::ListAssignAny && inv->operands.size() >= 2) {
                QoreValue rhs = getIRValue(values, inv->operands[0]);
                QoreValue idx = getIRValue(values, inv->operands[1]);
                return getListAssignmentValue(rhs, idx.getAsBigInt());
            }
            static const bool trace_lvmut = getenv("QORE_IR_TRACE_LVMUT_FALLBACK") != nullptr;
            if (trace_lvmut) {
                const QoreProgramLocation* loc = inv->loc;
                fprintf(stderr, "[ir-lvmut-fallback] opcode=%u loc=%s:%d\n",
                        static_cast<unsigned>(inv->opcode),
                        loc ? loc->getFileValue() : "?",
                        loc ? loc->start_line : 0);
                fflush(stderr);
            }
            return raiseIRAstFallback(xsink, "invoke", func, block, ip, inv, op, inv->expr);
        }

        // BackgroundInt: decomposed path with pre-evaluated args
        case QoreIROpcode::BackgroundInt: {
            auto* bg_op = dynamic_cast<const QoreBackgroundOperatorNode*>(
                inv->expr.getInternalNode());
            auto [matched, result] = runDecomposedBackground(bg_op, inv->operands,
                values, xsink);
            if (matched) {
                return result;
            }
            return raiseIRAstFallback(xsink, "invoke", func, block, ip, inv, op, inv->expr);
        }

        // Everything else (LoadLValue, expression ops, etc.)
        // evaluated through the original AST expression
        default:
            return raiseIRAstFallback(xsink, "invoke", func, block, ip, inv, op, inv->expr);
    }
}

// Execute a single on_block_exit handler body, using compiled IR if available, otherwise AST.
// Returns true if execution encountered an error.
static void executeHandlerBody(const IROnBlockExitHandler& handler, ExceptionSink* obe_xsink,
        std::vector<QoreValue>* parent_slot_cache = nullptr) {
    // Every on_block_exit handler must have been compiled to IR by
    // QoreIRLowering::compileAllHandlerIRs/compileBlockHandlerIRs before
    // the enclosing IR function is executed — a handler-lowering failure
    // now propagates as an outer-function lowering failure (the caller
    // falls back to AST at the function boundary).  If we still reach
    // here with a null handler_ir the invariant is broken; raise a
    // runtime error instead of silently AST-executing mid-IR.
    if (!handler.handler_ir) {
        assert(false && "OBE handler missing compiled IR — find & fix the upstream "
                        "lowering-failure propagation");
        if (obe_xsink) {
            obe_xsink->raiseException("IR-EXEC-ERROR",
                "internal: on_block_exit handler has no compiled IR — compile-time "
                "handler lowering must have failed silently");
        }
        return;
    }
    QoreValue rv;
    QoreIRInterpreter::execute(*handler.handler_ir, rv, obe_xsink, nullptr, nullptr, nullptr,
                               &handler.handler_ir->pre_instantiated_cache, nullptr, nullptr, nullptr,
                               false, nullptr, parent_slot_cache);
    rv.discard(obe_xsink);
}

// Execute on_block_exit handlers in reverse order (LIFO), matching the AST's
// StatementBlock::execIntern() semantics.  Returns the last non-zero return code, if any.
static int executeOnBlockExitHandlers(std::vector<IROnBlockExitHandler>& handlers, ExceptionSink* xsink,
        std::vector<QoreValue>* parent_slot_cache = nullptr) {
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
                    executeHandlerBody(handlers[i], &obe_xsink, parent_slot_cache);
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
// QoreIRStackLocation removed — see comment above for rationale.

bool QoreIRInterpreter::execute(const QoreIRFunction& func, QoreValue& return_value, ExceptionSink* xsink,
        std::vector<std::string>* cleanup_log, const std::vector<QoreValue>* args,
        const std::vector<QoreValue>* closure, const std::unordered_set<const LocalVar*>* pre_instantiated,
        const LocalVar* excluded_selfid, const StatementBlock* statements, QoreProgram* pgm, bool suppress_guard_deopt,
        const IRDirectParams* direct_params, std::vector<QoreValue>* parent_slot_cache) {
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
    size_t local_slot_count = func.local_var_slots.empty()
        ? 0 : static_cast<size_t>(func.max_local_slot_id) + 1;
    IRCallFrame& frame = tl_frame_pool.push(reserve_size, local_slot_count);
    // RAII guard: return the frame to the pool on any exit path
    struct FrameGuard {
        IRCallFrame& frame;
        ~FrameGuard() {
            // Pooled frames outlive the call.  Never let a dormant frame retain
            // refs from IR-only local caches or missed cleanup paths.
            frame.releaseReferences(nullptr);
            tl_frame_pool.pop();
        }
    } frame_guard{frame};

    IRValueSlots values{frame.values};
    auto& cleanup = frame.cleanup;
    auto& locals_slot_cache = frame.locals_slot_cache;
    auto& locals_lvar_cache = frame.locals_lvar_cache;
    auto& instantiated_locals = frame.instantiated_locals;
    auto& instantiated_locals_ordered = frame.instantiated_locals_ordered;
    auto& locally_uninstantiated = frame.locally_uninstantiated;
    auto& locals_ir_only = frame.locals_ir_only;
    auto& locals_instantiated = frame.locals_instantiated;
    auto& local_init_slots = frame.local_init_slots;
    auto& local_load_slots = frame.local_load_slots;
    auto& load_slot_registered = frame.load_slot_registered;
    auto& local_owned_slots = frame.local_owned_slots;
    if (!ensureInterpreterAnalysis(func, xsink)) {
        return false;
    }
    const std::vector<uint32_t>& value_use_counts = func.interpreter_value_use_counts;
    const std::vector<uint8_t>& dot_eval_only_bases = func.interpreter_dot_eval_only_bases;
    auto isDotEvalOnlyBase = [&](const QoreIRInstruction* i) -> bool {
        return i && i->result.isValid() && i->result.id < dot_eval_only_bases.size()
            && dot_eval_only_bases[i->result.id];
    };

    // Phase B2: Build reverse map for parent slot TLS access.
    // Maps slot_id -> LocalVar* for slots 0..parent_slot_count-1.
    // Dirty parent-slot writeback uses this to read the authoritative runtime
    // local value instead of an invalidated cache entry.
    std::vector<LocalVar*> parent_slot_to_lvar;
    if (func.parent_slot_count > 0) {
        parent_slot_to_lvar.resize(func.parent_slot_count, nullptr);
        for (auto& [lvar, slot_id] : func.local_var_slots) {
            if (lvar && slot_id < func.parent_slot_count) {
                parent_slot_to_lvar[slot_id] = const_cast<LocalVar*>(lvar);
            }
        }
    }
    std::vector<bool> parent_slot_dirty(func.parent_slot_count, false);
    std::vector<bool>* parent_slot_cache_dirty = nullptr;
    if (parent_slot_cache) {
        void** dirty_ptr = qore_rt_get_ir_slot_cache_dirty_ptr();
        if (dirty_ptr) {
            parent_slot_cache_dirty = reinterpret_cast<std::vector<bool>*>(*dirty_ptr);
            if (parent_slot_cache_dirty && parent_slot_cache_dirty->size() < func.parent_slot_count) {
                parent_slot_cache_dirty = nullptr;
            }
        }
    }
    auto markParentSlotDirty = [&](uint32_t slot_id) {
        if (slot_id < parent_slot_dirty.size()) {
            parent_slot_dirty[slot_id] = true;
        }
    };
    auto markParentLocalStoreDirty = [&](const QoreIRLocalInstruction* local_inst) {
        if (!local_inst || local_inst->slot_id == UINT32_MAX) {
            return;
        }
        if (local_inst->local && QoreTypeInfo::isReference(local_inst->local->getTypeInfo())) {
            return;
        }
        markParentSlotDirty(local_inst->slot_id);
    };

    // Phase B2: Copy parent's local values into handler frame (parent slot inheritance)
    // When this handler IR function is called from a parent, it inherits the parent's
    // local variable values for the first parent_slot_count slots
    if (func.parent_slot_count > 0 && parent_slot_cache) {
        assert(parent_slot_cache->size() >= func.parent_slot_count);
        for (uint32_t i = 0; i < func.parent_slot_count; ++i) {
            // Copy parent's value into handler's slot (with reference counting)
            const QoreValue& parent_val = (*parent_slot_cache)[i];
            locals_slot_cache[i] = parent_val;
            if (parent_val.hasNode()) {
                locals_slot_cache[i].refSelf();
            }
            // Mark as instantiated (parent is responsible for its own instantiation)
            locals_instantiated[i] = !parent_val.isNothing();
        }
    } else if (func.parent_slot_count > 0 && !parent_slot_to_lvar.empty()) {
        // TLS fallback: read parent locals from TLS stack (for LLVM-compiled parents)
        // LLVM-compiled functions instantiate locals on TLS via qore_rt_instantiate_local
        for (uint32_t i = 0; i < func.parent_slot_count; ++i) {
            const LocalVar* lvar = parent_slot_to_lvar[i];
            if (lvar) {
                // Read from TLS using the same mechanism as LoadLocal
                bool needs_deref = true;
                QoreValue parent_val = lvar->eval(needs_deref, xsink);
                if (!parent_val.isNothing()) {
                    locals_slot_cache[i] = parent_val.hasNode() ? parent_val.refSelf() : parent_val;
                    if (needs_deref && parent_val.hasNode()) {
                        parent_val.getInternalNode()->deref(xsink);
                    }
                    locals_instantiated[i] = true;
                }
            }
        }
    }

    // Phase B2: RAII guard for parent slot copy-back on all exit paths
    // Copies modified parent slot values back to parent's locals_slot_cache,
    // restoring parent's ownership of the references
    struct ParentSlotWriteback {
        uint32_t parent_slot_count;
        std::vector<QoreValue>& locals_slot_cache;
        std::vector<QoreValue>* parent_slot_cache;
        std::vector<LocalVar*>& slot_to_lvar;
        std::vector<bool>& parent_slot_dirty;
        std::vector<bool>* parent_slot_cache_dirty;
        ExceptionSink* xsink;

        ParentSlotWriteback(uint32_t psc, std::vector<QoreValue>& lsc,
                           std::vector<QoreValue>* psc_ptr, std::vector<LocalVar*>& stl,
                           std::vector<bool>& psd, std::vector<bool>* psc_dirty, ExceptionSink* xs)
            : parent_slot_count(psc), locals_slot_cache(lsc), parent_slot_cache(psc_ptr),
              slot_to_lvar(stl), parent_slot_dirty(psd),
              parent_slot_cache_dirty(psc_dirty), xsink(xs) {
        }

        QoreValue getWritebackValue(uint32_t i) {
            if (parent_slot_cache && i < locals_slot_cache.size()) {
                // Parent-slot-cache mode is used when a handler runs under an
                // LLVM/AOT parent.  Nested handler writebacks update this cache;
                // TLS can be stale during exception cleanup.
                QoreValue value = locals_slot_cache[i];
                if (!value.isNothing() || i >= slot_to_lvar.size() || !slot_to_lvar[i]) {
                    return value.hasNode() ? value.refSelf() : value;
                }
                // Some parent locals (notably reference/closure-use locals) are
                // not seeded into the native parent slot cache.  If the cache
                // entry is empty, fall back to the authoritative TLS value.
            }

            if (i < slot_to_lvar.size() && slot_to_lvar[i]) {
                bool needs_deref = true;
                QoreValue value = slot_to_lvar[i]->eval(needs_deref, xsink);
                if (xsink && *xsink) {
                    if (needs_deref && value.hasNode()) {
                        value.discard(xsink);
                    }
                    return QoreValue();
                }
                QoreValue rv = value.hasNode() ? value.refSelf() : value;
                if (needs_deref && value.hasNode()) {
                    value.discard(xsink);
                }
                return rv;
            }

            QoreValue value = locals_slot_cache[i];
            return value.hasNode() ? value.refSelf() : value;
        }

        ~ParentSlotWriteback() {
            // Copy modified parent slot values back to parent's locals_slot_cache
            if (parent_slot_count > 0 && parent_slot_cache) {
                assert(parent_slot_cache->size() >= parent_slot_count);
                for (uint32_t i = 0; i < parent_slot_count; ++i) {
                    if (!parent_slot_dirty[i]) {
                        continue;
                    }
                    QoreValue value = getWritebackValue(i);
                    (*parent_slot_cache)[i].discard(xsink);
                    (*parent_slot_cache)[i] = value;
                    if (parent_slot_cache_dirty) {
                        (*parent_slot_cache_dirty)[i] = true;
                    }
                }
            } else if (parent_slot_count > 0 && !slot_to_lvar.empty()) {
                // TLS write-back for LLVM-compiled parents
                // Write modified parent slot values back to TLS using assignLocalVarValue
                for (uint32_t i = 0; i < parent_slot_count; ++i) {
                    if (!parent_slot_dirty[i]) {
                        continue;
                    }
                    LocalVar* lvar = slot_to_lvar[i];
                    if (lvar) {
                        QoreValue value = getWritebackValue(i);
                        assignLocalVarValue(lvar, value, xsink);
                        value.discard(xsink);
                    }
                }
            }
        }
    };

    // Phase 2, Fix 2b: TLS guard for slot cache threading to exception-path handlers
    // When this IR interpreter frame is on the stack, nested handler IR functions can
    // access the parent's locals_slot_cache via qore_rt_exec_on_block_exit_impl
    struct TLSSlotCacheGuard {
        void** tls_slot_ptr;
        void** tls_dirty_ptr;
        std::vector<QoreValue>* prev_slot_cache;
        std::vector<bool>* prev_slot_cache_dirty;
        bool active = false;

        TLSSlotCacheGuard(std::vector<QoreValue>& locals_slot_cache, bool enable) {
            if (!enable) {
                return;
            }
            active = true;
            tls_slot_ptr = qore_rt_get_ir_slot_cache_ptr();
            tls_dirty_ptr = qore_rt_get_ir_slot_cache_dirty_ptr();
            prev_slot_cache = reinterpret_cast<std::vector<QoreValue>*>(*tls_slot_ptr);
            prev_slot_cache_dirty = reinterpret_cast<std::vector<bool>*>(*tls_dirty_ptr);
            *tls_slot_ptr = reinterpret_cast<void*>(&locals_slot_cache);
            *tls_dirty_ptr = nullptr;
        }

        ~TLSSlotCacheGuard() {
            if (!active) {
                return;
            }
            if (tls_slot_ptr) {
                *tls_slot_ptr = reinterpret_cast<void*>(prev_slot_cache);
            }
            if (tls_dirty_ptr) {
                *tls_dirty_ptr = reinterpret_cast<void*>(prev_slot_cache_dirty);
            }
        }
    } tls_slot_guard(locals_slot_cache, func.interpreter_needs_slot_cache_tls);

    // Precompute bitmask of IR-only local slots.  After function calls (without
    // reference args), only non-IR-only slots need cache invalidation because callees
    // can access non-IR-only locals through the TLS variable stack (Qore's scoping
    // allows inner functions to access outer locals).  IR-only locals exist only in
    // the slot cache and cannot be accessed through TLS, so they stay valid.
    bool has_non_ir_only_locals = func.interpreter_has_non_ir_only_locals;
    if (!func.interpreter_locals_ir_only.empty()) {
        assert(func.interpreter_locals_ir_only.size() <= locals_ir_only.size());
        std::copy(func.interpreter_locals_ir_only.begin(), func.interpreter_locals_ir_only.end(),
            locals_ir_only.begin());
    }
    // Direct params: pre-populate slot cache from caller-provided values,
    // bypassing TLS instantiate/eval/uninstantiate round-trip entirely.
    auto cleanupDirectParamSlots = [&](int last_arg) {
        for (int j = last_arg; j >= 0; --j) {
            if (j >= static_cast<int>(func.interpreter_param_slot_ids.size())) {
                continue;
            }
            uint32_t cleanup_sid = func.interpreter_param_slot_ids[static_cast<size_t>(j)];
            if (cleanup_sid < local_slot_count && locals_instantiated[cleanup_sid]) {
                locals_slot_cache[cleanup_sid].discard(xsink);
                locals_slot_cache[cleanup_sid] = QoreValue();
                locals_instantiated[cleanup_sid] = false;
            }
        }
    };
    if (direct_params && direct_params->nargs > 0) {
        for (int i = 0; i < direct_params->nargs; ++i) {
            // Find the slot_id for this param's LocalVar
            if (i < static_cast<int>(func.interpreter_param_slot_ids.size())) {
                uint32_t sid = func.interpreter_param_slot_ids[static_cast<size_t>(i)];
                if (sid < local_slot_count) {
                    QoreValue val = fromBits(direct_params->args[i]);
                    if (val.hasNode()) {
                        val.refSelf();
                    }

                    // Apply type filter like JIT path: match instantiateFastCallParams in JITRuntime.cpp
                    const LocalVar* lv = i < static_cast<int>(func.interpreter_param_local_vars.size())
                        ? func.interpreter_param_local_vars[static_cast<size_t>(i)]
                        : nullptr;
                    if (lv) {
                        const QoreTypeInfo* paramTypeInfo = lv->getTypeInfo();
                        if (!exactPrimitiveTypeAcceptsValue(paramTypeInfo, val)
                                && QoreTypeInfo::mayRequireFilter(paramTypeInfo, val)) {
                            QoreTypeInfo::acceptInputParam(paramTypeInfo, i, lv->getName(), val, xsink);
                            if (*xsink) {
                                val.discard(xsink);
                                // Error in type filtering - cleanup already-instantiated locals
                                cleanupDirectParamSlots(i - 1);
                                return false;
                            }
                        }
                    }

                    locals_slot_cache[sid] = val;
                    locals_instantiated[sid] = true;
                    if (sid < locals_ir_only.size()) {
                        // Direct params intentionally bypass the runtime local
                        // stack.  Preserve their slot-cache values across
                        // generic call/scope invalidation; there is no TLS
                        // value to reload on a later LoadLocal miss.
                        locals_ir_only[sid] = true;
                    }
                }
            }
        }
        // The direct-param slot cache now owns refs for parameter args, and
        // argv (created by the caller before execute()) owns refs for varargs.
        // Drop caller temp cleanup refs before running the callee body.
        if (direct_params->arg_cleanups) {
            qore_rt_clear_arg_cleanups(direct_params->arg_cleanups,
                    direct_params->nargs, xsink);
            if (*xsink) {
                cleanupDirectParamSlots(direct_params->nargs - 1);
                return false;
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
    auto preserveParentSlotForWriteback = [&](size_t slot_id) {
        // Dirty inherited parent slots must survive cache invalidation until
        // ParentSlotWriteback publishes them to the enclosing frame.
        return slot_id < parent_slot_dirty.size() && parent_slot_dirty[slot_id];
    };

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
                            executeHandlerBody(on_block_exit_handlers[i - 1], &obe_xsink, &locals_slot_cache);
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
                    if (preserveParentSlotForWriteback(i)) {
                        continue;
                    }
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
    auto cleanupLocalCaches = [&](bool preserve_ir_only = true) {
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
        // Clear local slot cache, preserving IR-only locals (loop counters etc.)
        // that only exist in the cache and have no variable-stack backing.
        for (size_t i = 0; i < locals_slot_cache.size(); ++i) {
            if (preserveParentSlotForWriteback(i)) {
                continue;
            }
            if (preserve_ir_only && i < locals_ir_only.size() && locals_ir_only[i]) {
                continue;  // Preserve IR-only locals
            }
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
                if (preserveParentSlotForWriteback(i)) {
                    continue;
                }
                if (!locals_ir_only[i]) {
                    locals_slot_cache[i].discard(xsink);
                    locals_slot_cache[i] = QoreValue();
                }
            }
        }
    };

    // Helper: discard all LoadLocal result slots for a given local variable,
    // clear the tracking vector, and reset the registration bitmap entries.
    // CRITICAL: Don't discard load slots that are still in the cleanup vector.
    // Return values are in cleanup and shouldn't be discarded by StoreLocal/operators.
    auto clearLoadSlots = [&](uint32_t slot_id) {
        if (slot_id >= local_load_slots.size() || local_load_slots[slot_id].empty()) {
            return;
        }
        for (uint32_t vid : local_load_slots[slot_id]) {
            // Skip if this value is still in cleanup (e.g., return value)
            bool in_cleanup = std::find(cleanup.begin(), cleanup.end(), vid) != cleanup.end();
            if (in_cleanup) {
                continue;  // Don't discard values that are in cleanup
            }
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
    auto isClosureContainer = [](const LocalVar* lv, const VarRefNode* container) {
        if (lv) {
            return lv->closureUse();
        }
        return container
            && (container->getType() == VT_CLOSURE
                || (container->getType() == VT_LOCAL && container->ref.id
                    && container->ref.id->closureUse()));
    };
    auto discardContainerValueSlot = [&](uint32_t vid, uint32_t container_slot_id, bool owns_slot_ref) {
        if (vid >= values.size()) {
            return;
        }
        if (container_slot_id != UINT32_MAX
                && container_slot_id < local_init_slots.size()
                && local_init_slots[container_slot_id] == vid) {
            removeCleanupEntry(cleanup, vid);
            values[vid] = QoreValue();
            return;
        }
        if (owns_slot_ref && removeCleanupEntry(cleanup, vid)) {
            values[vid].discard(xsink);
            // CRITICAL: clear the slot bits after discard.  The discard drops
            // the slot's +1 ref, and if that was the last reference the node
            // is freed.  Leaving the stale pointer in values[vid] turns any
            // later read (e.g., Return -> values[ret->value.id].discard)
            // into a use-after-free crash dereferencing freed memory.
            values[vid] = QoreValue();
        } else {
            removeCleanupEntry(cleanup, vid);
            values[vid] = QoreValue();
        }
        if (vid < load_slot_registered.size()) {
            load_slot_registered[vid] = false;
        }
    };

    struct LocalInstantiationCleanup {
        std::vector<const LocalVar*>& locals_ordered;
        ExceptionSink* xsink;
        const std::unordered_set<const LocalVar*>* pre_instantiated;
        ~LocalInstantiationCleanup() {
            cleanupInstantiatedLocals(locals_ordered, xsink, pre_instantiated);
        }
    } local_cleanup{instantiated_locals_ordered, xsink, pre_instantiated};

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

    // Declared after SlotCacheCleanup so dirty parent slots are published before
    // this frame releases its cached local references.
    ParentSlotWriteback parent_slot_writeback(func.parent_slot_count, locals_slot_cache, parent_slot_cache,
        parent_slot_to_lvar, parent_slot_dirty, parent_slot_cache_dirty, xsink);

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

    // Locals owned by this function.  Used by ensureLocalInstantiated() to
    // distinguish body locals from outer-scope variables (e.g. enclosing
    // function params referenced from handler bodies).  Body locals are always
    // function-owned even when a deserialized/cached IR producer did not include
    // them in pre_instantiated_locals.
    std::unordered_set<const void*> function_own_locals_storage(func.pre_instantiated_locals);
    for (LocalVar* lv : func.all_body_locals) {
        function_own_locals_storage.insert(reinterpret_cast<const void*>(lv));
    }
    const std::unordered_set<const void*>* function_own_locals = &function_own_locals_storage;

    // Helper: invalidate the closure variable cache for a given VarRefNode.
    // LoadClosure caches values in the closures map; lvalue operations modify the
    // cvstack directly, so the cache becomes stale.  Must be called before AND after
    // any lvalue operation that could modify a closure-use variable.
    auto invalidateClosureCache = [&](const VarRefNode* vrn) {
        if (vrn && vrn->ref.id) {
            qore_var_t vtype = vrn->getType();
            if (vtype == VT_CLOSURE || (vtype == VT_LOCAL && vrn->ref.id->closureUse())) {
                auto cit = closures.find(vrn->ref.id);
                if (cit != closures.end()) {
                    cit->second.discard(xsink);
                    closures.erase(cit);
                }
            }
        }
    };
    // Same as invalidateClosureCache, but keyed on a LocalVar* directly.  Used by
    // opcodes that carry an explicit LocalVar* (HashKeyStore/HashKeyStoreDynamic/
    // ListIndexStore) — they may have been AOT-deserialized with container=nullptr
    // and only container_lv populated, so the VarRefNode-based helper cannot run.
    auto invalidateClosureCacheLv = [&](const LocalVar* lv) {
        if (lv && lv->closureUse()) {
            auto cit = closures.find(lv);
            if (cit != closures.end()) {
                cit->second.discard(xsink);
                closures.erase(cit);
            }
        }
    };
    auto updateClosureCacheInt = [&](const LocalVar* lv, int64_t value) {
        if (lv && lv->closureUse()) {
            auto cit = closures.find(lv);
            if (cit != closures.end()) {
                cit->second.discard(xsink);
                cit->second = QoreValue(value);
            }
        }
    };
    auto invalidateLValuePathClosureCache = [&](const QoreIRLValuePathInstruction* path_inst) {
        if (!path_inst || path_inst->path.empty()) {
            return;
        }
        const LVPathStep& root = path_inst->path[0];
        if ((root.kind == LVPathStepKind::LocalVar || root.kind == LVPathStepKind::ClosureVar)
                && root.ref_ptr) {
            invalidateClosureCacheLv(static_cast<const LocalVar*>(root.ref_ptr));
        }
    };
    auto markParentLValueDirty = [&](const QoreIRLValueInstruction* lval_inst,
            const VarRefNode* lval_vrn) {
        if (parent_slot_dirty.empty() || !lval_inst) {
            return;
        }
        if (lval_vrn && lval_vrn->getTypeInfo()
                && QoreTypeInfo::isReference(lval_vrn->getTypeInfo())) {
            return;
        }
        uint32_t sid = lval_inst->lvalue_slot_id;
        if (!lval_inst->hasLocalTarget() && lval_vrn
                && (lval_vrn->getType() == VT_LOCAL || lval_vrn->getType() == VT_LOCAL_TS)
                && lval_vrn->ref.id) {
            auto it = func.local_var_slots.find(lval_vrn->ref.id);
            if (it != func.local_var_slots.end()) {
                sid = it->second;
            }
        }
        markParentSlotDirty(sid);
    };
    auto markParentLValuePathDirty = [&](const QoreIRLValuePathInstruction* path_inst) {
        if (parent_slot_dirty.empty() || !path_inst || !path_inst->hasLocalTarget()
                || path_inst->path.empty()) {
            return;
        }
        markParentSlotDirty(path_inst->lvalue_slot_id);
    };

    // Helper: make a private copy of an LValuePath instruction's path with
    // dynamic operands resolved for the current invocation.  MUST be used by
    // every LValuePath{Assign,Compound,Unary,BinaryMut,Ternary} handler instead
    // of mutating path_inst->path directly — the instruction is SHARED across
    // concurrent invocations of the same IR function, and per-call fields like
    // `step.name` (dynamic hash key), `step.slot_id` (dynamic list index), and
    // `step.slice_values` (multi-key/index slice) race between threads when
    // written in place.  Mirrors the JIT path in `patchLVPath` (JITRuntime.cpp)
    // which also copies into a local `path_copy`.  Without this, a thread
    // reading `step.name.c_str()` in navigatePath can see a stomped string
    // from another thread still patching.
    auto patchLVPathLocal = [&](const QoreIRLValuePathInstruction* path_inst)
            -> std::vector<LVPathStep> {
        std::vector<LVPathStep> path_copy = path_inst->path;
        for (auto& step : path_copy) {
            if (step.kind == LVPathStepKind::HashKey && step.operand_idx != UINT32_MAX) {
                QoreValue key_val = getIRValue(values, QoreIRValue(step.operand_idx));
                step.slice_values.clear();
                step.slice_values.push_back(key_val);
                QoreStringValueHelper key_str(key_val);
                step.name = key_str->c_str();
            } else if (step.kind == LVPathStepKind::ListIndex && step.operand_idx != UINT32_MAX) {
                QoreValue idx_val = getIRValue(values, QoreIRValue(step.operand_idx));
                step.index = idx_val.getAsBigInt();
            } else if (step.kind == LVPathStepKind::HashKeySlice
                    || step.kind == LVPathStepKind::ListIndexSlice
                    || step.kind == LVPathStepKind::ListRangeSlice) {
                step.slice_values.clear();
                step.slice_values.reserve(step.slice_operand_ids.size());
                for (uint32_t sid : step.slice_operand_ids) {
                    step.slice_values.push_back(getIRValue(values, QoreIRValue(sid)));
                }
            }
        }
        return path_copy;
    };

    // LValuePath roots are resolved through LValueHelper, so owned local roots
    // need the same stack instantiation invariant as StoreLocal/StoreLValue.
    auto ensureLValuePathRootLocal = [&](const QoreIRLValuePathInstruction* path_inst) {
        if (!path_inst || path_inst->path.empty()) {
            return;
        }
        const LVPathStep& root = path_inst->path[0];
        if ((root.kind != LVPathStepKind::LocalVar && root.kind != LVPathStepKind::ClosureVar)
                || !root.ref_ptr) {
            return;
        }
        auto* lv = const_cast<LocalVar*>(static_cast<const LocalVar*>(root.ref_ptr));
        ensureLocalInstantiated(lv, instantiated_locals, instantiated_locals_ordered,
            pre_instantiated, function_own_locals, &locally_uninstantiated);
        if (path_inst->lvalue_slot_id < locals_instantiated.size()) {
            locals_instantiated[path_inst->lvalue_slot_id] = true;
        }
    };

    // Helper: prepare slot cache before a lvalue operation.
    // Extracts the base VarRefNode, calls ensureLocalInstantiated, and pre-invalidates
    // the slot cache entry and closure cache. Returns the base VarRefNode.
    auto prepareLValueSlotCache = [&](const QoreIRLValueInstruction* lval_inst) -> const VarRefNode* {
        const VarRefNode* lval_vrn = extractLValueBaseVarRef(lval_inst->lvalue);
        if (lval_vrn && lval_vrn->ref.id) {
            qore_var_t vtype = lval_vrn->getType();
            if (vtype == VT_LOCAL || vtype == VT_LOCAL_TS || vtype == VT_CLOSURE) {
                ensureLocalInstantiated(lval_vrn->ref.id, instantiated_locals,
                    instantiated_locals_ordered, pre_instantiated,
                    function_own_locals, &locally_uninstantiated);
            }
            invalidateClosureCache(lval_vrn);
        }
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
        return lval_vrn;
    };

    auto prepareParseReferenceLocal = [&](const ParseReferenceNode* prn) {
        if (!prn) {
            return;
        }
        const VarRefNode* vrn = extractLValueBaseVarRef(prn->getLVExp());
        if (!vrn || !vrn->ref.id) {
            return;
        }
        qore_var_t vtype = vrn->getType();
        if (vtype != VT_LOCAL && vtype != VT_LOCAL_TS && vtype != VT_CLOSURE) {
            return;
        }
        ensureLocalInstantiated(vrn->ref.id, instantiated_locals,
            instantiated_locals_ordered, pre_instantiated,
            function_own_locals, &locally_uninstantiated);

        if ((vtype == VT_LOCAL_TS || vtype == VT_CLOSURE || vrn->ref.id->closureUse())
                && !thread_try_find_closure_var(vrn->ref.id->getName())
                && !thread_try_get_runtime_closure_var(vrn->ref.id)) {
            ensureLocalInstantiated(vrn->ref.id, instantiated_locals,
                instantiated_locals_ordered, pre_instantiated, nullptr,
                &locally_uninstantiated);
        }
    };

    // Helper: finalize slot cache after a lvalue operation.
    // For simple variable lvalues (VarRefNode directly): repopulate with result value.
    // For complex lvalues (member/index access): invalidate (set to NOTHING) so next
    // LoadLocal re-reads from TLS.  This distinction is critical: for h.value += 3,
    // res is the member's new value (8), not the container (h) — repopulating h's slot
    // with 8 would corrupt the cache.
    // Also invalidates the closures cache for closure-use variables.
    auto finalizeLValueSlotCache = [&](const QoreIRLValueInstruction* lval_inst,
            const VarRefNode* lval_vrn, const QoreValue& res, bool repopulate) {
        invalidateClosureCache(lval_vrn);
        // Handle local variable slot cache
        uint32_t sid = lval_inst->lvalue_slot_id;
        if (!lval_inst->hasLocalTarget() && lval_vrn
                && (lval_vrn->getType() == VT_LOCAL || lval_vrn->getType() == VT_LOCAL_TS)
                && lval_vrn->ref.id) {
            auto it = func.local_var_slots.find(lval_vrn->ref.id);
            if (it != func.local_var_slots.end()) {
                sid = it->second;
            }
        }
        if (sid < locals_slot_cache.size()) {
            locals_slot_cache[sid].discard(xsink);
            if (repopulate) {
                locals_slot_cache[sid] = res.hasNode() ? res.refSelf() : res;
            } else {
                locals_slot_cache[sid] = QoreValue();
            }
            clearLoadSlots(sid);
            markParentLValueDirty(lval_inst, lval_vrn);
        }
    };

    // RAII guard: fire remaining on_block_exit handlers on ANY function exit path.
    // Normal exits fire handlers via ScopeExit instructions (which clear scope_stack
    // and on_block_exit_handlers). Error exits (exception from Call/ExprOp/etc.)
    // skip ScopeExit and return false directly, leaving handlers unfired.
    // This guard catches those cases by calling fireScopeExits(0) before return,
    // ensuring on_exit/on_error/on_success handlers always fire.
    // Constructed AFTER local_cleanup so it's destroyed BEFORE it — handlers fire
    // while runtime locals are still valid on the thread-local variable stack.
    // Only fires handlers if scope_stack is not empty (handlers weren't already fired
    // via normal ScopeExit instructions).
    struct ScopeExitGuard {
        decltype(fireScopeExits)& fire;
        std::vector<size_t>& scope_stack;
        ~ScopeExitGuard() {
            // Only fire remaining handlers if scope_stack is not empty (handlers weren't already
            // fired via normal ScopeExit instructions during function execution).
            if (!scope_stack.empty()) {
                fire(0);
            }
        }
    } scope_exit_guard{fireScopeExits, scope_stack};

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
    bool debug_break_loop = false;  // set by dbgStep RC_BREAK to exit current loop (legacy fallback)
    // Set by dbgStep RC_BREAK to the exit block of the loop enclosing the break point.
    // Execution then continues normally (running statement/scope cleanups) until the
    // enclosing loop's condition BrIf — the unique branch whose false_target equals this
    // block — is reached and diverted to it.  Resolving the loop this way (instead of
    // "force the next BrIf false") avoids an inner if/switch BrIf wrongly consuming the
    // break, which left the loop running.  See QoreIRBasicBlock::enclosing_loop_exit.
    QoreIRBasicBlock* debug_break_target = nullptr;

    auto returnAfterUnhandledException = [&](bool values_cleaned = false) -> bool {
        if (getenv("QORE_IR_TRACE_EXCEPTIONS")) {
            fprintf(stderr, "[ir-exception] func='%s' return-unhandled xsink=%d\n",
                func.name.c_str(), xsink && *xsink ? 1 : 0);
            fflush(stderr);
        }
        if (debug_active) {
            tlpd->dbgFunctionExit(statements, return_value, xsink);
        }
        fireScopeExits();
        if (!values_cleaned) {
            cleanupValues(values, cleanup, xsink, true, cleanup_log);
        }
        cleanupLocalCaches(false);
        if (!xsink || !*xsink) {
            return_value = QoreValue();
            return true;
        }
        return false;
    };

    auto getDebugStatement = [&](const QoreIRInstruction* dbg_inst) -> AbstractStatement* {
        if (!pgm || !dbg_inst || !dbg_inst->loc || dbg_inst->cached_start_line < 0) {
            return nullptr;
        }
        return qore_program_private::get(*pgm)->getStatementFromIndex(
            dbg_inst->loc->getFile(), dbg_inst->cached_start_line + dbg_inst->loc->offset);
    };

    auto findDismissTarget = [](QoreIRBasicBlock* start, QoreIRBasicBlock*& target_block,
            size_t& target_ip) -> bool {
        if (!start) {
            return false;
        }
        std::vector<QoreIRBasicBlock*> stack{start};
        std::unordered_set<QoreIRBasicBlock*> seen;
        while (!stack.empty() && seen.size() < 64) {
            QoreIRBasicBlock* b = stack.back();
            stack.pop_back();
            if (!b || !seen.insert(b).second) {
                continue;
            }
            for (size_t i = 0; i < b->instructions.size(); ++i) {
                QoreIRInstruction* candidate = b->instructions[i].get();
                if (candidate->opcode == QoreIROpcode::DiscardTemps) {
                    target_block = b;
                    target_ip = i;
                    return true;
                }
                if (candidate->opcode == QoreIROpcode::PushTempMark) {
                    break;
                }
                if (auto* br = dynamic_cast<QoreIRBranchInstruction*>(candidate)) {
                    stack.push_back(br->target);
                    break;
                }
                if (auto* br_if = dynamic_cast<QoreIRBranchIfInstruction*>(candidate)) {
                    stack.push_back(br_if->false_target);
                    stack.push_back(br_if->true_target);
                    break;
                }
                if (auto* inv = dynamic_cast<QoreIRInvokeInstruction*>(candidate)) {
                    stack.push_back(inv->normal_target);
                    break;
                }
                if (auto* inv_md = dynamic_cast<QoreIRInvokeMethodDirectInstruction*>(candidate)) {
                    stack.push_back(inv_md->normal_target);
                    break;
                }
                if (auto* inv_dot = dynamic_cast<QoreIRInvokeDotEvalMethodDirectInstruction*>(candidate)) {
                    stack.push_back(inv_dot->normal_target);
                    break;
                }
            }
        }
        return false;
    };

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
    const bool trace_ir_refs = getenv("QORE_IR_TRACE_REFS") != nullptr;
    const bool trace_ir_refs_deep = getenv("QORE_IR_TRACE_REFS_DEEP") != nullptr;
    // Deep ref tracing intentionally dereferences object internals; leave it
    // off unless narrowing an IR lifetime/refcount bug under a debugger.
    auto traceIRRef = [&](const QoreIRInstruction* trace_inst, const char* event, uint32_t slot, QoreValue val,
            const char* action = nullptr) {
        if (!trace_ir_refs) {
            return;
        }
        const uint64_t raw = val.rawBits();
        const AbstractQoreNode* node = val.hasNode() ? val.getInternalNode() : nullptr;
        fprintf(stderr, "[ir-ref] func='%s' block='%s' ip=%zu opcode=%s(%d) event=%s slot=%u raw=0x%016llx "
            "node=%p has_node=%d",
            func.name.c_str(), block ? block->name.c_str() : "<none>", ip,
            trace_inst ? getOpcodeName(static_cast<int>(trace_inst->opcode)) : "<none>",
            trace_inst ? static_cast<int>(trace_inst->opcode) : -1, event, slot,
            static_cast<unsigned long long>(raw), static_cast<const void*>(node), node ? 1 : 0);
        if (action) {
            fprintf(stderr, " action=%s", action);
        }
        if (trace_ir_refs_deep && node) {
            qore_type_t type = node->getType();
            fprintf(stderr, " type=%d", static_cast<int>(type));
            if (type == NT_OBJECT) {
                auto* priv = qore_object_private::get(*const_cast<QoreObject*>(
                    static_cast<const QoreObject*>(node)));
                fprintf(stderr, " refs=%d rrefs=%d rcount=%d rset=%p deferred=%d recursive_found=%d status=%d",
                    priv->references.load(), priv->rrefs.load(), priv->rcount, static_cast<void*>(priv->rset),
                    priv->deferred_scan ? 1 : 0, priv->recursive_ref_found ? 1 : 0, priv->status);
            }
        }
        fputc('\n', stderr);
        fflush(stderr);
    };

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

    std::unordered_map<uint32_t, int32_t> weak_load_temp_slots;

    auto releaseWeakLoadTemp = [&](uint32_t id) {
        if (id >= values.size()) {
            return;
        }
        removeAllCleanupEntries(cleanup, id);
        values[id] = QoreValue();
        weak_load_temp_slots.erase(id);
    };

    auto clearSlotForOverwrite = [&](uint32_t id) {
        if (id >= values.size()) {
            return;
        }
        bool had_borrowed_value = weak_load_temp_slots.erase(id) > 0;
        bool had_cleanup = removeAllCleanupEntries(cleanup, id);
        if (had_cleanup && !had_borrowed_value) {
            values[id].discard(xsink);
        }
        values[id] = QoreValue();
    };

    auto trackWeakLoadTemp = [&](uint32_t id) {
        if (id >= func.interpreter_operand_use_counts.size()) {
            return;
        }
        int32_t remaining_uses = func.interpreter_operand_use_counts[id];
        if (remaining_uses > 0) {
            weak_load_temp_slots[id] = remaining_uses;
        }
    };

    auto consumeOperandUse = [&](uint32_t id) {
        auto it = weak_load_temp_slots.find(id);
        if (it == weak_load_temp_slots.end()) {
            return;
        }
        if (--it->second <= 0) {
            releaseWeakLoadTemp(id);
        }
    };

    const std::vector<uint8_t>& return_protected_slots = func.interpreter_return_protected_slots;
    const std::vector<uint32_t>& return_value_slot_ids = func.interpreter_return_value_slot_ids;
    const std::vector<uint32_t>& return_preserve_slot_ids = func.interpreter_return_preserve_slot_ids;
    const std::vector<int32_t>& operand_use_counts = func.interpreter_operand_use_counts;

    auto valueUsedLaterInCurrentBlock = [&block, &ip, xsink, &return_protected_slots](uint32_t id) -> bool {
        // A slot that is the operand of any Return in the function must stay
        // alive across scope exits regardless of basic-block locality.
        if (id < return_protected_slots.size() && return_protected_slots[id]) {
            return true;
        }
        if (!block) {
            return false;
        }
        size_t scan_count = 0;
        for (size_t i = ip + 1; i < block->instructions.size(); ++i) {
            if (((++scan_count % 100) == 0) && qore_check_cancel(xsink, "IR cleanup use scan")) {
                return true;
            }
            const QoreIRInstruction* future = block->instructions[i].get();
            if (!future) {
                continue;
            }
            if (future->opcode == QoreIROpcode::Return) {
                const auto* ret = static_cast<const QoreIRReturnInstruction*>(future);
                if (ret->has_value && ret->value.id == id) {
                    return true;
                }
            }
            for (QoreIRValue op : future->operands) {
                if (op.id == id) {
                    return true;
                }
            }
            if (isTerminator(future->opcode)) {
                break;
            }
        }
        return false;
    };

    auto valueNodeEscapesAsReturn = [&values, &return_value_slot_ids,
            &return_preserve_slot_ids](const AbstractQoreNode* node) -> bool {
        if (!node) {
            return false;
        }
        for (uint32_t id : return_value_slot_ids) {
            if (id < values.size() && values[id].hasNode() && values[id].getInternalNode() == node) {
                return true;
            }
        }
        for (uint32_t id : return_preserve_slot_ids) {
            if (id < values.size() && values[id].hasNode() && values[id].getInternalNode() == node) {
                return true;
            }
        }
        return false;
    };

    auto resultValueIsNeeded = [&operand_use_counts, &return_protected_slots](QoreIRValue result) -> bool {
        if (!result.isValid()) {
            return false;
        }
        return (result.id < operand_use_counts.size() && operand_use_counts[result.id] > 0)
            || (result.id < return_protected_slots.size() && return_protected_slots[result.id]);
    };

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
        // Track the last instruction dispatched so silent-failure tracing
        // (QORE_IR_TRACE_SILENT_FAIL=1) can identify which opcode returned
        // `ok=false` without raising an xsink exception.  Thread-local so
        // the caller (StatementBlock::execImpl / evalTiered fallback) can
        // read it post-return without plumbing it through the return value.
        tls_last_silent_fail_opcode = inst->opcode;
        tls_last_silent_fail_loc = inst->loc;
        // Unified per-line overhead: only update runtime_loc, check ephemeral refs,
        // and fire debug events when the source line changes.  Multiple IR instructions
        // on the same source line share the same location, so we only need one check
        // per line transition.  Same-line instructions skip the update entirely —
        // the runtime_loc was set on line entry and remains valid (same line number).
        // Uses cached ThreadData pointers (rl_cache) to avoid repeated TLS lookups
        // (pthread_getspecific + std::map::find per call).
        // Uses cached_start_line (-1 = no loc, >=0 = loc->start_line) to avoid
        // pointer dereferences on the hot path.
        // Debug: check for debugger attach/detach on every instruction (not gated
        // by line change). runtimeCheck() is very cheap (2 booleans + 1 enum, no locks)
        // and only runs when can_debug is true (program has PO_ALLOW_DEBUGGER).
        // This ensures breakProgramThread() signals are detected even in tight loops
        // where all instructions share the same source line.
        if (can_debug) {
            bool runtime_check = tlpd->runtimeCheck();
            if (!debug_active && runtime_check) {
                debug_active = true;
                if (tlpd->hasBreakFlag()) {
                    last_debug_line = -1;
                } else if (inst->cached_start_line >= 0 && inst->cached_start_line == last_ephemeral_line) {
                    last_debug_line = inst->cached_start_line;
                }
            } else if (debug_active && !runtime_check) {
                debug_active = false;
            }
        }
        if (inst->cached_start_line >= 0 && inst->cached_start_line != last_ephemeral_line) {
            // Update runtime_loc for exception/callstack reporting via cached pointers
            *rl_cache.stmt_ptr = nullptr;
            *rl_cache.loc_ptr = inst->loc;
            last_ephemeral_line = inst->cached_start_line;
            if (!debug_active) {
                last_debug_line = inst->cached_start_line;
            }
        }
        bool debug_statement_boundary = inst->opcode == QoreIROpcode::PushTempMark
            && block->name.find(".cond") == std::string::npos;
        if (debug_active && debug_statement_boundary && inst->cached_start_line >= 0
                && inst->cached_start_line != last_debug_line && !(xsink && *xsink)) {
            last_debug_line = inst->cached_start_line;
            AbstractStatement* dbg_stmt = getDebugStatement(inst);
            if (getenv("QORE_IR_TRACE_DEBUG")) {
                fprintf(stderr, "[ir-debug] func='%s' line=%d stmt=%p op=%d debug_active=%d\n",
                    func.name.c_str(), inst->cached_start_line + inst->loc->offset,
                    static_cast<void*>(dbg_stmt), static_cast<int>(inst->opcode), debug_active ? 1 : 0);
                fflush(stderr);
            }
            if (dbg_stmt) {
                int dbg_rc = tlpd->dbgStep(statements, dbg_stmt, xsink);
                if (getenv("QORE_IR_TRACE_DEBUG")) {
                    fprintf(stderr, "[ir-debug] func='%s' line=%d dbg_rc=%d xsink=%d\n",
                        func.name.c_str(), inst->cached_start_line + inst->loc->offset, dbg_rc,
                        xsink && *xsink ? 1 : 0);
                    fflush(stderr);
                }
                if (dbg_rc || *xsink) {
                    if (dbg_rc == RC_RETURN || *xsink) {
                        if (debug_active) {
                            tlpd->dbgFunctionExit(statements, return_value, xsink);
                        }
                        executeOnBlockExitHandlers(on_block_exit_handlers, xsink, &locals_slot_cache);
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return dbg_rc == RC_RETURN;
                    }
                    if (dbg_rc == RC_BREAK) {
                        // Debug break: exit the loop enclosing this statement, matching
                        // AST behavior where RC_BREAK unwinds to the innermost while/for
                        // loop.  Resolve the loop via the block's enclosing_loop_exit so
                        // the correct loop's condition BrIf is the one diverted (an inner
                        // if/switch BrIf must not consume the break).  Fall back to the
                        // legacy "force next BrIf false" only when no loop is annotated.
                        if (current_block->enclosing_loop_exit) {
                            debug_break_target = current_block->enclosing_loop_exit;
                        } else {
                            debug_break_loop = true;
                        }
                    }
                }
            }
        }

        // Verbose IR lowering diagnostics: enable with QORE_IR_DEBUG env var
        // Zero-cost when disabled: single static bool check only initialized once
        static bool ir_verbose_enabled = (getenv("QORE_IR_DEBUG") != nullptr);

        // Log instruction execution for diagnostics (zero-cost path when disabled)
        if (ir_verbose_enabled && func.name.find("monitor") != std::string::npos) {
            fprintf(stderr, "[IR-INSTR-PRE-%zu] func='%s' op=%d(%s) ", ip, func.name.c_str(),
                    (int)inst->opcode, inst->opcode == QoreIROpcode::GuardNotNothing ? "GuardNotNothing" : "");
            fprintf(stderr, "result_slot=%d\n", inst->result.id);
            fflush(stderr);
        }

        auto raiseMissingAOTExpr = [&](const char* what, const QoreValue& expr) {
            const QoreProgramLocation* loc = inst->loc;
            int line = loc ? loc->start_line + loc->offset : -1;
            xsink->raiseException("IR-EXEC-ERROR",
                "%s expression not resolved in AOT IR: function='%s' block='%s' ip=%zu opcode=%s(%d) "
                "result_slot=%d expr_type=%s source=%s:%d",
                what, func.name.c_str(), block->name.c_str(), ip,
                getOpcodeName(static_cast<int>(inst->opcode)), static_cast<int>(inst->opcode),
                inst->result.id, expr.getTypeName(), loc ? loc->getFileValue() : "", line);
        };

        switch (inst->opcode) {
            case QoreIROpcode::ConstInt: {
                auto* cinst = static_cast<QoreIRConstInstruction*>(inst);
                // Large ints (outside 48-bit inline range) are boxed as QoreBigIntNode
                // via setLargeInt() — those allocations must be tracked in cleanup.
                QoreValue v(cinst->constant.int_value);
                setOwnedValueSlot(values, cleanup, cinst->result.id, v, xsink);
                ++ip;
                break;
            }
            case QoreIROpcode::ConstFloat: {
                auto* cinst = static_cast<QoreIRConstInstruction*>(inst);
                // Problematic doubles (negative NaN, etc.) are boxed as QoreBigFloatNode
                // via setLargeFloat() — those allocations must be tracked in cleanup.
                QoreValue v(cinst->constant.float_value);
                setOwnedValueSlot(values, cleanup, cinst->result.id, v, xsink);
                ++ip;
                break;
            }
            case QoreIROpcode::ConstBool: {
                auto* cinst = static_cast<QoreIRConstInstruction*>(inst);
                setValueSlotDirect(values, cinst->result.id, QoreValue(cinst->constant.bool_value));
                ++ip;
                break;
            }
            case QoreIROpcode::ConstChar: {
                auto* cinst = static_cast<QoreIRConstInstruction*>(inst);
                setValueSlotDirect(values, cinst->result.id, QoreValue::makeChar(cinst->constant.char_value));
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
                setOwnedValueSlot(values, cleanup, cinst->result.id, QoreValue(str), xsink);
                ++ip;
                break;
            }
            case QoreIROpcode::ConstDate: {
                auto* cinst = static_cast<QoreIRConstInstruction*>(inst);
                DateTimeNode* dt;
                if (cinst->constant.date_is_relative) {
                    // Preserve the literal/component fields exactly.  ISO
                    // relative-date literals may intentionally contain values
                    // such as PT140S; AST mode preserves second=140, so IR
                    // must not carry that into minute=2, second=20.
                    dt = DateTimeNode::makeRelativeUnnormalized(
                        cinst->constant.rel_years, cinst->constant.rel_months,
                        cinst->constant.rel_days, cinst->constant.rel_hours,
                        cinst->constant.rel_minutes, cinst->constant.rel_seconds,
                        cinst->constant.rel_us);
                } else {
                    // date_microseconds is a UTC epoch from getEpochMicrosecondsUTC(); use
                    // makeAbsolute() which stores the epoch directly without local-to-UTC
                    // conversion (unlike DateTimeNode(s, ms) which goes through setLocalDate)
                    int64_t epoch_seconds = cinst->constant.date_microseconds / 1000000;
                    int us = static_cast<int>(cinst->constant.date_microseconds % 1000000);
                    // Use the stored timezone when available. nullptr means UTC.
                    // Fall back to currentTZ() only for legacy IR without date_zone.
                    const AbstractQoreZoneInfo* zone = cinst->constant.date_zone_set
                        ? cinst->constant.date_zone : currentTZ();
                    dt = DateTimeNode::makeAbsolute(zone, epoch_seconds, us);
                }
                setOwnedValueSlot(values, cleanup, cinst->result.id, QoreValue(dt), xsink);
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
                const QoreTypeInfo* typeInfo = substituteRuntimeTypeParams(ml->typeInfo);
                const QoreTypeInfo* declared_vtype = QoreTypeInfo::getUniqueReturnComplexList(typeInfo);
                if (!declared_vtype) {
                    declared_vtype = QoreTypeInfo::getReturnComplexListOrNothing(typeInfo);
                }
                if (declared_vtype == anyTypeInfo) {
                    declared_vtype = nullptr;
                }
                bool declared_type = declared_vtype && declared_vtype != autoTypeInfo;
                ReferenceHolder<QoreListNode> list(
                    new QoreListNode(declared_type ? declared_vtype : autoTypeInfo), xsink);
                const QoreTypeInfo* vtype = nullptr;
                bool vcommon = false;
                for (size_t i = 0; i < inst->operands.size(); ++i) {
                    if (i && !(i % 100) && qore_check_cancel(xsink, "IR list literal construction")) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    const auto& operand = inst->operands[i];
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
                if (declared_type) {
                    qore_list_private::get(*list)->complexTypeInfo = qore_get_complex_list_type(declared_vtype);
                } else {
                    if (!vtype || vtype == anyTypeInfo || !vcommon) {
                        vtype = autoTypeInfo;
                    }
                    qore_list_private::get(*list)->complexTypeInfo = qore_get_complex_list_type(vtype);
                }
                QoreListNode* raw_list = list.release();
                setOwnedValueSlot(values, cleanup, inst->result.id, QoreValue(raw_list), xsink);
                ++ip;
                break;
            }
            case QoreIROpcode::MakeHash: {
                const auto* mh = static_cast<const QoreIRMakeHashInstruction*>(inst);
                const QoreTypeInfo* typeInfo = substituteRuntimeTypeParams(mh->typeInfo);
                const QoreTypeInfo* declared_vtype = QoreTypeInfo::getUniqueReturnComplexHash(typeInfo);
                if (!declared_vtype) {
                    declared_vtype = QoreTypeInfo::getReturnComplexHashOrNothing(typeInfo);
                }
                if (declared_vtype == anyTypeInfo) {
                    declared_vtype = nullptr;
                }
                bool declared_type = declared_vtype && declared_vtype != autoTypeInfo;
                ReferenceHolder<QoreHashNode> hash(
                    new QoreHashNode(declared_type ? declared_vtype : autoTypeInfo), xsink);
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
                    if (i && !(i % 100) && qore_check_cancel(xsink, "IR hash literal construction")) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
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
                if (declared_type) {
                    qore_hash_private::get(*hash)->complexTypeInfo = qore_get_complex_hash_type(declared_vtype);
                } else {
                    if (!vtype || vtype == anyTypeInfo || !vcommon) {
                        vtype = autoTypeInfo;
                    }
                    qore_hash_private::get(*hash)->complexTypeInfo = qore_get_complex_hash_type(vtype);
                }
                setOwnedValueSlot(values, cleanup, inst->result.id, QoreValue(hash.release()), xsink);
                ++ip;
                break;
            }
            case QoreIROpcode::MakeHashConstKeys: {
                const auto* mhck = static_cast<const QoreIRMakeHashConstKeysInstruction*>(inst);
                const auto& ckeys = mhck->keys;
                size_t n = ckeys.size();
                assert(n == inst->operands.size());
                const QoreTypeInfo* typeInfo = substituteRuntimeTypeParams(mhck->typeInfo);
                const QoreTypeInfo* declared_vtype = QoreTypeInfo::getUniqueReturnComplexHash(typeInfo);
                if (!declared_vtype) {
                    declared_vtype = QoreTypeInfo::getReturnComplexHashOrNothing(typeInfo);
                }
                if (declared_vtype == anyTypeInfo) {
                    declared_vtype = nullptr;
                }
                bool declared_type = declared_vtype && declared_vtype != autoTypeInfo;
                ReferenceHolder<QoreHashNode> hash(
                    new QoreHashNode(declared_type ? declared_vtype : autoTypeInfo), xsink);
                qore_hash_private* hp = qore_hash_private::get(*hash);
                hp->hm.reserve(n);
                const QoreTypeInfo* vtype = nullptr;
                bool vcommon = false;
                for (size_t i = 0; i < n; ++i) {
                    if (i && !(i % 100) && qore_check_cancel(xsink, "IR hash literal construction")) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
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
                if (declared_type) {
                    hp->complexTypeInfo = qore_get_complex_hash_type(declared_vtype);
                } else {
                    if (!vtype || vtype == anyTypeInfo || !vcommon) {
                        vtype = autoTypeInfo;
                    }
                    hp->complexTypeInfo = qore_get_complex_hash_type(vtype);
                }
                setOwnedValueSlot(values, cleanup, inst->result.id, QoreValue(hash.release()), xsink);
                ++ip;
                break;
            }
            case QoreIROpcode::CreateEmptyList: {
                // element_type is the element type (e.g., bool); QoreListNode wraps it to list<bool>
                const QoreTypeInfo* elem_type = substituteRuntimeTypeParams(
                    inst->element_type ? inst->element_type : autoTypeInfo);
                QoreListNode* list = new QoreListNode(elem_type);
                setOwnedValueSlot(values, cleanup, inst->result.id, QoreValue(list), xsink);
                ++ip;
                break;
            }
            case QoreIROpcode::ListAppend: {
                QoreValue list_val = getIRValue(values, inst->operands[0]);
                QoreValue value = getIRValue(values, inst->operands[1]);
                QoreListNode* list = list_val.get<QoreListNode>();
                if (list) {
                    QoreIRValue value_operand = inst->operands[1];
                    bool transfer_owned_operand = value_operand.isValid()
                        && value_operand.id < values.size()
                        && value_operand.id < value_use_counts.size()
                        && value_use_counts[value_operand.id] == 1
                        && values[value_operand.id].hasNode()
                        && removeCleanupEntry(cleanup, value_operand.id);
                    QoreValue push_val;
                    if (transfer_owned_operand) {
                        push_val = values[value_operand.id];
                        values[value_operand.id] = QoreValue();
                    } else {
                        push_val = value.hasNode() ? value.refSelf() : value;
                    }
                    // Track element type for correct list<T> type info at runtime
                    qore_list_private::get(*list)->setListTypeFromNewElementType(
                        push_val.getFullTypeInfo());
                    list->push(push_val, xsink);
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
                    // Use element type from instruction (set by lowerPush) for proper
                    // coercion (e.g., list<softint> converts "3" to 3)
                    const QoreTypeInfo* elem_type = substituteRuntimeTypeParams(
                        inst->element_type ? inst->element_type : autoTypeInfo);
                    QoreListNode* l = new QoreListNode(elem_type);
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
                // element_type is the element type (e.g., bool); QoreListNode wraps it to list<bool>
                const QoreTypeInfo* elem_type = substituteRuntimeTypeParams(
                    inst->element_type ? inst->element_type : autoTypeInfo);
                QoreListNode* list = new QoreListNode(elem_type);
                if (capacity > 0) {
                    qore_list_private::get(*list)->reserve(static_cast<size_t>(capacity));
                }
                setOwnedValueSlot(values, cleanup, inst->result.id, QoreValue(list), xsink);
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
                    QoreIRValue value_operand = inst->operands[2];
                    bool transfer_owned_operand = value_operand.isValid()
                        && value_operand.id < values.size()
                        && value_operand.id < value_use_counts.size()
                        && value_use_counts[value_operand.id] == 1
                        && values[value_operand.id].hasNode()
                        && removeCleanupEntry(cleanup, value_operand.id);
                    QoreValue stored;
                    if (transfer_owned_operand) {
                        stored = values[value_operand.id];
                        values[value_operand.id] = QoreValue();
                    } else {
                        stored = val.hasNode() ? val.refSelf() : val;
                    }
                    qore_list_private* priv = qore_list_private::get(*l);
                    // Track element type for correct list<T> type info at runtime
                    priv->setListTypeFromNewElementType(stored.getTypeInfo());
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
                auto* closure_inst = static_cast<QoreIRExprInstruction*>(inst);
                QoreValue ref_val = getIRValue(values, inst->operands[0]);
                int nargs = static_cast<int>(inst->operands.size()) - 1;
                uint64_t ref_bits = toBits(ref_val);
                QoreValue result;
                bool inline_may_invalidate_external_caches = true;
                bool used_inline_ir = false;
                if (nargs == 0) {
                    used_inline_ir = tryExecuteInterpreterInlineIRClosure(ref_val, pgm, nullptr, 0, result, xsink,
                        &inline_may_invalidate_external_caches);
                    if (!used_inline_ir) {
                        result = fromBits(qore_rt_call_closure_0(ref_bits, xsink));
                    }
                } else if (nargs == 1) {
                    uint64_t arg0 = toBits(getIRValue(values, inst->operands[1]));
                    used_inline_ir = tryExecuteInterpreterInlineIRClosure(ref_val, pgm, &arg0, 1, result, xsink,
                        &inline_may_invalidate_external_caches);
                    if (!used_inline_ir) {
                        result = fromBits(qore_rt_call_closure_1(ref_bits, arg0, xsink));
                    }
                } else {
                    constexpr int SMALL_BUF = 8;
                    uint64_t small_buf[SMALL_BUF];
                    uint64_t* args = nargs <= SMALL_BUF ? small_buf : new uint64_t[nargs];
                    for (int i = 0; i < nargs; ++i) {
                        args[i] = toBits(getIRValue(values, inst->operands[i + 1]));
                    }
                    used_inline_ir = tryExecuteInterpreterInlineIRClosure(ref_val, pgm, args, nargs, result, xsink,
                        &inline_may_invalidate_external_caches);
                    if (!used_inline_ir) {
                        result = fromBits(qore_rt_call_closure_fast(ref_bits, args, nargs, xsink));
                    }
                    if (nargs > SMALL_BUF) {
                        delete[] args;
                    }
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
                if (closure_inst->has_ref_args) {
                    cleanupLocalCaches();
                } else if (!used_inline_ir || inline_may_invalidate_external_caches) {
                    invalidateExternalCaches();
                }
                if (resultValueIsNeeded(inst->result)) {
                    setValueSlot(values, inst->result.id, result, xsink);
                    if (result.hasNode()) {
                        cleanup.push_back(inst->result.id);
                    }
                } else {
                    result.discard(xsink);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::StringConcat: {
                // Multi-string concatenation - a + b + c + d in single pass
                if (inst->operands.empty()) {
                    setOwnedValueSlot(values, cleanup, inst->result.id,
                        QoreValue::makeStringValue(""), xsink);
                    ++ip;
                    break;
                }
                // Get first string to determine encoding
                QoreValue first = getIRValue(values, inst->operands[0]);
                const QoreEncoding* enc = QCS_DEFAULT;
                if (first.getType() == NT_STRING) {
                    QoreStringValueHelper first_str(first);
                    enc = first_str->getEncoding();
                }
                QoreStringNode* result = new QoreStringNode(enc);
                // Concatenate all operands
                for (const auto& operand : inst->operands) {
                    QoreValue v = getIRValue(values, operand);
                    if (v.getType() == NT_STRING) {
                        QoreStringValueHelper s(v);
                        result->concat(*s, xsink);
                        if (xsink && *xsink) {
                            result->deref();
                            cleanupValues(values, cleanup, xsink, true, cleanup_log);
                            cleanupLocalCaches();
                            return false;
                        }
                    }
                    // NOTHING values are skipped (treated as empty string)
                }
                setOwnedValueSlot(values, cleanup, inst->result.id, QoreValue(result), xsink);
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
            case QoreIROpcode::RefSelf: {
                if (inst->operands.empty()) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "refself missing operand");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                QoreValue val = getIRValue(values, inst->operands.front());
                traceIRRef(inst, "refself.input", inst->operands.front().id, val);
                QoreValue out = val.hasNode() ? val.refSelf() : val;
                traceIRRef(inst, "refself.output", inst->result.id, out);
                setOwnedValueSlot(values, cleanup, inst->result.id, out, xsink);
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
                // Pre-invalidation for all lvalue-modifying invoke opcodes.
                // Use extractLValueBaseVarRef on the full expression to handle ALL
                // lvalue operator types (assignment, compound assignment, shift, etc.)
                const VarRefNode* inv_base_var = nullptr;
                uint32_t inv_lvalue_sid = UINT32_MAX;
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
                        // Extract base variable from the full expression — handles ALL
                        // lvalue operator types (assignment, compound assignment, shift, etc.)
                        if (inv->expr.hasNode()) {
                            inv_base_var = extractLValueBaseVarRef(inv->expr);
                        }
                        if (inv_base_var
                                && (inv_base_var->getType() == VT_LOCAL || inv_base_var->getType() == VT_LOCAL_TS)
                                && inv_base_var->ref.id) {
                            ensureLocalInstantiated(inv_base_var->ref.id, instantiated_locals,
                                instantiated_locals_ordered, pre_instantiated,
                                function_own_locals, &locally_uninstantiated);
                            auto inv_slot_it = func.local_var_slots.find(
                                reinterpret_cast<const LocalVar*>(inv_base_var->ref.id));
                            if (inv_slot_it != func.local_var_slots.end()) {
                                inv_lvalue_sid = inv_slot_it->second;
                                if (inv_lvalue_sid < locals_slot_cache.size()) {
                                    locals_slot_cache[inv_lvalue_sid].discard(xsink);
                                    locals_slot_cache[inv_lvalue_sid] = QoreValue();
                                }
                                clearLoadSlots(inv_lvalue_sid);
                            }
                        }
                        break;
                    }
                    default:
                        break;
                }
                bool preserve_hash_key_weak_result = (inv->invoke_opcode == QoreIROpcode::HashKeyAccess
                    || inv->invoke_opcode == QoreIROpcode::LoadSelfMember
                    || inv->invoke_opcode == QoreIROpcode::LoadStaticVar) && isDotEvalOnlyBase(inst);
                QoreValue res = evalInvoke(inv, values, xsink, &func, block, ip, preserve_hash_key_weak_result);
                // Post-invalidation: ensure slot cache is cleared after the lvalue
                // operation modifies TLS. Always invalidate (not repopulate) since
                // the invoke expression may be a complex lvalue.
                if (inv_lvalue_sid < locals_slot_cache.size()) {
                    locals_slot_cache[inv_lvalue_sid].discard(xsink);
                    locals_slot_cache[inv_lvalue_sid] = QoreValue();
                    clearLoadSlots(inv_lvalue_sid);
                }
                // Invalidate external caches after Invoke — the AST call may have
                // modified globals, thread-locals, or closure variables.
                invalidateExternalCaches();

                // Calls with reference-capable arguments can write through to
                // caller locals, so the lighter external-cache invalidation is
                // not enough.
                if (inv->has_ref_args) {
                    switch (inv->invoke_opcode) {
                        case QoreIROpcode::Call:
                        case QoreIROpcode::CallDirect:
                        case QoreIROpcode::CallIndirect:
                        case QoreIROpcode::CallMethod:
                        case QoreIROpcode::CallStatic:
                        case QoreIROpcode::CallStaticDirect:
                        case QoreIROpcode::CallClosureDirect:
                            cleanupLocalCaches();
                            break;
                        default:
                            break;
                    }
                }
                // For reference write-through assignments via StoreLValue invoke, flush all local
                // caches — writing through a reference can modify any arbitrary local variable.
                if (inv->invoke_opcode == QoreIROpcode::StoreLValue && inv_base_var
                        && inv_base_var->getTypeInfo()
                        && QoreTypeInfo::isReference(inv_base_var->getTypeInfo())) {
                    cleanupLocalCaches();
                }
                if (xsink && *xsink) {
                    if (getenv("QORE_IR_TRACE_EXCEPTIONS")) {
                        fprintf(stderr, "[ir-exception] func='%s' invoke-op=%d loc=%s:%d ex-target=%p\n",
                            func.name.c_str(), static_cast<int>(inv->invoke_opcode),
                            inst->loc ? inst->loc->getFile() : "",
                            inst->loc ? inst->loc->start_line : 0,
                            static_cast<void*>(inv->exception_target));
                        fflush(stderr);
                    }
                    if (debug_active) {
                        tlpd->dbgException(getDebugStatement(inst), xsink);
                        if (getenv("QORE_IR_TRACE_EXCEPTIONS")) {
                            fprintf(stderr, "[ir-exception] func='%s' after-dbgException xsink=%d ex-target=%p\n",
                                func.name.c_str(), xsink && *xsink ? 1 : 0,
                                static_cast<void*>(inv->exception_target));
                            fflush(stderr);
                        }
                        // dbgException may dismiss the exception. AST execution aborts the
                        // statement that raised and then continues at statement cleanup; it
                        // does not resume the failed expression's normal continuation.
                        if (!*xsink) {
                            QoreIRBasicBlock* dismiss_block = nullptr;
                            size_t dismiss_ip = 0;
                            prev_block = block;
                            if (findDismissTarget(inv->normal_target, dismiss_block, dismiss_ip)) {
                                block = dismiss_block;
                                ip = dismiss_ip;
                            } else {
                                block = inv->normal_target;
                                ip = 0;
                            }
                            break;
                        }
                    }
                    if (!inv->exception_target) {
                        return returnAfterUnhandledException();
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    if (getenv("QORE_IR_TRACE_EXCEPTIONS")) {
                        fprintf(stderr, "[ir-exception] func='%s' branch-exception xsink=%d target=%p\n",
                            func.name.c_str(), xsink && *xsink ? 1 : 0,
                            static_cast<void*>(inv->exception_target));
                        fflush(stderr);
                    }
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
                if (getenv("QORE_IR_TRACE_EXCEPTIONS")) {
                    fprintf(stderr, "[ir-exception] func='%s' landingpad loc=%s:%d xsink=%d\n",
                        func.name.c_str(),
                        inst->loc ? inst->loc->getFile() : "",
                        inst->loc ? inst->loc->start_line : 0,
                        xsink && *xsink ? 1 : 0);
                    fflush(stderr);
                }
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
                                    executeHandlerBody(on_block_exit_handlers[i - 1], &obe_xsink, &locals_slot_cache);
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
                if (getenv("QORE_IR_TRACE_EXCEPTIONS")) {
                    fprintf(stderr, "[ir-exception] func='%s' catch.exception caught=%p xsink=%d\n",
                        func.name.c_str(), static_cast<void*>(caught), xsink && *xsink ? 1 : 0);
                    fflush(stderr);
                }
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
                        if (br->exception_target) {
                            block = br->exception_target;
                            ip = 0;
                            break;
                        }
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
                // Debug break: divert to the enclosing loop's exit.  Only the loop's
                // own condition BrIf has false_target == debug_break_target, so inner
                // if/switch BrIfs flow normally (preserving per-iteration cleanup) until
                // the loop header is reached.
                if (debug_break_target && br->false_target == debug_break_target) {
                    block = debug_break_target;
                    debug_break_target = nullptr;
                } else if (debug_break_loop) {
                    // legacy fallback: force false branch to exit the current loop
                    debug_break_loop = false;
                    block = br->false_target;
                } else {
                    block = cond.getAsBool() ? br->true_target : br->false_target;
                }
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
                        if (br->exception_target) {
                            block = br->exception_target;
                            ip = 0;
                            break;
                        }
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
                if (switch_val.getType() == NT_STRING) {
                    QoreStringValueHelper str(switch_val);
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
                bool result_slot_owned = false;
                bool weak_ref_load = false;

                if (local_inst->is_closure) {
                    // Closure path: always read from runtime stack (closures can
                    // modify the value between IR instructions)
                    if (local_inst->slot_id >= locals_instantiated.size()
                            || !locals_instantiated[local_inst->slot_id]) {
                        ensureLocalInstantiated(local_inst->local, instantiated_locals, instantiated_locals_ordered,
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
                        result_slot_owned = true;
                    } else if (!needs_deref && out.hasNode()) {
                        LocalVarValue* lvv = thread_try_find_lvar(local_inst->local);
                        weak_ref_load = lvv && isWeakReferenceType(lvv->val.getType());
                    }
                } else if (local_inst->auto_ref) {
                    // FAST PATH: direct slot cache access (most common case)
                    uint32_t sid = local_inst->slot_id;
                    if (sid != UINT32_MAX && sid < locals_slot_cache.size()) {
                        QoreValue cached_val = locals_slot_cache[sid];
                        bool is_ir_only_local = sid < locals_ir_only.size() && locals_ir_only[sid];
                        bool cache_has_value = !cached_val.isNothing()
                            || (is_ir_only_local && sid < locals_instantiated.size()
                                && locals_instantiated[sid]);
                        if (cache_has_value && cached_val.getType() != NT_REFERENCE) {
                            // Cache hit: one refSelf, no hash lookups, no instantiation check
                            out = cached_val.hasNode() ? cached_val.refSelf() : cached_val;
                            result_slot_owned = out.hasNode();
                            goto load_local_done;
                        }
                    }
                    // Cache miss: instantiate if needed, eval, populate cache
                    if (local_inst->local) {
                        if (local_inst->slot_id >= locals_instantiated.size()
                                || !locals_instantiated[local_inst->slot_id]) {
                            ensureLocalInstantiated(local_inst->local, instantiated_locals, instantiated_locals_ordered,
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
                        // Weak reference detection: LocalVar::eval() unwraps
                        // WeakReferenceNode/WeakHashReferenceNode/WeakListReferenceNode
                        // and returns the raw target with needs_deref=false. Caching the
                        // unwrapped value in the slot cache would create a persistent
                        // strong reference, preventing the destructor from firing when
                        // the last external strong ref is dropped. Skip caching so
                        // every access goes through TLS eval() which reads the current
                        // weak-ref target — matching AST-mode weak ref semantics.
                        // This is the LoadLocal-side fix for thread-object.qtest's
                        // transparent thread pattern; the matching fix in the while
                        // loop lowering emits DiscardTemps per iteration to drop the
                        // out slot's refSelf'd temp ref.
                        bool is_weak_ref_local = false;
                        if (!needs_deref && val.hasNode()) {
                            LocalVarValue* lvv = thread_try_find_lvar(local_inst->local);
                            if (lvv) {
                                qore_type_t raw_type = lvv->val.getType();
                                if (raw_type == NT_WEAKREF || raw_type == NT_WEAKREF_HASH
                                        || raw_type == NT_WEAKREF_LIST) {
                                    is_weak_ref_local = true;
                                }
                            }
                        }
                        // Store in slot cache for fast access on next load (unless weak-ref)
                        if (sid != UINT32_MAX && sid < locals_slot_cache.size()) {
                            locals_slot_cache[sid].discard(xsink);
                            if (is_weak_ref_local) {
                                // Clear cache entirely for weak-ref locals so next
                                // access re-evaluates via TLS (weak-ref target check)
                                locals_slot_cache[sid] = QoreValue();
                            } else {
                                locals_slot_cache[sid] = val.hasNode() ? val.refSelf() : val;
                            }
                        }
                        if (is_weak_ref_local && val.hasNode()) {
                            out = val;
                            weak_ref_load = true;
                        } else {
                            out = val.hasNode() ? val.refSelf() : val;
                            result_slot_owned = out.hasNode();
                        }
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
                            ensureLocalInstantiated(local_inst->local, instantiated_locals, instantiated_locals_ordered,
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
                            result_slot_owned = true;
                        } else if (!needs_deref && out.hasNode()) {
                            LocalVarValue* lvv = thread_try_find_lvar(local_inst->local);
                            weak_ref_load = lvv && isWeakReferenceType(lvv->val.getType());
                        }
                    }
                }
load_local_done:
                if (result_slot_owned) {
                    if (local_inst->result.id < values.size()
                            && weak_load_temp_slots.count(local_inst->result.id)) {
                        clearSlotForOverwrite(local_inst->result.id);
                    }
                    setValueSlot(values, local_inst->result.id, out, xsink);
                } else {
                    // Lvalue loads borrow the local's current node without taking a
                    // reference.  Do not use setValueSlot(), which would dereference
                    // a borrowed value left in this slot from an earlier iteration.
                    clearSlotForOverwrite(local_inst->result.id);
                    setValueSlotDirect(values, local_inst->result.id, out);
                    if (weak_ref_load && out.hasNode()) {
                        trackWeakLoadTemp(local_inst->result.id);
                    }
                }
                // Mark as local-owned for DGC container scan
                if (!weak_ref_load && local_inst->result.id >= 0) {
                    local_owned_slots.insert(local_inst->result.id);
                }
                if (result_slot_owned && out.hasNode()) {
                    cleanup.push_back(local_inst->result.id);
                }
                // Track LoadLocal result slot for cleanup in UninstantiateLocal
                // Only track node values that need reference cleanup; simple types
                // (int/float/bool) don't need tracking
                if (result_slot_owned
                        && out.hasNode()
                        && local_inst->slot_id != UINT32_MAX
                        && local_inst->slot_id < local_load_slots.size()) {
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
                bool is_weak_ref_local = false;
                bool result_slot_owned = false;
                if (!inst->operands.empty() && closure) {
                    QoreValue idx_val = getIRValue(values, inst->operands[0]);
                    int64 idx = idx_val.getAsBigInt();
                    QoreValue val;
                    if (idx >= 0 && static_cast<size_t>(idx) < closure->size()) {
                        val = (*closure)[static_cast<size_t>(idx)];
                    }
                    // Closure arg: val is shared reference, needs refSelf for value slot
                    out = val.hasNode() ? val.refSelf() : val;
                    result_slot_owned = out.hasNode();
                } else {
                    auto it = closures.find(local_inst->local);
                    if (it != closures.end() && it->second.getType() != NT_REFERENCE) {
                        // Cache hit: val is shared with cache, needs refSelf for value slot
                        QoreValue val = it->second;
                        out = val.hasNode() ? val.refSelf() : val;
                        result_slot_owned = out.hasNode();
                    } else if (local_inst->local) {
                        // Ensure the variable is instantiated before lookup.
                        // When a function with closureUse() vars executes in its own
                        // body (not as a closure), evalTiered skips instantiating these
                        // vars. ensureLocalInstantiated puts them on the cvstack.
                        bool has_env_cvv = thread_has_runtime_closure_env()
                            && thread_try_get_runtime_closure_var(local_inst->local);
                        if (!has_env_cvv && (local_inst->slot_id >= locals_instantiated.size()
                                || !locals_instantiated[local_inst->slot_id])) {
                            ensureLocalInstantiated(local_inst->local, instantiated_locals,
                                instantiated_locals_ordered, pre_instantiated,
                                function_own_locals, &locally_uninstantiated);
                            if (local_inst->slot_id < locals_instantiated.size()) {
                                locals_instantiated[local_inst->slot_id] = true;
                            }
                        }
                        ClosureVarValue* cv = resolve_closure_var_value(local_inst->local);
                        if (cv) {
                            is_weak_ref_local = isWeakReferenceType(cv->val.getType());
                            if (is_weak_ref_local) {
                                bool needs_deref = false;
                                QoreValue val = cv->eval(needs_deref, xsink);
                                if (xsink && *xsink) {
                                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                                    cleanupLocalCaches();
                                    return false;
                                }
                                out = val;
                            } else {
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
                                result_slot_owned = out.hasNode();
                            }
                        } else {
                            // Closure variable not found in closure context — fall back to
                            // local stack lookup. This happens when a function has variables
                            // with closureUse() set (captured by inner closures) but the
                            // current execution context is the function's own body, not a
                            // closure. In that case, the variable lives on the local stack.
                            if (LocalVarValue* lvv = thread_try_find_lvar(local_inst->local)) {
                                is_weak_ref_local = isWeakReferenceType(lvv->val.getType());
                                bool needs_deref = false;
                                out = lvv->eval(needs_deref, xsink);
                                if (xsink && *xsink) {
                                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                                    cleanupLocalCaches();
                                    return false;
                                }
                                // If needs_deref is false, we need to refSelf for the value slot
                                if (is_weak_ref_local) {
                                    // keep the borrowed weak target unowned
                                } else if (!needs_deref && out.hasNode()) {
                                    out = out.refSelf();
                                    result_slot_owned = true;
                                } else if (needs_deref && out.hasNode()) {
                                    result_slot_owned = true;
                                }
                            }
                        }
                    }
                }
                if (result_slot_owned) {
                    if (local_inst->result.id < values.size()
                            && weak_load_temp_slots.count(local_inst->result.id)) {
                        clearSlotForOverwrite(local_inst->result.id);
                    }
                    setValueSlot(values, local_inst->result.id, out, xsink);
                } else {
                    clearSlotForOverwrite(local_inst->result.id);
                    setValueSlotDirect(values, local_inst->result.id, out);
                    if (is_weak_ref_local && out.hasNode()) {
                        trackWeakLoadTemp(local_inst->result.id);
                    }
                }
                if (result_slot_owned && out.hasNode() && local_inst->auto_ref) {
                    cleanup.push_back(local_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::HashKeyAccess: {
                auto* hka_inst = static_cast<QoreIRHashKeyAccessInstruction*>(inst);
                bool preserve_weak_result = isDotEvalOnlyBase(inst);
                QoreValue raw_base = getIRValue(values, hka_inst->operands[0]);
                ValueEvalOptimizedRefHolder base_holder(raw_base, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                QoreValue base = *base_holder;
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
                } else if (base.getType() == NT_WEAKREF_HASH) {
                    const QoreHashNode* h = base.get<const WeakHashReferenceNode>()->get();
                    if (h) {
                        out = h->getKeyValue(hka_inst->key_name.c_str(), xsink);
                        if (xsink && *xsink) {
                            cleanupValues(values, cleanup, xsink, true, cleanup_log);
                            cleanupLocalCaches();
                            return false;
                        }
                        out.refSelf();
                        if (!h->getHashDecl() && !out.isNothing()) {
                            const QoreTypeInfo* vti = h->getValueTypeInfo();
                            if (QoreTypeInfo::hasType(vti) && vti != autoTypeInfo && vti != anyTypeInfo
                                    && !QoreTypeInfo::superSetOf(vti, out.getTypeInfo())) {
                                ValueHolder holder(out, xsink);
                                QoreTypeInfo::acceptInputKey(vti, hka_inst->key_name.c_str(), *holder, xsink);
                                if (xsink && *xsink) {
                                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                                    cleanupLocalCaches();
                                    return false;
                                }
                                out = holder.release();
                            }
                        }
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
                    if (!h->getHashDecl() && !out.isNothing()) {
                        const QoreTypeInfo* vti = h->getValueTypeInfo();
                        if (QoreTypeInfo::hasType(vti) && vti != autoTypeInfo && vti != anyTypeInfo
                                && !QoreTypeInfo::superSetOf(vti, out.getTypeInfo())) {
                            ValueHolder holder(out, xsink);
                            QoreTypeInfo::acceptInputKey(vti, hka_inst->key_name.c_str(), *holder, xsink);
                            if (xsink && *xsink) {
                                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                                cleanupLocalCaches();
                                return false;
                            }
                            out = holder.release();
                        }
                    }
                } else if (base.getType() == NT_OBJECT) {
                    QoreObject* o = const_cast<QoreObject*>(base.get<const QoreObject>());
                    out = o->evalMember(hka_inst->key_name.c_str(), xsink);
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                }
                if (!preserve_weak_result) {
                    evaluateOwnedWeakReferenceResult(out, xsink);
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
                QoreValue raw_base = getIRValue(values, hka_inst->operands[0]);
                ValueEvalOptimizedRefHolder base_holder(raw_base, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                QoreValue base = *base_holder;
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
                ValueHolder val_holder(val.refSelf(), xsink);
                if (hash_val.getType() == NT_HASH) {
                    QoreHashNode* h = hash_val.get<QoreHashNode>();

                    // Keep RHS referenced before COW, matching QoreAssignmentOperatorNode.
                    // This makes `h.b = h` copy the outer hash before storing the original.
                    if (h->reference_count() > 1) {
                        // COW: create unique copy and update the local variable.
                        // Pass new_h via TRANSFER (no refSelf) so the typed-lvalue coercion
                        // in LValueHelper::assign takes the "unique" in-place branch for
                        // hash<auto!> etc., leaving new_h in TLS at refcount 1.
                        QoreHashNode* new_h = h->copy();  // refcount 1, unique
                        // Prefer container_lv (set at AOT deser time) over
                        // container->ref.id (fresh-parse path); the container
                        // VarRefNode is not serialized, so AOT-loaded closure
                        // bodies have container==nullptr and must use _lv.
                        LocalVar* lv;
                        bool is_closure;
                        if (hks_inst->container_lv) {
                            lv = hks_inst->container_lv;
                            // Check at runtime: closureUse may be set after AOT
                            // deser time when a later closure captures this var.
                            is_closure = lv->closureUse();
                        } else {
                            lv = const_cast<LocalVar*>(
                                reinterpret_cast<const LocalVar*>(hks_inst->container->ref.id));
                            is_closure = (hks_inst->container->getType() == VT_CLOSURE);
                        }
                        if (is_closure) {
                            assignClosureVarValueTransfer(lv, QoreValue(new_h), xsink);
                        } else {
                            assignLocalVarValueTransfer(lv, QoreValue(new_h), xsink);
                        }
                        // After COW, LoadClosure's `closures` cache still holds a ref
                        // to the pre-COW hash (old contents).  Invalidate so the next
                        // load re-reads through the CVV and sees the new hash.
                        invalidateClosureCacheLv(lv);
                        if (xsink && *xsink) {
                            // new_h's ref was consumed by assign*Transfer (either stored
                            // in TLS or discarded on failure). Do not deref here.
                            cleanupValues(values, cleanup, xsink, true, cleanup_log);
                            cleanupLocalCaches();
                            return false;
                        }
                        // new_h is now in TLS with refcount 1 — safe for setKeyValue's
                        // reference_count() == 1 assertion.
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
                    discardContainerValueSlot(hks_inst->operands[0].id, hks_inst->container_slot_id,
                        isClosureContainer(hks_inst->container_lv, hks_inst->container));
                } else if (hash_val.isNothing()) {
                    LocalVar* lv;
                    bool is_closure;
                    if (hks_inst->container_lv) {
                        lv = hks_inst->container_lv;
                        is_closure = lv->closureUse();
                    } else {
                        lv = const_cast<LocalVar*>(
                            reinterpret_cast<const LocalVar*>(hks_inst->container->ref.id));
                        is_closure = (hks_inst->container->getType() == VT_CLOSURE);
                    }

                    // Auto-vivify according to the declared lvalue type, matching
                    // LValueHelper::doHashLValue() including hashdecl error behavior.
                    const QoreTypeInfo* ti = lv ? lv->getTypeInfoForLValue() : nullptr;
                    ti = qore_substitute_type_params_if_needed(ti);
                    QoreHashNode* new_h = makeImplicitHashForLValueType(ti, xsink);
                    if (!new_h || (xsink && *xsink)) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    new_h->setKeyValue(hks_inst->key_name.c_str(), val.refSelf(), xsink);
                    if (xsink && *xsink) {
                        new_h->deref(xsink);
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    if (is_closure) {
                        assignClosureVarValueTransfer(lv, QoreValue(new_h), xsink);
                    } else {
                        assignLocalVarValueTransfer(lv, QoreValue(new_h), xsink);
                    }
                    // Auto-vivify replaces NOTHING with a new hash; any prior LoadClosure
                    // of this var cached a stale NOTHING (or empty hash) — invalidate.
                    invalidateClosureCacheLv(lv);
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    clearLoadSlots(hks_inst->container_slot_id);
                    uint32_t csid = hks_inst->container_slot_id;
                    if (csid != UINT32_MAX && csid < locals_slot_cache.size()) {
                        locals_slot_cache[csid].discard(xsink);
                    }
                    discardContainerValueSlot(hks_inst->operands[0].id, hks_inst->container_slot_id,
                        isClosureContainer(hks_inst->container_lv, hks_inst->container));
                } else if (hash_val.getType() == NT_OBJECT) {
                    assignObjectMemberValue(const_cast<QoreObject*>(hash_val.get<const QoreObject>()),
                        hks_inst->key_name.c_str(), val, xsink);
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
                if (hks_inst->result.isValid()) {
                    setValueSlot(values, hks_inst->result.id, val, xsink);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::HashKeyStoreDynamic: {
                auto* hksd_inst = static_cast<QoreIRHashKeyStoreDynamicInstruction*>(inst);
                QoreValue hash_val = getIRValue(values, hksd_inst->operands[0]);
                QoreValue val      = getIRValue(values, hksd_inst->operands[1]);
                QoreValue key_val  = getIRValue(values, hksd_inst->operands[2]);
                ValueHolder val_holder(val.refSelf(), xsink);
                // Convert key to string
                QoreStringValueHelper key_str(key_val);
                if (hash_val.getType() == NT_HASH) {
                    QoreHashNode* h = hash_val.get<QoreHashNode>();
                    if (h->reference_count() > 1) {
                        // COW: see HashKeyStore above for rationale on *Transfer variants
                        QoreHashNode* new_h = h->copy();  // refcount 1, unique
                        LocalVar* lv;
                        bool is_closure;
                        if (hksd_inst->container_lv) {
                            lv = hksd_inst->container_lv;
                            is_closure = lv->closureUse();
                        } else {
                            lv = const_cast<LocalVar*>(
                                reinterpret_cast<const LocalVar*>(hksd_inst->container->ref.id));
                            is_closure = (hksd_inst->container->getType() == VT_CLOSURE);
                        }
                        if (is_closure) {
                            assignClosureVarValueTransfer(lv, QoreValue(new_h), xsink);
                        } else {
                            assignLocalVarValueTransfer(lv, QoreValue(new_h), xsink);
                        }
                        // After COW, invalidate stale LoadClosure cache entry
                        invalidateClosureCacheLv(lv);
                        if (xsink && *xsink) {
                            cleanupValues(values, cleanup, xsink, true, cleanup_log);
                            cleanupLocalCaches();
                            return false;
                        }
                        h = new_h;
                    }
                    // Check hashdecl key validity before assignment
                    if (qore_hash_private::get(*h)->checkKey(key_str->c_str(), xsink)) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    h->setKeyValue(key_str->c_str(), val.refSelf(), xsink);
                    clearLoadSlots(hksd_inst->container_slot_id);
                    uint32_t csid = hksd_inst->container_slot_id;
                    if (csid != UINT32_MAX && csid < locals_slot_cache.size()) {
                        locals_slot_cache[csid].discard(xsink);
                    }
                    discardContainerValueSlot(hksd_inst->operands[0].id, hksd_inst->container_slot_id,
                        isClosureContainer(hksd_inst->container_lv, hksd_inst->container));
                } else if (hash_val.isNothing()) {
                    LocalVar* lv;
                    bool is_closure;
                    if (hksd_inst->container_lv) {
                        lv = hksd_inst->container_lv;
                        is_closure = lv->closureUse();
                    } else {
                        lv = const_cast<LocalVar*>(
                            reinterpret_cast<const LocalVar*>(hksd_inst->container->ref.id));
                        is_closure = (hksd_inst->container->getType() == VT_CLOSURE);
                    }

                    // Auto-vivify according to the declared lvalue type, matching
                    // LValueHelper::doHashLValue() including hashdecl error behavior.
                    const QoreTypeInfo* ti = lv ? lv->getTypeInfoForLValue() : nullptr;
                    ti = qore_substitute_type_params_if_needed(ti);
                    QoreHashNode* new_h = makeImplicitHashForLValueType(ti, xsink);
                    if (!new_h || (xsink && *xsink)) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    new_h->setKeyValue(key_str->c_str(), val.refSelf(), xsink);
                    if (xsink && *xsink) {
                        new_h->deref(xsink);
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    if (is_closure) {
                        assignClosureVarValueTransfer(lv, QoreValue(new_h), xsink);
                    } else {
                        assignLocalVarValueTransfer(lv, QoreValue(new_h), xsink);
                    }
                    // Auto-vivify replaces NOTHING; invalidate stale LoadClosure cache
                    invalidateClosureCacheLv(lv);
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    clearLoadSlots(hksd_inst->container_slot_id);
                    uint32_t csid = hksd_inst->container_slot_id;
                    if (csid != UINT32_MAX && csid < locals_slot_cache.size()) {
                        locals_slot_cache[csid].discard(xsink);
                    }
                    discardContainerValueSlot(hksd_inst->operands[0].id, hksd_inst->container_slot_id,
                        isClosureContainer(hksd_inst->container_lv, hksd_inst->container));
                } else if (hash_val.getType() == NT_OBJECT) {
                    assignObjectMemberValue(const_cast<QoreObject*>(hash_val.get<const QoreObject>()),
                        key_str->c_str(), val, xsink);
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                if (hksd_inst->result.isValid()) {
                    setValueSlot(values, hksd_inst->result.id, val, xsink);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::ListIndexAccess: {
                QoreValue raw_list_val = getIRValue(values, inst->operands[0]);
                ValueEvalOptimizedRefHolder list_holder(raw_list_val, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                QoreValue list_val = *list_holder;
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
                ValueHolder val_holder(val.refSelf(), xsink);
                int64_t index = idx_val.getAsBigInt();
                // NOTE: the do-while(false) wrapper lets every error path below break out to the common tail, which
                // routes the exception to the instruction's exception target (i.e. the enclosing catch block).
                // Returning directly from an error path here would make the exception bypass any enclosing
                // try/catch in the same function
                do {
                    if (list_val.getType() == NT_LIST) {
                        QoreListNode* l = list_val.get<QoreListNode>();

                        // At this point, refcount = TLS (1) only (no artificial refs held).
                        // Trigger COW if there are additional external references beyond TLS.
                        if (l->reference_count() > 1) {
                            // COW: see HashKeyStore above for rationale on *Transfer variants.
                            // Same bug applies to list<auto!> type coercion in LValueHelper::assign.
                            QoreListNode* new_l = l->copy();  // refcount 1, unique
                            // Prefer container_lv (resolved at AOT deser time) over
                            // container->ref.id (fresh-parse path); the container
                            // VarRefNode is not serialized, so AOT-loaded closure
                            // bodies have container==nullptr and must use _lv.
                            LocalVar* lv;
                            bool is_closure;
                            if (lis_inst->container_lv) {
                                lv = lis_inst->container_lv;
                                // Check at runtime: closureUse may be set after AOT
                                // deser time when a later closure captures this var.
                                is_closure = lv->closureUse();
                            } else {
                                lv = const_cast<LocalVar*>(
                                    reinterpret_cast<const LocalVar*>(lis_inst->container->ref.id));
                                is_closure = (lis_inst->container->getType() == VT_CLOSURE);
                            }
                            if (is_closure) {
                                assignClosureVarValueTransfer(lv, QoreValue(new_l), xsink);
                            } else {
                                assignLocalVarValueTransfer(lv, QoreValue(new_l), xsink);
                            }
                            // After COW, invalidate stale LoadClosure cache entry
                            invalidateClosureCacheLv(lv);
                            if (xsink && *xsink) {
                                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                                cleanupLocalCaches();
                                return false;
                            }
                            l = new_l;
                        }

                        // NOTE: the index resolution and the store below must not exit the instruction early on
                        // error; they fall through to the container-slot cleanup that follows (exactly as
                        // HashKeyStore does after setKeyValue()), and the common tail then routes the exception to
                        // the enclosing catch block.  Skipping that cleanup leaves the borrowed container
                        // reference in its value slot and it is released a second time by the generic value
                        // cleanup at function exit

                        // Resolve the index against the list; negative indices count from the end when
                        // %negative-offsets is in effect, otherwise they are an error.  Without this check a
                        // negative index reaches QoreListNode::setEntry() as a huge size_t and writes before the
                        // start of the entry array, corrupting the heap.  Same semantics as
                        // qore_rt_list_index_store_cow() (JIT) and qore_rt_list_index_store_cow_aot() (AOT).
                        if (index < 0) {
                            if (runtime_check_parse_option(PO_NEGATIVE_OFFSETS)) {
                                index += static_cast<int64_t>(l->size());
                            }
                            if (index < 0) {
                                xsink->raiseException("NEGATIVE-LIST-INDEX", "list index " QLLD " is invalid (index "
                                    "must evaluate to a non-negative integer)", idx_val.getAsBigInt());
                            }
                        }

                        if (!(xsink && *xsink)) {
                            // Apply element type coercion if the list has a typed value type
                            // (e.g. list<softint> converts "50" → 50 before storing)
                            const QoreTypeInfo* vti = qore_list_private::get(*l)->getValueTypeInfo();
                            QoreValue entry = val.refSelf();
                            if (QoreTypeInfo::hasType(vti)
                                    && !QoreTypeInfo::superSetOf(vti, entry.getTypeInfo())) {
                                QoreTypeInfo::acceptAssignment(vti,
                                    "<list element assignment>", entry, xsink);
                            }
                            if (xsink && *xsink) {
                                entry.discard(xsink);
                            } else {
                                // Make the update with already-referenced (and potentially coerced) value.
                                // (list is already in TLS and will be cleaned up normally)
                                l->setEntry(index, entry, xsink);
                            }
                        }

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
                        discardContainerValueSlot(lis_inst->operands[0].id, lis_inst->container_slot_id,
                            isClosureContainer(lis_inst->container_lv, lis_inst->container));
                    } else if (list_val.isNothing()) {
                        // The list does not exist yet, so a negative index cannot be resolved from the end even with
                        // %negative-offsets; matches the JIT and AOT auto-vivify paths.
                        if (index < 0) {
                            xsink->raiseException("NEGATIVE-LIST-INDEX", "list index " QLLD " is invalid (index must "
                                "evaluate to a non-negative integer)", index);
                            break;
                        }
                        // Auto-vivify: use container's declared type so the element
                        // type comes out right (softlist<bool> → list<bool>, not list<auto>).
                        // Use *Transfer so typed lvalues (list<auto!>) take the unique
                        // in-place branch — no extra copy.
                        // Prefer container_lv (resolved at AOT deser time) over container:
                        // AOT-loaded bodies have container==nullptr (see COW branch above).
                        LocalVar* lv;
                        bool is_closure;
                        if (lis_inst->container_lv) {
                            lv = lis_inst->container_lv;
                            is_closure = lv->closureUse();
                        } else {
                            lv = const_cast<LocalVar*>(
                                reinterpret_cast<const LocalVar*>(lis_inst->container->ref.id));
                            is_closure = (lis_inst->container->getType() == VT_CLOSURE);
                        }
                        // Derive the declared lvalue type AOT-safely: from the container
                        // VarRefNode on the fresh-parse path, else from the LocalVar
                        // (resolving type parameters, as HashKeyStore auto-vivify does).
                        const QoreTypeInfo* varTI = lis_inst->container
                            ? lis_inst->container->getTypeInfo()
                            : (lv ? qore_substitute_type_params_if_needed(lv->getTypeInfoForLValue())
                                  : nullptr);
                        const QoreTypeInfo* elemTI = QoreTypeInfo::getReturnComplexListOrNothing(varTI);
                        QoreListNode* new_l = new QoreListNode(elemTI ? elemTI : autoTypeInfo);
                        {
                            const QoreTypeInfo* vti = qore_list_private::get(*new_l)->getValueTypeInfo();
                            QoreValue entry = val.refSelf();
                            if (QoreTypeInfo::hasType(vti)
                                    && !QoreTypeInfo::superSetOf(vti, entry.getTypeInfo())) {
                                QoreTypeInfo::acceptAssignment(vti,
                                    "<list element assignment>", entry, xsink);
                            }
                            if (!(xsink && *xsink)) {
                                new_l->setEntry(index, entry, xsink);
                            } else {
                                entry.discard(xsink);
                            }
                        }
                        if (xsink && *xsink) {
                            new_l->deref(xsink);
                            break;
                        }
                        if (is_closure) {
                            assignClosureVarValueTransfer(lv, QoreValue(new_l), xsink);
                        } else {
                            assignLocalVarValueTransfer(lv, QoreValue(new_l), xsink);
                        }
                        // Auto-vivify replaces NOTHING; invalidate stale LoadClosure cache
                        invalidateClosureCacheLv(lv);
                        if (xsink && *xsink) {
                            break;
                        }
                        clearLoadSlots(lis_inst->container_slot_id);
                        uint32_t csid = lis_inst->container_slot_id;
                        if (csid != UINT32_MAX && csid < locals_slot_cache.size()) {
                            locals_slot_cache[csid].discard(xsink);
                        }
                        discardContainerValueSlot(lis_inst->operands[0].id, lis_inst->container_slot_id,
                            isClosureContainer(lis_inst->container_lv, lis_inst->container));
                    }
                } while (false);
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
                    ensureLocalInstantiated(fused_inst->target, instantiated_locals, instantiated_locals_ordered,
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
                    if (fused_inst->target_slot_id < locals_slot_cache.size()) {
                        locals_slot_cache[fused_inst->target_slot_id] = QoreValue(target_val);
                    }
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
                    ensureLocalInstantiated(fused_inst->source, instantiated_locals, instantiated_locals_ordered,
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
                    if (fused_inst->source_slot_id < locals_slot_cache.size()) {
                        locals_slot_cache[fused_inst->source_slot_id] = QoreValue(source_val);
                    }
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
                    if (fused_inst->target->closureUse()) {
                        // Closure-use variable: write through cvstack, not lvstack
                        assignClosureVarValue(fused_inst->target, QoreValue(result_val), xsink);
                        if (xsink && *xsink) {
                            cleanupValues(values, cleanup, xsink, true, cleanup_log);
                            cleanupLocalCaches();
                            return false;
                        }
                        updateClosureCacheInt(fused_inst->target, result_val);
                    } else if (fused_inst->target_slot_id < locals_lvar_cache.size()) {
                        // Use cached LocalVarValue* for direct write-through (avoids TLS lookup)
                        LocalVarValue*& lvv = locals_lvar_cache[fused_inst->target_slot_id];
                        if (!lvv) {
                            lvv = thread_try_find_lvar(fused_inst->target);
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
                markParentSlotDirty(fused_inst->target_slot_id);
                // Set result value
                if (fused_inst->result.isValid()) {
                    setValueSlot(values, fused_inst->result.id, QoreValue(result_val), xsink);
                }
                ++ip;
                break;
            }

            case QoreIROpcode::IncrementLocalInt: {
                auto* fused_inst = static_cast<QoreIRIncrementLocalIntInstruction*>(inst);
                if (fused_inst->local && fused_inst->local->closureUse()) {
                    int64_t result_val = 0;
                    if (incrementClosureVarIntFast(fused_inst->local, fused_inst->delta, result_val, xsink)) {
                        if (fused_inst->slot_id < locals_slot_cache.size()) {
                            locals_slot_cache[fused_inst->slot_id] = QoreValue(result_val);
                        }
                        updateClosureCacheInt(fused_inst->local, result_val);
                        markParentSlotDirty(fused_inst->slot_id);
                        if (fused_inst->result.isValid()) {
                            setValueSlot(values, fused_inst->result.id, QoreValue(result_val), xsink);
                        }
                        ++ip;
                        break;
                    }
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                }
                // Read local from slot cache (fast path) or eval (cold path)
                int64_t local_val = 0;
                if (fused_inst->slot_id < locals_slot_cache.size()
                        && !locals_slot_cache[fused_inst->slot_id].isNothing()) {
                    local_val = locals_slot_cache[fused_inst->slot_id].getAsBigInt();
                } else {
                    ensureLocalInstantiated(fused_inst->local, instantiated_locals, instantiated_locals_ordered,
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
                    if (fused_inst->slot_id < locals_slot_cache.size()) {
                        locals_slot_cache[fused_inst->slot_id] = QoreValue(local_val);
                    }
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
                    if (fused_inst->local->closureUse()) {
                        // Closure-use variable: write through cvstack, not lvstack
                        assignClosureVarValue(fused_inst->local, QoreValue(result_val), xsink);
                        if (xsink && *xsink) {
                            cleanupValues(values, cleanup, xsink, true, cleanup_log);
                            cleanupLocalCaches();
                            return false;
                        }
                        updateClosureCacheInt(fused_inst->local, result_val);
                    } else if (fused_inst->slot_id < locals_lvar_cache.size()) {
                        // Use cached LocalVarValue* for direct write-through (avoids TLS lookup)
                        LocalVarValue*& lvv = locals_lvar_cache[fused_inst->slot_id];
                        if (!lvv) {
                            lvv = thread_try_find_lvar(fused_inst->local);
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
                markParentSlotDirty(fused_inst->slot_id);
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
                    ensureLocalInstantiated(fused_inst->lhs, instantiated_locals, instantiated_locals_ordered,
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
                    if (fused_inst->lhs_slot_id < locals_slot_cache.size()) {
                        locals_slot_cache[fused_inst->lhs_slot_id] = QoreValue(lhs_val);
                    }
                    if (nd && lv.hasNode()) {
                        lv.getInternalNode()->deref(xsink);
                    }
                }
                int64_t rhs_val = 0;
                if (fused_inst->rhs_slot_id < locals_slot_cache.size()
                        && !locals_slot_cache[fused_inst->rhs_slot_id].isNothing()) {
                    rhs_val = locals_slot_cache[fused_inst->rhs_slot_id].getAsBigInt();
                } else {
                    ensureLocalInstantiated(fused_inst->rhs, instantiated_locals, instantiated_locals_ordered,
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
                    if (fused_inst->rhs_slot_id < locals_slot_cache.size()) {
                        locals_slot_cache[fused_inst->rhs_slot_id] = QoreValue(rhs_val);
                    }
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
                        if (fused_inst->exception_target) {
                            block = fused_inst->exception_target;
                            ip = 0;
                            break;
                        }
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
                    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), nullptr);
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
                if (qore_ir_check_closure_self_valid(obj, xsink)) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                ValueHolder val(obj->getReferencedMemberNoMethod(sm_inst->member_name.c_str(), xsink), xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                // Dot-eval method dispatch handles weak refs itself. Preserve the raw
                // reference when this load is only used as a dot-eval base.
                bool needs_eval = !isDotEvalOnlyBase(inst) && val->needsEval();
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
                // Resolve vi from expr if not set (AOT-deserialized handler IR)
                if (!sv_inst->vi && sv_inst->expr.getType() == NT_CLASS_VARREF) {
                    sv_inst->vi = &(static_cast<StaticClassVarRefNode*>(
                        sv_inst->expr.getInternalNode()))->vi;
                }
                // issue 3523: evaluate in case the value is a reference
                ValueHolder val(sv_inst->vi->getReferencedValue(sv_inst->var_name.c_str(), xsink),
                        xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                bool needs_eval = !isDotEvalOnlyBase(inst) && val->needsEval();
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
                const QoreClass* qc = no_inst->qc;
                const AbstractQoreFunctionVariant* variant = no_inst->variant;
                if (!qc && no_inst->expr.hasNode()) {
                    const AbstractQoreNode* node = no_inst->expr.getInternalNode();
                    if (auto* no = dynamic_cast<const NewObjectCallNode*>(node)) {
                        qc = no->getClass();
                        variant = no->getVariant();
                        no_inst->object_type_info = no->getObjectTypeInfo();
                    } else if (auto* scoped = dynamic_cast<const ScopedObjectCallNode*>(node)) {
                        qc = scoped->oc;
                        variant = scoped->getVariant();
                        no_inst->object_type_info = scoped->getObjectTypeInfo();
                    } else if (auto* vrn = dynamic_cast<const VarRefNewObjectNode*>(node)) {
                        qc = QoreTypeInfo::getUniqueReturnClass(vrn->getTypeInfo());
                        variant = vrn->getVariant();
                        no_inst->object_type_info = vrn->getTypeInfo();
                    }
                    if (qc) {
                        no_inst->qc = qc;
                        no_inst->variant = variant;
                    }
                }
                if (!qc) {
                    xsink->raiseException("RUNTIME-ERROR",
                        "cannot construct object: class not resolved");
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                // Build NaN-boxed arg array from pre-computed IR operand values
                int nargs = static_cast<int>(no_inst->operands.size());
                constexpr int SMALL_BUF = 8;
                uint64_t nb_buf[SMALL_BUF];
                uint64_t* nb_args = nargs <= SMALL_BUF ? nb_buf : new uint64_t[nargs];
                for (int i = 0; i < nargs; ++i) {
                    nb_args[i] = toBits(getIRValue(values, no_inst->operands[i]));
                }
                // Dispatch via the shared no-AST runtime helper
                uint64_t rv = qore_rt_new_object_nb(qc, variant, no_inst->object_type_info, nb_args, nargs, xsink);
                if (nargs > SMALL_BUF) delete[] nb_args;
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                // Constructor may call closures that modify captured variables
                invalidateExternalCaches();
                QoreValue out = fromBits(rv);
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
                const RuntimeConstantRefNode* rcr = lc_inst->node
                    ? lc_inst->node
                    : dynamic_cast<const RuntimeConstantRefNode*>(lc_inst->expr.getInternalNode());
                if (rcr) {
                    out = const_cast<RuntimeConstantRefNode*>(rcr)->eval(
                            needs_deref, xsink);
                    if (!needs_deref && out.hasNode()) {
                        out = out.refSelf();
                    }
                    // Set exception location to the constant location if an exception occurred
                    if (xsink && *xsink && rcr->loc) {
                        xsink->setLastLocation(*rcr->loc);
                    }
                } else {
                    // AOT mode: expr holds the resolved constant value directly.
                    out = lc_inst->expr.needsEval()
                        ? lc_inst->expr.eval(xsink)
                        : lc_inst->expr.refSelf();
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
                            ensureLocalInstantiated(var, instantiated_locals, instantiated_locals_ordered, pre_instantiated,
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
                            ensureLocalInstantiated(var, instantiated_locals, instantiated_locals_ordered, pre_instantiated,
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
                QoreValue out;
                RuntimeConfig& rc = rc_get_current_ref();
                if (pr_inst->node) {
                    prepareParseReferenceLocal(pr_inst->node);
                    if (!pr_inst->operands.empty()) {
                        QoreValue key = getIRValue(values, pr_inst->operands[0]);
                        out = const_cast<ParseReferenceNode*>(pr_inst->node)->evalToRefWithResolvedHashKey(
                            rc, key, xsink);
                    } else {
                        out = const_cast<ParseReferenceNode*>(pr_inst->node)->evalToRef(rc, xsink);
                    }
                } else if (pr_inst->expr.hasNode()) {
                    // AOT mode: node is null; expr holds the reconstructed ParseReferenceNode
                    const ParseReferenceNode* prn = dynamic_cast<const ParseReferenceNode*>(
                        pr_inst->expr.getInternalNode());
                    if (!prn) {
                        xsink->raiseException("RUNTIME-ERROR", "parse reference expression has invalid type in AOT mode");
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    prepareParseReferenceLocal(prn);
                    if (!pr_inst->operands.empty() && prn) {
                        QoreValue key = getIRValue(values, pr_inst->operands[0]);
                        out = const_cast<ParseReferenceNode*>(prn)->evalToRefWithResolvedHashKey(rc, key, xsink);
                    } else {
                        out = const_cast<ParseReferenceNode*>(prn)->evalToRef(rc, xsink);
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
                const AbstractQoreNode* node = nhd_inst->node
                    ? nhd_inst->node
                    : nhd_inst->expr.getInternalNode();
                const NewHashDeclNode* nhd = dynamic_cast<const NewHashDeclNode*>(node);
                if (!nhd) {
                    raiseMissingAOTExpr("new hashdecl", nhd_inst->expr);
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                QoreValue out = const_cast<NewHashDeclNode*>(nhd)->eval(needs_deref, xsink);
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
                const AbstractQoreNode* node = nch_inst->node
                    ? nch_inst->node
                    : nch_inst->expr.getInternalNode();
                const NewComplexHashNode* nch = dynamic_cast<const NewComplexHashNode*>(node);
                if (!nch) {
                    raiseMissingAOTExpr("new complex hash", nch_inst->expr);
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                QoreValue out = const_cast<NewComplexHashNode*>(nch)->eval(needs_deref, xsink);
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
                const AbstractQoreNode* node = ncl_inst->node
                    ? ncl_inst->node
                    : ncl_inst->expr.getInternalNode();
                const NewComplexListNode* ncl = dynamic_cast<const NewComplexListNode*>(node);
                if (!ncl) {
                    raiseMissingAOTExpr("new complex list", ncl_inst->expr);
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                QoreValue out = const_cast<NewComplexListNode*>(ncl)->eval(needs_deref, xsink);
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
            case QoreIROpcode::NewComplexBuffer: {
                auto* ncb_inst = static_cast<QoreIRNewComplexBufferInstruction*>(inst);
                const AbstractQoreNode* node = ncb_inst->node
                    ? ncb_inst->node
                    : ncb_inst->expr.getInternalNode();
                const NewComplexBufferNode* ncb = dynamic_cast<const NewComplexBufferNode*>(node);
                if (!ncb) {
                    raiseMissingAOTExpr("new complex buffer", ncb_inst->expr);
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                QoreValue out;
                if (!ncb_inst->operands.empty()) {
                    QoreValue init = getIRValue(values, ncb_inst->operands[0]);
                    out = fromBits(qore_rt_new_complex_buffer_from_value(ncb->typeInfo, toBits(init), xsink));
                } else {
                    bool needs_deref = true;
                    out = const_cast<NewComplexBufferNode*>(ncb)->eval(needs_deref, xsink);
                    if (!needs_deref && out.hasNode()) {
                        out = out.refSelf();
                    }
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                setValueSlot(values, ncb_inst->result.id, out, xsink);
                if (out.hasNode()) {
                    cleanup.push_back(ncb_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::VrnConstruct: {
                auto* vrn_inst = static_cast<QoreIRVrnConstructInstruction*>(inst);
                QoreValue out;
                if (!vrn_inst->operands.empty()) {
                    QoreValue init = getIRValue(values, vrn_inst->operands[0]);
                    const AbstractQoreNode* node = vrn_inst->expr.getInternalNode();
                    const QoreTypeInfo* typeInfo = nullptr;
                    bool is_hash = false;
                    bool is_list = false;
                    if (vrn_inst->vrn) {
                        typeInfo = vrn_inst->vrn->getTypeInfo();
                        is_hash = vrn_inst->vrn->isComplexHashConstruct();
                        is_list = vrn_inst->vrn->isComplexListConstruct();
                    } else if (auto* vrn = dynamic_cast<const VarRefNewObjectNode*>(node)) {
                        typeInfo = vrn->getTypeInfo();
                        is_hash = vrn->isComplexHashConstruct();
                        is_list = vrn->isComplexListConstruct();
                    } else if (auto* nch = dynamic_cast<const NewComplexHashNode*>(node)) {
                        typeInfo = nch->typeInfo;
                        is_hash = true;
                    } else if (auto* ncl = dynamic_cast<const NewComplexListNode*>(node)) {
                        typeInfo = ncl->typeInfo;
                        is_list = true;
                    }

                    uint64_t result_bits = is_hash
                        ? qore_rt_new_complex_hash_from_hash(typeInfo, toBits(init), xsink)
                        : is_list
                            ? qore_rt_new_complex_list_from_value(typeInfo, toBits(init), xsink)
                            : toBits(QoreValue());
                    out = fromBits(result_bits);
                    if (!is_hash && !is_list && xsink) {
                        xsink->raiseException("IR-EXEC-ERROR",
                            "vrn.construct with lowered operand has no complex hash/list target metadata");
                    }
                } else if (vrn_inst->vrn) {
                    uint64_t result_bits = qore_rt_vrn_construct(vrn_inst->vrn, xsink);
                    out = fromBits(result_bits);
                } else if (vrn_inst->expr.hasNode()) {
                    auto* vrn = dynamic_cast<VarRefNewObjectNode*>(
                        const_cast<AbstractQoreNode*>(vrn_inst->expr.getInternalNode()));
                    if (vrn) {
                        uint64_t result_bits = qore_rt_vrn_construct(vrn, xsink);
                        out = fromBits(result_bits);
                    } else {
                        out = evalAndRef(vrn_inst->expr, xsink);
                    }
                } else if (xsink) {
                    xsink->raiseException("IR-EXEC-ERROR",
                        "vrn.construct has neither AST node nor serialized expression");
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                setValueSlot(values, vrn_inst->result.id, out, xsink);
                if (out.hasNode()) {
                    cleanup.push_back(vrn_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::NewHashDeclFromHash: {
                auto* nhdfh_inst = static_cast<QoreIRNewHashDeclFromHashInstruction*>(inst);
                QoreValue hash_val = getIRValue(values, inst->operands[0]);
                const QoreHashNode* init = hash_val.getType() == NT_HASH
                    ? hash_val.get<const QoreHashNode>() : nullptr;
                const TypedHashDecl* hd = resolveNewHashDeclFromHashTarget(*nhdfh_inst, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                if (!hd) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                QoreHashNode* result = typed_hash_decl_private::get(*hd)->newHash(
                    init, nhdfh_inst->runtime_check, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                QoreValue out = result ? QoreValue(result) : QoreValue();
                setValueSlot(values, nhdfh_inst->result.id, out, xsink);
                if (out.hasNode()) {
                    cleanup.push_back(nhdfh_inst->result.id);
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
                bool is_ir_only_local = local_inst->slot_id < locals_ir_only.size()
                    && locals_ir_only[local_inst->slot_id];
                if (!is_ir_only_local && (local_inst->slot_id >= locals_instantiated.size()
                        || !locals_instantiated[local_inst->slot_id])) {
                    ensureLocalInstantiated(local_inst->local, instantiated_locals, instantiated_locals_ordered, pre_instantiated,
                            function_own_locals, &locally_uninstantiated);
                    if (local_inst->slot_id < locals_instantiated.size()) {
                        locals_instantiated[local_inst->slot_id] = true;
                    }
                }
                QoreIRValue operand = local_inst->operands.front();
                QoreValue val = getIRValue(values, operand);
                ValueHolder weak_eval_holder(xsink);
                bool normalized_weak_ref = !local_inst->weak
                    && normalizeWeakReferenceForAssignment(val, weak_eval_holder, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                if (is_ir_only_local && !local_inst->weak
                        && local_inst->slot_id != UINT32_MAX
                        && local_inst->slot_id < locals_slot_cache.size()) {
                    QoreValue stored = coerceIRLocalValue(local_inst->local, val, xsink);
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
                    locals_slot_cache[local_inst->slot_id].discard(xsink);
                    locals_slot_cache[local_inst->slot_id] = stored;
                    if (local_inst->slot_id < locals_instantiated.size()) {
                        // For IR-only locals, this bit also records that the slot
                        // cache contains a valid value; QoreValue() is both
                        // NOTHING and the historical cache-miss sentinel.
                        locals_instantiated[local_inst->slot_id] = true;
                    }
                    if (local_inst->slot_id < locals_lvar_cache.size()) {
                        locals_lvar_cache[local_inst->slot_id] = nullptr;
                    }
                    if (local_inst->slot_id < local_init_slots.size()) {
                        local_init_slots[local_inst->slot_id] = UINT32_MAX;
                    }
                    markParentLocalStoreDirty(local_inst);
                    ++ip;
                    break;
                }
                bool transfer_owned_operand = !normalized_weak_ref
                    && operand.isValid()
                    && operand.id < values.size()
                    && operand.id < value_use_counts.size()
                    && value_use_counts[operand.id] == 1
                    && values[operand.id].hasNode()
                    && hasCleanupEntry(cleanup, operand.id);

                // Track this slot as local-owned for DGC container scan
                if (operand.id >= 0) {
                    local_owned_slots.insert(operand.id);
                }

                if (local_inst->weak) {
                    if (local_inst->slot_id != UINT32_MAX
                            && local_inst->slot_id < locals_slot_cache.size()) {
                        locals_slot_cache[local_inst->slot_id].discard(xsink);
                        locals_slot_cache[local_inst->slot_id] = QoreValue();
                    }
                    if (local_inst->slot_id != UINT32_MAX
                            && local_inst->slot_id < locals_lvar_cache.size()) {
                        locals_lvar_cache[local_inst->slot_id] = nullptr;
                    }
                    LValueHelper helper(xsink);
                    if (local_inst->local->getLValue(helper, false, true)) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    QoreValue stored = val.hasNode() ? val.refSelf() : val;
                    helper.assign(stored, "<lvalue>", true, true);
                    if (local_inst->slot_id != UINT32_MAX
                            && local_inst->slot_id < local_init_slots.size()) {
                        local_init_slots[local_inst->slot_id] = UINT32_MAX;
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
                    if (local_inst->result.isValid()) {
                        QoreValue res = helper.getReferencedValue();
                        setValueSlot(values, local_inst->result.id, res, xsink);
                        if (res.hasNode()) {
                            cleanup.push_back(local_inst->result.id);
                        }
                    }
                    markParentLocalStoreDirty(local_inst);
                    ++ip;
                    break;
                }

                if (transfer_owned_operand) {
                    // StoreLocal is the final IR use of this owned temporary, so
                    // move the reference into the runtime local.  Leaving the slot
                    // in cleanup gives exception paths a second owner for the same
                    // storage and can also force unnecessary CoW in typed lvalues.
                    if (local_inst->slot_id != UINT32_MAX
                            && local_inst->slot_id < locals_slot_cache.size()) {
                        locals_slot_cache[local_inst->slot_id].discard(xsink);
                        locals_slot_cache[local_inst->slot_id] = QoreValue();
                    }
                    if (local_inst->slot_id != UINT32_MAX
                            && local_inst->slot_id < locals_lvar_cache.size()) {
                        locals_lvar_cache[local_inst->slot_id] = nullptr;
                    }
                    assignLocalVarValueTransfer(local_inst->local, val, xsink);
                    removeCleanupEntry(cleanup, operand.id);
                    values[operand.id] = QoreValue();
                    local_owned_slots.erase(operand.id);
                    if (local_inst->slot_id != UINT32_MAX
                            && local_inst->slot_id < local_init_slots.size()) {
                        local_init_slots[local_inst->slot_id] = UINT32_MAX;
                    }
                    if (local_inst->local
                            && QoreTypeInfo::isReference(local_inst->local->getTypeInfo())) {
                        cleanupLocalCaches();
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
                    markParentLocalStoreDirty(local_inst);
                    ++ip;
                    break;
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
                                    lvv = thread_try_find_lvar(local_inst->local);
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
                markParentLocalStoreDirty(local_inst);
                ++ip;
                break;
            }
            case QoreIROpcode::InstantiateLocal: {
                auto* linst = static_cast<QoreIRLocalInstruction*>(inst);
                if (linst->local) {
                    ensureLocalInstantiated(linst->local, instantiated_locals, instantiated_locals_ordered, pre_instantiated,
                        function_own_locals, &locally_uninstantiated);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::UninstantiateLocal: {
                auto* local_inst = static_cast<QoreIRLocalInstruction*>(inst);
                if (getenv("QORE_DEBUG_UNINST")) {
                    bool ip2 = pre_instantiated && pre_instantiated->find(local_inst->local) != pre_instantiated->end();
                    fprintf(stderr, "UNINST: %s is_closure=%d slot=%d is_pre=%d\n",
                        local_inst->local ? local_inst->local->getName() : "null",
                        local_inst->is_closure, (int)local_inst->slot_id, ip2);
                }
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
                    // Mark as null in ordered list (reverse scan finds most recent first)
                    for (auto rit = instantiated_locals_ordered.rbegin();
                            rit != instantiated_locals_ordered.rend(); ++rit) {
                        if (*rit == local_inst->local) {
                            *rit = nullptr;
                            break;
                        }
                    }
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
                    // Unlike clearLoadSlots (used during StoreLocal), this ALSO discards
                    // values in the cleanup vector because the variable is going out of
                    // scope and ALL references must be released for deterministic destruction.
                    auto cleanupLocalSlots = [&](uint32_t var_slot_id) {
                        if (var_slot_id == UINT32_MAX) {
                            return;
                        }
                        // Clean up the init slot (from StoreLocal / VarRefNewObjectNode)
                        if (var_slot_id < local_init_slots.size()
                                && local_init_slots[var_slot_id] != UINT32_MAX) {
                            uint32_t init_slot = local_init_slots[var_slot_id];
                            if (valueUsedLaterInCurrentBlock(init_slot)) {
                                if (init_slot < values.size()) {
                                    traceIRRef(inst, "uninst.init-slot", init_slot, values[init_slot], "keep");
                                }
                                // keep it alive for the future use below
                            } else if (init_slot < values.size()) {
                                traceIRRef(inst, "uninst.init-slot", init_slot, values[init_slot], "discard");
                                values[init_slot].discard(xsink);
                                values[init_slot] = QoreValue();
                                local_init_slots[var_slot_id] = UINT32_MAX;
                            }
                        }
                        // Clean up all LoadLocal result slots for this local,
                        // INCLUDING those in the cleanup vector (scope exit releases all)
                        if (var_slot_id < local_load_slots.size()) {
                            std::vector<uint32_t> kept_load_slots;
                            for (uint32_t vid : local_load_slots[var_slot_id]) {
                                if (valueUsedLaterInCurrentBlock(vid)) {
                                    traceIRRef(inst, "uninst.load-slot", vid, values[vid], "keep");
                                    kept_load_slots.push_back(vid);
                                    continue;
                                }
                                if (vid < values.size()) {
                                    traceIRRef(inst, "uninst.load-slot", vid, values[vid], "discard");
                                    values[vid].discard(xsink);
                                    values[vid] = QoreValue();
                                }
                                if (vid < load_slot_registered.size()) {
                                    load_slot_registered[vid] = false;
                                }
                            }
                            local_load_slots[var_slot_id] = std::move(kept_load_slots);
                        }
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
                            // Clear slot cache for closure-use locals
                            if (local_inst->slot_id != UINT32_MAX
                                    && local_inst->slot_id < locals_slot_cache.size()) {
                                locals_slot_cache[local_inst->slot_id].discard(xsink);
                                locals_slot_cache[local_inst->slot_id] = QoreValue();
                            }
                            // Clear init/load slots for closure vars
                            cleanupLocalSlots(local_inst->slot_id);
                            // Release closures cache entry (StoreClosure adds refSelf'd copy)
                            {
                                auto cit = closures.find(local_inst->local);
                                if (cit != closures.end()) {
                                    cit->second.discard(xsink);
                                    closures.erase(cit);
                                }
                            }
                            // Scan values[] for refs to CVV's contained value and release
                            // them BEFORE clearValue so clearValue is the final deref
                            {
                                ClosureVarValue* cvv = thread_try_find_closure_var(
                                    local_inst->local->getName());
                                if (cvv) {
                                    QoreValue cvval = cvv->eval(xsink);
                                    if (cvval.hasNode()) {
                                        const AbstractQoreNode* node_ptr = cvval.getInternalNode();
                                        for (size_t vi = 0; vi < values.size(); ++vi) {
                                            if (values[vi].hasNode()
                                                && values[vi].getInternalNode() == node_ptr) {
                                                bool used_later = valueUsedLaterInCurrentBlock(vi);
                                                if (xsink && *xsink) {
                                                    break;
                                                }
                                                if (used_later) {
                                                    traceIRRef(inst, "uninst.closure-value-scan", vi, values[vi],
                                                        "keep");
                                                    continue;
                                                }
                                                if (valueNodeEscapesAsReturn(node_ptr)) {
                                                    traceIRRef(inst, "uninst.closure-value-scan", vi, values[vi],
                                                        "keep-return-peer");
                                                    continue;
                                                }
                                                traceIRRef(inst, "uninst.closure-value-scan", vi, values[vi],
                                                    "discard");
                                                values[vi].discard(xsink);
                                                values[vi] = QoreValue();
                                            }
                                        }
                                    }
                                    cvval.discard(xsink);
                                    // Clear CVV value ONLY when refcount==1 (no outliving
                                    // closures hold the CVV). DGC cycle detection (post-p27
                                    // cvec/cmap dedupe) handles the case where closures
                                    // cyclically reference the stored value; unconditional
                                    // clearing here would corrupt captured values visible
                                    // to long-running closures (e.g. background threads).
                                    if (cvv->references.load(std::memory_order_acquire) == 1) {
                                        cvv->clearValue(xsink);
                                    }
                                }
                            }
                            local_inst->local->uninstantiate(xsink);
                        } else {
                            LocalVarValue* lvar = thread_try_find_lvar(local_inst->local);
                            if (lvar) {
                                // Release ALL value slot references to this object
                                // BEFORE lvar->del() so del() is the final deref and
                                // triggers deterministic destruction at scope exit.
                                // This handles expression results (e.g., a.next = b
                                // returns object 'a') that hold extra references in
                                // the IR values array beyond the load/init slots.
                                bool nd = false;
                                QoreValue lval = lvar->eval(nd, xsink);
                                if (lval.hasNode()) {
                                    const AbstractQoreNode* node_ptr = lval.getInternalNode();
                                    bool is_obj = (lval.getType() == NT_OBJECT);
                                    for (size_t vi = 0; vi < values.size(); ++vi) {
                                        if (values[vi].hasNode()
                                            && values[vi].getInternalNode() == node_ptr) {
                                            bool used_later = valueUsedLaterInCurrentBlock(vi);
                                            if (xsink && *xsink) {
                                                break;
                                            }
                                            if (used_later) {
                                                traceIRRef(inst, "uninst.local-value-scan", vi, values[vi], "keep");
                                                continue;
                                            }
                                            if (valueNodeEscapesAsReturn(node_ptr)) {
                                                traceIRRef(inst, "uninst.local-value-scan", vi, values[vi],
                                                    "keep-return-peer");
                                                continue;
                                            }
                                            traceIRRef(inst, "uninst.local-value-scan", vi, values[vi], "discard");
                                            values[vi].discard(xsink);
                                            values[vi] = QoreValue();
                                        }
                                    }
                                    // DGC container scan: when an object leaves block scope,
                                    // also release unowned container temporaries (lists,
                                    // hashes, other objects) that may hold transitive refs
                                    // to the object. Skip slots owned by local variables to
                                    // avoid clearing active containers (loop iterators, etc).
                                    // Also remove this variable's init slot from
                                    // local_owned_slots so its constructor result can be
                                    // cleaned up (prevents Program objects from persisting
                                    // past block scope).
                                    if (is_obj && local_inst->is_block_exit) {
                                        // Remove this variable's owned slots
                                        if (local_inst->slot_id != UINT32_MAX
                                                && local_inst->slot_id < local_init_slots.size()) {
                                            local_owned_slots.erase(local_init_slots[local_inst->slot_id]);
                                        }
                                        for (size_t vi = 0; vi < values.size(); ++vi) {
                                            if (!values[vi].hasNode()) {
                                                continue;
                                            }
                                            if (valueUsedLaterInCurrentBlock(vi)) {
                                                traceIRRef(inst, "uninst.container-scan", vi, values[vi], "keep");
                                                continue;
                                            }
                                            if (local_owned_slots.count(vi)) {
                                                traceIRRef(inst, "uninst.container-scan", vi, values[vi],
                                                    "keep-local-owned");
                                                continue;
                                            }
                                            if (needs_scan(values[vi].getInternalNode())) {
                                                traceIRRef(inst, "uninst.container-scan", vi, values[vi], "discard");
                                                values[vi].discard(xsink);
                                                values[vi] = QoreValue();
                                            }
                                        }
                                    }
                                }
                                if (nd) {
                                    lval.discard(xsink);
                                }
                                lvar->del(xsink);
                            }
                        }
                        // Destructor runs in its own scope and cannot modify our locals
                        invalidateExternalCaches();
                    } else {
                        // Non-pre-instantiated: full uninstantiate (pop + destructor)
                        // Clean up value slots (init + load) BEFORE uninstantiating
                        cleanupLocalSlots(local_inst->slot_id);
                        // Clear slot cache
                        if (local_inst->slot_id != UINT32_MAX
                                && local_inst->slot_id < locals_slot_cache.size()) {
                            locals_slot_cache[local_inst->slot_id].discard(xsink);
                            locals_slot_cache[local_inst->slot_id] = QoreValue();
                        }
                        // For closure-use vars: release all extra refs BEFORE
                        // clearValue so clearValue is the final deref that triggers
                        // the Qore destructor through the proper CVV lifecycle.
                        // Order: closures cache → values[] scan → clearValue
                        if (local_inst->is_closure) {
                            // 1. Release closures cache entry
                            auto cit = closures.find(local_inst->local);
                            if (cit != closures.end()) {
                                cit->second.discard(xsink);
                                closures.erase(cit);
                            }
                            ClosureVarValue* cvv = thread_try_find_closure_var(
                                local_inst->local->getName());
                            if (cvv) {
                                // 2. Scan values[] for refs to the CVV's contained value
                                // and discard them BEFORE clearValue.
                                {
                                    QoreValue cvval = cvv->eval(xsink);
                                    if (cvval.hasNode()) {
                                        const AbstractQoreNode* node_ptr = cvval.getInternalNode();
                                        for (size_t vi = 0; vi < values.size(); ++vi) {
                                            if (values[vi].hasNode()
                                                && values[vi].getInternalNode() == node_ptr) {
                                                bool used_later = valueUsedLaterInCurrentBlock(vi);
                                                if (xsink && *xsink) {
                                                    break;
                                                }
                                                if (used_later) {
                                                    traceIRRef(inst, "uninst.closure-cvv-scan", vi, values[vi],
                                                        "keep");
                                                    continue;
                                                }
                                                if (valueNodeEscapesAsReturn(node_ptr)) {
                                                    traceIRRef(inst, "uninst.closure-cvv-scan", vi, values[vi],
                                                        "keep-return-peer");
                                                    continue;
                                                }
                                                traceIRRef(inst, "uninst.closure-cvv-scan", vi, values[vi],
                                                    "discard");
                                                values[vi].discard(xsink);
                                                values[vi] = QoreValue();
                                            }
                                        }
                                    }
                                    cvval.discard(xsink);
                                }
                                // 3. clearValue triggers destructor when the CVV is about
                                // to be deleted. Only clear when refs==1 so outliving
                                // closures (e.g. captured on a background thread) still
                                // see the captured value; DGC handles cycle collection.
                                if (cvv->references.load(std::memory_order_acquire) == 1) {
                                    cvv->clearValue(xsink);
                                }
                            }
                        }
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
                ValueHolder weak_eval_holder(xsink);

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
                } else if (!local_inst->weak) {
                    normalizeWeakReferenceForAssignment(val, weak_eval_holder, xsink);
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                }

                // Ensure variable is instantiated before store (same as LoadClosure)
                bool has_env_cvv = thread_has_runtime_closure_env()
                    && thread_try_get_runtime_closure_var(local_inst->local);
                if (!has_env_cvv && (local_inst->slot_id >= locals_instantiated.size()
                        || !locals_instantiated[local_inst->slot_id])) {
                    ensureLocalInstantiated(local_inst->local, instantiated_locals,
                        instantiated_locals_ordered, pre_instantiated,
                        function_own_locals, &locally_uninstantiated);
                    if (local_inst->slot_id < locals_instantiated.size()) {
                        locals_instantiated[local_inst->slot_id] = true;
                    }
                }
                storeValue(closures, local_inst->local, val, xsink);
                // Write-through: update the actual closure variable so changes
                // are visible outside the IR interpreter's local cache.
                assignClosureVarValue(local_inst->local, val, xsink, local_inst->initial_assignment);
                if (local_inst->result.isValid()) {
                    QoreValue res = val.hasNode() ? val.refSelf() : val;
                    setValueSlot(values, local_inst->result.id, res, xsink);
                    if (res.hasNode()) {
                        cleanup.push_back(local_inst->result.id);
                    }
                }
                // Drop the initial ownership ref from weak node creation;
                // storeValue() and assignClosureVarValue() each took their own ref
                if (local_inst->weak && val.hasNode()
                        && (val.getType() == NT_WEAKREF
                            || val.getType() == NT_WEAKREF_HASH
                            || val.getType() == NT_WEAKREF_LIST)) {
                    val.discard(xsink);
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
                markParentSlotDirty(local_inst->slot_id);
                ++ip;
                break;
            }
            case QoreIROpcode::LoadGlobal: {
                auto* var_inst = static_cast<QoreIRVarInstruction*>(inst);
                QoreValue out;
                auto it = globals.find(var_inst->var);
                if (it != globals.end() && it->second.getType() != NT_REFERENCE) {
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
                ValueHolder weak_eval_holder(xsink);

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
                } else if (!var_inst->weak) {
                    normalizeWeakReferenceForAssignment(val, weak_eval_holder, xsink);
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                }

                storeValue(globals, var_inst->var, val, xsink);
                // Write-through: update the actual global variable so changes
                // are visible outside the IR interpreter's local cache.
                assignGlobalVarValue(var_inst->var, val, xsink);
                if (var_inst->result.isValid()) {
                    QoreValue res = val.hasNode() ? val.refSelf() : val;
                    setValueSlot(values, var_inst->result.id, res, xsink);
                    if (res.hasNode()) {
                        cleanup.push_back(var_inst->result.id);
                    }
                }
                // Drop the initial ownership ref from weak node creation;
                // storeValue() and assignGlobalVarValue() each took their own ref
                if (var_inst->weak && val.hasNode()
                        && (val.getType() == NT_WEAKREF
                            || val.getType() == NT_WEAKREF_HASH
                            || val.getType() == NT_WEAKREF_LIST)) {
                    val.discard(xsink);
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
            case QoreIROpcode::LoadThreadLocal: {
                auto* var_inst = static_cast<QoreIRVarInstruction*>(inst);
                QoreValue out;
                auto it = threadlocals.find(var_inst->var);
                if (it != threadlocals.end() && it->second.getType() != NT_REFERENCE) {
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
                ValueHolder weak_eval_holder(xsink);

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
                } else if (!var_inst->weak) {
                    normalizeWeakReferenceForAssignment(val, weak_eval_holder, xsink);
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                }

                storeValue(threadlocals, var_inst->var, val, xsink);
                // Write-through: update the actual thread-local variable.
                assignGlobalVarValue(var_inst->var, val, xsink);
                if (var_inst->result.isValid()) {
                    QoreValue res = val.hasNode() ? val.refSelf() : val;
                    setValueSlot(values, var_inst->result.id, res, xsink);
                    if (res.hasNode()) {
                        cleanup.push_back(var_inst->result.id);
                    }
                }
                // Drop the initial ownership ref from weak node creation;
                // storeValue() and assignGlobalVarValue() each took their own ref
                if (var_inst->weak && val.hasNode()
                        && (val.getType() == NT_WEAKREF
                            || val.getType() == NT_WEAKREF_HASH
                            || val.getType() == NT_WEAKREF_LIST)) {
                    val.discard(xsink);
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
                // Native context init: push a Context frame and return its pointer.
                auto* context_inst = static_cast<QoreIRContextInstruction*>(inst);
                const char* name_cstr = context_inst->name.empty()
                    ? nullptr : context_inst->name.c_str();
                uint64_t state = qore_rt_context_init(name_cstr,
                    toBits(context_inst->exp),
                    toBits(context_inst->where_exp),
                    toBits(context_inst->sort_exp),
                    context_inst->sort_type, xsink);
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
            case QoreIROpcode::ContextRef: {
                auto* cri = static_cast<QoreIRContextRefInstruction*>(inst);
                QoreValue result = fromBits(qore_rt_context_ref_at(
                    cri->key.c_str(), cri->stack_offset, xsink));
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
                setValueSlot(values, inst->result.id, result, xsink);
                if (result.hasNode()) {
                    cleanup.push_back(inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::ContextRow: {
                QoreValue result = fromBits(qore_rt_context_row(xsink));
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
                setValueSlot(values, inst->result.id, result, xsink);
                if (result.hasNode()) {
                    cleanup.push_back(inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::ContextMaxPos: {
                QoreValue state_val = getIRValue(values, inst->operands[0]);
                int64_t max_pos = qore_rt_context_max_pos(
                    static_cast<uint64_t>(state_val.getAsBigInt()));
                setValueSlotDirect(values, inst->result.id, QoreValue(max_pos));
                ++ip;
                break;
            }
            case QoreIROpcode::ContextSetPos: {
                QoreValue state_val = getIRValue(values, inst->operands[0]);
                QoreValue idx_val = getIRValue(values, inst->operands[1]);
                qore_rt_context_set_pos(
                    static_cast<uint64_t>(state_val.getAsBigInt()),
                    idx_val.getAsBigInt());
                ++ip;
                break;
            }
            case QoreIROpcode::ContextDestroy: {
                QoreValue state_val = getIRValue(values, inst->operands[0]);
                qore_rt_context_destroy(
                    static_cast<uint64_t>(state_val.getAsBigInt()), xsink);
                ++ip;
                break;
            }
            case QoreIROpcode::Backquote: {
                auto* backquote_inst = static_cast<QoreIRBackquoteInstruction*>(inst);
                int rc = 0;
                QoreStringNode* str = backquoteEval(backquote_inst->command.c_str(), rc, xsink);
                QoreValue result = str ? QoreValue(str) : QoreValue();
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
                setValueSlot(values, inst->result.id, result, xsink);
                if (result.hasNode()) {
                    cleanup.push_back(inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::Find: {
                auto* find_inst = static_cast<QoreIRFindInstruction*>(inst);
                QoreValue result = fromBits(qore_rt_find_mode(toBits(find_inst->exp),
                    toBits(find_inst->find_exp), toBits(find_inst->where), find_inst->mode, xsink));
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
                setValueSlot(values, inst->result.id, result, xsink);
                if (result.hasNode()) {
                    cleanup.push_back(inst->result.id);
                }
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
            case QoreIROpcode::IterateValue: {
                QoreValue source = getIRValue(values, inst->operands[0]);
                QoreValue res = QoreIterateOperatorNode::evalIteratorValue(source,
                    QoreIterateOperatorNode::getElementTypeInfo(source, source.getTypeInfo()), xsink);
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
            case QoreIROpcode::IteratorCreate:
            case QoreIROpcode::IteratorCreateIterate: {
                auto* iter_inst = static_cast<QoreIRIteratorCreateInstruction*>(inst);
                QoreValue iterable = getIRValue(values, iter_inst->iterable);
                FunctionalOperator::FunctionalValueType value_type;
                FunctionalOperatorInterface* iter = nullptr;
                if (iter_inst->iterator_func) {
                    iter = iter_inst->iterator_func->getFunctionalIterator(value_type, xsink);
                } else if (inst->opcode == QoreIROpcode::IteratorCreateIterate) {
                    iter = QoreIterateOperatorNode::getFunctionalIterator(value_type, iterable, nullptr,
                        "streaming operator expression", xsink);
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
                    executeOnBlockExitHandlers(on_block_exit_handlers, xsink, &locals_slot_cache);
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                ++ip;
                break;
            }
            case QoreIROpcode::DebugBlock: {
                if (debug_active && !(xsink && *xsink)) {
                    const StatementBlock* dbg_block = nullptr;
                    if (AbstractStatement* dbg_stmt = getDebugStatement(inst)) {
                        dbg_block = dynamic_cast<const StatementBlock*>(dbg_stmt);
                    }
                    if (!dbg_block) {
                        dbg_block = statements;
                    }
                    int dbg_rc = tlpd->dbgSyntheticBlockStep(dbg_block, xsink);
                    if (dbg_rc || *xsink) {
                        if (dbg_rc == RC_RETURN || *xsink) {
                            if (debug_active) {
                                tlpd->dbgFunctionExit(statements, return_value, xsink);
                            }
                            executeOnBlockExitHandlers(on_block_exit_handlers, xsink, &locals_slot_cache);
                            cleanupValues(values, cleanup, xsink, true, cleanup_log);
                            cleanupLocalCaches();
                            return dbg_rc == RC_RETURN;
                        }
                        if (dbg_rc == RC_BREAK) {
                            // See the dbgStep RC_BREAK handler above for rationale.
                            if (current_block->enclosing_loop_exit) {
                                debug_break_target = current_block->enclosing_loop_exit;
                            } else {
                                debug_break_loop = true;
                            }
                        }
                    }
                }
                ++ip;
                break;
            }
            case QoreIROpcode::CheckException: {
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
            case QoreIROpcode::ScopeEnter: {
                // Record current handler list size so ScopeExit knows which handlers to execute
                scope_stack.push_back(on_block_exit_handlers.size());
                ++ip;
                break;
            }
            case QoreIROpcode::PushTempMark: {
                // Push sentinel marking the start of the current statement's
                // temp cleanup region.  UINT32_MAX is not a valid values[] index
                // (cleanup entries store instruction result slot ids bounded by
                // the per-function value pool size), and cleanupValues already
                // skips such out-of-range ids safely on exception unwind.
                cleanup.push_back(UINT32_MAX);
                ++ip;
                break;
            }
            case QoreIROpcode::CallAOTHelper: {
                // CallAOTHelper is emitted only by the AOT init-expression
                // outlining pass (Phase 1.5).  The IR interpreter should
                // never encounter it — outlined IR is LLVM-lowered only.
                if (xsink) {
                    xsink->raiseException("IR-INTERPRETER-ERROR",
                            "unexpected CallAOTHelper in interpreter path "
                            "(AOT-only opcode)");
                }
                return false;
            }
            case QoreIROpcode::DiscardTemps: {
                // Drain cleanup back to (and including) the nearest PushTempMark
                // sentinel so expression temps are destructed at end of
                // statement — matching AST-mode ValueEvalRefHolder destructor
                // timing.  Stops at the sentinel to preserve OUTER-scope temps
                // (e.g. a foreach list expression's iterator temp, which must
                // outlive the loop body).  Uses xsink (no_throw=false) so
                // destructor-raised exceptions propagate.  If no sentinel is
                // present (malformed IR), falls back to a full drain rather
                // than looping forever.
                while (!cleanup.empty()) {
                    uint32_t id = cleanup.back();
                    cleanup.pop_back();
                    if (id == UINT32_MAX) {
                        break;  // matching mark popped
                    }
                    if (id < values.size()) {
                        QoreValue& slot = values[id];
                        if (cleanup_log && slot.hasNode()) {
                            if (slot.getType() == NT_STRING) {
                                QoreStringValueHelper str(slot);
                                cleanup_log->push_back(str->getBuffer());
                            } else {
                                cleanup_log->push_back(slot.getTypeName());
                            }
                        }
                        slot.discard(xsink);
                        slot = QoreValue();
                    }
                }
                while (!weak_load_temp_slots.empty()) {
                    uint32_t id = weak_load_temp_slots.begin()->first;
                    releaseWeakLoadTemp(id);
                }
                // Strong refs produced by self/static weak-reference evaluation
                // are normal statement temporaries; the cleanup stack above owns
                // their lifetime.  Clear bookkeeping after statement cleanup
                // without tying lifetime to source lines, because a single
                // expression can span multiple lines.
                ephemeral_weak_ref_slots.clear();
                ++ip;
                break;
            }
            case QoreIROpcode::ScopeExit: {
                auto* scope_inst = static_cast<QoreIRScopeExitInstruction*>(inst);
                // Execute handlers registered since matching ScopeEnter (only if not inline-lowered)
                if (!scope_stack.empty()) {
                    size_t scope_start = scope_stack.back();
                    scope_stack.pop_back();
                    if (on_block_exit_handlers.size() <= scope_start) {
                        ++ip;
                        break;
                    }
                    // Skip handler execution if inline_lowered=true; just flush the vector
                    if (!scope_inst->inline_lowered) {
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
                                    executeHandlerBody(on_block_exit_handlers[i - 1], &obe_xsink, &locals_slot_cache);
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
                        invalidateExternalCaches();
                    }
                    // Always remove executed handlers (or all handlers if inline_lowered=true)
                    on_block_exit_handlers.resize(scope_start);
                    // Handler execution (both AST and compiled IR) can modify globals,
                    // threadlocals, closures, and non-IR-only locals through the TLS
                    // variable stack.  IR-only locals exist only in the slot cache and
                    // are unreachable by handlers, so they stay valid.
                    if (!scope_inst->inline_lowered) {
                        invalidateExternalCaches();
                        // If handler execution raised an exception, route to the
                        // ScopeExit instruction's exception_target (try/catch landing pad)
                        if (xsink && *xsink && inst->exception_target) {
                            prev_block = block;
                            block = inst->exception_target;
                            ip = 0;
                            break;
                        }
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

                // Verbose logging for guard failures (zero-cost when disabled)
                static bool ir_guard_verbose = (getenv("QORE_IR_DEBUG") != nullptr);
                if (ir_guard_verbose && func.name.find("monitor") != std::string::npos) {
                    uint32_t source_slot = guard_inst->operands.front().id;
                    fprintf(stderr, "[IR-GUARD-CHECK] func='%s' ip=%zu guard_op=%d value_slot=%u val_type='%s' val_is_nothing=%d\n",
                            func.name.c_str(), ip, (int)inst->opcode, source_slot,
                            value.getTypeName(), (int)value.isNothing());
                    fflush(stderr);
                }

                // Record type profile for this guard point
                if (guard_inst->guard_id < func.guard_profile_count) {
                    func.guard_profiles[guard_inst->guard_id].record(value);
                }
                if (!guardPredicate(inst->opcode, value, guard_inst->type_info)) {
                    if (guard_inst->deopt_target && !suppress_guard_deopt) {
                        // Guard type check failed — fall back to AST execution.
                        // Don't raise an exception; returning false without setting
                        // xsink tells evalTiered() to re-execute via AST.
                        const char* guard_type = "Unknown";
                        switch (inst->opcode) {
                            case QoreIROpcode::GuardInt: guard_type = "GuardInt"; break;
                            case QoreIROpcode::GuardFloat: guard_type = "GuardFloat"; break;
                            case QoreIROpcode::GuardType: guard_type = "GuardType"; break;
                            case QoreIROpcode::GuardNotNothing: guard_type = "GuardNotNothing"; break;
                            default: break;
                        }
                        printd(2, "QoreIRInterpreter::execute() guard failed (%s) for '%s' "
                            "— falling back to AST\n", guard_type, func.name.c_str());
                        static bool debug_guard = [] {
                            const char* debug_env = getenv("QORE_IR_DEBUG");
                            return debug_env && strstr(debug_env, "guard");
                        }();
                        if (debug_guard) {
                            fprintf(stderr, "[IR-GUARD-FAILED] execute() returning false (deopt) for '%s'\n",
                                    func.name.c_str());
                            fprintf(stderr, "                   Guard: %s, Value: %s\n", guard_type, value.getTypeName());
                            if (guard_inst->type_info) {
                                fprintf(stderr, "                   Expected: %s\n", QoreTypeInfo::getName(guard_inst->type_info));
                            }
                            fflush(stderr);
                        } else {
                            fprintf(stderr, "[IR-EXEC] execute() returning false (IR deopt triggered) for '%s' (guard: %s, value: %s)\n",
                                    func.name.c_str(), guard_type, value.getTypeName());
                            if (guard_inst->type_info) {
                                fprintf(stderr, "           expected type: %s\n", QoreTypeInfo::getName(guard_inst->type_info));
                            }
                            fflush(stderr);
                        }
                        // Fire on_block_exit handlers before cleanup — guards may fire
                        // after side-effecting code (e.g., Mutex::lock() + on_exit
                        // Mutex::unlock()); without this, on_exit handlers are orphaned
                        // and resources like mutexes remain locked.
                        executeOnBlockExitHandlers(on_block_exit_handlers, xsink, &locals_slot_cache);
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
            case QoreIROpcode::ToInt:
            case QoreIROpcode::ToFloat:
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
                    case NT_STRING: {
                        QoreStringNodeValueHelper val_str(val);
                        str = val_str.getReferencedValue();
                        break;
                    }
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
            case QoreIROpcode::AddTimeout:
            case QoreIROpcode::AddString:
            case QoreIROpcode::SubInt:
            case QoreIROpcode::SubFloat:
            case QoreIROpcode::SubAny:
            case QoreIROpcode::SubTimeout:
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
            case QoreIROpcode::PluginUnary:
            case QoreIROpcode::PluginBinary:
            case QoreIROpcode::PluginSubscript:
            case QoreIROpcode::PluginCall:
            case QoreIROpcode::PluginConstruct:
            case QoreIROpcode::PluginDenseBufferUnary:
            case QoreIROpcode::PluginDenseBufferBinary: {
                auto* plugin_inst = static_cast<QoreIRPluginInstruction*>(inst);
                uint32_t global_id = resolvePluginOperationId(*plugin_inst, xsink);
                if (!global_id || (xsink && *xsink)) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }

                uint64_t result_bits = toBits(QoreValue());
                QoreValue args_value;
                if (inst->opcode == QoreIROpcode::PluginUnary) {
                    QoreValue value = getIRValue(values, inst->operands[0]);
                    result_bits = qore_rt_plugin_unary(global_id, toBits(value), xsink);
                } else if (inst->opcode == QoreIROpcode::PluginBinary) {
                    QoreValue lhs = getIRValue(values, inst->operands[0]);
                    QoreValue rhs = getIRValue(values, inst->operands[1]);
                    result_bits = qore_rt_plugin_binary(global_id, toBits(lhs), toBits(rhs), xsink);
                } else if (inst->opcode == QoreIROpcode::PluginSubscript) {
                    QoreValue container = getIRValue(values, inst->operands[0]);
                    QoreValue key = getIRValue(values, inst->operands[1]);
                    result_bits = qore_rt_plugin_subscript(global_id, toBits(container), toBits(key), xsink);
                } else if (inst->opcode == QoreIROpcode::PluginCall) {
                    QoreValue self = getIRValue(values, inst->operands[0]);
                    args_value = makePluginArgsList(inst->operands, 1, values, xsink);
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    result_bits = qore_rt_plugin_call(global_id, toBits(self), toBits(args_value), xsink);
                } else if (inst->opcode == QoreIROpcode::PluginConstruct) {
                    args_value = makePluginArgsList(inst->operands, 0, values, xsink);
                    if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    result_bits = qore_rt_plugin_construct(global_id, toBits(args_value), xsink);
                } else if (inst->opcode == QoreIROpcode::PluginDenseBufferUnary) {
                    QoreValue result_buffer = getIRValue(values, inst->operands[0]);
                    QoreValue value = getIRValue(values, inst->operands[1]);
                    result_bits = qore_rt_plugin_dense_buffer_unary_values(global_id, toBits(result_buffer),
                        toBits(value), xsink);
                } else {
                    QoreValue result_buffer = getIRValue(values, inst->operands[0]);
                    QoreValue lhs = getIRValue(values, inst->operands[1]);
                    QoreValue rhs = getIRValue(values, inst->operands[2]);
                    result_bits = qore_rt_plugin_dense_buffer_binary_values(global_id, toBits(result_buffer),
                        toBits(lhs), toBits(rhs), xsink);
                }

                args_value.discard(xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                QoreValue res = fromBits(result_bits);
                setValueSlot(values, inst->result.id, res, xsink);
                if (res.hasNode()) {
                    cleanup.push_back(inst->result.id);
                }
                ++ip;
                break;
            }
            // Container access with pre-evaluated operands: use evalBinary
            case QoreIROpcode::HashDerefDynamic:
            case QoreIROpcode::ListIndexDynamic: {
                if (inst->operands.size() < 2) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "container access op missing operands");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                QoreValue left_val = getIRValue(values, inst->operands[0]);
                auto* expr_inst = static_cast<QoreIRExprInstruction*>(inst);
                int32_t string_index_char = 1;
                if (inst->opcode == QoreIROpcode::ListIndexDynamic && expr_inst->expr.hasNode()) {
                    if (auto* sq = dynamic_cast<const QoreSquareBracketsOperatorNode*>(
                            expr_inst->expr.getInternalNode())) {
                        string_index_char = sq->hasStringIndexChar() ? 1 : 0;
                    }
                }
                if (inst->opcode == QoreIROpcode::ListIndexDynamic
                        && !expr_inst->list_selector_kinds.empty()) {
                    std::vector<uint64_t> selector_bits;
                    selector_bits.reserve(inst->operands.size() - 1);
                    for (size_t i = 1; i < inst->operands.size(); ++i) {
                        selector_bits.push_back(toBits(getIRValue(values, inst->operands[i])));
                    }
                    QoreValue res = fromBits(qore_rt_list_index_selectors(toBits(left_val),
                        expr_inst->list_selector_kinds.data(),
                        static_cast<int32_t>(expr_inst->list_selector_kinds.size()),
                        selector_bits.data(), string_index_char, xsink));
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
                QoreValue right_val = getIRValue(values, inst->operands[1]);
                QoreValue res = inst->opcode == QoreIROpcode::ListIndexDynamic
                    ? QoreSquareBracketsOperatorNode::doSquareBrackets(left_val, right_val, true,
                        string_index_char != 0, runtime_check_parse_option(PO_NEGATIVE_OFFSETS), xsink)
                    : QoreIRInterpreter::evalBinary(inst->opcode, left_val, right_val, xsink);
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
                    res = raiseIRAstFallback(xsink, "expr", &func, block, ip, inst, inst->opcode,
                        expr_inst->expr);
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
                    res = raiseIRAstFallback(xsink, "expr", &func, block, ip, inst, inst->opcode,
                        expr_inst->expr);
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
                updateLocalVarFromLvalue(instantiated_locals, instantiated_locals_ordered, lval_inst->lvalue, res, xsink, pre_instantiated, function_own_locals, &locally_uninstantiated,
                    &func.local_var_slots, &locals_slot_cache);
                markParentLValueDirty(lval_inst, extractLValueBaseVarRef(lval_inst->lvalue));
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
                    if ((type == VT_LOCAL || type == VT_LOCAL_TS || type == VT_CLOSURE) && base_var->ref.id) {
                        ensureLocalInstantiated(base_var->ref.id, instantiated_locals, instantiated_locals_ordered, pre_instantiated,
                                function_own_locals, &locally_uninstantiated);
                    }
                    invalidateClosureCache(base_var);
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
                markParentLValueDirty(lval_inst, base_var);
                ++ip;
                break;
            }
            case QoreIROpcode::LValuePathAssign: {
                auto* path_inst = static_cast<QoreIRLValuePathInstruction*>(inst);
                if (path_inst->operands.empty() || path_inst->path.empty()) {
                    xsink->raiseException("IR-EXEC-ERROR", "lvalue.path.assign missing operand or path");
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }

                // Per-invocation private path copy; see patchLVPathLocal for why
                // mutating path_inst->path directly is a data race.
                std::vector<LVPathStep> path_copy = patchLVPathLocal(path_inst);
                ensureLValuePathRootLocal(path_inst);
                invalidateLValuePathClosureCache(path_inst);

                QoreValue val = getIRValue(values, path_inst->operands[0]);
                ValueHolder val_holder(val.refSelf(), xsink);
                QoreValue assign_val = val;
                ValueHolder eval_holder(xsink);
                qore_type_t val_type = val.getType();
                if (!path_inst->weak
                        && (val_type == NT_WEAKREF || val_type == NT_WEAKREF_HASH || val_type == NT_WEAKREF_LIST)) {
                    eval_holder = val.eval(xsink);
                    if (*xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    assign_val = *eval_holder;
                }

                // Scope the LValueHelper so it releases the object lock
                // BEFORE cache invalidation (which may deref objects and
                // try to acquire the same lock for GC scanning).
                QoreValue res;
                {
                    LValueHelper lvh(xsink);
                    if (lvh.navigatePath(path_copy.data(), path_copy.size(), false)) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    if (lvh.assign(assign_val.refSelf(), "<lvalue path assign>", true, path_inst->weak)) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    res = lvh.getReferencedValue();
                }
                // lvh is now destructed — object lock released
                invalidateLValuePathClosureCache(path_inst);

                // Cache invalidation: broad for reference roots (write-through can modify
                // any variable), targeted for non-reference roots.
                if (path_inst->hasLocalTarget()) {
                    markParentLValuePathDirty(path_inst);
                    bool is_ref = !path_inst->path.empty() && path_inst->path[0].type_info
                        && QoreTypeInfo::isReference(path_inst->path[0].type_info);
                    if (is_ref) {
                        for (size_t j = 0; j < locals_slot_cache.size(); ++j) {
                            if (preserveParentSlotForWriteback(j)) {
                                continue;
                            }
                            if (j < locals_ir_only.size() && locals_ir_only[j]) {
                                continue;
                            }
                            locals_slot_cache[j].discard(xsink);
                            locals_slot_cache[j] = QoreValue();
                        }
                        cleanupStoredValues(closures, xsink);
                        cleanupStoredValues(globals, xsink);
                    } else {
                        if (path_inst->lvalue_slot_id < locals_slot_cache.size()) {
                            locals_slot_cache[path_inst->lvalue_slot_id].discard(xsink);
                            locals_slot_cache[path_inst->lvalue_slot_id] = QoreValue();
                        }
                        clearLoadSlots(path_inst->lvalue_slot_id);
                    }
                }

                if (path_inst->result.isValid()) {
                    setValueSlot(values, path_inst->result.id, res, xsink);
                    if (res.hasNode()) {
                        cleanup.push_back(path_inst->result.id);
                    }
                }
                ++ip;
                break;
            }
            case QoreIROpcode::LValuePathCompound: {
                auto* path_inst = static_cast<QoreIRLValuePathInstruction*>(inst);
                if (path_inst->operands.empty() || path_inst->path.empty()) {
                    xsink->raiseException("IR-EXEC-ERROR", "lvalue.path.compound missing operand or path");
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                // Per-invocation private path copy; see patchLVPathLocal for why
                // mutating path_inst->path directly is a data race.
                std::vector<LVPathStep> path_copy = patchLVPathLocal(path_inst);
                ensureLValuePathRootLocal(path_inst);
                invalidateLValuePathClosureCache(path_inst);
                QoreValue rhs = getIRValue(values, path_inst->operands[0]);
                ValueHolder rhs_holder(rhs.refSelf(), xsink);
                QoreValue res;
                // Scope the LValueHelper so it releases the object lock
                // BEFORE cache invalidation (which may deref objects and
                // try to acquire the same lock for GC scanning).
                {
                    LValueHelper lvh(xsink);
                    if (lvh.navigatePath(path_copy.data(), path_copy.size(), false)) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    switch (path_inst->compound_op) {
                        case LVCompoundOp::AddAssign:
                            res = doPlusEqualsOnLValue(lvh, rhs, xsink);
                            break;
                        case LVCompoundOp::SubAssign:
                            res = doMinusEqualsOnLValue(lvh, rhs, xsink);
                            break;
                        default: {
                            qore_type_t vtype = lvh.getType();
                            if (vtype == NT_NUMBER || rhs.getType() == NT_NUMBER) {
                                switch (path_inst->compound_op) {
                                    case LVCompoundOp::MulAssign: lvh.multiplyEqualsNumber(rhs); break;
                                    case LVCompoundOp::DivAssign: lvh.divideEqualsNumber(rhs); break;
                                    default: res = QoreValue(); break;
                                }
                            } else if (vtype == NT_FLOAT || rhs.getType() == NT_FLOAT) {
                                double rv = rhs.getAsFloat();
                                switch (path_inst->compound_op) {
                                    case LVCompoundOp::MulAssign: res = lvh.multiplyEqualsFloat(rv); break;
                                    case LVCompoundOp::DivAssign: res = lvh.divideEqualsFloat(rv); break;
                                    default: res = QoreValue(); break;
                                }
                            } else {
                                int64 rv = rhs.getAsBigInt();
                                switch (path_inst->compound_op) {
                                    case LVCompoundOp::MulAssign: res = lvh.multiplyEqualsBigInt(rv); break;
                                    case LVCompoundOp::DivAssign: res = lvh.divideEqualsBigInt(rv); break;
                                    case LVCompoundOp::ModAssign: res = lvh.modulaEqualsBigInt(rv); break;
                                    case LVCompoundOp::AndAssign: res = lvh.andEqualsBigInt(rv); break;
                                    case LVCompoundOp::OrAssign: res = lvh.orEqualsBigInt(rv); break;
                                    case LVCompoundOp::XorAssign: res = lvh.xorEqualsBigInt(rv); break;
                                    case LVCompoundOp::ShlAssign: res = lvh.shiftLeftEqualsBigInt(rv); break;
                                    case LVCompoundOp::ShrAssign: res = lvh.shiftRightEqualsBigInt(rv); break;
                                    default: break;
                                }
                            }
                            break;
                        }
                    }
                }
                // lvh is now destructed — object lock released
                invalidateLValuePathClosureCache(path_inst);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                // Cache invalidation: broad for reference roots, targeted for others
                if (path_inst->hasLocalTarget()) {
                    markParentLValuePathDirty(path_inst);
                    bool is_ref = !path_inst->path.empty() && path_inst->path[0].type_info
                        && QoreTypeInfo::isReference(path_inst->path[0].type_info);
                    if (is_ref) {
                        for (size_t j = 0; j < locals_slot_cache.size(); ++j) {
                            if (preserveParentSlotForWriteback(j)) {
                                continue;
                            }
                            if (j < locals_ir_only.size() && locals_ir_only[j]) {
                                continue;
                            }
                            locals_slot_cache[j].discard(xsink);
                            locals_slot_cache[j] = QoreValue();
                        }
                        // Also clear closures and globals caches for reference write-through
                        cleanupStoredValues(closures, xsink);
                        cleanupStoredValues(globals, xsink);
                    } else {
                        if (path_inst->lvalue_slot_id < locals_slot_cache.size()) {
                            locals_slot_cache[path_inst->lvalue_slot_id].discard(xsink);
                            locals_slot_cache[path_inst->lvalue_slot_id] = QoreValue();
                        }
                        clearLoadSlots(path_inst->lvalue_slot_id);
                    }
                }
                if (path_inst->result.isValid()) {
                    setValueSlot(values, path_inst->result.id, res, xsink);
                    if (res.hasNode()) {
                        cleanup.push_back(path_inst->result.id);
                    }
                } else {
                    res.discard(xsink);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::LValuePathUnary: {
                auto* path_inst = static_cast<QoreIRLValuePathInstruction*>(inst);
                if (path_inst->path.empty()) {
                    xsink->raiseException("IR-EXEC-ERROR", "lvalue.path.unary missing path");
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                // Per-invocation private path copy; see patchLVPathLocal for why
                // mutating path_inst->path directly is a data race.  This is the
                // site where the race was first observed — under concurrent HTTP/2
                // async-I/O load, the `remove rv.hdr{"Connection", ...}` call in
                // HttpServerUtil::http_set_reply_headers stomped slice_values
                // between threads, corrupting the string pointers and SEGV'ing in
                // QoreStringValueHelper::setup on the first (dangling) key_val.
                std::vector<LVPathStep> path_copy = patchLVPathLocal(path_inst);
                ensureLValuePathRootLocal(path_inst);
                invalidateLValuePathClosureCache(path_inst);
                QoreValue res;
                bool is_remove = (path_inst->unary_op == LVUnaryOp::Remove
                                || path_inst->unary_op == LVUnaryOp::Delete);
                auto finish_delete_result = [&](QoreValue& value) -> bool {
                    if (path_inst->unary_op != LVUnaryOp::Delete) {
                        return true;
                    }
                    if (value.getType() == NT_OBJECT) {
                        QoreObject* o = value.get<QoreObject>();
                        if (o->isSystemObject()) {
                            xsink->raiseException("SYSTEM-OBJECT-ERROR",
                                "cannot delete a system constant object (class '%s')", o->getClassName());
                        } else {
                            o->doDelete(xsink);
                        }
                    }
                    value.discard(xsink);
                    value = QoreValue();
                    return !*xsink;
                };
                if (is_remove && path_copy.size() == 1
                        && path_copy[0].kind == LVPathStepKind::SelfMember) {
                    QoreObject* obj = runtime_get_stack_object();
                    if (!obj) {
                        xsink->raiseException("LVALUE-ERROR",
                            "no object context for self member remove");
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    if (qore_ir_check_closure_self_valid(obj, xsink)) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    res = qore_object_private::takeMember(*obj, xsink,
                        path_copy[0].name.c_str(), false);
                    if ((xsink && *xsink) || !finish_delete_result(res)) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    goto lvalue_path_unary_done;
                }
                if (is_remove && path_inst->path.size() >= 2) {
                    // For multi-step paths, navigate to the PARENT container, then
                    // remove/delete the key/element. LValueHelper::remove() only clears
                    // the value without removing the hash key; we need container-level removal.
                    LValueHelper lvh(xsink);
                    // Navigate to parent (for_remove=true: don't vivify intermediates)
                    if (!lvh.navigatePath(path_copy.data(), path_copy.size() - 1, true)) {
                        // Now remove/delete the final key/element from the container
                        const LVPathStep& last_step = path_copy.back();
                        QoreValue container = lvh.getValue();
                        qore_type_t ct = container.getType();
                        bool last_is_hash = (last_step.kind == LVPathStepKind::HashKeyConst
                                || last_step.kind == LVPathStepKind::HashKey);
                        bool last_is_hash_list = last_is_hash && last_step.slice_values.size() == 1
                                && last_step.slice_values[0].getType() == NT_LIST;
                        if ((last_step.kind == LVPathStepKind::HashKeySlice || last_is_hash_list)
                                && (ct == NT_HASH || ct == NT_OBJECT || ct == NT_WEAKREF)) {
                            res = executeLVHashKeySliceRemove(lvh, ct, last_step,
                                    path_inst->unary_op, xsink);
                        } else if ((last_step.kind == LVPathStepKind::HashKeyConst
                                || last_step.kind == LVPathStepKind::HashKey) && ct == NT_HASH) {
                            lvh.ensureUnique();
                            QoreHashNode* h = lvh.getValue().get<QoreHashNode>();
                            res = h->takeKeyValue(last_step.name.c_str());
                            finish_delete_result(res);
                        } else if ((last_step.kind == LVPathStepKind::HashKeyConst
                                || last_step.kind == LVPathStepKind::HashKey)
                                && (ct == NT_OBJECT || ct == NT_WEAKREF)) {
                            QoreObject* o = ct == NT_OBJECT
                                ? lvh.getValue().get<QoreObject>()
                                : lvh.getValue().get<const WeakReferenceNode>()->get();
                            if (o) {
                                res = qore_object_private::takeMember(*o, lvh, last_step.name.c_str());
                                finish_delete_result(res);
                            }
                        } else if (last_step.kind == LVPathStepKind::ListIndex && ct == NT_LIST) {
                            lvh.ensureUnique();
                            QoreListNode* l = lvh.getValue().get<QoreListNode>();
                            int64_t idx = last_step.index;
                            if (runtime_check_parse_option(PO_NEGATIVE_OFFSETS) && idx < 0) {
                                idx += static_cast<int64_t>(l->size());
                            }
                            if (idx >= 0 && static_cast<size_t>(idx) < l->size()) {
                                if (path_inst->unary_op == LVUnaryOp::Remove) {
                                    res = l->retrieveEntry(static_cast<size_t>(idx)).refSelf();
                                }
                                l->setEntry(static_cast<size_t>(idx), QoreValue(), xsink);
                            }
                        } else if (last_step.kind == LVPathStepKind::HashKeySlice
                                && (ct == NT_HASH || ct == NT_OBJECT || ct == NT_WEAKREF)) {
                            res = executeLVHashKeySliceRemove(lvh, ct, last_step,
                                    path_inst->unary_op, xsink);
                        } else if (last_step.kind == LVPathStepKind::ListIndexSlice
                                && (ct == NT_LIST || ct == NT_STRING || ct == NT_BINARY)) {
                            res = executeLVListIndexSliceRemove(lvh, ct, last_step,
                                    path_inst->unary_op, xsink);
                        } else if (last_step.kind == LVPathStepKind::ListRangeSlice
                                && (ct == NT_LIST || ct == NT_STRING || ct == NT_BINARY)) {
                            res = executeLVListRangeSliceRemove(lvh, ct, last_step,
                                    path_inst->unary_op, xsink);
                        }
                        // NT_NOTHING or other parent types: nothing to remove, fall through
                        if (xsink && *xsink) {
                            cleanupValues(values, cleanup, xsink, true, cleanup_log);
                            cleanupLocalCaches();
                            return false;
                        }
                    } else if (xsink && *xsink) {
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    // navigatePath failed without exception: parent doesn't exist, skip
                } else if (is_remove) {
                    // Single-step path: navigate to the variable itself and clear.
                    //
                    // Special case: if the variable holds a ReferenceNode (e.g.
                    // `reference<hash> r = \h.b; delete r;`), delegate to the AST
                    // LValueRemoveHelper on the reference — it dispatches through the
                    // reference's vexp to the correct container-level removal (hash
                    // takeKeyValue / list setEntry), matching AST semantics. Bare
                    // lvh.remove() on a reference-target only clears the slot and leaves
                    // the hash key behind; AST fallback here would trigger a spurious
                    // re-run of the function body (execute() returning false without an
                    // exception).
                    if (path_inst->path.size() == 1
                        && (path_inst->path[0].kind == LVPathStepKind::LocalVar
                            || path_inst->path[0].kind == LVPathStepKind::ClosureVar)) {
                        const LocalVar* lv = static_cast<const LocalVar*>(path_inst->path[0].ref_ptr);
                        // Peek at the raw slot value (LocalVarValue / ClosureVarValue) to
                        // detect a ReferenceNode — lv->eval() would dereference through the
                        // reference's vexp and hide the NT_REFERENCE marker. Use the variable's
                        // closure_use flag (NOT the LVPath step kind) to pick the right stack,
                        // since for VT_LOCAL_TS the path kind is LocalVar but the CVV lives on
                        // cvstack; thread_find_lvar would walk past the lvstack root and crash.
                        ReferenceNode* ref = nullptr;
                        if (lv) {
                            if (!lv->closureUse()) {
                                LocalVarValue* lvv = thread_try_find_lvar(lv);
                                if (lvv && lvv->val.getType() == NT_REFERENCE) {
                                    ref = reinterpret_cast<ReferenceNode*>(lvv->val.v.n);
                                }
                            } else {
                                ClosureVarValue* cvv = resolve_closure_var_value(lv);
                                if (cvv && cvv->val.getType() == NT_REFERENCE) {
                                    ref = reinterpret_cast<ReferenceNode*>(cvv->val.v.n);
                                }
                            }
                        }
                        if (ref) {
                            bool is_delete = (path_inst->unary_op == LVUnaryOp::Delete);
                            LValueRemoveHelper lvrh(*ref, xsink, is_delete);
                            if (lvrh && !*xsink) {
                                if (is_delete) {
                                    lvrh.deleteLValue();
                                    res = QoreValue();
                                } else {
                                    res = lvrh.removeValue();
                                }
                            }
                            if (*xsink) {
                                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                                cleanupLocalCaches();
                                return false;
                            }
                            goto single_step_remove_done;
                        }
                    }
                    {
                    LValueHelper lvh(xsink);
                    if (lvh.navigatePath(path_copy.data(), path_copy.size(), true)) {
                        if (*xsink) {
                            // Real error during navigation — propagate
                            cleanupValues(values, cleanup, xsink, true, cleanup_log);
                            cleanupLocalCaches();
                            return false;
                        }
                        // navigatePath failed without exception: target doesn't exist
                        // (e.g. `delete r` where r is a reference<hash> to an unassigned
                        // hash member h.b, and for_remove=true refuses to vivify the parent).
                        // Treat as no-op — matches AST semantics (QoreDeleteOperatorNode /
                        // QoreRemoveOperatorNode on a missing target both succeed silently)
                        // and the multi-step path case at :6736. Returning false here would
                        // trigger a false AST deopt and silently re-run the function body.
                        res = QoreValue();
                    } else if (path_inst->unary_op == LVUnaryOp::Remove) {
                        // `remove self` on a static_assignment lvalue (borrowed ref)
                        // must not propagate the borrowed value — discard it and return
                        // NOTHING.  Matches LValueRemoveHelper::deleteLValue's clearTemp
                        // behaviour when static_assignment is set.
                        bool static_assignment = false;
                        res = lvh.remove(static_assignment);
                        if (static_assignment) {
                            res = QoreValue();
                        }
                    } else {
                        // Delete: use remove(static_assignment) instead of removeValue(true)
                        // so `delete self` correctly handles the borrowed-ref case (self is
                        // instantiated via instantiateSelf with static_assignment=true —
                        // removeValue asserts !static_assignment and, with assertions off,
                        // would return the borrowed value and cause a double-free via the
                        // subsequent res.discard).  Mirrors LValueRemoveHelper::deleteLValue.
                        bool static_assignment = false;
                        res = lvh.remove(static_assignment);
                        if (res.getType() == NT_OBJECT) {
                            QoreObject* o = res.get<QoreObject>();
                            if (!o->isSystemObject()) {
                                o->doDelete(xsink);
                            }
                            // Only deref non-borrowed refs.  static_assignment means the
                            // LocalVarValue didn't own the +1 (e.g. `self`); the caller's
                            // ref covers the object lifetime until the method frame unwinds.
                            if (!static_assignment) {
                                res.discard(xsink);
                            }
                            res = QoreValue();
                        } else if (static_assignment) {
                            // Non-object with static_assignment (shouldn't occur for self
                            // but handle defensively) — borrowed ref, don't discard below.
                            res = QoreValue();
                        }
                    }
                    }  // end of LValueHelper scope
                    single_step_remove_done:;
                } else {
                    // Pop, Shift, Trim, Chomp use for_remove=true to avoid vivification:
                    // if the target is NOTHING, these should be no-ops rather than creating
                    // empty containers.  Pre/PostInc/Dec use for_remove=false since they
                    // legitimately need to create a value if NOTHING.
                    bool no_vivify = (path_inst->unary_op == LVUnaryOp::Pop
                        || path_inst->unary_op == LVUnaryOp::Shift
                        || path_inst->unary_op == LVUnaryOp::Trim
                        || path_inst->unary_op == LVUnaryOp::Chomp);
                    LValueHelper lvh(xsink);
                    if (lvh.navigatePath(path_copy.data(), path_copy.size(), no_vivify)) {
                        if (no_vivify && !*xsink) {
                            // navigatePath failed without error — lvalue doesn't exist, no-op
                            goto lvalue_path_unary_done;
                        }
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    switch (path_inst->unary_op) {
                        case LVUnaryOp::PreInc: {
                            qore_type_t t = lvh.getType();
                            if (t == NT_NUMBER) {
                                lvh.preIncrementNumber();
                                res = lvh.getReferencedValue();
                            } else if (t == NT_FLOAT) {
                                res = lvh.preIncrementFloat();
                            } else {
                                res = lvh.preIncrementBigInt();
                            }
                            break;
                        }
                        case LVUnaryOp::PreDec: {
                            qore_type_t t = lvh.getType();
                            if (t == NT_NUMBER) {
                                lvh.preDecrementNumber();
                                res = lvh.getReferencedValue();
                            } else if (t == NT_FLOAT) {
                                res = lvh.preDecrementFloat();
                            } else {
                                res = lvh.preDecrementBigInt();
                            }
                            break;
                        }
                        case LVUnaryOp::PostInc: {
                            qore_type_t t = lvh.getType();
                            if (t == NT_NUMBER) {
                                QoreNumberNode* n = lvh.postIncrementNumber(true);
                                if (n) {
                                    res = n;
                                }
                            } else if (t == NT_FLOAT) {
                                res = lvh.postIncrementFloat();
                            } else {
                                res = lvh.postIncrementBigInt();
                            }
                            break;
                        }
                        case LVUnaryOp::PostDec: {
                            qore_type_t t = lvh.getType();
                            if (t == NT_NUMBER) {
                                QoreNumberNode* n = lvh.postDecrementNumber(true);
                                if (n) {
                                    res = n;
                                }
                            } else if (t == NT_FLOAT) {
                                res = lvh.postDecrementFloat();
                            } else {
                                res = lvh.postDecrementBigInt();
                            }
                            break;
                        }
                        case LVUnaryOp::Shift: {
                            if (lvh.getType() != NT_LIST) {
                                break;
                            }
                            lvh.ensureUnique();
                            QoreListNode* l = lvh.getValue().get<QoreListNode>();
                            if (l && l->size() > 0) {
                                res = l->shift();
                            }
                            break;
                        }
                        case LVUnaryOp::Pop: {
                            if (lvh.getType() != NT_LIST) {
                                break;
                            }
                            lvh.ensureUnique();
                            QoreListNode* l = lvh.getValue().get<QoreListNode>();
                            if (l && l->size() > 0) {
                                res = l->pop();
                            }
                            break;
                        }
                        case LVUnaryOp::Trim: {
                            qore_type_t vtype = lvh.getType();
                            if (vtype == NT_STRING) {
                                lvh.ensureUnique();
                                QoreStringNode* str = lvh.getValue().get<QoreStringNode>();
                                if (str && str->trim(xsink)) {
                                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                                    cleanupLocalCaches();
                                    return false;
                                }
                            } else if (vtype == NT_LIST) {
                                lvh.ensureUnique();
                                QoreListNode* l = lvh.getValue().get<QoreListNode>();
                                if (l) {
                                    qore_list_private* ll = qore_list_private::get(*l);
                                    for (size_t i = 0, e = l->size(); i < e; ++i) {
                                        QoreValue& v = ll->getEntryReference(i);
                                        if (v.getType() == NT_STRING) {
                                            ensure_unique(v, xsink);
                                            if (v.get<QoreStringNode>()->trim(xsink)) {
                                                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                                                cleanupLocalCaches();
                                                return false;
                                            }
                                        }
                                    }
                                }
                            } else if (vtype == NT_HASH) {
                                lvh.ensureUnique();
                                QoreHashNode* h = lvh.getValue().get<QoreHashNode>();
                                if (h) {
                                    HashIterator hi(h);
                                    while (hi.next()) {
                                        if (hi.get().getType() == NT_STRING) {
                                            QoreValue& v = (*qhi_priv::get(hi)->i)->val;
                                            ensure_unique(v, xsink);
                                            if (v.get<QoreStringNode>()->trim(xsink)) {
                                                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                                                cleanupLocalCaches();
                                                return false;
                                            }
                                        }
                                    }
                                }
                            }
                            res = lvh.getReferencedValue();
                            break;
                        }
                        case LVUnaryOp::Chomp: {
                            qore_type_t vtype = lvh.getType();
                            if (vtype == NT_STRING) {
                                lvh.ensureUnique();
                                QoreStringNode* str = lvh.getValue().get<QoreStringNode>();
                                if (str) {
                                    res = QoreValue(static_cast<int64>(str->chomp()));
                                }
                            } else if (vtype == NT_LIST) {
                                lvh.ensureUnique();
                                QoreListNode* l = lvh.getValue().get<QoreListNode>();
                                if (l) {
                                    int64 count = 0;
                                    qore_list_private* ll = qore_list_private::get(*l);
                                    for (size_t i = 0, e = l->size(); i < e; ++i) {
                                        QoreValue& v = ll->getEntryReference(i);
                                        if (v.getType() == NT_STRING) {
                                            ensure_unique(v, xsink);
                                            count += static_cast<int64>(
                                                v.get<QoreStringNode>()->chomp());
                                        }
                                    }
                                    res = QoreValue(count);
                                }
                            } else if (vtype == NT_HASH) {
                                lvh.ensureUnique();
                                QoreHashNode* h = lvh.getValue().get<QoreHashNode>();
                                if (h) {
                                    int64 count = 0;
                                    HashIterator hi(h);
                                    while (hi.next()) {
                                        if (hi.get().getType() == NT_STRING) {
                                            QoreValue& v = (*qhi_priv::get(hi)->i)->val;
                                            ensure_unique(v, xsink);
                                            if (*xsink) {
                                                cleanupValues(values, cleanup, xsink, true, cleanup_log);
                                                cleanupLocalCaches();
                                                return false;
                                            }
                                            QoreStringNode* vs = v.get<QoreStringNode>();
                                            count += static_cast<int64>(vs->chomp());
                                        }
                                    }
                                    res = QoreValue(count);
                                }
                            }
                            break;
                        }
                        default:
                            xsink->raiseException("IR-EXEC-ERROR",
                                "unsupported unary op %d in lvalue.path.unary",
                                (int)path_inst->unary_op);
                            cleanupValues(values, cleanup, xsink, true, cleanup_log);
                            cleanupLocalCaches();
                            return false;
                    }
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
lvalue_path_unary_done:
                invalidateLValuePathClosureCache(path_inst);
                if (path_inst->lvalue_slot_id < locals_slot_cache.size()) {
                    locals_slot_cache[path_inst->lvalue_slot_id].discard(xsink);
                    locals_slot_cache[path_inst->lvalue_slot_id] = QoreValue();
                    clearLoadSlots(path_inst->lvalue_slot_id);
                    if (path_inst->lvalue_slot_id < local_init_slots.size()
                            && local_init_slots[path_inst->lvalue_slot_id] != UINT32_MAX) {
                        uint32_t init_slot = local_init_slots[path_inst->lvalue_slot_id];
                        if (init_slot < values.size()) {
                            removeCleanupEntry(cleanup, init_slot);
                            values[init_slot].discard(xsink);
                            values[init_slot] = QoreValue();
                        }
                        local_init_slots[path_inst->lvalue_slot_id] = UINT32_MAX;
                    }
                }
                markParentLValuePathDirty(path_inst);
                cleanupLocalCaches();
                if (path_inst->result.isValid()) {
                    setValueSlot(values, path_inst->result.id, res, xsink);
                    if (res.hasNode()) {
                        cleanup.push_back(path_inst->result.id);
                    }
                } else {
                    res.discard(xsink);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::LValuePathBinaryMut: {
                auto* path_inst = static_cast<QoreIRLValuePathInstruction*>(inst);
                if (path_inst->path.empty()) {
                    xsink->raiseException("IR-EXEC-ERROR", "lvalue.path.binary_mut missing path");
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                // Per-invocation private path copy; see patchLVPathLocal for why
                // mutating path_inst->path directly is a data race.
                std::vector<LVPathStep> path_copy = patchLVPathLocal(path_inst);
                ensureLValuePathRootLocal(path_inst);
                invalidateLValuePathClosureCache(path_inst);
                // Get RHS value (operands[0] for push/unshift)
                QoreValue rhs;
                if (!path_inst->operands.empty()) {
                    rhs = getIRValue(values, path_inst->operands[0]);
                }
                QoreValue res;
                LValueHelper lvh(xsink);
                if (lvh.navigatePath(path_copy.data(), path_copy.size(), false)) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                switch (path_inst->binary_mut_op) {
                    case LVBinaryMutOp::Push:
                    case LVBinaryMutOp::Unshift: {
                        // Auto-vivify NOTHING to empty list
                        if (lvh.getType() == NT_NOTHING) {
                            const QoreTypeInfo* vti = lvh.getTypeInfo();
                            if (QoreTypeInfo::parseAcceptsReturns(vti, NT_LIST)) {
                                const QoreTypeInfo* lti = vti == autoTypeInfo
                                    ? autoTypeInfo
                                    : QoreTypeInfo::getReturnComplexListOrNothing(vti);
                                if (lvh.assign(new QoreListNode(lti))) {
                                    assert(*xsink);
                                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                                    cleanupLocalCaches();
                                    return false;
                                }
                            }
                        }
                        if (lvh.getType() != NT_LIST) {
                            if (runtime_check_parse_option(PO_STRICT_ARGS)) {
                                xsink->raiseException(
                                    path_inst->binary_mut_op == LVBinaryMutOp::Push
                                        ? "PUSH-ERROR" : "UNSHIFT-ERROR",
                                    "the lvalue argument is type \"%s\"; expecting \"list\"",
                                    lvh.getTypeName());
                            }
                            cleanupValues(values, cleanup, xsink, true, cleanup_log);
                            cleanupLocalCaches();
                            return false;
                        }
                        lvh.ensureUnique();
                        QoreListNode* l = lvh.getValue().get<QoreListNode>();
                        if (path_inst->binary_mut_op == LVBinaryMutOp::Push) {
                            l->push(rhs.refSelf(), xsink);
                        } else {
                            l->insert(rhs.refSelf(), xsink);
                        }
                        res = l->refSelf();
                        break;
                    }
                    case LVBinaryMutOp::RegexSubst: {
                        // If not a string, do nothing (matches AST behavior)
                        if (!lvh.checkType(NT_STRING)) {
                            break;
                        }
                        // Get the regex from the pattern expression
                        if (path_inst->pattern_expr.hasNode()) {
                            auto* regex_op = dynamic_cast<const QoreRegexSubstOperatorNode*>(
                                path_inst->pattern_expr.getInternalNode());
                            if (regex_op && regex_op->getRegexSubst()) {
                                QoreStringNodeValueHelper str(lvh.getValue());
                                QoreStringNode* nv = regex_op->getRegexSubst()->exec(*str, xsink);
                                if (!*xsink && nv) {
                                    lvh.assign(nv);
                                    if (path_inst->ref_rv) {
                                        res = nv->refSelf();
                                    }
                                }
                            }
                        }
                        break;
                    }
                    case LVBinaryMutOp::Transliterate: {
                        // If not a string, do nothing
                        if (!lvh.checkType(NT_STRING)) {
                            break;
                        }
                        if (path_inst->pattern_expr.hasNode()) {
                            auto* trans_op = dynamic_cast<const QoreTransliterationOperatorNode*>(
                                path_inst->pattern_expr.getInternalNode());
                            if (trans_op && trans_op->getTransliteration()) {
                                QoreStringNodeValueHelper str(lvh.getValue());
                                QoreStringNode* nv = trans_op->getTransliteration()->exec(*str, xsink);
                                if (!*xsink && nv) {
                                    lvh.assign(nv);
                                    if (path_inst->ref_rv) {
                                        res = nv->refSelf();
                                    }
                                }
                            }
                        }
                        break;
                    }
                    default:
                        xsink->raiseException("IR-EXEC-ERROR",
                            "unsupported binary mutation op %d", (int)path_inst->binary_mut_op);
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                }
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                invalidateLValuePathClosureCache(path_inst);
                markParentLValuePathDirty(path_inst);
                cleanupLocalCaches();
                if (path_inst->result.isValid()) {
                    setValueSlot(values, path_inst->result.id, res, xsink);
                    if (res.hasNode()) {
                        cleanup.push_back(path_inst->result.id);
                    }
                } else {
                    res.discard(xsink);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::LValuePathTernary: {
                auto* path_inst = static_cast<QoreIRLValuePathInstruction*>(inst);
                if (path_inst->path.empty()) {
                    xsink->raiseException("IR-EXEC-ERROR", "lvalue.path.ternary missing path");
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                // Per-invocation private path copy; see patchLVPathLocal for why
                // mutating path_inst->path directly is a data race.
                std::vector<LVPathStep> path_copy = patchLVPathLocal(path_inst);
                ensureLValuePathRootLocal(path_inst);
                invalidateLValuePathClosureCache(path_inst);
                // Get the ternary operands: offset, length, replacement
                QoreValue offset_val = (path_inst->operands.size() > 0)
                    ? getIRValue(values, path_inst->operands[0]) : QoreValue();
                QoreValue length_val = (path_inst->operands.size() > 1)
                    ? getIRValue(values, path_inst->operands[1]) : QoreValue();
                QoreValue replacement_val = (path_inst->operands.size() > 2)
                    ? getIRValue(values, path_inst->operands[2]) : QoreValue();
                // Navigate to lvalue; for extract without replacement, avoid vivification
                bool no_vivify = (path_inst->ternary_op == LVTernaryOp::Extract
                    && replacement_val.isNothing());
                ReferenceHolder<QoreListNode> removed_list(xsink);
                LValueHelper lvh(xsink);
                if (lvh.navigatePath(path_copy.data(), path_copy.size(), no_vivify)) {
                    if (no_vivify && !*xsink) {
                        // Lvalue doesn't exist — no-op, return NOTHING
                        if (path_inst->result.isValid()) {
                            setValueSlot(values, path_inst->result.id, QoreValue(), xsink);
                        }
                        ++ip;
                        break;
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                QoreValue res;
                qore_type_t vt = lvh.getType();
                if (vt == NT_NOTHING) {
                    // Mirror AST behavior: if the lvalue has a default list/string type,
                    // auto-initialize it before extract/splice.
                    const QoreTypeInfo* ti = lvh.getTypeInfo();
                    if (ti == softListTypeInfo || ti == listTypeInfo || ti == stringTypeInfo
                            || ti == softStringTypeInfo) {
                        if (!lvh.assign(QoreTypeInfo::getDefaultQoreValue(ti))) {
                            vt = lvh.getType();
                        }
                    }
                }
                if (vt == NT_NOTHING) {
                    // Nothing to extract/splice — return NOTHING
                } else if (vt != NT_LIST && vt != NT_STRING && vt != NT_BINARY) {
                    xsink->raiseException("EXTRACT-ERROR",
                        "first (lvalue) argument to the extract operator is not a list, "
                        "string, or binary object");
                } else {
                    lvh.ensureUnique();
                    size_t offset = static_cast<size_t>(offset_val.getAsBigInt());
                    if (path_inst->ternary_op == LVTernaryOp::Splice) {
                        if (vt == NT_LIST) {
                            QoreListNode* vl = lvh.getValue().get<QoreListNode>();
                            if (length_val.isNothing() && replacement_val.isNothing()) {
                                removed_list = vl->splice(offset);
                            } else {
                                size_t length = static_cast<size_t>(length_val.getAsBigInt());
                                if (replacement_val.isNothing()) {
                                    removed_list = vl->splice(offset, length);
                                } else {
                                    removed_list = vl->splice(offset, length, replacement_val, xsink);
                                }
                            }
                        } else if (vt == NT_STRING) {
                            QoreStringNode* vs = lvh.getValue().get<QoreStringNode>();
                            if (length_val.isNothing() && replacement_val.isNothing()) {
                                vs->splice(offset, xsink);
                            } else {
                                size_t length = static_cast<size_t>(length_val.getAsBigInt());
                                if (replacement_val.isNothing()) {
                                    vs->splice(offset, length, xsink);
                                } else {
                                    vs->splice(offset, length, replacement_val, xsink);
                                }
                            }
                        } else { // NT_BINARY
                            BinaryNode* b = lvh.getValue().get<BinaryNode>();
                            if (length_val.isNothing() && replacement_val.isNothing()) {
                                b->splice(offset, b->size());
                            } else {
                                size_t length = static_cast<size_t>(length_val.getAsBigInt());
                                if (replacement_val.isNothing()) {
                                    b->splice(offset, length);
                                } else {
                                    if (replacement_val.getType() == NT_BINARY) {
                                        const BinaryNode* b1 = replacement_val.get<const BinaryNode>();
                                        b->splice(offset, length, b1->getPtr(), b1->size());
                                    } else {
                                        QoreStringNodeValueHelper sv(replacement_val);
                                        if (!sv->strlen()) {
                                            b->splice(offset, length);
                                        } else {
                                            b->splice(offset, length, sv->getBuffer(), sv->size());
                                        }
                                    }
                                }
                            }
                        }
                        if (path_inst->ref_rv && !*xsink) {
                            res = lvh.getReferencedValue();
                        }
                    } else {
                        if (vt == NT_LIST) {
                            QoreListNode* vl = lvh.getValue().get<QoreListNode>();
                            if (length_val.isNothing() && replacement_val.isNothing()) {
                                res = vl->extract(offset);
                            } else {
                                size_t length = static_cast<size_t>(length_val.getAsBigInt());
                                if (replacement_val.isNothing()) {
                                    res = vl->extract(offset, length);
                                } else {
                                    res = vl->extract(offset, length, replacement_val, xsink);
                                }
                            }
                        } else if (vt == NT_STRING) {
                            QoreStringNode* vs = lvh.getValue().get<QoreStringNode>();
                            if (length_val.isNothing() && replacement_val.isNothing()) {
                                res = vs->extract(offset, xsink);
                            } else {
                                size_t length = static_cast<size_t>(length_val.getAsBigInt());
                                if (replacement_val.isNothing()) {
                                    res = vs->extract(offset, length, xsink);
                                } else {
                                    res = vs->extract(offset, length, replacement_val, xsink);
                                }
                            }
                        } else { // NT_BINARY
                            BinaryNode* b = lvh.getValue().get<BinaryNode>();
                            BinaryNode* bout = new BinaryNode;
                            if (length_val.isNothing() && replacement_val.isNothing()) {
                                b->splice(offset, b->size(), bout);
                            } else {
                                size_t length = static_cast<size_t>(length_val.getAsBigInt());
                                if (replacement_val.isNothing()) {
                                    b->splice(offset, length, bout);
                                } else {
                                    if (replacement_val.getType() == NT_BINARY) {
                                        const BinaryNode* b1 = replacement_val.get<const BinaryNode>();
                                        b->splice(offset, length, b1->getPtr(), b1->size(), bout);
                                    } else {
                                        QoreStringNodeValueHelper sv(replacement_val);
                                        if (!sv->strlen()) {
                                            b->splice(offset, length, bout);
                                        } else {
                                            b->splice(offset, length, sv->getBuffer(), sv->size(), bout);
                                        }
                                    }
                                }
                            }
                            res = bout;
                        }
                    }
                }
                if (*xsink) {
                    res.discard(xsink);
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                if (!path_inst->ref_rv) {
                    res.discard(xsink);
                    res = QoreValue();
                }
                invalidateLValuePathClosureCache(path_inst);
                if (path_inst->hasLocalTarget()) {
                    markParentLValuePathDirty(path_inst);
                    bool is_ref = !path_inst->path.empty() && path_inst->path[0].type_info
                        && QoreTypeInfo::isReference(path_inst->path[0].type_info);
                    if (is_ref) {
                        for (size_t j = 0; j < locals_slot_cache.size(); ++j) {
                            if (preserveParentSlotForWriteback(j)) {
                                continue;
                            }
                            if (j < locals_ir_only.size() && locals_ir_only[j]) {
                                continue;
                            }
                            locals_slot_cache[j].discard(xsink);
                            locals_slot_cache[j] = QoreValue();
                        }
                        cleanupStoredValues(closures, xsink);
                        cleanupStoredValues(globals, xsink);
                    } else if (path_inst->lvalue_slot_id < locals_slot_cache.size()) {
                        locals_slot_cache[path_inst->lvalue_slot_id].discard(xsink);
                        locals_slot_cache[path_inst->lvalue_slot_id] = QoreValue();
                        clearLoadSlots(path_inst->lvalue_slot_id);
                    }
                }
                cleanupLocalCaches();
                if (path_inst->result.isValid()) {
                    setValueSlot(values, path_inst->result.id, res, xsink);
                    if (res.hasNode()) {
                        cleanup.push_back(path_inst->result.id);
                    }
                }
                ++ip;
                break;
            }
            case QoreIROpcode::PreIncLValue:
            case QoreIROpcode::PreDecLValue:
            case QoreIROpcode::PostIncLValue:
            case QoreIROpcode::PostDecLValue: {
                auto* lval_inst = static_cast<QoreIRLValueInstruction*>(inst);
                // Invalidate closure cache for closure-use variables
                const VarRefNode* inc_vrn = extractLValueBaseVarRef(lval_inst->lvalue);
                invalidateClosureCache(inc_vrn);
                // Targeted cache invalidation before the lvalue operation
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
                // evalLValueUnary returns the old value for post-inc/dec, new value for pre-inc/dec.
                // The slot cache was pre-invalidated above; next LoadLocal re-reads from TLS.
                QoreValue res = QoreIRInterpreter::evalLValueUnary(inst->opcode, lval_inst->lvalue, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                markParentLValueDirty(lval_inst, inc_vrn);
                setValueSlot(values, lval_inst->result.id, res, xsink);
                if (res.hasNode()) {
                    cleanup.push_back(lval_inst->result.id);
                }
                ++ip;
                break;
            }
            case QoreIROpcode::ShiftLValue: {
                auto* lval_inst = static_cast<QoreIRLValueInstruction*>(inst);
                const VarRefNode* lval_vrn = prepareLValueSlotCache(lval_inst);
                QoreValue res = QoreIRInterpreter::evalLValueUnary(inst->opcode, lval_inst->lvalue, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                // Shift returns the removed element — always invalidate (never repopulate)
                finalizeLValueSlotCache(lval_inst, lval_vrn, res, false);
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
                const VarRefNode* lval_vrn = prepareLValueSlotCache(lval_inst);
                // Simple variable: lvalue IS the VarRefNode (e.g., x += expr)
                // Complex lvalue: lvalue is member/index access (e.g., h.value += expr)
                bool is_simple_var = dynamic_cast<const VarRefNode*>(
                    lval_inst->lvalue.getInternalNode()) != nullptr;
                // Int fast path: probe type under lock, do direct operation if int
                QoreValue res;
                bool fast_path_done = false;
                {
                    LValueHelper v(lval_inst->lvalue, xsink);
                    if (v && v.getType() == NT_INT) {
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
                finalizeLValueSlotCache(lval_inst, lval_vrn, res, is_simple_var);
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
                const VarRefNode* lval_vrn = prepareLValueSlotCache(lval_inst);
                bool is_simple_var = dynamic_cast<const VarRefNode*>(
                    lval_inst->lvalue.getInternalNode()) != nullptr;
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
                finalizeLValueSlotCache(lval_inst, lval_vrn, res, is_simple_var);
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
                const VarRefNode* lval_vrn = prepareLValueSlotCache(lval_inst);
                bool is_simple_var = dynamic_cast<const VarRefNode*>(
                    lval_inst->lvalue.getInternalNode()) != nullptr;
                QoreValue res = QoreIRInterpreter::evalLValueBinary(inst->opcode, lval_inst->lvalue, right, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                finalizeLValueSlotCache(lval_inst, lval_vrn, res, is_simple_var);
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
                const VarRefNode* lval_vrn = prepareLValueSlotCache(lval_inst);
                QoreValue res = QoreIRInterpreter::evalLValueTernary(inst->opcode, lval_inst->lvalue, first, second,
                    third, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupLocalCaches();
                    return false;
                }
                // Splice returns the mutated lvalue; always invalidate and let the slot refresh from storage.
                finalizeLValueSlotCache(lval_inst, lval_vrn, res, false);
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
                    bool has_last_use_args = direct_inst->func && hasLastUseCallArgSlots(values,
                        direct_inst->operands, 0, value_use_counts);
                    bool prefer_inline_ir = has_last_use_args
                        && ensureInterpreterInlineIRFunctionState(direct_inst, pgm, nargs) > 0;
                    if (has_last_use_args && !prefer_inline_ir) {
                        ReferenceHolder<QoreListNode> arg_list(
                            buildArgList(values, direct_inst->operands, 0, xsink), xsink);
                        if (xsink && *xsink) {
                            return returnAfterUnhandledException();
                        }
                        consumeLastUseCallArgSlots(values, cleanup, direct_inst->operands, 0,
                            value_use_counts, &weak_load_temp_slots, xsink);
                        if (xsink && *xsink) {
                            return returnAfterUnhandledException();
                        }
                        RuntimeConfig& rc = rc_get_current_ref();
                        QoreProgram* call_pgm = direct_inst->pgm ? direct_inst->pgm : rc.getProgram();
                        if (!call_pgm) {
                            call_pgm = getProgram();
                        }
                        QoreValue res = direct_inst->func->evalFunctionTmpArgs(
                            direct_inst->variant, *arg_list, call_pgm, rc, xsink);
                        if (xsink && *xsink) {
                            return returnAfterUnhandledException();
                        }
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

                    // Build NaN-boxed args array from operands
                    constexpr int SMALL_BUF = 8;
                    uint64_t nb_buf[SMALL_BUF];
                    uint64_t* nanboxed_args = nargs <= SMALL_BUF ? nb_buf : new uint64_t[nargs];
                    for (int i = 0; i < nargs; ++i) {
                        nanboxed_args[i] = toBits(getIRValue(values, direct_inst->operands[i]));
                    }

                    QoreValue res;
                    bool inline_may_invalidate_external_caches = true;
                    bool used_inline_ir = tryExecuteInterpreterInlineIRFunction(direct_inst, pgm, nanboxed_args,
                        nargs, res, xsink, &inline_may_invalidate_external_caches);
                    if (!used_inline_ir) {
                        res = fromBits(qore_rt_call_fast(
                            direct_inst->func, direct_inst->variant, direct_inst->pgm,
                            nanboxed_args, nargs, xsink));
                    }

                    if (nargs > SMALL_BUF) { delete[] nanboxed_args; }
                    if (xsink && *xsink) {
                        return returnAfterUnhandledException();
                    }
                    if (has_last_use_args) {
                        consumeLastUseCallArgSlots(values, cleanup, direct_inst->operands, 0,
                            value_use_counts, &weak_load_temp_slots, xsink);
                        if (xsink && *xsink) {
                            return returnAfterUnhandledException();
                        }
                    }
                    // If call has reference args, callee may have modified caller's locals
                    if (direct_inst->has_ref_args) {
                        cleanupLocalCaches();
                    } else if (!used_inline_ir || inline_may_invalidate_external_caches) {
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
                    if (hasLastUseCallArgSlots(values, static_inst->operands, 0,
                            value_use_counts)) {
                        QoreListNode* arg_list = buildArgList(values,
                            static_inst->operands, 0, xsink);
                        if (xsink && *xsink) {
                            if (arg_list) {
                                arg_list->deref(xsink);
                            }
                            return returnAfterUnhandledException();
                        }
                        consumeLastUseCallArgSlots(values, cleanup,
                            static_inst->operands, 0, value_use_counts, &weak_load_temp_slots, xsink);
                        if (xsink && *xsink) {
                            if (arg_list) {
                                arg_list->deref(xsink);
                            }
                            return returnAfterUnhandledException();
                        }
                        const StaticMethodCallNode* call = dynamic_cast<const StaticMethodCallNode*>(
                            static_inst->expr.getInternalNode());
                        assert(call);
                        StaticMethodCallNode clone(*call, arg_list);
                        QoreValue res = evalAndRef(&clone, xsink);
                        if (xsink && *xsink) {
                            return returnAfterUnhandledException();
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
                    constexpr int SMALL_BUF = 8;
                    uint64_t nb_buf[SMALL_BUF];
                    uint64_t* nanboxed_args = nargs <= SMALL_BUF ? nb_buf : new uint64_t[nargs];
                    for (int i = 0; i < nargs; ++i) {
                        nanboxed_args[i] = toBits(getIRValue(values, static_inst->operands[i]));
                    }
                    QoreValue res;
                    bool inline_may_invalidate_external_caches = true;
                    bool used_inline_ir = tryExecuteInterpreterInlineIRStaticMethod(static_inst, pgm,
                        nanboxed_args, nargs, res, xsink, &inline_may_invalidate_external_caches);
                    if (!used_inline_ir) {
                        res = fromBits(qore_rt_call_static_method_direct(
                            static_inst->method, static_inst->variant,
                            nanboxed_args, nargs, xsink));
                    }
                    if (nargs > SMALL_BUF) { delete[] nanboxed_args; }
                    if (xsink && *xsink) {
                        return returnAfterUnhandledException();
                    }
                    if (static_inst->has_ref_args) {
                        cleanupLocalCaches();
                    } else if (!used_inline_ir || inline_may_invalidate_external_caches) {
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
                            lhs_val, rhs_val, true, sq->hasStringIndexChar(), sq->hasNegativeOffsets(), xsink);
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
                        return returnAfterUnhandledException();
                    }
                    consumeLastUseCallArgSlots(values, cleanup, inst->operands, arg_start,
                        value_use_counts, &weak_load_temp_slots, xsink);
                    if (xsink && *xsink) {
                        if (arg_list) {
                            arg_list->deref(xsink);
                        }
                        return returnAfterUnhandledException();
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
                            ensureLocalInstantiated(var_new_obj->ref.id, instantiated_locals, instantiated_locals_ordered, pre_instantiated,
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
                            const QoreTypeInfo* object_type_info
                                = qore_substitute_type_params_if_needed(scoped->getObjectTypeInfo());
                            res = qore_class_private::execConstructor(*scoped->oc, rc,
                                scoped->getVariant(), scoped->getArgs(), xsink, object_type_info);
                            used_operands = true;
                        }
                    }
                    if (!used_operands) {
                        // Direct eval — avoids evalExprNode() overhead
                        res = evalAndRef(call_expr, xsink);
                    }
                }
                if (xsink && *xsink) {
                    return returnAfterUnhandledException();
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
                    QoreObject* self = runtime_get_stack_object();
                    QoreValue res;
                    bool inline_may_invalidate_external_caches = true;
                    bool used_inline_ir = tryExecuteInterpreterInlineIRMethod(direct_inst, self, pgm,
                        nanboxed_args, nargs, res, xsink, &inline_may_invalidate_external_caches);
                    if (!used_inline_ir) {
                        res = fromBits(qore_rt_call_method_fast(
                            direct_inst->method, direct_inst->variant,
                            nanboxed_args, nargs, xsink));
                    }
                    if (nargs > SMALL_BUF) { delete[] nanboxed_args; }
                    if (xsink && *xsink) {
                        return returnAfterUnhandledException();
                    }
                    if (direct_inst->has_ref_args) {
                        cleanupLocalCaches();
                    } else {
                        // Skip invalidation for built-in methods without reference arguments
                        if (!direct_inst->method->isBuiltin()
                                && (!used_inline_ir || inline_may_invalidate_external_caches)) {
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
                if (!direct_inst->operands.empty()) {
                    qore_list_private* priv = qore_list_private::get(**arg_list);
                    priv->reserve(direct_inst->operands.size());
                    for (const auto& operand : direct_inst->operands) {
                        QoreValue arg_val = getIRValue(values, operand);
                        if (arg_val.hasNode()) {
                            arg_val.refSelf();
                        }
                        priv->pushIntern(arg_val);
                    }
                }

                // Get runtime config and call the method directly
                RuntimeConfig& rc = rc_get_current_ref();
                QoreValue res = qore_method_private::evalTmpArgs(*method, xsink, rc, self, *arg_list);

                if (xsink && *xsink) {
                    return returnAfterUnhandledException();
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
                    QoreObject* self = runtime_get_stack_object();
                    QoreValue res;
                    bool inline_may_invalidate_external_caches = true;
                    bool used_inline_ir = tryExecuteInterpreterInlineIRMethod(invoke_inst, self, pgm,
                        nanboxed_args, nargs, res, xsink, &inline_may_invalidate_external_caches);
                    if (!used_inline_ir) {
                        res = fromBits(qore_rt_call_method_fast(
                            invoke_inst->method, invoke_inst->variant,
                            nanboxed_args, nargs, xsink));
                    }
                    if (nargs > SMALL_BUF) { delete[] nanboxed_args; }
                    if (invoke_inst->has_ref_args) {
                        cleanupLocalCaches();
                    } else {
                        // For built-in methods without reference arguments that are known to not
                        // access Qore global state (like Future::isDone(), Counter::dec(), etc.),
                        // skip invalidation to avoid expensive hash map operations on hot paths
                        if (invoke_inst->method->isBuiltin()
                                || (used_inline_ir && !inline_may_invalidate_external_caches)) {
                            // Built-in methods are pure C++ and don't access Qore globals
                            // No invalidation needed
                        } else {
                            invalidateExternalCaches();
                        }
                    }
                    if (xsink && *xsink) {
                        // On exception, branch to exception target
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        if (!invoke_inst->exception_target) {
                            return returnAfterUnhandledException(true);
                        }
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
                if (!invoke_inst->operands.empty()) {
                    qore_list_private* priv = qore_list_private::get(**arg_list);
                    priv->reserve(invoke_inst->operands.size());
                    for (const auto& operand : invoke_inst->operands) {
                        QoreValue arg_val = getIRValue(values, operand);
                        if (arg_val.hasNode()) {
                            arg_val.refSelf();
                        }
                        priv->pushIntern(arg_val);
                    }
                }

                // Get runtime config and call the method directly
                RuntimeConfig& rc = rc_get_current_ref();
                QoreValue res = qore_method_private::evalTmpArgs(*method, xsink, rc, self, *arg_list);

                // Method call runs in its own frame and cannot modify caller's locals
                // Skip invalidation for built-in methods without reference arguments
                // (built-in methods are pure C++ and don't access Qore global state)
                if (!method->isBuiltin() || invoke_inst->has_ref_args) {
                    invalidateExternalCaches();
                }

                if (xsink && *xsink) {
                    // On exception, branch to exception target
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    if (!invoke_inst->exception_target) {
                        return returnAfterUnhandledException(true);
                    }
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

                // Unwrap weak references to get the underlying object —
                // WeakReferenceNode is transparent for method dispatch
                // (AST mode unwraps via evalImpl(); IR caches the raw node)
                if (base.getType() == NT_WEAKREF) {
                    QoreObject* o = base.get<const WeakReferenceNode>()->get();
                    if (!o || !o->isValid()) {
                        xsink->raiseException("OBJECT-ALREADY-DELETED",
                            "cannot call method on a deleted weak reference");
                        cleanupValues(values, cleanup, xsink, true, cleanup_log);
                        cleanupLocalCaches();
                        return false;
                    }
                    base = QoreValue(o);
                }

                // Build NaN-boxed args array from operands[1..n-1]
                int nargs = static_cast<int>(direct_inst->operands.size()) - 1;
                constexpr int SMALL_BUF = 8;
                uint64_t nb_buf[SMALL_BUF];
                uint64_t* nanboxed_args = nargs <= SMALL_BUF ? nb_buf : new uint64_t[nargs];
                for (int i = 0; i < nargs; ++i) {
                    nanboxed_args[i] = toBits(getIRValue(values, direct_inst->operands[i + 1]));
                }
                const QoreTypeParamInstantiation* explicit_inst = direct_inst->explicit_type_param_inst;

                QoreValue res;
                bool called_external = false;  // Track if we called external code
                if (direct_inst->pseudo) {
                    // Fast-path optimizations for common pseudo-methods
                    const char* method_name = direct_inst->method
                        ? direct_inst->method->getName()
                        : direct_inst->fallback_method_name;
                    qore_type_t base_type = base.getType();
                    bool name_dispatch_first = base_type == NT_OBJECT || base_type == NT_WEAKREF
                        || base_type == NT_HASH || base_type == NT_WEAKREF_HASH;

                    // Resolve pseudo-method at runtime if not resolved at
                    // deserialization time (e.g. pseudo-class not found)
                    const QoreMethod* pseudo_method = direct_inst->method;
                    const QoreClass* pseudo_qc = direct_inst->qc;
                    if (!pseudo_method && method_name) {
                        QoreClass* resolved_qc = nullptr;
                        pseudo_method = pseudo_classes_find_method(base_type, method_name, resolved_qc);
                        if (resolved_qc) {
                            pseudo_qc = resolved_qc;
                        }
                    }

                    if (name_dispatch_first && method_name) {
                        called_external = true;
                        res = fromBits(dot_eval_fallback_with_args(base, method_name,
                            nanboxed_args, nargs, xsink, explicit_inst));
                    } else if (method_name && !strcmp(method_name, "typeCode") && nargs == 0) {
                        // Inline: return type code constant
                        res = QoreValue(static_cast<int64_t>(base_type));
                    } else if (method_name && !strcmp(method_name, "size") && nargs == 0) {
                        // Inline: size() for lists and strings
                        if (base_type == NT_LIST) {
                            const QoreListNode* l = base.get<const QoreListNode>();
                            res = QoreValue(static_cast<int64_t>(l->size()));
                        } else if (base_type == NT_STRING) {
                            QoreStringValueHelper s(base);
                            res = QoreValue(static_cast<int64_t>(s->strlen()));
                        } else if (base_type == NT_HASH) {
                            const QoreHashNode* h = base.get<const QoreHashNode>();
                            res = QoreValue(static_cast<int64_t>(h->size()));
                        } else {
                            // Unsupported type, use runtime dispatch
                            called_external = true;
                            res = fromBits(qore_rt_dot_eval_pseudo_method_direct(
                                toBits(base), pseudo_method, pseudo_qc, direct_inst->variant,
                                nanboxed_args, nargs, xsink));
                        }
                    } else if (method_name && (!strcmp(method_name, "strlen") || !strcmp(method_name, "length")) && nargs == 0) {
                        // Inline: strlen()/length() for strings
                        // strlen() returns byte length, length() returns character count
                        if (base_type == NT_STRING) {
                            QoreStringValueHelper s(base);
                            res = QoreValue(static_cast<int64_t>(
                                !strcmp(method_name, "strlen") ? s->strlen() : s->length()));
                        } else {
                            // Unsupported type, use runtime dispatch
                            called_external = true;
                            res = fromBits(qore_rt_dot_eval_pseudo_method_direct(
                                toBits(base), pseudo_method, pseudo_qc, direct_inst->variant,
                                nanboxed_args, nargs, xsink));
                        }
                    } else if (method_name && !strcmp(method_name, "empty") && nargs == 0) {
                        // Inline: empty() for lists and strings
                        if (base_type == NT_LIST) {
                            const QoreListNode* l = base.get<const QoreListNode>();
                            res = QoreValue(l->empty() ? true : false);
                        } else if (base_type == NT_STRING) {
                            QoreStringValueHelper s(base);
                            res = QoreValue(s->strlen() == 0 ? true : false);
                        } else if (base_type == NT_HASH) {
                            const QoreHashNode* h = base.get<const QoreHashNode>();
                            res = QoreValue(h->empty() ? true : false);
                        } else {
                            // Unsupported type, use runtime dispatch
                            called_external = true;
                            res = fromBits(qore_rt_dot_eval_pseudo_method_direct(
                                toBits(base), pseudo_method, pseudo_qc, direct_inst->variant,
                                nanboxed_args, nargs, xsink));
                        }
                    } else if (method_name && !strcmp(method_name, "val") && nargs == 0) {
                        // Inline: val() for lists and strings (opposite of empty)
                        if (base_type == NT_LIST) {
                            const QoreListNode* l = base.get<const QoreListNode>();
                            res = QoreValue(l->empty() ? false : true);
                        } else if (base_type == NT_STRING) {
                            QoreStringValueHelper s(base);
                            res = QoreValue(s->strlen() == 0 ? false : true);
                        } else if (base_type == NT_HASH) {
                            const QoreHashNode* h = base.get<const QoreHashNode>();
                            res = QoreValue(h->empty() ? false : true);
                        } else {
                            // Unsupported type, use runtime dispatch
                            called_external = true;
                            res = fromBits(qore_rt_dot_eval_pseudo_method_direct(
                                toBits(base), pseudo_method, pseudo_qc, direct_inst->variant,
                                nanboxed_args, nargs, xsink));
                        }
                    } else if (method_name && !strcmp(method_name, "type") && nargs == 0) {
                        // Inline: type() - return type name string
                        res = QoreValue::makeStringValue(base.getTypeName());
                    } else {
                        // Unsupported pseudo-method, use generic runtime dispatch
                        called_external = true;
                        res = fromBits(qore_rt_dot_eval_pseudo_method_direct(
                            toBits(base), pseudo_method, pseudo_qc, direct_inst->variant,
                            nanboxed_args, nargs, xsink));
                    }
                } else {
                    const char* mname = direct_inst->fallback_method_name
                        ? direct_inst->fallback_method_name
                        : (direct_inst->method ? direct_inst->method->getName() : nullptr);
                    bool inline_may_invalidate_external_caches = true;
                    if (mname && tryExecuteInterpreterInlineIRDotEvalMethod(direct_inst, base, mname,
                            explicit_inst, pgm, nanboxed_args, nargs, res, xsink,
                            &inline_may_invalidate_external_caches)) {
                        // Result produced by the inline IR path.
                        called_external = inline_may_invalidate_external_caches;
                    } else if (direct_inst->method && direct_inst->qc) {
                        called_external = true;
                        res = fromBits(explicit_inst
                            ? qore_rt_dot_eval_method_direct_with_inst(toBits(base), direct_inst->method,
                                direct_inst->qc, direct_inst->variant, nanboxed_args, nargs, explicit_inst, xsink)
                            : qore_rt_dot_eval_method_direct(toBits(base), direct_inst->method, direct_inst->qc,
                                direct_inst->variant, nanboxed_args, nargs, xsink));
                    } else if (mname) {
                        called_external = true;
                        res = fromBits(dot_eval_fallback_with_args(base, mname,
                            nanboxed_args, nargs, xsink, explicit_inst));
                    } else {
                        xsink->raiseException("IR-ERROR",
                            "DotEvalMethodDirect: null method pointer and no method name");
                    }
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
                const QoreTypeParamInstantiation* explicit_inst = de_invoke_inst->explicit_type_param_inst;

                QoreValue res;
                bool called_external = false;  // Track if we called external code
                if (de_invoke_inst->pseudo) {
                    // Fast-path optimizations for common pseudo-methods
                    const char* method_name = de_invoke_inst->method
                        ? de_invoke_inst->method->getName()
                        : de_invoke_inst->fallback_method_name;
                    qore_type_t base_type = base.getType();
                    bool name_dispatch_first = base_type == NT_OBJECT || base_type == NT_WEAKREF
                        || base_type == NT_HASH || base_type == NT_WEAKREF_HASH;

                    // Resolve pseudo-method at runtime if not resolved at
                    // deserialization time
                    const QoreMethod* pseudo_method = de_invoke_inst->method;
                    const QoreClass* pseudo_qc = de_invoke_inst->qc;
                    if (!pseudo_method && method_name) {
                        QoreClass* resolved_qc = nullptr;
                        pseudo_method = pseudo_classes_find_method(base_type, method_name, resolved_qc);
                        if (resolved_qc) {
                            pseudo_qc = resolved_qc;
                        }
                    }

                    if (name_dispatch_first && method_name) {
                        called_external = true;
                        res = fromBits(dot_eval_fallback_with_args(base, method_name,
                            nanboxed_args, nargs, xsink, explicit_inst));
                    } else if (method_name && !strcmp(method_name, "typeCode") && nargs == 0) {
                        // Inline: return type code constant
                        res = QoreValue(static_cast<int64_t>(base_type));
                    } else if (method_name && !strcmp(method_name, "size") && nargs == 0) {
                        // Inline: size() for lists and strings
                        if (base_type == NT_LIST) {
                            const QoreListNode* l = base.get<const QoreListNode>();
                            res = QoreValue(static_cast<int64_t>(l->size()));
                        } else if (base_type == NT_STRING) {
                            QoreStringValueHelper s(base);
                            res = QoreValue(static_cast<int64_t>(s->strlen()));
                        } else if (base_type == NT_HASH) {
                            const QoreHashNode* h = base.get<const QoreHashNode>();
                            res = QoreValue(static_cast<int64_t>(h->size()));
                        } else {
                            // Unsupported type, use runtime dispatch
                            called_external = true;
                            res = fromBits(qore_rt_dot_eval_pseudo_method_direct(
                                toBits(base), pseudo_method, pseudo_qc, de_invoke_inst->variant,
                                nanboxed_args, nargs, xsink));
                        }
                    } else if (method_name && (!strcmp(method_name, "strlen") || !strcmp(method_name, "length")) && nargs == 0) {
                        // Inline: strlen()/length() for strings
                        // strlen() returns byte length, length() returns character count
                        if (base_type == NT_STRING) {
                            QoreStringValueHelper s(base);
                            res = QoreValue(static_cast<int64_t>(
                                !strcmp(method_name, "strlen") ? s->strlen() : s->length()));
                        } else {
                            // Unsupported type, use runtime dispatch
                            called_external = true;
                            res = fromBits(qore_rt_dot_eval_pseudo_method_direct(
                                toBits(base), pseudo_method, pseudo_qc, de_invoke_inst->variant,
                                nanboxed_args, nargs, xsink));
                        }
                    } else if (method_name && !strcmp(method_name, "empty") && nargs == 0) {
                        // Inline: empty() for lists and strings
                        if (base_type == NT_LIST) {
                            const QoreListNode* l = base.get<const QoreListNode>();
                            res = QoreValue(l->empty() ? true : false);
                        } else if (base_type == NT_STRING) {
                            QoreStringValueHelper s(base);
                            res = QoreValue(s->strlen() == 0 ? true : false);
                        } else if (base_type == NT_HASH) {
                            const QoreHashNode* h = base.get<const QoreHashNode>();
                            res = QoreValue(h->empty() ? true : false);
                        } else {
                            // Unsupported type, use runtime dispatch
                            called_external = true;
                            res = fromBits(qore_rt_dot_eval_pseudo_method_direct(
                                toBits(base), pseudo_method, pseudo_qc, de_invoke_inst->variant,
                                nanboxed_args, nargs, xsink));
                        }
                    } else if (method_name && !strcmp(method_name, "val") && nargs == 0) {
                        // Inline: val() for lists and strings (opposite of empty)
                        if (base_type == NT_LIST) {
                            const QoreListNode* l = base.get<const QoreListNode>();
                            res = QoreValue(l->empty() ? false : true);
                        } else if (base_type == NT_STRING) {
                            QoreStringValueHelper s(base);
                            res = QoreValue(s->strlen() == 0 ? false : true);
                        } else if (base_type == NT_HASH) {
                            const QoreHashNode* h = base.get<const QoreHashNode>();
                            res = QoreValue(h->empty() ? false : true);
                        } else {
                            // Unsupported type, use runtime dispatch
                            called_external = true;
                            res = fromBits(qore_rt_dot_eval_pseudo_method_direct(
                                toBits(base), pseudo_method, pseudo_qc, de_invoke_inst->variant,
                                nanboxed_args, nargs, xsink));
                        }
                    } else if (method_name && !strcmp(method_name, "type") && nargs == 0) {
                        // Inline: type() - return type name string
                        res = QoreValue::makeStringValue(base.getTypeName());
                    } else {
                        // Unsupported pseudo-method, use generic runtime dispatch
                        called_external = true;
                        res = fromBits(qore_rt_dot_eval_pseudo_method_direct(
                            toBits(base), pseudo_method, pseudo_qc, de_invoke_inst->variant,
                            nanboxed_args, nargs, xsink));
                    }
                } else {
                    const char* mname = de_invoke_inst->fallback_method_name
                        ? de_invoke_inst->fallback_method_name
                        : (de_invoke_inst->method ? de_invoke_inst->method->getName() : nullptr);
                    bool inline_may_invalidate_external_caches = true;
                    if (mname && tryExecuteInterpreterInlineIRDotEvalMethod(de_invoke_inst, base, mname,
                            explicit_inst, pgm, nanboxed_args, nargs, res, xsink,
                            &inline_may_invalidate_external_caches)) {
                        // Result produced by the inline IR path.
                        called_external = inline_may_invalidate_external_caches;
                    } else if (de_invoke_inst->method && de_invoke_inst->qc) {
                        called_external = true;
                        res = fromBits(explicit_inst
                            ? qore_rt_dot_eval_method_direct_with_inst(toBits(base), de_invoke_inst->method,
                                de_invoke_inst->qc, de_invoke_inst->variant, nanboxed_args, nargs, explicit_inst,
                                xsink)
                            : qore_rt_dot_eval_method_direct(toBits(base), de_invoke_inst->method,
                                de_invoke_inst->qc, de_invoke_inst->variant, nanboxed_args, nargs, xsink));
                    } else if (mname) {
                        called_external = true;
                        res = fromBits(dot_eval_fallback_with_args(base, mname,
                            nanboxed_args, nargs, xsink, explicit_inst));
                    } else {
                        xsink->raiseException("IR-ERROR",
                            "InvokeDotEvalMethodDirect: null method pointer and no method name");
                    }
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
                    if (!de_invoke_inst->exception_target) {
                        return returnAfterUnhandledException(true);
                    }
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
                    res = raiseIRAstFallback(xsink, "expr", &func, block, ip, inst, inst->opcode,
                        expr_inst->expr);
                }
            } else {
                res = raiseIRAstFallback(xsink, "expr", &func, block, ip, inst, inst->opcode,
                    expr_inst->expr);
            }
            if (xsink && *xsink) {
                executeOnBlockExitHandlers(on_block_exit_handlers, xsink, &locals_slot_cache);
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
                executeOnBlockExitHandlers(on_block_exit_handlers, xsink, &locals_slot_cache);
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
                executeOnBlockExitHandlers(on_block_exit_handlers, xsink, &locals_slot_cache);
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
                        res = raiseIRAstFallback(xsink, "expr", &func, block, ip, inst, inst->opcode,
                            expr_inst->expr);
                    }
                } else if (auto* extract_node = dynamic_cast<const QoreRegexExtractOperatorNode*>(
                        expr_inst->expr.getInternalNode())) {
                    QoreRegex* regex = extract_node->getRegex();
                    if (regex) {
                        QoreStringNodeValueHelper str(str_val);
                        res = QoreValue(regex->extractSubstrings(*str, xsink));
                    } else {
                        res = raiseIRAstFallback(xsink, "expr", &func, block, ip, inst, inst->opcode,
                            expr_inst->expr);
                    }
                } else {
                    res = raiseIRAstFallback(xsink, "expr", &func, block, ip, inst, inst->opcode,
                        expr_inst->expr);
                }
            } else {
                res = raiseIRAstFallback(xsink, "expr", &func, block, ip, inst, inst->opcode,
                    expr_inst->expr);
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
                if (inst->opcode == QoreIROpcode::ListAssignAny && inst->operands.size() >= 2) {
                    QoreValue rhs = getIRValue(values, inst->operands[0]);
                    QoreValue idx = getIRValue(values, inst->operands[1]);
                    QoreValue res = getListAssignmentValue(rhs, idx.getAsBigInt());
                    setValueSlot(values, inst->result.id, res, xsink);
                    if (res.hasNode()) {
                        cleanup.push_back(inst->result.id);
                    }
                    ++ip;
                    break;
                }
                // Invalidate all caches BEFORE the lvalue operation to prevent COW inflation
                cleanupLocalCaches();
                // Unsupported expression shape: fail fast instead of evaluating AST.
                QoreValue res = raiseIRAstFallback(xsink, "expr", &func, block, ip, inst, inst->opcode,
                    expr_inst->expr);
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
                    res = raiseIRAstFallback(xsink, "expr", &func, block, ip, inst, inst->opcode,
                        expr_inst->expr);
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
                    res = raiseIRAstFallback(xsink, "expr", &func, block, ip, inst, inst->opcode,
                        expr_inst->expr);
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
        // Background operator: only decomposed native call shapes are supported.
        case QoreIROpcode::BackgroundInt: {
                auto* expr_inst = static_cast<QoreIRExprInstruction*>(inst);
                QoreValue res;
                auto* bg_inst = dynamic_cast<const QoreIRBackgroundInstruction*>(inst);
                auto* bg_op = dynamic_cast<const QoreBackgroundOperatorNode*>(
                    expr_inst->expr.getInternalNode());
                auto [matched, bg_result] = bg_inst && !bg_op
                    ? runBackgroundMetadata(bg_inst, values, xsink)
                    : runDecomposedBackground(bg_op, expr_inst->operands, values, xsink);
                if (matched) {
                    res = bg_result;
                } else {
                    // The inner expression was not a decomposable call shape.
                    // Report it so native lowering can be added.
                    if (getenv("QORE_IR_TRACE_BG_FALLBACK")) {
                        const AbstractQoreNode* inner = bg_op ? bg_op->getExp().getInternalNode()
                                                              : nullptr;
                        const QoreProgramLocation* loc = inst->loc;
                        fprintf(stderr, "[bg-fallback] %s:%d inner_type=%s\n",
                            loc ? (loc->getFile() ? loc->getFile() : "<unknown>") : "<no-loc>",
                            loc ? loc->start_line : 0,
                            inner ? typeid(*inner).name() : "<null>");
                        fflush(stderr);
                    }
                    res = raiseIRAstFallback(xsink, "expr", &func, block, ip, inst, inst->opcode,
                        expr_inst->expr);
                }
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
                QoreValue res;
                if (!inst->operands.empty()) {
                    res = QoreIRInterpreter::evalUnary(inst->opcode,
                        getIRValue(values, inst->operands[0]), xsink);
                } else {
                    res = raiseIRAstFallback(xsink, "expr", &func, block, ip, inst, inst->opcode,
                        expr_inst->expr);
                }
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
                    res = raiseIRAstFallback(xsink, "expr", &func, block, ip, inst, inst->opcode,
                        expr_inst->expr);
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
                        res = raiseIRAstFallback(xsink, "expr", &func, block, ip, inst, inst->opcode,
                            expr_inst->expr);
                    }
                } else {
                    res = raiseIRAstFallback(xsink, "expr", &func, block, ip, inst, inst->opcode,
                        expr_inst->expr);
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
                    tlpd->dbgException(getDebugStatement(inst), xsink);
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
                return returnAfterUnhandledException(true);
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
                            // The args may contain unevaluated AST nodes (e.g., $1.err + "-NEW"
                            // wrapped in a QoreListNode by RethrowStatement::parseInitImpl).
                            // Evaluate like the AST path does via ValueEvalOptimizedRefHolder.
                            if (arg.needsEval()) {
                                ValueEvalOptimizedRefHolder v(arg, xsink);
                                if (!*xsink && v->getType() == NT_LIST) {
                                    ex = ex->replaceTop(*v->get<const QoreListNode>(), *xsink);
                                }
                            } else if (arg.getType() == NT_LIST) {
                                ex = ex->replaceTop(*arg.get<const QoreListNode>(), *xsink);
                            }
                        }
                        qore_es_private::get(*xsink)->rethrow(ex);
                    }
                }
                if (debug_active && *xsink) {
                    tlpd->dbgException(getDebugStatement(inst), xsink);
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
                return returnAfterUnhandledException();
            }
            case QoreIROpcode::Return: {
                auto* ret = static_cast<QoreIRReturnInstruction*>(inst);
                if (ret->has_value) {
                    QoreValue val = getIRValue(values, ret->value);
                    bool owned_return_slot = removeCleanupEntry(cleanup, ret->value.id);
                    traceIRRef(inst, "return.input", ret->value.id, val,
                        owned_return_slot ? "owned" : "borrowed");
                    if (owned_return_slot && ret->value.id < values.size()) {
                        // Match ReturnStatement::takeReferencedValue(): an owned IR slot is already a
                        // referenced return value, so move it out instead of refSelf()+discard.  The latter
                        // can collect self-referential objects while returning them.
                        return_value = values[ret->value.id];
                        values[ret->value.id] = QoreValue();
                        traceIRRef(inst, "return.move-owned", ret->value.id, return_value);
                    } else if (val.hasNode()) {
                        return_value = val.refSelf();
                        traceIRRef(inst, "return.output", ret->value.id, return_value);
                        if (ret->value.id < values.size()) {
                            // Borrowed return slots are not owned by this IR frame.  After refSelf(), the
                            // added reference belongs to return_value, so only owned slots can be discarded.
                            if (owned_return_slot) {
                                traceIRRef(inst, "return.discard-slot", ret->value.id, values[ret->value.id]);
                                values[ret->value.id].discard(xsink);
                            }
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
                if (getenv("QORE_IR_TRACE_EXCEPTIONS")) {
                    fprintf(stderr, "[ir-exception] func='%s' return xsink-after-dbg-exit=%d\n",
                        func.name.c_str(), xsink && *xsink ? 1 : 0);
                    fflush(stderr);
                }
                executeOnBlockExitHandlers(on_block_exit_handlers, xsink, &locals_slot_cache);
                cleanupValues(values, cleanup, xsink, false, cleanup_log);
                cleanupLocalCaches(false);
                return true;
            }
            case QoreIROpcode::ReturnNothing: {
                return_value = QoreValue();
                if (debug_active) {
                    tlpd->dbgFunctionExit(statements, return_value, xsink);
                }
                if (getenv("QORE_IR_TRACE_EXCEPTIONS")) {
                    fprintf(stderr, "[ir-exception] func='%s' return-nothing xsink-after-dbg-exit=%d\n",
                        func.name.c_str(), xsink && *xsink ? 1 : 0);
                    fflush(stderr);
                }
                executeOnBlockExitHandlers(on_block_exit_handlers, xsink, &locals_slot_cache);
                cleanupValues(values, cleanup, xsink, false, cleanup_log);
                cleanupLocalCaches(false);
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

        if (!weak_load_temp_slots.empty()) {
            for (const auto& op : inst->operands) {
                consumeOperandUse(op.id);
            }
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
    cleanupLocalCaches(false);
    return false;
}

QoreValue QoreIRInterpreter::evalUnary(QoreIROpcode op, const QoreValue& value, ExceptionSink* xsink) {
    switch (op) {
        case QoreIROpcode::ToBool:
            return QoreValue(value.getAsBool());
        case QoreIROpcode::ToInt:
            return QoreValue(value.getAsBigInt());
        case QoreIROpcode::ToFloat:
            return QoreValue(value.getAsFloat());
        case QoreIROpcode::Not:
            return QoreValue(!value.getAsBool());
        case QoreIROpcode::IsNullOrNothing:
            return QoreValue(value.isNullOrNothing());
        case QoreIROpcode::UnaryPlusAny: {
            // QoreSingleExpressionOperatorNode's destructor derefs its stored
            // exp unconditionally, so we must refSelf when passing a borrowed
            // value — otherwise the destructor steals a reference that the IR
            // interpreter owns elsewhere (cache + values[rid] for LoadLocal
            // results), causing a UAF when cache/values later deref.  Binary
            // ops in evalBinary already refSelf for the same reason.
            QoreUnaryPlusOperatorNode node(nullptr,
                value.hasNode() ? value.refSelf() : value);
            return evalAndRef(&node, xsink);
        }
        case QoreIROpcode::UnaryMinusInt:
            return QoreValue(-value.getAsBigInt());
        case QoreIROpcode::UnaryMinusFloat:
            return QoreValue(-value.getAsFloat());
        case QoreIROpcode::UnaryMinusAny: {
            // See UnaryPlusAny — refSelf balances the operator node's dtor.
            QoreUnaryMinusOperatorNode node(nullptr,
                value.hasNode() ? value.refSelf() : value);
            return evalAndRef(&node, xsink);
        }
        case QoreIROpcode::ExistsAny:
        case QoreIROpcode::ExistsBool:
            return QoreValue(!value.isNothing());
        case QoreIROpcode::IsCollectionType: {
            qore_type_t t = value.getType();
            return QoreValue(t == NT_LIST || t == NT_OBJECT);
        }
        case QoreIROpcode::KeysAny:
        case QoreIROpcode::KeysList:
        case QoreIROpcode::KeysHash: {
            qore_type_t t = value.getType();
            if (t == NT_HASH) {
                return value.get<const QoreHashNode>()->getKeys();
            }
            if (t == NT_OBJECT) {
                QoreObject* o = const_cast<QoreObject*>(value.get<const QoreObject>());
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
        case QoreIROpcode::ElementsAny:
        case QoreIROpcode::ElementsInt: {
            qore_type_t t = value.getType();
            if (t == NT_LIST) {
                return QoreValue(static_cast<int64>(value.get<const QoreListNode>()->size()));
            }
            if (t == NT_HASH) {
                return QoreValue(static_cast<int64>(value.get<const QoreHashNode>()->size()));
            }
            if (t == NT_STRING) {
                QoreStringValueHelper str(value);
                return QoreValue(static_cast<int64>(str->length()));
            }
            if (t == NT_BINARY) {
                return QoreValue(static_cast<int64>(value.get<const BinaryNode>()->size()));
            }
            if (t == NT_BUFFER) {
                return QoreValue(static_cast<int64>(value.get<const QoreBufferNode>()->size()));
            }
            if (t == NT_OBJECT) {
                return QoreValue(static_cast<int64>(
                    const_cast<QoreObject*>(value.get<const QoreObject>())->size(xsink)));
            }
            return QoreValue(0ll);
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
        case QoreIROpcode::AddTimeout: {
            // Timeout arithmetic: int operand is milliseconds, date operand is duration
            // Convert the int (ms) to a relative date, then add
            if (left.getType() == NT_INT && right.getType() == NT_DATE) {
                int64_t lms = left.getAsBigInt();
                int64_t secs = lms / 1000;
                int64_t ms = lms - (secs * 1000);
                DateTime l;
                // the leftover is in milliseconds; setRelativeDateSeconds() takes microseconds
                l.setRelativeDateSeconds(secs, static_cast<int>(ms * 1000));
                return right.get<const DateTimeNode>()->add(l);
            }
            if (left.getType() == NT_DATE && right.getType() == NT_INT) {
                int64_t rms = right.getAsBigInt();
                int64_t secs = rms / 1000;
                int64_t ms = rms - (secs * 1000);
                DateTime r;
                // the leftover is in milliseconds; setRelativeDateSeconds() takes microseconds
                r.setRelativeDateSeconds(secs, static_cast<int>(ms * 1000));
                return left.get<const DateTimeNode>()->add(r);
            }
            // Fallback for non-timeout cases
            bool needs_deref = true;
            QorePlusOperatorNode node(nullptr, left.refSelf(), right.refSelf());
            return evalAndRef(&node, xsink);
        }
        case QoreIROpcode::AddString: {
            // Typed string concatenation - both operands are known to be strings
            bool l_is_string = left.getType() == NT_STRING;
            bool r_is_string = right.getType() == NT_STRING;
            if (!l_is_string && !r_is_string) {
                return QoreValue();  // Both NOTHING
            }
            if (!l_is_string) {
                QoreStringNodeValueHelper rs(right);
                return QoreValue(rs.getReferencedValue());  // Copy right
            }
            if (!r_is_string) {
                QoreStringNodeValueHelper ls(left);
                return QoreValue(ls.getReferencedValue());  // Copy left
            }
            QoreStringNodeValueHelper ls(left);
            QoreStringNodeValueHelper rs(right);
            QoreStringNode* result = new QoreStringNode(**ls);
            result->concat(*rs, xsink);
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
        case QoreIROpcode::SubTimeout: {
            // Timeout arithmetic: int operand is milliseconds, date operand is duration
            if (left.getType() == NT_INT && right.getType() == NT_DATE) {
                int64_t lms = left.getAsBigInt();
                int64_t secs = lms / 1000;
                int64_t ms = lms - (secs * 1000);
                // the leftover is in milliseconds; makeRelativeFromSeconds() takes microseconds
                SimpleRefHolder<DateTimeNode> l(DateTimeNode::makeRelativeFromSeconds(secs, static_cast<int>(ms * 1000)));
                return l->subtractBy(right.get<const DateTimeNode>());
            }
            if (left.getType() == NT_DATE && right.getType() == NT_INT) {
                int64_t rms = right.getAsBigInt();
                int64_t secs = rms / 1000;
                int64_t ms = rms - (secs * 1000);
                DateTime r;
                // the leftover is in milliseconds; setRelativeDateSeconds() takes microseconds
                r.setRelativeDateSeconds(secs, static_cast<int>(ms * 1000));
                return left.get<const DateTimeNode>()->subtractBy(r);
            }
            // Fallback
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
            // issue #3157: timeout (int ms) += date → integer ms arithmetic
            if (left.getType() == NT_INT && right.getType() == NT_DATE) {
                int64_t ms = right.get<const DateTimeNode>()->getRelativeMilliseconds();
                return QoreValue(left.getAsBigInt() + ms);
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
            // Special handling for timeout type: stored as int (milliseconds), RHS may be date
            if (left.getType() == NT_INT && right.getType() == NT_DATE) {
                // Treat as timeout arithmetic: int (ms) - date
                int64_t ms = right.get<const DateTimeNode>()->getRelativeMilliseconds();
                return QoreValue(left.getAsBigInt() - ms);
            }
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
            const qore_list_private* lp = qore_list_private::get(*l);
            size_t start_idx = 0;
            int64_t result;
            if (right.isNothing()) {
                if (sz == 0) {
                    return QoreValue();
                }
                result = lp->entry[0].getAsBigInt();
                start_idx = 1;
            } else {
                result = right.getAsBigInt();
            }
            for (size_t i = start_idx; i < sz; ++i) {
                result += lp->entry[i].getAsBigInt();
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
            const qore_list_private* lp = qore_list_private::get(*l);
            size_t start_idx = 0;
            double result;
            if (right.isNothing()) {
                if (sz == 0) {
                    return QoreValue();
                }
                result = lp->entry[0].getAsFloat();
                start_idx = 1;
            } else {
                result = right.getAsFloat();
            }
            for (size_t i = start_idx; i < sz; ++i) {
                result += lp->entry[i].getAsFloat();
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
            const qore_list_private* lp = qore_list_private::get(*l);
            size_t start_idx = 0;
            int64_t result;
            if (right.isNothing()) {
                if (sz == 0) {
                    return QoreValue();
                }
                result = lp->entry[0].getAsBigInt();
                start_idx = 1;
            } else {
                result = right.getAsBigInt();
            }
            for (size_t i = start_idx; i < sz; ++i) {
                result *= lp->entry[i].getAsBigInt();
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
            const qore_list_private* lp = qore_list_private::get(*l);
            size_t start_idx = 0;
            double result;
            if (right.isNothing()) {
                if (sz == 0) {
                    return QoreValue();
                }
                result = lp->entry[0].getAsFloat();
                start_idx = 1;
            } else {
                result = right.getAsFloat();
            }
            for (size_t i = start_idx; i < sz; ++i) {
                result *= lp->entry[i].getAsFloat();
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
            const qore_list_private* lp = qore_list_private::get(*l);
            size_t start_idx = 0;
            int64_t result;
            if (right.isNothing()) {
                if (sz == 0) {
                    return QoreValue();
                }
                result = lp->entry[0].getAsBigInt();
                start_idx = 1;
            } else {
                result = right.getAsBigInt();
            }
            for (size_t i = start_idx; i < sz; ++i) {
                result -= lp->entry[i].getAsBigInt();
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
            const qore_list_private* lp = qore_list_private::get(*l);
            size_t start_idx = 0;
            double result;
            if (right.isNothing()) {
                if (sz == 0) {
                    return QoreValue();
                }
                result = lp->entry[0].getAsFloat();
                start_idx = 1;
            } else {
                result = right.getAsFloat();
            }
            for (size_t i = start_idx; i < sz; ++i) {
                result -= lp->entry[i].getAsFloat();
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
            const qore_list_private* lp = qore_list_private::get(*l);
            size_t start_idx = 0;
            int64_t result;
            if (right.isNothing()) {
                result = lp->entry[0].getAsBigInt();
                start_idx = 1;
            } else {
                result = right.getAsBigInt();
            }
            for (size_t i = start_idx; i < sz; ++i) {
                int64_t val = lp->entry[i].getAsBigInt();
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
            const qore_list_private* lp = qore_list_private::get(*l);
            size_t start_idx = 0;
            double result;
            if (right.isNothing()) {
                result = lp->entry[0].getAsFloat();
                start_idx = 1;
            } else {
                result = right.getAsFloat();
            }
            for (size_t i = start_idx; i < sz; ++i) {
                double val = lp->entry[i].getAsFloat();
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
            const qore_list_private* lp = qore_list_private::get(*l);
            size_t start_idx = 0;
            int64_t result;
            if (right.isNothing()) {
                result = lp->entry[0].getAsBigInt();
                start_idx = 1;
            } else {
                result = right.getAsBigInt();
            }
            for (size_t i = start_idx; i < sz; ++i) {
                int64_t val = lp->entry[i].getAsBigInt();
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
            const qore_list_private* lp = qore_list_private::get(*l);
            size_t start_idx = 0;
            double result;
            if (right.isNothing()) {
                result = lp->entry[0].getAsFloat();
                start_idx = 1;
            } else {
                result = right.getAsFloat();
            }
            for (size_t i = start_idx; i < sz; ++i) {
                double val = lp->entry[i].getAsFloat();
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
            const qore_list_private* lp = qore_list_private::get(*l);
            int64_t result = right.isNothing() ? 0ll : right.getAsBigInt();
            for (size_t i = 0; i < sz; ++i) {
                result += lp->entry[i].getAsBigInt();
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
            const qore_list_private* lp = qore_list_private::get(*l);
            double result = right.isNothing() ? 0.0 : right.getAsFloat();
            for (size_t i = 0; i < sz; ++i) {
                result += lp->entry[i].getAsFloat();
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
            const qore_list_private* lp = qore_list_private::get(*l);
            int64_t result = right.isNothing() ? 1ll : right.getAsBigInt();
            for (size_t i = 0; i < sz; ++i) {
                result *= lp->entry[i].getAsBigInt();
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
            const qore_list_private* lp = qore_list_private::get(*l);
            double result = right.isNothing() ? 1.0 : right.getAsFloat();
            for (size_t i = 0; i < sz; ++i) {
                result *= lp->entry[i].getAsFloat();
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
            const qore_list_private* lp = qore_list_private::get(*l);
            int64_t result = lp->entry[sz - 1].getAsBigInt();
            for (size_t i = sz - 1; i > 0; --i) {
                result -= lp->entry[i - 1].getAsBigInt();
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
            const qore_list_private* lp = qore_list_private::get(*l);
            double result = lp->entry[sz - 1].getAsFloat();
            for (size_t i = sz - 1; i > 0; --i) {
                result -= lp->entry[i - 1].getAsFloat();
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
            const qore_list_private* lp = qore_list_private::get(*l);
            int64_t result = lp->entry[0].getAsBigInt();
            for (size_t i = 1; i < sz; ++i) {
                int64_t val = lp->entry[i].getAsBigInt();
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
            const qore_list_private* lp = qore_list_private::get(*l);
            double result = lp->entry[0].getAsFloat();
            for (size_t i = 1; i < sz; ++i) {
                double val = lp->entry[i].getAsFloat();
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
            const qore_list_private* lp = qore_list_private::get(*l);
            int64_t result = lp->entry[0].getAsBigInt();
            for (size_t i = 1; i < sz; ++i) {
                int64_t val = lp->entry[i].getAsBigInt();
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
            const qore_list_private* lp = qore_list_private::get(*l);
            double result = lp->entry[0].getAsFloat();
            for (size_t i = 1; i < sz; ++i) {
                double val = lp->entry[i].getAsFloat();
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
        case QoreIROpcode::HashDerefDynamic: {
            // Dynamic hash/object dereference — mirrors QoreHashObjectDereferenceOperatorNode::evalImpl
            qore_type_t bt = left.getType();
            if (bt == NT_HASH) {
                const QoreHashNode* h = left.get<const QoreHashNode>();
                if (right.getType() == NT_LIST) {
                    return qore_hash_private::get(*h)->getSlice(right.get<const QoreListNode>(), xsink);
                }
                QoreStringValueHelper key(right);
                QoreValue v = h->getKeyValue(key->c_str(), xsink);
                if (xsink && *xsink) {
                    return QoreValue();
                }
                QoreValue out = v.refSelf();
                evaluateOwnedWeakReferenceResult(out, xsink);
                return out;
            }
            if (bt == NT_OBJECT) {
                QoreObject* o = const_cast<QoreObject*>(left.get<const QoreObject>());
                if (right.getType() == NT_LIST) {
                    return o->getSlice(right.get<const QoreListNode>(), xsink);
                }
                QoreStringValueHelper key(right);
                ValueHolder rv(o->evalMember(key->c_str(), xsink), xsink);
                if (xsink && *xsink) {
                    return QoreValue();
                }
                QoreValue out = rv.release();
                evaluateOwnedWeakReferenceResult(out, xsink);
                return out;
            }
            return QoreValue();
        }
        case QoreIROpcode::ListIndexDynamic: {
            // Dynamic list/container index: container[index]
            qore_type_t bt = left.getType();
            if (bt == NT_LIST || bt == NT_STRING || bt == NT_BINARY || bt == NT_BUFFER
                    || right.getType() == NT_LIST) {
                return QoreSquareBracketsOperatorNode::doSquareBrackets(left, right, true, true,
                    runtime_check_parse_option(PO_NEGATIVE_OFFSETS), xsink);
            }
            if (bt == NT_HASH) {
                // hash[key] is equivalent to hash{key}
                const QoreHashNode* h = left.get<const QoreHashNode>();
                QoreStringValueHelper kstr(right);
                QoreValue result = h->getKeyValue(kstr->c_str(), xsink);
                if (xsink && *xsink) {
                    return QoreValue();
                }
                QoreValue out = result.refSelf();
                evaluateOwnedWeakReferenceResult(out, xsink);
                return out;
            }
            return QoreValue();
        }
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
    QoreValue assign_value = value;
    ValueHolder eval_holder(xsink);
    if (!weak) {
        normalizeWeakReferenceForAssignment(assign_value, eval_holder, xsink);
        if (xsink && *xsink) {
            return QoreValue();
        }
    }
    // refSelf() before passing to assign() - assign() takes ownership via
    // assignAssume()/takeNode(), but value is a borrowed reference from the
    // caller's values map; without the extra ref, both the variable and the
    // values map would think they own the same single reference
    if (helper.assign(assign_value.refSelf(), "<lvalue>", true, weak)) {
        return QoreValue();
    }
    return helper.getReferencedValue();
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
// Inner function: perform += on an already-navigated LValueHelper.
// Used by both evalPlusEquals (AST path) and LValuePathCompound (path-based).
QoreValue doPlusEqualsOnLValue(LValueHelper& v, const QoreValue& right, ExceptionSink* xsink) {
    // values requiring dereferencing must be dereferenced outside the lock
    SafeDerefHelper sdh(xsink);

    qore_type_t vtype = v.getType();

    if (vtype == NT_NOTHING || vtype == NT_NULL) {
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

static QoreValue evalPlusEquals(const QoreValue& lvalue, const QoreValue& right, ExceptionSink* xsink) {
    ValueHolder right_holder(right.refSelf(), xsink);
    LValueHelper v(lvalue, xsink);
    if (!v) {
        return QoreValue();
    }
    return doPlusEqualsOnLValue(v, right, xsink);
}

// Inner function: perform -= on an already-navigated LValueHelper.
QoreValue doMinusEqualsOnLValue(LValueHelper& v, const QoreValue& right, ExceptionSink* xsink) {
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
        // Check if this is a timeout type, which uses integer arithmetic
        if (QoreTypeInfo::equal(v.getTypeInfo(), timeoutTypeInfo)) {
            // if the right side is also a date/timeout, convert to milliseconds
            if (right.getType() == NT_DATE) {
                int64 ms = right.get<const DateTimeNode>()->getRelativeMilliseconds();
                return v.minusEqualsBigInt(ms);
            } else {
                // Regular integer minus for timeout
                return v.minusEqualsBigInt(right.getAsBigInt());
            }
        }
        // Regular date arithmetic
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

static QoreValue evalMinusEquals(const QoreValue& lvalue, const QoreValue& right, ExceptionSink* xsink) {
    LValueHelper v(lvalue, xsink);
    if (!v) {
        return QoreValue();
    }
    return doMinusEqualsOnLValue(v, right, xsink);
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
