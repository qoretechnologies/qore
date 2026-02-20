# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file TableauCreateUserDataProvider.qc Tableau create user action data provider

/*  TableauCreateUserDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a user
public class TableauCreateUserRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Username",
                "short_desc": "The username",
                "desc": "The username for the new user",
                "required": True,
                "example_value": "jsmith@example.com",
            },
            "site_role": {
                "type": StringType,
                "display_name": "Site Role",
                "short_desc": "The site role for the user",
                "desc": "The site role for the user",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Creator", "display_name": "Creator"},
                    <AllowedValueInfo>{"value": "Explorer", "display_name": "Explorer"},
                    <AllowedValueInfo>{"value": "ExplorerCanPublish", "display_name": "Explorer (Can Publish)"},
                    <AllowedValueInfo>{"value": "SiteAdministratorCreator", "display_name": "Site Administrator Creator"},
                    <AllowedValueInfo>{"value": "SiteAdministratorExplorer", "display_name": "Site Administrator Explorer"},
                    <AllowedValueInfo>{"value": "Unlicensed", "display_name": "Unlicensed"},
                    <AllowedValueInfo>{"value": "Viewer", "display_name": "Viewer"},
                ),
            },
            "auth_setting": {
                "type": StringOrNothingType,
                "display_name": "Auth Setting",
                "short_desc": "The authentication setting",
                "desc": "The authentication setting for the user",
                "example_value": "ServerDefault",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Tableau create user action data provider
public class TableauCreateUserDataProvider inherits TableauDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new user in Tableau",
            "type": "TableauCreateUserDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        #! Response type for action registration
        static TableauUserDataType ResponseType();
    }

    constructor(TableauRestClient::TableauRestClient rest) : TableauDataProviderBase(rest) {
    }

    string getName() {
        return "create";
    }

    *string getDesc() {
        return "Create a new user in Tableau";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new TableauCreateUserRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("users", {"user": {"name": req.name, "siteRole": req.site_role}
            + (req.auth_setting ? {"authSetting": req.auth_setting} : {})}).user;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace TableauDataProvider
