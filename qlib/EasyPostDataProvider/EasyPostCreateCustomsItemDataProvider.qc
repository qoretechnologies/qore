# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for creating a customs item
public class EasyPostCreateCustomsItemRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "description": {
                "display_name": "Description",
                "short_desc": "Item description",
                "desc": "Description of the item being shipped",
                "type": SoftStringType,
                "required": True,
            },
            "quantity": {
                "display_name": "Quantity",
                "short_desc": "Number of units",
                "desc": "Number of units being shipped",
                "type": SoftIntType,
                "required": True,
            },
            "value": {
                "display_name": "Value",
                "short_desc": "Total value as a string",
                "desc": "Total value of all items as a string (e.g. `\"12.50\"`)",
                "type": SoftStringType,
                "required": True,
            },
            "weight": {
                "display_name": "Weight",
                "short_desc": "Weight in ounces",
                "desc": "Total weight in ounces",
                "type": SoftFloatType,
                "required": True,
            },
            "hs_tariff_number": {
                "display_name": "HS Tariff Number",
                "short_desc": "HS tariff number",
                "desc": "Six-digit Harmonized System tariff number",
                "type": SoftStringType,
                "required": True,
            },
            "origin_country": {
                "display_name": "Origin Country",
                "short_desc": "ISO 2-letter origin country",
                "desc": "ISO 2-letter country code of origin",
                "type": SoftStringType,
                "required": True,
            },
            "code": {
                "display_name": "Code",
                "short_desc": "Internal item code",
                "desc": "Internal reference code",
                "type": SoftStringOrNothingType,
            },
            "currency": {
                "display_name": "Currency",
                "short_desc": "ISO currency code",
                "desc": "ISO 4217 currency code",
                "type": SoftStringOrNothingType,
            },
            "manufacturer": {
                "display_name": "Manufacturer",
                "short_desc": "Manufacturer name",
                "desc": "Manufacturer of the item",
                "type": SoftStringOrNothingType,
            },
            "eccn": {
                "display_name": "ECCN",
                "short_desc": "Export Control Classification Number",
                "desc": "Export Control Classification Number",
                "type": SoftStringOrNothingType,
            },
            "printed_commodity_identifier": {
                "display_name": "Printed Commodity Identifier",
                "short_desc": "Identifier printed on label",
                "desc": "Optional identifier printed on customs forms",
                "type": SoftStringOrNothingType,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating a customs item
public class EasyPostCreateCustomsItemDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-customs-item",
            "desc": "Create a customs item",
            "type": "EasyPostCreateCustomsItemDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostCreateCustomsItemRequestDataType RequestType();
        static EasyPostCustomsItemType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "create-customs-item";
    }

    *string getDesc() {
        return "Create a customs item";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPostWrapped("customs_items", WrapperKeyCustomsItem, req);
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
