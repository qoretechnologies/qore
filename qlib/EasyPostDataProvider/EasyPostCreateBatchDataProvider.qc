# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for creating a batch
public class EasyPostCreateBatchRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "shipments": {
                "display_name": "Shipments",
                "short_desc": "Shipments to include in the batch",
                "desc": "List of shipment references (each `{id: shp_xxx}`) to include in the new batch",
                "type": new ListDataType("EasyPostCreateBatchShipmentList", new EasyPostIdRefType(), True),
                "preselected": True,
                "ref_data": "shipments",
            },
            "reference": {
                "display_name": "Reference",
                "short_desc": "Caller-supplied reference",
                "desc": "Caller-supplied reference",
                "type": SoftStringOrNothingType,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating a batch
public class EasyPostCreateBatchDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-batch",
            "desc": "Create a new batch of shipments",
            "type": "EasyPostCreateBatchDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostCreateBatchRequestDataType RequestType();
        static EasyPostBatchType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "create-batch";
    }

    *string getDesc() {
        return "Create a new batch of shipments";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPostWrapped("batches", WrapperKeyBatch, req);
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
