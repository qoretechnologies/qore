# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreDecisionTreeClassificationProcessor class definition

/** QoreDecisionTreeClassificationProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that classifies records using a Decision Tree
/** Accumulates records in windows and uses the ML::DecisionTree algorithm in classification
    mode to predict class labels for each record by learning decision rules from the data.

*/
public class QoreDecisionTreeClassificationProcessor inherits AbstractDataProviderMLProcessor {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreDecisionTreeClassificationProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "fields": <DataProviderOptionInfo>{
                "display_name": "Input Fields",
                "short_desc": "Which numeric fields to use for building decision rules",
                "desc": "The numeric fields the tree uses to learn decision rules. For example, "
                    "to classify loan applications, select `income`, `credit_score`, and "
                    "`debt_ratio` — the tree learns rules like \"if income > 50000 and "
                    "credit_score > 700, then approve.\"\n\n"
                    "At least 1 field is required",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "required": True,
                "input_field_names": True,
            },
            "target_field": <DataProviderOptionInfo>{
                "display_name": "Category Field",
                "short_desc": "Which numeric field contains the category code",
                "desc": "The field that contains a **numeric** category code for each record. "
                    "For example, `risk_level` with values `0`, `1`, `2` or `approved` "
                    "with values `0` (denied) and `1` (approved).\n\n"
                    "**Note**: categories must be numeric (integers); use a numeric code "
                    "for text categories (e.g., `0` = low, `1` = medium, `2` = high)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "input_field_names": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Batch Size",
                "short_desc": "How many example records to learn from",
                "desc": "How many labeled records to collect before the tree learns decision "
                    "rules and starts predicting. The tree studies these examples to find the "
                    "best splits.\n\n"
                    "**Tip**: use at least `50`\u2013`200` for reliable results",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "example_value": 100,
            },
            "max_depth": <DataProviderOptionInfo>{
                "display_name": "Maximum Tree Depth",
                "short_desc": "How many levels of decision rules to allow",
                "desc": "The maximum number of levels in the decision tree. Deeper trees can "
                    "capture more complex patterns but may memorize the training data instead "
                    "of learning general rules.\n\n"
                    "**Tip**: start with `5`\u2013`15`; increase if accuracy is too low, decrease "
                    "if the model overfits\n\n"
                    "Default: `10`",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "criterion": <DataProviderOptionInfo>{
                "display_name": "Split Quality Measure",
                "short_desc": "How to evaluate the quality of each split",
                "desc": "The function used to measure the quality of a split at each node in "
                    "the tree. Both options produce similar results in most cases",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "gini",
                        "display_name": "Gini Impurity (default)",
                        "desc": "Measures how often a randomly chosen record would be "
                            "misclassified. Fast and effective for most classification tasks",
                    },
                    <AllowedValueInfo>{
                        "value": "entropy",
                        "display_name": "Information Gain (Entropy)",
                        "desc": "Measures the information gained by each split. May produce "
                            "slightly more balanced trees but is computationally slower",
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

        #! Maximum tree depth
        *int max_depth;

        #! Split criterion
        *string criterion;

        #! Retained DecisionTree model instance
        *ML::DecisionTree model;

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
        max_depth = copts.max_depth;
        criterion = copts.criterion;
        buffer = ();
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "decision-tree-classification";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Classifies records by learning decision rules from training data; accumulates "
            "records in windows, fits a decision tree model, and emits predicted category "
            "labels with confidence scores";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records; emits predictions when window is full
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreDecisionTreeClassificationProcessor: not configured; provide constructor options";
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

    #! Processes a window of records through Decision Tree classification
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
        model_opts.task = "classification";
        if (exists max_depth) {
            model_opts.max_depth = max_depth;
        }
        if (exists criterion) {
            model_opts.criterion = criterion;
        }

        # fit the model
        if (!model) {
            model = new ML::DecisionTree(model_opts);
            model.fitMatrix(matrix, targets);
        } else {
            # predict with existing model
        }

        # predict all records
        list<hash<ML::DecisionTreeClassificationResult>> predictions = model.predictClassificationMatrix(matrix);

        # emit one event per record
        list<auto> events;
        for (int i = 0; i < records.lsize(); ++i) {
            hash<auto> record_values;
            foreach string f in (fields) {
                record_values{f} = DataProvider::AbstractDataProvider::getFieldValue(records[i], f);
            }
            record_values{target_field} = DataProvider::AbstractDataProvider::getFieldValue(
                records[i], target_field);
            push events, <DecisionTreeClassificationEventInfo>{
                "event_type": "decision-tree-classification",
                "predicted_class": predictions[i].predicted_class,
                "confidence": predictions[i].max_probability,
                "record_values": record_values,
            };
        }
        return events;
    }

    #! Processes complete shaped windows through Decision Tree classification
    private *hash<auto> processCompleteShapeWindows(auto records, DataProvider::DataProviderShape shape) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreDecisionTreeClassificationProcessor: not configured; provide constructor options";
        }

