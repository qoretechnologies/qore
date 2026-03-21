# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreLOFProcessor class definition

/** QoreLOFProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DataProviderML module
public namespace DataProviderML {
#! Pipeline processor that detects anomalies using Local Outlier Factor
/** Accumulates records in windows and uses the ML::LOF algorithm to
    score each record for anomalousness based on local density.

*/
public class QoreLOFProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreLOFProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "fields": <DataProviderOptionInfo>{
                "display_name": "Field Names",
                "short_desc": "Numeric fields to analyze for anomalies",
                "desc": "A comma-separated list of numeric field names from the input records "
                    "to analyze; the algorithm compares each point's local neighborhood "
                    "density to detect outliers; for example: \"temperature,pressure\"; "
                    "at least 1 field is required",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "required": True,
                "input_field_names": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Window Size",
                "short_desc": "Number of records to analyze together",
                "desc": "The number of records to collect before analyzing them for local "
                    "anomalies; all records in a window are compared against each other; "
                    "must be larger than the number of neighbors; must be at least 2.\n\n"
                    "**Sizing guidance**: use at least `50`; should be several times larger "
                    "than `k` (neighborhood size) for reliable results",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "example_value": 100,
            },
            "k": <DataProviderOptionInfo>{
                "display_name": "Neighborhood Size",
                "short_desc": "How many nearby points to compare against",
                "desc": "The number of closest data points used to define each point's local "
                    "neighborhood; larger values consider wider neighborhoods, making the "
                    "algorithm less sensitive to small local variations; must be smaller "
                    "than the window size.\n\n"
                    "**Typical values**: `10`\u2013`20` for most use cases; increase for noisy "
                    "data, decrease for small datasets; default: `20`",
                "type": AbstractDataProviderTypeMap."*int",
                "example_value": 20,
            },
            "threshold": <DataProviderOptionInfo>{
                "display_name": "Anomaly Threshold",
                "short_desc": "Score cutoff for flagging anomalies",
                "desc": "The outlier score cutoff; a score of `1.0` means a point has the "
                    "same density as its neighbors (normal); scores significantly above "
                    "`1.0` indicate the point is in a less dense region than its neighbors "
                    "(anomalous); lower thresholds flag more records.\n\n"
                    "**Typical values**: `1.5` (standard \u2014 catches moderate outliers), "
                    "`2.0` (conservative \u2014 only strong outliers); default: `1.5`",
                "type": AbstractDataProviderTypeMap."*float",
                "example_value": 1.5,
            },
        };
    }

    private {
        #! Configured field names
        list<string> fields;

        #! Configured window size
        int window_size;

        #! Number of neighbors
        int k = 20;

        #! Anomaly threshold
        float threshold = 1.5;

        #! Accumulated records
        list<hash<auto>> buffer;

        #! Whether configured
        bool configured = False;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        fields = DataProvider::AbstractDataProvider::parseFieldList(copts.fields);
        if (!fields) {
            throw "CONSTRUCTOR-ERROR", "no valid field names provided in 'fields' option";
        }
        window_size = copts.window_size;
        if (window_size < 2) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("window_size must be >= 2; got %d", window_size);
        }
        if (exists copts.k) {
            k = copts.k;
        }
        if (window_size <= k) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("window_size (%d) must be greater than k (%d); LOF requires at least "
                    "k + 1 data points", window_size, k);
        }
        if (exists copts.threshold) {
            threshold = copts.threshold;
        }
        buffer = ();
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "lof";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Detects anomalous data points using the Local Outlier Factor algorithm; "
            "scores each record based on the local density deviation relative to "
            "its neighbors";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records; emits scored records when window is full
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreLOFProcessor: not configured; provide constructor options";
        }
        push buffer, record;
        if (buffer.lsize() >= window_size) {
            list<auto> results = processWindow(buffer);
            buffer = ();
            return results;
        }
        return;
    }

    #! Flushes remaining buffered data
    /** @note If the buffer contains \c k or fewer records, the records are
        discarded because LOF requires at least \a k + 1 data points
        (k neighbors plus the query point). No results are emitted in this case.
    */
    private *list<auto> flushRecordsImpl() {
        if (!configured || !buffer) {
            return;
        }
        # need at least k+1 records to compute LOF
        if (buffer.lsize() > k) {
            list<auto> results = processWindow(buffer);
            buffer = ();
            return results;
        }
        buffer = ();
        return;
    }

    #! Processes a window of records through LOF
    private list<auto> processWindow(list<hash<auto>> records) {
        # extract matrix from fields
        list<list<float>> matrix;
        foreach hash<auto> rec in (records) {
            list<float> row;
            foreach string f in (fields) {
                push row, DataProvider::AbstractDataProvider::getFieldValue(rec, f).toFloat();
            }
            push matrix, row;
        }

        # create and fit LOF model
        int actual_k = k < matrix.lsize() ? k : matrix.lsize() - 1;
        ML::LOF lof({
            "k": actual_k,
            "threshold": threshold,
        });
        lof.fitMatrix(matrix);

        # score all records
        list<hash<ML::LOFResult>> scores = lof.scoreMatrix(matrix);

        # emit one event per record
        list<auto> events;
        for (int i = 0; i < records.lsize(); ++i) {
            hash<auto> record_values;
            foreach string f in (fields) {
                record_values{f} = DataProvider::AbstractDataProvider::getFieldValue(records[i], f);
            }
            push events, <LOFAnomalyEventInfo>{
                "event_type": "lof",
                "lof_score": scores[i].lof_score,
                "is_anomaly": scores[i].is_anomaly,
                "k_distance": scores[i].k_distance,
                "record_values": record_values,
            };
        }
        return events;
    }

    #! Does not support bulk API
    private bool supportsBulkApiImpl() {
        return False;
    }

    #! Returns the processor capability bitfield
    private int getProcessorCapabilitiesImpl() {
        return DPC_AGGREGATE;
    }

    #! Returns the output type
    private *DataProvider::AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
        return new HashDeclDataType(TypedHash::forName("DataProviderML::LOFAnomalyEventInfo"));
    }
}
}
