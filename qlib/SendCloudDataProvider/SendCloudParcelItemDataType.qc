# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudParcelItemDataType.qc SendCloud parcel (customs) line item data type

/*  SendCloudParcelItemDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! A single parcel (customs) item
public class SendCloudParcelItemDataType inherits HashDataType {
    public {
        const Fields = {
            "description": {
                "type": SoftStringType,
                "display_name": "Description",
                "short_desc": "Item description",
                "desc": "Description of the item as it will appear on the customs declaration",
                "required": True,
                "example_value": "T-shirt",
            },
            "quantity": {
                "type": SoftIntType,
                "display_name": "Quantity",
                "short_desc": "Number of units",
                "desc": "Number of units of this item included in the parcel",
                "required": True,
                "example_value": 1,
            },
            "value": {
                "type": SoftStringType,
                "display_name": "Unit Value",
                "short_desc": "Unit declared value",
                "desc": "Declared value per unit (string-encoded decimal) in the parcel currency",
                "required": True,
                "example_value": "19.99",
            },
            "weight": {
                "type": SoftStringType,
                "display_name": "Unit Weight",
                "short_desc": "Weight per unit (kg)",
                "desc": "Weight of a single unit (string-encoded decimal in kilograms)",
                "required": True,
                "example_value": "0.250",
            },
            "hs_code": {
                "type": SoftStringOrNothingType,
                "display_name": "HS Code",
                "short_desc": "Harmonized System code",
                "desc": "Harmonized System (customs tariff) code of the item",
            },
            "origin_country": {
                "type": SoftStringOrNothingType,
                "display_name": "Origin Country",
                "short_desc": "Country of manufacture",
                "desc": "ISO 3166-1 alpha-2 code of the country where the item was produced",
                "ref_data": "countries",
            },
            "product_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Product ID",
                "short_desc": "Internal product ID",
                "desc": "Internal identifier of the product (maps to SendCloud's product catalog)",
            },
            "properties": {
                "type": AutoHashOrNothingType,
                "display_name": "Properties",
                "short_desc": "Arbitrary item properties",
                "desc": "Free-form key/value metadata about the item",
            },
            "sku": {
                "type": SoftStringOrNothingType,
                "display_name": "SKU",
                "short_desc": "Stock keeping unit",
                "desc": "SKU of the item",
            },
            "return_reason": {
                "type": SoftIntOrNothingType,
                "display_name": "Return Reason",
                "short_desc": "Return reason code",
                "desc": "Return-specific reason code (only used for return items)",
            },
            "return_message": {
                "type": SoftStringOrNothingType,
                "display_name": "Return Message",
                "short_desc": "Return message",
                "desc": "Customer-supplied message explaining the return reason",
            },
            "mid_code": {
                "type": SoftStringOrNothingType,
                "display_name": "MID Code",
                "short_desc": "Manufacturer identification code",
                "desc": "Manufacturer identification code (used for US customs)",
            },
        };
    }

    constructor() : HashDataType("SendCloudParcelItemDataType") {
        addQoreFields(Fields);
    }
}

} # namespace SendCloudDataProvider
