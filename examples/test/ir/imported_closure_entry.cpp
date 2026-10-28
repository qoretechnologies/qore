/* Copyright 2026 Qore Technologies, s.r.o. -- SPDX-License-Identifier: MIT */
#define _QORE_LIB_INTERN
#define XXH_INLINE_ALL
#include <qore/intern/xxhash/xxhash.h>
#include <qore/Qore.h>
#include <qore/QoreDebugProgram.h>
#include <qore/intern/JITRuntime.h>
#include <qore/intern/QoreAOT.h>
#include <qore/intern/QoreClosureNode.h>
#include <qore/intern/qore_program_private.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <utility>
#if defined(__aarch64__)
#include <qore/macros-aarch64.h>
#elif defined(__x86_64__)
#include <qore/macros-x86_64.h>
#endif

static const QoreMethod* inspect_options = nullptr;
static unsigned checks = 0;
#define CHECK(value) do { ++checks; assert(value); } while (false)

static bool callerAllowsInjection(ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
    args->push(QoreValue(int64_t(0)), xsink);
    CHECK(!*xsink);
    ValueHolder result(inspect_options->execManaged(nullptr, *args, xsink), xsink);
    CHECK(!*xsink);
    return bool(QoreParseOptions(result->getAsBigInt()) & PO_ALLOW_INJECTION);
}

// A test-only builtin call refreshes the compatibility stack snapshot; the
// admitted scalar body cannot make this call itself. Preserve that test effect.
static bool liveAllowsInjection(ExceptionSink* xsink) {
    RuntimeConfig* rc = qore_rt_get_fast_runtime_config();
    const QoreStackLocation* saved_snapshot = rc->getStackLocation();
    bool allowed = callerAllowsInjection(xsink);
    rc->setStackLocation(saved_snapshot);
    return allowed;
}

static void sameContext(const RuntimeConfig& before, bool allows_injection) {
    RuntimeConfig after = *qore_rt_get_fast_runtime_config();
    CHECK(before.getProgram() == after.getProgram());
    CHECK(before.getLocation() == after.getLocation());
    CHECK(before.getStatement() == after.getStatement());
    CHECK(before.getParseOptions() == after.getParseOptions());
    CHECK(before.getObject() == after.getObject() && before.getClass() == after.getClass());
    CHECK(before.getClosureEnv() == after.getClosureEnv());
    CHECK(before.getStackLocation() == after.getStackLocation());
    ExceptionSink xsink;
    CHECK(liveAllowsInjection(&xsink) == allows_injection);
}

class StackProbe final : public QoreExternalRuntimeStackLocationHelper {
public:
    const QoreProgramLocation& getLocation() const override {
        static const QoreProgramLocation loc("imported-closure-probe", 1, 1);
        return loc;
    }
    const std::string& getCallName() const override {
        static const std::string name("imported-closure-probe");
        return name;
    }
    qore_call_t getCallType() const override { return CT_USER; }
};

