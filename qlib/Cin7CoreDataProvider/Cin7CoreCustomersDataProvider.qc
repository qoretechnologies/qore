# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreCustomersDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the Cin7CoreDataProvider module
public namespace Cin7CoreDataProvider {

#! Cin7Core customers collection data provider - list and search customers
public class Cin7CoreCustomersDataProvider inherits Cin7CoreDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "customers",
            "desc": "Cin7Core customers data provider - list and search customers",
            "type": "Cin7CoreCustomersDataProvider",
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
            "ID": <DataProviderOptionInfo>{
                "display_name": "Customer ID",
                "short_desc": "Filter by customer ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Return a specific customer by ID",
            },
            "Name": <DataProviderOptionInfo>{
                "display_name": "Name",
                "short_desc": "Filter by name",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter customers by name (partial match)",
            },
            "ContactFilter": <DataProviderOptionInfo>{
                "display_name": "Contact Filter",
                "short_desc": "Filter by contact info",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter by contact name, email, or phone",
            },
            "ModifiedSince": <DataProviderOptionInfo>{
                "display_name": "Modified Since",
                "short_desc": "Filter by modification date",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Return customers modified after this date/time",
            },
            "IncludeDeprecated": <DataProviderOptionInfo>{
                "display_name": "Include Deprecated",
                "short_desc": "Include deprecated customers",
                "type": AbstractDataProviderType::get(BoolOrNothingType),
                "desc": "Include deprecated/inactive customers in results",
            },
            "Page": <DataProviderOptionInfo>{
                "display_name": "Page",
                "short_desc": "Page number",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Page number for pagination (starting at 1)",
            },
            "Limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Results per page",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Number of results per page (default 100, max 1000)",
            },
        };
    }

    private {
        #! Record type
        static Cin7CoreCustomerDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(Cin7CoreRestClient::Cin7CoreRestClient rest) : Cin7CoreDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(Cin7CoreRestClient::Cin7CoreRestConnection conn) : Cin7CoreDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : Cin7CoreDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "customers";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Cin7Core customers data provider - list and search customers";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for customers
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> params;

        # Build query parameters from search options
        if (search_options.ID) {
            params.ID = search_options.ID;
        }
        if (search_options.Name) {
            params.Name = search_options.Name;
        }
        if (search_options.ContactFilter) {
            params.ContactFilter = search_options.ContactFilter;
        }
        if (search_options.ModifiedSince) {
            params.ModifiedSince = search_options.ModifiedSince;
        }
        if (search_options.IncludeDeprecated) {
            params.IncludeDeprecated = search_options.IncludeDeprecated;
        }
        if (search_options.Page) {
            params.Page = search_options.Page;
        } else {
            params.Page = 1;
        }
        if (search_options.Limit) {
            params.Limit = search_options.Limit;
        } else {
            params.Limit = DefaultPageSize;
        }

        @debug(log(LoggerLevel::DEBUG, "searchRecordsImpl: params=%y", params));

        hash<auto> response = doGet("/Customer", params);
        list<hash<auto>> records = response.CustomerList ?? ();

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace Cin7CoreDataProvider
