# -*- mode: qore; indent-tabs-mode: nil -*-

/*  SmartSheetWatchDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace SmartSheetDataProvider {

#! Base class for Smartsheet polling watch data providers
/** Uses the Smartsheet Events API (GET /events) to poll for changes. Events are fetched
    using a stream position cursor to avoid missing events.
*/
public class SmartSheetWatchDataProviderBase inherits SmartSheetDataProviderBase,
        DataProvider::AbstractWatchDataProviderBase {
    public {
        #! Constructor options for watch providers
        const WatchConstructorOptions = BaseConstructorOptions + {
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "How often to poll for events (seconds)",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Polling interval in seconds (minimum " + MinPollInterval + "s). "
                    "Smartsheet rate limit is 300 requests per minute",
                "default_value": DefaultPollInterval,
            },
            "sheetId": <DataProviderOptionInfo>{
                "display_name": "Sheet ID",
                "short_desc": "Sheet to watch for events",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The ID of the sheet to watch for events. Required for row-level "
                    "event providers",
                "ref_data": "sheets",
            },
        };
    }

    private {
        #! Stream position for event cursor
        *string stream_position;

        #! Sheet ID to watch (if applicable)
        *string sheet_id;

        #! Event type filter
        string event_type;
    }

    #! Creates the object
    constructor(RestClientIo::RestClientIo rest, string event_type, *hash<auto> options)
            : SmartSheetDataProviderBase(rest),
              AbstractWatchDataProviderBase(max(options.poll_interval ?? DefaultPollInterval,
                  MinPollInterval)) {
        self.event_type = event_type;
        self.sheet_id = options.sheetId;
    }

    destructor() {
        stopEventsIntern();
    }

    #! Cancels in-flight REST request during shutdown
    private cancelPollOperation() {
        rest.close();
    }

    #! Polls for new events once
    private pollOnce() {
        try {
            hash<auto> params;
            # since and streamPosition are mutually exclusive per Smartsheet API docs
            if (stream_position) {
                params.streamPosition = stream_position;
            } else {
                params.since = format_date("YYYY-MM-DDTHH:mm:ss.000Z", now_us() - 5m);
            }

            hash<auto> response = doGet("events", params);

            # Update stream position for next poll
            if (response.nextStreamPosition) {
                stream_position = response.nextStreamPosition;
            }

            if (response.data) {
                foreach hash<auto> event in (response.data) {
                    if (isStopping()) {
                        break;
                    }
                    # Filter by event type and object type
                    if (!matchesEvent(event)) {
                        continue;
                    }
                    # Enrich the event with full object data if possible
                    hash<auto> enriched = enrichEvent(event);
                    notifyObservers(event_type, enriched);
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            if (isStopping()) {
                return;
            }
            log(LoggerLevel::ERROR, "Error polling Smartsheet events: %s: %s",
                ex.err, ex.desc);
        }
    }

    #! Returns whether an event matches this provider's filter
    /** Override in subclasses to filter by specific event/object types.
        Subclasses should call this base method first to check the sheet_id filter.
    */
    private bool matchesEvent(hash<auto> event) {
        # Filter by sheet ID if configured
        if (sheet_id) {
            # For row-level events, sheetId is in additionalDetails
            *string event_sheet_id = event.additionalDetails.sheetId
                ? string(event.additionalDetails.sheetId)
                : NOTHING;
            # For sheet-level events, objectId is the sheet ID
            if (event.objectType == "SHEET") {
                event_sheet_id = string(event.objectId);
            }
            if (event_sheet_id != sheet_id) {
                return False;
            }
        }
        return True;
    }

    #! Enriches an event with full object data
    /** Override in subclasses to fetch full data for the event's object
    */
    private hash<auto> enrichEvent(hash<auto> event) {
        return event;
    }
}

} # namespace SmartSheetDataProvider
