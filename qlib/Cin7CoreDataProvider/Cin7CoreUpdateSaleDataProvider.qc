# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreUpdateSaleDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for updating a sale/order
public class Cin7CoreUpdateSaleDataProvider inherits Cin7CoreDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-sale",
            "desc": "Updates an existing sale/order in Cin7Core",
            "type": "Cin7CoreUpdateSaleDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static Cin7CoreUpdateSaleRequestDataType RequestType();

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
        return "update-sale";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Updates an existing sale/order in Cin7Core";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.ID.val(), "ID is required");

        hash<auto> body = {};

        # Required fields
        body.ID = req.ID;

        # Optional fields - only include if provided
        if (exists req.Customer) body.Customer = req.Customer;
        if (exists req.SaleOrderDate) body.SaleOrderDate = Cin7CoreDataProviderBase::formatDate(req.SaleOrderDate);
        if (exists req.ShipBy) body.ShipBy = Cin7CoreDataProviderBase::formatDate(req.ShipBy);
        if (exists req.RequiredBy) body.RequiredBy = Cin7CoreDataProviderBase::formatDate(req.RequiredBy);
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

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: updating sale %y", body.ID));
        hash<auto> response = doPut("/Sale", body);
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: updated sale %y", response.ID));
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

#! Request data type for updating a sale
public class Cin7CoreUpdateSaleRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "ID": {
                "type": StringType,
                "display_name": "Sale ID",
                "short_desc": "The sale ID to update",
                "desc": "The unique identifier (GUID) of the sale to update",
                "required": True,
                "ref_data": "sales",
            },
            "Customer": {
                "type": StringOrNothingType,
                "display_name": "Customer",
                "short_desc": "Customer name",
                "desc": "The customer name for this sale",
            },
            "SaleOrderDate": {
                "type": DateOrNothingType,
                "display_name": "Sale Order Date",
                "short_desc": "Date of the order",
                "desc": "The date when the sale order was created",
            },
            "ShipBy": {
                "type": DateOrNothingType,
                "display_name": "Ship By",
                "short_desc": "Ship by date",
                "desc": "The date by which the order should be shipped",
            },
            "RequiredBy": {
                "type": DateOrNothingType,
                "display_name": "Required By",
                "short_desc": "Required by date",
                "desc": "The date by which the customer requires the order",
            },
            "Location": {
                "type": StringOrNothingType,
                "display_name": "Location",
                "short_desc": "Warehouse location",
                "desc": "The warehouse location for this sale",
                "ref_data": "locations",
            },
            "Status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Sale status",
                "desc": "The current status of the sale",
            },
            "Currency": {
                "type": StringOrNothingType,
                "display_name": "Currency",
                "short_desc": "Currency code",
                "desc": "The currency code for this sale (e.g., USD, EUR)",
            },
            "CurrencyRate": {
                "type": FloatOrNothingType,
                "display_name": "Currency Rate",
                "short_desc": "Exchange rate",
                "desc": "The currency exchange rate",
            },
            "TaxRule": {
                "type": StringOrNothingType,
                "display_name": "Tax Rule",
                "short_desc": "Tax rule",
                "desc": "The tax rule to apply",
            },
            "TaxCalculation": {
                "type": StringOrNothingType,
                "display_name": "Tax Calculation",
                "short_desc": "Tax calculation method",
                "desc": "How taxes should be calculated (Inclusive, Exclusive)",
            },
            "Terms": {
                "type": StringOrNothingType,
                "display_name": "Terms",
                "short_desc": "Payment terms",
                "desc": "Payment terms for this sale",
            },
            "PriceTier": {
                "type": StringOrNothingType,
                "display_name": "Price Tier",
                "short_desc": "Price tier",
                "desc": "The price tier for this sale",
            },
            "ShippingAddress": {
                "type": new Cin7CoreAddressDataType(AutoHashOrNothingType),
                "display_name": "Shipping Address",
                "short_desc": "Shipping address",
                "desc": "The shipping address for this sale",
            },
            "BillingAddress": {
                "type": new Cin7CoreAddressDataType(AutoHashOrNothingType),
                "display_name": "Billing Address",
                "short_desc": "Billing address",
                "desc": "The billing address for this sale",
            },
            "ShippingNotes": {
                "type": StringOrNothingType,
                "display_name": "Shipping Notes",
                "short_desc": "Shipping notes",
                "desc": "Notes for shipping",
            },
            "CustomerReference": {
                "type": StringOrNothingType,
                "display_name": "Customer Reference",
                "short_desc": "Customer reference",
                "desc": "Customer's reference number or PO number",
            },
            "Comments": {
                "type": StringOrNothingType,
                "display_name": "Comments",
                "short_desc": "Comments",
                "desc": "External comments for this sale",
            },
            "InternalComments": {
                "type": StringOrNothingType,
                "display_name": "Internal Comments",
                "short_desc": "Internal comments",
                "desc": "Internal notes not visible to customer",
            },
            "Carrier": {
                "type": StringOrNothingType,
                "display_name": "Carrier",
                "short_desc": "Shipping carrier",
                "desc": "The shipping carrier for this sale",
            },
            "SalesRepresentative": {
                "type": StringOrNothingType,
                "display_name": "Sales Representative",
                "short_desc": "Sales rep",
                "desc": "The sales representative for this sale",
            },
            "Lines": {
                "type": new ListDataType("Lines", new Cin7CoreSaleLineItemDataType(), True),
                "display_name": "Lines",
                "short_desc": "Line items",
                "desc": "The line items for this sale",
            },
            "AdditionalCharges": {
                "type": new ListDataType("AdditionalCharges", new Cin7CoreAdditionalChargeDataType(), True),
                "display_name": "Additional Charges",
                "short_desc": "Additional charges",
                "desc": "Additional charges for this sale",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreUpdateSaleRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace Cin7CoreDataProvider
