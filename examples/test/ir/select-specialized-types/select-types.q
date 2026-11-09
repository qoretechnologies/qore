%modern
# select with the predicates that IR lowers to specialized opcodes ($1 > 0, $1 != 0, $1.key > 0), and map with the
# expressions that it lowers to specialized opcodes ($1 * c, $1 + c, $1 * $1), over lists of every kind of element; see
# select-specialized-types.qtest

hashdecl IntMember {
    int v = 0;
}

hashdecl FloatMember {
    float v = 0.0;
}

class C {
    public {
        auto v;
    }

    constructor(auto x) {
        v = x;
    }
}

#! describes a select result: its type and its values
string sub show(auto l) {
    if (l.typeCode() != NT_LIST) {
        return sprintf("%y", l);
    }
    return sprintf("%s %y", l.fullType(), (map $1.typeCode() == NT_OBJECT ? "object v=" + $1.v : $1, l));
}

list<auto> sub positive(list<auto> l) { return select l, $1 > 0; }
list<auto> sub non_zero(list<auto> l) { return select l, $1 != 0; }
list<auto> sub positive_try(list<auto> l) {
    try {
        return select l, $1 > 0;
    } catch (hash<ExceptionInfo> ex) {
        return (ex.err,);
    }
}
list<auto> sub non_zero_try(list<auto> l) {
    try {
        return select l, $1 != 0;
    } catch (hash<ExceptionInfo> ex) {
        return (ex.err,);
    }
}
auto sub positive_int(list<int> l) { return select l, $1 > 0; }
auto sub non_zero_int(list<int> l) { return select l, $1 != 0; }
auto sub positive_int_try(list<int> l) {
    try {
        return select l, $1 > 0;
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
auto sub positive_float(list<float> l) { return select l, $1 > 0; }
auto sub non_zero_float_try(list<float> l) {
    try {
        return select l, $1 != 0;
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
auto sub positive_or_nothing(list<*int> l) { return select l, $1 > 0; }
auto sub non_zero_or_nothing(list<*int> l) { return select l, $1 != 0; }
auto sub positive_number(list<number> l) { return select l, $1 > 0; }
auto sub key_any(list<auto> l) { return select l, $1.v > 0; }
auto sub key_hash(list<hash<auto>> l) { return select l, $1.v > 0; }
auto sub key_int_member(list<hash<IntMember>> l) { return select l, $1.v > 0; }
auto sub key_float_member(list<hash<FloatMember>> l) { return select l, $1.v > 0; }
auto sub key_int_values(list<hash<string, int>> l) { return select l, $1.v > 0; }
auto sub key_int_member_try(list<hash<IntMember>> l) {
    try {
        return select l, $1.v > 0;
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
auto sub fused_any(list<auto> l) { return map $1 * 2, select l, $1 > 0; }
auto sub fused_int(list<int> l) { return map $1 * 2, select l, $1 > 0; }
auto sub fused_float(list<float> l) { return map $1 + 1, select l, $1 > 0; }
auto sub scale_any(list<auto> l) { return map $1 * 2, l; }
auto sub scale_any_left(list<auto> l) { return map 2 * $1, l; }
auto sub offset_any(list<auto> l) { return map $1 + 1, l; }
auto sub offset_any_left(list<auto> l) { return map 1 + $1, l; }
auto sub square_any(list<auto> l) { return map $1 * $1, l; }
auto sub scale_int(list<int> l) { return map $1 * 3, l; }
auto sub scale_int_float(list<int> l) { return map $1 * 1.5, l; }
auto sub offset_float(list<float> l) { return map $1 + 1, l; }
auto sub square_float(list<float> l) { return map $1 * $1, l; }
auto sub scale_or_nothing(list<*int> l) { return map $1 * 2, l; }

sub main() {
    list<auto> mixed = ("2", -1, 3.7, "x", 2n, True, 0, NOTHING, 0.5, -0.5n, "0");
    # every case runs more than once, so that the functions are compiled with tiered execution
    for (int i = 0; i < 3; ++i) {
        printf("positive mixed: %s\n", show(positive(mixed)));
        printf("non-zero mixed: %s\n", show(non_zero(mixed)));
        printf("positive mixed in try: %s\n", show(positive_try(mixed)));
        printf("non-zero mixed in try: %s\n", show(non_zero_try(mixed)));
        printf("positive objects: %s\n", show(positive((new C(1), new C(-1)))));
        printf("positive int: %s\n", show(positive_int((3, -1, 0, 7))));
        printf("non-zero int: %s\n", show(non_zero_int((3, -1, 0, 7))));
        printf("positive int in try: %s\n", show(positive_int_try((3, -1, 0, 7))));
        printf("positive float: %s\n", show(positive_float((0.5, -0.5, 0.0, 2.25))));
        printf("non-zero float in try: %s\n", show(non_zero_float_try((0.5, -0.5, 0.0, 2.25))));
        printf("positive *int: %s\n", show(positive_or_nothing((1, NOTHING, -2, 4))));
        printf("non-zero *int: %s\n", show(non_zero_or_nothing((1, NOTHING, 0, 4))));
        printf("positive number: %s\n", show(positive_number((0.5n, -1n, 0n, 3n))));
        printf("key auto: %s\n", show(key_any(({"v": 0.5}, {"v": "3"}, {"v": -1}, new C(2), {"v": 2n}, {"w": 1}))));
        printf("key hash: %s\n", show(key_hash(({"v": 0.5}, {"v": "3"}, {"v": -1}, {"v": 0.2n}, {}))));
        printf("key int member: %s\n", show(key_int_member((<IntMember>{"v": 1}, <IntMember>{"v": -3},
            <IntMember>{}, <IntMember>{"v": 9}))));
        printf("key float member: %s\n", show(key_float_member((<FloatMember>{"v": 0.5}, <FloatMember>{"v": -3.0},
            <FloatMember>{}))));
        printf("key int values: %s\n", show(key_int_values(({"v": 1}, {"v": -3}, {"w": 2}, {"v": 9}))));
        printf("key int member in try: %s\n", show(key_int_member_try((<IntMember>{"v": 1}, <IntMember>{"v": -3}))));
        printf("fused mixed: %s\n", show(fused_any(("2", -1, 3.7, 0.5, True))));
        printf("fused int: %s\n", show(fused_int((2, -1, 3))));
        printf("fused float: %s\n", show(fused_float((0.5, -1.0, 3.25))));
        list<auto> values = ("2", 1.5, True, 2n, NOTHING, "x", 3);
        printf("scale mixed: %s\n", show(scale_any(values)));
        printf("scale mixed (constant first): %s\n", show(scale_any_left(values)));
        printf("offset mixed: %s\n", show(offset_any(values)));
        printf("offset mixed (constant first): %s\n", show(offset_any_left(values)));
        printf("square mixed: %s\n", show(square_any(values)));
        printf("scale int: %s\n", show(scale_int((1, -2, 3))));
        printf("scale int by float: %s\n", show(scale_int_float((1, -2, 3))));
        printf("offset float: %s\n", show(offset_float((0.5, -2.0))));
        printf("square float: %s\n", show(square_float((0.5, -2.0))));
        printf("scale *int: %s\n", show(scale_or_nothing((1, NOTHING, 3))));
    }
}

main();
