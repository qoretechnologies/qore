# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the checks of the range operator (x[m..n]) shared by RangeEffective.q and RangeEffectiveNoNegative.q, which define
# NegativeOffsets as the value of the negative-offsets parse option they use
#
# every sequence type (list, string, binary, buffer), empty and not, is sliced with every combination of a set of
# bounds - negative, beyond the size, reversed and both out of range, as constants, as int, float and untyped values
# and omitted - and each result is compared with the one that the reference rule below gives; the rule follows the
# documentation of the [] operator, without using the range operator

const Bounds = (-9, -6, -5, -1, 0, 1, 4, 5, 9);

# the slices with constant bounds; the slice closures are given a sequence of any type
const ConstantSlices = (
    {"start": -9, "stop": -9, "slice": auto sub (auto x) { return x[-9..-9]; }},
    {"start": -9, "stop": -6, "slice": auto sub (auto x) { return x[-9..-6]; }},
    {"start": -9, "stop": -5, "slice": auto sub (auto x) { return x[-9..-5]; }},
    {"start": -9, "stop": -1, "slice": auto sub (auto x) { return x[-9..-1]; }},
    {"start": -9, "stop": 0, "slice": auto sub (auto x) { return x[-9..0]; }},
    {"start": -9, "stop": 1, "slice": auto sub (auto x) { return x[-9..1]; }},
    {"start": -9, "stop": 4, "slice": auto sub (auto x) { return x[-9..4]; }},
    {"start": -9, "stop": 5, "slice": auto sub (auto x) { return x[-9..5]; }},
    {"start": -9, "stop": 9, "slice": auto sub (auto x) { return x[-9..9]; }},
    {"start": -6, "stop": -9, "slice": auto sub (auto x) { return x[-6..-9]; }},
    {"start": -6, "stop": -6, "slice": auto sub (auto x) { return x[-6..-6]; }},
    {"start": -6, "stop": -5, "slice": auto sub (auto x) { return x[-6..-5]; }},
    {"start": -6, "stop": -1, "slice": auto sub (auto x) { return x[-6..-1]; }},
    {"start": -6, "stop": 0, "slice": auto sub (auto x) { return x[-6..0]; }},
    {"start": -6, "stop": 1, "slice": auto sub (auto x) { return x[-6..1]; }},
    {"start": -6, "stop": 4, "slice": auto sub (auto x) { return x[-6..4]; }},
    {"start": -6, "stop": 5, "slice": auto sub (auto x) { return x[-6..5]; }},
    {"start": -6, "stop": 9, "slice": auto sub (auto x) { return x[-6..9]; }},
    {"start": -5, "stop": -9, "slice": auto sub (auto x) { return x[-5..-9]; }},
    {"start": -5, "stop": -6, "slice": auto sub (auto x) { return x[-5..-6]; }},
    {"start": -5, "stop": -5, "slice": auto sub (auto x) { return x[-5..-5]; }},
    {"start": -5, "stop": -1, "slice": auto sub (auto x) { return x[-5..-1]; }},
    {"start": -5, "stop": 0, "slice": auto sub (auto x) { return x[-5..0]; }},
    {"start": -5, "stop": 1, "slice": auto sub (auto x) { return x[-5..1]; }},
    {"start": -5, "stop": 4, "slice": auto sub (auto x) { return x[-5..4]; }},
    {"start": -5, "stop": 5, "slice": auto sub (auto x) { return x[-5..5]; }},
    {"start": -5, "stop": 9, "slice": auto sub (auto x) { return x[-5..9]; }},
    {"start": -1, "stop": -9, "slice": auto sub (auto x) { return x[-1..-9]; }},
    {"start": -1, "stop": -6, "slice": auto sub (auto x) { return x[-1..-6]; }},
    {"start": -1, "stop": -5, "slice": auto sub (auto x) { return x[-1..-5]; }},
    {"start": -1, "stop": -1, "slice": auto sub (auto x) { return x[-1..-1]; }},
    {"start": -1, "stop": 0, "slice": auto sub (auto x) { return x[-1..0]; }},
    {"start": -1, "stop": 1, "slice": auto sub (auto x) { return x[-1..1]; }},
    {"start": -1, "stop": 4, "slice": auto sub (auto x) { return x[-1..4]; }},
    {"start": -1, "stop": 5, "slice": auto sub (auto x) { return x[-1..5]; }},
    {"start": -1, "stop": 9, "slice": auto sub (auto x) { return x[-1..9]; }},
    {"start": 0, "stop": -9, "slice": auto sub (auto x) { return x[0..-9]; }},
    {"start": 0, "stop": -6, "slice": auto sub (auto x) { return x[0..-6]; }},
    {"start": 0, "stop": -5, "slice": auto sub (auto x) { return x[0..-5]; }},
    {"start": 0, "stop": -1, "slice": auto sub (auto x) { return x[0..-1]; }},
    {"start": 0, "stop": 0, "slice": auto sub (auto x) { return x[0..0]; }},
    {"start": 0, "stop": 1, "slice": auto sub (auto x) { return x[0..1]; }},
    {"start": 0, "stop": 4, "slice": auto sub (auto x) { return x[0..4]; }},
    {"start": 0, "stop": 5, "slice": auto sub (auto x) { return x[0..5]; }},
    {"start": 0, "stop": 9, "slice": auto sub (auto x) { return x[0..9]; }},
    {"start": 1, "stop": -9, "slice": auto sub (auto x) { return x[1..-9]; }},
    {"start": 1, "stop": -6, "slice": auto sub (auto x) { return x[1..-6]; }},
    {"start": 1, "stop": -5, "slice": auto sub (auto x) { return x[1..-5]; }},
    {"start": 1, "stop": -1, "slice": auto sub (auto x) { return x[1..-1]; }},
    {"start": 1, "stop": 0, "slice": auto sub (auto x) { return x[1..0]; }},
    {"start": 1, "stop": 1, "slice": auto sub (auto x) { return x[1..1]; }},
    {"start": 1, "stop": 4, "slice": auto sub (auto x) { return x[1..4]; }},
    {"start": 1, "stop": 5, "slice": auto sub (auto x) { return x[1..5]; }},
    {"start": 1, "stop": 9, "slice": auto sub (auto x) { return x[1..9]; }},
    {"start": 4, "stop": -9, "slice": auto sub (auto x) { return x[4..-9]; }},
    {"start": 4, "stop": -6, "slice": auto sub (auto x) { return x[4..-6]; }},
    {"start": 4, "stop": -5, "slice": auto sub (auto x) { return x[4..-5]; }},
    {"start": 4, "stop": -1, "slice": auto sub (auto x) { return x[4..-1]; }},
    {"start": 4, "stop": 0, "slice": auto sub (auto x) { return x[4..0]; }},
    {"start": 4, "stop": 1, "slice": auto sub (auto x) { return x[4..1]; }},
    {"start": 4, "stop": 4, "slice": auto sub (auto x) { return x[4..4]; }},
    {"start": 4, "stop": 5, "slice": auto sub (auto x) { return x[4..5]; }},
    {"start": 4, "stop": 9, "slice": auto sub (auto x) { return x[4..9]; }},
    {"start": 5, "stop": -9, "slice": auto sub (auto x) { return x[5..-9]; }},
    {"start": 5, "stop": -6, "slice": auto sub (auto x) { return x[5..-6]; }},
    {"start": 5, "stop": -5, "slice": auto sub (auto x) { return x[5..-5]; }},
    {"start": 5, "stop": -1, "slice": auto sub (auto x) { return x[5..-1]; }},
    {"start": 5, "stop": 0, "slice": auto sub (auto x) { return x[5..0]; }},
    {"start": 5, "stop": 1, "slice": auto sub (auto x) { return x[5..1]; }},
    {"start": 5, "stop": 4, "slice": auto sub (auto x) { return x[5..4]; }},
    {"start": 5, "stop": 5, "slice": auto sub (auto x) { return x[5..5]; }},
    {"start": 5, "stop": 9, "slice": auto sub (auto x) { return x[5..9]; }},
    {"start": 9, "stop": -9, "slice": auto sub (auto x) { return x[9..-9]; }},
    {"start": 9, "stop": -6, "slice": auto sub (auto x) { return x[9..-6]; }},
    {"start": 9, "stop": -5, "slice": auto sub (auto x) { return x[9..-5]; }},
    {"start": 9, "stop": -1, "slice": auto sub (auto x) { return x[9..-1]; }},
    {"start": 9, "stop": 0, "slice": auto sub (auto x) { return x[9..0]; }},
    {"start": 9, "stop": 1, "slice": auto sub (auto x) { return x[9..1]; }},
    {"start": 9, "stop": 4, "slice": auto sub (auto x) { return x[9..4]; }},
    {"start": 9, "stop": 5, "slice": auto sub (auto x) { return x[9..5]; }},
    {"start": 9, "stop": 9, "slice": auto sub (auto x) { return x[9..9]; }},
    {"start": -9, "slice": auto sub (auto x) { return x[-9..]; }},
    {"start": -6, "slice": auto sub (auto x) { return x[-6..]; }},
    {"start": -5, "slice": auto sub (auto x) { return x[-5..]; }},
    {"start": -1, "slice": auto sub (auto x) { return x[-1..]; }},
    {"start": 0, "slice": auto sub (auto x) { return x[0..]; }},
    {"start": 1, "slice": auto sub (auto x) { return x[1..]; }},
    {"start": 4, "slice": auto sub (auto x) { return x[4..]; }},
    {"start": 5, "slice": auto sub (auto x) { return x[5..]; }},
    {"start": 9, "slice": auto sub (auto x) { return x[9..]; }},
    {"stop": -9, "slice": auto sub (auto x) { return x[..-9]; }},
    {"stop": -6, "slice": auto sub (auto x) { return x[..-6]; }},
    {"stop": -5, "slice": auto sub (auto x) { return x[..-5]; }},
    {"stop": -1, "slice": auto sub (auto x) { return x[..-1]; }},
    {"stop": 0, "slice": auto sub (auto x) { return x[..0]; }},
    {"stop": 1, "slice": auto sub (auto x) { return x[..1]; }},
    {"stop": 4, "slice": auto sub (auto x) { return x[..4]; }},
    {"stop": 5, "slice": auto sub (auto x) { return x[..5]; }},
    {"stop": 9, "slice": auto sub (auto x) { return x[..9]; }},
);

int checks = 0;
int failures = 0;

# the indices of the elements of a sequence of n elements that x[start..stop] gives, in their order; a bound that is
# NOTHING is omitted
list<int> sub ref_indices(int n, *int start, *int stop) {
    int s = exists start ? ((NegativeOffsets && start < 0) ? start + n : start) : 0;
    int e = exists stop ? ((NegativeOffsets && stop < 0) ? stop + n : stop) : n - 1;
    list<int> rv = ();
    # an omitted start is the start of the sequence and an omitted stop its end: the range goes forward
    if (!n || (!exists start && e < 0) || (!exists stop && s >= n)) {
        return rv;
    }
    if (s <= e) {
        if (s >= n || e < 0) {
            return rv;
        }
        int last = e < n ? e : n - 1;
        for (int i = s < 0 ? 0 : s; i <= last; ++i) {
            push rv, i;
        }
    } else {
        if (e >= n || s < 0) {
            return rv;
        }
        int last = e < 0 ? 0 : e;
        for (int i = s < n ? s : n - 1; i >= last; --i) {
            push rv, i;
        }
    }
    return rv;
}

# the number of elements of a sequence
int sub seq_size(auto seq) {
    switch (seq.typeCode()) {
        case NT_STRING: return seq.length();
        default: return seq.size();
    }
}

