# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ClickFunnelsWatchDataProviderBase.qc Base class for ClickFunnels polling watch providers

/*  ClickFunnelsWatchDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace ClickFunnelsDataProvider {

#! Base class for ClickFunnels polling watch data providers
public class ClickFunnelsWatchDataProviderBase inherits ClickFunnelsDataProviderBase,
        DataProvider::AbstractTimestampWatchDataProvider {
    public {
        #! Constructor options for watch providers
        const WatchConstructorOptions = BaseConstructorOptions
                + ClickFunnelsDataProviderBase::WorkspaceIdSearchOption + {
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "How often to poll for new records (seconds)",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Polling interval in seconds (minimum " + MinPollInterval + ")",
                "default_value": DefaultPollInterval,
            },
            "start_date": <DataProviderOptionInfo>{
                "display_name": "Start Date",
                "short_desc": "Only detect records created after this date",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Only detect records created after this date/time",
            },
        };
    }

    #! Creates the object with a REST client
    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest, *hash<auto> options)
            : ClickFunnelsDataProviderBase(rest),
              AbstractTimestampWatchDataProvider(
                  max(options.poll_interval ?? DefaultPollInterval, MinPollInterval),
                  options.start_date) {
        if (options.workspace_id) {
            setWorkspaceId(options.workspace_id);
        }
    }

    #! Destructor - stops the polling thread
    destructor() {
        stopEventsIntern();
    }

    #! Cancels any in-flight REST request during shutdown
    private cancelPollOperation() {
        rest.close();
    }

    #! The upstream ordering is not guaranteed, so the watch orders records itself before delivery
    /** Records are sorted by their timestamp and delivered oldest-first, and the durable cursor
        advances only after a record has been delivered successfully.
    */
    string getSourceOrdering() {
        return WSO_UNKNOWN;
    }

    #! Returns the record's identity using the provider's own accessor
    private *string getWatchRecordId(hash<auto> record) {
        return getRecordId(record);
    }

    #! Returns the record's position in the feed using the provider's own accessor
    private *date getWatchRecordTimestamp(hash<auto> record) {
        return getRecordTimestamp(record);
    }

    #! Logs an error raised during a polling cycle
    private logPollError(hash<ExceptionInfo> ex) {
        log(LoggerLevel::ERROR, "Error polling %s: %s: %s", getEventName(), ex.err, ex.desc);
    }

    #! Logs an informational message about a polling cycle
    private logWatchInfo(string fmt, ...) {
        log(LoggerLevel::INFO, vsprintf(fmt, argv));
    }

    #! Returns the timestamp from a record for deduplication
    private *date getRecordTimestamp(hash<auto> record) {
        *string ts = record.created_at ?? record.updated_at;
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

    #! Returns example event data
    private auto getExampleEventDataImpl(string event_id) {
        try {
            list<hash<auto>> records = fetchRecords(now_us() - 30D);
            if (records) {
                return records[0];
            }
        } catch () {
        }
        return getFakeExampleData();
    }

    #! Returns fake example data
    private hash<auto> getFakeExampleData() {
        return {
            "id": 12345,
            "created_at": "2026-01-30T10:30:00Z",
            "updated_at": "2026-01-30T10:30:00Z",
        };
    }

    #! Returns the record ID as a string
    private string getRecordId(hash<auto> record) {
        return string(record.id);
    }

    #! Fetches records created/updated since the given date
    abstract private list<hash<auto>> fetchRecords(date since);

    #! Returns the event name for notifications
    abstract private string getEventName();

    #! Returns the data type for event records
    abstract private AbstractDataProviderType getRecordDataType();
}

} # namespace ClickFunnelsDataProvider
