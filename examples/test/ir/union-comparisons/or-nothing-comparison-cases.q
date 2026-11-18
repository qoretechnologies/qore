#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the cases of UnionComparisons.qc, parts 3 and 4 (generated for the operand types and values below): each function
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

auto sub c1(int x, *int y) {
    return x == y;
}
auto sub c2(int x, *float y) {
    return x == y;
}
auto sub c3(int x, bool y) {
    return x == y;
}
auto sub c4(int x, *bool y) {
    return x == y;
}
auto sub c5(int x, softint y) {
    return x == y;
}
auto sub c6(int x, *softint y) {
    return x == y;
}
auto sub c7(int x, softfloat y) {
    return x == y;
}
auto sub c8(int x, *softfloat y) {
    return x == y;
}
auto sub c9(float x, *int y) {
    return x == y;
}
auto sub c10(float x, *float y) {
    return x == y;
}
auto sub c11(float x, bool y) {
    return x == y;
}
auto sub c12(float x, *bool y) {
    return x == y;
}
auto sub c13(float x, softint y) {
    return x == y;
}
auto sub c14(float x, *softint y) {
    return x == y;
}
auto sub c15(float x, softfloat y) {
    return x == y;
}
auto sub c16(float x, *softfloat y) {
    return x == y;
}
auto sub c17(number x, *int y) {
    return x == y;
}
auto sub c18(number x, *float y) {
    return x == y;
}
auto sub c19(number x, bool y) {
    return x == y;
}
auto sub c20(number x, *bool y) {
    return x == y;
}
auto sub c21(number x, softint y) {
    return x == y;
}
auto sub c22(number x, *softint y) {
    return x == y;
}
auto sub c23(number x, softfloat y) {
    return x == y;
}
auto sub c24(number x, *softfloat y) {
    return x == y;
}
auto sub c25(*number x, *int y) {
    return x == y;
}
auto sub c26(*number x, *float y) {
    return x == y;
}
auto sub c27(*number x, bool y) {
    return x == y;
}
auto sub c28(*number x, *bool y) {
    return x == y;
}
auto sub c29(*number x, softint y) {
    return x == y;
}
auto sub c30(*number x, *softint y) {
    return x == y;
}
auto sub c31(*number x, softfloat y) {
    return x == y;
}
auto sub c32(*number x, *softfloat y) {
    return x == y;
}
auto sub c33(union<int, float> x, *int y) {
    return x == y;
}
auto sub c34(union<int, float> x, *float y) {
    return x == y;
}
auto sub c35(union<int, float> x, bool y) {
    return x == y;
}
auto sub c36(union<int, float> x, *bool y) {
    return x == y;
}
auto sub c37(union<int, float> x, softint y) {
    return x == y;
}
auto sub c38(union<int, float> x, *softint y) {
    return x == y;
}
auto sub c39(union<int, float> x, softfloat y) {
    return x == y;
}
auto sub c40(union<int, float> x, *softfloat y) {
    return x == y;
}
auto sub c41(union<int, number> x, *int y) {
    return x == y;
}
auto sub c42(union<int, number> x, *float y) {
    return x == y;
}
auto sub c43(union<int, number> x, bool y) {
    return x == y;
}
auto sub c44(union<int, number> x, *bool y) {
    return x == y;
}
auto sub c45(union<int, number> x, softint y) {
    return x == y;
}
auto sub c46(union<int, number> x, *softint y) {
    return x == y;
}
auto sub c47(union<int, number> x, softfloat y) {
    return x == y;
}
auto sub c48(union<int, number> x, *softfloat y) {
    return x == y;
}
auto sub c49(*union<int, float> x, *int y) {
    return x == y;
}
auto sub c50(*union<int, float> x, *float y) {
    return x == y;
}
auto sub c51(*union<int, float> x, bool y) {
    return x == y;
}
auto sub c52(*union<int, float> x, *bool y) {
    return x == y;
}
auto sub c53(*union<int, float> x, softint y) {
    return x == y;
}
auto sub c54(*union<int, float> x, *softint y) {
    return x == y;
}
auto sub c55(*union<int, float> x, softfloat y) {
    return x == y;
}
auto sub c56(*union<int, float> x, *softfloat y) {
    return x == y;
}
auto sub c57(union<int, bool> x, *int y) {
    return x == y;
}
auto sub c58(union<int, bool> x, *float y) {
    return x == y;
}
auto sub c59(union<int, bool> x, bool y) {
    return x == y;
}
auto sub c60(union<int, bool> x, *bool y) {
    return x == y;
}
auto sub c61(union<int, bool> x, softint y) {
    return x == y;
}
auto sub c62(union<int, bool> x, *softint y) {
    return x == y;
}
auto sub c63(union<int, bool> x, softfloat y) {
    return x == y;
}
auto sub c64(union<int, bool> x, *softfloat y) {
    return x == y;
}
auto sub c65(union<int, string> x, *int y) {
    return x == y;
}
auto sub c66(union<int, string> x, *float y) {
    return x == y;
}
auto sub c67(union<int, string> x, bool y) {
    return x == y;
}
auto sub c68(union<int, string> x, *bool y) {
    return x == y;
}
auto sub c69(union<int, string> x, softint y) {
    return x == y;
}
auto sub c70(union<int, string> x, *softint y) {
    return x == y;
}
auto sub c71(union<int, string> x, softfloat y) {
    return x == y;
}
auto sub c72(union<int, string> x, *softfloat y) {
    return x == y;
}
auto sub c73(*int x, int y) {
    return x == y;
}
auto sub c74(*int x, float y) {
    return x == y;
}
auto sub c75(*int x, number y) {
    return x == y;
}
auto sub c76(*int x, *number y) {
    return x == y;
}
auto sub c77(*int x, union<int, float> y) {
    return x == y;
}
auto sub c78(*int x, union<int, number> y) {
    return x == y;
}
auto sub c79(*int x, *union<int, float> y) {
    return x == y;
}
auto sub c80(*int x, union<int, bool> y) {
    return x == y;
}
auto sub c81(*int x, union<int, string> y) {
    return x == y;
}
auto sub c82(*int x, *int y) {
    return x == y;
}
auto sub c83(*int x, *float y) {
    return x == y;
}
auto sub c84(*int x, bool y) {
    return x == y;
}
auto sub c85(*int x, *bool y) {
    return x == y;
}
auto sub c86(*int x, softint y) {
    return x == y;
}
auto sub c87(*int x, *softint y) {
    return x == y;
}
auto sub c88(*int x, softfloat y) {
    return x == y;
}
auto sub c89(*int x, *softfloat y) {
    return x == y;
}
auto sub c90(*float x, int y) {
    return x == y;
}
auto sub c91(*float x, float y) {
    return x == y;
}
auto sub c92(*float x, number y) {
    return x == y;
}
auto sub c93(*float x, *number y) {
    return x == y;
}
auto sub c94(*float x, union<int, float> y) {
    return x == y;
}
auto sub c95(*float x, union<int, number> y) {
    return x == y;
}
auto sub c96(*float x, *union<int, float> y) {
    return x == y;
}
auto sub c97(*float x, union<int, bool> y) {
    return x == y;
}
auto sub c98(*float x, union<int, string> y) {
    return x == y;
}
auto sub c99(*float x, *int y) {
    return x == y;
}
auto sub c100(*float x, *float y) {
    return x == y;
}
auto sub c101(*float x, bool y) {
    return x == y;
}
auto sub c102(*float x, *bool y) {
    return x == y;
}
auto sub c103(*float x, softint y) {
    return x == y;
}
auto sub c104(*float x, *softint y) {
    return x == y;
}
auto sub c105(*float x, softfloat y) {
    return x == y;
}
auto sub c106(*float x, *softfloat y) {
    return x == y;
}
auto sub c107(bool x, int y) {
    return x == y;
}
auto sub c108(bool x, float y) {
    return x == y;
}
auto sub c109(bool x, number y) {
    return x == y;
}
auto sub c110(bool x, *number y) {
    return x == y;
}
auto sub c111(bool x, union<int, float> y) {
    return x == y;
}
auto sub c112(bool x, union<int, number> y) {
    return x == y;
}
auto sub c113(bool x, *union<int, float> y) {
    return x == y;
}
auto sub c114(bool x, union<int, bool> y) {
    return x == y;
}
auto sub c115(bool x, union<int, string> y) {
    return x == y;
}
auto sub c116(bool x, *int y) {
    return x == y;
}
auto sub c117(bool x, *float y) {
    return x == y;
}
auto sub c118(bool x, bool y) {
    return x == y;
}
auto sub c119(bool x, *bool y) {
    return x == y;
}
auto sub c120(bool x, softint y) {
    return x == y;
}
auto sub c121(bool x, *softint y) {
    return x == y;
}
auto sub c122(bool x, softfloat y) {
    return x == y;
}
auto sub c123(bool x, *softfloat y) {
    return x == y;
}
auto sub c124(*bool x, int y) {
    return x == y;
}
auto sub c125(*bool x, float y) {
    return x == y;
}
auto sub c126(*bool x, number y) {
    return x == y;
}
auto sub c127(*bool x, *number y) {
    return x == y;
}
auto sub c128(*bool x, union<int, float> y) {
    return x == y;
}
auto sub c129(*bool x, union<int, number> y) {
    return x == y;
}
auto sub c130(*bool x, *union<int, float> y) {
    return x == y;
}
auto sub c131(*bool x, union<int, bool> y) {
    return x == y;
}
auto sub c132(*bool x, union<int, string> y) {
    return x == y;
}
auto sub c133(*bool x, *int y) {
    return x == y;
}
auto sub c134(*bool x, *float y) {
    return x == y;
}
auto sub c135(*bool x, bool y) {
    return x == y;
}
auto sub c136(*bool x, *bool y) {
    return x == y;
}
auto sub c137(*bool x, softint y) {
    return x == y;
}
auto sub c138(*bool x, *softint y) {
    return x == y;
}
auto sub c139(*bool x, softfloat y) {
    return x == y;
}
auto sub c140(*bool x, *softfloat y) {
    return x == y;
}
auto sub c141(softint x, int y) {
    return x == y;
}
auto sub c142(softint x, float y) {
    return x == y;
}
auto sub c143(softint x, number y) {
    return x == y;
}
auto sub c144(softint x, *number y) {
    return x == y;
}
auto sub c145(softint x, union<int, float> y) {
    return x == y;
}
auto sub c146(softint x, union<int, number> y) {
    return x == y;
}
auto sub c147(softint x, *union<int, float> y) {
    return x == y;
}
auto sub c148(softint x, union<int, bool> y) {
    return x == y;
}
auto sub c149(softint x, union<int, string> y) {
    return x == y;
}
auto sub c150(softint x, *int y) {
    return x == y;
}
auto sub c151(softint x, *float y) {
    return x == y;
}
auto sub c152(softint x, bool y) {
    return x == y;
}
auto sub c153(softint x, *bool y) {
    return x == y;
}
auto sub c154(softint x, softint y) {
    return x == y;
}
auto sub c155(softint x, *softint y) {
    return x == y;
}
auto sub c156(softint x, softfloat y) {
    return x == y;
}
auto sub c157(softint x, *softfloat y) {
    return x == y;
}
auto sub c158(*softint x, int y) {
    return x == y;
}
auto sub c159(*softint x, float y) {
    return x == y;
}
auto sub c160(*softint x, number y) {
    return x == y;
}
auto sub c161(*softint x, *number y) {
    return x == y;
}
auto sub c162(*softint x, union<int, float> y) {
    return x == y;
}
auto sub c163(*softint x, union<int, number> y) {
    return x == y;
}
auto sub c164(*softint x, *union<int, float> y) {
    return x == y;
}
auto sub c165(*softint x, union<int, bool> y) {
    return x == y;
}
auto sub c166(*softint x, union<int, string> y) {
    return x == y;
}
auto sub c167(*softint x, *int y) {
    return x == y;
}
auto sub c168(*softint x, *float y) {
    return x == y;
}
auto sub c169(*softint x, bool y) {
    return x == y;
}
auto sub c170(*softint x, *bool y) {
    return x == y;
}
auto sub c171(*softint x, softint y) {
    return x == y;
}
auto sub c172(*softint x, *softint y) {
    return x == y;
}
auto sub c173(*softint x, softfloat y) {
    return x == y;
}
auto sub c174(*softint x, *softfloat y) {
    return x == y;
}
auto sub c175(softfloat x, int y) {
    return x == y;
}
auto sub c176(softfloat x, float y) {
    return x == y;
}
auto sub c177(softfloat x, number y) {
    return x == y;
}
auto sub c178(softfloat x, *number y) {
    return x == y;
}
auto sub c179(softfloat x, union<int, float> y) {
    return x == y;
}
auto sub c180(softfloat x, union<int, number> y) {
    return x == y;
}
auto sub c181(softfloat x, *union<int, float> y) {
    return x == y;
}
auto sub c182(softfloat x, union<int, bool> y) {
    return x == y;
}
auto sub c183(softfloat x, union<int, string> y) {
    return x == y;
}
auto sub c184(softfloat x, *int y) {
    return x == y;
}
auto sub c185(softfloat x, *float y) {
    return x == y;
}
auto sub c186(softfloat x, bool y) {
    return x == y;
}
auto sub c187(softfloat x, *bool y) {
    return x == y;
}
auto sub c188(softfloat x, softint y) {
    return x == y;
}
auto sub c189(softfloat x, *softint y) {
    return x == y;
}
auto sub c190(softfloat x, softfloat y) {
    return x == y;
}
auto sub c191(softfloat x, *softfloat y) {
    return x == y;
}
auto sub c192(*softfloat x, int y) {
    return x == y;
}
auto sub c193(*softfloat x, float y) {
    return x == y;
}
auto sub c194(*softfloat x, number y) {
    return x == y;
}
auto sub c195(*softfloat x, *number y) {
    return x == y;
}
auto sub c196(*softfloat x, union<int, float> y) {
    return x == y;
}
auto sub c197(*softfloat x, union<int, number> y) {
    return x == y;
}
auto sub c198(*softfloat x, *union<int, float> y) {
    return x == y;
}
auto sub c199(*softfloat x, union<int, bool> y) {
    return x == y;
}
auto sub c200(*softfloat x, union<int, string> y) {
    return x == y;
}
auto sub c201(*softfloat x, *int y) {
    return x == y;
}
auto sub c202(*softfloat x, *float y) {
    return x == y;
}
auto sub c203(*softfloat x, bool y) {
    return x == y;
}
auto sub c204(*softfloat x, *bool y) {
    return x == y;
}
auto sub c205(*softfloat x, softint y) {
    return x == y;
}
auto sub c206(*softfloat x, *softint y) {
    return x == y;
}
auto sub c207(*softfloat x, softfloat y) {
    return x == y;
}
auto sub c208(*softfloat x, *softfloat y) {
    return x == y;
}
auto sub c209(int x, *int y) {
    return x != y;
}
auto sub c210(int x, *float y) {
    return x != y;
}
auto sub c211(int x, bool y) {
    return x != y;
}
auto sub c212(int x, *bool y) {
    return x != y;
}
auto sub c213(int x, softint y) {
    return x != y;
}
auto sub c214(int x, *softint y) {
    return x != y;
}
auto sub c215(int x, softfloat y) {
    return x != y;
}
auto sub c216(int x, *softfloat y) {
    return x != y;
}
auto sub c217(float x, *int y) {
    return x != y;
}
auto sub c218(float x, *float y) {
    return x != y;
}
auto sub c219(float x, bool y) {
    return x != y;
}
auto sub c220(float x, *bool y) {
    return x != y;
}
auto sub c221(float x, softint y) {
    return x != y;
}
auto sub c222(float x, *softint y) {
    return x != y;
}
auto sub c223(float x, softfloat y) {
    return x != y;
}
auto sub c224(float x, *softfloat y) {
    return x != y;
}
auto sub c225(number x, *int y) {
    return x != y;
}
auto sub c226(number x, *float y) {
    return x != y;
}
auto sub c227(number x, bool y) {
    return x != y;
}
auto sub c228(number x, *bool y) {
    return x != y;
}
auto sub c229(number x, softint y) {
    return x != y;
}
auto sub c230(number x, *softint y) {
    return x != y;
}
auto sub c231(number x, softfloat y) {
    return x != y;
}
auto sub c232(number x, *softfloat y) {
    return x != y;
}
auto sub c233(*number x, *int y) {
    return x != y;
}
auto sub c234(*number x, *float y) {
    return x != y;
}
auto sub c235(*number x, bool y) {
    return x != y;
}
auto sub c236(*number x, *bool y) {
    return x != y;
}
auto sub c237(*number x, softint y) {
    return x != y;
}
auto sub c238(*number x, *softint y) {
    return x != y;
}
auto sub c239(*number x, softfloat y) {
    return x != y;
}
auto sub c240(*number x, *softfloat y) {
    return x != y;
}
auto sub c241(union<int, float> x, *int y) {
    return x != y;
}
auto sub c242(union<int, float> x, *float y) {
    return x != y;
}
auto sub c243(union<int, float> x, bool y) {
    return x != y;
}
auto sub c244(union<int, float> x, *bool y) {
    return x != y;
}
auto sub c245(union<int, float> x, softint y) {
    return x != y;
}
auto sub c246(union<int, float> x, *softint y) {
    return x != y;
}
auto sub c247(union<int, float> x, softfloat y) {
    return x != y;
}
auto sub c248(union<int, float> x, *softfloat y) {
    return x != y;
}
auto sub c249(union<int, number> x, *int y) {
    return x != y;
}
auto sub c250(union<int, number> x, *float y) {
    return x != y;
}
auto sub c251(union<int, number> x, bool y) {
    return x != y;
}
auto sub c252(union<int, number> x, *bool y) {
    return x != y;
}
auto sub c253(union<int, number> x, softint y) {
    return x != y;
}
auto sub c254(union<int, number> x, *softint y) {
    return x != y;
}
auto sub c255(union<int, number> x, softfloat y) {
    return x != y;
}
auto sub c256(union<int, number> x, *softfloat y) {
    return x != y;
}
auto sub c257(*union<int, float> x, *int y) {
    return x != y;
}
auto sub c258(*union<int, float> x, *float y) {
    return x != y;
}
auto sub c259(*union<int, float> x, bool y) {
    return x != y;
}
auto sub c260(*union<int, float> x, *bool y) {
    return x != y;
}
auto sub c261(*union<int, float> x, softint y) {
    return x != y;
}
auto sub c262(*union<int, float> x, *softint y) {
    return x != y;
}
auto sub c263(*union<int, float> x, softfloat y) {
    return x != y;
}
auto sub c264(*union<int, float> x, *softfloat y) {
    return x != y;
}
auto sub c265(union<int, bool> x, *int y) {
    return x != y;
}
auto sub c266(union<int, bool> x, *float y) {
    return x != y;
}
auto sub c267(union<int, bool> x, bool y) {
    return x != y;
}
auto sub c268(union<int, bool> x, *bool y) {
    return x != y;
}
auto sub c269(union<int, bool> x, softint y) {
    return x != y;
}
auto sub c270(union<int, bool> x, *softint y) {
    return x != y;
}
auto sub c271(union<int, bool> x, softfloat y) {
    return x != y;
}
auto sub c272(union<int, bool> x, *softfloat y) {
    return x != y;
}
auto sub c273(union<int, string> x, *int y) {
    return x != y;
}
auto sub c274(union<int, string> x, *float y) {
    return x != y;
}
auto sub c275(union<int, string> x, bool y) {
    return x != y;
}
auto sub c276(union<int, string> x, *bool y) {
    return x != y;
}
auto sub c277(union<int, string> x, softint y) {
    return x != y;
}
auto sub c278(union<int, string> x, *softint y) {
    return x != y;
}
auto sub c279(union<int, string> x, softfloat y) {
    return x != y;
}
auto sub c280(union<int, string> x, *softfloat y) {
    return x != y;
}
auto sub c281(*int x, int y) {
    return x != y;
}
auto sub c282(*int x, float y) {
    return x != y;
}
auto sub c283(*int x, number y) {
    return x != y;
}
auto sub c284(*int x, *number y) {
    return x != y;
}
auto sub c285(*int x, union<int, float> y) {
    return x != y;
}
auto sub c286(*int x, union<int, number> y) {
    return x != y;
}
auto sub c287(*int x, *union<int, float> y) {
    return x != y;
}
auto sub c288(*int x, union<int, bool> y) {
    return x != y;
}
auto sub c289(*int x, union<int, string> y) {
    return x != y;
}
auto sub c290(*int x, *int y) {
    return x != y;
}
auto sub c291(*int x, *float y) {
    return x != y;
}
auto sub c292(*int x, bool y) {
    return x != y;
}
auto sub c293(*int x, *bool y) {
    return x != y;
}
auto sub c294(*int x, softint y) {
    return x != y;
}
auto sub c295(*int x, *softint y) {
    return x != y;
}
auto sub c296(*int x, softfloat y) {
    return x != y;
}
auto sub c297(*int x, *softfloat y) {
    return x != y;
}
auto sub c298(*float x, int y) {
    return x != y;
}
auto sub c299(*float x, float y) {
    return x != y;
}
auto sub c300(*float x, number y) {
    return x != y;
}
auto sub c301(*float x, *number y) {
    return x != y;
}
auto sub c302(*float x, union<int, float> y) {
    return x != y;
}
auto sub c303(*float x, union<int, number> y) {
    return x != y;
}
auto sub c304(*float x, *union<int, float> y) {
    return x != y;
}
auto sub c305(*float x, union<int, bool> y) {
    return x != y;
}
auto sub c306(*float x, union<int, string> y) {
    return x != y;
}
auto sub c307(*float x, *int y) {
    return x != y;
}
auto sub c308(*float x, *float y) {
    return x != y;
}
auto sub c309(*float x, bool y) {
    return x != y;
}
auto sub c310(*float x, *bool y) {
    return x != y;
}
auto sub c311(*float x, softint y) {
    return x != y;
}
auto sub c312(*float x, *softint y) {
    return x != y;
}
auto sub c313(*float x, softfloat y) {
    return x != y;
}
auto sub c314(*float x, *softfloat y) {
    return x != y;
}
auto sub c315(bool x, int y) {
    return x != y;
}
auto sub c316(bool x, float y) {
    return x != y;
}
auto sub c317(bool x, number y) {
    return x != y;
}
auto sub c318(bool x, *number y) {
    return x != y;
}
auto sub c319(bool x, union<int, float> y) {
    return x != y;
}
auto sub c320(bool x, union<int, number> y) {
    return x != y;
}
auto sub c321(bool x, *union<int, float> y) {
    return x != y;
}
auto sub c322(bool x, union<int, bool> y) {
    return x != y;
}
auto sub c323(bool x, union<int, string> y) {
    return x != y;
}
auto sub c324(bool x, *int y) {
    return x != y;
}
auto sub c325(bool x, *float y) {
    return x != y;
}
auto sub c326(bool x, bool y) {
    return x != y;
}
auto sub c327(bool x, *bool y) {
    return x != y;
}
auto sub c328(bool x, softint y) {
    return x != y;
}
auto sub c329(bool x, *softint y) {
    return x != y;
}
auto sub c330(bool x, softfloat y) {
    return x != y;
}
auto sub c331(bool x, *softfloat y) {
    return x != y;
}
auto sub c332(*bool x, int y) {
    return x != y;
}
auto sub c333(*bool x, float y) {
    return x != y;
}
auto sub c334(*bool x, number y) {
    return x != y;
}
auto sub c335(*bool x, *number y) {
    return x != y;
}
auto sub c336(*bool x, union<int, float> y) {
    return x != y;
}
auto sub c337(*bool x, union<int, number> y) {
    return x != y;
}
auto sub c338(*bool x, *union<int, float> y) {
    return x != y;
}
auto sub c339(*bool x, union<int, bool> y) {
    return x != y;
}
auto sub c340(*bool x, union<int, string> y) {
    return x != y;
}
auto sub c341(*bool x, *int y) {
    return x != y;
}
auto sub c342(*bool x, *float y) {
    return x != y;
}
auto sub c343(*bool x, bool y) {
    return x != y;
}
auto sub c344(*bool x, *bool y) {
    return x != y;
}
auto sub c345(*bool x, softint y) {
    return x != y;
}
auto sub c346(*bool x, *softint y) {
    return x != y;
}
auto sub c347(*bool x, softfloat y) {
    return x != y;
}
auto sub c348(*bool x, *softfloat y) {
    return x != y;
}
auto sub c349(softint x, int y) {
    return x != y;
}
auto sub c350(softint x, float y) {
    return x != y;
}
auto sub c351(softint x, number y) {
    return x != y;
}
auto sub c352(softint x, *number y) {
    return x != y;
}
auto sub c353(softint x, union<int, float> y) {
    return x != y;
}
auto sub c354(softint x, union<int, number> y) {
    return x != y;
}
auto sub c355(softint x, *union<int, float> y) {
    return x != y;
}
auto sub c356(softint x, union<int, bool> y) {
    return x != y;
}
auto sub c357(softint x, union<int, string> y) {
    return x != y;
}
auto sub c358(softint x, *int y) {
    return x != y;
}
auto sub c359(softint x, *float y) {
    return x != y;
}
auto sub c360(softint x, bool y) {
    return x != y;
}
auto sub c361(softint x, *bool y) {
    return x != y;
}
auto sub c362(softint x, softint y) {
    return x != y;
}
auto sub c363(softint x, *softint y) {
    return x != y;
}
auto sub c364(softint x, softfloat y) {
    return x != y;
}
auto sub c365(softint x, *softfloat y) {
    return x != y;
}
auto sub c366(*softint x, int y) {
    return x != y;
}
auto sub c367(*softint x, float y) {
    return x != y;
}
auto sub c368(*softint x, number y) {
    return x != y;
}
auto sub c369(*softint x, *number y) {
    return x != y;
}
auto sub c370(*softint x, union<int, float> y) {
    return x != y;
}
auto sub c371(*softint x, union<int, number> y) {
    return x != y;
}
auto sub c372(*softint x, *union<int, float> y) {
    return x != y;
}
auto sub c373(*softint x, union<int, bool> y) {
    return x != y;
}
auto sub c374(*softint x, union<int, string> y) {
    return x != y;
}
auto sub c375(*softint x, *int y) {
    return x != y;
}
auto sub c376(*softint x, *float y) {
    return x != y;
}
auto sub c377(*softint x, bool y) {
    return x != y;
}
auto sub c378(*softint x, *bool y) {
    return x != y;
}
auto sub c379(*softint x, softint y) {
    return x != y;
}
auto sub c380(*softint x, *softint y) {
    return x != y;
}
auto sub c381(*softint x, softfloat y) {
    return x != y;
}
auto sub c382(*softint x, *softfloat y) {
    return x != y;
}
auto sub c383(softfloat x, int y) {
    return x != y;
}
auto sub c384(softfloat x, float y) {
    return x != y;
}
auto sub c385(softfloat x, number y) {
    return x != y;
}
auto sub c386(softfloat x, *number y) {
    return x != y;
}
auto sub c387(softfloat x, union<int, float> y) {
    return x != y;
}
auto sub c388(softfloat x, union<int, number> y) {
    return x != y;
}
auto sub c389(softfloat x, *union<int, float> y) {
    return x != y;
}
auto sub c390(softfloat x, union<int, bool> y) {
    return x != y;
}
auto sub c391(softfloat x, union<int, string> y) {
    return x != y;
}
auto sub c392(softfloat x, *int y) {
    return x != y;
}
auto sub c393(softfloat x, *float y) {
    return x != y;
}
auto sub c394(softfloat x, bool y) {
    return x != y;
}
auto sub c395(softfloat x, *bool y) {
    return x != y;
}
auto sub c396(softfloat x, softint y) {
    return x != y;
}
auto sub c397(softfloat x, *softint y) {
    return x != y;
}
auto sub c398(softfloat x, softfloat y) {
    return x != y;
}
auto sub c399(softfloat x, *softfloat y) {
    return x != y;
}
auto sub c400(*softfloat x, int y) {
    return x != y;
}
auto sub c401(*softfloat x, float y) {
    return x != y;
}
auto sub c402(*softfloat x, number y) {
    return x != y;
}
auto sub c403(*softfloat x, *number y) {
    return x != y;
}
auto sub c404(*softfloat x, union<int, float> y) {
    return x != y;
}
auto sub c405(*softfloat x, union<int, number> y) {
    return x != y;
}
auto sub c406(*softfloat x, *union<int, float> y) {
    return x != y;
}
auto sub c407(*softfloat x, union<int, bool> y) {
    return x != y;
}
auto sub c408(*softfloat x, union<int, string> y) {
    return x != y;
}
auto sub c409(*softfloat x, *int y) {
    return x != y;
}
auto sub c410(*softfloat x, *float y) {
    return x != y;
}
auto sub c411(*softfloat x, bool y) {
    return x != y;
}
auto sub c412(*softfloat x, *bool y) {
    return x != y;
}
auto sub c413(*softfloat x, softint y) {
    return x != y;
}
auto sub c414(*softfloat x, *softint y) {
    return x != y;
}
auto sub c415(*softfloat x, softfloat y) {
    return x != y;
}
auto sub c416(*softfloat x, *softfloat y) {
    return x != y;
}
auto sub c417(int x, *int y) {
    return x < y;
}
auto sub c418(int x, *float y) {
    return x < y;
}
auto sub c419(int x, bool y) {
    return x < y;
}
auto sub c420(int x, *bool y) {
    return x < y;
}
auto sub c421(int x, softint y) {
    return x < y;
}
auto sub c422(int x, *softint y) {
    return x < y;
}
auto sub c423(int x, softfloat y) {
    return x < y;
}
auto sub c424(int x, *softfloat y) {
    return x < y;
}
auto sub c425(float x, *int y) {
    return x < y;
}
auto sub c426(float x, *float y) {
    return x < y;
}
auto sub c427(float x, bool y) {
    return x < y;
}
auto sub c428(float x, *bool y) {
    return x < y;
}
auto sub c429(float x, softint y) {
    return x < y;
}
auto sub c430(float x, *softint y) {
    return x < y;
}
auto sub c431(float x, softfloat y) {
    return x < y;
}
auto sub c432(float x, *softfloat y) {
    return x < y;
}
auto sub c433(number x, *int y) {
    return x < y;
}
auto sub c434(number x, *float y) {
    return x < y;
}
auto sub c435(number x, bool y) {
    return x < y;
}
auto sub c436(number x, *bool y) {
    return x < y;
}
auto sub c437(number x, softint y) {
    return x < y;
}
auto sub c438(number x, *softint y) {
    return x < y;
}
auto sub c439(number x, softfloat y) {
    return x < y;
}
auto sub c440(number x, *softfloat y) {
    return x < y;
}
auto sub c441(*number x, *int y) {
    return x < y;
}
auto sub c442(*number x, *float y) {
    return x < y;
}
auto sub c443(*number x, bool y) {
    return x < y;
}
auto sub c444(*number x, *bool y) {
    return x < y;
}
auto sub c445(*number x, softint y) {
    return x < y;
}
auto sub c446(*number x, *softint y) {
    return x < y;
}
auto sub c447(*number x, softfloat y) {
    return x < y;
}
auto sub c448(*number x, *softfloat y) {
    return x < y;
}
auto sub c449(union<int, float> x, *int y) {
    return x < y;
}
auto sub c450(union<int, float> x, *float y) {
    return x < y;
}
auto sub c451(union<int, float> x, bool y) {
    return x < y;
}
auto sub c452(union<int, float> x, *bool y) {
    return x < y;
}
auto sub c453(union<int, float> x, softint y) {
    return x < y;
}
auto sub c454(union<int, float> x, *softint y) {
    return x < y;
}
auto sub c455(union<int, float> x, softfloat y) {
    return x < y;
}
auto sub c456(union<int, float> x, *softfloat y) {
    return x < y;
}
auto sub c457(union<int, number> x, *int y) {
    return x < y;
}
auto sub c458(union<int, number> x, *float y) {
    return x < y;
}
auto sub c459(union<int, number> x, bool y) {
    return x < y;
}
auto sub c460(union<int, number> x, *bool y) {
    return x < y;
}
auto sub c461(union<int, number> x, softint y) {
    return x < y;
}
auto sub c462(union<int, number> x, *softint y) {
    return x < y;
}
auto sub c463(union<int, number> x, softfloat y) {
    return x < y;
}
auto sub c464(union<int, number> x, *softfloat y) {
    return x < y;
}
auto sub c465(*union<int, float> x, *int y) {
    return x < y;
}
auto sub c466(*union<int, float> x, *float y) {
    return x < y;
}
auto sub c467(*union<int, float> x, bool y) {
    return x < y;
}
auto sub c468(*union<int, float> x, *bool y) {
    return x < y;
}
auto sub c469(*union<int, float> x, softint y) {
    return x < y;
}
auto sub c470(*union<int, float> x, *softint y) {
    return x < y;
}
auto sub c471(*union<int, float> x, softfloat y) {
    return x < y;
}
auto sub c472(*union<int, float> x, *softfloat y) {
    return x < y;
}
auto sub c473(union<int, bool> x, *int y) {
    return x < y;
}
auto sub c474(union<int, bool> x, *float y) {
    return x < y;
}
auto sub c475(union<int, bool> x, bool y) {
    return x < y;
}
auto sub c476(union<int, bool> x, *bool y) {
    return x < y;
}
auto sub c477(union<int, bool> x, softint y) {
    return x < y;
}
auto sub c478(union<int, bool> x, *softint y) {
    return x < y;
}
auto sub c479(union<int, bool> x, softfloat y) {
    return x < y;
}
auto sub c480(union<int, bool> x, *softfloat y) {
    return x < y;
}
auto sub c481(union<int, string> x, *int y) {
    return x < y;
}
auto sub c482(union<int, string> x, *float y) {
    return x < y;
}
auto sub c483(union<int, string> x, bool y) {
    return x < y;
}
auto sub c484(union<int, string> x, *bool y) {
    return x < y;
}
auto sub c485(union<int, string> x, softint y) {
    return x < y;
}
auto sub c486(union<int, string> x, *softint y) {
    return x < y;
}
auto sub c487(union<int, string> x, softfloat y) {
    return x < y;
}
auto sub c488(union<int, string> x, *softfloat y) {
    return x < y;
}
auto sub c489(*int x, int y) {
    return x < y;
}
auto sub c490(*int x, float y) {
    return x < y;
}
auto sub c491(*int x, number y) {
    return x < y;
}
auto sub c492(*int x, *number y) {
    return x < y;
}
auto sub c493(*int x, union<int, float> y) {
    return x < y;
}
auto sub c494(*int x, union<int, number> y) {
    return x < y;
}
auto sub c495(*int x, *union<int, float> y) {
    return x < y;
}
auto sub c496(*int x, union<int, bool> y) {
    return x < y;
}
auto sub c497(*int x, union<int, string> y) {
    return x < y;
}
auto sub c498(*int x, *int y) {
    return x < y;
}
auto sub c499(*int x, *float y) {
    return x < y;
}
auto sub c500(*int x, bool y) {
    return x < y;
}
auto sub c501(*int x, *bool y) {
    return x < y;
}
auto sub c502(*int x, softint y) {
    return x < y;
}
auto sub c503(*int x, *softint y) {
    return x < y;
}
auto sub c504(*int x, softfloat y) {
    return x < y;
}
auto sub c505(*int x, *softfloat y) {
    return x < y;
}
auto sub c506(*float x, int y) {
    return x < y;
}
auto sub c507(*float x, float y) {
    return x < y;
}
auto sub c508(*float x, number y) {
    return x < y;
}
auto sub c509(*float x, *number y) {
    return x < y;
}
auto sub c510(*float x, union<int, float> y) {
    return x < y;
}
auto sub c511(*float x, union<int, number> y) {
    return x < y;
}
auto sub c512(*float x, *union<int, float> y) {
    return x < y;
}
auto sub c513(*float x, union<int, bool> y) {
    return x < y;
}
auto sub c514(*float x, union<int, string> y) {
    return x < y;
}
auto sub c515(*float x, *int y) {
    return x < y;
}
auto sub c516(*float x, *float y) {
    return x < y;
}
auto sub c517(*float x, bool y) {
    return x < y;
}
auto sub c518(*float x, *bool y) {
    return x < y;
}
auto sub c519(*float x, softint y) {
    return x < y;
}
auto sub c520(*float x, *softint y) {
    return x < y;
}
auto sub c521(*float x, softfloat y) {
    return x < y;
}
auto sub c522(*float x, *softfloat y) {
    return x < y;
}
auto sub c523(bool x, int y) {
    return x < y;
}
auto sub c524(bool x, float y) {
    return x < y;
}
auto sub c525(bool x, number y) {
    return x < y;
}
auto sub c526(bool x, *number y) {
    return x < y;
}
auto sub c527(bool x, union<int, float> y) {
    return x < y;
}
auto sub c528(bool x, union<int, number> y) {
    return x < y;
}
auto sub c529(bool x, *union<int, float> y) {
    return x < y;
}
auto sub c530(bool x, union<int, bool> y) {
    return x < y;
}
auto sub c531(bool x, union<int, string> y) {
    return x < y;
}
auto sub c532(bool x, *int y) {
    return x < y;
}
auto sub c533(bool x, *float y) {
    return x < y;
}
auto sub c534(bool x, bool y) {
    return x < y;
}
auto sub c535(bool x, *bool y) {
    return x < y;
}
auto sub c536(bool x, softint y) {
    return x < y;
}
auto sub c537(bool x, *softint y) {
    return x < y;
}
auto sub c538(bool x, softfloat y) {
    return x < y;
}
auto sub c539(bool x, *softfloat y) {
    return x < y;
}
auto sub c540(*bool x, int y) {
    return x < y;
}
auto sub c541(*bool x, float y) {
    return x < y;
}
auto sub c542(*bool x, number y) {
    return x < y;
}
auto sub c543(*bool x, *number y) {
    return x < y;
}
auto sub c544(*bool x, union<int, float> y) {
    return x < y;
}
auto sub c545(*bool x, union<int, number> y) {
    return x < y;
}
auto sub c546(*bool x, *union<int, float> y) {
    return x < y;
}
auto sub c547(*bool x, union<int, bool> y) {
    return x < y;
}
auto sub c548(*bool x, union<int, string> y) {
    return x < y;
}
auto sub c549(*bool x, *int y) {
    return x < y;
}
auto sub c550(*bool x, *float y) {
    return x < y;
}
auto sub c551(*bool x, bool y) {
    return x < y;
}
auto sub c552(*bool x, *bool y) {
    return x < y;
}
auto sub c553(*bool x, softint y) {
    return x < y;
}
auto sub c554(*bool x, *softint y) {
    return x < y;
}
auto sub c555(*bool x, softfloat y) {
    return x < y;
}
auto sub c556(*bool x, *softfloat y) {
    return x < y;
}
auto sub c557(softint x, int y) {
    return x < y;
}
auto sub c558(softint x, float y) {
    return x < y;
}
auto sub c559(softint x, number y) {
    return x < y;
}
auto sub c560(softint x, *number y) {
    return x < y;
}
auto sub c561(softint x, union<int, float> y) {
    return x < y;
}
auto sub c562(softint x, union<int, number> y) {
    return x < y;
}
auto sub c563(softint x, *union<int, float> y) {
    return x < y;
}
auto sub c564(softint x, union<int, bool> y) {
    return x < y;
}
auto sub c565(softint x, union<int, string> y) {
    return x < y;
}
auto sub c566(softint x, *int y) {
    return x < y;
}
auto sub c567(softint x, *float y) {
    return x < y;
}
auto sub c568(softint x, bool y) {
    return x < y;
}
auto sub c569(softint x, *bool y) {
    return x < y;
}
auto sub c570(softint x, softint y) {
    return x < y;
}
auto sub c571(softint x, *softint y) {
    return x < y;
}
auto sub c572(softint x, softfloat y) {
    return x < y;
}
auto sub c573(softint x, *softfloat y) {
    return x < y;
}
auto sub c574(*softint x, int y) {
    return x < y;
}
auto sub c575(*softint x, float y) {
    return x < y;
}
auto sub c576(*softint x, number y) {
    return x < y;
}
auto sub c577(*softint x, *number y) {
    return x < y;
}
auto sub c578(*softint x, union<int, float> y) {
    return x < y;
}
auto sub c579(*softint x, union<int, number> y) {
    return x < y;
}
auto sub c580(*softint x, *union<int, float> y) {
    return x < y;
}
auto sub c581(*softint x, union<int, bool> y) {
    return x < y;
}
auto sub c582(*softint x, union<int, string> y) {
    return x < y;
}
auto sub c583(*softint x, *int y) {
    return x < y;
}
auto sub c584(*softint x, *float y) {
    return x < y;
}
auto sub c585(*softint x, bool y) {
    return x < y;
}
auto sub c586(*softint x, *bool y) {
    return x < y;
}
auto sub c587(*softint x, softint y) {
    return x < y;
}
auto sub c588(*softint x, *softint y) {
    return x < y;
}
auto sub c589(*softint x, softfloat y) {
    return x < y;
}
auto sub c590(*softint x, *softfloat y) {
    return x < y;
}
auto sub c591(softfloat x, int y) {
    return x < y;
}
auto sub c592(softfloat x, float y) {
    return x < y;
}
auto sub c593(softfloat x, number y) {
    return x < y;
}
auto sub c594(softfloat x, *number y) {
    return x < y;
}
auto sub c595(softfloat x, union<int, float> y) {
    return x < y;
}
auto sub c596(softfloat x, union<int, number> y) {
    return x < y;
}
auto sub c597(softfloat x, *union<int, float> y) {
    return x < y;
}
auto sub c598(softfloat x, union<int, bool> y) {
    return x < y;
}
auto sub c599(softfloat x, union<int, string> y) {
    return x < y;
}
auto sub c600(softfloat x, *int y) {
    return x < y;
}
auto sub c601(softfloat x, *float y) {
    return x < y;
}
auto sub c602(softfloat x, bool y) {
    return x < y;
}
auto sub c603(softfloat x, *bool y) {
    return x < y;
}
auto sub c604(softfloat x, softint y) {
    return x < y;
}
auto sub c605(softfloat x, *softint y) {
    return x < y;
}
auto sub c606(softfloat x, softfloat y) {
    return x < y;
}
auto sub c607(softfloat x, *softfloat y) {
    return x < y;
}
auto sub c608(*softfloat x, int y) {
    return x < y;
}
auto sub c609(*softfloat x, float y) {
    return x < y;
}
auto sub c610(*softfloat x, number y) {
    return x < y;
}
auto sub c611(*softfloat x, *number y) {
    return x < y;
}
auto sub c612(*softfloat x, union<int, float> y) {
    return x < y;
}
auto sub c613(*softfloat x, union<int, number> y) {
    return x < y;
}
auto sub c614(*softfloat x, *union<int, float> y) {
    return x < y;
}
auto sub c615(*softfloat x, union<int, bool> y) {
    return x < y;
}
auto sub c616(*softfloat x, union<int, string> y) {
    return x < y;
}
auto sub c617(*softfloat x, *int y) {
    return x < y;
}
auto sub c618(*softfloat x, *float y) {
    return x < y;
}
auto sub c619(*softfloat x, bool y) {
    return x < y;
}
auto sub c620(*softfloat x, *bool y) {
    return x < y;
}
auto sub c621(*softfloat x, softint y) {
    return x < y;
}
auto sub c622(*softfloat x, *softint y) {
    return x < y;
}
auto sub c623(*softfloat x, softfloat y) {
    return x < y;
}
auto sub c624(*softfloat x, *softfloat y) {
    return x < y;
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
    # values of *int
    list<auto> v9 = (2, NOTHING);
    # values of *float
    list<auto> v10 = (2.5, 2.0, NOTHING);
    # values of bool
    list<auto> v11 = (True, False);
    # values of *bool
    list<auto> v12 = (True, NOTHING);
    # values of softint
    list<auto> v13 = (2,);
    # values of *softint
    list<auto> v14 = (2, NOTHING);
    # values of softfloat
    list<auto> v15 = (2.5,);
    # values of *softfloat
    list<auto> v16 = (2.5, NOTHING);
    foreach auto a in (v0) {
        foreach auto b in (v9) {
            show(sprintf("int == *int (%y, %y)", a, b),
                auto sub () { return c1(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v10) {
            show(sprintf("int == *float (%y, %y)", a, b),
                auto sub () { return c2(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v11) {
            show(sprintf("int == bool (%y, %y)", a, b),
                auto sub () { return c3(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v12) {
            show(sprintf("int == *bool (%y, %y)", a, b),
                auto sub () { return c4(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v13) {
            show(sprintf("int == softint (%y, %y)", a, b),
                auto sub () { return c5(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v14) {
            show(sprintf("int == *softint (%y, %y)", a, b),
                auto sub () { return c6(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v15) {
            show(sprintf("int == softfloat (%y, %y)", a, b),
                auto sub () { return c7(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v16) {
            show(sprintf("int == *softfloat (%y, %y)", a, b),
                auto sub () { return c8(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v9) {
            show(sprintf("float == *int (%y, %y)", a, b),
                auto sub () { return c9(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v10) {
            show(sprintf("float == *float (%y, %y)", a, b),
                auto sub () { return c10(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v11) {
            show(sprintf("float == bool (%y, %y)", a, b),
                auto sub () { return c11(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v12) {
            show(sprintf("float == *bool (%y, %y)", a, b),
                auto sub () { return c12(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v13) {
            show(sprintf("float == softint (%y, %y)", a, b),
                auto sub () { return c13(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v14) {
            show(sprintf("float == *softint (%y, %y)", a, b),
                auto sub () { return c14(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v15) {
            show(sprintf("float == softfloat (%y, %y)", a, b),
                auto sub () { return c15(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v16) {
            show(sprintf("float == *softfloat (%y, %y)", a, b),
                auto sub () { return c16(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v9) {
            show(sprintf("number == *int (%y, %y)", a, b),
                auto sub () { return c17(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v10) {
            show(sprintf("number == *float (%y, %y)", a, b),
                auto sub () { return c18(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v11) {
            show(sprintf("number == bool (%y, %y)", a, b),
                auto sub () { return c19(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v12) {
            show(sprintf("number == *bool (%y, %y)", a, b),
                auto sub () { return c20(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v13) {
            show(sprintf("number == softint (%y, %y)", a, b),
                auto sub () { return c21(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v14) {
            show(sprintf("number == *softint (%y, %y)", a, b),
                auto sub () { return c22(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v15) {
            show(sprintf("number == softfloat (%y, %y)", a, b),
                auto sub () { return c23(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v16) {
            show(sprintf("number == *softfloat (%y, %y)", a, b),
                auto sub () { return c24(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v9) {
            show(sprintf("*number == *int (%y, %y)", a, b),
                auto sub () { return c25(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v10) {
            show(sprintf("*number == *float (%y, %y)", a, b),
                auto sub () { return c26(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v11) {
            show(sprintf("*number == bool (%y, %y)", a, b),
                auto sub () { return c27(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v12) {
            show(sprintf("*number == *bool (%y, %y)", a, b),
                auto sub () { return c28(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v13) {
            show(sprintf("*number == softint (%y, %y)", a, b),
                auto sub () { return c29(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v14) {
            show(sprintf("*number == *softint (%y, %y)", a, b),
                auto sub () { return c30(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v15) {
            show(sprintf("*number == softfloat (%y, %y)", a, b),
                auto sub () { return c31(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v16) {
            show(sprintf("*number == *softfloat (%y, %y)", a, b),
                auto sub () { return c32(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v9) {
            show(sprintf("union<int, float> == *int (%y, %y)", a, b),
                auto sub () { return c33(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v10) {
            show(sprintf("union<int, float> == *float (%y, %y)", a, b),
                auto sub () { return c34(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v11) {
            show(sprintf("union<int, float> == bool (%y, %y)", a, b),
                auto sub () { return c35(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v12) {
            show(sprintf("union<int, float> == *bool (%y, %y)", a, b),
                auto sub () { return c36(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v13) {
            show(sprintf("union<int, float> == softint (%y, %y)", a, b),
                auto sub () { return c37(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v14) {
            show(sprintf("union<int, float> == *softint (%y, %y)", a, b),
                auto sub () { return c38(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v15) {
            show(sprintf("union<int, float> == softfloat (%y, %y)", a, b),
                auto sub () { return c39(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v16) {
            show(sprintf("union<int, float> == *softfloat (%y, %y)", a, b),
                auto sub () { return c40(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v9) {
            show(sprintf("union<int, number> == *int (%y, %y)", a, b),
                auto sub () { return c41(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v10) {
            show(sprintf("union<int, number> == *float (%y, %y)", a, b),
                auto sub () { return c42(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v11) {
            show(sprintf("union<int, number> == bool (%y, %y)", a, b),
                auto sub () { return c43(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v12) {
            show(sprintf("union<int, number> == *bool (%y, %y)", a, b),
                auto sub () { return c44(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v13) {
            show(sprintf("union<int, number> == softint (%y, %y)", a, b),
                auto sub () { return c45(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v14) {
            show(sprintf("union<int, number> == *softint (%y, %y)", a, b),
                auto sub () { return c46(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v15) {
            show(sprintf("union<int, number> == softfloat (%y, %y)", a, b),
                auto sub () { return c47(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v16) {
            show(sprintf("union<int, number> == *softfloat (%y, %y)", a, b),
                auto sub () { return c48(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v9) {
            show(sprintf("*union<int, float> == *int (%y, %y)", a, b),
                auto sub () { return c49(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v10) {
            show(sprintf("*union<int, float> == *float (%y, %y)", a, b),
                auto sub () { return c50(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v11) {
            show(sprintf("*union<int, float> == bool (%y, %y)", a, b),
                auto sub () { return c51(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v12) {
            show(sprintf("*union<int, float> == *bool (%y, %y)", a, b),
                auto sub () { return c52(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v13) {
            show(sprintf("*union<int, float> == softint (%y, %y)", a, b),
                auto sub () { return c53(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v14) {
            show(sprintf("*union<int, float> == *softint (%y, %y)", a, b),
                auto sub () { return c54(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v15) {
            show(sprintf("*union<int, float> == softfloat (%y, %y)", a, b),
                auto sub () { return c55(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v16) {
            show(sprintf("*union<int, float> == *softfloat (%y, %y)", a, b),
                auto sub () { return c56(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v9) {
            show(sprintf("union<int, bool> == *int (%y, %y)", a, b),
                auto sub () { return c57(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v10) {
            show(sprintf("union<int, bool> == *float (%y, %y)", a, b),
                auto sub () { return c58(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v11) {
            show(sprintf("union<int, bool> == bool (%y, %y)", a, b),
                auto sub () { return c59(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v12) {
            show(sprintf("union<int, bool> == *bool (%y, %y)", a, b),
                auto sub () { return c60(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v13) {
            show(sprintf("union<int, bool> == softint (%y, %y)", a, b),
                auto sub () { return c61(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v14) {
            show(sprintf("union<int, bool> == *softint (%y, %y)", a, b),
                auto sub () { return c62(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v15) {
            show(sprintf("union<int, bool> == softfloat (%y, %y)", a, b),
                auto sub () { return c63(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v16) {
            show(sprintf("union<int, bool> == *softfloat (%y, %y)", a, b),
                auto sub () { return c64(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v9) {
            show(sprintf("union<int, string> == *int (%y, %y)", a, b),
                auto sub () { return c65(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v10) {
            show(sprintf("union<int, string> == *float (%y, %y)", a, b),
                auto sub () { return c66(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v11) {
            show(sprintf("union<int, string> == bool (%y, %y)", a, b),
                auto sub () { return c67(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v12) {
            show(sprintf("union<int, string> == *bool (%y, %y)", a, b),
                auto sub () { return c68(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v13) {
            show(sprintf("union<int, string> == softint (%y, %y)", a, b),
                auto sub () { return c69(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v14) {
            show(sprintf("union<int, string> == *softint (%y, %y)", a, b),
                auto sub () { return c70(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v15) {
            show(sprintf("union<int, string> == softfloat (%y, %y)", a, b),
                auto sub () { return c71(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v16) {
            show(sprintf("union<int, string> == *softfloat (%y, %y)", a, b),
                auto sub () { return c72(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v0) {
            show(sprintf("*int == int (%y, %y)", a, b),
                auto sub () { return c73(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v1) {
            show(sprintf("*int == float (%y, %y)", a, b),
                auto sub () { return c74(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v2) {
            show(sprintf("*int == number (%y, %y)", a, b),
                auto sub () { return c75(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v3) {
            show(sprintf("*int == *number (%y, %y)", a, b),
                auto sub () { return c76(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v4) {
            show(sprintf("*int == union<int, float> (%y, %y)", a, b),
                auto sub () { return c77(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v5) {
            show(sprintf("*int == union<int, number> (%y, %y)", a, b),
                auto sub () { return c78(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v6) {
            show(sprintf("*int == *union<int, float> (%y, %y)", a, b),
                auto sub () { return c79(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v7) {
            show(sprintf("*int == union<int, bool> (%y, %y)", a, b),
                auto sub () { return c80(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v8) {
            show(sprintf("*int == union<int, string> (%y, %y)", a, b),
                auto sub () { return c81(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v9) {
            show(sprintf("*int == *int (%y, %y)", a, b),
                auto sub () { return c82(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v10) {
            show(sprintf("*int == *float (%y, %y)", a, b),
                auto sub () { return c83(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v11) {
            show(sprintf("*int == bool (%y, %y)", a, b),
                auto sub () { return c84(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v12) {
            show(sprintf("*int == *bool (%y, %y)", a, b),
                auto sub () { return c85(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v13) {
            show(sprintf("*int == softint (%y, %y)", a, b),
                auto sub () { return c86(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v14) {
            show(sprintf("*int == *softint (%y, %y)", a, b),
                auto sub () { return c87(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v15) {
            show(sprintf("*int == softfloat (%y, %y)", a, b),
                auto sub () { return c88(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v16) {
            show(sprintf("*int == *softfloat (%y, %y)", a, b),
                auto sub () { return c89(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v0) {
            show(sprintf("*float == int (%y, %y)", a, b),
                auto sub () { return c90(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v1) {
            show(sprintf("*float == float (%y, %y)", a, b),
                auto sub () { return c91(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v2) {
            show(sprintf("*float == number (%y, %y)", a, b),
                auto sub () { return c92(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v3) {
            show(sprintf("*float == *number (%y, %y)", a, b),
                auto sub () { return c93(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v4) {
            show(sprintf("*float == union<int, float> (%y, %y)", a, b),
                auto sub () { return c94(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v5) {
            show(sprintf("*float == union<int, number> (%y, %y)", a, b),
                auto sub () { return c95(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v6) {
            show(sprintf("*float == *union<int, float> (%y, %y)", a, b),
                auto sub () { return c96(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v7) {
            show(sprintf("*float == union<int, bool> (%y, %y)", a, b),
                auto sub () { return c97(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v8) {
            show(sprintf("*float == union<int, string> (%y, %y)", a, b),
                auto sub () { return c98(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v9) {
            show(sprintf("*float == *int (%y, %y)", a, b),
                auto sub () { return c99(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v10) {
            show(sprintf("*float == *float (%y, %y)", a, b),
                auto sub () { return c100(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v11) {
            show(sprintf("*float == bool (%y, %y)", a, b),
                auto sub () { return c101(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v12) {
            show(sprintf("*float == *bool (%y, %y)", a, b),
                auto sub () { return c102(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v13) {
            show(sprintf("*float == softint (%y, %y)", a, b),
                auto sub () { return c103(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v14) {
            show(sprintf("*float == *softint (%y, %y)", a, b),
                auto sub () { return c104(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v15) {
            show(sprintf("*float == softfloat (%y, %y)", a, b),
                auto sub () { return c105(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v16) {
            show(sprintf("*float == *softfloat (%y, %y)", a, b),
                auto sub () { return c106(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v0) {
            show(sprintf("bool == int (%y, %y)", a, b),
                auto sub () { return c107(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v1) {
            show(sprintf("bool == float (%y, %y)", a, b),
                auto sub () { return c108(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v2) {
            show(sprintf("bool == number (%y, %y)", a, b),
                auto sub () { return c109(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v3) {
            show(sprintf("bool == *number (%y, %y)", a, b),
                auto sub () { return c110(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v4) {
            show(sprintf("bool == union<int, float> (%y, %y)", a, b),
                auto sub () { return c111(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v5) {
            show(sprintf("bool == union<int, number> (%y, %y)", a, b),
                auto sub () { return c112(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v6) {
            show(sprintf("bool == *union<int, float> (%y, %y)", a, b),
                auto sub () { return c113(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v7) {
            show(sprintf("bool == union<int, bool> (%y, %y)", a, b),
                auto sub () { return c114(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v8) {
            show(sprintf("bool == union<int, string> (%y, %y)", a, b),
                auto sub () { return c115(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v9) {
            show(sprintf("bool == *int (%y, %y)", a, b),
                auto sub () { return c116(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v10) {
            show(sprintf("bool == *float (%y, %y)", a, b),
                auto sub () { return c117(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v11) {
            show(sprintf("bool == bool (%y, %y)", a, b),
                auto sub () { return c118(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v12) {
            show(sprintf("bool == *bool (%y, %y)", a, b),
                auto sub () { return c119(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v13) {
            show(sprintf("bool == softint (%y, %y)", a, b),
                auto sub () { return c120(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v14) {
            show(sprintf("bool == *softint (%y, %y)", a, b),
                auto sub () { return c121(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v15) {
            show(sprintf("bool == softfloat (%y, %y)", a, b),
                auto sub () { return c122(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v16) {
            show(sprintf("bool == *softfloat (%y, %y)", a, b),
                auto sub () { return c123(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v0) {
            show(sprintf("*bool == int (%y, %y)", a, b),
                auto sub () { return c124(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v1) {
            show(sprintf("*bool == float (%y, %y)", a, b),
                auto sub () { return c125(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v2) {
            show(sprintf("*bool == number (%y, %y)", a, b),
                auto sub () { return c126(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v3) {
            show(sprintf("*bool == *number (%y, %y)", a, b),
                auto sub () { return c127(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v4) {
            show(sprintf("*bool == union<int, float> (%y, %y)", a, b),
                auto sub () { return c128(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v5) {
            show(sprintf("*bool == union<int, number> (%y, %y)", a, b),
                auto sub () { return c129(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v6) {
            show(sprintf("*bool == *union<int, float> (%y, %y)", a, b),
                auto sub () { return c130(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v7) {
            show(sprintf("*bool == union<int, bool> (%y, %y)", a, b),
                auto sub () { return c131(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v8) {
            show(sprintf("*bool == union<int, string> (%y, %y)", a, b),
                auto sub () { return c132(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v9) {
            show(sprintf("*bool == *int (%y, %y)", a, b),
                auto sub () { return c133(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v10) {
            show(sprintf("*bool == *float (%y, %y)", a, b),
                auto sub () { return c134(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v11) {
            show(sprintf("*bool == bool (%y, %y)", a, b),
                auto sub () { return c135(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v12) {
            show(sprintf("*bool == *bool (%y, %y)", a, b),
                auto sub () { return c136(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v13) {
            show(sprintf("*bool == softint (%y, %y)", a, b),
                auto sub () { return c137(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v14) {
            show(sprintf("*bool == *softint (%y, %y)", a, b),
                auto sub () { return c138(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v15) {
            show(sprintf("*bool == softfloat (%y, %y)", a, b),
                auto sub () { return c139(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v16) {
            show(sprintf("*bool == *softfloat (%y, %y)", a, b),
                auto sub () { return c140(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v0) {
            show(sprintf("softint == int (%y, %y)", a, b),
                auto sub () { return c141(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v1) {
            show(sprintf("softint == float (%y, %y)", a, b),
                auto sub () { return c142(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v2) {
            show(sprintf("softint == number (%y, %y)", a, b),
                auto sub () { return c143(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v3) {
            show(sprintf("softint == *number (%y, %y)", a, b),
                auto sub () { return c144(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v4) {
            show(sprintf("softint == union<int, float> (%y, %y)", a, b),
                auto sub () { return c145(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v5) {
            show(sprintf("softint == union<int, number> (%y, %y)", a, b),
                auto sub () { return c146(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v6) {
            show(sprintf("softint == *union<int, float> (%y, %y)", a, b),
                auto sub () { return c147(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v7) {
            show(sprintf("softint == union<int, bool> (%y, %y)", a, b),
                auto sub () { return c148(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v8) {
            show(sprintf("softint == union<int, string> (%y, %y)", a, b),
                auto sub () { return c149(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v9) {
            show(sprintf("softint == *int (%y, %y)", a, b),
                auto sub () { return c150(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v10) {
            show(sprintf("softint == *float (%y, %y)", a, b),
                auto sub () { return c151(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v11) {
            show(sprintf("softint == bool (%y, %y)", a, b),
                auto sub () { return c152(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v12) {
            show(sprintf("softint == *bool (%y, %y)", a, b),
                auto sub () { return c153(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v13) {
            show(sprintf("softint == softint (%y, %y)", a, b),
                auto sub () { return c154(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v14) {
            show(sprintf("softint == *softint (%y, %y)", a, b),
                auto sub () { return c155(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v15) {
            show(sprintf("softint == softfloat (%y, %y)", a, b),
                auto sub () { return c156(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v16) {
            show(sprintf("softint == *softfloat (%y, %y)", a, b),
                auto sub () { return c157(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v0) {
            show(sprintf("*softint == int (%y, %y)", a, b),
                auto sub () { return c158(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v1) {
            show(sprintf("*softint == float (%y, %y)", a, b),
                auto sub () { return c159(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v2) {
            show(sprintf("*softint == number (%y, %y)", a, b),
                auto sub () { return c160(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v3) {
            show(sprintf("*softint == *number (%y, %y)", a, b),
                auto sub () { return c161(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v4) {
            show(sprintf("*softint == union<int, float> (%y, %y)", a, b),
                auto sub () { return c162(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v5) {
            show(sprintf("*softint == union<int, number> (%y, %y)", a, b),
                auto sub () { return c163(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v6) {
            show(sprintf("*softint == *union<int, float> (%y, %y)", a, b),
                auto sub () { return c164(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v7) {
            show(sprintf("*softint == union<int, bool> (%y, %y)", a, b),
                auto sub () { return c165(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v8) {
            show(sprintf("*softint == union<int, string> (%y, %y)", a, b),
                auto sub () { return c166(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v9) {
            show(sprintf("*softint == *int (%y, %y)", a, b),
                auto sub () { return c167(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v10) {
            show(sprintf("*softint == *float (%y, %y)", a, b),
                auto sub () { return c168(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v11) {
            show(sprintf("*softint == bool (%y, %y)", a, b),
                auto sub () { return c169(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v12) {
            show(sprintf("*softint == *bool (%y, %y)", a, b),
                auto sub () { return c170(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v13) {
            show(sprintf("*softint == softint (%y, %y)", a, b),
                auto sub () { return c171(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v14) {
            show(sprintf("*softint == *softint (%y, %y)", a, b),
                auto sub () { return c172(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v15) {
            show(sprintf("*softint == softfloat (%y, %y)", a, b),
                auto sub () { return c173(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v16) {
            show(sprintf("*softint == *softfloat (%y, %y)", a, b),
                auto sub () { return c174(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v0) {
            show(sprintf("softfloat == int (%y, %y)", a, b),
                auto sub () { return c175(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v1) {
            show(sprintf("softfloat == float (%y, %y)", a, b),
                auto sub () { return c176(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v2) {
            show(sprintf("softfloat == number (%y, %y)", a, b),
                auto sub () { return c177(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v3) {
            show(sprintf("softfloat == *number (%y, %y)", a, b),
                auto sub () { return c178(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v4) {
            show(sprintf("softfloat == union<int, float> (%y, %y)", a, b),
                auto sub () { return c179(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v5) {
            show(sprintf("softfloat == union<int, number> (%y, %y)", a, b),
                auto sub () { return c180(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v6) {
            show(sprintf("softfloat == *union<int, float> (%y, %y)", a, b),
                auto sub () { return c181(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v7) {
            show(sprintf("softfloat == union<int, bool> (%y, %y)", a, b),
                auto sub () { return c182(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v8) {
            show(sprintf("softfloat == union<int, string> (%y, %y)", a, b),
                auto sub () { return c183(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v9) {
            show(sprintf("softfloat == *int (%y, %y)", a, b),
                auto sub () { return c184(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v10) {
            show(sprintf("softfloat == *float (%y, %y)", a, b),
                auto sub () { return c185(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v11) {
            show(sprintf("softfloat == bool (%y, %y)", a, b),
                auto sub () { return c186(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v12) {
            show(sprintf("softfloat == *bool (%y, %y)", a, b),
                auto sub () { return c187(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v13) {
            show(sprintf("softfloat == softint (%y, %y)", a, b),
                auto sub () { return c188(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v14) {
            show(sprintf("softfloat == *softint (%y, %y)", a, b),
                auto sub () { return c189(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v15) {
            show(sprintf("softfloat == softfloat (%y, %y)", a, b),
                auto sub () { return c190(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v16) {
            show(sprintf("softfloat == *softfloat (%y, %y)", a, b),
                auto sub () { return c191(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v0) {
            show(sprintf("*softfloat == int (%y, %y)", a, b),
                auto sub () { return c192(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v1) {
            show(sprintf("*softfloat == float (%y, %y)", a, b),
                auto sub () { return c193(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v2) {
            show(sprintf("*softfloat == number (%y, %y)", a, b),
                auto sub () { return c194(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v3) {
            show(sprintf("*softfloat == *number (%y, %y)", a, b),
                auto sub () { return c195(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v4) {
            show(sprintf("*softfloat == union<int, float> (%y, %y)", a, b),
                auto sub () { return c196(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v5) {
            show(sprintf("*softfloat == union<int, number> (%y, %y)", a, b),
                auto sub () { return c197(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v6) {
            show(sprintf("*softfloat == *union<int, float> (%y, %y)", a, b),
                auto sub () { return c198(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v7) {
            show(sprintf("*softfloat == union<int, bool> (%y, %y)", a, b),
                auto sub () { return c199(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v8) {
            show(sprintf("*softfloat == union<int, string> (%y, %y)", a, b),
                auto sub () { return c200(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v9) {
            show(sprintf("*softfloat == *int (%y, %y)", a, b),
                auto sub () { return c201(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v10) {
            show(sprintf("*softfloat == *float (%y, %y)", a, b),
                auto sub () { return c202(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v11) {
            show(sprintf("*softfloat == bool (%y, %y)", a, b),
                auto sub () { return c203(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v12) {
            show(sprintf("*softfloat == *bool (%y, %y)", a, b),
                auto sub () { return c204(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v13) {
            show(sprintf("*softfloat == softint (%y, %y)", a, b),
                auto sub () { return c205(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v14) {
            show(sprintf("*softfloat == *softint (%y, %y)", a, b),
                auto sub () { return c206(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v15) {
            show(sprintf("*softfloat == softfloat (%y, %y)", a, b),
                auto sub () { return c207(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v16) {
            show(sprintf("*softfloat == *softfloat (%y, %y)", a, b),
                auto sub () { return c208(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v9) {
            show(sprintf("int != *int (%y, %y)", a, b),
                auto sub () { return c209(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v10) {
            show(sprintf("int != *float (%y, %y)", a, b),
                auto sub () { return c210(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v11) {
            show(sprintf("int != bool (%y, %y)", a, b),
                auto sub () { return c211(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v12) {
            show(sprintf("int != *bool (%y, %y)", a, b),
                auto sub () { return c212(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v13) {
            show(sprintf("int != softint (%y, %y)", a, b),
                auto sub () { return c213(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v14) {
            show(sprintf("int != *softint (%y, %y)", a, b),
                auto sub () { return c214(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v15) {
            show(sprintf("int != softfloat (%y, %y)", a, b),
                auto sub () { return c215(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v16) {
            show(sprintf("int != *softfloat (%y, %y)", a, b),
                auto sub () { return c216(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v9) {
            show(sprintf("float != *int (%y, %y)", a, b),
                auto sub () { return c217(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v10) {
            show(sprintf("float != *float (%y, %y)", a, b),
                auto sub () { return c218(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v11) {
            show(sprintf("float != bool (%y, %y)", a, b),
                auto sub () { return c219(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v12) {
            show(sprintf("float != *bool (%y, %y)", a, b),
                auto sub () { return c220(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v13) {
            show(sprintf("float != softint (%y, %y)", a, b),
                auto sub () { return c221(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v14) {
            show(sprintf("float != *softint (%y, %y)", a, b),
                auto sub () { return c222(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v15) {
            show(sprintf("float != softfloat (%y, %y)", a, b),
                auto sub () { return c223(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v16) {
            show(sprintf("float != *softfloat (%y, %y)", a, b),
                auto sub () { return c224(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v9) {
            show(sprintf("number != *int (%y, %y)", a, b),
                auto sub () { return c225(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v10) {
            show(sprintf("number != *float (%y, %y)", a, b),
                auto sub () { return c226(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v11) {
            show(sprintf("number != bool (%y, %y)", a, b),
                auto sub () { return c227(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v12) {
            show(sprintf("number != *bool (%y, %y)", a, b),
                auto sub () { return c228(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v13) {
            show(sprintf("number != softint (%y, %y)", a, b),
                auto sub () { return c229(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v14) {
            show(sprintf("number != *softint (%y, %y)", a, b),
                auto sub () { return c230(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v15) {
            show(sprintf("number != softfloat (%y, %y)", a, b),
                auto sub () { return c231(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v16) {
            show(sprintf("number != *softfloat (%y, %y)", a, b),
                auto sub () { return c232(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v9) {
            show(sprintf("*number != *int (%y, %y)", a, b),
                auto sub () { return c233(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v10) {
            show(sprintf("*number != *float (%y, %y)", a, b),
                auto sub () { return c234(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v11) {
            show(sprintf("*number != bool (%y, %y)", a, b),
                auto sub () { return c235(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v12) {
            show(sprintf("*number != *bool (%y, %y)", a, b),
                auto sub () { return c236(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v13) {
            show(sprintf("*number != softint (%y, %y)", a, b),
                auto sub () { return c237(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v14) {
            show(sprintf("*number != *softint (%y, %y)", a, b),
                auto sub () { return c238(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v15) {
            show(sprintf("*number != softfloat (%y, %y)", a, b),
                auto sub () { return c239(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v16) {
            show(sprintf("*number != *softfloat (%y, %y)", a, b),
                auto sub () { return c240(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v9) {
            show(sprintf("union<int, float> != *int (%y, %y)", a, b),
                auto sub () { return c241(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v10) {
            show(sprintf("union<int, float> != *float (%y, %y)", a, b),
                auto sub () { return c242(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v11) {
            show(sprintf("union<int, float> != bool (%y, %y)", a, b),
                auto sub () { return c243(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v12) {
            show(sprintf("union<int, float> != *bool (%y, %y)", a, b),
                auto sub () { return c244(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v13) {
            show(sprintf("union<int, float> != softint (%y, %y)", a, b),
                auto sub () { return c245(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v14) {
            show(sprintf("union<int, float> != *softint (%y, %y)", a, b),
                auto sub () { return c246(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v15) {
            show(sprintf("union<int, float> != softfloat (%y, %y)", a, b),
                auto sub () { return c247(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v16) {
            show(sprintf("union<int, float> != *softfloat (%y, %y)", a, b),
                auto sub () { return c248(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v9) {
            show(sprintf("union<int, number> != *int (%y, %y)", a, b),
                auto sub () { return c249(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v10) {
            show(sprintf("union<int, number> != *float (%y, %y)", a, b),
                auto sub () { return c250(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v11) {
            show(sprintf("union<int, number> != bool (%y, %y)", a, b),
                auto sub () { return c251(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v12) {
            show(sprintf("union<int, number> != *bool (%y, %y)", a, b),
                auto sub () { return c252(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v13) {
            show(sprintf("union<int, number> != softint (%y, %y)", a, b),
                auto sub () { return c253(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v14) {
            show(sprintf("union<int, number> != *softint (%y, %y)", a, b),
                auto sub () { return c254(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v15) {
            show(sprintf("union<int, number> != softfloat (%y, %y)", a, b),
                auto sub () { return c255(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v16) {
            show(sprintf("union<int, number> != *softfloat (%y, %y)", a, b),
                auto sub () { return c256(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v9) {
            show(sprintf("*union<int, float> != *int (%y, %y)", a, b),
                auto sub () { return c257(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v10) {
            show(sprintf("*union<int, float> != *float (%y, %y)", a, b),
                auto sub () { return c258(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v11) {
            show(sprintf("*union<int, float> != bool (%y, %y)", a, b),
                auto sub () { return c259(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v12) {
            show(sprintf("*union<int, float> != *bool (%y, %y)", a, b),
                auto sub () { return c260(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v13) {
            show(sprintf("*union<int, float> != softint (%y, %y)", a, b),
                auto sub () { return c261(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v14) {
            show(sprintf("*union<int, float> != *softint (%y, %y)", a, b),
                auto sub () { return c262(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v15) {
            show(sprintf("*union<int, float> != softfloat (%y, %y)", a, b),
                auto sub () { return c263(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v16) {
            show(sprintf("*union<int, float> != *softfloat (%y, %y)", a, b),
                auto sub () { return c264(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v9) {
            show(sprintf("union<int, bool> != *int (%y, %y)", a, b),
                auto sub () { return c265(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v10) {
            show(sprintf("union<int, bool> != *float (%y, %y)", a, b),
                auto sub () { return c266(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v11) {
            show(sprintf("union<int, bool> != bool (%y, %y)", a, b),
                auto sub () { return c267(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v12) {
            show(sprintf("union<int, bool> != *bool (%y, %y)", a, b),
                auto sub () { return c268(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v13) {
            show(sprintf("union<int, bool> != softint (%y, %y)", a, b),
                auto sub () { return c269(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v14) {
            show(sprintf("union<int, bool> != *softint (%y, %y)", a, b),
                auto sub () { return c270(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v15) {
            show(sprintf("union<int, bool> != softfloat (%y, %y)", a, b),
                auto sub () { return c271(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v16) {
            show(sprintf("union<int, bool> != *softfloat (%y, %y)", a, b),
                auto sub () { return c272(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v9) {
            show(sprintf("union<int, string> != *int (%y, %y)", a, b),
                auto sub () { return c273(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v10) {
            show(sprintf("union<int, string> != *float (%y, %y)", a, b),
                auto sub () { return c274(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v11) {
            show(sprintf("union<int, string> != bool (%y, %y)", a, b),
                auto sub () { return c275(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v12) {
            show(sprintf("union<int, string> != *bool (%y, %y)", a, b),
                auto sub () { return c276(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v13) {
            show(sprintf("union<int, string> != softint (%y, %y)", a, b),
                auto sub () { return c277(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v14) {
            show(sprintf("union<int, string> != *softint (%y, %y)", a, b),
                auto sub () { return c278(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v15) {
            show(sprintf("union<int, string> != softfloat (%y, %y)", a, b),
                auto sub () { return c279(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v16) {
            show(sprintf("union<int, string> != *softfloat (%y, %y)", a, b),
                auto sub () { return c280(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v0) {
            show(sprintf("*int != int (%y, %y)", a, b),
                auto sub () { return c281(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v1) {
            show(sprintf("*int != float (%y, %y)", a, b),
                auto sub () { return c282(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v2) {
            show(sprintf("*int != number (%y, %y)", a, b),
                auto sub () { return c283(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v3) {
            show(sprintf("*int != *number (%y, %y)", a, b),
                auto sub () { return c284(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v4) {
            show(sprintf("*int != union<int, float> (%y, %y)", a, b),
                auto sub () { return c285(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v5) {
            show(sprintf("*int != union<int, number> (%y, %y)", a, b),
                auto sub () { return c286(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v6) {
            show(sprintf("*int != *union<int, float> (%y, %y)", a, b),
                auto sub () { return c287(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v7) {
            show(sprintf("*int != union<int, bool> (%y, %y)", a, b),
                auto sub () { return c288(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v8) {
            show(sprintf("*int != union<int, string> (%y, %y)", a, b),
                auto sub () { return c289(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v9) {
            show(sprintf("*int != *int (%y, %y)", a, b),
                auto sub () { return c290(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v10) {
            show(sprintf("*int != *float (%y, %y)", a, b),
                auto sub () { return c291(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v11) {
            show(sprintf("*int != bool (%y, %y)", a, b),
                auto sub () { return c292(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v12) {
            show(sprintf("*int != *bool (%y, %y)", a, b),
                auto sub () { return c293(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v13) {
            show(sprintf("*int != softint (%y, %y)", a, b),
                auto sub () { return c294(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v14) {
            show(sprintf("*int != *softint (%y, %y)", a, b),
                auto sub () { return c295(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v15) {
            show(sprintf("*int != softfloat (%y, %y)", a, b),
                auto sub () { return c296(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v16) {
            show(sprintf("*int != *softfloat (%y, %y)", a, b),
                auto sub () { return c297(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v0) {
            show(sprintf("*float != int (%y, %y)", a, b),
                auto sub () { return c298(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v1) {
            show(sprintf("*float != float (%y, %y)", a, b),
                auto sub () { return c299(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v2) {
            show(sprintf("*float != number (%y, %y)", a, b),
                auto sub () { return c300(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v3) {
            show(sprintf("*float != *number (%y, %y)", a, b),
                auto sub () { return c301(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v4) {
            show(sprintf("*float != union<int, float> (%y, %y)", a, b),
                auto sub () { return c302(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v5) {
            show(sprintf("*float != union<int, number> (%y, %y)", a, b),
                auto sub () { return c303(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v6) {
            show(sprintf("*float != *union<int, float> (%y, %y)", a, b),
                auto sub () { return c304(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v7) {
            show(sprintf("*float != union<int, bool> (%y, %y)", a, b),
                auto sub () { return c305(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v8) {
            show(sprintf("*float != union<int, string> (%y, %y)", a, b),
                auto sub () { return c306(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v9) {
            show(sprintf("*float != *int (%y, %y)", a, b),
                auto sub () { return c307(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v10) {
            show(sprintf("*float != *float (%y, %y)", a, b),
                auto sub () { return c308(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v11) {
            show(sprintf("*float != bool (%y, %y)", a, b),
                auto sub () { return c309(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v12) {
            show(sprintf("*float != *bool (%y, %y)", a, b),
                auto sub () { return c310(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v13) {
            show(sprintf("*float != softint (%y, %y)", a, b),
                auto sub () { return c311(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v14) {
            show(sprintf("*float != *softint (%y, %y)", a, b),
                auto sub () { return c312(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v15) {
            show(sprintf("*float != softfloat (%y, %y)", a, b),
                auto sub () { return c313(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v16) {
            show(sprintf("*float != *softfloat (%y, %y)", a, b),
                auto sub () { return c314(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v0) {
            show(sprintf("bool != int (%y, %y)", a, b),
                auto sub () { return c315(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v1) {
            show(sprintf("bool != float (%y, %y)", a, b),
                auto sub () { return c316(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v2) {
            show(sprintf("bool != number (%y, %y)", a, b),
                auto sub () { return c317(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v3) {
            show(sprintf("bool != *number (%y, %y)", a, b),
                auto sub () { return c318(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v4) {
            show(sprintf("bool != union<int, float> (%y, %y)", a, b),
                auto sub () { return c319(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v5) {
            show(sprintf("bool != union<int, number> (%y, %y)", a, b),
                auto sub () { return c320(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v6) {
            show(sprintf("bool != *union<int, float> (%y, %y)", a, b),
                auto sub () { return c321(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v7) {
            show(sprintf("bool != union<int, bool> (%y, %y)", a, b),
                auto sub () { return c322(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v8) {
            show(sprintf("bool != union<int, string> (%y, %y)", a, b),
                auto sub () { return c323(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v9) {
            show(sprintf("bool != *int (%y, %y)", a, b),
                auto sub () { return c324(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v10) {
            show(sprintf("bool != *float (%y, %y)", a, b),
                auto sub () { return c325(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v11) {
            show(sprintf("bool != bool (%y, %y)", a, b),
                auto sub () { return c326(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v12) {
            show(sprintf("bool != *bool (%y, %y)", a, b),
                auto sub () { return c327(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v13) {
            show(sprintf("bool != softint (%y, %y)", a, b),
                auto sub () { return c328(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v14) {
            show(sprintf("bool != *softint (%y, %y)", a, b),
                auto sub () { return c329(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v15) {
            show(sprintf("bool != softfloat (%y, %y)", a, b),
                auto sub () { return c330(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v16) {
            show(sprintf("bool != *softfloat (%y, %y)", a, b),
                auto sub () { return c331(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v0) {
            show(sprintf("*bool != int (%y, %y)", a, b),
                auto sub () { return c332(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v1) {
            show(sprintf("*bool != float (%y, %y)", a, b),
                auto sub () { return c333(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v2) {
            show(sprintf("*bool != number (%y, %y)", a, b),
                auto sub () { return c334(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v3) {
            show(sprintf("*bool != *number (%y, %y)", a, b),
                auto sub () { return c335(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v4) {
            show(sprintf("*bool != union<int, float> (%y, %y)", a, b),
                auto sub () { return c336(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v5) {
            show(sprintf("*bool != union<int, number> (%y, %y)", a, b),
                auto sub () { return c337(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v6) {
            show(sprintf("*bool != *union<int, float> (%y, %y)", a, b),
                auto sub () { return c338(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v7) {
            show(sprintf("*bool != union<int, bool> (%y, %y)", a, b),
                auto sub () { return c339(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v8) {
            show(sprintf("*bool != union<int, string> (%y, %y)", a, b),
                auto sub () { return c340(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v9) {
            show(sprintf("*bool != *int (%y, %y)", a, b),
                auto sub () { return c341(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v10) {
            show(sprintf("*bool != *float (%y, %y)", a, b),
                auto sub () { return c342(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v11) {
            show(sprintf("*bool != bool (%y, %y)", a, b),
                auto sub () { return c343(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v12) {
            show(sprintf("*bool != *bool (%y, %y)", a, b),
                auto sub () { return c344(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v13) {
            show(sprintf("*bool != softint (%y, %y)", a, b),
                auto sub () { return c345(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v14) {
            show(sprintf("*bool != *softint (%y, %y)", a, b),
                auto sub () { return c346(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v15) {
            show(sprintf("*bool != softfloat (%y, %y)", a, b),
                auto sub () { return c347(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v16) {
            show(sprintf("*bool != *softfloat (%y, %y)", a, b),
                auto sub () { return c348(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v0) {
            show(sprintf("softint != int (%y, %y)", a, b),
                auto sub () { return c349(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v1) {
            show(sprintf("softint != float (%y, %y)", a, b),
                auto sub () { return c350(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v2) {
            show(sprintf("softint != number (%y, %y)", a, b),
                auto sub () { return c351(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v3) {
            show(sprintf("softint != *number (%y, %y)", a, b),
                auto sub () { return c352(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v4) {
            show(sprintf("softint != union<int, float> (%y, %y)", a, b),
                auto sub () { return c353(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v5) {
            show(sprintf("softint != union<int, number> (%y, %y)", a, b),
                auto sub () { return c354(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v6) {
            show(sprintf("softint != *union<int, float> (%y, %y)", a, b),
                auto sub () { return c355(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v7) {
            show(sprintf("softint != union<int, bool> (%y, %y)", a, b),
                auto sub () { return c356(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v8) {
            show(sprintf("softint != union<int, string> (%y, %y)", a, b),
                auto sub () { return c357(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v9) {
            show(sprintf("softint != *int (%y, %y)", a, b),
                auto sub () { return c358(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v10) {
            show(sprintf("softint != *float (%y, %y)", a, b),
                auto sub () { return c359(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v11) {
            show(sprintf("softint != bool (%y, %y)", a, b),
                auto sub () { return c360(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v12) {
            show(sprintf("softint != *bool (%y, %y)", a, b),
                auto sub () { return c361(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v13) {
            show(sprintf("softint != softint (%y, %y)", a, b),
                auto sub () { return c362(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v14) {
            show(sprintf("softint != *softint (%y, %y)", a, b),
                auto sub () { return c363(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v15) {
            show(sprintf("softint != softfloat (%y, %y)", a, b),
                auto sub () { return c364(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v16) {
            show(sprintf("softint != *softfloat (%y, %y)", a, b),
                auto sub () { return c365(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v0) {
            show(sprintf("*softint != int (%y, %y)", a, b),
                auto sub () { return c366(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v1) {
            show(sprintf("*softint != float (%y, %y)", a, b),
                auto sub () { return c367(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v2) {
            show(sprintf("*softint != number (%y, %y)", a, b),
                auto sub () { return c368(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v3) {
            show(sprintf("*softint != *number (%y, %y)", a, b),
                auto sub () { return c369(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v4) {
            show(sprintf("*softint != union<int, float> (%y, %y)", a, b),
                auto sub () { return c370(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v5) {
            show(sprintf("*softint != union<int, number> (%y, %y)", a, b),
                auto sub () { return c371(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v6) {
            show(sprintf("*softint != *union<int, float> (%y, %y)", a, b),
                auto sub () { return c372(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v7) {
            show(sprintf("*softint != union<int, bool> (%y, %y)", a, b),
                auto sub () { return c373(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v8) {
            show(sprintf("*softint != union<int, string> (%y, %y)", a, b),
                auto sub () { return c374(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v9) {
            show(sprintf("*softint != *int (%y, %y)", a, b),
                auto sub () { return c375(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v10) {
            show(sprintf("*softint != *float (%y, %y)", a, b),
                auto sub () { return c376(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v11) {
            show(sprintf("*softint != bool (%y, %y)", a, b),
                auto sub () { return c377(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v12) {
            show(sprintf("*softint != *bool (%y, %y)", a, b),
                auto sub () { return c378(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v13) {
            show(sprintf("*softint != softint (%y, %y)", a, b),
                auto sub () { return c379(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v14) {
            show(sprintf("*softint != *softint (%y, %y)", a, b),
                auto sub () { return c380(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v15) {
            show(sprintf("*softint != softfloat (%y, %y)", a, b),
                auto sub () { return c381(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v16) {
            show(sprintf("*softint != *softfloat (%y, %y)", a, b),
                auto sub () { return c382(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v0) {
            show(sprintf("softfloat != int (%y, %y)", a, b),
                auto sub () { return c383(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v1) {
            show(sprintf("softfloat != float (%y, %y)", a, b),
                auto sub () { return c384(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v2) {
            show(sprintf("softfloat != number (%y, %y)", a, b),
                auto sub () { return c385(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v3) {
            show(sprintf("softfloat != *number (%y, %y)", a, b),
                auto sub () { return c386(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v4) {
            show(sprintf("softfloat != union<int, float> (%y, %y)", a, b),
                auto sub () { return c387(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v5) {
            show(sprintf("softfloat != union<int, number> (%y, %y)", a, b),
                auto sub () { return c388(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v6) {
            show(sprintf("softfloat != *union<int, float> (%y, %y)", a, b),
                auto sub () { return c389(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v7) {
            show(sprintf("softfloat != union<int, bool> (%y, %y)", a, b),
                auto sub () { return c390(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v8) {
            show(sprintf("softfloat != union<int, string> (%y, %y)", a, b),
                auto sub () { return c391(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v9) {
            show(sprintf("softfloat != *int (%y, %y)", a, b),
                auto sub () { return c392(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v10) {
            show(sprintf("softfloat != *float (%y, %y)", a, b),
                auto sub () { return c393(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v11) {
            show(sprintf("softfloat != bool (%y, %y)", a, b),
                auto sub () { return c394(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v12) {
            show(sprintf("softfloat != *bool (%y, %y)", a, b),
                auto sub () { return c395(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v13) {
            show(sprintf("softfloat != softint (%y, %y)", a, b),
                auto sub () { return c396(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v14) {
            show(sprintf("softfloat != *softint (%y, %y)", a, b),
                auto sub () { return c397(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v15) {
            show(sprintf("softfloat != softfloat (%y, %y)", a, b),
                auto sub () { return c398(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v16) {
            show(sprintf("softfloat != *softfloat (%y, %y)", a, b),
                auto sub () { return c399(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v0) {
            show(sprintf("*softfloat != int (%y, %y)", a, b),
                auto sub () { return c400(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v1) {
            show(sprintf("*softfloat != float (%y, %y)", a, b),
                auto sub () { return c401(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v2) {
            show(sprintf("*softfloat != number (%y, %y)", a, b),
                auto sub () { return c402(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v3) {
            show(sprintf("*softfloat != *number (%y, %y)", a, b),
                auto sub () { return c403(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v4) {
            show(sprintf("*softfloat != union<int, float> (%y, %y)", a, b),
                auto sub () { return c404(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v5) {
            show(sprintf("*softfloat != union<int, number> (%y, %y)", a, b),
                auto sub () { return c405(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v6) {
            show(sprintf("*softfloat != *union<int, float> (%y, %y)", a, b),
                auto sub () { return c406(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v7) {
            show(sprintf("*softfloat != union<int, bool> (%y, %y)", a, b),
                auto sub () { return c407(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v8) {
            show(sprintf("*softfloat != union<int, string> (%y, %y)", a, b),
                auto sub () { return c408(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v9) {
            show(sprintf("*softfloat != *int (%y, %y)", a, b),
                auto sub () { return c409(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v10) {
            show(sprintf("*softfloat != *float (%y, %y)", a, b),
                auto sub () { return c410(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v11) {
            show(sprintf("*softfloat != bool (%y, %y)", a, b),
                auto sub () { return c411(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v12) {
            show(sprintf("*softfloat != *bool (%y, %y)", a, b),
                auto sub () { return c412(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v13) {
            show(sprintf("*softfloat != softint (%y, %y)", a, b),
                auto sub () { return c413(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v14) {
            show(sprintf("*softfloat != *softint (%y, %y)", a, b),
                auto sub () { return c414(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v15) {
            show(sprintf("*softfloat != softfloat (%y, %y)", a, b),
                auto sub () { return c415(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v16) {
            show(sprintf("*softfloat != *softfloat (%y, %y)", a, b),
                auto sub () { return c416(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v9) {
            show(sprintf("int < *int (%y, %y)", a, b),
                auto sub () { return c417(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v10) {
            show(sprintf("int < *float (%y, %y)", a, b),
                auto sub () { return c418(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v11) {
            show(sprintf("int < bool (%y, %y)", a, b),
                auto sub () { return c419(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v12) {
            show(sprintf("int < *bool (%y, %y)", a, b),
                auto sub () { return c420(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v13) {
            show(sprintf("int < softint (%y, %y)", a, b),
                auto sub () { return c421(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v14) {
            show(sprintf("int < *softint (%y, %y)", a, b),
                auto sub () { return c422(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v15) {
            show(sprintf("int < softfloat (%y, %y)", a, b),
                auto sub () { return c423(a, b); });
        }
    }
    foreach auto a in (v0) {
        foreach auto b in (v16) {
            show(sprintf("int < *softfloat (%y, %y)", a, b),
                auto sub () { return c424(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v9) {
            show(sprintf("float < *int (%y, %y)", a, b),
                auto sub () { return c425(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v10) {
            show(sprintf("float < *float (%y, %y)", a, b),
                auto sub () { return c426(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v11) {
            show(sprintf("float < bool (%y, %y)", a, b),
                auto sub () { return c427(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v12) {
            show(sprintf("float < *bool (%y, %y)", a, b),
                auto sub () { return c428(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v13) {
            show(sprintf("float < softint (%y, %y)", a, b),
                auto sub () { return c429(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v14) {
            show(sprintf("float < *softint (%y, %y)", a, b),
                auto sub () { return c430(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v15) {
            show(sprintf("float < softfloat (%y, %y)", a, b),
                auto sub () { return c431(a, b); });
        }
    }
    foreach auto a in (v1) {
        foreach auto b in (v16) {
            show(sprintf("float < *softfloat (%y, %y)", a, b),
                auto sub () { return c432(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v9) {
            show(sprintf("number < *int (%y, %y)", a, b),
                auto sub () { return c433(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v10) {
            show(sprintf("number < *float (%y, %y)", a, b),
                auto sub () { return c434(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v11) {
            show(sprintf("number < bool (%y, %y)", a, b),
                auto sub () { return c435(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v12) {
            show(sprintf("number < *bool (%y, %y)", a, b),
                auto sub () { return c436(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v13) {
            show(sprintf("number < softint (%y, %y)", a, b),
                auto sub () { return c437(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v14) {
            show(sprintf("number < *softint (%y, %y)", a, b),
                auto sub () { return c438(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v15) {
            show(sprintf("number < softfloat (%y, %y)", a, b),
                auto sub () { return c439(a, b); });
        }
    }
    foreach auto a in (v2) {
        foreach auto b in (v16) {
            show(sprintf("number < *softfloat (%y, %y)", a, b),
                auto sub () { return c440(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v9) {
            show(sprintf("*number < *int (%y, %y)", a, b),
                auto sub () { return c441(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v10) {
            show(sprintf("*number < *float (%y, %y)", a, b),
                auto sub () { return c442(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v11) {
            show(sprintf("*number < bool (%y, %y)", a, b),
                auto sub () { return c443(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v12) {
            show(sprintf("*number < *bool (%y, %y)", a, b),
                auto sub () { return c444(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v13) {
            show(sprintf("*number < softint (%y, %y)", a, b),
                auto sub () { return c445(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v14) {
            show(sprintf("*number < *softint (%y, %y)", a, b),
                auto sub () { return c446(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v15) {
            show(sprintf("*number < softfloat (%y, %y)", a, b),
                auto sub () { return c447(a, b); });
        }
    }
    foreach auto a in (v3) {
        foreach auto b in (v16) {
            show(sprintf("*number < *softfloat (%y, %y)", a, b),
                auto sub () { return c448(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v9) {
            show(sprintf("union<int, float> < *int (%y, %y)", a, b),
                auto sub () { return c449(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v10) {
            show(sprintf("union<int, float> < *float (%y, %y)", a, b),
                auto sub () { return c450(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v11) {
            show(sprintf("union<int, float> < bool (%y, %y)", a, b),
                auto sub () { return c451(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v12) {
            show(sprintf("union<int, float> < *bool (%y, %y)", a, b),
                auto sub () { return c452(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v13) {
            show(sprintf("union<int, float> < softint (%y, %y)", a, b),
                auto sub () { return c453(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v14) {
            show(sprintf("union<int, float> < *softint (%y, %y)", a, b),
                auto sub () { return c454(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v15) {
            show(sprintf("union<int, float> < softfloat (%y, %y)", a, b),
                auto sub () { return c455(a, b); });
        }
    }
    foreach auto a in (v4) {
        foreach auto b in (v16) {
            show(sprintf("union<int, float> < *softfloat (%y, %y)", a, b),
                auto sub () { return c456(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v9) {
            show(sprintf("union<int, number> < *int (%y, %y)", a, b),
                auto sub () { return c457(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v10) {
            show(sprintf("union<int, number> < *float (%y, %y)", a, b),
                auto sub () { return c458(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v11) {
            show(sprintf("union<int, number> < bool (%y, %y)", a, b),
                auto sub () { return c459(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v12) {
            show(sprintf("union<int, number> < *bool (%y, %y)", a, b),
                auto sub () { return c460(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v13) {
            show(sprintf("union<int, number> < softint (%y, %y)", a, b),
                auto sub () { return c461(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v14) {
            show(sprintf("union<int, number> < *softint (%y, %y)", a, b),
                auto sub () { return c462(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v15) {
            show(sprintf("union<int, number> < softfloat (%y, %y)", a, b),
                auto sub () { return c463(a, b); });
        }
    }
    foreach auto a in (v5) {
        foreach auto b in (v16) {
            show(sprintf("union<int, number> < *softfloat (%y, %y)", a, b),
                auto sub () { return c464(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v9) {
            show(sprintf("*union<int, float> < *int (%y, %y)", a, b),
                auto sub () { return c465(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v10) {
            show(sprintf("*union<int, float> < *float (%y, %y)", a, b),
                auto sub () { return c466(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v11) {
            show(sprintf("*union<int, float> < bool (%y, %y)", a, b),
                auto sub () { return c467(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v12) {
            show(sprintf("*union<int, float> < *bool (%y, %y)", a, b),
                auto sub () { return c468(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v13) {
            show(sprintf("*union<int, float> < softint (%y, %y)", a, b),
                auto sub () { return c469(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v14) {
            show(sprintf("*union<int, float> < *softint (%y, %y)", a, b),
                auto sub () { return c470(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v15) {
            show(sprintf("*union<int, float> < softfloat (%y, %y)", a, b),
                auto sub () { return c471(a, b); });
        }
    }
    foreach auto a in (v6) {
        foreach auto b in (v16) {
            show(sprintf("*union<int, float> < *softfloat (%y, %y)", a, b),
                auto sub () { return c472(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v9) {
            show(sprintf("union<int, bool> < *int (%y, %y)", a, b),
                auto sub () { return c473(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v10) {
            show(sprintf("union<int, bool> < *float (%y, %y)", a, b),
                auto sub () { return c474(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v11) {
            show(sprintf("union<int, bool> < bool (%y, %y)", a, b),
                auto sub () { return c475(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v12) {
            show(sprintf("union<int, bool> < *bool (%y, %y)", a, b),
                auto sub () { return c476(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v13) {
            show(sprintf("union<int, bool> < softint (%y, %y)", a, b),
                auto sub () { return c477(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v14) {
            show(sprintf("union<int, bool> < *softint (%y, %y)", a, b),
                auto sub () { return c478(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v15) {
            show(sprintf("union<int, bool> < softfloat (%y, %y)", a, b),
                auto sub () { return c479(a, b); });
        }
    }
    foreach auto a in (v7) {
        foreach auto b in (v16) {
            show(sprintf("union<int, bool> < *softfloat (%y, %y)", a, b),
                auto sub () { return c480(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v9) {
            show(sprintf("union<int, string> < *int (%y, %y)", a, b),
                auto sub () { return c481(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v10) {
            show(sprintf("union<int, string> < *float (%y, %y)", a, b),
                auto sub () { return c482(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v11) {
            show(sprintf("union<int, string> < bool (%y, %y)", a, b),
                auto sub () { return c483(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v12) {
            show(sprintf("union<int, string> < *bool (%y, %y)", a, b),
                auto sub () { return c484(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v13) {
            show(sprintf("union<int, string> < softint (%y, %y)", a, b),
                auto sub () { return c485(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v14) {
            show(sprintf("union<int, string> < *softint (%y, %y)", a, b),
                auto sub () { return c486(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v15) {
            show(sprintf("union<int, string> < softfloat (%y, %y)", a, b),
                auto sub () { return c487(a, b); });
        }
    }
    foreach auto a in (v8) {
        foreach auto b in (v16) {
            show(sprintf("union<int, string> < *softfloat (%y, %y)", a, b),
                auto sub () { return c488(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v0) {
            show(sprintf("*int < int (%y, %y)", a, b),
                auto sub () { return c489(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v1) {
            show(sprintf("*int < float (%y, %y)", a, b),
                auto sub () { return c490(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v2) {
            show(sprintf("*int < number (%y, %y)", a, b),
                auto sub () { return c491(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v3) {
            show(sprintf("*int < *number (%y, %y)", a, b),
                auto sub () { return c492(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v4) {
            show(sprintf("*int < union<int, float> (%y, %y)", a, b),
                auto sub () { return c493(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v5) {
            show(sprintf("*int < union<int, number> (%y, %y)", a, b),
                auto sub () { return c494(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v6) {
            show(sprintf("*int < *union<int, float> (%y, %y)", a, b),
                auto sub () { return c495(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v7) {
            show(sprintf("*int < union<int, bool> (%y, %y)", a, b),
                auto sub () { return c496(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v8) {
            show(sprintf("*int < union<int, string> (%y, %y)", a, b),
                auto sub () { return c497(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v9) {
            show(sprintf("*int < *int (%y, %y)", a, b),
                auto sub () { return c498(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v10) {
            show(sprintf("*int < *float (%y, %y)", a, b),
                auto sub () { return c499(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v11) {
            show(sprintf("*int < bool (%y, %y)", a, b),
                auto sub () { return c500(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v12) {
            show(sprintf("*int < *bool (%y, %y)", a, b),
                auto sub () { return c501(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v13) {
            show(sprintf("*int < softint (%y, %y)", a, b),
                auto sub () { return c502(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v14) {
            show(sprintf("*int < *softint (%y, %y)", a, b),
                auto sub () { return c503(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v15) {
            show(sprintf("*int < softfloat (%y, %y)", a, b),
                auto sub () { return c504(a, b); });
        }
    }
    foreach auto a in (v9) {
        foreach auto b in (v16) {
            show(sprintf("*int < *softfloat (%y, %y)", a, b),
                auto sub () { return c505(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v0) {
            show(sprintf("*float < int (%y, %y)", a, b),
                auto sub () { return c506(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v1) {
            show(sprintf("*float < float (%y, %y)", a, b),
                auto sub () { return c507(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v2) {
            show(sprintf("*float < number (%y, %y)", a, b),
                auto sub () { return c508(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v3) {
            show(sprintf("*float < *number (%y, %y)", a, b),
                auto sub () { return c509(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v4) {
            show(sprintf("*float < union<int, float> (%y, %y)", a, b),
                auto sub () { return c510(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v5) {
            show(sprintf("*float < union<int, number> (%y, %y)", a, b),
                auto sub () { return c511(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v6) {
            show(sprintf("*float < *union<int, float> (%y, %y)", a, b),
                auto sub () { return c512(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v7) {
            show(sprintf("*float < union<int, bool> (%y, %y)", a, b),
                auto sub () { return c513(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v8) {
            show(sprintf("*float < union<int, string> (%y, %y)", a, b),
                auto sub () { return c514(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v9) {
            show(sprintf("*float < *int (%y, %y)", a, b),
                auto sub () { return c515(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v10) {
            show(sprintf("*float < *float (%y, %y)", a, b),
                auto sub () { return c516(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v11) {
            show(sprintf("*float < bool (%y, %y)", a, b),
                auto sub () { return c517(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v12) {
            show(sprintf("*float < *bool (%y, %y)", a, b),
                auto sub () { return c518(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v13) {
            show(sprintf("*float < softint (%y, %y)", a, b),
                auto sub () { return c519(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v14) {
            show(sprintf("*float < *softint (%y, %y)", a, b),
                auto sub () { return c520(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v15) {
            show(sprintf("*float < softfloat (%y, %y)", a, b),
                auto sub () { return c521(a, b); });
        }
    }
    foreach auto a in (v10) {
        foreach auto b in (v16) {
            show(sprintf("*float < *softfloat (%y, %y)", a, b),
                auto sub () { return c522(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v0) {
            show(sprintf("bool < int (%y, %y)", a, b),
                auto sub () { return c523(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v1) {
            show(sprintf("bool < float (%y, %y)", a, b),
                auto sub () { return c524(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v2) {
            show(sprintf("bool < number (%y, %y)", a, b),
                auto sub () { return c525(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v3) {
            show(sprintf("bool < *number (%y, %y)", a, b),
                auto sub () { return c526(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v4) {
            show(sprintf("bool < union<int, float> (%y, %y)", a, b),
                auto sub () { return c527(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v5) {
            show(sprintf("bool < union<int, number> (%y, %y)", a, b),
                auto sub () { return c528(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v6) {
            show(sprintf("bool < *union<int, float> (%y, %y)", a, b),
                auto sub () { return c529(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v7) {
            show(sprintf("bool < union<int, bool> (%y, %y)", a, b),
                auto sub () { return c530(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v8) {
            show(sprintf("bool < union<int, string> (%y, %y)", a, b),
                auto sub () { return c531(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v9) {
            show(sprintf("bool < *int (%y, %y)", a, b),
                auto sub () { return c532(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v10) {
            show(sprintf("bool < *float (%y, %y)", a, b),
                auto sub () { return c533(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v11) {
            show(sprintf("bool < bool (%y, %y)", a, b),
                auto sub () { return c534(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v12) {
            show(sprintf("bool < *bool (%y, %y)", a, b),
                auto sub () { return c535(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v13) {
            show(sprintf("bool < softint (%y, %y)", a, b),
                auto sub () { return c536(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v14) {
            show(sprintf("bool < *softint (%y, %y)", a, b),
                auto sub () { return c537(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v15) {
            show(sprintf("bool < softfloat (%y, %y)", a, b),
                auto sub () { return c538(a, b); });
        }
    }
    foreach auto a in (v11) {
        foreach auto b in (v16) {
            show(sprintf("bool < *softfloat (%y, %y)", a, b),
                auto sub () { return c539(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v0) {
            show(sprintf("*bool < int (%y, %y)", a, b),
                auto sub () { return c540(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v1) {
            show(sprintf("*bool < float (%y, %y)", a, b),
                auto sub () { return c541(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v2) {
            show(sprintf("*bool < number (%y, %y)", a, b),
                auto sub () { return c542(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v3) {
            show(sprintf("*bool < *number (%y, %y)", a, b),
                auto sub () { return c543(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v4) {
            show(sprintf("*bool < union<int, float> (%y, %y)", a, b),
                auto sub () { return c544(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v5) {
            show(sprintf("*bool < union<int, number> (%y, %y)", a, b),
                auto sub () { return c545(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v6) {
            show(sprintf("*bool < *union<int, float> (%y, %y)", a, b),
                auto sub () { return c546(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v7) {
            show(sprintf("*bool < union<int, bool> (%y, %y)", a, b),
                auto sub () { return c547(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v8) {
            show(sprintf("*bool < union<int, string> (%y, %y)", a, b),
                auto sub () { return c548(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v9) {
            show(sprintf("*bool < *int (%y, %y)", a, b),
                auto sub () { return c549(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v10) {
            show(sprintf("*bool < *float (%y, %y)", a, b),
                auto sub () { return c550(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v11) {
            show(sprintf("*bool < bool (%y, %y)", a, b),
                auto sub () { return c551(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v12) {
            show(sprintf("*bool < *bool (%y, %y)", a, b),
                auto sub () { return c552(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v13) {
            show(sprintf("*bool < softint (%y, %y)", a, b),
                auto sub () { return c553(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v14) {
            show(sprintf("*bool < *softint (%y, %y)", a, b),
                auto sub () { return c554(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v15) {
            show(sprintf("*bool < softfloat (%y, %y)", a, b),
                auto sub () { return c555(a, b); });
        }
    }
    foreach auto a in (v12) {
        foreach auto b in (v16) {
            show(sprintf("*bool < *softfloat (%y, %y)", a, b),
                auto sub () { return c556(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v0) {
            show(sprintf("softint < int (%y, %y)", a, b),
                auto sub () { return c557(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v1) {
            show(sprintf("softint < float (%y, %y)", a, b),
                auto sub () { return c558(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v2) {
            show(sprintf("softint < number (%y, %y)", a, b),
                auto sub () { return c559(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v3) {
            show(sprintf("softint < *number (%y, %y)", a, b),
                auto sub () { return c560(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v4) {
            show(sprintf("softint < union<int, float> (%y, %y)", a, b),
                auto sub () { return c561(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v5) {
            show(sprintf("softint < union<int, number> (%y, %y)", a, b),
                auto sub () { return c562(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v6) {
            show(sprintf("softint < *union<int, float> (%y, %y)", a, b),
                auto sub () { return c563(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v7) {
            show(sprintf("softint < union<int, bool> (%y, %y)", a, b),
                auto sub () { return c564(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v8) {
            show(sprintf("softint < union<int, string> (%y, %y)", a, b),
                auto sub () { return c565(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v9) {
            show(sprintf("softint < *int (%y, %y)", a, b),
                auto sub () { return c566(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v10) {
            show(sprintf("softint < *float (%y, %y)", a, b),
                auto sub () { return c567(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v11) {
            show(sprintf("softint < bool (%y, %y)", a, b),
                auto sub () { return c568(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v12) {
            show(sprintf("softint < *bool (%y, %y)", a, b),
                auto sub () { return c569(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v13) {
            show(sprintf("softint < softint (%y, %y)", a, b),
                auto sub () { return c570(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v14) {
            show(sprintf("softint < *softint (%y, %y)", a, b),
                auto sub () { return c571(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v15) {
            show(sprintf("softint < softfloat (%y, %y)", a, b),
                auto sub () { return c572(a, b); });
        }
    }
    foreach auto a in (v13) {
        foreach auto b in (v16) {
            show(sprintf("softint < *softfloat (%y, %y)", a, b),
                auto sub () { return c573(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v0) {
            show(sprintf("*softint < int (%y, %y)", a, b),
                auto sub () { return c574(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v1) {
            show(sprintf("*softint < float (%y, %y)", a, b),
                auto sub () { return c575(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v2) {
            show(sprintf("*softint < number (%y, %y)", a, b),
                auto sub () { return c576(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v3) {
            show(sprintf("*softint < *number (%y, %y)", a, b),
                auto sub () { return c577(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v4) {
            show(sprintf("*softint < union<int, float> (%y, %y)", a, b),
                auto sub () { return c578(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v5) {
            show(sprintf("*softint < union<int, number> (%y, %y)", a, b),
                auto sub () { return c579(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v6) {
            show(sprintf("*softint < *union<int, float> (%y, %y)", a, b),
                auto sub () { return c580(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v7) {
            show(sprintf("*softint < union<int, bool> (%y, %y)", a, b),
                auto sub () { return c581(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v8) {
            show(sprintf("*softint < union<int, string> (%y, %y)", a, b),
                auto sub () { return c582(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v9) {
            show(sprintf("*softint < *int (%y, %y)", a, b),
                auto sub () { return c583(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v10) {
            show(sprintf("*softint < *float (%y, %y)", a, b),
                auto sub () { return c584(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v11) {
            show(sprintf("*softint < bool (%y, %y)", a, b),
                auto sub () { return c585(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v12) {
            show(sprintf("*softint < *bool (%y, %y)", a, b),
                auto sub () { return c586(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v13) {
            show(sprintf("*softint < softint (%y, %y)", a, b),
                auto sub () { return c587(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v14) {
            show(sprintf("*softint < *softint (%y, %y)", a, b),
                auto sub () { return c588(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v15) {
            show(sprintf("*softint < softfloat (%y, %y)", a, b),
                auto sub () { return c589(a, b); });
        }
    }
    foreach auto a in (v14) {
        foreach auto b in (v16) {
            show(sprintf("*softint < *softfloat (%y, %y)", a, b),
                auto sub () { return c590(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v0) {
            show(sprintf("softfloat < int (%y, %y)", a, b),
                auto sub () { return c591(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v1) {
            show(sprintf("softfloat < float (%y, %y)", a, b),
                auto sub () { return c592(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v2) {
            show(sprintf("softfloat < number (%y, %y)", a, b),
                auto sub () { return c593(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v3) {
            show(sprintf("softfloat < *number (%y, %y)", a, b),
                auto sub () { return c594(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v4) {
            show(sprintf("softfloat < union<int, float> (%y, %y)", a, b),
                auto sub () { return c595(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v5) {
            show(sprintf("softfloat < union<int, number> (%y, %y)", a, b),
                auto sub () { return c596(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v6) {
            show(sprintf("softfloat < *union<int, float> (%y, %y)", a, b),
                auto sub () { return c597(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v7) {
            show(sprintf("softfloat < union<int, bool> (%y, %y)", a, b),
                auto sub () { return c598(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v8) {
            show(sprintf("softfloat < union<int, string> (%y, %y)", a, b),
                auto sub () { return c599(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v9) {
            show(sprintf("softfloat < *int (%y, %y)", a, b),
                auto sub () { return c600(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v10) {
            show(sprintf("softfloat < *float (%y, %y)", a, b),
                auto sub () { return c601(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v11) {
            show(sprintf("softfloat < bool (%y, %y)", a, b),
                auto sub () { return c602(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v12) {
            show(sprintf("softfloat < *bool (%y, %y)", a, b),
                auto sub () { return c603(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v13) {
            show(sprintf("softfloat < softint (%y, %y)", a, b),
                auto sub () { return c604(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v14) {
            show(sprintf("softfloat < *softint (%y, %y)", a, b),
                auto sub () { return c605(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v15) {
            show(sprintf("softfloat < softfloat (%y, %y)", a, b),
                auto sub () { return c606(a, b); });
        }
    }
    foreach auto a in (v15) {
        foreach auto b in (v16) {
            show(sprintf("softfloat < *softfloat (%y, %y)", a, b),
                auto sub () { return c607(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v0) {
            show(sprintf("*softfloat < int (%y, %y)", a, b),
                auto sub () { return c608(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v1) {
            show(sprintf("*softfloat < float (%y, %y)", a, b),
                auto sub () { return c609(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v2) {
            show(sprintf("*softfloat < number (%y, %y)", a, b),
                auto sub () { return c610(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v3) {
            show(sprintf("*softfloat < *number (%y, %y)", a, b),
                auto sub () { return c611(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v4) {
            show(sprintf("*softfloat < union<int, float> (%y, %y)", a, b),
                auto sub () { return c612(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v5) {
            show(sprintf("*softfloat < union<int, number> (%y, %y)", a, b),
                auto sub () { return c613(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v6) {
            show(sprintf("*softfloat < *union<int, float> (%y, %y)", a, b),
                auto sub () { return c614(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v7) {
            show(sprintf("*softfloat < union<int, bool> (%y, %y)", a, b),
                auto sub () { return c615(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v8) {
            show(sprintf("*softfloat < union<int, string> (%y, %y)", a, b),
                auto sub () { return c616(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v9) {
            show(sprintf("*softfloat < *int (%y, %y)", a, b),
                auto sub () { return c617(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v10) {
            show(sprintf("*softfloat < *float (%y, %y)", a, b),
                auto sub () { return c618(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v11) {
            show(sprintf("*softfloat < bool (%y, %y)", a, b),
                auto sub () { return c619(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v12) {
            show(sprintf("*softfloat < *bool (%y, %y)", a, b),
                auto sub () { return c620(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v13) {
            show(sprintf("*softfloat < softint (%y, %y)", a, b),
                auto sub () { return c621(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v14) {
            show(sprintf("*softfloat < *softint (%y, %y)", a, b),
                auto sub () { return c622(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v15) {
            show(sprintf("*softfloat < softfloat (%y, %y)", a, b),
                auto sub () { return c623(a, b); });
        }
    }
    foreach auto a in (v16) {
        foreach auto b in (v16) {
            show(sprintf("*softfloat < *softfloat (%y, %y)", a, b),
                auto sub () { return c624(a, b); });
        }
    }
}

main();
