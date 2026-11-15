# -*- mode: qore; indent-tabs-mode: nil -*-
# probes of generic-call-parse-type.qtest (qore#6151): nested calls with explicit type arguments, and other consumers
# of the type of a generic call resolved at run time

%modern

X sub identity<X>(X x) { return x; }

int sub takes_int(int i) { return i; }

class GenericCallParseTypeUtil {
    public {}

    X mid<X>(X x) { return x; }

    static X smid<X>(X x) { return x; }
}

auto sub nested_func(auto a) { return identity<int>(identity<int>(a)); }
auto sub nested_method(auto a) { GenericCallParseTypeUtil u(); return u.mid<int>(u.mid<int>(a)); }
auto sub nested_static(auto a) {
    return GenericCallParseTypeUtil::smid<int>(GenericCallParseTypeUtil::smid<int>(a));
}
auto sub nested_named(auto a) { return identity<int>(x: identity<int>(x: a)); }
auto sub nested_implicit(auto a) { return (identity(identity(a)),)[0]; }
auto sub as_arg(auto a) { return takes_int(identity<int>(a)); }
int sub as_return(auto a) { return identity<int>(a); }
auto sub as_local(auto a) { int i = identity<int>(a); return i; }

string sub generic_call_parse_type_probe(code c, auto v) {
    try {
        auto rv = c(v);
        return sprintf("%y %s", rv, rv.fullType());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub probes() {
    list<string> l = (
        generic_call_parse_type_probe(\nested_func(), 1),
        generic_call_parse_type_probe(\nested_method(), 1),
        generic_call_parse_type_probe(\nested_static(), 1),
        generic_call_parse_type_probe(\nested_named(), 1),
        generic_call_parse_type_probe(\nested_implicit(), "s"),
        generic_call_parse_type_probe(\as_arg(), 1),
        generic_call_parse_type_probe(\as_return(), 1),
        generic_call_parse_type_probe(\as_local(), 1),
    );
    return foldl $1 + "|" + $2, l;
}
