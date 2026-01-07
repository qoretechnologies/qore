# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractDataProcessor class definition

/** AbstractDataProcessor.qc Copyright 2019 - 2024 Qore Technologies, s.r.o.

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
#! Defines an abstract class for accepting data and outputting optionally transformed or filtered data
public class AbstractDataProcessor {
    private {
        #! Thread-local data to set before running the processor
        *hash<auto> thread_local_data;

        #! Thread-local data keys from \a thread_local_data
        *list<string> thread_local_data_keys;
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
        - The default implementation in the base class does nothing; to submit pending data for processing by
          subsequent elements in the pipeline when pipeline processing is finished, call enqueue with the pending data
        - No data should be cached or pending after this call; if the object requires closing or purging of internal
          state to make it reusable, it should be performed in this method
    */
    private flushImpl(code enqueue) {
    }

    #! Submits the data for processing
    /** @param enqueue a closure taking a single argument that enqueues the processed data for the next step in the
        pipeline; if no data should be processed onwards, do not call enqueue; if only one record should be processed
        onwards, then \a enqueue should be called only once; if multiple records are generated from the input data,
        then call it once for each generated record; prototype: @code{.py} code enqueue = sub (auto qdata) {} @endcode
        @param _data the data to process
    */
    private abstract submitImpl(code enqueue, auto _data);

    #! Returns @ref True if the data processor supports bulk operation
    /** @return @ref True if the data processor supports bulk operation

        @see @ref dataprovider_pipeline_bulk_processing
    */
    private abstract bool supportsBulkApiImpl();
}
}
