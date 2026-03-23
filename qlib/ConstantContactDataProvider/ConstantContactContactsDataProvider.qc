# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConstantContactContactsDataProvider.qc Constant Contact contacts collection data provider

/*  ConstantContactContactsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ConstantContactDataProvider module
public namespace ConstantContactDataProvider {

#! Constant Contact contacts collection data provider
public class ConstantContactContactsDataProvider inherits ConstantContactDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "contacts",
            "desc": "Constant Contact contacts data provider for listing and searching contacts",
            "type": "ConstantContactContactsDataProvider",
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
            "status": <DataProviderOptionInfo>{
                "display_name": "Status",
                "short_desc": "Filter by contact status",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter contacts by status",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "all",
                        "display_name": "All",
                        "desc": "All contacts",
                    },
                    <AllowedValueInfo>{
                        "value": "active",
                        "display_name": "Active",
                        "desc": "Active contacts",
                    },
                    <AllowedValueInfo>{
                        "value": "deleted",
                        "display_name": "Deleted",
                        "desc": "Deleted contacts",
                    },
                    <AllowedValueInfo>{
                        "value": "not_set",
                        "display_name": "Not Set",
                        "desc": "Contacts with no status set",
                    },
                    <AllowedValueInfo>{
                        "value": "pending_confirmation",
                        "display_name": "Pending Confirmation",
                        "desc": "Contacts pending confirmation",
                    },
                    <AllowedValueInfo>{
                        "value": "temp_hold",
                        "display_name": "Temporary Hold",
                        "desc": "Contacts on temporary hold",
                    },
                    <AllowedValueInfo>{
                        "value": "unsubscribed",
                        "display_name": "Unsubscribed",
                        "desc": "Unsubscribed contacts",
                    },
                ),
            },
            "list_id": <DataProviderOptionInfo>{
                "display_name": "List ID",
                "short_desc": "Filter by contact list",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter contacts by contact list membership",
                "ref_data": "contact_lists",
            },
            "updated_after": <DataProviderOptionInfo>{
                "display_name": "Updated After",
                "short_desc": "Filter by update date",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter contacts updated after this ISO-8601 date",
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
            "create": Class::forName("ConstantContactCreateContactDataProvider"),
            "update": Class::forName("ConstantContactUpdateContactDataProvider"),
            "delete": Class::forName("ConstantContactDeleteContactDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(ConstantContactRestClient::ConstantContactRestClient rest)
            : ConstantContactDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "contacts";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Constant Contact contacts data provider for listing and searching contacts";
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
        return new ConstantContactContactDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If contact_id provided, get single record
        if (search_options.contact_id) {
            hash<auto> result = doGet("contacts/" + search_options.contact_id);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }

        # Build query params
        hash<auto> params;
        if (search_options.email) {
            params.email = search_options.email;
        }
        if (search_options.status) {
            params.status = search_options.status;
        }
        if (search_options.list_id) {
            params.lists = search_options.list_id;
        }
        if (search_options.updated_after) {
            params.updated_after = search_options.updated_after;
        }
        if (search_options.limit) {
            params.limit = search_options.limit;
        }

        list<hash<auto>> all_records = fetchAllPages("contacts", "contacts", params,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ConstantContactDataProvider
