# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# fixture of NamedConnectionCreation.qtest: runs one operation of a Program restricted from the network, optionally
# sandboxed to connect only to the port of 127.0.0.1 in NCC_ALLOWED_PORT, in a process whose QORE_CONNECTION_PROVIDERS
# names the test provider
# arguments: <exec mode> <sandbox: 0|1> <operation: lookup|client|connect|create> <connection name>; prints the result

%modern
%no-child-restrictions
%requires ConnectionProvider
%requires RestClient

const Modes = {"ast": EM_AST, "ir": EM_IR, "jit": EM_JIT, "tiered": EM_TIERED};

string mode = ARGV[0];
bool sandbox = ARGV[1] == "1";
string op = ARGV[2];
string name = ARGV[3];

Program p(PO_NEW_STYLE | PO_NO_NETWORK);
p.setExecMode(Modes{mode});
if (sandbox) {
    SandboxManager sm();
    sm.setNetworkDefaultPolicy(False);
    sm.addNetworkAllowedIPRange("127.0.0.1/32");
    sm.addNetworkAllowedPort(int(getenv("NCC_ALLOWED_PORT")));
    p.setSandboxManager(sm);
}
p.parse("%modern\n%requires ConnectionProvider\n%requires RestClient\n"
    # live-provider-optin: the test's own named connections, to its local servers on 127.0.0.1
    "string sub lookup(string name) { try { get_connection(name); return \"created\"; } "
    "catch (hash<ExceptionInfo> ex) { return ex.err + \": \" + ex.desc; } }\n"
    # live-provider-optin: the test's own named connections, to its local servers on 127.0.0.1
    "string sub client(string name) { try { get_connection(name).get(False); return \"client\"; } "
    "catch (hash<ExceptionInfo> ex) { return ex.err + \": \" + ex.desc; } }\n"
    # live-provider-optin: the test's own named connections, to its local servers on 127.0.0.1
    "string sub connect(string name) { try { get_connection(name).get(True); return \"connected\"; } "
    "catch (hash<ExceptionInfo> ex) { return ex.err + \": \" + ex.desc; } }\n"
    "string sub create(string name) { try { new RestConnection(name, \"d\", \"http://127.0.0.1:1\", "
    "{\"monitor\": False}); return \"created\"; } catch (hash<ExceptionInfo> ex) { "
    "return ex.err + \": \" + ex.desc; } }", "restricted");
print(p.callFunction(op, name) + "\n");
