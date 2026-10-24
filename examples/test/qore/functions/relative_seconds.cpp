/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <qore/Qore.h>

#include <cstdio>
#include <memory>
#include <stdexcept>

static unsigned cases = 0;

static void require(bool valid, const char* message) {
    if (!valid) {
        throw std::runtime_error(message);
    }
}

static void check(const DateTime& value, int64 expected_us) {
    require(value.isRelative(), "expected a relative date");
    require(value.getRelativeMicroseconds() == expected_us, "incorrect duration");
    require(!value.getYear() && !value.getMonth() && !value.getDay(), "elapsed time became calendar time");
    require(value.getHour() == expected_us / 3600000000LL, "incorrect elapsed hours");
    require(value.getMinute() == (expected_us / 60000000) % 60, "incorrect minutes");
    require(value.getSecond() == (expected_us / 1000000) % 60, "incorrect seconds");
    require(value.getMicrosecond() == expected_us % 1000000, "incorrect microseconds");
    ++cases;
}

static void secondsCases() {
    std::unique_ptr<DateTime> origin(DateTime::makeAbsolute(nullptr, int64(1790000000), 123456));
    // An independent microsecond oracle stays within both the int hour and int64 duration ranges.
    for (int64 seconds : {3600000000LL, 0LL, 1LL, -1LL, 60LL, -60LL, 3600LL, -3600LL, 86400LL, -86400LL,
            2147483647LL, 2147483648LL, 2147486400LL, 4294967296LL,
            -2147483647LL, -2147483648LL, -2147486400LL, -3600000000LL, -4294967296LL,
            7730941129200LL, -7730941132800LL}) {
        for (int micros : {-2147483647 - 1, -1000001, -999999, -1, 0, 1, 999999, 1000001, 2147483647}) {
            const int64 expected_us = seconds * 1000000 + micros;
            std::unique_ptr<DateTime> value(DateTime::makeRelativeFromSeconds(seconds, micros));
            check(*value, expected_us);
            SimpleRefHolder<DateTimeNode> node(DateTimeNode::makeRelativeFromSeconds(seconds, micros));
            check(**node, expected_us);
            DateTime assigned(false);
            assigned.setRelativeDateSeconds(seconds, micros);
            check(assigned, expected_us);
        }
        QoreValue input(seconds);
        DateTimeValueHelper value(input);
        check(**value, seconds * 1000000);
        DateTimeNodeValueHelper node(input);
        check(**node, seconds * 1000000);
        DateTime converted(input);
        check(converted, seconds * 1000000);
        SimpleRefHolder<DateTimeNode> converted_node(new DateTimeNode(input));
        check(**converted_node, seconds * 1000000);
        DateTime added(true);
        added.addSecondsTo(seconds);
        check(added, seconds * 1000000);
        std::unique_ptr<DateTime> sum(origin->add(converted));
        require(sum->getEpochSecondsUTC() - origin->getEpochSecondsUTC() == seconds,
            "absolute-date addition lost elapsed seconds");
        require(sum->getMicrosecond() == 123456, "absolute-date addition lost microseconds");
        ++cases;
    }
    for (double seconds : {2147483648.25, 3600000000.5, -2147483648.25, -3600000000.5}) {
        QoreValue input(seconds);
        const int64 expected_us = static_cast<int64>(seconds * 1000000);
        DateTimeValueHelper value(input);
        check(**value, expected_us);
        DateTimeNodeValueHelper node(input);
        check(**node, expected_us);
    }
    // The fraction brings these inputs back inside the normalized hour range before it is narrowed.
    for (int64 seconds : {7730941132800LL, -7730941136400LL}) {
        int micros = seconds > 0 ? -1 : 1;
        int64 expected_us = seconds * 1000000 + micros;
        std::unique_ptr<DateTime> value(DateTime::makeRelativeFromSeconds(seconds, micros));
        check(*value, expected_us);
        SimpleRefHolder<DateTimeNode> node(DateTimeNode::makeRelativeFromSeconds(seconds, micros));
        check(**node, expected_us);
        DateTime assigned(false);
        assigned.setRelativeDateSeconds(seconds, micros);
        check(assigned, expected_us);
    }
}

int main() {
    qore_init(QL_MIT, "UTF-8", false, QLO_DISABLE_SIGNAL_HANDLING);
    int status = 0;
    try {
        secondsCases();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL after %u cases: %s\n", cases, error.what());
        status = 1;
    }
    qore_cleanup();
    if (!status) {
        std::printf("PASS: %u relative-seconds cases\n", cases);
    }
    return status;
}
