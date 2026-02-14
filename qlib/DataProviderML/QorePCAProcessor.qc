# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QorePCAProcessor class definition

/** QorePCAProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that reduces data dimensionality using Principal Component Analysis
/** Accumulates records in windows and uses the ML::PCA algorithm to
    transform each record into principal components when the window is full
    or on flush.

    @since %DataProviderML 1.0
*/
public class QorePCAProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QorePCAProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "fields": <DataProviderOptionInfo>{
                "display_name": "Field Names",
                "short_desc": "Numeric fields to reduce",
                "desc": "A comma-separated list of numeric field names from the input records "
                    "to compress into fewer dimensions; for example, 50 sensor readings can be "
                    "reduced to 3 combined values that capture the most important patterns; "
                    "at least 2 fields are required",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Window Size",
                "short_desc": "Number of records to analyze together",
                "desc": "The number of records to collect before analyzing patterns across all "
                    "the fields and producing reduced outputs; must be at least 2",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
            },
            "n_components": <DataProviderOptionInfo>{
                "display_name": "Number of Output Dimensions",
                "short_desc": "How many combined dimensions to keep",
                "desc": "The exact number of output dimensions to keep; if not set, the "
                    "algorithm automatically determines the number based on the variance "
                    "threshold; for example, setting this to 3 produces 3 values per record "
                    "regardless of how many input fields there are",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "variance_threshold": <DataProviderOptionInfo>{
                "display_name": "Information Preservation",
                "short_desc": "How much of the original information to preserve",
                "desc": "A value between 0.0 and 1.0 specifying what fraction of the total "
                    "information (variance) must be preserved; the algorithm keeps just enough "
                    "output dimensions to meet this threshold; 0.95 means keep 95%% of the "
                    "information; only used when the number of output dimensions is not set "
                    "explicitly; default: 0.95",
                "type": AbstractDataProviderTypeMap."*float",
            },
        };
    }

    private {
        #! Configured field names
        list<string> fields;

        #! Configured window size
        int window_size;

        #! Number of components (NOTHING = use variance threshold)
        *int n_components;

        #! Variance threshold
        float variance_threshold = 0.95;

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
        fields = map trim($1), copts.fields.split(","), trim($1).val();
        if (fields.lsize() < 2) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("at least 2 fields are required for PCA; got %d", fields.lsize());
        }
        window_size = copts.window_size;
        if (window_size < 2) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("window_size must be >= 2; got %d", window_size);
        }
        if (exists copts.n_components) {
            n_components = copts.n_components;
        }
        if (exists copts.variance_threshold) {
            variance_threshold = copts.variance_threshold;
        }
        buffer = ();
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "pca";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Reduces data dimensionality using Principal Component Analysis (PCA); "
            "accumulates records in windows, fits PCA, and transforms each record "
            "into principal components";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records; emits transformed records when window is full
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QorePCAProcessor: not configured; provide constructor options";
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
        buffer = ();
        return;
    }

    #! Processes a window of records through PCA
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

        # create PCA with appropriate options
        hash<auto> pca_opts;
        if (exists n_components) {
            pca_opts.n_components = n_components;
        } else {
            pca_opts.variance_threshold = variance_threshold;
        }
        ML::PCA pca(pca_opts);

        # fit and transform
        list<hash<ML::PCAResult>> results = pca.fitTransformMatrix(matrix);

        # emit one event per record
        list<auto> events;
        for (int i = 0; i < records.lsize(); ++i) {
            hash<auto> record_values;
            foreach string f in (fields) {
                record_values{f} = records[i]{f};
            }
            push events, <PCAEventInfo>{
                "event_type": "pca",
                "components": results[i].components,
                "total_explained_variance": results[i].total_explained_variance,
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
        return new HashDeclDataType(TypedHash::forName("DataProviderML::PCAEventInfo"));
    }
}
}
