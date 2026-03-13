# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyLeadsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the InstantlyDataProvider module
public namespace InstantlyDataProvider {

#! Instantly leads collection data provider
public class InstantlyLeadsDataProvider inherits InstantlyDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "leads",
            "desc": "Instantly leads data provider; list and search leads",
            "type": "InstantlyLeadsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "campaign_id": <DataProviderOptionInfo>{
                "display_name": "Campaign ID",
                "short_desc": "Filter by campaign UUID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter leads by the campaign UUID they belong to",
            },
            "list_id": <DataProviderOptionInfo>{
                "display_name": "List ID",
                "short_desc": "Filter by lead list UUID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter leads by the lead list UUID they belong to",
            },
            "email": <DataProviderOptionInfo>{
                "display_name": "Email",
                "short_desc": "Filter by email address",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter leads by email address",
            },
            "status": <DataProviderOptionInfo>{
                "display_name": "Status",
                "short_desc": "Filter by lead status",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Filter leads by status: `1` = Active, `2` = Paused, `3` = Completed, "
                    "`-1` = Bounced, `-2` = Unsubscribed, `-3` = Skipped",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 1, "display_name": "Active"},
                    <AllowedValueInfo>{"value": 2, "display_name": "Paused"},
                    <AllowedValueInfo>{"value": 3, "display_name": "Completed"},
                    <AllowedValueInfo>{"value": -1, "display_name": "Bounced"},
                    <AllowedValueInfo>{"value": -2, "display_name": "Unsubscribed"},
                    <AllowedValueInfo>{"value": -3, "display_name": "Skipped"},
                ),
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of results to return",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "The maximum number of results to return per page",
            },
        };
    }

    private {
        #! Record type
        static InstantlyLeadDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(InstantlyRestClient::InstantlyRestClient rest)
            : InstantlyDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "leads";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Instantly leads data provider; list and search leads";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for leads
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> params;

        if (search_options.campaign_id) {
            params.campaign_id = search_options.campaign_id;
        }
        if (search_options.list_id) {
            params.list_id = search_options.list_id;
        }
        if (search_options.email) {
            params.email = search_options.email;
        }
        if (exists search_options.status) {
            params.status = search_options.status;
        }
        if (search_options.limit) {
            params.limit = search_options.limit;
        }

        list<auto> records = doPaginatedPost("/leads/list", params);

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace InstantlyDataProvider
