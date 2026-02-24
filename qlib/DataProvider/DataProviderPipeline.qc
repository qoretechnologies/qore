# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DataProviderPipeline class definition

/** DataProviderPipeline.qc Copyright 2019 - 2026 Qore Technologies, s.r.o.

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

# assume local scope for variables, do not use "$" signs
%modern
# require type definitions everywhere
#! strict argument handling
# enable all warnings
# allow weak references
%allow-weak-references

#! contains all public definitions in the DataProvider module
public namespace DataProvider {
/** @defgroup pipeline_status_codes Pipeline Status Codes
*/
#/@{
#! Pipeline status: ABORTED
public const PS_ABORTED = "ABORTED";

#! Pipeline status: RUNNING
public const PS_RUNNING = "RUNNING";

#! Pipeline status: IDLE
public const PS_IDLE = "IDLE";
#/@}

#! Pipeline info
public hashdecl PipelineInfo {
    #! The name of the pipeline
    string name;

    #! Start of processing
    *date start_time;

    #! Stop time for processing
    *date stop_time;

    #! Pipeline status
    /** @see @ref pipeline_status_codes for possible values
    */
    string status;

    #! Number of input records submitted
    int record_count;

    #! Number of pipeline queues
    int num_queues;

    #! Flag that indicates if the pipeline is capable of bulk record processing
    /** @see @ref dataprovider_pipeline_bulk_processing
    */
    bool bulk;

    #! Total time processing end to end
    date duration;

    #! Total time processing end to end as a floating-point value in durationSecondsFloat
    float duration_secs;

    #! Records processed per second end to end
    float recs_per_sec;
}

#! Metrics for a single pipeline element (processor, mapper, or queue)
/** @since DataProvider 3.1
*/
public hashdecl PipelineElementMetrics {
    #! Element identifier (format: "queue_id:element_index")
    string element_id;

    #! Element type: "processor", "mapper", "queue"
    string element_type;

    #! Class name for processor/mapper elements
    *string class_name;

    #! Records processed by this element
    int records_processed = 0;

    #! Records output by this element (may differ from input for filters)
    int records_output = 0;

    #! Total processing time in microseconds
    int processing_time_us = 0;

    #! Minimum processing time per record in microseconds
    *int min_processing_time_us;

    #! Maximum processing time per record in microseconds
    *int max_processing_time_us;

    #! Total queue wait time in microseconds (time spent waiting in queue before processing)
    int queue_wait_time_us = 0;

    #! Current queue depth (for queue elements only)
    *int current_queue_depth;

    #! Maximum queue depth observed
    *int max_queue_depth;

    #! Number of times queue was full (backpressure events)
    int queue_full_count = 0;

    #! Timestamp of first record processed by this element
    *date first_record_time;

    #! Timestamp of last record processed by this element
    *date last_record_time;

    #! Number of errors encountered by this element
    int error_count = 0;
}

#! Per-element metrics delta for a single snapshot interval
/** @since DataProvider 3.1
*/
public hashdecl PipelineElementDelta {
    #! FSM state key (same as hash key in PipelineMetricsDelta.element_metrics)
    string element_id;

    #! Records that entered this element in this interval
    int records_processed = 0;

    #! Records that left this element in this interval
    int records_output = 0;

    #! Total processing time in this interval (microseconds)
    int processing_time_us = 0;

    #! Total queue-wait time in this interval (microseconds)
    int queue_wait_time_us = 0;

    #! Errors in this interval
    int error_count = 0;

    #! Backpressure events in this interval
    int queue_full_count = 0;

    #! processing_time_us / records_processed; 0.0 if no records processed in this interval
    float avg_processing_time_us = 0.0;

    #! records_processed / interval_secs for this interval; 0.0 if interval is zero or no records
    float throughput_rps = 0.0;
}

#! Metrics for the interval since the previous snapshot
/** @since DataProvider 3.1
*/
public hashdecl PipelineMetricsDelta {
    #! Milliseconds since the previous snapshot (time-since-start for the first snapshot)
    int interval_ms;

    #! Records submitted to the pipeline in this interval
    int records_submitted = 0;

    #! Records completed in this interval
    int records_completed = 0;

    #! Records completed / interval_secs for this interval; 0.0 if interval_ms == 0
    float throughput_rps = 0.0;

    #! Per-element deltas keyed by FSM state key (same keys as cumulative element_metrics)
    hash<string, hash<PipelineElementDelta>> element_metrics;
}

#! Complete pipeline metrics snapshot
/** @since DataProvider 3.1
*/
public hashdecl PipelineMetricsSnapshot {
    #! Pipeline name
    string pipeline_name;

    #! Pipeline status
    /** @see @ref pipeline_status_codes for possible values
    */
    string status;

    #! Pipeline start time
    *date start_time;

    #! Timestamp when this snapshot was taken
    date snapshot_time;

    #! Total records submitted to the pipeline
    int total_records_submitted = 0;

    #! Total records that have completed processing
    int total_records_completed = 0;

    #! Records currently being processed (in flight)
    int records_in_flight = 0;

    #! Overall throughput in records per second
    float throughput_rps = 0.0;

    #! Average end-to-end latency in microseconds
    float avg_latency_us = 0.0;

    #! Per-element metrics keyed by element ID
    hash<string, hash<PipelineElementMetrics>> element_metrics;

    #! Number of pipeline queues
    int num_queues;

    #! Whether the pipeline is operating in bulk mode
    bool bulk_mode;

    #! Total number of errors across all elements
    int total_errors = 0;

    #! Optional context data passed through from pipeline options
    *hash<auto> user_context;

    #! Delta metrics since the previous snapshot (always populated by getSnapshotIntern())
    hash<PipelineMetricsDelta> delta;
}

#! Pipeline option info
public hashdecl PipelineOptionInfo {
    #! a closure or call reference for debug logging
    /** it must take a single format string and then optional arguments corresponding to the placeholders in the format string
    */
    *code<nothing(string, ...)> debug_log;

    #! a closure or call reference for error logging
    /** it must take a single format string and then optional arguments corresponding to the placeholders in the format string
    */
    *code<nothing(string, ...)> error_log;

    #! a closure or call reference for info logging
    /** it must take a single format string and then optional arguments corresponding to the placeholders in the format string
    */
    *code<nothing(string, ...)> info_log;

    #! a closure or call reference for setting thread-local data in new pipeline queue threads
    /* it is called in the new pipeline queue thread with no arguments
    */
    *code<nothing()> thread_callback;

    #! the name of the pipeline for logging purposes; if this key is not included, a generic name will be generated
    *string name;

    #! Optional metrics collector for receiving periodic metrics updates
    /** The pipeline calls the collector's reportMetrics() method synchronously at intervals
        determined by the collector's getIntervalMs() and getRecordInterval() methods.
        The collector owns any async/export behavior.

        @since DataProvider 3.1
    */
    *AbstractPipelineMetricsCollector metrics_collector;
}

