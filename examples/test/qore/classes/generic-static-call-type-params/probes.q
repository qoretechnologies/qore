# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the probes of generic-static-call-type-params.qtest (qore#6186, qore#6285, qore#6308, qore#6256): the type
# parameters of the generic class of a static method called without type arguments of the class

%modern

#! a generic class with static methods
class G<T> {
    static T id(T v) { return v; }
    static string type() { return (new list<T>()).fullType(); }
    static list<T> wrap(T v) { return (v,); }
    # implicit and explicit calls in the class: the type parameters are those of the running instantiation
    static T implicitId(auto v) { return id(v); }
    static T explicitId(auto v) { return G::id(v); }
    static string implicitType() { return type(); }
    T instId(auto v) { return id(v); }
    # static generic methods with type parameters of their own, with and without a bound
    static string gen<X>(X v, T t) { return sprintf("%y %y %s", v, t, (new list<T>()).fullType()); }
    static string bnd<X: number>(X v, T t) { return sprintf("%y %y %s", v, t, (new list<X>()).fullType()); }
    # call references and dynamic calls made in the class
    static auto refId(auto v) { code c = \G::id(); return c(v); }
    static code makeRef() { return \G::id(); }
    static auto dynId(auto v) { return call_static_method("G", "id", v); }
}

#! the same generic class with a bound
class GB<T: number> {
    static T id(T v) { return v; }
    static string type() { return (new list<T>()).fullType(); }
}

#! a class that inherits a specialization
class H inherits G<float> {
    static auto viaClass(auto v) { return H::id(v); }
    static auto viaBase(auto v) { return G::id(v); }
    static auto implicit(auto v) { return id(v); }
    auto instViaBase(auto v) { return G::id(v); }
    auto instImplicit(auto v) { return id(v); }
    static auto refBase(auto v) { code c = \G::id(); return c(v); }
    static code makeBaseRef() { return \G::id(); }
}

class H2 inherits G<string> {}

#! a class that inherits a specialization and declares a static method of the same name: a reference to the method of
#! the generic class in its code calls that method
class H4 inherits G<string> {
    static string id(int i) { return "H4::id(int)"; }
    static auto refBase(auto v) { code c = \G::id(); return c(v); }
}

#! a dynamic call in a function called from the code of a class binds nothing of the class's
auto sub dyn_helper() { return call_static_method("G", "id", 1); }

#! overloaded static methods of a generic class called through a raw reference
class GO<T> {
    static string f(int a, T b) { return sprintf("v1 %s", (new list<T>()).fullType()); }
    static string f(string a, list<T> b) { return sprintf("v2 %s", (new list<T>()).fullType()); }
    static string dynHelper() { return dyn_helper().fullType(); }
}

#! subclass chains
class K<T> {
    static T id(T v) { return v; }
    static string type() { return (new list<T>()).fullType(); }
}
class HK inherits K<float> {}
class HH inherits HK {}

#! a generic class between the class and the generic class of the method
class KL<X> inherits G<list<X>> {
    static auto viaBase(auto v) { return G::id(v); }
}
class KLI inherits KL<int> {}

class HB inherits GB<float> {}

#! a non-generic control
class N {
    static int id(int v) { return v + 1; }
}
class NN inherits N {}

#! a generic class used by generic functions
class B<T> {
    static string sd() { return (new list<T>()).fullType(); }
    static string si(auto v) { return sprintf("%y", v instanceof list<T>); }
}
auto sub s1<U>() { return B<U>::sd(); }
auto sub s3<U>(auto v) { return B<U>::si(v); }
auto sub s4<U>() { code c = auto sub () { return B<U>::sd(); }; return c(); }
auto sub s5<U>() { return G<list<U>>::type(); }

#! a specialization of G compiled in a function that is never called (qore#6256)
string sub never_called() { return sprintf("%y", G<int>::id(2)); }

#! a generic class whose static method is called with one specialization in functions, in one that is never called,
#! and with another in the top-level code of the program (qore#6256)
class Q<T> {
    static T id(T v) { return v; }
}
class HQ inherits Q<string> {}
string sub never_called_q() { return sprintf("%y", Q<float>::id(2)); }

#! returns the value that a background thread pushed
auto sub bg_result(Queue q) {
    return q.get(30s);
}

#! one of the threads calling the static methods of different specializations at the same time
sub thread_worker(int t, Queue q) {
    string err;
    try {
        for (int i = 0; i < 200 && !err; ++i) {
            string got;
            string want;
            if (t % 2) {
                got = sprintf("%y %y %s", H::id(i), HH::id(i), s1<string>());
                want = sprintf("%y %y list<string>", float(i), float(i));
            } else {
                got = sprintf("%y %y %s", KLI::id((i,)), H2::id("s"), s1<int>());
                want = sprintf("%y \"s\" list<int>", (i,));
            }
            if (got != want) {
                err = sprintf("thread %d: %s", t, got);
            }
        }
    } catch (hash<ExceptionInfo> ex) {
        err = sprintf("thread %d: %s: %s", t, ex.err, ex.desc);
    }
    q.push(err ?? "");
}

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

string sub probe(string label, code c) {
    try {
        auto v = c();
        return sprintf("%s: %y %s", label, v, v.fullType());
    } catch (hash<ExceptionInfo> ex) {
        return sprintf("%s: %s", label, ex.err);
    }
}

