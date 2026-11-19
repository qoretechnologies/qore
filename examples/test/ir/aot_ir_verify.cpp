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
//! the message of a use of a value that its definition does not dominate
static const char* const NotDominated = "which is not defined on every path to it";

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

//! a handler reached from an instruction that can raise an exception; the handler returns the value given
static void handler(QoreIRFunction& fn, QoreIRBuilder& b, bool use_result) {
    QoreIRValue before = b.createConstInt(1)->result;
    QoreIRBasicBlock* h = fn.createBlock("handler");
    QoreIRBasicBlock* next = fn.createBlock("next");
    // an instruction that can raise an exception into the handler
    QoreIRInstruction* raising = b.createConstInt(2);
    raising->exception_target = h;
    b.createBranch(next);
    b.setBlock(next);
    b.createReturn(before);
    b.setBlock(h);
    b.createReturn(use_result ? raising->result : before);
}

//! returns a new constant with a value id of its own
static std::unique_ptr<QoreIRConstInstruction> newConst(QoreIRFunction& fn) {
    auto c = std::make_unique<QoreIRConstInstruction>();
    c->result = QoreIRValue(++fn.max_value_id);
    return c;
}

//! a release of a temporary on a path on which the temporary may not have been created, as the lowering of a
//! statement that leaves on several paths gives (\a kept: the release keeps a value, as a fused pipeline's stage)
static void releaseMaybe(QoreIRFunction& fn, QoreIRBuilder& b, bool kept) {
    QoreIRValue cond = b.createConstBool(true)->result;
    QoreIRValue k = b.createConstInt(0)->result;
    QoreIRBasicBlock* a = fn.createBlock("a");
    QoreIRBasicBlock* join = fn.createBlock("join");
    b.createBranchIf(cond, a, join);
    b.setBlock(a);
    QoreIRValue temp = b.createConstString("t")->result;
    b.createBranch(join);
    b.setBlock(join);
    if (kept) {
        b.createReleaseStageInput(temp, k);
    } else {
        b.createReleaseOwnedTemp(temp);
    }
    b.createReturnNothing();
}

//! a block that an instruction that can raise an exception reaches both by its exception edge and by the branch at
//! the end of its block, with a phi of the value given (\a after: the value of the raising instruction)
static void handlerPhi(QoreIRFunction& fn, QoreIRBuilder& b, bool after) {
    QoreIRValue before = b.createConstInt(1)->result;
    QoreIRBasicBlock* h = fn.createBlock("handler");
    QoreIRInstruction* raising = b.createConstInt(2);
    raising->exception_target = h;
    b.createBranch(h);
    b.setBlock(h);
    QoreIRValue v = b.createPhi({{after ? raising->result : before, fn.blocks[0].get()}})->result;
    b.createReturn(v);
}

//! a throw to a handler that returns a value defined before the throw
static void throwHandler(QoreIRFunction& fn, QoreIRBuilder& b) {
    QoreIRValue before = b.createConstInt(1)->result;
    QoreIRBasicBlock* h = fn.createBlock("handler");
    b.createThrow(before, h);
    b.setBlock(h);
    b.createReturn(before);
}

//! an iterator released on a path on which it may not have been created (\a dominated: on the path that creates it)
static void iteratorRelease(QoreIRFunction& fn, QoreIRBuilder& b, bool dominated) {
    QoreIRValue cond = b.createConstBool(true)->result;
    QoreIRValue list = b.createConstNothing()->result;
    QoreIRBasicBlock* a = fn.createBlock("a");
    QoreIRBasicBlock* join = fn.createBlock("join");
    b.createBranchIf(cond, a, join);
    b.setBlock(a);
    QoreIRValue it = b.createIteratorCreate(list)->result;
    if (dominated) {
        b.createIteratorRelease(it);
    }
    b.createBranch(join);
    b.setBlock(join);
    if (!dominated) {
        b.createIteratorRelease(it);
    }
    b.createReturnNothing();
}

//! the loop of the fold of a fused pipeline (qore#6549): the flag that the loop carries is merged by a phi of the
//! constant True from the block that starts the accumulator and from the block that adds to it; \a fixed: the
//! constant is defined before the branch to the two blocks, otherwise in the block that starts the accumulator only
static void foldFlag(QoreIRFunction& fn, QoreIRBuilder& b, bool fixed) {
    QoreIRValue initial = b.createConstBool(false)->result;
    QoreIRBasicBlock* entry = fn.blocks[0].get();
    QoreIRBasicBlock* header = fn.createBlock("header");
    QoreIRBasicBlock* step = fn.createBlock("step");
    QoreIRBasicBlock* init = fn.createBlock("init");
    QoreIRBasicBlock* fold = fn.createBlock("fold");
    QoreIRBasicBlock* cont = fn.createBlock("cont");
    QoreIRBasicBlock* done = fn.createBlock("done");
    b.createBranch(header);
    // the header: the flag, and whether there is another element
    b.setBlock(header);
    QoreIRPhiInstruction* flag = b.createPhi({{initial, entry}});
    QoreIRValue more = b.createConstBool(true)->result;
    QoreIRValue true_val;
    if (fixed) {
        true_val = b.createConstBool(true)->result;
    }
    b.createBranchIf(more, step, done);
    // the element: the flag decides between starting the accumulator and adding to it
    b.setBlock(step);
    b.createBranchIf(flag->result, fold, init);
    b.setBlock(init);
    if (!fixed) {
        true_val = b.createConstBool(true)->result;
    }
    b.createBranch(cont);
    b.setBlock(fold);
    b.createBranch(cont);
    b.setBlock(cont);
    QoreIRValue next = b.createPhi({{true_val, init}, {true_val, fold}})->result;
    b.createBranch(header);
    flag->incoming.push_back({next, cont});
    flag->operands.push_back(next);
    b.setBlock(done);
    b.createReturn(flag->result);
}

