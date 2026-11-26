# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The C++ API of shared objects (qore#7669): the builtin class CppApiTest::SharedObjectProbe of the cppapiuser test
# module calls qore_caller_creates_shared_objects(), qore_caller_can_share_objects(), qore_share_object(),
# qore_object_is_shared(), qore_check_shared_object_change(), qore_check_shared_object_callback() and
# qore_update_shared_object() from C++, for trusted code and for a tenant (a sandboxed Program); see
# shared-object-cpp-api.qtest

%modern
%enable-all-warnings
%no-child-restrictions

# the "cppapiuser" test module is built into the build tree by examples/test/module-cpp-api/CMakeLists.txt and is not
# installed, so QORE_MODULE_DIR must include the build tree (run_tests.sh does this)
%requires cppapiuser

#! a trusted class of the host
public class TrustedHelper {
    #! returns a closure of a trusted class
    static code getClosure() {
        return string sub () { return "trusted closure"; };
    }

    #! opens a C++ update scope with a closure of this class that rebuilds another probe from C++
    static hash<auto> nestedTrusted(CppApiTest::SharedObjectProbe p, CppApiTest::SharedObjectProbe other) {
        return p.update(hash<auto> sub () { return other.update(); });
    }
}

#! a class of the host whose objects are shared with the C++ API or with SandboxManager::shareObject()
public class HostObject {
    public {
        string v = "orig";
    }

    #! returns a closure of this class that changes the object
    code getSetter() {
        return auto sub () { v = "set by a closure of the class"; return v; };
    }

    #! builds a probe's cache from C++ in a Qore update scope of this object
    string buildInScope(CppApiTest::SharedObjectProbe p) {
        return SandboxManager::updateSharedObject(self, string sub () {
            v = "in scope";
            return p.build();
        });
    }
}

# the code of a tenant: a sandboxed Program
const TenantCode = "
%modern

string sub try_it(code c) {
    try {
        auto v = c();
        return sprintf(\"ok: %y\", v);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub try_desc(code c) {
    try {
        auto v = c();
        return sprintf(\"ok: %y\", v);
    } catch (hash<ExceptionInfo> ex) {
        return sprintf(\"%s: %s\", ex.err, ex.desc);
    }
}

hash<string, code> sub checks() {
    return {
        \"creates shared objects\": auto sub () { return CppApiTest::SharedObjectProbe::createsSharedObjects(); },
        \"can share objects\": auto sub () { return CppApiTest::SharedObjectProbe::canShareObjects(); },
        \"a probe that it creates is shared\": auto sub () {
            return CppApiTest::SharedObjectProbe::isShared(new CppApiTest::SharedObjectProbe());
        },
        \"it marks a probe that it creates\": auto sub () { new CppApiTest::SharedObjectProbe(1); },
        \"it marks an object that it creates\": auto sub () {
            CppApiTest::SharedObjectProbe::share(new CppApiTest::SharedObjectProbe(0));
        },
        \"a null object\": auto sub () { return CppApiTest::SharedObjectProbe::nullArgs(); },
    };
}

hash<string, code> sub probe_checks() {
    return {
        \"set the value\": auto sub (CppApiTest::SharedObjectProbe p) { p.set(\"tenant\"); return p.get(); },
        \"check a change\": auto sub (CppApiTest::SharedObjectProbe p) {
            CppApiTest::SharedObjectProbe::checkChange(p, \"set the state\");
        },
        \"mark it\": auto sub (CppApiTest::SharedObjectProbe p) { CppApiTest::SharedObjectProbe::share(p); },
        \"build the cache on first use\": auto sub (CppApiTest::SharedObjectProbe p) {
            return (p.build(), p.build(), p.updates());
        },
        \"update and share an object in the scope\": auto sub (CppApiTest::SharedObjectProbe p) {
            return p.update(NOTHING, True);
        },
        \"its own code in the scope\": auto sub (CppApiTest::SharedObjectProbe p) {
            return p.update(auto sub () {
                return (CppApiTest::SharedObjectProbe::canShareObjects(),
                    try_it(auto sub () { CppApiTest::SharedObjectProbe::share(new CppApiTest::SharedObjectProbe(0)); }));
            });
        },
        \"its own code opens a nested scope\": auto sub (CppApiTest::SharedObjectProbe p) {
            return p.update(auto sub () {
                return try_desc(auto sub () { return p.update(); });
            });
        },
        \"a C++ update that throws\": auto sub (CppApiTest::SharedObjectProbe p) {
            return (try_desc(auto sub () { p.update(NOTHING, False, True); }),
                CppApiTest::SharedObjectProbe::canShareObjects(), p.updates());
        },
        \"run its own code locked\": auto sub (CppApiTest::SharedObjectProbe p) {
            return p.runLocked(auto sub () { return \"tenant closure\"; });
        },
        \"run a closure of a trusted class locked\": auto sub (CppApiTest::SharedObjectProbe p) {
            return p.runLocked(TrustedHelper::getClosure());
        },
        \"check no callback\": auto sub (CppApiTest::SharedObjectProbe p) {
            CppApiTest::SharedObjectProbe::checkCallback(p, NOTHING, \"run code\");
        },
    };
}

