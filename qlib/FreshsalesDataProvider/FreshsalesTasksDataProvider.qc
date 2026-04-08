# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FreshsalesTasksDataProvider.qc Freshsales tasks collection data provider

/*  FreshsalesTasksDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FreshsalesDataProvider module
public namespace FreshsalesDataProvider {

#! Freshsales tasks collection data provider
public class FreshsalesTasksDataProvider inherits FreshsalesDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "tasks",
            "desc": "Freshsales tasks data provider for listing and searching tasks",
            "type": "FreshsalesTasksDataProvider",
            "has_record": True,
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
            "id": <DataProviderOptionInfo>{
                "display_name": "Task ID",
                "short_desc": "Get a specific task by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "The task ID to retrieve. When provided, returns a single record",
            },
            "filter": <DataProviderOptionInfo>{
                "display_name": "Filter",
                "short_desc": "Filter tasks by status",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter tasks by status",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "open", "display_name": "Open"},
                    <AllowedValueInfo>{"value": "due today", "display_name": "Due Today"},
                    <AllowedValueInfo>{"value": "due tomorrow", "display_name": "Due Tomorrow"},
                    <AllowedValueInfo>{"value": "overdue", "display_name": "Overdue"},
                    <AllowedValueInfo>{"value": "completed", "display_name": "Completed"},
                ),
                "preselected": True,
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
            "create": Class::forName("FreshsalesCreateTaskDataProvider"),
            "update": Class::forName("FreshsalesUpdateTaskDataProvider"),
            "delete": Class::forName("FreshsalesDeleteTaskDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(RestClientIo::RestClientIo rest) : FreshsalesDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "tasks";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Freshsales tasks data provider for listing and searching tasks";
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
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new FreshsalesTaskDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (search_options.id) {
            hash<auto> result = doGet("tasks/" + string(search_options.id));
            hash<auto> task = result.task ?? result;
            return new DefaultRecordIterator(new ListHashIterator((task,)),
                where_cond, search_options);
        }

        hash<auto> params;
        if (search_options.filter) {
            params.filter = search_options.filter;
        } else {
            params.filter = "open";
        }

        list<hash<auto>> all_records = fetchAllPages("tasks", "tasks", params,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records),
            where_cond, search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace FreshsalesDataProvider
