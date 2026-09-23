# -*- mode: qore; indent-tabs-mode: nil -*-
#! Bounded BFL job retrieval and synchronous video generation
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT

public namespace BflDataProvider {
#! Provides resumable regional job retrieval and verified media downloads
public class BflJobDataProvider inherits BflDataProviderCommon {
    public {
        const Helpers = {
            "get-routed-result": ("Get Routed Result", "Read a job through its original regional polling URL."),
            "wait-for-result": ("Wait for Result", "Wait for an existing job without resubmitting it."),
            "download-result": ("Download Result", "Retrieve and download a completed job's media before expiry."),
            "generate-video": ("Generate Video", "Generate and download a FLUX 3 video within one deadline."),
        };
        const ProviderInfo = <DataProviderInfo>{"type": "BflJobDataProvider", "supports_request": True};
    }
    private { string name; }
    #! Creates a helper without contacting the service
    /** @param name published helper identity
        @param rest authenticated client
    */
    constructor(string name, BflRestClientIo rest) : BflDataProviderCommon(rest) { self.name = name; }
    #! Returns the stable helper identity
    string getName() { return name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
    private *AbstractDataProviderType getRequestTypeImpl() { return requestType(name); }
    private *AbstractDataProviderType getResponseTypeImpl() { return name == "download-result" ? BflDownloadedAsset : BflCompletedJob; }

    #! Returns a static typed form for registration and execution
    /** @param name helper identity
        @return helper request type
    */
    static AbstractDataProviderType requestType(string name) {
        if (name == "download-result") {
            HashDataType type("BflDownloadRequest");
            type.addQoreFields(BflJobRequestDataType::Fields + BflJobRequestDataType::Controls + {
                "asset": {"display_name": "Asset", "type": AbstractDataProviderTypeMap."*string", "default_value": "sample",
                    "desc": "Download the media sample or save the encrypted draft bundle for later enhancement.",
                    "allowed_values": (<AllowedValueInfo>{"value": "sample", "display_name": "Media Sample"},
                        <AllowedValueInfo>{"value": "draft_cache", "display_name": "Draft Cache"})},
            });
            return type;
        }
        if (name != "generate-video") { return BflJobRequest; }
        HashDataType type("BflGenerateVideoRequest");
        foreach AbstractDataField field in (BflApi::getActionSet().getOperationEx("start-video").getRequestFields().iterator()) {
            type.addField(field);
        }
        type.addQoreFields(BflJobRequestDataType::Controls);
        return type;
    }

    #! Preserves strict scalar inputs before the shared provider type can coerce them
    /** @param req helper options
        @param request_options provider transport options
        @return current/completed job or verified media
        @throw BFL-INPUT-ERROR invalid helper options
    */
    auto doRequest(auto req, *hash<auto> request_options) {
        if (req.typeCode() != NT_HASH) { throw "BFL-INPUT-ERROR", "helper options must be an object"; }
        BflJobs::validateControls(rest, req);
        if (name == "generate-video") {
            checkRequest();
            return doRequestImpl(req, validateRequestOptions(request_options));
        }
        return BflDataProviderCommon::doRequest(req, request_options);
    }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        if (name == "generate-video") { return BflApi::generateMedia(rest, DefaultVideoModel, req); }
        date start = now_us();
        timeout budget = req.job_timeout ?? DefaultJobTimeout;
        hash<auto> job = req{"id", "polling_url"};
        if (name == "wait-for-result") { return rest.waitForResult(job, req); }
        hash<auto> result = BflJobs::get(rest, job, budget);
        if (name == "get-routed-result") { return result; }
        if (result.status != "Ready") {
            throw "BFL-JOB-ERROR", "download requires a Ready job", BflJobs::safeIdentity(job, result);
        }
        int remaining = int(budget) - get_duration_milliseconds(now_us() - start);
        if (remaining <= 0) { BflJobs::timedOut(job, result); }
        string asset = req.asset ?? "sample";
        if (result.result{asset}.typeCode() != NT_STRING || !result.result{asset}.val()) {
            throw "BFL-DOWNLOAD-ERROR", "the completed job does not contain the requested asset", BflJobs::safeIdentity(job, result);
        }
        try {
            return downloadMedia(result.result{asset}, remaining, req.max_bytes ?? 104857600,
                rest.getUrl() =~ /^http:/ ? rest.getUrl() : NOTHING, asset == "draft_cache");
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err =~ /TIMEOUT/) { BflJobs::timedOut(job, result); }
            throw "BFL-DOWNLOAD-ERROR", "completed job's media could not be downloaded", BflJobs::safeIdentity(job, result);
        }
    }
}
}
