# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RgbColorDataType class definition

/** RgbColorDataType.qc Copyright 2019 - 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the DataProvider module
public namespace DataProvider {
#! RGB color data type
/** A color with integer red, green and blue components from \c 0 to \c 255, and an opacity \c a from \c 0
    (transparent) to \c 1 (opaque), which is \c 1 when it is missing.  An opacity outside of this range is rejected;
    the integers \c 0 and \c 1 are accepted as well as numbers in between, and the opacity is returned as a
    \c float.
*/
public class RgbColorDataTypeBase inherits HashDataType {
    public {
        #! The type of the opacity: a \c float from \c 0 to \c 1; values outside of this range are rejected
        /** @since DataProvider 3.9
        */
        const OpacityType = (new QoreFloatDataType()).copyWithConstraints(<DataTypeConstraintInfo>{
            "minimum": 0,
            "maximum": 1,
        });

        #! The fields of the type
        const hash<auto> Fields = {
            "r": {
                "display_name": "Red",
                "short_desc": "Red color component as an integer from 0 - 255",
                "desc": "Red color component as an integer from `0` - `255`",
                "type": QoreUnsignedByteDataType,
            },
            "g": {
                "display_name": "Green",
                "short_desc": "Green color component as an integer from 0 - 255",
                "desc": "Green color component as an integer from `0` - `255`",
                "type": QoreUnsignedByteDataType,
            },
            "b": {
                "display_name": "Blue",
                "short_desc": "Blue color component as an integer from 0 - 255",
                "desc": "Blue color component as an integer from `0` - `255`",
                "type": QoreUnsignedByteDataType,
            },
            "a": {
                "display_name": "Opacity",
                "short_desc": "Opacity, from 0 (transparent) to 1 (opaque)",
                "desc": "Opacity, from `0` (transparent) to `1` (opaque)",
                "type": OpacityType,
                "default_value": 1.0,
            },
        };
    }

    constructor(Type base_type, string name) : HashDataType(base_type, name, NOTHING, {"qore.external_name": "rgbcolor"}) {
        addQoreFields(Fields);
    }
}

#! RGB color data type
public class RgbColorDataType inherits RgbColorDataTypeBase {
    constructor() : RgbColorDataTypeBase(AutoHashType, "rgbcolor") {
    }
}

#! RGB color "or nothing" data type
public class RgbColorOrNothingDataType inherits RgbColorDataTypeBase {
    constructor() : RgbColorDataTypeBase(AutoHashOrNothingType, "*rgbcolor") {
    }
}

#! RGB color data type
public const RgbColorDataType RgbColorDataType = new RgbColorDataType();

#! RGB color "or nothing" data type constant
public const RgbColorOrNothingDataType RgbColorOrNothingDataType = new RgbColorOrNothingDataType();
}
