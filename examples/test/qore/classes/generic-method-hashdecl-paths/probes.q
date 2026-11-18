# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the probes of generic-method-hashdecl-paths.qtest (qore#5720, qore#5661): the construction, cast and assignment of
# hashdecls and typed containers whose types use the type parameters of a generic method or function

%modern

hashdecl HD<T> {
    T a;
}

hashdecl HD2<X, Y> {
    X a;
    Y b;
}

#! the forms in static methods
class S {
    static hash<HD<U>> ctor<U>(U v) { hash<HD<U>> h({"a": v}); return h; }
    static hash<HD<U>> castForm<U>(U v) { return cast<hash<HD<U>>>({"a": v}); }
    static hash<HD<U>> newForm<U>(U v) { return new hash<HD<U>>({"a": v}); }
    static hash<string, U> assign<U>(U v) { hash<string, U> h = {"a": v}; return h; }
    static hash<string, U> typedCtor<U>(U v) { hash<string, U> h({"a": v}); return h; }
    static hash<string, U> typedCast<U>(U v) { return cast<hash<string, U>>({"a": v}); }
    static hash<string, U> typedNew<U>(U v) { return new hash<string, U>({"a": v}); }
    static list<U> listCast<U>(U v) { return cast<list<U>>((v,)); }
    static list<hash<HD<U>>> listHdCast<U>(U v) { return cast<list<hash<HD<U>>>>((cast<hash<HD<U>>>({"a": v}),)); }
    static list<hash<HD<U>>> listHdAssign<U>(U v) {
        list<hash<HD<U>>> l = (cast<hash<HD<U>>>({"a": v}),);
        return l;
    }
    static hash<string, list<U>> hashListCast<U>(U v) { return cast<hash<string, list<U>>>({"a": (v,)}); }
    static hash<string, list<U>> hashListCtor<U>(U v) { hash<string, list<U>> h({"a": (v,)}); return h; }
    static hash<string, list<U>> hashListAssign<U>(U v) { hash<string, list<U>> h = {"a": (v,)}; return h; }
    static hash<string, hash<HD<U>>> hashHdCast<U>(U v) {
        return cast<hash<string, hash<HD<U>>>>({"a": cast<hash<HD<U>>>({"a": v})});
    }
    static hash<HD<list<U>>> hdOfList<U>(U v) { return cast<hash<HD<list<U>>>>({"a": (v,)}); }
    # the value is checked against the type argument at run time
    static hash<HD<U>> castAny<U>(auto v) { return cast<hash<HD<U>>>({"a": v}); }
    static hash<HD<U>> newAny<U>(auto v) { return new hash<HD<U>>({"a": v}); }
    # the errors of a cast of a value of another type name the type of the instantiation
    static hash<HD<U>> castNonHash<U>(auto v) { return cast<hash<HD<U>>>(v); }
    static hash<string, U> typedCastNonHash<U>(auto v) { return cast<hash<string, U>>(v); }
    static list<U> listCastNonList<U>(auto v) { return cast<list<U>>(v); }
    static auto onHashCastNonHash<U>(auto v) { return cast<*hash<string, U>>(v); }
    # or-nothing and softlist casts
    static auto onHdCast<U>(U v) { return cast<*hash<HD<U>>>({"a": v}); }
    static auto onHdNothing<U>() { return cast<*hash<HD<U>>>(NOTHING); }
    static auto onListCast<U>(U v) { return cast<*list<U>>((v,)); }
    static auto onHashCast<U>(U v) { return cast<*hash<string, U>>({"a": v}); }
    static auto softlistCast<U>(U v) { return cast<softlist<U>>(v); }
    static auto softlistListCast<U>(U v) { return cast<softlist<U>>((v,)); }
    # default values of parameters (qore#6173)
    static auto defNew<U>(U v, auto h = new hash<HD<U>>()) { return h; }
    static auto defCast<U>(U v, auto h = cast<hash<HD<U>>>({})) { return h; }
    static auto defNewInit<U>(U v, auto h = new hash<HD<U>>({"a": 2})) { return h; }
    static auto defCastList<U>(U v, auto l = cast<list<hash<HD<U>>>>(())) { return l; }
    static auto defOnCast<U>(U v, auto h = cast<*hash<HD<U>>>({})) { return h; }
    static auto defNested<U>(U v, auto h = cast<hash<HD<list<U>>>>({})) { return h; }
    static auto defTyped<U>(U v, hash<HD<U>> h = new hash<HD<U>>()) { return h; }
    # in a closure in the generic method
    static auto closureCast<U>(U v) { code c = auto sub () { return cast<hash<HD<U>>>({"a": v}); }; return c(); }
    static auto closureNew<U>(U v) { code c = auto sub () { return new hash<HD<U>>({"a": v}); }; return c(); }
    # in a background thread, which evaluates a copy of the expression
    static auto bgCast<U>(U v) {
        Queue q();
        return bg_result(q, background q.push(cast<hash<HD<U>>>({"a": v})));
    }
    static auto bgList<U>(U v) {
        Queue q();
        return bg_result(q, background q.push(cast<list<U>>((v,))));
    }
}

