# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreProductsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Cin7Core products collection data provider - list and search products
public class Cin7CoreProductsDataProvider inherits Cin7CoreDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "products",
            "desc": "Cin7Core products data provider - list and search products",
            "type": "Cin7CoreProductsDataProvider",
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
                "display_name": "Product ID",
                "short_desc": "Filter by product ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Return a specific product by ID",
                "preselected": True,
            },
            "SKU": <DataProviderOptionInfo>{
                "display_name": "SKU",
                "short_desc": "Filter by SKU",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter products by SKU",
                "preselected": True,
            },
            "Name": <DataProviderOptionInfo>{
                "display_name": "Name",
                "short_desc": "Filter by name",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter products by name (partial match)",
                "preselected": True,
            },
            "Category": <DataProviderOptionInfo>{
                "display_name": "Category",
                "short_desc": "Filter by category",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter products by category",
            },
            "Brand": <DataProviderOptionInfo>{
                "display_name": "Brand",
                "short_desc": "Filter by brand",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter products by brand",
            },
            "ModifiedSince": <DataProviderOptionInfo>{
                "display_name": "Modified Since",
                "short_desc": "Filter by modification date",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Return products modified after this date/time",
            },
            "IncludeDeprecated": <DataProviderOptionInfo>{
                "display_name": "Include Deprecated",
                "short_desc": "Include deprecated products",
                "type": AbstractDataProviderType::get(BoolOrNothingType),
                "desc": "Include deprecated/inactive products in results",
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
        static Cin7CoreProductDataType recordType();
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
        return "products";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Cin7Core products data provider - list and search products";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for products
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> params;

        # Build query parameters from search options
        if (search_options.ID) {
            params.ID = search_options.ID;
        }
        if (search_options.SKU) {
            params.SKU = search_options.SKU;
        }
        if (search_options.Name) {
            params.Name = search_options.Name;
        }
        if (search_options.Category) {
            params.Category = search_options.Category;
        }
        if (search_options.Brand) {
            params.Brand = search_options.Brand;
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

        hash<auto> response = doGet("/Product", params);
        list<hash<auto>> records = response.Products ?? ();

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace Cin7CoreDataProvider
