# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SquarespaceDataProvider module definition

/*  SquarespaceTransactionDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Squarespace transaction record type
public class SquarespaceTransactionDataType inherits HashDataType {
    public {
        #! Field definitions for Squarespace transactions
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The transaction ID",
                "desc": "The unique identifier of the transaction",
            },
            "createdOn": {
                "type": DateOrNothingType,
                "display_name": "Created On",
                "short_desc": "When the transaction was created",
                "desc": "The timestamp when the transaction was created",
            },
            "modifiedOn": {
                "type": DateOrNothingType,
                "display_name": "Modified On",
                "short_desc": "When the transaction was last modified",
                "desc": "The timestamp when the transaction was last modified",
            },
            "customerEmail": {
                "type": StringOrNothingType,
                "display_name": "Customer Email",
                "short_desc": "The customer's email address",
                "desc": "The email address of the customer associated with the transaction",
            },
            "salesOrderId": {
                "type": StringOrNothingType,
                "display_name": "Sales Order ID",
                "short_desc": "The associated sales order ID",
                "desc": "The ID of the sales order associated with the transaction",
            },
            "voided": {
                "type": SoftBoolOrNothingType,
                "display_name": "Voided",
                "short_desc": "Whether the transaction is voided",
                "desc": "Indicates whether the transaction has been voided",
            },
            "totalSales": {
                "type": new SquarespaceMoneyDataType(AutoHashOrNothingType),
                "display_name": "Total Sales",
                "short_desc": "The total sales amount",
                "desc": "The total sales amount for the transaction",
            },
            "totalNetSales": {
                "type": new SquarespaceMoneyDataType(AutoHashOrNothingType),
                "display_name": "Total Net Sales",
                "short_desc": "The total net sales amount",
                "desc": "The total net sales amount for the transaction",
            },
            "totalNetShipping": {
                "type": new SquarespaceMoneyDataType(AutoHashOrNothingType),
                "display_name": "Total Net Shipping",
                "short_desc": "The total net shipping amount",
                "desc": "The total net shipping amount for the transaction",
            },
            "totalTaxes": {
                "type": new SquarespaceMoneyDataType(AutoHashOrNothingType),
                "display_name": "Total Taxes",
                "short_desc": "The total tax amount",
                "desc": "The total tax amount for the transaction",
            },
            "total": {
                "type": new SquarespaceMoneyDataType(AutoHashOrNothingType),
                "display_name": "Total",
                "short_desc": "The transaction total",
                "desc": "The total amount for the transaction",
            },
            "totalNetPayment": {
                "type": new SquarespaceMoneyDataType(AutoHashOrNothingType),
                "display_name": "Total Net Payment",
                "short_desc": "The total net payment amount",
                "desc": "The total net payment amount for the transaction",
            },
            "payments": {
                "type": SoftListOrNothingType,
                "display_name": "Payments",
                "short_desc": "The transaction payments",
                "desc": "The list of payments associated with the transaction",
            },
            "salesLineItems": {
                "type": SoftListOrNothingType,
                "display_name": "Sales Line Items",
                "short_desc": "The sales line items",
                "desc": "The list of sales line items in the transaction",
            },
            "discounts": {
                "type": SoftListOrNothingType,
                "display_name": "Discounts",
                "short_desc": "The transaction discounts",
                "desc": "The list of discounts applied to the transaction",
            },
            "shippingLineItems": {
                "type": SoftListOrNothingType,
                "display_name": "Shipping Line Items",
                "short_desc": "The shipping line items",
                "desc": "The list of shipping line items in the transaction",
            },
            "paymentGatewayError": {
                "type": AutoHashOrNothingType,
                "display_name": "Payment Gateway Error",
                "short_desc": "Payment gateway error details",
                "desc": "The payment gateway error details if the transaction encountered a payment error",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SquarespaceDataProvider
