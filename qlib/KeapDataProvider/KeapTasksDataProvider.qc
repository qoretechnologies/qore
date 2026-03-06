# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KeapTasksDataProvider.qc Keap tasks collection data provider

/*  KeapTasksDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Keap tasks collection data provider
public class KeapTasksDataProvider inherits KeapDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "tasks",
            "desc": "Keap tasks data provider for listing and searching tasks",
            "type": "KeapTasksDataProvider",
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
            "task_id": <DataProviderOptionInfo>{
                "display_name": "Task ID",
                "short_desc": "Get a specific task by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by task ID for single record lookup",
            },
            "contact_id": <DataProviderOptionInfo>{
                "display_name": "Contact ID",
                "short_desc": "Filter by contact ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter tasks by associated contact ID",
            },
            "completed": <DataProviderOptionInfo>{
                "display_name": "Completed",
                "short_desc": "Filter by completion status",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "Filter tasks by completion status (`True` for completed, `False` for "
                    "incomplete)",
            },
            "user_id": <DataProviderOptionInfo>{
                "display_name": "User ID",
                "short_desc": "Filter by assigned user ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Filter tasks by the assigned user ID",
            },
            "order_by": <DataProviderOptionInfo>{
                "display_name": "Order By",
                "short_desc": "Field to order results by",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Field to order results by",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };

        #! Child provider map
        const ChildMap = {
            "create": Class::forName("KeapCreateTaskDataProvider"),
            "update": Class::forName("KeapUpdateTaskDataProvider"),
            "delete": Class::forName("KeapDeleteTaskDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(KeapRestClient::KeapRestClient rest) : KeapDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "tasks";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Keap tasks data provider for listing and searching tasks";
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
        return new KeapTaskDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If task_id provided, get single record
        if (search_options.task_id) {
            hash<auto> result = doGet("tasks/" + search_options.task_id);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }

        # Build query params
        hash<auto> params;
        if (search_options.contact_id) {
            params.contact_id = search_options.contact_id;
        }
        if (exists search_options.completed) {
            params.completed = search_options.completed;
        }
        if (search_options.user_id) {
            params.user_id = search_options.user_id;
        }
        if (search_options.order_by) {
            params.order_by = search_options.order_by;
        }

        list<hash<auto>> all_records = fetchAllPages("tasks", "tasks", params,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KeapDataProvider
