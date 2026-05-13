# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaSalesOrdersDataProvider inherits KatanaDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "sales-orders", "desc": "Katana sales orders collection",
            "type": "KatanaSalesOrdersDataProvider",
            "has_record": True, "supports_read": True, "supports_native_search": True,
            "supports_children": True, "children_can_support_apis": True,
            "constructor_options": ConstructorOptions, "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = {
            "id": <DataProviderOptionInfo>{
                "display_name": "Order ID", "short_desc": "Get by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Sales order ID to retrieve"},
            "status": <DataProviderOptionInfo>{
                "display_name": "Status", "short_desc": "Filter by status",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by sales order status",
                "allowed_values": SoStatusUpdateAllowed},
            "customer_id": <DataProviderOptionInfo>{
                "display_name": "Customer ID", "short_desc": "Filter by customer",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Filter by customer ID", "ref_data": "customers"},
            "location_id": <DataProviderOptionInfo>{
                "display_name": "Location ID", "short_desc": "Filter by location",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Filter by location ID", "ref_data": "locations"},
            "order_no": <DataProviderOptionInfo>{
                "display_name": "Order Number", "short_desc": "Filter by order number",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by order number"},
            "source": <DataProviderOptionInfo>{
                "display_name": "Source", "short_desc": "Filter by source",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by sales order source"},
            "ecommerce_store_name": <DataProviderOptionInfo>{
                "display_name": "Ecommerce Store Name",
                "short_desc": "Filter by ecommerce store",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by the ecommerce store name"},
            "ecommerce_order_id": <DataProviderOptionInfo>{
                "display_name": "Ecommerce Order ID",
                "short_desc": "Filter by ecommerce order ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by the ecommerce platform's order identifier"},
            "ecommerce_order_type": <DataProviderOptionInfo>{
                "display_name": "Ecommerce Order Type",
                "short_desc": "Filter by ecommerce order type",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by the ecommerce order type"},
            "currency": <DataProviderOptionInfo>{
                "display_name": "Currency",
                "short_desc": "Filter by currency",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by ISO 4217 currency code (e.g. `USD`, `EUR`)"},
            "invoicing_status": <DataProviderOptionInfo>{
                "display_name": "Invoicing Status",
                "short_desc": "Filter by invoicing status",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by sales order invoicing status"},
            "product_availability": <DataProviderOptionInfo>{
                "display_name": "Product Availability",
                "short_desc": "Filter by product availability",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by product availability status",
                "allowed_values": SoProductAvailabilityAllowed},
            "ingredient_availability": <DataProviderOptionInfo>{
                "display_name": "Ingredient Availability",
                "short_desc": "Filter by ingredient availability",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by ingredient availability status",
                "allowed_values": SoIngredientAvailabilityAllowed},
            "production_status": <DataProviderOptionInfo>{
                "display_name": "Production Status",
                "short_desc": "Filter by production status",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by production status",
                "allowed_values": SoProductionStatusAllowed},
        } + CommonSearchOptions;
        const ChildMap = {
            "create": Class::forName("KatanaCreateSalesOrderDataProvider"),
            "update": Class::forName("KatanaUpdateSalesOrderDataProvider"),
            "delete": Class::forName("KatanaDeleteSalesOrderDataProvider"),
        };
    }
    constructor(RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {}
    string getName() { return "sales-orders"; }
    *string getDesc() { return "Katana sales orders collection"; }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) { return cls.newObject(rest); }
    }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new KatanaSalesOrderDataType().getFields();
    }
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        return runListSearch("sales_orders", where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
