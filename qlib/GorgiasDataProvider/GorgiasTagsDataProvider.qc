# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GorgiasTagsDataProvider.qc Gorgias tags collection data provider

/*  GorgiasTagsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GorgiasDataProvider module
public namespace GorgiasDataProvider {

#! Gorgias tags collection data provider
public class GorgiasTagsDataProvider inherits GorgiasDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "tags",
            "desc": "Gorgias tags data provider for listing and managing tags",
            "type": "GorgiasTagsDataProvider",
            "has_record": True,
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        const SearchOptions = {
            "tag_id": <DataProviderOptionInfo>{
                "display_name": "Tag ID",
                "short_desc": "Get a specific tag by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "The tag ID to retrieve. When provided, returns a single record",
                "ref_data": "tags",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };

        const ChildMap = {
            "create": Class::forName("GorgiasCreateTagDataProvider"),
            "update": Class::forName("GorgiasUpdateTagDataProvider"),
            "delete": Class::forName("GorgiasDeleteTagDataProvider"),
        };
    }

    constructor(RestClientIo::RestClientIo rest) : GorgiasDataProviderBase(rest) {
    }

    string getName() {
        return "tags";
    }

    *string getDesc() {
        return "Gorgias tags data provider for listing and managing tags";
    }

    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new GorgiasTagDataType().getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (search_options.tag_id) {
            hash<auto> result = doGet("tags/" + search_options.tag_id);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }

        list<hash<auto>> all_records = fetchAllPages("tags", NOTHING,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace GorgiasDataProvider
