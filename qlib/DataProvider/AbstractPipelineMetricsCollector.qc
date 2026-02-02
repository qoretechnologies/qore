# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractPipelineMetricsCollector class definition

/** AbstractPipelineMetricsCollector.qc Copyright 2019 - 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the DataProvider module
public namespace DataProvider {
#! Abstract interface for pipeline metrics collectors
/** Implementations own the async/export behavior; the pipeline calls reportMetrics() synchronously.

    @since DataProvider 3.1
*/
public class AbstractPipelineMetricsCollector {
    #! Returns the minimum time interval in milliseconds between metrics reports
    /** @return the interval in milliseconds; 0 to disable time-based reporting
    */
    abstract int getIntervalMs();

    #! Returns the minimum number of records between metrics reports
    /** @return the record interval; 0 to disable record-count-based reporting
    */
    abstract int getRecordInterval();

    #! Called synchronously by the pipeline to report metrics
    /** @param snapshot the current pipeline metrics snapshot

        @note Implementations should handle any async export internally
    */
    abstract reportMetrics(hash<PipelineMetricsSnapshot> snapshot);

    #! Called when the pipeline is shutting down
    /** Implementations should flush any pending metrics and release resources
    */
    abstract shutdown();
}
}
