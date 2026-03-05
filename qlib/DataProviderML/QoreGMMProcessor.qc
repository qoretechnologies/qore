# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreGMMProcessor class definition

/** QoreGMMProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that performs probabilistic clustering using Gaussian Mixture Models
/** Accumulates records in windows and uses the ML::GMM algorithm to
    assign each record to a cluster with probability scores.

    @since %DataProviderML 1.0
*/
public class QoreGMMProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreGMMProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "fields": <DataProviderOptionInfo>{
                "display_name": "Field Names",
                "short_desc": "Numeric fields to use for clustering",
                "desc": "A comma-separated list of numeric field names from the input records "
                    "to use for grouping; for example: \"revenue,frequency,recency\"; "
                    "at least 1 field is required",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "required": True,
                "input_field_names": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Window Size",
                "short_desc": "Number of records to cluster together",
                "desc": "The number of records to collect before assigning them to groups "
                    "with probability scores; must be at least 2.\n\n"
                    "**Sizing guidance**: GMM needs more data than k-means for stable "
                    "results; use `100`+ for `2`\u2013`3` groups, `500`+ for `5`+ groups; "
                    "use `full` covariance requires even more data",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "example_value": 200,
            },
            "n_components": <DataProviderOptionInfo>{
                "display_name": "Number of Groups",
                "short_desc": "How many groups to discover",
                "desc": "The number of groups (clusters) the algorithm should find in the "
                    "data; unlike k-means, each record gets a probability for each group "
                    "rather than a hard assignment; must be at least `1` and no more than "
                    "the number of records in the window.\n\n"
                    "**How to choose**: start with `2`\u2013`3` groups and increase if the "
                    "probabilities are spread too evenly (no clear group structure); "
                    "for example, `2` for binary classification, `3`\u2013`5` for multi-segment "
                    "analysis",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "example_value": 3,
            },
            "covariance_type": <DataProviderOptionInfo>{
                "display_name": "Covariance Type",
                "short_desc": "How cluster shapes are modeled",
                "desc": "Controls the shape of the clusters that the model can discover; "
                    "default: `full`",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "full",
                        "display_name": "Full (elliptical clusters)",
                        "desc": "Allows clusters to be elliptical in any orientation; "
                            "captures correlations between fields but requires more data "
                            "to estimate accurately",
                    },
                    <AllowedValueInfo>{
                        "value": "diagonal",
                        "display_name": "Diagonal (axis-aligned clusters)",
                        "desc": "Clusters are axis-aligned ellipses; faster and more stable "
                            "with limited data but cannot capture correlations between fields",
                    },
                ),
            },
        };
    }

    private {
        #! Configured field names
        list<string> fields;

        #! Configured window size
        int window_size;

        #! Number of components
        int n_components;

        #! Covariance type
        string covariance_type = "full";

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
        n_components = copts.n_components;
        if (n_components < 1) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("n_components must be >= 1; got %d", n_components);
        }
        if (window_size < n_components) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("window_size (%d) must be >= n_components (%d); GMM requires at least "
                    "as many data points as components", window_size, n_components);
        }
        if (exists copts.covariance_type) {
            covariance_type = copts.covariance_type;
        }
        buffer = ();
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "gmm";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Performs probabilistic clustering using Gaussian Mixture Models; "
            "accumulates records in windows, fits a GMM, and emits cluster "
            "assignments with probability scores for each record";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records; emits clustered records when window is full
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreGMMProcessor: not configured; provide constructor options";
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
    /** @note If the buffer contains fewer records than \c n_components, the
        records are discarded because GMM requires at least \c n_components
        data points to fit the model. No results are emitted in this case.
    */
    private *list<auto> flushRecordsImpl() {
        if (!configured || !buffer) {
            return;
        }
        if (buffer.lsize() >= n_components) {
            list<auto> results = processWindow(buffer);
            buffer = ();
            return results;
        }
        buffer = ();
        return;
    }

    #! Processes a window of records through GMM
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

        # create and fit GMM
        ML::GMM gmm({
            "n_components": n_components,
            "covariance_type": covariance_type,
        });
        gmm.fitMatrix(matrix);

        # predict cluster assignments
        list<hash<ML::GMMResult>> predictions = gmm.predictMatrix(matrix);

        # emit one event per record
        list<auto> events;
        for (int i = 0; i < records.lsize(); ++i) {
            hash<auto> record_values;
            foreach string f in (fields) {
                record_values{f} = DataProvider::AbstractDataProvider::getFieldValue(records[i], f);
            }
            push events, <GMMClusteringEventInfo>{
                "event_type": "gmm",
                "cluster": predictions[i].cluster,
                "max_probability": predictions[i].max_probability,
                "probabilities": predictions[i].probabilities,
                "log_likelihood": predictions[i].log_likelihood,
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
        return new HashDeclDataType(TypedHash::forName("DataProviderML::GMMClusteringEventInfo"));
    }
}
}
