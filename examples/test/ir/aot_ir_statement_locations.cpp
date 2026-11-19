/* -*- mode: c++; indent-tabs-mode: nil -*- */
// Copyright (C) 2026 Qore Technologies, s.r.o.
// SPDX-License-Identifier: LGPL-2.1-or-later

// qore#6646: the runtime locations of IR instructions (QoreIRInstruction::stmt_loc), the locations of the statements
// whose code they are, are written to AOT images (QORE_AOT_STATEMENT_LOCATIONS_VERSION) and read back with the same
// sharing: the instructions of a statement share its location, so that the IR interpreter and native code store the
// location only where the statement changes.  An instruction without one keeps none, the instructions of a
// PushTempMarkPair, which is written as two PushTempMark instructions, keep the pair's, the IR function of a handler
// written in an instruction has a location table of its own. The reader rejects images predating statement locations.

#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <qore/intern/QoreIR.h>
#include <qore/intern/QoreIRBuilder.h>
#include <qore/intern/QoreAOTBinary.h>
#include <qore/intern/QoreJITIncludes.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

static unsigned count = 0;
static bool ok = true;

static void expect(bool cond, const char* what) {
    ++count;
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", what);
        ok = false;
    }
}

//! returns true if the location has the lines and the file given
static bool same(const QoreProgramLocation* loc, int start_line, int end_line, const char* file) {
    return loc && loc->start_line == start_line && loc->end_line == end_line && loc->getFile()
        && !strcmp(loc->getFile(), file);
}

//! reads the IR function in the TOPLEVEL section of the image
static std::unique_ptr<QoreIRFunction> read(QoreProgram* pgm, const std::vector<uint8_t>& data, std::string& error) {
    QoreAOTBinaryReader reader;
    if (!reader.open(data.data(), data.size(), error)) {
        return nullptr;
    }
    const auto* sh = reader.findSection(QoreAOTSectionType::TOPLEVEL);
    if (!sh) {
        error = "no section";
        return nullptr;
    }
    const uint8_t* ptr = reader.getSectionData(*sh);
    const uint8_t* end = ptr + sh->size;
    return deserializeIRFunction(reader, ptr, end, pgm,
        [](const QoreAOTBinaryReader&, const uint8_t*& p, const uint8_t* e, std::string& err) {
            if (p == e || *p++ != static_cast<uint8_t>(AOTExprKind::CONST_NOTHING)) {
                err = "invalid fixture expression";
            }
            return QoreValue();
        }, nullptr, error);
}

//! writes the function with the AOT writer and reads it with the reader
static std::unique_ptr<QoreIRFunction> roundTrip(QoreProgram* pgm, const QoreIRFunction& fn, std::string& error) {
    QoreAOTBinaryWriter writer;
    uint32_t section = writer.beginSection(QoreAOTSectionType::TOPLEVEL);
    if (!serializeIRFunction(writer, fn, [](QoreAOTBinaryWriter& w, const QoreValue&) {
        w.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_NOTHING));
        return true;
    })) {
        error = "cannot write the function";
        return nullptr;
    }
    writer.endSection(section);
    QoreAOTBinaryHeader header{};
    header.magic = QORE_AOT_BINARY_MAGIC;
    header.version = QORE_AOT_BINARY_VERSION;
    header.feature_flags = writer.feature_flags;
    std::vector<uint8_t> data;
    if (!writer.finalize(header, data)) {
        error = "cannot finalize the payload";
        return nullptr;
    }
    return read(pgm, data, error);
}

