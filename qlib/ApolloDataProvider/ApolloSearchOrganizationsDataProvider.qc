# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ApolloSearchOrganizationsDataProvider.qc Apollo search organizations action data provider

/*  ApolloSearchOrganizationsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for searching organizations
public class ApolloSearchOrganizationsRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "organization_domains": {
                "type": SoftListOrNothingType,
                "display_name": "Organization Domains",
                "short_desc": "Company domains to search for",
                "desc": "List of company domains to search for",
            },
            "organization_locations": {
                "type": SoftListOrNothingType,
                "display_name": "Organization Locations",
                "short_desc": "Locations to filter by",
                "desc": "List of locations to filter by",
            },
            "organization_num_employees_ranges": {
                "type": SoftListOrNothingType,
                "display_name": "Employee Count Ranges",
                "short_desc": "Employee count ranges",
                "desc": "Employee count ranges (e.g., `1,10` or `1001,5000`)",
            },
            "q_keywords": {
                "type": StringOrNothingType,
                "display_name": "Keywords",
                "short_desc": "Keywords to search for",
                "desc": "Keywords to search for",
            },
            "page": {
                "type": SoftIntOrNothingType,
                "display_name": "Page",
                "short_desc": "Page number",
                "desc": "Page number for pagination",
            },
            "per_page": {
                "type": SoftIntOrNothingType,
                "display_name": "Per Page",
                "short_desc": "Results per page",
                "desc": "Results per page (max 100)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Apollo search organizations action data provider
public class ApolloSearchOrganizationsDataProvider inherits ApolloDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "organizations",
            "desc": "Search Apollo's organization database",
            "type": "ApolloSearchOrganizationsDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static *AbstractDataProviderType RequestType = new ApolloSearchOrganizationsRequestDataType();
    }

    constructor(ApolloRestClient::ApolloRestClient rest) : ApolloDataProviderBase(rest) {
    }

    string getName() {
        return "organizations";
    }

    *string getDesc() {
        return "Search Apollo's organization database";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ApolloSearchOrganizationsRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."list";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> result = doPost("mixed_companies/search", {} + req);
        return result.organizations ?? result;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ApolloDataProvider
