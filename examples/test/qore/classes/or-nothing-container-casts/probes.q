# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the probes of or-nothing-container-casts.qtest (qore#6174, qore#5734): or-nothing and soft list casts to typed
# containers in plain code, with the type parameters of generic methods and functions (U) and of a generic class (T)

%modern

hashdecl HD2<X, Y> {
    X a;
    Y b;
}

#! the forms in plain code; the methods return the values of the casts unchanged (auto)
class P {
    static auto onList(auto v) { return cast<*list<int>>(v); }
    static auto onHash(auto v) { return cast<*hash<string, int>>(v); }
    static auto softList(auto v) { return cast<softlist<int>>(v); }
    static auto onSoftList(auto v) { return cast<*softlist<int>>(v); }
    static auto nested(auto v) { return cast<*list<*hash<string, int>>>(v); }
    static auto hashOfList(auto v) { return cast<*hash<string, *list<int>>>(v); }
}

#! the forms in static methods with the method's U; the methods return the values of the casts unchanged (auto)
class S {
    static auto onList<U>(auto v) { return cast<*list<U>>(v); }
    static auto onHash<U>(auto v) { return cast<*hash<string, U>>(v); }
    static auto softList<U>(auto v) { return cast<softlist<U>>(v); }
    static auto onSoftList<U>(auto v) { return cast<*softlist<U>>(v); }
    static auto nested<U>(auto v) { return cast<*list<*hash<string, U>>>(v); }
    static auto hashOfList<U>(auto v) { return cast<*hash<string, *list<U>>>(v); }
}

#! the forms in normal methods with the method's U, whose return types of U check the values of the casts (qore#5734)
class N {
    *list<U> onList<U>(auto v) { return cast<*list<U>>(v); }
    *hash<string, U> onHash<U>(auto v) { return cast<*hash<string, U>>(v); }
    auto softList<U>(auto v) { return cast<softlist<U>>(v); }
    auto onSoftList<U>(auto v) { return cast<*softlist<U>>(v); }
    *list<*hash<string, U>> nested<U>(auto v) { return cast<*list<*hash<string, U>>>(v); }
    *hash<string, *list<U>> hashOfList<U>(auto v) { return cast<*hash<string, *list<U>>>(v); }
}

#! the forms in generic functions
*list<U> sub f_onList<U>(auto v) { return cast<*list<U>>(v); }
*hash<string, U> sub f_onHash<U>(auto v) { return cast<*hash<string, U>>(v); }
auto sub f_softList<U>(auto v) { return cast<softlist<U>>(v); }
auto sub f_onSoftList<U>(auto v) { return cast<*softlist<U>>(v); }
*list<*hash<string, U>> sub f_nested<U>(auto v) { return cast<*list<*hash<string, U>>>(v); }
*hash<string, *list<U>> sub f_hashOfList<U>(auto v) { return cast<*hash<string, *list<U>>>(v); }

#! the forms with the class's T
class G<T> {
    *list<T> onList(auto v) { return cast<*list<T>>(v); }
    *hash<string, T> onHash(auto v) { return cast<*hash<string, T>>(v); }
    auto softList(auto v) { return cast<softlist<T>>(v); }
    auto onSoftList(auto v) { return cast<*softlist<T>>(v); }
    *list<*hash<string, T>> nested(auto v) { return cast<*list<*hash<string, T>>>(v); }
    *hash<string, *list<T>> hashOfList(auto v) { return cast<*hash<string, *list<T>>>(v); }

    # the class's T and the method's U together
    *list<hash<HD2<T, U>>> both<U>(auto v) { return cast<*list<hash<HD2<T, U>>>>(v); }
    *hash<string, *list<U>> bothHash<U>(T t, auto v) { return cast<*hash<string, *list<U>>>(v); }
}

#! a value and its full type, or the exception raised
string sub probe(string label, code c) {
    try {
        auto v = c();
        return sprintf("%s: %y %s", label, v, v.fullType());
    } catch (hash<ExceptionInfo> ex) {
        return sprintf("%s: %s: %s", label, ex.err, ex.desc);
    }
}

