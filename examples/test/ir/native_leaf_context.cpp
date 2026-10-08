/* Copyright (c) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
struct QoreAOTContext;
struct QoreTypeParamInstantiation;
#include <qore/intern/JITRuntime.h>
const qore_class_private* runtime_get_class();
#include <qore/intern/CallReferenceNode.h>

extern "C" void init_NATIVE_LEAF_CONTEXT_qo(QoreProgram*);

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

struct LeafInvocation {
    uint64_t bits = 0;
    QorePreparedFunctionRefV3* cache = nullptr;
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

// Exercise a warmed native method PIC through its real compiled caller, including
// early rejection, published method overflow, and successful reuse afterward.
static void checkMethod(QoreProgram* program) {
    ExceptionSink xsink;
    ValueHolder object(program->callFunction("makeMethodProbe", nullptr, &xsink), &xsink);
    assert(!xsink && object->getType() == NT_OBJECT);
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
    args->push(object->refSelf(), &xsink);
    assert(!xsink);
    args->push(QoreValue(int64_t{7}), &xsink);
    assert(!xsink);
    for (unsigned i = 0; i < 16; ++i) {
        ValueHolder result(program->callFunction("methodProbe", *args, &xsink), &xsink);
        assert(!xsink && result->getAsBigInt() == 24);
    }
    RuntimeConfig* rc = qore_rt_get_fast_runtime_config();
    QoreProgram* caller = qore_get_call_program_context();
    size_t* limit = const_cast<size_t*>(qore_rt_get_stack_limit_v1(rc));
#if defined(__aarch64__) || defined(__x86_64__)
    if (limit) {
        const size_t saved_limit = *limit;
        unsigned observed_method = 0;
        unsigned observed_early = 0;
        // Include the compiled caller and public Program entry in the sweep.
        // Restore the real limit before inspecting diagnostics or cleanup.
        for (unsigned gap = 0; gap < 32768; gap += 16) {
            *limit = get_stack_pos() - gap;
            QoreValue value = program->callFunction("methodProbe", *args, &xsink);
            *limit = saved_limit;
            ValueHolder result(value, &xsink);
            assert(qore_get_call_program_context() == caller);
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
            }
            if (!(gap % 160)) {
                assert(!qore_check_cancel(&xsink, "native method guard sweep"));
            }
        }
        assert(observed_method && observed_early);
    }
#else
    assert(!limit);
#endif
    ValueHolder result(program->callFunction("methodProbe", *args, &xsink), &xsink);
    assert(!xsink && result->getAsBigInt() == 24 && qore_get_call_program_context() == caller);
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

static void check(bool inline_entry) {
    ExceptionSink xsink;
    QoreProgramHelper program(PO_MODERN, xsink);
    LeafInvocation invocation;
    {
        ProgramRuntimeExternalParseContextHelper parse_context(*program);
        assert(parse_context);
        program->getRootNS()->addBuiltinVariant(&invocation, "runLeafSweep", paddedSweep,
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
    checkMethod(*program);
    checkFloat(*program, inline_entry);
    invocation.bits = bits;
    invocation.cache = &cache;
    // Keep parsed callers' variables on TLS, independently of the native leaf.
    program->setExecMode(QEM_AST, true);
    // Vary both live locals and captures over a complete marker block, verifying
    // that caller values and both variable stacks survive materialized failures.
    std::string source = "%modern\n";
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
        source += "; };\nint total = runLeafSweep();\nif (closure() != seed * "
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
    assert(argc == 2);
    bool inline_entry = !std::strcmp(argv[1], "inline");
    assert(inline_entry || !std::strcmp(argv[1], "eager"));
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    check(inline_entry);
    std::thread worker([inline_entry]() {
        for (unsigned i = 0; i < 3; ++i) {
            QoreForeignThreadHelper foreign;
            check(inline_entry);
        }
    });
    worker.join();
    qore_cleanup();
    std::puts("native-leaf-context: ok");
}
