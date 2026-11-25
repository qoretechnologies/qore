# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The code of a tenant that Python and Java code call back, as a callable that trusted code passes to them, never writes
# the caches of a shared REST schema, nor runs a type build of it (qore#6843); prints only "SKIP" if the python or the jni
# module cannot be loaded; see shared-schema-foreign-callables.qtest

%modern
%enable-all-warnings
%no-child-restrictions

%try-module python
%define NoPython
%endtry

%try-module jni
%define NoJni
%endtry

%ifndef NoJni
%module-cmd(jni) import java.util.Optional
%endif

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

# the code of a tenant: a sandboxed Program
const TenantCode = "
%modern
%requires OpenApi3

# tries to have the shared schema cache data of its own, and to run a type build of it
string sub probe(string ctx_name) {
    OpenApi3Schema s = get_shared();
    list<string> l = ();
    try {
        s.getType(\"/foreign/\" + ctx_name, \"string\");
        s.components.schemas.Thing.properties.name.getField(\"foreign-\" + ctx_name, ctx_name);
        l += \"built\";
    } catch (hash<ExceptionInfo> ex) {
        l += ex.err;
    }
    try {
        s.runTypeBuild(sub () { s.getType(\"/foreign-build/\" + ctx_name, \"string\"); });
        l += \"build ran\";
    } catch (hash<ExceptionInfo> ex) {
        l += ex.err;
    }
    record(sprintf(\"%s: %s\", ctx_name, join(\", \", l)));
    return ctx_name;
}

code sub get_callback(string ctx_name) {
    return auto sub (auto m, auto args) { return probe(ctx_name); };
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

%ifdef NoPython
printf("SKIP\n");
exit(0);
%endif
%ifdef NoJni
printf("SKIP\n");
exit(0);
%endif

object name_so = shared.components.schemas.Thing.properties.name;
int types = member_size(shared, "typemap");
int fields = member_size(name_so, "fieldmap");
int uses = member_size(name_so, "usemap");

Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
p.setSandboxManager(new SandboxManager());
p.importFunction("get_shared");
p.importFunction("record");
p.parse(TenantCode, "tenant");

for (int i = 0; i < 50; ++i) {
%ifndef NoPython
    # a Python callable that calls the tenant's callback
    auto py_call = PythonProgram::evalExpression("lambda f: f()");
    py_call(p.callFunction("get_callback", "a callback that Python code calls"));
%endif
%ifndef NoJni
    # a Java function that calls the tenant's callback
    auto fn = implement_interface(new Jni::org::qore::jni::QoreInvocationHandler(
        p.callFunction("get_callback", "a callback that Java code calls")), load_class("java/util/function/Function"));
    # ("map" is a keyword of Qore)
    auto mapped = call_object_method(Optional::of("a"), "map", fn);
    mapped.get();
%endif
}

map printf("%s\n", $1), sort(results);
printf("the shared schema's caches are unchanged: type cache %y, field cache %y, use cache %y\n",
    member_size(shared, "typemap") == types, member_size(name_so, "fieldmap") == fields,
    member_size(name_so, "usemap") == uses);
