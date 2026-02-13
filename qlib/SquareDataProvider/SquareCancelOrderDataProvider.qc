# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquareCancelOrderDataProvider.qc Square cancel order action

/*  SquareCancelOrderDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for canceling an order
public class SquareCancelOrderRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "order_id": {
                "type": StringType,
                "display_name": "Order ID",
                "short_desc": "The order to cancel",
                "desc": "The unique identifier of the order to cancel",
                "required": True,
            },
            "version": {
                "type": IntOrNothingType,
                "display_name": "Version",
                "short_desc": "Order version",
                "desc": "The current version of the order (for optimistic concurrency)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Square cancel order action data provider
public class SquareCancelOrderDataProvider inherits SquareDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "cancel",
            "desc": "Cancel an order in Square",
            "type": "SquareCancelOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(SquareRestClient::SquareRestClient rest) : SquareDataProviderBase(rest) {
    }

    string getName() {
        return "cancel";
    }

    *string getDesc() {
        return "Cancel an order in Square";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new SquareCancelOrderRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new SquareOrderDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {};
        if (req.version) {
            body.version = req.version;
        }
        return doPut(sprintf("orders/%s/cancel", req.order_id), body).order;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquareDataProvider
