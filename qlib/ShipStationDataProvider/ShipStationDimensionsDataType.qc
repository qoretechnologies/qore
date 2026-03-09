# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationDimensionsDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! ShipStation dimensions record type
/** Used for package dimensions.
*/
public class ShipStationDimensionsDataType inherits HashDataType {
    public {
        #! Field definitions for ShipStation dimensions
        const Fields = {
            "length": {
                "type": FloatOrNothingType,
                "display_name": "Length",
                "short_desc": "Package length",
                "desc": "The package length in the specified units",
                "example_value": 12.0,
            },
            "width": {
                "type": FloatOrNothingType,
                "display_name": "Width",
                "short_desc": "Package width",
                "desc": "The package width in the specified units",
                "example_value": 8.0,
            },
            "height": {
                "type": FloatOrNothingType,
                "display_name": "Height",
                "short_desc": "Package height",
                "desc": "The package height in the specified units",
                "example_value": 6.0,
            },
            "units": {
                "type": StringOrNothingType,
                "display_name": "Units",
                "short_desc": "Dimension units",
                "desc": "The dimension units: 'inches' or 'centimeters'",
                "example_value": "inches",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "inches",
                        "display_name": "Inches",
                        "desc": "Dimensions in inches",
                    },
                    <AllowedValueInfo>{
                        "value": "centimeters",
                        "display_name": "Centimeters",
                        "desc": "Dimensions in centimeters",
                    },
                ),
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

} # namespace ShipStationDataProvider
