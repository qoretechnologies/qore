# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoInvoiceDataProvider module definition

/*  ZohoInvoiceWatchDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ZohoInvoiceDataProvider module
public namespace ZohoInvoiceDataProvider {

#! Base class for Zoho Invoice polling watch data providers
public class ZohoInvoiceWatchDataProviderBase inherits ZohoInvoiceDataProviderBase,
        DataProvider::DelayedObservable {
    public {
        #! Constructor options for watch providers
        const WatchConstructorOptions = BaseConstructorOptions + {
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "How often to poll for new records (seconds)",
                "type": AbstractDataProviderType::get(SoftIntType),
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
    constructor(ZohoInvoiceRestClient::ZohoInvoiceRestClient rest, *hash<auto> options)
            : ZohoInvoiceDataProviderBase(rest) {
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
        if (io_tid) {
            cond.wait(lck, 30s);
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

        while (!stop) {
            pollOnce();

            # Wait for next poll interval
            lck.lock();
            on_exit lck.unlock();
            if (!stop) {
                cond.wait(lck, poll_secs * 1000ms);
            }
        }
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
                string id = string(record{getIdField()});

                # Skip if already seen
                if (seen_ids{id}) {
                    continue;
                }

                # Check if this is a new record (created after last check)
                *string created_time = record.created_time;
                if (created_time) {
                    date record_time = date(created_time);
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
        } catch (hash<ExceptionInfo> ex) {
            # Log error but continue polling
            log(LoggerLevel::ERROR, "Error polling %s: %s: %s", getResourcePath(), ex.err, ex.desc);
        }
    }

    #! Returns query parameters for the poll request
    private hash<auto> getQueryParams() {
        return {
            "sort_column": "created_time",
            "sort_order": "A",
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

} # namespace ZohoInvoiceDataProvider
