#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The cases of parse-time-method-wrong-type.qtest (qore#6813), for debug builds of the library: a method call resolved
# at parse time from the declared type of a builtin's value (dbg_value_as_socket(), declared to return Socket) on a
# value of another type behaves as the same call on a value with no declared type.  Each line gives the result or the
# exception of a call, as "<typed|untyped> <method> <value type>: <result>"; a call with an argument with a side
# effect gives the arguments that were evaluated as well, as the receiver is checked before them.

%modern

auto sub id(auto v) {
    return v;
}

string sub result(code c) {
    try {
        return sprintf("%y", c());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

#! the arguments that were evaluated, in order
our list<string> args_evaluated;

#! an argument with a side effect: it records its evaluation
string sub arg(string s) {
    args_evaluated += s;
    return s;
}

#! the result of a call with arguments, and the arguments that were evaluated before it returned or raised
string sub result_args(code c) {
    args_evaluated = ();
    string res = result(c);
    return sprintf("%s args: %y", res, args_evaluated);
}

# calls with an argument with a side effect in functions of their own, whose receiver is the call of the builtin with
# a constant: the lowering decides from the receiver whether the receiver is checked before the arguments
bool sub order_int() { return dbg_value_as_socket(1).hasCallableMethod(arg("m")); }
bool sub order_string() { return dbg_value_as_socket("abc").hasCallableMethod(arg("m")); }
bool sub order_nothing() { return dbg_value_as_socket(NOTHING).hasCallableMethod(arg("m")); }
bool sub order_hash() { return dbg_value_as_socket({"a": 1}).hasCallableMethod(arg("m")); }
bool sub order_list() { return dbg_value_as_socket((1, 2)).hasCallableMethod(arg("m")); }
bool sub order_float() { return dbg_value_as_socket(1.5).hasCallableMethod(arg("m")); }
auto sub order_untyped(auto u) { return u.hasCallableMethod(arg("m")); }

foreach hash<auto> i in ({"int": 1, "string": "abc", "nothing": NOTHING, "hash": {"a": 1}, "list": (1, 2),
        "float": 1.5}.pairIterator()) {
    auto v = i.value;
    printf("typed order %s: %s\n", i.key, result_args(auto sub () { return call_function("order_" + i.key); }));
    printf("untyped order %s: %s\n", i.key, result_args(auto sub () { return order_untyped(v); }));
}

hash<string, auto> vals = {
    "int": 1,
    "string": "abc",
    "nothing": NOTHING,
    "hash": {"a": 1},
    "list": (1, 2),
    "float": 1.5,
};

foreach hash<auto> i in (vals.pairIterator()) {
    auto v = i.value;
    printf("typed isOpen %s: %s\n", i.key, result(auto sub () { return dbg_value_as_socket(v).isOpen(); }));
    printf("typed size %s: %s\n", i.key, result(auto sub () { return dbg_value_as_socket(v).size(); }));
    printf("typed toString %s: %s\n", i.key, result(auto sub () { return dbg_value_as_socket(v).toString(); }));
    printf("typed className %s: %s\n", i.key, result(auto sub () { return dbg_value_as_socket(v).className(); }));
    # a call with an argument with a side effect: the receiver is checked before the arguments are evaluated
    printf("typed hasCallableMethod %s: %s\n", i.key, result_args(auto sub () {
        return dbg_value_as_socket(v).hasCallableMethod(arg("m"));
    }));
    auto u = id(v);
    printf("untyped isOpen %s: %s\n", i.key, result(auto sub () { return u.isOpen(); }));
    printf("untyped size %s: %s\n", i.key, result(auto sub () { return u.size(); }));
    printf("untyped toString %s: %s\n", i.key, result(auto sub () { return u.toString(); }));
    printf("untyped className %s: %s\n", i.key, result(auto sub () { return u.className(); }));
    printf("untyped hasCallableMethod %s: %s\n", i.key, result_args(auto sub () {
        return u.hasCallableMethod(arg("m"));
    }));
}
