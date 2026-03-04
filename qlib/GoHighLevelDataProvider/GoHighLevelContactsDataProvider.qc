# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GoHighLevelContactsDataProvider.qc GoHighLevel contacts collection data provider

/*  GoHighLevelContactsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! GoHighLevel contacts collection data provider
public class GoHighLevelContactsDataProvider inherits GoHighLevelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "contacts",
            "desc": "GoHighLevel contacts data provider for listing and searching contacts",
            "type": "GoHighLevelContactsDataProvider",
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
                "desc": "Get a specific contact by ID",
            },
            "query": <DataProviderOptionInfo>{
                "display_name": "Search Query",
                "short_desc": "Search contacts by name, email, phone, or other fields",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Search contacts by name, email, phone, or other fields",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of results to return",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };

        #! Child provider map
        const ChildMap = {
            "create": Class::forName("GoHighLevelCreateContactDataProvider"),
            "update": Class::forName("GoHighLevelUpdateContactDataProvider"),
            "delete": Class::forName("GoHighLevelDeleteContactDataProvider"),
            "add-to-campaign": Class::forName("GoHighLevelAddContactToCampaignDataProvider"),
            "remove-from-campaign": Class::forName("GoHighLevelRemoveContactFromCampaignDataProvider"),
            "add-note": Class::forName("GoHighLevelAddNoteDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(GoHighLevelRestClient::GoHighLevelRestClient rest) : GoHighLevelDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "contacts";
    }

    #! Returns the provider description
    *string getDesc() {
        return "GoHighLevel contacts data provider for listing and searching contacts";
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
        return new GoHighLevelContactDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If contact_id provided, get single record
        if (search_options.contact_id) {
            hash<auto> result = doGet("contacts/" + search_options.contact_id);
            return new DefaultRecordIterator(new ListHashIterator((result.contact,)),
                where_cond, search_options);
        }

        # Search contacts via POST
        hash<auto> body = {
            "query": search_options.query ?? "",
            "limit": search_options.limit ?? DefaultPageSize,
        };
        hash<auto> response = doPost("contacts/search", body);
        return new DefaultRecordIterator(new ListHashIterator(response.contacts ?? ()),
            where_cond, search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace GoHighLevelDataProvider
