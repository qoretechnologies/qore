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

        @since DataProvider 3.4
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

    #! Performs a single polling operation
    /** Subclasses must implement this method to fetch records and call
        @ref notifyObservers() for each new record.
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
}

}