#! Pipeline element
class PipelineQueue {
    public {
        #! Queue ID
        int id;

        #! Parent lock
        Mutex lck;

        #! Parent counter
        Counter cnt;

        #! Queue condition variable
        Condition cond();

        #! Flush condition variable
        Condition flush_cond();

        #! Number of threads waiting data to be removed from the queue
        int queue_waiting = 0;

        #! Number of threads waiting on data
        int data_waiting = 0;

        #! Number of threads waiting on the flush cond
        int flush_waiting = 0;

        #! Data queue
        list<auto> queue;

        #! Maximum queue size
        int size;

        #! TID of the background thread
        int tid;

        #! Pipeline elements
        /** elements are either:
            - AbstractDataProcessor objects
            - list of PipelineQueue objects
        */
        list<auto> elems();

        #! Flush pipeline flag
        bool do_flush;

        #! Data flushed confirmation
        bool data_flushed;

        #! Flag indicating the processing thread is actively processing a record
        /** This is set to True when a record is dequeued and cleared when processing
            (including fan-out to child queues) is complete. Used by waitDone() to
            avoid returning prematurely while the processing thread is still submitting
            records to child queues.
        */
        bool processing = False;

        #! Local stop flag — set by the pipeline's stopInternUnlocked()
        /** Avoids evaluating the weak \c parent reference in the hot loop, which
            could trigger the pipeline destructor in this thread and deadlock on
            \c cnt.waitForZero().
        */
        bool queue_stop = False;

        #! Local abort flag — set by the pipeline's abort()/reportError()
        bool queue_abort = False;

        #! Parent object
        DataProviderPipeline parent;
    }

    #! Creates the object
    constructor(DataProviderPipeline parent, Mutex lck, Counter cnt, int id, int size) {
        self.size = size;
        self.parent := parent;
        self.lck = lck;
        self.cnt = cnt;

        cnt.inc();
        on_error {
            cnt.dec();
        }
        self.id = id;
        Counter run_cnt(1);
        tid = background run(run_cnt);
        # do not return until the background thread is running
        run_cnt.waitForZero();
    }

    #! Returns the pipeline ID
    int getId() {
        return id;
    }

    #! Submits data for processing
    /** @param qdata the data to process

        @throws PIPELINE-SUBMISSION-ABORTED cannot submit data; the pipeline is shutting down or has aborted
    */
    submit(auto qdata) {
        if (queue_abort) {
            throw "PIPELINE-SUBMISSION-ABORTED", sprintf("pipeline %y has aborted", parent.getName());
        }

        parent.logDebug("queue %d data submission: %y", id, qdata);
        bool lock = !lck.lockOwner();
        if (lock) {
            lck.lock();
        }
        on_exit if (lock) {
            lck.unlock();
        }

        while (!queue_stop && queue.lsize() == size) {
            # Record backpressure event when queue is full
            *PipelineMetricsCollector mc = parent.getMetricsCollector();
            if (mc) {
                mc.recordBackpressure(id.toString());
            }
            ++queue_waiting;
            cond.wait(lck);
            --queue_waiting;
        }

        if (queue_stop) {
            throw "PIPELINE-SUBMISSION-ABORTED", sprintf("pipeline %y: cannot submit data; the pipeline is shutting "
                "down", parent.getName());
        }

        # do not use the += operator in case the data is a list
        push queue, qdata;

        # Record queue depth for metrics
        *PipelineMetricsCollector mc = parent.getMetricsCollector();
        if (mc) {
            mc.recordQueueDepth(id.toString(), queue.lsize());
        }

        if (data_waiting) {
            cond.broadcast();
        }
    }

    #! Processing thread
    run(Counter run_cnt) {
        run_cnt.dec();

        parent.registerThread(self);

        lck.lock();
        on_exit lck.unlock();

        runIntern();
    }

    #! Wait for the queue to be empty and idle, then wait for all terminating pipelines to be empty
    /** @note Called in the pipeline lock

        Waits until both the queue is empty AND the processing thread is not actively processing
        a record. This prevents returning prematurely when the processing thread has dequeued
        the last record but is still submitting it to child queues during fan-out.
    */
    waitDone() {
        while (!queue_stop && (queue || processing)) {
            ++queue_waiting;
            cond.wait(lck);
            --queue_waiting;
        }

        if (elems.last().typeCode() == NT_LIST) {
            map $1.waitDone(), elems.last();
        }

        do_flush = True;
        while (!id && !queue_stop && do_flush) {
            if (data_waiting) {
                cond.broadcast();
            }
            ++flush_waiting;
            flush_cond.wait(lck);
            --flush_waiting;
        }

        if (data_flushed) {
            if (elems.last().typeCode() == NT_LIST) {
                map $1.waitDone(), elems.last();
            }
            remove data_flushed;
        }
    }

    /** @note Called in the pipeline lock
    */
    private runIntern() {
        # Decrement the thread counter on exit.  This MUST happen inside
        # runIntern() (while the caller still holds lck) rather than in
        # run()'s on_exit, because after runIntern() returns the run()
        # stack cleanup can drop the last strong reference to the pipeline
        # (via the weak "parent" member), which would fire the pipeline
        # destructor in this thread.  The destructor calls
        # cnt.waitForZero(), so cnt must already be decremented to avoid
        # self-deadlock.
        on_exit cnt.dec();

        #! wait for an event
        while (!queue_stop) {
            if (!queue) {
                if (!id && do_flush) {
                    flushIntern();
                    do_flush = False;
                    if (flush_waiting) {
                        flush_cond.broadcast();
                    }
                }

                ++data_waiting;
                cond.wait(lck);
                --data_waiting;
                continue;
            }

            auto qdata = shift queue;
            processing = True;
            if (queue_waiting) {
                cond.broadcast();
            }

            if (queue_abort) {
                # discard data
                processing = False;
                if (queue_waiting) {
                    cond.broadcast();
                }
                continue;
            }

            softlist<auto> data_recs;
            push data_recs, qdata;

            # Get metrics collector for this processing run
            *PipelineMetricsCollector mc = parent.getMetricsCollector();

            try {
                # iterate each element in the queue
                int elem_index = 0;
                foreach auto elem in (elems) {
                    softlist<auto> new_recs;
                    # iterate each data element in the current data set and submit it to processors
                    foreach auto data_elem in (data_recs) {
                        if (elem instanceof AbstractDataProcessor) {
                            # Measure processing time
                            date start_time = now_us();
                            *list<auto> new_elem_recs = elem.submitAndCollect(data_elem);
                            int processing_us = (now_us() - start_time).durationMicroseconds();

                            # if we have output data, save it for the next processor
                            if (new_elem_recs) {
                                # here we need to use += to concatenate lists
                                new_recs += new_elem_recs;
                            }
                            parent.logDebug("queue %d data processor %y output: %y", id, elem.className(),
                                new_elem_recs);

                            # Record metrics if collector is enabled
                            if (mc) {
                                string element_id = sprintf("%d:%d", id, elem_index);
                                # Count input records - for bulk hash data, count the list size
                                int records_in = 1;
                                if (data_elem.typeCode() == NT_HASH
                                        && data_elem.firstValue().typeCode() == NT_LIST) {
                                    records_in = data_elem.firstValue().lsize();
                                }
                                # Count output records
                                int records_out = 0;
                                foreach auto out_rec in (new_elem_recs) {
                                    if (out_rec.typeCode() == NT_HASH
                                            && out_rec.firstValue().typeCode() == NT_LIST) {
                                        records_out += out_rec.firstValue().lsize();
                                    } else {
                                        ++records_out;
                                    }
                                }
                                mc.recordElementProcessing(element_id, processing_us, records_in, records_out);
                            }
                        } else {
                            # must be the last entry in the list - submitting to child queues
                            # Note: this is NOT completion - records continue processing in child queues
                            map $1.submit(data_elem), elem;
                        }
                    }
                    data_recs = new_recs;
                    ++elem_index;
                }

                # If data reaches the end of the queue without child queues, record completion
                if (data_recs && mc) {
                    foreach auto final_rec in (data_recs) {
                        int records_completed = 1;
                        if (final_rec.typeCode() == NT_HASH && final_rec.firstValue().typeCode() == NT_LIST) {
                            records_completed = final_rec.firstValue().lsize();
                        }
                        mc.recordPipelineComplete(records_completed);
                    }
                }
            } catch (hash<ExceptionInfo> ex) {
                # Record error for the current queue
                if (mc) {
                    mc.recordError(id.toString());
                }
                parent.reportError(self, ex);
            }

            # Mark processing complete and wake waitDone() if waiting
            processing = False;
            if (queue_waiting) {
                cond.broadcast();
            }
        }
    }

    private flushIntern() {
        # Get metrics collector for flush operations
        *PipelineMetricsCollector mc = parent.getMetricsCollector();

        try {
            int elem_index = 0;
            foreach auto elem in (elems) {
                if (elem instanceof AbstractDataProcessor) {
                    # Measure flush processing time
                    date start_time = now_us();
                    *list<auto> flush_results = elem.flushAndCollect();
                    int processing_us = (now_us() - start_time).durationMicroseconds();

                    if (!flush_results) {
                        ++elem_index;
                        continue;
                    }
                    if (!data_flushed) {
                        data_flushed = True;
                    }

                    # Record flush metrics for the element that produced records
                    if (mc) {
                        string element_id = sprintf("%d:%d", id, elem_index);
                        int records_out = 0;
                        foreach auto out_rec in (flush_results) {
                            if (out_rec.typeCode() == NT_HASH && out_rec.firstValue().typeCode() == NT_LIST) {
                                records_out += out_rec.firstValue().lsize();
                            } else {
                                ++records_out;
                            }
                        }
                        # Flush produces records without input, so records_in = 0
                        mc.recordElementProcessing(element_id, processing_us, 0, records_out);
                    }

                    softlist<auto> data_recs = flush_results;
                    parent.logDebug("queue %d data processor %y flush output: %y", id, elem.className(),
                        flush_results);
                    int elem_size = elems.size();
                    # iterate subsequent processor elements in the queue
                    for (int i = $# + 1; i < elem_size; ++i) {
                        softlist<auto> new_recs;
                        # iterate each data element in the current data set and submit it to processors
                        foreach auto data_elem in (data_recs) {
                            if (elems[i] instanceof AbstractDataProcessor) {
                                AbstractDataProcessor elem2 = cast<AbstractDataProcessor>(elems[i]);

                                # Measure processing time
                                start_time = now_us();
                                *list<auto> new_elem_recs = elem2.submitAndCollect(data_elem);
                                processing_us = (now_us() - start_time).durationMicroseconds();

                                if (new_elem_recs) {
                                    # here we need to use += to concatenate lists
                                    new_recs += new_elem_recs;
                                }
                                parent.logDebug("queue %d data processor %y processing flushed output: %y", id,
                                    elem2.className(), new_elem_recs);

                                # Record metrics for processing flushed data
                                if (mc) {
                                    string element_id = sprintf("%d:%d", id, i);
                                    int records_in = 1;
                                    if (data_elem.typeCode() == NT_HASH
                                            && data_elem.firstValue().typeCode() == NT_LIST) {
                                        records_in = data_elem.firstValue().lsize();
                                    }
                                    int records_out = 0;
                                    foreach auto out_rec in (new_elem_recs) {
                                        if (out_rec.typeCode() == NT_HASH
                                                && out_rec.firstValue().typeCode() == NT_LIST) {
                                            records_out += out_rec.firstValue().lsize();
                                        } else {
                                            ++records_out;
                                        }
                                    }
                                    mc.recordElementProcessing(element_id, processing_us, records_in, records_out);
                                }
                            } else {
                                # must be the last entry in the list - submitting to child queues
                                map $1.submit(data_elem), elems[i];
                            }
                        }
                        data_recs = new_recs;
                    }

                    # Record completion for flushed data that reaches end of queue
                    if (data_recs && mc) {
                        foreach auto final_rec in (data_recs) {
                            int records_completed = 1;
                            if (final_rec.typeCode() == NT_HASH && final_rec.firstValue().typeCode() == NT_LIST) {
                                records_completed = final_rec.firstValue().lsize();
                            }
                            mc.recordPipelineComplete(records_completed);
                        }
                    }
                } else {
                    # must be the last entry in the list - propagate flush to child queues
                    map $1.flushIntern(), elem;
                }
                ++elem_index;
            }
        } catch (hash<ExceptionInfo> ex) {
            # Record error for flush operations
            if (mc) {
                mc.recordError(id.toString());
            }
            parent.reportError(self, ex);
        }
    }
}

#! Internal class for collecting pipeline metrics
/** This class provides thread-safe metrics collection for pipeline processing.
    Metrics are collected at key points during pipeline execution and reported
    via a callback at configurable intervals.

    @since DataProvider 3.1
*/
public class PipelineMetricsCollector {
    private {
        #! Metrics lock for thread-safe updates
        Mutex metrics_lock();

        #! Per-element metrics storage (keyed by internal auto-generated "queue_id:elem_index" IDs)
        hash<string, hash<PipelineElementMetrics>> element_metrics;

        #! Optional alias mapping: internal auto-ID → display ID (e.g. FSM state key)
        hash<string, string> element_id_alias;

        #! Total records submitted
        int total_submitted = 0;

        #! Total records completed
        int total_completed = 0;

        #! Timestamps for throughput calculation
        date collection_start;

        #! Last callback invocation time
        *date last_callback_time;

        #! Records processed since last callback
        int records_since_callback = 0;

        #! Previous snapshot time (NOTHING until first getSnapshotIntern() call)
        *date prev_snapshot_time;

        #! Previous total submitted count
        int prev_submitted = 0;

        #! Previous total completed count
        int prev_completed = 0;

        #! Previous element metrics snapshot (internal key → field values)
        hash<string, hash<auto>> prev_element_snap;

        #! Optional source element ID; when set, recordPipelineSubmit() also updates source element metrics
        *string source_element_id;

        #! Abstract metrics collector
        AbstractPipelineMetricsCollector collector;

        #! Flag to ensure shutdown is only called once
        bool shut_down = False;

        #! Parent pipeline reference (weak to avoid circular reference)
        DataProviderPipeline parent;
    }

    #! Creates the metrics collector
    /** @param parent the parent pipeline
        @param collector the abstract metrics collector implementation
    */
    constructor(DataProviderPipeline parent, AbstractPipelineMetricsCollector collector) {
        self.parent := parent;
        self.collector = collector;
        collection_start = now_us();
    }

    #! Initializes an element for metrics collection
    /** @param element_id the unique element identifier (format: "queue_id:element_index")
        @param element_type the element type: "processor", "mapper", or "queue"
        @param class_name optional class name for processor/mapper elements
        @param display_id optional display identifier; when set, getSnapshot() uses this key
               instead of element_id in the output (e.g. an FSM state key)
    */
    initElement(string element_id, string element_type, *string class_name, *string display_id) {
        metrics_lock.lock();
        on_exit metrics_lock.unlock();

        if (!element_metrics{element_id}) {
            element_metrics{element_id} = <PipelineElementMetrics>{
                "element_id": element_id,
                "element_type": element_type,
                "class_name": class_name,
            };
        }
        if (display_id) {
            element_id_alias{element_id} = display_id;
        }
    }

