# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LemlistDataProvider module definition

/*  LemlistActivitiesDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Lemlist activities collection data provider
public class LemlistActivitiesDataProvider inherits LemlistDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "activities",
            "desc": "Lemlist activities data provider - list email and engagement activities",
            "type": "LemlistActivitiesDataProvider",
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
            "type": <DataProviderOptionInfo>{
                "display_name": "Activity Type",
                "short_desc": "Filter by activity type",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter activities by type",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "emailsSent", "desc": "Emails sent"},
                    <AllowedValueInfo>{"value": "emailsOpened", "desc": "Emails opened"},
                    <AllowedValueInfo>{"value": "emailsClicked", "desc": "Emails clicked"},
                    <AllowedValueInfo>{"value": "emailsReplied", "desc": "Emails replied"},
                    <AllowedValueInfo>{"value": "emailsBounced", "desc": "Emails bounced"},
                    <AllowedValueInfo>{"value": "interested", "desc": "Lead marked as interested"},
                    <AllowedValueInfo>{"value": "notInterested", "desc": "Lead marked as not interested"},
                ),
                "preselected": True,
            },
            "campaignId": <DataProviderOptionInfo>{
                "display_name": "Campaign ID",
                "short_desc": "Filter by campaign",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter activities by campaign identifier",
                "preselected": True,
            },
            "leadId": <DataProviderOptionInfo>{
                "display_name": "Lead ID",
                "short_desc": "Filter by lead",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter activities by lead identifier",
                "ref_data": "leads",
            },
            "isFirst": <DataProviderOptionInfo>{
                "display_name": "First Occurrence Only",
                "short_desc": "Only return first occurrences",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "If `True`, only returns the first occurrence of each activity type per lead per campaign",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of results",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Maximum number of activities to return (max 100)",
            },
            "offset": <DataProviderOptionInfo>{
                "display_name": "Offset",
                "short_desc": "Number of records to skip",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Number of activities to skip for pagination",
            },
        };
    }

    private {
        #! Record type
        static LemlistActivityDataType recordType();
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
        return "activities";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Lemlist activities data provider - list email and engagement activities";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for activities
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> params;

        if (search_options.type) {
            params.type = search_options.type;
        }
        if (search_options.campaignId) {
            params.campaignId = search_options.campaignId;
        }
        if (search_options.leadId) {
            params.leadId = search_options.leadId;
        }
        if (exists search_options.isFirst) {
            params.isFirst = search_options.isFirst ? "true" : "false";
        }
        if (search_options.limit) {
            params.limit = search_options.limit;
        } else {
            params.limit = DefaultPageSize;
        }
        if (search_options.offset) {
            params.offset = search_options.offset;
        }

        auto response = doGet("/activities", params);
        list<hash<auto>> records;
        if (response.typeCode() == NT_LIST) {
            records = response;
        } else if (response.activities.typeCode() == NT_LIST) {
            records = response.activities;
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
