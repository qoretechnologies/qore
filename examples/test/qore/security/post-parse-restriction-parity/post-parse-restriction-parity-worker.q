#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the worker of post-parse-restriction-parity.qtest (qore#7578): a method call that the parser resolved, in code of a
# Program that a functional-domain restriction is set for after the code was parsed, on an object of a class of
# another Program that the Program has a copy of (imported or inherited), calls the method resolved at parse time, as
# on an object of the Program's own copy of the class, in every execution mode; prints one "<case>=<result>" line per
# check

%modern
%enable-all-warnings
%no-child-restrictions

#! a class without a functional domain that declares a method that a builtin class of the FILESYSTEM domain has
public class Lines {
    abstract *string readLine(bool include_eol = True, *string eol);
}

#! the abstract method is implemented by the builtin method of ReadOnlyFile, which is in the FILESYSTEM domain
public class ScriptFile inherits Lines, ReadOnlyFile {
    constructor(string path) : ReadOnlyFile(path) {
    }

    #! a call of a method of self in the code of the class
    *string selfRead() {
        return readLine(False);
    }
}

#! a class that declares an overload of its own of a method that it inherits from ReadOnlyFile
public class OwnOverload inherits ReadOnlyFile {
    constructor(string path) : ReadOnlyFile(path) {
    }

    *string readLine(int n) {
        return "own";
    }
}

#! a class that inherits the builtin methods of ReadOnlyFile only
public class Plain inherits ReadOnlyFile {
    constructor(string path) : ReadOnlyFile(path) {
    }
}

#! a subclass, which is not the class that the call is resolved for: its method is resolved by name at run time
public class SubScriptFile inherits ScriptFile {
    constructor(string path) : ScriptFile(path) {
    }
}

#! the code of the Program that makes the calls
const Code = "auto sub script_file(ScriptFile o) { return o.readLine(False); }
auto sub own_overload(OwnOverload o) { return o.readLine(False); }
auto sub own_overload_own(OwnOverload o) { return o.readLine(1); }
auto sub plain(Plain o) { return o.readLine(False); }
auto sub builtin(ReadOnlyFile o) { return o.readLine(False); }
auto sub file_name(ScriptFile o) { return o.getFileName(); }
auto sub named(ScriptFile o) { return o.readLine(include_eol: False); }
auto sub self_call(ScriptFile o) { return o.selfRead(); }
auto sub self_call_copy(ScriptFile o) { ScriptFile c('/dev/null'); return c.selfRead(); }
";

#! the functions of Code, and the class of the object that each is called with
const Calls = {
    "script_file": "ScriptFile",
    "own_overload": "OwnOverload",
    "own_overload_own": "OwnOverload",
    "plain": "Plain",
    "builtin": "ReadOnlyFile",
    "file_name": "ScriptFile",
    # a call with named arguments of the overloaded method is resolved at run time
    "named": "ScriptFile",
    "self_call": "ScriptFile",
    # an object of the Program's copy of the class: the class's own code calls a method of self on it
    "self_call_copy": "ScriptFile",
};

const ProbePath = "/dev/null";

#! creates an object of \a cls in this Program
object sub make(string cls) {
    return create_object(cls, ProbePath);
}

#! returns "called", or the error of the call
string sub call(Program p, string func, object o) {
    try {
        p.callFunction(func, o);
        return "called";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

#! returns a Program with the classes of this Program: imported, or inherited as a child Program inherits them
Program sub make_program(bool imported, int po = PO_MODERN) {
    Program p(po | (imported ? PO_NO_INHERIT_USER_CLASSES : 0));
    if (imported) {
        foreach string cls in (("Lines", "ScriptFile", "OwnOverload", "Plain")) {
            p.importClass(cls);
        }
    }
    return p;
}

list<string> results = ();

foreach bool imported in (True, False) {
    string label = imported ? "imported" : "inherited";
    # the restriction is set after the code is parsed: the calls resolved at parse time call the method resolved
    {
        Program p = make_program(imported);
        p.parse(Code, "calls");
        p.setParseOptions(PO_NO_FILESYSTEM);
        foreach hash<auto> i in (Calls.pairIterator()) {
            push results, sprintf("%s-%s=%s", label, i.key, call(p, i.key, make(i.value)));
        }
        # an object of a subclass: the method is resolved by name and checked when the call is made
        push results, sprintf("%s-subclass=%s", label, call(p, "script_file", new SubScriptFile(ProbePath)));
        # a call on a deleted object of a copy of the class raises the error of the dispatch by name
        {
            object o = make("ScriptFile");
            {
                object d = o;
                delete d;
            }
            try {
                p.callFunction("file_name", o);
                push results, label + "-deleted=called";
            } catch (hash<ExceptionInfo> ex) {
                push results, sprintf("%s-deleted=%s: %s", label, ex.err, ex.desc);
            }
        }
    }
    # with the restriction set before the code is parsed, the parser refuses the call
    {
        Program p = make_program(imported, PO_MODERN | PO_NO_FILESYSTEM);
        try {
            p.parse("auto sub f(ScriptFile o) { return o.readLine(False); }", "restricted");
            push results, label + "-parse=parsed";
        } catch (hash<ExceptionInfo> ex) {
            push results, sprintf("%s-parse=%s", label, ex.err);
        }
    }
}

print(results.join("\n") + "\n");
