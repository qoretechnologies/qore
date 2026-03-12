# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ApolloAccountsDataProvider.qc Apollo accounts collection data provider

/*  ApolloAccountsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Apollo accounts collection data provider
public class ApolloAccountsDataProvider inherits ApolloDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "accounts",
            "desc": "Apollo accounts data provider for listing and searching accounts",
            "type": "ApolloAccountsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "account_id": <DataProviderOptionInfo>{
                "display_name": "Account ID",
                "short_desc": "Get a specific account by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by account ID for single record lookup",
            },
            "name": <DataProviderOptionInfo>{
                "display_name": "Name",
                "short_desc": "Filter by account name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter accounts by name",
            },
            "domain": <DataProviderOptionInfo>{
                "display_name": "Domain",
                "short_desc": "Filter by domain",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter accounts by domain",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };

        #! Child provider map
        const ChildMap = {
            "create": Class::forName("ApolloCreateAccountDataProvider"),
            "update": Class::forName("ApolloUpdateAccountDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(ApolloRestClient::ApolloRestClient rest) : ApolloDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "accounts";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Apollo accounts data provider for listing and searching accounts";
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    #! Returns the record type
    private *AbstractDataProviderType getRecordTypeImpl() {
        return new ApolloAccountDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If account_id provided, get single record
        if (search_options.account_id) {
            hash<auto> result = doGet("accounts/" + search_options.account_id);
            hash<auto> account = result.account ?? result;
            return new DefaultRecordIterator(new ListHashIterator((account,)),
                where_cond, search_options);
        }

        # Build search body for POST /accounts/search
        hash<auto> body;
        if (search_options.name) {
            body.q_keywords = search_options.name;
        }
        if (search_options.domain) {
            body.domain = search_options.domain;
        }

        list<hash<auto>> records = fetchAllPagesPost("accounts/search", "accounts", body,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ApolloDataProvider
