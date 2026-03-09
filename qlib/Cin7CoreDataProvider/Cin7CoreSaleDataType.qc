# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreSaleDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Cin7Core sale data type
public class Cin7CoreSaleDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "ID": {
                "type": SoftStringOrNothingType,
                "display_name": "Sale ID",
                "short_desc": "Unique sale identifier",
                "desc": "The unique identifier (GUID) of the sale in Cin7Core",
            },
            "SaleOrderNumber": {
                "type": SoftStringOrNothingType,
                "display_name": "Sale Order Number",
                "short_desc": "Order number",
                "desc": "The sale order number",
            },
            "ExternalID": {
                "type": SoftStringOrNothingType,
                "display_name": "External ID",
                "short_desc": "External reference",
                "desc": "External system reference ID",
            },
            "Status": {
                "type": SoftStringOrNothingType,
                "display_name": "Status",
                "short_desc": "Sale status",
                "desc": "Current status of the sale order",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Draft", "desc": "Draft order"},
                    <AllowedValueInfo>{"value": "Authorised", "desc": "Authorised order"},
                    <AllowedValueInfo>{"value": "Picking", "desc": "Being picked"},
                    <AllowedValueInfo>{"value": "Packed", "desc": "Packed"},
                    <AllowedValueInfo>{"value": "Shipped", "desc": "Shipped"},
                    <AllowedValueInfo>{"value": "Completed", "desc": "Completed"},
                    <AllowedValueInfo>{"value": "Voided", "desc": "Voided"},
                ),
            },
            "CombinedPickingStatus": {
                "type": SoftStringOrNothingType,
                "display_name": "Picking Status",
                "short_desc": "Combined picking status",
                "desc": "Combined picking status across all fulfillments",
            },
            "CombinedPackingStatus": {
                "type": SoftStringOrNothingType,
                "display_name": "Packing Status",
                "short_desc": "Combined packing status",
                "desc": "Combined packing status across all fulfillments",
            },
            "CombinedShippingStatus": {
                "type": SoftStringOrNothingType,
                "display_name": "Shipping Status",
                "short_desc": "Combined shipping status",
                "desc": "Combined shipping status across all fulfillments",
            },
            "CombinedPaymentStatus": {
                "type": SoftStringOrNothingType,
                "display_name": "Payment Status",
                "short_desc": "Combined payment status",
                "desc": "Combined payment status across all invoices",
            },
            "CombinedInvoiceStatus": {
                "type": SoftStringOrNothingType,
                "display_name": "Invoice Status",
                "short_desc": "Combined invoice status",
                "desc": "Combined invoice status",
            },
            "CustomerID": {
                "type": SoftStringOrNothingType,
                "display_name": "Customer ID",
                "short_desc": "Customer identifier",
                "desc": "The GUID of the customer",
            },
            "CustomerName": {
                "type": SoftStringOrNothingType,
                "display_name": "Customer Name",
                "short_desc": "Customer name",
                "desc": "The name of the customer",
            },
            "Customer": {
                "type": SoftStringOrNothingType,
                "display_name": "Customer",
                "short_desc": "Customer reference",
                "desc": "Customer name or reference",
            },
            "Phone": {
                "type": SoftStringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Phone number",
                "desc": "Customer phone number",
            },
            "Email": {
                "type": SoftStringOrNothingType,
                "display_name": "Email",
                "short_desc": "Email address",
                "desc": "Customer email address",
            },
            "BillingAddress": {
                "type": new Cin7CoreAddressDataType(AutoHashOrNothingType),
                "display_name": "Billing Address",
                "short_desc": "Billing address",
                "desc": "The billing address for the sale",
            },
            "ShippingAddress": {
                "type": new Cin7CoreAddressDataType(AutoHashOrNothingType),
                "display_name": "Shipping Address",
                "short_desc": "Shipping address",
                "desc": "The shipping address for the sale",
            },
            "SaleOrderDate": {
                "type": SoftDateOrNothingType,
                "display_name": "Order Date",
                "short_desc": "Sale order date",
                "desc": "Date when the sale order was created",
            },
            "RequiredBy": {
                "type": SoftDateOrNothingType,
                "display_name": "Required By",
                "short_desc": "Required delivery date",
                "desc": "Date by which the order must be delivered",
            },
            "ShipBy": {
                "type": SoftDateOrNothingType,
                "display_name": "Ship By",
                "short_desc": "Ship by date",
                "desc": "Date by which the order must be shipped",
            },
            "Location": {
                "type": SoftStringOrNothingType,
                "display_name": "Location",
                "short_desc": "Warehouse location",
                "desc": "The warehouse location for the sale",
            },
            "Currency": {
                "type": SoftStringOrNothingType,
                "display_name": "Currency",
                "short_desc": "Currency code",
                "desc": "Three-letter ISO currency code",
            },
            "CurrencyRate": {
                "type": SoftFloatOrNothingType,
                "display_name": "Currency Rate",
                "short_desc": "Exchange rate",
                "desc": "Currency exchange rate",
            },
            "TaxRule": {
                "type": SoftStringOrNothingType,
                "display_name": "Tax Rule",
                "short_desc": "Tax rule",
                "desc": "Tax rule applied to the sale",
            },
            "TaxInclusive": {
                "type": SoftBoolOrNothingType,
                "display_name": "Tax Inclusive",
                "short_desc": "Prices include tax",
                "desc": "Whether prices include tax",
            },
            "Terms": {
                "type": SoftStringOrNothingType,
                "display_name": "Terms",
                "short_desc": "Payment terms",
                "desc": "Payment terms for the sale",
            },
            "Carrier": {
                "type": SoftStringOrNothingType,
                "display_name": "Carrier",
                "short_desc": "Shipping carrier",
                "desc": "The shipping carrier",
            },
            "Total": {
                "type": SoftFloatOrNothingType,
                "display_name": "Total",
                "short_desc": "Order total",
                "desc": "Total amount of the order",
            },
            "TotalBeforeTax": {
                "type": SoftFloatOrNothingType,
                "display_name": "Total Before Tax",
                "short_desc": "Subtotal",
                "desc": "Total before tax",
            },
            "TaxTotal": {
                "type": SoftFloatOrNothingType,
                "display_name": "Tax Total",
                "short_desc": "Tax amount",
                "desc": "Total tax amount",
            },
            "Paid": {
                "type": SoftFloatOrNothingType,
                "display_name": "Paid",
                "short_desc": "Amount paid",
                "desc": "Amount paid so far",
            },
            "Lines": {
                "type": new ListDataType("Lines", new Cin7CoreSaleLineItemDataType(), True),
                "display_name": "Line Items",
                "short_desc": "Order lines",
                "desc": "List of line items in the order",
            },
            "Fulfilments": {
                "type": new ListDataType("Fulfilments", new Cin7CoreFulfilmentDataType(), True),
                "display_name": "Fulfilments",
                "short_desc": "Fulfilment records",
                "desc": "List of fulfilment records",
            },
            "Invoices": {
                "type": new ListDataType("Invoices", new Cin7CoreInvoiceDataType(), True),
                "display_name": "Invoices",
                "short_desc": "Invoice records",
                "desc": "List of invoice records",
            },
            "CreditNotes": {
                "type": new ListDataType("CreditNotes", new Cin7CoreCreditNoteDataType(), True),
                "display_name": "Credit Notes",
                "short_desc": "Credit note records",
                "desc": "List of credit note records",
            },
            "LastModifiedOn": {
                "type": SoftDateOrNothingType,
                "display_name": "Last Modified",
                "short_desc": "Last modification date",
                "desc": "Timestamp when the sale was last modified",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreSaleDataType") {
        addQoreFields(Fields);
    }
}

