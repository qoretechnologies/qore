# probes of closure-arg-mismatch-tiered.qtest (qore#5864): a closure called with an argument that its signature does
# not accept raises RUNTIME-OVERLOAD-ERROR, as a call of a function with the same signature does, also when the
# closure's call is inlined at the IR tier of tiered execution; its body does not run then.  probes() returns one line
# per pass, and each pass first calls every closure with matching arguments, so that the code runs on every tier with
# tiered execution

%modern

class Accumulator {
    private { int n; }
    public code adder() { return int sub (int a) { n += a; return a; }; }
    public int count() { return n; }
}

string sub t(code c) {
    try {
        return sprintf("%y", c());
    } catch (hash<ExceptionInfo> ex) {
        # the depth of the call stack: the error is raised in the caller's frame, as in the AST interpreter
        return sprintf("%s %s (%d)", ex.err, (ex.desc =~ x/^([^;]*)/)[0], ex.callstack.size());
    }
}

string sub probes() {
    int calls = 0;
    code one = int sub (int a) { ++calls; return a; };
    code two = string sub (int a, string b) { ++calls; return sprintf("%d%s", a, b); };
    code nine = int sub (int a, int b, int c, int d, int e, int f, int g, int h, int i) {
        ++calls;
        return a + b + c + d + e + f + g + h + i;
    };
    # other parameter kinds: a reference, a soft type with a conversion, a variadic closure and a typed list
    code ref = int sub (reference<int> r) { ++calls; return r; };
    code soft = int sub (softint a) { ++calls; return a; };
    code var = int sub (int a, ...) { ++calls; return a + argv.size(); };
    code ints = int sub (list<int> l) { ++calls; return l.size(); };
    Accumulator acc();
    code method_closure = acc.adder();
    string rv;
    for (int i = 0; i < 8; ++i) {
        int x = 1;
        string s = "s";
        rv += sprintf("%d: %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; calls %d, count %d\n", i,
            t(auto sub () { return one(13); }),
            t(auto sub () { return one("l"); }),
            t(auto sub () { return two(1, "x"); }),
            t(auto sub () { return two(1, 2.5); }),
            t(auto sub () { return nine(1, 2, 3, 4, 5, 6, 7, 8, 9); }),
            t(auto sub () { return nine(1, 2, 3, 4, 5, 6, 7, 8, "9"); }),
            t(auto sub () { return method_closure(1); }),
            t(auto sub () { return method_closure("x"); }),
            t(auto sub () { return method_closure(<a1b2>); }),
            t(auto sub () { return ref(\x); }),
            t(auto sub () { return ref(\s); }),
            t(auto sub () { return soft("2"); }),
            t(auto sub () { return soft(<a1b2>); }),
            t(auto sub () { return var(1, 2, 3); }),
            t(auto sub () { return var("1", 2); }),
            t(auto sub () { return ints((1, 2)); }),
            t(auto sub () { return ints(("a",)); }),
            calls, acc.count());
        if (x != 1 || s != "s") {
            rv += "references changed\n";
        }
        calls = 0;
    }
    return rv;
}
