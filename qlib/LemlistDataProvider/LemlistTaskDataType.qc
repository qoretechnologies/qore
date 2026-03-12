# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LemlistDataProvider module definition

/*  LemlistTaskDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the LemlistDataProvider module
public namespace LemlistDataProvider {

#! Lemlist task data type
public class LemlistTaskDataType inherits HashDataType {
    public {
        const Fields = {
            "_id": {
                "type": StringOrNothingType,
                "display_name": "Task ID",
                "short_desc": "Unique task identifier",
                "desc": "The unique identifier for the task",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "Task type",
                "desc": "The type of task (e.g., `manualEmail`, `linkedinInvite`, `call`)",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Task status",
                "desc": "The current status of the task",
            },
            "leadId": {
                "type": StringOrNothingType,
                "display_name": "Lead ID",
                "short_desc": "Associated lead identifier",
                "desc": "The identifier of the lead this task is associated with",
            },
            "leadEmail": {
                "type": StringOrNothingType,
                "display_name": "Lead Email",
                "short_desc": "Lead email address",
                "desc": "The email address of the lead this task is for",
            },
            "campaignId": {
                "type": StringOrNothingType,
                "display_name": "Campaign ID",
                "short_desc": "Associated campaign identifier",
                "desc": "The identifier of the campaign this task belongs to",
            },
            "campaignName": {
                "type": StringOrNothingType,
                "display_name": "Campaign Name",
                "short_desc": "Campaign name",
                "desc": "The name of the campaign associated with this task",
            },
            "assignedTo": {
                "type": StringOrNothingType,
                "display_name": "Assigned To",
                "short_desc": "Task assignee",
                "desc": "The user or team member this task is assigned to",
            },
            "dueDate": {
                "type": StringOrNothingType,
                "display_name": "Due Date",
                "short_desc": "Task due date",
                "desc": "ISO 8601 timestamp when this task is due",
            },
            "createdAt": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "Task creation timestamp",
                "desc": "ISO 8601 timestamp when the task was created",
            },
            "updatedAt": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Task last update timestamp",
                "desc": "ISO 8601 timestamp when the task was last updated",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace LemlistDataProvider
