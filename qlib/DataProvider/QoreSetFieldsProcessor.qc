# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreSetFieldsProcessor class definition

/** QoreSetFieldsProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DataProvider module
public namespace DataProvider {
#! Pipeline processor that sets fields to constant values in each record
public class QoreSetFieldsProcessor inherits AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreSetFieldsProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_TRANSFORM,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "fields": <DataProviderOptionInfo>{
                "display_name": "Fields",
                "short_desc": "A hash of field names and values to set",
                "desc": "A hash of field names and values to set on each record; existing fields "
                    "are overwritten, new fields are added",
                "type": AbstractDataProviderTypeMap."*hash",
                "required": True,
            },
        };
    }

    private {
        #! The fields hash to apply
        hash<auto> fields;

        #! Flag indicating if the processor has been configured
        bool configured = False;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.fields) {
            fields = copts.fields;
            configured = True;
        }
    }

    #! Returns the name of this data provider
    string getName() {
        return "set-fields";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Sets fields to constant values in each record; if a field already exists it is "
            "overwritten, otherwise it is added";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Processes a single record and returns the result
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR", "QoreSetFieldsProcessor: not configured; provide 'fields' option";
        }
        return record + fields;
    }

    #! Indicates that this processor does not support the bulk API
    private bool supportsBulkApiImpl() {
        return False;
    }

    #! Returns the processor capability bitfield
    private int getProcessorCapabilitiesImpl() {
        return DPC_TRANSFORM;
    }
}
}
