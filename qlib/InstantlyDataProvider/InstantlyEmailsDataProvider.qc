# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyEmailsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Instantly emails collection data provider
public class InstantlyEmailsDataProvider inherits InstantlyDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "emails",
            "desc": "Instantly emails data provider; list and search emails",
            "type": "InstantlyEmailsDataProvider",
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
                "desc": "Filter emails by campaign UUID",
            },
            "lead_id": <DataProviderOptionInfo>{
                "display_name": "Lead ID",
                "short_desc": "Filter by lead UUID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter emails by lead UUID",
            },
            "ue_type": <DataProviderOptionInfo>{
                "display_name": "Email Type",
                "short_desc": "Filter by email type",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Filter emails by type: `1` = Campaign, `2` = Received, `3` = Sent, `4` = Scheduled",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 1, "display_name": "Campaign"},
                    <AllowedValueInfo>{"value": 2, "display_name": "Received"},
                    <AllowedValueInfo>{"value": 3, "display_name": "Sent"},
                    <AllowedValueInfo>{"value": 4, "display_name": "Scheduled"},
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
        static InstantlyEmailDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(InstantlyRestClient::InstantlyRestClient rest)
            : InstantlyDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "emails";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Instantly emails data provider; list and search emails";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for emails
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> params;

        if (search_options.campaign_id) {
            params.campaign_id = search_options.campaign_id;
        }
        if (search_options.lead_id) {
            params.lead_id = search_options.lead_id;
        }
        if (exists search_options.ue_type) {
            params.ue_type = search_options.ue_type;
        }
        if (search_options.limit) {
            params.limit = search_options.limit;
        }

        list<auto> records = doPaginatedGet("/emails", params);

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace InstantlyDataProvider
