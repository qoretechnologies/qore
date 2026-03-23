# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file RedditFlairDataProvider.qc Reddit flair data provider

/*  RedditFlairDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Reddit flair data provider
public class RedditFlairDataProvider inherits RedditDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "flair",
            "desc": "Reddit flair data provider for listing flair templates",
            "type": "RedditFlairDataProvider",
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
                "short_desc": "The subreddit to get flairs from",
                "type": AbstractDataProviderType::get(SoftStringType),
                "desc": "The subreddit display name to get flair templates from",
                "required": True,
            },
            "type": <DataProviderOptionInfo>{
                "display_name": "Flair Type",
                "short_desc": "Type of flair (link or user)",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The type of flair to retrieve. Default: `link`",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "link", "display_name": "Link Flair"},
                    <AllowedValueInfo>{"value": "user", "display_name": "User Flair"},
                ),
            },
        };

        #! Child provider map
        const ChildMap = {
            "set-flair": Class::forName("RedditSetFlairDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(RedditRestClient::RedditRestClientIo rest) : RedditDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "flair";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Reddit flair data provider for listing flair templates";
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
        return new RedditFlairDataType();
    }

    #! Returns an iterator for flair templates
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (!search_options.subreddit) {
            throw "REDDIT-ERROR", "subreddit is required to list flair templates";
        }

        string flair_type = search_options.type ?? "link";
        string path = "r/" + search_options.subreddit + "/api/" + flair_type + "_flair_v2";

        # Flair endpoints return a flat list, not a Listing
        hash<auto> response = rest.restGet(path);
        list<hash<auto>> results = ();
        if (response.body.typeCode() == NT_LIST) {
            foreach auto item in (response.body) {
                if (item.typeCode() == NT_HASH) {
                    results += item;
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
