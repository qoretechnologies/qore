# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ApolloSearchPeopleDataProvider.qc Apollo search people action data provider

/*  ApolloSearchPeopleDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for searching people
public class ApolloSearchPeopleRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "person_titles": {
                "type": SoftListOrNothingType,
                "display_name": "Person Titles",
                "short_desc": "Job titles to search for",
                "desc": "List of job titles to search for",
            },
            "person_locations": {
                "type": SoftListOrNothingType,
                "display_name": "Person Locations",
                "short_desc": "Locations to filter by",
                "desc": "List of locations to filter by",
            },
            "person_seniorities": {
                "type": SoftListOrNothingType,
                "display_name": "Person Seniorities",
                "short_desc": "Seniority levels to filter by",
                "desc": "List of seniority levels (e.g., `senior`, `manager`)",
            },
            "q_keywords": {
                "type": StringOrNothingType,
                "display_name": "Keywords",
                "short_desc": "Keywords to search for",
                "desc": "Keywords to search for",
            },
            "organization_domains": {
                "type": SoftListOrNothingType,
                "display_name": "Organization Domains",
                "short_desc": "Company domains to search within",
                "desc": "List of company domains to search within",
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

#! Apollo search people action data provider
public class ApolloSearchPeopleDataProvider inherits ApolloDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "people",
            "desc": "Search Apollo's people database",
            "type": "ApolloSearchPeopleDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static *AbstractDataProviderType RequestType = new ApolloSearchPeopleRequestDataType();
    }

    constructor(ApolloRestClient::ApolloRestClient rest) : ApolloDataProviderBase(rest) {
    }

    string getName() {
        return "people";
    }

    *string getDesc() {
        return "Search Apollo's people database";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ApolloSearchPeopleRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."list";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> result = doPost("mixed_people/search", {} + req);
        return result.people ?? result;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ApolloDataProvider
