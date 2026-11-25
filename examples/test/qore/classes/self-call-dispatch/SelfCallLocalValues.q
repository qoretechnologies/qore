%modern
# body locals of methods that are passed to calls of methods of self (m(x), Base::m(x)) and read and written after
# the calls: in loops, after try blocks, in catch blocks and on_exit handlers, with negative NaN, big ints, floats,
# strings and hashes (issue #7267); its output must be SelfCallLocalValues.expected, which the AST interpreter prints

class Base {
    auto id(auto v) { return v; }
    int twice(int i) { return i * 2; }
    thrower(int i) { if (i > 2) { throw "ERR", sprintf("i: %d", i); } }
}

class Derived inherits Base {
    # read and written after the call, in a loop
    string loop() {
        int t = 0;
        string acc = "";
        for (int i = 0; i < 4; ++i) {
            t += twice(i);
            acc += id(sprintf("%d", t));
            t += i;
        }
        return sprintf("t=%d acc=%s", t, acc);
    }

    # after a try block that catches an exception of the callee
    string afterTry() {
        int n = 0;
        int last = -1;
        for (int i = 0; i < 5; ++i) {
            try {
                thrower(i);
                n += twice(i);
            } catch (hash<ExceptionInfo> ex) {
                last = i;
                n += id(100);
            }
        }
        n += 1;
        return sprintf("n=%d last=%d", n, last);
    }

    # in a catch block and an on_exit handler
    string handlers() {
        int a = 7;
        string log = "";
        {
            on_exit {
                log += sprintf("exit:%d ", id(a));
                a += 1;
            }
            try {
                thrower(twice(a));
            } catch (hash<ExceptionInfo> ex) {
                log += sprintf("catch:%d ", id(a));
                a += 10;
            }
        }
        return sprintf("%sa=%d", log, a);
    }

    # negative NaN, big ints and floats
    string numbers() {
        float neg_nan = -@nan@;
        float f = 1.25;
        int big = 9007199254740993;
        int neg = -9223372036854775807;
        string r = sprintf("%y %y %d %d ", id(neg_nan).nanp(), id(f), id(big), Base::id(neg));
        try {
            thrower(id(1));
        } catch (hash<ExceptionInfo> ex) {
        }
        hash<auto> h = {"nan": neg_nan, "f": f, "big": big};
        big += 2;
        f *= 2;
        r += sprintf("%y %y %y %d %y", h.nan.nanp(), h.f, h.big, big, f);
        return r;
    }

    # a qualified self call, the local written after it
    string qualified() {
        int x = 3;
        int y = Base::twice(x);
        x = Base::twice(y) + x;
        return sprintf("x=%d y=%d", x, y);
    }

    # a hash local passed to a self call and changed after it
    string hashLocal() {
        hash<auto> h = {"a": 1};
        auto v = id(h);
        h.b = 2;
        return sprintf("%y %y", h, v);
    }
}

Derived d();
printf("loop: %s\n", d.loop());
printf("afterTry: %s\n", d.afterTry());
printf("handlers: %s\n", d.handlers());
printf("numbers: %s\n", d.numbers());
printf("qualified: %s\n", d.qualified());
printf("hashLocal: %s\n", d.hashLocal());
