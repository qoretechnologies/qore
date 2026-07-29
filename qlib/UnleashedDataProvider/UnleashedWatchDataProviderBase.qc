# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnleashedDataProvider module definition

/*  UnleashedWatchDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the UnleashedDataProvider module
public namespace UnleashedDataProvider {

#! Watch constructor options for Unleashed polling triggers
public const WatchConstructorOptions = UnleashedDataProviderBase::BaseConstructorOptions + {
    "poll_interval": <DataProviderOptionInfo>{
        "display_name": "Poll Interval",
        "short_desc": "Polling interval in seconds",
        "type": AbstractDataProviderType::get(SoftIntOrNothingType),
        "desc": "The polling interval in seconds (minimum " + MinPollInterval + ", default " + DefaultPollInterval + ")",
        "default_value": DefaultPollInterval,
    },
    "since": <DataProviderOptionInfo>{
        "display_name": "Since",
        "short_desc": "Return records modified since this date",
        "type": AbstractDataProviderType::get(DateOrNothingType),
        "desc": "Only return records modified after this date/time. If not provided, starts from now.",
    },
};

#! Base class for Unleashed watch/trigger data providers
public class UnleashedWatchDataProviderBase inherits UnleashedDataProviderBase,
        DataProvider::AbstractWatchDataProviderBase {
    public {
        #! The number of delivered record IDs retained before the set is pruned
        const MaxDeliveredIds = 10000;

        #! The number of least recently added IDs dropped when the set is pruned
        const DeliveredIdPruneCount = 5000;

        #! Constructor options for watch providers
        const WatchProviderConstructorOptions = WatchConstructorOptions;
    }

    private {
        #! Last poll time for filtering
        date since;

        #! Set of seen IDs for deduplication
        hash<string, bool> seen_ids = {};
    }

    #! Creates the object with a REST client
    constructor(UnleashedRestClient::UnleashedRestClient rest, *hash<auto> options)
            : UnleashedDataProviderBase(rest),
              AbstractWatchDataProviderBase(max(options.poll_interval ?? DefaultPollInterval, MinPollInterval)) {
        since = options.since ?? now_us();
    }

    #! Creates the object with a connection
    constructor(UnleashedRestClient::UnleashedRestConnection conn, *hash<auto> options)
            : UnleashedDataProviderBase(conn),
              AbstractWatchDataProviderBase(max(options.poll_interval ?? DefaultPollInterval, MinPollInterval)) {
        since = options.since ?? now_us();
    }

    #! Creates the object with options
    constructor(*hash<auto> options)
            : UnleashedDataProviderBase(options),
              AbstractWatchDataProviderBase(max(options.poll_interval ?? DefaultPollInterval, MinPollInterval)) {
        since = options.since ?? now_us();
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
    #! The upstream feed carries no reliable per-record ordering key
    /** Records are deduplicated by ID and the cursor is a wall-clock poll boundary, so no ordering
        guarantee can be offered; consumers must not assume that events arrive in source order.

        This provider is a documented exemption from the shared ordered template
        (@ref DataProvider::AbstractOrderedWatchDataProvider) because its API exposes no monotonic
        per-record position to order or checkpoint against. Its cursor is nonetheless durable.
    */
    string getSourceOrdering() {
        return WSO_UNORDERED;
    }

    #! Restores the poll-boundary cursor
    private restoreCheckpoint(hash<auto> checkpoint) {
        if (checkpoint.version.typeCode() != NT_INT || checkpoint.version != 1
                || checkpoint.since.typeCode() != NT_DATE || !checkpoint.since.absolute()
                || checkpoint.seen_ids.typeCode() != NT_LIST) {
            throw "WATCH-CHECKPOINT-ERROR", "invalid watch checkpoint; expecting version 1, an absolute since "
                "date/time value, and a seen_ids list";
        }
        hash<string, bool> restored = {};
        foreach auto id in (checkpoint.seen_ids) {
            if (id.typeCode() != NT_STRING || !id.val()) {
                throw "WATCH-CHECKPOINT-ERROR", "invalid watch checkpoint; record IDs must be non-empty strings";
            }
            restored{id} = True;
        }
        since = checkpoint.since;
        seen_ids = restored;
    }

    #! Durably advances the poll-boundary cursor
    /** The IDs delivered are stored with the boundary, because a wall-clock cursor alone cannot tell
        a record that has already been delivered from one that arrived in the same instant.
    */
    private commitCheckpoint(date poll_start, hash<string, bool> delivered) {
        if (!poll_start.absolute()) {
            throw "WATCH-CHECKPOINT-ERROR", "cannot persist a relative date/time value as a watch cursor";
        }
        persistCheckpoint({
            "version": 1,
            "since": poll_start,
            "seen_ids": keys delivered,
        });
        since = poll_start;
        seen_ids = delivered;
    }

    #! Polls once, advancing the cursor only after every record has been delivered
    /** @note The cursor is advanced only once the whole sweep has succeeded. Advancing it after a
        partial sweep would move the poll boundary past records that were never delivered.
    */
    private pollOnce() {
        date poll_start = now_us();
        list<hash<auto>> records = pollOnceImpl();

        hash<string, bool> delivered = seen_ids;
        foreach hash<auto> record in (records) {
            if (isStopping()) {
                # the cursor stays where it is, so the remaining records are swept again
                return;
            }
            string id = getRecordId(record);
            if (!delivered{id}) {
                notifyObservers(getEventName(), record);
                delivered{id} = True;
            }
        }

        # Bound the delivered set: drop the 5000 least recently added IDs. %Qore hashes keep
        # insertion order, so the leading keys are the ones added earliest and therefore the ones
        # least likely to reappear in a future poll window.
        if (delivered.size() > MaxDeliveredIds) {
            list<string> all_keys = keys delivered;
            foreach string key in (all_keys[0..(DeliveredIdPruneCount - 1)]) {
                remove delivered{key};
            }
        }

        commitCheckpoint(poll_start, delivered);
    }

    #! Called when a polling error occurs
    private logPollError(hash<ExceptionInfo> ex) {
        @debug(log(LoggerLevel::ERROR, "pollLoop error: %s: %s", ex.err, ex.desc));
    }

    #! Poll once and return any new records - override in subclasses
    abstract list<hash<auto>> pollOnceImpl();

    #! Get the unique ID from a record - override in subclasses
    abstract string getRecordId(hash<auto> record);

    #! Get the event name - override in subclasses
    abstract string getEventName();

    #! Get the event data type - override in subclasses
    abstract AbstractDataProviderType getEventDataType();

    #! Returns the event types supported by this provider
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            getEventName(): <DataProviderMessageInfo>{
                "desc": getDesc(),
                "type": getEventDataType(),
            },
        };
    }

    #! Returns example event data - tries real API data first, falls back to fake data
    private auto getExampleEventDataImpl(string event_id) {
        # Try to get real data from the API first
        try {
            list<hash<auto>> records = pollOnceImpl();
            if (records.size() > 0) {
                return records[0];
            }
        } catch () {
            # Fall through to fake data
        }
        # Return resource-specific fake data
        return getFakeExampleData();
    }

    #! Returns resource-specific fake example data - override in subclasses
    abstract hash<auto> getFakeExampleData();
}

} # namespace UnleashedDataProvider
