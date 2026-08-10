# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ConfluenceDataProvider module definition

/*  ConfluenceManifest.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
#! The operations the Confluence application exports, and how they are presented
/** @section confluencemanifest_ids Action IDs

    The action IDs are those of the TypeScript application this module replaces, which took them from the
    \c operationId of a vendored Swagger 2.0 document.  Atlassian generates that document and the OpenAPI 3
    one from the same source, so they agree and the IDs carry over unchanged — \c "getBlogPosts",
    \c "updatePage".  Each is recorded as \c operation_id as well, so an upstream rename surfaces as drift
    rather than silently renaming an action.

    @section confluencemanifest_added What This Application Adds

    Two read-only listings are exported that the TypeScript application did not:

    |!Action|!Why
    |\c getLabels|its label dropdowns called an endpoint the application did not export
    |\c getAttachments|its "new attachment" trigger polled an endpoint the application did not export, so an attachment could be reported but never listed or read

    @section confluencemanifest_id_twice The ID Appears Twice

    Confluence's update operations declare the identity of the object being changed in **both** the URL
    and a required \c id body property, and the two must always be equal.  Asking for it twice would let
    a user get it wrong; dropping the body one would produce a request Confluence rejects.  The body
    property therefore declares
    @ref RestSchemaActions::RestSchemaFieldOverlayInfo::value_from "value_from", so it is not an option and
    follows the path variable when the request is built.

    @section confluencemanifest_version Updating Needs the Current Version

    Confluence uses optimistic locking: an update must carry \c version.number **one greater** than the
    version it is changing, or it is rejected with a conflict.  So an update is always read-modify-write —
    fetch the page, add one to \c version.number, send it back.  That is Confluence's contract, not a
    presentation choice, and the option documents it rather than hiding it.

    @section confluencemanifest_pagination Pagination

    A Confluence v2 collection returns \c {results, _links}, and \c _links.next is a **relative** URL
    carrying an opaque cursor.  There is no total and no page number: page by taking the \c cursor argument
    out of that URL and passing it to the same action.
*/
public class ConfluenceManifest {
    public {
        #! Action group: pages
        const GroupPages = "Pages";

        #! Action group: blog posts
        const GroupBlogPosts = "Blog Posts";

        #! Action group: spaces
        const GroupSpaces = "Spaces";

        #! Action group: tasks
        const GroupTasks = "Tasks";

        #! Action group: labels and attachments
        const GroupContent = "Content";

        #! Reference data name: spaces
        const RefSpace = "confluence-space";

        #! Reference data name: pages
        const RefPage = "confluence-page";

        #! Reference data name: blog posts
        const RefBlogPost = "confluence-blogpost";

        #! Reference data name: labels
        const RefLabel = "confluence-label";

        #! Reference data name: tasks
        const RefTask = "confluence-task";

        #! Confluence's maximum page size for a collection
        const MaxLimit = ConfluenceRestClient::ConfluenceRestClientBase::MaxLimit;

        #! The dropdowns the application offers
        /** Each is produced by executing a listing action from this same manifest, so a dropdown can never
            drift away from an endpoint the application actually exports.  The records are read from
            Confluence's \c results envelope.
        */
        const ReferenceData = {
            RefSpace: <RestSchemaReferenceDataInfo>{
                "name": RefSpace,
                "action": "getSpaces",
                "options": {"limit": MaxLimit},
                "records_path": "results",
                "value_field": "id",
                "display_field": "name",
                "desc": "spaces on this Confluence site",
            },
            RefPage: <RestSchemaReferenceDataInfo>{
                "name": RefPage,
                "action": "getPages",
                "options": {"limit": MaxLimit},
                "records_path": "results",
                "value_field": "id",
                "display_field": "title",
                "desc": "pages on this Confluence site",
            },
            RefBlogPost: <RestSchemaReferenceDataInfo>{
                "name": RefBlogPost,
                "action": "getBlogPosts",
                "options": {"limit": MaxLimit},
                "records_path": "results",
                "value_field": "id",
                "display_field": "title",
                "desc": "blog posts on this Confluence site",
            },
            RefLabel: <RestSchemaReferenceDataInfo>{
                "name": RefLabel,
                "action": "getLabels",
                "options": {"limit": MaxLimit},
                "records_path": "results",
                "value_field": "id",
                "display_field": "name",
                "desc": "labels on this Confluence site",
            },
            RefTask: <RestSchemaReferenceDataInfo>{
                "name": RefTask,
                "action": "getTasks",
                "options": {"limit": MaxLimit},
                "records_path": "results",
                # a task carries no title of its own; its body is rich text, so the ID identifies it
                "value_field": "id",
                "desc": "tasks on this Confluence site",
            },
        };

        #! Overlay for the space an action works in
        const SpaceOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Space",
            "short_desc": "The space to work in",
            "ref_data": RefSpace,
            "supports_custom_values": True,
            "preselected": True,
        };

