# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedditDataProvider module definition

/*  RedditDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the RedditDataProvider module
public namespace RedditDataProvider {

#! The main Reddit data provider class (root provider)
public class RedditDataProvider inherits RedditDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "reddit",
            "desc": "Reddit data provider for posts, comments, subreddits, messages, "
                "voting, and moderation",
            "type": "RedditDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
            "children_can_support_records": True,
            "children_can_support_observers": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Child provider map
        const ChildMap = {
            # Collection providers
            "account": Class::forName("RedditAccountDataProvider"),
            "subreddits": Class::forName("RedditSubredditsDataProvider"),
            "posts": Class::forName("RedditPostsDataProvider"),
            "comments": Class::forName("RedditCommentsDataProvider"),
            "messages": Class::forName("RedditMessagesDataProvider"),
            "users": Class::forName("RedditUsersDataProvider"),
            "flair": Class::forName("RedditFlairDataProvider"),
            # Action providers
            "vote": Class::forName("RedditVoteDataProvider"),
            "moderation": Class::forName("RedditModerationDataProvider"),
            "execute-api-call": Class::forName("RedditExecuteApiCallDataProvider"),
            # Events container
            "events": Class::forName("RedditEventsDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : RedditDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(RedditRestClient::RedditRestClientIo rest) : RedditDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(RedditRestClient::RedditRestConnection conn) : RedditDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "reddit";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Reddit data provider for posts, comments, subreddits, messages, "
            "voting, and moderation";
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace RedditDataProvider
