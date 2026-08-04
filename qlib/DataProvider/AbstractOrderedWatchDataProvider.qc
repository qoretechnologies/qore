# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AbstractOrderedWatchDataProvider.qc shared ordered polling template for Watch data providers

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

#! Describes how a watch data provider's upstream feed is ordered
/** Every event-producing watch data provider declares this so that a consumer can tell an ordered
    feed from an unordered one instead of having to infer it from the implementation.

    @since DataProvider 3.6
*/
public enum WatchSourceOrdering : string {
    #! the upstream API returns items in ascending source order, and they are dispatched as received
    WSO_ASCENDING = "ascending",

    #! the upstream API returns items in descending source order; they are reversed before dispatch
    WSO_DESCENDING = "descending",

    #! the feed has no comparable source order; each item is dispatched independently
    WSO_UNORDERED = "unordered",

    #! the upstream ordering has not been verified, so no ordering guarantee can be offered
    WSO_UNKNOWN = "unknown",
}

#! What to do with items that snapshot mode collapsed away
/** @since DataProvider 3.6
*/
public enum WatchSupersededAction : string {
    #! leave superseded items untouched in the source system
    WSA_IGNORE = "ignore",

    #! move superseded items to the source system's trash / recycle bin, where recovery is possible
    WSA_TRASH = "trash",

    #! remove superseded items permanently; unrecoverable
    WSA_DELETE = "delete",
}

#! One candidate item in a watch polling cycle
/** An item is a cheap descriptor, not a payload: the shared template sorts, filters, and collapses
    items before any payload is fetched, so a backlog that is going to be collapsed away never costs
    a payload retrieval.

    @since DataProvider 3.6
*/
public hashdecl WatchItemInfo {
    #! The item's stable identity; unique in the provider's feed
    string id;

    #! The item's monotonic ordering key
    /** Items are dispatched in ascending order of this value; ties are broken by @ref id so that
        the dispatch order of a given set of items is deterministic
    */
    auto order_key;

    #! Optional grouping key for snapshot collapse
    /** When not set, all items in a cycle belong to one global group, so snapshot mode retains a
        single latest item. Providers that can group (ex: by mail thread) set this to the group's
        key, and snapshot mode then retains the latest item per key.
    */
    *string group_key;

    #! Opaque provider-defined data carried through the cycle
    auto data;
}

#! The outcome of one watch polling cycle
/** @since DataProvider 3.6
*/
public hashdecl WatchCycleInfo {
    #! the number of items dispatched to observers
    int dispatched = 0;

    #! the number of candidate items discarded because the durable cursor already covers them
    int discarded = 0;

    #! the number of items collapsed away by snapshot mode
    int superseded = 0;

    #! the number of items left for the next cycle by the per-cycle dispatch budget
    int deferred = 0;

    #! the number of items whose dispatch failed; the cycle stops at the first failure
    int failed = 0;
}

#! Describes an item that repeated dispatch failures have stuck the feed on
/** A cycle stops at its first dispatch failure so that the monotonic cursor cannot advance past an
    item that was never delivered. That is the right default, but an item that fails permanently then
    holds the whole feed, and the only symptom visible from outside is that events stop arriving.
    This is the record that turns that silence into a signal.

    @since DataProvider 3.6
*/
public hashdecl WatchItemBlockedInfo {
    #! The blocked item's stable identity
    string id;

    #! The blocked item's ordering key
    auto order_key;

    #! The number of consecutive cycles in which dispatching the item failed
    int attempts;

    #! @ref True when the item was finally dispatched and the feed is moving again
    bool cleared = False;

    #! The most recent dispatch failure; not set once @ref cleared is @ref True
    *hash<ExceptionInfo> ex;
}

#! Watch data provider base class implementing the shared ordered polling sequence
/** Watch providers that expose an ordered feed all have to solve the same problems: upstream
    pagination, upstream ordering that does not match delivery order, a durable cursor that must not
    advance past an item that was never delivered, and follow-up actions that must not run for an
    item observers never received. This class implements that sequence once:

    @code
    fetch
      -> discard items at or before the durable cursor
      -> sort ascending by the provider-defined ordering key
      -> optionally collapse superseded items (snapshot mode)
      -> optionally bound the batch from the oldest end (dispatch budget)
      -> for each item: fetch payload -> dispatch -> persist and advance the cursor
                        -> run the post-success hook -> apply the superseded action
    @endcode

    Subclasses supply the provider-specific pieces as typed hooks:
    - @ref fetchWatchItems() "fetchWatchItems()": the cycle's candidate items
    - @ref isNewWatchItem() "isNewWatchItem()": whether an item is beyond the durable cursor
    - @ref fetchWatchItemPayload() "fetchWatchItemPayload()": the item's full payload
    - @ref dispatchWatchItem() "dispatchWatchItem()": raising the event
    - @ref advanceWatchCursor() "advanceWatchCursor()": persisting and advancing the cursor
    - @ref afterWatchItemDispatched() "afterWatchItemDispatched()": the optional post-success action
    - @ref applySupersededAction() "applySupersededAction()": the optional collapse action
    - @ref restoreCheckpoint() "restoreCheckpoint()": cursor restoration

    @par Ordering
    Items are always dispatched in ascending order of their ordering key, whatever order the upstream
    API returned them in. This is not cosmetic: the durable cursor is monotonic, so dispatching a
    newer item first would advance the cursor past an older item that had not been delivered yet, and
    a failure at that point would drop the older item permanently.

    @par Advance on success only
    The cursor is advanced only after an item has been dispatched successfully, and the cycle
    <b>stops at the first failure</b> rather than continuing with later items. Continuing would
    advance the monotonic cursor past the failed item and lose it; stopping leaves it in place to be
    retried on the next cycle. Delivery is therefore at-least-once, never at-most-once.

    An item is never skipped to get the feed moving again, because skipping is exactly the loss the
    rule exists to prevent. An item that fails permanently therefore holds the feed indefinitely, so
    it is reported instead: @ref setWatchItemBlockedHandler() "setWatchItemBlockedHandler()"
    registers a handler that is called once the same item has failed a configurable number of cycles
    in a row, and called again when it is finally delivered.

    @par Durability
    With @ref AbstractWatchDataProviderBase::setCheckpointPersistence() "checkpoint persistence"
    configured, the cursor survives a host restart, so an item that was never dispatched is still
    dispatched after a reload. Without it, the cursor lives only in memory and the provider's
    behavior after a restart is provider-specific.

    @see @ref AbstractWatchDataProviderBase for the lifecycle and checkpoint machinery

    @since DataProvider 3.6
*/
public class AbstractOrderedWatchDataProvider inherits AbstractWatchDataProviderBase {
    public {
        #! Consecutive dispatch failures before a blocked item is reported by default
        const DefaultBlockedThreshold = 3;
    }

    private {
        #! collapse a backlog to the latest item per group?
        bool snapshot_mode = False;

        #! what to do with items collapsed away by snapshot mode
        string superseded_action = WSA_IGNORE;

        #! maximum number of items dispatched per cycle; 0 means unlimited
        int max_items_per_cycle = 0;

        #! the ID of the item the feed is currently stuck on, if any
        /** the block state below is owned by the polling thread; only the polling thread reads or
            writes it, so it needs no lock
        */
        *string blocked_id;

        #! consecutive dispatch failures recorded for blocked_id
        int blocked_attempts = 0;

        #! has the current block already been reported?
        bool blocked_reported = False;

        #! consecutive failures required before a block is reported
        /** immutable once polling has started; see setWatchItemBlockedHandler()
        */
        int blocked_threshold = DefaultBlockedThreshold;

        #! called when an item blocks the feed, and again when that block clears
        /** immutable once polling has started; see setWatchItemBlockedHandler()
        */
        *code<nothing(hash<WatchItemBlockedInfo>)> blocked_handler;
    }

    #! Creates the object
    /** @param poll_interval_secs the polling interval in seconds
    */
    constructor(int poll_interval_secs) : AbstractWatchDataProviderBase(poll_interval_secs) {
    }

