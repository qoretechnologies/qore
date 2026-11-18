# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# fixture of enum-hash-int-operand.qtest: a value of an int enum type read from a hash with a constant key is read as
# an int where an int compound assignment converts it to its base value, and keeps its enum type where it is observed
# (qore#6477)

%modern
%no-child-restrictions

enum E { A = 1, B = 2, C = 40 }
enum ES : string { X = "x" }

sub show(string label, auto v) {
    printf("%s: %y %s\n", label, v, v.fullType());
}

# the operands of the int compound assignments, in a loop as in the issue's benchmark
int sub sum_plus(hash<string, E> h, int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += h.k;
    }
    return t;
}

int sub sum_minus(hash<string, E> h, int n) {
    int t = 100;
    for (int i = 0; i < n; ++i) {
        t -= h.k;
    }
    return t;
}

int sub bit_ops(hash<string, E> h) {
    int t = 7;
    t &= h.k;
    int u = 1;
    u |= h.k;
    int x = 3;
    x ^= h.k;
    int s = 1;
    s <<= h.k;
    int r = 1024;
    r >>= h.k;
    return t + u + x + s + r;
}

# a missing key gives NOTHING, which is 0 as an int operand
int sub missing(hash<string, E> h) {
    int t = 5;
    t += h.nokey;
    t -= h.nokey;
    return t;
}

# the value of a key that the loop changes is read each time
int sub changing(int n) {
    hash<string, E> h = {"k": E::A};
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += h.k;
        h.k = (i % 2) ? E::A : E::C;
    }
    return t;
}

sub observe(hash<string, E> h) {
    # observed values keep the enum type
    show("auto local", h.k);
    E e = h.k;
    show("enum local", e);
    list<auto> l = (h.k,);
    show("list element", l[0]);
    show("argument", h.k);
    # an int sum of observed values is an int
    int t = h.k + h.k;
    show("int sum", t);
}

# other bases and value types: an or-nothing hash, a reference, an or-nothing enum value type (not an int operand),
# and a read in a try block
int sub other_bases(*hash<string, E> oh, reference<hash<string, E>> rh, hash<string, *E> nh) {
    int t = 0;
    t += oh.k;
    t += rh.k;
    t += nh.k;
    t += nh.nokey;
    try {
        t += oh.k;
    } catch (hash<ExceptionInfo> ex) {
        t = -1;
    }
    int m = 100;
    m %= oh.k;
    return t * 1000 + m;
}

# the temporaries of an operand are released when its value has been computed, before the rest of the expression
class D {
    public {
        hash<string, E> h = {"k": E::B};
    }
    private {
        string n;
    }
    constructor(string n) {
        self.n = n;
    }
    destructor() {
        printf("release %s\n", n);
    }
}

D sub mk(string n) {
    return new D(n);
}

int sub side(string n) {
    printf("after %s\n", n);
    return 0;
}

class M {
    public {
        int t = 0;
    }
    int run() {
        int r = (t += mk("member").h.k) + side("member");
        r = (t |= mk("member bit").h.k) + side("member bit");
        int v = 1;
        reference<int> rv = \v;
        r = (rv += mk("reference").h.k) + side("reference");
        return t + v;
    }
}

hash<string, E> h = {"k": E::C};
hash<string, E> rh = {"k": E::A};
show("other bases", other_bases(h, \rh, {"k": E::B}));
show("temporaries", (new M()).run());
show("plus", sum_plus(h, 5));
show("minus", sum_minus(h, 2));
show("bit ops", bit_ops({"k": E::B}));
show("missing", missing(h));
show("changing", changing(5));
observe(h);
# a string enum is not an int operand
hash<string, ES> hs = {"k": ES::X};
string str = "a";
str += hs.k;
show("string enum", str);
show("string enum value", hs.k);
