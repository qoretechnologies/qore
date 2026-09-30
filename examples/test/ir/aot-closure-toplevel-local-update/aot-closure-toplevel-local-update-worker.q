# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Worker for aot-closure-toplevel-local-update.qtest: functions update top-level local variables that a closure
# captures, called from inside other closures; prints one "<case>: <result>" line per case

%modern

list<string> log = ("top",);
int n = 0;
hash<string, int> h = {"a": 1, "b": 2};
list<string> seen;

# the closure that makes log, n and h closure-bound variables
code capture = sub () { push log, "cap"; n += 1000; h.cap = 1; };

sub reset() {
    log = ("top",);
    n = 0;
    h = {"a": 1, "b": 2};
    seen = ();
}

sub push_top() {
    push log, "fn";
}

sub postinc_top() {
    n++;
}

sub preinc_top() {
    ++n;
}

sub add_assign_top() {
    n += 5;
    log += "plus";
}

sub assign_top() {
    log = ("assigned",);
    n = 100;
}

sub elem_top() {
    log[0] = "elem";
    h.c = 3;
    h.a += 10;
}

sub remove_top() {
    remove h.a;
    delete h.b;
}

sub remove_all_top() {
    remove log;
}

sub list_ops_top() {
    unshift log, "first";
    push log, "last";
    pop log;
    shift log;
    splice log, 0, 0, "spliced";
}

sub rp(reference<list<string>> l, string v) {
    push l, v;
}

sub ref_top() {
    rp(\log, "ref");
    reference<int> r = \n;
    r += 7;
}

sub read_top() {
    seen = log + ("n=" + n,);
}

sub callee_own_local() {
    # a closure-bound local of the function with the name of the top-level one
    list<string> log = ("own",);
    code c = sub () { push log, "own-cl"; };
    c();
    push log, "own-fn";
    seen = log;
    # the top-level variable is not affected
}

sub recurse(int depth) {
    list<string> x = ("d" + depth,);
    code c = sub () {
        if (depth < 2) {
            recurse(depth + 1);
        }
        push x, "c" + depth;
    };
    c();
    seen += x;
    push log, "r" + depth;
}

class C {
    m() {
        push log, "method";
        n += 3;
    }

    static s() {
        push log, "static";
        n += 4;
    }
}

sub run(string name, code c) {
    reset();
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        push seen, "caught:" + ex.err;
    }
    printf("%s: %y %d %y %y\n", name, log, n, h, seen);
}

run("push_direct_ref", \push_top());
run("push_in_closure", sub () { push_top(); });
run("postinc_in_closure", sub () { postinc_top(); postinc_top(); });
run("preinc_in_closure", sub () { preinc_top(); });
run("add_assign_in_closure", sub () { add_assign_top(); });
run("assign_in_closure", sub () { assign_top(); });
run("elem_in_closure", sub () { elem_top(); });
run("remove_in_closure", sub () { remove_top(); });
run("remove_all_in_closure", sub () { remove_all_top(); push_top(); });
run("list_ops_in_closure", sub () { list_ops_top(); });
run("ref_in_closure", sub () { ref_top(); });
run("read_after_update_in_closure", sub () { push_top(); postinc_top(); read_top(); });
run("nested_closures", sub () { code inner = sub () { push_top(); postinc_top(); }; inner(); push_top(); });
run("capturing_closure", sub () { push log, "cl"; push_top(); n += 2; postinc_top(); });
run("callee_own_local_in_closure", sub () { callee_own_local(); push_top(); });
run("recursion_through_closure", sub () { recurse(0); });
run("method_in_closure", sub () { C c(); c.m(); C::s(); });
run("closure_arg_call", sub () { code f = \push_top(); f(); });
run("toplevel_capture_closure", sub () { capture(); push_top(); postinc_top(); });
