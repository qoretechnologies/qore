#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the worker of reference-element-store.qtest: assignments of an element of a container, or a member of an object,
# that holds a reference are made through the reference, as the AST interpreter makes them (issue #6661)

%modern

class T {
    public {
        string n;
    }
    constructor(string n) {
        self.n = n;
    }
}

class C {
    public {
        auto m;
    }
    setMember(int v) {
        m = v;
    }
    setSelfMember(int v) {
        self.m = v;
    }
}

hashdecl HD {
    auto a;
}

our *T g_p;
our int g = 1;

sub write_hash(hash<string, reference<*T>> h, string n) {
    h.a = new T(n);
}
sub write_list(list<reference<*T>> l, string n) {
    l[0] = new T(n);
}
sub write_auto_hash(hash<auto> h, int v) {
    h.a = v;
}
sub write_auto_list(list<auto> l, int v) {
    l[0] = v;
}
sub write_untyped_hash(hash h, int v) {
    h.a = v;
}
sub write_dynamic(hash<string, reference<int>> h, string k, int v) {
    h{k} = v;
}
sub write_compound(hash<string, reference<int>> h, int v) {
    h.a += v;
}
sub write_negative(list<reference<int>> l, int v) {
    l[-1] = v;
}
sub write_object(object o, int v) {
    o.m = v;
}
sub write_type_error(hash<string, reference<int>> h, auto v) {
    h.a = v;
}
sub write_hashdecl(hash<HD> h, int v) {
    h.a = v;
}
sub write_loop_local() {
    int x = 0;
    hash<string, reference<int>> h = {"a": \x};
    list<int> seen = ();
    for (int i = 1; i <= 3; ++i) {
        h.a = x + i;
        # the local is read again after the store through the element
        seen += x;
    }
    printf("a local read again in a loop: %y\n", seen);
}
sub write_local_target() {
    int x = 1;
    hash<string, reference<int>> h = {"a": \x};
    h.a = 31;
    printf("a local as the target: %d\n", x);
}

hash<string, reference<*T>> d = {"a": \g_p};
write_hash(d, "d");
printf("hash<string, reference<*T>> variable: %s\n", g_p.n);
write_hash({"a": \g_p}, "h");
printf("hash<string, reference<*T>> literal: %s\n", g_p.n);
write_list((\g_p,), "l");
printf("list<reference<*T>> literal: %s\n", g_p.n);
write_auto_hash({"a": \g}, 5);
printf("hash<auto>: %d\n", g);
write_auto_list((\g,), 6);
printf("list<auto>: %d\n", g);
write_untyped_hash({"a": \g}, 7);
printf("hash: %d\n", g);
write_dynamic({"a": \g}, "a", 8);
printf("dynamic key: %d\n", g);
write_compound({"a": \g}, 10);
printf("+=: %d\n", g);
write_negative((\g,), 9);
printf("negative index: %d\n", g);
{
    hash<auto> lh = {"a": \g};
    lh.a = 12;
    printf("local hash<auto>: %d\n", g);
    list<auto> ll = (\g,);
    ll[0] = 13;
    printf("local list<auto>: %d\n", g);
}
C c();
c.m = \g;
write_object(c, 21);
printf("object member: %d\n", g);
c.setMember(22);
printf("member in a method: %d\n", g);
c.setSelfMember(23);
printf("self member: %d\n", g);
try {
    write_type_error({"a": \g}, "str");
    printf("type error: none: %d\n", g);
} catch (hash<ExceptionInfo> ex) {
    printf("type error: %s: %s\n", ex.err, ex.desc);
}
write_local_target();
write_hashdecl(<HD>{"a": \g}, 41);
printf("hash<HD>: %d\n", g);
{
    C c2();
    c2.m = 1;
    write_auto_hash({"a": \c2.m}, 42);
    printf("an object member as the target: %d\n", c2.m);
}
write_loop_local();