    #! Sets the source element ID so that recordPipelineSubmit() also updates source element metrics
    /** @param element_id the element identifier for the source element (must already be initialized
               via initElement())

        When set, each call to recordPipelineSubmit() will increment the source element's
        records_processed and records_output and trigger maybeGetSnapshot(), so that source
        element metrics appear in periodic delta snapshots — not only in the final COMPLETE snapshot.

        @since DataProvider 3.1
    */
    setSourceElement(string element_id) {
        metrics_lock.lock();
        on_exit metrics_lock.unlock();
        if (!element_metrics.hasKey(element_id)) {
            throw "PIPELINE-ERROR", sprintf("element %y not initialized; call initElement() first", element_id);
        }
        source_element_id = element_id;
    }

    #! Records records being submitted to an element for processing
    /** @param element_id the element identifier
        @param count number of records (default: 1)
    */
    recordElementInput(string element_id, int count = 1) {
        metrics_lock.lock();
        on_exit metrics_lock.unlock();

        if (!element_metrics{element_id}.first_record_time) {
            element_metrics{element_id}.first_record_time = now_us();
        }
        element_metrics{element_id}.records_processed += count;
        element_metrics{element_id}.last_record_time = now_us();
    }

    #! Records records output from an element
    /** @param element_id the element identifier
        @param count number of records output (default: 1)
    */
    recordElementOutput(string element_id, int count = 1) {
        metrics_lock.lock();
        on_exit metrics_lock.unlock();

        element_metrics{element_id}.records_output += count;
    }

    #! Records processing time for an element
    /** @param element_id the element identifier
        @param processing_us total processing time in microseconds for this batch
        @param records_in number of input records processed in this batch (default: 1)
        @param records_out number of output records produced in this batch (default: same as records_in)
        @param queue_wait_us queue wait time in microseconds (time spent waiting before processing)

        @note For accurate per-record min/max tracking, the processing_us is divided by records_in
              to compute per-record averages. For bulk operations, this provides approximate
              per-record timing.
    */
    recordElementProcessing(string element_id, int processing_us, int records_in = 1,
            *int records_out, int queue_wait_us = 0) {
        # Default records_out to records_in if not specified
        if (!exists records_out) {
            records_out = records_in;
        }

        # Read collector thresholds before acquiring lock to avoid holding the lock
        # while calling into collector code that may block or throw
        int ri;
        int ims;
        try {
            ri = collector.getRecordInterval();
            ims = collector.getIntervalMs();
        } catch (hash<ExceptionInfo> ex) {
            parent.logError("metrics collector error reading thresholds: %s: %s", ex.err, ex.desc);
            ri = 0;
            ims = 0;
        }

        *hash<PipelineMetricsSnapshot> snapshot;
        {
            metrics_lock.lock();
            on_exit metrics_lock.unlock();

            # Update timestamps
            if (!element_metrics{element_id}.first_record_time) {
                element_metrics{element_id}.first_record_time = now_us();
            }
            element_metrics{element_id}.last_record_time = now_us();

            # Update record counts
            element_metrics{element_id}.records_processed += records_in;
            element_metrics{element_id}.records_output += records_out;

            # Update timing
            element_metrics{element_id}.processing_time_us += processing_us;
            element_metrics{element_id}.queue_wait_time_us += queue_wait_us;

            # Calculate per-record processing time for min/max tracking
            int per_record_us = records_in > 0 ? (processing_us / records_in) : processing_us;
            if (!element_metrics{element_id}.min_processing_time_us
                    || per_record_us < element_metrics{element_id}.min_processing_time_us) {
                element_metrics{element_id}.min_processing_time_us = per_record_us;
            }
            if (!element_metrics{element_id}.max_processing_time_us
                    || per_record_us > element_metrics{element_id}.max_processing_time_us) {
                element_metrics{element_id}.max_processing_time_us = per_record_us;
            }

            records_since_callback += records_in;
            snapshot = maybeGetSnapshot(ri, ims);
        }

        # Call collector outside the lock
        if (snapshot) {
            try {
                collector.reportMetrics(snapshot);
            } catch (hash<ExceptionInfo> ex) {
                parent.logError("metrics collector error: %s: %s", ex.err, ex.desc);
            }
        }
    }

    #! Records records entering the pipeline
    /** @param count number of records submitted (default: 1)

        When a source element has been set via setSourceElement(), this method also updates
        the source element's records_processed/records_output and triggers maybeGetSnapshot()
        so that source metrics appear in periodic delta snapshots.

        @note records_since_callback is only incremented when a source element is set, because
        without a source element there are no element-level metrics to snapshot and the pipeline
        queue workers handle their own snapshot triggering via recordElementProcessing().
    */
    recordPipelineSubmit(int count = 1) {
        # First pass: check source_element_id under lock; if set, release lock and fetch thresholds
        bool has_source;
        {
            metrics_lock.lock();
            on_exit metrics_lock.unlock();
            total_submitted += count;
            has_source = exists source_element_id;
        }

        if (!has_source) {
            return;
        }

        # Read collector thresholds outside lock (same pattern as recordElementProcessing)
        int ri;
        int ims;
        try {
            ri = collector.getRecordInterval();
            ims = collector.getIntervalMs();
        } catch (hash<ExceptionInfo> ex) {
            parent.logError("metrics collector error reading thresholds: %s: %s", ex.err, ex.desc);
            ri = 0;
            ims = 0;
        }

        # Second pass: update source element metrics under lock
        *hash<PipelineMetricsSnapshot> snapshot;
        {
            metrics_lock.lock();
            on_exit metrics_lock.unlock();

            # Re-check source_element_id under lock (could have been cleared between passes)
            if (source_element_id) {
                if (!element_metrics{source_element_id}.first_record_time) {
                    element_metrics{source_element_id}.first_record_time = now_us();
                }
                element_metrics{source_element_id}.last_record_time = now_us();
                element_metrics{source_element_id}.records_processed += count;
                element_metrics{source_element_id}.records_output += count;

                records_since_callback += count;
                snapshot = maybeGetSnapshot(ri, ims);
            }
        }

        if (snapshot) {
            try {
                collector.reportMetrics(snapshot);
            } catch (hash<ExceptionInfo> ex) {
                parent.logError("metrics collector error: %s: %s", ex.err, ex.desc);
            }
        }
    }

    #! Returns the total number of records submitted to the pipeline without advancing the delta window
    /** Unlike getSnapshot(), this method does not call getSnapshotIntern() and therefore has no
        side effects on delta computation.

        @return total records submitted so far
    */
    int getTotalSubmitted() {
        metrics_lock.lock();
        on_exit metrics_lock.unlock();
        return total_submitted;
    }

    #! Records records completing the pipeline
    /** @param count number of records completed (default: 1)
    */
    recordPipelineComplete(int count = 1) {
        metrics_lock.lock();
        on_exit metrics_lock.unlock();
        total_completed += count;
    }

    #! Records queue depth for a queue element
    /** @param queue_id the queue identifier
        @param depth current queue depth
    */
    recordQueueDepth(string queue_id, int depth) {
        metrics_lock.lock();
        on_exit metrics_lock.unlock();

        # Initialize element if not already tracked
        if (!element_metrics{queue_id}) {
            element_metrics{queue_id} = <PipelineElementMetrics>{
                "element_id": queue_id,
                "element_type": "queue",
            };
        }
        element_metrics{queue_id}.current_queue_depth = depth;
        if (!element_metrics{queue_id}.max_queue_depth
                || depth > element_metrics{queue_id}.max_queue_depth) {
            element_metrics{queue_id}.max_queue_depth = depth;
        }
    }

    #! Records a backpressure event (queue full)
    /** @param queue_id the queue identifier
    */
    recordBackpressure(string queue_id) {
        metrics_lock.lock();
        on_exit metrics_lock.unlock();

        # Initialize element if not already tracked
        if (!element_metrics{queue_id}) {
            element_metrics{queue_id} = <PipelineElementMetrics>{
                "element_id": queue_id,
                "element_type": "queue",
            };
        }
        ++element_metrics{queue_id}.queue_full_count;
    }

    #! Records an error for an element
    /** @param element_id the element identifier
    */
    recordError(string element_id) {
        metrics_lock.lock();
        on_exit metrics_lock.unlock();

        # Initialize element if not already tracked
        if (!element_metrics{element_id}) {
            element_metrics{element_id} = <PipelineElementMetrics>{
                "element_id": element_id,
                "element_type": "unknown",
            };
        }
        ++element_metrics{element_id}.error_count;
    }

    #! Checks if a metrics report is needed and returns the snapshot if so
    /** @return the snapshot to report, or @ref nothing if no report is needed

        @note Called with lock held; returns with lock held. The caller must invoke
        collector.reportMetrics() outside the lock with the returned snapshot.
    */
    private *hash<PipelineMetricsSnapshot> maybeGetSnapshot(int ri, int ims) {
        date now = now_us();
        bool should_invoke = False;

        # Check record interval threshold
        if (ri > 0 && records_since_callback >= ri) {
            should_invoke = True;
        } else if (ims > 0) {
            # Check time interval threshold
            if (last_callback_time) {
                int elapsed_ms = (now - last_callback_time).durationMilliseconds();
                if (elapsed_ms >= ims) {
                    should_invoke = True;
                }
            } else if ((now - collection_start).durationMilliseconds() >= ims) {
                should_invoke = True;
            }
        }

        if (should_invoke) {
            last_callback_time = now;
            records_since_callback = 0;
            return getSnapshotIntern();
        }
    }

    #! Forces a metrics report with the current metrics
    /** Does nothing if the collector has already been shut down.
    */
    forceCallback() {
        hash<PipelineMetricsSnapshot> snapshot;
        {
            metrics_lock.lock();
            on_exit metrics_lock.unlock();
            if (shut_down) {
                return;
            }
            snapshot = getSnapshotIntern();
            last_callback_time = now_us();
            records_since_callback = 0;
        }
        try {
            collector.reportMetrics(snapshot);
        } catch (hash<ExceptionInfo> ex) {
            parent.logError("metrics collector error: %s: %s", ex.err, ex.desc);
        }
    }

    #! Returns True if the collector has been shut down
    bool isShutDown() {
        metrics_lock.lock();
        on_exit metrics_lock.unlock();
        return shut_down;
    }

    #! Shuts down the collector; safe to call multiple times
    shutdown() {
        metrics_lock.lock();
        if (shut_down) {
            metrics_lock.unlock();
            return;
        }
        shut_down = True;
        metrics_lock.unlock();
        try {
            collector.shutdown();
        } catch (hash<ExceptionInfo> ex) {
            parent.logError("metrics collector shutdown error: %s: %s", ex.err, ex.desc);
        }
    }

    #! Returns a current metrics snapshot (public API)
    hash<PipelineMetricsSnapshot> getSnapshot() {
        metrics_lock.lock();
        on_exit metrics_lock.unlock();
        return getSnapshotIntern();
    }

    #! Returns a current metrics snapshot (internal, must be called with lock held)
    /** @note Side effect: advances the delta window — updates \c prev_snapshot_time, \c prev_submitted,
        \c prev_completed, and \c prev_element_snap so that the next call computes the delta since
        this call.  Two rapid consecutive calls will therefore produce an empty second delta
        (zero records, zero throughput).
    */
    private hash<PipelineMetricsSnapshot> getSnapshotIntern() {
        date now = now_us();
        float duration_secs = (now - collection_start).durationSecondsFloat();
        float throughput = duration_secs > 0 ? (total_completed / duration_secs) : 0.0;

        # Build output element metrics: remap aliases and filter out queue and mapper elements.
        # Internal element_metrics keys are auto-generated "queue_id:elem_index" IDs.
        # When element_id_alias contains an entry for a key, the output uses the alias (e.g. FSM state key).
        # Queue elements (element_type == "queue") and mapper elements (element_type == "mapper") are
        # always filtered out — they are infrastructure, not meaningful FSM states.
        hash<string, hash<PipelineElementMetrics>> out_metrics;
        foreach string k in (keys element_metrics) {
            hash<PipelineElementMetrics> em = element_metrics{k};
            if (em.element_type == "queue" || em.element_type == "mapper") {
                continue;
            }
            *string display_id = element_id_alias{k};
            if (display_id) {
                # Copy the metrics and update just the element_id field to the display alias.
                # (Using hashdecl merge via + would reset un-specified numeric fields to 0.)
                hash<PipelineElementMetrics> remapped = em;
                remapped.element_id = display_id;
                out_metrics{display_id} = remapped;
            } else {
                out_metrics{k} = em;
            }
        }

        # Calculate average latency across reported (non-queue) elements.
        # Note: total_errors is computed from ALL element_metrics (including queues) because
        # recordError() is called with the queue ID, which may have element_type "queue" if
        # recordQueueDepth() created the entry first.
        float total_latency = 0.0;
        int latency_samples = 0;
        foreach hash<auto> em in (out_metrics.pairIterator()) {
            if (em.value.records_processed > 0) {
                total_latency += em.value.processing_time_us + em.value.queue_wait_time_us;
                latency_samples += em.value.records_processed;
            }
        }
        float avg_latency = latency_samples > 0 ? (total_latency / latency_samples) : 0.0;

        int total_errors = 0;
        foreach hash<PipelineElementMetrics> em in (element_metrics.values()) {
            total_errors += em.error_count;
        }

        # Compute delta metrics for the interval since the previous snapshot
        date effective_prev_time = prev_snapshot_time ?? collection_start;
        int interval_ms = (now - effective_prev_time).durationMilliseconds();
        int delta_submitted = total_submitted - prev_submitted;
        int delta_completed = total_completed - prev_completed;
        float delta_secs = interval_ms / 1000.0;
        float delta_throughput = (delta_secs > 0.0) ? (delta_completed / delta_secs) : 0.0;

        hash<string, hash<PipelineElementDelta>> delta_elements;
        foreach string k in (keys element_metrics) {
            hash<PipelineElementMetrics> em = element_metrics{k};
            if (em.element_type == "queue" || em.element_type == "mapper") {
                continue;
            }
            *hash<auto> prev_em = prev_element_snap{k};
            int d_rp  = em.records_processed  - (prev_em.records_processed  ?? 0);
            int d_ro  = em.records_output     - (prev_em.records_output     ?? 0);
            int d_pt  = em.processing_time_us - (prev_em.processing_time_us ?? 0);
            int d_qw  = em.queue_wait_time_us - (prev_em.queue_wait_time_us ?? 0);
            int d_ec  = em.error_count        - (prev_em.error_count        ?? 0);
            int d_qfc = em.queue_full_count   - (prev_em.queue_full_count   ?? 0);
            string out_key = element_id_alias{k} ?? k;
            delta_elements{out_key} = <PipelineElementDelta>{
                "element_id"            : out_key,
                "records_processed"     : d_rp,
                "records_output"        : d_ro,
                "processing_time_us"    : d_pt,
                "queue_wait_time_us"    : d_qw,
                "error_count"           : d_ec,
                "queue_full_count"      : d_qfc,
                "avg_processing_time_us": d_rp > 0 ? (d_pt.toFloat() / d_rp) : 0.0,
                "throughput_rps"        : (delta_secs > 0.0 && d_rp > 0) ? (d_rp / delta_secs) : 0.0,
            };
        }
        hash<PipelineMetricsDelta> snapshot_delta = <PipelineMetricsDelta>{
            "interval_ms"      : interval_ms,
            "records_submitted": delta_submitted,
            "records_completed": delta_completed,
            "throughput_rps"   : delta_throughput,
            "element_metrics"  : delta_elements,
        };

        # Update prev_* state (still inside the lock — safe)
        prev_snapshot_time = now;
        prev_submitted = total_submitted;
        prev_completed = total_completed;
        # Only snapshot elements that appear in delta output (non-queue, non-mapper)
        hash<string, hash<auto>> new_prev;
        foreach string k in (keys element_metrics) {
            hash<PipelineElementMetrics> em = element_metrics{k};
            if (em.element_type == "queue" || em.element_type == "mapper") {
                continue;
            }
            new_prev{k} = {
                "records_processed"  : em.records_processed,
                "records_output"     : em.records_output,
                "processing_time_us" : em.processing_time_us,
                "queue_wait_time_us" : em.queue_wait_time_us,
                "error_count"        : em.error_count,
                "queue_full_count"   : em.queue_full_count,
            };
        }
        prev_element_snap = new_prev;

        return <PipelineMetricsSnapshot>{
            "pipeline_name": parent.getName(),
            "status": parent.aborting() ? PS_ABORTED : (parent.isProcessing() ? PS_RUNNING : PS_IDLE),
            "start_time": collection_start,
            "snapshot_time": now,
            "total_records_submitted": total_submitted,
            "total_records_completed": total_completed,
            "records_in_flight": total_submitted - total_completed,
            "throughput_rps": throughput,
            "avg_latency_us": avg_latency,
            "element_metrics": out_metrics,
            "num_queues": parent.getInfo().num_queues,
            "bulk_mode": parent.getInfo().bulk,
            "total_errors": total_errors,
            "delta": snapshot_delta,
        };
    }
}

