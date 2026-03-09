# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShippoDataProvider module definition

/*  ShippoValidateAddressDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#! Contains all public definitions in the ShippoDataProvider module
public namespace ShippoDataProvider {

#! Data provider for validating addresses
/** Creates an address with validation enabled and returns the validation results.
    Shippo validates addresses by creating them with validate=true.
*/
public class ShippoValidateAddressDataProvider inherits ShippoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "validate-address",
            "desc": "Validates a shipping address using Shippo's address validation service",
            "type": "ShippoValidateAddressDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static ShippoValidateAddressRequestDataType RequestType();

        #! Response type
        static ShippoAddressValidationResponseDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(ShippoRestClient::ShippoRestClient rest) : ShippoDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ShippoRestClient::ShippoRestConnection conn) : ShippoDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : ShippoDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "validate-address";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Validates a shipping address using Shippo's address validation service";
    }

    #! Makes a request and returns the response
    /** @param req the request
        @param request_options the request options (ignored)

        @return the response to the request with validation results
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {
            "name": req.name,
            "street1": req.street1,
            "city": req.city,
            "state": req.state,
            "zip": req.zip,
            "country": req.country,
            "validate": True,  # Always validate
        };

        # Optional fields
        if (exists req.company) {
            body.company = req.company;
        }
        if (exists req.street2) {
            body.street2 = req.street2;
        }
        if (exists req.street3) {
            body.street3 = req.street3;
        }
        if (exists req.phone) {
            body.phone = req.phone;
        }
        if (exists req.email) {
            body.email = req.email;
        }
        if (exists req.is_residential) {
            body.is_residential = req.is_residential;
        }

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: validating address with body: %y", body));
        hash<auto> response = doPost("/addresses", body);
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: address validated, is_complete=%y", response.is_complete));
        return response;
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Request data type for validating an address
public class ShippoValidateAddressRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "Full name of the person",
                "desc": "First and last name of the person at this address",
                "required": True,
            },
            "street1": {
                "type": StringType,
                "display_name": "Street Address 1",
                "short_desc": "Primary street address",
                "desc": "First line of the street address",
                "required": True,
            },
            "street2": {
                "type": SoftStringOrNothingType,
                "display_name": "Street Address 2",
                "short_desc": "Secondary street address",
                "desc": "Second line of the street address (apartment, suite, etc.)",
            },
            "street3": {
                "type": SoftStringOrNothingType,
                "display_name": "Street Address 3",
                "short_desc": "Additional street address",
                "desc": "Third line of the street address (optional)",
            },
            "city": {
                "type": StringType,
                "display_name": "City",
                "short_desc": "City name",
                "desc": "Name of the city",
                "required": True,
            },
            "state": {
                "type": StringType,
                "display_name": "State/Province",
                "short_desc": "State or province code",
                "desc": "State, province, or region code (e.g., CA for California)",
                "required": True,
            },
            "zip": {
                "type": StringType,
                "display_name": "ZIP/Postal Code",
                "short_desc": "Postal code",
                "desc": "ZIP or postal code",
                "required": True,
            },
            "country": {
                "type": StringType,
                "display_name": "Country",
                "short_desc": "ISO country code",
                "desc": "Two-letter ISO 3166-1 alpha-2 country code (e.g., US, CA, GB)",
                "required": True,
            },
            "company": {
                "type": SoftStringOrNothingType,
                "display_name": "Company",
                "short_desc": "Company or organization name",
                "desc": "Company name (optional)",
            },
            "phone": {
                "type": SoftStringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Phone number",
                "desc": "Phone number including country code",
            },
            "email": {
                "type": SoftStringOrNothingType,
                "display_name": "Email",
                "short_desc": "Email address",
                "desc": "Email address for shipment notifications",
            },
            "is_residential": {
                "type": SoftBoolOrNothingType,
                "display_name": "Residential",
                "short_desc": "Is residential address",
                "desc": "Indicates whether the address is a residential location",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoValidateAddressRequestDataType") {
        addQoreFields(Fields);
    }
}

#! Response data type for address validation
public class ShippoAddressValidationResponseDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "object_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Object ID",
                "short_desc": "Unique address identifier",
                "desc": "The unique identifier of the validated address object in Shippo",
            },
            "is_complete": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Complete",
                "short_desc": "Address is complete",
                "desc": "Indicates if the address has all required fields and is deliverable",
            },
            "validation_results": {
                "type": new ShippoValidationResultsDataType(AutoHashOrNothingType),
                "display_name": "Validation Results",
                "short_desc": "Address validation results",
                "desc": "Detailed validation results including `is_valid` flag and `messages` "
                    "array with correction details",
            },
            "name": {
                "type": SoftStringOrNothingType,
                "display_name": "Name",
                "short_desc": "Full name of the person",
                "desc": "First and last name of the person at this address",
            },
            "company": {
                "type": SoftStringOrNothingType,
                "display_name": "Company",
                "short_desc": "Company or organization name",
                "desc": "Company name",
            },
            "street1": {
                "type": SoftStringOrNothingType,
                "display_name": "Street Address 1",
                "short_desc": "Primary street address",
                "desc": "First line of the street address (possibly corrected)",
            },
            "street2": {
                "type": SoftStringOrNothingType,
                "display_name": "Street Address 2",
                "short_desc": "Secondary street address",
                "desc": "Second line of the street address",
            },
            "city": {
                "type": SoftStringOrNothingType,
                "display_name": "City",
                "short_desc": "City name",
                "desc": "Name of the city (possibly corrected)",
            },
            "state": {
                "type": SoftStringOrNothingType,
                "display_name": "State/Province",
                "short_desc": "State or province code",
                "desc": "State, province, or region code (possibly corrected)",
            },
            "zip": {
                "type": SoftStringOrNothingType,
                "display_name": "ZIP/Postal Code",
                "short_desc": "Postal code",
                "desc": "ZIP or postal code (possibly corrected/extended)",
            },
            "country": {
                "type": SoftStringOrNothingType,
                "display_name": "Country",
                "short_desc": "ISO country code",
                "desc": "Two-letter ISO country code",
            },
            "is_residential": {
                "type": SoftBoolOrNothingType,
                "display_name": "Residential",
                "short_desc": "Is residential address",
                "desc": "Indicates whether the address is a residential location",
            },
            "object_created": {
                "type": SoftStringOrNothingType,
                "display_name": "Created",
                "short_desc": "Creation timestamp",
                "desc": "ISO 8601 timestamp when the address was created",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoAddressValidationResponseDataType") {
        addQoreFields(Fields);
    }
}

} # namespace ShippoDataProvider
