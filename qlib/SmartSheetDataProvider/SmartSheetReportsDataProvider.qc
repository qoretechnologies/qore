# -*- mode: qore; indent-tabs-mode: nil -*-

/*  SmartSheetReportsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace SmartSheetDataProvider {

#! Smartsheet reports collection data provider (read-only)
public class SmartSheetReportsDataProvider inherits SmartSheetDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "reports",
            "desc": "Smartsheet reports data provider for listing and retrieving reports "
                "(read-only)",
            "type": "SmartSheetReportsDataProvider",
            "has_record": True,
            "supports_read": True,
            "supports_native_search": True,
            "search_options": SearchOptions,
        };

        const SearchOptions = {
            "reportId": <DataProviderOptionInfo>{
                "display_name": "Report ID",
                "short_desc": "Get a specific report by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The report ID to retrieve. When provided, returns a single report",
            },
        };
    }

    constructor(RestClientIo::RestClientIo rest) : SmartSheetDataProviderBase(rest) {
    }

    string getName() { return "reports"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new SmartSheetReportDataType().getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (search_options.reportId) {
            hash<auto> result = doGet("reports/" + search_options.reportId);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }
        list<hash<auto>> records = fetchAllPages("reports", "data");
        return new DefaultRecordIterator(new ListHashIterator(records),
            where_cond, search_options);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace SmartSheetDataProvider
