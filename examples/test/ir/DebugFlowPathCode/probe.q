#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# the probe of DebugFlowPathCode.qtest: code with step events, whose paths of a debugger's requests (#6201) release
# the temporaries, the locals and the handlers of the blocks that they leave, and whose code after the paths still
# has them; the AST interpreter is the reference

%modern

our list<string> log;
class D { private { string n; } constructor(string n) { self.n = n; } destructor() { log += "~" + n; } string get() {
    return n; } }
D sub mk(string n) { return new D(n); }
auto sub mka(string n) { return new D(n); }
string sub g(string a, string b) { return a + b; }

# a function local assigned in every case of a switch statement in a try statement: each case has the paths of
# its step events
string sub cases(int i) {
    string r = "";
    try {
        switch (i) {
            case 0: r = g(sprintf("a%d", i), sprintf("b%d", i)) + g(sprintf("c%d", i), sprintf("d%d", i)); break;
            case 1: r = g(sprintf("a%d", i), sprintf("b%d", i)) + g(sprintf("c%d", i), sprintf("d%d", i)); break;
            case 2: r = g(sprintf("a%d", i), sprintf("b%d", i)) + g(sprintf("c%d", i), sprintf("d%d", i)); break;
            case 3: r = g(sprintf("a%d", i), sprintf("b%d", i)) + g(sprintf("c%d", i), sprintf("d%d", i)); break;
            case 4: r = g(sprintf("a%d", i), sprintf("b%d", i)) + g(sprintf("c%d", i), sprintf("d%d", i)); break;
            case 5: r = g(sprintf("a%d", i), sprintf("b%d", i)) + g(sprintf("c%d", i), sprintf("d%d", i)); break;
            case 6: r = g(sprintf("a%d", i), sprintf("b%d", i)) + g(sprintf("c%d", i), sprintf("d%d", i)); break;
            case 7: r = g(sprintf("a%d", i), sprintf("b%d", i)) + g(sprintf("c%d", i), sprintf("d%d", i)); break;
            case 8: r = g(sprintf("a%d", i), sprintf("b%d", i)) + g(sprintf("c%d", i), sprintf("d%d", i)); break;
            case 9: r = g(sprintf("a%d", i), sprintf("b%d", i)) + g(sprintf("c%d", i), sprintf("d%d", i)); break;
            case 10: r = g(sprintf("a%d", i), sprintf("b%d", i)) + g(sprintf("c%d", i), sprintf("d%d", i)); break;
            case 11: r = g(sprintf("a%d", i), sprintf("b%d", i)) + g(sprintf("c%d", i), sprintf("d%d", i)); break;
        }
    } catch (hash<ExceptionInfo> ex) {
        r = ex.err;
    }
    return r;
}

# the temporaries of a switch statement in a loop, with objects, which the code after the paths releases
string sub loop_switch(int n) {
    string r = "";
    foreach int i in (xrange(n)) {
        r += mk("a" + i).get();
        switch (i) {
            case 1:
                r += mk("s" + i).get();
                log += "c1";
                break;
            default:
                r += mk("d" + i).get();
        }
        r += mk("b" + i).get();
        log += "e" + i;
    }
    return r;
}

# block-scoped locals in a loop, whose values the end of their block releases after the paths
sub block_locals(int n) {
    for (int i = 0; i < n; ++i) {
        {
            D d = mka("d" + i);
            log += "a" + i;
            *D e = mka("e" + i);
            log += "b" + i;
        }
        log += "c" + i;
    }
    log += "end";
}

# many locals and try statements with returns
int sub locals_tries(int n) {
    int i0 = n;
    string s1 = "v1";
    list<int> l2 = (1, 2, 3);
    hash<string, int> h3 = {"k": 3};
    int i4 = n + 4;
    string s5 = "v5";
    for (int k = 0; k < 4; ++k) {
        try {
            i0 = i0 + k;
            s1 = s1 + k;
            l2 += k;
            h3.k += k;
            if (k == 2 && n > 10) {
                return i0;
            }
        } catch (hash<ExceptionInfo> ex) {
            s5 += ex.err;
        }
        try {
            i4 = i4 + k;
            if (k == 3 && n < 0) {
                throw "NEG";
            }
        } catch (hash<ExceptionInfo> ex) {
            s5 += ex.err;
            return -1;
        }
    }
    return i0 + s1.size() + l2.size() + h3.k + i4 + s5.size();
}

# nested handlers that return, with try statements, whose paths lower the handlers of the blocks that they leave
sub nested_handlers() {
    try { throw "O"; } catch (hash<ExceptionInfo> o) {
        on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "out:" + r.err; } }
        {
            on_exit {
                try { throw "P"; } catch () {
                    {
                        on_exit {
                            {
                                on_exit { log += "h3"; return; }
                                try { throw "Q"; } catch () { log += "h2"; return; }
                            }
                        }
                        log += "h1"; return;
                    }
                }
            }
            D d("blk");
            try { throw "A"; } catch () { return; }
        }
    }
}

for (int n = 0; n < 2; ++n) {
    for (int i = 0; i < 13; ++i) {
        printf("cases %d %s\n", i, cases(i));
    }
    log = ();
    string r = loop_switch(3);
    printf("loop_switch %s %y\n", r, log);
    log = ();
    block_locals(2);
    printf("block_locals %y\n", log);
    printf("locals_tries %d %d\n", locals_tries(5), locals_tries(20));
    log = ();
    nested_handlers();
    printf("nested_handlers %y\n", log);
}
