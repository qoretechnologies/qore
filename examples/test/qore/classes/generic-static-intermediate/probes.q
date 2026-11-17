# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the probes of generic-static-intermediate.qtest (qore#6447, qore#6448): the specialization of a generic class that a
# class inherits through intermediate classes, and the static methods of a generic class called through a class that
# declares a method of the same name

%modern

#! a generic class with static and instance methods
class G<T> {
    static T id(T v) { return v; }
    static string type() { return (new list<T>()).fullType(); }
    T instId(auto v) { return id(v); }
}

#! a specialization
class H inherits G<float> {}

#! a generic intermediate class and subclass chains below its specialization
class K<U> inherits G<U> {}
class HK inherits K<float> {}
class HKK inherits HK {
    static auto implicit(auto v) { return id(v); }
    static auto viaBase(auto v) { return G::id(v); }
    static auto refImplicit(auto v) { code c = \id(); return c(v); }
}
class HKKK inherits HKK {}

#! the same with a generic intermediate class that defaults to a value-erased instance for raw use
class KR<U> [legacy_raw_construct] inherits G<U> {}
class HKR inherits KR<float> {}
class HKRR inherits HKR {}

#! a generic class over a class that is not generic
class W<V> inherits H {
    static auto implicit(auto v) { return id(v); }
    static auto viaBase(auto v) { return G::id(v); }
}

#! a generic intermediate class over a class that is not generic, and a generic class over it
class K2<U> inherits H {}
class W2<V> inherits K2<V> {}
class HW2 inherits W2<string> {}

#! multiple inheritance: the specialization comes through the second parent class, and through both sides of a
#! diamond
class Other {}
class MI inherits Other, HK {}
class D1 inherits HK {}
class D2 inherits HK {}
class DD inherits D1, D2 {}

#! a raw-construct generic class inherited without type arguments: its value-erased instance
class HR inherits KR {}
class HRR inherits HR {}

#! three generic levels and a class below them
class L1<P> inherits G<list<P>> {}
class L2<Q> inherits L1<hash<string, Q>> {}
class L3 inherits L2<int> {}
class L33 inherits L3 {}

#! explicit and default type arguments of the classes between
class GD<A, B = int> {
    static string ab(A a, B b) { return sprintf("%y %y", a, b); }
    static B idb(B b) { return b; }
}
class KD<X> inherits GD<X> {}
class HD inherits KD<string> {}
class HDD inherits HD {}
class KD2<X, Y = float> inherits GD<Y, X> {}
class HD2 inherits KD2<string> {}
class HD22 inherits HD2 {}

#! qore#6448: classes that declare a static method of the same name as the generic class's
class HX inherits G<float> {
    static string id(string s) { return "own " + s; }
    static string type(string s) { return "own type " + s; }
    static auto implicit(auto v) { return id(v); }
}
class HXX inherits HX {
    static auto implicit2(auto v) { return id(v); }
    static auto refImplicit(auto v) { code c = \id(); return c(v); }
}
class W3<V> inherits HX {
    static auto implicit(auto v) { return id(v); }
}
#! a generic class over it declaring a variant of its own with its type parameter
class W4<V> inherits HX {
    static string id(V v, int i) { return sprintf("W4 %y %d", v, i); }
}
#! a variant with the signature that the generic class's has with its type argument: the class's own is selected
class HY inherits G<float> {
    static string id(float f) { return sprintf("own float %y", f); }
}
class HYY inherits HY {}
#! a generic class adding a variant, and a class below its specialization
class KX<V> inherits G<V> {
    static string id(string s, V v) { return sprintf("KX %s %y", s, v); }
}
class HKX inherits KX<float> {}
#! the variant of a class between the generic class and one adding a variant
class HZ inherits HK {
    static string id(string s) { return "HZ " + s; }
}

string sub take_gf(G<float> g) { return "G<float> accepted"; }

