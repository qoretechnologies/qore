# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GorgiasWatchDataProviderBase.qc Base class for Gorgias polling watch data providers

/*  GorgiasWatchDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GorgiasDataProvider module
public namespace GorgiasDataProvider {

#! Base class for Gorgias polling watch data providers
public class GorgiasWatchDataProviderBase inherits GorgiasDataProviderBase,
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
            : GorgiasDataProviderBase(rest),
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

    #! Returns fake example data when no real records are available
    private hash<auto> getFakeExampleData() {
        return {
            "id": 12345,
            "uri": "/api/tickets/12345",
            "external_id": NOTHING,
            "subject": "Order not received - #98765",
            "status": "open",
            "priority": "normal",
            "channel": "email",
            "via": "helpdesk",
            "from_agent": False,
            "spam": False,
            "language": "en",
            "is_unread": True,
            "customer": {
                "id": 67890,
                "email": "jane.doe@example.com",
                "name": "Jane Doe",
            },
            "assignee_user": {
                "id": 111,
                "email": "agent@company.com",
                "name": "Support Agent",
            },
            "assignee_team": NOTHING,
            "tags": ({
                "id": 42,
                "name": "shipping",
            },),
            "messages": (),
            "meta": NOTHING,
            "custom_fields": NOTHING,
            "created_datetime": "2026-01-30T10:30:00+00:00",
            "opened_datetime": "2026-01-30T10:30:00+00:00",
            "updated_datetime": "2026-01-30T10:35:00+00:00",
            "closed_datetime": NOTHING,
            "last_message_datetime": "2026-01-30T10:30:00+00:00",
            "last_received_message_datetime": "2026-01-30T10:30:00+00:00",
            "snooze_datetime": NOTHING,
            "trashed_datetime": NOTHING,
            "satisfaction_survey": NOTHING,
        };
    }

    #! Returns the timestamp field to use for deduplication
    private string getTimestampField() {
        return "created_datetime";
    }

    #! Fetches records since the given date
    abstract private list<hash<auto>> fetchRecords(date since);

    #! Returns the event name for notifications
    abstract private string getEventName();

    #! Returns the data type for event records
    abstract private AbstractDataProviderType getRecordDataType();
}

} # namespace GorgiasDataProvider
