# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CustomerIoDataProvider module definition

/*  CustomerIoObjectDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CustomerIoDataProvider module
public namespace CustomerIoDataProvider {

#! Customer.io object record type
/** Represents an object in Customer.io.
*/
public class CustomerIoObjectDataType inherits HashDataType {
    public {
        #! Field definitions for Customer.io objects
        const Fields = {
            "object_type_id": {
                "type": StringOrNothingType,
                "display_name": "Object Type ID",
                "short_desc": "Object type ID",
                "desc": "The object type identifier",
            },
            "object_id": {
                "type": StringOrNothingType,
                "display_name": "Object ID",
                "short_desc": "Object ID",
                "desc": "The unique identifier for the object",
            },
            "cio_object_id": {
                "type": StringOrNothingType,
                "display_name": "CIO Object ID",
                "short_desc": "Customer.io immutable object ID",
                "desc": "The Customer.io immutable identifier for the object",
            },
            "attributes": {
                "type": AutoHashOrNothingType,
                "display_name": "Attributes",
                "short_desc": "Custom attributes",
                "desc": "Custom attributes associated with the object",
            },
            "relationships": {
                "type": AutoListOrNothingType,
                "display_name": "Relationships",
                "short_desc": "Relationships",
                "desc": "Relationships associated with the object",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace CustomerIoDataProvider
