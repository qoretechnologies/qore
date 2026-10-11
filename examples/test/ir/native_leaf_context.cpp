/* Copyright (c) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
#define _QORE_LIB_INTERN
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
#include <cstring>
#include <initializer_list>
#include <limits>
#include <string>
#include <thread>

#if defined(__aarch64__)
#include <qore/macros-aarch64.h>
#elif defined(__x86_64__)
#include <qore/macros-x86_64.h>
#endif

struct LeafInvocation {
    uint64_t bits = 0;
    QorePreparedFunctionRefV3* cache = nullptr;
    QoreProgram* program = nullptr;
    bool inline_method = false;
};

// Test-only adapter: the actual compiler-proven entry still executes, using a
// null view or a changed live limit after frame publication. Each test owns an unshared Program;
// the saved target belongs only to this thread and is restored before teardown.
static thread_local QoreAOTContext::NativeIntUnaryMethodEntryV3 null_view_target = nullptr;
static thread_local unsigned null_view_calls = 0;
static thread_local bool method_null_view = false;
static thread_local bool method_force_overflow = false;
static thread_local QoreObject* method_expected_object = nullptr;
static int64_t nullViewMethod(int64_t arg, QoreAOTContext* ctx, ExceptionSink* xsink,
        RuntimeConfig* rc, const size_t* view) {
    ++null_view_calls;
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
        assert(restore.limit && restore.limit == view);
        restore.saved = *restore.limit;
        *restore.limit = get_stack_pos() + 4096;
#else
        assert(false && "forced live limit requires a supported native stack view");
#endif
    }
    int64_t result = null_view_target(arg, ctx, xsink, rc, method_null_view ? nullptr : view);
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

class MethodStackProbe {
public:
    MethodStackProbe(QoreAOTContext* ctx, bool null_view, bool overflow = false,
            QoreObject* expected_object = nullptr)
            : ctx(null_view || overflow ? ctx : nullptr) {
        if (this->ctx) {
            assert(!null_view_target);
            null_view_target = ctx->native_int_unary_method_entry_v3.load(std::memory_order_acquire);
            assert(null_view_target && null_view_target != nullViewMethod);
            null_view_calls = 0;
            method_null_view = null_view;
            method_force_overflow = overflow;
            method_expected_object = expected_object;
            ctx->native_int_unary_method_entry_v3.store(nullViewMethod, std::memory_order_release);
        }
    }
    ~MethodStackProbe() {
        if (ctx) {
            assert(null_view_calls);
            ctx->native_int_unary_method_entry_v3.store(null_view_target, std::memory_order_release);
            null_view_target = nullptr;
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
        Arg arg, Result expected, uint32_t signature, ExceptionSink* xsink) {
    ValueHolder other(program->callFunction(other_factory, nullptr, xsink), xsink);
    assert(!*xsink && other->getType() == NT_FUNCREF);
    const auto* node = static_cast<const FunctionCallReferenceNode*>(other->get<const ResolvedCallReferenceNode>());
    assert(node->getProgram() == cache.pgm && !node->getTypedEntry(signature));
    uint64_t other_bits;
    std::memcpy(&other_bits, &*other, sizeof(other_bits));
    QoreValue empty;
    uint64_t empty_bits;
    std::memcpy(&empty_bits, &empty, sizeof(empty_bits));
    QorePreparedFunctionRefV3 cold{};
    cold.owner = empty_bits;
    const auto* saved_ref = cache.ref;
    for (uint64_t invalid : {other_bits, empty_bits}) {
        Result result = 123;
        assert(call(invalid, arg, &result, xsink, rc, &cold) == 0);
        assert(!*xsink && result == 123 && cold.owner == empty_bits);
        assert(call(invalid, arg, &result, xsink, rc, &cache) == 0);
        assert(!*xsink && result == 123 && cache.owner == valid_bits && cache.ref == saved_ref);
        assert(call(valid_bits, arg, &result, xsink, rc, &cache) == 1);
        assert(!*xsink && result == expected && cache.owner == valid_bits && cache.ref == saved_ref);
    }
}

// Invoked by parsed callers whose owned locals/closures are on the TLS stacks.
// The leaf remains an actually bound native entry from the script aggregate.
static QoreValue paddedSweep(const void* ptr, const QoreListNode*, RuntimeConfig&, ExceptionSink* xsink) {
    const auto& invocation = *static_cast<const LeafInvocation*>(ptr);
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
            int handled = qore_rt_try_funcref_int_int_v3(invocation.bits, 7, &result, xsink, rc, invocation.cache);
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
    assert(qore_rt_try_funcref_int_int_v3(invocation.bits, 7, &result, xsink, rc, invocation.cache) == 1);
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
    check(*object, true);
    ValueHolder opaque(QoreValue::makeOpaqueObject(object->get<QoreObject>()), &xsink);
    assert(opaque->isOpaqueObject() && opaque->isPointer());
    check(*opaque, true);
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
    const auto* admitted = cache->entries[0].load(std::memory_order_acquire);
    assert(admitted && admitted->native_scalar_uvb);
    auto* callee = admitted->native_scalar_uvb->getCachedAOTContext();
    // Publication readiness remains live even after a successful lookup.
    bool map_attached = callee->pc_loc_map_attached.exchange(false, std::memory_order_acq_rel);
    check(*object, false);
    callee->pc_loc_map_attached.store(map_attached, std::memory_order_release);
    check(*object, true);
    auto normal = callee->pc_loc_fn;
    callee->pc_loc_fn = nullptr;
    check(*object, false);
    callee->pc_loc_fn = normal;
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
    bool ready = ctx->pc_loc_map_attached.exchange(false, std::memory_order_acq_rel);
    call(*object, 1.25, false, -123.0);
    ctx->pc_loc_map_attached.store(ready, std::memory_order_release);
    auto normal = ctx->pc_loc_fn;
    ctx->pc_loc_fn = nullptr;
    call(*object, 1.25, false, -123.0);
    ctx->pc_loc_fn = normal;
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
    xsink.raiseException("FLOAT-METHOD-PENDING", "pre-existing exception");
    double pending_result = -123.0;
    assert(qore_rt_try_method_float_unary_v1(caller_ctx, slot, object->rawBits(), 1.25,
        &pending_result, &xsink, rc) == 1);
    assert(pending_result == 0.0 && xsink.getExceptionErr().get<const QoreStringNode>()->equal("FLOAT-METHOD-PENDING"));
    xsink.clear();
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

// Enter through a normal compiled function so probes inherit the same real
// source parse policy as the warmed callers; the external host's policy differs.
static QoreValue methodReceiverProbes(const void* ptr, const QoreListNode*, RuntimeConfig&, ExceptionSink*) {
    const auto& invocation = *static_cast<const LeafInvocation*>(ptr);
    checkMethodReceivers(invocation.program, "methodProbe");
    checkMethodReceivers(invocation.program, "optionalMethodProbe");
    checkMethodArgumentKeys(invocation.program, "methodArgumentProbe");
    checkMethodArgumentKeys(invocation.program, "optionalMethodArgumentProbe");
    checkFloatMethodReceivers(invocation.program, "floatMethodProbe", invocation.inline_method);
    checkFloatMethodReceivers(invocation.program, "optionalFloatMethodProbe", invocation.inline_method);
    checkFloatMethodReceivers(invocation.program, "floatMethodArgumentProbe", invocation.inline_method);
    checkGenericFloatMethods(invocation.program, invocation.inline_method, false);
    checkGenericFloatMethods(invocation.program, invocation.inline_method, true);
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
    assert(ctx && ctx->pc_loc_fn == uvb->getCachedAOTFunction());
    assert(ctx->native_int_unary_method_entry_v2.load(std::memory_order_acquire));
    assert((ctx->native_int_unary_method_entry_v3.load(std::memory_order_acquire) != nullptr) == inline_method);
    int forward_line = -1;
    int caller_line = -1;
    if (!std::strcmp(caller_name, "genericForwardMethodProbe")) {
        const auto* caller_function = reinterpret_cast<const QoreFunction*>(program->findFunction(caller_name));
        assert(caller_function);
        caller_line = caller_function->first()->getUserVariantBase()->getUserSignature()->getParseLocation()->start_line;
        const auto* forward_method = object->get<QoreObject>()->getClass()->findMethod("forward");
        assert(forward_method);
        const auto* forward_uvb = qore_method_private::get(*forward_method)->getFunction()->first()->getUserVariantBase();
        auto* forward_ctx = forward_uvb->getCachedAOTContext();
        assert(forward_ctx && forward_ctx->receiver_int_forward_ready.load(std::memory_order_acquire));
        forward_line = forward_ctx->locs[forward_ctx->receiver_int_forward_loc]->start_line;
        if (!std::getenv("QORE_DISABLE_AOT_EARLY_NATIVE_METHOD_ADMISSION")) {
            ForwardEntryProbe probe(forward_ctx);
            ValueHolder result(program->callFunction(caller_name, *args, &xsink), &xsink);
            assert(!xsink && result->getAsBigInt() == 24);
            bool enabled = !std::getenv("QORE_DISABLE_AOT_FORWARD_LEAF_PREFLIGHT")
                && !std::getenv("QORE_DISABLE_AOT_NATIVE_METHOD_DEFERRED_CONTEXT");
            assert(probe.calls == (enabled ? 0u : 1u));
            forward_ctx->receiver_int_forward_ready.store(false, std::memory_order_release);
            unsigned before = probe.calls;
            ValueHolder missed(program->callFunction(caller_name, *args, &xsink), &xsink);
            forward_ctx->receiver_int_forward_ready.store(true, std::memory_order_release);
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
        unsigned observed_method = 0;
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
                ++(method ? observed_method : observed_early);
            } else {
                assert(result->getAsBigInt() == 24);
                ++observed_success;
            }
            if (!(gap % 160)) {
                assert(!qore_check_cancel(&xsink, "native method guard sweep"));
            }
        }
        assert(observed_early && observed_success);
        // The smaller inline entry can stay above the earlier dispatch guard's
        // SP, leaving no published-overflow interval in this external sweep.
        // v2/null-view still exercise that interval; v3's own overflow is forced
        // below by changing the live limit only after publication.
        assert(observed_method || (inline_method && !null_view));
    }
#else
    assert(!limit);
#endif
    ValueHolder result(program->callFunction(caller_name, *args, &xsink), &xsink);
    assert(!xsink && result->getAsBigInt() == 24 && qore_get_call_program_context() == caller);
    assert(rc->getObject() == saved_object && rc->getClass() == saved_class);
    if (inline_method && !null_view) {
        checkMethod(program, caller_name, inline_method, true);
        if (limit) {
            for (bool forced_null : {false, true}) {
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
    assert(qore_rt_try_funcref_float_float_v3(bits, 1.25, &result, &xsink, rc, &cache) == 1);
    assert(!xsink && result == 1.75 && cache.owner == bits && cache.ref == target);
    checkPreparedMisses(program, "makeProbe", qore_rt_try_funcref_float_float_v3,
        bits, cache, rc, 1.25, 1.75, 10, &xsink);
    QoreProgram* caller = qore_get_call_program_context();
    size_t* limit = const_cast<size_t*>(qore_rt_get_stack_limit_v1(rc));
#if defined(__aarch64__) || defined(__x86_64__)
    if (limit) {
        const size_t saved_limit = *limit;
        unsigned observed_leaf = 0;
        unsigned observed_early = 0;
        for (unsigned gap = 0; gap < 4096; ++gap) {
            *limit = get_stack_pos() - gap;
            int handled = qore_rt_try_funcref_float_float_v3(bits, 1.25, &result, &xsink, rc, &cache);
            *limit = saved_limit;
            assert(handled == 1 && qore_get_call_program_context() == caller);
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
    assert(qore_rt_try_funcref_float_float_v3(bits, -3.0e307, &result, &xsink, rc, &cache) == 1);
    assert(!xsink && result == -3.0e307);
    QoreValue owner;
    std::memcpy(&owner, &cache.owner, sizeof(owner));
    owner.discard(&xsink);
    assert(!xsink);
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
    assert(qore_rt_try_funcref_int_int_v3(bits, 7, &result, &xsink, rc, &cache) == 1);
    assert(!xsink && result == 24);
    assert(cache.owner == bits && cache.ref == target);
    checkPreparedMisses(*program, "makeFloatProbe", qore_rt_try_funcref_int_int_v3,
        bits, cache, rc, int64_t{7}, int64_t{24}, 5, &xsink);
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
            int handled = qore_rt_try_funcref_int_int_v3(bits, 7, &result, &xsink, rc, &cache);
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
        assert(qore_rt_try_funcref_int_int_v3(bits, i, &result, &xsink, rc, &cache) == 1);
        assert(!xsink && result == i + 17);
    }
    checkMethod(*program, "methodProbe", inline_method);
    checkMethod(*program, "optionalMethodProbe", inline_method);
    checkMethod(*program, "genericForwardMethodProbe", inline_method);
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
