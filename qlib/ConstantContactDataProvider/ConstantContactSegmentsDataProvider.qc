# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConstantContactSegmentsDataProvider.qc Constant Contact segments collection data provider

/*  ConstantContactSegmentsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Constant Contact segments collection data provider
public class ConstantContactSegmentsDataProvider inherits ConstantContactDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "segments",
            "desc": "Constant Contact segments data provider for listing and searching segments",
            "type": "ConstantContactSegmentsDataProvider",
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
            "segment_id": <DataProviderOptionInfo>{
                "display_name": "Segment ID",
                "short_desc": "Get a specific segment by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Filter by segment ID for single record lookup",
            },
            "sort_by": <DataProviderOptionInfo>{
                "display_name": "Sort By",
                "short_desc": "Field to sort by",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Field to sort results by",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "name",
                        "display_name": "Name",
                        "desc": "Sort by segment name",
                    },
                    <AllowedValueInfo>{
                        "value": "date",
                        "display_name": "Date",
                        "desc": "Sort by date",
                    },
                    <AllowedValueInfo>{
                        "value": "modified_date",
                        "display_name": "Modified Date",
                        "desc": "Sort by last modified date",
                    },
                    <AllowedValueInfo>{
                        "value": "created_date",
                        "display_name": "Created Date",
                        "desc": "Sort by creation date",
                    },
                ),
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
            "create": Class::forName("ConstantContactCreateSegmentDataProvider"),
            "update": Class::forName("ConstantContactUpdateSegmentDataProvider"),
            "delete": Class::forName("ConstantContactDeleteSegmentDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(ConstantContactRestClient::ConstantContactRestClient rest)
            : ConstantContactDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "segments";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Constant Contact segments data provider for listing and searching segments";
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
        return new ConstantContactSegmentDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If segment_id provided, get single record
        if (search_options.segment_id) {
            hash<auto> result = doGet("segments/" + string(search_options.segment_id));
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }

        # Build query params
        hash<auto> params;
        if (search_options.sort_by) {
            params.sort_by = search_options.sort_by;
        }

        list<hash<auto>> all_records = fetchAllPages("segments", "segments", params,
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
