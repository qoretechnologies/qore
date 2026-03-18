# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file RedditSubredditsDataProvider.qc Reddit subreddits collection data provider

/*  RedditSubredditsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Reddit subreddits collection data provider
public class RedditSubredditsDataProvider inherits RedditDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "subreddits",
            "desc": "Reddit subreddits data provider for listing and searching subreddits",
            "type": "RedditSubredditsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "subreddit": <DataProviderOptionInfo>{
                "display_name": "Subreddit Name",
                "short_desc": "Get a specific subreddit by name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Get a specific subreddit by its display name",
            },
            "q": <DataProviderOptionInfo>{
                "display_name": "Search Query",
                "short_desc": "Search subreddits by query",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Search query to find subreddits",
            },
            "mine": <DataProviderOptionInfo>{
                "display_name": "My Subreddits",
                "short_desc": "List subscribed subreddits",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "Set to `True` to list the authenticated user's subscribed subreddits",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
            "sort": <DataProviderOptionInfo>{
                "display_name": "Sort",
                "short_desc": "Sort order for search results",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Sort order (relevance, activity)",
            },
        };

        #! Child provider map
        const ChildMap = {
            "subscribe": Class::forName("RedditSubscribeDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(RedditRestClient::RedditRestClient rest) : RedditDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "subreddits";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Reddit subreddits data provider for listing and searching subreddits";
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

    #! Returns the record type
    private *AbstractDataProviderType getRecordTypeImpl() {
        return new RedditSubredditDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # Get specific subreddit by name
        if (search_options.subreddit) {
            hash<auto> result = doGet("r/" + search_options.subreddit + "/about");
            hash<auto> data = result.data ?? result;
            return new DefaultRecordIterator(new ListHashIterator((data,)),
                where_cond, search_options);
        }

        # Search subreddits by query
        if (search_options.q) {
            hash<auto> params = {"q": search_options.q};
            if (search_options.sort) {
                params.sort = search_options.sort;
            }
            list<hash<auto>> records = fetchAllPages("subreddits/search", params,
                search_options.limit);
            return new DefaultRecordIterator(new ListHashIterator(records),
                where_cond, search_options);
        }

        # Default: list subscribed subreddits
        list<hash<auto>> records = fetchAllPages("subreddits/mine/subscriber", NOTHING,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(records),
            where_cond, search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace RedditDataProvider
