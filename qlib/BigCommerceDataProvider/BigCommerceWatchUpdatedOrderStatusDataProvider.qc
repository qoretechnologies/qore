# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceWatchUpdatedOrderStatusDataProvider.qc watch provider for BigCommerce order status changes

/*  BigCommerceWatchUpdatedOrderStatusDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BigCommerceDataProvider module
public namespace BigCommerceDataProvider {

#! Watch provider that triggers when an order's status changes in BigCommerce
/** Uses the V2 Orders API to poll recently modified orders and tracks the status_id
    for each order. Only fires an event when the status_id actually changes.
    The record ID includes the status_id so that the same order with a different status
    is treated as a new event by the base class deduplication.
*/
public class BigCommerceWatchUpdatedOrderStatusDataProvider inherits BigCommerceWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_ORDER_STATUS_UPDATED,
            "desc": "Triggers when an order's status changes in BigCommerce",
            "type": "BigCommerceWatchUpdatedOrderStatusDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    private {
        #! Tracks last known status_id for each order
        hash<string, int> seen_statuses;
    }

    #! Creates the object with a REST client
    constructor(BigCommerceRestClient::BigCommerceRestClient rest, *hash<auto> options)
            : BigCommerceWatchDataProviderBase(rest, options) {
    }

    #! Returns the provider name
    string getName() {
        return EVENT_ORDER_STATUS_UPDATED;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Triggers when an order's status changes in BigCommerce";
    }

    #! Fetches recently modified orders and filters to only those with status changes
    private list<hash<auto>> fetchRecords(date since) {
        hash<auto> params = {
            "sort": "date_modified:desc",
            "min_date_modified": formatDateTimeV2(since),
            "limit": string(DefaultPageSize),
        };
        auto response = doGetV2("orders", params);

        list<hash<auto>> results = ();
        list<auto> orders;
        if (response.typeCode() == NT_LIST) {
            orders = response;
        } else {
            return ();
        }

        foreach hash<auto> order in (orders) {
            string order_id = string(order.id);
            int current_status = order.status_id.toInt();

            # Check if we have seen this order before
            if (exists seen_statuses{order_id}) {
                # Only fire if status has changed
                if (seen_statuses{order_id} != current_status) {
                    results += order;
                }
            }
            # Update the tracked status
            seen_statuses{order_id} = current_status;
        }

        # Prune seen_statuses if it gets too large
        if (seen_statuses.size() > MaxSeenIds) {
            seen_statuses = {};
        }

        return results;
    }

    #! Returns a composite record ID that includes status_id
    /** This ensures that the same order with a different status is treated as a new event
    */
    private string getRecordId(hash<auto> record) {
        return sprintf("%s-status-%s", string(record.id), string(record.status_id));
    }

    #! Returns the timestamp from a record, preferring date_modified
    private *date getRecordTimestamp(hash<auto> record) {
        *string ts = record.date_modified ?? record.date_created;
        if (ts) {
            return date(ts);
        }
    }

    #! Returns the event name
    private string getEventName() {
        return EVENT_ORDER_STATUS_UPDATED;
    }

    #! Returns the record data type
    private AbstractDataProviderType getRecordDataType() {
        return new BigCommerceOrderDataType();
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns fake example data for documentation/testing
    private hash<auto> getFakeExampleData() {
        return {
            "id": 100,
            "status": "Shipped",
            "status_id": 2,
            "total_inc_tax": "100.00",
            "total_ex_tax": "90.00",
            "currency_code": "USD",
            "date_created": "Thu, 30 Jan 2026 10:30:00 +0000",
            "date_modified": "Fri, 31 Jan 2026 14:45:00 +0000",
        };
    }
}

} # namespace BigCommerceDataProvider