#! Defines a class for passing data through record processors
/** Record processing pipelines run in background threads.  Each queue has an integer queue ID; a queue with
    ID 0 is created by default as the initial queue.

    @note Pipeline data can be of any type
*/
public class DataProviderPipeline {
    public {}

    private {
        #! A descriptive name for logging purposes
        string name;

        #! Hash of queues keyed by queue ID
        hash<string, PipelineQueue> pmap;

        #! Bulk flag
        /** @see @ref dataprovider_pipeline_bulk_processing
        */
        bool do_bulk = True;

        #! Locked flag
        bool locked = False;

        #! Pipeline ID sequence generator
        Sequence seq(1);

        #! list of exceptions in pipelines
        list<hash<ExceptionInfo>> error_list;

        #! run start time
        date start_time;

        #! run stop time (set in waitDone())
        date stop_time;

        #! Record count
        int record_count = 0;

        #! Info log closure; takes a single format string and then arguments for format placeholders
        *code<nothing(string, ...)> info_log;

        #! Error log closure; takes a single format string and then arguments for format placeholders
        *code<nothing(string, ...)> error_log;

        #! Debug log closure; takes a single format string and then arguments for format placeholders
        *code<nothing(string, ...)> debug_log;

        #! a closure or call reference for setting thread-local data in new pipeline queue threads
        /* it is called in the new pipeline queue thread with no arguments
        */
        *code<nothing()> thread_callback;

        #! Atomic lock
        Mutex lck();

        #! Stop flag
        bool stop_flag;

        #! Abort flag
        bool abort_flag;

        #! Thread counter
        Counter cnt();

        #! Metrics collector for real-time metrics reporting
        /** @since DataProvider 3.1
        */
        *PipelineMetricsCollector metrics_collector;
    }

    #! Creates the object with the given options
    /** @param opts any options for the pipeline; see @ref PipelineOptionInfo for more information

        @note The object is created with an initial queue with ID 0
    */
    constructor(*hash<PipelineOptionInfo> opts) {
        info_log = opts.info_log;
        error_log = opts.error_log;
        debug_log = opts.debug_log;
        thread_callback = opts.thread_callback;
        name = opts.name ?? sprintf("unnamed pipeline %y", self.uniqueHash());

        # Initialize metrics collector if provided
        if (opts.metrics_collector) {
            metrics_collector = new PipelineMetricsCollector(self, opts.metrics_collector);
        }

        lck.lock();
        on_exit lck.unlock();

        pmap."0" = new PipelineQueue(self, lck, cnt, 0, 1);
    }

    #! Copy constructor; creates an empty pipeline with the same configuration as the original
    /**
    */
    copy() {
        # create new synchronization objects
        lck = new Mutex();
        seq = new Sequence();
        cnt = new Counter();

        # ensure the copy is reset
        resetIntern();

        # create new pipeline queues
        pmap = map {$1.key: copyPipeline($1.value)}, (remove pmap).pairIterator();

        # rename to label as copy
        name = sprintf("copy of %s", name);
    }

    #! Destroys the object
    /** To ensure that the destructor does not throw a \c PIPELINE-FAILED exception, call run() or runAsync() and
        waitDone()

        @throws PIPELINE-FAILED thrown if there are any errors in pipeline processing, in which case the exception
        argument will have a list of exception arguments thrown by pipeline queue threads
    */
    destructor() {
        stopIntern();
        cnt.waitForZero();
        if (metrics_collector && !metrics_collector.isShutDown()) {
            metrics_collector.forceCallback();
            metrics_collector.shutdown();
        }
        throwPipelineException();
    }

    #! Returns the pipeline name
    string getName() {
        return name;
    }

    #! Returns True if the given queue exists, False if not
    bool hasQueue(int id) {
        return exists pmap{id};
    }

    #! Returns True if the object is stopping
    bool stopping() {
        return stop_flag ?? False;
    }

    #! Returns True if the object is aborting
    bool aborting() {
        return abort_flag ?? False;
    }

    #! Registers a new thread
    registerThread(PipelineQueue queue) {
        if (thread_callback) {
            try {
                thread_callback();
            } catch (hash<ExceptionInfo> ex) {
                reportError(queue, ex);
            }
        }
    }

    #! Appends a data processor to the default queue
    /**
        @note The initial queue is queue 0

        @see appendQueue()
    */
    append(AbstractDataProcessor processor) {
        append(0, processor);
    }

    #! Appends a data processor to a queue
    /** @param id the queue ID as returned from appendQueue()
        @param processor the data processor to append to the pipeline queue
        @param custom_element_id optional custom element ID for metrics; when provided this ID is used
               instead of the auto-generated "queue_id:element_index" format, allowing callers to
               associate pipeline elements with meaningful external identifiers (e.g. FSM state keys)
        @param element_type element type for metrics; elements with type "mapper" or "queue" are
               excluded from metrics output snapshots (default: "processor")

        @throws PIPELINE-ERROR invalid queue ID or the queue already terminates in additional queues; the pipeline is
        locked

        @note The initial queue is queue 0

        @see appendQueue()
    */
    append(int id, AbstractDataProcessor processor, *string custom_element_id,
            string element_type = "processor") {
        lck.lock();
        on_exit lck.unlock();

        checkUpdatePipelineIntern(id);

        if (pmap{id}.elems.last() instanceof list<PipelineQueue>) {
            throw "PIPELINE-ERROR", sprintf("queue %d already terminated in additional queues; no more elements can "
                "be added after a queue terminates in additional queues", id);
        }

        pmap{id}.elems += processor;

        logDebug("added data processor %y to queue %d (%d element%s)", processor.className(), id,
            pmap{id}.elems.size(), pmap{id}.elems.size() == 1 ? "" : "s");

        # Initialize element metrics if collector is enabled.
        # Always use the auto-generated "queue_id:elem_index" ID internally so that the
        # PipelineQueue recording calls (which also compute this format) align correctly.
        # When custom_element_id is provided (e.g. an FSM state key), it is stored as a
        # display alias and applied during getSnapshot() output remapping.
        # Elements with element_type "mapper" are filtered from getSnapshot() output.
        if (metrics_collector) {
            string auto_id = sprintf("%d:%d", id, pmap{id}.elems.size() - 1);
            metrics_collector.initElement(auto_id, element_type, processor.className(), custom_element_id);
        }

        if (do_bulk && !processor.supportsBulkApi()) {
            do_bulk = False;
        }
    }

