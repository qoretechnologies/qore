/* -*- mode: c++; indent-tabs-mode: nil -*- */
// Copyright (C) 2026 Qore Technologies, s.r.o.
// SPDX-License-Identifier: LGPL-2.1-or-later

// qore#6451: the value ids of IR read from an AOT module are checked before the IR is used.  Each case builds a valid
// IR function with an instruction that holds value ids in fields of its own, writes it with the AOT writer and reads
// it with the reader, which must accept it; then it writes a copy with one id set beyond the highest value id of the
// function, or with the handle of an object set to a value that the instruction creating the object does not define,
// which the reader must refuse.  The runtime checks are run on IR that the reader accepts: a function whose parent
// slots its frame does not hold, a typed foreach over a value that is not a list, and the runtime functions of the
// handles of objects with no object.

#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <qore/QoreStringNode.h>
#include <qore/intern/LocalVar.h>
#include <qore/intern/QoreIR.h>
#include <qore/intern/QoreIRBuilder.h>
#include <qore/intern/QoreIRInterpreter.h>
#include <qore/intern/QoreAOTBinary.h>
#include <qore/intern/QoreJITIncludes.h>

#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

extern "C" uint64_t qore_rt_ref_foreach_get_entry(uint64_t state_ptr, int64_t index, ExceptionSink* xsink);
extern "C" void qore_rt_ref_foreach_record(uint64_t state_ptr, uint64_t value_bits, ExceptionSink* xsink);
extern "C" int64_t qore_rt_selector_iterator_feed(void* iter_ptr, int64_t flags, uint64_t first_bits,
    uint64_t second_bits, ExceptionSink* xsink);

using Build = std::function<void(QoreIRFunction&, QoreIRBuilder&)>;
using Mutate = std::function<void(QoreIRFunction&)>;

//! the message of an id beyond the highest value id
static const char* const Beyond = "beyond the highest value id";
//! the message of a handle that is not defined by an instruction that creates its object
static const char* const NotCreated = "not defined by an instruction that creates its object";

//! returns the first instruction with the given opcode
template <typename T = QoreIRInstruction>
static T* find(QoreIRFunction& fn, QoreIROpcode opcode, unsigned skip = 0) {
    for (auto& block : fn.blocks) {
        for (auto& inst : block->instructions) {
            if (inst->opcode == opcode && !skip--) {
                return static_cast<T*>(inst.get());
            }
        }
    }
    fprintf(stderr, "fixture has no opcode %u\n", static_cast<unsigned>(opcode));
    abort();
}

//! returns an id beyond the highest id of a result or a base operand of the function
static uint32_t beyond(const QoreIRFunction& fn) {
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
    return std::max(max_id, 127u) + 10;
}

