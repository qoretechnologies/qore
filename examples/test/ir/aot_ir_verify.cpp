/* -*- mode: c++; indent-tabs-mode: nil -*- */
// Copyright (C) 2026 Qore Technologies, s.r.o.
// SPDX-License-Identifier: LGPL-2.1-or-later

// qore#6468: IR read from an AOT module is verified before it is used.  Each case builds a valid IR function, writes it
// with the AOT writer and reads it with the reader, which must accept it; then it writes a changed copy, which the
// reader must refuse.  A change that the writer cannot express (a block index beyond the blocks of the function) is
// made in the written bytes: the function is written twice, with the field set to two different blocks, and the field
// is the two bytes at which the two writes differ; the checksum that follows the metadata is then written again with
// the qore-aot-reseal helper, as the other tests of patched IR do, so that the reader's checks of the IR see the patch.

#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <qore/intern/QoreIR.h>
#include <qore/intern/QoreIRBuilder.h>
#include <qore/intern/QoreAOTBinary.h>
#include <qore/intern/QoreJITIncludes.h>

#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

using Build = std::function<void(QoreIRFunction&, QoreIRBuilder&)>;
using Mutate = std::function<void(QoreIRFunction&)>;

//! the message of a block index beyond the blocks of the function
static const char* const BlockBeyond = "beyond the";
//! the message of a missing target
static const char* const NoTarget = "malformed IR: the instruction has no";

//! returns the first instruction with the given opcode
template <typename T = QoreIRInstruction>
static T* find(QoreIRFunction& fn, QoreIROpcode opcode) {
    for (auto& block : fn.blocks) {
        for (auto& inst : block->instructions) {
            if (inst->opcode == opcode) {
                return static_cast<T*>(inst.get());
            }
        }
    }
    fprintf(stderr, "fixture has no opcode %u\n", static_cast<unsigned>(opcode));
    abort();
}

//! returns the highest result or operand id of the function, which qcc writes as its highest value id
static uint32_t maxId(const QoreIRFunction& fn) {
    uint32_t max_id = 0;
    for (const auto& block : fn.blocks) {
        for (const auto& inst : block->instructions) {
            if (inst->result.isValid()) {
                max_id = std::max(max_id, inst->result.id);
            }
            for (const QoreIRValue& op : inst->operands) {
                if (op.isValid()) {
                    max_id = std::max(max_id, op.id);
                }
            }
        }
    }
    return std::max(max_id, 127u);
}

//! writes the function with the AOT writer
static bool write(const QoreIRFunction& fn, std::vector<uint8_t>& data, std::string& error) {
    QoreAOTBinaryWriter writer;
    writer.feature_flags |= QORE_AOT_FEAT_LVPATH_SLICE | QORE_AOT_FEAT_LVPATH_PATTERN;
    uint32_t section = writer.beginSection(QoreAOTSectionType::TOPLEVEL);
    if (!serializeIRFunction(writer, fn, [](QoreAOTBinaryWriter& w, const QoreValue&) {
        w.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_NOTHING));
        return true;
    })) {
        error = "cannot write the function";
        return false;
    }
    writer.endSection(section);
    QoreAOTBinaryHeader header{};
    header.magic = QORE_AOT_BINARY_MAGIC;
    header.version = QORE_AOT_BINARY_VERSION;
    header.feature_flags = writer.feature_flags;
    if (!writer.finalize(header, data)) {
        error = "cannot finalize the payload";
        return false;
    }
    return true;
}

//! reads the function with the reader
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

//! the qore-aot-reseal helper (the first argument of the test), which writes the checksum of patched metadata
static const char* reseal_helper = nullptr;
//! the message of metadata that does not match its checksum
static const char* const BadChecksum = "checksum";

//! writes the checksum of the patched bytes with the qore-aot-reseal helper, as the other tests of patched IR do
static bool reseal(std::vector<uint8_t>& data, std::string& error) {
    if (!reseal_helper) {
        error = "no qore-aot-reseal helper given";
        return false;
    }
    const char* tmp = getenv("TMPDIR");
    std::string path_str = std::string(tmp && *tmp ? tmp : "/tmp") + "/qore-aot-ir-verify-XXXXXX";
    std::vector<char> path_buf(path_str.begin(), path_str.end());
    path_buf.push_back('\0');
    char* path = path_buf.data();
    int fd = mkstemp(path);
    if (fd < 0) {
        error = "cannot create a temporary file";
        return false;
    }
    bool ok = ::write(fd, data.data(), data.size()) == static_cast<ssize_t>(data.size());
    close(fd);
    if (ok) {
        std::string cmd = std::string("'") + reseal_helper + "' '" + path + "' > /dev/null 2>&1";
        ok = !system(cmd.c_str());
        if (!ok) {
            error = "qore-aot-reseal failed";
        }
    }
    if (ok) {
        FILE* f = fopen(path, "rb");
        ok = f;
        if (f) {
            std::vector<uint8_t> resealed;
            uint8_t buf[4096];
            size_t n;
            while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
                resealed.insert(resealed.end(), buf, buf + n);
            }
            fclose(f);
            data.swap(resealed);
        }
    }
    unlink(path);
    return ok;
}

//! builds a fixture function
static std::unique_ptr<QoreIRFunction> build(const char* label, const Build& b) {
    auto fn = std::make_unique<QoreIRFunction>(label, label);
    QoreIRBuilder builder(fn.get());
    builder.setBlock(fn->createBlock("entry"));
    b(*fn, builder);
    fn->max_value_id = maxId(*fn);
    return fn;
}

static unsigned count = 0;

//! checks that the reader accepts the function, or refuses it with the message once it is changed
static bool check(QoreProgram* pgm, const char* label, const Build& b, const Mutate& mutate = nullptr,
        const char* expected = nullptr) {
    ++count;
    std::unique_ptr<QoreIRFunction> fn = build(label, b);
    if (mutate) {
        mutate(*fn);
    }
    std::vector<uint8_t> data;
    std::string error;
    std::unique_ptr<QoreIRFunction> ir = write(*fn, data, error) ? read(pgm, data, error) : nullptr;
    bool ok = expected ? !ir && error.find(expected) != std::string::npos : ir && error.empty();
    if (!ok) {
        fprintf(stderr, "%s: accepted=%d, error='%s', expected='%s'\n", label, bool(ir), error.c_str(),
            expected ? expected : "success");
    }
    return ok;
}

//! checks that the reader refuses the function with a block index beyond its blocks in the field that \a set sets
/** \a set sets the field to the block with the given index; the function is written with the field set to two
    blocks, the field is where the writes differ, and it is set to an index beyond the blocks of the function
*/
static bool checkBlockIndex(QoreProgram* pgm, const char* label, const Build& b,
        const std::function<void(QoreIRFunction&, QoreIRBasicBlock*)>& set, bool resealed = true) {
    ++count;
    std::unique_ptr<QoreIRFunction> fa = build(label, b);
    std::unique_ptr<QoreIRFunction> fb = build(label, b);
    set(*fa, fa->blocks[1].get());
    set(*fb, fb->blocks[2].get());
    std::vector<uint8_t> da, db;
    std::string error;
    if (!write(*fa, da, error) || !write(*fb, db, error) || da.size() != db.size()) {
        fprintf(stderr, "%s: cannot write the variants: %s\n", label, error.c_str());
        return false;
    }
    // the checksum that follows the metadata differs too; the field is where the metadata differs
    size_t metadata_size = da.size() >= QORE_AOT_METADATA_CHECKSUM_SIZE
        ? da.size() - QORE_AOT_METADATA_CHECKSUM_SIZE : 0;
    size_t diff = da.size();
    unsigned ndiff = 0;
    for (size_t i = 0; i < metadata_size; ++i) {
        if (da[i] != db[i]) {
            if (diff == da.size()) {
                diff = i;
            }
            ++ndiff;
        }
    }
    // block indices 1 and 2: one byte, the low byte of a little-endian u16
    if (ndiff != 1 || diff + 1 >= metadata_size) {
        fprintf(stderr, "%s: the variants differ in %u byte(s)\n", label, ndiff);
        return false;
    }
    // the valid variant is accepted
    std::unique_ptr<QoreIRFunction> ir = read(pgm, da, error);
    if (!ir) {
        fprintf(stderr, "%s: the valid variant is refused: %s\n", label, error.c_str());
        return false;
    }
    // an index beyond the blocks of the function, but not the index of no block (0xffff), in metadata whose checksum
    // is written again, so that the reader's checks of the IR see it
    da[diff] = 0xf0;
    da[diff + 1] = 0x7f;
    error.clear();
    if (resealed && !reseal(da, error)) {
        fprintf(stderr, "%s: %s\n", label, error.c_str());
        return false;
    }
    ir = read(pgm, da, error);
    // without the checksum written again, the patch is refused at the checksum, before the IR is read
    bool ok = !ir && (resealed
        ? error.find(BlockBeyond) != std::string::npos && error.find("block") != std::string::npos
        : error.find(BadChecksum) != std::string::npos);
    if (!ok) {
        fprintf(stderr, "%s: accepted=%d, error='%s', expected %s\n", label, bool(ir), error.c_str(),
            resealed ? "a block index beyond the blocks" : "a checksum error");
    }
    return ok;
}

