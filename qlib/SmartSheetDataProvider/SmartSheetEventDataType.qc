# -*- mode: qore; indent-tabs-mode: nil -*-

/*  SmartSheetEventDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SmartSheetDataProvider module
public namespace SmartSheetDataProvider {

#! Data type for Smartsheet event records from the Events API
public class SmartSheetEventDataType inherits HashDataType {
    public {
        #! Field definitions for Smartsheet events
        const Fields = {
            "eventId": {
                "type": StringType,
                "display_name": "Event ID",
                "short_desc": "Unique event identifier",
                "desc": "Unique identifier for the event",
            },
            "objectType": {
                "type": StringType,
                "display_name": "Object Type",
                "short_desc": "Type of object that changed",
                "desc": "The type of object that was affected (e.g., `SHEET`, `ROW`, `COLUMN`, "
                    "`COMMENT`, `ATTACHMENT`)",
            },
            "action": {
                "type": StringType,
                "display_name": "Action",
                "short_desc": "The action that occurred",
                "desc": "The action performed on the object (e.g., `CREATE`, `UPDATE`, `DELETE`, "
                    "`LOAD`, `SHARE`, `TRANSFER_OWNERSHIP`)",
            },
            "objectId": {
                "type": SoftIntType,
                "display_name": "Object ID",
                "short_desc": "ID of the affected object",
                "desc": "The ID of the object that was affected by the event",
            },
            "eventTimestamp": {
                "type": StringType,
                "display_name": "Event Timestamp",
                "short_desc": "When the event occurred",
                "desc": "ISO 8601 timestamp of when the event occurred",
            },
            "userId": {
                "type": SoftIntOrNothingType,
                "display_name": "User ID",
                "short_desc": "ID of the user who triggered the event",
                "desc": "The ID of the user whose action triggered the event",
            },
            "requestUserId": {
                "type": SoftIntOrNothingType,
                "display_name": "Request User ID",
                "short_desc": "ID of the user who made the request",
                "desc": "The ID of the user who made the API request that triggered the event "
                    "(may differ from `userId` for delegated actions)",
            },
            "source": {
                "type": StringOrNothingType,
                "display_name": "Source",
                "short_desc": "Source of the event",
                "desc": "The source of the event (e.g., `WEB_APP`, `MOBILE_IOS`, `MOBILE_ANDROID`, "
                    "`API`)",
            },
            "additionalDetails": {
                "type": AutoHashOrNothingType,
                "display_name": "Additional Details",
                "short_desc": "Extra event-specific details",
                "desc": "Additional details about the event, such as `sheetId` for row-level events",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SmartSheetDataProvider
