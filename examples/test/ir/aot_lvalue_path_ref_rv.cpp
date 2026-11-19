/* -*- mode: c++; indent-tabs-mode: nil -*- */
// Copyright (C) 2026 Qore Technologies, s.r.o.
// SPDX-License-Identifier: LGPL-2.1-or-later

// An LValuePath instruction keeps whether its value is used (ref_rv) through the serialized IR of an AOT image
// (qore#6328): the IR rebuilt from the image (debug IR, closure bodies without native code) makes an unused post
// increment or decrement copy no old value, and an unused remove or delete release its value in the operation.  An
// image of an earlier format has no such flag: its instructions use their values, as before.

#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <qore/intern/LocalVar.h>
#include <qore/intern/QoreIR.h>
#include <qore/intern/QoreAOTBinary.h>
#include <qore/intern/QoreAOTInstRegistry.h>

#include <cstdio>
#include <functional>

using O = QoreIROpcode;

//! the raw value of the LValuePath instruction group
static constexpr uint8_t LValuePathGroup = static_cast<uint8_t>(QoreIRInstGroup::LValuePath);

//! Reads the IR function of the TOPLEVEL section of an image and returns the ref_rv flag of its first instruction
/** @return 1 or 0 for the flag, -1 if the image cannot be read
*/
static int readFlag(QoreProgram* pgm, const char* label, std::vector<uint8_t>& data) {
    QoreAOTBinaryReader reader;
    std::string error;
    if (!reader.open(data.data(), data.size(), error)) {
        fprintf(stderr, "%s: cannot open the image: %s\n", label, error.c_str());
        return -1;
    }
    const auto* sh = reader.findSection(QoreAOTSectionType::TOPLEVEL);
    if (!sh) {
        fprintf(stderr, "%s: the image has no IR section\n", label);
        return -1;
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
    if (!ir || !error.empty() || ptr != end || ir->blocks.empty() || ir->blocks[0]->instructions.empty()) {
        fprintf(stderr, "%s: cannot read the IR: accepted=%d error='%s' at end=%d\n", label, bool(ir),
            error.c_str(), ptr == end);
        return -1;
    }
    auto* pi = qore_ir_inst_cast<const QoreIRLValuePathInstruction*>(ir->blocks[0]->instructions[0].get());
    if (!pi) {
        fprintf(stderr, "%s: the instruction read is not an LValuePath instruction\n", label);
        return -1;
    }
    return pi->ref_rv ? 1 : 0;
}

//! Writes an LValuePath instruction with the serializer of the current format and reads it back
static bool roundTrip(QoreProgram* pgm, LVUnaryOp op, bool ref_rv) {
    char label[64];
    snprintf(label, sizeof(label), "round trip of unary op %u with ref_rv=%d", static_cast<unsigned>(op), ref_rv);
    QoreIRFunction fn("payload", "payload");
    auto block = std::make_unique<QoreIRBasicBlock>("entry");
    auto inst = std::make_unique<QoreIRLValuePathInstruction>(O::LValuePathUnary);
    inst->unary_op = op;
    inst->ref_rv = ref_rv;
    inst->result = fn.createValue();
    block->instructions.push_back(std::move(inst));
    // the instruction is in a function that can run (qore#6468)
    block->instructions.push_back(std::make_unique<QoreIRReturnInstruction>());
    fn.blocks.push_back(std::move(block));
    QoreAOTBinaryWriter writer;
    writer.feature_flags = QORE_AOT_FEAT_LVPATH_PATTERN;
    uint32_t section = writer.beginSection(QoreAOTSectionType::TOPLEVEL);
    if (!serializeIRFunction(writer, fn, [](QoreAOTBinaryWriter& w, const QoreValue&) {
        w.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_NOTHING));
        return true;
    })) {
        fprintf(stderr, "%s: cannot serialize the IR\n", label);
        return false;
    }
    writer.endSection(section);
    QoreAOTBinaryHeader header{};
    header.magic = QORE_AOT_BINARY_MAGIC;
    header.version = QORE_AOT_BINARY_VERSION;
    header.feature_flags = writer.feature_flags;
    std::vector<uint8_t> data;
    if (!writer.finalize(header, data)) {
        fprintf(stderr, "%s: cannot finalize the image\n", label);
        return false;
    }
    int flag = readFlag(pgm, label, data);
    if (flag != (ref_rv ? 1 : 0)) {
        fprintf(stderr, "%s: ref_rv read back as %d\n", label, flag);
        return false;
    }
    return true;
}

