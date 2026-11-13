#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the list cases of part 5 of ArithParseResultTypes.qc (generated for the operand types and values below): each named
# function adds parameters of declared types, a list and a list or a list and an element (which are not narrowed),
# with the + operator and returns the result in a list literal, whose element type is the parse result type of the
# operator in the compiled modes (the AST interpreter takes it from the value); each case is called with values that
# the declared types allow (empty and non-empty lists, soft list values, or-nothing and untyped elements, values of a
# subclass, typed hashes in a hash<auto>, date subtypes) and prints the list with its full type and the full type of
# the result; the output of every execution mode must be that of the AST interpreter

%modern

hashdecl D {
    int x = 1;
}

class Base {
}

class Derived inherits Base {
}

sub show(string label, code c, softlist<auto> args) {
    try {
        auto v = call_function_args(c, args);
        printf("%s => %y %s %s\n", label, v, v.fullType(), v ? v[0].fullType() : "-");
    } catch (hash<ExceptionInfo> ex) {
        printf("%s => %s\n", label, ex.err);
    }
}

list<auto> sub f1(list a, list b) { return (a + b,); }
list<auto> sub f2(list a, list<auto> b) { return (a + b,); }
list<auto> sub f3(list a, list<int> b) { return (a + b,); }
list<auto> sub f4(list a, list<string> b) { return (a + b,); }
list<auto> sub f5(list a, list<*int> b) { return (a + b,); }
list<auto> sub f6(list a, softlist<int> b) { return (a + b,); }
list<auto> sub f7(list a, softlist b) { return (a + b,); }
list<auto> sub f8(list a, list<softint> b) { return (a + b,); }
list<auto> sub f9(list a, list<Base> b) { return (a + b,); }
list<auto> sub f10(list a, list<Derived> b) { return (a + b,); }
list<auto> sub f11(list a, list<date> b) { return (a + b,); }
list<auto> sub f12(list a, list<hash<string, int>> b) { return (a + b,); }
list<auto> sub f13(list a, int b) { return (a + b,); }
list<auto> sub f14(int a, list b) { return (a + b,); }
list<auto> sub f15(list a, softint b) { return (a + b,); }
list<auto> sub f16(softint a, list b) { return (a + b,); }
list<auto> sub f17(list a, *int b) { return (a + b,); }
list<auto> sub f18(*int a, list b) { return (a + b,); }
list<auto> sub f19(list a, string b) { return (a + b,); }
list<auto> sub f20(string a, list b) { return (a + b,); }
list<auto> sub f21(list a, hash<D> b) { return (a + b,); }
list<auto> sub f22(hash<D> a, list b) { return (a + b,); }
list<auto> sub f23(list a, union<int, string> b) { return (a + b,); }
list<auto> sub f24(union<int, string> a, list b) { return (a + b,); }
list<auto> sub f25(list a, auto b) { return (a + b,); }
list<auto> sub f26(auto a, list b) { return (a + b,); }
list<auto> sub f27(list a, float b) { return (a + b,); }
list<auto> sub f28(float a, list b) { return (a + b,); }
list<auto> sub f29(list a, timeout b) { return (a + b,); }
list<auto> sub f30(timeout a, list b) { return (a + b,); }
list<auto> sub f31(list a, object b) { return (a + b,); }
list<auto> sub f32(object a, list b) { return (a + b,); }
list<auto> sub f33(list a, Base b) { return (a + b,); }
list<auto> sub f34(Base a, list b) { return (a + b,); }
list<auto> sub f35(list a, hash<auto> b) { return (a + b,); }
list<auto> sub f36(hash<auto> a, list b) { return (a + b,); }
list<auto> sub f37(list a, date b) { return (a + b,); }
list<auto> sub f38(date a, list b) { return (a + b,); }
list<auto> sub f39(list a, date<relative> b) { return (a + b,); }
list<auto> sub f40(date<relative> a, list b) { return (a + b,); }
list<auto> sub f41(list a, bool b) { return (a + b,); }
list<auto> sub f42(bool a, list b) { return (a + b,); }
list<auto> sub f43(list a, binary b) { return (a + b,); }
list<auto> sub f44(binary a, list b) { return (a + b,); }
list<auto> sub f45(list<auto> a, list b) { return (a + b,); }
list<auto> sub f46(list<auto> a, list<auto> b) { return (a + b,); }
list<auto> sub f47(list<auto> a, list<int> b) { return (a + b,); }
list<auto> sub f48(list<auto> a, list<string> b) { return (a + b,); }
list<auto> sub f49(list<auto> a, list<*int> b) { return (a + b,); }
list<auto> sub f50(list<auto> a, softlist<int> b) { return (a + b,); }
list<auto> sub f51(list<auto> a, softlist b) { return (a + b,); }
list<auto> sub f52(list<auto> a, list<softint> b) { return (a + b,); }
list<auto> sub f53(list<auto> a, list<Base> b) { return (a + b,); }
list<auto> sub f54(list<auto> a, list<Derived> b) { return (a + b,); }
list<auto> sub f55(list<auto> a, list<date> b) { return (a + b,); }
list<auto> sub f56(list<auto> a, list<hash<string, int>> b) { return (a + b,); }
list<auto> sub f57(list<auto> a, int b) { return (a + b,); }
list<auto> sub f58(int a, list<auto> b) { return (a + b,); }
list<auto> sub f59(list<auto> a, softint b) { return (a + b,); }
list<auto> sub f60(softint a, list<auto> b) { return (a + b,); }
list<auto> sub f61(list<auto> a, *int b) { return (a + b,); }
list<auto> sub f62(*int a, list<auto> b) { return (a + b,); }
list<auto> sub f63(list<auto> a, string b) { return (a + b,); }
list<auto> sub f64(string a, list<auto> b) { return (a + b,); }
list<auto> sub f65(list<auto> a, hash<D> b) { return (a + b,); }
list<auto> sub f66(hash<D> a, list<auto> b) { return (a + b,); }
list<auto> sub f67(list<auto> a, union<int, string> b) { return (a + b,); }
list<auto> sub f68(union<int, string> a, list<auto> b) { return (a + b,); }
list<auto> sub f69(list<auto> a, auto b) { return (a + b,); }
list<auto> sub f70(auto a, list<auto> b) { return (a + b,); }
list<auto> sub f71(list<auto> a, float b) { return (a + b,); }
list<auto> sub f72(float a, list<auto> b) { return (a + b,); }
list<auto> sub f73(list<auto> a, timeout b) { return (a + b,); }
list<auto> sub f74(timeout a, list<auto> b) { return (a + b,); }
list<auto> sub f75(list<auto> a, object b) { return (a + b,); }
list<auto> sub f76(object a, list<auto> b) { return (a + b,); }
list<auto> sub f77(list<auto> a, Base b) { return (a + b,); }
list<auto> sub f78(Base a, list<auto> b) { return (a + b,); }
list<auto> sub f79(list<auto> a, hash<auto> b) { return (a + b,); }
list<auto> sub f80(hash<auto> a, list<auto> b) { return (a + b,); }
list<auto> sub f81(list<auto> a, date b) { return (a + b,); }
list<auto> sub f82(date a, list<auto> b) { return (a + b,); }
list<auto> sub f83(list<auto> a, date<relative> b) { return (a + b,); }
list<auto> sub f84(date<relative> a, list<auto> b) { return (a + b,); }
list<auto> sub f85(list<auto> a, bool b) { return (a + b,); }
list<auto> sub f86(bool a, list<auto> b) { return (a + b,); }
list<auto> sub f87(list<auto> a, binary b) { return (a + b,); }
list<auto> sub f88(binary a, list<auto> b) { return (a + b,); }
list<auto> sub f89(list<int> a, list b) { return (a + b,); }
list<auto> sub f90(list<int> a, list<auto> b) { return (a + b,); }
list<auto> sub f91(list<int> a, list<int> b) { return (a + b,); }
list<auto> sub f92(list<int> a, list<string> b) { return (a + b,); }
list<auto> sub f93(list<int> a, list<*int> b) { return (a + b,); }
list<auto> sub f94(list<int> a, softlist<int> b) { return (a + b,); }
list<auto> sub f95(list<int> a, softlist b) { return (a + b,); }
list<auto> sub f96(list<int> a, list<softint> b) { return (a + b,); }
list<auto> sub f97(list<int> a, list<Base> b) { return (a + b,); }
list<auto> sub f98(list<int> a, list<Derived> b) { return (a + b,); }
list<auto> sub f99(list<int> a, list<date> b) { return (a + b,); }
list<auto> sub f100(list<int> a, list<hash<string, int>> b) { return (a + b,); }
list<auto> sub f101(list<int> a, int b) { return (a + b,); }
list<auto> sub f102(int a, list<int> b) { return (a + b,); }
list<auto> sub f103(list<int> a, softint b) { return (a + b,); }
list<auto> sub f104(softint a, list<int> b) { return (a + b,); }
list<auto> sub f105(list<int> a, *int b) { return (a + b,); }
list<auto> sub f106(*int a, list<int> b) { return (a + b,); }
list<auto> sub f107(list<int> a, string b) { return (a + b,); }
list<auto> sub f108(string a, list<int> b) { return (a + b,); }
list<auto> sub f109(list<int> a, hash<D> b) { return (a + b,); }
list<auto> sub f110(hash<D> a, list<int> b) { return (a + b,); }
list<auto> sub f111(list<int> a, union<int, string> b) { return (a + b,); }
list<auto> sub f112(union<int, string> a, list<int> b) { return (a + b,); }
list<auto> sub f113(list<int> a, auto b) { return (a + b,); }
list<auto> sub f114(auto a, list<int> b) { return (a + b,); }
list<auto> sub f115(list<int> a, float b) { return (a + b,); }
list<auto> sub f116(float a, list<int> b) { return (a + b,); }
list<auto> sub f117(list<int> a, timeout b) { return (a + b,); }
list<auto> sub f118(timeout a, list<int> b) { return (a + b,); }
list<auto> sub f119(list<int> a, object b) { return (a + b,); }
list<auto> sub f120(object a, list<int> b) { return (a + b,); }
list<auto> sub f121(list<int> a, Base b) { return (a + b,); }
list<auto> sub f122(Base a, list<int> b) { return (a + b,); }
list<auto> sub f123(list<int> a, hash<auto> b) { return (a + b,); }
list<auto> sub f124(hash<auto> a, list<int> b) { return (a + b,); }
list<auto> sub f125(list<int> a, date b) { return (a + b,); }
list<auto> sub f126(date a, list<int> b) { return (a + b,); }
list<auto> sub f127(list<int> a, date<relative> b) { return (a + b,); }
list<auto> sub f128(date<relative> a, list<int> b) { return (a + b,); }
list<auto> sub f129(list<int> a, bool b) { return (a + b,); }
list<auto> sub f130(bool a, list<int> b) { return (a + b,); }
list<auto> sub f131(list<int> a, binary b) { return (a + b,); }
list<auto> sub f132(binary a, list<int> b) { return (a + b,); }
list<auto> sub f133(list<string> a, list b) { return (a + b,); }
list<auto> sub f134(list<string> a, list<auto> b) { return (a + b,); }
list<auto> sub f135(list<string> a, list<int> b) { return (a + b,); }
list<auto> sub f136(list<string> a, list<string> b) { return (a + b,); }
list<auto> sub f137(list<string> a, list<*int> b) { return (a + b,); }
list<auto> sub f138(list<string> a, softlist<int> b) { return (a + b,); }
list<auto> sub f139(list<string> a, softlist b) { return (a + b,); }
list<auto> sub f140(list<string> a, list<softint> b) { return (a + b,); }
list<auto> sub f141(list<string> a, list<Base> b) { return (a + b,); }
list<auto> sub f142(list<string> a, list<Derived> b) { return (a + b,); }
list<auto> sub f143(list<string> a, list<date> b) { return (a + b,); }
list<auto> sub f144(list<string> a, list<hash<string, int>> b) { return (a + b,); }
list<auto> sub f145(list<string> a, int b) { return (a + b,); }
list<auto> sub f146(int a, list<string> b) { return (a + b,); }
list<auto> sub f147(list<string> a, softint b) { return (a + b,); }
list<auto> sub f148(softint a, list<string> b) { return (a + b,); }
list<auto> sub f149(list<string> a, *int b) { return (a + b,); }
list<auto> sub f150(*int a, list<string> b) { return (a + b,); }
list<auto> sub f151(list<string> a, string b) { return (a + b,); }
list<auto> sub f152(string a, list<string> b) { return (a + b,); }
list<auto> sub f153(list<string> a, hash<D> b) { return (a + b,); }
list<auto> sub f154(hash<D> a, list<string> b) { return (a + b,); }
list<auto> sub f155(list<string> a, union<int, string> b) { return (a + b,); }
list<auto> sub f156(union<int, string> a, list<string> b) { return (a + b,); }
list<auto> sub f157(list<string> a, auto b) { return (a + b,); }
list<auto> sub f158(auto a, list<string> b) { return (a + b,); }
list<auto> sub f159(list<string> a, float b) { return (a + b,); }
list<auto> sub f160(float a, list<string> b) { return (a + b,); }
list<auto> sub f161(list<string> a, timeout b) { return (a + b,); }
list<auto> sub f162(timeout a, list<string> b) { return (a + b,); }
list<auto> sub f163(list<string> a, object b) { return (a + b,); }
list<auto> sub f164(object a, list<string> b) { return (a + b,); }
list<auto> sub f165(list<string> a, Base b) { return (a + b,); }
list<auto> sub f166(Base a, list<string> b) { return (a + b,); }
list<auto> sub f167(list<string> a, hash<auto> b) { return (a + b,); }
list<auto> sub f168(hash<auto> a, list<string> b) { return (a + b,); }
list<auto> sub f169(list<string> a, date b) { return (a + b,); }
list<auto> sub f170(date a, list<string> b) { return (a + b,); }
list<auto> sub f171(list<string> a, date<relative> b) { return (a + b,); }
list<auto> sub f172(date<relative> a, list<string> b) { return (a + b,); }
list<auto> sub f173(list<string> a, bool b) { return (a + b,); }
list<auto> sub f174(bool a, list<string> b) { return (a + b,); }
list<auto> sub f175(list<string> a, binary b) { return (a + b,); }
list<auto> sub f176(binary a, list<string> b) { return (a + b,); }
list<auto> sub f177(list<*int> a, list b) { return (a + b,); }
list<auto> sub f178(list<*int> a, list<auto> b) { return (a + b,); }
list<auto> sub f179(list<*int> a, list<int> b) { return (a + b,); }
list<auto> sub f180(list<*int> a, list<string> b) { return (a + b,); }
list<auto> sub f181(list<*int> a, list<*int> b) { return (a + b,); }
list<auto> sub f182(list<*int> a, softlist<int> b) { return (a + b,); }
list<auto> sub f183(list<*int> a, softlist b) { return (a + b,); }
list<auto> sub f184(list<*int> a, list<softint> b) { return (a + b,); }
list<auto> sub f185(list<*int> a, list<Base> b) { return (a + b,); }
list<auto> sub f186(list<*int> a, list<Derived> b) { return (a + b,); }
list<auto> sub f187(list<*int> a, list<date> b) { return (a + b,); }
list<auto> sub f188(list<*int> a, list<hash<string, int>> b) { return (a + b,); }
list<auto> sub f189(list<*int> a, int b) { return (a + b,); }
list<auto> sub f190(int a, list<*int> b) { return (a + b,); }
list<auto> sub f191(list<*int> a, softint b) { return (a + b,); }
list<auto> sub f192(softint a, list<*int> b) { return (a + b,); }
list<auto> sub f193(list<*int> a, *int b) { return (a + b,); }
list<auto> sub f194(*int a, list<*int> b) { return (a + b,); }
list<auto> sub f195(list<*int> a, string b) { return (a + b,); }
list<auto> sub f196(string a, list<*int> b) { return (a + b,); }
list<auto> sub f197(list<*int> a, hash<D> b) { return (a + b,); }
list<auto> sub f198(hash<D> a, list<*int> b) { return (a + b,); }
list<auto> sub f199(list<*int> a, union<int, string> b) { return (a + b,); }
list<auto> sub f200(union<int, string> a, list<*int> b) { return (a + b,); }
list<auto> sub f201(list<*int> a, auto b) { return (a + b,); }
list<auto> sub f202(auto a, list<*int> b) { return (a + b,); }
list<auto> sub f203(list<*int> a, float b) { return (a + b,); }
list<auto> sub f204(float a, list<*int> b) { return (a + b,); }
list<auto> sub f205(list<*int> a, timeout b) { return (a + b,); }
list<auto> sub f206(timeout a, list<*int> b) { return (a + b,); }
list<auto> sub f207(list<*int> a, object b) { return (a + b,); }
list<auto> sub f208(object a, list<*int> b) { return (a + b,); }
list<auto> sub f209(list<*int> a, Base b) { return (a + b,); }
list<auto> sub f210(Base a, list<*int> b) { return (a + b,); }
list<auto> sub f211(list<*int> a, hash<auto> b) { return (a + b,); }
list<auto> sub f212(hash<auto> a, list<*int> b) { return (a + b,); }
list<auto> sub f213(list<*int> a, date b) { return (a + b,); }
list<auto> sub f214(date a, list<*int> b) { return (a + b,); }
list<auto> sub f215(list<*int> a, date<relative> b) { return (a + b,); }
list<auto> sub f216(date<relative> a, list<*int> b) { return (a + b,); }
list<auto> sub f217(list<*int> a, bool b) { return (a + b,); }
list<auto> sub f218(bool a, list<*int> b) { return (a + b,); }
list<auto> sub f219(list<*int> a, binary b) { return (a + b,); }
list<auto> sub f220(binary a, list<*int> b) { return (a + b,); }
list<auto> sub f221(softlist<int> a, list b) { return (a + b,); }
list<auto> sub f222(softlist<int> a, list<auto> b) { return (a + b,); }
list<auto> sub f223(softlist<int> a, list<int> b) { return (a + b,); }
list<auto> sub f224(softlist<int> a, list<string> b) { return (a + b,); }
list<auto> sub f225(softlist<int> a, list<*int> b) { return (a + b,); }
list<auto> sub f226(softlist<int> a, softlist<int> b) { return (a + b,); }
list<auto> sub f227(softlist<int> a, softlist b) { return (a + b,); }
list<auto> sub f228(softlist<int> a, list<softint> b) { return (a + b,); }
list<auto> sub f229(softlist<int> a, list<Base> b) { return (a + b,); }
list<auto> sub f230(softlist<int> a, list<Derived> b) { return (a + b,); }
list<auto> sub f231(softlist<int> a, list<date> b) { return (a + b,); }
list<auto> sub f232(softlist<int> a, list<hash<string, int>> b) { return (a + b,); }
list<auto> sub f233(softlist<int> a, int b) { return (a + b,); }
list<auto> sub f234(int a, softlist<int> b) { return (a + b,); }
list<auto> sub f235(softlist<int> a, softint b) { return (a + b,); }
list<auto> sub f236(softint a, softlist<int> b) { return (a + b,); }
list<auto> sub f237(softlist<int> a, *int b) { return (a + b,); }
list<auto> sub f238(*int a, softlist<int> b) { return (a + b,); }
list<auto> sub f239(softlist<int> a, string b) { return (a + b,); }
list<auto> sub f240(string a, softlist<int> b) { return (a + b,); }
list<auto> sub f241(softlist<int> a, hash<D> b) { return (a + b,); }
list<auto> sub f242(hash<D> a, softlist<int> b) { return (a + b,); }
list<auto> sub f243(softlist<int> a, union<int, string> b) { return (a + b,); }
list<auto> sub f244(union<int, string> a, softlist<int> b) { return (a + b,); }
list<auto> sub f245(softlist<int> a, auto b) { return (a + b,); }
list<auto> sub f246(auto a, softlist<int> b) { return (a + b,); }
list<auto> sub f247(softlist<int> a, float b) { return (a + b,); }
list<auto> sub f248(float a, softlist<int> b) { return (a + b,); }
list<auto> sub f249(softlist<int> a, timeout b) { return (a + b,); }
list<auto> sub f250(timeout a, softlist<int> b) { return (a + b,); }
list<auto> sub f251(softlist<int> a, object b) { return (a + b,); }
list<auto> sub f252(object a, softlist<int> b) { return (a + b,); }
list<auto> sub f253(softlist<int> a, Base b) { return (a + b,); }
list<auto> sub f254(Base a, softlist<int> b) { return (a + b,); }
list<auto> sub f255(softlist<int> a, hash<auto> b) { return (a + b,); }
list<auto> sub f256(hash<auto> a, softlist<int> b) { return (a + b,); }
list<auto> sub f257(softlist<int> a, date b) { return (a + b,); }
list<auto> sub f258(date a, softlist<int> b) { return (a + b,); }
list<auto> sub f259(softlist<int> a, date<relative> b) { return (a + b,); }
list<auto> sub f260(date<relative> a, softlist<int> b) { return (a + b,); }
list<auto> sub f261(softlist<int> a, bool b) { return (a + b,); }
list<auto> sub f262(bool a, softlist<int> b) { return (a + b,); }
list<auto> sub f263(softlist<int> a, binary b) { return (a + b,); }
list<auto> sub f264(binary a, softlist<int> b) { return (a + b,); }
list<auto> sub f265(softlist a, list b) { return (a + b,); }
list<auto> sub f266(softlist a, list<auto> b) { return (a + b,); }
list<auto> sub f267(softlist a, list<int> b) { return (a + b,); }
list<auto> sub f268(softlist a, list<string> b) { return (a + b,); }
list<auto> sub f269(softlist a, list<*int> b) { return (a + b,); }
list<auto> sub f270(softlist a, softlist<int> b) { return (a + b,); }
list<auto> sub f271(softlist a, softlist b) { return (a + b,); }
list<auto> sub f272(softlist a, list<softint> b) { return (a + b,); }
list<auto> sub f273(softlist a, list<Base> b) { return (a + b,); }
list<auto> sub f274(softlist a, list<Derived> b) { return (a + b,); }
list<auto> sub f275(softlist a, list<date> b) { return (a + b,); }
list<auto> sub f276(softlist a, list<hash<string, int>> b) { return (a + b,); }
list<auto> sub f277(softlist a, int b) { return (a + b,); }
list<auto> sub f278(int a, softlist b) { return (a + b,); }
list<auto> sub f279(softlist a, softint b) { return (a + b,); }
list<auto> sub f280(softint a, softlist b) { return (a + b,); }
list<auto> sub f281(softlist a, *int b) { return (a + b,); }
list<auto> sub f282(*int a, softlist b) { return (a + b,); }
list<auto> sub f283(softlist a, string b) { return (a + b,); }
list<auto> sub f284(string a, softlist b) { return (a + b,); }
list<auto> sub f285(softlist a, hash<D> b) { return (a + b,); }
list<auto> sub f286(hash<D> a, softlist b) { return (a + b,); }
list<auto> sub f287(softlist a, union<int, string> b) { return (a + b,); }
list<auto> sub f288(union<int, string> a, softlist b) { return (a + b,); }
list<auto> sub f289(softlist a, auto b) { return (a + b,); }
list<auto> sub f290(auto a, softlist b) { return (a + b,); }
list<auto> sub f291(softlist a, float b) { return (a + b,); }
list<auto> sub f292(float a, softlist b) { return (a + b,); }
list<auto> sub f293(softlist a, timeout b) { return (a + b,); }
list<auto> sub f294(timeout a, softlist b) { return (a + b,); }
list<auto> sub f295(softlist a, object b) { return (a + b,); }
list<auto> sub f296(object a, softlist b) { return (a + b,); }
list<auto> sub f297(softlist a, Base b) { return (a + b,); }
list<auto> sub f298(Base a, softlist b) { return (a + b,); }
list<auto> sub f299(softlist a, hash<auto> b) { return (a + b,); }
list<auto> sub f300(hash<auto> a, softlist b) { return (a + b,); }
list<auto> sub f301(softlist a, date b) { return (a + b,); }
list<auto> sub f302(date a, softlist b) { return (a + b,); }
list<auto> sub f303(softlist a, date<relative> b) { return (a + b,); }
list<auto> sub f304(date<relative> a, softlist b) { return (a + b,); }
list<auto> sub f305(softlist a, bool b) { return (a + b,); }
list<auto> sub f306(bool a, softlist b) { return (a + b,); }
list<auto> sub f307(softlist a, binary b) { return (a + b,); }
list<auto> sub f308(binary a, softlist b) { return (a + b,); }
list<auto> sub f309(list<softint> a, list b) { return (a + b,); }
list<auto> sub f310(list<softint> a, list<auto> b) { return (a + b,); }
list<auto> sub f311(list<softint> a, list<int> b) { return (a + b,); }
list<auto> sub f312(list<softint> a, list<string> b) { return (a + b,); }
list<auto> sub f313(list<softint> a, list<*int> b) { return (a + b,); }
list<auto> sub f314(list<softint> a, softlist<int> b) { return (a + b,); }
list<auto> sub f315(list<softint> a, softlist b) { return (a + b,); }
list<auto> sub f316(list<softint> a, list<softint> b) { return (a + b,); }
list<auto> sub f317(list<softint> a, list<Base> b) { return (a + b,); }
list<auto> sub f318(list<softint> a, list<Derived> b) { return (a + b,); }
list<auto> sub f319(list<softint> a, list<date> b) { return (a + b,); }
list<auto> sub f320(list<softint> a, list<hash<string, int>> b) { return (a + b,); }
list<auto> sub f321(list<softint> a, int b) { return (a + b,); }
list<auto> sub f322(int a, list<softint> b) { return (a + b,); }
list<auto> sub f323(list<softint> a, softint b) { return (a + b,); }
list<auto> sub f324(softint a, list<softint> b) { return (a + b,); }
list<auto> sub f325(list<softint> a, *int b) { return (a + b,); }
list<auto> sub f326(*int a, list<softint> b) { return (a + b,); }
list<auto> sub f327(list<softint> a, string b) { return (a + b,); }
list<auto> sub f328(string a, list<softint> b) { return (a + b,); }
list<auto> sub f329(list<softint> a, hash<D> b) { return (a + b,); }
list<auto> sub f330(hash<D> a, list<softint> b) { return (a + b,); }
list<auto> sub f331(list<softint> a, union<int, string> b) { return (a + b,); }
list<auto> sub f332(union<int, string> a, list<softint> b) { return (a + b,); }
list<auto> sub f333(list<softint> a, auto b) { return (a + b,); }
list<auto> sub f334(auto a, list<softint> b) { return (a + b,); }
list<auto> sub f335(list<softint> a, float b) { return (a + b,); }
list<auto> sub f336(float a, list<softint> b) { return (a + b,); }
list<auto> sub f337(list<softint> a, timeout b) { return (a + b,); }
list<auto> sub f338(timeout a, list<softint> b) { return (a + b,); }
list<auto> sub f339(list<softint> a, object b) { return (a + b,); }
list<auto> sub f340(object a, list<softint> b) { return (a + b,); }
list<auto> sub f341(list<softint> a, Base b) { return (a + b,); }
list<auto> sub f342(Base a, list<softint> b) { return (a + b,); }
list<auto> sub f343(list<softint> a, hash<auto> b) { return (a + b,); }
list<auto> sub f344(hash<auto> a, list<softint> b) { return (a + b,); }
list<auto> sub f345(list<softint> a, date b) { return (a + b,); }
list<auto> sub f346(date a, list<softint> b) { return (a + b,); }
list<auto> sub f347(list<softint> a, date<relative> b) { return (a + b,); }
list<auto> sub f348(date<relative> a, list<softint> b) { return (a + b,); }
list<auto> sub f349(list<softint> a, bool b) { return (a + b,); }
list<auto> sub f350(bool a, list<softint> b) { return (a + b,); }
list<auto> sub f351(list<softint> a, binary b) { return (a + b,); }
list<auto> sub f352(binary a, list<softint> b) { return (a + b,); }
list<auto> sub f353(list<Base> a, list b) { return (a + b,); }
list<auto> sub f354(list<Base> a, list<auto> b) { return (a + b,); }
list<auto> sub f355(list<Base> a, list<int> b) { return (a + b,); }
list<auto> sub f356(list<Base> a, list<string> b) { return (a + b,); }
list<auto> sub f357(list<Base> a, list<*int> b) { return (a + b,); }
list<auto> sub f358(list<Base> a, softlist<int> b) { return (a + b,); }
list<auto> sub f359(list<Base> a, softlist b) { return (a + b,); }
list<auto> sub f360(list<Base> a, list<softint> b) { return (a + b,); }
list<auto> sub f361(list<Base> a, list<Base> b) { return (a + b,); }
list<auto> sub f362(list<Base> a, list<Derived> b) { return (a + b,); }
list<auto> sub f363(list<Base> a, list<date> b) { return (a + b,); }
list<auto> sub f364(list<Base> a, list<hash<string, int>> b) { return (a + b,); }
list<auto> sub f365(list<Base> a, int b) { return (a + b,); }
list<auto> sub f366(int a, list<Base> b) { return (a + b,); }
list<auto> sub f367(list<Base> a, softint b) { return (a + b,); }
list<auto> sub f368(softint a, list<Base> b) { return (a + b,); }
list<auto> sub f369(list<Base> a, *int b) { return (a + b,); }
list<auto> sub f370(*int a, list<Base> b) { return (a + b,); }
list<auto> sub f371(list<Base> a, string b) { return (a + b,); }
list<auto> sub f372(string a, list<Base> b) { return (a + b,); }
list<auto> sub f373(list<Base> a, hash<D> b) { return (a + b,); }
list<auto> sub f374(hash<D> a, list<Base> b) { return (a + b,); }
list<auto> sub f375(list<Base> a, union<int, string> b) { return (a + b,); }
list<auto> sub f376(union<int, string> a, list<Base> b) { return (a + b,); }
list<auto> sub f377(list<Base> a, auto b) { return (a + b,); }
list<auto> sub f378(auto a, list<Base> b) { return (a + b,); }
list<auto> sub f379(list<Base> a, float b) { return (a + b,); }
list<auto> sub f380(float a, list<Base> b) { return (a + b,); }
list<auto> sub f381(list<Base> a, timeout b) { return (a + b,); }
list<auto> sub f382(timeout a, list<Base> b) { return (a + b,); }
list<auto> sub f383(list<Base> a, object b) { return (a + b,); }
list<auto> sub f384(object a, list<Base> b) { return (a + b,); }
list<auto> sub f385(list<Base> a, Base b) { return (a + b,); }
list<auto> sub f386(Base a, list<Base> b) { return (a + b,); }
list<auto> sub f387(list<Base> a, hash<auto> b) { return (a + b,); }
list<auto> sub f388(hash<auto> a, list<Base> b) { return (a + b,); }
list<auto> sub f389(list<Base> a, date b) { return (a + b,); }
list<auto> sub f390(date a, list<Base> b) { return (a + b,); }
list<auto> sub f391(list<Base> a, date<relative> b) { return (a + b,); }
list<auto> sub f392(date<relative> a, list<Base> b) { return (a + b,); }
list<auto> sub f393(list<Base> a, bool b) { return (a + b,); }
list<auto> sub f394(bool a, list<Base> b) { return (a + b,); }
list<auto> sub f395(list<Base> a, binary b) { return (a + b,); }
list<auto> sub f396(binary a, list<Base> b) { return (a + b,); }
list<auto> sub f397(list<Derived> a, list b) { return (a + b,); }
list<auto> sub f398(list<Derived> a, list<auto> b) { return (a + b,); }
list<auto> sub f399(list<Derived> a, list<int> b) { return (a + b,); }
list<auto> sub f400(list<Derived> a, list<string> b) { return (a + b,); }
list<auto> sub f401(list<Derived> a, list<*int> b) { return (a + b,); }
list<auto> sub f402(list<Derived> a, softlist<int> b) { return (a + b,); }
list<auto> sub f403(list<Derived> a, softlist b) { return (a + b,); }
list<auto> sub f404(list<Derived> a, list<softint> b) { return (a + b,); }
list<auto> sub f405(list<Derived> a, list<Base> b) { return (a + b,); }
list<auto> sub f406(list<Derived> a, list<Derived> b) { return (a + b,); }
list<auto> sub f407(list<Derived> a, list<date> b) { return (a + b,); }
list<auto> sub f408(list<Derived> a, list<hash<string, int>> b) { return (a + b,); }
list<auto> sub f409(list<Derived> a, int b) { return (a + b,); }
list<auto> sub f410(int a, list<Derived> b) { return (a + b,); }
list<auto> sub f411(list<Derived> a, softint b) { return (a + b,); }
list<auto> sub f412(softint a, list<Derived> b) { return (a + b,); }
list<auto> sub f413(list<Derived> a, *int b) { return (a + b,); }
list<auto> sub f414(*int a, list<Derived> b) { return (a + b,); }
list<auto> sub f415(list<Derived> a, string b) { return (a + b,); }
list<auto> sub f416(string a, list<Derived> b) { return (a + b,); }
list<auto> sub f417(list<Derived> a, hash<D> b) { return (a + b,); }
list<auto> sub f418(hash<D> a, list<Derived> b) { return (a + b,); }
list<auto> sub f419(list<Derived> a, union<int, string> b) { return (a + b,); }
list<auto> sub f420(union<int, string> a, list<Derived> b) { return (a + b,); }
list<auto> sub f421(list<Derived> a, auto b) { return (a + b,); }
list<auto> sub f422(auto a, list<Derived> b) { return (a + b,); }
list<auto> sub f423(list<Derived> a, float b) { return (a + b,); }
list<auto> sub f424(float a, list<Derived> b) { return (a + b,); }
list<auto> sub f425(list<Derived> a, timeout b) { return (a + b,); }
list<auto> sub f426(timeout a, list<Derived> b) { return (a + b,); }
list<auto> sub f427(list<Derived> a, object b) { return (a + b,); }
list<auto> sub f428(object a, list<Derived> b) { return (a + b,); }
list<auto> sub f429(list<Derived> a, Base b) { return (a + b,); }
list<auto> sub f430(Base a, list<Derived> b) { return (a + b,); }
list<auto> sub f431(list<Derived> a, hash<auto> b) { return (a + b,); }
list<auto> sub f432(hash<auto> a, list<Derived> b) { return (a + b,); }
list<auto> sub f433(list<Derived> a, date b) { return (a + b,); }
list<auto> sub f434(date a, list<Derived> b) { return (a + b,); }
list<auto> sub f435(list<Derived> a, date<relative> b) { return (a + b,); }
list<auto> sub f436(date<relative> a, list<Derived> b) { return (a + b,); }
list<auto> sub f437(list<Derived> a, bool b) { return (a + b,); }
list<auto> sub f438(bool a, list<Derived> b) { return (a + b,); }
list<auto> sub f439(list<Derived> a, binary b) { return (a + b,); }
list<auto> sub f440(binary a, list<Derived> b) { return (a + b,); }
list<auto> sub f441(list<date> a, list b) { return (a + b,); }
list<auto> sub f442(list<date> a, list<auto> b) { return (a + b,); }
list<auto> sub f443(list<date> a, list<int> b) { return (a + b,); }
list<auto> sub f444(list<date> a, list<string> b) { return (a + b,); }
list<auto> sub f445(list<date> a, list<*int> b) { return (a + b,); }
list<auto> sub f446(list<date> a, softlist<int> b) { return (a + b,); }
list<auto> sub f447(list<date> a, softlist b) { return (a + b,); }
list<auto> sub f448(list<date> a, list<softint> b) { return (a + b,); }
list<auto> sub f449(list<date> a, list<Base> b) { return (a + b,); }
list<auto> sub f450(list<date> a, list<Derived> b) { return (a + b,); }
list<auto> sub f451(list<date> a, list<date> b) { return (a + b,); }
list<auto> sub f452(list<date> a, list<hash<string, int>> b) { return (a + b,); }
list<auto> sub f453(list<date> a, int b) { return (a + b,); }
list<auto> sub f454(int a, list<date> b) { return (a + b,); }
list<auto> sub f455(list<date> a, softint b) { return (a + b,); }
list<auto> sub f456(softint a, list<date> b) { return (a + b,); }
list<auto> sub f457(list<date> a, *int b) { return (a + b,); }
list<auto> sub f458(*int a, list<date> b) { return (a + b,); }
list<auto> sub f459(list<date> a, string b) { return (a + b,); }
list<auto> sub f460(string a, list<date> b) { return (a + b,); }
list<auto> sub f461(list<date> a, hash<D> b) { return (a + b,); }
list<auto> sub f462(hash<D> a, list<date> b) { return (a + b,); }
list<auto> sub f463(list<date> a, union<int, string> b) { return (a + b,); }
list<auto> sub f464(union<int, string> a, list<date> b) { return (a + b,); }
list<auto> sub f465(list<date> a, auto b) { return (a + b,); }
list<auto> sub f466(auto a, list<date> b) { return (a + b,); }
list<auto> sub f467(list<date> a, float b) { return (a + b,); }
list<auto> sub f468(float a, list<date> b) { return (a + b,); }
list<auto> sub f469(list<date> a, timeout b) { return (a + b,); }
list<auto> sub f470(timeout a, list<date> b) { return (a + b,); }
list<auto> sub f471(list<date> a, object b) { return (a + b,); }
list<auto> sub f472(object a, list<date> b) { return (a + b,); }
list<auto> sub f473(list<date> a, Base b) { return (a + b,); }
list<auto> sub f474(Base a, list<date> b) { return (a + b,); }
list<auto> sub f475(list<date> a, hash<auto> b) { return (a + b,); }
list<auto> sub f476(hash<auto> a, list<date> b) { return (a + b,); }
list<auto> sub f477(list<date> a, date b) { return (a + b,); }
list<auto> sub f478(date a, list<date> b) { return (a + b,); }
list<auto> sub f479(list<date> a, date<relative> b) { return (a + b,); }
list<auto> sub f480(date<relative> a, list<date> b) { return (a + b,); }
list<auto> sub f481(list<date> a, bool b) { return (a + b,); }
list<auto> sub f482(bool a, list<date> b) { return (a + b,); }
list<auto> sub f483(list<date> a, binary b) { return (a + b,); }
list<auto> sub f484(binary a, list<date> b) { return (a + b,); }
list<auto> sub f485(list<hash<string, int>> a, list b) { return (a + b,); }
list<auto> sub f486(list<hash<string, int>> a, list<auto> b) { return (a + b,); }
list<auto> sub f487(list<hash<string, int>> a, list<int> b) { return (a + b,); }
list<auto> sub f488(list<hash<string, int>> a, list<string> b) { return (a + b,); }
list<auto> sub f489(list<hash<string, int>> a, list<*int> b) { return (a + b,); }
list<auto> sub f490(list<hash<string, int>> a, softlist<int> b) { return (a + b,); }
list<auto> sub f491(list<hash<string, int>> a, softlist b) { return (a + b,); }
list<auto> sub f492(list<hash<string, int>> a, list<softint> b) { return (a + b,); }
list<auto> sub f493(list<hash<string, int>> a, list<Base> b) { return (a + b,); }
list<auto> sub f494(list<hash<string, int>> a, list<Derived> b) { return (a + b,); }
list<auto> sub f495(list<hash<string, int>> a, list<date> b) { return (a + b,); }
list<auto> sub f496(list<hash<string, int>> a, list<hash<string, int>> b) { return (a + b,); }
list<auto> sub f497(list<hash<string, int>> a, int b) { return (a + b,); }
list<auto> sub f498(int a, list<hash<string, int>> b) { return (a + b,); }
list<auto> sub f499(list<hash<string, int>> a, softint b) { return (a + b,); }
list<auto> sub f500(softint a, list<hash<string, int>> b) { return (a + b,); }
list<auto> sub f501(list<hash<string, int>> a, *int b) { return (a + b,); }
list<auto> sub f502(*int a, list<hash<string, int>> b) { return (a + b,); }
list<auto> sub f503(list<hash<string, int>> a, string b) { return (a + b,); }
list<auto> sub f504(string a, list<hash<string, int>> b) { return (a + b,); }
list<auto> sub f505(list<hash<string, int>> a, hash<D> b) { return (a + b,); }
list<auto> sub f506(hash<D> a, list<hash<string, int>> b) { return (a + b,); }
list<auto> sub f507(list<hash<string, int>> a, union<int, string> b) { return (a + b,); }
list<auto> sub f508(union<int, string> a, list<hash<string, int>> b) { return (a + b,); }
list<auto> sub f509(list<hash<string, int>> a, auto b) { return (a + b,); }
list<auto> sub f510(auto a, list<hash<string, int>> b) { return (a + b,); }
list<auto> sub f511(list<hash<string, int>> a, float b) { return (a + b,); }
list<auto> sub f512(float a, list<hash<string, int>> b) { return (a + b,); }
list<auto> sub f513(list<hash<string, int>> a, timeout b) { return (a + b,); }
list<auto> sub f514(timeout a, list<hash<string, int>> b) { return (a + b,); }
list<auto> sub f515(list<hash<string, int>> a, object b) { return (a + b,); }
list<auto> sub f516(object a, list<hash<string, int>> b) { return (a + b,); }
list<auto> sub f517(list<hash<string, int>> a, Base b) { return (a + b,); }
list<auto> sub f518(Base a, list<hash<string, int>> b) { return (a + b,); }
list<auto> sub f519(list<hash<string, int>> a, hash<auto> b) { return (a + b,); }
list<auto> sub f520(hash<auto> a, list<hash<string, int>> b) { return (a + b,); }
list<auto> sub f521(list<hash<string, int>> a, date b) { return (a + b,); }
list<auto> sub f522(date a, list<hash<string, int>> b) { return (a + b,); }
list<auto> sub f523(list<hash<string, int>> a, date<relative> b) { return (a + b,); }
list<auto> sub f524(date<relative> a, list<hash<string, int>> b) { return (a + b,); }
list<auto> sub f525(list<hash<string, int>> a, bool b) { return (a + b,); }
list<auto> sub f526(bool a, list<hash<string, int>> b) { return (a + b,); }
list<auto> sub f527(list<hash<string, int>> a, binary b) { return (a + b,); }
list<auto> sub f528(binary a, list<hash<string, int>> b) { return (a + b,); }

