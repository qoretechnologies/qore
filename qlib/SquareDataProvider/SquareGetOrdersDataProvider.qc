# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquareGetOrdersDataProvider.qc Square batch get orders action

/*  SquareGetOrdersDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquareDataProvider module
public namespace SquareDataProvider {

#! Request type for batch getting orders
public class SquareGetOrdersRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "order_ids": {
                "type": new Type("list<string>"),
                "display_name": "Order IDs",
                "short_desc": "List of order IDs to retrieve",
                "desc": "List of order IDs to retrieve (up to 100)",
                "required": True,
            },
            "location_id": {
                "type": StringOrNothingType,
                "display_name": "Location ID",
                "short_desc": "Optional location ID",
                "desc": "Optional location ID to scope the request",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Response type for batch get orders (list of orders)
public class SquareGetOrdersResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "orders": {
                "type": AutoListOrNothingType,
                "display_name": "Orders",
                "short_desc": "Retrieved orders",
                "desc": "List of retrieved order objects",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Square batch get orders action data provider
public class SquareGetOrdersDataProvider inherits SquareDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "batch-get",
            "desc": "Retrieve multiple orders by ID from Square",
            "type": "SquareGetOrdersDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(SquareRestClient::SquareRestClient rest) : SquareDataProviderBase(rest) {
    }

    string getName() {
        return "batch-get";
    }

    *string getDesc() {
        return "Retrieve multiple orders by ID from Square";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new SquareGetOrdersRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new SquareGetOrdersResponseDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {
            "order_ids": req.order_ids,
        };
        if (req.location_id) {
            body.location_id = req.location_id;
        }
        return doPost("orders/batch-retrieve", body);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquareDataProvider
