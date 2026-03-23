# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file RedditUsersDataProvider.qc Reddit users data provider

/*  RedditUsersDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Reddit users data provider
public class RedditUsersDataProvider inherits RedditDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "users",
            "desc": "Reddit users data provider for looking up user info and activity",
            "type": "RedditUsersDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "username": <DataProviderOptionInfo>{
                "display_name": "Username",
                "short_desc": "The Reddit username to look up",
                "type": AbstractDataProviderType::get(SoftStringType),
                "desc": "The Reddit username to look up",
                "required": True,
            },
            "type": <DataProviderOptionInfo>{
                "display_name": "Content Type",
                "short_desc": "Type of user content to retrieve",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Type of content to retrieve. Default: `about`",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "about", "display_name": "User Info"},
                    <AllowedValueInfo>{"value": "submitted", "display_name": "User's Posts"},
                    <AllowedValueInfo>{"value": "comments", "display_name": "User's Comments"},
                ),
            },
            "sort": <DataProviderOptionInfo>{
                "display_name": "Sort",
                "short_desc": "Sort order for content",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Sort order for user content",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "hot", "display_name": "Hot"},
                    <AllowedValueInfo>{"value": "new", "display_name": "New"},
                    <AllowedValueInfo>{"value": "top", "display_name": "Top"},
                    <AllowedValueInfo>{"value": "controversial", "display_name": "Controversial"},
                ),
            },
            "t": <DataProviderOptionInfo>{
                "display_name": "Time Range",
                "short_desc": "Time range for top sort",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Time range for `top` sort",
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
    }

    #! Creates the object with a REST client
    constructor(RedditRestClient::RedditRestClientIo rest) : RedditDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "users";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Reddit users data provider for looking up user info and activity";
    }

    #! Returns the record type
    private *AbstractDataProviderType getRecordTypeImpl() {
        return new RedditAccountDataType();
    }

    #! Returns an iterator for user records
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (!search_options.username) {
            throw "REDDIT-ERROR", "username is required to look up a user";
        }

        string content_type = search_options.type ?? "about";

        if (content_type == "about") {
            hash<auto> result = doGet("user/" + search_options.username + "/about");
            hash<auto> data = result.data ?? result;
            return new DefaultRecordIterator(new ListHashIterator((data,)),
                where_cond, search_options);
        }

        # submitted or comments
        hash<auto> params;
        if (search_options.sort) {
            params.sort = search_options.sort;
        }
        if (search_options.t) {
            params.t = search_options.t;
        }
        list<hash<auto>> records = fetchAllPages(
            "user/" + search_options.username + "/" + content_type, params,
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
