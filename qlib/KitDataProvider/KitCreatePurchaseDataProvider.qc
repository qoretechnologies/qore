# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KitCreatePurchaseDataProvider.qc Kit create purchase action data provider

/*  KitCreatePurchaseDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KitDataProvider module
public namespace KitDataProvider {

#! Request type for creating a purchase
public class KitCreatePurchaseRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating a purchase
        const Fields = {
            "email_address": {
                "type": StringType,
                "display_name": "Email Address",
                "short_desc": "The buyer's email address",
                "desc": "The email address of the buyer",
                "required": True,
                "example_value": "user@example.com",
            },
            "transaction_id": {
                "type": StringType,
                "display_name": "Transaction ID",
                "short_desc": "The external transaction ID",
                "desc": "The external transaction identifier for the purchase",
                "required": True,
                "example_value": "txn_12345",
            },
            "products": {
                "type": new ListDataType("Products", new KitProductDataType(), True),
                "display_name": "Products",
                "short_desc": "List of products in the purchase",
                "desc": "A list of products in the purchase, each containing product details such as "
                    "`name`, `pid`, `lid`, `quantity`, and `unit_price`",
                "required": True,
            },
            "currency": {
                "type": StringOrNothingType,
                "display_name": "Currency",
                "short_desc": "The currency code",
                "desc": "The ISO 4217 currency code for the purchase (defaults to `USD`)",
                "example_value": "USD",
                "preselected": True,
            },
            "transaction_time": {
                "type": DateOrNothingType,
                "display_name": "Transaction Time",
                "short_desc": "When the transaction occurred",
                "desc": "The timestamp when the transaction occurred",
                "preselected": True,
            },
            "subtotal": {
                "type": FloatOrNothingType,
                "display_name": "Subtotal",
                "short_desc": "The purchase subtotal",
                "desc": "The subtotal amount of the purchase before tax, shipping, and discount",
            },
            "tax": {
                "type": FloatOrNothingType,
                "display_name": "Tax",
                "short_desc": "The tax amount",
                "desc": "The tax amount applied to the purchase",
            },
            "shipping": {
                "type": FloatOrNothingType,
                "display_name": "Shipping",
                "short_desc": "The shipping cost",
                "desc": "The shipping cost for the purchase",
            },
            "discount": {
                "type": FloatOrNothingType,
                "display_name": "Discount",
                "short_desc": "The discount amount",
                "desc": "The discount amount applied to the purchase",
            },
            "total": {
                "type": FloatOrNothingType,
                "display_name": "Total",
                "short_desc": "The total amount",
                "desc": "The total amount of the purchase",
                "preselected": True,
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The purchase status",
                "desc": "The current status of the purchase",
                "allowed_values": (<AllowedValueInfo>{"value": "paid"}, <AllowedValueInfo>{"value": "refunded"}),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Kit create purchase action data provider
public class KitCreatePurchaseDataProvider inherits KitDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new purchase in Kit",
            "type": "KitCreatePurchaseDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(KitRestClient::KitRestClient rest) : KitDataProviderBase(rest) {
    }

    string getName() {
        return "create";
    }

    *string getDesc() {
        return "Create a new purchase in Kit";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KitCreatePurchaseRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KitPurchaseDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> response = doPost("purchases", req);
        return response.purchase;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KitDataProvider
