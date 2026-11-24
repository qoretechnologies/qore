# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The code of a tenant never writes the caches of a shared REST schema, whatever runs it: constant, static member,
# global variable and member initializers, default arguments of its methods called by the host, closures created in
# its methods of objects that trusted code created, and its callbacks in closures of trusted code; the schema writes
# its caches only in its own type builds, which a tenant cannot run code in (qore#6843); see
# shared-schema-frameless.qtest

%modern
%enable-all-warnings
%no-child-restrictions

%requires reflection
%requires OpenApi3
%requires json

const OpenApi3Doc = {
    "openapi": "3.0.0",
    "info": {"title": "test", "version": "1.0"},
    "servers": ({"url": "http://localhost/api"},),
    "paths": {},
    "components": {"schemas": {
        "Thing": {"type": "object", "properties": {"name": {"type": "string"}}},
    }},
};

OpenApi3Schema shared = OpenApi3Loader::fromString(make_json(OpenApi3Doc), True);
shared.getTypeForReference("Thing");

#! the shared schema, for the code of the tenant
OpenApi3Schema sub get_shared() {
    return shared;
}

#! the results of the tenant's probes, once each, in the order of the first
list<string> results = ();
hash<string, bool> seen = {};
sub record(string result) {
    if (!seen{result}) {
        seen{result} = True;
        results += result;
    }
}

#! trusted code that runs a callback in a closure of its own
public class TrustedRunner {
    static auto run(code callback) {
        code wrapper = auto sub () { return callback(); };
        return wrapper();
    }
}

# the code of a tenant: a sandboxed Program
const TenantCode = "
%modern
%requires OpenApi3

# tries to have the shared schema cache data of its own, in each way that it can
string sub probe(string ctx_name) {
    OpenApi3Schema s = get_shared();
    list<string> l = ();
    try {
        s.getType(\"/frameless/\" + ctx_name, \"string\");
        s.components.schemas.Thing.properties.name.getField(\"frameless-\" + ctx_name, ctx_name);
        l += \"built\";
    } catch (hash<ExceptionInfo> ex) {
        l += ex.err;
    }
    try {
        s.runTypeBuild(sub () { s.getType(\"/frameless-build/\" + ctx_name, \"string\"); });
        l += \"build ran\";
    } catch (hash<ExceptionInfo> ex) {
        l += ex.err;
    }
    record(sprintf(\"%s: %s\", ctx_name, join(\", \", l)));
    return ctx_name;
}

const C = probe(\"a constant initializer\");

class Statics {
    public {
        static string s = probe(\"a static member initializer\");
    }
}

class WithDefault {
    string m(string a = probe(\"a default argument of a method called by the host\")) {
        return a;
    }
}

class WithMember {
    public {
        string v = probe(\"a member initializer of an object that the host creates\");
    }
}

class WithClosure {
    code getClosure() {
        return sub () { probe(\"a closure created in a method of an object that the host created\"); };
    }
}

our string g = probe(\"a global variable initializer\");

# a constant whose initializer runs when the constant is first used, which may be in the host's parse
string sub use_constant() {
    return C;
}

object sub make_with_default() {
    return new WithDefault();
}

object sub make_with_member() {
    return new WithMember();
}

object sub make_with_closure() {
    return new WithClosure();
}

code sub get_callback() {
    return sub () { probe(\"a callback in a closure of trusted code\"); };
}
";

# the size of a private hash member of an object, read with Reflection by trusted test code
int sub member_size(object o, string name) {
    foreach Class c in (Class::getClass(o).getClassHierarchy()) {
        foreach AbstractClassMember m in (c.getMembers()) {
            if (m.getName() == name && m instanceof NormalMember) {
                return cast<NormalMember>(m).getValue(o).size();
            }
        }
    }
    return -1;
}

object name_so = shared.components.schemas.Thing.properties.name;
int types = member_size(shared, "typemap");
int fields = member_size(name_so, "fieldmap");
int uses = member_size(name_so, "usemap");

Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
p.setSandboxManager(new SandboxManager());
p.importFunction("get_shared");
p.importFunction("record");
# constant and static member initializers run in the host's parse
p.parse(TenantCode, "tenant");
# global variable initializers run in the host's run of the Program
p.run();
p.callFunction("use_constant");
# a default argument evaluated when the host calls the method
p.callFunction("make_with_default").m();
# a member initializer of an object that the host creates
{
    Class c = Class::getClass(p.callFunction("make_with_member"));
    c.newObject();
}
# a closure created in a method of an object that the host created
{
    Class c = Class::getClass(p.callFunction("make_with_closure"));
    object o = c.newObject();
    code closure = o.getClosure();
    closure();
}
# a callback of the tenant in a closure of trusted code
TrustedRunner::run(p.callFunction("get_callback"));
# the same in a loop, for the code that the JIT compiles
for (int i = 0; i < 200; ++i) {
    p.callFunction("make_with_default").m();
    TrustedRunner::run(p.callFunction("get_callback"));
}

# in the order of the names: the initializers run in an order that depends on the execution mode
map printf("%s\n", $1), sort(results);
printf("the shared schema's caches are unchanged: type cache %y, field cache %y, use cache %y\n",
    member_size(shared, "typemap") == types, member_size(name_so, "fieldmap") == fields,
    member_size(name_so, "usemap") == uses);