#! runs the probes
list<string> sub probes() {
    auto one = 1;
    list<string> l = (
        # qore#6186: through a subclass of a specialization, as through the specialization
        probe("G<float>::id", auto sub () { return G<float>::id(1); }),
        probe("H::id", auto sub () { return H::id(1); }),
        probe("H::wrap", auto sub () { return H::wrap(1); }),
        probe("H::type", auto sub () { return H::type(); }),
        probe("H2::id", auto sub () { return H2::id(one); }),
        probe("G<string>::id", auto sub () { return G<string>::id(one); }),
        probe("H.viaClass", auto sub () { return H::viaClass(1); }),
        probe("H.viaBase", auto sub () { return H::viaBase(1); }),
        probe("H.implicit", auto sub () { return H::implicit(1); }),
        probe("H.instViaBase", auto sub () { return (new H()).instViaBase(1); }),
        probe("H.instImplicit", auto sub () { return (new H()).instImplicit(1); }),
        probe("H.implicitId", auto sub () { return H::implicitId(1); }),
        probe("H.explicitId", auto sub () { return H::explicitId(1); }),
        probe("H.implicitType", auto sub () { return H::implicitType(); }),
        probe("H.instId", auto sub () { return (new H()).instId(1); }),
        # in the generic class, through a specialization
        probe("G<float>.implicitId", auto sub () { return G<float>::implicitId(1); }),
        probe("G<float>.explicitId", auto sub () { return G<float>::explicitId(1); }),
        probe("G<int>.implicitType", auto sub () { return G<int>::implicitType(); }),
        probe("G<float>.instId", auto sub () { return (new G<float>()).instId(1); }),
        # static generic methods with type parameters of their own
        probe("H::gen", auto sub () { return H::gen("a", 1); }),
        probe("G<float>::gen", auto sub () { return G<float>::gen("a", 1); }),
        probe("H::gen<string>", auto sub () { return H::gen<string>("a", 1); }),
        probe("H::bnd", auto sub () { return H::bnd(2, 1); }),
        probe("ref H::gen", auto sub () { code c = \H::gen(); return c("a", 1); }),
        # subclass chains and a generic class between
        probe("HK::id", auto sub () { return HK::id(1); }),
        probe("HH::id", auto sub () { return HH::id(1); }),
        probe("HH::type", auto sub () { return HH::type(); }),
        probe("KL<int>::id", auto sub () { return KL<int>::id((1,)); }),
        probe("KLI::id", auto sub () { return KLI::id((1,)); }),
        probe("KLI::type", auto sub () { return KLI::type(); }),
        probe("KLI.viaBase", auto sub () { return KLI::viaBase((1,)); }),
        probe("KL<string>.viaBase", auto sub () { return KL<string>::viaBase(("a",)); }),
        # a bound
        probe("HB::id", auto sub () { return HB::id(1); }),
        probe("HB::type", auto sub () { return HB::type(); }),
        # qore#6285: call references and dynamic calls
        probe("ref H::id", auto sub () { code c = \H::id(); return c(1); }),
        probe("ref HH::id", auto sub () { code c = \HH::id(); return c(1); }),
        probe("ref G::id", auto sub () { code c = \G::id(); return c(1); }),
        probe("ref G::wrap", auto sub () { code c = \G::wrap(); return c(1); }),
        probe("ref GB::id", auto sub () { code c = \GB::id(); return c(1); }),
        probe("call_static_method H", auto sub () { return call_static_method("H", "id", 1); }),
        probe("call_static_method KLI", auto sub () { return call_static_method("KLI", "id", (1,)); }),
        probe("call_static_method G", auto sub () { return call_static_method("G", "id", 1); }),
        probe("call_static_method_args H", auto sub () { return call_static_method_args("H", "id", (1,)); }),
        probe("get_call_reference H", auto sub () { code c = get_call_reference("H::id"); return c(1); }),
        probe("G<float>.refId", auto sub () { return G<float>::refId(1); }),
        probe("G<float>.makeRef", auto sub () { code c = G<float>::makeRef(); return c(1); }),
        probe("G<float>.dynId", auto sub () { return G<float>::dynId(1); }),
        probe("H.refBase", auto sub () { return H::refBase(1); }),
        probe("H.makeBaseRef", auto sub () { code c = H::makeBaseRef(); return c(1); }),
        probe("H4.refBase", auto sub () { return H4::refBase("s"); }),
        probe("dyn_helper", auto sub () { return dyn_helper(); }),
        probe("GO<string>.dynHelper", auto sub () { return GO<string>::dynHelper(); }),
        probe("GO::f", auto sub () { return GO::f("x", cast<list<int>>((1,))); }),
        probe("ref GO::f v1", auto sub () { code c = \GO::f(); return c(1, 2.5); }),
        probe("ref GO::f v2", auto sub () { code c = \GO::f(); return c("x", cast<list<int>>((1,))); }),
        probe("call_static_method GO", auto sub () { return call_static_method("GO", "f", "x", ("a",)); }),
        # qore#6308: a specialization with the type parameters of a generic function
        probe("s1<int>", auto sub () { return s1<int>(); }),
        probe("s3<string>", auto sub () { return s3<string>(cast<list<int>>((1,))); }),
        probe("s3<int>", auto sub () { return s3<int>(cast<list<int>>((1,))); }),
        probe("s4<int>", auto sub () { return s4<int>(); }),
        probe("s5<string>", auto sub () { return s5<string>(); }),
        probe("B<float>::sd", auto sub () { return B<float>::sd(); }),
        # background calls and threads
        probe("background H::id", auto sub () { Queue q(); background q.push(H::id(1)); return bg_result(q); }),
        probe("background s1<int>", auto sub () { Queue q(); background q.push(s1<int>()); return bg_result(q); }),
        probe("threads", \thread_probe()),
        # non-generic controls
        probe("NN::id", auto sub () { return NN::id(1); }),
        probe("ref NN::id", auto sub () { code c = \NN::id(); return c(1); }),
        probe("call_static_method NN", auto sub () { return call_static_method("NN", "id", 1); }),
    );
    return l;
}
