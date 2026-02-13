# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractDataProcessor class definition

/** AbstractDataProcessor.qc Copyright 2019 - 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the DataProvider module
public namespace DataProvider {
/** @defgroup dp_processor_capability_codes Data Processor Capability Codes
    These codes form a bitfield; a processor may combine multiple capabilities

    @since %DataProvider 3.3
*/
#/@{
#! Processor can transform records: 1 input -> 1 output
public const DPC_TRANSFORM = (1 << 0);
#! Processor can filter records: 1 input -> 0 or 1 output
public const DPC_FILTER = (1 << 1);
#! Processor can expand records: 1 input -> N outputs
public const DPC_EXPAND = (1 << 2);
#! Processor can aggregate records: N inputs -> M outputs; uses flush to emit pending
public const DPC_AGGREGATE = (1 << 3);
#! Processor is a data sink: consumes records, writes to external target; may or may not pass through
public const DPC_SINK = (1 << 4);
#/@}

#! Map of DPC_* processor capability codes to descriptive names
/** @since %DataProvider 3.3
*/
public const ProcessorCapabilityNameMap = {
    DPC_TRANSFORM: "TRANSFORM",
    DPC_FILTER: "FILTER",
    DPC_EXPAND: "EXPAND",
    DPC_AGGREGATE: "AGGREGATE",
    DPC_SINK: "SINK",
};

#! Defines an abstract class for accepting data and outputting optionally transformed or filtered data
public class AbstractDataProcessor inherits Qore::Serializable {
    private {
        #! Thread-local data to set before running the processor
        transient *hash<auto> thread_local_data;

        #! Thread-local data keys from \a thread_local_data
        transient *list<string> thread_local_data_keys;

        #! Recursion guard for bidirectional default dispatch between submitImpl and processRecordImpl
        transient bool in_dispatch = False;

        #! Recursion guard for bidirectional default dispatch between flushImpl and flushRecordsImpl
        transient bool in_flush_dispatch = False;
    }

    #! Sets thread-local data to set before running the processor
    setThreadLocalData(*hash<auto> thread_local_data) {
        self.thread_local_data = thread_local_data;
        self.thread_local_data_keys = keys thread_local_data;
    }

    #! Returns the expected type of data to be submitted, if available
    /** @return the expected type of data to be submitted, if available

        @note Calls getExpectedTypeImpl() to provide the return value
    */
    AbstractDataProviderType getExpectedType() {
        return getExpectedTypeImpl() ?? AbstractDataProviderType::anyDataType;
    }

    #! Returns the type of data that will be returned, if available
    /** @return the type of data that will be returned, if available

        @note Calls getReturnTypeImpl() to provide the return value
    */
    AbstractDataProviderType getReturnType() {
        return getReturnTypeImpl() ?? AbstractDataProviderType::anyDataType;
    }

    #! Returns @ref True if the data processor supports bulk operation
    /** @return @ref True if the data processor supports bulk operation

        @note Calls supportsBulkApiImpl() to return the answer
    */
    bool supportsBulkApi() {
        return supportsBulkApiImpl();
    }

    #! Submits the data for processing
    /** @param enqueue a closure taking a single argument that enqueues the processed data for the next step in the
        pipeline; if no data should be processed onwards, do not call enqueue; if only one record should be processed
        onwards, then \a enqueue should be called only once; if multiple records are generated from the input data,
        then call it once for each generated record; prototype: @code{.py} code enqueue = sub (auto qdata) {} @endcode
        @param _data the data to process

        Sets thread-local data before running and clears on exit if any is set

        @note
        - Calls submitImpl() on the data to do the actual processing
        - Accept and return type information is not enforced in this method; it must be enforced in submitImpl()
    */
    submit(code enqueue, auto _data) {
        *hash<auto> tld;
        if (thread_local_data) {
            tld = get_all_thread_data();
            save_thread_data(thread_local_data);
        }
        on_exit if (thread_local_data) {
            delete_thread_data(thread_local_data_keys);
            save_thread_data(tld);
        }
        return submitImpl(enqueue, _data);
    }

