# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedditDataProvider module definition

/*  RedditSubredditDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Reddit subreddit record type
public class RedditSubredditDataType inherits HashDataType {
    public {
        #! Field definitions for Reddit subreddits
        const Fields = {
            "display_name": {
                "type": StringType,
                "display_name": "Display Name",
                "short_desc": "The subreddit display name",
                "desc": "The display name of the subreddit (without r/ prefix)",
                "example_value": "programming",
            },
            "name": {
                "type": StringType,
                "display_name": "Fullname",
                "short_desc": "The subreddit fullname (t5_xxxxx)",
                "desc": "The type-prefixed fullname of the subreddit (t5_ prefix)",
            },
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The subreddit ID",
                "desc": "The unique identifier of the subreddit (base36)",
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "The subreddit title",
                "desc": "The full title of the subreddit",
            },
            "public_description": {
                "type": StringOrNothingType,
                "display_name": "Public Description",
                "short_desc": "Brief public description",
                "desc": "The public-facing description of the subreddit",
            },
            "subscribers": {
                "type": IntOrNothingType,
                "display_name": "Subscribers",
                "short_desc": "Number of subscribers",
                "desc": "The number of subscribers to the subreddit",
            },
            "created_utc": {
                "type": FloatOrNothingType,
                "display_name": "Created (UTC)",
                "short_desc": "Subreddit creation time",
                "desc": "Unix timestamp when the subreddit was created",
            },
            "over18": {
                "type": SoftBoolOrNothingType,
                "display_name": "NSFW",
                "short_desc": "Whether the subreddit is NSFW",
                "desc": "Indicates if the subreddit is marked as NSFW (over 18)",
            },
            "subreddit_type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "Subreddit type",
                "desc": "The type of subreddit (public, private, restricted, etc.)",
            },
            "url": {
                "type": StringOrNothingType,
                "display_name": "URL",
                "short_desc": "Subreddit URL path",
                "desc": "The URL path of the subreddit (e.g., /r/programming/)",
            },
            "icon_img": {
                "type": StringOrNothingType,
                "display_name": "Icon",
                "short_desc": "Subreddit icon URL",
                "desc": "URL of the subreddit icon image",
            },
            "banner_img": {
                "type": StringOrNothingType,
                "display_name": "Banner",
                "short_desc": "Subreddit banner URL",
                "desc": "URL of the subreddit banner image",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace RedditDataProvider
