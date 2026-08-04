# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AbstractWatchDataProviderBase.qc abstract base class for polling Watch data providers

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

#! Abstract base class for polling Watch data providers
/**
    Provides common thread lifecycle management for Watch data providers that poll an external
    API at regular intervals. Subclasses implement @ref pollOnce() to perform the actual data
    fetching and observer notification.

    Features:
    - WaitGroup-based thread lifecycle management
    - Condition-based sleep for instant wakeup on stop (no busy-wait polling)
    - Clean shutdown with @ref stopEventsIntern()

    @par Dispatch order
    A provider whose events represent an <b>ordered feed</b> must dispatch them in ascending source
    order, and must declare that it does so with @ref getSourceOrdering() and
    @ref hasAscendingDispatch(). Order is not a presentation detail: a watch cursor is monotonic, so
    dispatching a newer item before an older one advances the cursor past an item that has not been
    delivered, and a failure at that point drops the older item permanently. A provider whose feed
    has no comparable order declares @ref WSO_UNORDERED; a provider whose upstream ordering has not
    been established declares @ref WSO_UNKNOWN and therefore offers no ordering guarantee at all.

    @ref DataProvider::AbstractOrderedWatchDataProvider "AbstractOrderedWatchDataProvider" implements
    the whole ordered sequence — fetch, sort, cursor filtering, optional collapse, in-order dispatch,
    and advance-on-success — so a provider should only implement @ref pollOnce() directly when its
    feed does not fit that shape.

    @par Durability
    Checkpoint persistence is optional at this layer because %Qore does not own a database:
    - <b>with</b> a persistence callback configured through @ref setCheckpointPersistence(): the
      cursor is durable, so delivery is at-least-once across host reloads and restarts;
    - <b>without</b> one: the cursor lives only in memory and behavior after a restart is
      provider-specific — typically the provider resumes from its configured start position, or from
      the restart time when it has none.

    @anchor watch_removal_semantics
    @par Removal follows the delivery call, not the consumer's work
    A watch provider observes one thing: that raising an event returned without an exception.
    @ref DataProvider::Observable::notifyObservers() "notifyObservers()" lets observer failures
    reach the source, so that single fact does cover a consumer that does its work <b>in the
    delivery thread</b>: such a consumer signals failure by throwing, the cycle stops without
    advancing the cursor, no post-dispatch action runs, and the item is delivered again.

    It does not, and at this layer cannot, cover a consumer that hands the event off to another
    thread, queue, or process. There the delivery call returns as soon as the hand-off succeeds, so
    an option that removes the source item after its event has been raised (ex: deleting mail)
    removes it while the real work is still outstanding, and a consumer that then fails, times out,
    or is restarted mid-processing has lost the source item. Providers must document such options in
    exactly those terms and must not describe them as covering an asynchronous consumer. Workloads
    with an asynchronous consumer leave those options unset and act from the consumer once
    processing has succeeded, using the item identity that every event carries.

    @since DataProvider 3.4
*/
public class AbstractWatchDataProviderBase inherits DataProvider::DelayedObservable {
    private {
        #! WaitGroup for tracking the polling thread
        WaitGroup wg();

        #! Mutex for condition-based sleep
        Mutex sleep_m();

        #! Condition for instant wakeup on stop
        Condition sleep_cond();

        #! Stop flag
        bool stop_flag = False;

        #! True once the polling thread has been started
        bool started = False;

        #! Optional callback that durably stores a provider-owned checkpoint
        *code<nothing(hash<auto>)> checkpoint_saver;

        #! Poll interval in seconds
        int poll_interval_secs;
    }

    #! Creates the object
    /** @param poll_interval_secs the polling interval in seconds
    */
    constructor(int poll_interval_secs) {
        self.poll_interval_secs = poll_interval_secs;
    }

    #! Returns the polling interval in seconds
    int getPollIntervalSeconds() {
        return poll_interval_secs;
    }

