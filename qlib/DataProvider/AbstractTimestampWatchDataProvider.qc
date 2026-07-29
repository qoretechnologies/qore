# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AbstractTimestampWatchDataProvider.qc timestamp-cursor Watch data provider base class

/*  DataProvider Copyright 2019 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DataProvider module
public namespace DataProvider {

#! Watch data provider base class for feeds positioned by a record timestamp
/** Most polling watch providers track their position in an upstream feed with a "records changed
    since <i>t</i>" query. This class implements that cursor once on top of the
    @ref AbstractOrderedWatchDataProvider "shared ordered polling template", so a provider only has to
    say how to fetch records and what event to raise.

    The cursor is a timestamp plus the set of record IDs already delivered <b>at exactly that
    timestamp</b>: a record is new when its timestamp is later than the cursor, or equal to it and
    not in the delivered set. The ID set is what makes the cursor safe at its own boundary: a timestamp-only
    cursor either re-delivers every record sharing the boundary timestamp (if the query is
    inclusive) or drops them (if it is exclusive). The ID set is reset whenever the cursor moves to a
    newer timestamp, so it stays small.

    @par Ordering
    Records are dispatched in ascending timestamp order whatever order the upstream API returned
    them in, and the cursor advances only after a record has been dispatched successfully. See
    @ref AbstractOrderedWatchDataProvider for the full sequence.

    @par Records without a usable timestamp
    A record whose timestamp field is missing or unparseable has no position in the feed, so it
    cannot be ordered against the cursor and is not dispatched; the fact is logged rather than
    passed over silently. A provider whose feed carries its timestamp somewhere else overrides
    @ref getRecordTimestamp().

    @par Durability
    The checkpoint format is
    @code{.py}
{
    "version": 1,
    "last_check": <the timestamp of the newest record delivered>,
    "last_ids": (<the IDs delivered at that timestamp>,),
}
    @endcode

    Without @ref AbstractWatchDataProviderBase::setCheckpointPersistence() "checkpoint persistence"
    the cursor lives only in memory, and a restart resumes from the provider's start date, or from
    the restart time when no start date was given.

    @since DataProvider 3.6
*/
public class AbstractTimestampWatchDataProvider inherits AbstractOrderedWatchDataProvider {
    private {
        #! the timestamp of the newest record delivered
        date last_check;

        #! the IDs of the records delivered at exactly @ref last_check
        hash<string, bool> last_ids = {};
    }

    #! Creates the object
    /** @param poll_interval_secs the polling interval in seconds
        @param start_date do not deliver records earlier than this timestamp; a record at exactly
        this timestamp <i>is</i> delivered, because nothing has been delivered at that position yet.
        When not given, the cursor starts at the current time, so only records that arrive after the
        provider starts are delivered

        @throw WATCH-CHECKPOINT-ERROR the start date is a relative date/time value
    */
    constructor(int poll_interval_secs, *date start_date) : AbstractOrderedWatchDataProvider(poll_interval_secs) {
        if (exists start_date && !start_date.absolute()) {
            throw "WATCH-CHECKPOINT-ERROR", sprintf("the watch start date must be an absolute date/time value; got "
                "%y", start_date);
        }
        last_check = start_date ?? now_us();
    }

    #! Returns the timestamp of the newest record delivered
    /** @since DataProvider 3.6
    */
    date getLastCheck() {
        return last_check;
    }

    #! Returns the IDs of the records delivered at exactly @ref getLastCheck()
    /** @since DataProvider 3.6
    */
    list<string> getLastIds() {
        return keys last_ids;
    }

    #! @cond nodoc
    #! Restores a timestamp cursor
    private restoreCheckpoint(hash<auto> checkpoint) {
        if (checkpoint.version.typeCode() != NT_INT || checkpoint.version != 1
                || checkpoint.last_check.typeCode() != NT_DATE || !checkpoint.last_check.absolute()
                || checkpoint.last_ids.typeCode() != NT_LIST) {
            throw "WATCH-CHECKPOINT-ERROR", "invalid timestamp watch checkpoint; expecting version 1, an absolute "
                "last_check date/time value, and a last_ids list";
        }
        hash<string, bool> restored = {};
        foreach auto id in (checkpoint.last_ids) {
            if (id.typeCode() != NT_STRING || !id.val()) {
                throw "WATCH-CHECKPOINT-ERROR", "invalid timestamp watch checkpoint; record IDs must be non-empty "
                    "strings";
            }
            restored{id} = True;
        }
        last_check = checkpoint.last_check;
        last_ids = restored;
    }

