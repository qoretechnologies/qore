/* Copyright (c) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
#define _QORE_LIB_INTERN
#define XXH_INLINE_ALL
#include <qore/intern/xxhash/xxhash.h>
#include <qore/Qore.h>
#include <qore/intern/QoreAOT.h>
#include <qore/intern/QoreIRToLLVM.h>

#include <llvm/IR/Verifier.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <string>

static constexpr unsigned local_count = 51;
static_assert(local_count < 100 && local_count * 2 >= 100);

static void checkLowering(const LVList& locals, bool fast, bool following_block) {
    llvm::LLVMContext context;
    llvm::Module module("local-cleanup-cancellation", context);
    QoreIRFunction ir("cleanup_probe");
    ir.all_body_locals = locals.lv;
    // An optimized-away body can still have runtime-visible local declarations.
    // With 51 locals and one instruction, earlier independent scans stay below
    // 100; the cleanup's shared forward/reverse counter reaches its checkpoint.
    ir.createBlock("entry")->appendInstruction<QoreIRReturnInstruction>();
    if (following_block) {
        // Exercise the instruction-loop rejection as well as the final
        // pre-verification rejection reached with a single return block.
        ir.createBlock("unreachable")->appendInstruction<QoreIRReturnInstruction>();
    }
    AOTSlotMap slots;
    for (unsigned i = 0; i < local_count; ++i) {
        slots.local_slots.emplace(locals.lv[i], static_cast<int32_t>(i));
    }
    QoreIRToLLVM lowerer(context);
    lowerer.setAOTMode(&slots);
    lowerer.setEmitDebugInfo(false);
    const std::string name = fast ? "cleanup_probe_fast" : ir.name;
    if (fast) {
        lowerer.setFastEntryMode(name, nullptr);
    }

    assert(!qore_cancel_thread(q_gettid(), "local cleanup emission regression"));
    std::string error;
    bool lowered = lowerer.lowerFunction(ir, module, error);
    qore_clear_thread_cancel();
    if (lowered || error != "cancelled during LLVM local cleanup emission") {
        std::fprintf(stderr, "unexpected lowering result (%d): %s\n", lowered, error.c_str());
        assert(false);
    }
    // No partially emitted body may be published, including the fast entry's
    // partially generated reverse pops. Reuse the very same compiler and name.
    assert(!module.getFunction(name));
    assert(!llvm::verifyModule(module, &llvm::errs()));
    error.clear();
    lowered = lowerer.lowerFunction(ir, module, error);
    if (!lowered) {
        std::fprintf(stderr, "lowerer did not recover: %s\n", error.c_str());
        assert(false);
    }
    assert(error.empty());
    assert(module.getFunction(name) && !module.getFunction(name)->isDeclaration());
    assert(!llvm::verifyModule(module, &llvm::errs()));
}

static void check() {
    ExceptionSink xsink;
    QoreProgramHelper program(PO_MODERN, xsink);
    std::string source = "%modern\nsub localDeclarations() {\n";
    for (unsigned i = 0; i < local_count; ++i) {
        source += "auto local" + std::to_string(i) + ";\n";
    }
    source += "}\n";
    program->parse(source.c_str(), "local-cleanup-declarations", &xsink);
    assert(!xsink);
    QoreExternalProgramContextHelper program_context(&xsink, *program);
    assert(!xsink);
    // QoreExternalFunction is the public view of QoreFunction; declarations
    // stay owned by this Program until both compiler instances are destroyed.
    const auto* function = reinterpret_cast<const QoreFunction*>(program->findFunction("localDeclarations"));
    assert(function && function->first()->getUserVariantBase());
    const LVList* locals = function->first()->getUserVariantBase()->getStatementBlock()->getLVList();
    assert(locals && locals->size() == local_count);
    checkLowering(*locals, false, false);
    checkLowering(*locals, false, true);
    checkLowering(*locals, true, false);
    checkLowering(*locals, true, true);
    assert(!xsink);
}

int main() {
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    check();
    qore_cleanup();
    std::puts("aot-local-cleanup-cancellation: ok");
}
