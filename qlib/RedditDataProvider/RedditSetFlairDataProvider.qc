# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file RedditSetFlairDataProvider.qc Reddit set flair action

/*  RedditSetFlairDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for setting flair
public class RedditSetFlairRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "subreddit": {
                "type": StringType,
                "display_name": "Subreddit",
                "short_desc": "The subreddit",
                "desc": "The display name of the subreddit",
                "required": True,
            },
            "flair_template_id": {
                "type": StringOrNothingType,
                "display_name": "Flair Template ID",
                "short_desc": "The flair template to apply",
                "desc": "The ID of the flair template to apply",
            },
            "link": {
                "type": StringOrNothingType,
                "display_name": "Link Fullname",
                "short_desc": "The post fullname for link flair",
                "desc": "The fullname (t3_ prefix) of the post to set link flair on",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Username",
                "short_desc": "The username for user flair",
                "desc": "The username to set user flair for",
            },
            "text": {
                "type": StringOrNothingType,
                "display_name": "Flair Text",
                "short_desc": "Custom flair text",
                "desc": "Custom flair text (if the template allows editing)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Reddit set flair action data provider
public class RedditSetFlairDataProvider inherits RedditDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "set-flair",
            "desc": "Set flair on a post or user in a subreddit",
            "type": "RedditSetFlairDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(RedditRestClient::RedditRestClient rest) : RedditDataProviderBase(rest) {
    }

    string getName() {
        return "set-flair";
    }

    *string getDesc() {
        return "Set flair on a post or user in a subreddit";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new RedditSetFlairRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string subreddit = remove req.subreddit;
        return doPost("r/" + subreddit + "/api/selectflair", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace RedditDataProvider