#! Cin7Core create sale request data type
public class Cin7CoreCreateSaleRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "Customer": {
                "type": SoftStringType,
                "display_name": "Customer",
                "short_desc": "Customer name",
                "desc": "The customer name (required). Must match an existing customer",
                "required": True,
            },
            "Contact": {
                "type": SoftStringOrNothingType,
                "display_name": "Contact",
                "short_desc": "Contact name",
                "desc": "The contact name at the customer",
            },
            "Phone": {
                "type": SoftStringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Phone number",
                "desc": "Customer phone number",
            },
            "Email": {
                "type": SoftStringOrNothingType,
                "display_name": "Email",
                "short_desc": "Email address",
                "desc": "Customer email address",
            },
            "BillingAddress": {
                "type": new Cin7CoreAddressDataType(AutoHashOrNothingType),
                "display_name": "Billing Address",
                "short_desc": "Billing address",
                "desc": "The billing address for the sale",
            },
            "ShippingAddress": {
                "type": new Cin7CoreAddressDataType(AutoHashOrNothingType),
                "display_name": "Shipping Address",
                "short_desc": "Shipping address",
                "desc": "The shipping address for the sale",
            },
            "SaleOrderDate": {
                "type": SoftDateOrNothingType,
                "display_name": "Order Date",
                "short_desc": "Sale order date",
                "desc": "Date when the sale order was created (defaults to today)",
            },
            "RequiredBy": {
                "type": SoftDateOrNothingType,
                "display_name": "Required By",
                "short_desc": "Required delivery date",
                "desc": "Date by which the order must be delivered",
            },
            "ShipBy": {
                "type": SoftDateOrNothingType,
                "display_name": "Ship By",
                "short_desc": "Ship by date",
                "desc": "Date by which the order must be shipped",
            },
            "Location": {
                "type": SoftStringOrNothingType,
                "display_name": "Location",
                "short_desc": "Warehouse location",
                "desc": "The warehouse location for the sale",
            },
            "Currency": {
                "type": SoftStringOrNothingType,
                "display_name": "Currency",
                "short_desc": "Currency code",
                "desc": "Three-letter ISO currency code",
            },
            "TaxRule": {
                "type": SoftStringOrNothingType,
                "display_name": "Tax Rule",
                "short_desc": "Tax rule",
                "desc": "Tax rule to apply",
            },
            "TaxInclusive": {
                "type": SoftBoolOrNothingType,
                "display_name": "Tax Inclusive",
                "short_desc": "Prices include tax",
                "desc": "Whether prices include tax",
            },
            "Terms": {
                "type": SoftStringOrNothingType,
                "display_name": "Terms",
                "short_desc": "Payment terms",
                "desc": "Payment terms for the sale",
            },
            "Carrier": {
                "type": SoftStringOrNothingType,
                "display_name": "Carrier",
                "short_desc": "Shipping carrier",
                "desc": "The shipping carrier",
            },
            "Lines": {
                "type": new ListDataType("Lines", new Cin7CoreSaleLineItemDataType(), True),
                "display_name": "Line Items",
                "short_desc": "Order lines",
                "desc": "List of line items to add to the order",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreCreateSaleRequestDataType") {
        addQoreFields(Fields);
    }
}

