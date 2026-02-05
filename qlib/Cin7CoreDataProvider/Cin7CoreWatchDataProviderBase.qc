# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreWatchDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the Cin7CoreDataProvider module
public namespace Cin7CoreDataProvider {

#! Watch constructor options for Cin7Core polling triggers
public const WatchConstructorOptions = Cin7CoreDataProviderBase::BaseConstructorOptions + {
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

#! Base class for Cin7Core watch/trigger data providers
public class Cin7CoreWatchDataProviderBase inherits Cin7CoreDataProviderBase,
        DataProvider::DelayedObservable {
    public {
        #! Constructor options for watch providers
        const WatchProviderConstructorOptions = WatchConstructorOptions;
    }

    private {
        #! Poll interval in seconds
        int poll_interval;

        #! Last poll time for filtering
        date since;

        #! Set of seen IDs for deduplication
        hash<string, bool> seen_ids = {};

        #! Whether the observable is running
        bool running = False;

        #! Background polling thread counter
        Counter thread_counter();
    }

    #! Creates the object with a REST client
    constructor(Cin7CoreRestClient::Cin7CoreRestClient rest, *hash<auto> options)
            : Cin7CoreDataProviderBase(rest) {
        initOptions(options);
    }

    #! Creates the object with a connection
    constructor(Cin7CoreRestClient::Cin7CoreRestConnection conn, *hash<auto> options)
            : Cin7CoreDataProviderBase(conn) {
        initOptions(options);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : Cin7CoreDataProviderBase(options) {
        initOptions(options);
    }

    #! Destructor
    destructor() {
        stopEventsIntern();
    }

    #! Stops event generation
    stopEvents() {
        stopEventsIntern();
    }

    #! Returns True if events are still being generated
    bool isActive() {
        return running;
    }

    #! Initialize poll options
    private initOptions(*hash<auto> options) {
        poll_interval = options.poll_interval ?? DefaultPollInterval;
        if (poll_interval < MinPollInterval) {
            poll_interval = MinPollInterval;
        }
        since = options.since ?? now_us();
    }

    #! Start the observer/polling
    observersReady() {
        if (running) {
            return;
        }
        running = True;
        thread_counter.inc();
        background pollLoop();
    }

    #! Internal method to stop events
    private stopEventsIntern() {
        running = False;
        thread_counter.waitForZero();
    }

    #! Main polling loop
    private pollLoop() {
        on_exit thread_counter.dec();

        while (running) {
            try {
                date poll_start = now_us();
                list<hash<auto>> records = pollOnce();

                foreach hash<auto> record in (records) {
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
            } catch (hash<ExceptionInfo> ex) {
                @debug(log(LoggerLevel::ERROR, "pollLoop error: %s: %s", ex.err, ex.desc));
            }

            # Sleep for poll interval
            int sleep_ms = poll_interval * 1000;
            while (running && sleep_ms > 0) {
                int sleep_chunk = min(1000, sleep_ms);
                usleep(sleep_chunk * 1000);
                sleep_ms -= sleep_chunk;
            }
        }
    }

    #! Poll once and return any new records - override in subclasses
    abstract list<hash<auto>> pollOnce();

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
            list<hash<auto>> records = pollOnce();
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

} # namespace Cin7CoreDataProvider
