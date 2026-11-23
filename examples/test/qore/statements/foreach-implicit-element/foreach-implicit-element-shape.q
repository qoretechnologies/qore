%modern
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the functions whose native code foreach-implicit-element-native.qtest checks (qore#7270): the foreach statements
# of the functions named "skip_*" do not set the implicit index, and those of the functions named "keep_*" do

%disable-warning unreferenced-variable

const Keys = ("k0", "k1", "k2", "k3");

int sub ten() {
    return 10;
}

# the body tests a key, as in the loops of the issue
int sub skip_keys() {
    hash<auto> h = {"k1": 1, "k3": 3};
    int n = 0;
    foreach string k in (Keys) {
        if (exists h{k}) {
            ++n;
        }
    }
    return n;
}

# the body calls a function
int sub skip_call() {
    int n = 0;
    foreach string k in (Keys) {
        n += ten();
    }
    return n;
}

# a loop over a reference
int sub skip_ref() {
    list<int> l = (1, 2, 3);
    foreach int v in (\l) {
        v *= 2;
    }
    return l[2];
}

# a loop over lazy operators
int sub skip_lazy() {
    list<int> l = (1, 2, 3);
    int n = 0;
    foreach int v in (map $1 * 2, select l, $1 > 1) {
        n += v;
    }
    return n;
}

# only an operator in the body reads the implicit index: its own
int sub skip_operator() {
    int n = 0;
    foreach string k in (Keys) {
        n += (map $# + ten(), (1, 2))[1];
    }
    return n;
}

# a context statement in the body reads the loop's index at run time
int sub keep_context() {
    hash<auto> data = {"a": (0, 1, 2, 3)};
    int n = 0;
    foreach string k in (Keys) {
        context (data) where (%a == $#) {
            n += %a;
        }
    }
    return n;
}

# a background expression in the body reads the loop's index in the new thread
int sub keep_background() {
    Counter c();
    foreach string k in (Keys) {
        c.inc();
        background (sub (int i) { c.dec(); })($#);
    }
    c.waitForZero();
    return 1;
}

# a reference loop whose body reads its index and calls a function
int sub keep_ref() {
    list<int> l = (1, 2, 3);
    foreach int v in (\l) {
        v += $# + ten();
    }
    return l[2];
}

# a loop over lazy operators whose body reads its index
int sub keep_lazy() {
    list<int> l = (1, 2, 3);
    int n = 0;
    foreach int v in (map $1 * 2, select l, $1 > 1) {
        n += v * $#;
    }
    return n;
}

printf("%y\n", (skip_keys(), skip_call(), skip_ref(), skip_lazy(), skip_operator(), keep_context(),
    keep_background(), keep_ref(), keep_lazy()));
