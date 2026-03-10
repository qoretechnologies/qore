# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KeapCreateTaskDataProvider.qc Keap create task action data provider

/*  KeapCreateTaskDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a task
public class KeapCreateTaskRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "assigned_to_user_id": {
                "type": StringType,
                "display_name": "Assigned User ID",
                "short_desc": "The assigned user",
                "desc": "The ID of the user assigned to the task",
                "required": True,
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "The task title",
                "desc": "The title of the task",
                "example_value": "Follow up with lead",
            },
            "contact_id": {
                "type": StringOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "The associated contact",
                "desc": "The unique identifier of the contact associated with the task",
            },
            "due_time": {
                "type": DateOrNothingType,
                "display_name": "Due Time",
                "short_desc": "The task due date/time",
                "desc": "The due date/time of the task",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The task description",
                "desc": "A description of the task",
            },
            "priority": {
                "type": StringOrNothingType,
                "display_name": "Priority",
                "short_desc": "The task priority",
                "desc": "The priority level of the task",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "CRITICAL", "desc": "Critical priority"},
                    <AllowedValueInfo>{"value": "ESSENTIAL", "desc": "Essential priority"},
                    <AllowedValueInfo>{"value": "NONESSENTIAL", "desc": "Non-essential priority"},
                ),
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "The task type",
                "desc": "The type of the task",
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

#! Keap create task action data provider
public class KeapCreateTaskDataProvider inherits KeapDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new task in Keap",
            "type": "KeapCreateTaskDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static *AbstractDataProviderType ResponseType = new KeapTaskDataType();
        static *AbstractDataProviderType RequestType = new KeapCreateTaskRequestDataType();
    }

    constructor(KeapRestClient::KeapRestClient rest) : KeapDataProviderBase(rest) {
    }

    string getName() {
        return "create";
    }

    *string getDesc() {
        return "Create a new task in Keap";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KeapCreateTaskRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KeapTaskDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("tasks", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KeapDataProvider
