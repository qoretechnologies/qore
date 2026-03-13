# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyCampaignSubsequencesDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Instantly campaign subsequences collection data provider
public class InstantlyCampaignSubsequencesDataProvider inherits InstantlyDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "campaign-subsequences",
            "desc": "Instantly campaign subsequences data provider; list and search subsequences",
            "type": "InstantlyCampaignSubsequencesDataProvider",
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
            "parent_campaign": <DataProviderOptionInfo>{
                "display_name": "Parent Campaign",
                "short_desc": "Filter by parent campaign UUID",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The parent campaign UUID to list subsequences for (required)",
                "required": True,
            },
            "search": <DataProviderOptionInfo>{
                "display_name": "Search",
                "short_desc": "Search subsequences",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter subsequences by search query",
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
        static InstantlyCampaignSubsequenceDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(InstantlyRestClient::InstantlyRestClient rest)
            : InstantlyDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "campaign-subsequences";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Instantly campaign subsequences data provider; list and search subsequences";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for subsequences
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> params;

        if (search_options.parent_campaign) {
            params.parent_campaign = search_options.parent_campaign;
        }
        if (search_options.search) {
            params.search = search_options.search;
        }
        if (search_options.limit) {
            params.limit = search_options.limit;
        }

        list<auto> records = doPaginatedGet("/subsequences", params);

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace InstantlyDataProvider
