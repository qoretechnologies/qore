# Copyright 2026 Qore Technologies, s.r.o.

# the probe of the reference case of AOTGenericLocalType.qtest (qore#6121): a qcc executable whose generic functions
# take references to selected elements of generic locals loads, and its output is that of the AST interpreter
%modern

sub show(string label, code c) {
    try {
        printf("%s => %y\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s => %s: %s\n", label, ex.err, ex.desc);
    }
}

auto sub writeElement<T>(auto y) {
    hash<string, T> z = {"value": y};
    string key = "value";
    sub (reference<T> r) { r = y; }(\z{key});
    return z{"value"};
}

auto sub readElement<T>(auto y) {
    hash<string, T> z = {"value": y};
    string key = "value";
    return auto sub (reference r) { return r; }(\z{key});
}

auto sub readLiteralKey<T>(auto y) {
    hash<string, T> z = {"value": y};
    return auto sub (reference r) { return r; }(\z{"value"});
}

auto sub readListElement<T>(auto y) {
    list<T> l = (y,);
    return auto sub (reference r) { return r; }(\l[0]);
}

auto sub readNestedElement<T>(auto y) {
    hash<string, hash<string, T>> z = {"a": {"value": y}};
    return auto sub (reference r) { return r; }(\z.a.value);
}

show("write element", auto sub () { return writeElement<string>("s"); });
show("read element", auto sub () { return readElement<string>("s"); });
show("read literal key", auto sub () { return readLiteralKey<string>("s"); });
show("read list element", auto sub () { return readListElement<string>("s"); });
show("read nested element", auto sub () { return readNestedElement<string>("s"); });
