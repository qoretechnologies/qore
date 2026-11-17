# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# prints the results of number divisions by a divisor that is not zero but converts to the float 0.0 (1e-400n, and
# the string "1e-400" for a number dividend) and by divisors that are zero, with / and /= on every kind of lvalue,
# twice, so that tiered execution runs native code (qore#6387)
%modern

string sub res(code c) {
    try {
        auto v = c();
        # the quotients of a tiny divisor have 400 digits: their exponent is enough
        return v.typeCode() == NT_NUMBER && v > 1e300n ? sprintf("number %s", v.toString(NF_Scientific)) : sprintf("%y", v);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

number sub div_typed(number a, number b) {
    return a / b;
}

auto sub div_untyped(auto a, auto b) {
    return a / b;
}

number sub dive_typed(number a, number b) {
    a /= b;
    return a;
}

auto sub dive_untyped(auto a, auto b) {
    a /= b;
    return a;
}

number sub dive_soft(softnumber a, auto b) {
    a /= b;
    return a;
}

hash<auto> sub dive_member(number a, auto b) {
    hash<auto> h = {"n": a};
    h.n /= b;
    return h;
}

number sub dive_constant(number a) {
    a /= 1e-400n;
    return a;
}

class C {
    public {
        number n = 1n;
    }

    number dive(auto b) {
        n /= b;
        return n;
    }
}

list<string> sub probes() {
    number tiny = 1e-400n;
    list<string> l = ();
    l += "/ typed tiny: " + res(auto sub () { return div_typed(1n, tiny); });
    l += "/ untyped tiny: " + res(auto sub () { return div_untyped(1n, tiny); });
    l += "/= typed tiny: " + res(auto sub () { return dive_typed(1n, tiny); });
    l += "/= untyped tiny: " + res(auto sub () { return dive_untyped(1n, tiny); });
    l += "/= softnumber tiny: " + res(auto sub () { return dive_soft(1n, tiny); });
    l += "/= member tiny: " + res(auto sub () { return dive_member(1n, tiny).n; });
    l += "/= constant tiny: " + res(auto sub () { return dive_constant(1n); });
    l += "/= object member tiny: " + res(auto sub () { return new C().dive(tiny); });
    l += "/= untyped int tiny: " + res(auto sub () { return dive_untyped(1, tiny); });
    l += "/= untyped number string tiny: " + res(auto sub () { return dive_untyped(1n, "1e-400"); });
    l += "/= typed zero: " + res(auto sub () { return dive_typed(1n, 0n); });
    l += "/= typed -0: " + res(auto sub () { return dive_typed(1n, -0.0n); });
    l += "/= untyped float zero: " + res(auto sub () { return dive_untyped(1n, 0.0); });
    l += "/= untyped int zero: " + res(auto sub () { return dive_untyped(1n, 0); });
    l += "/= untyped string zero: " + res(auto sub () { return dive_untyped(1n, "0"); });
    l += "/= untyped NOTHING: " + res(auto sub () { return dive_untyped(1n, NOTHING); });
    l += "/= untyped NaN: " + res(auto sub () { return dive_untyped(1n, @nan@n); });
    l += "/= member zero: " + res(auto sub () { return dive_member(1n, 0n).n; });
    l += "/= object member zero: " + res(auto sub () { return new C().dive(0n); });
    l += "/ typed zero: " + res(auto sub () { return div_typed(1n, 0n); });
    return l;
}

for (int i = 0; i < 2; ++i) {
    printf("%d: %s\n", i, probes().join("; "));
}
