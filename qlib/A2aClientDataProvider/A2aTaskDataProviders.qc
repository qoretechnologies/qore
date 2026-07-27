# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore A2A task data providers

/*  Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the A2aClientDataProvider module
public namespace A2aClientDataProvider {
#! A2A tasks container data provider
public class A2aTasksDataProvider inherits A2aClientDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "tasks",
            "desc": "A2A tasks data provider",
            "type": "A2aTasksDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
        };
    }

    constructor(A2aClient a2a) : A2aClientDataProviderBase(a2a) {
    }

    string getName() {
        return "tasks";
    }

    private *list<string> getChildProviderNamesImpl() {
        return ("get", "cancel", "list");
    }

    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        switch (name) {
            case "get":
                return new A2aTaskGetDataProvider(a2a);
            case "cancel":
                return new A2aTaskCancelDataProvider(a2a);
            case "list":
                return new A2aTaskListDataProvider(a2a);
        }
        return NOTHING;
    }

    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! A2A get task data provider
public class A2aTaskGetDataProvider inherits A2aClientDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get",
            "desc": "Gets A2A task status by ID",
            "type": "A2aTaskGetDataProvider",
            "supports_request": True,
        };

    }

    constructor(A2aClient a2a) : A2aClientDataProviderBase(a2a) {
    }

    string getName() {
        return "get";
    }

    *string getDesc() {
        return "Gets A2A task status by ID";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return A2aTaskGetRequestDataType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return A2aTaskResultDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.id);
        @debug("Getting task %y", req.id);
        return a2a.getTask(req.id);
    }

    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! A2A cancel task data provider
public class A2aTaskCancelDataProvider inherits A2aClientDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "cancel",
            "desc": "Cancels an A2A task by ID",
            "type": "A2aTaskCancelDataProvider",
            "supports_request": True,
        };

    }

    constructor(A2aClient a2a) : A2aClientDataProviderBase(a2a) {
    }

    string getName() {
        return "cancel";
    }

    *string getDesc() {
        return "Cancels an A2A task by ID";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return A2aTaskCancelRequestDataType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return A2aTaskResultDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.id);
        @debug("Canceling task %y", req.id);
        return a2a.cancelTask(req.id);
    }

    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! A2A task list request data type
public class A2aTaskListRequestDataType inherits HashDataType {
    public {
        #! Field descriptions
        const Fields = {
            "contextId": {
                "type": StringOrNothingType,
                "display_name": "Context ID",
                "short_desc": "Filter by context ID",
                "desc": "Optional context ID to filter tasks by",
            },
            "pageSize": {
                "type": IntOrNothingType,
                "display_name": "Page Size",
                "short_desc": "Number of results per page (v1.0)",
                "desc": "Optional page size for cursor-based pagination",
            },
            "pageToken": {
                "type": StringOrNothingType,
                "display_name": "Page Token",
                "short_desc": "Pagination cursor (v1.0)",
                "desc": "Optional cursor for retrieving the next page of results",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const A2aTaskListRequestDataType = new A2aTaskListRequestDataType();

#! A2A task list result data type
public class A2aTaskListResultDataType inherits HashDataType {
    public {
        #! Field descriptions
        const Fields = {
            "tasks": {
                "type": AutoListType,
                "display_name": "Tasks",
                "short_desc": "List of tasks",
                "desc": "List of A2A tasks matching the filter criteria",
            },
            "nextPageToken": {
                "type": StringOrNothingType,
                "display_name": "Next Page Token",
                "short_desc": "Cursor for next page (v1.0)",
                "desc": "Cursor token for retrieving the next page; empty if no more results",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const A2aTaskListResultDataType = new A2aTaskListResultDataType();

#! A2A list tasks data provider
public class A2aTaskListDataProvider inherits A2aClientDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list",
            "desc": "Lists A2A tasks with optional filtering",
            "type": "A2aTaskListDataProvider",
            "supports_request": True,
        };

    }

    constructor(A2aClient a2a) : A2aClientDataProviderBase(a2a) {
    }

    string getName() {
        return "list";
    }

    *string getDesc() {
        return "Lists A2A tasks with optional filtering";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return A2aTaskListRequestDataType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return A2aTaskListResultDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @debug("Listing tasks");
        return a2a.listTasks(req);
    }

    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
