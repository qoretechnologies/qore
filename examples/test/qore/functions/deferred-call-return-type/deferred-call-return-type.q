%modern
# a call whose variant is resolved at run time only because another variant could match an argument without a value
# has the return type of either variant: sort(keys h) is *list<auto> (list<auto> or nothing), not untyped (issue
# #5686); see deferred-call-return-type.qtest

# the Qorus shape: a map into a typed hash
hash<string, list<string>> sub sorted_keys(hash<string, hash<string, bool>> values) {
    return map {$1.key: sort(keys $1.value)}, values.pairIterator();
}
hash<string, list<string>> sub sorted_keys_desc(hash<string, hash<string, bool>> values) {
    return map {$1.key: sort_descending(keys $1.value)}, values.pairIterator();
}
hash<string, list<string>> sub sorted_keys_stable(hash<string, hash<string, bool>> values) {
    return map {$1.key: sort_stable(keys $1.value)}, values.pairIterator();
}
hash<string, list<string>> sub sorted_keys_desc_stable(hash<string, hash<string, bool>> values) {
    return map {$1.key: sort_descending_stable(keys $1.value)}, values.pairIterator();
}
# a generic function: the call has the type of its instantiation
list<T> sub gen<T>(list<T> l) { return l; }
nothing sub gen(nothing n) {}
hash<string, list<string>> sub generic_keys(hash<string, hash<string, bool>> values) {
    return map {$1.key: gen(keys $1.value)}, values.pairIterator();
}
# no value at run time: the variant for no value is resolved
*list<string> sub sorted_or_nothing(auto v) {
    return sort(keys v);
}

hash<string, hash<string, bool>> values = {"a": {"y": True, "x": True, "z": True}};
printf("sort: %y\n", sorted_keys(values));
printf("sort_descending: %y\n", sorted_keys_desc(values));
printf("sort_stable: %y\n", sorted_keys_stable(values));
printf("sort_descending_stable: %y\n", sorted_keys_desc_stable(values));
printf("no value: %y\n", sorted_or_nothing(NOTHING));
printf("value: %y\n", sorted_or_nothing({"b": 1, "a": 2}));
printf("generic: %y\n", generic_keys(values));

# the parse-time type of the call: a bool parameter refuses any typed value, and the parse error names the type
string sub parse_type(string decls, string expr) {
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
    try {
        p.parse(sprintf("%s\nbool sub tb(bool b) { return b; }\nsub f(hash<string, bool> h, hash<string, int> hi) "
            "{ tb(%s); }\n", decls, expr), "probe");
    } catch (hash<ExceptionInfo> ex) {
        *list<auto> m = (ex.desc =~ x/call supplies ([^ ]+)/);
        return m ? m[0] : ex.err;
    }
    return "untyped";
}
const Probes = (
    ("", "sort(keys h)"),
    ("", "sort_descending(keys h)"),
    ("", "sort_stable(keys h)"),
    ("", "sort_descending_stable(keys h)"),
    # a generic function and a method of a generic class: the instantiation of the call
    ("list<T> sub g<T>(list<T> l) { return l; }\nnothing sub g(nothing n) {}", "g(keys h)"),
    ("class C<T> { list<T> m(list<T> l) { return l; } nothing m(nothing n) {} }",
        "(new C<string>()).m(keys h)"),
    # user functions, methods, static methods, named calls and variable arguments
    ("int sub u(int i) { return i; }\nnothing sub u(nothing n) {}", "u(hi.a + 1)"),
    ("class B { int m(int i) { return i; } nothing m(nothing n) {} }", "(new B()).m(hi.a + 1)"),
    ("class S { static int s(int i) { return i; } static nothing s(nothing n) {} }", "S::s(hi.a + 1)"),
    ("int sub nm(int i) { return i; }\nnothing sub nm(nothing i) {}", "nm(i: hi.a + 1)"),
    ("int sub va(int i, ...) { return i; }\nnothing sub va(nothing i) {}", "va(hi.a + 1)"),
    # the same type, and types that do not have a common type
    ("int sub eq(int i) { return i; }\nint sub eq(nothing n) { return 0; }", "eq(hi.a + 1)"),
    ("", "pow(h.a ? 1 : 2.0, 2)"),
);
foreach list<string> probe in (Probes) {
    printf("type of %s: %s\n", probe[1], parse_type(probe[0], probe[1]));
}
