# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaVariantsDataProvider inherits KatanaDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "variants", "desc": "Katana variants collection",
            "type": "KatanaVariantsDataProvider",
            "has_record": True, "supports_read": True, "supports_native_search": True,
            "supports_children": True, "children_can_support_apis": True,
            "constructor_options": ConstructorOptions, "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = {
            "id": <DataProviderOptionInfo>{
                "display_name": "Variant ID", "short_desc": "Get by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Variant ID to retrieve"},
            "sku": <DataProviderOptionInfo>{
                "display_name": "SKU", "short_desc": "Filter by SKU",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by variant SKU"},
            "product_id": <DataProviderOptionInfo>{
                "display_name": "Product ID", "short_desc": "Filter by product",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Filter by parent product ID", "ref_data": "products"},
            "material_id": <DataProviderOptionInfo>{
                "display_name": "Material ID", "short_desc": "Filter by material",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Filter by parent material ID", "ref_data": "materials"},
            "internal_barcode": <DataProviderOptionInfo>{
                "display_name": "Internal Barcode",
                "short_desc": "Filter by internal barcode",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by internal barcode"},
            "registered_barcode": <DataProviderOptionInfo>{
                "display_name": "Registered Barcode",
                "short_desc": "Filter by registered barcode",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by registered barcode"},
        } + CommonSearchOptions;
        const ChildMap = {
            "create": Class::forName("KatanaCreateVariantDataProvider"),
            "update": Class::forName("KatanaUpdateVariantDataProvider"),
            "delete": Class::forName("KatanaDeleteVariantDataProvider"),
        };
    }
    constructor(RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {}
    string getName() { return "variants"; }
    *string getDesc() { return "Katana variants collection"; }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) { return cls.newObject(rest); }
    }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new KatanaVariantRecordDataType().getFields();
    }
    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        return runListSearch("variants", where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
