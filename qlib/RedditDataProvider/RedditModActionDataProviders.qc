# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file RedditModActionDataProviders.qc Reddit moderation action data providers

/*  RedditModActionDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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

# --- Common request types ---

#! Simple request with just an ID
public class RedditModIdRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Thing ID",
                "short_desc": "The fullname of the thing",
                "desc": "The fullname of the post or comment to perform the action on",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

# --- Approve ---

#! Reddit approve action data provider
public class RedditApproveDataProvider inherits RedditDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "approve",
            "desc": "Approve a post or comment",
            "type": "RedditApproveDataProvider",
            "supports_request": True,
            "constructor_options": BaseConstructorOptions,
        };
    }

    constructor(RedditRestClient::RedditRestClient rest) : RedditDataProviderBase(rest) {
    }

    string getName() { return "approve"; }
    *string getDesc() { return "Approve a post or comment"; }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new RedditModIdRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("api/approve", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Remove ---

#! Request type for removing
public class RedditRemoveRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Thing ID",
                "short_desc": "The fullname of the thing",
                "desc": "The fullname of the post or comment to remove",
                "required": True,
            },
            "spam": {
                "type": SoftBoolOrNothingType,
                "display_name": "Spam",
                "short_desc": "Mark as spam",
                "desc": "Whether to mark the item as spam. Default: False",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Reddit remove action data provider
public class RedditRemoveDataProvider inherits RedditDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "remove",
            "desc": "Remove a post or comment",
            "type": "RedditRemoveDataProvider",
            "supports_request": True,
            "constructor_options": BaseConstructorOptions,
        };
    }

    constructor(RedditRestClient::RedditRestClient rest) : RedditDataProviderBase(rest) {
    }

    string getName() { return "remove"; }
    *string getDesc() { return "Remove a post or comment"; }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new RedditRemoveRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("api/remove", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Distinguish ---

#! Request type for distinguishing
public class RedditDistinguishRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Thing ID",
                "short_desc": "The fullname of the thing",
                "desc": "The fullname of the post or comment to distinguish",
                "required": True,
            },
            "how": {
                "type": StringType,
                "display_name": "How",
                "short_desc": "Distinguished type",
                "desc": "How to distinguish: 'yes' for moderator, 'no' to remove, 'admin', 'special'",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Reddit distinguish action data provider
public class RedditDistinguishDataProvider inherits RedditDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "distinguish",
            "desc": "Distinguish a post or comment as a moderator",
            "type": "RedditDistinguishDataProvider",
            "supports_request": True,
            "constructor_options": BaseConstructorOptions,
        };
    }

    constructor(RedditRestClient::RedditRestClient rest) : RedditDataProviderBase(rest) {
    }

    string getName() { return "distinguish"; }
    *string getDesc() { return "Distinguish a post or comment as a moderator"; }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new RedditDistinguishRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req.api_type = "json";
        return doPost("api/distinguish", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Lock ---

#! Reddit lock action data provider
public class RedditLockDataProvider inherits RedditDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "lock",
            "desc": "Lock a post (prevent new comments)",
            "type": "RedditLockDataProvider",
            "supports_request": True,
            "constructor_options": BaseConstructorOptions,
        };
    }

    constructor(RedditRestClient::RedditRestClient rest) : RedditDataProviderBase(rest) {
    }

    string getName() { return "lock"; }
    *string getDesc() { return "Lock a post (prevent new comments)"; }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new RedditModIdRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("api/lock", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Unlock ---

#! Reddit unlock action data provider
public class RedditUnlockDataProvider inherits RedditDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "unlock",
            "desc": "Unlock a post (allow new comments)",
            "type": "RedditUnlockDataProvider",
            "supports_request": True,
            "constructor_options": BaseConstructorOptions,
        };
    }

    constructor(RedditRestClient::RedditRestClient rest) : RedditDataProviderBase(rest) {
    }

    string getName() { return "unlock"; }
    *string getDesc() { return "Unlock a post (allow new comments)"; }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new RedditModIdRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("api/unlock", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Sticky ---

#! Request type for sticky
public class RedditStickyRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Post ID",
                "short_desc": "The fullname of the post",
                "desc": "The fullname (t3_ prefix) of the post to sticky/unsticky",
                "required": True,
            },
            "state": {
                "type": SoftBoolType,
                "display_name": "Sticky State",
                "short_desc": "Whether to sticky or unsticky",
                "desc": "True to sticky, False to unsticky",
                "required": True,
            },
            "num": {
                "type": IntOrNothingType,
                "display_name": "Sticky Slot",
                "short_desc": "Sticky slot (1 or 2)",
                "desc": "The sticky slot number (1 or 2). Default: automatic",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Reddit sticky action data provider
public class RedditStickyDataProvider inherits RedditDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "sticky",
            "desc": "Sticky or unsticky a post in a subreddit",
            "type": "RedditStickyDataProvider",
            "supports_request": True,
            "constructor_options": BaseConstructorOptions,
        };
    }

    constructor(RedditRestClient::RedditRestClient rest) : RedditDataProviderBase(rest) {
    }

    string getName() { return "sticky"; }
    *string getDesc() { return "Sticky or unsticky a post in a subreddit"; }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new RedditStickyRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req.api_type = "json";
        return doPost("api/set_subreddit_sticky", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Mark NSFW ---

#! Reddit mark NSFW action data provider
public class RedditMarkNsfwDataProvider inherits RedditDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "mark-nsfw",
            "desc": "Mark a post as NSFW",
            "type": "RedditMarkNsfwDataProvider",
            "supports_request": True,
            "constructor_options": BaseConstructorOptions,
        };
    }

    constructor(RedditRestClient::RedditRestClient rest) : RedditDataProviderBase(rest) {
    }

    string getName() { return "mark-nsfw"; }
    *string getDesc() { return "Mark a post as NSFW"; }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new RedditModIdRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("api/marknsfw", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Unmark NSFW ---

#! Reddit unmark NSFW action data provider
public class RedditUnmarkNsfwDataProvider inherits RedditDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "unmark-nsfw",
            "desc": "Unmark a post as NSFW",
            "type": "RedditUnmarkNsfwDataProvider",
            "supports_request": True,
            "constructor_options": BaseConstructorOptions,
        };
    }

    constructor(RedditRestClient::RedditRestClient rest) : RedditDataProviderBase(rest) {
    }

    string getName() { return "unmark-nsfw"; }
    *string getDesc() { return "Unmark a post as NSFW"; }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new RedditModIdRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("api/unmarknsfw", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Mark Spoiler ---

#! Reddit mark spoiler action data provider
public class RedditSpoilerDataProvider inherits RedditDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "mark-spoiler",
            "desc": "Mark a post as a spoiler",
            "type": "RedditSpoilerDataProvider",
            "supports_request": True,
            "constructor_options": BaseConstructorOptions,
        };
    }

    constructor(RedditRestClient::RedditRestClient rest) : RedditDataProviderBase(rest) {
    }

    string getName() { return "mark-spoiler"; }
    *string getDesc() { return "Mark a post as a spoiler"; }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new RedditModIdRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("api/spoiler", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Unmark Spoiler ---

#! Reddit unmark spoiler action data provider
public class RedditUnspoilerDataProvider inherits RedditDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "unmark-spoiler",
            "desc": "Unmark a post as a spoiler",
            "type": "RedditUnspoilerDataProvider",
            "supports_request": True,
            "constructor_options": BaseConstructorOptions,
        };
    }

    constructor(RedditRestClient::RedditRestClient rest) : RedditDataProviderBase(rest) {
    }

    string getName() { return "unmark-spoiler"; }
    *string getDesc() { return "Unmark a post as a spoiler"; }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new RedditModIdRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("api/unspoiler", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace RedditDataProvider