#! the forms in normal methods
class N {
    hash<HD<U>> ctor<U>(U v) { hash<HD<U>> h({"a": v}); return h; }
    hash<HD<U>> castForm<U>(U v) { return cast<hash<HD<U>>>({"a": v}); }
    hash<HD<U>> newForm<U>(U v) { return new hash<HD<U>>({"a": v}); }
    hash<string, U> assign<U>(U v) { hash<string, U> h = {"a": v}; return h; }
    hash<string, U> typedCtor<U>(U v) { hash<string, U> h({"a": v}); return h; }
    hash<string, U> typedCast<U>(U v) { return cast<hash<string, U>>({"a": v}); }
    hash<string, U> typedNew<U>(U v) { return new hash<string, U>({"a": v}); }
    list<U> listCast<U>(U v) { return cast<list<U>>((v,)); }
    list<hash<HD<U>>> listHdCast<U>(U v) { return cast<list<hash<HD<U>>>>((cast<hash<HD<U>>>({"a": v}),)); }
    list<hash<HD<U>>> listHdAssign<U>(U v) { list<hash<HD<U>>> l = (cast<hash<HD<U>>>({"a": v}),); return l; }
    hash<string, list<U>> hashListCast<U>(U v) { return cast<hash<string, list<U>>>({"a": (v,)}); }
    hash<string, list<U>> hashListCtor<U>(U v) { hash<string, list<U>> h({"a": (v,)}); return h; }
    hash<string, list<U>> hashListAssign<U>(U v) { hash<string, list<U>> h = {"a": (v,)}; return h; }
    hash<string, hash<HD<U>>> hashHdCast<U>(U v) {
        return cast<hash<string, hash<HD<U>>>>({"a": cast<hash<HD<U>>>({"a": v})});
    }
    hash<HD<list<U>>> hdOfList<U>(U v) { return cast<hash<HD<list<U>>>>({"a": (v,)}); }
}

