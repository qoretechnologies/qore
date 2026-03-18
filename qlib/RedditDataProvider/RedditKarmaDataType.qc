# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedditDataProvider module definition

/*  RedditKarmaDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Reddit karma breakdown record type
public class RedditKarmaDataType inherits HashDataType {
    public {
        #! Field definitions for Reddit karma entries
        const Fields = {
            "sr": {
                "type": StringType,
                "display_name": "Subreddit",
                "short_desc": "The subreddit name",
                "desc": "The display name of the subreddit",
            },
            "comment_karma": {
                "type": IntOrNothingType,
                "display_name": "Comment Karma",
                "short_desc": "Comment karma in this subreddit",
                "desc": "Comment karma earned in this subreddit",
            },
            "link_karma": {
                "type": IntOrNothingType,
                "display_name": "Link Karma",
                "short_desc": "Link karma in this subreddit",
                "desc": "Link (post) karma earned in this subreddit",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace RedditDataProvider
