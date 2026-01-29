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
#include <qore/intern/ForEachStatement.h>
#include <qore/intern/FunctionCallNode.h>
#include <qore/intern/CallReferenceCallNode.h>
#include <qore/intern/OnBlockExitStatement.h>
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
        case QoreIROpcode::RegexMatchAny:
        case QoreIROpcode::RegexMatchBool:
        case QoreIROpcode::RegexExtractAny:
        case QoreIROpcode::RegexExtractList:
        case QoreIROpcode::RegexSubstAny:
        case QoreIROpcode::RegexSubstString:
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
            break;
    }
    if (xsink) {
        xsink->raiseException("IR-INTERPRETER-ERROR", "unsupported expression opcode");
    }
    return QoreValue();
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

static void ensureLocalInstantiated(LocalVar* var, std::unordered_set<const LocalVar*>& locals) {
    if (!var) {
        return;
    }
    if (locals.insert(var).second) {
        var->instantiate(0);
    }
}

static void cleanupInstantiatedLocals(const std::unordered_set<const LocalVar*>& locals, ExceptionSink* xsink) {
    for (auto* var : locals) {
        if (var) {
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
    helper.assign(value);
}

static void updateLocalVarFromLvalue(std::unordered_map<const void*, QoreValue>& locals,
        std::unordered_set<const LocalVar*>& instantiated_locals, const QoreValue& lvalue,
        const QoreValue& value, ExceptionSink* xsink) {
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
        ensureLocalInstantiated(var_ref->ref.id, instantiated_locals);
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

bool QoreIRInterpreter::execute(const QoreIRFunction& func, QoreValue& return_value, ExceptionSink* xsink,
        std::vector<std::string>* cleanup_log, const std::vector<QoreValue>* args,
        const std::vector<QoreValue>* closure) {
    std::unordered_map<uint32_t, QoreValue> values;
    std::vector<uint32_t> cleanup;
    std::unordered_map<const void*, QoreValue> locals;
    std::unordered_set<const LocalVar*> instantiated_locals;
    std::unordered_map<const void*, QoreValue> globals;
    std::unordered_map<const void*, QoreValue> threadlocals;
    std::unordered_map<const void*, QoreValue> closures;
    struct LocalInstantiationCleanup {
        std::unordered_set<const LocalVar*>& locals;
        ExceptionSink* xsink;
        ~LocalInstantiationCleanup() {
            cleanupInstantiatedLocals(locals, xsink);
        }
    } local_cleanup{instantiated_locals, xsink};
    if (func.blocks.empty()) {
        if (xsink) {
            xsink->raiseException("IR-EXEC-ERROR", "function has no basic blocks");
        }
        return false;
    }
    QoreIRBasicBlock* block = func.blocks.front().get();
    QoreIRBasicBlock* prev_block = nullptr;
    size_t ip = 0;

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
                DateTimeNode* dt = new DateTimeNode(cinst->constant.date_microseconds, cinst->constant.date_is_relative);
                values[cinst->result.id] = QoreValue(dt);
                cleanup.push_back(cinst->result.id);
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
                QoreValue res = QoreIRInterpreter::evalExpr(inv->invoke_opcode, inv->expr, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    prev_block = block;
                    block = inv->exception_target;
                    ip = 0;
                    break;
                }
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
                break;
            }
            case QoreIROpcode::BrIf: {
                auto* br = static_cast<QoreIRBranchIfInstruction*>(inst);
                QoreValue cond = getIRValue(values, br->condition);
                prev_block = block;
                block = cond.getAsBool() ? br->true_target : br->false_target;
                ip = 0;
                break;
            }
            case QoreIROpcode::LoadLocal: {
                auto* local_inst = static_cast<QoreIRLocalInstruction*>(inst);
                ensureLocalInstantiated(local_inst->local, instantiated_locals);
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
                    val = it != closures.end() ? it->second : QoreValue();
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
                ensureLocalInstantiated(local_inst->local, instantiated_locals);
                QoreValue val = getIRValue(values, local_inst->operands.front());
                storeValue(locals, local_inst->local, val, xsink);
                assignLocalVarValue(local_inst->local, val, xsink);
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
                ++ip;
                break;
            }
            case QoreIROpcode::LoadGlobal: {
                auto* var_inst = static_cast<QoreIRVarInstruction*>(inst);
                auto it = globals.find(var_inst->var);
                QoreValue val = it != globals.end() ? it->second : QoreValue();
                QoreValue out = val.hasNode() ? val.refSelf() : val;
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
                ++ip;
                break;
            }
            case QoreIROpcode::LoadThreadLocal: {
                auto* var_inst = static_cast<QoreIRVarInstruction*>(inst);
                auto it = threadlocals.find(var_inst->var);
                QoreValue val = it != threadlocals.end() ? it->second : QoreValue();
                QoreValue out = val.hasNode() ? val.refSelf() : val;
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
                ++ip;
                break;
            }
            case QoreIROpcode::Debug: {
                auto* debug_inst = static_cast<QoreIRDebugInstruction*>(inst);
                QoreValue stmt_return;
                int rc = QoreIRInterpreter::execStatement(QoreIROpcode::Debug, debug_inst->stmt,
                    stmt_return, xsink);
                if (rc || (xsink && *xsink)) {
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
            case QoreIROpcode::OnBlockExit: {
                auto* obe_inst = static_cast<QoreIROnBlockExitInstruction*>(inst);
                if (!obe_inst->stmt) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-ERROR", "on-block-exit requires a statement");
                    }
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                QoreValue stmt_return;
                int rc = const_cast<OnBlockExitStatement*>(obe_inst->stmt)->exec(stmt_return, xsink);
                if (rc || (xsink && *xsink)) {
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
                if (!guardPredicate(inst->opcode, value, guard_inst->type_info)) {
                    if (xsink) {
                        xsink->raiseException("IR-EXEC-GUARD-FAIL", "type guard failed");
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
            case QoreIROpcode::AddAssignAny:
            case QoreIROpcode::SubAssignInt:
            case QoreIROpcode::SubAssignAny:
            case QoreIROpcode::MulAssignInt:
            case QoreIROpcode::MulAssignAny:
            case QoreIROpcode::DivAssignInt:
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
            case QoreIROpcode::MapAny:
            case QoreIROpcode::MapInt:
            case QoreIROpcode::MapFloat:
            case QoreIROpcode::SelectAny:
            case QoreIROpcode::SelectInt:
            case QoreIROpcode::SelectFloat:
            case QoreIROpcode::RangeAny:
            case QoreIROpcode::RangeInt:
            case QoreIROpcode::RangeFloat:
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
            case QoreIROpcode::HashMapAny: {
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
            case QoreIROpcode::HashMapSelectAny: {
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
                updateLocalVarFromLvalue(locals, instantiated_locals, lval_inst->lvalue, res, xsink);
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
                values[lval_inst->result.id] = res;
                if (res.hasNode()) {
                    cleanup.push_back(lval_inst->result.id);
                }
                updateLocalVarFromLvalue(locals, instantiated_locals, lval_inst->lvalue, res, xsink);
                ++ip;
                break;
            }
            case QoreIROpcode::PreIncLValue:
            case QoreIROpcode::PreDecLValue:
            case QoreIROpcode::PostIncLValue:
            case QoreIROpcode::PostDecLValue:
            case QoreIROpcode::ShiftLValue:
            case QoreIROpcode::UnshiftLValue: {
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
                QoreValue updated = QoreIRInterpreter::evalLValueLoad(lval_inst->lvalue, xsink);
                if (xsink && *xsink) {
                    cleanupValues(values, cleanup, xsink, true, cleanup_log);
                    cleanupStoredValues(locals, nullptr);
                    cleanupStoredValues(globals, nullptr);
                    cleanupStoredValues(threadlocals, nullptr);
                    cleanupStoredValues(closures, nullptr);
                    return false;
                }
                values[lval_inst->result.id] = updated;
                if (updated.hasNode()) {
                    cleanup.push_back(lval_inst->result.id);
                }
                updateLocalVarFromLvalue(locals, instantiated_locals, lval_inst->lvalue, updated, xsink);
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
            case QoreIROpcode::ShrAssignLValue: {
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
                values[lval_inst->result.id] = res;
                if (res.hasNode()) {
                    cleanup.push_back(lval_inst->result.id);
                }
                updateLocalVarFromLvalue(locals, instantiated_locals, lval_inst->lvalue, res, xsink);
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
                values[lval_inst->result.id] = res;
                if (res.hasNode()) {
                    cleanup.push_back(lval_inst->result.id);
                }
                updateLocalVarFromLvalue(locals, instantiated_locals, lval_inst->lvalue, res, xsink);
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
                    QoreListNode* arg_list = buildArgList(values, expr_inst->operands, 0, xsink);
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
        case QoreIROpcode::RegexMatchAny:
            case QoreIROpcode::RegexMatchBool:
            case QoreIROpcode::RegexExtractAny:
            case QoreIROpcode::RegexExtractList:
            case QoreIROpcode::RegexSubstAny:
            case QoreIROpcode::RegexSubstString:
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
                    QoreValue owned_arg = arg.hasNode() ? arg.refSelf() : arg;
                    xsink->raiseExceptionArg("IR-EXEC-THROW", owned_arg, "throw");
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
                cleanupValues(values, cleanup, xsink, false, cleanup_log);
                cleanupStoredValues(locals, xsink);
                cleanupStoredValues(globals, xsink);
                cleanupStoredValues(threadlocals, xsink);
                cleanupStoredValues(closures, xsink);
                return true;
            }
            case QoreIROpcode::ReturnNothing:
                return_value = QoreValue();
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
            return node.eval(needs_deref, xsink);
        }
        case QoreIROpcode::UnaryMinusInt:
            return QoreValue(-value.getAsBigInt());
        case QoreIROpcode::UnaryMinusFloat:
            return QoreValue(-value.getAsFloat());
        case QoreIROpcode::UnaryMinusAny: {
            bool needs_deref = true;
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
            bool needs_deref = true;
            QorePlusOperatorNode node(nullptr, left.refSelf(), right.refSelf());
            return node.eval(needs_deref, xsink);
        }
        case QoreIROpcode::SubInt:
            return QoreValue(left.getAsBigInt() - right.getAsBigInt());
        case QoreIROpcode::SubFloat:
            return QoreValue(left.getAsFloat() - right.getAsFloat());
        case QoreIROpcode::SubAny: {
            bool needs_deref = true;
            QoreMinusOperatorNode node(nullptr, left.refSelf(), right.refSelf());
            return node.eval(needs_deref, xsink);
        }
        case QoreIROpcode::MulInt:
            return QoreValue(left.getAsBigInt() * right.getAsBigInt());
        case QoreIROpcode::MulFloat:
            return QoreValue(left.getAsFloat() * right.getAsFloat());
        case QoreIROpcode::MulAny: {
            bool needs_deref = true;
            QoreMultiplicationOperatorNode node(nullptr, left.refSelf(), right.refSelf());
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
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreBinaryAndOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::OrInt:
            return QoreValue(left.getAsBigInt() | right.getAsBigInt());
        case QoreIROpcode::OrAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreBinaryOrOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::XorInt:
            return QoreValue(left.getAsBigInt() ^ right.getAsBigInt());
        case QoreIROpcode::XorAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreBinaryXorOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::AddAssignInt:
            return QoreValue(left.getAsBigInt() + right.getAsBigInt());
        case QoreIROpcode::AddAssignAny: {
            bool needs_deref = true;
            QorePlusOperatorNode node(nullptr, left.refSelf(), right.refSelf());
            return node.eval(needs_deref, xsink);
        }
        case QoreIROpcode::SubAssignInt:
            return QoreValue(left.getAsBigInt() - right.getAsBigInt());
        case QoreIROpcode::SubAssignAny: {
            bool needs_deref = true;
            QoreMinusOperatorNode node(nullptr, left.refSelf(), right.refSelf());
            return node.eval(needs_deref, xsink);
        }
        case QoreIROpcode::MulAssignInt:
            return QoreValue(left.getAsBigInt() * right.getAsBigInt());
        case QoreIROpcode::MulAssignAny: {
            bool needs_deref = true;
            QoreMultiplicationOperatorNode node(nullptr, left.refSelf(), right.refSelf());
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
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreBinaryAndOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::OrAssignInt:
            return QoreValue(left.getAsBigInt() | right.getAsBigInt());
        case QoreIROpcode::OrAssignAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreBinaryOrOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::XorAssignInt:
            return QoreValue(left.getAsBigInt() ^ right.getAsBigInt());
        case QoreIROpcode::XorAssignAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreBinaryXorOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::ShlInt:
            return QoreValue(left.getAsBigInt() << right.getAsBigInt());
        case QoreIROpcode::ShlAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreShiftLeftOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::ShrInt:
            return QoreValue(left.getAsBigInt() >> right.getAsBigInt());
        case QoreIROpcode::ShrAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreShiftRightOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::ShlAssignInt:
            return QoreValue(left.getAsBigInt() << right.getAsBigInt());
        case QoreIROpcode::ShlAssignAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreShiftLeftEqualsOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::ShrAssignInt:
            return QoreValue(left.getAsBigInt() >> right.getAsBigInt());
        case QoreIROpcode::ShrAssignAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreShiftRightEqualsOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::FoldlAny:
        case QoreIROpcode::FoldlInt:
        case QoreIROpcode::FoldlFloat: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreFoldlOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::FoldrAny:
        case QoreIROpcode::FoldrInt:
        case QoreIROpcode::FoldrFloat: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreFoldrOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::MapAny:
        case QoreIROpcode::MapInt:
        case QoreIROpcode::MapFloat: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreMapOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::SelectAny:
        case QoreIROpcode::SelectInt:
        case QoreIROpcode::SelectFloat: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreSelectOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::RangeAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreRangeOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::RangeInt:
        case QoreIROpcode::RangeFloat: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreRangeOperatorNode(nullptr, left.refSelf(), right.refSelf())), xsink);
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
        case QoreIROpcode::RangeSliceAny:
        case QoreIROpcode::RangeSliceInt:
        case QoreIROpcode::RangeSliceFloat: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreSquareBracketsRangeOperatorNode(nullptr,
                first.refSelf(), second.refSelf(), third.refSelf())), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::MapSelectAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreMapSelectOperatorNode(nullptr,
                first.refSelf(), second.refSelf(), third.refSelf())), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::HashMapAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreHashMapOperatorNode(nullptr,
                first.refSelf(), second.refSelf(), third.refSelf())), xsink);
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

QoreValue QoreIRInterpreter::evalQuaternary(QoreIROpcode op, const QoreValue& first, const QoreValue& second,
        const QoreValue& third, const QoreValue& fourth, ExceptionSink* xsink) {
    switch (op) {
        case QoreIROpcode::HashMapSelectAny: {
            bool needs_deref = true;
            ValueHolder node(QoreValue(new QoreHashMapSelectOperatorNode(nullptr,
                first.refSelf(), second.refSelf(), third.refSelf(), fourth.refSelf())), xsink);
            return node->eval(needs_deref, xsink);
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
    if (helper.assign(value, "<lvalue>")) {
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
    bool needs_deref = true;
    QoreValue lvalue_ref = lvalue.refSelf();
    switch (op) {
        case QoreIROpcode::AddAssignLValue: {
            ValueHolder node(QoreValue(new QorePlusEqualsOperatorNode(nullptr, lvalue_ref, right.refSelf())), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::SubAssignLValue: {
            ValueHolder node(QoreValue(new QoreMinusEqualsOperatorNode(nullptr, lvalue_ref, right.refSelf())), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::MulAssignLValue: {
            ValueHolder node(QoreValue(new QoreMultiplyEqualsOperatorNode(nullptr, lvalue_ref, right.refSelf())), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::DivAssignLValue: {
            ValueHolder node(QoreValue(new QoreDivideEqualsOperatorNode(nullptr, lvalue_ref, right.refSelf())), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::ModAssignLValue: {
            ValueHolder node(QoreValue(new QoreModuloEqualsOperatorNode(nullptr, lvalue_ref, right.refSelf())), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::AndAssignLValue: {
            ValueHolder node(QoreValue(new QoreAndEqualsOperatorNode(nullptr, lvalue_ref, right.refSelf())), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::OrAssignLValue: {
            ValueHolder node(QoreValue(new QoreOrEqualsOperatorNode(nullptr, lvalue_ref, right.refSelf())), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::XorAssignLValue: {
            ValueHolder node(QoreValue(new QoreXorEqualsOperatorNode(nullptr, lvalue_ref, right.refSelf())), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::ShlAssignLValue: {
            ValueHolder node(QoreValue(new QoreShiftLeftEqualsOperatorNode(nullptr, lvalue_ref, right.refSelf())), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::ShrAssignLValue: {
            ValueHolder node(QoreValue(new QoreShiftRightEqualsOperatorNode(nullptr, lvalue_ref, right.refSelf())), xsink);
            return node->eval(needs_deref, xsink);
        }
        case QoreIROpcode::UnshiftLValue: {
            ValueHolder node(QoreValue(new QoreUnshiftOperatorNode(nullptr, lvalue_ref, right.refSelf())), xsink);
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
    bool needs_deref = true;
    QoreValue lvalue_ref = lvalue.refSelf();
    switch (op) {
        case QoreIROpcode::SpliceLValue: {
            ValueHolder node(QoreValue(new QoreSpliceOperatorNode(nullptr, lvalue_ref,
                first.refSelf(), second.refSelf(), third.refSelf())), xsink);
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
