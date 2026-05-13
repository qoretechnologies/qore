# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaCreatePurchaseOrderDataProvider inherits KatanaDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new purchase order in Katana",
            "type": "KatanaCreatePurchaseOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;

        static KatanaCreatePurchaseOrderRequestDataType RequestType();

        #! Fields submittable only when `entity_type` is `outsourced`
        const OutsourcedOnlyFields = ("tracking_location_id",);
    }
    constructor(RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {}
    string getName() { return "create"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *AbstractDataProviderType getRequestTypeWithOptionsImpl(*hash<auto> options) {
        return KatanaCreatePurchaseOrderDataProvider::buildRequestTypeForEntityType(
            options.entity_type);
    }
    private *AbstractDataProviderType getRequestTypeWithDataImpl(auto req) {
        *string entity_type = req.typeCode() == NT_HASH ? req.entity_type : NOTHING;
        return KatanaCreatePurchaseOrderDataProvider::buildRequestTypeForEntityType(entity_type);
    }
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KatanaPurchaseOrderDataType();
    }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("purchase_orders", req);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Builds a request type that hides outsourced-only fields when entity_type != outsourced
    static AbstractDataProviderType buildRequestTypeForEntityType(*string entity_type) {
        hash<string, AbstractDataField> fields = RequestType.getFields();
        bool include_outsourced = (entity_type == "outsourced");
        HashDataType rv("KatanaPurchaseOrderDynamicRequest");
        foreach string key in (keys fields) {
            if (!include_outsourced && inlist(key, OutsourcedOnlyFields)) {
                continue;
            }
            rv.addField(fields{key});
        }
        return rv;
    }
}

} # namespace KatanaDataProvider
