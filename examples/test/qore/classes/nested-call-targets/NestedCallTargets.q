#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# nested calls whose targets are checked before their arguments are evaluated (issue #5784), at nesting depths below
# and above those where native code reads the target of a call again from memory after its arguments (issue #6872):
# the targets are a global, temporary objects with destructors and code values; one of them is a deleted object or a
# value that is not code, or the innermost argument raises; prints what each case evaluates, raises and destroys

%modern

class T {
    private {
        string name;
    }

    constructor(string name) {
        self.name = name;
    }

    destructor() {
        printf("  destroyed %s\n", name);
    }

    auto id(auto x) {
        return x;
    }
}

our T o("o");
#! a deleted object, through another variable
our T dead;
{
    T deleted("deleted");
    dead = deleted;
    delete deleted;
}
our code cid = auto sub (auto x) { return x; };
our auto notcode = 1;

#! returns a closure, printing its name
code sub make_code(string name) {
    printf("  code %s\n", name);
    return auto sub (auto x) { return x; };
}

#! an argument with a side effect
int sub side(string name) {
    printf("  arg %s\n", name);
    return 1;
}

#! an argument that raises
int sub raise_it(string name) {
    printf("  raise %s\n", name);
    throw "INNER", name;
}

sub check(string name, code c) {
    printf("%s:\n", name);
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        printf("  %s: %s\n", ex.err, ex.desc);
    }
}

