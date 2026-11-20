# -*- mode: qore; indent-tabs-mode: nil -*-
# the probes of float-int-conversion.qtest (qore#6181): the conversion of a float or number value out of the range of
# an int, an infinite value or NaN to an int

%modern

#! the float values converted: out of range, infinite, NaN and the limits of an int
const Floats = (1e300, -1e300, @inf@, -@inf@, @nan@, -@nan@, @inf@ - @inf@, -5e307, 9223372036854775808.0,
    -9223372036854775808.0, 9223372036854774784.0, -9223372036854774784.0, 2.9, -2.9, 0.5, -0.5, 5e-324, -5e-324,
    -0.0);

#! the number equivalents of Floats
const Numbers = (1e300n, -1e300n, @inf@n, -@inf@n, @nan@n, -@nan@n, @inf@n - @inf@n, -5e307n,
    9223372036854775808n, -9223372036854775808n, 9223372036854774784n, -9223372036854774784n, 2.9n, -2.9n, 0.5n,
    -0.5n, 5e-324n, -5e-324n, -0.0n);

int sub soft_param(softint i) { return i; }

#! the conversions of a float typed as float
string sub float_typed(float f) {
    list<auto> l = ();
    l += int(f);
    softint si = f;
    l += si;
    softint sa = 0;
    sa = f;
    l += sa;
    l += soft_param(f);
    l += f % 1000;
    l += f & 0xffff;
    l += f | 0;
    l += f ^ 0;
    l += f >> 60;
    l += f << 0;
    l += ~f;
    l += "abcdef".substr(f, 2);
    l += substr("abcdef", f);
    l += sprintf("%d", f);
    l += (1, 2, 3)[f] ?? "none";
    l += "abc"[f] ?? "none";
    # a native float local in the JIT
    float g = f;
    softint sg = g;
    l += sg;
    # int() of a native float computed in the function (llvm.fptosi.sat in native code)
    float h = f * 1.0;
    l += int(h);
    # compound assignments: an int lvalue with a float operand, an untyped lvalue holding a float
    int pe = 0;
    pe += f;
    l += pe;
    auto ao = f;
    ao |= 0;
    l += ao;
    auto aa = f;
    aa &= 0xffff;
    l += aa;
    return foldl $1 + "/" + $2, (map sprintf("%y", $1), l);
}

#! the conversions of a float or number in an untyped value
string sub untyped(auto a) {
    list<auto> l = ();
    l += int(a);
    softint si = a;
    l += si;
    l += soft_param(a);
    l += a % 1000;
    l += a & 0xffff;
    l += a | 0;
    l += a >> 60;
    l += ~a;
    l += substr("abcdef", a);
    l += sprintf("%d", a);
    l += (1, 2, 3)[a] ?? "none";
    int pe = 0;
    pe += a;
    l += pe;
    auto ao = a;
    ao |= 0;
    l += ao;
    return foldl $1 + "/" + $2, (map sprintf("%y", $1), l);
}

#! the conversions of a number typed as number
string sub number_typed(number n) {
    list<auto> l = ();
    l += int(n);
    softint si = n;
    l += si;
    l += soft_param(n);
    l += n % 1000;
    l += n & 0xffff;
    l += substr("abcdef", n);
    return foldl $1 + "/" + $2, (map sprintf("%y", $1), l);
}

#! the dense int64 ColumnarResult column built from floats and numbers, and the column names of the buffer errors
/** a value outside the range of an int64 is refused with COLUMNAR-RESULT-ERROR instead of saturating (qore#6681)
*/
string sub columnar() {
    list<auto> l = ();
    list<auto> v = Floats + Numbers;
    foreach string native_type in (("bigint", "numeric(10,0)")) {
        try {
            ColumnarResult r = ColumnarResult::fromColumnHash({"i": v}, {"i": {"native_type": native_type}});
            l += sprintf("no exception: %y", r.toColumnHash().i.toList());
        } catch (hash<ExceptionInfo> ex) {
            l += ex.err + ": " + ex.desc;
        }
    }
    {
        list<hash<auto>> rows = ();
        foreach auto x in (v) {
            rows += {"i": x};
        }
        try {
            ColumnarResult r = ColumnarResult::fromRows(rows, {"i": {"native_type": "bigint"}});
            l += sprintf("no exception: %y", r.toColumnHash().i.toList());
        } catch (hash<ExceptionInfo> ex) {
            l += ex.err + ": " + ex.desc;
        }
    }
    try {
        ColumnarResult r = ColumnarResult::fromColumnHash({"col_f": (1.0, "x")}, {"col_f": {"type": NT_FLOAT}});
        l += sprintf("no exception: %y", r.toColumnHash());
    } catch (hash<ExceptionInfo> ex) {
        l += ex.err + ": " + ex.desc;
    }
    try {
        ColumnarResult r = ColumnarResult::fromColumnHash({"col_d": (1n, @nan@n)},
            {"col_d": {"native_type": "numeric(20,2)"}});
        l += sprintf("no exception: %y", r.toColumnHash());
    } catch (hash<ExceptionInfo> ex) {
        l += ex.err + ": " + ex.desc;
    }
    return foldl $1 + "\n" + $2, l;
}

#! the conversions of constant values
string sub constants() {
    list<auto> l = ();
    l += int(1e300);
    l += int(-1e300);
    l += int(@inf@);
    l += int(@nan@);
    softint si = 1e300;
    l += si;
    l += 1e300 % 1000;
    l += int(1e300n);
    l += int(@nan@n);
    # the limits and small values as constants
    l += int(9223372036854775808.0);
    l += int(-9223372036854775808.0);
    l += int(9223372036854774784.0);
    l += int(-@inf@);
    l += int(0.5);
    l += int(-0.5);
    l += int(5e-324);
    l += int(-5e-324);
    # native float locals with constant values, which native code folds
    float c1 = 1e300;
    l += int(c1);
    float c2 = -9223372036854775808.0;
    l += int(c2);
    float c3 = 9223372036854775808.0;
    l += int(c3);
    float c4 = @nan@;
    l += int(c4);
    float c5 = -0.5;
    l += int(c5);
    float c6 = 9223372036854774784.0;
    l += int(c6);
    # pow(int, int) with a result out of the range of an int
    l += pow(10, 100);
    l += pow(-10, 99);
    return foldl $1 + "/" + $2, (map sprintf("%y", $1), l);
}

string sub probes() {
    list<string> l = ("c " + constants(),);
    foreach float f in (Floats) {
        l += "f " + float_typed(f);
        l += "a " + untyped(f);
    }
    foreach number n in (Numbers) {
        l += "n " + number_typed(n);
        l += "u " + untyped(n);
    }
    l += columnar();
    return foldl $1 + "\n" + $2, l;
}
