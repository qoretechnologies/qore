# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A return in a handler run past the inline cap of the lowering of handlers ends the code around the handler's run as
# a return statement there would (#6552); see HandlerReturnPastCapFlow.qc.  The nested handlers are written
# without indentation: each "on_exit {" line opens a handler in the body of the one before.

%modern
%enable-all-warnings
our list<string> lg = ();
class T {
    public { string n; }
    constructor(string n) { self.n = n; }
    destructor() { lg += "~" + n; }
}
string sub show(auto v) {
    return v.typeCode() == NT_OBJECT ? "T:" + v.n : sprintf("%y", v);
}

# handlers nested 16 deep in a block of the function, which ends; the statements after it
auto sub blk16(int x) {
    {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {

if (x == 16) return "h16";
}
if (x == 15) return "h15";
}
}
}
}
}
}
}
}
}
}
}
}
}
}
if (x == 1) return "h1";
}
    }
    lg += "after-block";
    return "body";
}
# handlers nested 17 deep in a block of the function, which ends; the statements after it
auto sub blk17(int x) {
    {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {

if (x == 17) return "h17";
}
if (x == 16) return "h16";
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
if (x == 1) return "h1";
}
    }
    lg += "after-block";
    return "body";
}
# handlers nested 20 deep in a block of the function, which ends; the statements after it
auto sub blk20(int x) {
    {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {

if (x == 20) return "h20";
}
if (x == 19) return "h19";
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
if (x == 1) return "h1";
}
    }
    lg += "after-block";
    return "body";
}
# handler k registers the next one in a block of its own, and a statement follows the block
auto sub aft16(int x) {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
if (x == 16) return "h16";
} lg += "after16"; }
if (x == 15) return "h15";
} lg += "after15"; }
} lg += "after14"; }
} lg += "after13"; }
} lg += "after12"; }
} lg += "after11"; }
} lg += "after10"; }
} lg += "after9"; }
} lg += "after8"; }
} lg += "after7"; }
} lg += "after6"; }
} lg += "after5"; }
} lg += "after4"; }
} lg += "after3"; }
if (x == 2) return "h2";
} lg += "after2"; }
if (x == 1) return "h1";
} lg += "after1"; }
    return "body";
}
# handler k registers the next one in a block of its own, and a statement follows the block
auto sub aft17(int x) {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
if (x == 17) return "h17";
} lg += "after17"; }
if (x == 16) return "h16";
} lg += "after16"; }
} lg += "after15"; }
} lg += "after14"; }
} lg += "after13"; }
} lg += "after12"; }
} lg += "after11"; }
} lg += "after10"; }
} lg += "after9"; }
} lg += "after8"; }
} lg += "after7"; }
} lg += "after6"; }
} lg += "after5"; }
} lg += "after4"; }
} lg += "after3"; }
if (x == 2) return "h2";
} lg += "after2"; }
if (x == 1) return "h1";
} lg += "after1"; }
    return "body";
}
# a break leaves a block of a loop whose handler returns, in a handler past the cap
auto sub brk(int x) {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
while (True) {
    on_exit { if (x == 1) return "brk"; }
    lg += "loop";
    break;
}
lg += "after-loop";
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
    return "body";
}
# a continue leaves a block of a loop whose handler returns in the second iteration
auto sub cnt(int x) {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
for (int i = 0; i < 3; ++i) {
    on_exit { if (x == 1 && i == 1) return "cnt" + i; }
    lg += "i" + i;
    continue;
}
lg += "after-loop";
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
    return "body";
}
# a block with a local and handlers past the cap: the handlers, then the local, then the return value
auto sub bord(int x) {
    {
        T l("loc");
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
lg += "h17"; if (x == 1) return new T("ret17");
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
    }
    lg += "after-block";
    return new T("body");
}
# a handler past the cap returns in the first iteration of a loop of a handler
auto sub lp(int x) {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
for (int i = 0; i < 3; ++i) { on_exit { if (x == 1) return new T("lp-" + i); } }
lg += "after-loop";
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
    return new T("lp-body");
}
# a function whose own handlers past the cap return a value inside it
string sub inner(int x) {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {

if (x == 17) return "h17";
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
    return "inner-body";
}
# handlers past the cap call it: the code after the block runs, unless the caller's own handler returns
auto sub nst(int x) {
    {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
lg += inner(x == 2 ? 17 : 0);
if (x == 17) return "h17";
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
    }
    lg += "after-block";
    return "body";
}
# a closure called from a handler past the cap returns from its own handler
auto sub cls(int x) {
    code c = string sub () { on_exit { return "c-handler"; } return "c-body"; };
    {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
lg += c();
if (x == 17) return "h17";
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
    }
    lg += "after-block";
    return "body";
}
# an on_error handler, run out of line, with handlers nested 16 deep in a block and a statement after it
auto sub hf16(int x) {
    on_error {
        {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {

if (x == 16) return "h16";
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
        }
        lg += "after-block";
        return "on_error";
    }
    throw "ERR";
}
# an on_error handler, run out of line, with handlers nested 17 deep in a block and a statement after it
auto sub hf17(int x) {
    on_error {
        {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {

if (x == 17) return "h17";
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
        }
        lg += "after-block";
        return "on_error";
    }
    throw "ERR";
}
# handlers nested 20 deep in a block return integers
auto sub bv(int x) {
    {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
if (x == 1) return 0x655265526552; if (x == 2) return 0x613761376137; if (x == 3) return 8;
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
    }
    lg += "after-block";
    return "body";
}
# a function for the threads, with a log of its own
auto sub thr(int x) {
    list<string> l = ();
    {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {

if (x == 17) return "h17";
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
    }
    l += "after-block";
    return l;
}

sub run(string label, code f, list<int> args) {
    foreach int x in (args) {
        for (int i = 0; i < 2; ++i) {
            lg = ();
            string v;
            try {
                v = show(f(x));
            } catch (hash<ExceptionInfo> ex) {
                v = "ex:" + ex.err;
            }
            printf("%s(%d)[%d]: %s %y\n", label, x, i, v, lg);
        }
    }
}

run("blk16", \blk16(), (0, 1, 15, 16));
run("blk17", \blk17(), (0, 1, 16, 17));
run("blk20", \blk20(), (0, 1, 19, 20));
run("aft16", \aft16(), (0, 1, 2, 15, 16));
run("aft17", \aft17(), (0, 1, 2, 16, 17));
run("brk", \brk(), (0, 1));
run("cnt", \cnt(), (0, 1));
run("nst", \nst(), (0, 2, 17));
run("cls", \cls(), (0, 17));
run("hf16", \hf16(), (0, 16));
run("hf17", \hf17(), (0, 17));

# two threads call thr() 200 times each with arguments that return from a handler past the cap and that do not
hash<string, int> thr_results();
Mutex thr_m();
Counter thr_c(2);
for (int t = 0; t < 2; ++t) {
    background sub () {
        on_exit thr_c.dec();
        for (int i = 0; i < 200; ++i) {
            string r = sprintf("%y/%y", thr(17), thr(0));
            thr_m.lock();
            on_exit thr_m.unlock();
            ++thr_results{r};
        }
    }();
}
thr_c.waitForZero();
printf("thr: %y\n", thr_results);
run("bv", \bv(), (0, 1, 2, 3));
run("bord", \bord(), (0, 1));
run("lp", \lp(), (0, 1));
