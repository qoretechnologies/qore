# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Allowed values for batch label format
const BatchLabelFormatAllowedValues = (
    <AllowedValueInfo>{"value": "PDF", "desc": "PDF"},
    <AllowedValueInfo>{"value": "ZPL", "desc": "Zebra ZPL"},
    <AllowedValueInfo>{"value": "EPL2", "desc": "Eltron EPL2"},
);

#! Request type for generating a combined batch label
public class EasyPostBatchLabelRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "batch_id": {
                "display_name": "Batch ID",
                "short_desc": "Batch ID (batch_xxx)",
                "desc": "Batch to generate combined labels for",
                "type": SoftStringType,
                "required": True,
                "preselected": True,
            },
            "file_format": {
                "display_name": "File Format",
                "short_desc": "Combined label file format",
                "desc": "Output format for the combined label file",
                "type": SoftStringType,
                "required": True,
                "preselected": True,
                "allowed_values": BatchLabelFormatAllowedValues,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for generating combined batch labels
public class EasyPostBatchLabelDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "batch-label",
            "desc": "Generate a combined label file for all shipments in a batch",
            "type": "EasyPostBatchLabelDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostBatchLabelRequestDataType RequestType();
        static EasyPostBatchType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "batch-label";
    }

    *string getDesc() {
        return "Generate a combined label file for all shipments in a batch";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost(sprintf("batches/%s/label", req.batch_id), {"file_format": req.file_format});
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
