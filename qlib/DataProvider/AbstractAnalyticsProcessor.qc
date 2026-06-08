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

#! Percentile/quantile event hashdecl
/** @since %DataProvider 3.3
*/
public hashdecl PercentileEventInfo inherits AnalyticsEventInfo {
    #! Percentile values keyed by name (e.g. "p25", "p50", "p75", etc.)
    hash<auto> percentiles;
    #! Interquartile range (P75 - P25)
    float iqr;
    #! Minimum value in the window
    float min_val;
    #! Maximum value in the window
    float max_val;
    #! Skewness of the distribution (0 = symmetric, positive = right-skewed)
    float skewness;
    #! Excess kurtosis of the distribution (0 = normal/mesokurtic; Fisher's definition)
    float kurtosis;
    #! Number of values in the window
    int count;
}

#! Correlation event hashdecl
/** @since %DataProvider 3.3
*/
public hashdecl CorrelationEventInfo {
    #! Event type identifier
    string event_type = "correlation";
    #! Window size used
    int window_size;
    #! Number of windows processed
    int window_count;
    #! Pairwise correlations keyed by "fieldA_fieldB"
    hash<auto> correlations;
    #! Full correlation matrix keyed by field name
    hash<auto> matrix;
    #! Number of values in the window
    int count;
}

#! Cross-correlation event hashdecl
/** @since %DataProvider 3.3
*/
public hashdecl CrossCorrelationEventInfo {
    #! Event type identifier
    string event_type = "cross-correlation";
    #! First field name
    string field_x;
    #! Second field name
    string field_y;
    #! Window size used
    int window_size;
    #! Number of windows processed
    int window_count;
    #! Maximum lag computed
    int max_lag;
    #! Correlation at each lag keyed by lag string (e.g. "-3", "0", "2")
    hash<auto> lag_correlations;
    #! Lag with the strongest absolute correlation
    int best_lag;
    #! Correlation value at the best lag
    float best_correlation;
    #! Number of values in the window
    int count;
}

#! Histogram event hashdecl
/** @since %DataProvider 3.3
*/
public hashdecl HistogramEventInfo inherits AnalyticsEventInfo {
    #! Bin edge values (N+1 edges for N bins)
    list<float> bin_edges;
    #! Count of values in each bin
    list<int> bin_counts;
    #! Normalized frequencies (sum to 1.0)
    list<float> bin_frequencies;
    #! Width of each bin (for uniform binning)
    float bin_width;
    #! Number of values in the window
    int count;
}

#! Rate of change event hashdecl
/** @since %DataProvider 3.3
*/
public hashdecl RateOfChangeEventInfo inherits AnalyticsEventInfo {
    #! The actual record value
    float record_value;
    #! First derivative (rate of change)
    float rate_of_change;
    #! Second derivative (acceleration)
    float acceleration;
    #! Percentage change relative to previous value
    float percent_change;
}

#! EWMA control chart event hashdecl
/** @since %DataProvider 3.3
*/
public hashdecl EWMAControlChartEventInfo inherits AnalyticsEventInfo {
    #! The actual record value
    float record_value;
    #! Current EWMA statistic
    float ewma;
    #! Upper control limit
    float ucl;
    #! Lower control limit
    float lcl;
    #! Center/target line
    float center;
    #! Whether the EWMA is outside control limits
    bool out_of_control;
    #! Signal classification: "normal", "upper_violation", or "lower_violation"
    string signal;
}

