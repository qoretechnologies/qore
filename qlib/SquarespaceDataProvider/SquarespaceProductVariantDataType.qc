# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SquarespaceDataProvider module definition

/*  SquarespaceProductVariantDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Squarespace product variant record type
public class SquarespaceProductVariantDataType inherits HashDataType {
    public {
        #! Field definitions for Squarespace product variants
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The variant ID",
                "desc": "The unique identifier of the product variant",
            },
            "sku": {
                "type": StringOrNothingType,
                "display_name": "SKU",
                "short_desc": "The variant SKU",
                "desc": "The stock keeping unit identifier for the variant",
            },
            "pricing": {
                "type": new SquarespaceFullPricingDataType(AutoHashOrNothingType),
                "display_name": "Pricing",
                "short_desc": "The variant pricing",
                "desc": "The pricing information for the variant (`basePrice`, `salePrice`, `onSale`)",
            },
            "stock": {
                "type": new SquarespaceStockDataType(AutoHashOrNothingType),
                "display_name": "Stock",
                "short_desc": "The stock information",
                "desc": "The stock information for the variant (`quantity`, `unlimited`)",
            },
            "attributes": {
                "type": AutoHashOrNothingType,
                "display_name": "Attributes",
                "short_desc": "The variant attributes",
                "desc": "The key-value pairs of variant attributes",
            },
            "shippingMeasurements": {
                "type": new SquarespaceShippingMeasurementsDataType(AutoHashOrNothingType),
                "display_name": "Shipping Measurements",
                "short_desc": "The shipping measurements",
                "desc": "The shipping measurements for the variant (`weight`, `dimensions`)",
            },
            "image": {
                "type": AutoHashOrNothingType,
                "display_name": "Image",
                "short_desc": "The variant image",
                "desc": "The image associated with the variant",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SquarespaceDataProvider
