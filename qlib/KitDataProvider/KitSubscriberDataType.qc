# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KitDataProvider module definition

/*  KitSubscriberDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KitDataProvider module
public namespace KitDataProvider {

#! Kit subscriber record type
public class KitSubscriberDataType inherits HashDataType {
    public {
        #! Field definitions for Kit subscribers
        const Fields = {
            "id": {
                "type": SoftIntType,
                "display_name": "ID",
                "short_desc": "The subscriber ID",
                "desc": "The unique identifier of the subscriber",
            },
            "first_name": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "The subscriber's first name",
                "desc": "The first name of the subscriber",
            },
            "email_address": {
                "type": StringType,
                "display_name": "Email Address",
                "short_desc": "The subscriber's email address",
                "desc": "The email address of the subscriber",
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "The subscriber's state",
                "desc": "The current state of the subscriber",
                "allowed_values": (<AllowedValueInfo>{"value": "active"}, <AllowedValueInfo>{"value": "inactive"}, <AllowedValueInfo>{"value": "bounced"}, <AllowedValueInfo>{"value": "complained"}, <AllowedValueInfo>{"value": "cancelled"}),
            },
            "fields": {
                "type": AutoHashOrNothingType,
                "display_name": "Fields",
                "short_desc": "Custom fields",
                "desc": "A hash of custom field values for the subscriber",
            },
            "created_at": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the subscriber was created",
                "desc": "The timestamp when the subscriber was created",
            },
            "tagged_at": {
                "type": DateOrNothingType,
                "display_name": "Tagged At",
                "short_desc": "When the subscriber was tagged",
                "desc": "The timestamp when the subscriber was tagged",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace KitDataProvider