# a description of a value that gives its type and its elements
string sub describe(auto v) {
    switch (v.typeCode()) {
        case NT_LIST: return sprintf("%s %y", v.fullType(), v);
        case NT_STRING: return sprintf("string %y", v);
        case NT_BINARY: return "binary " + v.toHex();
        case NT_BUFFER: return sprintf("%s %y", v.fullType(), v.toList());
    }
    return sprintf("%s %y", v.type(), v);
}

# the description of the value made of the given elements of a sequence
string sub describe_elements(auto seq, list<int> idx) {
    switch (seq.typeCode()) {
        case NT_LIST: {
            list<auto> l = ();
            foreach int i in (idx) {
                push l, seq[i];
            }
            return sprintf("%s %y", seq.fullType(), l);
        }
        case NT_STRING: {
            string str = "";
            foreach int i in (idx) {
                str += seq.substr(i, 1);
            }
            return sprintf("string %y", str);
        }
        case NT_BINARY: {
            string hex = "";
            foreach int i in (idx) {
                hex += sprintf("%02x", seq[i]);
            }
            return "binary " + hex;
        }
        case NT_BUFFER: {
            list<auto> l = ();
            foreach int i in (idx) {
                push l, seq[i];
            }
            return sprintf("%s %y", seq.fullType(), l);
        }
    }
    throw "TEST-ERROR", sprintf("unexpected sequence type %y", seq.type());
}

# the indices of the elements of a sequence of n elements that are not in idx, in their order
list<int> sub rest_indices(int n, list<int> idx) {
    hash<auto> h = map {$1: True}, idx;
    list<int> rv = ();
    for (int i = 0; i < n; ++i) {
        if (!h{i}) {
            push rv, i;
        }
    }
    return rv;
}

string sub bound_str(auto b) {
    return exists b ? sprintf("%y", b) : "";
}

