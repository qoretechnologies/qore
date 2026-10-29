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

extern "C" void init_NATIVE_LEAF_CONTEXT_qo(QoreProgram*);

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <string>
#include <thread>
#include <type_traits>

#if defined(__aarch64__)
#include <qore/macros-aarch64.h>
#elif defined(__x86_64__)
#include <qore/macros-x86_64.h>
#endif

// The compiler may seed sparse IR identities, then overwrite a former highest seed during specialization.
static void checkLocalSlots() {
    std::array<int, 8> keys{};
    AOTSlotMap slots;
    assert(slots.getLocalSlot(&keys[0]) == 0);
    assert(slots.getLocalSlot(&keys[0]) == 0);
    slots.local_slots[&keys[1]] = 17;
    assert(slots.refreshLocalSlotBound());
    assert(slots.getLocalSlot(&keys[2]) == 18);
    slots.local_slots[&keys[1]] = 1;
    slots.local_slots[&keys[2]] = 2;
    assert(slots.refreshLocalSlotBound());
    assert(slots.getLocalSlot(&keys[3]) == 3);
    AOTSlotMap copy = slots;
    assert(copy.getLocalSlot(&keys[4]) == 4);
    assert(!slots.hasLocalSlot(&keys[4]));
    slots.local_slots[&keys[5]] = -1;
    assert(!slots.refreshLocalSlotBound());
    slots.local_slots[&keys[5]] = INT32_MAX;
    assert(!slots.refreshLocalSlotBound());
    slots.local_slots[&keys[5]] = INT32_MAX - 2;
    assert(slots.refreshLocalSlotBound());
    assert(slots.getLocalSlot(&keys[6]) == INT32_MAX - 1);
    bool exhausted = false;
    try {
        slots.getLocalSlot(&keys[7]);
    } catch (const std::length_error&) {
        exhausted = true;
    }
    assert(exhausted && !slots.hasLocalSlot(&keys[7]));
    assert(slots.getLocalSlot(&keys[6]) == INT32_MAX - 1);
    slots.local_slots.clear();
    assert(slots.refreshLocalSlotBound());
    assert(slots.getLocalSlot(&keys[7]) == 0);
    std::array<int, 128> cancellation_keys{};
    for (size_t i = 0; i < cancellation_keys.size(); ++i) {
        slots.local_slots[&cancellation_keys[i]] = static_cast<int32_t>(i + 1);
    }
    assert(!qore_cancel_thread(q_gettid(), "AOT local slot bounds regression"));
    assert(!slots.refreshLocalSlotBound());
    qore_clear_thread_cancel();
    assert(slots.refreshLocalSlotBound());
    assert(slots.getLocalSlot(&keys[0]) == 129);
}

static uint64_t mismatchedNormalBody(QoreAOTContext*, ExceptionSink*) { return 0; }
static uint64_t mismatchedReceiverEntry(int64_t, uint64_t, RuntimeConfig*, QoreAOTContext*, ExceptionSink*) {
    return 0;
}

static uint64_t cancelledBackgroundBody(QoreAOTContext*, ExceptionSink*) {
    assert(false && "cancelled background preparation must not start a thread");
    return 0;
}

static void checkCancelledBackgroundPreparation(QoreAOTContext* ctx) {
    assert(ctx->num_locals > 0 && ctx->locals[0]);
    std::array<int32_t, 256> slots{};
    std::array<uint64_t, 256> values{};
    ExceptionSink xsink;
    assert(!qore_cancel_thread(q_gettid(), "native background preparation regression"));
    uint64_t bits = qore_rt_background_helper_aot(ctx, cancelledBackgroundBody, slots.data(),
        values.data(), static_cast<int>(values.size()), 0, &xsink);
    qore_clear_thread_cancel();
    QoreValue result;
    std::memcpy(&result, &bits, sizeof(result));
    assert(result.isNothing() && xsink);
    const auto* description = xsink.getExceptionErr().get<const QoreStringNode>();
    assert(description && !std::strcmp(description->c_str(), "THREAD-CANCELLED"));
    xsink.clear();
}

static void checkForwardPublication(QoreAOTContext* ctx) {
    // No Qore code runs while this private Program's binding is withdrawn. Descriptors outlive both publishers.
    const auto* original = ctx->receiver_int_forward_entry.exchange(nullptr, std::memory_order_acq_rel);
    assert(original && original->entry && original->loc);
    const auto normal = ctx->native_body_fn;
    auto absent = [&]() { assert(!ctx->receiver_int_forward_entry.load(std::memory_order_acquire)); };
    qore_rt_bind_receiver_int_method_v3(nullptr, normal, original);
    qore_rt_bind_receiver_int_method_v3(ctx, nullptr, original);
    qore_rt_bind_receiver_int_method_v3(ctx, mismatchedNormalBody, original);
    qore_rt_bind_receiver_int_method_v3(ctx, normal, nullptr);
    absent();
    const QoreAOTReceiverIntForwardEntryV3 invalid[] = {
        {nullptr, original->slot, original->loc},
        {original->entry, -1, original->loc},
        {original->entry, ctx->num_exprs, original->loc},
        {original->entry, original->slot, nullptr},
        {mismatchedReceiverEntry, original->slot, original->loc},
    };
    for (const auto& descriptor : invalid) {
        qore_rt_bind_receiver_int_method_v3(ctx, normal, &descriptor);
        absent();
    }
    auto entry = ctx->receiver_int_method_entry_v1.exchange(nullptr, std::memory_order_acq_rel);
    qore_rt_bind_receiver_int_method_v1(ctx, normal, entry, 1);
    assert(!ctx->receiver_int_method_entry_v1.load(std::memory_order_acquire));
    qore_rt_bind_receiver_int_method_v2(ctx, normal, entry, 0, original->slot, 0);
    assert(ctx->receiver_int_method_entry_v1.load(std::memory_order_acquire) == entry);
    absent();
    const QoreAOTReceiverIntForwardEntryV3 candidates[] = {*original, *original};
    std::atomic<bool> start{false};
    auto publish = [&](unsigned index) {
        start.wait(false, std::memory_order_acquire);
        for (unsigned n = 0; n < 64; ++n) {
            qore_rt_bind_receiver_int_method_v3(ctx, normal, &candidates[index]);
            const auto* observed = ctx->receiver_int_forward_entry.load(std::memory_order_acquire);
            assert(observed == &candidates[0] || observed == &candidates[1]);
            assert(observed->entry == entry && observed->slot == original->slot && observed->loc == original->loc);
        }
    };
    std::thread first(publish, 0), second(publish, 1);
    start.store(true, std::memory_order_release);
    start.notify_all();
    first.join();
    second.join();
    const auto* winner = ctx->receiver_int_forward_entry.load(std::memory_order_acquire);
    qore_rt_bind_receiver_int_method_v3(ctx, normal, original);
    assert(ctx->receiver_int_forward_entry.load(std::memory_order_acquire) == winner);
    ctx->receiver_int_forward_entry.store(original, std::memory_order_release);
}

struct LeafInvocation {
    uint64_t bits = 0;
    QorePreparedFunctionRefV3* cache = nullptr;
    QoreProgram* program = nullptr;
    bool inline_method = false;
};

struct ReusableCallbackFrame {
    QorePreparedFunctionRefFrameV4 slot{};
    ~ReusableCallbackFrame() {
        qore_rt_release_funcref_frame_v1(&slot);
        assert(!slot.frame);
        qore_rt_release_funcref_frame_v1(&slot);
        assert(!slot.frame);
    }
};

// Test-only adapter: the actual compiler-proven entry still executes, using a
// null view or a changed live limit after frame publication. Each test owns an unshared Program;
// the saved target belongs only to this thread and is restored before teardown.
static thread_local QoreAOTContext::NativeIntUnaryMethodEntryV3 method_stack_target_v3 = nullptr;
static thread_local QoreAOTContext::NativeIntUnaryMethodEntryV2 method_stack_target_v2 = nullptr;
static thread_local unsigned method_stack_calls = 0;
static thread_local bool method_null_view = false;
static thread_local bool method_force_overflow = false;
static thread_local QoreObject* method_expected_object = nullptr;
static int64_t nullViewMethod(int64_t arg, QoreAOTContext* ctx, ExceptionSink* xsink,
        RuntimeConfig* rc, const size_t* view) {
    ++method_stack_calls;
    struct RestoreLimit {
        size_t* limit;
        size_t saved;
        ~RestoreLimit() {
            if (limit) {
                *limit = saved;
            }
        }
    } restore{nullptr, 0};
    if (method_force_overflow) {
#if defined(__aarch64__) || defined(__x86_64__)
        restore.limit = const_cast<size_t*>(qore_rt_get_stack_limit_v1(rc));
        assert(restore.limit && (!method_stack_target_v3 || restore.limit == view));
        restore.saved = *restore.limit;
        *restore.limit = get_stack_pos() + 4096;
#else
        assert(false && "forced live limit requires a supported native stack view");
#endif
    }
    int64_t result = method_stack_target_v3
        ? method_stack_target_v3(arg, ctx, xsink, rc, method_null_view ? nullptr : view)
        : method_stack_target_v2(arg, ctx, xsink, rc);
    if (method_force_overflow) {
        assert(*xsink && method_expected_object);
        // Inspect only after the real callee's failing guard has materialized
        // context. Restore the artificial limit before a member access probe.
        *restore.limit = restore.saved;
        assert(rc->getObject() == method_expected_object);
        assert(rc->getClass() == qore_class_private::get(*method_expected_object->getClass()));
        ExceptionSink probe_sink;
        uint64_t bits = qore_rt_load_self_member("leafContextMarker", &probe_sink);
        QoreValue value;
        std::memcpy(&value, &bits, sizeof(value));
        ValueHolder marker(value, &probe_sink);
        // This reads the live object and checks private access using the live
        // defining class, independently of the RuntimeConfig snapshot above.
        assert(!probe_sink && marker->getAsBigInt() == 927);
    }
    return result;
}

static int64_t explicitStackMethod(int64_t arg, QoreAOTContext* ctx, ExceptionSink* xsink, RuntimeConfig* rc) {
    return nullViewMethod(arg, ctx, xsink, rc, nullptr);
}

class MethodStackProbe {
public:
    MethodStackProbe(QoreAOTContext* ctx, bool null_view, bool overflow = false,
            QoreObject* expected_object = nullptr)
            : ctx(null_view || overflow ? ctx : nullptr) {
        if (this->ctx) {
            assert(!method_stack_target_v3 && !method_stack_target_v2);
            method_stack_target_v3 = ctx->native_int_unary_method_entry_v3.load(std::memory_order_acquire);
            assert(method_stack_target_v3 != nullViewMethod);
            method_stack_calls = 0;
            method_null_view = null_view;
            method_force_overflow = overflow;
            method_expected_object = expected_object;
            if (method_stack_target_v3) {
                ctx->native_int_unary_method_entry_v3.store(nullViewMethod, std::memory_order_release);
            } else {
                assert(overflow && !null_view);
                method_stack_target_v2 = ctx->native_int_unary_method_entry_v2.load(std::memory_order_acquire);
                assert(method_stack_target_v2 && method_stack_target_v2 != explicitStackMethod);
                ctx->native_int_unary_method_entry_v2.store(explicitStackMethod, std::memory_order_release);
            }
        }
    }
    ~MethodStackProbe() {
        if (ctx) {
            assert(method_stack_calls);
            if (method_stack_target_v3) {
                ctx->native_int_unary_method_entry_v3.store(method_stack_target_v3, std::memory_order_release);
                method_stack_target_v3 = nullptr;
            } else {
                ctx->native_int_unary_method_entry_v2.store(method_stack_target_v2, std::memory_order_release);
                method_stack_target_v2 = nullptr;
            }
            method_expected_object = nullptr;
        }
    }
private:
    QoreAOTContext* ctx;
};

// Count execution of the ordinary receiver companion in this unshared Program.
// A preflight hit bypasses it; a deliberately unpublished proof must execute it.
class ForwardEntryProbe {
public:
    explicit ForwardEntryProbe(QoreAOTContext* ctx) : ctx(ctx),
            saved(ctx->receiver_int_method_entry_v1.load(std::memory_order_acquire)) {
        assert(saved && !current);
        current = this;
        ctx->receiver_int_method_entry_v1.store(invoke, std::memory_order_release);
    }
    ~ForwardEntryProbe() {
        ctx->receiver_int_method_entry_v1.store(saved, std::memory_order_release);
        current = nullptr;
    }
    unsigned calls = 0;
private:
    static thread_local ForwardEntryProbe* current;
    QoreAOTContext* ctx;
    QoreAOTContext::ReceiverIntMethodEntryV1 saved;
    static uint64_t invoke(int64_t arg, uint64_t receiver, RuntimeConfig* rc,
            QoreAOTContext* ctx, ExceptionSink* xsink) {
        assert(current);
        ++current->calls;
        return current->saved(arg, receiver, rc, ctx, xsink);
    }
};
thread_local ForwardEntryProbe* ForwardEntryProbe::current = nullptr;

struct FloatMethodProbeState {
    QoreAOTContext::NativeFloatUnaryMethodEntryV3 target;
    QoreObject* object;
    bool null_view;
    bool overflow;
    unsigned calls = 0;
};
static thread_local FloatMethodProbeState* float_method_probe = nullptr;

static double floatMethodStackAdapter(double arg, QoreAOTContext* ctx, ExceptionSink* xsink,
        RuntimeConfig* rc, const size_t* view) {
    assert(float_method_probe);
    auto& probe = *float_method_probe;
    ++probe.calls;
    struct RestoreLimit {
        size_t* limit = nullptr;
        size_t saved = 0;
        ~RestoreLimit() {
            if (limit) {
                *limit = saved;
            }
        }
    } restore;
    if (probe.overflow) {
#if defined(__aarch64__) || defined(__x86_64__)
        restore.limit = const_cast<size_t*>(qore_rt_get_stack_limit_v1(rc));
        assert(restore.limit && restore.limit == view);
        restore.saved = *restore.limit;
        *restore.limit = get_stack_pos() + 4096;
#else
        assert(false && "forced float method limit requires native stack view");
#endif
    }
    double result = probe.target(arg, ctx, xsink, rc, probe.null_view ? nullptr : view);
    if (probe.overflow) {
        assert(*xsink && result == 0.0 && probe.object);
        *restore.limit = restore.saved;
        assert(rc->getObject() == probe.object);
        assert(rc->getClass() == qore_class_private::get(*probe.object->getClass()));
        ExceptionSink probe_sink;
        uint64_t bits = qore_rt_load_self_member("leafContextMarker", &probe_sink);
        QoreValue value;
        std::memcpy(&value, &bits, sizeof(value));
        ValueHolder marker(value, &probe_sink);
        assert(!probe_sink && marker->getAsBigInt() == 927);
    }
    return result;
}

// Test-only replacement of one unshared Program's binding. The real compiler
// companion still runs and the original entry is restored before state dies.
class FloatMethodStackProbe {
public:
    FloatMethodStackProbe(QoreAOTContext* ctx, QoreObject* object, bool null_view, bool overflow)
        : ctx(ctx), state{ctx->native_float_unary_method_entry_v3.load(std::memory_order_acquire),
            object, null_view, overflow} {
        assert(state.target && !float_method_probe);
        float_method_probe = &state;
        ctx->native_float_unary_method_entry_v3.store(floatMethodStackAdapter, std::memory_order_release);
    }
    ~FloatMethodStackProbe() {
        assert(state.calls);
        ctx->native_float_unary_method_entry_v3.store(state.target, std::memory_order_release);
        float_method_probe = nullptr;
    }
private:
    QoreAOTContext* ctx;
    FloatMethodProbeState state;
};

