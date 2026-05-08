# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for adding shipments to a batch
public class EasyPostBatchAddShipmentsRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "batch_id": {
                "display_name": "Batch ID",
                "short_desc": "Batch ID (batch_xxx)",
                "desc": "Batch to add shipments to",
                "type": SoftStringType,
                "required": True,
                "preselected": True,
            },
            "shipments": {
                "display_name": "Shipments",
                "short_desc": "Shipments to add (each {id: shp_xxx})",
                "desc": "List of shipment references to add to the batch (each `{id: shp_xxx}`)",
                "type": new ListDataType("EasyPostBatchAddShipmentList", new EasyPostIdRefType(), False),
                "required": True,
                "preselected": True,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for adding shipments to a batch
public class EasyPostBatchAddShipmentsDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "batch-add-shipments",
            "desc": "Add shipments to an existing batch",
            "type": "EasyPostBatchAddShipmentsDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostBatchAddShipmentsRequestDataType RequestType();
        static EasyPostBatchType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "batch-add-shipments";
    }

    *string getDesc() {
        return "Add shipments to an existing batch";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost(sprintf("batches/%s/add_shipments", req.batch_id), {"shipments": req.shipments});
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
