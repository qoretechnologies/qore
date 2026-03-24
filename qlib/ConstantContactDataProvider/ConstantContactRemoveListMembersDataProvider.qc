# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConstantContactRemoveListMembersDataProvider.qc Constant Contact remove list members action data provider

/*  ConstantContactRemoveListMembersDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for removing list members
public class ConstantContactRemoveListMembersRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "source": {
                "type": new ConstantContactBulkActivitySourceDataType(),
                "display_name": "Source",
                "short_desc": "Source of contacts",
                "desc": "The source of contacts to remove from the lists. Use one of the "
                    "mutually exclusive fields: `contact_ids`, `list_ids`, `tag_ids`, "
                    "`all_active_contacts`, or `engagement_level`",
                "required": True,
            },
            "list_ids": {
                "type": AutoListType,
                "display_name": "Target List IDs",
                "short_desc": "Target list IDs",
                "desc": "A list of contact list IDs to remove the contacts from",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Constant Contact remove list members action data provider
public class ConstantContactRemoveListMembersDataProvider inherits ConstantContactDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "remove-list-members",
            "desc": "Remove contacts from list memberships in Constant Contact via bulk activity",
            "type": "ConstantContactRemoveListMembersDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Static request type
        static ConstantContactRemoveListMembersRequestDataType RequestType();
    }

    #! Creates the object with a REST client
    constructor(RestClientIo::RestClientIo rest)
            : ConstantContactDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "remove-list-members";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Remove contacts from list memberships in Constant Contact via bulk activity";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ConstantContactRemoveListMembersRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    #! Makes the request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("activities/remove_list_memberships", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ConstantContactDataProvider
