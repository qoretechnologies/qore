#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT

# calls of a function imported with Program::importFunction() whose Program has been deleted, from the importing
# Program; imported-function-deleted-program.qtest runs it in every execution mode and compiled with AOT
/*  An imported function runs in the Program it is imported from, with that Program's names and global variables;
    once that Program has been deleted, every call raises PROGRAM-ERROR.  The importing Program shares the function's
    code (its variants) with the Program it is imported from, and the code uses that Program's data (the local
    variables of its parameters, its globals, the functions it calls), which the Program's deletion frees: a call
    that reached the code used freed memory (issue #5751).

    The argument "debug" adds a call made while another thread holds a reference to the deleted Program (debug
    builds only).
*/

%modern
%no-child-restrictions

const Calls = "
string sub call_direct() { return defined('x'); }
string sub call_by_name() { return call_function('defined', 'x'); }
code sub get_ref() { return \\defined(); }
string sub call_ref_new() { code c = \\defined(); return c('x'); }
string sub call_loop() { string s; for (int i = 0; i < 3; ++i) { s = defined('x'); } return s; }
";

const CallNames = ("call_direct", "call_by_name", "call_ref_new", "call_loop");

const DefiningSrc = "our int counter;
string sub helper(string s) { return s + 'y'; }
string sub defined(string s) { ++counter; return helper(s); }
sub export_to(Program p) { p.importFunction('defined'); }
int sub get_counter() { return counter; }";

# calls f in p and prints the result or the error
sub show(string label, Program p, string f) {
    try {
        printf("%s: %s: %y\n", label, f, p.callFunction(f));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, f, ex.err);
    }
}

# calls c and prints the result or the error
sub show_ref(string label, code c) {
    try {
        printf("%s: %y\n", label, c("x"));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

# returns a Program that defines the function
Program sub get_defining() {
    Program p(PO_NEW_STYLE);
    p.parse(DefiningSrc, "defining");
    return p;
}

# returns a Program that imports the function from \a from (which defines it or imported it) and calls it
Program sub get_importing(Program from, string name) {
    Program p(PO_NEW_STYLE);
    from.callFunction("export_to", p);
    p.parse("sub export_to(Program p) { p.importFunction('defined'); }\n" + Calls, name);
    return p;
}

# warmed calls: the importing code runs on every tier with tiered execution before the deletion, the imported
# function directly and through an intermediate Program (an import of an import)
{
    Program defining = get_defining();
    Program user = get_importing(defining, "user");
    Program mid = get_importing(defining, "mid");
    Program chained = get_importing(mid, "chained");
    hash<string, Program> callers = {"user": user, "chained": chained};
    hash<string, code> refs;
    int calls;
    for (int i = 0; i < 40; ++i) {
        foreach hash<auto> c in (callers.pairIterator()) {
            foreach string f in (CallNames) {
                c.value.callFunction(f);
            }
            refs{c.key} = c.value.callFunction("get_ref");
            refs{c.key}("x");
            # call_loop calls the function 3 times
            calls += 7;
        }
    }
    # the function runs in its own Program, with its own globals
    printf("warmed: before: %y counter: %y\n", user.callFunction("call_direct"), defining.callFunction("get_counter")
        == calls + 1);

    # the deletion of an importing Program does not affect the other importers
    delete user;
    show("warmed: importer deleted", chained, "call_direct");

    delete defining;
    # the intermediate Program still holds its copy of the function
    show("warmed: after", mid, "call_direct");
    delete mid;
    foreach string f in (CallNames) {
        show("warmed: after", chained, f);
    }
    show_ref("warmed: after: old ref", refs.chained);
    show_ref("warmed: after: old ref of a deleted importer", refs.user);
}

# the first call of the function is made after the deletion
{
    Program defining = get_defining();
    Program user = get_importing(defining, "user");
    Program mid = get_importing(defining, "mid");
    Program chained = get_importing(mid, "chained");
    delete defining;
    delete mid;
    foreach string f in (CallNames) {
        show("first: user", user, f);
        show("first: chained", chained, f);
    }
}

# the importing code is compiled after the deletion, before the function was ever called
{
    Program defining = get_defining();
    Program user(PO_NEW_STYLE);
    defining.callFunction("export_to", user);
    user.parse("int sub f(int x) { int s = 0; for (int i = 0; i < x; ++i) { s += defined('x').size(); } return s; }",
        "user");
    delete defining;
    int sum;
    for (int i = 0; i < 20; ++i) {
        sum += user.callFunction("f", 0);
    }
    try {
        printf("compiled after: %d %y\n", sum, user.callFunction("f", 3));
    } catch (hash<ExceptionInfo> ex) {
        printf("compiled after: %d %s\n", sum, ex.err);
    }
}

# another thread holds a reference to the deleted Program: it stays allocated, but no thread can enter it
if (ARGV[0] == "debug") {
    Program defining = get_defining();
    Program user = get_importing(defining, "user");
    for (int i = 0; i < 5; ++i) {
        user.callFunction("call_loop");
    }
    call_function("dbg_hold_source_program_call", defining);
    *string err;
    Counter done(1);
    background sub () {
        try {
            user.callFunction("call_direct");
        } catch (hash<ExceptionInfo> ex) {
            err = ex.err;
        }
        done.dec();
    }();
    hash<auto> h = call_function("dbg_wait_source_program_call_held", 60000);
    delete defining;
    foreach string f in (CallNames) {
        show("held: user", user, f);
    }
    bool continued = call_function("dbg_continue_source_program_call", 60000);
    done.waitForZero();
    printf("held: held: %y referenced: %y continued: %y err: %y\n", h.held, h.referenced, continued, err);
}
