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
#include <llvm/Config/llvm-config.h>
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

static std::string fixture(const std::string& test) {
    std::string layout = "{ i64, ptr, ptr, ptr, ptr, ptr, ptr, ptr }";
    std::string init = "store i64 0, ptr %cache\n";
    std::string extras;
    std::string argument = "%ref";
    std::string result = "%result";
    std::string name = "qore_rt_try_funcref_int_int_v5";
    std::string call_suffix;
    std::string after;
    if (test == "no-init") init.clear();
    if (test == "late-init") { after = init; init.clear(); }
    if (test == "nonzero-store") extras = "store i64 1, ptr %cache\n";
    if (test == "volatile-store") extras = "store volatile i64 0, ptr %cache\n";
    if (test == "atomic-store") extras = "store atomic i64 0, ptr %cache seq_cst, align 8\n";
    if (test == "escape") extras = "call void @escape(ptr %cache)\n";
    if (test == "gep") extras = "%alias = getelementptr i8, ptr %cache, i64 0\ncall void @escape(ptr %alias)\n";
    if (test == "load") { extras = "%changing = load i64, ptr %source\n"; argument = "%changing"; }
    if (test == "phi") argument = "%changing";
    if (test == "constant") argument = "0";
    if (test == "alias-result") result = "%cache";
    if (test == "bundle") call_suffix = " [\"proof-test\"(ptr %cache)]";
    if (test == "unknown-helper") name = "qore_rt_try_funcref_int_int_v4";
    if (test == "wrong-layout") layout = "{ i64, ptr, ptr, ptr, ptr, ptr, ptr }";
    if (test == "wrong-pointer-layout") layout = "{ i64, ptr addrspace(1), ptr, ptr, ptr, ptr, ptr, ptr }";
    if (test == "owner-load") extras = "%owner = load i64, ptr %cache\n";
    if (test == "volatile-load") extras = "%owner = load volatile i64, ptr %cache\n";
    if (test == "clear") after = "store i64 0, ptr %cache\n";
    std::string params = "i64, i64, ptr, ptr, ptr, ptr, ptr";
    std::string actual = "i64 " + argument + ", i64 7, ptr " + result
        + ", ptr %xsink, ptr %rc, ptr %cache, ptr %frame";
    if (test == "wrong-signature") {
        params = "i64, double, ptr, ptr, ptr, ptr, ptr";
        actual = "i64 " + argument + ", double 7.0, ptr %result, ptr %xsink, ptr %rc, ptr %cache, ptr %frame";
    }
    if (test == "float-unary") {
        name = "qore_rt_try_funcref_float_float_v4";
        params = "i64, double, ptr, ptr, ptr, ptr, ptr";
        actual = "i64 %ref, double 7.0, ptr %result, ptr %xsink, ptr %rc, ptr %cache, ptr %frame";
    }
    if (test == "int-zero" || test == "float-zero") {
        name = test == "int-zero" ? "qore_rt_try_funcref_int_0_v4" : "qore_rt_try_funcref_float_0_v4";
        params = "i64, ptr, ptr, ptr, ptr, ptr";
        actual = "i64 %ref, ptr %result, ptr %xsink, ptr %rc, ptr %cache, ptr %frame";
    }
    std::string declarations = "declare i32 @" + name + "(" + params + ")\n"
        "declare void @escape(ptr)\n";
    std::string lifetime;
    if (test == "lifetime") {
#if LLVM_VERSION_MAJOR >= 22
        declarations += "declare void @llvm.lifetime.start.p0(ptr)\ndeclare void @llvm.lifetime.end.p0(ptr)\n";
        lifetime = "call void @llvm.lifetime.start.p0(ptr %cache)\n";
        after += "call void @llvm.lifetime.end.p0(ptr %cache)\n";
#else
        declarations += "declare void @llvm.lifetime.start.p0(i64, ptr)\ndeclare void @llvm.lifetime.end.p0(i64, ptr)\n";
        lifetime = "call void @llvm.lifetime.start.p0(i64 -1, ptr %cache)\n";
        after += "call void @llvm.lifetime.end.p0(i64 -1, ptr %cache)\n";
#endif
    }
    std::string source = declarations + "define void @probe(i64 %ref, i64 %other, ptr %source, "
        "ptr %result, ptr %xsink, ptr %rc, ptr %frame) {\nentry:\n";
    if (test == "nonentry-alloca") source += "br label %body\nbody:\n";
    source += "%cache = alloca " + layout + (test == "array" ? ", i64 2\n" : "\n") + lifetime + init;
    if (test == "phi") source += "br label %body\nbody:\n%changing = phi i64 [ %ref, %entry ]\n";
    source += extras + "%first = call i32 @" + name + "(" + actual + ")" + call_suffix + "\n" + after;
    if (test == "clone" || test == "different-argument" || test == "clear") {
        std::string second = test == "different-argument" ? "%other" : "%ref";
        source += "%second = call i32 @" + name + "(i64 " + second
            + ", i64 9, ptr %result, ptr %xsink, ptr %rc, ptr %cache, ptr %frame)\n";
    }
    return source + "ret void\n}\n";
}

