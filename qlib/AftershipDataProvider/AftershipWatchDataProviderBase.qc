# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AftershipDataProvider module definition

/** AftershipWatchDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the AftershipDataProvider module
public namespace AftershipDataProvider {
#! Base class for Aftership polling watch providers
public class AftershipWatchDataProviderBase inherits AftershipDataProviderBase,
        DataProvider::AbstractWatchDataProviderBase {
    public {
        #! Watch constructor options
        const WatchConstructorOptions = ConstructorOptions + {
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "Polling interval in seconds",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Polling interval in seconds (minimum `" + MinPollInterval + "`, default `"
                    + DefaultPollInterval + "`)",
                "default_value": DefaultPollInterval,
            },
        };

        #! Maximum number of seen IDs to track before pruning
        const MaxSeenIds = 10000;
    }

    private {
        #! Last check timestamp
        date last_check;

        #! Set of seen record IDs for deduplication
        hash<string, bool> seen_ids;
    }

    #! Creates the object from a RestClientIo client
    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : AftershipDataProviderBase(rest),
              AbstractWatchDataProviderBase(
                  max(options.poll_interval ?? DefaultPollInterval, MinPollInterval)) {
        last_check = now_us();
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options)
            : AftershipDataProviderBase(options),
              AbstractWatchDataProviderBase(
                  max(options.poll_interval ?? DefaultPollInterval, MinPollInterval)) {
        last_check = now_us();
    }

    #! Cancels in-flight REST operations for graceful shutdown
    private cancelPollOperation() {
        rest.close();
    }

    #! Polls for new/changed records
    private pollOnce() {
        try {
            list<hash<auto>> records = fetchRecords(last_check);
            date newest = last_check;
            foreach hash<auto> record in (records) {
                string record_id = getRecordId(record);
                # skip already-seen records
                if (seen_ids{record_id}) {
                    continue;
                }
                # check timestamp
                *string ts_str = record{getTimestampField()};
                if (ts_str.val()) {
                    date ts = date(ts_str);
                    if (ts <= last_check) {
                        continue;
                    }
                    if (ts > newest) {
                        newest = ts;
                    }
                }
                seen_ids{record_id} = True;
                notifyObservers(getEventName(), record);
            }
            if (newest > last_check) {
                last_check = newest;
            }
            # prune seen_ids if too large
            if (seen_ids.size() > MaxSeenIds) {
                seen_ids = {};
            }
        } catch (hash<ExceptionInfo> ex) {
            error("Error polling for %s events: %s: %s", getEventName(), ex.err, ex.desc);
        }
    }

    #! Returns example event data
    private auto getExampleEventDataImpl(string event_id) {
        try {
            list<hash<auto>> records = fetchRecords(now_us() - 30D);
            if (records) {
                return records[0];
            }
        } catch () {
        }
        return getFakeExampleData();
    }

    #! Returns fake example data when no real records are available
    abstract private hash<auto> getFakeExampleData();

    #! Returns the event types
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            getEventName(): <DataProviderMessageInfo>{
                "desc": sprintf("Fired when a %s event is detected", getEventName()),
                "type": getRecordDataType(),
            },
        };
    }

    #! Fetches records from the API since the given timestamp
    abstract private list<hash<auto>> fetchRecords(date since);

    #! Returns the event name
    abstract private string getEventName();

    #! Returns the record data type
    abstract private AbstractDataProviderType getRecordDataType();

    #! Returns the ID of a record for deduplication
    private string getRecordId(hash<auto> record) {
        return record.id ?? record.tracking_number ?? "";
    }

    #! Returns the timestamp field name for filtering
    private string getTimestampField() {
        return "updated_at";
    }
}
}
