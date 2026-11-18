# probes of generic-default-type-param.qtest (qore#5928): a default value of a generic function's or method's
# parameter that constructs a value of a type with the callable's type parameter (new list<T>(), new hash<string, T>())
# is read from a qcc executable with the type parameter of its callable, also with explicit type arguments and through
# call_function(); probes() returns one line per pass, so that the code runs on every tier with tiered execution

%modern

auto sub generic_default<T = softint>(auto value = new list<T>("23")) { return value; }
auto sub plain_default(auto value = new list<softint>("23")) { return value; }
auto sub hash_default<T = softint>(auto value = new hash<string, T>({"a": "4"})) { return value; }

class C {
    static auto m<T = softint>(auto value = new list<T>("6")) { return value; }
    auto im<T = softint>(auto value = new list<T>("7")) { return value; }
}

string sub probes() {
    C c();
    string rv;
    for (int i = 0; i < 8; ++i) {
        rv += sprintf("%d: %y %y %y %y %y %y %y %y\n", i, generic_default(), generic_default<string>(),
            plain_default(), hash_default(), C::m(), c.im(), call_function(\generic_default()),
            C::m<string>());
    }
    return rv;
}
