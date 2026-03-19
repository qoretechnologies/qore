# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KitBulkSubscriberDataType.qc Kit bulk subscriber data type for bulk create operations

/*  KitBulkSubscriberDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Data type for a subscriber in a Kit bulk create operation
public class KitBulkSubscriberDataType inherits HashDataType {
    public {
        #! Field definitions for a bulk subscriber
        const Fields = {
            "email_address": {
                "type": StringType,
                "display_name": "Email Address",
                "short_desc": "The subscriber's email address",
                "desc": "The email address of the subscriber",
                "required": True,
                "example_value": "user@example.com",
            },
            "first_name": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "The subscriber's first name",
                "desc": "The first name of the subscriber",
                "example_value": "John",
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "The subscriber state",
                "desc": "The state of the subscriber",
                "example_value": "active",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "active",
                        "desc": "Active subscriber",
                    },
                    <AllowedValueInfo>{
                        "value": "cancelled",
                        "desc": "Cancelled/unsubscribed subscriber",
                    },
                    <AllowedValueInfo>{
                        "value": "bounced",
                        "desc": "Bounced subscriber",
                    },
                    <AllowedValueInfo>{
                        "value": "complained",
                        "desc": "Subscriber who filed a complaint",
                    },
                    <AllowedValueInfo>{
                        "value": "inactive",
                        "desc": "Inactive subscriber",
                    },
                ),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace KitDataProvider
