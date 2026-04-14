# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CircleCommentDataType.qc defines data types for Circle comments

/*  CircleCommentDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Circle comment response data type
public class CircleCommentDataType inherits HashDataType {
    public {
        #! Field definitions for a Circle comment
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "Comment ID",
                "short_desc": "Unique identifier for the comment",
                "desc": "The unique identifier for the comment",
            },
            "body": {
                "type": StringOrNothingType,
                "display_name": "Body",
                "short_desc": "Comment content",
                "desc": "The body content of the comment",
            },
            "post_id": {
                "type": IntOrNothingType,
                "display_name": "Post ID",
                "short_desc": "ID of the parent post",
                "desc": "The ID of the post this comment belongs to",
            },
            "user_id": {
                "type": IntOrNothingType,
                "display_name": "User ID",
                "short_desc": "Author user ID",
                "desc": "The ID of the user who created the comment",
            },
            "user_name": {
                "type": StringOrNothingType,
                "display_name": "User Name",
                "short_desc": "Author name",
                "desc": "The name of the user who created the comment",
            },
            "likes_count": {
                "type": IntOrNothingType,
                "display_name": "Likes Count",
                "short_desc": "Number of likes",
                "desc": "The total number of likes on this comment",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "The date and time the comment was created",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "The date and time the comment was last updated",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Circle create comment request data type
public class CircleCreateCommentRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating a Circle comment
        const Fields = {
            "post_id": {
                "type": IntType,
                "display_name": "Post ID",
                "short_desc": "Post to comment on",
                "desc": "The ID of the post to add the comment to",
                "required": True,
                "ref_data": "posts",
                "example_value": 12345,
            },
            "body": {
                "type": StringType,
                "display_name": "Body",
                "short_desc": "Comment content",
                "desc": "The body content of the comment in HTML format. "
                    "Example: `<p>Great post!</p>`",
                "required": True,
                "example_value": "Great post, thanks for sharing!",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Circle delete comment request data type
public class CircleDeleteCommentRequestDataType inherits HashDataType {
    public {
        #! Field definitions for deleting a Circle comment
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "Comment ID",
                "short_desc": "ID of the comment to delete",
                "desc": "The unique identifier of the comment to delete",
                "required": True,
            },
        };
        # NOTE: no ref_data on comment id since comments ref_data is not supported
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace CircleDataProvider
