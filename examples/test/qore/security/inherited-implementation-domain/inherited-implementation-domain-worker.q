#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the worker of inherited-implementation-domain.qtest (qore#7543): a builtin method of a class in a functional domain
# that implements, in a Qore class that inherits both, an abstract method of a parent class that has no functional
# domain, is refused to code restricted from the domain as any other builtin method of the class, whatever the form
# of the call; prints one "<case>=ok" line per check, or "<case>=<the differences>"

%modern
%enable-all-warnings
%no-child-restrictions
%requires FsUtil
%requires HttpClientIo

#! a class without a functional domain that declares a method that a builtin class of the FILESYSTEM domain has
class Lines {
    abstract *string readLine(bool include_eol = True, *string eol);
}

#! the abstract method is implemented by the builtin method of ReadOnlyFile, which is in the FILESYSTEM domain
class ScriptFile inherits Lines, ReadOnlyFile {
    constructor(string path) : ReadOnlyFile(path) {
    }
}

#! a class without a functional domain that declares a method that a builtin class without a domain has
class Reader {
    abstract *binary readBinary(int limit = -1);
}

#! the abstract method is implemented by the builtin method of StreamReader, which has no functional domain
class ScriptStream inherits Reader, StreamReader {
    constructor() : StreamReader(new BinaryInputStream(<6162>)) {
    }
}

#! the abstract method is implemented by a method in Qore
class ScriptLines inherits Lines {
    *string readLine(bool include_eol = True, *string eol) {
        return "line";
    }
}

#! a class that declares an overload of its own of a method that it inherits from a builtin class in the FILESYSTEM
#! domain: a call of the inherited builtin overload resolves through the method of this class
class OwnOverload inherits ReadOnlyFile {
    constructor(string path) : ReadOnlyFile(path) {
    }

    *string readLine(int n) {
        return "own";
    }
}

#! the methods called: their arguments, and the abstract parent class that declares them
const Methods = {
    "getState": {"type": "AbstractPollOperation", "args": "", "values": ()},
    "getGoal": {"type": "AbstractPollOperation", "args": "", "values": ()},
    "goalReached": {"type": "AbstractPollOperation", "args": "", "values": ()},
    "continuePoll": {"type": "AbstractPollOperation", "args": "", "values": ()},
    "readLine": {"type": "Lines", "args": "False", "values": (False,)},
    "readBinary": {"type": "Reader", "args": "1", "values": (1,)},
    # the overload of OwnOverload, a class in the FILESYSTEM domain, which code restricted from it cannot name
    "readLineOwn": {"name": "readLine", "type": "object", "args": "1", "values": (1,)},
};

#! the forms of a call: on an object reference, through a variable of the abstract parent class, with
#! call_object_method_args(), through a call reference, and through a call reference that the host calls
const Forms = ("object", "typed", "dynamic", "call-ref", "host-call-ref");

const Refused = "INVALID-FUNCTION-ACCESS";

list<string> results = ();

#! the code of the Program that makes the calls
string sub make_code() {
    string code;
    foreach hash<auto> i in (Methods.pairIterator()) {
        string f = i.key;
        string m = i.value.name ?? f;
        string a = i.value.args;
        code += sprintf("auto sub object_%s(object o) { return o.%s(%s); }\n", f, m, a);
        code += sprintf("auto sub typed_%s(%s o) { return o.%s(%s); }\n", f, i.value.type, m, a);
        code += sprintf("auto sub dynamic_%s(object o) { return call_object_method_args(o, \"%s\", (%s)); }\n", f,
            m, a);
        code += sprintf("auto sub call_ref_%s(object o) { code c = \\o.%s(); return c(%s); }\n", f, m, a);
        code += sprintf("code sub make_ref_%s(object o) { return \\o.%s(); }\n", f, m);
    }
    return code;
}

