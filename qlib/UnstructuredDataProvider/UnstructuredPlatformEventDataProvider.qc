# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnstructuredDataProvider module: Platform API job events

/** UnstructuredPlatformEventDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

public namespace UnstructuredDataProvider {
#! Watches Unstructured for workflow runs that have finished
/** A workflow run is asynchronous, so the point at which an integration can act on its output is when the
    job reaches a terminal status.  This provider polls the job listing and fires an event for each job
    that has finished since the last cycle.

    @section unstructured_events_why_polling Why Polling and Not Webhooks

    Unstructured can post job notifications to a webhook channel, and this module exposes the actions that
    create, verify and delete those channels.  Their **deliveries** are not consumed here, because the
    Platform API description documents a consumer-supplied signing secret but no signature header, no
    algorithm and no signed payload format.  A webhook endpoint is a URL anyone can post to: without
    verification a fabricated `job.completed` would tell an integration that documents are ready which were
    never processed.  Accepting deliveries unverified is not an option, and guessing the scheme is not
    verification, so the events here are derived from the API's own job listing instead - which is
    authoritative and needs no inbound connectivity.

    Use the notification-channel actions to route Unstructured's webhooks to an endpoint that does verify
    them, when Unstructured documents how.

    @since %UnstructuredDataProvider 1.0
*/
public class UnstructuredJobWatchDataProvider inherits DataProvider::AbstractDataProvider,
        DataProvider::AbstractTimestampWatchDataProvider {
    public {
        #! The name of this provider
        const ProviderName = "job-finished";

        #! The event fired when a job reaches a terminal status
        const EventJobFinished = "job-finished";

        #! The default polling interval in seconds
        const DefaultPollInterval = 60;

        #! The shortest polling interval accepted
        /** A workflow run takes minutes at best, so polling faster than this only spends requests.
        */
        const MinPollInterval = 15;

        #! Job statuses that mean the run is over
        /** A job in one of these will not change again, so it is safe to fire an event for it exactly
            once.  \c COMPLETED covers a run that finished with per-file errors as well as a clean one;
            \c get-job-failed-files says which files those were.
        */
        const TerminalStatuses = {
            "COMPLETED": True,
            "FAILED": True,
            "STOPPED": True,
            "REJECTED": True,
        };

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": ProviderName,
            "type": "UnstructuredJobWatchDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Constructor options
        const ConstructorOptions = {
            "platformclient": <DataProviderOptionInfo>{
                "display_name": "Platform REST Client",
                "short_desc": "The Platform API REST client to use",
                "type": AbstractDataProviderType::get(
                    new Type("UnstructuredRestClient::UnstructuredPlatformRestClientIo"),
                    NOTHING, {DTT_ClientOnly: True}),
                "desc": "The Unstructured Platform API REST client to use",
            },
            "workflow_id": <DataProviderOptionInfo>{
                "display_name": "Workflow",
                "short_desc": "Watch only this workflow's runs",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Watch only the runs of this workflow.  Leave unset to watch every workflow in "
                    "the account.",
            },
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "How often to check for finished jobs, in seconds",
                "type": AbstractDataProviderTypeMap."*softint",
                "desc": "How often to check for finished jobs, in seconds; the minimum is `"
                    + MinPollInterval + "`.",
                "default_value": DefaultPollInterval,
            },
            "start_date": <DataProviderOptionInfo>{
                "display_name": "Start Date",
                "short_desc": "Only fire for jobs created after this date",
                "type": AbstractDataProviderTypeMap."*date",
                "desc": "Only fire for jobs created on or after this date.",
            },
        };
    }

    private {
        #! The Platform API client
        *UnstructuredRestClient::UnstructuredPlatformRestClientIo rest;

        #! The workflow to watch, if any
        *string workflow_id;
    }

    #! Creates the object from a client and options
    constructor(*UnstructuredRestClient::UnstructuredPlatformRestClientIo rest, *hash<auto> options)
            : AbstractTimestampWatchDataProvider(
                max(options.poll_interval ?? DefaultPollInterval, MinPollInterval),
                options.start_date) {
        self.rest = rest;
        workflow_id = options.workflow_id;
    }

    #! Creates the object from options alone
    constructor(*hash<auto> options)
            : AbstractTimestampWatchDataProvider(
                max(options.poll_interval ?? DefaultPollInterval, MinPollInterval),
                options.start_date) {
        rest = options.platformclient;
        workflow_id = options.workflow_id;
    }

    #! Stops the polling thread
    destructor() {
        stopEventsIntern();
    }

    string getName() {
        return ProviderName;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! The job listing is not ordered, so the watch orders the records itself
    string getSourceOrdering() {
        return WSO_UNKNOWN;
    }

    #! Returns the event fired for a finished job
    private string getEventName() {
        return EventJobFinished;
    }

    #! Returns the finished jobs created since the given date
    /** The Platform API has no "changed since" filter on jobs, so the listing is fetched and narrowed
        here; the base class deduplicates by ID so a job is delivered exactly once.
    */
    private list<hash<auto>> fetchRecords(date since) {
        if (!rest) {
            throw "UNSTRUCTURED-WATCH-ERROR", "no Unstructured Platform API client is configured for "
                "this event provider";
        }
        string path = UnstructuredRestClient::UnstructuredPlatformRestClientIo::ApiPath + "/jobs/";
        if (workflow_id.val()) {
            path += "?workflow_id=" + workflow_id;
        }
        auto body = rest.restGet(path).body;
        list<hash<auto>> rv = ();
        foreach auto job in (body.typeCode() == NT_LIST ? body : ()) {
            if (!TerminalStatuses{job.status}) {
                continue;
            }
            *date created = UnstructuredJobWatchDataProvider::getJobDate(job);
            if (exists created && exists since && created < since) {
                continue;
            }
            rv += job;
        }
        return rv;
    }

    #! Returns the job's ID
    private *string getWatchRecordId(hash<auto> record) {
        return record.id;
    }

    #! Returns the job's position in the feed
    private *date getWatchRecordTimestamp(hash<auto> record) {
        return UnstructuredJobWatchDataProvider::getJobDate(record);
    }

    #! Returns the events this provider fires
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EventJobFinished: <DataProviderMessageInfo>{
                "desc": "Fired once for each workflow run that reaches a terminal status: "
                    "`COMPLETED`, `FAILED`, `STOPPED` or `REJECTED`.\n\n"
                    "A `COMPLETED` job may still have failed on individual files; use "
                    "**Get Job Failed Files** to find out which.",
                "type": UnstructuredJobEventDataType,
            },
        };
    }

    #! Returns example event data, preferring a real job
    private auto getExampleEventDataImpl(string event_id) {
        try {
            list<hash<auto>> jobs = fetchRecords(now_us() - 30D);
            if (jobs) {
                return jobs[0];
            }
        } catch (hash<ExceptionInfo> ex) {
            # fall through to the example below
        }
        return {
            "id": "3f5c9b1e-6d2a-4c8f-9a7b-1e2d3c4b5a6f",
            "workflow_id": "c1cc4367-3e35-4af9-9d77-397cd6a9df2c",
            "workflow_name": "Contracts to Pinecone",
            "status": "COMPLETED",
            "created_at": 2026-09-06T09:15:00Z,
            "runtime": "00:04:12",
            "job_type": "scheduled",
        };
    }

    #! Returns the creation date of a job record
    private static *date getJobDate(hash<auto> job) {
        auto v = job.created_at;
        if (!exists v) {
            return;
        }
        return v.typeCode() == NT_DATE ? v : date(v.toString());
    }
}