show("list + list ((1,), (1,))", \f1(), ((1,), (1,)));
show("list + list ((1,), ())", \f1(), ((1,), ()));
show("list + list ((), (1,))", \f1(), ((), (1,)));
show("list + list ((), ())", \f1(), ((), ()));
show("list + list<auto> ((1,), (1, \"a\"))", \f2(), ((1,), (1, "a")));
show("list + list<auto> ((1,), ())", \f2(), ((1,), ()));
show("list + list<auto> ((), (1, \"a\"))", \f2(), ((), (1, "a")));
show("list + list<auto> ((), ())", \f2(), ((), ()));
show("list + list<int> ((1,), (1,))", \f3(), ((1,), (1,)));
show("list + list<int> ((1,), ())", \f3(), ((1,), ()));
show("list + list<int> ((), (1,))", \f3(), ((), (1,)));
show("list + list<int> ((), ())", \f3(), ((), ()));
show("list + list<string> ((1,), (\"a\",))", \f4(), ((1,), ("a",)));
show("list + list<string> ((), (\"a\",))", \f4(), ((), ("a",)));
show("list + list<*int> ((1,), (1,))", \f5(), ((1,), (1,)));
show("list + list<*int> ((), (1,))", \f5(), ((), (1,)));
show("list + softlist<int> ((1,), (1,))", \f6(), ((1,), (1,)));
show("list + softlist<int> ((1,), 1)", \f6(), ((1,), 1));
show("list + softlist<int> ((), (1,))", \f6(), ((), (1,)));
show("list + softlist<int> ((), 1)", \f6(), ((), 1));
show("list + softlist ((1,), (1,))", \f7(), ((1,), (1,)));
show("list + softlist ((1,), 2)", \f7(), ((1,), 2));
show("list + softlist ((), (1,))", \f7(), ((), (1,)));
show("list + softlist ((), 2)", \f7(), ((), 2));
show("list + list<softint> ((1,), (1,))", \f8(), ((1,), (1,)));
show("list + list<softint> ((), (1,))", \f8(), ((), (1,)));
show("list + list<Base> ((1,), (new Derived(),))", \f9(), ((1,), (new Derived(),)));
show("list + list<Base> ((), (new Derived(),))", \f9(), ((), (new Derived(),)));
show("list + list<Derived> ((1,), (new Derived(),))", \f10(), ((1,), (new Derived(),)));
show("list + list<Derived> ((), (new Derived(),))", \f10(), ((), (new Derived(),)));
show("list + list<date> ((1,), (1h,))", \f11(), ((1,), (1h,)));
show("list + list<date> ((), (1h,))", \f11(), ((), (1h,)));
show("list + list<hash<string, int>> ((1,), ({\"a\": 1},))", \f12(), ((1,), ({"a": 1},)));
show("list + list<hash<string, int>> ((), ({\"a\": 1},))", \f12(), ((), ({"a": 1},)));
show("list + int ((1,), 2)", \f13(), ((1,), 2));
show("list + int ((), 2)", \f13(), ((), 2));
show("int + list (2, (1,))", \f14(), (2, (1,)));
show("int + list (2, ())", \f14(), (2, ()));
show("list + softint ((1,), 2)", \f15(), ((1,), 2));
show("list + softint ((1,), \"3\")", \f15(), ((1,), "3"));
show("list + softint ((), 2)", \f15(), ((), 2));
show("list + softint ((), \"3\")", \f15(), ((), "3"));
show("softint + list (2, (1,))", \f16(), (2, (1,)));
show("softint + list (2, ())", \f16(), (2, ()));
show("softint + list (\"3\", (1,))", \f16(), ("3", (1,)));
show("softint + list (\"3\", ())", \f16(), ("3", ()));
show("list + *int ((1,), 2)", \f17(), ((1,), 2));
show("list + *int ((1,), NOTHING)", \f17(), ((1,), NOTHING));
show("list + *int ((), 2)", \f17(), ((), 2));
show("list + *int ((), NOTHING)", \f17(), ((), NOTHING));
show("*int + list (2, (1,))", \f18(), (2, (1,)));
show("*int + list (2, ())", \f18(), (2, ()));
show("*int + list (NOTHING, (1,))", \f18(), (NOTHING, (1,)));
show("*int + list (NOTHING, ())", \f18(), (NOTHING, ()));
show("list + string ((1,), \"s\")", \f19(), ((1,), "s"));
show("list + string ((), \"s\")", \f19(), ((), "s"));
show("string + list (\"s\", (1,))", \f20(), ("s", (1,)));
show("string + list (\"s\", ())", \f20(), ("s", ()));
show("list + hash<D> ((1,), <D>{})", \f21(), ((1,), <D>{}));
show("list + hash<D> ((), <D>{})", \f21(), ((), <D>{}));
show("hash<D> + list (<D>{}, (1,))", \f22(), (<D>{}, (1,)));
show("hash<D> + list (<D>{}, ())", \f22(), (<D>{}, ()));
show("list + union<int, string> ((1,), 2)", \f23(), ((1,), 2));
show("list + union<int, string> ((1,), \"s\")", \f23(), ((1,), "s"));
show("list + union<int, string> ((), 2)", \f23(), ((), 2));
show("list + union<int, string> ((), \"s\")", \f23(), ((), "s"));
show("union<int, string> + list (2, (1,))", \f24(), (2, (1,)));
show("union<int, string> + list (2, ())", \f24(), (2, ()));
show("union<int, string> + list (\"s\", (1,))", \f24(), ("s", (1,)));
show("union<int, string> + list (\"s\", ())", \f24(), ("s", ()));
show("list + auto ((1,), 2)", \f25(), ((1,), 2));
show("list + auto ((1,), \"s\")", \f25(), ((1,), "s"));
show("list + auto ((1,), NOTHING)", \f25(), ((1,), NOTHING));
show("list + auto ((), 2)", \f25(), ((), 2));
show("list + auto ((), \"s\")", \f25(), ((), "s"));
show("list + auto ((), NOTHING)", \f25(), ((), NOTHING));
show("auto + list (2, (1,))", \f26(), (2, (1,)));
show("auto + list (2, ())", \f26(), (2, ()));
show("auto + list (\"s\", (1,))", \f26(), ("s", (1,)));
show("auto + list (\"s\", ())", \f26(), ("s", ()));
show("auto + list (NOTHING, (1,))", \f26(), (NOTHING, (1,)));
show("auto + list (NOTHING, ())", \f26(), (NOTHING, ()));
show("list + float ((1,), 2.5)", \f27(), ((1,), 2.5));
show("list + float ((), 2.5)", \f27(), ((), 2.5));
show("float + list (2.5, (1,))", \f28(), (2.5, (1,)));
show("float + list (2.5, ())", \f28(), (2.5, ()));
show("list + timeout ((1,), 1000)", \f29(), ((1,), 1000));
show("list + timeout ((), 1000)", \f29(), ((), 1000));
show("timeout + list (1000, (1,))", \f30(), (1000, (1,)));
show("timeout + list (1000, ())", \f30(), (1000, ()));
show("list + object ((1,), new Derived())", \f31(), ((1,), new Derived()));
show("list + object ((), new Derived())", \f31(), ((), new Derived()));
show("object + list (new Derived(), (1,))", \f32(), (new Derived(), (1,)));
show("object + list (new Derived(), ())", \f32(), (new Derived(), ()));
show("list + Base ((1,), new Derived())", \f33(), ((1,), new Derived()));
show("list + Base ((), new Derived())", \f33(), ((), new Derived()));
show("Base + list (new Derived(), (1,))", \f34(), (new Derived(), (1,)));
show("Base + list (new Derived(), ())", \f34(), (new Derived(), ()));
show("list + hash<auto> ((1,), {\"b\": 2})", \f35(), ((1,), {"b": 2}));
show("list + hash<auto> ((), {\"b\": 2})", \f35(), ((), {"b": 2}));
show("hash<auto> + list ({\"b\": 2}, (1,))", \f36(), ({"b": 2}, (1,)));
show("hash<auto> + list ({\"b\": 2}, ())", \f36(), ({"b": 2}, ()));
show("list + date ((1,), 2h)", \f37(), ((1,), 2h));
show("list + date ((1,), 2026-01-01T00:00:00Z)", \f37(), ((1,), 2026-01-01T00:00:00Z));
show("list + date ((), 2h)", \f37(), ((), 2h));
show("list + date ((), 2026-01-01T00:00:00Z)", \f37(), ((), 2026-01-01T00:00:00Z));
show("date + list (2h, (1,))", \f38(), (2h, (1,)));
show("date + list (2h, ())", \f38(), (2h, ()));
show("date + list (2026-01-01T00:00:00Z, (1,))", \f38(), (2026-01-01T00:00:00Z, (1,)));
show("date + list (2026-01-01T00:00:00Z, ())", \f38(), (2026-01-01T00:00:00Z, ()));
show("list + date<relative> ((1,), 2h)", \f39(), ((1,), 2h));
show("list + date<relative> ((), 2h)", \f39(), ((), 2h));
show("date<relative> + list (2h, (1,))", \f40(), (2h, (1,)));
show("date<relative> + list (2h, ())", \f40(), (2h, ()));
show("list + bool ((1,), True)", \f41(), ((1,), True));
show("list + bool ((), True)", \f41(), ((), True));
show("bool + list (True, (1,))", \f42(), (True, (1,)));
show("bool + list (True, ())", \f42(), (True, ()));
show("list + binary ((1,), <01>)", \f43(), ((1,), <01>));
show("list + binary ((), <01>)", \f43(), ((), <01>));
show("binary + list (<01>, (1,))", \f44(), (<01>, (1,)));
show("binary + list (<01>, ())", \f44(), (<01>, ()));
show("list<auto> + list ((1, \"a\"), (1,))", \f45(), ((1, "a"), (1,)));
show("list<auto> + list ((1, \"a\"), ())", \f45(), ((1, "a"), ()));
show("list<auto> + list ((), (1,))", \f45(), ((), (1,)));
show("list<auto> + list ((), ())", \f45(), ((), ()));
show("list<auto> + list<auto> ((1, \"a\"), (1, \"a\"))", \f46(), ((1, "a"), (1, "a")));
show("list<auto> + list<auto> ((1, \"a\"), ())", \f46(), ((1, "a"), ()));
show("list<auto> + list<auto> ((), (1, \"a\"))", \f46(), ((), (1, "a")));
show("list<auto> + list<auto> ((), ())", \f46(), ((), ()));
show("list<auto> + list<int> ((1, \"a\"), (1,))", \f47(), ((1, "a"), (1,)));
show("list<auto> + list<int> ((1, \"a\"), ())", \f47(), ((1, "a"), ()));
show("list<auto> + list<int> ((), (1,))", \f47(), ((), (1,)));
show("list<auto> + list<int> ((), ())", \f47(), ((), ()));
show("list<auto> + list<string> ((1, \"a\"), (\"a\",))", \f48(), ((1, "a"), ("a",)));
show("list<auto> + list<string> ((), (\"a\",))", \f48(), ((), ("a",)));
show("list<auto> + list<*int> ((1, \"a\"), (1,))", \f49(), ((1, "a"), (1,)));
show("list<auto> + list<*int> ((), (1,))", \f49(), ((), (1,)));
show("list<auto> + softlist<int> ((1, \"a\"), (1,))", \f50(), ((1, "a"), (1,)));
show("list<auto> + softlist<int> ((1, \"a\"), 1)", \f50(), ((1, "a"), 1));
show("list<auto> + softlist<int> ((), (1,))", \f50(), ((), (1,)));
show("list<auto> + softlist<int> ((), 1)", \f50(), ((), 1));
show("list<auto> + softlist ((1, \"a\"), (1,))", \f51(), ((1, "a"), (1,)));
show("list<auto> + softlist ((1, \"a\"), 2)", \f51(), ((1, "a"), 2));
show("list<auto> + softlist ((), (1,))", \f51(), ((), (1,)));
show("list<auto> + softlist ((), 2)", \f51(), ((), 2));
show("list<auto> + list<softint> ((1, \"a\"), (1,))", \f52(), ((1, "a"), (1,)));
show("list<auto> + list<softint> ((), (1,))", \f52(), ((), (1,)));
show("list<auto> + list<Base> ((1, \"a\"), (new Derived(),))", \f53(), ((1, "a"), (new Derived(),)));
show("list<auto> + list<Base> ((), (new Derived(),))", \f53(), ((), (new Derived(),)));
show("list<auto> + list<Derived> ((1, \"a\"), (new Derived(),))", \f54(), ((1, "a"), (new Derived(),)));
show("list<auto> + list<Derived> ((), (new Derived(),))", \f54(), ((), (new Derived(),)));
show("list<auto> + list<date> ((1, \"a\"), (1h,))", \f55(), ((1, "a"), (1h,)));
show("list<auto> + list<date> ((), (1h,))", \f55(), ((), (1h,)));
show("list<auto> + list<hash<string, int>> ((1, \"a\"), ({\"a\": 1},))", \f56(), ((1, "a"), ({"a": 1},)));
show("list<auto> + list<hash<string, int>> ((), ({\"a\": 1},))", \f56(), ((), ({"a": 1},)));
show("list<auto> + int ((1, \"a\"), 2)", \f57(), ((1, "a"), 2));
show("list<auto> + int ((), 2)", \f57(), ((), 2));
show("int + list<auto> (2, (1, \"a\"))", \f58(), (2, (1, "a")));
show("int + list<auto> (2, ())", \f58(), (2, ()));
show("list<auto> + softint ((1, \"a\"), 2)", \f59(), ((1, "a"), 2));
show("list<auto> + softint ((1, \"a\"), \"3\")", \f59(), ((1, "a"), "3"));
show("list<auto> + softint ((), 2)", \f59(), ((), 2));
show("list<auto> + softint ((), \"3\")", \f59(), ((), "3"));
show("softint + list<auto> (2, (1, \"a\"))", \f60(), (2, (1, "a")));
show("softint + list<auto> (2, ())", \f60(), (2, ()));
show("softint + list<auto> (\"3\", (1, \"a\"))", \f60(), ("3", (1, "a")));
show("softint + list<auto> (\"3\", ())", \f60(), ("3", ()));
show("list<auto> + *int ((1, \"a\"), 2)", \f61(), ((1, "a"), 2));
show("list<auto> + *int ((1, \"a\"), NOTHING)", \f61(), ((1, "a"), NOTHING));
show("list<auto> + *int ((), 2)", \f61(), ((), 2));
show("list<auto> + *int ((), NOTHING)", \f61(), ((), NOTHING));
show("*int + list<auto> (2, (1, \"a\"))", \f62(), (2, (1, "a")));
show("*int + list<auto> (2, ())", \f62(), (2, ()));
show("*int + list<auto> (NOTHING, (1, \"a\"))", \f62(), (NOTHING, (1, "a")));
show("*int + list<auto> (NOTHING, ())", \f62(), (NOTHING, ()));
show("list<auto> + string ((1, \"a\"), \"s\")", \f63(), ((1, "a"), "s"));
show("list<auto> + string ((), \"s\")", \f63(), ((), "s"));
show("string + list<auto> (\"s\", (1, \"a\"))", \f64(), ("s", (1, "a")));
show("string + list<auto> (\"s\", ())", \f64(), ("s", ()));
show("list<auto> + hash<D> ((1, \"a\"), <D>{})", \f65(), ((1, "a"), <D>{}));
show("list<auto> + hash<D> ((), <D>{})", \f65(), ((), <D>{}));
show("hash<D> + list<auto> (<D>{}, (1, \"a\"))", \f66(), (<D>{}, (1, "a")));
show("hash<D> + list<auto> (<D>{}, ())", \f66(), (<D>{}, ()));
show("list<auto> + union<int, string> ((1, \"a\"), 2)", \f67(), ((1, "a"), 2));
show("list<auto> + union<int, string> ((1, \"a\"), \"s\")", \f67(), ((1, "a"), "s"));
show("list<auto> + union<int, string> ((), 2)", \f67(), ((), 2));
show("list<auto> + union<int, string> ((), \"s\")", \f67(), ((), "s"));
show("union<int, string> + list<auto> (2, (1, \"a\"))", \f68(), (2, (1, "a")));
show("union<int, string> + list<auto> (2, ())", \f68(), (2, ()));
show("union<int, string> + list<auto> (\"s\", (1, \"a\"))", \f68(), ("s", (1, "a")));
show("union<int, string> + list<auto> (\"s\", ())", \f68(), ("s", ()));
show("list<auto> + auto ((1, \"a\"), 2)", \f69(), ((1, "a"), 2));
show("list<auto> + auto ((1, \"a\"), \"s\")", \f69(), ((1, "a"), "s"));
show("list<auto> + auto ((1, \"a\"), NOTHING)", \f69(), ((1, "a"), NOTHING));
show("list<auto> + auto ((), 2)", \f69(), ((), 2));
show("list<auto> + auto ((), \"s\")", \f69(), ((), "s"));
show("list<auto> + auto ((), NOTHING)", \f69(), ((), NOTHING));
show("auto + list<auto> (2, (1, \"a\"))", \f70(), (2, (1, "a")));
show("auto + list<auto> (2, ())", \f70(), (2, ()));
show("auto + list<auto> (\"s\", (1, \"a\"))", \f70(), ("s", (1, "a")));
show("auto + list<auto> (\"s\", ())", \f70(), ("s", ()));
show("auto + list<auto> (NOTHING, (1, \"a\"))", \f70(), (NOTHING, (1, "a")));
show("auto + list<auto> (NOTHING, ())", \f70(), (NOTHING, ()));
show("list<auto> + float ((1, \"a\"), 2.5)", \f71(), ((1, "a"), 2.5));
show("list<auto> + float ((), 2.5)", \f71(), ((), 2.5));
show("float + list<auto> (2.5, (1, \"a\"))", \f72(), (2.5, (1, "a")));
show("float + list<auto> (2.5, ())", \f72(), (2.5, ()));
show("list<auto> + timeout ((1, \"a\"), 1000)", \f73(), ((1, "a"), 1000));
show("list<auto> + timeout ((), 1000)", \f73(), ((), 1000));
show("timeout + list<auto> (1000, (1, \"a\"))", \f74(), (1000, (1, "a")));
show("timeout + list<auto> (1000, ())", \f74(), (1000, ()));
show("list<auto> + object ((1, \"a\"), new Derived())", \f75(), ((1, "a"), new Derived()));
show("list<auto> + object ((), new Derived())", \f75(), ((), new Derived()));
show("object + list<auto> (new Derived(), (1, \"a\"))", \f76(), (new Derived(), (1, "a")));
show("object + list<auto> (new Derived(), ())", \f76(), (new Derived(), ()));
show("list<auto> + Base ((1, \"a\"), new Derived())", \f77(), ((1, "a"), new Derived()));
show("list<auto> + Base ((), new Derived())", \f77(), ((), new Derived()));
show("Base + list<auto> (new Derived(), (1, \"a\"))", \f78(), (new Derived(), (1, "a")));
show("Base + list<auto> (new Derived(), ())", \f78(), (new Derived(), ()));
show("list<auto> + hash<auto> ((1, \"a\"), {\"b\": 2})", \f79(), ((1, "a"), {"b": 2}));
show("list<auto> + hash<auto> ((), {\"b\": 2})", \f79(), ((), {"b": 2}));
show("hash<auto> + list<auto> ({\"b\": 2}, (1, \"a\"))", \f80(), ({"b": 2}, (1, "a")));
show("hash<auto> + list<auto> ({\"b\": 2}, ())", \f80(), ({"b": 2}, ()));
show("list<auto> + date ((1, \"a\"), 2h)", \f81(), ((1, "a"), 2h));
show("list<auto> + date ((1, \"a\"), 2026-01-01T00:00:00Z)", \f81(), ((1, "a"), 2026-01-01T00:00:00Z));
show("list<auto> + date ((), 2h)", \f81(), ((), 2h));
show("list<auto> + date ((), 2026-01-01T00:00:00Z)", \f81(), ((), 2026-01-01T00:00:00Z));
show("date + list<auto> (2h, (1, \"a\"))", \f82(), (2h, (1, "a")));
show("date + list<auto> (2h, ())", \f82(), (2h, ()));
show("date + list<auto> (2026-01-01T00:00:00Z, (1, \"a\"))", \f82(), (2026-01-01T00:00:00Z, (1, "a")));
show("date + list<auto> (2026-01-01T00:00:00Z, ())", \f82(), (2026-01-01T00:00:00Z, ()));
show("list<auto> + date<relative> ((1, \"a\"), 2h)", \f83(), ((1, "a"), 2h));
show("list<auto> + date<relative> ((), 2h)", \f83(), ((), 2h));
show("date<relative> + list<auto> (2h, (1, \"a\"))", \f84(), (2h, (1, "a")));
show("date<relative> + list<auto> (2h, ())", \f84(), (2h, ()));
show("list<auto> + bool ((1, \"a\"), True)", \f85(), ((1, "a"), True));
show("list<auto> + bool ((), True)", \f85(), ((), True));
show("bool + list<auto> (True, (1, \"a\"))", \f86(), (True, (1, "a")));
show("bool + list<auto> (True, ())", \f86(), (True, ()));
show("list<auto> + binary ((1, \"a\"), <01>)", \f87(), ((1, "a"), <01>));
show("list<auto> + binary ((), <01>)", \f87(), ((), <01>));
show("binary + list<auto> (<01>, (1, \"a\"))", \f88(), (<01>, (1, "a")));
show("binary + list<auto> (<01>, ())", \f88(), (<01>, ()));
show("list<int> + list ((1,), (1,))", \f89(), ((1,), (1,)));
show("list<int> + list ((1,), ())", \f89(), ((1,), ()));
show("list<int> + list ((), (1,))", \f89(), ((), (1,)));
show("list<int> + list ((), ())", \f89(), ((), ()));
show("list<int> + list<auto> ((1,), (1, \"a\"))", \f90(), ((1,), (1, "a")));
show("list<int> + list<auto> ((1,), ())", \f90(), ((1,), ()));
show("list<int> + list<auto> ((), (1, \"a\"))", \f90(), ((), (1, "a")));
show("list<int> + list<auto> ((), ())", \f90(), ((), ()));
show("list<int> + list<int> ((1,), (1,))", \f91(), ((1,), (1,)));
show("list<int> + list<int> ((1,), ())", \f91(), ((1,), ()));
show("list<int> + list<int> ((), (1,))", \f91(), ((), (1,)));
show("list<int> + list<int> ((), ())", \f91(), ((), ()));
show("list<int> + list<string> ((1,), (\"a\",))", \f92(), ((1,), ("a",)));
show("list<int> + list<string> ((), (\"a\",))", \f92(), ((), ("a",)));
show("list<int> + list<*int> ((1,), (1,))", \f93(), ((1,), (1,)));
show("list<int> + list<*int> ((), (1,))", \f93(), ((), (1,)));
show("list<int> + softlist<int> ((1,), (1,))", \f94(), ((1,), (1,)));
show("list<int> + softlist<int> ((1,), 1)", \f94(), ((1,), 1));
show("list<int> + softlist<int> ((), (1,))", \f94(), ((), (1,)));
show("list<int> + softlist<int> ((), 1)", \f94(), ((), 1));
show("list<int> + softlist ((1,), (1,))", \f95(), ((1,), (1,)));
show("list<int> + softlist ((1,), 2)", \f95(), ((1,), 2));
show("list<int> + softlist ((), (1,))", \f95(), ((), (1,)));
show("list<int> + softlist ((), 2)", \f95(), ((), 2));
show("list<int> + list<softint> ((1,), (1,))", \f96(), ((1,), (1,)));
show("list<int> + list<softint> ((), (1,))", \f96(), ((), (1,)));
show("list<int> + list<Base> ((1,), (new Derived(),))", \f97(), ((1,), (new Derived(),)));
show("list<int> + list<Base> ((), (new Derived(),))", \f97(), ((), (new Derived(),)));
show("list<int> + list<Derived> ((1,), (new Derived(),))", \f98(), ((1,), (new Derived(),)));
show("list<int> + list<Derived> ((), (new Derived(),))", \f98(), ((), (new Derived(),)));
show("list<int> + list<date> ((1,), (1h,))", \f99(), ((1,), (1h,)));
show("list<int> + list<date> ((), (1h,))", \f99(), ((), (1h,)));
show("list<int> + list<hash<string, int>> ((1,), ({\"a\": 1},))", \f100(), ((1,), ({"a": 1},)));
show("list<int> + list<hash<string, int>> ((), ({\"a\": 1},))", \f100(), ((), ({"a": 1},)));
show("list<int> + int ((1,), 2)", \f101(), ((1,), 2));
show("list<int> + int ((), 2)", \f101(), ((), 2));
show("int + list<int> (2, (1,))", \f102(), (2, (1,)));
show("int + list<int> (2, ())", \f102(), (2, ()));
show("list<int> + softint ((1,), 2)", \f103(), ((1,), 2));
show("list<int> + softint ((1,), \"3\")", \f103(), ((1,), "3"));
show("list<int> + softint ((), 2)", \f103(), ((), 2));
show("list<int> + softint ((), \"3\")", \f103(), ((), "3"));
show("softint + list<int> (2, (1,))", \f104(), (2, (1,)));
show("softint + list<int> (2, ())", \f104(), (2, ()));
show("softint + list<int> (\"3\", (1,))", \f104(), ("3", (1,)));
show("softint + list<int> (\"3\", ())", \f104(), ("3", ()));
show("list<int> + *int ((1,), 2)", \f105(), ((1,), 2));
show("list<int> + *int ((1,), NOTHING)", \f105(), ((1,), NOTHING));
show("list<int> + *int ((), 2)", \f105(), ((), 2));
show("list<int> + *int ((), NOTHING)", \f105(), ((), NOTHING));
show("*int + list<int> (2, (1,))", \f106(), (2, (1,)));
show("*int + list<int> (2, ())", \f106(), (2, ()));
show("*int + list<int> (NOTHING, (1,))", \f106(), (NOTHING, (1,)));
show("*int + list<int> (NOTHING, ())", \f106(), (NOTHING, ()));
show("list<int> + string ((1,), \"s\")", \f107(), ((1,), "s"));
show("list<int> + string ((), \"s\")", \f107(), ((), "s"));
show("string + list<int> (\"s\", (1,))", \f108(), ("s", (1,)));
show("string + list<int> (\"s\", ())", \f108(), ("s", ()));
show("list<int> + hash<D> ((1,), <D>{})", \f109(), ((1,), <D>{}));
show("list<int> + hash<D> ((), <D>{})", \f109(), ((), <D>{}));
show("hash<D> + list<int> (<D>{}, (1,))", \f110(), (<D>{}, (1,)));
show("hash<D> + list<int> (<D>{}, ())", \f110(), (<D>{}, ()));
show("list<int> + union<int, string> ((1,), 2)", \f111(), ((1,), 2));
show("list<int> + union<int, string> ((1,), \"s\")", \f111(), ((1,), "s"));
show("list<int> + union<int, string> ((), 2)", \f111(), ((), 2));
show("list<int> + union<int, string> ((), \"s\")", \f111(), ((), "s"));
show("union<int, string> + list<int> (2, (1,))", \f112(), (2, (1,)));
show("union<int, string> + list<int> (2, ())", \f112(), (2, ()));
show("union<int, string> + list<int> (\"s\", (1,))", \f112(), ("s", (1,)));
show("union<int, string> + list<int> (\"s\", ())", \f112(), ("s", ()));
show("list<int> + auto ((1,), 2)", \f113(), ((1,), 2));
show("list<int> + auto ((1,), \"s\")", \f113(), ((1,), "s"));
show("list<int> + auto ((1,), NOTHING)", \f113(), ((1,), NOTHING));
show("list<int> + auto ((), 2)", \f113(), ((), 2));
show("list<int> + auto ((), \"s\")", \f113(), ((), "s"));
show("list<int> + auto ((), NOTHING)", \f113(), ((), NOTHING));
show("auto + list<int> (2, (1,))", \f114(), (2, (1,)));
show("auto + list<int> (2, ())", \f114(), (2, ()));
show("auto + list<int> (\"s\", (1,))", \f114(), ("s", (1,)));
show("auto + list<int> (\"s\", ())", \f114(), ("s", ()));
show("auto + list<int> (NOTHING, (1,))", \f114(), (NOTHING, (1,)));
show("auto + list<int> (NOTHING, ())", \f114(), (NOTHING, ()));
show("list<int> + float ((1,), 2.5)", \f115(), ((1,), 2.5));
show("list<int> + float ((), 2.5)", \f115(), ((), 2.5));
show("float + list<int> (2.5, (1,))", \f116(), (2.5, (1,)));
show("float + list<int> (2.5, ())", \f116(), (2.5, ()));
show("list<int> + timeout ((1,), 1000)", \f117(), ((1,), 1000));
show("list<int> + timeout ((), 1000)", \f117(), ((), 1000));
show("timeout + list<int> (1000, (1,))", \f118(), (1000, (1,)));
show("timeout + list<int> (1000, ())", \f118(), (1000, ()));
show("list<int> + object ((1,), new Derived())", \f119(), ((1,), new Derived()));
show("list<int> + object ((), new Derived())", \f119(), ((), new Derived()));
show("object + list<int> (new Derived(), (1,))", \f120(), (new Derived(), (1,)));
show("object + list<int> (new Derived(), ())", \f120(), (new Derived(), ()));
show("list<int> + Base ((1,), new Derived())", \f121(), ((1,), new Derived()));
show("list<int> + Base ((), new Derived())", \f121(), ((), new Derived()));
show("Base + list<int> (new Derived(), (1,))", \f122(), (new Derived(), (1,)));
show("Base + list<int> (new Derived(), ())", \f122(), (new Derived(), ()));
show("list<int> + hash<auto> ((1,), {\"b\": 2})", \f123(), ((1,), {"b": 2}));
show("list<int> + hash<auto> ((), {\"b\": 2})", \f123(), ((), {"b": 2}));
show("hash<auto> + list<int> ({\"b\": 2}, (1,))", \f124(), ({"b": 2}, (1,)));
show("hash<auto> + list<int> ({\"b\": 2}, ())", \f124(), ({"b": 2}, ()));
show("list<int> + date ((1,), 2h)", \f125(), ((1,), 2h));
show("list<int> + date ((1,), 2026-01-01T00:00:00Z)", \f125(), ((1,), 2026-01-01T00:00:00Z));
show("list<int> + date ((), 2h)", \f125(), ((), 2h));
show("list<int> + date ((), 2026-01-01T00:00:00Z)", \f125(), ((), 2026-01-01T00:00:00Z));
show("date + list<int> (2h, (1,))", \f126(), (2h, (1,)));
show("date + list<int> (2h, ())", \f126(), (2h, ()));
show("date + list<int> (2026-01-01T00:00:00Z, (1,))", \f126(), (2026-01-01T00:00:00Z, (1,)));
show("date + list<int> (2026-01-01T00:00:00Z, ())", \f126(), (2026-01-01T00:00:00Z, ()));
show("list<int> + date<relative> ((1,), 2h)", \f127(), ((1,), 2h));
show("list<int> + date<relative> ((), 2h)", \f127(), ((), 2h));
show("date<relative> + list<int> (2h, (1,))", \f128(), (2h, (1,)));
show("date<relative> + list<int> (2h, ())", \f128(), (2h, ()));
show("list<int> + bool ((1,), True)", \f129(), ((1,), True));
show("list<int> + bool ((), True)", \f129(), ((), True));
show("bool + list<int> (True, (1,))", \f130(), (True, (1,)));
show("bool + list<int> (True, ())", \f130(), (True, ()));
show("list<int> + binary ((1,), <01>)", \f131(), ((1,), <01>));
show("list<int> + binary ((), <01>)", \f131(), ((), <01>));
show("binary + list<int> (<01>, (1,))", \f132(), (<01>, (1,)));
show("binary + list<int> (<01>, ())", \f132(), (<01>, ()));
show("list<string> + list ((\"a\",), (1,))", \f133(), (("a",), (1,)));
show("list<string> + list ((\"a\",), ())", \f133(), (("a",), ()));
show("list<string> + list<auto> ((\"a\",), (1, \"a\"))", \f134(), (("a",), (1, "a")));
show("list<string> + list<auto> ((\"a\",), ())", \f134(), (("a",), ()));
show("list<string> + list<int> ((\"a\",), (1,))", \f135(), (("a",), (1,)));
show("list<string> + list<int> ((\"a\",), ())", \f135(), (("a",), ()));
show("list<string> + list<string> ((\"a\",), (\"a\",))", \f136(), (("a",), ("a",)));
show("list<string> + list<*int> ((\"a\",), (1,))", \f137(), (("a",), (1,)));
show("list<string> + softlist<int> ((\"a\",), (1,))", \f138(), (("a",), (1,)));
show("list<string> + softlist<int> ((\"a\",), 1)", \f138(), (("a",), 1));
show("list<string> + softlist ((\"a\",), (1,))", \f139(), (("a",), (1,)));
show("list<string> + softlist ((\"a\",), 2)", \f139(), (("a",), 2));
show("list<string> + list<softint> ((\"a\",), (1,))", \f140(), (("a",), (1,)));
show("list<string> + list<Base> ((\"a\",), (new Derived(),))", \f141(), (("a",), (new Derived(),)));
show("list<string> + list<Derived> ((\"a\",), (new Derived(),))", \f142(), (("a",), (new Derived(),)));
show("list<string> + list<date> ((\"a\",), (1h,))", \f143(), (("a",), (1h,)));
show("list<string> + list<hash<string, int>> ((\"a\",), ({\"a\": 1},))", \f144(), (("a",), ({"a": 1},)));
show("list<string> + int ((\"a\",), 2)", \f145(), (("a",), 2));
show("int + list<string> (2, (\"a\",))", \f146(), (2, ("a",)));
show("list<string> + softint ((\"a\",), 2)", \f147(), (("a",), 2));
show("list<string> + softint ((\"a\",), \"3\")", \f147(), (("a",), "3"));
show("softint + list<string> (2, (\"a\",))", \f148(), (2, ("a",)));
show("softint + list<string> (\"3\", (\"a\",))", \f148(), ("3", ("a",)));
show("list<string> + *int ((\"a\",), 2)", \f149(), (("a",), 2));
show("list<string> + *int ((\"a\",), NOTHING)", \f149(), (("a",), NOTHING));
show("*int + list<string> (2, (\"a\",))", \f150(), (2, ("a",)));
show("*int + list<string> (NOTHING, (\"a\",))", \f150(), (NOTHING, ("a",)));
show("list<string> + string ((\"a\",), \"s\")", \f151(), (("a",), "s"));
show("string + list<string> (\"s\", (\"a\",))", \f152(), ("s", ("a",)));
show("list<string> + hash<D> ((\"a\",), <D>{})", \f153(), (("a",), <D>{}));
show("hash<D> + list<string> (<D>{}, (\"a\",))", \f154(), (<D>{}, ("a",)));
show("list<string> + union<int, string> ((\"a\",), 2)", \f155(), (("a",), 2));
show("list<string> + union<int, string> ((\"a\",), \"s\")", \f155(), (("a",), "s"));
show("union<int, string> + list<string> (2, (\"a\",))", \f156(), (2, ("a",)));
show("union<int, string> + list<string> (\"s\", (\"a\",))", \f156(), ("s", ("a",)));
show("list<string> + auto ((\"a\",), 2)", \f157(), (("a",), 2));
show("list<string> + auto ((\"a\",), \"s\")", \f157(), (("a",), "s"));
show("list<string> + auto ((\"a\",), NOTHING)", \f157(), (("a",), NOTHING));
show("auto + list<string> (2, (\"a\",))", \f158(), (2, ("a",)));
show("auto + list<string> (\"s\", (\"a\",))", \f158(), ("s", ("a",)));
show("auto + list<string> (NOTHING, (\"a\",))", \f158(), (NOTHING, ("a",)));
show("list<string> + float ((\"a\",), 2.5)", \f159(), (("a",), 2.5));
show("float + list<string> (2.5, (\"a\",))", \f160(), (2.5, ("a",)));
show("list<string> + timeout ((\"a\",), 1000)", \f161(), (("a",), 1000));
show("timeout + list<string> (1000, (\"a\",))", \f162(), (1000, ("a",)));
show("list<string> + object ((\"a\",), new Derived())", \f163(), (("a",), new Derived()));
show("object + list<string> (new Derived(), (\"a\",))", \f164(), (new Derived(), ("a",)));
show("list<string> + Base ((\"a\",), new Derived())", \f165(), (("a",), new Derived()));
show("Base + list<string> (new Derived(), (\"a\",))", \f166(), (new Derived(), ("a",)));
show("list<string> + hash<auto> ((\"a\",), {\"b\": 2})", \f167(), (("a",), {"b": 2}));
show("hash<auto> + list<string> ({\"b\": 2}, (\"a\",))", \f168(), ({"b": 2}, ("a",)));
show("list<string> + date ((\"a\",), 2h)", \f169(), (("a",), 2h));
show("list<string> + date ((\"a\",), 2026-01-01T00:00:00Z)", \f169(), (("a",), 2026-01-01T00:00:00Z));
show("date + list<string> (2h, (\"a\",))", \f170(), (2h, ("a",)));
show("date + list<string> (2026-01-01T00:00:00Z, (\"a\",))", \f170(), (2026-01-01T00:00:00Z, ("a",)));
show("list<string> + date<relative> ((\"a\",), 2h)", \f171(), (("a",), 2h));
show("date<relative> + list<string> (2h, (\"a\",))", \f172(), (2h, ("a",)));
show("list<string> + bool ((\"a\",), True)", \f173(), (("a",), True));
show("bool + list<string> (True, (\"a\",))", \f174(), (True, ("a",)));
show("list<string> + binary ((\"a\",), <01>)", \f175(), (("a",), <01>));
show("binary + list<string> (<01>, (\"a\",))", \f176(), (<01>, ("a",)));
show("list<*int> + list ((1,), (1,))", \f177(), ((1,), (1,)));
show("list<*int> + list ((1,), ())", \f177(), ((1,), ()));
show("list<*int> + list<auto> ((1,), (1, \"a\"))", \f178(), ((1,), (1, "a")));
show("list<*int> + list<auto> ((1,), ())", \f178(), ((1,), ()));
show("list<*int> + list<int> ((1,), (1,))", \f179(), ((1,), (1,)));
show("list<*int> + list<int> ((1,), ())", \f179(), ((1,), ()));
show("list<*int> + list<string> ((1,), (\"a\",))", \f180(), ((1,), ("a",)));
show("list<*int> + list<*int> ((1,), (1,))", \f181(), ((1,), (1,)));
show("list<*int> + softlist<int> ((1,), (1,))", \f182(), ((1,), (1,)));
show("list<*int> + softlist<int> ((1,), 1)", \f182(), ((1,), 1));
show("list<*int> + softlist ((1,), (1,))", \f183(), ((1,), (1,)));
show("list<*int> + softlist ((1,), 2)", \f183(), ((1,), 2));
show("list<*int> + list<softint> ((1,), (1,))", \f184(), ((1,), (1,)));
show("list<*int> + list<Base> ((1,), (new Derived(),))", \f185(), ((1,), (new Derived(),)));
show("list<*int> + list<Derived> ((1,), (new Derived(),))", \f186(), ((1,), (new Derived(),)));
show("list<*int> + list<date> ((1,), (1h,))", \f187(), ((1,), (1h,)));
show("list<*int> + list<hash<string, int>> ((1,), ({\"a\": 1},))", \f188(), ((1,), ({"a": 1},)));
show("list<*int> + int ((1,), 2)", \f189(), ((1,), 2));
show("int + list<*int> (2, (1,))", \f190(), (2, (1,)));
show("list<*int> + softint ((1,), 2)", \f191(), ((1,), 2));
show("list<*int> + softint ((1,), \"3\")", \f191(), ((1,), "3"));
show("softint + list<*int> (2, (1,))", \f192(), (2, (1,)));
show("softint + list<*int> (\"3\", (1,))", \f192(), ("3", (1,)));
show("list<*int> + *int ((1,), 2)", \f193(), ((1,), 2));
show("list<*int> + *int ((1,), NOTHING)", \f193(), ((1,), NOTHING));
show("*int + list<*int> (2, (1,))", \f194(), (2, (1,)));
show("*int + list<*int> (NOTHING, (1,))", \f194(), (NOTHING, (1,)));
show("list<*int> + string ((1,), \"s\")", \f195(), ((1,), "s"));
show("string + list<*int> (\"s\", (1,))", \f196(), ("s", (1,)));
show("list<*int> + hash<D> ((1,), <D>{})", \f197(), ((1,), <D>{}));
show("hash<D> + list<*int> (<D>{}, (1,))", \f198(), (<D>{}, (1,)));
show("list<*int> + union<int, string> ((1,), 2)", \f199(), ((1,), 2));
show("list<*int> + union<int, string> ((1,), \"s\")", \f199(), ((1,), "s"));
show("union<int, string> + list<*int> (2, (1,))", \f200(), (2, (1,)));
show("union<int, string> + list<*int> (\"s\", (1,))", \f200(), ("s", (1,)));
show("list<*int> + auto ((1,), 2)", \f201(), ((1,), 2));
show("list<*int> + auto ((1,), \"s\")", \f201(), ((1,), "s"));
show("list<*int> + auto ((1,), NOTHING)", \f201(), ((1,), NOTHING));
show("auto + list<*int> (2, (1,))", \f202(), (2, (1,)));
show("auto + list<*int> (\"s\", (1,))", \f202(), ("s", (1,)));
show("auto + list<*int> (NOTHING, (1,))", \f202(), (NOTHING, (1,)));
show("list<*int> + float ((1,), 2.5)", \f203(), ((1,), 2.5));
show("float + list<*int> (2.5, (1,))", \f204(), (2.5, (1,)));
show("list<*int> + timeout ((1,), 1000)", \f205(), ((1,), 1000));
show("timeout + list<*int> (1000, (1,))", \f206(), (1000, (1,)));
show("list<*int> + object ((1,), new Derived())", \f207(), ((1,), new Derived()));
show("object + list<*int> (new Derived(), (1,))", \f208(), (new Derived(), (1,)));
show("list<*int> + Base ((1,), new Derived())", \f209(), ((1,), new Derived()));
show("Base + list<*int> (new Derived(), (1,))", \f210(), (new Derived(), (1,)));
show("list<*int> + hash<auto> ((1,), {\"b\": 2})", \f211(), ((1,), {"b": 2}));
show("hash<auto> + list<*int> ({\"b\": 2}, (1,))", \f212(), ({"b": 2}, (1,)));
show("list<*int> + date ((1,), 2h)", \f213(), ((1,), 2h));
show("list<*int> + date ((1,), 2026-01-01T00:00:00Z)", \f213(), ((1,), 2026-01-01T00:00:00Z));
show("date + list<*int> (2h, (1,))", \f214(), (2h, (1,)));
show("date + list<*int> (2026-01-01T00:00:00Z, (1,))", \f214(), (2026-01-01T00:00:00Z, (1,)));
show("list<*int> + date<relative> ((1,), 2h)", \f215(), ((1,), 2h));
show("date<relative> + list<*int> (2h, (1,))", \f216(), (2h, (1,)));
show("list<*int> + bool ((1,), True)", \f217(), ((1,), True));
show("bool + list<*int> (True, (1,))", \f218(), (True, (1,)));
show("list<*int> + binary ((1,), <01>)", \f219(), ((1,), <01>));
show("binary + list<*int> (<01>, (1,))", \f220(), (<01>, (1,)));
show("softlist<int> + list ((1,), (1,))", \f221(), ((1,), (1,)));
show("softlist<int> + list ((1,), ())", \f221(), ((1,), ()));
show("softlist<int> + list (1, (1,))", \f221(), (1, (1,)));
show("softlist<int> + list (1, ())", \f221(), (1, ()));
show("softlist<int> + list<auto> ((1,), (1, \"a\"))", \f222(), ((1,), (1, "a")));
show("softlist<int> + list<auto> ((1,), ())", \f222(), ((1,), ()));
show("softlist<int> + list<auto> (1, (1, \"a\"))", \f222(), (1, (1, "a")));
show("softlist<int> + list<auto> (1, ())", \f222(), (1, ()));
show("softlist<int> + list<int> ((1,), (1,))", \f223(), ((1,), (1,)));
show("softlist<int> + list<int> ((1,), ())", \f223(), ((1,), ()));
show("softlist<int> + list<int> (1, (1,))", \f223(), (1, (1,)));
show("softlist<int> + list<int> (1, ())", \f223(), (1, ()));
show("softlist<int> + list<string> ((1,), (\"a\",))", \f224(), ((1,), ("a",)));
show("softlist<int> + list<string> (1, (\"a\",))", \f224(), (1, ("a",)));
show("softlist<int> + list<*int> ((1,), (1,))", \f225(), ((1,), (1,)));
show("softlist<int> + list<*int> (1, (1,))", \f225(), (1, (1,)));
show("softlist<int> + softlist<int> ((1,), (1,))", \f226(), ((1,), (1,)));
show("softlist<int> + softlist<int> ((1,), 1)", \f226(), ((1,), 1));
show("softlist<int> + softlist<int> (1, (1,))", \f226(), (1, (1,)));
show("softlist<int> + softlist<int> (1, 1)", \f226(), (1, 1));
show("softlist<int> + softlist ((1,), (1,))", \f227(), ((1,), (1,)));
show("softlist<int> + softlist ((1,), 2)", \f227(), ((1,), 2));
show("softlist<int> + softlist (1, (1,))", \f227(), (1, (1,)));
show("softlist<int> + softlist (1, 2)", \f227(), (1, 2));
show("softlist<int> + list<softint> ((1,), (1,))", \f228(), ((1,), (1,)));
show("softlist<int> + list<softint> (1, (1,))", \f228(), (1, (1,)));
show("softlist<int> + list<Base> ((1,), (new Derived(),))", \f229(), ((1,), (new Derived(),)));
show("softlist<int> + list<Base> (1, (new Derived(),))", \f229(), (1, (new Derived(),)));
show("softlist<int> + list<Derived> ((1,), (new Derived(),))", \f230(), ((1,), (new Derived(),)));
show("softlist<int> + list<Derived> (1, (new Derived(),))", \f230(), (1, (new Derived(),)));
show("softlist<int> + list<date> ((1,), (1h,))", \f231(), ((1,), (1h,)));
show("softlist<int> + list<date> (1, (1h,))", \f231(), (1, (1h,)));
show("softlist<int> + list<hash<string, int>> ((1,), ({\"a\": 1},))", \f232(), ((1,), ({"a": 1},)));
show("softlist<int> + list<hash<string, int>> (1, ({\"a\": 1},))", \f232(), (1, ({"a": 1},)));
show("softlist<int> + int ((1,), 2)", \f233(), ((1,), 2));
show("softlist<int> + int (1, 2)", \f233(), (1, 2));
show("int + softlist<int> (2, (1,))", \f234(), (2, (1,)));
show("int + softlist<int> (2, 1)", \f234(), (2, 1));
show("softlist<int> + softint ((1,), 2)", \f235(), ((1,), 2));
show("softlist<int> + softint ((1,), \"3\")", \f235(), ((1,), "3"));
show("softlist<int> + softint (1, 2)", \f235(), (1, 2));
show("softlist<int> + softint (1, \"3\")", \f235(), (1, "3"));
show("softint + softlist<int> (2, (1,))", \f236(), (2, (1,)));
show("softint + softlist<int> (2, 1)", \f236(), (2, 1));
show("softint + softlist<int> (\"3\", (1,))", \f236(), ("3", (1,)));
show("softint + softlist<int> (\"3\", 1)", \f236(), ("3", 1));
show("softlist<int> + *int ((1,), 2)", \f237(), ((1,), 2));
show("softlist<int> + *int ((1,), NOTHING)", \f237(), ((1,), NOTHING));
show("softlist<int> + *int (1, 2)", \f237(), (1, 2));
show("softlist<int> + *int (1, NOTHING)", \f237(), (1, NOTHING));
show("*int + softlist<int> (2, (1,))", \f238(), (2, (1,)));
show("*int + softlist<int> (2, 1)", \f238(), (2, 1));
show("*int + softlist<int> (NOTHING, (1,))", \f238(), (NOTHING, (1,)));
show("*int + softlist<int> (NOTHING, 1)", \f238(), (NOTHING, 1));
show("softlist<int> + string ((1,), \"s\")", \f239(), ((1,), "s"));
show("softlist<int> + string (1, \"s\")", \f239(), (1, "s"));
show("string + softlist<int> (\"s\", (1,))", \f240(), ("s", (1,)));
show("string + softlist<int> (\"s\", 1)", \f240(), ("s", 1));
show("softlist<int> + hash<D> ((1,), <D>{})", \f241(), ((1,), <D>{}));
show("softlist<int> + hash<D> (1, <D>{})", \f241(), (1, <D>{}));
show("hash<D> + softlist<int> (<D>{}, (1,))", \f242(), (<D>{}, (1,)));
show("hash<D> + softlist<int> (<D>{}, 1)", \f242(), (<D>{}, 1));
show("softlist<int> + union<int, string> ((1,), 2)", \f243(), ((1,), 2));
show("softlist<int> + union<int, string> ((1,), \"s\")", \f243(), ((1,), "s"));
show("softlist<int> + union<int, string> (1, 2)", \f243(), (1, 2));
show("softlist<int> + union<int, string> (1, \"s\")", \f243(), (1, "s"));
show("union<int, string> + softlist<int> (2, (1,))", \f244(), (2, (1,)));
show("union<int, string> + softlist<int> (2, 1)", \f244(), (2, 1));
show("union<int, string> + softlist<int> (\"s\", (1,))", \f244(), ("s", (1,)));
show("union<int, string> + softlist<int> (\"s\", 1)", \f244(), ("s", 1));
show("softlist<int> + auto ((1,), 2)", \f245(), ((1,), 2));
show("softlist<int> + auto ((1,), \"s\")", \f245(), ((1,), "s"));
show("softlist<int> + auto ((1,), NOTHING)", \f245(), ((1,), NOTHING));
show("softlist<int> + auto (1, 2)", \f245(), (1, 2));
show("softlist<int> + auto (1, \"s\")", \f245(), (1, "s"));
show("softlist<int> + auto (1, NOTHING)", \f245(), (1, NOTHING));
show("auto + softlist<int> (2, (1,))", \f246(), (2, (1,)));
show("auto + softlist<int> (2, 1)", \f246(), (2, 1));
show("auto + softlist<int> (\"s\", (1,))", \f246(), ("s", (1,)));
show("auto + softlist<int> (\"s\", 1)", \f246(), ("s", 1));
show("auto + softlist<int> (NOTHING, (1,))", \f246(), (NOTHING, (1,)));
show("auto + softlist<int> (NOTHING, 1)", \f246(), (NOTHING, 1));
show("softlist<int> + float ((1,), 2.5)", \f247(), ((1,), 2.5));
show("softlist<int> + float (1, 2.5)", \f247(), (1, 2.5));
show("float + softlist<int> (2.5, (1,))", \f248(), (2.5, (1,)));
show("float + softlist<int> (2.5, 1)", \f248(), (2.5, 1));
show("softlist<int> + timeout ((1,), 1000)", \f249(), ((1,), 1000));
show("softlist<int> + timeout (1, 1000)", \f249(), (1, 1000));
show("timeout + softlist<int> (1000, (1,))", \f250(), (1000, (1,)));
show("timeout + softlist<int> (1000, 1)", \f250(), (1000, 1));
show("softlist<int> + object ((1,), new Derived())", \f251(), ((1,), new Derived()));
show("softlist<int> + object (1, new Derived())", \f251(), (1, new Derived()));
show("object + softlist<int> (new Derived(), (1,))", \f252(), (new Derived(), (1,)));
show("object + softlist<int> (new Derived(), 1)", \f252(), (new Derived(), 1));
show("softlist<int> + Base ((1,), new Derived())", \f253(), ((1,), new Derived()));
show("softlist<int> + Base (1, new Derived())", \f253(), (1, new Derived()));
show("Base + softlist<int> (new Derived(), (1,))", \f254(), (new Derived(), (1,)));
show("Base + softlist<int> (new Derived(), 1)", \f254(), (new Derived(), 1));
show("softlist<int> + hash<auto> ((1,), {\"b\": 2})", \f255(), ((1,), {"b": 2}));
show("softlist<int> + hash<auto> (1, {\"b\": 2})", \f255(), (1, {"b": 2}));
show("hash<auto> + softlist<int> ({\"b\": 2}, (1,))", \f256(), ({"b": 2}, (1,)));
show("hash<auto> + softlist<int> ({\"b\": 2}, 1)", \f256(), ({"b": 2}, 1));
show("softlist<int> + date ((1,), 2h)", \f257(), ((1,), 2h));
show("softlist<int> + date ((1,), 2026-01-01T00:00:00Z)", \f257(), ((1,), 2026-01-01T00:00:00Z));
show("softlist<int> + date (1, 2h)", \f257(), (1, 2h));
show("softlist<int> + date (1, 2026-01-01T00:00:00Z)", \f257(), (1, 2026-01-01T00:00:00Z));
show("date + softlist<int> (2h, (1,))", \f258(), (2h, (1,)));
show("date + softlist<int> (2h, 1)", \f258(), (2h, 1));
show("date + softlist<int> (2026-01-01T00:00:00Z, (1,))", \f258(), (2026-01-01T00:00:00Z, (1,)));
show("date + softlist<int> (2026-01-01T00:00:00Z, 1)", \f258(), (2026-01-01T00:00:00Z, 1));
show("softlist<int> + date<relative> ((1,), 2h)", \f259(), ((1,), 2h));
show("softlist<int> + date<relative> (1, 2h)", \f259(), (1, 2h));
show("date<relative> + softlist<int> (2h, (1,))", \f260(), (2h, (1,)));
show("date<relative> + softlist<int> (2h, 1)", \f260(), (2h, 1));
show("softlist<int> + bool ((1,), True)", \f261(), ((1,), True));
show("softlist<int> + bool (1, True)", \f261(), (1, True));
show("bool + softlist<int> (True, (1,))", \f262(), (True, (1,)));
show("bool + softlist<int> (True, 1)", \f262(), (True, 1));
show("softlist<int> + binary ((1,), <01>)", \f263(), ((1,), <01>));
show("softlist<int> + binary (1, <01>)", \f263(), (1, <01>));
show("binary + softlist<int> (<01>, (1,))", \f264(), (<01>, (1,)));
show("binary + softlist<int> (<01>, 1)", \f264(), (<01>, 1));
show("softlist + list ((1,), (1,))", \f265(), ((1,), (1,)));
show("softlist + list ((1,), ())", \f265(), ((1,), ()));
show("softlist + list (2, (1,))", \f265(), (2, (1,)));
show("softlist + list (2, ())", \f265(), (2, ()));
show("softlist + list<auto> ((1,), (1, \"a\"))", \f266(), ((1,), (1, "a")));
show("softlist + list<auto> ((1,), ())", \f266(), ((1,), ()));
show("softlist + list<auto> (2, (1, \"a\"))", \f266(), (2, (1, "a")));
show("softlist + list<auto> (2, ())", \f266(), (2, ()));
show("softlist + list<int> ((1,), (1,))", \f267(), ((1,), (1,)));
show("softlist + list<int> ((1,), ())", \f267(), ((1,), ()));
show("softlist + list<int> (2, (1,))", \f267(), (2, (1,)));
show("softlist + list<int> (2, ())", \f267(), (2, ()));
show("softlist + list<string> ((1,), (\"a\",))", \f268(), ((1,), ("a",)));
show("softlist + list<string> (2, (\"a\",))", \f268(), (2, ("a",)));
show("softlist + list<*int> ((1,), (1,))", \f269(), ((1,), (1,)));
show("softlist + list<*int> (2, (1,))", \f269(), (2, (1,)));
show("softlist + softlist<int> ((1,), (1,))", \f270(), ((1,), (1,)));
show("softlist + softlist<int> ((1,), 1)", \f270(), ((1,), 1));
show("softlist + softlist<int> (2, (1,))", \f270(), (2, (1,)));
show("softlist + softlist<int> (2, 1)", \f270(), (2, 1));
show("softlist + softlist ((1,), (1,))", \f271(), ((1,), (1,)));
show("softlist + softlist ((1,), 2)", \f271(), ((1,), 2));
show("softlist + softlist (2, (1,))", \f271(), (2, (1,)));
show("softlist + softlist (2, 2)", \f271(), (2, 2));
show("softlist + list<softint> ((1,), (1,))", \f272(), ((1,), (1,)));
show("softlist + list<softint> (2, (1,))", \f272(), (2, (1,)));
show("softlist + list<Base> ((1,), (new Derived(),))", \f273(), ((1,), (new Derived(),)));
show("softlist + list<Base> (2, (new Derived(),))", \f273(), (2, (new Derived(),)));
show("softlist + list<Derived> ((1,), (new Derived(),))", \f274(), ((1,), (new Derived(),)));
show("softlist + list<Derived> (2, (new Derived(),))", \f274(), (2, (new Derived(),)));
show("softlist + list<date> ((1,), (1h,))", \f275(), ((1,), (1h,)));
show("softlist + list<date> (2, (1h,))", \f275(), (2, (1h,)));
show("softlist + list<hash<string, int>> ((1,), ({\"a\": 1},))", \f276(), ((1,), ({"a": 1},)));
show("softlist + list<hash<string, int>> (2, ({\"a\": 1},))", \f276(), (2, ({"a": 1},)));
show("softlist + int ((1,), 2)", \f277(), ((1,), 2));
show("softlist + int (2, 2)", \f277(), (2, 2));
show("int + softlist (2, (1,))", \f278(), (2, (1,)));
show("int + softlist (2, 2)", \f278(), (2, 2));
show("softlist + softint ((1,), 2)", \f279(), ((1,), 2));
show("softlist + softint ((1,), \"3\")", \f279(), ((1,), "3"));
show("softlist + softint (2, 2)", \f279(), (2, 2));
show("softlist + softint (2, \"3\")", \f279(), (2, "3"));
show("softint + softlist (2, (1,))", \f280(), (2, (1,)));
show("softint + softlist (2, 2)", \f280(), (2, 2));
show("softint + softlist (\"3\", (1,))", \f280(), ("3", (1,)));
show("softint + softlist (\"3\", 2)", \f280(), ("3", 2));
show("softlist + *int ((1,), 2)", \f281(), ((1,), 2));
show("softlist + *int ((1,), NOTHING)", \f281(), ((1,), NOTHING));
show("softlist + *int (2, 2)", \f281(), (2, 2));
show("softlist + *int (2, NOTHING)", \f281(), (2, NOTHING));
show("*int + softlist (2, (1,))", \f282(), (2, (1,)));
show("*int + softlist (2, 2)", \f282(), (2, 2));
show("*int + softlist (NOTHING, (1,))", \f282(), (NOTHING, (1,)));
show("*int + softlist (NOTHING, 2)", \f282(), (NOTHING, 2));
show("softlist + string ((1,), \"s\")", \f283(), ((1,), "s"));
show("softlist + string (2, \"s\")", \f283(), (2, "s"));
show("string + softlist (\"s\", (1,))", \f284(), ("s", (1,)));
show("string + softlist (\"s\", 2)", \f284(), ("s", 2));
show("softlist + hash<D> ((1,), <D>{})", \f285(), ((1,), <D>{}));
show("softlist + hash<D> (2, <D>{})", \f285(), (2, <D>{}));
show("hash<D> + softlist (<D>{}, (1,))", \f286(), (<D>{}, (1,)));
show("hash<D> + softlist (<D>{}, 2)", \f286(), (<D>{}, 2));
show("softlist + union<int, string> ((1,), 2)", \f287(), ((1,), 2));
show("softlist + union<int, string> ((1,), \"s\")", \f287(), ((1,), "s"));
show("softlist + union<int, string> (2, 2)", \f287(), (2, 2));
show("softlist + union<int, string> (2, \"s\")", \f287(), (2, "s"));
show("union<int, string> + softlist (2, (1,))", \f288(), (2, (1,)));
show("union<int, string> + softlist (2, 2)", \f288(), (2, 2));
show("union<int, string> + softlist (\"s\", (1,))", \f288(), ("s", (1,)));
show("union<int, string> + softlist (\"s\", 2)", \f288(), ("s", 2));
show("softlist + auto ((1,), 2)", \f289(), ((1,), 2));
show("softlist + auto ((1,), \"s\")", \f289(), ((1,), "s"));
show("softlist + auto ((1,), NOTHING)", \f289(), ((1,), NOTHING));
show("softlist + auto (2, 2)", \f289(), (2, 2));
show("softlist + auto (2, \"s\")", \f289(), (2, "s"));
show("softlist + auto (2, NOTHING)", \f289(), (2, NOTHING));
show("auto + softlist (2, (1,))", \f290(), (2, (1,)));
show("auto + softlist (2, 2)", \f290(), (2, 2));
show("auto + softlist (\"s\", (1,))", \f290(), ("s", (1,)));
show("auto + softlist (\"s\", 2)", \f290(), ("s", 2));
show("auto + softlist (NOTHING, (1,))", \f290(), (NOTHING, (1,)));
show("auto + softlist (NOTHING, 2)", \f290(), (NOTHING, 2));
show("softlist + float ((1,), 2.5)", \f291(), ((1,), 2.5));
show("softlist + float (2, 2.5)", \f291(), (2, 2.5));
show("float + softlist (2.5, (1,))", \f292(), (2.5, (1,)));
show("float + softlist (2.5, 2)", \f292(), (2.5, 2));
show("softlist + timeout ((1,), 1000)", \f293(), ((1,), 1000));
show("softlist + timeout (2, 1000)", \f293(), (2, 1000));
show("timeout + softlist (1000, (1,))", \f294(), (1000, (1,)));
show("timeout + softlist (1000, 2)", \f294(), (1000, 2));
show("softlist + object ((1,), new Derived())", \f295(), ((1,), new Derived()));
show("softlist + object (2, new Derived())", \f295(), (2, new Derived()));
show("object + softlist (new Derived(), (1,))", \f296(), (new Derived(), (1,)));
show("object + softlist (new Derived(), 2)", \f296(), (new Derived(), 2));
show("softlist + Base ((1,), new Derived())", \f297(), ((1,), new Derived()));
show("softlist + Base (2, new Derived())", \f297(), (2, new Derived()));
show("Base + softlist (new Derived(), (1,))", \f298(), (new Derived(), (1,)));
show("Base + softlist (new Derived(), 2)", \f298(), (new Derived(), 2));
show("softlist + hash<auto> ((1,), {\"b\": 2})", \f299(), ((1,), {"b": 2}));
show("softlist + hash<auto> (2, {\"b\": 2})", \f299(), (2, {"b": 2}));
show("hash<auto> + softlist ({\"b\": 2}, (1,))", \f300(), ({"b": 2}, (1,)));
show("hash<auto> + softlist ({\"b\": 2}, 2)", \f300(), ({"b": 2}, 2));
show("softlist + date ((1,), 2h)", \f301(), ((1,), 2h));
show("softlist + date ((1,), 2026-01-01T00:00:00Z)", \f301(), ((1,), 2026-01-01T00:00:00Z));
show("softlist + date (2, 2h)", \f301(), (2, 2h));
show("softlist + date (2, 2026-01-01T00:00:00Z)", \f301(), (2, 2026-01-01T00:00:00Z));
show("date + softlist (2h, (1,))", \f302(), (2h, (1,)));
show("date + softlist (2h, 2)", \f302(), (2h, 2));
show("date + softlist (2026-01-01T00:00:00Z, (1,))", \f302(), (2026-01-01T00:00:00Z, (1,)));
show("date + softlist (2026-01-01T00:00:00Z, 2)", \f302(), (2026-01-01T00:00:00Z, 2));
show("softlist + date<relative> ((1,), 2h)", \f303(), ((1,), 2h));
show("softlist + date<relative> (2, 2h)", \f303(), (2, 2h));
show("date<relative> + softlist (2h, (1,))", \f304(), (2h, (1,)));
show("date<relative> + softlist (2h, 2)", \f304(), (2h, 2));
show("softlist + bool ((1,), True)", \f305(), ((1,), True));
show("softlist + bool (2, True)", \f305(), (2, True));
show("bool + softlist (True, (1,))", \f306(), (True, (1,)));
show("bool + softlist (True, 2)", \f306(), (True, 2));
show("softlist + binary ((1,), <01>)", \f307(), ((1,), <01>));
show("softlist + binary (2, <01>)", \f307(), (2, <01>));
show("binary + softlist (<01>, (1,))", \f308(), (<01>, (1,)));
show("binary + softlist (<01>, 2)", \f308(), (<01>, 2));
show("list<softint> + list ((1,), (1,))", \f309(), ((1,), (1,)));
show("list<softint> + list ((1,), ())", \f309(), ((1,), ()));
show("list<softint> + list<auto> ((1,), (1, \"a\"))", \f310(), ((1,), (1, "a")));
show("list<softint> + list<auto> ((1,), ())", \f310(), ((1,), ()));
show("list<softint> + list<int> ((1,), (1,))", \f311(), ((1,), (1,)));
show("list<softint> + list<int> ((1,), ())", \f311(), ((1,), ()));
show("list<softint> + list<string> ((1,), (\"a\",))", \f312(), ((1,), ("a",)));
show("list<softint> + list<*int> ((1,), (1,))", \f313(), ((1,), (1,)));
show("list<softint> + softlist<int> ((1,), (1,))", \f314(), ((1,), (1,)));
show("list<softint> + softlist<int> ((1,), 1)", \f314(), ((1,), 1));
show("list<softint> + softlist ((1,), (1,))", \f315(), ((1,), (1,)));
show("list<softint> + softlist ((1,), 2)", \f315(), ((1,), 2));
show("list<softint> + list<softint> ((1,), (1,))", \f316(), ((1,), (1,)));
show("list<softint> + list<Base> ((1,), (new Derived(),))", \f317(), ((1,), (new Derived(),)));
show("list<softint> + list<Derived> ((1,), (new Derived(),))", \f318(), ((1,), (new Derived(),)));
show("list<softint> + list<date> ((1,), (1h,))", \f319(), ((1,), (1h,)));
show("list<softint> + list<hash<string, int>> ((1,), ({\"a\": 1},))", \f320(), ((1,), ({"a": 1},)));
show("list<softint> + int ((1,), 2)", \f321(), ((1,), 2));
show("int + list<softint> (2, (1,))", \f322(), (2, (1,)));
show("list<softint> + softint ((1,), 2)", \f323(), ((1,), 2));
show("list<softint> + softint ((1,), \"3\")", \f323(), ((1,), "3"));
show("softint + list<softint> (2, (1,))", \f324(), (2, (1,)));
show("softint + list<softint> (\"3\", (1,))", \f324(), ("3", (1,)));
show("list<softint> + *int ((1,), 2)", \f325(), ((1,), 2));
show("list<softint> + *int ((1,), NOTHING)", \f325(), ((1,), NOTHING));
show("*int + list<softint> (2, (1,))", \f326(), (2, (1,)));
show("*int + list<softint> (NOTHING, (1,))", \f326(), (NOTHING, (1,)));
show("list<softint> + string ((1,), \"s\")", \f327(), ((1,), "s"));
show("string + list<softint> (\"s\", (1,))", \f328(), ("s", (1,)));
show("list<softint> + hash<D> ((1,), <D>{})", \f329(), ((1,), <D>{}));
show("hash<D> + list<softint> (<D>{}, (1,))", \f330(), (<D>{}, (1,)));
show("list<softint> + union<int, string> ((1,), 2)", \f331(), ((1,), 2));
show("list<softint> + union<int, string> ((1,), \"s\")", \f331(), ((1,), "s"));
show("union<int, string> + list<softint> (2, (1,))", \f332(), (2, (1,)));
show("union<int, string> + list<softint> (\"s\", (1,))", \f332(), ("s", (1,)));
show("list<softint> + auto ((1,), 2)", \f333(), ((1,), 2));
show("list<softint> + auto ((1,), \"s\")", \f333(), ((1,), "s"));
show("list<softint> + auto ((1,), NOTHING)", \f333(), ((1,), NOTHING));
show("auto + list<softint> (2, (1,))", \f334(), (2, (1,)));
show("auto + list<softint> (\"s\", (1,))", \f334(), ("s", (1,)));
show("auto + list<softint> (NOTHING, (1,))", \f334(), (NOTHING, (1,)));
show("list<softint> + float ((1,), 2.5)", \f335(), ((1,), 2.5));
show("float + list<softint> (2.5, (1,))", \f336(), (2.5, (1,)));
show("list<softint> + timeout ((1,), 1000)", \f337(), ((1,), 1000));
show("timeout + list<softint> (1000, (1,))", \f338(), (1000, (1,)));
show("list<softint> + object ((1,), new Derived())", \f339(), ((1,), new Derived()));
show("object + list<softint> (new Derived(), (1,))", \f340(), (new Derived(), (1,)));
show("list<softint> + Base ((1,), new Derived())", \f341(), ((1,), new Derived()));
show("Base + list<softint> (new Derived(), (1,))", \f342(), (new Derived(), (1,)));
show("list<softint> + hash<auto> ((1,), {\"b\": 2})", \f343(), ((1,), {"b": 2}));
show("hash<auto> + list<softint> ({\"b\": 2}, (1,))", \f344(), ({"b": 2}, (1,)));
show("list<softint> + date ((1,), 2h)", \f345(), ((1,), 2h));
show("list<softint> + date ((1,), 2026-01-01T00:00:00Z)", \f345(), ((1,), 2026-01-01T00:00:00Z));
show("date + list<softint> (2h, (1,))", \f346(), (2h, (1,)));
show("date + list<softint> (2026-01-01T00:00:00Z, (1,))", \f346(), (2026-01-01T00:00:00Z, (1,)));
show("list<softint> + date<relative> ((1,), 2h)", \f347(), ((1,), 2h));
show("date<relative> + list<softint> (2h, (1,))", \f348(), (2h, (1,)));
show("list<softint> + bool ((1,), True)", \f349(), ((1,), True));
show("bool + list<softint> (True, (1,))", \f350(), (True, (1,)));
show("list<softint> + binary ((1,), <01>)", \f351(), ((1,), <01>));
show("binary + list<softint> (<01>, (1,))", \f352(), (<01>, (1,)));
show("list<Base> + list ((new Derived(),), (1,))", \f353(), ((new Derived(),), (1,)));
show("list<Base> + list ((new Derived(),), ())", \f353(), ((new Derived(),), ()));
show("list<Base> + list<auto> ((new Derived(),), (1, \"a\"))", \f354(), ((new Derived(),), (1, "a")));
show("list<Base> + list<auto> ((new Derived(),), ())", \f354(), ((new Derived(),), ()));
show("list<Base> + list<int> ((new Derived(),), (1,))", \f355(), ((new Derived(),), (1,)));
show("list<Base> + list<int> ((new Derived(),), ())", \f355(), ((new Derived(),), ()));
show("list<Base> + list<string> ((new Derived(),), (\"a\",))", \f356(), ((new Derived(),), ("a",)));
show("list<Base> + list<*int> ((new Derived(),), (1,))", \f357(), ((new Derived(),), (1,)));
show("list<Base> + softlist<int> ((new Derived(),), (1,))", \f358(), ((new Derived(),), (1,)));
show("list<Base> + softlist<int> ((new Derived(),), 1)", \f358(), ((new Derived(),), 1));
show("list<Base> + softlist ((new Derived(),), (1,))", \f359(), ((new Derived(),), (1,)));
show("list<Base> + softlist ((new Derived(),), 2)", \f359(), ((new Derived(),), 2));
show("list<Base> + list<softint> ((new Derived(),), (1,))", \f360(), ((new Derived(),), (1,)));
show("list<Base> + list<Base> ((new Derived(),), (new Derived(),))", \f361(), ((new Derived(),), (new Derived(),)));
show("list<Base> + list<Derived> ((new Derived(),), (new Derived(),))", \f362(), ((new Derived(),), (new Derived(),)));
show("list<Base> + list<date> ((new Derived(),), (1h,))", \f363(), ((new Derived(),), (1h,)));
show("list<Base> + list<hash<string, int>> ((new Derived(),), ({\"a\": 1},))",
    \f364(), ((new Derived(),), ({"a": 1},)));
