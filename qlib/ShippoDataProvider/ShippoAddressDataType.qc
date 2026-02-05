# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShippoDataProvider module definition

/*  ShippoAddressDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Shippo address data type
public class ShippoAddressDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "object_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Object ID",
                "short_desc": "Unique address identifier",
                "desc": "The unique identifier of the address object in Shippo",
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
                "desc": "Company name (optional)",
            },
            "street1": {
                "type": SoftStringOrNothingType,
                "display_name": "Street Address 1",
                "short_desc": "Primary street address",
                "desc": "First line of the street address",
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
                "type": SoftStringOrNothingType,
                "display_name": "City",
                "short_desc": "City name",
                "desc": "Name of the city",
            },
            "state": {
                "type": SoftStringOrNothingType,
                "display_name": "State/Province",
                "short_desc": "State or province code",
                "desc": "State, province, or region code (e.g., CA for California)",
            },
            "zip": {
                "type": SoftStringOrNothingType,
                "display_name": "ZIP/Postal Code",
                "short_desc": "Postal code",
                "desc": "ZIP or postal code",
            },
            "country": {
                "type": SoftStringOrNothingType,
                "display_name": "Country",
                "short_desc": "ISO country code",
                "desc": "Two-letter ISO 3166-1 alpha-2 country code (e.g., US, CA, GB)",
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
            "validate": {
                "type": SoftBoolOrNothingType,
                "display_name": "Validate",
                "short_desc": "Validate address",
                "desc": "Set to true to automatically validate the address",
            },
            "is_complete": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Complete",
                "short_desc": "Address is complete",
                "desc": "Indicates if the address has all required fields",
            },
            "validation_results": {
                "type": AutoHashOrNothingType,
                "display_name": "Validation Results",
                "short_desc": "Address validation results",
                "desc": "Results from address validation including any suggested corrections",
            },
            "object_created": {
                "type": SoftStringOrNothingType,
                "display_name": "Created",
                "short_desc": "Creation timestamp",
                "desc": "ISO 8601 timestamp when the address was created",
            },
            "object_updated": {
                "type": SoftStringOrNothingType,
                "display_name": "Updated",
                "short_desc": "Update timestamp",
                "desc": "ISO 8601 timestamp when the address was last updated",
            },
            "object_owner": {
                "type": SoftStringOrNothingType,
                "display_name": "Owner",
                "short_desc": "Owner email",
                "desc": "Email of the user who owns this address",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoAddressDataType") {
        addQoreFields(Fields);
    }
}

#! Shippo address data type for input (creating/updating addresses)
public class ShippoAddressInputDataType inherits HashDataType {
    private {
        #! Field definitions for input
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "Full name of the person",
                "desc": "First and last name of the person at this address",
                "required": True,
            },
            "company": {
                "type": SoftStringOrNothingType,
                "display_name": "Company",
                "short_desc": "Company or organization name",
                "desc": "Company name (optional)",
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
            "validate": {
                "type": SoftBoolOrNothingType,
                "display_name": "Validate",
                "short_desc": "Validate address",
                "desc": "Set to true to automatically validate the address",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoAddressInputDataType") {
        addQoreFields(Fields);
    }
}

} # namespace ShippoDataProvider
