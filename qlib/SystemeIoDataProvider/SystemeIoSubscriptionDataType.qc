# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SystemeIoDataProvider module definition

/*  SystemeIoSubscriptionDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SystemeIoDataProvider module
public namespace SystemeIoDataProvider {

#! Systeme.io subscription record type
public class SystemeIoSubscriptionDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The subscription ID",
                "desc": "The unique identifier of the subscription",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Subscription status",
                "desc": "The current status of the subscription",
            },
            "createdAt": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "The date and time when the subscription was created",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Systeme.io cancel subscription request data type
public class SystemeIoCancelSubscriptionRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "Subscription ID",
                "short_desc": "The subscription ID to cancel",
                "desc": "The unique identifier of the subscription to cancel",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SystemeIoDataProvider
