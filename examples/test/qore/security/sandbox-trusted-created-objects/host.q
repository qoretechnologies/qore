# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright (C) 2026 Qore Technologies, s.r.o.

# The trusted code of sandbox-trusted-created-objects.qtest (issue #7921): the classes of a trusted host, whose code
# creates objects while it serves a sandboxed caller, and the cases that the test checks.  The test parses this source
# into a trusted Program of each execution mode and compiles it with qcc, with a driver that prints the results.  It
# has no parse directives of its own: the test and the driver give them.

#! the class whose objects the trusted code creates; declared by trusted code, it does not inherit SandboxSerializable
public class ProbeOrder inherits Serializable {
    public {
        string pub = "PUBLIC";
    }

    private {
        string name = "ORDER";
    }

    #! serializes its own object
    binary serializeOrder() {
        return Serializable::serialize(self);
    }

    #! serializes its own object to data
    hash<auto> serializeOrderToData() {
        return Serializable::serializeToData(self);
    }

    #! serializes a copy of its own object
    binary serializeCopy() {
        ProbeOrder c = self.copy();
        return Serializable::serialize(c);
    }

    #! returns what the reads of the whole object show of its non-public member: "private" or "public"
    hash<string, string> readSelf() {
        return {
            "format": sprintf("%y", self) =~ /ORDER/ ? "private" : "public",
            "keys": (keys self).contains("name") ? "private" : "public",
            "size": self.size() == 2 ? "private" : "public",
            "merge": ({} + self).hasKey("name") ? "private" : "public",
        };
    }

    #! reads the non-public member of another object of the class, calls its non-public method and deletes it
    string useOther(ProbeOrder other) {
        string rv = other.name + "/" + other.privName();
        delete other;
        return rv;
    }

    private string privName() {
        return name;
    }
}

#! the trusted API that sandboxed code calls
public class ProbeClient {
    #! makes the object serialize itself
    static binary send(ProbeOrder order) {
        return order.serializeOrder();
    }

    #! creates an object for the sandboxed caller and makes it serialize itself
    static binary makeAndSend() {
        return send(new ProbeOrder());
    }

    #! creates an object for the sandboxed caller and makes it serialize itself to data
    static hash<auto> makeAndSendToData() {
        return (new ProbeOrder()).serializeOrderToData();
    }

    #! creates an object for the sandboxed caller and makes it serialize a copy of itself
    static binary makeAndSendCopy() {
        return (new ProbeOrder()).serializeCopy();
    }

    #! creates an object for the sandboxed caller and makes it read itself as a whole
    static hash<string, string> makeAndReadSelf() {
        return (new ProbeOrder()).readSelf();
    }

    #! creates two objects for the sandboxed caller, the first of which uses the second
    static string makeAndUseOther() {
        return (new ProbeOrder()).useOther(new ProbeOrder());
    }

    #! creates an object for the sandboxed caller, which keeps it
    static ProbeOrder make() {
        return new ProbeOrder();
    }

    #! creates an object for the sandboxed caller and hands it over to it
    static ProbeOrder makeTransferred() {
        ProbeOrder o();
        SandboxManager::transferObjectToCaller(o);
        return o;
    }
}

#! the code of the tenants, which call the trusted API
const ProbeTenantSource = "%modern
binary sub send(object o) { return ProbeClient::send(o); }
binary sub make_and_send() { return ProbeClient::makeAndSend(); }
hash<auto> sub make_and_send_to_data() { return ProbeClient::makeAndSendToData(); }
binary sub make_and_send_copy() { return ProbeClient::makeAndSendCopy(); }
hash<string, string> sub make_and_read_self() { return ProbeClient::makeAndReadSelf(); }
string sub make_and_use_other() { return ProbeClient::makeAndUseOther(); }
object sub make() { return ProbeClient::make(); }
object sub make_transferred() { return ProbeClient::makeTransferred(); }
object sub make_own() { return new ProbeOrder(); }
binary sub serialize_directly(object o) { return Serializable::serialize(o); }
string sub delete_it(object o) {
    try {
        delete o;
        return \"deleted\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
";

#! creates a tenant: a sandboxed Program that the trusted API and its class are imported into
Program sub probe_tenant(*Qore::ExecMode mode) {
    Program p(PO_NEW_STYLE | PO_NO_CHILD_PO_RESTRICTIONS | PO_NO_INHERIT_USER_CLASSES);
    if (exists mode) {
        p.setExecMode(mode);
    }
    p.setSandboxManager(new SandboxManager());
    p.importClass("ProbeClient");
    p.importClass("ProbeOrder");
    p.parse(ProbeTenantSource, "probe-tenant");
    return p;
}

#! returns "PASS" if the serialized data is the object with its non-public member, else the exception
string sub probe_check(code c) {
    try {
        auto v = c();
        auto data = v.typeCode() == NT_BINARY ? Serializable::deserializeToData(v) : v;
        return sprintf("%y", data) =~ /"ORDER"/ ? "PASS" : sprintf("NO-STATE: %y", data);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

#! runs the cases; returns the result of each
hash<string, auto> sub probe_run_cases(*Qore::ExecMode mode) {
    Program a = probe_tenant(mode);
    Program b = probe_tenant(mode);
    hash<string, auto> rv;

    # an object that trusted code created before the sandbox call serializes itself
    rv."created before sandbox" = probe_check(auto sub () { return a.callFunction("send", new ProbeOrder()); });
    # an object that trusted class code created during the sandbox call serializes itself
    rv."created for sandbox call" = probe_check(auto sub () { return a.callFunction("make_and_send"); });
    rv."created for sandbox call to data" = probe_check(auto sub () { return a.callFunction("make_and_send_to_data"); });
    rv."copy of object created for sandbox call" = probe_check(auto sub () {
        return a.callFunction("make_and_send_copy");
    });
    # the reads of the whole object by its own code
    rv."read self" = a.callFunction("make_and_read_self");
    # the member reads, non-public method calls and deletes of the trusted class's code
    try {
        rv."use other" = a.callFunction("make_and_use_other");
    } catch (hash<ExceptionInfo> ex) {
        rv."use other" = ex.err;
    }

    # an object that trusted code created for A and that A keeps, made to serialize itself by A and by B
    object kept = a.callFunction("make");
    rv."kept serialized for A" = probe_check(auto sub () { return a.callFunction("send", kept); });
    rv."kept serialized for B" = probe_check(auto sub () { return b.callFunction("send", kept); });
    # sandboxed code gets no state of the object, nor deletes it, as no sandbox owns it
    rv."kept serialized by A's code" = probe_check(auto sub () { return a.callFunction("serialize_directly", kept); });
    rv."kept deleted by A's code" = a.callFunction("delete_it", kept);

    # an object that A's code created: its own code serializes it for A, not for B
    object own = a.callFunction("make_own");
    rv."A's object serialized for A" = probe_check(auto sub () { return a.callFunction("send", own); });
    rv."A's object serialized for B" = probe_check(auto sub () { return b.callFunction("send", own); });
    rv."A's object deleted by B's code" = b.callFunction("delete_it", own);

    # an object that trusted code handed over to A is A's
    object transferred = a.callFunction("make_transferred");
    rv."transferred serialized for A" = probe_check(auto sub () { return a.callFunction("send", transferred); });
    rv."transferred serialized for B" = probe_check(auto sub () { return b.callFunction("send", transferred); });
    return rv;
}
