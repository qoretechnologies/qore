/* -*- indent-tabs-mode: nil -*- */
/*
    QoreIRPrinter.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#include <qore/intern/QoreIRPrinter.h>

#include <ostream>

#include <qore/intern/QoreIR.h>
#include <qore/intern/LocalVar.h>
#include <qore/intern/Variable.h>

static const char* opcodeName(QoreIROpcode op) {
    switch (op) {
        case QoreIROpcode::ConstInt: return "const.int";
        case QoreIROpcode::ConstFloat: return "const.float";
        case QoreIROpcode::ConstBool: return "const.bool";
        case QoreIROpcode::ConstNothing: return "const.nothing";
        case QoreIROpcode::ConstString: return "const.string";
        case QoreIROpcode::ConstDate: return "const.date";
        case QoreIROpcode::AddInt: return "add.int";
        case QoreIROpcode::AddFloat: return "add.float";
        case QoreIROpcode::AddAny: return "add.any";
        case QoreIROpcode::SubInt: return "sub.int";
        case QoreIROpcode::SubFloat: return "sub.float";
        case QoreIROpcode::SubAny: return "sub.any";
        case QoreIROpcode::MulInt: return "mul.int";
        case QoreIROpcode::MulFloat: return "mul.float";
        case QoreIROpcode::MulAny: return "mul.any";
        case QoreIROpcode::DivInt: return "div.int";
        case QoreIROpcode::DivFloat: return "div.float";
        case QoreIROpcode::DivAny: return "div.any";
        case QoreIROpcode::ModInt: return "mod.int";
        case QoreIROpcode::ModAny: return "mod.any";
        case QoreIROpcode::AndInt: return "and.int";
        case QoreIROpcode::AndAny: return "and.any";
        case QoreIROpcode::OrInt: return "or.int";
        case QoreIROpcode::OrAny: return "or.any";
        case QoreIROpcode::XorInt: return "xor.int";
        case QoreIROpcode::XorAny: return "xor.any";
        case QoreIROpcode::ShlInt: return "shl.int";
        case QoreIROpcode::ShlAny: return "shl.any";
        case QoreIROpcode::ShrInt: return "shr.int";
        case QoreIROpcode::ShrAny: return "shr.any";
        case QoreIROpcode::ShlAssignInt: return "shl.assign.int";
        case QoreIROpcode::ShlAssignAny: return "shl.assign.any";
        case QoreIROpcode::ShrAssignInt: return "shr.assign.int";
        case QoreIROpcode::ShrAssignAny: return "shr.assign.any";
        case QoreIROpcode::AddAssignInt: return "add.assign.int";
        case QoreIROpcode::AddAssignAny: return "add.assign.any";
        case QoreIROpcode::SubAssignInt: return "sub.assign.int";
        case QoreIROpcode::SubAssignAny: return "sub.assign.any";
        case QoreIROpcode::MulAssignInt: return "mul.assign.int";
        case QoreIROpcode::MulAssignAny: return "mul.assign.any";
        case QoreIROpcode::DivAssignInt: return "div.assign.int";
        case QoreIROpcode::DivAssignAny: return "div.assign.any";
        case QoreIROpcode::ModAssignInt: return "mod.assign.int";
        case QoreIROpcode::ModAssignAny: return "mod.assign.any";
        case QoreIROpcode::AndAssignInt: return "and.assign.int";
        case QoreIROpcode::AndAssignAny: return "and.assign.any";
        case QoreIROpcode::OrAssignInt: return "or.assign.int";
        case QoreIROpcode::OrAssignAny: return "or.assign.any";
        case QoreIROpcode::XorAssignInt: return "xor.assign.int";
        case QoreIROpcode::XorAssignAny: return "xor.assign.any";
        case QoreIROpcode::LoadLValue: return "load.lvalue";
        case QoreIROpcode::StoreLValue: return "store.lvalue";
        case QoreIROpcode::PreIncLValue: return "preinc.lvalue";
        case QoreIROpcode::PreDecLValue: return "predec.lvalue";
        case QoreIROpcode::PostIncLValue: return "postinc.lvalue";
        case QoreIROpcode::PostDecLValue: return "postdec.lvalue";
        case QoreIROpcode::AddAssignLValue: return "add.assign.lvalue";
        case QoreIROpcode::SubAssignLValue: return "sub.assign.lvalue";
        case QoreIROpcode::MulAssignLValue: return "mul.assign.lvalue";
        case QoreIROpcode::DivAssignLValue: return "div.assign.lvalue";
        case QoreIROpcode::ModAssignLValue: return "mod.assign.lvalue";
        case QoreIROpcode::AndAssignLValue: return "and.assign.lvalue";
        case QoreIROpcode::OrAssignLValue: return "or.assign.lvalue";
        case QoreIROpcode::XorAssignLValue: return "xor.assign.lvalue";
        case QoreIROpcode::ShlAssignLValue: return "shl.assign.lvalue";
        case QoreIROpcode::ShrAssignLValue: return "shr.assign.lvalue";
        case QoreIROpcode::ShiftLValue: return "shift.lvalue";
        case QoreIROpcode::UnshiftLValue: return "unshift.lvalue";
        case QoreIROpcode::SpliceLValue: return "splice.lvalue";
        case QoreIROpcode::ExtractAny: return "extract.any";
        case QoreIROpcode::RemoveAny: return "remove.any";
        case QoreIROpcode::KeysAny: return "keys.any";
        case QoreIROpcode::RegexMatchAny: return "regex.match.any";
        case QoreIROpcode::RegexExtractAny: return "regex.extract.any";
        case QoreIROpcode::RegexSubstAny: return "regex.subst.any";
        case QoreIROpcode::ExistsAny: return "exists.any";
        case QoreIROpcode::ElementsAny: return "elements.any";
        case QoreIROpcode::DotEvalAny: return "dot.eval.any";
        case QoreIROpcode::Foreach: return "foreach";
        case QoreIROpcode::OnBlockExit: return "on.block.exit";
        case QoreIROpcode::ThreadExit: return "thread.exit";
        case QoreIROpcode::Debug: return "debug";
        case QoreIROpcode::EqInt: return "eq.int";
        case QoreIROpcode::EqAny: return "eq.any";
        case QoreIROpcode::NeInt: return "ne.int";
        case QoreIROpcode::NeAny: return "ne.any";
        case QoreIROpcode::EqHard: return "eq.hard";
        case QoreIROpcode::NeHard: return "ne.hard";
        case QoreIROpcode::LtInt: return "lt.int";
        case QoreIROpcode::LtFloat: return "lt.float";
        case QoreIROpcode::LtAny: return "lt.any";
        case QoreIROpcode::LeInt: return "le.int";
        case QoreIROpcode::LeFloat: return "le.float";
        case QoreIROpcode::LeAny: return "le.any";
        case QoreIROpcode::GtInt: return "gt.int";
        case QoreIROpcode::GtFloat: return "gt.float";
        case QoreIROpcode::GtAny: return "gt.any";
        case QoreIROpcode::GeInt: return "ge.int";
        case QoreIROpcode::GeFloat: return "ge.float";
        case QoreIROpcode::GeAny: return "ge.any";
        case QoreIROpcode::CmpInt: return "cmp.int";
        case QoreIROpcode::CmpFloat: return "cmp.float";
        case QoreIROpcode::CmpAny: return "cmp.any";
        case QoreIROpcode::ToBool: return "to.bool";
        case QoreIROpcode::Not: return "not";
        case QoreIROpcode::IsNullOrNothing: return "is.null-or-nothing";
        case QoreIROpcode::Phi: return "phi";
        case QoreIROpcode::UnaryPlusAny: return "unary.plus.any";
        case QoreIROpcode::UnaryMinusInt: return "unary.minus.int";
        case QoreIROpcode::UnaryMinusFloat: return "unary.minus.float";
        case QoreIROpcode::UnaryMinusAny: return "unary.minus.any";
        case QoreIROpcode::FoldlAny: return "foldl.any";
        case QoreIROpcode::FoldrAny: return "foldr.any";
        case QoreIROpcode::MapAny: return "map.any";
        case QoreIROpcode::SelectAny: return "select.any";
        case QoreIROpcode::MapSelectAny: return "map.select.any";
        case QoreIROpcode::HashMapAny: return "hash.map.any";
        case QoreIROpcode::HashMapSelectAny: return "hash.map.select.any";
        case QoreIROpcode::RangeAny: return "range.any";
        case QoreIROpcode::RangeSliceAny: return "range.slice.any";
        case QoreIROpcode::CastAny: return "cast.any";
        case QoreIROpcode::Br: return "br";
        case QoreIROpcode::BrIf: return "br.if";
        case QoreIROpcode::Return: return "return";
        case QoreIROpcode::ReturnNothing: return "return.nothing";
        case QoreIROpcode::LoadLocal: return "load.local";
        case QoreIROpcode::StoreLocal: return "store.local";
        case QoreIROpcode::LoadArg: return "load.arg";
        case QoreIROpcode::LoadClosure: return "load.closure";
        case QoreIROpcode::StoreClosure: return "store.closure";
        case QoreIROpcode::LoadGlobal: return "load.global";
        case QoreIROpcode::StoreGlobal: return "store.global";
        case QoreIROpcode::LoadThreadLocal: return "load.threadlocal";
        case QoreIROpcode::StoreThreadLocal: return "store.threadlocal";
        case QoreIROpcode::Call: return "call";
        case QoreIROpcode::CallIndirect: return "call.indirect";
        case QoreIROpcode::CallMethod: return "call.method";
        case QoreIROpcode::CallStatic: return "call.static";
        case QoreIROpcode::Invoke: return "invoke";
        case QoreIROpcode::GuardInt: return "guard.int";
        case QoreIROpcode::GuardFloat: return "guard.float";
        case QoreIROpcode::GuardType: return "guard.type";
        case QoreIROpcode::GuardNotNothing: return "guard.not-nothing";
        case QoreIROpcode::LandingPad: return "landingpad";
        case QoreIROpcode::CatchException: return "catch.exception";
        case QoreIROpcode::Rethrow: return "rethrow";
        case QoreIROpcode::Throw: return "throw";
        case QoreIROpcode::InvokeSimError: return "invoke.sim.error";
        case QoreIROpcode::Incref: return "incref";
        case QoreIROpcode::Decref: return "decref";
        case QoreIROpcode::DecrefNoThrow: return "decref.nothrow";
    }
    return "unknown";
}

static void printConstant(const QoreIRConstInstruction& inst, std::ostream& out) {
    switch (inst.constant.kind) {
        case QoreIRConstant::Kind::Int:
            out << inst.constant.int_value;
            break;
        case QoreIRConstant::Kind::Float:
            out << inst.constant.float_value;
            break;
        case QoreIRConstant::Kind::Bool:
            out << (inst.constant.bool_value ? "true" : "false");
            break;
        case QoreIRConstant::Kind::Nothing:
            out << "nothing";
            break;
        case QoreIRConstant::Kind::String:
            out << "\"" << inst.constant.string_value << "\"";
            break;
        case QoreIRConstant::Kind::Date:
            out << (inst.constant.date_is_relative ? "rel:" : "abs:") << inst.constant.date_microseconds;
            break;
    }
}

void QoreIRPrinter::print(const QoreIRFunction& func, std::ostream& out) {
    out << "func @" << func.name << "\n";
    for (const auto& block : func.blocks) {
        out << block->name << ":\n";
        for (const auto& inst : block->instructions) {
            out << "  " << opcodeName(inst->opcode);
            if (inst->opcode == QoreIROpcode::Invoke) {
                auto* invoke_inst = dynamic_cast<const QoreIRInvokeInstruction*>(inst.get());
                if (invoke_inst && invoke_inst->invoke_opcode != QoreIROpcode::Invoke) {
                    out << "." << opcodeName(invoke_inst->invoke_opcode);
                }
            }
            if (inst->opcode == QoreIROpcode::LoadLocal || inst->opcode == QoreIROpcode::StoreLocal
                    || inst->opcode == QoreIROpcode::LoadClosure || inst->opcode == QoreIROpcode::StoreClosure) {
                auto* local_inst = dynamic_cast<const QoreIRLocalInstruction*>(inst.get());
                if (local_inst && local_inst->local) {
                    out << " @" << local_inst->local->getName();
                }
            } else if (inst->opcode == QoreIROpcode::LoadGlobal || inst->opcode == QoreIROpcode::StoreGlobal
                    || inst->opcode == QoreIROpcode::LoadThreadLocal
                    || inst->opcode == QoreIROpcode::StoreThreadLocal) {
                auto* var_inst = dynamic_cast<const QoreIRVarInstruction*>(inst.get());
                if (var_inst && var_inst->var) {
                    out << " $" << var_inst->var->getName();
                }
            } else if (inst->opcode == QoreIROpcode::LoadLValue || inst->opcode == QoreIROpcode::StoreLValue
                    || inst->opcode == QoreIROpcode::PreIncLValue || inst->opcode == QoreIROpcode::PreDecLValue
                    || inst->opcode == QoreIROpcode::PostIncLValue || inst->opcode == QoreIROpcode::PostDecLValue
                    || inst->opcode == QoreIROpcode::AddAssignLValue || inst->opcode == QoreIROpcode::SubAssignLValue
                    || inst->opcode == QoreIROpcode::MulAssignLValue || inst->opcode == QoreIROpcode::DivAssignLValue
                    || inst->opcode == QoreIROpcode::ModAssignLValue || inst->opcode == QoreIROpcode::AndAssignLValue
                    || inst->opcode == QoreIROpcode::OrAssignLValue || inst->opcode == QoreIROpcode::XorAssignLValue
                    || inst->opcode == QoreIROpcode::ShlAssignLValue
                    || inst->opcode == QoreIROpcode::ShrAssignLValue
                    || inst->opcode == QoreIROpcode::ShiftLValue
                    || inst->opcode == QoreIROpcode::UnshiftLValue
                    || inst->opcode == QoreIROpcode::SpliceLValue) {
                auto* lv_inst = dynamic_cast<const QoreIRLValueInstruction*>(inst.get());
                if (lv_inst && lv_inst->lvalue.hasNode()) {
                    out << " <lvalue>";
                }
            } else if (inst->opcode == QoreIROpcode::Call
                    || inst->opcode == QoreIROpcode::CallIndirect
                    || inst->opcode == QoreIROpcode::CallMethod
                    || inst->opcode == QoreIROpcode::CallStatic
                    || inst->opcode == QoreIROpcode::CastAny
                    || inst->opcode == QoreIROpcode::ExtractAny
                    || inst->opcode == QoreIROpcode::RemoveAny
                    || inst->opcode == QoreIROpcode::KeysAny) {
                auto* expr_inst = dynamic_cast<const QoreIRExprInstruction*>(inst.get());
                if (expr_inst && expr_inst->expr.hasNode()) {
                    out << " <expr>";
                }
            } else if (inst->opcode == QoreIROpcode::Invoke) {
                auto* invoke_inst = dynamic_cast<const QoreIRInvokeInstruction*>(inst.get());
                if (invoke_inst && invoke_inst->expr.hasNode()) {
                    out << " <expr>";
                }
            }
            if (inst->result.isValid()) {
                out << " -> %" << inst->result.id;
            }
            if (inst->opcode != QoreIROpcode::Phi && !inst->operands.empty()) {
                out << " ";
                for (size_t i = 0; i < inst->operands.size(); ++i) {
                    if (i) {
                        out << ", ";
                    }
                    out << "%" << inst->operands[i].id;
                }
            }
            if (inst->opcode == QoreIROpcode::ConstInt
                    || inst->opcode == QoreIROpcode::ConstFloat
                    || inst->opcode == QoreIROpcode::ConstBool
                    || inst->opcode == QoreIROpcode::ConstNothing
                    || inst->opcode == QoreIROpcode::ConstString
                    || inst->opcode == QoreIROpcode::ConstDate) {
                auto* cinst = dynamic_cast<const QoreIRConstInstruction*>(inst.get());
                if (cinst) {
                    out << " = ";
                    printConstant(*cinst, out);
                }
            } else if (inst->opcode == QoreIROpcode::Br) {
                auto* br = dynamic_cast<const QoreIRBranchInstruction*>(inst.get());
                if (br && br->target) {
                    out << " -> " << br->target->name;
                }
            } else if (inst->opcode == QoreIROpcode::BrIf) {
                auto* br = dynamic_cast<const QoreIRBranchIfInstruction*>(inst.get());
                if (br) {
                    out << " %" << br->condition.id;
                    if (br->true_target) {
                        out << " then " << br->true_target->name;
                    }
                    if (br->false_target) {
                        out << " else " << br->false_target->name;
                    }
                }
            } else if (inst->opcode == QoreIROpcode::Invoke) {
                auto* invoke_inst = dynamic_cast<const QoreIRInvokeInstruction*>(inst.get());
                if (invoke_inst) {
                    if (invoke_inst->normal_target) {
                        out << " then " << invoke_inst->normal_target->name;
                    }
                    if (invoke_inst->exception_target) {
                        out << " else " << invoke_inst->exception_target->name;
                    }
                }
            } else if (inst->opcode == QoreIROpcode::Phi) {
                auto* phi = dynamic_cast<const QoreIRPhiInstruction*>(inst.get());
                if (phi) {
                    out << " ";
                    for (size_t i = 0; i < phi->incoming.size(); ++i) {
                        if (i) {
                            out << ", ";
                        }
                        out << "[%" << phi->incoming[i].value.id;
                        if (phi->incoming[i].block) {
                            out << ", " << phi->incoming[i].block->name;
                        }
                        out << "]";
                    }
                }
            } else if (inst->opcode == QoreIROpcode::Return || inst->opcode == QoreIROpcode::ReturnNothing) {
                auto* ret = dynamic_cast<const QoreIRReturnInstruction*>(inst.get());
                if (ret && ret->has_value) {
                    out << " %" << ret->value.id;
                }
            }
            out << "\n";
        }
    }
}