// Rejected signatures and NOTHING must neither populate an empty slot nor
// invalidate its existing valid target. Every slot still belongs to one ABI.
template <typename Call, typename Arg, typename Result>
static void checkPreparedMisses(QoreProgram* program, const char* other_factory, Call call,
        uint64_t valid_bits, QorePreparedFunctionRefV3& cache, RuntimeConfig* rc,
        Arg arg, Result expected, uint32_t signature, ExceptionSink* xsink, bool invariant = false) {
    ValueHolder other(program->callFunction(other_factory, nullptr, xsink), xsink);
    assert(!*xsink && other->getType() == NT_FUNCREF);
    const auto* node = static_cast<const FunctionCallReferenceNode*>(other->get<const ResolvedCallReferenceNode>());
    assert(node->getProgram() == cache.pgm && !node->getTypedEntry(signature));
    uint64_t other_bits;
    std::memcpy(&other_bits, &*other, sizeof(other_bits));
    QoreValue empty;
    uint64_t empty_bits;
    std::memcpy(&empty_bits, &empty, sizeof(empty_bits));
    const auto* saved_ref = cache.ref;
    auto check_invalid = [&](uint64_t invalid) {
        QorePreparedFunctionRefV3 cold{};
        cold.owner = empty_bits;
        Result result = 123;
        assert(call(invalid, arg, &result, xsink, rc, &cold) == 0);
        assert(!*xsink && result == 123 && cold.owner == empty_bits);
        // V9's compiler contract excludes changing a live descriptor's reference.
        // Its invalid-reference cases use a fresh, empty descriptor above.
        if (!invariant) {
            assert(call(invalid, arg, &result, xsink, rc, &cache) == 0);
            assert(!*xsink && result == 123 && cache.owner == valid_bits && cache.ref == saved_ref);
        }
        assert(call(valid_bits, arg, &result, xsink, rc, &cache) == 1);
        assert(!*xsink && result == expected && cache.owner == valid_bits && cache.ref == saved_ref);
    };
    check_invalid(other_bits);
    check_invalid(empty_bits);
    // A miss must preserve both the result and any retained target for every
    // value representation, without treating an inline payload as a node.
    for (QoreValue value : {QoreValue::makeNull(), QoreValue(true), QoreValue(int64_t{7}), QoreValue(1.25),
            QoreValue::makeChar('x'), QoreValue::makeShortString("short", 5)}) {
        check_invalid(value.rawBits());
    }
    ValueHolder boxed_int(QoreValue(INT64_MAX), xsink);
    ValueHolder boxed_float(QoreValue(-3.0e307), xsink);
    assert(boxed_int->isPointer() && boxed_float->isPointer());
    check_invalid(boxed_int->rawBits());
    check_invalid(boxed_float->rawBits());
    ValueHolder long_string(new QoreStringNode("not a callback reference"), xsink);
    check_invalid(long_string->rawBits());
    ValueHolder enumeration(program->callFunction("makeEnumProbe", nullptr, xsink), xsink);
    assert(!*xsink && enumeration->isEnum());
    check_invalid(enumeration->rawBits());
    ValueHolder object(program->callFunction("makeMethodProbe", nullptr, xsink), xsink);
    assert(!*xsink && object->getType() == NT_OBJECT);
    check_invalid(object->rawBits());
    ValueHolder opaque(QoreValue::makeOpaqueObject(object->get<QoreObject>()), xsink);
    assert(opaque->isOpaqueObject() && opaque->isPointer());
    check_invalid(opaque->rawBits());
}

// Invoked by parsed callers whose owned locals/closures are on the TLS stacks.
// The leaf remains an actually bound native entry from the script aggregate.
static QoreValue paddedSweep(const void* ptr, const QoreListNode*, RuntimeConfig&, ExceptionSink* xsink) {
    const auto& invocation = *static_cast<const LeafInvocation*>(ptr);
    ReusableCallbackFrame reusable;
    RuntimeConfig* rc = qore_rt_get_fast_runtime_config();
    int64_t result = 0;
    QoreProgram* caller = qore_get_call_program_context();
    size_t* limit = const_cast<size_t*>(qore_rt_get_stack_limit_v1(rc));
#if defined(__aarch64__) || defined(__x86_64__)
    if (limit) {
        const size_t saved_limit = *limit;
        unsigned published = 0;
        for (unsigned gap = 0; gap < 4096; gap += 16) {
            *limit = get_stack_pos() - gap;
            int handled = qore_rt_try_funcref_int_int_v4(invocation.bits, 7, &result, xsink, rc, invocation.cache,
                &reusable.slot);
            *limit = saved_limit;
            assert(handled == 1 && qore_get_call_program_context() == caller);
            if (*xsink) {
                assert(xsink->getExceptionErr().get<const QoreStringNode>()->equal("STACK-LIMIT-EXCEEDED"));
                ReferenceHolder<QoreHashNode> info(xsink->getExceptionInfo(), xsink);
                assert(!*xsink);
                const auto* stack = info->getKeyValue("callstack").get<const QoreListNode>();
                assert(stack);
                for (size_t i = 0; i < stack->size(); ++i) {
                    const auto* frame = stack->retrieveEntry(i).get<const QoreHashNode>();
                    assert(frame);
                    const auto* name = frame->getKeyValue("function").get<const QoreStringNode>();
                    if (name && std::strstr(name->c_str(), "nativeLeafProbe")) {
                        ++published;
                    }
                    if (!(i % 100)) {
                        assert(!qore_check_cancel(xsink, "padded native leaf stack inspection"));
                    }
                }
            } else {
                assert(result == 24);
            }
            if (!(gap % 160)) {
                assert(!qore_check_cancel(xsink, "padded native leaf guard sweep"));
            }
        }
        assert(published);
    }
#else
    assert(!limit);
#endif
    assert(qore_rt_try_funcref_int_int_v4(invocation.bits, 7, &result, xsink, rc, invocation.cache,
        &reusable.slot) == 1);
    assert(!*xsink && result == 24);
    return QoreValue(result);
}

// Find the single method PIC populated by an actual compiled probe caller.
static QoreAOTContext* getMethodProbeContext(QoreProgram* program, const char* caller_name, int& slot) {
    ExceptionSink xsink;
    // QoreExternalFunction is the public view of QoreFunction (QoreReflection.cpp).
    // Use the exported lookup; internal namespace lookups are not exported.
    const auto* function = reinterpret_cast<const QoreFunction*>(program->findFunction(caller_name));
    assert(function);
    const UserVariantBase* uvb = function->first()->getUserVariantBase();
    assert(uvb && uvb->hasCachedAOT());
    QoreAOTContext* ctx = uvb->getCachedAOTContext();
    assert(ctx);
    slot = -1;
    for (int i = 0; i < ctx->num_exprs; ++i) {
        if (ctx->call_targets[i].method_dispatch_cache.load(std::memory_order_acquire)) {
            assert(slot == -1);
            slot = i;
        }
        if (!(i % 100)) {
            assert(!qore_check_cancel(&xsink, "native method receiver slots"));
        }
    }
    assert(slot >= 0);
    return ctx;
}

// Keep private forwarding on its own two-receiver site: a saturated PIC can
// return the right value through fallback without testing native class context.
static void checkPrivateForwardReceivers(QoreProgram* program) {
    ExceptionSink xsink;
    ValueHolder base(program->callFunction("makePrivateBaseProbe", nullptr, &xsink), &xsink);
    assert(!xsink && base->getType() == NT_OBJECT);
    const auto* defining_class = qore_class_private::get(*base->get<QoreObject>()->getClass());
    const bool accepted = !std::getenv("QORE_DISABLE_AOT_NATIVE_FORWARD_LEAF")
        && !std::getenv("QORE_DISABLE_AOT_FORWARD_LEAF_PREFLIGHT")
        && !std::getenv("QORE_DISABLE_AOT_NATIVE_METHOD_DEFERRED_CONTEXT")
        && !std::getenv("QORE_DISABLE_AOT_EARLY_NATIVE_METHOD_ADMISSION");
    RuntimeConfig* rc = qore_rt_get_fast_runtime_config();
    QoreProgram* caller = qore_get_call_program_context();
    QoreObject* old_object = rc->getObject();
    const qore_class_private* old_class = rc->getClass();
    for (const char* factory : {"makePrivateBaseProbe", "makePrivateDerivedProbe"}) {
        ValueHolder object(program->callFunction(factory, nullptr, &xsink), &xsink);
        assert(!xsink && object->getType() == NT_OBJECT);
        ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
        args->push(object->refSelf(), &xsink);
        assert(!xsink);
        args->push(QoreValue(int64_t{7}), &xsink);
        assert(!xsink);
        for (unsigned i = 0; i < 16; ++i) {
            ValueHolder result(program->callFunction("nativePrivateForwardMethodProbe", *args, &xsink), &xsink);
            assert(!xsink && result->getAsBigInt() == 24);
        }
        int slot;
        QoreAOTContext* ctx = getMethodProbeContext(program, "nativePrivateForwardMethodProbe", slot);
        auto* cache = ctx->call_targets[slot].method_dispatch_cache.load(std::memory_order_acquire);
        assert(cache);
        bool found = false;
        for (auto& cache_slot : cache->entries) {
            const auto* entry = cache_slot.load(std::memory_order_acquire);
            if (entry && entry->receiver_class == object->get<QoreObject>()->getClass()) {
                found = true;
                if (accepted) {
                    assert(entry->native_scalar_program == program);
                    assert(entry->native_scalar_class_ctx == defining_class);
                    assert(entry->native_scalar_dispatch_class_ctx == defining_class);
                }
            }
        }
        assert(found);
        // A derived private method does not override the base's private call.
        // Require actual native handling, including full-width integer inputs,
        // and unchanged output/context for disabled-path misses.
        for (int64_t value : {int64_t{7}, INT64_MIN, INT64_MAX - 17, int64_t{1} << 50}) {
            int64_t result = -123;
            assert(qore_rt_try_method_int_unary_v1(ctx, slot, object->rawBits(), value,
                &result, &xsink, rc) == static_cast<int>(accepted));
            assert(!xsink && result == (accepted ? value + 17 : -123));
            assert(qore_get_call_program_context() == caller);
            assert(rc->getObject() == old_object && rc->getClass() == old_class);
        }
    }
}

// A pending exception or thread exit must prevent both scalar ABI variants from
// entering the warmed native target, without touching the boxed fallback result.
template <typename Scalar>
static void checkPendingMethodSink(QoreAOTContext* ctx, int slot, QoreValue receiver, Scalar arg,
        RuntimeConfig* rc) {
    enum class Pending { Exception, ThreadExit, Both };
    QoreProgram* caller = qore_get_call_program_context();
    QoreObject* object = rc->getObject();
    const auto* cls = rc->getClass();
    for (Pending pending : {Pending::Exception, Pending::ThreadExit, Pending::Both}) {
        for (bool fallback_abi : {false, true}) {
            ExceptionSink xsink;
            if (pending != Pending::ThreadExit) {
                xsink.raiseException("NATIVE-METHOD-PENDING", "pre-existing method exception");
            }
            if (pending != Pending::Exception) {
                xsink.raiseThreadExit();
            }
            Scalar result = -123;
            const uint64_t sentinel = QoreValue(true).rawBits();
            uint64_t boxed = sentinel;
            int handled;
            if constexpr (std::is_same_v<Scalar, double>) {
                handled = fallback_abi
                    ? qore_rt_try_method_float_unary_v2(ctx, slot, receiver.rawBits(), arg, &result,
                        &boxed, &xsink, rc)
                    : qore_rt_try_method_float_unary_v1(ctx, slot, receiver.rawBits(), arg, &result, &xsink, rc);
            } else {
                handled = fallback_abi
                    ? qore_rt_try_method_int_unary_v2(ctx, slot, receiver.rawBits(), arg, &result,
                        &boxed, &xsink, rc)
                    : qore_rt_try_method_int_unary_v1(ctx, slot, receiver.rawBits(), arg, &result, &xsink, rc);
            }
            assert(handled == 1 && result == 0 && boxed == sentinel);
            assert(xsink.isException() == (pending != Pending::ThreadExit));
            assert(xsink.isThreadExit() == (pending != Pending::Exception));
            if (xsink.isException()) {
                assert(xsink.getExceptionErr().get<const QoreStringNode>()->equal("NATIVE-METHOD-PENDING"));
                assert(xsink.getExceptionDesc().get<const QoreStringNode>()->equal("pre-existing method exception"));
            }
            assert(qore_get_call_program_context() == caller);
            assert(rc->getObject() == object && rc->getClass() == cls);
            xsink.clear();
        }
    }
}

