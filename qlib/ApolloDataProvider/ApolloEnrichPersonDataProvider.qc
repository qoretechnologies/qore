# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ApolloEnrichPersonDataProvider.qc Apollo enrich person action data provider

/*  ApolloEnrichPersonDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for enriching a person
public class ApolloEnrichPersonRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "The person's email address",
                "desc": "The person's email address for enrichment",
                "preselected": True,
                "example_value": "john@example.com",
            },
            "first_name": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "The person's first name",
                "desc": "The person's first name",
            },
            "last_name": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "The person's last name",
                "desc": "The person's last name",
            },
            "organization_name": {
                "type": StringOrNothingType,
                "display_name": "Organization Name",
                "short_desc": "The person's company name",
                "desc": "The person's company name",
            },
            "domain": {
                "type": StringOrNothingType,
                "display_name": "Domain",
                "short_desc": "The person's company domain",
                "desc": "The person's company domain",
            },
            "linkedin_url": {
                "type": StringOrNothingType,
                "display_name": "LinkedIn URL",
                "short_desc": "The person's LinkedIn URL",
                "desc": "The person's LinkedIn URL",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Apollo enrich person action data provider
public class ApolloEnrichPersonDataProvider inherits ApolloDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "person",
            "desc": "Enrich a person's data using Apollo",
            "type": "ApolloEnrichPersonDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static *AbstractDataProviderType ResponseType = new ApolloPersonEnrichmentDataType();
        static *AbstractDataProviderType RequestType = new ApolloEnrichPersonRequestDataType();
    }

    constructor(ApolloRestClient::ApolloRestClient rest) : ApolloDataProviderBase(rest) {
    }

    string getName() {
        return "person";
    }

    *string getDesc() {
        return "Enrich a person's data using Apollo";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ApolloEnrichPersonRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new ApolloPersonEnrichmentDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> result = doPost("people/match", {} + req);
        return result.person ?? result;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ApolloDataProvider
