# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CloseWatchDataProviderBase.qc Base class for Close polling watch data providers

/*  CloseWatchDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CloseDataProvider module
public namespace CloseDataProvider {

#! Base class for Close polling watch data providers using the Event Log API
public class CloseWatchDataProviderBase inherits CloseDataProviderBase,
        DataProvider::AbstractTimestampWatchDataProvider {
    public {
        #! Constructor options for watch providers
        const WatchConstructorOptions = BaseConstructorOptions + {
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "How often to poll for new events (seconds)",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Polling interval in seconds (minimum " + MinPollInterval + ")",
                "default_value": DefaultPollInterval,
            },
            "start_date": <DataProviderOptionInfo>{
                "display_name": "Start Date",
                "short_desc": "Only detect events after this date",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Only detect events after this date/time",
            },
        };
    }

    #! Creates the object with a REST client
    constructor(CloseRestClient::CloseRestClient rest, *hash<auto> options)
            : CloseDataProviderBase(rest),
              AbstractTimestampWatchDataProvider(max(options.poll_interval ?? DefaultPollInterval, MinPollInterval),
                  options.start_date) {
    }

    #! Destructor - stops the polling thread
    destructor() {
        stopEventsIntern();
    }

    #! Cancels any in-flight REST request during shutdown
    private cancelPollOperation() {
        rest.close();
    }

    #! The upstream ordering is not relied on; records are ordered by timestamp before delivery
    /** Records are sorted by their timestamp and delivered oldest-first, and the durable cursor
        advances only after a record has been delivered successfully.
    */
    string getSourceOrdering() {
        return WSO_DESCENDING;
    }

    #! Fetches every record changed at or after the given timestamp
    private list<hash<auto>> fetchRecords(date since) {
        hash<auto> params = {
            "object_type": getObjectType(),
            "action": getAction(),
            "date_updated__gte": CloseDataProviderBase::formatDateTime(since),
            "_limit": 50,
        };
        return fetchEventPages(params, 1000);
    }

    #! Returns the record field holding the record's position in the feed
    private string getTimestampField() {
        return "date_updated";
    }

    #! Logs an error raised during a polling cycle
    private logPollError(hash<ExceptionInfo> ex) {
        log(LoggerLevel::ERROR, "Error polling %s: %s: %s", getEventName(), ex.err, ex.desc);
    }

    #! Logs an informational message about a polling cycle
    private logWatchInfo(string fmt, ...) {
        log(LoggerLevel::INFO, vsprintf(fmt, argv));
    }

    #! Returns a hash of all supported event types
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            getEventName(): <DataProviderMessageInfo>{
                "desc": sprintf("Fired when a %s event is detected", getEventName()),
                "type": getRecordDataType(),
            },
        };
    }

    #! Returns example event data for documentation/testing
    private auto getExampleEventDataImpl(string event_id) {
        # Try to get a real event from the API
        try {
            string since_str = CloseDataProviderBase::formatDateTime(now_us() - 30D);
            hash<auto> params = {
                "object_type": getObjectType(),
                "action": getAction(),
                "date_updated__gte": since_str,
                "_limit": 1,
            };
            hash<auto> response = doGet("event", params);
            if (response.data && response.data.size() > 0) {
                return response.data[0];
            }
        } catch () {
            # Fall through to fake data if API call fails
        }
        # Fall back to fake example data
        return getFakeExampleData();
    }

    #! Returns fake example data when no real events are available
    private hash<auto> getFakeExampleData() {
        return {
            "id": "ev_EXAMPLE12345",
            "object_type": getObjectType(),
            "object_id": "EXAMPLE_OBJ_12345",
            "action": getAction(),
            "lead_id": "lead_EXAMPLE12345",
            "user_id": "user_EXAMPLE12345",
            "date_created": "2026-01-30T10:30:00Z",
            "date_updated": "2026-01-30T10:30:00Z",
            "data": {},
            "previous_data": NOTHING,
            "changed_fields": NOTHING,
            "meta": {"request_method": "POST"},
            "organization_id": "orga_EXAMPLE12345",
            "request_id": "req_EXAMPLE12345",
        };
    }

    #! Returns the Close object_type for the Event Log API
    abstract private string getObjectType();

    #! Returns the Close action for the Event Log API
    abstract private string getAction();

    #! Returns the event name for notifications
    abstract private string getEventName();

    #! Returns the data type for event records
    abstract private AbstractDataProviderType getRecordDataType();
}

} # namespace CloseDataProvider
