# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BitbucketDataProvider module definition

/*  BitbucketRecordTables.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the BitbucketDataProvider module
public namespace BitbucketDataProvider {
#! The Bitbucket record-based tables
/** Bitbucket is one of the two applications in this family whose API publishes a real filter expression
    language, so a whole search expression can be sent over the wire rather than a handful of query
    arguments.  That is what
    @ref DataProvider::DataProviderInfo::supports_native_search "supports_native_search" means, and the
    \c repositories and \c commit-comments tables declare it because Bitbucket really does the searching.

    @section bitbucketrecord_bbql BBQL

    Bitbucket Query Language is passed in the \c q argument: an infix comparison per predicate, joined with
    \c AND, \c OR and \c NOT, with string values double-quoted and timestamps written as bare ISO 8601.
    @ref BitbucketRecordTables::Bbql "Bbql" declares it, and
    @ref RestSchemaActions::RestSchemaRecordFilter "RestSchemaRecordFilter" renders the expression tree into
    it.

    Bitbucket rejects a query naming a field it does not index, so a predicate on any other field is
    evaluated here instead of being sent and turned into an error.  The fields each table will accept are
    declared with it.

    \c ~ is rendered but is **not** taken as exact.  Bitbucket documents it as "contains" without saying
    whether it is case-sensitive, so it narrows what has to be read and the predicate is applied again to
    what comes back: the search stays correct either way, and still costs one request instead of a walk.

    @section bitbucketrecord_pullrequests Pull Requests Cannot Be Queried

    Bitbucket's published description declares \c q on its repository and commit-comment listings and
    **not** on its pull request listing, though the API accepts it there.  An overlay cannot add a
    parameter - that is structural rather than presentational - so the pull requests table filters on
    \c state alone and evaluates the rest of a search here.  Repairing that belongs in the import
    specification's \c normalize block, where the same gap in Bitbucket's pagination arguments was repaired.

    @section bitbucketrecord_paging Paging

    Every Bitbucket collection answers with <tt>{values, page, pagelen, size, next}</tt>, and \c next is
    absent on the last page.  The tables page by number, because \c page is what the pruned document
    declares, and stop on a short page.
*/
public class BitbucketRecordTables {
    public {
        #! The action group the record-based actions belong to
        const RecordGroup = "Record-Based";

        #! Bitbucket Query Language, as sent in the \c q argument
        /** \c ~ is declared in @ref RestSchemaRecordQueryInfo::inexact_ops "inexact_ops" by every table that
            uses this syntax, because Bitbucket does not document whether it matches case-sensitively.
        */
        const Bbql = <RestSchemaQuerySyntaxInfo>{
            "name": "BBQL",
            "binary_ops": {
                DP_SEARCH_OP_EQ: "=",
                DP_SEARCH_OP_NE: "!=",
                DP_SEARCH_OP_LT: "<",
                DP_SEARCH_OP_LE: "<=",
                DP_SEARCH_OP_GT: ">",
                DP_SEARCH_OP_GE: ">=",
                DP_OP_CONTAINS: "~",
            },
            "and_op": "AND",
            "or_op": "OR",
            "not_op": "NOT",
            "render_value": \BitbucketRecordTables::renderValue(),
        };

        #! The operators BBQL spells without meaning exactly what they mean here
        const BbqlInexact = {DP_OP_CONTAINS: True};

        #! How every Bitbucket collection pages
        const Paging = <RestSchemaRecordPagingInfo>{
            "mode": RSRP_PAGE,
            "page_option": "page",
            "size_option": "pagelen",
            "max_size": 100,
        };

        #! The repositories table
        const RepositoriesTable = <RestSchemaRecordTableInfo>{
            "name": "repositories",
            "display_name": "Repositories",
            "short_desc": "the repositories in one workspace",
            "desc": "The repositories in one workspace, as records; Bitbucket evaluates a search over them "
                "itself",
            "list_action": "repositories_workspace_get",
            "get_action": "repositories_workspace_repo_slug_get",
            "create_action": "repositories_workspace_repo_slug_post",
            "update_action": "repositories_workspace_repo_slug_put",
            "delete_action": "repositories_workspace_repo_slug_delete",
            # a repository record carries its UUID and its "workspace/slug" full name, and not the slug on
            # its own; Bitbucket accepts either the slug or the brace-wrapped UUID wherever a path takes a
            # "repo_slug", so the UUID is what addresses a record here
            "id_field": "uuid",
            "id_option": "repo_slug",
            "scope_options": ("workspace",),
            "records_path": ("values",),
            "paging": Paging,
            "search_options": ("role", "sort"),
            "query": <RestSchemaRecordQueryInfo>{
                "option": "q",
                "syntax": Bbql,
                "inexact_ops": BbqlInexact,
                # Bitbucket rejects a query naming a field it does not index
                "fields": {
                    "name": True, "slug": True, "full_name": True, "description": True, "language": True,
                    "scm": True, "is_private": True, "fork_policy": True, "has_issues": True,
                    "has_wiki": True, "created_on": True, "updated_on": True, "size": True, "uuid": True,
                },
            },
        };

        #! The pull requests table
        /** Bitbucket's description does not declare \c q here, so a search filters on \c state and is
            answered for the rest by reading the listing.
        */
        const PullRequestsTable = <RestSchemaRecordTableInfo>{
            "name": "pullrequests",
            "display_name": "Pull Requests",
            "short_desc": "the pull requests in one repository",
            "desc": "The pull requests in one repository, as records",
            "list_action": "repositories_workspace_repo_slug_pullrequests_get",
            "get_action": "repositories_workspace_repo_slug_pullrequests_pull_request_id_get",
            "create_action": "repositories_workspace_repo_slug_pullrequests_post",
            "update_action": "repositories_workspace_repo_slug_pullrequests_pull_request_id_put",
            "id_field": "id",
            "id_option": "pull_request_id",
            "scope_options": ("workspace", "repo_slug"),
            "records_path": ("values",),
            "paging": Paging,
            "search_options": ("state",),
            "filters": (
                <RestSchemaRecordFilterInfo>{
                    "field": "state", "operator": DP_SEARCH_OP_EQ, "option": "state", "exact": True,
                },
            ),
        };

        #! The pull request comments table
        const PullRequestCommentsTable = <RestSchemaRecordTableInfo>{
            "name": "pullrequest-comments",
            "display_name": "Pull Request Comments",
            "short_desc": "the comments on one pull request",
            "desc": "The comments on one pull request, as records",
            "list_action": "repositories_workspace_repo_slug_pullrequests_pull_request_id_comments_get",
            "get_action":
                "repositories_workspace_repo_slug_pullrequests_pull_request_id_comments_comment_id_get",
            "create_action": "repositories_workspace_repo_slug_pullrequests_pull_request_id_comments_post",
            "update_action":
                "repositories_workspace_repo_slug_pullrequests_pull_request_id_comments_comment_id_put",
            "delete_action":
                "repositories_workspace_repo_slug_pullrequests_pull_request_id_comments_comment_id_delete",
            "id_field": "id",
            "id_option": "comment_id",
            "scope_options": ("workspace", "repo_slug", "pull_request_id"),
            "records_path": ("values",),
            "paging": Paging,
        };

        #! The commit comments table
        /** Bitbucket declares \c q on this listing, so it evaluates a search over commit comments itself.
        */
        const CommitCommentsTable = <RestSchemaRecordTableInfo>{
            "name": "commit-comments",
            "display_name": "Commit Comments",
            "short_desc": "the comments on one commit",
            "desc": "The comments on one commit, as records; Bitbucket evaluates a search over them itself",
            "list_action": "repositories_workspace_repo_slug_commit_commit_comments_get",
            "create_action": "repositories_workspace_repo_slug_commit_commit_comments_post",
            "id_field": "id",
            "scope_options": ("workspace", "repo_slug", "commit"),
            "records_path": ("values",),
            "paging": Paging,
            "search_options": ("sort",),
            "query": <RestSchemaRecordQueryInfo>{
                "option": "q",
                "syntax": Bbql,
                "inexact_ops": BbqlInexact,
                "fields": {"id": True, "created_on": True, "updated_on": True, "deleted": True},
            },
        };

        #! Every table, in presentation order
        const Tables = (RepositoriesTable, PullRequestsTable, PullRequestCommentsTable, CommitCommentsTable);

        #! The IDs the record-based actions are registered under
        /** Bitbucket is a new application rather than a replacement, so these are the conventional IDs.
        */
        const RecordActionIds = <RestSchemaRecordActionIds>{};
    }

    #! Renders one value as BBQL spells it
    /** A string is double-quoted with its quotes and backslashes escaped, a timestamp is written bare in
        ISO 8601, and a boolean is a bare keyword.  An unset value is \c null, which is how BBQL asks for a
        property that is not set.
    */
    static string renderValue(auto value, string field) {
        switch (value.typeCode()) {
            case NT_NOTHING:
            case NT_NULL:
                return "null";
            case NT_BOOLEAN:
                return value ? "true" : "false";
            case NT_DATE:
                return value.format("YYYY-MM-DDTHH:mm:SSZ");
            case NT_INT:
            case NT_FLOAT:
            case NT_NUMBER:
                return value.toString();
        }
        return "\"" + replace(replace(value.toString(), "\\", "\\\\"), "\"", "\\\"") + "\"";
    }
}
}
