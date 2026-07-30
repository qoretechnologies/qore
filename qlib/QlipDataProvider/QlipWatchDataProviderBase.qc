# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file QlipWatchDataProviderBase.qc Base class for Qlip polling watch data providers

/*  QlipWatchDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the QlipDataProvider module
public namespace QlipDataProvider {

#! Base class for Qlip polling watch (trigger) data providers
public class QlipWatchDataProviderBase inherits QlipDataProviderBase,
        DataProvider::AbstractWatchDataProviderBase {
    public {
        #! Constructor options for watch providers
        const WatchConstructorOptions = BaseConstructorOptions + {
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "How often to poll for new builds (seconds)",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Polling interval in seconds (minimum " + MinPollInterval + ")",
                "default_value": DefaultPollInterval,
            },
            "start_date": <DataProviderOptionInfo>{
                "display_name": "Start Date",
                "short_desc": "Only detect builds created after this date",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Only detect builds created after this date/time",
            },
        };
    }

    private {
        #! Last check time
        date last_check;

        #! Primary set of seen IDs for deduplication (current bucket)
        hash<string, bool> seen_ids;

        #! Previous bucket of seen IDs (for 2-bucket rotation)
        hash<string, bool> old_seen_ids;
    }

    #! Creates the object with a REST client
    constructor(QlipRestClient::QlipRestClient rest, *hash<auto> options)
            : QlipDataProviderBase(rest),
              AbstractWatchDataProviderBase(max(options.poll_interval ?? DefaultPollInterval,
                    MinPollInterval)) {
        last_check = options.start_date ?? now_us();
    }

    #! Destructor - stops the polling thread
    destructor() {
        stopEventsIntern();
    }

    #! Cancels any in-flight REST request during shutdown
    private cancelPollOperation() {
        rest.close();
    }

    #! Polls for new records once
    private pollOnce() {
        try {
            list<hash<auto>> records = fetchRecords(last_check);
            date new_last_check = last_check;

            foreach hash<auto> record in (records) {
                if (isStopping()) {
                    break;
                }
                string id = getRecordId(record);

                # Skip if already seen (check both current and previous buckets)
                if (seen_ids{id} || old_seen_ids{id}) {
                    continue;
                }

                # Check timestamp to avoid old records
                *date record_time = getRecordTimestamp(record);
                if (record_time) {
                    if (record_time <= last_check) {
                        continue;
                    }
                    if (record_time > new_last_check) {
                        new_last_check = record_time;
                    }
                }

                # Mark as seen + notify observers
                seen_ids{id} = True;
                notifyObservers(getEventName(), record);
            }

            last_check = new_last_check;

            # Rotate seen_ids buckets when the current bucket exceeds half capacity
            if (seen_ids.size() > MaxSeenIds / 2) {
                old_seen_ids = seen_ids;
                seen_ids = {};
            }
        } catch (hash<ExceptionInfo> ex) {
            # Log only ex.err (not ex.desc) to avoid leaking the Bearer token from HTTP error details
            log(LoggerLevel::ERROR, "Error polling %s: %s", getEventName(), ex.err);
        }
    }

    #! Returns the timestamp from a build record for deduplication (Qlip uses camelCase)
    private *date getRecordTimestamp(hash<auto> record) {
        *string ts = record.createdAt ?? record.updatedAt;
        if (ts) {
            return date(ts);
        }
    }

    #! Returns a hash of all supported event types
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            getEventName(): <DataProviderMessageInfo>{
                "desc": sprintf("Fired when a new %s event is detected", getEventName()),
                "type": getRecordDataType(),
            },
        };
    }

    #! Returns example event data for documentation/testing
    private auto getExampleEventDataImpl(string event_id) {
        try {
            list<hash<auto>> records = fetchRecords(now_us() - 30D);
            if (records) {
                return records[0];
            }
        } catch () {
            # Fall through to fake data if the API call fails
        }
        return getFakeExampleData();
    }

    #! Returns fake example data when no real records are available
    private hash<auto> getFakeExampleData() {
        return {
            "id": "20260101-000000",
            "buildNumber": 1,
            "projectName": "example",
            "branch": "main",
            "status": "pending",
            "createdAt": "2026-01-01T00:00:00.000Z",
        };
    }

    #! Returns the record ID as a string
    private string getRecordId(hash<auto> record) {
        return string(record.id);
    }

    #! Fetches build records created since the given date
    /** Subclasses implement this to poll the Qlip API for their event.
    */
    abstract private list<hash<auto>> fetchRecords(date since);

    #! Returns the event name for notifications
    abstract private string getEventName();

    #! Returns the data type for event records
    abstract private AbstractDataProviderType getRecordDataType();
}

} # namespace QlipDataProvider
