# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreKMeansProcessor class definition

/** QoreKMeansProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that segments data into groups using k-means clustering
/** Accumulates records in windows and uses the ML::KMeans algorithm to
    assign each record to a cluster centroid. Supports online learning:
    the first window fits the model, subsequent windows update it incrementally.

    @since %DataProviderML 1.0
*/
public class QoreKMeansProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreKMeansProcessor",
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
                "desc": "The number of records to accumulate before performing k-means "
                    "clustering on the window",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
            },
            "k": <DataProviderOptionInfo>{
                "display_name": "Number of Clusters",
                "short_desc": "Number of clusters to create",
                "desc": "The number of clusters (k) to partition the data into; must be "
                    "at least 1 and at most the number of records in the window",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
            },
            "online": <DataProviderOptionInfo>{
                "display_name": "Online Learning",
                "short_desc": "Update model incrementally with new windows",
                "desc": "When True, the first window fits the model and subsequent windows "
                    "update it incrementally; when False, each window refits from scratch; "
                    "default: True",
                "type": AbstractDataProviderTypeMap."*bool",
            },
        };
    }

    private {
        #! Configured field names
        list<string> fields;

        #! Configured window size
        int window_size;

        #! Number of clusters
        int k;

        #! Online learning mode
        bool online = True;

        #! Retained KMeans instance for online learning
        *ML::KMeans kmeans;

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
        k = copts.k;
        if (k < 1) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("k must be >= 1; got %d", k);
        }
        if (exists copts.online) {
            online = copts.online;
        }
        buffer = ();
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "kmeans-clustering";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Segments data into k groups using k-means clustering; accumulates records "
            "in windows, clusters them, and emits one event per record with cluster "
            "assignment; supports online learning for incremental model updates";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records; emits clustered records when window is full
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreKMeansProcessor: not configured; provide constructor options";
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
        if (buffer.lsize() >= k) {
            list<auto> results = processWindow(buffer);
            buffer = ();
            return results;
        }
        buffer = ();
        return;
    }

    #! Processes a window of records through KMeans
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

        # fit or update the model
        if (!kmeans) {
            kmeans = new ML::KMeans({"k": k});
            kmeans.fitMatrix(matrix);
        } else if (online) {
            kmeans.updateMatrix(matrix);
        } else {
            kmeans = new ML::KMeans({"k": k});
            kmeans.fitMatrix(matrix);
        }

        # predict cluster assignments
        list<hash<ML::KMeansResult>> predictions = kmeans.predictMatrix(matrix);

        # emit one event per record
        list<auto> events;
        for (int i = 0; i < records.lsize(); ++i) {
            hash<auto> record_values;
            foreach string f in (fields) {
                record_values{f} = records[i]{f};
            }
            push events, <KMeansClusteringEventInfo>{
                "event_type": "kmeans-clustering",
                "cluster": predictions[i].cluster,
                "distance": predictions[i].distance,
                "centroid_distances": predictions[i].centroid_distances,
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
        return new HashDeclDataType(TypedHash::forName("DataProviderML::KMeansClusteringEventInfo"));
    }
}
}
