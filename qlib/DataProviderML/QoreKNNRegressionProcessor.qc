# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreKNNRegressionProcessor class definition

/** QoreKNNRegressionProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that predicts numeric values using K-Nearest Neighbors regression
/** Accumulates records in windows and uses the ML::KNN algorithm in regression
    mode to predict numeric target values for each record based on its nearest neighbors.

*/
public class QoreKNNRegressionProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreKNNRegressionProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "fields": <DataProviderOptionInfo>{
                "display_name": "Comparison Fields",
                "short_desc": "Which numeric fields to use when finding similar records",
                "desc": "The numeric fields used to determine which records are most similar. "
                    "For example, to predict a house price, select `sqft`, `bedrooms`, and "
                    "`bathrooms` — the model finds the most similar past houses and averages "
                    "their prices.\n\n"
                    "At least 1 field is required",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "required": True,
                "input_field_names": True,
            },
            "target_field": <DataProviderOptionInfo>{
                "display_name": "Value to Predict",
                "short_desc": "Which numeric field to estimate",
                "desc": "The numeric field the model will predict for new records. For "
                    "example, `price`, `delivery_days`, or `temperature`. The model "
                    "averages this field from the most similar past records",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "input_field_names": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Batch Size",
                "short_desc": "How many example records to learn from",
                "desc": "How many records to collect as the reference set. New records are "
                    "compared against these examples to find the most similar ones. Must be "
                    "larger than the number of neighbors.\n\n"
                    "**Tip**: use at least `50`\u2013`200` for reliable results",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "example_value": 100,
            },
            "k": <DataProviderOptionInfo>{
                "display_name": "Number of Neighbors",
                "short_desc": "How many similar records to average",
                "desc": "How many of the most similar records to average when making a "
                    "prediction. For example, `5` means \"find the 5 most similar past "
                    "houses and average their prices.\"\n\n"
                    "**Tip**: start with `5`\u2013`10`; smaller values react more to "
                    "individual records, larger values give smoother predictions",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "example_value": 5,
            },
            "metric": <DataProviderOptionInfo>{
                "display_name": "Similarity Method",
                "short_desc": "How to measure how similar two records are",
                "desc": "How to calculate the similarity between records",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "euclidean",
                        "display_name": "Straight-Line Distance (default)",
                        "desc": "Measures the direct distance between two records' values. "
                            "Works well when all fields are on a similar scale",
                    },
                    <AllowedValueInfo>{
                        "value": "manhattan",
                        "display_name": "Block Distance",
                        "desc": "Measures distance by adding up the differences in each "
                            "field separately. Less affected by a single field with a very "
                            "large difference",
                    },
                    <AllowedValueInfo>{
                        "value": "cosine",
                        "display_name": "Direction Similarity",
                        "desc": "Measures whether records point in the same direction rather "
                            "than whether they have the same magnitude. Best for comparing "
                            "proportions or patterns",
                    },
                ),
            },
            "weight_func": <DataProviderOptionInfo>{
                "display_name": "Neighbor Influence",
                "short_desc": "Should closer neighbors count more?",
                "desc": "Whether all similar records count equally, or closer ones have "
                    "more say in the prediction",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "uniform",
                        "display_name": "Equal Influence (default)",
                        "desc": "All neighbors contribute equally to the average",
                    },
                    <AllowedValueInfo>{
                        "value": "distance",
                        "display_name": "Closer = More Influence",
                        "desc": "Records that are more similar to the new record have a "
                            "bigger effect on the predicted value",
                    },
                ),
            },
        };
    }

    private {
        #! Configured feature field names
        list<string> fields;

        #! Target field name
        string target_field;

        #! Configured window size
        int window_size;

        #! Number of neighbors
        int k;

        #! Distance metric
        *string metric;

        #! Weight function
        *string weight_func;

        #! Retained KNN model instance
        *ML::KNN model;

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
        target_field = copts.target_field;
        if (!target_field.val()) {
            throw "CONSTRUCTOR-ERROR", "target_field option is required";
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
        if (window_size < k) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("window_size (%d) must be >= k (%d); KNN requires at least "
                    "as many data points as neighbors", window_size, k);
        }
        metric = copts.metric;
        weight_func = copts.weight_func;
        buffer = ();
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "knn-regression";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Predicts numeric values by averaging the values of the most similar "
            "records in the training data; accumulates records in windows, fits a model, "
            "and emits predicted values for each record";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records; emits predictions when window is full
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreKNNRegressionProcessor: not configured; provide constructor options";
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

    #! Processes a window of records through KNN regression
    private list<auto> processWindow(list<hash<auto>> records) {
        # extract feature matrix and target vector
        list<list<float>> matrix;
        list<float> targets;
        foreach hash<auto> rec in (records) {
            list<float> row;
            foreach string f in (fields) {
                push row, DataProvider::AbstractDataProvider::getFieldValue(rec, f).toFloat();
            }
            push matrix, row;
            push targets, DataProvider::AbstractDataProvider::getFieldValue(rec, target_field).toFloat();
        }

        # create options hash for the model
        hash<auto> model_opts = {
            "k": k,
            "task": "regression",
        };
        if (exists metric) {
            model_opts.metric = metric;
        }
        if (exists weight_func) {
            model_opts.weight_func = weight_func;
        }

        # fit the model on the first window or if no model exists
        if (!model) {
            model = new ML::KNN(model_opts);
            model.fitMatrix(matrix, targets);
        } else {
            # predict with existing model
        }

        # predict all records
        list<hash<ML::KNNRegressionResult>> predictions = model.predictMatrix(matrix);

        # emit one event per record
        list<auto> events;
        for (int i = 0; i < records.lsize(); ++i) {
            hash<auto> record_values;
            foreach string f in (fields) {
                record_values{f} = DataProvider::AbstractDataProvider::getFieldValue(records[i], f);
            }
            record_values{target_field} = DataProvider::AbstractDataProvider::getFieldValue(
                records[i], target_field);
            push events, <KNNRegressionEventInfo>{
                "event_type": "knn-regression",
                "prediction": predictions[i].prediction,
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
        return new HashDeclDataType(TypedHash::forName("DataProviderML::KNNRegressionEventInfo"));
    }
}
}