// the fixtures: each a valid function with at least three blocks

static void loop(QoreIRFunction& fn, QoreIRBuilder& b) {
    QoreIRValue cond = b.createConstBool(true)->result;
    QoreIRBasicBlock* a = fn.createBlock("a");
    QoreIRBasicBlock* c = fn.createBlock("c");
    QoreIRBasicBlock* done = fn.createBlock("done");
    b.createBranch(a);
    b.setBlock(a);
    b.createBranchIf(cond, c, done);
    b.setBlock(c);
    b.createBranch(done);
    b.setBlock(done);
    b.createReturnNothing();
}

static void iterator(QoreIRFunction& fn, QoreIRBuilder& b) {
    QoreIRValue list = b.createConstNothing()->result;
    QoreIRValue it = b.createIteratorCreate(list)->result;
    QoreIRBasicBlock* loop = fn.createBlock("loop");
    QoreIRBasicBlock* body = fn.createBlock("body");
    QoreIRBasicBlock* done = fn.createBlock("done");
    b.createBranch(loop);
    b.setBlock(loop);
    b.createIteratorNext(it, done, body);
    b.setBlock(body);
    b.createBranch(loop);
    b.setBlock(done);
    b.createIteratorRelease(it);
    b.createReturnNothing();
}

static void phi(QoreIRFunction& fn, QoreIRBuilder& b) {
    QoreIRValue cond = b.createConstBool(true)->result;
    QoreIRBasicBlock* a = fn.createBlock("a");
    QoreIRBasicBlock* c = fn.createBlock("c");
    QoreIRBasicBlock* join = fn.createBlock("join");
    b.createBranchIf(cond, a, c);
    b.setBlock(a);
    QoreIRValue v1 = b.createConstInt(1)->result;
    b.createBranch(join);
    b.setBlock(c);
    QoreIRValue v2 = b.createConstInt(2)->result;
    b.createBranch(join);
    b.setBlock(join);
    QoreIRValue v = b.createPhi({{v1, a}, {v2, c}})->result;
    b.createReturn(v);
}

static void switchInt(QoreIRFunction& fn, QoreIRBuilder& b) {
    QoreIRValue i = b.createConstInt(1)->result;
    QoreIRBasicBlock* a = fn.createBlock("a");
    QoreIRBasicBlock* c = fn.createBlock("c");
    b.createSwitchInt(i, a, {{1, c}});
    b.setBlock(a);
    b.createReturnNothing();
    b.setBlock(c);
    b.createReturnNothing();
}

static void switchString(QoreIRFunction& fn, QoreIRBuilder& b) {
    QoreIRValue s = b.createConstString("a")->result;
    QoreIRBasicBlock* a = fn.createBlock("a");
    QoreIRBasicBlock* c = fn.createBlock("c");
    b.createSwitchString(s, a, {{"a", c}});
    b.setBlock(a);
    b.createReturnNothing();
    b.setBlock(c);
    b.createReturnNothing();
}

