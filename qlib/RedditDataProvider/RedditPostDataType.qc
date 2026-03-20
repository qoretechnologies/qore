# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedditDataProvider module definition

/*  RedditPostDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Reddit post/link record type
public class RedditPostDataType inherits HashDataType {
    public {
        #! Field definitions for Reddit posts
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The post ID",
                "desc": "The unique identifier of the post (base36)",
            },
            "name": {
                "type": StringType,
                "display_name": "Fullname",
                "short_desc": "The post fullname (t3_xxxxx)",
                "desc": "The type-prefixed fullname of the post (t3_ prefix)",
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "The post title",
                "desc": "The title of the post",
            },
            "selftext": {
                "type": StringOrNothingType,
                "display_name": "Body Text",
                "short_desc": "The post body text",
                "desc": "The body text of the post (for self/text posts)",
            },
            "author": {
                "type": StringOrNothingType,
                "display_name": "Author",
                "short_desc": "The post author",
                "desc": "The username of the post author",
            },
            "subreddit": {
                "type": StringOrNothingType,
                "display_name": "Subreddit",
                "short_desc": "The subreddit name",
                "desc": "The display name of the subreddit where the post was made",
            },
            "score": {
                "type": IntOrNothingType,
                "display_name": "Score",
                "short_desc": "The post score",
                "desc": "The net score (upvotes minus downvotes) of the post",
            },
            "upvote_ratio": {
                "type": FloatOrNothingType,
                "display_name": "Upvote Ratio",
                "short_desc": "Ratio of upvotes",
                "desc": "The ratio of upvotes to total votes (0.0 to 1.0)",
            },
            "num_comments": {
                "type": IntOrNothingType,
                "display_name": "Comment Count",
                "short_desc": "Number of comments",
                "desc": "The number of comments on the post",
            },
            "created_utc": {
                "type": FloatOrNothingType,
                "display_name": "Created (UTC)",
                "short_desc": "Post creation time",
                "desc": "Unix timestamp when the post was created",
            },
            "url": {
                "type": StringOrNothingType,
                "display_name": "URL",
                "short_desc": "The post URL",
                "desc": "The URL of the post (external link or self post URL)",
            },
            "permalink": {
                "type": StringOrNothingType,
                "display_name": "Permalink",
                "short_desc": "Permalink to the post",
                "desc": "The permanent link path to the post on Reddit",
            },
            "is_self": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Self Post",
                "short_desc": "Whether this is a text post",
                "desc": "Indicates if this is a self/text post (`True`) or a link post (`False`)",
            },
            "over_18": {
                "type": SoftBoolOrNothingType,
                "display_name": "NSFW",
                "short_desc": "Whether the post is NSFW",
                "desc": "Indicates if the post is marked as NSFW (over 18)",
            },
            "stickied": {
                "type": SoftBoolOrNothingType,
                "display_name": "Stickied",
                "short_desc": "Whether the post is stickied",
                "desc": "Indicates if the post is stickied (pinned) in the subreddit",
            },
            "locked": {
                "type": SoftBoolOrNothingType,
                "display_name": "Locked",
                "short_desc": "Whether the post is locked",
                "desc": "Indicates if the post is locked (no new comments allowed)",
            },
            "distinguished": {
                "type": StringOrNothingType,
                "display_name": "Distinguished",
                "short_desc": "Distinguished status",
                "desc": "The distinguished status of the post (`moderator`, `admin`, or `NOTHING`)",
            },
            "link_flair_text": {
                "type": StringOrNothingType,
                "display_name": "Flair",
                "short_desc": "The post flair text",
                "desc": "The flair text associated with the post",
            },
            "thumbnail": {
                "type": StringOrNothingType,
                "display_name": "Thumbnail",
                "short_desc": "Thumbnail URL",
                "desc": "URL of the post thumbnail image",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace RedditDataProvider
