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
    #! The Smartsheet event stream is returned in ascending event order
    /** The stream position is an opaque cursor, so this provider is a documented exemption from the
        shared ordered template (@ref DataProvider::AbstractOrderedWatchDataProvider): there is no
        per-event key to sort or compare against. Events are delivered in the order the stream
        returns them, and the stream position is made durable only once a whole page has been
        delivered.
    */
    string getSourceOrdering() {
        return WSO_ASCENDING;
    }

    #! Restores the event stream position
    #! Returns the configuration that defines which event stream this provider follows
    /** The sheet scopes the events and the event type selects which of them are delivered, so
        either one changing is a different stream and the opaque stream position from the previous
        one cannot be reused.  The poll interval is excluded: it changes how often the same stream
        is read, not what it contains.

        @since SmartSheetDataProvider 1.1
    */
    private *hash<auto> getWatchStreamIdentityImpl() {
        return {
            "sheet_id": sheet_id,
            "event_type": event_type,
        };
    }

    private restoreCheckpoint(hash<auto> checkpoint) {
        if (checkpoint.version.typeCode() != NT_INT || checkpoint.version != 1
                || checkpoint.stream_position.typeCode() != NT_STRING || !checkpoint.stream_position.val()) {
            throw "WATCH-CHECKPOINT-ERROR", "invalid Smartsheet watch checkpoint; expecting version 1 and a "
                "non-empty stream_position";
        }
        stream_position = checkpoint.stream_position;
    }

    #! Durably advances the event stream position
    private commitCheckpoint(string next_position) {
        if (!next_position.val()) {
            throw "WATCH-CHECKPOINT-ERROR", "cannot persist an empty Smartsheet stream position";
        }
        persistCheckpoint({
            "version": 1,
            "stream_position": next_position,
        });
        stream_position = next_position;
    }

    #! Polls once, advancing the stream position only after the page has been delivered
    /** @note The stream position returned with a page is stored only after every event on that page
        has been raised. Storing it first would skip the whole page if delivery then failed; storing
        it afterwards means a failure re-reads the page, so delivery is at-least-once.
    */
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

            if (response.data) {
                foreach hash<auto> event in (response.data) {
                    if (isStopping()) {
                        # the stream position stays where it is, so this page is read again
                        return;
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

            # Update stream position for next poll now that the page has been delivered
            if (response.nextStreamPosition) {
                commitCheckpoint(response.nextStreamPosition);
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
