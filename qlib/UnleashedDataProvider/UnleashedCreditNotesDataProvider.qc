# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnleashedDataProvider module definition

/*  UnleashedCreditNotesDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the UnleashedDataProvider module
public namespace UnleashedDataProvider {

#! Credit notes search data provider for Unleashed
public class UnleashedCreditNotesDataProvider inherits UnleashedDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "credit-notes",
            "desc": "Search and list credit notes from Unleashed",
            "type": "UnleashedCreditNotesDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options for credit notes
        const SearchOptions = {
            "creditNumber": <DataProviderOptionInfo>{
                "display_name": "Credit Number",
                "short_desc": "Filter by credit note number",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter by specific credit note number",
            },
            "creditStatus": <DataProviderOptionInfo>{
                "display_name": "Credit Status",
                "short_desc": "Filter by credit note status",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter by credit note status (Parked, Completed)",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Parked", "desc": "Parked credit notes"},
                    <AllowedValueInfo>{"value": "Completed", "desc": "Completed credit notes"},
                ),
            },
            "startDate": <DataProviderOptionInfo>{
                "display_name": "Start Date",
                "short_desc": "Filter by start date",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Return credit notes from this date (YYYY-MM-DD)",
            },
            "endDate": <DataProviderOptionInfo>{
                "display_name": "End Date",
                "short_desc": "Filter by end date",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Return credit notes until this date (YYYY-MM-DD)",
            },
            "modifiedSince": <DataProviderOptionInfo>{
                "display_name": "Modified Since",
                "short_desc": "Filter by modification date",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Return credit notes modified after this date/time",
            },
            "pageSize": <DataProviderOptionInfo>{
                "display_name": "Page Size",
                "short_desc": "Results per page",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Number of results per page (max 200, default 200)",
            },
        };
    }

    private {
        #! Record type
        static UnleashedCreditNoteDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(UnleashedRestClient::UnleashedRestClient rest) : UnleashedDataProviderBase(rest) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : UnleashedDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "credit-notes";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Search and list credit notes from Unleashed";
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the record type
    private *AbstractDataProviderType getRecordTypeImpl(*hash<auto> search_options) {
        return recordType;
    }

    #! Performs a search/query
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> params;

        if (search_options.creditNumber) {
            params.creditNumber = search_options.creditNumber;
        }
        if (search_options.creditStatus) {
            params.creditStatus = search_options.creditStatus;
        }
        if (search_options.startDate) {
            params.startDate = formatDate(search_options.startDate);
        }
        if (search_options.endDate) {
            params.endDate = formatDate(search_options.endDate);
        }
        if (search_options.modifiedSince) {
            params.modifiedSince = formatDate(search_options.modifiedSince);
        }
        if (search_options.pageSize) {
            params.pageSize = search_options.pageSize;
        } else {
            params.pageSize = DefaultPageSize;
        }

        auto response = doGet("/CreditNotes", params);

        list<hash<auto>> records;
        if (response.typeCode() == NT_HASH && response.Items.typeCode() == NT_LIST) {
            records = response.Items;
        }

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond,
            search_options, recordType.getFields());
    }
}

} # namespace UnleashedDataProvider
