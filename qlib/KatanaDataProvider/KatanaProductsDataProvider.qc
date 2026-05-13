# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaProductsDataProvider inherits KatanaDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "products",
            "desc": "Katana products collection data provider",
            "type": "KatanaProductsDataProvider",
            "has_record": True,
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = {
            "id": <DataProviderOptionInfo>{
                "display_name": "Product ID", "short_desc": "Get a specific product by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Product ID to retrieve",
            },
            "name": <DataProviderOptionInfo>{
                "display_name": "Name", "short_desc": "Filter by name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by product name",
            },
            "is_sellable": <DataProviderOptionInfo>{
                "display_name": "Is Sellable", "short_desc": "Filter by sellable flag",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "Filter by `is_sellable` flag",
            },
            "is_producible": <DataProviderOptionInfo>{
                "display_name": "Is Producible", "short_desc": "Filter by producible flag",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "Filter by `is_producible` flag",
            },
            "is_purchasable": <DataProviderOptionInfo>{
                "display_name": "Is Purchasable", "short_desc": "Filter by purchasable flag",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "Filter by `is_purchasable` flag",
            },
            "category_name": <DataProviderOptionInfo>{
                "display_name": "Category Name", "short_desc": "Filter by category",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by category name",
            },
            "default_supplier_id": <DataProviderOptionInfo>{
                "display_name": "Default Supplier ID",
                "short_desc": "Filter by default supplier",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Filter by default supplier ID",
            },
            "batch_tracked": <DataProviderOptionInfo>{
                "display_name": "Batch Tracked",
                "short_desc": "Filter by batch tracking",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "Filter by batch tracking flag",
            },
            "serial_tracked": <DataProviderOptionInfo>{
                "display_name": "Serial Tracked",
                "short_desc": "Filter by serial tracking",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "Filter by serial tracking flag",
            },
            "uom": <DataProviderOptionInfo>{
                "display_name": "UoM",
                "short_desc": "Filter by unit of measure",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by unit of measure (e.g. `pcs`, `kg`)",
            },
            "is_auto_assembly": <DataProviderOptionInfo>{
                "display_name": "Is Auto Assembly",
                "short_desc": "Filter by auto-assembly flag",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "Filter by the `is_auto_assembly` flag",
            },
            "include_archived": <DataProviderOptionInfo>{
                "display_name": "Include Archived",
                "short_desc": "Include archived products",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "If `True`, includes archived products in the response",
            },
        } + CommonSearchOptions;
        const ChildMap = {
            "create": Class::forName("KatanaCreateProductDataProvider"),
            "update": Class::forName("KatanaUpdateProductDataProvider"),
            "delete": Class::forName("KatanaDeleteProductDataProvider"),
        };
    }

    constructor(RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {}

    string getName() { return "products"; }
    *string getDesc() { return "Katana products collection"; }

    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) { return cls.newObject(rest); }
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new KatanaProductDataType().getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        return runListSearch("products", where_cond, search_options);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
