# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file TableauExtractRefreshTasksDataProvider.qc Tableau extract refresh tasks collection data provider

/*  TableauExtractRefreshTasksDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the TableauDataProvider module
public namespace TableauDataProvider {

#! Tableau extract refresh tasks collection data provider
public class TableauExtractRefreshTasksDataProvider inherits TableauDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "extract-refresh-tasks",
            "desc": "Tableau extract refresh tasks data provider for listing and searching "
                "extract refresh tasks",
            "type": "TableauExtractRefreshTasksDataProvider",
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
                "short_desc": "Get a specific extract refresh task by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by task ID for single record lookup",
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
            "delete": Class::forName("TableauDeleteExtractRefreshTaskDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(TableauRestClient::TableauRestClient rest) : TableauDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "extract-refresh-tasks";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Tableau extract refresh tasks data provider for listing and searching "
            "extract refresh tasks";
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
        return new TableauExtractRefreshTaskDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If task_id provided, get single record
        if (search_options.task_id) {
            hash<auto> result = doGet("tasks/extractRefreshes/" + search_options.task_id);
            return new DefaultRecordIterator(new ListHashIterator((result.task,)),
                where_cond, search_options);
        }

        # Build query params
        hash<auto> params;

        list<hash<auto>> all_records = fetchAllPages("tasks/extractRefreshes", "tasks.task",
            params, search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace TableauDataProvider