    #! Durably advances the timestamp cursor
    /** The replacement checkpoint is stored before the in-memory cursor changes, so a persistence
        failure retries the record instead of advancing past it.
    */
    private commitCheckpoint(date record_date, string id) {
        if (!record_date.absolute() || !id.val()) {
            throw "WATCH-CHECKPOINT-ERROR", "cannot persist a timestamp checkpoint with a relative date/time value "
                "or an empty record ID";
        }
        if (record_date < last_check) {
            throw "WATCH-CHECKPOINT-ERROR", sprintf("cannot move the watch cursor backwards from %y to %y",
                last_check, record_date);
        }
        hash<string, bool> next_ids = record_date == last_check ? last_ids : {};
        next_ids{id} = True;
        persistCheckpoint({
            "version": 1,
            "last_check": record_date,
            "last_ids": keys next_ids,
        });
        last_check = record_date;
        last_ids = next_ids;
    }

    #! Returns the cycle's candidate records as watch items
    private list<hash<WatchItemInfo>> fetchWatchItems() {
        string ts_field = getTimestampField();
        string id_field = getIdField();
        list<hash<WatchItemInfo>> items = ();
        foreach hash<auto> record in (fetchRecords(last_check)) {
            if (isStopping()) {
                break;
            }
            *string id = exists record{id_field} ? string(record{id_field}) : NOTHING;
            if (!id.val()) {
                logWatchInfo("watch record without a usable %y identity ignored", id_field);
                continue;
            }
            *date record_date = getRecordTimestamp(record, ts_field);
            if (!exists record_date) {
                logWatchInfo("watch record %y has no usable %y timestamp, so it has no position in the feed and "
                    "cannot be delivered in order; ignored", id, ts_field);
                continue;
            }
            push items, <WatchItemInfo>{
                "id": id,
                "order_key": record_date,
                "data": record,
            };
        }
        return items;
    }

    #! Returns @ref True if the record lies beyond the durable cursor
    private bool isNewWatchItem(hash<WatchItemInfo> item) {
        date record_date = item.order_key;
        if (record_date > last_check) {
            return True;
        }
        if (record_date == last_check) {
            return !last_ids{item.id};
        }
        return False;
    }

    #! Raises the provider's event for one record
    private dispatchWatchItem(hash<WatchItemInfo> item, auto payload, list<hash<WatchItemInfo>> superseded) {
        notifyObservers(getEventName(), payload);
    }

    #! Durably advances the cursor to the given record
    private advanceWatchCursor(hash<WatchItemInfo> item) {
        commitCheckpoint(item.order_key, item.id);
    }
    #! @endcond

    #! Returns the record's position in the feed
    /** The default implementation reads @ref getTimestampField() from the record and accepts a
        date/time value or a string that can be converted to one.

        @param record the record as returned by @ref fetchRecords()
        @param field the timestamp field name

        @return the record's timestamp, or @ref nothing if the record has no usable timestamp, in
        which case the record is not delivered
    */
    private *date getRecordTimestamp(hash<auto> record, string field) {
        auto val = record{field};
        switch (val.typeCode()) {
            case NT_DATE:
                return val.absolute() ? val : NOTHING;
            case NT_STRING:
                return val.val() ? date(val) : NOTHING;
        }
    }

    #! Returns the record field holding the record's position in the feed
    /** The default is \c "created_at"; override in providers whose feed is positioned by a different
        field (ex: \c "updated_at")
    */
    private string getTimestampField() {
        return "created_at";
    }

    #! Returns the record field holding the record's stable identity
    /** The default is \c "id"
    */
    private string getIdField() {
        return "id";
    }

    #! Fetches every record changed at or after the given timestamp
    /** Implementations must follow upstream pagination: a record that is not returned cannot be
        delivered. The order of the returned records does not matter.

        @param since the cursor timestamp; returning records at or before it is harmless because the
        template discards them
    */
    abstract private list<hash<auto>> fetchRecords(date since);

    #! Returns the name of the event raised for each new record
    abstract private string getEventName();
}
}
