# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KitTagSubscribersDataProvider.qc Kit tag subscribers collection data provider

/*  KitTagSubscribersDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Kit tag subscribers collection data provider
public class KitTagSubscribersDataProvider inherits KitDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "tag-subscribers",
            "desc": "Kit tag subscribers data provider for listing subscribers of a tag",
            "type": "KitTagSubscribersDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "tag_id": <DataProviderOptionInfo>{
                "display_name": "Tag ID",
                "short_desc": "The tag ID to list subscribers for",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The tag ID to list subscribers for (required)",
                "required": True,
                "ref_data": "tags",
            },
            "subscriber_state": <DataProviderOptionInfo>{
                "display_name": "Subscriber State",
                "short_desc": "Filter by subscriber state",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter subscribers by state",
                "allowed_values": (<AllowedValueInfo>{"value": "active"}, <AllowedValueInfo>{"value": "inactive"}),
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };
    }

    #! Creates the object with a REST client
    constructor(KitRestClient::KitRestClient rest) : KitDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "tag-subscribers";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Kit tag subscribers data provider for listing subscribers of a tag";
    }

    #! Returns the record type
    private *AbstractDataProviderType getRecordTypeImpl() {
        return new KitSubscriberDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (!search_options.tag_id) {
            throw "SEARCH-OPTION-ERROR", "tag_id is required for tag-subscribers provider";
        }

        # Build query params
        hash<auto> params;
        if (search_options.subscriber_state) {
            params.subscriber_state = search_options.subscriber_state;
        }

        list<hash<auto>> all_records = fetchAllPages(
            sprintf("tags/%s/subscribers", encode_url(search_options.tag_id, True)), "subscribers", params,
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
