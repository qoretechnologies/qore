#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
#
# Copyright 2026 Qore Technologies, s.r.o.
#
# Helper for CdsRestDataProviderNarrowedCallerHash.qtest: environment variables are read once when a process
# starts, so the auto-configuration from CDS_* variables is exercised in a child process started with them set.
# Prints one line per provider class named in the arguments: "<class>: <client URL>" or "<class>: <exception>".

%modern

%prepend-module-path "${SCRIPT_DIR}/../../../../qlib"
%requires DataProvider
%requires CdsRestDataProvider

#! client options without a URL; the URL comes from CDS_URL, and the token avoids an authorization at construction, and
#  the instance type is used by Business Central only
const ClientOptions = {
    "cdsrestclient_options": {
        "oauth2_client_id": "test-id",
        "oauth2_client_secret": "test-secret",
        "token": "test-token",
        "instance_type": "sandbox",
    },
};

DataProvider::setAutoConfig();

foreach string cls in (ARGV) {
    try {
        object dp = create_object("CdsRestDataProvider::" + cls, ClientOptions);
        printf("%s: %s\n", cls, dp.getRestClient().getConstructorOptions().url);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", cls, ex.err, ex.desc);
    }
}