static const char* const Released = "which is released on a path to it";
static const char* const Freed = "whose object is released on a path to it";
static const char* const NotInnermost = "that is not the innermost one open on every path to it";
static const char* const Differ = "open differ on the paths into block";

//! an iterator created on either of two paths, whose handle a phi merges and the join releases (\a original: the
//! iterator of the first path is released through its own handle before the phi copies it)
static void iteratorPhi(QoreIRFunction& fn, QoreIRBuilder& b, bool original) {
    QoreIRValue cond = b.createConstBool(true)->result;
    QoreIRValue list = b.createConstNothing()->result;
    QoreIRBasicBlock* a = fn.createBlock("a");
    QoreIRBasicBlock* c = fn.createBlock("c");
    QoreIRBasicBlock* join = fn.createBlock("join");
    b.createBranchIf(cond, a, c);
    b.setBlock(a);
    QoreIRValue it1 = b.createIteratorCreate(list)->result;
    if (original) {
        b.createIteratorRelease(it1);
    }
    b.createBranch(join);
    b.setBlock(c);
    QoreIRValue it2 = b.createIteratorCreate(list)->result;
    b.createBranch(join);
    b.setBlock(join);
    QoreIRValue p = b.createPhi({{it1, a}, {it2, c}})->result;
    b.createIteratorRelease(p);
    b.createReturnNothing();
}

//! an iterator whose handle a phi copies, released through the copy; \a use_original: the iterator is then stepped
//! through its original handle
static void iteratorCopy(QoreIRFunction& fn, QoreIRBuilder& b, bool use_original) {
    QoreIRValue list = b.createConstNothing()->result;
    QoreIRValue it = b.createIteratorCreate(list)->result;
    QoreIRBasicBlock* a = fn.createBlock("a");
    QoreIRBasicBlock* body = fn.createBlock("body");
    QoreIRBasicBlock* done = fn.createBlock("done");
    b.createBranch(a);
    b.setBlock(a);
    QoreIRValue copy = b.createPhi({{it, fn.blocks[0].get()}})->result;
    b.createIteratorRelease(copy);
    if (use_original) {
        b.createIteratorNext(it, done, body);
    } else {
        b.createBranch(done);
    }
    b.setBlock(body);
    b.createBranch(done);
    b.setBlock(done);
    b.createReturnNothing();
}

//! a temp scope opened and closed in a block of its own, or (\a branch) on one path only
static void tempScope(QoreIRFunction& fn, QoreIRBuilder& b, bool branch) {
    QoreIRValue cond = b.createConstBool(true)->result;
    QoreIRBasicBlock* a = fn.createBlock("a");
    QoreIRBasicBlock* join = fn.createBlock("join");
    b.createBranchIf(cond, a, join);
    b.setBlock(a);
    b.createPushTempMark();
    b.createConstString("t");
    if (!branch) {
        b.createDiscardTemps();
    }
    b.createBranch(join);
    b.setBlock(join);
    b.createReturnNothing();
}

//! a temp scope opened before a loop and closed in its body (\a in_body: opened in the body, as the lowering does)
static void tempScopeLoop(QoreIRFunction& fn, QoreIRBuilder& b, bool in_body) {
    QoreIRValue list = b.createConstNothing()->result;
    QoreIRValue it = b.createIteratorCreate(list)->result;
    QoreIRBasicBlock* loop = fn.createBlock("loop");
    QoreIRBasicBlock* body = fn.createBlock("body");
    QoreIRBasicBlock* done = fn.createBlock("done");
    QoreIRInstruction* mark = nullptr;
    if (!in_body) {
        mark = b.createPushTempMark();
    }
    b.createBranch(loop);
    b.setBlock(loop);
    b.createIteratorNext(it, done, body);
    b.setBlock(body);
    if (in_body) {
        b.createPushTempMark();
    }
    b.createConstString("t");
    QoreIRInstruction* discard = b.createDiscardTemps();
    if (mark) {
        discard->temp_scope_id = mark->temp_scope_id;
    }
    b.createBranch(loop);
    b.setBlock(done);
    b.createReturnNothing();
}

