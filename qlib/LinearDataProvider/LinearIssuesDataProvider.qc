# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinearDataProvider module definition

/*  LinearIssuesDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Linear issues collection data provider
public class LinearIssuesDataProvider inherits LinearDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "issues",
            "desc": "Linear issues data provider",
            "type": "LinearIssuesDataProvider",
            "supports_read": True,
            "supports_create": True,
            "supports_children": True,
            "supports_native_search": True,
            "has_record": True,
            "children_can_support_records": True,
            "search_options": SearchOptions,
            "create_options": CreateOptions,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "teamId": <DataProviderOptionInfo>{
                "display_name": "Team ID",
                "short_desc": "Filter by team ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter issues by team ID",
            },
            "projectId": <DataProviderOptionInfo>{
                "display_name": "Project ID",
                "short_desc": "Filter by project ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter issues by project ID",
            },
            "cycleId": <DataProviderOptionInfo>{
                "display_name": "Cycle ID",
                "short_desc": "Filter by cycle ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter issues by cycle ID",
            },
            "assigneeId": <DataProviderOptionInfo>{
                "display_name": "Assignee ID",
                "short_desc": "Filter by assignee ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter issues by assignee user ID",
            },
            "stateId": <DataProviderOptionInfo>{
                "display_name": "State ID",
                "short_desc": "Filter by state ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter issues by workflow state ID",
            },
            "priority": <DataProviderOptionInfo>{
                "display_name": "Priority",
                "short_desc": "Filter by priority",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Filter issues by priority (0-4)",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of results",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };

        #! Create options
        const CreateOptions = {};
    }

    private {
        #! Record type
        static LinearIssueDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(LinearRestClient::LinearRestClient rest) : LinearDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(LinearRestClient::LinearRestConnection conn) : LinearDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "issues";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Linear issues collection data provider";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        # Issues are accessed by ID directly
        return ();
    }

    #! Returns a child provider by name (issue ID)
    private *AbstractDataProvider getChildProviderImpl(string name) {
        @assert(name.val(), "issue ID cannot be empty");
        return new LinearIssueDataProvider(rest, name);
    }

    #! Returns True, as any issue ID can be a child
    private bool hasChildProviderImpl(string name) {
        return True;
    }

    #! Searches for issues
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> filter;

        # Build filter from search options
        if (search_options.teamId) {
            filter.team = {"id": {"eq": search_options.teamId}};
        }
        if (search_options.projectId) {
            filter.project = {"id": {"eq": search_options.projectId}};
        }
        if (search_options.cycleId) {
            filter.cycle = {"id": {"eq": search_options.cycleId}};
        }
        if (search_options.assigneeId) {
            filter.assignee = {"id": {"eq": search_options.assigneeId}};
        }
        if (search_options.stateId) {
            filter.state = {"id": {"eq": search_options.stateId}};
        }
        if (exists search_options.priority) {
            filter.priority = {"eq": search_options.priority};
        }

        hash<auto> vars;
        if (filter) {
            vars.filter = filter;
        }
        @debug(log(LoggerLevel::DEBUG, "searchRecordsImpl: filter=%y", filter));

        int pageSize = search_options.limit ?? DefaultPageSize;

        return new LinearRecordIterator(rest, ListIssuesQuery, "issues", vars, pageSize,
            recordType.getFields());
    }

    #! Creates a new issue
    private *hash<auto> createRecordImpl(hash<auto> rec, *hash<auto> create_options) {
        @assert(rec.teamId.val(), "teamId is required to create an issue");
        @assert(rec.title.val(), "title is required to create an issue");
        @debug(log(LoggerLevel::DEBUG, "createRecordImpl: creating issue with title=%y teamId=%y", rec.title,
            rec.teamId));
        hash<auto> data = doGraphQL(CreateIssueMutation, {"input": rec});
        @assert(data.issueCreate, "issueCreate response cannot be null");
        if (!data.issueCreate.success) {
            throw "CREATE-ERROR", "Failed to create issue";
        }
        @assert(data.issueCreate.issue, "created issue cannot be null on success");
        @debug(log(LoggerLevel::DEBUG, "createRecordImpl: created issue id=%y", data.issueCreate.issue.id));
        return data.issueCreate.issue;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace LinearDataProvider
