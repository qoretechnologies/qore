# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GoHighLevelDeleteTaskDataProvider.qc GoHighLevel delete task action data provider

/*  GoHighLevelDeleteTaskDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request data type for the delete task action
public class GoHighLevelDeleteTaskRequestDataType inherits HashDataType {
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
                "short_desc": "The task ID to delete",
                "desc": "The task ID to delete",
                "required": True,
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! GoHighLevel delete task action data provider
public class GoHighLevelDeleteTaskDataProvider inherits GoHighLevelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete",
            "desc": "Delete a task in GoHighLevel",
            "type": "GoHighLevelDeleteTaskDataProvider",
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
        return "delete";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Delete a task in GoHighLevel";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new GoHighLevelDeleteTaskRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doDelete("contacts/" + req.contact_id + "/tasks/" + req.task_id);
        return {};
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace GoHighLevelDataProvider
