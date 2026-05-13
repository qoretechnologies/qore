# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for retrieving a parcel
public class EasyPostGetParcelRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "parcel_id": {
                "display_name": "Parcel ID",
                "short_desc": "Parcel ID (prcl_xxx)",
                "desc": "Existing EasyPost parcel ID to retrieve",
                "type": SoftStringType,
                "required": True,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for retrieving a parcel by ID
public class EasyPostGetParcelDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-parcel",
            "desc": "Retrieve an EasyPost parcel by ID",
            "type": "EasyPostGetParcelDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostGetParcelRequestDataType RequestType();
        static EasyPostParcelType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "get-parcel";
    }

    *string getDesc() {
        return "Retrieve an EasyPost parcel by ID";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doGet(sprintf("parcels/%s", req.parcel_id));
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
