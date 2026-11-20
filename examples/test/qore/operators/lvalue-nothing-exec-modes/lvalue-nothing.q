#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# the lvalue operators on NOTHING, which check_lvalue() accepts as an lvalue: they change nothing and return NOTHING
# (issue #6480); the right side of push and unshift is evaluated

%modern
%disable-all-warnings

our int calls = 0;

int sub f() {
    ++calls;
    return 1;
}

auto sub g() {
    ++calls;
    return 2;
}

auto sub g3() {
    return (1, 2, 3);
}

sub show(string name, auto v) {
    printf("%s: %y\n", name, v);
}

sub in_function() {
    show("function trim", trim NOTHING);
    show("function chomp", chomp NOTHING);
    show("function pop", pop NOTHING);
    show("function shift", shift NOTHING);
    show("function push", push NOTHING, f());
    show("function unshift", unshift NOTHING, f());
    show("function remove", remove NOTHING);
    show("function delete", delete NOTHING);
    show("function subst", NOTHING =~ s/a/b/);
    show("function trans", NOTHING =~ tr/a/b/);
    show("function trim paren", trim (NOTHING));
    show("function assign", NOTHING = g());
    # a NOTHING element of a list assignment is not assigned; the elements after it are
    auto a;
    auto b;
    show("function list assign", (a, NOTHING, b) = g3());
    show("function list a", a);
    show("function list b", b);
    # a NOTHING element accepts a value of any type
    string s1;
    string s2;
    show("function typed list assign", (s1, NOTHING, s2) = ("x", 2, "z"));
    show("function typed list", (s1, s2));
    # a reference to NOTHING has no lvalue: the loop has no iterations
    int n = 0;
    foreach auto v in (\NOTHING) {
        ++n;
    }
    show("function foreach ref", n);
}

in_function();
show("top trim", trim NOTHING);
show("top chomp", chomp NOTHING);
show("top pop", pop NOTHING);
show("top shift", shift NOTHING);
show("top push", push NOTHING, f());
show("top unshift", unshift NOTHING, f());
show("top remove", remove NOTHING);
show("top delete", delete NOTHING);
show("top subst", NOTHING =~ s/a/b/);
show("top trans", NOTHING =~ tr/a/b/);
show("top assign", NOTHING = g());
{
    auto a;
    auto b;
    show("top list assign", (a, NOTHING, b) = g3());
    show("top list a", a);
    show("top list b", b);
    int n = 0;
    foreach auto v in (\NOTHING) {
        ++n;
    }
    show("top foreach ref", n);
}
printf("calls: %d\n", calls);
