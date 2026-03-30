# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BitlyUpdateGroupDataProvider.qc Bitly update group action data provider

/*  BitlyUpdateGroupDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BitlyDataProvider module
public namespace BitlyDataProvider {

#! Request type for updating a group
public class BitlyUpdateGroupRequestDataType inherits HashDataType {
    public {
        #! Field definitions for updating a group
        const Fields = {
            "group_guid": {
                "type": StringType,
                "display_name": "Group GUID",
                "short_desc": "The group to update",
                "desc": "The GUID of the group to update",
                "required": True,
                "ref_data": "groups",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "New group name",
                "desc": "The new name for the group",
                "preselected": True,
            },
            "organization_guid": {
                "type": StringOrNothingType,
                "display_name": "Organization GUID",
                "short_desc": "New organization",
                "desc": "The GUID of the organization to move the group to",
                "ref_data": "organizations",
            },
            "bsds": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Branded Short Domains",
                "short_desc": "BSDs to associate",
                "desc": "A list of branded short domains to associate with the group",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Bitly update group action data provider
public class BitlyUpdateGroupDataProvider inherits BitlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing group",
            "type": "BitlyUpdateGroupDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(RestClientIo::RestClientIo rest) : BitlyDataProviderBase(rest) {
    }

    string getName() {
        return "update";
    }

    *string getDesc() {
        return "Update an existing group";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BitlyUpdateGroupRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BitlyGroupDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string group_guid = remove req.group_guid;
        return doPatch("groups/" + group_guid, req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BitlyDataProvider
