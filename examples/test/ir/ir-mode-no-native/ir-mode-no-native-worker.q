#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# runs hot code: functions called many times, a closure, a method, and a long loop, which are compiled to native
# code with JIT and tiered execution; prints the result of the work

%modern

class Counter2 {
    private {
        int n = 0;
    }

    int add(int i) {
        n += i;
        return n;
    }
}

int sub hot(int i) {
    return i * 2 + 1;
}

int sub loop(int n) {
    int sum = 0;
    for (int i = 0; i < n; ++i) {
        sum += i % 7;
    }
    return sum;
}

# the functions are called through call references, which are not inlined in the caller, so each call is a call of
# the function itself
code<int(int)> h = \hot();
int total = 0;
for (int i = 0; i < 2000; ++i) {
    total += h(i);
}
code<int(int)> c = int sub (int i) { return i - 1; };
for (int i = 0; i < 2000; ++i) {
    total += c(i);
}
Counter2 cnt();
code<int(int)> a = \cnt.add();
for (int i = 0; i < 2000; ++i) {
    a(1);
}
total += cnt.add(0);
code<int(int)> l = \loop();
for (int i = 0; i < 3; ++i) {
    total += l(200000);
}
printf("%d\n", total);