// Exercise the receiver gate itself after the ordinary caller has populated its
// real PIC. Successful strong/opaque calls must actually use the continuation;
// rejected representations must leave result, exception and context untouched.
static void checkMethodReceivers(QoreProgram* program, const char* caller_name) {
    ExceptionSink xsink;
    ValueHolder object(program->callFunction("makeMethodProbe", nullptr, &xsink), &xsink);
    assert(!xsink && object->getType() == NT_OBJECT);
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
    args->push(object->refSelf(), &xsink);
    assert(!xsink);
    args->push(QoreValue(int64_t{7}), &xsink);
    assert(!xsink);
    for (unsigned i = 0; i < 16; ++i) {
        ValueHolder result(program->callFunction(caller_name, *args, &xsink), &xsink);
        assert(!xsink && result->getAsBigInt() == 24);
    }
    int slot;
    QoreAOTContext* ctx = getMethodProbeContext(program, caller_name, slot);
    RuntimeConfig* rc = qore_rt_get_fast_runtime_config();
    QoreProgram* caller = qore_get_call_program_context();
    QoreObject* old_object = rc->getObject();
    const qore_class_private* old_class = rc->getClass();
    const auto* retained_status = static_cast<const std::atomic<int>*>(
        qore_rt_get_object_status_view_v1(object->rawBits()));
    auto check_exact = [&](QoreValue receiver) {
        const QoreObject* obj = receiver.getType() == NT_OBJECT ? receiver.get<const QoreObject>() : nullptr;
        const auto* status_view = static_cast<const std::atomic<int>*>(
            qore_rt_get_object_status_view_v1(receiver.rawBits()));
        bool supported = sizeof(int) == 4 && sizeof(std::atomic<int>) == 4
            && alignof(std::atomic<int>) >= 4 && std::atomic<int>::is_always_lock_free
            && !std::getenv("QORE_DISABLE_AOT_INLINE_RECEIVER_VALIDITY");
        assert(static_cast<bool>(status_view) == (supported && obj));
        if (status_view) {
            assert((status_view->load(std::memory_order_seq_cst) != -1) == obj->isValid());
        }
        // Name-method sites need not carry a fixed class. This thread owns the
        // unshared Program: install a test target only for these non-calling
        // predicates, then restore it before invoking the real continuation.
        const QoreClass* saved = ctx->call_targets[slot].qc;
        for (const QoreClass* target : {saved, object->get<QoreObject>()->getClass()}) {
            ctx->call_targets[slot].qc = target;
            bool exact = obj && target && obj->getClass() == target;
            assert(qore_rt_object_has_exact_aot_target_class_only(ctx, slot, receiver.rawBits())
                == static_cast<int>(exact));
            assert(qore_rt_object_has_exact_aot_target_class(ctx, slot, receiver.rawBits())
                == static_cast<int>(exact && obj->isValid()));
        }
        ctx->call_targets[slot].qc = saved;
        assert(!qore_rt_object_has_exact_aot_target_class(nullptr, slot, receiver.rawBits()));
        assert(!qore_rt_object_has_exact_aot_target_class_only(nullptr, slot, receiver.rawBits()));
        assert(!qore_rt_object_has_exact_aot_target_class(ctx, -1, receiver.rawBits()));
        assert(!qore_rt_object_has_exact_aot_target_class_only(ctx, ctx->num_exprs, receiver.rawBits()));
    };
    auto check = [&](QoreValue receiver, bool accepted) {
        // The exact-class caller's independent validity guard must agree with
        // generic value dispatch, including opaque objects and weak wrappers.
        bool valid = receiver.getType() == NT_OBJECT && receiver.get<QoreObject>()->isValid();
        assert(qore_rt_object_is_valid(receiver.rawBits()) == static_cast<int>(valid));
        check_exact(receiver);
        int64_t result = -123;
        assert(qore_rt_try_method_int_unary_v1(ctx, slot, receiver.rawBits(), 7, &result, &xsink, rc)
            == static_cast<int>(accepted));
        assert(!xsink && result == (accepted ? 24 : -123));
        assert(qore_get_call_program_context() == caller);
        assert(rc->getObject() == old_object && rc->getClass() == old_class);
    };
    if (!std::strcmp(caller_name, "nativeForwardMethodProbe")) {
        if (std::getenv("QORE_DISABLE_AOT_NATIVE_FORWARD_LEAF")
                || std::getenv("QORE_DISABLE_AOT_FORWARD_LEAF_PREFLIGHT")
                || std::getenv("QORE_DISABLE_AOT_NATIVE_METHOD_DEFERRED_CONTEXT")
                || std::getenv("QORE_DISABLE_AOT_EARLY_NATIVE_METHOD_ADMISSION")) {
            check(*object, false);
            return;
        }
        // Native int64_t arguments need no immediate-value box. Exercise both
        // sides of that representation boundary through the real warmed PIC.
        for (int64_t value : {INT64_MIN, INT64_MAX - 17, int64_t{1} << 50}) {
            int64_t result = -123;
            assert(qore_rt_try_method_int_unary_v1(ctx, slot, object->rawBits(), value,
                &result, &xsink, rc) == 1);
            assert(!xsink && result == value + 17);
            assert(qore_get_call_program_context() == caller);
            assert(rc->getObject() == old_object && rc->getClass() == old_class);
        }
        const auto* next = object->get<QoreObject>()->getClass()->findMethod("next");
        assert(next);
        auto* leaf_ctx = qore_method_private::get(*next)->getFunction()->first()->getUserVariantBase()
            ->getCachedAOTContext();
        assert(leaf_ctx);
        if (leaf_ctx->native_int_unary_method_entry_v3.load(std::memory_order_acquire)
                && qore_rt_get_stack_limit_v1(rc)) {
            ValueHolder snapshot(program->callFunction("makeBaseMethodProbe", nullptr, &xsink), &xsink);
            assert(!xsink && snapshot->getType() == NT_OBJECT);
            const auto* snapshot_class = qore_class_private::get(*snapshot->get<QoreObject>()->getClass());
            for (bool null_view : {false, true}) {
                // Both logical frames share the supplied configuration. Each
                // must restore its own layer, not replace the caller's distinct
                // legacy snapshot with its live object/class context.
                rc->setObject(snapshot->get<QoreObject>());
                rc->setClass(snapshot_class);
                {
                    MethodStackProbe forced(leaf_ctx, null_view, true, object->get<QoreObject>());
                    int64_t result = -123;
                    assert(qore_rt_try_method_int_unary_v1(ctx, slot, object->rawBits(), 7,
                        &result, &xsink, rc) == 1);
                    assert(xsink && result == 0 && qore_get_call_program_context() == caller);
                    assert(rc->getObject() == snapshot->get<QoreObject>() && rc->getClass() == snapshot_class);
                }
                rc->setObject(old_object);
                rc->setClass(old_class);
                assert(xsink.getExceptionErr().get<const QoreStringNode>()->equal("STACK-LIMIT-EXCEEDED"));
                ReferenceHolder<QoreHashNode> info(xsink.getExceptionInfo(), &xsink);
                assert(!xsink && info);
            }
        }
    }
    check(*object, true);
    ValueHolder opaque(QoreValue::makeOpaqueObject(object->get<QoreObject>()), &xsink);
    assert(opaque->isOpaqueObject() && opaque->isPointer());
    check(*opaque, true);
    if (std::strcmp(caller_name, "nativeForwardMethodProbe")) {
        checkPendingMethodSink(ctx, slot, *object, int64_t{7}, rc);
        checkPendingMethodSink(ctx, slot, *opaque, int64_t{7}, rc);
    }
    uint64_t weak_bits = qore_rt_make_weak_value(object->rawBits(), &xsink);
    assert(!xsink);
    QoreValue weak_value;
    std::memcpy(&weak_value, &weak_bits, sizeof(weak_value));
    ValueHolder weak(weak_value, &xsink);
    assert(weak->isPointer() && weak->getType() != NT_OBJECT);
    check(*weak, false);
    check(QoreValue(), false);
    check(QoreValue::makeNull(), false);
    check(QoreValue(true), false);
    check(QoreValue(int64_t{7}), false);
    check(QoreValue(1.25), false);
    check(QoreValue::makeChar('x'), false);
    check(QoreValue::makeShortString("short", 5), false);
    ValueHolder boxed_int(QoreValue(INT64_MAX), &xsink);
    assert(boxed_int->isPointer());
    check(*boxed_int, false);
    ValueHolder boxed_float(QoreValue(-3.0e307), &xsink);
    assert(boxed_float->isPointer());
    check(*boxed_float, false);
    ValueHolder long_string(new QoreStringNode("not a method receiver"), &xsink);
    check(*long_string, false);
    ValueHolder list(new QoreListNode(autoTypeInfo), &xsink);
    check(*list, false);
    ValueHolder hash(new QoreHashNode(autoTypeInfo), &xsink);
    check(*hash, false);
    ValueHolder enumeration(program->callFunction("makeEnumProbe", nullptr, &xsink), &xsink);
    assert(!xsink && enumeration->isEnum());
    check(*enumeration, false);
    check(*object, true);
    // Warm a second receiver class at the same call site, then verify that
    // subsequent calls still select the original receiver's override.
    ValueHolder replacement(program->callFunction("makeBaseMethodProbe", nullptr, &xsink), &xsink);
    assert(!xsink && replacement->getType() == NT_OBJECT);
    check_exact(*replacement);
    ValueHolder opaque_replacement(QoreValue::makeOpaqueObject(replacement->get<QoreObject>()), &xsink);
    check_exact(*opaque_replacement);
    ReferenceHolder<QoreListNode> replacement_args(new QoreListNode(autoTypeInfo), &xsink);
    replacement_args->push(replacement->refSelf(), &xsink);
    assert(!xsink);
    replacement_args->push(QoreValue(int64_t{7}), &xsink);
    assert(!xsink);
    ValueHolder replacement_result(program->callFunction(caller_name, *replacement_args, &xsink), &xsink);
    assert(!xsink && replacement_result->getAsBigInt() == 14);
    check(*object, true);
    auto* cache = ctx->call_targets[slot].method_dispatch_cache.load(std::memory_order_acquire);
    assert(cache);
    // Both fixture receivers inherit the admitted method's defining class;
    // their normalized nested-dispatch context must retain that exact class.
    // Do not replace the separate context used by cold frame restoration.
    for (auto& cache_slot : cache->entries) {
        const auto* entry = cache_slot.load(std::memory_order_acquire);
        if (entry && entry->native_scalar_program) {
            assert(entry->native_scalar_class_ctx);
            assert(entry->native_scalar_dispatch_class_ctx == entry->native_scalar_class_ctx);
        }
    }
    const auto* admitted = cache->entries[0].load(std::memory_order_acquire);
    assert(admitted && admitted->native_scalar_uvb);
    auto* callee = admitted->native_scalar_uvb->getCachedAOTContext();
    // Exact body identity remains live even after a successful lookup.
    check(*object, true);
    auto normal = callee->native_body_fn;
    callee->native_body_fn = nullptr;
    check(*object, false);
    callee->native_body_fn = normal;
    check(*object, true);
    // Publishing a real class invalidates the old dispatch epoch. The
    // continuation must miss until the ordinary path establishes a fresh key.
    {
        QoreProgramHelper mutation(PO_MODERN, xsink);
        ProgramRuntimeExternalParseContextHelper parse_context(*mutation);
        assert(parse_context);
        mutation->parse("%modern\nclass MethodEpochProbe { int probe() { return 1; } }\n",
            "native-method-epoch", &xsink);
        assert(!xsink);
    }
    check(*object, false);
    ValueHolder rewarmed(program->callFunction(caller_name, *args, &xsink), &xsink);
    assert(!xsink && rewarmed->getAsBigInt() == 24);
    check(*object, true);
    // C++ hierarchy construction must invalidate the same key even without
    // adding a method. These isolated classes are not exposed to other threads.
    // Warm before each mutation so construction alone cannot satisfy the test.
    for (unsigned kind = 0; kind < 3; ++kind) {
        QoreClassHolder base_owner(new QoreClass("NativeEpochBase", "::NativeEpochBase"));
        QoreClassHolder child_owner(new QoreClass("NativeEpochChild", "::NativeEpochChild"));
        QoreClass* parent = base_owner;
        QoreClass* child = child_owner;
        ValueHolder ready(program->callFunction(caller_name, *args, &xsink), &xsink);
        assert(!xsink && ready->getAsBigInt() == 24);
        check(*object, true);
        if (kind == 0) {
            child->addBuiltinBaseClass(parent);
        } else if (kind == 1) {
            child->addBaseClass(parent, false);
        } else {
            child->addBaseClass(parent, Private, false);
        }
        check(*object, false);
        ValueHolder rebuilt(program->callFunction(caller_name, *args, &xsink), &xsink);
        assert(!xsink && rebuilt->getAsBigInt() == 24);
        check(*object, true);
    }
    // The guard must reread deletion status, even for an unchanged receiver
    // still owned by both ordinary and opaque references.
    object->get<QoreObject>()->doDelete(&xsink);
    assert(!xsink);
    assert(!retained_status || retained_status->load(std::memory_order_seq_cst) == -1);
    check(*object, false);
    check(*opaque, false);
    check(*weak, false);
    replacement->get<QoreObject>()->doDelete(&xsink);
    assert(!xsink);
    check_exact(*replacement);
    check_exact(*opaque_replacement);
}

// Check the immutable key against the supplied argument, not the callee's
// signature: coercion can put an integer key on a float(float) method.
static void checkNativeScalarKey(const QoreAOTMethodDispatchCacheEntry& entry, QoreProgram* program) {
    uintptr_t expected = 0;
    if (entry.native_scalar_uvb && !std::getenv("QORE_DISABLE_AOT_NATIVE_SCALAR_KEY")
            && !entry.arg_states[0]
            && (entry.arg_types[0] == bigIntTypeInfo || entry.arg_types[0] == floatTypeInfo)) {
        assert(entry.nargs == 1 && entry.native_scalar_program == program);
        expected = reinterpret_cast<uintptr_t>(program);
        assert(expected && !(expected & 1));
        expected |= static_cast<uintptr_t>(entry.arg_types[0] == floatTypeInfo);
    }
    assert(entry.native_scalar_match_key == expected);
}

// An int-backed enum can resolve to the same int(int) method, but its dispatch
// argument key is not the native helper's integer key. First require a miss with
// only that enum entry, then admission after a later integer entry is published.
// The enum stays in the first PIC slot, exercising lookup past a nonmatching key.
static void checkMethodArgumentKeys(QoreProgram* program, const char* caller_name) {
    ExceptionSink xsink;
    ValueHolder object(program->callFunction("makeMethodProbe", nullptr, &xsink), &xsink);
    assert(!xsink && object->getType() == NT_OBJECT);
    ValueHolder enumeration(program->callFunction("makeEnumProbe", nullptr, &xsink), &xsink);
    assert(!xsink && enumeration->isEnum() && enumeration->getTypeInfo() != bigIntTypeInfo);
    auto warm = [&](QoreValue value) {
        ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
        args->push(object->refSelf(), &xsink);
        assert(!xsink);
        args->push(value.refSelf(), &xsink);
        assert(!xsink);
        for (unsigned i = 0; i < 16; ++i) {
            ValueHolder result(program->callFunction(caller_name, *args, &xsink), &xsink);
            assert(!xsink && result->getAsBigInt() == 24);
        }
    };
    warm(*enumeration);
    int slot;
    QoreAOTContext* ctx = getMethodProbeContext(program, caller_name, slot);
    auto* cache = ctx->call_targets[slot].method_dispatch_cache.load(std::memory_order_acquire);
    const auto* first = cache->entries[0].load(std::memory_order_acquire);
    assert(first && first->nargs == 1 && first->arg_types[0] == enumeration->getTypeInfo()
        && first->arg_states[0] == 0);
    checkNativeScalarKey(*first, program);
    assert(!first->native_scalar_match_key);
    assert(!cache->entries[1].load(std::memory_order_acquire));
    RuntimeConfig* rc = qore_rt_get_fast_runtime_config();
    QoreProgram* caller = qore_get_call_program_context();
    QoreObject* old_object = rc->getObject();
    const qore_class_private* old_class = rc->getClass();
    auto check = [&](bool accepted) {
        // Deliberately make snapshot and live caller context different. Native
        // admission must use the live key and restore the independent snapshot,
        // on both an enum-key miss and a successful integer continuation.
        QoreObject* snapshot_object = object->get<QoreObject>();
        const auto* snapshot_class = qore_class_private::get(*snapshot_object->getClass());
        rc->setObject(snapshot_object);
        rc->setClass(snapshot_class);
        int64_t result = -123;
        assert(qore_rt_try_method_int_unary_v1(ctx, slot, object->rawBits(), 7, &result, &xsink, rc)
            == static_cast<int>(accepted));
        assert(!xsink && result == (accepted ? 24 : -123));
        assert(qore_get_call_program_context() == caller);
        assert(rc->getObject() == snapshot_object && rc->getClass() == snapshot_class);
        rc->setObject(old_object);
        rc->setClass(old_class);
    };
    check(false);
    warm(QoreValue(int64_t{7}));
    const auto* second = cache->entries[1].load(std::memory_order_acquire);
    assert(second && second->nargs == 1 && second->arg_types[0] == bigIntTypeInfo
        && second->arg_states[0] == 0 && second->native_scalar_uvb);
    checkNativeScalarKey(*second, program);
    assert(cache->entries[0].load(std::memory_order_acquire) == first);
    check(true);
    auto* target_ctx = second->native_scalar_uvb->getCachedAOTContext();
    if (target_ctx->native_int_unary_method_entry_v3.load(std::memory_order_acquire)
            && qore_rt_get_stack_limit_v1(rc)) {
        ValueHolder other(program->callFunction("makeMethodProbe", nullptr, &xsink), &xsink);
        assert(!xsink && other->getType() == NT_OBJECT);
        for (bool null_view : {false, true}) {
            // A distinct snapshot receiver must survive materialization and
            // unwinding; restoring the live caller object instead is incorrect.
            rc->setObject(other->get<QoreObject>());
            rc->setClass(nullptr);
            {
                MethodStackProbe forced(target_ctx, null_view, true, object->get<QoreObject>());
                int64_t result = -123;
                assert(qore_rt_try_method_int_unary_v1(ctx, slot, object->rawBits(), 7, &result, &xsink, rc) == 1);
                assert(xsink && result == 0 && qore_get_call_program_context() == caller);
                assert(rc->getObject() == other->get<QoreObject>() && !rc->getClass());
            }
            rc->setObject(old_object);
            rc->setClass(old_class);
            assert(xsink.getExceptionErr().get<const QoreStringNode>()->equal("STACK-LIMIT-EXCEEDED"));
            ReferenceHolder<QoreHashNode> info(xsink.getExceptionInfo(), &xsink);
            assert(!xsink && info);
        }
    }
    warm(*enumeration);
    check(true);
}

