# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# fixture of generic-function-local-object-type.qtest: objects of generic classes constructed in generic functions and
# methods with their own type parameters as type arguments (qore#6261)

%modern

class B<T> {
    public {
        list<T> l;
    }

    constructor(list<T> x) {
        l = x;
    }

    # the type argument of the object, and its value
    string desc() {
        return sprintf("%s %y", (new list<T>()).fullType(), l);
    }

    # adds a value to the member, which the type argument checks
    string add(auto v) {
        try {
            l += v;
            return sprintf("added: %y", l);
        } catch (hash<ExceptionInfo> ex) {
            return ex.err;
        }
    }
}

hashdecl H<T> {
    T v;
}

string sub viaDecl<U>(list<U> l) { B<U> b(l); return b.desc(); }
string sub viaNew<U>(list<U> l) { B<U> b = new B<U>(l); return b.desc(); }
string sub viaNewExpr<U>(list<U> l) { return (new B<U>(l)).desc(); }
string sub viaClosure<U>(list<U> l) { code c = string sub () { B<U> b(l); return b.desc(); }; return c(); }
string sub viaClosureNew<U>(list<U> l) { code c = string sub () { return (new B<U>(l)).desc(); }; return c(); }
string sub viaNested<U>(list<U> l) { return viaDecl<U>(l); }
string sub viaList<U>(list<U> l) { B<list<U>> b((l,)); return b.desc(); }
string sub viaOptional<U>(list<U> l) { *B<U> b = new B<U>(l); return b.desc(); }
string sub viaHashDecl<U>(U v) { hash<H<U>> h({"v": v}); return h.fullType(); }
string sub viaTwo<U, V>(list<U> l, list<V> m) { B<U> b(l); B<V> c(m); return b.desc() + " / " + c.desc(); }
# the type argument checks the values added to the object's member
string sub addChecked<U>(list<U> l, auto v) { B<U> b(l); return b.add(v); }

class M {
    string viaMethod<U>(list<U> l) { B<U> b(l); return b.desc(); }
    static string viaStatic<U>(list<U> l) { B<U> b(l); return b.desc(); }
}

class G<T> {
    string viaClassParam(list<T> l) { B<T> b(l); return b.desc(); }
    string viaBoth<U>(list<T> l, list<U> m) { B<T> b(l); B<U> c(m); return b.desc() + " / " + c.desc(); }
}

sub run(string label, code c) {
    try {
        printf("%s: %s\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}

sub main() {
    run("declaration", string sub () { return viaDecl<int>((1, 2)); });
    run("declaration, inferred type argument", string sub () { return viaDecl((1, 2)); });
    run("declaration, string", string sub () { return viaDecl<string>(("a",)); });
    run("new", string sub () { return viaNew<int>((1, 2)); });
    run("new expression", string sub () { return viaNewExpr<int>((1, 2)); });
    run("declaration in a closure", string sub () { return viaClosure<int>((1, 2)); });
    run("new expression in a closure", string sub () { return viaClosureNew<int>((1, 2)); });
    run("nested generic call", string sub () { return viaNested<float>((1.5,)); });
    run("list type argument", string sub () { return viaList<int>((1, 2)); });
    run("or-nothing type", string sub () { return viaOptional<int>((1, 2)); });
    run("generic hashdecl", string sub () { return viaHashDecl<int>(1); });
    run("two type parameters", string sub () { return viaTwo<int, string>((1,), ("a",)); });
    run("method", string sub () { M m(); return m.viaMethod<int>((1, 2)); });
    run("static method", string sub () { return M::viaStatic<int>((1, 2)); });
    run("class type parameter", string sub () { G<int> g(); return g.viaClassParam((1, 2)); });
    run("class and method type parameters", string sub () { G<int> g(); return g.viaBoth<string>((1,), ("a",)); });
    run("checked add of a matching value", string sub () { return addChecked<int>((1,), 2); });
    run("checked add of another type", string sub () { return addChecked<int>((1,), "x"); });
    for (int i = 0; i < 3; ++i) {
        run("loop " + i, string sub () { return viaDecl<int>((i,)); });
    }
}

main();
