# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BigMLForecastProcessor class definition

/*  BigMLForecastProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that gets time series forecasts from a BigML model
/** Sends each record to the BigML forecast API using a pre-trained time series
    model and returns the record enriched with forecast predictions.

    @since %BigMLDataProvider 1.1
*/
public class BigMLForecastProcessor inherits BigMLDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "BigMLForecastProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_TRANSFORM,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "timeseries": <DataProviderOptionInfo>{
                "display_name": "Time Series Resource",
                "short_desc": "BigML time series resource ID",
                "desc": "The BigML resource ID of the time series model to use for forecasting "
                    "(e.g., \"timeseries/abc123\")",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "input_fields": <DataProviderOptionInfo>{
                "display_name": "Input Fields",
                "short_desc": "Record fields to send as input data",
                "desc": "A comma-separated list of field names from the input records to send as "
                    "input_data to the BigML forecast API; these fields should match the "
                    "objective fields of the time series model",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "horizon": <DataProviderOptionInfo>{
                "display_name": "Forecast Horizon",
                "short_desc": "Number of steps to forecast ahead",
                "desc": "The number of time steps to forecast into the future; "
                    "default: 1",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    private {
        #! The time series resource ID
        string ts_resource;

        #! The input field names
        list<string> input_fields;

        #! The forecast horizon
        int horizon = 1;

        #! Whether configured
        bool configured = False;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) : BigMLDataProviderBase(options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        ts_resource = copts.timeseries;
        input_fields = map trim($1), copts.input_fields.split(","), trim($1).val();
        if (!input_fields) {
            throw "CONSTRUCTOR-ERROR", "no valid field names provided in 'input_fields' option";
        }
        if (exists copts.horizon) {
            horizon = copts.horizon;
            if (horizon < 1) {
                throw "CONSTRUCTOR-ERROR",
                    sprintf("horizon must be >= 1; got %d", horizon);
            }
        }
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "forecast";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Gets time series forecasts from a BigML time series model for each record "
            "in the pipeline; sends the configured input fields to the BigML forecast API "
            "and returns the record enriched with forecast predictions and confidence intervals";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Processes a single record through BigML forecast
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "BigMLForecastProcessor: not configured; provide constructor options";
        }

        # Extract input fields from the record
        hash<auto> input_data;
        foreach string f in (input_fields) {
            if (exists record{f}) {
                input_data{f} = record{f};
            }
        }

        # Build forecast request body
        hash<auto> body = {
            "timeseries": ts_resource,
            "input_data": input_data,
        };
        if (horizon != 1) {
            body.horizon = horizon;
        }

        # Call BigML forecast API
        hash<auto> response = doPost("forecast", body);

        # Return enriched record
        return <BigMLForecastEventInfo>{
            "event_type": "bigml-forecast",
            "forecast": response.forecast ?? {},
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
            TypedHash::forName("BigMLDataProvider::BigMLForecastEventInfo"));
    }
}
}
