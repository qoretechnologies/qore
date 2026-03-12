# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ApolloCreateAccountDataProvider.qc Apollo create account action data provider

/*  ApolloCreateAccountDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ApolloDataProvider module
public namespace ApolloDataProvider {

#! Request type for creating an account
public class ApolloCreateAccountRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The account name",
                "desc": "The name of the account",
                "example_value": "Acme Corp",
                "required": True,
            },
            "domain": {
                "type": StringOrNothingType,
                "display_name": "Domain",
                "short_desc": "The company domain",
                "desc": "The domain of the company",
                "preselected": True,
            },
            "website_url": {
                "type": StringOrNothingType,
                "display_name": "Website URL",
                "short_desc": "The company website URL",
                "desc": "The website URL of the company",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone",
                "short_desc": "The company phone number",
                "desc": "The phone number of the company",
            },
            "owner_id": {
                "type": StringOrNothingType,
                "display_name": "Owner ID",
                "short_desc": "The owner user ID",
                "desc": "The ID of the user who owns this account",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Apollo create account action data provider
public class ApolloCreateAccountDataProvider inherits ApolloDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new account in Apollo",
            "type": "ApolloCreateAccountDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static *AbstractDataProviderType ResponseType = new ApolloAccountDataType();
        static *AbstractDataProviderType RequestType = new ApolloCreateAccountRequestDataType();
    }

    constructor(ApolloRestClient::ApolloRestClient rest) : ApolloDataProviderBase(rest) {
    }

    string getName() {
        return "create";
    }

    *string getDesc() {
        return "Create a new account in Apollo";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ApolloCreateAccountRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new ApolloAccountDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> result = doPost("accounts", {} + req);
        return result.account ?? result;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ApolloDataProvider
