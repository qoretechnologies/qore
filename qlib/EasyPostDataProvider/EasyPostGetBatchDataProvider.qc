# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for retrieving a batch
public class EasyPostGetBatchRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "batch_id": {
                "display_name": "Batch ID",
                "short_desc": "Batch ID (batch_xxx)",
                "desc": "Batch to retrieve",
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

#! Data provider for retrieving a batch by ID
public class EasyPostGetBatchDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-batch",
            "desc": "Retrieve an EasyPost batch by ID",
            "type": "EasyPostGetBatchDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostGetBatchRequestDataType RequestType();
        static EasyPostBatchType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "get-batch";
    }

    *string getDesc() {
        return "Retrieve an EasyPost batch by ID";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doGet(sprintf("batches/%s", req.batch_id));
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
