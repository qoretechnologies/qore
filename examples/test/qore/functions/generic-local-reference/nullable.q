# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern
%requires reflection

class Box<T> {
    public { T value; }
    constructor(T value) { self.value = value; }
}

auto sub assignBox<T>(auto input) {
    *Box<T> box = input;
    return box.value;
}

auto sub requireBox<T>(auto input) {
    Box<T> box = input;
    return box.value;
}

code sub boxSetter<T>() {
    *Box<T> box;
    return auto sub(auto input) {
        box = input;
        return box.value;
    };
}

auto sub clearBox<T>(Box<T> input) {
    *Box<T> box = input;
    reference ref = \box;
    ref = NOTHING;
    return box;
}

auto sub assignNullable<T>(auto input) {
    *T value = input;
    return value;
}

sub show(string name, code f) {
    try {
        printf("%s: %y\n", name, f());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", name, ex.err);
    }
}

show("box", auto sub() { return assignBox<int>(new Box<int>(42)); });
show("box nothing", auto sub() { return assignBox<int>(NOTHING); });
show("required box", auto sub() { return requireBox<int>(NOTHING); });
show("wrong type argument", auto sub() { return assignBox<int>(new Box<string>("x")); });
show("captured box", auto sub() { code setter = boxSetter<int>(); return setter(NOTHING); });
show("reference to box", auto sub() { return clearBox<int>(new Box<int>(42)); });

show("parameterized type argument", auto sub() {
    return assignNullable<Box<int>>(new Box<int>(42)).value;
});
show("wrong parameterized type argument", auto sub() {
    return assignNullable<Box<int>>(new Box<string>("x")).value;
});

show("typed callable", auto sub() {
    auto f = assignNullable<code<int(int)>>(int sub(int x) { return x + 1; });
    return f(41);
});
show("callable nothing", auto sub() { return assignNullable<code<int(int)>>(NOTHING); });
show("scalar", auto sub() { return assignNullable<code<int(int)>>(23); });
show("wrong signature", auto sub() {
    auto f = assignNullable<code<int(int)>>(string sub(string s) { return s; });
    return "accepted";
});

# Both directions of the conversion must preserve the exact declaration, including varargs.
foreach string name in ("Box<int>", "code<int(string, ...)>") {
    Type required(name);
    Type optional("*" + name);
    printf("%s round trip: %y %y\n", name, optional.isEqual(required.getOrNothingType()),
        required.isEqual(optional.getMandatoryType()));
}
