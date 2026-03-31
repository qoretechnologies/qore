# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BitlyDataProvider module definition

/*  BitlyRenderCustomizationsDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BitlyDataProvider module
public namespace BitlyDataProvider {

#! QR code render customizations type for appearance settings
public class BitlyRenderCustomizationsDataType inherits HashDataType {
    public {
        #! Field definitions for render customizations
        const Fields = {
            "background_color": {
                "type": StringOrNothingType,
                "display_name": "Background Color",
                "short_desc": "QR code background color",
                "desc": "The background color as a hex string with alpha channel "
                    "(e.g., `\"#FFFFFFFF\"`)",
                "example_value": "#FFFFFFFF",
            },
            "dot_pattern_color": {
                "type": StringOrNothingType,
                "display_name": "Dot Pattern Color",
                "short_desc": "QR code dot pattern color",
                "desc": "The color of the QR code dot pattern as a hex string "
                    "(e.g., `\"#000000FF\"`)",
                "example_value": "#000000FF",
            },
            "dot_pattern_type": {
                "type": StringOrNothingType,
                "display_name": "Dot Pattern Type",
                "short_desc": "QR code dot shape/pattern",
                "desc": "The shape of the dots in the QR code pattern",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "standard", "display_name": "Standard"},
                    <AllowedValueInfo>{"value": "rounded", "display_name": "Rounded"},
                    <AllowedValueInfo>{"value": "circle", "display_name": "Circle"},
                    <AllowedValueInfo>{"value": "block", "display_name": "Block"},
                    <AllowedValueInfo>{"value": "blob", "display_name": "Blob"},
                    <AllowedValueInfo>{"value": "vertical", "display_name": "Vertical"},
                    <AllowedValueInfo>{"value": "horizontal", "display_name": "Horizontal"},
                    <AllowedValueInfo>{"value": "triangle", "display_name": "Triangle"},
                    <AllowedValueInfo>{"value": "heart", "display_name": "Heart"},
                    <AllowedValueInfo>{"value": "star", "display_name": "Star"},
                    <AllowedValueInfo>{"value": "diamond", "display_name": "Diamond"},
                ),
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

} # namespace BitlyDataProvider
