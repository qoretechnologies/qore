# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinearDataProvider module definition

/*  LinearProjectsDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the LinearDataProvider module
public namespace LinearDataProvider {

#! Linear projects collection data provider
public class LinearProjectsDataProvider inherits LinearDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "projects",
            "desc": "Linear projects data provider",
            "type": "LinearProjectsDataProvider",
            "supports_read": True,
            "supports_create": True,
            "supports_children": True,
            "supports_native_search": True,
            "has_record": True,
            "children_can_support_records": True,
            "search_options": SearchOptions,
        };

        #! Search options
        const SearchOptions = {
            "state": <DataProviderOptionInfo>{
                "display_name": "State",
                "short_desc": "Filter by project state",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter projects by state (planned, started, paused, completed, canceled)",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of results",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };
    }

    private {
        #! Record type
        static LinearProjectDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(LinearRestClient::LinearRestClient rest) : LinearDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(LinearRestClient::LinearRestConnection conn) : LinearDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "projects";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Linear projects collection data provider";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return ();
    }

    #! Returns a child provider by name (project ID)
    private *AbstractDataProvider getChildProviderImpl(string name) {
        @assert(name.val(), "project ID cannot be empty");
        return new LinearProjectDataProvider(rest, name);
    }

    #! Returns True, as any project ID can be a child
    private bool hasChildProviderImpl(string name) {
        return True;
    }

    #! Searches for projects
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> filter;
        if (search_options.state) {
            filter.state = {"eq": search_options.state};
        }

        hash<auto> vars;
        if (filter) {
            vars.filter = filter;
        }
        @debug(log(LoggerLevel::DEBUG, "searchRecordsImpl: filter=%y", filter));

        int pageSize = search_options.limit ?? DefaultPageSize;

        return new LinearRecordIterator(rest, ListProjectsQuery, "projects", vars, pageSize,
            recordType.getFields());
    }

    #! Creates a new project
    private *hash<auto> createRecordImpl(hash<auto> rec, *hash<auto> create_options) {
        @assert(rec.name.val(), "project name is required");
        @debug(log(LoggerLevel::DEBUG, "createRecordImpl: creating project name=%y", rec.name));
        hash<auto> data = doGraphQL(CreateProjectMutation, {"input": rec});
        @assert(data.projectCreate, "projectCreate response cannot be null");
        if (!data.projectCreate.success) {
            throw "CREATE-ERROR", "Failed to create project";
        }
        @assert(data.projectCreate.project, "created project cannot be null on success");
        @debug(log(LoggerLevel::DEBUG, "createRecordImpl: created project id=%y", data.projectCreate.project.id));
        return data.projectCreate.project;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace LinearDataProvider
