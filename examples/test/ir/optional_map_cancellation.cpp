/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
#include <qore/Qore.h>
// Internal runtime declarations also refer to these opaque AOT types.
struct QoreAOTContext;
struct QoreTypeParamInstantiation;
#include <qore/intern/JITRuntime.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <cstring>

static uint64_t toBits(const QoreValue& value) {
    static_assert(sizeof(value) == sizeof(uint64_t));
    uint64_t result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

static QoreValue fromBits(uint64_t bits) {
    QoreValue result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

static void checkScannedTypeCancellation() {
    // Exercise each IR/JIT/AOT helper directly so no language-level checkpoint
    // can consume the cancellation before the native scan.
    for (int helper = 0; helper < 3; ++helper) {
        ExceptionSink xsink;
        ReferenceHolder<QoreListNode> input(new QoreListNode(autoTypeInfo), &xsink);
        for (int i = 0; i < 205; ++i) {
            if (i && !(i % 100)) {
                assert(!qore_check_cancel(&xsink, "select type scan test setup"));
            }
            input->push(QoreValue(static_cast<int64_t>(i)), &xsink);
            assert(!xsink);
        }
        const QoreTypeInfo* original_type = input->getTypeInfo();
        auto scan = [&]() {
            uint64_t bits = toBits(QoreValue(*input));
            switch (helper) {
                case 0:
                    qore_rt_list_set_scanned_type(bits, autoTypeInfo, &xsink);
                    break;
                case 1:
                    qore_rt_list_set_scanned_type_by_type_path(bits, nullptr, 0, &xsink);
                    break;
                default:
                    qore_rt_list_set_scanned_type_from_source(bits, bits, &xsink);
                    break;
            }
        };
        assert(!qore_cancel_thread(q_gettid(), "select type scan cancellation test"));
        scan();
        assert(xsink);
        assert(xsink.getExceptionErr().get<const QoreStringNode>()->equal("THREAD-CANCELLED"));
        assert(input->getTypeInfo() == original_type && input->size() == 205);
        assert(input->retrieveEntry(204).getAsBigInt() == 204);
        qore_clear_thread_cancel();
        xsink.clear();
        scan();
        assert(!xsink && input->getValueTypeInfo() == bigIntTypeInfo);
    }
}

static void checkCancellation() {
    ExceptionSink xsink;
    ReferenceHolder<QoreHashNode> item(new QoreHashNode(autoTypeInfo), &xsink);
    item->setKeyValue("identity", QoreValue::makeStringValue("decimal"), &xsink);
    assert(!xsink);
    ReferenceHolder<QoreListNode> input(new QoreListNode(autoTypeInfo), &xsink);
    for (int i = 0; i < 205; ++i) {
        if (i && !(i % 100)) {
            assert(!qore_check_cancel(&xsink, "map test setup"));
        }
        input->push(item->refSelf(), &xsink);
        assert(!xsink);
    }
    // Request cancellation before directly entering the native loop: no Qore
    // statement, thread scheduling race or polling can consume it first.
    assert(!qore_cancel_thread(q_gettid(), "map cancellation test"));
    QoreValue cancelled = fromBits(qore_rt_map_hash_key_value(toBits(QoreValue(*input)), "identity", &xsink));
    assert(cancelled.isNothing() && xsink);
    assert(xsink.getExceptionErr().get<const QoreStringNode>()->equal("THREAD-CANCELLED"));
    qore_clear_thread_cancel();
    xsink.clear();
    QoreValue restored = fromBits(qore_rt_map_hash_key_value(toBits(QoreValue(*input)), "identity", &xsink));
    assert(!xsink && restored.getType() == NT_LIST);
    const QoreListNode* list = restored.get<const QoreListNode>();
    assert(list->size() == 205);
    assert(list->retrieveEntry(204).get<const QoreStringNode>()->equal("decimal"));
    restored.discard(&xsink);
    assert(!xsink);
}

int main() {
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    checkCancellation();
    checkScannedTypeCancellation();
    qore_cleanup();
    std::puts("optional-map-cancellation: ok");
}
