# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SegmentDataProvider module definition

/*  SegmentTrackingPlansDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SegmentDataProvider module
public namespace SegmentDataProvider {

#! Segment tracking plans collection data provider
public class SegmentTrackingPlansDataProvider inherits SegmentDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "tracking-plans",
            "desc": "Segment tracking plans data provider - list tracking plans with cursor-based pagination",
            "type": "SegmentTrackingPlansDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };

        #! Search options
        const SearchOptions = {
            "count": <DataProviderOptionInfo>{
                "display_name": "Count",
                "short_desc": "Number of results per page",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Number of results per page (`1`-`1000`, default: `100`)",
            },
            "type": <DataProviderOptionInfo>{
                "display_name": "Type",
                "short_desc": "Filter by tracking plan type",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter tracking plans by type",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "LIVE", "display_name": "Live", "desc": "Live tracking plan"},
                    <AllowedValueInfo>{"value": "PROPERTY_LIBRARY", "display_name": "Property Library",
                        "desc": "Property library"},
                    <AllowedValueInfo>{"value": "RULE_LIBRARY", "display_name": "Rule Library",
                        "desc": "Rule library"},
                    <AllowedValueInfo>{"value": "TEMPLATE", "display_name": "Template",
                        "desc": "Template tracking plan"},
                ),
            },
        };
    }

    private {
        #! Record type
        static SegmentTrackingPlanDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(RestClientIo::RestClientIo rest) : SegmentDataProviderBase(rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : SegmentDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "tracking-plans";
    }

    #! Returns the provider description
    *string getDesc() {
        return ProviderInfo.desc;
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for tracking plans
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # Build query with optional type filter
        string path = "tracking-plans";
        if (search_options.type) {
            path += sprintf("?type=%s", encode_url(search_options.type, True));
        }
        list<auto> records = doPaginatedGet(path, search_options.count);
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SegmentDataProvider
