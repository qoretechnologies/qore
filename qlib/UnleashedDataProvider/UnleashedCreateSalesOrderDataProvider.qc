# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnleashedDataProvider module definition

/*  UnleashedCreateSalesOrderDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for creating a sales order
public class UnleashedCreateSalesOrderDataProvider inherits UnleashedDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-sales-order",
            "desc": "Creates a new sales order in Unleashed",
            "type": "UnleashedCreateSalesOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static UnleashedCreateSalesOrderRequestDataType RequestType();

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
        return "create-sales-order";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Creates a new sales order in Unleashed";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {};

        # Required and optional fields
        if (exists req.Customer) body.Customer = req.Customer;
        if (exists req.OrderNumber) body.OrderNumber = req.OrderNumber;
        if (exists req.OrderDate) body.OrderDate = formatDate(req.OrderDate);
        if (exists req.RequiredDate) body.RequiredDate = formatDate(req.RequiredDate);
        if (exists req.OrderStatus) body.OrderStatus = req.OrderStatus;
        if (exists req.SalesOrderLines) body.SalesOrderLines = req.SalesOrderLines;
        if (exists req.Comments) body.Comments = req.Comments;
        if (exists req.DeliveryName) body.DeliveryName = req.DeliveryName;
        if (exists req.DeliveryStreetAddress) body.DeliveryStreetAddress = req.DeliveryStreetAddress;
        if (exists req.DeliverySuburb) body.DeliverySuburb = req.DeliverySuburb;
        if (exists req.DeliveryCity) body.DeliveryCity = req.DeliveryCity;
        if (exists req.DeliveryRegion) body.DeliveryRegion = req.DeliveryRegion;
        if (exists req.DeliveryCountry) body.DeliveryCountry = req.DeliveryCountry;
        if (exists req.DeliveryPostCode) body.DeliveryPostCode = req.DeliveryPostCode;

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: creating sales order"));
        hash<auto> response = doPost("/SalesOrders", body);
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: created sales order %y", response.SalesOrder.OrderNumber));
        return response.SalesOrder;
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

#! Request data type for creating a sales order
public class UnleashedCreateSalesOrderRequestDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "Customer": {
                "type": AutoHashOrNothingType,
                "display_name": "Customer",
                "short_desc": "Customer information",
                "desc": "The customer for this sales order (must include Guid)",
            },
            "OrderNumber": {
                "type": SoftStringOrNothingType,
                "display_name": "Order Number",
                "short_desc": "Order number",
                "desc": "The sales order number",
            },
            "OrderDate": {
                "type": SoftDateOrNothingType,
                "display_name": "Order Date",
                "short_desc": "Order date",
                "desc": "The order date",
            },
            "RequiredDate": {
                "type": SoftDateOrNothingType,
                "display_name": "Required Date",
                "short_desc": "Required delivery date",
                "desc": "The required delivery date",
            },
            "OrderStatus": {
                "type": SoftStringOrNothingType,
                "display_name": "Order Status",
                "short_desc": "Order status",
                "desc": "The order status (e.g., 'Parked', 'Placed')",
            },
            "SalesOrderLines": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Sales Order Lines",
                "short_desc": "Order line items",
                "desc": "List of line items for this sales order",
            },
            "Comments": {
                "type": SoftStringOrNothingType,
                "display_name": "Comments",
                "short_desc": "Order comments",
                "desc": "Comments or notes for this order",
            },
            "DeliveryName": {
                "type": SoftStringOrNothingType,
                "display_name": "Delivery Name",
                "short_desc": "Delivery recipient name",
                "desc": "Name of the delivery recipient",
            },
            "DeliveryStreetAddress": {
                "type": SoftStringOrNothingType,
                "display_name": "Delivery Street Address",
                "short_desc": "Delivery street address",
                "desc": "Street address for delivery",
            },
            "DeliverySuburb": {
                "type": SoftStringOrNothingType,
                "display_name": "Delivery Suburb",
                "short_desc": "Delivery suburb",
                "desc": "Suburb for delivery",
            },
            "DeliveryCity": {
                "type": SoftStringOrNothingType,
                "display_name": "Delivery City",
                "short_desc": "Delivery city",
                "desc": "City for delivery",
            },
            "DeliveryRegion": {
                "type": SoftStringOrNothingType,
                "display_name": "Delivery Region",
                "short_desc": "Delivery region/state",
                "desc": "Region or state for delivery",
            },
            "DeliveryCountry": {
                "type": SoftStringOrNothingType,
                "display_name": "Delivery Country",
                "short_desc": "Delivery country",
                "desc": "Country for delivery",
            },
            "DeliveryPostCode": {
                "type": SoftStringOrNothingType,
                "display_name": "Delivery Post Code",
                "short_desc": "Delivery postal code",
                "desc": "Postal code for delivery",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("UnleashedCreateSalesOrderRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace UnleashedDataProvider
