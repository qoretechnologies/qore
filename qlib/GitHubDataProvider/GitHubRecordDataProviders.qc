# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GitHubDataProvider module definition

/*  GitHubRecordDataProviders.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the GitHubDataProvider module
public namespace GitHubDataProvider {
#! Base class for the GitHub record-based tables
/** GitHub is not a database, and these tables are not its response shapes.  Each one is a **flattened,
    writable projection** the application owns: an issue's labels arrive from GitHub as objects and are
    presented here as a list of names, its assignee as a login rather than a user object, and a pull
    request's base and head as branch names rather than as the repository-and-ref objects GitHub returns.

    That is why the record types here are declared rather than generated, unlike everything on the action
    surface.  A generated type would be GitHub's own shape, which is not what a record consumer writes back:
    the same record has to be readable and creatable, and GitHub accepts label **names** on create while
    returning label **objects** on read.

    The projection is the one the superseded TypeScript application defined, kept property for property,
    because existing mappers read these column names.

    @section githubrecord_scope The Repository Is a Search and Create Option

    A table is scoped to one repository, and \c owner and \c repo are therefore required search and create
    options rather than connection settings — exactly as in the application this module replaces.

    @section githubrecord_lag A New Record Is Not Immediately Listed

    GitHub's collection endpoints lag its writes by a few seconds: an issue created through
    @ref createRecord() — or through the \c "issues-create" action — is readable straight away by number,
    and is **not** yet in what the listing returns.  Verified against the live API: creating an issue and
    searching the table in the same breath returns the records that existed before it.

    So a workflow that creates a record and then searches for it will not find it, and should use what
    @ref createRecord() returned rather than reading it back.  This is GitHub's behaviour, not this
    module's; there is nothing to poll or retry that would make it appear sooner, and the record is not
    lost.
*/
public class GitHubTableDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! The repository options every table needs, for both searching and creating
        const RepoOptions = {
            "owner": <DataProviderOptionInfo>{
                "display_name": "Owner",
                "short_desc": "The user or organization that owns the repository",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The account the repository belongs to — a user or an organization",
                "required": True,
                "preselected": True,
                "ref_data": GitHubManifest::RefOwner,
                "allowed_values_creatable": True,
            },
            "repo": <DataProviderOptionInfo>{
                "display_name": "Repository",
                "short_desc": "The name of the repository, without the owner",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The repository name on its own, **not** `owner/name`",
                "required": True,
                "preselected": True,
                "ref_data": GitHubManifest::RefRepository,
                "allowed_values_creatable": True,
                "depends_on": ("owner",),
            },
        };

        #! The limit option every table's search accepts
        const LimitOption = {
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Stop after this many records",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Stop after this many records; without it the whole collection is read, a page of "
                    + GitHubManifest::MaxPerPage + " at a time",
            },
        };

        #! The sort direction option
        const DirectionOption = {
            "direction": <DataProviderOptionInfo>{
                "display_name": "Direction",
                "short_desc": "\"asc\" or \"desc\"",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The sort direction: `asc` or `desc`",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "asc", "desc": "oldest first"},
                    <AllowedValueInfo>{"value": "desc", "desc": "newest first"},
                ),
            },
        };

        #! The fields every table carries, none of which can be written
        const CommonReadOnlyFields = (
            {"name": "id", "desc": "the unique ID; not writable", "type": "*int"},
            {"name": "created_at", "desc": "when the record was created; not writable", "type": "*string"},
            {"name": "updated_at", "desc": "when the record last changed; not writable", "type": "*string"},
        );
    }

    private {
        #! The REST client used for API calls
        *GitHubRestClient::GitHubRestClientIo rest;

        #! The record fields, built on first use
        *hash<string, AbstractDataField> record_type;
    }

    #! Creates the object
    constructor(*GitHubRestClient::GitHubRestClientIo rest) {
        self.rest = rest;
    }

    #! Returns the fields of this table's record type
    abstract private list<hash<auto>> getRecordFields();

    #! Returns the path of the repository collection backing this table
    /** @param owner the repository owner
        @param repo the repository name
    */
    abstract private string getCollectionPath(string owner, string repo);

    #! Returns a GitHub record in this table's flattened shape
    abstract private hash<auto> formatRecord(hash<auto> record);

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        if (!record_type) {
            hash<string, AbstractDataField> rv = {};
            foreach hash<auto> f in (getRecordFields()) {
                rv{f.name} = new QoreDataField(f.name, f.desc, AbstractDataProviderTypeMap{f."type"});
            }
            record_type = rv;
        }
        return record_type;
    }

    #! Returns an iterator over the records matching the search criteria
    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> scope = getRepoScope(search_options, "search");
        # "auto!" because the query arguments are a mix of strings and integers, and a hash narrowed to the
        # type of its first value would reject the rest
        hash<auto!> args = getSearchArgs(search_options);
        list<hash<auto>> records = getRecords(getCollectionPath(scope.owner, scope.repo), args,
            search_options.limit ? search_options.limit.toInt() : NOTHING);
        return new DefaultRecordIterator<hash<auto>>(new ListHashIterator(records), where_cond,
            search_options);
    }

    #! Returns the query arguments a search sends, beyond paging
    private hash<auto> getSearchArgs(*hash<auto> search_options) {
        hash<auto!> args = {};
        foreach string name in (("state", "sort", "direction")) {
            if (exists search_options{name} && search_options{name}.toString().val()) {
                args{name} = search_options{name}.toString();
            }
        }
        return args;
    }

    #! Returns the owner and repository a request is scoped to
    /** @param options the search or create options
        @param what \c "search" or \c "create", for the error message

        @throw GITHUB-RECORD-ERROR the options do not name a repository
    */
    private hash<auto> getRepoScope(*hash<auto> options, string what) {
        *string owner = options.owner.toString();
        *string repo = options.repo.toString();
        if (!owner.val() || !repo.val()) {
            throw "GITHUB-RECORD-ERROR", sprintf("the %y table needs the \"owner\" and \"repo\" %s options "
                "to name the repository to work in", getName(), what);
        }
        return {"owner": owner, "repo": repo};
    }

    #! Reads a GitHub collection, following pages until it is exhausted or the limit is reached
    /** @param path the collection path
        @param args the query arguments, without paging
        @param limit stop after this many records, if given

        @return the records, in this table's flattened shape
    */
    private list<hash<auto>> getRecords(string path, *hash<auto> args, *int limit) {
        if (!rest) {
            throw "GITHUB-RECORD-ERROR", "no REST client is available; this data provider was created "
                "without one";
        }
        int per_page = GitHubManifest::MaxPerPage;
        list<hash<auto>> rv = ();
        int page = 0;
        while (True) {
            if (exists limit && rv.size() >= limit) {
                break;
            }
            hash<auto!> qargs = (args ?? {}) + {"per_page": per_page, "page": ++page};
            list<string> parts = ();
            foreach hash<auto> i in (qargs.pairIterator()) {
                if (exists i.value) {
                    parts += sprintf("%s=%s", i.key, encode_url(i.value.toString(), True));
                }
            }
            hash<RestClient::RestResponseInfo> resp = rest.restGet(path + "?" + join("&", parts));
            auto body = resp.body;
            if (!exists body) {
                break;
            }
            if (body.typeCode() != NT_LIST) {
                throw "GITHUB-RECORD-ERROR", sprintf("expected a list from %y, but got type %y", path,
                    body.type());
            }
            foreach auto rec in (body) {
                if (rec.typeCode() != NT_HASH) {
                    continue;
                }
                if (exists limit && rv.size() >= limit) {
                    break;
                }
                rv += formatRecord(rec);
            }
            # a short page is the last page: GitHub reports the next page in the "Link" response header, and
            # a page smaller than the size asked for means there is nothing after it
            if (body.size() < per_page) {
                break;
            }
        }
        return rv;
    }

    #! Creates a record and returns it in this table's flattened shape
    private *hash<auto> createRecordImpl(hash<auto> rec, *hash<auto> create_options) {
        if (!rest) {
            throw "GITHUB-RECORD-ERROR", "no REST client is available; this data provider was created "
                "without one";
        }
        hash<auto> scope = getRepoScope(create_options, "create");
        # a record arrives with every column present, and GitHub rejects a null where it wants a value or
        # an omitted property, so the empty ones are dropped rather than sent
        hash<auto!> body = {};
        foreach hash<auto> i in (rec.pairIterator()) {
            if (!exists i.value) {
                continue;
            }
            if (i.value.typeCode() == NT_LIST && !i.value) {
                continue;
            }
            if (i.value.typeCode() == NT_STRING && !i.value.val()) {
                continue;
            }
            body{i.key} = i.value;
        }
        # the identity and timestamps are GitHub's to assign
        body -= ("id", "created_at", "updated_at");
        hash<RestClient::RestResponseInfo> resp = rest.restPost(getCollectionPath(scope.owner, scope.repo),
            body);
        if (resp.body.typeCode() != NT_HASH) {
            throw "GITHUB-RECORD-ERROR", sprintf("expected a record from GitHub, but got type %y",
                resp.body.type());
        }
        return formatRecord(resp.body);
    }
}

