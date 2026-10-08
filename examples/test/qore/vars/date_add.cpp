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

static void check(const DateTime& left, const DateTime& right, const DateTime& expected) {
    const DateTime original_left(left);
    const DateTime original_right(right);
    for (bool pointer : {false, true}) {
        std::unique_ptr<DateTime> result(pointer ? left.add(&right) : left.add(right));
        require(result->isEqual(expected), "DateTime::add returned the wrong date");
        require(result->isRelative() == expected.isRelative(), "incorrect result kind");
        require(result->getZone() == expected.getZone(), "incorrect result timezone");
        require(left.isEqual(original_left) && right.isEqual(original_right), "an input was mutated");
        require(result.get() != &left && result.get() != &right, "result aliases an input");
        ++cases;
    }
}

static void epochCases() {
    // An independent integer-microsecond oracle covers negative epochs, both carry
    // directions, zero operands and all four absolute/relative operand combinations.
    for (int64 seconds : {-86401LL, -1LL, 0LL, 1LL, 1790000000LL}) {
        for (int micros : {0, 1, 999999}) {
            std::unique_ptr<DateTime> absolute(DateTime::makeAbsolute(nullptr, seconds, micros));
            for (int64 delta : {-86400LL, -1LL, 0LL, 1LL, 86400LL}) {
                for (int delta_us : {-999999, 0, 999999}) {
                    std::unique_ptr<DateTime> relative(DateTime::makeRelativeFromSeconds(delta, delta_us));
                    const int64 total = seconds * 1000000 + micros + delta * 1000000 + delta_us;
                    int64 expected_seconds = total / 1000000;
                    int expected_us = static_cast<int>(total % 1000000);
                    if (expected_us < 0) {
                        --expected_seconds;
                        expected_us += 1000000;
                    }
                    std::unique_ptr<DateTime> expected(DateTime::makeAbsolute(nullptr,
                        expected_seconds, expected_us));
                    check(*absolute, *relative, *expected);
                    check(*relative, *absolute, *expected);

                    std::unique_ptr<DateTime> other_absolute(DateTime::makeAbsolute(nullptr, delta, delta_us));
                    // The shared date engine expresses the sum of two epoch
                    // offsets in the current default timezone.
                    std::unique_ptr<DateTime> expected_absolute(DateTime::makeAbsolute(currentTZ(),
                        expected_seconds, expected_us));
                    check(*absolute, *other_absolute, *expected_absolute);
                    std::unique_ptr<DateTime> left_relative(DateTime::makeRelativeFromSeconds(seconds, micros));
                    std::unique_ptr<DateTime> expected_relative(DateTime::makeRelativeFromSeconds(
                        total / 1000000, static_cast<int>(total % 1000000)));
                    check(*left_relative, *relative, *expected_relative);
                }
            }
        }
    }
}

static void calendarCases() {
    struct CalendarCase {
        const char* start;
        int years;
        int months;
        int days;
        const char* expected;
    };
    for (const auto& row : {
            CalendarCase{"2024-02-28T12:30:00.123456Z", 0, 0, 1, "2024-02-29T12:30:00.123456Z"},
            CalendarCase{"2024-03-01T12:30:00.123456Z", 0, 0, -1, "2024-02-29T12:30:00.123456Z"},
            CalendarCase{"2026-01-15T08:00:00+05:30", 0, 1, 0, "2026-02-15T08:00:00+05:30"},
            CalendarCase{"2026-01-15T08:00:00-04:00", 0, -1, 0, "2025-12-15T08:00:00-04:00"},
            CalendarCase{"2026-07-15T08:00:00Z", 1, 0, 0, "2027-07-15T08:00:00Z"},
            CalendarCase{"2026-07-15T08:00:00Z", -1, 0, 0, "2025-07-15T08:00:00Z"}}) {
        DateTime start(nullptr, row.start);
        DateTime expected(nullptr, row.expected);
        std::unique_ptr<DateTime> delta(DateTime::makeRelative(row.years, row.months, row.days));
        check(start, *delta, expected);
        check(*delta, start, expected);
    }
    // Self-addition exercises pointer aliasing without changing either source.
    std::unique_ptr<DateTime> value(DateTime::makeAbsolute(nullptr, int64(1), 750000));
    std::unique_ptr<DateTime> doubled(DateTime::makeAbsolute(currentTZ(), int64(3), 500000));
    check(*value, *value, *doubled);
    std::unique_ptr<DateTime> relative(DateTime::makeRelativeFromSeconds(-1, -750000));
    std::unique_ptr<DateTime> relative_doubled(DateTime::makeRelativeFromSeconds(-3, -500000));
    check(*relative, *relative, *relative_doubled);

    ExceptionSink xsink;
    const AbstractQoreZoneInfo* zone = find_create_timezone("Europe/Prague", &xsink);
    require(!xsink && zone, "cannot load calendar timezone fixture");
    for (const auto& row : {CalendarCase{"2026-03-28T12:00:00", 0, 0, 1, "2026-03-29T12:00:00"},
            CalendarCase{"2026-10-24T12:00:00", 0, 0, 1, "2026-10-25T12:00:00"}}) {
        DateTime start(zone, row.start);
        DateTime expected(zone, row.expected);
        std::unique_ptr<DateTime> day(DateTime::makeRelative(0, 0, 1));
        check(start, *day, expected);
        check(*day, start, expected);
    }
}

int main() {
    qore_init(QL_MIT, "UTF-8", false, QLO_DISABLE_SIGNAL_HANDLING);
    int status = 0;
    try {
        epochCases();
        calendarCases();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL after %u cases: %s\n", cases, error.what());
        status = 1;
    }
    qore_cleanup();
    if (!status) {
        std::printf("PASS: %u DateTime addition cases\n", cases);
    }
    return status;
}
