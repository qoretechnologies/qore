# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FreshsalesContactsDataProvider.qc Freshsales contacts collection data provider

/*  FreshsalesContactsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FreshsalesDataProvider module
public namespace FreshsalesDataProvider {

#! Freshsales contacts collection data provider
public class FreshsalesContactsDataProvider inherits FreshsalesDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "contacts",
            "desc": "Freshsales contacts data provider for listing and searching contacts",
            "type": "FreshsalesContactsDataProvider",
            "has_record": True,
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
            "id": <DataProviderOptionInfo>{
                "display_name": "Contact ID",
                "short_desc": "Get a specific contact by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "The contact ID to retrieve. When provided, returns a single record",
            },
            "view_id": <DataProviderOptionInfo>{
                "display_name": "View ID",
                "short_desc": "Filter by view",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "The ID of the view to list contacts from. Views are predefined "
                    "filters in Freshsales",
                "preselected": True,
            },
            "email": <DataProviderOptionInfo>{
                "display_name": "Email",
                "short_desc": "Search by email address",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Search for contacts by email address",
                "preselected": True,
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
            "create": Class::forName("FreshsalesCreateContactDataProvider"),
            "update": Class::forName("FreshsalesUpdateContactDataProvider"),
            "delete": Class::forName("FreshsalesDeleteContactDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(RestClientIo::RestClientIo rest) : FreshsalesDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "contacts";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Freshsales contacts data provider for listing and searching contacts";
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
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new FreshsalesContactDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If ID provided, get single record
        if (search_options.id) {
            hash<auto> result = doGet("contacts/" + string(search_options.id));
            # Freshsales wraps response in a "contact" key
            hash<auto> contact = result.contact ?? result;
            return new DefaultRecordIterator(new ListHashIterator((contact,)),
                where_cond, search_options);
        }

        # Search by email using lookup endpoint
        if (search_options.email) {
            hash<auto> result = doGet("lookup", {"q": search_options.email,
                "f": "email", "entities": "contact"});
            list<hash<auto>> contacts = result.contacts.contacts ?? ();
            if (search_options.limit && contacts.size() > search_options.limit) {
                contacts = contacts[0..search_options.limit-1];
            }
            return new DefaultRecordIterator(new ListHashIterator(contacts),
                where_cond, search_options);
        }

        # List contacts from a view
        if (search_options.view_id) {
            list<hash<auto>> all_records = fetchAllPages(
                "contacts/view/" + string(search_options.view_id), "contacts",
                NOTHING, search_options.limit);
            return new DefaultRecordIterator(new ListHashIterator(all_records),
                where_cond, search_options);
        }

        # Default: list all contacts (requires a view; use filter views endpoint)
        hash<auto> filters = doGet("contacts/filters");
        *list<hash<auto>> filter_list = filters.filters;
        if (filter_list) {
            # Use the first available view
            list<hash<auto>> all_records = fetchAllPages(
                "contacts/view/" + string(filter_list[0].id), "contacts",
                NOTHING, search_options.limit);
            return new DefaultRecordIterator(new ListHashIterator(all_records),
                where_cond, search_options);
        }

        return new DefaultRecordIterator(new ListHashIterator(()),
            where_cond, search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace FreshsalesDataProvider
