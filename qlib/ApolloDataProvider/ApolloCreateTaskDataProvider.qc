# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ApolloCreateTaskDataProvider.qc Apollo create task action data provider

/*  ApolloCreateTaskDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ApolloDataProvider module
public namespace ApolloDataProvider {

#! Request type for creating a task
public class ApolloCreateTaskRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "The task type",
                "desc": "The task type",
                "preselected": True,
                "example_value": "call",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "call", "desc": "Phone call"},
                    <AllowedValueInfo>{"value": "email", "desc": "Email"},
                    <AllowedValueInfo>{"value": "todo", "desc": "To-do item"},
                    <AllowedValueInfo>{"value": "action_item", "desc": "Action item"},
                    <AllowedValueInfo>{"value": "linkedin", "desc": "LinkedIn outreach"},
                ),
            },
            "priority": {
                "type": StringOrNothingType,
                "display_name": "Priority",
                "short_desc": "The task priority",
                "desc": "The task priority level",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "high", "desc": "High priority"},
                    <AllowedValueInfo>{"value": "normal", "desc": "Normal priority"},
                    <AllowedValueInfo>{"value": "low", "desc": "Low priority"},
                ),
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The task status",
                "desc": "The task completion status",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "open", "desc": "Open task"},
                    <AllowedValueInfo>{"value": "complete", "desc": "Completed task"},
                ),
            },
            "note": {
                "type": StringOrNothingType,
                "display_name": "Note",
                "short_desc": "Notes for the task",
                "desc": "Notes for the task",
            },
            "due_date": {
                "type": StringOrNothingType,
                "display_name": "Due Date",
                "short_desc": "The due date",
                "desc": "The due date in ISO 8601 format",
            },
            "user_id": {
                "type": StringOrNothingType,
                "display_name": "User ID",
                "short_desc": "The assigned user ID",
                "desc": "The assigned user ID",
            },
            "contact_ids": {
                "type": SoftListOrNothingType,
                "display_name": "Contact IDs",
                "short_desc": "Related contact IDs",
                "desc": "List of contact IDs associated with this task",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Apollo create task action data provider
public class ApolloCreateTaskDataProvider inherits ApolloDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new task in Apollo",
            "type": "ApolloCreateTaskDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static *AbstractDataProviderType ResponseType = new ApolloTaskDataType();
        static *AbstractDataProviderType RequestType = new ApolloCreateTaskRequestDataType();
    }

    constructor(ApolloRestClient::ApolloRestClient rest) : ApolloDataProviderBase(rest) {
    }

    string getName() {
        return "create";
    }

    *string getDesc() {
        return "Create a new task in Apollo";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ApolloCreateTaskRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new ApolloTaskDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> result = doPost("tasks", {} + req);
        return result.task ?? result;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ApolloDataProvider
