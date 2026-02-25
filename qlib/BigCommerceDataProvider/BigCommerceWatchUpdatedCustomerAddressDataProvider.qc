# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceWatchUpdatedCustomerAddressDataProvider.qc watch provider for updated BigCommerce customer addresses

/*  BigCommerceWatchUpdatedCustomerAddressDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Watch provider that triggers when a customer address is updated in BigCommerce
/** The BigCommerce customer addresses API does not support date filtering, so this provider
    tracks a hash of address content fields and detects changes by including the content hash
    in the record ID. When address content changes, the composite ID changes, causing the
    base class deduplication to treat it as a new event.
*/
public class BigCommerceWatchUpdatedCustomerAddressDataProvider inherits BigCommerceWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_CUSTOMER_ADDRESS_UPDATED,
            "desc": "Triggers when a customer address is updated in BigCommerce",
            "type": "BigCommerceWatchUpdatedCustomerAddressDataProvider",
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
        return EVENT_CUSTOMER_ADDRESS_UPDATED;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Triggers when a customer address is updated in BigCommerce";
    }

    #! Fetches customer address records
    /** No date filter available on this endpoint; relies on content-hash deduplication */
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

    #! Returns a composite record ID that includes a hash of modifiable fields
    /** This ensures that when address content changes, the ID changes, causing the
        base class to treat the updated address as a new event
    */
    private string getRecordId(hash<auto> record) {
        # Build a string from mutable address fields to detect changes
        string content = string(record.first_name ?? "") + "|"
            + string(record.last_name ?? "") + "|"
            + string(record.company ?? "") + "|"
            + string(record.address1 ?? "") + "|"
            + string(record.address2 ?? "") + "|"
            + string(record.city ?? "") + "|"
            + string(record.state_or_province ?? "") + "|"
            + string(record.postal_code ?? "") + "|"
            + string(record.country_code ?? "") + "|"
            + string(record.phone ?? "");
        string content_hash = content.toMD5().toHex();
        return sprintf("%s-%s", string(record.id), content_hash);
    }

    #! Returns the timestamp from a record
    /** Customer addresses do not have date fields, so return NOTHING to rely on
        content-hash-based deduplication only
    */
    private *date getRecordTimestamp(hash<auto> record) {
        return NOTHING;
    }

    #! Returns the event name
    private string getEventName() {
        return EVENT_CUSTOMER_ADDRESS_UPDATED;
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
            "address1": "456 Oak Ave",
            "city": "Austin",
            "state_or_province": "TX",
            "postal_code": "78702",
            "country_code": "US",
        };
    }
}

} # namespace BigCommerceDataProvider
