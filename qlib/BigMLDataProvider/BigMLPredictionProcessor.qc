# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BigMLPredictionProcessor class definition

/*  BigMLPredictionProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that gets predictions from a BigML model for each record
/** Sends each record to the BigML prediction API and returns the record enriched
    with prediction results.

    @since %BigMLDataProvider 1.0
*/
public class BigMLPredictionProcessor inherits BigMLDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "BigMLPredictionProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_TRANSFORM,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "model": <DataProviderOptionInfo>{
                "display_name": "Model Resource",
                "short_desc": "BigML model, ensemble, or deepnet resource ID",
                "desc": "The BigML resource ID of the model to use for predictions; "
                    "can be a model (e.g., \"model/abc123\"), ensemble (e.g., \"ensemble/abc123\"), "
                    "or deepnet (e.g., \"deepnet/abc123\")",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "input_fields": <DataProviderOptionInfo>{
                "display_name": "Input Fields",
                "short_desc": "Record fields to send as input data",
                "desc": "A comma-separated list of field names from the input records to send as "
                    "input_data to the BigML prediction API; these fields must match the fields "
                    "expected by the model",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
        };
    }

    private {
        #! The model/ensemble/deepnet resource ID
        string model_resource;

        #! The input field names
        list<string> input_fields;

        #! Whether configured
        bool configured = False;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) : BigMLDataProviderBase(options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        model_resource = copts.model;
        input_fields = map trim($1), copts.input_fields.split(","), trim($1).val();
        if (!input_fields) {
            throw "CONSTRUCTOR-ERROR", "no valid field names provided in 'input_fields' option";
        }
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "predict";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Gets predictions from a BigML model, ensemble, or deepnet for each record "
            "in the pipeline; sends the configured input fields to the BigML prediction API "
            "and returns the record enriched with prediction results";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Processes a single record through BigML prediction
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "BigMLPredictionProcessor: not configured; provide constructor options";
        }

        # Extract input fields from the record
        hash<auto> input_data;
        foreach string f in (input_fields) {
            if (exists record{f}) {
                input_data{f} = record{f};
            }
        }

        # Determine the resource type key (model, ensemble, or deepnet)
        string resource_key;
        if (model_resource =~ /^ensemble\//) {
            resource_key = "ensemble";
        } else if (model_resource =~ /^deepnet\//) {
            resource_key = "deepnet";
        } else {
            resource_key = "model";
        }

        # Call BigML prediction API
        hash<auto> response = doPost("prediction", {
            resource_key: model_resource,
            "input_data": input_data,
        });

        # Return enriched record
        return <BigMLPredictionEventInfo>{
            "event_type": "bigml-predict",
            "prediction": response.prediction,
            "confidence": response.confidence ?? 0.0,
            "objective_field": response.objective_field_name ?? "",
            "resource": response.resource ?? "",
            "record_values": input_data,
        };
    }

    #! Does not support bulk API
    private bool supportsBulkApiImpl() {
        return False;
    }

    #! Returns the processor capability bitfield
    private int getProcessorCapabilitiesImpl() {
        return DPC_TRANSFORM;
    }

    #! Returns the output type
    private *DataProvider::AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
        return new HashDeclDataType(
            TypedHash::forName("BigMLDataProvider::BigMLPredictionEventInfo"));
    }
}
}
