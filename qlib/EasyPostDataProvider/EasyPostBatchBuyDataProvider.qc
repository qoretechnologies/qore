# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for buying a batch
public class EasyPostBatchBuyRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "batch_id": {
                "display_name": "Batch ID",
                "short_desc": "Batch ID (batch_xxx)",
                "desc": "Batch to buy (purchase labels for all shipments)",
                "type": SoftStringType,
                "required": True,
                "ref_data": "batches",
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for buying a batch
public class EasyPostBatchBuyDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "batch-buy",
            "desc": "Buy a batch (purchase labels for all shipments in the batch)",
            "type": "EasyPostBatchBuyDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostBatchBuyRequestDataType RequestType();
        static EasyPostBatchType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "batch-buy";
    }

    *string getDesc() {
        return "Buy a batch (purchase labels for all shipments in the batch)";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost(sprintf("batches/%s/buy", req.batch_id), {});
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
