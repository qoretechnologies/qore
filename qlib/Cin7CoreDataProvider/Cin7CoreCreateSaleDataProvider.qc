# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreCreateSaleDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for creating a sale/order
public class Cin7CoreCreateSaleDataProvider inherits Cin7CoreDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-sale",
            "desc": "Creates a new sale/order in Cin7Core",
            "type": "Cin7CoreCreateSaleDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static Cin7CoreCreateSaleRequestDataType RequestType();

        #! Response type
        static Cin7CoreSaleDataType ResponseType();
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
        return "create-sale";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Creates a new sale/order in Cin7Core";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.Customer.val(), "Customer is required");

        hash<auto> body = {};

        # Required fields
        body.Customer = req.Customer;

        # Optional fields
        if (exists req.CustomerID) body.CustomerID = req.CustomerID;
        if (exists req.SaleOrderNumber) body.SaleOrderNumber = req.SaleOrderNumber;
        if (exists req.SaleOrderDate) body.SaleOrderDate = req.SaleOrderDate;
        if (exists req.ShipBy) body.ShipBy = req.ShipBy;
        if (exists req.RequiredBy) body.RequiredBy = req.RequiredBy;
        if (exists req.Location) body.Location = req.Location;
        if (exists req.Status) body.Status = req.Status;
        if (exists req.Currency) body.Currency = req.Currency;
        if (exists req.CurrencyRate) body.CurrencyRate = req.CurrencyRate;
        if (exists req.TaxRule) body.TaxRule = req.TaxRule;
        if (exists req.TaxCalculation) body.TaxCalculation = req.TaxCalculation;
        if (exists req.Terms) body.Terms = req.Terms;
        if (exists req.PriceTier) body.PriceTier = req.PriceTier;
        if (exists req.ShippingAddress) body.ShippingAddress = req.ShippingAddress;
        if (exists req.BillingAddress) body.BillingAddress = req.BillingAddress;
        if (exists req.ShippingNotes) body.ShippingNotes = req.ShippingNotes;
        if (exists req.CustomerReference) body.CustomerReference = req.CustomerReference;
        if (exists req.Comments) body.Comments = req.Comments;
        if (exists req.InternalComments) body.InternalComments = req.InternalComments;
        if (exists req.Carrier) body.Carrier = req.Carrier;
        if (exists req.SalesRepresentative) body.SalesRepresentative = req.SalesRepresentative;
        if (exists req.Lines) body.Lines = req.Lines;
        if (exists req.AdditionalCharges) body.AdditionalCharges = req.AdditionalCharges;
        if (exists req.SkipQuote) body.SkipQuote = req.SkipQuote;

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: creating sale for customer %y", body.Customer));
        hash<auto> response = doPost("/Sale", body);
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: created sale %y", response.ID));
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
