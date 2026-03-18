# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedditDataProvider module definition

/*  RedditMessageDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Reddit message record type
public class RedditMessageDataType inherits HashDataType {
    public {
        #! Field definitions for Reddit messages
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The message ID",
                "desc": "The unique identifier of the message",
            },
            "name": {
                "type": StringType,
                "display_name": "Fullname",
                "short_desc": "The message fullname (t4_xxxxx)",
                "desc": "The type-prefixed fullname of the message (t4_ prefix)",
            },
            "subject": {
                "type": StringOrNothingType,
                "display_name": "Subject",
                "short_desc": "The message subject",
                "desc": "The subject line of the message",
            },
            "body": {
                "type": StringOrNothingType,
                "display_name": "Body",
                "short_desc": "The message text",
                "desc": "The body text of the message",
            },
            "author": {
                "type": StringOrNothingType,
                "display_name": "Author",
                "short_desc": "The message sender",
                "desc": "The username of the message sender",
            },
            "dest": {
                "type": StringOrNothingType,
                "display_name": "Destination",
                "short_desc": "The message recipient",
                "desc": "The username of the message recipient",
            },
            "created_utc": {
                "type": FloatOrNothingType,
                "display_name": "Created (UTC)",
                "short_desc": "Message creation time",
                "desc": "Unix timestamp when the message was sent",
            },
            "new": {
                "type": SoftBoolOrNothingType,
                "display_name": "Unread",
                "short_desc": "Whether the message is unread",
                "desc": "Indicates if the message has not been read yet",
            },
            "was_comment": {
                "type": SoftBoolOrNothingType,
                "display_name": "Was Comment",
                "short_desc": "Whether this is a comment reply",
                "desc": "Indicates if this message is a reply notification from a comment",
            },
            "parent_id": {
                "type": StringOrNothingType,
                "display_name": "Parent ID",
                "short_desc": "The parent message fullname",
                "desc": "The fullname of the parent message in a thread",
            },
            "first_message_name": {
                "type": StringOrNothingType,
                "display_name": "First Message",
                "short_desc": "First message in thread",
                "desc": "The fullname of the first message in the thread",
            },
            "subreddit": {
                "type": StringOrNothingType,
                "display_name": "Subreddit",
                "short_desc": "Related subreddit",
                "desc": "The subreddit related to this message, if any",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace RedditDataProvider
