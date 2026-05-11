# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Sets the safety stock level for a variant/location pair (POST /inventory_safety_stock_levels)
public class KatanaSetInventorySafetyStockLevelDataProvider inherits KatanaDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "set-safety-stock-level",
            "desc": "Set the safety stock level for a variant at a location in Katana",
            "type": "KatanaSetInventorySafetyStockLevelDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
    }
    constructor(RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {}
    string getName() { return "set-safety-stock-level"; }
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KatanaInventoryThresholdRequestDataType();
    }
    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderType::get(AutoHashType);
    }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("inventory_safety_stock_levels", req);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Sets the reorder point for a variant/location pair (POST /inventory_reorder_points)
public class KatanaSetInventoryReorderPointDataProvider inherits KatanaDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "set-reorder-point",
            "desc": "Set the reorder point for a variant at a location in Katana",
            "type": "KatanaSetInventoryReorderPointDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
    }
    constructor(RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {}
    string getName() { return "set-reorder-point"; }
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KatanaInventoryThresholdRequestDataType();
    }
    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderType::get(AutoHashType);
    }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("inventory_reorder_points", req);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
