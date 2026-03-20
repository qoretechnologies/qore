# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file RedditSubscribeDataProvider.qc Reddit subscribe/unsubscribe action

/*  RedditSubscribeDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for subscribing/unsubscribing
public class RedditSubscribeRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "action": {
                "type": StringType,
                "display_name": "Action",
                "short_desc": "Subscribe or unsubscribe",
                "desc": "The action to perform",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "sub", "display_name": "Subscribe"},
                    <AllowedValueInfo>{"value": "unsub", "display_name": "Unsubscribe"},
                ),
            },
            "sr_name": {
                "type": StringType,
                "display_name": "Subreddit Name",
                "short_desc": "The subreddit to subscribe to",
                "desc": "The display name of the subreddit (without r/ prefix)",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Reddit subscribe/unsubscribe action data provider
public class RedditSubscribeDataProvider inherits RedditDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "subscribe",
            "desc": "Subscribe to or unsubscribe from a subreddit",
            "type": "RedditSubscribeDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static AbstractDataProviderType ResponseType = AbstractDataProviderTypeMap."hash";
    }

    constructor(RedditRestClient::RedditRestClient rest) : RedditDataProviderBase(rest) {
    }

    string getName() {
        return "subscribe";
    }

    *string getDesc() {
        return "Subscribe to or unsubscribe from a subreddit";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new RedditSubscribeRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("api/subscribe", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace RedditDataProvider
