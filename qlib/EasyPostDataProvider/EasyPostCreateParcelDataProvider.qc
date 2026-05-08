# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for creating a parcel
public class EasyPostCreateParcelRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "length": {
                "display_name": "Length",
                "short_desc": "Length in inches",
                "desc": "Parcel length in inches",
                "type": SoftFloatOrNothingType,
                "preselected": True,
            },
            "width": {
                "display_name": "Width",
                "short_desc": "Width in inches",
                "desc": "Parcel width in inches",
                "type": SoftFloatOrNothingType,
                "preselected": True,
            },
            "height": {
                "display_name": "Height",
                "short_desc": "Height in inches",
                "desc": "Parcel height in inches",
                "type": SoftFloatOrNothingType,
                "preselected": True,
            },
            "weight": {
                "display_name": "Weight",
                "short_desc": "Weight in ounces",
                "desc": "Parcel weight in ounces",
                "type": SoftFloatType,
                "required": True,
                "preselected": True,
            },
            "predefined_package": {
                "display_name": "Predefined Package",
                "short_desc": "Carrier-defined package code",
                "desc": "Carrier-specific predefined package identifier",
                "type": SoftStringOrNothingType,
                "allowed_values": PredefinedPackageAllowedValues,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating a parcel
public class EasyPostCreateParcelDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-parcel",
            "desc": "Create a new EasyPost parcel",
            "type": "EasyPostCreateParcelDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostCreateParcelRequestDataType RequestType();
        static EasyPostParcelType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "create-parcel";
    }

    *string getDesc() {
        return "Create a new EasyPost parcel";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPostWrapped("parcels", WrapperKeyParcel, req);
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