//! Reads an LValuePath instruction written in the format before QORE_AOT_LVPATH_REF_RV_VERSION, which has no flag
static bool earlierFormat(QoreProgram* pgm, LVUnaryOp op) {
    char label[64];
    snprintf(label, sizeof(label), "format %u, unary op %u", QORE_AOT_LVPATH_REF_RV_VERSION - 1,
        static_cast<unsigned>(op));
    const uint16_t version = QORE_AOT_LVPATH_REF_RV_VERSION - 1;
    QoreAOTBinaryWriter writer;
    writer.feature_flags = QORE_AOT_FEAT_LVPATH_PATTERN;
    uint32_t section = writer.beginSection(QoreAOTSectionType::TOPLEVEL);
    // the function header and its block, as serializeIRFunction() wrote them in that format
    writer.writeStringRef("payload");
    writer.writeU32(2); // maximum value ID
    writer.writeU32(0); // local slots
    writer.writeU32(0); // guards
    writer.writeStringRef(""); // return type
    writer.writeU32(0); // parent slots
    writer.writeU16(1); // blocks
    writer.writeU16(0); // local variables
    writer.writeU16(0); // body locals
    writer.writeU32(0); // handler-own slots
    writer.writeStringRef("entry");
    writer.writeU8(0); // block flags
    writer.writeU16(2); // instructions: the instruction and a return, so that the function can run (qore#6468)
    writer.writeU16(static_cast<uint16_t>(O::LValuePathUnary));
    writer.writeU8(LValuePathGroup);
    writer.writeU32(1); // result
    writer.writeU8(0); // operands
    writer.writeU16(0xffff); // no exception target
    writer.writeU8(0); // assignment mode
    writer.writeU8(0); // compound op
    writer.writeU8(static_cast<uint8_t>(op));
    writer.writeU8(0); // binary mutation op
    writer.writeU8(0); // ternary op
    writer.writeU8(0); // no pattern
    writer.writeU8(0); // path steps
    writer.writeU32(0); // source start/end line
    writer.writeU32(0);
    writer.writeStringRef("");
    writer.writeU16(static_cast<uint16_t>(O::ReturnNothing));
    writer.writeU8(static_cast<uint8_t>(QoreIRInstGroup::Return));
    writer.writeU32(0); // no result
    writer.writeU8(0); // operands
    writer.writeU16(0xffff); // no exception target
    writer.writeU8(0); // no value
    writer.writeU32(0); // source start/end line
    writer.writeU32(0);
    writer.writeStringRef("");
    writer.endSection(section);
    QoreAOTBinaryHeader header{};
    header.magic = QORE_AOT_BINARY_MAGIC;
    header.version = version;
    header.feature_flags = writer.feature_flags;
    std::vector<uint8_t> data;
    if (!writer.finalize(header, data)) {
        fprintf(stderr, "%s: cannot finalize the image\n", label);
        return false;
    }
    int flag = readFlag(pgm, label, data);
    if (flag != 1) {
        fprintf(stderr, "%s: ref_rv read as %d; an instruction of an earlier format uses its value\n", label, flag);
        return false;
    }
    return true;
}

int main() {
    // The signal thread must stay disabled for valgrind, just as for qore -b.
    qore_init(QL_MIT, "UTF-8", false, QLO_DISABLE_SIGNAL_HANDLING);
    QoreProgram* pgm = qore_create_program(PO_NEW_STYLE);
    unsigned count = 0;
    bool ok = true;
    for (LVUnaryOp op : {LVUnaryOp::PostInc, LVUnaryOp::PostDec, LVUnaryOp::Remove, LVUnaryOp::Delete}) {
        for (bool ref_rv : {false, true}) {
            ok = roundTrip(pgm, op, ref_rv) && ok;
            ++count;
        }
        ok = earlierFormat(pgm, op) && ok;
        ++count;
    }
    qore_destroy_program(pgm);
    qore_cleanup();
    if (ok) {
        printf("PASS: %u AOT LValuePath ref_rv checks\n", count);
    }
    return ok ? 0 : 1;
}