    #! This method is called for each pipeline element to permit flushing of any pending data at the end of processing
    /** @param enqueue a closure taking a single argument that enqueues the processed data for the next step in the
        pipeline; if no data should be processed onwards, do not call enqueue; if only one record should be processed
        onwards, then \a enqueue should be called only once; if multiple records are generated, then call it once for
        each generated record; prototype: @code{.py} code enqueue = sub (auto qdata) {} @endcode

        @note
        - Calls flushImpl() on the data to do the actual processing
        - Accept and return type information is not enforced in this method; it must be enforced in flushImpl()
    */
    flush(code enqueue) {
        flushImpl(enqueue);
    }

    #! Submits data for processing and returns all output records as a list
    /** @param _data the data to process

        @return a list of output records, or @ref nothing if no output was generated

        This method uses the closure-based submit() API internally but collects and returns results as a list.
        Each element in the returned list is one output record, preserving the exact semantics of the enqueue
        callback pattern.

        @note Unlike processRecord(), this method correctly handles both list-valued data and multi-output
        processors without ambiguity.

        @since %DataProvider 3.3
    */
    *list<auto> submitAndCollect(auto _data) {
        *list<auto> results;
        code enqueue = sub (auto new_qdata) {
            push results, new_qdata;
        };
        submit(enqueue, _data);
        return results;
    }

    #! Flushes any pending data and returns all output records as a list
    /** @return a list of flushed records, or @ref nothing if no data was flushed

        This method uses the closure-based flush() API internally but collects and returns results as a list.

        @since %DataProvider 3.3
    */
    *list<auto> flushAndCollect() {
        *list<auto> results;
        code enqueue = sub (auto new_qdata) {
            push results, new_qdata;
        };
        flush(enqueue);
        return results;
    }

    #! Processes a single record and returns the result
    /** @param record the input data to process

        @return the processed data, or @ref nothing if the record was filtered out

        Sets thread-local data before running and clears on exit if any is set

        @note Calls processRecordImpl() to do the actual processing

        @note List return values are interpreted as multiple output records; pipeline data should not be a list
        unless intended as multiple outputs.

        @since %DataProvider 3.3
    */
    *auto processRecord(auto record) {
        *hash<auto> tld;
        if (thread_local_data) {
            tld = get_all_thread_data();
            save_thread_data(thread_local_data);
        }
        on_exit if (thread_local_data) {
            delete_thread_data(thread_local_data_keys);
            save_thread_data(tld);
        }
        return processRecordImpl(record);
    }

    #! Processes a list of records and returns the results
    /** @param records a list of input records to process

        @return a list of processed records, or @ref nothing if all records were filtered out

        Sets thread-local data before running and clears on exit if any is set

        @note Calls processRecordsImpl() to do the actual processing

        @since %DataProvider 3.3
    */
    *list<auto> processRecords(list<auto> records) {
        *hash<auto> tld;
        if (thread_local_data) {
            tld = get_all_thread_data();
            save_thread_data(thread_local_data);
        }
        on_exit if (thread_local_data) {
            delete_thread_data(thread_local_data_keys);
            save_thread_data(tld);
        }
        return processRecordsImpl(records);
    }

    #! Processes records in bulk (hash-of-lists) format and returns the results
    /** @param records a hash-of-lists representing bulk records where each key is a field name and each value is a
        list of values for that field

        @return a hash-of-lists of processed records, or @ref nothing if all records were filtered out

        Sets thread-local data before running and clears on exit if any is set

        @note Calls processRecordsBulkImpl() to do the actual processing

        @since %DataProvider 3.3
    */
    *hash<auto> processRecordsBulk(hash<auto> records) {
        *hash<auto> tld;
        if (thread_local_data) {
            tld = get_all_thread_data();
            save_thread_data(thread_local_data);
        }
        on_exit if (thread_local_data) {
            delete_thread_data(thread_local_data_keys);
            save_thread_data(tld);
        }
        return processRecordsBulkImpl(records);
    }

