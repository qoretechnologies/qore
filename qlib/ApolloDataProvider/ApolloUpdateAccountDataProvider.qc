# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ApolloUpdateAccountDataProvider.qc Apollo update account action data provider

/*  ApolloUpdateAccountDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for updating an account
public class ApolloUpdateAccountRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "account_id": {
                "type": StringType,
                "display_name": "Account ID",
                "short_desc": "The account ID to update",
                "desc": "The unique identifier of the account to update",
                "required": True,
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The new account name",
                "desc": "The new name of the account",
            },
            "domain": {
                "type": StringOrNothingType,
                "display_name": "Domain",
                "short_desc": "The new domain",
                "desc": "The new domain of the company",
            },
            "website_url": {
                "type": StringOrNothingType,
                "display_name": "Website URL",
                "short_desc": "The new website URL",
                "desc": "The new website URL of the company",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone",
                "short_desc": "The new phone number",
                "desc": "The new phone number of the company",
            },
            "owner_id": {
                "type": StringOrNothingType,
                "display_name": "Owner ID",
                "short_desc": "The new owner user ID",
                "desc": "The new owner user ID for the account",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Apollo update account action data provider
public class ApolloUpdateAccountDataProvider inherits ApolloDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing account in Apollo",
            "type": "ApolloUpdateAccountDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static *AbstractDataProviderType ResponseType = new ApolloAccountDataType();
        static *AbstractDataProviderType RequestType = new ApolloUpdateAccountRequestDataType();
    }

    constructor(ApolloRestClient::ApolloRestClient rest) : ApolloDataProviderBase(rest) {
    }

    string getName() {
        return "update";
    }

    *string getDesc() {
        return "Update an existing account in Apollo";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ApolloUpdateAccountRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new ApolloAccountDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {} + req;
        string id = remove body.account_id;
        hash<auto> result = doPatch("accounts/" + id, body);
        return result.account ?? result;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ApolloDataProvider
