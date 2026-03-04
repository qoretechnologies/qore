# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreLinearRegressionProcessor class definition

/** QoreLinearRegressionProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that performs linear regression on data windows
/** Accumulates records in windows and uses the ML::LinearRegression algorithm to
    fit a model and predict target values for each record.

    @since %DataProviderML 1.0
*/
public class QoreLinearRegressionProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreLinearRegressionProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "fields": <DataProviderOptionInfo>{
                "display_name": "Input Fields",
                "short_desc": "Numeric fields used to make predictions",
                "desc": "A comma-separated list of numeric field names that the model uses "
                    "to predict the target; for example, if predicting price, the input "
                    "fields might be \"square_feet,bedrooms,bathrooms\"; at least 1 field "
                    "is required",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "required": True,
                "input_field_names": True,
            },
            "target_field": <DataProviderOptionInfo>{
                "display_name": "Target Field",
                "short_desc": "The field to predict",
                "desc": "The name of the numeric field that the model will learn to predict; "
                    "for example, \"price\" or \"sales_volume\"; this field must exist in "
                    "the input records",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "input_field_names": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Window Size",
                "short_desc": "Number of records to learn from",
                "desc": "The number of records to collect before fitting the model and "
                    "generating predictions; the model learns the relationship between "
                    "input fields and the target from this batch; must be at least 2",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
            },
            "fit_intercept": <DataProviderOptionInfo>{
                "display_name": "Fit Intercept",
                "short_desc": "Include a baseline constant in the model",
                "desc": "When enabled, the model includes a baseline constant (intercept); "
                    "this should almost always be enabled unless you know the target is "
                    "zero when all inputs are zero; default: `True` (enabled)",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "normalize": <DataProviderOptionInfo>{
                "display_name": "Normalize Input",
                "short_desc": "Scale input fields to comparable ranges",
                "desc": "When enabled, input fields are scaled to have similar ranges before "
                    "fitting; this can improve accuracy when input fields have very different "
                    "units or magnitudes (e.g., mixing square feet and number of rooms); "
                    "default: `False` (disabled)",
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

        #! Fit intercept option
        bool fit_intercept = True;

        #! Normalize option
        bool do_normalize = False;

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
        if (exists copts.fit_intercept) {
            fit_intercept = copts.fit_intercept;
        }
        if (exists copts.normalize) {
            do_normalize = copts.normalize;
        }
        buffer = ();
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "linear-regression";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Performs linear regression on data windows; accumulates records, "
            "fits a model, and emits predictions for each record with R-squared "
            "and coefficient information";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records; emits predictions when window is full
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreLinearRegressionProcessor: not configured; provide constructor options";
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

    #! Processes a window of records through LinearRegression
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

        # create and fit model
        ML::LinearRegression lr({
            "fit_intercept": fit_intercept,
            "normalize": do_normalize,
        });
        lr.fitMatrix(matrix, targets);

        # predict all records
        list<hash<ML::LinearRegressionResult>> predictions = lr.predictMatrix(matrix);

        # emit one event per record
        list<auto> events;
        for (int i = 0; i < records.lsize(); ++i) {
            hash<auto> record_values;
            foreach string f in (fields) {
                record_values{f} = DataProvider::AbstractDataProvider::getFieldValue(records[i], f);
            }
            record_values{target_field} = DataProvider::AbstractDataProvider::getFieldValue(records[i], target_field);
            push events, <LinearRegressionEventInfo>{
                "event_type": "linear-regression",
                "prediction": predictions[i].prediction,
                "actual": targets[i],
                "residual": targets[i] - predictions[i].prediction,
                "r_squared": predictions[i].r_squared,
                "coefficients": predictions[i].coefficients,
                "intercept": predictions[i].intercept,
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
        return new HashDeclDataType(TypedHash::forName("DataProviderML::LinearRegressionEventInfo"));
    }
}
}
