# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ChargebeeDataProvider module definition

/*  ChargebeeInvoiceDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ChargebeeDataProvider module
public namespace ChargebeeDataProvider {

#! Chargebee invoice record type
/** Represents an invoice in Chargebee.
*/
public class ChargebeeInvoiceDataType inherits HashDataType {
    private {
        #! Field definitions for Chargebee invoices
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Invoice ID",
                "short_desc": "Unique invoice identifier",
                "desc": "The unique identifier for the invoice in Chargebee",
                "required": True,
            },
            "customer_id": {
                "type": StringType,
                "display_name": "Customer ID",
                "short_desc": "Associated customer identifier",
                "desc": "The identifier of the customer associated with this invoice",
                "required": True,
            },
            "subscription_id": {
                "type": StringOrNothingType,
                "display_name": "Subscription ID",
                "short_desc": "Associated subscription identifier",
                "desc": "The identifier of the subscription associated with this invoice",
            },
            "recurring": {
                "type": SoftBoolOrNothingType,
                "display_name": "Recurring",
                "short_desc": "Whether the invoice is recurring",
                "desc": "Indicates whether this is a recurring invoice",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Current invoice status",
                "desc": "The current status of the invoice",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "paid",
                        "display_name": "Paid",
                        "desc": "Invoice has been fully paid",
                    },
                    <AllowedValueInfo>{
                        "value": "posted",
                        "display_name": "Posted",
                        "desc": "Invoice has been posted",
                    },
                    <AllowedValueInfo>{
                        "value": "payment_due",
                        "display_name": "Payment Due",
                        "desc": "Invoice is awaiting payment",
                    },
                    <AllowedValueInfo>{
                        "value": "not_paid",
                        "display_name": "Not Paid",
                        "desc": "Invoice payment has failed or was not completed",
                    },
                    <AllowedValueInfo>{
                        "value": "voided",
                        "display_name": "Voided",
                        "desc": "Invoice has been voided",
                    },
                    <AllowedValueInfo>{
                        "value": "pending",
                        "display_name": "Pending",
                        "desc": "Invoice is pending finalization",
                    },
                ),
            },
            "price_type": {
                "type": StringOrNothingType,
                "display_name": "Price Type",
                "short_desc": "Tax inclusion in price",
                "desc": "Whether prices are tax-inclusive or tax-exclusive",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "tax_exclusive",
                        "display_name": "Tax Exclusive",
                        "desc": "Prices do not include tax",
                    },
                    <AllowedValueInfo>{
                        "value": "tax_inclusive",
                        "display_name": "Tax Inclusive",
                        "desc": "Prices include tax",
                    },
                ),
            },
            "date": {
                "type": IntOrNothingType,
                "display_name": "Date",
                "short_desc": "Invoice date",
                "desc": "Unix timestamp (in seconds) of the invoice date",
            },
            "due_date": {
                "type": IntOrNothingType,
                "display_name": "Due Date",
                "short_desc": "Payment due date",
                "desc": "Unix timestamp (in seconds) when payment is due",
            },
            "currency_code": {
                "type": StringOrNothingType,
                "display_name": "Currency Code",
                "short_desc": "Invoice currency",
                "desc": "The ISO 4217 currency code for the invoice",
            },
            "total": {
                "type": IntOrNothingType,
                "display_name": "Total",
                "short_desc": "Total invoice amount",
                "desc": "The total amount of the invoice (in cents)",
            },
            "amount_paid": {
                "type": IntOrNothingType,
                "display_name": "Amount Paid",
                "short_desc": "Amount already paid",
                "desc": "The amount that has been paid on this invoice (in cents)",
            },
            "amount_adjusted": {
                "type": IntOrNothingType,
                "display_name": "Amount Adjusted",
                "short_desc": "Amount adjusted via credits",
                "desc": "The amount adjusted via credits on this invoice (in cents)",
            },
            "amount_due": {
                "type": IntOrNothingType,
                "display_name": "Amount Due",
                "short_desc": "Amount still owed",
                "desc": "The outstanding amount due on this invoice (in cents)",
            },
            "credits_applied": {
                "type": IntOrNothingType,
                "display_name": "Credits Applied",
                "short_desc": "Credits applied to invoice",
                "desc": "The total credits applied to this invoice (in cents)",
            },
            "write_off_amount": {
                "type": IntOrNothingType,
                "display_name": "Write-Off Amount",
                "short_desc": "Amount written off",
                "desc": "The amount written off on this invoice (in cents)",
            },
            "sub_total": {
                "type": IntOrNothingType,
                "display_name": "Subtotal",
                "short_desc": "Invoice subtotal before tax",
                "desc": "The subtotal of the invoice before taxes (in cents)",
            },
            "tax": {
                "type": IntOrNothingType,
                "display_name": "Tax",
                "short_desc": "Total tax amount",
                "desc": "The total tax amount on the invoice (in cents)",
            },
            "net_term_days": {
                "type": IntOrNothingType,
                "display_name": "Net Term Days",
                "short_desc": "Payment net term in days",
                "desc": "The number of days within which payment is due",
            },
            "dunning_status": {
                "type": StringOrNothingType,
                "display_name": "Dunning Status",
                "short_desc": "Current dunning status",
                "desc": "The current status of the dunning process for this invoice",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "in_progress",
                        "display_name": "In Progress",
                        "desc": "Dunning process is actively running",
                    },
                    <AllowedValueInfo>{
                        "value": "exhausted",
                        "display_name": "Exhausted",
                        "desc": "All dunning attempts have been exhausted",
                    },
                    <AllowedValueInfo>{
                        "value": "stopped",
                        "display_name": "Stopped",
                        "desc": "Dunning has been manually stopped",
                    },
                    <AllowedValueInfo>{
                        "value": "success",
                        "display_name": "Success",
                        "desc": "Payment was successfully collected during dunning",
                    },
                ),
            },
            "paid_at": {
                "type": IntOrNothingType,
                "display_name": "Paid At",
                "short_desc": "Payment timestamp",
                "desc": "Unix timestamp (in seconds) when the invoice was fully paid",
            },
            "voided_at": {
                "type": IntOrNothingType,
                "display_name": "Voided At",
                "short_desc": "Void timestamp",
                "desc": "Unix timestamp (in seconds) when the invoice was voided",
            },
            "deleted": {
                "type": SoftBoolOrNothingType,
                "display_name": "Deleted",
                "short_desc": "Whether the invoice is deleted",
                "desc": "Indicates whether the invoice has been deleted",
            },
            "first_invoice": {
                "type": SoftBoolOrNothingType,
                "display_name": "First Invoice",
                "short_desc": "Whether this is the first invoice",
                "desc": "Indicates whether this is the first invoice for the subscription",
            },
            "created_at": {
                "type": IntOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "Unix timestamp (in seconds) when the invoice was created",
            },
            "updated_at": {
                "type": IntOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "Unix timestamp (in seconds) when the invoice was last updated",
            },
            "resource_version": {
                "type": IntOrNothingType,
                "display_name": "Resource Version",
                "short_desc": "Version number for concurrency control",
                "desc": "An incrementing version number used for concurrency control",
            },
            "channel": {
                "type": StringOrNothingType,
                "display_name": "Channel",
                "short_desc": "Sales channel",
                "desc": "The channel through which the invoice was generated",
            },
            "billing_address": {
                "type": AutoHashOrNothingType,
                "display_name": "Billing Address",
                "short_desc": "Invoice billing address",
                "desc": "The billing address on the invoice",
            },
            "shipping_address": {
                "type": AutoHashOrNothingType,
                "display_name": "Shipping Address",
                "short_desc": "Invoice shipping address",
                "desc": "The shipping address on the invoice",
            },
            "line_items": {
                "type": SoftListOrNothingType,
                "display_name": "Line Items",
                "short_desc": "Invoice line items",
                "desc": "The list of line items on the invoice",
            },
            "linked_payments": {
                "type": SoftListOrNothingType,
                "display_name": "Linked Payments",
                "short_desc": "Payments linked to this invoice",
                "desc": "The list of payments linked to this invoice",
            },
            "notes": {
                "type": SoftListOrNothingType,
                "display_name": "Notes",
                "short_desc": "Invoice notes",
                "desc": "Notes associated with the invoice",
            },
            "meta_data": {
                "type": AutoHashOrNothingType,
                "display_name": "Metadata",
                "short_desc": "Custom metadata",
                "desc": "A set of key-value pairs stored as additional information for the invoice",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ChargebeeDataProvider
