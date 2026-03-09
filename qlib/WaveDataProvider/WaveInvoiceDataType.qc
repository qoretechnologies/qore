# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WaveDataProvider module definition

/*  WaveInvoiceDataType.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the WaveDataProvider module
public namespace WaveDataProvider {

#! Wave invoice record type
public class WaveInvoiceDataType inherits HashDataType {
    private {
        #! Field definitions for Wave invoices
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The unique identifier of the invoice",
                "desc": "The unique identifier of the invoice",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The invoice status",
                "desc": "The current status of the invoice",
                "example_value": "SENT",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "DRAFT", "desc": "Draft invoice"},
                    <AllowedValueInfo>{"value": "SAVED", "desc": "Saved invoice"},
                    <AllowedValueInfo>{"value": "SENT", "desc": "Sent invoice"},
                    <AllowedValueInfo>{"value": "VIEWED", "desc": "Viewed by customer"},
                    <AllowedValueInfo>{"value": "OVERDUE", "desc": "Overdue invoice"},
                    <AllowedValueInfo>{"value": "PAID", "desc": "Paid invoice"},
                    <AllowedValueInfo>{"value": "PARTIAL", "desc": "Partially paid"},
                    <AllowedValueInfo>{"value": "UNPAID", "desc": "Unpaid invoice"},
                ),
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "The invoice title",
                "desc": "The title displayed on the invoice",
                "example_value": "Invoice",
            },
            "subhead": {
                "type": StringOrNothingType,
                "display_name": "Subhead",
                "short_desc": "The invoice subhead",
                "desc": "The subhead text displayed on the invoice",
                "example_value": "Consulting Services",
            },
            "invoiceNumber": {
                "type": StringOrNothingType,
                "display_name": "Invoice Number",
                "short_desc": "The invoice number",
                "desc": "The invoice number for reference",
                "example_value": "INV-0001",
            },
            "invoiceDate": {
                "type": DateOrNothingType,
                "display_name": "Invoice Date",
                "short_desc": "The invoice date",
                "desc": "The date of the invoice",
            },
            "poNumber": {
                "type": StringOrNothingType,
                "display_name": "PO Number",
                "short_desc": "The purchase order number",
                "desc": "The purchase order number associated with the invoice",
                "example_value": "PO-12345",
            },
            "dueDate": {
                "type": DateOrNothingType,
                "display_name": "Due Date",
                "short_desc": "The payment due date",
                "desc": "The date by which payment is due",
            },
            "pdfUrl": {
                "type": StringOrNothingType,
                "display_name": "PDF URL",
                "short_desc": "The PDF download URL",
                "desc": "The URL to download the invoice as a PDF",
            },
            "viewUrl": {
                "type": StringOrNothingType,
                "display_name": "View URL",
                "short_desc": "The invoice view URL",
                "desc": "The URL for the customer to view the invoice online",
            },
            "customer": {
                "type": new WaveCustomerRefDataType(),
                "display_name": "Customer",
                "short_desc": "The invoice customer",
                "desc": "The customer the invoice is for (id, name, email)",
            },
            "currency": {
                "type": new WaveCurrencyRefDataType(),
                "display_name": "Currency",
                "short_desc": "The invoice currency",
                "desc": "The currency of the invoice (code)",
            },
            "amountDue": {
                "type": new WaveMoneyRefDataType(),
                "display_name": "Amount Due",
                "short_desc": "The amount due",
                "desc": "The remaining amount due on the invoice (value, currency)",
            },
            "amountPaid": {
                "type": new WaveMoneyRefDataType(),
                "display_name": "Amount Paid",
                "short_desc": "The amount paid",
                "desc": "The total amount paid on the invoice (value, currency)",
            },
            "taxTotal": {
                "type": new WaveMoneyRefDataType(),
                "display_name": "Tax Total",
                "short_desc": "The total tax amount",
                "desc": "The total tax amount on the invoice (value, currency)",
            },
            "total": {
                "type": new WaveMoneyRefDataType(),
                "display_name": "Total",
                "short_desc": "The invoice total",
                "desc": "The total amount of the invoice including taxes (value, currency)",
            },
            "exchangeRate": {
                "type": FloatOrNothingType,
                "display_name": "Exchange Rate",
                "short_desc": "The exchange rate",
                "desc": "The exchange rate applied to the invoice",
                "example_value": 1.0,
            },
            "footer": {
                "type": StringOrNothingType,
                "display_name": "Footer",
                "short_desc": "The invoice footer",
                "desc": "The footer text displayed on the invoice",
                "example_value": "Thank you for your business!",
            },
            "memo": {
                "type": StringOrNothingType,
                "display_name": "Memo",
                "short_desc": "The invoice memo",
                "desc": "Internal memo for the invoice",
                "example_value": "Net 30 terms",
            },
            "disableCreditCardPayments": {
                "type": BoolOrNothingType,
                "display_name": "Disable Credit Card Payments",
                "short_desc": "Whether credit card payments are disabled",
                "desc": "If `True`, credit card payments are disabled for this invoice",
                "example_value": False,
            },
            "disableBankPayments": {
                "type": BoolOrNothingType,
                "display_name": "Disable Bank Payments",
                "short_desc": "Whether bank payments are disabled",
                "desc": "If `True`, bank payments are disabled for this invoice",
                "example_value": False,
            },
            "itemTitle": {
                "type": StringOrNothingType,
                "display_name": "Item Title",
                "short_desc": "The item column title",
                "desc": "Custom title for the item column on the invoice",
                "example_value": "Products & Services",
            },
            "unitTitle": {
                "type": StringOrNothingType,
                "display_name": "Unit Title",
                "short_desc": "The unit column title",
                "desc": "Custom title for the unit column on the invoice",
                "example_value": "Quantity",
            },
            "priceTitle": {
                "type": StringOrNothingType,
                "display_name": "Price Title",
                "short_desc": "The price column title",
                "desc": "Custom title for the price column on the invoice",
                "example_value": "Rate",
            },
            "amountTitle": {
                "type": StringOrNothingType,
                "display_name": "Amount Title",
                "short_desc": "The amount column title",
                "desc": "Custom title for the amount column on the invoice",
                "example_value": "Amount",
            },
            "hideName": {
                "type": BoolOrNothingType,
                "display_name": "Hide Name",
                "short_desc": "Whether to hide the item name",
                "desc": "If `True`, the item name column is hidden on the invoice",
                "example_value": False,
            },
            "hideDescription": {
                "type": BoolOrNothingType,
                "display_name": "Hide Description",
                "short_desc": "Whether to hide the description",
                "desc": "If `True`, the description column is hidden on the invoice",
                "example_value": False,
            },
            "hideUnit": {
                "type": BoolOrNothingType,
                "display_name": "Hide Unit",
                "short_desc": "Whether to hide the unit column",
                "desc": "If `True`, the unit/quantity column is hidden on the invoice",
                "example_value": False,
            },
            "hidePrice": {
                "type": BoolOrNothingType,
                "display_name": "Hide Price",
                "short_desc": "Whether to hide the price column",
                "desc": "If `True`, the price column is hidden on the invoice",
                "example_value": False,
            },
            "hideAmount": {
                "type": BoolOrNothingType,
                "display_name": "Hide Amount",
                "short_desc": "Whether to hide the amount column",
                "desc": "If `True`, the amount column is hidden on the invoice",
                "example_value": False,
            },
            "lastSentAt": {
                "type": DateOrNothingType,
                "display_name": "Last Sent At",
                "short_desc": "When the invoice was last sent",
                "desc": "The date and time when the invoice was last sent to the customer",
            },
            "lastSentVia": {
                "type": StringOrNothingType,
                "display_name": "Last Sent Via",
                "short_desc": "How the invoice was last sent",
                "desc": "The method used to last send the invoice (e.g., email)",
                "example_value": "email",
            },
            "lastViewedAt": {
                "type": DateOrNothingType,
                "display_name": "Last Viewed At",
                "short_desc": "When the invoice was last viewed",
                "desc": "The date and time when the invoice was last viewed by the customer",
            },
            "items": {
                "type": new ListDataType("InvoiceItemRefList", new WaveInvoiceItemRefDataType(), True),
                "display_name": "Items",
                "short_desc": "The invoice line items",
                "desc": "The line items on the invoice (description, quantity, unitPrice, subtotal, total, "
                    "product, account)",
            },
            "createdAt": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the invoice was created",
                "desc": "The date and time when the invoice was created",
            },
            "modifiedAt": {
                "type": DateOrNothingType,
                "display_name": "Modified At",
                "short_desc": "When the invoice was last modified",
                "desc": "The date and time when the invoice was last modified",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Wave invoice create input type
public class WaveInvoiceCreateDataType inherits HashDataType {
    private {
        #! Field definitions for creating invoices
        const Fields = {
            "businessId": {
                "type": StringType,
                "display_name": "Business ID",
                "short_desc": "The business ID (required)",
                "desc": "The ID of the business to create the invoice in",
                "required": True,
            },
            "customerId": {
                "type": StringType,
                "display_name": "Customer ID",
                "short_desc": "The customer ID (required)",
                "desc": "The ID of the customer the invoice is for",
                "required": True,
            },
            "items": {
                "type": new ListDataType("InvoiceCreateItemInputList", new WaveInvoiceCreateItemInputDataType(), True),
                "display_name": "Items",
                "short_desc": "The invoice line items",
                "desc": "The line items for the invoice (productId, description, quantity, unitPrice, taxIds)",
            },
            "invoiceDate": {
                "type": DateOrNothingType,
                "display_name": "Invoice Date",
                "short_desc": "The invoice date",
                "desc": "The date of the invoice (defaults to today if not specified)",
            },
            "dueDate": {
                "type": DateOrNothingType,
                "display_name": "Due Date",
                "short_desc": "The payment due date",
                "desc": "The date by which payment is due",
            },
            "currency": {
                "type": StringOrNothingType,
                "display_name": "Currency",
                "short_desc": "The currency code",
                "desc": "The ISO 4217 currency code for the invoice (e.g., USD, CAD)",
                "example_value": "USD",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The invoice status",
                "desc": "The initial status of the invoice",
                "example_value": "DRAFT",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "DRAFT", "desc": "Draft invoice"},
                    <AllowedValueInfo>{"value": "SENT", "desc": "Sent invoice"},
                ),
            },
            "memo": {
                "type": StringOrNothingType,
                "display_name": "Memo",
                "short_desc": "The invoice memo",
                "desc": "Internal memo for the invoice",
                "example_value": "Net 30 terms",
            },
            "footer": {
                "type": StringOrNothingType,
                "display_name": "Footer",
                "short_desc": "The invoice footer",
                "desc": "The footer text displayed on the invoice",
                "example_value": "Thank you for your business!",
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "The invoice title",
                "desc": "The title displayed on the invoice",
                "example_value": "Invoice",
            },
            "subhead": {
                "type": StringOrNothingType,
                "display_name": "Subhead",
                "short_desc": "The invoice subhead",
                "desc": "The subhead text displayed on the invoice",
                "example_value": "Consulting Services",
            },
            "poNumber": {
                "type": StringOrNothingType,
                "display_name": "PO Number",
                "short_desc": "The purchase order number",
                "desc": "The purchase order number associated with the invoice",
                "example_value": "PO-12345",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Wave invoice create item input type (for items in invoice create mutations)
public class WaveInvoiceCreateItemInputDataType inherits HashDataType {
    private {
        #! Field definitions for invoice create item inputs
        const Fields = {
            "productId": {
                "type": StringType,
                "display_name": "Product ID",
                "short_desc": "The product or service ID (required)",
                "desc": "The ID of the product or service for this line item",
                "required": True,
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The line item description",
                "desc": "The description for this line item",
                "example_value": "Web development services",
            },
            "quantity": {
                "type": FloatOrNothingType,
                "display_name": "Quantity",
                "short_desc": "The item quantity",
                "desc": "The quantity of the item (defaults to 1)",
                "example_value": 1.0,
            },
            "unitPrice": {
                "type": FloatOrNothingType,
                "display_name": "Unit Price",
                "short_desc": "The unit price",
                "desc": "The price per unit of the item (overrides the product default price)",
                "example_value": 150.0,
            },
            "taxIds": {
                "type": new ListDataType("TaxIdList", StringType, True),
                "display_name": "Tax IDs",
                "short_desc": "The sales tax IDs to apply",
                "desc": "A list of sales tax IDs to apply to this line item",
            },
        };
    }

    constructor() : HashDataType("InvoiceCreateItemInput") {
        addQoreFields(Fields);
    }
}

} # namespace WaveDataProvider
