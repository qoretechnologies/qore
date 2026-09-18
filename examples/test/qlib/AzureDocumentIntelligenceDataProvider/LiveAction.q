#!/usr/bin/env qore
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT
# Private subprocess protocol for live_lifecycle.py. Never run with inherited stdout logging.
%modern
%prepend-module-path "${SCRIPT_DIR}/../../../../qlib"
%requires AzureDocumentIntelligenceDataProvider
%requires json

try {
    hash<auto> command = parse_json(stdin.read(-1));
    hash<auto> auth = command.bearer ? {"entra_token": ENV.AZURE_DI_TOKEN} : {"apikey": ENV.AZURE_DI_KEY};
    if (!ENV.AZURE_DI_ENDPOINT || !(command.bearer ? ENV.AZURE_DI_TOKEN : ENV.AZURE_DI_KEY)) {
        throw "AZURE-LIVE-CONFIG-ERROR", "missing private endpoint/authentication environment";
    }
    AzureDocumentIntelligenceRestConnection connection("azure-live-lifecycle", "Synthetic live verification",
        ENV.AZURE_DI_ENDPOINT, {}, auth);
    AzureDocumentIntelligenceRestClientIo client = cast<AzureDocumentIntelligenceRestClientIo>(connection.getAsync());
    auto result;
    if (command.action == "ping") {
        if (!connection.ping(True).ok) { throw "AZURE-LIVE-ERROR", "connection ping failed"; }
        AzureDocumentIntelligenceRestConnection from_hash({"name": "azure-live-hash",
            "url": ENV.AZURE_DI_ENDPOINT, "opts": auth});
        if (!from_hash.ping(True).ok) { throw "AZURE-LIVE-ERROR", "hash connection ping failed"; }
        AzureDocumentIntelligenceRestClient sync(({"url": ENV.AZURE_DI_ENDPOINT}) + auth, True);
        foreach AbstractRestClient original in (sync, client) {
            foreach AbstractRestClient copy in (original, original.copyWithUrl(ENV.AZURE_DI_ENDPOINT)) {
                if (!exists copy.restGet(AzureDocumentIntelligenceRestConnection::DefaultPingPath).body.customDocumentModels.count) {
                    throw "AZURE-LIVE-ERROR", "client copy authentication failed";
                }
            }
        }
        result = {"ok": True};
    } else {
        hash<auto> options = command.options ?? {};
        if (command.bytes_field) { options{command.bytes_field} = parse_base64_string(options{command.bytes_field}); }
        AzureDocumentIntelligenceDataProvider provider(client);
        AbstractDataProvider action = provider.getChildProvider(command.action);
        result = action.doRequest(options);
        action.getResponseType().acceptsValue(result);
        if (result.typeCode() == NT_HASH && result.body.typeCode() == NT_BINARY) {
            result.binary_body = make_base64_string(result.body);
            delete result.body;
        }
    }
    print("QORE-LIVE-RESULT " + make_json({"result": result}) + "\n");
} catch (hash<ExceptionInfo> ex) {
    # All diagnostics remain captured in the parent, including secret-bearing service responses.
    print("QORE-LIVE-RESULT " + make_json({"error": ex.err, "detail": ex.desc, "argument": ex.arg}) + "\n");
    return 1;
}
