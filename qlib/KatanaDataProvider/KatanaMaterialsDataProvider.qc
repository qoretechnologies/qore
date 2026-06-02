# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaMaterialsDataProvider inherits KatanaDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "materials", "desc": "Katana materials collection",
            "type": "KatanaMaterialsDataProvider",
            "has_record": True, "supports_read": True, "supports_native_search": True,
            "supports_children": True, "children_can_support_apis": True,
            "constructor_options": ConstructorOptions, "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = {
            "id": <DataProviderOptionInfo>{
                "display_name": "Material ID", "short_desc": "Get by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Material ID to retrieve"},
            "name": <DataProviderOptionInfo>{
                "display_name": "Name", "short_desc": "Filter by name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by material name"},
            "is_sellable": <DataProviderOptionInfo>{
                "display_name": "Is Sellable", "short_desc": "Sellable filter",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "Filter by `is_sellable`"},
            "is_purchasable": <DataProviderOptionInfo>{
                "display_name": "Is Purchasable", "short_desc": "Purchasable filter",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "Filter by `is_purchasable`"},
            "category_name": <DataProviderOptionInfo>{
                "display_name": "Category Name", "short_desc": "Filter by category",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by category name"},
            "default_supplier_id": <DataProviderOptionInfo>{
                "display_name": "Default Supplier ID",
                "short_desc": "Default supplier filter",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Filter by default supplier ID"},
            "batch_tracked": <DataProviderOptionInfo>{
                "display_name": "Batch Tracked", "short_desc": "Batch tracking filter",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "Filter by batch tracking flag"},
            "serial_tracked": <DataProviderOptionInfo>{
                "display_name": "Serial Tracked", "short_desc": "Serial tracking filter",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "Filter by serial tracking flag"},
        } + CommonSearchOptions;
        const ChildMap = {
            "create": Class::forName("KatanaCreateMaterialDataProvider"),
            "update": Class::forName("KatanaUpdateMaterialDataProvider"),
            "delete": Class::forName("KatanaDeleteMaterialDataProvider"),
        };
    }
    constructor(RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {}
    string getName() { return "materials"; }
    *string getDesc() { return "Katana materials collection"; }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) { return cls.newObject(rest); }
    }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new KatanaMaterialDataType().getFields();
    }
    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        return runListSearch("materials", where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
