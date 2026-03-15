# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KitBroadcastsDataProvider.qc Kit broadcasts collection data provider

/*  KitBroadcastsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KitDataProvider module
public namespace KitDataProvider {

#! Kit broadcasts collection data provider
public class KitBroadcastsDataProvider inherits KitDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "broadcasts",
            "desc": "Kit broadcasts data provider for listing and searching broadcasts",
            "type": "KitBroadcastsDataProvider",
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
            "broadcast_id": <DataProviderOptionInfo>{
                "display_name": "Broadcast ID",
                "short_desc": "Get a specific broadcast by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by broadcast ID for single record lookup",
                "ref_data": "broadcasts",
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
            "create": Class::forName("KitCreateBroadcastDataProvider"),
            "update": Class::forName("KitUpdateBroadcastDataProvider"),
            "delete": Class::forName("KitDeleteBroadcastDataProvider"),
            "stats": Class::forName("KitGetBroadcastStatsDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(KitRestClient::KitRestClient rest) : KitDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "broadcasts";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Kit broadcasts data provider for listing and searching broadcasts";
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
        return new KitBroadcastDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If broadcast_id provided, get single record
        if (search_options.broadcast_id) {
            hash<auto> result = doGet("broadcasts/" + encode_url(search_options.broadcast_id, True));
            return new DefaultRecordIterator(new ListHashIterator((result.broadcast,)),
                where_cond, search_options);
        }

        list<hash<auto>> all_records = fetchAllPages("broadcasts", "broadcasts", NOTHING,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KitDataProvider
