# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! SmartRate `time_in_transit` percentile breakdown (response-only)
public class EasyPostTimeInTransitType inherits HashDataType {
    public {
        const Fields = {
            "percentile_50": {"display_name": "Percentile 50", "type": SoftIntOrNothingType, "desc": "Estimated transit business days at the 50th percentile"},
            "percentile_75": {"display_name": "Percentile 75", "type": SoftIntOrNothingType, "desc": "Estimated transit business days at the 75th percentile"},
            "percentile_85": {"display_name": "Percentile 85", "type": SoftIntOrNothingType, "desc": "Estimated transit business days at the 85th percentile"},
            "percentile_90": {"display_name": "Percentile 90", "type": SoftIntOrNothingType, "desc": "Estimated transit business days at the 90th percentile"},
            "percentile_95": {"display_name": "Percentile 95", "type": SoftIntOrNothingType, "desc": "Estimated transit business days at the 95th percentile"},
            "percentile_97": {"display_name": "Percentile 97", "type": SoftIntOrNothingType, "desc": "Estimated transit business days at the 97th percentile"},
            "percentile_99": {"display_name": "Percentile 99", "type": SoftIntOrNothingType, "desc": "Estimated transit business days at the 99th percentile"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

#! SmartRate object: a Rate enriched with `time_in_transit` percentile data
public class EasyPostSmartRateType inherits HashDataType {
    public {
        const Fields = {
            "id": {"display_name": "ID", "type": SoftStringOrNothingType, "desc": "Rate ID (`rate_xxx`)"},
            "object": {"display_name": "Object Type", "type": SoftStringOrNothingType, "desc": "Always `Rate`"},
            "mode": {"display_name": "Mode", "type": SoftStringOrNothingType, "desc": "`production` or `test`"},
            "service": {"display_name": "Service", "type": SoftStringOrNothingType, "desc": "Carrier service code"},
            "carrier": {"display_name": "Carrier", "type": SoftStringOrNothingType, "desc": "Carrier name"},
            "carrier_account_id": {"display_name": "Carrier Account ID", "type": SoftStringOrNothingType, "desc": "Carrier account ID (`ca_xxx`)"},
            "shipment_id": {"display_name": "Shipment ID", "type": SoftStringOrNothingType, "desc": "Shipment ID (`shp_xxx`)"},
            "rate": {"display_name": "Rate", "type": SoftStringOrNothingType, "desc": "Quoted price as a string"},
            "currency": {"display_name": "Currency", "type": SoftStringOrNothingType, "desc": "ISO currency code"},
            "list_rate": {"display_name": "List Rate", "type": SoftStringOrNothingType, "desc": "List price"},
            "list_currency": {"display_name": "List Currency", "type": SoftStringOrNothingType, "desc": "List rate currency"},
            "retail_rate": {"display_name": "Retail Rate", "type": SoftStringOrNothingType, "desc": "Retail price"},
            "retail_currency": {"display_name": "Retail Currency", "type": SoftStringOrNothingType, "desc": "Retail rate currency"},
            "delivery_days": {"display_name": "Delivery Days", "type": SoftIntOrNothingType, "desc": "Estimated delivery days"},
            "est_delivery_days": {"display_name": "Est. Delivery Days", "type": SoftIntOrNothingType, "desc": "Estimated delivery days"},
            "delivery_date": {"display_name": "Delivery Date", "type": SoftDateOrNothingType, "desc": "Estimated delivery date"},
            "delivery_date_guaranteed": {"display_name": "Delivery Date Guaranteed", "type": SoftBoolOrNothingType, "desc": "Whether the delivery date is guaranteed"},
            "time_in_transit": {"display_name": "Time In Transit", "type": new EasyPostTimeInTransitType(AutoHashOrNothingType), "desc": "Percentile breakdown of expected transit business days"},
            "created_at": {"display_name": "Created At", "type": SoftDateOrNothingType, "desc": "Creation timestamp"},
            "updated_at": {"display_name": "Updated At", "type": SoftDateOrNothingType, "desc": "Last-update timestamp"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

#! Request type for retrieving SmartRates for a shipment
public class EasyPostGetShipmentSmartratesRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "shipment_id": {
                "display_name": "Shipment ID",
                "short_desc": "Shipment ID (shp_xxx)",
                "desc": "Shipment to retrieve SmartRates for",
                "type": SoftStringType,
                "required": True,
                "ref_data": "shipments",
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for retrieving SmartRates (rates with `time_in_transit` percentiles) for a shipment
public class EasyPostGetShipmentSmartratesDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-shipment-smartrates",
            "desc": "Retrieve SmartRates (rates with time-in-transit percentiles) for a shipment",
            "type": "EasyPostGetShipmentSmartratesDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostGetShipmentSmartratesRequestDataType RequestType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "get-shipment-smartrates";
    }

    *string getDesc() {
        return "Retrieve SmartRates for a shipment";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        auto response = doGet(sprintf("shipments/%s/smartrate", req.shipment_id));
        # API returns { "result": [<rate>, ...] }; unwrap.
        return response.result ?? response;
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return new ListDataType("EasyPostShipmentSmartRateList", new EasyPostSmartRateType(), True);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace EasyPostDataProvider