#! runs the call in a background thread and returns its result, or the error that it raised
auto sub bg_call(code c) {
    Queue q();
    background sub () {
        try {
            q.push({"v": c()});
        } catch (hash<ExceptionInfo> ex) {
            q.push({"err": ex.err});
        }
    }();
    hash<auto> h = q.get(30s);
    if (h.err) {
        throw h.err;
    }
    return h.v;
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
    auto w2 = new W2<string>();
    auto hkk = new HKK();
    auto hkr = new HKRR();
    auto l33 = new L33();
    list<string> l = (
        # qore#6447: through intermediate classes
        probe("HK::id", auto sub () { return HK::id(1); }),
        probe("HKK::id", auto sub () { return HKK::id(1); }),
        probe("HKKK::id", auto sub () { return HKKK::id(1); }),
        probe("HKKK::type", auto sub () { return HKKK::type(); }),
        probe("HKK.implicit", auto sub () { return HKK::implicit(1); }),
        probe("HKK.viaBase", auto sub () { return HKK::viaBase(1); }),
        probe("HKK.refImplicit", auto sub () { return HKK::refImplicit(1); }),
        probe("HKK.instId", auto sub () { return hkk.instId(1); }),
        probe("HKRR::id", auto sub () { return HKRR::id(1); }),
        probe("HKRR.instId", auto sub () { return hkr.instId(1); }),
        probe("W<string>::id", auto sub () { return W<string>::id(1); }),
        probe("W<string>.implicit", auto sub () { return W<string>::implicit(1); }),
        probe("W<string>.viaBase", auto sub () { return W<string>::viaBase(1); }),
        probe("W2<string>::id", auto sub () { return W2<string>::id(1); }),
        probe("W2<string>::type", auto sub () { return W2<string>::type(); }),
        probe("HW2::id", auto sub () { return HW2::id(1); }),
        probe("MI::id", auto sub () { return MI::id(1); }),
        probe("MI instanceof G<float>", auto sub () { return new MI() instanceof G<float>; }),
        probe("DD::id", auto sub () { return DD::id(1); }),
        probe("DD::type", auto sub () { return DD::type(); }),
        probe("DD instanceof G<float>", auto sub () { return new DD() instanceof G<float>; }),
        probe("take_gf DD", auto sub () { return take_gf(new DD()); }),
        probe("HRR::id", auto sub () { return HRR::id(1); }),
        probe("HRR::type", auto sub () { return HRR::type(); }),
        probe("L3::id", auto sub () { return L3::id(({"a": 1},)); }),
        probe("L33::type", auto sub () { return L33::type(); }),
        probe("L33.instId", auto sub () { return l33.instId(({"a": 1},)); }),
        probe("HDD::ab", auto sub () { return HDD::ab("a", 2); }),
        probe("HDD::idb", auto sub () { return HDD::idb(2); }),
        probe("HD22::ab", auto sub () { return HD22::ab(1, "s"); }),
        probe("HD22::idb", auto sub () { return HD22::idb("s"); }),
        # type checks
        probe("W2 instanceof G<float>", auto sub () { return w2 instanceof G<float>; }),
        probe("W2 instanceof G<int>", auto sub () { return w2 instanceof G<int>; }),
        probe("new W2 instanceof G<float>", auto sub () { return new W2<string>() instanceof G<float>; }),
        probe("HKK instanceof G<float>", auto sub () { return hkk instanceof G<float>; }),
        probe("HKK instanceof G<int>", auto sub () { return hkk instanceof G<int>; }),
        probe("HKRR instanceof G<float>", auto sub () { return hkr instanceof G<float>; }),
        probe("L33 instanceof G<list<hash<string, int>>>", auto sub () {
            return l33 instanceof G<list<hash<string, int>>>;
        }),
        probe("L33 instanceof G<list<hash<string, float>>>", auto sub () {
            return l33 instanceof G<list<hash<string, float>>>;
        }),
        probe("take_gf W2", auto sub () { return take_gf(w2); }),
        probe("take_gf new W2", auto sub () { return take_gf(new W2<string>()); }),
        probe("take_gf HKK", auto sub () { return take_gf(hkk); }),
        probe("take_gf new HKKK", auto sub () { return take_gf(new HKKK()); }),
        probe("G<float> var W2", auto sub () { G<float> g = w2; return g.instId(1); }),
        probe("cast<G<float>> HKK", auto sub () { return cast<G<float>>(hkk).instId(1); }),
        # call references and dynamic calls
        probe("ref HKK::id", auto sub () { code c = \HKK::id(); return c(1); }),
        probe("ref HKRR::id", auto sub () { code c = \HKRR::id(); return c(1); }),
        probe("ref L33::id", auto sub () { code c = \L33::id(); return c(({"a": 1},)); }),
        probe("call_static_method HKK", auto sub () { return call_static_method("HKK", "id", 1); }),
        probe("call_static_method HD22", auto sub () { return call_static_method("HD22", "idb", "s"); }),
        # qore#6448: a class declaring a static method of the same name as the generic class's
        probe("HX::id", auto sub () { return HX::id(1); }),
        probe("HX::id own", auto sub () { return HX::id("x"); }),
        probe("HX::type", auto sub () { return HX::type(); }),
        probe("HX::type own", auto sub () { return HX::type("x"); }),
        probe("HX.instId", auto sub () { return (new HX()).instId(1); }),
        probe("ref HX::type", auto sub () { code c = \HX::type(); return c(); }),
        probe("HX.implicit", auto sub () { return HX::implicit(1); }),
        probe("HXX::id", auto sub () { return HXX::id(1); }),
        probe("HXX.implicit2", auto sub () { return HXX::implicit2(1); }),
        probe("HXX.implicit2 own", auto sub () { return HXX::implicit2("x"); }),
        probe("HXX.refImplicit", auto sub () { return HXX::refImplicit(1); }),
        probe("W3<string>.implicit", auto sub () { return W3<string>::implicit(1); }),
        probe("W3<string>::id", auto sub () { return W3<string>::id(1); }),
        probe("W4<string>::id", auto sub () { return W4<string>::id(1); }),
        probe("W4<string>::id own", auto sub () { return W4<string>::id("a", 2); }),
        probe("W4<string>::id HX", auto sub () { return W4<string>::id("a"); }),
        probe("HY::id", auto sub () { return HY::id(1.5); }),
        probe("HYY::id", auto sub () { return HYY::id(1.5); }),
        probe("HKX::id", auto sub () { return HKX::id(1); }),
        probe("HKX::id own", auto sub () { return HKX::id("a", 1); }),
        probe("HZ::id", auto sub () { return HZ::id(1); }),
        probe("HZ::id own", auto sub () { return HZ::id("a"); }),
        probe("ref HX::id", auto sub () { code c = \HX::id(); return c(1); }),
        probe("ref HX::id own", auto sub () { code c = \HX::id(); return c("x"); }),
        probe("ref HXX::id", auto sub () { code c = \HXX::id(); return c(1); }),
        probe("ref HZ::id", auto sub () { code c = \HZ::id(); return c(1); }),
        probe("call_static_method HX", auto sub () { return call_static_method("HX", "id", 1); }),
        probe("call_static_method HXX", auto sub () { return call_static_method("HXX", "id", 1); }),
        probe("call_static_method_args HZ", auto sub () { return call_static_method_args("HZ", "id", (1,)); }),
        probe("get_call_reference HX", auto sub () { code c = get_call_reference("HX::id"); return c(1); }),
        probe("background HX::id", auto sub () { return bg_call(auto sub () { return HX::id(1); }); }),
        probe("background HKK::id", auto sub () { return bg_call(auto sub () { return HKK::id(1); }); }),
    );
    return l;
}
