# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InsightlyDataProvider module definition

/*  InsightlyProductPricebookDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the InsightlyDataProvider module
public namespace InsightlyDataProvider {

#! Insightly product pricebook entry data type
public class InsightlyProductPricebookDataType inherits HashDataType {
    public {
        #! Field definitions for Insightly product pricebook entries
        const Fields = {
            "PRICEBOOK_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Pricebook ID",
                "short_desc": "The pricebook identifier",
                "desc": "The unique identifier of the pricebook entry",
            },
            "CURRENCY_CODE": {
                "type": SoftStringOrNothingType,
                "display_name": "Currency Code",
                "short_desc": "The currency code",
                "desc": "The ISO 4217 currency code (e.g., `USD`, `EUR`, `GBP`)",
                "example_value": "USD",
            },
            "PRICE": {
                "type": SoftFloatOrNothingType,
                "display_name": "Price",
                "short_desc": "The price in the given currency",
                "desc": "The price of the product in the specified currency",
            },
            "ACTIVE": {
                "type": SoftBoolOrNothingType,
                "display_name": "Active",
                "short_desc": "Whether the pricebook entry is active",
                "desc": "If `True`, the pricebook entry is active",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("InsightlyProductPricebookDataType") {
        addQoreFields(Fields);
    }
}

} # namespace InsightlyDataProvider
