#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# a probe of UnassignedTypedLvalues.qtest (qore#5937): the operators applied to locals of non-optional types
# that are not assigned, and the division of members and global variables without a value; its output in every
# execution mode and as a qcc executable must be that of the AST interpreter

%modern
# the unassigned values are deliberate: their run-time handling is tested
%no-strict-parse-errors

any sub f0() { int x; return x / 2; }
any sub f1() { int x; return x % 2; }
any sub f2() { int x; return x - x; }
any sub f3() { int x; return x * x; }
any sub f4() { int x; return x + x + 1; }
any sub f5() { string s; return s + s + 'a'; }
any sub f6() { string s; return 'a' + s; }
any sub f7() { string s; return s + s; }
any sub f8() { int x; return -x; }
any sub f9() { int x; return x << 1; }
any sub f10() { int x; return x < 1; }
any sub f11() { int x; return x == 0; }
any sub f12() { int x; return x <=> 0; }
any sub f13() { float x; return x / 2; }
any sub f14() { float x; return x / 2.0; }
any sub f15() { int x; return x / 2.0; }
any sub f16() { int x; int y = 1; return y / x; }
any sub f17() { int x; return x & 1; }
any sub f18() { int x; return x | 1; }
any sub f19() { int x; x++; return x; }
any sub f20() { int x; int y = x / 2; return y; }
any sub f21() { int x = 4; delete x; return x / 2; }
list<auto> sub f22() { int x; return (x / 2, 1); }
any sub f23() { number x; return x / 2; }
any sub f24() { int x; return 1 - x; }
any sub f25() { int x; return x - 1; }
any sub f26() { int x; float f = 1.0; return x * f; }
any sub f27() { int x; return x + 0; }
any sub f28() { int x; return x * 1; }
any sub f29() { string s; return s.size(); }
any sub f30() { int x; return x ?* 3; }
any sub f31() { int x; return x === 0; }
any sub f32() { int x; return !x; }
any sub f33() { int x; return x / 2 * 3; }
any sub f34() { int x; return 6 / (x + 1); }

class DivMember {
    public {
        int m;
    }

    int half() {
        int y = m / 2;
        return y;
    }
}

our int div_global;

int sub globalHalf() {
    int y = div_global / 2;
    return y;
}

int sub branchHalf(bool b) {
    int x;
    if (b) {
        x = 4;
    }
    int y = x / 2;
    return y;
}

list<auto> sub branchHalfList(bool b) {
    int x;
    if (b) {
        x = 4;
    }
    return (x / 2, 1);
}

sub show(string l, code c) {
    try {
        auto v = c();
        printf("%s => %y %s\n", l, v, v.fullType());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s => %s\n", l, ex.err);
    }
}

show("f0", \f0());
show("f1", \f1());
show("f2", \f2());
show("f3", \f3());
show("f4", \f4());
show("f5", \f5());
show("f6", \f6());
show("f7", \f7());
show("f8", \f8());
show("f9", \f9());
show("f10", \f10());
show("f11", \f11());
show("f12", \f12());
show("f13", \f13());
show("f14", \f14());
show("f15", \f15());
show("f16", \f16());
show("f17", \f17());
show("f18", \f18());
show("f19", \f19());
show("f20", \f20());
show("f21", \f21());
show("f22", \f22());
show("f23", \f23());
show("f24", \f24());
show("f25", \f25());
show("f26", \f26());
show("f27", \f27());
show("f28", \f28());
show("f29", \f29());
show("f30", \f30());
show("f31", \f31());
show("f32", \f32());
show("f33", \f33());
show("f34", \f34());
show("member / 2", \(new DivMember()).half());
show("global / 2", \globalHalf());
show("branch local / 2", auto sub () { return branchHalf(False); });
show("branch local / 2 in a list", auto sub () { return branchHalfList(False); });
