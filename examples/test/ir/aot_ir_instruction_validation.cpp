/* -*- mode: c++; indent-tabs-mode: nil -*- */
// Copyright (C) 2026 Qore Technologies, s.r.o.
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <qore/QoreStringNode.h>
#include <qore/intern/LocalVar.h>
#include <qore/intern/QoreIR.h>
#include <qore/intern/QoreAOTBinary.h>
#include <qore/intern/QoreAOTInstRegistry.h>
#include <qore/intern/qore_thread_intern.h>

#include <algorithm>
#include <cstdio>
#include <functional>

using O = QoreIROpcode;
using G = QoreIRInstGroup;
using Payload = std::function<void(QoreAOTBinaryWriter&)>;

// Real wire payloads, including intentionally inconsistent opcode/group/count/kind fields.  Do not
// execute malformed IR or manufacture test-only opcodes.  The production function reader must reject it.
static bool check(QoreProgram* pgm, const char* label, O opcode, uint8_t group,
        const std::vector<uint32_t>& operands, const Payload& payload, const char* expected,
        bool tag_only = false, bool wide = false, uint16_t version = QORE_AOT_BINARY_VERSION,
        uint32_t result_id = 3, uint64_t features = 0, const Payload* stmt_loc = nullptr,
        const Payload* unwind = nullptr, const std::function<bool(const QoreIRFunction&)>* inspect = nullptr,
        const Payload* exit_restores = nullptr,
        QoreIRExceptionExitRestore expected_restore = QoreIRExceptionExitRestore::None) {
    QoreAOTBinaryWriter writer;
    writer.feature_flags = features | (wide ? QORE_AOT_FEAT_WIDE_IR_OPERANDS : 0);
    uint32_t section = writer.beginSection(QoreAOTSectionType::TOPLEVEL);
    writer.writeStringRef("instruction-validation");
    writer.writeU32(32); // maximum value ID
    writer.writeU32(unwind ? 2 : 0); // maximum local slot ID
    writer.writeU32(0); // guards
    writer.writeStringRef(""); // return type
    writer.writeU32(0); // parent slots
    writer.writeU16(1); // blocks
    writer.writeU16(unwind ? 2 : 0); // local variables
    writer.writeU16(0); // body locals
    if (unwind) {
        // Sparse slots distinguish an unknown in-range ID from one beyond the frame.
        for (uint32_t slot : {0u, 2u}) {
            writer.writeStringRef(slot ? "inner" : "outer");
            writer.writeStringRef("auto");
            writer.writeU32(slot);
            writer.writeU8(static_cast<uint8_t>(AOTIRLocalBinding::NONE));
            writer.writeU32(UINT32_MAX);
        }
    }
    if (version >= QORE_AOT_HANDLER_OWN_LOCALS_VERSION) {
        writer.writeU32(0); // handler-own slots
    }
    writer.writeStringRef("entry");
    writer.writeU8(0); // block flags
    // the instruction is in a function that can run (qore#6468): a constant defines each value that it uses, and a
    // return ends the block
    std::vector<uint32_t> defined;
    if (!tag_only) {
        for (uint32_t id : operands) {
            if (id && id <= 32 && id != result_id && std::find(defined.begin(), defined.end(), id) == defined.end()) {
                defined.push_back(id);
            }
        }
    }
    // the base fields after the opcode and the group, and the source location, of a simple instruction
    auto writeSimple = [&](O op, G grp, uint32_t result, const std::function<void()>& simple_payload) {
        writer.writeU16(static_cast<uint16_t>(op));
        writer.writeU8(static_cast<uint8_t>(grp));
        writer.writeU32(result);
        if (wide) {
            writer.writeU16(0);
        } else {
            writer.writeU8(0);
        }
        writer.writeU16(0xffff); // no exception target
        simple_payload();
        if (version >= 11) {
            writer.writeU32(0);
            writer.writeU32(0);
        } else {
            writer.writeU16(0);
            writer.writeU16(0);
        }
        writer.writeStringRef("");
        // no runtime location (QORE_AOT_STATEMENT_LOCATIONS_VERSION)
        if (version >= QORE_AOT_STATEMENT_LOCATIONS_VERSION) {
            writer.writeU8(static_cast<uint8_t>(QoreAOTStmtLocTag::None));
        }
        // no temp scopes (QORE_AOT_TEMP_SCOPES_VERSION, qore#6742)
        if (version >= QORE_AOT_TEMP_SCOPES_VERSION) {
            writer.writeU8(0);
            writer.writeU8(0);
        }
    };
    writer.writeU16(static_cast<uint16_t>(tag_only ? 1 : defined.size() + 2)); // instructions
    for (uint32_t id : defined) {
        writeSimple(O::ConstNothing, G::Const, id, [&]() {
            writer.writeU8(static_cast<uint8_t>(QoreIRConstant::Kind::Nothing));
        });
    }
    writer.writeU16(static_cast<uint16_t>(opcode));
    writer.writeU8(group);
    if (!tag_only) {
        writer.writeU32(result_id);
        if (wide) {
            writer.writeU16(static_cast<uint16_t>(operands.size()));
        } else {
            writer.writeU8(static_cast<uint8_t>(operands.size()));
        }
        for (uint32_t id : operands) {
            writer.writeU32(id);
        }
        writer.writeU16(0xffff); // no exception target
        payload(writer);
        if (version >= 11) {
            writer.writeU32(0); // source start/end line
            writer.writeU32(0);
        } else {
            writer.writeU16(0);
            writer.writeU16(0);
        }
        writer.writeStringRef("");
        // the runtime location (QORE_AOT_STATEMENT_LOCATIONS_VERSION): none, unless the case gives one
        if (version >= QORE_AOT_STATEMENT_LOCATIONS_VERSION) {
            if (stmt_loc) {
                (*stmt_loc)(writer);
            } else {
                writer.writeU8(static_cast<uint8_t>(QoreAOTStmtLocTag::None));
            }
        }
        // no temp scopes (QORE_AOT_TEMP_SCOPES_VERSION, qore#6742)
        if (version >= QORE_AOT_TEMP_SCOPES_VERSION) {
            writer.writeU8(0);
            writer.writeU8(0);
        }
        writeSimple(O::ReturnNothing, G::Return, 0, [&]() {
            writer.writeU8(0); // no value
        });
    }
    if (version >= QORE_AOT_CATCH_TABLES_VERSION) {
        writer.writeU32(0); // catch-exit depths (qore#6733)
        writer.writeU32(0); // catch-cleanup results
    }
    // the exit restorations (QORE_AOT_EXIT_RESTORE_VERSION, qore#7379): none, unless the case gives them
    if (version >= QORE_AOT_EXIT_RESTORE_VERSION) {
        if (exit_restores) {
            (*exit_restores)(writer);
        } else {
            writer.writeU32(0);
        }
    }
    if (version >= QORE_AOT_LOCAL_UNWIND_LOCATIONS_VERSION) {
        if (unwind) {
            (*unwind)(writer);
        } else {
            writer.writeU16(0);
        }
    }
    writer.endSection(section);
    QoreAOTBinaryHeader header{};
    header.magic = QORE_AOT_BINARY_MAGIC;
    header.version = version;
    header.feature_flags = writer.feature_flags;
    std::vector<uint8_t> data;
    if (!writer.finalize(header, data)) {
        fprintf(stderr, "%s: cannot finalize payload\n", label);
        return false;
    }
    QoreAOTBinaryReader reader;
    std::string error;
    if (!reader.open(data.data(), data.size(), error)) {
        if (expected && error.find(expected) != std::string::npos) {
            return true;
        }
        fprintf(stderr, "%s: cannot open payload: %s\n", label, error.c_str());
        return false;
    }
    const auto* sh = reader.findSection(QoreAOTSectionType::TOPLEVEL);
    if (!sh) {
        fprintf(stderr, "%s: missing payload section\n", label);
        return false;
    }
    const uint8_t* ptr = reader.getSectionData(*sh);
    const uint8_t* end = ptr + sh->size;
    unsigned expression_reads = 0;
    auto ir = deserializeIRFunction(reader, ptr, end, pgm,
        [&expression_reads](const QoreAOTBinaryReader& r, const uint8_t*& p, const uint8_t* end,
                std::string& error) {
            ++expression_reads;
            if (p != end) {
                auto kind = static_cast<AOTExprKind>(*p++);
                if (kind == AOTExprKind::CONST_NOTHING) {
                    return QoreValue();
                }
                if (kind == AOTExprKind::CONST_STRING && end - p >= 4) {
                    return QoreValue(new QoreStringNode(r.readStringRef(p)));
                }
            }
            error = "invalid fixture expression";
            return QoreValue();
        }, nullptr, error);
    bool ok = expected ? !ir && error.find(expected) != std::string::npos
        : ir && error.empty() && ptr == end;
    // the instruction under test, after the constants of its operands, has the exit restoration that was read
    if (ok && !expected && exit_restores) {
        size_t index = defined.size();
        ok = ir->blocks.front()->instructions.size() > index
            && ir->blocks.front()->instructions[index]->restore_on_exception_exit == expected_restore;
        if (!ok) {
            fprintf(stderr, "%s: the exit restoration of the instruction was not read\n", label);
        }
    }
    if (ok && unwind && !expected) {
        // The nested local must reuse the precise location written for the instruction before the catch tables.
        const auto* loc = ir->blocks.front()->instructions.front()->stmt_loc;
        ok = loc && ir->local_unwind_locations.size() == 2 && ir->local_var_slots.size() == 2;
        for (const auto& [local, slot] : ir->local_var_slots) {
            auto site = ir->local_unwind_locations.find(local);
            ok = ok && site != ir->local_unwind_locations.end() && site->second == (slot ? loc : nullptr);
        }
    }
    // The case checks what the reader made of the instruction.
    if (ok && inspect && !expected) {
        ok = (*inspect)(*ir);
    }
    // A mismatched group must fail before its payload reader is called, even with no payload bytes.
    if (tag_only && expression_reads) {
        ok = false;
    }
    if (!ok) {
        fprintf(stderr, "%s opcode=%u group=%u: accepted=%d, error='%s', expected='%s'\n", label,
            static_cast<uint16_t>(opcode), group, bool(ir), error.c_str(), expected ? expected : "success");
    }
    return ok;
}

