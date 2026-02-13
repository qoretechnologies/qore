# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquareWatchNewEventsDataProvider.qc Square Watch New Events trigger provider

/*  SquareWatchNewEventsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquareDataProvider module
public namespace SquareDataProvider {

#! Watch for new events in Square via the Events API (pull-based, 28-day retention)
public class SquareWatchNewEventsDataProvider inherits SquareWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_EVENTS,
            "desc": "Watch for new events in Square via the Events API",
            "type": "SquareWatchNewEventsDataProvider",
            "supports_observable": True,
            "constructor_options": WatchNewEventsConstructorOptions,
        };

        #! Constructor options specific to Watch New Events
        const WatchNewEventsConstructorOptions = WatchConstructorOptions + {
            "event_types": <DataProviderOptionInfo>{
                "display_name": "Event Types",
                "short_desc": "Square event types to watch",
                "type": AbstractDataProviderType::get(SoftListOrNothingType),
                "desc": "List of Square event types to watch (e.g. 'payment.created', "
                    "'order.updated'). Defaults to all supported event types. Supported types: "
                    + (foldl $1 + ", " + $2, AllSquareEventTypes),
            },
        };
    }

    private {
        #! Event types to filter for
        list<string> event_types;
    }

    #! Creates the object with a REST client
    constructor(SquareRestClient::SquareRestClient rest, *hash<auto> options)
            : SquareWatchDataProviderBase(rest, options) {
        if (options.event_types) {
            event_types = options.event_types;
        } else {
            event_types = AllSquareEventTypes;
        }
    }

    #! Returns the provider name
    string getName() { return EVENT_NEW_EVENTS; }

    #! Returns the provider description
    *string getDesc() {
        return "Watch for new events in Square via the Events API (pull-based, 28-day retention)";
    }

    #! Fetches new events since the given date via the Square Events API
    private list<hash<auto>> fetchRecords(date since) {
        string since_str = formatDateTimeSquare(since);
        hash<auto> body = {
            "query": {
                "filter": {
                    "event_types": event_types,
                    "created_at": {
                        "start_at": since_str,
                    },
                },
                "sort": {
                    "field": "CREATED_AT",
                    "order": "ASC",
                },
            },
        };

        # Paginate through all events
        list<hash<auto>> results = ();
        *string cursor;
        while (True) {
            hash<auto> req_body = body;
            if (cursor) {
                req_body.cursor = cursor;
            }
            hash<auto> response = doPost("events", req_body);
            if (response.events) {
                results += response.events;
            }
            if (results.size() >= 1000) {
                break;
            }
            cursor = response.cursor;
            if (!cursor) {
                break;
            }
        }
        return results;
    }

    #! Returns the record ID (use event_id for events)
    private string getRecordId(hash<auto> record) {
        return string(record.event_id ?? record.id);
    }

    #! Returns the event name
    private string getEventName() { return EVENT_NEW_EVENTS; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the data type for event records
    private AbstractDataProviderType getRecordDataType() { return new SquareWebhookEventDataType(); }

    #! Returns fake example data for events
    private hash<auto> getFakeExampleData() {
        return {
            "event_id": "EVENT_ID_12345",
            "event_type": "payment.created",
            "merchant_id": "MERCHANT_ID_12345",
            "location_id": "LOCATION_ID_12345",
            "created_at": "2026-01-30T10:30:00Z",
            "data": {
                "type": "payment",
                "id": "PAYMENT_ID_12345",
                "object": {"payment": {"id": "PAYMENT_ID_12345", "status": "COMPLETED"}},
            },
        };
    }
}

} # namespace SquareDataProvider
