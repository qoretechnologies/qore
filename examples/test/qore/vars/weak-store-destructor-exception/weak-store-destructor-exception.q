#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# Stores through a reference with the weak (:=), opaque (@=) and plain (=) assignment operators, each replacing an
# object whose destructor raises an exception, and prints the exception that the store's own handler caught and the
# target after the store: every execution mode and qcc executable must print the output of the AST interpreter (see
# weak-store-destructor-exception.qtest)

%modern
%allow-weak-references
%allow-opaque-references

#! an object whose destructor raises an exception
class ThrowingDestructor {
    public {
        int n;
    }

    constructor(int n) {
        self.n = n;
    }

    destructor() {
        throw "DTOR", sprintf("destructor %d", n);
    }
}

#! a value that a store keeps; an object, which is never copied, so the caller's reference keeps a weak store's target
class Value {
    public {
        int v;
    }

    constructor(int v) {
        self.v = v;
    }
}

#! stores through the reference with :=; the stored value is held by the caller, as a weak store does not keep it
string sub weak_store(any rr, Value held) {
    try {
        rr := held;
    } catch (hash<ExceptionInfo> ex) {
        return ex.err + ": " + ex.desc;
    }
    return "no exception";
}

#! stores through the reference with @=
string sub opaque_store(any rr, Value held) {
    try {
        rr @= held;
    } catch (hash<ExceptionInfo> ex) {
        return ex.err + ": " + ex.desc;
    }
    return "no exception";
}

#! stores through the reference with =
string sub plain_store(any rr, Value held) {
    try {
        rr = held;
    } catch (hash<ExceptionInfo> ex) {
        return ex.err + ": " + ex.desc;
    }
    return "no exception";
}

#! stores through the reference with := and uses the result of the store
string sub weak_store_result(any rr, Value held) {
    any result;
    try {
        result = (rr := held);
    } catch (hash<ExceptionInfo> ex) {
        return sprintf("%s: %s; result: %s", ex.err, ex.desc, target(result));
    }
    return sprintf("no exception; result: %s", target(result));
}

#! stores in a closure
string sub closure_store(any rr, Value held) {
    string caught = "no exception";
    sub () {
        try {
            rr := held;
        } catch (hash<ExceptionInfo> ex) {
            caught = ex.err + ": " + ex.desc;
        }
    }();
    return caught;
}

#! stores in each iteration of a loop
list<string> sub loop_store(list<Value> held) {
    list<string> rv = ();
    for (int i = 0; i < held.size(); ++i) {
        any x = new ThrowingDestructor(10 + i);
        any r = \x;
        try {
            r := held[i];
        } catch (hash<ExceptionInfo> ex) {
            rv += sprintf("%d: %s: %s; target: %s", i, ex.err, ex.desc, target(x));
            continue;
        }
        rv += sprintf("%d: no exception; target: %s", i, target(x));
    }
    return rv;
}

#! the target as it prints: the value of a Value, else the target as it is
string sub target(any y) {
    return y instanceof Value ? sprintf("Value %d", y.v) : sprintf("%y", y);
}

sub check(string name, code store, int n) {
    Value held(n);
    any y = new ThrowingDestructor(n);
    string caught = store(\y, held);
    printf("%s: %s; target: %s\n", name, caught, target(y));
}

check("weak", \weak_store(), 1);
check("opaque", \opaque_store(), 2);
check("plain", \plain_store(), 3);
check("weak result", \weak_store_result(), 4);
check("closure", \closure_store(), 5);
map printf("loop %s\n", $1), loop_store((new Value(10), new Value(11), new Value(12)));
