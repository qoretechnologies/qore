# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GorgiasUsersDataProvider.qc Gorgias users collection data provider

/*  GorgiasUsersDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GorgiasDataProvider module
public namespace GorgiasDataProvider {

#! Gorgias users collection data provider (read-only)
public class GorgiasUsersDataProvider inherits GorgiasDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "users",
            "desc": "Gorgias users data provider for listing helpdesk agents",
            "type": "GorgiasUsersDataProvider",
            "has_record": True,
            "supports_read": True,
            "supports_native_search": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        const SearchOptions = {
            "user_id": <DataProviderOptionInfo>{
                "display_name": "User ID",
                "short_desc": "Get a specific user by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "The user ID to retrieve. When provided, returns a single record",
                "ref_data": "users",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };
    }

    constructor(RestClientIo::RestClientIo rest) : GorgiasDataProviderBase(rest) {
    }

    string getName() {
        return "users";
    }

    *string getDesc() {
        return "Gorgias users data provider for listing helpdesk agents";
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new GorgiasUserDataType().getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (search_options.user_id) {
            hash<auto> result = doGet("users/" + search_options.user_id);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }

        list<hash<auto>> all_records = fetchAllPages("users", NOTHING,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace GorgiasDataProvider
