/* Copyright (c) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
#include <qore/Qore.h>
struct QoreAOTContext;
struct QoreTypeParamInstantiation;
#include <qore/intern/JITRuntime.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <limits>

static void check() {
    ExceptionSink xsink;
    const int64 values[] = {0, 1, -1, 140737488355327LL, 140737488355328LL,
        -140737488355328LL, -140737488355329LL,
        std::numeric_limits<int64>::max(), std::numeric_limits<int64>::min()};
    for (int64 value : values) {
        ValueHolder original(QoreValue(value), &xsink);
        AbstractQoreNode* box = original->getInternalNode();
        if (box) {
            assert(box->reference_count() == 1);
        }
        {
            QoreListNode* ptr = new QoreListNode(bigIntTypeInfo);
            ReferenceHolder<QoreListNode> list(ptr, &xsink);
            uint64_t list_bits = QoreValue(ptr).rawBits();
            assert(qore_rt_list_push_int_in_place_unchecked_v1(list_bits, value) == list_bits);
            assert(qore_rt_list_push_int_bits_in_place_unchecked_v1(list_bits, original->rawBits()) == list_bits);
            assert(qore_rt_list_push_int_bits_in_place_unchecked_v1(list_bits, original->rawBits()) == list_bits);
            assert(!xsink && ptr->reference_count() == 1 && ptr->size() == 3);
            assert(ptr->retrieveEntry(0).getAsBigInt() == value);
            assert(ptr->retrieveEntry(1).rawBits() == original->rawBits());
            assert(ptr->retrieveEntry(2).rawBits() == original->rawBits());
            if (box) {
                assert(box->reference_count() == 3);
            }
        }
        assert(!xsink);
        if (box) {
            assert(box->reference_count() == 1);
        }
        // Releasing the source first must also leave the list's value alive.
        {
            QoreListNode* ptr = new QoreListNode(bigIntTypeInfo);
            ReferenceHolder<QoreListNode> list(ptr, &xsink);
            assert(qore_rt_list_push_int_bits_in_place_unchecked_v1(
                QoreValue(ptr).rawBits(), original->rawBits()) == QoreValue(ptr).rawBits());
            original.release().discard(&xsink);
            assert(!xsink && ptr->retrieveEntry(0).getAsBigInt() == value);
            if (box) {
                assert(box->reference_count() == 1);
            }
        }
        assert(!xsink);
    }
}

int main() {
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    check();
    qore_cleanup();
    std::puts("native-int-list-push: ok");
}
