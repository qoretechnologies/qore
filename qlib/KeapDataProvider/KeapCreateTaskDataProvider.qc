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
    private {
        const Fields = {
            "title": {
                "type": StringType,
                "display_name": "Title",
                "short_desc": "The task title",
                "desc": "The title of the task",
                "required": True,
            },
            "contact_id": {
                "type": StringOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "The associated contact",
                "desc": "The unique identifier of the contact associated with the task",
            },
            "due_date": {
                "type": DateOrNothingType,
                "display_name": "Due Date",
                "short_desc": "The task due date",
                "desc": "The due date of the task",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The task description",
                "desc": "A description of the task",
            },
            "priority": {
                "type": IntOrNothingType,
                "display_name": "Priority",
                "short_desc": "The task priority",
                "desc": "The priority level of the task",
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
            "user_id": {
                "type": IntOrNothingType,
                "display_name": "User ID",
                "short_desc": "The assigned user",
                "desc": "The ID of the user assigned to the task",
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
