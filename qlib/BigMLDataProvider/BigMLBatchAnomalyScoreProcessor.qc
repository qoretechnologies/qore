# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BigMLBatchAnomalyScoreProcessor class definition

/*  BigMLBatchAnomalyScoreProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BigMLDataProvider module
public namespace BigMLDataProvider {
#! Pipeline processor that creates batch anomaly scores using a BigML anomaly detector
/** Accumulates records in a window, creates a temporary BigML source and dataset,
    runs a batch anomaly score, and emits enriched records with anomaly scores.

    @since %BigMLDataProvider 1.0
*/
public class BigMLBatchAnomalyScoreProcessor inherits BigMLDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "BigMLBatchAnomalyScoreProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "anomaly": <DataProviderOptionInfo>{
                "display_name": "Anomaly Detector",
                "short_desc": "BigML anomaly detector resource ID",
                "desc": "The BigML resource ID of the anomaly detector to use for batch scoring "
                    "(e.g., \"anomaly/abc123\")",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "input_fields": <DataProviderOptionInfo>{
                "display_name": "Input Fields",
                "short_desc": "Record fields to send as input data",
                "desc": "A comma-separated list of field names from the input records to include "
                    "in the batch anomaly score dataset",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Window Size",
                "short_desc": "Number of records to batch together",
                "desc": "The number of records to accumulate before sending a batch anomaly score "
                    "request; must be at least 2",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
            },
            "threshold": <DataProviderOptionInfo>{
                "display_name": "Anomaly Threshold",
                "short_desc": "Score above which a record is flagged as anomalous",
                "desc": "A value between 0.0 and 1.0; records with an anomaly score at or above "
                    "this threshold are flagged as anomalous; default: 0.5",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "poll_interval_ms": <DataProviderOptionInfo>{
                "display_name": "Poll Interval (ms)",
                "short_desc": "How often to check batch status",
                "desc": "How often (in milliseconds) to check if the batch operation has "
                    "completed; default: 2000",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "timeout_ms": <DataProviderOptionInfo>{
                "display_name": "Timeout (ms)",
                "short_desc": "Maximum wait time for batch completion",
                "desc": "Maximum time (in milliseconds) to wait for the batch operation to "
                    "complete; default: 300000 (5 minutes)",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    private {
        string anomaly_resource;
        list<string> input_fields;
        int window_size;
        float threshold = 0.5;
        int poll_interval_ms = 2000;
        int timeout_ms = 300000;
        list<hash<auto>> buffer;
        bool configured = False;
    }

    constructor() {
    }

    constructor(hash<auto> options) : BigMLDataProviderBase(options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        anomaly_resource = copts.anomaly;
        input_fields = map trim($1), copts.input_fields.split(","), trim($1).val();
        if (!input_fields) {
            throw "CONSTRUCTOR-ERROR", "no valid field names provided in 'input_fields' option";
        }
        window_size = copts.window_size;
        if (window_size < 2) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("window_size must be >= 2; got %d", window_size);
        }
        if (exists copts.threshold) { threshold = copts.threshold; }
        if (exists copts.poll_interval_ms) { poll_interval_ms = copts.poll_interval_ms; }
        if (exists copts.timeout_ms) { timeout_ms = copts.timeout_ms; }
        buffer = ();
        configured = True;
    }

    string getName() {
        return "batch-anomaly-score";
    }

    *string getDesc() {
        return "Creates batch anomaly scores using a BigML anomaly detector; accumulates records, "
            "uploads them to BigML, runs batch scoring, and emits each record with its anomaly score";
    }

    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "BigMLBatchAnomalyScoreProcessor: not configured; provide constructor options";
        }
        push buffer, record;
        if (buffer.lsize() >= window_size) {
            list<auto> results = processWindow(buffer);
            buffer = ();
            return results;
        }
        return;
    }

    private *list<auto> flushRecordsImpl() {
        if (!configured || !buffer) {
            return;
        }
        if (buffer.lsize() >= 2) {
            list<auto> results = processWindow(buffer);
            buffer = ();
            return results;
        }
        # Single record fallback
        if (buffer.lsize() == 1) {
            hash<auto> record = buffer[0];
            buffer = ();
            hash<auto> input_data = extractInputData(record, input_fields);
            hash<auto> response = doPost("anomalyscore", {
                "anomaly": anomaly_resource,
                "input_data": input_data,
            });
            float score = response.score ?? 0.0;
            return (<BigMLAnomalyScoreEventInfo>{
                "event_type": "bigml-anomaly-score",
                "anomaly_score": score,
                "is_anomaly": score >= threshold,
                "resource": response.resource ?? "",
                "record_values": input_data,
            },);
        }
        buffer = ();
        return;
    }

    private list<auto> processWindow(list<hash<auto>> records) {
        # Create CSV data
        string csv_data = buildCsvData(input_fields, records);

        # Create source -> dataset -> batch anomaly score
        hash<auto> source = doPost("source", {"data": csv_data,
            "name": sprintf("bigml-batch-anomaly-%s", now_us().format("YYYYMMDDHHmmSS"))});
        string source_resource = source.resource;
        source = pollUntilFinished(source_resource, poll_interval_ms, timeout_ms);

        hash<auto> dataset = doPost("dataset", {"source": source_resource});
        string dataset_resource = dataset.resource;
        dataset = pollUntilFinished(dataset_resource, poll_interval_ms, timeout_ms);

        hash<auto> batch = doPost("batchanomalyscore", {
            "anomaly": anomaly_resource,
            "dataset": dataset_resource,
            "output_dataset": True,
            "all_fields": True,
        });
        string batch_resource = batch.resource;
        batch = pollUntilFinished(batch_resource, poll_interval_ms, timeout_ms);

        # Download batch results
        list<hash<auto>> result_rows;
        try {
            result_rows = downloadBatchResults(batch_resource);
        } catch () {
            # Fall back to output dataset download if batch download fails
            try {
                *string output_dataset = batch.output_dataset_resource;
                if (output_dataset) {
                    pollUntilFinished(output_dataset, poll_interval_ms, timeout_ms);
                    result_rows = downloadBatchResults(output_dataset);
                }
            } catch () {}
        }

        # Emit events with actual results
        list<auto> events;
        for (int i = 0; i < records.lsize(); ++i) {
            hash<auto> input_data = extractInputData(records[i], input_fields);
            *hash<auto> result = i < result_rows.lsize() ? result_rows[i] : NOTHING;
            float score = result.score.toFloat() ?? 0.0;
            push events, <BigMLAnomalyScoreEventInfo>{
                "event_type": "bigml-anomaly-score",
                "anomaly_score": score,
                "is_anomaly": score >= threshold,
                "resource": batch_resource,
                "record_values": input_data,
            };
        }

        # Clean up
        try { doDelete(batch_resource); } catch () {}
        try { doDelete(dataset_resource); } catch () {}
        try { doDelete(source_resource); } catch () {}

        return events;
    }

    private bool supportsBulkApiImpl() { return False; }
    private int getProcessorCapabilitiesImpl() { return DPC_AGGREGATE; }
    private *DataProvider::AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
        return new HashDeclDataType(
            TypedHash::forName("BigMLDataProvider::BigMLAnomalyScoreEventInfo"));
    }
}
}
