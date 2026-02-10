# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WaveDataProvider module definition

/*  WaveProductsDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the WaveDataProvider module
public namespace WaveDataProvider {

#! Wave products collection data provider
public class WaveProductsDataProvider inherits WaveDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "products",
            "desc": "Wave products data provider",
            "type": "WaveProductsDataProvider",
            "supports_read": True,
            "supports_create": True,
            "supports_native_search": True,
            "has_record": True,
            "search_options": SearchOptions,
            "create_options": CreateOptions,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "businessId": <DataProviderOptionInfo>{
                "display_name": "Business",
                "short_desc": "The business to list products for",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The ID of the business to list products for (required)",
            },
            "id": <DataProviderOptionInfo>{
                "display_name": "Product ID",
                "short_desc": "Get a specific product by ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "The ID of a specific product to retrieve",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of results",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };

        #! Create options
        const CreateOptions = {};
    }

    private {
        #! Record type
        static WaveProductDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(WaveRestClient::WaveRestClient rest) : WaveDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(WaveRestClient::WaveRestConnection conn) : WaveDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "products";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Wave products collection data provider";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for products
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # Get a single product by ID
        if (search_options.id) {
            *hash<auto> product;
            try {
                hash<auto> data = doGraphQL(GetProductQuery, {
                    "businessId": search_options.businessId,
                    "productId": search_options.id,
                });
                product = data.business.product;
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err == "GRAPHQL-ERROR") {
                    product = NOTHING;
                } else {
                    rethrow;
                }
            }
            if (!product) {
                return new DefaultRecordIterator(new ListHashIterator(()),
                    where_cond, search_options, recordType.getFields());
            }
            return new DefaultRecordIterator(new ListHashIterator((flattenEdges(product),)),
                where_cond, search_options, recordType.getFields());
        }

        hash<auto> vars = {"businessId": search_options.businessId};
        int pageSize = search_options.limit ?? DefaultPageSize;

        return new WaveRecordIterator(rest, ListProductsQuery, "business", "products",
            vars, pageSize, recordType.getFields());
    }

    #! Returns the record type for create operations
    private *hash<string, AbstractDataField> getCreateRecordTypeImpl(*hash<auto> search_options) {
        return (new WaveProductCreateDataType()).getFields();
    }

    #! Returns the record type for update operations
    private *hash<string, AbstractDataField> getUpdateRecordTypeImpl(*hash<auto> search_options) {
        return (new WaveProductUpdateDataType()).getFields();
    }

    #! Creates a new product
    private *hash<auto> createRecordImpl(hash<auto> rec, *hash<auto> create_options) {
        @assert(rec.businessId.val(), "businessId is required to create a product");
        @assert(rec.name.val(), "name is required to create a product");
        @debug(log(LoggerLevel::DEBUG, "createRecordImpl: creating product with name=%y businessId=%y", rec.name,
            rec.businessId));
        hash<auto> result = doMutation(ProductCreateMutation, {"input": rec}, "productCreate", "product");
        @debug(log(LoggerLevel::DEBUG, "createRecordImpl: created product id=%y", result.id));
        return flattenEdges(result);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace WaveDataProvider
