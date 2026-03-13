# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LemlistDataProvider module definition

/*  LemlistCampaignsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the LemlistDataProvider module
public namespace LemlistDataProvider {

#! Lemlist campaigns collection data provider
public class LemlistCampaignsDataProvider inherits LemlistDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "campaigns",
            "desc": "Lemlist campaigns data provider - list and search campaigns",
            "type": "LemlistCampaignsDataProvider",
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
            "campaignId": <DataProviderOptionInfo>{
                "display_name": "Campaign ID",
                "short_desc": "ID of a specific campaign to retrieve",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "The unique identifier of a specific campaign to retrieve. When provided, returns only that campaign.",
                "ref_data": "campaigns",
            },
            "status": <DataProviderOptionInfo>{
                "display_name": "Status",
                "short_desc": "Filter campaigns by status",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter campaigns by their current status",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "running", "desc": "Running campaigns"},
                    <AllowedValueInfo>{"value": "draft", "desc": "Draft campaigns"},
                    <AllowedValueInfo>{"value": "archived", "desc": "Archived campaigns"},
                    <AllowedValueInfo>{"value": "ended", "desc": "Ended campaigns"},
                    <AllowedValueInfo>{"value": "paused", "desc": "Paused campaigns"},
                    <AllowedValueInfo>{"value": "errors", "desc": "Campaigns with errors"},
                ),
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of results",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Maximum number of campaigns to return (max 100)",
            },
            "offset": <DataProviderOptionInfo>{
                "display_name": "Offset",
                "short_desc": "Number of records to skip",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Number of campaigns to skip for pagination",
            },
        };
    }

    private {
        #! Record type
        static LemlistCampaignDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(LemlistRestClient::LemlistRestClient rest)
            : LemlistDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(LemlistRestClient::LemlistRestConnection conn) : LemlistDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "campaigns";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Lemlist campaigns data provider - list and search campaigns";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for campaigns
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # Single-record lookup by campaignId (used by get-campaign DPAT_FIND_SINGLE)
        if (search_options.campaignId) {
            auto campaign = doGet(sprintf("/campaigns/%s", encode_url(search_options.campaignId, True)));
            return new DefaultRecordIterator(new ListHashIterator((campaign,)), where_cond, NOTHING,
                recordType.getFields());
        }

        hash<auto> params;
        if (search_options.status) {
            params.status = search_options.status;
        }
        if (search_options.limit) {
            params.limit = search_options.limit;
        } else {
            params.limit = DefaultPageSize;
        }
        if (search_options.offset) {
            params.offset = search_options.offset;
        }

        auto response = doGet("/campaigns", params);
        list<hash<auto>> records;
        if (response.typeCode() == NT_LIST) {
            records = response;
        } else if (response.campaigns.typeCode() == NT_LIST) {
            records = response.campaigns;
        }

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, NOTHING,
            recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace LemlistDataProvider
