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
#include <qore/intern/CallReferenceNode.h>

extern "C" void init_ZERO_ARG_GETTER_qo(QoreProgram*);

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <thread>
#include <type_traits>

#if defined(__aarch64__)
#include <qore/macros-aarch64.h>
#elif defined(__x86_64__)
#include <qore/macros-x86_64.h>
#endif

// Every invocation owns an unshared Program. The adapter forwards to the real
// compiled companion and restores the atomic before Program destruction.
template <typename Scalar>
struct GetterProbe {
    using Entry = std::conditional_t<std::is_same_v<Scalar, double>,
        QoreAOTContext::ScalarGetterFloatEntryV1, QoreAOTContext::ScalarGetterIntEntryV1>;
    static thread_local GetterProbe* current;
    QoreAOTContext* ctx;
    QoreObject* object;
    Entry saved;
    unsigned calls = 0;
    unsigned misses = 0;
    bool overflow = false;
    static auto& binding(QoreAOTContext* ctx) {
        if constexpr (std::is_same_v<Scalar, double>) {
            return ctx->scalar_getter_float_entry_v1;
        } else {
            return ctx->scalar_getter_int_entry_v1;
        }
    }
    GetterProbe(QoreAOTContext* ctx, QoreObject* object) : ctx(ctx), object(object),
            saved(binding(ctx).load(std::memory_order_acquire)) {
        assert(!current);
        current = this;
        if (saved) { binding(ctx).store(invoke, std::memory_order_release); }
    }
    ~GetterProbe() {
        binding(ctx).store(saved, std::memory_order_release);
        current = nullptr;
    }
    static Scalar invoke(QoreObject* object, const qore_class_private* cls, RuntimeConfig* rc,
            int* accepted, QoreAOTContext* ctx, ExceptionSink* xsink) {
        assert(current && current->ctx == ctx && current->object == object && current->saved);
        assert(rc && rc->getObject() == object && rc->getClass() == cls);
        assert(accepted && *accepted == 1 && !*xsink);
        ++current->calls;
        struct RestoreLimit {
            size_t* address = nullptr;
            size_t saved = 0;
            ~RestoreLimit() { if (address) { *address = saved; } }
        } limit;
        if (current->overflow) {
#if defined(__aarch64__) || defined(__x86_64__)
            limit.address = const_cast<size_t*>(qore_rt_get_stack_limit_v1(rc));
            assert(limit.address);
            limit.saved = *limit.address;
            *limit.address = get_stack_pos() + 4096;
#else
            assert(false && "forced getter limit requires a native stack view");
#endif
        }
        Scalar result = current->saved(object, cls, rc, accepted, ctx, xsink);
        if (current->overflow) {
            assert(*xsink && result == 0 && *accepted == 1);
            assert(rc->getObject() == object && rc->getClass() == cls);
        }
        if (!*accepted) { ++current->misses; }
        return result;
    }
};
template <typename Scalar> thread_local GetterProbe<Scalar>* GetterProbe<Scalar>::current = nullptr;

static bool enabled(const char* name) { return !std::getenv(name); }

