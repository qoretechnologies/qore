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
        DataProvider::AbstractWatchDataProviderBase {
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

    private {
        #! Last check time
        date last_check;

        #! Set of seen IDs to avoid duplicates
        hash<string, bool> seen_ids;
    }

    #! Creates the object with a REST client
    constructor(FreshBooksRestClient::FreshBooksRestClient rest, *hash<auto> options)
            : FreshBooksDataProviderBase(rest),
              AbstractWatchDataProviderBase(max(options.poll_interval ?? DefaultPollInterval, MinPollInterval)) {
        if (options.account_id) {
            rest.setAccountId(options.account_id);
        }
        last_check = options.start_date ?? now_us();
    }

    #! Destructor - stops the polling thread
    destructor() {
        stopEventsIntern();
    }

    #! Polls for new records once
    private pollOnce() {
        try {
            # Get records modified since last check
            hash<auto> params = getQueryParams();

            hash<auto> response = doGet(getResourcePath(), params);
            list<auto> records = response{getResourceKey()} ?? ();

            date new_last_check = last_check;

            foreach hash<auto> record in (records) {
                # check for shutdown between iterations
                if (isStopping()) {
                    break;
                }
                string id = getRecordId(record);

                # Skip if already seen
                if (seen_ids{id}) {
                    continue;
                }

                # Check if this is a new record (created after last check)
                *string created_date = record.created_at ?? record.create_date;
                if (created_date) {
                    date record_time = date(created_date);
                    if (record_time <= last_check) {
                        continue;
                    }
                    if (record_time > new_last_check) {
                        new_last_check = record_time;
                    }
                }

                # Mark as seen
                seen_ids{id} = True;

                # Notify observers
                notifyObservers(getEventName(), record);
            }

            last_check = new_last_check;

            # Prune seen_ids if it gets too large (time-based filtering is primary dedup)
            if (seen_ids.size() > MaxSeenIds) {
                seen_ids = {};
            }
        } catch (hash<ExceptionInfo> ex) {
            # Log error but continue polling
            log(LoggerLevel::ERROR, "Error polling %s: %s: %s", getResourcePath(), ex.err, ex.desc);
        }
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
