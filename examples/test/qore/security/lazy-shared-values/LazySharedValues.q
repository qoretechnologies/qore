# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# Process-wide values that a library module builds lazily with LazySharedMap and LazySharedValue are built as trusted
# code for whichever caller needs them first: what the builder creates is marked as shared and takes neither the
# sandbox nor the restrictions of that caller, unlike the plain lazy pattern; the builds are single-flight, a failed
# build leaves nothing behind, and only trusted code can create the objects (qore#7684); see lazy-shared-values.qtest

%modern
%enable-all-warnings
%no-child-restrictions
%append-module-path "${SCRIPT_DIR}/modules"

%requires LazySharedProbe

# the code of a tenant (a sandboxed Program) and of a restricted Program
const CallerCode = "
%modern
%requires LazySharedProbe

# the shared state of the value of a key, or the exception
string sub value_of(string key) {
    try {
        hash<auto> v = LazySharedProbe::Probe::get(key);
        return sprintf(\"obj shared: %y, built as trusted: %y\", SandboxManager::isSharedObject(v.obj),
            v.creates_shared);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub legacy_value_of(string key) {
    try {
        hash<auto> v = LazySharedProbe::Probe::getLegacy(key);
        return sprintf(\"obj shared: %y, built as trusted: %y\", SandboxManager::isSharedObject(v.obj),
            v.creates_shared);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub single_value() {
    hash<auto> v = LazySharedProbe::Probe::getSingle();
    return sprintf(\"obj shared: %y, built as trusted: %y\", SandboxManager::isSharedObject(v.obj), v.creates_shared);
}

# the exception of a call, or the result
string sub try_it(code c) {
    try {
        return sprintf(\"%y\", c());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub get_key(string key, timeout timeout_ms = 0) {
    return try_it(auto sub () { return LazySharedProbe::Probe::get(key, timeout_ms).obj.name; });
}

string sub change_value(string key) {
    return try_it(auto sub () { hash<auto> v = LazySharedProbe::Probe::get(key); v.obj.name = \"changed\";
        return \"changed\"; });
}

string sub create_value() {
    return try_it(auto sub () {
        LazySharedValue v(int sub () { return 1; });
        return sprintf(\"process-shared: %y, value: %y, the object is shared: %y\", v.isProcessShared(), v.get(),
            SandboxManager::isSharedObject(v));
    });
}

string sub create_map() {
    return try_it(auto sub () {
        LazySharedMap m(string sub (string k) { if (k == \"bad\") { throw \"BAD-KEY\"; } return k; });
        string bad;
        try {
            m.get(\"bad\");
        } catch (hash<ExceptionInfo> ex) {
            bad = ex.err;
        }
        return sprintf(\"process-shared: %y, value: %y, a failed key: %s, entries: %d\", m.isProcessShared(),
            m.get(\"k\"), bad, m.size());
    });
}

string sub call_deputy(code deputy) {
    return try_it(deputy);
}

# whether the restricted code is restricted in a method of the object of a value: it is not if the object lends it
string sub object_lends(string key, bool legacy) {
    return try_it(auto sub () {
        hash<auto> v = legacy ? LazySharedProbe::Probe::getLegacy(key) : LazySharedProbe::Probe::get(key);
        return v.obj.callerRestricted() ? \"not lent\" : \"lent\";
    });
}

bool sub restricted_self() {
    return SandboxManager::isCallerRestricted(DOM_FILESYSTEM);
}

string sub get_limited(string key) {
    return try_it(auto sub () { return LazySharedProbe::Probe::getLimited(key).obj.name; });
}

code sub tenant_builder() {
    return int sub () { return 1; };
}

nothing sub give_object() {
    LazySharedProbe::Probe::setTenantObject(new LazySharedProbe::Built(\"tenant\"));
}
";

const TenantOnlyCode = "
# a private value of the tenant: its builder tries a file that the tenant's sandbox does not let it read
LazySharedValue sub private_value() {
    return new LazySharedValue(hash<auto> sub () {
        string fs;
        try {
            ReadOnlyFile::readTextFile(\"/etc/passwd\");
            fs = \"read\";
        } catch (hash<ExceptionInfo> ex) {
            fs = ex.err;
        }
        return {
            \"obj\": new LazySharedProbe::Built(\"private\"),
            \"sandboxed\": SandboxManager::isSandboxed(),
            \"fs\": fs,
        };
    });
}

string sub create_lazily(bool require_shared) {
    return LazySharedProbe::Probe::createLazily(require_shared);
}

string sub describe_private(LazySharedValue v) {
    hash<auto> r = v.get();
    return sprintf(\"obj shared: %y, sandboxed: %y, the file: %s\", SandboxManager::isSharedObject(r.obj),
        r.sandboxed, r.fs);
}

";

Program tenant(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
tenant.setSandboxManager(new SandboxManager());
tenant.parse(CallerCode + TenantOnlyCode, "tenant");

# restricted by a parse option, with no sandbox
Program restricted(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION | PO_NO_FILESYSTEM);
restricted.parse(CallerCode, "restricted");

string sub describe(hash<auto> v) {
    return sprintf("obj shared: %y, built as trusted: %y, its code restricted for an unrestricted caller: %y",
        SandboxManager::isSharedObject(v.obj), v.creates_shared, v.closure());
}

# 1: only trusted code creates process-shared objects; other code creates private ones
printf("tenant creates a LazySharedValue: %s; a LazySharedMap: %s\n", tenant.callFunction("create_value"),
    tenant.callFunction("create_map"));
code deputy = string sub () {
    LazySharedValue v(int sub () { return 1; });
    return sprintf("process-shared: %y", v.isProcessShared());
};
printf("trusted deputy creates one for the tenant: %s\n", tenant.callFunction("call_deputy", deputy));
printf("trusted code creates one with the tenant's builder: %s\n", (string sub () {
    try {
        LazySharedValue v(tenant.callFunction("tenant_builder"));
        return sprintf("process-shared: %y", v.isProcessShared());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
})());

{
    LazySharedValue v(int sub () { return 1; });
    printf("trusted code creates one: process-shared: %y, the object is shared: %y\n", v.isProcessShared(),
        SandboxManager::isSharedObject(v));
}
printf("a bound callback as the builder: %s\n", (string sub () {
    try {
        code cap = SandboxManager::createTrustedBuildCall(int sub () { return 1; });
        LazySharedValue v(cap);
        return "created";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
})());
# a private value's builder runs as the tenant's code, also when trusted code needs the value first
{
    LazySharedValue v = tenant.callFunction("private_value");
    printf("tenant's private value: process-shared: %y; the tenant gets it first: %s\n", v.isProcessShared(),
        tenant.callFunction("describe_private", v));
    LazySharedValue v2 = tenant.callFunction("private_value");
    printf("trusted code gets a private value first: %s\n", (string sub () {
        hash<auto> r = v2.get();
        return sprintf("obj shared: %y, sandboxed: %y, the file: %s", SandboxManager::isSharedObject(r.obj),
            r.sandboxed, r.fs);
    })());
}

# library code that creates a lazy value for whichever caller needs it first: private for a tenant, unless the
# library requires a process-shared value
printf("library code creates a lazy value for the tenant: %s; with require_shared: %s; for trusted code with "
    "require_shared: %s\n", tenant.callFunction("create_lazily", False), tenant.callFunction("create_lazily", True),
    LazySharedProbe::Probe::createLazily(True));

# 2: a sandboxed first caller
printf("tenant first caller: %s; plain lazy pattern: %s\n", tenant.callFunction("value_of", "tenant"),
    tenant.callFunction("legacy_value_of", "tenant"));
printf("tenant changes the value: %s\n", tenant.callFunction("change_value", "tenant"));
printf("trusted code gets the tenant-built value: %s\n", describe(LazySharedProbe::Probe::get("tenant")));
printf("tenant first caller of a LazySharedValue: %s, built: %y\n", tenant.callFunction("single_value"),
    LazySharedProbe::Probe::isSingleBuilt());

# 3: a restricted, unsandboxed first caller; the code in the value runs later for an unrestricted caller
printf("restricted first caller: %s; plain lazy pattern: %s\n", restricted.callFunction("value_of", "restricted"),
    restricted.callFunction("legacy_value_of", "restricted"));
printf("its closure for an unrestricted caller: restricted: %y; plain lazy pattern: %y\n",
    LazySharedProbe::Probe::get("restricted").closure(), LazySharedProbe::Probe::getLegacy("restricted").closure());
printf("the restricted caller is still restricted itself: %y\n", restricted.callFunction("restricted_self"));
printf("trusted first caller: %s; plain lazy pattern: %s\n", describe(LazySharedProbe::Probe::get("trusted")),
    describe(LazySharedProbe::Probe::getLegacy("trusted")));
printf("the file system for a restricted caller in a method of its object: %s; plain lazy pattern: %s\n",
    restricted.callFunction("object_lends", "trusted", False),
    restricted.callFunction("object_lends", "trusted", True));
{
    hash<auto> r = LazySharedProbe::Probe::getInModuleThread("module-thread");
    printf("first caller with nothing for the barrier to hide (%y): %s; its object for a restricted caller: %s\n",
        r.hides_nothing, describe(r.value), restricted.callFunction("object_lends", "module-thread", False));
}
code deputy_get = string sub () {
    return describe(LazySharedProbe::Probe::get("deputy")) + "; plain lazy pattern: "
        + describe(LazySharedProbe::Probe::getLegacy("deputy"));
};
printf("trusted deputy first caller for the tenant: %s\n", tenant.callFunction("call_deputy", deputy_get));
printf("first caller with a hidden sandboxed caller record: %s\n",
    call_function("dbg_call_with_sandboxed_caller_record", tenant,
        string sub () {
            return describe(LazySharedProbe::Probe::get("hidden")) + "; plain lazy pattern: "
                + describe(LazySharedProbe::Probe::getLegacy("hidden"));
        }));
printf("the values for an unrestricted caller: tenant: %s; restricted: %s\n",
    describe(LazySharedProbe::Probe::get("tenant")), describe(LazySharedProbe::Probe::get("restricted")));

# 4: several first callers: one build, one value
{
    Counter done(3);
    list<string> results = ();
    Mutex lck();
    background (sub () {
        on_exit done.dec();
        string r = tenant.callFunction("get_key", "race");
        lck.lock(); results += r; lck.unlock();
    })();
    LazySharedProbe::Probe::waitRaceStarted();
    int t2 = background (sub () {
        on_exit done.dec();
        string r = tenant.callFunction("get_key", "race");
        lck.lock(); results += r; lck.unlock();
    })();
    # a timeout of 0 or less is no limit
    int t3 = background (sub () {
        on_exit done.dec();
        string r = tenant.callFunction("get_key", "race", -1);
        lck.lock(); results += r; lck.unlock();
    })();
    # both wait for the build before it ends
    printf("the other first callers wait: %y\n", (call_function("dbg_lazy_shared_wait_waiting", t2, 30000),
        call_function("dbg_lazy_shared_wait_waiting", t3, 30000)));
    # a caller with a timeout gives up while the build runs
    printf("a caller with a timeout while another builds: %s\n", tenant.callFunction("get_key", "race", 1ms));
    LazySharedProbe::Probe::releaseRace();
    done.waitForZero();
    printf("race: builds: %d, results: %y\n", LazySharedProbe::Probe::builds("race"), sort(results));
}

# 5: failures leave no entry
{
    int before = LazySharedProbe::Probe::size();
    string first = tenant.callFunction("get_key", "fail-once");
    int after_failure = LazySharedProbe::Probe::size() - before;
    string second = tenant.callFunction("get_key", "fail-once");
    printf("a build that fails: %s, new entries: %d; the next caller: %s, new entries: %d\n", first, after_failure,
        second, LazySharedProbe::Probe::size() - before);
}
{
    int before = LazySharedProbe::Probe::size();
    list<string> errs = map tenant.callFunction("get_key", "unknown-" + $1), xrange(50);
    printf("tenant asks for 50 unknown keys: errors: %y, new entries: %d\n", (map {$1: True}, errs).keys(),
        LazySharedProbe::Probe::size() - before);
}
printf("a builder that needs its own value: %s, built: %y\n", tenant.callFunction("get_key", "self"),
    LazySharedProbe::Probe::isBuilt("self"));
tenant.callFunction("give_object");
printf("a builder that returns an object that the tenant created: %s, built: %y\n",
    tenant.callFunction("get_key", "tenant-object"), LazySharedProbe::Probe::isBuilt("tenant-object"));
# a cycle of waits of two builders: raised instead of a deadlock, and nothing is left behind
{
    int before = LazySharedProbe::Probe::size();
    Counter done(2);
    list<string> results = ();
    Mutex lck();
    background (sub () {
        on_exit done.dec();
        string r = tenant.callFunction("get_key", "cycA");
        lck.lock(); results += "cycA: " + r; lck.unlock();
    })();
    LazySharedProbe::Probe::waitCycAStarted();
    int t2 = background (sub () {
        on_exit done.dec();
        string r = tenant.callFunction("get_key", "cycB");
        lck.lock(); results += "cycB: " + r; lck.unlock();
    })();
    # the builder of cycB waits for cycA, which the first thread builds
    bool waiting = call_function("dbg_lazy_shared_wait_waiting", t2, 30000);
    LazySharedProbe::Probe::releaseCycA();
    done.waitForZero();
    printf("cycle: the second builder waited: %y, results: %y, new entries: %d\n", waiting, sort(results),
        LazySharedProbe::Probe::size() - before);
}
printf("a map limited to one entry: %y, then %s, entries: %d\n", LazySharedProbe::Probe::getLimited("trusted").obj.name,
    tenant.callFunction("get_limited", "race"), LazySharedProbe::Probe::limitedSize());
