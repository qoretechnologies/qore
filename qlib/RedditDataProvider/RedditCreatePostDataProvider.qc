# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file RedditCreatePostDataProvider.qc Reddit create post action

/*  RedditCreatePostDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace RedditDataProvider {

#! Request type for creating a post
public class RedditCreatePostRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "sr": {
                "type": StringType,
                "display_name": "Subreddit",
                "short_desc": "The subreddit to post to",
                "desc": "The display name of the subreddit to submit the post to",
                "required": True,
                "example_value": "test",
            },
            "title": {
                "type": StringType,
                "display_name": "Title",
                "short_desc": "The post title",
                "desc": "The title of the post (max 300 characters)",
                "required": True,
                "example_value": "My Example Post",
            },
            "kind": {
                "type": StringType,
                "display_name": "Post Type",
                "short_desc": "Type of post (self or link)",
                "desc": "The type of post: `self` for text post, `link` for link post",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "self", "display_name": "Text Post"},
                    <AllowedValueInfo>{"value": "link", "display_name": "Link Post"},
                ),
            },
            "text": {
                "type": StringOrNothingType,
                "display_name": "Body Text",
                "short_desc": "Body text for self posts",
                "desc": "The body text content for self/text posts (markdown formatted)",
            },
            "url": {
                "type": StringOrNothingType,
                "display_name": "URL",
                "short_desc": "URL for link posts",
                "desc": "The URL for link posts",
            },
            "flair_id": {
                "type": StringOrNothingType,
                "display_name": "Flair ID",
                "short_desc": "Post flair template ID",
                "desc": "The flair template ID to apply to the post",
            },
            "flair_text": {
                "type": StringOrNothingType,
                "display_name": "Flair Text",
                "short_desc": "Post flair text",
                "desc": "Custom flair text (if the flair template allows editing)",
            },
            "nsfw": {
                "type": SoftBoolOrNothingType,
                "display_name": "NSFW",
                "short_desc": "Mark as NSFW",
                "desc": "Whether to mark the post as NSFW",
            },
            "spoiler": {
                "type": SoftBoolOrNothingType,
                "display_name": "Spoiler",
                "short_desc": "Mark as spoiler",
                "desc": "Whether to mark the post as a spoiler",
            },
            "sendreplies": {
                "type": SoftBoolOrNothingType,
                "display_name": "Send Replies",
                "short_desc": "Send reply notifications",
                "desc": "Whether to send inbox notifications for replies. Default: `True`",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Reddit create post action data provider
public class RedditCreatePostDataProvider inherits RedditDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new post (submission) on Reddit",
            "type": "RedditCreatePostDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static RedditPostDataType ResponseType();
    }

    constructor(RedditRestClient::RedditRestClient rest) : RedditDataProviderBase(rest) {
    }

    string getName() {
        return "create";
    }

    *string getDesc() {
        return "Create a new post (submission) on Reddit";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new RedditCreatePostRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new RedditPostDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req.api_type = "json";
        hash<auto> response = doPost("api/submit", req);
        # Extract post data from Reddit's response: {"json": {"data": {"id": ..., "name": ..., "url": ...}}}
        return response.json.data ?? response;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace RedditDataProvider
