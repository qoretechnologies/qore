#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# calls every kind of call reference (function, static method, method of an object resolved at run time and when the
# reference is made, closure, closure of an object, and each of them bound to the sandbox) positionally, in a try
# block, with named arguments and through call_function_args(), compares them, calls them in the background, and
# calls method references to deleted objects; prints what each case returns and raises

%modern
%allow-weak-references

int sub f(int a, int b = 2) {
    return a * 10 + b;
}

int sub g(int a, int b = 2) {
    return a * 10 + b;
}

namespace NS {
    int sub f(int a, int b = 2) {
        return a * 10 + b + 7;
    }

    class K {
        static int s(int a, int b = 2) {
            return 2000 + a * 10 + b;
        }
    }
}

class C {
    public {
        int base;
    }

    constructor(int b) {
        base = b;
    }

    int m(int a, int b = 2) {
        return base + a * 10 + b;
    }

    static int s(int a, int b = 2) {
        return 1000 + a * 10 + b;
    }

    #! a reference made inside the class to a method of its own object
    code getLocalRef() {
        return \m();
    }

    #! a reference made inside the class to a method of its own object, through self
    code getSelfRef() {
        return \self.m();
    }

    #! a closure of the object
    code getClosure() {
        return int sub (int a, int b = 2) { return base + a * 100 + b; };
    }
}

class D inherits C {
    constructor() : C(500) {
    }

    #! overrides the method: a reference made in C is dispatched on the object's class
    int m(int a, int b = 2) {
        return base + a * 20 + b;
    }
}

string sub err(hash<ExceptionInfo> ex) {
    return sprintf("%s: %s", ex.err, ex.desc);
}

#! calls through the reference positionally in a loop, in a loop in a try block, with named arguments and through
#! call_function_args()
string sub calls(code c) {
    int sum = 0;
    for (int i = 0; i < 20; ++i) {
        sum += c(i, 1);
    }
    int tsum = 0;
    for (int i = 0; i < 20; ++i) {
        try {
            tsum += c(i);
        } catch (hash<ExceptionInfo> ex) {
            return "try: " + err(ex);
        }
    }
    string named;
    try {
        named = sprintf("%d/%d", c(b: 5, a: 1), c(3, b: 4));
    } catch (hash<ExceptionInfo> ex) {
        named = err(ex);
    }
    int args = call_function_args(c, (4, 7));
    return sprintf("loop %d try %d named %s args %d", sum, tsum, named, args);
}

#! calls a reference in the background and returns its result
int sub background_call(code c, int a) {
    Queue q();
    background q.push(c(a, 3));
    return q.get();
}

C typed(1);
object untyped = new C(2);
auto any = new D();
C dc = new D();

hash<string, code> refs = {
    "function": \f(),
    "static method": \C::s(),
    # references by a scoped path (references from objects compiled per file with a source symbol manifest, which
    # resolve when they are evaluated, are compared in ir/AOTNamedCallRefModules.qtest)
    "function in a namespace": \NS::f(),
    "static method in a namespace": \NS::K::s(),
    "method, typed receiver": \typed.m(),
    "method, object receiver": \untyped.m(),
    "method, auto receiver": \any.m(),
    "method in class": typed.getLocalRef(),
    "method in class, subclass": dc.getLocalRef(),
    "method through self": dc.getSelfRef(),
    "closure": int sub (int a, int b = 2) { return a * 1000 + b; },
    "closure of an object": dc.getClosure(),
};

foreach hash<auto> i in (refs.pairIterator()) {
    printf("%s: %s, background %d\n", i.key, calls(i.value), background_call(i.value, 6));
    code bound = SandboxManager::bindCall(i.value);
    printf("%s, bound: %s, background %d\n", i.key, calls(bound), background_call(bound, 6));
    printf("%s: equal to itself %y, to its bound callback %y, bound to itself %y\n", i.key, i.value == i.value,
        i.value == bound, bound == bound);
}

printf("function references to the same function: %y, to another function: %y\n", \f() == \f(), \f() == \g());
printf("static method references to the same method: %y, to a function: %y\n", \C::s() == \C::s(),
    \C::s() == \f());
printf("references by a scoped path to the same function: %y, to another function: %y, to the same static method: "
    "%y, to another static method: %y\n", \NS::f() == \NS::f(), \NS::f() == \f(), \NS::K::s() == \NS::K::s(),
    \NS::K::s() == \C::s());
printf("method references to the same method of the same object: %y, of another object: %y, to a function: %y\n",
    \typed.m() == \typed.m(), \typed.m() == \untyped.m(), \typed.m() == \f());
printf("method references with an object receiver to the same method of the same object: %y, of another object: "
    "%y\n", \untyped.m() == \untyped.m(), \untyped.m() == \any.m());
printf("references made in the class to the same object: %y, to another object: %y\n",
    typed.getLocalRef() == typed.getLocalRef(), typed.getLocalRef() == dc.getLocalRef());
printf("references made through self to the same object: %y, to another object: %y\n",
    dc.getSelfRef() == dc.getSelfRef(), dc.getSelfRef() == typed.getSelfRef());

# method references to deleted objects raise before the arguments are evaluated (not tested for a reference bound
# to the sandbox, whose call in the compiled execution modes evaluates the arguments before the bound reference
# checks the object: issue #6225)
int sub arg(reference<int> count) {
    return ++count;
}

hash<string, code> deleted_refs;
{
    C c1(3);
    object c2 = new C(4);
    C c3(5);
    deleted_refs = {
        "method, typed receiver": \c1.m(),
        "method, object receiver": \c2.m(),
        "method in class": c3.getLocalRef(),
        "method through self": c3.getSelfRef(),
    };
    delete c1;
    delete c2;
    delete c3;
}
foreach hash<auto> i in (deleted_refs.pairIterator()) {
    int count = 0;
    # called through a local variable: a call of a call reference in a hash value with an argument with side effects
    # evaluates the argument before it checks the object in the compiled execution modes (issue #6225)
    code c = i.value;
    try {
        c(arg(\count));
        printf("%s, deleted object: no exception\n", i.key);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s, deleted object: %s, arguments evaluated: %d\n", i.key, err(ex), count);
    }
}
