# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LemlistDataProvider module definition

/*  LemlistLeadsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Lemlist leads collection data provider
public class LemlistLeadsDataProvider inherits LemlistDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "leads",
            "desc": "Lemlist leads data provider - list leads in a campaign",
            "type": "LemlistLeadsDataProvider",
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
                "short_desc": "Campaign to list leads from",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The identifier of the campaign to retrieve leads from",
                "required": True,
                "ref_data": "campaigns",
            },
            "email": <DataProviderOptionInfo>{
                "display_name": "Email",
                "short_desc": "Lead email address for single lookup",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "The email address of a specific lead to retrieve from the campaign",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of results",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Maximum number of leads to return (max 100)",
            },
            "offset": <DataProviderOptionInfo>{
                "display_name": "Offset",
                "short_desc": "Number of records to skip",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Number of leads to skip for pagination",
            },
        };
    }

    private {
        #! Record type
        static LemlistLeadDataType recordType();
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
        return "leads";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Lemlist leads data provider - list leads in a campaign";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for leads in a campaign
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (!search_options.campaignId) {
            throw "LEMLIST-ERROR", "search option 'campaignId' is required for listing leads";
        }

        # If email is provided, get a specific lead by email
        if (search_options.email) {
            # Preserve @ in email - Lemlist API doesn't decode %40 in path segments
            string encoded_email = replace(encode_url(search_options.email, True), "%40", "@");
            auto response = doGet(sprintf("/campaigns/%s/leads/%s",
                encode_url(search_options.campaignId, True), encoded_email));
            list<hash<auto>> records;
            if (response.typeCode() == NT_HASH) {
                records = (response,);
            }
            return new DefaultRecordIterator(new ListHashIterator(records), where_cond, NOTHING,
                recordType.getFields());
        }

        hash<auto> params;
        if (search_options.limit) {
            params.limit = search_options.limit;
        } else {
            params.limit = DefaultPageSize;
        }
        if (search_options.offset) {
            params.offset = search_options.offset;
        }

        auto response = doGet(sprintf("/campaigns/%s/leads", encode_url(search_options.campaignId, True)), params);
        list<hash<auto>> records;
        if (response.typeCode() == NT_LIST) {
            records = response;
        } else if (response.leads.typeCode() == NT_LIST) {
            records = response.leads;
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
