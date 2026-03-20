# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file RedditVoteDataProvider.qc Reddit vote action

/*  RedditVoteDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for voting
public class RedditVoteRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Thing ID",
                "short_desc": "The fullname of the thing to vote on",
                "desc": "The fullname of the post (t3_) or comment (t1_) to vote on",
                "required": True,
                "example_value": "t3_abc123",
            },
            "dir": {
                "type": IntType,
                "display_name": "Vote Direction",
                "short_desc": "Vote direction (1, 0, -1)",
                "desc": "The vote direction: `1` for upvote, `0` to remove vote, `-1` for downvote",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": 1, "display_name": "Upvote"},
                    <AllowedValueInfo>{"value": 0, "display_name": "Remove Vote"},
                    <AllowedValueInfo>{"value": -1, "display_name": "Downvote"},
                ),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Reddit vote action data provider
public class RedditVoteDataProvider inherits RedditDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "vote",
            "desc": "Vote (upvote, downvote, or unvote) on a post or comment",
            "type": "RedditVoteDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static AbstractDataProviderType ResponseType = AbstractDataProviderTypeMap."hash";
    }

    constructor(RedditRestClient::RedditRestClient rest) : RedditDataProviderBase(rest) {
    }

    string getName() {
        return "vote";
    }

    *string getDesc() {
        return "Vote (upvote, downvote, or unvote) on a post or comment";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new RedditVoteRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("api/vote", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace RedditDataProvider
