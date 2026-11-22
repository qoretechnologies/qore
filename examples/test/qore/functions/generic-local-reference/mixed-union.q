# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern
%requires reflection

enum Color : int { Red = 1 }

class Box<T> {
    public { T value; }
    constructor(T value) { self.value = value; }
}

auto sub assign<T>(auto input) {
    union<Box<T>, string> value = input;
    return value;
}

auto sub optional<T>(auto input) {
    *union<Box<T>, string> value = input;
    return value;
}

auto sub late<T>(auto input) {
    union<Box<int>, T> value = input;
    return value;
}

auto sub leading<T>(auto input) {
    union<T, Box<int>> value = input;
    return value;
}

auto sub simple<T>(auto input) {
    union<T, int> value = input;
    return value;
}

auto sub optionalLate<T>(auto input) {
    *union<Box<int>, T> value = input;
    return value;
}

auto sub filtered<T>(auto input) {
    union<Box<T>, softint> value = input;
    return value;
}

auto sub filteredHash<T>(auto input) {
    *union<Box<T>, hash<string, softint>> value = input;
    return value;
}

auto sub callable<T>(auto input) {
    union<Box<T>, code<T(T)>> value = input;
    return value(10);
}

auto sub hashValue<T>(auto input) {
    hash<string, union<int, T>> values = {"entry": input};
    return values.entry;
}

auto sub optionalArgument<T>(auto input) {
    *T value = input;
    return value;
}

sub show(string label, code probe) {
    try {
        printf("%s: %y\n", label, probe());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

show("box", auto sub() { return assign<int>(new Box<int>(42)).value; });
show("other member", auto sub() { return assign<int>("text"); });
show("wrong box", auto sub() { return assign<int>(new Box<string>("no")).value; });
show("mandatory", auto sub() { return assign<int>(NOTHING); });
show("optional", auto sub() { return optional<int>(NOTHING); });
show("optional member", auto sub() { return optional<int>("text"); });
show("late parameter", auto sub() { return late<string>("text"); });
show("late parameter rejects int", auto sub() { return late<string>(10); });
show("late nullable parameter", auto sub() { return late<string>(NOTHING); });
show("leading nullable parameter", auto sub() { return leading<string>(NOTHING); });
show("nullable scalar union", auto sub() { return simple<string>(NOTHING); });
show("optional late parameter", auto sub() { return optionalLate<string>(NOTHING); });
show("soft conversion", auto sub() { return filtered<int>("42"); });
hash<string, string> source = {"key": "42"};
auto converted = filteredHash<int>(source);
printf("shared hash: %s %y %y\n", converted.fullType(), converted.key, source.key);
show("shared hash enforces values", auto sub () { converted.key = {}; return converted.key; });
show("callable", auto sub() { return callable<int>(int sub(int n) { return n + 1; }); });
show("wrong signature", auto sub() { return callable<int>(string sub(string s) { return s; }); });
show("symbolic hash element", auto sub() { return hashValue<string>("text"); });
show("union argument", auto sub() { return optionalArgument<union<Box<int>, string>>("text"); });
show("union argument rejects int", auto sub() { return optionalArgument<union<Box<int>, string>>(42); });
Type required("union<Box<int>, string>");
Type nullable("*union<Box<int>, string>");
printf("round trip: %y %y\n", required.getOrNothingType().isEqual(nullable),
    nullable.getMandatoryType().isEqual(required));
union<Box<int>, *Color> sub maybeColor() { return Color::Red; }
union<Box<int>, Color> sub requireColor() { return Color::Red; }
Type enumMember = Function::forName("maybeColor").getVariants()[0].getReturnType();
Type requiredEnum = Function::forName("requireColor").getVariants()[0].getReturnType();
printf("nullable enum member: %y\n", enumMember.getMandatoryType().isEqual(requiredEnum));
Type nested("union<Box<int>, union<*int, string>>");
Type requiredNested("union<Box<int>, union<int, string>>");
printf("nested nullable member: %y\n", nested.getMandatoryType().isEqual(requiredNested));

class Declarations<T> {
    union<Box<int>, T> required(auto x) { return x; }
    *union<Box<int>, T> optional(auto x) { return x; }
}
Class owner = Class::forName("Declarations");
Type symbol = owner.getMethod("required").getVariants()[0].getReturnType();
Type optionalSymbol = owner.getMethod("optional").getVariants()[0].getReturnType();
printf("symbolic round trip: %y %y %y\n", !symbol.isEqual(optionalSymbol),
    symbol.getOrNothingType().isEqual(optionalSymbol), optionalSymbol.getMandatoryType().isEqual(symbol));