#! Seasonality detection event hashdecl
/** @since %DataProvider 3.3
*/
public hashdecl SeasonalityDetectionEventInfo inherits AnalyticsEventInfo {
    #! Whether significant periodicity was detected
    bool has_seasonality;
    #! Strongest detected period (0 if none)
    int dominant_period;
    #! Autocorrelation at the dominant period
    float dominant_correlation;
    #! All significant periods with their correlations
    list<hash<auto>> periods;
    #! Full autocorrelation function values
    list<float> autocorrelation;
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
        const hash<string, hash<DataProviderOptionInfo>> BaseConstructorOptions = {
            "fields": <DataProviderOptionInfo>{
                "display_name": "Field Names",
                "short_desc": "Field names to analyze",
                "desc": "A list of field names to compute analytics for; "
                    "each field must contain numeric values in the input records; "
                    "supports dot-notation for nested fields (e.g. `sensor.temperature`)",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "required": True,
                "input_field_names": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Window Size",
                "short_desc": "Number of records in each analytics window",
                "desc": "The number of records to accumulate in each analytics window; "
                    "for window-level processors, an event is emitted when the window is full; "
                    "for record-level processors, this controls the sliding window size.\n\n"
                    "**Sizing guidance**: use `20`\u2013`50` for real-time alerting where fast "
                    "response matters, `100`\u2013`500` for general trend and statistics analysis, "
                    "or `1000`+ for high-frequency data where you need stable long-term "
                    "statistics; the window must be large enough to capture at least one full "
                    "cycle of any pattern you want to detect",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "example_value": 100,
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

        #! Preferred shape for pending flush output
        DataProviderShape pending_output_shape = DataProviderShape::RowRecords;

        #! True if the pending output shape has been set for buffered records
        bool pending_output_shape_set = False;

        #! True while a shaped bulk block is dispatching through processRecordImpl()
        bool processing_shaped_bulk = False;

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
        # parse field names (supports both comma-separated strings and lists)
        fields = AbstractDataProvider::parseFieldList(copts.fields);
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

    #! Records the shape of accumulated input for shape-preserving flushes
    private updatePendingOutputShape(DataProviderShape shape) {
        if (!pending_output_shape_set) {
            pending_output_shape = cast<enum<DataProviderShape>>(shape);
            pending_output_shape_set = True;
            return;
        }
        if (pending_output_shape != shape) {
            pending_output_shape = cast<enum<DataProviderShape>>(DataProviderShape::RowRecords);
        }
    }

    #! Clears pending flush shape state
    private clearPendingOutputShape() {
        pending_output_shape = cast<enum<DataProviderShape>>(DataProviderShape::RowRecords);
        pending_output_shape_set = False;
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
        if (!processing_shaped_bulk) {
            updatePendingOutputShape(DataProviderShape::RowRecords);
        }
        list<auto> events;
        foreach string field in (fields) {
            auto raw = AbstractDataProvider::getFieldValue(record, field);
            *auto event = processFieldValue(field, raw);
            if (exists event) {
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

    #! Processes one analytics field value
    private *hash<auto> processFieldValue(string field, auto raw) {
        if (!exists raw) {
            return;
        }
        int tc = raw.typeCode();
        float value;
        if (tc == NT_INT || tc == NT_FLOAT || tc == NT_NUMBER) {
            value = raw.toFloat();
        } else if (tc == NT_STRING) {
            if (!regex(raw, "^-?\\d+(\\.\\d+)?([eE][+-]?\\d+)?$")) {
                # skip non-numeric string values
                return;
            }
            value = raw.toFloat();
        } else if (tc == NT_BOOLEAN) {
            value = raw ? 1.0 : 0.0;
        } else {
            # skip unsupported types
            return;
        }
        counts{field}++;
        if (getAnalyticsMode() == ANALYTICS_MODE_WINDOW) {
            push buffers{field}, value;
            if (buffers{field}.lsize() >= window_size) {
                window_counts{field}++;
                hash<auto> event = analyzeWindow(field, buffers{field}, counts{field});
                buffers{field} = ();
                return event;
            }
            return;
        }

        # record-level mode: capture historical window before adding current value
        list<float> history = buffers{field};
        push buffers{field}, value;
        if (buffers{field}.lsize() > window_size) {
            shift buffers{field};
        }
        window_counts{field}++;
        return analyzeRecord(field, value, history, counts{field});
    }

    #! Builds event records from any remaining buffered data
    private *list<auto> buildFlushRows() {
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

    #! Flushes any remaining buffered data as row records for direct callers
    private *list<auto> flushRecordsImpl() {
        *list<auto> events = buildFlushRows();
        clearPendingOutputShape();
        return events;
    }

    #! Flushes any remaining buffered data in the negotiated pipeline shape
    private flushImpl(code enqueue) {
        *list<auto> events = buildFlushRows();
        DataProviderShape output_shape = pending_output_shape;
        clearPendingOutputShape();
        AbstractDataProcessor::emitRecordListShape(events, output_shape, enqueue);
    }

    #! Processes records in a given bulk shape
    private *hash<auto> processRecordsBulkShape(hash<auto> records, DataProviderShape shape) {
        updatePendingOutputShape(shape);
        list<auto> results;
        processing_shaped_bulk = True;
        on_exit {
            processing_shaped_bulk = False;
        }
        HashListIterator it(records);
        while (it.next()) {
            *auto result = processRecordImpl(it.getValue());
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

    #! Processes records in bulk (hash-of-lists) format
    private *hash<auto> processRecordsBulkImpl(hash<auto> records) {
        return processRecordsBulkShape(records, DataProviderShape::HashOfLists);
    }

    #! Processes DataFrame or BufferColumns input by reading configured top-level fields directly
    private *auto processColumnarRecordsShape(auto records, DataProviderShape shape) {
        updatePendingOutputShape(shape);
        hash<auto> columns = getAnalyticsFieldColumns(records, shape);
        if (!columns) {
            return;
        }

        int record_count = AbstractDataProcessor::shapeRecordCount(records, shape);
        list<auto> results;
        for (int i = 0; i < record_count; ++i) {
            foreach string field in (fields) {
                if (!columns.hasKey(field)) {
                    continue;
                }
                *auto event = processFieldValue(field, columns{field}[i]);
                if (exists event) {
                    push results, event;
                }
            }
        }
        return AbstractDataProcessor::convertHashListResultShape(
            results ? AbstractDataProcessor::recordsToHashList(results) : NOTHING, shape);
    }

    #! Returns selected top-level input columns for columnar analytics processing
    private *hash<auto> getAnalyticsFieldColumns(auto records, DataProviderShape shape) {
        return AbstractDataProcessor::getTopLevelShapeColumns(records, shape, fields);
    }

    #! Processes data in an explicitly negotiated shape
    /** Analytics processors need record-wise bulk handling so window state and emitted
        event records are derived from logical rows instead of the hash-of-lists block itself.

        @since DataProvider 3.5
    */
    private *auto processRecordsShapeImpl(auto records, DataProviderShape shape) {
        switch (shape) {
            case DataProviderShape::HashOfLists:
                return processRecordsBulkShape(records, shape);
            case DataProviderShape::BufferColumns:
            case DataProviderShape::DataFrameBlock: {
                *auto shaped_result = processColumnarRecordsShape(records, shape);
                if (exists shaped_result) {
                    return shaped_result;
                }
                return AbstractDataProcessor::convertHashListResultShape(
                    processRecordsBulkShape(AbstractDataProcessor::convertShape(
                        records, shape, DataProviderShape::HashOfLists)[0], shape), shape);
            }
            case DataProviderShape::RowRecords:
                return processRecordImpl(records);
        }
        throw "PROCESSOR-SHAPE-ERROR", sprintf("%s cannot process unsupported data shape %s",
            self.className(), AbstractDataProcessor::shapeName(shape));
    }

    #! Returns True — analytics processors support bulk API
    private bool supportsBulkApiImpl() {
        return True;
    }

    #! Returns supported input shapes
    private list<DataProviderShape> supportedInputShapesImpl() {
        return (DataProviderShape::DataFrameBlock, DataProviderShape::BufferColumns,
            DataProviderShape::HashOfLists, DataProviderShape::RowRecords);
    }

    #! Returns supported output shapes
    private list<DataProviderShape> supportedOutputShapesImpl() {
        return (DataProviderShape::DataFrameBlock, DataProviderShape::BufferColumns,
            DataProviderShape::HashOfLists, DataProviderShape::RowRecords);
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
