# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FreshBooksDataProvider module definition

/*  FreshBooksWatchDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FreshBooksDataProvider module
public namespace FreshBooksDataProvider {

#! Base class for FreshBooks polling watch data providers
public class FreshBooksWatchDataProviderBase inherits FreshBooksDataProviderBase,
        DataProvider::AbstractTimestampWatchDataProvider {
    public {
        #! Maximum number of seen IDs to track before pruning
        const MaxSeenIds = 10000;

        #! Constructor options for watch providers
        const WatchConstructorOptions = BaseConstructorOptions + AccountIdSearchOption + {
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "How often to poll for new records (seconds)",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Polling interval in seconds (minimum 30)",
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
    constructor(FreshBooksRestClient::FreshBooksRestClient rest, *hash<auto> options)
            : FreshBooksDataProviderBase(rest),
              AbstractTimestampWatchDataProvider(max(options.poll_interval ?? DefaultPollInterval, MinPollInterval),
                  options.start_date) {
        if (options.account_id) {
            rest.setAccountId(options.account_id);
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

    #! The upstream ordering is not relied on; records are ordered by timestamp before delivery
    /** Records are sorted by their timestamp and delivered oldest-first, and the durable cursor
        advances only after a record has been delivered successfully.
    */
    string getSourceOrdering() {
        return WSO_UNKNOWN;
    }

    #! Fetches every record changed at or after the given timestamp
    private list<hash<auto>> fetchRecords(date since) {
        hash<auto> response = doGet(getResourcePath(), getQueryParams());
        return response{getResourceKey()} ?? ();
    }

    #! Returns the record's identity using the provider's own accessor
    private *string getWatchRecordId(hash<auto> record) {
        return getRecordId(record);
    }

    #! Logs an error raised during a polling cycle
    private logPollError(hash<ExceptionInfo> ex) {
        log(LoggerLevel::ERROR, "Error polling %s: %s: %s", getEventName(), ex.err, ex.desc);
    }

    #! Logs an informational message about a polling cycle
    private logWatchInfo(string fmt, ...) {
        log(LoggerLevel::INFO, "%s", vsprintf(fmt, argv));
    }

    #! Returns query parameters for the poll request
    /** Subclasses can override to provide resource-specific query parameters
    */
    private hash<auto> getQueryParams() {
        hash<auto> params;
        params."per_page" = DefaultPageSize;
        params."page" = 1;
        # Add date filter if we have a last check time
        if (last_check) {
            params."search[date_from]" = format_date("YYYY-MM-DD", last_check);
        }
        return params;
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
    /** Tries to fetch a real record from the API first; falls back to fake example data if no records exist
    */
    private auto getExampleEventDataImpl(string event_id) {
        # Try to get a real record from the API
        try {
            hash<auto> params = {
                "per_page": 1,
                "page": 1,
            };
            hash<auto> response = doGet(getResourcePath(), params);
            list<auto> records = response{getResourceKey()} ?? ();
            if (records) {
                return records[0];
            }
        } catch () {
            # Fall through to fake data if API call fails
        }
        # Fall back to fake example data
        return getFakeExampleData();
    }

    #! Returns fake example data when no real records are available
    /** Subclasses should override to provide resource-specific example fields
    */
    private hash<auto> getFakeExampleData() {
        return {
            "id": 123456,
            "created_at": "2026-01-30 10:30:00",
            "updated": "2026-01-30 10:30:00",
        };
    }

    #! Returns the API path for this resource
    abstract private string getResourcePath();

    #! Returns the key in the response containing the records
    abstract private string getResourceKey();

    #! Returns the record ID as a string
    abstract private string getRecordId(hash<auto> record);

    #! Returns the event name for notifications
    abstract private string getEventName();

    #! Returns the data type for event records
    /** Subclasses must override to return the resource-specific record data type
    */
    abstract private AbstractDataProviderType getRecordDataType();
}

} # namespace FreshBooksDataProvider
