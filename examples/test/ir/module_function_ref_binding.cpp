/* Copyright 2026 Qore Technologies, s.r.o. -- SPDX-License-Identifier: MIT */
#define _QORE_LIB_INTERN
#define XXH_INLINE_ALL
#include <qore/intern/xxhash/xxhash.h>
#include <qore/Qore.h>
#include <qore/QoreDebugProgram.h>
#include <qore/RuntimeConfig.h>
#include <qore/intern/QoreAOT.h>
#include <qore/intern/JITRuntime.h>
#include <qore/intern/QoreJITException.h>
#include <qore/intern/CallReferenceNode.h>
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
#include <thread>

#if defined(__aarch64__)
#include <qore/macros-aarch64.h>
#elif defined(__x86_64__)
#include <qore/macros-x86_64.h>
#endif

static unsigned checks = 0;
#define CHECK(value) do { ++checks; assert(value); } while (false)

static uint64_t bits(QoreValue value) {
    uint64_t result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

struct Prepared {
    QorePreparedFunctionRefV3 cache{};
    QorePreparedFunctionRefFrameV4 frame{};
    ExceptionSink* xsink;
    explicit Prepared(ExceptionSink& sink) : xsink(&sink) {
        cache.owner = bits(QoreValue());
    }
    ~Prepared() {
        qore_rt_release_funcref_frame_v1(&frame);
        QoreValue owner;
        std::memcpy(&owner, &cache.owner, sizeof(owner));
        owner.discard(xsink);
    }
};

static uint64_t otherBody(QoreAOTContext*, ExceptionSink*) { return 0; }

// Test-only entries on unshared references exercise the helper's caught-unwind
// contract. Production scalar-body eligibility is unchanged.
[[noreturn]] static void throwInlineResult(ExceptionSink* xsink) {
    xsink->raiseException("CALLBACK-INLINE-THROW", "test native callback unwind");
    throw QoreJITException();
}
static int64_t throwIntUnary(int64_t, QoreAOTContext*, ExceptionSink* xsink, RuntimeConfig*, const size_t*) {
    throwInlineResult(xsink);
}
static int64_t throwIntZero(QoreAOTContext*, ExceptionSink* xsink, RuntimeConfig*, const size_t*) {
    throwInlineResult(xsink);
}
static double throwFloatUnary(double, QoreAOTContext*, ExceptionSink* xsink, RuntimeConfig*, const size_t*) {
    throwInlineResult(xsink);
}
static double throwFloatZero(QoreAOTContext*, ExceptionSink* xsink, RuntimeConfig*, const size_t*) {
    throwInlineResult(xsink);
}

// Test-only instrumentation on an unshared reference observes the live frame
// through the existing builtin, without depending on RuntimeConfig's snapshot
// of parse options. No production native-body eligibility is broadened.
static const QoreMethod* inspect_options = nullptr;

static int64_t inspectOptions(ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
    args->push(QoreValue(int64_t(0)), xsink);
    ValueHolder result(inspect_options->execManaged(nullptr, *args, xsink), xsink);
    return bool(QoreParseOptions(result->getAsBigInt()) & PO_ALLOW_INJECTION);
}

static uint64_t inspectGeneric(const uint64_t*, QoreAOTContext*, ExceptionSink* xsink) {
    return bits(QoreValue(inspectOptions(xsink)));
}

static int64_t inspectTyped(int64_t, QoreAOTContext*, ExceptionSink* xsink) {
    return inspectOptions(xsink);
}

static int64_t inspectExplicit(int64_t, QoreAOTContext*, ExceptionSink* xsink, RuntimeConfig*) {
    return inspectOptions(xsink);
}

static int64_t inspectInline(int64_t, QoreAOTContext*, ExceptionSink* xsink, RuntimeConfig* rc, const size_t* limit) {
#if defined(__aarch64__) || defined(__x86_64__)
    // A successful scalar entry cannot inspect its deferred context. Force the
    // real cold edge before observing it through a builtin, as an overflow does.
    CHECK(limit);
    size_t* mutable_limit = const_cast<size_t*>(limit);
    size_t saved = *mutable_limit;
    *mutable_limit = get_stack_pos() + 4096;
    ExceptionSink overflow;
    int error = qore_rt_check_stack_v2(&overflow, rc);
    *mutable_limit = saved;
    CHECK(error && overflow);
    CHECK(overflow.getExceptionErr().get<const QoreStringNode>()->equal("STACK-LIMIT-EXCEEDED"));
    overflow.clear();
#endif
    return inspectOptions(xsink);
}

static void checkCallerOptions(QoreProgram* pgm, bool enabled) {
    if (!enabled) {
        return;
    }
    ExceptionSink xsink;
    ValueHolder value(pgm->callFunction("ModuleRefBinding::makeIdentity", nullptr, &xsink), &xsink);
    CHECK(!xsink && value->getType() == NT_FUNCREF);
    auto* ref = dynamic_cast<FunctionCallReferenceNode*>(value->getInternalNode());
    CHECK(ref && ref->getTypedEntry(5));
    const auto* uvb = ref->getFastCallMetadata().uvb;
    CHECK(uvb && uvb->pgm != pgm);
    CHECK(pgm->getParseOptions() & PO_ALLOW_INJECTION);
    CHECK(!(uvb->pgm->getParseOptions() & PO_ALLOW_INJECTION));
    const QoreClass* cls = pgm->findClass("Qore::Program", &xsink);
    CHECK(!xsink && cls);
    inspect_options = cls->findStaticMethod("getCallerCapabilityMask");
    CHECK(inspect_options);
    auto normal = ref->getNormalEntry();
    ref->setNativeEntry(inspectGeneric, normal);
    qore_rt_bind_function_call_ref_typed_v1(bits(*value), normal,
        reinterpret_cast<QoreFunctionRefTypedEntry>(inspectTyped), 5);
    qore_rt_bind_function_call_ref_typed_v2(bits(*value), normal,
        reinterpret_cast<QoreFunctionRefTypedEntry>(inspectExplicit), 5);
    RuntimeConfig* rc = qore_rt_get_fast_runtime_config();
    // Unsupported stack-limit targets retain the eager v2 probe; the typed v2
    // binding above clears any previous inline-stack binding on this test ref.
    if (qore_rt_get_stack_limit_v1(rc)) {
        qore_rt_bind_function_call_ref_typed_v3(bits(*value), normal,
            reinterpret_cast<QoreFunctionRefTypedEntry>(inspectInline), 5);
    }
    int64_t original = inspectOptions(&xsink);
    uint64_t args[] = {bits(QoreValue(int64_t(0)))};
    uint64_t result_bits = qore_rt_call_closure_fast(bits(*value), args, 1, &xsink);
    QoreValue generic;
    std::memcpy(&generic, &result_bits, sizeof(generic));
    ValueHolder result_value(generic, &xsink);
    // Injection requires both the importing caller and the module defining the
    // body to allow it. This module explicitly lacks it (checked above).
    CHECK(!xsink && result_value->getAsBigInt() == 0);
    CHECK(inspectOptions(&xsink) == original);
    int64_t result = -1;
    CHECK(qore_rt_try_funcref_int_int_v1(bits(*value), 0, &result, &xsink) == 1);
    CHECK(!xsink && result == 0 && inspectOptions(&xsink) == original);
    CHECK(qore_rt_try_funcref_int_int_v2(bits(*value), 0, &result, &xsink, rc) == 1);
    CHECK(!xsink && result == 0 && inspectOptions(&xsink) == original);
    Prepared prepared(xsink);
    CHECK(qore_rt_try_funcref_int_int_v3(bits(*value), 0, &result, &xsink, rc, &prepared.cache) == 1);
    CHECK(!xsink && result == 0 && inspectOptions(&xsink) == original);
    for (unsigned i = 0; i < 3; ++i) {
        CHECK(qore_rt_try_funcref_int_int_v5(bits(*value), 0, &result, &xsink, rc,
            &prepared.cache, &prepared.frame) == 1);
        CHECK(!xsink && result == 0 && inspectOptions(&xsink) == original);
        CHECK(qore_rt_try_funcref_int_int_v9(bits(*value), 0, &result, &xsink, rc,
            &prepared.cache, &prepared.frame) == 1);
        CHECK(!xsink && result == 0 && inspectOptions(&xsink) == original);
    }
    {
        QoreExternalProgramContextHelper owner_context(&xsink, uvb->pgm);
        CHECK(!xsink);
        int64_t owner_original = inspectOptions(&xsink);
        bool cross = !std::getenv("QORE_DISABLE_AOT_CROSS_PROGRAM_FUNCREF_FAST_CALL");
        result = -1;
        CHECK(qore_rt_try_funcref_int_int_v9(bits(*value), 0, &result, &xsink, rc,
            &prepared.cache, &prepared.frame) == cross);
        CHECK(!xsink && result == (cross ? 0 : -1));
        CHECK(rc->getProgram() == uvb->pgm && inspectOptions(&xsink) == owner_original);
    }
    CHECK(rc->getProgram() == pgm && inspectOptions(&xsink) == original && !xsink);
    inspect_options = nullptr;
}

static void checkInt(QoreProgram* pgm, const char* name, int64_t arg, int64_t expected, bool enabled) {
    ExceptionSink xsink;
    ValueHolder value(pgm->callFunction(name, nullptr, &xsink), &xsink);
    CHECK(!xsink && value->getType() == NT_FUNCREF);
    auto* ref = dynamic_cast<FunctionCallReferenceNode*>(value->getInternalNode());
    CHECK(ref && ref->getProgram() == pgm);
    const auto* uvb = ref->getFunction()->first()->getUserVariantBase();
    CHECK(uvb && uvb->pgm != pgm);
    CHECK(bool(ref->getFastCallMetadata().uvb) == enabled);
    CHECK(bool(ref->getTypedEntry(5)) == enabled);
    CHECK(bool(ref->getTypedRuntimeConfigEntry(5)) == enabled);
    CHECK(bool(ref->getTypedInlineStackEntry(5)) == enabled);
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
    args->push(QoreValue(arg), &xsink);
    ValueHolder ordinary(ref->invoke(*args, &xsink), &xsink);
    CHECK(!xsink && ordinary->getAsBigInt() == expected);
    RuntimeConfig* rc = qore_rt_get_fast_runtime_config();
    Prepared prepared(xsink);
    auto call = [&](int64_t& result) {
        return qore_rt_try_funcref_int_int_v9(bits(*value), arg, &result, &xsink, rc,
            &prepared.cache, &prepared.frame);
    };
    for (unsigned i = 0; i < 3; ++i) {
        int64_t result = 123;
        CHECK(call(result) == enabled);
        CHECK(!xsink && result == (enabled ? expected : 123));
    }
    if (!enabled) {
        CHECK(!prepared.cache.owner && !prepared.frame.frame);
        return;
    }
    CHECK(prepared.cache.ref == ref && prepared.frame.frame);
    // Binding fields are replaced only on this unshared test reference.
    auto original_native = ref->getNativeEntry();
    auto original_normal = ref->getNormalEntry();
    auto original_typed = ref->getTypedEntry(5);
    auto original_explicit = ref->getTypedRuntimeConfigEntry(5);
    auto original_inline = ref->getTypedInlineStackEntry(5);
    ref->setTypedEntry(nullptr, 5, original_normal);
    ref->setNativeEntry(nullptr, original_normal);
    qore_rt_bind_function_call_ref_typed_v1(bits(*value), original_normal, original_typed, 5);
    CHECK(!ref->getTypedEntry(5));
    ref->setNativeEntry(original_native, otherBody);
    qore_rt_bind_function_call_ref_typed_v1(bits(*value), original_normal, original_typed, 5);
    CHECK(!ref->getTypedEntry(5));
    ref->setNativeEntry(original_native, original_normal);
    qore_rt_bind_function_call_ref_typed_v1(bits(*value), original_normal, original_typed, 9);
    CHECK(!ref->getTypedEntry(9));
    qore_rt_bind_function_call_ref_typed_v1(bits(*value), original_normal, original_typed, 5);
    qore_rt_bind_function_call_ref_typed_v2(bits(*value), original_normal, original_explicit, 5);
    qore_rt_bind_function_call_ref_typed_v3(bits(*value), original_normal, original_inline, 5);
    CHECK(ref->getTypedEntry(5) == original_typed);
    CHECK(ref->getTypedRuntimeConfigEntry(5) == original_explicit);
    CHECK(ref->getTypedInlineStackEntry(5) == original_inline);
    {
        auto* frame = prepared.frame.frame;
        QoreProgram* caller = rc->getProgram();
        ref->setTypedInlineStackEntry(reinterpret_cast<QoreFunctionRefTypedEntry>(throwIntUnary));
        int64_t result = 123;
        CHECK(call(result) == 1 && result == 0 && xsink);
        CHECK(xsink.getExceptionErr().get<const QoreStringNode>()->equal("CALLBACK-INLINE-THROW"));
        CHECK(rc->getProgram() == caller && prepared.frame.frame == frame);
        xsink.clear();
        ref->setTypedInlineStackEntry(original_inline);
        CHECK(call(result) == 1 && !xsink && result == expected && prepared.frame.frame == frame);
    }
    {
        // Match an importing caller -> internal module loop -> captured caller
        // transition. A cold foreign-context invocation must not retain a cache.
        QoreExternalProgramContextHelper owner_context(&xsink, uvb->pgm);
        CHECK(!xsink && rc->getProgram() == uvb->pgm);
        bool cross = !std::getenv("QORE_DISABLE_AOT_CROSS_PROGRAM_FUNCREF_FAST_CALL");
        Prepared cold(xsink);
        auto cross_call = [&](int64_t& output) {
            return qore_rt_try_funcref_int_int_v9(bits(*value), arg, &output, &xsink, rc,
                &cold.cache, &cold.frame);
        };
        int64_t output = 123;
        CHECK(cross_call(output) == cross && !xsink && output == (cross ? expected : 123));
        CHECK(rc->getProgram() == uvb->pgm && !cold.cache.owner && !cold.frame.frame);
        output = 123;
        CHECK(call(output) == cross && !xsink && output == (cross ? expected : 123));
        CHECK(rc->getProgram() == uvb->pgm && prepared.cache.ref == ref);
        output = 123;
        CHECK(qore_rt_try_funcref_int_int_v2(bits(*value), arg, &output, &xsink, rc) == cross);
        CHECK(!xsink && output == (cross ? expected : 123) && rc->getProgram() == uvb->pgm);
        if (cross) {
            xsink.raiseException("MODULE-CROSS-PENDING", "pending sink must prevent program entry");
            CHECK(cross_call(output) == 1 && output == 0 && xsink);
            CHECK(rc->getProgram() == uvb->pgm);
            xsink.clear();
#if defined(__aarch64__) || defined(__x86_64__)
            if (size_t* limit = const_cast<size_t*>(qore_rt_get_stack_limit_v1(rc))) {
                size_t saved_limit = *limit;
                unsigned errors = 0;
                unsigned successes = 0;
                // Bounded sweep exercises guards on both sides of Program entry.
                for (unsigned gap = 0; gap <= 4096; gap += 64) {
                    *limit = get_stack_pos() - gap;
                    int handled = cross_call(output);
                    *limit = saved_limit;
                    CHECK(handled == 1 && rc->getProgram() == uvb->pgm);
                    if (xsink) {
                        CHECK(output == 0);
                        CHECK(xsink.getExceptionErr().get<const QoreStringNode>()->equal("STACK-LIMIT-EXCEEDED"));
                        xsink.clear();
                        ++errors;
                    } else {
                        CHECK(output == expected);
                        ++successes;
                    }
                }
                CHECK(errors && successes && !cold.cache.owner && !cold.frame.frame);
            }
#endif
        }
        ReferenceHolder<QoreDebugProgram> debugger(new QoreDebugProgram(), &xsink);
        for (QoreProgram* attached : {pgm, uvb->pgm}) {
            debugger->addProgram(attached, &xsink);
            CHECK(!xsink);
            output = 123;
            CHECK(cross_call(output) == 0 && output == 123 && !xsink);
            CHECK(rc->getProgram() == uvb->pgm);
            debugger->removeProgram(attached);
        }
        CHECK(cross_call(output) == cross && !xsink && rc->getProgram() == uvb->pgm);
    }
    CHECK(rc->getProgram() == pgm);
#if defined(__aarch64__) || defined(__x86_64__)
    if (size_t* limit = const_cast<size_t*>(qore_rt_get_stack_limit_v1(rc))) {
        size_t saved_limit = *limit;
        *limit = get_stack_pos() + 4096;
        int64_t overflow_result = 123;
        int handled = call(overflow_result);
        *limit = saved_limit;
        CHECK(handled == 1 && overflow_result == 0 && xsink);
        CHECK(xsink.getExceptionErr().get<const QoreStringNode>()->equal("STACK-LIMIT-EXCEEDED"));
        xsink.clear();
        CHECK(call(overflow_result) == 1 && !xsink && overflow_result == expected);
    }
#endif
    xsink.raiseException("MODULE-CALL-PENDING", "pending sink must prevent the native body");
    int64_t result = 123;
    CHECK(call(result) == 1 && result == 0 && xsink);
    xsink.clear();
    {
        QoreProgramHelper foreign(PO_MODERN, xsink);
        QoreExternalProgramContextHelper context(&xsink, *foreign);
        result = 123;
        CHECK(call(result) == 0 && result == 123 && !xsink);
    }
    // An unshared test reference can simulate a stale binding; no other thread
    // or Program observes this replacement, and the original pair is restored.
    auto native = ref->getNativeEntry();
    auto normal = ref->getNormalEntry();
    ref->setNativeEntry(native, otherBody);
    result = 123;
    CHECK(call(result) == 0 && result == 123 && !xsink);
    ref->setNativeEntry(native, normal);
    result = 123;
    CHECK(call(result) == 1 && result == expected && !xsink);

    ReferenceHolder<QoreDebugProgram> debugger(new QoreDebugProgram(), &xsink);
    for (QoreProgram* attached : {pgm, uvb->pgm}) {
        debugger->addProgram(attached, &xsink);
        CHECK(!xsink && qore_program_private::get(*attached)->hasDebuggerAttached());
        result = 123;
        CHECK(call(result) == 0 && result == 123 && !xsink);
        // Also check lookup before a cache exists, not just invariant hits.
        CHECK(qore_rt_try_funcref_int_int_v2(bits(*value), arg, &result, &xsink, rc) == 0);
        debugger->removeProgram(attached);
        CHECK(!qore_program_private::get(*attached)->hasDebuggerAttached());
        CHECK(call(result) == 1 && result == expected && !xsink);
    }
}

static void checkOther(QoreProgram* pgm, bool enabled) {
    ExceptionSink xsink;
    RuntimeConfig* rc = qore_rt_get_fast_runtime_config();
    for (const char* name : {"ModuleRefBinding::makeFloat", "ModuleRefBinding::makeFloatZero",
            "ModuleRefBinding::makeIntZero"}) {
        ValueHolder value(pgm->callFunction(name, nullptr, &xsink), &xsink);
        CHECK(!xsink && value->getType() == NT_FUNCREF);
        Prepared prepared(xsink);
        for (unsigned i = 0; i < 3; ++i) {
            int handled;
            if (!std::strcmp(name, "ModuleRefBinding::makeIntZero")) {
                int64_t result = 123;
                handled = qore_rt_try_funcref_int_0_v9(bits(*value), &result, &xsink, rc,
                    &prepared.cache, &prepared.frame);
                CHECK(result == (enabled ? 17 : 123));
            } else {
                double result = 123.0;
                bool unary = !std::strcmp(name, "ModuleRefBinding::makeFloat");
                handled = unary
                    ? qore_rt_try_funcref_float_float_v9(bits(*value), 1.25, &result, &xsink, rc,
                        &prepared.cache, &prepared.frame)
                    : qore_rt_try_funcref_float_0_v9(bits(*value), &result, &xsink, rc,
                        &prepared.cache, &prepared.frame);
                CHECK(result == (enabled ? (unary ? 65533.75 : 17.5) : 123.0));
            }
            CHECK(!xsink && handled == enabled);
        }
        if (enabled) {
            auto* ref = dynamic_cast<FunctionCallReferenceNode*>(value->getInternalNode());
            CHECK(ref && prepared.frame.frame);
            bool integer = !std::strcmp(name, "ModuleRefBinding::makeIntZero");
            bool unary = !std::strcmp(name, "ModuleRefBinding::makeFloat");
            uint32_t signature = integer ? 1 : unary ? 10 : 2;
            auto original_inline = ref->getTypedInlineStackEntry(signature);
            CHECK(original_inline);
            auto* frame = prepared.frame.frame;
            QoreProgram* caller = rc->getProgram();
            ref->setTypedInlineStackEntry(integer
                ? reinterpret_cast<QoreFunctionRefTypedEntry>(throwIntZero)
                : unary ? reinterpret_cast<QoreFunctionRefTypedEntry>(throwFloatUnary)
                    : reinterpret_cast<QoreFunctionRefTypedEntry>(throwFloatZero));
            if (integer) {
                int64_t result = 123;
                CHECK(qore_rt_try_funcref_int_0_v9(bits(*value), &result, &xsink, rc,
                    &prepared.cache, &prepared.frame) == 1 && result == 0);
            } else {
                double result = -123.5;
                int handled = unary
                    ? qore_rt_try_funcref_float_float_v9(bits(*value), 1.25, &result, &xsink, rc,
                        &prepared.cache, &prepared.frame)
                    : qore_rt_try_funcref_float_0_v9(bits(*value), &result, &xsink, rc,
                        &prepared.cache, &prepared.frame);
                CHECK(handled == 1 && result == 0.0);
            }
            CHECK(xsink && xsink.getExceptionErr().get<const QoreStringNode>()->equal("CALLBACK-INLINE-THROW"));
            CHECK(rc->getProgram() == caller && prepared.frame.frame == frame);
            xsink.clear();
            ref->setTypedInlineStackEntry(original_inline);
        }
    }
    for (bool unary : {false, true}) {
        ValueHolder value(pgm->callFunction(unary ? "ModuleRefBinding::makeBool"
            : "ModuleRefBinding::makeBoolZero", nullptr, &xsink), &xsink);
        CHECK(!xsink && value->getType() == NT_FUNCREF);
        auto* ref = dynamic_cast<FunctionCallReferenceNode*>(value->getInternalNode());
        CHECK(ref && bool(ref->getFastCallMetadata().uvb) == enabled);
        uint32_t signature = unary ? 15 : 3;
        // A boolean body without a compiler-published scalar entry remains a
        // normal fallback even when its exact signature metadata is present.
        bool bound = ref->getTypedEntry(signature) != nullptr;
        CHECK(!bound || (enabled && ref->getNativeEntry()));
        for (bool argument : {false, true}) {
            int64_t result = 123;
            int handled = unary
                ? qore_rt_try_funcref_bool_bool_v2(bits(*value), argument, &result, &xsink, rc)
                : qore_rt_try_funcref_bool_0_v2(bits(*value), &result, &xsink, rc);
            bool expected = !unary || !argument;
            CHECK(handled == bound && !xsink && result == (bound ? int64_t(expected) : 123));
            ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
            if (unary) {
                args->push(QoreValue(argument), &xsink);
            }
            ValueHolder ordinary(ref->invoke(*args, &xsink), &xsink);
            CHECK(!xsink && ordinary->getType() == NT_BOOLEAN && ordinary->getAsBool() == expected);
        }
    }
    for (const char* name : {"ModuleRefBinding::makeOptional", "ModuleRefBinding::makeTwo",
            "ModuleRefBinding::makeEffect"}) {
        ValueHolder value(pgm->callFunction(name, nullptr, &xsink), &xsink);
        CHECK(!xsink && value->getType() == NT_FUNCREF);
        int64_t result = 123;
        CHECK(qore_rt_try_funcref_int_int_v2(bits(*value), 7, &result, &xsink, rc) == 0);
        CHECK(!xsink && result == 123);
    }
}

static void check(bool enabled) {
    ExceptionSink xsink;
    QoreProgramHelper pgm(PO_MODERN | PO_ALLOW_DEBUGGER, xsink);
    pgm->setExecMode(QEM_IR, true);
    pgm->parse("%modern\n%requires ModuleRefBinding\n", "module reference binding", &xsink);
    // Both source and AOT modules inherit the loading Program's options. Enable
    // this capability after loading so the probe still distinguishes the
    // callback's executing Program from the Program that defined its body.
    if (!xsink) {
        pgm->setParseOptions(PO_ALLOW_INJECTION, &xsink);
    }
    if (xsink) {
        xsink.handleExceptions();
        CHECK(false);
    }
    QoreExternalProgramContextHelper context(&xsink, *pgm);
    checkInt(*pgm, "ModuleRefBinding::makePrivate", 7, 248, enabled);
    checkInt(*pgm, "ModuleRefBinding::makePublic", -7, 65322, enabled);
    checkInt(*pgm, "ModuleRefBinding::makeForwarded", 7, 248, enabled);
    checkInt(*pgm, "ModuleRefBinding::makeIdentity", std::numeric_limits<int64_t>::min(),
        std::numeric_limits<int64_t>::min(), enabled);
    checkInt(*pgm, "ModuleRefBinding::makeIdentity", std::numeric_limits<int64_t>::max(),
        std::numeric_limits<int64_t>::max(), enabled);
    checkOther(*pgm, enabled);
    checkCallerOptions(*pgm, enabled);
    CHECK(!xsink);
}

int main(int argc, char** argv) {
    CHECK(argc == 2);
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    bool enabled = std::atoi(argv[1]) != 0;
    check(enabled);
    // Reacquire native contexts after each foreign-thread registration. All
    // probes are serial, so the unshared binding/debugger instrumentation and
    // assertion counter cannot race with another probe.
    std::thread worker([enabled]() {
        for (unsigned i = 0; i < 3; ++i) {
            QoreForeignThreadHelper foreign;
            check(enabled);
        }
    });
    worker.join();
    qore_cleanup();
    std::printf("module-funcref-binding: %u checks passed\n", checks);
}
