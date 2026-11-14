#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# the probe of AOTFinalMapSelfCall.qtest: map, select and fold expressions in a method of a final class call a method of
# the object through self with the implicit element, which qcc took for a fast entry whose argument it checked for
# NOTHING without boxing it (qore#6081)

%modern

final class G {
    int fi(int x) { return x + 1; }
    private int pf(int x) { return x * 2; }
    string fs(string s) { return s + "!"; }
    int f2(int a, int b) { return a * 10 + b; }

    run() {
        printf("map self: %y\n", map self.fi($1), (9, 10));
        printf("map unqualified: %y\n", map fi($1), (9, 10));
        printf("map private: %y\n", map pf($1), (1, 2, 3));
        printf("map string: %y\n", map fs($1), ("a", "b"));
        printf("map two args: %y\n", map f2($1, $#), (7, 8));
        printf("map hash values: %y\n", map fi($1.value), {"a": 1, "b": 2}.pairIterator());
        printf("select: %y\n", select (9, 10, 11), self.fi($1) > 10);
        printf("foldl: %y\n", foldl fi($1) + $2, (1, 2, 3));
        printf("nested: %y\n", map (map fi($1), (1, 2)), (1, 2));
        list<int> l = map self.fi($1), (1, 2);
        printf("assigned: %y\n", l);
        # an element that is NOTHING is refused by the parameter, as it is by every mode
        try {
            printf("nothing: %y\n", map fi($1), (1, NOTHING));
        } catch (hash<ExceptionInfo> ex) {
            printf("nothing: %s\n", ex.err);
        }
        try {
            printf("select nothing: %y\n", select (1, NOTHING), fi($1) > 0);
        } catch (hash<ExceptionInfo> ex) {
            printf("select nothing: %s\n", ex.err);
        }
        try {
            printf("foldl nothing: %y\n", foldl fi($1) + $2, (1, NOTHING, 3));
        } catch (hash<ExceptionInfo> ex) {
            printf("foldl nothing: %s\n", ex.err);
        }
    }
}

# a class that is not final and a function, which used other paths
class N {
    int fi(int x) { return x + 1; }
    run() { printf("not final: %y\n", map self.fi($1), (9, 10)); }
}

int sub fn(int x) { return x + 1; }

G g();
g.run();
N n();
n.run();
printf("function: %y\n", map fn($1), (9, 10));
