#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# calls functions and a method directly, many times, from top-level code and from a function, so that the IR
# interpreter runs them in its caller; prints the result of the work

%modern

class Acc {
    private {
        int total = 0;
    }

    #! a method that the IR interpreter runs inline in its caller
    int add(int i) {
        int x = 0;
        for (int j = 0; j < 2; ++j) {
            x += i + j;
        }
        total += x;
        return total;
    }
}

#! a leaf function that the IR interpreter evaluates without running its body
int sub leaf(int a, int b) {
    return a + b;
}

#! a function with a loop that the IR interpreter runs inline in its caller
int sub looped(int n) {
    int t = 0;
    for (int i = 0; i < 3; ++i) {
        t += n * i;
    }
    return t;
}

#! calls the method directly from a function body
int sub use_method(Acc acc, int i) {
    return acc.add(i);
}

int sum = 0;
Acc acc();
# direct calls from top-level code
for (int i = 0; i < 300; ++i) {
    sum += leaf(i, 1);
    sum += looped(i);
    sum += use_method(acc, i);
}
printf("%d\n", sum);
