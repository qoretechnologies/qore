#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The first calls through references to many functions with call-reference entries, made by many threads at once,
# while another thread loads a module compiled with qcc, whose image registers its call-reference entries when it is
# loaded (qore#5578): every thread must get every result; run under ThreadSanitizer, no data race may be reported.
# The module is loaded at run time (load_module()), from the module path.

%modern
%no-child-restrictions

const Threads = 8;
const Rounds = 50;

int sub f0(int x) { return x * 3 + 0; }
int sub f1(int x) { return x * 3 + 1; }
int sub f2(int x) { return x * 3 + 2; }
int sub f3(int x) { return x * 3 + 3; }
int sub f4(int x) { return x * 3 + 4; }
int sub f5(int x) { return x * 3 + 5; }
int sub f6(int x) { return x * 3 + 6; }
int sub f7(int x) { return x * 3 + 7; }
int sub f8(int x) { return x * 3 + 8; }
int sub f9(int x) { return x * 3 + 9; }
int sub f10(int x) { return x * 3 + 10; }
int sub f11(int x) { return x * 3 + 11; }
int sub f12(int x) { return x * 3 + 12; }
int sub f13(int x) { return x * 3 + 13; }
int sub f14(int x) { return x * 3 + 14; }
int sub f15(int x) { return x * 3 + 15; }
int sub f16(int x) { return x * 3 + 16; }
int sub f17(int x) { return x * 3 + 17; }
int sub f18(int x) { return x * 3 + 18; }
int sub f19(int x) { return x * 3 + 19; }
int sub f20(int x) { return x * 3 + 20; }
int sub f21(int x) { return x * 3 + 21; }
int sub f22(int x) { return x * 3 + 22; }
int sub f23(int x) { return x * 3 + 23; }
int sub f24(int x) { return x * 3 + 24; }
int sub f25(int x) { return x * 3 + 25; }
int sub f26(int x) { return x * 3 + 26; }
int sub f27(int x) { return x * 3 + 27; }
int sub f28(int x) { return x * 3 + 28; }
int sub f29(int x) { return x * 3 + 29; }
int sub f30(int x) { return x * 3 + 30; }
int sub f31(int x) { return x * 3 + 31; }

list<code> sub refs() {
    return (
        \f0(),
        \f1(),
        \f2(),
        \f3(),
        \f4(),
        \f5(),
        \f6(),
        \f7(),
        \f8(),
        \f9(),
        \f10(),
        \f11(),
        \f12(),
        \f13(),
        \f14(),
        \f15(),
        \f16(),
        \f17(),
        \f18(),
        \f19(),
        \f20(),
        \f21(),
        \f22(),
        \f23(),
        \f24(),
        \f25(),
        \f26(),
        \f27(),
        \f28(),
        \f29(),
        \f30(),
        \f31(),
    );
}

int sub run(list<code> fs, int x) {
    int sum = 0;
    foreach code f in (fs) {
        x = f(x) & 65535;
        sum += x;
    }
    return sum;
}

Counter start(1);
Queue results();
for (int t = 0; t < Threads; ++t) {
    background sub () {
        # each thread makes references of its own, and all of them make their first calls at once
        list<code> fs = refs();
        start.waitForZero();
        int sum = 0;
        for (int r = 0; r < Rounds; ++r) {
            sum += run(fs, r);
        }
        results.push(sum);
    }();
}
background sub () {
    start.waitForZero();
    load_module("CallRefNativeEntryModule");
    results.push(call_function("mod_loop", 200, 7));
}();
start.dec();
list<int> sums = ();
for (int t = 0; t <= Threads; ++t) {
    push sums, results.get();
}
sums = sort(sums);
printf("%s\n", (map sprintf("%d", $1), sums).join(" "));
