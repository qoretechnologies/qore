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

    #! returns a closure of a trusted class that reports whether it, and a thread that it starts, can share objects
    static code getThreadCheck() {
        return list<bool> sub () {
            Counter done(1);
            bool in_thread;
            background sub () {
                on_exit done.dec();
                in_thread = CppApiTest::SharedObjectProbe::canShareObjects();
            }();
            done.waitForZero();
            return (CppApiTest::SharedObjectProbe::canShareObjects(), in_thread);
        };
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

    #! runs a callback of its caller in a Qore update scope of this object
    auto callBackInScope(code cb) {
        return SandboxManager::updateSharedObject(self, auto sub () { return cb(); });
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
        \"its own code changes the value in the scope\": auto sub (CppApiTest::SharedObjectProbe p) {
            return p.update(auto sub () { return try_desc(auto sub () { p.set(\"tenant\"); }); });
        },
        \"the scope ends with the update\": auto sub (CppApiTest::SharedObjectProbe p) {
            p.update();
            return CppApiTest::SharedObjectProbe::canShareObjects();
        },
        \"a thread that a closure of a trusted class starts in the scope\": auto sub (CppApiTest::SharedObjectProbe p) {
            return p.update(TrustedHelper::getThreadCheck());
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

string sub update_open(CppApiTest::SharedObjectProbe p, string path) {
    return try_desc(auto sub () { return p.update(NOTHING, False, False, path); });
}

string sub update_check_own(CppApiTest::SharedObjectProbe p) {
    return try_desc(auto sub () { return p.update(auto sub () { return \"tenant\"; }, False, False, NOTHING, True); });
}

string sub update_check_trusted(CppApiTest::SharedObjectProbe p) {
    return try_desc(auto sub () { return p.update(TrustedHelper::getClosure(), False, False, NOTHING, True); });
}

string sub nested_in_qore_scope(HostObject o, CppApiTest::SharedObjectProbe p) {
    return try_desc(auto sub () { return o.callBackInScope(auto sub () { return try_desc(auto sub () {
        return p.update(); }); }); });
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
    printf("a file that the update opens: %y\n", t.update(NOTHING, False, False, get_script_path()));
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
    printf("trusted code updates a probe of the tenant that it marked: %s\n", try_desc(auto sub () {
        return t.update(); }));
}
{
    HostObject o();
    CppApiTest::SharedObjectProbe::share(o);
    printf("change a member of an object marked from C++: %s, %y\n", p.callFunction("write_member", o), o.v);
    printf("update an object of a Qore class from C++: %s\n", p.callFunction("update_object", o));
    printf("its own code changes the object in the update scope: %s, %y\n", p.callFunction("update_object_own", o),
        o.v);
    # a hazard that the API documents: code of a Program that no sandbox governs that the update calls has the
    # rights of the scope, whoever supplied it; a module never calls code that its caller gives in a scope
    printf("(hazard) a closure of a trusted class that the tenant gives changes the object in the scope: %s, %y\n",
        p.callFunction("update_object_class", o), o.v);
}
{
    # the update is trusted module code, but the tenant is still the caller that it serves: its sandbox (which denies
    # every path by default) governs what the update does for it, and the update cannot run the tenant's code
    CppApiTest::SharedObjectProbe t(1);
    printf("a file that the update opens for the tenant: %s\n", p.callFunction("update_open", t, get_script_path()));
    printf("the update checks a callback of the tenant: %s\n", p.callFunction("update_check_own", t));
    printf("the update checks a closure of a trusted class that the tenant gives: %s\n",
        p.callFunction("update_check_trusted", t));
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
    printf("its own code opens a C++ scope in a Qore update scope: %s\n", p.callFunction("nested_in_qore_scope", o,
        t));
}

# the code of a trusted (governed) sandbox, one that allows uncontrolled APIs, is sandboxed but not restricted: it
# creates, shares and changes shared objects as trusted code does, while its sandbox's policies still apply (qore#7737)
printf("--- a tenant of a trusted (governed) sandbox\n");
Program g(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
SandboxManager gsm();
gsm.setUncontrolledApiPolicy(True);
g.setSandboxManager(gsm);
g.parse(TenantCode, "governed tenant");
foreach string name in ("creates shared objects", "can share objects", "a probe that it creates is shared",
        "it marks a probe that it creates") {
    printf("%s: %s\n", name, g.callFunction("run_check", name));
}
foreach string name in ("set the value", "mark it", "its own code in the scope", "run its own code locked") {
    CppApiTest::SharedObjectProbe shared(1);
    printf("%s: shared: %s\n", name, g.callFunction("run_probe_check", name, shared));
}
{
    CppApiTest::SharedObjectProbe t(1);
    printf("a file that the update opens for it: %s\n", g.callFunction("update_open", t, get_script_path()));
    printf("the update checks a callback of it: %s\n", g.callFunction("update_check_own", t));
}
{
    # a probe of the governed tenant is a probe of a Program that is not restricted: its scope can be opened
    CppApiTest::SharedObjectProbe t = g.callFunction("create_probe");
    CppApiTest::SharedObjectProbe::share(t);
    printf("it updates a probe of its own that is shared: %s\n", g.callFunction("update", t));
    printf("trusted code updates a shared probe of it: %s\n", try_desc(auto sub () { return t.update(); }));
}
# the classification is decided at each call: a manager that no longer allows uncontrolled APIs restricts its code
gsm.setUncontrolledApiPolicy(False);
printf("--- the same tenant after its sandbox refuses uncontrolled APIs\n");
foreach string name in ("creates shared objects", "can share objects", "it marks a probe that it creates") {
    printf("%s: %s\n", name, g.callFunction("run_check", name));
}
{
    CppApiTest::SharedObjectProbe shared(1);
    printf("set the value: shared: %s\n", g.callFunction("run_probe_check", "set the value", shared));
    printf("build the cache on first use: shared: %s\n", g.callFunction("run_probe_check",
        "build the cache on first use", shared));
}
