# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file PdfCoJobCheckDataProvider.qc PDF.co job check action data provider

/*  PdfCoJobCheckDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the PdfCoDataProvider module
public namespace PdfCoDataProvider {

#! Request type for checking the status of an asynchronous PDF.co job
public class PdfCoJobCheckRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "jobId": {
                "type": StringType,
                "display_name": "Job ID",
                "short_desc": "The job identifier returned by an async API call",
                "desc": "The job identifier returned by an async API call",
                "required": True,
                "preselected": True,
            },
            "force": {
                "type": SoftBoolOrNothingType,
                "display_name": "Force Check",
                "short_desc": "Forcibly check job status for long-running jobs",
                "desc": "Set to `True` to forcibly check job status for long-running jobs",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! PDF.co job check action data provider
public class PdfCoJobCheckDataProvider inherits PdfCoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "job-check",
            "desc": "Checks the status of an asynchronous PDF.co background job",
            "type": "PdfCoJobCheckDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(PdfCoRestClient::PdfCoRestClient rest) : PdfCoDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "job-check";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Checks the status of an asynchronous PDF.co background job";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new PdfCoJobCheckRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new PdfCoJobCheckResponseDataType();
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("job/check", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace PdfCoDataProvider