#! the forms in generic functions
hash<HD<U>> sub f_ctor<U>(U v) { hash<HD<U>> h({"a": v}); return h; }
hash<HD<U>> sub f_castForm<U>(U v) { return cast<hash<HD<U>>>({"a": v}); }
hash<HD<U>> sub f_newForm<U>(U v) { return new hash<HD<U>>({"a": v}); }
hash<string, U> sub f_assign<U>(U v) { hash<string, U> h = {"a": v}; return h; }
hash<string, U> sub f_typedCtor<U>(U v) { hash<string, U> h({"a": v}); return h; }
hash<string, U> sub f_typedCast<U>(U v) { return cast<hash<string, U>>({"a": v}); }
hash<string, U> sub f_typedNew<U>(U v) { return new hash<string, U>({"a": v}); }
list<U> sub f_listCast<U>(U v) { return cast<list<U>>((v,)); }
list<hash<HD<U>>> sub f_listHdCast<U>(U v) { return cast<list<hash<HD<U>>>>((cast<hash<HD<U>>>({"a": v}),)); }
list<hash<HD<U>>> sub f_listHdAssign<U>(U v) { list<hash<HD<U>>> l = (cast<hash<HD<U>>>({"a": v}),); return l; }
hash<string, list<U>> sub f_hashListCast<U>(U v) { return cast<hash<string, list<U>>>({"a": (v,)}); }
hash<string, list<U>> sub f_hashListCtor<U>(U v) { hash<string, list<U>> h({"a": (v,)}); return h; }
hash<string, list<U>> sub f_hashListAssign<U>(U v) { hash<string, list<U>> h = {"a": (v,)}; return h; }
hash<string, hash<HD<U>>> sub f_hashHdCast<U>(U v) {
    return cast<hash<string, hash<HD<U>>>>({"a": cast<hash<HD<U>>>({"a": v})});
}
hash<HD<list<U>>> sub f_hdOfList<U>(U v) { return cast<hash<HD<list<U>>>>({"a": (v,)}); }

#! the forms in a generic method of a generic class: the paths use the class's T and the method's U
class G<T> {
    hash<HD2<T, U>> ctor<U>(T t, U v) { hash<HD2<T, U>> h({"a": t, "b": v}); return h; }
    hash<HD2<T, U>> castForm<U>(T t, U v) { return cast<hash<HD2<T, U>>>({"a": t, "b": v}); }
    hash<HD2<T, U>> newForm<U>(T t, U v) { return new hash<HD2<T, U>>({"a": t, "b": v}); }
    hash<string, hash<HD2<T, U>>> hashHdCast<U>(T t, U v) {
        return cast<hash<string, hash<HD2<T, U>>>>({"k": cast<hash<HD2<T, U>>>({"a": t, "b": v})});
    }
    list<hash<HD2<T, U>>> listHdCast<U>(T t, U v) {
        return cast<list<hash<HD2<T, U>>>>((cast<hash<HD2<T, U>>>({"a": t, "b": v}),));
    }
    hash<string, list<U>> hashListAssign<U>(T t, U v) { hash<string, list<U>> h = {"k": (v,)}; return h; }
    static hash<HD2<T, U>> sctor<U>(T t, U v) { hash<HD2<T, U>> h({"a": t, "b": v}); return h; }
    # a default value with the class's T and the method's U (qore#6173)
    auto defHd2<U>(T t, U v, auto h = new hash<HD2<T, U>>()) { return h; }
}

#! returns the value that a background thread pushed, or raises an exception if the thread ended without one
/** The thread is joined, so the result does not depend on when the thread starts running: a thread that background
    started is not in thread_list() before it runs, and thread_join() waits for it all the same (issue #6452)
*/
auto sub bg_result(Queue q, int tid) {
    if (!thread_join(tid, 30s)) {
        throw "BACKGROUND-ERROR", sprintf("thread %d gave no result in 30 seconds", tid);
    }
    if (!q.size()) {
        throw "BACKGROUND-ERROR", sprintf("thread %d ended without a result", tid);
    }
    return q.get();
}

#! the paths of a generic class with only the class's T, run for two instantiations of the class in one program
class GT<T> {
    auto castT(T t) { return cast<hash<HD<T>>>({"a": t}); }
    auto newT(T t) { return new hash<HD<T>>({"a": t}); }
    auto ctorT(T t) { hash<HD<T>> h({"a": t}); return h; }
    auto listT(T t) { return cast<list<T>>((t,)); }
    # default values of parameters with the class's T (qore#6173)
    auto defNewT(auto h = new hash<HD<T>>()) { return h; }
    auto defCastT(auto h = cast<hash<HD<T>>>({})) { return h; }
}

