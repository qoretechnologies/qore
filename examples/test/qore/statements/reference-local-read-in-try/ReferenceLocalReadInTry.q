#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# a local declared with a reference type is read where the code reads it: an exception raised by the read is handled
# by the try block around the read (issue #5908); prints what each case reads and handles

%modern

class C {
    m(auto a) {
        printf("  C::m(%y)\n", a);
    }
}

#! a class with a declared member: a reference to an undeclared member of one of its objects raises INVALID-MEMBER
#! when it is read
class Declared {
    public {
        auto y;
    }
}

sub f(auto a) {
    printf("  f(%y)\n", a);
}

#! a reference parameter read as the argument of a method call in a try block
sub method_arg(reference p) {
    C o();
    try {
        o.m(p);
    } catch (hash<ExceptionInfo> ex) {
        printf("  local handler: %s\n", ex.err);
    }
}

#! the same, read in an expression
sub method_arg_expr(reference p) {
    C o();
    try {
        o.m(p + 0);
    } catch (hash<ExceptionInfo> ex) {
        printf("  local handler: %s\n", ex.err);
    }
}

#! the same with a parameter that can also have no value
sub optional_method_arg(*reference p) {
    C o();
    try {
        o.m(p);
    } catch (hash<ExceptionInfo> ex) {
        printf("  local handler: %s\n", ex.err);
    }
}

#! a reference parameter read as the argument of a function call in a try block
sub function_arg(reference p) {
    try {
        f(p);
    } catch (hash<ExceptionInfo> ex) {
        printf("  local handler: %s\n", ex.err);
    }
}

#! a reference parameter read after other code in the try block
sub read_later(reference p) {
    C o();
    try {
        printf("  before the read\n");
        o.m(p);
    } catch (hash<ExceptionInfo> ex) {
        printf("  local handler: %s\n", ex.err);
    }
    printf("  after the try block\n");
}

#! a reference parameter read without a try block: the caller's handler gets the exception
sub no_handler(reference p) {
    C o();
    printf("  before the read\n");
    o.m(p);
    printf("  after the read (wrong)\n");
}

#! a reference parameter read by an on_exit statement in a try block
sub on_exit_read(reference p) {
    C o();
    try {
        on_exit {
            o.m(p);
        }
        printf("  in the try block\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("  local handler: %s\n", ex.err);
    }
}

#! a local declared as a reference, read in a try block
sub local_reference() {
    object g = new Declared();
    reference x = \g.q;
    C o();
    try {
        o.m(x);
    } catch (hash<ExceptionInfo> ex) {
        printf("  local handler: %s\n", ex.err);
    }
}

#! a reference parameter to a valid lvalue is read and written through the reference
sub valid_reference(reference p) {
    C o();
    o.m(p);
    p = p + 1;
    o.m(p);
}

sub check(string name, code c) {
    printf("%s:\n", name);
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        printf("  caller handler: %s\n", ex.err);
    }
}

sub cases() {
    # the cases that failed in the JIT and AOT code: the read was made when the function was entered
    check("method call argument", sub () { object g = new Declared(); method_arg(\g.q); });
    check("method call argument in an expression", sub () { object g = new Declared(); method_arg_expr(\g.q); });
    check("read after other code", sub () { object g = new Declared(); read_later(\g.q); });
    check("no handler", sub () { object g = new Declared(); no_handler(\g.q); });
    check("on_exit read", sub () { object g = new Declared(); on_exit_read(\g.q); });
    # the same reads where every execution mode already matched the AST interpreter
    check("optional reference parameter", sub () { object g = new Declared(); optional_method_arg(\g.q); });
    check("function call argument", sub () { object g = new Declared(); function_arg(\g.q); });
    check("local declared as a reference", \local_reference());
    check("valid reference", sub () {
        int i = 1;
        valid_reference(\i);
        printf("  i: %d\n", i);
    });
}

# the cases run more than once, so that the tiered execution mode compiles them
for (int i = 0; i < 3; ++i) {
    printf("round %d\n", i);
    cases();
}
