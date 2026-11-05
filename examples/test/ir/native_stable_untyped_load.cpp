/* Copyright (c) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
#include <qore/Qore.h>
#include <qore/ReferenceArgumentHelper.h>
#include <qore/QoreTypeSafeReferenceHelper.h>
struct QoreAOTContext;
struct QoreTypeParamInstantiation;
#include <qore/intern/JITRuntime.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <cstring>

extern "C" uint64_t qore_rt_deref_if_reference(uint64_t, ExceptionSink*);

static uint64_t classifiedRead(uint64_t val, uint8_t* non_reference, ExceptionSink* xsink) {
    uint64_t reference_result;
    *non_reference = qore_rt_deref_stable_local_v2(val, &reference_result, xsink);
    return *non_reference ? val : reference_result;
}

static QoreValue fromBits(uint64_t bits) {
    QoreValue value;
    static_assert(sizeof(value) == sizeof(bits));
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

static void check() {
    ExceptionSink xsink;
    QoreProgramHelper program(QoreParseOptions{}, xsink);
    QoreExternalProgramContextHelper context(&xsink, *program);
    assert(!xsink);
    ReferenceArgumentHelper reference(QoreValue(7), &xsink);
    assert(!xsink);
    ValueHolder input(QoreValue(reference.getArg()), &xsink);
    const auto* node = input->get<const ReferenceNode>();
    uint8_t non_reference = 0;
    for (int value = 7; value < 12; ++value) {
        {
            QoreTypeSafeReferenceHelper writer(node, &xsink);
            assert(writer && !xsink);
            assert(!writer.assign(QoreValue(value)));
        }
        assert(!xsink);
        ValueHolder actual(fromBits(classifiedRead(input->rawBits(),
            &non_reference, &xsink)), &xsink);
        assert(!xsink && !non_reference && actual->getAsBigInt() == value);
        ValueHolder original(fromBits(qore_rt_deref_if_reference(input->rawBits(), &xsink)), &xsink);
        assert(!xsink && original->getAsBigInt() == value);
    }
    {
        QoreTypeSafeReferenceHelper writer(node, &xsink);
        assert(writer && !xsink);
        assert(!writer.assign(QoreValue(new QoreStringNode("an owned reference target"))));
    }
    assert(!xsink);
    ValueHolder result(fromBits(classifiedRead(input->rawBits(), &non_reference, &xsink)), &xsink);
    assert(!xsink && !non_reference && result->get<const QoreStringNode>()->equal("an owned reference target"));
    {
        QoreTypeSafeReferenceHelper writer(node, &xsink);
        assert(writer && !xsink);
        assert(!writer.assign(QoreValue()));
    }
    assert(!xsink && result->get<const QoreStringNode>()->equal("an owned reference target"));
    ValueHolder absent(fromBits(classifiedRead(input->rawBits(), &non_reference, &xsink)), &xsink);
    assert(!xsink && !non_reference && absent->isNothing());
    QoreValue scalar(42);
    assert(classifiedRead(scalar.rawBits(), &non_reference, &xsink) == scalar.rawBits());
    assert(!xsink && non_reference == 1);
    uint64_t untouched = 123;
    assert(qore_rt_deref_stable_local_v2(scalar.rawBits(), &untouched, &xsink) == 1);
    assert(!xsink && untouched == 123);
}

int main() {
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    check();
    qore_cleanup();
    std::puts("stable-untyped-load-native: ok");
}
