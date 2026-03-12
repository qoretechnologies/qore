# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CloseUpdateTaskDataProvider.qc Close update task action data provider

/*  CloseUpdateTaskDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CloseDataProvider module
public namespace CloseDataProvider {

#! Request type for updating a task
public class CloseUpdateTaskRequestDataType inherits HashDataType {
    public {
        #! Field definitions for updating a task
        const Fields = {
            "task_id": {
                "type": StringType,
                "display_name": "Task ID",
                "short_desc": "The task ID to update",
                "desc": "The unique identifier of the task to update",
                "required": True,
            },
            "assigned_to": {
                "type": StringOrNothingType,
                "display_name": "Assigned To",
                "short_desc": "The user to assign the task to",
                "desc": "The new user ID to assign this task to",
            },
            "date": {
                "type": StringOrNothingType,
                "display_name": "Date",
                "short_desc": "The task due date",
                "desc": "The new due date of the task in YYYY-MM-DD format",
                "example_value": "2026-03-15",
            },
            "is_complete": {
                "type": BoolOrNothingType,
                "display_name": "Is Complete",
                "short_desc": "Whether the task is complete",
                "desc": "Set to `True` to mark the task as complete, or `False` to mark it as "
                    "incomplete",
            },
            "text": {
                "type": StringOrNothingType,
                "display_name": "Text",
                "short_desc": "The task text",
                "desc": "The new text description or body of the task",
                "example_value": "Updated follow-up notes",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Close update task action data provider
public class CloseUpdateTaskDataProvider inherits CloseDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing task in Close",
            "type": "CloseUpdateTaskDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(CloseRestClient::CloseRestClient rest) : CloseDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "update";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Update an existing task in Close";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new CloseUpdateTaskRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new CloseTaskDataType();
    }

    #! Makes the request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string task_id = remove req.task_id;
        return doPut("task/" + task_id, req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CloseDataProvider
