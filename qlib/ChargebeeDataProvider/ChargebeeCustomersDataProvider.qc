# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ChargebeeDataProvider module definition

/*  ChargebeeCustomersDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ChargebeeDataProvider module
public namespace ChargebeeDataProvider {

#! Chargebee customers collection data provider
public class ChargebeeCustomersDataProvider inherits ChargebeeDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "customers",
            "desc": "Chargebee customers data provider; list and search customers",
            "type": "ChargebeeCustomersDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "id": <DataProviderOptionInfo>{
                "display_name": "Customer ID",
                "short_desc": "Get a specific customer by ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter by customer ID for single record lookup",
            },
            "status": <DataProviderOptionInfo>{
                "display_name": "Status",
                "short_desc": "Filter by customer status",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter customers by status",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "active", "desc": "Active customer"},
                ),
            },
            "email": <DataProviderOptionInfo>{
                "display_name": "Email",
                "short_desc": "Filter by email address",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter customers by exact email address (email[is])",
            },
            "first_name": <DataProviderOptionInfo>{
                "display_name": "First Name",
                "short_desc": "Filter by first name",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter customers by exact first name (first_name[is])",
            },
            "last_name": <DataProviderOptionInfo>{
                "display_name": "Last Name",
                "short_desc": "Filter by last name",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter customers by exact last name (last_name[is])",
            },
            "company": <DataProviderOptionInfo>{
                "display_name": "Company",
                "short_desc": "Filter by company name",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter customers by exact company name (company[is])",
            },
            "auto_collection": <DataProviderOptionInfo>{
                "display_name": "Auto Collection",
                "short_desc": "Filter by auto-collection setting",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter customers by auto-collection setting",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "on", "desc": "Auto-collection enabled"},
                    <AllowedValueInfo>{"value": "off", "desc": "Auto-collection disabled"},
                ),
            },
            "created_at_from": <DataProviderOptionInfo>{
                "display_name": "Created At From",
                "short_desc": "Filter by creation date (after)",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Filter customers created at or after this Unix timestamp (created_at[after])",
            },
            "created_at_to": <DataProviderOptionInfo>{
                "display_name": "Created At To",
                "short_desc": "Filter by creation date (before)",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Filter customers created at or before this Unix timestamp (created_at[before])",
            },
            "updated_at_from": <DataProviderOptionInfo>{
                "display_name": "Updated At From",
                "short_desc": "Filter by update date (after)",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Filter customers updated at or after this Unix timestamp (updated_at[after])",
            },
            "updated_at_to": <DataProviderOptionInfo>{
                "display_name": "Updated At To",
                "short_desc": "Filter by update date (before)",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Filter customers updated at or before this Unix timestamp (updated_at[before])",
            },
            "sort_by": <DataProviderOptionInfo>{
                "display_name": "Sort By",
                "short_desc": "Sort field and direction",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Sort results by field and direction (e.g., sort_by[asc] or sort_by[desc])",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "created_at[asc]", "desc": "Sort by creation date ascending"},
                    <AllowedValueInfo>{"value": "created_at[desc]", "desc": "Sort by creation date descending"},
                    <AllowedValueInfo>{"value": "updated_at[asc]", "desc": "Sort by update date ascending"},
                    <AllowedValueInfo>{"value": "updated_at[desc]", "desc": "Sort by update date descending"},
                ),
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results per page",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Maximum number of results to return (1-100, default 100)",
            },
            "offset": <DataProviderOptionInfo>{
                "display_name": "Offset",
                "short_desc": "Pagination cursor",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Cursor string from previous response's next_offset for pagination",
            },
        };
    }

    private {
        #! Record type
        static ChargebeeCustomerDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(ChargebeeRestClient::ChargebeeRestClient rest)
            : ChargebeeDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "customers";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Chargebee customers data provider; list and search customers";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for customers
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If id provided, get single record
        if (search_options.id) {
            hash<auto> response = doGet("/customers/" + search_options.id);
            return new DefaultRecordIterator(new ListHashIterator((response.customer,)),
                where_cond, search_options, recordType.getFields());
        }

        # Build query parameters from search options with Chargebee filter notation
        hash<auto> params;
        if (search_options.status) {
            params."status[is]" = search_options.status;
        }
        if (search_options.email) {
            params."email[is]" = search_options.email;
        }
        if (search_options.first_name) {
            params."first_name[is]" = search_options.first_name;
        }
        if (search_options.last_name) {
            params."last_name[is]" = search_options.last_name;
        }
        if (search_options.company) {
            params."company[is]" = search_options.company;
        }
        if (search_options.auto_collection) {
            params."auto_collection[is]" = search_options.auto_collection;
        }
        if (search_options.created_at_from) {
            params."created_at[after]" = search_options.created_at_from;
        }
        if (search_options.created_at_to) {
            params."created_at[before]" = search_options.created_at_to;
        }
        if (search_options.updated_at_from) {
            params."updated_at[after]" = search_options.updated_at_from;
        }
        if (search_options.updated_at_to) {
            params."updated_at[before]" = search_options.updated_at_to;
        }
        if (search_options.sort_by) {
            params.sort_by = search_options.sort_by;
        }
        if (search_options.limit) {
            params.limit = search_options.limit;
        }
        if (search_options.offset) {
            params.offset = search_options.offset;
        }

        hash<auto> response = doGet("/customers", params);
        list<hash<auto>> records = map $1.customer, response.list, $1.customer;
        if (!records) {
            records = ();
        }

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ChargebeeDataProvider
