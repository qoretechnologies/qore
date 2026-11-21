# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# Code that the sandboxes of different isolation domains govern together belongs to no isolation domain (qore#7009):
# a method of an object that one tenant's call created runs under that tenant's sandbox as well (its provenance), so
# when another tenant calls it, the sandboxes of both govern it: it gets no tenant's domain, it cannot create a
# sandbox, a Program or a thread, and a trusted build that it enters runs the code of neither tenant; the same tenant,
# and a tenant with a sandbox without a domain, are unaffected.  See sandbox-isolation-composite.qtest

%modern
%enable-all-warnings
%no-child-restrictions

# the code of a tenant: a sandboxed Program
const TenantCode = "%new-style
%require-types
%strict-args

# an object that this tenant's call creates with the given factory
object sub make(code factory) {
    return factory();
}

# a closure of this tenant's code that a trusted build runs
code sub own() {
    return string sub () { return \"ran\"; };
}

# a closure of this tenant's code that creates a sandbox and returns its isolation domain
code sub mk_manager() {
    return string sub () {
        SandboxManager m();
        return m.getIsolationDomain() ?? \"none\";
    };
}

# calls the checks of an object
string sub run(object o, code build_domain, code build_call, code own, code owner_mgr) {
    return o.checks(build_domain, build_call, own, owner_mgr, mk_manager());
}
";

#! trusted code: the checks, in a method of an object that a tenant's call creates
class Checks {
    string checks(code build_domain, code build_call, code own, code owner_mgr, code caller_mgr) {
        list<string> l = ();
        l += Checks::tryIt(string sub () { return SandboxManager::getCallerIsolationDomain() ?? "none"; });
        # a sandbox that trusted code creates belongs to no tenant ("none"), whatever governs it
        l += Checks::tryIt(string sub () {
            SandboxManager m();
            return m.getIsolationDomain() ?? "none";
        });
        l += Checks::tryIt(string sub () {
            Program p(PO_NEW_STYLE);
            return "created";
        });
        l += Checks::tryIt(string sub () {
            Counter c(1);
            background c.dec();
            c.waitForZero();
            return "started";
        });
        l += Checks::tryIt(string sub () { return build_domain(); });
        l += Checks::tryIt(string sub () { return build_call(own); });
        # a sandbox that the code of each tenant creates here: a closure of the object's tenant runs as that tenant's
        # code; one of the calling tenant runs under the sandboxes of both
        l += Checks::tryIt(owner_mgr);
        l += Checks::tryIt(caller_mgr);
        return join(" ", l);
    }

    #! the result of the code, or its exception
    static string tryIt(code c) {
        try {
            return c();
        } catch (hash<ExceptionInfo> ex) {
            return ex.err;
        }
    }
}

# trusted code: creates an object of the checks
Checks sub new_checks() {
    return new Checks();
}

# a tenant, with a sandbox in the given isolation domain, if any
Program sub tenant(string name, *string domain) {
    SandboxManager sm();
    if (domain) {
        sm.setIsolationDomain(domain);
    }
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
    p.setSandboxManager(sm);
    p.parse(TenantCode, name);
    return p;
}

# trusted code that a trusted build runs: the isolation domain of the build's caller
string sub build_domain_impl() {
    return SandboxManager::getCallerIsolationDomain() ?? "none";
}

# trusted code that a trusted build runs: calls the code that the caller gives it
string sub build_call_impl(code c) {
    return c();
}

code build_domain = SandboxManager::createTrustedBuildCall(\build_domain_impl());
code build_call = SandboxManager::createTrustedBuildCall(\build_call_impl());

hash<string, Program> tenants = {
    "A": tenant("tenant-a", "domain-a"),
    "B": tenant("tenant-b", "domain-b"),
    "N": tenant("tenant-n"),
};

# the checks of an object that the first tenant's call created, called by the second tenant; A and B in both roles,
# so that the composite of their sandboxes has their members in both orders; N's sandbox has no domain, so it is
# treated as host code and A keeps its domain (see "Isolation Domains (Tenants)" in the sandboxing docs:
# sandbox_isolation_domains)
foreach list<string> pair in ((("A", "A"), ("A", "B"), ("B", "A"), ("B", "B"), ("A", "N"), ("N", "A"))) {
    Program owner = tenants{pair[0]};
    Program caller = tenants{pair[1]};
    object checks = owner.callFunction("make", \new_checks());
    code own = owner.callFunction("own");
    code owner_mgr = owner.callFunction("mk_manager");
    printf("%s's object called by %s: %s\n", pair[0], pair[1], caller.callFunction("run", checks, build_domain,
        build_call, own, owner_mgr));
}
