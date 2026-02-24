# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceWatchNewCustomerAddressDataProvider.qc watch provider for new BigCommerce customer addresses

/*  BigCommerceWatchNewCustomerAddressDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Watch provider that triggers when a new customer address is created in BigCommerce
/** The BigCommerce customer addresses API does not support date filtering, so this provider
    relies on the seen_ids deduplication mechanism in the base class to detect new addresses.
*/
public class BigCommerceWatchNewCustomerAddressDataProvider inherits BigCommerceWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_CUSTOMER_ADDRESS_CREATED,
            "desc": "Triggers when a new customer address is created in BigCommerce",
            "type": "BigCommerceWatchNewCustomerAddressDataProvider",
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
        return EVENT_CUSTOMER_ADDRESS_CREATED;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Triggers when a new customer address is created in BigCommerce";
    }

    #! Fetches customer address records
    /** No date filter available on this endpoint; relies on seen_ids deduplication */
    private list<hash<auto>> fetchRecords(date since) {
        hash<auto> params = {
            "limit": string(DefaultPageSize),
        };
        hash<auto> response = doGetV3("customers/addresses", params);
        if (response.data.typeCode() == NT_LIST) {
            return response.data;
        }
        return ();
    }

    #! Returns the timestamp from a record
    /** Customer addresses do not have date fields, so return NOTHING to rely on
        ID-based deduplication only
    */
    private *date getRecordTimestamp(hash<auto> record) {
        return NOTHING;
    }

    #! Returns the event name
    private string getEventName() {
        return EVENT_CUSTOMER_ADDRESS_CREATED;
    }

    #! Returns the record data type
    private AbstractDataProviderType getRecordDataType() {
        return new BigCommerceCustomerAddressDataType();
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns fake example data for documentation/testing
    private hash<auto> getFakeExampleData() {
        return {
            "id": 1001,
            "customer_id": 1,
            "first_name": "John",
            "last_name": "Doe",
            "address1": "123 Main St",
            "city": "Austin",
            "state_or_province": "TX",
            "postal_code": "78701",
            "country_code": "US",
        };
    }
}

} # namespace BigCommerceDataProvider
