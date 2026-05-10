# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for creating standalone insurance
public class EasyPostCreateInsuranceRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "to_address": {
                "display_name": "To Address",
                "short_desc": "Destination address",
                "desc": "Destination address (or `{id: adr_xxx}`)",
                "type": new EasyPostAddressInputType(AutoHashOrNothingType),
                "preselected": True,
            },
            "from_address": {
                "display_name": "From Address",
                "short_desc": "Origin address",
                "desc": "Origin address (or `{id: adr_xxx}`)",
                "type": new EasyPostAddressInputType(AutoHashOrNothingType),
                "preselected": True,
            },
            "tracking_code": {
                "display_name": "Tracking Code",
                "short_desc": "Carrier tracking code",
                "desc": "Carrier tracking code for the shipment being insured",
                "type": SoftStringType,
                "required": True,
                "example_value": "9405500000000000000000",
            },
            "amount": {
                "display_name": "Amount",
                "short_desc": "Amount to insure (string)",
                "desc": "Amount to insure as a string (e.g. `\"100.00\"`)",
                "type": SoftStringType,
                "required": True,
                "example_value": "100.00",
            },
            "carrier": {
                "display_name": "Carrier",
                "short_desc": "Carrier name",
                "desc": "Carrier name (e.g. `USPS`, `UPS`, `FedEx`). EasyPost will auto-detect from "
                    "`tracking_code` when omitted.",
                "type": SoftStringOrNothingType,
                "ref_data": "carrier_names",
            },
            "reference": {
                "display_name": "Reference",
                "short_desc": "Caller-supplied reference",
                "desc": "Caller-supplied reference",
                "type": SoftStringOrNothingType,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating standalone insurance
public class EasyPostCreateInsuranceDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-insurance",
            "desc": "Create a standalone insurance record",
            "type": "EasyPostCreateInsuranceDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostCreateInsuranceRequestDataType RequestType();
        static EasyPostInsuranceType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "create-insurance";
    }

    *string getDesc() {
        return "Create a standalone insurance record";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPostWrapped("insurances", WrapperKeyInsurance, req);
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
