#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# the probe of CatchExitHandlerOrder.qtest: a return, break or continue out of a catch block closes the catch block
# before the on_exit handlers of the blocks enclosing it run, so a rethrow in them raises the exception of the
# enclosing catch block (qore#6017); the AST interpreter is the reference

%modern

our list<string> log;
sub rt(string tag, code c) {
    try { throw "O"; } catch (hash<ExceptionInfo> o) {
        on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += tag + "-rth:" + r.err; } }
        c();
    }
}
string sub ret_val() {
    try { throw "O"; } catch (hash<ExceptionInfo> o) {
        on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "rth:" + r.err; } }
        try { try { throw "A"; } catch (hash<ExceptionInfo> a) { throw "CALL"; } }
        catch (hash<ExceptionInfo> e) { return "ret"; }
    }
    return "none";
}
sub ret_nothing() {
    try { throw "O"; } catch (hash<ExceptionInfo> o) {
        on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "rth:" + r.err; } }
        try { throw "A"; } catch () { return; }
    }
}
sub brk() {
    try { throw "O"; } catch (hash<ExceptionInfo> o) {
        for (int i = 0; i < 3; ++i) {
            on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "rth" + i + ":" + r.err; } }
            try { throw "B" + i; } catch (hash<ExceptionInfo> b) { if (i == 1) break; continue; }
        }
    }
}
sub nested() {
    try { throw "O"; } catch (hash<ExceptionInfo> o) {
        on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "o:" + r.err; } }
        try { throw "M"; } catch (hash<ExceptionInfo> m) {
            on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "m:" + r.err; } }
            try { throw "I"; } catch (hash<ExceptionInfo> i) {
                on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "i:" + r.err; } }
                return;
            }
        }
    }
}
class D { private { string n; } constructor(string n) { self.n = n; } destructor() { log += "~" + n; } }
sub dtor() {
    try { throw "O"; } catch (hash<ExceptionInfo> o) {
        D d1("d1");
        on_exit { log += "x1"; }
        try { throw "A"; } catch (hash<ExceptionInfo> a) {
            D d2("d2");
            on_exit { log += "x2"; }
            return;
        }
    }
}
# an on_exit handler with a try statement of its own, run by a return out of a catch block in a block inside a catch
# block
string sub in_handler() {
    try { throw "O"; } catch (hash<ExceptionInfo> o) {
        {
            on_exit {
                try { throw "H"; } catch (hash<ExceptionInfo> h) {
                    on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "hh:" + r.err; } }
                }
            }
            on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "blk:" + r.err; } }
            try { throw "A"; } catch () { return "r"; }
        }
    }
    return "none";
}
sub ret_in_handler_catch() {
    try { throw "O"; } catch (hash<ExceptionInfo> o) {
        on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "outer:" + r.err; } }
        {
            on_exit {
                try { throw "H"; } catch (hash<ExceptionInfo> h) { log += "h"; }
            }
            try { throw "A"; } catch () { return; }
        }
    }
}
# the handlers of three nested catch blocks left by a return; a handler whose try statement catches an exception
# must not leave the scopes of the others unbalanced (qore#6017)
sub m000() {
    try { throw "O"; } catch (hash<ExceptionInfo> o) {
        on_exit { log += "o"; }
        try { throw "M"; } catch (hash<ExceptionInfo> m) {
            on_exit { log += "m"; }
            try { throw "I"; } catch (hash<ExceptionInfo> i) {
                on_exit { log += "i"; }
                return;
            }
        }
    }
}
sub m001() {
    try { throw "O"; } catch (hash<ExceptionInfo> o) {
        on_exit { log += "o"; }
        try { throw "M"; } catch (hash<ExceptionInfo> m) {
            on_exit { log += "m"; }
            try { throw "I"; } catch (hash<ExceptionInfo> i) {
                on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "i:" + r.err; } }
                return;
            }
        }
    }
}
sub m010() {
    try { throw "O"; } catch (hash<ExceptionInfo> o) {
        on_exit { log += "o"; }
        try { throw "M"; } catch (hash<ExceptionInfo> m) {
            on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "m:" + r.err; } }
            try { throw "I"; } catch (hash<ExceptionInfo> i) {
                on_exit { log += "i"; }
                return;
            }
        }
    }
}
sub m011() {
    try { throw "O"; } catch (hash<ExceptionInfo> o) {
        on_exit { log += "o"; }
        try { throw "M"; } catch (hash<ExceptionInfo> m) {
            on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "m:" + r.err; } }
            try { throw "I"; } catch (hash<ExceptionInfo> i) {
                on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "i:" + r.err; } }
                return;
            }
        }
    }
}
sub m100() {
    try { throw "O"; } catch (hash<ExceptionInfo> o) {
        on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "o:" + r.err; } }
        try { throw "M"; } catch (hash<ExceptionInfo> m) {
            on_exit { log += "m"; }
            try { throw "I"; } catch (hash<ExceptionInfo> i) {
                on_exit { log += "i"; }
                return;
            }
        }
    }
}
sub m101() {
    try { throw "O"; } catch (hash<ExceptionInfo> o) {
        on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "o:" + r.err; } }
        try { throw "M"; } catch (hash<ExceptionInfo> m) {
            on_exit { log += "m"; }
            try { throw "I"; } catch (hash<ExceptionInfo> i) {
                on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "i:" + r.err; } }
                return;
            }
        }
    }
}
sub m110() {
    try { throw "O"; } catch (hash<ExceptionInfo> o) {
        on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "o:" + r.err; } }
        try { throw "M"; } catch (hash<ExceptionInfo> m) {
            on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "m:" + r.err; } }
            try { throw "I"; } catch (hash<ExceptionInfo> i) {
                on_exit { log += "i"; }
                return;
            }
        }
    }
}
sub m111() {
    try { throw "O"; } catch (hash<ExceptionInfo> o) {
        on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "o:" + r.err; } }
        try { throw "M"; } catch (hash<ExceptionInfo> m) {
            on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "m:" + r.err; } }
            try { throw "I"; } catch (hash<ExceptionInfo> i) {
                on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "i:" + r.err; } }
                return;
            }
        }
    }
}

