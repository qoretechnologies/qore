#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# Worker for aot-shadowed-locals.qtest: runs each case and prints one "<case>: <result>" line per case.
# Every case declares a local variable that shadows the top-level local "log" (same name, same type) or another
# local, in nested blocks, loops, closures, on_block_exit handlers, and through references, and then uses both
# the shadowing and the shadowed variable.  The result of a case is the top-level variable followed by the
# value the shadowing variable had ("seen").  The qtest runs this script in every execution mode and as
# qcc-compiled executables and compares every line with the expected (interpreter) result.

%modern
# nested blocks below deliberately redeclare a name of an enclosing block
%disable-warning duplicate-local-vars

list<string> log = ("top",);
list<string> seen;

sub reset() {
    log = ("top",);
    seen = ();
}

sub run(string name, code c) {
    reset();
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        push seen, "caught:" + ex.err;
    }
    printf("%s: %y %y\n", name, log, seen);
}

sub rp(reference<list<string>> l, string v) {
    push l, v;
}

# --- nested blocks and loops ---

sub block_shadow() {
    {
        list<string> log = ("inner",);
        push log, "x";
        seen = log;
    }
    push log, "outer";
}

sub block_shadow_after() {
    push log, "before";
    {
        list<string> log = ("inner",);
        push log, "x";
        seen = log;
    }
}

sub sibling_blocks() {
    if (True) {
        list<string> log = ("a",);
        push log, "a2";
        seen += log;
    }
    push log, "mid";
    if (True) {
        list<string> log = ("b",);
        push log, "b2";
        seen += log;
    }
}

sub nested_shadow() {
    {
        list<string> log = ("l1",);
        {
            list<string> log = ("l2",);
            push log, "x";
            seen += log;
        }
        push log, "y";
        seen += log;
    }
    push log, "top-y";
}

sub for_shadow() {
    for (int i = 0; i < 2; ++i) {
        list<string> log = ("f" + i,);
        push log, "x";
        seen += log;
    }
    push log, "after";
}

sub foreach_shadow() {
    foreach list<string> log in ((("e1",), ("e2",))) {
        push log, "x";
        seen += log;
    }
    push log, "after";
}

sub while_shadow() {
    int i = 0;
    while (i++ < 2) {
        list<string> log = ("w",);
        push log, string(i);
        seen += log;
    }
    push log, "after";
}

sub different_type() {
    {
        int log = 5;
        log += 2;
        seen = (string(log),);
    }
    push log, "dt";
}

# --- parameters ---

sub param_shadow(list<string> log) {
    push log, "param";
    seen = log;
}

sub param_shadow_case() {
    param_shadow(("p",));
}

sub param_shadow_block(list<string> p) {
    {
        list<string> log = p;
        push log, "pb";
        seen = log;
    }
    push log, "outer";
}

sub param_shadow_block_case() {
    param_shadow_block(("p",));
}

# --- closures ---

sub closure_captures_inner() {
    {
        list<string> log = ("inner",);
        code f = sub () { push log, "cl"; };
        f();
        seen = log;
    }
    push log, "outer";
}

sub closure_refs_top() {
    {
        list<string> log = ("inner",);
        push log, "y";
        seen = log;
    }
    code f = sub () { push log, "cl-top"; };
    f();
}

sub closure_local_shadow() {
    code f = sub () {
        {
            list<string> log = ("cl-inner",);
            push log, "x";
            seen = log;
        }
        push log, "cl-outer";
    };
    f();
}

sub closure_loop() {
    list<code> cl;
    for (int i = 0; i < 2; ++i) {
        list<string> log = ("l" + i,);
        push cl, sub () { push log, "c"; seen += log; };
    }
    map $1(), cl;
    push log, "after";
}

sub closure_handler_shadow() {
    code c = sub () {
        {
            list<string> log = ("cl-inner",);
            on_exit {
                push log, "cl-h-in";
                seen = log;
            }
        }
        on_exit push log, "cl-h-top";
    };
    c();
}

# --- on_block_exit handlers ---

sub handler_top() {
    {
        list<string> log = ("inner",);
        push log, "z";
        seen = log;
    }
    on_exit push log, "h-top";
}

sub handler_top_error() {
    {
        list<string> log = ("inner",);
        push log, "z";
        seen = log;
    }
    on_error push log, "h-err";
    throw "E";
}

sub handler_top_success() {
    {
        list<string> log = ("inner",);
        push log, "z";
        seen = log;
    }
    on_success push log, "h-ok";
}

sub handler_inner() {
    {
        list<string> log = ("inner",);
        on_exit {
            push log, "h-in";
            seen = log;
        }
        push log, "body";
    }
    push log, "outer";
}

sub handler_inner_error() {
    {
        list<string> log = ("inner",);
        on_error {
            push log, "h-in-err";
            seen = log;
        }
        push log, "body";
        throw "E";
    }
}

sub handler_both() {
    on_exit push log, "h-outer";
    {
        list<string> log = ("inner",);
        on_exit {
            push log, "h-in";
            seen = log;
        }
    }
}

sub nested_handler_own() {
    on_exit {
        list<string> hl = ("h",);
        on_exit {
            push hl, "nested";
            seen = hl;
        }
        push log, "h-outer";
    }
    push log, "body";
}

sub nested_handler_shadow() {
    {
        list<string> log = ("inner",);
        on_exit {
            on_exit {
                push log, "nested-in";
                seen = log;
            }
        }
    }
    on_exit {
        on_exit push log, "nested-top";
    }
}

# --- references ---

sub ref_shadow() {
    {
        list<string> log = ("inner",);
        rp(\log, "r-in");
        seen = log;
    }
    rp(\log, "r-top");
}

sub ref_closure() {
    {
        list<string> log = ("inner",);
        code c = sub () { reference<list<string>> r = \log; push r, "rc"; };
        c();
        seen = log;
    }
    reference<list<string>> r2 = \log;
    push r2, "r2";
}

# --- methods ---

class C {
    m() {
        {
            list<string> log = ("m-inner",);
            push log, "x";
            seen = log;
        }
        push log, "method";
    }
}

sub method_case() {
    C c();
    c.m();
}

run("block_shadow", \block_shadow());
run("block_shadow_after", \block_shadow_after());
run("sibling_blocks", \sibling_blocks());
run("nested_shadow", \nested_shadow());
run("for_shadow", \for_shadow());
run("foreach_shadow", \foreach_shadow());
run("while_shadow", \while_shadow());
run("different_type", \different_type());
run("param_shadow", \param_shadow_case());
run("param_shadow_block", \param_shadow_block_case());
run("closure_captures_inner", \closure_captures_inner());
run("closure_refs_top", \closure_refs_top());
run("closure_local_shadow", \closure_local_shadow());
run("closure_loop", \closure_loop());
run("closure_handler_shadow", \closure_handler_shadow());
run("handler_top", \handler_top());
run("handler_top_error", \handler_top_error());
run("handler_top_success", \handler_top_success());
run("handler_inner", \handler_inner());
run("handler_inner_error", \handler_inner_error());
run("handler_both", \handler_both());
run("nested_handler_own", \nested_handler_own());
run("nested_handler_shadow", \nested_handler_shadow());
run("ref_shadow", \ref_shadow());
run("ref_closure", \ref_closure());
run("method", \method_case());

# top-level code shadowing a top-level local in a nested block
reset();
{
    list<string> log = ("tl-inner",);
    push log, "x";
    seen = log;
}
push log, "tl-outer";
printf("toplevel_block: %y %y\n", log, seen);
