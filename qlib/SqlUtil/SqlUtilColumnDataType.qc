# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SqlUtilColumnDataType class definition

/** SqlUtilColumnDataType.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

# minimum required Qore version
%requires qore >= 2.0
# assume local scope for variables, do not use "$" signs
%modern
# require type definitions everywhere
#! strict argument handling
# enable all warnings

#! contains all public definitions in the SqlUtil module
public namespace SqlUtil {
#! generic type for SQL datatypes where no translations are provided for NULL for "or nothing" types
/**
    @since SqlUtil 1.6
*/
public class SqlUtilColumnDataType inherits DataProvider::QoreDataType {
    #! creates the object
    constructor(string native_type, string type, *hash<auto> options)
        : QoreDataType(new Type(OptimalQoreSoftDataTypeMap{type} ?? type), options + {"qore.no_null": True}) {
        name = native_type;
        if (or_nothing_type) {
            base_type_hash{NT_NULL} = True;
        }
    }

    #! creates the object
    constructor(string native_type, Type type, *hash<auto> options)
        : QoreDataType(type, options + {"qore.no_null": True}) {
        name = native_type;
        if (or_nothing_type) {
            base_type_hash{NT_NULL} = True;
        }
    }

    #! returns the value if the value can be assigned to the type
    /** @param value the value to assign to the type

        @return the value to be assigned; can be converted by the type
    */
    auto acceptsValue(auto value) {
        if (base_type_hash{value.typeCode()}) {
            return value;
        }
        return QoreDataType::acceptsValue(value);
    }
}
}
