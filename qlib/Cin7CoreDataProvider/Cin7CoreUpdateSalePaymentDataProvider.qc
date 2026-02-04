# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreUpdateSalePaymentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for updating a sale payment
public class Cin7CoreUpdateSalePaymentDataProvider inherits Cin7CoreDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-sale-payment",
            "desc": "Updates an existing sale payment in Cin7Core",
            "type": "Cin7CoreUpdateSalePaymentDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static Cin7CoreUpdateSalePaymentRequestDataType RequestType();

        #! Response type
        static Cin7CoreSalePaymentDataType ResponseType();
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
        return "update-sale-payment";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Updates an existing sale payment in Cin7Core";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.ID.val(), "ID is required");
        @assert(req.SaleID.val(), "SaleID is required");

        hash<auto> body = {
            "ID": req.ID,
            "SaleID": req.SaleID,
        };

        # Optional fields
        if (exists req.Amount) body.Amount = req.Amount;
        if (exists req.DatePaid) body.DatePaid = req.DatePaid;
        if (exists req.Account) body.Account = req.Account;
        if (exists req.PaymentMethod) body.PaymentMethod = req.PaymentMethod;
        if (exists req.Reference) body.Reference = req.Reference;

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: updating payment %y for sale %y", req.ID, req.SaleID));
        hash<auto> response = doPut("/SalePayment", body);
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: updated payment"));
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

#! Request data type for updating a sale payment
public class Cin7CoreUpdateSalePaymentRequestDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "ID": {
                "type": StringType,
                "display_name": "Payment ID",
                "short_desc": "The payment ID",
                "desc": "The unique identifier (GUID) of the payment to update",
                "required": True,
            },
            "SaleID": {
                "type": StringType,
                "display_name": "Sale ID",
                "short_desc": "The sale ID",
                "desc": "The unique identifier (GUID) of the sale",
                "required": True,
                "ref_data": "sales",
            },
            "Amount": {
                "type": FloatOrNothingType,
                "display_name": "Amount",
                "short_desc": "Payment amount",
                "desc": "The payment amount",
            },
            "DatePaid": {
                "type": DateOrNothingType,
                "display_name": "Date Paid",
                "short_desc": "Payment date",
                "desc": "The date the payment was made",
            },
            "Account": {
                "type": StringOrNothingType,
                "display_name": "Account",
                "short_desc": "Account name",
                "desc": "The account to record the payment",
            },
            "PaymentMethod": {
                "type": StringOrNothingType,
                "display_name": "Payment Method",
                "short_desc": "Payment method",
                "desc": "The method of payment (e.g., Cash, Credit Card, Bank Transfer)",
            },
            "Reference": {
                "type": StringOrNothingType,
                "display_name": "Reference",
                "short_desc": "Payment reference",
                "desc": "Reference number or note for the payment",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreUpdateSalePaymentRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace Cin7CoreDataProvider
