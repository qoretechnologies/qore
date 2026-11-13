# probes of default-call-type-args.qtest (qore#5964): a default value that calls a generic function, static method or
# method of a constant with explicit type arguments (also the type parameter of the callable whose default value it
# is), or a static method of a generic class instantiation, keeps them in a qcc executable; probes() returns one line
# per pass, so that the code runs on every tier with tiered execution

%modern

auto sub inner<T = int>(auto x = new list<T>("8")) { return x; }
auto sub nongen(auto v = inner<softint>()) { return v; }
auto sub nongen_s(auto v = inner<string>()) { return v; }
auto sub g_call<T = softint>(auto v = inner<T>()) { return v; }

class C {
    static auto m<T = int>(auto x = new list<T>("9")) { return x; }
}
auto sub sm(auto v = C::m<softint>()) { return v; }
auto sub sm_s(auto v = C::m<string>()) { return v; }
auto sub g_sm<T = softint>(auto v = C::m<T>()) { return v; }

class K {
    auto gm<T = int>(auto x = new list<T>("5")) { return x; }
}
const KObj = new K();
auto sub cm(auto v = KObj.gm<softint>()) { return v; }
auto sub cm_s(auto v = KObj.gm<string>()) { return v; }

class G<T = int> {
    static auto m(auto x = new list<T>("7")) { return x; }
}
auto sub gcls(auto v = G<softint>::m()) { return v; }
auto sub gcls_s(auto v = G<string>::m()) { return v; }

string sub probes() {
    string rv;
    for (int i = 0; i < 8; ++i) {
        rv += sprintf("%d: %y %y %y %y %y %y %y %y %y %y %y %y\n", i, nongen(), nongen_s(), g_call(),
            g_call<string>(), sm(), sm_s(), g_sm(), g_sm<string>(), cm(), cm_s(), gcls(), gcls_s());
    }
    return rv;
}