static std::unique_ptr<llvm::Module> parse(llvm::LLVMContext& context, const std::string& source) {
    llvm::SMDiagnostic diagnostic;
    auto module = llvm::parseAssemblyString(source, diagnostic, context);
    if (!module) diagnostic.print("invariant-callback-proof", llvm::errs());
    assert(module && !llvm::verifyModule(*module, &llvm::errs()));
    return module;
}

static unsigned selected(const llvm::Module& module) {
    unsigned count = 0, checked = 0;
    for (const auto& function : module) {
        if (++checked % 100 == 0) assert(!qore_check_cancel(nullptr, "test callback selection inspection"));
        for (const auto& block : function) {
            for (const auto& inst : block) {
                if (++checked % 100 == 0) assert(!qore_check_cancel(nullptr, "test callback selection inspection"));
                auto* call = llvm::dyn_cast<llvm::CallInst>(&inst);
                if (call && call->getCalledFunction()
                        && call->getCalledFunction()->getName().ends_with("_v9")) ++count;
            }
        }
    }
    return count;
}

static void check() {
    ExceptionSink xsink;
    QoreProgramHelper program(PO_MODERN, xsink);
    QoreExternalProgramContextHelper program_context(&xsink, *program);
    assert(!xsink);
    unsigned tests = 0;
    for (const char* name : {"eligible", "clone", "different-argument", "clear", "owner-load", "lifetime", "no-init",
            "late-init", "nonzero-store", "volatile-store", "atomic-store", "escape", "gep", "load", "phi",
            "constant", "alias-result", "bundle", "unknown-helper", "wrong-layout", "wrong-pointer-layout",
            "wrong-signature", "volatile-load", "nonentry-alloca", "array", "float-unary", "int-zero", "float-zero"}) {
        llvm::LLVMContext context;
        auto module = parse(context, fixture(name));
        std::string error;
        bool ok = QoreIRToLLVM::selectInvariantFunctionRefs(*module, error);
        if (!ok) std::fprintf(stderr, "%s: %s\n", name, error.c_str());
        assert(ok && error.empty());
        unsigned expected = std::string(name) == "clone" || std::string(name) == "clear" ? 2
            : std::string(name) == "eligible" || std::string(name) == "owner-load"
                || std::string(name) == "lifetime" || std::string(name) == "float-unary"
                || std::string(name) == "int-zero" || std::string(name) == "float-zero" ? 1 : 0;
        if (selected(*module) != expected) std::fprintf(stderr, "%s: selected %u, expected %u\n", name,
            selected(*module), expected);
        assert(selected(*module) == expected);
        ++tests;
    }
    {
        llvm::LLVMContext context;
        auto module = parse(context, fixture("clone"));
        std::string error;
        assert(!setenv("QORE_DISABLE_AOT_INVARIANT_FUNCREF", "1", 1));
        assert(QoreIRToLLVM::selectInvariantFunctionRefs(*module, error) && selected(*module) == 0);
        assert(!unsetenv("QORE_DISABLE_AOT_INVARIANT_FUNCREF"));
        assert(QoreIRToLLVM::selectInvariantFunctionRefs(*module, error) && selected(*module) == 2);
        ++tests;
    }
    {
        llvm::LLVMContext context;
        std::string source;
        for (unsigned i = 0; i < 110; ++i) {
            if (i % 100 == 0) assert(!qore_check_cancel(nullptr, "test callback cancellation setup"));
            source += "declare void @padding" + std::to_string(i) + "()\n";
        }
        auto module = parse(context, source + fixture("eligible"));
        std::string error;
        assert(!qore_cancel_thread(q_gettid(), "invariant callback proof regression"));
        bool ok = QoreIRToLLVM::selectInvariantFunctionRefs(*module, error);
        qore_clear_thread_cancel();
        assert(!ok && error == "cancelled during LLVM invariant callback proof" && selected(*module) == 0);
        error.clear();
        assert(QoreIRToLLVM::selectInvariantFunctionRefs(*module, error) && selected(*module) == 1);
        ++tests;
    }
    assert(!xsink);
    std::printf("invariant-callback-proof: %u cases passed\n", tests);
}

int main() {
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    check();
    qore_cleanup();
}