//! writes the function with the AOT writer and reads it with the reader
static std::unique_ptr<QoreIRFunction> roundTrip(QoreProgram* pgm, const QoreIRFunction& fn, std::string& error) {
    QoreAOTBinaryWriter writer;
    // the format of lvalue paths that qcc always declares
    writer.feature_flags |= QORE_AOT_FEAT_LVPATH_SLICE | QORE_AOT_FEAT_LVPATH_PATTERN;
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

static unsigned count = 0;

//! builds the function, applies the change and checks that the reader accepts it, or refuses it with the message
static bool check(QoreProgram* pgm, const char* label, const Build& build, const Mutate& mutate = nullptr,
        const char* expected = nullptr) {
    ++count;
    QoreIRFunction fn(label, label);
    QoreIRBuilder builder(&fn);
    builder.setBlock(fn.createBlock("entry"));
    build(fn, builder);
    // the highest value id of the valid function, which qcc writes (computeSlotIdsAndEmbed())
    fn.max_value_id = beyond(fn) - 10;
    if (mutate) {
        mutate(fn);
    }
    std::string error;
    std::unique_ptr<QoreIRFunction> ir = roundTrip(pgm, fn, error);
    bool ok = expected ? !ir && error.find(expected) != std::string::npos : ir && error.empty();
    if (!ok) {
        fprintf(stderr, "%s: accepted=%d, error='%s', expected='%s'\n", label, bool(ir), error.c_str(),
            expected ? expected : "success");
    }
    return ok;
}

// the fixtures: each a valid function with an instruction that holds value ids in fields of its own

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

static void typedForeach(QoreIRFunction& fn, QoreIRBuilder& b, bool list_value) {
    QoreIRValue list = list_value ? b.createConstNothing()->result : b.createConstInt(1)->result;
    QoreIRValue index = b.createConstInt(0)->result;
    QoreIRValue limit = b.createConstInt(3)->result;
    QoreIRBasicBlock* loop = fn.createBlock("loop");
    QoreIRBasicBlock* body = fn.createBlock("body");
    QoreIRBasicBlock* done = fn.createBlock("done");
    b.createBranch(loop);
    b.setBlock(loop);
    b.createTypedForeachNext(list, index, limit, bigIntTypeInfo, done, body);
    b.setBlock(body);
    b.createBranch(done);
    b.setBlock(done);
    b.createReturnNothing();
}

static void selectors(QoreIRFunction& fn, QoreIRBuilder& b) {
    QoreIRValue seq = b.createConstNothing()->result;
    QoreIRValue it = b.createIteratorCreateSelectors(seq, 0, 1)->result;
    // an iterator that is not one of a list of selectors
    b.createIteratorCreate(seq);
    QoreIRValue idx = b.createConstInt(0)->result;
    b.createSelectorIteratorPending(it);
    b.createSelectorIteratorFeed(it, 0, {idx});
    QoreIRBasicBlock* body = fn.createBlock("body");
    QoreIRBasicBlock* done = fn.createBlock("done");
    b.createIteratorNextSelectors(it, done, body);
    b.setBlock(body);
    b.createBranch(done);
    b.setBlock(done);
    b.createIteratorRelease(it);
    b.createReturnNothing();
}

static void context(QoreIRFunction& fn, QoreIRBuilder& b) {
    QoreIRValue ctx = b.createContext("c", QoreValue(), QoreValue(), QoreValue(), 0)->result;
    QoreIRValue idx = b.createConstInt(0)->result;
    b.createContextMaxPos(ctx);
    b.createContextSetPos(ctx, idx);
    b.createContextDestroy(ctx);
    b.createReturnNothing();
}

static void suspend(QoreIRFunction& fn, QoreIRBuilder& b) {
    QoreIRValue handle = b.createExceptionSuspend()->result;
    b.createConstInt(0);
    b.createExceptionResume(handle);
    b.createReturnNothing();
}

static void branches(QoreIRFunction& fn, QoreIRBuilder& b) {
    QoreIRValue cond = b.createConstBool(true)->result;
    QoreIRValue i = b.createConstInt(1)->result;
    QoreIRValue s = b.createConstString("a")->result;
    QoreIRBasicBlock* a = fn.createBlock("a");
    QoreIRBasicBlock* c = fn.createBlock("c");
    QoreIRBasicBlock* d = fn.createBlock("d");
    QoreIRBasicBlock* e = fn.createBlock("e");
    b.createBranchIf(cond, a, c);
    b.setBlock(a);
    b.createSwitchInt(i, c, {});
    b.setBlock(c);
    b.createSwitchString(s, d, {});
    b.setBlock(d);
    b.createBranch(e);
    b.setBlock(e);
    b.createReturn(i);
}

static void phi(QoreIRFunction& fn, QoreIRBuilder& b) {
    QoreIRValue cond = b.createConstBool(true)->result;
    QoreIRValue list = b.createConstNothing()->result;
    QoreIRBasicBlock* a = fn.createBlock("a");
    QoreIRBasicBlock* c = fn.createBlock("c");
    QoreIRBasicBlock* join = fn.createBlock("join");
    QoreIRBasicBlock* body = fn.createBlock("body");
    QoreIRBasicBlock* done = fn.createBlock("done");
    b.createBranchIf(cond, a, c);
    b.setBlock(a);
    QoreIRValue it1 = b.createIteratorCreate(list)->result;
    QoreIRValue v1 = b.createConstInt(1)->result;
    b.createBranch(join);
    b.setBlock(c);
    QoreIRValue it2 = b.createIteratorCreate(list)->result;
    QoreIRValue v2 = b.createConstInt(2)->result;
    b.createBranch(join);
    b.setBlock(join);
    b.createPhi({{v1, a}, {v2, c}});
    // an iterator merged by a phi
    QoreIRValue it = b.createPhi({{it1, a}, {it2, c}})->result;
    b.createIteratorNext(it, done, body);
    b.setBlock(body);
    b.createBranch(done);
    b.setBlock(done);
    b.createReturnNothing();
}

static void lvaluePath(QoreIRFunction& fn, QoreIRBuilder& b) {
    QoreIRValue key = b.createConstString("k")->result;
    QoreIRValue val = b.createConstInt(1)->result;
    auto* path = fn.blocks.back()->appendInstruction<QoreIRLValuePathInstruction>(QoreIROpcode::LValuePathAssign);
    path->operands.push_back(val);
    path->operands.push_back(key);
    LVPathStep root;
    root.kind = LVPathStepKind::LocalVar;
    root.slot_id = 0;
    path->path.push_back(root);
    LVPathStep step;
    step.kind = LVPathStepKind::HashKey;
    step.operand_idx = key.id;
    path->path.push_back(step);
    LVPathStep slice;
    slice.kind = LVPathStepKind::HashKeySlice;
    slice.slice_operand_ids = {key.id};
    path->path.push_back(slice);
    b.createReturnNothing();
}

//! a chain of phis, each with the previous one twice as its incoming values, that merges an iterator
static void phiChain(QoreIRFunction& fn, QoreIRBuilder& b) {
    QoreIRValue list = b.createConstNothing()->result;
    QoreIRBasicBlock* a = fn.createBlock("a");
    QoreIRBasicBlock* c = fn.createBlock("c");
    QoreIRBasicBlock* join = fn.createBlock("join");
    QoreIRBasicBlock* body = fn.createBlock("body");
    QoreIRBasicBlock* done = fn.createBlock("done");
    QoreIRValue cond = b.createConstBool(true)->result;
    b.createBranchIf(cond, a, c);
    b.setBlock(a);
    QoreIRValue it = b.createIteratorCreate(list)->result;
    b.createBranch(join);
    b.setBlock(c);
    b.createBranch(join);
    b.setBlock(join);
    for (unsigned i = 0; i < 200; ++i) {
        it = b.createPhi({{it, a}, {it, c}})->result;
    }
    b.createIteratorNext(it, done, body);
    b.setBlock(body);
    b.createBranch(done);
    b.setBlock(done);
    b.createReturnNothing();
}

// the runtime checks of IR that the reader accepts

//! runs the function read back from the AOT writer with the IR interpreter; returns the exception, if any
static std::string runIR(QoreProgram* pgm, const QoreIRFunction& fn, bool& ok) {
    std::string error;
    std::unique_ptr<QoreIRFunction> ir = roundTrip(pgm, fn, error);
    if (!ir) {
        ok = false;
        return "not read: " + error;
    }
    ir->computeSlotIdsAndEmbed();
    ExceptionSink xsink;
    QoreValue rv;
    QoreIRInterpreter::execute(*ir, rv, &xsink, nullptr);
    rv.discard(&xsink);
    std::string err;
    if (xsink) {
        QoreValue e = xsink.getExceptionErr();
        err = e.getType() == NT_STRING ? e.get<const QoreStringNode>()->c_str() : "<exception>";
        xsink.clear();
    }
    ok = true;
    return err;
}

static bool runtimeChecks(QoreProgram* pgm) {
    bool ok = true;
    // parent slots that the frame does not hold
    {
        ++count;
        QoreIRFunction fn("parent-slots", "parent-slots");
        QoreIRBuilder b(&fn);
        b.setBlock(fn.createBlock("entry"));
        b.createReturnNothing();
        fn.parent_slot_count = 1000;
        bool run_ok;
        std::string err = runIR(pgm, fn, run_ok);
        if (!run_ok || err != "IR-ERROR") {
            fprintf(stderr, "parent slots beyond the frame: '%s', expected IR-ERROR\n", err.c_str());
            ok = false;
        }
    }
    // a typed foreach over a value that is not a list ends the loop
    {
        ++count;
        QoreIRFunction fn("typed-foreach-int", "typed-foreach-int");
        QoreIRBuilder b(&fn);
        b.setBlock(fn.createBlock("entry"));
        typedForeach(fn, b, false);
        bool run_ok;
        std::string err = runIR(pgm, fn, run_ok);
        if (!run_ok || !err.empty()) {
            fprintf(stderr, "typed foreach over an int: '%s', expected no exception\n", err.c_str());
            ok = false;
        }
    }
    // the runtime functions of the handles of objects accept no object
    {
        ++count;
        ExceptionSink xsink;
        uint64_t entry = qore_rt_ref_foreach_get_entry(0, 0, &xsink);
        qore_rt_ref_foreach_record(0, toBits(QoreValue(1)), &xsink);
        int64_t fed = qore_rt_selector_iterator_feed(nullptr, 0, toBits(QoreValue(0)), toBits(QoreValue()), &xsink);
        if (entry != toBits(QoreValue()) || fed != 0 || xsink) {
            fprintf(stderr, "null handles: entry=%llu fed=%lld exception=%d\n",
                static_cast<unsigned long long>(entry), static_cast<long long>(fed), bool(xsink));
            xsink.clear();
            ok = false;
        }
    }
    return ok;
}

int main() {
    // the signal thread must stay disabled for valgrind, just as for qore -b
    qore_init(QL_MIT, "UTF-8", false, QLO_DISABLE_SIGNAL_HANDLING);
    QoreProgram* pgm = qore_create_program(PO_NEW_STYLE);
    bool ok = true;
    auto run = [&](const char* label, const Build& build, const Mutate& mutate = nullptr,
            const char* expected = nullptr) {
        ok = check(pgm, label, build, mutate, expected) && ok;
    };

    // iterators
    run("iterator", iterator);
    run("iterator: IteratorNext iterator beyond", iterator, [](QoreIRFunction& fn) {
        find<QoreIRIteratorNextInstruction>(fn, QoreIROpcode::IteratorNext)->iterator = QoreIRValue(beyond(fn));
    }, Beyond);
    run("iterator: IteratorNext iterator not created", iterator, [](QoreIRFunction& fn) {
        find<QoreIRIteratorNextInstruction>(fn, QoreIROpcode::IteratorNext)->iterator
            = find(fn, QoreIROpcode::ConstNothing)->result;
    }, NotCreated);
    run("iterator: IteratorRelease handle not created", iterator, [](QoreIRFunction& fn) {
        find(fn, QoreIROpcode::IteratorRelease)->operands[0] = find(fn, QoreIROpcode::ConstNothing)->result;
    }, NotCreated);
    run("iterator: release value beyond", iterator, [](QoreIRFunction& fn) {
        find<QoreIRIteratorNextInstruction>(fn, QoreIROpcode::IteratorNext)->release_values.push_back(
            QoreIRValue(beyond(fn)));
    }, Beyond);
    run("iterator: result defined twice", iterator, [](QoreIRFunction& fn) {
        find(fn, QoreIROpcode::IteratorCreate)->result = find(fn, QoreIROpcode::ConstNothing)->result;
    }, "defined more than once");

    // typed foreach
    auto typed = [](QoreIRFunction& fn, QoreIRBuilder& b) { typedForeach(fn, b, true); };
    run("typed foreach", typed);
    run("typed foreach: list beyond", typed, [](QoreIRFunction& fn) {
        find<QoreIRIteratorNextInstruction>(fn, QoreIROpcode::TypedForeachNextInt)->iterator
            = QoreIRValue(beyond(fn));
    }, Beyond);

    // lists of selectors
    run("selectors", selectors);
    for (QoreIROpcode op : {QoreIROpcode::SelectorIteratorPending, QoreIROpcode::SelectorIteratorFeed,
            QoreIROpcode::IteratorRelease}) {
        run("selectors: handle is the sequence", selectors, [op](QoreIRFunction& fn) {
            find(fn, op)->operands[0] = find(fn, QoreIROpcode::ConstNothing)->result;
        }, NotCreated);
    }
    run("selectors: pending handle is another iterator", selectors, [](QoreIRFunction& fn) {
        find(fn, QoreIROpcode::SelectorIteratorPending)->operands[0] = find(fn, QoreIROpcode::IteratorCreate)->result;
    }, NotCreated);
    run("selectors: feed handle is another iterator", selectors, [](QoreIRFunction& fn) {
        find(fn, QoreIROpcode::SelectorIteratorFeed)->operands[0] = find(fn, QoreIROpcode::IteratorCreate)->result;
    }, NotCreated);
    run("selectors: IteratorNextSelectors iterator not created", selectors, [](QoreIRFunction& fn) {
        find<QoreIRIteratorNextInstruction>(fn, QoreIROpcode::IteratorNextSelectors)->iterator
            = find(fn, QoreIROpcode::ConstInt)->result;
    }, NotCreated);
    run("selectors: IteratorNextSelectors iterator beyond", selectors, [](QoreIRFunction& fn) {
        find<QoreIRIteratorNextInstruction>(fn, QoreIROpcode::IteratorNextSelectors)->iterator
            = QoreIRValue(beyond(fn));
    }, Beyond);

    // contexts
    run("context", context);
    for (QoreIROpcode op : {QoreIROpcode::ContextMaxPos, QoreIROpcode::ContextSetPos,
            QoreIROpcode::ContextDestroy}) {
        run("context: handle not created", context, [op](QoreIRFunction& fn) {
            find(fn, op)->operands[0] = find(fn, QoreIROpcode::ConstInt)->result;
        }, NotCreated);
    }

    // suspended exceptions
    run("suspend", suspend);
    run("suspend: handle not created", suspend, [](QoreIRFunction& fn) {
        find(fn, QoreIROpcode::ExceptionResume)->operands[0] = find(fn, QoreIROpcode::ConstInt)->result;
    }, NotCreated);

    // conditions, switch values and return values
    run("branches", branches);
    run("branches: condition beyond", branches, [](QoreIRFunction& fn) {
        find<QoreIRBranchIfInstruction>(fn, QoreIROpcode::BrIf)->condition = QoreIRValue(beyond(fn));
    }, Beyond);
    run("branches: int switch value beyond", branches, [](QoreIRFunction& fn) {
        find<QoreIRSwitchIntInstruction>(fn, QoreIROpcode::SwitchInt)->switch_val = QoreIRValue(beyond(fn));
    }, Beyond);
    run("branches: string switch value beyond", branches, [](QoreIRFunction& fn) {
        find<QoreIRSwitchStringInstruction>(fn, QoreIROpcode::SwitchString)->switch_val = QoreIRValue(beyond(fn));
    }, Beyond);
    run("branches: return value beyond", branches, [](QoreIRFunction& fn) {
        find<QoreIRReturnInstruction>(fn, QoreIROpcode::Return)->value = QoreIRValue(beyond(fn));
    }, Beyond);

    // phis
    run("phi", phi);
    run("phi: incoming value beyond", phi, [](QoreIRFunction& fn) {
        find<QoreIRPhiInstruction>(fn, QoreIROpcode::Phi)->incoming[0].value = QoreIRValue(beyond(fn));
    }, Beyond);
    run("phi: iterator merged with a value", phi, [](QoreIRFunction& fn) {
        find<QoreIRPhiInstruction>(fn, QoreIROpcode::Phi, 1)->incoming[1].value
            = find(fn, QoreIROpcode::ConstNothing)->result;
    }, NotCreated);

    // UINT32_MAX is no value only for the key of an lvalue path step; elsewhere it is beyond the frame
    run("UINT32_MAX: return value", branches, [](QoreIRFunction& fn) {
        find<QoreIRReturnInstruction>(fn, QoreIROpcode::Return)->value = QoreIRValue(UINT32_MAX);
    }, Beyond);
    run("UINT32_MAX: condition", branches, [](QoreIRFunction& fn) {
        find<QoreIRBranchIfInstruction>(fn, QoreIROpcode::BrIf)->condition = QoreIRValue(UINT32_MAX);
    }, Beyond);
    run("UINT32_MAX: int switch value", branches, [](QoreIRFunction& fn) {
        find<QoreIRSwitchIntInstruction>(fn, QoreIROpcode::SwitchInt)->switch_val = QoreIRValue(UINT32_MAX);
    }, Beyond);
    run("UINT32_MAX: phi incoming value", phi, [](QoreIRFunction& fn) {
        find<QoreIRPhiInstruction>(fn, QoreIROpcode::Phi)->incoming[0].value = QoreIRValue(UINT32_MAX);
    }, Beyond);
    run("UINT32_MAX: iterator", iterator, [](QoreIRFunction& fn) {
        find<QoreIRIteratorNextInstruction>(fn, QoreIROpcode::IteratorNext)->iterator = QoreIRValue(UINT32_MAX);
    }, Beyond);
    run("UINT32_MAX: slice selector", lvaluePath, [](QoreIRFunction& fn) {
        find<QoreIRLValuePathInstruction>(fn, QoreIROpcode::LValuePathAssign)->path[2].slice_operand_ids[0]
            = UINT32_MAX;
    }, Beyond);

    // the values that an instruction always reads must be values
    run("no value: condition", branches, [](QoreIRFunction& fn) {
        find<QoreIRBranchIfInstruction>(fn, QoreIROpcode::BrIf)->condition = QoreIRValue();
    }, "which is no value");
    run("no value: string switch value", branches, [](QoreIRFunction& fn) {
        find<QoreIRSwitchStringInstruction>(fn, QoreIROpcode::SwitchString)->switch_val = QoreIRValue();
    }, "which is no value");
    run("no value: return value", branches, [](QoreIRFunction& fn) {
        find<QoreIRReturnInstruction>(fn, QoreIROpcode::Return)->value = QoreIRValue();
    }, "which is no value");

    // a long chain of phis that merge an iterator is checked once per phi
    run("phi chain", phiChain);
    run("phi chain: a value merged at its start", phiChain, [](QoreIRFunction& fn) {
        find<QoreIRPhiInstruction>(fn, QoreIROpcode::Phi)->incoming[1].value
            = find(fn, QoreIROpcode::ConstNothing)->result;
    }, NotCreated);

    // lvalue paths
    run("lvalue path", lvaluePath);
    run("lvalue path: key beyond", lvaluePath, [](QoreIRFunction& fn) {
        find<QoreIRLValuePathInstruction>(fn, QoreIROpcode::LValuePathAssign)->path[1].operand_idx = beyond(fn);
    }, Beyond);
    run("lvalue path: slice selector beyond", lvaluePath, [](QoreIRFunction& fn) {
        find<QoreIRLValuePathInstruction>(fn, QoreIROpcode::LValuePathAssign)->path[2].slice_operand_ids[0]
            = beyond(fn);
    }, Beyond);

    ok = runtimeChecks(pgm) && ok;

    qore_destroy_program(pgm);
    qore_cleanup();
    if (ok) {
        printf("PASS: %u AOT IR value id checks\n", count);
    }
    return ok ? 0 : 1;
}
