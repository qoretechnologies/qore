# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for generating a batch scan form
public class EasyPostBatchScanFormRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "batch_id": {
                "display_name": "Batch ID",
                "short_desc": "Batch ID (batch_xxx)",
                "desc": "Batch to generate a scan form for",
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

#! Data provider for generating a scan form
public class EasyPostBatchScanFormDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "batch-scan-form",
            "desc": "Generate a scan form for all shipments in a batch",
            "type": "EasyPostBatchScanFormDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostBatchScanFormRequestDataType RequestType();
        static EasyPostBatchType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "batch-scan-form";
    }

    *string getDesc() {
        return "Generate a scan form for all shipments in a batch";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost(sprintf("batches/%s/scan_form", req.batch_id), {});
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
