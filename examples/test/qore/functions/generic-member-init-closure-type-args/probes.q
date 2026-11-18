# probes of generic-member-init-closure-type-args.qtest (qore#5865): the explicit type arguments of a call in a closure
# in a member initializer are kept in qcc-compiled code, whose member initializers are serialized with the closure's
# IR: function, static method, method and self method calls, with a type argument naming the class's type parameter
# (identity<T>(a)) and a concrete one (identity<int>(a)), in a generic and in a plain class, in a member initializer
# and in a closure created in a method; probes() returns one line per pass, so that the code runs on every tier with
# tiered execution

%modern

X sub identity<X>(X x) { return x; }

# a default value that has the text of the type argument marker of serialized calls
string sub default_marked(int x, string s = "\n@qore-aot-call-type-args-v1:1\nint") {
    return sprintf("%d%d", x, s.size());
}

class Util {
    public {}
    static X sid<X>(X x) { return x; }
    X mid<X>(X x) { return x; }
}

class Box<T> {
    public {
        code param = auto sub (auto a) { return identity<T>(a); };
        code concrete = auto sub (auto a) { return identity<int>(a); };
        code typed = auto sub (T a) { return a; };
        code stat = auto sub (auto a) { return Util::sid<T>(a); };
        # a concrete type argument: one naming T finds no variant in any mode (qore#5892)
        code meth = auto sub (auto a) { Util u(); return u.mid<int>(a); };
        code selfm = auto sub (auto a) { return own<T>(a); };
    }

    public X own<X>(X x) { return x; }

    public code fromMethod() {
        return auto sub (auto a) { return identity<T>(a); };
    }
}

class Plain {
    public {
        # not in a closure: a method call with an explicit type argument in the initializer itself, which converts
        # its argument: 12 (int), and "12" (string) if the type argument is lost
        auto inline_value = (new Util()).mid<softint>("12");
        code marked = auto sub (int a) { return default_marked(a); };
        code concrete = auto sub (auto a) { return identity<string>(a); };
        code stat = auto sub (auto a) { return Util::sid<string>(a); };
        code meth = auto sub (auto a) { Util u(); return u.mid<string>(a); };
        code selfm = auto sub (auto a) { return own<string>(a); };
    }

    public X own<X>(X x) { return x; }
}

auto sub t(code c, auto v) {
    try {
        auto rv = c(v);
        return sprintf("%y %s", rv, rv.fullType());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub probes() {
    Box<int> bi();
    Box<string> bs();
    Plain p();
    string rv;
    for (int i = 0; i < 8; ++i) {
        rv += sprintf("%d: %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; "
            "%s; %s; %s\n", i,
            t(bi.param, 1), t(bi.param, "s"),
            t(bs.param, "s"), t(bs.param, 1.5),
            t(bi.concrete, 2), t(bi.concrete, "s"),
            t(bi.typed, 3), t(bi.typed, "s"),
            t(bi.fromMethod(), 4), t(bi.fromMethod(), "s"),
            t(p.concrete, "p"), t(p.concrete, 5),
            t(bi.stat, 6), t(bi.stat, "s"), t(bi.meth, 7), t(bi.meth, "s"), t(bi.selfm, 8), t(bi.selfm, "s"),
            t(p.stat, "q"), t(p.stat, 9), t(p.meth, "r"), t(p.meth, 10), t(p.selfm, "u"), t(p.selfm, 11),
            sprintf("%y %s", p.inline_value, p.inline_value.fullType()), t(p.marked, 13));
    }
    return rv;
}
