# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CloseTasksDataProvider.qc Close tasks collection data provider

/*  CloseTasksDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CloseDataProvider module
public namespace CloseDataProvider {

#! Close tasks collection data provider
public class CloseTasksDataProvider inherits CloseDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "tasks",
            "desc": "Close tasks data provider for listing and searching tasks",
            "type": "CloseTasksDataProvider",
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
            "lead_id": <DataProviderOptionInfo>{
                "display_name": "Lead ID",
                "short_desc": "Filter by lead ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter tasks by the lead they are associated with",
            },
            "_type": <DataProviderOptionInfo>{
                "display_name": "Task Type",
                "short_desc": "Task type filter",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter tasks by type (e.g., `lead`, `incoming_email`, `outgoing_call`)",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Lead",
                        "short_desc": "Lead tasks",
                        "desc": "Filter to show only lead tasks",
                        "value": "lead",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Incoming Email",
                        "short_desc": "Incoming email tasks",
                        "desc": "Filter to show only incoming email tasks",
                        "value": "incoming_email",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Outgoing Call",
                        "short_desc": "Outgoing call tasks",
                        "desc": "Filter to show only outgoing call tasks",
                        "value": "outgoing_call",
                    },
                ),
            },
            "is_complete": <DataProviderOptionInfo>{
                "display_name": "Is Complete",
                "short_desc": "Filter by completion status",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "Filter tasks by completion status (`True` for complete, `False` for "
                    "incomplete)",
            },
            "assigned_to": <DataProviderOptionInfo>{
                "display_name": "Assigned To",
                "short_desc": "Filter by assigned user",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter tasks by the user ID they are assigned to",
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
            "create": Class::forName("CloseCreateTaskDataProvider"),
            "update": Class::forName("CloseUpdateTaskDataProvider"),
            "delete": Class::forName("CloseDeleteTaskDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(CloseRestClient::CloseRestClient rest) : CloseDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "tasks";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Close tasks data provider for listing and searching tasks";
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
        return new CloseTaskDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If task_id provided, get single record
        if (search_options.task_id) {
            hash<auto> result = doGet("task/" + search_options.task_id);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }

        # Build query params
        hash<auto> params;
        if (search_options.lead_id) {
            params.lead_id = search_options.lead_id;
        }
        if (search_options._type) {
            params._type = search_options._type;
        }
        if (exists search_options.is_complete) {
            params.is_complete = search_options.is_complete ? "true" : "false";
        }
        if (search_options.assigned_to) {
            params.assigned_to = search_options.assigned_to;
        }

        list<hash<auto>> all_records = fetchAllPages("task", params,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CloseDataProvider
