# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file RedditGetKarmaDataProvider.qc Reddit get karma action

/*  RedditGetKarmaDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Reddit get karma action data provider
public class RedditGetKarmaDataProvider inherits RedditDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-karma",
            "desc": "Get the authenticated user's karma breakdown by subreddit",
            "type": "RedditGetKarmaDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static RedditKarmaDataType ResponseType();
    }

    constructor(RedditRestClient::RedditRestClientIo rest) : RedditDataProviderBase(rest) {
    }

    string getName() {
        return "get-karma";
    }

    *string getDesc() {
        return "Get the authenticated user's karma breakdown by subreddit";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return AbstractDataProviderTypeMap."nothing";
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new RedditKarmaDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> response = doGet("api/v1/me/karma");
        # Extract karma list from Reddit's response: {"kind": "KarmaList", "data": [...]}
        if (response.data.typeCode() == NT_LIST) {
            return response.data;
        }
        return ();
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace RedditDataProvider
