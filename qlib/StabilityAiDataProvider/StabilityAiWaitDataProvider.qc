# -*- mode: qore; indent-tabs-mode: nil -*-
#! Bounded retrieval of existing Stability AI jobs
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT
public namespace StabilityAiDataProvider {
#! Input fields for bounded retrieval of an existing service job
public class StabilityAiWaitRequestType inherits HashDataType {
    public {
        const Fields = {
            "job_id": {"display_name": "Job ID", "desc": "Job ID returned by the corresponding start action.",
                "type": AbstractDataProviderTypeMap."string", "required": True},
            "operation_timeout": {"display_name": "Wait Timeout", "desc": "Total wait time including status requests.",
                "type": AbstractDataProviderTypeMap."*timeout", "default_value": 5m},
            "poll_interval": {"display_name": "Poll Interval", "desc": "Polling delay; at least ten seconds in production.",
                "type": AbstractDataProviderTypeMap."*timeout", "default_value": 10s},
        };
    }
    #! Creates the wait request fields without accessing the service
    constructor() : HashDataType("StabilityAiWaitRequest") { addQoreFields(Fields); }
}
#! Shared wait request type
public const StabilityAiWaitRequest = new StabilityAiWaitRequestType();

#! Waits for existing asynchronous image, upscale, or audio results
public class StabilityAiWaitDataProvider inherits StabilityAiDataProviderCommon {
    public {
        const Jobs = {
            "wait-for-image": ("get-image-result", "Wait for Image Job", "Wait for an asynchronous background or lighting edit."),
            "wait-for-upscale": ("get-upscale-result", "Wait for Creative Upscale", "Wait for an existing creative upscale job."),
            "wait-for-audio": ("get-audio-result", "Wait for Audio Job", "Wait for an existing Stable Audio 3 job."),
        };
        const ProviderInfo = <DataProviderInfo>{"type": "StabilityAiWaitDataProvider", "supports_request": True};
        const RequestType = StabilityAiWaitRequest;
    }
    private { string name; }
    #! Creates a helper for one of the declared result families
    /** @param name wait action from Jobs
        @param rest authenticated client; each request creates its own deadline-bound copy
    */
    constructor(string name, StabilityAiRestClientIo rest) : StabilityAiDataProviderCommon(rest) { self.name = name; }
    #! Returns the stable wait action identity
    /** @return action identity */
    string getName() { return name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
    private *AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *AbstractDataProviderType getResponseTypeImpl() { return StabilityAiOperation::MediaType; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        StabilityAiOperation::validateOptions(Jobs{name}[0], req{"job_id",});
        timeout overall = req.operation_timeout ?? 5m;
        timeout interval = req.poll_interval ?? 10s;
        if (overall <= 0 || interval <= 0) { throw "STABILITYAI-INPUT-ERROR", "timeouts must be positive"; }
        if (rest.getUrl() !~ /^http:/ && interval < 10000) {
            throw "STABILITYAI-INPUT-ERROR", "production polling requires a delay of at least ten seconds";
        }
        date start = now_us();
        # A private copy gives each wait its own request deadline without mutating shared client state.
        StabilityAiRestClientIo client = cast<StabilityAiRestClientIo>(rest.copyWithUrl(rest.getUrl()));
        StabilityAiOperation getter(Jobs{name}[0], client);
        *hash<auto> last_status;
        int delay = interval;
        QoreAsyncJobUtils::AsyncJobPoller poller(hash<auto> sub (string job) {
            int remaining = int(overall) - get_duration_milliseconds(now_us() - start);
            if (remaining <= 0) {
                throw "STABILITYAI-JOB-TIMEOUT", "job wait deadline expired before the status request",
                    {"job_id": job, "last_status": last_status};
            }
            client.setTimeout(min(30000, remaining));
            client.setConnectTimeout(min(10000, remaining));
            try {
                last_status = getter.doRequest({"job_id": job});
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err =~ /TIMEOUT/) {
                    throw "STABILITYAI-JOB-TIMEOUT", "status request exceeded the job wait deadline",
                        {"job_id": job, "last_status": last_status};
                }
                rethrow;
            }
            delay = max(int(interval), (last_status.retry_after ?? 0) * 1000);
            return last_status;
        }, bool sub (hash<auto> status) { return status.status == "succeeded"; }, "Stability AI job", "STABILITYAI-JOB-TIMEOUT");
        poller.setSleeper(sub (timeout suggested) {
            int remaining = max(0, int(overall) - get_duration_milliseconds(now_us() - start));
            usleep(min(delay, remaining) * 1000);
        });
        int remaining = int(overall) - get_duration_milliseconds(now_us() - start);
        if (remaining <= 0) {
            throw "STABILITYAI-JOB-TIMEOUT", "job wait deadline expired during client preparation",
                {"job_id": req.job_id, "last_status": last_status};
        }
        return poller.poll(req.job_id, remaining, interval);
    }
}
}
