# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file TableauPollEventDataProviderBase.qc Base class for Tableau polling event data providers

/*  TableauPollEventDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the TableauDataProvider module
public namespace TableauDataProvider {

#! Base class for Tableau polling event data providers
public class TableauPollEventDataProviderBase inherits TableauDataProviderBase,
        DataProvider::DelayedObservable {
    public {
        #! Constructor options for polling watch providers
        const WatchConstructorOptions = BaseConstructorOptions + {
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "How often to poll for new records (seconds)",
                "type": AbstractDataProviderType::get(SoftIntType),
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

    private {
        #! Mutex for thread synchronization
        Mutex lck();

        #! Condition variable for sleeping
        Condition cond();

        #! Background thread ID
        int io_tid;

        #! Stop flag
        bool stop = False;

        #! Poll interval in seconds
        int poll_secs;

        #! Start date for filtering
        date start_date;

        #! Last check time
        date last_check;

        #! Set of seen IDs to avoid duplicates
        hash<string, bool> seen_ids;
    }

    #! Creates the object with a REST client
    constructor(TableauRestClient::TableauRestClient rest, *hash<auto> options)
            : TableauDataProviderBase(rest) {
        poll_secs = options.poll_interval ?? DefaultPollInterval;
        if (poll_secs < MinPollInterval) {
            poll_secs = MinPollInterval;
        }
        if (options.start_date) {
            start_date = options.start_date;
        }
    }

    #! Destructor - stops the polling thread
    destructor() {
        stopEventsIntern();
    }

    #! Called when observers are ready
    observersReady() {
        lck.lock();
        on_exit lck.unlock();
        if (io_tid) {
            return;
        }
        io_tid = background pollThread();
    }

    #! Stops event generation
    stopEvents() {
        stopEventsIntern();
    }

    #! Internal method to stop events
    private stopEventsIntern() {
        lck.lock();
        on_exit lck.unlock();
        stop = True;
        cond.broadcast();
        # Wait in a loop until the polling thread exits
        while (io_tid) {
            cond.wait(lck, 5s);
        }
    }

    #! Returns True if events are still being generated
    bool isActive() {
        return !stop && io_tid > 0;
    }

    #! The polling thread
    private pollThread() {
        on_exit {
            lck.lock();
            on_exit lck.unlock();
            remove io_tid;
            cond.broadcast();
        }

        # Initialize tracking state
        last_check = start_date ?? now_us();

        while (True) {
            # Check stop flag under lock
            {
                lck.lock();
                on_exit lck.unlock();
                if (stop) {
                    break;
                }
            }

            pollOnce();

            # Wait for next poll interval under lock
            {
                lck.lock();
                on_exit lck.unlock();
                if (stop) {
                    break;
                }
                cond.wait(lck, poll_secs * 1000ms);
            }
        }
    }

    #! Polls for new records once
    private pollOnce() {
        try {
            list<hash<auto>> records = fetchRecords(last_check);
            date new_last_check = last_check;

            foreach hash<auto> record in (records) {
                string id = getRecordId(record);

                # Skip if already seen
                if (seen_ids{id}) {
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

                # Mark as seen
                seen_ids{id} = True;

                # Notify observers
                notifyObservers(getEventName(), record);
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

    #! Returns the timestamp from a record for deduplication
    private *date getRecordTimestamp(hash<auto> record) {
        *string ts = record.updatedAt ?? record.createdAt;
        if (ts) {
            return date(ts);
        }
    }

    #! Returns the record ID as a string
    private string getRecordId(hash<auto> record) {
        return string(record.id);
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
        # Try to get a real record from the API
        try {
            list<hash<auto>> records = fetchRecords(now_us() - 30D);
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
    private hash<auto> getFakeExampleData() {
        return {
            "id": "EXAMPLE_ID_12345",
            "createdAt": "2026-01-30T10:30:00Z",
            "updatedAt": "2026-01-30T10:30:00Z",
        };
    }

    #! Fetches records created/updated since the given date
    /** Subclasses must implement this to poll their specific Tableau API endpoint
    */
    abstract private list<hash<auto>> fetchRecords(date since);

    #! Returns the event name for notifications
    abstract private string getEventName();

    #! Returns the data type for event records
    abstract private AbstractDataProviderType getRecordDataType();
}

} # namespace TableauDataProvider
