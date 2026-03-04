# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GoHighLevelDataProvider module definition

/*  GoHighLevelTaskDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

%modern

#! Contains all public definitions in the GoHighLevelDataProvider module
public namespace GoHighLevelDataProvider {

#! GoHighLevel task record type
public class GoHighLevelTaskDataType inherits HashDataType {
    public {
        #! Field definitions for GoHighLevel tasks
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Task ID",
                "short_desc": "Unique identifier of the task",
                "desc": "Unique identifier of the task",
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "The task title",
                "desc": "The task title",
            },
            "body": {
                "type": StringOrNothingType,
                "display_name": "Body",
                "short_desc": "The task body/description",
                "desc": "The task body/description",
            },
            "dueDate": {
                "type": StringOrNothingType,
                "display_name": "Due Date",
                "short_desc": "The task due date",
                "desc": "The task due date",
            },
            "completed": {
                "type": SoftBoolOrNothingType,
                "display_name": "Completed",
                "short_desc": "Whether the task is completed",
                "desc": "Whether the task is completed",
            },
            "assignedTo": {
                "type": StringOrNothingType,
                "display_name": "Assigned To",
                "short_desc": "User ID the task is assigned to",
                "desc": "User ID the task is assigned to",
            },
            "contactId": {
                "type": StringOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "The contact this task belongs to",
                "desc": "The contact this task belongs to",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace GoHighLevelDataProvider
