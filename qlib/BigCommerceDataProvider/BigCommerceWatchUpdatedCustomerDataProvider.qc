# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceWatchUpdatedCustomerDataProvider.qc watch provider for updated BigCommerce customers

/*  BigCommerceWatchUpdatedCustomerDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Watch provider that triggers when a customer is updated in BigCommerce
public class BigCommerceWatchUpdatedCustomerDataProvider inherits BigCommerceWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_CUSTOMER_UPDATED,
            "desc": "Triggers when a customer is updated in BigCommerce",
            "type": "BigCommerceWatchUpdatedCustomerDataProvider",
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
        return EVENT_CUSTOMER_UPDATED;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Triggers when a customer is updated in BigCommerce";
    }

    #! Fetches customer records modified since the given date
    private list<hash<auto>> fetchRecords(date since) {
        hash<auto> params = {
            "sort": "date_modified:desc",
            "date_modified:min": formatDateTimeV3(since),
            "limit": string(DefaultPageSize),
        };
        hash<auto> response = doGetV3("customers", params);
        if (response.data.typeCode() == NT_LIST) {
            return response.data;
        }
        return ();
    }

    #! Returns the timestamp from a record, preferring date_modified for update detection
    private *date getRecordTimestamp(hash<auto> record) {
        *string ts = record.date_modified ?? record.date_created;
        if (ts) {
            return date(ts);
        }
    }

    #! Returns the event name
    private string getEventName() {
        return EVENT_CUSTOMER_UPDATED;
    }

    #! Returns the record data type
    private AbstractDataProviderType getRecordDataType() {
        return new BigCommerceCustomerDataType();
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns fake example data for documentation/testing
    private hash<auto> getFakeExampleData() {
        return {
            "id": 12345,
            "first_name": "John",
            "last_name": "Doe",
            "email": "john@example.com",
            "company": "Acme Corp",
            "phone": "+1-555-0100",
            "date_created": "2026-01-15T10:30:00+00:00",
            "date_modified": "2026-01-30T14:45:00+00:00",
        };
    }
}

} # namespace BigCommerceDataProvider
