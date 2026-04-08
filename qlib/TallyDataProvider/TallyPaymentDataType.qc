# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TallyDataProvider module definition

/** TallyPaymentDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the TallyDataProvider module
public namespace TallyDataProvider {
#! Payment configuration data type for forms
public class TallyPaymentDataType inherits HashDataType {
    public {
        const Fields = {
            "amount": {
                "display_name": "Amount",
                "short_desc": "Payment amount",
                "desc": "The payment amount configured for this form",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "currency": {
                "display_name": "Currency",
                "short_desc": "Payment currency code",
                "desc": "The currency code for the payment",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "USD", "display_name": "US Dollar"},
                    <AllowedValueInfo>{"value": "EUR", "display_name": "Euro"},
                    <AllowedValueInfo>{"value": "GBP", "display_name": "British Pound"},
                    <AllowedValueInfo>{"value": "CAD", "display_name": "Canadian Dollar"},
                    <AllowedValueInfo>{"value": "AUD", "display_name": "Australian Dollar"},
                    <AllowedValueInfo>{"value": "JPY", "display_name": "Japanese Yen"},
                    <AllowedValueInfo>{"value": "CNY", "display_name": "Chinese Yuan"},
                    <AllowedValueInfo>{"value": "INR", "display_name": "Indian Rupee"},
                ),
            },
        };
    }

    #! Creates the object
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}
}
