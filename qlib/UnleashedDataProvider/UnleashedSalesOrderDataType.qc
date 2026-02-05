# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnleashedDataProvider module definition

/*  UnleashedSalesOrderDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Sales order data type for Unleashed
public class UnleashedSalesOrderDataType inherits HashDataType {
    private {
        const Fields = {
            "Guid": {
                "type": SoftStringOrNothingType,
                "display_name": "Order GUID",
                "short_desc": "Unique order identifier",
                "desc": "The unique GUID identifier of the sales order",
            },
            "OrderNumber": {
                "type": SoftStringOrNothingType,
                "display_name": "Order Number",
                "short_desc": "Sales order number",
                "desc": "The sales order number",
            },
            "OrderDate": {
                "type": SoftDateOrNothingType,
                "display_name": "Order Date",
                "short_desc": "Order date",
                "desc": "The date the order was placed",
            },
            "OrderStatus": {
                "type": SoftStringOrNothingType,
                "display_name": "Order Status",
                "short_desc": "Status of the order",
                "desc": "The current status of the sales order (Open, Parked, Backordered, Placed, Completed, Deleted)",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Open", "desc": "Order is open"},
                    <AllowedValueInfo>{"value": "Parked", "desc": "Order is parked/on hold"},
                    <AllowedValueInfo>{"value": "Backordered", "desc": "Order is backordered"},
                    <AllowedValueInfo>{"value": "Placed", "desc": "Order has been placed"},
                    <AllowedValueInfo>{"value": "Completed", "desc": "Order is completed"},
                    <AllowedValueInfo>{"value": "Deleted", "desc": "Order has been deleted"},
                ),
            },
            "CompletedDate": {
                "type": SoftDateOrNothingType,
                "display_name": "Completed Date",
                "short_desc": "Completion date",
                "desc": "The date the order was completed",
            },
            "RequiredDate": {
                "type": SoftDateOrNothingType,
                "display_name": "Required Date",
                "short_desc": "Required delivery date",
                "desc": "The required delivery date",
            },
            "Customer": {
                "type": SoftAutoHashOrNothingType,
                "display_name": "Customer",
                "short_desc": "Customer details",
                "desc": "The customer for this order",
            },
            "Warehouse": {
                "type": SoftAutoHashOrNothingType,
                "display_name": "Warehouse",
                "short_desc": "Warehouse details",
                "desc": "The warehouse for this order",
            },
            "Currency": {
                "type": SoftAutoHashOrNothingType,
                "display_name": "Currency",
                "short_desc": "Currency details",
                "desc": "The currency for this order",
            },
            "ExchangeRate": {
                "type": SoftFloatOrNothingType,
                "display_name": "Exchange Rate",
                "short_desc": "Currency exchange rate",
                "desc": "The exchange rate for this order",
            },
            "DiscountRate": {
                "type": SoftFloatOrNothingType,
                "display_name": "Discount Rate",
                "short_desc": "Order discount rate",
                "desc": "The discount rate applied to this order",
            },
            "Tax": {
                "type": SoftAutoHashOrNothingType,
                "display_name": "Tax",
                "short_desc": "Tax details",
                "desc": "The tax configuration for this order",
            },
            "TaxRate": {
                "type": SoftFloatOrNothingType,
                "display_name": "Tax Rate",
                "short_desc": "Tax rate",
                "desc": "The tax rate applied to this order",
            },
            "SubTotal": {
                "type": SoftFloatOrNothingType,
                "display_name": "Sub Total",
                "short_desc": "Order subtotal",
                "desc": "The subtotal before tax",
            },
            "TaxTotal": {
                "type": SoftFloatOrNothingType,
                "display_name": "Tax Total",
                "short_desc": "Total tax",
                "desc": "The total tax amount",
            },
            "Total": {
                "type": SoftFloatOrNothingType,
                "display_name": "Total",
                "short_desc": "Order total",
                "desc": "The total order amount including tax",
            },
            "SalesOrderLines": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Sales Order Lines",
                "short_desc": "Order line items",
                "desc": "The line items for this sales order",
            },
            "Salesperson": {
                "type": SoftAutoHashOrNothingType,
                "display_name": "Salesperson",
                "short_desc": "Assigned salesperson",
                "desc": "The salesperson assigned to this order",
            },
            "DeliveryName": {
                "type": SoftStringOrNothingType,
                "display_name": "Delivery Name",
                "short_desc": "Delivery contact name",
                "desc": "The name for delivery",
            },
            "DeliveryStreetAddress": {
                "type": SoftStringOrNothingType,
                "display_name": "Delivery Street Address",
                "short_desc": "Delivery street address",
                "desc": "The street address for delivery",
            },
            "DeliverySuburb": {
                "type": SoftStringOrNothingType,
                "display_name": "Delivery Suburb",
                "short_desc": "Delivery suburb",
                "desc": "The suburb for delivery",
            },
            "DeliveryCity": {
                "type": SoftStringOrNothingType,
                "display_name": "Delivery City",
                "short_desc": "Delivery city",
                "desc": "The city for delivery",
            },
            "DeliveryRegion": {
                "type": SoftStringOrNothingType,
                "display_name": "Delivery Region",
                "short_desc": "Delivery region/state",
                "desc": "The region/state for delivery",
            },
            "DeliveryCountry": {
                "type": SoftStringOrNothingType,
                "display_name": "Delivery Country",
                "short_desc": "Delivery country",
                "desc": "The country for delivery",
            },
            "DeliveryPostCode": {
                "type": SoftStringOrNothingType,
                "display_name": "Delivery Post Code",
                "short_desc": "Delivery postal code",
                "desc": "The postal code for delivery",
            },
            "Comments": {
                "type": SoftStringOrNothingType,
                "display_name": "Comments",
                "short_desc": "Order comments",
                "desc": "Comments or notes for this order",
            },
            "CustomerRef": {
                "type": SoftStringOrNothingType,
                "display_name": "Customer Reference",
                "short_desc": "Customer reference",
                "desc": "Customer's reference number for this order",
            },
            "CreatedOn": {
                "type": SoftDateOrNothingType,
                "display_name": "Created On",
                "short_desc": "Creation date",
                "desc": "When the order was created",
            },
            "LastModifiedOn": {
                "type": SoftDateOrNothingType,
                "display_name": "Last Modified On",
                "short_desc": "Last modification date",
                "desc": "When the order was last modified",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("UnleashedSalesOrderDataType") {
        addQoreFields(Fields);
    }
}

#! Sales order line data type
public class UnleashedSalesOrderLineDataType inherits HashDataType {
    private {
        const Fields = {
            "Guid": {
                "type": SoftStringOrNothingType,
                "display_name": "Line GUID",
                "short_desc": "Unique line identifier",
                "desc": "The unique GUID identifier of the order line",
            },
            "LineNumber": {
                "type": SoftIntOrNothingType,
                "display_name": "Line Number",
                "short_desc": "Line number",
                "desc": "The line number within the order",
            },
            "Product": {
                "type": SoftAutoHashOrNothingType,
                "display_name": "Product",
                "short_desc": "Product details",
                "desc": "The product for this line",
            },
            "OrderQuantity": {
                "type": SoftFloatOrNothingType,
                "display_name": "Order Quantity",
                "short_desc": "Quantity ordered",
                "desc": "The quantity ordered",
            },
            "UnitPrice": {
                "type": SoftFloatOrNothingType,
                "display_name": "Unit Price",
                "short_desc": "Price per unit",
                "desc": "The price per unit",
            },
            "DiscountRate": {
                "type": SoftFloatOrNothingType,
                "display_name": "Discount Rate",
                "short_desc": "Line discount rate",
                "desc": "The discount rate for this line",
            },
            "TaxRate": {
                "type": SoftFloatOrNothingType,
                "display_name": "Tax Rate",
                "short_desc": "Line tax rate",
                "desc": "The tax rate for this line",
            },
            "LineTotal": {
                "type": SoftFloatOrNothingType,
                "display_name": "Line Total",
                "short_desc": "Total for line",
                "desc": "The total amount for this line before tax",
            },
            "LineTax": {
                "type": SoftFloatOrNothingType,
                "display_name": "Line Tax",
                "short_desc": "Tax for line",
                "desc": "The tax amount for this line",
            },
            "Comments": {
                "type": SoftStringOrNothingType,
                "display_name": "Comments",
                "short_desc": "Line comments",
                "desc": "Comments for this line item",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("UnleashedSalesOrderLineDataType") {
        addQoreFields(Fields);
    }
}

#! Sales order creation request data type
public class UnleashedCreateSalesOrderRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "OrderNumber": {
                "type": SoftStringOrNothingType,
                "display_name": "Order Number",
                "short_desc": "Sales order number",
                "desc": "The sales order number (auto-generated if not provided)",
            },
            "OrderDate": {
                "type": SoftDateOrNothingType,
                "display_name": "Order Date",
                "short_desc": "Order date",
                "desc": "The date the order was placed",
            },
            "OrderStatus": {
                "type": SoftStringType,
                "display_name": "Order Status",
                "short_desc": "Status of the order",
                "desc": "The status of the sales order (required)",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Open", "desc": "Order is open"},
                    <AllowedValueInfo>{"value": "Parked", "desc": "Order is parked/on hold"},
                ),
            },
            "Customer": {
                "type": SoftAutoHashType,
                "display_name": "Customer",
                "short_desc": "Customer details",
                "desc": "The customer for this order (Guid or CustomerCode required)",
                "required": True,
            },
            "Warehouse": {
                "type": SoftAutoHashType,
                "display_name": "Warehouse",
                "short_desc": "Warehouse details",
                "desc": "The warehouse for this order (Guid or WarehouseCode required)",
                "required": True,
            },
            "Currency": {
                "type": SoftAutoHashType,
                "display_name": "Currency",
                "short_desc": "Currency details",
                "desc": "The currency for this order (Guid or CurrencyCode required)",
                "required": True,
            },
            "ExchangeRate": {
                "type": SoftFloatType,
                "display_name": "Exchange Rate",
                "short_desc": "Currency exchange rate",
                "desc": "The exchange rate for this order (required)",
                "required": True,
            },
            "Tax": {
                "type": SoftAutoHashType,
                "display_name": "Tax",
                "short_desc": "Tax details",
                "desc": "The tax configuration (TaxCode or TaxRate required)",
                "required": True,
            },
            "SalesOrderLines": {
                "type": SoftAutoListType,
                "display_name": "Sales Order Lines",
                "short_desc": "Order line items",
                "desc": "The line items for this sales order (at least one required)",
                "required": True,
            },
            "SubTotal": {
                "type": SoftFloatType,
                "display_name": "Sub Total",
                "short_desc": "Order subtotal",
                "desc": "The subtotal before tax (required)",
                "required": True,
            },
            "TaxTotal": {
                "type": SoftFloatType,
                "display_name": "Tax Total",
                "short_desc": "Total tax",
                "desc": "The total tax amount (required)",
                "required": True,
            },
            "Total": {
                "type": SoftFloatType,
                "display_name": "Total",
                "short_desc": "Order total",
                "desc": "The total order amount including tax (required)",
                "required": True,
            },
            "Comments": {
                "type": SoftStringOrNothingType,
                "display_name": "Comments",
                "short_desc": "Order comments",
                "desc": "Comments or notes for this order",
            },
            "CustomerRef": {
                "type": SoftStringOrNothingType,
                "display_name": "Customer Reference",
                "short_desc": "Customer reference",
                "desc": "Customer's reference number for this order",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("UnleashedCreateSalesOrderRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace UnleashedDataProvider
