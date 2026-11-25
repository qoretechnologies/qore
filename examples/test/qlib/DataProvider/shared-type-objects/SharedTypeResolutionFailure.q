# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# Where the sandbox that governs code cannot be resolved, what the code creates is treated as shared, so sandboxed
# code cannot change it, and a change of a process-wide registry is refused: both fail closed (qore#6843); see
# shared-type-resolution-failure.qtest.  Needs a debug build of the library, whose dbg_sandbox_fail_next_resolutions()
# makes the resolutions fail as if no memory could be allocated

%modern
%enable-all-warnings
%no-child-restrictions

%requires DataProvider

# the code of a tenant: a sandboxed Program
const TenantCode = "
%modern
%requires DataProvider

string sub change(DataProvider::HashDataType t) {
    try {
        t.addField(new DataProvider::QoreDataField(\"tenant\", \"tenant\", Reflection::StringOrNothingType));
        return \"changed\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
";

Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
p.setSandboxManager(new SandboxManager());
p.parse(TenantCode, "tenant");

# the debug function is called by name, so that the fixture parses in any build
call_function("dbg_sandbox_fail_next_resolutions", 1000);
HashDataType t("created");
bool registration_allowed = SandboxManager::isProcessWideRegistrationAllowed();
call_function("dbg_sandbox_fail_next_resolutions", 0);

printf("a change of a process-wide registry while the sandbox cannot be resolved is allowed: %y\n",
    registration_allowed);
printf("the tenant changes a type that trusted code created while its sandbox could not be resolved: %s\n",
    p.callFunction("change", t));
printf("the type: %y\n", keys t.getFields());
