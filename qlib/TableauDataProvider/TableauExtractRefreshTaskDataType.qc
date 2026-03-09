# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TableauDataProvider module definition

/*  TableauExtractRefreshTaskDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the TableauDataProvider module
public namespace TableauDataProvider {

#! Tableau extract refresh task record type
public class TableauExtractRefreshTaskDataType inherits HashDataType {
    public {
        #! Field definitions for Tableau extract refresh tasks
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The extract refresh task ID",
                "desc": "The unique identifier of the extract refresh task",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "The task type",
                "desc": "The type of the extract refresh task",
            },
            "priority": {
                "type": IntOrNothingType,
                "display_name": "Priority",
                "short_desc": "The task priority",
                "desc": "The priority of the task",
            },
            "consecutiveFailedCount": {
                "type": IntOrNothingType,
                "display_name": "Consecutive Failed Count",
                "short_desc": "Number of consecutive failures",
                "desc": "Number of consecutive failures",
            },
            "schedule": {
                "type": AutoHashOrNothingType,
                "display_name": "Schedule",
                "short_desc": "Schedule information",
                "desc": "Schedule information for the task",
            },
            "datasource": {
                "type": AutoHashOrNothingType,
                "display_name": "Datasource",
                "short_desc": "The associated datasource",
                "desc": "The datasource associated with this task",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace TableauDataProvider
