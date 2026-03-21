# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreDBSCANClusteringProcessor class definition

/** QoreDBSCANClusteringProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that discovers data clusters using DBSCAN
/** Accumulates records in windows and uses the ML::DBSCAN algorithm to
    cluster records when the window is full or on flush.

*/
public class QoreDBSCANClusteringProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreDBSCANClusteringProcessor",
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
                    "to use for finding groups; for example: \"latitude,longitude\" or "
                    "\"revenue,customer_count\"; at least 1 field is required",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "required": True,
                "input_field_names": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Window Size",
                "short_desc": "Number of records to cluster together",
                "desc": "The number of records to collect before grouping them into clusters; "
                    "all records in a window are analyzed together; must be at least 2.\n\n"
                    "**Sizing guidance**: use `50`\u2013`200` for exploratory clustering, `500`+ "
                    "for stable cluster discovery; DBSCAN needs enough points to form "
                    "dense regions",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "example_value": 100,
            },
            "epsilon": <DataProviderOptionInfo>{
                "display_name": "Neighborhood Radius",
                "short_desc": "How close points must be to form a cluster",
                "desc": "The maximum distance between two data points for them to be "
                    "considered neighbors; smaller values create tighter, more specific "
                    "clusters; larger values merge nearby groups into larger clusters.\n\n"
                    "**How to choose**: the right value depends on the scale of your data. "
                    "A practical approach is to look at the typical range of your values: "
                    "if values range from `0`\u2013`100`, try `5`\u2013`15`; if values range from "
                    "`0`\u2013`1.0`, try `0.1`\u2013`0.3`. Start with a larger value (fewer clusters) "
                    "and decrease it to find more specific groups",
                "type": AbstractDataProviderTypeMap."float",
                "required": True,
                "example_value": 5.0,
            },
            "min_points": <DataProviderOptionInfo>{
                "display_name": "Minimum Cluster Size",
                "short_desc": "Minimum neighbors to form a cluster",
                "desc": "The minimum number of nearby points required to start a cluster; "
                    "points with fewer neighbors than this are labeled as noise (not "
                    "belonging to any cluster); higher values require denser groups; "
                    "default: `5`",
                "type": AbstractDataProviderTypeMap."*int",
                "example_value": 5,
            },
            "metric": <DataProviderOptionInfo>{
                "display_name": "Distance Metric",
                "short_desc": "How to measure distance between data points",
                "desc": "The method used to measure how far apart two data points are; "
                    "this affects how clusters are shaped and which points are considered "
                    "neighbors; default: `euclidean`",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "euclidean",
                        "display_name": "Euclidean (straight-line distance)",
                        "desc": "Straight-line distance between points; best for data where "
                            "all dimensions have similar scales",
                    },
                    <AllowedValueInfo>{
                        "value": "manhattan",
                        "display_name": "Manhattan (city-block distance)",
                        "desc": "Sum of absolute differences along each dimension; less "
                            "sensitive to large differences in a single dimension",
                    },
                    <AllowedValueInfo>{
                        "value": "cosine",
                        "display_name": "Cosine (angular distance)",
                        "desc": "Measures the angle between data points rather than magnitude; "
                            "best when the direction matters more than the size of values",
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

        #! Epsilon
        float epsilon;

        #! Minimum points
        int min_points = 5;

        #! Distance metric
        string metric = "euclidean";

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
        epsilon = copts.epsilon;
        if (exists copts.min_points) {
            min_points = copts.min_points;
        }
        if (exists copts.metric) {
            metric = copts.metric;
        }
        buffer = ();
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "dbscan-clustering";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Discovers natural data clusters using the DBSCAN density-based clustering "
            "algorithm; accumulates records in windows, clusters them, and emits one "
            "event per record with cluster assignment";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records; emits clustered records when window is full
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreDBSCANClusteringProcessor: not configured; provide constructor options";
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

    #! Processes a window of records through DBSCAN
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

        # create DBSCAN and cluster
        ML::DBSCAN dbscan({
            "epsilon": epsilon,
            "min_points": min_points,
            "metric": metric,
        });
        list<hash<ML::DBSCANResult>> clusters = dbscan.clusterMatrix(matrix);

        # emit one event per record
        list<auto> events;
        for (int i = 0; i < records.lsize(); ++i) {
            hash<auto> record_values;
            foreach string f in (fields) {
                record_values{f} = DataProvider::AbstractDataProvider::getFieldValue(records[i], f);
            }
            push events, <DBSCANClusteringEventInfo>{
                "event_type": "dbscan-clustering",
                "cluster": clusters[i].cluster,
                "is_core": clusters[i].is_core,
                "neighbor_count": clusters[i].neighbor_count,
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
        return new HashDeclDataType(TypedHash::forName("DataProviderML::DBSCANClusteringEventInfo"));
    }
}
}
