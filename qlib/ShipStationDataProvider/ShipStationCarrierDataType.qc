# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationCarrierDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! ShipStation carrier record type
public class ShipStationCarrierDataType inherits HashDataType {
    private {
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "Carrier name",
                "desc": "The display name of the carrier",
                "example_value": "Stamps.com",
            },
            "code": {
                "type": StringType,
                "display_name": "Code",
                "short_desc": "Carrier code",
                "desc": "The unique code for this carrier",
                "example_value": "stamps_com",
            },
            "accountNumber": {
                "type": StringOrNothingType,
                "display_name": "Account Number",
                "short_desc": "Carrier account number",
                "desc": "Your account number with this carrier",
            },
            "requiresFundedAccount": {
                "type": BoolOrNothingType,
                "display_name": "Requires Funded Account",
                "short_desc": "Needs prepaid balance",
                "desc": "Whether this carrier requires a funded account balance",
            },
            "balance": {
                "type": FloatOrNothingType,
                "display_name": "Balance",
                "short_desc": "Account balance",
                "desc": "The current balance on this carrier account",
            },
            "nickname": {
                "type": StringOrNothingType,
                "display_name": "Nickname",
                "short_desc": "Account nickname",
                "desc": "A custom nickname for this carrier account",
            },
            "shippingProviderId": {
                "type": IntOrNothingType,
                "display_name": "Shipping Provider ID",
                "short_desc": "Provider ID",
                "desc": "The internal shipping provider ID",
            },
            "primary": {
                "type": BoolOrNothingType,
                "display_name": "Primary",
                "short_desc": "Is primary carrier",
                "desc": "Whether this is the primary carrier account",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ShipStationDataProvider
