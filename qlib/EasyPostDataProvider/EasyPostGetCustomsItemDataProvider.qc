# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for retrieving a customs item
public class EasyPostGetCustomsItemRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "customs_item_id": {
                "display_name": "Customs Item ID",
                "short_desc": "Customs item ID (cstitem_xxx)",
                "desc": "Customs item to retrieve",
                "type": SoftStringType,
                "required": True,
                "preselected": True,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for retrieving a customs item
public class EasyPostGetCustomsItemDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-customs-item",
            "desc": "Retrieve a customs item by ID",
            "type": "EasyPostGetCustomsItemDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostGetCustomsItemRequestDataType RequestType();
        static EasyPostCustomsItemType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "get-customs-item";
    }

    *string getDesc() {
        return "Retrieve a customs item by ID";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doGet(sprintf("customs_items/%s", req.customs_item_id));
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
