# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CircleWatchMemberAcceptedDataProvider.qc Circle watch member accepted invitation

/*  CircleWatchMemberAcceptedDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Circle watch member accepted invitation data provider
public class CircleWatchMemberAcceptedDataProvider inherits CircleWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_MEMBER_ACCEPTED,
            "desc": "Fires when a member accepts an invitation to Circle",
            "type": "CircleWatchMemberAcceptedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : CircleWatchDataProviderBase(rest, options) {
    }

    string getName() {
        return EVENT_MEMBER_ACCEPTED;
    }

    private list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages("community_members", "records");
    }

    private string getEventName() {
        return EVENT_MEMBER_ACCEPTED;
    }

    private AbstractDataProviderType getRecordDataType() {
        return new CircleMemberDataType();
    }

    private hash<auto> getFakeExampleData() {
        return {
            "id": 500,
            "name": "Jane Smith",
            "email": "jane@example.com",
            "headline": "Community Manager",
            "bio": "Passionate about building communities",
            "posts_count": 5,
            "comments_count": 12,
            "community_id": 100,
            "created_at": "2026-01-30T10:30:00Z",
            "updated_at": "2026-01-30T10:30:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CircleDataProvider