#! The GitHub issues table
public class GitHubIssuesDataProvider inherits GitHubTableDataProviderBase {
    public {
        #! The table name
        const TableName = "issues";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": TableName,
            "type": "GitHubIssuesDataProvider",
            "has_record": True,
            "supports_read": True,
            "supports_create": True,
            "supports_native_search": True,
            "search_options": SearchOptions,
            "create_options": CreateOptions,
        };

        #! Search options
        const SearchOptions = RepoOptions + LimitOption + DirectionOption + {
            "state": <DataProviderOptionInfo>{
                "display_name": "State",
                "short_desc": "Return open, closed or all issues",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Which issues to return; GitHub's own default is `open`",
                "default_value": "all",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "open", "desc": "only open issues"},
                    <AllowedValueInfo>{"value": "closed", "desc": "only closed issues"},
                    <AllowedValueInfo>{"value": "all", "desc": "both"},
                ),
            },
            "sort": <DataProviderOptionInfo>{
                "display_name": "Sort",
                "short_desc": "What to sort by",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "What to sort by",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "created", "desc": "when the issue was opened"},
                    <AllowedValueInfo>{"value": "updated", "desc": "when the issue last changed"},
                    <AllowedValueInfo>{"value": "comments", "desc": "how many comments it has"},
                ),
            },
        };

        #! Create options
        const CreateOptions = RepoOptions;
    }

    #! Creates the object
    constructor(*GitHubRestClient::GitHubRestClientIo rest) : GitHubTableDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return TableName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "GitHub issues as records; note that GitHub returns pull requests here as well";
    }

    #! Returns the fields of this table's record type
    private list<hash<auto>> getRecordFields() {
        return CommonReadOnlyFields + (
            {"name": "number", "desc": "the issue number within the repository; not writable",
                "type": "*int"},
            {"name": "state", "desc": "\"open\" or \"closed\"", "type": "*string"},
            {"name": "title", "desc": "the issue title", "type": "string"},
            {"name": "body", "desc": "the issue description", "type": "*string"},
            {"name": "assignee", "desc": "the login of the user assigned", "type": "*string"},
            {"name": "assignees", "desc": "the logins of the users assigned", "type": "*list"},
            {"name": "labels", "desc": "the names of the labels applied", "type": "*list"},
            {"name": "milestone", "desc": "the title of the milestone", "type": "*string"},
            {"name": "type", "desc": "the issue type, ex: \"Bug\"", "type": "*string"},
        );
    }

    #! Returns the path of the repository collection backing this table
    private string getCollectionPath(string owner, string repo) {
        return sprintf("/repos/%s/%s/issues", owner, repo);
    }

    #! Flattens a GitHub issue to this table's shape
    private hash<auto> formatRecord(hash<auto> record) {
        return record{(map $1.name, getRecordFields())} + {
            "labels": (map $1.name ?? $1.toString(), record.labels) ?? (),
            "assignee": record.assignee.login,
            "assignees": (map $1.login, record.assignees) ?? (),
            "milestone": record.milestone.title,
            "type": record."type".name ?? record."type",
        };
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! The GitHub pull requests table
public class GitHubPullsDataProvider inherits GitHubTableDataProviderBase {
    public {
        #! The table name
        const TableName = "pulls";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": TableName,
            "type": "GitHubPullsDataProvider",
            "has_record": True,
            "supports_read": True,
            "supports_create": True,
            "supports_native_search": True,
            "search_options": SearchOptions,
            "create_options": CreateOptions,
        };

        #! Search options
        const SearchOptions = RepoOptions + LimitOption + DirectionOption + {
            "state": <DataProviderOptionInfo>{
                "display_name": "State",
                "short_desc": "Return open, closed or all pull requests",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Which pull requests to return; GitHub's own default is `open`",
                "default_value": "all",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "open", "desc": "only open pull requests"},
                    <AllowedValueInfo>{"value": "closed", "desc": "only closed pull requests"},
                    <AllowedValueInfo>{"value": "all", "desc": "both"},
                ),
            },
            "sort": <DataProviderOptionInfo>{
                "display_name": "Sort",
                "short_desc": "What to sort by",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "What to sort by",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "created", "desc": "when the pull request was opened"},
                    <AllowedValueInfo>{"value": "updated", "desc": "when it last changed"},
                    <AllowedValueInfo>{"value": "popularity", "desc": "how many comments it has"},
                    <AllowedValueInfo>{"value": "long-running",
                        "desc": "open longest, among those open over a month"},
                ),
            },
        };

        #! Create options
        const CreateOptions = RepoOptions;
    }

    #! Creates the object
    constructor(*GitHubRestClient::GitHubRestClientIo rest) : GitHubTableDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return TableName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "GitHub pull requests as records";
    }

    #! Returns the fields of this table's record type
    private list<hash<auto>> getRecordFields() {
        return CommonReadOnlyFields + (
            {"name": "number", "desc": "the pull request number within the repository; not writable",
                "type": "*int"},
            {"name": "state", "desc": "\"open\" or \"closed\"", "type": "*string"},
            {"name": "title", "desc": "the pull request title", "type": "*string"},
            {"name": "head", "desc": "the branch the changes are on", "type": "string"},
            {"name": "base", "desc": "the branch the changes are for", "type": "string"},
            {"name": "head_repo", "desc": "the repository the head branch is in, for a fork",
                "type": "*string"},
            {"name": "body", "desc": "the pull request description", "type": "*string"},
            {"name": "maintainer_can_modify", "desc": "whether maintainers may push to the head branch",
                "type": "*bool"},
            {"name": "draft", "desc": "whether the pull request is a draft", "type": "*bool"},
            {"name": "issue", "desc": "the issue number to convert into a pull request, on create only",
                "type": "*int"},
        );
    }

    #! Returns the path of the repository collection backing this table
    private string getCollectionPath(string owner, string repo) {
        return sprintf("/repos/%s/%s/pulls", owner, repo);
    }

    #! Flattens a GitHub pull request to this table's shape
    private hash<auto> formatRecord(hash<auto> record) {
        return record{(map $1.name, getRecordFields())} + {
            "base": record.base.ref,
            "head": record.head.ref,
            "head_repo": record.head.repo.full_name,
        };
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! The GitHub releases table
public class GitHubReleasesDataProvider inherits GitHubTableDataProviderBase {
    public {
        #! The table name
        const TableName = "releases";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": TableName,
            "type": "GitHubReleasesDataProvider",
            "has_record": True,
            "supports_read": True,
            "supports_create": True,
            "supports_native_search": True,
            "search_options": SearchOptions,
            "create_options": CreateOptions,
        };

        #! Search options
        /** GitHub's releases collection accepts no sort, state or direction: it always returns releases
            newest first, so declaring those options would offer filters the API ignores.
        */
        const SearchOptions = RepoOptions + LimitOption;

        #! Create options
        const CreateOptions = RepoOptions;
    }

    #! Creates the object
    constructor(*GitHubRestClient::GitHubRestClientIo rest) : GitHubTableDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return TableName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "GitHub releases as records";
    }

    #! Returns the fields of this table's record type
    private list<hash<auto>> getRecordFields() {
        return CommonReadOnlyFields + (
            {"name": "tag_name", "desc": "the git tag for the release", "type": "string"},
            {"name": "target_commitish", "desc": "the branch or commit to tag, when the tag is new",
                "type": "*string"},
            {"name": "name", "desc": "the release title", "type": "*string"},
            {"name": "body", "desc": "the release notes", "type": "*string"},
            {"name": "draft", "desc": "whether the release is unpublished", "type": "*bool"},
            {"name": "prerelease", "desc": "whether the release is a pre-release", "type": "*bool"},
            {"name": "discussion_category_name",
                "desc": "the discussion category to open a discussion in", "type": "*string"},
            {"name": "generate_release_notes",
                "desc": "write the notes from the commits since the last release, on create only",
                "type": "*bool"},
            {"name": "make_latest", "desc": "\"true\", \"false\" or \"legacy\"", "type": "*string"},
        );
    }

    #! Returns the path of the repository collection backing this table
    private string getCollectionPath(string owner, string repo) {
        return sprintf("/repos/%s/%s/releases", owner, repo);
    }

    #! Narrows a GitHub release to this table's shape
    private hash<auto> formatRecord(hash<auto> record) {
        return record{(map $1.name, getRecordFields())};
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! The container holding the GitHub record-based tables
/** @section githubtables_actions The Table Actions

    The three tables are reachable from the action surface through \c "search", \c "search-single" and
    \c "create", registered in the module's \c init closure against the path \c "/tables/{table}".  Those
    three action IDs are the ones the superseded TypeScript application registered, so a workflow that
    already calls them keeps working.

    An action names its table in the path rather than in an option, and its remaining options therefore
    depend on which table was named — \c issues accepts a state and a label filter that \c releases has no
    notion of.  That is what \c data_dependent_options declares: the options are resolved by walking to the
    chosen table, and @ref RecordActionProviderInfo describes only what all three tables have in common.
*/
public class GitHubTablesDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": GitHubDataProvider::TablesChildName,
            "type": "GitHubTablesDataProvider",
            "supports_children": True,
            "children_can_support_records": True,
        };

        #! The tables, keyed by name
        const TableMap = {
            GitHubIssuesDataProvider::TableName: Class::forName("GitHubIssuesDataProvider"),
            GitHubPullsDataProvider::TableName: Class::forName("GitHubPullsDataProvider"),
            GitHubReleasesDataProvider::TableName: Class::forName("GitHubReleasesDataProvider"),
        };

        #! The table named in a record action's path
        const TablePathVar = <DataProviderPathVarInfo>{
            "display_name": "Table",
            "short_desc": "The name of the GitHub table",
            "desc": "The table to work with: `issues`, `pulls` or `releases`",
            "example_value": GitHubIssuesDataProvider::TableName,
        };

        #! What every table supports, for the record-based actions
        /** The per-table search and create options are not here; they differ by table and are resolved
            through \c data_dependent_options.  What is here is the capability set and the options every
            table requires whatever it is: the repository the table is scoped to.
        */
        const RecordActionProviderInfo = <DataProviderInfo>{
            "type": "GitHubTableDataProviderBase",
            "has_record": True,
            "supports_read": True,
            "supports_create": True,
            "supports_native_search": True,
            "search_options": GitHubTableDataProviderBase::RepoOptions
                + GitHubTableDataProviderBase::LimitOption,
            "create_options": GitHubTableDataProviderBase::RepoOptions,
        };
    }

    private {
        #! The REST client used for API calls
        *GitHubRestClient::GitHubRestClientIo rest;
    }

    #! Creates the object
    constructor(*GitHubRestClient::GitHubRestClientIo rest) {
        self.rest = rest;
    }

    #! Returns the data provider name
    string getName() {
        return GitHubDataProvider::TablesChildName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "GitHub record-based tables";
    }

    #! Returns the table names
    private *list<string> getChildProviderNamesImpl() {
        return keys TableMap;
    }

    #! Returns the given table or @ref nothing if the table is unknown
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = TableMap{name};
        if (!cls) {
            return;
        }
        return cast<AbstractDataProvider>(cls.newObject(rest));
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