//! a raising instruction in a temp scope nested in the temp scope of its handler, and in a scope that the handler's
//! landing pad leaves (\a landing_pad: the handler starts with a landing pad); the handler's path meets the normal
//! path in the outer temp scope
static void scopeHandler(QoreIRFunction& fn, QoreIRBuilder& b, bool landing_pad) {
    QoreIRBasicBlock* h = fn.createBlock("handler");
    QoreIRBasicBlock* next = fn.createBlock("next");
    QoreIRBasicBlock* join = fn.createBlock("join");
    b.createPushTempMark();
    b.createScopeEnter(1);
    b.createPushTempMark();
    QoreIRInstruction* raising = b.createConstInt(2);
    raising->exception_target = h;
    b.createDiscardTemps();
    b.createScopeExit(1);
    b.createBranch(next);
    b.setBlock(next);
    b.createBranch(join);
    b.setBlock(h);
    if (landing_pad) {
        b.createLandingPad(0);
    }
    b.createBranch(join);
    b.setBlock(join);
    b.createDiscardTemps();
    b.createReturnNothing();
}

//! two nested implicit arguments, restored innermost first (\a swapped: outermost first)
static void implicitArgs(QoreIRFunction& fn, QoreIRBuilder& b, bool swapped) {
    QoreIRValue v1 = b.createConstInt(1)->result;
    QoreIRValue v2 = b.createConstInt(2)->result;
    QoreIRValue outer = b.createPushImplicitArg(v1)->result;
    QoreIRValue inner = b.createPushImplicitArg(v2)->result;
    b.createPopImplicitArg(swapped ? outer : inner);
    b.createPopImplicitArg(swapped ? inner : outer);
    QoreIRBasicBlock* a = fn.createBlock("a");
    QoreIRBasicBlock* c = fn.createBlock("c");
    b.createBranch(a);
    b.setBlock(a);
    b.createBranch(c);
    b.setBlock(c);
    b.createReturnNothing();
}

//! two nested contexts, destroyed innermost first (\a swapped: outermost first)
static void contexts(QoreIRFunction& fn, QoreIRBuilder& b, bool swapped) {
    QoreIRValue outer = b.createContext("a", QoreValue(), QoreValue(), QoreValue(), 0)->result;
    QoreIRValue inner = b.createContext("b", QoreValue(), QoreValue(), QoreValue(), 0)->result;
    b.createContextDestroy(swapped ? outer : inner);
    b.createContextDestroy(swapped ? inner : outer);
    QoreIRBasicBlock* a = fn.createBlock("a");
    QoreIRBasicBlock* c = fn.createBlock("c");
    b.createBranch(a);
    b.setBlock(a);
    b.createBranch(c);
    b.setBlock(c);
    b.createReturnNothing();
}

//! a value used in the block after the one that releases it (\a decref: released with Decref, ReleaseOwnedTemp
//! otherwise)
static void releaseThenUse(QoreIRFunction& fn, QoreIRBuilder& b, bool decref) {
    QoreIRValue t = b.createConstString("t")->result;
    QoreIRBasicBlock* a = fn.createBlock("a");
    QoreIRBasicBlock* c = fn.createBlock("c");
    b.createBranch(a);
    b.setBlock(a);
    if (decref) {
        b.createDecref(t);
    } else {
        b.createReleaseOwnedTemp(t);
    }
    b.createBranch(c);
    b.setBlock(c);
    b.createReturn(t);
}

//! exceptions set aside and raised again (\a twice: raised again twice)
static void resumeTwice(QoreIRFunction& fn, QoreIRBuilder& b, bool twice) {
    QoreIRValue h = b.createExceptionSuspend()->result;
    QoreIRBasicBlock* a = fn.createBlock("a");
    QoreIRBasicBlock* c = fn.createBlock("c");
    b.createBranch(a);
    b.setBlock(a);
    b.createExceptionResume(h);
    b.createBranch(c);
    b.setBlock(c);
    if (twice) {
        b.createExceptionResume(h);
    }
    b.createReturnNothing();
}

//! a step of an iterator in the entry block, whose targets return (the done target and the continue target can be
//! any of the other blocks)
static void iteratorStep(QoreIRFunction& fn, QoreIRBuilder& b) {
    QoreIRValue list = b.createConstNothing()->result;
    QoreIRValue it = b.createIteratorCreate(list)->result;
    QoreIRBasicBlock* a = fn.createBlock("a");
    QoreIRBasicBlock* c = fn.createBlock("c");
    QoreIRBasicBlock* d = fn.createBlock("d");
    b.createIteratorNext(it, d, a);
    for (QoreIRBasicBlock* block : {a, c, d}) {
        b.setBlock(block);
        b.createIteratorRelease(it);
        b.createReturnNothing();
    }
}

//! a value released in a loop: defined in the body before its release (\a before_loop: defined before the loop)
static void releaseInLoop(QoreIRFunction& fn, QoreIRBuilder& b, bool before_loop) {
    QoreIRValue list = b.createConstNothing()->result;
    QoreIRValue it = b.createIteratorCreate(list)->result;
    QoreIRValue outer = b.createConstString("o")->result;
    QoreIRBasicBlock* loop = fn.createBlock("loop");
    QoreIRBasicBlock* body = fn.createBlock("body");
    QoreIRBasicBlock* done = fn.createBlock("done");
    b.createBranch(loop);
    b.setBlock(loop);
    b.createIteratorNext(it, done, body);
    b.setBlock(body);
    QoreIRValue t = before_loop ? outer : b.createConstString("t")->result;
    b.createListSize(t);
    b.createReleaseOwnedTemp(t);
    b.createBranch(loop);
    b.setBlock(done);
    b.createReturnNothing();
}

