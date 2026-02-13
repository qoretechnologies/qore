# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreRecordLimitProcessor class definition

/** QoreRecordLimitProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that limits the number of records passed through
public class QoreRecordLimitProcessor inherits AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreRecordLimitProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_FILTER,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "limit": <DataProviderOptionInfo>{
                "display_name": "Record Limit",
                "short_desc": "Maximum number of records to pass through",
                "desc": "The maximum number of records to pass through the pipeline; "
                    "once the limit is reached, all subsequent records are dropped",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
            },
        };
    }

    private {
        #! Maximum number of records
        int limit;

        #! Current record count
        int count = 0;

        #! Flag indicating if the processor has been configured
        bool configured = False;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (exists copts.limit) {
            limit = copts.limit;
            configured = True;
        }
    }

    #! Returns the name of this data provider
    string getName() {
        return "record-limit";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Limits the number of records passed through the pipeline; the first N records are "
            "passed through and all subsequent records are dropped";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Processes a single record and returns the result
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR", "QoreRecordLimitProcessor: not configured; provide the 'limit' option";
        }
        return ++count <= limit ? record : NOTHING;
    }

    #! Returns the processor capability bitfield
    private int getProcessorCapabilitiesImpl() {
        return DPC_FILTER;
    }
}
}
