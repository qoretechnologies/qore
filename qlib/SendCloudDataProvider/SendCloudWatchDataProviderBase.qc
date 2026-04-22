# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudWatchDataProviderBase.qc SendCloud polling watch base class

/*  SendCloudWatchDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SendCloudDataProvider module
public namespace SendCloudDataProvider {

#! Base class for polling-based SendCloud watch providers
public class SendCloudWatchDataProviderBase inherits SendCloudDataProviderBase,
        DataProvider::AbstractWatchDataProviderBase {
    public {
        #! Watch constructor options (extend with per-subclass options if needed)
        const WatchConstructorOptions = BaseConstructorOptions + {
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "Polling interval in seconds",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Polling interval in seconds (minimum `" + MinPollInterval + "`, default `"
                    + DefaultPollInterval + "`)",
                "default_value": DefaultPollInterval,
            },
            "start_date": <DataProviderOptionInfo>{
                "display_name": "Start Date",
                "short_desc": "Start watching from this date",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Only emit events for records updated on or after this timestamp "
                    "(default: now at construction time)",
            },
        };
    }

    private {
        #! Last check timestamp (updated each poll cycle)
        date last_check;
    }

    #! Creates the object from a RestClientIo client plus options
    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : SendCloudDataProviderBase(rest),
              AbstractWatchDataProviderBase(
                  max(options.poll_interval ?? DefaultPollInterval, MinPollInterval)) {
        last_check = options.start_date ?? now_us();
    }

    #! Cancels in-flight REST operations for graceful shutdown
    private cancelPollOperation() {
        if (rest) {
            rest.close();
        }
    }

    #! Polls for new/changed records and emits events
    private pollOnce() {
        try {
            list<hash<auto>> records = fetchRecords(last_check);
            date newest = last_check;
            foreach hash<auto> record in (records) {
                if (!shouldEmit(record)) {
                    continue;
                }
                *string ts_str = record{getTimestampField()};
                if (ts_str.val()) {
                    date ts = date(ts_str);
                    if (ts > newest) {
                        newest = ts;
                    }
                }
                notifyObservers(getEventName(), record);
            }
            if (newest > last_check) {
                last_check = newest;
            }
        } catch (hash<ExceptionInfo> ex) {
            error("Error polling for %s events: %s: %s", getEventName(), ex.err, ex.desc);
        }
    }

    #! Returns example event data for the given event
    private auto getExampleEventDataImpl(string event_id) {
        try {
            list<hash<auto>> records = fetchRecords(now_us() - 7D);
            if (records) {
                return records[0];
            }
        } catch () {
        }
        return getFakeExampleData();
    }

    #! Returns the event types supported by this provider
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            getEventName(): <DataProviderMessageInfo>{
                "desc": sprintf("Fired when a %s event is detected", getEventName()),
                "type": getRecordDataType(),
            },
        };
    }

    #! Fetches records from the API since the given timestamp
    /** Subclasses must override.
    */
    abstract private list<hash<auto>> fetchRecords(date since);

    #! Returns the event name (DPAT_EVENT action_val)
    abstract private string getEventName();

    #! Returns the record data type for this event
    abstract private AbstractDataProviderType getRecordDataType();

    #! Returns fake example data when the API is unreachable
    abstract private hash<auto> getFakeExampleData();

    #! Decides whether the given record should be emitted (for status-change filtering)
    /** Default: always emit (subclass can override for status-transition tracking).
    */
    private bool shouldEmit(hash<auto> record) {
        return True;
    }

    #! Returns the timestamp field name for filtering
    private string getTimestampField() {
        return "updated_at";
    }
}

} # namespace SendCloudDataProvider
