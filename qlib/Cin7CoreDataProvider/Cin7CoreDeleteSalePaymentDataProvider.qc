# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreDeleteSalePaymentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for deleting a sale payment
public class Cin7CoreDeleteSalePaymentDataProvider inherits Cin7CoreDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-sale-payment",
            "desc": "Deletes a payment from a sale in Cin7Core",
            "type": "Cin7CoreDeleteSalePaymentDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static Cin7CoreDeleteSalePaymentRequestDataType RequestType();

        #! Response type
        static HashDataType ResponseType();
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
        return "delete-sale-payment";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Deletes a payment from a sale in Cin7Core";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.SaleID.val(), "SaleID is required");
        @assert(req.PaymentID.val(), "PaymentID is required");

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: deleting payment %y from sale %y",
            req.PaymentID, req.SaleID));
        doDelete("/SalePayment", {"SaleID": req.SaleID, "PaymentID": req.PaymentID});
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: deleted payment"));
        return {"success": True, "SaleID": req.SaleID, "PaymentID": req.PaymentID};
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

#! Request data type for deleting a sale payment
public class Cin7CoreDeleteSalePaymentRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "SaleID": {
                "type": StringType,
                "display_name": "Sale ID",
                "short_desc": "The sale ID",
                "desc": "The unique identifier (GUID) of the sale",
                "required": True,
                "ref_data": "sales",
            },
            "PaymentID": {
                "type": StringType,
                "display_name": "Payment ID",
                "short_desc": "The payment ID",
                "desc": "The unique identifier (GUID) of the payment to delete",
                "required": True,
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreDeleteSalePaymentRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace Cin7CoreDataProvider
