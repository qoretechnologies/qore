#!/usr/bin/env qore
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT
# Opt-in: the caller supplies resource credentials privately through the environment.
%modern
%prepend-module-path "${SCRIPT_DIR}/../../../../qlib"
%requires AzureDocumentIntelligenceDataProvider

if (!ENV.AZURE_DI_ENDPOINT || !ENV.AZURE_DI_KEY) {
    throw "AZURE-LIVE-CONFIG-ERROR", "live verification requires the endpoint and key environment variables";
}
hash<auto> opts = {"apikey": ENV.AZURE_DI_KEY};
AzureDocumentIntelligenceRestConnection connection("azure-live", "Synthetic fixture verification",
    ENV.AZURE_DI_ENDPOINT, {}, opts);
if (!connection.ping(True).ok) { throw "AZURE-LIVE-ERROR", "valid-key connection ping failed"; }
AzureDocumentIntelligenceRestConnection from_hash({"name": "azure-live-hash", "url": ENV.AZURE_DI_ENDPOINT,
    "opts": opts});
if (!from_hash.ping(True).ok) { throw "AZURE-LIVE-ERROR", "hash-constructor ping failed"; }
AzureDocumentIntelligenceRestConnection invalid("azure-live-invalid", "Invalid fixture key", ENV.AZURE_DI_ENDPOINT,
    {}, {"apikey": "invalid-qore-5421-fixture-key"});
if (invalid.ping(False).ok) { throw "AZURE-LIVE-ERROR", "invalid-key ping was accepted"; }
print("live ping: both constructors authenticated; invalid key rejected\n");

AzureDocumentIntelligenceRestClientIo client = cast<AzureDocumentIntelligenceRestClientIo>(connection.getAsync());
AzureDocumentIntelligenceDataProvider provider(client);
binary document = File::readBinaryFile(get_script_dir() + "/synthetic-invoice.pdf");
hash<auto> started = provider.getChildProvider("start-analysis-stream").doRequest({
    "model_id": "prebuilt-read", "body": document, "pages": "1", "output": ("pdf",),
});
if (started.status_code != 202 || !started.headers."operation-location".val()) {
    throw "AZURE-LIVE-ERROR", "raw start lost the accepted job";
}
string path = AzureDocumentIntelligenceRestClientBase::servicePath(client.getUrl(), started.headers."operation-location");
string result_id = split("?", split("/analyzeResults/", path)[1])[0];
hash<auto> result;
date deadline = now_us() + 2m;
while (now_us() < deadline) {
    result = provider.getChildProvider("get-analysis-result").doRequest({"model_id": "prebuilt-read", "result_id": result_id});
    if (inlist(result.status, TerminalStatuses)) { break; }
    usleep(1s);
}
if (result.status != "succeeded" || result.analyzeResult.content.find("QORE TEXTRACT TEST INVOICE") == -1
        || result.analyzeResult.content.find("12.34") == -1) {
    throw "AZURE-LIVE-ERROR", "synthetic read extraction did not match the expected text";
}
hash<auto> pdf = provider.getChildProvider("get-result-pdf").doRequest({"model_id": "prebuilt-read", "result_id": result_id});
if (pdf.body.typeCode() != NT_BINARY || pdf.headers."content-type" !~ /^application\/pdf/
        || pdf.body[0..4] != binary("%PDF-")) {
    throw "AZURE-LIVE-ERROR", "searchable PDF bytes or media type were lost";
}
printf("live read: synthetic content verified; searchable PDF bytes=%d\n", pdf.body.size());
foreach string model in ("prebuilt-layout", "prebuilt-invoice") {
    hash<auto> request = {"model_id": model, "document_bytes": document, "pages": "1", "analysis_timeout": 2m};
    if (model == "prebuilt-layout") {
        request.features = ("keyValuePairs",);
        request.outputContentFormat = "markdown";
        request.output = ("figures",);
    }
    hash<auto> analysis = provider.getChildProvider("analyze-document").doRequest(request);
    if (analysis.modelId != model || analysis.content.find("12.34") == -1 || analysis.pages.size() != 1) {
        throw "AZURE-LIVE-ERROR", "synthetic helper extraction failed";
    }
    printf("live %s: content verified pages=%d documents=%d figures=%d\n", model, analysis.pages.size(),
        analysis.documents.size(), analysis.figures.size());
}
print("live verification complete: three synthetic one-page analyses; no custom training or batch jobs\n");
