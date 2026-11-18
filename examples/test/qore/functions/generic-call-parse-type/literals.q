# -*- mode: qore; indent-tabs-mode: nil -*-
# probes of generic-call-parse-type.qtest (qore#5896): list, hash and typed hash literals and other expressions built
# from generic calls resolved at run time

%modern

X sub identity<X>(X x) { return x; }

list<X> sub wrap<X>(X x) { return (x,); }

X sub bounded<X: int>(X x) { return x; }

# bounds that accept values of other types than they return
X sub soft_bounded<X: softint>(X x) { return x; }

X sub float_bounded<X: float>(X x) { return x; }

hashdecl GenericCallParseTypeHash {
    int k;
}

# bounds whose child types have members of their own
hashdecl GenericCallParseTypeParentHash {
    int a = 1;
}

hashdecl GenericCallParseTypeChildHash inherits GenericCallParseTypeParentHash {
    int b = 2;
}

class GenericCallParseTypeParent {
    public {
        int x = 1;
    }
}

class GenericCallParseTypeChild inherits GenericCallParseTypeParent {
    public {
        int y = 2;
    }
}

X sub parent_hash_bounded<X: hash<GenericCallParseTypeParentHash>>(X x) { return x; }

X sub parent_class_bounded<X: GenericCallParseTypeParent>(X x) { return x; }

class GenericCallParseTypeUtil {
    public {}

    X mid<X>(X x) { return x; }

    static X smid<X>(X x) { return x; }
}

auto sub list_explicit(auto a) { return (identity<int>(a),)[0]; }
auto sub list_implicit(auto a) { return (identity(a),)[0]; }
auto sub hash_explicit(auto a) { return {"k": identity<int>(a)}.k; }
auto sub hash_implicit(auto a) { return {"k": identity(a)}.k; }
auto sub typed_hash(auto a) { return (<GenericCallParseTypeHash>{"k": identity<int>(a)}).k; }
auto sub list_container(auto a) { return (wrap<int>(a),)[0][0]; }
auto sub list_bounded(auto a) { return (bounded(a),)[0]; }
auto sub hash_bounded(auto a) { return {"k": bounded(a)}.k; }
auto sub soft_plus(auto a) { return soft_bounded(a) + 1; }
auto sub soft_hash(auto a) { return {"k": soft_bounded(a)}.k; }
auto sub float_plus(auto a) { return float_bounded(a) + 1; }
# the call has the type that an argument of type int gives it: integer division, as before with "int a"
auto sub float_div(auto a) { return float_bounded(a) / 2; }
# the type argument inferred at run time is the child type, whose members are accessible
auto sub child_hash_member(auto a) { return parent_hash_bounded(a).b; }
auto sub child_class_member(auto a) { return parent_class_bounded(a).y; }
auto sub list_method(auto a) { GenericCallParseTypeUtil u(); return (u.mid<int>(a),)[0]; }
auto sub hash_method(auto a) { GenericCallParseTypeUtil u(); return {"k": u.mid<int>(a)}.k; }
auto sub hash_static(auto a) { return {"k": GenericCallParseTypeUtil::smid<int>(a)}.k; }
auto sub hash_closure(auto a) { code c = auto sub (auto x) { return {"k": identity<int>(x)}.k; }; return c(a); }
auto sub map_implicit(auto a) { return map identity($1), a; }
auto sub map_explicit(auto a) { return map identity<int>($1), a; }
# the value is checked against the explicit type argument as before
auto sub list_mismatch(auto a) { return (identity<int>(a),)[0]; }

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
        generic_call_parse_type_probe(\list_explicit(), 1),
        generic_call_parse_type_probe(\list_implicit(), "s"),
        generic_call_parse_type_probe(\hash_explicit(), 1),
        generic_call_parse_type_probe(\hash_implicit(), 1),
        generic_call_parse_type_probe(\typed_hash(), 1),
        generic_call_parse_type_probe(\list_container(), 1),
        generic_call_parse_type_probe(\list_bounded(), 1),
        generic_call_parse_type_probe(\hash_bounded(), 1),
        generic_call_parse_type_probe(\soft_plus(), "12"),
        generic_call_parse_type_probe(\soft_hash(), "12"),
        generic_call_parse_type_probe(\float_plus(), 1),
        generic_call_parse_type_probe(\float_div(), 1),
        generic_call_parse_type_probe(\child_hash_member(), new hash<GenericCallParseTypeChildHash>()),
        generic_call_parse_type_probe(\child_class_member(), new GenericCallParseTypeChild()),
        generic_call_parse_type_probe(\list_method(), 1),
        generic_call_parse_type_probe(\hash_method(), 1),
        generic_call_parse_type_probe(\hash_static(), 1),
        generic_call_parse_type_probe(\hash_closure(), 1),
        generic_call_parse_type_probe(\map_implicit(), (1, 2)),
        generic_call_parse_type_probe(\map_explicit(), (1, 2)),
        generic_call_parse_type_probe(\list_mismatch(), "s"),
    );
    return foldl $1 + "|" + $2, l;
}
