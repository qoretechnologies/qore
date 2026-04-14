# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file UnbouncePageGroupsDataProvider.qc Unbounce page groups collection data provider

/*  UnbouncePageGroupsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the UnbounceDataProvider module
public namespace UnbounceDataProvider {

#! Unbounce page groups collection data provider
public class UnbouncePageGroupsDataProvider inherits UnbounceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "page-groups",
            "desc": "Unbounce page groups data provider for listing page groups",
            "type": "UnbouncePageGroupsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "sub_account_id": <DataProviderOptionInfo>{
                "display_name": "Sub Account ID",
                "short_desc": "Sub-account to list page groups for",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The sub-account ID to list page groups for. Required for listing",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
            "offset": <DataProviderOptionInfo>{
                "display_name": "Offset",
                "short_desc": "Result offset",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Number of results to skip for pagination",
            },
        };
    }

    #! Creates the object with a REST client
    constructor(*RestClientIo::RestClientIo rest) : UnbounceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "page-groups";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Unbounce page groups data provider for listing page groups";
    }

    #! Returns the record type
    private *AbstractDataProviderType getRecordTypeImpl() {
        return new UnbouncePageGroupDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (!search_options.sub_account_id) {
            throw "UNBOUNCE-ERROR", "sub_account_id is required for listing page groups";
        }

        list<hash<auto>> all_records = fetchAllPages(
            "sub_accounts/" + search_options.sub_account_id + "/page_groups",
            "page_groups", NOTHING, search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace UnbounceDataProvider
