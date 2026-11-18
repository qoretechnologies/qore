#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the cases of UnionComparisons.qc, parts 1 and 2 (generated for the operand types and values below): each function
# compares parameters of declared types (which are not narrowed) with a comparison operator; each case is called with
# values of every type that the declared types allow; the output of every execution mode must be that of the AST
# interpreter

%modern

sub show(string label, code<auto()> c) {
    try {
        printf("%s => %y\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s => %s\n", label, ex.err);
    }
}

auto sub c1(int x, int y) {
    return x == y;
}
auto sub c2(int x, float y) {
    return x == y;
}
auto sub c3(int x, number y) {
    return x == y;
}
auto sub c4(int x, *number y) {
    return x == y;
}
auto sub c5(int x, union<int, float> y) {
    return x == y;
}
auto sub c6(int x, union<int, number> y) {
    return x == y;
}
auto sub c7(int x, *union<int, float> y) {
    return x == y;
}
auto sub c8(int x, union<int, bool> y) {
    return x == y;
}
auto sub c9(int x, union<int, string> y) {
    return x == y;
}
auto sub c10(float x, int y) {
    return x == y;
}
auto sub c11(float x, float y) {
    return x == y;
}
auto sub c12(float x, number y) {
    return x == y;
}
auto sub c13(float x, *number y) {
    return x == y;
}
auto sub c14(float x, union<int, float> y) {
    return x == y;
}
auto sub c15(float x, union<int, number> y) {
    return x == y;
}
auto sub c16(float x, *union<int, float> y) {
    return x == y;
}
auto sub c17(float x, union<int, bool> y) {
    return x == y;
}
auto sub c18(float x, union<int, string> y) {
    return x == y;
}
auto sub c19(number x, int y) {
    return x == y;
}
auto sub c20(number x, float y) {
    return x == y;
}
auto sub c21(number x, number y) {
    return x == y;
}
auto sub c22(number x, *number y) {
    return x == y;
}
auto sub c23(number x, union<int, float> y) {
    return x == y;
}
auto sub c24(number x, union<int, number> y) {
    return x == y;
}
auto sub c25(number x, *union<int, float> y) {
    return x == y;
}
auto sub c26(number x, union<int, bool> y) {
    return x == y;
}
auto sub c27(number x, union<int, string> y) {
    return x == y;
}
auto sub c28(*number x, int y) {
    return x == y;
}
auto sub c29(*number x, float y) {
    return x == y;
}
auto sub c30(*number x, number y) {
    return x == y;
}
auto sub c31(*number x, *number y) {
    return x == y;
}
auto sub c32(*number x, union<int, float> y) {
    return x == y;
}
auto sub c33(*number x, union<int, number> y) {
    return x == y;
}
auto sub c34(*number x, *union<int, float> y) {
    return x == y;
}
auto sub c35(*number x, union<int, bool> y) {
    return x == y;
}
auto sub c36(*number x, union<int, string> y) {
    return x == y;
}
auto sub c37(union<int, float> x, int y) {
    return x == y;
}
auto sub c38(union<int, float> x, float y) {
    return x == y;
}
auto sub c39(union<int, float> x, number y) {
    return x == y;
}
auto sub c40(union<int, float> x, *number y) {
    return x == y;
}
auto sub c41(union<int, float> x, union<int, float> y) {
    return x == y;
}
auto sub c42(union<int, float> x, union<int, number> y) {
    return x == y;
}
auto sub c43(union<int, float> x, *union<int, float> y) {
    return x == y;
}
auto sub c44(union<int, float> x, union<int, bool> y) {
    return x == y;
}
auto sub c45(union<int, float> x, union<int, string> y) {
    return x == y;
}
auto sub c46(union<int, number> x, int y) {
    return x == y;
}
auto sub c47(union<int, number> x, float y) {
    return x == y;
}
auto sub c48(union<int, number> x, number y) {
    return x == y;
}
auto sub c49(union<int, number> x, *number y) {
    return x == y;
}
auto sub c50(union<int, number> x, union<int, float> y) {
    return x == y;
}
auto sub c51(union<int, number> x, union<int, number> y) {
    return x == y;
}
auto sub c52(union<int, number> x, *union<int, float> y) {
    return x == y;
}
auto sub c53(union<int, number> x, union<int, bool> y) {
    return x == y;
}
auto sub c54(union<int, number> x, union<int, string> y) {
    return x == y;
}
auto sub c55(*union<int, float> x, int y) {
    return x == y;
}
auto sub c56(*union<int, float> x, float y) {
    return x == y;
}
auto sub c57(*union<int, float> x, number y) {
    return x == y;
}
auto sub c58(*union<int, float> x, *number y) {
    return x == y;
}
auto sub c59(*union<int, float> x, union<int, float> y) {
    return x == y;
}
auto sub c60(*union<int, float> x, union<int, number> y) {
    return x == y;
}
auto sub c61(*union<int, float> x, *union<int, float> y) {
    return x == y;
}
auto sub c62(*union<int, float> x, union<int, bool> y) {
    return x == y;
}
auto sub c63(*union<int, float> x, union<int, string> y) {
    return x == y;
}
auto sub c64(union<int, bool> x, int y) {
    return x == y;
}
auto sub c65(union<int, bool> x, float y) {
    return x == y;
}
auto sub c66(union<int, bool> x, number y) {
    return x == y;
}
auto sub c67(union<int, bool> x, *number y) {
    return x == y;
}
auto sub c68(union<int, bool> x, union<int, float> y) {
    return x == y;
}
auto sub c69(union<int, bool> x, union<int, number> y) {
    return x == y;
}
auto sub c70(union<int, bool> x, *union<int, float> y) {
    return x == y;
}
auto sub c71(union<int, bool> x, union<int, bool> y) {
    return x == y;
}
auto sub c72(union<int, bool> x, union<int, string> y) {
    return x == y;
}
auto sub c73(union<int, string> x, int y) {
    return x == y;
}
auto sub c74(union<int, string> x, float y) {
    return x == y;
}
auto sub c75(union<int, string> x, number y) {
    return x == y;
}
auto sub c76(union<int, string> x, *number y) {
    return x == y;
}
auto sub c77(union<int, string> x, union<int, float> y) {
    return x == y;
}
auto sub c78(union<int, string> x, union<int, number> y) {
    return x == y;
}
auto sub c79(union<int, string> x, *union<int, float> y) {
    return x == y;
}
auto sub c80(union<int, string> x, union<int, bool> y) {
    return x == y;
}
auto sub c81(union<int, string> x, union<int, string> y) {
    return x == y;
}
auto sub c82(int x, int y) {
    return x != y;
}
auto sub c83(int x, float y) {
    return x != y;
}
auto sub c84(int x, number y) {
    return x != y;
}
auto sub c85(int x, *number y) {
    return x != y;
}
auto sub c86(int x, union<int, float> y) {
    return x != y;
}
auto sub c87(int x, union<int, number> y) {
    return x != y;
}
auto sub c88(int x, *union<int, float> y) {
    return x != y;
}
auto sub c89(int x, union<int, bool> y) {
    return x != y;
}
auto sub c90(int x, union<int, string> y) {
    return x != y;
}
auto sub c91(float x, int y) {
    return x != y;
}
auto sub c92(float x, float y) {
    return x != y;
}
auto sub c93(float x, number y) {
    return x != y;
}
auto sub c94(float x, *number y) {
    return x != y;
}
auto sub c95(float x, union<int, float> y) {
    return x != y;
}
auto sub c96(float x, union<int, number> y) {
    return x != y;
}
auto sub c97(float x, *union<int, float> y) {
    return x != y;
}
auto sub c98(float x, union<int, bool> y) {
    return x != y;
}
auto sub c99(float x, union<int, string> y) {
    return x != y;
}
auto sub c100(number x, int y) {
    return x != y;
}
auto sub c101(number x, float y) {
    return x != y;
}
auto sub c102(number x, number y) {
    return x != y;
}
auto sub c103(number x, *number y) {
    return x != y;
}
auto sub c104(number x, union<int, float> y) {
    return x != y;
}
auto sub c105(number x, union<int, number> y) {
    return x != y;
}
auto sub c106(number x, *union<int, float> y) {
    return x != y;
}
auto sub c107(number x, union<int, bool> y) {
    return x != y;
}
auto sub c108(number x, union<int, string> y) {
    return x != y;
}
auto sub c109(*number x, int y) {
    return x != y;
}
auto sub c110(*number x, float y) {
    return x != y;
}
auto sub c111(*number x, number y) {
    return x != y;
}
auto sub c112(*number x, *number y) {
    return x != y;
}
auto sub c113(*number x, union<int, float> y) {
    return x != y;
}
auto sub c114(*number x, union<int, number> y) {
    return x != y;
}
auto sub c115(*number x, *union<int, float> y) {
    return x != y;
}
auto sub c116(*number x, union<int, bool> y) {
    return x != y;
}
auto sub c117(*number x, union<int, string> y) {
    return x != y;
}
auto sub c118(union<int, float> x, int y) {
    return x != y;
}
auto sub c119(union<int, float> x, float y) {
    return x != y;
}
auto sub c120(union<int, float> x, number y) {
    return x != y;
}
auto sub c121(union<int, float> x, *number y) {
    return x != y;
}
auto sub c122(union<int, float> x, union<int, float> y) {
    return x != y;
}
auto sub c123(union<int, float> x, union<int, number> y) {
    return x != y;
}
auto sub c124(union<int, float> x, *union<int, float> y) {
    return x != y;
}
auto sub c125(union<int, float> x, union<int, bool> y) {
    return x != y;
}
auto sub c126(union<int, float> x, union<int, string> y) {
    return x != y;
}
auto sub c127(union<int, number> x, int y) {
    return x != y;
}
auto sub c128(union<int, number> x, float y) {
    return x != y;
}
auto sub c129(union<int, number> x, number y) {
    return x != y;
}
auto sub c130(union<int, number> x, *number y) {
    return x != y;
}
auto sub c131(union<int, number> x, union<int, float> y) {
    return x != y;
}
auto sub c132(union<int, number> x, union<int, number> y) {
    return x != y;
}
auto sub c133(union<int, number> x, *union<int, float> y) {
    return x != y;
}
auto sub c134(union<int, number> x, union<int, bool> y) {
    return x != y;
}
auto sub c135(union<int, number> x, union<int, string> y) {
    return x != y;
}
auto sub c136(*union<int, float> x, int y) {
    return x != y;
}
auto sub c137(*union<int, float> x, float y) {
    return x != y;
}
auto sub c138(*union<int, float> x, number y) {
    return x != y;
}
auto sub c139(*union<int, float> x, *number y) {
    return x != y;
}
auto sub c140(*union<int, float> x, union<int, float> y) {
    return x != y;
}
auto sub c141(*union<int, float> x, union<int, number> y) {
    return x != y;
}
auto sub c142(*union<int, float> x, *union<int, float> y) {
    return x != y;
}
auto sub c143(*union<int, float> x, union<int, bool> y) {
    return x != y;
}
auto sub c144(*union<int, float> x, union<int, string> y) {
    return x != y;
}
auto sub c145(union<int, bool> x, int y) {
    return x != y;
}
auto sub c146(union<int, bool> x, float y) {
    return x != y;
}
auto sub c147(union<int, bool> x, number y) {
    return x != y;
}
auto sub c148(union<int, bool> x, *number y) {
    return x != y;
}
auto sub c149(union<int, bool> x, union<int, float> y) {
    return x != y;
}
auto sub c150(union<int, bool> x, union<int, number> y) {
    return x != y;
}
auto sub c151(union<int, bool> x, *union<int, float> y) {
    return x != y;
}
auto sub c152(union<int, bool> x, union<int, bool> y) {
    return x != y;
}
auto sub c153(union<int, bool> x, union<int, string> y) {
    return x != y;
}
auto sub c154(union<int, string> x, int y) {
    return x != y;
}
auto sub c155(union<int, string> x, float y) {
    return x != y;
}
auto sub c156(union<int, string> x, number y) {
    return x != y;
}
auto sub c157(union<int, string> x, *number y) {
    return x != y;
}
auto sub c158(union<int, string> x, union<int, float> y) {
    return x != y;
}
auto sub c159(union<int, string> x, union<int, number> y) {
    return x != y;
}
auto sub c160(union<int, string> x, *union<int, float> y) {
    return x != y;
}
auto sub c161(union<int, string> x, union<int, bool> y) {
    return x != y;
}
auto sub c162(union<int, string> x, union<int, string> y) {
    return x != y;
}
auto sub c163(int x, int y) {
    return x < y;
}
auto sub c164(int x, float y) {
    return x < y;
}
auto sub c165(int x, number y) {
    return x < y;
}
auto sub c166(int x, *number y) {
    return x < y;
}
auto sub c167(int x, union<int, float> y) {
    return x < y;
}
auto sub c168(int x, union<int, number> y) {
    return x < y;
}
auto sub c169(int x, *union<int, float> y) {
    return x < y;
}
auto sub c170(int x, union<int, bool> y) {
    return x < y;
}
auto sub c171(int x, union<int, string> y) {
    return x < y;
}
auto sub c172(float x, int y) {
    return x < y;
}
auto sub c173(float x, float y) {
    return x < y;
}
auto sub c174(float x, number y) {
    return x < y;
}
auto sub c175(float x, *number y) {
    return x < y;
}
auto sub c176(float x, union<int, float> y) {
    return x < y;
}
auto sub c177(float x, union<int, number> y) {
    return x < y;
}
auto sub c178(float x, *union<int, float> y) {
    return x < y;
}
auto sub c179(float x, union<int, bool> y) {
    return x < y;
}
auto sub c180(float x, union<int, string> y) {
    return x < y;
}
auto sub c181(number x, int y) {
    return x < y;
}
auto sub c182(number x, float y) {
    return x < y;
}
auto sub c183(number x, number y) {
    return x < y;
}
auto sub c184(number x, *number y) {
    return x < y;
}
auto sub c185(number x, union<int, float> y) {
    return x < y;
}
auto sub c186(number x, union<int, number> y) {
    return x < y;
}
auto sub c187(number x, *union<int, float> y) {
    return x < y;
}
auto sub c188(number x, union<int, bool> y) {
    return x < y;
}
auto sub c189(number x, union<int, string> y) {
    return x < y;
}
auto sub c190(*number x, int y) {
    return x < y;
}
auto sub c191(*number x, float y) {
    return x < y;
}
auto sub c192(*number x, number y) {
    return x < y;
}
auto sub c193(*number x, *number y) {
    return x < y;
}
auto sub c194(*number x, union<int, float> y) {
    return x < y;
}
auto sub c195(*number x, union<int, number> y) {
    return x < y;
}
auto sub c196(*number x, *union<int, float> y) {
    return x < y;
}
auto sub c197(*number x, union<int, bool> y) {
    return x < y;
}
auto sub c198(*number x, union<int, string> y) {
    return x < y;
}
auto sub c199(union<int, float> x, int y) {
    return x < y;
}
auto sub c200(union<int, float> x, float y) {
    return x < y;
}
auto sub c201(union<int, float> x, number y) {
    return x < y;
}
auto sub c202(union<int, float> x, *number y) {
    return x < y;
}
auto sub c203(union<int, float> x, union<int, float> y) {
    return x < y;
}
auto sub c204(union<int, float> x, union<int, number> y) {
    return x < y;
}
auto sub c205(union<int, float> x, *union<int, float> y) {
    return x < y;
}
auto sub c206(union<int, float> x, union<int, bool> y) {
    return x < y;
}
auto sub c207(union<int, float> x, union<int, string> y) {
    return x < y;
}
auto sub c208(union<int, number> x, int y) {
    return x < y;
}
auto sub c209(union<int, number> x, float y) {
    return x < y;
}
auto sub c210(union<int, number> x, number y) {
    return x < y;
}
auto sub c211(union<int, number> x, *number y) {
    return x < y;
}
auto sub c212(union<int, number> x, union<int, float> y) {
    return x < y;
}
auto sub c213(union<int, number> x, union<int, number> y) {
    return x < y;
}
auto sub c214(union<int, number> x, *union<int, float> y) {
    return x < y;
}
auto sub c215(union<int, number> x, union<int, bool> y) {
    return x < y;
}
auto sub c216(union<int, number> x, union<int, string> y) {
    return x < y;
}
auto sub c217(*union<int, float> x, int y) {
    return x < y;
}
auto sub c218(*union<int, float> x, float y) {
    return x < y;
}
auto sub c219(*union<int, float> x, number y) {
    return x < y;
}
auto sub c220(*union<int, float> x, *number y) {
    return x < y;
}
auto sub c221(*union<int, float> x, union<int, float> y) {
    return x < y;
}
auto sub c222(*union<int, float> x, union<int, number> y) {
    return x < y;
}
auto sub c223(*union<int, float> x, *union<int, float> y) {
    return x < y;
}
auto sub c224(*union<int, float> x, union<int, bool> y) {
    return x < y;
}
auto sub c225(*union<int, float> x, union<int, string> y) {
    return x < y;
}
auto sub c226(union<int, bool> x, int y) {
    return x < y;
}
auto sub c227(union<int, bool> x, float y) {
    return x < y;
}
auto sub c228(union<int, bool> x, number y) {
    return x < y;
}
auto sub c229(union<int, bool> x, *number y) {
    return x < y;
}
auto sub c230(union<int, bool> x, union<int, float> y) {
    return x < y;
}
auto sub c231(union<int, bool> x, union<int, number> y) {
    return x < y;
}
auto sub c232(union<int, bool> x, *union<int, float> y) {
    return x < y;
}
auto sub c233(union<int, bool> x, union<int, bool> y) {
    return x < y;
}
auto sub c234(union<int, bool> x, union<int, string> y) {
    return x < y;
}
auto sub c235(union<int, string> x, int y) {
    return x < y;
}
auto sub c236(union<int, string> x, float y) {
    return x < y;
}
auto sub c237(union<int, string> x, number y) {
    return x < y;
}
auto sub c238(union<int, string> x, *number y) {
    return x < y;
}
auto sub c239(union<int, string> x, union<int, float> y) {
    return x < y;
}
auto sub c240(union<int, string> x, union<int, number> y) {
    return x < y;
}
auto sub c241(union<int, string> x, *union<int, float> y) {
    return x < y;
}
auto sub c242(union<int, string> x, union<int, bool> y) {
    return x < y;
}
auto sub c243(union<int, string> x, union<int, string> y) {
    return x < y;
}
auto sub c244(int x, int y) {
    return x <= y;
}
auto sub c245(int x, float y) {
    return x <= y;
}
auto sub c246(int x, number y) {
    return x <= y;
}
auto sub c247(int x, *number y) {
    return x <= y;
}
auto sub c248(int x, union<int, float> y) {
    return x <= y;
}
auto sub c249(int x, union<int, number> y) {
    return x <= y;
}
auto sub c250(int x, *union<int, float> y) {
    return x <= y;
}
auto sub c251(int x, union<int, bool> y) {
    return x <= y;
}
auto sub c252(int x, union<int, string> y) {
    return x <= y;
}
auto sub c253(float x, int y) {
    return x <= y;
}
auto sub c254(float x, float y) {
    return x <= y;
}
auto sub c255(float x, number y) {
    return x <= y;
}
auto sub c256(float x, *number y) {
    return x <= y;
}
auto sub c257(float x, union<int, float> y) {
    return x <= y;
}
auto sub c258(float x, union<int, number> y) {
    return x <= y;
}
auto sub c259(float x, *union<int, float> y) {
    return x <= y;
}
auto sub c260(float x, union<int, bool> y) {
    return x <= y;
}
auto sub c261(float x, union<int, string> y) {
    return x <= y;
}
auto sub c262(number x, int y) {
    return x <= y;
}
auto sub c263(number x, float y) {
    return x <= y;
}
auto sub c264(number x, number y) {
    return x <= y;
}
auto sub c265(number x, *number y) {
    return x <= y;
}
auto sub c266(number x, union<int, float> y) {
    return x <= y;
}
auto sub c267(number x, union<int, number> y) {
    return x <= y;
}
auto sub c268(number x, *union<int, float> y) {
    return x <= y;
}
auto sub c269(number x, union<int, bool> y) {
    return x <= y;
}
auto sub c270(number x, union<int, string> y) {
    return x <= y;
}
auto sub c271(*number x, int y) {
    return x <= y;
}
auto sub c272(*number x, float y) {
    return x <= y;
}
auto sub c273(*number x, number y) {
    return x <= y;
}
auto sub c274(*number x, *number y) {
    return x <= y;
}
auto sub c275(*number x, union<int, float> y) {
    return x <= y;
}
auto sub c276(*number x, union<int, number> y) {
    return x <= y;
}
auto sub c277(*number x, *union<int, float> y) {
    return x <= y;
}
auto sub c278(*number x, union<int, bool> y) {
    return x <= y;
}
auto sub c279(*number x, union<int, string> y) {
    return x <= y;
}
auto sub c280(union<int, float> x, int y) {
    return x <= y;
}
auto sub c281(union<int, float> x, float y) {
    return x <= y;
}
auto sub c282(union<int, float> x, number y) {
    return x <= y;
}
auto sub c283(union<int, float> x, *number y) {
    return x <= y;
}
auto sub c284(union<int, float> x, union<int, float> y) {
    return x <= y;
}
auto sub c285(union<int, float> x, union<int, number> y) {
    return x <= y;
}
auto sub c286(union<int, float> x, *union<int, float> y) {
    return x <= y;
}
auto sub c287(union<int, float> x, union<int, bool> y) {
    return x <= y;
}
auto sub c288(union<int, float> x, union<int, string> y) {
    return x <= y;
}
auto sub c289(union<int, number> x, int y) {
    return x <= y;
}
auto sub c290(union<int, number> x, float y) {
    return x <= y;
}
auto sub c291(union<int, number> x, number y) {
    return x <= y;
}
auto sub c292(union<int, number> x, *number y) {
    return x <= y;
}
auto sub c293(union<int, number> x, union<int, float> y) {
    return x <= y;
}
auto sub c294(union<int, number> x, union<int, number> y) {
    return x <= y;
}
auto sub c295(union<int, number> x, *union<int, float> y) {
    return x <= y;
}
auto sub c296(union<int, number> x, union<int, bool> y) {
    return x <= y;
}
auto sub c297(union<int, number> x, union<int, string> y) {
    return x <= y;
}
auto sub c298(*union<int, float> x, int y) {
    return x <= y;
}
auto sub c299(*union<int, float> x, float y) {
    return x <= y;
}
auto sub c300(*union<int, float> x, number y) {
    return x <= y;
}
auto sub c301(*union<int, float> x, *number y) {
    return x <= y;
}
auto sub c302(*union<int, float> x, union<int, float> y) {
    return x <= y;
}
auto sub c303(*union<int, float> x, union<int, number> y) {
    return x <= y;
}
auto sub c304(*union<int, float> x, *union<int, float> y) {
    return x <= y;
}
auto sub c305(*union<int, float> x, union<int, bool> y) {
    return x <= y;
}
auto sub c306(*union<int, float> x, union<int, string> y) {
    return x <= y;
}
auto sub c307(union<int, bool> x, int y) {
    return x <= y;
}
auto sub c308(union<int, bool> x, float y) {
    return x <= y;
}
auto sub c309(union<int, bool> x, number y) {
    return x <= y;
}
auto sub c310(union<int, bool> x, *number y) {
    return x <= y;
}
auto sub c311(union<int, bool> x, union<int, float> y) {
    return x <= y;
}
auto sub c312(union<int, bool> x, union<int, number> y) {
    return x <= y;
}
auto sub c313(union<int, bool> x, *union<int, float> y) {
    return x <= y;
}
auto sub c314(union<int, bool> x, union<int, bool> y) {
    return x <= y;
}
auto sub c315(union<int, bool> x, union<int, string> y) {
    return x <= y;
}
auto sub c316(union<int, string> x, int y) {
    return x <= y;
}
auto sub c317(union<int, string> x, float y) {
    return x <= y;
}
auto sub c318(union<int, string> x, number y) {
    return x <= y;
}
auto sub c319(union<int, string> x, *number y) {
    return x <= y;
}
auto sub c320(union<int, string> x, union<int, float> y) {
    return x <= y;
}
auto sub c321(union<int, string> x, union<int, number> y) {
    return x <= y;
}
auto sub c322(union<int, string> x, *union<int, float> y) {
    return x <= y;
}
auto sub c323(union<int, string> x, union<int, bool> y) {
    return x <= y;
}
auto sub c324(union<int, string> x, union<int, string> y) {
    return x <= y;
}
auto sub c325(int x, int y) {
    return x > y;
}
auto sub c326(int x, float y) {
    return x > y;
}
auto sub c327(int x, number y) {
    return x > y;
}
auto sub c328(int x, *number y) {
    return x > y;
}
auto sub c329(int x, union<int, float> y) {
    return x > y;
}
auto sub c330(int x, union<int, number> y) {
    return x > y;
}
auto sub c331(int x, *union<int, float> y) {
    return x > y;
}
auto sub c332(int x, union<int, bool> y) {
    return x > y;
}
auto sub c333(int x, union<int, string> y) {
    return x > y;
}
auto sub c334(float x, int y) {
    return x > y;
}
auto sub c335(float x, float y) {
    return x > y;
}
auto sub c336(float x, number y) {
    return x > y;
}
auto sub c337(float x, *number y) {
    return x > y;
}
auto sub c338(float x, union<int, float> y) {
    return x > y;
}
auto sub c339(float x, union<int, number> y) {
    return x > y;
}
auto sub c340(float x, *union<int, float> y) {
    return x > y;
}
auto sub c341(float x, union<int, bool> y) {
    return x > y;
}
auto sub c342(float x, union<int, string> y) {
    return x > y;
}
auto sub c343(number x, int y) {
    return x > y;
}
auto sub c344(number x, float y) {
    return x > y;
}
auto sub c345(number x, number y) {
    return x > y;
}
auto sub c346(number x, *number y) {
    return x > y;
}
auto sub c347(number x, union<int, float> y) {
    return x > y;
}
auto sub c348(number x, union<int, number> y) {
    return x > y;
}
auto sub c349(number x, *union<int, float> y) {
    return x > y;
}
auto sub c350(number x, union<int, bool> y) {
    return x > y;
}
auto sub c351(number x, union<int, string> y) {
    return x > y;
}
auto sub c352(*number x, int y) {
    return x > y;
}
auto sub c353(*number x, float y) {
    return x > y;
}
auto sub c354(*number x, number y) {
    return x > y;
}
auto sub c355(*number x, *number y) {
    return x > y;
}
auto sub c356(*number x, union<int, float> y) {
    return x > y;
}
auto sub c357(*number x, union<int, number> y) {
    return x > y;
}
auto sub c358(*number x, *union<int, float> y) {
    return x > y;
}
auto sub c359(*number x, union<int, bool> y) {
    return x > y;
}
auto sub c360(*number x, union<int, string> y) {
    return x > y;
}
auto sub c361(union<int, float> x, int y) {
    return x > y;
}
auto sub c362(union<int, float> x, float y) {
    return x > y;
}
auto sub c363(union<int, float> x, number y) {
    return x > y;
}
auto sub c364(union<int, float> x, *number y) {
    return x > y;
}
auto sub c365(union<int, float> x, union<int, float> y) {
    return x > y;
}
auto sub c366(union<int, float> x, union<int, number> y) {
    return x > y;
}
auto sub c367(union<int, float> x, *union<int, float> y) {
    return x > y;
}
auto sub c368(union<int, float> x, union<int, bool> y) {
    return x > y;
}
auto sub c369(union<int, float> x, union<int, string> y) {
    return x > y;
}
auto sub c370(union<int, number> x, int y) {
    return x > y;
}
auto sub c371(union<int, number> x, float y) {
    return x > y;
}
auto sub c372(union<int, number> x, number y) {
    return x > y;
}
auto sub c373(union<int, number> x, *number y) {
    return x > y;
}
auto sub c374(union<int, number> x, union<int, float> y) {
    return x > y;
}
auto sub c375(union<int, number> x, union<int, number> y) {
    return x > y;
}
auto sub c376(union<int, number> x, *union<int, float> y) {
    return x > y;
}
auto sub c377(union<int, number> x, union<int, bool> y) {
    return x > y;
}
auto sub c378(union<int, number> x, union<int, string> y) {
    return x > y;
}
auto sub c379(*union<int, float> x, int y) {
    return x > y;
}
auto sub c380(*union<int, float> x, float y) {
    return x > y;
}
auto sub c381(*union<int, float> x, number y) {
    return x > y;
}
auto sub c382(*union<int, float> x, *number y) {
    return x > y;
}
auto sub c383(*union<int, float> x, union<int, float> y) {
    return x > y;
}
auto sub c384(*union<int, float> x, union<int, number> y) {
    return x > y;
}
auto sub c385(*union<int, float> x, *union<int, float> y) {
    return x > y;
}
auto sub c386(*union<int, float> x, union<int, bool> y) {
    return x > y;
}
auto sub c387(*union<int, float> x, union<int, string> y) {
    return x > y;
}
auto sub c388(union<int, bool> x, int y) {
    return x > y;
}
auto sub c389(union<int, bool> x, float y) {
    return x > y;
}
auto sub c390(union<int, bool> x, number y) {
    return x > y;
}
auto sub c391(union<int, bool> x, *number y) {
    return x > y;
}
auto sub c392(union<int, bool> x, union<int, float> y) {
    return x > y;
}
auto sub c393(union<int, bool> x, union<int, number> y) {
    return x > y;
}
auto sub c394(union<int, bool> x, *union<int, float> y) {
    return x > y;
}
auto sub c395(union<int, bool> x, union<int, bool> y) {
    return x > y;
}
auto sub c396(union<int, bool> x, union<int, string> y) {
    return x > y;
}
auto sub c397(union<int, string> x, int y) {
    return x > y;
}
auto sub c398(union<int, string> x, float y) {
    return x > y;
}
auto sub c399(union<int, string> x, number y) {
    return x > y;
}
auto sub c400(union<int, string> x, *number y) {
    return x > y;
}
auto sub c401(union<int, string> x, union<int, float> y) {
    return x > y;
}
auto sub c402(union<int, string> x, union<int, number> y) {
    return x > y;
}
auto sub c403(union<int, string> x, *union<int, float> y) {
    return x > y;
}
auto sub c404(union<int, string> x, union<int, bool> y) {
    return x > y;
}
auto sub c405(union<int, string> x, union<int, string> y) {
    return x > y;
}
auto sub c406(int x, int y) {
    return x >= y;
}
auto sub c407(int x, float y) {
    return x >= y;
}
auto sub c408(int x, number y) {
    return x >= y;
}
auto sub c409(int x, *number y) {
    return x >= y;
}
auto sub c410(int x, union<int, float> y) {
    return x >= y;
}
auto sub c411(int x, union<int, number> y) {
    return x >= y;
}
auto sub c412(int x, *union<int, float> y) {
    return x >= y;
}
auto sub c413(int x, union<int, bool> y) {
    return x >= y;
}
auto sub c414(int x, union<int, string> y) {
    return x >= y;
}
auto sub c415(float x, int y) {
    return x >= y;
}
auto sub c416(float x, float y) {
    return x >= y;
}
auto sub c417(float x, number y) {
    return x >= y;
}
auto sub c418(float x, *number y) {
    return x >= y;
}
auto sub c419(float x, union<int, float> y) {
    return x >= y;
}
auto sub c420(float x, union<int, number> y) {
    return x >= y;
}
auto sub c421(float x, *union<int, float> y) {
    return x >= y;
}
auto sub c422(float x, union<int, bool> y) {
    return x >= y;
}
auto sub c423(float x, union<int, string> y) {
    return x >= y;
}
auto sub c424(number x, int y) {
    return x >= y;
}
auto sub c425(number x, float y) {
    return x >= y;
}
auto sub c426(number x, number y) {
    return x >= y;
}
auto sub c427(number x, *number y) {
    return x >= y;
}
auto sub c428(number x, union<int, float> y) {
    return x >= y;
}
auto sub c429(number x, union<int, number> y) {
    return x >= y;
}
auto sub c430(number x, *union<int, float> y) {
    return x >= y;
}
auto sub c431(number x, union<int, bool> y) {
    return x >= y;
}
auto sub c432(number x, union<int, string> y) {
    return x >= y;
}
auto sub c433(*number x, int y) {
    return x >= y;
}
auto sub c434(*number x, float y) {
    return x >= y;
}
auto sub c435(*number x, number y) {
    return x >= y;
}
auto sub c436(*number x, *number y) {
    return x >= y;
}
auto sub c437(*number x, union<int, float> y) {
    return x >= y;
}
auto sub c438(*number x, union<int, number> y) {
    return x >= y;
}
auto sub c439(*number x, *union<int, float> y) {
    return x >= y;
}
auto sub c440(*number x, union<int, bool> y) {
    return x >= y;
}
auto sub c441(*number x, union<int, string> y) {
    return x >= y;
}
auto sub c442(union<int, float> x, int y) {
    return x >= y;
}
auto sub c443(union<int, float> x, float y) {
    return x >= y;
}
auto sub c444(union<int, float> x, number y) {
    return x >= y;
}
auto sub c445(union<int, float> x, *number y) {
    return x >= y;
}
auto sub c446(union<int, float> x, union<int, float> y) {
    return x >= y;
}
auto sub c447(union<int, float> x, union<int, number> y) {
    return x >= y;
}
auto sub c448(union<int, float> x, *union<int, float> y) {
    return x >= y;
}
auto sub c449(union<int, float> x, union<int, bool> y) {
    return x >= y;
}
auto sub c450(union<int, float> x, union<int, string> y) {
    return x >= y;
}
auto sub c451(union<int, number> x, int y) {
    return x >= y;
}
auto sub c452(union<int, number> x, float y) {
    return x >= y;
}
auto sub c453(union<int, number> x, number y) {
    return x >= y;
}
auto sub c454(union<int, number> x, *number y) {
    return x >= y;
}
auto sub c455(union<int, number> x, union<int, float> y) {
    return x >= y;
}
auto sub c456(union<int, number> x, union<int, number> y) {
    return x >= y;
}
auto sub c457(union<int, number> x, *union<int, float> y) {
    return x >= y;
}
auto sub c458(union<int, number> x, union<int, bool> y) {
    return x >= y;
}
auto sub c459(union<int, number> x, union<int, string> y) {
    return x >= y;
}
auto sub c460(*union<int, float> x, int y) {
    return x >= y;
}
auto sub c461(*union<int, float> x, float y) {
    return x >= y;
}
auto sub c462(*union<int, float> x, number y) {
    return x >= y;
}
auto sub c463(*union<int, float> x, *number y) {
    return x >= y;
}
auto sub c464(*union<int, float> x, union<int, float> y) {
    return x >= y;
}
auto sub c465(*union<int, float> x, union<int, number> y) {
    return x >= y;
}
auto sub c466(*union<int, float> x, *union<int, float> y) {
    return x >= y;
}
auto sub c467(*union<int, float> x, union<int, bool> y) {
    return x >= y;
}
auto sub c468(*union<int, float> x, union<int, string> y) {
    return x >= y;
}
auto sub c469(union<int, bool> x, int y) {
    return x >= y;
}
auto sub c470(union<int, bool> x, float y) {
    return x >= y;
}
auto sub c471(union<int, bool> x, number y) {
    return x >= y;
}
auto sub c472(union<int, bool> x, *number y) {
    return x >= y;
}
auto sub c473(union<int, bool> x, union<int, float> y) {
    return x >= y;
}
auto sub c474(union<int, bool> x, union<int, number> y) {
    return x >= y;
}
auto sub c475(union<int, bool> x, *union<int, float> y) {
    return x >= y;
}
auto sub c476(union<int, bool> x, union<int, bool> y) {
    return x >= y;
}
auto sub c477(union<int, bool> x, union<int, string> y) {
    return x >= y;
}
auto sub c478(union<int, string> x, int y) {
    return x >= y;
}
auto sub c479(union<int, string> x, float y) {
    return x >= y;
}
auto sub c480(union<int, string> x, number y) {
    return x >= y;
}
auto sub c481(union<int, string> x, *number y) {
    return x >= y;
}
auto sub c482(union<int, string> x, union<int, float> y) {
    return x >= y;
}
auto sub c483(union<int, string> x, union<int, number> y) {
    return x >= y;
}
auto sub c484(union<int, string> x, *union<int, float> y) {
    return x >= y;
}
auto sub c485(union<int, string> x, union<int, bool> y) {
    return x >= y;
}
auto sub c486(union<int, string> x, union<int, string> y) {
    return x >= y;
}
auto sub c487(int x, int y) {
    return x <=> y;
}
auto sub c488(int x, float y) {
    return x <=> y;
}
auto sub c489(int x, number y) {
    return x <=> y;
}
auto sub c490(int x, *number y) {
    return x <=> y;
}
auto sub c491(int x, union<int, float> y) {
    return x <=> y;
}
auto sub c492(int x, union<int, number> y) {
    return x <=> y;
}
auto sub c493(int x, *union<int, float> y) {
    return x <=> y;
}
auto sub c494(int x, union<int, bool> y) {
    return x <=> y;
}
auto sub c495(int x, union<int, string> y) {
    return x <=> y;
}
auto sub c496(float x, int y) {
    return x <=> y;
}
auto sub c497(float x, float y) {
    return x <=> y;
}
auto sub c498(float x, number y) {
    return x <=> y;
}
auto sub c499(float x, *number y) {
    return x <=> y;
}
auto sub c500(float x, union<int, float> y) {
    return x <=> y;
}
auto sub c501(float x, union<int, number> y) {
    return x <=> y;
}
auto sub c502(float x, *union<int, float> y) {
    return x <=> y;
}
auto sub c503(float x, union<int, bool> y) {
    return x <=> y;
}
auto sub c504(float x, union<int, string> y) {
    return x <=> y;
}
auto sub c505(number x, int y) {
    return x <=> y;
}
auto sub c506(number x, float y) {
    return x <=> y;
}
auto sub c507(number x, number y) {
    return x <=> y;
}
auto sub c508(number x, *number y) {
    return x <=> y;
}
auto sub c509(number x, union<int, float> y) {
    return x <=> y;
}
auto sub c510(number x, union<int, number> y) {
    return x <=> y;
}
auto sub c511(number x, *union<int, float> y) {
    return x <=> y;
}
auto sub c512(number x, union<int, bool> y) {
    return x <=> y;
}
auto sub c513(number x, union<int, string> y) {
    return x <=> y;
}
auto sub c514(*number x, int y) {
    return x <=> y;
}
auto sub c515(*number x, float y) {
    return x <=> y;
}
auto sub c516(*number x, number y) {
    return x <=> y;
}
auto sub c517(*number x, *number y) {
    return x <=> y;
}
auto sub c518(*number x, union<int, float> y) {
    return x <=> y;
}
auto sub c519(*number x, union<int, number> y) {
    return x <=> y;
}
auto sub c520(*number x, *union<int, float> y) {
    return x <=> y;
}
auto sub c521(*number x, union<int, bool> y) {
    return x <=> y;
}
auto sub c522(*number x, union<int, string> y) {
    return x <=> y;
}
auto sub c523(union<int, float> x, int y) {
    return x <=> y;
}
auto sub c524(union<int, float> x, float y) {
    return x <=> y;
}
auto sub c525(union<int, float> x, number y) {
    return x <=> y;
}
auto sub c526(union<int, float> x, *number y) {
    return x <=> y;
}
auto sub c527(union<int, float> x, union<int, float> y) {
    return x <=> y;
}
auto sub c528(union<int, float> x, union<int, number> y) {
    return x <=> y;
}
auto sub c529(union<int, float> x, *union<int, float> y) {
    return x <=> y;
}
auto sub c530(union<int, float> x, union<int, bool> y) {
    return x <=> y;
}
auto sub c531(union<int, float> x, union<int, string> y) {
    return x <=> y;
}
auto sub c532(union<int, number> x, int y) {
    return x <=> y;
}
auto sub c533(union<int, number> x, float y) {
    return x <=> y;
}
auto sub c534(union<int, number> x, number y) {
    return x <=> y;
}
auto sub c535(union<int, number> x, *number y) {
    return x <=> y;
}
auto sub c536(union<int, number> x, union<int, float> y) {
    return x <=> y;
}
auto sub c537(union<int, number> x, union<int, number> y) {
    return x <=> y;
}
auto sub c538(union<int, number> x, *union<int, float> y) {
    return x <=> y;
}
auto sub c539(union<int, number> x, union<int, bool> y) {
    return x <=> y;
}
auto sub c540(union<int, number> x, union<int, string> y) {
    return x <=> y;
}
auto sub c541(*union<int, float> x, int y) {
    return x <=> y;
}
auto sub c542(*union<int, float> x, float y) {
    return x <=> y;
}
auto sub c543(*union<int, float> x, number y) {
    return x <=> y;
}
auto sub c544(*union<int, float> x, *number y) {
    return x <=> y;
}
auto sub c545(*union<int, float> x, union<int, float> y) {
    return x <=> y;
}
auto sub c546(*union<int, float> x, union<int, number> y) {
    return x <=> y;
}
auto sub c547(*union<int, float> x, *union<int, float> y) {
    return x <=> y;
}
auto sub c548(*union<int, float> x, union<int, bool> y) {
    return x <=> y;
}
auto sub c549(*union<int, float> x, union<int, string> y) {
    return x <=> y;
}
auto sub c550(union<int, bool> x, int y) {
    return x <=> y;
}
auto sub c551(union<int, bool> x, float y) {
    return x <=> y;
}
auto sub c552(union<int, bool> x, number y) {
    return x <=> y;
}
auto sub c553(union<int, bool> x, *number y) {
    return x <=> y;
}
auto sub c554(union<int, bool> x, union<int, float> y) {
    return x <=> y;
}
auto sub c555(union<int, bool> x, union<int, number> y) {
    return x <=> y;
}
auto sub c556(union<int, bool> x, *union<int, float> y) {
    return x <=> y;
}
auto sub c557(union<int, bool> x, union<int, bool> y) {
    return x <=> y;
}
auto sub c558(union<int, bool> x, union<int, string> y) {
    return x <=> y;
}
auto sub c559(union<int, string> x, int y) {
    return x <=> y;
}
auto sub c560(union<int, string> x, float y) {
    return x <=> y;
}
auto sub c561(union<int, string> x, number y) {
    return x <=> y;
}
auto sub c562(union<int, string> x, *number y) {
    return x <=> y;
}
auto sub c563(union<int, string> x, union<int, float> y) {
    return x <=> y;
}
auto sub c564(union<int, string> x, union<int, number> y) {
    return x <=> y;
}
auto sub c565(union<int, string> x, *union<int, float> y) {
    return x <=> y;
}
auto sub c566(union<int, string> x, union<int, bool> y) {
    return x <=> y;
}
auto sub c567(union<int, string> x, union<int, string> y) {
    return x <=> y;
}
auto sub c568(int x, int y) {
    return x === y;
}
auto sub c569(int x, float y) {
    return x === y;
}
auto sub c570(int x, number y) {
    return x === y;
}
auto sub c571(int x, *number y) {
    return x === y;
}
auto sub c572(int x, union<int, float> y) {
    return x === y;
}
auto sub c573(int x, union<int, number> y) {
    return x === y;
}
auto sub c574(int x, *union<int, float> y) {
    return x === y;
}
auto sub c575(int x, union<int, bool> y) {
    return x === y;
}
auto sub c576(int x, union<int, string> y) {
    return x === y;
}
auto sub c577(float x, int y) {
    return x === y;
}
auto sub c578(float x, float y) {
    return x === y;
}
auto sub c579(float x, number y) {
    return x === y;
}
auto sub c580(float x, *number y) {
    return x === y;
}
auto sub c581(float x, union<int, float> y) {
    return x === y;
}
auto sub c582(float x, union<int, number> y) {
    return x === y;
}
auto sub c583(float x, *union<int, float> y) {
    return x === y;
}
auto sub c584(float x, union<int, bool> y) {
    return x === y;
}
auto sub c585(float x, union<int, string> y) {
    return x === y;
}
auto sub c586(number x, int y) {
    return x === y;
}
auto sub c587(number x, float y) {
    return x === y;
}
auto sub c588(number x, number y) {
    return x === y;
}
auto sub c589(number x, *number y) {
    return x === y;
}
auto sub c590(number x, union<int, float> y) {
    return x === y;
}
auto sub c591(number x, union<int, number> y) {
    return x === y;
}
auto sub c592(number x, *union<int, float> y) {
    return x === y;
}
auto sub c593(number x, union<int, bool> y) {
    return x === y;
}
auto sub c594(number x, union<int, string> y) {
    return x === y;
}
auto sub c595(*number x, int y) {
    return x === y;
}
auto sub c596(*number x, float y) {
    return x === y;
}
auto sub c597(*number x, number y) {
    return x === y;
}
auto sub c598(*number x, *number y) {
    return x === y;
}
auto sub c599(*number x, union<int, float> y) {
    return x === y;
}
auto sub c600(*number x, union<int, number> y) {
    return x === y;
}
auto sub c601(*number x, *union<int, float> y) {
    return x === y;
}
auto sub c602(*number x, union<int, bool> y) {
    return x === y;
}
auto sub c603(*number x, union<int, string> y) {
    return x === y;
}
auto sub c604(union<int, float> x, int y) {
    return x === y;
}
auto sub c605(union<int, float> x, float y) {
    return x === y;
}
auto sub c606(union<int, float> x, number y) {
    return x === y;
}
auto sub c607(union<int, float> x, *number y) {
    return x === y;
}
auto sub c608(union<int, float> x, union<int, float> y) {
    return x === y;
}
auto sub c609(union<int, float> x, union<int, number> y) {
    return x === y;
}
auto sub c610(union<int, float> x, *union<int, float> y) {
    return x === y;
}
auto sub c611(union<int, float> x, union<int, bool> y) {
    return x === y;
}
auto sub c612(union<int, float> x, union<int, string> y) {
    return x === y;
}
auto sub c613(union<int, number> x, int y) {
    return x === y;
}
auto sub c614(union<int, number> x, float y) {
    return x === y;
}
auto sub c615(union<int, number> x, number y) {
    return x === y;
}
auto sub c616(union<int, number> x, *number y) {
    return x === y;
}
auto sub c617(union<int, number> x, union<int, float> y) {
    return x === y;
}
auto sub c618(union<int, number> x, union<int, number> y) {
    return x === y;
}
auto sub c619(union<int, number> x, *union<int, float> y) {
    return x === y;
}
auto sub c620(union<int, number> x, union<int, bool> y) {
    return x === y;
}
auto sub c621(union<int, number> x, union<int, string> y) {
    return x === y;
}
auto sub c622(*union<int, float> x, int y) {
    return x === y;
}
auto sub c623(*union<int, float> x, float y) {
    return x === y;
}
auto sub c624(*union<int, float> x, number y) {
    return x === y;
}
auto sub c625(*union<int, float> x, *number y) {
    return x === y;
}
auto sub c626(*union<int, float> x, union<int, float> y) {
    return x === y;
}
auto sub c627(*union<int, float> x, union<int, number> y) {
    return x === y;
}
auto sub c628(*union<int, float> x, *union<int, float> y) {
    return x === y;
}
auto sub c629(*union<int, float> x, union<int, bool> y) {
    return x === y;
}
auto sub c630(*union<int, float> x, union<int, string> y) {
    return x === y;
}
auto sub c631(union<int, bool> x, int y) {
    return x === y;
}
auto sub c632(union<int, bool> x, float y) {
    return x === y;
}
auto sub c633(union<int, bool> x, number y) {
    return x === y;
}
auto sub c634(union<int, bool> x, *number y) {
    return x === y;
}
auto sub c635(union<int, bool> x, union<int, float> y) {
    return x === y;
}
auto sub c636(union<int, bool> x, union<int, number> y) {
    return x === y;
}
auto sub c637(union<int, bool> x, *union<int, float> y) {
    return x === y;
}
auto sub c638(union<int, bool> x, union<int, bool> y) {
    return x === y;
}
auto sub c639(union<int, bool> x, union<int, string> y) {
    return x === y;
}
auto sub c640(union<int, string> x, int y) {
    return x === y;
}
auto sub c641(union<int, string> x, float y) {
    return x === y;
}
auto sub c642(union<int, string> x, number y) {
    return x === y;
}
auto sub c643(union<int, string> x, *number y) {
    return x === y;
}
auto sub c644(union<int, string> x, union<int, float> y) {
    return x === y;
}
auto sub c645(union<int, string> x, union<int, number> y) {
    return x === y;
}
auto sub c646(union<int, string> x, *union<int, float> y) {
    return x === y;
}
auto sub c647(union<int, string> x, union<int, bool> y) {
    return x === y;
}
auto sub c648(union<int, string> x, union<int, string> y) {
    return x === y;
}
sub main() {
    # values of int
    list<auto> v0 = (2,);
    # values of float
    list<auto> v1 = (2.5, 1.0);
    # values of number
    list<auto> v2 = (1.0000000000000000001n, 2n);
    # values of *number
    list<auto> v3 = (1.0000000000000000001n, NOTHING);
    # values of union<int, float>
    list<auto> v4 = (2, 2.5);
    # values of union<int, number>
    list<auto> v5 = (2, 1.0000000000000000001n);
    # values of *union<int, float>
    list<auto> v6 = (2, 2.5, NOTHING);
    # values of union<int, bool>
    list<auto> v7 = (1, True);
    # values of union<int, string>
    list<auto> v8 = (2, "2x");
    foreach auto a in (v0) {
        foreach auto b in (v0) {
            show(sprintf("int == int (%y, %y)", a, b),
                auto sub () { return c1(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v1) {
            show(sprintf("int == float (%y, %y)", a, b),
                auto sub () { return c2(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v2) {
            show(sprintf("int == number (%y, %y)", a, b),
                auto sub () { return c3(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v3) {
            show(sprintf("int == *number (%y, %y)", a, b),
                auto sub () { return c4(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v4) {
            show(sprintf("int == union<int, float> (%y, %y)", a, b),
                auto sub () { return c5(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v5) {
            show(sprintf("int == union<int, number> (%y, %y)", a, b),
                auto sub () { return c6(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v6) {
            show(sprintf("int == *union<int, float> (%y, %y)", a, b),
                auto sub () { return c7(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v7) {
            show(sprintf("int == union<int, bool> (%y, %y)", a, b),
                auto sub () { return c8(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v8) {
            show(sprintf("int == union<int, string> (%y, %y)", a, b),
                auto sub () { return c9(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v0) {
            show(sprintf("float == int (%y, %y)", a, b),
                auto sub () { return c10(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v1) {
            show(sprintf("float == float (%y, %y)", a, b),
                auto sub () { return c11(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v2) {
            show(sprintf("float == number (%y, %y)", a, b),
                auto sub () { return c12(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v3) {
            show(sprintf("float == *number (%y, %y)", a, b),
                auto sub () { return c13(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v4) {
            show(sprintf("float == union<int, float> (%y, %y)", a, b),
                auto sub () { return c14(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v5) {
            show(sprintf("float == union<int, number> (%y, %y)", a, b),
                auto sub () { return c15(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v6) {
            show(sprintf("float == *union<int, float> (%y, %y)", a, b),
                auto sub () { return c16(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v7) {
            show(sprintf("float == union<int, bool> (%y, %y)", a, b),
                auto sub () { return c17(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v8) {
            show(sprintf("float == union<int, string> (%y, %y)", a, b),
                auto sub () { return c18(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v0) {
            show(sprintf("number == int (%y, %y)", a, b),
                auto sub () { return c19(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v1) {
            show(sprintf("number == float (%y, %y)", a, b),
                auto sub () { return c20(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v2) {
            show(sprintf("number == number (%y, %y)", a, b),
                auto sub () { return c21(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v3) {
            show(sprintf("number == *number (%y, %y)", a, b),
                auto sub () { return c22(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v4) {
            show(sprintf("number == union<int, float> (%y, %y)", a, b),
                auto sub () { return c23(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v5) {
            show(sprintf("number == union<int, number> (%y, %y)", a, b),
                auto sub () { return c24(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v6) {
            show(sprintf("number == *union<int, float> (%y, %y)", a, b),
                auto sub () { return c25(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v7) {
            show(sprintf("number == union<int, bool> (%y, %y)", a, b),
                auto sub () { return c26(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v8) {
            show(sprintf("number == union<int, string> (%y, %y)", a, b),
                auto sub () { return c27(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v0) {
            show(sprintf("*number == int (%y, %y)", a, b),
                auto sub () { return c28(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v1) {
            show(sprintf("*number == float (%y, %y)", a, b),
                auto sub () { return c29(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v2) {
            show(sprintf("*number == number (%y, %y)", a, b),
                auto sub () { return c30(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v3) {
            show(sprintf("*number == *number (%y, %y)", a, b),
                auto sub () { return c31(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v4) {
            show(sprintf("*number == union<int, float> (%y, %y)", a, b),
                auto sub () { return c32(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v5) {
            show(sprintf("*number == union<int, number> (%y, %y)", a, b),
                auto sub () { return c33(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v6) {
            show(sprintf("*number == *union<int, float> (%y, %y)", a, b),
                auto sub () { return c34(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v7) {
            show(sprintf("*number == union<int, bool> (%y, %y)", a, b),
                auto sub () { return c35(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v8) {
            show(sprintf("*number == union<int, string> (%y, %y)", a, b),
                auto sub () { return c36(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v0) {
            show(sprintf("union<int, float> == int (%y, %y)", a, b),
                auto sub () { return c37(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v1) {
            show(sprintf("union<int, float> == float (%y, %y)", a, b),
                auto sub () { return c38(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v2) {
            show(sprintf("union<int, float> == number (%y, %y)", a, b),
                auto sub () { return c39(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v3) {
            show(sprintf("union<int, float> == *number (%y, %y)", a, b),
                auto sub () { return c40(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v4) {
            show(sprintf("union<int, float> == union<int, float> (%y, %y)", a, b),
                auto sub () { return c41(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v5) {
            show(sprintf("union<int, float> == union<int, number> (%y, %y)", a, b),
                auto sub () { return c42(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v6) {
            show(sprintf("union<int, float> == *union<int, float> (%y, %y)", a, b),
                auto sub () { return c43(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v7) {
            show(sprintf("union<int, float> == union<int, bool> (%y, %y)", a, b),
                auto sub () { return c44(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v8) {
            show(sprintf("union<int, float> == union<int, string> (%y, %y)", a, b),
                auto sub () { return c45(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v0) {
            show(sprintf("union<int, number> == int (%y, %y)", a, b),
                auto sub () { return c46(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v1) {
            show(sprintf("union<int, number> == float (%y, %y)", a, b),
                auto sub () { return c47(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v2) {
            show(sprintf("union<int, number> == number (%y, %y)", a, b),
                auto sub () { return c48(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v3) {
            show(sprintf("union<int, number> == *number (%y, %y)", a, b),
                auto sub () { return c49(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v4) {
            show(sprintf("union<int, number> == union<int, float> (%y, %y)", a, b),
                auto sub () { return c50(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v5) {
            show(sprintf("union<int, number> == union<int, number> (%y, %y)", a, b),
                auto sub () { return c51(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v6) {
            show(sprintf("union<int, number> == *union<int, float> (%y, %y)", a, b),
                auto sub () { return c52(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v7) {
            show(sprintf("union<int, number> == union<int, bool> (%y, %y)", a, b),
                auto sub () { return c53(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v8) {
            show(sprintf("union<int, number> == union<int, string> (%y, %y)", a, b),
                auto sub () { return c54(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v0) {
            show(sprintf("*union<int, float> == int (%y, %y)", a, b),
                auto sub () { return c55(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v1) {
            show(sprintf("*union<int, float> == float (%y, %y)", a, b),
                auto sub () { return c56(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v2) {
            show(sprintf("*union<int, float> == number (%y, %y)", a, b),
                auto sub () { return c57(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v3) {
            show(sprintf("*union<int, float> == *number (%y, %y)", a, b),
                auto sub () { return c58(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v4) {
            show(sprintf("*union<int, float> == union<int, float> (%y, %y)", a, b),
                auto sub () { return c59(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v5) {
            show(sprintf("*union<int, float> == union<int, number> (%y, %y)", a, b),
                auto sub () { return c60(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v6) {
            show(sprintf("*union<int, float> == *union<int, float> (%y, %y)", a, b),
                auto sub () { return c61(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v7) {
            show(sprintf("*union<int, float> == union<int, bool> (%y, %y)", a, b),
                auto sub () { return c62(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v8) {
            show(sprintf("*union<int, float> == union<int, string> (%y, %y)", a, b),
                auto sub () { return c63(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v0) {
            show(sprintf("union<int, bool> == int (%y, %y)", a, b),
                auto sub () { return c64(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v1) {
            show(sprintf("union<int, bool> == float (%y, %y)", a, b),
                auto sub () { return c65(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v2) {
            show(sprintf("union<int, bool> == number (%y, %y)", a, b),
                auto sub () { return c66(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v3) {
            show(sprintf("union<int, bool> == *number (%y, %y)", a, b),
                auto sub () { return c67(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v4) {
            show(sprintf("union<int, bool> == union<int, float> (%y, %y)", a, b),
                auto sub () { return c68(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v5) {
            show(sprintf("union<int, bool> == union<int, number> (%y, %y)", a, b),
                auto sub () { return c69(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v6) {
            show(sprintf("union<int, bool> == *union<int, float> (%y, %y)", a, b),
                auto sub () { return c70(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v7) {
            show(sprintf("union<int, bool> == union<int, bool> (%y, %y)", a, b),
                auto sub () { return c71(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v8) {
            show(sprintf("union<int, bool> == union<int, string> (%y, %y)", a, b),
                auto sub () { return c72(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v0) {
            show(sprintf("union<int, string> == int (%y, %y)", a, b),
                auto sub () { return c73(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v1) {
            show(sprintf("union<int, string> == float (%y, %y)", a, b),
                auto sub () { return c74(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v2) {
            show(sprintf("union<int, string> == number (%y, %y)", a, b),
                auto sub () { return c75(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v3) {
            show(sprintf("union<int, string> == *number (%y, %y)", a, b),
                auto sub () { return c76(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v4) {
            show(sprintf("union<int, string> == union<int, float> (%y, %y)", a, b),
                auto sub () { return c77(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v5) {
            show(sprintf("union<int, string> == union<int, number> (%y, %y)", a, b),
                auto sub () { return c78(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v6) {
            show(sprintf("union<int, string> == *union<int, float> (%y, %y)", a, b),
                auto sub () { return c79(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v7) {
            show(sprintf("union<int, string> == union<int, bool> (%y, %y)", a, b),
                auto sub () { return c80(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v8) {
            show(sprintf("union<int, string> == union<int, string> (%y, %y)", a, b),
                auto sub () { return c81(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v0) {
            show(sprintf("int != int (%y, %y)", a, b),
                auto sub () { return c82(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v1) {
            show(sprintf("int != float (%y, %y)", a, b),
                auto sub () { return c83(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v2) {
            show(sprintf("int != number (%y, %y)", a, b),
                auto sub () { return c84(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v3) {
            show(sprintf("int != *number (%y, %y)", a, b),
                auto sub () { return c85(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v4) {
            show(sprintf("int != union<int, float> (%y, %y)", a, b),
                auto sub () { return c86(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v5) {
            show(sprintf("int != union<int, number> (%y, %y)", a, b),
                auto sub () { return c87(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v6) {
            show(sprintf("int != *union<int, float> (%y, %y)", a, b),
                auto sub () { return c88(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v7) {
            show(sprintf("int != union<int, bool> (%y, %y)", a, b),
                auto sub () { return c89(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v8) {
            show(sprintf("int != union<int, string> (%y, %y)", a, b),
                auto sub () { return c90(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v0) {
            show(sprintf("float != int (%y, %y)", a, b),
                auto sub () { return c91(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v1) {
            show(sprintf("float != float (%y, %y)", a, b),
                auto sub () { return c92(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v2) {
            show(sprintf("float != number (%y, %y)", a, b),
                auto sub () { return c93(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v3) {
            show(sprintf("float != *number (%y, %y)", a, b),
                auto sub () { return c94(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v4) {
            show(sprintf("float != union<int, float> (%y, %y)", a, b),
                auto sub () { return c95(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v5) {
            show(sprintf("float != union<int, number> (%y, %y)", a, b),
                auto sub () { return c96(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v6) {
            show(sprintf("float != *union<int, float> (%y, %y)", a, b),
                auto sub () { return c97(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v7) {
            show(sprintf("float != union<int, bool> (%y, %y)", a, b),
                auto sub () { return c98(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v8) {
            show(sprintf("float != union<int, string> (%y, %y)", a, b),
                auto sub () { return c99(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v0) {
            show(sprintf("number != int (%y, %y)", a, b),
                auto sub () { return c100(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v1) {
            show(sprintf("number != float (%y, %y)", a, b),
                auto sub () { return c101(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v2) {
            show(sprintf("number != number (%y, %y)", a, b),
                auto sub () { return c102(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v3) {
            show(sprintf("number != *number (%y, %y)", a, b),
                auto sub () { return c103(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v4) {
            show(sprintf("number != union<int, float> (%y, %y)", a, b),
                auto sub () { return c104(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v5) {
            show(sprintf("number != union<int, number> (%y, %y)", a, b),
                auto sub () { return c105(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v6) {
            show(sprintf("number != *union<int, float> (%y, %y)", a, b),
                auto sub () { return c106(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v7) {
            show(sprintf("number != union<int, bool> (%y, %y)", a, b),
                auto sub () { return c107(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v8) {
            show(sprintf("number != union<int, string> (%y, %y)", a, b),
                auto sub () { return c108(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v0) {
            show(sprintf("*number != int (%y, %y)", a, b),
                auto sub () { return c109(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v1) {
            show(sprintf("*number != float (%y, %y)", a, b),
                auto sub () { return c110(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v2) {
            show(sprintf("*number != number (%y, %y)", a, b),
                auto sub () { return c111(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v3) {
            show(sprintf("*number != *number (%y, %y)", a, b),
                auto sub () { return c112(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v4) {
            show(sprintf("*number != union<int, float> (%y, %y)", a, b),
                auto sub () { return c113(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v5) {
            show(sprintf("*number != union<int, number> (%y, %y)", a, b),
                auto sub () { return c114(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v6) {
            show(sprintf("*number != *union<int, float> (%y, %y)", a, b),
                auto sub () { return c115(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v7) {
            show(sprintf("*number != union<int, bool> (%y, %y)", a, b),
                auto sub () { return c116(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v8) {
            show(sprintf("*number != union<int, string> (%y, %y)", a, b),
                auto sub () { return c117(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v0) {
            show(sprintf("union<int, float> != int (%y, %y)", a, b),
                auto sub () { return c118(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v1) {
            show(sprintf("union<int, float> != float (%y, %y)", a, b),
                auto sub () { return c119(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v2) {
            show(sprintf("union<int, float> != number (%y, %y)", a, b),
                auto sub () { return c120(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v3) {
            show(sprintf("union<int, float> != *number (%y, %y)", a, b),
                auto sub () { return c121(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v4) {
            show(sprintf("union<int, float> != union<int, float> (%y, %y)", a, b),
                auto sub () { return c122(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v5) {
            show(sprintf("union<int, float> != union<int, number> (%y, %y)", a, b),
                auto sub () { return c123(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v6) {
            show(sprintf("union<int, float> != *union<int, float> (%y, %y)", a, b),
                auto sub () { return c124(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v7) {
            show(sprintf("union<int, float> != union<int, bool> (%y, %y)", a, b),
                auto sub () { return c125(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v8) {
            show(sprintf("union<int, float> != union<int, string> (%y, %y)", a, b),
                auto sub () { return c126(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v0) {
            show(sprintf("union<int, number> != int (%y, %y)", a, b),
                auto sub () { return c127(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v1) {
            show(sprintf("union<int, number> != float (%y, %y)", a, b),
                auto sub () { return c128(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v2) {
            show(sprintf("union<int, number> != number (%y, %y)", a, b),
                auto sub () { return c129(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v3) {
            show(sprintf("union<int, number> != *number (%y, %y)", a, b),
                auto sub () { return c130(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v4) {
            show(sprintf("union<int, number> != union<int, float> (%y, %y)", a, b),
                auto sub () { return c131(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v5) {
            show(sprintf("union<int, number> != union<int, number> (%y, %y)", a, b),
                auto sub () { return c132(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v6) {
            show(sprintf("union<int, number> != *union<int, float> (%y, %y)", a, b),
                auto sub () { return c133(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v7) {
            show(sprintf("union<int, number> != union<int, bool> (%y, %y)", a, b),
                auto sub () { return c134(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v8) {
            show(sprintf("union<int, number> != union<int, string> (%y, %y)", a, b),
                auto sub () { return c135(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v0) {
            show(sprintf("*union<int, float> != int (%y, %y)", a, b),
                auto sub () { return c136(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v1) {
            show(sprintf("*union<int, float> != float (%y, %y)", a, b),
                auto sub () { return c137(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v2) {
            show(sprintf("*union<int, float> != number (%y, %y)", a, b),
                auto sub () { return c138(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v3) {
            show(sprintf("*union<int, float> != *number (%y, %y)", a, b),
                auto sub () { return c139(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v4) {
            show(sprintf("*union<int, float> != union<int, float> (%y, %y)", a, b),
                auto sub () { return c140(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v5) {
            show(sprintf("*union<int, float> != union<int, number> (%y, %y)", a, b),
                auto sub () { return c141(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v6) {
            show(sprintf("*union<int, float> != *union<int, float> (%y, %y)", a, b),
                auto sub () { return c142(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v7) {
            show(sprintf("*union<int, float> != union<int, bool> (%y, %y)", a, b),
                auto sub () { return c143(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v8) {
            show(sprintf("*union<int, float> != union<int, string> (%y, %y)", a, b),
                auto sub () { return c144(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v0) {
            show(sprintf("union<int, bool> != int (%y, %y)", a, b),
                auto sub () { return c145(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v1) {
            show(sprintf("union<int, bool> != float (%y, %y)", a, b),
                auto sub () { return c146(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v2) {
            show(sprintf("union<int, bool> != number (%y, %y)", a, b),
                auto sub () { return c147(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v3) {
            show(sprintf("union<int, bool> != *number (%y, %y)", a, b),
                auto sub () { return c148(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v4) {
            show(sprintf("union<int, bool> != union<int, float> (%y, %y)", a, b),
                auto sub () { return c149(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v5) {
            show(sprintf("union<int, bool> != union<int, number> (%y, %y)", a, b),
                auto sub () { return c150(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v6) {
            show(sprintf("union<int, bool> != *union<int, float> (%y, %y)", a, b),
                auto sub () { return c151(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v7) {
            show(sprintf("union<int, bool> != union<int, bool> (%y, %y)", a, b),
                auto sub () { return c152(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v8) {
            show(sprintf("union<int, bool> != union<int, string> (%y, %y)", a, b),
                auto sub () { return c153(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v0) {
            show(sprintf("union<int, string> != int (%y, %y)", a, b),
                auto sub () { return c154(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v1) {
            show(sprintf("union<int, string> != float (%y, %y)", a, b),
                auto sub () { return c155(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v2) {
            show(sprintf("union<int, string> != number (%y, %y)", a, b),
                auto sub () { return c156(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v3) {
            show(sprintf("union<int, string> != *number (%y, %y)", a, b),
                auto sub () { return c157(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v4) {
            show(sprintf("union<int, string> != union<int, float> (%y, %y)", a, b),
                auto sub () { return c158(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v5) {
            show(sprintf("union<int, string> != union<int, number> (%y, %y)", a, b),
                auto sub () { return c159(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v6) {
            show(sprintf("union<int, string> != *union<int, float> (%y, %y)", a, b),
                auto sub () { return c160(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v7) {
            show(sprintf("union<int, string> != union<int, bool> (%y, %y)", a, b),
                auto sub () { return c161(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v8) {
            show(sprintf("union<int, string> != union<int, string> (%y, %y)", a, b),
                auto sub () { return c162(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v0) {
            show(sprintf("int < int (%y, %y)", a, b),
                auto sub () { return c163(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v1) {
            show(sprintf("int < float (%y, %y)", a, b),
                auto sub () { return c164(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v2) {
            show(sprintf("int < number (%y, %y)", a, b),
                auto sub () { return c165(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v3) {
            show(sprintf("int < *number (%y, %y)", a, b),
                auto sub () { return c166(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v4) {
            show(sprintf("int < union<int, float> (%y, %y)", a, b),
                auto sub () { return c167(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v5) {
            show(sprintf("int < union<int, number> (%y, %y)", a, b),
                auto sub () { return c168(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v6) {
            show(sprintf("int < *union<int, float> (%y, %y)", a, b),
                auto sub () { return c169(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v7) {
            show(sprintf("int < union<int, bool> (%y, %y)", a, b),
                auto sub () { return c170(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v8) {
            show(sprintf("int < union<int, string> (%y, %y)", a, b),
                auto sub () { return c171(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v0) {
            show(sprintf("float < int (%y, %y)", a, b),
                auto sub () { return c172(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v1) {
            show(sprintf("float < float (%y, %y)", a, b),
                auto sub () { return c173(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v2) {
            show(sprintf("float < number (%y, %y)", a, b),
                auto sub () { return c174(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v3) {
            show(sprintf("float < *number (%y, %y)", a, b),
                auto sub () { return c175(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v4) {
            show(sprintf("float < union<int, float> (%y, %y)", a, b),
                auto sub () { return c176(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v5) {
            show(sprintf("float < union<int, number> (%y, %y)", a, b),
                auto sub () { return c177(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v6) {
            show(sprintf("float < *union<int, float> (%y, %y)", a, b),
                auto sub () { return c178(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v7) {
            show(sprintf("float < union<int, bool> (%y, %y)", a, b),
                auto sub () { return c179(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v8) {
            show(sprintf("float < union<int, string> (%y, %y)", a, b),
                auto sub () { return c180(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v0) {
            show(sprintf("number < int (%y, %y)", a, b),
                auto sub () { return c181(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v1) {
            show(sprintf("number < float (%y, %y)", a, b),
                auto sub () { return c182(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v2) {
            show(sprintf("number < number (%y, %y)", a, b),
                auto sub () { return c183(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v3) {
            show(sprintf("number < *number (%y, %y)", a, b),
                auto sub () { return c184(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v4) {
            show(sprintf("number < union<int, float> (%y, %y)", a, b),
                auto sub () { return c185(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v5) {
            show(sprintf("number < union<int, number> (%y, %y)", a, b),
                auto sub () { return c186(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v6) {
            show(sprintf("number < *union<int, float> (%y, %y)", a, b),
                auto sub () { return c187(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v7) {
            show(sprintf("number < union<int, bool> (%y, %y)", a, b),
                auto sub () { return c188(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v8) {
            show(sprintf("number < union<int, string> (%y, %y)", a, b),
                auto sub () { return c189(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v0) {
            show(sprintf("*number < int (%y, %y)", a, b),
                auto sub () { return c190(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v1) {
            show(sprintf("*number < float (%y, %y)", a, b),
                auto sub () { return c191(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v2) {
            show(sprintf("*number < number (%y, %y)", a, b),
                auto sub () { return c192(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v3) {
            show(sprintf("*number < *number (%y, %y)", a, b),
                auto sub () { return c193(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v4) {
            show(sprintf("*number < union<int, float> (%y, %y)", a, b),
                auto sub () { return c194(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v5) {
            show(sprintf("*number < union<int, number> (%y, %y)", a, b),
                auto sub () { return c195(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v6) {
            show(sprintf("*number < *union<int, float> (%y, %y)", a, b),
                auto sub () { return c196(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v7) {
            show(sprintf("*number < union<int, bool> (%y, %y)", a, b),
                auto sub () { return c197(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v8) {
            show(sprintf("*number < union<int, string> (%y, %y)", a, b),
                auto sub () { return c198(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v0) {
            show(sprintf("union<int, float> < int (%y, %y)", a, b),
                auto sub () { return c199(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v1) {
            show(sprintf("union<int, float> < float (%y, %y)", a, b),
                auto sub () { return c200(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v2) {
            show(sprintf("union<int, float> < number (%y, %y)", a, b),
                auto sub () { return c201(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v3) {
            show(sprintf("union<int, float> < *number (%y, %y)", a, b),
                auto sub () { return c202(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v4) {
            show(sprintf("union<int, float> < union<int, float> (%y, %y)", a, b),
                auto sub () { return c203(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v5) {
            show(sprintf("union<int, float> < union<int, number> (%y, %y)", a, b),
                auto sub () { return c204(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v6) {
            show(sprintf("union<int, float> < *union<int, float> (%y, %y)", a, b),
                auto sub () { return c205(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v7) {
            show(sprintf("union<int, float> < union<int, bool> (%y, %y)", a, b),
                auto sub () { return c206(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v8) {
            show(sprintf("union<int, float> < union<int, string> (%y, %y)", a, b),
                auto sub () { return c207(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v0) {
            show(sprintf("union<int, number> < int (%y, %y)", a, b),
                auto sub () { return c208(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v1) {
            show(sprintf("union<int, number> < float (%y, %y)", a, b),
                auto sub () { return c209(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v2) {
            show(sprintf("union<int, number> < number (%y, %y)", a, b),
                auto sub () { return c210(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v3) {
            show(sprintf("union<int, number> < *number (%y, %y)", a, b),
                auto sub () { return c211(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v4) {
            show(sprintf("union<int, number> < union<int, float> (%y, %y)", a, b),
                auto sub () { return c212(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v5) {
            show(sprintf("union<int, number> < union<int, number> (%y, %y)", a, b),
                auto sub () { return c213(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v6) {
            show(sprintf("union<int, number> < *union<int, float> (%y, %y)", a, b),
                auto sub () { return c214(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v7) {
            show(sprintf("union<int, number> < union<int, bool> (%y, %y)", a, b),
                auto sub () { return c215(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v8) {
            show(sprintf("union<int, number> < union<int, string> (%y, %y)", a, b),
                auto sub () { return c216(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v0) {
            show(sprintf("*union<int, float> < int (%y, %y)", a, b),
                auto sub () { return c217(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v1) {
            show(sprintf("*union<int, float> < float (%y, %y)", a, b),
                auto sub () { return c218(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v2) {
            show(sprintf("*union<int, float> < number (%y, %y)", a, b),
                auto sub () { return c219(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v3) {
            show(sprintf("*union<int, float> < *number (%y, %y)", a, b),
                auto sub () { return c220(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v4) {
            show(sprintf("*union<int, float> < union<int, float> (%y, %y)", a, b),
                auto sub () { return c221(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v5) {
            show(sprintf("*union<int, float> < union<int, number> (%y, %y)", a, b),
                auto sub () { return c222(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v6) {
            show(sprintf("*union<int, float> < *union<int, float> (%y, %y)", a, b),
                auto sub () { return c223(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v7) {
            show(sprintf("*union<int, float> < union<int, bool> (%y, %y)", a, b),
                auto sub () { return c224(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v8) {
            show(sprintf("*union<int, float> < union<int, string> (%y, %y)", a, b),
                auto sub () { return c225(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v0) {
            show(sprintf("union<int, bool> < int (%y, %y)", a, b),
                auto sub () { return c226(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v1) {
            show(sprintf("union<int, bool> < float (%y, %y)", a, b),
                auto sub () { return c227(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v2) {
            show(sprintf("union<int, bool> < number (%y, %y)", a, b),
                auto sub () { return c228(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v3) {
            show(sprintf("union<int, bool> < *number (%y, %y)", a, b),
                auto sub () { return c229(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v4) {
            show(sprintf("union<int, bool> < union<int, float> (%y, %y)", a, b),
                auto sub () { return c230(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v5) {
            show(sprintf("union<int, bool> < union<int, number> (%y, %y)", a, b),
                auto sub () { return c231(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v6) {
            show(sprintf("union<int, bool> < *union<int, float> (%y, %y)", a, b),
                auto sub () { return c232(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v7) {
            show(sprintf("union<int, bool> < union<int, bool> (%y, %y)", a, b),
                auto sub () { return c233(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v8) {
            show(sprintf("union<int, bool> < union<int, string> (%y, %y)", a, b),
                auto sub () { return c234(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v0) {
            show(sprintf("union<int, string> < int (%y, %y)", a, b),
                auto sub () { return c235(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v1) {
            show(sprintf("union<int, string> < float (%y, %y)", a, b),
                auto sub () { return c236(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v2) {
            show(sprintf("union<int, string> < number (%y, %y)", a, b),
                auto sub () { return c237(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v3) {
            show(sprintf("union<int, string> < *number (%y, %y)", a, b),
                auto sub () { return c238(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v4) {
            show(sprintf("union<int, string> < union<int, float> (%y, %y)", a, b),
                auto sub () { return c239(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v5) {
            show(sprintf("union<int, string> < union<int, number> (%y, %y)", a, b),
                auto sub () { return c240(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v6) {
            show(sprintf("union<int, string> < *union<int, float> (%y, %y)", a, b),
                auto sub () { return c241(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v7) {
            show(sprintf("union<int, string> < union<int, bool> (%y, %y)", a, b),
                auto sub () { return c242(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v8) {
            show(sprintf("union<int, string> < union<int, string> (%y, %y)", a, b),
                auto sub () { return c243(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v0) {
            show(sprintf("int <= int (%y, %y)", a, b),
                auto sub () { return c244(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v1) {
            show(sprintf("int <= float (%y, %y)", a, b),
                auto sub () { return c245(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v2) {
            show(sprintf("int <= number (%y, %y)", a, b),
                auto sub () { return c246(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v3) {
            show(sprintf("int <= *number (%y, %y)", a, b),
                auto sub () { return c247(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v4) {
            show(sprintf("int <= union<int, float> (%y, %y)", a, b),
                auto sub () { return c248(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v5) {
            show(sprintf("int <= union<int, number> (%y, %y)", a, b),
                auto sub () { return c249(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v6) {
            show(sprintf("int <= *union<int, float> (%y, %y)", a, b),
                auto sub () { return c250(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v7) {
            show(sprintf("int <= union<int, bool> (%y, %y)", a, b),
                auto sub () { return c251(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v8) {
            show(sprintf("int <= union<int, string> (%y, %y)", a, b),
                auto sub () { return c252(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v0) {
            show(sprintf("float <= int (%y, %y)", a, b),
                auto sub () { return c253(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v1) {
            show(sprintf("float <= float (%y, %y)", a, b),
                auto sub () { return c254(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v2) {
            show(sprintf("float <= number (%y, %y)", a, b),
                auto sub () { return c255(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v3) {
            show(sprintf("float <= *number (%y, %y)", a, b),
                auto sub () { return c256(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v4) {
            show(sprintf("float <= union<int, float> (%y, %y)", a, b),
                auto sub () { return c257(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v5) {
            show(sprintf("float <= union<int, number> (%y, %y)", a, b),
                auto sub () { return c258(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v6) {
            show(sprintf("float <= *union<int, float> (%y, %y)", a, b),
                auto sub () { return c259(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v7) {
            show(sprintf("float <= union<int, bool> (%y, %y)", a, b),
                auto sub () { return c260(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v8) {
            show(sprintf("float <= union<int, string> (%y, %y)", a, b),
                auto sub () { return c261(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v0) {
            show(sprintf("number <= int (%y, %y)", a, b),
                auto sub () { return c262(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v1) {
            show(sprintf("number <= float (%y, %y)", a, b),
                auto sub () { return c263(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v2) {
            show(sprintf("number <= number (%y, %y)", a, b),
                auto sub () { return c264(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v3) {
            show(sprintf("number <= *number (%y, %y)", a, b),
                auto sub () { return c265(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v4) {
            show(sprintf("number <= union<int, float> (%y, %y)", a, b),
                auto sub () { return c266(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v5) {
            show(sprintf("number <= union<int, number> (%y, %y)", a, b),
                auto sub () { return c267(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v6) {
            show(sprintf("number <= *union<int, float> (%y, %y)", a, b),
                auto sub () { return c268(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v7) {
            show(sprintf("number <= union<int, bool> (%y, %y)", a, b),
                auto sub () { return c269(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v8) {
            show(sprintf("number <= union<int, string> (%y, %y)", a, b),
                auto sub () { return c270(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v0) {
            show(sprintf("*number <= int (%y, %y)", a, b),
                auto sub () { return c271(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v1) {
            show(sprintf("*number <= float (%y, %y)", a, b),
                auto sub () { return c272(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v2) {
            show(sprintf("*number <= number (%y, %y)", a, b),
                auto sub () { return c273(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v3) {
            show(sprintf("*number <= *number (%y, %y)", a, b),
                auto sub () { return c274(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v4) {
            show(sprintf("*number <= union<int, float> (%y, %y)", a, b),
                auto sub () { return c275(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v5) {
            show(sprintf("*number <= union<int, number> (%y, %y)", a, b),
                auto sub () { return c276(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v6) {
            show(sprintf("*number <= *union<int, float> (%y, %y)", a, b),
                auto sub () { return c277(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v7) {
            show(sprintf("*number <= union<int, bool> (%y, %y)", a, b),
                auto sub () { return c278(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v8) {
            show(sprintf("*number <= union<int, string> (%y, %y)", a, b),
                auto sub () { return c279(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v0) {
            show(sprintf("union<int, float> <= int (%y, %y)", a, b),
                auto sub () { return c280(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v1) {
            show(sprintf("union<int, float> <= float (%y, %y)", a, b),
                auto sub () { return c281(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v2) {
            show(sprintf("union<int, float> <= number (%y, %y)", a, b),
                auto sub () { return c282(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v3) {
            show(sprintf("union<int, float> <= *number (%y, %y)", a, b),
                auto sub () { return c283(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v4) {
            show(sprintf("union<int, float> <= union<int, float> (%y, %y)", a, b),
                auto sub () { return c284(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v5) {
            show(sprintf("union<int, float> <= union<int, number> (%y, %y)", a, b),
                auto sub () { return c285(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v6) {
            show(sprintf("union<int, float> <= *union<int, float> (%y, %y)", a, b),
                auto sub () { return c286(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v7) {
            show(sprintf("union<int, float> <= union<int, bool> (%y, %y)", a, b),
                auto sub () { return c287(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v8) {
            show(sprintf("union<int, float> <= union<int, string> (%y, %y)", a, b),
                auto sub () { return c288(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v0) {
            show(sprintf("union<int, number> <= int (%y, %y)", a, b),
                auto sub () { return c289(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v1) {
            show(sprintf("union<int, number> <= float (%y, %y)", a, b),
                auto sub () { return c290(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v2) {
            show(sprintf("union<int, number> <= number (%y, %y)", a, b),
                auto sub () { return c291(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v3) {
            show(sprintf("union<int, number> <= *number (%y, %y)", a, b),
                auto sub () { return c292(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v4) {
            show(sprintf("union<int, number> <= union<int, float> (%y, %y)", a, b),
                auto sub () { return c293(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v5) {
            show(sprintf("union<int, number> <= union<int, number> (%y, %y)", a, b),
                auto sub () { return c294(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v6) {
            show(sprintf("union<int, number> <= *union<int, float> (%y, %y)", a, b),
                auto sub () { return c295(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v7) {
            show(sprintf("union<int, number> <= union<int, bool> (%y, %y)", a, b),
                auto sub () { return c296(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v8) {
            show(sprintf("union<int, number> <= union<int, string> (%y, %y)", a, b),
                auto sub () { return c297(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v0) {
            show(sprintf("*union<int, float> <= int (%y, %y)", a, b),
                auto sub () { return c298(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v1) {
            show(sprintf("*union<int, float> <= float (%y, %y)", a, b),
                auto sub () { return c299(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v2) {
            show(sprintf("*union<int, float> <= number (%y, %y)", a, b),
                auto sub () { return c300(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v3) {
            show(sprintf("*union<int, float> <= *number (%y, %y)", a, b),
                auto sub () { return c301(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v4) {
            show(sprintf("*union<int, float> <= union<int, float> (%y, %y)", a, b),
                auto sub () { return c302(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v5) {
            show(sprintf("*union<int, float> <= union<int, number> (%y, %y)", a, b),
                auto sub () { return c303(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v6) {
            show(sprintf("*union<int, float> <= *union<int, float> (%y, %y)", a, b),
                auto sub () { return c304(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v7) {
            show(sprintf("*union<int, float> <= union<int, bool> (%y, %y)", a, b),
                auto sub () { return c305(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v8) {
            show(sprintf("*union<int, float> <= union<int, string> (%y, %y)", a, b),
                auto sub () { return c306(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v0) {
            show(sprintf("union<int, bool> <= int (%y, %y)", a, b),
                auto sub () { return c307(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v1) {
            show(sprintf("union<int, bool> <= float (%y, %y)", a, b),
                auto sub () { return c308(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v2) {
            show(sprintf("union<int, bool> <= number (%y, %y)", a, b),
                auto sub () { return c309(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v3) {
            show(sprintf("union<int, bool> <= *number (%y, %y)", a, b),
                auto sub () { return c310(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v4) {
            show(sprintf("union<int, bool> <= union<int, float> (%y, %y)", a, b),
                auto sub () { return c311(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v5) {
            show(sprintf("union<int, bool> <= union<int, number> (%y, %y)", a, b),
                auto sub () { return c312(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v6) {
            show(sprintf("union<int, bool> <= *union<int, float> (%y, %y)", a, b),
                auto sub () { return c313(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v7) {
            show(sprintf("union<int, bool> <= union<int, bool> (%y, %y)", a, b),
                auto sub () { return c314(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v8) {
            show(sprintf("union<int, bool> <= union<int, string> (%y, %y)", a, b),
                auto sub () { return c315(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v0) {
            show(sprintf("union<int, string> <= int (%y, %y)", a, b),
                auto sub () { return c316(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v1) {
            show(sprintf("union<int, string> <= float (%y, %y)", a, b),
                auto sub () { return c317(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v2) {
            show(sprintf("union<int, string> <= number (%y, %y)", a, b),
                auto sub () { return c318(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v3) {
            show(sprintf("union<int, string> <= *number (%y, %y)", a, b),
                auto sub () { return c319(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v4) {
            show(sprintf("union<int, string> <= union<int, float> (%y, %y)", a, b),
                auto sub () { return c320(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v5) {
            show(sprintf("union<int, string> <= union<int, number> (%y, %y)", a, b),
                auto sub () { return c321(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v6) {
            show(sprintf("union<int, string> <= *union<int, float> (%y, %y)", a, b),
                auto sub () { return c322(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v7) {
            show(sprintf("union<int, string> <= union<int, bool> (%y, %y)", a, b),
                auto sub () { return c323(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v8) {
            show(sprintf("union<int, string> <= union<int, string> (%y, %y)", a, b),
                auto sub () { return c324(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v0) {
            show(sprintf("int > int (%y, %y)", a, b),
                auto sub () { return c325(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v1) {
            show(sprintf("int > float (%y, %y)", a, b),
                auto sub () { return c326(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v2) {
            show(sprintf("int > number (%y, %y)", a, b),
                auto sub () { return c327(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v3) {
            show(sprintf("int > *number (%y, %y)", a, b),
                auto sub () { return c328(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v4) {
            show(sprintf("int > union<int, float> (%y, %y)", a, b),
                auto sub () { return c329(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v5) {
            show(sprintf("int > union<int, number> (%y, %y)", a, b),
                auto sub () { return c330(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v6) {
            show(sprintf("int > *union<int, float> (%y, %y)", a, b),
                auto sub () { return c331(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v7) {
            show(sprintf("int > union<int, bool> (%y, %y)", a, b),
                auto sub () { return c332(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v8) {
            show(sprintf("int > union<int, string> (%y, %y)", a, b),
                auto sub () { return c333(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v0) {
            show(sprintf("float > int (%y, %y)", a, b),
                auto sub () { return c334(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v1) {
            show(sprintf("float > float (%y, %y)", a, b),
                auto sub () { return c335(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v2) {
            show(sprintf("float > number (%y, %y)", a, b),
                auto sub () { return c336(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v3) {
            show(sprintf("float > *number (%y, %y)", a, b),
                auto sub () { return c337(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v4) {
            show(sprintf("float > union<int, float> (%y, %y)", a, b),
                auto sub () { return c338(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v5) {
            show(sprintf("float > union<int, number> (%y, %y)", a, b),
                auto sub () { return c339(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v6) {
            show(sprintf("float > *union<int, float> (%y, %y)", a, b),
                auto sub () { return c340(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v7) {
            show(sprintf("float > union<int, bool> (%y, %y)", a, b),
                auto sub () { return c341(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v8) {
            show(sprintf("float > union<int, string> (%y, %y)", a, b),
                auto sub () { return c342(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v0) {
            show(sprintf("number > int (%y, %y)", a, b),
                auto sub () { return c343(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v1) {
            show(sprintf("number > float (%y, %y)", a, b),
                auto sub () { return c344(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v2) {
            show(sprintf("number > number (%y, %y)", a, b),
                auto sub () { return c345(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v3) {
            show(sprintf("number > *number (%y, %y)", a, b),
                auto sub () { return c346(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v4) {
            show(sprintf("number > union<int, float> (%y, %y)", a, b),
                auto sub () { return c347(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v5) {
            show(sprintf("number > union<int, number> (%y, %y)", a, b),
                auto sub () { return c348(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v6) {
            show(sprintf("number > *union<int, float> (%y, %y)", a, b),
                auto sub () { return c349(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v7) {
            show(sprintf("number > union<int, bool> (%y, %y)", a, b),
                auto sub () { return c350(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v8) {
            show(sprintf("number > union<int, string> (%y, %y)", a, b),
                auto sub () { return c351(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v0) {
            show(sprintf("*number > int (%y, %y)", a, b),
                auto sub () { return c352(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v1) {
            show(sprintf("*number > float (%y, %y)", a, b),
                auto sub () { return c353(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v2) {
            show(sprintf("*number > number (%y, %y)", a, b),
                auto sub () { return c354(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v3) {
            show(sprintf("*number > *number (%y, %y)", a, b),
                auto sub () { return c355(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v4) {
            show(sprintf("*number > union<int, float> (%y, %y)", a, b),
                auto sub () { return c356(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v5) {
            show(sprintf("*number > union<int, number> (%y, %y)", a, b),
                auto sub () { return c357(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v6) {
            show(sprintf("*number > *union<int, float> (%y, %y)", a, b),
                auto sub () { return c358(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v7) {
            show(sprintf("*number > union<int, bool> (%y, %y)", a, b),
                auto sub () { return c359(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v8) {
            show(sprintf("*number > union<int, string> (%y, %y)", a, b),
                auto sub () { return c360(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v0) {
            show(sprintf("union<int, float> > int (%y, %y)", a, b),
                auto sub () { return c361(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v1) {
            show(sprintf("union<int, float> > float (%y, %y)", a, b),
                auto sub () { return c362(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v2) {
            show(sprintf("union<int, float> > number (%y, %y)", a, b),
                auto sub () { return c363(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v3) {
            show(sprintf("union<int, float> > *number (%y, %y)", a, b),
                auto sub () { return c364(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v4) {
            show(sprintf("union<int, float> > union<int, float> (%y, %y)", a, b),
                auto sub () { return c365(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v5) {
            show(sprintf("union<int, float> > union<int, number> (%y, %y)", a, b),
                auto sub () { return c366(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v6) {
            show(sprintf("union<int, float> > *union<int, float> (%y, %y)", a, b),
                auto sub () { return c367(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v7) {
            show(sprintf("union<int, float> > union<int, bool> (%y, %y)", a, b),
                auto sub () { return c368(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v8) {
            show(sprintf("union<int, float> > union<int, string> (%y, %y)", a, b),
                auto sub () { return c369(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v0) {
            show(sprintf("union<int, number> > int (%y, %y)", a, b),
                auto sub () { return c370(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v1) {
            show(sprintf("union<int, number> > float (%y, %y)", a, b),
                auto sub () { return c371(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v2) {
            show(sprintf("union<int, number> > number (%y, %y)", a, b),
                auto sub () { return c372(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v3) {
            show(sprintf("union<int, number> > *number (%y, %y)", a, b),
                auto sub () { return c373(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v4) {
            show(sprintf("union<int, number> > union<int, float> (%y, %y)", a, b),
                auto sub () { return c374(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v5) {
            show(sprintf("union<int, number> > union<int, number> (%y, %y)", a, b),
                auto sub () { return c375(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v6) {
            show(sprintf("union<int, number> > *union<int, float> (%y, %y)", a, b),
                auto sub () { return c376(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v7) {
            show(sprintf("union<int, number> > union<int, bool> (%y, %y)", a, b),
                auto sub () { return c377(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v8) {
            show(sprintf("union<int, number> > union<int, string> (%y, %y)", a, b),
                auto sub () { return c378(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v0) {
            show(sprintf("*union<int, float> > int (%y, %y)", a, b),
                auto sub () { return c379(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v1) {
            show(sprintf("*union<int, float> > float (%y, %y)", a, b),
                auto sub () { return c380(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v2) {
            show(sprintf("*union<int, float> > number (%y, %y)", a, b),
                auto sub () { return c381(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v3) {
            show(sprintf("*union<int, float> > *number (%y, %y)", a, b),
                auto sub () { return c382(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v4) {
            show(sprintf("*union<int, float> > union<int, float> (%y, %y)", a, b),
                auto sub () { return c383(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v5) {
            show(sprintf("*union<int, float> > union<int, number> (%y, %y)", a, b),
                auto sub () { return c384(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v6) {
            show(sprintf("*union<int, float> > *union<int, float> (%y, %y)", a, b),
                auto sub () { return c385(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v7) {
            show(sprintf("*union<int, float> > union<int, bool> (%y, %y)", a, b),
                auto sub () { return c386(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v8) {
            show(sprintf("*union<int, float> > union<int, string> (%y, %y)", a, b),
                auto sub () { return c387(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v0) {
            show(sprintf("union<int, bool> > int (%y, %y)", a, b),
                auto sub () { return c388(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v1) {
            show(sprintf("union<int, bool> > float (%y, %y)", a, b),
                auto sub () { return c389(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v2) {
            show(sprintf("union<int, bool> > number (%y, %y)", a, b),
                auto sub () { return c390(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v3) {
            show(sprintf("union<int, bool> > *number (%y, %y)", a, b),
                auto sub () { return c391(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v4) {
            show(sprintf("union<int, bool> > union<int, float> (%y, %y)", a, b),
                auto sub () { return c392(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v5) {
            show(sprintf("union<int, bool> > union<int, number> (%y, %y)", a, b),
                auto sub () { return c393(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v6) {
            show(sprintf("union<int, bool> > *union<int, float> (%y, %y)", a, b),
                auto sub () { return c394(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v7) {
            show(sprintf("union<int, bool> > union<int, bool> (%y, %y)", a, b),
                auto sub () { return c395(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v8) {
            show(sprintf("union<int, bool> > union<int, string> (%y, %y)", a, b),
                auto sub () { return c396(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v0) {
            show(sprintf("union<int, string> > int (%y, %y)", a, b),
                auto sub () { return c397(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v1) {
            show(sprintf("union<int, string> > float (%y, %y)", a, b),
                auto sub () { return c398(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v2) {
            show(sprintf("union<int, string> > number (%y, %y)", a, b),
                auto sub () { return c399(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v3) {
            show(sprintf("union<int, string> > *number (%y, %y)", a, b),
                auto sub () { return c400(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v4) {
            show(sprintf("union<int, string> > union<int, float> (%y, %y)", a, b),
                auto sub () { return c401(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v5) {
            show(sprintf("union<int, string> > union<int, number> (%y, %y)", a, b),
                auto sub () { return c402(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v6) {
            show(sprintf("union<int, string> > *union<int, float> (%y, %y)", a, b),
                auto sub () { return c403(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v7) {
            show(sprintf("union<int, string> > union<int, bool> (%y, %y)", a, b),
                auto sub () { return c404(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v8) {
            show(sprintf("union<int, string> > union<int, string> (%y, %y)", a, b),
                auto sub () { return c405(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v0) {
            show(sprintf("int >= int (%y, %y)", a, b),
                auto sub () { return c406(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v1) {
            show(sprintf("int >= float (%y, %y)", a, b),
                auto sub () { return c407(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v2) {
            show(sprintf("int >= number (%y, %y)", a, b),
                auto sub () { return c408(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v3) {
            show(sprintf("int >= *number (%y, %y)", a, b),
                auto sub () { return c409(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v4) {
            show(sprintf("int >= union<int, float> (%y, %y)", a, b),
                auto sub () { return c410(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v5) {
            show(sprintf("int >= union<int, number> (%y, %y)", a, b),
                auto sub () { return c411(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v6) {
            show(sprintf("int >= *union<int, float> (%y, %y)", a, b),
                auto sub () { return c412(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v7) {
            show(sprintf("int >= union<int, bool> (%y, %y)", a, b),
                auto sub () { return c413(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v8) {
            show(sprintf("int >= union<int, string> (%y, %y)", a, b),
                auto sub () { return c414(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v0) {
            show(sprintf("float >= int (%y, %y)", a, b),
                auto sub () { return c415(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v1) {
            show(sprintf("float >= float (%y, %y)", a, b),
                auto sub () { return c416(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v2) {
            show(sprintf("float >= number (%y, %y)", a, b),
                auto sub () { return c417(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v3) {
            show(sprintf("float >= *number (%y, %y)", a, b),
                auto sub () { return c418(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v4) {
            show(sprintf("float >= union<int, float> (%y, %y)", a, b),
                auto sub () { return c419(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v5) {
            show(sprintf("float >= union<int, number> (%y, %y)", a, b),
                auto sub () { return c420(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v6) {
            show(sprintf("float >= *union<int, float> (%y, %y)", a, b),
                auto sub () { return c421(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v7) {
            show(sprintf("float >= union<int, bool> (%y, %y)", a, b),
                auto sub () { return c422(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v8) {
            show(sprintf("float >= union<int, string> (%y, %y)", a, b),
                auto sub () { return c423(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v0) {
            show(sprintf("number >= int (%y, %y)", a, b),
                auto sub () { return c424(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v1) {
            show(sprintf("number >= float (%y, %y)", a, b),
                auto sub () { return c425(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v2) {
            show(sprintf("number >= number (%y, %y)", a, b),
                auto sub () { return c426(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v3) {
            show(sprintf("number >= *number (%y, %y)", a, b),
                auto sub () { return c427(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v4) {
            show(sprintf("number >= union<int, float> (%y, %y)", a, b),
                auto sub () { return c428(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v5) {
            show(sprintf("number >= union<int, number> (%y, %y)", a, b),
                auto sub () { return c429(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v6) {
            show(sprintf("number >= *union<int, float> (%y, %y)", a, b),
                auto sub () { return c430(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v7) {
            show(sprintf("number >= union<int, bool> (%y, %y)", a, b),
                auto sub () { return c431(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v8) {
            show(sprintf("number >= union<int, string> (%y, %y)", a, b),
                auto sub () { return c432(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v0) {
            show(sprintf("*number >= int (%y, %y)", a, b),
                auto sub () { return c433(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v1) {
            show(sprintf("*number >= float (%y, %y)", a, b),
                auto sub () { return c434(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v2) {
            show(sprintf("*number >= number (%y, %y)", a, b),
                auto sub () { return c435(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v3) {
            show(sprintf("*number >= *number (%y, %y)", a, b),
                auto sub () { return c436(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v4) {
            show(sprintf("*number >= union<int, float> (%y, %y)", a, b),
                auto sub () { return c437(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v5) {
            show(sprintf("*number >= union<int, number> (%y, %y)", a, b),
                auto sub () { return c438(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v6) {
            show(sprintf("*number >= *union<int, float> (%y, %y)", a, b),
                auto sub () { return c439(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v7) {
            show(sprintf("*number >= union<int, bool> (%y, %y)", a, b),
                auto sub () { return c440(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v8) {
            show(sprintf("*number >= union<int, string> (%y, %y)", a, b),
                auto sub () { return c441(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v0) {
            show(sprintf("union<int, float> >= int (%y, %y)", a, b),
                auto sub () { return c442(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v1) {
            show(sprintf("union<int, float> >= float (%y, %y)", a, b),
                auto sub () { return c443(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v2) {
            show(sprintf("union<int, float> >= number (%y, %y)", a, b),
                auto sub () { return c444(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v3) {
            show(sprintf("union<int, float> >= *number (%y, %y)", a, b),
                auto sub () { return c445(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v4) {
            show(sprintf("union<int, float> >= union<int, float> (%y, %y)", a, b),
                auto sub () { return c446(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v5) {
            show(sprintf("union<int, float> >= union<int, number> (%y, %y)", a, b),
                auto sub () { return c447(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v6) {
            show(sprintf("union<int, float> >= *union<int, float> (%y, %y)", a, b),
                auto sub () { return c448(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v7) {
            show(sprintf("union<int, float> >= union<int, bool> (%y, %y)", a, b),
                auto sub () { return c449(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v8) {
            show(sprintf("union<int, float> >= union<int, string> (%y, %y)", a, b),
                auto sub () { return c450(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v0) {
            show(sprintf("union<int, number> >= int (%y, %y)", a, b),
                auto sub () { return c451(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v1) {
            show(sprintf("union<int, number> >= float (%y, %y)", a, b),
                auto sub () { return c452(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v2) {
            show(sprintf("union<int, number> >= number (%y, %y)", a, b),
                auto sub () { return c453(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v3) {
            show(sprintf("union<int, number> >= *number (%y, %y)", a, b),
                auto sub () { return c454(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v4) {
            show(sprintf("union<int, number> >= union<int, float> (%y, %y)", a, b),
                auto sub () { return c455(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v5) {
            show(sprintf("union<int, number> >= union<int, number> (%y, %y)", a, b),
                auto sub () { return c456(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v6) {
            show(sprintf("union<int, number> >= *union<int, float> (%y, %y)", a, b),
                auto sub () { return c457(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v7) {
            show(sprintf("union<int, number> >= union<int, bool> (%y, %y)", a, b),
                auto sub () { return c458(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v8) {
            show(sprintf("union<int, number> >= union<int, string> (%y, %y)", a, b),
                auto sub () { return c459(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v0) {
            show(sprintf("*union<int, float> >= int (%y, %y)", a, b),
                auto sub () { return c460(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v1) {
            show(sprintf("*union<int, float> >= float (%y, %y)", a, b),
                auto sub () { return c461(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v2) {
            show(sprintf("*union<int, float> >= number (%y, %y)", a, b),
                auto sub () { return c462(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v3) {
            show(sprintf("*union<int, float> >= *number (%y, %y)", a, b),
                auto sub () { return c463(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v4) {
            show(sprintf("*union<int, float> >= union<int, float> (%y, %y)", a, b),
                auto sub () { return c464(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v5) {
            show(sprintf("*union<int, float> >= union<int, number> (%y, %y)", a, b),
                auto sub () { return c465(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v6) {
            show(sprintf("*union<int, float> >= *union<int, float> (%y, %y)", a, b),
                auto sub () { return c466(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v7) {
            show(sprintf("*union<int, float> >= union<int, bool> (%y, %y)", a, b),
                auto sub () { return c467(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v8) {
            show(sprintf("*union<int, float> >= union<int, string> (%y, %y)", a, b),
                auto sub () { return c468(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v0) {
            show(sprintf("union<int, bool> >= int (%y, %y)", a, b),
                auto sub () { return c469(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v1) {
            show(sprintf("union<int, bool> >= float (%y, %y)", a, b),
                auto sub () { return c470(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v2) {
            show(sprintf("union<int, bool> >= number (%y, %y)", a, b),
                auto sub () { return c471(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v3) {
            show(sprintf("union<int, bool> >= *number (%y, %y)", a, b),
                auto sub () { return c472(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v4) {
            show(sprintf("union<int, bool> >= union<int, float> (%y, %y)", a, b),
                auto sub () { return c473(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v5) {
            show(sprintf("union<int, bool> >= union<int, number> (%y, %y)", a, b),
                auto sub () { return c474(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v6) {
            show(sprintf("union<int, bool> >= *union<int, float> (%y, %y)", a, b),
                auto sub () { return c475(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v7) {
            show(sprintf("union<int, bool> >= union<int, bool> (%y, %y)", a, b),
                auto sub () { return c476(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v8) {
            show(sprintf("union<int, bool> >= union<int, string> (%y, %y)", a, b),
                auto sub () { return c477(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v0) {
            show(sprintf("union<int, string> >= int (%y, %y)", a, b),
                auto sub () { return c478(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v1) {
            show(sprintf("union<int, string> >= float (%y, %y)", a, b),
                auto sub () { return c479(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v2) {
            show(sprintf("union<int, string> >= number (%y, %y)", a, b),
                auto sub () { return c480(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v3) {
            show(sprintf("union<int, string> >= *number (%y, %y)", a, b),
                auto sub () { return c481(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v4) {
            show(sprintf("union<int, string> >= union<int, float> (%y, %y)", a, b),
                auto sub () { return c482(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v5) {
            show(sprintf("union<int, string> >= union<int, number> (%y, %y)", a, b),
                auto sub () { return c483(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v6) {
            show(sprintf("union<int, string> >= *union<int, float> (%y, %y)", a, b),
                auto sub () { return c484(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v7) {
            show(sprintf("union<int, string> >= union<int, bool> (%y, %y)", a, b),
                auto sub () { return c485(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v8) {
            show(sprintf("union<int, string> >= union<int, string> (%y, %y)", a, b),
                auto sub () { return c486(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v0) {
            show(sprintf("int <=> int (%y, %y)", a, b),
                auto sub () { return c487(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v1) {
            show(sprintf("int <=> float (%y, %y)", a, b),
                auto sub () { return c488(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v2) {
            show(sprintf("int <=> number (%y, %y)", a, b),
                auto sub () { return c489(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v3) {
            show(sprintf("int <=> *number (%y, %y)", a, b),
                auto sub () { return c490(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v4) {
            show(sprintf("int <=> union<int, float> (%y, %y)", a, b),
                auto sub () { return c491(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v5) {
            show(sprintf("int <=> union<int, number> (%y, %y)", a, b),
                auto sub () { return c492(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v6) {
            show(sprintf("int <=> *union<int, float> (%y, %y)", a, b),
                auto sub () { return c493(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v7) {
            show(sprintf("int <=> union<int, bool> (%y, %y)", a, b),
                auto sub () { return c494(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v8) {
            show(sprintf("int <=> union<int, string> (%y, %y)", a, b),
                auto sub () { return c495(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v0) {
            show(sprintf("float <=> int (%y, %y)", a, b),
                auto sub () { return c496(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v1) {
            show(sprintf("float <=> float (%y, %y)", a, b),
                auto sub () { return c497(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v2) {
            show(sprintf("float <=> number (%y, %y)", a, b),
                auto sub () { return c498(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v3) {
            show(sprintf("float <=> *number (%y, %y)", a, b),
                auto sub () { return c499(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v4) {
            show(sprintf("float <=> union<int, float> (%y, %y)", a, b),
                auto sub () { return c500(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v5) {
            show(sprintf("float <=> union<int, number> (%y, %y)", a, b),
                auto sub () { return c501(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v6) {
            show(sprintf("float <=> *union<int, float> (%y, %y)", a, b),
                auto sub () { return c502(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v7) {
            show(sprintf("float <=> union<int, bool> (%y, %y)", a, b),
                auto sub () { return c503(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v8) {
            show(sprintf("float <=> union<int, string> (%y, %y)", a, b),
                auto sub () { return c504(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v0) {
            show(sprintf("number <=> int (%y, %y)", a, b),
                auto sub () { return c505(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v1) {
            show(sprintf("number <=> float (%y, %y)", a, b),
                auto sub () { return c506(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v2) {
            show(sprintf("number <=> number (%y, %y)", a, b),
                auto sub () { return c507(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v3) {
            show(sprintf("number <=> *number (%y, %y)", a, b),
                auto sub () { return c508(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v4) {
            show(sprintf("number <=> union<int, float> (%y, %y)", a, b),
                auto sub () { return c509(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v5) {
            show(sprintf("number <=> union<int, number> (%y, %y)", a, b),
                auto sub () { return c510(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v6) {
            show(sprintf("number <=> *union<int, float> (%y, %y)", a, b),
                auto sub () { return c511(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v7) {
            show(sprintf("number <=> union<int, bool> (%y, %y)", a, b),
                auto sub () { return c512(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v8) {
            show(sprintf("number <=> union<int, string> (%y, %y)", a, b),
                auto sub () { return c513(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v0) {
            show(sprintf("*number <=> int (%y, %y)", a, b),
                auto sub () { return c514(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v1) {
            show(sprintf("*number <=> float (%y, %y)", a, b),
                auto sub () { return c515(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v2) {
            show(sprintf("*number <=> number (%y, %y)", a, b),
                auto sub () { return c516(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v3) {
            show(sprintf("*number <=> *number (%y, %y)", a, b),
                auto sub () { return c517(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v4) {
            show(sprintf("*number <=> union<int, float> (%y, %y)", a, b),
                auto sub () { return c518(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v5) {
            show(sprintf("*number <=> union<int, number> (%y, %y)", a, b),
                auto sub () { return c519(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v6) {
            show(sprintf("*number <=> *union<int, float> (%y, %y)", a, b),
                auto sub () { return c520(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v7) {
            show(sprintf("*number <=> union<int, bool> (%y, %y)", a, b),
                auto sub () { return c521(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v8) {
            show(sprintf("*number <=> union<int, string> (%y, %y)", a, b),
                auto sub () { return c522(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v0) {
            show(sprintf("union<int, float> <=> int (%y, %y)", a, b),
                auto sub () { return c523(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v1) {
            show(sprintf("union<int, float> <=> float (%y, %y)", a, b),
                auto sub () { return c524(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v2) {
            show(sprintf("union<int, float> <=> number (%y, %y)", a, b),
                auto sub () { return c525(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v3) {
            show(sprintf("union<int, float> <=> *number (%y, %y)", a, b),
                auto sub () { return c526(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v4) {
            show(sprintf("union<int, float> <=> union<int, float> (%y, %y)", a, b),
                auto sub () { return c527(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v5) {
            show(sprintf("union<int, float> <=> union<int, number> (%y, %y)", a, b),
                auto sub () { return c528(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v6) {
            show(sprintf("union<int, float> <=> *union<int, float> (%y, %y)", a, b),
                auto sub () { return c529(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v7) {
            show(sprintf("union<int, float> <=> union<int, bool> (%y, %y)", a, b),
                auto sub () { return c530(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v8) {
            show(sprintf("union<int, float> <=> union<int, string> (%y, %y)", a, b),
                auto sub () { return c531(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v0) {
            show(sprintf("union<int, number> <=> int (%y, %y)", a, b),
                auto sub () { return c532(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v1) {
            show(sprintf("union<int, number> <=> float (%y, %y)", a, b),
                auto sub () { return c533(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v2) {
            show(sprintf("union<int, number> <=> number (%y, %y)", a, b),
                auto sub () { return c534(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v3) {
            show(sprintf("union<int, number> <=> *number (%y, %y)", a, b),
                auto sub () { return c535(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v4) {
            show(sprintf("union<int, number> <=> union<int, float> (%y, %y)", a, b),
                auto sub () { return c536(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v5) {
            show(sprintf("union<int, number> <=> union<int, number> (%y, %y)", a, b),
                auto sub () { return c537(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v6) {
            show(sprintf("union<int, number> <=> *union<int, float> (%y, %y)", a, b),
                auto sub () { return c538(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v7) {
            show(sprintf("union<int, number> <=> union<int, bool> (%y, %y)", a, b),
                auto sub () { return c539(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v8) {
            show(sprintf("union<int, number> <=> union<int, string> (%y, %y)", a, b),
                auto sub () { return c540(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v0) {
            show(sprintf("*union<int, float> <=> int (%y, %y)", a, b),
                auto sub () { return c541(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v1) {
            show(sprintf("*union<int, float> <=> float (%y, %y)", a, b),
                auto sub () { return c542(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v2) {
            show(sprintf("*union<int, float> <=> number (%y, %y)", a, b),
                auto sub () { return c543(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v3) {
            show(sprintf("*union<int, float> <=> *number (%y, %y)", a, b),
                auto sub () { return c544(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v4) {
            show(sprintf("*union<int, float> <=> union<int, float> (%y, %y)", a, b),
                auto sub () { return c545(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v5) {
            show(sprintf("*union<int, float> <=> union<int, number> (%y, %y)", a, b),
                auto sub () { return c546(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v6) {
            show(sprintf("*union<int, float> <=> *union<int, float> (%y, %y)", a, b),
                auto sub () { return c547(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v7) {
            show(sprintf("*union<int, float> <=> union<int, bool> (%y, %y)", a, b),
                auto sub () { return c548(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v8) {
            show(sprintf("*union<int, float> <=> union<int, string> (%y, %y)", a, b),
                auto sub () { return c549(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v0) {
            show(sprintf("union<int, bool> <=> int (%y, %y)", a, b),
                auto sub () { return c550(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v1) {
            show(sprintf("union<int, bool> <=> float (%y, %y)", a, b),
                auto sub () { return c551(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v2) {
            show(sprintf("union<int, bool> <=> number (%y, %y)", a, b),
                auto sub () { return c552(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v3) {
            show(sprintf("union<int, bool> <=> *number (%y, %y)", a, b),
                auto sub () { return c553(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v4) {
            show(sprintf("union<int, bool> <=> union<int, float> (%y, %y)", a, b),
                auto sub () { return c554(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v5) {
            show(sprintf("union<int, bool> <=> union<int, number> (%y, %y)", a, b),
                auto sub () { return c555(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v6) {
            show(sprintf("union<int, bool> <=> *union<int, float> (%y, %y)", a, b),
                auto sub () { return c556(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v7) {
            show(sprintf("union<int, bool> <=> union<int, bool> (%y, %y)", a, b),
                auto sub () { return c557(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v8) {
            show(sprintf("union<int, bool> <=> union<int, string> (%y, %y)", a, b),
                auto sub () { return c558(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v0) {
            show(sprintf("union<int, string> <=> int (%y, %y)", a, b),
                auto sub () { return c559(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v1) {
            show(sprintf("union<int, string> <=> float (%y, %y)", a, b),
                auto sub () { return c560(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v2) {
            show(sprintf("union<int, string> <=> number (%y, %y)", a, b),
                auto sub () { return c561(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v3) {
            show(sprintf("union<int, string> <=> *number (%y, %y)", a, b),
                auto sub () { return c562(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v4) {
            show(sprintf("union<int, string> <=> union<int, float> (%y, %y)", a, b),
                auto sub () { return c563(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v5) {
            show(sprintf("union<int, string> <=> union<int, number> (%y, %y)", a, b),
                auto sub () { return c564(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v6) {
            show(sprintf("union<int, string> <=> *union<int, float> (%y, %y)", a, b),
                auto sub () { return c565(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v7) {
            show(sprintf("union<int, string> <=> union<int, bool> (%y, %y)", a, b),
                auto sub () { return c566(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v8) {
            show(sprintf("union<int, string> <=> union<int, string> (%y, %y)", a, b),
                auto sub () { return c567(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v0) {
            show(sprintf("int === int (%y, %y)", a, b),
                auto sub () { return c568(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v1) {
            show(sprintf("int === float (%y, %y)", a, b),
                auto sub () { return c569(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v2) {
            show(sprintf("int === number (%y, %y)", a, b),
                auto sub () { return c570(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v3) {
            show(sprintf("int === *number (%y, %y)", a, b),
                auto sub () { return c571(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v4) {
            show(sprintf("int === union<int, float> (%y, %y)", a, b),
                auto sub () { return c572(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v5) {
            show(sprintf("int === union<int, number> (%y, %y)", a, b),
                auto sub () { return c573(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v6) {
            show(sprintf("int === *union<int, float> (%y, %y)", a, b),
                auto sub () { return c574(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v7) {
            show(sprintf("int === union<int, bool> (%y, %y)", a, b),
                auto sub () { return c575(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v8) {
            show(sprintf("int === union<int, string> (%y, %y)", a, b),
                auto sub () { return c576(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v0) {
            show(sprintf("float === int (%y, %y)", a, b),
                auto sub () { return c577(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v1) {
            show(sprintf("float === float (%y, %y)", a, b),
                auto sub () { return c578(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v2) {
            show(sprintf("float === number (%y, %y)", a, b),
                auto sub () { return c579(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v3) {
            show(sprintf("float === *number (%y, %y)", a, b),
                auto sub () { return c580(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v4) {
            show(sprintf("float === union<int, float> (%y, %y)", a, b),
                auto sub () { return c581(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v5) {
            show(sprintf("float === union<int, number> (%y, %y)", a, b),
                auto sub () { return c582(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v6) {
            show(sprintf("float === *union<int, float> (%y, %y)", a, b),
                auto sub () { return c583(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v7) {
            show(sprintf("float === union<int, bool> (%y, %y)", a, b),
                auto sub () { return c584(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v8) {
            show(sprintf("float === union<int, string> (%y, %y)", a, b),
                auto sub () { return c585(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v0) {
            show(sprintf("number === int (%y, %y)", a, b),
                auto sub () { return c586(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v1) {
            show(sprintf("number === float (%y, %y)", a, b),
                auto sub () { return c587(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v2) {
            show(sprintf("number === number (%y, %y)", a, b),
                auto sub () { return c588(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v3) {
            show(sprintf("number === *number (%y, %y)", a, b),
                auto sub () { return c589(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v4) {
            show(sprintf("number === union<int, float> (%y, %y)", a, b),
                auto sub () { return c590(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v5) {
            show(sprintf("number === union<int, number> (%y, %y)", a, b),
                auto sub () { return c591(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v6) {
            show(sprintf("number === *union<int, float> (%y, %y)", a, b),
                auto sub () { return c592(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v7) {
            show(sprintf("number === union<int, bool> (%y, %y)", a, b),
                auto sub () { return c593(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v8) {
            show(sprintf("number === union<int, string> (%y, %y)", a, b),
                auto sub () { return c594(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v0) {
            show(sprintf("*number === int (%y, %y)", a, b),
                auto sub () { return c595(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v1) {
            show(sprintf("*number === float (%y, %y)", a, b),
                auto sub () { return c596(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v2) {
            show(sprintf("*number === number (%y, %y)", a, b),
                auto sub () { return c597(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v3) {
            show(sprintf("*number === *number (%y, %y)", a, b),
                auto sub () { return c598(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v4) {
            show(sprintf("*number === union<int, float> (%y, %y)", a, b),
                auto sub () { return c599(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v5) {
            show(sprintf("*number === union<int, number> (%y, %y)", a, b),
                auto sub () { return c600(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v6) {
            show(sprintf("*number === *union<int, float> (%y, %y)", a, b),
                auto sub () { return c601(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v7) {
            show(sprintf("*number === union<int, bool> (%y, %y)", a, b),
                auto sub () { return c602(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v8) {
            show(sprintf("*number === union<int, string> (%y, %y)", a, b),
                auto sub () { return c603(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v0) {
            show(sprintf("union<int, float> === int (%y, %y)", a, b),
                auto sub () { return c604(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v1) {
            show(sprintf("union<int, float> === float (%y, %y)", a, b),
                auto sub () { return c605(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v2) {
            show(sprintf("union<int, float> === number (%y, %y)", a, b),
                auto sub () { return c606(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v3) {
            show(sprintf("union<int, float> === *number (%y, %y)", a, b),
                auto sub () { return c607(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v4) {
            show(sprintf("union<int, float> === union<int, float> (%y, %y)", a, b),
                auto sub () { return c608(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v5) {
            show(sprintf("union<int, float> === union<int, number> (%y, %y)", a, b),
                auto sub () { return c609(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v6) {
            show(sprintf("union<int, float> === *union<int, float> (%y, %y)", a, b),
                auto sub () { return c610(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v7) {
            show(sprintf("union<int, float> === union<int, bool> (%y, %y)", a, b),
                auto sub () { return c611(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v8) {
            show(sprintf("union<int, float> === union<int, string> (%y, %y)", a, b),
                auto sub () { return c612(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v0) {
            show(sprintf("union<int, number> === int (%y, %y)", a, b),
                auto sub () { return c613(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v1) {
            show(sprintf("union<int, number> === float (%y, %y)", a, b),
                auto sub () { return c614(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v2) {
            show(sprintf("union<int, number> === number (%y, %y)", a, b),
                auto sub () { return c615(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v3) {
            show(sprintf("union<int, number> === *number (%y, %y)", a, b),
                auto sub () { return c616(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v4) {
            show(sprintf("union<int, number> === union<int, float> (%y, %y)", a, b),
                auto sub () { return c617(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v5) {
            show(sprintf("union<int, number> === union<int, number> (%y, %y)", a, b),
                auto sub () { return c618(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v6) {
            show(sprintf("union<int, number> === *union<int, float> (%y, %y)", a, b),
                auto sub () { return c619(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v7) {
            show(sprintf("union<int, number> === union<int, bool> (%y, %y)", a, b),
                auto sub () { return c620(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v8) {
            show(sprintf("union<int, number> === union<int, string> (%y, %y)", a, b),
                auto sub () { return c621(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v0) {
            show(sprintf("*union<int, float> === int (%y, %y)", a, b),
                auto sub () { return c622(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v1) {
            show(sprintf("*union<int, float> === float (%y, %y)", a, b),
                auto sub () { return c623(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v2) {
            show(sprintf("*union<int, float> === number (%y, %y)", a, b),
                auto sub () { return c624(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v3) {
            show(sprintf("*union<int, float> === *number (%y, %y)", a, b),
                auto sub () { return c625(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v4) {
            show(sprintf("*union<int, float> === union<int, float> (%y, %y)", a, b),
                auto sub () { return c626(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v5) {
            show(sprintf("*union<int, float> === union<int, number> (%y, %y)", a, b),
                auto sub () { return c627(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v6) {
            show(sprintf("*union<int, float> === *union<int, float> (%y, %y)", a, b),
                auto sub () { return c628(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v7) {
            show(sprintf("*union<int, float> === union<int, bool> (%y, %y)", a, b),
                auto sub () { return c629(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v8) {
            show(sprintf("*union<int, float> === union<int, string> (%y, %y)", a, b),
                auto sub () { return c630(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v0) {
            show(sprintf("union<int, bool> === int (%y, %y)", a, b),
                auto sub () { return c631(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v1) {
            show(sprintf("union<int, bool> === float (%y, %y)", a, b),
                auto sub () { return c632(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v2) {
            show(sprintf("union<int, bool> === number (%y, %y)", a, b),
                auto sub () { return c633(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v3) {
            show(sprintf("union<int, bool> === *number (%y, %y)", a, b),
                auto sub () { return c634(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v4) {
            show(sprintf("union<int, bool> === union<int, float> (%y, %y)", a, b),
                auto sub () { return c635(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v5) {
            show(sprintf("union<int, bool> === union<int, number> (%y, %y)", a, b),
                auto sub () { return c636(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v6) {
            show(sprintf("union<int, bool> === *union<int, float> (%y, %y)", a, b),
                auto sub () { return c637(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v7) {
            show(sprintf("union<int, bool> === union<int, bool> (%y, %y)", a, b),
                auto sub () { return c638(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v8) {
            show(sprintf("union<int, bool> === union<int, string> (%y, %y)", a, b),
                auto sub () { return c639(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v0) {
            show(sprintf("union<int, string> === int (%y, %y)", a, b),
                auto sub () { return c640(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v1) {
            show(sprintf("union<int, string> === float (%y, %y)", a, b),
                auto sub () { return c641(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v2) {
            show(sprintf("union<int, string> === number (%y, %y)", a, b),
                auto sub () { return c642(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v3) {
            show(sprintf("union<int, string> === *number (%y, %y)", a, b),
                auto sub () { return c643(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v4) {
            show(sprintf("union<int, string> === union<int, float> (%y, %y)", a, b),
                auto sub () { return c644(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v5) {
            show(sprintf("union<int, string> === union<int, number> (%y, %y)", a, b),
                auto sub () { return c645(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v6) {
            show(sprintf("union<int, string> === *union<int, float> (%y, %y)", a, b),
                auto sub () { return c646(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v7) {
            show(sprintf("union<int, string> === union<int, bool> (%y, %y)", a, b),
                auto sub () { return c647(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v8) {
            show(sprintf("union<int, string> === union<int, string> (%y, %y)", a, b),
                auto sub () { return c648(a, b); });
        }
    }
}

main();
