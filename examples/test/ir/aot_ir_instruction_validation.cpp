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
        uint32_t result_id = 3, uint64_t features = 0) {
    QoreAOTBinaryWriter writer;
    writer.feature_flags = features | (wide ? QORE_AOT_FEAT_WIDE_IR_OPERANDS : 0);
    uint32_t section = writer.beginSection(QoreAOTSectionType::TOPLEVEL);
    writer.writeStringRef("instruction-validation");
    writer.writeU32(32); // maximum value ID
    writer.writeU32(0); // local slots
    writer.writeU32(0); // guards
    writer.writeStringRef(""); // return type
    writer.writeU32(0); // parent slots
    writer.writeU16(1); // blocks
    writer.writeU16(0); // local variables
    writer.writeU16(0); // body locals
    if (version >= QORE_AOT_HANDLER_OWN_LOCALS_VERSION) {
        writer.writeU32(0); // handler-own slots
    }
    writer.writeStringRef("entry");
    writer.writeU8(0); // block flags
    writer.writeU16(1); // instructions
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

int main() {
    qore_init(QL_MIT, "UTF-8", false, QLO_DISABLE_SIGNAL_HANDLING);
    QoreProgram* pgm = qore_create_program(PO_NEW_STYLE);
    bool ok = true;
    unsigned count = 0;
    auto run = [&](const char* label, O op, G group, std::vector<uint32_t> operands, Payload payload,
            const char* error = nullptr, bool tag_only = false, bool wide = false,
            uint16_t version = QORE_AOT_BINARY_VERSION, uint32_t result_id = 3, uint64_t features = 0) {
        ok = check(pgm, label, op, static_cast<uint8_t>(group), operands, payload, error,
            tag_only, wide, version, result_id, features) && ok;
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
    // The same guard applies to the oldest supported format and the latest one.
    run("legacy v9 wrong class", O::ConstInt, G::Base, {}, empty, pairing, false, false, 9);
    run("legacy v9 valid base", O::AddInt, G::Base, {1, 2}, empty, nullptr, false, false, 9);
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
    run("lowered sprintf ternary", O::SprintfIntFixed, G::Base, {1, 2, 3}, empty);
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
    qore_destroy_program(pgm);
    qore_cleanup();
    if (ok) {
        printf("PASS: %u AOT IR instruction validation checks\n", count);
    }
    return ok ? 0 : 1;
}
