# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# fixture of generic-function-local-object-type.qtest: objects of generic classes constructed in generic functions and
# methods with their own type parameters as type arguments (qore#6261), and hashdecl casts with them (qore#6327)

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

# hashdecl casts with the type parameters of the code (qore#6327)
namespace NS {
    hashdecl K<T> {
        T v;
    }
}
string sub castHd<U>(auto v) { return cast<hash<H<U>>>(v).fullType(); }
string sub castHdLiteral<U>() { return cast<hash<H<U>>>({"v": 2}).fullType(); }
string sub castHdNested<U>(auto v) { return cast<hash<NS::K<list<U>>>>(v).fullType(); }
string sub castHdClosure<U>(auto v) { code c = string sub () { return cast<hash<H<U>>>(v).fullType(); }; return c(); }
string sub castHdSecond<U, V>(auto v) { return cast<hash<H<V>>>(v).fullType(); }
string sub castHdOrNothing<U>(auto v) { return sprintf("%y", cast<*hash<H<U>>>(v)); }

class M {
    string viaMethod<U>(list<U> l) { B<U> b(l); return b.desc(); }
    static string viaStatic<U>(list<U> l) { B<U> b(l); return b.desc(); }
}

class G<T> {
    string viaClassParam(list<T> l) { B<T> b(l); return b.desc(); }
    string castClassParam(auto v) { return cast<hash<H<T>>>(v).fullType(); }
    string castMethodParam<U>(auto v) { return cast<hash<H<U>>>(v).fullType(); }
    static string castStatic<U>(auto v) { return cast<hash<NS::K<hash<string, U>>>>(v).fullType(); }
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
    run("hashdecl cast", string sub () { return castHd<int>({"v": 1}); });
    run("hashdecl cast, string", string sub () { return castHd<string>({"v": "a"}); });
    run("hashdecl cast, wrong value", string sub () { return castHd<int>({"v": "a"}); });
    run("hashdecl cast, or nothing", string sub () { return castHdOrNothing<int>(); });
    run("hashdecl cast, literal", string sub () { return castHdLiteral<int>(); });
    run("hashdecl cast, nested type argument", string sub () { return castHdNested<int>({"v": (1,)}); });
    run("hashdecl cast in a closure", string sub () { return castHdClosure<int>({"v": 1}); });
    run("hashdecl cast, second type parameter", string sub () { return castHdSecond<int, string>({"v": "a"}); });
    run("hashdecl cast, class type parameter", string sub () { G<int> g(); return g.castClassParam({"v": 1}); });
    run("hashdecl cast, method type parameter", string sub () {
        G<int> g();
        return g.castMethodParam<string>({"v": "a"});
    });
    run("hashdecl cast, static method", string sub () { return G<int>::castStatic<int>({"v": {"a": 1}}); });
    for (int i = 0; i < 3; ++i) {
        run("loop " + i, string sub () { return viaDecl<int>((i,)); });
    }
}

main();
