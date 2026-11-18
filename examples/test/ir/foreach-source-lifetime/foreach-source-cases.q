%modern
# the cases of foreach-source-lifetime.qtest: the point at which the operand of the list of a foreach statement is
# released when the list is keys, a slice or a list of indices, which the AST interpreter iterates with an iterator
# that keeps a hash, or a list sequence, for the whole loop, and any other operand before the body runs (qore#5770)

# Copyright (C) 2026 Qore Technologies, s.r.o.

class E {
    public {
        string n;
    }

    constructor(string n) {
        self.n = n;
    }

    destructor() {
        printf("  dtor %s\n", n);
    }
}

class C {
    public {
        E e;
        int x = 1;
    }

    constructor(string n) {
        e = new E(n);
    }
}

#! an object whose destructor raises an exception
class D {
    public {
        E e;
    }

    constructor(string n) {
        e = new E(n);
    }

    destructor() {
        throw "D";
    }
}

#! raises an exception
int sub raise() {
    throw "I";
}

#! a hash with an object and an int
hash<auto> sub mkh(string n) {
    return {"a": new E(n), "b": 1};
}

#! a list with an object and two ints
list<auto> sub mkl(string n) {
    return (new E(n), 1, 2);
}

#! a list of indices
list<int> sub ixl() {
    return (1, 2);
}

#! records a point of the loop
sub mark(auto v) {
    printf("  body %y\n", v);
}

#! runs a case
sub t(string name, code<auto()> c) {
    printf("%s:\n", name);
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        printf("  ! %s\n", ex.err);
    }
    printf("  end\n");
}

# keys of a hash: the iterator keeps the hash
sub keys_hash() {
    foreach string k in (keys mkh("kh")) {
        mark(k);
    }
}

# keys of an object: the keys and members are copied first, and the object is released before the body; the copy
# keeps its member
sub keys_object() {
    foreach string k in (keys new C("ko")) {
        mark(k);
    }
}

# the hash of a local that the body removes
sub keys_local_removed() {
    hash<auto> h = mkh("kl");
    foreach string k in (keys h) {
        remove h;
        mark(k);
    }
}

# an untyped loop variable
sub keys_untyped() {
    foreach auto k in (keys mkh("ku")) {
        mark(k);
    }
}

# a slice of a list: the iterator keeps the list
sub slice() {
    foreach auto v in (mkl("sl")[1..2]) {
        mark(v);
    }
}

sub slice_open() {
    foreach auto v in (mkl("so")[1..]) {
        mark(v);
    }
}

# a list of indices: the iterator keeps the list
sub indices() {
    foreach auto v in (mkl("ix")[1, 2]) {
        mark(v);
    }
}

sub indices_list() {
    foreach auto v in (mkl("il")[ixl()]) {
        mark(v);
    }
}

# a slice of a local that the body removes: the iterator keeps the list, which the AST interpreter borrowed from the
# variable when its indices needed no evaluation (qore#5770)
sub slice_local_removed() {
    list<auto> l = mkl("slr");
    foreach auto v in (l[1..2]) {
        remove l;
        mark(v);
    }
}

# a list of indices of a local that the body removes
sub indices_local_removed() {
    list<auto> l = mkl("ilr");
    foreach auto v in (l[1, 2]) {
        remove l;
        mark(v);
    }
}

# an empty range: the slice is made first, from the list evaluated once, and the list released before the body
sub slice_empty() {
    foreach auto v in (mkl("se")[5..6]) {
        mark(v);
    }
}

# one index: the value is made first, and the list released before the body
sub one_index() {
    foreach auto v in (mkl("oi")[1]) {
        mark(v);
    }
}

# a key of a hash: the value is made first
sub hash_key() {
    foreach auto v in (mkh("hk"){"b"}) {
        mark(v);
    }
}

# a break, a return, an exception and a nested loop
sub keys_break() {
    foreach string k in (keys mkh("kb")) {
        mark(k);
        break;
    }
    printf("  after\n");
}

sub keys_return() {
    on_exit printf("  on_exit\n");
    foreach string k in (keys mkh("kr")) {
        mark(k);
        return;
    }
}

sub slice_return() {
    on_exit printf("  on_exit\n");
    foreach auto v in (mkl("sr")[1..2]) {
        mark(v);
        return;
    }
}

sub keys_exception() {
    try {
        foreach string k in (keys mkh("kx")) {
            mark(k);
            throw "X";
        }
    } catch (hash<ExceptionInfo> ex) {
        printf("  catch %s\n", ex.err);
    }
}

# an exception that leaves the function
sub keys_exception_out() {
    on_exit printf("  on_exit\n");
    foreach string k in (keys mkh("ky")) {
        mark(k);
        throw "Y";
    }
}

sub keys_nested() {
    foreach string k in (keys mkh("ko")) {
        foreach string j in (keys mkh("ki")) {
            mark(k + j);
        }
    }
}

sub keys_continue() {
    foreach string k in (keys mkh("kc")) {
        if (k == "a") {
            continue;
        }
        mark(k);
    }
    printf("  after\n");
}

# the destructor of a temporary of the list raises when the temporaries are released, before the body: the
# iterator, whose copy of the object's members keeps a member, is released before the handler
sub keys_dtor_exception() {
    try {
        foreach string k in (keys new D("kd")) {
            mark(k);
        }
    } catch (hash<ExceptionInfo> ex) {
        printf("  catch %s\n", ex.err);
    }
}

# an index raises: the sequence is released before the handler
sub slice_index_exception() {
    try {
        foreach auto v in (mkl("sx")[raise()..2]) {
            mark(v);
        }
    } catch (hash<ExceptionInfo> ex) {
        printf("  catch %s\n", ex.err);
    }
}

# streaming operators over keys and a slice
sub take_keys() {
    foreach auto v in (take 1, keys mkh("tk")) {
        mark(v);
    }
}

sub drop_slice() {
    foreach auto v in (drop 1, mkl("ds")[1..2]) {
        mark(v);
    }
}

# map and select over keys, a slice and indices
sub map_keys() {
    foreach auto v in (map $1, keys mkh("mk")) {
        mark(v);
    }
}

sub select_slice() {
    foreach auto v in (select mkl("ss")[1..2], $1 > 0) {
        mark(v);
    }
}

sub map_indices() {
    foreach auto v in (map $1, mkl("mi")[1, 2]) {
        mark(v);
    }
}

sub main() {
    t("keys hash", \keys_hash());
    t("keys object", \keys_object());
    t("keys local removed", \keys_local_removed());
    t("keys untyped", \keys_untyped());
    t("slice", \slice());
    t("slice open", \slice_open());
    t("indices", \indices());
    t("indices list", \indices_list());
    t("slice local removed", \slice_local_removed());
    t("indices local removed", \indices_local_removed());
    t("slice empty", \slice_empty());
    t("one index", \one_index());
    t("hash key", \hash_key());
    t("keys break", \keys_break());
    t("keys return", \keys_return());
    t("slice return", \slice_return());
    t("keys exception", \keys_exception());
    t("keys exception out", \keys_exception_out());
    t("keys nested", \keys_nested());
    t("keys continue", \keys_continue());
    t("keys destructor exception", \keys_dtor_exception());
    t("slice index exception", \slice_index_exception());
    t("take keys", \take_keys());
    t("drop slice", \drop_slice());
    t("map keys", \map_keys());
    t("select slice", \select_slice());
    t("map indices", \map_indices());
}

main();
