#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# runs functions and methods that use or do not use argv and self in many threads at once while they and their
# callers are compiled to native code; prints the number of wrong results

%modern

int sub leaf(int i) {
    return i + 1;
}

int sub twice(int i) {
    int x = leaf(i);
    return leaf(x) * 2;
}

int sub count_args(...) {
    return argv.size() + leaf(0);
}

class Acc {
    private {
        int base;
    }

    constructor(int base) {
        self.base = base;
    }

    int get(int i) {
        return base + twice(i);
    }

    static int sget(int i) {
        return twice(i) - 1;
    }
}

const Threads = 8;
const Iterations = 3000;

our Counter done();
our Counter start(1);
our int wrong = 0;
our Mutex m();

sub run(int t) {
    Acc acc(t);
    code<int(int)> tw = \twice();
    code<int(int)> ag = \acc.get();
    code<int(int)> sg = \Acc::sget();
    code<int(...)> ca = \count_args();
    start.waitForZero();
    for (int i = 0; i < Iterations; ++i) {
        int expected = (i + 2) * 2;
        if (tw(i) != expected || ag(i) != t + expected || sg(i) != expected - 1 || ca(i, t, 1) != 4) {
            AutoLock al(m);
            ++wrong;
        }
    }
}

sub start_thread(int t) {
    background sub () {
        on_exit done.dec();
        run(t);
    }();
}

for (int t = 0; t < Threads; ++t) {
    done.inc();
    start_thread(t);
}
start.dec();
done.waitForZero();
printf("wrong: %d\n", wrong);
