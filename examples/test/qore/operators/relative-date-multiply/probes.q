# probes of relative-date-multiply.qtest (qore#6109): a relative date multiplied by a number scales each component
# (years, months, days, hours, minutes, seconds and microseconds) instead of flattening the date into hours; the
# fraction of a component that a float factor gives is carried to the smaller units as the ISO 8601 duration parser
# carries the fraction of the product written in decimal (a year is 365 days, a month 30 days and a day 24 hours);
# probes() returns one line per probe, and is called once per pass, so that the code runs on every tier with tiered
# execution

%modern
# *= warns that the return value of a function call in its operand is ignored, which is not the case (an issue of
# its own, not of the date operations tested here)
%disable-warning return-value-ignored

# the result of a probe: the value, or the error code of the exception
string sub try_value(code c) {
    try {
        return sprintf("%y", c());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# compares a duration string with a product: equal as relative dates and with the same components
string sub same(string iso, date product) {
    date d = date(iso);
    return sprintf("%y %s %y (equal: %y, same components: %y)", d, d == product ? "==" : "!=", product,
        d == product, sprintf("%y", d) == sprintf("%y", product));
}

# compares unit(1) * f with date("P<f><unit>") for each unit and decimal factor: the same duration with the same
# normalized components; returns "ok" or the differences
string sub differential() {
    list<string> factors = ("0.1", "0.2", "0.3", "0.5", "0.57", "0.7", "0.9", "1.1", "1.5", "2.3", "2.7", "3.14159",
        "0.0000001", "0.0000015", "0.000001", "0.123456789", "7.25", "12.5", "0.333333", "0.999999", "100.01",
        "0.05", "0.0416666", "1.0000001", "0.4", "0.6", "0.8", "4.9", "17.17", "0.03125");
    # the constructor of a unit, the ISO 8601 designator and whether it follows the time designator
    list<list<auto>> units = (
        (date sub (int n) { return years(n); }, "Y", False),
        (date sub (int n) { return months(n); }, "M", False),
        (date sub (int n) { return days(n); }, "D", False),
        (date sub (int n) { return hours(n); }, "H", True),
        (date sub (int n) { return minutes(n); }, "M", True),
        (date sub (int n) { return seconds(n); }, "S", True),
    );
    list<string> diffs;
    foreach list<auto> u in (units) {
        foreach string f in (factors) {
            date product = u[0](1) * f.toFloat();
            date parsed = date("P" + (u[2] ? "T" : "") + f + u[1]);
            # the same duration with the same components once the parsed value is normalized (the parser keeps the
            # components as written, ex: 12.5M is 12 months 15 days; the product is normalized: 1 year 15 days)
            if (product != parsed || sprintf("%y", product) != sprintf("%y", parsed * 1)) {
                diffs += sprintf("%s%s: %y != %y", f, u[1], product, parsed);
            }
        }
    }
    return diffs ? diffs.join("; ") : sprintf("ok (%d cases)", units.size() * factors.size());
}

list<string> sub probes() {
    # operands in variables, so that the operations are evaluated at run time and not folded at parse time
    date month = months(1);
    date year = years(1);
    date day = days(1);
    int two = 2;
    float half = 1.5;
    list<string> l;

    # integer factors: each component is multiplied
    l += "months(1) * 2: " + try_value(date sub () { return month * two; });
    l += "years(1) * 2: " + try_value(date sub () { return year * two; });
    l += "days(1) * 2: " + try_value(date sub () { return day * two; });
    l += "2 * months(1): " + try_value(date sub () { return two * month; });
    l += "2024-01-31 + months(1) * 2: " + try_value(date sub () { return 2024-01-31T00:00:00Z + month * two; });
    l += "P1Y2M3DT4H5M6S * 3: " + try_value(date sub () { return date("P1Y2M3DT4H5M6S") * 3; });
    l += "months(7) * 2: " + try_value(date sub () { return months(7) * two; });
    l += "seconds(40) * 3: " + try_value(date sub () { return seconds(40) * 3; });
    l += "hours(5) * -2: " + try_value(date sub () { return hours(5) * -2; });
    l += "days(1) * 0: " + try_value(date sub () { return day * 0; });
    # float factors: the fraction of each component is carried to the smaller units
    l += "months(1) * 1.5: " + try_value(date sub () { return month * half; });
    l += "years(1) * 1.5: " + try_value(date sub () { return year * half; });
    l += "days(1) * 1.5: " + try_value(date sub () { return day * half; });
    l += "P1Y2M * 0.5: " + try_value(date sub () { return date("P1Y2M") * 0.5; });
    l += "hours(1) * 0.25: " + try_value(date sub () { return hours(1) * 0.25; });
    l += "microseconds(3) * 0.5: " + try_value(date sub () { return microseconds(3) * 0.5; });
    l += "days(-1) * 1.5: " + try_value(date sub () { return days(-1) * half; });
    l += "2024-01-31 + months(1) * 2.0: " + try_value(date sub () { return 2024-01-31T00:00:00Z + month * 2.0; });
    # a number factor is a float factor
    l += "months(1) * 1.5n: " + try_value(date sub () { return month * 1.5n; });
    # the ISO 8601 duration parser and the product give the same value
    l += "P1.5M: " + same("P1.5M", month * half);
    l += "P1.5Y: " + same("P1.5Y", year * half);
    l += "P1.5D: " + same("P1.5D", day * half);
    l += "PT1.5H: " + same("PT1.5H", hours(1) * half);
    l += "PT1.5M: " + same("PT1.5M", minutes(1) * half);
    # a decimal factor that a float cannot hold exactly: the fraction is read as the decimal of the product
    l += "P2.3M: " + same("P2.3M", month * 2.3);
    l += "PT0.57S: " + same("PT0.57S", seconds(1) * 0.57);
    l += "PT0.0000015S: " + same("PT0.0000015S", seconds(1) * 0.0000015);
    l += "P0.0000001Y: " + same("P0.0000001Y", year * 0.0000001);
    # the fraction is exact with all its digits (a 20-digit prefix gives 861us)
    l += "P0.00000000002733384069001Y: " + same("P0.00000000002733384069001Y", year * 0.00000000002733384069001);
    # a fraction of a microsecond is truncated, as date(float) truncates it
    l += "PT0.9999999996S: " + same("PT0.9999999996S", seconds(1) * 0.9999999996);
    # the product of a component other than 1 is the decimal of the product
    l += "months(3) * 0.1: " + try_value(date sub () { return months(3) * 0.1; });
    # differential: unit(1) * f and date("P<f><unit>") for many decimal factors
    l += "differential: " + differential();
    # *= with a date lvalue or operand gives what * gives
    l += "*= int: " + try_value(date sub () { auto d = months(1); d *= 2; return d; });
    l += "*= float: " + try_value(date sub () { date d = months(1); d *= 1.5; return d; });
    l += "*= number: " + try_value(date sub () { auto d = days(1); d *= 1.5n; return d; });
    l += "*= date operand: " + try_value(auto sub () { auto i = 3; i *= hours(1); return i; });
    l += "*= hash member: " + try_value(date sub () { hash<auto> h = {"d": months(1)}; h.d *= 3; return h.d; });
    l += "*= list element: " + try_value(date sub () { list<auto> lst = (months(1),); lst[0] *= 3; return lst[0]; });
    l += "*= overflow: " + try_value(date sub () { auto d = hours(2147483647); d *= 2; return d; });
    l += "*= softint lvalue: " + try_value(int sub () { softint i = 2; i *= hours(1); return i; });
    return l;
}