show("list<Base> + int ((new Derived(),), 2)", \f365(), ((new Derived(),), 2));
show("int + list<Base> (2, (new Derived(),))", \f366(), (2, (new Derived(),)));
show("list<Base> + softint ((new Derived(),), 2)", \f367(), ((new Derived(),), 2));
show("list<Base> + softint ((new Derived(),), \"3\")", \f367(), ((new Derived(),), "3"));
show("softint + list<Base> (2, (new Derived(),))", \f368(), (2, (new Derived(),)));
show("softint + list<Base> (\"3\", (new Derived(),))", \f368(), ("3", (new Derived(),)));
show("list<Base> + *int ((new Derived(),), 2)", \f369(), ((new Derived(),), 2));
show("list<Base> + *int ((new Derived(),), NOTHING)", \f369(), ((new Derived(),), NOTHING));
show("*int + list<Base> (2, (new Derived(),))", \f370(), (2, (new Derived(),)));
show("*int + list<Base> (NOTHING, (new Derived(),))", \f370(), (NOTHING, (new Derived(),)));
show("list<Base> + string ((new Derived(),), \"s\")", \f371(), ((new Derived(),), "s"));
show("string + list<Base> (\"s\", (new Derived(),))", \f372(), ("s", (new Derived(),)));
show("list<Base> + hash<D> ((new Derived(),), <D>{})", \f373(), ((new Derived(),), <D>{}));
show("hash<D> + list<Base> (<D>{}, (new Derived(),))", \f374(), (<D>{}, (new Derived(),)));
show("list<Base> + union<int, string> ((new Derived(),), 2)", \f375(), ((new Derived(),), 2));
show("list<Base> + union<int, string> ((new Derived(),), \"s\")", \f375(), ((new Derived(),), "s"));
show("union<int, string> + list<Base> (2, (new Derived(),))", \f376(), (2, (new Derived(),)));
show("union<int, string> + list<Base> (\"s\", (new Derived(),))", \f376(), ("s", (new Derived(),)));
show("list<Base> + auto ((new Derived(),), 2)", \f377(), ((new Derived(),), 2));
show("list<Base> + auto ((new Derived(),), \"s\")", \f377(), ((new Derived(),), "s"));
show("list<Base> + auto ((new Derived(),), NOTHING)", \f377(), ((new Derived(),), NOTHING));
show("auto + list<Base> (2, (new Derived(),))", \f378(), (2, (new Derived(),)));
show("auto + list<Base> (\"s\", (new Derived(),))", \f378(), ("s", (new Derived(),)));
show("auto + list<Base> (NOTHING, (new Derived(),))", \f378(), (NOTHING, (new Derived(),)));
show("list<Base> + float ((new Derived(),), 2.5)", \f379(), ((new Derived(),), 2.5));
show("float + list<Base> (2.5, (new Derived(),))", \f380(), (2.5, (new Derived(),)));
show("list<Base> + timeout ((new Derived(),), 1000)", \f381(), ((new Derived(),), 1000));
show("timeout + list<Base> (1000, (new Derived(),))", \f382(), (1000, (new Derived(),)));
show("list<Base> + object ((new Derived(),), new Derived())", \f383(), ((new Derived(),), new Derived()));
show("object + list<Base> (new Derived(), (new Derived(),))", \f384(), (new Derived(), (new Derived(),)));
show("list<Base> + Base ((new Derived(),), new Derived())", \f385(), ((new Derived(),), new Derived()));
show("Base + list<Base> (new Derived(), (new Derived(),))", \f386(), (new Derived(), (new Derived(),)));
show("list<Base> + hash<auto> ((new Derived(),), {\"b\": 2})", \f387(), ((new Derived(),), {"b": 2}));
show("hash<auto> + list<Base> ({\"b\": 2}, (new Derived(),))", \f388(), ({"b": 2}, (new Derived(),)));
show("list<Base> + date ((new Derived(),), 2h)", \f389(), ((new Derived(),), 2h));
show("list<Base> + date ((new Derived(),), 2026-01-01T00:00:00Z)", \f389(), ((new Derived(),), 2026-01-01T00:00:00Z));
show("date + list<Base> (2h, (new Derived(),))", \f390(), (2h, (new Derived(),)));
show("date + list<Base> (2026-01-01T00:00:00Z, (new Derived(),))", \f390(), (2026-01-01T00:00:00Z, (new Derived(),)));
show("list<Base> + date<relative> ((new Derived(),), 2h)", \f391(), ((new Derived(),), 2h));
show("date<relative> + list<Base> (2h, (new Derived(),))", \f392(), (2h, (new Derived(),)));
show("list<Base> + bool ((new Derived(),), True)", \f393(), ((new Derived(),), True));
show("bool + list<Base> (True, (new Derived(),))", \f394(), (True, (new Derived(),)));
show("list<Base> + binary ((new Derived(),), <01>)", \f395(), ((new Derived(),), <01>));
show("binary + list<Base> (<01>, (new Derived(),))", \f396(), (<01>, (new Derived(),)));
show("list<Derived> + list ((new Derived(),), (1,))", \f397(), ((new Derived(),), (1,)));
show("list<Derived> + list ((new Derived(),), ())", \f397(), ((new Derived(),), ()));
show("list<Derived> + list<auto> ((new Derived(),), (1, \"a\"))", \f398(), ((new Derived(),), (1, "a")));
show("list<Derived> + list<auto> ((new Derived(),), ())", \f398(), ((new Derived(),), ()));
show("list<Derived> + list<int> ((new Derived(),), (1,))", \f399(), ((new Derived(),), (1,)));
show("list<Derived> + list<int> ((new Derived(),), ())", \f399(), ((new Derived(),), ()));
show("list<Derived> + list<string> ((new Derived(),), (\"a\",))", \f400(), ((new Derived(),), ("a",)));
show("list<Derived> + list<*int> ((new Derived(),), (1,))", \f401(), ((new Derived(),), (1,)));
show("list<Derived> + softlist<int> ((new Derived(),), (1,))", \f402(), ((new Derived(),), (1,)));
show("list<Derived> + softlist<int> ((new Derived(),), 1)", \f402(), ((new Derived(),), 1));
show("list<Derived> + softlist ((new Derived(),), (1,))", \f403(), ((new Derived(),), (1,)));
show("list<Derived> + softlist ((new Derived(),), 2)", \f403(), ((new Derived(),), 2));
show("list<Derived> + list<softint> ((new Derived(),), (1,))", \f404(), ((new Derived(),), (1,)));
show("list<Derived> + list<Base> ((new Derived(),), (new Derived(),))", \f405(), ((new Derived(),), (new Derived(),)));
show("list<Derived> + list<Derived> ((new Derived(),), (new Derived(),))",
    \f406(), ((new Derived(),), (new Derived(),)));
