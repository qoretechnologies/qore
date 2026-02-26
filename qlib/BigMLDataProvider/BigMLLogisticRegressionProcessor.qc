# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BigMLLogisticRegressionProcessor class definition

/*  BigMLLogisticRegressionProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that gets classification predictions from a BigML logistic regression model
/** Sends each record to the BigML prediction API using a pre-trained logistic regression
    model and returns the record enriched with the predicted class and per-class probabilities.

    @since %BigMLDataProvider 1.1
*/
public class BigMLLogisticRegressionProcessor inherits BigMLDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "BigMLLogisticRegressionProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_TRANSFORM,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "logisticregression": <DataProviderOptionInfo>{
                "display_name": "Logistic Regression Resource",
                "short_desc": "BigML logistic regression resource ID",
                "desc": "The BigML resource ID of the logistic regression model to use "
                    "(e.g., \"logisticregression/abc123\")",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "input_fields": <DataProviderOptionInfo>{
                "display_name": "Input Fields",
                "short_desc": "Record fields to send as input data",
                "desc": "A comma-separated list of field names from the input records to send as "
                    "input_data to the BigML prediction API; these fields must match the features "
                    "expected by the logistic regression model",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
        };
    }

    private {
        #! The logistic regression resource ID
        string lr_resource;

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
        lr_resource = copts.logisticregression;
        input_fields = map trim($1), copts.input_fields.split(","), trim($1).val();
        if (!input_fields) {
            throw "CONSTRUCTOR-ERROR", "no valid field names provided in 'input_fields' option";
        }
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "logistic-regression";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Gets classification predictions from a BigML logistic regression model for each "
            "record in the pipeline; sends the configured input fields to the BigML prediction "
            "API and returns the record enriched with the predicted class and per-class "
            "probabilities";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Processes a single record through BigML logistic regression prediction
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "BigMLLogisticRegressionProcessor: not configured; provide constructor options";
        }

        # Extract input fields from the record
        hash<auto> input_data;
        foreach string f in (input_fields) {
            if (exists record{f}) {
                input_data{f} = record{f};
            }
        }

        # Call BigML prediction API with logistic regression resource
        hash<auto> response = doPost("prediction", {
            "logisticregression": lr_resource,
            "input_data": input_data,
        });

        # Extract per-class probabilities from the response
        hash<auto> probs;
        float top_prob = 0.0;
        if (response.probabilities.typeCode() == NT_LIST) {
            foreach auto p in (response.probabilities) {
                if (p.typeCode() == NT_HASH) {
                    probs{p.category} = p.probability;
                }
            }
        }
        if (response.probability.typeCode() == NT_FLOAT
                || response.probability.typeCode() == NT_NUMBER) {
            top_prob = response.probability.toFloat();
        }

        # Return enriched record
        return <BigMLLogisticRegressionEventInfo>{
            "event_type": "bigml-logistic-regression",
            "prediction": response.prediction,
            "probability": top_prob,
            "probabilities": probs,
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
            TypedHash::forName("BigMLDataProvider::BigMLLogisticRegressionEventInfo"));
    }
}
}
