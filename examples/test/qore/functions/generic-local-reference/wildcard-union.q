# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern

class Box<T> {}

auto sub lowerLast<T>(auto x) {
    union<Box<T>, ? super T> v = x;
    return v;
}

auto sub lowerFirst<T>(auto x) {
    union<? super T, Box<T>> v = x;
    return v;
}

auto sub upperLast<T>(auto x) {
    union<Box<T>, ? extends T> v = x;
    return v;
}

auto sub nestedBound<T>(auto x) {
    union<Box<T>, ? super list<T>> v = x;
    return v;
}

auto sub wildcardOnly<T>(auto x) {
    union<int, ? super T> v = x;
    return v;
}

class Holder<T> {
    auto upperFirst(auto x) {
        union<? extends T, Box<T>> v = x;
        return v;
    }
}

sub check(string label, code<auto(auto)> callback, auto value) {
    try {
        auto result = callback(value);
        printf("%s: %s\n", label, exists result ? "value" : "NOTHING");
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

Box<string> box();
Holder<string> holder();
check("lower last", auto sub (auto x) { return lowerLast<string>(x); }, box);
check("lower first", auto sub (auto x) { return lowerFirst<string>(x); }, box);
check("upper last", auto sub (auto x) { return upperLast<string>(x); }, NOTHING);
check("upper last Box", auto sub (auto x) { return upperLast<string>(x); }, box);
check("class upper first", \holder.upperFirst(), NOTHING);
check("class upper first Box", \holder.upperFirst(), box);
check("nested bound", auto sub (auto x) { return nestedBound<string>(x); }, NOTHING);
check("only wildcard refers to T", auto sub (auto x) { return wildcardOnly<string>(x); }, NOTHING);
check("integer member", auto sub (auto x) { return wildcardOnly<string>(x); }, 42);
