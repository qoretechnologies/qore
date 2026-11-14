# probes of relative-date-overflow.qtest (qore#6103): date arithmetic and conversions whose result does not fit the
# 32-bit components of a relative date (or the year of an absolute date) raise INVALID-DATE instead of wrapping, and
# relative date literals out of range are parse errors; probes() returns one line per probe, and is called once per
# pass, so that the code runs on every tier with tiered execution

%modern

# the result of a probe: the value, or the error code of the exception
string sub try_value(code c) {
    try {
        return sprintf("%y", c());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# the result of parsing and running a function that returns the expression: its value, or the parse error and the
# message given for it
string sub try_parse(string expr) {
    try {
        Program p(PO_NEW_STYLE);
        p.parse(sprintf("auto sub t() { return %s; }", expr), "literal");
        return sprintf("%y", p.callFunction("t"));
    } catch (hash<ExceptionInfo> ex) {
        return sprintf("%s: %s", ex.err, ex.desc);
    }
}

list<string> sub probes() {
    # operands in variables, so that the operations are evaluated at run time and not folded at parse time
    date maxh = hours(2147483647);
    date minh = hours(-2147483648);
    date maxy = years(2147483647);
    date mind = days(-2147483648);
    date absd = 2026-01-01T00:00:00Z;
    date big_abs = date("9999999-01-01T00:00:00Z");
    date first_abs = 0001-01-01T00:00:00Z;
    int big = 9223372036855;
    int maxint = 9223372036854775807;
    float bigf = 9223372036855.0;
    float hugef = 1e30;
    float nan = float("nan");
    date big2e9 = date("2000000000-01-01T00:00:00Z");
    date neg15e8 = date("-1500000000-01-01T00:00:00Z");
    date pos15e8 = date("1500000000-01-01T00:00:00Z");
    date minyear = date("-2147483647-01-01T00:00:00Z");
    float inf = float("inf");
    number bign = 2n;
    list<string> l;

    # relative + relative
    l += "rel+rel hours: " + try_value(date sub () { return maxh + hours(1); });
    l += "rel+rel carry into hours: " + try_value(date sub () { return maxh + minutes(59) + minutes(1); });
    l += "rel+rel in range: " + try_value(date sub () { return maxh + minutes(59); });
    l += "rel+rel carry into years: " + try_value(date sub () { return maxy + months(11) + months(1); });
    l += "rel+rel days: " + try_value(date sub () { return mind + days(-1); });
    # relative - relative
    l += "rel-rel hours: " + try_value(date sub () { return minh - hours(1); });
    l += "rel-rel in range: " + try_value(date sub () { return minh - minutes(1); });
    l += "rel-rel days: " + try_value(date sub () { return mind - days(1); });
    l += "rel-rel years: " + try_value(date sub () { return maxy - years(-1); });
    # absolute + relative
    l += "abs+rel years: " + try_value(date sub () { return absd + maxy; });
    l += "abs+rel in range: " + try_value(date sub () { return absd + years(1000000); });
    l += "abs+rel hours in range: " + try_value(date sub () { return absd + maxh; });
    # absolute - relative
    l += "abs-rel years: " + try_value(date sub () { return absd - maxy - maxy; });
    l += "abs-rel years in range: " + try_value(date sub () { return absd - maxy; });
    l += "abs-rel smallest hours: " + try_value(date sub () { return absd - minh; });
    # absolute - absolute
    l += "abs-abs: " + try_value(date sub () { return big_abs - first_abs; });
    l += "abs-abs in range: " + try_value(date sub () { return 2026-01-01T00:00:00Z - first_abs; });
    # absolute + absolute: the sum of the epoch offsets
    l += "abs+abs: " + try_value(date sub () { return big2e9 + big2e9; });
    l += "abs+abs negative: " + try_value(date sub () { return -neg15e8 + pos15e8; });
    l += "abs+abs int64: " + try_value(date sub () { return localtime(maxint) + localtime(10); });
    # in variables: the sum is in the current time zone, which a sum folded at parse time would take from the
    # compiler (qcc)
    date day2 = 1970-01-02T00:00:00Z;
    date hour1 = 1970-01-01T01:00:00.5Z;
    l += "abs+abs in range: " + try_value(date sub () { return day2 + hour1; });
    # an absolute date out of range plus days
    l += "abs out of range+days: " + try_value(date sub () { return localtime(maxint) + days(1); });
    # unary minus of an absolute date: the range of the epoch offsets is not symmetric
    l += "-abs smallest year: " + try_value(date sub () { return -minyear; });
    l += "-abs in range: " + try_value(date sub () { return -1970-01-01T00:00:01.500000Z; });
    # unary minus
    l += "-smallest hours: " + try_value(date sub () { return -minh; });
    l += "-largest hours: " + try_value(date sub () { return -maxh; });
    # += and -=
    l += "+= hours: " + try_value(date sub () { date d = maxh; d += hours(1); return d; });
    l += "-= hours: " + try_value(date sub () { date d = minh; d -= hours(1); return d; });
    l += "+= int: " + try_value(date sub () { date d = maxh; d += 3600; return d; });
    l += "+= int out of range: " + try_value(date sub () { date d = absd; d += big; return d; });
    l += "-= int in range: " + try_value(date sub () { date d = maxh; d -= 3600; return d; });
    # a date and an int
    l += "rel+int: " + try_value(date sub () { return maxh + 3600; });
    l += "int-rel: " + try_value(date sub () { return big - hours(1); });
    # multiplication
    l += "rel*int: " + try_value(date sub () { return maxh * 2; });
    l += "int*rel: " + try_value(date sub () { return 2 * maxh; });
    l += "rel*float: " + try_value(date sub () { return maxh * 1.5; });
    l += "rel*number: " + try_value(date sub () { return maxh * bign; });
    l += "rel*nan: " + try_value(date sub () { return hours(1) * nan; });
    l += "rel*inf: " + try_value(date sub () { return hours(1) * inf; });
    l += "rel*maxint: " + try_value(date sub () { return hours(1) * maxint; });
    l += "rel*int in range: " + try_value(date sub () { return hours(2) * -1073741824; });
    # date(int) and date(float)
    l += "date(int): " + try_value(date sub () { return date(big); });
    l += "date(int) in range: " + try_value(date sub () { return date(big - 1536207867256); });
    l += "date(float): " + try_value(date sub () { return date(bigf); });
    l += "date(huge float): " + try_value(date sub () { return date(hugef); });
    l += "date(nan): " + try_value(date sub () { return date(nan); });
    l += "date(float) in range: " + try_value(date sub () { return date(-1.5); });
    # durations
    l += "get_duration_seconds(): " + try_value(int sub () { return get_duration_seconds(maxy); });
    l += "get_duration_milliseconds(): " + try_value(int sub () { return get_duration_milliseconds(maxy); });
    l += "get_duration_microseconds(): " + try_value(int sub () { return get_duration_microseconds(maxy); });
    l += "durationMicroseconds(): " + try_value(int sub () { return maxy.durationMicroseconds(); });
    l += "get_duration_microseconds() in range: "
        + try_value(int sub () { return get_duration_microseconds(maxh); });
    # the comparison of relative dates near the limits
    l += "compare near limits: " + try_value(bool sub () { return (days(2147483647) + hours(48)) > years(5883516); });
    l += "compare equal: " + try_value(bool sub () { return days(365) == years(1); });
    # literals
    l += "literal h: " + try_parse("2147483648h");
    l += "literal Y: " + try_parse("2147483648Y");
    l += "literal s: " + try_parse("9223372036855s");
    l += "literal ms: " + try_parse("9223372036854775ms");
    l += "literal us: " + try_parse("9223372036854775807us");
    l += "literal beyond int64: " + try_parse("9223372036854775808D");
    l += "literal s in range: " + try_parse("3000000000s");
    l += "literal m in range: " + try_parse("128849018879m");
    l += "literal h in range: " + try_parse("2147483647h");
    # constant operands: the operation is folded at parse time
    l += "constant operands: " + try_parse("2147483647h + 1h");
    return l;
}