        #! Overlay for the space that scopes a listing
        const SpaceFilterOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Spaces",
            "short_desc": "Return only content in these spaces",
            "element_ref_data": RefSpace,
            "supports_custom_values": True,
            "preselected": True,
        };

        #! Overlay for the page an action works on
        const PageOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Page",
            "ref_data": RefPage,
            "supports_custom_values": True,
            "preselected": True,
        };

        #! Overlay for the blog post an action works on
        const BlogPostOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Blog Post",
            "ref_data": RefBlogPost,
            "supports_custom_values": True,
            "preselected": True,
        };

        #! Overlay for the label an action works on
        const LabelOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Label",
            "ref_data": RefLabel,
            "supports_custom_values": True,
            "preselected": True,
        };

        #! Overlay for the \c id body property of an update
        /** Confluence requires the identity in the body as well as the URL, and the two must be equal, so
            it follows the path variable rather than being asked for a second time.
        */
        const IdFollowsPathOverlay = <RestSchemaFieldOverlayInfo>{
            "value_from": "id",
        };

        #! Overlay for the \c version property of an update
        const VersionOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Version",
            "short_desc": "The new version; \"number\" must be one more than the current version",
            "desc": "Confluence uses optimistic locking, so `number` must be **one greater** than the "
                "version being changed or the update is rejected with a conflict.\n\n"
                "Read the object first, add one to its `version.number`, and send that here. `message` is "
                "the optional change note shown in the page history.",
            "preselected": True,
        };

        #! Overlay for the body of a page or blog post
        const BodyOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Body",
            "short_desc": "The content; set \"representation\" to say what \"value\" is written in",
            "desc": "The content itself.\n\n"
                "`representation` says how `value` is written — `storage` for Confluence's XHTML storage "
                "format, or `atlas_doc_format` for the JSON document format. Getting this wrong is "
                "usually why content appears as escaped markup rather than as a rendered page.",
            "preselected": True,
        };

        #! Overlay shared by the \c limit option of every listing action
        const LimitOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Limit",
            "short_desc": "How many records to return in one page",
            "desc": "How many records to return in one page.\n\n"
                "There is no total in the response: `_links.next` is a relative URL carrying the cursor "
                "for the following page, and its absence is what says the collection is exhausted.",
            "preselected": True,
            "example_value": 50,
        };

        #! Overlay shared by the \c cursor option of every listing action
        const CursorOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Cursor",
            "short_desc": "The opaque cursor for the next page",
            "desc": "The pagination cursor: take the `cursor` query argument out of the `_links.next` URL "
                "in the previous response and pass it here.\n\n"
                "It is opaque and must not be constructed by hand.",
        };

        #! Overlay applied to the paging options of every listing action
        const PagingOverlay = {
            "limit": LimitOverlay,
            "cursor": CursorOverlay,
        };

        #! The operations the application exports
        const Manifest = (
            # --- Pages ---
            <RestSchemaActionInfo>{
                "action": "getPages",
                "method": "GET",
                "path": "/pages",
                "operation_id": "getPages",
                "display_name": "List Pages",
                "short_desc": "List the pages on the site",
                "desc": "Lists pages across the whole site.\n\n"
                    "`space-id` restricts the listing to particular spaces, `status` selects drafts or "
                    "archived pages as well as current ones, and `title` matches an exact title.\n\n"
                    "The body is not returned unless `body-format` asks for it, which is worth leaving "
                    "unset when only titles and IDs are needed.",
                "groups": (GroupPages,),
                "field_order": ("space-id", "title", "status", "sort", "body-format", "limit", "cursor"),
                "fields": PagingOverlay + {
                    "space-id": SpaceFilterOverlay,
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Pages",
                        "short_desc": "Return only these pages",
                        "element_ref_data": RefPage,
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "createPage",
                "method": "POST",
                "path": "/pages",
                "operation_id": "createPage",
                "display_name": "Create Page",
                "short_desc": "Create a page in a space",
                "desc": "Creates a page.\n\n"
                    "`spaceId` is required. Without `parentId` the page is created at the top level of "
                    "the space. Set `status` to `draft` to create it unpublished.",
                "groups": (GroupPages,),
                "field_order": ("spaceId", "title", "body", "parentId", "status", "private"),
                "fields": {
                    "spaceId": SpaceOverlay,
                    "parentId": PageOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Parent Page",
                        "short_desc": "Create the page under this page",
                    },
                    "body": BodyOverlay,
                    "title": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Title",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "getPageById",
                "method": "GET",
                "path": "/pages/{id}",
                "operation_id": "getPageById",
                "display_name": "Get Page",
                "short_desc": "Retrieve one page",
                "desc": "Retrieves a page.\n\n"
                    "Set `body-format` to get the content — `storage` for the XHTML storage format — and "
                    "the various `include-*` options for labels, versions, properties and the like. None "
                    "of them is returned by default.",
                "groups": (GroupPages,),
                "field_order": ("id", "body-format", "version", "get-draft", "include-labels"),
                "fields": {
                    "id": PageOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "updatePage",
                "method": "PUT",
                "path": "/pages/{id}",
                "operation_id": "updatePage",
                "display_name": "Update Page",
                "short_desc": "Update a page's title, body, parent or status",
                "desc": "Updates a page.\n\n"
                    "This is a **read-modify-write**: `version.number` must be one greater than the "
                    "page's current version, so read the page first. `title`, `body` and `status` are all "
                    "required by Confluence even when they are not changing, so send the current values "
                    "for the ones being kept.",
                "groups": (GroupPages,),
                "field_order": ("id", "title", "body", "version", "status", "spaceId", "parentId"),
                "fields": {
                    "id": PageOverlay,
                    "body.id": IdFollowsPathOverlay,
                    "spaceId": SpaceOverlay + <RestSchemaFieldOverlayInfo>{"preselected": False},
                    "parentId": PageOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Parent Page",
                        "preselected": False,
                    },
                    "body.body": BodyOverlay,
                    "version": VersionOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "updatePageTitle",
                "method": "PUT",
                "path": "/pages/{id}/title",
                "operation_id": "updatePageTitle",
                "display_name": "Rename Page",
                "short_desc": "Change only a page's title",
                "desc": "Changes a page's title without touching its body.\n\n"
                    "This exists because **Update Page** requires the body as well, so renaming a page "
                    "through it means reading and resending the whole page.\n\n"
                    "Unlike **Update Page** this takes no version, so it is not a read-modify-write: "
                    "Confluence takes the rename as given.",
                "groups": (GroupPages,),
                "field_order": ("id", "title", "status"),
                "fields": {
                    "id": PageOverlay,
                    "title": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Title",
                        "short_desc": "The new title",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "deletePage",
                "method": "DELETE",
                "path": "/pages/{id}",
                "operation_id": "deletePage",
                "display_name": "Delete Page",
                "short_desc": "Move a page to the trash, or purge it",
                "desc": "Deletes a page.\n\n"
                    "By default the page goes to the space's trash and can be restored. `purge` removes "
                    "it permanently, and only works on a page that is already in the trash.",
                "groups": (GroupPages,),
                "field_order": ("id", "purge", "draft"),
                "fields": {
                    "id": PageOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "getPagesInSpace",
                "method": "GET",
                "path": "/spaces/{id}/pages",
                "operation_id": "getPagesInSpace",
                "display_name": "List Pages in Space",
                "short_desc": "List the pages in one space",
                "desc": "Lists the pages in a space.",
                "groups": (GroupPages,),
                "field_order": ("id", "title", "status", "sort", "body-format", "limit", "cursor"),
                "fields": PagingOverlay + {
                    "id": SpaceOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "getLabelPages",
                "method": "GET",
                "path": "/labels/{id}/pages",
                "operation_id": "getLabelPages",
                "display_name": "List Pages for Label",
                "short_desc": "List the pages carrying one label",
                "desc": "Lists the pages a label is applied to.",
                "groups": (GroupPages,),
                "field_order": ("id", "space-id", "sort", "body-format", "limit", "cursor"),
                "fields": PagingOverlay + {
                    "id": LabelOverlay,
                    "space-id": SpaceFilterOverlay,
                },
            },

            # --- Blog Posts ---
            <RestSchemaActionInfo>{
                "action": "getBlogPosts",
                "method": "GET",
                "path": "/blogposts",
                "operation_id": "getBlogPosts",
                "display_name": "List Blog Posts",
                "short_desc": "List the blog posts on the site",
                "desc": "Lists blog posts across the whole site, with the same filters as **List Pages**.",
                "groups": (GroupBlogPosts,),
                "field_order": ("space-id", "title", "status", "sort", "body-format", "limit", "cursor"),
                "fields": PagingOverlay + {
                    "space-id": SpaceFilterOverlay,
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Blog Posts",
                        "short_desc": "Return only these blog posts",
                        "element_ref_data": RefBlogPost,
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "createBlogPost",
                "method": "POST",
                "path": "/blogposts",
                "operation_id": "createBlogPost",
                "display_name": "Create Blog Post",
                "short_desc": "Create a blog post in a space",
                "desc": "Creates a blog post. `spaceId` is required; a blog post has no parent.",
                "groups": (GroupBlogPosts,),
                "field_order": ("spaceId", "title", "body", "status", "private"),
                "fields": {
                    "spaceId": SpaceOverlay,
                    "body": BodyOverlay,
                    "title": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Title",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "getBlogPostById",
                "method": "GET",
                "path": "/blogposts/{id}",
                "operation_id": "getBlogPostById",
                "display_name": "Get Blog Post",
                "short_desc": "Retrieve one blog post",
                "desc": "Retrieves a blog post. As with a page, the body and the `include-*` extras are "
                    "returned only when asked for.",
                "groups": (GroupBlogPosts,),
                "field_order": ("id", "body-format", "version", "include-labels"),
                "fields": {
                    "id": BlogPostOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "updateBlogPost",
                "method": "PUT",
                "path": "/blogposts/{id}",
                "operation_id": "updateBlogPost",
                "display_name": "Update Blog Post",
                "short_desc": "Update a blog post's title, body or status",
                "desc": "Updates a blog post.\n\n"
                    "As with a page this is a read-modify-write: `version.number` must be one greater "
                    "than the current version, and `title`, `body` and `status` are required even when "
                    "unchanged.",
                "groups": (GroupBlogPosts,),
                "field_order": ("id", "title", "body", "version", "status", "spaceId"),
                "fields": {
                    "id": BlogPostOverlay,
                    "body.id": IdFollowsPathOverlay,
                    "spaceId": SpaceOverlay + <RestSchemaFieldOverlayInfo>{"preselected": False},
                    "body.body": BodyOverlay,
                    "version": VersionOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "deleteBlogPost",
                "method": "DELETE",
                "path": "/blogposts/{id}",
                "operation_id": "deleteBlogPost",
                "display_name": "Delete Blog Post",
                "short_desc": "Move a blog post to the trash, or purge it",
                "desc": "Deletes a blog post. As with a page, it goes to the trash unless `purge` is set.",
                "groups": (GroupBlogPosts,),
                "field_order": ("id", "purge", "draft"),
                "fields": {
                    "id": BlogPostOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "getBlogPostsInSpace",
                "method": "GET",
                "path": "/spaces/{id}/blogposts",
                "operation_id": "getBlogPostsInSpace",
                "display_name": "List Blog Posts in Space",
                "short_desc": "List the blog posts in one space",
                "desc": "Lists the blog posts in a space.",
                "groups": (GroupBlogPosts,),
                "field_order": ("id", "title", "status", "sort", "body-format", "limit", "cursor"),
                "fields": PagingOverlay + {
                    "id": SpaceOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "getLabelBlogPosts",
                "method": "GET",
                "path": "/labels/{id}/blogposts",
                "operation_id": "getLabelBlogPosts",
                "display_name": "List Blog Posts for Label",
                "short_desc": "List the blog posts carrying one label",
                "desc": "Lists the blog posts a label is applied to.",
                "groups": (GroupBlogPosts,),
                "field_order": ("id", "space-id", "sort", "body-format", "limit", "cursor"),
                "fields": PagingOverlay + {
                    "id": LabelOverlay,
                    "space-id": SpaceFilterOverlay,
                },
            },

            # --- Spaces ---
            <RestSchemaActionInfo>{
                "action": "getSpaces",
                "method": "GET",
                "path": "/spaces",
                "operation_id": "getSpaces",
                "display_name": "List Spaces",
                "short_desc": "List the spaces on the site",
                "desc": "Lists spaces.\n\n"
                    "`type` separates a personal space from a global one, and `status` selects archived "
                    "spaces as well as current ones.",
                "groups": (GroupSpaces,),
                "field_order": ("keys", "type", "status", "sort", "limit", "cursor"),
                "fields": PagingOverlay,
            },
            <RestSchemaActionInfo>{
                "action": "getSpaceById",
                "method": "GET",
                "path": "/spaces/{id}",
                "operation_id": "getSpaceById",
                "display_name": "Get Space",
                "short_desc": "Retrieve one space",
                "desc": "Retrieves a space with its key, name, type and homepage.",
                "groups": (GroupSpaces,),
                "field_order": ("id", "description-format"),
                "fields": {
                    "id": SpaceOverlay,
                },
            },

            # --- Tasks ---
            <RestSchemaActionInfo>{
                "action": "getTasks",
                "method": "GET",
                "path": "/tasks",
                "operation_id": "getTasks",
                "display_name": "List Tasks",
                "short_desc": "List the tasks on the site",
                "desc": "Lists tasks — the checkboxes inside pages and blog posts.\n\n"
                    "`status` selects complete or incomplete ones, and the various `*-id` filters restrict "
                    "the listing to one space, page or assignee.",
                "groups": (GroupTasks,),
                "field_order": ("space-id", "page-id", "status", "assigned-to", "limit", "cursor"),
                "fields": PagingOverlay,
            },
            <RestSchemaActionInfo>{
                "action": "getTaskById",
                "method": "GET",
                "path": "/tasks/{id}",
                "operation_id": "getTaskById",
                "display_name": "Get Task",
                "short_desc": "Retrieve one task",
                "desc": "Retrieves a task with its status, body and the content it lives in.",
                "groups": (GroupTasks,),
                "field_order": ("id", "body-format"),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Task",
                        "ref_data": RefTask,
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "updateTask",
                "method": "PUT",
                "path": "/tasks/{id}",
                "operation_id": "updateTask",
                "display_name": "Update Task",
                "short_desc": "Complete or reopen a task",
                "desc": "Updates a task, which in practice means marking it `complete` or `incomplete`.\n\n"
                    "Unlike a page, a task carries no version, so there is no read-modify-write here.",
                "groups": (GroupTasks,),
                "field_order": ("id", "status"),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Task",
                        "ref_data": RefTask,
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "body.id": IdFollowsPathOverlay,
                    "status": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Status",
                        "short_desc": "\"complete\" or \"incomplete\"",
                        "preselected": True,
                    },
                },
            },

            # --- Content ---
            <RestSchemaActionInfo>{
                "action": "getLabels",
                "method": "GET",
                "path": "/labels",
                "operation_id": "getLabels",
                "display_name": "List Labels",
                "short_desc": "List the labels on the site",
                "desc": "Lists labels.\n\n"
                    "This is what backs the label dropdowns on the label listings; the application this "
                    "module replaces offered those dropdowns without exporting the endpoint that fills "
                    "them.",
                "groups": (GroupContent,),
                "field_order": ("label-id", "prefix", "sort", "limit", "cursor"),
                "fields": PagingOverlay,
            },
            <RestSchemaActionInfo>{
                "action": "getAttachments",
                "method": "GET",
                "path": "/attachments",
                "operation_id": "getAttachments",
                "display_name": "List Attachments",
                "short_desc": "List the attachments on the site",
                "desc": "Lists attachments, with the content each is attached to and its media type.\n\n"
                    "This is exported so that the **New Attachment** event has something to read: the "
                    "application this module replaces polled attachments without exporting any way to "
                    "list or read one.",
                "groups": (GroupContent,),
                "field_order": ("status", "mediaType", "filename", "sort", "limit", "cursor"),
                "fields": PagingOverlay,
            },
        );
    }
}
}
