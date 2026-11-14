%modern
%disable-warning invalid-operation

auto sub f1(int a, int b) { a += b; return a; }
auto sub f2(int a, *int b) { a += b; return a; }
auto sub f3(*int a, int b) { a += b; return a; }
auto sub f4(*int a, *int b) { a += b; return a; }
auto sub f5(float a, int b) { a += b; return a; }
auto sub f6(float a, *int b) { a += b; return a; }
auto sub f7(float a, float b) { a += b; return a; }
auto sub f8(float a, *float b) { a += b; return a; }
auto sub f9(*float a, int b) { a += b; return a; }
auto sub f10(*float a, *int b) { a += b; return a; }
auto sub f11(*float a, float b) { a += b; return a; }
auto sub f12(*float a, *float b) { a += b; return a; }
auto sub f13(int a, int b) { a -= b; return a; }
auto sub f14(int a, *int b) { a -= b; return a; }
auto sub f15(*int a, int b) { a -= b; return a; }
auto sub f16(*int a, *int b) { a -= b; return a; }
auto sub f17(float a, int b) { a -= b; return a; }
auto sub f18(float a, *int b) { a -= b; return a; }
auto sub f19(float a, float b) { a -= b; return a; }
auto sub f20(float a, *float b) { a -= b; return a; }
auto sub f21(*float a, int b) { a -= b; return a; }
auto sub f22(*float a, *int b) { a -= b; return a; }
auto sub f23(*float a, float b) { a -= b; return a; }
auto sub f24(*float a, *float b) { a -= b; return a; }
auto sub f25(int a, int b) { a *= b; return a; }
auto sub f26(int a, *int b) { a *= b; return a; }
auto sub f27(*int a, int b) { a *= b; return a; }
auto sub f28(*int a, *int b) { a *= b; return a; }
auto sub f29(float a, int b) { a *= b; return a; }
auto sub f30(float a, *int b) { a *= b; return a; }
auto sub f31(float a, float b) { a *= b; return a; }
auto sub f32(float a, *float b) { a *= b; return a; }
auto sub f33(*float a, int b) { a *= b; return a; }
auto sub f34(*float a, *int b) { a *= b; return a; }
auto sub f35(*float a, float b) { a *= b; return a; }
auto sub f36(*float a, *float b) { a *= b; return a; }
auto sub f37(int a, int b) { a /= b; return a; }
auto sub f38(int a, *int b) { a /= b; return a; }
auto sub f39(*int a, int b) { a /= b; return a; }
auto sub f40(*int a, *int b) { a /= b; return a; }
auto sub f41(float a, int b) { a /= b; return a; }
auto sub f42(float a, *int b) { a /= b; return a; }
auto sub f43(float a, float b) { a /= b; return a; }
auto sub f44(float a, *float b) { a /= b; return a; }
auto sub f45(*float a, int b) { a /= b; return a; }
auto sub f46(*float a, *int b) { a /= b; return a; }
auto sub f47(*float a, float b) { a /= b; return a; }
auto sub f48(*float a, *float b) { a /= b; return a; }
auto sub f49(int a, int b) { a %= b; return a; }
auto sub f50(int a, *int b) { a %= b; return a; }
auto sub f51(*int a, int b) { a %= b; return a; }
auto sub f52(*int a, *int b) { a %= b; return a; }
auto sub f53(int a, int b) { a &= b; return a; }
auto sub f54(int a, *int b) { a &= b; return a; }
auto sub f55(*int a, int b) { a &= b; return a; }
auto sub f56(*int a, *int b) { a &= b; return a; }
auto sub f57(int a, int b) { a |= b; return a; }
auto sub f58(int a, *int b) { a |= b; return a; }
auto sub f59(*int a, int b) { a |= b; return a; }
auto sub f60(*int a, *int b) { a |= b; return a; }
auto sub f61(int a, int b) { a ^= b; return a; }
auto sub f62(int a, *int b) { a ^= b; return a; }
auto sub f63(*int a, int b) { a ^= b; return a; }
auto sub f64(*int a, *int b) { a ^= b; return a; }
auto sub f65(int a, int b) { a <<= b; return a; }
auto sub f66(int a, *int b) { a <<= b; return a; }
auto sub f67(*int a, int b) { a <<= b; return a; }
auto sub f68(*int a, *int b) { a <<= b; return a; }
auto sub f69(int a, int b) { a >>= b; return a; }
auto sub f70(int a, *int b) { a >>= b; return a; }
auto sub f71(*int a, int b) { a >>= b; return a; }
auto sub f72(*int a, *int b) { a >>= b; return a; }

sub show(string label, code c) {
    string r;
    try {
        r = sprintf("%y", c());
    } catch (hash<ExceptionInfo> ex) {
        r = "EXC " + ex.err;
    }
    printf("%s = %s\n", label, r);
}

show("int += int: 0 += 0", auto sub () { return f1(0, 0); });
show("int += int: 0 += 3", auto sub () { return f1(0, 3); });
show("int += int: 0 += -2", auto sub () { return f1(0, -2); });
show("int += int: 3 += 0", auto sub () { return f1(3, 0); });
show("int += int: 3 += 3", auto sub () { return f1(3, 3); });
show("int += int: 3 += -2", auto sub () { return f1(3, -2); });
show("int += int: -2 += 0", auto sub () { return f1(-2, 0); });
show("int += int: -2 += 3", auto sub () { return f1(-2, 3); });
show("int += int: -2 += -2", auto sub () { return f1(-2, -2); });
show("int += int_: 0 += NOTHING", auto sub () { return f2(0, NOTHING); });
show("int += int_: 0 += 0", auto sub () { return f2(0, 0); });
show("int += int_: 0 += 3", auto sub () { return f2(0, 3); });
show("int += int_: 3 += NOTHING", auto sub () { return f2(3, NOTHING); });
show("int += int_: 3 += 0", auto sub () { return f2(3, 0); });
show("int += int_: 3 += 3", auto sub () { return f2(3, 3); });
show("int += int_: -2 += NOTHING", auto sub () { return f2(-2, NOTHING); });
show("int += int_: -2 += 0", auto sub () { return f2(-2, 0); });
show("int += int_: -2 += 3", auto sub () { return f2(-2, 3); });
show("int_ += int: NOTHING += 0", auto sub () { return f3(NOTHING, 0); });
show("int_ += int: NOTHING += 3", auto sub () { return f3(NOTHING, 3); });
show("int_ += int: NOTHING += -2", auto sub () { return f3(NOTHING, -2); });
show("int_ += int: 0 += 0", auto sub () { return f3(0, 0); });
show("int_ += int: 0 += 3", auto sub () { return f3(0, 3); });
show("int_ += int: 0 += -2", auto sub () { return f3(0, -2); });
show("int_ += int: 3 += 0", auto sub () { return f3(3, 0); });
show("int_ += int: 3 += 3", auto sub () { return f3(3, 3); });
show("int_ += int: 3 += -2", auto sub () { return f3(3, -2); });
show("int_ += int_: NOTHING += NOTHING", auto sub () { return f4(NOTHING, NOTHING); });
show("int_ += int_: NOTHING += 0", auto sub () { return f4(NOTHING, 0); });
show("int_ += int_: NOTHING += 3", auto sub () { return f4(NOTHING, 3); });
show("int_ += int_: 0 += NOTHING", auto sub () { return f4(0, NOTHING); });
show("int_ += int_: 0 += 0", auto sub () { return f4(0, 0); });
show("int_ += int_: 0 += 3", auto sub () { return f4(0, 3); });
show("int_ += int_: 3 += NOTHING", auto sub () { return f4(3, NOTHING); });
show("int_ += int_: 3 += 0", auto sub () { return f4(3, 0); });
show("int_ += int_: 3 += 3", auto sub () { return f4(3, 3); });
show("float += int: 0.0 += 0", auto sub () { return f5(0.0, 0); });
show("float += int: 0.0 += 3", auto sub () { return f5(0.0, 3); });
show("float += int: 0.0 += -2", auto sub () { return f5(0.0, -2); });
show("float += int: 1.5 += 0", auto sub () { return f5(1.5, 0); });
show("float += int: 1.5 += 3", auto sub () { return f5(1.5, 3); });
show("float += int: 1.5 += -2", auto sub () { return f5(1.5, -2); });
show("float += int: -2.5 += 0", auto sub () { return f5(-2.5, 0); });
show("float += int: -2.5 += 3", auto sub () { return f5(-2.5, 3); });
show("float += int: -2.5 += -2", auto sub () { return f5(-2.5, -2); });
show("float += int_: 0.0 += NOTHING", auto sub () { return f6(0.0, NOTHING); });
show("float += int_: 0.0 += 0", auto sub () { return f6(0.0, 0); });
show("float += int_: 0.0 += 3", auto sub () { return f6(0.0, 3); });
show("float += int_: 1.5 += NOTHING", auto sub () { return f6(1.5, NOTHING); });
show("float += int_: 1.5 += 0", auto sub () { return f6(1.5, 0); });
show("float += int_: 1.5 += 3", auto sub () { return f6(1.5, 3); });
show("float += int_: -2.5 += NOTHING", auto sub () { return f6(-2.5, NOTHING); });
show("float += int_: -2.5 += 0", auto sub () { return f6(-2.5, 0); });
show("float += int_: -2.5 += 3", auto sub () { return f6(-2.5, 3); });
show("float += float: 0.0 += 0.0", auto sub () { return f7(0.0, 0.0); });
show("float += float: 0.0 += 1.5", auto sub () { return f7(0.0, 1.5); });
show("float += float: 0.0 += -2.5", auto sub () { return f7(0.0, -2.5); });
show("float += float: 1.5 += 0.0", auto sub () { return f7(1.5, 0.0); });
show("float += float: 1.5 += 1.5", auto sub () { return f7(1.5, 1.5); });
show("float += float: 1.5 += -2.5", auto sub () { return f7(1.5, -2.5); });
show("float += float: -2.5 += 0.0", auto sub () { return f7(-2.5, 0.0); });
show("float += float: -2.5 += 1.5", auto sub () { return f7(-2.5, 1.5); });
show("float += float: -2.5 += -2.5", auto sub () { return f7(-2.5, -2.5); });
show("float += float_: 0.0 += NOTHING", auto sub () { return f8(0.0, NOTHING); });
show("float += float_: 0.0 += 0.0", auto sub () { return f8(0.0, 0.0); });
show("float += float_: 0.0 += 1.5", auto sub () { return f8(0.0, 1.5); });
show("float += float_: 1.5 += NOTHING", auto sub () { return f8(1.5, NOTHING); });
show("float += float_: 1.5 += 0.0", auto sub () { return f8(1.5, 0.0); });
show("float += float_: 1.5 += 1.5", auto sub () { return f8(1.5, 1.5); });
show("float += float_: -2.5 += NOTHING", auto sub () { return f8(-2.5, NOTHING); });
show("float += float_: -2.5 += 0.0", auto sub () { return f8(-2.5, 0.0); });
show("float += float_: -2.5 += 1.5", auto sub () { return f8(-2.5, 1.5); });
show("float_ += int: NOTHING += 0", auto sub () { return f9(NOTHING, 0); });
show("float_ += int: NOTHING += 3", auto sub () { return f9(NOTHING, 3); });
show("float_ += int: NOTHING += -2", auto sub () { return f9(NOTHING, -2); });
show("float_ += int: 0.0 += 0", auto sub () { return f9(0.0, 0); });
show("float_ += int: 0.0 += 3", auto sub () { return f9(0.0, 3); });
show("float_ += int: 0.0 += -2", auto sub () { return f9(0.0, -2); });
show("float_ += int: 1.5 += 0", auto sub () { return f9(1.5, 0); });
show("float_ += int: 1.5 += 3", auto sub () { return f9(1.5, 3); });
show("float_ += int: 1.5 += -2", auto sub () { return f9(1.5, -2); });
show("float_ += int_: NOTHING += NOTHING", auto sub () { return f10(NOTHING, NOTHING); });
show("float_ += int_: NOTHING += 0", auto sub () { return f10(NOTHING, 0); });
show("float_ += int_: NOTHING += 3", auto sub () { return f10(NOTHING, 3); });
show("float_ += int_: 0.0 += NOTHING", auto sub () { return f10(0.0, NOTHING); });
show("float_ += int_: 0.0 += 0", auto sub () { return f10(0.0, 0); });
show("float_ += int_: 0.0 += 3", auto sub () { return f10(0.0, 3); });
show("float_ += int_: 1.5 += NOTHING", auto sub () { return f10(1.5, NOTHING); });
show("float_ += int_: 1.5 += 0", auto sub () { return f10(1.5, 0); });
show("float_ += int_: 1.5 += 3", auto sub () { return f10(1.5, 3); });
show("float_ += float: NOTHING += 0.0", auto sub () { return f11(NOTHING, 0.0); });
show("float_ += float: NOTHING += 1.5", auto sub () { return f11(NOTHING, 1.5); });
show("float_ += float: NOTHING += -2.5", auto sub () { return f11(NOTHING, -2.5); });
show("float_ += float: 0.0 += 0.0", auto sub () { return f11(0.0, 0.0); });
show("float_ += float: 0.0 += 1.5", auto sub () { return f11(0.0, 1.5); });
show("float_ += float: 0.0 += -2.5", auto sub () { return f11(0.0, -2.5); });
show("float_ += float: 1.5 += 0.0", auto sub () { return f11(1.5, 0.0); });
show("float_ += float: 1.5 += 1.5", auto sub () { return f11(1.5, 1.5); });
show("float_ += float: 1.5 += -2.5", auto sub () { return f11(1.5, -2.5); });
show("float_ += float_: NOTHING += NOTHING", auto sub () { return f12(NOTHING, NOTHING); });
show("float_ += float_: NOTHING += 0.0", auto sub () { return f12(NOTHING, 0.0); });
show("float_ += float_: NOTHING += 1.5", auto sub () { return f12(NOTHING, 1.5); });
show("float_ += float_: 0.0 += NOTHING", auto sub () { return f12(0.0, NOTHING); });
show("float_ += float_: 0.0 += 0.0", auto sub () { return f12(0.0, 0.0); });
show("float_ += float_: 0.0 += 1.5", auto sub () { return f12(0.0, 1.5); });
show("float_ += float_: 1.5 += NOTHING", auto sub () { return f12(1.5, NOTHING); });
show("float_ += float_: 1.5 += 0.0", auto sub () { return f12(1.5, 0.0); });
show("float_ += float_: 1.5 += 1.5", auto sub () { return f12(1.5, 1.5); });
show("int -= int: 0 -= 0", auto sub () { return f13(0, 0); });
show("int -= int: 0 -= 3", auto sub () { return f13(0, 3); });
show("int -= int: 0 -= -2", auto sub () { return f13(0, -2); });
show("int -= int: 3 -= 0", auto sub () { return f13(3, 0); });
show("int -= int: 3 -= 3", auto sub () { return f13(3, 3); });
show("int -= int: 3 -= -2", auto sub () { return f13(3, -2); });
show("int -= int: -2 -= 0", auto sub () { return f13(-2, 0); });
show("int -= int: -2 -= 3", auto sub () { return f13(-2, 3); });
show("int -= int: -2 -= -2", auto sub () { return f13(-2, -2); });
show("int -= int_: 0 -= NOTHING", auto sub () { return f14(0, NOTHING); });
show("int -= int_: 0 -= 0", auto sub () { return f14(0, 0); });
show("int -= int_: 0 -= 3", auto sub () { return f14(0, 3); });
show("int -= int_: 3 -= NOTHING", auto sub () { return f14(3, NOTHING); });
show("int -= int_: 3 -= 0", auto sub () { return f14(3, 0); });
show("int -= int_: 3 -= 3", auto sub () { return f14(3, 3); });
show("int -= int_: -2 -= NOTHING", auto sub () { return f14(-2, NOTHING); });
show("int -= int_: -2 -= 0", auto sub () { return f14(-2, 0); });
show("int -= int_: -2 -= 3", auto sub () { return f14(-2, 3); });
show("int_ -= int: NOTHING -= 0", auto sub () { return f15(NOTHING, 0); });
show("int_ -= int: NOTHING -= 3", auto sub () { return f15(NOTHING, 3); });
show("int_ -= int: NOTHING -= -2", auto sub () { return f15(NOTHING, -2); });
show("int_ -= int: 0 -= 0", auto sub () { return f15(0, 0); });
show("int_ -= int: 0 -= 3", auto sub () { return f15(0, 3); });
show("int_ -= int: 0 -= -2", auto sub () { return f15(0, -2); });
show("int_ -= int: 3 -= 0", auto sub () { return f15(3, 0); });
show("int_ -= int: 3 -= 3", auto sub () { return f15(3, 3); });
show("int_ -= int: 3 -= -2", auto sub () { return f15(3, -2); });
show("int_ -= int_: NOTHING -= NOTHING", auto sub () { return f16(NOTHING, NOTHING); });
show("int_ -= int_: NOTHING -= 0", auto sub () { return f16(NOTHING, 0); });
show("int_ -= int_: NOTHING -= 3", auto sub () { return f16(NOTHING, 3); });
show("int_ -= int_: 0 -= NOTHING", auto sub () { return f16(0, NOTHING); });
show("int_ -= int_: 0 -= 0", auto sub () { return f16(0, 0); });
show("int_ -= int_: 0 -= 3", auto sub () { return f16(0, 3); });
show("int_ -= int_: 3 -= NOTHING", auto sub () { return f16(3, NOTHING); });
show("int_ -= int_: 3 -= 0", auto sub () { return f16(3, 0); });
show("int_ -= int_: 3 -= 3", auto sub () { return f16(3, 3); });
show("float -= int: 0.0 -= 0", auto sub () { return f17(0.0, 0); });
show("float -= int: 0.0 -= 3", auto sub () { return f17(0.0, 3); });
show("float -= int: 0.0 -= -2", auto sub () { return f17(0.0, -2); });
show("float -= int: 1.5 -= 0", auto sub () { return f17(1.5, 0); });
show("float -= int: 1.5 -= 3", auto sub () { return f17(1.5, 3); });
show("float -= int: 1.5 -= -2", auto sub () { return f17(1.5, -2); });
show("float -= int: -2.5 -= 0", auto sub () { return f17(-2.5, 0); });
show("float -= int: -2.5 -= 3", auto sub () { return f17(-2.5, 3); });
show("float -= int: -2.5 -= -2", auto sub () { return f17(-2.5, -2); });
show("float -= int_: 0.0 -= NOTHING", auto sub () { return f18(0.0, NOTHING); });
show("float -= int_: 0.0 -= 0", auto sub () { return f18(0.0, 0); });
show("float -= int_: 0.0 -= 3", auto sub () { return f18(0.0, 3); });
show("float -= int_: 1.5 -= NOTHING", auto sub () { return f18(1.5, NOTHING); });
show("float -= int_: 1.5 -= 0", auto sub () { return f18(1.5, 0); });
show("float -= int_: 1.5 -= 3", auto sub () { return f18(1.5, 3); });
show("float -= int_: -2.5 -= NOTHING", auto sub () { return f18(-2.5, NOTHING); });
show("float -= int_: -2.5 -= 0", auto sub () { return f18(-2.5, 0); });
show("float -= int_: -2.5 -= 3", auto sub () { return f18(-2.5, 3); });
show("float -= float: 0.0 -= 0.0", auto sub () { return f19(0.0, 0.0); });
show("float -= float: 0.0 -= 1.5", auto sub () { return f19(0.0, 1.5); });
show("float -= float: 0.0 -= -2.5", auto sub () { return f19(0.0, -2.5); });
show("float -= float: 1.5 -= 0.0", auto sub () { return f19(1.5, 0.0); });
show("float -= float: 1.5 -= 1.5", auto sub () { return f19(1.5, 1.5); });
show("float -= float: 1.5 -= -2.5", auto sub () { return f19(1.5, -2.5); });
show("float -= float: -2.5 -= 0.0", auto sub () { return f19(-2.5, 0.0); });
show("float -= float: -2.5 -= 1.5", auto sub () { return f19(-2.5, 1.5); });
show("float -= float: -2.5 -= -2.5", auto sub () { return f19(-2.5, -2.5); });
show("float -= float_: 0.0 -= NOTHING", auto sub () { return f20(0.0, NOTHING); });
show("float -= float_: 0.0 -= 0.0", auto sub () { return f20(0.0, 0.0); });
show("float -= float_: 0.0 -= 1.5", auto sub () { return f20(0.0, 1.5); });
show("float -= float_: 1.5 -= NOTHING", auto sub () { return f20(1.5, NOTHING); });
show("float -= float_: 1.5 -= 0.0", auto sub () { return f20(1.5, 0.0); });
show("float -= float_: 1.5 -= 1.5", auto sub () { return f20(1.5, 1.5); });
show("float -= float_: -2.5 -= NOTHING", auto sub () { return f20(-2.5, NOTHING); });
show("float -= float_: -2.5 -= 0.0", auto sub () { return f20(-2.5, 0.0); });
show("float -= float_: -2.5 -= 1.5", auto sub () { return f20(-2.5, 1.5); });
show("float_ -= int: NOTHING -= 0", auto sub () { return f21(NOTHING, 0); });
show("float_ -= int: NOTHING -= 3", auto sub () { return f21(NOTHING, 3); });
show("float_ -= int: NOTHING -= -2", auto sub () { return f21(NOTHING, -2); });
show("float_ -= int: 0.0 -= 0", auto sub () { return f21(0.0, 0); });
show("float_ -= int: 0.0 -= 3", auto sub () { return f21(0.0, 3); });
show("float_ -= int: 0.0 -= -2", auto sub () { return f21(0.0, -2); });
show("float_ -= int: 1.5 -= 0", auto sub () { return f21(1.5, 0); });
show("float_ -= int: 1.5 -= 3", auto sub () { return f21(1.5, 3); });
show("float_ -= int: 1.5 -= -2", auto sub () { return f21(1.5, -2); });
show("float_ -= int_: NOTHING -= NOTHING", auto sub () { return f22(NOTHING, NOTHING); });
show("float_ -= int_: NOTHING -= 0", auto sub () { return f22(NOTHING, 0); });
show("float_ -= int_: NOTHING -= 3", auto sub () { return f22(NOTHING, 3); });
show("float_ -= int_: 0.0 -= NOTHING", auto sub () { return f22(0.0, NOTHING); });
show("float_ -= int_: 0.0 -= 0", auto sub () { return f22(0.0, 0); });
show("float_ -= int_: 0.0 -= 3", auto sub () { return f22(0.0, 3); });
show("float_ -= int_: 1.5 -= NOTHING", auto sub () { return f22(1.5, NOTHING); });
show("float_ -= int_: 1.5 -= 0", auto sub () { return f22(1.5, 0); });
show("float_ -= int_: 1.5 -= 3", auto sub () { return f22(1.5, 3); });
show("float_ -= float: NOTHING -= 0.0", auto sub () { return f23(NOTHING, 0.0); });
show("float_ -= float: NOTHING -= 1.5", auto sub () { return f23(NOTHING, 1.5); });
show("float_ -= float: NOTHING -= -2.5", auto sub () { return f23(NOTHING, -2.5); });
show("float_ -= float: 0.0 -= 0.0", auto sub () { return f23(0.0, 0.0); });
show("float_ -= float: 0.0 -= 1.5", auto sub () { return f23(0.0, 1.5); });
show("float_ -= float: 0.0 -= -2.5", auto sub () { return f23(0.0, -2.5); });
show("float_ -= float: 1.5 -= 0.0", auto sub () { return f23(1.5, 0.0); });
show("float_ -= float: 1.5 -= 1.5", auto sub () { return f23(1.5, 1.5); });
show("float_ -= float: 1.5 -= -2.5", auto sub () { return f23(1.5, -2.5); });
show("float_ -= float_: NOTHING -= NOTHING", auto sub () { return f24(NOTHING, NOTHING); });
show("float_ -= float_: NOTHING -= 0.0", auto sub () { return f24(NOTHING, 0.0); });
show("float_ -= float_: NOTHING -= 1.5", auto sub () { return f24(NOTHING, 1.5); });
show("float_ -= float_: 0.0 -= NOTHING", auto sub () { return f24(0.0, NOTHING); });
show("float_ -= float_: 0.0 -= 0.0", auto sub () { return f24(0.0, 0.0); });
show("float_ -= float_: 0.0 -= 1.5", auto sub () { return f24(0.0, 1.5); });
show("float_ -= float_: 1.5 -= NOTHING", auto sub () { return f24(1.5, NOTHING); });
show("float_ -= float_: 1.5 -= 0.0", auto sub () { return f24(1.5, 0.0); });
show("float_ -= float_: 1.5 -= 1.5", auto sub () { return f24(1.5, 1.5); });
show("int *= int: 0 *= 0", auto sub () { return f25(0, 0); });
show("int *= int: 0 *= 3", auto sub () { return f25(0, 3); });
show("int *= int: 0 *= -2", auto sub () { return f25(0, -2); });
show("int *= int: 3 *= 0", auto sub () { return f25(3, 0); });
show("int *= int: 3 *= 3", auto sub () { return f25(3, 3); });
show("int *= int: 3 *= -2", auto sub () { return f25(3, -2); });
show("int *= int: -2 *= 0", auto sub () { return f25(-2, 0); });
show("int *= int: -2 *= 3", auto sub () { return f25(-2, 3); });
show("int *= int: -2 *= -2", auto sub () { return f25(-2, -2); });
show("int *= int_: 0 *= NOTHING", auto sub () { return f26(0, NOTHING); });
show("int *= int_: 0 *= 0", auto sub () { return f26(0, 0); });
show("int *= int_: 0 *= 3", auto sub () { return f26(0, 3); });
show("int *= int_: 3 *= NOTHING", auto sub () { return f26(3, NOTHING); });
show("int *= int_: 3 *= 0", auto sub () { return f26(3, 0); });
show("int *= int_: 3 *= 3", auto sub () { return f26(3, 3); });
show("int *= int_: -2 *= NOTHING", auto sub () { return f26(-2, NOTHING); });
show("int *= int_: -2 *= 0", auto sub () { return f26(-2, 0); });
show("int *= int_: -2 *= 3", auto sub () { return f26(-2, 3); });
show("int_ *= int: NOTHING *= 0", auto sub () { return f27(NOTHING, 0); });
show("int_ *= int: NOTHING *= 3", auto sub () { return f27(NOTHING, 3); });
show("int_ *= int: NOTHING *= -2", auto sub () { return f27(NOTHING, -2); });
show("int_ *= int: 0 *= 0", auto sub () { return f27(0, 0); });
show("int_ *= int: 0 *= 3", auto sub () { return f27(0, 3); });
show("int_ *= int: 0 *= -2", auto sub () { return f27(0, -2); });
show("int_ *= int: 3 *= 0", auto sub () { return f27(3, 0); });
show("int_ *= int: 3 *= 3", auto sub () { return f27(3, 3); });
show("int_ *= int: 3 *= -2", auto sub () { return f27(3, -2); });
show("int_ *= int_: NOTHING *= NOTHING", auto sub () { return f28(NOTHING, NOTHING); });
show("int_ *= int_: NOTHING *= 0", auto sub () { return f28(NOTHING, 0); });
show("int_ *= int_: NOTHING *= 3", auto sub () { return f28(NOTHING, 3); });
show("int_ *= int_: 0 *= NOTHING", auto sub () { return f28(0, NOTHING); });
show("int_ *= int_: 0 *= 0", auto sub () { return f28(0, 0); });
show("int_ *= int_: 0 *= 3", auto sub () { return f28(0, 3); });
show("int_ *= int_: 3 *= NOTHING", auto sub () { return f28(3, NOTHING); });
show("int_ *= int_: 3 *= 0", auto sub () { return f28(3, 0); });
show("int_ *= int_: 3 *= 3", auto sub () { return f28(3, 3); });
show("float *= int: 0.0 *= 0", auto sub () { return f29(0.0, 0); });
show("float *= int: 0.0 *= 3", auto sub () { return f29(0.0, 3); });
show("float *= int: 0.0 *= -2", auto sub () { return f29(0.0, -2); });
show("float *= int: 1.5 *= 0", auto sub () { return f29(1.5, 0); });
show("float *= int: 1.5 *= 3", auto sub () { return f29(1.5, 3); });
show("float *= int: 1.5 *= -2", auto sub () { return f29(1.5, -2); });
show("float *= int: -2.5 *= 0", auto sub () { return f29(-2.5, 0); });
show("float *= int: -2.5 *= 3", auto sub () { return f29(-2.5, 3); });
show("float *= int: -2.5 *= -2", auto sub () { return f29(-2.5, -2); });
show("float *= int_: 0.0 *= NOTHING", auto sub () { return f30(0.0, NOTHING); });
show("float *= int_: 0.0 *= 0", auto sub () { return f30(0.0, 0); });
show("float *= int_: 0.0 *= 3", auto sub () { return f30(0.0, 3); });
show("float *= int_: 1.5 *= NOTHING", auto sub () { return f30(1.5, NOTHING); });
show("float *= int_: 1.5 *= 0", auto sub () { return f30(1.5, 0); });
show("float *= int_: 1.5 *= 3", auto sub () { return f30(1.5, 3); });
show("float *= int_: -2.5 *= NOTHING", auto sub () { return f30(-2.5, NOTHING); });
show("float *= int_: -2.5 *= 0", auto sub () { return f30(-2.5, 0); });
show("float *= int_: -2.5 *= 3", auto sub () { return f30(-2.5, 3); });
show("float *= float: 0.0 *= 0.0", auto sub () { return f31(0.0, 0.0); });
show("float *= float: 0.0 *= 1.5", auto sub () { return f31(0.0, 1.5); });
show("float *= float: 0.0 *= -2.5", auto sub () { return f31(0.0, -2.5); });
show("float *= float: 1.5 *= 0.0", auto sub () { return f31(1.5, 0.0); });
show("float *= float: 1.5 *= 1.5", auto sub () { return f31(1.5, 1.5); });
show("float *= float: 1.5 *= -2.5", auto sub () { return f31(1.5, -2.5); });
show("float *= float: -2.5 *= 0.0", auto sub () { return f31(-2.5, 0.0); });
show("float *= float: -2.5 *= 1.5", auto sub () { return f31(-2.5, 1.5); });
show("float *= float: -2.5 *= -2.5", auto sub () { return f31(-2.5, -2.5); });
show("float *= float_: 0.0 *= NOTHING", auto sub () { return f32(0.0, NOTHING); });
show("float *= float_: 0.0 *= 0.0", auto sub () { return f32(0.0, 0.0); });
show("float *= float_: 0.0 *= 1.5", auto sub () { return f32(0.0, 1.5); });
show("float *= float_: 1.5 *= NOTHING", auto sub () { return f32(1.5, NOTHING); });
show("float *= float_: 1.5 *= 0.0", auto sub () { return f32(1.5, 0.0); });
show("float *= float_: 1.5 *= 1.5", auto sub () { return f32(1.5, 1.5); });
show("float *= float_: -2.5 *= NOTHING", auto sub () { return f32(-2.5, NOTHING); });
show("float *= float_: -2.5 *= 0.0", auto sub () { return f32(-2.5, 0.0); });
show("float *= float_: -2.5 *= 1.5", auto sub () { return f32(-2.5, 1.5); });
show("float_ *= int: NOTHING *= 0", auto sub () { return f33(NOTHING, 0); });
show("float_ *= int: NOTHING *= 3", auto sub () { return f33(NOTHING, 3); });
show("float_ *= int: NOTHING *= -2", auto sub () { return f33(NOTHING, -2); });
show("float_ *= int: 0.0 *= 0", auto sub () { return f33(0.0, 0); });
show("float_ *= int: 0.0 *= 3", auto sub () { return f33(0.0, 3); });
show("float_ *= int: 0.0 *= -2", auto sub () { return f33(0.0, -2); });
show("float_ *= int: 1.5 *= 0", auto sub () { return f33(1.5, 0); });
show("float_ *= int: 1.5 *= 3", auto sub () { return f33(1.5, 3); });
show("float_ *= int: 1.5 *= -2", auto sub () { return f33(1.5, -2); });
show("float_ *= int_: NOTHING *= NOTHING", auto sub () { return f34(NOTHING, NOTHING); });
show("float_ *= int_: NOTHING *= 0", auto sub () { return f34(NOTHING, 0); });
show("float_ *= int_: NOTHING *= 3", auto sub () { return f34(NOTHING, 3); });
show("float_ *= int_: 0.0 *= NOTHING", auto sub () { return f34(0.0, NOTHING); });
show("float_ *= int_: 0.0 *= 0", auto sub () { return f34(0.0, 0); });
show("float_ *= int_: 0.0 *= 3", auto sub () { return f34(0.0, 3); });
show("float_ *= int_: 1.5 *= NOTHING", auto sub () { return f34(1.5, NOTHING); });
show("float_ *= int_: 1.5 *= 0", auto sub () { return f34(1.5, 0); });
show("float_ *= int_: 1.5 *= 3", auto sub () { return f34(1.5, 3); });
show("float_ *= float: NOTHING *= 0.0", auto sub () { return f35(NOTHING, 0.0); });
show("float_ *= float: NOTHING *= 1.5", auto sub () { return f35(NOTHING, 1.5); });
show("float_ *= float: NOTHING *= -2.5", auto sub () { return f35(NOTHING, -2.5); });
show("float_ *= float: 0.0 *= 0.0", auto sub () { return f35(0.0, 0.0); });
show("float_ *= float: 0.0 *= 1.5", auto sub () { return f35(0.0, 1.5); });
show("float_ *= float: 0.0 *= -2.5", auto sub () { return f35(0.0, -2.5); });
show("float_ *= float: 1.5 *= 0.0", auto sub () { return f35(1.5, 0.0); });
show("float_ *= float: 1.5 *= 1.5", auto sub () { return f35(1.5, 1.5); });
show("float_ *= float: 1.5 *= -2.5", auto sub () { return f35(1.5, -2.5); });
show("float_ *= float_: NOTHING *= NOTHING", auto sub () { return f36(NOTHING, NOTHING); });
show("float_ *= float_: NOTHING *= 0.0", auto sub () { return f36(NOTHING, 0.0); });
show("float_ *= float_: NOTHING *= 1.5", auto sub () { return f36(NOTHING, 1.5); });
show("float_ *= float_: 0.0 *= NOTHING", auto sub () { return f36(0.0, NOTHING); });
show("float_ *= float_: 0.0 *= 0.0", auto sub () { return f36(0.0, 0.0); });
show("float_ *= float_: 0.0 *= 1.5", auto sub () { return f36(0.0, 1.5); });
show("float_ *= float_: 1.5 *= NOTHING", auto sub () { return f36(1.5, NOTHING); });
show("float_ *= float_: 1.5 *= 0.0", auto sub () { return f36(1.5, 0.0); });
show("float_ *= float_: 1.5 *= 1.5", auto sub () { return f36(1.5, 1.5); });
show("int /= int: 0 /= 0", auto sub () { return f37(0, 0); });
show("int /= int: 0 /= 3", auto sub () { return f37(0, 3); });
show("int /= int: 0 /= -2", auto sub () { return f37(0, -2); });
show("int /= int: 3 /= 0", auto sub () { return f37(3, 0); });
show("int /= int: 3 /= 3", auto sub () { return f37(3, 3); });
show("int /= int: 3 /= -2", auto sub () { return f37(3, -2); });
show("int /= int: -2 /= 0", auto sub () { return f37(-2, 0); });
show("int /= int: -2 /= 3", auto sub () { return f37(-2, 3); });
show("int /= int: -2 /= -2", auto sub () { return f37(-2, -2); });
show("int /= int_: 0 /= NOTHING", auto sub () { return f38(0, NOTHING); });
show("int /= int_: 0 /= 0", auto sub () { return f38(0, 0); });
show("int /= int_: 0 /= 3", auto sub () { return f38(0, 3); });
show("int /= int_: 3 /= NOTHING", auto sub () { return f38(3, NOTHING); });
show("int /= int_: 3 /= 0", auto sub () { return f38(3, 0); });
show("int /= int_: 3 /= 3", auto sub () { return f38(3, 3); });
show("int /= int_: -2 /= NOTHING", auto sub () { return f38(-2, NOTHING); });
show("int /= int_: -2 /= 0", auto sub () { return f38(-2, 0); });
show("int /= int_: -2 /= 3", auto sub () { return f38(-2, 3); });
show("int_ /= int: NOTHING /= 0", auto sub () { return f39(NOTHING, 0); });
show("int_ /= int: NOTHING /= 3", auto sub () { return f39(NOTHING, 3); });
show("int_ /= int: NOTHING /= -2", auto sub () { return f39(NOTHING, -2); });
show("int_ /= int: 0 /= 0", auto sub () { return f39(0, 0); });
show("int_ /= int: 0 /= 3", auto sub () { return f39(0, 3); });
show("int_ /= int: 0 /= -2", auto sub () { return f39(0, -2); });
show("int_ /= int: 3 /= 0", auto sub () { return f39(3, 0); });
show("int_ /= int: 3 /= 3", auto sub () { return f39(3, 3); });
show("int_ /= int: 3 /= -2", auto sub () { return f39(3, -2); });
show("int_ /= int_: NOTHING /= NOTHING", auto sub () { return f40(NOTHING, NOTHING); });
show("int_ /= int_: NOTHING /= 0", auto sub () { return f40(NOTHING, 0); });
show("int_ /= int_: NOTHING /= 3", auto sub () { return f40(NOTHING, 3); });
show("int_ /= int_: 0 /= NOTHING", auto sub () { return f40(0, NOTHING); });
show("int_ /= int_: 0 /= 0", auto sub () { return f40(0, 0); });
show("int_ /= int_: 0 /= 3", auto sub () { return f40(0, 3); });
show("int_ /= int_: 3 /= NOTHING", auto sub () { return f40(3, NOTHING); });
show("int_ /= int_: 3 /= 0", auto sub () { return f40(3, 0); });
show("int_ /= int_: 3 /= 3", auto sub () { return f40(3, 3); });
show("float /= int: 0.0 /= 0", auto sub () { return f41(0.0, 0); });
show("float /= int: 0.0 /= 3", auto sub () { return f41(0.0, 3); });
show("float /= int: 0.0 /= -2", auto sub () { return f41(0.0, -2); });
show("float /= int: 1.5 /= 0", auto sub () { return f41(1.5, 0); });
show("float /= int: 1.5 /= 3", auto sub () { return f41(1.5, 3); });
show("float /= int: 1.5 /= -2", auto sub () { return f41(1.5, -2); });
show("float /= int: -2.5 /= 0", auto sub () { return f41(-2.5, 0); });
show("float /= int: -2.5 /= 3", auto sub () { return f41(-2.5, 3); });
show("float /= int: -2.5 /= -2", auto sub () { return f41(-2.5, -2); });
show("float /= int_: 0.0 /= NOTHING", auto sub () { return f42(0.0, NOTHING); });
show("float /= int_: 0.0 /= 0", auto sub () { return f42(0.0, 0); });
show("float /= int_: 0.0 /= 3", auto sub () { return f42(0.0, 3); });
show("float /= int_: 1.5 /= NOTHING", auto sub () { return f42(1.5, NOTHING); });
show("float /= int_: 1.5 /= 0", auto sub () { return f42(1.5, 0); });
show("float /= int_: 1.5 /= 3", auto sub () { return f42(1.5, 3); });
show("float /= int_: -2.5 /= NOTHING", auto sub () { return f42(-2.5, NOTHING); });
show("float /= int_: -2.5 /= 0", auto sub () { return f42(-2.5, 0); });
show("float /= int_: -2.5 /= 3", auto sub () { return f42(-2.5, 3); });
show("float /= float: 0.0 /= 0.0", auto sub () { return f43(0.0, 0.0); });
show("float /= float: 0.0 /= 1.5", auto sub () { return f43(0.0, 1.5); });
show("float /= float: 0.0 /= -2.5", auto sub () { return f43(0.0, -2.5); });
show("float /= float: 1.5 /= 0.0", auto sub () { return f43(1.5, 0.0); });
show("float /= float: 1.5 /= 1.5", auto sub () { return f43(1.5, 1.5); });
show("float /= float: 1.5 /= -2.5", auto sub () { return f43(1.5, -2.5); });
show("float /= float: -2.5 /= 0.0", auto sub () { return f43(-2.5, 0.0); });
show("float /= float: -2.5 /= 1.5", auto sub () { return f43(-2.5, 1.5); });
show("float /= float: -2.5 /= -2.5", auto sub () { return f43(-2.5, -2.5); });
show("float /= float_: 0.0 /= NOTHING", auto sub () { return f44(0.0, NOTHING); });
show("float /= float_: 0.0 /= 0.0", auto sub () { return f44(0.0, 0.0); });
show("float /= float_: 0.0 /= 1.5", auto sub () { return f44(0.0, 1.5); });
show("float /= float_: 1.5 /= NOTHING", auto sub () { return f44(1.5, NOTHING); });
show("float /= float_: 1.5 /= 0.0", auto sub () { return f44(1.5, 0.0); });
show("float /= float_: 1.5 /= 1.5", auto sub () { return f44(1.5, 1.5); });
show("float /= float_: -2.5 /= NOTHING", auto sub () { return f44(-2.5, NOTHING); });
show("float /= float_: -2.5 /= 0.0", auto sub () { return f44(-2.5, 0.0); });
show("float /= float_: -2.5 /= 1.5", auto sub () { return f44(-2.5, 1.5); });
show("float_ /= int: NOTHING /= 0", auto sub () { return f45(NOTHING, 0); });
show("float_ /= int: NOTHING /= 3", auto sub () { return f45(NOTHING, 3); });
show("float_ /= int: NOTHING /= -2", auto sub () { return f45(NOTHING, -2); });
show("float_ /= int: 0.0 /= 0", auto sub () { return f45(0.0, 0); });
show("float_ /= int: 0.0 /= 3", auto sub () { return f45(0.0, 3); });
show("float_ /= int: 0.0 /= -2", auto sub () { return f45(0.0, -2); });
show("float_ /= int: 1.5 /= 0", auto sub () { return f45(1.5, 0); });
show("float_ /= int: 1.5 /= 3", auto sub () { return f45(1.5, 3); });
show("float_ /= int: 1.5 /= -2", auto sub () { return f45(1.5, -2); });
show("float_ /= int_: NOTHING /= NOTHING", auto sub () { return f46(NOTHING, NOTHING); });
show("float_ /= int_: NOTHING /= 0", auto sub () { return f46(NOTHING, 0); });
show("float_ /= int_: NOTHING /= 3", auto sub () { return f46(NOTHING, 3); });
show("float_ /= int_: 0.0 /= NOTHING", auto sub () { return f46(0.0, NOTHING); });
show("float_ /= int_: 0.0 /= 0", auto sub () { return f46(0.0, 0); });
show("float_ /= int_: 0.0 /= 3", auto sub () { return f46(0.0, 3); });
show("float_ /= int_: 1.5 /= NOTHING", auto sub () { return f46(1.5, NOTHING); });
show("float_ /= int_: 1.5 /= 0", auto sub () { return f46(1.5, 0); });
show("float_ /= int_: 1.5 /= 3", auto sub () { return f46(1.5, 3); });
show("float_ /= float: NOTHING /= 0.0", auto sub () { return f47(NOTHING, 0.0); });
show("float_ /= float: NOTHING /= 1.5", auto sub () { return f47(NOTHING, 1.5); });
show("float_ /= float: NOTHING /= -2.5", auto sub () { return f47(NOTHING, -2.5); });
show("float_ /= float: 0.0 /= 0.0", auto sub () { return f47(0.0, 0.0); });
show("float_ /= float: 0.0 /= 1.5", auto sub () { return f47(0.0, 1.5); });
show("float_ /= float: 0.0 /= -2.5", auto sub () { return f47(0.0, -2.5); });
show("float_ /= float: 1.5 /= 0.0", auto sub () { return f47(1.5, 0.0); });
show("float_ /= float: 1.5 /= 1.5", auto sub () { return f47(1.5, 1.5); });
show("float_ /= float: 1.5 /= -2.5", auto sub () { return f47(1.5, -2.5); });
show("float_ /= float_: NOTHING /= NOTHING", auto sub () { return f48(NOTHING, NOTHING); });
show("float_ /= float_: NOTHING /= 0.0", auto sub () { return f48(NOTHING, 0.0); });
show("float_ /= float_: NOTHING /= 1.5", auto sub () { return f48(NOTHING, 1.5); });
show("float_ /= float_: 0.0 /= NOTHING", auto sub () { return f48(0.0, NOTHING); });
show("float_ /= float_: 0.0 /= 0.0", auto sub () { return f48(0.0, 0.0); });
show("float_ /= float_: 0.0 /= 1.5", auto sub () { return f48(0.0, 1.5); });
show("float_ /= float_: 1.5 /= NOTHING", auto sub () { return f48(1.5, NOTHING); });
show("float_ /= float_: 1.5 /= 0.0", auto sub () { return f48(1.5, 0.0); });
show("float_ /= float_: 1.5 /= 1.5", auto sub () { return f48(1.5, 1.5); });
show("int %= int: 0 %= 0", auto sub () { return f49(0, 0); });
show("int %= int: 0 %= 3", auto sub () { return f49(0, 3); });
show("int %= int: 0 %= -2", auto sub () { return f49(0, -2); });
show("int %= int: 3 %= 0", auto sub () { return f49(3, 0); });
show("int %= int: 3 %= 3", auto sub () { return f49(3, 3); });
show("int %= int: 3 %= -2", auto sub () { return f49(3, -2); });
show("int %= int: -2 %= 0", auto sub () { return f49(-2, 0); });
show("int %= int: -2 %= 3", auto sub () { return f49(-2, 3); });
show("int %= int: -2 %= -2", auto sub () { return f49(-2, -2); });
show("int %= int_: 0 %= NOTHING", auto sub () { return f50(0, NOTHING); });
show("int %= int_: 0 %= 0", auto sub () { return f50(0, 0); });
show("int %= int_: 0 %= 3", auto sub () { return f50(0, 3); });
show("int %= int_: 3 %= NOTHING", auto sub () { return f50(3, NOTHING); });
show("int %= int_: 3 %= 0", auto sub () { return f50(3, 0); });
show("int %= int_: 3 %= 3", auto sub () { return f50(3, 3); });
show("int %= int_: -2 %= NOTHING", auto sub () { return f50(-2, NOTHING); });
show("int %= int_: -2 %= 0", auto sub () { return f50(-2, 0); });
show("int %= int_: -2 %= 3", auto sub () { return f50(-2, 3); });
show("int_ %= int: NOTHING %= 0", auto sub () { return f51(NOTHING, 0); });
show("int_ %= int: NOTHING %= 3", auto sub () { return f51(NOTHING, 3); });
show("int_ %= int: NOTHING %= -2", auto sub () { return f51(NOTHING, -2); });
show("int_ %= int: 0 %= 0", auto sub () { return f51(0, 0); });
show("int_ %= int: 0 %= 3", auto sub () { return f51(0, 3); });
show("int_ %= int: 0 %= -2", auto sub () { return f51(0, -2); });
show("int_ %= int: 3 %= 0", auto sub () { return f51(3, 0); });
show("int_ %= int: 3 %= 3", auto sub () { return f51(3, 3); });
show("int_ %= int: 3 %= -2", auto sub () { return f51(3, -2); });
show("int_ %= int_: NOTHING %= NOTHING", auto sub () { return f52(NOTHING, NOTHING); });
show("int_ %= int_: NOTHING %= 0", auto sub () { return f52(NOTHING, 0); });
show("int_ %= int_: NOTHING %= 3", auto sub () { return f52(NOTHING, 3); });
show("int_ %= int_: 0 %= NOTHING", auto sub () { return f52(0, NOTHING); });
show("int_ %= int_: 0 %= 0", auto sub () { return f52(0, 0); });
show("int_ %= int_: 0 %= 3", auto sub () { return f52(0, 3); });
show("int_ %= int_: 3 %= NOTHING", auto sub () { return f52(3, NOTHING); });
show("int_ %= int_: 3 %= 0", auto sub () { return f52(3, 0); });
show("int_ %= int_: 3 %= 3", auto sub () { return f52(3, 3); });
show("int &= int: 0 &= 0", auto sub () { return f53(0, 0); });
show("int &= int: 0 &= 3", auto sub () { return f53(0, 3); });
show("int &= int: 0 &= -2", auto sub () { return f53(0, -2); });
show("int &= int: 3 &= 0", auto sub () { return f53(3, 0); });
show("int &= int: 3 &= 3", auto sub () { return f53(3, 3); });
show("int &= int: 3 &= -2", auto sub () { return f53(3, -2); });
show("int &= int: -2 &= 0", auto sub () { return f53(-2, 0); });
show("int &= int: -2 &= 3", auto sub () { return f53(-2, 3); });
show("int &= int: -2 &= -2", auto sub () { return f53(-2, -2); });
show("int &= int_: 0 &= NOTHING", auto sub () { return f54(0, NOTHING); });
show("int &= int_: 0 &= 0", auto sub () { return f54(0, 0); });
show("int &= int_: 0 &= 3", auto sub () { return f54(0, 3); });
show("int &= int_: 3 &= NOTHING", auto sub () { return f54(3, NOTHING); });
show("int &= int_: 3 &= 0", auto sub () { return f54(3, 0); });
show("int &= int_: 3 &= 3", auto sub () { return f54(3, 3); });
show("int &= int_: -2 &= NOTHING", auto sub () { return f54(-2, NOTHING); });
show("int &= int_: -2 &= 0", auto sub () { return f54(-2, 0); });
show("int &= int_: -2 &= 3", auto sub () { return f54(-2, 3); });
show("int_ &= int: NOTHING &= 0", auto sub () { return f55(NOTHING, 0); });
show("int_ &= int: NOTHING &= 3", auto sub () { return f55(NOTHING, 3); });
show("int_ &= int: NOTHING &= -2", auto sub () { return f55(NOTHING, -2); });
show("int_ &= int: 0 &= 0", auto sub () { return f55(0, 0); });
show("int_ &= int: 0 &= 3", auto sub () { return f55(0, 3); });
show("int_ &= int: 0 &= -2", auto sub () { return f55(0, -2); });
show("int_ &= int: 3 &= 0", auto sub () { return f55(3, 0); });
show("int_ &= int: 3 &= 3", auto sub () { return f55(3, 3); });
show("int_ &= int: 3 &= -2", auto sub () { return f55(3, -2); });
show("int_ &= int_: NOTHING &= NOTHING", auto sub () { return f56(NOTHING, NOTHING); });
show("int_ &= int_: NOTHING &= 0", auto sub () { return f56(NOTHING, 0); });
show("int_ &= int_: NOTHING &= 3", auto sub () { return f56(NOTHING, 3); });
show("int_ &= int_: 0 &= NOTHING", auto sub () { return f56(0, NOTHING); });
show("int_ &= int_: 0 &= 0", auto sub () { return f56(0, 0); });
show("int_ &= int_: 0 &= 3", auto sub () { return f56(0, 3); });
show("int_ &= int_: 3 &= NOTHING", auto sub () { return f56(3, NOTHING); });
show("int_ &= int_: 3 &= 0", auto sub () { return f56(3, 0); });
show("int_ &= int_: 3 &= 3", auto sub () { return f56(3, 3); });
show("int |= int: 0 |= 0", auto sub () { return f57(0, 0); });
show("int |= int: 0 |= 3", auto sub () { return f57(0, 3); });
show("int |= int: 0 |= -2", auto sub () { return f57(0, -2); });
show("int |= int: 3 |= 0", auto sub () { return f57(3, 0); });
show("int |= int: 3 |= 3", auto sub () { return f57(3, 3); });
show("int |= int: 3 |= -2", auto sub () { return f57(3, -2); });
show("int |= int: -2 |= 0", auto sub () { return f57(-2, 0); });
show("int |= int: -2 |= 3", auto sub () { return f57(-2, 3); });
show("int |= int: -2 |= -2", auto sub () { return f57(-2, -2); });
show("int |= int_: 0 |= NOTHING", auto sub () { return f58(0, NOTHING); });
show("int |= int_: 0 |= 0", auto sub () { return f58(0, 0); });
show("int |= int_: 0 |= 3", auto sub () { return f58(0, 3); });
show("int |= int_: 3 |= NOTHING", auto sub () { return f58(3, NOTHING); });
show("int |= int_: 3 |= 0", auto sub () { return f58(3, 0); });
show("int |= int_: 3 |= 3", auto sub () { return f58(3, 3); });
show("int |= int_: -2 |= NOTHING", auto sub () { return f58(-2, NOTHING); });
show("int |= int_: -2 |= 0", auto sub () { return f58(-2, 0); });
show("int |= int_: -2 |= 3", auto sub () { return f58(-2, 3); });
show("int_ |= int: NOTHING |= 0", auto sub () { return f59(NOTHING, 0); });
show("int_ |= int: NOTHING |= 3", auto sub () { return f59(NOTHING, 3); });
show("int_ |= int: NOTHING |= -2", auto sub () { return f59(NOTHING, -2); });
show("int_ |= int: 0 |= 0", auto sub () { return f59(0, 0); });
show("int_ |= int: 0 |= 3", auto sub () { return f59(0, 3); });
show("int_ |= int: 0 |= -2", auto sub () { return f59(0, -2); });
show("int_ |= int: 3 |= 0", auto sub () { return f59(3, 0); });
show("int_ |= int: 3 |= 3", auto sub () { return f59(3, 3); });
show("int_ |= int: 3 |= -2", auto sub () { return f59(3, -2); });
show("int_ |= int_: NOTHING |= NOTHING", auto sub () { return f60(NOTHING, NOTHING); });
show("int_ |= int_: NOTHING |= 0", auto sub () { return f60(NOTHING, 0); });
show("int_ |= int_: NOTHING |= 3", auto sub () { return f60(NOTHING, 3); });
show("int_ |= int_: 0 |= NOTHING", auto sub () { return f60(0, NOTHING); });
show("int_ |= int_: 0 |= 0", auto sub () { return f60(0, 0); });
show("int_ |= int_: 0 |= 3", auto sub () { return f60(0, 3); });
show("int_ |= int_: 3 |= NOTHING", auto sub () { return f60(3, NOTHING); });
show("int_ |= int_: 3 |= 0", auto sub () { return f60(3, 0); });
show("int_ |= int_: 3 |= 3", auto sub () { return f60(3, 3); });
show("int ^= int: 0 ^= 0", auto sub () { return f61(0, 0); });
show("int ^= int: 0 ^= 3", auto sub () { return f61(0, 3); });
show("int ^= int: 0 ^= -2", auto sub () { return f61(0, -2); });
show("int ^= int: 3 ^= 0", auto sub () { return f61(3, 0); });
show("int ^= int: 3 ^= 3", auto sub () { return f61(3, 3); });
show("int ^= int: 3 ^= -2", auto sub () { return f61(3, -2); });
show("int ^= int: -2 ^= 0", auto sub () { return f61(-2, 0); });
show("int ^= int: -2 ^= 3", auto sub () { return f61(-2, 3); });
show("int ^= int: -2 ^= -2", auto sub () { return f61(-2, -2); });
show("int ^= int_: 0 ^= NOTHING", auto sub () { return f62(0, NOTHING); });
show("int ^= int_: 0 ^= 0", auto sub () { return f62(0, 0); });
show("int ^= int_: 0 ^= 3", auto sub () { return f62(0, 3); });
show("int ^= int_: 3 ^= NOTHING", auto sub () { return f62(3, NOTHING); });
show("int ^= int_: 3 ^= 0", auto sub () { return f62(3, 0); });
show("int ^= int_: 3 ^= 3", auto sub () { return f62(3, 3); });
show("int ^= int_: -2 ^= NOTHING", auto sub () { return f62(-2, NOTHING); });
show("int ^= int_: -2 ^= 0", auto sub () { return f62(-2, 0); });
show("int ^= int_: -2 ^= 3", auto sub () { return f62(-2, 3); });
show("int_ ^= int: NOTHING ^= 0", auto sub () { return f63(NOTHING, 0); });
show("int_ ^= int: NOTHING ^= 3", auto sub () { return f63(NOTHING, 3); });
show("int_ ^= int: NOTHING ^= -2", auto sub () { return f63(NOTHING, -2); });
show("int_ ^= int: 0 ^= 0", auto sub () { return f63(0, 0); });
show("int_ ^= int: 0 ^= 3", auto sub () { return f63(0, 3); });
show("int_ ^= int: 0 ^= -2", auto sub () { return f63(0, -2); });
show("int_ ^= int: 3 ^= 0", auto sub () { return f63(3, 0); });
show("int_ ^= int: 3 ^= 3", auto sub () { return f63(3, 3); });
show("int_ ^= int: 3 ^= -2", auto sub () { return f63(3, -2); });
show("int_ ^= int_: NOTHING ^= NOTHING", auto sub () { return f64(NOTHING, NOTHING); });
show("int_ ^= int_: NOTHING ^= 0", auto sub () { return f64(NOTHING, 0); });
show("int_ ^= int_: NOTHING ^= 3", auto sub () { return f64(NOTHING, 3); });
show("int_ ^= int_: 0 ^= NOTHING", auto sub () { return f64(0, NOTHING); });
show("int_ ^= int_: 0 ^= 0", auto sub () { return f64(0, 0); });
show("int_ ^= int_: 0 ^= 3", auto sub () { return f64(0, 3); });
show("int_ ^= int_: 3 ^= NOTHING", auto sub () { return f64(3, NOTHING); });
show("int_ ^= int_: 3 ^= 0", auto sub () { return f64(3, 0); });
show("int_ ^= int_: 3 ^= 3", auto sub () { return f64(3, 3); });
show("int <<= int: 0 <<= 0", auto sub () { return f65(0, 0); });
show("int <<= int: 0 <<= 3", auto sub () { return f65(0, 3); });
show("int <<= int: 0 <<= -2", auto sub () { return f65(0, -2); });
show("int <<= int: 3 <<= 0", auto sub () { return f65(3, 0); });
show("int <<= int: 3 <<= 3", auto sub () { return f65(3, 3); });
show("int <<= int: 3 <<= -2", auto sub () { return f65(3, -2); });
show("int <<= int: -2 <<= 0", auto sub () { return f65(-2, 0); });
show("int <<= int: -2 <<= 3", auto sub () { return f65(-2, 3); });
show("int <<= int: -2 <<= -2", auto sub () { return f65(-2, -2); });
show("int <<= int_: 0 <<= NOTHING", auto sub () { return f66(0, NOTHING); });
show("int <<= int_: 0 <<= 0", auto sub () { return f66(0, 0); });
show("int <<= int_: 0 <<= 3", auto sub () { return f66(0, 3); });
show("int <<= int_: 3 <<= NOTHING", auto sub () { return f66(3, NOTHING); });
show("int <<= int_: 3 <<= 0", auto sub () { return f66(3, 0); });
show("int <<= int_: 3 <<= 3", auto sub () { return f66(3, 3); });
show("int <<= int_: -2 <<= NOTHING", auto sub () { return f66(-2, NOTHING); });
show("int <<= int_: -2 <<= 0", auto sub () { return f66(-2, 0); });
show("int <<= int_: -2 <<= 3", auto sub () { return f66(-2, 3); });
show("int_ <<= int: NOTHING <<= 0", auto sub () { return f67(NOTHING, 0); });
show("int_ <<= int: NOTHING <<= 3", auto sub () { return f67(NOTHING, 3); });
show("int_ <<= int: NOTHING <<= -2", auto sub () { return f67(NOTHING, -2); });
show("int_ <<= int: 0 <<= 0", auto sub () { return f67(0, 0); });
show("int_ <<= int: 0 <<= 3", auto sub () { return f67(0, 3); });
show("int_ <<= int: 0 <<= -2", auto sub () { return f67(0, -2); });
show("int_ <<= int: 3 <<= 0", auto sub () { return f67(3, 0); });
show("int_ <<= int: 3 <<= 3", auto sub () { return f67(3, 3); });
show("int_ <<= int: 3 <<= -2", auto sub () { return f67(3, -2); });
show("int_ <<= int_: NOTHING <<= NOTHING", auto sub () { return f68(NOTHING, NOTHING); });
show("int_ <<= int_: NOTHING <<= 0", auto sub () { return f68(NOTHING, 0); });
show("int_ <<= int_: NOTHING <<= 3", auto sub () { return f68(NOTHING, 3); });
show("int_ <<= int_: 0 <<= NOTHING", auto sub () { return f68(0, NOTHING); });
show("int_ <<= int_: 0 <<= 0", auto sub () { return f68(0, 0); });
show("int_ <<= int_: 0 <<= 3", auto sub () { return f68(0, 3); });
show("int_ <<= int_: 3 <<= NOTHING", auto sub () { return f68(3, NOTHING); });
show("int_ <<= int_: 3 <<= 0", auto sub () { return f68(3, 0); });
show("int_ <<= int_: 3 <<= 3", auto sub () { return f68(3, 3); });
show("int >>= int: 0 >>= 0", auto sub () { return f69(0, 0); });
show("int >>= int: 0 >>= 3", auto sub () { return f69(0, 3); });
show("int >>= int: 0 >>= -2", auto sub () { return f69(0, -2); });
show("int >>= int: 3 >>= 0", auto sub () { return f69(3, 0); });
show("int >>= int: 3 >>= 3", auto sub () { return f69(3, 3); });
show("int >>= int: 3 >>= -2", auto sub () { return f69(3, -2); });
show("int >>= int: -2 >>= 0", auto sub () { return f69(-2, 0); });
show("int >>= int: -2 >>= 3", auto sub () { return f69(-2, 3); });
show("int >>= int: -2 >>= -2", auto sub () { return f69(-2, -2); });
show("int >>= int_: 0 >>= NOTHING", auto sub () { return f70(0, NOTHING); });
show("int >>= int_: 0 >>= 0", auto sub () { return f70(0, 0); });
show("int >>= int_: 0 >>= 3", auto sub () { return f70(0, 3); });
show("int >>= int_: 3 >>= NOTHING", auto sub () { return f70(3, NOTHING); });
show("int >>= int_: 3 >>= 0", auto sub () { return f70(3, 0); });
show("int >>= int_: 3 >>= 3", auto sub () { return f70(3, 3); });
show("int >>= int_: -2 >>= NOTHING", auto sub () { return f70(-2, NOTHING); });
show("int >>= int_: -2 >>= 0", auto sub () { return f70(-2, 0); });
show("int >>= int_: -2 >>= 3", auto sub () { return f70(-2, 3); });
show("int_ >>= int: NOTHING >>= 0", auto sub () { return f71(NOTHING, 0); });
show("int_ >>= int: NOTHING >>= 3", auto sub () { return f71(NOTHING, 3); });
show("int_ >>= int: NOTHING >>= -2", auto sub () { return f71(NOTHING, -2); });
show("int_ >>= int: 0 >>= 0", auto sub () { return f71(0, 0); });
show("int_ >>= int: 0 >>= 3", auto sub () { return f71(0, 3); });
show("int_ >>= int: 0 >>= -2", auto sub () { return f71(0, -2); });
show("int_ >>= int: 3 >>= 0", auto sub () { return f71(3, 0); });
show("int_ >>= int: 3 >>= 3", auto sub () { return f71(3, 3); });
show("int_ >>= int: 3 >>= -2", auto sub () { return f71(3, -2); });
show("int_ >>= int_: NOTHING >>= NOTHING", auto sub () { return f72(NOTHING, NOTHING); });
show("int_ >>= int_: NOTHING >>= 0", auto sub () { return f72(NOTHING, 0); });
show("int_ >>= int_: NOTHING >>= 3", auto sub () { return f72(NOTHING, 3); });
show("int_ >>= int_: 0 >>= NOTHING", auto sub () { return f72(0, NOTHING); });
show("int_ >>= int_: 0 >>= 0", auto sub () { return f72(0, 0); });
show("int_ >>= int_: 0 >>= 3", auto sub () { return f72(0, 3); });
show("int_ >>= int_: 3 >>= NOTHING", auto sub () { return f72(3, NOTHING); });
show("int_ >>= int_: 3 >>= 0", auto sub () { return f72(3, 0); });
show("int_ >>= int_: 3 >>= 3", auto sub () { return f72(3, 3); });
