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

# the parse-time type of the call: a wrong use is a parse error naming it
string sub parse_type(string expr) {
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
    try {
        p.parse(sprintf("int sub ti(int i) { return i; }\nsub f(hash<string, bool> h) { ti(%s); }\n", expr), "probe");
    } catch (hash<ExceptionInfo> ex) {
        *list<auto> m = (ex.desc =~ x/call supplies ([^ ]+)/);
        return m ? m[0] : ex.err;
    }
    return "untyped";
}
foreach string expr in (("sort(keys h)", "sort_descending(keys h)", "sort_stable(keys h)",
        "sort_descending_stable(keys h)", "pow(h.a ? 1 : 2.0, 2)")) {
    printf("type of %s: %s\n", expr, parse_type(expr));
}
