#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# Worker for on-block-exit-outer-vars.qtest: runs each case and prints one "<case>: <result>" line per case.
# Every case reads and writes, in on_error / on_exit / on_success handlers, variables that the handler
# reaches from outside the handler: top-level locals, function locals and parameters, variables captured by
# closures (also nested closures), references, and handlers in loops.  Most variables are used ONLY by the
# handler, so that nothing but the handler binds them.  The qtest runs this script in every execution mode
# and as a qcc-compiled executable and compares every line with the expected (interpreter) result.

%modern

# top-level locals: the handlers below are the only code in their functions that uses them
list<string> tl_log;
int tl_count = 0;
string tl_str = "tl";
hash<auto> tl_hash = {};

sub out(string name, auto v) {
    printf("%s: %y\n", name, v);
}

sub run(string name, code c) {
    string res;
    try {
        auto rv = c();
        res = sprintf("returned %y", rv);
    } catch (hash<ExceptionInfo> ex) {
        res = sprintf("caught %s:%s", ex.err, ex.desc);
        *hash<ExceptionInfo> n = ex.next;
        while (n) {
            res += sprintf(" -> %s:%s", n.err, n.desc);
            n = n.next;
        }
    }
    printf("%s: %s\n", name, res);
}

# --- top-level locals ---

sub tl_on_error_write() {
    on_error push tl_log, "oe";
    throw "E", "d";
}

sub tl_on_error_read() {
    on_error throw "R", sprintf("%y", tl_log);
    throw "E", "d";
}

int sub tl_on_exit_write() {
    on_exit {
        ++tl_count;
        tl_str += "+exit";
    }
    return 1;
}

int sub tl_on_success_write() {
    on_success tl_hash.success = tl_count;
    return 2;
}

sub tl_on_error_if(bool x) {
    on_error if (x) {
        push tl_log, "oe-if:" + $1.err;
        tl_hash{$1.err} = $1.desc;
    }
    throw "IF", "if-desc";
}

sub tl_nested_handler() {
    on_exit {
        on_exit push tl_log, "nested-exit";
        push tl_log, "outer-exit";
    }
    push tl_log, "body";
}

sub tl_loop(int n) {
    for (int i = 0; i < n; ++i) {
        on_exit push tl_log, "loop:" + i;
    }
}

sub tl_foreach_error(list<auto> l) {
    foreach auto v in (l) {
        on_error push tl_log, sprintf("fe-oe:%d", $#);
        if (v) {
            throw "FE", "at " + $#;
        }
    }
}

# --- function locals and parameters ---

list<string> sub fn_local_write() {
    list<string> l;
    try {
        on_error push l, "oe";
        throw "E", "d";
    } catch () {
    }
    return l;
}

int sub fn_local_only_in_handler() {
    int n = 5;
    {
        on_exit n += 10;
    }
    return n;
}

string sub fn_param_only_in_handler(string p) {
    string rv = "";
    {
        on_exit rv = p + "!";
    }
    return rv;
}

sub fn_param_write_error(string p) {
    on_error push tl_log, "param:" + p;
    throw "E", "d";
}

int sub fn_handler_local() {
    int rv = 0;
    {
        on_exit {
            int inner = 7;
            rv = inner * 2;
        }
    }
    return rv;
}

# --- closures ---

list<string> sub cl_on_error_write() {
    list<string> l2;
    code c = sub () {
        on_error push l2, "c-oe";
        throw "E", "d";
    };
    try {
        c();
    } catch () {
    }
    return l2;
}

string sub cl_on_error_read() {
    string s = "captured";
    code c = sub () {
        on_error throw "R", s;
        throw "E", "d";
    };
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        return sprintf("%s:%s -> %s:%s", ex.err, ex.desc, ex.next.err, ex.next.desc);
    }
    return "none";
}

int sub cl_on_exit_counter(int n) {
    int count = 0;
    code c = sub () {
        on_exit ++count;
    };
    for (int i = 0; i < n; ++i) {
        c();
    }
    return count;
}

