# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquareWatchDataProviders.qc Square polling watch data providers

/*  SquareWatchDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace SquareDataProvider {

#! Watch for new customers in Square
public class SquareWatchCustomersDataProvider inherits SquareWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_CUSTOMER_CREATED,
            "desc": "Watch for new customers in Square",
            "type": "SquareWatchCustomersDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(SquareRestClient::SquareRestClient rest, *hash<auto> options)
            : SquareWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_CUSTOMER_CREATED; }

    #! Returns the provider description
    *string getDesc() { return "Watch for new customers in Square"; }

    #! Fetches new customers since the given date
    private list<hash<auto>> fetchRecords(date since) {
        string since_str = formatDateTimeSquare(since);
        hash<auto> body = {
            "query": {
                "filter": {
                    "created_at": {
                        "start_at": since_str,
                    },
                },
                "sort": {
                    "field": "CREATED_AT",
                    "order": "ASC",
                },
            },
            "limit": DefaultPageSize,
        };
        return fetchAllPagesPost("customers/search", "customers", body, 1000);
    }

    #! Returns the event name
    private string getEventName() { return EVENT_CUSTOMER_CREATED; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the data type for customer event records
    private AbstractDataProviderType getRecordDataType() { return new SquareCustomerDataType(); }

    #! Returns fake example data for customers
    private hash<auto> getFakeExampleData() {
        return SquareWatchDataProviderBase::getFakeExampleData() + {
            "given_name": "John",
            "family_name": "Smith",
            "email_address": "john.smith@example.com",
            "phone_number": "+1-555-123-4567",
            "company_name": "Acme Corporation",
            "version": 1,
        };
    }
}

#! Watch for new or updated orders in Square
public class SquareWatchOrdersDataProvider inherits SquareWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_ORDER_CREATED_OR_UPDATED,
            "desc": "Watch for new or updated orders in Square",
            "type": "SquareWatchOrdersDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    private {
        #! Cached location IDs for order search (required by the API)
        *list<string> location_ids;
    }

    #! Creates the object with a REST client
    constructor(SquareRestClient::SquareRestClient rest, *hash<auto> options)
            : SquareWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_ORDER_CREATED_OR_UPDATED; }

    #! Returns the provider description
    *string getDesc() { return "Watch for new or updated orders in Square"; }

    #! Fetches new/updated orders since the given date
    private list<hash<auto>> fetchRecords(date since) {
        # Get location IDs (required for orders search)
        if (!location_ids) {
            location_ids = getLocationIds();
            if (!location_ids) {
                return ();
            }
        }

        string since_str = formatDateTimeSquare(since);
        hash<auto> body = {
            "location_ids": location_ids,
            "query": {
                "filter": {
                    "date_time_filter": {
                        "updated_at": {
                            "start_at": since_str,
                        },
                    },
                },
                "sort": {
                    "sort_field": "UPDATED_AT",
                    "sort_order": "ASC",
                },
            },
            "limit": DefaultPageSize,
        };
        return fetchAllPagesPost("orders/search", "orders", body, 1000);
    }

    #! Returns the record timestamp (use updated_at for orders)
    private *date getRecordTimestamp(hash<auto> record) {
        *string ts = record.updated_at ?? record.created_at;
        if (ts) {
            return date(ts);
        }
    }

    #! Returns the event name
    private string getEventName() { return EVENT_ORDER_CREATED_OR_UPDATED; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the data type for order event records
    private AbstractDataProviderType getRecordDataType() { return new SquareOrderDataType(); }

    #! Returns fake example data for orders
    private hash<auto> getFakeExampleData() {
        return SquareWatchDataProviderBase::getFakeExampleData() + {
            "location_id": "LOCATION_ID_12345",
            "state": "OPEN",
            "total_money": {"amount": 1500, "currency": "USD"},
            "version": 1,
        };
    }
}

#! Watch for new or updated payments in Square
public class SquareWatchPaymentsDataProvider inherits SquareWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_PAYMENT_CREATED_OR_UPDATED,
            "desc": "Watch for new or updated payments in Square",
            "type": "SquareWatchPaymentsDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(SquareRestClient::SquareRestClient rest, *hash<auto> options)
            : SquareWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_PAYMENT_CREATED_OR_UPDATED; }

    #! Returns the provider description
    *string getDesc() { return "Watch for new or updated payments in Square"; }

    #! Fetches new/updated payments since the given date
    private list<hash<auto>> fetchRecords(date since) {
        string since_str = formatDateTimeSquare(since);
        hash<auto> params = {
            "begin_time": since_str,
            "sort_order": "ASC",
            "limit": DefaultPageSize,
        };
        return fetchAllPages("payments", "payments", params, 1000);
    }

    #! Returns the event name
    private string getEventName() { return EVENT_PAYMENT_CREATED_OR_UPDATED; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the data type for payment event records
    private AbstractDataProviderType getRecordDataType() { return new SquarePaymentDataType(); }

    #! Returns fake example data for payments
    private hash<auto> getFakeExampleData() {
        return SquareWatchDataProviderBase::getFakeExampleData() + {
            "amount_money": {"amount": 1000, "currency": "USD"},
            "total_money": {"amount": 1000, "currency": "USD"},
            "status": "COMPLETED",
            "source_type": "CARD",
            "location_id": "LOCATION_ID_12345",
        };
    }
}

#! Watch for new appointments (bookings) in Square
public class SquareNewAppointmentDataProvider inherits SquareWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_APPOINTMENT,
            "desc": "Watch for new appointments (bookings) in Square",
            "type": "SquareNewAppointmentDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(SquareRestClient::SquareRestClient rest, *hash<auto> options)
            : SquareWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_NEW_APPOINTMENT; }

    #! Returns the provider description
    *string getDesc() { return "Watch for new appointments (bookings) in Square"; }

    #! Fetches new bookings since the given date
    private list<hash<auto>> fetchRecords(date since) {
        string since_str = formatDateTimeSquare(since);
        hash<auto> params = {
            "start_at_min": since_str,
            "limit": DefaultPageSize,
        };
        return fetchAllPages("bookings", "bookings", params, 1000);
    }

    #! Returns the event name
    private string getEventName() { return EVENT_NEW_APPOINTMENT; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the data type for booking event records
    private AbstractDataProviderType getRecordDataType() { return new SquareBookingDataType(); }

    #! Returns fake example data for bookings
    private hash<auto> getFakeExampleData() {
        return SquareWatchDataProviderBase::getFakeExampleData() + {
            "status": "ACCEPTED",
            "start_at": "2026-02-15T14:00:00Z",
            "location_id": "LOCATION_ID_12345",
            "customer_id": "CUSTOMER_ID_12345",
            "appointment_segments": ({"team_member_id": "TM_12345", "duration_minutes": 60},),
        };
    }
}

#! Watch for new catalog items in Square
public class SquareNewCatalogItemDataProvider inherits SquareWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_CATALOG_ITEM,
            "desc": "Watch for new catalog items in Square",
            "type": "SquareNewCatalogItemDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(SquareRestClient::SquareRestClient rest, *hash<auto> options)
            : SquareWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_NEW_CATALOG_ITEM; }

    #! Returns the provider description
    *string getDesc() { return "Watch for new catalog items in Square"; }

    #! Fetches new catalog items since the given date
    private list<hash<auto>> fetchRecords(date since) {
        string since_str = formatDateTimeSquare(since);
        hash<auto> body = {
            "object_types": ("ITEM",),
            "begin_time": since_str,
            "limit": DefaultPageSize,
        };
        return fetchAllPagesPost("catalog/search", "objects", body, 1000);
    }

    #! Returns the event name
    private string getEventName() { return EVENT_NEW_CATALOG_ITEM; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the data type for catalog item event records
    private AbstractDataProviderType getRecordDataType() { return new SquareCatalogItemDataType(); }

    #! Returns fake example data for catalog items
    private hash<auto> getFakeExampleData() {
        return SquareWatchDataProviderBase::getFakeExampleData() + {
            "type": "ITEM",
            "is_deleted": False,
            "present_at_all_locations": True,
            "item_data": {
                "name": "Example Product",
                "description": "An example catalog item",
            },
            "version": 1,
        };
    }
}

#! Watch for new invoices in Square
public class SquareNewInvoiceDataProvider inherits SquareWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_INVOICE,
            "desc": "Watch for new invoices in Square",
            "type": "SquareNewInvoiceDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    private {
        #! Cached location ID for invoice listing (required by the API)
        *string cached_location_id;
    }

    #! Creates the object with a REST client
    constructor(SquareRestClient::SquareRestClient rest, *hash<auto> options)
            : SquareWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_NEW_INVOICE; }

    #! Returns the provider description
    *string getDesc() { return "Watch for new invoices in Square"; }

    #! Fetches new invoices since the given date (client-side filtering on created_at)
    private list<hash<auto>> fetchRecords(date since) {
        # Get location ID (required for invoices listing)
        if (!cached_location_id) {
            cached_location_id = getFirstLocationId();
        }

        hash<auto> params = {
            "location_id": cached_location_id,
            "limit": DefaultPageSize,
        };
        list<hash<auto>> all_invoices = fetchAllPages("invoices", "invoices", params, 1000);

        # Client-side filtering since Square's Invoices API doesn't support date filtering
        list<hash<auto>> new_invoices = ();
        foreach hash<auto> invoice in (all_invoices) {
            if (invoice.created_at) {
                date created = date(invoice.created_at);
                if (created > since) {
                    new_invoices += invoice;
                }
            }
        }
        return new_invoices;
    }

    #! Returns the event name
    private string getEventName() { return EVENT_NEW_INVOICE; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the data type for invoice event records
    private AbstractDataProviderType getRecordDataType() { return new SquareInvoiceDataType(); }

    #! Returns fake example data for invoices
    private hash<auto> getFakeExampleData() {
        return SquareWatchDataProviderBase::getFakeExampleData() + {
            "version": 1,
            "location_id": "LOCATION_ID_12345",
            "order_id": "ORDER_ID_12345",
            "invoice_number": "INV-0001",
            "status": "DRAFT",
            "payment_requests": ({"request_type": "BALANCE"},),
        };
    }
}

} # namespace SquareDataProvider
