#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the cases of parts 1-2 of ArithParseResultTypes.qc (generated for the operand types and values below): each function
# applies an arithmetic operator to parameters of declared types (which are not narrowed) and returns the result in a
# list literal, whose element type is the parse result type of the operator in the compiled modes (the AST interpreter
# takes it from the value): a parse result type that does not allow the value raises RUNTIME-TYPE-ERROR, and one whose
# first member converts it gives another value; each case is called with values of every type that the declared types
# allow and prints the list (an object as its class name); the output of every execution mode must be that of the AST
# interpreter

%modern
# some combinations are invalid operations, which the parser warns about
%disable-warning invalid-operation

class C {
    public { int m = 1; }
}

sub show(string label, code<auto()> c) {
    try {
        auto v = c();
        printf("%s => %y\n", label, map $1.typeCode() == NT_OBJECT ? "object<" + $1.className() + ">" : $1, v);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s => %s\n", label, ex.err);
    }
}

auto sub c1(int x, int y) {
    return (x + y,);
}
auto sub c2(int x, float y) {
    return (x + y,);
}
auto sub c3(int x, number y) {
    return (x + y,);
}
auto sub c4(int x, string y) {
    return (x + y,);
}
auto sub c5(int x, bool y) {
    return (x + y,);
}
auto sub c6(int x, date y) {
    return (x + y,);
}
auto sub c7(int x, *int y) {
    return (x + y,);
}
auto sub c8(int x, *float y) {
    return (x + y,);
}
auto sub c9(int x, *number y) {
    return (x + y,);
}
auto sub c10(int x, union<int, float> y) {
    return (x + y,);
}
auto sub c11(int x, union<int, string> y) {
    return (x + y,);
}
auto sub c12(int x, *union<int, number> y) {
    return (x + y,);
}
auto sub c13(int x, list<int> y) {
    return (x + y,);
}
auto sub c14(int x, hash<auto> y) {
    return (x + y,);
}
auto sub c15(int x, binary y) {
    return (x + y,);
}
auto sub c16(int x, C y) {
    return (x + y,);
}
auto sub c17(float x, int y) {
    return (x + y,);
}
auto sub c18(float x, float y) {
    return (x + y,);
}
auto sub c19(float x, number y) {
    return (x + y,);
}
auto sub c20(float x, string y) {
    return (x + y,);
}
auto sub c21(float x, bool y) {
    return (x + y,);
}
auto sub c22(float x, date y) {
    return (x + y,);
}
auto sub c23(float x, *int y) {
    return (x + y,);
}
auto sub c24(float x, *float y) {
    return (x + y,);
}
auto sub c25(float x, *number y) {
    return (x + y,);
}
auto sub c26(float x, union<int, float> y) {
    return (x + y,);
}
auto sub c27(float x, union<int, string> y) {
    return (x + y,);
}
auto sub c28(float x, *union<int, number> y) {
    return (x + y,);
}
auto sub c29(float x, list<int> y) {
    return (x + y,);
}
auto sub c30(float x, hash<auto> y) {
    return (x + y,);
}
auto sub c31(float x, binary y) {
    return (x + y,);
}
auto sub c32(float x, C y) {
    return (x + y,);
}
auto sub c33(number x, int y) {
    return (x + y,);
}
auto sub c34(number x, float y) {
    return (x + y,);
}
auto sub c35(number x, number y) {
    return (x + y,);
}
auto sub c36(number x, string y) {
    return (x + y,);
}
auto sub c37(number x, bool y) {
    return (x + y,);
}
auto sub c38(number x, date y) {
    return (x + y,);
}
auto sub c39(number x, *int y) {
    return (x + y,);
}
auto sub c40(number x, *float y) {
    return (x + y,);
}
auto sub c41(number x, *number y) {
    return (x + y,);
}
auto sub c42(number x, union<int, float> y) {
    return (x + y,);
}
auto sub c43(number x, union<int, string> y) {
    return (x + y,);
}
auto sub c44(number x, *union<int, number> y) {
    return (x + y,);
}
auto sub c45(number x, list<int> y) {
    return (x + y,);
}
auto sub c46(number x, hash<auto> y) {
    return (x + y,);
}
auto sub c47(number x, binary y) {
    return (x + y,);
}
auto sub c48(number x, C y) {
    return (x + y,);
}
auto sub c49(string x, int y) {
    return (x + y,);
}
auto sub c50(string x, float y) {
    return (x + y,);
}
auto sub c51(string x, number y) {
    return (x + y,);
}
auto sub c52(string x, string y) {
    return (x + y,);
}
auto sub c53(string x, bool y) {
    return (x + y,);
}
auto sub c54(string x, date y) {
    return (x + y,);
}
auto sub c55(string x, *int y) {
    return (x + y,);
}
auto sub c56(string x, *float y) {
    return (x + y,);
}
auto sub c57(string x, *number y) {
    return (x + y,);
}
auto sub c58(string x, union<int, float> y) {
    return (x + y,);
}
auto sub c59(string x, union<int, string> y) {
    return (x + y,);
}
auto sub c60(string x, *union<int, number> y) {
    return (x + y,);
}
auto sub c61(string x, list<int> y) {
    return (x + y,);
}
auto sub c62(string x, hash<auto> y) {
    return (x + y,);
}
auto sub c63(string x, binary y) {
    return (x + y,);
}
auto sub c64(string x, C y) {
    return (x + y,);
}
auto sub c65(bool x, int y) {
    return (x + y,);
}
auto sub c66(bool x, float y) {
    return (x + y,);
}
auto sub c67(bool x, number y) {
    return (x + y,);
}
auto sub c68(bool x, string y) {
    return (x + y,);
}
auto sub c69(bool x, bool y) {
    return (x + y,);
}
auto sub c70(bool x, date y) {
    return (x + y,);
}
auto sub c71(bool x, *int y) {
    return (x + y,);
}
auto sub c72(bool x, *float y) {
    return (x + y,);
}
auto sub c73(bool x, *number y) {
    return (x + y,);
}
auto sub c74(bool x, union<int, float> y) {
    return (x + y,);
}
auto sub c75(bool x, union<int, string> y) {
    return (x + y,);
}
auto sub c76(bool x, *union<int, number> y) {
    return (x + y,);
}
auto sub c77(bool x, list<int> y) {
    return (x + y,);
}
auto sub c78(bool x, hash<auto> y) {
    return (x + y,);
}
auto sub c79(bool x, binary y) {
    return (x + y,);
}
auto sub c80(bool x, C y) {
    return (x + y,);
}
auto sub c81(date x, int y) {
    return (x + y,);
}
auto sub c82(date x, float y) {
    return (x + y,);
}
auto sub c83(date x, number y) {
    return (x + y,);
}
auto sub c84(date x, string y) {
    return (x + y,);
}
auto sub c85(date x, bool y) {
    return (x + y,);
}
auto sub c86(date x, date y) {
    return (x + y,);
}
auto sub c87(date x, *int y) {
    return (x + y,);
}
auto sub c88(date x, *float y) {
    return (x + y,);
}
auto sub c89(date x, *number y) {
    return (x + y,);
}
auto sub c90(date x, union<int, float> y) {
    return (x + y,);
}
auto sub c91(date x, union<int, string> y) {
    return (x + y,);
}
auto sub c92(date x, *union<int, number> y) {
    return (x + y,);
}
auto sub c93(date x, list<int> y) {
    return (x + y,);
}
auto sub c94(date x, hash<auto> y) {
    return (x + y,);
}
auto sub c95(date x, binary y) {
    return (x + y,);
}
auto sub c96(date x, C y) {
    return (x + y,);
}
auto sub c97(*int x, int y) {
    return (x + y,);
}
auto sub c98(*int x, float y) {
    return (x + y,);
}
auto sub c99(*int x, number y) {
    return (x + y,);
}
auto sub c100(*int x, string y) {
    return (x + y,);
}
auto sub c101(*int x, bool y) {
    return (x + y,);
}
auto sub c102(*int x, date y) {
    return (x + y,);
}
auto sub c103(*int x, *int y) {
    return (x + y,);
}
auto sub c104(*int x, *float y) {
    return (x + y,);
}
auto sub c105(*int x, *number y) {
    return (x + y,);
}
auto sub c106(*int x, union<int, float> y) {
    return (x + y,);
}
auto sub c107(*int x, union<int, string> y) {
    return (x + y,);
}
auto sub c108(*int x, *union<int, number> y) {
    return (x + y,);
}
auto sub c109(*int x, list<int> y) {
    return (x + y,);
}
auto sub c110(*int x, hash<auto> y) {
    return (x + y,);
}
auto sub c111(*int x, binary y) {
    return (x + y,);
}
auto sub c112(*int x, C y) {
    return (x + y,);
}
auto sub c113(*float x, int y) {
    return (x + y,);
}
auto sub c114(*float x, float y) {
    return (x + y,);
}
auto sub c115(*float x, number y) {
    return (x + y,);
}
auto sub c116(*float x, string y) {
    return (x + y,);
}
auto sub c117(*float x, bool y) {
    return (x + y,);
}
auto sub c118(*float x, date y) {
    return (x + y,);
}
auto sub c119(*float x, *int y) {
    return (x + y,);
}
auto sub c120(*float x, *float y) {
    return (x + y,);
}
auto sub c121(*float x, *number y) {
    return (x + y,);
}
auto sub c122(*float x, union<int, float> y) {
    return (x + y,);
}
auto sub c123(*float x, union<int, string> y) {
    return (x + y,);
}
auto sub c124(*float x, *union<int, number> y) {
    return (x + y,);
}
auto sub c125(*float x, list<int> y) {
    return (x + y,);
}
auto sub c126(*float x, hash<auto> y) {
    return (x + y,);
}
auto sub c127(*float x, binary y) {
    return (x + y,);
}
auto sub c128(*float x, C y) {
    return (x + y,);
}
auto sub c129(*number x, int y) {
    return (x + y,);
}
auto sub c130(*number x, float y) {
    return (x + y,);
}
auto sub c131(*number x, number y) {
    return (x + y,);
}
auto sub c132(*number x, string y) {
    return (x + y,);
}
auto sub c133(*number x, bool y) {
    return (x + y,);
}
auto sub c134(*number x, date y) {
    return (x + y,);
}
auto sub c135(*number x, *int y) {
    return (x + y,);
}
auto sub c136(*number x, *float y) {
    return (x + y,);
}
auto sub c137(*number x, *number y) {
    return (x + y,);
}
auto sub c138(*number x, union<int, float> y) {
    return (x + y,);
}
auto sub c139(*number x, union<int, string> y) {
    return (x + y,);
}
auto sub c140(*number x, *union<int, number> y) {
    return (x + y,);
}
auto sub c141(*number x, list<int> y) {
    return (x + y,);
}
auto sub c142(*number x, hash<auto> y) {
    return (x + y,);
}
auto sub c143(*number x, binary y) {
    return (x + y,);
}
auto sub c144(*number x, C y) {
    return (x + y,);
}
auto sub c145(union<int, float> x, int y) {
    return (x + y,);
}
auto sub c146(union<int, float> x, float y) {
    return (x + y,);
}
auto sub c147(union<int, float> x, number y) {
    return (x + y,);
}
auto sub c148(union<int, float> x, string y) {
    return (x + y,);
}
auto sub c149(union<int, float> x, bool y) {
    return (x + y,);
}
auto sub c150(union<int, float> x, date y) {
    return (x + y,);
}
auto sub c151(union<int, float> x, *int y) {
    return (x + y,);
}
auto sub c152(union<int, float> x, *float y) {
    return (x + y,);
}
auto sub c153(union<int, float> x, *number y) {
    return (x + y,);
}
auto sub c154(union<int, float> x, union<int, float> y) {
    return (x + y,);
}
auto sub c155(union<int, float> x, union<int, string> y) {
    return (x + y,);
}
auto sub c156(union<int, float> x, *union<int, number> y) {
    return (x + y,);
}
auto sub c157(union<int, float> x, list<int> y) {
    return (x + y,);
}
auto sub c158(union<int, float> x, hash<auto> y) {
    return (x + y,);
}
auto sub c159(union<int, float> x, binary y) {
    return (x + y,);
}
auto sub c160(union<int, float> x, C y) {
    return (x + y,);
}
auto sub c161(union<int, string> x, int y) {
    return (x + y,);
}
auto sub c162(union<int, string> x, float y) {
    return (x + y,);
}
auto sub c163(union<int, string> x, number y) {
    return (x + y,);
}
auto sub c164(union<int, string> x, string y) {
    return (x + y,);
}
auto sub c165(union<int, string> x, bool y) {
    return (x + y,);
}
auto sub c166(union<int, string> x, date y) {
    return (x + y,);
}
auto sub c167(union<int, string> x, *int y) {
    return (x + y,);
}
auto sub c168(union<int, string> x, *float y) {
    return (x + y,);
}
auto sub c169(union<int, string> x, *number y) {
    return (x + y,);
}
auto sub c170(union<int, string> x, union<int, float> y) {
    return (x + y,);
}
auto sub c171(union<int, string> x, union<int, string> y) {
    return (x + y,);
}
auto sub c172(union<int, string> x, *union<int, number> y) {
    return (x + y,);
}
auto sub c173(union<int, string> x, list<int> y) {
    return (x + y,);
}
auto sub c174(union<int, string> x, hash<auto> y) {
    return (x + y,);
}
auto sub c175(union<int, string> x, binary y) {
    return (x + y,);
}
auto sub c176(union<int, string> x, C y) {
    return (x + y,);
}
auto sub c177(*union<int, number> x, int y) {
    return (x + y,);
}
auto sub c178(*union<int, number> x, float y) {
    return (x + y,);
}
auto sub c179(*union<int, number> x, number y) {
    return (x + y,);
}
auto sub c180(*union<int, number> x, string y) {
    return (x + y,);
}
auto sub c181(*union<int, number> x, bool y) {
    return (x + y,);
}
auto sub c182(*union<int, number> x, date y) {
    return (x + y,);
}
auto sub c183(*union<int, number> x, *int y) {
    return (x + y,);
}
auto sub c184(*union<int, number> x, *float y) {
    return (x + y,);
}
auto sub c185(*union<int, number> x, *number y) {
    return (x + y,);
}
auto sub c186(*union<int, number> x, union<int, float> y) {
    return (x + y,);
}
auto sub c187(*union<int, number> x, union<int, string> y) {
    return (x + y,);
}
auto sub c188(*union<int, number> x, *union<int, number> y) {
    return (x + y,);
}
auto sub c189(*union<int, number> x, list<int> y) {
    return (x + y,);
}
auto sub c190(*union<int, number> x, hash<auto> y) {
    return (x + y,);
}
auto sub c191(*union<int, number> x, binary y) {
    return (x + y,);
}
auto sub c192(*union<int, number> x, C y) {
    return (x + y,);
}
auto sub c193(list<int> x, int y) {
    return (x + y,);
}
auto sub c194(list<int> x, float y) {
    return (x + y,);
}
auto sub c195(list<int> x, number y) {
    return (x + y,);
}
auto sub c196(list<int> x, string y) {
    return (x + y,);
}
auto sub c197(list<int> x, bool y) {
    return (x + y,);
}
auto sub c198(list<int> x, date y) {
    return (x + y,);
}
auto sub c199(list<int> x, *int y) {
    return (x + y,);
}
auto sub c200(list<int> x, *float y) {
    return (x + y,);
}
auto sub c201(list<int> x, *number y) {
    return (x + y,);
}
auto sub c202(list<int> x, union<int, float> y) {
    return (x + y,);
}
auto sub c203(list<int> x, union<int, string> y) {
    return (x + y,);
}
auto sub c204(list<int> x, *union<int, number> y) {
    return (x + y,);
}
auto sub c205(list<int> x, list<int> y) {
    return (x + y,);
}
auto sub c206(list<int> x, hash<auto> y) {
    return (x + y,);
}
auto sub c207(list<int> x, binary y) {
    return (x + y,);
}
auto sub c208(list<int> x, C y) {
    return (x + y,);
}
auto sub c209(hash<auto> x, int y) {
    return (x + y,);
}
auto sub c210(hash<auto> x, float y) {
    return (x + y,);
}
auto sub c211(hash<auto> x, number y) {
    return (x + y,);
}
auto sub c212(hash<auto> x, string y) {
    return (x + y,);
}
auto sub c213(hash<auto> x, bool y) {
    return (x + y,);
}
auto sub c214(hash<auto> x, date y) {
    return (x + y,);
}
auto sub c215(hash<auto> x, *int y) {
    return (x + y,);
}
auto sub c216(hash<auto> x, *float y) {
    return (x + y,);
}
auto sub c217(hash<auto> x, *number y) {
    return (x + y,);
}
auto sub c218(hash<auto> x, union<int, float> y) {
    return (x + y,);
}
auto sub c219(hash<auto> x, union<int, string> y) {
    return (x + y,);
}
auto sub c220(hash<auto> x, *union<int, number> y) {
    return (x + y,);
}
auto sub c221(hash<auto> x, list<int> y) {
    return (x + y,);
}
auto sub c222(hash<auto> x, hash<auto> y) {
    return (x + y,);
}
auto sub c223(hash<auto> x, binary y) {
    return (x + y,);
}
auto sub c224(hash<auto> x, C y) {
    return (x + y,);
}
auto sub c225(binary x, int y) {
    return (x + y,);
}
auto sub c226(binary x, float y) {
    return (x + y,);
}
auto sub c227(binary x, number y) {
    return (x + y,);
}
auto sub c228(binary x, string y) {
    return (x + y,);
}
auto sub c229(binary x, bool y) {
    return (x + y,);
}
auto sub c230(binary x, date y) {
    return (x + y,);
}
auto sub c231(binary x, *int y) {
    return (x + y,);
}
auto sub c232(binary x, *float y) {
    return (x + y,);
}
auto sub c233(binary x, *number y) {
    return (x + y,);
}
auto sub c234(binary x, union<int, float> y) {
    return (x + y,);
}
auto sub c235(binary x, union<int, string> y) {
    return (x + y,);
}
auto sub c236(binary x, *union<int, number> y) {
    return (x + y,);
}
auto sub c237(binary x, list<int> y) {
    return (x + y,);
}
auto sub c238(binary x, hash<auto> y) {
    return (x + y,);
}
auto sub c239(binary x, binary y) {
    return (x + y,);
}
auto sub c240(binary x, C y) {
    return (x + y,);
}
auto sub c241(C x, int y) {
    return (x + y,);
}
auto sub c242(C x, float y) {
    return (x + y,);
}
auto sub c243(C x, number y) {
    return (x + y,);
}
auto sub c244(C x, string y) {
    return (x + y,);
}
auto sub c245(C x, bool y) {
    return (x + y,);
}
auto sub c246(C x, date y) {
    return (x + y,);
}
auto sub c247(C x, *int y) {
    return (x + y,);
}
auto sub c248(C x, *float y) {
    return (x + y,);
}
auto sub c249(C x, *number y) {
    return (x + y,);
}
auto sub c250(C x, union<int, float> y) {
    return (x + y,);
}
auto sub c251(C x, union<int, string> y) {
    return (x + y,);
}
auto sub c252(C x, *union<int, number> y) {
    return (x + y,);
}
auto sub c253(C x, list<int> y) {
    return (x + y,);
}
auto sub c254(C x, hash<auto> y) {
    return (x + y,);
}
auto sub c255(C x, binary y) {
    return (x + y,);
}
auto sub c256(C x, C y) {
    return (x + y,);
}
auto sub c257(int x, int y) {
    return (x - y,);
}
auto sub c258(int x, float y) {
    return (x - y,);
}
auto sub c259(int x, number y) {
    return (x - y,);
}
auto sub c260(int x, string y) {
    return (x - y,);
}
auto sub c261(int x, bool y) {
    return (x - y,);
}
auto sub c262(int x, date y) {
    return (x - y,);
}
auto sub c263(int x, *int y) {
    return (x - y,);
}
auto sub c264(int x, *float y) {
    return (x - y,);
}
auto sub c265(int x, *number y) {
    return (x - y,);
}
auto sub c266(int x, union<int, float> y) {
    return (x - y,);
}
auto sub c267(int x, union<int, string> y) {
    return (x - y,);
}
auto sub c268(int x, *union<int, number> y) {
    return (x - y,);
}
auto sub c269(int x, list<int> y) {
    return (x - y,);
}
auto sub c270(int x, hash<auto> y) {
    return (x - y,);
}
auto sub c271(int x, binary y) {
    return (x - y,);
}
auto sub c272(int x, C y) {
    return (x - y,);
}
auto sub c273(float x, int y) {
    return (x - y,);
}
auto sub c274(float x, float y) {
    return (x - y,);
}
auto sub c275(float x, number y) {
    return (x - y,);
}
auto sub c276(float x, string y) {
    return (x - y,);
}
auto sub c277(float x, bool y) {
    return (x - y,);
}
auto sub c278(float x, date y) {
    return (x - y,);
}
auto sub c279(float x, *int y) {
    return (x - y,);
}
auto sub c280(float x, *float y) {
    return (x - y,);
}
auto sub c281(float x, *number y) {
    return (x - y,);
}
auto sub c282(float x, union<int, float> y) {
    return (x - y,);
}
auto sub c283(float x, union<int, string> y) {
    return (x - y,);
}
auto sub c284(float x, *union<int, number> y) {
    return (x - y,);
}
auto sub c285(float x, list<int> y) {
    return (x - y,);
}
auto sub c286(float x, hash<auto> y) {
    return (x - y,);
}
auto sub c287(float x, binary y) {
    return (x - y,);
}
auto sub c288(float x, C y) {
    return (x - y,);
}
auto sub c289(number x, int y) {
    return (x - y,);
}
auto sub c290(number x, float y) {
    return (x - y,);
}
auto sub c291(number x, number y) {
    return (x - y,);
}
auto sub c292(number x, string y) {
    return (x - y,);
}
auto sub c293(number x, bool y) {
    return (x - y,);
}
auto sub c294(number x, date y) {
    return (x - y,);
}
auto sub c295(number x, *int y) {
    return (x - y,);
}
auto sub c296(number x, *float y) {
    return (x - y,);
}
auto sub c297(number x, *number y) {
    return (x - y,);
}
auto sub c298(number x, union<int, float> y) {
    return (x - y,);
}
auto sub c299(number x, union<int, string> y) {
    return (x - y,);
}
auto sub c300(number x, *union<int, number> y) {
    return (x - y,);
}
auto sub c301(number x, list<int> y) {
    return (x - y,);
}
auto sub c302(number x, hash<auto> y) {
    return (x - y,);
}
auto sub c303(number x, binary y) {
    return (x - y,);
}
auto sub c304(number x, C y) {
    return (x - y,);
}
auto sub c305(string x, int y) {
    return (x - y,);
}
auto sub c306(string x, float y) {
    return (x - y,);
}
auto sub c307(string x, number y) {
    return (x - y,);
}
auto sub c308(string x, string y) {
    return (x - y,);
}
auto sub c309(string x, bool y) {
    return (x - y,);
}
auto sub c310(string x, date y) {
    return (x - y,);
}
auto sub c311(string x, *int y) {
    return (x - y,);
}
auto sub c312(string x, *float y) {
    return (x - y,);
}
auto sub c313(string x, *number y) {
    return (x - y,);
}
auto sub c314(string x, union<int, float> y) {
    return (x - y,);
}
auto sub c315(string x, union<int, string> y) {
    return (x - y,);
}
auto sub c316(string x, *union<int, number> y) {
    return (x - y,);
}
auto sub c317(string x, list<int> y) {
    return (x - y,);
}
auto sub c318(string x, hash<auto> y) {
    return (x - y,);
}
auto sub c319(string x, binary y) {
    return (x - y,);
}
auto sub c320(string x, C y) {
    return (x - y,);
}
auto sub c321(bool x, int y) {
    return (x - y,);
}
auto sub c322(bool x, float y) {
    return (x - y,);
}
auto sub c323(bool x, number y) {
    return (x - y,);
}
auto sub c324(bool x, string y) {
    return (x - y,);
}
auto sub c325(bool x, bool y) {
    return (x - y,);
}
auto sub c326(bool x, date y) {
    return (x - y,);
}
auto sub c327(bool x, *int y) {
    return (x - y,);
}
auto sub c328(bool x, *float y) {
    return (x - y,);
}
auto sub c329(bool x, *number y) {
    return (x - y,);
}
auto sub c330(bool x, union<int, float> y) {
    return (x - y,);
}
auto sub c331(bool x, union<int, string> y) {
    return (x - y,);
}
auto sub c332(bool x, *union<int, number> y) {
    return (x - y,);
}
auto sub c333(bool x, list<int> y) {
    return (x - y,);
}
auto sub c334(bool x, hash<auto> y) {
    return (x - y,);
}
auto sub c335(bool x, binary y) {
    return (x - y,);
}
auto sub c336(bool x, C y) {
    return (x - y,);
}
auto sub c337(date x, int y) {
    return (x - y,);
}
auto sub c338(date x, float y) {
    return (x - y,);
}
auto sub c339(date x, number y) {
    return (x - y,);
}
auto sub c340(date x, string y) {
    return (x - y,);
}
auto sub c341(date x, bool y) {
    return (x - y,);
}
auto sub c342(date x, date y) {
    return (x - y,);
}
auto sub c343(date x, *int y) {
    return (x - y,);
}
auto sub c344(date x, *float y) {
    return (x - y,);
}
auto sub c345(date x, *number y) {
    return (x - y,);
}
auto sub c346(date x, union<int, float> y) {
    return (x - y,);
}
auto sub c347(date x, union<int, string> y) {
    return (x - y,);
}
auto sub c348(date x, *union<int, number> y) {
    return (x - y,);
}
auto sub c349(date x, list<int> y) {
    return (x - y,);
}
auto sub c350(date x, hash<auto> y) {
    return (x - y,);
}
auto sub c351(date x, binary y) {
    return (x - y,);
}
auto sub c352(date x, C y) {
    return (x - y,);
}
auto sub c353(*int x, int y) {
    return (x - y,);
}
auto sub c354(*int x, float y) {
    return (x - y,);
}
auto sub c355(*int x, number y) {
    return (x - y,);
}
auto sub c356(*int x, string y) {
    return (x - y,);
}
auto sub c357(*int x, bool y) {
    return (x - y,);
}
auto sub c358(*int x, date y) {
    return (x - y,);
}
auto sub c359(*int x, *int y) {
    return (x - y,);
}
auto sub c360(*int x, *float y) {
    return (x - y,);
}
auto sub c361(*int x, *number y) {
    return (x - y,);
}
auto sub c362(*int x, union<int, float> y) {
    return (x - y,);
}
auto sub c363(*int x, union<int, string> y) {
    return (x - y,);
}
auto sub c364(*int x, *union<int, number> y) {
    return (x - y,);
}
auto sub c365(*int x, list<int> y) {
    return (x - y,);
}
auto sub c366(*int x, hash<auto> y) {
    return (x - y,);
}
auto sub c367(*int x, binary y) {
    return (x - y,);
}
auto sub c368(*int x, C y) {
    return (x - y,);
}
auto sub c369(*float x, int y) {
    return (x - y,);
}
auto sub c370(*float x, float y) {
    return (x - y,);
}
auto sub c371(*float x, number y) {
    return (x - y,);
}
auto sub c372(*float x, string y) {
    return (x - y,);
}
auto sub c373(*float x, bool y) {
    return (x - y,);
}
auto sub c374(*float x, date y) {
    return (x - y,);
}
auto sub c375(*float x, *int y) {
    return (x - y,);
}
auto sub c376(*float x, *float y) {
    return (x - y,);
}
auto sub c377(*float x, *number y) {
    return (x - y,);
}
auto sub c378(*float x, union<int, float> y) {
    return (x - y,);
}
auto sub c379(*float x, union<int, string> y) {
    return (x - y,);
}
auto sub c380(*float x, *union<int, number> y) {
    return (x - y,);
}
auto sub c381(*float x, list<int> y) {
    return (x - y,);
}
auto sub c382(*float x, hash<auto> y) {
    return (x - y,);
}
auto sub c383(*float x, binary y) {
    return (x - y,);
}
auto sub c384(*float x, C y) {
    return (x - y,);
}
auto sub c385(*number x, int y) {
    return (x - y,);
}
auto sub c386(*number x, float y) {
    return (x - y,);
}
auto sub c387(*number x, number y) {
    return (x - y,);
}
auto sub c388(*number x, string y) {
    return (x - y,);
}
auto sub c389(*number x, bool y) {
    return (x - y,);
}
auto sub c390(*number x, date y) {
    return (x - y,);
}
auto sub c391(*number x, *int y) {
    return (x - y,);
}
auto sub c392(*number x, *float y) {
    return (x - y,);
}
auto sub c393(*number x, *number y) {
    return (x - y,);
}
auto sub c394(*number x, union<int, float> y) {
    return (x - y,);
}
auto sub c395(*number x, union<int, string> y) {
    return (x - y,);
}
auto sub c396(*number x, *union<int, number> y) {
    return (x - y,);
}
auto sub c397(*number x, list<int> y) {
    return (x - y,);
}
auto sub c398(*number x, hash<auto> y) {
    return (x - y,);
}
auto sub c399(*number x, binary y) {
    return (x - y,);
}
auto sub c400(*number x, C y) {
    return (x - y,);
}
auto sub c401(union<int, float> x, int y) {
    return (x - y,);
}
auto sub c402(union<int, float> x, float y) {
    return (x - y,);
}
auto sub c403(union<int, float> x, number y) {
    return (x - y,);
}
auto sub c404(union<int, float> x, string y) {
    return (x - y,);
}
auto sub c405(union<int, float> x, bool y) {
    return (x - y,);
}
auto sub c406(union<int, float> x, date y) {
    return (x - y,);
}
auto sub c407(union<int, float> x, *int y) {
    return (x - y,);
}
auto sub c408(union<int, float> x, *float y) {
    return (x - y,);
}
auto sub c409(union<int, float> x, *number y) {
    return (x - y,);
}
auto sub c410(union<int, float> x, union<int, float> y) {
    return (x - y,);
}
auto sub c411(union<int, float> x, union<int, string> y) {
    return (x - y,);
}
auto sub c412(union<int, float> x, *union<int, number> y) {
    return (x - y,);
}
auto sub c413(union<int, float> x, list<int> y) {
    return (x - y,);
}
auto sub c414(union<int, float> x, hash<auto> y) {
    return (x - y,);
}
auto sub c415(union<int, float> x, binary y) {
    return (x - y,);
}
auto sub c416(union<int, float> x, C y) {
    return (x - y,);
}
auto sub c417(union<int, string> x, int y) {
    return (x - y,);
}
auto sub c418(union<int, string> x, float y) {
    return (x - y,);
}
auto sub c419(union<int, string> x, number y) {
    return (x - y,);
}
auto sub c420(union<int, string> x, string y) {
    return (x - y,);
}
auto sub c421(union<int, string> x, bool y) {
    return (x - y,);
}
auto sub c422(union<int, string> x, date y) {
    return (x - y,);
}
auto sub c423(union<int, string> x, *int y) {
    return (x - y,);
}
auto sub c424(union<int, string> x, *float y) {
    return (x - y,);
}
auto sub c425(union<int, string> x, *number y) {
    return (x - y,);
}
auto sub c426(union<int, string> x, union<int, float> y) {
    return (x - y,);
}
auto sub c427(union<int, string> x, union<int, string> y) {
    return (x - y,);
}
auto sub c428(union<int, string> x, *union<int, number> y) {
    return (x - y,);
}
auto sub c429(union<int, string> x, list<int> y) {
    return (x - y,);
}
auto sub c430(union<int, string> x, hash<auto> y) {
    return (x - y,);
}
auto sub c431(union<int, string> x, binary y) {
    return (x - y,);
}
auto sub c432(union<int, string> x, C y) {
    return (x - y,);
}
auto sub c433(*union<int, number> x, int y) {
    return (x - y,);
}
auto sub c434(*union<int, number> x, float y) {
    return (x - y,);
}
auto sub c435(*union<int, number> x, number y) {
    return (x - y,);
}
auto sub c436(*union<int, number> x, string y) {
    return (x - y,);
}
auto sub c437(*union<int, number> x, bool y) {
    return (x - y,);
}
auto sub c438(*union<int, number> x, date y) {
    return (x - y,);
}
auto sub c439(*union<int, number> x, *int y) {
    return (x - y,);
}
auto sub c440(*union<int, number> x, *float y) {
    return (x - y,);
}
auto sub c441(*union<int, number> x, *number y) {
    return (x - y,);
}
auto sub c442(*union<int, number> x, union<int, float> y) {
    return (x - y,);
}
auto sub c443(*union<int, number> x, union<int, string> y) {
    return (x - y,);
}
auto sub c444(*union<int, number> x, *union<int, number> y) {
    return (x - y,);
}
auto sub c445(*union<int, number> x, list<int> y) {
    return (x - y,);
}
auto sub c446(*union<int, number> x, hash<auto> y) {
    return (x - y,);
}
auto sub c447(*union<int, number> x, binary y) {
    return (x - y,);
}
auto sub c448(*union<int, number> x, C y) {
    return (x - y,);
}
auto sub c449(list<int> x, int y) {
    return (x - y,);
}
auto sub c450(list<int> x, float y) {
    return (x - y,);
}
auto sub c451(list<int> x, number y) {
    return (x - y,);
}
auto sub c452(list<int> x, string y) {
    return (x - y,);
}
auto sub c453(list<int> x, bool y) {
    return (x - y,);
}
auto sub c454(list<int> x, date y) {
    return (x - y,);
}
auto sub c455(list<int> x, *int y) {
    return (x - y,);
}
auto sub c456(list<int> x, *float y) {
    return (x - y,);
}
auto sub c457(list<int> x, *number y) {
    return (x - y,);
}
auto sub c458(list<int> x, union<int, float> y) {
    return (x - y,);
}
auto sub c459(list<int> x, union<int, string> y) {
    return (x - y,);
}
auto sub c460(list<int> x, *union<int, number> y) {
    return (x - y,);
}
auto sub c461(list<int> x, list<int> y) {
    return (x - y,);
}
auto sub c462(list<int> x, hash<auto> y) {
    return (x - y,);
}
auto sub c463(list<int> x, binary y) {
    return (x - y,);
}
auto sub c464(list<int> x, C y) {
    return (x - y,);
}
auto sub c465(hash<auto> x, int y) {
    return (x - y,);
}
auto sub c466(hash<auto> x, float y) {
    return (x - y,);
}
auto sub c467(hash<auto> x, number y) {
    return (x - y,);
}
auto sub c468(hash<auto> x, string y) {
    return (x - y,);
}
auto sub c469(hash<auto> x, bool y) {
    return (x - y,);
}
auto sub c470(hash<auto> x, date y) {
    return (x - y,);
}
auto sub c471(hash<auto> x, *int y) {
    return (x - y,);
}
auto sub c472(hash<auto> x, *float y) {
    return (x - y,);
}
auto sub c473(hash<auto> x, *number y) {
    return (x - y,);
}
auto sub c474(hash<auto> x, union<int, float> y) {
    return (x - y,);
}
auto sub c475(hash<auto> x, union<int, string> y) {
    return (x - y,);
}
auto sub c476(hash<auto> x, *union<int, number> y) {
    return (x - y,);
}
auto sub c477(hash<auto> x, list<int> y) {
    return (x - y,);
}
auto sub c478(hash<auto> x, hash<auto> y) {
    return (x - y,);
}
auto sub c479(hash<auto> x, binary y) {
    return (x - y,);
}
auto sub c480(hash<auto> x, C y) {
    return (x - y,);
}
auto sub c481(binary x, int y) {
    return (x - y,);
}
auto sub c482(binary x, float y) {
    return (x - y,);
}
auto sub c483(binary x, number y) {
    return (x - y,);
}
auto sub c484(binary x, string y) {
    return (x - y,);
}
auto sub c485(binary x, bool y) {
    return (x - y,);
}
auto sub c486(binary x, date y) {
    return (x - y,);
}
auto sub c487(binary x, *int y) {
    return (x - y,);
}
auto sub c488(binary x, *float y) {
    return (x - y,);
}
auto sub c489(binary x, *number y) {
    return (x - y,);
}
auto sub c490(binary x, union<int, float> y) {
    return (x - y,);
}
auto sub c491(binary x, union<int, string> y) {
    return (x - y,);
}
auto sub c492(binary x, *union<int, number> y) {
    return (x - y,);
}
auto sub c493(binary x, list<int> y) {
    return (x - y,);
}
auto sub c494(binary x, hash<auto> y) {
    return (x - y,);
}
auto sub c495(binary x, binary y) {
    return (x - y,);
}
auto sub c496(binary x, C y) {
    return (x - y,);
}
auto sub c497(C x, int y) {
    return (x - y,);
}
auto sub c498(C x, float y) {
    return (x - y,);
}
auto sub c499(C x, number y) {
    return (x - y,);
}
auto sub c500(C x, string y) {
    return (x - y,);
}
auto sub c501(C x, bool y) {
    return (x - y,);
}
auto sub c502(C x, date y) {
    return (x - y,);
}
auto sub c503(C x, *int y) {
    return (x - y,);
}
auto sub c504(C x, *float y) {
    return (x - y,);
}
auto sub c505(C x, *number y) {
    return (x - y,);
}
auto sub c506(C x, union<int, float> y) {
    return (x - y,);
}
auto sub c507(C x, union<int, string> y) {
    return (x - y,);
}
auto sub c508(C x, *union<int, number> y) {
    return (x - y,);
}
auto sub c509(C x, list<int> y) {
    return (x - y,);
}
auto sub c510(C x, hash<auto> y) {
    return (x - y,);
}
auto sub c511(C x, binary y) {
    return (x - y,);
}
auto sub c512(C x, C y) {
    return (x - y,);
}
auto sub c513(int x, int y) {
    return (x * y,);
}
auto sub c514(int x, float y) {
    return (x * y,);
}
auto sub c515(int x, number y) {
    return (x * y,);
}
auto sub c516(int x, string y) {
    return (x * y,);
}
auto sub c517(int x, bool y) {
    return (x * y,);
}
auto sub c518(int x, date y) {
    return (x * y,);
}
auto sub c519(int x, *int y) {
    return (x * y,);
}
auto sub c520(int x, *float y) {
    return (x * y,);
}
auto sub c521(int x, *number y) {
    return (x * y,);
}
auto sub c522(int x, union<int, float> y) {
    return (x * y,);
}
auto sub c523(int x, union<int, string> y) {
    return (x * y,);
}
auto sub c524(int x, *union<int, number> y) {
    return (x * y,);
}
auto sub c525(int x, list<int> y) {
    return (x * y,);
}
auto sub c526(int x, hash<auto> y) {
    return (x * y,);
}
auto sub c527(int x, binary y) {
    return (x * y,);
}
auto sub c528(int x, C y) {
    return (x * y,);
}
auto sub c529(float x, int y) {
    return (x * y,);
}
auto sub c530(float x, float y) {
    return (x * y,);
}
auto sub c531(float x, number y) {
    return (x * y,);
}
auto sub c532(float x, string y) {
    return (x * y,);
}
auto sub c533(float x, bool y) {
    return (x * y,);
}
auto sub c534(float x, date y) {
    return (x * y,);
}
auto sub c535(float x, *int y) {
    return (x * y,);
}
auto sub c536(float x, *float y) {
    return (x * y,);
}
auto sub c537(float x, *number y) {
    return (x * y,);
}
auto sub c538(float x, union<int, float> y) {
    return (x * y,);
}
auto sub c539(float x, union<int, string> y) {
    return (x * y,);
}
auto sub c540(float x, *union<int, number> y) {
    return (x * y,);
}
auto sub c541(float x, list<int> y) {
    return (x * y,);
}
auto sub c542(float x, hash<auto> y) {
    return (x * y,);
}
auto sub c543(float x, binary y) {
    return (x * y,);
}
auto sub c544(float x, C y) {
    return (x * y,);
}
auto sub c545(number x, int y) {
    return (x * y,);
}
auto sub c546(number x, float y) {
    return (x * y,);
}
auto sub c547(number x, number y) {
    return (x * y,);
}
auto sub c548(number x, string y) {
    return (x * y,);
}
auto sub c549(number x, bool y) {
    return (x * y,);
}
auto sub c550(number x, date y) {
    return (x * y,);
}
auto sub c551(number x, *int y) {
    return (x * y,);
}
auto sub c552(number x, *float y) {
    return (x * y,);
}
auto sub c553(number x, *number y) {
    return (x * y,);
}
auto sub c554(number x, union<int, float> y) {
    return (x * y,);
}
auto sub c555(number x, union<int, string> y) {
    return (x * y,);
}
auto sub c556(number x, *union<int, number> y) {
    return (x * y,);
}
auto sub c557(number x, list<int> y) {
    return (x * y,);
}
auto sub c558(number x, hash<auto> y) {
    return (x * y,);
}
auto sub c559(number x, binary y) {
    return (x * y,);
}
auto sub c560(number x, C y) {
    return (x * y,);
}
auto sub c561(string x, int y) {
    return (x * y,);
}
auto sub c562(string x, float y) {
    return (x * y,);
}
auto sub c563(string x, number y) {
    return (x * y,);
}
auto sub c564(string x, string y) {
    return (x * y,);
}
auto sub c565(string x, bool y) {
    return (x * y,);
}
auto sub c566(string x, date y) {
    return (x * y,);
}
auto sub c567(string x, *int y) {
    return (x * y,);
}
auto sub c568(string x, *float y) {
    return (x * y,);
}
auto sub c569(string x, *number y) {
    return (x * y,);
}
auto sub c570(string x, union<int, float> y) {
    return (x * y,);
}
auto sub c571(string x, union<int, string> y) {
    return (x * y,);
}
auto sub c572(string x, *union<int, number> y) {
    return (x * y,);
}
auto sub c573(string x, list<int> y) {
    return (x * y,);
}
auto sub c574(string x, hash<auto> y) {
    return (x * y,);
}
auto sub c575(string x, binary y) {
    return (x * y,);
}
auto sub c576(string x, C y) {
    return (x * y,);
}
auto sub c577(bool x, int y) {
    return (x * y,);
}
auto sub c578(bool x, float y) {
    return (x * y,);
}
auto sub c579(bool x, number y) {
    return (x * y,);
}
auto sub c580(bool x, string y) {
    return (x * y,);
}
auto sub c581(bool x, bool y) {
    return (x * y,);
}
auto sub c582(bool x, date y) {
    return (x * y,);
}
auto sub c583(bool x, *int y) {
    return (x * y,);
}
auto sub c584(bool x, *float y) {
    return (x * y,);
}
auto sub c585(bool x, *number y) {
    return (x * y,);
}
auto sub c586(bool x, union<int, float> y) {
    return (x * y,);
}
auto sub c587(bool x, union<int, string> y) {
    return (x * y,);
}
auto sub c588(bool x, *union<int, number> y) {
    return (x * y,);
}
auto sub c589(bool x, list<int> y) {
    return (x * y,);
}
auto sub c590(bool x, hash<auto> y) {
    return (x * y,);
}
auto sub c591(bool x, binary y) {
    return (x * y,);
}
auto sub c592(bool x, C y) {
    return (x * y,);
}
auto sub c593(date x, int y) {
    return (x * y,);
}
auto sub c594(date x, float y) {
    return (x * y,);
}
auto sub c595(date x, number y) {
    return (x * y,);
}
auto sub c596(date x, string y) {
    return (x * y,);
}
auto sub c597(date x, bool y) {
    return (x * y,);
}
auto sub c598(date x, date y) {
    return (x * y,);
}
auto sub c599(date x, *int y) {
    return (x * y,);
}
auto sub c600(date x, *float y) {
    return (x * y,);
}
auto sub c601(date x, *number y) {
    return (x * y,);
}
auto sub c602(date x, union<int, float> y) {
    return (x * y,);
}
auto sub c603(date x, union<int, string> y) {
    return (x * y,);
}
auto sub c604(date x, *union<int, number> y) {
    return (x * y,);
}
auto sub c605(date x, list<int> y) {
    return (x * y,);
}
auto sub c606(date x, hash<auto> y) {
    return (x * y,);
}
auto sub c607(date x, binary y) {
    return (x * y,);
}
auto sub c608(date x, C y) {
    return (x * y,);
}
auto sub c609(*int x, int y) {
    return (x * y,);
}
auto sub c610(*int x, float y) {
    return (x * y,);
}
auto sub c611(*int x, number y) {
    return (x * y,);
}
auto sub c612(*int x, string y) {
    return (x * y,);
}
auto sub c613(*int x, bool y) {
    return (x * y,);
}
auto sub c614(*int x, date y) {
    return (x * y,);
}
auto sub c615(*int x, *int y) {
    return (x * y,);
}
auto sub c616(*int x, *float y) {
    return (x * y,);
}
auto sub c617(*int x, *number y) {
    return (x * y,);
}
auto sub c618(*int x, union<int, float> y) {
    return (x * y,);
}
auto sub c619(*int x, union<int, string> y) {
    return (x * y,);
}
auto sub c620(*int x, *union<int, number> y) {
    return (x * y,);
}
auto sub c621(*int x, list<int> y) {
    return (x * y,);
}
auto sub c622(*int x, hash<auto> y) {
    return (x * y,);
}
auto sub c623(*int x, binary y) {
    return (x * y,);
}
auto sub c624(*int x, C y) {
    return (x * y,);
}
auto sub c625(*float x, int y) {
    return (x * y,);
}
auto sub c626(*float x, float y) {
    return (x * y,);
}
auto sub c627(*float x, number y) {
    return (x * y,);
}
auto sub c628(*float x, string y) {
    return (x * y,);
}
auto sub c629(*float x, bool y) {
    return (x * y,);
}
auto sub c630(*float x, date y) {
    return (x * y,);
}
auto sub c631(*float x, *int y) {
    return (x * y,);
}
auto sub c632(*float x, *float y) {
    return (x * y,);
}
auto sub c633(*float x, *number y) {
    return (x * y,);
}
auto sub c634(*float x, union<int, float> y) {
    return (x * y,);
}
auto sub c635(*float x, union<int, string> y) {
    return (x * y,);
}
auto sub c636(*float x, *union<int, number> y) {
    return (x * y,);
}
auto sub c637(*float x, list<int> y) {
    return (x * y,);
}
auto sub c638(*float x, hash<auto> y) {
    return (x * y,);
}
auto sub c639(*float x, binary y) {
    return (x * y,);
}
auto sub c640(*float x, C y) {
    return (x * y,);
}
auto sub c641(*number x, int y) {
    return (x * y,);
}
auto sub c642(*number x, float y) {
    return (x * y,);
}
auto sub c643(*number x, number y) {
    return (x * y,);
}
auto sub c644(*number x, string y) {
    return (x * y,);
}
auto sub c645(*number x, bool y) {
    return (x * y,);
}
auto sub c646(*number x, date y) {
    return (x * y,);
}
auto sub c647(*number x, *int y) {
    return (x * y,);
}
auto sub c648(*number x, *float y) {
    return (x * y,);
}
auto sub c649(*number x, *number y) {
    return (x * y,);
}
auto sub c650(*number x, union<int, float> y) {
    return (x * y,);
}
auto sub c651(*number x, union<int, string> y) {
    return (x * y,);
}
auto sub c652(*number x, *union<int, number> y) {
    return (x * y,);
}
auto sub c653(*number x, list<int> y) {
    return (x * y,);
}
auto sub c654(*number x, hash<auto> y) {
    return (x * y,);
}
auto sub c655(*number x, binary y) {
    return (x * y,);
}
auto sub c656(*number x, C y) {
    return (x * y,);
}
auto sub c657(union<int, float> x, int y) {
    return (x * y,);
}
auto sub c658(union<int, float> x, float y) {
    return (x * y,);
}
auto sub c659(union<int, float> x, number y) {
    return (x * y,);
}
auto sub c660(union<int, float> x, string y) {
    return (x * y,);
}
auto sub c661(union<int, float> x, bool y) {
    return (x * y,);
}
auto sub c662(union<int, float> x, date y) {
    return (x * y,);
}
auto sub c663(union<int, float> x, *int y) {
    return (x * y,);
}
auto sub c664(union<int, float> x, *float y) {
    return (x * y,);
}
auto sub c665(union<int, float> x, *number y) {
    return (x * y,);
}
auto sub c666(union<int, float> x, union<int, float> y) {
    return (x * y,);
}
auto sub c667(union<int, float> x, union<int, string> y) {
    return (x * y,);
}
auto sub c668(union<int, float> x, *union<int, number> y) {
    return (x * y,);
}
auto sub c669(union<int, float> x, list<int> y) {
    return (x * y,);
}
auto sub c670(union<int, float> x, hash<auto> y) {
    return (x * y,);
}
auto sub c671(union<int, float> x, binary y) {
    return (x * y,);
}
auto sub c672(union<int, float> x, C y) {
    return (x * y,);
}
auto sub c673(union<int, string> x, int y) {
    return (x * y,);
}
auto sub c674(union<int, string> x, float y) {
    return (x * y,);
}
auto sub c675(union<int, string> x, number y) {
    return (x * y,);
}
auto sub c676(union<int, string> x, string y) {
    return (x * y,);
}
auto sub c677(union<int, string> x, bool y) {
    return (x * y,);
}
auto sub c678(union<int, string> x, date y) {
    return (x * y,);
}
auto sub c679(union<int, string> x, *int y) {
    return (x * y,);
}
auto sub c680(union<int, string> x, *float y) {
    return (x * y,);
}
auto sub c681(union<int, string> x, *number y) {
    return (x * y,);
}
auto sub c682(union<int, string> x, union<int, float> y) {
    return (x * y,);
}
auto sub c683(union<int, string> x, union<int, string> y) {
    return (x * y,);
}
auto sub c684(union<int, string> x, *union<int, number> y) {
    return (x * y,);
}
auto sub c685(union<int, string> x, list<int> y) {
    return (x * y,);
}
auto sub c686(union<int, string> x, hash<auto> y) {
    return (x * y,);
}
auto sub c687(union<int, string> x, binary y) {
    return (x * y,);
}
auto sub c688(union<int, string> x, C y) {
    return (x * y,);
}
auto sub c689(*union<int, number> x, int y) {
    return (x * y,);
}
auto sub c690(*union<int, number> x, float y) {
    return (x * y,);
}
auto sub c691(*union<int, number> x, number y) {
    return (x * y,);
}
auto sub c692(*union<int, number> x, string y) {
    return (x * y,);
}
auto sub c693(*union<int, number> x, bool y) {
    return (x * y,);
}
auto sub c694(*union<int, number> x, date y) {
    return (x * y,);
}
auto sub c695(*union<int, number> x, *int y) {
    return (x * y,);
}
auto sub c696(*union<int, number> x, *float y) {
    return (x * y,);
}
auto sub c697(*union<int, number> x, *number y) {
    return (x * y,);
}
auto sub c698(*union<int, number> x, union<int, float> y) {
    return (x * y,);
}
auto sub c699(*union<int, number> x, union<int, string> y) {
    return (x * y,);
}
auto sub c700(*union<int, number> x, *union<int, number> y) {
    return (x * y,);
}
auto sub c701(*union<int, number> x, list<int> y) {
    return (x * y,);
}
auto sub c702(*union<int, number> x, hash<auto> y) {
    return (x * y,);
}
auto sub c703(*union<int, number> x, binary y) {
    return (x * y,);
}
auto sub c704(*union<int, number> x, C y) {
    return (x * y,);
}
auto sub c705(list<int> x, int y) {
    return (x * y,);
}
auto sub c706(list<int> x, float y) {
    return (x * y,);
}
auto sub c707(list<int> x, number y) {
    return (x * y,);
}
auto sub c708(list<int> x, string y) {
    return (x * y,);
}
auto sub c709(list<int> x, bool y) {
    return (x * y,);
}
auto sub c710(list<int> x, date y) {
    return (x * y,);
}
auto sub c711(list<int> x, *int y) {
    return (x * y,);
}
auto sub c712(list<int> x, *float y) {
    return (x * y,);
}
auto sub c713(list<int> x, *number y) {
    return (x * y,);
}
auto sub c714(list<int> x, union<int, float> y) {
    return (x * y,);
}
auto sub c715(list<int> x, union<int, string> y) {
    return (x * y,);
}
auto sub c716(list<int> x, *union<int, number> y) {
    return (x * y,);
}
auto sub c717(list<int> x, list<int> y) {
    return (x * y,);
}
auto sub c718(list<int> x, hash<auto> y) {
    return (x * y,);
}
auto sub c719(list<int> x, binary y) {
    return (x * y,);
}
auto sub c720(list<int> x, C y) {
    return (x * y,);
}
auto sub c721(hash<auto> x, int y) {
    return (x * y,);
}
auto sub c722(hash<auto> x, float y) {
    return (x * y,);
}
auto sub c723(hash<auto> x, number y) {
    return (x * y,);
}
auto sub c724(hash<auto> x, string y) {
    return (x * y,);
}
auto sub c725(hash<auto> x, bool y) {
    return (x * y,);
}
auto sub c726(hash<auto> x, date y) {
    return (x * y,);
}
auto sub c727(hash<auto> x, *int y) {
    return (x * y,);
}
auto sub c728(hash<auto> x, *float y) {
    return (x * y,);
}
auto sub c729(hash<auto> x, *number y) {
    return (x * y,);
}
auto sub c730(hash<auto> x, union<int, float> y) {
    return (x * y,);
}
auto sub c731(hash<auto> x, union<int, string> y) {
    return (x * y,);
}
auto sub c732(hash<auto> x, *union<int, number> y) {
    return (x * y,);
}
auto sub c733(hash<auto> x, list<int> y) {
    return (x * y,);
}
auto sub c734(hash<auto> x, hash<auto> y) {
    return (x * y,);
}
auto sub c735(hash<auto> x, binary y) {
    return (x * y,);
}
auto sub c736(hash<auto> x, C y) {
    return (x * y,);
}
auto sub c737(binary x, int y) {
    return (x * y,);
}
auto sub c738(binary x, float y) {
    return (x * y,);
}
auto sub c739(binary x, number y) {
    return (x * y,);
}
auto sub c740(binary x, string y) {
    return (x * y,);
}
auto sub c741(binary x, bool y) {
    return (x * y,);
}
auto sub c742(binary x, date y) {
    return (x * y,);
}
auto sub c743(binary x, *int y) {
    return (x * y,);
}
auto sub c744(binary x, *float y) {
    return (x * y,);
}
auto sub c745(binary x, *number y) {
    return (x * y,);
}
auto sub c746(binary x, union<int, float> y) {
    return (x * y,);
}
auto sub c747(binary x, union<int, string> y) {
    return (x * y,);
}
auto sub c748(binary x, *union<int, number> y) {
    return (x * y,);
}
auto sub c749(binary x, list<int> y) {
    return (x * y,);
}
auto sub c750(binary x, hash<auto> y) {
    return (x * y,);
}
auto sub c751(binary x, binary y) {
    return (x * y,);
}
auto sub c752(binary x, C y) {
    return (x * y,);
}
auto sub c753(C x, int y) {
    return (x * y,);
}
auto sub c754(C x, float y) {
    return (x * y,);
}
auto sub c755(C x, number y) {
    return (x * y,);
}
auto sub c756(C x, string y) {
    return (x * y,);
}
auto sub c757(C x, bool y) {
    return (x * y,);
}
auto sub c758(C x, date y) {
    return (x * y,);
}
auto sub c759(C x, *int y) {
    return (x * y,);
}
auto sub c760(C x, *float y) {
    return (x * y,);
}
auto sub c761(C x, *number y) {
    return (x * y,);
}
auto sub c762(C x, union<int, float> y) {
    return (x * y,);
}
auto sub c763(C x, union<int, string> y) {
    return (x * y,);
}
auto sub c764(C x, *union<int, number> y) {
    return (x * y,);
}
auto sub c765(C x, list<int> y) {
    return (x * y,);
}
auto sub c766(C x, hash<auto> y) {
    return (x * y,);
}
auto sub c767(C x, binary y) {
    return (x * y,);
}
auto sub c768(C x, C y) {
    return (x * y,);
}
auto sub c769(int x, int y) {
    return (x / y,);
}
auto sub c770(int x, float y) {
    return (x / y,);
}
auto sub c771(int x, number y) {
    return (x / y,);
}
auto sub c772(int x, string y) {
    return (x / y,);
}
auto sub c773(int x, bool y) {
    return (x / y,);
}
auto sub c774(int x, date y) {
    return (x / y,);
}
auto sub c775(int x, *int y) {
    return (x / y,);
}
auto sub c776(int x, *float y) {
    return (x / y,);
}
auto sub c777(int x, *number y) {
    return (x / y,);
}
auto sub c778(int x, union<int, float> y) {
    return (x / y,);
}
auto sub c779(int x, union<int, string> y) {
    return (x / y,);
}
auto sub c780(int x, *union<int, number> y) {
    return (x / y,);
}
auto sub c781(int x, list<int> y) {
    return (x / y,);
}
auto sub c782(int x, hash<auto> y) {
    return (x / y,);
}
auto sub c783(int x, binary y) {
    return (x / y,);
}
auto sub c784(int x, C y) {
    return (x / y,);
}
auto sub c785(float x, int y) {
    return (x / y,);
}
auto sub c786(float x, float y) {
    return (x / y,);
}
auto sub c787(float x, number y) {
    return (x / y,);
}
auto sub c788(float x, string y) {
    return (x / y,);
}
auto sub c789(float x, bool y) {
    return (x / y,);
}
auto sub c790(float x, date y) {
    return (x / y,);
}
auto sub c791(float x, *int y) {
    return (x / y,);
}
auto sub c792(float x, *float y) {
    return (x / y,);
}
auto sub c793(float x, *number y) {
    return (x / y,);
}
auto sub c794(float x, union<int, float> y) {
    return (x / y,);
}
auto sub c795(float x, union<int, string> y) {
    return (x / y,);
}
auto sub c796(float x, *union<int, number> y) {
    return (x / y,);
}
auto sub c797(float x, list<int> y) {
    return (x / y,);
}
auto sub c798(float x, hash<auto> y) {
    return (x / y,);
}
auto sub c799(float x, binary y) {
    return (x / y,);
}
auto sub c800(float x, C y) {
    return (x / y,);
}
auto sub c801(number x, int y) {
    return (x / y,);
}
auto sub c802(number x, float y) {
    return (x / y,);
}
auto sub c803(number x, number y) {
    return (x / y,);
}
auto sub c804(number x, string y) {
    return (x / y,);
}
auto sub c805(number x, bool y) {
    return (x / y,);
}
auto sub c806(number x, date y) {
    return (x / y,);
}
auto sub c807(number x, *int y) {
    return (x / y,);
}
auto sub c808(number x, *float y) {
    return (x / y,);
}
auto sub c809(number x, *number y) {
    return (x / y,);
}
auto sub c810(number x, union<int, float> y) {
    return (x / y,);
}
auto sub c811(number x, union<int, string> y) {
    return (x / y,);
}
auto sub c812(number x, *union<int, number> y) {
    return (x / y,);
}
auto sub c813(number x, list<int> y) {
    return (x / y,);
}
auto sub c814(number x, hash<auto> y) {
    return (x / y,);
}
auto sub c815(number x, binary y) {
    return (x / y,);
}
auto sub c816(number x, C y) {
    return (x / y,);
}
auto sub c817(string x, int y) {
    return (x / y,);
}
auto sub c818(string x, float y) {
    return (x / y,);
}
auto sub c819(string x, number y) {
    return (x / y,);
}
auto sub c820(string x, string y) {
    return (x / y,);
}
auto sub c821(string x, bool y) {
    return (x / y,);
}
auto sub c822(string x, date y) {
    return (x / y,);
}
auto sub c823(string x, *int y) {
    return (x / y,);
}
auto sub c824(string x, *float y) {
    return (x / y,);
}
auto sub c825(string x, *number y) {
    return (x / y,);
}
auto sub c826(string x, union<int, float> y) {
    return (x / y,);
}
auto sub c827(string x, union<int, string> y) {
    return (x / y,);
}
auto sub c828(string x, *union<int, number> y) {
    return (x / y,);
}
auto sub c829(string x, list<int> y) {
    return (x / y,);
}
auto sub c830(string x, hash<auto> y) {
    return (x / y,);
}
auto sub c831(string x, binary y) {
    return (x / y,);
}
auto sub c832(string x, C y) {
    return (x / y,);
}
auto sub c833(bool x, int y) {
    return (x / y,);
}
auto sub c834(bool x, float y) {
    return (x / y,);
}
auto sub c835(bool x, number y) {
    return (x / y,);
}
auto sub c836(bool x, string y) {
    return (x / y,);
}
auto sub c837(bool x, bool y) {
    return (x / y,);
}
auto sub c838(bool x, date y) {
    return (x / y,);
}
auto sub c839(bool x, *int y) {
    return (x / y,);
}
auto sub c840(bool x, *float y) {
    return (x / y,);
}
auto sub c841(bool x, *number y) {
    return (x / y,);
}
auto sub c842(bool x, union<int, float> y) {
    return (x / y,);
}
auto sub c843(bool x, union<int, string> y) {
    return (x / y,);
}
auto sub c844(bool x, *union<int, number> y) {
    return (x / y,);
}
auto sub c845(bool x, list<int> y) {
    return (x / y,);
}
auto sub c846(bool x, hash<auto> y) {
    return (x / y,);
}
auto sub c847(bool x, binary y) {
    return (x / y,);
}
auto sub c848(bool x, C y) {
    return (x / y,);
}
auto sub c849(date x, int y) {
    return (x / y,);
}
auto sub c850(date x, float y) {
    return (x / y,);
}
auto sub c851(date x, number y) {
    return (x / y,);
}
auto sub c852(date x, string y) {
    return (x / y,);
}
auto sub c853(date x, bool y) {
    return (x / y,);
}
auto sub c854(date x, date y) {
    return (x / y,);
}
auto sub c855(date x, *int y) {
    return (x / y,);
}
auto sub c856(date x, *float y) {
    return (x / y,);
}
auto sub c857(date x, *number y) {
    return (x / y,);
}
auto sub c858(date x, union<int, float> y) {
    return (x / y,);
}
auto sub c859(date x, union<int, string> y) {
    return (x / y,);
}
auto sub c860(date x, *union<int, number> y) {
    return (x / y,);
}
auto sub c861(date x, list<int> y) {
    return (x / y,);
}
auto sub c862(date x, hash<auto> y) {
    return (x / y,);
}
auto sub c863(date x, binary y) {
    return (x / y,);
}
auto sub c864(date x, C y) {
    return (x / y,);
}
auto sub c865(*int x, int y) {
    return (x / y,);
}
auto sub c866(*int x, float y) {
    return (x / y,);
}
auto sub c867(*int x, number y) {
    return (x / y,);
}
auto sub c868(*int x, string y) {
    return (x / y,);
}
auto sub c869(*int x, bool y) {
    return (x / y,);
}
auto sub c870(*int x, date y) {
    return (x / y,);
}
auto sub c871(*int x, *int y) {
    return (x / y,);
}
auto sub c872(*int x, *float y) {
    return (x / y,);
}
auto sub c873(*int x, *number y) {
    return (x / y,);
}
auto sub c874(*int x, union<int, float> y) {
    return (x / y,);
}
auto sub c875(*int x, union<int, string> y) {
    return (x / y,);
}
auto sub c876(*int x, *union<int, number> y) {
    return (x / y,);
}
auto sub c877(*int x, list<int> y) {
    return (x / y,);
}
auto sub c878(*int x, hash<auto> y) {
    return (x / y,);
}
auto sub c879(*int x, binary y) {
    return (x / y,);
}
auto sub c880(*int x, C y) {
    return (x / y,);
}
auto sub c881(*float x, int y) {
    return (x / y,);
}
auto sub c882(*float x, float y) {
    return (x / y,);
}
auto sub c883(*float x, number y) {
    return (x / y,);
}
auto sub c884(*float x, string y) {
    return (x / y,);
}
auto sub c885(*float x, bool y) {
    return (x / y,);
}
auto sub c886(*float x, date y) {
    return (x / y,);
}
auto sub c887(*float x, *int y) {
    return (x / y,);
}
auto sub c888(*float x, *float y) {
    return (x / y,);
}
auto sub c889(*float x, *number y) {
    return (x / y,);
}
auto sub c890(*float x, union<int, float> y) {
    return (x / y,);
}
auto sub c891(*float x, union<int, string> y) {
    return (x / y,);
}
auto sub c892(*float x, *union<int, number> y) {
    return (x / y,);
}
auto sub c893(*float x, list<int> y) {
    return (x / y,);
}
auto sub c894(*float x, hash<auto> y) {
    return (x / y,);
}
auto sub c895(*float x, binary y) {
    return (x / y,);
}
auto sub c896(*float x, C y) {
    return (x / y,);
}
auto sub c897(*number x, int y) {
    return (x / y,);
}
auto sub c898(*number x, float y) {
    return (x / y,);
}
auto sub c899(*number x, number y) {
    return (x / y,);
}
auto sub c900(*number x, string y) {
    return (x / y,);
}
auto sub c901(*number x, bool y) {
    return (x / y,);
}
auto sub c902(*number x, date y) {
    return (x / y,);
}
auto sub c903(*number x, *int y) {
    return (x / y,);
}
auto sub c904(*number x, *float y) {
    return (x / y,);
}
auto sub c905(*number x, *number y) {
    return (x / y,);
}
auto sub c906(*number x, union<int, float> y) {
    return (x / y,);
}
auto sub c907(*number x, union<int, string> y) {
    return (x / y,);
}
auto sub c908(*number x, *union<int, number> y) {
    return (x / y,);
}
auto sub c909(*number x, list<int> y) {
    return (x / y,);
}
auto sub c910(*number x, hash<auto> y) {
    return (x / y,);
}
auto sub c911(*number x, binary y) {
    return (x / y,);
}
auto sub c912(*number x, C y) {
    return (x / y,);
}
auto sub c913(union<int, float> x, int y) {
    return (x / y,);
}
auto sub c914(union<int, float> x, float y) {
    return (x / y,);
}
auto sub c915(union<int, float> x, number y) {
    return (x / y,);
}
auto sub c916(union<int, float> x, string y) {
    return (x / y,);
}
auto sub c917(union<int, float> x, bool y) {
    return (x / y,);
}
auto sub c918(union<int, float> x, date y) {
    return (x / y,);
}
auto sub c919(union<int, float> x, *int y) {
    return (x / y,);
}
auto sub c920(union<int, float> x, *float y) {
    return (x / y,);
}
auto sub c921(union<int, float> x, *number y) {
    return (x / y,);
}
auto sub c922(union<int, float> x, union<int, float> y) {
    return (x / y,);
}
auto sub c923(union<int, float> x, union<int, string> y) {
    return (x / y,);
}
auto sub c924(union<int, float> x, *union<int, number> y) {
    return (x / y,);
}
auto sub c925(union<int, float> x, list<int> y) {
    return (x / y,);
}
auto sub c926(union<int, float> x, hash<auto> y) {
    return (x / y,);
}
auto sub c927(union<int, float> x, binary y) {
    return (x / y,);
}
auto sub c928(union<int, float> x, C y) {
    return (x / y,);
}
auto sub c929(union<int, string> x, int y) {
    return (x / y,);
}
auto sub c930(union<int, string> x, float y) {
    return (x / y,);
}
auto sub c931(union<int, string> x, number y) {
    return (x / y,);
}
auto sub c932(union<int, string> x, string y) {
    return (x / y,);
}
auto sub c933(union<int, string> x, bool y) {
    return (x / y,);
}
auto sub c934(union<int, string> x, date y) {
    return (x / y,);
}
auto sub c935(union<int, string> x, *int y) {
    return (x / y,);
}
auto sub c936(union<int, string> x, *float y) {
    return (x / y,);
}
auto sub c937(union<int, string> x, *number y) {
    return (x / y,);
}
auto sub c938(union<int, string> x, union<int, float> y) {
    return (x / y,);
}
auto sub c939(union<int, string> x, union<int, string> y) {
    return (x / y,);
}
auto sub c940(union<int, string> x, *union<int, number> y) {
    return (x / y,);
}
auto sub c941(union<int, string> x, list<int> y) {
    return (x / y,);
}
auto sub c942(union<int, string> x, hash<auto> y) {
    return (x / y,);
}
auto sub c943(union<int, string> x, binary y) {
    return (x / y,);
}
auto sub c944(union<int, string> x, C y) {
    return (x / y,);
}
auto sub c945(*union<int, number> x, int y) {
    return (x / y,);
}
auto sub c946(*union<int, number> x, float y) {
    return (x / y,);
}
auto sub c947(*union<int, number> x, number y) {
    return (x / y,);
}
auto sub c948(*union<int, number> x, string y) {
    return (x / y,);
}
auto sub c949(*union<int, number> x, bool y) {
    return (x / y,);
}
auto sub c950(*union<int, number> x, date y) {
    return (x / y,);
}
auto sub c951(*union<int, number> x, *int y) {
    return (x / y,);
}
auto sub c952(*union<int, number> x, *float y) {
    return (x / y,);
}
auto sub c953(*union<int, number> x, *number y) {
    return (x / y,);
}
auto sub c954(*union<int, number> x, union<int, float> y) {
    return (x / y,);
}
auto sub c955(*union<int, number> x, union<int, string> y) {
    return (x / y,);
}
auto sub c956(*union<int, number> x, *union<int, number> y) {
    return (x / y,);
}
auto sub c957(*union<int, number> x, list<int> y) {
    return (x / y,);
}
auto sub c958(*union<int, number> x, hash<auto> y) {
    return (x / y,);
}
auto sub c959(*union<int, number> x, binary y) {
    return (x / y,);
}
auto sub c960(*union<int, number> x, C y) {
    return (x / y,);
}
auto sub c961(list<int> x, int y) {
    return (x / y,);
}
auto sub c962(list<int> x, float y) {
    return (x / y,);
}
auto sub c963(list<int> x, number y) {
    return (x / y,);
}
auto sub c964(list<int> x, string y) {
    return (x / y,);
}
auto sub c965(list<int> x, bool y) {
    return (x / y,);
}
auto sub c966(list<int> x, date y) {
    return (x / y,);
}
auto sub c967(list<int> x, *int y) {
    return (x / y,);
}
auto sub c968(list<int> x, *float y) {
    return (x / y,);
}
auto sub c969(list<int> x, *number y) {
    return (x / y,);
}
auto sub c970(list<int> x, union<int, float> y) {
    return (x / y,);
}
auto sub c971(list<int> x, union<int, string> y) {
    return (x / y,);
}
auto sub c972(list<int> x, *union<int, number> y) {
    return (x / y,);
}
auto sub c973(list<int> x, list<int> y) {
    return (x / y,);
}
auto sub c974(list<int> x, hash<auto> y) {
    return (x / y,);
}
auto sub c975(list<int> x, binary y) {
    return (x / y,);
}
auto sub c976(list<int> x, C y) {
    return (x / y,);
}
auto sub c977(hash<auto> x, int y) {
    return (x / y,);
}
auto sub c978(hash<auto> x, float y) {
    return (x / y,);
}
auto sub c979(hash<auto> x, number y) {
    return (x / y,);
}
auto sub c980(hash<auto> x, string y) {
    return (x / y,);
}
auto sub c981(hash<auto> x, bool y) {
    return (x / y,);
}
auto sub c982(hash<auto> x, date y) {
    return (x / y,);
}
auto sub c983(hash<auto> x, *int y) {
    return (x / y,);
}
auto sub c984(hash<auto> x, *float y) {
    return (x / y,);
}
auto sub c985(hash<auto> x, *number y) {
    return (x / y,);
}
auto sub c986(hash<auto> x, union<int, float> y) {
    return (x / y,);
}
auto sub c987(hash<auto> x, union<int, string> y) {
    return (x / y,);
}
auto sub c988(hash<auto> x, *union<int, number> y) {
    return (x / y,);
}
auto sub c989(hash<auto> x, list<int> y) {
    return (x / y,);
}
auto sub c990(hash<auto> x, hash<auto> y) {
    return (x / y,);
}
auto sub c991(hash<auto> x, binary y) {
    return (x / y,);
}
auto sub c992(hash<auto> x, C y) {
    return (x / y,);
}
auto sub c993(binary x, int y) {
    return (x / y,);
}
auto sub c994(binary x, float y) {
    return (x / y,);
}
auto sub c995(binary x, number y) {
    return (x / y,);
}
auto sub c996(binary x, string y) {
    return (x / y,);
}
auto sub c997(binary x, bool y) {
    return (x / y,);
}
auto sub c998(binary x, date y) {
    return (x / y,);
}
auto sub c999(binary x, *int y) {
    return (x / y,);
}
auto sub c1000(binary x, *float y) {
    return (x / y,);
}
auto sub c1001(binary x, *number y) {
    return (x / y,);
}
auto sub c1002(binary x, union<int, float> y) {
    return (x / y,);
}
auto sub c1003(binary x, union<int, string> y) {
    return (x / y,);
}
auto sub c1004(binary x, *union<int, number> y) {
    return (x / y,);
}
auto sub c1005(binary x, list<int> y) {
    return (x / y,);
}
auto sub c1006(binary x, hash<auto> y) {
    return (x / y,);
}
auto sub c1007(binary x, binary y) {
    return (x / y,);
}
auto sub c1008(binary x, C y) {
    return (x / y,);
}
auto sub c1009(C x, int y) {
    return (x / y,);
}
auto sub c1010(C x, float y) {
    return (x / y,);
}
auto sub c1011(C x, number y) {
    return (x / y,);
}
auto sub c1012(C x, string y) {
    return (x / y,);
}
auto sub c1013(C x, bool y) {
    return (x / y,);
}
auto sub c1014(C x, date y) {
    return (x / y,);
}
auto sub c1015(C x, *int y) {
    return (x / y,);
}
auto sub c1016(C x, *float y) {
    return (x / y,);
}
auto sub c1017(C x, *number y) {
    return (x / y,);
}
auto sub c1018(C x, union<int, float> y) {
    return (x / y,);
}
auto sub c1019(C x, union<int, string> y) {
    return (x / y,);
}
auto sub c1020(C x, *union<int, number> y) {
    return (x / y,);
}
auto sub c1021(C x, list<int> y) {
    return (x / y,);
}
auto sub c1022(C x, hash<auto> y) {
    return (x / y,);
}
auto sub c1023(C x, binary y) {
    return (x / y,);
}
auto sub c1024(C x, C y) {
    return (x / y,);
}

# a value in the label, an object as its class name
string sub vs(auto v) {
    return v.typeCode() == NT_OBJECT ? "object<" + v.className() + ">" : sprintf("%y", v);
}

sub main() {
    # values of int
    list<auto> v0 = (2,);
    # values of float
    list<auto> v1 = (2.5,);
    # values of number
    list<auto> v2 = (2.5n,);
    # values of string
    list<auto> v3 = ("3",);
    # values of bool
    list<auto> v4 = (True,);
    # values of date
    list<auto> v5 = (2026-01-02T00:00:00Z, 1h);
    # values of *int
    list<auto> v6 = (2, NOTHING);
    # values of *float
    list<auto> v7 = (2.5, NOTHING);
    # values of *number
    list<auto> v8 = (2.5n, NOTHING);
    # values of union<int, float>
    list<auto> v9 = (2, 2.5);
    # values of union<int, string>
    list<auto> v10 = (2, "3");
    # values of *union<int, number>
    list<auto> v11 = (2, 2.5n, NOTHING);
    # values of list<int>
    list<auto> v12 = ((1, 2),);
    # values of hash<auto>
    list<auto> v13 = ({"a": 1},);
    # values of binary
    list<auto> v14 = (<01>,);
    # values of C
    list<auto> v15 = (new C(),);
    foreach auto a in (v0) {
        foreach auto b in (v0) {
            show(sprintf("int + int (%s, %s)", vs(a), vs(b)), auto sub () { return c1(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v1) {
            show(sprintf("int + float (%s, %s)", vs(a), vs(b)), auto sub () { return c2(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v2) {
            show(sprintf("int + number (%s, %s)", vs(a), vs(b)), auto sub () { return c3(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v3) {
            show(sprintf("int + string (%s, %s)", vs(a), vs(b)), auto sub () { return c4(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v4) {
            show(sprintf("int + bool (%s, %s)", vs(a), vs(b)), auto sub () { return c5(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v5) {
            show(sprintf("int + date (%s, %s)", vs(a), vs(b)), auto sub () { return c6(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v6) {
            show(sprintf("int + *int (%s, %s)", vs(a), vs(b)), auto sub () { return c7(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v7) {
            show(sprintf("int + *float (%s, %s)", vs(a), vs(b)), auto sub () { return c8(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v8) {
            show(sprintf("int + *number (%s, %s)", vs(a), vs(b)), auto sub () { return c9(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v9) {
            show(sprintf("int + union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c10(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v10) {
            show(sprintf("int + union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c11(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v11) {
            show(sprintf("int + *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c12(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v12) {
            show(sprintf("int + list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c13(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v13) {
            show(sprintf("int + hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c14(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v14) {
            show(sprintf("int + binary (%s, %s)", vs(a), vs(b)), auto sub () { return c15(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v15) {
            show(sprintf("int + C (%s, %s)", vs(a), vs(b)), auto sub () { return c16(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v0) {
            show(sprintf("float + int (%s, %s)", vs(a), vs(b)), auto sub () { return c17(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v1) {
            show(sprintf("float + float (%s, %s)", vs(a), vs(b)), auto sub () { return c18(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v2) {
            show(sprintf("float + number (%s, %s)", vs(a), vs(b)), auto sub () { return c19(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v3) {
            show(sprintf("float + string (%s, %s)", vs(a), vs(b)), auto sub () { return c20(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v4) {
            show(sprintf("float + bool (%s, %s)", vs(a), vs(b)), auto sub () { return c21(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v5) {
            show(sprintf("float + date (%s, %s)", vs(a), vs(b)), auto sub () { return c22(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v6) {
            show(sprintf("float + *int (%s, %s)", vs(a), vs(b)), auto sub () { return c23(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v7) {
            show(sprintf("float + *float (%s, %s)", vs(a), vs(b)), auto sub () { return c24(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v8) {
            show(sprintf("float + *number (%s, %s)", vs(a), vs(b)), auto sub () { return c25(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v9) {
            show(sprintf("float + union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c26(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v10) {
            show(sprintf("float + union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c27(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v11) {
            show(sprintf("float + *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c28(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v12) {
            show(sprintf("float + list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c29(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v13) {
            show(sprintf("float + hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c30(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v14) {
            show(sprintf("float + binary (%s, %s)", vs(a), vs(b)), auto sub () { return c31(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v15) {
            show(sprintf("float + C (%s, %s)", vs(a), vs(b)), auto sub () { return c32(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v0) {
            show(sprintf("number + int (%s, %s)", vs(a), vs(b)), auto sub () { return c33(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v1) {
            show(sprintf("number + float (%s, %s)", vs(a), vs(b)), auto sub () { return c34(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v2) {
            show(sprintf("number + number (%s, %s)", vs(a), vs(b)), auto sub () { return c35(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v3) {
            show(sprintf("number + string (%s, %s)", vs(a), vs(b)), auto sub () { return c36(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v4) {
            show(sprintf("number + bool (%s, %s)", vs(a), vs(b)), auto sub () { return c37(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v5) {
            show(sprintf("number + date (%s, %s)", vs(a), vs(b)), auto sub () { return c38(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v6) {
            show(sprintf("number + *int (%s, %s)", vs(a), vs(b)), auto sub () { return c39(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v7) {
            show(sprintf("number + *float (%s, %s)", vs(a), vs(b)), auto sub () { return c40(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v8) {
            show(sprintf("number + *number (%s, %s)", vs(a), vs(b)), auto sub () { return c41(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v9) {
            show(sprintf("number + union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c42(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v10) {
            show(sprintf("number + union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c43(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v11) {
            show(sprintf("number + *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c44(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v12) {
            show(sprintf("number + list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c45(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v13) {
            show(sprintf("number + hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c46(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v14) {
            show(sprintf("number + binary (%s, %s)", vs(a), vs(b)), auto sub () { return c47(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v15) {
            show(sprintf("number + C (%s, %s)", vs(a), vs(b)), auto sub () { return c48(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v0) {
            show(sprintf("string + int (%s, %s)", vs(a), vs(b)), auto sub () { return c49(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v1) {
            show(sprintf("string + float (%s, %s)", vs(a), vs(b)), auto sub () { return c50(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v2) {
            show(sprintf("string + number (%s, %s)", vs(a), vs(b)), auto sub () { return c51(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v3) {
            show(sprintf("string + string (%s, %s)", vs(a), vs(b)), auto sub () { return c52(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v4) {
            show(sprintf("string + bool (%s, %s)", vs(a), vs(b)), auto sub () { return c53(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v5) {
            show(sprintf("string + date (%s, %s)", vs(a), vs(b)), auto sub () { return c54(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v6) {
            show(sprintf("string + *int (%s, %s)", vs(a), vs(b)), auto sub () { return c55(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v7) {
            show(sprintf("string + *float (%s, %s)", vs(a), vs(b)), auto sub () { return c56(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v8) {
            show(sprintf("string + *number (%s, %s)", vs(a), vs(b)), auto sub () { return c57(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v9) {
            show(sprintf("string + union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c58(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v10) {
            show(sprintf("string + union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c59(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v11) {
            show(sprintf("string + *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c60(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v12) {
            show(sprintf("string + list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c61(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v13) {
            show(sprintf("string + hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c62(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v14) {
            show(sprintf("string + binary (%s, %s)", vs(a), vs(b)), auto sub () { return c63(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v15) {
            show(sprintf("string + C (%s, %s)", vs(a), vs(b)), auto sub () { return c64(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v0) {
            show(sprintf("bool + int (%s, %s)", vs(a), vs(b)), auto sub () { return c65(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v1) {
            show(sprintf("bool + float (%s, %s)", vs(a), vs(b)), auto sub () { return c66(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v2) {
            show(sprintf("bool + number (%s, %s)", vs(a), vs(b)), auto sub () { return c67(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v3) {
            show(sprintf("bool + string (%s, %s)", vs(a), vs(b)), auto sub () { return c68(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v4) {
            show(sprintf("bool + bool (%s, %s)", vs(a), vs(b)), auto sub () { return c69(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v5) {
            show(sprintf("bool + date (%s, %s)", vs(a), vs(b)), auto sub () { return c70(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v6) {
            show(sprintf("bool + *int (%s, %s)", vs(a), vs(b)), auto sub () { return c71(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v7) {
            show(sprintf("bool + *float (%s, %s)", vs(a), vs(b)), auto sub () { return c72(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v8) {
            show(sprintf("bool + *number (%s, %s)", vs(a), vs(b)), auto sub () { return c73(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v9) {
            show(sprintf("bool + union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c74(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v10) {
            show(sprintf("bool + union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c75(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v11) {
            show(sprintf("bool + *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c76(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v12) {
            show(sprintf("bool + list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c77(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v13) {
            show(sprintf("bool + hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c78(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v14) {
            show(sprintf("bool + binary (%s, %s)", vs(a), vs(b)), auto sub () { return c79(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v15) {
            show(sprintf("bool + C (%s, %s)", vs(a), vs(b)), auto sub () { return c80(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v0) {
            show(sprintf("date + int (%s, %s)", vs(a), vs(b)), auto sub () { return c81(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v1) {
            show(sprintf("date + float (%s, %s)", vs(a), vs(b)), auto sub () { return c82(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v2) {
            show(sprintf("date + number (%s, %s)", vs(a), vs(b)), auto sub () { return c83(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v3) {
            show(sprintf("date + string (%s, %s)", vs(a), vs(b)), auto sub () { return c84(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v4) {
            show(sprintf("date + bool (%s, %s)", vs(a), vs(b)), auto sub () { return c85(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v5) {
            show(sprintf("date + date (%s, %s)", vs(a), vs(b)), auto sub () { return c86(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v6) {
            show(sprintf("date + *int (%s, %s)", vs(a), vs(b)), auto sub () { return c87(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v7) {
            show(sprintf("date + *float (%s, %s)", vs(a), vs(b)), auto sub () { return c88(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v8) {
            show(sprintf("date + *number (%s, %s)", vs(a), vs(b)), auto sub () { return c89(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v9) {
            show(sprintf("date + union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c90(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v10) {
            show(sprintf("date + union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c91(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v11) {
            show(sprintf("date + *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c92(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v12) {
            show(sprintf("date + list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c93(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v13) {
            show(sprintf("date + hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c94(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v14) {
            show(sprintf("date + binary (%s, %s)", vs(a), vs(b)), auto sub () { return c95(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v15) {
            show(sprintf("date + C (%s, %s)", vs(a), vs(b)), auto sub () { return c96(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v0) {
            show(sprintf("*int + int (%s, %s)", vs(a), vs(b)), auto sub () { return c97(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v1) {
            show(sprintf("*int + float (%s, %s)", vs(a), vs(b)), auto sub () { return c98(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v2) {
            show(sprintf("*int + number (%s, %s)", vs(a), vs(b)), auto sub () { return c99(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v3) {
            show(sprintf("*int + string (%s, %s)", vs(a), vs(b)), auto sub () { return c100(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v4) {
            show(sprintf("*int + bool (%s, %s)", vs(a), vs(b)), auto sub () { return c101(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v5) {
            show(sprintf("*int + date (%s, %s)", vs(a), vs(b)), auto sub () { return c102(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v6) {
            show(sprintf("*int + *int (%s, %s)", vs(a), vs(b)), auto sub () { return c103(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v7) {
            show(sprintf("*int + *float (%s, %s)", vs(a), vs(b)), auto sub () { return c104(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v8) {
            show(sprintf("*int + *number (%s, %s)", vs(a), vs(b)), auto sub () { return c105(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v9) {
            show(sprintf("*int + union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c106(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v10) {
            show(sprintf("*int + union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c107(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v11) {
            show(sprintf("*int + *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c108(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v12) {
            show(sprintf("*int + list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c109(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v13) {
            show(sprintf("*int + hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c110(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v14) {
            show(sprintf("*int + binary (%s, %s)", vs(a), vs(b)), auto sub () { return c111(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v15) {
            show(sprintf("*int + C (%s, %s)", vs(a), vs(b)), auto sub () { return c112(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v0) {
            show(sprintf("*float + int (%s, %s)", vs(a), vs(b)), auto sub () { return c113(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v1) {
            show(sprintf("*float + float (%s, %s)", vs(a), vs(b)), auto sub () { return c114(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v2) {
            show(sprintf("*float + number (%s, %s)", vs(a), vs(b)), auto sub () { return c115(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v3) {
            show(sprintf("*float + string (%s, %s)", vs(a), vs(b)), auto sub () { return c116(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v4) {
            show(sprintf("*float + bool (%s, %s)", vs(a), vs(b)), auto sub () { return c117(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v5) {
            show(sprintf("*float + date (%s, %s)", vs(a), vs(b)), auto sub () { return c118(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v6) {
            show(sprintf("*float + *int (%s, %s)", vs(a), vs(b)), auto sub () { return c119(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v7) {
            show(sprintf("*float + *float (%s, %s)", vs(a), vs(b)), auto sub () { return c120(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v8) {
            show(sprintf("*float + *number (%s, %s)", vs(a), vs(b)), auto sub () { return c121(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v9) {
            show(sprintf("*float + union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c122(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v10) {
            show(sprintf("*float + union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c123(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v11) {
            show(sprintf("*float + *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c124(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v12) {
            show(sprintf("*float + list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c125(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v13) {
            show(sprintf("*float + hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c126(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v14) {
            show(sprintf("*float + binary (%s, %s)", vs(a), vs(b)), auto sub () { return c127(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v15) {
            show(sprintf("*float + C (%s, %s)", vs(a), vs(b)), auto sub () { return c128(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v0) {
            show(sprintf("*number + int (%s, %s)", vs(a), vs(b)), auto sub () { return c129(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v1) {
            show(sprintf("*number + float (%s, %s)", vs(a), vs(b)), auto sub () { return c130(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v2) {
            show(sprintf("*number + number (%s, %s)", vs(a), vs(b)), auto sub () { return c131(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v3) {
            show(sprintf("*number + string (%s, %s)", vs(a), vs(b)), auto sub () { return c132(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v4) {
            show(sprintf("*number + bool (%s, %s)", vs(a), vs(b)), auto sub () { return c133(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v5) {
            show(sprintf("*number + date (%s, %s)", vs(a), vs(b)), auto sub () { return c134(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v6) {
            show(sprintf("*number + *int (%s, %s)", vs(a), vs(b)), auto sub () { return c135(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v7) {
            show(sprintf("*number + *float (%s, %s)", vs(a), vs(b)), auto sub () { return c136(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v8) {
            show(sprintf("*number + *number (%s, %s)", vs(a), vs(b)), auto sub () { return c137(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v9) {
            show(sprintf("*number + union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c138(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v10) {
            show(sprintf("*number + union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c139(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v11) {
            show(sprintf("*number + *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c140(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v12) {
            show(sprintf("*number + list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c141(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v13) {
            show(sprintf("*number + hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c142(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v14) {
            show(sprintf("*number + binary (%s, %s)", vs(a), vs(b)), auto sub () { return c143(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v15) {
            show(sprintf("*number + C (%s, %s)", vs(a), vs(b)), auto sub () { return c144(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v0) {
            show(sprintf("union<int, float> + int (%s, %s)", vs(a), vs(b)), auto sub () { return c145(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v1) {
            show(sprintf("union<int, float> + float (%s, %s)", vs(a), vs(b)), auto sub () { return c146(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v2) {
            show(sprintf("union<int, float> + number (%s, %s)", vs(a), vs(b)), auto sub () { return c147(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v3) {
            show(sprintf("union<int, float> + string (%s, %s)", vs(a), vs(b)), auto sub () { return c148(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v4) {
            show(sprintf("union<int, float> + bool (%s, %s)", vs(a), vs(b)), auto sub () { return c149(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v5) {
            show(sprintf("union<int, float> + date (%s, %s)", vs(a), vs(b)), auto sub () { return c150(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v6) {
            show(sprintf("union<int, float> + *int (%s, %s)", vs(a), vs(b)), auto sub () { return c151(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v7) {
            show(sprintf("union<int, float> + *float (%s, %s)", vs(a), vs(b)), auto sub () { return c152(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v8) {
            show(sprintf("union<int, float> + *number (%s, %s)", vs(a), vs(b)), auto sub () { return c153(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v9) {
            show(sprintf("union<int, float> + union<int, float> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c154(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v10) {
            show(sprintf("union<int, float> + union<int, string> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c155(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v11) {
            show(sprintf("union<int, float> + *union<int, number> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c156(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v12) {
            show(sprintf("union<int, float> + list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c157(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v13) {
            show(sprintf("union<int, float> + hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c158(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v14) {
            show(sprintf("union<int, float> + binary (%s, %s)", vs(a), vs(b)), auto sub () { return c159(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v15) {
            show(sprintf("union<int, float> + C (%s, %s)", vs(a), vs(b)), auto sub () { return c160(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v0) {
            show(sprintf("union<int, string> + int (%s, %s)", vs(a), vs(b)), auto sub () { return c161(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v1) {
            show(sprintf("union<int, string> + float (%s, %s)", vs(a), vs(b)), auto sub () { return c162(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v2) {
            show(sprintf("union<int, string> + number (%s, %s)", vs(a), vs(b)), auto sub () { return c163(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v3) {
            show(sprintf("union<int, string> + string (%s, %s)", vs(a), vs(b)), auto sub () { return c164(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v4) {
            show(sprintf("union<int, string> + bool (%s, %s)", vs(a), vs(b)), auto sub () { return c165(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v5) {
            show(sprintf("union<int, string> + date (%s, %s)", vs(a), vs(b)), auto sub () { return c166(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v6) {
            show(sprintf("union<int, string> + *int (%s, %s)", vs(a), vs(b)), auto sub () { return c167(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v7) {
            show(sprintf("union<int, string> + *float (%s, %s)", vs(a), vs(b)), auto sub () { return c168(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v8) {
            show(sprintf("union<int, string> + *number (%s, %s)", vs(a), vs(b)), auto sub () { return c169(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v9) {
            show(sprintf("union<int, string> + union<int, float> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c170(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v10) {
            show(sprintf("union<int, string> + union<int, string> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c171(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v11) {
            show(sprintf("union<int, string> + *union<int, number> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c172(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v12) {
            show(sprintf("union<int, string> + list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c173(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v13) {
            show(sprintf("union<int, string> + hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c174(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v14) {
            show(sprintf("union<int, string> + binary (%s, %s)", vs(a), vs(b)), auto sub () { return c175(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v15) {
            show(sprintf("union<int, string> + C (%s, %s)", vs(a), vs(b)), auto sub () { return c176(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v0) {
            show(sprintf("*union<int, number> + int (%s, %s)", vs(a), vs(b)), auto sub () { return c177(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v1) {
            show(sprintf("*union<int, number> + float (%s, %s)", vs(a), vs(b)), auto sub () { return c178(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v2) {
            show(sprintf("*union<int, number> + number (%s, %s)", vs(a), vs(b)), auto sub () { return c179(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v3) {
            show(sprintf("*union<int, number> + string (%s, %s)", vs(a), vs(b)), auto sub () { return c180(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v4) {
            show(sprintf("*union<int, number> + bool (%s, %s)", vs(a), vs(b)), auto sub () { return c181(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v5) {
            show(sprintf("*union<int, number> + date (%s, %s)", vs(a), vs(b)), auto sub () { return c182(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v6) {
            show(sprintf("*union<int, number> + *int (%s, %s)", vs(a), vs(b)), auto sub () { return c183(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v7) {
            show(sprintf("*union<int, number> + *float (%s, %s)", vs(a), vs(b)), auto sub () { return c184(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v8) {
            show(sprintf("*union<int, number> + *number (%s, %s)", vs(a), vs(b)), auto sub () { return c185(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v9) {
            show(sprintf("*union<int, number> + union<int, float> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c186(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v10) {
            show(sprintf("*union<int, number> + union<int, string> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c187(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v11) {
            show(sprintf("*union<int, number> + *union<int, number> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c188(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v12) {
            show(sprintf("*union<int, number> + list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c189(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v13) {
            show(sprintf("*union<int, number> + hash<auto> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c190(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v14) {
            show(sprintf("*union<int, number> + binary (%s, %s)", vs(a), vs(b)), auto sub () { return c191(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v15) {
            show(sprintf("*union<int, number> + C (%s, %s)", vs(a), vs(b)), auto sub () { return c192(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v0) {
            show(sprintf("list<int> + int (%s, %s)", vs(a), vs(b)), auto sub () { return c193(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v1) {
            show(sprintf("list<int> + float (%s, %s)", vs(a), vs(b)), auto sub () { return c194(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v2) {
            show(sprintf("list<int> + number (%s, %s)", vs(a), vs(b)), auto sub () { return c195(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v3) {
            show(sprintf("list<int> + string (%s, %s)", vs(a), vs(b)), auto sub () { return c196(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v4) {
            show(sprintf("list<int> + bool (%s, %s)", vs(a), vs(b)), auto sub () { return c197(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v5) {
            show(sprintf("list<int> + date (%s, %s)", vs(a), vs(b)), auto sub () { return c198(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v6) {
            show(sprintf("list<int> + *int (%s, %s)", vs(a), vs(b)), auto sub () { return c199(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v7) {
            show(sprintf("list<int> + *float (%s, %s)", vs(a), vs(b)), auto sub () { return c200(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v8) {
            show(sprintf("list<int> + *number (%s, %s)", vs(a), vs(b)), auto sub () { return c201(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v9) {
            show(sprintf("list<int> + union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c202(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v10) {
            show(sprintf("list<int> + union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c203(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v11) {
            show(sprintf("list<int> + *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c204(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v12) {
            show(sprintf("list<int> + list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c205(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v13) {
            show(sprintf("list<int> + hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c206(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v14) {
            show(sprintf("list<int> + binary (%s, %s)", vs(a), vs(b)), auto sub () { return c207(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v15) {
            show(sprintf("list<int> + C (%s, %s)", vs(a), vs(b)), auto sub () { return c208(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v0) {
            show(sprintf("hash<auto> + int (%s, %s)", vs(a), vs(b)), auto sub () { return c209(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v1) {
            show(sprintf("hash<auto> + float (%s, %s)", vs(a), vs(b)), auto sub () { return c210(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v2) {
            show(sprintf("hash<auto> + number (%s, %s)", vs(a), vs(b)), auto sub () { return c211(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v3) {
            show(sprintf("hash<auto> + string (%s, %s)", vs(a), vs(b)), auto sub () { return c212(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v4) {
            show(sprintf("hash<auto> + bool (%s, %s)", vs(a), vs(b)), auto sub () { return c213(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v5) {
            show(sprintf("hash<auto> + date (%s, %s)", vs(a), vs(b)), auto sub () { return c214(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v6) {
            show(sprintf("hash<auto> + *int (%s, %s)", vs(a), vs(b)), auto sub () { return c215(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v7) {
            show(sprintf("hash<auto> + *float (%s, %s)", vs(a), vs(b)), auto sub () { return c216(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v8) {
            show(sprintf("hash<auto> + *number (%s, %s)", vs(a), vs(b)), auto sub () { return c217(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v9) {
            show(sprintf("hash<auto> + union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c218(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v10) {
            show(sprintf("hash<auto> + union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c219(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v11) {
            show(sprintf("hash<auto> + *union<int, number> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c220(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v12) {
            show(sprintf("hash<auto> + list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c221(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v13) {
            show(sprintf("hash<auto> + hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c222(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v14) {
            show(sprintf("hash<auto> + binary (%s, %s)", vs(a), vs(b)), auto sub () { return c223(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v15) {
            show(sprintf("hash<auto> + C (%s, %s)", vs(a), vs(b)), auto sub () { return c224(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v0) {
            show(sprintf("binary + int (%s, %s)", vs(a), vs(b)), auto sub () { return c225(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v1) {
            show(sprintf("binary + float (%s, %s)", vs(a), vs(b)), auto sub () { return c226(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v2) {
            show(sprintf("binary + number (%s, %s)", vs(a), vs(b)), auto sub () { return c227(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v3) {
            show(sprintf("binary + string (%s, %s)", vs(a), vs(b)), auto sub () { return c228(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v4) {
            show(sprintf("binary + bool (%s, %s)", vs(a), vs(b)), auto sub () { return c229(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v5) {
            show(sprintf("binary + date (%s, %s)", vs(a), vs(b)), auto sub () { return c230(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v6) {
            show(sprintf("binary + *int (%s, %s)", vs(a), vs(b)), auto sub () { return c231(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v7) {
            show(sprintf("binary + *float (%s, %s)", vs(a), vs(b)), auto sub () { return c232(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v8) {
            show(sprintf("binary + *number (%s, %s)", vs(a), vs(b)), auto sub () { return c233(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v9) {
            show(sprintf("binary + union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c234(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v10) {
            show(sprintf("binary + union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c235(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v11) {
            show(sprintf("binary + *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c236(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v12) {
            show(sprintf("binary + list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c237(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v13) {
            show(sprintf("binary + hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c238(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v14) {
            show(sprintf("binary + binary (%s, %s)", vs(a), vs(b)), auto sub () { return c239(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v15) {
            show(sprintf("binary + C (%s, %s)", vs(a), vs(b)), auto sub () { return c240(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v0) {
            show(sprintf("C + int (%s, %s)", vs(a), vs(b)), auto sub () { return c241(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v1) {
            show(sprintf("C + float (%s, %s)", vs(a), vs(b)), auto sub () { return c242(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v2) {
            show(sprintf("C + number (%s, %s)", vs(a), vs(b)), auto sub () { return c243(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v3) {
            show(sprintf("C + string (%s, %s)", vs(a), vs(b)), auto sub () { return c244(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v4) {
            show(sprintf("C + bool (%s, %s)", vs(a), vs(b)), auto sub () { return c245(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v5) {
            show(sprintf("C + date (%s, %s)", vs(a), vs(b)), auto sub () { return c246(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v6) {
            show(sprintf("C + *int (%s, %s)", vs(a), vs(b)), auto sub () { return c247(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v7) {
            show(sprintf("C + *float (%s, %s)", vs(a), vs(b)), auto sub () { return c248(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v8) {
            show(sprintf("C + *number (%s, %s)", vs(a), vs(b)), auto sub () { return c249(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v9) {
            show(sprintf("C + union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c250(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v10) {
            show(sprintf("C + union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c251(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v11) {
            show(sprintf("C + *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c252(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v12) {
            show(sprintf("C + list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c253(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v13) {
            show(sprintf("C + hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c254(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v14) {
            show(sprintf("C + binary (%s, %s)", vs(a), vs(b)), auto sub () { return c255(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v15) {
            show(sprintf("C + C (%s, %s)", vs(a), vs(b)), auto sub () { return c256(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v0) {
            show(sprintf("int - int (%s, %s)", vs(a), vs(b)), auto sub () { return c257(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v1) {
            show(sprintf("int - float (%s, %s)", vs(a), vs(b)), auto sub () { return c258(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v2) {
            show(sprintf("int - number (%s, %s)", vs(a), vs(b)), auto sub () { return c259(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v3) {
            show(sprintf("int - string (%s, %s)", vs(a), vs(b)), auto sub () { return c260(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v4) {
            show(sprintf("int - bool (%s, %s)", vs(a), vs(b)), auto sub () { return c261(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v5) {
            show(sprintf("int - date (%s, %s)", vs(a), vs(b)), auto sub () { return c262(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v6) {
            show(sprintf("int - *int (%s, %s)", vs(a), vs(b)), auto sub () { return c263(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v7) {
            show(sprintf("int - *float (%s, %s)", vs(a), vs(b)), auto sub () { return c264(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v8) {
            show(sprintf("int - *number (%s, %s)", vs(a), vs(b)), auto sub () { return c265(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v9) {
            show(sprintf("int - union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c266(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v10) {
            show(sprintf("int - union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c267(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v11) {
            show(sprintf("int - *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c268(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v12) {
            show(sprintf("int - list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c269(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v13) {
            show(sprintf("int - hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c270(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v14) {
            show(sprintf("int - binary (%s, %s)", vs(a), vs(b)), auto sub () { return c271(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v15) {
            show(sprintf("int - C (%s, %s)", vs(a), vs(b)), auto sub () { return c272(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v0) {
            show(sprintf("float - int (%s, %s)", vs(a), vs(b)), auto sub () { return c273(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v1) {
            show(sprintf("float - float (%s, %s)", vs(a), vs(b)), auto sub () { return c274(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v2) {
            show(sprintf("float - number (%s, %s)", vs(a), vs(b)), auto sub () { return c275(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v3) {
            show(sprintf("float - string (%s, %s)", vs(a), vs(b)), auto sub () { return c276(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v4) {
            show(sprintf("float - bool (%s, %s)", vs(a), vs(b)), auto sub () { return c277(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v5) {
            show(sprintf("float - date (%s, %s)", vs(a), vs(b)), auto sub () { return c278(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v6) {
            show(sprintf("float - *int (%s, %s)", vs(a), vs(b)), auto sub () { return c279(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v7) {
            show(sprintf("float - *float (%s, %s)", vs(a), vs(b)), auto sub () { return c280(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v8) {
            show(sprintf("float - *number (%s, %s)", vs(a), vs(b)), auto sub () { return c281(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v9) {
            show(sprintf("float - union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c282(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v10) {
            show(sprintf("float - union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c283(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v11) {
            show(sprintf("float - *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c284(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v12) {
            show(sprintf("float - list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c285(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v13) {
            show(sprintf("float - hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c286(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v14) {
            show(sprintf("float - binary (%s, %s)", vs(a), vs(b)), auto sub () { return c287(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v15) {
            show(sprintf("float - C (%s, %s)", vs(a), vs(b)), auto sub () { return c288(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v0) {
            show(sprintf("number - int (%s, %s)", vs(a), vs(b)), auto sub () { return c289(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v1) {
            show(sprintf("number - float (%s, %s)", vs(a), vs(b)), auto sub () { return c290(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v2) {
            show(sprintf("number - number (%s, %s)", vs(a), vs(b)), auto sub () { return c291(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v3) {
            show(sprintf("number - string (%s, %s)", vs(a), vs(b)), auto sub () { return c292(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v4) {
            show(sprintf("number - bool (%s, %s)", vs(a), vs(b)), auto sub () { return c293(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v5) {
            show(sprintf("number - date (%s, %s)", vs(a), vs(b)), auto sub () { return c294(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v6) {
            show(sprintf("number - *int (%s, %s)", vs(a), vs(b)), auto sub () { return c295(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v7) {
            show(sprintf("number - *float (%s, %s)", vs(a), vs(b)), auto sub () { return c296(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v8) {
            show(sprintf("number - *number (%s, %s)", vs(a), vs(b)), auto sub () { return c297(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v9) {
            show(sprintf("number - union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c298(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v10) {
            show(sprintf("number - union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c299(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v11) {
            show(sprintf("number - *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c300(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v12) {
            show(sprintf("number - list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c301(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v13) {
            show(sprintf("number - hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c302(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v14) {
            show(sprintf("number - binary (%s, %s)", vs(a), vs(b)), auto sub () { return c303(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v15) {
            show(sprintf("number - C (%s, %s)", vs(a), vs(b)), auto sub () { return c304(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v0) {
            show(sprintf("string - int (%s, %s)", vs(a), vs(b)), auto sub () { return c305(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v1) {
            show(sprintf("string - float (%s, %s)", vs(a), vs(b)), auto sub () { return c306(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v2) {
            show(sprintf("string - number (%s, %s)", vs(a), vs(b)), auto sub () { return c307(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v3) {
            show(sprintf("string - string (%s, %s)", vs(a), vs(b)), auto sub () { return c308(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v4) {
            show(sprintf("string - bool (%s, %s)", vs(a), vs(b)), auto sub () { return c309(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v5) {
            show(sprintf("string - date (%s, %s)", vs(a), vs(b)), auto sub () { return c310(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v6) {
            show(sprintf("string - *int (%s, %s)", vs(a), vs(b)), auto sub () { return c311(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v7) {
            show(sprintf("string - *float (%s, %s)", vs(a), vs(b)), auto sub () { return c312(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v8) {
            show(sprintf("string - *number (%s, %s)", vs(a), vs(b)), auto sub () { return c313(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v9) {
            show(sprintf("string - union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c314(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v10) {
            show(sprintf("string - union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c315(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v11) {
            show(sprintf("string - *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c316(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v12) {
            show(sprintf("string - list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c317(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v13) {
            show(sprintf("string - hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c318(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v14) {
            show(sprintf("string - binary (%s, %s)", vs(a), vs(b)), auto sub () { return c319(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v15) {
            show(sprintf("string - C (%s, %s)", vs(a), vs(b)), auto sub () { return c320(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v0) {
            show(sprintf("bool - int (%s, %s)", vs(a), vs(b)), auto sub () { return c321(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v1) {
            show(sprintf("bool - float (%s, %s)", vs(a), vs(b)), auto sub () { return c322(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v2) {
            show(sprintf("bool - number (%s, %s)", vs(a), vs(b)), auto sub () { return c323(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v3) {
            show(sprintf("bool - string (%s, %s)", vs(a), vs(b)), auto sub () { return c324(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v4) {
            show(sprintf("bool - bool (%s, %s)", vs(a), vs(b)), auto sub () { return c325(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v5) {
            show(sprintf("bool - date (%s, %s)", vs(a), vs(b)), auto sub () { return c326(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v6) {
            show(sprintf("bool - *int (%s, %s)", vs(a), vs(b)), auto sub () { return c327(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v7) {
            show(sprintf("bool - *float (%s, %s)", vs(a), vs(b)), auto sub () { return c328(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v8) {
            show(sprintf("bool - *number (%s, %s)", vs(a), vs(b)), auto sub () { return c329(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v9) {
            show(sprintf("bool - union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c330(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v10) {
            show(sprintf("bool - union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c331(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v11) {
            show(sprintf("bool - *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c332(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v12) {
            show(sprintf("bool - list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c333(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v13) {
            show(sprintf("bool - hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c334(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v14) {
            show(sprintf("bool - binary (%s, %s)", vs(a), vs(b)), auto sub () { return c335(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v15) {
            show(sprintf("bool - C (%s, %s)", vs(a), vs(b)), auto sub () { return c336(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v0) {
            show(sprintf("date - int (%s, %s)", vs(a), vs(b)), auto sub () { return c337(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v1) {
            show(sprintf("date - float (%s, %s)", vs(a), vs(b)), auto sub () { return c338(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v2) {
            show(sprintf("date - number (%s, %s)", vs(a), vs(b)), auto sub () { return c339(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v3) {
            show(sprintf("date - string (%s, %s)", vs(a), vs(b)), auto sub () { return c340(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v4) {
            show(sprintf("date - bool (%s, %s)", vs(a), vs(b)), auto sub () { return c341(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v5) {
            show(sprintf("date - date (%s, %s)", vs(a), vs(b)), auto sub () { return c342(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v6) {
            show(sprintf("date - *int (%s, %s)", vs(a), vs(b)), auto sub () { return c343(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v7) {
            show(sprintf("date - *float (%s, %s)", vs(a), vs(b)), auto sub () { return c344(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v8) {
            show(sprintf("date - *number (%s, %s)", vs(a), vs(b)), auto sub () { return c345(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v9) {
            show(sprintf("date - union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c346(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v10) {
            show(sprintf("date - union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c347(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v11) {
            show(sprintf("date - *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c348(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v12) {
            show(sprintf("date - list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c349(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v13) {
            show(sprintf("date - hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c350(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v14) {
            show(sprintf("date - binary (%s, %s)", vs(a), vs(b)), auto sub () { return c351(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v15) {
            show(sprintf("date - C (%s, %s)", vs(a), vs(b)), auto sub () { return c352(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v0) {
            show(sprintf("*int - int (%s, %s)", vs(a), vs(b)), auto sub () { return c353(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v1) {
            show(sprintf("*int - float (%s, %s)", vs(a), vs(b)), auto sub () { return c354(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v2) {
            show(sprintf("*int - number (%s, %s)", vs(a), vs(b)), auto sub () { return c355(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v3) {
            show(sprintf("*int - string (%s, %s)", vs(a), vs(b)), auto sub () { return c356(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v4) {
            show(sprintf("*int - bool (%s, %s)", vs(a), vs(b)), auto sub () { return c357(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v5) {
            show(sprintf("*int - date (%s, %s)", vs(a), vs(b)), auto sub () { return c358(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v6) {
            show(sprintf("*int - *int (%s, %s)", vs(a), vs(b)), auto sub () { return c359(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v7) {
            show(sprintf("*int - *float (%s, %s)", vs(a), vs(b)), auto sub () { return c360(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v8) {
            show(sprintf("*int - *number (%s, %s)", vs(a), vs(b)), auto sub () { return c361(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v9) {
            show(sprintf("*int - union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c362(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v10) {
            show(sprintf("*int - union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c363(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v11) {
            show(sprintf("*int - *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c364(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v12) {
            show(sprintf("*int - list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c365(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v13) {
            show(sprintf("*int - hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c366(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v14) {
            show(sprintf("*int - binary (%s, %s)", vs(a), vs(b)), auto sub () { return c367(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v15) {
            show(sprintf("*int - C (%s, %s)", vs(a), vs(b)), auto sub () { return c368(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v0) {
            show(sprintf("*float - int (%s, %s)", vs(a), vs(b)), auto sub () { return c369(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v1) {
            show(sprintf("*float - float (%s, %s)", vs(a), vs(b)), auto sub () { return c370(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v2) {
            show(sprintf("*float - number (%s, %s)", vs(a), vs(b)), auto sub () { return c371(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v3) {
            show(sprintf("*float - string (%s, %s)", vs(a), vs(b)), auto sub () { return c372(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v4) {
            show(sprintf("*float - bool (%s, %s)", vs(a), vs(b)), auto sub () { return c373(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v5) {
            show(sprintf("*float - date (%s, %s)", vs(a), vs(b)), auto sub () { return c374(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v6) {
            show(sprintf("*float - *int (%s, %s)", vs(a), vs(b)), auto sub () { return c375(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v7) {
            show(sprintf("*float - *float (%s, %s)", vs(a), vs(b)), auto sub () { return c376(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v8) {
            show(sprintf("*float - *number (%s, %s)", vs(a), vs(b)), auto sub () { return c377(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v9) {
            show(sprintf("*float - union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c378(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v10) {
            show(sprintf("*float - union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c379(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v11) {
            show(sprintf("*float - *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c380(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v12) {
            show(sprintf("*float - list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c381(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v13) {
            show(sprintf("*float - hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c382(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v14) {
            show(sprintf("*float - binary (%s, %s)", vs(a), vs(b)), auto sub () { return c383(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v15) {
            show(sprintf("*float - C (%s, %s)", vs(a), vs(b)), auto sub () { return c384(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v0) {
            show(sprintf("*number - int (%s, %s)", vs(a), vs(b)), auto sub () { return c385(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v1) {
            show(sprintf("*number - float (%s, %s)", vs(a), vs(b)), auto sub () { return c386(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v2) {
            show(sprintf("*number - number (%s, %s)", vs(a), vs(b)), auto sub () { return c387(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v3) {
            show(sprintf("*number - string (%s, %s)", vs(a), vs(b)), auto sub () { return c388(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v4) {
            show(sprintf("*number - bool (%s, %s)", vs(a), vs(b)), auto sub () { return c389(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v5) {
            show(sprintf("*number - date (%s, %s)", vs(a), vs(b)), auto sub () { return c390(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v6) {
            show(sprintf("*number - *int (%s, %s)", vs(a), vs(b)), auto sub () { return c391(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v7) {
            show(sprintf("*number - *float (%s, %s)", vs(a), vs(b)), auto sub () { return c392(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v8) {
            show(sprintf("*number - *number (%s, %s)", vs(a), vs(b)), auto sub () { return c393(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v9) {
            show(sprintf("*number - union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c394(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v10) {
            show(sprintf("*number - union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c395(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v11) {
            show(sprintf("*number - *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c396(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v12) {
            show(sprintf("*number - list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c397(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v13) {
            show(sprintf("*number - hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c398(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v14) {
            show(sprintf("*number - binary (%s, %s)", vs(a), vs(b)), auto sub () { return c399(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v15) {
            show(sprintf("*number - C (%s, %s)", vs(a), vs(b)), auto sub () { return c400(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v0) {
            show(sprintf("union<int, float> - int (%s, %s)", vs(a), vs(b)), auto sub () { return c401(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v1) {
            show(sprintf("union<int, float> - float (%s, %s)", vs(a), vs(b)), auto sub () { return c402(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v2) {
            show(sprintf("union<int, float> - number (%s, %s)", vs(a), vs(b)), auto sub () { return c403(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v3) {
            show(sprintf("union<int, float> - string (%s, %s)", vs(a), vs(b)), auto sub () { return c404(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v4) {
            show(sprintf("union<int, float> - bool (%s, %s)", vs(a), vs(b)), auto sub () { return c405(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v5) {
            show(sprintf("union<int, float> - date (%s, %s)", vs(a), vs(b)), auto sub () { return c406(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v6) {
            show(sprintf("union<int, float> - *int (%s, %s)", vs(a), vs(b)), auto sub () { return c407(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v7) {
            show(sprintf("union<int, float> - *float (%s, %s)", vs(a), vs(b)), auto sub () { return c408(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v8) {
            show(sprintf("union<int, float> - *number (%s, %s)", vs(a), vs(b)), auto sub () { return c409(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v9) {
            show(sprintf("union<int, float> - union<int, float> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c410(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v10) {
            show(sprintf("union<int, float> - union<int, string> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c411(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v11) {
            show(sprintf("union<int, float> - *union<int, number> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c412(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v12) {
            show(sprintf("union<int, float> - list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c413(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v13) {
            show(sprintf("union<int, float> - hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c414(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v14) {
            show(sprintf("union<int, float> - binary (%s, %s)", vs(a), vs(b)), auto sub () { return c415(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v15) {
            show(sprintf("union<int, float> - C (%s, %s)", vs(a), vs(b)), auto sub () { return c416(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v0) {
            show(sprintf("union<int, string> - int (%s, %s)", vs(a), vs(b)), auto sub () { return c417(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v1) {
            show(sprintf("union<int, string> - float (%s, %s)", vs(a), vs(b)), auto sub () { return c418(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v2) {
            show(sprintf("union<int, string> - number (%s, %s)", vs(a), vs(b)), auto sub () { return c419(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v3) {
            show(sprintf("union<int, string> - string (%s, %s)", vs(a), vs(b)), auto sub () { return c420(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v4) {
            show(sprintf("union<int, string> - bool (%s, %s)", vs(a), vs(b)), auto sub () { return c421(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v5) {
            show(sprintf("union<int, string> - date (%s, %s)", vs(a), vs(b)), auto sub () { return c422(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v6) {
            show(sprintf("union<int, string> - *int (%s, %s)", vs(a), vs(b)), auto sub () { return c423(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v7) {
            show(sprintf("union<int, string> - *float (%s, %s)", vs(a), vs(b)), auto sub () { return c424(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v8) {
            show(sprintf("union<int, string> - *number (%s, %s)", vs(a), vs(b)), auto sub () { return c425(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v9) {
            show(sprintf("union<int, string> - union<int, float> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c426(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v10) {
            show(sprintf("union<int, string> - union<int, string> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c427(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v11) {
            show(sprintf("union<int, string> - *union<int, number> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c428(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v12) {
            show(sprintf("union<int, string> - list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c429(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v13) {
            show(sprintf("union<int, string> - hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c430(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v14) {
            show(sprintf("union<int, string> - binary (%s, %s)", vs(a), vs(b)), auto sub () { return c431(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v15) {
            show(sprintf("union<int, string> - C (%s, %s)", vs(a), vs(b)), auto sub () { return c432(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v0) {
            show(sprintf("*union<int, number> - int (%s, %s)", vs(a), vs(b)), auto sub () { return c433(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v1) {
            show(sprintf("*union<int, number> - float (%s, %s)", vs(a), vs(b)), auto sub () { return c434(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v2) {
            show(sprintf("*union<int, number> - number (%s, %s)", vs(a), vs(b)), auto sub () { return c435(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v3) {
            show(sprintf("*union<int, number> - string (%s, %s)", vs(a), vs(b)), auto sub () { return c436(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v4) {
            show(sprintf("*union<int, number> - bool (%s, %s)", vs(a), vs(b)), auto sub () { return c437(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v5) {
            show(sprintf("*union<int, number> - date (%s, %s)", vs(a), vs(b)), auto sub () { return c438(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v6) {
            show(sprintf("*union<int, number> - *int (%s, %s)", vs(a), vs(b)), auto sub () { return c439(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v7) {
            show(sprintf("*union<int, number> - *float (%s, %s)", vs(a), vs(b)), auto sub () { return c440(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v8) {
            show(sprintf("*union<int, number> - *number (%s, %s)", vs(a), vs(b)), auto sub () { return c441(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v9) {
            show(sprintf("*union<int, number> - union<int, float> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c442(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v10) {
            show(sprintf("*union<int, number> - union<int, string> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c443(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v11) {
            show(sprintf("*union<int, number> - *union<int, number> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c444(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v12) {
            show(sprintf("*union<int, number> - list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c445(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v13) {
            show(sprintf("*union<int, number> - hash<auto> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c446(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v14) {
            show(sprintf("*union<int, number> - binary (%s, %s)", vs(a), vs(b)), auto sub () { return c447(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v15) {
            show(sprintf("*union<int, number> - C (%s, %s)", vs(a), vs(b)), auto sub () { return c448(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v0) {
            show(sprintf("list<int> - int (%s, %s)", vs(a), vs(b)), auto sub () { return c449(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v1) {
            show(sprintf("list<int> - float (%s, %s)", vs(a), vs(b)), auto sub () { return c450(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v2) {
            show(sprintf("list<int> - number (%s, %s)", vs(a), vs(b)), auto sub () { return c451(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v3) {
            show(sprintf("list<int> - string (%s, %s)", vs(a), vs(b)), auto sub () { return c452(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v4) {
            show(sprintf("list<int> - bool (%s, %s)", vs(a), vs(b)), auto sub () { return c453(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v5) {
            show(sprintf("list<int> - date (%s, %s)", vs(a), vs(b)), auto sub () { return c454(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v6) {
            show(sprintf("list<int> - *int (%s, %s)", vs(a), vs(b)), auto sub () { return c455(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v7) {
            show(sprintf("list<int> - *float (%s, %s)", vs(a), vs(b)), auto sub () { return c456(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v8) {
            show(sprintf("list<int> - *number (%s, %s)", vs(a), vs(b)), auto sub () { return c457(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v9) {
            show(sprintf("list<int> - union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c458(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v10) {
            show(sprintf("list<int> - union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c459(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v11) {
            show(sprintf("list<int> - *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c460(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v12) {
            show(sprintf("list<int> - list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c461(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v13) {
            show(sprintf("list<int> - hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c462(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v14) {
            show(sprintf("list<int> - binary (%s, %s)", vs(a), vs(b)), auto sub () { return c463(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v15) {
            show(sprintf("list<int> - C (%s, %s)", vs(a), vs(b)), auto sub () { return c464(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v0) {
            show(sprintf("hash<auto> - int (%s, %s)", vs(a), vs(b)), auto sub () { return c465(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v1) {
            show(sprintf("hash<auto> - float (%s, %s)", vs(a), vs(b)), auto sub () { return c466(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v2) {
            show(sprintf("hash<auto> - number (%s, %s)", vs(a), vs(b)), auto sub () { return c467(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v3) {
            show(sprintf("hash<auto> - string (%s, %s)", vs(a), vs(b)), auto sub () { return c468(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v4) {
            show(sprintf("hash<auto> - bool (%s, %s)", vs(a), vs(b)), auto sub () { return c469(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v5) {
            show(sprintf("hash<auto> - date (%s, %s)", vs(a), vs(b)), auto sub () { return c470(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v6) {
            show(sprintf("hash<auto> - *int (%s, %s)", vs(a), vs(b)), auto sub () { return c471(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v7) {
            show(sprintf("hash<auto> - *float (%s, %s)", vs(a), vs(b)), auto sub () { return c472(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v8) {
            show(sprintf("hash<auto> - *number (%s, %s)", vs(a), vs(b)), auto sub () { return c473(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v9) {
            show(sprintf("hash<auto> - union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c474(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v10) {
            show(sprintf("hash<auto> - union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c475(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v11) {
            show(sprintf("hash<auto> - *union<int, number> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c476(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v12) {
            show(sprintf("hash<auto> - list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c477(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v13) {
            show(sprintf("hash<auto> - hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c478(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v14) {
            show(sprintf("hash<auto> - binary (%s, %s)", vs(a), vs(b)), auto sub () { return c479(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v15) {
            show(sprintf("hash<auto> - C (%s, %s)", vs(a), vs(b)), auto sub () { return c480(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v0) {
            show(sprintf("binary - int (%s, %s)", vs(a), vs(b)), auto sub () { return c481(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v1) {
            show(sprintf("binary - float (%s, %s)", vs(a), vs(b)), auto sub () { return c482(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v2) {
            show(sprintf("binary - number (%s, %s)", vs(a), vs(b)), auto sub () { return c483(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v3) {
            show(sprintf("binary - string (%s, %s)", vs(a), vs(b)), auto sub () { return c484(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v4) {
            show(sprintf("binary - bool (%s, %s)", vs(a), vs(b)), auto sub () { return c485(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v5) {
            show(sprintf("binary - date (%s, %s)", vs(a), vs(b)), auto sub () { return c486(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v6) {
            show(sprintf("binary - *int (%s, %s)", vs(a), vs(b)), auto sub () { return c487(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v7) {
            show(sprintf("binary - *float (%s, %s)", vs(a), vs(b)), auto sub () { return c488(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v8) {
            show(sprintf("binary - *number (%s, %s)", vs(a), vs(b)), auto sub () { return c489(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v9) {
            show(sprintf("binary - union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c490(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v10) {
            show(sprintf("binary - union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c491(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v11) {
            show(sprintf("binary - *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c492(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v12) {
            show(sprintf("binary - list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c493(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v13) {
            show(sprintf("binary - hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c494(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v14) {
            show(sprintf("binary - binary (%s, %s)", vs(a), vs(b)), auto sub () { return c495(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v15) {
            show(sprintf("binary - C (%s, %s)", vs(a), vs(b)), auto sub () { return c496(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v0) {
            show(sprintf("C - int (%s, %s)", vs(a), vs(b)), auto sub () { return c497(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v1) {
            show(sprintf("C - float (%s, %s)", vs(a), vs(b)), auto sub () { return c498(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v2) {
            show(sprintf("C - number (%s, %s)", vs(a), vs(b)), auto sub () { return c499(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v3) {
            show(sprintf("C - string (%s, %s)", vs(a), vs(b)), auto sub () { return c500(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v4) {
            show(sprintf("C - bool (%s, %s)", vs(a), vs(b)), auto sub () { return c501(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v5) {
            show(sprintf("C - date (%s, %s)", vs(a), vs(b)), auto sub () { return c502(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v6) {
            show(sprintf("C - *int (%s, %s)", vs(a), vs(b)), auto sub () { return c503(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v7) {
            show(sprintf("C - *float (%s, %s)", vs(a), vs(b)), auto sub () { return c504(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v8) {
            show(sprintf("C - *number (%s, %s)", vs(a), vs(b)), auto sub () { return c505(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v9) {
            show(sprintf("C - union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c506(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v10) {
            show(sprintf("C - union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c507(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v11) {
            show(sprintf("C - *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c508(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v12) {
            show(sprintf("C - list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c509(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v13) {
            show(sprintf("C - hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c510(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v14) {
            show(sprintf("C - binary (%s, %s)", vs(a), vs(b)), auto sub () { return c511(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v15) {
            show(sprintf("C - C (%s, %s)", vs(a), vs(b)), auto sub () { return c512(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v0) {
            show(sprintf("int * int (%s, %s)", vs(a), vs(b)), auto sub () { return c513(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v1) {
            show(sprintf("int * float (%s, %s)", vs(a), vs(b)), auto sub () { return c514(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v2) {
            show(sprintf("int * number (%s, %s)", vs(a), vs(b)), auto sub () { return c515(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v3) {
            show(sprintf("int * string (%s, %s)", vs(a), vs(b)), auto sub () { return c516(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v4) {
            show(sprintf("int * bool (%s, %s)", vs(a), vs(b)), auto sub () { return c517(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v5) {
            show(sprintf("int * date (%s, %s)", vs(a), vs(b)), auto sub () { return c518(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v6) {
            show(sprintf("int * *int (%s, %s)", vs(a), vs(b)), auto sub () { return c519(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v7) {
            show(sprintf("int * *float (%s, %s)", vs(a), vs(b)), auto sub () { return c520(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v8) {
            show(sprintf("int * *number (%s, %s)", vs(a), vs(b)), auto sub () { return c521(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v9) {
            show(sprintf("int * union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c522(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v10) {
            show(sprintf("int * union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c523(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v11) {
            show(sprintf("int * *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c524(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v12) {
            show(sprintf("int * list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c525(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v13) {
            show(sprintf("int * hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c526(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v14) {
            show(sprintf("int * binary (%s, %s)", vs(a), vs(b)), auto sub () { return c527(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v15) {
            show(sprintf("int * C (%s, %s)", vs(a), vs(b)), auto sub () { return c528(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v0) {
            show(sprintf("float * int (%s, %s)", vs(a), vs(b)), auto sub () { return c529(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v1) {
            show(sprintf("float * float (%s, %s)", vs(a), vs(b)), auto sub () { return c530(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v2) {
            show(sprintf("float * number (%s, %s)", vs(a), vs(b)), auto sub () { return c531(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v3) {
            show(sprintf("float * string (%s, %s)", vs(a), vs(b)), auto sub () { return c532(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v4) {
            show(sprintf("float * bool (%s, %s)", vs(a), vs(b)), auto sub () { return c533(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v5) {
            show(sprintf("float * date (%s, %s)", vs(a), vs(b)), auto sub () { return c534(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v6) {
            show(sprintf("float * *int (%s, %s)", vs(a), vs(b)), auto sub () { return c535(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v7) {
            show(sprintf("float * *float (%s, %s)", vs(a), vs(b)), auto sub () { return c536(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v8) {
            show(sprintf("float * *number (%s, %s)", vs(a), vs(b)), auto sub () { return c537(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v9) {
            show(sprintf("float * union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c538(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v10) {
            show(sprintf("float * union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c539(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v11) {
            show(sprintf("float * *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c540(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v12) {
            show(sprintf("float * list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c541(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v13) {
            show(sprintf("float * hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c542(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v14) {
            show(sprintf("float * binary (%s, %s)", vs(a), vs(b)), auto sub () { return c543(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v15) {
            show(sprintf("float * C (%s, %s)", vs(a), vs(b)), auto sub () { return c544(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v0) {
            show(sprintf("number * int (%s, %s)", vs(a), vs(b)), auto sub () { return c545(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v1) {
            show(sprintf("number * float (%s, %s)", vs(a), vs(b)), auto sub () { return c546(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v2) {
            show(sprintf("number * number (%s, %s)", vs(a), vs(b)), auto sub () { return c547(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v3) {
            show(sprintf("number * string (%s, %s)", vs(a), vs(b)), auto sub () { return c548(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v4) {
            show(sprintf("number * bool (%s, %s)", vs(a), vs(b)), auto sub () { return c549(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v5) {
            show(sprintf("number * date (%s, %s)", vs(a), vs(b)), auto sub () { return c550(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v6) {
            show(sprintf("number * *int (%s, %s)", vs(a), vs(b)), auto sub () { return c551(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v7) {
            show(sprintf("number * *float (%s, %s)", vs(a), vs(b)), auto sub () { return c552(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v8) {
            show(sprintf("number * *number (%s, %s)", vs(a), vs(b)), auto sub () { return c553(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v9) {
            show(sprintf("number * union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c554(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v10) {
            show(sprintf("number * union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c555(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v11) {
            show(sprintf("number * *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c556(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v12) {
            show(sprintf("number * list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c557(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v13) {
            show(sprintf("number * hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c558(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v14) {
            show(sprintf("number * binary (%s, %s)", vs(a), vs(b)), auto sub () { return c559(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v15) {
            show(sprintf("number * C (%s, %s)", vs(a), vs(b)), auto sub () { return c560(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v0) {
            show(sprintf("string * int (%s, %s)", vs(a), vs(b)), auto sub () { return c561(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v1) {
            show(sprintf("string * float (%s, %s)", vs(a), vs(b)), auto sub () { return c562(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v2) {
            show(sprintf("string * number (%s, %s)", vs(a), vs(b)), auto sub () { return c563(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v3) {
            show(sprintf("string * string (%s, %s)", vs(a), vs(b)), auto sub () { return c564(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v4) {
            show(sprintf("string * bool (%s, %s)", vs(a), vs(b)), auto sub () { return c565(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v5) {
            show(sprintf("string * date (%s, %s)", vs(a), vs(b)), auto sub () { return c566(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v6) {
            show(sprintf("string * *int (%s, %s)", vs(a), vs(b)), auto sub () { return c567(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v7) {
            show(sprintf("string * *float (%s, %s)", vs(a), vs(b)), auto sub () { return c568(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v8) {
            show(sprintf("string * *number (%s, %s)", vs(a), vs(b)), auto sub () { return c569(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v9) {
            show(sprintf("string * union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c570(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v10) {
            show(sprintf("string * union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c571(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v11) {
            show(sprintf("string * *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c572(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v12) {
            show(sprintf("string * list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c573(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v13) {
            show(sprintf("string * hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c574(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v14) {
            show(sprintf("string * binary (%s, %s)", vs(a), vs(b)), auto sub () { return c575(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v15) {
            show(sprintf("string * C (%s, %s)", vs(a), vs(b)), auto sub () { return c576(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v0) {
            show(sprintf("bool * int (%s, %s)", vs(a), vs(b)), auto sub () { return c577(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v1) {
            show(sprintf("bool * float (%s, %s)", vs(a), vs(b)), auto sub () { return c578(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v2) {
            show(sprintf("bool * number (%s, %s)", vs(a), vs(b)), auto sub () { return c579(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v3) {
            show(sprintf("bool * string (%s, %s)", vs(a), vs(b)), auto sub () { return c580(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v4) {
            show(sprintf("bool * bool (%s, %s)", vs(a), vs(b)), auto sub () { return c581(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v5) {
            show(sprintf("bool * date (%s, %s)", vs(a), vs(b)), auto sub () { return c582(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v6) {
            show(sprintf("bool * *int (%s, %s)", vs(a), vs(b)), auto sub () { return c583(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v7) {
            show(sprintf("bool * *float (%s, %s)", vs(a), vs(b)), auto sub () { return c584(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v8) {
            show(sprintf("bool * *number (%s, %s)", vs(a), vs(b)), auto sub () { return c585(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v9) {
            show(sprintf("bool * union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c586(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v10) {
            show(sprintf("bool * union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c587(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v11) {
            show(sprintf("bool * *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c588(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v12) {
            show(sprintf("bool * list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c589(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v13) {
            show(sprintf("bool * hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c590(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v14) {
            show(sprintf("bool * binary (%s, %s)", vs(a), vs(b)), auto sub () { return c591(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v15) {
            show(sprintf("bool * C (%s, %s)", vs(a), vs(b)), auto sub () { return c592(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v0) {
            show(sprintf("date * int (%s, %s)", vs(a), vs(b)), auto sub () { return c593(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v1) {
            show(sprintf("date * float (%s, %s)", vs(a), vs(b)), auto sub () { return c594(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v2) {
            show(sprintf("date * number (%s, %s)", vs(a), vs(b)), auto sub () { return c595(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v3) {
            show(sprintf("date * string (%s, %s)", vs(a), vs(b)), auto sub () { return c596(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v4) {
            show(sprintf("date * bool (%s, %s)", vs(a), vs(b)), auto sub () { return c597(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v5) {
            show(sprintf("date * date (%s, %s)", vs(a), vs(b)), auto sub () { return c598(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v6) {
            show(sprintf("date * *int (%s, %s)", vs(a), vs(b)), auto sub () { return c599(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v7) {
            show(sprintf("date * *float (%s, %s)", vs(a), vs(b)), auto sub () { return c600(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v8) {
            show(sprintf("date * *number (%s, %s)", vs(a), vs(b)), auto sub () { return c601(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v9) {
            show(sprintf("date * union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c602(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v10) {
            show(sprintf("date * union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c603(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v11) {
            show(sprintf("date * *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c604(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v12) {
            show(sprintf("date * list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c605(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v13) {
            show(sprintf("date * hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c606(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v14) {
            show(sprintf("date * binary (%s, %s)", vs(a), vs(b)), auto sub () { return c607(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v15) {
            show(sprintf("date * C (%s, %s)", vs(a), vs(b)), auto sub () { return c608(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v0) {
            show(sprintf("*int * int (%s, %s)", vs(a), vs(b)), auto sub () { return c609(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v1) {
            show(sprintf("*int * float (%s, %s)", vs(a), vs(b)), auto sub () { return c610(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v2) {
            show(sprintf("*int * number (%s, %s)", vs(a), vs(b)), auto sub () { return c611(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v3) {
            show(sprintf("*int * string (%s, %s)", vs(a), vs(b)), auto sub () { return c612(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v4) {
            show(sprintf("*int * bool (%s, %s)", vs(a), vs(b)), auto sub () { return c613(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v5) {
            show(sprintf("*int * date (%s, %s)", vs(a), vs(b)), auto sub () { return c614(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v6) {
            show(sprintf("*int * *int (%s, %s)", vs(a), vs(b)), auto sub () { return c615(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v7) {
            show(sprintf("*int * *float (%s, %s)", vs(a), vs(b)), auto sub () { return c616(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v8) {
            show(sprintf("*int * *number (%s, %s)", vs(a), vs(b)), auto sub () { return c617(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v9) {
            show(sprintf("*int * union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c618(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v10) {
            show(sprintf("*int * union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c619(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v11) {
            show(sprintf("*int * *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c620(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v12) {
            show(sprintf("*int * list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c621(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v13) {
            show(sprintf("*int * hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c622(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v14) {
            show(sprintf("*int * binary (%s, %s)", vs(a), vs(b)), auto sub () { return c623(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v15) {
            show(sprintf("*int * C (%s, %s)", vs(a), vs(b)), auto sub () { return c624(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v0) {
            show(sprintf("*float * int (%s, %s)", vs(a), vs(b)), auto sub () { return c625(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v1) {
            show(sprintf("*float * float (%s, %s)", vs(a), vs(b)), auto sub () { return c626(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v2) {
            show(sprintf("*float * number (%s, %s)", vs(a), vs(b)), auto sub () { return c627(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v3) {
            show(sprintf("*float * string (%s, %s)", vs(a), vs(b)), auto sub () { return c628(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v4) {
            show(sprintf("*float * bool (%s, %s)", vs(a), vs(b)), auto sub () { return c629(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v5) {
            show(sprintf("*float * date (%s, %s)", vs(a), vs(b)), auto sub () { return c630(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v6) {
            show(sprintf("*float * *int (%s, %s)", vs(a), vs(b)), auto sub () { return c631(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v7) {
            show(sprintf("*float * *float (%s, %s)", vs(a), vs(b)), auto sub () { return c632(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v8) {
            show(sprintf("*float * *number (%s, %s)", vs(a), vs(b)), auto sub () { return c633(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v9) {
            show(sprintf("*float * union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c634(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v10) {
            show(sprintf("*float * union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c635(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v11) {
            show(sprintf("*float * *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c636(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v12) {
            show(sprintf("*float * list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c637(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v13) {
            show(sprintf("*float * hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c638(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v14) {
            show(sprintf("*float * binary (%s, %s)", vs(a), vs(b)), auto sub () { return c639(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v15) {
            show(sprintf("*float * C (%s, %s)", vs(a), vs(b)), auto sub () { return c640(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v0) {
            show(sprintf("*number * int (%s, %s)", vs(a), vs(b)), auto sub () { return c641(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v1) {
            show(sprintf("*number * float (%s, %s)", vs(a), vs(b)), auto sub () { return c642(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v2) {
            show(sprintf("*number * number (%s, %s)", vs(a), vs(b)), auto sub () { return c643(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v3) {
            show(sprintf("*number * string (%s, %s)", vs(a), vs(b)), auto sub () { return c644(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v4) {
            show(sprintf("*number * bool (%s, %s)", vs(a), vs(b)), auto sub () { return c645(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v5) {
            show(sprintf("*number * date (%s, %s)", vs(a), vs(b)), auto sub () { return c646(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v6) {
            show(sprintf("*number * *int (%s, %s)", vs(a), vs(b)), auto sub () { return c647(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v7) {
            show(sprintf("*number * *float (%s, %s)", vs(a), vs(b)), auto sub () { return c648(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v8) {
            show(sprintf("*number * *number (%s, %s)", vs(a), vs(b)), auto sub () { return c649(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v9) {
            show(sprintf("*number * union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c650(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v10) {
            show(sprintf("*number * union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c651(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v11) {
            show(sprintf("*number * *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c652(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v12) {
            show(sprintf("*number * list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c653(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v13) {
            show(sprintf("*number * hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c654(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v14) {
            show(sprintf("*number * binary (%s, %s)", vs(a), vs(b)), auto sub () { return c655(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v15) {
            show(sprintf("*number * C (%s, %s)", vs(a), vs(b)), auto sub () { return c656(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v0) {
            show(sprintf("union<int, float> * int (%s, %s)", vs(a), vs(b)), auto sub () { return c657(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v1) {
            show(sprintf("union<int, float> * float (%s, %s)", vs(a), vs(b)), auto sub () { return c658(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v2) {
            show(sprintf("union<int, float> * number (%s, %s)", vs(a), vs(b)), auto sub () { return c659(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v3) {
            show(sprintf("union<int, float> * string (%s, %s)", vs(a), vs(b)), auto sub () { return c660(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v4) {
            show(sprintf("union<int, float> * bool (%s, %s)", vs(a), vs(b)), auto sub () { return c661(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v5) {
            show(sprintf("union<int, float> * date (%s, %s)", vs(a), vs(b)), auto sub () { return c662(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v6) {
            show(sprintf("union<int, float> * *int (%s, %s)", vs(a), vs(b)), auto sub () { return c663(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v7) {
            show(sprintf("union<int, float> * *float (%s, %s)", vs(a), vs(b)), auto sub () { return c664(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v8) {
            show(sprintf("union<int, float> * *number (%s, %s)", vs(a), vs(b)), auto sub () { return c665(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v9) {
            show(sprintf("union<int, float> * union<int, float> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c666(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v10) {
            show(sprintf("union<int, float> * union<int, string> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c667(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v11) {
            show(sprintf("union<int, float> * *union<int, number> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c668(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v12) {
            show(sprintf("union<int, float> * list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c669(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v13) {
            show(sprintf("union<int, float> * hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c670(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v14) {
            show(sprintf("union<int, float> * binary (%s, %s)", vs(a), vs(b)), auto sub () { return c671(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v15) {
            show(sprintf("union<int, float> * C (%s, %s)", vs(a), vs(b)), auto sub () { return c672(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v0) {
            show(sprintf("union<int, string> * int (%s, %s)", vs(a), vs(b)), auto sub () { return c673(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v1) {
            show(sprintf("union<int, string> * float (%s, %s)", vs(a), vs(b)), auto sub () { return c674(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v2) {
            show(sprintf("union<int, string> * number (%s, %s)", vs(a), vs(b)), auto sub () { return c675(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v3) {
            show(sprintf("union<int, string> * string (%s, %s)", vs(a), vs(b)), auto sub () { return c676(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v4) {
            show(sprintf("union<int, string> * bool (%s, %s)", vs(a), vs(b)), auto sub () { return c677(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v5) {
            show(sprintf("union<int, string> * date (%s, %s)", vs(a), vs(b)), auto sub () { return c678(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v6) {
            show(sprintf("union<int, string> * *int (%s, %s)", vs(a), vs(b)), auto sub () { return c679(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v7) {
            show(sprintf("union<int, string> * *float (%s, %s)", vs(a), vs(b)), auto sub () { return c680(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v8) {
            show(sprintf("union<int, string> * *number (%s, %s)", vs(a), vs(b)), auto sub () { return c681(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v9) {
            show(sprintf("union<int, string> * union<int, float> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c682(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v10) {
            show(sprintf("union<int, string> * union<int, string> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c683(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v11) {
            show(sprintf("union<int, string> * *union<int, number> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c684(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v12) {
            show(sprintf("union<int, string> * list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c685(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v13) {
            show(sprintf("union<int, string> * hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c686(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v14) {
            show(sprintf("union<int, string> * binary (%s, %s)", vs(a), vs(b)), auto sub () { return c687(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v15) {
            show(sprintf("union<int, string> * C (%s, %s)", vs(a), vs(b)), auto sub () { return c688(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v0) {
            show(sprintf("*union<int, number> * int (%s, %s)", vs(a), vs(b)), auto sub () { return c689(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v1) {
            show(sprintf("*union<int, number> * float (%s, %s)", vs(a), vs(b)), auto sub () { return c690(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v2) {
            show(sprintf("*union<int, number> * number (%s, %s)", vs(a), vs(b)), auto sub () { return c691(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v3) {
            show(sprintf("*union<int, number> * string (%s, %s)", vs(a), vs(b)), auto sub () { return c692(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v4) {
            show(sprintf("*union<int, number> * bool (%s, %s)", vs(a), vs(b)), auto sub () { return c693(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v5) {
            show(sprintf("*union<int, number> * date (%s, %s)", vs(a), vs(b)), auto sub () { return c694(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v6) {
            show(sprintf("*union<int, number> * *int (%s, %s)", vs(a), vs(b)), auto sub () { return c695(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v7) {
            show(sprintf("*union<int, number> * *float (%s, %s)", vs(a), vs(b)), auto sub () { return c696(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v8) {
            show(sprintf("*union<int, number> * *number (%s, %s)", vs(a), vs(b)), auto sub () { return c697(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v9) {
            show(sprintf("*union<int, number> * union<int, float> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c698(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v10) {
            show(sprintf("*union<int, number> * union<int, string> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c699(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v11) {
            show(sprintf("*union<int, number> * *union<int, number> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c700(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v12) {
            show(sprintf("*union<int, number> * list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c701(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v13) {
            show(sprintf("*union<int, number> * hash<auto> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c702(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v14) {
            show(sprintf("*union<int, number> * binary (%s, %s)", vs(a), vs(b)), auto sub () { return c703(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v15) {
            show(sprintf("*union<int, number> * C (%s, %s)", vs(a), vs(b)), auto sub () { return c704(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v0) {
            show(sprintf("list<int> * int (%s, %s)", vs(a), vs(b)), auto sub () { return c705(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v1) {
            show(sprintf("list<int> * float (%s, %s)", vs(a), vs(b)), auto sub () { return c706(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v2) {
            show(sprintf("list<int> * number (%s, %s)", vs(a), vs(b)), auto sub () { return c707(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v3) {
            show(sprintf("list<int> * string (%s, %s)", vs(a), vs(b)), auto sub () { return c708(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v4) {
            show(sprintf("list<int> * bool (%s, %s)", vs(a), vs(b)), auto sub () { return c709(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v5) {
            show(sprintf("list<int> * date (%s, %s)", vs(a), vs(b)), auto sub () { return c710(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v6) {
            show(sprintf("list<int> * *int (%s, %s)", vs(a), vs(b)), auto sub () { return c711(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v7) {
            show(sprintf("list<int> * *float (%s, %s)", vs(a), vs(b)), auto sub () { return c712(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v8) {
            show(sprintf("list<int> * *number (%s, %s)", vs(a), vs(b)), auto sub () { return c713(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v9) {
            show(sprintf("list<int> * union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c714(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v10) {
            show(sprintf("list<int> * union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c715(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v11) {
            show(sprintf("list<int> * *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c716(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v12) {
            show(sprintf("list<int> * list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c717(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v13) {
            show(sprintf("list<int> * hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c718(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v14) {
            show(sprintf("list<int> * binary (%s, %s)", vs(a), vs(b)), auto sub () { return c719(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v15) {
            show(sprintf("list<int> * C (%s, %s)", vs(a), vs(b)), auto sub () { return c720(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v0) {
            show(sprintf("hash<auto> * int (%s, %s)", vs(a), vs(b)), auto sub () { return c721(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v1) {
            show(sprintf("hash<auto> * float (%s, %s)", vs(a), vs(b)), auto sub () { return c722(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v2) {
            show(sprintf("hash<auto> * number (%s, %s)", vs(a), vs(b)), auto sub () { return c723(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v3) {
            show(sprintf("hash<auto> * string (%s, %s)", vs(a), vs(b)), auto sub () { return c724(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v4) {
            show(sprintf("hash<auto> * bool (%s, %s)", vs(a), vs(b)), auto sub () { return c725(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v5) {
            show(sprintf("hash<auto> * date (%s, %s)", vs(a), vs(b)), auto sub () { return c726(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v6) {
            show(sprintf("hash<auto> * *int (%s, %s)", vs(a), vs(b)), auto sub () { return c727(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v7) {
            show(sprintf("hash<auto> * *float (%s, %s)", vs(a), vs(b)), auto sub () { return c728(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v8) {
            show(sprintf("hash<auto> * *number (%s, %s)", vs(a), vs(b)), auto sub () { return c729(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v9) {
            show(sprintf("hash<auto> * union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c730(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v10) {
            show(sprintf("hash<auto> * union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c731(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v11) {
            show(sprintf("hash<auto> * *union<int, number> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c732(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v12) {
            show(sprintf("hash<auto> * list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c733(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v13) {
            show(sprintf("hash<auto> * hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c734(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v14) {
            show(sprintf("hash<auto> * binary (%s, %s)", vs(a), vs(b)), auto sub () { return c735(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v15) {
            show(sprintf("hash<auto> * C (%s, %s)", vs(a), vs(b)), auto sub () { return c736(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v0) {
            show(sprintf("binary * int (%s, %s)", vs(a), vs(b)), auto sub () { return c737(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v1) {
            show(sprintf("binary * float (%s, %s)", vs(a), vs(b)), auto sub () { return c738(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v2) {
            show(sprintf("binary * number (%s, %s)", vs(a), vs(b)), auto sub () { return c739(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v3) {
            show(sprintf("binary * string (%s, %s)", vs(a), vs(b)), auto sub () { return c740(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v4) {
            show(sprintf("binary * bool (%s, %s)", vs(a), vs(b)), auto sub () { return c741(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v5) {
            show(sprintf("binary * date (%s, %s)", vs(a), vs(b)), auto sub () { return c742(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v6) {
            show(sprintf("binary * *int (%s, %s)", vs(a), vs(b)), auto sub () { return c743(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v7) {
            show(sprintf("binary * *float (%s, %s)", vs(a), vs(b)), auto sub () { return c744(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v8) {
            show(sprintf("binary * *number (%s, %s)", vs(a), vs(b)), auto sub () { return c745(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v9) {
            show(sprintf("binary * union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c746(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v10) {
            show(sprintf("binary * union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c747(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v11) {
            show(sprintf("binary * *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c748(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v12) {
            show(sprintf("binary * list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c749(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v13) {
            show(sprintf("binary * hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c750(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v14) {
            show(sprintf("binary * binary (%s, %s)", vs(a), vs(b)), auto sub () { return c751(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v15) {
            show(sprintf("binary * C (%s, %s)", vs(a), vs(b)), auto sub () { return c752(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v0) {
            show(sprintf("C * int (%s, %s)", vs(a), vs(b)), auto sub () { return c753(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v1) {
            show(sprintf("C * float (%s, %s)", vs(a), vs(b)), auto sub () { return c754(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v2) {
            show(sprintf("C * number (%s, %s)", vs(a), vs(b)), auto sub () { return c755(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v3) {
            show(sprintf("C * string (%s, %s)", vs(a), vs(b)), auto sub () { return c756(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v4) {
            show(sprintf("C * bool (%s, %s)", vs(a), vs(b)), auto sub () { return c757(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v5) {
            show(sprintf("C * date (%s, %s)", vs(a), vs(b)), auto sub () { return c758(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v6) {
            show(sprintf("C * *int (%s, %s)", vs(a), vs(b)), auto sub () { return c759(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v7) {
            show(sprintf("C * *float (%s, %s)", vs(a), vs(b)), auto sub () { return c760(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v8) {
            show(sprintf("C * *number (%s, %s)", vs(a), vs(b)), auto sub () { return c761(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v9) {
            show(sprintf("C * union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c762(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v10) {
            show(sprintf("C * union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c763(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v11) {
            show(sprintf("C * *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c764(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v12) {
            show(sprintf("C * list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c765(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v13) {
            show(sprintf("C * hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c766(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v14) {
            show(sprintf("C * binary (%s, %s)", vs(a), vs(b)), auto sub () { return c767(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v15) {
            show(sprintf("C * C (%s, %s)", vs(a), vs(b)), auto sub () { return c768(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v0) {
            show(sprintf("int / int (%s, %s)", vs(a), vs(b)), auto sub () { return c769(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v1) {
            show(sprintf("int / float (%s, %s)", vs(a), vs(b)), auto sub () { return c770(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v2) {
            show(sprintf("int / number (%s, %s)", vs(a), vs(b)), auto sub () { return c771(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v3) {
            show(sprintf("int / string (%s, %s)", vs(a), vs(b)), auto sub () { return c772(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v4) {
            show(sprintf("int / bool (%s, %s)", vs(a), vs(b)), auto sub () { return c773(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v5) {
            show(sprintf("int / date (%s, %s)", vs(a), vs(b)), auto sub () { return c774(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v6) {
            show(sprintf("int / *int (%s, %s)", vs(a), vs(b)), auto sub () { return c775(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v7) {
            show(sprintf("int / *float (%s, %s)", vs(a), vs(b)), auto sub () { return c776(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v8) {
            show(sprintf("int / *number (%s, %s)", vs(a), vs(b)), auto sub () { return c777(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v9) {
            show(sprintf("int / union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c778(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v10) {
            show(sprintf("int / union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c779(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v11) {
            show(sprintf("int / *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c780(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v12) {
            show(sprintf("int / list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c781(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v13) {
            show(sprintf("int / hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c782(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v14) {
            show(sprintf("int / binary (%s, %s)", vs(a), vs(b)), auto sub () { return c783(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v15) {
            show(sprintf("int / C (%s, %s)", vs(a), vs(b)), auto sub () { return c784(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v0) {
            show(sprintf("float / int (%s, %s)", vs(a), vs(b)), auto sub () { return c785(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v1) {
            show(sprintf("float / float (%s, %s)", vs(a), vs(b)), auto sub () { return c786(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v2) {
            show(sprintf("float / number (%s, %s)", vs(a), vs(b)), auto sub () { return c787(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v3) {
            show(sprintf("float / string (%s, %s)", vs(a), vs(b)), auto sub () { return c788(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v4) {
            show(sprintf("float / bool (%s, %s)", vs(a), vs(b)), auto sub () { return c789(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v5) {
            show(sprintf("float / date (%s, %s)", vs(a), vs(b)), auto sub () { return c790(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v6) {
            show(sprintf("float / *int (%s, %s)", vs(a), vs(b)), auto sub () { return c791(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v7) {
            show(sprintf("float / *float (%s, %s)", vs(a), vs(b)), auto sub () { return c792(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v8) {
            show(sprintf("float / *number (%s, %s)", vs(a), vs(b)), auto sub () { return c793(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v9) {
            show(sprintf("float / union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c794(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v10) {
            show(sprintf("float / union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c795(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v11) {
            show(sprintf("float / *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c796(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v12) {
            show(sprintf("float / list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c797(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v13) {
            show(sprintf("float / hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c798(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v14) {
            show(sprintf("float / binary (%s, %s)", vs(a), vs(b)), auto sub () { return c799(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v15) {
            show(sprintf("float / C (%s, %s)", vs(a), vs(b)), auto sub () { return c800(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v0) {
            show(sprintf("number / int (%s, %s)", vs(a), vs(b)), auto sub () { return c801(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v1) {
            show(sprintf("number / float (%s, %s)", vs(a), vs(b)), auto sub () { return c802(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v2) {
            show(sprintf("number / number (%s, %s)", vs(a), vs(b)), auto sub () { return c803(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v3) {
            show(sprintf("number / string (%s, %s)", vs(a), vs(b)), auto sub () { return c804(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v4) {
            show(sprintf("number / bool (%s, %s)", vs(a), vs(b)), auto sub () { return c805(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v5) {
            show(sprintf("number / date (%s, %s)", vs(a), vs(b)), auto sub () { return c806(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v6) {
            show(sprintf("number / *int (%s, %s)", vs(a), vs(b)), auto sub () { return c807(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v7) {
            show(sprintf("number / *float (%s, %s)", vs(a), vs(b)), auto sub () { return c808(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v8) {
            show(sprintf("number / *number (%s, %s)", vs(a), vs(b)), auto sub () { return c809(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v9) {
            show(sprintf("number / union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c810(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v10) {
            show(sprintf("number / union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c811(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v11) {
            show(sprintf("number / *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c812(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v12) {
            show(sprintf("number / list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c813(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v13) {
            show(sprintf("number / hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c814(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v14) {
            show(sprintf("number / binary (%s, %s)", vs(a), vs(b)), auto sub () { return c815(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v15) {
            show(sprintf("number / C (%s, %s)", vs(a), vs(b)), auto sub () { return c816(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v0) {
            show(sprintf("string / int (%s, %s)", vs(a), vs(b)), auto sub () { return c817(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v1) {
            show(sprintf("string / float (%s, %s)", vs(a), vs(b)), auto sub () { return c818(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v2) {
            show(sprintf("string / number (%s, %s)", vs(a), vs(b)), auto sub () { return c819(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v3) {
            show(sprintf("string / string (%s, %s)", vs(a), vs(b)), auto sub () { return c820(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v4) {
            show(sprintf("string / bool (%s, %s)", vs(a), vs(b)), auto sub () { return c821(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v5) {
            show(sprintf("string / date (%s, %s)", vs(a), vs(b)), auto sub () { return c822(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v6) {
            show(sprintf("string / *int (%s, %s)", vs(a), vs(b)), auto sub () { return c823(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v7) {
            show(sprintf("string / *float (%s, %s)", vs(a), vs(b)), auto sub () { return c824(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v8) {
            show(sprintf("string / *number (%s, %s)", vs(a), vs(b)), auto sub () { return c825(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v9) {
            show(sprintf("string / union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c826(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v10) {
            show(sprintf("string / union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c827(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v11) {
            show(sprintf("string / *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c828(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v12) {
            show(sprintf("string / list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c829(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v13) {
            show(sprintf("string / hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c830(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v14) {
            show(sprintf("string / binary (%s, %s)", vs(a), vs(b)), auto sub () { return c831(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v15) {
            show(sprintf("string / C (%s, %s)", vs(a), vs(b)), auto sub () { return c832(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v0) {
            show(sprintf("bool / int (%s, %s)", vs(a), vs(b)), auto sub () { return c833(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v1) {
            show(sprintf("bool / float (%s, %s)", vs(a), vs(b)), auto sub () { return c834(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v2) {
            show(sprintf("bool / number (%s, %s)", vs(a), vs(b)), auto sub () { return c835(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v3) {
            show(sprintf("bool / string (%s, %s)", vs(a), vs(b)), auto sub () { return c836(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v4) {
            show(sprintf("bool / bool (%s, %s)", vs(a), vs(b)), auto sub () { return c837(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v5) {
            show(sprintf("bool / date (%s, %s)", vs(a), vs(b)), auto sub () { return c838(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v6) {
            show(sprintf("bool / *int (%s, %s)", vs(a), vs(b)), auto sub () { return c839(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v7) {
            show(sprintf("bool / *float (%s, %s)", vs(a), vs(b)), auto sub () { return c840(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v8) {
            show(sprintf("bool / *number (%s, %s)", vs(a), vs(b)), auto sub () { return c841(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v9) {
            show(sprintf("bool / union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c842(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v10) {
            show(sprintf("bool / union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c843(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v11) {
            show(sprintf("bool / *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c844(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v12) {
            show(sprintf("bool / list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c845(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v13) {
            show(sprintf("bool / hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c846(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v14) {
            show(sprintf("bool / binary (%s, %s)", vs(a), vs(b)), auto sub () { return c847(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v15) {
            show(sprintf("bool / C (%s, %s)", vs(a), vs(b)), auto sub () { return c848(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v0) {
            show(sprintf("date / int (%s, %s)", vs(a), vs(b)), auto sub () { return c849(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v1) {
            show(sprintf("date / float (%s, %s)", vs(a), vs(b)), auto sub () { return c850(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v2) {
            show(sprintf("date / number (%s, %s)", vs(a), vs(b)), auto sub () { return c851(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v3) {
            show(sprintf("date / string (%s, %s)", vs(a), vs(b)), auto sub () { return c852(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v4) {
            show(sprintf("date / bool (%s, %s)", vs(a), vs(b)), auto sub () { return c853(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v5) {
            show(sprintf("date / date (%s, %s)", vs(a), vs(b)), auto sub () { return c854(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v6) {
            show(sprintf("date / *int (%s, %s)", vs(a), vs(b)), auto sub () { return c855(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v7) {
            show(sprintf("date / *float (%s, %s)", vs(a), vs(b)), auto sub () { return c856(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v8) {
            show(sprintf("date / *number (%s, %s)", vs(a), vs(b)), auto sub () { return c857(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v9) {
            show(sprintf("date / union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c858(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v10) {
            show(sprintf("date / union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c859(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v11) {
            show(sprintf("date / *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c860(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v12) {
            show(sprintf("date / list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c861(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v13) {
            show(sprintf("date / hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c862(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v14) {
            show(sprintf("date / binary (%s, %s)", vs(a), vs(b)), auto sub () { return c863(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v15) {
            show(sprintf("date / C (%s, %s)", vs(a), vs(b)), auto sub () { return c864(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v0) {
            show(sprintf("*int / int (%s, %s)", vs(a), vs(b)), auto sub () { return c865(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v1) {
            show(sprintf("*int / float (%s, %s)", vs(a), vs(b)), auto sub () { return c866(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v2) {
            show(sprintf("*int / number (%s, %s)", vs(a), vs(b)), auto sub () { return c867(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v3) {
            show(sprintf("*int / string (%s, %s)", vs(a), vs(b)), auto sub () { return c868(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v4) {
            show(sprintf("*int / bool (%s, %s)", vs(a), vs(b)), auto sub () { return c869(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v5) {
            show(sprintf("*int / date (%s, %s)", vs(a), vs(b)), auto sub () { return c870(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v6) {
            show(sprintf("*int / *int (%s, %s)", vs(a), vs(b)), auto sub () { return c871(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v7) {
            show(sprintf("*int / *float (%s, %s)", vs(a), vs(b)), auto sub () { return c872(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v8) {
            show(sprintf("*int / *number (%s, %s)", vs(a), vs(b)), auto sub () { return c873(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v9) {
            show(sprintf("*int / union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c874(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v10) {
            show(sprintf("*int / union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c875(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v11) {
            show(sprintf("*int / *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c876(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v12) {
            show(sprintf("*int / list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c877(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v13) {
            show(sprintf("*int / hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c878(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v14) {
            show(sprintf("*int / binary (%s, %s)", vs(a), vs(b)), auto sub () { return c879(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v15) {
            show(sprintf("*int / C (%s, %s)", vs(a), vs(b)), auto sub () { return c880(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v0) {
            show(sprintf("*float / int (%s, %s)", vs(a), vs(b)), auto sub () { return c881(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v1) {
            show(sprintf("*float / float (%s, %s)", vs(a), vs(b)), auto sub () { return c882(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v2) {
            show(sprintf("*float / number (%s, %s)", vs(a), vs(b)), auto sub () { return c883(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v3) {
            show(sprintf("*float / string (%s, %s)", vs(a), vs(b)), auto sub () { return c884(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v4) {
            show(sprintf("*float / bool (%s, %s)", vs(a), vs(b)), auto sub () { return c885(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v5) {
            show(sprintf("*float / date (%s, %s)", vs(a), vs(b)), auto sub () { return c886(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v6) {
            show(sprintf("*float / *int (%s, %s)", vs(a), vs(b)), auto sub () { return c887(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v7) {
            show(sprintf("*float / *float (%s, %s)", vs(a), vs(b)), auto sub () { return c888(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v8) {
            show(sprintf("*float / *number (%s, %s)", vs(a), vs(b)), auto sub () { return c889(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v9) {
            show(sprintf("*float / union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c890(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v10) {
            show(sprintf("*float / union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c891(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v11) {
            show(sprintf("*float / *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c892(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v12) {
            show(sprintf("*float / list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c893(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v13) {
            show(sprintf("*float / hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c894(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v14) {
            show(sprintf("*float / binary (%s, %s)", vs(a), vs(b)), auto sub () { return c895(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v15) {
            show(sprintf("*float / C (%s, %s)", vs(a), vs(b)), auto sub () { return c896(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v0) {
            show(sprintf("*number / int (%s, %s)", vs(a), vs(b)), auto sub () { return c897(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v1) {
            show(sprintf("*number / float (%s, %s)", vs(a), vs(b)), auto sub () { return c898(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v2) {
            show(sprintf("*number / number (%s, %s)", vs(a), vs(b)), auto sub () { return c899(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v3) {
            show(sprintf("*number / string (%s, %s)", vs(a), vs(b)), auto sub () { return c900(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v4) {
            show(sprintf("*number / bool (%s, %s)", vs(a), vs(b)), auto sub () { return c901(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v5) {
            show(sprintf("*number / date (%s, %s)", vs(a), vs(b)), auto sub () { return c902(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v6) {
            show(sprintf("*number / *int (%s, %s)", vs(a), vs(b)), auto sub () { return c903(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v7) {
            show(sprintf("*number / *float (%s, %s)", vs(a), vs(b)), auto sub () { return c904(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v8) {
            show(sprintf("*number / *number (%s, %s)", vs(a), vs(b)), auto sub () { return c905(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v9) {
            show(sprintf("*number / union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c906(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v10) {
            show(sprintf("*number / union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c907(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v11) {
            show(sprintf("*number / *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c908(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v12) {
            show(sprintf("*number / list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c909(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v13) {
            show(sprintf("*number / hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c910(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v14) {
            show(sprintf("*number / binary (%s, %s)", vs(a), vs(b)), auto sub () { return c911(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v15) {
            show(sprintf("*number / C (%s, %s)", vs(a), vs(b)), auto sub () { return c912(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v0) {
            show(sprintf("union<int, float> / int (%s, %s)", vs(a), vs(b)), auto sub () { return c913(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v1) {
            show(sprintf("union<int, float> / float (%s, %s)", vs(a), vs(b)), auto sub () { return c914(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v2) {
            show(sprintf("union<int, float> / number (%s, %s)", vs(a), vs(b)), auto sub () { return c915(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v3) {
            show(sprintf("union<int, float> / string (%s, %s)", vs(a), vs(b)), auto sub () { return c916(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v4) {
            show(sprintf("union<int, float> / bool (%s, %s)", vs(a), vs(b)), auto sub () { return c917(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v5) {
            show(sprintf("union<int, float> / date (%s, %s)", vs(a), vs(b)), auto sub () { return c918(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v6) {
            show(sprintf("union<int, float> / *int (%s, %s)", vs(a), vs(b)), auto sub () { return c919(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v7) {
            show(sprintf("union<int, float> / *float (%s, %s)", vs(a), vs(b)), auto sub () { return c920(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v8) {
            show(sprintf("union<int, float> / *number (%s, %s)", vs(a), vs(b)), auto sub () { return c921(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v9) {
            show(sprintf("union<int, float> / union<int, float> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c922(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v10) {
            show(sprintf("union<int, float> / union<int, string> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c923(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v11) {
            show(sprintf("union<int, float> / *union<int, number> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c924(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v12) {
            show(sprintf("union<int, float> / list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c925(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v13) {
            show(sprintf("union<int, float> / hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c926(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v14) {
            show(sprintf("union<int, float> / binary (%s, %s)", vs(a), vs(b)), auto sub () { return c927(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v15) {
            show(sprintf("union<int, float> / C (%s, %s)", vs(a), vs(b)), auto sub () { return c928(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v0) {
            show(sprintf("union<int, string> / int (%s, %s)", vs(a), vs(b)), auto sub () { return c929(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v1) {
            show(sprintf("union<int, string> / float (%s, %s)", vs(a), vs(b)), auto sub () { return c930(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v2) {
            show(sprintf("union<int, string> / number (%s, %s)", vs(a), vs(b)), auto sub () { return c931(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v3) {
            show(sprintf("union<int, string> / string (%s, %s)", vs(a), vs(b)), auto sub () { return c932(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v4) {
            show(sprintf("union<int, string> / bool (%s, %s)", vs(a), vs(b)), auto sub () { return c933(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v5) {
            show(sprintf("union<int, string> / date (%s, %s)", vs(a), vs(b)), auto sub () { return c934(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v6) {
            show(sprintf("union<int, string> / *int (%s, %s)", vs(a), vs(b)), auto sub () { return c935(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v7) {
            show(sprintf("union<int, string> / *float (%s, %s)", vs(a), vs(b)), auto sub () { return c936(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v8) {
            show(sprintf("union<int, string> / *number (%s, %s)", vs(a), vs(b)), auto sub () { return c937(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v9) {
            show(sprintf("union<int, string> / union<int, float> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c938(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v10) {
            show(sprintf("union<int, string> / union<int, string> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c939(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v11) {
            show(sprintf("union<int, string> / *union<int, number> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c940(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v12) {
            show(sprintf("union<int, string> / list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c941(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v13) {
            show(sprintf("union<int, string> / hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c942(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v14) {
            show(sprintf("union<int, string> / binary (%s, %s)", vs(a), vs(b)), auto sub () { return c943(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v15) {
            show(sprintf("union<int, string> / C (%s, %s)", vs(a), vs(b)), auto sub () { return c944(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v0) {
            show(sprintf("*union<int, number> / int (%s, %s)", vs(a), vs(b)), auto sub () { return c945(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v1) {
            show(sprintf("*union<int, number> / float (%s, %s)", vs(a), vs(b)), auto sub () { return c946(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v2) {
            show(sprintf("*union<int, number> / number (%s, %s)", vs(a), vs(b)), auto sub () { return c947(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v3) {
            show(sprintf("*union<int, number> / string (%s, %s)", vs(a), vs(b)), auto sub () { return c948(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v4) {
            show(sprintf("*union<int, number> / bool (%s, %s)", vs(a), vs(b)), auto sub () { return c949(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v5) {
            show(sprintf("*union<int, number> / date (%s, %s)", vs(a), vs(b)), auto sub () { return c950(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v6) {
            show(sprintf("*union<int, number> / *int (%s, %s)", vs(a), vs(b)), auto sub () { return c951(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v7) {
            show(sprintf("*union<int, number> / *float (%s, %s)", vs(a), vs(b)), auto sub () { return c952(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v8) {
            show(sprintf("*union<int, number> / *number (%s, %s)", vs(a), vs(b)), auto sub () { return c953(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v9) {
            show(sprintf("*union<int, number> / union<int, float> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c954(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v10) {
            show(sprintf("*union<int, number> / union<int, string> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c955(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v11) {
            show(sprintf("*union<int, number> / *union<int, number> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c956(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v12) {
            show(sprintf("*union<int, number> / list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c957(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v13) {
            show(sprintf("*union<int, number> / hash<auto> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c958(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v14) {
            show(sprintf("*union<int, number> / binary (%s, %s)", vs(a), vs(b)), auto sub () { return c959(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v15) {
            show(sprintf("*union<int, number> / C (%s, %s)", vs(a), vs(b)), auto sub () { return c960(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v0) {
            show(sprintf("list<int> / int (%s, %s)", vs(a), vs(b)), auto sub () { return c961(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v1) {
            show(sprintf("list<int> / float (%s, %s)", vs(a), vs(b)), auto sub () { return c962(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v2) {
            show(sprintf("list<int> / number (%s, %s)", vs(a), vs(b)), auto sub () { return c963(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v3) {
            show(sprintf("list<int> / string (%s, %s)", vs(a), vs(b)), auto sub () { return c964(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v4) {
            show(sprintf("list<int> / bool (%s, %s)", vs(a), vs(b)), auto sub () { return c965(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v5) {
            show(sprintf("list<int> / date (%s, %s)", vs(a), vs(b)), auto sub () { return c966(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v6) {
            show(sprintf("list<int> / *int (%s, %s)", vs(a), vs(b)), auto sub () { return c967(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v7) {
            show(sprintf("list<int> / *float (%s, %s)", vs(a), vs(b)), auto sub () { return c968(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v8) {
            show(sprintf("list<int> / *number (%s, %s)", vs(a), vs(b)), auto sub () { return c969(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v9) {
            show(sprintf("list<int> / union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c970(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v10) {
            show(sprintf("list<int> / union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c971(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v11) {
            show(sprintf("list<int> / *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c972(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v12) {
            show(sprintf("list<int> / list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c973(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v13) {
            show(sprintf("list<int> / hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c974(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v14) {
            show(sprintf("list<int> / binary (%s, %s)", vs(a), vs(b)), auto sub () { return c975(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v15) {
            show(sprintf("list<int> / C (%s, %s)", vs(a), vs(b)), auto sub () { return c976(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v0) {
            show(sprintf("hash<auto> / int (%s, %s)", vs(a), vs(b)), auto sub () { return c977(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v1) {
            show(sprintf("hash<auto> / float (%s, %s)", vs(a), vs(b)), auto sub () { return c978(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v2) {
            show(sprintf("hash<auto> / number (%s, %s)", vs(a), vs(b)), auto sub () { return c979(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v3) {
            show(sprintf("hash<auto> / string (%s, %s)", vs(a), vs(b)), auto sub () { return c980(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v4) {
            show(sprintf("hash<auto> / bool (%s, %s)", vs(a), vs(b)), auto sub () { return c981(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v5) {
            show(sprintf("hash<auto> / date (%s, %s)", vs(a), vs(b)), auto sub () { return c982(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v6) {
            show(sprintf("hash<auto> / *int (%s, %s)", vs(a), vs(b)), auto sub () { return c983(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v7) {
            show(sprintf("hash<auto> / *float (%s, %s)", vs(a), vs(b)), auto sub () { return c984(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v8) {
            show(sprintf("hash<auto> / *number (%s, %s)", vs(a), vs(b)), auto sub () { return c985(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v9) {
            show(sprintf("hash<auto> / union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c986(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v10) {
            show(sprintf("hash<auto> / union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c987(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v11) {
            show(sprintf("hash<auto> / *union<int, number> (%s, %s)", vs(a), vs(b)),
                auto sub () { return c988(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v12) {
            show(sprintf("hash<auto> / list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c989(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v13) {
            show(sprintf("hash<auto> / hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c990(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v14) {
            show(sprintf("hash<auto> / binary (%s, %s)", vs(a), vs(b)), auto sub () { return c991(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v15) {
            show(sprintf("hash<auto> / C (%s, %s)", vs(a), vs(b)), auto sub () { return c992(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v0) {
            show(sprintf("binary / int (%s, %s)", vs(a), vs(b)), auto sub () { return c993(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v1) {
            show(sprintf("binary / float (%s, %s)", vs(a), vs(b)), auto sub () { return c994(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v2) {
            show(sprintf("binary / number (%s, %s)", vs(a), vs(b)), auto sub () { return c995(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v3) {
            show(sprintf("binary / string (%s, %s)", vs(a), vs(b)), auto sub () { return c996(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v4) {
            show(sprintf("binary / bool (%s, %s)", vs(a), vs(b)), auto sub () { return c997(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v5) {
            show(sprintf("binary / date (%s, %s)", vs(a), vs(b)), auto sub () { return c998(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v6) {
            show(sprintf("binary / *int (%s, %s)", vs(a), vs(b)), auto sub () { return c999(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v7) {
            show(sprintf("binary / *float (%s, %s)", vs(a), vs(b)), auto sub () { return c1000(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v8) {
            show(sprintf("binary / *number (%s, %s)", vs(a), vs(b)), auto sub () { return c1001(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v9) {
            show(sprintf("binary / union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c1002(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v10) {
            show(sprintf("binary / union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c1003(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v11) {
            show(sprintf("binary / *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c1004(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v12) {
            show(sprintf("binary / list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c1005(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v13) {
            show(sprintf("binary / hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c1006(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v14) {
            show(sprintf("binary / binary (%s, %s)", vs(a), vs(b)), auto sub () { return c1007(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v15) {
            show(sprintf("binary / C (%s, %s)", vs(a), vs(b)), auto sub () { return c1008(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v0) {
            show(sprintf("C / int (%s, %s)", vs(a), vs(b)), auto sub () { return c1009(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v1) {
            show(sprintf("C / float (%s, %s)", vs(a), vs(b)), auto sub () { return c1010(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v2) {
            show(sprintf("C / number (%s, %s)", vs(a), vs(b)), auto sub () { return c1011(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v3) {
            show(sprintf("C / string (%s, %s)", vs(a), vs(b)), auto sub () { return c1012(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v4) {
            show(sprintf("C / bool (%s, %s)", vs(a), vs(b)), auto sub () { return c1013(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v5) {
            show(sprintf("C / date (%s, %s)", vs(a), vs(b)), auto sub () { return c1014(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v6) {
            show(sprintf("C / *int (%s, %s)", vs(a), vs(b)), auto sub () { return c1015(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v7) {
            show(sprintf("C / *float (%s, %s)", vs(a), vs(b)), auto sub () { return c1016(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v8) {
            show(sprintf("C / *number (%s, %s)", vs(a), vs(b)), auto sub () { return c1017(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v9) {
            show(sprintf("C / union<int, float> (%s, %s)", vs(a), vs(b)), auto sub () { return c1018(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v10) {
            show(sprintf("C / union<int, string> (%s, %s)", vs(a), vs(b)), auto sub () { return c1019(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v11) {
            show(sprintf("C / *union<int, number> (%s, %s)", vs(a), vs(b)), auto sub () { return c1020(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v12) {
            show(sprintf("C / list<int> (%s, %s)", vs(a), vs(b)), auto sub () { return c1021(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v13) {
            show(sprintf("C / hash<auto> (%s, %s)", vs(a), vs(b)), auto sub () { return c1022(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v14) {
            show(sprintf("C / binary (%s, %s)", vs(a), vs(b)), auto sub () { return c1023(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v15) {
            show(sprintf("C / C (%s, %s)", vs(a), vs(b)), auto sub () { return c1024(a, b); });
        }
    }
}

main();
