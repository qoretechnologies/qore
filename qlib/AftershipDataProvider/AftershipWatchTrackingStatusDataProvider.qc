# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AftershipDataProvider module definition

/** AftershipWatchTrackingStatusDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the AftershipDataProvider module
public namespace AftershipDataProvider {
#! Watch provider for tracking status changes
public class AftershipWatchTrackingStatusDataProvider inherits AftershipWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_TRACKING_STATUS_CHANGED,
            "type": "AftershipWatchTrackingStatusDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        #! Last known status per tracking ID for change detection
        hash<string, string> known_statuses;
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : AftershipWatchDataProviderBase(rest, options) {
    }

    constructor(*hash<auto> options) : AftershipWatchDataProviderBase(options) {
    }

    string getName() { return ProviderInfo.name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Fetches recently updated trackings
    private list<hash<auto>> fetchRecords(date since) {
        string since_str = since.format("YYYY-MM-DDTHH:mm:ssZ");
        string path = "trackings?updated_at_min=" + encode_url(since_str, True)
            + "&limit=" + MaxPageSize;
        hash<auto> response = doTrackingCommand("GET", path);
        list<hash<auto>> trackings = response.body.data.trackings ?? ();

        # filter to only trackings with changed status
        list<hash<auto>> changed = ();
        foreach hash<auto> tracking in (trackings) {
            string tid = tracking.id ?? "";
            string status = tracking.tag ?? "";
            *string last_status = known_statuses{tid};
            if (!exists last_status || last_status != status) {
                known_statuses{tid} = status;
                if (exists last_status) {
                    # only fire event if we previously knew the status (avoid initial load noise)
                    changed += tracking;
                }
            }
        }

        # prune known_statuses if too large
        if (known_statuses.size() > MaxSeenIds) {
            known_statuses = {};
        }

        return changed;
    }

    #! Returns fake example data for tracking status changes
    private hash<auto> getFakeExampleData() {
        return {
            "id": "abcdef1234567890abcdef1234567890",
            "tracking_number": "1Z999AA10123456784",
            "slug": "ups",
            "tag": "Delivered",
            "subtag": "Delivered_001",
            "subtag_message": "Package delivered",
            "title": "Order #12345",
            "customer_name": "Jane Smith",
            "order_id": "ORD-12345",
            "order_number": "#12345",
            "origin_country_region": "USA",
            "destination_country_region": "USA",
            "destination_city": "San Francisco",
            "destination_state": "CA",
            "expected_delivery": "2026-03-30",
            "signed_by": "Jane Smith",
            "transit_time": 3,
            "shipment_package_count": 1,
            "courier_tracking_link": "https://www.ups.com/track?tracknum=1Z999AA10123456784",
            "created_at": "2026-03-27T10:00:00Z",
            "updated_at": "2026-03-30T14:30:00Z",
        };
    }

    #! Returns the event name
    private string getEventName() {
        return EVENT_TRACKING_STATUS_CHANGED;
    }

    #! Returns the record data type
    private AbstractDataProviderType getRecordDataType() {
        return AftershipTrackingDataType;
    }
}
}
