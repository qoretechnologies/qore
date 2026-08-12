# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ConfluenceDataProvider module definition

/*  ConfluenceRecordTables.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the ConfluenceDataProvider module
public namespace ConfluenceDataProvider {
#! The Confluence record-based tables
/** Three of Confluence's v2 collections are record-shaped and are exposed as tables: \c pages,
    \c blogposts and \c tasks.  Each is built from actions the manifest already exports, so the request
    contracts and the record shapes are the pinned schema's.

    @section confluencerecord_type The Records Are the Schema's Own Shape

    Unlike GitHub's tables, these publish Confluence's own record shape rather than a projection of it: this
    application has no predecessor whose column names have to be preserved, and the v2 API already returns a
    flat object.  The record type is therefore the element type of each listing's response, which cannot
    drift from the contract the actions use.

    @section confluencerecord_paging Confluence Pages With an Opaque Cursor

    A v2 collection reports the next page as a relative URL carrying a \c cursor argument, both in the
    \c Link response header and in the body's <tt>_links.next</tt>.  An action publishes the response body
    and not the HTTP envelope, so the body is what the tables read, and the cursor is taken out of that URL.
    \c limit is capped at 250 by the API and is asked for in full, so a search reads as few pages as
    Confluence allows.

    @section confluencerecord_pushdown What Confluence Filters

    The v2 listings take a small set of exact filters and this module sends them: a page or blog post by
    space, title or status, a task by status, space, page, assignee, creator or completer.  Everything else
    in a search is evaluated here as the records arrive.

    A task's date bounds are exact and are sent as well.  Confluence documents \c created-at-from and its
    siblings as **inclusive** and as epoch **milliseconds**, so they carry \c >= and \c <= exactly, and a
    value that cannot be read as a point in time is not sent at all rather than sent in the wrong unit.

    @section confluencerecord_readonly Three Collections Are Read Only

    \c spaces, \c labels and \c attachments list and are never written: a space is created through a
    long-running job rather than through this collection, a label is applied to content rather than created
    on its own, and an attachment is uploaded to a page.  This application exports no create, update or
    delete for any of them, and a table naming an action the manifest does not export could only ever fail,
    so they declare none.

    \c spaces has the single-record read the other tables have, so a search naming one space reads that one
    space, and the record type is the richer of the two descriptions.  \c labels and \c attachments have
    none: Confluence publishes no path that reads one of either.

    An attachment's file name is its \c title, which is what Confluence's \c filename argument filters on.

    @section confluencerecord_version An Update Needs the Version Number

    Confluence rejects an update that does not carry the record's current \c version, which is its optimistic
    locking: two writers who both read version 3 cannot both write version 4.  The version is part of the
    record, so a read-modify-write through a table carries it naturally; an update assembled by hand has to
    include it, and the error Confluence returns says so.
*/
public class ConfluenceRecordTables {
    public {
        #! The action group the record-based actions belong to
        const RecordGroup = "Record-Based";

        #! How every v2 collection pages
        const Paging = <RestSchemaRecordPagingInfo>{
            "mode": RSRP_CURSOR,
            "page_option": "cursor",
            "size_option": "limit",
            "max_size": 250,
            "next_path": ("_links", "next"),
            "next_cursor": \ConfluenceRecordTables::cursorFromLink(),
        };

        #! The filters a page and a blog post listing share
        const ContentFilters = (
            <RestSchemaRecordFilterInfo>{
                "field": "spaceId", "operator": DP_SEARCH_OP_EQ, "option": "space-id", "exact": True,
            },
            <RestSchemaRecordFilterInfo>{
                "field": "title", "operator": DP_SEARCH_OP_EQ, "option": "title", "exact": True,
            },
            <RestSchemaRecordFilterInfo>{
                "field": "status", "operator": DP_SEARCH_OP_EQ, "option": "status", "exact": True,
            },
            <RestSchemaRecordFilterInfo>{
                "field": "id", "operator": DP_SEARCH_OP_IN, "option": "id", "multi": True, "exact": True,
            },
        );

        #! The pages table
        const PagesTable = <RestSchemaRecordTableInfo>{
            "name": "pages",
            "display_name": "Pages",
            "short_desc": "Confluence pages as records",
            "desc": "Confluence pages as records",
            "list_action": "getPages",
            "get_action": "getPageById",
            "create_action": "createPage",
            "update_action": "updatePage",
            "delete_action": "deletePage",
            "id_field": "id",
            "records_path": ("results",),
            "paging": Paging,
            "filters": ContentFilters,
            "search_options": ("sort", "body-format"),
        };

        #! The blog posts table
        const BlogPostsTable = <RestSchemaRecordTableInfo>{
            "name": "blogposts",
            "display_name": "Blog Posts",
            "short_desc": "Confluence blog posts as records",
            "desc": "Confluence blog posts as records",
            "list_action": "getBlogPosts",
            "get_action": "getBlogPostById",
            "create_action": "createBlogPost",
            "update_action": "updateBlogPost",
            "delete_action": "deleteBlogPost",
            "id_field": "id",
            "records_path": ("results",),
            "paging": Paging,
            "filters": ContentFilters,
            "search_options": ("sort", "body-format"),
        };

        #! The tasks table
        /** Confluence creates a task by writing a checkbox into a page's body and deletes it by removing it,
            so its API exports neither a create nor a delete for one: a task is read and its status is
            changed, and that is all.
        */
        const TasksTable = <RestSchemaRecordTableInfo>{
            "name": "tasks",
            "display_name": "Tasks",
            "short_desc": "Confluence tasks as records",
            "desc": "Confluence tasks as records; a task is created by writing a checkbox into a page and "
                "cannot be created or deleted through the API",
            "list_action": "getTasks",
            "get_action": "getTaskById",
            "update_action": "updateTask",
            "id_field": "id",
            "records_path": ("results",),
            "paging": Paging,
            "filters": (
                <RestSchemaRecordFilterInfo>{
                    "field": "status", "operator": DP_SEARCH_OP_EQ, "option": "status", "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "spaceId", "operator": DP_SEARCH_OP_EQ, "option": "space-id", "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "pageId", "operator": DP_SEARCH_OP_EQ, "option": "page-id", "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "blogPostId", "operator": DP_SEARCH_OP_EQ, "option": "blogpost-id",
                    "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "assignedTo", "operator": DP_SEARCH_OP_EQ, "option": "assigned-to",
                    "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "createdBy", "operator": DP_SEARCH_OP_EQ, "option": "created-by", "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "completedBy", "operator": DP_SEARCH_OP_EQ, "option": "completed-by",
                    "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "id", "operator": DP_SEARCH_OP_IN, "option": "task-id", "multi": True,
                    "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "createdAt", "operator": DP_SEARCH_OP_GE, "option": "created-at-from",
                    "exact": True, "to_wire": \ConfluenceRecordTables::toEpochMillis(),
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "createdAt", "operator": DP_SEARCH_OP_LE, "option": "created-at-to",
                    "exact": True, "to_wire": \ConfluenceRecordTables::toEpochMillis(),
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "dueAt", "operator": DP_SEARCH_OP_GE, "option": "due-at-from",
                    "exact": True, "to_wire": \ConfluenceRecordTables::toEpochMillis(),
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "dueAt", "operator": DP_SEARCH_OP_LE, "option": "due-at-to",
                    "exact": True, "to_wire": \ConfluenceRecordTables::toEpochMillis(),
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "completedAt", "operator": DP_SEARCH_OP_GE, "option": "completed-at-from",
                    "exact": True, "to_wire": \ConfluenceRecordTables::toEpochMillis(),
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "completedAt", "operator": DP_SEARCH_OP_LE, "option": "completed-at-to",
                    "exact": True, "to_wire": \ConfluenceRecordTables::toEpochMillis(),
                },
            ),
            "search_options": ("body-format", "include-blank-tasks"),
        };

        #! The spaces table
        /** @see @ref confluencerecord_readonly
        */
        const SpacesTable = <RestSchemaRecordTableInfo>{
            "name": "spaces",
            "display_name": "Spaces",
            "short_desc": "Confluence spaces as records",
            "desc": "Confluence spaces as records; a search narrows them by key, by type and by status.  "
                "Spaces are read only here",
            "list_action": "getSpaces",
            "get_action": "getSpaceById",
            "id_field": "id",
            "records_path": ("results",),
            "paging": Paging,
            "filters": (
                <RestSchemaRecordFilterInfo>{
                    "field": "id", "operator": DP_SEARCH_OP_EQ, "option": "ids", "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "id", "operator": DP_SEARCH_OP_IN, "option": "ids", "multi": True,
                    "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "key", "operator": DP_SEARCH_OP_EQ, "option": "keys", "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "key", "operator": DP_SEARCH_OP_IN, "option": "keys", "multi": True,
                    "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "type", "operator": DP_SEARCH_OP_EQ, "option": "type", "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "status", "operator": DP_SEARCH_OP_EQ, "option": "status", "exact": True,
                },
            ),
            "search_options": ("sort", "labels", "favorited-by", "not-favorited-by", "description-format",
                "include-icon"),
        };

        #! The labels table
        /** @see @ref confluencerecord_readonly
        */
        const LabelsTable = <RestSchemaRecordTableInfo>{
            "name": "labels",
            "display_name": "Labels",
            "short_desc": "the labels defined in the site",
            "desc": "The labels defined in the site, as records; a search narrows them by prefix.  Labels "
                "are read only here - a label is applied to content rather than created on its own",
            "list_action": "getLabels",
            "id_field": "id",
            "records_path": ("results",),
            "paging": Paging,
            "filters": (
                <RestSchemaRecordFilterInfo>{
                    "field": "id", "operator": DP_SEARCH_OP_EQ, "option": "label-id", "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "id", "operator": DP_SEARCH_OP_IN, "option": "label-id", "multi": True,
                    "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "prefix", "operator": DP_SEARCH_OP_EQ, "option": "prefix", "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "prefix", "operator": DP_SEARCH_OP_IN, "option": "prefix", "multi": True,
                    "exact": True,
                },
            ),
            "search_options": ("sort",),
        };

        #! The attachments table
        /** An attachment's file name is its \c title, which is what Confluence's \c filename argument
            filters on, so a predicate on \c title is the one that is sent.

            @see @ref confluencerecord_readonly
        */
        const AttachmentsTable = <RestSchemaRecordTableInfo>{
            "name": "attachments",
            "display_name": "Attachments",
            "short_desc": "the attachments in the site",
            "desc": "The attachments in the site, as records; a search narrows them by status, by media "
                "type and by file name.  Attachments are read only here - an attachment is uploaded to a "
                "page or a blog post",
            "list_action": "getAttachments",
            "id_field": "id",
            "records_path": ("results",),
            "paging": Paging,
            "filters": (
                <RestSchemaRecordFilterInfo>{
                    "field": "status", "operator": DP_SEARCH_OP_EQ, "option": "status", "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "status", "operator": DP_SEARCH_OP_IN, "option": "status", "multi": True,
                    "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "mediaType", "operator": DP_SEARCH_OP_EQ, "option": "mediaType", "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "title", "operator": DP_SEARCH_OP_EQ, "option": "filename", "exact": True,
                },
            ),
            "search_options": ("sort",),
        };

        #! Every table, in presentation order
        const Tables = (PagesTable, BlogPostsTable, TasksTable, SpacesTable, LabelsTable, AttachmentsTable);

        #! The IDs the record-based actions are registered under
        /** Confluence is a new application rather than a replacement, so these are the conventional IDs.
        */
        const RecordActionIds = <RestSchemaRecordActionIds>{};
    }

    #! Returns the cursor carried by a next-page link
    /** Confluence reports the next page as a relative URL rather than as the cursor itself.
    */
    static *string cursorFromLink(auto value) {
        if (!value.val()) {
            return;
        }
        *list<string> m = (value.toString() =~ x/[?&]cursor=([^&]+)/);
        if (!m) {
            return;
        }
        return decode_url(m[0]);
    }

    #! Returns a point in time as the epoch milliseconds Confluence's task date bounds take
    /** A value that cannot be read as a point in time returns @ref nothing, which leaves the predicate for
        the client side rather than sending a number that means something else.
    */
    static auto toEpochMillis(auto value) {
        date d;
        switch (value.typeCode()) {
            case NT_DATE:
                d = value;
                break;
            case NT_INT:
                # already a count; the caller is speaking Confluence's own units
                return value;
            case NT_STRING: {
                d = date(value);
                if (!d) {
                    return;
                }
                break;
            }
            default:
                return;
        }
        return (get_epoch_seconds(d) * 1000) + d.milliseconds();
    }
}
}
