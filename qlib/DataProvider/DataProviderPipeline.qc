# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DataProviderPipeline class definition

/** DataProviderPipeline.qc Copyright 2019 - 2024 Qore Technologies, s.r.o.

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
        if (parent.aborting()) {
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

        while (!parent.stopping() && queue.lsize() == size) {
            ++queue_waiting;
            cond.wait(lck);
            --queue_waiting;
        }

        if (parent.stopping()) {
            throw "PIPELINE-SUBMISSION-ABORTED", sprintf("pipeline %y: cannot submit data; the pipeline is shutting "
                "down", parent.getName());
        }

        # do not use the += operator in case the data is a list
        push queue, qdata;
        if (data_waiting) {
            cond.broadcast();
        }
    }

    #! Processing thread
    run(Counter run_cnt) {
        on_exit {
            cnt.dec();
        }
        run_cnt.dec();

        parent.registerThread(self);

        lck.lock();
        on_exit lck.unlock();

        runIntern();
    }

    #! Wait for the queue to be empty, then wait for all terminating pipelines to be empty
    /** @note Called in the pipeline lock
    */
    waitDone() {
        while (!parent.stopping() && queue) {
            ++queue_waiting;
            cond.wait(lck);
            --queue_waiting;
        }

        if (elems.last().typeCode() == NT_LIST) {
            map $1.waitDone(), elems.last();
        }

        do_flush = True;
        while (!id && !parent.stopping() && do_flush) {
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
        #! wait for an event
        while (!parent.stopping()) {
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
            if (queue_waiting) {
                cond.broadcast();
            }

            if (parent.aborting()) {
                # discard data;
                continue;
            }

            softlist<auto> data_recs;
            push data_recs, qdata;

            try {
                # iterate each element in the queue
                foreach auto elem in (elems) {
                    softlist<auto> new_recs;
                    # iterate each data element in the current data set and submit it to processors
                    foreach auto data_elem in (data_recs) {
                        if (elem instanceof AbstractDataProcessor) {
                            # new data for the next processor
                            *softlist<auto> new_elem_recs;
                            # enqueue data for the next processor in the queue
                            code enqueue = sub (auto new_qdata) {
                                push new_elem_recs, new_qdata;
                            };
                            elem.submit(enqueue, data_elem);
                            # if we have output data, save it for the next processor
                            if (new_elem_recs) {
                                # here we need to use += to concatenate lists
                                new_recs += new_elem_recs;
                            }
                            parent.logDebug("queue %d data processor %y output: %y", id, elem.className(), new_elem_recs);
                        } else {
                            # must be the last entry in the list
                            map $1.submit(data_elem), elem;
                        }
                    }
                    data_recs = new_recs;
                }
            } catch (hash<ExceptionInfo> ex) {
                parent.reportError(self, ex);
            }
        }
    }

    private flushIntern() {
        try {
            foreach auto elem in (elems) {
                if (elem instanceof AbstractDataProcessor) {
                    *softlist<auto> new_elem_recs;
                    code enqueue = sub (auto new_qdata) {
                        push new_elem_recs, new_qdata;
                    };
                    elem.flush(enqueue);
                    if (!new_elem_recs) {
                        continue;
                    }
                    if (!data_flushed) {
                        data_flushed = True;
                    }
                    softlist<auto> data_recs = new_elem_recs;
                    parent.logDebug("queue %d data processor %y flush output: %y", id, elem.className(), new_elem_recs);
                    int elem_size = elems.size();
                    # iterate subsequent processor elements in the queue
                    for (int i = $# + 1; i < elem_size; ++i) {
                        softlist<auto> new_recs;
                        # iterate each data element in the current data set and submit it to processors
                        foreach auto data_elem in (data_recs) {
                            if (elems[i] instanceof AbstractDataProcessor) {
                                AbstractDataProcessor elem2 = cast<AbstractDataProcessor>(elems[i]);
                                remove new_elem_recs;
                                elem2.submit(enqueue, data_elem);
                                if (new_elem_recs) {
                                    # here we need to use += to concatenate lists
                                    new_recs += new_elem_recs;
                                }
                                parent.logDebug("queue %d data processor %y processing flushed output: %y", id, elem.className(), new_elem_recs);
                            } else {
                                # must be the last entry in the list
                                map $1.submit(data_elem), elems[i];
                            }
                        }
                        data_recs = new_recs;
                    }
                } else {
                    # must be the last entry in the list
                    map $1.flushIntern(), elem;
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            parent.reportError(self, ex);
        }
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

        @throws PIPELINE-ERROR invalid queue ID or the queue already terminates in additional queues; the pipeline is
        locked

        @note The initial queue is queue 0

        @see appendQueue()
    */
    append(int id, AbstractDataProcessor processor) {
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

        lck.lock();
        on_exit lck.unlock();

        logInfo("pipeline %y: abort(<%s exceptions>) called with %sactive pipeline", name,
            ignore_exceptions ? "throw" : "ignore", locked ? "" : "in");

        if (!locked) {
            return;
        }

        pmap."0".waitDone();

        stop_time = now_us();

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
        }

        if (abort_flag) {
            delete abort_flag;
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
            map $1.cond.broadcast(), pmap.iterator();
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


