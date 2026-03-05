# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreFulfilmentLineDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the Cin7CoreDataProvider module
public namespace Cin7CoreDataProvider {

#! Cin7Core fulfilment line data type (used for pick/pack/ship line items)
public class Cin7CoreFulfilmentLineDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "ProductID": {
                "type": SoftStringOrNothingType,
                "display_name": "Product ID",
                "short_desc": "Product identifier",
                "desc": "The unique identifier of the product",
            },
            "SKU": {
                "type": SoftStringOrNothingType,
                "display_name": "SKU",
                "short_desc": "Stock keeping unit",
                "desc": "The product SKU",
            },
            "Name": {
                "type": SoftStringOrNothingType,
                "display_name": "Name",
                "short_desc": "Product name",
                "desc": "The product name",
            },
            "Quantity": {
                "type": SoftFloatOrNothingType,
                "display_name": "Quantity",
                "short_desc": "Quantity",
                "desc": "The quantity for this line",
            },
            "Bin": {
                "type": SoftStringOrNothingType,
                "display_name": "Bin",
                "short_desc": "Bin location",
                "desc": "The bin location within the warehouse",
            },
            "Batch": {
                "type": SoftStringOrNothingType,
                "display_name": "Batch",
                "short_desc": "Batch number",
                "desc": "The batch number",
            },
            "ExpiryDate": {
                "type": SoftDateOrNothingType,
                "display_name": "Expiry Date",
                "short_desc": "Expiration date",
                "desc": "The product's expiry date",
            },
            "Box": {
                "type": SoftStringOrNothingType,
                "display_name": "Box",
                "short_desc": "Box reference",
                "desc": "The box or carton reference for shipping",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreFulfilmentLineDataType") {
        addQoreFields(Fields);
    }
}

#! Cin7Core additional charge data type
public class Cin7CoreAdditionalChargeDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "Description": {
                "type": SoftStringOrNothingType,
                "display_name": "Description",
                "short_desc": "Charge description",
                "desc": "Description of the additional charge",
            },
            "Price": {
                "type": SoftFloatOrNothingType,
                "display_name": "Price",
                "short_desc": "Charge amount",
                "desc": "The price of the additional charge",
            },
            "Quantity": {
                "type": SoftFloatOrNothingType,
                "display_name": "Quantity",
                "short_desc": "Quantity",
                "desc": "The quantity for this charge",
            },
            "Tax": {
                "type": SoftFloatOrNothingType,
                "display_name": "Tax",
                "short_desc": "Tax amount",
                "desc": "The tax amount for this charge",
            },
            "Total": {
                "type": SoftFloatOrNothingType,
                "display_name": "Total",
                "short_desc": "Total amount",
                "desc": "The total amount for this charge",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreAdditionalChargeDataType") {
        addQoreFields(Fields);
    }
}

} # namespace Cin7CoreDataProvider