hash<auto> sub cl_on_success() {
    hash<auto> h = {};
    code c = int sub (int x) {
        on_success h{x} = x * x;
        return x;
    };
    map c($1), (1, 2, 3);
    return h;
}

list<string> sub cl_nested() {
    list<string> l;
    code outer = sub () {
        code inner = sub () {
            on_error push l, "inner-oe";
            throw "E", "d";
        };
        on_error push l, "outer-oe";
        inner();
    };
    try {
        outer();
    } catch () {
    }
    return l;
}

list<string> sub cl_nested_handler() {
    list<string> l;
    code c = sub () {
        on_exit {
            on_exit push l, "nested-exit";
            push l, "exit";
        }
    };
    c();
    return l;
}

list<string> sub cl_loop() {
    list<string> l;
    code c = sub (int n) {
        for (int i = 0; i < n; ++i) {
            on_exit push l, "i" + i;
        }
    };
    c(3);
    return l;
}

list<string> sub cl_top_level() {
    code c = sub () {
        on_error push tl_log, "closure-tl";
        throw "E", "d";
    };
    try {
        c();
    } catch () {
    }
    return tl_log;
}

# --- references ---

sub ref_write(reference<list<string>> r) {
    on_error push r, "ref-oe";
    throw "E", "d";
}

list<string> sub ref_caller() {
    list<string> l = ("start",);
    try {
        ref_write(\l);
    } catch () {
    }
    return l;
}

sub ref_on_exit(reference<int> r) {
    on_exit r += 100;
}

int sub ref_on_exit_caller() {
    int i = 1;
    ref_on_exit(\i);
    return i;
}

list<string> sub ref_top_level() {
    ref_write(\tl_log);
}

# --- methods ---

class C {
    public {
        list<string> log = ();
    }

    run() {
        on_error push log, "method-oe";
        throw "E", "d";
    }

    list<string> closure() {
        list<string> l;
        code c = sub () {
            on_error {
                push l, "method-closure-oe";
                push log, "member-from-closure";
            }
            throw "E", "d";
        };
        try {
            c();
        } catch () {
        }
        return l;
    }
}

# top-level locals
tl_log = ();
try { tl_on_error_write(); } catch () {}
out("tl_on_error_write", tl_log);
tl_log = ("a", "b");
run("tl_on_error_read", \tl_on_error_read());
out("tl_on_exit_write", (tl_on_exit_write(), tl_count, tl_str));
out("tl_on_success_write", (tl_on_success_write(), tl_hash));
tl_log = ();
try { tl_on_error_if(True); } catch () {}
out("tl_on_error_if", (tl_log, tl_hash));
tl_log = ();
tl_nested_handler();
out("tl_nested_handler", tl_log);
tl_log = ();
tl_loop(3);
out("tl_loop", tl_log);
tl_log = ();
run("tl_foreach_error", sub () { tl_foreach_error((0, 0, 1, 0)); });
out("tl_foreach_error_log", tl_log);

# function locals and parameters
out("fn_local_write", fn_local_write());
out("fn_local_only_in_handler", fn_local_only_in_handler());
out("fn_param_only_in_handler", fn_param_only_in_handler("p"));
tl_log = ();
try { fn_param_write_error("pp"); } catch () {}
out("fn_param_write_error", tl_log);
out("fn_handler_local", fn_handler_local());

# closures
out("cl_on_error_write", cl_on_error_write());
out("cl_on_error_read", cl_on_error_read());
out("cl_on_exit_counter", cl_on_exit_counter(4));
out("cl_on_success", cl_on_success());
out("cl_nested", cl_nested());
out("cl_nested_handler", cl_nested_handler());
out("cl_loop", cl_loop());
tl_log = ();
out("cl_top_level", cl_top_level());

# references
out("ref_caller", ref_caller());
out("ref_on_exit_caller", ref_on_exit_caller());
tl_log = ("x",);
try { ref_top_level(); } catch () {}
out("ref_top_level", tl_log);

# methods
C obj();
try { obj.run(); } catch () {}
out("method", obj.log);
out("method_closure", (obj.closure(), obj.log));
