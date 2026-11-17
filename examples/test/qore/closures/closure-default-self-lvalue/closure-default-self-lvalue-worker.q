#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# Changes the object through self in default argument values of closures created in methods (qore#5949), which
# have the closure's self variable as its code has.  The cases run three times, so that tiered execution compiles
# them, and print in the last run.

%modern

class Base {
    public {
        int a = 14;
        hash<auto> h = {"x": 1, "y": 2};
        list<auto> l = (1, 2);
        *int n = 5;
    }

    code assign() { return auto sub (auto x = (self.a = 20)) { return x; }; }
    code plusEquals() { return auto sub (auto x = (self.a += 5)) { return x; }; }
    code preIncrement() { return auto sub (auto x = ++self.a) { return x; }; }
    code postIncrement() { return auto sub (auto x = self.a++) { return x; }; }
    code nested() { return auto sub (auto x = (self.h.x = 10)) { return x; }; }
    code subscript() { return auto sub (auto x = (self{"a"} = 30)) { return x; }; }
    code listElement() { return auto sub (auto x = (self.l[1] = 7)) { return x; }; }
    code push() { return auto sub (auto x = (push self.l, 3)) { return x; }; }
    code removeKey() { return auto sub (auto x = remove self.h.y) { return x; }; }
    code deleteKey() { return auto sub (auto x = (delete self.h.x, 0)) { return x; }; }
    code removeMember() { return auto sub (auto x = remove self.n) { return x; }; }
    code deleteMember() { return auto sub (auto x = (delete self.n, 0)) { return x; }; }
    code mergeHash() { return auto sub (auto x = (self += {"a": 5}).a) { return x; }; }
    code removeByName() { return auto sub (auto x = (self -= "n").a) { return x; }; }
    code read() { return auto sub (auto x = self.a) { return x; }; }
    code body() { return auto sub (auto x = 1) { self.a = x + 40; return self.a; }; }
    code mixed() { return auto sub (auto x = (self.a = 50), auto y = self.a + 1) { return (x, y); }; }

    #! calls a closure in the method, where the method's self is instantiated
    auto callInMethod() {
        code c = auto sub (auto x = (self.a = 60)) { return x; };
        return c();
    }
}

#! true in the last run, which prints the results
our bool print_results;

#! runs one case and prints its outcome
sub check(string label, code op) {
    string outcome;
    try {
        outcome = sprintf("%y", op());
    } catch (hash<ExceptionInfo> ex) {
        outcome = ex.err + ": " + ex.desc;
    }
    if (print_results) {
        printf("%s: %s\n", label, outcome);
    }
}

sub run_cases() {
    check("assign", auto sub () { Base b(); return (b.assign()(), b.a); });
    check("+=", auto sub () { Base b(); return (b.plusEquals()(), b.a); });
    check("pre-increment", auto sub () { Base b(); return (b.preIncrement()(), b.a); });
    check("post-increment", auto sub () { Base b(); return (b.postIncrement()(), b.a); });
    check("nested member", auto sub () { Base b(); return (b.nested()(), b.h); });
    check("subscript", auto sub () { Base b(); return (b.subscript()(), b.a); });
    check("list element", auto sub () { Base b(); return (b.listElement()(), b.l); });
    check("push", auto sub () { Base b(); return (b.push()(), b.l); });
    check("remove", auto sub () { Base b(); return (b.removeKey()(), b.h); });
    check("delete", auto sub () { Base b(); return (b.deleteKey()(), b.h); });
    check("remove member", auto sub () { Base b(); return (b.removeMember()(), b.n); });
    check("delete member", auto sub () { Base b(); return (b.deleteMember()(), b.n); });
    check("self +=", auto sub () { Base b(); return (b.mergeHash()(), b.a); });
    check("self -=", auto sub () { Base b(); return (b.removeByName()(), b.n); });
    check("read", auto sub () { Base b(); return (b.read()(), b.a); });
    check("body", auto sub () { Base b(); return (b.body()(), b.a); });
    check("two defaults", auto sub () { Base b(); return (b.mixed()(), b.a); });
    check("argument given", auto sub () { Base b(); return (b.assign()(3), b.a); });
    check("in method", auto sub () { Base b(); return (b.callInMethod(), b.a); });
    check("background", auto sub () {
        Base b();
        code c = b.assign();
        Counter cnt(1);
        auto rv;
        background sub () {
            on_exit cnt.dec();
            try {
                rv = c();
            } catch (hash<ExceptionInfo> ex) {
                rv = ex.err;
            }
        }();
        cnt.waitForZero();
        return (rv, b.a);
    });
}

for (int i = 0; i < 3; ++i) {
    print_results = (i == 2);
    run_cases();
}
