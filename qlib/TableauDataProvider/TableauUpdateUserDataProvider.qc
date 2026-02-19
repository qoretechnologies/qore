# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file TableauUpdateUserDataProvider.qc Tableau update user action data provider

/*  TableauUpdateUserDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a user
public class TableauUpdateUserRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "user_id": {
                "type": StringType,
                "display_name": "User ID",
                "short_desc": "The user ID to update",
                "desc": "The unique identifier of the user to update",
                "required": True,
            },
            "full_name": {
                "type": StringOrNothingType,
                "display_name": "Full Name",
                "short_desc": "The user's full name",
                "desc": "The new full name of the user",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "The user's email address",
                "desc": "The new email address of the user",
            },
            "site_role": {
                "type": StringOrNothingType,
                "display_name": "Site Role",
                "short_desc": "The user's site role",
                "desc": "The new site role for the user",
            },
            "auth_setting": {
                "type": StringOrNothingType,
                "display_name": "Auth Setting",
                "short_desc": "The authentication setting",
                "desc": "The new authentication setting for the user",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Tableau update user action data provider
public class TableauUpdateUserDataProvider inherits TableauDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing user in Tableau",
            "type": "TableauUpdateUserDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(TableauRestClient::TableauRestClient rest) : TableauDataProviderBase(rest) {
    }

    string getName() {
        return "update";
    }

    *string getDesc() {
        return "Update an existing user in Tableau";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new TableauUpdateUserRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new TableauUserDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string user_id = remove req.user_id;
        hash<auto> body;
        if (req.full_name) body.fullName = req.full_name;
        if (req.email) body.email = req.email;
        if (req.site_role) body.siteRole = req.site_role;
        if (req.auth_setting) body.authSetting = req.auth_setting;
        return doPut("users/" + user_id, {"user": body}).user;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace TableauDataProvider