static void checkFloatMethodReceivers(QoreProgram* program, const char* caller_name, bool inline_method) {
    ExceptionSink xsink;
    ValueHolder object(program->callFunction("makeFloatMethodProbe", nullptr, &xsink), &xsink);
    assert(!xsink && object->getType() == NT_OBJECT);
    auto warm = [&](QoreValue receiver, QoreValue value, double expected) {
        ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
        args->push(receiver.refSelf(), &xsink);
        assert(!xsink);
        args->push(value.refSelf(), &xsink);
        assert(!xsink);
        for (unsigned i = 0; i < 16; ++i) {
            ValueHolder result(program->callFunction(caller_name, *args, &xsink), &xsink);
            assert(!xsink && result->getType() == NT_FLOAT && result->getAsFloat() == expected);
        }
    };
    // The auto-argument caller first publishes an int argument key for a float
    // signature through ordinary coercion. It must not authorize a double-key hit.
    bool mixed_key = !std::strcmp(caller_name, "floatMethodArgumentProbe");
    warm(*object, mixed_key ? QoreValue(int64_t{7}) : QoreValue(1.25), mixed_key ? 7.5 : 1.75);
    int slot;
    QoreAOTContext* caller_ctx = getMethodProbeContext(program, caller_name, slot);
    auto* cache = caller_ctx->call_targets[slot].method_dispatch_cache.load(std::memory_order_acquire);
    assert(cache);
    const auto* first = cache->entries[0].load(std::memory_order_acquire);
    assert(first && first->nargs == 1
        && first->arg_types[0] == (mixed_key ? bigIntTypeInfo : floatTypeInfo));
    checkNativeScalarKey(*first, program);
    RuntimeConfig* rc = qore_rt_get_fast_runtime_config();
    QoreProgram* caller = qore_get_call_program_context();
    QoreObject* old_object = rc->getObject();
    const auto* old_class = rc->getClass();
    auto call = [&](QoreValue receiver, double arg, bool handled, double expected) {
        double result = -123.0;
        assert(qore_rt_try_method_float_unary_v1(caller_ctx, slot, receiver.rawBits(), arg, &result, &xsink, rc)
            == static_cast<int>(handled));
        assert(!xsink && (std::isnan(expected) ? std::isnan(result) : result == expected));
        if (handled && !expected) {
            assert(std::signbit(result) == std::signbit(expected));
        }
        assert(qore_get_call_program_context() == caller);
        assert(rc->getObject() == old_object && rc->getClass() == old_class);
    };
    if (mixed_key) {
        call(*object, 1.25, false, -123.0);
        warm(*object, QoreValue(1.25), 1.75);
        const auto* second = cache->entries[1].load(std::memory_order_acquire);
        assert(second && second->nargs == 1 && second->arg_types[0] == floatTypeInfo
            && second->arg_states[0] == 0 && second->native_scalar_uvb);
        checkNativeScalarKey(*second, program);
        assert(cache->entries[0].load(std::memory_order_acquire) == first);
    }
    call(*object, 1.25, true, 1.75);
    int64_t wrong_result = -123;
    assert(!qore_rt_try_method_int_unary_v1(caller_ctx, slot, object->rawBits(), 7,
        &wrong_result, &xsink, rc));
    assert(!xsink && wrong_result == -123);
    ValueHolder opaque(QoreValue::makeOpaqueObject(object->get<QoreObject>()), &xsink);
    call(*opaque, 1.25, true, 1.75);
    uint64_t weak_bits = qore_rt_make_weak_value(object->rawBits(), &xsink);
    assert(!xsink);
    QoreValue weak_value;
    std::memcpy(&weak_value, &weak_bits, sizeof(weak_value));
    ValueHolder weak(weak_value, &xsink);
    for (QoreValue value : {QoreValue(), QoreValue::makeNull(), QoreValue(true), QoreValue(int64_t{7}),
            QoreValue(1.25), *weak}) {
        call(value, 1.25, false, -123.0);
    }
    const auto* method = object->get<QoreObject>()->getClass()->findMethod("next");
    assert(method);
    const auto* uvb = qore_method_private::get(*method)->getFunction()->first()->getUserVariantBase();
    assert(uvb);
    auto* ctx = uvb->getCachedAOTContext();
    assert(ctx && ctx->native_float_unary_method_entry_v2.load(std::memory_order_acquire));
    assert(static_cast<bool>(ctx->native_float_unary_method_entry_v3.load(std::memory_order_acquire))
        == inline_method);
    auto normal = ctx->native_body_fn;
    ctx->native_body_fn = nullptr;
    call(*object, 1.25, false, -123.0);
    ctx->native_body_fn = normal;
    assert(!ctx->uses_self && !ctx->uses_argv);
    ctx->uses_self = true;
    call(*object, 1.25, false, -123.0);
    ctx->uses_self = false;
    ctx->uses_argv = true;
    call(*object, 1.25, false, -123.0);
    ctx->uses_argv = false;
    auto* pgm = ctx->pgm;
    ctx->pgm = nullptr;
    call(*object, 1.25, false, -123.0);
    ctx->pgm = pgm;
    call(*object, 1.25, true, 1.75);
    auto saved_v2 = ctx->native_float_unary_method_entry_v2.exchange(nullptr, std::memory_order_acq_rel);
    auto saved_v3 = ctx->native_float_unary_method_entry_v3.exchange(nullptr, std::memory_order_acq_rel);
    call(*object, 1.25, false, -123.0);
    ctx->native_float_unary_method_entry_v2.store(saved_v2, std::memory_order_release);
    ctx->native_float_unary_method_entry_v3.store(saved_v3, std::memory_order_release);
    checkPendingMethodSink(caller_ctx, slot, *object, 1.25, rc);
    checkPendingMethodSink(caller_ctx, slot, *opaque, 1.25, rc);
    call(*object, 1.25, true, 1.75);
    ValueHolder snapshot(program->callFunction("makeFloatIdentityProbe", nullptr, &xsink), &xsink);
    assert(!xsink && snapshot->getType() == NT_OBJECT);
    if (inline_method && qore_rt_get_stack_limit_v1(rc)) {
        for (bool null_view : {false, true}) {
            for (bool overflow : {false, true}) {
                {
                    // RuntimeConfig's compatibility snapshot can differ from
                    // live thread context; preserve each independently.
                    rc->setObject(snapshot->get<QoreObject>());
                    const auto* snapshot_class = qore_class_private::get(*snapshot->get<QoreObject>()->getClass());
                    rc->setClass(snapshot_class);
                    FloatMethodStackProbe probe(ctx, object->get<QoreObject>(), null_view, overflow);
                    double result = -123.0;
                    assert(qore_rt_try_method_float_unary_v1(caller_ctx, slot, object->rawBits(), 1.25,
                        &result, &xsink, rc) == 1);
                    assert(static_cast<bool>(xsink) == overflow && result == (overflow ? 0.0 : 1.75));
                    assert(rc->getObject() == snapshot->get<QoreObject>() && rc->getClass() == snapshot_class);
                    if (overflow) {
                        assert(xsink.getExceptionErr().get<const QoreStringNode>()->equal("STACK-LIMIT-EXCEEDED"));
                        ReferenceHolder<QoreHashNode> info(xsink.getExceptionInfo(), &xsink);
                        assert(!xsink && info);
                        const auto* stack = info->getKeyValue("callstack").get<const QoreListNode>();
                        assert(stack);
                        bool published = false;
                        for (size_t i = 0; i < stack->size(); ++i) {
                            const auto* frame = stack->retrieveEntry(i).get<const QoreHashNode>();
                            assert(frame);
                            const auto* name = frame->getKeyValue("function").get<const QoreStringNode>();
                            published |= name && std::strstr(name->c_str(), "NativeLeafFloatMethodDerived");
                            if (!(i % 100)) {
                                assert(!qore_check_cancel(&xsink, "float method frame inspection"));
                            }
                        }
                        assert(published);
                    }
                    rc->setObject(old_object);
                    rc->setClass(old_class);
                }
                call(*object, 1.25, true, 1.75);
            }
        }
    }
    ValueHolder identity(program->callFunction("makeFloatIdentityProbe", nullptr, &xsink), &xsink);
    assert(!xsink);
    warm(*identity, QoreValue(1.25), 1.25);
    for (double value : {0.0, -0.0, 1.25, -3.0e307, -6.0e307,
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::quiet_NaN()}) {
        call(*identity, value, true, value);
    }
    object->get<QoreObject>()->doDelete(&xsink);
    assert(!xsink);
    call(*object, 1.25, false, -123.0);
    call(*opaque, 1.25, false, -123.0);
}

// Observe which real companion generic dispatch selects. Bindings belong to
// this test's unshared Program and are restored before the probe goes away.
class GenericFloatBindingProbe {
public:
    GenericFloatBindingProbe(QoreAOTContext* ctx, QoreObject* object, unsigned version, bool eager)
        : ctx(ctx), object(object), eager(eager),
          v1(ctx->native_float_unary_method_entry.load(std::memory_order_acquire)),
          v2(ctx->native_float_unary_method_entry_v2.load(std::memory_order_acquire)),
          v3(ctx->native_float_unary_method_entry_v3.load(std::memory_order_acquire)) {
        assert(!current && v1 && v2 && version >= 1 && version <= 3);
        assert(version != 3 || v3);
        current = this;
        ctx->native_float_unary_method_entry.store(callV1, std::memory_order_release);
        ctx->native_float_unary_method_entry_v2.store(version >= 2 ? callV2 : nullptr, std::memory_order_release);
        ctx->native_float_unary_method_entry_v3.store(version == 3 ? callV3 : nullptr, std::memory_order_release);
    }
    ~GenericFloatBindingProbe() {
        ctx->native_float_unary_method_entry.store(v1, std::memory_order_release);
        ctx->native_float_unary_method_entry_v2.store(v2, std::memory_order_release);
        ctx->native_float_unary_method_entry_v3.store(v3, std::memory_order_release);
        current = nullptr;
    }
    unsigned count(unsigned version) const {
        assert(version >= 1 && version <= 3);
        return calls[version - 1];
    }
private:
    void record(unsigned version, RuntimeConfig& rc) {
        ++calls[version - 1];
        if (eager || version == 1) {
            assert(rc.getObject() == object);
            assert(rc.getClass() == qore_class_private::get(*object->getClass()));
            ExceptionSink xsink;
            uint64_t bits = qore_rt_load_self_member("leafContextMarker", &xsink);
            QoreValue value;
            std::memcpy(&value, &bits, sizeof(value));
            ValueHolder marker(value, &xsink);
            assert(!xsink && marker->getAsBigInt() == 927);
        }
    }
    static double callV1(double arg, QoreAOTContext* ctx, ExceptionSink* xsink) {
        assert(current && current->ctx == ctx);
        current->record(1, *qore_rt_get_fast_runtime_config());
        return current->v1(arg, ctx, xsink);
    }
    static double callV2(double arg, QoreAOTContext* ctx, ExceptionSink* xsink, RuntimeConfig* rc) {
        assert(current && current->ctx == ctx);
        current->record(2, *rc);
        return current->v2(arg, ctx, xsink, rc);
    }
    static double callV3(double arg, QoreAOTContext* ctx, ExceptionSink* xsink, RuntimeConfig* rc,
            const size_t* limit) {
        assert(current && current->ctx == ctx);
        current->record(3, *rc);
        return current->v3(arg, ctx, xsink, rc, limit);
    }
    static inline thread_local GenericFloatBindingProbe* current = nullptr;
    QoreAOTContext* ctx;
    QoreObject* object;
    bool eager;
    QoreAOTContext::NativeFloatUnaryMethodEntry v1;
    QoreAOTContext::NativeFloatUnaryMethodEntryV2 v2;
    QoreAOTContext::NativeFloatUnaryMethodEntryV3 v3;
    std::array<unsigned, 3> calls{};
};

static void checkGenericFloatMethods(QoreProgram* program, bool inline_method, bool late) {
    ExceptionSink xsink;
    const char* caller_name = late ? "genericFloatLateProbe" : "genericFloatMethodProbe";
    ValueHolder object(program->callFunction("makeFloatMethodProbe", nullptr, &xsink), &xsink);
    assert(!xsink && object->getType() == NT_OBJECT);
    const auto* method = object->get<QoreObject>()->getClass()->findMethod("next");
    assert(method);
    const auto* uvb = qore_method_private::get(*method)->getFunction()->first()->getUserVariantBase();
    auto* ctx = uvb->getCachedAOTContext();
    assert(ctx && !ctx->uses_self);
    ReferenceHolder<QoreListNode> warm_args(new QoreListNode(autoTypeInfo), &xsink);
    warm_args->push(object->refSelf(), &xsink);
    assert(!xsink);
    warm_args->push(QoreValue(1.25), &xsink);
    assert(!xsink);
    // Publish a real cache entry while the live body rejects native dispatch.
    // Early admission records only immutable signature/Program proofs; after
    // restoration it must discover the companion without changing the entry.
    ctx->uses_self = late;
    ValueHolder warm(program->callFunction(caller_name, *warm_args, &xsink), &xsink);
    ctx->uses_self = false;
    assert(!xsink && warm->getAsFloat() == 1.75);
    int slot;
    QoreAOTContext* caller_ctx = getMethodProbeContext(program, caller_name, slot);
    const auto* cache = caller_ctx->call_targets[slot].method_dispatch_cache.load(std::memory_order_acquire);
    assert(cache);
    const auto* admission = cache->entries[0].load(std::memory_order_acquire);
    const bool cached_admission = !late || !std::getenv("QORE_DISABLE_AOT_EARLY_NATIVE_METHOD_ADMISSION");
    assert(admission && static_cast<bool>(admission->native_scalar_uvb) == cached_admission);
    RuntimeConfig* rc = qore_rt_get_fast_runtime_config();
    QoreObject* old_object = rc->getObject();
    const auto* old_class = rc->getClass();
    auto call = [&](QoreValue receiver, QoreValue argument) {
        uint64_t args[] = {argument.refSelf().rawBits()};
        uint64_t cleanup = args[0];
        uint64_t* cleanups[] = {&cleanup};
        uint64_t bits = qore_rt_dot_eval_method_direct_aot_consume_args(caller_ctx, slot,
            receiver.rawBits(), args, cleanups, 1, &xsink);
        QoreValue consumed;
        std::memcpy(&consumed, &cleanup, sizeof(consumed));
        assert(consumed.isNothing());
        QoreValue result;
        std::memcpy(&result, &bits, sizeof(result));
        assert(rc->getObject() == old_object && rc->getClass() == old_class);
        return result;
    };
    bool eager = !cached_admission || std::getenv("QORE_DISABLE_AOT_NATIVE_METHOD_DEFERRED_CONTEXT");
    for (unsigned version = 1; version <= (inline_method ? 3u : 2u); ++version) {
        GenericFloatBindingProbe probe(ctx, object->get<QoreObject>(), version, eager);
        ValueHolder result(call(*object, QoreValue(1.25)), &xsink);
        assert(!xsink && result->getType() == NT_FLOAT && result->getAsFloat() == 1.75);
        assert(probe.count(version) == 1);
        // An integer argument still needs ordinary float coercion; it must not
        // call an int ABI or reuse float admission without a type match.
        ValueHolder converted(call(*object, QoreValue(int64_t{7})), &xsink);
        assert(!xsink && converted->getType() == NT_FLOAT && converted->getAsFloat() == 7.5);
        assert(probe.count(version) == 1);
    }
    // v2 has two independent output channels. A miss must leave both untouched;
    // native success writes only the native channel, and ordinary matched
    // dispatch transfers one owned boxed result without claiming a native type.
    constexpr double native_sentinel = -123.25;
    constexpr uint64_t boxed_sentinel = 0x123456789abcdef0ULL;
    const bool matched_enabled = !std::getenv("QORE_DISABLE_AOT_MATCHED_SCALAR_FALLBACK");
    auto native_call = [&](QoreValue receiver, bool accepted) {
        double native = native_sentinel;
        uint64_t boxed = boxed_sentinel;
        int handled = qore_rt_try_method_float_unary_v2(caller_ctx, slot, receiver.rawBits(),
            1.25, &native, &boxed, &xsink, rc);
        assert(handled == static_cast<int>(accepted));
        assert(!xsink && native == (accepted ? 1.75 : native_sentinel) && boxed == boxed_sentinel);
        assert(rc->getObject() == old_object && rc->getClass() == old_class);
    };
    native_call(*object, cached_admission);
    native_call(QoreValue(int64_t{7}), false);
    {
        // This Program is unshared. Force a live body miss without changing
        // immutable PIC admission, then restore the context even on unwinding.
        struct RestoreSelf {
            QoreAOTContext* ctx;
            bool saved;
            ~RestoreSelf() { ctx->uses_self = saved; }
        } restore{ctx, ctx->uses_self};
        ctx->uses_self = true;
        const bool accepted = cached_admission && matched_enabled;
        for (double argument : {1.25, -3.0e307}) {
            double native = native_sentinel;
            uint64_t boxed = boxed_sentinel;
            int handled = qore_rt_try_method_float_unary_v2(caller_ctx, slot, object->rawBits(),
                argument, &native, &boxed, &xsink, rc);
            assert(!xsink && handled == (accepted ? 2 : 0) && native == native_sentinel);
            if (accepted) {
                QoreValue value;
                std::memcpy(&value, &boxed, sizeof(value));
                ValueHolder result(value, &xsink);
                assert(result->getType() == NT_FLOAT && result->getAsFloat() == argument + 0.5);
            } else {
                assert(boxed == boxed_sentinel);
            }
            assert(!xsink && rc->getObject() == old_object && rc->getClass() == old_class);
        }
        xsink.raiseException("MATCHED-METHOD-PENDING", "pre-existing exception");
        double native = native_sentinel;
        uint64_t boxed = boxed_sentinel;
        int handled = qore_rt_try_method_float_unary_v2(caller_ctx, slot, object->rawBits(),
            1.25, &native, &boxed, &xsink, rc);
        assert(handled == (accepted ? 2 : 0) && native == native_sentinel);
        assert(boxed == (accepted ? QoreValue().rawBits() : boxed_sentinel));
        assert(xsink.getExceptionErr().get<const QoreStringNode>()->equal("MATCHED-METHOD-PENDING"));
        xsink.clear();
        assert(rc->getObject() == old_object && rc->getClass() == old_class);
    }
    // A transient body miss must not poison native admission after restoration.
    native_call(*object, cached_admission);
    // The ordinary coercion above warmed an integer-argument key after the
    // live context was restored, even when early admission is disabled. Its
    // float signature cannot justify an integer result or an integer companion.
    bool integer_admission = false;
    for (auto& cache_slot : cache->entries) {
        const auto* entry = cache_slot.load(std::memory_order_acquire);
        if (entry && entry->receiver_class == object->get<QoreObject>()->getClass()
                && entry->arg_types[0] == bigIntTypeInfo && !entry->arg_states[0]) {
            integer_admission = entry->native_scalar_uvb == uvb;
        }
    }
    assert(integer_admission);
    for (int64_t argument : {int64_t{7}, int64_t{1} << 50}) {
        int64_t native = -123;
        uint64_t boxed = boxed_sentinel;
        int handled = qore_rt_try_method_int_unary_v2(caller_ctx, slot, object->rawBits(),
            argument, &native, &boxed, &xsink, rc);
        assert(!xsink && handled == (matched_enabled ? 2 : 0) && native == -123);
        if (matched_enabled) {
            QoreValue value;
            std::memcpy(&value, &boxed, sizeof(value));
            ValueHolder result(value, &xsink);
            assert(result->getType() == NT_FLOAT && result->getAsFloat() == static_cast<double>(argument) + 0.5);
        } else {
            assert(boxed == boxed_sentinel);
        }
        assert(!xsink && rc->getObject() == old_object && rc->getClass() == old_class);
    }
    if (inline_method && qore_rt_get_stack_limit_v1(rc)) {
        for (bool null_view : {false, true}) {
            for (bool overflow : {false, true}) {
                FloatMethodStackProbe probe(ctx, object->get<QoreObject>(), null_view, overflow);
                ValueHolder result(call(*object, QoreValue(1.25)), &xsink);
                assert(static_cast<bool>(xsink) == overflow);
                if (overflow) {
                    assert(result->isNothing());
                    assert(xsink.getExceptionErr().get<const QoreStringNode>()->equal("STACK-LIMIT-EXCEEDED"));
                    ReferenceHolder<QoreHashNode> info(xsink.getExceptionInfo(), &xsink);
                    assert(!xsink && info);
                    const auto* stack = info->getKeyValue("callstack").get<const QoreListNode>();
                    assert(stack);
                    bool published = false;
                    for (size_t i = 0; i < stack->size(); ++i) {
                        const auto* frame = stack->retrieveEntry(i).get<const QoreHashNode>();
                        assert(frame);
                        const auto* name = frame->getKeyValue("function").get<const QoreStringNode>();
                        published |= name && std::strstr(name->c_str(), "NativeLeafFloatMethodDerived");
                        if (!(i % 100)) {
                            assert(!qore_check_cancel(&xsink, "generic float method frame inspection"));
                        }
                    }
                    assert(published);
                } else {
                    assert(result->getType() == NT_FLOAT && result->getAsFloat() == 1.75);
                }
            }
        }
    }
    assert(admission->native_scalar_uvb == (cached_admission ? uvb : nullptr));
    ValueHolder identity(program->callFunction("makeFloatIdentityProbe", nullptr, &xsink), &xsink);
    assert(!xsink);
    for (double value : {0.0, -0.0, 1.25, -3.0e307, -6.0e307,
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::quiet_NaN()}) {
        ValueHolder argument(QoreValue(value), &xsink);
        ValueHolder result(call(*identity, *argument), &xsink);
        assert(!xsink && result->getType() == NT_FLOAT);
        double actual = result->getAsFloat();
        assert(std::isnan(value) ? std::isnan(actual) : value == actual);
        if (!value) {
            assert(std::signbit(value) == std::signbit(actual));
        }
    }
}

