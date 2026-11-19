# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A handler's return is kept when a later handler of its block completes normally (#6677); see
# handler-return-later-handler.qtest

%modern
%enable-all-warnings
our list<string> log = ();
class T { public { string n; } constructor(string n) { self.n = n; } destructor() { log += "~" + n; } }

# the issue: two on_exit handlers of a block of a loop that is left normally; the first one to run returns
string sub issue() {
    int n = 0;
    while (n < 3) {
        ++n;
        if (n == 2) {
            on_exit { log += "second"; }
            on_exit { log += "first"; return "handler"; }
        }
    }
    return "end";
}
# a block left normally whose handler returns, then the code after it
auto sub blk() {
    T l("blk-l");
    if (True) {
        T b("blk-b");
        on_exit { log += "second"; }
        on_exit { log += "first"; return new T("blk-h"); }
        log += "body";
    }
    log += "never";
    return new T("blk-end");
}
# the function's own block ends without a return
auto sub top() {
    T l("top-l");
    on_exit { log += "second"; }
    on_exit { log += "first"; return new T("top-h"); }
    log += "body";
}
# three handlers: the one run first, second or last returns
auto sub th1() {
    if (True) {
        on_exit { log += "h3"; }
        on_exit { log += "h2"; }
        on_exit { log += "h1"; return new T("th1"); }
    }
    return new T("th1-end");
}
auto sub th2() {
    if (True) {
        on_exit { log += "h3"; }
        on_exit { log += "h2"; return new T("th2"); }
        on_exit { log += "h1"; }
    }
    return new T("th2-end");
}
auto sub th3() {
    if (True) {
        on_exit { log += "h3"; return new T("th3"); }
        on_exit { log += "h2"; }
        on_exit { log += "h1"; }
    }
    return new T("th3-end");
}
# a later handler's return replaces the value; one that completes normally after it keeps it
auto sub rr() {
    if (True) {
        on_exit { log += "h4"; }
        on_exit { log += "h3"; return new T("rr-later"); }
        on_exit { log += "h2"; }
        on_exit { log += "h1"; return new T("rr-earlier"); }
    }
    return new T("rr-end");
}
# on_success, then on_exit
auto sub so() {
    if (True) {
        on_exit { log += "exit"; }
        on_success { log += "success"; return new T("so-s"); }
    }
    return new T("so-end");
}
# on_exit, then on_success
auto sub os() {
    if (True) {
        on_success { log += "success"; }
        on_exit { log += "exit"; return new T("os-x"); }
    }
    return new T("os-end");
}
# on_exit, then an on_error that does not run
auto sub oe() {
    if (True) {
        on_error { log += "error"; }
        on_exit { log += "exit"; return new T("oe-x"); }
        on_error { log += "error2"; }
    }
    return new T("oe-end");
}
# an exception: the handler's return does not end it, and its value is released
auto sub ox() {
    try {
        if (True) {
            T b("ox-b");
            on_error { log += "error"; }
            on_exit { log += "exit"; return new T("ox-x"); }
            throw "OX";
        }
        log += "never";
    } catch (hash<ExceptionInfo> ex) {
        log += "catch:" + ex.err;
    }
    return new T("ox-after");
}
# on_error returns while an exception leaves the block, then on_exit completes normally
auto sub eo() {
    try {
        if (True) {
            on_exit { log += "exit"; }
            on_error { log += "error"; return new T("eo-e"); }
            throw "EO";
        }
    } catch (hash<ExceptionInfo> ex) {
        log += "catch:" + ex.err;
    }
    return new T("eo-after");
}
# a later handler raises an exception after the return
auto sub lt() {
    try {
        if (True) {
            on_exit { log += "throw"; throw "LT"; }
            on_exit { log += "first"; return new T("lt-x"); }
        }
        log += "never";
    } catch (hash<ExceptionInfo> ex) {
        log += "catch:" + ex.err;
    }
    return new T("lt-after");
}
# a later handler raises an exception after the return, with no try statement: the function raises it
auto sub ltf() {
    if (True) {
        on_exit { log += "throw"; throw "LTF"; }
        on_exit { log += "h2"; }
        on_exit { log += "first"; return new T("ltf-x"); }
    }
    log += "never";
    return new T("ltf-end");
}
# a later on_error runs after the exception that a handler raises after a return
auto sub lte() {
    try {
        if (True) {
            on_error { log += "error"; }
            on_exit { log += "throw"; throw "LTE"; }
            on_exit { log += "first"; return new T("lte-x"); }
        }
    } catch (hash<ExceptionInfo> ex) {
        log += "catch:" + ex.err;
    }
    return new T("lte-after");
}
# nested blocks: the inner block's handler returns, the inner and outer blocks' later handlers complete normally
auto sub nb() {
    if (True) {
        on_exit { log += "o2"; }
        on_exit { log += "o1"; }
        if (True) {
            on_exit { log += "i2"; }
            on_exit { log += "i1"; return new T("nb-inner"); }
        }
        log += "never";
    }
    log += "never";
    return new T("nb-end");
}
# nested blocks: the inner block's handler returns and the outer block's handler replaces it
auto sub nr() {
    if (True) {
        on_exit { log += "o2"; }
        on_exit { log += "o1"; return new T("nr-outer"); }
        if (True) {
            on_exit { log += "i2"; }
            on_exit { log += "i1"; return new T("nr-inner"); }
        }
    }
    return new T("nr-end");
}
# loops: the handler's return replaces the body's continue or break
auto sub lc() {
    for (int i = 0; i < 3; ++i) {
        T b("lc-b" + i);
        on_exit { log += "second" + i; }
        on_exit { log += "first" + i; if (i == 1) return new T("lc-h"); }
        continue;
    }
    return new T("lc-end");
}
auto sub lb() {
    foreach int i in (range(3)) {
        on_exit { log += "second" + i; }
        on_exit { log += "first" + i; if (i == 1) return new T("lb-h"); }
        if (i == 1) {
            break;
        }
    }
    return new T("lb-end");
}
# a loop whose body block is left normally, with handlers in each iteration
auto sub lw() {
    int i = 0;
    while (i < 5) {
        on_exit { log += "second" + i; ++i; }
        on_exit { log += "first" + i; if (i == 2) return new T("lw-h"); }
    }
    return new T("lw-end");
}
# a method and a static method
class C {
    auto m() {
        if (True) {
            on_exit { log += "second"; }
            on_exit { log += "first"; return new T("m-h"); }
        }
        return new T("m-end");
    }
    static auto sm() {
        if (True) {
            on_exit { log += "second"; }
            on_exit { log += "first"; return new T("sm-h"); }
        }
        return new T("sm-end");
    }
}
# a closure, and a closure called in a handler
code cl = auto sub () {
    if (True) {
        on_exit { log += "second"; }
        on_exit { log += "first"; return new T("cl-h"); }
    }
    return new T("cl-end");
};
auto sub hc() {
    if (True) {
        on_exit { log += "second"; }
        on_exit { log += "first"; return cl(); }
    }
    return new T("hc-end");
}
# in a background thread: the code after the block does not run, and the value is released when the call ends
auto sub bg_code() {
    if (True) {
        on_exit { log += "second"; }
        on_exit { log += "first"; return new T("bg-h"); }
    }
    log += "never";
}
# thread_exit in the block: the handlers run, and the returned value is released when the call ends
auto sub te_code() {
    if (True) {
        on_exit { log += "second"; }
        on_exit { log += "first"; return new T("te-h"); }
        thread_exit;
    }
    log += "never";
}
# thread_exit in a later handler after a handler's return
auto sub thx_code() {
    if (True) {
        on_exit { log += "third"; }
        on_exit { log += "second"; thread_exit; }
        on_exit { log += "first"; return new T("thx-h"); }
    }
    log += "never";
}
# runs the code in a background thread and waits for it to end
string sub in_thread(code c) {
    Counter cnt(1);
    background sub () {
        on_exit cnt.dec();
        c();
        log += "after";
    }();
    cnt.waitForZero();
    return "done";
}

sub show(string label, code c) {
    for (int i = 0; i < 2; ++i) {
        log = ();
        try {
            auto v = c();
            log += "rv:" + (v instanceof T ? v.n : (exists v ? sprintf("%s", v) : "nothing"));
        } catch (hash<ExceptionInfo> ex) {
            log += "ex:" + ex.err;
        }
        printf("%s: %y\n", label, log);
    }
}
show("issue", \issue());
show("blk", \blk());
show("top", \top());
show("th1", \th1());
show("th2", \th2());
show("th3", \th3());
show("rr", \rr());
show("so", \so());
show("os", \os());
show("oe", \oe());
show("ox", \ox());
show("eo", \eo());
show("lt", \lt());
show("ltf", \ltf());
show("lte", \lte());
show("nb", \nb());
show("nr", \nr());
show("lc", \lc());
show("lb", \lb());
show("lw", \lw());
show("m", auto sub () { C c(); return c.m(); });
show("sm", \C::sm());
show("cl", cl);
show("hc", \hc());
show("bg", string sub () { return in_thread(\bg_code()); });
show("te", string sub () { return in_thread(\te_code()); });
show("thx", string sub () { return in_thread(\thx_code()); });