#! The data fired for a finished workflow run
public class UnstructuredJobEventDataTypeImpl inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "Job ID",
                "short_desc": "The job that finished",
                "desc": "The job that finished.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "workflow_id": {
                "display_name": "Workflow ID",
                "short_desc": "The workflow that was run",
                "desc": "The workflow this job was a run of.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "workflow_name": {
                "display_name": "Workflow Name",
                "short_desc": "The name of the workflow that was run",
                "desc": "The name of the workflow this job was a run of.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "status": {
                "display_name": "Status",
                "short_desc": "The terminal status the job reached",
                "desc": "The terminal status the job reached.",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "COMPLETED", "display_name": "Completed"},
                    <AllowedValueInfo>{"value": "FAILED", "display_name": "Failed"},
                    <AllowedValueInfo>{"value": "STOPPED", "display_name": "Stopped"},
                    <AllowedValueInfo>{"value": "REJECTED", "display_name": "Rejected"},
                ),
            },
            "created_at": {
                "display_name": "Created",
                "short_desc": "When the job was created",
                "desc": "When the job was created.",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "runtime": {
                "display_name": "Runtime",
                "short_desc": "How long the job ran for",
                "desc": "How long the job ran for, as Unstructured reports it.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "job_type": {
                "display_name": "Job Type",
                "short_desc": "Whether the run was scheduled or explicit",
                "desc": "Whether the run was scheduled or started explicitly.",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() : HashDataType("UnstructuredJobEvent") {
        addQoreFields(Fields);
        # a job record carries fields that depend on the workflow, so unknown keys are kept
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

#! The data type fired for a finished workflow run
public const UnstructuredJobEventDataType = new UnstructuredJobEventDataTypeImpl();
}
