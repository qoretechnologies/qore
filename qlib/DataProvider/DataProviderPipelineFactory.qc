# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DataProviderPipelineFactory class definition

/** DataProviderPipelineFactory.qc Copyright 2019 - 2026 Qore Technologies, s.r.o.

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
#! Pipeline factory info
public hashdecl PipelineFactoryInfo {
    #! Options for new pipelines
    *hash<auto> opts;

    #! Number of pipeline queues
    int num_queues;

    #! Flag that indicates if the pipelines to be created are capable of bulk record processing
    bool bulk;
}

#! Internal hashdecl for pipeline queue info
hashdecl PipelineQueueInfo {
    #! The queue ID
    int id;

    #! Queue elements; either AbstractDataProcessor objects or a list of PipelineQueueInfo hashes
    list<auto> elems();

    #! Maximum queue size
    int size;
}

#! A factory class for creating DataProviderPipeline objects
/** @note This object is not thread safe for initialization; it should be initialized in a single thread. Once
    initialized it can be safely used in multiple threads.
*/
public class DataProviderPipelineFactory {
    public {}

    private {
        #! Options for the pipelines
        *hash<PipelineOptionInfo> opts;

        #! Hash of queues keyed by queue ID
        hash<string, hash<PipelineQueueInfo>> pmap;

        #! Bulk flag
        bool do_bulk = True;
    }

    #! Creates the object with the given options
    /** @param opts any options for the pipeline; see @ref PipelineOptionInfo for more information

        @note The object is created with an initial queue with ID 0
    */
    constructor(*hash<PipelineOptionInfo> opts) {
        self.opts = opts;
        pmap."0" = <PipelineQueueInfo>{
            "size": 1,
        };
    }

    #! Returns pipeline factory info
    /** @return pipeline factory info; see @ref PipelineFactoryInfo for more information
    */
    hash<PipelineFactoryInfo> getInfo() {
        return <PipelineFactoryInfo>{
            "opts": opts,
            "num_queues": pmap.size(),
            "bulk": do_bulk,
        };
    }

    #! Creates a new DataProviderPipeline object
    /** @param opts override any options given in the constructor
    */
    DataProviderPipeline create(*hash<PipelineOptionInfo> opts) {
        DataProviderPipeline pipe(self.opts + opts);
        processQueue(pipe, 0, pmap."0");
        return pipe;
    }

    #! Appends a data processor to the default pipeline
    /**
        @note The initial queue is queue 0
    */
    append(AbstractDataProcessor processor) {
        append(0, processor);
    }

    #! Appends a data processor to a pipeline
    /** @param id the queue ID as returned from appendQueue()
        @param processor the data processor to append to the pipeline

        @throws PIPELINE-ERROR invalid queue ID or the queue already terminates in additional queues

        @note The initial queue is queue 0

        @see appendQueue()
    */
    append(int id, AbstractDataProcessor processor) {
        checkUpdateQueue(id);

        if (pmap{id}.elems.last() instanceof hash<PipelineQueueInfo>) {
            throw "PIPELINE-ERROR", sprintf("queue %d already terminated in additional queues; no more elements can "
                "be added after a queue terminates in additional queues", id);
        }

        pmap{id}.elems += processor;

        if (do_bulk && !processor.supportsBulkApi()) {
            do_bulk = False;
        }
    }

    #! Appends a new queue to an existing pipeline and returns the new queue ID
    /** @param id the queue to which the new pipeline will be appended

        @return the new queue ID

        @throw PIPELINE-ERROR the pipeline is locked, or the given queue does not exist

        @note The initial queue is queue 0

        @see append(int, AbstractDataProcessor)
    */
    int appendQueue(int id) {
        checkUpdateQueue(id);

        int new_id = pmap.size();
        hash<PipelineQueueInfo> queue({
            "id": new_id,
            "size": 1,
        });
        pmap{new_id} = queue;

        if (!pmap{id}.elems || (pmap{id}.elems.last() instanceof AbstractDataProcessor)) {
            list<hash<PipelineQueueInfo>> pipeline_list();
            push pmap{id}.elems, pipeline_list;
        }

        # add pipeline to the final element list
        pmap{id}.elems[pmap{id}.elems.size() - 1] += queue;
        return new_id;
    }

    #! Checks if the given queue exists
    private checkUpdateQueue(softstring id) {
        if (!pmap{id}) {
            throw "PIPELINE-ERROR", sprintf("queue %d does not exist; known queues: %y", id, (map $1.toInt(), keys pmap));
        }
    }

    #! Creates a queue in a pipeline
    /** Recursively processes queue elements from queue 0, creating child queues in depth-first
        order (matching the order in which appendQueue() assigned IDs during factory setup).

        Uses the canonical pmap entry for each child queue (which has the elements added via
        append()), not the copy stored in the parent's pipeline_list.
    */
    private processQueue(DataProviderPipeline pipe, int id, hash<PipelineQueueInfo> queue) {
        foreach auto elem in (queue.elems) {
            if (elem instanceof AbstractDataProcessor) {
                pipe.append(id, elem);
            } else {
                foreach hash<PipelineQueueInfo> new_queue in (elem) {
                    int queue_id = pipe.appendQueue(id);
                    if (queue_id != new_queue.id) {
                        throw "PIPELINE-ERROR", sprintf("the pipeline factory configuration is "
                            "inconsistent; adding queue %y; got new ID %y", new_queue, queue_id);
                    }
                    # Use the canonical pmap entry (which has elements added via append()),
                    # not the copy stored in the parent's pipeline_list
                    processQueue(pipe, queue_id, pmap{new_queue.id.toString()});
                }
            }
        }
    }
}
}
