#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# prints the element type, nullability, decimal precision and scale and the values of range slices of buffers, empty
# and not (qore#6263); BufferRangeDecimal.expected holds the output that every execution mode must print

%modern

string sub params(auto b) {
    hash<auto> s = ColumnarResult::fromColumnHash({"n": b}).schemaV2()[0];
    return sprintf("%s size %d precision %d scale %d %y", b.fullType(), b.size(), s.precision, s.scale,
        b.toList());
}

sub show(string label, auto b) {
    printf("%s: %s\n", label, params(b));
}

# constant indices on a typed buffer
sub typed_constant(buffer<decimal128> b, string name) {
    show(name + "[5..6]", b[5..6]);
    show(name + "[2..9]", b[2..9]);
    show(name + "[0..1]", b[0..1]);
    show(name + "[1..0]", b[1..0]);
    show(name + "[2..0]", b[2..0]);
    show(name + "[9..2]", b[9..2]);
    show(name + "[-1..-2]", b[-1..-2]);
    show(name + "[-5..-6]", b[-5..-6]);
    # a range with a negative bound clamped to an empty sequence is a different issue
    if (b.size()) {
        show(name + "[-9..1]", b[-9..1]);
        show(name + "[1..-9]", b[1..-9]);
    }
}

# int, float and untyped indices from variables on an untyped value
sub dynamic_indices(auto b, string name) {
    int s = 5;
    int e = 6;
    show(name + "[int 5..6]", b[s..e]);
    s = 1;
    e = 0;
    show(name + "[int 1..0]", b[s..e]);
    float fs = 5.0;
    float fe = 6.0;
    show(name + "[float 5..6]", b[fs..fe]);
    fs = 0.0;
    fe = 1.0;
    show(name + "[float 0..1]", b[fs..fe]);
    auto as = 7;
    auto ae = 9;
    show(name + "[auto 7..9]", b[as..ae]);
    as = -1;
    ae = -2;
    show(name + "[auto -1..-2]", b[as..ae]);
}

# a nullable buffer
sub nullable(buffer<*decimal128> b) {
    show("nullable[5..6]", b[5..6]);
    show("nullable[0..2]", b[0..2]);
    show("nullable[2..0]", b[2..0]);
    show("nullable[-9..-8]", b[-9..-8]);
}

# a buffer of another type
sub other(buffer<int64> b) {
    show("int64[5..6]", b[5..6]);
    show("int64[1..0]", b[1..0]);
    buffer<*float64> f = new buffer<*float64>((1.5, NOTHING));
    show("float64[5..6]", f[5..6]);
    show("float64[1..0]", f[1..0]);
}

buffer<decimal128> b = ColumnarResult::fromColumnHash({"n": (1.25n, 2.5n, -3.75n)},
    {"n": {"native_type": "numeric", "precision": 10, "scale": 2}}).getColumn("n");
typed_constant(b, "b");
dynamic_indices(b, "b");

buffer<decimal128> e = ColumnarResult::fromColumnHash({"n": ()},
    {"n": {"native_type": "numeric", "precision": 12, "scale": 4, "nullable": False}}).getColumn("n");
show("empty", e);
typed_constant(e, "empty");
dynamic_indices(e, "empty");

# precision 38, scale inferred from the values
buffer<decimal128> inferred = new buffer<decimal128>((0.001n, 2n, -123.456n));
show("inferred[3..4]", inferred[3..4]);
show("inferred[1..0]", inferred[1..0]);

buffer<*decimal128> n = ColumnarResult::fromColumnHash({"n": (1.5n, NOTHING, 0.125n)},
    {"n": {"native_type": "numeric", "precision": 20, "scale": 3}}).getColumn("n");
nullable(n);

other(new buffer<int64>((1, 2, 3)));
