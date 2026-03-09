# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ChargebeeDataProvider module definition

/*  ChargebeeCreateInvoiceDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ChargebeeDataProvider module
public namespace ChargebeeDataProvider {

#! Request type for creating an invoice
public class ChargebeeCreateInvoiceRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "customer_id": {
                "type": StringType,
                "display_name": "Customer ID",
                "short_desc": "The customer to create invoice for",
                "desc": "The unique identifier of the customer for whom the invoice is created",
                "required": True,
            },
            "currency_code": {
                "type": StringOrNothingType,
                "display_name": "Currency Code",
                "short_desc": "Invoice currency",
                "desc": "The currency code for the invoice (e.g., `USD`, `EUR`)",
            },
            "invoice_note": {
                "type": StringOrNothingType,
                "display_name": "Invoice Note",
                "short_desc": "Note on the invoice",
                "desc": "A note to be displayed on the invoice",
            },
            "item_prices": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Item Prices",
                "short_desc": "List of item price objects",
                "desc": "List of item price objects with item_price_id, quantity, and unit_price",
            },
            "charges": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Charges",
                "short_desc": "List of charge objects",
                "desc": "List of charge objects with amount and description",
            },
            "shipping_address": {
                "type": AutoHashOrNothingType,
                "display_name": "Shipping Address",
                "short_desc": "Shipping address for the invoice",
                "desc": "The shipping address associated with the invoice",
            },
            "meta_data": {
                "type": AutoHashOrNothingType,
                "display_name": "Metadata",
                "short_desc": "Custom metadata",
                "desc": "A set of key-value pairs for storing additional information about the invoice",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating a new invoice
public class ChargebeeCreateInvoiceDataProvider inherits ChargebeeDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-invoice",
            "desc": "Create a new invoice for charge items and charges in Chargebee",
            "type": "ChargebeeCreateInvoiceDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static ChargebeeCreateInvoiceRequestDataType RequestType();

        #! Response type
        static ChargebeeInvoiceDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(ChargebeeRestClient::ChargebeeRestClient rest)
            : ChargebeeDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ChargebeeRestClient::ChargebeeRestConnection conn) : ChargebeeDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "create-invoice";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new invoice for charge items and charges in Chargebee";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: creating invoice for customer %s", req.customer_id));

        hash<auto> response = doPost("/invoices/create_for_charge_items_and_charges", req);
        return response.invoice;
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ChargebeeDataProvider
