#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The cases of system-member-combination.qtest (qore#6772): a class hierarchy that combines a member of a user class
# with a member of a system class of the same name is refused, whatever the order of the parent classes, and the error
# names the class being parsed.  Each case parses a Program of its own and gives "ok" or the error.

%modern

string sub parse_case(string code) {
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
    try {
        p.parse(code, "case");
        return "ok";
    } catch (hash<ExceptionInfo> ex) {
        return sprintf("%s: %s", ex.err, ex.desc);
    }
}

hash<string, string> cases = {
    # a public member of a system class (FtpClient::path) and a public member of a user class: both orders
    "public order 1": "class U { public { string path = \"user\"; } } class X inherits FtpClient, U {}",
    "public order 2": "class U { public { string path = \"user\"; } } class X inherits U, FtpClient {}",
    # the same with the user class's member declared as int: both orders
    "public int order 1": "class A { public { int path = 42; } } class X inherits FtpClient, A {}",
    "public int order 2": "class A { public { int path = 42; } } class X inherits A, FtpClient {}",
    # the private:internal member goal of a system class (SocketPollOperationBase, inherited by PollPipeline;
    # qore#6839) and a private member of a user class: accepted in both orders, as a private:internal member never
    # reaches the check and the user class's member has storage of its own (see
    # ../AbstractPollOperation/system-poll-member-access.qtest)
    "private order 1": "class U { private { string goal; } } class X inherits PollPipeline, U {}",
    "private order 2": "class U { private { string goal; } } class X inherits U, PollPipeline {}",
    # no member of the same name: accepted
    "other member": "class U { public { string other_path = \"user\"; } } class X inherits FtpClient, U {}",
    # two system classes with a member of the same name: accepted, as before
    "two system classes": "class X inherits SocketPollOperationBase, Http1ClientPollOperationBase {}",
};

foreach hash<auto> i in (cases.pairIterator()) {
    printf("%s: %s\n", i.key, parse_case(i.value));
}
