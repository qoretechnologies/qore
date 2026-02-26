# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BigMLAssociationSetProcessor class definition

/*  BigMLAssociationSetProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that gets association rules from a BigML association model
/** Sends each record to the BigML association set API using a pre-trained association
    model and returns the record enriched with matching association rules.

    @since %BigMLDataProvider 1.1
*/
public class BigMLAssociationSetProcessor inherits BigMLDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "BigMLAssociationSetProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_TRANSFORM,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "association": <DataProviderOptionInfo>{
                "display_name": "Association Resource",
                "short_desc": "BigML association resource ID",
                "desc": "The BigML resource ID of the association model to use for rule matching "
                    "(e.g., \"association/abc123\")",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "input_fields": <DataProviderOptionInfo>{
                "display_name": "Input Fields",
                "short_desc": "Record fields to send as input data",
                "desc": "A comma-separated list of field names from the input records to send as "
                    "input_data to the BigML association set API; these fields represent the items "
                    "to match against association rules",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
        };
    }

    private {
        #! The association resource ID
        string assoc_resource;

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
        assoc_resource = copts.association;
        input_fields = map trim($1), copts.input_fields.split(","), trim($1).val();
        if (!input_fields) {
            throw "CONSTRUCTOR-ERROR", "no valid field names provided in 'input_fields' option";
        }
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "association-set";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Gets matching association rules from a BigML association model for each record "
            "in the pipeline; sends the configured input fields to the BigML association set "
            "API and returns the record enriched with matching rules and their confidence scores";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Processes a single record through BigML association set
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "BigMLAssociationSetProcessor: not configured; provide constructor options";
        }

        # Extract input fields from the record
        hash<auto> input_data;
        foreach string f in (input_fields) {
            if (exists record{f}) {
                input_data{f} = record{f};
            }
        }

        # Call BigML association set API
        hash<auto> response = doPost("associationset", {
            "association": assoc_resource,
            "input_data": input_data,
        });

        # Return enriched record
        return <BigMLAssociationSetEventInfo>{
            "event_type": "bigml-association-set",
            "rules": response.association_set.rules ?? (),
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
            TypedHash::forName("BigMLDataProvider::BigMLAssociationSetEventInfo"));
    }
}
}
