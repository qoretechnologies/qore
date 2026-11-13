#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# a call through self or an unqualified call to a method on an object that has been deleted raises
# OBJECT-ALREADY-DELETED, before its arguments are evaluated when they can show it, as the AST interpreter raises it
# (issue #5823); prints what each case evaluates and raises

%modern

auto sub arg_value() {
    printf("  arg evaluated\n");
    return 1;
}

#! a final class: its methods are called directly
final class F {
    m(auto a) {
        printf("  F::m() called\n");
    }

    self_literal() {
        delete self;
        self.m(1);
    }

    self_function_arg() {
        delete self;
        self.m(arg_value());
    }

    plain_literal() {
        delete self;
        m(1);
    }

    plain_function_arg() {
        delete self;
        m(arg_value());
    }

    #! the call is made in a try block of its own, whose handler is in the same method
    local_try() {
        delete self;
        try {
            m(arg_value());
        } catch (hash<ExceptionInfo> ex) {
            printf("  local handler: %s\n", ex.err);
        }
        try {
            self.m(1);
        } catch (hash<ExceptionInfo> ex) {
            printf("  local handler: %s\n", ex.err);
        }
    }

    valid() {
        self.m(arg_value());
        m(1);
    }
}

#! a class that is not final: its methods are dispatched at run time, except a final method
class C {
    m(auto a) {
        printf("  C::m() called\n");
    }

    final fm(auto a) {
        printf("  C::fm() called\n");
    }

    static sm(auto a) {
        printf("  C::sm() called\n");
    }

    self_literal() {
        delete self;
        self.m(1);
    }

    self_function_arg() {
        delete self;
        self.m(arg_value());
    }

    plain_literal() {
        delete self;
        m(1);
    }

    plain_function_arg() {
        delete self;
        m(arg_value());
    }

    final_method_self() {
        delete self;
        self.fm(arg_value());
    }

    final_method_plain() {
        delete self;
        fm(1);
    }

    final_method_plain_function_arg() {
        delete self;
        fm(arg_value());
    }

    #! a static method needs no object
    static_method() {
        delete self;
        sm(arg_value());
    }

    #! closures created in the method call methods of the object
    closure_function_arg() {
        code c = sub () {
            m(arg_value());
        };
        delete self;
        c();
    }

    closure_literal() {
        code c = sub () {
            m(1);
        };
        delete self;
        c();
    }

    #! an on_exit handler of the method calls a method of the object
    on_exit_call() {
        on_exit {
            try {
                m(arg_value());
            } catch (hash<ExceptionInfo> ex) {
                printf("  on_exit handler: %s: %s\n", ex.err, ex.desc);
            }
        }
        delete self;
    }

    valid() {
        self.m(arg_value());
        m(1);
        fm(arg_value());
    }
}

sub check(string name, code c) {
    printf("%s:\n", name);
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        printf("  %s: %s\n", ex.err, ex.desc);
    }
}

sub cases() {
    check("final class, self, literal argument", sub () { F f(); f.self_literal(); });
    check("final class, self, function argument", sub () { F f(); f.self_function_arg(); });
    check("final class, unqualified, literal argument", sub () { F f(); f.plain_literal(); });
    check("final class, unqualified, function argument", sub () { F f(); f.plain_function_arg(); });
    check("final class, local try block", sub () { F f(); f.local_try(); });
    check("final class, valid object", sub () { F f(); f.valid(); });
    check("class, self, literal argument", sub () { C c(); c.self_literal(); });
    check("class, self, function argument", sub () { C c(); c.self_function_arg(); });
    check("class, unqualified, literal argument", sub () { C c(); c.plain_literal(); });
    check("class, unqualified, function argument", sub () { C c(); c.plain_function_arg(); });
    check("final method, self, function argument", sub () { C c(); c.final_method_self(); });
    check("final method, unqualified, literal argument", sub () { C c(); c.final_method_plain(); });
    check("final method, unqualified, function argument", sub () {
        C c();
        c.final_method_plain_function_arg();
    });
    check("static method", sub () { C c(); c.static_method(); });
    check("closure, function argument", sub () { C c(); c.closure_function_arg(); });
    check("closure, literal argument", sub () { C c(); c.closure_literal(); });
    check("on_exit handler", sub () { C c(); c.on_exit_call(); });
    check("class, valid object", sub () { C c(); c.valid(); });
}

# the cases run more than once, so that the tiered execution mode compiles them
for (int i = 0; i < 3; ++i) {
    printf("round %d\n", i);
    cases();
}
