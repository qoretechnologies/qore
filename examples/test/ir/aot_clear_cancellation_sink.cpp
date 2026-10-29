/* Copyright (c) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
#define _QORE_LIB_INTERN
#define XXH_INLINE_ALL
#include <qore/intern/xxhash/xxhash.h>
#include <qore/Qore.h>
#include <qore/intern/QoreAOT.h>
#include <qore/intern/QoreIRToLLVM.h>
#include <llvm/AsmParser/Parser.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/SourceMgr.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>

static std::string sinkCheck(const std::string& prefix, const std::string& sink,
        const std::string& success, const std::string& test, bool candidate) {
    bool initial = prefix == "entry";
    std::string p = "%" + prefix;
    std::string provenance = (initial && test == "missing-entry-provenance")
        || (!initial && test == "missing-loop-provenance") ? "" : ", !qore.xsink.priv !0";
    std::string tag = candidate && test != "missing-candidate" ? ", !qore.cancel.sink !0" : "";
    std::string offset = (initial && test == "wrong-offset")
        || (!initial && test == "wrong-loop-offset") ? "21" : "20";
    std::string predicate = initial && test == "reversed-predicate" ? "eq" : "ne";
    std::string volatility = initial && test == "volatile-head" ? "volatile " : "";
    std::string middle = ((initial && test == "stale-entry-check")
        || (!initial && test == "impure-loop-check")) ? "call void @unknown(ptr %sink)\n" : "";
    return p + ".priv = load ptr, ptr " + sink + provenance + "\n"
        + p + ".head = load " + volatility + "ptr, ptr " + p + ".priv\n"
        + middle + p + ".head.set = icmp " + predicate + " ptr " + p + ".head, null\n"
        + p + ".exit.ptr = getelementptr i8, ptr " + p + ".priv, i64 " + offset + "\n"
        + p + ".exit = load i8, ptr " + p + ".exit.ptr\n"
        + p + ".exit.set = icmp ne i8 " + p + ".exit, 0\n"
        + p + ".set = or i1 " + p + ".head.set, " + p + ".exit.set" + tag + "\n"
        + "br i1 " + p + ".set, label %error, label %" + success + "\n";
}

static std::string fixture(const std::string& test) {
    bool hop = test == "loop-hop" || test == "unguarded-loop";
    bool cycle = hop || test == "loop-phi" || test == "checked-call" || test == "cold-unknown-call"
        || test == "existing-success-edge";
    std::string extras;
    if (test == "unknown-call") extras = "call void @unknown(ptr %sink)\n";
    if (test == "readonly-call") extras = "call void @read_only(ptr %sink)\n";
    if (test == "unknown-store") extras = "store i64 7, ptr %other\n";
    if (test == "stack-store") extras = "store i64 7, ptr %local\n";
    if (test == "atomic-store") extras = "store atomic i8 0, ptr %flag seq_cst, align 1\n";
    if (test == "volatile-store") extras = "store volatile ptr null, ptr %other, !qore.runtime.location !0\n";
    if (test == "location-store") extras = "store ptr null, ptr %other, !qore.runtime.location !0\n";
    if (test == "volatile-load") extras = "%flag2 = load volatile i8, ptr %flag\n";
    if (test == "atomic-load") extras = "%flag2 = load atomic i8, ptr %flag seq_cst, align 1\n";
    if (test == "pure-intrinsic") extras = "%bound = call i64 @llvm.smax.i64(i64 %n, i64 1)\n";
    if (test == "checked-call" || test == "checked-ordinary-call") {
        extras = "call void @unknown(ptr %sink)\n"
            + sinkCheck("call", "%sink", "call.clear", test, false) + "call.clear:\n";
    }
    std::string source = "declare void @unknown(ptr)\n"
        "declare i64 @qore_rt_check_cancel_v2(ptr, ptr, ptr)\n"
        "declare void @read_only(ptr) memory(read)\n"
        "declare i64 @llvm.smax.i64(i64, i64)\n"
        "define i64 @probe(ptr %sink, ptr %other, ptr %flag, i64 %n) {\nentry:\n"
        "%local = alloca i64\n";
    if (test == "call-before-entry") source += "call void @unknown(ptr %sink)\n";
    if (test == "unguarded-entry" || test == "unguarded-loop") {
        source += "br label %header\n";
    } else {
        source += sinkCheck("entry", "%sink", "header", test, test != "ordinary-entry");
    }
    source += "header:\n%i = phi i64 [ 0, %entry ]"
        + std::string(cycle ? (hop ? ", [ %next, %resume ]" : ", [ %next, %check ]") : "")
        + std::string(test == "existing-success-edge" ? ", [ %next, %fast ]\n" : "\n")
        + "%done = icmp eq i64 %i, %n\nbr i1 %done, label %exit, label %body\n"
        "body:\n%next = add i64 %i, 1\n"
        "%poll = load atomic i8, ptr %flag seq_cst, align 1\n"
        "%pending = icmp ne i8 %poll, 0\nbr i1 %pending, label %slow, label %fast\n"
        "fast:\n" + extras;
    if (test == "existing-success-edge") {
        source += "%again = icmp eq i64 %n, 7\nbr i1 %again, label %check, label %header\n";
    } else if (test == "duplicate-edge") {
        source += "%again = icmp eq i64 %n, 7\nbr i1 %again, label %check, label %check\n";
    } else {
        source += "br label %check\n";
    }
    source += "slow:\n";
    if (test == "cold-unknown-call") source += "call void @unknown(ptr %sink)\n";
    source += "%slow.result = call i64 @qore_rt_check_cancel_v2(ptr %sink, ptr null, ptr null)\n"
        "br label %check\ncheck:\n";
    if (test == "check-phi") source += "%unused = phi i64 [ 1, %fast ], [ 2, %slow ]\n";
    if (test == "nonargument-sink") source += "%loaded = load ptr, ptr %other\n";
    std::string sink = test == "different-sink" ? "%other" : test == "nonargument-sink" ? "%loaded" : "%sink";
    source += sinkCheck("loop", sink, cycle ? (hop ? "resume" : "header") : "exit", test, true);
    if (hop) source += "resume:\nbr label %header\n";
    if (test == "escaping-check-value") {
        source += "error:\n%escaped = phi ptr [ %entry.priv, %entry ], [ %loop.priv, %check ]\n"
            "%result = ptrtoint ptr %escaped to i64\nret i64 %result\n";
    } else {
        source += "error:\nret i64 -1\n";
    }
    source += "exit:\nret i64 %i\n}\n!0 = !{}\n";
    return source;
}

static std::unique_ptr<llvm::Module> parse(llvm::LLVMContext& context, const std::string& source) {
    llvm::SMDiagnostic diagnostic;
    auto module = llvm::parseAssemblyString(source, diagnostic, context);
    if (!module) diagnostic.print("clear-cancellation-sink", llvm::errs());
    assert(module && !llvm::verifyModule(*module, &llvm::errs()));
    return module;
}

static std::string render(const llvm::Module& module) {
    std::string text;
    llvm::raw_string_ostream stream(text);
    module.print(stream, nullptr);
    return text;
}

static void check() {
    ExceptionSink xsink;
    QoreProgramHelper program(PO_MODERN, xsink);
    QoreExternalProgramContextHelper program_context(&xsink, *program);
    assert(!xsink);
    unsigned tests = 0;
    for (const char* name : {"eligible", "location-store", "atomic-load", "pure-intrinsic", "call-before-entry",
            "unguarded-entry", "missing-entry-provenance", "missing-loop-provenance", "missing-candidate",
            "wrong-offset", "reversed-predicate", "volatile-head", "stale-entry-check", "impure-loop-check",
            "unknown-call", "readonly-call", "unknown-store", "stack-store", "atomic-store", "volatile-store",
            "check-phi", "different-sink", "nonargument-sink", "duplicate-edge", "volatile-load",
            "wrong-loop-offset", "existing-success-edge", "escaping-check-value", "checked-call",
            "cold-unknown-call", "loop-hop", "loop-phi", "unguarded-loop", "ordinary-entry",
            "checked-ordinary-call"}) {
        llvm::LLVMContext context;
        auto module = parse(context, fixture(name));
        std::string before = render(*module), error;
        bool ok = QoreIRToLLVM::bypassClearCancellationChecks(*module, error);
        if (!ok) std::fprintf(stderr, "%s: %s\n", name, error.c_str());
        assert(ok && error.empty());
        bool expected = tests < 5 || std::string(name) == "loop-hop";
        if ((render(*module) != before) != expected) std::fprintf(stderr, "%s: unexpected CFG change\n", name);
        assert((render(*module) != before) == expected);
        if (expected) {
            auto* function = module->getFunction("probe");
            llvm::DominatorTree dom(*function);
            llvm::Instruction* slow = nullptr;
            llvm::Instruction* loop = nullptr;
            unsigned sinks = 0, exit_bytes = 0, polls = 0, checked = 0;
            for (auto& block : *function) {
                for (auto& inst : block) {
                    if (++checked % 100 == 0) assert(!qore_check_cancel(nullptr, "clear sink test inspection"));
                    if (inst.getName() == "loop.priv") loop = &inst;
                    if (inst.getMetadata("qore.xsink.priv")) ++sinks;
                    if (inst.getName().ends_with(".exit")) ++exit_bytes;
                    if (auto* load = llvm::dyn_cast<llvm::LoadInst>(&inst); load && load->isAtomic()) ++polls;
                    if (auto* call = llvm::dyn_cast<llvm::CallInst>(&inst); call
                            && call->getCalledFunction()->getName() == "qore_rt_check_cancel_v2") slow = call;
                }
            }
            // Pending entry and thread-exit reads remain. The loop's full
            // check can now run only after the slow cancellation helper.
            assert(sinks == 2 && exit_bytes == 2 && polls >= 1 && loop && slow);
            assert(dom.dominates(slow, loop));
        }
        ++tests;
    }
    {
        llvm::LLVMContext context;
        auto module = parse(context, fixture("eligible"));
        std::string before = render(*module), error;
        assert(!setenv("QORE_DISABLE_LLVM_CLEAR_CANCEL_SINK", "1", 1));
        assert(QoreIRToLLVM::bypassClearCancellationChecks(*module, error));
        assert(render(*module) == before);
        assert(!unsetenv("QORE_DISABLE_LLVM_CLEAR_CANCEL_SINK"));
        assert(QoreIRToLLVM::bypassClearCancellationChecks(*module, error));
        assert(render(*module) != before);
        ++tests;
    }
    {
        llvm::LLVMContext context;
        std::string source;
        for (unsigned i = 0; i < 110; ++i) {
            if (i % 100 == 0) assert(!qore_check_cancel(nullptr, "clear sink test setup"));
            source += "declare void @padding" + std::to_string(i) + "()\n";
        }
        auto module = parse(context, source + fixture("eligible"));
        std::string before = render(*module), error;
        assert(!qore_cancel_thread(q_gettid(), "clear sink compiler cancellation"));
        bool ok = QoreIRToLLVM::bypassClearCancellationChecks(*module, error);
        qore_clear_thread_cancel();
        assert(!ok && error == "cancelled during LLVM cancellation sink analysis");
        assert(render(*module) == before);
        error.clear();
        assert(QoreIRToLLVM::bypassClearCancellationChecks(*module, error));
        assert(render(*module) != before);
        ++tests;
    }
    assert(!xsink);
    std::printf("clear-cancellation-sink: %u cases passed\n", tests);
}

int main() {
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    check();
    qore_cleanup();
}
