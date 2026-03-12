# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ApolloBulkEnrichPeopleDataProvider.qc Apollo bulk enrich people action data provider

/*  ApolloBulkEnrichPeopleDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for bulk enriching people
public class ApolloBulkEnrichPeopleRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "details": {
                "type": AutoListType,
                "display_name": "Details",
                "short_desc": "List of person detail objects",
                "desc": "List of person detail objects, each containing fields like `email`, `first_name`, "
                    "`last_name`, `organization_name`, `domain`",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Apollo bulk enrich people action data provider
public class ApolloBulkEnrichPeopleDataProvider inherits ApolloDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "bulk-people",
            "desc": "Bulk enrich people data using Apollo",
            "type": "ApolloBulkEnrichPeopleDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static *AbstractDataProviderType RequestType = new ApolloBulkEnrichPeopleRequestDataType();
    }

    constructor(ApolloRestClient::ApolloRestClient rest) : ApolloDataProviderBase(rest) {
    }

    string getName() {
        return "bulk-people";
    }

    *string getDesc() {
        return "Bulk enrich people data using Apollo";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ApolloBulkEnrichPeopleRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."list";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> result = doPost("people/bulk_match", {} + req);
        return result.matches ?? result;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ApolloDataProvider
