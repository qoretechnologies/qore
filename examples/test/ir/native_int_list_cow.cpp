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

static QoreValue fromBits(uint64_t bits) {
    QoreSimpleValue value;
    value.setRawBits(bits);
    return QoreValue(value);
}

static void check() {
    ExceptionSink xsink;
    const int64 values[] = {0, 1, -1, 140737488355327LL, 140737488355328LL,
        -140737488355328LL, -140737488355329LL,
        std::numeric_limits<int64>::max(), std::numeric_limits<int64>::min()};
    for (int64 value : values) {
        ValueHolder original(QoreValue(value), &xsink);
        AbstractQoreNode* box = original->getInternalNode();
        for (int boxed = 0; boxed < 2; ++boxed) {
            auto append = [&](uint64_t bits) {
                ValueHolder argument(boxed ? original->refSelf() : QoreValue(value), &xsink);
                return qore_rt_list_push_typed(bits, argument->rawBits(), bigIntTypeInfo, &xsink);
            };
            {
                ReferenceHolder<QoreListNode> list(new QoreListNode(bigIntTypeInfo), &xsink);
                const uint64_t bits = QoreValue(*list).rawBits();
                {
                    ValueHolder result(fromBits(append(bits)), &xsink);
                    assert(!xsink && result->rawBits() == bits && list->reference_count() == 2);
                    assert(list->size() == 1 && list->retrieveEntry(0).getAsBigInt() == value);
                    if (boxed) {
                        assert(list->retrieveEntry(0).rawBits() == original->rawBits());
                    }
                }
                assert(list->reference_count() == 1);
                list->setEntry(3, QoreValue(value), &xsink);
                assert(!xsink && list->size() == 4 && list->retrieveEntry(1).isNothing());
                ReferenceHolder<QoreListNode> alias(list->listRefSelf(), &xsink);
                ValueHolder result(fromBits(append(bits)), &xsink);
                QoreListNode* copy = result->get<QoreListNode>();
                assert(!xsink && copy != *list && list->reference_count() == 2);
                assert(copy->reference_count() == 1 && copy->size() == 5 && list->size() == 4);
                assert(copy->getTypeInfo() == list->getTypeInfo());
                assert(copy->retrieveEntry(0).rawBits() == list->retrieveEntry(0).rawBits());
                assert(copy->retrieveEntry(1).isNothing() && copy->retrieveEntry(2).isNothing());
                assert(copy->retrieveEntry(4).getAsBigInt() == value);
                if (boxed) {
                    assert(copy->retrieveEntry(4).rawBits() == original->rawBits());
                }
            }
            assert(!xsink && (!box || box->reference_count() == 1));
            // Reflection can replace a declared list<int> with other runtime
            // metadata. Preserve the generic append's conversion and COW rules.
            for (const QoreTypeInfo* type : {autoTypeInfo, floatTypeInfo}) {
                ReferenceHolder<QoreListNode> list(new QoreListNode(type), &xsink);
                list->push(QoreValue(int64_t{3}), &xsink);
                assert(!xsink);
                ReferenceHolder<QoreListNode> alias(list->listRefSelf(), &xsink);
                ValueHolder result(fromBits(append(QoreValue(*list).rawBits())), &xsink);
                const QoreListNode* copy = result->get<QoreListNode>();
                assert(!xsink && copy != *list && list->size() == 1 && copy->size() == 2);
                assert(copy->getTypeInfo() == list->getTypeInfo());
                if (type == floatTypeInfo) {
                    assert(copy->retrieveEntry(1).getType() == NT_FLOAT);
                    assert(copy->retrieveEntry(1).getAsFloat() == static_cast<double>(value));
                } else {
                    assert(copy->retrieveEntry(1).getType() == NT_INT);
                    assert(copy->retrieveEntry(1).getAsBigInt() == value);
                }
            }
            assert(!xsink && (!box || box->reference_count() == 1));
            {
                ReferenceHolder<QoreListNode> list(new QoreListNode(bigIntTypeInfo), &xsink);
                xsink.raiseException("PREEXISTING", "retain generic push sink behavior");
                ValueHolder result(fromBits(append(QoreValue(*list).rawBits())), &xsink);
                assert(xsink && result->isNothing() && list->size() == 1);
                assert(list->reference_count() == 1);
                xsink.clear();
            }
            assert(!xsink && (!box || box->reference_count() == 1));
            {
                ValueHolder result(fromBits(append(QoreValue().rawBits())), &xsink);
                QoreListNode* list = result->get<QoreListNode>();
                assert(!xsink && list->reference_count() == 1 && list->size() == 1);
                assert(list->retrieveEntry(0).getAsBigInt() == value);
                assert(list->getTypeInfo() == qore_get_complex_list_type(bigIntTypeInfo));
            }
            assert(!xsink && (!box || box->reference_count() == 1));
        }
    }
}

int main() {
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    check();
    qore_cleanup();
    std::puts("native-int-list-cow: ok");
}
