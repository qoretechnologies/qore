# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudBrandsDataProvider.qc SendCloud brands collection data provider

/*  SendCloudBrandsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SendCloudDataProvider module
public namespace SendCloudDataProvider {

#! Collection provider for SendCloud brands
public class SendCloudBrandsDataProvider inherits SendCloudDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "brands",
            "desc": "List SendCloud brands",
            "type": "SendCloudBrandsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        const SearchOptions = {
            "cursor": <DataProviderOptionInfo>{
                "display_name": "Cursor",
                "short_desc": "Pagination cursor",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Opaque pagination cursor returned by a previous page",
            },
            "page_size": <DataProviderOptionInfo>{
                "display_name": "Page Size",
                "short_desc": "Results per page (max 100)",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Number of results per page; SendCloud caps this at 100",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum total records",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of aggregated records to return across pages",
            },
        };
    }

    constructor(*RestClientIo::RestClientIo rest) : SendCloudDataProviderBase(rest) {
    }

    string getName() {
        return "brands";
    }

    *string getDesc() {
        return "List SendCloud brands";
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new SendCloudBrandDataType().getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> params;
        if (search_options.page_size) {
            params.page_size = search_options.page_size;
        }
        if (search_options.cursor) {
            params.cursor = search_options.cursor;
        }
        list<auto> records = fetchAllCursorPages("brands", "brands", params, search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SendCloudDataProvider
