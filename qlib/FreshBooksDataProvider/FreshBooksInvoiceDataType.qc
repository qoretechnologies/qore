# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FreshBooksDataProvider module definition

/*  FreshBooksInvoiceDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FreshBooksDataProvider module
public namespace FreshBooksDataProvider {

#! FreshBooks invoice line item type
public class FreshBooksInvoiceLineDataType inherits HashDataType {
    private {
        #! Field definitions for FreshBooks invoice line items
        const Fields = {
            "lineid": {
                "type": IntOrNothingType,
                "display_name": "Line ID",
                "short_desc": "The line item ID",
                "desc": "The unique identifier of the invoice line item",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The line description",
                "desc": "The description of the invoice line item",
            },
            "amount": {
                "type": AutoHashType,
                "display_name": "Amount",
                "short_desc": "The line amount",
                "desc": "The total amount of the line item (hash with 'amount' and 'code' keys)",
            },
            "quantity": {
                "type": StringOrNothingType,
                "display_name": "Quantity",
                "short_desc": "The quantity",
                "desc": "The quantity of the line item (string in FreshBooks API)",
            },
            "unit_cost": {
                "type": AutoHashType,
                "display_name": "Unit Cost",
                "short_desc": "The unit cost",
                "desc": "The unit cost of the line item (hash with 'amount' and 'code' keys)",
            },
            "taxName1": {
                "type": StringOrNothingType,
                "display_name": "Tax Name 1",
                "short_desc": "The first tax name",
                "desc": "The name of the first tax applied to the line item",
            },
            "taxPercent1": {
                "type": StringOrNothingType,
                "display_name": "Tax Percent 1",
                "short_desc": "The first tax percentage",
                "desc": "The percentage of the first tax applied to the line item",
            },
            "taxAmount1": {
                "type": StringOrNothingType,
                "display_name": "Tax Amount 1",
                "short_desc": "The first tax amount",
                "desc": "The amount of the first tax applied to the line item",
            },
            "taxName2": {
                "type": StringOrNothingType,
                "display_name": "Tax Name 2",
                "short_desc": "The second tax name",
                "desc": "The name of the second tax applied to the line item",
            },
            "taxPercent2": {
                "type": StringOrNothingType,
                "display_name": "Tax Percent 2",
                "short_desc": "The second tax percentage",
                "desc": "The percentage of the second tax applied to the line item",
            },
            "taxAmount2": {
                "type": StringOrNothingType,
                "display_name": "Tax Amount 2",
                "short_desc": "The second tax amount",
                "desc": "The amount of the second tax applied to the line item",
            },
            "type": {
                "type": IntOrNothingType,
                "display_name": "Type",
                "short_desc": "The line type",
                "desc": "The type of the line item (0=normal, 1=rebilling)",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! FreshBooks invoice record type
public class FreshBooksInvoiceDataType inherits HashDataType {
    private {
        #! Field definitions for FreshBooks invoices
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The invoice ID",
                "desc": "The unique identifier of the invoice",
            },
            "invoiceid": {
                "type": IntType,
                "display_name": "Invoice ID",
                "short_desc": "The internal invoice ID",
                "desc": "The internal invoice ID",
            },
            "invoice_number": {
                "type": StringOrNothingType,
                "display_name": "Invoice Number",
                "short_desc": "The invoice number",
                "desc": "The invoice number for reference",
                "example_value": "INV-001",
            },
            "customerid": {
                "type": IntType,
                "display_name": "Customer ID",
                "short_desc": "The customer ID",
                "desc": "The ID of the customer the invoice is for",
            },
            "create_date": {
                "type": StringOrNothingType,
                "display_name": "Create Date",
                "short_desc": "The creation date",
                "desc": "The date the invoice was created (YYYY-MM-DD format)",
            },
            "due_offset_days": {
                "type": IntOrNothingType,
                "display_name": "Due Offset Days",
                "short_desc": "Days until due",
                "desc": "The number of days from the creation date until the invoice is due",
            },
            "currency_code": {
                "type": StringOrNothingType,
                "display_name": "Currency Code",
                "short_desc": "The currency code",
                "desc": "The ISO 4217 currency code for the invoice",
                "example_value": "USD",
            },
            "language": {
                "type": StringOrNothingType,
                "display_name": "Language",
                "short_desc": "The language code",
                "desc": "The language code for the invoice",
            },
            "status": {
                "type": IntOrNothingType,
                "display_name": "Status",
                "short_desc": "The invoice status",
                "desc": "The status of the invoice",
            },
            "amount": {
                "type": AutoHashType,
                "display_name": "Amount",
                "short_desc": "The total amount",
                "desc": "The total amount of the invoice (hash with 'amount' and 'code' keys)",
            },
            "outstanding": {
                "type": AutoHashType,
                "display_name": "Outstanding",
                "short_desc": "The outstanding amount",
                "desc": "The outstanding amount of the invoice (hash with 'amount' and 'code' keys)",
            },
            "discount_value": {
                "type": StringOrNothingType,
                "display_name": "Discount Value",
                "short_desc": "The discount value",
                "desc": "The discount value applied to the invoice",
            },
            "notes": {
                "type": StringOrNothingType,
                "display_name": "Notes",
                "short_desc": "The invoice notes",
                "desc": "Notes displayed on the invoice",
            },
            "terms": {
                "type": StringOrNothingType,
                "display_name": "Terms",
                "short_desc": "The invoice terms",
                "desc": "The payment terms displayed on the invoice",
            },
            "po_number": {
                "type": StringOrNothingType,
                "display_name": "PO Number",
                "short_desc": "The purchase order number",
                "desc": "The purchase order number associated with the invoice",
            },
            "vis_state": {
                "type": IntOrNothingType,
                "display_name": "Visibility State",
                "short_desc": "The visibility state",
                "desc": "The visibility state of the invoice (0=active, 1=deleted, 2=archived)",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! FreshBooks invoice create request type
public class FreshBooksInvoiceCreateDataType inherits HashDataType {
    private {
        #! Field definitions for creating invoices
        const Fields = FreshBooksDataProviderBase::AccountIdField + {
            "customerid": {
                "type": IntType,
                "display_name": "Customer ID",
                "short_desc": "The customer ID (required)",
                "desc": "The ID of the customer the invoice is for",
                "required": True,
            },
            "invoice_number": {
                "type": StringOrNothingType,
                "display_name": "Invoice Number",
                "short_desc": "The invoice number",
                "desc": "The invoice number for reference",
                "example_value": "INV-001",
            },
            "create_date": {
                "type": StringOrNothingType,
                "display_name": "Create Date",
                "short_desc": "The creation date",
                "desc": "The date the invoice was created (YYYY-MM-DD format)",
            },
            "due_offset_days": {
                "type": IntOrNothingType,
                "display_name": "Due Offset Days",
                "short_desc": "Days until due",
                "desc": "The number of days from the creation date until the invoice is due",
            },
            "currency_code": {
                "type": StringOrNothingType,
                "display_name": "Currency Code",
                "short_desc": "The currency code",
                "desc": "The ISO 4217 currency code for the invoice",
                "example_value": "USD",
            },
            "language": {
                "type": StringOrNothingType,
                "display_name": "Language",
                "short_desc": "The language code",
                "desc": "The language code for the invoice",
            },
            "discount_value": {
                "type": StringOrNothingType,
                "display_name": "Discount Value",
                "short_desc": "The discount value",
                "desc": "The discount value to apply to the invoice",
            },
            "notes": {
                "type": StringOrNothingType,
                "display_name": "Notes",
                "short_desc": "The invoice notes",
                "desc": "Notes to display on the invoice",
            },
            "terms": {
                "type": StringOrNothingType,
                "display_name": "Terms",
                "short_desc": "The invoice terms",
                "desc": "The payment terms to display on the invoice",
            },
            "po_number": {
                "type": StringOrNothingType,
                "display_name": "PO Number",
                "short_desc": "The purchase order number",
                "desc": "The purchase order number to associate with the invoice",
            },
            "lines": {
                "type": AutoListOrNothingType,
                "display_name": "Line Items",
                "short_desc": "Invoice line items",
                "desc": "List of line items, each with fields: description, qty, unit_cost "
                    "(hash with 'amount' and 'code'), name, taxName1, taxAmount1, taxName2, taxAmount2",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! FreshBooks invoice update request type
public class FreshBooksInvoiceUpdateDataType inherits HashDataType {
    private {
        #! Field definitions for updating invoices
        const Fields = FreshBooksDataProviderBase::AccountIdField + {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The invoice ID (required)",
                "desc": "The unique identifier of the invoice to update",
                "required": True,
            },
            "customerid": {
                "type": IntOrNothingType,
                "display_name": "Customer ID",
                "short_desc": "The customer ID",
                "desc": "The new customer ID for the invoice",
            },
            "invoice_number": {
                "type": StringOrNothingType,
                "display_name": "Invoice Number",
                "short_desc": "The invoice number",
                "desc": "The new invoice number for reference",
                "example_value": "INV-001",
            },
            "create_date": {
                "type": StringOrNothingType,
                "display_name": "Create Date",
                "short_desc": "The creation date",
                "desc": "The new creation date (YYYY-MM-DD format)",
            },
            "due_offset_days": {
                "type": IntOrNothingType,
                "display_name": "Due Offset Days",
                "short_desc": "Days until due",
                "desc": "The new number of days from the creation date until the invoice is due",
            },
            "currency_code": {
                "type": StringOrNothingType,
                "display_name": "Currency Code",
                "short_desc": "The currency code",
                "desc": "The new ISO 4217 currency code for the invoice",
                "example_value": "USD",
            },
            "language": {
                "type": StringOrNothingType,
                "display_name": "Language",
                "short_desc": "The language code",
                "desc": "The new language code for the invoice",
            },
            "discount_value": {
                "type": StringOrNothingType,
                "display_name": "Discount Value",
                "short_desc": "The discount value",
                "desc": "The new discount value for the invoice",
            },
            "notes": {
                "type": StringOrNothingType,
                "display_name": "Notes",
                "short_desc": "The invoice notes",
                "desc": "The new notes for the invoice",
            },
            "terms": {
                "type": StringOrNothingType,
                "display_name": "Terms",
                "short_desc": "The invoice terms",
                "desc": "The new payment terms for the invoice",
            },
            "po_number": {
                "type": StringOrNothingType,
                "display_name": "PO Number",
                "short_desc": "The purchase order number",
                "desc": "The new purchase order number for the invoice",
            },
            "lines": {
                "type": AutoListOrNothingType,
                "display_name": "Line Items",
                "short_desc": "Invoice line items",
                "desc": "List of line items (replaces existing lines - include ALL desired lines). "
                    "Each with fields: description, qty, unit_cost (hash with 'amount' and 'code'), "
                    "name, taxName1, taxAmount1, taxName2, taxAmount2",
            },
            "vis_state": {
                "type": IntOrNothingType,
                "display_name": "Visibility State",
                "short_desc": "The visibility state",
                "desc": "The new visibility state of the invoice (0=active, 1=deleted, 2=archived)",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace FreshBooksDataProvider
