#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# Calls through references to user functions (qore#5578): the compiled execution modes make a call through an exact
# function reference without the generic call path when the reference and the call allow it, and through the generic
# call path otherwise; the result, exceptions and call stacks of every case must be those of the AST interpreter.
#
# Prints a line for each case.  With the argument "report" (debug builds), it prints after each case whether its calls
# took the fast path (dbg_function_reference_fast_calls()) instead, or the call-reference entry of an AOT function
# ("native", dbg_function_reference_native_calls()).

%modern
%no-child-restrictions
%requires ./FunctionReferenceFastPathModule.qm

const N = 200;

const UserFunctions = {
    "inc": True, "thrower": True, "deep": True, "loop_ref": True, "call_ref": True, "call_ref_any": True,
    "call_ref0": True, "call_ref3": True, "bad_return": True, "mod_throw": True, "try_loop": True,
};

bool report = ARGV[0] == "report";

int sub inc(int x) {
    return x + 1;
}

float sub half(float f) {
    return f / 2.0;
}

string sub greet(string s) {
    return "hi " + s;
}

bool sub neg(bool b) {
    return !b;
}

list<auto> sub wrap(auto v) {
    return (v, v);
}

hash<auto> sub mkh(string k, int v) {
    return {k: v};
}

int sub zero() {
    return 42;
}

int sub add3(int a, int b, int c) {
    return a * 100 + b * 10 + c;
}

int sub thrower(int x) {
    throw "FUNCREF-ERROR", sprintf("x=%d", x), x;
}

int sub deep(int x) {
    return thrower(x) + 1;
}

int sub bad_return(string s) {
    auto v = s == "ok" ? 1 : s;
    return v;
}

int sub with_default(int x = 5) {
    return x * 3;
}

int sub over(int x) {
    return x + 1000;
}

int sub over(string s) {
    return s.size();
}

synchronized int sub sync_inc(int x) {
    return x + 1;
}

int sub soft(softint x) {
    return x + 1;
}

auto sub reads_argv(int x) {
    return (x, argv);
}

sub set_ref(reference<int> r, int v) {
    r = v;
}

int sub any_param(auto v) {
    return v.typeCode();
}

int sub list_size(list<auto> l) {
    return l.size();
}

#! an object whose destructor logs, or raises an exception
class Tmp {
    private {
        string n;
        *Queue log;
    }

    constructor(string n, *Queue log) {
        self.n = n;
        self.log = log;
    }

    destructor() {
        if (log) {
            log.push("destroy " + n);
        } else {
            throw "DESTRUCTOR-ERROR", n;
        }
    }

    string name() {
        return n;
    }
}

string sub take_tmp(Tmp t) {
    return t.name();
}

int sub take_tmp_bad_return(Tmp t) {
    auto v = t.name();
    return v;
}

# the argument is a temporary, whose reference the call gives to the callee (issue #5882)
auto sub call_tmp(code f, string n, *Queue log) {
    auto r = f(new Tmp(n, log));
    if (log) {
        log.push(sprintf("returned %y", r));
    }
    return r;
}

string sub errs(hash<ExceptionInfo> ex) {
    list<string> l = ();
    *hash<ExceptionInfo> e = ex;
    while (e) {
        push l, e.err;
        e = e.next;
    }
    return l.join(",");
}

int sub loop_ref(code f, int n) {
    int sum = 0;
    for (int i = 0; i < n; ++i) {
        sum += f(i);
    }
    return sum;
}

float sub loop_ref_float(code f, int n) {
    float sum = 0.0;
    for (int i = 0; i < n; ++i) {
        sum += f(float(i));
    }
    return sum;
}

auto sub call_ref(code f, int x) {
    return f(x);
}

auto sub call_ref_any(code f, auto x) {
    return f(x);
}

auto sub call_ref0(code f) {
    return f();
}

auto sub call_ref2(code f, auto a, auto b) {
    return f(a, b);
}

auto sub call_ref3(code f, auto a, auto b, auto c) {
    return f(a, b, c);
}

auto sub call_ref_named(code f) {
    return f(x: 7);
}

string sub stack_of(hash<ExceptionInfo> ex) {
    list<string> l = ();
    foreach hash<auto> frame in (ex.callstack) {
        if (UserFunctions{frame.function}) {
            push l, sprintf("%s:%d", frame.function, frame.line);
        }
    }
    return l.join(" <- ");
}

string sub err(hash<ExceptionInfo> ex) {
    return sprintf("%s: %s (arg %y)", ex.err, ex.desc, ex.arg);
}

int sub try_loop(code f, int n) {
    int caught = 0;
    for (int i = 0; i < n; ++i) {
        try {
            f(i);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "FUNCREF-ERROR" && ex.arg == i) {
                ++caught;
            }
        }
    }
    return caught;
}

int fast_calls_before = 0;
int native_calls_before = 0;

sub case_start() {
    # the report reads the counts of a debug build of the library
    if (report && Option::HAVE_LIBRARY_DEBUGGING) {
        fast_calls_before = call_function("dbg_function_reference_fast_calls");
        native_calls_before = call_function("dbg_function_reference_native_calls");
    }
}

sub case_end(string name, string result) {
    # the report reads the counts of a debug build of the library
    if (report && Option::HAVE_LIBRARY_DEBUGGING) {
        int calls = call_function("dbg_function_reference_fast_calls") - fast_calls_before;
        # a call through the call-reference entry of an AOT function (qore#5578) takes the fast path of AOT code
        int native = call_function("dbg_function_reference_native_calls") - native_calls_before;
        printf("%s: %s\n", name, native ? "native" : (calls ? "fast" : "generic"));
    } else {
        printf("%s: %s\n", name, result);
    }
}

sub run_case(string name, code c) {
    case_start();
    string result;
    try {
        result = c();
    } catch (hash<ExceptionInfo> ex) {
        result = "unexpected " + err(ex);
    }
    case_end(name, result);
}

# scalar and container arguments with 0, 1, 2 and 3 parameters
run_case("int", string sub () { return sprintf("%d", loop_ref(\inc(), N)); });
run_case("float", string sub () { return sprintf("%.2f", loop_ref_float(\half(), N)); });
run_case("string", string sub () {
    string s;
    for (int i = 0; i < N; ++i) {
        s = call_ref_any(\greet(), "w" + i);
    }
    return s;
});
run_case("bool", string sub () {
    int t = 0;
    for (int i = 0; i < N; ++i) {
        if (call_ref_any(\neg(), (i % 2) == 0)) {
            ++t;
        }
    }
    return sprintf("%d", t);
});
run_case("list", string sub () {
    auto v;
    for (int i = 0; i < N; ++i) {
        v = call_ref_any(\wrap(), {"i": i});
    }
    return sprintf("%y", v);
});
run_case("hash, 2 args", string sub () {
    auto v;
    for (int i = 0; i < N; ++i) {
        v = call_ref2(\mkh(), "k" + i, i);
    }
    return sprintf("%y", v);
});
run_case("no args", string sub () {
    int t = 0;
    for (int i = 0; i < N; ++i) {
        t += call_ref0(\zero());
    }
    return sprintf("%d", t);
});
run_case("3 args", string sub () {
    int t = 0;
    for (int i = 0; i < N; ++i) {
        t += call_ref3(\add3(), i % 10, 2, 3);
    }
    return sprintf("%d", t);
});
run_case("auto param", string sub () {
    list<int> l = ();
    foreach auto v in ((1, 1.5, "s", True, NOTHING, (1,), {})) {
        push l, call_ref_any(\any_param(), v);
    }
    return sprintf("%y", l);
});

# arguments whose references the call gives to the callee: they are released when the callee returns, after the
# check of its return value
run_case("given temporaries", string sub () {
    Queue log();
    for (int i = 0; i < N; ++i) {
        call_tmp(\take_tmp(), "t" + i, i >= N - 2 ? log : new Queue());
    }
    list<string> l = ();
    while (log.size()) {
        push l, log.get();
    }
    return l.join("; ");
});
run_case("given temporary, return type and destructor exception", string sub () {
    for (int i = 0; i < N; ++i) {
        call_tmp(\take_tmp(), "w" + i, new Queue());
    }
    try {
        call_tmp(\take_tmp_bad_return(), "bad");
        return "no exception";
    } catch (hash<ExceptionInfo> ex) {
        return errs(ex);
    }
});
run_case("typed list", string sub () {
    int t = 0;
    for (int i = 0; i < N; ++i) {
        list<auto> l = (1, 2, i);
        t += call_ref_any(\list_size(), l);
    }
    return sprintf("%d", t);
});

# exceptions raised by the callee, with the call stack of the user functions
run_case("exception", string sub () {
    try {
        call_ref(\thrower(), 7);
        return "no exception";
    } catch (hash<ExceptionInfo> ex) {
        return err(ex) + " stack: " + stack_of(ex);
    }
});
run_case("nested exception", string sub () {
    try {
        call_ref(\deep(), 8);
        return "no exception";
    } catch (hash<ExceptionInfo> ex) {
        return err(ex) + " stack: " + stack_of(ex);
    }
});
run_case("exceptions in a loop", string sub () {
    return sprintf("caught %d", try_loop(\thrower(), N));
});
run_case("return type", string sub () {
    int t = 0;
    for (int i = 0; i < N; ++i) {
        t += call_ref_any(\bad_return(), "ok");
    }
    try {
        return sprintf("%d %y", t, call_ref_any(\bad_return(), "s"));
    } catch (hash<ExceptionInfo> ex) {
        return sprintf("%d %s: %s stack: %s", t, ex.err, ex.desc, stack_of(ex));
    }
});

# named arguments, which the generic call path binds, and positional calls of the same reference after them
run_case("named", string sub () {
    int t = 0;
    for (int i = 0; i < N; ++i) {
        t += call_ref_named(\inc());
    }
    return sprintf("%d", t);
});
run_case("positional after named", string sub () {
    code f = \inc();
    int t = call_ref_named(f);
    for (int i = 0; i < N; ++i) {
        t += call_ref(f, i);
    }
    return sprintf("%d", t);
});

# arguments that the variant does not take as they are: the generic call path raises its errors
run_case("too few args", string sub () {
    try {
        return sprintf("%y", call_ref_any(\add3(), 1));
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
});
run_case("too many args", string sub () {
    try {
        return sprintf("%y", call_ref3(\inc(), 1, 2, 3));
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
});
run_case("no arg for a parameter", string sub () {
    try {
        return sprintf("%y", call_ref0(\inc()));
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
});
run_case("wrong type", string sub () {
    try {
        return sprintf("%y", call_ref_any(\inc(), "str"));
    } catch (hash<ExceptionInfo> ex) {
        return ex.err + ": " + ex.desc.split("\n")[0];
    }
});
run_case("NOTHING arg", string sub () {
    try {
        return sprintf("%y", call_ref_any(\inc(), NOTHING));
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
});
run_case("wrong type after warm-up", string sub () {
    int t = loop_ref(\inc(), N);
    code f = \inc();
    t += loop_ref(f, N);
    try {
        call_ref_any(f, 1.5);
        return "no exception";
    } catch (hash<ExceptionInfo> ex) {
        return sprintf("%d %s", t, ex.err);
    }
});

# variants that need the generic call path
run_case("default argument", string sub () {
    int t = 0;
    for (int i = 0; i < N; ++i) {
        t += call_ref0(\with_default()) + call_ref(\with_default(), i);
    }
    return sprintf("%d", t);
});
run_case("overloaded", string sub () {
    int t = 0;
    for (int i = 0; i < N; ++i) {
        t += call_ref_any(\over(), i) + call_ref_any(\over(), "abc");
    }
    return sprintf("%d", t);
});
run_case("synchronized", string sub () {
    return sprintf("%d", loop_ref(\sync_inc(), N));
});
run_case("soft parameter", string sub () {
    int t = 0;
    for (int i = 0; i < N; ++i) {
        t += call_ref_any(\soft(), string(i));
    }
    return sprintf("%d", t);
});
run_case("implicit arguments", string sub () {
    return sprintf("%y", call_ref(\reads_argv(), 3));
});
run_case("reference argument", string sub () {
    int v = 0;
    code f = \set_ref();
    for (int i = 0; i < N; ++i) {
        f(\v, i);
    }
    return sprintf("%d", v);
});

# a later parse cannot add an overload to a function that calls through a reference to it have taken the fast path
# for (the fast path checks on every call that the function still has its single variant); the calls are unchanged
run_case("overload added later", string sub () {
    Program p(PO_NEW_STYLE | PO_ALLOW_REPARSE);
    p.parse("%modern
string sub f(int x) { return \"int\"; }
code sub get_ref() { return \\f(); }
string sub call_n(code c, auto x, int n) { string s; for (int i = 0; i < n; ++i) { s = c(x); } return s; }
string sub call_int_n(code c, int x, int n) { string s; for (int i = 0; i < n; ++i) { s = c(x); } return s; }
", "first");
    code r = p.callFunction("get_ref");
    string before = p.callFunction("call_int_n", r, 1, N);
    string added;
    try {
        p.parse("%modern
string sub f(string x) { return \"string\"; }
", "second");
        added = "added";
    } catch (hash<ExceptionInfo> ex) {
        added = ex.err;
    }
    string after = p.callFunction("call_int_n", r, 1, N);
    string other;
    try {
        other = p.callFunction("call_n", r, "s", N);
    } catch (hash<ExceptionInfo> ex) {
        other = ex.err;
    }
    return sprintf("%s %s %s %s", before, added, after, other);
});

# a reference shared by threads that make their first calls at once
run_case("concurrent first use", string sub () {
    code r = \inc();
    int threads = 16;
    Counter ready(threads);
    Counter go(1);
    Queue results();
    for (int t = 0; t < threads; ++t) {
        background sub () {
            ready.dec();
            go.waitForZero();
            results.push(loop_ref(r, N));
        }();
    }
    ready.waitForZero();
    go.dec();
    int total = 0;
    for (int t = 0; t < threads; ++t) {
        total += results.get();
    }
    return sprintf("%d", total);
});

# a reference made in a background thread, and calls made by the expression of the background operator
run_case("background", string sub () {
    Queue q();
    background q.push(\inc());
    code r = q.get();
    Queue results();
    background results.push(loop_ref(r, N));
    background results.push(r(41));
    int a = results.get();
    int b = results.get();
    return sprintf("%d", a + b);
});

# a module's function, called through a reference that the script made, runs in the module's Program
run_case("module function", string sub () {
    return sprintf("%d", loop_ref(\mod_step(), N));
});
run_case("module function exception", string sub () {
    int caught = 0;
    string last;
    for (int i = 0; i < N; ++i) {
        try {
            call_ref(\mod_throw(), i);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "MODULE-FUNCREF-ERROR" && ex.arg == i) {
                ++caught;
            }
            last = err(ex) + " stack: " + stack_of(ex);
        }
    }
    return sprintf("caught %d, last %s", caught, last);
});
run_case("module's reference", string sub () {
    return sprintf("%d", loop_ref(mod_get_ref(), N));
});
run_case("script reference called by the module", string sub () {
    int t = 0;
    for (int i = 0; i < N; ++i) {
        t += mod_call(\inc(), i);
    }
    return sprintf("%d", t);
});

# the function of a Program that was deleted: its import into another Program keeps the function, not its Program
run_case("deleted Program", string sub () {
    Program a(PO_NEW_STYLE);
    a.parse("%modern
int sub g(int x) { return x + 7; }
sub export_to(Program p) { p.importFunction(\"g\"); }
", "a");
    Program b(PO_NEW_STYLE);
    a.callFunction("export_to", b);
    b.parse("%modern
code sub mk_ref() { return \\g(); }
int sub run(code c, int n) { int t = 0; for (int i = 0; i < n; ++i) { t += c(i); } return t; }
", "b");
    code r = b.callFunction("mk_ref");
    int before = b.callFunction("run", r, N);
    delete a;
    try {
        return sprintf("%d %y", before, b.callFunction("run", r, 1));
    } catch (hash<ExceptionInfo> ex) {
        return sprintf("%d %s", before, ex.err);
    }
});

# a function of a Program with thread entry handlers, imported into another Program: each call enters its Program
# once, with the handlers (generic call path: the Program can be deleted)
run_case("thread entry handler", string sub () {
    Program b(PO_NEW_STYLE);
    b.parse("%modern
int sub g(int x) { return x + 1; }
sub export_to(Program p) { p.importFunction(\"g\"); }
", "b");
    Queue log();
    b.setThreadEntryHandler(auto sub () { log.push("enter"); }, sub (auto v) { log.push("exit"); });
    Program a(PO_NEW_STYLE);
    b.callFunction("export_to", a);
    a.parse("%modern
int sub run(int n) {
    code r = \\g();
    Qore::Thread::Queue q();
    background sub () {
        int t = 0;
        for (int i = 0; i < n; ++i) {
            t += r(i);
        }
        q.push(t);
    }();
    return q.get();
}
", "a");
    int t = a.callFunction("run", 10);
    int entries = 0;
    int exits = 0;
    while (log.size()) {
        if (log.get() == "enter") {
            ++entries;
        } else {
            ++exits;
        }
    }
    return sprintf("%d entries %d exits %d", t, entries, exits);
});

# the sandbox: references made by sandboxed code, called by trusted code and the reverse, references reached through a
# Program lent to sandboxed code, and a reference bound to the sandbox
string denied = sprintf("%s%sqore-funcref-fast-%d-%d", ENV.TMPDIR ?? "/tmp", DirSep, getpid(), now_us().getEpochSeconds() * 1000000 + now_us().microseconds());
{
    File file();
    file.open2(denied, O_CREAT | O_WRONLY | O_TRUNC);
    file.write("secret\n");
}
on_exit unlink(denied);

string sub tprobe(string path) {
    try {
        ReadOnlyFile f(path);
        return "read:" + trim(f.read(-1));
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub probe_n(code c, string path, int n) {
    string s;
    for (int i = 0; i < n; ++i) {
        s = c(path);
    }
    return s;
}

const ProbeSource = "%modern
string sub probe(string path) {
    try {
        Qore::ReadOnlyFile f(path);
        return \"read:\" + trim(f.read(-1));
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
string sub probe_n(code c, string path, int n) {
    string s;
    for (int i = 0; i < n; ++i) {
        s = c(path);
    }
    return s;
}
";

SandboxManager manager();
manager.addFilesystemDeniedPath(denied);
Program sandboxed(PO_NEW_STYLE);
sandboxed.setSandboxManager(manager);
sandboxed.importFunction("tprobe");
sandboxed.parse(ProbeSource + "
code sub mk_ref() { return \\probe(); }
code sub mk_trusted_ref() { return \\tprobe(); }
string sub self_call(string path, int n) { return probe_n(\\probe(), path, n); }
string sub call_given(code c, string path, int n) { return probe_n(c, path, n); }
string sub use_lent(Program l, string path) { return l.callFunction(\"lent_self_call\", path); }
string sub use_lent_ref(Program l, string path, int n) { return probe_n(l.callFunction(\"lent_ref\"), path, n); }
code sub mk_bound() { return Qore::SandboxManager::bindCall(\\probe()); }
", "sandboxed");

Program lent(PO_NEW_STYLE);
lent.parse(ProbeSource + "
string sub lent_self_call(string path) { return probe_n(\\probe(), path, 50); }
code sub lent_ref() { return \\probe(); }
", "lent");

run_case("trusted self call", string sub () { return probe_n(\tprobe(), denied, N); });
run_case("sandboxed reference called by trusted code", string sub () {
    return probe_n(sandboxed.callFunction("mk_ref"), denied, N);
});
run_case("sandboxed reference to a trusted function called by trusted code", string sub () {
    return probe_n(sandboxed.callFunction("mk_trusted_ref"), denied, N);
});
run_case("trusted reference called by sandboxed code", string sub () {
    return sandboxed.callFunction("call_given", \tprobe(), denied, N);
});
run_case("sandboxed self call", string sub () {
    return sandboxed.callFunction("self_call", denied, N);
});
run_case("lent Program's own call", string sub () {
    return sandboxed.callFunction("use_lent", lent, denied);
});
run_case("lent Program's reference called by sandboxed code", string sub () {
    return sandboxed.callFunction("use_lent_ref", lent, denied, N);
});
run_case("lent Program called by trusted code", string sub () {
    return lent.callFunction("lent_self_call", denied);
});
run_case("sandbox-bound reference", string sub () {
    return probe_n(sandboxed.callFunction("mk_bound"), denied, N);
});
run_case("trusted sandbox-bound reference", string sub () {
    return probe_n(SandboxManager::bindCall(\tprobe()), denied, N);
});
