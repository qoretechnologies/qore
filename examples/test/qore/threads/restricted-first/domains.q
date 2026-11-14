#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# a module's declared domains widen only the parse-option domains of its own code, never its caller's sandbox
# (qorus#867): DomThreadNet declares NETWORK and THREAD_CONTROL, DomThreadPlain declares nothing

# Copyright 2026 Qore Technologies, s.r.o.

%new-style
%require-types
%strict-args

string path = ARGV[0];
# a listener for the connections of the module's thread
Socket listener();
listener.bind("127.0.0.1:0");
listener.listen();
int port = listener.getSocketInfo().port;
Counter accepting(1);
background sub () {
    on_exit accepting.dec();
    # closing the listener at the end ends the loop
    try {
        while (True) {
            listener.accept().close();
        }
    } catch (hash<ExceptionInfo> ex) {
        if (ex.err != "SOCKET-CLOSED") {
            rethrow;
        }
    }
}();
on_exit {
    listener.close();
    accepting.waitForZero();
}

# a caller that may not use the network, the filesystem nor control threads
{
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_NO_THREAD_CONTROL | PO_NO_NETWORK
        | PO_NO_FILESYSTEM);
    p.loadModule("DomThreadNet");
    p.parse("hash<auto> sub probe(int port, string path) { return DomThreadNet::probe(port, path); }", "restricted");
    hash<auto> h = p.callFunction("probe", port, path);
    # the declared NETWORK domain is granted to the module's thread, the undeclared FILESYSTEM domain is not
    printf("restricted: net %s, fs %s\n", h.net, h.fs);
}

# a module that declares nothing cannot start a thread for such a caller: the caller cannot load it
{
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_NO_THREAD_CONTROL);
    try {
        p.loadModule("DomThreadPlain");
        printf("plain: loaded\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("plain: %s\n", ex.err);
    }
}

# a sandboxed caller whose sandbox denies the network: the declaration does not widen the sandbox
{
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_NO_THREAD_CONTROL);
    p.loadModule("DomThreadNet");
    p.parse("hash<auto> sub probe(int port, string path) { return DomThreadNet::probe(port, path); }", "sandboxed");
    SandboxManager sm();
    sm.setNetworkDefaultPolicy(False);
    p.setSandboxManager(sm);
    hash<auto> h = p.callFunction("probe", port, path);
    printf("sandboxed: net %s\n", h.net);
}
