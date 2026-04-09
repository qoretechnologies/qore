# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CircleWatchNewSpacesDataProvider.qc Circle watch new spaces data provider

/*  CircleWatchNewSpacesDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Space data type for watch events
public class CircleSpaceDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "Space ID",
                "short_desc": "Unique identifier for the space",
                "desc": "The unique identifier for the space",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Space name",
                "desc": "The name of the space",
            },
            "slug": {
                "type": StringOrNothingType,
                "display_name": "Slug",
                "short_desc": "URL-friendly slug",
                "desc": "The URL-friendly slug for the space",
            },
            "is_private": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Private",
                "short_desc": "Whether the space is private",
                "desc": "Whether the space is private",
            },
            "community_id": {
                "type": IntOrNothingType,
                "display_name": "Community ID",
                "short_desc": "Community the space belongs to",
                "desc": "The ID of the community this space belongs to",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "The date and time the space was created",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "The date and time the space was last updated",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Circle watch new spaces data provider
public class CircleWatchNewSpacesDataProvider inherits CircleWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_SPACE,
            "desc": "Fires when a new space is created in Circle",
            "type": "CircleWatchNewSpacesDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : CircleWatchDataProviderBase(rest, options) {
    }

    string getName() {
        return EVENT_NEW_SPACE;
    }

    private list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages("spaces", "records");
    }

    private string getEventName() {
        return EVENT_NEW_SPACE;
    }

    private AbstractDataProviderType getRecordDataType() {
        return new CircleSpaceDataType();
    }

    private hash<auto> getFakeExampleData() {
        return {
            "id": 200,
            "name": "General Discussion",
            "slug": "general-discussion",
            "is_private": False,
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
