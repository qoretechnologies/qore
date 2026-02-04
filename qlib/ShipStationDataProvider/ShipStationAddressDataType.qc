# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationAddressDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShipStationDataProvider module
public namespace ShipStationDataProvider {

#! ShipStation address record type
/** Used for billing and shipping addresses in orders, warehouses, and other entities.
*/
public class ShipStationAddressDataType inherits HashDataType {
    private {
        #! Field definitions for ShipStation addresses
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "Recipient name",
                "desc": "The name of the person or company at this address",
                "example_value": "John Smith",
            },
            "company": {
                "type": StringOrNothingType,
                "display_name": "Company",
                "short_desc": "Company name",
                "desc": "The company name at this address",
                "example_value": "Acme Corp",
            },
            "street1": {
                "type": StringType,
                "display_name": "Street Address 1",
                "short_desc": "Primary street address",
                "desc": "The first line of the street address",
                "example_value": "123 Main Street",
            },
            "street2": {
                "type": StringOrNothingType,
                "display_name": "Street Address 2",
                "short_desc": "Secondary street address",
                "desc": "The second line of the street address (apartment, suite, etc.)",
                "example_value": "Suite 100",
            },
            "street3": {
                "type": StringOrNothingType,
                "display_name": "Street Address 3",
                "short_desc": "Additional street address",
                "desc": "The third line of the street address (if needed)",
            },
            "city": {
                "type": StringType,
                "display_name": "City",
                "short_desc": "City name",
                "desc": "The city name",
                "example_value": "Austin",
            },
            "state": {
                "type": StringType,
                "display_name": "State/Province",
                "short_desc": "State or province code",
                "desc": "The state or province code (e.g., TX, CA, ON)",
                "example_value": "TX",
            },
            "postalCode": {
                "type": StringType,
                "display_name": "Postal Code",
                "short_desc": "ZIP or postal code",
                "desc": "The ZIP code (US) or postal code",
                "example_value": "78701",
            },
            "country": {
                "type": StringType,
                "display_name": "Country",
                "short_desc": "Country code",
                "desc": "The ISO 2-letter country code",
                "example_value": "US",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Phone number",
                "desc": "The phone number at this address",
                "example_value": "+1-555-123-4567",
            },
            "residential": {
                "type": BoolOrNothingType,
                "display_name": "Residential",
                "short_desc": "Is residential address",
                "desc": "Whether this is a residential address (affects shipping rates)",
                "example_value": True,
            },
            "addressVerified": {
                "type": StringOrNothingType,
                "display_name": "Address Verified",
                "short_desc": "Verification status",
                "desc": "The address verification status from ShipStation",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ShipStationDataProvider
