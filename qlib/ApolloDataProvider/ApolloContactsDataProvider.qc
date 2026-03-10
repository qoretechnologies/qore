# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ApolloContactsDataProvider.qc Apollo contacts collection data provider

/*  ApolloContactsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Apollo contacts collection data provider
public class ApolloContactsDataProvider inherits ApolloDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "contacts",
            "desc": "Apollo contacts data provider for listing and searching contacts",
            "type": "ApolloContactsDataProvider",
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
            "contact_id": <DataProviderOptionInfo>{
                "display_name": "Contact ID",
                "short_desc": "Get a specific contact by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by contact ID for single record lookup",
            },
            "email": <DataProviderOptionInfo>{
                "display_name": "Email",
                "short_desc": "Filter by email address",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter contacts by email address",
            },
            "first_name": <DataProviderOptionInfo>{
                "display_name": "First Name",
                "short_desc": "Filter by first name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter contacts by first name",
            },
            "last_name": <DataProviderOptionInfo>{
                "display_name": "Last Name",
                "short_desc": "Filter by last name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter contacts by last name",
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
            "create": Class::forName("ApolloCreateContactDataProvider"),
            "update": Class::forName("ApolloUpdateContactDataProvider"),
            "delete": Class::forName("ApolloDeleteContactDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(ApolloRestClient::ApolloRestClient rest) : ApolloDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "contacts";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Apollo contacts data provider for listing and searching contacts";
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
        return new ApolloContactDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If contact_id provided, get single record
        if (search_options.contact_id) {
            hash<auto> result = doGet("contacts/" + search_options.contact_id);
            hash<auto> contact = result.contact ?? result;
            return new DefaultRecordIterator(new ListHashIterator((contact,)),
                where_cond, search_options);
        }

        # Build search body for POST /contacts/search
        hash<auto> body;
        if (search_options.email) {
            body.q_keywords = search_options.email;
        }
        if (search_options.first_name) {
            body.first_name = search_options.first_name;
        }
        if (search_options.last_name) {
            body.last_name = search_options.last_name;
        }

        list<hash<auto>> records = fetchAllPagesPost("contacts/search", "contacts", body,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ApolloDataProvider
