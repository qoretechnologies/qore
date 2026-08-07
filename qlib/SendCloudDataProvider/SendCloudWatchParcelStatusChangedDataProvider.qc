# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudWatchParcelStatusChangedDataProvider.qc SendCloud parcel-status-changed event provider

/*  SendCloudWatchParcelStatusChangedDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SendCloudDataProvider module
public namespace SendCloudDataProvider {

#! Watch provider: emits an event when a parcel's status changes
public class SendCloudWatchParcelStatusChangedDataProvider inherits SendCloudWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_PARCEL_STATUS_CHANGED,
            "type": "SendCloudWatchParcelStatusChangedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        #! Last known status per parcel ID for change detection
        hash<string, string> known_statuses;
    }

    #! Creates the object with a REST client and options
    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : SendCloudWatchDataProviderBase(rest, options) {
    }

    string getName() {
        return ProviderInfo.name;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Fetches parcels updated since the given timestamp and filters to status-changed records.
    /** Uses the v2 \c /parcels endpoint's documented \c updated_after query parameter (an earlier
        revision used \c updated_at_from, which SendCloud silently dropped — verified against
        the v2 OpenAPI spec, the only update-time filter is \c updated_after).
    */
    private list<hash<auto>> fetchRecords(date since) {
        string since_str = SendCloudDataProviderBase::formatDateTime(since);
        hash<auto> params = {
            "updated_after": since_str,
            "page_size": MaxPageSize,
        };
        list<hash<auto>> parcels;
        try {
            parcels = fetchAllCursorPages("parcels", "parcels", params);
        } catch (hash<ExceptionInfo> ex) {
            LoggerWrapper::error(sprintf("SendCloudWatchParcelStatusChanged: error fetching parcels: %s: %s",
                ex.err, ex.desc));
            return ();
        }

        list<hash<auto>> changed = ();
        foreach hash<auto> p in (parcels) {
            string pid = string(p.id);
            string status = string(p.status.id ?? p.status.message ?? "");
            *string last_status = known_statuses{pid};
            if (!exists last_status || last_status != status) {
                known_statuses{pid} = status;
                if (exists last_status) {
                    # only fire event if we previously knew the status (avoid initial-load noise)
                    changed += p;
                }
            }
        }

        # prune known_statuses if too large
        if (known_statuses.size() > MaxSeenIds) {
            known_statuses = {};
        }

        return changed;
    }

    private string getEventName() {
        return EVENT_PARCEL_STATUS_CHANGED;
    }

    private AbstractDataProviderType getRecordDataType() {
        return new SendCloudParcelDataType();
    }

    #! Use date_updated as the timestamp field for SendCloud parcels
    private string getTimestampField() {
        return "date_updated";
    }

    #! Returns fake example data for a parcel status change
    private hash<auto> getFakeExampleData() {
        return {
            "id": 12345678,
            "name": "John Doe",
            "company_name": "",
            "address": "Stadhuisplein 10",
            "city": "Eindhoven",
            "postal_code": "5611EM",
            "country": {
                "iso_2": "NL",
                "iso_3": "NLD",
                "name": "Netherlands",
            },
            "telephone": "+31201234567",
            "email": "john@example.com",
            "tracking_number": "3SABCD1234567",
            "tracking_url": "https://tracking.sendcloud.sc/forward?carrier=postnl&code=3SABCD1234567",
            "status": {
                "id": 11,
                "message": "Delivered",
            },
            "date_created": "2026-04-22T10:00:00+00:00",
            "date_updated": "2026-04-22T15:30:00+00:00",
            "date_announced": "2026-04-22T10:05:00+00:00",
        };
    }
}

} # namespace SendCloudDataProvider
