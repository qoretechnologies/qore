# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KeapMergeContactsDataProvider.qc Keap merge contacts action data provider

/*  KeapMergeContactsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KeapDataProvider module
public namespace KeapDataProvider {

#! Request type for merging contacts
public class KeapMergeContactsRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "contact_id": {
                "type": StringType,
                "display_name": "Contact ID",
                "short_desc": "The surviving contact",
                "desc": "The unique identifier of the surviving contact",
                "required": True,
            },
            "duplicate_contact_id": {
                "type": StringType,
                "display_name": "Duplicate Contact ID",
                "short_desc": "The contact to merge and delete",
                "desc": "The unique identifier of the contact to merge and delete",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Keap merge contacts action data provider
public class KeapMergeContactsDataProvider inherits KeapDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "merge",
            "desc": "Merge two contacts in Keap, keeping the surviving contact and deleting the duplicate",
            "type": "KeapMergeContactsDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(KeapRestClient::KeapRestClient rest) : KeapDataProviderBase(rest) {
    }

    string getName() {
        return "merge";
    }

    *string getDesc() {
        return "Merge two contacts in Keap, keeping the surviving contact and deleting the duplicate";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KeapMergeContactsRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KeapContactDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = remove req.contact_id;
        return doPost("contacts/" + id + ":merge", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KeapDataProvider
