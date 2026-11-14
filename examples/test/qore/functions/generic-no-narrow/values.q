# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern

# Each generic local is paired with its written-out concrete declaration.
auto sub list_init<T>() { *T value = (1,); value += ("s",); return value; }
auto sub concrete_list_init() { *list<auto!> value = (1,); value += ("s",); return value; }
auto sub list_plain<T>() { T value = (1,); value += ("s",); return value; }
auto sub concrete_list_plain() { list<auto!> value = (1,); value += ("s",); return value; }
auto sub list_deferred<T>() { *T value; if (value !== NOTHING) throw "ASSIGNED"; value = (1,); value += ("s",); return value; }
auto sub concrete_list_deferred() { *list<auto!> value; if (value !== NOTHING) throw "ASSIGNED"; value = (1,); value += ("s",); return value; }
auto sub list_reassign<T>() { *T value = (1,); value = ("s",); value += (2,); return value; }
auto sub concrete_list_reassign() { *list<auto!> value = (1,); value = ("s",); value += (2,); return value; }
auto sub list_copy<T>() { list<int> source = (1,); *T value = source; value += ("s",); return (source.fullType(), source, value); }
auto sub concrete_list_copy() { list<int> source = (1,); *list<auto!> value = source; value += ("s",); return (source.fullType(), source, value); }
auto sub hash_init<T>() { *T value = {"i": 1}; value{"s"} = "s"; return value; }
auto sub concrete_hash_init() { *hash<auto!> value = {"i": 1}; value{"s"} = "s"; return value; }
auto sub hash_plain<T>() { T value = {"i": 1}; value{"s"} = "s"; return value; }
auto sub concrete_hash_plain() { hash<auto!> value = {"i": 1}; value{"s"} = "s"; return value; }
auto sub hash_deferred<T>() { *T value; if (value !== NOTHING) throw "ASSIGNED"; value = {"i": 1}; value{"s"} = "s"; return value; }
auto sub concrete_hash_deferred() { *hash<auto!> value; if (value !== NOTHING) throw "ASSIGNED"; value = {"i": 1}; value{"s"} = "s"; return value; }
auto sub hash_reassign<T>() { *T value = {"i": 1}; value = {"s": "s"}; value{"i"} = 2; return value; }
auto sub concrete_hash_reassign() { *hash<auto!> value = {"i": 1}; value = {"s": "s"}; value{"i"} = 2; return value; }
auto sub hash_copy<T>() { hash<string, int> source = {"i": 1}; *T value = source; value{"s"} = "s"; return (source.fullType(), source, value); }
auto sub concrete_hash_copy() { hash<string, int> source = {"i": 1}; *hash<auto!> value = source; value{"s"} = "s"; return (source.fullType(), source, value); }
auto sub list_nested<T>() { *T value = ((1,),); value += (("s",),); return value; }
auto sub concrete_list_nested() { *list<list<auto!>> value = ((1,),); value += (("s",),); return value; }
auto sub hash_nested<T>() { *T value = {"first": {"i": 1}}; value{"second"} = {"s": "s"}; return value; }
auto sub concrete_hash_nested() { *hash<string, hash<auto!>> value = {"first": {"i": 1}}; value{"second"} = {"s": "s"}; return value; }
# No-narrow coercion does not recursively widen existing inner values. These pairs must still reject the write.
auto sub list_nested_element<T>() { *T value = ((1,),); value[0] += ("s",); return value; }
auto sub concrete_list_nested_element() { *list<list<auto!>> value = ((1,),); value[0] += ("s",); return value; }
auto sub hash_nested_element<T>() { *T value = {"first": {"i": 1}}; value{"first"}{"s"} = "s"; return value; }
auto sub concrete_hash_nested_element() { *hash<string, hash<auto!>> value = {"first": {"i": 1}}; value{"first"}{"s"} = "s"; return value; }

auto sub nullable<T>() { *T value = NULL; return value; }
auto sub nested_call<T>() { return list_init<T>(); }
class Box<T> {
    auto listValue() { *T value = (1,); value += ("s",); return value; }
    auto hashValue() { *T value = {"i": 1}; value{"s"} = "s"; return value; }
    auto methodValue<U>() { *U value = (1,); value += ("s",); return value; }
}
class Derived<T> inherits Box<T> {}

sub show(string name, code<auto()> callback) {
    try {
        printf("%s: %y\n", name, callback());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", name, ex.err);
    }
}

show("list_init", auto sub () { return list_init<list<auto!>>(); });
show("concrete_list_init", \concrete_list_init());
show("list_plain", auto sub () { return list_plain<list<auto!>>(); });
show("concrete_list_plain", \concrete_list_plain());
show("list_deferred", auto sub () { return list_deferred<list<auto!>>(); });
show("concrete_list_deferred", \concrete_list_deferred());
show("list_reassign", auto sub () { return list_reassign<list<auto!>>(); });
show("concrete_list_reassign", \concrete_list_reassign());
show("list_copy", auto sub () { return list_copy<list<auto!>>(); });
show("concrete_list_copy", \concrete_list_copy());
show("hash_init", auto sub () { return hash_init<hash<auto!>>(); });
show("concrete_hash_init", \concrete_hash_init());
show("hash_plain", auto sub () { return hash_plain<hash<auto!>>(); });
show("concrete_hash_plain", \concrete_hash_plain());
show("hash_deferred", auto sub () { return hash_deferred<hash<auto!>>(); });
show("concrete_hash_deferred", \concrete_hash_deferred());
show("hash_reassign", auto sub () { return hash_reassign<hash<auto!>>(); });
show("concrete_hash_reassign", \concrete_hash_reassign());
show("hash_copy", auto sub () { return hash_copy<hash<auto!>>(); });
show("concrete_hash_copy", \concrete_hash_copy());
show("list_nested", auto sub () { return list_nested<list<list<auto!>>>(); });
show("concrete_list_nested", \concrete_list_nested());
show("hash_nested", auto sub () { return hash_nested<hash<string, hash<auto!>>>(); });
show("concrete_hash_nested", \concrete_hash_nested());
show("list_nested_element", auto sub () { return list_nested_element<list<list<auto!>>>(); });
show("concrete_list_nested_element", \concrete_list_nested_element());
show("hash_nested_element", auto sub () { return hash_nested_element<hash<string, hash<auto!>>>(); });
show("concrete_hash_nested_element", \concrete_hash_nested_element());
show("nullable_list", auto sub () { return nullable<list<auto!>>(); });
show("nullable_hash", auto sub () { return nullable<hash<auto!>>(); });
show("already_nullable_list", auto sub () { return list_init<*list<auto!>>(); });
show("already_nullable_hash", auto sub () { return hash_init<*hash<auto!>>(); });
show("nested_call", auto sub () { return nested_call<list<auto!>>(); });
show("box_list", auto sub () { Box<list<auto!>> b(); return b.listValue(); });
show("box_hash", auto sub () { Box<hash<auto!>> b(); return b.hashValue(); });
show("derived_list", auto sub () { Derived<list<auto!>> b(); return b.listValue(); });
show("derived_hash", auto sub () { Derived<hash<auto!>> b(); return b.hashValue(); });
show("method_type", auto sub () { Box<string> b(); return b.methodValue<list<auto!>>(); });
show("narrow_list_control", auto sub () { return list_init<list<auto>>(); });
show("narrow_hash_control", auto sub () { return hash_init<hash<auto>>(); });
