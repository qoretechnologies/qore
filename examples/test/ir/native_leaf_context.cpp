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
#include <thread>

#if defined(__aarch64__)
#include <qore/macros-aarch64.h>
#elif defined(__x86_64__)
#include <qore/macros-x86_64.h>
#endif

static void check(bool inline_entry) {
    ExceptionSink xsink;
    QoreProgramHelper program(PO_MODERN, xsink);
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
