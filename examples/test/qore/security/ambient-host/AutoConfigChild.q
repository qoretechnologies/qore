#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the checks of ambient-auto-config.qtest: the data providers' auto-configuration must be set before any type is
# created, so it runs in a process of its own; the environment of the process has the options; prints one line per
# result: the key, "=", and the value (%y)

%modern
%no-child-restrictions

%requires DataProvider
%requires ServiceNowRestDataProvider
%requires ./AmbientCallers.qm

const Source = "%modern
%requires DataProvider
%requires ServiceNowRestDataProvider
%requires AmbientCallers
bool sub auto_config() {
    return DataProvider::DataProvider::getAutoConfig();
}
# the host's grants: library code that establishes a policy barrier or calls a system-policy capability (no closure:
# a policy barrier does not mask the provenance of a callback of the calling code)
bool sub auto_config_grant(string how) {
    return how == \"barrier\"
        ? AmbientCallers::invoke_barrier(\"DataProvider::DataProvider\", \"getAutoConfig\")
        : AmbientCallers::invoke_capability(\"DataProvider::DataProvider\", \"getAutoConfig\");
}
hash<auto> sub servicenow(*hash<auto> opts) {
    ServiceNowRestDataProvider::ServiceNowRestDataProvider dp({\"url\": \"http://localhost:1\"} + opts);
    return dp.getRestClient().getConstructorOptions(){\"username\", \"password\"};
}
";

DataProvider::setAutoConfig();
printf("host_auto_config=%y\n", DataProvider::getAutoConfig());
{
    ServiceNowRestDataProvider dp({"url": "http://localhost:1"});
    printf("host_servicenow=%y\n", dp.getRestClient().getConstructorOptions(){"username", "password"});
}
hash<string, Program> pgms = AmbientCallers::make_programs(Source, "auto-config");
foreach hash<auto> i in (pgms.pairIterator()) {
    string kind = replace(replace(i.key, ": ", "_"), " ", "_");
    printf("%s_auto_config=%y\n", kind, i.value.callFunction("auto_config"));
    printf("%s_auto_config_barrier=%y\n", kind, i.value.callFunction("auto_config_grant", "barrier"));
    printf("%s_auto_config_capability=%y\n", kind, i.value.callFunction("auto_config_grant", "capability"));
    printf("%s_servicenow=%y\n", kind, i.value.callFunction("servicenow"));
    # options that the caller gives are its own
    printf("%s_servicenow_explicit=%y\n", kind, i.value.callFunction("servicenow", {"username": "me",
        "password": "my-password"}));
}
