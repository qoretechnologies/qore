%modern
%disable-warning invalid-operation

auto sub f1(int a, int b) { return a + b; }
auto sub f2(int a, *int b) { return a + b; }
auto sub f3(int a, float b) { return a + b; }
auto sub f4(int a, *float b) { return a + b; }
auto sub f5(*int a, int b) { return a + b; }
auto sub f6(*int a, *int b) { return a + b; }
auto sub f7(*int a, float b) { return a + b; }
auto sub f8(*int a, *float b) { return a + b; }
auto sub f9(float a, int b) { return a + b; }
auto sub f10(float a, *int b) { return a + b; }
auto sub f11(float a, float b) { return a + b; }
auto sub f12(float a, *float b) { return a + b; }
auto sub f13(*float a, int b) { return a + b; }
auto sub f14(*float a, *int b) { return a + b; }
auto sub f15(*float a, float b) { return a + b; }
auto sub f16(*float a, *float b) { return a + b; }
auto sub f17(int a, int b) { return a - b; }
auto sub f18(int a, *int b) { return a - b; }
auto sub f19(int a, float b) { return a - b; }
auto sub f20(int a, *float b) { return a - b; }
auto sub f21(*int a, int b) { return a - b; }
auto sub f22(*int a, *int b) { return a - b; }
auto sub f23(*int a, float b) { return a - b; }
auto sub f24(*int a, *float b) { return a - b; }
auto sub f25(float a, int b) { return a - b; }
auto sub f26(float a, *int b) { return a - b; }
auto sub f27(float a, float b) { return a - b; }
auto sub f28(float a, *float b) { return a - b; }
auto sub f29(*float a, int b) { return a - b; }
auto sub f30(*float a, *int b) { return a - b; }
auto sub f31(*float a, float b) { return a - b; }
auto sub f32(*float a, *float b) { return a - b; }
auto sub f33(int a, int b) { return a * b; }
auto sub f34(int a, *int b) { return a * b; }
auto sub f35(int a, float b) { return a * b; }
auto sub f36(int a, *float b) { return a * b; }
auto sub f37(*int a, int b) { return a * b; }
auto sub f38(*int a, *int b) { return a * b; }
auto sub f39(*int a, float b) { return a * b; }
auto sub f40(*int a, *float b) { return a * b; }
auto sub f41(float a, int b) { return a * b; }
auto sub f42(float a, *int b) { return a * b; }
auto sub f43(float a, float b) { return a * b; }
auto sub f44(float a, *float b) { return a * b; }
auto sub f45(*float a, int b) { return a * b; }
auto sub f46(*float a, *int b) { return a * b; }
auto sub f47(*float a, float b) { return a * b; }
auto sub f48(*float a, *float b) { return a * b; }
auto sub f49(int a, int b) { return a / b; }
auto sub f50(int a, *int b) { return a / b; }
auto sub f51(int a, float b) { return a / b; }
auto sub f52(int a, *float b) { return a / b; }
auto sub f53(*int a, int b) { return a / b; }
auto sub f54(*int a, *int b) { return a / b; }
auto sub f55(*int a, float b) { return a / b; }
auto sub f56(*int a, *float b) { return a / b; }
auto sub f57(float a, int b) { return a / b; }
auto sub f58(float a, *int b) { return a / b; }
auto sub f59(float a, float b) { return a / b; }
auto sub f60(float a, *float b) { return a / b; }
auto sub f61(*float a, int b) { return a / b; }
auto sub f62(*float a, *int b) { return a / b; }
auto sub f63(*float a, float b) { return a / b; }
auto sub f64(*float a, *float b) { return a / b; }
auto sub f65(int a, int b) { return a % b; }
auto sub f66(int a, *int b) { return a % b; }
auto sub f67(*int a, int b) { return a % b; }
auto sub f68(*int a, *int b) { return a % b; }
auto sub f69(int a, int b) { return a & b; }
auto sub f70(int a, *int b) { return a & b; }
auto sub f71(*int a, int b) { return a & b; }
auto sub f72(*int a, *int b) { return a & b; }
auto sub f73(int a, int b) { return a | b; }
auto sub f74(int a, *int b) { return a | b; }
auto sub f75(*int a, int b) { return a | b; }
auto sub f76(*int a, *int b) { return a | b; }
auto sub f77(int a, int b) { return a ^ b; }
auto sub f78(int a, *int b) { return a ^ b; }
auto sub f79(*int a, int b) { return a ^ b; }
auto sub f80(*int a, *int b) { return a ^ b; }
auto sub f81(int a, int b) { return a << b; }
auto sub f82(int a, *int b) { return a << b; }
auto sub f83(*int a, int b) { return a << b; }
auto sub f84(*int a, *int b) { return a << b; }
auto sub f85(int a, int b) { return a >> b; }
auto sub f86(int a, *int b) { return a >> b; }
auto sub f87(*int a, int b) { return a >> b; }
auto sub f88(*int a, *int b) { return a >> b; }

sub show(string label, code c) {
    string r;
    try {
        r = sprintf("%y", c());
    } catch (hash<ExceptionInfo> ex) {
        r = "EXC " + ex.err;
    }
    printf("%s = %s\n", label, r);
}

