# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConstantContactAddListMembersDataProvider.qc Constant Contact add list members action data provider

/*  ConstantContactAddListMembersDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for adding list members
public class ConstantContactAddListMembersRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "source": {
                "type": AutoHashType,
                "display_name": "Source",
                "short_desc": "Source of contacts",
                "desc": "The source of contacts to add to the lists. Can contain `contact_ids`, "
                    "`list_ids`, `tag_ids`, `segment_id`, or `all_active_contacts`",
                "required": True,
            },
            "list_ids": {
                "type": AutoListType,
                "display_name": "Target List IDs",
                "short_desc": "Target list IDs",
                "desc": "A list of contact list IDs to add the contacts to",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Constant Contact add list members action data provider
public class ConstantContactAddListMembersDataProvider inherits ConstantContactDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "add-list-members",
            "desc": "Add contacts to list memberships in Constant Contact via bulk activity",
            "type": "ConstantContactAddListMembersDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Static request type
        static ConstantContactAddListMembersRequestDataType RequestType();
    }

    #! Creates the object with a REST client
    constructor(ConstantContactRestClient::ConstantContactRestClient rest)
            : ConstantContactDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "add-list-members";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Add contacts to list memberships in Constant Contact via bulk activity";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ConstantContactAddListMembersRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    #! Makes the request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("activities/add_list_memberships", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ConstantContactDataProvider
