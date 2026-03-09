# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquarespaceUpdateStockDataProvider.qc Squarespace update stock/inventory action data provider

/*  SquarespaceUpdateStockDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquarespaceDataProvider module
public namespace SquarespaceDataProvider {

#! Request type for updating stock/inventory
public class SquarespaceUpdateStockRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "variant_id": {
                "type": StringType,
                "display_name": "Variant ID",
                "short_desc": "The variant ID to adjust stock for",
                "desc": "The ID of the product variant to adjust inventory for",
                "required": True,
            },
            "quantity": {
                "type": SoftIntType,
                "display_name": "Quantity",
                "short_desc": "The quantity to adjust",
                "desc": "The quantity to increment or decrement (always positive; operation controls direction)",
                "required": True,
            },
            "operation": {
                "type": StringOrNothingType,
                "display_name": "Operation",
                "short_desc": "INCREMENT or DECREMENT (default: INCREMENT)",
                "desc": "The operation to perform: INCREMENT to add stock, DECREMENT to remove stock "
                    "(default: INCREMENT)",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "INCREMENT", "desc": "Add stock"},
                    <AllowedValueInfo>{"value": "DECREMENT", "desc": "Remove stock"},
                ),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Squarespace update stock/inventory action data provider
public class SquarespaceUpdateStockDataProvider inherits SquarespaceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-stock",
            "desc": "Adjust inventory stock for a product variant in Squarespace",
            "type": "SquarespaceUpdateStockDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(SquarespaceRestClient::SquarespaceRestClient rest) : SquarespaceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "update-stock";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Adjust inventory stock for a product variant in Squarespace";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new SquarespaceUpdateStockRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string operation = (req.operation ?? "INCREMENT").upr();
        int qty = abs(req.quantity);

        string key;
        if (operation == "DECREMENT") {
            key = "decrementQuantity";
        } else {
            key = "incrementQuantity";
        }

        hash<auto> body = {
            key: (
                {
                    "variantId": req.variant_id,
                    "quantity": qty,
                },
            ),
        };

        return doPost("1.0/commerce/inventory/adjustments", body);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquarespaceDataProvider
