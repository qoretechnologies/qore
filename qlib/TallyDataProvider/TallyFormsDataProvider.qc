# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TallyDataProvider module definition

/** TallyFormsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Search options for listing forms
public const TallyFormsSearchOptions = {
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
        "desc": "Number of forms per page (1-500, default `50`)",
        "preselected": True,
    },
    "workspaceIds": <DataProviderOptionInfo>{
        "display_name": "Workspace IDs",
        "short_desc": "Filter by workspace IDs",
        "type": AbstractDataProviderType::get(SoftListOrNothingType),
        "desc": "List of workspace IDs to filter results",
    },
};

#! Data provider for listing/searching forms (DPAT_FIND)
public class TallyFormsDataProvider inherits TallyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "forms",
            "type": "TallyFormsDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_read": True,
            "has_record": True,
            "supports_native_search": True,
            "search_options": TallyFormsSearchOptions,
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
        hash<auto> params = {};
        if (search_options.page) params.page = search_options.page;
        if (search_options.limit) params.limit = search_options.limit;
        if (search_options.workspaceIds) params.workspaceIds =
            (search_options.workspaceIds.typeCode() == NT_LIST
                ? search_options.workspaceIds.join(",")
                : search_options.workspaceIds);

        string path = "forms";
        if (params) {
            path += "?" + (
                map sprintf("%s=%s", encode_url($1.key, True), encode_url(string($1.value), True)),
                    params.pairIterator()
            ).join("&");
        }
        hash<auto> response = doRestCommand("GET", path);
        list<hash<auto>> recs = response.body.items ?? ();
        return new DefaultRecordIterator(new ListIterator(recs));
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return TallyFormDataType.getFields();
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}
}
