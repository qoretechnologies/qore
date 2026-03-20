# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file RedditPostsDataProvider.qc Reddit posts collection data provider

/*  RedditPostsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Reddit posts collection data provider
public class RedditPostsDataProvider inherits RedditDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "posts",
            "desc": "Reddit posts data provider for listing and searching posts",
            "type": "RedditPostsDataProvider",
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
                "display_name": "Subreddit",
                "short_desc": "The subreddit to list posts from",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The subreddit display name to list posts from (required for listing, "
                    "optional for searching)",
            },
            "sort": <DataProviderOptionInfo>{
                "display_name": "Sort",
                "short_desc": "Sort order for posts",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Sort order for posts. Default: `hot`",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "hot", "display_name": "Hot"},
                    <AllowedValueInfo>{"value": "new", "display_name": "New"},
                    <AllowedValueInfo>{"value": "top", "display_name": "Top"},
                    <AllowedValueInfo>{"value": "rising", "display_name": "Rising"},
                ),
            },
            "q": <DataProviderOptionInfo>{
                "display_name": "Search Query",
                "short_desc": "Search posts by query",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Search query to find posts",
            },
            "t": <DataProviderOptionInfo>{
                "display_name": "Time Range",
                "short_desc": "Time range for top/search",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Time range for `top` sort or search",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "hour", "display_name": "Past Hour"},
                    <AllowedValueInfo>{"value": "day", "display_name": "Past 24 Hours"},
                    <AllowedValueInfo>{"value": "week", "display_name": "Past Week"},
                    <AllowedValueInfo>{"value": "month", "display_name": "Past Month"},
                    <AllowedValueInfo>{"value": "year", "display_name": "Past Year"},
                    <AllowedValueInfo>{"value": "all", "display_name": "All Time"},
                ),
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };

        #! Child provider map
        const ChildMap = {
            "create": Class::forName("RedditCreatePostDataProvider"),
            "edit": Class::forName("RedditEditTextDataProvider"),
            "delete": Class::forName("RedditDeleteThingDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(RedditRestClient::RedditRestClient rest) : RedditDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "posts";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Reddit posts data provider for listing and searching posts";
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
        return new RedditPostDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # Search posts
        if (search_options.q) {
            hash<auto> params = {
                "q": search_options.q,
                "type": "link",
            };
            if (search_options.t) {
                params.t = search_options.t;
            }
            if (search_options.sort) {
                params.sort = search_options.sort;
            }
            string path;
            if (search_options.subreddit) {
                path = "r/" + search_options.subreddit + "/search";
                params.restrict_sr = "on";
            } else {
                path = "search";
            }
            list<hash<auto>> records = fetchAllPages(path, params, search_options.limit);
            return new DefaultRecordIterator(new ListHashIterator(records),
                where_cond, search_options);
        }

        # List posts from subreddit
        if (!search_options.subreddit) {
            throw "REDDIT-ERROR", "subreddit is required when not searching (provide 'q' or "
                "'subreddit' option)";
        }
        string sort = search_options.sort ?? "hot";
        hash<auto> params;
        if (search_options.t) {
            params.t = search_options.t;
        }
        list<hash<auto>> records = fetchAllPages(
            "r/" + search_options.subreddit + "/" + sort, params, search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(records),
            where_cond, search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace RedditDataProvider