#! member defaults of a generic class with the class's T (qore#6241)
class GM<T> {
    public {
        hash<HD<T>> h1 = new hash<HD<T>>();
        hash<HD<T>> h2();
        hash<HD<T>> h3 = cast<hash<HD<T>>>({});
        # with constructor arguments
        hash<HD<T>> h4({});
    }
}

#! a subclass of an instantiation of the generic class
class GMI inherits GM<int> {
}

#! the same member defaults without type parameters, a control
class MC {
    public {
        hash<HD<int>> h1 = new hash<HD<int>>();
        hash<HD<int>> h2();
        hash<HD<int>> h3 = cast<hash<HD<int>>>({});
    }
}

#! one of the threads running different instantiations of the same generic methods at the same time
sub thread_worker(int t, Queue q) {
    string err;
    try {
        for (int i = 0; i < 200 && !err; ++i) {
            string got;
            string want;
            if (t % 2) {
                got = S::castForm<string>("s").fullType() + "," + S::newForm<string>("s").fullType();
                want = "hash<HD<string>>,hash<HD<string>>";
            } else {
                got = S::castForm<int>(i).fullType() + "," + S::newForm<int>(i).fullType();
                want = "hash<HD<int>>,hash<HD<int>>";
            }
            if (got != want) {
                err = sprintf("thread %d: %s", t, got);
            }
        }
    } catch (hash<ExceptionInfo> ex) {
        # every thread gives a result, so that a failure is reported at once
        err = sprintf("thread %d: %s: %s", t, ex.err, ex.desc);
    }
    q.push(err ?? "");
}

#! runs threads with different instantiations of the same generic methods at the same time
string sub thread_probe() {
    Queue q();
    for (int t = 0; t < 4; ++t) {
        background thread_worker(t, q);
    }
    list<string> errs;
    for (int t = 0; t < 4; ++t) {
        string e = q.get(30s);
        if (e) {
            errs += e;
        }
    }
    return errs ? sprintf("%y", errs) : "ok";
}

#! a concrete control: the same forms without type parameters
hash<HD<int>> sub concrete_cast(int v) { return cast<hash<HD<int>>>({"a": v}); }
hash<HD<int>> sub concrete_new(int v) { return new hash<HD<int>>({"a": v}); }

string sub probe(string label, code c) {
    try {
        auto v = c();
        return sprintf("%s: %y %s", label, v, v.fullType());
    } catch (hash<ExceptionInfo> ex) {
        return sprintf("%s: %s: %s", label, ex.err, ex.desc);
    }
}