sub check(string label, auto seq, *int start, *int stop, string actual, *string expected) {
    ++checks;
    if (!exists expected) {
        expected = describe_elements(seq, ref_indices(seq_size(seq), start, stop));
    }
    if (actual != expected) {
        if (++failures <= ((ENV.RANGE_CHECK_FAIL_LIMIT ?? 100).toInt())) {
            printf("FAIL %s %s[%s..%s]: got %s, expected %s\n", label, describe(seq), bound_str(start),
                bound_str(stop), actual, expected);
        }
    }
}

# a check whose expression raised an exception
sub check_exception(string label, auto seq, *int start, *int stop, hash<ExceptionInfo> ex) {
    check(label, seq, start, stop, sprintf("exception %s: %s", ex.err, ex.desc), "no exception");
}

# slices with constant bounds
sub check_constants(auto seq) {
    foreach hash<auto> c in (ConstantSlices) {
        try {
            check("constant", seq, c.start, c.stop, describe(c.slice(seq)));
        } catch (hash<ExceptionInfo> ex) {
            check_exception("constant", seq, c.start, c.stop, ex);
        }
    }
}

# slices with bounds from variables of the given type; NOTHING omits a bound
sub check_dynamic(auto seq) {
    list<auto> bounds = Bounds + (NOTHING,);
    foreach auto s in (bounds) {
        foreach auto e in (bounds) {
            *int is = s;
            *int ie = e;
            try {
                check("int", seq, s, e, describe(seq[is..ie]));
            } catch (hash<ExceptionInfo> ex) {
                check_exception("int", seq, s, e, ex);
            }
            *float fs = exists s ? float(s) : NOTHING;
            *float fe = exists e ? float(e) : NOTHING;
            try {
                check("float", seq, s, e, describe(seq[fs..fe]));
            } catch (hash<ExceptionInfo> ex) {
                check_exception("float", seq, s, e, ex);
            }
            auto as = s;
            auto ae = e;
            try {
                check("auto", seq, s, e, describe(seq[as..ae]));
            } catch (hash<ExceptionInfo> ex) {
                check_exception("auto", seq, s, e, ex);
            }
            check_typed(seq, is, ie);
            try {
                check_iterate(seq, is, ie);
            } catch (hash<ExceptionInfo> ex) {
                check_exception("foreach", seq, s, e, ex);
            }
            try {
                check_remove(seq, is, ie);
            } catch (hash<ExceptionInfo> ex) {
                check_exception("remove", seq, s, e, ex);
            }
        }
    }
}

# slices of a sequence with a declared type
sub check_typed(auto seq, *int s, *int e) {
    switch (seq.typeCode()) {
        case NT_LIST: check_list(seq, s, e); break;
        case NT_STRING: check_string(seq, s, e); break;
        case NT_BINARY: check_binary(seq, s, e); break;
    }
}

sub check_list(list<auto> seq, *int s, *int e) {
    try {
        check("typed", seq, s, e, describe(seq[s..e]));
    } catch (hash<ExceptionInfo> ex) {
        check_exception("typed", seq, s, e, ex);
    }
}

sub check_string(string seq, *int s, *int e) {
    try {
        check("typed", seq, s, e, describe(seq[s..e]));
    } catch (hash<ExceptionInfo> ex) {
        check_exception("typed", seq, s, e, ex);
    }
}

sub check_binary(binary seq, *int s, *int e) {
    try {
        check("typed", seq, s, e, describe(seq[s..e]));
    } catch (hash<ExceptionInfo> ex) {
        check_exception("typed", seq, s, e, ex);
    }
}

# the elements of a range of a list iterated in place
sub check_iterate(auto seq, *int s, *int e) {
    if (seq.typeCode() != NT_LIST) {
        return;
    }
    list<auto> l = ();
    foreach auto v in (seq[s..e]) {
        push l, v;
    }
    list<int> idx = ref_indices(seq_size(seq), s, e);
    list<auto> el = map seq[$1], idx;
    check("foreach", seq, s, e, sprintf("%y", l), sprintf("%y", el));
}

# the value removed by the remove operator and the value left
sub check_remove(auto seq, *int s, *int e) {
    if (seq.typeCode() != NT_LIST && seq.typeCode() != NT_STRING && seq.typeCode() != NT_BINARY) {
        return;
    }
    auto c = seq;
    auto r = remove c[s..e];
    list<int> idx = ref_indices(seq_size(seq), s, e);
    string expected = describe_elements(seq, idx);
    # the remove operator gives an untyped list
    if (seq.typeCode() == NT_LIST) {
        expected = expected.substr(expected.find(" "));
        check("remove", seq, s, e, sprintf("%y", r), expected.substr(1));
    } else {
        check("remove", seq, s, e, describe(r), expected);
    }
    check("remove (rest)", seq, s, e, describe(c), describe_elements(seq, rest_indices(seq_size(seq), idx)));
}

