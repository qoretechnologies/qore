# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BitlyBitlinksDataProvider.qc Bitly bitlinks collection data provider

/*  BitlyBitlinksDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BitlyDataProvider module
public namespace BitlyDataProvider {

#! Bitly bitlinks collection data provider
public class BitlyBitlinksDataProvider inherits BitlyDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "bitlinks",
            "desc": "Bitly bitlinks data provider for listing and searching shortened links",
            "type": "BitlyBitlinksDataProvider",
            "has_record": True,
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
            "bitlink": <DataProviderOptionInfo>{
                "display_name": "Bitlink",
                "short_desc": "Get a specific bitlink by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The bitlink ID to retrieve (e.g., `bit.ly/abc123`). "
                    "When provided, returns a single record",
            },
            "group_guid": <DataProviderOptionInfo>{
                "display_name": "Group GUID",
                "short_desc": "Filter by group (required for listing)",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The group GUID to list bitlinks for. Required when listing "
                    "bitlinks (not needed when retrieving a single bitlink by ID)",
                "ref_data": "groups",
            },
            "keyword": <DataProviderOptionInfo>{
                "display_name": "Keyword",
                "short_desc": "Search by keyword",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Search bitlinks by hostname and path (case-insensitive)",
            },
            "created_after": <DataProviderOptionInfo>{
                "display_name": "Created After",
                "short_desc": "Filter by creation date (after)",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter bitlinks created after this ISO 8601 timestamp",
            },
            "created_before": <DataProviderOptionInfo>{
                "display_name": "Created Before",
                "short_desc": "Filter by creation date (before)",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter bitlinks created before this ISO 8601 timestamp",
            },
            "modified_after": <DataProviderOptionInfo>{
                "display_name": "Modified After",
                "short_desc": "Filter by modification date",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter bitlinks modified after this ISO 8601 timestamp",
            },
            "archived": <DataProviderOptionInfo>{
                "display_name": "Archived",
                "short_desc": "Filter by archive status",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by whether bitlinks are archived",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "on", "display_name": "Archived"},
                    <AllowedValueInfo>{"value": "off", "display_name": "Not Archived"},
                    <AllowedValueInfo>{"value": "both", "display_name": "Both"},
                ),
            },
            "custom_bitlink": <DataProviderOptionInfo>{
                "display_name": "Custom Bitlink",
                "short_desc": "Filter custom back-halves",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter bitlinks that have custom back-halves",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "on", "display_name": "Has Custom"},
                    <AllowedValueInfo>{"value": "off", "display_name": "No Custom"},
                    <AllowedValueInfo>{"value": "both", "display_name": "Both"},
                ),
            },
            "tags": <DataProviderOptionInfo>{
                "display_name": "Tags",
                "short_desc": "Filter by tags",
                "type": AbstractDataProviderType::get(SoftAutoListOrNothingType),
                "desc": "Filter bitlinks by tags",
            },
            "campaign_guid": <DataProviderOptionInfo>{
                "display_name": "Campaign GUID",
                "short_desc": "Filter by campaign",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter bitlinks by campaign GUID",
                "ref_data": "campaigns",
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
            "shorten": Class::forName("BitlyShortenBitlinkDataProvider"),
            "create": Class::forName("BitlyCreateBitlinkDataProvider"),
            "update": Class::forName("BitlyUpdateBitlinkDataProvider"),
            "delete": Class::forName("BitlyDeleteBitlinkDataProvider"),
            "clicks": Class::forName("BitlyBitlinkClicksDataProvider"),
            "create-custom": Class::forName("BitlyCreateCustomBitlinkDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(RestClientIo::RestClientIo rest) : BitlyDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "bitlinks";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Bitly bitlinks data provider for listing and searching shortened links";
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
        return new BitlyBitlinkDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If bitlink ID provided, get single record
        if (search_options.bitlink) {
            hash<auto> result = doGet("bitlinks/" + encodeBitlink(search_options.bitlink));
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }

        # group_guid is required for listing
        *string group_guid = search_options.group_guid;
        if (!group_guid) {
            # Auto-detect default group
            hash<auto> groups = doGet("groups");
            if (groups.groups) {
                group_guid = groups.groups[0].guid;
            }
            if (!group_guid) {
                throw "BITLY-ERROR", "group_guid is required to list bitlinks and no default "
                    "group could be detected";
            }
        }

        # Build query params
        hash<auto> params;
        if (search_options.keyword) {
            params.keyword = search_options.keyword;
        }
        if (search_options.created_after) {
            params.created_after = search_options.created_after;
        }
        if (search_options.created_before) {
            params.created_before = search_options.created_before;
        }
        if (search_options.modified_after) {
            params.modified_after = search_options.modified_after;
        }
        if (search_options.archived) {
            params.archived = search_options.archived;
        }
        if (search_options.custom_bitlink) {
            params.custom_bitlink = search_options.custom_bitlink;
        }
        if (search_options.tags) {
            params.tags = search_options.tags;
        }
        if (search_options.campaign_guid) {
            params.campaign_guid = search_options.campaign_guid;
        }

        list<hash<auto>> all_records = fetchAllPages(
            "groups/" + group_guid + "/bitlinks", "links", params, search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BitlyDataProvider
