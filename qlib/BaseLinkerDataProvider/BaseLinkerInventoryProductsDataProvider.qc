# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerInventoryProductsDataProvider.qc BaseLinker inventory products collection
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! BaseLinker inventory products collection (`getInventoryProductsList`)
/** Uses page-number pagination (1000 per page). Supplying `product_id` triggers
    a single-fetch via `getInventoryProductsData`.
*/
public class BaseLinkerInventoryProductsDataProvider inherits BaseLinkerDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "inventory-products",
            "desc": "List and search BaseLinker inventory products "
                "(`getInventoryProductsList`). Supply `product_id` for single-fetch "
                "via `getInventoryProductsData`.",
            "type": "BaseLinkerInventoryProductsDataProvider",
            "has_record": True,
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        #! Action-provider children reachable via \c getChildProvider()
        const ChildMap = {
            "add-inventory-product":
                Class::forName("BaseLinkerDataProvider::BaseLinkerAddInventoryProductDataProvider"),
            "delete-inventory-product":
                Class::forName("BaseLinkerDataProvider::BaseLinkerDeleteInventoryProductDataProvider"),
        };

        const SearchOptions = {
            "inventory_id": <DataProviderOptionInfo>{
                "display_name": "Inventory ID",
                "short_desc": "BaseLinker inventory (catalog) identifier",
                "type": AbstractDataProviderType::get(SoftStringType),
                "desc": "The catalog to list products from.",
                "required": True,
                "ref_data": "inventories",
                "has_dependents": True,
                "on_change": ("refetch",),
            },
            "product_id": <DataProviderOptionInfo>{
                "display_name": "Product ID",
                "short_desc": "Fetch a single product by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "If supplied, only the matching product is returned (via "
                    "`getInventoryProductsData`).",
                "ref_data": "inventory_products",
                "depends_on": ("inventory_id",),
            },
            "filter_id": <DataProviderOptionInfo>{
                "display_name": "Filter ID",
                "short_desc": "Filter by product identifier",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Only return the product with this identifier.",
                "ref_data": "inventory_products",
                "depends_on": ("inventory_id",),
            },
            "filter_category_id": <DataProviderOptionInfo>{
                "display_name": "Filter Category",
                "short_desc": "Filter by category identifier",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Only return products in the given category.",
                "ref_data": "inventory_categories",
                "depends_on": ("inventory_id",),
            },
            "filter_ean": <DataProviderOptionInfo>{
                "display_name": "Filter EAN",
                "short_desc": "Filter by EAN",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Only return products matching this EAN.",
            },
            "filter_sku": <DataProviderOptionInfo>{
                "display_name": "Filter SKU",
                "short_desc": "Filter by SKU",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Only return products matching this SKU.",
            },
            "filter_name": <DataProviderOptionInfo>{
                "display_name": "Filter Name",
                "short_desc": "Filter by product name (substring match)",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Only return products whose name contains this substring.",
            },
            "filter_price_from": <DataProviderOptionInfo>{
                "display_name": "Filter Price From",
                "short_desc": "Minimum price",
                "type": AbstractDataProviderType::get(SoftFloatOrNothingType),
                "desc": "Return products priced at or above this value.",
            },
            "filter_price_to": <DataProviderOptionInfo>{
                "display_name": "Filter Price To",
                "short_desc": "Maximum price",
                "type": AbstractDataProviderType::get(SoftFloatOrNothingType),
                "desc": "Return products priced at or below this value.",
            },
            "filter_stock_from": <DataProviderOptionInfo>{
                "display_name": "Filter Stock From",
                "short_desc": "Minimum stock count",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Return products with stock at or above this value.",
            },
            "filter_stock_to": <DataProviderOptionInfo>{
                "display_name": "Filter Stock To",
                "short_desc": "Maximum stock count",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Return products with stock at or below this value.",
            },
            "filter_sort": <DataProviderOptionInfo>{
                "display_name": "Sort Order",
                "short_desc": "Result ordering expression",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Result sort expression accepted by BaseLinker "
                    "`getInventoryProductsList`. The API documents `id [ASC|DESC]`.",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "id ASC",  "display_name": "ID ascending"},
                    <AllowedValueInfo>{"value": "id DESC", "display_name": "ID descending"},
                ),
            },
            "filter_locations": <DataProviderOptionInfo>{
                "display_name": "Filter Locations",
                "short_desc": "Filter by warehouse location",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Return only products assigned to the supplied warehouse location "
                    "(see `locations` on @ref BaseLinkerAddInventoryProductRequestDataType for "
                    "the location-code format).",
            },
            "include_variants": <DataProviderOptionInfo>{
                "display_name": "Include Variants",
                "short_desc": "Include product variants in the output",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "When `True`, include product variants alongside their parent "
                    "products in the results.",
            },
            "page": <DataProviderOptionInfo>{
                "display_name": "Page",
                "short_desc": "Starting page number",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Page number for pagination (starting at 1).",
                "preselected": True,
                "default_value": 1,
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results across pages",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Overall maximum number of records returned.",
            },
        };
    }

    constructor(*RestClientIo::RestClientIo rest) : BaseLinkerDataProviderBase(rest) {
    }
    constructor(BaseLinkerRestClient::BaseLinkerRestConnection conn)
            : BaseLinkerDataProviderBase(conn) {
    }
    constructor(*hash<auto> options) : BaseLinkerDataProviderBase(options) {
    }

    string getName() { return "inventory-products"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return (new BaseLinkerInventoryProductDataType()).getFields();
    }

    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (!exists search_options.inventory_id) {
            throw "BASELINKER-SEARCH-ERROR", "`inventory_id` search option is required";
        }

        list<auto> records = ();
        string inventory_id = search_options.inventory_id;

        # Single-product fast path via getInventoryProductsData
        if (exists search_options.product_id) {
            hash<auto> response = callMethod("getInventoryProductsData", {
                "inventory_id": inventory_id,
                "products": (string(search_options.product_id),),
            });
            # products comes back as { pid: {...product hash...} }
            foreach string pid in (keys response.products ?? {}) {
                auto p = response.products{pid};
                if (p.typeCode() == NT_HASH) {
                    records += {"product_id": pid} + p;
                }
            }
            return new DefaultRecordIterator<hash<auto>>(new ListHashIterator(records), where_cond,
                search_options, (new BaseLinkerInventoryProductDataType()).getFields());
        }

        int remaining = search_options.limit ? search_options.limit.toInt() : 1000000;
        int page = search_options.page ? search_options.page.toInt() : 1;

        hash<auto> base_params = cast<hash<auto>>({"inventory_id": inventory_id});
        if (exists search_options.filter_id) {
            base_params.filter_id = search_options.filter_id;
        }
        if (exists search_options.filter_category_id) {
            base_params.filter_category_id = search_options.filter_category_id;
        }
        if (exists search_options.filter_ean) {
            base_params.filter_ean = search_options.filter_ean;
        }
        if (exists search_options.filter_sku) {
            base_params.filter_sku = search_options.filter_sku;
        }
        if (exists search_options.filter_name) {
            base_params.filter_name = search_options.filter_name;
        }
        if (exists search_options.filter_price_from) {
            base_params.filter_price_from = float(search_options.filter_price_from);
        }
        if (exists search_options.filter_price_to) {
            base_params.filter_price_to = float(search_options.filter_price_to);
        }
        if (exists search_options.filter_stock_from) {
            base_params.filter_stock_from = int(search_options.filter_stock_from);
        }
        if (exists search_options.filter_stock_to) {
            base_params.filter_stock_to = int(search_options.filter_stock_to);
        }
        if (exists search_options.filter_sort) {
            base_params.filter_sort = search_options.filter_sort;
        }
        if (exists search_options.filter_locations) {
            base_params.filter_locations = search_options.filter_locations;
        }
        if (exists search_options.include_variants) {
            base_params.include_variants = boolean(search_options.include_variants);
        }

        while (remaining > 0) {
            hash<auto> params = base_params + {"page": page};
            hash<auto> response = callMethod("getInventoryProductsList", params);
            *hash<auto> products = response.products;
            if (!products) {
                break;
            }
            int page_count = 0;
            foreach string pid in (keys products) {
                auto p = products{pid};
                hash<auto> rec = {"product_id": pid};
                if (p.typeCode() == NT_HASH) {
                    rec += p;
                }
                records += rec;
                ++page_count;
                --remaining;
                if (!remaining) {
                    break;
                }
            }
            if (page_count < InventoryProductsPageSize) {
                break;
            }
            ++page;
        }

        return new DefaultRecordIterator<hash<auto>>(new ListHashIterator(records), where_cond,
            search_options, (new BaseLinkerInventoryProductDataType()).getFields());
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BaseLinkerDataProvider
