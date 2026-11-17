#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# methods that a class declares with the name of a parent class's method, in a class with more than one subclass:
# the inheritance list of each method holds each parent method once, so a call tests each variant once (qore#6466)

%modern

class A {
    static int id(int v) { return v; }
    int m(int v) { return v; }
}
class B inherits A {
    static string id(string s) { return "B " + s; }
    string m(string s) { return "B " + s; }
}
# more than one subclass, which resolve the hierarchy of B again
class C inherits B {}
class D inherits B {
    static float id(float f) { return f * 2; }
}
class E inherits D {}

# a generic parent class
class G<T> {
    static T gid(T v) { return v; }
}
class HX inherits G<float> {
    static string gid(string s) { return "HX " + s; }
}
class HXX inherits HX {}
class HXY inherits HX {}

#! returns the variants listed by the RUNTIME-OVERLOAD-ERROR of a call, sorted, with how often each is listed
hash<string, int> sub tested(code c) {
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        hash<string, int> rv = {};
        foreach string line in (ex.desc.split("\n")) {
            if (line =~ /^ +::/) {
                string v = trim(line);
                rv{v} = (rv{v} ?? 0) + 1;
            }
        }
        return rv;
    }
    return {"no exception": 1};
}

list<string> sub run() {
    list<string> out = ();
    auto v = (1,);
    # each variant is listed once
    push out, sprintf("B::id: %y", tested(sub () { B::id(v); }));
    push out, sprintf("C::id: %y", tested(sub () { C::id(v); }));
    push out, sprintf("E::id: %y", tested(sub () { E::id(v); }));
    B b();
    push out, sprintf("B.m: %y", tested(sub () { b.m(v); }));
    C c();
    push out, sprintf("C.m: %y", tested(sub () { c.m(v); }));
    push out, sprintf("HX::gid: %y", tested(sub () { HX::gid(v); }));
    # overload resolution through the subclasses
    push out, sprintf("calls: %y", (B::id(1), B::id("x"), C::id(2), C::id("y"), E::id(3), E::id(1.5), E::id("z"),
        b.m(4), b.m("w"), c.m(5), HX::gid("v")));
    return out;
}

# runs more than once, so that the tiered execution mode compiles the code
list<string> out;
for (int i = 0; i < 3; ++i) {
    out = run();
}
map printf("%s\n", $1), out;
