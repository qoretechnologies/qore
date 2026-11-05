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
using NativeEntry = QoreAOTContext::ScalarMemberFloatEntry;
static auto& entry(QoreAOTContext* ctx) { return ctx->scalar_member_float_entry; }
static constexpr const char* factory = "CrossProgramMemberEntry::makeFloat";
static constexpr const char* call_name = "CrossProgramMemberEntry::callFloat";
#else
using Scalar = int64_t;
using NativeEntry = QoreAOTContext::ScalarMemberIntEntry;
static auto& entry(QoreAOTContext* ctx) { return ctx->scalar_member_int_entry; }
#ifdef PROBE_EFFECT
static constexpr const char* factory = "CrossProgramMemberEntry::makeEffect";
static constexpr const char* call_name = "CrossProgramMemberEntry::callEffect";
#else
static constexpr const char* factory = "CrossProgramMemberEntry::make";
static constexpr const char* call_name = "CrossProgramMemberEntry::call";
#endif
#endif
// Only this thread uses the test context. Restore wrapped companions before
// destroying either Program; no production publication contract is changed.
static unsigned checks = 0;
#define CHECK(value) do { ++checks; assert(value); } while (false)
static NativeEntry target;
static QoreProgram* caller;
static unsigned calls = 0;
static bool force_overflow = false;
#ifdef PROBE_EFFECT
static uint64_t (*resume_target)(int32_t, QoreAOTContext*, ExceptionSink*);
static unsigned resumes = 0;
#endif
class StackProbe final : public QoreExternalRuntimeStackLocationHelper {
public:
    const QoreProgramLocation& getLocation() const override {
        static const QoreProgramLocation loc("cross-program-member-entry", 1, 1);
        return loc;
    }
    const std::string& getCallName() const override {
        static const std::string name("cross-program-member-entry");
        return name;
    }
    qore_call_t getCallType() const override { return CT_USER; }
};
static void observe(QoreAOTContext* ctx, RuntimeConfig* rc) {
    CHECK(ctx->pgm != caller && getProgram() == ctx->pgm && rc->getProgram() == ctx->pgm);
    StackProbe observer;
    const auto* frame = observer.getNext();
    CHECK(frame && frame->getProgram() == caller);
}
static Scalar invoke(Scalar arg, QoreObject* object, const qore_class_private* class_ctx,
        RuntimeConfig* rc, int* accepted, QoreAOTContext* ctx, ExceptionSink* xsink) {
    ++calls;
    observe(ctx, rc);
    CHECK(rc->getObject() == object && rc->getClass() == class_ctx);
    struct RestoreLimit {
        size_t* address = nullptr;
        size_t saved = 0;
        ~RestoreLimit() { if (address) { *address = saved; } }
    } restore;
    if (force_overflow) {
#if defined(__aarch64__) || defined(__x86_64__)
        restore.address = const_cast<size_t*>(qore_rt_get_stack_limit_v1(rc));
        CHECK(restore.address);
        restore.saved = *restore.address;
        *restore.address = get_stack_pos() + 4096;
#else
        CHECK(false);
#endif
    }
    return target(arg, object, class_ctx, rc, accepted, ctx, xsink);
}
#ifdef PROBE_EFFECT
static uint64_t resume(int32_t completed, QoreAOTContext* ctx, ExceptionSink* xsink) {
    ++resumes;
    CHECK(completed == 1);
    observe(ctx, qore_rt_get_fast_runtime_config());
    return resume_target(completed, ctx, xsink);
}
#endif
static void run(bool expected) {
    ExceptionSink xsink;
    QoreProgramHelper pgm(PO_MODERN, xsink);
    pgm->setExecMode(QEM_IR, true);
    pgm->parse("%modern\n%requires CrossProgramMemberEntry\n", "cross-program-member-entry", &xsink);
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
    auto call = [&](Scalar input = 7, bool overflow = false, bool cold = false) {
        args->setEntry(1, QoreValue(input), &xsink);
#ifdef PROBE_EFFECT
        const int64_t second = cold ? int64_t(1) << 60 : 20;
        object->setMemberValue("first", nullptr, QoreValue(int64_t(0)), &xsink);
        object->setMemberValue("second", nullptr, QoreValue(second), &xsink);
#endif
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
#ifdef PROBE_EFFECT
        ValueHolder first_value(object->evalMember("first", &xsink), &xsink);
        ValueHolder second_value(object->evalMember("second", &xsink), &xsink);
        CHECK(!xsink && first_value->getAsBigInt() == (overflow ? 0 : 1));
        CHECK(second_value->getAsBigInt() == second - (overflow ? 0 : 1));
#endif
    };
    for (unsigned i = 0; i < 8; ++i) { call(); }
    const auto* method = object->getClass()->findMethod("next");
    CHECK(method);
    const auto* uvb = qore_method_private::get(*method)->getFunction()->first()->getUserVariantBase();
    CHECK(uvb && uvb->hasCachedAOT());
    auto* ctx = uvb->getCachedAOTContext();
    CHECK(ctx && ctx->pgm == object->getProgram() && ctx->uses_self && !ctx->uses_argv);
    target = entry(ctx).load(std::memory_order_acquire);
    CHECK(target);
    const auto* f = reinterpret_cast<const QoreFunction*>(pgm->findFunction(call_name));
    CHECK(f);
    auto* host = f->first()->getUserVariantBase()->getCachedAOTContext();
    CHECK(host);
    unsigned found = 0;
    for (int i = 0; i < host->num_exprs; ++i) {
        auto* cache = host->call_targets[i].method_dispatch_cache.load(std::memory_order_acquire);
        if (cache) {
            for (auto& slot : cache->entries) {
                const auto* cached = slot.load(std::memory_order_acquire);
                if (cached && cached->variant->getUserVariantBase() == uvb) {
                    CHECK(cached->caller_program == caller && cached->object_program == object->getProgram());
                    CHECK(cached->generic_fast_uvb == uvb && !cached->native_scalar_uvb);
                    CHECK(cached->generic_scalar_params == !std::getenv("QORE_DISABLE_AOT_SCALAR_PARAM_BINDING_CACHE"));
                    ++found;
                }
            }
        }
        if (!(i % 100)) { CHECK(!qore_check_cancel(&xsink, "member probe slots")); }
    }
    CHECK(found == 1);
#ifdef PROBE_EFFECT
    const auto* original = ctx->effect_member_int_entry.load(std::memory_order_acquire);
    CHECK(original && original->fast == target && original->updates == 2);
    auto wrapped = *original;
    wrapped.fast = invoke;
    resume_target = original->resume;
    wrapped.resume = resume;
    ctx->effect_member_int_entry.store(&wrapped, std::memory_order_release);
#endif
    entry(ctx).store(invoke, std::memory_order_release);
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
#ifdef PROBE_EFFECT
    call(7, false, true);
    ++expected_calls;
    CHECK(resumes == (expected ? 1 : 0));
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
    auto old = entry(ctx).exchange(nullptr, std::memory_order_acq_rel);
    unsigned saved_calls = calls;
    call();
    CHECK(calls == saved_calls);
    entry(ctx).store(old, std::memory_order_release);
    auto normal = ctx->native_body_fn;
    ctx->native_body_fn = nullptr;
    call();
    CHECK(calls == saved_calls);
    ctx->native_body_fn = normal;
    entry(ctx).store(target, std::memory_order_release);
#ifdef PROBE_EFFECT
    ctx->effect_member_int_entry.store(original, std::memory_order_release);
#endif
    std::printf("cross-program-member-entry: %u entries; %u checks passed\n", calls, checks);
}
int main(int argc, char** argv) {
    CHECK(argc == 2);
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    run(std::atoi(argv[1]));
    qore_cleanup();
}
