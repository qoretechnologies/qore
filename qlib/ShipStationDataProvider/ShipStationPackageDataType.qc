# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationPackageDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! ShipStation package data type (carrier package type)
public class ShipStationPackageDataType inherits HashDataType {
    #! Creates the object
    constructor() : HashDataType("ShipStationPackage", {
        "carrierCode": {
            "display_name": "Carrier Code",
            "short_desc": "Carrier code",
            "type": SoftStringType,
            "desc": "The carrier code (e.g., 'ups', 'fedex', 'stamps_com')",
        },
        "code": {
            "display_name": "Package Code",
            "short_desc": "Package code",
            "type": SoftStringType,
            "desc": "The package code (e.g., 'package', 'letter', 'flat_rate_box')",
        },
        "name": {
            "display_name": "Package Name",
            "short_desc": "Name of the package type",
            "type": SoftStringType,
            "desc": "The display name of the package type",
        },
        "domestic": {
            "display_name": "Domestic",
            "short_desc": "Whether package is domestic",
            "type": SoftBoolOrNothingType,
            "desc": "Whether this package type is available for domestic shipments",
        },
        "international": {
            "display_name": "International",
            "short_desc": "Whether package is international",
            "type": SoftBoolOrNothingType,
            "desc": "Whether this package type is available for international shipments",
        },
    }) {
    }
}

} # namespace ShipStationDataProvider
