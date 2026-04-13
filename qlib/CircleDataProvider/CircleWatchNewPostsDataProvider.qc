# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CircleWatchNewPostsDataProvider.qc Circle watch new posts data provider

/*  CircleWatchNewPostsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CircleDataProvider module
public namespace CircleDataProvider {

#! Circle watch new posts data provider
public class CircleWatchNewPostsDataProvider inherits CircleWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_POST,
            "desc": "Fires when a new post is created in Circle",
            "type": "CircleWatchNewPostsDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : CircleWatchDataProviderBase(rest, options) {
    }

    string getName() {
        return EVENT_NEW_POST;
    }

    private list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages("posts", "records", {"status": "published"});
    }

    private string getEventName() {
        return EVENT_NEW_POST;
    }

    private AbstractDataProviderType getRecordDataType() {
        return new CirclePostDataType();
    }

    private hash<auto> getFakeExampleData() {
        return {
            "id": 12345,
            "name": "Welcome Post",
            "body": "<p>Welcome to the community!</p>",
            "slug": "welcome-post",
            "status": "published",
            "space_id": 100,
            "user_id": 1,
            "comments_count": 3,
            "likes_count": 10,
            "is_pinned": False,
            "is_comments_enabled": True,
            "is_liking_enabled": True,
            "created_at": "2026-01-30T10:30:00Z",
            "updated_at": "2026-01-30T10:30:00Z",
            "published_at": "2026-01-30T10:30:00Z",
            "url": "https://community.example.com/c/general/welcome-post",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CircleDataProvider
