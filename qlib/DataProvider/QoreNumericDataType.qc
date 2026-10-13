# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file QoreNumericDataType.qc provides a data type for any numeric value

/*  QoreNumericDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! A number of any kind: an integer, a float or an arbitrary-precision number, kept as it is given
/** This is the type of an expression argument that takes a number (@ref DataProviderSignatureNumberType): it is
    assignable from \c int, \c float and \c number values and from types that give only such values (ex: \c *int
    or \c softnumber), as the \c number type is, but an accepted value is not converted, so an expression gets the
    integer or float that it was given, as when it is evaluated without its argument types.

    @since DataProvider 3.9
*/
public class QoreNumericDataType inherits QoreDataType {
    #! creates the object
    constructor() : QoreDataType(NumberType, NOTHING, NOTHING, NOTHING, <NameDescInfo>{
        "name": "number",
        "display_name": "Number",
        "short_desc": "An integer, a float or an arbitrary-precision number",
        "desc": "An integer, a float or an arbitrary-precision number",
    }) {
    }

    #! returns the value if the value can be assigned to the type
    /** @param value the value to assign to the type

        @return the value unchanged if it is an integer, a float or a number; any other value is accepted or
        rejected as the \c number type accepts or rejects it

        @throw RUNTIME-TYPE-ERROR value cannot be assigned to type
    */
    auto acceptsValue(auto value) {
        switch (value.typeCode()) {
            case NT_INT:
            case NT_FLOAT:
            case NT_NUMBER:
                return value;
        }
        return QoreDataType::acceptsValue(value);
    }
}
}
