#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The cases of raise-release-order.qtest (qore#5649, qore#5670, qore#5714): the order in which values are released
# when a statement or an operator raises: the list of a foreach statement, the elements of a streaming operator whose
# predicate, map stage or consumer raises, and the temporaries of a call that raises or whose release raises.  Each
# case logs the creation and the destruction of its objects; the output is the same in every execution mode and
# compiled with qcc.

%modern

our list<string> log = ();

sub event(string s) {
    log += s;
}

# qore#5649: the list of a foreach statement that raises
class Obj {
    private {
        string n;
    }

    constructor(string n) {
        self.n = n;
        event("+" + n);
    }

    destructor() {
        event("~" + n);
    }

    string name() {
        return n;
    }
}

# the assignment of the loop variable raises (a type error)
sub foreach_type_error() {
    on_exit event("on_exit");
    foreach Obj o in ((new Obj("l"), 1)) {
        event("body " + o.name());
    }
}

# the list is the value of a local variable
sub foreach_local_list() {
    on_exit event("on_exit");
    list<auto> l = (new Obj("l"), 1);
    foreach Obj o in (l) {
        event("body " + o.name());
    }
}

# the body raises
sub foreach_body_raises() {
    on_exit event("on_exit");
    foreach Obj o in ((new Obj("l"),)) {
        event("body " + o.name());
        throw "BODY";
    }
}

# an iterator over the list
sub foreach_iterator_raises() {
    on_exit event("on_exit");
    foreach Obj o in (new ListIterator((new Obj("l"), 1))) {
        event("body " + o.name());
    }
}

# an on_error handler
sub foreach_on_error() {
    on_error event("on_error");
    foreach Obj o in ((new Obj("l"), 1)) {
        event("body " + o.name());
    }
}

# qore#5670: the elements of a streaming operator whose predicate or consumer raises
class Holder {
    public {
        string name;
    }

    constructor(string n) {
        name = n;
    }

    destructor() {
        event("released " + name);
    }
}

bool sub sel_fail(Holder h, string name) {
    if (h.name == name) {
        throw "SELECT";
    }
    return True;
}

string sub map_fail(Holder h, string name) {
    sel_fail(h, name);
    return h.name;
}

# an iterator that makes each element when it gets to it
class HolderIterator inherits AbstractIterator {
    private {
        int n;
        int i = 0;
        *Holder cur;
    }

    constructor(int n) {
        self.n = n;
    }

    bool next() {
        remove cur;
        if (i >= n) {
            return False;
        }
        cur = new Holder("h" + ++i);
        return True;
    }

    auto getValue() {
        auto v = cur;
        remove cur;
        return v;
    }

    bool valid() {
        return exists cur;
    }
}

# the elements: made by the iterator, or a list of them that the iterator holds
our bool list_iter;

AbstractIterator sub holders(int n) {
    if (list_iter) {
        list<Holder> l = map new Holder("h" + $1), xrange(1, n);
        return new ListIterator(l);
    }
    return new HolderIterator(n);
}

sub boom(auto v) {
    throw "CONSUMER";
}

sub s_takewhile() {
    list<Holder> r = (takewhile sel_fail($1, "h3"), holders(4));
}

sub s_count() {
    int c = (count sel_fail($1, "h2"), holders(3));
}

sub s_consumer() {
    boom((take 2, holders(3)));
}

sub s_plain_consumer() {
    boom(holders(2));
}

sub s_select() {
    list<auto> r = (select holders(3), sel_fail($1, "h2"));
}

sub s_map() {
    list<auto> r = (map map_fail($1, "h2"), holders(3));
}

sub s_any() {
    bool b = (any !sel_fail($1, "h2"), holders(3));
}

sub s_first() {
    auto v = (first !sel_fail($1, "h2"), holders(3));
}

# qore#5714: the temporaries of a predicate or map-stage call that raises, and of a call whose release raises
class H {
    public {
        string n;
        bool raise_del;
    }

    constructor(string n, bool raise_del = False) {
        self.n = n;
        self.raise_del = raise_del;
    }

    destructor() {
        event("del " + n + (raise_del ? " (raises)" : ""));
        if (raise_del) {
            throw "DEL";
        }
    }
}

H sub mk(auto v) {
    return new H(sprintf("%s", v));
}

H sub mkdt(auto v) {
    return new H(sprintf("%s", v), True);
}

int sub stage(H h, string fail) {
    if (h.n == fail) {
        throw "STAGE";
    }
    return 1;
}

bool sub pred(H h, string fail) {
    if (h.n == fail) {
        throw "PRED";
    }
    return True;
}

list<H> sub os(int n) {
    return map new H("h" + $1), xrange(1, n);
}

int sub two(H a, H b) {
    return 1;
}

# a map stage of a fused pipeline raises; its argument is a temporary made from the element (fmap_n_raises)
sub fmap_n_raises() {
    list<auto> r = (map stage(mk($1), "3"), (1, 2, 3, 4));
}

# a map stage over objects raises
sub map_obj_raises() {
    list<auto> r = (map stage($1, "h2"), os(3));
}

# a select predicate whose argument is a temporary raises
sub select_tmp_raises() {
    list<auto> r = (select (1, 2, 3), pred(mk($1), "2"));
}

# a select over a map whose predicate raises
sub map_select_raises() {
    list<auto> r = (select (map mk($1), (1, 2, 3)), pred($1, "2"));
}

# the destructor of a map stage's temporary raises: the temporaries that a functional operator creates for an element
# are destroyed once the element's value has been computed (qore#6288)
sub map_mkdt() {
    list<auto> l2 = ();
    l2 += (map mkdt($1.n).n, os(2));
}

# on_error in a called function without a try statement: a destructor raises at the release of the temporaries of
# the statement, the first argument's or the second's
sub on_error_dt() {
    on_error event("on_error");
    int x = two(mkdt("h1"), mk("h2"));
}

sub on_error_dt2() {
    on_error event("on_error");
    int x = two(mk("h1"), mkdt("h2"));
}

# a chain receiver in an operand that is evaluated at run time
sub chain_recv() {
    list<auto> r = map new hash<string, auto>({"n": mk($1).n}), (1, 2);
    event(sprintf("%y", r));
}

sub run(string name, string f) {
    log = ();
    try {
        call_function(f);
    } catch (hash<ExceptionInfo> ex) {
        event("caught " + ex.err);
    }
    event("after");
    printf("%s: %s\n", name, log.join(", "));
}

foreach string f in (("foreach_type_error", "foreach_local_list", "foreach_body_raises", "foreach_iterator_raises",
        "foreach_on_error")) {
    run(f, f);
}
foreach bool li in ((False, True)) {
    list_iter = li;
    foreach string f in (("s_takewhile", "s_count", "s_consumer", "s_plain_consumer", "s_select", "s_map", "s_any",
            "s_first")) {
        run(f + (li ? "(list)" : "(gen)"), f);
    }
}
foreach string f in (("fmap_n_raises", "map_obj_raises", "select_tmp_raises", "map_select_raises", "map_mkdt",
        "on_error_dt", "on_error_dt2", "chain_recv")) {
    run(f, f);
}
