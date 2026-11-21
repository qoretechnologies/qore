# Copyright (C) 2026 Qore Technologies, s.r.o.
# qore#6126: captured storage and references bind the substituted type of every kind of type that names a type
# parameter (type parameters, unions, typed code, containers, parameterized classes and hashdecls), a type that names no
# declaration and one that does are bound alternately by the same declaration, and type arguments that are typed code
# types with and without a declaration in their signature are bound.
%modern

class Box<T> {
    public { *T v; }
    constructor(*T v) { self.v = v; }
}
hashdecl HBox<T> { *T v; }
class Foo { }

code sub mkParam<T>() { *T v; return auto sub (auto x) { v = x; return v; }; }
code sub mkUnion<T>() { *union<T, bool> v; return auto sub (auto x) { v = x; return v; }; }
code sub mkCode<T>() { *code<T(T)> v; return auto sub (auto x) { v = x; return v ? v(1) : NOTHING; }; }
code sub mkList<T>() { *list<T> v; return auto sub (auto x) { v = x; return v; }; }
code sub mkSoftList<T>() { *softlist<T> v; return auto sub (auto x) { v = x; return v; }; }
code sub mkPlainSoftList<T>() { softlist<T> v = (); return auto sub (auto x) { v = x; return v; }; }
code sub mkHash<T>() { *hash<string, T> v; return auto sub (auto x) { v = x; return v; }; }
code sub mkBox<T>() { Box<T> v; return auto sub (auto x) { v = x; return v.v; }; }
code sub mkHBox<T>() { *hash<HBox<T>> v; return auto sub (auto x) { v = x; return v ? v.v : NOTHING; }; }
code sub mkRef<T>() {
    *T v;
    reference<*T> r = \v;
    return auto sub (auto x) { r = x; return v; };
}
code sub mkRefList<T>() {
    *list<T> v;
    reference r = \v;
    return auto sub (auto x) { r = x; return v; };
}

string sub outcome(code c, auto x) {
    try {
        return sprintf("%y", c(x));
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

sub check(string name, code c, ...) {
    list<string> l = map outcome(c, $1), argv;
    printf("%s: %s\n", name, (foldl $1 + " | " + $2, l));
}

code sub intCode() { return int sub (int i) { return i + 1; }; }
code sub strCode() { return string sub (string s) { return s + "!"; }; }
code sub fooCode() { return int sub (Foo f) { return 7; }; }
# the type argument of the call is a typed code type, with and without a declaration in its signature; only values that
# the type accepts are given, as *T with a typed code type argument accepts any value (#7144)
code sub mkCodeArg<T>() { *T v; return auto sub (auto x) { v = x; return v ? v(new Foo()) : NOTHING; }; }
code sub mkIntCodeArg<T>() { *T v; return auto sub (auto x) { v = x; return v ? v(2) : NOTHING; }; }

# made before the loop: a Box constructor called in the arguments below raises a type error (#7136)
Box<int> intBox(1);
Box<string> stringBox("a");
for (int iteration = 0; iteration < 3; ++iteration) {
    printf("iteration %d\n", iteration);
    check("param<int>", mkParam<int>(), NOTHING, 1, "2", "x", 1.5);
    check("param<Foo>", mkParam<Foo>(), NOTHING, new Foo(), 1);
    check("param<int> again", mkParam<int>(), NOTHING, 3, "x");
    check("union<int>", mkUnion<int>(), NOTHING, 1, True, "x");
    check("union<string>", mkUnion<string>(), NOTHING, "a", False, 2);
    check("code<int>", mkCode<int>(), NOTHING, intCode(), strCode());
    check("list<int>", mkList<int>(), NOTHING, (1, 2), ("a",), 1);
    check("list<string>", mkList<string>(), NOTHING, ("a",), (1, 2));
    check("softlist<int>", mkSoftList<int>(), NOTHING, 1, (1, "2"), "x");
    check("softlist<int> plain", mkPlainSoftList<int>(), 1, (2, 3), ("x",), "x");
    check("param<code<int(Foo)>>", mkCodeArg<code<int(Foo)>>(), NOTHING, fooCode());
    check("param<code<int(int)>>", mkIntCodeArg<code<int(int)>>(), NOTHING, intCode());
    check("hash<string, int>", mkHash<int>(), NOTHING, {"a": 1}, {"a": "x"});
    check("Box<int>", mkBox<int>(), intBox, stringBox, 1);
    check("HBox<int>", mkHBox<int>(), NOTHING, cast<hash<HBox<int>>>({"v": 2}), {"v": 2}, 1);
    check("ref<int>", mkRef<int>(), NOTHING, 1, "2", "x");
    check("ref<Foo>", mkRef<Foo>(), NOTHING, 1);
    check("ref list<int>", mkRefList<int>(), NOTHING, (1,), ("x",));
}
