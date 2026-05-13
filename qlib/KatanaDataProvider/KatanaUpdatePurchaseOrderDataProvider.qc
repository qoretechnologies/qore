# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaUpdatePurchaseOrderDataProvider inherits KatanaDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing purchase order in Katana",
            "type": "KatanaUpdatePurchaseOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;

        static KatanaUpdatePurchaseOrderRequestDataType RequestType();

        #! Fields submittable only when `entity_type` is `outsourced`
        const OutsourcedOnlyFields = ("tracking_location_id",);
    }
    constructor(RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {}
    string getName() { return "update"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *AbstractDataProviderType getRequestTypeWithOptionsImpl(*hash<auto> options) {
        return KatanaUpdatePurchaseOrderDataProvider::buildRequestTypeForEntityType(
            options.entity_type);
    }
    private *AbstractDataProviderType getRequestTypeWithDataImpl(auto req) {
        *string entity_type = req.typeCode() == NT_HASH ? req.entity_type : NOTHING;
        return KatanaUpdatePurchaseOrderDataProvider::buildRequestTypeForEntityType(entity_type);
    }
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KatanaPurchaseOrderDataType();
    }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = req;
        int id = body.id;
        delete body.id;
        # entity_type is a UI-only controlling field for dynamic types — the API rejects it on update
        delete body.entity_type;
        return doPatch(sprintf("purchase_orders/%d", id), body);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Builds a request type that hides outsourced-only fields when entity_type != outsourced
    static AbstractDataProviderType buildRequestTypeForEntityType(*string entity_type) {
        hash<string, AbstractDataField> fields = RequestType.getFields();
        bool include_outsourced = (entity_type == "outsourced");
        HashDataType rv("KatanaPurchaseOrderUpdateDynamicRequest");
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
