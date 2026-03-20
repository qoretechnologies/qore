# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file RedditCommentsDataProvider.qc Reddit comments collection data provider

/*  RedditCommentsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Reddit comments collection data provider
public class RedditCommentsDataProvider inherits RedditDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "comments",
            "desc": "Reddit comments data provider for listing comments on a post",
            "type": "RedditCommentsDataProvider",
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
            "article": <DataProviderOptionInfo>{
                "display_name": "Post ID",
                "short_desc": "The post ID to get comments for",
                "type": AbstractDataProviderType::get(SoftStringType),
                "desc": "The post ID (base36, without t3_ prefix) to retrieve comments for",
                "required": True,
            },
            "sort": <DataProviderOptionInfo>{
                "display_name": "Sort",
                "short_desc": "Comment sort order",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Sort order for comments",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "best", "display_name": "Best"},
                    <AllowedValueInfo>{"value": "top", "display_name": "Top"},
                    <AllowedValueInfo>{"value": "new", "display_name": "New"},
                    <AllowedValueInfo>{"value": "controversial", "display_name": "Controversial"},
                    <AllowedValueInfo>{"value": "old", "display_name": "Old"},
                    <AllowedValueInfo>{"value": "qa", "display_name": "Q&A"},
                ),
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of comments to return",
            },
        };

        #! Child provider map
        const ChildMap = {
            "create": Class::forName("RedditCreateCommentDataProvider"),
            "edit": Class::forName("RedditEditTextDataProvider"),
            "delete": Class::forName("RedditDeleteThingDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(RedditRestClient::RedditRestClient rest) : RedditDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "comments";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Reddit comments data provider for listing comments on a post";
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
        return new RedditCommentDataType();
    }

    #! Returns an iterator for comments on a post
    /** Reddit's GET /comments/{article} returns an array of two Listings:
        - Index 0: the post itself
        - Index 1: the comments
    */
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (!search_options.article) {
            throw "REDDIT-ERROR", "article (post ID) is required to list comments";
        }

        hash<auto> params;
        if (search_options.sort) {
            params.sort = search_options.sort;
        }
        if (search_options.limit) {
            params.limit = search_options.limit;
        }

        # GET /comments/{article} returns a list of two Listings
        hash<auto> response = doGet("comments/" + search_options.article, params);

        # The response is actually a list; the comments are in index [1]
        list<hash<auto>> results = ();
        auto comment_listing;
        if (response.typeCode() == NT_LIST && response.size() > 1) {
            comment_listing = response[1];
        } else {
            comment_listing = response;
        }

        # Unwrap the Listing children
        *list<auto> children = comment_listing.data.children;
        if (children) {
            foreach auto child in (children) {
                if (child.kind == "t1" && child.data) {
                    results += child.data;
                }
            }
        }

        return new DefaultRecordIterator(new ListHashIterator(results),
            where_cond, search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace RedditDataProvider
