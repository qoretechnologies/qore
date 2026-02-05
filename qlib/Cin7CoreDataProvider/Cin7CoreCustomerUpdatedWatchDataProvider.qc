# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreCustomerUpdatedWatchDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the Cin7CoreDataProvider module
public namespace Cin7CoreDataProvider {

#! Watch provider for customer updates
public class Cin7CoreCustomerUpdatedWatchDataProvider inherits Cin7CoreWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_CUSTOMER_UPDATED,
            "desc": "Triggers when a customer is created or updated",
            "type": "Cin7CoreCustomerUpdatedWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(Cin7CoreRestClient::Cin7CoreRestClient rest, *hash<auto> options)
            : Cin7CoreWatchDataProviderBase(rest, options) {
    }

    #! Creates the object with a connection
    constructor(Cin7CoreRestClient::Cin7CoreRestConnection conn, *hash<auto> options)
            : Cin7CoreWatchDataProviderBase(conn, options) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : Cin7CoreWatchDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return EVENT_CUSTOMER_UPDATED;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Triggers when a customer is created or updated";
    }

    #! Poll for customer updates
    list<hash<auto>> pollOnce() {
        hash<auto> params = {
            "ModifiedSince": since,
            "Page": 1,
            "Limit": MaxPageSize,
        };

        @debug(log(LoggerLevel::DEBUG, "pollOnce: polling for customer updates since %y", since));
        hash<auto> response = doGet("/Customer", params);
        return response.CustomerList ?? ();
    }

    #! Get the unique ID from a customer record
    string getRecordId(hash<auto> record) {
        return record.ID ?? "";
    }

    #! Get the event name
    string getEventName() {
        return EVENT_CUSTOMER_UPDATED;
    }

    #! Get the event data type
    AbstractDataProviderType getEventDataType() {
        return new Cin7CoreCustomerDataType();
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns resource-specific fake example data for customers
    hash<auto> getFakeExampleData() {
        return {
            "ID": "12345678-1234-1234-1234-123456789012",
            "Name": "Acme Corporation",
            "Status": "Active",
            "Currency": "USD",
            "PaymentTerm": "Net 30",
            "AccountReceivable": "200",
            "RevenueAccount": "400",
            "TaxRule": "Standard Tax",
            "Discount": 5.0,
            "Comments": "Preferred customer",
            "TaxNumber": "123-456-789",
            "CreditLimit": 10000.00,
            "IsOnCreditHold": False,
            "Tags": "wholesale,priority",
            "LastModifiedOn": "2026-01-15T10:30:00Z",
        };
    }
}

} # namespace Cin7CoreDataProvider
