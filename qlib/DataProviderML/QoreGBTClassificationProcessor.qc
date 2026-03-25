# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreGBTClassificationProcessor class definition

/** QoreGBTClassificationProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that classifies records using Gradient Boosted Trees
/** Accumulates records in windows and uses the ML::GradientBoostedTrees algorithm in
    classification mode to predict class labels by sequentially building trees that
    correct each other's mistakes.

*/
public class QoreGBTClassificationProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreGBTClassificationProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "fields": <DataProviderOptionInfo>{
                "display_name": "Input Fields",
                "short_desc": "Which numeric fields to use for classification",
                "desc": "The numeric fields the model uses to classify records. For example, "
                    "to detect fraudulent transactions, select `amount`, `hour`, and "
                    "`merchant_category` — the model builds a sequence of trees, each one "
                    "correcting the mistakes of the previous ones.\n\n"
                    "At least 1 field is required",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "required": True,
                "input_field_names": True,
            },
            "target_field": <DataProviderOptionInfo>{
                "display_name": "Category Field",
                "short_desc": "Which numeric field contains the category code",
                "desc": "The field that contains a **numeric** category code for each record. "
                    "For example, `is_fraud` with values `0` (legitimate) and `1` (fraud) or "
                    "`severity` with values `0`, `1`, `2`.\n\n"
                    "**Note**: categories must be numeric (integers); use a numeric code "
                    "for text categories (e.g., `0` = normal, `1` = suspicious, `2` = fraud)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "input_field_names": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Batch Size",
                "short_desc": "How many example records to learn from",
                "desc": "How many labeled records to collect before the model learns and "
                    "starts predicting. Gradient boosting benefits from more data to "
                    "avoid overfitting.\n\n"
                    "**Tip**: use at least `100`\u2013`500` for reliable results",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "example_value": 200,
            },
            "n_estimators": <DataProviderOptionInfo>{
                "display_name": "Number of Boosting Rounds",
                "short_desc": "How many sequential trees to build",
                "desc": "The number of trees to build sequentially. Each new tree focuses on "
                    "correcting the errors of the previous trees. More rounds can improve "
                    "accuracy but increase computation time and risk overfitting.\n\n"
                    "**Tip**: `50`\u2013`200` rounds work well for most tasks\n\n"
                    "Default: `100`",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "learning_rate": <DataProviderOptionInfo>{
                "display_name": "Step Size",
                "short_desc": "How aggressively each tree corrects the previous ones",
                "desc": "Controls how much each new tree corrects the errors of the previous "
                    "trees. Think of it as a \"caution dial\" — lower values are more cautious "
                    "(slower but more accurate), higher values are more aggressive (faster but "
                    "may overshoot)",
                "type": AbstractDataProviderTypeMap."*float",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": 0.01,
                        "display_name": "Very Cautious (0.01)",
                        "desc": "Each tree makes a tiny correction. Needs many trees (500+) "
                            "but often gives the best accuracy. Best when you have plenty of data",
                    },
                    <AllowedValueInfo>{
                        "value": 0.05,
                        "display_name": "Cautious (0.05)",
                        "desc": "Small corrections per tree. Good balance for most datasets "
                            "with 200+ trees",
                    },
                    <AllowedValueInfo>{
                        "value": 0.1,
                        "display_name": "Balanced (0.1, default)",
                        "desc": "The standard starting point. Works well with 100 trees for "
                            "most problems",
                    },
                    <AllowedValueInfo>{
                        "value": 0.2,
                        "display_name": "Aggressive (0.2)",
                        "desc": "Larger corrections per tree. Trains faster but may overfit "
                            "on small datasets. Try with 50\u2013100 trees",
                    },
                    <AllowedValueInfo>{
                        "value": 0.3,
                        "display_name": "Very Aggressive (0.3)",
                        "desc": "Each tree makes a big correction. Risk of overfitting — use "
                            "only with large datasets or few trees",
                    },
                ),
            },
            "max_depth": <DataProviderOptionInfo>{
                "display_name": "Maximum Tree Depth",
                "short_desc": "How many levels of decision rules per tree",
                "desc": "The maximum number of levels in each tree. Gradient boosting typically "
                    "uses shallow trees (stumps or small trees) that each capture one aspect "
                    "of the pattern.\n\n"
                    "**Tip**: `2`\u2013`6` works best for gradient boosting; deeper trees risk "
                    "overfitting\n\n"
                    "Default: `3`",
                "type": AbstractDataProviderTypeMap."*int",
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

        #! Number of estimators (boosting rounds)
        *int n_estimators;

        #! Learning rate
        *float learning_rate;

        #! Maximum tree depth
        *int max_depth;

        #! Retained GradientBoostedTrees model instance
        *ML::GradientBoostedTrees model;

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
        n_estimators = copts.n_estimators;
        learning_rate = copts.learning_rate;
        max_depth = copts.max_depth;
        buffer = ();
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "gbt-classification";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Classifies records using gradient boosted trees that sequentially correct "
            "each other's mistakes; accumulates records in windows, fits a model, and "
            "emits predicted category labels with confidence scores";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records; emits predictions when window is full
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreGBTClassificationProcessor: not configured; provide constructor options";
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

    #! Processes a window of records through GBT classification
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
        if (exists n_estimators) {
            model_opts.n_estimators = n_estimators;
        }
        if (exists learning_rate) {
            model_opts.learning_rate = learning_rate;
        }
        if (exists max_depth) {
            model_opts.max_depth = max_depth;
        }

        # fit the model
        if (!model) {
            model = new ML::GradientBoostedTrees(model_opts);
            model.fitMatrix(matrix, targets);
        } else {
            # predict with existing model
        }

        # predict all records
        list<hash<ML::GBTClassificationResult>> predictions = model.predictClassificationMatrix(matrix);

        # emit one event per record
        list<auto> events;
        for (int i = 0; i < records.lsize(); ++i) {
            hash<auto> record_values;
            foreach string f in (fields) {
                record_values{f} = DataProvider::AbstractDataProvider::getFieldValue(records[i], f);
            }
            record_values{target_field} = DataProvider::AbstractDataProvider::getFieldValue(
                records[i], target_field);
            push events, <GBTClassificationEventInfo>{
                "event_type": "gbt-classification",
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
        return new HashDeclDataType(TypedHash::forName("DataProviderML::GBTClassificationEventInfo"));
    }
}
}
