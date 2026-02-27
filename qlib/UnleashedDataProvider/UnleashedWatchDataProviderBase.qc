# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnleashedDataProvider module definition

/*  UnleashedWatchDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the UnleashedDataProvider module
public namespace UnleashedDataProvider {

#! Watch constructor options for Unleashed polling triggers
public const WatchConstructorOptions = UnleashedDataProviderBase::BaseConstructorOptions + {
    "poll_interval": <DataProviderOptionInfo>{
        "display_name": "Poll Interval",
        "short_desc": "Polling interval in seconds",
        "type": AbstractDataProviderType::get(IntType),
        "desc": "The polling interval in seconds (minimum " + MinPollInterval + ", default " + DefaultPollInterval + ")",
        "default_value": DefaultPollInterval,
        "required": True,
    },
    "since": <DataProviderOptionInfo>{
        "display_name": "Since",
        "short_desc": "Return records modified since this date",
        "type": AbstractDataProviderType::get(DateOrNothingType),
        "desc": "Only return records modified after this date/time. If not provided, starts from now.",
    },
};

#! Base class for Unleashed watch/trigger data providers
public class UnleashedWatchDataProviderBase inherits UnleashedDataProviderBase,
        DataProvider::AbstractWatchDataProviderBase {
    public {
        #! Constructor options for watch providers
        const WatchProviderConstructorOptions = WatchConstructorOptions;
    }

    private {
        #! Last poll time for filtering
        date since;

        #! Set of seen IDs for deduplication
        hash<string, bool> seen_ids = {};
    }

    #! Creates the object with a REST client
    constructor(UnleashedRestClient::UnleashedRestClient rest, *hash<auto> options)
            : UnleashedDataProviderBase(rest),
              AbstractWatchDataProviderBase(max(options.poll_interval ?? DefaultPollInterval, MinPollInterval)) {
        since = options.since ?? now_us();
    }

    #! Creates the object with a connection
    constructor(UnleashedRestClient::UnleashedRestConnection conn, *hash<auto> options)
            : UnleashedDataProviderBase(conn),
              AbstractWatchDataProviderBase(max(options.poll_interval ?? DefaultPollInterval, MinPollInterval)) {
        since = options.since ?? now_us();
    }

    #! Creates the object with options
    constructor(*hash<auto> options)
            : UnleashedDataProviderBase(options),
              AbstractWatchDataProviderBase(max(options.poll_interval ?? DefaultPollInterval, MinPollInterval)) {
        since = options.since ?? now_us();
    }

    #! Destructor
    destructor() {
        stopEventsIntern();
    }

    #! Performs a single polling operation
    private pollOnce() {
        date poll_start = now_us();
        list<hash<auto>> records = pollOnceImpl();

        foreach hash<auto> record in (records) {
            if (isStopping()) {
                break;
            }
            string id = getRecordId(record);
            if (!seen_ids{id}) {
                seen_ids{id} = True;
                notifyObservers(getEventName(), record);
            }
        }

        # Update since time for next poll
        since = poll_start;

        # Clean up old seen_ids periodically (keep last 10000)
        if (seen_ids.size() > 10000) {
            # Remove oldest half
            int count = 0;
            foreach string key in (keys seen_ids) {
                if (count++ < 5000) {
                    remove seen_ids{key};
                } else {
                    break;
                }
            }
        }
    }

    #! Called when a polling error occurs
    private logPollError(hash<ExceptionInfo> ex) {
        @debug(log(LoggerLevel::ERROR, "pollLoop error: %s: %s", ex.err, ex.desc));
    }

    #! Poll once and return any new records - override in subclasses
    abstract list<hash<auto>> pollOnceImpl();

    #! Get the unique ID from a record - override in subclasses
    abstract string getRecordId(hash<auto> record);

    #! Get the event name - override in subclasses
    abstract string getEventName();

    #! Get the event data type - override in subclasses
    abstract AbstractDataProviderType getEventDataType();

    #! Returns the event types supported by this provider
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            getEventName(): <DataProviderMessageInfo>{
                "desc": getDesc(),
                "type": getEventDataType(),
            },
        };
    }

    #! Returns example event data - tries real API data first, falls back to fake data
    private auto getExampleEventDataImpl(string event_id) {
        # Try to get real data from the API first
        try {
            list<hash<auto>> records = pollOnceImpl();
            if (records.size() > 0) {
                return records[0];
            }
        } catch () {
            # Fall through to fake data
        }
        # Return resource-specific fake data
        return getFakeExampleData();
    }

    #! Returns resource-specific fake example data - override in subclasses
    abstract hash<auto> getFakeExampleData();
}

} # namespace UnleashedDataProvider
