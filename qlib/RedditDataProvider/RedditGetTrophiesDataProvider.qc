# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file RedditGetTrophiesDataProvider.qc Reddit get trophies action

/*  RedditGetTrophiesDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Reddit get trophies action data provider
public class RedditGetTrophiesDataProvider inherits RedditDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-trophies",
            "desc": "Get the authenticated user's trophies",
            "type": "RedditGetTrophiesDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static RedditTrophyDataType ResponseType();
    }

    constructor(RedditRestClient::RedditRestClient rest) : RedditDataProviderBase(rest) {
    }

    string getName() {
        return "get-trophies";
    }

    *string getDesc() {
        return "Get the authenticated user's trophies";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return AbstractDataProviderTypeMap."nothing";
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new RedditTrophyDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> response = doGet("api/v1/me/trophies");
        # Extract trophy list from Reddit's response:
        # {"kind": "TrophyList", "data": {"trophies": [{"kind": "t6", "data": {...}}, ...]}}
        list<hash<auto>> result = ();
        if (response.data.trophies.typeCode() == NT_LIST) {
            foreach auto trophy in (response.data.trophies) {
                if (trophy.data) {
                    result += trophy.data;
                }
            }
        }
        return result;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace RedditDataProvider
