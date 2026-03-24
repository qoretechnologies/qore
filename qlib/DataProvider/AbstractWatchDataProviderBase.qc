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

    #! Called when all observers have been added to the object
    /** Starts the background polling thread. Safe to call multiple times; only the first call
        starts a thread.
    */
    observersReady() {
        AutoLock al(sleep_m);
        if (stop_flag) {
            return;
        }
        wg.add();
        background pollThread();
    }

    #! Called to stop generating events
    stopEvents() {
        stopEventsIntern();
    }

    #! Returns @ref True if the polling thread is running
    bool isActive() {
        return !stop_flag;
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
}

}
