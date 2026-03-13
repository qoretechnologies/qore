# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LemlistDataProvider module definition

/*  LemlistUpdateTaskDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the LemlistDataProvider module
public namespace LemlistDataProvider {

#! Request type for updating a task
public class LemlistUpdateTaskRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "taskId": {
                "type": StringType,
                "display_name": "Task ID",
                "short_desc": "ID of the task to update",
                "desc": "The unique identifier of the task to update",
                "required": True,
                "ref_data": "tasks",
                "example_value": "tsk_abc123",
            },
            "status": {
                "type": SoftStringOrNothingType,
                "display_name": "Status",
                "short_desc": "New task status",
                "desc": "The updated status of the task",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "done", "desc": "Task completed"},
                    <AllowedValueInfo>{"value": "ignored", "desc": "Task ignored"},
                ),
                "preselected": True,
            },
            "assignedTo": {
                "type": SoftStringOrNothingType,
                "display_name": "Assigned To",
                "short_desc": "User to assign the task to",
                "desc": "Updated assignee for the task",
            },
            "dueDate": {
                "type": DateOrNothingType,
                "display_name": "Due Date",
                "short_desc": "Updated task due date",
                "desc": "Updated date and time when this task is due",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for updating a task
public class LemlistUpdateTaskDataProvider inherits LemlistDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-task",
            "desc": "Update a Lemlist manual task",
            "type": "LemlistUpdateTaskDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static LemlistUpdateTaskRequestDataType RequestType();

        #! Response type
        static LemlistTaskDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(LemlistRestClient::LemlistRestClient rest)
            : LemlistDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(LemlistRestClient::LemlistRestConnection conn) : LemlistDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "update-task";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Update a Lemlist manual task";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string task_id = req.taskId;
        hash<auto> body = req - ("taskId",);
        if (body.dueDate) {
            body.dueDate = format_date("YYYY-MM-DDTHH:mm:SS", gmtime(body.dueDate)) + "Z";
        }
        return doPatch(sprintf("/tasks/%s", encode_url(task_id, True)), body);
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace LemlistDataProvider