    #! Configures durable checkpoint persistence for the watch provider
    /** The checkpoint is restored synchronously before this method returns. Watch providers call the
        callback only after raising an event successfully, so they can offer at-least-once event
        delivery across process restarts.

        This method must be called before @ref observersReady(). A checkpoint is deliberately opaque
        to this base class; subclasses validate and restore their own checkpoint format.

        @param checkpoint the previously stored checkpoint, if any
        @param saver a callback that durably stores a replacement checkpoint before returning

        @throw WATCH-CHECKPOINT-ERROR if polling has already started
        @throw any exception raised while validating the provider-owned checkpoint or storing a
        replacement checkpoint

        @par Example
        A mail integration can restore its last delivered server message ID before observers start,
        then store each new ID transactionally after the message event has been accepted.

        @see getWatchStreamIdentity()

        @since DataProvider 3.6
    */
    setCheckpointPersistence(*hash<auto> checkpoint, code<nothing(hash<auto>)> saver) {
        AutoLock al(sleep_m);
        if (started) {
            throw "WATCH-CHECKPOINT-ERROR", "checkpoint persistence cannot be configured after polling has started";
        }
        if (checkpoint) {
            restoreCheckpoint(checkpoint);
        }
        checkpoint_saver = saver;
    }

    #! Called when all observers have been added to the object
    /** Starts the background polling thread. Safe to call multiple times; only the first call
        starts a thread.
    */
    observersReady() {
        AutoLock al(sleep_m);
        if (stop_flag || started) {
            return;
        }
        started = True;
        wg.add();
        on_error {
            started = False;
            wg.done();
        }
        background pollThread();
    }

    #! Called to stop generating events
    stopEvents() {
        stopEventsIntern();
    }

    #! Returns @ref True if the polling thread is running
    bool isActive() {
        AutoLock al(sleep_m);
        return started && !stop_flag;
    }

    #! Stops event generation and waits for the polling thread to exit
    private stopEventsIntern() {
        {
            AutoLock al(sleep_m);
            stop_flag = True;
            sleep_cond.broadcast();
        }
        # Cancel any in-flight polling operation (e.g., a blocked HTTP request)
        try {
            cancelPollOperation();
        } catch (hash<ExceptionInfo> ex) {
            # Ignore cancel errors to ensure wg.wait() always runs
        }
        wg.wait();
    }

    #! Polling thread main loop
    private pollThread() {
        on_exit wg.done();

        while (!stop_flag) {
            try {
                pollOnce();
            } catch (hash<ExceptionInfo> ex) {
                logPollError(ex);
            }

            # Wait for next poll interval with instant wakeup on stop
            {
                AutoLock al(sleep_m);
                if (!stop_flag) {
                    sleep_cond.wait(sleep_m, poll_interval_secs * 1000);
                }
            }
        }
    }

    #! Returns the ordering of this provider's upstream feed
    /** Used to tell an ordered feed from an unordered one without having to read the implementation.

        The default is @ref WSO_UNKNOWN, which declares that the upstream ordering has not been
        established and that the provider therefore offers no ordering guarantee; every provider
        that raises events representing an ordered feed must override this.

        @since DataProvider 3.6
    */
    string getSourceOrdering() {
        return WSO_UNKNOWN;
    }

    #! Returns @ref True if this provider dispatches events in ascending source order
    /** The default is @ref False, because a provider that implements @ref pollOnce() itself
        dispatches in whatever order its upstream API returned. Providers using
        @ref DataProvider::AbstractOrderedWatchDataProvider "AbstractOrderedWatchDataProvider" get an
        ascending guarantee from the shared template.

        @since DataProvider 3.6
    */
    bool hasAscendingDispatch() {
        return False;
    }

    #! Performs a single polling operation
    /** Subclasses must implement this method to fetch records and call
        @ref notifyObservers() for each new record.

        @par Ordering obligation
        An implementation whose events represent an ordered feed must dispatch them in ascending
        source order and must advance its cursor only after a successful dispatch, stopping the cycle
        at the first failure rather than continuing with later items — continuing advances a
        monotonic cursor past the failed item and drops it. An implementation that cannot meet that
        obligation must declare @ref WSO_UNORDERED (its events have no comparable order) or
        @ref WSO_UNKNOWN (its ordering is unverified, so consumers must not assume one).

        @ref DataProvider::AbstractOrderedWatchDataProvider "AbstractOrderedWatchDataProvider"
        implements these obligations once; prefer it over implementing this method directly.
    */
    abstract private pollOnce();

    #! Called when a polling error occurs
    /** Override this method to provide custom error logging.

        The default implementation silently ignores errors.

        @param ex the exception information
    */
    private logPollError(hash<ExceptionInfo> ex) {
        # Default: ignore; subclasses should override for proper logging
    }

    #! Called during shutdown to cancel any in-flight polling operation
    /** Override this method to cancel blocking I/O (e.g., close a REST client) so the
        polling thread can exit promptly instead of waiting for the operation to complete
        or time out.

        The default implementation does nothing.

        @since DataProvider 3.4
    */
    private cancelPollOperation() {
        # Default: no-op; subclasses should override to cancel in-flight requests
    }

    #! Returns @ref True if the stop flag is set
    /** Useful for subclasses to check during long-running pollOnce() operations
    */
    private bool isStopping() {
        return stop_flag;
    }

    #! Persists a provider-owned checkpoint, if persistence has been configured
    /** The persistence callback must not return until the checkpoint is durable. Exceptions are
        propagated so a provider cannot silently advance beyond a checkpoint that was not stored.

        @note The callback is configured before polling starts and is immutable while the polling
        thread is active.
    */
    private persistCheckpoint(hash<auto> checkpoint) {
        if (checkpoint_saver) {
            checkpoint_saver(checkpoint);
        }
    }

    #! Restores a provider-owned checkpoint
    /** Subclasses override this method to validate and apply their checkpoint format.

        The default implementation rejects non-empty checkpoints because silently ignoring state
        would give a false durability guarantee.
    */
    private restoreCheckpoint(hash<auto> checkpoint) {
        if (checkpoint) {
            throw "WATCH-CHECKPOINT-ERROR", sprintf("watch provider %y does not support checkpoints",
                self.className());
        }
    }

    #! Returns the configuration that defines which logical stream this provider follows
    /** A framework storing checkpoints must key them by the stream they belong to, and only the
        provider knows which of its options define that stream.  Without this, an embedder can only
        key on the whole configuration, which makes editing an operational option — the poll interval,
        whether attachments are fetched, whether consumed items are removed — look like a different
        stream: the checkpoint becomes unreachable and the next poll rescans from the beginning.  For
        a mailbox watch that means redelivering the entire mailbox after an ordinary tuning change.

        Conversely, a key that ignores configuration entirely resumes one stream's cursor against
        another: changing the search criteria would resume at a position established under the
        previous criteria and silently skip everything older than it that the new criteria match.

        Providers therefore declare only the options that define the stream itself — the mailbox, the
        search criteria, the scan origin — and omit everything that only affects how the stream is
        polled or what is done with an item after delivery.  Values must be the configured values, not
        expanded runtime values, so that rotating a credential does not look like a new stream.

        @return the stream-defining configuration, or @ref NOTHING if this provider does not declare
        one, in which case the embedder has no choice but to fall back to the whole configuration

        @note Implementations must report configuration fixed at construction time, so that this can
        be called from any thread without synchronization; it is normally called alongside
        @ref setCheckpointPersistence(), before the polling thread exists.

        @see setCheckpointPersistence()

        @since DataProvider 3.6
    */
    *hash<auto> getWatchStreamIdentity() {
        return getWatchStreamIdentityImpl();
    }

    #! Returns the configuration that defines which logical stream this provider follows
    /** The default implementation returns @ref NOTHING; every watch provider that keeps a cursor
        should override this method.

        @return the stream-defining configuration, or @ref NOTHING if not declared

        @see getWatchStreamIdentity()

        @since DataProvider 3.6
    */
    private *hash<auto> getWatchStreamIdentityImpl() {
        # intentionally empty: no declared stream identity
    }
}

}
