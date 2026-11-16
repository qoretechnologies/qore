# -*- mode: qore; indent-tabs-mode: nil -*-
# probes of generic-call-parse-type.qtest (qore#5896, qore#6151): generic calls resolved at run time made in generic
# code, whose explicit type arguments are the type parameters of the caller

%modern

X sub identity<X>(X x) { return x; }

list<X> sub wrap<X>(X x) { return (x,); }

# the explicit type argument is a type parameter of the calling generic function
X sub outer<X>(auto a) { return (identity<X>(a),)[0]; }
X sub outer_nested<X>(auto a) { return identity<X>(identity<X>(a)); }
auto sub outer_hash<X>(auto a) { return {"k": identity<X>(a)}; }
auto sub outer_wrap<X>(auto a) { return (wrap<X>(a),); }
auto sub outer_map<X>(auto a) { return map identity<X>($1), a; }

class GenericCallParseTypeBox<T> {
    public {}

    # the explicit type argument is the type parameter of the calling class
    auto list_of(auto a) { return (identity<T>(a),)[0]; }
    auto hash_of(auto a) { return {"k": identity<T>(a)}.k; }
    auto nested(auto a) { return identity<T>(identity<T>(a)); }
    auto own(auto a) { return (ident<int>(a),)[0]; }
    X ident<X>(X x) { return x; }
}

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
        generic_call_parse_type_probe(auto sub (auto a) { return outer<int>(a); }, 1),
        generic_call_parse_type_probe(auto sub (auto a) { return outer(a); }, 1),
        generic_call_parse_type_probe(auto sub (auto a) { return outer_nested<int>(a); }, 1),
        generic_call_parse_type_probe(auto sub (auto a) { return outer_hash<int>(a); }, 1),
        generic_call_parse_type_probe(auto sub (auto a) { return outer_wrap<int>(a); }, 1),
        generic_call_parse_type_probe(auto sub (auto a) { return outer_map<int>(a); }, (1, 2)),
        generic_call_parse_type_probe(auto sub (auto a) { GenericCallParseTypeBox<int> b(); return b.list_of(a); }, 1),
        generic_call_parse_type_probe(auto sub (auto a) { GenericCallParseTypeBox<int> b(); return b.hash_of(a); }, 1),
        generic_call_parse_type_probe(auto sub (auto a) { GenericCallParseTypeBox<int> b(); return b.nested(a); }, 1),
        generic_call_parse_type_probe(auto sub (auto a) { GenericCallParseTypeBox<string> b(); return b.own(a); }, 1),
    );
    return foldl $1 + "|" + $2, l;
}