# exits whose handlers raise, rethrow, return or have handlers of their own lowered inline (qore#6017)
# throw in a handler run by a return out of a catch block, caught in the same frame
sub s2() {
    try { throw "O"; } catch (hash<ExceptionInfo> o) {
        try {
            {
                on_exit { throw "H"; }
                try { throw "A"; } catch () { return; }
            }
        } catch (hash<ExceptionInfo> h) {
            log += "h:" + h.err;
        }
        try { rethrow; } catch (hash<ExceptionInfo> r) { log += "o:" + r.err; }
    }
}
# throw in such a handler propagating out of the function: the caller's catch block must keep its exception
sub s3() {
    {
        on_exit { throw "H"; }
        try { throw "A"; } catch () { return; }
    }
}
sub s3c() {
    try { throw "C"; } catch (hash<ExceptionInfo> c) {
        try { s3(); } catch (hash<ExceptionInfo> x) { log += "x:" + x.err; }
        try { rethrow; } catch (hash<ExceptionInfo> r) { log += "c:" + r.err; }
    }
}
# bare rethrow in such a handler propagating out of the function
sub s4() {
    try { throw "O"; } catch (hash<ExceptionInfo> o) {
        {
            on_exit { rethrow; }
            try { throw "A"; } catch () { return; }
        }
    }
}
sub s4c() {
    try { throw "C"; } catch (hash<ExceptionInfo> c) {
        try { s4(); } catch (hash<ExceptionInfo> x) { log += "x:" + x.err; }
        try { rethrow; } catch (hash<ExceptionInfo> r) { log += "c:" + r.err; }
    }
}
# return inside a handler inlined at a return out of catch blocks (barrier skip)
sub s5() {
    try { throw "O"; } catch (hash<ExceptionInfo> o) {
        on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "out:" + r.err; } }
        {
            on_exit { log += "h"; return; }
            try { throw "A"; } catch () { return; }
        }
    }
}
sub s5c() {
    try { throw "C"; } catch (hash<ExceptionInfo> c) {
        s5();
        try { rethrow; } catch (hash<ExceptionInfo> r) { log += "c:" + r.err; }
    }
}
# return inside a catch block inside a handler inlined at a return out of catch blocks
sub s6() {
    try { throw "O"; } catch (hash<ExceptionInfo> o) {
        on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "out:" + r.err; } }
        {
            on_exit { try { throw "H"; } catch (hash<ExceptionInfo> h) {
                on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "hh:" + r.err; } }
                return; } }
            try { throw "A"; } catch () { return; }
        }
    }
}
sub s6c() {
    try { throw "C"; } catch (hash<ExceptionInfo> c) {
        s6();
        try { rethrow; } catch (hash<ExceptionInfo> r) { log += "c:" + r.err; }
    }
}
# break out of a catch block inside a handler loop, inlined at a return out of a catch block
sub s7() {
    try { throw "O"; } catch (hash<ExceptionInfo> o) {
        {
            on_exit {
                for (int i = 0; i < 2; ++i) {
                    on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "l" + i + ":" + r.err; } }
                    try { throw "L" + i; } catch () { if (i == 1) break; continue; }
                }
                try { rethrow; } catch (hash<ExceptionInfo> r) { log += "after:" + r.err; }
            }
            try { throw "A"; } catch () { return; }
        }
    }
}
# a return in a handler (H2) lowered at the normal end of a block in a handler (H1) that a return out of a catch
# block runs: the catch block left by the first return lies under the outer barrier
sub s8() {
    try { throw "O"; } catch (hash<ExceptionInfo> o) {
        on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "out:" + r.err; } }
        {
            on_exit {
                {
                    on_exit { log += "h2"; return; }
                    log += "h1";
                }
            }
            try { throw "A"; } catch () { return; }
        }
    }
}
sub s8c() {
    try { throw "C"; } catch (hash<ExceptionInfo> c) {
        s8();
        try { rethrow; } catch (hash<ExceptionInfo> r) { log += "c:" + r.err; }
    }
}
# same, two catch levels in the handler chain: return in a catch in H1 runs H2 whose body returns
sub s9() {
    try { throw "O"; } catch (hash<ExceptionInfo> o) {
        on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "out:" + r.err; } }
        {
            on_exit {
                {
                    on_exit { log += "h2"; return; }
                    try { throw "B"; } catch () { log += "h1"; return; }
                }
            }
            try { throw "A"; } catch () { return; }
        }
    }
}
sub s9c() {
    try { throw "C"; } catch (hash<ExceptionInfo> c) {
        s9();
        try { rethrow; } catch (hash<ExceptionInfo> r) { log += "c:" + r.err; }
    }
}
# a return in a block with on_exit inside a handler run by a return that left a block with on_exit (scope_stack cut)
sub s10() {
    {
        on_exit {
            {
                on_exit { log += "z"; }
                log += "h1";
                return;
            }
        }
        {
            on_exit { log += "y"; }
            return;
        }
    }
}
# same through catch blocks
sub s11() {
    try { throw "O"; } catch (hash<ExceptionInfo> o) {
        on_exit {
            try { throw "H"; } catch (hash<ExceptionInfo> h) {
                on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "z:" + r.err; } }
                return;
            }
        }
        try { throw "A"; } catch (hash<ExceptionInfo> a) {
            on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "y:" + r.err; } }
            return;
        }
    }
}