template <typename Scalar>
static void checkGetter(QoreProgram* program, const char* factory) {
    constexpr bool floating = std::is_same_v<Scalar, double>;
    using Probe = GetterProbe<Scalar>;
    ExceptionSink xsink;
    ValueHolder owner(program->callFunction(factory, nullptr, &xsink), &xsink);
    assert(!xsink && owner->getType() == NT_OBJECT);
    QoreObject* object = owner->get<QoreObject>();
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
    args->push(owner->refSelf(), &xsink);
    assert(!xsink);
    RuntimeConfig* rc = qore_rt_get_fast_runtime_config();
    auto* old_object = rc->getObject();
    const auto* old_class = rc->getClass();
    auto call = [&](Scalar expected) {
        ValueHolder value(program->callFunction("zeroNativeCall", *args, &xsink), &xsink);
        assert(!xsink && value->getType() == (floating ? NT_FLOAT : NT_INT));
        if constexpr (floating) {
            assert(std::isnan(expected) ? std::isnan(value->getAsFloat()) : expected == value->getAsFloat());
        } else {
            assert(expected == value->getAsBigInt());
        }
        assert(rc->getObject() == old_object && rc->getClass() == old_class);
    };
    Scalar initial = floating ? static_cast<Scalar>(0.75) : static_cast<Scalar>(18);
    for (unsigned i = 0; i < 16; ++i) { call(initial); }
    const auto* method = object->getClass()->findMethod("read");
    assert(method);
    const auto* uvb = qore_method_private::get(*method)->getFunction()->first()->getUserVariantBase();
    assert(uvb && uvb->hasCachedAOT());
    QoreAOTContext* ctx = uvb->getCachedAOTContext();
    assert(ctx && ctx->uses_self && !ctx->uses_argv);
    const bool published = enabled("QORE_DISABLE_AOT_ZERO_ARG_GETTER")
        && enabled("QORE_DISABLE_AOT_SCALAR_MEMBER_ENTRY");
    assert(bool(Probe::binding(ctx).load(std::memory_order_acquire)) == published);
    if constexpr (floating) {
        assert(!ctx->scalar_getter_int_entry_v1.load(std::memory_order_acquire));
    } else {
        assert(!ctx->scalar_getter_float_entry_v1.load(std::memory_order_acquire));
    }
    const auto* function = reinterpret_cast<const QoreFunction*>(program->findFunction("zeroNativeCall"));
    assert(function);
    QoreAOTContext* caller = function->first()->getUserVariantBase()->getCachedAOTContext();
    assert(caller && caller->native_body_fn != ctx->native_body_fn);
    auto bind = [&](auto normal, typename Probe::Entry entry) {
        if constexpr (floating) { qore_rt_bind_getter_float_v1(ctx, normal, entry); }
        else { qore_rt_bind_getter_int_v1(ctx, normal, entry); }
    };
    Probe probe(ctx, object);
    if (published) {
        Probe::binding(ctx).store(nullptr, std::memory_order_release);
        bind(caller->native_body_fn, Probe::invoke);
        bind(nullptr, Probe::invoke);
        bind(ctx->native_body_fn, nullptr);
        assert(!Probe::binding(ctx).load(std::memory_order_acquire));
        bind(ctx->native_body_fn, Probe::invoke);
        assert(Probe::binding(ctx).load(std::memory_order_acquire) == Probe::invoke);
        bind(ctx->native_body_fn, probe.saved);
        assert(Probe::binding(ctx).load(std::memory_order_acquire) == Probe::invoke);
    }
    const bool native = published && enabled("QORE_DISABLE_AOT_NATIVE_METHOD_ENTRY")
        && enabled("QORE_DISABLE_AOT_NATIVE_METHOD_ADMISSION_CACHE")
        && enabled("QORE_DISABLE_AOT_GENERIC_METHOD_ADMISSION_CACHE")
        && enabled("QORE_DISABLE_AOT_SCALAR_PARAM_BINDING_CACHE")
        && enabled("QORE_DISABLE_AOT_POLYMORPHIC_DISPATCH_CACHE")
        && enabled("QORE_DISABLE_AOT_POLYMORPHIC_DISPATCH_FAST_ENTRY")
        && (!floating || (enabled("QORE_DISABLE_AOT_NATIVE_FLOAT_METHOD_ENTRY")
            && enabled("QORE_DISABLE_AOT_GENERIC_NATIVE_FLOAT_METHOD_ENTRY")));
    unsigned before = probe.calls;
    for (unsigned i = 0; i < 16; ++i) { call(initial); }
    assert(probe.calls - before == (native ? 16u : 0u));
    // A null/mismatched/unready live entry must not be bypassed by the PIC proof.
    if (published) {
        auto normal = ctx->native_body_fn;
        ctx->native_body_fn = caller->native_body_fn;
        before = probe.calls;
        call(initial);
        assert(probe.calls == before);
        ctx->native_body_fn = normal;
        Probe::binding(ctx).store(nullptr, std::memory_order_release);
        call(initial);
        assert(probe.calls == before);
        assert(Probe::binding(ctx).load(std::memory_order_acquire) == probe.saved);
        Probe::binding(ctx).store(Probe::invoke, std::memory_order_release);
        call(initial);
        assert(probe.calls - before == (native ? 1u : 0u));
    }
    // Probe failure must leave all observable work to the ordinary method.
    before = probe.misses;
    object->setValue("value", QoreValue(), &xsink);
    assert(!xsink);
    call(floating ? static_cast<Scalar>(0.25) : static_cast<Scalar>(1));
    assert(probe.misses - before == (native ? 1u : 0u));
    if constexpr (floating) {
        for (double value : {0.0, -0.0, 1.25, -9.0e307, std::numeric_limits<double>::infinity(),
                -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
            object->setValue("value", QoreValue(value), &xsink);
            assert(!xsink);
            call(value + 0.25);
        }
    } else {
        for (int64_t value : {int64_t{0}, int64_t{-1}, int64_t{1} << 47, -(int64_t{1} << 47) - 1,
                int64_t{1} << 50}) {
            object->setValue("value", QoreValue(value), &xsink);
            assert(!xsink);
            call(value + 1);
        }
    }
#if defined(__aarch64__) || defined(__x86_64__)
    if (native && qore_rt_get_stack_limit_v1(rc)) {
        before = probe.calls;
        probe.overflow = true;
        ValueHolder value(program->callFunction("zeroNativeCall", *args, &xsink), &xsink);
        probe.overflow = false;
        assert(probe.calls == before + 1 && xsink && value->isNothing());
        assert(xsink.getExceptionErr().get<const QoreStringNode>()->equal("STACK-LIMIT-EXCEEDED"));
        ReferenceHolder<QoreHashNode> info(xsink.getExceptionInfo(), &xsink);
        assert(!xsink && info);
        const auto* stack = info->getKeyValue("callstack").get<const QoreListNode>();
        assert(stack);
        bool seen = false;
        for (size_t i = 0; i < stack->size(); ++i) {
            const auto* frame = stack->retrieveEntry(i).get<const QoreHashNode>();
            assert(frame);
            const auto* name = frame->getKeyValue("function").get<const QoreStringNode>();
            seen |= name && name->equal(floating ? "ZeroNativeFloat::read" : "ZeroNativeInt::read");
            if (!(i % 100)) { assert(!qore_check_cancel(&xsink, "zero-argument getter stack inspection")); }
        }
        assert(seen && rc->getObject() == old_object && rc->getClass() == old_class);
    }
#endif
    // A separately compiled caller Program must not reuse same-Program admission.
    object->setValue("value", floating ? QoreValue(0.5) : QoreValue(int64_t{17}), &xsink);
    assert(!xsink);
    before = probe.calls;
    {
        QoreProgramHelper foreign(PO_MODERN | PO_USER_INHERITANCE_OPTIONS, xsink);
        qore_aot_script_begin_batch(*foreign);
        init_ZERO_ARG_GETTER_qo(*foreign);
        assert(!qore_aot_script_end_batch(*foreign));
        QoreExternalProgramContextHelper foreign_context(&xsink, *foreign);
        assert(!xsink);
        for (unsigned i = 0; i < 16; ++i) {
            ValueHolder value(foreign->callFunction("zeroNativeCall", *args, &xsink), &xsink);
            assert(!xsink && value->getType() == (floating ? NT_FLOAT : NT_INT));
            assert(floating ? value->getAsFloat() == 0.75 : value->getAsBigInt() == 18);
        }
        assert(probe.calls == before);
    }
    // Loading/tearing down another Program advances the global dispatch epoch.
    // Refill the ordinary PIC before asserting a subsequent same-Program hit.
    call(initial);
    before = probe.calls;
    call(initial);
    assert(probe.calls - before == (native ? 1u : 0u));
}

static void check() {
    ExceptionSink xsink;
    QoreProgramHelper program(PO_MODERN, xsink);
    qore_aot_script_begin_batch(*program);
    init_ZERO_ARG_GETTER_qo(*program);
    assert(!qore_aot_script_end_batch(*program));
    QoreExternalProgramContextHelper context(&xsink, *program);
    assert(!xsink);
    checkGetter<int64_t>(*program, "makeInt");
    checkGetter<double>(*program, "makeFloat");
}

int main() {
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    check();
    std::thread worker([]() { QoreForeignThreadHelper foreign; check(); });
    worker.join();
    qore_cleanup();
    std::puts("zero-arg-getter-native: ok");
}
