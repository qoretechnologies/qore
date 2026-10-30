/* Copyright 2026 Qore Technologies, s.r.o. -- SPDX-License-Identifier: MIT */
#define _QORE_LIB_INTERN
#define XXH_INLINE_ALL
#include <qore/intern/xxhash/xxhash.h>
#include <qore/Qore.h>
#include <qore/intern/JITRuntime.h>
#include <qore/intern/QoreAOT.h>
#include <qore/intern/CallReferenceNode.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <cmath>
#if defined(__aarch64__)
#include <qore/macros-aarch64.h>
#elif defined(__x86_64__)
#include <qore/macros-x86_64.h>
#endif
#ifdef PROBE_FLOAT
using Scalar = double;
using NativeEntry = QoreAOTContext::NativeFloatUnaryMethodEntryV3;
using NativeEntry1 = QoreAOTContext::NativeFloatUnaryMethodEntry;
static auto& entry3(QoreAOTContext* ctx) { return ctx->native_float_unary_method_entry_v3; }
static auto& entry1(QoreAOTContext* ctx) { return ctx->native_float_unary_method_entry; }
static constexpr const char* factory = "CrossProgramMethodEntry::makeFloat";
static constexpr const char* call_name = "CrossProgramMethodEntry::callFloat";
#else
using Scalar = int64_t;
using NativeEntry = QoreAOTContext::NativeIntUnaryMethodEntryV3;
using NativeEntry1 = QoreAOTContext::NativeIntUnaryMethodEntry;
static auto& entry3(QoreAOTContext* ctx) { return ctx->native_int_unary_method_entry_v3; }
static auto& entry1(QoreAOTContext* ctx) { return ctx->native_int_unary_method_entry; }
static constexpr const char* factory = "CrossProgramMethodEntry::make";
static constexpr const char* call_name = "CrossProgramMethodEntry::call";
#endif
// A single-threaded test wraps a compiler-proven method in an unshared module
// context and restores its binding before destroying either Program.
static unsigned checks = 0;
#define CHECK(value) do { ++checks; assert(value); } while (false)
static NativeEntry target;
static NativeEntry1 target1;
static QoreProgram* caller;
static QoreObject* receiver;
static const qore_class_private* defining_class;
static unsigned calls = 0;
static bool force_overflow = false;
class StackProbe final : public QoreExternalRuntimeStackLocationHelper {
public:
    const QoreProgramLocation& getLocation() const override {
        static const QoreProgramLocation loc("cross-program-method-entry", 1, 1);
        return loc;
    }
    const std::string& getCallName() const override {
        static const std::string name("cross-program-method-entry");
        return name;
    }
    qore_call_t getCallType() const override { return CT_USER; }
};
static Scalar invokeBody(Scalar arg, QoreAOTContext* ctx, ExceptionSink* xsink,
        RuntimeConfig* rc, const size_t* limit, bool legacy) {
    ++calls;
    CHECK(ctx->pgm != caller && getProgram() == ctx->pgm && rc->getProgram() == ctx->pgm);
    {
        StackProbe observer;
        const auto* frame = observer.getNext();
        CHECK(frame && frame->getProgram() == caller);
    }
    struct RestoreLimit {
        size_t* address = nullptr;
        size_t saved = 0;
        ~RestoreLimit() { if (address) { *address = saved; } }
    } restore;
    if (force_overflow) {
#if defined(__aarch64__) || defined(__x86_64__)
        restore.address = const_cast<size_t*>(limit);
        CHECK(restore.address);
        restore.saved = *restore.address;
        *restore.address = get_stack_pos() + 4096;
#else
        CHECK(false);
#endif
    }
    auto check_materialized = [&]() {
        if (force_overflow) {
            CHECK(*xsink);
            CHECK(rc->getObject() == receiver && rc->getClass() == defining_class);
            CHECK(rc->getProgram() == ctx->pgm && getProgram() == ctx->pgm);
            StackProbe observer;
            const auto* frame = observer.getNext();
            CHECK(frame && frame->getProgram() == caller);
        }
    };
    try {
        Scalar result = legacy ? target1(arg, ctx, xsink) : target(arg, ctx, xsink, rc, limit);
        check_materialized();
        return result;
    } catch (...) {
        // Inspect while the dispatch scope still owns its context, before RAII restores the caller.
        check_materialized();
        throw;
    }
}
static Scalar invoke(Scalar arg, QoreAOTContext* ctx, ExceptionSink* xsink,
        RuntimeConfig* rc, const size_t* limit) {
    return invokeBody(arg, ctx, xsink, rc, limit, false);
}
static Scalar invokeLegacy(Scalar arg, QoreAOTContext* ctx, ExceptionSink* xsink) {
    RuntimeConfig* rc = qore_rt_get_fast_runtime_config();
    return invokeBody(arg, ctx, xsink, rc, qore_rt_get_stack_limit_v1(rc), true);
}
static void run(bool expected) {
    ExceptionSink xsink;
    QoreProgramHelper pgm(PO_MODERN, xsink);
    pgm->setExecMode(QEM_IR, true);
    pgm->parse("%modern\n%requires CrossProgramMethodEntry\n", "cross-program-method-entry", &xsink);
    if (xsink) { xsink.handleExceptions(); CHECK(false); }
    QoreExternalProgramContextHelper active(&xsink, *pgm);
    caller = *pgm;
    ValueHolder owner(pgm->callFunction(factory, nullptr, &xsink), &xsink);
    CHECK(!xsink && owner->getType() == NT_OBJECT);
    auto* object = owner->get<QoreObject>();
    CHECK(object->getProgram() != caller && object->getProgram() == object->getClass()->getSourceProgram());
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
    args->push(owner->refSelf(), &xsink);
    args->push(QoreValue(Scalar(7)), &xsink);
    CHECK(!xsink);
    RuntimeConfig* rc = qore_rt_get_fast_runtime_config();
    auto call = [&](Scalar input = 7, bool overflow = false) {
        args->setEntry(1, QoreValue(input), &xsink);
        CHECK(!xsink);
        RuntimeConfig before = *rc;
        ValueHolder answer(pgm->callFunction(call_name, *args, &xsink), &xsink);
        if (overflow) {
            CHECK(xsink && answer->isNothing());
            CHECK(xsink.getExceptionErr().get<const QoreStringNode>()->equal("STACK-LIMIT-EXCEEDED"));
            xsink.clear();
        } else {
            CHECK(!xsink);
#ifdef PROBE_FLOAT
            CHECK(std::isnan(input) ? std::isnan(answer->getAsFloat()) : answer->getAsFloat() == input + 17.0);
#else
            CHECK(answer->getAsBigInt() == input + 17);
#endif
        }
        CHECK(getProgram() == caller && rc->getProgram() == before.getProgram());
        CHECK(rc->getObject() == before.getObject() && rc->getClass() == before.getClass());
        CHECK(rc->getLocation() == before.getLocation() && rc->getStatement() == before.getStatement());
        CHECK(rc->getParseOptions() == before.getParseOptions() && rc->getClosureEnv() == before.getClosureEnv());
    };
    for (unsigned i = 0; i < 8; ++i) { call(); }
    const auto* method = object->getClass()->findMethod("next");
    CHECK(method);
    const auto* uvb = qore_method_private::get(*method)->getFunction()->first()->getUserVariantBase();
    CHECK(uvb && uvb->hasCachedAOT());
    receiver = object;
    defining_class = qore_class_private::get(*object->getClass());
    auto* ctx = uvb->getCachedAOTContext();
    CHECK(ctx && ctx->pgm == object->getProgram() && !ctx->uses_self && !ctx->uses_argv);
    target = entry3(ctx).load(std::memory_order_acquire);
    CHECK(target);
    target1 = entry1(ctx).load(std::memory_order_acquire);
    CHECK(target1);
    const auto* f = reinterpret_cast<const QoreFunction*>(pgm->findFunction(call_name));
    CHECK(f);
    auto* host = f->first()->getUserVariantBase()->getCachedAOTContext();
    CHECK(host);
    unsigned found = 0;
    for (int i = 0; i < host->num_exprs; ++i) {
        auto* cache = host->call_targets[i].method_dispatch_cache.load(std::memory_order_acquire);
        if (cache) {
            for (auto& slot : cache->entries) {
                const auto* entry = slot.load(std::memory_order_acquire);
                if (entry && entry->variant->getUserVariantBase() == uvb) {
                    CHECK(entry->caller_program == caller && entry->object_program == object->getProgram());
                    CHECK(entry->generic_fast_uvb == uvb && !entry->native_scalar_uvb);
                    ++found;
                }
            }
        }
        if (!(i % 100)) { CHECK(!qore_check_cancel(&xsink, "method probe slots")); }
    }
    CHECK(found == 1);
    entry3(ctx).store(invoke, std::memory_order_release);
    entry1(ctx).store(invokeLegacy, std::memory_order_release);
    for (unsigned i = 0; i < 8; ++i) { call(); }
    CHECK(calls == (expected ? 8 : 0));
    for (Scalar value : {Scalar(0), Scalar(-7), Scalar(std::numeric_limits<int64_t>::min()),
            Scalar(std::numeric_limits<int64_t>::max() - 17)}) {
        call(value);
    }
    unsigned expected_calls = 12;
#ifdef PROBE_FLOAT
    call(std::numeric_limits<double>::infinity());
    call(-std::numeric_limits<double>::infinity());
    call(std::numeric_limits<double>::quiet_NaN());
    expected_calls += 3;
#endif
    CHECK(calls == (expected ? expected_calls : 0));
    if (expected && qore_rt_get_stack_limit_v1(rc)) {
        force_overflow = true;
        call(7, true);
        force_overflow = false;
        call();
        expected_calls += 2;
        CHECK(calls == expected_calls);
    }
    auto old = entry1(ctx).exchange(nullptr, std::memory_order_acq_rel);
    unsigned saved_calls = calls;
    call();
    CHECK(calls == saved_calls);
    entry1(ctx).store(old, std::memory_order_release);
    auto normal = ctx->native_body_fn;
    ctx->native_body_fn = nullptr;
    call();
    CHECK(calls == saved_calls);
    ctx->native_body_fn = normal;
    entry3(ctx).store(target, std::memory_order_release);
    entry1(ctx).store(target1, std::memory_order_release);
    std::printf("cross-program-method-entry: %u entries; %u checks passed\n", calls, checks);
}
int main(int argc, char** argv) {
    CHECK(argc == 2);
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    run(std::atoi(argv[1]));
    qore_cleanup();
}
