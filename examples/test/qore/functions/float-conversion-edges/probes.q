# -*- mode: qore; indent-tabs-mode: nil -*-
# the probes of float-conversion-edges.qtest (qore#6212): conversions of a float or number that cast it to an integer
# without a range check

%modern

%requires json

#! an absolute date from a number (softdate): the microseconds of the fraction and values out of range
string sub number_dates() {
    list<string> l = ();
    # the fraction: it was taken from a float of the int of the value, which lost precision above 2^24 seconds and was
    # undefined above 2^31
    foreach number n in ((1731690000.5n, 3000000000.25n, -1.5n, 16777217.75n, 1e15n)) {
        # the seconds are local time, so the local date and time do not depend on the time zone
        softdate d = n;
        l += sprintf("%s.%06d", d.format("YYYY-MM-DD HH:mm:SS"), d.microseconds());
    }
    # NaN gives the date of 0 seconds, and a value out of range the date of the saturated seconds, as infinity does
    softdate nan_date = @nan@n;
    softdate zero_date = 0n;
    l += sprintf("nan=0:%y", nan_date == zero_date);
    softdate big = 1e300n;
    softdate inf_date = @inf@n;
    l += sprintf("1e300=inf:%y", big == inf_date);
    softdate neg_big = -1e300n;
    softdate neg_inf_date = -@inf@n;
    l += sprintf("-1e300=-inf:%y", neg_big == neg_inf_date);
    # the dates of the saturated seconds: the conversion of their local time to UTC overflowed in a time zone ahead of
    # UTC (the tests run with TZ=Europe/Prague)
    l += sprintf("min:%d max:%d", get_epoch_seconds(neg_big), get_epoch_seconds(big));
    return foldl $1 + "|" + $2, l;
}

#! round(), ceil() and floor() of a float with a precision whose power of 10 does not fit an int
string sub rounding(float v, int p) {
    return sprintf("%.17g/%.17g/%.17g", round(v, p), ceil(v, p), floor(v, p));
}

#! round(), ceil() and floor() of a number and an int with a precision that does not fit an int or whose power of 10 is
#! not finite
string sub nrounding(number v, int p) {
    return sprintf("%y/%y/%y", round(v, p), ceil(v, p), floor(v, p));
}

string sub irounding(int v, int p) {
    return sprintf("%d/%d/%d", round(v, p), ceil(v, p), floor(v, p));
}

string sub roundings() {
    list<string> l = ();
    l += nrounding(1.5n, 4294967297);
    l += nrounding(1.5n, 400);
    l += nrounding(123.5n, -400);
    l += nrounding(5.5n, -9223372036854775807 - 1);
    l += nrounding(645084.36481772n, -2);
    l += irounding(5, -4294967295);
    l += irounding(9223372036854775807, -1);
    l += irounding(-9223372036854775807 - 1, -1);
    l += irounding(5, -20);
    l += irounding(-5, -20);
    l += irounding(5, -9223372036854775807 - 1);
    l += irounding(-6450, -2);
    l += irounding(6450, -3);
    l += irounding(-6450, -4);
    l += irounding(645, 4294967297);
    l += rounding(1.23456789012345, 12);
    l += rounding(-1.23456789012345, 10);
    l += rounding(1.5, 400);
    l += rounding(1e300, 10);
    l += rounding(123.0, -400);
    l += rounding(-0.5, -400);
    l += rounding(5.5, -9223372036854775807 - 1);
    l += rounding(6450.36481, 3);
    l += rounding(-6450.36481, -2);
    return foldl $1 + "|" + $2, l;
}

#! a CBOR epoch time (tag 1) given as a float
string sub cbor_epochs() {
    list<string> l = ();
    # 1.5, NaN, +infinity, 1e300, -2^63 (fits), 2^63 (does not fit), and the unsigned ints 2^63 - 1 (fits) and 2^63
    # (does not fit)
    foreach binary b in ((<c1fb3ff8000000000000>, <c1fb7ff8000000000000>, <c1fb7ff0000000000000>,
            <c1fb7e37e43c8800759c>, <c1fbc3e0000000000000>, <c1fb43e0000000000000>, <c11b7fffffffffffffff>,
            <c11b8000000000000000>)) {
        try {
            date d = parse_cbor(b);
            l += sprintf("%d.%06d", get_epoch_seconds(d), d.microseconds());
        } catch (hash<ExceptionInfo> ex) {
            l += ex.err + ": " + ex.desc;
        }
    }
    return foldl $1 + "|" + $2, l;
}

string sub probes() {
    return "dates: " + number_dates() + "\nround: " + roundings() + "\ncbor: " + cbor_epochs();
}
