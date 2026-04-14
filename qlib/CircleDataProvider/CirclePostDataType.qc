# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CirclePostDataType.qc defines data types for Circle posts

/*  CirclePostDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CircleDataProvider module
public namespace CircleDataProvider {

#! Circle post response data type
public class CirclePostDataType inherits HashDataType {
    public {
        #! Field definitions for a Circle post
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "Post ID",
                "short_desc": "Unique identifier for the post",
                "desc": "The unique identifier for the post",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Post title",
                "desc": "The title of the post",
            },
            "body": {
                "type": StringOrNothingType,
                "display_name": "Body",
                "short_desc": "Post content",
                "desc": "The body content of the post in rich text format",
            },
            "slug": {
                "type": StringOrNothingType,
                "display_name": "Slug",
                "short_desc": "URL-friendly slug",
                "desc": "The URL-friendly slug for the post",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Post status",
                "desc": "The publication status of the post",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "published", "display_name": "Published"},
                    <AllowedValueInfo>{"value": "draft", "display_name": "Draft"},
                    <AllowedValueInfo>{"value": "scheduled", "display_name": "Scheduled"},
                ),
            },
            "space_id": {
                "type": IntOrNothingType,
                "display_name": "Space ID",
                "short_desc": "ID of the space containing this post",
                "desc": "The ID of the space this post belongs to",
            },
            "user_id": {
                "type": IntOrNothingType,
                "display_name": "User ID",
                "short_desc": "Author user ID",
                "desc": "The ID of the user who created the post",
            },
            "comments_count": {
                "type": IntOrNothingType,
                "display_name": "Comments Count",
                "short_desc": "Number of comments",
                "desc": "The total number of comments on this post",
            },
            "likes_count": {
                "type": IntOrNothingType,
                "display_name": "Likes Count",
                "short_desc": "Number of likes",
                "desc": "The total number of likes on this post",
            },
            "is_pinned": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Pinned",
                "short_desc": "Whether the post is pinned",
                "desc": "Whether the post is pinned to the top of its space",
            },
            "is_comments_enabled": {
                "type": SoftBoolOrNothingType,
                "display_name": "Comments Enabled",
                "short_desc": "Whether comments are enabled",
                "desc": "Whether comments are enabled on this post",
            },
            "is_comments_closed": {
                "type": SoftBoolOrNothingType,
                "display_name": "Comments Closed",
                "short_desc": "Whether comments are closed",
                "desc": "Whether comments are closed on this post",
            },
            "is_liking_enabled": {
                "type": SoftBoolOrNothingType,
                "display_name": "Liking Enabled",
                "short_desc": "Whether liking is enabled",
                "desc": "Whether liking is enabled on this post",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "The date and time the post was created",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "The date and time the post was last updated",
            },
            "published_at": {
                "type": StringOrNothingType,
                "display_name": "Published At",
                "short_desc": "Publication timestamp",
                "desc": "The date and time the post was published",
            },
            "url": {
                "type": StringOrNothingType,
                "display_name": "URL",
                "short_desc": "Post URL",
                "desc": "The URL of the post",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Circle create post request data type
public class CircleCreatePostRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating a Circle post
        const Fields = {
            "space_id": {
                "type": IntType,
                "display_name": "Space ID",
                "short_desc": "Space to create the post in",
                "desc": "The ID of the space to create the post in",
                "required": True,
                "ref_data": "spaces",
                "example_value": 12345,
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "Post title",
                "desc": "The title of the post",
                "required": True,
                "example_value": "Welcome to the community",
            },
            "body": {
                "type": StringOrNothingType,
                "display_name": "Body",
                "short_desc": "Post content",
                "desc": "The body content of the post in HTML format. "
                    "Example: `<p>Hello <strong>world</strong></p>`",
                "example_value": "Hello everyone! This is a new post.",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Post status",
                "desc": "The publication status of the post",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "published", "display_name": "Published"},
                    <AllowedValueInfo>{"value": "draft", "display_name": "Draft"},
                    <AllowedValueInfo>{"value": "scheduled", "display_name": "Scheduled"},
                ),
                "default_value": "published",
            },
            "is_pinned": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Pinned",
                "short_desc": "Pin post to top of space",
                "desc": "Whether to pin the post to the top of its space",
            },
            "is_comments_enabled": {
                "type": SoftBoolOrNothingType,
                "display_name": "Comments Enabled",
                "short_desc": "Enable comments on the post",
                "desc": "Whether to enable comments on the post. Default: `True`",
            },
            "is_liking_enabled": {
                "type": SoftBoolOrNothingType,
                "display_name": "Liking Enabled",
                "short_desc": "Enable liking on the post",
                "desc": "Whether to enable liking on the post. Default: `True`",
            },
            "published_at": {
                "type": DateOrNothingType,
                "display_name": "Published At",
                "short_desc": "Publication date for scheduling",
                "desc": "The date and time to publish the post in ISO 8601 format. "
                    "Use with `status` set to `scheduled` to schedule a post for future "
                    "publication. Example: `2026-04-15T10:00:00Z`",
            },
            "skip_notifications": {
                "type": SoftBoolOrNothingType,
                "display_name": "Skip Notifications",
                "short_desc": "Skip sending notifications",
                "desc": "If `True`, no notifications will be sent to members about this post",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Circle update post request data type
public class CircleUpdatePostRequestDataType inherits HashDataType {
    public {
        #! Field definitions for updating a Circle post
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "Post ID",
                "short_desc": "ID of the post to update",
                "desc": "The unique identifier of the post to update",
                "required": True,
                "ref_data": "posts",
                "example_value": 12345,
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Post title",
                "desc": "The title of the post",
                "example_value": "Updated post title",
            },
            "body": {
                "type": StringOrNothingType,
                "display_name": "Body",
                "short_desc": "Post content",
                "desc": "The body content of the post in HTML format. "
                    "Example: `<p>Hello <strong>world</strong></p>`",
                "example_value": "Updated post content.",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Post status",
                "desc": "The publication status of the post",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "published", "display_name": "Published"},
                    <AllowedValueInfo>{"value": "draft", "display_name": "Draft"},
                    <AllowedValueInfo>{"value": "scheduled", "display_name": "Scheduled"},
                ),
            },
            "is_pinned": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Pinned",
                "short_desc": "Pin post to top of space",
                "desc": "Whether to pin the post to the top of its space",
            },
            "is_comments_enabled": {
                "type": SoftBoolOrNothingType,
                "display_name": "Comments Enabled",
                "short_desc": "Enable comments on the post",
                "desc": "Whether to enable comments on the post",
            },
            "is_comments_closed": {
                "type": SoftBoolOrNothingType,
                "display_name": "Comments Closed",
                "short_desc": "Close comments on the post",
                "desc": "Whether to close comments on the post",
            },
            "is_liking_enabled": {
                "type": SoftBoolOrNothingType,
                "display_name": "Liking Enabled",
                "short_desc": "Enable liking on the post",
                "desc": "Whether to enable liking on the post",
            },
            "published_at": {
                "type": DateOrNothingType,
                "display_name": "Published At",
                "short_desc": "Publication date",
                "desc": "The date and time to publish the post in ISO 8601 format. "
                    "Example: `2026-04-15T10:00:00Z`",
            },
            "skip_notifications": {
                "type": SoftBoolOrNothingType,
                "display_name": "Skip Notifications",
                "short_desc": "Skip sending notifications",
                "desc": "If `True`, no notifications will be sent about this update",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Circle delete post request data type
public class CircleDeletePostRequestDataType inherits HashDataType {
    public {
        #! Field definitions for deleting a Circle post
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "Post ID",
                "short_desc": "ID of the post to delete",
                "desc": "The unique identifier of the post to delete",
                "required": True,
                "ref_data": "posts",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace CircleDataProvider
