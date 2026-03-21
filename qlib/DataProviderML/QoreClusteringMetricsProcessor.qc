# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreClusteringMetricsProcessor class definition

/** QoreClusteringMetricsProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that measures how well data was grouped into clusters
/** Accumulates records containing cluster assignments and data fields, then
    evaluates clustering quality using silhouette score, Davies-Bouldin index,
    and Calinski-Harabasz index via the corresponding ML module functions.

    On flush (or when the window is full), emits a single
    ClusteringMetricsEventInfo event summarizing the batch.

*/
public class QoreClusteringMetricsProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreClusteringMetricsProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "fields": <DataProviderOptionInfo>{
                "display_name": "Data Fields Used for Grouping",
                "short_desc": "Which numeric fields were used to create the groups",
                "desc": "The numeric fields that were used to group the data. For example, if "
                    "customers were grouped by `age`, `income`, and `spending_score`, select "
                    "those same fields here so the quality score can check whether the groups "
                    "make sense.\n\n"
                    "At least 1 field is required",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "required": True,
                "input_field_names": True,
            },
            "cluster_field": <DataProviderOptionInfo>{
                "display_name": "Group Assignment Field",
                "short_desc": "Which field says which group each record belongs to",
                "desc": "The field in each record that says which group it was assigned to "
                    "(e.g., `cluster`, `segment`, `group_id`). This is typically a number "
                    "like `0`, `1`, `2` assigned by a grouping step earlier in the pipeline",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Batch Size",
                "short_desc": "How many records to collect before scoring",
                "desc": "How many records to collect before computing a quality score. All "
                    "records in the batch are evaluated together. The batch must contain "
                    "at least 2 distinct groups.\n\n"
                    "**Tip**: use at least `50` for meaningful scores",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "example_value": 100,
            },
        };
    }

    private {
        #! Configured data field names
        list<string> fields;

        #! Configured cluster field name
        string cluster_field;

        #! Configured window size
        int window_size;

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
        cluster_field = copts.cluster_field;
        window_size = copts.window_size;
        if (window_size < 2) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("window_size must be >= 2; got %d", window_size);
        }
        buffer = ();
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "clustering-metrics";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Evaluates whether the clusters are well-separated and cohesive; "
            "accumulates records in batches and emits silhouette score, Davies-Bouldin "
            "index, and Calinski-Harabasz index";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records; emits metrics when window is full
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreClusteringMetricsProcessor: not configured; provide constructor options";
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

    #! Processes a window of records through clustering metrics
    private list<auto> processWindow(list<hash<auto>> records) {
        # extract data matrix and cluster labels
        list<list<float>> matrix;
        list<float> labels;
        hash<string, bool> unique_clusters;
        foreach hash<auto> rec in (records) {
            list<float> row;
            foreach string f in (fields) {
                push row, DataProvider::AbstractDataProvider::getFieldValue(rec, f).toFloat();
            }
            push matrix, row;
            float label = DataProvider::AbstractDataProvider::getFieldValue(rec, cluster_field).toFloat();
            push labels, label;
            unique_clusters{string(label)} = True;
        }

        int n_clusters = unique_clusters.size();

        float silhouette = ML::ml_silhouette_score(matrix, labels);
        float davies_bouldin = ML::ml_davies_bouldin_score(matrix, labels);
        float calinski_harabasz = ML::ml_calinski_harabasz_score(matrix, labels);

        return (<ClusteringMetricsEventInfo>{
            "event_type": "clustering-metrics",
            "silhouette_score": silhouette,
            "davies_bouldin_score": davies_bouldin,
            "calinski_harabasz_score": calinski_harabasz,
            "sample_count": records.lsize(),
            "n_clusters": n_clusters,
        },);
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
            TypedHash::forName("DataProviderML::ClusteringMetricsEventInfo"));
    }
}
}
