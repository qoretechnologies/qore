# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LemlistDataProvider module definition

/*  LemlistTasksDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Lemlist tasks collection data provider
public class LemlistTasksDataProvider inherits LemlistDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "tasks",
            "desc": "Lemlist tasks data provider - list manual tasks",
            "type": "LemlistTasksDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "taskId": <DataProviderOptionInfo>{
                "display_name": "Task ID",
                "short_desc": "ID of a specific task to retrieve",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "The unique identifier of a task to retrieve",
                "ref_data": "tasks",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of results",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Maximum number of tasks to return (max 100)",
            },
            "offset": <DataProviderOptionInfo>{
                "display_name": "Offset",
                "short_desc": "Number of records to skip",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Number of tasks to skip for pagination",
            },
        };
    }

    private {
        #! Record type
        static LemlistTaskDataType recordType();
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
        return "tasks";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Lemlist tasks data provider - list manual tasks";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for tasks
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If taskId is provided, get a specific task
        if (search_options.taskId) {
            auto response = doGet(sprintf("/tasks/%s", search_options.taskId));
            list<hash<auto>> records;
            if (response.typeCode() == NT_HASH) {
                records = (response,);
            }
            return new DefaultRecordIterator(new ListHashIterator(records), where_cond, NOTHING,
                recordType.getFields());
        }

        hash<auto> params;
        if (search_options.limit) {
            params.limit = search_options.limit;
        } else {
            params.limit = DefaultPageSize;
        }
        if (search_options.offset) {
            params.offset = search_options.offset;
        }

        auto response = doGet("/tasks", params);
        list<hash<auto>> records;
        if (response.typeCode() == NT_LIST) {
            records = response;
        } else if (response.tasks.typeCode() == NT_LIST) {
            records = response.tasks;
        } else if (response.data.typeCode() == NT_LIST) {
            records = response.data;
        }

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, NOTHING,
            recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace LemlistDataProvider
