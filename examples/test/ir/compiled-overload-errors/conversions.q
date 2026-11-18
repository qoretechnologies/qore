%modern
# string(), int() and float() of values that the builtin conversion functions do not accept (issue #5660); see
# compiled-overload-errors.qtest
class O { }
auto sub v(string t) {
    switch (t) {
        case "object": return new O();
        case "hash": return {"a": 1};
        case "list": return (1, 2);
        case "nothing": return NOTHING;
        case "null": return NULL;
        case "binary": return <0102>;
        case "date": return 2026-01-01;
        case "closure": return sub () {};
        case "int": return 5;
        case "float": return 1.5;
        case "number": return 2.5n;
        case "bool": return True;
        case "string": return "7";
        case "char": return c'A';
        case "nulchar": return c'\0';
    }
}
sub conv(string fn, string t) {
    # a string with a NUL character is not printed
    if (fn == "string" && t == "nulchar") {
        return;
    }
    auto a = v(t);
    try {
        auto r;
        switch (fn) {
            case "string": r = string(a); break;
            case "int": r = int(a); break;
            case "float": r = float(a); break;
            case "boolean": r = boolean(a); break;
        }
        printf("%s(%s) = %y\n", fn, t, r);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s(%s): %s\n", fn, t, ex.err);
    }
}
foreach string fn in (("string", "int", "float", "boolean")) {
    foreach string t in (("object", "hash", "list", "nothing", "null", "binary", "date", "closure", "int", "float",
            "number", "bool", "string", "char", "nulchar")) {
        conv(fn, t);
    }
}

# typed arguments: char types are accepted; a union type that has a type that the builtin does not accept is checked
sub typed(string label, code c) {
    try {
        printf("%s = %y\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}
sub char_param(*char c) {
    typed("string(*char)", auto sub () { return string(c); });
    typed("int(*char)", auto sub () { return int(c); });
    typed("float(*char)", auto sub () { return float(c); });
}
sub softchar_param(softchar c) {
    typed("string(softchar)", auto sub () { return string(c); });
    typed("int(softchar)", auto sub () { return int(c); });
    typed("float(softchar)", auto sub () { return float(c); });
}
sub union_int_object(union<int, object> x) {
    typed("string(union<int, object>)", auto sub () { return string(x); });
}
sub union_binary_date(union<binary, date> x) {
    typed("float(union<binary, date>)", auto sub () { return float(x); });
}
sub union_string_hash(*union<string, hash> x) {
    typed("string(*union<string, hash>)", auto sub () { return string(x); });
}
char_param(c'B');
char_param();
softchar_param(c'C');
softchar_param("D");
union_int_object(new O());
union_int_object(5);
union_binary_date(<0102>);
union_binary_date(2026-01-01);
union_string_hash({"a": 1});
union_string_hash("x");
union_string_hash();
