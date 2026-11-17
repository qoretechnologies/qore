# probes of number-round-exact.qtest (qore#6278): round(), ceil() and floor() of a number with a precision find the
# multiple of the power of 10 exactly, from the binary value of the number, and give it correctly rounded to the
# number's precision; probes() returns the results, one per line, as shortest round-trip strings in scientific
# notation with the precision of each result

%modern

#! round(), ceil() and floor() of a number with a precision
string sub rcf(number v, int p) {
    return sprintf("%s,%d: %s %s %s", v.toStringRoundTrip(True), p, fmt(round(v, p)), fmt(ceil(v, p)),
        fmt(floor(v, p)));
}

#! a result with its precision
string sub fmt(number n) {
    return sprintf("%s(%d)", n.toStringRoundTrip(True), n.prec());
}

string sub probes() {
    list<string> l = ();
    # halfway values, which are exactly binary: rounded away from zero
    l += rcf(0.125n, 2);
    l += rcf(-0.125n, 2);
    l += rcf(0.375n, 2);
    l += rcf(1.0625n, 3);
    l += rcf(0.03125n, 4);
    l += rcf(2.5n, 0);
    l += rcf(-2.5n, 0);
    l += rcf(250n, -2);
    l += rcf(-150n, -2);
    # the precision of the value's last binary digit, and above it: the value itself
    l += rcf(0.125n, 3);
    l += rcf(0.125n, 4);
    l += rcf(0.1n, 400);
    l += rcf(1.5n, 4294967297);
    # a precision whose multiple of 10^-k differs from the value by less than an eighth of its last binary digit: the
    # value itself
    l += rcf(0.1n, 41);
    l += rcf(-0.1n, 100);
    # decimal fractions that are not exactly binary: rounded from their binary value
    l += rcf(2.675n, 2);
    l += rcf(1.005n, 2);
    l += rcf(645084.36481772n, 5);
    l += rcf(645084.36481772n, -4);
    l += rcf(-645084.36481772n, 3);
    # precisions whose power of 10 is not exact as a float (above 10^22) or does not fit one (above 10^308)
    l += rcf(1.2345678901234567890123456789n, 24);
    l += rcf(1.2345678901234567890123456789n, 27);
    l += rcf(645084.36481772n, 30);
    l += rcf(123.5n, -400);
    l += rcf(-123.5n, -400);
    l += rcf(1e400n, -350);
    l += rcf(1.5e400n, -400);
    l += rcf(1.5e-400n, 400);
    l += rcf(1.25e-400n, 401);
    # values with large exponents
    l += rcf(1e-100000n, 99999);
    l += rcf(1e-100000n, 100000);
    l += rcf(3e100000n, -100000);
    l += rcf(1.5e-100000n, 100000);
    l += rcf(-2.5e100000n, -100000);
    # values near the limits of the exponent range, where 10^k is out of the range
    l += rcf(5e-323228497n, 323228497);
    l += rcf(-5e-323228497n, 323228496);
    l += rcf(4.5e323228495n, -323228495);
    # a power of 10 out of the exponent range of a number
    l += rcf(5.5n, -9223372036854775807 - 1);
    l += rcf(-5.5n, 9223372036854775807);
    # zero and negative zero
    l += rcf(0n, 2);
    l += rcf(-0n, 2);
    l += rcf(-0.001n, 2);
    l += rcf(0.001n, -2);
    # a result of 0 keeps the sign of the value
    l += rcf(-0.4n, 0);
    # the results compare equal to the literals of the same precision
    l += sprintf("eq: %y %y %y %y %y", round(0.125n, 2) == 0.13n, round(1.005n, 2) == 1.01n,
        round(2.675n, 2) == 2.67n, round(645084.36481772n, 5) == 645084.36482n,
        round(645084.36481772n, 30) == 645084.36481772n);
    return l.join("\n");
}
