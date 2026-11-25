# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern

class Animal {}
class Dog inherits Animal {}
class Box<T> {}

auto sub upper<T>(auto value) {
    *Box<? extends T> v = value;
    return v;
}

auto sub directUpper(auto value) {
    *Box<? extends Animal> v = value;
    return v;
}

class Check<T> {
    auto lower(auto value) {
        *Box<? super T> v = value;
        return v;
    }
}

auto sub nested<T>(auto value) {
    *Box<? extends Box<T>> v = value;
    return v;
}

sub check(string label, code<auto(auto)> f, auto value) {
    try {
        f(value);
        printf("%s: accepted\n", label);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

Box<int> ints();
Box<Animal> animals();
Box<Dog> dogs();
Check<Dog> dogCheck();
check("direct subclass", \directUpper(), dogs);
check("direct unrelated", \directUpper(), ints);
check("upper nullable", auto sub (auto v) { return upper<Animal>(v); }, NOTHING);
check("upper subclass", auto sub (auto v) { return upper<Animal>(v); }, dogs);
check("upper exact", auto sub (auto v) { return upper<Animal>(v); }, animals);
check("upper unrelated", auto sub (auto v) { return upper<Animal>(v); }, ints);
check("upper rebound", auto sub (auto v) { return upper<int>(v); }, animals);
check("lower parent", \dogCheck.lower(), animals);
check("lower exact", \dogCheck.lower(), dogs);
check("lower unrelated", \dogCheck.lower(), ints);
Box<Box<int>> nestedInts();
Box<Box<Animal>> nestedAnimals();
check("nested exact", auto sub (auto v) { return nested<Animal>(v); }, nestedAnimals);
check("nested unrelated", auto sub (auto v) { return nested<Animal>(v); }, nestedInts);