        list<string> value_fields;
        foreach string field in (fields) {
            push value_fields, field;
        }
        push value_fields, target_field;
        if (buffer || AbstractDataProviderMLProcessor::hasNestedFieldPath(value_fields)) {
            return;
        }

        int count = DataProvider::AbstractDataProcessor::shapeRecordCount(records, shape);
        if (!count) {
            return {"results": ()};
        }

        hash<auto> field_values = AbstractDataProviderMLProcessor::getShapeFieldValueLists(
            records, shape, value_fields, count);
        list<auto> events;
        int offset = 0;
        while (offset + window_size <= count) {
            events += processMatrixWindow(field_values, value_fields, offset, window_size);
            offset += window_size;
        }

        if (offset < count) {
            buffer = AbstractDataProviderMLProcessor::materializeShapeRows(records, shape, offset, count - offset);
        }
        return {"results": events};
    }

    #! Processes one matrix window through Decision Tree classification
    private list<auto> processMatrixWindow(hash<auto> field_values, list<string> value_fields,
            int offset, int count) {
        list<list<float>> matrix = AbstractDataProviderMLProcessor::fieldValueListsToMatrix(
            field_values, fields, offset, count);
        list<float> targets;
        for (int row = offset; row < offset + count; ++row) {
            push targets, field_values{target_field}[row].toFloat();
        }

        hash<auto> model_opts;
        model_opts.task = "classification";
        if (exists max_depth) {
            model_opts.max_depth = max_depth;
        }
        if (exists criterion) {
            model_opts.criterion = criterion;
        }

        if (!model) {
            model = new ML::DecisionTree(model_opts);
            model.fitMatrix(matrix, targets);
        }

        list<hash<ML::DecisionTreeClassificationResult>> predictions = model.predictClassificationMatrix(matrix);

        list<auto> events;
        for (int i = 0; i < count; ++i) {
            int row = offset + i;
            push events, <DecisionTreeClassificationEventInfo>{
                "event_type": "decision-tree-classification",
                "predicted_class": predictions[i].predicted_class,
                "confidence": predictions[i].max_probability,
                "record_values": AbstractDataProviderMLProcessor::recordValuesFromFieldValueLists(
                    field_values, value_fields, row),
            };
        }
        return events;
    }

    #! Uses the row-oriented implementation as a semantics-preserving fallback
    private *auto processShapeRowsFallback(auto records, DataProvider::DataProviderShape shape) {
        list<auto> rows = DataProvider::AbstractDataProcessor::convertShape(
            records, shape, DataProvider::DataProviderShape::RowRecords);
        *list<auto> results;
        foreach auto row in (rows) {
            *auto row_result = processRecordImpl(row);
            if (exists row_result) {
                if (row_result.typeCode() == NT_LIST) {
                    results += row_result;
                } else {
                    push results, row_result;
                }
            }
        }
        return AbstractDataProviderMLProcessor::convertRecordListOutputShape(results, shape);
    }

    #! Processes records in bulk hash-of-lists format
    private *hash<auto> processRecordsBulkImpl(hash<auto> records) {
        *auto result = processRecordsShapeImpl(records, DataProvider::DataProviderShape::HashOfLists);
        if (!exists result) {
            return;
        }
        return cast<hash>(result);
    }

    #! Processes shaped records without first materializing row hashes when possible
    private *auto processRecordsShapeImpl(auto records, DataProvider::DataProviderShape shape) {
        updatePendingOutputShape(shape);
        if (shape == DataProvider::DataProviderShape::RowRecords) {
            return processRecordImpl(records);
        }

        *hash<auto> native_result = processCompleteShapeWindows(records, shape);
        if (exists native_result) {
            return AbstractDataProviderMLProcessor::convertRecordListOutputShape(native_result.results, shape);
        }
        return processShapeRowsFallback(records, shape);
    }

    #! Supports shaped bulk API
    private bool supportsBulkApiImpl() {
        return True;
    }

    #! Returns the processor capability bitfield
    private int getProcessorCapabilitiesImpl() {
        return DPC_AGGREGATE;
    }

    #! Returns the output type
    private *DataProvider::AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
        return new HashDeclDataType(TypedHash::forName("DataProviderML::DecisionTreeClassificationEventInfo"));
    }
}
}
