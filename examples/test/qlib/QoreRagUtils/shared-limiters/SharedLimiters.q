# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A sandboxed Program shares the rate limiters of EmbedderRateLimiter::getShared() only with its own code, so that it
# cannot change the limits of the host's limiter of an account or block it (issue #7686); see shared-limiters.qtest

%modern
%enable-all-warnings
%no-child-restrictions

# EmbedderRateLimiter is declared by QoreRagTypes, which VoyageRestClient requires: QoreRagUtils, which loads the
# ml module and its native libraries (whose output depends on the host, qore#7686), is not needed
%requires QoreRagTypes
%requires VoyageRestClient

const Url = "https://api.voyageai.com/v1";

# the host's limiter of an account
EmbedderRateLimiter host = VoyageRestClientBase::getRateLimiter(VoyageRestClientBase::RateLimitEmbed, Url,
    "account-key", "voyage-3", 300, 100000);

# the code of a tenant (a sandboxed Program) that uses the same account
const TenantCode = "
%modern
%requires QoreRagTypes
%requires VoyageRestClient

const Url = \"https://api.voyageai.com/v1\";

EmbedderRateLimiter sub limiter(*int rpm, *int tpm) {
    return VoyageRestClientBase::getRateLimiter(VoyageRestClientBase::RateLimitEmbed, Url, \"account-key\",
        \"voyage-3\", rpm, tpm);
}

# configures the limiter of the account, blocks it and changes its limits
string sub change() {
    string rv;
    try {
        EmbedderRateLimiter l = limiter(1, 1);
        l.block(3600000);
        l.setLimits(1, 1);
        rv = sprintf(\"limits of the tenant's limiter: %y\", l.getLimits());
    } catch (hash<ExceptionInfo> ex) {
        rv = ex.err;
    }
    return rv;
}

# the limiter of the account that the code of the tenant's Program shares
bool sub same_limiter() {
    return limiter() == limiter();
}

# fills the registry with limiters of the tenant
sub fill(int n) {
    map EmbedderRateLimiter::getShared(\"tenant-\" + $1), xrange(n);
}

# sets the limits of the tenant's limiter of a key
sub set_limits(string key) {
    EmbedderRateLimiter::getShared(key, 1, 1);
}

# whether a new key still gets a kept limiter
bool sub kept(string key) {
    return EmbedderRateLimiter::getShared(key) == EmbedderRateLimiter::getShared(key);
}
";

Program sub new_tenant() {
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
    p.setSandboxManager(new SandboxManager());
    p.parse(TenantCode, "tenant");
    return p;
}

Program tenant = new_tenant();
printf("the tenant: %s\n", tenant.callFunction("change"));
printf("the host's limits: %y\n", host.getLimits());
printf("the host's request: %s\n", (string sub () {
    try {
        host.admit(1, host.getClockUs() + 1000000);
        return "admitted";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
})());
printf("the tenant's code shares its limiter: %y\n", tenant.callFunction("same_limiter"));
printf("the host's limiter is the tenant's: %y\n", host == tenant.callFunction("limiter"));
Program other = new_tenant();
printf("another tenant's limiter is the first tenant's: %y\n",
    other.callFunction("limiter") == tenant.callFunction("limiter"));
# a limiter of trusted code whose key has the form of a key of the tenant's Program
EmbedderRateLimiter host_pgm = EmbedderRateLimiter::getShared(sprintf("pgm:%d|acct", tenant.getProgramId()), 300,
    100000);
tenant.callFunction("set_limits", "acct");
printf("the host's limiter of a key with the form of the tenant's: limits: %y\n", host_pgm.getLimits());
# the bound of the limiters of a sandboxed Program (EmbedderRateLimiter::MaxProgramLimiters)
tenant.callFunction("fill", 64);
printf("a new key of a tenant after its bound: kept: %y\n", tenant.callFunction("kept", "tenant-new"));
printf("a new key of another tenant: kept: %y\n", other.callFunction("kept", "other-new"));
printf("the host's limiter of the account is unchanged: %y\n", host == VoyageRestClientBase::getRateLimiter(
    VoyageRestClientBase::RateLimitEmbed, Url, "account-key", "voyage-3"));