static Payload invoke(O opcode, bool wrong_node = false) {
    return [opcode, wrong_node](QoreAOTBinaryWriter& w) {
        w.writeU8(static_cast<uint8_t>(wrong_node ? AOTExprKind::CONST_STRING : AOTExprKind::CONST_NOTHING));
        if (wrong_node) {
            w.writeStringRef("not an instanceof node");
        }
        w.writeU16(static_cast<uint16_t>(opcode));
        w.writeStringRef("");
        w.writeU8(0); // assignment mode
        w.writeU16(0xffff); // normal/exception targets (not executed)
        w.writeU16(0xffff);
    };
}

// The payload of a background helper instruction (group 62) of format \a version: the helper's self mode, its callee
// (QORE_AOT_BACKGROUND_HELPER_CALLEE_VERSION, qore#7087), \a params parameters by slot and the helper's IR, a function
// with a local variable for each parameter that returns
static Payload background_helper(uint16_t version, int32_t callee, uint32_t params) {
    return [version, callee, params](QoreAOTBinaryWriter& w) {
        w.writeU8(static_cast<uint8_t>(QoreIRBackgroundKind::Helper));
        w.writeStringRef("helper");
        w.writeU8(0); // no reference arguments
        w.writeU8(0); // QBHS_NONE
        if (version >= QORE_AOT_BACKGROUND_HELPER_CALLEE_VERSION) {
            w.writeU32(static_cast<uint32_t>(callee));
        }
        w.writeU32(params);
        for (uint32_t i = 0; i < params; ++i) {
            w.writeU32(i);
        }
        w.writeStringRef("background-helper");
        w.writeU32(1); // maximum value ID
        w.writeU32(params); // maximum local slot ID
        w.writeU32(0); // guards
        w.writeStringRef(""); // return type
        w.writeU32(0); // parent slots
        w.writeU16(1); // blocks
        w.writeU16(static_cast<uint16_t>(params)); // local variables
        w.writeU16(0); // body locals
        for (uint32_t i = 0; i < params; ++i) {
            w.writeStringRef("p");
            w.writeStringRef("auto");
            w.writeU32(i);
            w.writeU8(static_cast<uint8_t>(AOTIRLocalBinding::NONE));
            w.writeU32(UINT32_MAX);
        }
        if (version >= QORE_AOT_HANDLER_OWN_LOCALS_VERSION) {
            w.writeU32(0); // handler-own slots
        }
        w.writeStringRef("entry");
        w.writeU8(0); // block flags
        w.writeU16(1); // instructions: return
        w.writeU16(static_cast<uint16_t>(O::ReturnNothing));
        w.writeU8(static_cast<uint8_t>(G::Return));
        w.writeU32(0);
        w.writeU8(0); // operands
        w.writeU16(0xffff); // no exception target
        w.writeU8(0); // no value
        w.writeU32(0); // source start/end line
        w.writeU32(0);
        w.writeStringRef("");
        w.writeU8(static_cast<uint8_t>(QoreAOTStmtLocTag::None));
        w.writeU8(0); // no temp scopes
        w.writeU8(0);
        w.writeU32(0); // catch-exit depths
        w.writeU32(0); // catch-cleanup results
        if (version >= QORE_AOT_EXIT_RESTORE_VERSION) {
            w.writeU32(0); // exit restorations (qore#7379)
        }
        w.writeU16(0); // local unwind locations
    };
}

