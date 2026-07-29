# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerWatchDataProviderBase.qc base class for BaseLinker polling watch data providers

/*  BaseLinkerWatchDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! Shared constructor-option hash for BaseLinker watch providers
/** Adds a polling-interval option and a `since` cursor (date/time) on top of
    the base client/connection options inherited from @ref BaseLinkerDataProviderBase.
*/
public const WatchConstructorOptions = BaseLinkerDataProviderBase::BaseConstructorOptions + {
    "poll_interval": <DataProviderOptionInfo>{
        "display_name": "Poll Interval",
        "short_desc": "Polling interval in seconds",
        "type": AbstractDataProviderType::get(SoftIntOrNothingType),
        "desc": "The polling interval in seconds (minimum " + MinPollInterval + ", default "
            + DefaultPollInterval + "). BaseLinker retains journal logs for only 3 days, so "
            "choose an interval well below that; defaults are sized to respect the 100 req/min "
            "rate limit.",
        "default_value": DefaultPollInterval,
    },
    "since": <DataProviderOptionInfo>{
        "display_name": "Since",
        "short_desc": "Only return journal entries after this date/time",
        "type": AbstractDataProviderType::get(DateOrNothingType),
        "desc": "Journal cursor start as a date/time value. If not provided, polling starts "
            "from now (no historical backfill). The date is converted to Unix seconds before "
            "being passed to the BaseLinker `getJournalList` API.",
    },
};

