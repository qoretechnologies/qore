# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SquarespaceDataProvider module definition

/*  SquarespaceInventoryItemDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquarespaceDataProvider module
public namespace SquarespaceDataProvider {

#! Squarespace inventory item record type
public class SquarespaceInventoryItemDataType inherits HashDataType {
    private {
        #! Field definitions for Squarespace inventory items
        const Fields = {
            "variantId": {
                "type": StringType,
                "display_name": "Variant ID",
                "short_desc": "The variant ID",
                "desc": "The unique identifier of the variant this inventory item belongs to",
            },
            "sku": {
                "type": StringOrNothingType,
                "display_name": "SKU",
                "short_desc": "The item SKU",
                "desc": "The stock keeping unit identifier for the inventory item",
            },
            "isUnlimited": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Unlimited",
                "short_desc": "Whether stock is unlimited",
                "desc": "Indicates whether the inventory item has unlimited stock",
            },
            "quantity": {
                "type": SoftIntOrNothingType,
                "display_name": "Quantity",
                "short_desc": "The stock quantity",
                "desc": "The available stock quantity for the inventory item",
            },
            "descriptor": {
                "type": StringOrNothingType,
                "display_name": "Descriptor",
                "short_desc": "The item descriptor",
                "desc": "A descriptor for the inventory item",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SquarespaceDataProvider
