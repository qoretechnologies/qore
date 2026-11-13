%modern
auto sub f(*int x, *int y) { return x + y; }
auto sub g(*float x, int y) { return x + y; }
auto sub h(*int x, *int y) { return (x + y,); }
auto sub mu(*float x, int y) { return x * y; }
auto sub dv(*int x, int y) { return x / y; }
auto sub mi(*int x, int y) { return x - y; }
auto sub mi2(int x, *int y) { return x - y; }
auto sub mu2(*int x, *int y) { return x * y; }
auto sub ad2(*float x, *float y) { return x + y; }
auto sub ad3(*int x, *float y) { return x + y; }
printf("%y %y %y\n", f(NOTHING, NOTHING), g(NOTHING, 2), h(NOTHING, NOTHING));
printf("%y %y %y %y %y\n", mu(NOTHING, 2), dv(NOTHING, 2), mi(NOTHING, 2), mi2(2, NOTHING), mu2(NOTHING, NOTHING));
printf("%y %y %y\n", ad2(NOTHING, NOTHING), ad3(NOTHING, 1.5), ad3(2, NOTHING));
printf("%y %y\n", f(1, 2), f(NOTHING, 3));

# other shapes: signed zero, infinities, a comparison, an object member and a global
float sub bigf() { return 1.0e308; }
auto sub sz(*float x, float y) { return x + y; }
auto sub sz2(*float x, float y) { return x * y; }
auto sub cmp(*int x, int y) { return (x == y, x < y, x <=> y); }
class Holder {
    public {
        *int a;
        *int b;
    }
}
auto sub member(Holder h) { return h.a + h.b; }
our *int ga;
our *int gb;
auto sub globals() { return ga + gb; }
printf("%y %y %y %y\n", sz(NOTHING, -0.0), sz2(NOTHING, -0.0), sz(NOTHING, bigf() * 10.0), sz2(NOTHING, -bigf() * 10.0));
printf("%y %y\n", cmp(NOTHING, 0), cmp(2, 2));
Holder h();
printf("%y %y\n", member(h), globals());
h.a = 3;
ga = 4;
printf("%y %y\n", member(h), globals());
