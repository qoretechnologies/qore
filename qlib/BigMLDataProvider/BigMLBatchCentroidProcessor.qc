# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BigMLBatchCentroidProcessor class definition

/*  BigMLBatchCentroidProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that creates batch centroid assignments using a BigML cluster
/** Accumulates records in a window, creates a temporary BigML source and dataset,
    runs a batch centroid assignment, and emits enriched records with cluster results.

    @since %BigMLDataProvider 1.0
*/
public class BigMLBatchCentroidProcessor inherits BigMLDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "BigMLBatchCentroidProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "cluster": <DataProviderOptionInfo>{
                "display_name": "Cluster Model",
                "short_desc": "BigML cluster resource ID",
                "desc": "The BigML resource ID of the cluster model to use for batch centroid "
                    "assignment (e.g., \"cluster/abc123\")",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "input_fields": <DataProviderOptionInfo>{
                "display_name": "Input Fields",
                "short_desc": "Record fields to send as input data",
                "desc": "A comma-separated list of field names from the input records to include "
                    "in the batch centroid dataset",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Window Size",
                "short_desc": "Number of records to batch together",
                "desc": "The number of records to accumulate before sending a batch centroid "
                    "request; must be at least 2",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
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
        string cluster_resource;
        list<string> input_fields;
        int window_size;
        int poll_interval_ms = 2000;
        int timeout_ms = 300000;
        list<hash<auto>> buffer;
        bool configured = False;
    }

    constructor() {
    }

    constructor(hash<auto> options) : BigMLDataProviderBase(options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        cluster_resource = copts.cluster;
        input_fields = map trim($1), copts.input_fields.split(","), trim($1).val();
        if (!input_fields) {
            throw "CONSTRUCTOR-ERROR", "no valid field names provided in 'input_fields' option";
        }
        window_size = copts.window_size;
        if (window_size < 2) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("window_size must be >= 2; got %d", window_size);
        }
        if (exists copts.poll_interval_ms) { poll_interval_ms = copts.poll_interval_ms; }
        if (exists copts.timeout_ms) { timeout_ms = copts.timeout_ms; }
        buffer = ();
        configured = True;
    }

    string getName() {
        return "batch-centroid";
    }

    *string getDesc() {
        return "Creates batch centroid assignments using a BigML cluster model; accumulates records, "
            "uploads them to BigML, runs batch centroid assignment, and emits each record with its "
            "cluster assignment";
    }

    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "BigMLBatchCentroidProcessor: not configured; provide constructor options";
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
            hash<auto> input_data;
            foreach string f in (input_fields) {
                if (exists record{f}) {
                    input_data{f} = record{f};
                }
            }
            hash<auto> response = doPost("centroid", {
                "cluster": cluster_resource,
                "input_data": input_data,
            });
            return (<BigMLCentroidEventInfo>{
                "event_type": "bigml-centroid",
                "centroid_name": response.centroid_name ?? "",
                "centroid_id": response.centroid_id ?? "",
                "distance": response.distance ?? 0.0,
                "resource": response.resource ?? "",
                "record_values": input_data,
            },);
        }
        buffer = ();
        return;
    }

    private list<auto> processWindow(list<hash<auto>> records) {
        # Create CSV data
        string csv_data = input_fields.join(",") + "\n";
        foreach hash<auto> rec in (records) {
            list<string> row;
            foreach string f in (input_fields) {
                auto val = rec{f};
                if (exists val) {
                    string sval = string(val);
                    if (sval =~ /[,"\n]/) {
                        sval = "\"" + regex_subst(sval, "\"", "\"\"", RE_Global) + "\"";
                    }
                    push row, sval;
                } else {
                    push row, "";
                }
            }
            csv_data += row.join(",") + "\n";
        }

        # Create source -> dataset -> batch centroid
        hash<auto> source = doPost("source", {"data": csv_data,
            "name": sprintf("bigml-batch-centroid-%s", now_us().format("YYYYMMDDHHmmSS"))});
        string source_resource = source.resource;
        source = pollUntilFinished(source_resource, poll_interval_ms, timeout_ms);

        hash<auto> dataset = doPost("dataset", {"source": source_resource});
        string dataset_resource = dataset.resource;
        dataset = pollUntilFinished(dataset_resource, poll_interval_ms, timeout_ms);

        hash<auto> batch = doPost("batchcentroid", {
            "cluster": cluster_resource,
            "dataset": dataset_resource,
            "all_fields": True,
        });
        string batch_resource = batch.resource;
        batch = pollUntilFinished(batch_resource, poll_interval_ms, timeout_ms);

        # Emit events
        list<auto> events;
        for (int i = 0; i < records.lsize(); ++i) {
            hash<auto> input_data;
            foreach string f in (input_fields) {
                input_data{f} = records[i]{f};
            }
            push events, <BigMLCentroidEventInfo>{
                "event_type": "bigml-centroid",
                "centroid_name": "",
                "centroid_id": "",
                "distance": 0.0,
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
            TypedHash::forName("BigMLDataProvider::BigMLCentroidEventInfo"));
    }
}
}