// This single-threaded fixture wraps an unshared module context's compiler-proven
// entry. It observes cold materialization without broadening production eligibility.
struct EntryProbe {
    QoreAOTContext* ctx;
    QoreAOTContext::NativeIntUnaryClosureEntry target;
    QoreProgram* caller;
    bool overflow = false;
    bool reject = false;
    unsigned calls = 0;
    static EntryProbe* current;
    EntryProbe(QoreAOTContext* ctx, QoreProgram* caller) : ctx(ctx),
            target(ctx->native_int_unary_closure_entry_v2.load(std::memory_order_acquire)), caller(caller) {
        CHECK(target && !current);
        current = this;
        ctx->native_int_unary_closure_entry_v2.store(invoke, std::memory_order_release);
    }
    ~EntryProbe() {
        ctx->native_int_unary_closure_entry_v2.store(target, std::memory_order_release);
        current = nullptr;
    }
    static int64_t invoke(int64_t arg, const QoreClosureBase* closure, RuntimeConfig* rc,
            int* accepted, QoreAOTContext* ctx, ExceptionSink* xsink) {
        EntryProbe& probe = *current;
        ++probe.calls;
        CHECK(ctx == probe.ctx && ctx->pgm != closure->getProgram());
        CHECK(getProgram() == closure->getProgram() && rc->getProgram() == closure->getProgram());
        {
            StackProbe observer;
            const QoreStackLocation* frame = observer.getNext();
            CHECK(frame && frame->getCallName() == "<anonymous closure>");
            CHECK(frame->getProgram() == probe.caller);
        }
        if (probe.reject) {
            *accepted = 0;
            return 0;
        }
        struct RestoreLimit {
            size_t* address = nullptr;
            size_t saved = 0;
            ~RestoreLimit() { if (address) { *address = saved; } }
        } restore;
        if (probe.overflow) {
#if defined(__aarch64__) || defined(__x86_64__)
            restore.address = const_cast<size_t*>(qore_rt_get_stack_limit_v1(rc));
            CHECK(restore.address);
            restore.saved = *restore.address;
            *restore.address = get_stack_pos() + 4096;
#else
            CHECK(false);
#endif
        }
        int64_t result = probe.target(arg, closure, rc, accepted, ctx, xsink);
        if (probe.overflow) {
            *restore.address = restore.saved;
            CHECK(*xsink && qore_get_call_program_context() == closure->getProgram());
            CHECK(rc->getObject() == closure->getObject() && rc->getClass() == closure->getClassCtx());
            // Inspect through the existing public builtin after the native guard
            // has materialized the frame; no private TLS symbol is exported.
            // Preserve the original stack exception while executing the builtin.
            ExceptionSink options_sink;
            CHECK(liveAllowsInjection(&options_sink));
            // A cold call may replace the RuntimeConfig implementation; restoration
            // must update the current implementation, not a retained old pointer.
            RuntimeConfig replacement(*rc);
            *rc = std::move(replacement);
        }
        return result;
    }
};
EntryProbe* EntryProbe::current = nullptr;

static uint64_t wrongBody(QoreAOTContext*, ExceptionSink*) {
    CHECK(false);
    return 0;
}

