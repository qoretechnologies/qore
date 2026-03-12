# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CloseCreateTaskDataProvider.qc Close create task action data provider

/*  CloseCreateTaskDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a task
public class CloseCreateTaskRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating a task
        const Fields = {
            "lead_id": {
                "type": StringType,
                "display_name": "Lead ID",
                "short_desc": "The lead to associate the task with",
                "desc": "The unique identifier of the lead to associate this task with",
                "required": True,
            },
            "_type": {
                "type": StringType,
                "display_name": "Task Type",
                "short_desc": "The task type",
                "desc": "The type of the task to create: `lead` or `outgoing_call`",
                "required": True,
                "example_value": "lead",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Lead",
                        "short_desc": "Lead task",
                        "desc": "Create a lead task",
                        "value": "lead",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Outgoing Call",
                        "short_desc": "Outgoing call task",
                        "desc": "Create an outgoing call task",
                        "value": "outgoing_call",
                    },
                ),
            },
            "text": {
                "type": StringOrNothingType,
                "display_name": "Text",
                "short_desc": "The task text",
                "desc": "The text description or body of the task",
                "preselected": True,
                "example_value": "Follow up with client about proposal",
            },
            "date": {
                "type": StringOrNothingType,
                "display_name": "Date",
                "short_desc": "The task due date",
                "desc": "The due date of the task in YYYY-MM-DD format",
                "example_value": "2026-03-15",
            },
            "assigned_to": {
                "type": StringOrNothingType,
                "display_name": "Assigned To",
                "short_desc": "The user to assign the task to",
                "desc": "The unique identifier of the user to assign this task to",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Close create task action data provider
public class CloseCreateTaskDataProvider inherits CloseDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new task in Close",
            "type": "CloseCreateTaskDataProvider",
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
        return "create";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new task in Close";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new CloseCreateTaskRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new CloseTaskDataType();
    }

    #! Makes the request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("task", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CloseDataProvider