sub cases() {
    check("method depth 1, valid", sub () { printf("  %y\n", o.id(side("inner"))); });
    check("method depth 1, invalid target at level 0", sub () { printf("  %y\n", dead.id(side("inner"))); });
    check("method depth 1, inner argument raises", sub () { printf("  %y\n", o.id(raise_it("inner"))); });
    check("closure depth 1, valid", sub () { printf("  %y\n", cid(side("inner"))); });
    check("closure depth 1, invalid target at level 0", sub () { printf("  %y\n", notcode(side("inner"))); });
    check("closure depth 1, inner argument raises", sub () { printf("  %y\n", cid(raise_it("inner"))); });
    check("method depth 4, valid", sub () { printf("  %y\n", o.id((new T("r1")).id(o.id((new T("r3")).id(side("inner")))))); });
    check("method depth 4, invalid target at level 0", sub () { printf("  %y\n", dead.id((new T("r1")).id(o.id((new T("r3")).id(side("inner")))))); });
    check("method depth 4, invalid target at level 2", sub () { printf("  %y\n", o.id((new T("r1")).id(dead.id((new T("r3")).id(side("inner")))))); });
    check("method depth 4, invalid target at level 3", sub () { printf("  %y\n", o.id((new T("r1")).id(o.id(dead.id(side("inner")))))); });
    check("method depth 4, inner argument raises", sub () { printf("  %y\n", o.id((new T("r1")).id(o.id((new T("r3")).id(raise_it("inner")))))); });
    check("closure depth 4, valid", sub () { printf("  %y\n", cid(make_code("c1")(cid(make_code("c3")(side("inner")))))); });
    check("closure depth 4, invalid target at level 0", sub () { printf("  %y\n", notcode(make_code("c1")(cid(make_code("c3")(side("inner")))))); });
    check("closure depth 4, invalid target at level 2", sub () { printf("  %y\n", cid(make_code("c1")(notcode(make_code("c3")(side("inner")))))); });
    check("closure depth 4, invalid target at level 3", sub () { printf("  %y\n", cid(make_code("c1")(cid(notcode(side("inner")))))); });
    check("closure depth 4, inner argument raises", sub () { printf("  %y\n", cid(make_code("c1")(cid(make_code("c3")(raise_it("inner")))))); });
    check("method depth 5, valid", sub () { printf("  %y\n", o.id((new T("r1")).id(o.id((new T("r3")).id(o.id(side("inner"))))))); });
    check("method depth 5, invalid target at level 0", sub () { printf("  %y\n", dead.id((new T("r1")).id(o.id((new T("r3")).id(o.id(side("inner"))))))); });
    check("method depth 5, invalid target at level 2", sub () { printf("  %y\n", o.id((new T("r1")).id(dead.id((new T("r3")).id(o.id(side("inner"))))))); });
    check("method depth 5, invalid target at level 4", sub () { printf("  %y\n", o.id((new T("r1")).id(o.id((new T("r3")).id(dead.id(side("inner"))))))); });
    check("method depth 5, inner argument raises", sub () { printf("  %y\n", o.id((new T("r1")).id(o.id((new T("r3")).id(o.id(raise_it("inner"))))))); });
    check("closure depth 5, valid", sub () { printf("  %y\n", cid(make_code("c1")(cid(make_code("c3")(cid(side("inner"))))))); });
    check("closure depth 5, invalid target at level 0", sub () { printf("  %y\n", notcode(make_code("c1")(cid(make_code("c3")(cid(side("inner"))))))); });
    check("closure depth 5, invalid target at level 2", sub () { printf("  %y\n", cid(make_code("c1")(notcode(make_code("c3")(cid(side("inner"))))))); });
    check("closure depth 5, invalid target at level 4", sub () { printf("  %y\n", cid(make_code("c1")(cid(make_code("c3")(notcode(side("inner"))))))); });
    check("closure depth 5, inner argument raises", sub () { printf("  %y\n", cid(make_code("c1")(cid(make_code("c3")(cid(raise_it("inner"))))))); });
    check("method depth 8, valid", sub () { printf("  %y\n", o.id((new T("r1")).id(o.id((new T("r3")).id(o.id((new T("r5")).id(o.id((new T("r7")).id(side("inner")))))))))); });
    check("method depth 8, invalid target at level 0", sub () { printf("  %y\n", dead.id((new T("r1")).id(o.id((new T("r3")).id(o.id((new T("r5")).id(o.id((new T("r7")).id(side("inner")))))))))); });
    check("method depth 8, invalid target at level 4", sub () { printf("  %y\n", o.id((new T("r1")).id(o.id((new T("r3")).id(dead.id((new T("r5")).id(o.id((new T("r7")).id(side("inner")))))))))); });
    check("method depth 8, invalid target at level 7", sub () { printf("  %y\n", o.id((new T("r1")).id(o.id((new T("r3")).id(o.id((new T("r5")).id(o.id(dead.id(side("inner")))))))))); });
    check("method depth 8, inner argument raises", sub () { printf("  %y\n", o.id((new T("r1")).id(o.id((new T("r3")).id(o.id((new T("r5")).id(o.id((new T("r7")).id(raise_it("inner")))))))))); });
    check("closure depth 8, valid", sub () { printf("  %y\n", cid(make_code("c1")(cid(make_code("c3")(cid(make_code("c5")(cid(make_code("c7")(side("inner")))))))))); });
    check("closure depth 8, invalid target at level 0", sub () { printf("  %y\n", notcode(make_code("c1")(cid(make_code("c3")(cid(make_code("c5")(cid(make_code("c7")(side("inner")))))))))); });
    check("closure depth 8, invalid target at level 4", sub () { printf("  %y\n", cid(make_code("c1")(cid(make_code("c3")(notcode(make_code("c5")(cid(make_code("c7")(side("inner")))))))))); });
    check("closure depth 8, invalid target at level 7", sub () { printf("  %y\n", cid(make_code("c1")(cid(make_code("c3")(cid(make_code("c5")(cid(notcode(side("inner")))))))))); });
    check("closure depth 8, inner argument raises", sub () { printf("  %y\n", cid(make_code("c1")(cid(make_code("c3")(cid(make_code("c5")(cid(make_code("c7")(raise_it("inner")))))))))); });
    check("method depth 40, valid", sub () { printf("  %y\n", o.id((new T("r1")).id(o.id((new T("r3")).id(o.id((new T("r5")).id(o.id((new T("r7")).id(o.id((new T("r9")).id(o.id((new T("r11")).id(o.id((new T("r13")).id(o.id((new T("r15")).id(o.id((new T("r17")).id(o.id((new T("r19")).id(o.id((new T("r21")).id(o.id((new T("r23")).id(o.id((new T("r25")).id(o.id((new T("r27")).id(o.id((new T("r29")).id(o.id((new T("r31")).id(o.id((new T("r33")).id(o.id((new T("r35")).id(o.id((new T("r37")).id(o.id((new T("r39")).id(side("inner")))))))))))))))))))))))))))))))))))))))))); });
    check("method depth 40, invalid target at level 0", sub () { printf("  %y\n", dead.id((new T("r1")).id(o.id((new T("r3")).id(o.id((new T("r5")).id(o.id((new T("r7")).id(o.id((new T("r9")).id(o.id((new T("r11")).id(o.id((new T("r13")).id(o.id((new T("r15")).id(o.id((new T("r17")).id(o.id((new T("r19")).id(o.id((new T("r21")).id(o.id((new T("r23")).id(o.id((new T("r25")).id(o.id((new T("r27")).id(o.id((new T("r29")).id(o.id((new T("r31")).id(o.id((new T("r33")).id(o.id((new T("r35")).id(o.id((new T("r37")).id(o.id((new T("r39")).id(side("inner")))))))))))))))))))))))))))))))))))))))))); });
    check("method depth 40, invalid target at level 20", sub () { printf("  %y\n", o.id((new T("r1")).id(o.id((new T("r3")).id(o.id((new T("r5")).id(o.id((new T("r7")).id(o.id((new T("r9")).id(o.id((new T("r11")).id(o.id((new T("r13")).id(o.id((new T("r15")).id(o.id((new T("r17")).id(o.id((new T("r19")).id(dead.id((new T("r21")).id(o.id((new T("r23")).id(o.id((new T("r25")).id(o.id((new T("r27")).id(o.id((new T("r29")).id(o.id((new T("r31")).id(o.id((new T("r33")).id(o.id((new T("r35")).id(o.id((new T("r37")).id(o.id((new T("r39")).id(side("inner")))))))))))))))))))))))))))))))))))))))))); });
    check("method depth 40, invalid target at level 39", sub () { printf("  %y\n", o.id((new T("r1")).id(o.id((new T("r3")).id(o.id((new T("r5")).id(o.id((new T("r7")).id(o.id((new T("r9")).id(o.id((new T("r11")).id(o.id((new T("r13")).id(o.id((new T("r15")).id(o.id((new T("r17")).id(o.id((new T("r19")).id(o.id((new T("r21")).id(o.id((new T("r23")).id(o.id((new T("r25")).id(o.id((new T("r27")).id(o.id((new T("r29")).id(o.id((new T("r31")).id(o.id((new T("r33")).id(o.id((new T("r35")).id(o.id((new T("r37")).id(o.id(dead.id(side("inner")))))))))))))))))))))))))))))))))))))))))); });
    check("method depth 40, inner argument raises", sub () { printf("  %y\n", o.id((new T("r1")).id(o.id((new T("r3")).id(o.id((new T("r5")).id(o.id((new T("r7")).id(o.id((new T("r9")).id(o.id((new T("r11")).id(o.id((new T("r13")).id(o.id((new T("r15")).id(o.id((new T("r17")).id(o.id((new T("r19")).id(o.id((new T("r21")).id(o.id((new T("r23")).id(o.id((new T("r25")).id(o.id((new T("r27")).id(o.id((new T("r29")).id(o.id((new T("r31")).id(o.id((new T("r33")).id(o.id((new T("r35")).id(o.id((new T("r37")).id(o.id((new T("r39")).id(raise_it("inner")))))))))))))))))))))))))))))))))))))))))); });
    check("closure depth 40, valid", sub () { printf("  %y\n", cid(make_code("c1")(cid(make_code("c3")(cid(make_code("c5")(cid(make_code("c7")(cid(make_code("c9")(cid(make_code("c11")(cid(make_code("c13")(cid(make_code("c15")(cid(make_code("c17")(cid(make_code("c19")(cid(make_code("c21")(cid(make_code("c23")(cid(make_code("c25")(cid(make_code("c27")(cid(make_code("c29")(cid(make_code("c31")(cid(make_code("c33")(cid(make_code("c35")(cid(make_code("c37")(cid(make_code("c39")(side("inner")))))))))))))))))))))))))))))))))))))))))); });
    check("closure depth 40, invalid target at level 0", sub () { printf("  %y\n", notcode(make_code("c1")(cid(make_code("c3")(cid(make_code("c5")(cid(make_code("c7")(cid(make_code("c9")(cid(make_code("c11")(cid(make_code("c13")(cid(make_code("c15")(cid(make_code("c17")(cid(make_code("c19")(cid(make_code("c21")(cid(make_code("c23")(cid(make_code("c25")(cid(make_code("c27")(cid(make_code("c29")(cid(make_code("c31")(cid(make_code("c33")(cid(make_code("c35")(cid(make_code("c37")(cid(make_code("c39")(side("inner")))))))))))))))))))))))))))))))))))))))))); });
    check("closure depth 40, invalid target at level 20", sub () { printf("  %y\n", cid(make_code("c1")(cid(make_code("c3")(cid(make_code("c5")(cid(make_code("c7")(cid(make_code("c9")(cid(make_code("c11")(cid(make_code("c13")(cid(make_code("c15")(cid(make_code("c17")(cid(make_code("c19")(notcode(make_code("c21")(cid(make_code("c23")(cid(make_code("c25")(cid(make_code("c27")(cid(make_code("c29")(cid(make_code("c31")(cid(make_code("c33")(cid(make_code("c35")(cid(make_code("c37")(cid(make_code("c39")(side("inner")))))))))))))))))))))))))))))))))))))))))); });
    check("closure depth 40, invalid target at level 39", sub () { printf("  %y\n", cid(make_code("c1")(cid(make_code("c3")(cid(make_code("c5")(cid(make_code("c7")(cid(make_code("c9")(cid(make_code("c11")(cid(make_code("c13")(cid(make_code("c15")(cid(make_code("c17")(cid(make_code("c19")(cid(make_code("c21")(cid(make_code("c23")(cid(make_code("c25")(cid(make_code("c27")(cid(make_code("c29")(cid(make_code("c31")(cid(make_code("c33")(cid(make_code("c35")(cid(make_code("c37")(cid(notcode(side("inner")))))))))))))))))))))))))))))))))))))))))); });
    check("closure depth 40, inner argument raises", sub () { printf("  %y\n", cid(make_code("c1")(cid(make_code("c3")(cid(make_code("c5")(cid(make_code("c7")(cid(make_code("c9")(cid(make_code("c11")(cid(make_code("c13")(cid(make_code("c15")(cid(make_code("c17")(cid(make_code("c19")(cid(make_code("c21")(cid(make_code("c23")(cid(make_code("c25")(cid(make_code("c27")(cid(make_code("c29")(cid(make_code("c31")(cid(make_code("c33")(cid(make_code("c35")(cid(make_code("c37")(cid(make_code("c39")(raise_it("inner")))))))))))))))))))))))))))))))))))))))))); });
}

# the cases run more than once, so that the tiered execution mode compiles them
for (int i = 0; i < 3; ++i) {
    printf("round %d\n", i);
    cases();
}