#! the result of a call of \a m on \a obj in the form \a form in \a p: the error, or "called"
string sub call(Program p, string form, string m, object obj) {
    try {
        if (form == "host-call-ref") {
            code c = p.callFunction("make_ref_" + m, obj);
            call_function_args(c, Methods{m}.values);
        } else {
            p.callFunction(replace(form, "-", "_") + "_" + m, obj);
        }
        return "called";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

#! checks that each of \a forms of a call of each of \a methods on \a obj in a Program with \a po gives \a expected
sub check(string name, int po, object obj, list<string> methods, string expected, list<string> forms = Forms) {
    Program p(po);
    p.importClass("Lines");
    p.importClass("Reader");
    p.parse(make_code(), "calls");
    list<string> diffs = ();
    foreach string m in (methods) {
        foreach string form in (forms) {
            string rv = call(p, form, m, obj);
            if (rv != expected) {
                push diffs, sprintf("%s %s(): %s", form, m, rv);
            }
        }
    }
    push results, name + "=" + (diffs ? diffs.join("; ") : "ok");
}

# a listener that the poll operation targets: no connection may be made to it by the restricted code
Socket listener();
listener.bind("127.0.0.1:0");
listener.listen();
int port = listener.getSocketInfo().port;

TmpDir tmp("qore-inherited-implementation-domain-");
string path = tmp.path + DirSep + "lines.txt";
{
    File f();
    f.open2(path, O_CREAT | O_WRONLY | O_TRUNC);
    f.write("line\n");
}

list<string> poll_methods = ("getState", "getGoal", "goalReached", "continuePoll");

{
    HttpClientIo::Http1ClientConnection conn(parse_url("http://127.0.0.1:" + port));
    # the methods of the poll operation of the connection that the Qore class HttpClientIo::Http1ClientPollOperation
    # inherits from Qore::Http1ClientPollOperationBase, which implement the abstract methods of
    # Qore::AbstractPollOperation that it also inherits from HttpClientIo::HttpClientPollOperation
    check("network-poll-operation", PO_MODERN | PO_NO_NETWORK, conn.getPollOperation(), poll_methods, Refused);
    # no connection was made
    push results, "network-no-connection=" + (listener.accept(0) ? "connected" : "ok");
    # code that may use the network can call them
    check("network-poll-operation-allowed", PO_MODERN, conn.getPollOperation(), ("getState", "getGoal",
        "goalReached"), "called");
    # a method of the module that calls a builtin method of the poll operation for restricted code still does
    {
        Program p(PO_MODERN | PO_NO_NETWORK);
        p.parse("auto sub f(object c) { return c.getActiveStreamCount(); }", "module-call");
        string rv;
        try {
            rv = sprintf("%y", p.callFunction("f", conn));
        } catch (hash<ExceptionInfo> ex) {
            rv = ex.err;
        }
        push results, "network-module-method=" + (rv == "0" ? "ok" : rv);
    }
}

# the same with a class of this Program and a builtin class in the FILESYSTEM domain
check("filesystem-file", PO_MODERN | PO_NO_FILESYSTEM, new ScriptFile(path), ("readLine",), Refused);
check("filesystem-file-allowed", PO_MODERN, new ScriptFile(path), ("readLine",), "called");
# the builtin overload of a method of which the class declares an overload of its own; the class's own overload stays
# callable
list<string> untyped_forms = select Forms, $1 != "typed";
check("filesystem-own-overload", PO_MODERN | PO_NO_FILESYSTEM, new OwnOverload(path), ("readLine",), Refused,
    untyped_forms);
check("filesystem-own-overload-own", PO_MODERN | PO_NO_FILESYSTEM, new OwnOverload(path), ("readLineOwn",),
    "called");

# an implementation without a functional domain stays callable by restricted code
check("no-domain-builtin", PO_MODERN | PO_NO_FILESYSTEM | PO_NO_NETWORK, new ScriptStream(), ("readBinary",),
    "called");
check("no-domain-qore", PO_MODERN | PO_NO_FILESYSTEM | PO_NO_NETWORK, new ScriptLines(), ("readLine",), "called");

listener.close();

print(results.join("\n") + "\n");