// Replace one unshared Program's member binding with an adapter that still
// executes the real compiled entry. This distinguishes a fast hit from an
// accidentally correct ordinary fallback and forces overflow after publication.
template <typename Scalar>
class MemberEntryProbe {
public:
    static auto& binding(QoreAOTContext* ctx) {
        if constexpr (std::is_same_v<Scalar, double>) {
            return ctx->scalar_member_float_entry;
        } else {
            return ctx->scalar_member_int_entry;
        }
    }
    MemberEntryProbe(QoreAOTContext* ctx, bool enabled, bool overflow = false)
        : ctx(enabled ? ctx : nullptr), overflow(overflow) {
        if (this->ctx) {
            assert(!current);
            saved = binding(ctx).exchange(invoke, std::memory_order_acq_rel);
            assert(saved);
            current = this;
        }
    }
    ~MemberEntryProbe() {
        if (ctx) {
            assert(calls);
            binding(ctx).store(saved, std::memory_order_release);
            current = nullptr;
        }
    }
private:
    using Entry = std::conditional_t<std::is_same_v<Scalar, double>, QoreAOTContext::ScalarMemberFloatEntry,
        QoreAOTContext::ScalarMemberIntEntry>;
    static thread_local MemberEntryProbe* current;
    QoreAOTContext* ctx;
    bool overflow;
    Entry saved = nullptr;
    unsigned calls = 0;
    static Scalar invoke(Scalar arg, QoreObject* object, const qore_class_private* cls, RuntimeConfig* rc,
            int* accepted, QoreAOTContext* ctx, ExceptionSink* xsink) {
        assert(current && current->ctx == ctx && object && cls && *accepted == 1);
        ++current->calls;
        struct RestoreLimit {
            size_t* limit = nullptr;
            size_t saved = 0;
            ~RestoreLimit() { if (limit) { *limit = saved; } }
        } restore;
        if (current->overflow) {
#if defined(__aarch64__) || defined(__x86_64__)
            restore.limit = const_cast<size_t*>(qore_rt_get_stack_limit_v1(rc));
            assert(restore.limit);
            restore.saved = *restore.limit;
            *restore.limit = get_stack_pos() + 4096;
#else
            assert(false && "forced member limit requires a native stack view");
#endif
        }
        Scalar result = current->saved(arg, object, cls, rc, accepted, ctx, xsink);
        if (current->overflow) {
            assert(*xsink && result == 0 && *accepted == 1);
            *restore.limit = restore.saved;
            assert(rc->getObject() == object && rc->getClass() == cls);
            ExceptionSink probe_sink;
            uint64_t bits = qore_rt_load_self_member("leafContextMarker", &probe_sink);
            QoreValue value;
            std::memcpy(&value, &bits, sizeof(value));
            ValueHolder marker(value, &probe_sink);
            assert(!probe_sink && marker->getAsBigInt() == 927);
        }
        return result;
    }
};
template <typename Scalar>
thread_local MemberEntryProbe<Scalar>* MemberEntryProbe<Scalar>::current = nullptr;

template <typename Scalar, bool Internal = false>
static void checkMemberMethods(QoreProgram* program) {
    constexpr bool floating = std::is_same_v<Scalar, double>;
    const char* caller_name = Internal ? (floating ? "memberInternalFloatProbe" : "memberInternalIntProbe")
        : (floating ? "memberFloatProbe" : "memberIntProbe");
    const char* class_name = Internal ? (floating ? "NativeLeafMemberInternalFloat" : "NativeLeafMemberInternalInt")
        : (floating ? "NativeLeafMemberFloat" : "NativeLeafMemberInt");
    ExceptionSink xsink;
    const char* factory = Internal ? (floating ? "makeMemberInternalFloatProbe" : "makeMemberInternalIntProbe")
        : (floating ? "makeMemberFloatProbe" : "makeMemberIntProbe");
    ValueHolder owner(program->callFunction(factory,
        nullptr, &xsink), &xsink);
    assert(!xsink && owner->getType() == NT_OBJECT);
    QoreObject* object = owner->get<QoreObject>();
    const auto* cls = qore_class_private::get(*object->getClass());
    const Scalar arg = 1;
    const Scalar initial = static_cast<Scalar>(floating ? 0.5 : 17);
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
    args->push(owner->refSelf(), &xsink);
    assert(!xsink);
    args->push(QoreValue(arg), &xsink);
    assert(!xsink);
    for (unsigned i = 0; i < 16; ++i) {
        ValueHolder result(program->callFunction(caller_name, *args, &xsink), &xsink);
        assert(!xsink && result->getType() == (floating ? NT_FLOAT : NT_INT));
        assert((floating ? result->getAsFloat() : result->getAsBigInt()) == initial + arg);
    }
    int slot;
    QoreAOTContext* caller = getMethodProbeContext(program, caller_name, slot);
    const auto* method = object->getClass()->findMethod("next");
    assert(method);
    const auto* uvb = qore_method_private::get(*method)->getFunction()->first()->getUserVariantBase();
    assert(uvb && uvb->hasCachedAOT());
    auto* ctx = uvb->getCachedAOTContext();
    assert(ctx && ctx->uses_self);
    const bool published = !std::getenv("QORE_DISABLE_AOT_SCALAR_MEMBER_ENTRY");
    assert(bool(MemberEntryProbe<Scalar>::binding(ctx).load(std::memory_order_acquire)) == published);
    const bool admitted = !std::getenv("QORE_DISABLE_AOT_EARLY_NATIVE_METHOD_ADMISSION");
    const bool enabled = admitted && published;
    const bool fallback = admitted && !std::getenv("QORE_DISABLE_AOT_MATCHED_SCALAR_FALLBACK");
    RuntimeConfig* rc = qore_rt_get_fast_runtime_config();
    QoreObject* old_object = rc->getObject();
    const auto* old_class = rc->getClass();
    auto invoke = [&](Scalar* result, uint64_t* boxed) {
        if constexpr (floating) {
            return boxed ? qore_rt_try_method_float_unary_v2(caller, slot, owner->rawBits(), arg,
                result, boxed, &xsink, rc) : qore_rt_try_method_float_unary_v1(caller, slot, owner->rawBits(),
                    arg, result, &xsink, rc);
        } else {
            return boxed ? qore_rt_try_method_int_unary_v2(caller, slot, owner->rawBits(), arg,
                result, boxed, &xsink, rc) : qore_rt_try_method_int_unary_v1(caller, slot, owner->rawBits(),
                    arg, result, &xsink, rc);
        }
    };
    auto read = [&](QoreObject* receiver, const char* name, Scalar* result) {
        const size_t length = name ? std::strlen(name) : 0;
        const uint64_t hash64 = name ? XXH64(name, length, 0) : 0;
        const uint32_t hash32 = name ? XXH32(name, length, 0) : 0;
        const Scalar initial_result = result ? *result : 0;
        Scalar prehashed_result = initial_result;
        int prehashed;
        int ordinary;
        if constexpr (floating) {
            prehashed = qore_rt_try_load_member_float_prehashed_v1(receiver, name, hash64, hash32,
                cls, result ? &prehashed_result : nullptr);
            ordinary = qore_rt_try_load_member_float_v1(receiver, name, cls, result);
        } else {
            prehashed = qore_rt_try_load_member_int_prehashed_v1(receiver, name, hash64, hash32,
                cls, result ? &prehashed_result : nullptr);
            ordinary = qore_rt_try_load_member_int_v1(receiver, name, cls, result);
        }
        assert(prehashed == ordinary);
        assert(!result || !std::memcmp(&prehashed_result, result, sizeof(Scalar)));
        if constexpr (Internal) {
            Scalar internal_result = initial_result;
            int resolved;
            if constexpr (floating) {
                resolved = qore_rt_try_load_member_float_internal_prehashed_v1(receiver, name, hash64, hash32,
                    cls, result ? &internal_result : nullptr);
                assert(!qore_rt_try_load_member_float_internal_prehashed_v1(receiver, name, hash64, hash32,
                    nullptr, result ? &internal_result : nullptr));
            } else {
                resolved = qore_rt_try_load_member_int_internal_prehashed_v1(receiver, name, hash64, hash32,
                    cls, result ? &internal_result : nullptr);
                assert(!qore_rt_try_load_member_int_internal_prehashed_v1(receiver, name, hash64, hash32,
                    nullptr, result ? &internal_result : nullptr));
            }
            assert(resolved == ordinary);
            assert(!result || !std::memcmp(&internal_result, result, sizeof(Scalar)));
        }
        return ordinary;
    };
    auto set = [&](QoreValue value) {
        if constexpr (Internal) {
            object->setMemberValue("delta", object->getClass(), value, &xsink);
        } else {
            object->setValue("delta", value, &xsink);
        }
        assert(!xsink);
    };
    auto equal = [](Scalar a, Scalar b) { return a == b || (std::isnan(a) && std::isnan(b)); };
    auto check = [&](bool boxed, int handled, Scalar expected) {
        Scalar result = -123;
        uint64_t bits = 0x123456789abcdef0;
        assert(invoke(&result, boxed ? &bits : nullptr) == handled);
        assert(!xsink && rc->getObject() == old_object && rc->getClass() == old_class);
        if (handled == 2) {
            QoreValue value;
            std::memcpy(&value, &bits, sizeof(value));
            ValueHolder owned(value, &xsink);
            assert(result == -123 && owned->getType() == (floating ? NT_FLOAT : NT_INT));
            assert(equal(floating ? owned->getAsFloat() : owned->getAsBigInt(), expected));
        } else {
            assert(bits == 0x123456789abcdef0 && equal(result, handled ? expected : Scalar{-123}));
        }
    };
    std::array<Scalar, 3> values;
    if constexpr (floating) {
        values = {0.5, -9.0e307, std::numeric_limits<double>::quiet_NaN()};
    } else {
        values = {17, int64_t{1} << 50, -(int64_t{1} << 50)};
    }
    for (Scalar value : values) {
        set(QoreValue(value));
        assert(!xsink);
        Scalar output = -123;
        assert(read(object, "delta", &output) == 1 && equal(output, value));
        MemberEntryProbe<Scalar> probe(ctx, enabled);
        check(false, enabled ? 1 : 0, value + arg);
        check(true, enabled ? 1 : fallback ? 2 : 0, value + arg);
    }
    Scalar output = -123;
    assert(!read(nullptr, "delta", &output) && output == -123);
    assert(!read(object, nullptr, &output) && output == -123);
    assert(!read(object, "missing", &output) && output == -123);
    assert(!read(object, "delta", nullptr));
    ValueHolder enumeration(program->callFunction("makeEnumProbe", nullptr, &xsink), &xsink);
    assert(!xsink);
    for (QoreValue value : {QoreValue(), *enumeration, floating ? QoreValue(int64_t{7}) : QoreValue(1.25)}) {
        set(value.refSelf());
        assert(!xsink);
        assert(!read(object, "delta", &output) && output == -123);
        MemberEntryProbe<Scalar> probe(ctx, enabled);
        check(false, 0, -123);
        if (value.isNothing()) {
            check(true, fallback ? 2 : 0, arg);
        }
    }
    set(QoreValue(initial));
    assert(!xsink);
    auto normal = ctx->native_body_fn;
    ctx->native_body_fn = nullptr;
    check(false, 0, -123);
    ctx->native_body_fn = normal;
    if (enabled && qore_rt_get_stack_limit_v1(rc)) {
        {
            MemberEntryProbe<Scalar> probe(ctx, true, true);
            Scalar result = -123;
            assert(invoke(&result, nullptr) == 1 && result == 0 && xsink);
            assert(rc->getObject() == old_object && rc->getClass() == old_class);
            assert(xsink.getExceptionErr().get<const QoreStringNode>()->equal("STACK-LIMIT-EXCEEDED"));
            ReferenceHolder<QoreHashNode> info(xsink.getExceptionInfo(), &xsink);
            assert(!xsink && info);
            const auto* stack = info->getKeyValue("callstack").get<const QoreListNode>();
            assert(stack);
            bool published = false;
            for (size_t i = 0; i < stack->size(); ++i) {
                const auto* frame = stack->retrieveEntry(i).get<const QoreHashNode>();
                assert(frame);
                const auto* name = frame->getKeyValue("function").get<const QoreStringNode>();
                published |= name && std::strstr(name->c_str(), class_name);
                if (!(i % 100)) {
                    assert(!qore_check_cancel(&xsink, "scalar member overflow frame inspection"));
                }
            }
            assert(published);
        }
        check(false, 1, initial + arg);
        xsink.raiseException("MEMBER-PENDING", "pre-existing exception");
        Scalar result = -123;
        assert(invoke(&result, nullptr) == 1 && result == 0 && xsink);
        assert(xsink.getExceptionErr().get<const QoreStringNode>()->equal("MEMBER-PENDING"));
        xsink.clear();
        assert(rc->getObject() == old_object && rc->getClass() == old_class);
    }
    object->doDelete(&xsink);
    assert(!xsink && !read(object, "delta", &output) && output == -123);
    check(false, 0, -123);
}