// returns the background instruction of a function read by check()
static const QoreIRBackgroundInstruction* find_background(const QoreIRFunction& func) {
    for (const auto& block : func.blocks) {
        for (const auto& inst : block->instructions) {
            if (inst->opcode == O::BackgroundInt) {
                return static_cast<const QoreIRBackgroundInstruction*>(inst.get());
            }
        }
    }
    return nullptr;
}

int main() {
    qore_init(QL_MIT, "UTF-8", false, QLO_DISABLE_SIGNAL_HANDLING);
    QoreProgram* pgm = qore_create_program(PO_NEW_STYLE);
    bool ok = true;
    unsigned count = 0;
    auto run = [&](const char* label, O op, G group, std::vector<uint32_t> operands, Payload payload,
            const char* error = nullptr, bool tag_only = false, bool wide = false,
            uint16_t version = QORE_AOT_BINARY_VERSION, uint32_t result_id = 3, uint64_t features = 0,
            const Payload* stmt_loc = nullptr) {
        ok = check(pgm, label, op, static_cast<uint8_t>(group), operands, payload, error,
            tag_only, wide, version, result_id, features, stmt_loc) && ok;
        ++count;
    };
    const Payload empty = [](QoreAOTBinaryWriter&) {};
    const char* pairing = "invalid AOT IR opcode/group pairing";
    const char* arity = "invalid AOT IR operand count";
    // Cover each registered serializable group, including tags beyond the first 64-bit mask word.
    for (unsigned group = 0; group < AOT_INST_GROUP_TABLE_SIZE; ++group) {
        const auto* info = getAOTInstGroupInfo(static_cast<uint8_t>(group));
        if (info && info->is_serializable) {
            O wrong_op = group == static_cast<unsigned>(G::Const) ? O::Br : O::ConstInt;
            run("wrong group", wrong_op, static_cast<G>(group), {}, empty, pairing, true);
        }
    }
    for (G group : {G::Base, G::Expr}) {
        run("complete wrong-class payload", O::ConstInt, group, {}, empty, pairing);
    }
    run("v9 payload", O::AddInt, G::Base, {1, 2}, empty, "unsupported binary format version 9 ", false, false, 9);
    run("AK v47 payload", O::AddInt, G::Base, {1, 2}, empty, "unsupported binary format version 47 ",
        false, false, QORE_AOT_BATCH_AK_VERSION);
    run("AL v48 payload", O::AddInt, G::Base, {1, 2}, empty, "unsupported binary format version 48 ",
        false, false, QORE_AOT_BATCH_AL_VERSION);
    run("AM v49 payload", O::AddInt, G::Base, {1, 2}, empty, "unsupported binary format version 49 ",
        false, false, QORE_AOT_BATCH_AM_VERSION);
    run("AN v50 payload", O::AddInt, G::Base, {1, 2}, empty, "unsupported binary format version 50 ",
        false, false, QORE_AOT_BATCH_AN_VERSION);
    run("AO v51 payload", O::AddInt, G::Base, {1, 2}, empty, "unsupported binary format version 51 ",
        false, false, QORE_AOT_BATCH_AO_VERSION);
    run("AOT-identity v52 payload", O::AddInt, G::Base, {1, 2}, empty, "unsupported binary format version 52 ",
        false, false, QORE_AOT_BATCH_AOT_IDENTITY_VERSION);
    // the exit restorations of the instructions (qore#7379): (block, instruction, kind) entries; the instruction under
    // test is the second of the block, after the constant of its operand
    auto exitRestore = [](uint16_t inst, uint32_t kind) -> Payload {
        return [inst, kind](QoreAOTBinaryWriter& w) {
            w.writeU32(1);
            w.writeU16(0);
            w.writeU16(inst);
            w.writeU32(kind);
        };
    };
    {
        struct ExitRestoreCase {
            const char* label;
            O op;
            uint16_t inst;
            uint32_t kind;
            const char* error;
            QoreIRExceptionExitRestore restore;
        };
        const ExitRestoreCase cases[] = {
            {"foreach element restored after the handlers", O::PushImplicitElement, 1,
                static_cast<uint32_t>(QoreIRExceptionExitRestore::AfterHandlers), nullptr,
                QoreIRExceptionExitRestore::AfterHandlers},
            {"operator element restored before the handlers", O::PushImplicitElement, 1,
                static_cast<uint32_t>(QoreIRExceptionExitRestore::BeforeHandlers), nullptr,
                QoreIRExceptionExitRestore::BeforeHandlers},
            {"operator argument restored before the handlers", O::PushImplicitArg, 1,
                static_cast<uint32_t>(QoreIRExceptionExitRestore::BeforeHandlers), nullptr,
                QoreIRExceptionExitRestore::BeforeHandlers},
            {"argument restored after the handlers", O::PushImplicitArg, 1,
                static_cast<uint32_t>(QoreIRExceptionExitRestore::AfterHandlers),
                "an entry of the exit restoration table has an invalid kind", QoreIRExceptionExitRestore::None},
            {"unknown restoration kind", O::PushImplicitElement, 1, 3,
                "an entry of the exit restoration table has an invalid kind", QoreIRExceptionExitRestore::None},
            {"no restoration kind", O::PushImplicitElement, 1, 0,
                "an entry of the exit restoration table has an invalid kind", QoreIRExceptionExitRestore::None},
            {"restoration of a constant", O::PushImplicitElement, 0,
                static_cast<uint32_t>(QoreIRExceptionExitRestore::BeforeHandlers),
                "an entry of the exit restoration table is an instruction that sets no implicit context",
                QoreIRExceptionExitRestore::None},
            {"restoration beyond the block", O::PushImplicitElement, 9,
                static_cast<uint32_t>(QoreIRExceptionExitRestore::BeforeHandlers),
                "an entry of the exit restoration table is beyond the instructions of the function",
                QoreIRExceptionExitRestore::None},
        };
        for (const ExitRestoreCase& c : cases) {
            Payload table = exitRestore(c.inst, c.kind);
            ok = check(pgm, c.label, c.op, static_cast<uint8_t>(G::Base), {1}, empty, c.error, false, false,
                QORE_AOT_BINARY_VERSION, 3, 0, nullptr, nullptr, nullptr, &table, c.restore) && ok;
            ++count;
        }
    }
    for (O op : {O::ConstInt, O::Br, O::BrIf, O::Return, O::Invoke, O::InvokeMethodDirect,
            O::ScopeEnter, O::MakeList, O::DebugStatement}) {
        run("wrong nested opcode", O::Invoke, G::Invoke, {}, invoke(op), pairing);
    }
    for (bool wide : {false, true}) {
        run("valid binary", O::AddInt, G::Base, {1, 2}, empty, nullptr, false, wide);
        run("missing binary operand", O::AddInt, G::Base, {1}, empty, arity, false, wide);
        run("extra binary operand", O::AddInt, G::Base, {1, 2, 3}, empty, arity, false, wide);
        run("invalid SSA operand", O::AddInt, G::Base, {0, 2}, empty,
            "invalid AOT IR operand value id", false, wide);
        run("valid nested binary", O::Invoke, G::Invoke, {1, 2}, invoke(O::AddInt), nullptr, false, wide);
        run("missing nested operand", O::Invoke, G::Invoke, {1}, invoke(O::AddInt), arity, false, wide);
        run("extra nested operand", O::Invoke, G::Invoke, {1, 2, 3}, invoke(O::AddInt), arity, false, wide);
        run("minimum operands", O::ListIndexDynamic, G::Expr, {1}, empty, arity, false, wide);
        run("closure callee required", O::Invoke, G::Invoke, {}, invoke(O::CallClosureDirect), arity, false, wide);
        run("argument index count", O::LoadArg, G::Base, {1, 2}, empty, arity, false, wide);
    }
    // Variable arity still has operation-specific lower/upper bounds.
    for (auto [op, group] : {std::pair{O::ListSetScannedType, G::Base},
            std::pair{O::DotEvalMethodDirect, G::DotEvalMethodDirect},
            std::pair{O::InvokeDotEvalMethodDirect, G::InvokeDotEvalMethodDirect},
            std::pair{O::ToString, G::Base}, std::pair{O::Sprintf, G::Base}}) {
        run("variable operands missing", op, group, {}, empty, arity);
    }
    run("scanned type upper bound", O::ListSetScannedType, G::Base, {1, 2, 3}, empty, arity);
    run("selector fixed buffer bound", O::ListSelectorStep, G::Base,
        std::vector<uint32_t>(3 + QORE_IR_LIST_SELECTOR_STEP_MAX * 2, 1), empty, arity);
    run("selector continuation prefix", O::ListSelectorStep, G::Base, {1, 2}, empty, arity,
        false, false, QORE_AOT_BINARY_VERSION, 0);
    for (bool wrong_node : {false, true}) {
        run("invoke expression kind", O::Invoke, G::Invoke, {1}, invoke(O::InstanceOfBool, wrong_node),
            "invalid AOT IR expression kind");
    }
    run("ordinary expression kind", O::InstanceOfBool, G::Expr, {1}, [](QoreAOTBinaryWriter& w) {
        w.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_NOTHING));
        w.writeU8(0);
    }, "invalid AOT IR expression kind");
    // Real lowering emits these as Expr even when no AST payload is needed by execution.
    for (O op : {O::ToString, O::ToStringChecked, O::ToIntChecked, O::ToFloatChecked,
            O::IterateValue, O::ListIntSprintfJoin}) {
        run("lowered expression form", op, G::Expr,
            std::vector<uint32_t>(op == O::ListIntSprintfJoin ? 4 : 1, 1), [](QoreAOTBinaryWriter& w) {
                w.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_NOTHING));
                w.writeU8(0);
            });
    }
    run("lowered sprintf ternary", O::SprintfIntFixed, G::Base, {1, 2, 4}, empty);
    for (auto [keys, values] : {std::pair{1u, 0u}, std::pair{0u, 1u}, std::pair{1u, 1u}}) {
        run("constant hash keys", O::MakeHashConstKeys, G::MakeHashConstKeys,
            std::vector<uint32_t>(values, 1), [keys](QoreAOTBinaryWriter& w) {
                w.writeU16(keys);
                for (unsigned i = 0; i < keys; ++i) {
                    w.writeStringRef("key");
                }
            }, keys == values ? nullptr : "invalid AOT IR constant hash key operand count");
    }
    struct SelectorCase {
        std::vector<uint8_t> kinds;
        unsigned values;
        const char* error;
    };
    for (const auto& test : {SelectorCase{{1}, 2, "invalid AOT IR list selector operand count"},
            SelectorCase{{2}, 2, "invalid AOT IR list selector kind"},
            SelectorCase{{0}, 3, "invalid AOT IR list selector operand count"},
            SelectorCase{{1}, 3, nullptr}, SelectorCase{{1, 0}, 4, nullptr}, SelectorCase{{}, 2, nullptr}}) {
        run("selector metadata", O::ListIndexDynamic, G::Expr, std::vector<uint32_t>(test.values, 1),
            [&test](QoreAOTBinaryWriter& w) {
                w.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_NOTHING));
                w.writeU8(0);
                w.writeU8(test.kinds.size());
                for (uint8_t kind : test.kinds) {
                    w.writeU8(kind);
                }
            }, test.error, false, false, QORE_AOT_BINARY_VERSION, 3, QORE_AOT_FEAT_LIST_SELECTOR_RANGE);
    }
    // A valid opcode/class pair cannot use another constant payload kind (or an unknown kind).
    for (O op : {O::ConstInt, O::ConstFloat, O::ConstBool, O::ConstBoolBoxed, O::ConstChar,
            O::ConstNothing, O::ConstNull, O::ConstString, O::ConstDate, O::ConstEnum}) {
        run("constant discriminator", op, G::Const, {}, [](QoreAOTBinaryWriter& w) {
            w.writeU8(255);
        }, "invalid AOT IR constant kind");
    }
    run("mismatched constant type", O::ConstInt, G::Const, {}, [](QoreAOTBinaryWriter& w) {
        w.writeU8(static_cast<uint8_t>(QoreIRConstant::Kind::String));
        w.writeStringRef("not an integer");
    }, "invalid AOT IR constant kind");
    run("valid int constant", O::ConstInt, G::Const, {}, [](QoreAOTBinaryWriter& w) {
        w.writeU8(static_cast<uint8_t>(QoreIRConstant::Kind::Int));
        w.writeI64(42);
    });
    run("valid nothing constant", O::ConstNothing, G::Const, {}, [](QoreAOTBinaryWriter& w) {
        w.writeU8(static_cast<uint8_t>(QoreIRConstant::Kind::Nothing));
    });
    // the paths of a debugger's flow requests of a step event and of a ScopeExit (#6200, #6201): an image of an
    // earlier version has none, and a mask with an unknown bit or a block that does not exist is rejected
    auto debug_step = [](int mask, std::vector<uint16_t> blocks) -> Payload {
        return [mask, blocks](QoreAOTBinaryWriter& w) {
            w.writeU16(0); // nest depth
            w.writeU8(0); // nest flags
            if (mask >= 0) {
                w.writeU8(static_cast<uint8_t>(mask));
                for (uint16_t b : blocks) {
                    w.writeU16(b);
                }
            }
        };
    };
    run("step event without paths before the flow version", O::DebugStatement, G::DebugStatement, {},
        debug_step(-1, {}), "unsupported binary format version 45 ", false, false, QORE_AOT_DEBUG_FLOW_VERSION - 1, 0);
    run("step event without paths", O::DebugStatement, G::DebugStatement, {}, debug_step(0, {}), nullptr,
        false, false, QORE_AOT_BINARY_VERSION, 0);
    run("step event with paths", O::DebugStatement, G::DebugStatement, {}, debug_step(7, {0, 0, 0}), nullptr,
        false, false, QORE_AOT_BINARY_VERSION, 0);
    for (int mask : {0x08, 0x10, 0x80, 0xff}) {
        run("step event flow mask", O::DebugStatement, G::DebugStatement, {}, debug_step(mask, {0, 0, 0}),
            "invalid debugger flow mask", false, false, QORE_AOT_BINARY_VERSION, 0);
    }
    run("step event flow block", O::DebugStatement, G::DebugStatement, {}, debug_step(1, {5}),
        "invalid debugger flow path block", false, false, QORE_AOT_BINARY_VERSION, 0);
    auto scope_exit = [](int mask, std::vector<uint16_t> blocks) -> Payload {
        return [mask, blocks](QoreAOTBinaryWriter& w) {
            w.writeU32(1); // scope id
            w.writeU8(0); // is_error
            w.writeU8(0); // flags
            if (mask >= 0) {
                w.writeU8(static_cast<uint8_t>(mask));
                for (uint16_t b : blocks) {
                    w.writeU16(b);
                }
            }
        };
    };
    // the function of a check has only the instruction: a ScopeExit that is decoded is then refused by the verification
    // of the stacks, as no scope is open (qore#6468), which happens only once the instruction has been decoded
    const char* no_scope = "ScopeExit closes an entry of the scopes that is not the innermost one";
    run("ScopeExit without paths before the flow version", O::ScopeExit, G::ScopeExit, {}, scope_exit(-1, {}),
        "unsupported binary format version 45 ", false, false, QORE_AOT_DEBUG_FLOW_VERSION - 1, 0);
    run("ScopeExit with paths", O::ScopeExit, G::ScopeExit, {}, scope_exit(5, {0, 0}), no_scope, false, false,
        QORE_AOT_BINARY_VERSION, 0);
    run("ScopeExit flow mask", O::ScopeExit, G::ScopeExit, {}, scope_exit(0x40, {}),
        "invalid debugger flow mask", false, false, QORE_AOT_BINARY_VERSION, 0);
    // the runtime location of an instruction (QORE_AOT_STATEMENT_LOCATIONS_VERSION, qore#6646): a new entry of the
    // function's table is read; a tag that does not exist, the location of an instruction before it when there is
    // none, an entry beyond the table and a new entry without a line are refused
    auto stmt_loc = [&](const char* label, const Payload& loc, const char* error) {
        run(label, O::AddInt, G::Base, {1, 2}, empty, error, false, false, QORE_AOT_BINARY_VERSION, 3, 0, &loc);
    };
    auto new_entry = [](int line) -> Payload {
        return [line](QoreAOTBinaryWriter& w) {
            w.writeU8(static_cast<uint8_t>(QoreAOTStmtLocTag::New));
            w.writeU32(static_cast<uint32_t>(line));
            w.writeU32(static_cast<uint32_t>(line));
            w.writeStringRef("stmt.q");
        };
    };
    stmt_loc("statement location", new_entry(3), nullptr);
    stmt_loc("statement location without a line", new_entry(0), "a runtime location has no line");
    stmt_loc("statement location tag", [](QoreAOTBinaryWriter& w) {
        w.writeU8(4);
    }, "invalid runtime location tag 4");
    stmt_loc("statement location of none before", [](QoreAOTBinaryWriter& w) {
        w.writeU8(static_cast<uint8_t>(QoreAOTStmtLocTag::Previous));
    }, "has the runtime location of an instruction before it that has none");
    stmt_loc("statement location entry", [](QoreAOTBinaryWriter& w) {
        w.writeU8(static_cast<uint8_t>(QoreAOTStmtLocTag::Entry));
        w.writeU32(0);
    }, "runtime location 0 is beyond the 0 location(s) of the function");
    if constexpr (QORE_AOT_BINARY_VERSION >= QORE_AOT_LOCAL_UNWIND_LOCATIONS_VERSION) {
        const Payload nothing = [](QoreAOTBinaryWriter& w) {
            w.writeU8(static_cast<uint8_t>(QoreIRConstant::Kind::Nothing));
        };
        auto mapping = [&](const char* label, const Payload& wire, const char* error) {
            const Payload instruction_location = new_entry(3);
            ok = check(pgm, label, O::ConstNothing, static_cast<uint8_t>(G::Const), {}, nothing, error,
                false, false, QORE_AOT_BINARY_VERSION, 3, 0, &instruction_location, &wire) && ok;
            ++count;
        };
        auto slots = [](std::initializer_list<uint32_t> ids) -> Payload {
            return [ids = std::vector<uint32_t>(ids)](QoreAOTBinaryWriter& w) {
                w.writeU16(static_cast<uint16_t>(ids.size()));
                for (uint32_t slot : ids) {
                    w.writeU32(slot);
                    w.writeU8(static_cast<uint8_t>(QoreAOTStmtLocTag::None));
                }
            };
        };
        mapping("caller and nested unwind locations", [](QoreAOTBinaryWriter& w) {
            w.writeU16(2);
            w.writeU32(0);
            w.writeU8(static_cast<uint8_t>(QoreAOTStmtLocTag::None));
            w.writeU32(2);
            w.writeU8(static_cast<uint8_t>(QoreAOTStmtLocTag::Entry));
            w.writeU32(0);
        }, nullptr);
        mapping("too many unwind slots", slots({0, 1, 2}), "local unwind location count exceeds");
        mapping("unknown unwind slot", slots({1}), "unknown or unordered local unwind slot");
        mapping("out-of-range unwind slot", slots({3}), "unknown or unordered local unwind slot");
        mapping("duplicate unwind slot", slots({0, 0}), "unknown or unordered local unwind slot");
        mapping("unordered unwind slots", slots({2, 0}), "unknown or unordered local unwind slot");
        mapping("truncated unwind section", [](QoreAOTBinaryWriter& w) { w.writeU16(1); },
            "truncated IR function");
        // The instruction created location entry zero. A short index must fail even if the bounds-checked
        // read returns zero and that index would otherwise resolve.
        mapping("truncated unwind location index", [](QoreAOTBinaryWriter& w) {
            w.writeU16(1);
            w.writeU32(0);
            w.writeU8(static_cast<uint8_t>(QoreAOTStmtLocTag::Entry));
            w.writeU16(0);
        }, "truncated IR function");
        mapping("invalid unwind location tag", [](QoreAOTBinaryWriter& w) {
            w.writeU16(1);
            w.writeU32(0);
            w.writeU8(255);
        }, "invalid runtime location tag");
        mapping("unknown unwind location index", [](QoreAOTBinaryWriter& w) {
            w.writeU16(1);
            w.writeU32(0);
            w.writeU8(static_cast<uint8_t>(QoreAOTStmtLocTag::Entry));
            w.writeU32(1);
        }, "runtime location 1 is beyond");
    }
    {
        // the callee of a background helper (qore#7087): a v52 image records it; the reader refuses a v51 image, whose
        // helpers have none, with the message to rebuild it
        auto callee = [&](const char* label, uint16_t version, int32_t value, uint32_t params, int32_t read,
                const char* error = nullptr) {
            std::vector<uint32_t> operands;
            for (uint32_t i = 0; i < params; ++i) {
                operands.push_back(i + 1);
            }
            const std::function<bool(const QoreIRFunction&)> inspect = [label, read, params](
                    const QoreIRFunction& func) {
                const QoreIRBackgroundInstruction* bi = find_background(func);
                if (!bi || bi->kind != QoreIRBackgroundKind::Helper || !bi->helper_ir
                        || bi->helper_params.size() != params || bi->helper_callee != read) {
                    fprintf(stderr, "%s: callee %d read as %d\n", label, read, bi ? bi->helper_callee : -100);
                    return false;
                }
                return true;
            };
            ok = check(pgm, label, O::BackgroundInt, static_cast<uint8_t>(G::Background), operands,
                background_helper(version, value, params), error, false, false, version, 30, 0, nullptr, nullptr,
                &inspect) && ok;
            ++count;
        };
        static_assert(QORE_AOT_BINARY_MIN_VERSION >= QORE_AOT_BACKGROUND_HELPER_CALLEE_VERSION,
            "the reader refuses the layout before the callee");
        constexpr uint16_t old_version = QORE_AOT_BACKGROUND_HELPER_CALLEE_VERSION - 1;
        // the versions that the reader supports, which a later batch can raise (qore#7379)
        const std::string old_error = "unsupported binary format version " + std::to_string(old_version)
            + " (supported " + std::to_string(QORE_AOT_BINARY_MIN_VERSION) + "-"
            + std::to_string(QORE_AOT_BINARY_VERSION) + "); rebuild the artifact with this version of qcc";
        callee("v51 helper without a callee", old_version, 0, 2, 0, old_error.c_str());
        callee("helper callee parameter 0", QORE_AOT_BINARY_VERSION, 0, 2, 0);
        callee("helper callee parameter 1", QORE_AOT_BINARY_VERSION, 1, 2, 1);
        callee("helper callee checked in the thread", QORE_AOT_BINARY_VERSION, QBH_CHECK_IN_THREAD, 2,
            QBH_CHECK_IN_THREAD);
        callee("helper callee unknown", QORE_AOT_BINARY_VERSION, QBH_CALLEE_UNKNOWN, 1, QBH_CALLEE_UNKNOWN);
        callee("helper callee beyond its parameters", QORE_AOT_BINARY_VERSION, 2, 2, 0,
            "invalid background helper callee 2");
        callee("helper callee without parameters", QORE_AOT_BINARY_VERSION, 0, 0, 0,
            "invalid background helper callee 0");
        callee("helper callee below the markers", QORE_AOT_BINARY_VERSION, -3, 1, 0,
            "invalid background helper callee -3");
    }
    // the v52 refusal and the eight exit restoration cases of v53 (qore#7379)
    constexpr unsigned expected_count = QORE_AOT_BINARY_VERSION >= QORE_AOT_EXIT_RESTORE_VERSION ? 187
        : (QORE_AOT_BINARY_VERSION >= QORE_AOT_LOCAL_UNWIND_LOCATIONS_VERSION ? 178 : 168);
    if (count != expected_count) {
        fprintf(stderr, "ran %u of %u expected checks for AOT format %u\n", count, expected_count,
            static_cast<unsigned>(QORE_AOT_BINARY_VERSION));
        ok = false;
    }
    qore_destroy_program(pgm);
    qore_cleanup();
    if (ok) {
        printf("PASS: %u AOT IR instruction validation checks\n", count);
    }
    return ok ? 0 : 1;
}
