# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BigMLTrainAndPredictProcessor class definition

/*  BigMLTrainAndPredictProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BigMLDataProvider module
public namespace BigMLDataProvider {
#! Pipeline processor that trains a BigML model on buffered records and predicts on the same data
/** Accumulates records in a window, uploads them to BigML to create a source and dataset,
    trains a model of the specified type, then runs predictions on each record using the
    trained model. This combines training and prediction into a single pipeline step,
    eliminating the need for users to manually create sources, datasets, and models.

    Supports eight model types:
    - \b model: Standard decision tree for classification/regression (supervised)
    - \b ensemble: Random forest for improved accuracy (supervised)
    - \b linearregression: Linear regression for numeric prediction (supervised)
    - \b logisticregression: Logistic regression for classification (supervised)
    - \b anomaly: Isolation forest for anomaly detection (unsupervised)
    - \b cluster: K-means clustering for segmentation (unsupervised)
    - \b timeseries: Time series for forecasting (unsupervised)
    - \b pca: PCA for dimensionality reduction (unsupervised)

    @since %BigMLDataProvider 1.0
*/
public class BigMLTrainAndPredictProcessor inherits BigMLDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "BigMLTrainAndPredictProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Valid model types
        const ModelTypes = ("model", "ensemble", "anomaly", "cluster",
            "linearregression", "logisticregression", "timeseries", "pca");

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "model_type": <DataProviderOptionInfo>{
                "display_name": "Model Type",
                "short_desc": "Type of BigML model to train",
                "desc": "The type of BigML model to train. Supervised types (model, ensemble, "
                    "linearregression, logisticregression) require an objective field; "
                    "unsupervised types (anomaly, cluster, timeseries, pca) do not.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "model",
                        "desc": "Standard decision tree model for classification and regression",
                    },
                    <AllowedValueInfo>{
                        "value": "ensemble",
                        "desc": "Random forest ensemble for improved accuracy",
                    },
                    <AllowedValueInfo>{
                        "value": "anomaly",
                        "desc": "Isolation forest for anomaly detection (unsupervised)",
                    },
                    <AllowedValueInfo>{
                        "value": "cluster",
                        "desc": "K-means clustering for segmentation (unsupervised)",
                    },
                    <AllowedValueInfo>{
                        "value": "linearregression",
                        "desc": "Linear regression for numeric prediction (supervised)",
                    },
                    <AllowedValueInfo>{
                        "value": "logisticregression",
                        "desc": "Logistic regression for classification (supervised)",
                    },
                    <AllowedValueInfo>{
                        "value": "timeseries",
                        "desc": "Time series for forecasting (unsupervised)",
                    },
                    <AllowedValueInfo>{
                        "value": "pca",
                        "desc": "PCA for dimensionality reduction (unsupervised)",
                    },
                ),
            },
            "input_fields": <DataProviderOptionInfo>{
                "display_name": "Input Fields",
                "short_desc": "Record fields to use as features",
                "desc": "A comma-separated list of field names from the input records to use "
                    "as features for model training and prediction input; for example: "
                    "\"age,income,spending_score\"",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "objective_field": <DataProviderOptionInfo>{
                "display_name": "Objective Field",
                "short_desc": "Target field to predict (supervised models only)",
                "desc": "The field name from the input records that contains the target value "
                    "to predict; required for supervised model types (model, ensemble, "
                    "linearregression, logisticregression); ignored for unsupervised types "
                    "(anomaly, cluster, timeseries, pca)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Window Size",
                "short_desc": "Number of records to train on",
                "desc": "The number of records to accumulate before training a model and "
                    "generating predictions; larger windows produce better models but take "
                    "longer to train; must be at least 5",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
            },
            "model_name": <DataProviderOptionInfo>{
                "display_name": "Model Name",
                "short_desc": "Optional name for the trained model",
                "desc": "An optional name for the trained BigML model; if not provided, "
                    "an auto-generated name with timestamp is used",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "cleanup_model": <DataProviderOptionInfo>{
                "display_name": "Delete Model After Use",
                "short_desc": "Whether to delete the model after predictions",
                "desc": "When True, the trained model is deleted after predictions are "
                    "generated; when False (default), the model is kept in your BigML "
                    "account so it can be reused with the 'predict' processor action",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "poll_interval_ms": <DataProviderOptionInfo>{
                "display_name": "Poll Interval (ms)",
                "short_desc": "How often to check training status",
                "desc": "How often (in milliseconds) to check if BigML resource creation "
                    "has completed; default: 2000",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "timeout_ms": <DataProviderOptionInfo>{
                "display_name": "Timeout (ms)",
                "short_desc": "Maximum wait time per step",
                "desc": "Maximum time (in milliseconds) to wait for each step (source "
                    "creation, dataset creation, model training) to complete; "
                    "default: 300000 (5 minutes)",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    private {
        #! The model type to train
        string model_type;

        #! The input field names (features)
        list<string> input_fields;

        #! The objective field name (target for supervised models)
        *string objective_field;

        #! Window size for batching
        int window_size;

        #! Optional model name
        *string model_name;

        #! Whether to delete the model after use
        bool cleanup_model = False;

        #! Poll interval for resource status
        int poll_interval_ms = 2000;

        #! Timeout for resource creation
        int timeout_ms = 300000;

        #! Accumulated records
        list<hash<auto>> buffer;

        #! Whether configured
        bool configured = False;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) : BigMLDataProviderBase(options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        model_type = copts.model_type;
        if (!inlist(model_type, ModelTypes)) {
            throw "CONSTRUCTOR-ERROR", sprintf("invalid model_type %y; valid types: %y",
                model_type, ModelTypes);
        }
        input_fields = map trim($1), copts.input_fields.split(","), trim($1).val();
        if (!input_fields) {
            throw "CONSTRUCTOR-ERROR", "no valid field names provided in 'input_fields' option";
        }
        # supervised models require an objective field
        if (model_type == "model" || model_type == "ensemble"
                || model_type == "linearregression" || model_type == "logisticregression") {
            if (!copts.objective_field) {
                throw "CONSTRUCTOR-ERROR",
                    sprintf("'objective_field' is required for supervised model type '%s'",
                        model_type);
            }
            objective_field = copts.objective_field;
        }
        window_size = copts.window_size;
        if (window_size < 5) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("window_size must be >= 5 for meaningful training; got %d", window_size);
        }
        if (copts.model_name) {
            model_name = copts.model_name;
        }
        if (exists copts.cleanup_model) {
            cleanup_model = copts.cleanup_model;
        }
        if (exists copts.poll_interval_ms) {
            poll_interval_ms = copts.poll_interval_ms;
        }
        if (exists copts.timeout_ms) {
            timeout_ms = copts.timeout_ms;
        }
        buffer = ();
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "train-and-predict";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Trains a BigML model on buffered records and predicts on the same data in a "
            "single pipeline step; uploads training data, creates a model, runs predictions, "
            "and emits enriched records with results — no manual source/dataset/model creation "
            "needed";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns dynamic action options based on current option values
    /** Overrides tryGetActionOptionsWithOptionsImpl directly to ensure this works
        regardless of the DataProvider module version (the DPAT_PIPELINE_PROCESSOR dispatch
        in the base class was added in DataProvider 3.3).
    */
    private *hash<string, hash<ActionOptionInfo>> tryGetActionOptionsWithOptionsImpl(
            hash<DataProviderActionInfo> action, *hash<auto> options) {
        return getDynamicOptions(action, options);
    }

    #! Returns dynamic action options based on current option values
    private *hash<string, hash<ActionOptionInfo>> tryGetActionOptionsForPipelineProcessorWithOptionsImpl(
            hash<DataProviderActionInfo> action, *hash<auto> options) {
        return getDynamicOptions(action, options);
    }

    #! Core dynamic options logic shared by both override methods
    private *hash<string, hash<ActionOptionInfo>> getDynamicOptions(
            hash<DataProviderActionInfo> action, *hash<auto> options) {
        # handle both wrapped {"value": "model"} and unwrapped "model" formats
        *string mt = options.model_type.value ?? options.model_type;
        LoggerWrapper::info("BigML train-and-predict dynamic options: model_type=%y options=%y "
            "action.data_dependent_options=%y action.action_code=%y", mt, keys options,
            action.data_dependent_options, action.action_code);
        if (!mt) {
            return action.options;
        }
        hash<string, hash<ActionOptionInfo>> rv = action.options;
        if (mt == "model" || mt == "ensemble" || mt == "linearregression"
                || mt == "logisticregression") {
            # supervised: objective_field is required and enabled
            rv.objective_field.required = True;
            rv.objective_field.disabled = False;
        } else {
            # unsupervised (anomaly, cluster, timeseries, pca): disable objective_field
            rv.objective_field.disabled = True;
            rv.objective_field.required = False;
        }
        LoggerWrapper::info("BigML train-and-predict returning: objective_field.required=%y "
            "objective_field.disabled=%y", rv.objective_field.required, rv.objective_field.disabled);
        return rv;
    }

    #! Accumulates records; trains and predicts when window is full
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "BigMLTrainAndPredictProcessor: not configured; provide constructor options";
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
        # need at least 5 records for meaningful training
        if (buffer.lsize() >= 5) {
            list<auto> results = processWindow(buffer);
            buffer = ();
            return results;
        }
        buffer = ();
        return;
    }

    #! Trains a model on the records and predicts on each one
    private list<auto> processWindow(list<hash<auto>> records) {
        # Build CSV columns: input_fields + objective_field (if supervised)
        list<string> csv_fields = input_fields;
        if (objective_field) {
            push csv_fields, objective_field;
        }

        # Create CSV data from records
        string csv_data = csv_fields.join(",") + "\n";
        foreach hash<auto> rec in (records) {
            list<string> row;
            foreach string f in (csv_fields) {
                auto val = rec{f};
                if (exists val) {
                    string sval = string(val);
                    # Escape CSV fields containing commas, quotes, or newlines
                    if (sval =~ /[,"\n]/) {
                        sval = "\"" + regex_subst(sval, "\"", "\"\"", RE_Global) + "\"";
                    }
                    push row, sval;
                } else {
                    push row, "";
                }
            }
            csv_data += row.join(",") + "\n";
        }

        string ts = now_us().format("YYYYMMDDHHmmSS");

        # Step 1: Upload source
        hash<auto> source = doPost("source", {
            "data": csv_data,
            "name": sprintf("bigml-train-predict-src-%s", ts),
        });
        string source_id = source.resource;
        pollUntilFinished(source_id, poll_interval_ms, timeout_ms);

        # Step 2: Create dataset
        hash<auto> dataset = doPost("dataset", {
            "source": source_id,
        });
        string dataset_id = dataset.resource;
        pollUntilFinished(dataset_id, poll_interval_ms, timeout_ms);

        # Step 3: Train model
        hash<auto> model_body = {
            "dataset": dataset_id,
            "name": model_name ?? sprintf("bigml-%s-%s", model_type, ts),
        };
        if (objective_field && (model_type == "model" || model_type == "ensemble"
                || model_type == "linearregression" || model_type == "logisticregression")) {
            model_body.objective_field = objective_field;
        }
        hash<auto> trained_model = doPost(model_type, model_body);
        string model_id = trained_model.resource;
        pollUntilFinished(model_id, poll_interval_ms, timeout_ms);

        # Step 4: Predict on each record
        list<auto> events;
        foreach hash<auto> rec in (records) {
            # Build input data from input_fields only (not objective field)
            hash<auto> input_data;
            foreach string f in (input_fields) {
                if (exists rec{f}) {
                    input_data{f} = rec{f};
                }
            }

            switch (model_type) {
                case "model":
                case "ensemble": {
                    hash<auto> pred = doPost("prediction", {
                        model_type: model_id,
                        "input_data": input_data,
                    });
                    push events, <BigMLTrainAndPredictEventInfo>{
                        "event_type": "bigml-train-and-predict",
                        "model_type": model_type,
                        "model_resource": model_id,
                        "prediction": pred.prediction,
                        "confidence": pred.confidence ?? 0.0,
                        "objective_field": pred.objective_field_name ?? objective_field,
                        "resource": pred.resource ?? "",
                        "record_values": input_data,
                    };
                    break;
                }
                case "anomaly": {
                    hash<auto> score = doPost("anomalyscore", {
                        "anomaly": model_id,
                        "input_data": input_data,
                    });
                    push events, <BigMLTrainAndPredictEventInfo>{
                        "event_type": "bigml-train-and-predict",
                        "model_type": model_type,
                        "model_resource": model_id,
                        "anomaly_score": score.score ?? 0.0,
                        "resource": score.resource ?? "",
                        "record_values": input_data,
                    };
                    break;
                }
                case "cluster": {
                    hash<auto> centroid = doPost("centroid", {
                        "cluster": model_id,
                        "input_data": input_data,
                    });
                    push events, <BigMLTrainAndPredictEventInfo>{
                        "event_type": "bigml-train-and-predict",
                        "model_type": model_type,
                        "model_resource": model_id,
                        "centroid_name": centroid.centroid_name ?? "",
                        "centroid_id": centroid.centroid_id ?? "",
                        "distance": centroid.distance ?? 0.0,
                        "resource": centroid.resource ?? "",
                        "record_values": input_data,
                    };
                    break;
                }
                case "linearregression": {
                    hash<auto> pred = doPost("prediction", {
                        "linearregression": model_id,
                        "input_data": input_data,
                    });
                    push events, <BigMLTrainAndPredictEventInfo>{
                        "event_type": "bigml-train-and-predict",
                        "model_type": model_type,
                        "model_resource": model_id,
                        "prediction": pred.prediction,
                        "confidence": pred.confidence ?? 0.0,
                        "objective_field": pred.objective_field_name ?? objective_field,
                        "resource": pred.resource ?? "",
                        "record_values": input_data,
                    };
                    break;
                }
                case "logisticregression": {
                    hash<auto> pred = doPost("prediction", {
                        "logisticregression": model_id,
                        "input_data": input_data,
                    });
                    # Extract per-class probabilities
                    hash<auto> probs;
                    if (pred.probabilities.typeCode() == NT_LIST) {
                        foreach auto p in (pred.probabilities) {
                            if (p.typeCode() == NT_HASH) {
                                probs{p.category} = p.probability;
                            }
                        }
                    }
                    push events, <BigMLTrainAndPredictEventInfo>{
                        "event_type": "bigml-train-and-predict",
                        "model_type": model_type,
                        "model_resource": model_id,
                        "prediction": pred.prediction,
                        "confidence": pred.probability.typeCode() == NT_FLOAT
                            || pred.probability.typeCode() == NT_NUMBER
                            ? pred.probability.toFloat() : 0.0,
                        "probability": pred.probability.typeCode() == NT_FLOAT
                            || pred.probability.typeCode() == NT_NUMBER
                            ? pred.probability.toFloat() : 0.0,
                        "probabilities": probs,
                        "objective_field": pred.objective_field_name ?? objective_field,
                        "resource": pred.resource ?? "",
                        "record_values": input_data,
                    };
                    break;
                }
                case "timeseries": {
                    hash<auto> fc = doPost("forecast", {
                        "timeseries": model_id,
                        "input_data": input_data,
                    });
                    push events, <BigMLTrainAndPredictEventInfo>{
                        "event_type": "bigml-train-and-predict",
                        "model_type": model_type,
                        "model_resource": model_id,
                        "forecast": fc.forecast ?? {},
                        "resource": fc.resource ?? "",
                        "record_values": input_data,
                    };
                    break;
                }
                case "pca": {
                    hash<auto> proj = doPost("projection", {
                        "pca": model_id,
                        "input_data": input_data,
                    });
                    push events, <BigMLTrainAndPredictEventInfo>{
                        "event_type": "bigml-train-and-predict",
                        "model_type": model_type,
                        "model_resource": model_id,
                        "projection": proj.projection ?? {},
                        "resource": proj.resource ?? "",
                        "record_values": input_data,
                    };
                    break;
                }
            }
        }

        # Step 5: Cleanup temporary resources (always clean source + dataset)
        try { doDelete(source_id); } catch () {}
        try { doDelete(dataset_id); } catch () {}
        if (cleanup_model) {
            try { doDelete(model_id); } catch () {}
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
        return new HashDeclDataType(
            TypedHash::forName("BigMLDataProvider::BigMLTrainAndPredictEventInfo"));
    }
}
}
