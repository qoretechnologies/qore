/* Copyright (c) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
#define _QORE_LIB_INTERN
#define XXH_INLINE_ALL
#include <qore/intern/xxhash/xxhash.h>
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <qore/intern/QoreAOT.h>
#include <qore/intern/JITRuntime.h>
#include <qore/intern/QoreClosureNode.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <thread>
#if defined(__aarch64__)
#include <qore/macros-aarch64.h>
#elif defined(__x86_64__)
#include <qore/macros-x86_64.h>
#endif

extern "C" void init_CLOSURE_DEFERRED_CONTEXT_qo(QoreProgram*);

// Each worker owns a separate Program; this test-only adapter wraps the real
// compiler-proven entry and changes its live limit only after frame publication.
struct ClosureProbe {
    QoreAOTContext* ctx;
    QoreAOTContext::NativeIntUnaryClosureEntry target;
    bool overflow = false;
    unsigned calls = 0;
    static thread_local ClosureProbe* current;
    explicit ClosureProbe(QoreAOTContext* ctx) : ctx(ctx),
            target(ctx->native_int_unary_closure_entry.load(std::memory_order_acquire)) {
        assert(target && !current);
        current = this;
        ctx->native_int_unary_closure_entry.store(invoke, std::memory_order_release);
    }
    ~ClosureProbe() {
        ctx->native_int_unary_closure_entry.store(target, std::memory_order_release);
        current = nullptr;
    }
    static int64_t invoke(int64_t arg, const QoreClosureBase* closure, RuntimeConfig* rc,
            int* accepted, QoreAOTContext* ctx, ExceptionSink* xsink) {
        auto& probe = *current;
        assert(ctx == probe.ctx);
        ++probe.calls;
        struct RestoreLimit {
            size_t* address = nullptr;
            size_t saved = 0;
            ~RestoreLimit() { if (address) { *address = saved; } }
        } restore;
        if (probe.overflow) {
#if defined(__aarch64__) || defined(__x86_64__)
            restore.address = const_cast<size_t*>(qore_rt_get_stack_limit_v1(rc));
            assert(restore.address);
            restore.saved = *restore.address;
            *restore.address = get_stack_pos() + 4096;
#else
            assert(false && "requires native stack view");
#endif
        }
        int64_t result = probe.target(arg, closure, rc, accepted, ctx, xsink);
        if (probe.overflow) {
            assert(*xsink && qore_get_call_program_context() == closure->getProgram());
            assert(rc->getObject() == closure->getObject() && rc->getClass() == closure->getClassCtx());
        }
        return result;
    }
};
thread_local ClosureProbe* ClosureProbe::current = nullptr;

static void sameContext(const RuntimeConfig& before) {
    RuntimeConfig after = *qore_rt_get_fast_runtime_config();
    assert(before.getProgram() == after.getProgram());
    assert(before.getLocation() == after.getLocation());
    assert(before.getStatement() == after.getStatement());
    assert(before.getParseOptions() == after.getParseOptions());
    assert(before.getObject() == after.getObject() && before.getClass() == after.getClass());
    assert(before.getClosureEnv() == after.getClosureEnv());
    assert(before.getStackLocation() == after.getStackLocation());
}

struct Invocation {
    uint64_t bits;
    QoreAOTContext* ctx;
    QorePreparedFunctionRefV3 cache{};
};

static QoreValue sweep(const void* ptr, const QoreListNode*, RuntimeConfig&, ExceptionSink* xsink) {
    auto& invocation = *static_cast<Invocation*>(const_cast<void*>(ptr));
    RuntimeConfig* rc = qore_rt_get_fast_runtime_config();
    RuntimeConfig before = *rc;
    QoreProgram* caller = qore_get_call_program_context();
    ClosureProbe probe(invocation.ctx);
    for (bool request_overflow : {false, true, false}) {
        // Unsupported native views still exercise ordinary calls/restoration;
        // the production full stack guard remains enabled on those targets.
        bool overflow = request_overflow && qore_rt_get_stack_limit_v1(rc);
        probe.overflow = overflow;
        int64_t result = 123;
        assert(qore_rt_try_funcref_int_int_v3(invocation.bits, 7, &result, xsink, rc, &invocation.cache) == 1);
        assert(result == (overflow ? 0 : 24) && static_cast<bool>(*xsink) == overflow);
        sameContext(before);
        assert(qore_get_call_program_context() == caller);
        if (overflow) {
            assert(xsink->getExceptionErr().get<const QoreStringNode>()->equal("STACK-LIMIT-EXCEEDED"));
            ReferenceHolder<QoreHashNode> info(xsink->getExceptionInfo(), xsink);
            assert(!*xsink);
            const auto* stack = info->getKeyValue("callstack").get<const QoreListNode>();
            assert(stack);
            unsigned closures = 0;
            for (size_t i = 0; i < stack->size(); ++i) {
                const auto* frame = stack->retrieveEntry(i).get<const QoreHashNode>();
                const auto* name = frame->getKeyValue("function").get<const QoreStringNode>();
                if (name && name->equal("<anonymous closure>")) {
                    ++closures;
                    assert(frame->getKeyValue("file").get<const QoreStringNode>()->strlen());
                }
                if (!(i % 100)) { assert(!qore_check_cancel(xsink, "closure overflow stack inspection")); }
            }
            assert(closures == 1);
        }
    }
    assert(probe.calls == 3);
    xsink->raiseException("CLOSURE-PENDING", "pending sink must prevent entry");
    int64_t result = 123;
    assert(qore_rt_try_funcref_int_int_v3(invocation.bits, 7, &result, xsink, rc, &invocation.cache) == 1);
    assert(*xsink && result == 0 && probe.calls == 3);
    xsink->clear();
    sameContext(before);
    bool ready = invocation.ctx->pc_loc_map_attached.exchange(false, std::memory_order_acq_rel);
    assert(ready);
    result = 123;
    int handled = qore_rt_try_funcref_int_int_v3(invocation.bits, 7, &result, xsink, rc, &invocation.cache);
    invocation.ctx->pc_loc_map_attached.store(ready, std::memory_order_release);
    assert(!handled && !*xsink && result == 123 && probe.calls == 3);
    sameContext(before);
    return QoreValue(int64_t(24));
}

static void check() {
    ExceptionSink xsink;
    QoreProgramHelper program(PO_MODERN, xsink);
    Invocation invocation{};
    QoreValue empty;
    uint64_t empty_bits;
    std::memcpy(&empty_bits, &empty, sizeof(empty_bits));
    invocation.cache.owner = empty_bits;
    {
        ProgramRuntimeExternalParseContextHelper parse(*program);
        assert(parse);
        program->getRootNS()->addBuiltinVariant(&invocation, "runClosureSweep", sweep,
            QCF_NO_FLAGS, QDOM_DEFAULT, bigIntTypeInfo, 0);
    }
    qore_aot_script_begin_batch(*program);
    init_CLOSURE_DEFERRED_CONTEXT_qo(*program);
    assert(!qore_aot_script_end_batch(*program));
    QoreExternalProgramContextHelper context(&xsink, *program);
    // Caller locals and captures span variable-stack allocation boundaries.
    std::string source = "%modern\n";
    for (unsigned padding : {0, 31, 32, 33, 63, 64, 65, 70}) {
        source += "int sub padded" + std::to_string(padding) + "() {\n";
        for (unsigned i = 0; i <= padding; ++i) {
            source += "string local" + std::to_string(i) + " = \"preserved\";\n";
            source += "int capture" + std::to_string(i) + " = 17;\n";
        }
        source += "code<int()> callback = int sub() { return capture0";
        for (unsigned i = 1; i <= padding; ++i) { source += " + capture" + std::to_string(i); }
        source += "; };\nint result = runClosureSweep();\n";
        source += "if (callback() != " + std::to_string(17 * (padding + 1))
            + ") { throw \"CLOSURE-CONTEXT\", \"capture changed\"; }\n";
        for (unsigned i = 0; i <= padding; ++i) {
            source += "if (local" + std::to_string(i)
                + " != \"preserved\") { throw \"CLOSURE-CONTEXT\", \"local changed\"; }\n";
        }
        source += "return result;\n}\n";
    }
    {
        ProgramRuntimeExternalParseContextHelper parse(*program);
        assert(parse);
        program->parse(source.c_str(), "closure-context-padded", &xsink);
        assert(!xsink);
    }
    for (const char* factory : {"closureContextPlain", "closureContextCaptured", "closureContextObject"}) {
        ValueHolder ref(program->callFunction(factory, nullptr, &xsink), &xsink);
        assert(!xsink && ref->getType() == NT_RUNTIME_CLOSURE);
        const auto* closure = static_cast<const QoreClosureBase*>(ref->getInternalNode());
        const auto* uvb = closure->getFunction()->first()->getUserVariantBase();
        ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
        args->push(ref->refSelf(), &xsink);
        args->push(QoreValue(int64_t(7)), &xsink);
        assert(!xsink);
        ValueHolder warm(program->callFunction("closureContextCaller", *args, &xsink), &xsink);
        assert(!xsink && warm->getAsBigInt() == 24);
        invocation.ctx = uvb->getCachedAOTContext();
        assert(invocation.ctx && invocation.ctx->native_int_unary_closure_entry.load(std::memory_order_acquire));
        std::memcpy(&invocation.bits, &*ref, sizeof(invocation.bits));
        qore_exec_mode_t mode = program->getExecMode();
        program->setExecMode(QEM_AST, true);
        for (unsigned padding : {0, 31, 32, 33, 63, 64, 65, 70}) {
            std::string name = "padded" + std::to_string(padding);
            ValueHolder value(program->callFunction(name.c_str(), nullptr, &xsink), &xsink);
            assert(!xsink && value->getAsBigInt() == 24 && invocation.cache.owner == empty_bits);
        }
        program->setExecMode(mode, true);
        QoreProgramHelper other(PO_MODERN, xsink);
        QoreExternalProgramContextHelper other_context(&xsink, *other);
        int64_t result = 123;
        assert(qore_rt_try_funcref_int_int_v3(invocation.bits, 7, &result, &xsink,
            qore_rt_get_fast_runtime_config(), &invocation.cache) == 0);
        assert(!xsink && result == 123 && invocation.cache.owner == empty_bits);
    }
}

int main() {
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    check();
    std::thread worker([]() {
        for (unsigned i = 0; i < 3; ++i) {
            QoreForeignThreadHelper foreign;
            check();
        }
    });
    worker.join();
    qore_cleanup();
    std::puts("closure-deferred-context: ok");
}