static const char* const NotCreated = "changes a container that is not created by an instruction that creates its type";

//! a list that the function creates and appends to, then stores to and reads (\a container: the instruction that
//! creates the container, \a read: the list is read with ListGetInt)
static void listChange(QoreIRFunction& fn, QoreIRBuilder& b, QoreIROpcode container, bool read) {
    QoreIRValue zero = b.createConstInt(0)->result;
    QoreIRValue one = b.createConstInt(1)->result;
    QoreIRValue list;
    switch (container) {
        case QoreIROpcode::CreateEmptyList:
            list = b.createEmptyList()->result;
            break;
        case QoreIROpcode::CreateSizedList:
            list = b.createSizedList(one)->result;
            break;
        default:
            list = b.createConstString("not a list")->result;
            break;
    }
    QoreIRBasicBlock* a = fn.createBlock("a");
    QoreIRBasicBlock* c = fn.createBlock("c");
    b.createBranch(a);
    b.setBlock(a);
    b.createListAppend(list, one);
    b.createListSetInt(list, zero, one);
    b.createBranch(c);
    b.setBlock(c);
    if (read) {
        b.createListGetInt(list, zero);
    }
    b.createReturn(list);
}

// the runtime helpers of the JIT that store to a list (JITRuntime.cpp)
extern "C" void qore_rt_list_set_int(uint64_t list_bits, int64_t index, int64_t value);
extern "C" void qore_rt_list_set_float(uint64_t list_bits, int64_t index, double value);
extern "C" void qore_rt_list_set_value(uint64_t list_bits, int64_t index, uint64_t value_bits);

//! checks that the runtime stores to a list only at an index below its length or at its end
static bool checkListStores() {
    ++count;
    QoreListNode* l = new QoreListNode(autoTypeInfo);
    QoreValue lv(l);
    uint64_t bits;
    memcpy(&bits, &lv, sizeof bits);
    QoreValue s(new QoreStringNode("s"));
    uint64_t sbits;
    memcpy(&sbits, &s, sizeof sbits);
    qore_rt_list_set_int(bits, 0, 1);
    qore_rt_list_set_float(bits, 1, 2.0);
    // beyond the end, and a negative index: no store (the value given to qore_rt_list_set_value is released)
    qore_rt_list_set_int(bits, 3, 3);
    qore_rt_list_set_int(bits, -1, 3);
    qore_rt_list_set_float(bits, -1, 3.0);
    // each store is given a reference of its own
    s.refSelf();
    qore_rt_list_set_value(bits, -1, sbits);
    s.refSelf();
    qore_rt_list_set_value(bits, 1000000, sbits);
    bool ok = l->size() == 2 && l->retrieveEntry(0).getAsBigInt() == 1 && l->retrieveEntry(1).getAsFloat() == 2.0
        && s.getInternalNode()->reference_count() == 1;
    if (!ok) {
        fprintf(stderr, "list stores: size %zu, string references %d\n", l->size(),
            s.getInternalNode()->reference_count());
    }
    l->deref(nullptr);
    s.discard(nullptr);
    return ok;
}

int main(int argc, char** argv) {
    // the fixtures write malformed IR, which the writer refuses in a debug build otherwise
    setenv("QORE_AOT_TEST_NO_WRITE_VERIFY", "1", 1);
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
    runIndex("IteratorNext done target beyond", iteratorStep, [](QoreIRFunction& fn, QoreIRBasicBlock* to) {
        find<QoreIRIteratorNextInstruction>(fn, QoreIROpcode::IteratorNext)->done_target = to;
    });
    runIndex("IteratorNext continue target beyond", iteratorStep, [](QoreIRFunction& fn, QoreIRBasicBlock* to) {
        find<QoreIRIteratorNextInstruction>(fn, QoreIROpcode::IteratorNext)->continue_target = to;
    });
    runIndex("phi incoming block beyond", phi, [](QoreIRFunction& fn, QoreIRBasicBlock* to) {
        find<QoreIRPhiInstruction>(fn, QoreIROpcode::Phi)->incoming[0].block = to;
    });
    runIndex("exception target beyond", loop, [](QoreIRFunction& fn, QoreIRBasicBlock* to) {
        // the branch of the entry block, after the value that the targets use
        find(fn, QoreIROpcode::Br)->exception_target = to;
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

    // the structure of the blocks
    run("block without terminator", loop, [](QoreIRFunction& fn) {
        // the entry block ends with its constant
        fn.blocks[0]->instructions.pop_back();
    }, "has no terminator");
    // the instructions after the terminator of a block never run, and their values are not defined
    run("instructions after the terminator of a block", loop, [](QoreIRFunction& fn) {
        fn.blocks[2]->instructions.push_back(newConst(fn));
    });
    run("use of a value defined after the terminator of a block", loop, [](QoreIRFunction& fn) {
        auto c = newConst(fn);
        find<QoreIRBranchIfInstruction>(fn, QoreIROpcode::BrIf)->condition = c->result;
        fn.blocks[0]->instructions.push_back(std::move(c));
    }, "which no instruction that can run defines");
    run("empty block", loop, [](QoreIRFunction& fn) {
        fn.blocks[2]->instructions.clear();
    }, "has no instructions");
    run("phi after another instruction", phi, [](QoreIRFunction& fn) {
        auto& insts = fn.blocks[3]->instructions;
        // the phi is the first instruction of the join block: an instruction is put before it
        insts.insert(insts.begin(), newConst(fn));
    }, "a phi follows another instruction");
    run("phi with an invalid value kind", phi, [](QoreIRFunction& fn) {
        find<QoreIRPhiInstruction>(fn, QoreIROpcode::Phi)->value_kind = static_cast<QoreIRPhiValueKind>(9);
    }, "invalid value kind");

    // values used where their definitions do not dominate the use
    run("use of a value of another path", phi, [](QoreIRFunction& fn) {
        // the join block returns the value of one of its predecessors instead of the phi
        find<QoreIRReturnInstruction>(fn, QoreIROpcode::Return)->value = find(fn, QoreIROpcode::ConstInt)->result;
    }, NotDominated);
    run("use before the definition", loop, [](QoreIRFunction& fn) {
        // the branch of block a uses a value that a later block defines
        auto c = newConst(fn);
        find<QoreIRBranchIfInstruction>(fn, QoreIROpcode::BrIf)->condition = c->result;
        auto& insts = fn.blocks[2]->instructions;
        insts.insert(insts.begin(), std::move(c));
    }, NotDominated);
    run("use of a value of a block that never runs", loop, [](QoreIRFunction& fn) {
        QoreIRBasicBlock* dead = fn.createBlock("dead");
        auto c = newConst(fn);
        find<QoreIRBranchIfInstruction>(fn, QoreIROpcode::BrIf)->condition = c->result;
        dead->instructions.push_back(std::move(c));
        auto br = std::make_unique<QoreIRBranchInstruction>();
        br->target = fn.blocks[3].get();
        dead->instructions.push_back(std::move(br));
    }, "which no instruction that can run defines");
    run("phi incoming value of another predecessor", phi, [](QoreIRFunction& fn) {
        auto* p = find<QoreIRPhiInstruction>(fn, QoreIROpcode::Phi);
        std::swap(p->incoming[0].value, p->incoming[1].value);
        p->operands = {p->incoming[0].value, p->incoming[1].value};
    }, "which is not defined at the end of that block");

    // the release of a temporary that the function owns may be on a path on which it was not created, but the
    // temporary must be defined by an instruction that can run, and the value that the release keeps must be defined
    // on every path
    run("release of a temporary that may not have been created", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        releaseMaybe(fn, b, false);
    });
    run("release of a temporary that no instruction that can run defines",
        [](QoreIRFunction& fn, QoreIRBuilder& b) { releaseMaybe(fn, b, false); }, [](QoreIRFunction& fn) {
        // the temporary is defined in a block that never runs
        QoreIRBasicBlock* dead = fn.createBlock("dead");
        auto c = newConst(fn);
        find(fn, QoreIROpcode::ReleaseOwnedTemp)->operands[0] = c->result;
        dead->instructions.push_back(std::move(c));
        dead->instructions.push_back(std::make_unique<QoreIRReturnInstruction>());
    }, "which no instruction that can run defines");
    run("release keeping a value that may not be defined", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        releaseMaybe(fn, b, true);
    }, [](QoreIRFunction& fn) {
        // the kept value is the temporary of the other path
        auto* rel = find(fn, QoreIROpcode::ReleaseOwnedTemp);
        rel->operands[1] = find(fn, QoreIROpcode::ConstString)->result;
    }, NotDominated);

    // the values that a step of an iterator releases (those that the loop derived from the previous element) may
    // not be defined at the first step, but must be defined by an instruction that can run
    run("iterator step releasing a value of the loop body", iterator, [](QoreIRFunction& fn) {
        // the body block defines a value that the next step releases
        auto c = newConst(fn);
        find<QoreIRIteratorNextInstruction>(fn, QoreIROpcode::IteratorNext)->release_values.push_back(c->result);
        auto& insts = fn.blocks[2]->instructions;
        insts.insert(insts.begin(), std::move(c));
    });
    run("iterator step releasing a value that no instruction that can run defines", iterator, [](QoreIRFunction& fn) {
        QoreIRBasicBlock* dead = fn.createBlock("dead");
        auto c = newConst(fn);
        find<QoreIRIteratorNextInstruction>(fn, QoreIROpcode::IteratorNext)->release_values.push_back(c->result);
        dead->instructions.push_back(std::move(c));
        dead->instructions.push_back(std::make_unique<QoreIRReturnInstruction>());
    }, "which no instruction that can run defines");

    // the element of a step of an iterator is defined on its continue edge only
    run("iterator element used in the loop body", iterator, [](QoreIRFunction& fn) {
        auto* next = find<QoreIRIteratorNextInstruction>(fn, QoreIROpcode::IteratorNext);
        next->result = QoreIRValue(++fn.max_value_id);
        auto ret = std::make_unique<QoreIRReturnInstruction>();
        ret->opcode = QoreIROpcode::Return;
        ret->has_value = true;
        ret->value = next->result;
        // the body returns the element instead of branching back
        fn.blocks[2]->instructions.back() = std::move(ret);
    });
    run("iterator element used after the iterator is done", iterator, [](QoreIRFunction& fn) {
        auto* next = find<QoreIRIteratorNextInstruction>(fn, QoreIROpcode::IteratorNext);
        next->result = QoreIRValue(++fn.max_value_id);
        auto ret = std::make_unique<QoreIRReturnInstruction>();
        ret->opcode = QoreIROpcode::Return;
        ret->has_value = true;
        ret->value = next->result;
        fn.blocks[3]->instructions.back() = std::move(ret);
    }, NotDominated);

    // the list of a step of a typed foreach is its operand
    run("typed foreach list that is not its operand", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        QoreIRValue list = b.createConstNothing()->result;
        QoreIRValue other = b.createConstNothing()->result;
        QoreIRValue index = b.createConstInt(0)->result;
        QoreIRValue limit = b.createConstInt(3)->result;
        QoreIRBasicBlock* loop = fn.createBlock("loop");
        QoreIRBasicBlock* body = fn.createBlock("body");
        QoreIRBasicBlock* done = fn.createBlock("done");
        b.createBranch(loop);
        b.setBlock(loop);
        b.createTypedForeachNext(list, index, limit, bigIntTypeInfo, done, body)->iterator = other;
        b.setBlock(body);
        b.createBranch(done);
        b.setBlock(done);
        b.createReturnNothing();
    }, nullptr, "that is not its operand");

    // a phi has an incoming value for each predecessor
    run("phi without the incoming value of a predecessor", phi, [](QoreIRFunction& fn) {
        auto* p = find<QoreIRPhiInstruction>(fn, QoreIROpcode::Phi);
        p->incoming.pop_back();
        p->operands.pop_back();
    }, "has no incoming value from its predecessor");

    // an iterator released on a path on which it may not exist is refused: only the releases of temporaries and of
    // the values of an iterator step are released if the frame holds them
    run("iterator released on its path", [](QoreIRFunction& fn, QoreIRBuilder& b) { iteratorRelease(fn, b, true); });
    run("iterator released on a path on which it may not exist", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        iteratorRelease(fn, b, false);
    }, nullptr, NotDominated);

    // a throw to its handler, and a block reached by an exception edge and a branch from the same block
    run("throw", throwHandler);
    run("handler phi of a value defined before the raising instruction", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        handlerPhi(fn, b, false);
    });
    run("handler phi of the value of the raising instruction", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        handlerPhi(fn, b, true);
    }, nullptr, "which is not defined at the end of that block");

    // the fold of a fused pipeline (qore#6549): the fixed shape is accepted, the old one refused
    run("fold flag of a fused pipeline", [](QoreIRFunction& fn, QoreIRBuilder& b) { foldFlag(fn, b, true); });
    run("fold flag of a fused pipeline (qore#6549)", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        foldFlag(fn, b, false);
    }, nullptr, "which is not defined at the end of that block");

    // a raising instruction that is the first of its block: its exception edge leaves from the entry of the block
    run("handler uses the value of the first instruction of its block, which raises", [](QoreIRFunction& fn,
            QoreIRBuilder& b) {
        QoreIRBasicBlock* h = fn.createBlock("handler");
        QoreIRBasicBlock* next = fn.createBlock("next");
        QoreIRInstruction* raising = b.createConstInt(2);
        raising->exception_target = h;
        b.createBranch(next);
        b.setBlock(next);
        b.createReturnNothing();
        b.setBlock(h);
        b.createReturn(raising->result);
    }, nullptr, NotDominated);
    // an iterator step whose done and continue targets are the same block: the element is not defined on the done
    // edge
    run("iterator element used in a block that is also the done target", iterator, [](QoreIRFunction& fn) {
        auto* next = find<QoreIRIteratorNextInstruction>(fn, QoreIROpcode::IteratorNext);
        next->result = QoreIRValue(++fn.max_value_id);
        next->done_target = next->continue_target;
        auto ret = std::make_unique<QoreIRReturnInstruction>();
        ret->opcode = QoreIROpcode::Return;
        ret->has_value = true;
        ret->value = next->result;
        fn.blocks[2]->instructions.back() = std::move(ret);
    }, NotDominated);
    // a phi with an incoming value from a block that is not a predecessor of its block
    run("phi incoming value from a block that is not a predecessor", phi, [](QoreIRFunction& fn) {
        auto* p = find<QoreIRPhiInstruction>(fn, QoreIROpcode::Phi);
        p->incoming.push_back({find(fn, QoreIROpcode::ConstBool)->result, fn.blocks[0].get()});
        p->operands.push_back(p->incoming.back().value);
    }, "which is not a predecessor of its block");

    // the handler of an exception: a value defined before the instruction that raises it is available, the value of
    // the instruction is not
    run("handler", [](QoreIRFunction& fn, QoreIRBuilder& b) { handler(fn, b, false); });
    run("handler uses the value of the raising instruction", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        handler(fn, b, true);
    }, nullptr, NotDominated);

    // the ownership of the values: no use of a value after its release on any path; a release that clears the slot
    // of the value may be repeated, a release of its object may not; a phi's copy of the handle of an object
    // releases the object with the handle
    run("temporary released in a loop that defines it", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        releaseInLoop(fn, b, false);
    });
    run("temporary released in a loop that uses it again", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        releaseInLoop(fn, b, true);
    }, nullptr, Released);
    run("use of a temporary after its release", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        releaseThenUse(fn, b, false);
    }, nullptr, Released);
    run("iterator released twice", iterator, [](QoreIRFunction& fn) {
        // the second release finds the slot cleared
        auto& insts = fn.blocks[3]->instructions;
        auto rel = std::make_unique<QoreIRInstruction>(QoreIROpcode::IteratorRelease);
        rel->operands.push_back(insts[0]->operands[0]);
        insts.insert(insts.begin(), std::move(rel));
    });
    run("iterator stepped after it is done", iterator, [](QoreIRFunction& fn) {
        // the done block steps the iterator again
        auto* next = find<QoreIRIteratorNextInstruction>(fn, QoreIROpcode::IteratorNext);
        auto again = std::make_unique<QoreIRIteratorNextInstruction>(next->iterator, next->done_target,
            next->continue_target);
        again->result = QoreIRValue(++fn.max_value_id);
        // the done block steps it again instead of releasing it
        auto& done_insts = fn.blocks[3]->instructions;
        done_insts.erase(done_insts.begin());
        done_insts.back() = std::move(again);
    }, Released);
    run("iterator stepped after its release", iterator, [](QoreIRFunction& fn) {
        auto& insts = fn.blocks[1]->instructions;
        auto rel = std::make_unique<QoreIRInstruction>(QoreIROpcode::IteratorRelease);
        rel->operands.push_back(find<QoreIRIteratorNextInstruction>(fn, QoreIROpcode::IteratorNext)->iterator);
        insts.insert(insts.begin(), std::move(rel));
    }, Released);
    run("suspended exceptions resumed", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        resumeTwice(fn, b, false);
    });
    run("suspended exceptions resumed twice", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        resumeTwice(fn, b, true);
    }, nullptr, Freed);
    run("use of a value after its decref", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        releaseThenUse(fn, b, true);
    }, nullptr, Freed);
    run("iterator copied by a phi, released once on each path", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        iteratorPhi(fn, b, false);
    });
    run("iterator released, then copied by a phi", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        iteratorPhi(fn, b, true);
    }, nullptr, Released);
    run("iterator released through a copy", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        iteratorCopy(fn, b, false);
    });
    run("iterator released through a copy, then stepped", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        iteratorCopy(fn, b, true);
    }, nullptr, Freed);

    // the stacks: the same entries are open on every path into a point, and each pop is of the innermost entry
    run("temp scope", [](QoreIRFunction& fn, QoreIRBuilder& b) { tempScope(fn, b, false); });
    run("temp scope open on one path", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        tempScope(fn, b, true);
    }, nullptr, Differ);
    run("temp scope of a loop body", [](QoreIRFunction& fn, QoreIRBuilder& b) { tempScopeLoop(fn, b, true); });
    run("temp scope closed in each iteration of a loop", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        tempScopeLoop(fn, b, false);
    }, nullptr, Differ);
    run("temp scope closed twice", [](QoreIRFunction& fn, QoreIRBuilder& b) { tempScope(fn, b, false); },
            [](QoreIRFunction& fn) {
        auto& insts = fn.blocks[1]->instructions;
        auto discard = std::make_unique<QoreIRInstruction>(QoreIROpcode::DiscardTemps);
        discard->temp_scope_id = find(fn, QoreIROpcode::PushTempMark)->temp_scope_id;
        insts.insert(insts.end() - 1, std::move(discard));
    }, NotInnermost);
    run("handler with a landing pad", [](QoreIRFunction& fn, QoreIRBuilder& b) { scopeHandler(fn, b, true); });
    run("handler without a landing pad", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        // the scopes are not known on the path from the exception edge: it meets the normal path
        scopeHandler(fn, b, false);
    });
    run("scope closed twice", [](QoreIRFunction& fn, QoreIRBuilder& b) { scopeHandler(fn, b, true); },
            [](QoreIRFunction& fn) {
        auto* exit = find<QoreIRScopeExitInstruction>(fn, QoreIROpcode::ScopeExit);
        auto again = std::make_unique<QoreIRScopeExitInstruction>(exit->scope_id);
        auto& insts = fn.blocks[0]->instructions;
        for (size_t i = 0; i < insts.size(); ++i) {
            if (insts[i].get() == exit) {
                insts.insert(insts.begin() + i + 1, std::move(again));
                break;
            }
        }
    }, NotInnermost);
    run("implicit argument restored twice", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        implicitArgs(fn, b, false);
    }, [](QoreIRFunction& fn) {
        // the inner restore is made again after the outer one
        auto* pop = find(fn, QoreIROpcode::PopImplicitArg);
        auto again = std::make_unique<QoreIRInstruction>(QoreIROpcode::PopImplicitArg);
        again->operands = pop->operands;
        auto& insts = fn.blocks[1]->instructions;
        insts.insert(insts.begin(), std::move(again));
    }, Freed);
    run("handler with a landing pad of more scopes than are open", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        scopeHandler(fn, b, true);
    }, [](QoreIRFunction& fn) {
        find<QoreIRLandingPadInstruction>(fn, QoreIROpcode::LandingPad)->scope_depth = 2;
    }, "fewer are open");
    run("scope exit of another scope", [](QoreIRFunction& fn, QoreIRBuilder& b) { scopeHandler(fn, b, true); },
            [](QoreIRFunction& fn) {
        find<QoreIRScopeExitInstruction>(fn, QoreIROpcode::ScopeExit)->scope_id = 2;
    }, NotInnermost);
    run("scope closed by a cleanup block whose handlers can raise", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        // the scope is closed before its handlers run: the exception edge of its exit leaves it closed
        QoreIRBasicBlock* h = fn.createBlock("cleanup");
        QoreIRBasicBlock* next = fn.createBlock("next");
        QoreIRBasicBlock* cont = fn.createBlock("cont");
        b.createScopeEnter(1);
        QoreIRInstruction* raising = b.createConstInt(2);
        raising->exception_target = h;
        b.createBranch(next);
        b.setBlock(next);
        b.createScopeExit(1);
        b.createBranch(cont);
        b.setBlock(h);
        b.createScopeExit(1, true)->exception_target = cont;
        b.createBranch(cont);
        b.setBlock(cont);
        b.createReturnNothing();
    });
    run("implicit arguments", [](QoreIRFunction& fn, QoreIRBuilder& b) { implicitArgs(fn, b, false); });
    run("implicit arguments restored outermost first", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        implicitArgs(fn, b, true);
    }, nullptr, NotInnermost);
    run("implicit argument restored from another value", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        implicitArgs(fn, b, false);
    }, [](QoreIRFunction& fn) {
        find(fn, QoreIROpcode::PopImplicitArg)->operands[0] = find(fn, QoreIROpcode::ConstInt)->result;
    }, NotInnermost);
    run("contexts", [](QoreIRFunction& fn, QoreIRBuilder& b) { contexts(fn, b, false); });
    run("context destroyed with an exception target", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        contexts(fn, b, false);
    }, [](QoreIRFunction& fn) {
        find(fn, QoreIROpcode::ContextDestroy)->exception_target = fn.blocks[1].get();
    }, "has an exception target");
    run("landing pad after another instruction", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        scopeHandler(fn, b, true);
    }, [](QoreIRFunction& fn) {
        auto* lp = find(fn, QoreIROpcode::LandingPad);
        for (auto& block : fn.blocks) {
            auto& insts = block->instructions;
            if (!insts.empty() && insts[0].get() == lp) {
                std::swap(insts[0], insts[1]);
            }
        }
    }, "a landing pad is not the first instruction of its block");

    run("contexts destroyed outermost first", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        contexts(fn, b, true);
    }, nullptr, NotInnermost);

    // the containers that the function changes are created by it, of their type, and are not read through the address
    // of their entries
    run("list created and changed", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        listChange(fn, b, QoreIROpcode::CreateEmptyList, false);
    });
    run("sized list created and changed", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        listChange(fn, b, QoreIROpcode::CreateSizedList, false);
    });
    run("list appended to that the function does not create", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        listChange(fn, b, QoreIROpcode::ConstString, false);
    }, nullptr, NotCreated);
    run("list stored to that the function does not create", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        listChange(fn, b, QoreIROpcode::CreateEmptyList, false);
    }, [](QoreIRFunction& fn) {
        find(fn, QoreIROpcode::ListSetInt)->operands[0] = find(fn, QoreIROpcode::ConstInt)->result;
    }, NotCreated);
    run("implicit arguments set from a value that is not a list", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        QoreIRValue v = b.createConstString("x")->result;
        QoreIRValue saved = b.createSetImplicitArgv(v)->result;
        b.createPopImplicitArg(saved);
        QoreIRBasicBlock* a = fn.createBlock("a");
        QoreIRBasicBlock* c = fn.createBlock("c");
        b.createBranch(a);
        b.setBlock(a);
        b.createBranch(c);
        b.setBlock(c);
        b.createReturnNothing();
    }, nullptr, NotCreated);
    run("hash key set in a list", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        QoreIRValue key = b.createConstString("k")->result;
        QoreIRValue list = b.createEmptyList()->result;
        QoreIRBasicBlock* a = fn.createBlock("a");
        QoreIRBasicBlock* c = fn.createBlock("c");
        b.createBranch(a);
        b.setBlock(a);
        b.createHashSetKeyValue(list, key, key);
        b.createBranch(c);
        b.setBlock(c);
        b.createReturn(list);
    }, nullptr, NotCreated);
    run("entries read of a list that the function changes", [](QoreIRFunction& fn, QoreIRBuilder& b) {
        listChange(fn, b, QoreIROpcode::CreateSizedList, true);
    }, nullptr, "reads the entries of a list that the function changes");
    ok = checkListStores() && ok;

    qore_destroy_program(pgm);
    qore_cleanup();
    if (!ok) {
        fprintf(stderr, "FAIL: %u AOT IR verification checks\n", count);
        return 1;
    }
    printf("PASS: %u AOT IR verification checks\n", count);
    return 0;
}