// An unshared Program's pair is replaced together for the lifetime of this
// test-only probe. Both adapters execute the real compiler-produced entries;
// a correct ordinary fallback cannot satisfy the fast/resume hit assertions.
class EffectEntryProbe {
public:
    explicit EffectEntryProbe(QoreAOTContext* ctx, bool overflow = false,
            QoreAOTCallTarget* retire_target = nullptr)
            : ctx(ctx), saved(ctx->effect_member_int_entry.load(std::memory_order_acquire)),
              overflow(overflow), retire_target(retire_target) {
        assert(saved && !current && saved->fast == ctx->scalar_member_int_entry.load(std::memory_order_acquire));
        intercepted = {invoke, resume, saved->updates};
        current = this;
        ctx->effect_member_int_entry.store(&intercepted, std::memory_order_release);
        ctx->scalar_member_int_entry.store(invoke, std::memory_order_release);
    }
    ~EffectEntryProbe() {
        assert(calls);
        ctx->scalar_member_int_entry.store(saved->fast, std::memory_order_release);
        ctx->effect_member_int_entry.store(saved, std::memory_order_release);
        current = nullptr;
    }
    unsigned calls = 0;
    unsigned resumes = 0;
    unsigned retirements = 0;
    int accepted = -1;
    int completed = -1;
private:
    static inline thread_local EffectEntryProbe* current = nullptr;
    QoreAOTContext* ctx;
    const QoreAOTEffectMemberIntEntryV1* saved;
    QoreAOTEffectMemberIntEntryV1 intercepted{};
    bool overflow;
    QoreAOTCallTarget* retire_target;
    static int64_t invoke(int64_t arg, QoreObject* object, const qore_class_private* cls, RuntimeConfig* rc,
            int* accepted, QoreAOTContext* ctx, ExceptionSink* xsink) {
        assert(current && current->ctx == ctx && object && cls && *accepted == 1);
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
            assert(false && "forced effect limit requires a native stack view");
#endif
        }
        int64_t result = current->saved->fast(arg, object, cls, rc, accepted, ctx, xsink);
        current->accepted = *accepted;
        if (current->overflow) {
            assert(*xsink && result == 0 && *accepted == 1);
            *limit.address = limit.saved;
            assert(rc->getObject() == object && rc->getClass() == cls);
            ExceptionSink probe_sink;
            uint64_t bits = qore_rt_load_self_member("leafContextMarker", &probe_sink);
            QoreValue value;
            std::memcpy(&value, &bits, sizeof(value));
            ValueHolder marker(value, &probe_sink);
            assert(!probe_sink && marker->getAsBigInt() == 927);
        }
        return result;
    }
    static uint64_t resume(int32_t completed, QoreAOTContext* ctx, ExceptionSink* xsink) {
        assert(current && current->ctx == ctx);
        ++current->resumes;
        current->completed = completed;
        if (current->retire_target) {
            // Generic reentry can retire the caller's PIC storage. Only the
            // callee context remains live here; the continuation must already
            // own its diagnostic name and stop borrowing the old descriptor.
            // This Program is unshared, with no concurrent cache users.
            auto* retired = current->retire_target->method_dispatch_cache.exchange(
                nullptr, std::memory_order_acq_rel);
            assert(retired);
            delete retired;
            current->retire_target = nullptr;
            ++current->retirements;
        }
        return current->saved->resume(completed, ctx, xsink);
    }
};

static void checkEffectMethods(QoreProgram* program) {
    ExceptionSink xsink;
    ValueHolder owner(program->callFunction("makeEffectIntProbe", nullptr, &xsink), &xsink);
    assert(!xsink && owner->getType() == NT_OBJECT);
    QoreObject* object = owner->get<QoreObject>();
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
    args->push(owner->refSelf(), &xsink);
    args->push(QoreValue(int64_t{1}), &xsink);
    assert(!xsink);
    for (unsigned i = 0; i < 16; ++i) {
        ValueHolder value(program->callFunction("effectIntProbe", *args, &xsink), &xsink);
        assert(!xsink && value->getType() == NT_INT && value->getAsBigInt() == 18);
    }
    int slot;
    QoreAOTContext* caller = getMethodProbeContext(program, "effectIntProbe", slot);
    const auto* method = object->getClass()->findMethod("next");
    assert(method);
    const auto* uvb = qore_method_private::get(*method)->getFunction()->first()->getUserVariantBase();
    assert(uvb && uvb->hasCachedAOT());
    auto* ctx = uvb->getCachedAOTContext();
    assert(ctx && ctx->uses_self);
    const bool published = !std::getenv("QORE_DISABLE_AOT_EFFECT_MEMBER_ENTRY")
        && !std::getenv("QORE_DISABLE_AOT_NATIVE_METHOD_DEFERRED_CONTEXT");
    const auto* descriptor = ctx->effect_member_int_entry.load(std::memory_order_acquire);
    assert(bool(descriptor) == published);
    assert(bool(ctx->scalar_member_int_entry.load(std::memory_order_acquire)) == published);
    if (descriptor) {
        assert(descriptor->updates == 2 && descriptor->resume
            && descriptor->fast == ctx->scalar_member_int_entry.load(std::memory_order_acquire));
        // Wrong-body, malformed and conflicting publication must leave both
        // bindings intact, including when a matching normal body runs again.
        qore_rt_bind_effect_member_int_v1(ctx, caller->native_body_fn, descriptor);
        QoreAOTEffectMemberIntEntryV1 invalid = *descriptor;
        invalid.updates = 0;
        qore_rt_bind_effect_member_int_v1(ctx, ctx->native_body_fn, &invalid);
        invalid.updates = 22;
        qore_rt_bind_effect_member_int_v1(ctx, ctx->native_body_fn, &invalid);
        invalid = *descriptor;
        qore_rt_bind_effect_member_int_v1(ctx, ctx->native_body_fn, &invalid);
        assert(ctx->effect_member_int_entry.load(std::memory_order_acquire) == descriptor);
        assert(ctx->scalar_member_int_entry.load(std::memory_order_acquire) == descriptor->fast);
    }
    const bool admitted = !std::getenv("QORE_DISABLE_AOT_EARLY_NATIVE_METHOD_ADMISSION");
    const bool enabled = admitted && published;
    const bool fallback = admitted && !std::getenv("QORE_DISABLE_AOT_MATCHED_SCALAR_FALLBACK");
    RuntimeConfig* rc = qore_rt_get_fast_runtime_config();
    QoreObject* old_object = rc->getObject();
    const auto* old_class = rc->getClass();
    auto set = [&](const char* name, QoreValue value) {
        object->setValue(name, value, &xsink);
        assert(!xsink);
    };
    auto counters = [&](int64_t first, int64_t second) {
        ValueHolder a(object->getReferencedMemberNoMethod("first", &xsink), &xsink);
        ValueHolder b(object->getReferencedMemberNoMethod("second", &xsink), &xsink);
        assert(!xsink && a->getAsBigInt() == first && b->getAsBigInt() == second);
    };
    // Compare the actual update helpers, including rejecting malformed calls
    // before effects. A precomputed hash changes lookup only, not admission.
    const auto* cls = qore_class_private::get(*object->getClass());
    for (bool prehashed : {false, true}) {
        auto update = [&](QoreObject* receiver, const char* name, int64_t delta,
                const qore_class_private* context) {
            if (!prehashed) {
                return qore_rt_try_update_member_int_v1(receiver, name, context, delta);
            }
            size_t length = name ? std::strlen(name) : 0;
            return qore_rt_try_update_member_int_prehashed_v1(receiver, name,
                name ? XXH64(name, length, 0) : 0, name ? XXH32(name, length, 0) : 0, context, delta);
        };
        set("first", QoreValue(int64_t{0}));
        set("second", QoreValue(int64_t{20}));
        assert(!update(nullptr, "first", 1, cls));
        assert(!update(object, nullptr, 1, cls));
        assert(!update(object, "first", 0, cls));
        assert(!update(object, "first", 2, cls));
        assert(!update(object, "missing", 1, cls));
        assert(!update(object, "leafContextMarker", 1, nullptr));
        counters(0, 20);
        assert(update(object, "first", 1, nullptr));
        assert(update(object, "second", -1, cls));
        counters(1, 19);
        set("first", QoreValue((int64_t{1} << 47) - 1));
        set("second", QoreValue(-(int64_t{1} << 47)));
        assert(!update(object, "first", 1, cls) && !update(object, "second", -1, cls));
        counters((int64_t{1} << 47) - 1, -(int64_t{1} << 47));
        set("first", QoreValue(0.0));
        assert(!update(object, "first", 1, cls));
        ValueHolder unchanged(object->getReferencedMemberNoMethod("first", &xsink), &xsink);
        assert(!xsink && unchanged->getType() == NT_FLOAT && unchanged->getAsFloat() == 0.0);
    }
    auto invoke = [&](int64_t arg, int64_t* value, uint64_t* boxed = nullptr) {
        return boxed ? qore_rt_try_method_int_unary_v2(caller, slot, owner->rawBits(), arg,
            value, boxed, &xsink, rc) : qore_rt_try_method_int_unary_v1(caller, slot, owner->rawBits(),
                arg, value, &xsink, rc);
    };
    struct Case { int64_t first; int64_t second; bool floating; int completed; };
    for (const Case& test : {Case{0, 20, false, -1}, Case{(int64_t{1} << 47) - 1, 20, false, 0},
            Case{0, -(int64_t{1} << 47), false, 1}, Case{0, 20, true, 2}}) {
        for (int64_t arg : {int64_t{1}, int64_t{1} << 60}) {
            for (bool boxed : {false, true}) {
                set("first", QoreValue(test.first));
                set("second", QoreValue(test.second));
                set("delta", test.floating ? QoreValue(17.0) : QoreValue(int64_t{17}));
                int64_t result = -123;
                uint64_t bits = 0x123456789abcdef0;
                int handled;
                if (enabled) {
                    EffectEntryProbe probe(ctx);
                    handled = invoke(arg, &result, boxed ? &bits : nullptr);
                    assert(probe.calls == 1 && probe.resumes == unsigned(test.completed >= 0));
                    assert(probe.accepted == (test.completed < 0 ? 1 : test.completed + 2));
                    assert(probe.completed == test.completed);
                } else {
                    handled = invoke(arg, &result, boxed ? &bits : nullptr);
                }
                bool expected_exception = test.floating && handled;
                if (bool(xsink) != expected_exception || handled != (enabled ? 1 : boxed && fallback ? 2 : 0)) {
                    const auto* err = xsink.getExceptionErr().get<const QoreStringNode>();
                    std::fprintf(stderr, "effect probe: completed=%d arg=%lld boxed=%d handled=%d error=%s\n",
                        test.completed, static_cast<long long>(arg), boxed, handled, err ? err->c_str() : "none");
                }
                assert(bool(xsink) == expected_exception && handled == (enabled ? 1 : boxed && fallback ? 2 : 0));
                assert(rc->getObject() == old_object && rc->getClass() == old_class);
                if (expected_exception) {
                    assert(xsink.getExceptionErr().get<const QoreStringNode>()->equal("RUNTIME-TYPE-ERROR"));
                    ReferenceHolder<QoreHashNode> info(xsink.getExceptionInfo(), &xsink);
                    assert(!xsink && info);
                    const auto* stack = info->getKeyValue("callstack").get<const QoreListNode>();
                    assert(stack);
                    unsigned frames = 0;
                    for (size_t i = 0; i < stack->size(); ++i) {
                        const auto* frame = stack->retrieveEntry(i).get<const QoreHashNode>();
                        assert(frame);
                        const auto* name = frame->getKeyValue("function").get<const QoreStringNode>();
                        frames += name && name->equal("NativeLeafEffectInt::next");
                        if (!(i % 100)) { assert(!qore_check_cancel(&xsink, "effect return frame inspection")); }
                    }
                    assert(frames == 1);
                }
                int64_t expected = arg + 17;
                if (handled == 2) {
                    QoreValue value;
                    std::memcpy(&value, &bits, sizeof(value));
                    ValueHolder owned(value, &xsink);
                    assert(result == -123);
                    assert(expected_exception ? owned->isNothing()
                        : owned->getType() == NT_INT && owned->getAsBigInt() == expected);
                } else {
                    assert(bits == 0x123456789abcdef0
                        && result == (handled ? expected_exception ? 0 : expected : -123));
                }
                counters(test.first + !!handled, test.second - !!handled);
            }
        }
    }
    set("first", QoreValue(int64_t{0}));
    set("second", QoreValue(int64_t{20}));
    set("delta", QoreValue(int64_t{17}));
    int64_t result = -123;
    auto normal = ctx->native_body_fn;
    ctx->native_body_fn = nullptr;
    assert(!invoke(1, &result) && result == -123);
    ctx->native_body_fn = normal;
    counters(0, 20);
    if (enabled && qore_rt_get_stack_limit_v1(rc)) {
        {
            EffectEntryProbe probe(ctx, true);
            assert(invoke(1, &result) == 1 && result == 0 && xsink);
            assert(probe.calls == 1 && !probe.resumes);
            assert(rc->getObject() == old_object && rc->getClass() == old_class);
            assert(xsink.getExceptionErr().get<const QoreStringNode>()->equal("STACK-LIMIT-EXCEEDED"));
            ReferenceHolder<QoreHashNode> info(xsink.getExceptionInfo(), &xsink);
            assert(!xsink && info);
            const auto* stack = info->getKeyValue("callstack").get<const QoreListNode>();
            assert(stack);
            unsigned frames = 0;
            for (size_t i = 0; i < stack->size(); ++i) {
                const auto* frame = stack->retrieveEntry(i).get<const QoreHashNode>();
                assert(frame);
                const auto* name = frame->getKeyValue("function").get<const QoreStringNode>();
                frames += name && name->equal("NativeLeafEffectInt::next");
                if (!(i % 100)) { assert(!qore_check_cancel(&xsink, "effect member frame inspection")); }
            }
            assert(frames == 1);
        }
        counters(0, 20);
        xsink.raiseException("EFFECT-PENDING", "pre-existing exception");
        assert(invoke(1, &result) == 1 && result == 0 && xsink);
        assert(xsink.getExceptionErr().get<const QoreStringNode>()->equal("EFFECT-PENDING"));
        xsink.clear();
        counters(0, 20);
        assert(invoke(1, &result) == 1 && result == 18 && !xsink);
        counters(1, 19);
    }
    if (enabled) {
        for (bool boxed : {false, true}) {
            set("delta", QoreValue(int64_t{17}));
            ValueHolder warmed(program->callFunction("effectIntProbe", *args, &xsink), &xsink);
            assert(!xsink && warmed->getAsBigInt() == 18);
            set("first", QoreValue(int64_t{0}));
            set("second", QoreValue(int64_t{20}));
            set("delta", QoreValue(17.0));
            uint64_t bits = 0x123456789abcdef0;
            {
                EffectEntryProbe probe(ctx, false, &caller->call_targets[slot]);
                assert(invoke(1, &result, boxed ? &bits : nullptr) == 1 && result == 0 && xsink);
                assert(probe.calls == 1 && probe.resumes == 1 && probe.retirements == 1);
                assert(probe.accepted == 4 && probe.completed == 2);
            }
            assert(!caller->call_targets[slot].method_dispatch_cache.load(std::memory_order_acquire));
            assert(bits == 0x123456789abcdef0);
            assert(rc->getObject() == old_object && rc->getClass() == old_class);
            assert(xsink.getExceptionErr().get<const QoreStringNode>()->equal("RUNTIME-TYPE-ERROR"));
            ReferenceHolder<QoreHashNode> info(xsink.getExceptionInfo(), &xsink);
            assert(!xsink && info);
            const auto* stack = info->getKeyValue("callstack").get<const QoreListNode>();
            assert(stack);
            unsigned frames = 0;
            for (size_t i = 0; i < stack->size(); ++i) {
                const auto* frame = stack->retrieveEntry(i).get<const QoreHashNode>();
                assert(frame);
                const auto* name = frame->getKeyValue("function").get<const QoreStringNode>();
                frames += name && name->equal("NativeLeafEffectInt::next");
                if (!(i % 100)) { assert(!qore_check_cancel(&xsink, "retired effect cache frame inspection")); }
            }
            assert(frames == 1);
            counters(1, 19);
        }
        set("delta", QoreValue(int64_t{17}));
        ValueHolder rewarmed(program->callFunction("effectIntProbe", *args, &xsink), &xsink);
        assert(!xsink && rewarmed->getAsBigInt() == 18);
        counters(2, 18);
    }
    object->doDelete(&xsink);
    assert(!xsink && !invoke(1, &result));
}

