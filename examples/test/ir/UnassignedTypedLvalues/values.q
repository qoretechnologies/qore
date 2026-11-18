#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# a probe of UnassignedTypedLvalues.qtest (qore#5937): lvalues of non-optional types that have no value (locals that
# are not assigned, members, static and global variables), read as operands and as values of list literals; its output
# in every execution mode and as a qcc executable must be that of the AST interpreter

%modern

auto sub id(auto x) { return x; }
sub show(string label, code c) {
    try { auto v = c(); printf("%s => %y %s\n", label, v, v.fullType()); }
    catch (hash<ExceptionInfo> ex) { printf("%s => %s\n", label, ex.err); }
}

class C0 { public { int m; static int sm; } }
class C1 { public { float m; static float sm; } }
class C2 { public { string m; static string sm; } }
class C3 { public { bool m; static bool sm; } }
class C4 { public { number m; static number sm; } }
class C5 { public { date m; static date sm; } }
class C6 { public { list<int> m; static list<int> sm; } }
class C7 { public { hash<string, int> m; static hash<string, int> sm; } }
class C8 { public { softlist<int> m; static softlist<int> sm; } }
class C9 { public { binary m; static binary sm; } }
our int g0;
list<auto> sub l1() { int X; return (X,); }
list<auto> sub mm1(C0 o) { return (o.m,); }
list<auto> sub sm1() { return (C0::sm,); }
list<auto> sub br1(bool b) { int X; if (b) { X = 1; } return (X,); }
list<auto> sub l2() { int X; return (X + X,); }
list<auto> sub mm2(C0 o) { return (o.m + o.m,); }
list<auto> sub sm2() { return (C0::sm + C0::sm,); }
list<auto> sub br2(bool b) { int X; if (b) { X = 1; } return (X + X,); }
list<auto> sub l3() { int X; return (exists X,); }
list<auto> sub mm3(C0 o) { return (exists o.m,); }
list<auto> sub sm3() { return (exists C0::sm,); }
list<auto> sub br3(bool b) { int X; if (b) { X = 1; } return (exists X,); }
list<auto> sub l4() { int X; return (X == X,); }
list<auto> sub mm4(C0 o) { return (o.m == o.m,); }
list<auto> sub sm4() { return (C0::sm == C0::sm,); }
list<auto> sub br4(bool b) { int X; if (b) { X = 1; } return (X == X,); }
list<auto> sub l5() { int X; return (id(X),); }
list<auto> sub mm5(C0 o) { return (id(o.m),); }
list<auto> sub sm5() { return (id(C0::sm),); }
list<auto> sub br5(bool b) { int X; if (b) { X = 1; } return (id(X),); }
list<auto> sub l6() { int X; return (X.typeCode(),); }
list<auto> sub mm6(C0 o) { return (o.m.typeCode(),); }
list<auto> sub sm6() { return (C0::sm.typeCode(),); }
list<auto> sub br6(bool b) { int X; if (b) { X = 1; } return (X.typeCode(),); }
list<auto> sub gg7() { return (g0 + g0,); }
our float g7;
list<auto> sub l8() { float X; return (X,); }
list<auto> sub mm8(C1 o) { return (o.m,); }
list<auto> sub sm8() { return (C1::sm,); }
list<auto> sub br8(bool b) { float X; if (b) { X = 1.5; } return (X,); }
list<auto> sub l9() { float X; return (X + X,); }
list<auto> sub mm9(C1 o) { return (o.m + o.m,); }
list<auto> sub sm9() { return (C1::sm + C1::sm,); }
list<auto> sub br9(bool b) { float X; if (b) { X = 1.5; } return (X + X,); }
list<auto> sub l10() { float X; return (exists X,); }
list<auto> sub mm10(C1 o) { return (exists o.m,); }
list<auto> sub sm10() { return (exists C1::sm,); }
list<auto> sub br10(bool b) { float X; if (b) { X = 1.5; } return (exists X,); }
list<auto> sub l11() { float X; return (X == X,); }
list<auto> sub mm11(C1 o) { return (o.m == o.m,); }
list<auto> sub sm11() { return (C1::sm == C1::sm,); }
list<auto> sub br11(bool b) { float X; if (b) { X = 1.5; } return (X == X,); }
list<auto> sub l12() { float X; return (id(X),); }
list<auto> sub mm12(C1 o) { return (id(o.m),); }
list<auto> sub sm12() { return (id(C1::sm),); }
list<auto> sub br12(bool b) { float X; if (b) { X = 1.5; } return (id(X),); }
list<auto> sub l13() { float X; return (X.typeCode(),); }
list<auto> sub mm13(C1 o) { return (o.m.typeCode(),); }
list<auto> sub sm13() { return (C1::sm.typeCode(),); }
list<auto> sub br13(bool b) { float X; if (b) { X = 1.5; } return (X.typeCode(),); }
list<auto> sub gg14() { return (g7 + g7,); }
our string g14;
list<auto> sub l15() { string X; return (X,); }
list<auto> sub mm15(C2 o) { return (o.m,); }
list<auto> sub sm15() { return (C2::sm,); }
list<auto> sub br15(bool b) { string X; if (b) { X = "s"; } return (X,); }
list<auto> sub l16() { string X; return (X + X,); }
list<auto> sub mm16(C2 o) { return (o.m + o.m,); }
list<auto> sub sm16() { return (C2::sm + C2::sm,); }
list<auto> sub br16(bool b) { string X; if (b) { X = "s"; } return (X + X,); }
list<auto> sub l17() { string X; return (exists X,); }
list<auto> sub mm17(C2 o) { return (exists o.m,); }
list<auto> sub sm17() { return (exists C2::sm,); }
list<auto> sub br17(bool b) { string X; if (b) { X = "s"; } return (exists X,); }
list<auto> sub l18() { string X; return (X == X,); }
list<auto> sub mm18(C2 o) { return (o.m == o.m,); }
list<auto> sub sm18() { return (C2::sm == C2::sm,); }
list<auto> sub br18(bool b) { string X; if (b) { X = "s"; } return (X == X,); }
list<auto> sub l19() { string X; return (id(X),); }
list<auto> sub mm19(C2 o) { return (id(o.m),); }
list<auto> sub sm19() { return (id(C2::sm),); }
list<auto> sub br19(bool b) { string X; if (b) { X = "s"; } return (id(X),); }
list<auto> sub l20() { string X; return (X.typeCode(),); }
list<auto> sub mm20(C2 o) { return (o.m.typeCode(),); }
list<auto> sub sm20() { return (C2::sm.typeCode(),); }
list<auto> sub br20(bool b) { string X; if (b) { X = "s"; } return (X.typeCode(),); }
list<auto> sub gg21() { return (g14 + g14,); }
our bool g21;
list<auto> sub l22() { bool X; return (X,); }
list<auto> sub mm22(C3 o) { return (o.m,); }
list<auto> sub sm22() { return (C3::sm,); }
list<auto> sub br22(bool b) { bool X; if (b) { X = True; } return (X,); }
list<auto> sub l23() { bool X; return (X + X,); }
list<auto> sub mm23(C3 o) { return (o.m + o.m,); }
list<auto> sub sm23() { return (C3::sm + C3::sm,); }
list<auto> sub br23(bool b) { bool X; if (b) { X = True; } return (X + X,); }
list<auto> sub l24() { bool X; return (exists X,); }
list<auto> sub mm24(C3 o) { return (exists o.m,); }
list<auto> sub sm24() { return (exists C3::sm,); }
list<auto> sub br24(bool b) { bool X; if (b) { X = True; } return (exists X,); }
list<auto> sub l25() { bool X; return (X == X,); }
list<auto> sub mm25(C3 o) { return (o.m == o.m,); }
list<auto> sub sm25() { return (C3::sm == C3::sm,); }
list<auto> sub br25(bool b) { bool X; if (b) { X = True; } return (X == X,); }
list<auto> sub l26() { bool X; return (id(X),); }
list<auto> sub mm26(C3 o) { return (id(o.m),); }
list<auto> sub sm26() { return (id(C3::sm),); }
list<auto> sub br26(bool b) { bool X; if (b) { X = True; } return (id(X),); }
list<auto> sub l27() { bool X; return (X.typeCode(),); }
list<auto> sub mm27(C3 o) { return (o.m.typeCode(),); }
list<auto> sub sm27() { return (C3::sm.typeCode(),); }
list<auto> sub br27(bool b) { bool X; if (b) { X = True; } return (X.typeCode(),); }
list<auto> sub gg28() { return (g21 + g21,); }
our number g28;
list<auto> sub l29() { number X; return (X,); }
list<auto> sub mm29(C4 o) { return (o.m,); }
list<auto> sub sm29() { return (C4::sm,); }
list<auto> sub br29(bool b) { number X; if (b) { X = 1n; } return (X,); }
list<auto> sub l30() { number X; return (X + X,); }
list<auto> sub mm30(C4 o) { return (o.m + o.m,); }
list<auto> sub sm30() { return (C4::sm + C4::sm,); }
list<auto> sub br30(bool b) { number X; if (b) { X = 1n; } return (X + X,); }
list<auto> sub l31() { number X; return (exists X,); }
list<auto> sub mm31(C4 o) { return (exists o.m,); }
list<auto> sub sm31() { return (exists C4::sm,); }
list<auto> sub br31(bool b) { number X; if (b) { X = 1n; } return (exists X,); }
list<auto> sub l32() { number X; return (X == X,); }
list<auto> sub mm32(C4 o) { return (o.m == o.m,); }
list<auto> sub sm32() { return (C4::sm == C4::sm,); }
list<auto> sub br32(bool b) { number X; if (b) { X = 1n; } return (X == X,); }
list<auto> sub l33() { number X; return (id(X),); }
list<auto> sub mm33(C4 o) { return (id(o.m),); }
list<auto> sub sm33() { return (id(C4::sm),); }
list<auto> sub br33(bool b) { number X; if (b) { X = 1n; } return (id(X),); }
list<auto> sub l34() { number X; return (X.typeCode(),); }
list<auto> sub mm34(C4 o) { return (o.m.typeCode(),); }
list<auto> sub sm34() { return (C4::sm.typeCode(),); }
list<auto> sub br34(bool b) { number X; if (b) { X = 1n; } return (X.typeCode(),); }
list<auto> sub gg35() { return (g28 + g28,); }
our date g35;
list<auto> sub l36() { date X; return (X,); }
list<auto> sub mm36(C5 o) { return (o.m,); }
list<auto> sub sm36() { return (C5::sm,); }
list<auto> sub br36(bool b) { date X; if (b) { X = 1h; } return (X,); }
list<auto> sub l37() { date X; return (X + X,); }
list<auto> sub mm37(C5 o) { return (o.m + o.m,); }
list<auto> sub sm37() { return (C5::sm + C5::sm,); }
list<auto> sub br37(bool b) { date X; if (b) { X = 1h; } return (X + X,); }
list<auto> sub l38() { date X; return (exists X,); }
list<auto> sub mm38(C5 o) { return (exists o.m,); }
list<auto> sub sm38() { return (exists C5::sm,); }
list<auto> sub br38(bool b) { date X; if (b) { X = 1h; } return (exists X,); }
list<auto> sub l39() { date X; return (X == X,); }
list<auto> sub mm39(C5 o) { return (o.m == o.m,); }
list<auto> sub sm39() { return (C5::sm == C5::sm,); }
list<auto> sub br39(bool b) { date X; if (b) { X = 1h; } return (X == X,); }
list<auto> sub l40() { date X; return (id(X),); }
list<auto> sub mm40(C5 o) { return (id(o.m),); }
list<auto> sub sm40() { return (id(C5::sm),); }
list<auto> sub br40(bool b) { date X; if (b) { X = 1h; } return (id(X),); }
list<auto> sub l41() { date X; return (X.typeCode(),); }
list<auto> sub mm41(C5 o) { return (o.m.typeCode(),); }
list<auto> sub sm41() { return (C5::sm.typeCode(),); }
list<auto> sub br41(bool b) { date X; if (b) { X = 1h; } return (X.typeCode(),); }
list<auto> sub gg42() { return (g35 + g35,); }
our list<int> g42;
list<auto> sub l43() { list<int> X; return (X,); }
list<auto> sub mm43(C6 o) { return (o.m,); }
list<auto> sub sm43() { return (C6::sm,); }
list<auto> sub br43(bool b) { list<int> X; if (b) { X = (1,); } return (X,); }
list<auto> sub l44() { list<int> X; return (X + X,); }
list<auto> sub mm44(C6 o) { return (o.m + o.m,); }
list<auto> sub sm44() { return (C6::sm + C6::sm,); }
list<auto> sub br44(bool b) { list<int> X; if (b) { X = (1,); } return (X + X,); }
list<auto> sub l45() { list<int> X; return (exists X,); }
list<auto> sub mm45(C6 o) { return (exists o.m,); }
list<auto> sub sm45() { return (exists C6::sm,); }
list<auto> sub br45(bool b) { list<int> X; if (b) { X = (1,); } return (exists X,); }
list<auto> sub l46() { list<int> X; return (X == X,); }
list<auto> sub mm46(C6 o) { return (o.m == o.m,); }
list<auto> sub sm46() { return (C6::sm == C6::sm,); }
list<auto> sub br46(bool b) { list<int> X; if (b) { X = (1,); } return (X == X,); }
list<auto> sub l47() { list<int> X; return (id(X),); }
list<auto> sub mm47(C6 o) { return (id(o.m),); }
list<auto> sub sm47() { return (id(C6::sm),); }
list<auto> sub br47(bool b) { list<int> X; if (b) { X = (1,); } return (id(X),); }
list<auto> sub l48() { list<int> X; return (X.typeCode(),); }
list<auto> sub mm48(C6 o) { return (o.m.typeCode(),); }
list<auto> sub sm48() { return (C6::sm.typeCode(),); }
list<auto> sub br48(bool b) { list<int> X; if (b) { X = (1,); } return (X.typeCode(),); }
list<auto> sub gg49() { return (g42 + g42,); }
our hash<string, int> g49;
list<auto> sub l50() { hash<string, int> X; return (X,); }
list<auto> sub mm50(C7 o) { return (o.m,); }
list<auto> sub sm50() { return (C7::sm,); }
list<auto> sub br50(bool b) { hash<string, int> X; if (b) { X = {"a": 1}; } return (X,); }
list<auto> sub l51() { hash<string, int> X; return (X + X,); }
list<auto> sub mm51(C7 o) { return (o.m + o.m,); }
list<auto> sub sm51() { return (C7::sm + C7::sm,); }
list<auto> sub br51(bool b) { hash<string, int> X; if (b) { X = {"a": 1}; } return (X + X,); }
list<auto> sub l52() { hash<string, int> X; return (exists X,); }
list<auto> sub mm52(C7 o) { return (exists o.m,); }
list<auto> sub sm52() { return (exists C7::sm,); }
list<auto> sub br52(bool b) { hash<string, int> X; if (b) { X = {"a": 1}; } return (exists X,); }
list<auto> sub l53() { hash<string, int> X; return (X == X,); }
list<auto> sub mm53(C7 o) { return (o.m == o.m,); }
list<auto> sub sm53() { return (C7::sm == C7::sm,); }
list<auto> sub br53(bool b) { hash<string, int> X; if (b) { X = {"a": 1}; } return (X == X,); }
list<auto> sub l54() { hash<string, int> X; return (id(X),); }
list<auto> sub mm54(C7 o) { return (id(o.m),); }
list<auto> sub sm54() { return (id(C7::sm),); }
list<auto> sub br54(bool b) { hash<string, int> X; if (b) { X = {"a": 1}; } return (id(X),); }
list<auto> sub l55() { hash<string, int> X; return (X.typeCode(),); }
list<auto> sub mm55(C7 o) { return (o.m.typeCode(),); }
list<auto> sub sm55() { return (C7::sm.typeCode(),); }
list<auto> sub br55(bool b) { hash<string, int> X; if (b) { X = {"a": 1}; } return (X.typeCode(),); }
list<auto> sub gg56() { return (g49 + g49,); }
our softlist<int> g56;
list<auto> sub l57() { softlist<int> X; return (X,); }
list<auto> sub mm57(C8 o) { return (o.m,); }
list<auto> sub sm57() { return (C8::sm,); }
list<auto> sub br57(bool b) { softlist<int> X; if (b) { X = (1,); } return (X,); }
list<auto> sub l58() { softlist<int> X; return (X + X,); }
list<auto> sub mm58(C8 o) { return (o.m + o.m,); }
list<auto> sub sm58() { return (C8::sm + C8::sm,); }
list<auto> sub br58(bool b) { softlist<int> X; if (b) { X = (1,); } return (X + X,); }
list<auto> sub l59() { softlist<int> X; return (exists X,); }
list<auto> sub mm59(C8 o) { return (exists o.m,); }
list<auto> sub sm59() { return (exists C8::sm,); }
list<auto> sub br59(bool b) { softlist<int> X; if (b) { X = (1,); } return (exists X,); }
list<auto> sub l60() { softlist<int> X; return (X == X,); }
list<auto> sub mm60(C8 o) { return (o.m == o.m,); }
list<auto> sub sm60() { return (C8::sm == C8::sm,); }
list<auto> sub br60(bool b) { softlist<int> X; if (b) { X = (1,); } return (X == X,); }
list<auto> sub l61() { softlist<int> X; return (id(X),); }
list<auto> sub mm61(C8 o) { return (id(o.m),); }
list<auto> sub sm61() { return (id(C8::sm),); }
list<auto> sub br61(bool b) { softlist<int> X; if (b) { X = (1,); } return (id(X),); }
list<auto> sub l62() { softlist<int> X; return (X.typeCode(),); }
list<auto> sub mm62(C8 o) { return (o.m.typeCode(),); }
list<auto> sub sm62() { return (C8::sm.typeCode(),); }
list<auto> sub br62(bool b) { softlist<int> X; if (b) { X = (1,); } return (X.typeCode(),); }
list<auto> sub gg63() { return (g56 + g56,); }
our binary g63;
list<auto> sub l64() { binary X; return (X,); }
list<auto> sub mm64(C9 o) { return (o.m,); }
list<auto> sub sm64() { return (C9::sm,); }
list<auto> sub br64(bool b) { binary X; if (b) { X = <01>; } return (X,); }
list<auto> sub l65() { binary X; return (X + X,); }
list<auto> sub mm65(C9 o) { return (o.m + o.m,); }
list<auto> sub sm65() { return (C9::sm + C9::sm,); }
list<auto> sub br65(bool b) { binary X; if (b) { X = <01>; } return (X + X,); }
list<auto> sub l66() { binary X; return (exists X,); }
list<auto> sub mm66(C9 o) { return (exists o.m,); }
list<auto> sub sm66() { return (exists C9::sm,); }
list<auto> sub br66(bool b) { binary X; if (b) { X = <01>; } return (exists X,); }
list<auto> sub l67() { binary X; return (X == X,); }
list<auto> sub mm67(C9 o) { return (o.m == o.m,); }
list<auto> sub sm67() { return (C9::sm == C9::sm,); }
list<auto> sub br67(bool b) { binary X; if (b) { X = <01>; } return (X == X,); }
list<auto> sub l68() { binary X; return (id(X),); }
list<auto> sub mm68(C9 o) { return (id(o.m),); }
list<auto> sub sm68() { return (id(C9::sm),); }
list<auto> sub br68(bool b) { binary X; if (b) { X = <01>; } return (id(X),); }
list<auto> sub l69() { binary X; return (X.typeCode(),); }
list<auto> sub mm69(C9 o) { return (o.m.typeCode(),); }
list<auto> sub sm69() { return (C9::sm.typeCode(),); }
list<auto> sub br69(bool b) { binary X; if (b) { X = <01>; } return (X.typeCode(),); }
list<auto> sub gg70() { return (g63 + g63,); }

