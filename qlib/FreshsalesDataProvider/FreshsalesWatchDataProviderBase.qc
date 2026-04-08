# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FreshsalesWatchDataProviderBase.qc Base class for Freshsales polling watch data providers

/*  FreshsalesWatchDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FreshsalesDataProvider module
public namespace FreshsalesDataProvider {

#! Base class for Freshsales polling watch data providers
public class FreshsalesWatchDataProviderBase inherits FreshsalesDataProviderBase,
        DataProvider::AbstractWatchDataProviderBase {
    public {
        #! Constructor options for watch providers
        const WatchConstructorOptions = BaseConstructorOptions + {
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "How often to poll for new records (seconds)",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Polling interval in seconds (minimum " + MinPollInterval + ")",
                "default_value": DefaultPollInterval,
            },
        };
    }

    private {
        #! Last check time
        date last_check;

        #! Set of seen IDs to avoid duplicates
        hash<string, bool> seen_ids;
    }

    #! Creates the object with a REST client (async)
    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : FreshsalesDataProviderBase(rest),
              AbstractWatchDataProviderBase(max(options.poll_interval ?? DefaultPollInterval,
                  MinPollInterval)) {
        last_check = now_us();
    }

    #! Destructor - stops the polling thread
    destructor() {
        stopEventsIntern();
    }

    #! Cancels any in-flight REST request during shutdown (critical for RestClientIo)
    private cancelPollOperation() {
        rest.close();
    }

    #! Polls for new records once
    private pollOnce() {
        try {
            list<hash<auto>> records = fetchRecords(last_check);
            date new_last_check = last_check;

            foreach hash<auto> record in (records) {
                if (isStopping()) {
                    break;
                }
                string id = string(record.id);

                # Skip if already seen
                if (seen_ids{id}) {
                    continue;
                }

                # Check timestamp to avoid old records
                *date record_time;
                *string ts = record{getTimestampField()};
                if (ts) {
                    record_time = date(ts);
                }
                if (record_time) {
                    if (record_time <= last_check) {
                        continue;
                    }
                    if (record_time > new_last_check) {
                        new_last_check = record_time;
                    }
                }

                seen_ids{id} = True;
                notifyObservers(getEventName(), record);
            }

            last_check = new_last_check;

            # Prune oldest half of seen_ids if it gets too large
            if (seen_ids.size() > 10000) {
                list<string> all_keys = keys seen_ids;
                int half = all_keys.size() / 2;
                foreach string k in (all_keys[0..half-1]) {
                    delete seen_ids{k};
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "Error polling %s: %s: %s", getEventName(), ex.err,
                ex.desc);
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
        try {
            list<hash<auto>> records = fetchRecords(now_us() - 30D);
            if (records) {
                return records[0];
            }
        } catch () {
        }
        return getFakeExampleData();
    }

    #! Returns fake example data specific to the resource type — must be overridden
    abstract hash<auto> getFakeExampleData();

    #! Returns the event name for this watch provider — must be overridden
    abstract string getEventName();

    #! Returns the timestamp field name — must be overridden
    abstract string getTimestampField();

    #! Returns the data type for event records — must be overridden
    abstract AbstractDataProviderType getRecordDataType();

    #! Fetches records newer than the given date — must be overridden
    abstract list<hash<auto>> fetchRecords(date last_check);
}

} # namespace FreshsalesDataProvider
