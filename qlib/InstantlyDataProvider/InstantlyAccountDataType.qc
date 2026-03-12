# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyAccountDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Instantly email account data type
public class InstantlyAccountDataType inherits HashDataType {
    public {
        const Fields = {
            "email": {
                "display_name": "Email",
                "short_desc": "Email address",
                "type": StringType,
                "desc": "The email address of the account",
                "required": True,
            },
            "id": {
                "display_name": "Account ID",
                "short_desc": "Account UUID",
                "type": StringOrNothingType,
                "desc": "The unique identifier (UUID) of the account",
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Account status code",
                "type": IntOrNothingType,
                "desc": "The status of the account: `1` = Active, `2` = Paused, `3` = Maintenance, "
                    "`-1` = Connection Error, `-2` = Soft Bounce, `-3` = Sending Error",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 1, "display_name": "Active"},
                    <AllowedValueInfo>{"value": 2, "display_name": "Paused"},
                    <AllowedValueInfo>{"value": 3, "display_name": "Maintenance"},
                    <AllowedValueInfo>{"value": -1, "display_name": "Connection Error"},
                    <AllowedValueInfo>{"value": -2, "display_name": "Soft Bounce"},
                    <AllowedValueInfo>{"value": -3, "display_name": "Sending Error"},
                ),
            },
            "provider_code": {
                "display_name": "Provider Code",
                "short_desc": "Email provider code",
                "type": IntOrNothingType,
                "desc": "The email provider code: `1` = Custom IMAP/SMTP, `2` = Google, `3` = Microsoft, "
                    "`4` = AWS, `8` = AirMail",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 1, "display_name": "Custom IMAP/SMTP"},
                    <AllowedValueInfo>{"value": 2, "display_name": "Google"},
                    <AllowedValueInfo>{"value": 3, "display_name": "Microsoft"},
                    <AllowedValueInfo>{"value": 4, "display_name": "AWS"},
                    <AllowedValueInfo>{"value": 8, "display_name": "AirMail"},
                ),
            },
            "warmup_status": {
                "display_name": "Warmup Status",
                "short_desc": "Warmup status code",
                "type": IntOrNothingType,
                "desc": "The warmup status: `0` = Paused, `1` = Active, `-1` = Banned, "
                    "`-2` = Spam Unknown, `-3` = Suspended",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 0, "display_name": "Paused"},
                    <AllowedValueInfo>{"value": 1, "display_name": "Active"},
                    <AllowedValueInfo>{"value": -1, "display_name": "Banned"},
                    <AllowedValueInfo>{"value": -2, "display_name": "Spam Unknown"},
                    <AllowedValueInfo>{"value": -3, "display_name": "Suspended"},
                ),
            },
            "daily_limit": {
                "display_name": "Daily Limit",
                "short_desc": "Email sending limit per day",
                "type": IntOrNothingType,
                "desc": "The maximum number of emails that can be sent per day from this account",
            },
            "tracking_domain": {
                "display_name": "Tracking Domain",
                "short_desc": "Custom tracking domain",
                "type": StringOrNothingType,
                "desc": "The custom tracking domain configured for this account",
            },
            "created_at": {
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "type": DateOrNothingType,
                "desc": "The date and time the account was created",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace InstantlyDataProvider