#! the probes of one context: c is called with the name of the form and its argument
list<string> sub context_probes(string ctx, code c) {
    return (
        probe(ctx + ".onList", auto sub () { return c("onList", (1,)); }),
        probe(ctx + ".onListNothing", auto sub () { return c("onList", NOTHING); }),
        probe(ctx + ".onListBad", auto sub () { return c("onList", 1); }),
        probe(ctx + ".onHash", auto sub () { return c("onHash", {"a": 1}); }),
        probe(ctx + ".onHashNothing", auto sub () { return c("onHash", NOTHING); }),
        probe(ctx + ".softList", auto sub () { return c("softList", 1); }),
        probe(ctx + ".softListList", auto sub () { return c("softList", (1,)); }),
        probe(ctx + ".softListNothing", auto sub () { return c("softList", NOTHING); }),
        probe(ctx + ".softListBad", auto sub () { return c("softList", "x"); }),
        probe(ctx + ".onSoftList", auto sub () { return c("onSoftList", 1); }),
        probe(ctx + ".onSoftListList", auto sub () { return c("onSoftList", (1,)); }),
        probe(ctx + ".onSoftListNothing", auto sub () { return c("onSoftList", NOTHING); }),
        probe(ctx + ".onSoftListBad", auto sub () { return c("onSoftList", {"a": "x"}); }),
        probe(ctx + ".nested", auto sub () { return c("nested", ({"a": 1},)); }),
        probe(ctx + ".nestedNothing", auto sub () { return c("nested", NOTHING); }),
        probe(ctx + ".hashOfList", auto sub () { return c("hashOfList", {"a": (1,)}); }),
        probe(ctx + ".hashOfListNothing", auto sub () { return c("hashOfList", NOTHING); }),
    );
}

list<string> sub probes() {
    N n();
    G<int> g();
    list<string> l = context_probes("P", auto sub (string m, auto v) {
        switch (m) {
            case "onList": return P::onList(v);
            case "onHash": return P::onHash(v);
            case "softList": return P::softList(v);
            case "onSoftList": return P::onSoftList(v);
            case "nested": return P::nested(v);
            case "hashOfList": return P::hashOfList(v);
        }
    });
    l += context_probes("S", auto sub (string m, auto v) {
        switch (m) {
            case "onList": return S::onList<int>(v);
            case "onHash": return S::onHash<int>(v);
            case "softList": return S::softList<int>(v);
            case "onSoftList": return S::onSoftList<int>(v);
            case "nested": return S::nested<int>(v);
            case "hashOfList": return S::hashOfList<int>(v);
        }
    });
    l += context_probes("N", auto sub (string m, auto v) {
        switch (m) {
            case "onList": return n.onList<int>(v);
            case "onHash": return n.onHash<int>(v);
            case "softList": return n.softList<int>(v);
            case "onSoftList": return n.onSoftList<int>(v);
            case "nested": return n.nested<int>(v);
            case "hashOfList": return n.hashOfList<int>(v);
        }
    });
    l += context_probes("f", auto sub (string m, auto v) {
        switch (m) {
            case "onList": return f_onList<int>(v);
            case "onHash": return f_onHash<int>(v);
            case "softList": return f_softList<int>(v);
            case "onSoftList": return f_onSoftList<int>(v);
            case "nested": return f_nested<int>(v);
            case "hashOfList": return f_hashOfList<int>(v);
        }
    });
    l += context_probes("G", auto sub (string m, auto v) {
        switch (m) {
            case "onList": return g.onList(v);
            case "onHash": return g.onHash(v);
            case "softList": return g.softList(v);
            case "onSoftList": return g.onSoftList(v);
            case "nested": return g.nested(v);
            case "hashOfList": return g.hashOfList(v);
        }
    });
    G<string> gs();
    l += (
        probe("G<string>.onList", auto sub () { return gs.onList(("s",)); }),
        probe("G<string>.onHash", auto sub () { return gs.onHash({"a": "s"}); }),
        probe("G.both", auto sub () { return g.both<string>(({"a": 1, "b": "s"},)); }),
        probe("G.bothNothing", auto sub () { return g.both<string>(); }),
        probe("G.bothHash", auto sub () { return g.bothHash<string>(1, {"a": ("s",)}); }),
        probe("G.bothHashNothing", auto sub () { return g.bothHash<string>(1); }),
    );
    return l;
}
