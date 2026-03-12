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
        DataProvider::AbstractWatchDataProviderBase {
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

    private {
        #! Last check time for high-water mark
        date last_check;

        #! Set of seen event IDs to avoid duplicates
        hash<string, bool> seen_ids;
    }

    #! Creates the object with a REST client
    constructor(CloseRestClient::CloseRestClient rest, *hash<auto> options)
            : CloseDataProviderBase(rest),
              AbstractWatchDataProviderBase(max(options.poll_interval ?? DefaultPollInterval, MinPollInterval)) {
        last_check = options.start_date ?? now_us();
    }

    #! Destructor - stops the polling thread
    destructor() {
        stopEventsIntern();
    }

    #! Polls for new events once
    private pollOnce() {
        try {
            # Fetch events from Close Event Log API
            string since_str = CloseDataProviderBase::formatDateTime(last_check);
            hash<auto> params = {
                "object_type": getObjectType(),
                "action": getAction(),
                "date_updated__gte": since_str,
                "_limit": 50,
            };
            list<hash<auto>> events = fetchEventPages(params, 1000);

            # Close returns latest-first; reverse for correct watermark advancement
            events = reverse(events);

            date new_last_check = last_check;

            foreach hash<auto> event in (events) {
                # Check for shutdown between iterations
                if (isStopping()) {
                    break;
                }
                string id = string(event.id);

                # Skip if already seen
                if (seen_ids{id}) {
                    continue;
                }

                # Check timestamp to avoid old events
                *date event_time;
                if (event.date_updated) {
                    event_time = date(event.date_updated);
                }
                if (event_time) {
                    if (event_time <= last_check) {
                        continue;
                    }
                    if (event_time > new_last_check) {
                        new_last_check = event_time;
                    }
                }

                # Mark as seen
                seen_ids{id} = True;

                # Notify observers
                notifyObservers(getEventName(), event);
            }

            last_check = new_last_check;

            # Prune seen_ids if it gets too large
            if (seen_ids.size() > MaxSeenIds) {
                seen_ids = {};
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "Error polling %s: %s: %s", getEventName(), ex.err, ex.desc);
        }
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