show("local int lit", \l1());
show("member int lit", auto sub () { return mm1(new C0()); });
show("static int lit", \sm1());
show("branch int lit", auto sub () { return br1(False); });
show("local int plus", \l2());
show("member int plus", auto sub () { return mm2(new C0()); });
show("static int plus", \sm2());
show("branch int plus", auto sub () { return br2(False); });
show("local int exists", \l3());
show("member int exists", auto sub () { return mm3(new C0()); });
show("static int exists", \sm3());
show("branch int exists", auto sub () { return br3(False); });
show("local int eq", \l4());
show("member int eq", auto sub () { return mm4(new C0()); });
show("static int eq", \sm4());
show("branch int eq", auto sub () { return br4(False); });
show("local int arg", \l5());
show("member int arg", auto sub () { return mm5(new C0()); });
show("static int arg", \sm5());
show("branch int arg", auto sub () { return br5(False); });
show("local int typecode", \l6());
show("member int typecode", auto sub () { return mm6(new C0()); });
show("static int typecode", \sm6());
show("branch int typecode", auto sub () { return br6(False); });
show("local float lit", \l8());
show("member float lit", auto sub () { return mm8(new C1()); });
show("static float lit", \sm8());
show("branch float lit", auto sub () { return br8(False); });
show("local float plus", \l9());
show("member float plus", auto sub () { return mm9(new C1()); });
show("static float plus", \sm9());
show("branch float plus", auto sub () { return br9(False); });
show("local float exists", \l10());
show("member float exists", auto sub () { return mm10(new C1()); });
show("static float exists", \sm10());
show("branch float exists", auto sub () { return br10(False); });
show("local float eq", \l11());
show("member float eq", auto sub () { return mm11(new C1()); });
show("static float eq", \sm11());
show("branch float eq", auto sub () { return br11(False); });
show("local float arg", \l12());
show("member float arg", auto sub () { return mm12(new C1()); });
show("static float arg", \sm12());
show("branch float arg", auto sub () { return br12(False); });
show("local float typecode", \l13());
show("member float typecode", auto sub () { return mm13(new C1()); });
show("static float typecode", \sm13());
show("branch float typecode", auto sub () { return br13(False); });
show("local string lit", \l15());
show("member string lit", auto sub () { return mm15(new C2()); });
show("static string lit", \sm15());
show("branch string lit", auto sub () { return br15(False); });
show("local string plus", \l16());
show("member string plus", auto sub () { return mm16(new C2()); });
show("static string plus", \sm16());
show("branch string plus", auto sub () { return br16(False); });
show("local string exists", \l17());
show("member string exists", auto sub () { return mm17(new C2()); });
show("static string exists", \sm17());
show("branch string exists", auto sub () { return br17(False); });
show("local string eq", \l18());
show("member string eq", auto sub () { return mm18(new C2()); });
show("static string eq", \sm18());
show("branch string eq", auto sub () { return br18(False); });
show("local string arg", \l19());
show("member string arg", auto sub () { return mm19(new C2()); });
show("static string arg", \sm19());
show("branch string arg", auto sub () { return br19(False); });
show("local string typecode", \l20());
show("member string typecode", auto sub () { return mm20(new C2()); });
show("static string typecode", \sm20());
show("branch string typecode", auto sub () { return br20(False); });
show("local bool lit", \l22());
show("member bool lit", auto sub () { return mm22(new C3()); });
show("static bool lit", \sm22());
show("branch bool lit", auto sub () { return br22(False); });
show("local bool plus", \l23());
show("member bool plus", auto sub () { return mm23(new C3()); });
show("static bool plus", \sm23());
show("branch bool plus", auto sub () { return br23(False); });
show("local bool exists", \l24());
show("member bool exists", auto sub () { return mm24(new C3()); });
show("static bool exists", \sm24());
show("branch bool exists", auto sub () { return br24(False); });
show("local bool eq", \l25());
show("member bool eq", auto sub () { return mm25(new C3()); });
show("static bool eq", \sm25());
show("branch bool eq", auto sub () { return br25(False); });
show("local bool arg", \l26());
show("member bool arg", auto sub () { return mm26(new C3()); });
show("static bool arg", \sm26());
show("branch bool arg", auto sub () { return br26(False); });
show("local bool typecode", \l27());
show("member bool typecode", auto sub () { return mm27(new C3()); });
show("static bool typecode", \sm27());
show("branch bool typecode", auto sub () { return br27(False); });
show("local number lit", \l29());
show("member number lit", auto sub () { return mm29(new C4()); });
show("static number lit", \sm29());
show("branch number lit", auto sub () { return br29(False); });
show("local number plus", \l30());
show("member number plus", auto sub () { return mm30(new C4()); });
show("static number plus", \sm30());
show("branch number plus", auto sub () { return br30(False); });
show("local number exists", \l31());
show("member number exists", auto sub () { return mm31(new C4()); });
show("static number exists", \sm31());
show("branch number exists", auto sub () { return br31(False); });
show("local number eq", \l32());
show("member number eq", auto sub () { return mm32(new C4()); });
show("static number eq", \sm32());
show("branch number eq", auto sub () { return br32(False); });
show("local number arg", \l33());
show("member number arg", auto sub () { return mm33(new C4()); });
show("static number arg", \sm33());
show("branch number arg", auto sub () { return br33(False); });
show("local number typecode", \l34());
show("member number typecode", auto sub () { return mm34(new C4()); });
show("static number typecode", \sm34());
show("branch number typecode", auto sub () { return br34(False); });
show("local date lit", \l36());
show("member date lit", auto sub () { return mm36(new C5()); });
show("static date lit", \sm36());
show("branch date lit", auto sub () { return br36(False); });
show("local date plus", \l37());
show("member date plus", auto sub () { return mm37(new C5()); });
show("static date plus", \sm37());
show("branch date plus", auto sub () { return br37(False); });
show("local date exists", \l38());
show("member date exists", auto sub () { return mm38(new C5()); });
show("static date exists", \sm38());
show("branch date exists", auto sub () { return br38(False); });
show("local date eq", \l39());
show("member date eq", auto sub () { return mm39(new C5()); });
show("static date eq", \sm39());
show("branch date eq", auto sub () { return br39(False); });
show("local date arg", \l40());
show("member date arg", auto sub () { return mm40(new C5()); });
show("static date arg", \sm40());
show("branch date arg", auto sub () { return br40(False); });
show("local date typecode", \l41());
show("member date typecode", auto sub () { return mm41(new C5()); });
show("static date typecode", \sm41());
show("branch date typecode", auto sub () { return br41(False); });
show("local list<int> lit", \l43());
show("member list<int> lit", auto sub () { return mm43(new C6()); });
show("static list<int> lit", \sm43());
show("branch list<int> lit", auto sub () { return br43(False); });
show("local list<int> plus", \l44());
show("member list<int> plus", auto sub () { return mm44(new C6()); });
show("static list<int> plus", \sm44());
show("branch list<int> plus", auto sub () { return br44(False); });
show("local list<int> exists", \l45());
show("member list<int> exists", auto sub () { return mm45(new C6()); });
show("static list<int> exists", \sm45());
show("branch list<int> exists", auto sub () { return br45(False); });
show("local list<int> eq", \l46());
show("member list<int> eq", auto sub () { return mm46(new C6()); });
show("static list<int> eq", \sm46());
show("branch list<int> eq", auto sub () { return br46(False); });
show("local list<int> arg", \l47());
show("member list<int> arg", auto sub () { return mm47(new C6()); });
show("static list<int> arg", \sm47());
show("branch list<int> arg", auto sub () { return br47(False); });
show("local list<int> typecode", \l48());
show("member list<int> typecode", auto sub () { return mm48(new C6()); });
show("static list<int> typecode", \sm48());
show("branch list<int> typecode", auto sub () { return br48(False); });
show("local hash<string, int> lit", \l50());
show("member hash<string, int> lit", auto sub () { return mm50(new C7()); });
show("static hash<string, int> lit", \sm50());
show("branch hash<string, int> lit", auto sub () { return br50(False); });
show("local hash<string, int> plus", \l51());
show("member hash<string, int> plus", auto sub () { return mm51(new C7()); });
show("static hash<string, int> plus", \sm51());
show("branch hash<string, int> plus", auto sub () { return br51(False); });
show("local hash<string, int> exists", \l52());
show("member hash<string, int> exists", auto sub () { return mm52(new C7()); });
show("static hash<string, int> exists", \sm52());
show("branch hash<string, int> exists", auto sub () { return br52(False); });
show("local hash<string, int> eq", \l53());
show("member hash<string, int> eq", auto sub () { return mm53(new C7()); });
show("static hash<string, int> eq", \sm53());
show("branch hash<string, int> eq", auto sub () { return br53(False); });
show("local hash<string, int> arg", \l54());
show("member hash<string, int> arg", auto sub () { return mm54(new C7()); });
show("static hash<string, int> arg", \sm54());
show("branch hash<string, int> arg", auto sub () { return br54(False); });
show("local hash<string, int> typecode", \l55());
show("member hash<string, int> typecode", auto sub () { return mm55(new C7()); });
show("static hash<string, int> typecode", \sm55());
show("branch hash<string, int> typecode", auto sub () { return br55(False); });
show("local softlist<int> lit", \l57());
show("member softlist<int> lit", auto sub () { return mm57(new C8()); });
show("static softlist<int> lit", \sm57());
show("branch softlist<int> lit", auto sub () { return br57(False); });
show("local softlist<int> plus", \l58());
show("member softlist<int> plus", auto sub () { return mm58(new C8()); });
show("static softlist<int> plus", \sm58());
show("branch softlist<int> plus", auto sub () { return br58(False); });
show("local softlist<int> exists", \l59());
show("member softlist<int> exists", auto sub () { return mm59(new C8()); });
show("static softlist<int> exists", \sm59());
show("branch softlist<int> exists", auto sub () { return br59(False); });
show("local softlist<int> eq", \l60());
show("member softlist<int> eq", auto sub () { return mm60(new C8()); });
show("static softlist<int> eq", \sm60());
show("branch softlist<int> eq", auto sub () { return br60(False); });
show("local softlist<int> arg", \l61());
show("member softlist<int> arg", auto sub () { return mm61(new C8()); });
show("static softlist<int> arg", \sm61());
show("branch softlist<int> arg", auto sub () { return br61(False); });
show("local softlist<int> typecode", \l62());
show("member softlist<int> typecode", auto sub () { return mm62(new C8()); });
show("static softlist<int> typecode", \sm62());
show("branch softlist<int> typecode", auto sub () { return br62(False); });
show("local binary lit", \l64());
show("member binary lit", auto sub () { return mm64(new C9()); });
show("static binary lit", \sm64());
show("branch binary lit", auto sub () { return br64(False); });
show("local binary plus", \l65());
show("member binary plus", auto sub () { return mm65(new C9()); });
show("static binary plus", \sm65());
show("branch binary plus", auto sub () { return br65(False); });
show("local binary exists", \l66());
show("member binary exists", auto sub () { return mm66(new C9()); });
show("static binary exists", \sm66());
show("branch binary exists", auto sub () { return br66(False); });
show("local binary eq", \l67());
show("member binary eq", auto sub () { return mm67(new C9()); });
show("static binary eq", \sm67());
show("branch binary eq", auto sub () { return br67(False); });
show("local binary arg", \l68());
show("member binary arg", auto sub () { return mm68(new C9()); });
show("static binary arg", \sm68());
show("branch binary arg", auto sub () { return br68(False); });
show("local binary typecode", \l69());
show("member binary typecode", auto sub () { return mm69(new C9()); });
show("static binary typecode", \sm69());
show("branch binary typecode", auto sub () { return br69(False); });
