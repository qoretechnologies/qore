# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CustomerIoDataProvider module definition

/*  CustomerIoPeopleDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CustomerIoDataProvider module
public namespace CustomerIoDataProvider {

#! Customer.io people collection data provider
public class CustomerIoPeopleDataProvider inherits CustomerIoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "people",
            "desc": "Customer.io people data provider - list and search people",
            "type": "CustomerIoPeopleDataProvider",
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
            "email": <DataProviderOptionInfo>{
                "display_name": "Email",
                "short_desc": "Filter by email address",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter people by email address",
            },
            "id": <DataProviderOptionInfo>{
                "display_name": "ID",
                "short_desc": "Filter by customer ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter people by customer ID",
            },
            "cio_id": <DataProviderOptionInfo>{
                "display_name": "CIO ID",
                "short_desc": "Filter by Customer.io ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter people by Customer.io internal ID",
            },
            "page": <DataProviderOptionInfo>{
                "display_name": "Page",
                "short_desc": "Page number",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Page number for pagination (starting at 1)",
            },
            "page_size": <DataProviderOptionInfo>{
                "display_name": "Page Size",
                "short_desc": "Results per page",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Number of results per page",
            },
        };
    }

    private {
        #! Record type
        static CustomerIoPersonDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(CustomerIoRestClient::CustomerIoRestClient rest)
            : CustomerIoDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(CustomerIoRestClient::CustomerIoRestConnection conn)
            : CustomerIoDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "people";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Customer.io people data provider - list and search people";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for people
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> params;

        # Build query parameters from search options
        # Note: GET /customers requires at least one filter (email, id, or cio_id)
        if (search_options.email) {
            params.email = search_options.email;
        }
        if (search_options.id) {
            params.id = search_options.id;
        }
        if (search_options.cio_id) {
            params.cio_id = search_options.cio_id;
        }
        if (search_options.page) {
            params.page = search_options.page;
        }
        if (search_options.page_size) {
            params.page_size = search_options.page_size;
        }

        # If no filter is provided, return empty results (API requires at least one filter)
        if (!params) {
            list<hash<auto>> empty_list = ();
            return new DefaultRecordIterator(new ListHashIterator(empty_list), where_cond,
                search_options, recordType.getFields());
        }

        @debug(log(LoggerLevel::DEBUG, "searchRecordsImpl: params=%y", params));

        auto response = doGet("/customers", params);
        list<hash<auto>> records = response.results ?? response.customers ?? ();

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CustomerIoDataProvider
