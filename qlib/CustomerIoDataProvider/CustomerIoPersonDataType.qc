# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CustomerIoDataProvider module definition

/*  CustomerIoPersonDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Customer.io person/customer record type
/** Represents a person (customer) in Customer.io.
*/
public class CustomerIoPersonDataType inherits HashDataType {
    public {
        #! Field definitions for Customer.io persons
        const Fields = {
            "id": {
                "type": StringOrNothingType,
                "display_name": "ID",
                "short_desc": "Customer ID",
                "desc": "The unique identifier for the customer",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "Email address",
                "desc": "The email address of the customer",
            },
            "cio_id": {
                "type": StringOrNothingType,
                "display_name": "CIO ID",
                "short_desc": "Customer.io immutable ID",
                "desc": "The Customer.io immutable identifier for the customer",
            },
            "created_at": {
                "type": IntOrNothingType,
                "display_name": "Created At",
                "short_desc": "Unix timestamp when created",
                "desc": "Unix timestamp when the customer was created",
            },
            "attributes": {
                "type": AutoHashOrNothingType,
                "display_name": "Attributes",
                "short_desc": "Custom attributes",
                "desc": "Custom attributes associated with the customer",
            },
            "unsubscribed": {
                "type": BoolOrNothingType,
                "display_name": "Unsubscribed",
                "short_desc": "Unsubscribed status",
                "desc": "Whether the customer has unsubscribed from communications",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace CustomerIoDataProvider
