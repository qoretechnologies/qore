# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelAdjustmentsDataProvider.qc Deel invoice adjustments collection data provider

/*  DeelAdjustmentsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DeelDataProvider module
public namespace DeelDataProvider {

#! Deel invoice adjustments collection data provider
public class DeelAdjustmentsDataProvider inherits DeelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "adjustments",
            "desc": "Deel invoice adjustments data provider for listing and searching adjustments",
            "type": "DeelAdjustmentsDataProvider",
            "supports_read": True,
            "has_record": True,
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
            "adjustment_id": <DataProviderOptionInfo>{
                "display_name": "Adjustment ID",
                "short_desc": "Get a specific adjustment by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by adjustment ID for single record lookup",
            },
            "contract_id": <DataProviderOptionInfo>{
                "display_name": "Contract ID",
                "short_desc": "Filter by contract",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter adjustments by associated contract ID",
            },
            "status": <DataProviderOptionInfo>{
                "display_name": "Status",
                "short_desc": "Filter by adjustment status",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter adjustments by status (e.g., `pending`, `approved`, `declined`)",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "pending", "desc": "Pending adjustments"},
                    <AllowedValueInfo>{"value": "approved", "desc": "Approved adjustments"},
                    <AllowedValueInfo>{"value": "declined", "desc": "Declined adjustments"},
                ),
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
            "offset": <DataProviderOptionInfo>{
                "display_name": "Offset",
                "short_desc": "Results offset",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Number of results to skip (for manual pagination)",
            },
        };

        #! Child provider map
        const ChildMap = {
            "create": Class::forName("DeelCreateAdjustmentDataProvider"),
            "delete": Class::forName("DeelDeleteAdjustmentDataProvider"),
            "review": Class::forName("DeelReviewAdjustmentDataProvider"),
            "update": Class::forName("DeelUpdateAdjustmentDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(DeelRestClient::DeelRestClient rest) : DeelDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "adjustments";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Deel invoice adjustments data provider for listing and searching adjustments";
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
        return new DeelAdjustmentDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If adjustment_id provided, get single record
        if (search_options.adjustment_id) {
            hash<auto> result = doGet("invoice-adjustments/" + encode_url(search_options.adjustment_id, True));
            list<hash<auto>> data = result.data ? (result.data,) : ();
            return new DefaultRecordIterator(new ListHashIterator(data),
                where_cond, search_options);
        }

        # Build query params for listing
        hash<auto> params;
        if (search_options.contract_id) {
            params.contractId = search_options.contract_id;
        }
        if (search_options.status) {
            params.status = search_options.status;
        }

        list<hash<auto>> all_records = fetchAllPages("invoice-adjustments", "adjustments", params,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DeelDataProvider
