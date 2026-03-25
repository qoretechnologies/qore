# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreRandomForestClassificationProcessor class definition

/** QoreRandomForestClassificationProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that classifies records using a Random Forest
/** Accumulates records in windows and uses the ML::RandomForest algorithm in classification
    mode to predict class labels by combining the votes of multiple decision trees.

*/
public class QoreRandomForestClassificationProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreRandomForestClassificationProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "fields": <DataProviderOptionInfo>{
                "display_name": "Input Fields",
                "short_desc": "Which numeric fields to use for classification",
                "desc": "The numeric fields the forest uses to classify records. For example, "
                    "to classify email as spam or not spam, select `word_count`, "
                    "`link_count`, and `caps_ratio` — each tree in the forest learns "
                    "different rules and they vote on the final category.\n\n"
                    "At least 1 field is required",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "required": True,
                "input_field_names": True,
            },
            "target_field": <DataProviderOptionInfo>{
                "display_name": "Category Field",
                "short_desc": "Which numeric field contains the category code",
                "desc": "The field that contains a **numeric** category code for each record. "
                    "For example, `spam` with values `0` (not spam) and `1` (spam) or "
                    "`quality` with values `0`, `1`, `2`.\n\n"
                    "**Note**: categories must be numeric (integers); use a numeric code "
                    "for text categories (e.g., `0` = low, `1` = medium, `2` = high)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "input_field_names": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Batch Size",
                "short_desc": "How many example records to learn from",
                "desc": "How many labeled records to collect before the forest learns and "
                    "starts predicting. Each tree in the forest trains on a random subset "
                    "of these records.\n\n"
                    "**Tip**: use at least `100`\u2013`500` for reliable results",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "example_value": 200,
            },
            "n_trees": <DataProviderOptionInfo>{
                "display_name": "Number of Trees",
                "short_desc": "How many decision trees to grow in the forest",
                "desc": "The number of decision trees to build. More trees generally improve "
                    "accuracy but increase computation time. Each tree sees a random subset "
                    "of the data and features, then they vote on the final prediction.\n\n"
                    "**Tip**: `50`\u2013`200` trees work well for most tasks\n\n"
                    "Default: `100`",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "max_depth": <DataProviderOptionInfo>{
                "display_name": "Maximum Tree Depth",
                "short_desc": "How many levels of decision rules per tree",
                "desc": "The maximum number of levels in each decision tree. Deeper trees can "
                    "capture more complex patterns but may overfit.\n\n"
                    "**Tip**: start with `5`\u2013`15`\n\n"
                    "Default: `10`",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "max_features": <DataProviderOptionInfo>{
                "display_name": "Features per Tree",
                "short_desc": "How many fields each tree considers at each split",
                "desc": "How many input fields each tree considers when deciding how to split "
                    "at each node. Using fewer features per tree increases diversity among "
                    "trees and usually improves accuracy",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "sqrt",
                        "display_name": "Square Root of Fields (default)",
                        "desc": "Each tree considers the square root of the total number of "
                            "fields at each split. The standard choice for classification",
                    },
                    <AllowedValueInfo>{
                        "value": "log2",
                        "display_name": "Log2 of Fields",
                        "desc": "Each tree considers log2 of the total number of fields at "
                            "each split. Uses fewer features, increasing tree diversity",
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

        #! Number of trees
        *int n_trees;

        #! Maximum tree depth
        *int max_depth;

        #! Max features strategy
        *string max_features;

        #! Retained RandomForest model instance
        *ML::RandomForest model;

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
        n_trees = copts.n_trees;
        max_depth = copts.max_depth;
        max_features = copts.max_features;
        buffer = ();
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "random-forest-classification";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Classifies records using an ensemble of decision trees that vote on the "
            "final category; accumulates records in windows, fits a random forest model, "
            "and emits predicted category labels with confidence scores";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records; emits predictions when window is full
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreRandomForestClassificationProcessor: not configured; provide constructor options";
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

    #! Processes a window of records through Random Forest classification
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
            "task": "classification",
        };
        if (exists n_trees) {
            model_opts.n_trees = n_trees;
        }
        if (exists max_depth) {
            model_opts.max_depth = max_depth;
        }
        if (exists max_features) {
            model_opts.max_features = max_features;
        }

        # fit the model
        if (!model) {
            model = new ML::RandomForest(model_opts);
            model.fitMatrix(matrix, targets);
        } else {
            # predict with existing model
        }

        # predict all records
        list<hash<ML::RandomForestClassificationResult>> predictions = model.predictClassificationMatrix(matrix);

        # emit one event per record
        list<auto> events;
        for (int i = 0; i < records.lsize(); ++i) {
            hash<auto> record_values;
            foreach string f in (fields) {
                record_values{f} = DataProvider::AbstractDataProvider::getFieldValue(records[i], f);
            }
            record_values{target_field} = DataProvider::AbstractDataProvider::getFieldValue(
                records[i], target_field);
            push events, <RandomForestClassificationEventInfo>{
                "event_type": "random-forest-classification",
                "predicted_class": predictions[i].predicted_class,
                "confidence": predictions[i].max_probability,
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
        return new HashDeclDataType(TypedHash::forName("DataProviderML::RandomForestClassificationEventInfo"));
    }
}
}
