# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the target of debugger-inline-method-calls.qtest: run() calls a method and a static method in a loop, attaches a
# debugger in its iteration i == 2 and removes it in its iteration i == 4; the lines tagged "### <tag>" are found by
# the test

class C {
    int m(int a) {
        int x = a;                          ### m_first
        for (int j = 0; j < 3; ++j) {
            x += 10;                        ### m_body
        }
        return x;
    }

    static int s(int a) {
        int y = a;                          ### s_first
        for (int j = 0; j < 3; ++j) {
            y += 100;                       ### s_body
        }
        return y;
    }
}

list<int> sub run(int n, code attach, code detach) {
    C o();
    list<int> r = ();
    for (int i = 0; i < n; ++i) {
        if (i == 2) {
            attach();
        }
        if (i == 4) {
            detach();
        }
        push r, o.m(i);
        push r, C::s(i);
    }
    return r;
}
