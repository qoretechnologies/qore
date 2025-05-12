# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreUnsignedByteDataType class definition

/** QoreUnsignedByteDataType.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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
#! Unsigned byte data type for ints with values from 0 - 255
public class QoreUnsignedByteDataType inherits QoreIntDataTypeBase {
    public {
        #! Int type options
        const Options = {
            "number.min": 0,
            "number.max": 255,
        };
    }

    #! Creates the object
    constructor(*hash<auto> opts) : QoreIntDataTypeBase(IntType, Options + opts) {
    }

    #! returns the type name
    string getName() {
        return "ubyte";
    }

    #! Returns a "soft" type equivalent to the current type
    /** The base class method returns the same type; this method must be overridden in child classes to return a
        usable "soft" type

        @return a "soft" type equivalent to the current type
    */
    AbstractDataProviderType getSoftType() {
        return new QoreSoftUnsignedByteDataType();
    }
}

#! Unsigned byte data type for ints with values from 0 - 255
public const QoreUnsignedByteDataType = new QoreUnsignedByteDataType();
}

