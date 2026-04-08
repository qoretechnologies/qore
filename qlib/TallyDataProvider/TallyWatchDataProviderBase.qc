# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TallyDataProvider module definition

/** TallyWatchDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the TallyDataProvider module
public namespace TallyDataProvider {
#! Base class for Tally polling watch providers
public class TallyWatchDataProviderBase inherits TallyDataProviderBase,
        DataProvider::AbstractWatchDataProviderBase {
    public {
        #! Watch constructor options
        const WatchConstructorOptions = ConstructorOptions + {
            "form_id": <DataProviderOptionInfo>{
                "display_name": "Form ID",
                "short_desc": "The form to watch for submissions",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The ID of the form to watch for new submissions",
            },
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "Polling interval in seconds",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Polling interval in seconds (minimum `" + MinPollInterval + "`, default `"
                    + DefaultPollInterval + "`)",
                "default_value": DefaultPollInterval,
            },
        };

        #! Maximum number of seen IDs to track before pruning
        const MaxSeenIds = 10000;
    }

    private {
        #! Form ID to watch
        *string form_id;

        #! Last seen submission ID for cursor-based pagination
        *string last_submission_id;

        #! Set of seen record IDs for deduplication
        hash<string, bool> seen_ids;

        #! Flag to track first poll (seed cursor without firing events)
        bool first_poll = True;
    }

    #! Creates the object from a RestClientIo client
    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : TallyDataProviderBase(rest),
              AbstractWatchDataProviderBase(
                  max(options.poll_interval ?? DefaultPollInterval, MinPollInterval)) {
        *hash<auto> ctx = DataProviderDataContextHelper::getHash();
        form_id = options.form_id ?? ctx.form_id ?? "";
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options)
            : TallyDataProviderBase(options),
              AbstractWatchDataProviderBase(
                  max(options.poll_interval ?? DefaultPollInterval, MinPollInterval)) {
        *hash<auto> ctx = DataProviderDataContextHelper::getHash();
        form_id = options.form_id ?? ctx.form_id ?? "";
    }

    #! Cancels in-flight REST operations for graceful shutdown
    private cancelPollOperation() {
        rest.close();
    }

    #! Polls for new submissions
    private pollOnce() {
        if (!form_id) {
            error("Cannot poll for submissions: form_id not set");
            return;
        }
        try {
            list<hash<auto>> records = fetchRecords();
            if (first_poll) {
                # on first poll, seed the cursor and seen IDs without firing events
                first_poll = False;
                foreach hash<auto> record in (records) {
                    seen_ids{record.id ?? ""} = True;
                }
                if (records.size()) {
                    last_submission_id = records.last().id;
                }
                return;
            }
            foreach hash<auto> record in (records) {
                string record_id = record.id ?? "";
                if (seen_ids{record_id}) {
                    continue;
                }
                seen_ids{record_id} = True;
                notifyObservers(getEventName(), record);
            }
            # update cursor to last seen ID
            if (records.size()) {
                last_submission_id = records.last().id;
            }
            # prune seen_ids if too large
            if (seen_ids.size() > MaxSeenIds) {
                seen_ids = {};
            }
        } catch (hash<ExceptionInfo> ex) {
            error("Error polling for %s events: %s: %s", getEventName(), ex.err, ex.desc);
        }
    }

    #! Fetches new submissions using cursor-based pagination
    private list<hash<auto>> fetchRecords() {
        string path = "forms/" + form_id + "/submissions?limit=" + MaxPageSize;
        if (last_submission_id) {
            path += "&afterId=" + encode_url(last_submission_id, True);
        }
        hash<auto> response = doRestCommand("GET", path);
        return response.body.submissions ?? ();
    }

    #! Returns example event data
    private auto getExampleEventDataImpl(string event_id) {
        try {
            list<hash<auto>> records = fetchRecords();
            if (records) {
                return records[0];
            }
        } catch () {
        }
        return getFakeExampleData();
    }

    #! Returns fake example data when no real records are available
    private hash<auto> getFakeExampleData() {
        return {
            "id": "abc123def456",
            "formId": form_id,
            "isCompleted": True,
            "submittedAt": "2026-04-07T12:00:00.000Z",
            "createdAt": "2026-04-07T12:00:00.000Z",
            "updatedAt": "2026-04-07T12:00:00.000Z",
            "responses": (
                {
                    "id": "resp_001",
                    "formId": form_id,
                    "questionId": "q_001",
                    "respondentId": "respondent_001",
                    "submissionId": "abc123def456",
                    "sessionUuid": "550e8400-e29b-41d4-a716-446655440000",
                    "answer": "Example answer",
                    "formattedAnswer": "Example answer",
                    "createdAt": "2026-04-07T12:00:00.000Z",
                    "updatedAt": "2026-04-07T12:00:00.000Z",
                },
            ),
        };
    }

    #! Returns the event types
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            getEventName(): <DataProviderMessageInfo>{
                "desc": sprintf("Fired when a new form submission is received for form %y", form_id),
                "type": getRecordDataType(),
            },
        };
    }

    #! Returns the event name
    private string getEventName() {
        return EVENT_FORM_SUBMISSION;
    }

    #! Returns the record data type
    private AbstractDataProviderType getRecordDataType() {
        return TallySubmissionDataType;
    }
}
}