list<string> sub check_names() {
    return keys checks();
}

list<string> sub probe_check_names() {
    return keys probe_checks();
}

string sub run_check(string name) {
    return try_desc(checks(){name});
}

string sub run_probe_check(string name, CppApiTest::SharedObjectProbe p) {
    return try_desc(auto sub () { return probe_checks(){name}(p); });
}

CppApiTest::SharedObjectProbe sub create_probe() {
    return new CppApiTest::SharedObjectProbe(0);
}

string sub update(CppApiTest::SharedObjectProbe p) {
    return try_desc(auto sub () { return p.update(); });
}

string sub update_object(object o, *code inner) {
    return try_desc(auto sub () { return CppApiTest::SharedObjectProbe::updateObject(o, inner); });
}

string sub update_object_own(HostObject o) {
    return update_object(o, auto sub () { o.v = \"tenant\"; return o.v; });
}

string sub update_object_class(HostObject o) {
    return update_object(o, o.getSetter());
}

string sub write_member(HostObject o) {
    return try_it(auto sub () { o.v = \"tenant\"; return o.v; });
}

string sub nested_trusted(CppApiTest::SharedObjectProbe p, CppApiTest::SharedObjectProbe other) {
    return try_desc(auto sub () { return TrustedHelper::nestedTrusted(p, other); });
}

string sub build_in_scope(HostObject o, CppApiTest::SharedObjectProbe p) {
    return try_desc(auto sub () { return (o.buildInScope(p), o.v); });
}
";

Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
p.setSandboxManager(new SandboxManager());
p.parse(TenantCode, "tenant");

string sub try_desc(code c) {
    try {
        auto v = c();
        return sprintf("ok: %y", v);
    } catch (hash<ExceptionInfo> ex) {
        return sprintf("%s: %s", ex.err, ex.desc);
    }
}

printf("--- trusted code\n");
printf("creates shared objects: %y, can share objects: %y\n", CppApiTest::SharedObjectProbe::createsSharedObjects(),
    CppApiTest::SharedObjectProbe::canShareObjects());
{
    CppApiTest::SharedObjectProbe t();
    printf("a probe that it creates: shared: %y (SandboxManager: %y), has a Program: %y\n",
        CppApiTest::SharedObjectProbe::isShared(t), SandboxManager::isSharedObject(t), t.hasProgram());
    t.set("trusted");
    printf("set the value of a shared probe: %y\n", t.get());
    CppApiTest::SharedObjectProbe::checkChange(t, "set the state");
    printf("build the cache of a shared probe: %y %y, updates: %d\n", t.build(), t.build(), t.updates());
    printf("update and share an object in the scope: %y\n", t.update(NOTHING, True));
    printf("a C++ update that throws: %s, can share objects: %y\n", try_desc(auto sub () { t.update(NOTHING, False, True); }),
        CppApiTest::SharedObjectProbe::canShareObjects());
    printf("run its own code locked: %y\n", t.runLocked(auto sub () { return "trusted code"; }));
    CppApiTest::SharedObjectProbe::checkCallback(t, NOTHING, "run code");
    printf("check no callback: ok\n");
}
{
    CppApiTest::SharedObjectProbe t(0);
    printf("a probe that it does not mark: shared: %y, update: %s\n", CppApiTest::SharedObjectProbe::isShared(t),
        try_desc(auto sub () { return t.update(); }));
    printf("build the cache of a probe that is not shared: %y, updates: %d\n", t.build(), t.updates());
}
{
    HostObject o();
    CppApiTest::SharedObjectProbe::share(o);
    printf("an object of a Qore class marked from C++: SandboxManager: %y; update: %s\n",
        SandboxManager::isSharedObject(o), try_desc(auto sub () {
            return CppApiTest::SharedObjectProbe::updateObject(o, o.getSetter());
        }));
    HostObject o2();
    SandboxManager::shareObject(o2);
    printf("an object marked with SandboxManager::shareObject(): C++: %y\n", CppApiTest::SharedObjectProbe::isShared(o2));
}
{
    CppApiTest::SharedObjectProbe u = CppApiTest::SharedObjectProbe::createUnowned(True);
    printf("a probe with no Program: shared: %y, has a Program: %y, update: %s\n",
        CppApiTest::SharedObjectProbe::isShared(u), u.hasProgram(), try_desc(auto sub () { return u.update(); }));
    CppApiTest::SharedObjectProbe n = CppApiTest::SharedObjectProbe::createUnowned(False);
    printf("a probe with no Program that is not shared: update: %s\n", try_desc(auto sub () { return n.update(); }));
}
printf("null pointers: %y\n", CppApiTest::SharedObjectProbe::nullArgs());

printf("--- a tenant\n");
foreach string name in (p.callFunction("check_names")) {
    printf("%s: %s\n", name, p.callFunction("run_check", name));
}
foreach string name in (p.callFunction("probe_check_names")) {
    CppApiTest::SharedObjectProbe shared(1);
    CppApiTest::SharedObjectProbe plain(0);
    printf("%s: shared: %s; not shared: %s\n", name, p.callFunction("run_probe_check", name, shared),
        p.callFunction("run_probe_check", name, plain));
}
{
    CppApiTest::SharedObjectProbe u = CppApiTest::SharedObjectProbe::createUnowned(True);
    printf("update a shared probe with no Program: %s\n", p.callFunction("update", u));
}
{
    CppApiTest::SharedObjectProbe t = p.callFunction("create_probe");
    CppApiTest::SharedObjectProbe::share(t);
    printf("update a probe of the tenant that trusted code marked: %s\n", p.callFunction("update", t));
}
{
    HostObject o();
    CppApiTest::SharedObjectProbe::share(o);
    printf("change a member of an object marked from C++: %s, %y\n", p.callFunction("write_member", o), o.v);
    printf("update an object of a Qore class from C++: %s\n", p.callFunction("update_object", o));
    printf("its own code changes the object in the update scope: %s, %y\n", p.callFunction("update_object_own", o),
        o.v);
    printf("a closure of the class changes the object in the update scope: %s, %y\n",
        p.callFunction("update_object_class", o), o.v);
}
{
    CppApiTest::SharedObjectProbe t(1);
    CppApiTest::SharedObjectProbe other(1);
    printf("a closure of a trusted class opens a nested C++ scope: %s\n", p.callFunction("nested_trusted", t, other));
}
{
    HostObject o();
    SandboxManager::shareObject(o);
    CppApiTest::SharedObjectProbe t(1);
    printf("a C++ scope in a Qore update scope: %s\n", p.callFunction("build_in_scope", o, t));
}
