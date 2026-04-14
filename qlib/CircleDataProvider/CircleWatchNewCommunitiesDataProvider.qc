# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CircleWatchNewCommunitiesDataProvider.qc Circle watch new communities data provider

/*  CircleWatchNewCommunitiesDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Community data type for watch events
public class CircleCommunityDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "Community ID",
                "short_desc": "Unique identifier for the community",
                "desc": "The unique identifier for the community",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Community name",
                "desc": "The name of the community",
            },
            "slug": {
                "type": StringOrNothingType,
                "display_name": "Slug",
                "short_desc": "URL-friendly community slug",
                "desc": "The URL-friendly slug for the community",
            },
            "locale": {
                "type": StringOrNothingType,
                "display_name": "Locale",
                "short_desc": "Community locale",
                "desc": "The locale of the community",
            },
            "is_private": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Private",
                "short_desc": "Whether the community is private",
                "desc": "Whether the community is private",
            },
            "white_label": {
                "type": SoftBoolOrNothingType,
                "display_name": "White Label",
                "short_desc": "Whether white labeling is enabled",
                "desc": "Whether white labeling is enabled for the community",
            },
            "weekly_digest_enabled": {
                "type": SoftBoolOrNothingType,
                "display_name": "Weekly Digest Enabled",
                "short_desc": "Whether weekly digest is enabled",
                "desc": "Whether the weekly digest email is enabled for the community",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "The date and time the community was created",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "The date and time the community was last updated",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Circle watch new communities data provider
public class CircleWatchNewCommunitiesDataProvider inherits CircleWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_COMMUNITY,
            "desc": "Fires when a new community is created in Circle",
            "type": "CircleWatchNewCommunitiesDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : CircleWatchDataProviderBase(rest, options) {
    }

    string getName() {
        return EVENT_NEW_COMMUNITY;
    }

    private list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages("communities", "records");
    }

    private string getEventName() {
        return EVENT_NEW_COMMUNITY;
    }

    private AbstractDataProviderType getRecordDataType() {
        return new CircleCommunityDataType();
    }

    private hash<auto> getFakeExampleData() {
        return {
            "id": 100,
            "name": "My Community",
            "slug": "my-community",
            "locale": "en",
            "is_private": False,
            "white_label": False,
            "weekly_digest_enabled": True,
            "created_at": "2026-01-30T10:30:00Z",
            "updated_at": "2026-01-30T10:30:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CircleDataProvider
