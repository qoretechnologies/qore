# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file RedditWatchDataProviders.qc Reddit watch/trigger data providers

/*  RedditWatchDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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

# ========== Watch New Posts ==========

#! Watch for new posts in a subreddit
public class RedditWatchNewPostsDataProvider inherits RedditWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_POST,
            "desc": "Watch for new posts in a subreddit",
            "type": "RedditWatchNewPostsDataProvider",
            "supports_observable": True,
            "constructor_options": WatchNewPostsConstructorOptions,
        };

        #! Constructor options
        const WatchNewPostsConstructorOptions = WatchConstructorOptions + {
            "subreddit": <DataProviderOptionInfo>{
                "display_name": "Subreddit",
                "short_desc": "The subreddit to watch for new posts",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The display name of the subreddit to watch for new posts",
            },
        };
    }

    private {
        #! The subreddit to watch
        *string subreddit;
    }

    constructor(RedditRestClient::RedditRestClient rest, *hash<auto> options)
            : RedditWatchDataProviderBase(rest, options) {
        if (options.subreddit) {
            subreddit = options.subreddit;
        }
    }

    string getName() { return EVENT_NEW_POST; }
    *string getDesc() { return subreddit ? "Watch for new posts in subreddit r/" + subreddit : "Watch for new posts in a subreddit"; }

    private string getSubreddit() {
        if (!subreddit) {
            *hash<auto> ctx = DataProvider::DataProviderDataContextHelper::getHash();
            subreddit = ctx.subreddit;
        }
        if (!subreddit) {
            throw "REDDIT-ERROR", "subreddit is required for watching new posts; "
                "provide it as a constructor option or via data context";
        }
        return subreddit;
    }

    private list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages("r/" + getSubreddit() + "/new", NOTHING, DefaultPageSize);
    }

    private string getEventName() { return EVENT_NEW_POST; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
    private AbstractDataProviderType getRecordDataType() { return new RedditPostDataType(); }

    private hash<auto> getFakeExampleData() {
        return {
            "id": "abc123",
            "name": "t3_abc123",
            "title": "Example Post Title",
            "selftext": "This is an example self post body with some text content.",
            "author": "example_user",
            "subreddit": subreddit ?? "test",
            "score": 42,
            "upvote_ratio": 0.95,
            "num_comments": 7,
            "created_utc": float(now_us().getEpochSeconds()),
            "url": "https://www.reddit.com/r/test/comments/abc123/example_post_title/",
            "permalink": "/r/test/comments/abc123/example_post_title/",
            "is_self": True,
            "over_18": False,
            "stickied": False,
            "locked": False,
        };
    }
}

# ========== Watch New Comments ==========

#! Watch for new comments on a post
public class RedditWatchNewCommentsDataProvider inherits RedditWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_COMMENT,
            "desc": "Watch for new comments on a post",
            "type": "RedditWatchNewCommentsDataProvider",
            "supports_observable": True,
            "constructor_options": WatchNewCommentsConstructorOptions,
        };

        #! Constructor options
        const WatchNewCommentsConstructorOptions = WatchConstructorOptions + {
            "article": <DataProviderOptionInfo>{
                "display_name": "Post ID",
                "short_desc": "The post ID to watch for comments",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The post ID (base36, without t3_ prefix) to watch for new comments",
            },
        };
    }

    private {
        #! The post ID to watch
        *string article;
    }

    constructor(RedditRestClient::RedditRestClient rest, *hash<auto> options)
            : RedditWatchDataProviderBase(rest, options) {
        if (options.article) {
            article = options.article;
        }
    }

    string getName() { return EVENT_NEW_COMMENT; }
    *string getDesc() { return article ? "Watch for new comments on post " + article : "Watch for new comments on a post"; }

    private string getArticle() {
        if (!article) {
            *hash<auto> ctx = DataProvider::DataProviderDataContextHelper::getHash();
            article = ctx.article;
        }
        if (!article) {
            throw "REDDIT-ERROR", "article (post ID) is required for watching new comments; "
                "provide it as a constructor option or via data context";
        }
        return article;
    }

    private list<hash<auto>> fetchRecords(date since) {
        hash<auto> response = doGet("comments/" + getArticle(), {"sort": "new"});
        list<hash<auto>> results = ();
        # GET /comments/{article} returns a list of two Listings
        auto comment_listing;
        if (response.typeCode() == NT_LIST && response.size() > 1) {
            comment_listing = response[1];
        } else {
            comment_listing = response;
        }
        *list<auto> children = comment_listing.data.children;
        if (children) {
            foreach auto child in (children) {
                if (child.kind == "t1" && child.data) {
                    results += child.data;
                }
            }
        }
        return results;
    }

    private string getEventName() { return EVENT_NEW_COMMENT; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
    private AbstractDataProviderType getRecordDataType() { return new RedditCommentDataType(); }

    private hash<auto> getFakeExampleData() {
        return {
            "id": "xyz789",
            "name": "t1_xyz789",
            "body": "Example comment text",
            "author": "example_user",
            "subreddit": "test",
            "score": 5,
            "created_utc": float(now_us().getEpochSeconds()),
            "parent_id": "t3_" + (article ?? "abc123"),
            "link_id": "t3_" + (article ?? "abc123"),
            "permalink": "/r/test/comments/abc123/example_post/xyz789/",
            "is_submitter": False,
            "stickied": False,
        };
    }
}

# ========== Watch New Messages ==========

#! Watch for new unread messages
public class RedditWatchNewMessagesDataProvider inherits RedditWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_MESSAGE,
            "desc": "Watch for new unread messages",
            "type": "RedditWatchNewMessagesDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(RedditRestClient::RedditRestClient rest, *hash<auto> options)
            : RedditWatchDataProviderBase(rest, options) {
    }

    string getName() { return EVENT_NEW_MESSAGE; }
    *string getDesc() { return "Watch for new unread messages"; }

    private list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages("message/unread", NOTHING, DefaultPageSize);
    }

    private string getEventName() { return EVENT_NEW_MESSAGE; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
    private AbstractDataProviderType getRecordDataType() { return new RedditMessageDataType(); }

    private hash<auto> getFakeExampleData() {
        return {
            "id": "msg123",
            "name": "t4_msg123",
            "subject": "Example Message",
            "body": "Example message body",
            "author": "example_user",
            "dest": "recipient_user",
            "created_utc": float(now_us().getEpochSeconds()),
            "new": True,
            "was_comment": False,
            "first_message_name": "t4_msg100",
        };
    }
}

} # namespace RedditDataProvider
