# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreBoolDataTypeBase class definition

/** QoreBoolDataTypeBase.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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
#! describes a data type based on softint with validation for parsing strings
public class QoreBoolDataTypeBase inherits QoreDataType {
    #! creates the object
    /** @param type the boolean type
        @param options type options
        @param tags type tags
        @param default_value any default value (since DataProvider 3.8)
        @param attr any display name and descriptions (since DataProvider 3.8)

        @throw TYPE-ERROR invalid type; must be a bool type

        @note public since DataProvider 3.8, so that
        @ref AbstractDataProviderType::get() "AbstractDataProviderType::get()" returns a type that validates strings
        for every boolean type
    */
    constructor(Type type, *hash<auto> options, *hash<auto> tags, auto default_value, *hash<NameDescInfo> attr)
            : QoreDataType(type, options, tags, default_value, attr) {
        if (type.getBaseTypeCode() != NT_BOOLEAN) {
            throw "TYPE-ERROR", sprintf("cannot create a %s object from type %y", self.className(),
                type.getName());
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
                # check for valid integer values
                if (value !~ /^([-+])?[0-9]+$/) {
                    throw "RUNTIME-TYPE-ERROR", sprintf("value %y cannot by parsed by type %y; the input data "
                        "contains non-integer text", value, getName());
                }
                return boolean(value);
            }
        }

        return QoreDataType::acceptsValue(value);
    }

    #! returns a "soft" type equivalent to the current type
    /** @return a "soft" type equivalent to the current type
    */
    AbstractDataProviderType getSoftType() {
        if (soft_type) {
            return self;
        }
        return new QoreBoolDataTypeBase(base_type_hash{NT_NOTHING} ? SoftBoolOrNothingType : SoftBoolType,
            getOptions());
    }
}
}
