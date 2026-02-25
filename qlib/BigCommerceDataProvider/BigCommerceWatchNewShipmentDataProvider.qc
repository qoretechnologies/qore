# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceWatchNewShipmentDataProvider.qc watch provider for new BigCommerce shipments

/*  BigCommerceWatchNewShipmentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Watch provider that triggers when a new shipment is created in BigCommerce
/** Uses the V2 Orders API to find recently modified orders, then fetches shipments
    for each order and filters by date_created to find new shipments.
*/
public class BigCommerceWatchNewShipmentDataProvider inherits BigCommerceWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_SHIPMENT_CREATED,
            "desc": "Triggers when a new shipment is created in BigCommerce",
            "type": "BigCommerceWatchNewShipmentDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(BigCommerceRestClient::BigCommerceRestClient rest, *hash<auto> options)
            : BigCommerceWatchDataProviderBase(rest, options) {
    }

    #! Returns the provider name
    string getName() {
        return EVENT_SHIPMENT_CREATED;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Triggers when a new shipment is created in BigCommerce";
    }

    #! Fetches new shipment records by checking recently modified orders
    /** Gets orders modified since the last check, then fetches shipments for each
        order and filters to only those created after the since date.
    */
    private list<hash<auto>> fetchRecords(date since) {
        # First get recently modified orders (which may have new shipments)
        hash<auto> params = {
            "sort": "date_modified:desc",
            "min_date_modified": formatDateTimeV2(since),
            "limit": string(DefaultPageSize),
        };
        auto orders_response = doGetV2("orders", params);

        list<hash<auto>> results = ();
        list<auto> orders;
        if (orders_response.typeCode() == NT_LIST) {
            orders = orders_response;
        } else {
            return ();
        }

        foreach hash<auto> order in (orders) {
            try {
                auto shipments = doGetV2(sprintf("orders/%d/shipments", order.id));
                if (shipments.typeCode() == NT_LIST) {
                    foreach hash<auto> shipment in (shipments) {
                        # Filter shipments by date_created > since
                        *string ts = shipment.date_created;
                        if (ts) {
                            date shipment_date = date(ts);
                            if (shipment_date > since) {
                                results += shipment;
                            }
                        }
                    }
                }
            } catch (hash<ExceptionInfo> ex) {
                # Skip orders where shipment fetch fails (e.g., 404)
                log(LoggerLevel::DEBUG, "Error fetching shipments for order %d: %s: %s",
                    order.id, ex.err, ex.desc);
            }
        }

        return results;
    }

    #! Returns the event name
    private string getEventName() {
        return EVENT_SHIPMENT_CREATED;
    }

    #! Returns the record data type
    private AbstractDataProviderType getRecordDataType() {
        return new BigCommerceShipmentDataType();
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns fake example data for documentation/testing
    private hash<auto> getFakeExampleData() {
        return {
            "id": 50,
            "order_id": 100,
            "tracking_number": "1Z999AA10123456784",
            "shipping_method": "USPS Priority",
            "shipping_provider": "usps",
            "date_created": "Thu, 30 Jan 2026 10:30:00 +0000",
        };
    }
}

} # namespace BigCommerceDataProvider
