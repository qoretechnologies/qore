# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file RedditComposeMessageDataProvider.qc Reddit compose message action

/*  RedditComposeMessageDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for composing a message
public class RedditComposeMessageRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "to": {
                "type": StringType,
                "display_name": "To",
                "short_desc": "The recipient username",
                "desc": "The username of the message recipient",
                "required": True,
                "example_value": "spez",
            },
            "subject": {
                "type": StringType,
                "display_name": "Subject",
                "short_desc": "The message subject",
                "desc": "The subject line of the message",
                "required": True,
                "example_value": "Hello",
            },
            "text": {
                "type": StringType,
                "display_name": "Text",
                "short_desc": "The message body",
                "desc": "The body text of the message (markdown formatted)",
                "required": True,
                "example_value": "Hi, I wanted to reach out about...",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Reddit compose message action data provider
public class RedditComposeMessageDataProvider inherits RedditDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "compose",
            "desc": "Send a private message to a Reddit user",
            "type": "RedditComposeMessageDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static AbstractDataProviderType ResponseType = AbstractDataProviderTypeMap."hash";
    }

    constructor(RedditRestClient::RedditRestClient rest) : RedditDataProviderBase(rest) {
    }

    string getName() {
        return "compose";
    }

    *string getDesc() {
        return "Send a private message to a Reddit user";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new RedditComposeMessageRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req.api_type = "json";
        return doPost("api/compose", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace RedditDataProvider
