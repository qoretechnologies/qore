# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyAuditLogDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the InstantlyDataProvider module
public namespace InstantlyDataProvider {

#! Instantly audit log data type
public class InstantlyAuditLogDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Audit Log ID",
                "short_desc": "Audit log UUID",
                "type": StringType,
                "desc": "The unique identifier (UUID) of the audit log entry",
                "required": True,
            },
            "action": {
                "display_name": "Action",
                "short_desc": "Action performed",
                "type": StringOrNothingType,
                "desc": "The action that was performed",
            },
            "user_id": {
                "display_name": "User ID",
                "short_desc": "User UUID",
                "type": StringOrNothingType,
                "desc": "The unique identifier (UUID) of the user who performed the action",
            },
            "user_email": {
                "display_name": "User Email",
                "short_desc": "User email address",
                "type": StringOrNothingType,
                "desc": "The email address of the user who performed the action",
            },
            "resource_type": {
                "display_name": "Resource Type",
                "short_desc": "Type of resource affected",
                "type": StringOrNothingType,
                "desc": "The type of resource that was affected by the action",
            },
            "resource_id": {
                "display_name": "Resource ID",
                "short_desc": "Resource UUID",
                "type": StringOrNothingType,
                "desc": "The unique identifier (UUID) of the resource that was affected",
            },
            "details": {
                "display_name": "Details",
                "short_desc": "Action details",
                "type": AutoHashOrNothingType,
                "desc": "Additional details about the action performed",
            },
            "timestamp": {
                "display_name": "Timestamp",
                "short_desc": "Action timestamp",
                "type": DateOrNothingType,
                "desc": "The date and time when the action was performed",
            },
            "ip_address": {
                "display_name": "IP Address",
                "short_desc": "Client IP address",
                "type": StringOrNothingType,
                "desc": "The IP address from which the action was performed",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace InstantlyDataProvider
