# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LemlistDataProvider module definition

/*  LemlistCreateTaskDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a task
public class LemlistCreateTaskRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "type": StringType,
                "display_name": "Task Type",
                "short_desc": "Type of task to create",
                "desc": "The type of manual task to create",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "manualEmail", "desc": "Manual email"},
                    <AllowedValueInfo>{"value": "linkedinInvite", "desc": "LinkedIn invite"},
                    <AllowedValueInfo>{"value": "linkedinMessage", "desc": "LinkedIn message"},
                    <AllowedValueInfo>{"value": "call", "desc": "Phone call"},
                    <AllowedValueInfo>{"value": "api", "desc": "API task"},
                ),
            },
            "leadId": {
                "type": StringType,
                "display_name": "Lead ID",
                "short_desc": "Lead to assign the task to",
                "desc": "The identifier of the lead this task is for",
                "required": True,
                "ref_data": "leads",
                "example_value": "lea_abc123",
            },
            "campaignId": {
                "type": SoftStringOrNothingType,
                "display_name": "Campaign ID",
                "short_desc": "Campaign to associate the task with",
                "desc": "The identifier of the campaign this task belongs to",
                "preselected": True,
                "ref_data": "campaigns",
                "example_value": "cam_abc123",
            },
            "assignedTo": {
                "type": SoftStringOrNothingType,
                "display_name": "Assigned To",
                "short_desc": "User to assign the task to",
                "desc": "The email or identifier of the user to assign this task to",
            },
            "dueDate": {
                "type": DateOrNothingType,
                "display_name": "Due Date",
                "short_desc": "Task due date",
                "desc": "Date and time when this task is due",
                "example_value": 2026-03-15T09:00:00,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating a task
public class LemlistCreateTaskDataProvider inherits LemlistDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-task",
            "desc": "Create a manual task in Lemlist",
            "type": "LemlistCreateTaskDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static LemlistCreateTaskRequestDataType RequestType();

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
        return "create-task";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a manual task in Lemlist";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = req;
        if (body.dueDate) {
            body.dueDate = format_date("YYYY-MM-DDTHH:mm:SS", gmtime(body.dueDate)) + "Z";
        }
        return doPost("/tasks", body);
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
