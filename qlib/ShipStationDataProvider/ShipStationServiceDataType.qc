# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationServiceDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! ShipStation service data type (carrier service)
public class ShipStationServiceDataType inherits HashDataType {
    #! Creates the object
    constructor() : HashDataType("ShipStationService", {
        "carrierCode": {
            "display_name": "Carrier Code",
            "short_desc": "Carrier code",
            "type": SoftStringType,
            "desc": "The carrier code (e.g., `ups`, `fedex`, `stamps_com`)",
        },
        "code": {
            "display_name": "Service Code",
            "short_desc": "Service code",
            "type": SoftStringType,
            "desc": "The service code (e.g., 'ups_ground', 'fedex_2day')",
        },
        "name": {
            "display_name": "Service Name",
            "short_desc": "Name of the service",
            "type": SoftStringType,
            "desc": "The display name of the service (e.g., 'UPS Ground', 'FedEx 2Day')",
        },
        "domestic": {
            "display_name": "Domestic",
            "short_desc": "Whether service is domestic",
            "type": SoftBoolOrNothingType,
            "desc": "Whether this service is available for domestic shipments",
        },
        "international": {
            "display_name": "International",
            "short_desc": "Whether service is international",
            "type": SoftBoolOrNothingType,
            "desc": "Whether this service is available for international shipments",
        },
    }) {
    }
}

} # namespace ShipStationDataProvider
