# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksWatchDataProviderBase class definition

/*  DatabricksWatchDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DatabricksDataProvider module
public namespace DatabricksDataProvider {

#! Base class for Databricks polling watch data providers
public class DatabricksWatchDataProviderBase inherits DatabricksDataProviderBase,
        DataProvider::DelayedObservable {
    public {
        #! Maximum number of seen IDs to track before pruning
        const MaxSeenIds = 10000;

        #! Constructor options for watch providers
        const WatchConstructorOptions = BaseConstructorOptions + {
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "How often to poll for changes (seconds)",
                "type": AbstractDataProviderType::get(SoftIntType),
                "desc": "Polling interval in seconds (minimum 30)",
                "default_value": DefaultPollInterval,
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

        #! Set of seen IDs to avoid duplicates, mapped to insertion sequence number
        hash<string, int> seen_ids;

        #! Monotonically increasing sequence counter for seen_ids ordering
        int seen_seq = 0;
    }

    #! Creates the object with a REST client and options
    constructor(DatabricksRestClient::DatabricksRestClient rest, *hash<auto> options)
            : DatabricksDataProviderBase(rest) {
        poll_secs = options.poll_interval ?? DefaultPollInterval;
        if (poll_secs < MinPollInterval) {
            poll_secs = MinPollInterval;
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
        while (io_tid) {
            cond.wait(lck);
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

        while (True) {
            {
                lck.lock();
                on_exit lck.unlock();
                if (stop) {
                    break;
                }
            }

            pollOnce();

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

    #! Prunes seen_ids if it grows too large, keeping the most recent half
    private pruneSeenIds() {
        if (seen_ids.size() > MaxSeenIds) {
            # Find the median sequence number and keep only entries above it
            int cutoff = seen_seq - (MaxSeenIds / 2);
            hash<string, int> new_ids;
            foreach string key in (keys seen_ids) {
                if (seen_ids{key} > cutoff) {
                    new_ids{key} = seen_ids{key};
                }
            }
            seen_ids = new_ids;
        }
    }

    #! Returns a hash of all supported event types
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            getEventName(): <DataProviderMessageInfo>{
                "desc": getEventDesc(),
                "type": AbstractDataProviderType::get(AutoHashType),
            },
        };
    }

    #! Returns example event data
    private auto getExampleEventDataImpl(string event_id) {
        return getFakeExampleData();
    }

    #! Polls for changes once — subclasses must implement
    abstract private pollOnce();

    #! Returns the event name
    abstract private string getEventName();

    #! Returns the event description
    abstract private string getEventDesc();

    #! Returns fake example data
    abstract private hash<auto> getFakeExampleData();
}

} # namespace DatabricksDataProvider
