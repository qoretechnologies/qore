# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Allowed values for `predefined_package`
const PredefinedPackageAllowedValues = (
    <AllowedValueInfo>{"value": "Card", "desc": "Card"},
    <AllowedValueInfo>{"value": "Letter", "desc": "Letter"},
    <AllowedValueInfo>{"value": "Flat", "desc": "Flat"},
    <AllowedValueInfo>{"value": "FlatRateEnvelope", "desc": "USPS Flat Rate Envelope"},
    <AllowedValueInfo>{"value": "LegalFlatRateEnvelope", "desc": "USPS Legal Flat Rate Envelope"},
    <AllowedValueInfo>{"value": "PaddedFlatRateEnvelope", "desc": "USPS Padded Flat Rate Envelope"},
    <AllowedValueInfo>{"value": "SmallFlatRateBox", "desc": "USPS Small Flat Rate Box"},
    <AllowedValueInfo>{"value": "MediumFlatRateBox", "desc": "USPS Medium Flat Rate Box"},
    <AllowedValueInfo>{"value": "LargeFlatRateBox", "desc": "USPS Large Flat Rate Box"},
    <AllowedValueInfo>{"value": "Parcel", "desc": "Generic Parcel"},
    <AllowedValueInfo>{"value": "SoftPack", "desc": "Soft Pack"},
    <AllowedValueInfo>{"value": "RegionalRateBoxA", "desc": "USPS Regional Rate Box A"},
    <AllowedValueInfo>{"value": "RegionalRateBoxB", "desc": "USPS Regional Rate Box B"},
    <AllowedValueInfo>{"value": "FedExEnvelope", "desc": "FedEx Envelope"},
    <AllowedValueInfo>{"value": "FedExBox", "desc": "FedEx Box"},
    <AllowedValueInfo>{"value": "FedExPak", "desc": "FedEx Pak"},
    <AllowedValueInfo>{"value": "FedExTube", "desc": "FedEx Tube"},
    <AllowedValueInfo>{"value": "FedExSmallBox", "desc": "FedEx Small Box"},
    <AllowedValueInfo>{"value": "FedExMediumBox", "desc": "FedEx Medium Box"},
    <AllowedValueInfo>{"value": "FedExLargeBox", "desc": "FedEx Large Box"},
    <AllowedValueInfo>{"value": "FedEx10kgBox", "desc": "FedEx 10kg Box"},
    <AllowedValueInfo>{"value": "FedEx25kgBox", "desc": "FedEx 25kg Box"},
    <AllowedValueInfo>{"value": "UPSLetter", "desc": "UPS Letter"},
    <AllowedValueInfo>{"value": "UPSExpressBox", "desc": "UPS Express Box"},
    <AllowedValueInfo>{"value": "Pak", "desc": "Pak"},
    <AllowedValueInfo>{"value": "Tube", "desc": "Tube"},
    <AllowedValueInfo>{"value": "SmallExpressBox", "desc": "Small Express Box"},
    <AllowedValueInfo>{"value": "MediumExpressBox", "desc": "Medium Express Box"},
    <AllowedValueInfo>{"value": "LargeExpressBox", "desc": "Large Express Box"},
    <AllowedValueInfo>{"value": "JumboDocument", "desc": "Jumbo Document"},
    <AllowedValueInfo>{"value": "JumboParcel", "desc": "Jumbo Parcel"},
    <AllowedValueInfo>{"value": "Document", "desc": "Document"},
    <AllowedValueInfo>{"value": "DHLFlyer", "desc": "DHL Flyer"},
    <AllowedValueInfo>{"value": "Domestic", "desc": "Domestic"},
    <AllowedValueInfo>{"value": "ExpressDocument", "desc": "Express Document"},
    <AllowedValueInfo>{"value": "DHLExpressEnvelope", "desc": "DHL Express Envelope"},
    <AllowedValueInfo>{"value": "LargePackage", "desc": "Large Package"},
    <AllowedValueInfo>{"value": "MediumPackage", "desc": "Medium Package"},
    <AllowedValueInfo>{"value": "SmallPackage", "desc": "Small Package"},
);

#! EasyPost parcel input type (polymorphic: id or full parcel fields)
public class EasyPostParcelInputType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Parcel ID",
                "short_desc": "Existing parcel ID (e.g. prcl_xxx)",
                "desc": "If provided, references an existing EasyPost parcel; in that case the other "
                    "fields may be omitted.",
                "type": SoftStringOrNothingType,
            },
            "length": {
                "display_name": "Length",
                "short_desc": "Length in inches",
                "desc": "Parcel length in inches",
                "type": SoftFloatOrNothingType,
            },
            "width": {
                "display_name": "Width",
                "short_desc": "Width in inches",
                "desc": "Parcel width in inches",
                "type": SoftFloatOrNothingType,
            },
            "height": {
                "display_name": "Height",
                "short_desc": "Height in inches",
                "desc": "Parcel height in inches",
                "type": SoftFloatOrNothingType,
            },
            "weight": {
                "display_name": "Weight",
                "short_desc": "Weight in ounces",
                "desc": "Parcel weight in ounces",
                "type": SoftFloatOrNothingType,
            },
            "predefined_package": {
                "display_name": "Predefined Package",
                "short_desc": "Carrier-defined package code",
                "desc": "Carrier-specific predefined package identifier (e.g. `FedExEnvelope`, "
                    "`SmallFlatRateBox`)",
                "type": SoftStringOrNothingType,
                "allowed_values": PredefinedPackageAllowedValues,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

#! EasyPost parcel record (response type)
public class EasyPostParcelType inherits HashDataType {
    public {
        const Fields = {
            "id": {"display_name": "ID", "type": SoftStringOrNothingType, "desc": "Parcel ID (`prcl_xxx`)"},
            "object": {"display_name": "Object Type", "type": SoftStringOrNothingType, "desc": "Always `Parcel`"},
            "mode": {"display_name": "Mode", "type": SoftStringOrNothingType, "desc": "`production` or `test`"},
            "length": {"display_name": "Length", "type": SoftFloatOrNothingType, "desc": "Length in inches"},
            "width": {"display_name": "Width", "type": SoftFloatOrNothingType, "desc": "Width in inches"},
            "height": {"display_name": "Height", "type": SoftFloatOrNothingType, "desc": "Height in inches"},
            "weight": {"display_name": "Weight", "type": SoftFloatOrNothingType, "desc": "Weight in ounces"},
            "predefined_package": {"display_name": "Predefined Package", "type": SoftStringOrNothingType, "desc": "Carrier-specific predefined package identifier"},
            "created_at": {"display_name": "Created At", "type": SoftDateOrNothingType, "desc": "Creation timestamp"},
            "updated_at": {"display_name": "Updated At", "type": SoftDateOrNothingType, "desc": "Last-update timestamp"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace EasyPostDataProvider