int main(int argc, char** argv) {
    reseal_helper = argc > 1 ? argv[1] : nullptr;
    // the signal thread must stay disabled for valgrind, just as for qore -b
    qore_init(QL_MIT, "UTF-8", false, QLO_DISABLE_SIGNAL_HANDLING);
    QoreProgram* pgm = qore_create_program(PO_NEW_STYLE);
    bool ok = true;
    auto run = [&](const char* label, const Build& b, const Mutate& mutate = nullptr,
            const char* expected = nullptr) {
        ok = check(pgm, label, b, mutate, expected) && ok;
    };
    auto runIndex = [&](const char* label, const Build& b,
            const std::function<void(QoreIRFunction&, QoreIRBasicBlock*)>& set) {
        ok = checkBlockIndex(pgm, label, b, set) && ok;
    };

    // the valid fixtures
    run("loop", loop);
    run("iterator", iterator);
    run("phi", phi);
    run("switch int", switchInt);
    run("switch string", switchString);

    // block indices beyond the blocks of the function
    runIndex("Br target beyond", loop, [](QoreIRFunction& fn, QoreIRBasicBlock* to) {
        find<QoreIRBranchInstruction>(fn, QoreIROpcode::Br)->target = to;
    });
    runIndex("BrIf true target beyond", loop, [](QoreIRFunction& fn, QoreIRBasicBlock* to) {
        find<QoreIRBranchIfInstruction>(fn, QoreIROpcode::BrIf)->true_target = to;
    });
    runIndex("BrIf false target beyond", loop, [](QoreIRFunction& fn, QoreIRBasicBlock* to) {
        find<QoreIRBranchIfInstruction>(fn, QoreIROpcode::BrIf)->false_target = to;
    });
    runIndex("IteratorNext done target beyond", iterator, [](QoreIRFunction& fn, QoreIRBasicBlock* to) {
        find<QoreIRIteratorNextInstruction>(fn, QoreIROpcode::IteratorNext)->done_target = to;
    });
    runIndex("IteratorNext continue target beyond", iterator, [](QoreIRFunction& fn, QoreIRBasicBlock* to) {
        find<QoreIRIteratorNextInstruction>(fn, QoreIROpcode::IteratorNext)->continue_target = to;
    });
    runIndex("phi incoming block beyond", phi, [](QoreIRFunction& fn, QoreIRBasicBlock* to) {
        find<QoreIRPhiInstruction>(fn, QoreIROpcode::Phi)->incoming[0].block = to;
    });
    runIndex("exception target beyond", loop, [](QoreIRFunction& fn, QoreIRBasicBlock* to) {
        find(fn, QoreIROpcode::ConstBool)->exception_target = to;
    });

    // a patch whose checksum is not written again is refused at the checksum, before the IR is read
    ok = checkBlockIndex(pgm, "Br target beyond, not resealed", loop, [](QoreIRFunction& fn, QoreIRBasicBlock* to) {
        find<QoreIRBranchInstruction>(fn, QoreIROpcode::Br)->target = to;
    }, false) && ok;

    // targets that the instruction always branches to and does not have (a block of no function, which the writer
    // writes as no block)
    QoreIRBasicBlock foreign("foreign");
    QoreIRBasicBlock* none = &foreign;
    run("Br without target", loop, [none](QoreIRFunction& fn) {
        find<QoreIRBranchInstruction>(fn, QoreIROpcode::Br)->target = none;
    }, NoTarget);
    run("BrIf without true target", loop, [none](QoreIRFunction& fn) {
        find<QoreIRBranchIfInstruction>(fn, QoreIROpcode::BrIf)->true_target = none;
    }, NoTarget);
    run("BrIf without false target", loop, [none](QoreIRFunction& fn) {
        find<QoreIRBranchIfInstruction>(fn, QoreIROpcode::BrIf)->false_target = none;
    }, NoTarget);
    run("IteratorNext without done target", iterator, [none](QoreIRFunction& fn) {
        find<QoreIRIteratorNextInstruction>(fn, QoreIROpcode::IteratorNext)->done_target = none;
    }, NoTarget);
    run("IteratorNext without continue target", iterator, [none](QoreIRFunction& fn) {
        find<QoreIRIteratorNextInstruction>(fn, QoreIROpcode::IteratorNext)->continue_target = none;
    }, NoTarget);
    run("phi without incoming block", phi, [none](QoreIRFunction& fn) {
        find<QoreIRPhiInstruction>(fn, QoreIROpcode::Phi)->incoming[1].block = none;
    }, NoTarget);
    run("SwitchInt without default target", switchInt, [none](QoreIRFunction& fn) {
        find<QoreIRSwitchIntInstruction>(fn, QoreIROpcode::SwitchInt)->default_target = none;
    }, NoTarget);
    run("SwitchInt without case target", switchInt, [none](QoreIRFunction& fn) {
        find<QoreIRSwitchIntInstruction>(fn, QoreIROpcode::SwitchInt)->cases[0].target = none;
    }, NoTarget);
    run("SwitchString without default target", switchString, [none](QoreIRFunction& fn) {
        find<QoreIRSwitchStringInstruction>(fn, QoreIROpcode::SwitchString)->default_target = none;
    }, NoTarget);
    run("SwitchString without case target", switchString, [none](QoreIRFunction& fn) {
        find<QoreIRSwitchStringInstruction>(fn, QoreIROpcode::SwitchString)->cases[0].target = none;
    }, NoTarget);

    qore_destroy_program(pgm);
    qore_cleanup();
    if (!ok) {
        fprintf(stderr, "FAIL: %u AOT IR verification checks\n", count);
        return 1;
    }
    printf("PASS: %u AOT IR verification checks\n", count);
    return 0;
}