    #! Flushes any pending data and returns the results
    /** @return a list of pending records, or @ref nothing if no pending data

        Sets thread-local data before running and clears on exit if any is set

        @note Calls flushRecordsImpl() to do the actual processing

        @since %DataProvider 3.3
    */
    *list<auto> flushRecords() {
        *hash<auto> tld;
        if (thread_local_data) {
            tld = get_all_thread_data();
            save_thread_data(thread_local_data);
        }
        on_exit if (thread_local_data) {
            delete_thread_data(thread_local_data_keys);
            save_thread_data(tld);
        }
        return flushRecordsImpl();
    }

    #! Returns the processor capability bitfield
    /** @return a bitfield of DPC_* constants describing the processor's capabilities

        @note Calls getProcessorCapabilitiesImpl() to provide the return value

        @since %DataProvider 3.3
    */
    int getProcessorCapabilities() {
        return getProcessorCapabilitiesImpl();
    }

    #! Converts a hash-of-lists to a list of record hashes
    /** @param hash_of_lists a hash where each key value is a list of values

        @return a list of hashes, where each hash has the same keys as the input and each value is taken from the
        corresponding list position

        @since %DataProvider 3.3
    */
    static list<auto> hashListToRecords(hash<auto> hash_of_lists) {
        list<auto> result;
        HashListIterator it(hash_of_lists);
        while (it.next()) {
            push result, it.getValue();
        }
        return result;
    }

    #! Converts a list of record hashes to a hash-of-lists
    /** @param records a list of hashes with the same keys

        @return a hash where each key value is a list of values taken from the corresponding records

        @since %DataProvider 3.3
    */
    static hash<auto> recordsToHashList(list<auto> records) {
        hash<auto> result;
        foreach hash<auto> rec in (records) {
            foreach hash<auto> i in (rec.pairIterator()) {
                push result{i.key}, i.value;
            }
        }
        return result;
    }

    #! Returns the expected type of data to be submitted, if available
    /** This base class method returns @ref nothing; reimplement in subclasses to provide a type
    */
    private *AbstractDataProviderType getExpectedTypeImpl() {
    }

    #! Returns the type of data that will be returned, if available
    /** This base class method returns @ref nothing; reimplement in subclasses to provide a type
    */
    private *AbstractDataProviderType getReturnTypeImpl() {
    }

    #! This method is called for each pipeline element to permit flushing of any pending data at the end of processing
    /** @param enqueue a closure taking a single argument that enqueues the processed data for the next step in the
        pipeline; if no data should be processed onwards, do not call enqueue; if only one record should be processed
        onwards, then \a enqueue should be called only once; if multiple records are generated, then call it once for
        each generated record; prototype: @code{.py} code enqueue = sub (auto qdata) {} @endcode

        @note
        - The default implementation bridges to flushRecordsImpl() and enqueues any results; to submit pending data
          for processing by subsequent elements in the pipeline when pipeline processing is finished, either override
          this method and call enqueue, or override flushRecordsImpl() and return the pending records
        - No data should be cached or pending after this call; if the object requires closing or purging of internal
          state to make it reusable, it should be performed in this method
    */
    private flushImpl(code enqueue) {
        if (in_flush_dispatch) {
            # neither flushImpl nor flushRecordsImpl has been overridden; nothing to do
            return;
        }
        in_flush_dispatch = True;
        on_exit {
            in_flush_dispatch = False;
        }
        *list<auto> results = flushRecordsImpl();
        if (results) {
            map enqueue($1), results;
        }
    }

    #! Submits the data for processing using the closure-based API
    /** @param enqueue a closure taking a single argument that enqueues the processed data for the next step in the
        pipeline; if no data should be processed onwards, do not call enqueue; if only one record should be processed
        onwards, then \a enqueue should be called only once; if multiple records are generated from the input data,
        then call it once for each generated record; prototype: @code{.py} code enqueue = sub (auto qdata) {} @endcode
        @param _data the data to process

        The default implementation bridges to the return-based API by calling processRecordImpl() and enqueuing
        any results.

        @note Subclasses that override submitImpl() do not need to override processRecordImpl() and vice versa.
        However, at least one of the two must be overridden, or a \c PROCESSOR-ERROR exception will be thrown at
        runtime due to infinite recursion detection.
    */
    private submitImpl(code enqueue, auto _data) {
        if (in_dispatch) {
            throw "PROCESSOR-ERROR", sprintf("%s: neither submitImpl() nor processRecordImpl() has been "
                "overridden; at least one must be implemented", self.className());
        }
        in_dispatch = True;
        on_exit {
            in_dispatch = False;
        }
        *auto result = processRecordImpl(_data);
        if (exists result) {
            if (result.typeCode() == NT_LIST) {
                map enqueue($1), result;
            } else {
                enqueue(result);
            }
        }
    }

