# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreBinaryDataTypeBase class definition

/** QoreBinaryDataTypeBase.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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
#! describes a data type based on binary types with validation for parsing strings when used with soft types
public class QoreBinaryDataTypeBase inherits QoreDataType {
    #! creates the object
    /** @throw TYPE-ERROR invalid type; must be a binary type
    */
    private constructor(Type type, *hash<auto> options, *hash<auto> tags) : QoreDataType(type, options, tags) {
        if (type.getBaseTypeCode() != NT_BINARY) {
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
                value = binary(value);
            }
        }
        return QoreDataType::acceptsValue(value);
    }

    #! returns a "soft" type equivalent to the current type
    /** @return a "soft" type equivalent to the current type, with the same validation constraints
    */
    AbstractDataProviderType getSoftType() {
        if (soft_type) {
            return self;
        }
        return new QoreBinaryDataTypeBase(base_type_hash{NT_NOTHING}
            ? SoftBinaryOrNothingType
            : SoftBinaryType, getOptions()).copyWithConstraints(constraints);
    }
}
}
