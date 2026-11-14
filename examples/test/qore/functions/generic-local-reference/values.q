# Copyright (C) 2026 Qore Technologies, s.r.o.
# qore#6108: references and captured storage bind the creating invocation's type arguments.
%modern

auto sub typed<T>(auto y) {
    *T z;
    sub (reference<*T> r) { r = y; }(\z);
    return z;
}

auto sub untyped<T>(auto y) {
    *T z;
    code f = sub (reference r) { r = y; };
    f(\z);
    return z;
}

auto sub direct<T>(auto y) { *T z; z = y; return z; }
auto sub directRef<T>(auto y) { *T z; auto r = \z; z = y; return z; }
auto sub initialized<T>(auto y) {
    *T z = y;
    sub (reference r) { r = y; }(\z);
    return z;
}

code sub setter<T>() {
    *T z;
    return auto sub (auto y) { z = y; return z; };
}

reference<*T> sub makeRef<T>() { *T z; return \z; }
auto sub foreignWrite<U>(reference r, auto y) { r = y; return r; }
auto sub escapedReference(auto y) {
    reference r = makeRef<string>();
    return foreignWrite<int>(\r, y);
}

auto sub noValue<T>() { *T z; auto r = \z; return z; }

class Box<T> {
    auto local(auto y) {
        *T z;
        sub (reference<*T> r) { r = y; }(\z);
        return z;
    }
    code makeSetter() {
        *T z;
        return auto sub (auto y) { z = y; return z; };
    }
}
class StringBox inherits Box<string> { }

string sub outcome(code c) {
    try { return sprintf("%y", c()); }
    catch (hash<ExceptionInfo> ex) { return ex.err + ": " + ex.desc; }
}

sub check(string name, code c) { printf("%s: %s\n", name, outcome(c)); }

for (int iteration = 0; iteration < 3; ++iteration) {
    # Reuse each shared parse tree with distinct concrete type arguments.
    check("typed-string", auto sub () { return typed<string>("s"); });
    check("typed-int", auto sub () { return typed<int>(7); });
    check("untyped-string", auto sub () { return untyped<string>("s"); });
    check("untyped-int", auto sub () { return untyped<int>(7); });
    check("direct", auto sub () { return direct<string>("s"); });
    check("direct-ref", auto sub () { return directRef<string>("s"); });
    check("initialized", auto sub () { return initialized<string>("s"); });
    check("nullable", auto sub () { return untyped<string>(NOTHING); });
    check("unassigned", auto sub () { return noValue<int>(); });
    check("invalid-direct", auto sub () { return direct<string>(1); });
    check("invalid-ref", auto sub () { return untyped<string>(1); });
    code strings = setter<string>();
    code ints = setter<int>();
    check("escaped-string", auto sub () { return strings("s"); });
    check("escaped-int", auto sub () { return ints(7); });
    check("escaped-string-again", auto sub () { return strings("t"); });
    check("escaped-invalid", auto sub () { return strings(1); });
    check("escaped-reference", auto sub () { return escapedReference("s"); });
    check("foreign-invalid", auto sub () { return escapedReference(1); });
    Box<string> box();
    StringBox inherited();
    check("class-local", auto sub () { return box.local("s"); });
    check("inherited-local", auto sub () { return inherited.local("s"); });
    code classSetter = inherited.makeSetter();
    check("class-escaped", auto sub () { return classSetter("s"); });
    check("class-invalid", auto sub () { return classSetter(1); });
}
