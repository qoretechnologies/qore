# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LemlistDataProvider module definition

/*  LemlistDeleteTaskDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for deleting a task
public class LemlistDeleteTaskRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "taskId": {
                "type": StringType,
                "display_name": "Task ID",
                "short_desc": "ID of the task to delete",
                "desc": "The unique identifier of the task to delete",
                "required": True,
                "ref_data": "tasks",
                "example_value": "tsk_abc123",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Response type for deleting a task
public class LemlistDeleteTaskResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "deleted": {
                "type": SoftBoolOrNothingType,
                "display_name": "Deleted",
                "short_desc": "Whether the task was deleted",
                "desc": "`True` if the task was successfully deleted",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for deleting a task
public class LemlistDeleteTaskDataProvider inherits LemlistDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-task",
            "desc": "Delete a Lemlist manual task",
            "type": "LemlistDeleteTaskDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static LemlistDeleteTaskRequestDataType RequestType();

        #! Response type
        static LemlistDeleteTaskResponseDataType ResponseType();
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
        return "delete-task";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Delete a Lemlist manual task";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        auto result = doDelete(sprintf("/tasks/%s", encode_url(req.taskId, True)));
        if (!exists result || result.typeCode() == NT_NOTHING) {
            return {"deleted": True};
        }
        return result;
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
