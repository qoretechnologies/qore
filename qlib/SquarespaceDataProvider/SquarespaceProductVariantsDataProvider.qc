# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquarespaceProductVariantsDataProvider.qc Squarespace product variants collection data provider

/*  SquarespaceProductVariantsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquarespaceDataProvider module
public namespace SquarespaceDataProvider {

#! Squarespace product variants collection data provider
public class SquarespaceProductVariantsDataProvider inherits SquarespaceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "product-variants",
            "desc": "Squarespace product variants data provider for listing and searching "
                "product variants",
            "type": "SquarespaceProductVariantsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "product_id": <DataProviderOptionInfo>{
                "display_name": "Product ID",
                "short_desc": "Product ID to list variants for",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Product ID to list variants for (required for variant lookup)",
            },
            "variant_id": <DataProviderOptionInfo>{
                "display_name": "Variant ID",
                "short_desc": "Get a specific variant by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Get a specific variant by ID (requires product_id)",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };

        #! Child provider map
        const ChildMap = {
            "create": Class::forName("SquarespaceCreateProductVariantDataProvider"),
            "update": Class::forName("SquarespaceUpdateProductVariantDataProvider"),
            "delete": Class::forName("SquarespaceDeleteProductVariantDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(SquarespaceRestClient::SquarespaceRestClient rest) : SquarespaceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "product-variants";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Squarespace product variants data provider for listing and searching "
            "product variants";
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new SquarespaceProductVariantDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    /** Squarespace does not have a standalone variants endpoint; variants are nested
        within the product response. This provider fetches the product by product_id
        and returns its variants array, optionally filtering by variant_id.
    */
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (!search_options.product_id) {
            throw "SEARCH-OPTION-ERROR", "product_id is required to list product variants";
        }

        # Fetch the product to get its variants
        # Squarespace wraps responses in {"products": [...]}
        hash<auto> result = doGet("v2/commerce/products/" + search_options.product_id);
        hash<auto> product = result.products ? result.products[0] : result;
        list<hash<auto>> variants = product.variants ?? ();

        # If variant_id provided, filter to that specific variant
        if (search_options.variant_id) {
            list<hash<auto>> filtered = ();
            foreach hash<auto> variant in (variants) {
                if (variant.id == search_options.variant_id) {
                    filtered += variant;
                    break;
                }
            }
            variants = filtered;
        }

        # Apply limit if provided
        if (search_options.limit && variants.size() > search_options.limit) {
            variants = variants[0..search_options.limit - 1];
        }

        return new DefaultRecordIterator(new ListHashIterator(variants), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquarespaceDataProvider
