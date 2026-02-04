# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreCreateCustomerDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for creating a customer
public class Cin7CoreCreateCustomerDataProvider inherits Cin7CoreDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-customer",
            "desc": "Creates a new customer in Cin7Core",
            "type": "Cin7CoreCreateCustomerDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static Cin7CoreCreateCustomerRequestDataType RequestType();

        #! Response type
        static Cin7CoreCustomerDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(Cin7CoreRestClient::Cin7CoreRestClient rest) : Cin7CoreDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(Cin7CoreRestClient::Cin7CoreRestConnection conn) : Cin7CoreDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : Cin7CoreDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "create-customer";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Creates a new customer in Cin7Core";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.Name.val(), "Name is required");

        hash<auto> body = {};

        # Required fields
        body.Name = req.Name;

        # Optional fields
        if (exists req.Status) body.Status = req.Status;
        if (exists req.Currency) body.Currency = req.Currency;
        if (exists req.PaymentTerm) body.PaymentTerm = req.PaymentTerm;
        if (exists req.AccountReceivable) body.AccountReceivable = req.AccountReceivable;
        if (exists req.RevenueAccount) body.RevenueAccount = req.RevenueAccount;
        if (exists req.TaxRule) body.TaxRule = req.TaxRule;
        if (exists req.Discount) body.Discount = req.Discount;
        if (exists req.Comments) body.Comments = req.Comments;
        if (exists req.TaxNumber) body.TaxNumber = req.TaxNumber;
        if (exists req.CreditLimit) body.CreditLimit = req.CreditLimit;
        if (exists req.Contacts) body.Contacts = req.Contacts;
        if (exists req.Addresses) body.Addresses = req.Addresses;
        if (exists req.Tags) body.Tags = req.Tags;

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: creating customer %y", body.Name));
        hash<auto> response = doPost("/Customer", body);
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: created customer %y", response.ID));
        return response;
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace Cin7CoreDataProvider
