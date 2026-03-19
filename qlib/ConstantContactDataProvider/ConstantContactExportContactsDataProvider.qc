# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConstantContactExportContactsDataProvider.qc Constant Contact export contacts action data provider

/*  ConstantContactExportContactsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ConstantContactDataProvider module
public namespace ConstantContactDataProvider {

#! Request type for exporting contacts
public class ConstantContactExportContactsRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "list_ids": {
                "type": AutoListOrNothingType,
                "display_name": "List IDs",
                "short_desc": "List IDs to export contacts from",
                "desc": "A list of contact list IDs to export contacts from",
            },
            "segment_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Segment ID",
                "short_desc": "Segment ID to export contacts from",
                "desc": "The segment ID to export contacts from",
            },
            "fields": {
                "type": AutoListOrNothingType,
                "display_name": "Fields",
                "short_desc": "Fields to include in export",
                "desc": "A list of field names to include in the export",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Constant Contact export contacts action data provider
public class ConstantContactExportContactsDataProvider inherits ConstantContactDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "export-contacts",
            "desc": "Export contacts from Constant Contact via bulk activity",
            "type": "ConstantContactExportContactsDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Static request type
        static ConstantContactExportContactsRequestDataType RequestType();
    }

    #! Creates the object with a REST client
    constructor(ConstantContactRestClient::ConstantContactRestClient rest)
            : ConstantContactDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "export-contacts";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Export contacts from Constant Contact via bulk activity";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ConstantContactExportContactsRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    #! Makes the request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("activities/contact_exports", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ConstantContactDataProvider
