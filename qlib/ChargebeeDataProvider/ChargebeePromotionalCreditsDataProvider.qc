# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ChargebeeDataProvider module definition

/*  ChargebeePromotionalCreditsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Chargebee promotional credits collection data provider
public class ChargebeePromotionalCreditsDataProvider inherits ChargebeeDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "promotional-credits",
            "desc": "Chargebee promotional credits data provider; list and search promotional credits",
            "type": "ChargebeePromotionalCreditsDataProvider",
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
                "display_name": "Promotional Credit ID",
                "short_desc": "Get a specific promotional credit by ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter by promotional credit ID for single record lookup",
            },
            "customer_id": <DataProviderOptionInfo>{
                "display_name": "Customer ID",
                "short_desc": "Filter by customer ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter promotional credits by customer ID (customer_id[is])",
            },
            "type": <DataProviderOptionInfo>{
                "display_name": "Type",
                "short_desc": "Filter by promotional credit type",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter promotional credits by type (type[is])",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "increment", "desc": "Credit increment"},
                    <AllowedValueInfo>{"value": "decrement", "desc": "Credit decrement"},
                    <AllowedValueInfo>{"value": "set", "desc": "Credit set"},
                ),
            },
            "created_at_from": <DataProviderOptionInfo>{
                "display_name": "Created At From",
                "short_desc": "Filter by creation date (after)",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Filter promotional credits created at or after this Unix timestamp "
                    "(created_at[after])",
            },
            "created_at_to": <DataProviderOptionInfo>{
                "display_name": "Created At To",
                "short_desc": "Filter by creation date (before)",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Filter promotional credits created at or before this Unix timestamp "
                    "(created_at[before])",
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
        static ChargebeePromotionalCreditDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(ChargebeeRestClient::ChargebeeRestClient rest)
            : ChargebeeDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "promotional-credits";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Chargebee promotional credits data provider; list and search promotional credits";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for promotional credits
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If id provided, get single record
        if (search_options.id) {
            hash<auto> response = doGet("/promotional_credits/" + search_options.id);
            return new DefaultRecordIterator(new ListHashIterator((response.promotional_credit,)),
                where_cond, search_options, recordType.getFields());
        }

        # Build query parameters from search options with Chargebee filter notation
        hash<auto> params;
        if (search_options.customer_id) {
            params."customer_id[is]" = search_options.customer_id;
        }
        if (search_options.type) {
            params."type[is]" = search_options.type;
        }
        if (search_options.created_at_from) {
            params."created_at[after]" = search_options.created_at_from;
        }
        if (search_options.created_at_to) {
            params."created_at[before]" = search_options.created_at_to;
        }
        if (search_options.limit) {
            params.limit = search_options.limit;
        }
        if (search_options.offset) {
            params.offset = search_options.offset;
        }

        hash<auto> response = doGet("/promotional_credits", params);
        list<hash<auto>> records = map $1.promotional_credit, response.list, $1.promotional_credit;
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
