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
    public {
        const Fields = {
            "task_id": {
                "type": StringType,
                "display_name": "Task ID",
                "short_desc": "The task ID to update",
                "desc": "The unique identifier of the task to update",
                "required": True,
            },
            "assigned_to_user_id": {
                "type": StringOrNothingType,
                "display_name": "Assigned User ID",
                "short_desc": "The assigned user",
                "desc": "The new assigned user ID for the task",
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "The task title",
                "desc": "The new title of the task",
            },
            "contact_id": {
                "type": StringOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "The associated contact",
                "desc": "The new contact ID associated with the task",
            },
            "due_time": {
                "type": DateOrNothingType,
                "display_name": "Due Time",
                "short_desc": "The task due date/time",
                "desc": "The new due date/time of the task",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The task description",
                "desc": "The new description of the task",
            },
            "priority": {
                "type": StringOrNothingType,
                "display_name": "Priority",
                "short_desc": "The task priority",
                "desc": "The new priority level of the task",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "CRITICAL", "desc": "Critical priority"},
                    <AllowedValueInfo>{"value": "ESSENTIAL", "desc": "Essential priority"},
                    <AllowedValueInfo>{"value": "NONESSENTIAL", "desc": "Non-essential priority"},
                ),
            },
            "completed": {
                "type": SoftBoolOrNothingType,
                "display_name": "Completed",
                "short_desc": "Whether the task is completed",
                "desc": "Whether the task is marked as completed",
            },
            "completion_time": {
                "type": DateOrNothingType,
                "display_name": "Completion Time",
                "short_desc": "When the task was completed",
                "desc": "The date/time when the task was completed",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "The task type",
                "desc": "The new type of the task",
            },
            "remind_time_mins": {
                "type": IntOrNothingType,
                "display_name": "Reminder Minutes",
                "short_desc": "Minutes before reminder",
                "desc": "Number of minutes before the start time to send a reminder",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 5},
                    <AllowedValueInfo>{"value": 10},
                    <AllowedValueInfo>{"value": 15},
                    <AllowedValueInfo>{"value": 30},
                    <AllowedValueInfo>{"value": 60},
                    <AllowedValueInfo>{"value": 120},
                    <AllowedValueInfo>{"value": 240},
                    <AllowedValueInfo>{"value": 480},
                    <AllowedValueInfo>{"value": 1440},
                    <AllowedValueInfo>{"value": 2880},
                ),
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

        static *AbstractDataProviderType ResponseType = new KeapTaskDataType();
        static *AbstractDataProviderType RequestType = new KeapUpdateTaskRequestDataType();
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
