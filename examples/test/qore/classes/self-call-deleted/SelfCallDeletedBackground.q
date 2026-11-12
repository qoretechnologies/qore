#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# a call through self in a background expression, made after the expression deleted the object, raises
# OBJECT-ALREADY-DELETED before its arguments are evaluated, as the AST interpreter raises it (issue #5823); prints
# what each case evaluates on stdout (the exception that ends the background thread is on stderr)

%modern

int sub side(string s) {
    printf("  side %s\n", s);
    return 1;
}

#! decrements its counter when the background thread that holds it releases it
class Token {
    private {
        Counter cnt;
    }

    constructor(Counter cnt) {
        self.cnt = cnt;
    }

    destructor() {
        cnt.dec();
    }

    int get() {
        return 0;
    }
}

final class F {
    int m(int x) {
        printf("  F::m(%d)\n", x);
        return x;
    }

    kill() {
        delete self;
    }

    #! an unqualified call to a method of a final class
    plain(Token t) {
        background (t.get(), kill(), m(side("plain")));
    }

    #! a call through self
    selfCall(Token t) {
        background (t.get(), kill(), self.m(side("self")));
    }
}

#! runs a case and waits for its background thread to end
sub run(string name, code c) {
    printf("%s:\n", name);
    Counter cnt(1);
    c(new Token(cnt));
    cnt.waitForZero();
}

# the cases run more than once, so that the tiered execution mode compiles them
for (int i = 0; i < 3; ++i) {
    printf("round %d\n", i);
    run("unqualified call", sub (Token t) { F f(); f.plain(t); });
    run("call through self", sub (Token t) { F f(); f.selfCall(t); });
}
