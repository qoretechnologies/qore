# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BigMLCentroidProcessor class definition

/*  BigMLCentroidProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that assigns records to clusters using a BigML cluster model
/** Sends each record to the BigML centroid API and returns the record enriched
    with cluster assignment results.

    @since %BigMLDataProvider 1.0
*/
public class BigMLCentroidProcessor inherits BigMLDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "BigMLCentroidProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_TRANSFORM,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "cluster": <DataProviderOptionInfo>{
                "display_name": "Cluster Model",
                "short_desc": "BigML cluster resource ID",
                "desc": "The BigML resource ID of the cluster model to use for centroid assignment "
                    "(e.g., \"cluster/abc123\")",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "input_fields": <DataProviderOptionInfo>{
                "display_name": "Input Fields",
                "short_desc": "Record fields to send as input data",
                "desc": "A comma-separated list of field names from the input records to send as "
                    "input_data to the BigML centroid API",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
        };
    }

    private {
        #! The cluster resource ID
        string cluster_resource;

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
        cluster_resource = copts.cluster;
        input_fields = map trim($1), copts.input_fields.split(","), trim($1).val();
        if (!input_fields) {
            throw "CONSTRUCTOR-ERROR", "no valid field names provided in 'input_fields' option";
        }
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "centroid";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Assigns records to clusters using a BigML cluster model; sends the configured "
            "input fields to the BigML centroid API and returns the record enriched with "
            "cluster assignment and distance information";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Processes a single record through BigML centroid assignment
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "BigMLCentroidProcessor: not configured; provide constructor options";
        }

        hash<auto> input_data;
        foreach string f in (input_fields) {
            if (exists record{f}) {
                input_data{f} = record{f};
            }
        }

        hash<auto> response = doPost("centroid", {
            "cluster": cluster_resource,
            "input_data": input_data,
        });

        return <BigMLCentroidEventInfo>{
            "event_type": "bigml-centroid",
            "centroid_name": response.centroid_name ?? "",
            "centroid_id": response.centroid_id ?? "",
            "distance": response.distance ?? 0.0,
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
            TypedHash::forName("BigMLDataProvider::BigMLCentroidEventInfo"));
    }
}
}
