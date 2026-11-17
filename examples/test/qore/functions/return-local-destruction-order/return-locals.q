#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# the locals that a function's return expression reads are destroyed in reverse declaration order with the function's
# other locals, after the return expression, as the AST interpreter destroys them (issue #6309); run by
# return-local-destruction-order.qtest in every execution mode and compiled with qcc.  Each function is called twice:
# the first call of a function through a call reference runs on the IR tier and compiles the function when it returns
# (with the synchronous compilation of the test's JIT and tiered runs), the second runs its native code; nested() and
# param_read(), called by name from a closure, are compiled before their first call in those runs, so the ir run and
# the IR tier run of the test cover their interpreted code

%modern

string log;

class D {
    public {
        string n;
    }

    constructor(string n) {
        self.n = n;
    }

    destructor() {
        log += "~" + n;
    }

    string get() {
        return n;
    }

    string with(D other) {
        return n + other.n;
    }
}

hashdecl H {
    D o;
}

#! the reproducer of the issue: the return reads the local declared last
string sub two_locals() {
    list<D> l = (new D("l"),);
    D k = new D("k");
    log += "|";
    return k.get();
}

#! only the local that the return reads moved to the end
string sub three_locals() {
    list<D> l = (new D("l"),);
    D k = new D("k");
    D z = new D("z");
    log += "|";
    return k.get();
}

#! a member of the local
string sub member_read() {
    D a = new D("a");
    D b = new D("b");
    log += "|";
    return b.n;
}

#! a method call on one local with another local as its argument
string sub method_arg() {
    D a = new D("a");
    D b = new D("b");
    D c = new D("c");
    log += "|";
    return b.with(a);
}

#! a container local that holds the object
string sub container_read() {
    D a = new D("a");
    hash<H> h = <H>{"o": new D("h")};
    log += "|";
    return h.o.get();
}

#! the first local only, which gave the AST's order before the fix
string sub first_local() {
    list<D> l = (new D("l"),);
    D k = new D("k");
    log += "|";
    return l[0].get();
}

#! a constant, which gave the AST's order before the fix
string sub constant() {
    list<D> l = (new D("l"),);
    D k = new D("k");
    log += "|";
    return "x";
}

#! a return in an inner block, which leaves both blocks
string sub nested(bool b) {
    D outer = new D("outer");
    if (b) {
        D inner = new D("inner");
        D last = new D("last");
        log += "|";
        return inner.get() + outer.get();
    }
    return "none";
}

#! the handler of the block runs before its locals are destroyed
string sub handler() {
    D a = new D("a");
    on_exit log += "|exit";
    D b = new D("b");
    log += "|";
    return b.get();
}

#! the original shape: a foreach body that reassigns the list it iterates and a local
string sub foreach_reassign() {
    list<D> t = (new D("t"),);
    D m = new D("m");
    foreach D d in (t) {
        t = (new D("t2"),);
        m = new D("m2");
    }
    log += "|";
    return m.get();
}

#! a parameter that the return reads is the caller's: it is released after the function's locals
string sub param_read(D p) {
    D a = new D("a");
    log += "|";
    return p.get() + a.get();
}

#! a try block around the return
string sub in_try() {
    D a = new D("a");
    try {
        D b = new D("b");
        log += "|";
        return b.get();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

#! the variable of a catch block that the return reads
string sub in_catch() {
    D a = new D("a");
    try {
        throw "E", "e", new D("e");
    } catch (hash<ExceptionInfo> ex) {
        log += "|";
        return ex.arg.get();
    }
}

#! a local of a catch block: only the catch block's own cleanups (its variable and scope) are released after it
string sub catch_block_local() {
    try {
        throw "E", "e", new D("e");
    } catch (hash<ExceptionInfo> ex) {
        D k = new D("k");
        log += "|";
        return k.get();
    }
}

#! the only object local of the function: no other local is released after it
string sub only_local() {
    int i = 1;
    D a = new D("a");
    log += "|";
    return a.get() + i;
}

sub run(string name, code c) {
    for (int i = 1; i <= 2; ++i) {
        log = "";
        string rv = c();
        printf("%s %d: %s %s\n", name, i, rv, log);
    }
}

run("two_locals", \two_locals());
run("three_locals", \three_locals());
run("member_read", \member_read());
run("method_arg", \method_arg());
run("container_read", \container_read());
run("first_local", \first_local());
run("constant", \constant());
run("nested", string sub () { return nested(True); });
run("handler", \handler());
run("foreach_reassign", \foreach_reassign());
run("param_read", string sub () { return param_read(new D("p")); });
run("in_try", \in_try());
run("in_catch", \in_catch());
run("catch_block_local", \catch_block_local());
run("only_local", \only_local());
