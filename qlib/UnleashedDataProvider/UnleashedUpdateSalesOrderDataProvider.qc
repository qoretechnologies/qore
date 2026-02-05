# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnleashedDataProvider module definition

/*  UnleashedUpdateSalesOrderDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the UnleashedDataProvider module
public namespace UnleashedDataProvider {

#! Data provider for updating a sales order
public class UnleashedUpdateSalesOrderDataProvider inherits UnleashedDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-sales-order",
            "desc": "Updates an existing sales order in Unleashed",
            "type": "UnleashedUpdateSalesOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static UnleashedUpdateSalesOrderRequestDataType RequestType();

        #! Response type
        static UnleashedSalesOrderDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(UnleashedRestClient::UnleashedRestClient rest) : UnleashedDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(UnleashedRestClient::UnleashedRestConnection conn) : UnleashedDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : UnleashedDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "update-sales-order";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Updates an existing sales order in Unleashed (note: Unleashed uses PUT for sales order updates)";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string guid = req.guid;
        @assert(guid.val(), "guid is required");

        hash<auto> body = {};

        # Optional fields
        if (exists req.OrderStatus) body.OrderStatus = req.OrderStatus;
        if (exists req.RequiredDate) body.RequiredDate = formatDate(req.RequiredDate);
        if (exists req.Comments) body.Comments = req.Comments;
        if (exists req.SalesOrderLines) body.SalesOrderLines = req.SalesOrderLines;

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: updating sales order %y", guid));
        hash<auto> response = doPut("/SalesOrders/" + guid, body);
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: updated sales order %y", response.Guid));
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

#! Request data type for updating a sales order
public class UnleashedUpdateSalesOrderRequestDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "guid": {
                "type": StringType,
                "display_name": "Sales Order GUID",
                "short_desc": "The sales order GUID",
                "desc": "The unique identifier (GUID) of the sales order to update",
                "required": True,
                "ref_data": "salesorders",
            },
            "OrderStatus": {
                "type": SoftStringOrNothingType,
                "display_name": "Order Status",
                "short_desc": "Order status",
                "desc": "The order status",
            },
            "RequiredDate": {
                "type": SoftDateOrNothingType,
                "display_name": "Required Date",
                "short_desc": "Required delivery date",
                "desc": "The required delivery date",
            },
            "Comments": {
                "type": SoftStringOrNothingType,
                "display_name": "Comments",
                "short_desc": "Order comments",
                "desc": "Comments or notes for this order",
            },
            "SalesOrderLines": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Sales Order Lines",
                "short_desc": "Order line items",
                "desc": "List of line items for this sales order",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("UnleashedUpdateSalesOrderRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace UnleashedDataProvider
