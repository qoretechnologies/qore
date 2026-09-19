# -*- mode: qore; indent-tabs-mode: nil -*-
#! Bounded polling of an existing Document AI operation
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT

public namespace GoogleDocumentAiDataProvider {
#! Waits for an existing job while preserving its identity and final outcome
public class GoogleDocumentAiWaitOperationDataProvider inherits GoogleDocumentAiDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{"name": "wait-for-operation",
            "type": "GoogleDocumentAiWaitOperationDataProvider", "supports_request": True,
            "constructor_options": ConstructorOptions};
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        static HashDataType RequestType;
        static AbstractDataProviderType ResponseType;
    }

    private { static Mutex type_lock(); }

    #! Creates the helper over the existing authenticated transport
    /** @param rest authenticated client
        @param project_id resource project
        @param region resource location; defaults to US
        @param sa_json_path optional credential file override
    */
    constructor(GoogleDocumentAiRestClientIo rest, string project_id, *string region, *string sa_json_path)
            : GoogleDocumentAiDataProviderCommon(rest, project_id, region, sa_json_path) {}
    #! Creates the helper from connection options
    /** @param options options declared by ConstructorOptions */
    constructor(*hash<auto> options) : GoogleDocumentAiDataProviderCommon(options) {}

    #! Initializes helper types after inventory publication
    static initializeTypes() {
        AutoLock al(type_lock);
        if (RequestType) { return; }
        HashDataType request("GoogleDocumentAiWaitOperationRequest");
        request.addQoreFields({
            "operation_id": {"type": AbstractDataProviderTypeMap."string", "required": True,
                "display_name": "Operation ID", "desc": "Operation ID or full resource name returned when starting the job."},
            "api_version": {"type": AbstractDataProviderTypeMap."*string", "display_name": "API Version",
                "desc": "Use the API version that started the operation.", "default_value": "v1", "allowed_values": (
                    <AllowedValueInfo>{"value": "v1", "display_name": "Stable (v1)"},
                    <AllowedValueInfo>{"value": "v1beta3", "display_name": "Beta (v1beta3)"})},
            "operation_timeout": {"type": AbstractDataProviderTypeMap."*timeout", "display_name": "Wait Timeout",
                "desc": "Maximum time to wait; timeout errors retain the operation name and last observed status.",
                "default_value": 5m},
            "poll_interval": {"type": AbstractDataProviderTypeMap."*timeout", "display_name": "Poll Interval",
                "desc": "Delay between status requests.", "default_value": 1500ms},
        });
        list<AbstractDataProviderType> outcomes = map OpenApi3::OpenApi3HashDataType::forResponse(
            $1.getSchema().getTypeForReference("GoogleLongrunningOperation")), GoogleDocumentAiSchema::getActionSets();
        ResponseType = new QoreUnionDataType(outcomes);
        RequestType = request;
    }

    string getName() { return ProviderInfo.name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
    private *AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string base = GoogleDocumentAiResources::prefix(project_id, region) + "/operations/";
        string id = GoogleDocumentAiResources::id(req.operation_id, base);
        string resource = base + id;
        string path = "/" + (req.api_version ?? "v1") + "/projects/" + encode_url(project_id, True)
            + "/locations/" + encode_url(region, True) + "/operations/" + encode_url(id, True);
        timeout overall = req.operation_timeout ?? 5m;
        date start = now_us();
        GoogleDocumentAiRestClientIo client = rest;
        *hash<auto> last_status;
        QoreAsyncJobUtils::AsyncJobPoller poller(hash<auto> sub (string job) {
            int remaining = max(1, int(overall) - get_duration_milliseconds(now_us() - start));
            auto body;
            try {
                body = client.restDoRequestWithTimeout("GET", path, NOTHING, NOTHING, remaining).body;
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err =~ /TIMEOUT/) {
                    throw "GOOGLE-DOCUMENTAI-OPERATION-TIMEOUT", "operation status request exceeded the wait deadline",
                        {"job_id": job, "last_status": last_status};
                }
                rethrow;
            }
            if (body.typeCode() != NT_HASH || body.name != job
                    || (body.hasKey("done") && body.done.typeCode() != NT_BOOLEAN)
                    || (body.hasKey("error") && body.error.typeCode() != NT_HASH)
                    || (body.hasKey("response") && body.response.typeCode() != NT_HASH)
                    || (body.hasKey("error") && body.hasKey("response"))
                    || (!body.done && (body.hasKey("error") || body.hasKey("response")))) {
                throw "GOOGLE-DOCUMENTAI-OPERATION-ERROR", "malformed operation status", {"job_id": job};
            }
            last_status = body;
            return body;
        }, bool sub (hash<auto> status) { return status.done ?? False; }, "Document AI operation",
            "GOOGLE-DOCUMENTAI-OPERATION-TIMEOUT");
        # Final service errors are data, alongside successful outcomes and per-document batch statuses.
        # This preserves the complete job record for workflow branching and failure analysis.
        return poller.poll(resource, overall, req.poll_interval);
    }
}
}
