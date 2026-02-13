# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SquareDataProvider module definition

/*  SquareInvoiceDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Square invoice record type
public class SquareInvoiceDataType inherits HashDataType {
    private {
        #! Field definitions for Square invoices
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The invoice ID",
                "desc": "The unique identifier of the invoice",
            },
            "version": {
                "type": IntOrNothingType,
                "display_name": "Version",
                "short_desc": "The invoice version",
                "desc": "The current version of the invoice (for optimistic concurrency)",
            },
            "location_id": {
                "type": StringOrNothingType,
                "display_name": "Location ID",
                "short_desc": "The location ID",
                "desc": "The ID of the location where the invoice was created",
            },
            "order_id": {
                "type": StringOrNothingType,
                "display_name": "Order ID",
                "short_desc": "The associated order ID",
                "desc": "The ID of the order associated with this invoice",
            },
            "primary_recipient": {
                "type": AutoHashOrNothingType,
                "display_name": "Primary Recipient",
                "short_desc": "The invoice recipient",
                "desc": "The primary recipient of the invoice (hash with customer_id, "
                    "given_name, family_name, email_address, phone_number, company_name)",
            },
            "payment_requests": {
                "type": AutoListOrNothingType,
                "display_name": "Payment Requests",
                "short_desc": "Payment requests on the invoice",
                "desc": "List of payment requests specifying how the invoice should be paid "
                    "(each with uid, request_type, due_date, computed_amount_money, etc.)",
            },
            "invoice_number": {
                "type": StringOrNothingType,
                "display_name": "Invoice Number",
                "short_desc": "The invoice number",
                "desc": "A user-defined invoice number",
                "example_value": "INV-001",
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "The invoice title",
                "desc": "The title of the invoice",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The invoice description",
                "desc": "A description of the invoice",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The invoice status",
                "desc": "The current status of the invoice (DRAFT, UNPAID, SCHEDULED, "
                    "PARTIALLY_PAID, PAID, PARTIALLY_REFUNDED, REFUNDED, CANCELED, FAILED, "
                    "PAYMENT_PENDING)",
            },
            "delivery_method": {
                "type": StringOrNothingType,
                "display_name": "Delivery Method",
                "short_desc": "How the invoice is delivered",
                "desc": "The delivery method for the invoice (EMAIL, SHARE_MANUALLY, SMS)",
            },
            "scheduled_at": {
                "type": StringOrNothingType,
                "display_name": "Scheduled At",
                "short_desc": "When the invoice is scheduled",
                "desc": "The date the invoice is scheduled to be sent (RFC 3339 format)",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the invoice was created",
                "desc": "The timestamp when the invoice was created (RFC 3339 format)",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the invoice was last updated",
                "desc": "The timestamp when the invoice was last updated (RFC 3339 format)",
            },
            "public_url": {
                "type": StringOrNothingType,
                "display_name": "Public URL",
                "short_desc": "The public URL for the invoice",
                "desc": "The URL where the customer can view and pay the invoice",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SquareDataProvider
