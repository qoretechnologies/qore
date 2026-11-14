# probes of member-init-closure-static-call.qtest (qore#5897): a static method call in a closure of a member
# initializer, which an AOT image reads before the methods of the class called are attached, is made also when the
# closure is compiled to native code: plain, with a default argument, generic, in a try block (an invoke), and one
# that raises; probes() returns one line per pass, so that the code runs on every tier with tiered execution

%modern

class Util {
    public {}
    static string sn(int x) { return sprintf("%d", x); }
    static string sd(int x, string s = "abc") { return sprintf("%d%d", x, s.size()); }
    static X sid<X>(X x) { return x; }
    static int raises(int x) { throw "UTIL-ERROR", sprintf("%d", x); }
    static string sref(reference<int> r) { r += 10; return sprintf("%d", r); }
}

class P {
    public {
        code plain = auto sub (int a) { return Util::sn(a); };
        code dflt = auto sub (int a) { return Util::sd(a); };
        code generic = auto sub (auto a) { return Util::sid(a); };
        code invoke = auto sub (int a) {
            try {
                return Util::sn(a + 1);
            } catch (hash<ExceptionInfo> ex) {
                return ex.err;
            }
        };
        code raising = auto sub (int a) {
            try {
                return Util::raises(a);
            } catch (hash<ExceptionInfo> ex) {
                return ex.err + " " + ex.desc;
            }
        };
        code mismatch = auto sub (auto a) { return Util::sn(a); };
        # a reference argument: the variable of the closure is updated
        code ref = auto sub (int a) { int r = a; string s = Util::sref(\r); return sprintf("%s/%d", s, r); };
    }
}

auto sub t(code c, auto v) {
    try {
        auto rv = c(v);
        return sprintf("%y", rv);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub probes() {
    P p();
    string rv;
    for (int i = 0; i < 8; ++i) {
        rv += sprintf("%d: %s; %s; %s; %s; %s; %s; %s; %s\n", i, t(p.plain, 1), t(p.dflt, 2), t(p.generic, "g"),
            t(p.invoke, 3), t(p.raising, 4), t(p.mismatch, 5), t(p.mismatch, "x"), t(p.ref, 6));
    }
    return rv;
}