    #! Appends a new queue to an existing pipeline and returns the new queue ID
    /** @param id the queue to which the new queue will be appended

        @return the new queue ID

        @throws PIPELINE-ERROR the pipeline is locked, or the given queue does not exist

        @note The initial queue is queue 0

        @see append(int, AbstractDataProcessor)
    */
    int appendQueue(int id) {
        lck.lock();
        on_exit lck.unlock();

        checkUpdatePipelineIntern(id);

        int new_id = seq.next();
        PipelineQueue queue(self, lck, cnt, new_id, 1);
        pmap{new_id} = queue;

        if (!pmap{id}.elems || (pmap{id}.elems.last() instanceof AbstractDataProcessor)) {
            list<PipelineQueue> pipeline_list();
            push pmap{id}.elems, pipeline_list;
        }

        # add pipeline to the final element list
        pmap{id}.elems[pmap{id}.elems.size() - 1] += queue;
        logDebug("added pipeline %d to pipeline %d (%d element%s)", new_id, id,
            pmap{id}.elems.size(), pmap{id}.elems.size() == 1 ? "" : "s");
        return new_id;
    }

    #! Returns True if the pipeline is processing data
    bool isProcessing() {
        return locked;
    }

    #! Submits data for processing
    submit(auto _data) {
        logInfo("submit data called (%s)", _data.fullType());

        lck.lock();
        on_exit lck.unlock();

        checkSubmitIntern();

        submitIntern(_data);
    }

    #! Submits data for processing
    submitData(AbstractIterator i) {
        lck.lock();
        on_exit lck.unlock();

        checkSubmitIntern();

        map submitDataIntern($1), i;
    }

    #! Submits data for processing
    submit(AbstractDataProviderRecordIterator i) {
        logInfo("submit AbstractDataProviderRecordIterator called");

        lck.lock();
        on_exit lck.unlock();

        checkSubmitIntern();

        if (do_bulk && i.supportsBulkApi()) {
            submitBulkIntern(i.getBulkApi());
        } else {
            map submitDataIntern($1), i;
        }
    }

    #! Submits data for processing
    submit(AbstractDataProviderBulkRecordInterface i) {
        logInfo("submit AbstractDataProviderBulkRecordInterface called");

        lck.lock();
        on_exit lck.unlock();

        checkSubmitIntern();

        if (do_bulk) {
            submitBulkIntern(i);
        } else {
            map submitDataIntern($1), i.getRecordIterator();
        }
    }

    #! Resets the pipeline
    /** @throws PIPELINE-ERROR this method cannot be called while the pipeline is processing data; call abort() or
        waitDone() before resetting if the pipeline is processing data
    */
    reset() {
        lck.lock();
        on_exit lck.unlock();

        checkLockedIntern();

        resetIntern();
    }

    #! Waits for all queues to have processed remaining data
    /** @throws PIPELINE-FAILED thrown if there are any errors in pipeline processing, in which case the exception
        argument will have a list of exception arguments thrown by pipeline queue threads
    */
    waitDone() {
        lck.lock();
        on_exit lck.unlock();

        if (!locked) {
            return;
        }
        # unlock on exit
        on_exit {
            locked = False;
        }

        # wait for primary queue to complete processing
        pmap."0".waitDone();

        # set the stop time
        stop_time = now_us();

        # Send final metrics and shut down collector
        if (metrics_collector) {
            metrics_collector.forceCallback();
            metrics_collector.shutdown();
        }

        logInfo("completed processing %d pipeline%s; status %y errors: %d processing time: %y records processed: %d",
            pmap.size(), pmap.size() == 1 ? "" : "s", error_list ? "ERROR" : "COMPLETE", error_list.size(),
            stop_time - start_time, record_count);

        # throw any exceptions
        throwPipelineException();
    }

    #! Aborts execution of a pipeline in progress
    /** @param ignore_exceptions if True then any processing exceptions are ignored

        @throws PIPELINE-FAILED thrown if ignore_exceptions is not True and there are any errors in pipeline
        processing, in which case the exception argument will have a list of exception arguments thrown by pipeline
        threads

        @note The pipeline must be reset once aborted to be used again
    */
    abort(*bool ignore_exceptions) {
        # do not grab the lock first to interrupt iterators adding data to the pipeline
        abort_flag = True;
        foreach PipelineQueue q in (pmap.iterator()) {
            q.queue_abort = True;
        }

        lck.lock();
        on_exit lck.unlock();

        logInfo("pipeline %y: abort(<%s exceptions>) called with %sactive pipeline", name,
            ignore_exceptions ? "throw" : "ignore", locked ? "" : "in");

        if (!locked) {
            return;
        }

        pmap."0".waitDone();

        stop_time = now_us();

        # Send final metrics and shut down collector
        if (metrics_collector) {
            metrics_collector.forceCallback();
            metrics_collector.shutdown();
        }

        locked = False;

        if (ignore_exceptions) {
            remove error_list;
        } else {
            throwPipelineException();
        }
    }

    #! Returns pipeline info
    /** @return pipeline info; see @ref PipelineInfo for more information

        @note record count and performance intormation is only valid after the pipeline has completed processing
    */
    hash<PipelineInfo> getInfo() {
        string status;
        if (abort_flag) {
            status = PS_ABORTED;
        } else if (locked) {
            status = PS_RUNNING;
        } else {
            status = PS_IDLE;
        }

        date effective_stop_time = stop_time ?? now_us();
        date duration = effective_stop_time - (start_time ?? effective_stop_time);
        float duration_secs = duration.durationSecondsFloat();
        float recs_per_sec = duration_secs ? (record_count / duration_secs) : 0.0;

        return <PipelineInfo>{
            "name": name,
            "start_time": start_time,
            "stop_time": stop_time,
            "status": status,
            "record_count": record_count,
            "num_queues": pmap.size(),
            "bulk": do_bulk,
            "duration": duration,
            "duration_secs": duration_secs,
            "recs_per_sec": recs_per_sec,
        };
    }

    #! Returns the current metrics snapshot, if metrics collection is enabled
    /** @return a metrics snapshot or @ref nothing if metrics collection is not enabled

        @since DataProvider 3.1
    */
    *hash<PipelineMetricsSnapshot> getMetrics() {
        if (metrics_collector) {
            return metrics_collector.getSnapshot();
        }
    }

    #! Returns the metrics collector, if metrics collection is enabled
    /** @return the metrics collector or @ref nothing if metrics collection is not enabled

        @since DataProvider 3.1

        @note This is primarily for internal use by PipelineQueue
    */
    *PipelineMetricsCollector getMetricsCollector() {
        return metrics_collector;
    }

