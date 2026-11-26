# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A loader that sandboxed code gives to a lookup of the process-wide data provider type cache or connection scheme
# registry runs without the lock of the cache, so that it cannot block the other callers (issue #7679); see
# shared-cache-loaders.qtest

%modern
%enable-all-warnings
%no-child-restrictions

%requires DataProvider
%requires ConnectionProvider

# how long the host waits for a lookup while the tenant's loader runs; a lookup that does not wait for the loader
# completes well within it
const LookupWait = 5s;

# the code of a tenant (a sandboxed Program)
const TenantCode = "
%modern
%requires DataProvider
%requires ConnectionProvider

# a type lookup with a loader that waits until the host releases it
*string sub type_lookup_with_waiting_loader(Queue entered, Counter release) {
    *AbstractDataProviderType t = DataProvider::getTypeCache().getTypeImpl(\"test-7679/missing/type\", False,
        sub (list<string> path) {
            entered.push(True);
            release.waitForZero();
        });
    return t ? t.getName() : NOTHING;
}

# a scheme lookup with a loader that waits until the host releases it
*string sub scheme_lookup_with_waiting_loader(Queue entered, Counter release) {
    *hash<ConnectionSchemeInfo> info = ConnectionSchemeCache::getSchemeImpl(\"test-7679-missing\", False,
        sub (string scheme) {
            entered.push(True);
            release.waitForZero();
        });
    return info ? info.cls.getName() : NOTHING;
}

# a type that the module loads for the tenant on demand, and an unknown type
string sub lazy_type() {
    *AbstractDataProviderType t = DataProvider::getType(\"qore/ftp/event\");
    return sprintf(\"%y, unknown: %y\", t ? t.getName() : NOTHING, DataProvider::getType(\"test-7679/unknown\"));
}

# a loader of the tenant that returns an entry of its own: the result is the tenant's, the cache is unchanged
*string sub own_loader_result() {
    *AbstractDataProviderType t = DataProvider::getTypeCache().getTypeImpl(\"test-7679/own\", False,
        *DataProviderTypeEntry sub (list<string> path) {
            DataProviderTypeEntry root();
            root.addChild(\"own\");
            root.getChild(\"own\").setType(new HashDataType(\"own\"));
            return root.getChild(\"own\");
        });
    return t ? t.getName() : NOTHING;
}
";

Program tenant(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
tenant.setSandboxManager(new SandboxManager());
tenant.parse(TenantCode, "tenant");

# runs a lookup of the tenant whose loader waits, and a lookup of the host while it waits
string sub check(string tenant_func, code host_lookup) {
    Queue entered();
    Counter release(1);
    Queue tenant_result();
    background (sub () {
        try {
            tenant_result.push(tenant.callFunction(tenant_func, entered, release));
        } catch (hash<ExceptionInfo> ex) {
            tenant_result.push(ex.err);
        }
    })();
    entered.get();
    Queue host_result();
    background (sub () {
        try {
            host_result.push(host_lookup());
        } catch (hash<ExceptionInfo> ex) {
            host_result.push(ex.err);
        }
    })();
    string rv;
    try {
        rv = sprintf("the host's lookup: %y", host_result.get(LookupWait));
    } catch (hash<ExceptionInfo> ex) {
        rv = "the host's lookup: BLOCKED";
    }
    release.dec();
    if (rv =~ /BLOCKED/) {
        rv += sprintf(" (then %y)", host_result.get());
    }
    return rv + sprintf(", the tenant's lookup: %y", tenant_result.get());
}

DataProvider::registerType("test-7679/registered", new HashDataType("registered"));
printf("type cache: %s\n", check("type_lookup_with_waiting_loader", string sub () {
    return DataProvider::getType("test-7679/registered").getName();
}));
printf("type cache registration: %s\n", check("type_lookup_with_waiting_loader", string sub () {
    DataProvider::registerType("test-7679/host", new HashDataType("host"));
    return DataProvider::getType("test-7679/host").getName();
}));
printf("scheme registry: %s\n", check("scheme_lookup_with_waiting_loader", string sub () {
    return ConnectionSchemeCache::getScheme("http").cls.getName();
}));
printf("a type that the module loads for the tenant: %s\n", tenant.callFunction("lazy_type"));
printf("the tenant's own loader result: %y, the cache: %y\n", tenant.callFunction("own_loader_result"),
    DataProvider::getType("test-7679/own"));