// Enter through a normal compiled function so probes inherit the same real
// source parse policy as the warmed callers; the external host's policy differs.
static QoreValue methodReceiverProbes(const void* ptr, const QoreListNode*, RuntimeConfig&, ExceptionSink*) {
    const auto& invocation = *static_cast<const LeafInvocation*>(ptr);
    checkMethodReceivers(invocation.program, "methodProbe");
    checkMethodReceivers(invocation.program, "optionalMethodProbe");
    checkMethodReceivers(invocation.program, "nativeForwardMethodProbe");
    checkPrivateForwardReceivers(invocation.program);
    checkMethodArgumentKeys(invocation.program, "methodArgumentProbe");
    checkMethodArgumentKeys(invocation.program, "optionalMethodArgumentProbe");
    checkFloatMethodReceivers(invocation.program, "floatMethodProbe", invocation.inline_method);
    checkFloatMethodReceivers(invocation.program, "optionalFloatMethodProbe", invocation.inline_method);
    checkFloatMethodReceivers(invocation.program, "floatMethodArgumentProbe", invocation.inline_method);
    checkGenericFloatMethods(invocation.program, invocation.inline_method, false);
    checkGenericFloatMethods(invocation.program, invocation.inline_method, true);
    checkMemberMethods<int64_t>(invocation.program);
    checkMemberMethods<double>(invocation.program);
    checkMemberMethods<int64_t, true>(invocation.program);
    checkMemberMethods<double, true>(invocation.program);
    checkEffectMethods(invocation.program);
    return QoreValue(int64_t{24});
}

// Exercise a warmed native method PIC through its real compiled caller, including
// early rejection, published method overflow, and successful reuse afterward.
static void checkMethod(QoreProgram* program, const char* caller_name, bool inline_method,
        bool null_view = false) {
    ExceptionSink xsink;
    ValueHolder object(program->callFunction("makeMethodProbe", nullptr, &xsink), &xsink);
    assert(!xsink && object->getType() == NT_OBJECT);
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
    args->push(object->refSelf(), &xsink);
    assert(!xsink);
    args->push(QoreValue(int64_t{7}), &xsink);
    assert(!xsink);
    for (unsigned i = 0; i < 16; ++i) {
        ValueHolder result(program->callFunction(caller_name, *args, &xsink), &xsink);
        assert(!xsink && result->getAsBigInt() == 24);
    }
    const QoreMethod* method = object->get<QoreObject>()->getClass()->findMethod("next");
    assert(method);
    const UserVariantBase* uvb = qore_method_private::get(*method)->getFunction()->first()->getUserVariantBase();
    assert(uvb && uvb->hasCachedAOT());
    QoreAOTContext* ctx = uvb->getCachedAOTContext();
    assert(ctx && ctx->native_body_fn == uvb->getCachedAOTFunction());
    assert(ctx->native_int_unary_method_entry_v2.load(std::memory_order_acquire));
    assert((ctx->native_int_unary_method_entry_v3.load(std::memory_order_acquire) != nullptr) == inline_method);
    int forward_line = -1;
    int caller_line = -1;
    if (!std::strcmp(caller_name, "genericForwardMethodProbe")
            || !std::strcmp(caller_name, "nativeForwardMethodProbe")) {
        const auto* caller_function = reinterpret_cast<const QoreFunction*>(program->findFunction(caller_name));
        assert(caller_function);
        caller_line = caller_function->first()->getUserVariantBase()->getUserSignature()->getParseLocation()->start_line;
        const auto* forward_method = object->get<QoreObject>()->getClass()->findMethod("forward");
        assert(forward_method);
        const auto* forward_uvb = qore_method_private::get(*forward_method)->getFunction()->first()->getUserVariantBase();
        auto* forward_ctx = forward_uvb->getCachedAOTContext();
        assert(forward_ctx);
        const auto* forward = forward_ctx->receiver_int_forward_entry.load(std::memory_order_acquire);
        assert(forward && forward->loc);
        checkForwardPublication(forward_ctx);
        checkCancelledBackgroundPreparation(forward_ctx);
        forward_line = forward->loc->start_line;
        if (!std::getenv("QORE_DISABLE_AOT_EARLY_NATIVE_METHOD_ADMISSION")) {
            ForwardEntryProbe probe(forward_ctx);
            ValueHolder result(program->callFunction(caller_name, *args, &xsink), &xsink);
            assert(!xsink && result->getAsBigInt() == 24);
            bool enabled = !std::getenv("QORE_DISABLE_AOT_FORWARD_LEAF_PREFLIGHT")
                && !std::getenv("QORE_DISABLE_AOT_NATIVE_METHOD_DEFERRED_CONTEXT");
            assert(probe.calls == (enabled ? 0u : 1u));
            forward_ctx->receiver_int_forward_entry.store(nullptr, std::memory_order_release);
            unsigned before = probe.calls;
            ValueHolder missed(program->callFunction(caller_name, *args, &xsink), &xsink);
            forward_ctx->receiver_int_forward_entry.store(forward, std::memory_order_release);
            assert(!xsink && missed->getAsBigInt() == 24 && probe.calls == before + 1);
        }
    }
    MethodStackProbe force_null(ctx, null_view);
    RuntimeConfig* rc = qore_rt_get_fast_runtime_config();
    QoreObject* saved_object = rc->getObject();
    const auto* saved_class = rc->getClass();
    QoreProgram* caller = qore_get_call_program_context();
    size_t* limit = const_cast<size_t*>(qore_rt_get_stack_limit_v1(rc));
#if defined(__aarch64__) || defined(__x86_64__)
    if (limit) {
        const size_t saved_limit = *limit;
        unsigned observed_early = 0;
        unsigned observed_success = 0;
        // Include the compiled caller and public Program entry in the sweep.
        // Restore the real limit before inspecting diagnostics or cleanup.
        for (unsigned gap = 0; gap < 32768; gap += 16) {
            *limit = get_stack_pos() - gap;
            QoreValue value = program->callFunction(caller_name, *args, &xsink);
            *limit = saved_limit;
            ValueHolder result(value, &xsink);
            assert(qore_get_call_program_context() == caller);
            assert(rc->getObject() == saved_object && rc->getClass() == saved_class);
            if (xsink) {
                assert(xsink.getExceptionErr().get<const QoreStringNode>()->equal("STACK-LIMIT-EXCEEDED"));
                ReferenceHolder<QoreHashNode> info(xsink.getExceptionInfo(), &xsink);
                assert(!xsink);
                const auto* stack = info->getKeyValue("callstack").get<const QoreListNode>();
                assert(stack);
                bool method = false;
                for (size_t i = 0; i < stack->size(); ++i) {
                    const auto* frame = stack->retrieveEntry(i).get<const QoreHashNode>();
                    assert(frame);
                    const auto* name = frame->getKeyValue("function").get<const QoreStringNode>();
                    method |= name && std::strstr(name->c_str(), "NativeLeafMethodDerived");
                    if (!(i % 100)) {
                        assert(!qore_check_cancel(&xsink, "native method stack inspection"));
                    }
                }
                observed_early += !method;
            } else {
                assert(result->getAsBigInt() == 24);
                ++observed_success;
            }
            if (!(gap % 160)) {
                assert(!qore_check_cancel(&xsink, "native method guard sweep"));
            }
        }
        assert(observed_early && observed_success);
        // Frame sizes and tail calls can leave either entry above the earlier
        // dispatch guard's SP. Force each entry's own overflow below instead
        // of depending on a published-overflow interval in this external sweep.
    }
#else
    assert(!limit);
#endif
    ValueHolder result(program->callFunction(caller_name, *args, &xsink), &xsink);
    assert(!xsink && result->getAsBigInt() == 24 && qore_get_call_program_context() == caller);
    assert(rc->getObject() == saved_object && rc->getClass() == saved_class);
    if (inline_method && !null_view) {
        checkMethod(program, caller_name, inline_method, true);
    }
    if (!null_view && limit) {
        for (bool forced_null : {false, true}) {
            if (forced_null && !inline_method) {
                continue;
            }
            {
                MethodStackProbe forced(ctx, forced_null, true, object->get<QoreObject>());
                ValueHolder failed(program->callFunction(caller_name, *args, &xsink), &xsink);
                assert(xsink && qore_get_call_program_context() == caller);
                assert(rc->getObject() == saved_object && rc->getClass() == saved_class);
                assert(xsink.getExceptionErr().get<const QoreStringNode>()->equal("STACK-LIMIT-EXCEEDED"));
                ReferenceHolder<QoreHashNode> info(xsink.getExceptionInfo(), &xsink);
                assert(!xsink);
                const auto* file = info->getKeyValue("file").get<const QoreStringNode>();
                assert(file && !file->empty());
                assert(info->getKeyValue("line").getAsBigInt()
                    == uvb->getUserSignature()->getParseLocation()->start_line);
                const auto* stack = info->getKeyValue("callstack").get<const QoreListNode>();
                assert(stack);
                bool published = false;
                bool forward_published = false;
                for (size_t i = 0; i < stack->size(); ++i) {
                    const auto* frame = stack->retrieveEntry(i).get<const QoreHashNode>();
                    assert(frame);
                    const auto* name = frame->getKeyValue("function").get<const QoreStringNode>();
                    published |= name && std::strstr(name->c_str(), "NativeLeafMethodDerived");
                    forward_published |= name && std::strstr(name->c_str(), "::forward");
                    if (caller_line >= 0 && name && std::strstr(name->c_str(), "::forward")) {
                        assert(frame->getKeyValue("line").getAsBigInt() == caller_line);
                    }
                    if (forward_line >= 0 && name && std::strstr(name->c_str(), "::next")) {
                        if (frame->getKeyValue("line").getAsBigInt() != forward_line) {
                            std::fprintf(stderr, "%s call-site line %lld, expected %d\n", name->c_str(),
                                static_cast<long long>(frame->getKeyValue("line").getAsBigInt()), forward_line);
                        }
                        assert(frame->getKeyValue("line").getAsBigInt() == forward_line);
                    }
                    if (!(i % 100)) {
                        assert(!qore_check_cancel(&xsink, "forced method stack inspection"));
                    }
                }
                assert(published);
                assert(forward_line < 0 || forward_published);
            }
            ValueHolder recovered(program->callFunction(caller_name, *args, &xsink), &xsink);
            assert(!xsink && recovered->getAsBigInt() == 24 && qore_get_call_program_context() == caller);
            assert(rc->getObject() == saved_object && rc->getClass() == saved_class);
        }
    }
}

// Called with the parsed caller's owned locals and captures still live. The
// compiled required/nullable callers must not consume either variable stack
// when a native method overflow materializes its otherwise-empty markers.
static QoreValue paddedMethodSweep(const void* ptr, const QoreListNode*, RuntimeConfig&, ExceptionSink*) {
    const auto& invocation = *static_cast<const LeafInvocation*>(ptr);
    checkMethod(invocation.program, "methodProbe", invocation.inline_method);
    checkMethod(invocation.program, "optionalMethodProbe", invocation.inline_method);
    checkMethod(invocation.program, "genericIntMethodProbe", invocation.inline_method);
    checkMethod(invocation.program, "genericForwardMethodProbe", invocation.inline_method);
    checkMethod(invocation.program, "nativeForwardMethodProbe", invocation.inline_method);
    return QoreValue(int64_t{24});
}

// Check the FP helper's independently compiled frame/return path as well as
// the integer path below. Keep this before switching parsed callers to AST.
static void checkFloat(QoreProgram* program, bool inline_entry) {
    ExceptionSink xsink;
    ValueHolder ref(program->callFunction("makeFloatProbe", nullptr, &xsink), &xsink);
    assert(!xsink && ref->getType() == NT_FUNCREF);
    const auto* node = ref->get<const ResolvedCallReferenceNode>();
    const auto* target = static_cast<const FunctionCallReferenceNode*>(node);
    assert(target->getTypedEntry(10));
    assert((target->getTypedInlineStackEntry(10) != nullptr) == inline_entry);
    QoreExternalProgramContextHelper context(&xsink, node->getProgram());
    assert(!xsink);
    uint64_t bits;
    std::memcpy(&bits, &*ref, sizeof(bits));
    QorePreparedFunctionRefV3 cache{};
    QoreValue empty;
    std::memcpy(&cache.owner, &empty, sizeof(cache.owner));
    RuntimeConfig* rc = qore_rt_get_fast_runtime_config();
    double result = 0;
    for (unsigned version : {3, 4, 9}) {
        bool reuse = version != 3;
        ReusableCallbackFrame reusable;
        auto call = [&reusable, reuse, version](uint64_t bits, double arg, double* result, ExceptionSink* sink,
                RuntimeConfig* config, QorePreparedFunctionRefV3* prepared) {
            if (version == 9) {
                return qore_rt_try_funcref_float_float_v9(bits, arg, result, sink, config, prepared, &reusable.slot);
            }
            return reuse ? qore_rt_try_funcref_float_float_v4(bits, arg, result, sink, config, prepared,
                    &reusable.slot)
                : qore_rt_try_funcref_float_float_v3(bits, arg, result, sink, config, prepared);
        };
        assert(call(bits, 1.25, &result, &xsink, rc, &cache) == 1);
        assert(!xsink && result == 1.75 && cache.owner == bits && cache.ref == target);
        assert(!reusable.slot.frame);
        assert(call(bits, 1.25, &result, &xsink, rc, &cache) == 1);
        assert(!xsink && result == 1.75 && bool(reusable.slot.frame) == (reuse && inline_entry));
        void* original_frame = reusable.slot.frame;
        xsink.raiseException("CALLBACK-PENDING", "pending floating sink must prevent entry");
        result = 123.0;
        assert(call(bits, 1.25, &result, &xsink, rc, &cache) == 1);
        assert(result == 0.0 && xsink.getExceptionErr().get<const QoreStringNode>()->equal("CALLBACK-PENDING"));
        {
            ReferenceHolder<QoreHashNode> info(xsink.getExceptionInfo(), &xsink);
            assert(!xsink && info);
        }
        assert(reusable.slot.frame == original_frame);
        {
            QoreProgramHelper foreign(PO_MODERN, xsink);
            QoreExternalProgramContextHelper foreign_context(&xsink, *foreign);
            assert(!xsink);
            result = 123.0;
            assert(call(bits, 1.25, &result, &xsink, qore_rt_get_fast_runtime_config(), &cache) == 0);
            assert(!xsink && result == 123.0 && cache.owner == bits);
        }
        auto* leaf_ctx = cache.uvb->getCachedAOTContext();
        assert(leaf_ctx);
        const auto* saved_location = qore_rt_get_fast_runtime_config()->getLocation();
        assert(call(bits, 1.25, &result, &xsink, rc, &cache) == 1);
        assert(qore_rt_get_fast_runtime_config()->getLocation() == saved_location);
        assert(!xsink && result == 1.75 && reusable.slot.frame == original_frame);
        checkPreparedMisses(program, "makeProbe", call,
            bits, cache, rc, 1.25, 1.75, 10, &xsink, version == 9);
        QoreProgram* caller = qore_get_call_program_context();
        size_t* limit = const_cast<size_t*>(qore_rt_get_stack_limit_v1(rc));
#if defined(__aarch64__) || defined(__x86_64__)
        if (limit) {
            const size_t saved_limit = *limit;
            unsigned observed_leaf = 0;
            unsigned observed_early = 0;
            for (unsigned gap = 0; gap < 4096; ++gap) {
                *limit = get_stack_pos() - gap;
                int handled = call(bits, 1.25, &result, &xsink, rc, &cache);
                *limit = saved_limit;
                assert(handled == 1 && qore_get_call_program_context() == caller);
                if (xsink) {
                    assert(result == 0.0);
                    assert(xsink.getExceptionErr().get<const QoreStringNode>()->equal("STACK-LIMIT-EXCEEDED"));
                    ReferenceHolder<QoreHashNode> info(xsink.getExceptionInfo(), &xsink);
                    assert(!xsink);
                    const auto* stack = info->getKeyValue("callstack").get<const QoreListNode>();
                    assert(stack);
                    bool leaf = false;
                    for (size_t i = 0; i < stack->size(); ++i) {
                        const auto* frame = stack->retrieveEntry(i).get<const QoreHashNode>();
                        assert(frame);
                        const auto* name = frame->getKeyValue("function").get<const QoreStringNode>();
                        leaf |= name && std::strstr(name->c_str(), "nativeLeafFloatProbe");
                        if (!(i % 100)) {
                            assert(!qore_check_cancel(&xsink, "float leaf stack inspection"));
                        }
                    }
                    ++(leaf ? observed_leaf : observed_early);
                } else {
                    assert(result == 1.75);
                }
                if (!(gap % 10)) {
                    assert(!qore_check_cancel(&xsink, "float leaf guard sweep"));
                }
            }
            assert(observed_leaf && observed_early);
        }
#else
        assert(!limit);
#endif
        assert(call(bits, -3.0e307, &result, &xsink, rc, &cache) == 1);
        assert(!xsink && result == -3.0e307);
        // Replace the retained target while keeping the same inactive frame. Native
        // double arguments/results must preserve signed zero, NaN and wide values.
        ValueHolder identity(program->callFunction("makeFloatCallbackIdentityProbe", nullptr, &xsink), &xsink);
        assert(!xsink && identity->getType() == NT_FUNCREF);
        uint64_t identity_bits;
        std::memcpy(&identity_bits, &*identity, sizeof(identity_bits));
        // A new reference starts a new v9 descriptor lifetime. Legacy helpers
        // continue to exercise replacement of an existing positive owner/frame.
        auto retire_invariant = [&]() {
            qore_rt_release_funcref_frame_v1(&reusable.slot);
            QoreValue owner;
            std::memcpy(&owner, &cache.owner, sizeof(owner));
            owner.discard(&xsink);
            assert(!xsink);
            std::memcpy(&cache.owner, &empty, sizeof(cache.owner));
        };
        if (version == 9) {
            retire_invariant();
            assert(call(identity_bits, 1.25, &result, &xsink, rc, &cache) == 1);
            assert(!xsink && result == 1.25 && !reusable.slot.frame);
        }
        for (double value : {0.0, -0.0, 1.25, -3.0e307, 1.0e308,
                std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
                std::numeric_limits<double>::quiet_NaN(), -std::numeric_limits<double>::quiet_NaN()}) {
            assert(call(identity_bits, value, &result, &xsink, rc, &cache) == 1);
            assert(!xsink && (std::isnan(value) ? std::isnan(result) : result == value));
            if (value == 0.0) {
                assert(std::signbit(result) == std::signbit(value));
            }
            assert(reusable.slot.frame == original_frame);
        }
        if (version == 9) {
            retire_invariant();
            assert(call(bits, 1.25, &result, &xsink, rc, &cache) == 1);
            assert(!xsink && result == 1.75 && !reusable.slot.frame);
        }
        assert(call(bits, 1.25, &result, &xsink, rc, &cache) == 1);
        assert(!xsink && result == 1.75 && reusable.slot.frame == original_frame);
        qore_rt_release_funcref_frame_v1(&reusable.slot);
        assert(!reusable.slot.frame);
        assert(call(bits, 1.25, &result, &xsink, rc, &cache) == 1);
        assert(!xsink && result == 1.75 && bool(reusable.slot.frame) == (reuse && inline_entry));
        qore_rt_release_funcref_frame_v1(&reusable.slot);
        QoreValue owner;
        std::memcpy(&owner, &cache.owner, sizeof(owner));
        owner.discard(&xsink);
        assert(!xsink);
        std::memcpy(&cache.owner, &empty, sizeof(cache.owner));
    }
}

