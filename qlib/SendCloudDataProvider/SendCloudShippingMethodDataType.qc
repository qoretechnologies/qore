# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudShippingMethodDataType.qc SendCloud shipping method response data types

/*  SendCloudShippingMethodDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SendCloudDataProvider module
public namespace SendCloudDataProvider {

#! A country entry inside a shipping-method payload (with price and lead time)
public class SendCloudShippingMethodCountryDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftIntOrNothingType,
                "display_name": "Country ID",
                "short_desc": "SendCloud country ID",
                "desc": "SendCloud's internal numeric ID for this country",
            },
            "name": {
                "type": SoftStringOrNothingType,
                "display_name": "Country Name",
                "short_desc": "Country display name",
                "desc": "Human-readable country name",
            },
            "iso_2": {
                "type": SoftStringOrNothingType,
                "display_name": "ISO 2",
                "short_desc": "ISO 3166-1 alpha-2 code",
                "desc": "Two-letter ISO country code",
            },
            "iso_3": {
                "type": SoftStringOrNothingType,
                "display_name": "ISO 3",
                "short_desc": "ISO 3166-1 alpha-3 code",
                "desc": "Three-letter ISO country code",
            },
            "price": {
                "type": SoftFloatOrNothingType,
                "display_name": "Price",
                "short_desc": "Base shipping price",
                "desc": "Price for the shipping method to this country",
            },
            "lead_time_hours": {
                "type": SoftIntOrNothingType,
                "display_name": "Lead Time (Hours)",
                "short_desc": "Estimated transit time in hours",
                "desc": "Estimated delivery lead time in hours",
            },
        };
    }

    constructor() : HashDataType("SendCloudShippingMethodCountryDataType") {
        addQoreFields(Fields);
    }
}

#! SendCloud shipping-method record
public class SendCloudShippingMethodDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftIntOrNothingType,
                "display_name": "ID",
                "short_desc": "Shipping method ID",
                "desc": "Numeric ID of the shipping method",
            },
            "name": {
                "type": SoftStringOrNothingType,
                "display_name": "Name",
                "short_desc": "Shipping method name",
                "desc": "Human-readable name of the shipping method",
            },
            "carrier": {
                "type": SoftStringOrNothingType,
                "display_name": "Carrier",
                "short_desc": "Carrier code",
                "desc": "Carrier code (e.g., postnl, dhl, ups)",
            },
            "min_weight": {
                "type": SoftStringOrNothingType,
                "display_name": "Minimum Weight",
                "short_desc": "Minimum supported weight (kg)",
                "desc": "Minimum weight (kg, string-encoded decimal) supported by this method",
            },
            "max_weight": {
                "type": SoftStringOrNothingType,
                "display_name": "Maximum Weight",
                "short_desc": "Maximum supported weight (kg)",
                "desc": "Maximum weight (kg, string-encoded decimal) supported by this method",
            },
            "service_point_input": {
                "type": SoftStringOrNothingType,
                "display_name": "Service Point Input",
                "short_desc": "Whether a service point is required",
                "desc": "Whether a service point is required, optional, or not used by this method",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "none", "display_name": "None",
                        "desc": "No service point input allowed"},
                    <AllowedValueInfo>{"value": "required", "display_name": "Required",
                        "desc": "A service point must be supplied"},
                    <AllowedValueInfo>{"value": "optional", "display_name": "Optional",
                        "desc": "A service point may optionally be supplied"},
                ),
            },
            "price": {
                "type": SoftFloatOrNothingType,
                "display_name": "Price",
                "short_desc": "Base price",
                "desc": "Base price for this shipping method",
            },
            "countries": {
                "type": new ListDataType("ShippingMethodCountries",
                    new SendCloudShippingMethodCountryDataType(), True),
                "display_name": "Countries",
                "short_desc": "Supported destination countries",
                "desc": "List of destination countries with their prices and lead times",
            },
        };
    }

    constructor() : HashDataType("SendCloudShippingMethodDataType") {
        addQoreFields(Fields);
    }
}

} # namespace SendCloudDataProvider
