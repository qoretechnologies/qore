# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GoHighLevelTasksDataProvider.qc GoHighLevel tasks collection data provider

/*  GoHighLevelTasksDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! GoHighLevel tasks collection data provider
public class GoHighLevelTasksDataProvider inherits GoHighLevelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "tasks",
            "desc": "GoHighLevel tasks data provider for listing and searching tasks",
            "type": "GoHighLevelTasksDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "contact_id": <DataProviderOptionInfo>{
                "display_name": "Contact ID",
                "short_desc": "The contact ID to list tasks for (required)",
                "type": AbstractDataProviderType::get(SoftStringType),
                "desc": "The contact ID to list tasks for (required)",
                "required": True,
            },
            "task_id": <DataProviderOptionInfo>{
                "display_name": "Task ID",
                "short_desc": "Get a specific task by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Get a specific task by ID",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of results to return",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };

        #! Child provider map
        const ChildMap = {
            "create": Class::forName("GoHighLevelCreateTaskDataProvider"),
            "update": Class::forName("GoHighLevelUpdateTaskDataProvider"),
            "delete": Class::forName("GoHighLevelDeleteTaskDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(GoHighLevelRestClient::GoHighLevelRestClient rest) : GoHighLevelDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "tasks";
    }

    #! Returns the provider description
    *string getDesc() {
        return "GoHighLevel tasks data provider for listing and searching tasks";
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    #! Returns the record type
    private *AbstractDataProviderType getRecordTypeImpl() {
        return new GoHighLevelTaskDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # contact_id is required for the tasks API
        string contact_id = search_options.contact_id;
        if (!contact_id) {
            throw "GOHIGHLEVEL-TASKS-ERROR", "contact_id is required to list tasks";
        }

        # If task_id provided, get single record
        if (search_options.task_id) {
            hash<auto> result = doGet("contacts/" + contact_id + "/tasks/" + search_options.task_id);
            return new DefaultRecordIterator(new ListHashIterator((result.task,)),
                where_cond, search_options);
        }

        # List all tasks for the contact
        *hash<auto> qparams;
        if (search_options.limit) {
            qparams.limit = search_options.limit;
        }
        hash<auto> response = doGet("contacts/" + contact_id + "/tasks", qparams);
        return new DefaultRecordIterator(new ListHashIterator(response.tasks ?? ()),
            where_cond, search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace GoHighLevelDataProvider
