# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationWeightDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! ShipStation weight record type
/** Used for package and order weights.
*/
public class ShipStationWeightDataType inherits HashDataType {
    private {
        #! Field definitions for ShipStation weight
        const Fields = {
            "value": {
                "type": FloatType,
                "display_name": "Value",
                "short_desc": "Weight value",
                "desc": "The numeric weight value",
                "example_value": 2.5,
            },
            "units": {
                "type": StringType,
                "display_name": "Units",
                "short_desc": "Weight units",
                "desc": "The weight units: 'ounces', 'pounds', or 'grams'",
                "example_value": "ounces",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "ounces",
                        "display_name": "Ounces",
                        "desc": "Weight in ounces (oz)",
                    },
                    <AllowedValueInfo>{
                        "value": "pounds",
                        "display_name": "Pounds",
                        "desc": "Weight in pounds (lbs)",
                    },
                    <AllowedValueInfo>{
                        "value": "grams",
                        "display_name": "Grams",
                        "desc": "Weight in grams (g)",
                    },
                ),
            },
            "WeightUnits": {
                "type": IntOrNothingType,
                "display_name": "Weight Units Code",
                "short_desc": "Numeric weight units code",
                "desc": "The numeric code for weight units (1=pounds, 2=ounces, 3=grams)",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ShipStationDataProvider
