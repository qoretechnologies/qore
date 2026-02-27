# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelTimesheetDataType.qc Deel timesheet record data type

/*  DeelTimesheetDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DeelDataProvider module
public namespace DeelDataProvider {

#! Deel timesheet record type
public class DeelTimesheetDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftStringType,
                "display_name": "ID",
                "short_desc": "The timesheet ID",
                "desc": "The unique identifier of the timesheet entry",
            },
            "contract_id": {
                "type": StringOrNothingType,
                "display_name": "Contract ID",
                "short_desc": "The associated contract ID",
                "desc": "The ID of the contract this timesheet belongs to",
            },
            "date": {
                "type": StringOrNothingType,
                "display_name": "Date",
                "short_desc": "The timesheet date",
                "desc": "The date of the timesheet entry (YYYY-MM-DD format)",
                "example_value": "2026-02-01",
            },
            "quantity": {
                "type": NumberOrNothingType,
                "display_name": "Hours (Quantity)",
                "short_desc": "The number of hours worked",
                "desc": "The number of hours worked (returned as `quantity` by the Deel API)",
                "example_value": 8.0,
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Work description",
                "desc": "A description of the work performed",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The timesheet status",
                "desc": "The current status of the timesheet "
                    "(e.g., `pending`, `approved`, `rejected`)",
                "example_value": "pending",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the timesheet was created",
                "desc": "The timestamp when the timesheet entry was created (ISO 8601 format)",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the timesheet was last updated",
                "desc": "The timestamp when the timesheet entry was last updated (ISO 8601 format)",
            },
            "worker": {
                "type": AutoHashOrNothingType,
                "display_name": "Worker",
                "short_desc": "The worker details",
                "desc": "Information about the worker who submitted the timesheet",
            },
            "reviewer": {
                "type": AutoHashOrNothingType,
                "display_name": "Reviewer",
                "short_desc": "The reviewer details",
                "desc": "Information about who reviewed the timesheet",
            },
            "rejection_reason": {
                "type": StringOrNothingType,
                "display_name": "Rejection Reason",
                "short_desc": "Reason for rejection",
                "desc": "The reason the timesheet was rejected, if applicable",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace DeelDataProvider
