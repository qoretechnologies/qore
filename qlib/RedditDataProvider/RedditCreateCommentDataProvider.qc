# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file RedditCreateCommentDataProvider.qc Reddit create comment action

/*  RedditCreateCommentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a comment
public class RedditCreateCommentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "parent": {
                "type": StringType,
                "display_name": "Parent",
                "short_desc": "The fullname of the parent",
                "desc": "The fullname of the parent post (t3_) or comment (t1_) to reply to",
                "required": True,
            },
            "text": {
                "type": StringType,
                "display_name": "Text",
                "short_desc": "The comment text",
                "desc": "The body text of the comment (markdown formatted)",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Reddit create comment action data provider
public class RedditCreateCommentDataProvider inherits RedditDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new comment on a post or in reply to another comment",
            "type": "RedditCreateCommentDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(RedditRestClient::RedditRestClient rest) : RedditDataProviderBase(rest) {
    }

    string getName() {
        return "create";
    }

    *string getDesc() {
        return "Create a new comment on a post or in reply to another comment";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new RedditCreateCommentRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new RedditCommentDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req.api_type = "json";
        return doPost("api/comment", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace RedditDataProvider