    #! Sets the source element for metrics collection
    /** When set, recordPipelineSubmit() will also update the source element's metrics incrementally,
        so that source element metrics appear in periodic delta snapshots — not only in the final
        COMPLETE snapshot.

        @param element_id the element identifier for the source element

        @note This is a no-op if metrics collection is not enabled (i.e. no metrics_collector was
        configured in PipelineOptionInfo).

        @since DataProvider 3.1
    */
    setSourceElement(string element_id) {
        if (metrics_collector) {
            metrics_collector.setSourceElement(element_id);
        }
    }

    #! Called from a pipeline queue object to report a fatal error durring processing
    reportError(PipelineQueue queue, hash<ExceptionInfo> ex) {
        logError("pipeline %d: %s", queue.getId(), get_exception_string(ex));

        bool lock = !lck.lockOwner();
        if (lock) {
            lck.lock();
        }
        on_exit if (lock) {
            lck.unlock();
        }

        error_list += ex;
        abort_flag = True;
        foreach PipelineQueue q in (pmap.iterator()) {
            q.queue_abort = True;
        }
    }

    #! Logs to the info log, if set
    logInfo(string fmt, ...) {
        if (info_log) {
            softlist<auto> args += ("PIPELINE " + name + " INFO: " + fmt);
            if (argv) {
                args += argv;
            }
            call_function_args(info_log, args);
        }
    }

    #! Logs to the error log, if set
    logError(string fmt, ...) {
        if (error_log) {
            softlist<auto> args += ("PIPELINE " + name + " ERROR: " + fmt);
            if (argv) {
                args += argv;
            }
            call_function_args(error_log, args);
        }
    }

    #! Logs to the debug log, if set
    logDebug(string fmt, ...) {
        if (debug_log) {
            softlist<auto> args += ("PIPELINE " + name + " DEBUG: " + fmt);
            if (argv) {
                args += argv;
            }
            call_function_args(debug_log, args);
        }
    }

    #! Called by the copy constructor to copy the queues
    private:internal PipelineQueue copyPipeline(PipelineQueue old_queue) {
        *PipelineQueue queue = pmap{old_queue.id};
        if (!queue) {
            queue = new PipelineQueue(self, lck, cnt, old_queue.id, old_queue.size);
            pmap{old_queue.id} = queue;
        }

        queue.elems = old_queue.elems;
        auto last = queue.elems.last();
        if (last.typeCode() == NT_LIST) {
            queue.elems[queue.elems.size() - 1] = map copyPipeline($1), last;
        }
        return queue;
    }

    #! Submits data for processing
    /** Must be called with the lock held
    */
    private:internal submitIntern(auto _data) {
        if (do_bulk && _data.typeCode() == NT_HASH && ((int rec_cnt = _data.firstValue().lsize()) > 1)) {
            logInfo("pipeline %y: bulk input records: %d", name, rec_cnt);
            record_count += rec_cnt;
            # Record metrics for bulk submission
            if (metrics_collector) {
                metrics_collector.recordPipelineSubmit(rec_cnt);
            }
            pmap."0".submit(_data);
        } else {
            submitDataIntern(_data);
        }
    }

    #! Submits a single record for processing
    /** Must be called with the lock held
    */
    private:internal submitDataIntern(auto _data) {
        ++record_count;
        # Record metrics for single record submission
        if (metrics_collector) {
            metrics_collector.recordPipelineSubmit();
        }
        pmap."0".submit(_data);
    }

    #! Submits bulk data for processing
    /** Must be called with the lock held

        @see @ref dataprovider_pipeline_bulk_processing
    */
    private:internal submitBulkIntern(AbstractDataProviderBulkRecordInterface i) {
        while (*hash<auto> recs = i.getValue()) {
            int rec_cnt = recs.firstValue().lsize();
            logInfo("pipeline %y: bulk input records: %d", name, rec_cnt);
            record_count += rec_cnt;
            # Record metrics for bulk submission
            if (metrics_collector) {
                metrics_collector.recordPipelineSubmit(rec_cnt);
            }
            pmap."0".submit(recs);
        }
    }

    #! Resets the pipeline
    /** Must be called with the lock held
    */
    private:internal resetIntern() {
        if (record_count) {
            record_count = 0;
        }

        if (start_time) {
            remove start_time;
        }

        if (stop_time) {
            remove stop_time;
        }

        if (stop_flag) {
            delete stop_flag;
            foreach PipelineQueue q in (pmap.iterator()) {
                q.queue_stop = False;
            }
        }

        if (abort_flag) {
            delete abort_flag;
            foreach PipelineQueue q in (pmap.iterator()) {
                q.queue_abort = False;
            }
        }
    }

    #! Throws an exception if the pipeline cannot be used; locks the pipeline for changes otherwise
    /** Must be called with the lock held
    */
    private:internal checkSubmitIntern() {
        if (!locked) {
            if (!pmap."0".elems) {
                throw "PIPELINE-ERROR", sprintf("pipeline %y has no elements", name);
            }

            if (error_list || abort_flag) {
                throw "PIPELINE-ERROR", sprintf("pipeline %y has an error status; call %s::reset() or %s::abort() before "
                    "submitting new data", name, self.className(), self.className());
            }

            remove stop_time;
            locked = True;
            if (!start_time) {
                logInfo("started processing %d queue%s", pmap.size(), pmap.size() == 1 ? "" : "s");
                start_time = now_us();
            }
        }
    }

    #! Throws an exception if the pipeline is locked
    /** Must be called with the lock held
    */
    private:internal checkLockedIntern() {
        if (locked) {
            throw "PIPELINE-ERROR", sprintf("pipeline %y is running and cannot be changed or reset; call waitDone() "
                "or abort() before making changes to a running pipeline", name);
        }
    }

    #! Throws an exception if errors occured in background pipeline processing
    /** Must be called with the lock held
    */
    private:internal throwPipelineException() {
        # throw exceptions
        if (*list<hash<ExceptionInfo>> errs = remove error_list) {
            throw "PIPELINE-FAILED", sprintf("pipeline %y: %d pipeline%s threw exceptions in processing", name,
                errs.size(), errs.size() == 1 ? "" :"s"), errs;
        }
    }

    #! Check if the given queue exists
    private:internal checkUpdatePipelineIntern(int id) {
        checkLockedIntern();
        if (!pmap{id}) {
            throw "PIPELINE-ERROR", sprintf("pileine %y: queue ID %d does not exist; known pipelines: %y", name, id,
                (map $1.toInt(), keys pmap));
        }
    }

    #! Stops all background pipeline queues
    private:internal stopIntern() {
        lck.lock();
        on_exit lck.unlock();

        stopInternUnlocked();
    }

    #! Stops all background pipeline queues; lock must be held
    private:internal stopInternUnlocked() {
        if (!stop_flag) {
            stop_flag = True;
            logDebug("pipeline %y stopping %d queue thread%s", name, pmap.size(), pmap.size() == 1 ? "" : "s");
            foreach PipelineQueue q in (pmap.iterator()) {
                q.queue_stop = True;
                q.cond.broadcast();
            }
        }
    }
}
}

*list sub get_stack() {
    if (!HAVE_RUNTIME_THREAD_STACK_TRACE)
        return;
    *list stack = get_thread_call_stack();
    if (!stack)
        return;
    splice stack, 0, 2;
    return map $1.type != "new-thread" ? sprintf("%s %s()", get_ex_pos($1), $1.function) : "new-thread", stack;
}


