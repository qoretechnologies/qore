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

    @since %DataProviderML 1.0
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
                "short_desc": "Comma-separated numeric field names to cluster",
                "desc": "A comma-separated list of numeric field names to use as features "
                    "for clustering; at least 1 field is required",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Window Size",
                "short_desc": "Number of records per clustering window",
                "desc": "The number of records to accumulate before performing DBSCAN "
                    "clustering on the window",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
            },
            "epsilon": <DataProviderOptionInfo>{
                "display_name": "Epsilon (Neighborhood Radius)",
                "short_desc": "Maximum distance between neighboring points",
                "desc": "The maximum distance between two points for them to be considered "
                    "neighbors; smaller values create tighter, more numerous clusters",
                "type": AbstractDataProviderTypeMap."float",
                "required": True,
            },
            "min_points": <DataProviderOptionInfo>{
                "display_name": "Minimum Points",
                "short_desc": "Minimum neighbors to form a dense region",
                "desc": "The minimum number of points within epsilon distance to form a "
                    "dense region (core point); default: 5",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "metric": <DataProviderOptionInfo>{
                "display_name": "Distance Metric",
                "short_desc": "Distance metric for computing point distances",
                "desc": "The distance metric to use: 'euclidean' (straight-line distance), "
                    "'manhattan' (city-block distance), or 'cosine' (angular distance); "
                    "default: 'euclidean'",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "euclidean",
                        "desc": "Straight-line (L2) distance",
                    },
                    <AllowedValueInfo>{
                        "value": "manhattan",
                        "desc": "City-block (L1) distance",
                    },
                    <AllowedValueInfo>{
                        "value": "cosine",
                        "desc": "Angular (cosine) distance",
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
        fields = map trim($1), copts.fields.split(","), trim($1).val();
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
                push row, rec{f}.toFloat();
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
                record_values{f} = records[i]{f};
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
