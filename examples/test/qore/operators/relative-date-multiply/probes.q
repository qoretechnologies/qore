# probes of relative-date-multiply.qtest (qore#6109): a relative date multiplied by a number scales each component
# (years, months, days, hours, minutes, seconds and microseconds) instead of flattening the date into hours; the
# fraction of a component that a float factor gives is carried to the smaller units as the ISO 8601 duration parser
# carries it (a year is 365 days, a month 30 days and a day 24 hours) and rounded to the nearest microsecond;
# probes() returns one line per probe, and is called once per pass, so that the code runs on every tier with tiered
# execution

%modern

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
    # a decimal factor that a float cannot hold exactly: the fractions are rounded to the nearest microsecond
    l += "P2.3M: " + same("P2.3M", month * 2.3);
    l += "PT0.57S: " + try_value(date sub () { return seconds(1) * 0.57; });
    return l;
}
