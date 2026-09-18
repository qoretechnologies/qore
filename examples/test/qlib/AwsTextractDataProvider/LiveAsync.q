#!/usr/bin/env qore
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT
# Opt-in integration helper. The caller creates and removes the named synthetic S3 fixture.
%modern
%prepend-module-path "${SCRIPT_DIR}/../../../../qlib"
%requires AwsTextractDataProvider
if (!ENV.AWS_TEXTRACT_TEST_BUCKET) {
    throw "LIVE-FIXTURE-ERROR", "AWS_TEXTRACT_TEST_BUCKET must name a task-owned synthetic fixture bucket";
}
AwsTextractDataProvider::AwsTextractDataProvider provider({});
hash<auto> started = provider.getChildProvider("start-document-text-detection").doRequest({
    "DocumentLocation": {"S3Object": {"Bucket": ENV.AWS_TEXTRACT_TEST_BUCKET, "Name": "synthetic-invoice.pdf"}},
});
if (!started.JobId.val()) { throw "LIVE-FIXTURE-ERROR", "start action did not return JobId"; }
hash<auto> request = hash({"JobId": started.JobId, "MaxResults": 2});
list<hash<auto>> blocks = ();
hash<auto> response;
int pages = 0;
for (int poll = 0; poll < 35; ++poll) {
    response = provider.getChildProvider("get-document-text-detection").doRequest(request);
    if (response.JobStatus == "IN_PROGRESS") { usleep(2s); continue; }
    if (response.JobStatus != "SUCCEEDED") {
        throw "LIVE-FIXTURE-ERROR", "job ended with " + response.JobStatus;
    }
    blocks += response.Blocks;
    ++pages;
    if (!response.NextToken.val()) { break; }
    request.NextToken = response.NextToken;
}
string text = (map $1.Text ?? "", blocks).join(" ");
if (response.JobStatus != "SUCCEEDED" || response.NextToken.val() || pages < 2
        || index(text, "QORE TEXTRACT TEST INVOICE") < 0 || index(text, "12.34") < 0) {
    throw "LIVE-FIXTURE-ERROR", "bounded asynchronous extraction or pagination did not complete";
}
printf("live asynchronous text extraction succeeded: %d result pages, %d blocks, synthetic text verified\n",
    pages, blocks.size());