#! runs the probes
list<string> sub probes() {
    N n();
    G<string> g();
    GT<int> gi();
    GT<string> gs();
    list<string> l = (
        probe("S.ctor", auto sub () { return S::ctor<int>(1); }),
        probe("S.cast", auto sub () { return S::castForm<int>(1); }),
        probe("S.new", auto sub () { return S::newForm<int>(1); }),
        probe("S.assign", auto sub () { return S::assign<int>(1); }),
        probe("S.typedCtor", auto sub () { return S::typedCtor<int>(1); }),
        probe("S.typedCast", auto sub () { return S::typedCast<int>(1); }),
        probe("S.typedNew", auto sub () { return S::typedNew<int>(1); }),
        probe("S.listCast", auto sub () { return S::listCast<int>(1); }),
        probe("S.listHdCast", auto sub () { return S::listHdCast<int>(1); }),
        probe("S.listHdAssign", auto sub () { return S::listHdAssign<int>(1); }),
        probe("S.hashListCast", auto sub () { return S::hashListCast<int>(1); }),
        probe("S.hashListCtor", auto sub () { return S::hashListCtor<int>(1); }),
        probe("S.hashListAssign", auto sub () { return S::hashListAssign<int>(1); }),
        probe("S.hashHdCast", auto sub () { return S::hashHdCast<int>(1); }),
        probe("S.hdOfList", auto sub () { return S::hdOfList<int>(1); }),
        # another instantiation of the same method
        probe("S.cast<string>", auto sub () { return S::castForm<string>("s"); }),
        probe("S.new<string>", auto sub () { return S::newForm<string>("s"); }),
        # the type argument is checked: a string is not an int
        probe("S.castBad", auto sub () { return S::castAny<int>("s"); }),
        probe("S.newBad", auto sub () { return S::newAny<int>("s"); }),
        probe("S.castNonHash", auto sub () { return S::castNonHash<int>(1); }),
        probe("S.typedCastNonHash", auto sub () { return S::typedCastNonHash<int>(1); }),
        probe("S.listCastNonList", auto sub () { return S::listCastNonList<int>(1); }),
        probe("S.onHashCastNonHash", auto sub () { return S::onHashCastNonHash<int>(1); }),
        probe("S.bgCast", auto sub () { return S::bgCast<int>(1); }),
        probe("S.bgList", auto sub () { return S::bgList<int>(1); }),
        probe("S.onHdCast", auto sub () { return S::onHdCast<int>(1); }),
        probe("S.onHdNothing", auto sub () { return S::onHdNothing<int>(); }),
        probe("S.softlistListCast", auto sub () { return S::softlistListCast<int>(1); }),
        probe("S.defNew", auto sub () { return S::defNew<int>(1); }),
        probe("S.defCast", auto sub () { return S::defCast<int>(1); }),
        probe("S.defNewInit", auto sub () { return S::defNewInit<int>(1); }),
        probe("S.defCastList", auto sub () { return S::defCastList<int>(1); }),
        probe("S.defNew<string>", auto sub () { return S::defNew<string>("s"); }),
        probe("S.defOnCast", auto sub () { return S::defOnCast<int>(1); }),
        probe("S.defNested", auto sub () { return S::defNested<int>(1); }),
        probe("S.defTyped", auto sub () { return S::defTyped<int>(1); }),
        probe("S.closureCast", auto sub () { return S::closureCast<int>(1); }),
        probe("S.closureNew", auto sub () { return S::closureNew<int>(1); }),
        probe("N.ctor", auto sub () { return n.ctor<int>(1); }),
        probe("N.cast", auto sub () { return n.castForm<int>(1); }),
        probe("N.new", auto sub () { return n.newForm<int>(1); }),
        probe("N.assign", auto sub () { return n.assign<int>(1); }),
        probe("N.typedCtor", auto sub () { return n.typedCtor<int>(1); }),
        probe("N.typedCast", auto sub () { return n.typedCast<int>(1); }),
        probe("N.typedNew", auto sub () { return n.typedNew<int>(1); }),
        probe("N.listCast", auto sub () { return n.listCast<int>(1); }),
        probe("N.listHdCast", auto sub () { return n.listHdCast<int>(1); }),
        probe("N.listHdAssign", auto sub () { return n.listHdAssign<int>(1); }),
        probe("N.hashListCast", auto sub () { return n.hashListCast<int>(1); }),
        probe("N.hashListCtor", auto sub () { return n.hashListCtor<int>(1); }),
        probe("N.hashListAssign", auto sub () { return n.hashListAssign<int>(1); }),
        probe("N.hashHdCast", auto sub () { return n.hashHdCast<int>(1); }),
        probe("N.hdOfList", auto sub () { return n.hdOfList<int>(1); }),
        probe("f.ctor", auto sub () { return f_ctor<int>(1); }),
        probe("f.cast", auto sub () { return f_castForm<int>(1); }),
        probe("f.new", auto sub () { return f_newForm<int>(1); }),
        probe("f.assign", auto sub () { return f_assign<int>(1); }),
        probe("f.typedCtor", auto sub () { return f_typedCtor<int>(1); }),
        probe("f.typedCast", auto sub () { return f_typedCast<int>(1); }),
        probe("f.typedNew", auto sub () { return f_typedNew<int>(1); }),
        probe("f.listCast", auto sub () { return f_listCast<int>(1); }),
        probe("f.listHdCast", auto sub () { return f_listHdCast<int>(1); }),
        probe("f.listHdAssign", auto sub () { return f_listHdAssign<int>(1); }),
        probe("f.hashListCast", auto sub () { return f_hashListCast<int>(1); }),
        probe("f.hashListCtor", auto sub () { return f_hashListCtor<int>(1); }),
        probe("f.hashListAssign", auto sub () { return f_hashListAssign<int>(1); }),
        probe("f.hashHdCast", auto sub () { return f_hashHdCast<int>(1); }),
        probe("f.hdOfList", auto sub () { return f_hdOfList<int>(1); }),
        probe("G.ctor", auto sub () { return g.ctor<int>("x", 1); }),
        probe("G.cast", auto sub () { return g.castForm<int>("x", 1); }),
        probe("G.new", auto sub () { return g.newForm<int>("x", 1); }),
        probe("G.hashHdCast", auto sub () { return g.hashHdCast<int>("x", 1); }),
        probe("G.listHdCast", auto sub () { return g.listHdCast<int>("x", 1); }),
        probe("G.hashListAssign", auto sub () { return g.hashListAssign<int>("x", 1); }),
        probe("G.sctor", auto sub () { return G<string>::sctor<int>("x", 1); }),
        probe("G.defHd2", auto sub () { return g.defHd2<int>("x", 1); }),
        probe("GM<int>.h1", auto sub () { GM<int> o(); return o.h1; }),
        probe("GM<int>.h2", auto sub () { GM<int> o(); return o.h2; }),
        probe("GM<int>.h3", auto sub () { GM<int> o(); return o.h3; }),
        probe("GM<string>.h1", auto sub () { GM<string> o(); return o.h1; }),
        probe("GM<string>.h2", auto sub () { GM<string> o(); return o.h2; }),
        probe("GM<string>.h3", auto sub () { GM<string> o(); return o.h3; }),
        probe("GM<int>.h4", auto sub () { GM<int> o(); return o.h4; }),
        probe("GMI.h1", auto sub () { GMI o(); return o.h1; }),
        probe("MC.h1", auto sub () { MC o(); return o.h1; }),
        probe("MC.h2", auto sub () { MC o(); return o.h2; }),
        probe("MC.h3", auto sub () { MC o(); return o.h3; }),
        # two instantiations of a generic class in turn: the class's T is the receiver's
        probe("GT<int>.cast", auto sub () { return gi.castT(1); }),
        probe("GT<string>.cast", auto sub () { return gs.castT("s"); }),
        probe("GT<int>.new", auto sub () { return gi.newT(1); }),
        probe("GT<string>.new", auto sub () { return gs.newT("s"); }),
        probe("GT<int>.ctor", auto sub () { return gi.ctorT(1); }),
        probe("GT<string>.ctor", auto sub () { return gs.ctorT("s"); }),
        probe("GT<int>.list", auto sub () { return gi.listT(1); }),
        probe("GT<string>.list", auto sub () { return gs.listT("s"); }),
        probe("GT<int>.defNew", auto sub () { return gi.defNewT(); }),
        probe("GT<string>.defCast", auto sub () { return gs.defCastT(); }),
        probe("threads", auto sub () { return thread_probe(); }),
        probe("concrete.cast", auto sub () { return concrete_cast(1); }),
        probe("concrete.new", auto sub () { return concrete_new(1); }),
        probe("S.onListCast", auto sub () { return S::onListCast<int>(1); }),
        probe("S.onHashCast", auto sub () { return S::onHashCast<int>(1); }),
        probe("S.softlistCast", auto sub () { return S::softlistCast<int>(1); }),
    );
    return l;
}