    #! Returns @ref True if the provider collapses a backlog to the latest item per group
    /** @since DataProvider 3.6
    */
    bool hasSnapshotMode() {
        return snapshot_mode;
    }

    #! Returns the action applied to items that snapshot mode collapsed away
    /** @since DataProvider 3.6
    */
    string getSupersededAction() {
        return superseded_action;
    }

    #! Returns the maximum number of items dispatched per cycle; 0 means unlimited
    /** @since DataProvider 3.6
    */
    int getMaxItemsPerCycle() {
        return max_items_per_cycle;
    }

    #! Sets the handler called when repeated dispatch failures block the feed
    /** A permanently-failing item is retried forever by design: skipping it is what loses data. The
        cost of that choice is that the feed stops, so the block has to be reported rather than left
        for someone to infer from a silent log.

        @param handler called with a @ref WatchItemBlockedInfo once an item has failed @a threshold
        cycles in a row, and called once more with @ref WatchItemBlockedInfo::cleared "cleared" set
        when that item is finally dispatched; an exception thrown by the handler is logged through
        @ref AbstractWatchDataProviderBase::logPollError() "logPollError()" and does not stop polling
        @param threshold consecutive failures before the block is reported; defaults to
        @ref DefaultBlockedThreshold

        @throw WATCH-POLICY-ERROR the threshold is less than 1
        @throw WATCH-HANDLER-ERROR polling has already started

        @note the handler is called from the polling thread, so it must be installed before polling
        starts; installing it afterwards is rejected rather than raced

        @since DataProvider 3.6
    */
    setWatchItemBlockedHandler(code<nothing(hash<WatchItemBlockedInfo>)> handler, *int threshold) {
        if (exists threshold && threshold < 1) {
            throw "WATCH-POLICY-ERROR", sprintf("the blocked-item threshold must be at least 1 (got %d); a "
                "threshold of 0 would report every transient failure as a block", threshold);
        }
        AutoLock al(sleep_m);
        if (started) {
            throw "WATCH-HANDLER-ERROR", "the blocked-item handler cannot be configured after polling has started";
        }
        blocked_handler = handler;
        blocked_threshold = threshold ?? DefaultBlockedThreshold;
    }

    #! Returns @ref True if this provider dispatches events in ascending source order
    /** The template sorts every cycle's items by their ordering key before dispatching them, so the
        guarantee holds whatever order the upstream API used; it is only withheld for a feed that
        declares itself @ref WSO_UNORDERED, where there is no source order to ascend.

        @since DataProvider 3.6
    */
    bool hasAscendingDispatch() {
        return getSourceOrdering() != WSO_UNORDERED;
    }

    #! @cond nodoc
    #! Sets the snapshot-collapse and dispatch-budget policy for the provider
    /** @param snapshot the backlog is collapsed to the latest item per group when @ref True
        @param action the action applied to collapsed items; one of @ref WatchSupersededAction
        @param max_items the maximum number of items dispatched per cycle; 0 or unset means unlimited

        @throw WATCH-POLICY-ERROR the superseded action is unknown, the item budget is negative, or a
        superseded action other than @ref WSA_IGNORE was requested without snapshot mode
    */
    private setWatchPolicy(bool snapshot, *string action, *int max_items) {
        if (action.val() && action != WSA_IGNORE && action != WSA_TRASH && action != WSA_DELETE) {
            throw "WATCH-POLICY-ERROR", sprintf("superseded action %y is unknown; expecting one of %y", action,
                (WSA_IGNORE, WSA_TRASH, WSA_DELETE));
        }
        if (max_items && max_items < 0) {
            throw "WATCH-POLICY-ERROR", sprintf("the per-cycle item budget cannot be negative (got %d)", max_items);
        }
        string new_action = action ?? WSA_IGNORE;
        if (!snapshot && new_action != WSA_IGNORE) {
            throw "WATCH-POLICY-ERROR", sprintf("superseded action %y requires snapshot mode; without it no item "
                "is superseded, so the action could only remove items that were delivered", new_action);
        }
        snapshot_mode = snapshot;
        superseded_action = new_action;
        max_items_per_cycle = max_items ?? 0;
    }

    #! Performs a single polling operation using the shared sequence
    private pollOnce() {
        runWatchCycle(fetchWatchItems());
    }

    #! Runs the shared ordered processing sequence over a cycle's candidate items
    /** @param items the cycle's candidate items in any order

        @return a summary of what the cycle did

        @note this method never throws for a dispatch failure; the failure is logged through
        @ref AbstractWatchDataProviderBase::logPollError() "logPollError()" and reported in the
        return value, because the polling thread must keep running
    */
    private hash<WatchCycleInfo> runWatchCycle(list<hash<WatchItemInfo>> items) {
        hash<WatchCycleInfo> info();

        # discard everything at or before the durable cursor before doing any other work
        /* a cycle that lists items and then silently drops all of them is indistinguishable from a
           hung poller in the log, so the discard is always reported: the counts and the highest
           discarded ordering key against the cursor are what tell an operator that the provider is
           working and simply has nothing new */
        int listed = items.size();
        list<hash<WatchItemInfo>> new_items = ();
        auto highest_discarded;
        foreach hash<WatchItemInfo> item in (items) {
            if (isNewWatchItem(item)) {
                push new_items, item;
                continue;
            }
            ++info.discarded;
            if (!exists highest_discarded || compareWatchOrderKeys(item.order_key, highest_discarded) > 0) {
                highest_discarded = item.order_key;
            }
        }
        if (info.discarded) {
            *string cursor_desc = getWatchCursorDescription();
            logWatchDebug("%d of %d listed item(s) already covered by the cursor%s discarded; highest discarded "
                "ordering key: %y; %d new item(s) remain", info.discarded, listed,
                cursor_desc.val() ? sprintf(" (%s)", cursor_desc) : "", highest_discarded, new_items.size());
        }
        items = new_items;
        if (!items) {
            return info;
        }

        # dispatch order is ascending in the provider's ordering key whatever order the upstream
        # API used; ties break on identity so that a given set of items always dispatches the same way
        items = sort(items, int sub (hash<WatchItemInfo> l, hash<WatchItemInfo> r) {
            int rv = compareWatchOrderKeys(l.order_key, r.order_key);
            return rv ? rv : (l.id <=> r.id);
        });

        # each batch entry is a retained item plus the items it superseded
        list<hash<auto>> batch;
        if (snapshot_mode) {
            batch = collapseWatchItems(items);
        } else {
            batch = map ("item": $1, "superseded": ()), items;
        }

        # bound the batch from the oldest end so that the cursor still advances contiguously; the
        # remainder is dispatched by later cycles rather than skipped
        if (max_items_per_cycle > 0 && batch.size() > max_items_per_cycle) {
            info.deferred = batch.size() - max_items_per_cycle;
            batch = batch[0..(max_items_per_cycle - 1)];
            logWatchInfo("watch dispatch budget of %d item(s) reached; %d item(s) deferred to the next cycle",
                max_items_per_cycle, info.deferred);
        }

        foreach hash<auto> entry in (batch) {
            if (isStopping()) {
                break;
            }
            hash<WatchItemInfo> item = entry.item;
            list<hash<WatchItemInfo>> superseded = entry.superseded;
            try {
                auto payload = fetchWatchItemPayload(item);
                dispatchWatchItem(item, payload, superseded);
                # persist before advancing: a persistence failure must retry, not skip
                advanceWatchCursor(item);
                afterWatchItemDispatched(item, payload);
            } catch (hash<ExceptionInfo> ex) {
                ++info.failed;
                logPollError(ex);
                noteWatchItemFailed(item, ex);
                # stop here: dispatching a later item would advance the monotonic cursor past this
                # one and drop it permanently on what may well be a transient error
                break;
            }
            ++info.dispatched;
            noteWatchItemDelivered(item);
            if (superseded) {
                info.superseded += superseded.size();
                # the retained event is already delivered and its cursor is durable, so a failure
                # here cannot cost an event; it only leaves the superseded items in place
                try {
                    applySupersededAction(superseded);
                } catch (hash<ExceptionInfo> ex) {
                    logPollError(ex);
                }
            }
        }
        return info;
    }

