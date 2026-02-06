# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnleashedDataProvider module definition

/*  UnleashedNewCustomerEventDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the UnleashedDataProvider module
public namespace UnleashedDataProvider {

#! Watch provider for new customer detection
public class UnleashedNewCustomerEventDataProvider inherits UnleashedWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_CUSTOMER,
            "desc": "Triggers when a new customer is created or modified",
            "type": "UnleashedNewCustomerEventDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(UnleashedRestClient::UnleashedRestClient rest, *hash<auto> options)
            : UnleashedWatchDataProviderBase(rest, options) {
    }

    #! Creates the object with a connection
    constructor(UnleashedRestClient::UnleashedRestConnection conn, *hash<auto> options)
            : UnleashedWatchDataProviderBase(conn, options) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : UnleashedWatchDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return EVENT_NEW_CUSTOMER;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Triggers when a new customer is created or modified";
    }

    #! Poll for customer updates
    list<hash<auto>> pollOnce() {
        hash<auto> params = {
            "modifiedSince": formatDate(since),
            "pageSize": MaxPageSize,
        };

        @debug(log(LoggerLevel::DEBUG, "pollOnce: polling for new customers since %y", since));
        auto response = doGet("/Customers", params);

        list<hash<auto>> customers;
        if (response.typeCode() == NT_HASH && response.Items.typeCode() == NT_LIST) {
            customers = response.Items;
        }

        @debug(log(LoggerLevel::DEBUG, "pollOnce: found %d new/modified customers", customers.size()));
        return customers;
    }

    #! Get the unique ID from a customer record
    string getRecordId(hash<auto> record) {
        return record.Guid ?? "";
    }

    #! Get the event name
    string getEventName() {
        return EVENT_NEW_CUSTOMER;
    }

    #! Get the event data type
    AbstractDataProviderType getEventDataType() {
        return new UnleashedCustomerDataType();
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns resource-specific fake example data for customers
    hash<auto> getFakeExampleData() {
        return {
            "Guid": "12345678-1234-1234-1234-123456789012",
            "CustomerCode": "CUST001",
            "CustomerName": "Acme Corporation",
            "Currency": {
                "Guid": "87654321-4321-4321-4321-210987654321",
                "CurrencyCode": "USD",
            },
            "CreditLimit": 10000.00,
            "DiscountRate": 5.0,
            "Taxable": True,
            "StopCredit": False,
            "Obsolete": False,
            "ContactFirstName": "John",
            "ContactLastName": "Doe",
            "Email": "john.doe@acme.com",
            "PhoneNumber": "+1-555-1234",
            "MobileNumber": "+1-555-5678",
            "Website": "https://www.acme.com",
            "CreatedOn": "2026-01-15T10:00:00Z",
            "LastModifiedOn": "2026-01-15T10:30:00Z",
        };
    }
}

} # namespace UnleashedDataProvider
