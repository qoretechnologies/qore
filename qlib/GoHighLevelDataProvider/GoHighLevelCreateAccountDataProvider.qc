# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GoHighLevelCreateAccountDataProvider.qc GoHighLevel create account action data provider

/*  GoHighLevelCreateAccountDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

%modern

#! Contains all public definitions in the GoHighLevelDataProvider module
public namespace GoHighLevelDataProvider {

#! Request data type for the create account action
public class GoHighLevelCreateAccountRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The account/location name",
                "desc": "The account/location name",
                "required": True,
            },
            "email": {
                "type": StringType,
                "display_name": "Email",
                "short_desc": "The account email address",
                "desc": "The account email address",
                "required": True,
                "example_value": "admin@example.com",
            },
            "phone": {
                "type": StringType,
                "display_name": "Phone",
                "short_desc": "The account phone number",
                "desc": "The account phone number",
                "required": True,
            },
            "address": {
                "type": StringOrNothingType,
                "display_name": "Address",
                "short_desc": "The street address",
                "desc": "The street address",
            },
            "city": {
                "type": StringOrNothingType,
                "display_name": "City",
                "short_desc": "The city",
                "desc": "The city",
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "The state or province",
                "desc": "The state or province",
            },
            "postalCode": {
                "type": StringOrNothingType,
                "display_name": "Postal Code",
                "short_desc": "The postal/zip code",
                "desc": "The postal/zip code",
            },
            "country": {
                "type": StringOrNothingType,
                "display_name": "Country",
                "short_desc": "2-letter country code",
                "desc": "2-letter country code",
            },
            "timezone": {
                "type": StringOrNothingType,
                "display_name": "Timezone",
                "short_desc": "Timezone identifier",
                "desc": "Timezone identifier",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! GoHighLevel create account action data provider
public class GoHighLevelCreateAccountDataProvider inherits GoHighLevelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-account",
            "desc": "Create a new location/sub-account in GoHighLevel",
            "type": "GoHighLevelCreateAccountDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(GoHighLevelRestClient::GoHighLevelRestClient rest) : GoHighLevelDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "create-account";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new location/sub-account in GoHighLevel";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new GoHighLevelCreateAccountRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("locations", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace GoHighLevelDataProvider
