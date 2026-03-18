# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedditDataProvider module definition

/*  RedditAccountDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Reddit account record type
public class RedditAccountDataType inherits HashDataType {
    public {
        #! Field definitions for Reddit accounts
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Username",
                "short_desc": "The Reddit username",
                "desc": "The username of the Reddit account",
                "example_value": "spez",
            },
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The account ID",
                "desc": "The unique identifier of the Reddit account",
            },
            "created_utc": {
                "type": FloatOrNothingType,
                "display_name": "Created (UTC)",
                "short_desc": "Account creation time",
                "desc": "Unix timestamp when the account was created",
            },
            "link_karma": {
                "type": IntOrNothingType,
                "display_name": "Link Karma",
                "short_desc": "Total link karma",
                "desc": "Total link (post) karma for the account",
            },
            "comment_karma": {
                "type": IntOrNothingType,
                "display_name": "Comment Karma",
                "short_desc": "Total comment karma",
                "desc": "Total comment karma for the account",
            },
            "is_gold": {
                "type": SoftBoolOrNothingType,
                "display_name": "Gold Status",
                "short_desc": "Whether the user has Reddit Gold",
                "desc": "Indicates if the user has Reddit Gold/Premium",
            },
            "is_mod": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Moderator",
                "short_desc": "Whether the user is a moderator",
                "desc": "Indicates if the user moderates any subreddits",
            },
            "has_verified_email": {
                "type": SoftBoolOrNothingType,
                "display_name": "Email Verified",
                "short_desc": "Whether the email is verified",
                "desc": "Indicates if the user has verified their email address",
            },
            "icon_img": {
                "type": StringOrNothingType,
                "display_name": "Avatar URL",
                "short_desc": "URL of the user's avatar",
                "desc": "URL of the user's profile avatar image",
            },
            "subreddit": {
                "type": AutoHashOrNothingType,
                "display_name": "User Subreddit",
                "short_desc": "The user's profile subreddit",
                "desc": "Hash containing information about the user's profile subreddit (u/username)",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace RedditDataProvider
