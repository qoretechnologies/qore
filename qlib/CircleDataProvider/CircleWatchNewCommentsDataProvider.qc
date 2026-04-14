# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CircleWatchNewCommentsDataProvider.qc Circle watch new comments data provider

/*  CircleWatchNewCommentsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Circle watch new comments data provider
public class CircleWatchNewCommentsDataProvider inherits CircleWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_COMMENT,
            "desc": "Fires when a new comment is posted in Circle",
            "type": "CircleWatchNewCommentsDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : CircleWatchDataProviderBase(rest, options) {
    }

    string getName() {
        return EVENT_NEW_COMMENT;
    }

    private list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages("comments", "records");
    }

    private string getEventName() {
        return EVENT_NEW_COMMENT;
    }

    private AbstractDataProviderType getRecordDataType() {
        return new CircleCommentDataType();
    }

    private hash<auto> getFakeExampleData() {
        return {
            "id": 67890,
            "body": "<p>Great post, thanks for sharing!</p>",
            "post_id": 12345,
            "user_id": 2,
            "user_name": "Jane Smith",
            "likes_count": 1,
            "created_at": "2026-01-30T11:00:00Z",
            "updated_at": "2026-01-30T11:00:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CircleDataProvider
