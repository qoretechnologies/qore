# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TallyDataProvider module definition

/** TallySubmissionsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the TallyDataProvider module
public namespace TallyDataProvider {
#! Search options for listing submissions
public const TallySubmissionsSearchOptions = {
    "form_id": <DataProviderOptionInfo>{
        "display_name": "Form ID",
        "short_desc": "The form to list submissions for",
        "type": AbstractDataProviderTypeMap."string",
        "desc": "The ID of the form to retrieve submissions from",
        "required": True,
        "preselected": True,
    },
    "page": <DataProviderOptionInfo>{
        "display_name": "Page",
        "short_desc": "Page number",
        "type": AbstractDataProviderTypeMap."*int",
        "desc": "Page number for pagination (default `1`)",
    },
    "limit": <DataProviderOptionInfo>{
        "display_name": "Limit",
        "short_desc": "Number of results per page",
        "type": AbstractDataProviderTypeMap."*int",
        "desc": "Number of submissions per page (1-500, default `50`)",
        "preselected": True,
    },
    "filter": <DataProviderOptionInfo>{
        "display_name": "Filter",
        "short_desc": "Submission status filter",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "Filter submissions by completion status",
        "allowed_values": (
            <AllowedValueInfo>{"value": "all", "display_name": "All"},
            <AllowedValueInfo>{"value": "completed", "display_name": "Completed"},
            <AllowedValueInfo>{"value": "partial", "display_name": "Partial"},
        ),
    },
    "startDate": <DataProviderOptionInfo>{
        "display_name": "Start Date",
        "short_desc": "Filter by start date",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "Filter submissions submitted on or after this ISO 8601 date",
    },
    "endDate": <DataProviderOptionInfo>{
        "display_name": "End Date",
        "short_desc": "Filter by end date",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "Filter submissions submitted on or before this ISO 8601 date",
    },
    "afterId": <DataProviderOptionInfo>{
        "display_name": "After ID",
        "short_desc": "Cursor-based pagination",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "Get submissions after a specific submission ID for cursor-based pagination",
    },
};

#! Data provider for listing/searching submissions (DPAT_FIND)
public class TallySubmissionsDataProvider inherits TallyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "submissions",
            "type": "TallySubmissionsDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_read": True,
            "has_record": True,
            "supports_native_search": True,
            "search_options": TallySubmissionsSearchOptions,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    constructor(RestClientIo::RestClientIo rest) : TallyDataProviderBase(rest) {}
    constructor(*hash<auto> options) : TallyDataProviderBase(options) {}
    string getName() { return ProviderInfo.name; }

    private *AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (!search_options.form_id) {
            throw "SEARCH-OPTION-ERROR", "missing required 'form_id' search option";
        }

        hash<auto> params = {};
        if (search_options.page) params.page = search_options.page;
        if (search_options.limit) params.limit = search_options.limit;
        if (search_options.filter) params.filter = search_options.filter;
        if (search_options.startDate) params.startDate = search_options.startDate;
        if (search_options.endDate) params.endDate = search_options.endDate;
        if (search_options.afterId) params.afterId = search_options.afterId;

        string path = "forms/" + search_options.form_id + "/submissions";
        if (params) {
            path += "?" + (
                map sprintf("%s=%s", encode_url($1.key, True), encode_url(string($1.value), True)),
                    params.pairIterator()
            ).join("&");
        }
        hash<auto> response = doRestCommand("GET", path);
        list<hash<auto>> recs = response.body.submissions ?? ();
        return new DefaultRecordIterator(new ListIterator(recs));
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return TallySubmissionDataType.getFields();
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}
}
