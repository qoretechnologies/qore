#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the worker of network-domain-classes.qtest (qore#7105): code that may not use the network (PO_NO_NETWORK) cannot
# create a builtin class that starts a connection, neither by its name in code nor by a name given at run time; prints
# one "<case>=ok" line per check, or "<case>=expected <x>, got <y>"

%modern
%enable-all-warnings
%no-child-restrictions

list<string> results = ();

sub check(string name, auto expected, auto actual) {
    push results, name + "=" + (expected === actual ? "ok" : sprintf("expected %y, got %y", expected, actual));
}

#! the error of parsing the given code in a new Program with the given options, or "parsed"
string sub parse_err(int po, string code) {
    Program p(po);
    try {
        p.parse(code, "probe");
        return "parsed";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# a listener that the probes target
Socket listener();
listener.bind("127.0.0.1:0");
listener.listen();
int port = listener.getSocketInfo().port;

const Restricted = PO_MODERN | PO_NO_NETWORK;
const Plain = PO_MODERN;

# the class in code: refused at parse time; the class can be named as a type, as can Socket, but its methods
# cannot be called (on an object that a Program that may use the network gives the code)
check("parse-new", "ILLEGAL-CLASS-INSTANTIATION", parse_err(Restricted,
    sprintf("sub f() { Http1Connection c(\"127.0.0.1\", %d); }", port)));
check("parse-method", "PARSE-EXCEPTION", parse_err(Restricted,
    "bool sub f(Http1Connection c) { return c.waitForReady(1); }"));

# the class by a name given at run time: refused when the object is created
{
    Program p(Restricted);
    p.parse("auto sub f(string cls, int port) { return create_object(cls, \"127.0.0.1\", port); }", "probe");
    try {
        p.callFunction("f", "Http1Connection", port);
        check("runtime-create-object", "CREATE-OBJECT-ERROR", "created");
    } catch (hash<ExceptionInfo> ex) {
        check("runtime-create-object", "CREATE-OBJECT-ERROR", ex.err);
    }
}

# code that may use the network still can (the class is otherwise unchanged)
check("plain-parse", "parsed", parse_err(Plain, sprintf("sub f() { Http1Connection c(\"127.0.0.1\", %d); }", port)));
{
    Program p(Plain);
    p.parse("bool sub f(int port) { Http1Connection c(\"127.0.0.1\", port); on_exit c.close(); "
        "return c.waitForReady(30000); }", "probe");
    check("plain-connects", True, p.callFunction("f", port));
    Socket s = listener.accept(30000);
    s.close();
}

listener.close();

map printf("%s\n", $1), results;
