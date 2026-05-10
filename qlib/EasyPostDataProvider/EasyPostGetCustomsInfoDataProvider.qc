# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for retrieving customs info
public class EasyPostGetCustomsInfoRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "customs_info_id": {
                "display_name": "Customs Info ID",
                "short_desc": "Customs info ID (cstinfo_xxx)",
                "desc": "Customs info ID to retrieve",
                "type": SoftStringType,
                "required": True,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for retrieving customs info
public class EasyPostGetCustomsInfoDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-customs-info",
            "desc": "Retrieve a customs info object by ID",
            "type": "EasyPostGetCustomsInfoDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostGetCustomsInfoRequestDataType RequestType();
        static EasyPostCustomsInfoType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "get-customs-info";
    }

    *string getDesc() {
        return "Retrieve a customs info object by ID";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doGet(sprintf("customs_infos/%s", req.customs_info_id));
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
