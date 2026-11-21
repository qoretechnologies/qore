# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# Reflection uses a method or member of a class inherited with private:internal on an object of the inheriting class
# and of its subclass: every element for a caller that may use every element, and as the language does for the class
# context of a caller to which the language's member access applies (qore#6644); see
# reflection-private-internal.qtest

%modern
%no-child-restrictions
%requires reflection

# the classes, the operations and the callers, in this Program and in a child Program whose functional domains are
# restricted, to which the language's member access applies
const Code = "
class A {
    public {
        int v = 1;
    }
    int m() { return 2; }
}
class BI inherits private:internal A {
    static string ops(object o) OPS
}
class CI inherits BI {
    static string ops(object o) OPS
}
class X {
    static string ops(object o) OPS
}

# the result of an operation, or its exception
string sub try_get(code c) {
    try {
        return sprintf(\"%y\", c());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub outside(object o) OPS

string sub all() {
    list<string> l = ();
    foreach string cls in ((\"BI\", \"CI\")) {
        l += sprintf(\"%s object: outside: %s\", cls, outside(create_object(cls)));
        l += sprintf(\"%s object: BI: %s\", cls, BI::ops(create_object(cls)));
        l += sprintf(\"%s object: CI: %s\", cls, CI::ops(create_object(cls)));
        l += sprintf(\"%s object: X: %s\", cls, X::ops(create_object(cls)));
    }
    return l.join(\"\\n\");
}
";

# the reflection operations on an object, in the class context of the code that contains them (closures run in the
# class context of the code that creates them)
const Ops = "{
        Class a = Class::forName(\"A\");
        NormalMethod m = a.getNormalMethod(\"m\");
        NormalMember v = a.getMember(\"v\");
        list<string> l = ();
        l += try_get(auto sub () { return m.call(o); });
        l += try_get(auto sub () { return m.callArgs(o); });
        l += try_get(auto sub () { return m.getVariant().call(o); });
        l += try_get(auto sub () { return v.getValue(o); });
        l += try_get(auto sub () { v.setValue(o, 5); return v.getValue(o); });
        return l.join(\" \");
    }";
const Src = replace(Code, "OPS", Ops);

# the caller may use every element
Program p(PO_NEW_STYLE | PO_NO_INHERIT_USER_CLASSES | PO_NO_INHERIT_USER_FUNC_VARIANTS);
p.loadModule("reflection");
p.parse(Src, "unrestricted");
printf("unrestricted caller:\n%s\n", p.callFunction("all"));

# the language's member access applies to the caller
Program r(PO_NEW_STYLE | PO_NO_INHERIT_USER_CLASSES | PO_NO_INHERIT_USER_FUNC_VARIANTS | PO_NO_PROCESS_CONTROL);
r.loadModule("reflection");
r.parse(Src, "restricted");
printf("restricted caller:\n%s\n", r.callFunction("all"));
