# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BigMLAnomalyScoreProcessor class definition

/*  BigMLAnomalyScoreProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that scores records for anomalies using a BigML anomaly detector
/** Sends each record to the BigML anomaly score API and returns the record enriched
    with anomaly detection results.

    @since %BigMLDataProvider 1.0
*/
public class BigMLAnomalyScoreProcessor inherits BigMLDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "BigMLAnomalyScoreProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_TRANSFORM,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "anomaly": <DataProviderOptionInfo>{
                "display_name": "Anomaly Detector",
                "short_desc": "BigML anomaly detector resource ID",
                "desc": "The BigML resource ID of the anomaly detector to use for scoring "
                    "(e.g., \"anomaly/abc123\")",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "input_fields": <DataProviderOptionInfo>{
                "display_name": "Input Fields",
                "short_desc": "Record fields to send as input data",
                "desc": "A comma-separated list of field names from the input records to send as "
                    "input_data to the BigML anomaly score API",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "threshold": <DataProviderOptionInfo>{
                "display_name": "Anomaly Threshold",
                "short_desc": "Score above which a record is flagged as anomalous",
                "desc": "A value between 0.0 and 1.0; records with an anomaly score at or above "
                    "this threshold are flagged as anomalous; default: 0.5",
                "type": AbstractDataProviderTypeMap."*float",
            },
        };
    }

    private {
        #! The anomaly detector resource ID
        string anomaly_resource;

        #! The input field names
        list<string> input_fields;

        #! The anomaly threshold
        float threshold = 0.5;

        #! Whether configured
        bool configured = False;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) : BigMLDataProviderBase(options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        anomaly_resource = copts.anomaly;
        input_fields = map trim($1), copts.input_fields.split(","), trim($1).val();
        if (!input_fields) {
            throw "CONSTRUCTOR-ERROR", "no valid field names provided in 'input_fields' option";
        }
        if (exists copts.threshold) {
            threshold = copts.threshold;
        }
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "anomaly-score";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Scores records for anomalies using a BigML anomaly detector; sends the configured "
            "input fields to the BigML anomaly score API and returns the record enriched with "
            "anomaly score and classification";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Processes a single record through BigML anomaly scoring
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "BigMLAnomalyScoreProcessor: not configured; provide constructor options";
        }

        hash<auto> input_data;
        foreach string f in (input_fields) {
            if (exists record{f}) {
                input_data{f} = record{f};
            }
        }

        hash<auto> response = doPost("anomalyscore", {
            "anomaly": anomaly_resource,
            "input_data": input_data,
        });

        float score = response.score ?? 0.0;

        return <BigMLAnomalyScoreEventInfo>{
            "event_type": "bigml-anomaly-score",
            "anomaly_score": score,
            "is_anomaly": score >= threshold,
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
            TypedHash::forName("BigMLDataProvider::BigMLAnomalyScoreEventInfo"));
    }
}
}
