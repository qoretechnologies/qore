# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShippoDataProvider module definition

/*  ShippoParcelDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Shippo parcel data type
public class ShippoParcelDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "object_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Object ID",
                "short_desc": "Unique parcel identifier",
                "desc": "The unique identifier of the parcel object in Shippo",
            },
            "length": {
                "type": SoftFloatOrNothingType,
                "display_name": "Length",
                "short_desc": "Package length",
                "desc": "Length of the package (in the units specified by distance_unit)",
            },
            "width": {
                "type": SoftFloatOrNothingType,
                "display_name": "Width",
                "short_desc": "Package width",
                "desc": "Width of the package (in the units specified by distance_unit)",
            },
            "height": {
                "type": SoftFloatOrNothingType,
                "display_name": "Height",
                "short_desc": "Package height",
                "desc": "Height of the package (in the units specified by distance_unit)",
            },
            "distance_unit": {
                "type": SoftStringOrNothingType,
                "display_name": "Distance Unit",
                "short_desc": "Unit for dimensions",
                "desc": "Unit of measurement for length, width, and height",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "in", "desc": "Inches"},
                    <AllowedValueInfo>{"value": "cm", "desc": "Centimeters"},
                    <AllowedValueInfo>{"value": "ft", "desc": "Feet"},
                    <AllowedValueInfo>{"value": "mm", "desc": "Millimeters"},
                    <AllowedValueInfo>{"value": "m", "desc": "Meters"},
                    <AllowedValueInfo>{"value": "yd", "desc": "Yards"},
                ),
            },
            "weight": {
                "type": SoftFloatOrNothingType,
                "display_name": "Weight",
                "short_desc": "Package weight",
                "desc": "Weight of the package (in the units specified by mass_unit)",
            },
            "mass_unit": {
                "type": SoftStringOrNothingType,
                "display_name": "Mass Unit",
                "short_desc": "Unit for weight",
                "desc": "Unit of measurement for weight",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "g", "desc": "Grams"},
                    <AllowedValueInfo>{"value": "oz", "desc": "Ounces"},
                    <AllowedValueInfo>{"value": "lb", "desc": "Pounds"},
                    <AllowedValueInfo>{"value": "kg", "desc": "Kilograms"},
                ),
            },
            "template": {
                "type": SoftStringOrNothingType,
                "display_name": "Template",
                "short_desc": "Parcel template",
                "desc": "A parcel template is a predefined package used by a carrier (e.g., USPS_FlatRateEnvelope)",
            },
            "extra": {
                "type": new ShippoParcelExtraDataType(AutoHashOrNothingType),
                "display_name": "Extra",
                "short_desc": "Extra options",
                "desc": "Additional options for the parcel including COD, insurance, and label references",
            },
            "metadata": {
                "type": SoftStringOrNothingType,
                "display_name": "Metadata",
                "short_desc": "Custom metadata",
                "desc": "A string of up to 100 characters for storing additional information",
            },
            "object_created": {
                "type": SoftStringOrNothingType,
                "display_name": "Created",
                "short_desc": "Creation timestamp",
                "desc": "ISO 8601 timestamp when the parcel was created",
            },
            "object_updated": {
                "type": SoftStringOrNothingType,
                "display_name": "Updated",
                "short_desc": "Update timestamp",
                "desc": "ISO 8601 timestamp when the parcel was last updated",
            },
            "object_owner": {
                "type": SoftStringOrNothingType,
                "display_name": "Owner",
                "short_desc": "Owner email",
                "desc": "Email of the user who owns this parcel",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoParcelDataType") {
        addQoreFields(Fields);
    }
}

#! Shippo parcel data type for input (creating parcels)
public class ShippoParcelInputDataType inherits HashDataType {
    private {
        #! Field definitions for input
        const Fields = {
            "length": {
                "type": SoftFloatType,
                "display_name": "Length",
                "short_desc": "Package length",
                "desc": "Length of the package (in the units specified by distance_unit)",
                "required": True,
            },
            "width": {
                "type": SoftFloatType,
                "display_name": "Width",
                "short_desc": "Package width",
                "desc": "Width of the package (in the units specified by distance_unit)",
                "required": True,
            },
            "height": {
                "type": SoftFloatType,
                "display_name": "Height",
                "short_desc": "Package height",
                "desc": "Height of the package (in the units specified by distance_unit)",
                "required": True,
            },
            "distance_unit": {
                "type": StringType,
                "display_name": "Distance Unit",
                "short_desc": "Unit for dimensions",
                "desc": "Unit of measurement for length, width, and height",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "in", "desc": "Inches"},
                    <AllowedValueInfo>{"value": "cm", "desc": "Centimeters"},
                    <AllowedValueInfo>{"value": "ft", "desc": "Feet"},
                    <AllowedValueInfo>{"value": "mm", "desc": "Millimeters"},
                    <AllowedValueInfo>{"value": "m", "desc": "Meters"},
                    <AllowedValueInfo>{"value": "yd", "desc": "Yards"},
                ),
            },
            "weight": {
                "type": SoftFloatType,
                "display_name": "Weight",
                "short_desc": "Package weight",
                "desc": "Weight of the package (in the units specified by mass_unit)",
                "required": True,
            },
            "mass_unit": {
                "type": StringType,
                "display_name": "Mass Unit",
                "short_desc": "Unit for weight",
                "desc": "Unit of measurement for weight",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "g", "desc": "Grams"},
                    <AllowedValueInfo>{"value": "oz", "desc": "Ounces"},
                    <AllowedValueInfo>{"value": "lb", "desc": "Pounds"},
                    <AllowedValueInfo>{"value": "kg", "desc": "Kilograms"},
                ),
            },
            "template": {
                "type": SoftStringOrNothingType,
                "display_name": "Template",
                "short_desc": "Parcel template",
                "desc": "A parcel template is a predefined package used by a carrier (e.g., USPS_FlatRateEnvelope)",
            },
            "extra": {
                "type": new ShippoParcelExtraDataType(AutoHashOrNothingType),
                "display_name": "Extra",
                "short_desc": "Extra options",
                "desc": "Additional options for the parcel including COD, insurance, and label references",
            },
            "metadata": {
                "type": SoftStringOrNothingType,
                "display_name": "Metadata",
                "short_desc": "Custom metadata",
                "desc": "A string of up to 100 characters for storing additional information",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoParcelInputDataType") {
        addQoreFields(Fields);
    }
}

} # namespace ShippoDataProvider
