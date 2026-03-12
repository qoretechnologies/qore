# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ApolloEnrichOrganizationDataProvider.qc Apollo enrich organization action data provider

/*  ApolloEnrichOrganizationDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for enriching an organization
public class ApolloEnrichOrganizationRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "domain": {
                "type": StringType,
                "display_name": "Domain",
                "short_desc": "The organization domain",
                "desc": "The organization domain to enrich",
                "required": True,
                "preselected": True,
                "example_value": "apollo.io",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Apollo enrich organization action data provider
public class ApolloEnrichOrganizationDataProvider inherits ApolloDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "organization",
            "desc": "Enrich an organization's data using Apollo",
            "type": "ApolloEnrichOrganizationDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static *AbstractDataProviderType ResponseType = new ApolloOrganizationEnrichmentDataType();
        static *AbstractDataProviderType RequestType = new ApolloEnrichOrganizationRequestDataType();
    }

    constructor(ApolloRestClient::ApolloRestClient rest) : ApolloDataProviderBase(rest) {
    }

    string getName() {
        return "organization";
    }

    *string getDesc() {
        return "Enrich an organization's data using Apollo";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ApolloEnrichOrganizationRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new ApolloOrganizationEnrichmentDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> result = doGet("organizations/enrich", {"domain": req.domain});
        return result.organization ?? result;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ApolloDataProvider
