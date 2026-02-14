# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreIsolationForestProcessor class definition

/** QoreIsolationForestProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that detects anomalous data points using Isolation Forest
/** Accumulates records in windows and uses the ML::IsolationForest algorithm to
    score each record for anomalousness when the window is full or on flush.

    @since %DataProviderML 1.0
*/
public class QoreIsolationForestProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreIsolationForestProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "fields": <DataProviderOptionInfo>{
                "display_name": "Field Names",
                "short_desc": "Comma-separated numeric field names to analyze",
                "desc": "A comma-separated list of numeric field names to use as features "
                    "for anomaly detection; at least 1 field is required",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Window Size",
                "short_desc": "Number of records per analysis window",
                "desc": "The number of records to accumulate before fitting the isolation "
                    "forest and scoring all records in the window",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
            },
            "n_trees": <DataProviderOptionInfo>{
                "display_name": "Number of Trees",
                "short_desc": "Number of isolation trees to build",
                "desc": "The number of isolation trees in the forest; more trees give more "
                    "stable scores but take longer to compute; default: 100",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "sample_size": <DataProviderOptionInfo>{
                "display_name": "Sample Size",
                "short_desc": "Number of samples per tree",
                "desc": "The number of samples used to build each isolation tree; smaller "
                    "values make anomalies more distinguishable; default: 256",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "threshold": <DataProviderOptionInfo>{
                "display_name": "Anomaly Threshold",
                "short_desc": "Score threshold for anomaly classification",
                "desc": "Anomaly score threshold (0.0-1.0); records with scores >= threshold "
                    "are classified as anomalous; default: 0.5",
                "type": AbstractDataProviderTypeMap."*float",
            },
        };
    }

    private {
        #! Configured field names
        list<string> fields;

        #! Configured window size
        int window_size;

        #! Number of trees
        int n_trees = 100;

        #! Sample size
        int sample_size = 256;

        #! Anomaly threshold
        float threshold = 0.5;

        #! Accumulated records (list of hashes)
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
        fields = map trim($1), copts.fields.split(","), trim($1).val();
        if (!fields) {
            throw "CONSTRUCTOR-ERROR", "no valid field names provided in 'fields' option";
        }
        window_size = copts.window_size;
        if (window_size < 2) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("window_size must be >= 2; got %d", window_size);
        }
        if (exists copts.n_trees) {
            n_trees = copts.n_trees;
        }
        if (exists copts.sample_size) {
            sample_size = copts.sample_size;
        }
        if (exists copts.threshold) {
            threshold = copts.threshold;
        }
        buffer = ();
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "isolation-forest";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Detects anomalous data points using the Isolation Forest algorithm; "
            "accumulates records in windows, fits a forest, and scores each record "
            "for anomalousness";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records; emits scored records when window is full
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreIsolationForestProcessor: not configured; provide constructor options";
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
    private *list<auto> flushRecordsImpl() {
        if (!configured || !buffer) {
            return;
        }
        # need at least 2 records to fit
        if (buffer.lsize() >= 2) {
            list<auto> results = processWindow(buffer);
            buffer = ();
            return results;
        }
        buffer = ();
        return;
    }

    #! Processes a window of records through Isolation Forest
    private list<auto> processWindow(list<hash<auto>> records) {
        # extract matrix from fields
        list<list<float>> matrix;
        foreach hash<auto> rec in (records) {
            list<float> row;
            foreach string f in (fields) {
                push row, rec{f}.toFloat();
            }
            push matrix, row;
        }

        # create and fit isolation forest
        ML::IsolationForest iforest({
            "n_trees": n_trees,
            "sample_size": sample_size > matrix.lsize() ? matrix.lsize() : sample_size,
            "threshold": threshold,
        });
        iforest.fitMatrix(matrix);

        # score all records
        list<hash<ML::IsolationForestResult>> scores = iforest.scoreMatrix(matrix);

        # emit one event per record
        list<auto> events;
        for (int i = 0; i < records.lsize(); ++i) {
            hash<auto> record_values;
            foreach string f in (fields) {
                record_values{f} = records[i]{f};
            }
            push events, <IsolationForestAnomalyEventInfo>{
                "event_type": "isolation-forest",
                "anomaly_score": scores[i].anomaly_score,
                "is_anomaly": scores[i].is_anomaly,
                "average_path_length": scores[i].average_path_length,
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
        return new HashDeclDataType(TypedHash::forName("DataProviderML::IsolationForestAnomalyEventInfo"));
    }
}
}
