#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The worker of remove-destructor-exception.qtest: a destructor that "remove" runs raises an exception that the
# enclosing try statement catches, whatever the statement after the remove (qore#6823)

%modern

class Thrower {
    destructor() {
        throw "DTOR-ERR", "x";
    }
}

sub hf(hash<auto> h) {}

sub top_level_case(string label, code body) {
    try {
        body();
        printf("%s: not caught\n", label);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: caught %s\n", label, ex.err);
    }
}

# the case of the issue: a closure-captured local, an empty hash literal argument after the remove
sub captured_hash_arg() {
    try {
        Thrower t();
        code cap = int sub () { return t ? 1 : 0; };
        remove t;
        hf({});
        printf("captured_hash_arg: not caught\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("captured_hash_arg: caught %s\n", ex.err);
    }
}

sub captured_hash_decl() {
    try {
        Thrower t();
        code cap = int sub () { return t ? 1 : 0; };
        remove t;
        hash<auto> h = {};
        printf("captured_hash_decl: not caught %y\n", h);
    } catch (hash<ExceptionInfo> ex) {
        printf("captured_hash_decl: caught %s\n", ex.err);
    }
}

sub captured_hash_literal() {
    try {
        Thrower t();
        code cap = int sub () { return t ? 1 : 0; };
        remove t;
        hash<auto> h = {"a": 1};
        printf("captured_hash_literal: not caught %y\n", h);
    } catch (hash<ExceptionInfo> ex) {
        printf("captured_hash_literal: caught %s\n", ex.err);
    }
}

sub uncaptured_hash_arg() {
    try {
        Thrower t();
        code cap = sub () {};
        remove t;
        hf({});
        printf("uncaptured_hash_arg: not caught\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("uncaptured_hash_arg: caught %s\n", ex.err);
    }
}

sub plain_local_hash_arg() {
    try {
        Thrower t();
        remove t;
        hf({});
        printf("plain_local_hash_arg: not caught\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("plain_local_hash_arg: caught %s\n", ex.err);
    }
}

sub captured_last_statement() {
    try {
        Thrower t();
        code cap = int sub () { return t ? 1 : 0; };
        remove t;
    } catch (hash<ExceptionInfo> ex) {
        printf("captured_last_statement: caught %s\n", ex.err);
    }
}

sub hash_member_hash_arg() {
    try {
        hash<auto> c = {"t": new Thrower()};
        code cap = int sub () { return c ? 1 : 0; };
        remove c.t;
        hf({});
        printf("hash_member_hash_arg: not caught\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("hash_member_hash_arg: caught %s\n", ex.err);
    }
}

sub list_element_hash_arg() {
    try {
        list<auto> l = (new Thrower(),);
        remove l[0];
        hf({});
        printf("list_element_hash_arg: not caught\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("list_element_hash_arg: caught %s\n", ex.err);
    }
}

# the statements after the try statement run after the exception was caught
int sub after_catch() {
    int n = 0;
    try {
        Thrower t();
        code cap = int sub () { return t ? 1 : 0; };
        remove t;
        hf({});
        n = -1;
    } catch (hash<ExceptionInfo> ex) {
        ++n;
    }
    hash<auto> h = {"n": n};
    return h.n + 1;
}

captured_hash_arg();
captured_hash_decl();
captured_hash_literal();
uncaptured_hash_arg();
plain_local_hash_arg();
captured_last_statement();
hash_member_hash_arg();
list_element_hash_arg();
printf("after_catch: %d\n", after_catch());

# the same in top-level code
try {
    Thrower t();
    code cap = int sub () { return t ? 1 : 0; };
    remove t;
    hf({});
    printf("top_level: not caught\n");
} catch (hash<ExceptionInfo> ex) {
    printf("top_level: caught %s\n", ex.err);
}
printf("done\n");
