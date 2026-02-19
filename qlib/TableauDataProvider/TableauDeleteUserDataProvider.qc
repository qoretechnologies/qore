# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file TableauDeleteUserDataProvider.qc Tableau delete user action data provider

/*  TableauDeleteUserDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the TableauDataProvider module
public namespace TableauDataProvider {

#! Request type for deleting a user
public class TableauDeleteUserRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "user_id": {
                "type": StringType,
                "display_name": "User ID",
                "short_desc": "The user to delete",
                "desc": "The unique identifier of the user to delete",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Tableau delete user action data provider
public class TableauDeleteUserDataProvider inherits TableauDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete",
            "desc": "Delete a user in Tableau",
            "type": "TableauDeleteUserDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(TableauRestClient::TableauRestClient rest) : TableauDataProviderBase(rest) {
    }

    string getName() {
        return "delete";
    }

    *string getDesc() {
        return "Delete a user in Tableau";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new TableauDeleteUserRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doDelete("users/" + req.user_id);
        return {};
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace TableauDataProvider
