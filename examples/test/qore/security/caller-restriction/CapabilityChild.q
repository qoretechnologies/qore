#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the checks of caller-restriction-capability.qtest: the host lends a connection of library code to a restricted
# Program; ARGV[0] is "restricted" or "unrestricted"; prints one line per result: the key, "=", and the value (%y)

%modern
%no-child-restrictions
%requires CapProbe
%requires CapInherit
%requires CapRegistry
%requires ConnectionProvider
%requires RestClient
%requires SmtpClient

int po = PO_NEW_STYLE;
if (ARGV[0] == "restricted") {
    po |= PO_NO_NETWORK | PO_NO_UNCONTROLLED_APIS | PO_NO_PROCESS_CONTROL;
}
Program p(po);
p.parse("%modern
%requires CapProbe
%requires CapRegistry
class Sub inherits CapProbe::Conn {
}
# connections of the library that the host lends: their client is created when they are used
string sub lent_client(object c, bool async) {
    try {
        async ? c.getAsync(False) : c.get(False);
        return \"client\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
hash<auto> sub run(CapProbe::Conn lent, code host_ref, hash<string, list<auto>> lent_connections, code make_in_grant,
        CapProbe::DtorProbe dtor) {
    hash<auto> h;
    # the destructor of a lent object that the restricted code deletes is not called as a capability
    delete dtor;
    h.lent_destructor = CapProbe::destructor_probes()[0];
    # library code that acts on a connection of the host, which it holds or which the restricted code gives it, is
    # checked against the code that calls the library: only the code that the object was handed to uses it as a
    # capability
    h.registry_get = CapRegistry::registered_get();
    h.helper_get = CapRegistry::helper_get(lent);
    # a connection that trusted module code creates under a host grant for the restricted code is not lent: the grant
    # does not outlive itself
    object made = make_in_grant();
    h.made_in_grant_get = made.get();
    foreach hash<auto> i in (lent_connections.pairIterator()) {
        h{\"lent_connection_\" + i.key} = lent_client(i.value[0], i.value[1]);
    }
    # a connection that the host lent: its methods do what they do
    h.lent_get = lent.get();
    h.lent_probe = lent.probe();
    h.lent_nested_get = lent.nested().get();
    h.lent_nested_probe = lent.nested().probe();
    h.lent_thread = lent.threadProbe();
    h.lent_native = lent.nativeProbe();
    h.lent_process = lent.processProbe();
    # a factory of the lent connection that creates an object of the class that the restricted code names: only an
    # object of the lent module's own class is lent, not one of another module's class
    h.factory_other_probe = lent.make(\"CapGadget::Gadget\").probe();
    h.factory_own_get = lent.make(\"CapProbe::Conn\").get();
    # a module loaded for the first time in a lent method: what its initialization creates is not lent
    lent.loadLazy();
    h.lazy_shared_get = CapProbe::lazy_shared().get();
    # code of the restricted Program that a lent connection calls is restricted
    h.lent_callback = lent.callback(bool sub () { return SandboxManager::isCallerRestricted(DOM_NETWORK); });
    # a reference to a method of the lent connection: restricted if the restricted Program created it
    h.own_ref_to_lent = (\\lent.probe())();
    h.host_ref_to_lent = host_ref();
    # the library's own shared state, created in its initialization, is not lent: it is checked against its caller
    h.library_shared_get = CapProbe::shared().get();
    h.library_shared_probe = CapProbe::shared().probe();
    # connections that the restricted Program created itself are restricted
    CapProbe::Conn mine();
    h.own_get = mine.get();
    h.own_probe = mine.probe();
    Sub s();
    h.own_subclass_get = s.get();
    return h;
}
", ARGV[0]);
CapProbe::Conn lent();
Socket listener();
listener.bind("127.0.0.1:0");
listener.listen();
int port = listener.getSocketInfo().port;
hash<string, list<auto>> lent_connections = {
    "http": (new ConnectionProvider::HttpConnection("c", "d", sprintf("http://127.0.0.1:%d", port),
        {"monitor": False}), True),
    "rest": (new RestClient::RestConnection("c", "d", sprintf("http://127.0.0.1:%d", port), {"monitor": False}),
        True),
    "smtp": (new SmtpClient::SmtpConnection("c", "d", sprintf("smtp://127.0.0.1:%d", port), {"monitor": False}),
        False),
};
CapRegistry::register(lent);
code make_in_grant = SandboxManager::createSystemPolicyCall(\CapRegistry::make());
map printf("%s=%y\n", $1.key, $1.value), p.callFunction("run", lent, \lent.probe(), lent_connections,
    make_in_grant, new CapProbe::DtorProbe()).pairIterator();
# a sandboxed Program (its sandbox allows the network) with the same restrictions connects through the lent
# connection: its sandbox still governs the connection, but its parse options are what the lent object lends
{
    SandboxManager sm();
    sm.setNetworkDefaultPolicy(True);
    Program s(po);
    s.setSandboxManager(sm);
    s.parse("%modern
string sub connect(object c) {
    try {
        c.get(True);
        return \"connected\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
", "sandboxed");
    printf("sandboxed_lent_connect=%y\n", s.callFunction("connect", lent_connections.http[0]));
}
# the restricted code's top-level code calls the lent connection in every execution mode: JIT-compiled code calls the
# methods of objects without a creation context directly, which must not bypass the capability boundary (nor the
# restrictions of the restricted code's own objects)
# also as a sandboxed Program, whose module calls the JIT and IR fast paths set the caller's restrictions for after they
# entered the method's Program
foreach hash<auto> i in ({"ast": EM_AST, "ir": EM_IR, "jit": EM_JIT, "tiered": EM_TIERED, "sbast": EM_AST,
        "sbir": EM_IR, "sbjit": EM_JIT, "sbtiered": EM_TIERED}.pairIterator()) {
    Program q(po);
    if (i.key =~ /^sb/) {
        SandboxManager qsm();
        qsm.setNetworkDefaultPolicy(True);
        q.setSandboxManager(qsm);
    }
    q.setExecMode(i.value);
    q.parse("%modern
%requires CapProbe
our object lent;
our object inh;
our hash<auto> result;
result.lent_get = lent.get();
result.lent_probe = lent.probe();
# a factory of the lent connection: only an object of a class of the lent method's module is lent
result.factory_other_probe = lent.make(\"CapGadget::Gadget\").probe();
result.factory_own_get = lent.make(\"CapProbe::Conn\").get();
# a lent object of a class of another module that inherits CapProbe's: an inherited method lends the classes of the
# module of its code (CapProbe), the subclass's own method those of its module
result.inherited_base_get = inh.make(\"CapProbe::Conn\").get();
result.inherited_own_probe = inh.makeOwn().probe();
# CapProbe's method calls the subclass's override from module code: the object is created in CapProbe's lent method
# and is not of CapProbe's classes, so it is not lent
result.inherited_override_probe = inh.makeHooked().probe();
CapProbe::Conn own();
result.own_get = own.get();
", "top-level-" + i.key);
    q.setGlobalVarValue("lent", lent);
    q.setGlobalVarValue("inh", new CapInherit::Sub());
    q.run();
    hash<auto> res = q.getGlobalVariable("result");
    map printf("top_%s_%s=%y\n", i.key, $1.key, $1.value), res.pairIterator();
}
listener.close();