#! Base class for BaseLinker polling watch data providers
/** Polls BaseLinker's `getJournalList` method with a fixed `logs_types` filter
    (provided by the concrete subclass) and a monotonically-increasing `last_log_id`
    cursor. New log entries are emitted as observable events via @ref notifyObservers().

    Subclasses supply the numeric `log_type` code and the event name; everything else
    (polling loop, cursor tracking, deduplication) is handled here.
*/
public class BaseLinkerWatchDataProviderBase inherits BaseLinkerDataProviderBase,
        DataProvider::AbstractOrderedWatchDataProvider {
    private {
        #! Last journal `log_id` seen; BaseLinker returns entries strictly after this value
        softint last_log_id = 0;

        #! Optional date/time lower bound (inclusive) for the first poll
        *date since;
    }

    #! Creates the object with an async REST client
    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : BaseLinkerDataProviderBase(rest),
              AbstractOrderedWatchDataProvider(max(options.poll_interval ?? DefaultPollInterval,
                  MinPollInterval)) {
        since = options.since;
    }

    #! Creates the object with a connection
    constructor(BaseLinkerRestClient::BaseLinkerRestConnection conn, *hash<auto> options)
            : BaseLinkerDataProviderBase(conn),
              AbstractOrderedWatchDataProvider(max(options.poll_interval ?? DefaultPollInterval,
                  MinPollInterval)) {
        since = options.since;
    }

    #! Creates the object with options
    constructor(*hash<auto> options)
            : BaseLinkerDataProviderBase(options),
              AbstractOrderedWatchDataProvider(max(options.poll_interval ?? DefaultPollInterval,
                  MinPollInterval)) {
        since = options.since;
    }

    #! Destructor
    destructor() {
        stopEventsIntern();
    }

    #! Cancels any in-flight REST request during shutdown
    private cancelPollOperation() {
        rest.close();
    }

    #! The BaseLinker journal is returned in ascending `log_id` order
    string getSourceOrdering() {
        return WSO_ASCENDING;
    }

    #! Returns the highest journal `log_id` delivered so far
    /** @since BaseLinkerDataProvider 1.0
    */
    softint getLastLogId() {
        return last_log_id;
    }

    #! Restores the journal cursor
    #! Returns the configuration that defines which journal scan this provider follows
    /** The lower bound decides where the journal is entered, so changing it defines a different
        scan and must start a new cursor.  The poll interval is excluded: it changes how often the
        same journal is read, not which entries it covers.

        @since BaseLinkerDataProvider 1.1
    */
    private *hash<auto> getWatchStreamIdentityImpl() {
        return {
            "since": since,
        };
    }

    private restoreCheckpoint(hash<auto> checkpoint) {
        if (checkpoint.version.typeCode() != NT_INT || checkpoint.version != 1
                || checkpoint.last_log_id.typeCode() != NT_INT || checkpoint.last_log_id < 0) {
            throw "WATCH-CHECKPOINT-ERROR", "invalid BaseLinker watch checkpoint; expecting version 1 and a "
                "non-negative last_log_id";
        }
        last_log_id = checkpoint.last_log_id;
    }

    #! Durably advances the journal cursor
    private commitCheckpoint(softint log_id) {
        if (log_id < 0) {
            throw "WATCH-CHECKPOINT-ERROR", "cannot persist a negative BaseLinker journal log_id";
        }
        persistCheckpoint({
            "version": 1,
            "last_log_id": int(log_id),
        });
        last_log_id = log_id;
    }

    #! Fetches the journal entries after the current cursor
    private list<hash<WatchItemInfo>> fetchWatchItems() {
        hash<auto> params = cast<hash>({
            "logs_types": (getLogType(),),
        });
        if (last_log_id > 0) {
            params.last_log_id = last_log_id;
        } else if (exists since) {
            params.date_from = int(get_epoch_seconds(since));
        }
        hash<auto> response = callMethod("getJournalList", params);
        list<hash<WatchItemInfo>> items = ();
        foreach hash<auto> entry in (response.logs ?? ()) {
            push items, <WatchItemInfo>{
                "id": string(entry.log_id),
                "order_key": int(entry.log_id),
                "data": entry,
            };
        }
        return items;
    }

    #! Returns @ref True if the journal entry lies beyond the cursor
    private bool isNewWatchItem(hash<WatchItemInfo> item) {
        return item.order_key > last_log_id;
    }

    #! Raises the event for one journal entry
    private dispatchWatchItem(hash<WatchItemInfo> item, auto payload, list<hash<WatchItemInfo>> superseded) {
        notifyObservers(getEventName(), convertEpochFields(payload));
    }

    #! Durably advances the cursor to the given journal entry
    private advanceWatchCursor(hash<WatchItemInfo> item) {
        commitCheckpoint(item.order_key);
    }

    #! Logs an error raised during a polling cycle
    private logPollError(hash<ExceptionInfo> ex) {
        log(LoggerLevel::ERROR, "BaseLinker watch %s: poll failed: %s: %s", getEventName(), ex.err, ex.desc);
    }

    #! Logs an informational message about a polling cycle
    private logWatchInfo(string fmt, ...) {
        log(LoggerLevel::INFO, "%s", vsprintf(fmt, argv));
    }

    #! Returns a hash of all supported event types
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            getEventName(): <DataProviderMessageInfo>{
                "desc": sprintf("Fired when BaseLinker journal log type %d (%s) is emitted",
                    getLogType(), getEventName()),
                "type": new BaseLinkerJournalLogDataType(),
            },
        };
    }

    #! Returns example event data for documentation / flow-builder previews
    private auto getExampleEventDataImpl(string event_id) {
        try {
            hash<auto> response = callMethod("getJournalList", {
                "logs_types": (getLogType(),),
            });
            list<auto> logs = response.logs ?? ();
            if (logs) {
                return convertEpochFields(logs[0]);
            }
        } catch (hash<ExceptionInfo> ex) {
            # Fall through to fake example data on API error
        }
        return {
            "log_id": 1000001,
            "log_type": getLogType(),
            "order_id": 123456,
            "object_id": NOTHING,
            "date": now_us(),
        };
    }

    #! Converts BaseLinker journal-log epoch-seconds fields to absolute dates
    private static hash<auto> convertEpochFields(hash<auto> entry) {
        if (exists entry.date && entry.date.typeCode() == NT_INT) {
            entry.date = 1970-01-01Z + seconds(entry.date);
        }
        return entry;
    }

    #! Returns the BaseLinker numeric log_type code for this watch provider
    abstract int getLogType();

    #! Returns the kebab-case event name constant for this watch provider
    abstract string getEventName();
}

} # namespace BaseLinkerDataProvider