static void checkClosure(QoreProgram* pgm, const char* factory, bool enabled) {
    ExceptionSink xsink;
    ValueHolder value(pgm->callFunction(factory, nullptr, &xsink), &xsink);
    CHECK(!xsink && value->getType() == NT_RUNTIME_CLOSURE);
    const auto* closure = static_cast<const QoreClosureBase*>(value->getInternalNode());
    const auto* uvb = closure->getFunction()->first()->getUserVariantBase();
    CHECK(uvb && uvb->pgm != pgm && closure->getProgram() == pgm);
    CHECK(!(uvb->pgm->getParseOptions() & PO_ALLOW_INJECTION));
    RuntimeConfig* rc = qore_rt_get_fast_runtime_config();
    const uint64_t bits = value->rawBits();
    const bool original_options = liveAllowsInjection(&xsink);
    auto ordinary = [&](int64_t argument, bool overflow = false) {
        ValueHolder arg(QoreValue(argument), &xsink);
        uint64_t args[] = {arg->rawBits()};
        uint64_t raw = qore_rt_call_closure_fast(bits, args, 1, &xsink);
        QoreValue result;
        std::memcpy(&result, &raw, sizeof(result));
        ValueHolder answer(result, &xsink);
        CHECK(bool(xsink) == overflow);
        if (overflow) {
            CHECK(answer->isNothing());
            CHECK(xsink.getExceptionErr().get<const QoreStringNode>()->equal("STACK-LIMIT-EXCEEDED"));
            xsink.clear();
        } else {
            CHECK(answer->getAsBigInt() == argument + 17);
        }
    };
    ordinary(7);
    QoreAOTContext* ctx = uvb->getCachedAOTContext();
    CHECK(ctx && ctx->pgm == uvb->pgm && ctx->native_int_unary_closure_entry_v2.load());
    QorePreparedFunctionRefV3 cache{};
    QorePreparedFunctionRefFrameV4 frame{};
    for (unsigned helper : {3, 9}) {
        auto invoke = [&](int64_t argument, int64_t& result) {
            return helper == 3
                ? qore_rt_try_funcref_int_int_v3(bits, argument, &result, &xsink, rc, &cache)
                : qore_rt_try_funcref_int_int_v9(bits, argument, &result, &xsink, rc, &cache, &frame);
        };
        RuntimeConfig before = *rc;
        for (int64_t argument : {int64_t(0), int64_t(7), int64_t(-7),
                std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max() - 17}) {
            int64_t result = -1;
            CHECK(invoke(argument, result) == enabled);
            CHECK(!xsink && result == (enabled ? argument + 17 : -1));
            ordinary(argument);
            sameContext(before, original_options);
            CHECK(!cache.owner && !frame.frame);
        }
        if (!enabled) { continue; }
        auto normal = ctx->native_body_fn;
        ctx->native_body_fn = wrongBody;
        int64_t result = -1;
        CHECK(invoke(7, result) == 0 && result == -1 && !xsink);
        ctx->native_body_fn = normal;
        auto native = ctx->native_int_unary_closure_entry_v2.load();
        ctx->native_int_unary_closure_entry_v2.store(nullptr);
        CHECK(invoke(7, result) == 0 && !xsink);
        ctx->native_int_unary_closure_entry_v2.store(native);
        ReferenceHolder<QoreDebugProgram> debugger(new QoreDebugProgram(), &xsink);
        for (QoreProgram* attached : {pgm, uvb->pgm}) {
            debugger->addProgram(attached, &xsink);
            CHECK(!xsink && qore_program_private::get(*attached)->hasDebuggerAttached());
            CHECK(invoke(7, result) == 0 && !xsink);
            debugger->removeProgram(attached);
            CHECK(invoke(7, result) == 1 && result == 24 && !xsink);
        }
        xsink.raiseException("IMPORTED-CLOSURE-PENDING", "a pending exception prevents native execution");
        CHECK(invoke(7, result) == 1 && result == 0 && xsink);
        CHECK(xsink.getExceptionErr().get<const QoreStringNode>()->equal("IMPORTED-CLOSURE-PENDING"));
        xsink.clear();
        sameContext(before, original_options);
        EntryProbe probe(ctx, pgm);
        for (bool requested : {false, true, false}) {
            probe.overflow = requested && qore_rt_get_stack_limit_v1(rc);
            CHECK(invoke(7, result) == 1);
            CHECK(bool(xsink) == probe.overflow && result == (probe.overflow ? 0 : 24));
            if (probe.overflow) {
                CHECK(xsink.getExceptionErr().get<const QoreStringNode>()->equal("STACK-LIMIT-EXCEEDED"));
                xsink.clear();
            }
            sameContext(before, original_options);
        }
        probe.reject = true;
        CHECK(invoke(7, result) == 0 && !xsink);
        ordinary(7); // rejected native capture admission falls back without changing the answer
        sameContext(before, original_options);
        probe.reject = false;
        QoreProgramHelper foreign(PO_MODERN, xsink);
        {
            QoreExternalProgramContextHelper active(&xsink, *foreign);
            const bool foreign_options = liveAllowsInjection(&xsink);
            RuntimeConfig foreign_before = *qore_rt_get_fast_runtime_config();
            CHECK(invoke(7, result) == 0 && !xsink);
            probe.caller = *foreign;
            unsigned calls = probe.calls;
            ordinary(7);
            CHECK(probe.calls == calls + 1);
            sameContext(foreign_before, foreign_options);
            if (qore_rt_get_stack_limit_v1(rc)) {
                probe.overflow = true;
                ordinary(7, true);
                probe.overflow = false;
                CHECK(probe.calls == calls + 2);
                sameContext(foreign_before, foreign_options);
            }
        }
        sameContext(before, original_options);
    }
}

int main(int argc, char** argv) {
    CHECK(argc == 2);
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    {
        ExceptionSink xsink;
        QoreProgramHelper pgm(PO_MODERN | PO_ALLOW_DEBUGGER, xsink);
        pgm->setExecMode(QEM_IR, true);
        pgm->parse("%modern\n%requires ImportedClosureEntry\n"
            "code<int(int)> sub plain() { return ImportedClosureEntry::plain(); }\n"
            "code<int(int)> sub capture() { return ImportedClosureEntry::capture(); }\n",
            "imported-closure-entry", &xsink);
        if (xsink) { xsink.handleExceptions(); CHECK(false); }
        pgm->setParseOptions(PO_ALLOW_INJECTION, &xsink);
        QoreExternalProgramContextHelper active(&xsink, *pgm);
        CHECK(!xsink);
        const QoreClass* cls = pgm->findClass("Qore::Program", &xsink);
        CHECK(!xsink && cls);
        inspect_options = cls->findStaticMethod("getCallerCapabilityMask");
        CHECK(inspect_options);
        for (const char* factory : {"plain", "capture"}) {
            checkClosure(*pgm, factory, std::atoi(argv[1]));
        }
    }
    qore_cleanup();
    std::printf("imported-closure-entry: %u checks passed\n", checks);
}