# exits in loops (ref foreach, value foreach, context) and handlers nested three levels inline (qore#6017)
# a break and a continue out of a catch block in a loop whose body has a local with a destructor
sub b1() {
    for (int i = 0; i < 3; ++i) {
        D d("b" + i);
        try { throw "A"; } catch (hash<ExceptionInfo> a) {
            on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "h" + i + ":" + r.err; } }
            if (i == 1) { break; }
            continue;
        }
    }
    log += "end";
}
# return in a handler run by a break out of a catch block in a ref foreach
sub b2(reference<list<int>> l) {
    foreach int v in (\l) {
        D d("r" + v);
        {
            on_exit { log += "h" + v; if (v == 2) { return; } }
            try { throw "A"; } catch () { v += 10; if (v == 11) { continue; } break; }
        }
    }
    log += "after";
}
# return in a handler run by a return out of a catch block in a value foreach over objects
sub b3() {
    foreach D o in ((new D("x"), new D("y"))) {
        D d("in");
        {
            on_exit { log += "h"; return; }
            try { throw "A"; } catch () { return; }
        }
    }
}
# return in a handler inside a context loop
sub b4() {
    context (("a": (1, 2))) {
        {
            on_exit { log += "h" + %a; return; }
            try { throw "A"; } catch () { if (%a == 1) { continue; } return; }
        }
    }
}
# three nested handler levels, innermost returns
sub b5() {
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
# a handler with a loop and break out of a catch with temporaries in the loop condition
sub b6() {
    {
        on_exit {
            foreach string s in (("a", "b", "c")) {
                on_exit { log += "l" + s; }
                try { throw s; } catch (hash<ExceptionInfo> e) { if (e.err == "b") { break; } }
            }
        }
        try { throw "A"; } catch () { return; }
    }
}
for (int n = 0; n < 2; ++n) {
    log = (); string r = ret_val(); printf("ret_val %y %y\n", log, r);
    log = (); ret_nothing(); printf("ret_nothing %y\n", log);
    log = (); brk(); printf("brk %y\n", log);
    log = (); nested(); printf("nested %y\n", log);
    log = (); dtor(); printf("dtor %y\n", log);
    log = (); r = in_handler(); printf("in_handler %y %y\n", log, r);
    log = (); ret_in_handler_catch(); printf("ret_in_handler_catch %y\n", log);
    log = (); rt("cl", sub () { try { throw "A"; } catch () { return; } }); printf("closure %y\n", log);
    foreach string f in (("m000", "m001", "m010", "m011", "m100", "m101", "m110", "m111")) {
        log = ();
        call_function(f);
        printf("%s %y\n", f, log);
    }
    foreach string f in (("s2", "s3c", "s4c", "s5c", "s6c", "s7", "s8c", "s9c", "s10", "s11")) {
        log = ();
        try {
            call_function(f);
        } catch (hash<ExceptionInfo> e) {
            log += "EXC:" + e.err + ":" + e.desc;
        }
        printf("%s %y\n", f, log);
    }
    list<int> l = (1, 2, 3);
    log = ();
    try { b1(); } catch (hash<ExceptionInfo> e) { log += "EXC:" + e.err; }
    printf("b1 %y\n", log);
    log = ();
    try { b2(\l); } catch (hash<ExceptionInfo> e) { log += "EXC:" + e.err; }
    printf("b2 %y %y\n", log, l);
    foreach string f in (("b3", "b4", "b5", "b6")) {
        log = ();
        try { call_function(f); } catch (hash<ExceptionInfo> e) { log += "EXC:" + e.err + ":" + e.desc; }
        printf("%s %y\n", f, log);
    }
}
