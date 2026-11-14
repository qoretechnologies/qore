# Copyright (C) 2026 Qore Technologies, s.r.o.
# Captured parameters keep the callee receiver when called from another instantiation.
%modern
class Box<T> {
    code capture(T value) {
        return auto sub () { return value; };
    }
    auto from(Box<string> other) {
        code c = other.capture("42");
        return c();
    }
}
class StringBox inherits Box<string> { }
class IntBox inherits Box<int> { }
Box<int> caller();
Box<string> callee();
IntBox inherited_caller();
StringBox inherited_callee();
for (int i = 0; i < 5; ++i) {
    try { printf("%y / %y\n", caller.from(callee), inherited_caller.from(inherited_callee)); }
    catch (hash<ExceptionInfo> ex) { printf("%s: %s\n", ex.err, ex.desc); }
}

class Params<T> {
    code defaults(*T value = NOTHING) { return auto sub(auto input) { value = input; return value; }; }
    code missing(*T value) { return auto sub(auto input) { value = input; return value; }; }
    auto from(Params<string> other) {
        code defaulted = other.defaults();
        code omitted = other.missing();
        return (defaulted("d"), omitted("m"));
    }
}
Params<string> string_params();
Params<int> int_params();
for (int i = 0; i < 5; ++i) {
    try { printf("default/missing: %y\n", int_params.from(string_params)); }
    catch (hash<ExceptionInfo> ex) { printf("%s: %s\n", ex.err, ex.desc); }
}
