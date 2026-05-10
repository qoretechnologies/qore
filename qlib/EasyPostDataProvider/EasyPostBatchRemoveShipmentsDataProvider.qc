# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for removing shipments from a batch
public class EasyPostBatchRemoveShipmentsRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "batch_id": {
                "display_name": "Batch ID",
                "short_desc": "Batch ID (batch_xxx)",
                "desc": "Batch to remove shipments from",
                "type": SoftStringType,
                "required": True,
                "ref_data": "batches",
            },
            "shipments": {
                "display_name": "Shipments",
                "short_desc": "Shipments to remove (each {id: shp_xxx})",
                "desc": "List of shipment references to remove from the batch (each `{id: shp_xxx}`)",
                "type": new ListDataType("EasyPostBatchRemoveShipmentList", new EasyPostIdRefType(), False),
                "required": True,
                "ref_data": "shipments",
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for removing shipments from a batch
public class EasyPostBatchRemoveShipmentsDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "batch-remove-shipments",
            "desc": "Remove shipments from an existing batch",
            "type": "EasyPostBatchRemoveShipmentsDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostBatchRemoveShipmentsRequestDataType RequestType();
        static EasyPostBatchType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "batch-remove-shipments";
    }

    *string getDesc() {
        return "Remove shipments from an existing batch";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost(sprintf("batches/%s/remove_shipments", req.batch_id), {"shipments": req.shipments});
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace EasyPostDataProvider
