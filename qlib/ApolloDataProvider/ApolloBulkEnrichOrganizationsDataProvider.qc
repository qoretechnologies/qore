# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ApolloBulkEnrichOrganizationsDataProvider.qc Apollo bulk enrich organizations action data provider

/*  ApolloBulkEnrichOrganizationsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for bulk enriching organizations
public class ApolloBulkEnrichOrganizationsRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "domains": {
                "type": AutoListType,
                "display_name": "Domains",
                "short_desc": "List of organization domains",
                "desc": "List of organization domains to enrich",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Apollo bulk enrich organizations action data provider
public class ApolloBulkEnrichOrganizationsDataProvider inherits ApolloDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "bulk-organizations",
            "desc": "Bulk enrich organizations using Apollo",
            "type": "ApolloBulkEnrichOrganizationsDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static *AbstractDataProviderType RequestType = new ApolloBulkEnrichOrganizationsRequestDataType();
    }

    constructor(ApolloRestClient::ApolloRestClient rest) : ApolloDataProviderBase(rest) {
    }

    string getName() {
        return "bulk-organizations";
    }

    *string getDesc() {
        return "Bulk enrich organizations using Apollo";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ApolloBulkEnrichOrganizationsRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."list";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> result = doPost("organizations/bulk_enrich", {} + req);
        return result.organizations ?? result;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ApolloDataProvider
