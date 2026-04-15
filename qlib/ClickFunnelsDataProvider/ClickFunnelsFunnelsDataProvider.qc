# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ClickFunnelsFunnelsDataProvider.qc ClickFunnels funnels collection and actions

/*  ClickFunnelsFunnelsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace ClickFunnelsDataProvider {

public class ClickFunnelsFunnelsDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "funnels",
            "desc": "ClickFunnels funnels data provider",
            "type": "ClickFunnelsFunnelsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = {
            "funnel_id": <DataProviderOptionInfo>{
                "display_name": "Funnel ID",
                "short_desc": "Get a specific funnel by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by funnel ID for single record lookup",
            },
            "after": <DataProviderOptionInfo>{
                "display_name": "After Cursor",
                "short_desc": "Pagination cursor",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The cursor value for fetching the next page of results",
            },
            "sort_order": <DataProviderOptionInfo>{
                "display_name": "Sort Order",
                "short_desc": "Sort order for results",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Sort order for results",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "asc", "display_name": "Ascending"},
                    <AllowedValueInfo>{"value": "desc", "display_name": "Descending"},
                ),
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };
        const ChildMap = {
            "update": Class::forName("ClickFunnelsUpdateFunnelDataProvider"),
        };
    }

    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest) : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "funnels"; }
    *string getDesc() { return "ClickFunnels funnels data provider"; }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name}; if (cls) { return cls.newObject(rest); }
    }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) { return new ClickFunnelsFunnelDataType().getFields(); }
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        if (search_options.funnel_id) {
            auto result = doGet("funnels/" + search_options.funnel_id);
            return new DefaultRecordIterator(new ListHashIterator((result,)), where_cond, search_options);
        }
        hash<auto> params;
        if (search_options.sort_order) { params.sort_order = search_options.sort_order; }
        list<auto> records = fetchAllPages(getWorkspacePath("funnels"), params, search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Update Funnel ---
public class ClickFunnelsUpdateFunnelRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "funnel_id": { "type": StringType, "display_name": "Funnel ID", "short_desc": "The funnel to update", "desc": "The unique identifier of the funnel to update", "required": True, "ref_data": "funnels" },
            "name": { "type": StringOrNothingType, "display_name": "Name", "short_desc": "The funnel name", "desc": "The new name for the funnel" },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ClickFunnelsUpdateFunnelDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{ "name": "update", "desc": "Update a funnel in ClickFunnels", "type": "ClickFunnelsUpdateFunnelDataProvider", "supports_request": True, "constructor_options": ConstructorOptions };
        const ConstructorOptions = BaseConstructorOptions;
        static *AbstractDataProviderType ResponseType = new ClickFunnelsFunnelDataType();
        static *AbstractDataProviderType RequestType = new ClickFunnelsUpdateFunnelRequestDataType();
    }
    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest) : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "update"; }
    *string getDesc() { return "Update a funnel in ClickFunnels"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return new ClickFunnelsUpdateFunnelRequestDataType(); }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ClickFunnelsFunnelDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = remove req.funnel_id;
        return doPut("funnels/" + id, {"funnel": req});
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace ClickFunnelsDataProvider
