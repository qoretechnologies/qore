# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreRegressionMetricsProcessor class definition

/** QoreRegressionMetricsProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that measures prediction accuracy for numeric values
/** Accumulates records containing predicted and true numeric values, then computes
    MSE, RMSE, MAE, R², and explained variance using the corresponding ML module
    functions.

    On flush (or when the window is full), emits a single
    RegressionMetricsEventInfo event summarizing the batch.

*/
public class QoreRegressionMetricsProcessor inherits AbstractDataProviderMLProcessor {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreRegressionMetricsProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "predicted_field": <DataProviderOptionInfo>{
                "display_name": "Predicted Number Field",
                "short_desc": "Which field has the model's predicted number",
                "desc": "The field in each record that contains the number your model predicted. "
                    "For example, if your model writes its price estimate to `predicted_price`, "
                    "enter `predicted_price` here",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "true_field": <DataProviderOptionInfo>{
                "display_name": "Correct Answer Field",
                "short_desc": "Which field has the actual correct number",
                "desc": "The field in each record that contains the correct answer — what the "
                    "number actually was. For example, if the real sale price is stored in "
                    "`actual_price`, enter `actual_price` here",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Batch Size",
                "short_desc": "How many records to collect before scoring",
                "desc": "How many records to collect before computing a quality score. All "
                    "records in the batch are evaluated together.\n\n"
                    "**Tip**: use at least `30` for meaningful scores; `200`+ for a stable "
                    "assessment",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "example_value": 100,
            },
        };
    }

    private {
        #! Configured predicted field name
        string predicted_field;

        #! Configured true field name
        string true_field;

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
        predicted_field = copts.predicted_field;
        true_field = copts.true_field;
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
        return "regression-metrics";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Compares predicted numbers against actual values to measure prediction accuracy; "
            "accumulates records in batches and emits MSE, RMSE, MAE, R², and explained "
            "variance metrics";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records; emits metrics when window is full
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreRegressionMetricsProcessor: not configured; provide constructor options";
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

    #! Processes a window of records through regression metrics
    private list<auto> processWindow(list<hash<auto>> records) {
        list<float> y_true;
        list<float> y_pred;
        foreach hash<auto> rec in (records) {
            push y_true,
                DataProvider::AbstractDataProvider::getFieldValue(rec, true_field).toFloat();
            push y_pred,
                DataProvider::AbstractDataProvider::getFieldValue(rec, predicted_field).toFloat();
        }

        float mse = ML::ml_mse(y_true, y_pred);
        float rmse = ML::ml_rmse(y_true, y_pred);
        float mae = ML::ml_mae(y_true, y_pred);
        float r2 = ML::ml_r2_score(y_true, y_pred);
        float explained_variance = ML::ml_explained_variance(y_true, y_pred);

        return (<RegressionMetricsEventInfo>{
            "event_type": "regression-metrics",
            "mse": mse,
            "rmse": rmse,
            "mae": mae,
            "r2": r2,
            "explained_variance": explained_variance,
            "sample_count": records.lsize(),
        },);
    }

    #! Processes records in bulk hash-of-lists format without row materialization when possible
    private *hash<auto> processRecordsBulkImpl(hash<auto> records) {
        updatePendingOutputShape(DataProvider::DataProviderShape::HashOfLists);
        *auto result = processRecordsShapeImpl(records, DataProvider::DataProviderShape::HashOfLists);
        return exists result ? cast<hash>(result) : NOTHING;
    }

    #! Processes an explicitly negotiated pipeline shape without row materialization when possible
    private *auto processRecordsShapeImpl(auto records, DataProvider::DataProviderShape shape) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreRegressionMetricsProcessor: not configured; provide constructor options";
        }

        updatePendingOutputShape(shape);
        if (shape == DataProvider::DataProviderShape::RowRecords) {
            return processRecordImpl(records);
        }

        if (hasNestedFields() || buffer) {
            return processShapeRowsFallback(records, shape);
        }

        int count = DataProvider::AbstractDataProcessor::shapeRecordCount(records, shape);
        if (!count) {
            return;
        }

        *hash<auto> values = getShapeMetricValues(records, shape, count);
        if (!exists values) {
            return processShapeRowsFallback(records, shape);
        }

        list<float> y_true = values{"y_true"};
        list<float> y_pred = values{"y_pred"};
        *list<auto> events;
        int offset = 0;
        while (offset + window_size <= count) {
            events += processMetricWindow(y_true, y_pred, offset, window_size);
            offset += window_size;
        }

        if (offset < count) {
            buffer = AbstractDataProviderMLProcessor::materializeShapeRows(records, shape, offset, count - offset);
        }

        return AbstractDataProviderMLProcessor::convertRecordListOutputShape(events, shape);
    }

    #! Returns @ref True if either configured field requires row materialization
    private bool hasNestedFields() {
        return predicted_field.find(".") >= 0 || true_field.find(".") >= 0;
    }

    #! Reads configured numeric fields directly from shaped columns
    private *hash<auto> getShapeMetricValues(auto records, DataProvider::DataProviderShape shape, int count) {
        *auto true_column = DataProvider::AbstractDataProcessor::getShapeColumn(records, shape, true_field);
        *auto pred_column = DataProvider::AbstractDataProcessor::getShapeColumn(records, shape, predicted_field);
        if (!exists true_column || !exists pred_column) {
            return;
        }

        return {
            "y_true": DataProvider::AbstractDataProcessor::columnValuesToFloatList(true_column, count, true_field),
            "y_pred": DataProvider::AbstractDataProcessor::columnValuesToFloatList(
                pred_column, count, predicted_field),
        };
    }

    #! Uses the row-oriented implementation when a column fast path is not safe
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

    #! Computes regression metrics for a complete value window
    private list<auto> processMetricWindow(list<float> all_y_true, list<float> all_y_pred, int offset, int count) {
        list<float> y_true;
        list<float> y_pred;
        if (offset == 0 && count == all_y_true.lsize()) {
            y_true = all_y_true;
            y_pred = all_y_pred;
        } else {
            for (int i = offset; i < offset + count; ++i) {
                push y_true, all_y_true[i];
                push y_pred, all_y_pred[i];
            }
        }

        float mse = ML::ml_mse(y_true, y_pred);
        float rmse = ML::ml_rmse(y_true, y_pred);
        float mae = ML::ml_mae(y_true, y_pred);
        float r2 = ML::ml_r2_score(y_true, y_pred);
        float explained_variance = ML::ml_explained_variance(y_true, y_pred);

        return (<RegressionMetricsEventInfo>{
            "event_type": "regression-metrics",
            "mse": mse,
            "rmse": rmse,
            "mae": mae,
            "r2": r2,
            "explained_variance": explained_variance,
            "sample_count": count,
        },);
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
        return new HashDeclDataType(
            TypedHash::forName("DataProviderML::RegressionMetricsEventInfo"));
    }
}
}