show("list<Derived> + list<date> ((new Derived(),), (1h,))", \f407(), ((new Derived(),), (1h,)));
show("list<Derived> + list<hash<string, int>> ((new Derived(),), ({\"a\": 1},))",
    \f408(), ((new Derived(),), ({"a": 1},)));
show("list<Derived> + int ((new Derived(),), 2)", \f409(), ((new Derived(),), 2));
show("int + list<Derived> (2, (new Derived(),))", \f410(), (2, (new Derived(),)));
show("list<Derived> + softint ((new Derived(),), 2)", \f411(), ((new Derived(),), 2));
show("list<Derived> + softint ((new Derived(),), \"3\")", \f411(), ((new Derived(),), "3"));
show("softint + list<Derived> (2, (new Derived(),))", \f412(), (2, (new Derived(),)));
show("softint + list<Derived> (\"3\", (new Derived(),))", \f412(), ("3", (new Derived(),)));
show("list<Derived> + *int ((new Derived(),), 2)", \f413(), ((new Derived(),), 2));
show("list<Derived> + *int ((new Derived(),), NOTHING)", \f413(), ((new Derived(),), NOTHING));
show("*int + list<Derived> (2, (new Derived(),))", \f414(), (2, (new Derived(),)));
show("*int + list<Derived> (NOTHING, (new Derived(),))", \f414(), (NOTHING, (new Derived(),)));
show("list<Derived> + string ((new Derived(),), \"s\")", \f415(), ((new Derived(),), "s"));
show("string + list<Derived> (\"s\", (new Derived(),))", \f416(), ("s", (new Derived(),)));
show("list<Derived> + hash<D> ((new Derived(),), <D>{})", \f417(), ((new Derived(),), <D>{}));
show("hash<D> + list<Derived> (<D>{}, (new Derived(),))", \f418(), (<D>{}, (new Derived(),)));
show("list<Derived> + union<int, string> ((new Derived(),), 2)", \f419(), ((new Derived(),), 2));
show("list<Derived> + union<int, string> ((new Derived(),), \"s\")", \f419(), ((new Derived(),), "s"));
show("union<int, string> + list<Derived> (2, (new Derived(),))", \f420(), (2, (new Derived(),)));
show("union<int, string> + list<Derived> (\"s\", (new Derived(),))", \f420(), ("s", (new Derived(),)));
show("list<Derived> + auto ((new Derived(),), 2)", \f421(), ((new Derived(),), 2));
show("list<Derived> + auto ((new Derived(),), \"s\")", \f421(), ((new Derived(),), "s"));
show("list<Derived> + auto ((new Derived(),), NOTHING)", \f421(), ((new Derived(),), NOTHING));
show("auto + list<Derived> (2, (new Derived(),))", \f422(), (2, (new Derived(),)));
show("auto + list<Derived> (\"s\", (new Derived(),))", \f422(), ("s", (new Derived(),)));
show("auto + list<Derived> (NOTHING, (new Derived(),))", \f422(), (NOTHING, (new Derived(),)));
show("list<Derived> + float ((new Derived(),), 2.5)", \f423(), ((new Derived(),), 2.5));
show("float + list<Derived> (2.5, (new Derived(),))", \f424(), (2.5, (new Derived(),)));
show("list<Derived> + timeout ((new Derived(),), 1000)", \f425(), ((new Derived(),), 1000));
show("timeout + list<Derived> (1000, (new Derived(),))", \f426(), (1000, (new Derived(),)));
show("list<Derived> + object ((new Derived(),), new Derived())", \f427(), ((new Derived(),), new Derived()));
show("object + list<Derived> (new Derived(), (new Derived(),))", \f428(), (new Derived(), (new Derived(),)));
show("list<Derived> + Base ((new Derived(),), new Derived())", \f429(), ((new Derived(),), new Derived()));
show("Base + list<Derived> (new Derived(), (new Derived(),))", \f430(), (new Derived(), (new Derived(),)));
show("list<Derived> + hash<auto> ((new Derived(),), {\"b\": 2})", \f431(), ((new Derived(),), {"b": 2}));
show("hash<auto> + list<Derived> ({\"b\": 2}, (new Derived(),))", \f432(), ({"b": 2}, (new Derived(),)));
show("list<Derived> + date ((new Derived(),), 2h)", \f433(), ((new Derived(),), 2h));
show("list<Derived> + date ((new Derived(),), 2026-01-01T00:00:00Z)",
    \f433(), ((new Derived(),), 2026-01-01T00:00:00Z));
