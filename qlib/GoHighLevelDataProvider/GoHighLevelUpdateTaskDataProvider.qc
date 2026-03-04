# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GoHighLevelUpdateTaskDataProvider.qc GoHighLevel update task action data provider

/*  GoHighLevelUpdateTaskDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request data type for the update task action
public class GoHighLevelUpdateTaskRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "contact_id": {
                "type": StringType,
                "display_name": "Contact ID",
                "short_desc": "The contact ID the task belongs to",
                "desc": "The contact ID the task belongs to",
                "required": True,
            },
            "task_id": {
                "type": StringType,
                "display_name": "Task ID",
                "short_desc": "The task ID to update",
                "desc": "The task ID to update",
                "required": True,
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
                "short_desc": "Task description",
                "desc": "Task description",
            },
            "dueDate": {
                "type": StringOrNothingType,
                "display_name": "Due Date",
                "short_desc": "Due date in ISO 8601 format",
                "desc": "Due date in ISO 8601 format",
            },
            "assignedTo": {
                "type": StringOrNothingType,
                "display_name": "Assigned To",
                "short_desc": "User ID to assign to",
                "desc": "User ID to assign to",
            },
            "completed": {
                "type": SoftBoolOrNothingType,
                "display_name": "Completed",
                "short_desc": "Whether the task is completed",
                "desc": "Whether the task is completed",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! GoHighLevel update task action data provider
public class GoHighLevelUpdateTaskDataProvider inherits GoHighLevelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing task in GoHighLevel",
            "type": "GoHighLevelUpdateTaskDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(GoHighLevelRestClient::GoHighLevelRestClient rest) : GoHighLevelDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "update";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Update an existing task in GoHighLevel";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new GoHighLevelUpdateTaskRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new GoHighLevelTaskDataType();
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string contact_id = remove req.contact_id;
        string task_id = remove req.task_id;
        return doPut("contacts/" + contact_id + "/tasks/" + task_id, req).task;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace GoHighLevelDataProvider
