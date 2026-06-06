# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenSearchDataProvider search action utilities

/** OpenSearchSearchActionUtil.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the OpenSearchDataProvider module
public namespace OpenSearchDataProvider {
#! Common top-level search request fields for OpenSearch search actions
public const SearchTopLevelFields = {
    "from": {
        "type": IntOrNothingType,
        "display_name": "From",
        "short_desc": "Starting document offset",
        "desc": "Starting document offset for search results",
        "example_value": 0,
    },
    "size": {
        "type": IntOrNothingType,
        "display_name": "Result Size",
        "short_desc": "Number of hits to return",
        "desc": "The number of search hits to return",
        "example_value": 10,
    },
    "_source": {
        "type": AutoType,
        "display_name": "Source Fields",
        "short_desc": "Source field selection",
        "desc": "Optional `_source` selection for returned documents; accepts OpenSearch-compatible boolean, "
            "string, list, or hash values",
        "example_value": {"excludes": ("embedding",)},
    },
    "track_total_hits": {
        "type": AutoType,
        "display_name": "Track Total Hits",
        "short_desc": "Total hit tracking",
        "desc": "Optional total-hit tracking control; accepts OpenSearch-compatible boolean or integer values",
        "example_value": True,
    },
    "search_pipeline": {
        "type": StringOrNothingType,
        "display_name": "Search Pipeline",
        "short_desc": "Search pipeline name",
        "desc": "Optional search pipeline to use when executing the search",
        "example_value": "semantic-search-pipeline",
    },
    "preference": {
        "type": StringOrNothingType,
        "display_name": "Preference",
        "short_desc": "Shard replica preference",
        "desc": "Which shard replicas to execute the search on",
        "example_value": "_local",
    },
    "routing": {
        "type": StringOrNothingType,
        "display_name": "Routing",
        "short_desc": "Custom routing value",
        "desc": "Custom routing value to limit search to specific shards",
        "example_value": "user123",
    },
};

#! Common helpers for OpenSearch search actions
public class OpenSearchSearchActionUtil {
    private {
        #! Fields that are emitted in the JSON search body instead of the URI query string
        const SearchBodyArgs = ("from", "size", "_source", "track_total_hits");
    }

    #! Moves the given key from the source hash to the target hash if it exists
    /** @param target target hash reference
        @param source source hash reference
        @param key key to move
    */
    static public moveIfExists(reference<hash<auto>> target, reference<hash<auto>> source, string key) {
        if (exists source{key}) {
            target{key} = remove source{key};
        }
    }

    #! Moves common top-level search body options from a request hash into a search body hash
    /** @param body search body hash reference
        @param req request hash reference
    */
    static public addTopLevelSearchOptions(reference<hash<auto>> body, reference<hash<auto>> req) {
        foreach string arg in (SearchBodyArgs) {
            moveIfExists(\body, \req, arg);
        }
    }
}
}