show("date + list<Derived> (2h, (new Derived(),))", \f434(), (2h, (new Derived(),)));
show("date + list<Derived> (2026-01-01T00:00:00Z, (new Derived(),))",
    \f434(), (2026-01-01T00:00:00Z, (new Derived(),)));
show("list<Derived> + date<relative> ((new Derived(),), 2h)", \f435(), ((new Derived(),), 2h));
show("date<relative> + list<Derived> (2h, (new Derived(),))", \f436(), (2h, (new Derived(),)));
show("list<Derived> + bool ((new Derived(),), True)", \f437(), ((new Derived(),), True));
show("bool + list<Derived> (True, (new Derived(),))", \f438(), (True, (new Derived(),)));
show("list<Derived> + binary ((new Derived(),), <01>)", \f439(), ((new Derived(),), <01>));
show("binary + list<Derived> (<01>, (new Derived(),))", \f440(), (<01>, (new Derived(),)));
show("list<date> + list ((1h,), (1,))", \f441(), ((1h,), (1,)));
show("list<date> + list ((1h,), ())", \f441(), ((1h,), ()));
show("list<date> + list<auto> ((1h,), (1, \"a\"))", \f442(), ((1h,), (1, "a")));
show("list<date> + list<auto> ((1h,), ())", \f442(), ((1h,), ()));
show("list<date> + list<int> ((1h,), (1,))", \f443(), ((1h,), (1,)));
show("list<date> + list<int> ((1h,), ())", \f443(), ((1h,), ()));
show("list<date> + list<string> ((1h,), (\"a\",))", \f444(), ((1h,), ("a",)));
show("list<date> + list<*int> ((1h,), (1,))", \f445(), ((1h,), (1,)));
show("list<date> + softlist<int> ((1h,), (1,))", \f446(), ((1h,), (1,)));
show("list<date> + softlist<int> ((1h,), 1)", \f446(), ((1h,), 1));
show("list<date> + softlist ((1h,), (1,))", \f447(), ((1h,), (1,)));
show("list<date> + softlist ((1h,), 2)", \f447(), ((1h,), 2));
show("list<date> + list<softint> ((1h,), (1,))", \f448(), ((1h,), (1,)));
show("list<date> + list<Base> ((1h,), (new Derived(),))", \f449(), ((1h,), (new Derived(),)));
show("list<date> + list<Derived> ((1h,), (new Derived(),))", \f450(), ((1h,), (new Derived(),)));
show("list<date> + list<date> ((1h,), (1h,))", \f451(), ((1h,), (1h,)));
show("list<date> + list<hash<string, int>> ((1h,), ({\"a\": 1},))", \f452(), ((1h,), ({"a": 1},)));
show("list<date> + int ((1h,), 2)", \f453(), ((1h,), 2));
show("int + list<date> (2, (1h,))", \f454(), (2, (1h,)));
show("list<date> + softint ((1h,), 2)", \f455(), ((1h,), 2));
show("list<date> + softint ((1h,), \"3\")", \f455(), ((1h,), "3"));
show("softint + list<date> (2, (1h,))", \f456(), (2, (1h,)));
show("softint + list<date> (\"3\", (1h,))", \f456(), ("3", (1h,)));
show("list<date> + *int ((1h,), 2)", \f457(), ((1h,), 2));
show("list<date> + *int ((1h,), NOTHING)", \f457(), ((1h,), NOTHING));
show("*int + list<date> (2, (1h,))", \f458(), (2, (1h,)));
show("*int + list<date> (NOTHING, (1h,))", \f458(), (NOTHING, (1h,)));
show("list<date> + string ((1h,), \"s\")", \f459(), ((1h,), "s"));
show("string + list<date> (\"s\", (1h,))", \f460(), ("s", (1h,)));
show("list<date> + hash<D> ((1h,), <D>{})", \f461(), ((1h,), <D>{}));
show("hash<D> + list<date> (<D>{}, (1h,))", \f462(), (<D>{}, (1h,)));
show("list<date> + union<int, string> ((1h,), 2)", \f463(), ((1h,), 2));
show("list<date> + union<int, string> ((1h,), \"s\")", \f463(), ((1h,), "s"));
show("union<int, string> + list<date> (2, (1h,))", \f464(), (2, (1h,)));
show("union<int, string> + list<date> (\"s\", (1h,))", \f464(), ("s", (1h,)));
show("list<date> + auto ((1h,), 2)", \f465(), ((1h,), 2));
show("list<date> + auto ((1h,), \"s\")", \f465(), ((1h,), "s"));
show("list<date> + auto ((1h,), NOTHING)", \f465(), ((1h,), NOTHING));
show("auto + list<date> (2, (1h,))", \f466(), (2, (1h,)));
show("auto + list<date> (\"s\", (1h,))", \f466(), ("s", (1h,)));
show("auto + list<date> (NOTHING, (1h,))", \f466(), (NOTHING, (1h,)));
show("list<date> + float ((1h,), 2.5)", \f467(), ((1h,), 2.5));
show("float + list<date> (2.5, (1h,))", \f468(), (2.5, (1h,)));
show("list<date> + timeout ((1h,), 1000)", \f469(), ((1h,), 1000));
show("timeout + list<date> (1000, (1h,))", \f470(), (1000, (1h,)));
show("list<date> + object ((1h,), new Derived())", \f471(), ((1h,), new Derived()));
show("object + list<date> (new Derived(), (1h,))", \f472(), (new Derived(), (1h,)));
show("list<date> + Base ((1h,), new Derived())", \f473(), ((1h,), new Derived()));
show("Base + list<date> (new Derived(), (1h,))", \f474(), (new Derived(), (1h,)));
show("list<date> + hash<auto> ((1h,), {\"b\": 2})", \f475(), ((1h,), {"b": 2}));
show("hash<auto> + list<date> ({\"b\": 2}, (1h,))", \f476(), ({"b": 2}, (1h,)));
show("list<date> + date ((1h,), 2h)", \f477(), ((1h,), 2h));
show("list<date> + date ((1h,), 2026-01-01T00:00:00Z)", \f477(), ((1h,), 2026-01-01T00:00:00Z));
show("date + list<date> (2h, (1h,))", \f478(), (2h, (1h,)));
show("date + list<date> (2026-01-01T00:00:00Z, (1h,))", \f478(), (2026-01-01T00:00:00Z, (1h,)));
show("list<date> + date<relative> ((1h,), 2h)", \f479(), ((1h,), 2h));
show("date<relative> + list<date> (2h, (1h,))", \f480(), (2h, (1h,)));
show("list<date> + bool ((1h,), True)", \f481(), ((1h,), True));
show("bool + list<date> (True, (1h,))", \f482(), (True, (1h,)));
show("list<date> + binary ((1h,), <01>)", \f483(), ((1h,), <01>));
show("binary + list<date> (<01>, (1h,))", \f484(), (<01>, (1h,)));
show("list<hash<string, int>> + list (({\"a\": 1},), (1,))", \f485(), (({"a": 1},), (1,)));
show("list<hash<string, int>> + list (({\"a\": 1},), ())", \f485(), (({"a": 1},), ()));
show("list<hash<string, int>> + list<auto> (({\"a\": 1},), (1, \"a\"))", \f486(), (({"a": 1},), (1, "a")));
show("list<hash<string, int>> + list<auto> (({\"a\": 1},), ())", \f486(), (({"a": 1},), ()));
show("list<hash<string, int>> + list<int> (({\"a\": 1},), (1,))", \f487(), (({"a": 1},), (1,)));
show("list<hash<string, int>> + list<int> (({\"a\": 1},), ())", \f487(), (({"a": 1},), ()));
show("list<hash<string, int>> + list<string> (({\"a\": 1},), (\"a\",))", \f488(), (({"a": 1},), ("a",)));
show("list<hash<string, int>> + list<*int> (({\"a\": 1},), (1,))", \f489(), (({"a": 1},), (1,)));
show("list<hash<string, int>> + softlist<int> (({\"a\": 1},), (1,))", \f490(), (({"a": 1},), (1,)));
show("list<hash<string, int>> + softlist<int> (({\"a\": 1},), 1)", \f490(), (({"a": 1},), 1));
show("list<hash<string, int>> + softlist (({\"a\": 1},), (1,))", \f491(), (({"a": 1},), (1,)));
show("list<hash<string, int>> + softlist (({\"a\": 1},), 2)", \f491(), (({"a": 1},), 2));
show("list<hash<string, int>> + list<softint> (({\"a\": 1},), (1,))", \f492(), (({"a": 1},), (1,)));
show("list<hash<string, int>> + list<Base> (({\"a\": 1},), (new Derived(),))",
    \f493(), (({"a": 1},), (new Derived(),)));
