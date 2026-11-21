#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# The script of child-program-private-internal-base.qtest: a class that inherits a parent class with private:internal
# is imported into a child Program, which uses it and subclasses it; the class's methods call a method of the parent
# class, in one Program and in the child (qore#6671)

%modern

class Base {
    public {
        string b = "base-member";
    }

    string bm() { return "base-method"; }
    static string sbm() { return "base-static-method"; }
}

class Mid inherits private:internal Base {
    # calls of the parent class's methods
    string call() {
        try {
            return bm();
        } catch (hash<ExceptionInfo> ex) {
            return ex.err;
        }
    }

    string callStatic() {
        try {
            return Base::sbm();
        } catch (hash<ExceptionInfo> ex) {
            return ex.err;
        }
    }

    string callRef() {
        try {
            return call_function(\bm());
        } catch (hash<ExceptionInfo> ex) {
            return ex.err;
        }
    }

    # the parent class's member, for an object of the class itself
    string member() {
        return sprintf("%y", b);
    }
}

class Sub1 inherits Mid {}

printf("one program, subclass: %s %s %s\n", (new Sub1()).call(), (new Sub1()).callStatic(), (new Sub1()).callRef());

Program p(PO_NEW_STYLE | PO_NO_INHERIT_USER_CLASSES);
p.importClass("Mid");
p.parse("class Sub inherits Mid {}
string sub sub_calls() { Sub s(); return sprintf(\"%s %s %s\", s.call(), s.callStatic(), s.callRef()); }
string sub mid_calls() { Mid m(); return sprintf(\"%s %s %s %s\", m.call(), m.callStatic(), m.callRef(), m.member()); }
", "child");
printf("child, subclass: %s\n", p.callFunction("sub_calls"));
printf("child, imported class: %s\n", p.callFunction("mid_calls"));
