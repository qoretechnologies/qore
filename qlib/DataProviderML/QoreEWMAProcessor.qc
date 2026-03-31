# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreEWMAProcessor class definition

/** QoreEWMAProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that enriches records with exponential weighted moving averages
/** Computes an exponential weighted moving average (EWMA) for specified numeric fields.
    The EWMA gives more weight to recent values, making it responsive to changes while
    smoothing out noise. Each input record is passed through enriched with EWMA fields.

*/
public class QoreEWMAProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreEWMAProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_TRANSFORM,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "fields": <DataProviderOptionInfo>{
                "display_name": "Numeric Fields to Smooth",
                "short_desc": "Which numeric fields to compute EWMA for",
                "desc": "The numeric fields from your data that should have exponential "
                    "weighted moving averages computed. For example, select `temperature`, "
                    "`cpu_usage`, and `response_time` to track smoothed versions of each "
                    "signal.\n\n"
                    "At least 1 field is required",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "required": True,
                "input_field_names": True,
            },
            "alpha": <DataProviderOptionInfo>{
                "display_name": "Smoothing Factor",
                "short_desc": "How much weight to give the latest value (0 to 1)",
                "desc": "Controls how quickly the moving average reacts to new values. "
                    "A value close to `1.0` makes the average track the latest value closely "
                    "(almost no smoothing), while a value close to `0.0` makes it change "
                    "slowly (heavy smoothing).\n\n"
                    "Formula: `ewma = alpha * current_value + (1 - alpha) * previous_ewma`\n\n"
                    "**Tip**: start with `0.3` and adjust — lower for smoother signals, "
                    "higher for more responsive tracking",
                "type": AbstractDataProviderTypeMap."float",
                "required": True,
                "allowed_values_creatable": True,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": 0.1,
                        "display_name": "Smooth (0.1)",
                        "desc": "Heavily smoothed — slow to react to changes, best for "
                            "filtering out high-frequency noise in stable signals",
                    },
                    <AllowedValueInfo>{
                        "value": 0.2,
                        "display_name": "Moderate (0.2)",
                        "desc": "Good balance between smoothness and responsiveness — "
                            "suitable for most monitoring use cases",
                    },
                    <AllowedValueInfo>{
                        "value": 0.3,
                        "display_name": "Default (0.3)",
                        "desc": "Standard starting point — responds to trends within a few "
                            "records while still filtering noise",
                    },
                    <AllowedValueInfo>{
                        "value": 0.5,
                        "display_name": "Reactive (0.5)",
                        "desc": "Responds quickly to changes — useful when recent values "
                            "matter more than historical ones",
                    },
                    <AllowedValueInfo>{
                        "value": 0.9,
                        "display_name": "Very Reactive (0.9)",
                        "desc": "Almost no smoothing — tracks the raw signal closely, best "
                            "for detecting sudden shifts immediately",
                    },
                ),
            },
        };
    }

    private {
        #! Configured field names
        list<string> fields;

        #! Smoothing factor (0 < alpha <= 1)
        float alpha;

        #! Current EWMA values per field
        hash<string, float> ewma_values;

        #! Whether configured
        bool configured = False;

        #! Whether the first record has been seen (initializes EWMA)
        bool initialized = False;
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
        alpha = copts.alpha.toFloat();
        if (alpha <= 0.0 || alpha > 1.0) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("alpha must be in the range (0.0, 1.0]; got %g", alpha);
        }
        ewma_values = {};
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "ewma";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Enriches records with exponential weighted moving averages (EWMA) for "
            "specified numeric fields; gives more weight to recent values for responsive "
            "trend tracking while smoothing out noise";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Processes a single record: updates EWMA state and enriches the record
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreEWMAProcessor: not configured; provide constructor options";
        }

        # start with the original record
        hash<auto> result = {} + record;

        foreach string field in (fields) {
            float current_value =
                DataProvider::AbstractDataProvider::getFieldValue(record, field).toFloat();

            if (!initialized) {
                # first record: initialize EWMA to the raw value
                ewma_values{field} = current_value;
            } else {
                # subsequent records: apply EWMA formula
                ewma_values{field} = alpha * current_value
                    + (1.0 - alpha) * ewma_values{field};
            }

            result{field + "_ewma"} = ewma_values{field};
        }

        if (!initialized) {
            initialized = True;
        }

        return result;
    }

    #! Does not support bulk API
    private bool supportsBulkApiImpl() {
        return False;
    }

    #! Returns the processor capability bitfield
    private int getProcessorCapabilitiesImpl() {
        return DPC_TRANSFORM;
    }

    #! Returns NOTHING — output type is dynamic (input record enriched with EWMA fields)
    private *DataProvider::AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
        return NOTHING;
    }
}
}
