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
/** Adds a polling-interval option and a `since` cursor (Unix seconds) on top of
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
        "display_name": "Since (Unix Seconds)",
        "short_desc": "Only return journal entries after this timestamp",
        "type": AbstractDataProviderType::get(SoftIntOrNothingType),
        "desc": "Journal cursor start, as Unix seconds since epoch. If not provided, polling "
            "starts from now (no historical backfill).",
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
        DataProvider::AbstractWatchDataProviderBase {
    private {
        #! Last journal `log_id` seen; BaseLinker returns entries strictly after this value
        softint last_log_id = 0;

        #! Optional Unix-seconds lower bound (inclusive) for the first poll
        *softint since;
    }

    #! Creates the object with an async REST client
    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : BaseLinkerDataProviderBase(rest),
              AbstractWatchDataProviderBase(max(options.poll_interval ?? DefaultPollInterval,
                  MinPollInterval)) {
        since = options.since;
    }

    #! Creates the object with a connection
    constructor(BaseLinkerRestClient::BaseLinkerRestConnection conn, *hash<auto> options)
            : BaseLinkerDataProviderBase(conn),
              AbstractWatchDataProviderBase(max(options.poll_interval ?? DefaultPollInterval,
                  MinPollInterval)) {
        since = options.since;
    }

    #! Creates the object with options
    constructor(*hash<auto> options)
            : BaseLinkerDataProviderBase(options),
              AbstractWatchDataProviderBase(max(options.poll_interval ?? DefaultPollInterval,
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

    #! Performs a single polling operation
    private pollOnce() {
        try {
            hash<auto> params = cast<hash<auto>>({
                "logs_types": (getLogType(),),
            });
            if (last_log_id > 0) {
                params.last_log_id = last_log_id;
            } else if (exists since) {
                params.date_from = since;
            }
            hash<auto> response = callMethod("getJournalList", params);
            list<auto> logs = response.logs ?? ();
            foreach hash<auto> entry in (logs) {
                if (isStopping()) {
                    break;
                }
                softint lid = int(entry.log_id);
                if (lid > last_log_id) {
                    last_log_id = lid;
                }
                notifyObservers(getEventName(), entry);
            }
            # If we had only a `since` cursor, once we've seen at least one entry,
            # switch to log_id-based cursoring so we don't re-poll the same window.
            if (!last_log_id && logs) {
                last_log_id = int(logs[logs.size() - 1].log_id);
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "BaseLinker watch %s: poll failed: %s: %s",
                getEventName(), ex.err, ex.desc);
        }
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
                return logs[0];
            }
        } catch (hash<ExceptionInfo> ex) {
            # Fall through to fake example data on API error
        }
        return {
            "log_id": 1000001,
            "log_type": getLogType(),
            "order_id": 123456,
            "object_id": NOTHING,
            "date": int(get_epoch_seconds(now_us())),
        };
    }

    #! Returns the BaseLinker numeric log_type code for this watch provider
    abstract int getLogType();

    #! Returns the kebab-case event name constant for this watch provider
    abstract string getEventName();
}

} # namespace BaseLinkerDataProvider