//! the statements of the function: their locations are shared by the instructions of each
static void roundTripLocations(QoreProgram* pgm) {
    QoreProgramLocation stmt1("stmt.q", 3, 6);
    QoreProgramLocation stmt2("stmt.q", 4, 4);
    QoreProgramLocation expr("stmt.q", 5, 5);

    QoreIRFunction fn("statement-locations", "statement-locations");
    QoreIRBuilder b(&fn);
    QoreIRBasicBlock* entry = fn.createBlock("entry");
    QoreIRBasicBlock* next = fn.createBlock("next");
    b.setBlock(entry);
    // an instruction of the statement whose own location is an expression's
    b.createConstInt(1, &expr)->stmt_loc = &stmt1;
    // another instruction of the same statement
    b.createConstInt(2, &expr)->stmt_loc = &stmt1;
    // an instruction of a nested statement on its own line, with the statement's own location
    b.createConstInt(3, &stmt2)->stmt_loc = &stmt2;
    // an instruction outside the statements
    b.createConstInt(4)->stmt_loc = nullptr;
    b.createBranch(next)->stmt_loc = &stmt1;
    b.setBlock(next);
    // the first statement again, after the nested statement's
    b.createConstInt(5, &expr)->stmt_loc = &stmt2;
    b.createConstInt(6, &expr)->stmt_loc = &stmt1;
    QoreIRInstruction* pair = next->appendInstruction<QoreIRInstruction>(QoreIROpcode::PushTempMarkPair);
    pair->loc = &stmt2;
    pair->stmt_loc = &stmt2;
    b.createReturnNothing()->stmt_loc = &stmt1;
    fn.max_value_id = 32;

    std::string error;
    std::unique_ptr<QoreIRFunction> ir = roundTrip(pgm, fn, error);
    expect(ir && error.empty(), "the function with statement locations is read back");
    if (!ir) {
        fprintf(stderr, "error: %s\n", error.c_str());
        return;
    }
    auto& e = ir->blocks[0]->instructions;
    auto& n = ir->blocks[1]->instructions;
    expect(e.size() == 5 && n.size() == 5, "the instructions are read back, the pair as two marks");
    if (e.size() != 5 || n.size() != 5) {
        return;
    }
    expect(same(e[0]->stmt_loc, 3, 6, "stmt.q"), "the statement's location is read");
    expect(same(e[0]->loc, 5, 5, "stmt.q"), "the instruction's own location is kept");
    expect(e[1]->stmt_loc == e[0]->stmt_loc, "the instructions of a statement share its location");
    expect(same(e[2]->stmt_loc, 4, 4, "stmt.q") && e[2]->stmt_loc != e[0]->stmt_loc,
        "a nested statement has a location of its own");
    expect(e[2]->stmt_loc == e[2]->loc, "a statement location with the lines of the instruction's own is that one");
    expect(!e[3]->stmt_loc, "an instruction outside the statements has no statement location");
    expect(e[4]->stmt_loc == e[0]->stmt_loc, "an instruction after one without a location takes the table's entry");
    expect(n[0]->stmt_loc == e[2]->stmt_loc, "a statement's location in another block is the same");
    expect(n[1]->stmt_loc == e[0]->stmt_loc, "a statement's location after a nested statement's is the same");
    expect(n[2]->opcode == QoreIROpcode::PushTempMark && n[3]->opcode == QoreIROpcode::PushTempMark
        && n[2]->stmt_loc == e[2]->stmt_loc && n[3]->stmt_loc == e[2]->stmt_loc,
        "both marks of a pair have the pair's statement location");
    expect(n[4]->stmt_loc == e[0]->stmt_loc, "the return has the statement's location");
}

