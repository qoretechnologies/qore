# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreHashDataType class definition

/** QoreHashDataType.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

# assume local scope for variables, do not use "$" signs
%modern
# require type definitions everywhere
#! strict argument handling
# enable all warnings

%requires reflection

#! contains all public definitions in the DataProvider module
public namespace DataProvider {
#! describes a data type
public class QoreHashDataType inherits QoreDataType {
    #! creates the type
    /** @throw TYPE-ERROR invalid type; must be a hash type but not a hashdecl
    */
    constructor(Type type = HashType, *hash<auto> options, *hash<auto> tags, *hash<auto> default_value,
            *hash<NameDescInfo> attr) : QoreDataType(type, options, tags, default_value, attr) {
        if (type.getBaseTypeCode() != NT_HASH || type.isTypedHash()) {
            throw "TYPE-ERROR", sprintf("cannot create a %s object from type %y", self.className(), type.getName());
        }
    }

    #! get the given field type if it exists, otherwise return @ref nothing
    *AbstractDataProviderType getFieldType(string field_name) {
        return getElementType();
    }
}
}
