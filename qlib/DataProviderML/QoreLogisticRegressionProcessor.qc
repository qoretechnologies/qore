# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreLogisticRegressionProcessor class definition

/** QoreLogisticRegressionProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that performs logistic regression classification on data windows
/** Accumulates records in windows and uses the ML::LogisticRegression algorithm to
    fit a model and predict class labels for each record.

*/
public class QoreLogisticRegressionProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreLogisticRegressionProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "fields": <DataProviderOptionInfo>{
                "display_name": "Fields to Analyze",
                "short_desc": "Which numeric fields help decide the category",
                "desc": "The numeric fields the model uses to decide which category each "
                    "record belongs to. For example, if categorizing loan applications as "
                    "approved/denied, select fields like `income`, `credit_score`, and "
                    "`debt_ratio`.\n\n"
                    "At least 1 field is required",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "required": True,
                "input_field_names": True,
            },
            "target_field": <DataProviderOptionInfo>{
                "display_name": "Category Field",
                "short_desc": "Which field contains the correct category",
                "desc": "The field that says what category each record actually belongs to. "
                    "For example, `status` (with values like `\"approved\"` or `\"denied\"`), "
                    "or `risk_level` (with values like `1`, `2`, `3`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "input_field_names": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Batch Size",
                "short_desc": "How many records to learn from before predicting",
                "desc": "How many labeled records to collect before the model learns and "
                    "starts predicting. The model studies these examples to understand the "
                    "pattern between the input fields and the category.\n\n"
                    "**Tip**: use at least 20\u201330 records per input field — for example, "
                    "with 4 fields, use at least `80`\u2013`120` records",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "example_value": 100,
            },
            "learning_rate": <DataProviderOptionInfo>{
                "display_name": "Learning Speed",
                "short_desc": "How quickly the model adjusts to the data",
                "desc": "How big of a step the model takes when learning from each example. "
                    "Smaller values are safer but slower; larger values are faster but may "
                    "miss the best answer. Most users should leave this at the default.\n\n"
                    "Default: `0.01`",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "max_iterations": <DataProviderOptionInfo>{
                "display_name": "Maximum Learning Passes",
                "short_desc": "How many times to review the data",
                "desc": "The maximum number of times the model reviews the data to improve "
                    "its understanding. More passes can improve accuracy but take longer. "
                    "The model stops early if it has already found a good enough answer.\n\n"
                    "Default: `1000`",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "regularization": <DataProviderOptionInfo>{
                "display_name": "Overfitting Prevention",
                "short_desc": "How strongly to prevent memorizing instead of learning",
                "desc": "Controls how much the model is discouraged from memorizing the "
                    "training data instead of learning general patterns. Higher values "
                    "produce simpler, more general models. Lower values allow the model "
                    "to fit the training data more closely.\n\n"
                    "Default: `0.01`",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "penalty": <DataProviderOptionInfo>{
                "display_name": "Overfitting Prevention Method",
                "short_desc": "Whether to apply overfitting prevention",
                "desc": "Whether and how to prevent the model from memorizing the training "
                    "data. Enabled by default and recommended for most use cases",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "l2",
                        "display_name": "Enabled (recommended)",
                        "desc": "Prevents the model from relying too heavily on any single "
                            "field; recommended for most use cases",
                    },
                    <AllowedValueInfo>{
                        "value": "none",
                        "display_name": "Disabled",
                        "desc": "No overfitting prevention; use only when you have a very "
                            "large dataset relative to the number of fields",
                    },
                ),
            },
            "online": <DataProviderOptionInfo>{
                "display_name": "Incremental Learning",
                "short_desc": "Continuously improve categorization with new data",
                "desc": "When enabled, the first batch of records creates the initial model "
                    "and each subsequent batch refines it, allowing the categorization to "
                    "improve as more labeled examples flow through the pipeline. For example, "
                    "a spam filter can learn from newly labeled emails over time.\n\n"
                    "When disabled, each batch trains a completely new model from scratch.\n\n"
                    "Default: `True` (enabled)",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "fit_intercept": <DataProviderOptionInfo>{
                "display_name": "Include Baseline",
                "short_desc": "Include a baseline value in the model",
                "desc": "Whether to include a baseline constant. This should almost always "
                    "be enabled — it allows the model to account for the overall tendency "
                    "in the data (e.g., if most applications are approved, the baseline "
                    "captures that).\n\n"
                    "Default: enabled",
                "type": AbstractDataProviderTypeMap."*bool",
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

        #! Learning rate
        *float learning_rate;

        #! Max iterations
        *int max_iterations;

        #! Regularization strength
        *float regularization;

        #! Penalty type
        *string penalty;

        #! Fit intercept option
        *bool fit_intercept;

        #! Online learning mode
        bool online = True;

        #! Retained model instance
        *ML::LogisticRegression model;

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
        if (exists copts.online) {
            online = copts.online;
        }
        learning_rate = copts.learning_rate;
        max_iterations = copts.max_iterations;
        regularization = copts.regularization;
        penalty = copts.penalty;
        fit_intercept = copts.fit_intercept;
        buffer = ();
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "logistic-regression";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Performs logistic regression classification on data windows; accumulates "
            "records, fits a model, and emits predicted category labels with probability "
            "scores for each record";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records; emits predictions when window is full
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreLogisticRegressionProcessor: not configured; provide constructor options";
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

    #! Processes a window of records through LogisticRegression
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
        hash<auto> model_opts;
        if (exists learning_rate) {
            model_opts.learning_rate = learning_rate;
        }
        if (exists max_iterations) {
            model_opts.max_iterations = max_iterations;
        }
        if (exists regularization) {
            model_opts.regularization = regularization;
        }
        if (exists penalty) {
            model_opts.penalty = penalty;
        }
        if (exists fit_intercept) {
            model_opts.fit_intercept = fit_intercept;
        }

        # fit or update the model
        if (!model) {
            model = new ML::LogisticRegression(model_opts);
            model.fitMatrix(matrix, targets);
        } else if (online) {
            float lr = learning_rate ?? 0.01;
            model.updateMatrix(matrix, targets, lr);
        } else {
            model = new ML::LogisticRegression(model_opts);
            model.fitMatrix(matrix, targets);
        }

        # predict all records
        list<hash<ML::LogisticRegressionResult>> predictions = model.predictMatrix(matrix);

        # emit one event per record
        list<auto> events;
        for (int i = 0; i < records.lsize(); ++i) {
            hash<auto> record_values;
            foreach string f in (fields) {
                record_values{f} = DataProvider::AbstractDataProvider::getFieldValue(records[i], f);
            }
            record_values{target_field} = DataProvider::AbstractDataProvider::getFieldValue(
                records[i], target_field);
            push events, <LogisticRegressionEventInfo>{
                "event_type": "logistic-regression",
                "predicted_class": predictions[i].predicted_class,
                "max_probability": predictions[i].max_probability,
                "probabilities": predictions[i].probabilities,
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
        return new HashDeclDataType(TypedHash::forName("DataProviderML::LogisticRegressionEventInfo"));
    }
}
}
