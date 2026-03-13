# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyBulkAssignLeadsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for bulk assigning leads
public class InstantlyBulkAssignLeadsRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "lead_ids": {
                "type": SoftAutoListType,
                "display_name": "Lead IDs",
                "short_desc": "List of lead UUIDs to assign",
                "desc": "The list of lead UUIDs to assign",
                "required": True,
            },
            "user_ids": {
                "type": SoftAutoListType,
                "display_name": "User IDs",
                "short_desc": "List of user UUIDs to assign to",
                "desc": "The list of user UUIDs to assign the leads to",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Instantly bulk assign leads action data provider
public class InstantlyBulkAssignLeadsDataProvider inherits InstantlyDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "bulk-assign-leads",
            "desc": "Bulk assign leads to users in Instantly",
            "type": "InstantlyBulkAssignLeadsDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static InstantlyBulkAssignLeadsRequestDataType RequestType();

        #! Response type
        static HashDataType ResponseType("BulkAssignLeadsResponse");
    }

    #! Creates the object with a REST client
    constructor(InstantlyRestClient::InstantlyRestClient rest) : InstantlyDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "bulk-assign-leads";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Bulk assign leads to users in Instantly";
    }

    #! Makes the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("/leads/bulk-assign", req);
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
