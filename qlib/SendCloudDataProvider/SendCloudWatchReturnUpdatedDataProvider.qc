# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudWatchReturnUpdatedDataProvider.qc SendCloud return-updated event provider

/*  SendCloudWatchReturnUpdatedDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Watch provider: emits an event when a return is updated
public class SendCloudWatchReturnUpdatedDataProvider inherits SendCloudWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_RETURN_UPDATED,
            "type": "SendCloudWatchReturnUpdatedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
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

    #! Fetches returns updated since the given timestamp
    private list<hash<auto>> fetchRecords(date since) {
        string since_str = SendCloudDataProviderBase::formatDateTime(since);
        hash<auto> params = {
            "created_at_from": since_str,
            "page_size": MaxPageSize,
        };
        list<hash<auto>> return_list;
        try {
            return_list = fetchAllCursorPages("returns", "returns", params);
        } catch (hash<ExceptionInfo> ex) {
            error("SendCloudWatchReturnUpdated: error fetching returns: %s: %s", ex.err, ex.desc);
            return ();
        }
        # filter to returns with updated_at > since
        list<hash<auto>> changed = ();
        foreach hash<auto> r in (return_list) {
            *string ts_str = r.updated_at ?? r.created_at;
            if (ts_str.val()) {
                date ts = date(ts_str);
                if (ts > since) {
                    changed += r;
                }
            } else {
                changed += r;
            }
        }
        return changed;
    }

    private string getEventName() {
        return EVENT_RETURN_UPDATED;
    }

    private AbstractDataProviderType getRecordDataType() {
        return new SendCloudReturnDataType();
    }

    #! Returns fake example data for a return update
    private hash<auto> getFakeExampleData() {
        return {
            "id": 98765,
            "status": "announced",
            "created_at": "2026-04-20T10:00:00+00:00",
            "updated_at": "2026-04-22T12:00:00+00:00",
            "reason": "damaged",
            "tracking_number": "3SABCD7654321",
            "products": (
                {
                    "description": "Blue T-shirt",
                    "quantity": 1,
                    "value": "25.00",
                    "sku": "TSHIRT-BLUE-M",
                    "return_reason": 1,
                },
            ),
            "refund": {
                "refund_type": "money",
                "total_refund": "25.00",
            },
        };
    }
}

} # namespace SendCloudDataProvider
