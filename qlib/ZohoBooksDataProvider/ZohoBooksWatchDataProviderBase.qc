# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoBooksDataProvider module definition

/*  ZohoBooksWatchDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ZohoBooksDataProvider module
public namespace ZohoBooksDataProvider {

#! Base class for Zoho Books polling watch data providers
public class ZohoBooksWatchDataProviderBase inherits ZohoBooksDataProviderBase,
        DataProvider::AbstractTimestampWatchDataProvider {
    public {
        #! Maximum number of seen IDs to track before pruning
        const MaxSeenIds = 10000;

        #! Constructor options for watch providers
        const WatchConstructorOptions = BaseConstructorOptions + {
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
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest, *hash<auto> options)
            : ZohoBooksDataProviderBase(rest),
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

    #! The API is queried newest-first; records are reversed into delivery order
    /** Records are sorted by their timestamp and delivered oldest-first, and the durable cursor
        advances only after a record has been delivered successfully. The query issued by
        @ref getQueryParams() must keep the newest-first order this declares, because
        @ref fetchRecords() relies on it to know when it has paged back to the cursor.
    */
    string getSourceOrdering() {
        return WSO_DESCENDING;
    }

    #! Fetches every record created at or after the given timestamp, following pagination
    /** The API returns records newest-first, so the walk stops as soon as a page reaches a record at
        or before the cursor: every remaining page is older still.

        Walking the pages matters now that the cursor is durable. Returning only the newest page
        would let the cursor advance past older records that were never fetched, dropping them
        permanently rather than merely delaying them.

        @note if shutdown interrupts the walk the partial result is discarded, because a
        newest-first prefix is not safe to deliver: doing so would advance the cursor past the
        records the walk had not reached yet
    */
    private list<hash<auto>> fetchRecords(date since) {
        list<hash<auto>> records = ();
        int page = 1;
        while (True) {
            hash<auto> response = doGet(getResourcePath(), getQueryParams() + {"page": page});
            list<auto> batch = response{getResourceKey()} ?? ();
            records += batch;
            if (!batch || !response.page_context.has_more_page || reachedCursor(batch, since)) {
                break;
            }
            if (isStopping()) {
                return ();
            }
            ++page;
        }
        return records;
    }

    #! Returns @ref True if the page reaches back to the cursor
    private bool reachedCursor(list<auto> batch, date since) {
        foreach hash<auto> record in (batch) {
            *date record_date = getWatchRecordTimestamp(record);
            if (exists record_date && record_date <= since) {
                return True;
            }
        }
        return False;
    }

    #! Returns the record field holding the record's position in the feed
    private string getTimestampField() {
        return "created_time";
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
    private hash<auto> getQueryParams() {
        return {
            "sort_column": "created_time",
            "sort_order": "D",
            "per_page": 200,
        };
    }

    #! Returns a hash of all supported event types
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            getEventName(): <DataProviderMessageInfo>{
                "desc": sprintf("A new %s has been created", getResourceKey()),
                "type": AbstractDataProviderType::get(AutoHashType),
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
                "sort_column": "created_time",
                "sort_order": "D",
                "page": 1,
                "per_page": 1,
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
            getIdField(): "123456789012345678",
            "created_time": "2026-01-30T10:30:00+00:00",
            "last_modified_time": "2026-01-30T10:30:00+00:00",
        };
    }

    #! Returns the API path for this resource
    abstract private string getResourcePath();

    #! Returns the key in the response containing the records
    abstract private string getResourceKey();

    #! Returns the field name for the record ID
    abstract private string getIdField();

    #! Returns the event name for notifications
    abstract private string getEventName();
}

} # namespace ZohoBooksDataProvider
