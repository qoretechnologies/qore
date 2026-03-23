# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file RedditUpdatePrefsDataProvider.qc Reddit update preferences action

/*  RedditUpdatePrefsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating preferences
public class RedditUpdatePrefsRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "over_18": {
                "type": SoftBoolOrNothingType,
                "display_name": "Over 18",
                "short_desc": "Show NSFW content",
                "desc": "Whether to show NSFW (over 18) content",
            },
            "email_messages": {
                "type": SoftBoolOrNothingType,
                "display_name": "Email Messages",
                "short_desc": "Send email notifications",
                "desc": "Whether to send email notifications for messages",
            },
            "show_link_flair": {
                "type": SoftBoolOrNothingType,
                "display_name": "Show Link Flair",
                "short_desc": "Show link flair",
                "desc": "Whether to show link flair",
            },
            "default_comment_sort": {
                "type": StringOrNothingType,
                "display_name": "Default Comment Sort",
                "short_desc": "Default comment sort order",
                "desc": "Default comment sort order",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "best", "display_name": "Best"},
                    <AllowedValueInfo>{"value": "top", "display_name": "Top"},
                    <AllowedValueInfo>{"value": "new", "display_name": "New"},
                    <AllowedValueInfo>{"value": "controversial", "display_name": "Controversial"},
                    <AllowedValueInfo>{"value": "old", "display_name": "Old"},
                    <AllowedValueInfo>{"value": "qa", "display_name": "Q&A"},
                ),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Reddit update preferences action data provider
public class RedditUpdatePrefsDataProvider inherits RedditDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-prefs",
            "desc": "Update the authenticated user's preferences",
            "type": "RedditUpdatePrefsDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static AbstractDataProviderType ResponseType = AbstractDataProviderTypeMap."hash";
    }

    constructor(RedditRestClient::RedditRestClientIo rest) : RedditDataProviderBase(rest) {
    }

    string getName() {
        return "update-prefs";
    }

    *string getDesc() {
        return "Update the authenticated user's preferences";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new RedditUpdatePrefsRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPatch("api/v1/me/prefs", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace RedditDataProvider