static void check(bool inline_entry, bool inline_method) {
    ExceptionSink xsink;
    QoreProgramHelper program(PO_MODERN, xsink);
    LeafInvocation invocation;
    invocation.program = *program;
    invocation.inline_method = inline_method;
    {
        ProgramRuntimeExternalParseContextHelper parse_context(*program);
        assert(parse_context);
        program->getRootNS()->addBuiltinVariant(&invocation, "runLeafSweep", paddedSweep,
            QCF_NO_FLAGS, QDOM_DEFAULT, bigIntTypeInfo, 0);
        program->getRootNS()->addBuiltinVariant(&invocation, "runMethodSweep", paddedMethodSweep,
            QCF_NO_FLAGS, QDOM_DEFAULT, bigIntTypeInfo, 0);
        program->getRootNS()->addBuiltinVariant(&invocation, "runReceiverProbes", methodReceiverProbes,
            QCF_NO_FLAGS, QDOM_DEFAULT, bigIntTypeInfo, 0);
    }
    qore_aot_script_begin_batch(*program);
    init_NATIVE_LEAF_CONTEXT_qo(*program);
    assert(!qore_aot_script_end_batch(*program));
    QoreExternalProgramContextHelper context(&xsink, *program);
    ValueHolder ref(program->callFunction("makeProbe", nullptr, &xsink), &xsink);
    assert(!xsink);
    assert(ref->getType() == NT_FUNCREF);
    const auto* target = static_cast<const FunctionCallReferenceNode*>(ref->get<const ResolvedCallReferenceNode>());
    assert(target->getTypedEntry(5));
    assert((target->getTypedInlineStackEntry(5) != nullptr) == inline_entry);
    QoreProgram* target_program = ref->get<const ResolvedCallReferenceNode>()->getProgram();
    assert(target_program);
    QoreExternalProgramContextHelper target_context(&xsink, target_program);
    assert(!xsink);
    uint64_t bits;
    static_assert(sizeof(bits) == sizeof(QoreValue));
    std::memcpy(&bits, &*ref, sizeof(bits));
    RuntimeConfig* rc = qore_rt_get_fast_runtime_config();
    QorePreparedFunctionRefV3 cache{};
    // Initialize the owner from the actual NaN-boxed NOTHING representation.
    QoreValue empty;
    std::memcpy(&cache.owner, &empty, sizeof(cache.owner));
    int64_t result = 0;
    for (unsigned version : {3, 4, 5, 9}) {
        bool reuse = version != 3;
        if (version >= 5) {
            // Start v5/v9 from the same cold descriptor as v3/v4. The preceding
            // v4 iteration deliberately leaves its retained owner warm.
            QoreValue owner;
            std::memcpy(&owner, &cache.owner, sizeof(owner));
            owner.discard(&xsink);
            assert(!xsink);
            std::memcpy(&cache.owner, &empty, sizeof(cache.owner));
        }
        ReusableCallbackFrame reusable;
        auto call = [&reusable, version](uint64_t bits, int64_t arg, int64_t* result, ExceptionSink* sink,
                RuntimeConfig* config, QorePreparedFunctionRefV3* prepared) {
            if (version == 9) {
                return qore_rt_try_funcref_int_int_v9(bits, arg, result, sink, config, prepared, &reusable.slot);
            }
            if (version == 5) {
                return qore_rt_try_funcref_int_int_v5(bits, arg, result, sink, config, prepared, &reusable.slot);
            }
            return version == 4 ? qore_rt_try_funcref_int_int_v4(bits, arg, result, sink, config, prepared, &reusable.slot)
                : qore_rt_try_funcref_int_int_v3(bits, arg, result, sink, config, prepared);
        };
        assert(call(bits, 7, &result, &xsink, rc, &cache) == 1);
        assert(!xsink && result == 24);
        assert(cache.owner == bits && cache.ref == target);
        assert(!reusable.slot.frame); // first call only populates the cache
        assert(call(bits, 7, &result, &xsink, rc, &cache) == 1);
        assert(!xsink && result == 24);
        assert(bool(reusable.slot.frame) == (reuse && inline_entry));
        void* original_frame = reusable.slot.frame;
        checkPreparedMisses(*program, "makeFloatProbe", call,
            bits, cache, rc, int64_t{7}, int64_t{24}, 5, &xsink, version == 9);
        xsink.raiseException("CALLBACK-PENDING", "pending sink must prevent entry");
        result = 123;
        assert(call(bits, 7, &result, &xsink, rc, &cache) == 1);
        assert(result == 0 && xsink.getExceptionErr().get<const QoreStringNode>()->equal("CALLBACK-PENDING"));
        {
            ReferenceHolder<QoreHashNode> info(xsink.getExceptionInfo(), &xsink);
            assert(!xsink && info);
        }
        assert(reusable.slot.frame == original_frame);
        {
            // Admission remains Program-specific even with a warm frame/cache.
            QoreProgramHelper foreign(PO_MODERN, xsink);
            QoreExternalProgramContextHelper foreign_context(&xsink, *foreign);
            assert(!xsink);
            result = 123;
            assert(call(bits, 7, &result, &xsink, qore_rt_get_fast_runtime_config(), &cache) == 0);
            assert(!xsink && result == 123 && cache.owner == bits);
        }
        assert(call(bits, 7, &result, &xsink, rc, &cache) == 1);
        assert(!xsink && result == 24 && reusable.slot.frame == original_frame);
        auto* leaf_ctx = cache.uvb->getCachedAOTContext();
        assert(leaf_ctx);
        const auto* saved_location = qore_rt_get_fast_runtime_config()->getLocation();
        assert(call(bits, 7, &result, &xsink, rc, &cache) == 1);
        assert(qore_rt_get_fast_runtime_config()->getLocation() == saved_location);
        assert(!xsink && result == 24 && reusable.slot.frame == original_frame);
        QoreProgram* caller = qore_get_call_program_context();
        size_t* limit = const_cast<size_t*>(qore_rt_get_stack_limit_v1(rc));
#if defined(__aarch64__) || defined(__x86_64__)
        if (limit) {
            unsigned observed_leaf = 0;
            unsigned observed_early = 0;
            const size_t saved_limit = *limit;
            // Test-only mutation of the same-thread live limit. Sweep individual
            // bytes to cover the caller/frame/callee guard boundaries independent
            // of release/debug frame sizes; always restore before inspecting errors.
            for (unsigned gap = 0; gap < 4096; ++gap) {
                *limit = get_stack_pos() - gap;
                int handled = call(bits, 7, &result, &xsink, rc, &cache);
                *limit = saved_limit;
                assert(handled == 1);
                assert(qore_get_call_program_context() == caller);
                if (xsink) {
                    assert(xsink.getExceptionErr().get<const QoreStringNode>()->equal("STACK-LIMIT-EXCEEDED"));
                    ReferenceHolder<QoreHashNode> info(xsink.getExceptionInfo(), &xsink);
                    assert(!xsink);
                    const auto* stack = info->getKeyValue("callstack").get<const QoreListNode>();
                    assert(stack);
                    bool leaf = false;
                    for (size_t i = 0; i < stack->size(); ++i) {
                        const auto* frame = stack->retrieveEntry(i).get<const QoreHashNode>();
                        assert(frame);
                        const auto* name = frame->getKeyValue("function").get<const QoreStringNode>();
                        if (name && std::strstr(name->c_str(), "nativeLeafProbe")) {
                            const auto* file = info->getKeyValue("file").get<const QoreStringNode>();
                            assert(file && !file->empty());
                            leaf = true;
                        }
                        if (!(i % 100)) {
                            assert(!qore_check_cancel(&xsink, "native leaf stack inspection"));
                        }
                    }
                    ++(leaf ? observed_leaf : observed_early);
                } else {
                    assert(result == 24);
                }
                if (!(gap % 10)) {
                    assert(!qore_check_cancel(&xsink, "native leaf guard sweep"));
                }
            }
            assert(observed_leaf && observed_early);
        }
#else
        assert(!limit);
#endif
        for (unsigned i = 0; i < 20; ++i) {
            assert(call(bits, i, &result, &xsink, rc, &cache) == 1);
            assert(!xsink && result == i + 17);
        }
        assert(reusable.slot.frame == original_frame);
        qore_rt_release_funcref_frame_v1(&reusable.slot);
        assert(!reusable.slot.frame);
        if (!reuse) {
            QoreValue owner;
            std::memcpy(&owner, &cache.owner, sizeof(owner));
            owner.discard(&xsink);
            assert(!xsink);
            std::memcpy(&cache.owner, &empty, sizeof(cache.owner));
        } else {
            // Reconstruct in the same scratch after cleanup, with the owner intact.
            assert(call(bits, 7, &result, &xsink, rc, &cache) == 1);
            assert(!xsink && result == 24 && bool(reusable.slot.frame) == inline_entry);
        }
    }
    checkMethod(*program, "methodProbe", inline_method);
    checkMethod(*program, "optionalMethodProbe", inline_method);
    checkMethod(*program, "genericForwardMethodProbe", inline_method);
    checkMethod(*program, "nativeForwardMethodProbe", inline_method);
    checkFloat(*program, inline_entry);
    invocation.bits = bits;
    invocation.cache = &cache;
    // Keep parsed callers' variables on TLS, independently of the native leaf.
    program->setExecMode(QEM_AST, true);
    // Vary both live locals and captures over a complete marker block, verifying
    // that caller values and both variable stacks survive materialized failures.
    std::string source = "%modern\ncode<int()> sub makeReceiverProbe() { return \\runReceiverProbes(); }\n";
    for (unsigned padding = 0; padding <= 70; ++padding) {
        source += "int sub padded" + std::to_string(padding) + "(int seed) {\n";
        for (unsigned local = 0; local < padding; ++local) {
            source += "string pad" + std::to_string(local) + " = string(seed);\n";
        }
        for (unsigned capture = 0; capture <= padding; ++capture) {
            source += "int captured" + std::to_string(capture) + " = seed;\n";
        }
        source += "code<int()> closure = int sub() { return captured0";
        for (unsigned capture = 1; capture <= padding; ++capture) {
            source += " + captured" + std::to_string(capture);
        }
        source += "; };\nint total = runLeafSweep();\n";
        if (!padding || padding == 31 || padding == 32 || padding == 33
                || padding == 63 || padding == 64 || padding == 65 || padding == 70) {
            source += "if (runMethodSweep() != 24) { throw \"LEAF-CONTEXT\", \"method changed\"; }\n";
        }
        source += "if (closure() != seed * "
            + std::to_string(padding + 1) + ") { throw \"LEAF-CONTEXT\", \"closure changed\"; }\n";
        for (unsigned local = 0; local < padding; ++local) {
            source += "if (pad" + std::to_string(local)
                + " != string(seed)) { throw \"LEAF-CONTEXT\", \"local changed\"; }\n"
                "total += pad" + std::to_string(local) + ".size();\n";
        }
        source += "return total;\n}\n";
    }
    {
        ProgramRuntimeExternalParseContextHelper parse_context(*program);
        assert(parse_context);
        program->parse(source.c_str(), "native-leaf-padded", &xsink);
    }
    if (xsink) {
        xsink.handleExceptions();
        assert(false && "padded caller source must parse");
    }
    assert(!xsink);
    {
        ReferenceHolder<QoreListNode> probe_args(new QoreListNode(autoTypeInfo), &xsink);
        ValueHolder probe(program->callFunction("makeReceiverProbe", nullptr, &xsink), &xsink);
        assert(!xsink);
        probe_args->push(probe.release(), &xsink);
        assert(!xsink);
        ValueHolder result(program->callFunction("methodReceiverProbe", *probe_args, &xsink), &xsink);
        assert(!xsink && result->getAsBigInt() == 24);
    }
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
    args->push(QoreValue(int64_t(7)), &xsink);
    assert(!xsink);
    for (unsigned padding = 0; padding <= 70; ++padding) {
        std::string name = "padded" + std::to_string(padding);
        ValueHolder actual(program->callFunction(name.c_str(), *args, &xsink), &xsink);
        assert(!xsink && actual->getAsBigInt() == 24 + padding);
        if (!(padding % 10)) {
            assert(!qore_check_cancel(&xsink, "native leaf padded callers"));
        }
    }
    QoreValue owner;
    std::memcpy(&owner, &cache.owner, sizeof(owner));
    owner.discard(&xsink);
    assert(!xsink);
    ValueHolder control(program->callFunction("contextControl", nullptr, &xsink), &xsink);
    assert(!xsink && control->getAsBigInt() == 23);
}

int main(int argc, char** argv) {
    assert(argc == 3);
    bool inline_entry = !std::strcmp(argv[1], "inline");
    assert(inline_entry || !std::strcmp(argv[1], "eager"));
    bool inline_method = !std::strcmp(argv[2], "inline");
    assert(inline_method || !std::strcmp(argv[2], "eager"));
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    checkLocalSlots();
    check(inline_entry, inline_method);
    std::thread worker([inline_entry, inline_method]() {
        for (unsigned i = 0; i < 3; ++i) {
            QoreForeignThreadHelper foreign;
            check(inline_entry, inline_method);
        }
    });
    worker.join();
    qore_cleanup();
    std::puts("native-leaf-context: ok");
}
