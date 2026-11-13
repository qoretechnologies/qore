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