    #! Returns @ref True if the data processor supports bulk operation
    /** @return @ref True if the data processor supports bulk operation

        The default implementation returns @ref True.

        @see @ref dataprovider_pipeline_bulk_processing
    */
    private bool supportsBulkApiImpl() {
        return True;
    }

    #! Processes a single record and returns the result
    /** @param record the input data to process

        @return the processed data, or @ref nothing if the record was filtered out

        The default implementation bridges to the closure-based API by wrapping submitImpl() with a
        capturing closure.

        @note Subclasses that override processRecordImpl() do not need to override submitImpl() and vice versa.
        However, at least one of the two must be overridden, or a \c PROCESSOR-ERROR exception will be thrown at
        runtime due to infinite recursion detection.

        @since %DataProvider 3.3
    */
    private *auto processRecordImpl(auto record) {
        if (in_dispatch) {
            throw "PROCESSOR-ERROR", sprintf("%s: neither submitImpl() nor processRecordImpl() has been "
                "overridden; at least one must be implemented", self.className());
        }
        in_dispatch = True;
        on_exit {
            in_dispatch = False;
        }
        *list<auto> results;
        code enqueue = sub (auto new_qdata) {
            push results, new_qdata;
        };
        submitImpl(enqueue, record);
        if (!results) {
            return;
        }
        if (results.lsize() == 1) {
            return results[0];
        }
        return results;
    }

    #! Processes a list of records and returns the results
    /** @param records a list of input records to process

        @return a list of processed records, or @ref nothing if all records were filtered out

        The default implementation iterates over each record and calls processRecordImpl().

        @since %DataProvider 3.3
    */
    private *list<auto> processRecordsImpl(list<auto> records) {
        *list<auto> results;
        foreach auto record in (records) {
            *auto result = processRecordImpl(record);
            if (exists result) {
                if (result.typeCode() == NT_LIST) {
                    results += result;
                } else {
                    push results, result;
                }
            }
        }
        return results;
    }

    #! Processes records in bulk (hash-of-lists) format and returns the results
    /** @param records a hash-of-lists representing bulk records

        @return a hash-of-lists of processed records, or @ref nothing if all records were filtered out

        The default implementation passes the hash-of-lists through processRecordImpl() as a single unit, which
        maintains backwards compatibility with processors that handle hash-of-lists directly in submitImpl().

        @since %DataProvider 3.3
    */
    private *hash<auto> processRecordsBulkImpl(hash<auto> records) {
        *auto result = processRecordImpl(records);
        if (!exists result) {
            return;
        }
        if (result.typeCode() == NT_HASH) {
            return result;
        }
        # if the result is not a hash, wrap it in a hash-of-lists
        if (result.typeCode() == NT_LIST) {
            return AbstractDataProcessor::recordsToHashList(result);
        }
        return {"value": (result,)};
    }

    #! Flushes any pending data and returns the results
    /** @return a list of pending records, or @ref nothing if no pending data

        The default implementation bridges to the closure-based flushImpl() with a capturing closure.

        @since %DataProvider 3.3
    */
    private *list<auto> flushRecordsImpl() {
        if (in_flush_dispatch) {
            # neither flushImpl nor flushRecordsImpl has been overridden; nothing to do
            return;
        }
        in_flush_dispatch = True;
        on_exit {
            in_flush_dispatch = False;
        }
        *list<auto> results;
        code enqueue = sub (auto new_qdata) {
            push results, new_qdata;
        };
        flushImpl(enqueue);
        return results;
    }

    #! Returns the processor capability bitfield
    /** @return a bitfield of DPC_* constants describing the processor's capabilities

        The default implementation returns 0 (no declared capabilities).

        @since %DataProvider 3.3
    */
    private int getProcessorCapabilitiesImpl() {
        return 0;
    }
}
}
