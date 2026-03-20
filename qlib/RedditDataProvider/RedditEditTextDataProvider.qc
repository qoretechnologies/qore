# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file RedditEditTextDataProvider.qc Reddit edit text action

/*  RedditEditTextDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for editing text
public class RedditEditTextRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "thing_id": {
                "type": StringType,
                "display_name": "Thing ID",
                "short_desc": "The fullname of the thing to edit",
                "desc": "The fullname (t1_ for comment, t3_ for self post) of the thing to edit",
                "required": True,
                "example_value": "t3_abc123",
            },
            "text": {
                "type": StringType,
                "display_name": "Text",
                "short_desc": "The new text content",
                "desc": "The new body text (markdown formatted)",
                "required": True,
                "example_value": "Updated text content",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Reddit edit text action data provider
public class RedditEditTextDataProvider inherits RedditDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "edit",
            "desc": "Edit the text of a self post or comment",
            "type": "RedditEditTextDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static AbstractDataProviderType ResponseType = AbstractDataProviderTypeMap."hash";
    }

    constructor(RedditRestClient::RedditRestClient rest) : RedditDataProviderBase(rest) {
    }

    string getName() {
        return "edit";
    }

    *string getDesc() {
        return "Edit the text of a self post or comment";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new RedditEditTextRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req.api_type = "json";
        return doPost("api/editusertext", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace RedditDataProvider
