# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KitSubscribersDataProvider.qc Kit subscribers collection data provider

/*  KitSubscribersDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Kit subscribers collection data provider
public class KitSubscribersDataProvider inherits KitDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "subscribers",
            "desc": "Kit subscribers data provider for listing and searching subscribers",
            "type": "KitSubscribersDataProvider",
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
            "subscriber_id": <DataProviderOptionInfo>{
                "display_name": "Subscriber ID",
                "short_desc": "Get a specific subscriber by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by subscriber ID for single record lookup",
            },
            "email_address": <DataProviderOptionInfo>{
                "display_name": "Email Address",
                "short_desc": "Filter by email address",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter subscribers by email address",
            },
            "state": <DataProviderOptionInfo>{
                "display_name": "State",
                "short_desc": "Filter by subscriber state",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter subscribers by state",
                "allowed_values": (<AllowedValueInfo>{"value": "active"}, <AllowedValueInfo>{"value": "inactive"}, <AllowedValueInfo>{"value": "bounced"}, <AllowedValueInfo>{"value": "complained"}, <AllowedValueInfo>{"value": "cancelled"}),
            },
            "tag_id": <DataProviderOptionInfo>{
                "display_name": "Tag ID",
                "short_desc": "Filter by tag ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter subscribers by tag ID",
                "ref_data": "tags",
            },
            "created_after": <DataProviderOptionInfo>{
                "display_name": "Created After",
                "short_desc": "Filter by creation date (after)",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Filter subscribers created after this date",
            },
            "created_before": <DataProviderOptionInfo>{
                "display_name": "Created Before",
                "short_desc": "Filter by creation date (before)",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Filter subscribers created before this date",
            },
            "updated_after": <DataProviderOptionInfo>{
                "display_name": "Updated After",
                "short_desc": "Filter by update date (after)",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Filter subscribers updated after this date",
            },
            "updated_before": <DataProviderOptionInfo>{
                "display_name": "Updated Before",
                "short_desc": "Filter by update date (before)",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Filter subscribers updated before this date",
            },
            "sort_field": <DataProviderOptionInfo>{
                "display_name": "Sort Field",
                "short_desc": "Field to sort by",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The field to sort results by",
                "allowed_values": (<AllowedValueInfo>{"value": "created_at"}, <AllowedValueInfo>{"value": "updated_at"}),
            },
            "sort_order": <DataProviderOptionInfo>{
                "display_name": "Sort Order",
                "short_desc": "Sort order",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The sort order for results",
                "allowed_values": (<AllowedValueInfo>{"value": "asc"}, <AllowedValueInfo>{"value": "desc"}),
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
            "create": Class::forName("KitCreateSubscriberDataProvider"),
            "update": Class::forName("KitUpdateSubscriberDataProvider"),
            "unsubscribe": Class::forName("KitUnsubscribeDataProvider"),
            "bulk-create": Class::forName("KitBulkCreateSubscribersDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(KitRestClient::KitRestClient rest) : KitDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "subscribers";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Kit subscribers data provider for listing and searching subscribers";
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
        return new KitSubscriberDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If subscriber_id provided, get single record
        if (search_options.subscriber_id) {
            hash<auto> result = doGet("subscribers/" + encode_url(search_options.subscriber_id, True));
            return new DefaultRecordIterator(new ListHashIterator((result.subscriber,)),
                where_cond, search_options);
        }

        # Build query params
        hash<auto> params;
        if (search_options.email_address) {
            params.email_address = search_options.email_address;
        }
        if (search_options.state) {
            params.status = search_options.state;
        }
        if (search_options.tag_id) {
            params.tag_id = search_options.tag_id;
        }
        if (search_options.created_after) {
            params.created_after = search_options.created_after.format("YYYY-MM-DDTHH:mm:ssZ");
        }
        if (search_options.created_before) {
            params.created_before = search_options.created_before.format("YYYY-MM-DDTHH:mm:ssZ");
        }
        if (search_options.updated_after) {
            params.updated_after = search_options.updated_after.format("YYYY-MM-DDTHH:mm:ssZ");
        }
        if (search_options.updated_before) {
            params.updated_before = search_options.updated_before.format("YYYY-MM-DDTHH:mm:ssZ");
        }
        if (search_options.sort_field) {
            params.sort_field = search_options.sort_field;
        }
        if (search_options.sort_order) {
            params.sort_order = search_options.sort_order;
        }

        list<hash<auto>> all_records = fetchAllPages("subscribers", "subscribers", params,
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