    #! Records a dispatch failure and reports the item once it has blocked the feed
    /** @param item the item whose dispatch failed
        @param ex the failure
    */
    private noteWatchItemFailed(hash<WatchItemInfo> item, hash<ExceptionInfo> ex) {
        if (blocked_id != item.id) {
            # a different item now leads the feed, so this is the start of a new block
            blocked_id = item.id;
            blocked_attempts = 0;
            blocked_reported = False;
        }
        ++blocked_attempts;
        if (blocked_reported || blocked_attempts < blocked_threshold) {
            return;
        }
        blocked_reported = True;
        logWatchInfo("item %y has failed %d consecutive cycle(s) and is holding up the feed; it is retried every "
            "cycle and no later item can be delivered until it succeeds: %s: %s", item.id, blocked_attempts,
            ex.err, ex.desc);
        callBlockedHandler(<WatchItemBlockedInfo>{
            "id": item.id,
            "order_key": item.order_key,
            "attempts": blocked_attempts,
            "ex": ex,
        });
    }

    #! Clears any block recorded against an item that has now been delivered
    /** @param item the item that was dispatched successfully
    */
    private noteWatchItemDelivered(hash<WatchItemInfo> item) {
        if (blocked_id != item.id) {
            return;
        }
        int attempts = blocked_attempts;
        bool reported = blocked_reported;
        remove blocked_id;
        blocked_attempts = 0;
        blocked_reported = False;
        if (!reported) {
            return;
        }
        logWatchInfo("item %y was delivered after %d failed cycle(s); the feed is moving again", item.id, attempts);
        callBlockedHandler(<WatchItemBlockedInfo>{
            "id": item.id,
            "order_key": item.order_key,
            "attempts": attempts,
            "cleared": True,
        });
    }

    #! Calls the blocked-item handler, if any
    /** A handler failure is reported and discarded: the handler exists to make a stalled feed
        visible, so a broken handler must not become a second way for the polling thread to die.
    */
    private callBlockedHandler(hash<WatchItemBlockedInfo> info) {
        if (!blocked_handler) {
            return;
        }
        try {
            blocked_handler(info);
        } catch (hash<ExceptionInfo> ex) {
            logPollError(ex);
        }
    }

    #! Collapses items to the latest item per group, preserving ascending dispatch order
    /** @param items the cycle's new items, already sorted ascending

        @return a list of hashes with an \c "item" key holding the retained item and a
        \c "superseded" key holding the items it collapsed away, ordered ascending by the retained
        item's ordering key

        @note the retained item of each group is the group's newest item, so every superseded item
        precedes it; the cursor therefore covers a superseded item as soon as the item that
        superseded it has been committed
    */
    private list<hash<auto>> collapseWatchItems(list<hash<WatchItemInfo>> items) {
        hash<string, list<hash<WatchItemInfo>>> groups();
        foreach hash<WatchItemInfo> item in (items) {
            string key = item.group_key ?? "";
            if (!exists groups{key}) {
                groups{key} = ();
            }
            push groups{key}, item;
        }
        list<hash<auto>> batch = ();
        foreach string key in (keys groups) {
            list<hash<WatchItemInfo>> members = groups{key};
            hash<WatchItemInfo> retained = members.last();
            push batch, {
                "item": retained,
                "superseded": members.size() > 1 ? members[0..(members.size() - 2)] : (),
            };
        }
        return sort(batch, int sub (hash<auto> l, hash<auto> r) {
            int rv = compareWatchOrderKeys(l.item.order_key, r.item.order_key);
            return rv ? rv : (l.item.id <=> r.item.id);
        });
    }
    #! @endcond

    #! Returns the cycle's candidate items
    /** Implementations must return every item in scope for the cycle, following upstream pagination
        as needed: an item that is not returned cannot be dispatched, and snapshot mode can only
        collapse over the items it is given.

        The order of the returned items does not matter; the template sorts them.
    */
    abstract private list<hash<WatchItemInfo>> fetchWatchItems();