//! the IR function of a handler written in an instruction of a function has a statement location table of its own:
//! the function's table continues after it, and the handler's locations are its own
static void nestedTable(QoreProgram* pgm) {
    QoreProgramLocation stmt1("stmt.q", 3, 6);
    QoreProgramLocation stmt2("stmt.q", 4, 4);

    auto handler = std::make_shared<QoreIRFunction>("handler", "handler");
    {
        QoreIRBuilder hb(handler.get());
        hb.setBlock(handler->createBlock("entry"));
        hb.createConstInt(1)->stmt_loc = &stmt2;
        // a location of the enclosing function's table is a new entry of the handler's
        hb.createConstInt(2)->stmt_loc = &stmt1;
        hb.createReturnNothing()->stmt_loc = &stmt1;
        handler->max_value_id = 32;
    }

    QoreIRFunction fn("nested-table", "nested-table");
    QoreIRBuilder b(&fn);
    QoreIRBasicBlock* entry = fn.createBlock("entry");
    b.setBlock(entry);
    b.createConstInt(1)->stmt_loc = &stmt1;
    QoreIRInstruction* obe = entry->appendInstruction<QoreIROnBlockExitInstruction>(OBE_Unconditional, handler);
    obe->stmt_loc = &stmt1;
    // the location of the instruction before the handler's function, which the function's table continues with
    b.createConstInt(2)->stmt_loc = &stmt1;
    // a location of the handler's table only is a new entry of the function's
    b.createConstInt(3)->stmt_loc = &stmt2;
    b.createReturnNothing()->stmt_loc = &stmt1;
    fn.max_value_id = 32;

    std::string error;
    std::unique_ptr<QoreIRFunction> ir = roundTrip(pgm, fn, error);
    expect(ir && error.empty(), "the function with a handler is read back");
    if (!ir) {
        fprintf(stderr, "error: %s\n", error.c_str());
        return;
    }
    auto& e = ir->blocks[0]->instructions;
    expect(e.size() == 5 && e[1]->opcode == QoreIROpcode::OnBlockExit, "the instructions are read back");
    if (e.size() != 5 || e[1]->opcode != QoreIROpcode::OnBlockExit) {
        return;
    }
    const auto* robe = static_cast<const QoreIROnBlockExitInstruction*>(e[1].get());
    expect(robe->handler_ir && robe->handler_ir->blocks.size() == 1
        && robe->handler_ir->blocks[0]->instructions.size() == 3, "the handler's function is read back");
    if (!robe->handler_ir || robe->handler_ir->blocks.size() != 1
            || robe->handler_ir->blocks[0]->instructions.size() != 3) {
        return;
    }
    auto& h = robe->handler_ir->blocks[0]->instructions;
    expect(same(h[0]->stmt_loc, 4, 4, "stmt.q") && same(h[1]->stmt_loc, 3, 6, "stmt.q")
        && h[2]->stmt_loc == h[1]->stmt_loc, "the handler's instructions have their statement locations");
    expect(h[1]->stmt_loc != e[0]->stmt_loc, "the handler's function has a location table of its own");
    expect(e[1]->stmt_loc == e[0]->stmt_loc && e[2]->stmt_loc == e[0]->stmt_loc,
        "the function's table continues after the handler's function");
    expect(same(e[3]->stmt_loc, 4, 4, "stmt.q") && e[3]->stmt_loc != h[0]->stmt_loc,
        "a location of the handler's table only is a new entry of the function's");
    expect(e[4]->stmt_loc == e[0]->stmt_loc, "the return has the function's statement location");
}

//! Checks refusal of an image written before QORE_AOT_STATEMENT_LOCATIONS_VERSION
static void earlierFormat(QoreProgram* pgm) {
    QoreAOTBinaryWriter writer;
    uint32_t section = writer.beginSection(QoreAOTSectionType::TOPLEVEL);
    // the function header and its block, as serializeIRFunction() wrote them in that format
    writer.writeStringRef("earlier-format");
    writer.writeU32(32); // maximum value ID
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
    writer.writeU16(3); // instructions
    for (QoreIROpcode op : {QoreIROpcode::PushTempMark, QoreIROpcode::PushTempMark, QoreIROpcode::ReturnNothing}) {
        int line = 7;
        writer.writeU16(static_cast<uint16_t>(op));
        writer.writeU8(static_cast<uint8_t>(QoreIRInstGroup::Base));
        writer.writeU32(0); // result
        writer.writeU8(0); // operands
        writer.writeU16(0xffff); // no exception target
        writer.writeU32(static_cast<uint32_t>(line)); // source start/end line
        writer.writeU32(static_cast<uint32_t>(line));
        writer.writeStringRef("earlier.q");
    }
    writer.endSection(section);
    QoreAOTBinaryHeader header{};
    header.magic = QORE_AOT_BINARY_MAGIC;
    header.version = QORE_AOT_STATEMENT_LOCATIONS_VERSION - 1;
    header.feature_flags = writer.feature_flags;
    std::vector<uint8_t> data;
    if (!writer.finalize(header, data)) {
        expect(false, "the image of the earlier format is finalized");
        return;
    }
    std::string error;
    std::unique_ptr<QoreIRFunction> ir = read(pgm, data, error);
    expect(!ir && error.find("unsupported binary format version 46 ") != std::string::npos,
        "the reader rejects the earlier format before decoding statement locations");
}

int main() {
    qore_init(QL_MIT, "UTF-8", false, QLO_DISABLE_SIGNAL_HANDLING);
    QoreProgram* pgm = qore_create_program(PO_NEW_STYLE);
    roundTripLocations(pgm);
    nestedTable(pgm);
    earlierFormat(pgm);
    qore_destroy_program(pgm);
    qore_cleanup();
    if (ok) {
        printf("PASS: %u AOT IR statement location checks\n", count);
    }
    return ok ? 0 : 1;
}
