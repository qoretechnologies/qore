# probes of relative-date-range.qtest (qore#6101): the functions that make a relative date raise INVALID-DATE for a
# value that does not fit the date's 32-bit components instead of wrapping; probes() returns one line per pass, so
# that the code runs on every tier with tiered execution

%modern

string sub try_date(code c) {
    try {
        return sprintf("%y", c());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub probes() {
    int big = 9223372036855;
    list<string> l;
    # out of range: the argument (or its whole hours) does not fit an int
    l += try_date(date sub () { return seconds(big); });
    l += try_date(date sub () { return seconds(7730941132800); });
    l += try_date(date sub () { return seconds(-7730941136400); });
    l += try_date(date sub () { return minutes(128849018880); });
    l += try_date(date sub () { return hours(2147483648); });
    l += try_date(date sub () { return hours(-2147483649); });
    l += try_date(date sub () { return days(2147483648); });
    l += try_date(date sub () { return months(2147483648); });
    l += try_date(date sub () { return years(-2147483649); });
    l += try_date(date sub () { return milliseconds(9223372036854775807); });
    l += try_date(date sub () { return microseconds(-9223372036854775807); });
    # in range, at the limits
    l += try_date(date sub () { return seconds(7730941132799); });
    l += try_date(date sub () { return seconds(-7730941132800); });
    l += try_date(date sub () { return minutes(2147483649); });
    l += try_date(date sub () { return hours(2147483647); });
    l += try_date(date sub () { return years(-2147483648); });
    l += try_date(date sub () { return milliseconds(-1500); });
    l += try_date(date sub () { return microseconds(1500); });
    return l.join(",");
}
