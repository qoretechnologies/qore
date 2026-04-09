# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CircleWatchMemberTaggedDataProvider.qc Circle watch member tagged data provider

/*  CircleWatchMemberTaggedDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Circle watch member tagged data provider
public class CircleWatchMemberTaggedDataProvider inherits CircleWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_MEMBER_TAGGED,
            "desc": "Fires when a member is tagged in Circle",
            "type": "CircleWatchMemberTaggedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : CircleWatchDataProviderBase(rest, options) {
    }

    string getName() {
        return EVENT_MEMBER_TAGGED;
    }

    private list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages("community_members", "records");
    }

    private string getEventName() {
        return EVENT_MEMBER_TAGGED;
    }

    private AbstractDataProviderType getRecordDataType() {
        return new CircleMemberDataType();
    }

    private hash<auto> getFakeExampleData() {
        return {
            "id": 500,
            "name": "John Doe",
            "email": "john@example.com",
            "headline": "Developer",
            "bio": "Software engineer and community member",
            "posts_count": 10,
            "comments_count": 25,
            "community_id": 100,
            "created_at": "2026-01-15T08:00:00Z",
            "updated_at": "2026-01-30T10:30:00Z",
        };
    }

    private string getTimestampField() {
        return "updated_at";
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CircleDataProvider
