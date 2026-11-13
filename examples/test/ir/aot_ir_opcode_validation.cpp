/* -*- mode: c++; indent-tabs-mode: nil -*- */
// Copyright (C) 2026 Qore Technologies, s.r.o.
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <qore/QoreStringNode.h>
#include <qore/QoreString.h>
#include <qore/intern/LocalVar.h>
#include <qore/intern/QoreIR.h>
#include <qore/intern/QoreAOTBinary.h>

#include <cstdio>

struct OpcodeCase {
    uint16_t opcode;
    const char* error;
};

// Construct real serialized IR, deliberately claiming an Expr or Invoke group for an unsupported
// opcode. Neither the instruction group nor the nested invoke opcode is trusted by the decoder.
static bool check(QoreProgram* pgm, const OpcodeCase& test, bool nested) {
    QoreIRFunction fn("payload", "payload");
    auto block = std::make_unique<QoreIRBasicBlock>("entry");
    QoreIROpcode opcode = static_cast<QoreIROpcode>(test.opcode);
    if (nested) {
        auto inst = std::make_unique<QoreIRInvokeInstruction>(QoreValue(), nullptr, nullptr);
        inst->invoke_opcode = opcode;
        inst->result = fn.createValue();
        block->instructions.push_back(std::move(inst));
    } else {
        auto inst = std::make_unique<QoreIRExprInstruction>(opcode, QoreValue());
        inst->result = fn.createValue();
        block->instructions.push_back(std::move(inst));
    }
    fn.blocks.push_back(std::move(block));
    QoreAOTBinaryWriter writer;
    uint32_t section = writer.beginSection(QoreAOTSectionType::TOPLEVEL);
    if (!serializeIRFunction(writer, fn, [](QoreAOTBinaryWriter& w, const QoreValue&) {
        w.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_NOTHING));
        return true;
    })) {
        fprintf(stderr, "cannot serialize test payload\n");
        return false;
    }
    writer.endSection(section);
    QoreAOTBinaryHeader header{};
    header.magic = QORE_AOT_BINARY_MAGIC;
    header.version = QORE_AOT_BINARY_VERSION;
    std::vector<uint8_t> data;
    if (!writer.finalize(header, data)) {
        fprintf(stderr, "cannot finalize test payload\n");
        return false;
    }
    QoreAOTBinaryReader reader;
    std::string error;
    if (!reader.open(data.data(), data.size(), error)) {
        fprintf(stderr, "cannot open test payload: %s\n", error.c_str());
        return false;
    }
    const auto* sh = reader.findSection(QoreAOTSectionType::TOPLEVEL);
    if (!sh) {
        fprintf(stderr, "test payload has no IR section\n");
        return false;
    }
    const uint8_t* ptr = reader.getSectionData(*sh);
    const uint8_t* end = ptr + sh->size;
    auto ir = deserializeIRFunction(reader, ptr, end, pgm,
        [](const QoreAOTBinaryReader&, const uint8_t*& p, const uint8_t* end, std::string& error) {
            if (p == end || *p++ != static_cast<uint8_t>(AOTExprKind::CONST_NOTHING)) {
                error = "invalid test expression";
            }
            return QoreValue();
        }, nullptr, error);
    // The function reader prefixes errors with the failing block/instruction and group context.
    bool ok = test.error ? !ir && error.ends_with(test.error) : ir && error.empty() && ptr == end;
    if (!ok) {
        fprintf(stderr, "%s opcode %u: accepted=%d, error='%s', expected='%s'\n",
            nested ? "Invoke" : "Expr", test.opcode, bool(ir), error.c_str(), test.error ? test.error : "");
    }
    return ok;
}

int main() {
    // The signal thread must stay disabled for valgrind, just as for qore -b.
    qore_init(QL_MIT, "UTF-8", false, QLO_DISABLE_SIGNAL_HANDLING);
    QoreProgram* pgm = qore_create_program(PO_NEW_STYLE);
    // Literal numeric IDs ensure that the removed opcode stays reserved in old serialized inputs.
    const OpcodeCase cases[] = {
        {295, nullptr},
        {297, "unsupported AOT IR opcode CallIndirect (297)"},
        {141, "unsupported AOT IR opcode Summarize (141)"},
        {360, "unsupported AOT IR opcode CallAOTHelper (360)"},
        {65535, "unsupported AOT IR opcode <unknown> (65535)"},
    };
    unsigned count = 0;
    bool ok = true;
    for (const auto& test : cases) {
        for (bool nested : {false, true}) {
            ok = check(pgm, test, nested) && ok;
            ++count;
        }
    }
    qore_destroy_program(pgm);
    qore_cleanup();
    if (ok) {
        printf("PASS: %u AOT IR opcode validation checks\n", count);
    }
    return ok ? 0 : 1;
}
