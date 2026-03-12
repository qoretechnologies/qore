# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyExportLeadsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the InstantlyDataProvider module
public namespace InstantlyDataProvider {

#! Request type for exporting leads
public class InstantlyExportLeadsRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "campaign_id": {
                "type": StringOrNothingType,
                "display_name": "Campaign ID",
                "short_desc": "Campaign UUID to export from",
                "desc": "The UUID of the campaign to export leads from",
                "ref_data": "campaigns",
                "preselected": True,
            },
            "list_id": {
                "type": StringOrNothingType,
                "display_name": "List ID",
                "short_desc": "Lead list UUID to export from",
                "desc": "The UUID of the lead list to export leads from",
                "ref_data": "lead_lists",
                "preselected": True,
            },
            "format": {
                "type": StringOrNothingType,
                "display_name": "Format",
                "short_desc": "Export format",
                "desc": "The format for the export",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Instantly export leads action data provider
public class InstantlyExportLeadsDataProvider inherits InstantlyDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "export-leads",
            "desc": "Export leads from Instantly",
            "type": "InstantlyExportLeadsDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static InstantlyExportLeadsRequestDataType RequestType();

        #! Response type
        static HashDataType ResponseType("ExportLeadsResponse");
    }

    #! Creates the object with a REST client
    constructor(InstantlyRestClient::InstantlyRestClient rest) : InstantlyDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "export-leads";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Export leads from Instantly";
    }

    #! Makes the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("/leads/export", req);
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace InstantlyDataProvider