# slices of literal sequences with literal bounds
sub check_literals() {
    try {
        check("literal", "", -9, 1, describe(""[-9..1]));
    } catch (hash<ExceptionInfo> ex) {
        check_exception("literal", "", -9, 1, ex);
    }
    try {
        check("literal", "", 1, -9, describe(""[1..-9]));
    } catch (hash<ExceptionInfo> ex) {
        check_exception("literal", "", 1, -9, ex);
    }
    try {
        check("literal", "", 0, 0, describe(""[0..0]));
    } catch (hash<ExceptionInfo> ex) {
        check_exception("literal", "", 0, 0, ex);
    }
    try {
        check("literal", "äöüßé", -2, NOTHING, describe("äöüßé"[-2..]));
    } catch (hash<ExceptionInfo> ex) {
        check_exception("literal", "äöüßé", -2, NOTHING, ex);
    }
    try {
        check("literal", "äöüßé", 3, 1, describe("äöüßé"[3..1]));
    } catch (hash<ExceptionInfo> ex) {
        check_exception("literal", "äöüßé", 3, 1, ex);
    }
    try {
        check("literal", "äöüßé", 6, 9, describe("äöüßé"[6..9]));
    } catch (hash<ExceptionInfo> ex) {
        check_exception("literal", "äöüßé", 6, 9, ex);
    }
    try {
        check("literal", "äöüßé", 9, -9, describe("äöüßé"[9..-9]));
    } catch (hash<ExceptionInfo> ex) {
        check_exception("literal", "äöüßé", 9, -9, ex);
    }
    try {
        check("literal", binary(), -9, 1, describe(binary()[-9..1]));
    } catch (hash<ExceptionInfo> ex) {
        check_exception("literal", binary(), -9, 1, ex);
    }
    try {
        check("literal", binary(), 1, -9, describe(binary()[1..-9]));
    } catch (hash<ExceptionInfo> ex) {
        check_exception("literal", binary(), 1, -9, ex);
    }
    try {
        check("literal", <0102030405>, -9, 1, describe(<0102030405>[-9..1]));
    } catch (hash<ExceptionInfo> ex) {
        check_exception("literal", <0102030405>, -9, 1, ex);
    }
    try {
        check("literal", <0102030405>, 9, -2, describe(<0102030405>[9..-2]));
    } catch (hash<ExceptionInfo> ex) {
        check_exception("literal", <0102030405>, 9, -2, ex);
    }
}

# the sequences that are sliced: empty, of one element and of five elements of each type
list<auto> sub sequences() {
    list<auto> rv = (
        (),
        (1,),
        (1, 2, 3, 4, 5),
        ("a", 2, NOTHING, 4.5, True),
        "",
        "a",
        "abcde",
        "ä",
        "äöüßé",
        "aä€😀z",
        binary(),
        <01>,
        <0102030405>,
    );
    # empty values left by the removal of all their elements, which can keep the storage they had
    binary eb = <0102>;
    remove eb[0..1];
    push rv, eb;
    string es = "äb";
    remove es[0..1];
    push rv, es;
    list<auto> el = (1, 2);
    remove el[0..1];
    push rv, el;
    # typed lists
    list<int> li = ();
    push rv, li;
    li = (7,);
    push rv, li;
    li = (5, 4, 3, 2, 1);
    push rv, li;
    list<string> ls = ();
    push rv, ls;
    ls = ("ä", "b", "€", "d", "😀");
    push rv, ls;
    # buffers of several element types
    foreach list<auto> vals in (((), (10,), (10, -20, 30, -40, 50))) {
        push rv, new buffer<int8>(vals);
        push rv, new buffer<int64>(vals);
        push rv, new buffer<float64>(map float($1), vals);
        push rv, new buffer<decimal128>(map number($1), vals);
        push rv, new buffer<string>(map string($1), vals);
        push rv, new buffer<bool>(map $1 > 0, vals);
    }
    foreach list<auto> vals in (((), (NOTHING,), (1, NOTHING, 3, NOTHING, 5))) {
        push rv, new buffer<*int64>(vals);
        push rv, new buffer<*float64>(map exists $1 ? float($1) : NOTHING, vals);
        push rv, new buffer<*decimal128>(map exists $1 ? number($1) : NOTHING, vals);
    }
    return rv;
}

foreach auto seq in (sequences()) {
    check_constants(seq);
    check_dynamic(seq);
}
check_literals();
printf("range operator checks: %d, failures: %d\n", checks, failures);
