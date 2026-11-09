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
    }
}
sub conv(string fn, string t) {
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
    foreach string t in (("object", "hash", "list", "nothing", "null", "binary", "date", "closure", "int", "float", "number", "bool", "string")) {
        conv(fn, t);
    }
}
