# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KitTagsDataProvider.qc Kit tags collection data provider

/*  KitTagsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Kit tags collection data provider
public class KitTagsDataProvider inherits KitDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "tags",
            "desc": "Kit tags data provider for listing and searching tags",
            "type": "KitTagsDataProvider",
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
            "tag_id": <DataProviderOptionInfo>{
                "display_name": "Tag ID",
                "short_desc": "Get a specific tag by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by tag ID for single record lookup",
            },
            "name": <DataProviderOptionInfo>{
                "display_name": "Name",
                "short_desc": "Filter by tag name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter tags by name",
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
            "create": Class::forName("KitCreateTagDataProvider"),
            "update": Class::forName("KitUpdateTagDataProvider"),
            "tag-subscriber": Class::forName("KitTagSubscriberDataProvider"),
            "untag-subscriber": Class::forName("KitUntagSubscriberDataProvider"),
            "bulk-tag": Class::forName("KitBulkTagSubscribersDataProvider"),
            "tag-subscribers": Class::forName("KitTagSubscribersDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(KitRestClient::KitRestClient rest) : KitDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "tags";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Kit tags data provider for listing and searching tags";
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
        return new KitTagDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # Kit API does not support GET /tags/{id}, so we always list all tags
        # and filter client-side
        list<hash<auto>> all_records = fetchAllPages("tags", "tags", NOTHING,
            search_options.limit);

        # Filter by tag_id if provided
        if (search_options.tag_id) {
            list<hash<auto>> filtered = ();
            foreach hash<auto> tag in (all_records) {
                if (string(tag.id) == search_options.tag_id) {
                    filtered += tag;
                    break;
                }
            }
            return new DefaultRecordIterator(new ListHashIterator(filtered),
                where_cond, search_options);
        }

        # Filter by name if provided
        if (search_options.name) {
            list<hash<auto>> filtered = ();
            foreach hash<auto> tag in (all_records) {
                if (tag.name == search_options.name) {
                    filtered += tag;
                }
            }
            all_records = filtered;
        }

        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KitDataProvider
