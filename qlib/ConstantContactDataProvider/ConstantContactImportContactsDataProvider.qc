# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConstantContactImportContactsDataProvider.qc Constant Contact import contacts action data provider

/*  ConstantContactImportContactsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for importing contacts
public class ConstantContactImportContactsRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "import_data": {
                "type": AutoListType,
                "display_name": "Import Data",
                "short_desc": "List of contact data hashes",
                "desc": "A list of contact data hashes to import",
                "required": True,
            },
            "list_ids": {
                "type": AutoListOrNothingType,
                "display_name": "List IDs",
                "short_desc": "List IDs to add imported contacts to",
                "desc": "A list of contact list IDs to add the imported contacts to",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Constant Contact import contacts action data provider
public class ConstantContactImportContactsDataProvider inherits ConstantContactDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "import-contacts",
            "desc": "Import contacts into Constant Contact via bulk activity",
            "type": "ConstantContactImportContactsDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Static request type
        static ConstantContactImportContactsRequestDataType RequestType();
    }

    #! Creates the object with a REST client
    constructor(RestClientIo::RestClientIo rest)
            : ConstantContactDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "import-contacts";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Import contacts into Constant Contact via bulk activity";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ConstantContactImportContactsRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    #! Makes the request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("activities/contacts_json_import", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ConstantContactDataProvider