show("list<hash<string, int>> + list<Derived> (({\"a\": 1},), (new Derived(),))",
    \f494(), (({"a": 1},), (new Derived(),)));
show("list<hash<string, int>> + list<date> (({\"a\": 1},), (1h,))", \f495(), (({"a": 1},), (1h,)));
show("list<hash<string, int>> + list<hash<string, int>> (({\"a\": 1},), ({\"a\": 1},))",
    \f496(), (({"a": 1},), ({"a": 1},)));
show("list<hash<string, int>> + int (({\"a\": 1},), 2)", \f497(), (({"a": 1},), 2));
show("int + list<hash<string, int>> (2, ({\"a\": 1},))", \f498(), (2, ({"a": 1},)));
show("list<hash<string, int>> + softint (({\"a\": 1},), 2)", \f499(), (({"a": 1},), 2));
show("list<hash<string, int>> + softint (({\"a\": 1},), \"3\")", \f499(), (({"a": 1},), "3"));
show("softint + list<hash<string, int>> (2, ({\"a\": 1},))", \f500(), (2, ({"a": 1},)));
show("softint + list<hash<string, int>> (\"3\", ({\"a\": 1},))", \f500(), ("3", ({"a": 1},)));
show("list<hash<string, int>> + *int (({\"a\": 1},), 2)", \f501(), (({"a": 1},), 2));
show("list<hash<string, int>> + *int (({\"a\": 1},), NOTHING)", \f501(), (({"a": 1},), NOTHING));
show("*int + list<hash<string, int>> (2, ({\"a\": 1},))", \f502(), (2, ({"a": 1},)));
show("*int + list<hash<string, int>> (NOTHING, ({\"a\": 1},))", \f502(), (NOTHING, ({"a": 1},)));
show("list<hash<string, int>> + string (({\"a\": 1},), \"s\")", \f503(), (({"a": 1},), "s"));
show("string + list<hash<string, int>> (\"s\", ({\"a\": 1},))", \f504(), ("s", ({"a": 1},)));
show("list<hash<string, int>> + hash<D> (({\"a\": 1},), <D>{})", \f505(), (({"a": 1},), <D>{}));
show("hash<D> + list<hash<string, int>> (<D>{}, ({\"a\": 1},))", \f506(), (<D>{}, ({"a": 1},)));
show("list<hash<string, int>> + union<int, string> (({\"a\": 1},), 2)", \f507(), (({"a": 1},), 2));
show("list<hash<string, int>> + union<int, string> (({\"a\": 1},), \"s\")", \f507(), (({"a": 1},), "s"));
show("union<int, string> + list<hash<string, int>> (2, ({\"a\": 1},))", \f508(), (2, ({"a": 1},)));
show("union<int, string> + list<hash<string, int>> (\"s\", ({\"a\": 1},))", \f508(), ("s", ({"a": 1},)));
show("list<hash<string, int>> + auto (({\"a\": 1},), 2)", \f509(), (({"a": 1},), 2));
show("list<hash<string, int>> + auto (({\"a\": 1},), \"s\")", \f509(), (({"a": 1},), "s"));
show("list<hash<string, int>> + auto (({\"a\": 1},), NOTHING)", \f509(), (({"a": 1},), NOTHING));
show("auto + list<hash<string, int>> (2, ({\"a\": 1},))", \f510(), (2, ({"a": 1},)));
show("auto + list<hash<string, int>> (\"s\", ({\"a\": 1},))", \f510(), ("s", ({"a": 1},)));
show("auto + list<hash<string, int>> (NOTHING, ({\"a\": 1},))", \f510(), (NOTHING, ({"a": 1},)));
show("list<hash<string, int>> + float (({\"a\": 1},), 2.5)", \f511(), (({"a": 1},), 2.5));
show("float + list<hash<string, int>> (2.5, ({\"a\": 1},))", \f512(), (2.5, ({"a": 1},)));
show("list<hash<string, int>> + timeout (({\"a\": 1},), 1000)", \f513(), (({"a": 1},), 1000));
show("timeout + list<hash<string, int>> (1000, ({\"a\": 1},))", \f514(), (1000, ({"a": 1},)));
show("list<hash<string, int>> + object (({\"a\": 1},), new Derived())", \f515(), (({"a": 1},), new Derived()));
show("object + list<hash<string, int>> (new Derived(), ({\"a\": 1},))", \f516(), (new Derived(), ({"a": 1},)));
show("list<hash<string, int>> + Base (({\"a\": 1},), new Derived())", \f517(), (({"a": 1},), new Derived()));
show("Base + list<hash<string, int>> (new Derived(), ({\"a\": 1},))", \f518(), (new Derived(), ({"a": 1},)));
show("list<hash<string, int>> + hash<auto> (({\"a\": 1},), {\"b\": 2})", \f519(), (({"a": 1},), {"b": 2}));
show("hash<auto> + list<hash<string, int>> ({\"b\": 2}, ({\"a\": 1},))", \f520(), ({"b": 2}, ({"a": 1},)));
show("list<hash<string, int>> + date (({\"a\": 1},), 2h)", \f521(), (({"a": 1},), 2h));
show("list<hash<string, int>> + date (({\"a\": 1},), 2026-01-01T00:00:00Z)",
    \f521(), (({"a": 1},), 2026-01-01T00:00:00Z));
show("date + list<hash<string, int>> (2h, ({\"a\": 1},))", \f522(), (2h, ({"a": 1},)));
show("date + list<hash<string, int>> (2026-01-01T00:00:00Z, ({\"a\": 1},))",
    \f522(), (2026-01-01T00:00:00Z, ({"a": 1},)));
show("list<hash<string, int>> + date<relative> (({\"a\": 1},), 2h)", \f523(), (({"a": 1},), 2h));
show("date<relative> + list<hash<string, int>> (2h, ({\"a\": 1},))", \f524(), (2h, ({"a": 1},)));
show("list<hash<string, int>> + bool (({\"a\": 1},), True)", \f525(), (({"a": 1},), True));
show("bool + list<hash<string, int>> (True, ({\"a\": 1},))", \f526(), (True, ({"a": 1},)));
show("list<hash<string, int>> + binary (({\"a\": 1},), <01>)", \f527(), (({"a": 1},), <01>));
show("binary + list<hash<string, int>> (<01>, ({\"a\": 1},))", \f528(), (<01>, ({"a": 1},)));
