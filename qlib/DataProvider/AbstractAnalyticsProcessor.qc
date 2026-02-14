# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractAnalyticsProcessor class definition

/** AbstractAnalyticsProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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

/** @defgroup analytics_mode_constants Analytics Processor Mode Constants
    @since %DataProvider 3.3
*/
#/@{
#! Window-level analytics: accumulate records, emit one event per field per window
public const ANALYTICS_MODE_WINDOW = 0;
#! Record-level analytics: emit one event per record per field, maintaining a sliding window
public const ANALYTICS_MODE_RECORD = 1;
#/@}

#! Base analytics event hashdecl with common fields
/** @since %DataProvider 3.3
*/
public hashdecl AnalyticsEventInfo {
    #! The type of analytics event (e.g. "statistics", "moving-average", "trend", "anomaly")
    string event_type;
    #! The field name this event was computed for
    string field;
    #! The configured window size
    int window_size;
    #! The total number of windows or records processed for this field
    int window_count;
}

#! Statistics event hashdecl for running statistics analytics
/** @since %DataProvider 3.3
*/
public hashdecl StatisticsEventInfo inherits AnalyticsEventInfo {
    #! Number of values in this window
    int count;
    #! Sum of values in this window
    float sum;
    #! Mean (average) of values in this window
    float avg;
    #! Minimum value in this window
    float min_val;
    #! Maximum value in this window
    float max_val;
    #! Sample standard deviation of values in this window
    float stddev;
    #! Sample variance of values in this window
    float variance;
}

#! Moving average event hashdecl
/** @since %DataProvider 3.3
*/
public hashdecl MovingAverageEventInfo inherits AnalyticsEventInfo {
    #! Simple moving average over the window
    float sma;
    #! Exponential moving average
    float ema;
    #! The alpha parameter used for EMA
    float ema_alpha;
}

#! Trend analysis event hashdecl
/** @since %DataProvider 3.3
*/
public hashdecl TrendEventInfo inherits AnalyticsEventInfo {
    #! Slope of the linear regression line
    float slope;
    #! Y-intercept of the linear regression line
    float intercept;
    #! R-squared (coefficient of determination)
    float r_squared;
    #! Trend direction: "rising", "falling", or "flat"
    string direction;
}

#! Anomaly detection event hashdecl
/** @since %DataProvider 3.3
*/
public hashdecl AnomalyEventInfo inherits AnalyticsEventInfo {
    #! Z-score of the current value
    float z_score;
    #! Configured anomaly threshold
    float threshold;
    #! Whether the value is considered anomalous
    bool is_anomaly;
    #! The actual record value that was tested
    float record_value;
    #! Mean of the current window
    float mean;
    #! Standard deviation of the current window
    float stddev;
}

#! Abstract base class for streaming analytics pipeline processors
/** Analytics processors accumulate data in windows and emit analytics events.
    They inherit from @ref AbstractDataProvider and support two emission modes:
    - Window-level: accumulate N records, emit one event per field per window
    - Record-level: emit one event per record per field, maintaining a sliding window

    @since %DataProvider 3.3
*/
public class AbstractAnalyticsProcessor inherits AbstractDataProvider {
    public {
        #! Base constructor options shared by all analytics processors
        const BaseConstructorOptions = {
            "fields": <DataProviderOptionInfo>{
                "display_name": "Field Names",
                "short_desc": "Comma-separated field names to analyze",
                "desc": "A comma-separated list of field names to compute analytics for; "
                    "each field must contain numeric values in the input records",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Window Size",
                "short_desc": "Number of records in each analytics window",
                "desc": "The number of records to accumulate in each analytics window; "
                    "for window-level processors, an event is emitted when the window is full; "
                    "for record-level processors, this controls the sliding window size",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
            },
        };
    }

    private {
        #! List of field names to analyze
        list<string> fields;

        #! Configured window size
        int window_size;

        #! Per-field circular buffers of accumulated values
        hash<string, list<float>> buffers;

        #! Per-field total record counts
        hash<string, int> counts;

        #! Per-field window counts (number of completed windows or record events)
        hash<string, int> window_counts;

        #! Flag indicating if the processor has been configured
        bool configured = False;
    }

    #! Returns the analytics mode for this processor
    /** @return @ref ANALYTICS_MODE_WINDOW or @ref ANALYTICS_MODE_RECORD
    */
    abstract int getAnalyticsMode();

    #! Analyzes a complete window of values for a field (window-level processors)
    /** @param field the field name
        @param values the list of values in the window
        @param total_count the total number of values seen for this field

        @return a hash with the analytics results
    */
    abstract hash<auto> analyzeWindow(string field, list<float> values, int total_count);

    #! Analyzes a single record value in the context of the sliding window (record-level processors)
    /** @param field the field name
        @param value the current record value
        @param history the historical sliding window of values (excluding the current value)
        @param total_count the total number of values seen for this field

        @return a hash with the analytics results

        @note The @a history parameter contains only values seen @em before the current record.
        This is important for anomaly detection where statistics must be computed from historical
        data only, to avoid the test value dampening its own anomaly score.
    */
    abstract hash<auto> analyzeRecord(string field, float value, list<float> history, int total_count);

    #! Returns the output type for the analytics events
    /** @return the output type for the analytics events produced by this processor
    */
    abstract AbstractDataProviderType getAnalyticsOutputType();

    #! Initializes the analytics processor with the given options
    /** Called by subclass constructors after checkOptions().

        @param options the options hash from the constructor
        @param constructor_options the constructor option definitions to validate against
    */
    initAnalytics(hash<auto> options, hash<auto> constructor_options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", constructor_options, options);
        # parse comma-separated field names
        fields = map trim($1), copts.fields.split(","), trim($1).val();
        if (!fields) {
            throw "CONSTRUCTOR-ERROR", "no valid field names provided in 'fields' option";
        }
        window_size = copts.window_size;
        if (window_size < 1) {
            throw "CONSTRUCTOR-ERROR", sprintf("window_size must be >= 1; got %d", window_size);
        }
        # initialize per-field state
        foreach string field in (fields) {
            buffers{field} = ();
            counts{field} = 0;
            window_counts{field} = 0;
        }
        configured = True;
    }

    #! Returns data provider static info
    /** Must be overridden in subclasses to return the appropriate ProviderInfo
    */
    abstract hash<DataProviderInfo> getStaticInfoImpl();

    #! Processes a single record and returns the result
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR", sprintf("%s: not configured; provide constructor options", self.className());
        }
        list<auto> events;
        foreach string field in (fields) {
            if (!record.hasKey(field)) {
                continue;
            }
            auto raw = record{field};
            int tc = raw.typeCode();
            float value;
            if (tc == NT_INT || tc == NT_FLOAT || tc == NT_NUMBER) {
                value = raw.toFloat();
            } else if (tc == NT_STRING) {
                if (!regex(raw, "^-?\\d+(\\.\\d+)?([eE][+-]?\\d+)?$")) {
                    # skip non-numeric string values
                    continue;
                }
                value = raw.toFloat();
            } else if (tc == NT_BOOLEAN) {
                value = raw ? 1.0 : 0.0;
            } else {
                # skip unsupported types
                continue;
            }
            counts{field}++;
            if (getAnalyticsMode() == ANALYTICS_MODE_WINDOW) {
                push buffers{field}, value;
                if (buffers{field}.lsize() >= window_size) {
                    window_counts{field}++;
                    hash<auto> event = analyzeWindow(field, buffers{field}, counts{field});
                    push events, event;
                    buffers{field} = ();
                }
            } else {
                # record-level mode: capture historical window before adding current value
                list<float> history = buffers{field};
                push buffers{field}, value;
                if (buffers{field}.lsize() > window_size) {
                    shift buffers{field};
                }
                window_counts{field}++;
                hash<auto> event = analyzeRecord(field, value, history, counts{field});
                push events, event;
            }
        }
        if (!events) {
            return;
        }
        if (events.lsize() == 1) {
            return events[0];
        }
        return events;
    }

    #! Flushes any remaining buffered data
    private *list<auto> flushRecordsImpl() {
        if (!configured) {
            return;
        }
        list<auto> events;
        foreach string field in (fields) {
            if (buffers{field}.lsize() > 0) {
                window_counts{field}++;
                hash<auto> event = analyzeWindow(field, buffers{field}, counts{field});
                push events, event;
                buffers{field} = ();
            }
        }
        return events ? events : NOTHING;
    }

    #! Processes records in bulk (hash-of-lists) format
    private *hash<auto> processRecordsBulkImpl(hash<auto> records) {
        list<auto> record_list = AbstractDataProcessor::hashListToRecords(records);
        list<auto> results;
        foreach auto rec in (record_list) {
            *auto result = processRecordImpl(rec);
            if (exists result) {
                if (result.typeCode() == NT_LIST) {
                    results += result;
                } else {
                    push results, result;
                }
            }
        }
        if (!results) {
            return;
        }
        return AbstractDataProcessor::recordsToHashList(results);
    }

    #! Returns True — analytics processors support bulk API
    private bool supportsBulkApiImpl() {
        return True;
    }

    #! Returns the output type for the pipeline processor
    private *AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
        return getAnalyticsOutputType();
    }

    #! Returns the processor capability bitfield
    private int getProcessorCapabilitiesImpl() {
        return DPC_ANALYTICS;
    }
}
}
