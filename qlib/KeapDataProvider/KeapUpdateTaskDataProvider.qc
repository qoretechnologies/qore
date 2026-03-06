# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KeapUpdateTaskDataProvider.qc Keap update task action data provider

/*  KeapUpdateTaskDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KeapDataProvider module
public namespace KeapDataProvider {

#! Request type for updating a task
public class KeapUpdateTaskRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "task_id": {
                "type": StringType,
                "display_name": "Task ID",
                "short_desc": "The task ID to update",
                "desc": "The unique identifier of the task to update",
                "required": True,
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "The task title",
                "desc": "The new title of the task",
            },
            "due_date": {
                "type": DateOrNothingType,
                "display_name": "Due Date",
                "short_desc": "The task due date",
                "desc": "The new due date of the task",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The task description",
                "desc": "The new description of the task",
            },
            "priority": {
                "type": IntOrNothingType,
                "display_name": "Priority",
                "short_desc": "The task priority",
                "desc": "The new priority level of the task",
            },
            "completed": {
                "type": SoftBoolOrNothingType,
                "display_name": "Completed",
                "short_desc": "Whether the task is completed",
                "desc": "Whether the task is marked as completed",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Keap update task action data provider
public class KeapUpdateTaskDataProvider inherits KeapDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing task in Keap",
            "type": "KeapUpdateTaskDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(KeapRestClient::KeapRestClient rest) : KeapDataProviderBase(rest) {
    }

    string getName() {
        return "update";
    }

    *string getDesc() {
        return "Update an existing task in Keap";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KeapUpdateTaskRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KeapTaskDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = remove req.task_id;
        return doPatch("tasks/" + id, req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KeapDataProvider
