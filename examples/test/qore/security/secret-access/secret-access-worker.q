#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

%modern
%no-child-restrictions

%prepend-module-path "${SCRIPT_DIR}/lib"
%prepend-module-path "${SCRIPT_DIR}"
%requires SecretAccessLib

#! The checks of SandboxManager::checkSecretAccess() (qore#7863): prints one line for each case, "<case> <result>",
#! where the result is "ok" if the call gave the secret, or the error of the exception; the test runs it in each
#! execution mode and as a program compiled with qcc

#! the code of the borrower
const BorrowerCode = "%modern
%requires SecretAccessLib
string sub run(code c) {
    try {
        return c() == \"THE-SECRET\" ? \"ok\" : \"wrong\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
string sub direct(SalHolder h) {
    return run(string sub () { return h.getSecret(); });
}
string sub provider(SalProvider p, SalHolder h) {
    return run(string sub () { return p.use(h); });
}
string sub providerTwice(SalProvider p, SalHolder h) {
    return run(string sub () { return p.useTwice(h); });
}
string sub providerByName(SalProvider p, SalHolder h) {
    return run(string sub () { return p.useByName(h); });
}
string sub callObject(SalHolder h) {
    return run(string sub () { return call_object_method(h, \"getSecret\"); });
}
string sub callRef(SalHolder h) {
    code c = \\h.getSecret();
    return run(c);
}
string sub callback(SalProvider p, SalHolder h) {
    return run(string sub () { return p.callback(string sub () { return h.getSecret(); }); });
}
string sub ownHolder() {
    return run(string sub () { SalHolder h(); return h.getSecret(); });
}
";

const PathModuleCode = "%modern
%requires SecretAccessLib
%requires %s
string sub run(code c) {
    try {
        return c() == \"THE-SECRET\" ? \"ok\" : \"wrong\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
string sub viaPathModule(SalHolder h) {
    return run(string sub () { return new %s().use(h); });
}
";

#! a Program that a sandbox restricts; tenant: it has an isolation domain and may use uncontrolled APIs
Program sub getBorrower(string code, bool tenant = False) {
    Program p(PO_NO_CHILD_PO_RESTRICTIONS);
    SandboxManager sm();
    if (tenant) {
        sm.setUncontrolledApiPolicy(True);
        sm.setIsolationDomain("tenant-a");
    }
    # the borrower may read the module that it loads by its path
    sm.addFilesystemAllowedPath(path: get_script_dir(), access_modes: QSEC_READ);
    p.setSandboxManager(sm);
    p.parse(code, "borrower");
    return p;
}

sub report(string name, string result) {
    printf("%s %s\n", name, result);
}

sub run_cases() {
    SalHolder holder();
    SalProvider provider();

    # the host (the owner, not restricted)
    string host;
    try {
        host = holder.getSecret() == "THE-SECRET" ? "ok" : "wrong";
    } catch (hash<ExceptionInfo> ex) {
        host = ex.err;
    }
    report("host", host);

    foreach bool tenant in (False, True) {
        string pfx = tenant ? "tenant-" : "restricted-";
        Program p = getBorrower(BorrowerCode, tenant);
        report(pfx + "direct", p.callFunction("direct", holder));
        report(pfx + "provider", p.callFunction("provider", provider, holder));
        report(pfx + "provider-twice", p.callFunction("providerTwice", provider, holder));
        report(pfx + "provider-by-name", p.callFunction("providerByName", provider, holder));
        report(pfx + "call-object-method", p.callFunction("callObject", holder));
        report(pfx + "call-ref", p.callFunction("callRef", holder));
        report(pfx + "callback", p.callFunction("callback", provider, holder));
        report(pfx + "own-holder", p.callFunction("ownHolder"));
        string mod = tenant ? "SecretAccessPathT" : "SecretAccessPath";
        Program pm = getBorrower(sprintf(PathModuleCode, get_script_dir() + "/" + mod + ".qm",
            tenant ? "SaptUser" : "SapUser"), tenant);
        report(pfx + "path-module", pm.callFunction("viaPathModule", holder));
    }
}

run_cases();
