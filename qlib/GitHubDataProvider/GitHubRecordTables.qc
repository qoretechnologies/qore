# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GitHubDataProvider module definition

/*  GitHubRecordTables.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
#! The GitHub record-based tables
/** GitHub is not a database, and these tables are not its response shapes.  Each one is a **flattened,
    writable projection** the application owns: an issue's labels arrive from GitHub as objects and are
    presented here as a list of names, its assignee as a login rather than a user object, and a pull
    request's base and head as branch names rather than as the repository-and-ref objects GitHub returns.

    That is why the record types here are declared rather than derived from the schema, unlike everything on
    the action surface.  A derived type would be GitHub's own shape, which is not what a record consumer
    writes back: the same record has to be readable and creatable, and GitHub accepts label **names** on
    create while returning label **objects** on read.

    The projection is the one the superseded TypeScript application defined, kept property for property,
    because existing mappers read these column names.

    @section githubrecord_actions Built From the Manifest, Not Beside It

    Every table is a @ref RestSchemaActions::RestSchemaRecordTableInfo "RestSchemaRecordTableInfo" naming
    actions the manifest already exports, so the paths, the request contracts, the escaping and the response
    shapes are the pinned schema's and are not written a second time here.  What this file adds is the
    projection, the identity, and which predicates GitHub can evaluate for itself.

    @section githubrecord_scope The Repository Is a Search and Create Option

    The \c issues, \c pulls and \c releases tables are each scoped to one repository, and \c owner and
    \c repo are therefore required search and create options rather than connection settings - exactly as in
    the application this module replaces.

    \c repositories is the exception, because its records **are** the repositories: its listing spans every
    owner the credential can see, so it has no scope at all and names each record by its owner and its name
    instead.

    @section githubrecord_number The Identity Is the Number, Not the ID

    An issue and a pull request each carry two identities: GitHub's global \c id, and the \c number that
    identifies it **within its repository**.  Every endpoint that reads, updates or comments on one takes the
    number, and nothing takes the global ID, so the number is what a record is addressed by here.  Both are
    in the record, because the superseded application published both.

    @section githubrecord_pushdown What GitHub Filters, and What This Module Filters

    GitHub's issue and pull request listings take a handful of query arguments, and those are sent when a
    search asks for what they express: an issue's \c state, a pull request's \c base branch, and \c since for
    issues updated after a point in time.  Everything else in a search is evaluated here, against each record
    as it arrives, and the listing is paged until the caller's limit is met **by matching records**.

    \c since is declared inexact - it is sent as a narrowing hint and the predicate is applied again to what
    comes back - because GitHub documents it as "last updated after" without saying whether the bound is
    inclusive, and a filter whose boundary is undocumented cannot be relied on to answer the predicate.

    Two GitHub filters are deliberately **not** used.  Its \c assignee argument matches an issue assigned to a
    user, while this projection's \c assignee column is the *first* of possibly several assignees, so the two
    do not describe the same set.  Its \c milestone argument takes a milestone number, while the column is the
    milestone's title.  Sending either would answer a search with records that do not match it.

    @section githubrecord_nodelete Only a Repository Can Be Deleted

    GitHub's REST API cannot delete an issue or a pull request - both are closed rather than removed - and it
    exports no delete for a release either, so those three tables support read, create and update and say so.
    A **repository** can be deleted, and permanently, so the record delete action is registered and refuses
    the tables that cannot honour it rather than pretending.

    @section githubrecord_lag A New Record Is Not Immediately Listed

    GitHub's collection endpoints lag its writes by a few seconds: an issue created through
    \c createRecord() - or through the \c "issues-create" action - is readable straight away by number, and is
    **not** yet in what the listing returns.  Verified against the live API: creating an issue and searching
    the table in the same breath returns the records that existed before it.

    So a workflow that creates a record and then searches for it will not find it, and should use what
    \c createRecord() returned rather than reading it back.  This is GitHub's behaviour, not this module's;
    there is nothing to poll or retry that would make it appear sooner, and the record is not lost.
*/
public class GitHubRecordTables {
    public {
        #! The action group the record-based actions belong to
        const RecordGroup = "Record-Based";

        #! How every GitHub collection pages
        /** GitHub reports the next page in the \c Link response header rather than in the body, and the body
            is the bare array, so a page shorter than the size asked for is the last one.
        */
        const Paging = <RestSchemaRecordPagingInfo>{
            "mode": RSRP_PAGE,
            "page_option": "page",
            "size_option": "per_page",
            "max_size": GitHubManifest::MaxPerPage,
        };

        #! The fields every table carries, none of which can be written
        const CommonReadOnlyFields = (
            {"name": "id", "desc": "the unique ID; not writable", "type": "*int"},
            {"name": "created_at", "desc": "when the record was created; not writable", "type": "*string"},
            {"name": "updated_at", "desc": "when the record last changed; not writable", "type": "*string"},
        );

        #! The columns no table sends on a write
        const ReadOnlyFields = ("id", "created_at", "updated_at", "number");

        #! The issues table's columns
        const IssueFields = CommonReadOnlyFields + (
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

        #! The pull requests table's columns
        const PullFields = CommonReadOnlyFields + (
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

        #! The releases table's columns
        const ReleaseFields = CommonReadOnlyFields + (
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

        #! The state search option, which a table defaults to "all" where GitHub's listing defaults to "open"
        /** The application this module replaces returned open **and** closed records from a table by
            default, and a table that quietly adopted GitHub's own default would return a different set.
        */
        const StateDefaultsToAll = {
            "state": {"default_value": "all"},
        };

        #! The issues table
        const IssuesTable = <RestSchemaRecordTableInfo>{
            "name": "issues",
            "display_name": "Issues",
            "short_desc": "GitHub issues as records",
            "desc": "GitHub issues as records; note that GitHub returns pull requests here as well",
            "list_action": "issues-list-for-repo",
            "get_action": "issues-get",
            "create_action": "issues-create",
            "update_action": "issues-update",
            "id_field": "number",
            "id_option": "issue_number",
            "scope_options": ("owner", "repo"),
            "search_options": ("state", "sort", "direction"),
            "search_option_overrides": StateDefaultsToAll,
            "paging": Paging,
            "filters": (
                <RestSchemaRecordFilterInfo>{
                    "field": "state", "operator": DP_SEARCH_OP_EQ, "option": "state", "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "updated_at", "operator": DP_SEARCH_OP_GT, "option": "since",
                    "to_wire": \GitHubRecordTables::toTimestamp(),
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "updated_at", "operator": DP_SEARCH_OP_GE, "option": "since",
                    "to_wire": \GitHubRecordTables::toTimestamp(),
                },
            ),
            "record_type": RestSchemaTableDataProvider::makeRecordType(IssueFields),
            "read_only_fields": ReadOnlyFields,
            "to_record": hash<auto> sub (hash<auto> rec) {
                return rec{(map $1.name, IssueFields)} + {
                    "labels": (map $1.name ?? $1.toString(), rec.labels) ?? (),
                    "assignee": rec.assignee.login,
                    "assignees": (map $1.login, rec.assignees) ?? (),
                    "milestone": rec.milestone.title,
                    "type": rec."type".name ?? rec."type",
                };
            },
            "to_wire": \GitHubRecordTables::toWire(),
        };

        #! The pull requests table
        const PullsTable = <RestSchemaRecordTableInfo>{
            "name": "pulls",
            "display_name": "Pull Requests",
            "short_desc": "GitHub pull requests as records",
            "desc": "GitHub pull requests as records",
            "list_action": "pulls-list",
            "get_action": "pulls-get",
            "create_action": "pulls-create",
            "update_action": "pulls-update",
            "id_field": "number",
            "id_option": "pull_number",
            "scope_options": ("owner", "repo"),
            "search_options": ("state", "sort", "direction"),
            "search_option_overrides": StateDefaultsToAll,
            "paging": Paging,
            "filters": (
                <RestSchemaRecordFilterInfo>{
                    "field": "state", "operator": DP_SEARCH_OP_EQ, "option": "state", "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "base", "operator": DP_SEARCH_OP_EQ, "option": "base", "exact": True,
                },
            ),
            "record_type": RestSchemaTableDataProvider::makeRecordType(PullFields),
            "read_only_fields": ReadOnlyFields,
            "to_record": hash<auto> sub (hash<auto> rec) {
                return rec{(map $1.name, PullFields)} + {
                    "base": rec.base.ref,
                    "head": rec.head.ref,
                    "head_repo": rec.head.repo.full_name,
                };
            },
            "to_wire": \GitHubRecordTables::toWire(),
        };

        #! The releases table
        /** GitHub's releases collection accepts no state, sort or direction: it always returns releases
            newest first, so declaring those options would offer filters the API ignores.
        */
        const ReleasesTable = <RestSchemaRecordTableInfo>{
            "name": "releases",
            "display_name": "Releases",
            "short_desc": "GitHub releases as records",
            "desc": "GitHub releases as records",
            "list_action": "repos-list-releases",
            "create_action": "repos-create-release",
            "id_field": "id",
            "scope_options": ("owner", "repo"),
            "paging": Paging,
            "record_type": RestSchemaTableDataProvider::makeRecordType(ReleaseFields),
            "read_only_fields": ReadOnlyFields,
            "to_record": hash<auto> sub (hash<auto> rec) {
                return rec{(map $1.name, ReleaseFields)};
            },
            "to_wire": \GitHubRecordTables::toWire(),
        };

        #! The repositories table
        /** The one table in this application whose records are **not** scoped to a repository, because they
            are the repositories: the listing spans every owner the credential can see.

            @section githubrecord_repos_id Its Identity Has Two Parts

            GitHub addresses a repository by its \c {owner} **and** its \c {repo}, both path variables, so a
            record is addressed by two values rather than one - see
            @ref RestSchemaActions::RestSchemaRecordTableInfo::id_parts "id_parts".  The record carries the
            second as \c name and the first as \c "owner.login", because a repository's \c owner is a user
            object rather than a string.

            The owner cannot be a scope option here even though it is one on every other GitHub table: a
            scope is the same for every record in a search, and this listing returns repositories belonging
            to different owners in one response.

            @section githubrecord_repos_nocreate There Is No Create

            GitHub creates a repository under an organisation - \c "repos-create-in-org" - or under the
            authenticated user, and the two are different operations with different scopes; neither is the
            collection this table lists.  Creating one stays an action, and the table reads, updates and
            deletes.

            @section githubrecord_repos_nofilter Nothing Is Pushed Down

            GitHub's repository listing takes \c visibility, \c affiliation and \c type, and **\c type may
            not be combined with either of the others** - a request carrying both is rejected.  Wiring a
            predicate to \c visibility would therefore turn a search into a 422 for any caller who had also
            set \c type, so all three stay ordinary search options that a caller sets deliberately, and a
            predicate is evaluated here.
        */
        const RepositoriesTable = <RestSchemaRecordTableInfo>{
            "name": "repositories",
            "display_name": "Repositories",
            "short_desc": "the repositories the credential can see",
            "desc": "The repositories the credential can see, as records; a repository is addressed by its "
                "owner and its name",
            "list_action": "repos-list-for-authenticated-user",
            "get_action": "repos-get",
            "update_action": "repos-update",
            "delete_action": "repos-delete",
            "id_parts": {"owner": "owner.login", "repo": "name"},
            "paging": Paging,
            "search_options": ("visibility", "affiliation", "type", "sort", "direction", "since", "before"),
        };

        #! Every table, in presentation order
        const Tables = (IssuesTable, PullsTable, ReleasesTable, RepositoriesTable);

        #! The IDs the record-based actions are registered under
        /** The first three are the IDs the superseded TypeScript application registered, so a workflow
            that already calls them keeps working; \c update and \c delete are new and take the
            conventional names.
        */
        const RecordActionIds = <RestSchemaRecordActionIds>{
            "search_records": "search",
            "search_single_record": "search-single",
            "create_records": "create",
            "update_records": "update",
            "delete_records": "delete",
        };
    }

    #! Returns a record's writable values
    /** A record arrives with every column present, and GitHub rejects a null where it wants a value or an
        omitted property, so the empty ones are dropped rather than sent.  The projection's column names are
        already the names GitHub's writes take, so nothing else has to be translated.
    */
    static hash<auto> toWire(hash<auto> rec) {
        # "auto!" because a record's values are of every type, and a hash narrowed to the first one would
        # reject the rest
        hash<auto!> rv = {};
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
            rv{i.key} = i.value;
        }
        return rv;
    }

    #! Returns a timestamp in the ISO 8601 form GitHub's "since" argument takes
    /** A predicate may compare a timestamp column against a %Qore @ref date or against the string GitHub
        returned; both have to reach the wire in the one form GitHub accepts.
    */
    static auto toTimestamp(auto value) {
        if (value.typeCode() == NT_DATE) {
            return value.format("YYYY-MM-DDTHH:mm:SSZ");
        }
        return value;
    }
}
}
