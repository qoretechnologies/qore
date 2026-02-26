# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BigMLBatchProjectionProcessor class definition

/*  BigMLBatchProjectionProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that creates batch projections using a BigML PCA model
/** Accumulates records in a window, creates a temporary BigML source and dataset,
    runs a batch projection, and emits enriched records with projection results.

    @since %BigMLDataProvider 1.1
*/
public class BigMLBatchProjectionProcessor inherits BigMLDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "BigMLBatchProjectionProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "pca": <DataProviderOptionInfo>{
                "display_name": "PCA Resource",
                "short_desc": "BigML PCA resource ID",
                "desc": "The BigML resource ID of the PCA model to use for batch projections "
                    "(e.g., \"pca/abc123\")",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "input_fields": <DataProviderOptionInfo>{
                "display_name": "Input Fields",
                "short_desc": "Record fields to send as input data",
                "desc": "A comma-separated list of field names from the input records to include "
                    "in the batch projection dataset",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Window Size",
                "short_desc": "Number of records to batch together",
                "desc": "The number of records to accumulate before sending a batch projection "
                    "request to BigML; larger windows are more efficient but use more memory; "
                    "must be at least 2",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
            },
            "poll_interval_ms": <DataProviderOptionInfo>{
                "display_name": "Poll Interval (ms)",
                "short_desc": "How often to check batch status",
                "desc": "How often (in milliseconds) to check if the batch projection has "
                    "completed; default: 2000",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "timeout_ms": <DataProviderOptionInfo>{
                "display_name": "Timeout (ms)",
                "short_desc": "Maximum wait time for batch completion",
                "desc": "Maximum time (in milliseconds) to wait for the batch projection to "
                    "complete; default: 300000 (5 minutes)",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    private {
        #! The PCA resource ID
        string pca_resource;

        #! The input field names
        list<string> input_fields;

        #! Window size for batching
        int window_size;

        #! Poll interval for batch status
        int poll_interval_ms = 2000;

        #! Timeout for batch completion
        int timeout_ms = 300000;

        #! Accumulated records
        list<hash<auto>> buffer;

        #! Whether configured
        bool configured = False;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) : BigMLDataProviderBase(options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        pca_resource = copts.pca;
        input_fields = map trim($1), copts.input_fields.split(","), trim($1).val();
        if (!input_fields) {
            throw "CONSTRUCTOR-ERROR", "no valid field names provided in 'input_fields' option";
        }
        window_size = copts.window_size;
        if (window_size < 2) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("window_size must be >= 2; got %d", window_size);
        }
        if (exists copts.poll_interval_ms) {
            poll_interval_ms = copts.poll_interval_ms;
        }
        if (exists copts.timeout_ms) {
            timeout_ms = copts.timeout_ms;
        }
        buffer = ();
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "batch-projection";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Creates batch projections using a BigML PCA model; accumulates records in a "
            "window, uploads them to BigML as a temporary dataset, runs a batch projection, "
            "and emits each record enriched with its projection result";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records; processes batch when window is full
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "BigMLBatchProjectionProcessor: not configured; provide constructor options";
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
        if (buffer.lsize() >= 2) {
            list<auto> results = processWindow(buffer);
            buffer = ();
            return results;
        }
        # For single records, fall back to individual projection
        if (buffer.lsize() == 1) {
            hash<auto> record = buffer[0];
            buffer = ();
            hash<auto> input_data;
            foreach string f in (input_fields) {
                if (exists record{f}) {
                    input_data{f} = record{f};
                }
            }
            hash<auto> response = doPost("projection", {
                "pca": pca_resource,
                "input_data": input_data,
            });
            return (<BigMLProjectionEventInfo>{
                "event_type": "bigml-projection",
                "projection": response.projection ?? {},
                "resource": response.resource ?? "",
                "record_values": input_data,
            },);
        }
        buffer = ();
        return;
    }

    #! Processes a window of records as a batch projection
    private list<auto> processWindow(list<hash<auto>> records) {
        # Create CSV data from records
        string csv_data = input_fields.join(",") + "\n";
        foreach hash<auto> rec in (records) {
            list<string> row;
            foreach string f in (input_fields) {
                auto val = rec{f};
                if (exists val) {
                    string sval = string(val);
                    # Escape CSV fields containing commas or quotes
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

        # Step 1: Create a source from the CSV data
        hash<auto> source = doPost("source", {
            "data": csv_data,
            "name": sprintf("bigml-batch-projection-%s", now_us().format("YYYYMMDDHHmmSS")),
        });
        string source_resource = source.resource;
        source = pollUntilFinished(source_resource, poll_interval_ms, timeout_ms);

        # Step 2: Create a dataset from the source
        hash<auto> dataset = doPost("dataset", {
            "source": source_resource,
        });
        string dataset_resource = dataset.resource;
        dataset = pollUntilFinished(dataset_resource, poll_interval_ms, timeout_ms);

        # Step 3: Create batch projection
        hash<auto> batch = doPost("batchprojection", {
            "pca": pca_resource,
            "dataset": dataset_resource,
            "output_dataset": True,
            "all_fields": True,
        });
        string batch_resource = batch.resource;
        batch = pollUntilFinished(batch_resource, poll_interval_ms, timeout_ms);

        # Step 4: Get the output dataset with results
        string output_dataset = batch.output_dataset_resource;
        hash<auto> output = pollUntilFinished(output_dataset, poll_interval_ms, timeout_ms);

        # Step 5: Emit projection events for each record
        list<auto> events;
        for (int i = 0; i < records.lsize(); ++i) {
            hash<auto> input_data;
            foreach string f in (input_fields) {
                input_data{f} = records[i]{f};
            }
            push events, <BigMLProjectionEventInfo>{
                "event_type": "bigml-projection",
                "projection": {},
                "resource": batch_resource,
                "record_values": input_data,
            };
        }

        # Step 6: Clean up temporary resources (best effort)
        try { doDelete(batch_resource); } catch () {}
        try { doDelete(dataset_resource); } catch () {}
        try { doDelete(source_resource); } catch () {}
        if (output_dataset) {
            try { doDelete(output_dataset); } catch () {}
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
        return new HashDeclDataType(
            TypedHash::forName("BigMLDataProvider::BigMLProjectionEventInfo"));
    }
}
}
