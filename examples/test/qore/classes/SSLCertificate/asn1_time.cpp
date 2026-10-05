/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
#include <qore/Qore.h>
#include <qore/QoreSSLBase.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>

static void check(int type, const char* text, const char* expected, int length = -1) {
    std::unique_ptr<ASN1_STRING, decltype(&ASN1_STRING_free)> time(ASN1_STRING_type_new(type), ASN1_STRING_free);
    assert(time);
    if (length < 0) {
        length = static_cast<int>(strlen(text));
    }
    assert(ASN1_STRING_set(time.get(), text, length));
    ExceptionSink xsink;
    ReferenceHolder<DateTimeNode> actual(QoreSSLBase::ASN1_TIME_to_DateTime(time.get()), &xsink);
    if (!expected) {
        assert(!actual);
        return;
    }
    ReferenceHolder<DateTimeNode> reference(new DateTimeNode(nullptr, expected, &xsink), &xsink);
    if (!actual) {
        std::fprintf(stderr, "conversion failed for %s\n", text);
    }
    assert(!xsink && actual);
    if (actual->getEpochSecondsUTC() != reference->getEpochSecondsUTC()
            || actual->getMicrosecond() != reference->getMicrosecond()) {
        std::fprintf(stderr, "unexpected conversion for %s: %lld.%06d instead of %lld.%06d\n", text,
            static_cast<long long>(actual->getEpochSecondsUTC()), actual->getMicrosecond(),
            static_cast<long long>(reference->getEpochSecondsUTC()), reference->getMicrosecond());
    }
    assert(actual->getEpochSecondsUTC() == reference->getEpochSecondsUTC());
    assert(actual->getMicrosecond() == reference->getMicrosecond());
    assert(!actual->getZone());
    // Conversion must neither mutate the input nor assume its logical length includes a NUL.
    assert(ASN1_STRING_type(time.get()) == type);
    assert(ASN1_STRING_length(time.get()) == length);
}

int main() {
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    check(V_ASN1_UTCTIME, "261005070000Z", "2026-10-05T07:00:00Z");
    assert(!QoreSSLBase::ASN1_TIME_to_DateTime(nullptr));
    check(V_ASN1_UTCTIME, "500101000000Z", "1950-01-01T00:00:00Z");
    check(V_ASN1_UTCTIME, "491231235959Z", "2049-12-31T23:59:59Z");
    check(V_ASN1_GENERALIZEDTIME, "20510101000000Z", "2051-01-01T00:00:00Z");
    check(V_ASN1_GENERALIZEDTIME, "19490101000000Z", "1949-01-01T00:00:00Z");
    check(V_ASN1_GENERALIZEDTIME, "20510101000000.123456Z", "2051-01-01T00:00:00.123456Z");
    check(V_ASN1_UTCTIME, "261005070000+0200", "2026-10-05T05:00:00Z");
    check(V_ASN1_GENERALIZEDTIME, "20510101000000-0500", "2051-01-01T05:00:00Z");
    check(V_ASN1_UTCTIME, "2610050700-0500", "2026-10-05T12:00:00Z");
    check(V_ASN1_GENERALIZEDTIME, "205101010000+0530", "2050-12-31T18:30:00Z");
    check(V_ASN1_UTCTIME, "261005070000Zignored", "2026-10-05T07:00:00Z", 13);
    check(V_ASN1_UTCTIME, "261005070000Z", nullptr, 12);
    check(V_ASN1_UTCTIME, "261005070000Z\0ignored", nullptr, 21);
    check(V_ASN1_UTCTIME, "", nullptr);
    check(V_ASN1_UTCTIME, "261305070000Z", nullptr);
    check(V_ASN1_UTCTIME, "260230070000Z", nullptr);
    check(V_ASN1_GENERALIZEDTIME, "20510229000000Z", nullptr);
    check(V_ASN1_GENERALIZEDTIME, "20510101000000Zjunk", nullptr);
    check(V_ASN1_OCTET_STRING, "20510101000000Z", nullptr);
    qore_cleanup();
    std::puts("ASN.1 time conversion: OK");
}
