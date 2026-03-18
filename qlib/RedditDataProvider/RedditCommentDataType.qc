# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedditDataProvider module definition

/*  RedditCommentDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the RedditDataProvider module
public namespace RedditDataProvider {

#! Reddit comment record type
public class RedditCommentDataType inherits HashDataType {
    public {
        #! Field definitions for Reddit comments
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The comment ID",
                "desc": "The unique identifier of the comment (base36)",
            },
            "name": {
                "type": StringType,
                "display_name": "Fullname",
                "short_desc": "The comment fullname (t1_xxxxx)",
                "desc": "The type-prefixed fullname of the comment (t1_ prefix)",
            },
            "body": {
                "type": StringOrNothingType,
                "display_name": "Body",
                "short_desc": "The comment text",
                "desc": "The raw text content of the comment",
            },
            "author": {
                "type": StringOrNothingType,
                "display_name": "Author",
                "short_desc": "The comment author",
                "desc": "The username of the comment author",
            },
            "subreddit": {
                "type": StringOrNothingType,
                "display_name": "Subreddit",
                "short_desc": "The subreddit name",
                "desc": "The display name of the subreddit where the comment was made",
            },
            "score": {
                "type": IntOrNothingType,
                "display_name": "Score",
                "short_desc": "The comment score",
                "desc": "The net score (upvotes minus downvotes) of the comment",
            },
            "created_utc": {
                "type": FloatOrNothingType,
                "display_name": "Created (UTC)",
                "short_desc": "Comment creation time",
                "desc": "Unix timestamp when the comment was created",
            },
            "parent_id": {
                "type": StringOrNothingType,
                "display_name": "Parent ID",
                "short_desc": "The parent thing fullname",
                "desc": "The fullname of the parent (t1_ for comment, t3_ for post)",
            },
            "link_id": {
                "type": StringOrNothingType,
                "display_name": "Link ID",
                "short_desc": "The parent post fullname",
                "desc": "The fullname of the parent post (t3_ prefix)",
            },
            "permalink": {
                "type": StringOrNothingType,
                "display_name": "Permalink",
                "short_desc": "Permalink to the comment",
                "desc": "The permanent link path to the comment on Reddit",
            },
            "is_submitter": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is OP",
                "short_desc": "Whether the author is the post submitter",
                "desc": "Indicates if the comment author is the original post submitter",
            },
            "distinguished": {
                "type": StringOrNothingType,
                "display_name": "Distinguished",
                "short_desc": "Distinguished status",
                "desc": "The distinguished status of the comment (moderator, admin, or NOTHING)",
            },
            "stickied": {
                "type": SoftBoolOrNothingType,
                "display_name": "Stickied",
                "short_desc": "Whether the comment is stickied",
                "desc": "Indicates if the comment is stickied (pinned) at the top",
            },
            "edited": {
                "type": AutoOrNothingType,
                "display_name": "Edited",
                "short_desc": "Edit timestamp or False",
                "desc": "Unix timestamp of the last edit, or False if never edited",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace RedditDataProvider
