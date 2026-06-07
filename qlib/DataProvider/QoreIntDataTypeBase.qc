# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreIntDataTypeBase class definition

/** QoreIntDataTypeBase.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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
#! describes a data type based on int types with validation for parsing strings when used with soft types
public class QoreIntDataTypeBase inherits QoreDataType {
    private {
        #! supported non-soft options
        const hash<string, hash<DataProviderTypeOptionInfo>> SupportedOptions = {
            "number.min": <DataProviderTypeOptionInfo>{
                "type": Type::Int,
                "desc": "The minimum value for this type; by default values less than this will be silently "
                    "converted to this value on assignment",
            },
            "number.max": <DataProviderTypeOptionInfo>{
                "type": Type::Int,
                "desc": "The maximum value for this type; by default values greater than this will be silently "
                    "converted to this value on assignment",
            },
        };

        #! supported soft options
        const hash<string, hash<DataProviderTypeOptionInfo>> SupportedSoftOptions = {
            "number.format": <DataProviderTypeOptionInfo>{
                "type": Type::String,
                "desc": "The format string for converting strings to numbers",
            },
        };
    }

    #! Creates the object
    /** @throw TYPE-ERROR invalid type; must be an integer type or invalid options
    */
    private constructor(Type type, *hash<auto> options, *hash<auto> tags) : QoreDataType(type, options, tags) {
        if (type.getBaseTypeCode() != NT_INT) {
            throw "TYPE-ERROR", sprintf("Cannot create a %s object from type %y", self.className(),
                type.getName());
        }
        if (exists options."number.min" && exists options."number.max"
            && options."number.min" > options."number.max") {
            throw "TYPE-ERROR", sprintf("Cannot create type %y with \"number.min\" (%d) > \"number.max\" (%d)",
                self.className(), options."number.min", options."number.max");
        }
    }

    #! returns the value if the value can be assigned to the type
    /** @param value the value to assign to the type

        @return the value to be assigned; can be converted by the type

        @throw RUNTIME-TYPE-ERROR value cannot be assigned to type
    */
    auto acceptsValue(auto value) {
        # try to convert from a string
        if (value.typeCode() == NT_STRING) {
            # issue #4836: "" -> NOTHING
            if (!value.val()) {
                remove value;
            } else if (soft_type) {
                if (options."number.format") {
                    value = parse_int(value, options."number.format");
                } else {
                    # check for valid integer values
                    if (value !~ /^([-+])?[0-9]+$/) {
                        throw "RUNTIME-TYPE-ERROR", sprintf("value %y cannot by parsed by type %y; the input data contains "
                            "non-integer text", value, getName());
                    }
                    value = int(value);
                }
            }
        }

        *int rv = QoreDataType::acceptsValue(value);
        if (exists rv) {
            if (exists options."number.min" && rv < options."number.min") {
                rv = options."number.min";
            } else if (exists options."number.max" && rv > options."number.max") {
                rv = options."number.max";
            }
        }
        return rv;
    }

    #! returns supported options
    *hash<string, hash<DataProviderTypeOptionInfo>> getSupportedOptions() {
        return QoreDataType::getSupportedOptions() + SupportedOptions + (soft_type ? SupportedSoftOptions : NOTHING);
    }

    #! returns a "soft" type equivalent to the current type
    /** @return a "soft" type equivalent to the current type
    */
    AbstractDataProviderType getSoftType() {
        if (soft_type) {
            return self;
        }
        return new QoreIntDataTypeBase(base_type_hash{NT_NOTHING} ? SoftIntOrNothingType : SoftIntType, getOptions());
    }
}
}