#! Cin7Core sale line item data type
public class Cin7CoreSaleLineItemDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "ProductID": {
                "type": SoftStringOrNothingType,
                "display_name": "Product ID",
                "short_desc": "Product identifier",
                "desc": "The product GUID",
            },
            "SKU": {
                "type": SoftStringOrNothingType,
                "display_name": "SKU",
                "short_desc": "Stock keeping unit",
                "desc": "The product SKU",
            },
            "Name": {
                "type": SoftStringOrNothingType,
                "display_name": "Name",
                "short_desc": "Product name",
                "desc": "The product name",
            },
            "Quantity": {
                "type": SoftFloatOrNothingType,
                "display_name": "Quantity",
                "short_desc": "Order quantity",
                "desc": "The quantity ordered",
            },
            "Price": {
                "type": SoftFloatOrNothingType,
                "display_name": "Price",
                "short_desc": "Unit price",
                "desc": "The unit price",
            },
            "Discount": {
                "type": SoftFloatOrNothingType,
                "display_name": "Discount",
                "short_desc": "Discount %",
                "desc": "Line discount percentage",
            },
            "Tax": {
                "type": SoftFloatOrNothingType,
                "display_name": "Tax",
                "short_desc": "Tax amount",
                "desc": "Tax amount for this line",
            },
            "Total": {
                "type": SoftFloatOrNothingType,
                "display_name": "Total",
                "short_desc": "Line total",
                "desc": "Total for this line",
            },
            "Comment": {
                "type": SoftStringOrNothingType,
                "display_name": "Comment",
                "short_desc": "Line comment",
                "desc": "Comment for this line item",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreSaleLineItemDataType") {
        addQoreFields(Fields);
    }
}

} # namespace Cin7CoreDataProvider