show("int + int: 0 + 0", auto sub () { return f1(0, 0); });
show("int + int: 0 + 3", auto sub () { return f1(0, 3); });
show("int + int: 0 + -2", auto sub () { return f1(0, -2); });
show("int + int: 3 + 0", auto sub () { return f1(3, 0); });
show("int + int: 3 + 3", auto sub () { return f1(3, 3); });
show("int + int: 3 + -2", auto sub () { return f1(3, -2); });
show("int + int: -2 + 0", auto sub () { return f1(-2, 0); });
show("int + int: -2 + 3", auto sub () { return f1(-2, 3); });
show("int + int: -2 + -2", auto sub () { return f1(-2, -2); });
show("int + int_: 0 + NOTHING", auto sub () { return f2(0, NOTHING); });
show("int + int_: 0 + 0", auto sub () { return f2(0, 0); });
show("int + int_: 0 + 3", auto sub () { return f2(0, 3); });
show("int + int_: 3 + NOTHING", auto sub () { return f2(3, NOTHING); });
show("int + int_: 3 + 0", auto sub () { return f2(3, 0); });
show("int + int_: 3 + 3", auto sub () { return f2(3, 3); });
show("int + int_: -2 + NOTHING", auto sub () { return f2(-2, NOTHING); });
show("int + int_: -2 + 0", auto sub () { return f2(-2, 0); });
show("int + int_: -2 + 3", auto sub () { return f2(-2, 3); });
show("int + float: 0 + 0.0", auto sub () { return f3(0, 0.0); });
show("int + float: 0 + 1.5", auto sub () { return f3(0, 1.5); });
show("int + float: 0 + -2.5", auto sub () { return f3(0, -2.5); });
show("int + float: 3 + 0.0", auto sub () { return f3(3, 0.0); });
show("int + float: 3 + 1.5", auto sub () { return f3(3, 1.5); });
show("int + float: 3 + -2.5", auto sub () { return f3(3, -2.5); });
show("int + float: -2 + 0.0", auto sub () { return f3(-2, 0.0); });
show("int + float: -2 + 1.5", auto sub () { return f3(-2, 1.5); });
show("int + float: -2 + -2.5", auto sub () { return f3(-2, -2.5); });
show("int + float_: 0 + NOTHING", auto sub () { return f4(0, NOTHING); });
show("int + float_: 0 + 0.0", auto sub () { return f4(0, 0.0); });
show("int + float_: 0 + 1.5", auto sub () { return f4(0, 1.5); });
show("int + float_: 3 + NOTHING", auto sub () { return f4(3, NOTHING); });
show("int + float_: 3 + 0.0", auto sub () { return f4(3, 0.0); });
show("int + float_: 3 + 1.5", auto sub () { return f4(3, 1.5); });
show("int + float_: -2 + NOTHING", auto sub () { return f4(-2, NOTHING); });
show("int + float_: -2 + 0.0", auto sub () { return f4(-2, 0.0); });
show("int + float_: -2 + 1.5", auto sub () { return f4(-2, 1.5); });
show("int_ + int: NOTHING + 0", auto sub () { return f5(NOTHING, 0); });
show("int_ + int: NOTHING + 3", auto sub () { return f5(NOTHING, 3); });
show("int_ + int: NOTHING + -2", auto sub () { return f5(NOTHING, -2); });
show("int_ + int: 0 + 0", auto sub () { return f5(0, 0); });
show("int_ + int: 0 + 3", auto sub () { return f5(0, 3); });
show("int_ + int: 0 + -2", auto sub () { return f5(0, -2); });
show("int_ + int: 3 + 0", auto sub () { return f5(3, 0); });
show("int_ + int: 3 + 3", auto sub () { return f5(3, 3); });
show("int_ + int: 3 + -2", auto sub () { return f5(3, -2); });
show("int_ + int_: NOTHING + NOTHING", auto sub () { return f6(NOTHING, NOTHING); });
show("int_ + int_: NOTHING + 0", auto sub () { return f6(NOTHING, 0); });
show("int_ + int_: NOTHING + 3", auto sub () { return f6(NOTHING, 3); });
show("int_ + int_: 0 + NOTHING", auto sub () { return f6(0, NOTHING); });
show("int_ + int_: 0 + 0", auto sub () { return f6(0, 0); });
show("int_ + int_: 0 + 3", auto sub () { return f6(0, 3); });
show("int_ + int_: 3 + NOTHING", auto sub () { return f6(3, NOTHING); });
show("int_ + int_: 3 + 0", auto sub () { return f6(3, 0); });
show("int_ + int_: 3 + 3", auto sub () { return f6(3, 3); });
show("int_ + float: NOTHING + 0.0", auto sub () { return f7(NOTHING, 0.0); });
show("int_ + float: NOTHING + 1.5", auto sub () { return f7(NOTHING, 1.5); });
show("int_ + float: NOTHING + -2.5", auto sub () { return f7(NOTHING, -2.5); });
show("int_ + float: 0 + 0.0", auto sub () { return f7(0, 0.0); });
show("int_ + float: 0 + 1.5", auto sub () { return f7(0, 1.5); });
show("int_ + float: 0 + -2.5", auto sub () { return f7(0, -2.5); });
show("int_ + float: 3 + 0.0", auto sub () { return f7(3, 0.0); });
show("int_ + float: 3 + 1.5", auto sub () { return f7(3, 1.5); });
show("int_ + float: 3 + -2.5", auto sub () { return f7(3, -2.5); });
show("int_ + float_: NOTHING + NOTHING", auto sub () { return f8(NOTHING, NOTHING); });
show("int_ + float_: NOTHING + 0.0", auto sub () { return f8(NOTHING, 0.0); });
show("int_ + float_: NOTHING + 1.5", auto sub () { return f8(NOTHING, 1.5); });
show("int_ + float_: 0 + NOTHING", auto sub () { return f8(0, NOTHING); });
show("int_ + float_: 0 + 0.0", auto sub () { return f8(0, 0.0); });
show("int_ + float_: 0 + 1.5", auto sub () { return f8(0, 1.5); });
show("int_ + float_: 3 + NOTHING", auto sub () { return f8(3, NOTHING); });
show("int_ + float_: 3 + 0.0", auto sub () { return f8(3, 0.0); });
show("int_ + float_: 3 + 1.5", auto sub () { return f8(3, 1.5); });
show("float + int: 0.0 + 0", auto sub () { return f9(0.0, 0); });
show("float + int: 0.0 + 3", auto sub () { return f9(0.0, 3); });
show("float + int: 0.0 + -2", auto sub () { return f9(0.0, -2); });
show("float + int: 1.5 + 0", auto sub () { return f9(1.5, 0); });
show("float + int: 1.5 + 3", auto sub () { return f9(1.5, 3); });
show("float + int: 1.5 + -2", auto sub () { return f9(1.5, -2); });
show("float + int: -2.5 + 0", auto sub () { return f9(-2.5, 0); });
show("float + int: -2.5 + 3", auto sub () { return f9(-2.5, 3); });
show("float + int: -2.5 + -2", auto sub () { return f9(-2.5, -2); });
show("float + int_: 0.0 + NOTHING", auto sub () { return f10(0.0, NOTHING); });
show("float + int_: 0.0 + 0", auto sub () { return f10(0.0, 0); });
show("float + int_: 0.0 + 3", auto sub () { return f10(0.0, 3); });
show("float + int_: 1.5 + NOTHING", auto sub () { return f10(1.5, NOTHING); });
show("float + int_: 1.5 + 0", auto sub () { return f10(1.5, 0); });
show("float + int_: 1.5 + 3", auto sub () { return f10(1.5, 3); });
show("float + int_: -2.5 + NOTHING", auto sub () { return f10(-2.5, NOTHING); });
show("float + int_: -2.5 + 0", auto sub () { return f10(-2.5, 0); });
show("float + int_: -2.5 + 3", auto sub () { return f10(-2.5, 3); });
show("float + float: 0.0 + 0.0", auto sub () { return f11(0.0, 0.0); });
show("float + float: 0.0 + 1.5", auto sub () { return f11(0.0, 1.5); });
show("float + float: 0.0 + -2.5", auto sub () { return f11(0.0, -2.5); });
show("float + float: 1.5 + 0.0", auto sub () { return f11(1.5, 0.0); });
show("float + float: 1.5 + 1.5", auto sub () { return f11(1.5, 1.5); });
show("float + float: 1.5 + -2.5", auto sub () { return f11(1.5, -2.5); });
show("float + float: -2.5 + 0.0", auto sub () { return f11(-2.5, 0.0); });
show("float + float: -2.5 + 1.5", auto sub () { return f11(-2.5, 1.5); });
show("float + float: -2.5 + -2.5", auto sub () { return f11(-2.5, -2.5); });
show("float + float_: 0.0 + NOTHING", auto sub () { return f12(0.0, NOTHING); });
show("float + float_: 0.0 + 0.0", auto sub () { return f12(0.0, 0.0); });
show("float + float_: 0.0 + 1.5", auto sub () { return f12(0.0, 1.5); });
show("float + float_: 1.5 + NOTHING", auto sub () { return f12(1.5, NOTHING); });
show("float + float_: 1.5 + 0.0", auto sub () { return f12(1.5, 0.0); });
show("float + float_: 1.5 + 1.5", auto sub () { return f12(1.5, 1.5); });
show("float + float_: -2.5 + NOTHING", auto sub () { return f12(-2.5, NOTHING); });
show("float + float_: -2.5 + 0.0", auto sub () { return f12(-2.5, 0.0); });
show("float + float_: -2.5 + 1.5", auto sub () { return f12(-2.5, 1.5); });
show("float_ + int: NOTHING + 0", auto sub () { return f13(NOTHING, 0); });
show("float_ + int: NOTHING + 3", auto sub () { return f13(NOTHING, 3); });
show("float_ + int: NOTHING + -2", auto sub () { return f13(NOTHING, -2); });
show("float_ + int: 0.0 + 0", auto sub () { return f13(0.0, 0); });
show("float_ + int: 0.0 + 3", auto sub () { return f13(0.0, 3); });
show("float_ + int: 0.0 + -2", auto sub () { return f13(0.0, -2); });
show("float_ + int: 1.5 + 0", auto sub () { return f13(1.5, 0); });
show("float_ + int: 1.5 + 3", auto sub () { return f13(1.5, 3); });
show("float_ + int: 1.5 + -2", auto sub () { return f13(1.5, -2); });
show("float_ + int_: NOTHING + NOTHING", auto sub () { return f14(NOTHING, NOTHING); });
show("float_ + int_: NOTHING + 0", auto sub () { return f14(NOTHING, 0); });
show("float_ + int_: NOTHING + 3", auto sub () { return f14(NOTHING, 3); });
show("float_ + int_: 0.0 + NOTHING", auto sub () { return f14(0.0, NOTHING); });
show("float_ + int_: 0.0 + 0", auto sub () { return f14(0.0, 0); });
show("float_ + int_: 0.0 + 3", auto sub () { return f14(0.0, 3); });
show("float_ + int_: 1.5 + NOTHING", auto sub () { return f14(1.5, NOTHING); });
show("float_ + int_: 1.5 + 0", auto sub () { return f14(1.5, 0); });
show("float_ + int_: 1.5 + 3", auto sub () { return f14(1.5, 3); });
show("float_ + float: NOTHING + 0.0", auto sub () { return f15(NOTHING, 0.0); });
show("float_ + float: NOTHING + 1.5", auto sub () { return f15(NOTHING, 1.5); });
show("float_ + float: NOTHING + -2.5", auto sub () { return f15(NOTHING, -2.5); });
show("float_ + float: 0.0 + 0.0", auto sub () { return f15(0.0, 0.0); });
show("float_ + float: 0.0 + 1.5", auto sub () { return f15(0.0, 1.5); });
show("float_ + float: 0.0 + -2.5", auto sub () { return f15(0.0, -2.5); });
show("float_ + float: 1.5 + 0.0", auto sub () { return f15(1.5, 0.0); });
show("float_ + float: 1.5 + 1.5", auto sub () { return f15(1.5, 1.5); });
show("float_ + float: 1.5 + -2.5", auto sub () { return f15(1.5, -2.5); });
show("float_ + float_: NOTHING + NOTHING", auto sub () { return f16(NOTHING, NOTHING); });
show("float_ + float_: NOTHING + 0.0", auto sub () { return f16(NOTHING, 0.0); });
show("float_ + float_: NOTHING + 1.5", auto sub () { return f16(NOTHING, 1.5); });
show("float_ + float_: 0.0 + NOTHING", auto sub () { return f16(0.0, NOTHING); });
show("float_ + float_: 0.0 + 0.0", auto sub () { return f16(0.0, 0.0); });
show("float_ + float_: 0.0 + 1.5", auto sub () { return f16(0.0, 1.5); });
show("float_ + float_: 1.5 + NOTHING", auto sub () { return f16(1.5, NOTHING); });
show("float_ + float_: 1.5 + 0.0", auto sub () { return f16(1.5, 0.0); });
show("float_ + float_: 1.5 + 1.5", auto sub () { return f16(1.5, 1.5); });
show("int - int: 0 - 0", auto sub () { return f17(0, 0); });
show("int - int: 0 - 3", auto sub () { return f17(0, 3); });
show("int - int: 0 - -2", auto sub () { return f17(0, -2); });
show("int - int: 3 - 0", auto sub () { return f17(3, 0); });
show("int - int: 3 - 3", auto sub () { return f17(3, 3); });
show("int - int: 3 - -2", auto sub () { return f17(3, -2); });
show("int - int: -2 - 0", auto sub () { return f17(-2, 0); });
show("int - int: -2 - 3", auto sub () { return f17(-2, 3); });
show("int - int: -2 - -2", auto sub () { return f17(-2, -2); });
show("int - int_: 0 - NOTHING", auto sub () { return f18(0, NOTHING); });
show("int - int_: 0 - 0", auto sub () { return f18(0, 0); });
show("int - int_: 0 - 3", auto sub () { return f18(0, 3); });
show("int - int_: 3 - NOTHING", auto sub () { return f18(3, NOTHING); });
show("int - int_: 3 - 0", auto sub () { return f18(3, 0); });
show("int - int_: 3 - 3", auto sub () { return f18(3, 3); });
show("int - int_: -2 - NOTHING", auto sub () { return f18(-2, NOTHING); });
show("int - int_: -2 - 0", auto sub () { return f18(-2, 0); });
show("int - int_: -2 - 3", auto sub () { return f18(-2, 3); });
show("int - float: 0 - 0.0", auto sub () { return f19(0, 0.0); });
show("int - float: 0 - 1.5", auto sub () { return f19(0, 1.5); });
show("int - float: 0 - -2.5", auto sub () { return f19(0, -2.5); });
show("int - float: 3 - 0.0", auto sub () { return f19(3, 0.0); });
show("int - float: 3 - 1.5", auto sub () { return f19(3, 1.5); });
show("int - float: 3 - -2.5", auto sub () { return f19(3, -2.5); });
show("int - float: -2 - 0.0", auto sub () { return f19(-2, 0.0); });
show("int - float: -2 - 1.5", auto sub () { return f19(-2, 1.5); });
show("int - float: -2 - -2.5", auto sub () { return f19(-2, -2.5); });
show("int - float_: 0 - NOTHING", auto sub () { return f20(0, NOTHING); });
show("int - float_: 0 - 0.0", auto sub () { return f20(0, 0.0); });
show("int - float_: 0 - 1.5", auto sub () { return f20(0, 1.5); });
show("int - float_: 3 - NOTHING", auto sub () { return f20(3, NOTHING); });
show("int - float_: 3 - 0.0", auto sub () { return f20(3, 0.0); });
show("int - float_: 3 - 1.5", auto sub () { return f20(3, 1.5); });
show("int - float_: -2 - NOTHING", auto sub () { return f20(-2, NOTHING); });
show("int - float_: -2 - 0.0", auto sub () { return f20(-2, 0.0); });
show("int - float_: -2 - 1.5", auto sub () { return f20(-2, 1.5); });
show("int_ - int: NOTHING - 0", auto sub () { return f21(NOTHING, 0); });
show("int_ - int: NOTHING - 3", auto sub () { return f21(NOTHING, 3); });
show("int_ - int: NOTHING - -2", auto sub () { return f21(NOTHING, -2); });
show("int_ - int: 0 - 0", auto sub () { return f21(0, 0); });
show("int_ - int: 0 - 3", auto sub () { return f21(0, 3); });
show("int_ - int: 0 - -2", auto sub () { return f21(0, -2); });
show("int_ - int: 3 - 0", auto sub () { return f21(3, 0); });
show("int_ - int: 3 - 3", auto sub () { return f21(3, 3); });
show("int_ - int: 3 - -2", auto sub () { return f21(3, -2); });
show("int_ - int_: NOTHING - NOTHING", auto sub () { return f22(NOTHING, NOTHING); });
show("int_ - int_: NOTHING - 0", auto sub () { return f22(NOTHING, 0); });
show("int_ - int_: NOTHING - 3", auto sub () { return f22(NOTHING, 3); });
show("int_ - int_: 0 - NOTHING", auto sub () { return f22(0, NOTHING); });
show("int_ - int_: 0 - 0", auto sub () { return f22(0, 0); });
show("int_ - int_: 0 - 3", auto sub () { return f22(0, 3); });
show("int_ - int_: 3 - NOTHING", auto sub () { return f22(3, NOTHING); });
show("int_ - int_: 3 - 0", auto sub () { return f22(3, 0); });
show("int_ - int_: 3 - 3", auto sub () { return f22(3, 3); });
show("int_ - float: NOTHING - 0.0", auto sub () { return f23(NOTHING, 0.0); });
show("int_ - float: NOTHING - 1.5", auto sub () { return f23(NOTHING, 1.5); });
show("int_ - float: NOTHING - -2.5", auto sub () { return f23(NOTHING, -2.5); });
show("int_ - float: 0 - 0.0", auto sub () { return f23(0, 0.0); });
show("int_ - float: 0 - 1.5", auto sub () { return f23(0, 1.5); });
show("int_ - float: 0 - -2.5", auto sub () { return f23(0, -2.5); });
show("int_ - float: 3 - 0.0", auto sub () { return f23(3, 0.0); });
show("int_ - float: 3 - 1.5", auto sub () { return f23(3, 1.5); });
show("int_ - float: 3 - -2.5", auto sub () { return f23(3, -2.5); });
show("int_ - float_: NOTHING - NOTHING", auto sub () { return f24(NOTHING, NOTHING); });
show("int_ - float_: NOTHING - 0.0", auto sub () { return f24(NOTHING, 0.0); });
show("int_ - float_: NOTHING - 1.5", auto sub () { return f24(NOTHING, 1.5); });
show("int_ - float_: 0 - NOTHING", auto sub () { return f24(0, NOTHING); });
show("int_ - float_: 0 - 0.0", auto sub () { return f24(0, 0.0); });
show("int_ - float_: 0 - 1.5", auto sub () { return f24(0, 1.5); });
show("int_ - float_: 3 - NOTHING", auto sub () { return f24(3, NOTHING); });
show("int_ - float_: 3 - 0.0", auto sub () { return f24(3, 0.0); });
show("int_ - float_: 3 - 1.5", auto sub () { return f24(3, 1.5); });
show("float - int: 0.0 - 0", auto sub () { return f25(0.0, 0); });
show("float - int: 0.0 - 3", auto sub () { return f25(0.0, 3); });
show("float - int: 0.0 - -2", auto sub () { return f25(0.0, -2); });
show("float - int: 1.5 - 0", auto sub () { return f25(1.5, 0); });
show("float - int: 1.5 - 3", auto sub () { return f25(1.5, 3); });
show("float - int: 1.5 - -2", auto sub () { return f25(1.5, -2); });
show("float - int: -2.5 - 0", auto sub () { return f25(-2.5, 0); });
show("float - int: -2.5 - 3", auto sub () { return f25(-2.5, 3); });
show("float - int: -2.5 - -2", auto sub () { return f25(-2.5, -2); });
show("float - int_: 0.0 - NOTHING", auto sub () { return f26(0.0, NOTHING); });
show("float - int_: 0.0 - 0", auto sub () { return f26(0.0, 0); });
show("float - int_: 0.0 - 3", auto sub () { return f26(0.0, 3); });
show("float - int_: 1.5 - NOTHING", auto sub () { return f26(1.5, NOTHING); });
show("float - int_: 1.5 - 0", auto sub () { return f26(1.5, 0); });
show("float - int_: 1.5 - 3", auto sub () { return f26(1.5, 3); });
show("float - int_: -2.5 - NOTHING", auto sub () { return f26(-2.5, NOTHING); });
show("float - int_: -2.5 - 0", auto sub () { return f26(-2.5, 0); });
show("float - int_: -2.5 - 3", auto sub () { return f26(-2.5, 3); });
show("float - float: 0.0 - 0.0", auto sub () { return f27(0.0, 0.0); });
show("float - float: 0.0 - 1.5", auto sub () { return f27(0.0, 1.5); });
show("float - float: 0.0 - -2.5", auto sub () { return f27(0.0, -2.5); });
show("float - float: 1.5 - 0.0", auto sub () { return f27(1.5, 0.0); });
show("float - float: 1.5 - 1.5", auto sub () { return f27(1.5, 1.5); });
show("float - float: 1.5 - -2.5", auto sub () { return f27(1.5, -2.5); });
show("float - float: -2.5 - 0.0", auto sub () { return f27(-2.5, 0.0); });
show("float - float: -2.5 - 1.5", auto sub () { return f27(-2.5, 1.5); });
show("float - float: -2.5 - -2.5", auto sub () { return f27(-2.5, -2.5); });
show("float - float_: 0.0 - NOTHING", auto sub () { return f28(0.0, NOTHING); });
show("float - float_: 0.0 - 0.0", auto sub () { return f28(0.0, 0.0); });
show("float - float_: 0.0 - 1.5", auto sub () { return f28(0.0, 1.5); });
show("float - float_: 1.5 - NOTHING", auto sub () { return f28(1.5, NOTHING); });
show("float - float_: 1.5 - 0.0", auto sub () { return f28(1.5, 0.0); });
show("float - float_: 1.5 - 1.5", auto sub () { return f28(1.5, 1.5); });
show("float - float_: -2.5 - NOTHING", auto sub () { return f28(-2.5, NOTHING); });
show("float - float_: -2.5 - 0.0", auto sub () { return f28(-2.5, 0.0); });
show("float - float_: -2.5 - 1.5", auto sub () { return f28(-2.5, 1.5); });
show("float_ - int: NOTHING - 0", auto sub () { return f29(NOTHING, 0); });
show("float_ - int: NOTHING - 3", auto sub () { return f29(NOTHING, 3); });
show("float_ - int: NOTHING - -2", auto sub () { return f29(NOTHING, -2); });
show("float_ - int: 0.0 - 0", auto sub () { return f29(0.0, 0); });
show("float_ - int: 0.0 - 3", auto sub () { return f29(0.0, 3); });
show("float_ - int: 0.0 - -2", auto sub () { return f29(0.0, -2); });
show("float_ - int: 1.5 - 0", auto sub () { return f29(1.5, 0); });
show("float_ - int: 1.5 - 3", auto sub () { return f29(1.5, 3); });
show("float_ - int: 1.5 - -2", auto sub () { return f29(1.5, -2); });
show("float_ - int_: NOTHING - NOTHING", auto sub () { return f30(NOTHING, NOTHING); });
show("float_ - int_: NOTHING - 0", auto sub () { return f30(NOTHING, 0); });
show("float_ - int_: NOTHING - 3", auto sub () { return f30(NOTHING, 3); });
show("float_ - int_: 0.0 - NOTHING", auto sub () { return f30(0.0, NOTHING); });
show("float_ - int_: 0.0 - 0", auto sub () { return f30(0.0, 0); });
show("float_ - int_: 0.0 - 3", auto sub () { return f30(0.0, 3); });
show("float_ - int_: 1.5 - NOTHING", auto sub () { return f30(1.5, NOTHING); });
show("float_ - int_: 1.5 - 0", auto sub () { return f30(1.5, 0); });
show("float_ - int_: 1.5 - 3", auto sub () { return f30(1.5, 3); });
show("float_ - float: NOTHING - 0.0", auto sub () { return f31(NOTHING, 0.0); });
show("float_ - float: NOTHING - 1.5", auto sub () { return f31(NOTHING, 1.5); });
show("float_ - float: NOTHING - -2.5", auto sub () { return f31(NOTHING, -2.5); });
show("float_ - float: 0.0 - 0.0", auto sub () { return f31(0.0, 0.0); });
show("float_ - float: 0.0 - 1.5", auto sub () { return f31(0.0, 1.5); });
show("float_ - float: 0.0 - -2.5", auto sub () { return f31(0.0, -2.5); });
show("float_ - float: 1.5 - 0.0", auto sub () { return f31(1.5, 0.0); });
show("float_ - float: 1.5 - 1.5", auto sub () { return f31(1.5, 1.5); });
show("float_ - float: 1.5 - -2.5", auto sub () { return f31(1.5, -2.5); });
show("float_ - float_: NOTHING - NOTHING", auto sub () { return f32(NOTHING, NOTHING); });
show("float_ - float_: NOTHING - 0.0", auto sub () { return f32(NOTHING, 0.0); });
show("float_ - float_: NOTHING - 1.5", auto sub () { return f32(NOTHING, 1.5); });
show("float_ - float_: 0.0 - NOTHING", auto sub () { return f32(0.0, NOTHING); });
show("float_ - float_: 0.0 - 0.0", auto sub () { return f32(0.0, 0.0); });
show("float_ - float_: 0.0 - 1.5", auto sub () { return f32(0.0, 1.5); });
show("float_ - float_: 1.5 - NOTHING", auto sub () { return f32(1.5, NOTHING); });
show("float_ - float_: 1.5 - 0.0", auto sub () { return f32(1.5, 0.0); });
show("float_ - float_: 1.5 - 1.5", auto sub () { return f32(1.5, 1.5); });
show("int * int: 0 * 0", auto sub () { return f33(0, 0); });
show("int * int: 0 * 3", auto sub () { return f33(0, 3); });
show("int * int: 0 * -2", auto sub () { return f33(0, -2); });
show("int * int: 3 * 0", auto sub () { return f33(3, 0); });
show("int * int: 3 * 3", auto sub () { return f33(3, 3); });
show("int * int: 3 * -2", auto sub () { return f33(3, -2); });
show("int * int: -2 * 0", auto sub () { return f33(-2, 0); });
show("int * int: -2 * 3", auto sub () { return f33(-2, 3); });
show("int * int: -2 * -2", auto sub () { return f33(-2, -2); });
show("int * int_: 0 * NOTHING", auto sub () { return f34(0, NOTHING); });
show("int * int_: 0 * 0", auto sub () { return f34(0, 0); });
show("int * int_: 0 * 3", auto sub () { return f34(0, 3); });
show("int * int_: 3 * NOTHING", auto sub () { return f34(3, NOTHING); });
show("int * int_: 3 * 0", auto sub () { return f34(3, 0); });
show("int * int_: 3 * 3", auto sub () { return f34(3, 3); });
show("int * int_: -2 * NOTHING", auto sub () { return f34(-2, NOTHING); });
show("int * int_: -2 * 0", auto sub () { return f34(-2, 0); });
show("int * int_: -2 * 3", auto sub () { return f34(-2, 3); });
show("int * float: 0 * 0.0", auto sub () { return f35(0, 0.0); });
show("int * float: 0 * 1.5", auto sub () { return f35(0, 1.5); });
show("int * float: 0 * -2.5", auto sub () { return f35(0, -2.5); });
show("int * float: 3 * 0.0", auto sub () { return f35(3, 0.0); });
show("int * float: 3 * 1.5", auto sub () { return f35(3, 1.5); });
show("int * float: 3 * -2.5", auto sub () { return f35(3, -2.5); });
show("int * float: -2 * 0.0", auto sub () { return f35(-2, 0.0); });
show("int * float: -2 * 1.5", auto sub () { return f35(-2, 1.5); });
show("int * float: -2 * -2.5", auto sub () { return f35(-2, -2.5); });
show("int * float_: 0 * NOTHING", auto sub () { return f36(0, NOTHING); });
show("int * float_: 0 * 0.0", auto sub () { return f36(0, 0.0); });
show("int * float_: 0 * 1.5", auto sub () { return f36(0, 1.5); });
show("int * float_: 3 * NOTHING", auto sub () { return f36(3, NOTHING); });
show("int * float_: 3 * 0.0", auto sub () { return f36(3, 0.0); });
show("int * float_: 3 * 1.5", auto sub () { return f36(3, 1.5); });
show("int * float_: -2 * NOTHING", auto sub () { return f36(-2, NOTHING); });
show("int * float_: -2 * 0.0", auto sub () { return f36(-2, 0.0); });
show("int * float_: -2 * 1.5", auto sub () { return f36(-2, 1.5); });
show("int_ * int: NOTHING * 0", auto sub () { return f37(NOTHING, 0); });
show("int_ * int: NOTHING * 3", auto sub () { return f37(NOTHING, 3); });
show("int_ * int: NOTHING * -2", auto sub () { return f37(NOTHING, -2); });
show("int_ * int: 0 * 0", auto sub () { return f37(0, 0); });
show("int_ * int: 0 * 3", auto sub () { return f37(0, 3); });
show("int_ * int: 0 * -2", auto sub () { return f37(0, -2); });
show("int_ * int: 3 * 0", auto sub () { return f37(3, 0); });
show("int_ * int: 3 * 3", auto sub () { return f37(3, 3); });
show("int_ * int: 3 * -2", auto sub () { return f37(3, -2); });
show("int_ * int_: NOTHING * NOTHING", auto sub () { return f38(NOTHING, NOTHING); });
show("int_ * int_: NOTHING * 0", auto sub () { return f38(NOTHING, 0); });
show("int_ * int_: NOTHING * 3", auto sub () { return f38(NOTHING, 3); });
show("int_ * int_: 0 * NOTHING", auto sub () { return f38(0, NOTHING); });
show("int_ * int_: 0 * 0", auto sub () { return f38(0, 0); });
show("int_ * int_: 0 * 3", auto sub () { return f38(0, 3); });
show("int_ * int_: 3 * NOTHING", auto sub () { return f38(3, NOTHING); });
show("int_ * int_: 3 * 0", auto sub () { return f38(3, 0); });
show("int_ * int_: 3 * 3", auto sub () { return f38(3, 3); });
show("int_ * float: NOTHING * 0.0", auto sub () { return f39(NOTHING, 0.0); });
show("int_ * float: NOTHING * 1.5", auto sub () { return f39(NOTHING, 1.5); });
show("int_ * float: NOTHING * -2.5", auto sub () { return f39(NOTHING, -2.5); });
show("int_ * float: 0 * 0.0", auto sub () { return f39(0, 0.0); });
show("int_ * float: 0 * 1.5", auto sub () { return f39(0, 1.5); });
show("int_ * float: 0 * -2.5", auto sub () { return f39(0, -2.5); });
show("int_ * float: 3 * 0.0", auto sub () { return f39(3, 0.0); });
show("int_ * float: 3 * 1.5", auto sub () { return f39(3, 1.5); });
show("int_ * float: 3 * -2.5", auto sub () { return f39(3, -2.5); });
show("int_ * float_: NOTHING * NOTHING", auto sub () { return f40(NOTHING, NOTHING); });
show("int_ * float_: NOTHING * 0.0", auto sub () { return f40(NOTHING, 0.0); });
show("int_ * float_: NOTHING * 1.5", auto sub () { return f40(NOTHING, 1.5); });
show("int_ * float_: 0 * NOTHING", auto sub () { return f40(0, NOTHING); });
show("int_ * float_: 0 * 0.0", auto sub () { return f40(0, 0.0); });
show("int_ * float_: 0 * 1.5", auto sub () { return f40(0, 1.5); });
show("int_ * float_: 3 * NOTHING", auto sub () { return f40(3, NOTHING); });
show("int_ * float_: 3 * 0.0", auto sub () { return f40(3, 0.0); });
show("int_ * float_: 3 * 1.5", auto sub () { return f40(3, 1.5); });
show("float * int: 0.0 * 0", auto sub () { return f41(0.0, 0); });
show("float * int: 0.0 * 3", auto sub () { return f41(0.0, 3); });
show("float * int: 0.0 * -2", auto sub () { return f41(0.0, -2); });
show("float * int: 1.5 * 0", auto sub () { return f41(1.5, 0); });
show("float * int: 1.5 * 3", auto sub () { return f41(1.5, 3); });
show("float * int: 1.5 * -2", auto sub () { return f41(1.5, -2); });
show("float * int: -2.5 * 0", auto sub () { return f41(-2.5, 0); });
show("float * int: -2.5 * 3", auto sub () { return f41(-2.5, 3); });
show("float * int: -2.5 * -2", auto sub () { return f41(-2.5, -2); });
show("float * int_: 0.0 * NOTHING", auto sub () { return f42(0.0, NOTHING); });
show("float * int_: 0.0 * 0", auto sub () { return f42(0.0, 0); });
show("float * int_: 0.0 * 3", auto sub () { return f42(0.0, 3); });
show("float * int_: 1.5 * NOTHING", auto sub () { return f42(1.5, NOTHING); });
show("float * int_: 1.5 * 0", auto sub () { return f42(1.5, 0); });
show("float * int_: 1.5 * 3", auto sub () { return f42(1.5, 3); });
show("float * int_: -2.5 * NOTHING", auto sub () { return f42(-2.5, NOTHING); });
show("float * int_: -2.5 * 0", auto sub () { return f42(-2.5, 0); });
show("float * int_: -2.5 * 3", auto sub () { return f42(-2.5, 3); });
show("float * float: 0.0 * 0.0", auto sub () { return f43(0.0, 0.0); });
show("float * float: 0.0 * 1.5", auto sub () { return f43(0.0, 1.5); });
show("float * float: 0.0 * -2.5", auto sub () { return f43(0.0, -2.5); });
show("float * float: 1.5 * 0.0", auto sub () { return f43(1.5, 0.0); });
show("float * float: 1.5 * 1.5", auto sub () { return f43(1.5, 1.5); });
show("float * float: 1.5 * -2.5", auto sub () { return f43(1.5, -2.5); });
show("float * float: -2.5 * 0.0", auto sub () { return f43(-2.5, 0.0); });
show("float * float: -2.5 * 1.5", auto sub () { return f43(-2.5, 1.5); });
show("float * float: -2.5 * -2.5", auto sub () { return f43(-2.5, -2.5); });
show("float * float_: 0.0 * NOTHING", auto sub () { return f44(0.0, NOTHING); });
show("float * float_: 0.0 * 0.0", auto sub () { return f44(0.0, 0.0); });
show("float * float_: 0.0 * 1.5", auto sub () { return f44(0.0, 1.5); });
show("float * float_: 1.5 * NOTHING", auto sub () { return f44(1.5, NOTHING); });
show("float * float_: 1.5 * 0.0", auto sub () { return f44(1.5, 0.0); });
show("float * float_: 1.5 * 1.5", auto sub () { return f44(1.5, 1.5); });
show("float * float_: -2.5 * NOTHING", auto sub () { return f44(-2.5, NOTHING); });
show("float * float_: -2.5 * 0.0", auto sub () { return f44(-2.5, 0.0); });
show("float * float_: -2.5 * 1.5", auto sub () { return f44(-2.5, 1.5); });
show("float_ * int: NOTHING * 0", auto sub () { return f45(NOTHING, 0); });
show("float_ * int: NOTHING * 3", auto sub () { return f45(NOTHING, 3); });
show("float_ * int: NOTHING * -2", auto sub () { return f45(NOTHING, -2); });
show("float_ * int: 0.0 * 0", auto sub () { return f45(0.0, 0); });
show("float_ * int: 0.0 * 3", auto sub () { return f45(0.0, 3); });
show("float_ * int: 0.0 * -2", auto sub () { return f45(0.0, -2); });
show("float_ * int: 1.5 * 0", auto sub () { return f45(1.5, 0); });
show("float_ * int: 1.5 * 3", auto sub () { return f45(1.5, 3); });
show("float_ * int: 1.5 * -2", auto sub () { return f45(1.5, -2); });
show("float_ * int_: NOTHING * NOTHING", auto sub () { return f46(NOTHING, NOTHING); });
show("float_ * int_: NOTHING * 0", auto sub () { return f46(NOTHING, 0); });
show("float_ * int_: NOTHING * 3", auto sub () { return f46(NOTHING, 3); });
show("float_ * int_: 0.0 * NOTHING", auto sub () { return f46(0.0, NOTHING); });
show("float_ * int_: 0.0 * 0", auto sub () { return f46(0.0, 0); });
show("float_ * int_: 0.0 * 3", auto sub () { return f46(0.0, 3); });
show("float_ * int_: 1.5 * NOTHING", auto sub () { return f46(1.5, NOTHING); });
show("float_ * int_: 1.5 * 0", auto sub () { return f46(1.5, 0); });
show("float_ * int_: 1.5 * 3", auto sub () { return f46(1.5, 3); });
show("float_ * float: NOTHING * 0.0", auto sub () { return f47(NOTHING, 0.0); });
show("float_ * float: NOTHING * 1.5", auto sub () { return f47(NOTHING, 1.5); });
show("float_ * float: NOTHING * -2.5", auto sub () { return f47(NOTHING, -2.5); });
show("float_ * float: 0.0 * 0.0", auto sub () { return f47(0.0, 0.0); });
show("float_ * float: 0.0 * 1.5", auto sub () { return f47(0.0, 1.5); });
show("float_ * float: 0.0 * -2.5", auto sub () { return f47(0.0, -2.5); });
show("float_ * float: 1.5 * 0.0", auto sub () { return f47(1.5, 0.0); });
show("float_ * float: 1.5 * 1.5", auto sub () { return f47(1.5, 1.5); });
show("float_ * float: 1.5 * -2.5", auto sub () { return f47(1.5, -2.5); });
show("float_ * float_: NOTHING * NOTHING", auto sub () { return f48(NOTHING, NOTHING); });
show("float_ * float_: NOTHING * 0.0", auto sub () { return f48(NOTHING, 0.0); });
show("float_ * float_: NOTHING * 1.5", auto sub () { return f48(NOTHING, 1.5); });
show("float_ * float_: 0.0 * NOTHING", auto sub () { return f48(0.0, NOTHING); });
show("float_ * float_: 0.0 * 0.0", auto sub () { return f48(0.0, 0.0); });
show("float_ * float_: 0.0 * 1.5", auto sub () { return f48(0.0, 1.5); });
show("float_ * float_: 1.5 * NOTHING", auto sub () { return f48(1.5, NOTHING); });
show("float_ * float_: 1.5 * 0.0", auto sub () { return f48(1.5, 0.0); });
show("float_ * float_: 1.5 * 1.5", auto sub () { return f48(1.5, 1.5); });
show("int / int: 0 / 0", auto sub () { return f49(0, 0); });
show("int / int: 0 / 3", auto sub () { return f49(0, 3); });
show("int / int: 0 / -2", auto sub () { return f49(0, -2); });
show("int / int: 3 / 0", auto sub () { return f49(3, 0); });
show("int / int: 3 / 3", auto sub () { return f49(3, 3); });
show("int / int: 3 / -2", auto sub () { return f49(3, -2); });
show("int / int: -2 / 0", auto sub () { return f49(-2, 0); });
show("int / int: -2 / 3", auto sub () { return f49(-2, 3); });
show("int / int: -2 / -2", auto sub () { return f49(-2, -2); });
show("int / int_: 0 / NOTHING", auto sub () { return f50(0, NOTHING); });
show("int / int_: 0 / 0", auto sub () { return f50(0, 0); });
show("int / int_: 0 / 3", auto sub () { return f50(0, 3); });
show("int / int_: 3 / NOTHING", auto sub () { return f50(3, NOTHING); });
show("int / int_: 3 / 0", auto sub () { return f50(3, 0); });
show("int / int_: 3 / 3", auto sub () { return f50(3, 3); });
show("int / int_: -2 / NOTHING", auto sub () { return f50(-2, NOTHING); });
show("int / int_: -2 / 0", auto sub () { return f50(-2, 0); });
show("int / int_: -2 / 3", auto sub () { return f50(-2, 3); });
show("int / float: 0 / 0.0", auto sub () { return f51(0, 0.0); });
show("int / float: 0 / 1.5", auto sub () { return f51(0, 1.5); });
show("int / float: 0 / -2.5", auto sub () { return f51(0, -2.5); });
show("int / float: 3 / 0.0", auto sub () { return f51(3, 0.0); });
show("int / float: 3 / 1.5", auto sub () { return f51(3, 1.5); });
show("int / float: 3 / -2.5", auto sub () { return f51(3, -2.5); });
show("int / float: -2 / 0.0", auto sub () { return f51(-2, 0.0); });
show("int / float: -2 / 1.5", auto sub () { return f51(-2, 1.5); });
show("int / float: -2 / -2.5", auto sub () { return f51(-2, -2.5); });
show("int / float_: 0 / NOTHING", auto sub () { return f52(0, NOTHING); });
show("int / float_: 0 / 0.0", auto sub () { return f52(0, 0.0); });
show("int / float_: 0 / 1.5", auto sub () { return f52(0, 1.5); });
show("int / float_: 3 / NOTHING", auto sub () { return f52(3, NOTHING); });
show("int / float_: 3 / 0.0", auto sub () { return f52(3, 0.0); });
show("int / float_: 3 / 1.5", auto sub () { return f52(3, 1.5); });
show("int / float_: -2 / NOTHING", auto sub () { return f52(-2, NOTHING); });
show("int / float_: -2 / 0.0", auto sub () { return f52(-2, 0.0); });
show("int / float_: -2 / 1.5", auto sub () { return f52(-2, 1.5); });
show("int_ / int: NOTHING / 0", auto sub () { return f53(NOTHING, 0); });
show("int_ / int: NOTHING / 3", auto sub () { return f53(NOTHING, 3); });
show("int_ / int: NOTHING / -2", auto sub () { return f53(NOTHING, -2); });
show("int_ / int: 0 / 0", auto sub () { return f53(0, 0); });
show("int_ / int: 0 / 3", auto sub () { return f53(0, 3); });
show("int_ / int: 0 / -2", auto sub () { return f53(0, -2); });
show("int_ / int: 3 / 0", auto sub () { return f53(3, 0); });
show("int_ / int: 3 / 3", auto sub () { return f53(3, 3); });
show("int_ / int: 3 / -2", auto sub () { return f53(3, -2); });
show("int_ / int_: NOTHING / NOTHING", auto sub () { return f54(NOTHING, NOTHING); });
show("int_ / int_: NOTHING / 0", auto sub () { return f54(NOTHING, 0); });
show("int_ / int_: NOTHING / 3", auto sub () { return f54(NOTHING, 3); });
show("int_ / int_: 0 / NOTHING", auto sub () { return f54(0, NOTHING); });
show("int_ / int_: 0 / 0", auto sub () { return f54(0, 0); });
show("int_ / int_: 0 / 3", auto sub () { return f54(0, 3); });
show("int_ / int_: 3 / NOTHING", auto sub () { return f54(3, NOTHING); });
show("int_ / int_: 3 / 0", auto sub () { return f54(3, 0); });
show("int_ / int_: 3 / 3", auto sub () { return f54(3, 3); });
show("int_ / float: NOTHING / 0.0", auto sub () { return f55(NOTHING, 0.0); });
show("int_ / float: NOTHING / 1.5", auto sub () { return f55(NOTHING, 1.5); });
show("int_ / float: NOTHING / -2.5", auto sub () { return f55(NOTHING, -2.5); });
show("int_ / float: 0 / 0.0", auto sub () { return f55(0, 0.0); });
show("int_ / float: 0 / 1.5", auto sub () { return f55(0, 1.5); });
show("int_ / float: 0 / -2.5", auto sub () { return f55(0, -2.5); });
show("int_ / float: 3 / 0.0", auto sub () { return f55(3, 0.0); });
show("int_ / float: 3 / 1.5", auto sub () { return f55(3, 1.5); });
show("int_ / float: 3 / -2.5", auto sub () { return f55(3, -2.5); });
show("int_ / float_: NOTHING / NOTHING", auto sub () { return f56(NOTHING, NOTHING); });
show("int_ / float_: NOTHING / 0.0", auto sub () { return f56(NOTHING, 0.0); });
show("int_ / float_: NOTHING / 1.5", auto sub () { return f56(NOTHING, 1.5); });
show("int_ / float_: 0 / NOTHING", auto sub () { return f56(0, NOTHING); });
show("int_ / float_: 0 / 0.0", auto sub () { return f56(0, 0.0); });
show("int_ / float_: 0 / 1.5", auto sub () { return f56(0, 1.5); });
show("int_ / float_: 3 / NOTHING", auto sub () { return f56(3, NOTHING); });
show("int_ / float_: 3 / 0.0", auto sub () { return f56(3, 0.0); });
show("int_ / float_: 3 / 1.5", auto sub () { return f56(3, 1.5); });
show("float / int: 0.0 / 0", auto sub () { return f57(0.0, 0); });
show("float / int: 0.0 / 3", auto sub () { return f57(0.0, 3); });
show("float / int: 0.0 / -2", auto sub () { return f57(0.0, -2); });
show("float / int: 1.5 / 0", auto sub () { return f57(1.5, 0); });
show("float / int: 1.5 / 3", auto sub () { return f57(1.5, 3); });
show("float / int: 1.5 / -2", auto sub () { return f57(1.5, -2); });
show("float / int: -2.5 / 0", auto sub () { return f57(-2.5, 0); });
show("float / int: -2.5 / 3", auto sub () { return f57(-2.5, 3); });
show("float / int: -2.5 / -2", auto sub () { return f57(-2.5, -2); });
show("float / int_: 0.0 / NOTHING", auto sub () { return f58(0.0, NOTHING); });
show("float / int_: 0.0 / 0", auto sub () { return f58(0.0, 0); });
show("float / int_: 0.0 / 3", auto sub () { return f58(0.0, 3); });
show("float / int_: 1.5 / NOTHING", auto sub () { return f58(1.5, NOTHING); });
show("float / int_: 1.5 / 0", auto sub () { return f58(1.5, 0); });
show("float / int_: 1.5 / 3", auto sub () { return f58(1.5, 3); });
show("float / int_: -2.5 / NOTHING", auto sub () { return f58(-2.5, NOTHING); });
show("float / int_: -2.5 / 0", auto sub () { return f58(-2.5, 0); });
show("float / int_: -2.5 / 3", auto sub () { return f58(-2.5, 3); });
show("float / float: 0.0 / 0.0", auto sub () { return f59(0.0, 0.0); });
show("float / float: 0.0 / 1.5", auto sub () { return f59(0.0, 1.5); });
show("float / float: 0.0 / -2.5", auto sub () { return f59(0.0, -2.5); });
show("float / float: 1.5 / 0.0", auto sub () { return f59(1.5, 0.0); });
show("float / float: 1.5 / 1.5", auto sub () { return f59(1.5, 1.5); });
show("float / float: 1.5 / -2.5", auto sub () { return f59(1.5, -2.5); });
show("float / float: -2.5 / 0.0", auto sub () { return f59(-2.5, 0.0); });
show("float / float: -2.5 / 1.5", auto sub () { return f59(-2.5, 1.5); });
show("float / float: -2.5 / -2.5", auto sub () { return f59(-2.5, -2.5); });
show("float / float_: 0.0 / NOTHING", auto sub () { return f60(0.0, NOTHING); });
show("float / float_: 0.0 / 0.0", auto sub () { return f60(0.0, 0.0); });
show("float / float_: 0.0 / 1.5", auto sub () { return f60(0.0, 1.5); });
show("float / float_: 1.5 / NOTHING", auto sub () { return f60(1.5, NOTHING); });
show("float / float_: 1.5 / 0.0", auto sub () { return f60(1.5, 0.0); });
show("float / float_: 1.5 / 1.5", auto sub () { return f60(1.5, 1.5); });
show("float / float_: -2.5 / NOTHING", auto sub () { return f60(-2.5, NOTHING); });
show("float / float_: -2.5 / 0.0", auto sub () { return f60(-2.5, 0.0); });
show("float / float_: -2.5 / 1.5", auto sub () { return f60(-2.5, 1.5); });
show("float_ / int: NOTHING / 0", auto sub () { return f61(NOTHING, 0); });
show("float_ / int: NOTHING / 3", auto sub () { return f61(NOTHING, 3); });
show("float_ / int: NOTHING / -2", auto sub () { return f61(NOTHING, -2); });
show("float_ / int: 0.0 / 0", auto sub () { return f61(0.0, 0); });
show("float_ / int: 0.0 / 3", auto sub () { return f61(0.0, 3); });
show("float_ / int: 0.0 / -2", auto sub () { return f61(0.0, -2); });
show("float_ / int: 1.5 / 0", auto sub () { return f61(1.5, 0); });
show("float_ / int: 1.5 / 3", auto sub () { return f61(1.5, 3); });
show("float_ / int: 1.5 / -2", auto sub () { return f61(1.5, -2); });
show("float_ / int_: NOTHING / NOTHING", auto sub () { return f62(NOTHING, NOTHING); });
show("float_ / int_: NOTHING / 0", auto sub () { return f62(NOTHING, 0); });
show("float_ / int_: NOTHING / 3", auto sub () { return f62(NOTHING, 3); });
show("float_ / int_: 0.0 / NOTHING", auto sub () { return f62(0.0, NOTHING); });
show("float_ / int_: 0.0 / 0", auto sub () { return f62(0.0, 0); });
show("float_ / int_: 0.0 / 3", auto sub () { return f62(0.0, 3); });
show("float_ / int_: 1.5 / NOTHING", auto sub () { return f62(1.5, NOTHING); });
show("float_ / int_: 1.5 / 0", auto sub () { return f62(1.5, 0); });
show("float_ / int_: 1.5 / 3", auto sub () { return f62(1.5, 3); });
show("float_ / float: NOTHING / 0.0", auto sub () { return f63(NOTHING, 0.0); });
show("float_ / float: NOTHING / 1.5", auto sub () { return f63(NOTHING, 1.5); });
show("float_ / float: NOTHING / -2.5", auto sub () { return f63(NOTHING, -2.5); });
show("float_ / float: 0.0 / 0.0", auto sub () { return f63(0.0, 0.0); });
show("float_ / float: 0.0 / 1.5", auto sub () { return f63(0.0, 1.5); });
show("float_ / float: 0.0 / -2.5", auto sub () { return f63(0.0, -2.5); });
show("float_ / float: 1.5 / 0.0", auto sub () { return f63(1.5, 0.0); });
show("float_ / float: 1.5 / 1.5", auto sub () { return f63(1.5, 1.5); });
show("float_ / float: 1.5 / -2.5", auto sub () { return f63(1.5, -2.5); });
show("float_ / float_: NOTHING / NOTHING", auto sub () { return f64(NOTHING, NOTHING); });
show("float_ / float_: NOTHING / 0.0", auto sub () { return f64(NOTHING, 0.0); });
show("float_ / float_: NOTHING / 1.5", auto sub () { return f64(NOTHING, 1.5); });
show("float_ / float_: 0.0 / NOTHING", auto sub () { return f64(0.0, NOTHING); });
show("float_ / float_: 0.0 / 0.0", auto sub () { return f64(0.0, 0.0); });
show("float_ / float_: 0.0 / 1.5", auto sub () { return f64(0.0, 1.5); });
show("float_ / float_: 1.5 / NOTHING", auto sub () { return f64(1.5, NOTHING); });
show("float_ / float_: 1.5 / 0.0", auto sub () { return f64(1.5, 0.0); });
show("float_ / float_: 1.5 / 1.5", auto sub () { return f64(1.5, 1.5); });
show("int % int: 0 % 0", auto sub () { return f65(0, 0); });
show("int % int: 0 % 3", auto sub () { return f65(0, 3); });
show("int % int: 0 % -2", auto sub () { return f65(0, -2); });
show("int % int: 3 % 0", auto sub () { return f65(3, 0); });
show("int % int: 3 % 3", auto sub () { return f65(3, 3); });
show("int % int: 3 % -2", auto sub () { return f65(3, -2); });
show("int % int: -2 % 0", auto sub () { return f65(-2, 0); });
show("int % int: -2 % 3", auto sub () { return f65(-2, 3); });
show("int % int: -2 % -2", auto sub () { return f65(-2, -2); });
show("int % int_: 0 % NOTHING", auto sub () { return f66(0, NOTHING); });
show("int % int_: 0 % 0", auto sub () { return f66(0, 0); });
show("int % int_: 0 % 3", auto sub () { return f66(0, 3); });
show("int % int_: 3 % NOTHING", auto sub () { return f66(3, NOTHING); });
show("int % int_: 3 % 0", auto sub () { return f66(3, 0); });
show("int % int_: 3 % 3", auto sub () { return f66(3, 3); });
show("int % int_: -2 % NOTHING", auto sub () { return f66(-2, NOTHING); });
show("int % int_: -2 % 0", auto sub () { return f66(-2, 0); });
show("int % int_: -2 % 3", auto sub () { return f66(-2, 3); });
show("int_ % int: NOTHING % 0", auto sub () { return f67(NOTHING, 0); });
show("int_ % int: NOTHING % 3", auto sub () { return f67(NOTHING, 3); });
show("int_ % int: NOTHING % -2", auto sub () { return f67(NOTHING, -2); });
show("int_ % int: 0 % 0", auto sub () { return f67(0, 0); });
show("int_ % int: 0 % 3", auto sub () { return f67(0, 3); });
show("int_ % int: 0 % -2", auto sub () { return f67(0, -2); });
show("int_ % int: 3 % 0", auto sub () { return f67(3, 0); });
show("int_ % int: 3 % 3", auto sub () { return f67(3, 3); });
show("int_ % int: 3 % -2", auto sub () { return f67(3, -2); });
show("int_ % int_: NOTHING % NOTHING", auto sub () { return f68(NOTHING, NOTHING); });
show("int_ % int_: NOTHING % 0", auto sub () { return f68(NOTHING, 0); });
show("int_ % int_: NOTHING % 3", auto sub () { return f68(NOTHING, 3); });
show("int_ % int_: 0 % NOTHING", auto sub () { return f68(0, NOTHING); });
show("int_ % int_: 0 % 0", auto sub () { return f68(0, 0); });
show("int_ % int_: 0 % 3", auto sub () { return f68(0, 3); });
show("int_ % int_: 3 % NOTHING", auto sub () { return f68(3, NOTHING); });
show("int_ % int_: 3 % 0", auto sub () { return f68(3, 0); });
show("int_ % int_: 3 % 3", auto sub () { return f68(3, 3); });
show("int & int: 0 & 0", auto sub () { return f69(0, 0); });
show("int & int: 0 & 3", auto sub () { return f69(0, 3); });
show("int & int: 0 & -2", auto sub () { return f69(0, -2); });
show("int & int: 3 & 0", auto sub () { return f69(3, 0); });
show("int & int: 3 & 3", auto sub () { return f69(3, 3); });
show("int & int: 3 & -2", auto sub () { return f69(3, -2); });
show("int & int: -2 & 0", auto sub () { return f69(-2, 0); });
show("int & int: -2 & 3", auto sub () { return f69(-2, 3); });
show("int & int: -2 & -2", auto sub () { return f69(-2, -2); });
show("int & int_: 0 & NOTHING", auto sub () { return f70(0, NOTHING); });
show("int & int_: 0 & 0", auto sub () { return f70(0, 0); });
show("int & int_: 0 & 3", auto sub () { return f70(0, 3); });
show("int & int_: 3 & NOTHING", auto sub () { return f70(3, NOTHING); });
show("int & int_: 3 & 0", auto sub () { return f70(3, 0); });
show("int & int_: 3 & 3", auto sub () { return f70(3, 3); });
show("int & int_: -2 & NOTHING", auto sub () { return f70(-2, NOTHING); });
show("int & int_: -2 & 0", auto sub () { return f70(-2, 0); });
show("int & int_: -2 & 3", auto sub () { return f70(-2, 3); });
show("int_ & int: NOTHING & 0", auto sub () { return f71(NOTHING, 0); });
show("int_ & int: NOTHING & 3", auto sub () { return f71(NOTHING, 3); });
show("int_ & int: NOTHING & -2", auto sub () { return f71(NOTHING, -2); });
show("int_ & int: 0 & 0", auto sub () { return f71(0, 0); });
show("int_ & int: 0 & 3", auto sub () { return f71(0, 3); });
show("int_ & int: 0 & -2", auto sub () { return f71(0, -2); });
show("int_ & int: 3 & 0", auto sub () { return f71(3, 0); });
show("int_ & int: 3 & 3", auto sub () { return f71(3, 3); });
show("int_ & int: 3 & -2", auto sub () { return f71(3, -2); });
show("int_ & int_: NOTHING & NOTHING", auto sub () { return f72(NOTHING, NOTHING); });
show("int_ & int_: NOTHING & 0", auto sub () { return f72(NOTHING, 0); });
show("int_ & int_: NOTHING & 3", auto sub () { return f72(NOTHING, 3); });
show("int_ & int_: 0 & NOTHING", auto sub () { return f72(0, NOTHING); });
show("int_ & int_: 0 & 0", auto sub () { return f72(0, 0); });
show("int_ & int_: 0 & 3", auto sub () { return f72(0, 3); });
show("int_ & int_: 3 & NOTHING", auto sub () { return f72(3, NOTHING); });
show("int_ & int_: 3 & 0", auto sub () { return f72(3, 0); });
show("int_ & int_: 3 & 3", auto sub () { return f72(3, 3); });
show("int | int: 0 | 0", auto sub () { return f73(0, 0); });
show("int | int: 0 | 3", auto sub () { return f73(0, 3); });
show("int | int: 0 | -2", auto sub () { return f73(0, -2); });
show("int | int: 3 | 0", auto sub () { return f73(3, 0); });
show("int | int: 3 | 3", auto sub () { return f73(3, 3); });
show("int | int: 3 | -2", auto sub () { return f73(3, -2); });
show("int | int: -2 | 0", auto sub () { return f73(-2, 0); });
show("int | int: -2 | 3", auto sub () { return f73(-2, 3); });
show("int | int: -2 | -2", auto sub () { return f73(-2, -2); });
show("int | int_: 0 | NOTHING", auto sub () { return f74(0, NOTHING); });
show("int | int_: 0 | 0", auto sub () { return f74(0, 0); });
show("int | int_: 0 | 3", auto sub () { return f74(0, 3); });
show("int | int_: 3 | NOTHING", auto sub () { return f74(3, NOTHING); });
show("int | int_: 3 | 0", auto sub () { return f74(3, 0); });
show("int | int_: 3 | 3", auto sub () { return f74(3, 3); });
show("int | int_: -2 | NOTHING", auto sub () { return f74(-2, NOTHING); });
show("int | int_: -2 | 0", auto sub () { return f74(-2, 0); });
show("int | int_: -2 | 3", auto sub () { return f74(-2, 3); });
show("int_ | int: NOTHING | 0", auto sub () { return f75(NOTHING, 0); });
show("int_ | int: NOTHING | 3", auto sub () { return f75(NOTHING, 3); });
show("int_ | int: NOTHING | -2", auto sub () { return f75(NOTHING, -2); });
show("int_ | int: 0 | 0", auto sub () { return f75(0, 0); });
show("int_ | int: 0 | 3", auto sub () { return f75(0, 3); });
show("int_ | int: 0 | -2", auto sub () { return f75(0, -2); });
show("int_ | int: 3 | 0", auto sub () { return f75(3, 0); });
show("int_ | int: 3 | 3", auto sub () { return f75(3, 3); });
show("int_ | int: 3 | -2", auto sub () { return f75(3, -2); });
show("int_ | int_: NOTHING | NOTHING", auto sub () { return f76(NOTHING, NOTHING); });
show("int_ | int_: NOTHING | 0", auto sub () { return f76(NOTHING, 0); });
show("int_ | int_: NOTHING | 3", auto sub () { return f76(NOTHING, 3); });
show("int_ | int_: 0 | NOTHING", auto sub () { return f76(0, NOTHING); });
show("int_ | int_: 0 | 0", auto sub () { return f76(0, 0); });
show("int_ | int_: 0 | 3", auto sub () { return f76(0, 3); });
show("int_ | int_: 3 | NOTHING", auto sub () { return f76(3, NOTHING); });
show("int_ | int_: 3 | 0", auto sub () { return f76(3, 0); });
show("int_ | int_: 3 | 3", auto sub () { return f76(3, 3); });
show("int ^ int: 0 ^ 0", auto sub () { return f77(0, 0); });
show("int ^ int: 0 ^ 3", auto sub () { return f77(0, 3); });
show("int ^ int: 0 ^ -2", auto sub () { return f77(0, -2); });
show("int ^ int: 3 ^ 0", auto sub () { return f77(3, 0); });
show("int ^ int: 3 ^ 3", auto sub () { return f77(3, 3); });
show("int ^ int: 3 ^ -2", auto sub () { return f77(3, -2); });
show("int ^ int: -2 ^ 0", auto sub () { return f77(-2, 0); });
show("int ^ int: -2 ^ 3", auto sub () { return f77(-2, 3); });
show("int ^ int: -2 ^ -2", auto sub () { return f77(-2, -2); });
show("int ^ int_: 0 ^ NOTHING", auto sub () { return f78(0, NOTHING); });
show("int ^ int_: 0 ^ 0", auto sub () { return f78(0, 0); });
show("int ^ int_: 0 ^ 3", auto sub () { return f78(0, 3); });
show("int ^ int_: 3 ^ NOTHING", auto sub () { return f78(3, NOTHING); });
show("int ^ int_: 3 ^ 0", auto sub () { return f78(3, 0); });
show("int ^ int_: 3 ^ 3", auto sub () { return f78(3, 3); });
show("int ^ int_: -2 ^ NOTHING", auto sub () { return f78(-2, NOTHING); });
show("int ^ int_: -2 ^ 0", auto sub () { return f78(-2, 0); });
show("int ^ int_: -2 ^ 3", auto sub () { return f78(-2, 3); });
show("int_ ^ int: NOTHING ^ 0", auto sub () { return f79(NOTHING, 0); });
show("int_ ^ int: NOTHING ^ 3", auto sub () { return f79(NOTHING, 3); });
show("int_ ^ int: NOTHING ^ -2", auto sub () { return f79(NOTHING, -2); });
show("int_ ^ int: 0 ^ 0", auto sub () { return f79(0, 0); });
show("int_ ^ int: 0 ^ 3", auto sub () { return f79(0, 3); });
show("int_ ^ int: 0 ^ -2", auto sub () { return f79(0, -2); });
show("int_ ^ int: 3 ^ 0", auto sub () { return f79(3, 0); });
show("int_ ^ int: 3 ^ 3", auto sub () { return f79(3, 3); });
show("int_ ^ int: 3 ^ -2", auto sub () { return f79(3, -2); });
show("int_ ^ int_: NOTHING ^ NOTHING", auto sub () { return f80(NOTHING, NOTHING); });
show("int_ ^ int_: NOTHING ^ 0", auto sub () { return f80(NOTHING, 0); });
show("int_ ^ int_: NOTHING ^ 3", auto sub () { return f80(NOTHING, 3); });
show("int_ ^ int_: 0 ^ NOTHING", auto sub () { return f80(0, NOTHING); });
show("int_ ^ int_: 0 ^ 0", auto sub () { return f80(0, 0); });
show("int_ ^ int_: 0 ^ 3", auto sub () { return f80(0, 3); });
show("int_ ^ int_: 3 ^ NOTHING", auto sub () { return f80(3, NOTHING); });
show("int_ ^ int_: 3 ^ 0", auto sub () { return f80(3, 0); });
show("int_ ^ int_: 3 ^ 3", auto sub () { return f80(3, 3); });
show("int << int: 0 << 0", auto sub () { return f81(0, 0); });
show("int << int: 0 << 3", auto sub () { return f81(0, 3); });
show("int << int: 0 << -2", auto sub () { return f81(0, -2); });
show("int << int: 3 << 0", auto sub () { return f81(3, 0); });
show("int << int: 3 << 3", auto sub () { return f81(3, 3); });
show("int << int: 3 << -2", auto sub () { return f81(3, -2); });
show("int << int: -2 << 0", auto sub () { return f81(-2, 0); });
show("int << int: -2 << 3", auto sub () { return f81(-2, 3); });
show("int << int: -2 << -2", auto sub () { return f81(-2, -2); });
show("int << int_: 0 << NOTHING", auto sub () { return f82(0, NOTHING); });
show("int << int_: 0 << 0", auto sub () { return f82(0, 0); });
show("int << int_: 0 << 3", auto sub () { return f82(0, 3); });
show("int << int_: 3 << NOTHING", auto sub () { return f82(3, NOTHING); });
show("int << int_: 3 << 0", auto sub () { return f82(3, 0); });
show("int << int_: 3 << 3", auto sub () { return f82(3, 3); });
show("int << int_: -2 << NOTHING", auto sub () { return f82(-2, NOTHING); });
show("int << int_: -2 << 0", auto sub () { return f82(-2, 0); });
show("int << int_: -2 << 3", auto sub () { return f82(-2, 3); });
show("int_ << int: NOTHING << 0", auto sub () { return f83(NOTHING, 0); });
show("int_ << int: NOTHING << 3", auto sub () { return f83(NOTHING, 3); });
show("int_ << int: NOTHING << -2", auto sub () { return f83(NOTHING, -2); });
show("int_ << int: 0 << 0", auto sub () { return f83(0, 0); });
show("int_ << int: 0 << 3", auto sub () { return f83(0, 3); });
show("int_ << int: 0 << -2", auto sub () { return f83(0, -2); });
show("int_ << int: 3 << 0", auto sub () { return f83(3, 0); });
show("int_ << int: 3 << 3", auto sub () { return f83(3, 3); });
show("int_ << int: 3 << -2", auto sub () { return f83(3, -2); });
show("int_ << int_: NOTHING << NOTHING", auto sub () { return f84(NOTHING, NOTHING); });
show("int_ << int_: NOTHING << 0", auto sub () { return f84(NOTHING, 0); });
show("int_ << int_: NOTHING << 3", auto sub () { return f84(NOTHING, 3); });
show("int_ << int_: 0 << NOTHING", auto sub () { return f84(0, NOTHING); });
show("int_ << int_: 0 << 0", auto sub () { return f84(0, 0); });
show("int_ << int_: 0 << 3", auto sub () { return f84(0, 3); });
show("int_ << int_: 3 << NOTHING", auto sub () { return f84(3, NOTHING); });
show("int_ << int_: 3 << 0", auto sub () { return f84(3, 0); });
show("int_ << int_: 3 << 3", auto sub () { return f84(3, 3); });
show("int >> int: 0 >> 0", auto sub () { return f85(0, 0); });
show("int >> int: 0 >> 3", auto sub () { return f85(0, 3); });
show("int >> int: 0 >> -2", auto sub () { return f85(0, -2); });
show("int >> int: 3 >> 0", auto sub () { return f85(3, 0); });
show("int >> int: 3 >> 3", auto sub () { return f85(3, 3); });
show("int >> int: 3 >> -2", auto sub () { return f85(3, -2); });
show("int >> int: -2 >> 0", auto sub () { return f85(-2, 0); });
show("int >> int: -2 >> 3", auto sub () { return f85(-2, 3); });
show("int >> int: -2 >> -2", auto sub () { return f85(-2, -2); });
show("int >> int_: 0 >> NOTHING", auto sub () { return f86(0, NOTHING); });
show("int >> int_: 0 >> 0", auto sub () { return f86(0, 0); });
show("int >> int_: 0 >> 3", auto sub () { return f86(0, 3); });
show("int >> int_: 3 >> NOTHING", auto sub () { return f86(3, NOTHING); });
show("int >> int_: 3 >> 0", auto sub () { return f86(3, 0); });
show("int >> int_: 3 >> 3", auto sub () { return f86(3, 3); });
show("int >> int_: -2 >> NOTHING", auto sub () { return f86(-2, NOTHING); });
show("int >> int_: -2 >> 0", auto sub () { return f86(-2, 0); });
show("int >> int_: -2 >> 3", auto sub () { return f86(-2, 3); });
show("int_ >> int: NOTHING >> 0", auto sub () { return f87(NOTHING, 0); });
show("int_ >> int: NOTHING >> 3", auto sub () { return f87(NOTHING, 3); });
show("int_ >> int: NOTHING >> -2", auto sub () { return f87(NOTHING, -2); });
show("int_ >> int: 0 >> 0", auto sub () { return f87(0, 0); });
show("int_ >> int: 0 >> 3", auto sub () { return f87(0, 3); });
show("int_ >> int: 0 >> -2", auto sub () { return f87(0, -2); });
show("int_ >> int: 3 >> 0", auto sub () { return f87(3, 0); });
show("int_ >> int: 3 >> 3", auto sub () { return f87(3, 3); });
show("int_ >> int: 3 >> -2", auto sub () { return f87(3, -2); });
show("int_ >> int_: NOTHING >> NOTHING", auto sub () { return f88(NOTHING, NOTHING); });
show("int_ >> int_: NOTHING >> 0", auto sub () { return f88(NOTHING, 0); });
show("int_ >> int_: NOTHING >> 3", auto sub () { return f88(NOTHING, 3); });
show("int_ >> int_: 0 >> NOTHING", auto sub () { return f88(0, NOTHING); });
show("int_ >> int_: 0 >> 0", auto sub () { return f88(0, 0); });
show("int_ >> int_: 0 >> 3", auto sub () { return f88(0, 3); });
show("int_ >> int_: 3 >> NOTHING", auto sub () { return f88(3, NOTHING); });
show("int_ >> int_: 3 >> 0", auto sub () { return f88(3, 0); });
show("int_ >> int_: 3 >> 3", auto sub () { return f88(3, 3); });
