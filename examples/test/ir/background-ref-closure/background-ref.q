%modern
# background expressions in closures that take references to local variables; see background-ref-closure.qtest

#! the case of issue #5677
string sub in_closure() {
    string rv;
    code c = sub () {
        int k = 3;
        Counter c2(1);
        background sub (reference<int> r) { r += 100; c2.dec(); }(\k);
        c2.waitForZero();
        rv = sprintf("k=%d", k);
    };
    c();
    return rv;
}

#! the function that creates the closure has a background helper with a reference parameter of the same type, and so
#! a variable of the same (internal) name as the closure's helper parameter
string sub same_name_in_parent() {
    int a = 1;
    Counter ca(1);
    background sub (reference<int> r) { r += 10; ca.dec(); }(\a);
    ca.waitForZero();
    string rv;
    code c = sub () {
        int b = 2;
        Counter cb(1);
        background sub (reference<int> r) { r *= 7; cb.dec(); }(\b);
        cb.waitForZero();
        rv = sprintf("b=%d", b);
    };
    c();
    return sprintf("a=%d %s", a, rv);
}

#! a background expression in an on_block_exit handler of a closure
string sub in_handler() {
    string rv;
    code c = sub () {
        int h = 5;
        on_exit {
            Counter ch(1);
            background sub (reference<int> r) { r -= 2; ch.dec(); }(\h);
            ch.waitForZero();
            rv = sprintf("h=%d", h);
        }
    };
    c();
    return rv;
}

#! a background expression in the expression of another one (a nested helper), each with references
string sub nested() {
    string rv;
    code c = sub () {
        int x = 1;
        int y = 2;
        Counter cx(1);
        Counter cy(1);
        background sub (int tid, reference<int> r) { cy.waitForZero(); r += 20; cx.dec(); }(
            background sub (reference<int> r2) { r2 += 40; cy.dec(); }(\y), \x);
        cx.waitForZero();
        rv = sprintf("x=%d y=%d", x, y);
    };
    c();
    return rv;
}

#! several references and other values taken when the thread starts
string sub several() {
    string rv;
    code c = sub (string pfx) {
        int m = 1;
        string s = "s";
        Counter cs(1);
        background sub (reference<int> r1, reference<string> r2, string p) {
            r1 += 2;
            r2 += p;
            cs.dec();
        }(\m, \s, pfx + "!");
        cs.waitForZero();
        rv = sprintf("m=%d s=%s", m, s);
    };
    c("p");
    return rv;
}

#! a reference to a parameter of the closure
string sub closure_param() {
    code c = string sub (int prm) {
        Counter cp(1);
        background sub (reference<int> r) { r *= 3; cp.dec(); }(\prm);
        cp.waitForZero();
        return sprintf("prm=%d", prm);
    };
    return c(4);
}

sub main() {
    # every case runs more than once, so that the closures are compiled with tiered execution
    for (int i = 0; i < 3; ++i) {
        printf("in closure: %s\n", in_closure());
        printf("same name in parent: %s\n", same_name_in_parent());
        printf("in handler: %s\n", in_handler());
        printf("nested: %s\n", nested());
        printf("several: %s\n", several());
        printf("closure param: %s\n", closure_param());
    }
}

main();