    #! Returns @ref True if the item lies beyond the durable cursor
    /** Called before any payload is fetched, so this must be answerable from the item descriptor
        alone.
    */
    abstract private bool isNewWatchItem(hash<WatchItemInfo> item);

    #! Dispatches one item to observers
    /** @param item the item being dispatched
        @param payload the value returned by @ref fetchWatchItemPayload()
        @param superseded the items snapshot mode collapsed into this one; empty when snapshot mode
        is not enabled

        Implementations raise the provider's event here. When @a superseded is not empty the event
        should carry the collapse metadata so a consumer can tell a collapsed event from a plain one.

        An exception stops the cycle without advancing the cursor, so the item is retried.
    */
    abstract private dispatchWatchItem(hash<WatchItemInfo> item, auto payload,
            list<hash<WatchItemInfo>> superseded);

    #! Durably advances the cursor to the given item
    /** Implementations must call
        @ref AbstractWatchDataProviderBase::persistCheckpoint() "persistCheckpoint()" <b>before</b>
        mutating the in-memory cursor, so that a persistence failure retries the item instead of
        silently advancing past it.

        An exception stops the cycle; the item is dispatched again on the next cycle, which is why
        delivery is at-least-once.
    */
    abstract private advanceWatchCursor(hash<WatchItemInfo> item);

    #! Returns the item's payload
    /** Called only for items that are actually going to be dispatched, so a collapsed backlog costs
        no payload retrievals.

        The default implementation returns the item's @ref WatchItemInfo::data "data" value.
    */
    private auto fetchWatchItemPayload(hash<WatchItemInfo> item) {
        return item.data;
    }

    #! Compares two ordering keys
    /** @return -1 if @a l sorts before @a r, 0 if they sort equally, 1 if @a l sorts after @a r

        The default implementation uses the \c <=> operator, which covers the usual integer, date,
        and string keys.
    */
    private int compareWatchOrderKeys(auto l, auto r) {
        return l <=> r;
    }

    #! Runs the provider's post-success action for a dispatched item
    /** Called after the item has been dispatched and its cursor committed, so an exception here
        cannot cost an event; it stops the cycle and the item is dispatched again on the next cycle.

        @note This hook runs when the <b>event has been raised</b>, which is not the same as the
        consumer having <b>processed</b> it; see @ref watch_removal_semantics.

        The default implementation does nothing.
    */
    private afterWatchItemDispatched(hash<WatchItemInfo> item, auto payload) {
        # default: no post-success action
    }

    #! Applies the configured action to items that snapshot mode collapsed away
    /** Called after the retained item's event has been raised and its cursor committed.

        The default implementation does nothing for @ref WSA_IGNORE and throws for any other action,
        because silently ignoring a request to remove superseded items would leave a caller believing
        that mail or records were being cleaned up when they were not.

        @throw WATCH-POLICY-ERROR a removal action was configured but the provider does not implement
        one
    */
    private applySupersededAction(list<hash<WatchItemInfo>> superseded) {
        if (superseded_action != WSA_IGNORE) {
            throw "WATCH-POLICY-ERROR", sprintf("watch provider %y was configured with superseded action %y but "
                "does not implement one", self.className(), superseded_action);
        }
    }

    #! Logs an informational message about a polling cycle
    /** Overridden by providers that have a logger; the default implementation discards the message.
    */
    private logWatchInfo(string fmt, ...) {
        # default: ignore; subclasses should override for proper logging
    }

    #! Logs a debug-level message about a polling cycle
    /** Overridden by providers that have a logger; the default implementation discards the message.

        @since DataProvider 3.6
    */
    private logWatchDebug(string fmt, ...) {
        # default: ignore; subclasses should override for proper logging
    }

    #! Returns a short description of the current durable cursor for log messages
    /** Providers whose cursor has a compact rendering override this so that a cursor-discard log
        line can be read without cross-referencing the persisted checkpoint.

        The default implementation returns @ref nothing.

        @since DataProvider 3.6
    */
    private *string getWatchCursorDescription() {
    }
}
}
