# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShippoDataProvider module definition

/*  ShippoParcelExtraDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Shippo parcel extra data type (additional parcel options)
public class ShippoParcelExtraDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "COD": {
                "type": new ShippoCodDataType(AutoHashOrNothingType),
                "display_name": "COD",
                "short_desc": "Collect on delivery",
                "desc": "Collection on delivery details for this parcel (UPS only)",
            },
            "insurance": {
                "type": new ShippoInsuranceDataType(AutoHashOrNothingType),
                "display_name": "Insurance",
                "short_desc": "Parcel insurance",
                "desc": "Insurance details for this parcel. Specify `amount`, `content`, and `currency` "
                    "for XCover insurance, or additionally set `provider` for carrier-provided insurance",
            },
            "reference_1": {
                "type": SoftStringOrNothingType,
                "display_name": "Reference 1",
                "short_desc": "Label reference text 1",
                "desc": "Optional text (up to 50 characters) printed on the shipping label if supported by "
                    "the carrier",
            },
            "reference_2": {
                "type": SoftStringOrNothingType,
                "display_name": "Reference 2",
                "short_desc": "Label reference text 2",
                "desc": "Optional text (up to 50 characters) printed on the shipping label if supported by "
                    "the carrier",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoParcelExtraDataType") {
        addQoreFields(Fields);
    }

    #! Creates the object with a base type (for optional fields)
    constructor(Type base_type) : HashDataType(base_type, "ShippoParcelExtraDataType") {
        addQoreFields(Fields);
    }
}

} # namespace ShippoDataProvider
