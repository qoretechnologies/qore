# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ChargebeeDataProvider module definition

/*  ChargebeeTransactionDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Chargebee transaction record type
/** Represents a transaction in Chargebee.
*/
public class ChargebeeTransactionDataType inherits HashDataType {
    private {
        #! Field definitions for Chargebee transactions
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Transaction ID",
                "short_desc": "Unique transaction identifier",
                "desc": "The unique identifier for the transaction in Chargebee",
                "required": True,
            },
            "customer_id": {
                "type": StringOrNothingType,
                "display_name": "Customer ID",
                "short_desc": "Associated customer identifier",
                "desc": "The identifier of the customer associated with this transaction",
            },
            "subscription_id": {
                "type": StringOrNothingType,
                "display_name": "Subscription ID",
                "short_desc": "Associated subscription identifier",
                "desc": "The identifier of the subscription associated with this transaction",
            },
            "gateway_account_id": {
                "type": StringOrNothingType,
                "display_name": "Gateway Account ID",
                "short_desc": "Payment gateway account identifier",
                "desc": "The identifier of the payment gateway account used for the transaction",
            },
            "payment_source_id": {
                "type": StringOrNothingType,
                "display_name": "Payment Source ID",
                "short_desc": "Payment source identifier",
                "desc": "The identifier of the payment source used for the transaction",
            },
            "payment_method": {
                "type": StringOrNothingType,
                "display_name": "Payment Method",
                "short_desc": "Method of payment",
                "desc": "The payment method used for this transaction",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "card",
                        "display_name": "Card",
                        "desc": "Payment made by credit or debit card",
                    },
                    <AllowedValueInfo>{
                        "value": "cash",
                        "display_name": "Cash",
                        "desc": "Payment made in cash",
                    },
                    <AllowedValueInfo>{
                        "value": "check",
                        "display_name": "Check",
                        "desc": "Payment made by check",
                    },
                    <AllowedValueInfo>{
                        "value": "bank_transfer",
                        "display_name": "Bank Transfer",
                        "desc": "Payment made via bank transfer",
                    },
                    <AllowedValueInfo>{
                        "value": "other",
                        "display_name": "Other",
                        "desc": "Payment made by other means",
                    },
                ),
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "Transaction type",
                "desc": "The type of transaction",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "authorization",
                        "display_name": "Authorization",
                        "desc": "An authorization hold on the payment method",
                    },
                    <AllowedValueInfo>{
                        "value": "payment",
                        "display_name": "Payment",
                        "desc": "A payment transaction",
                    },
                    <AllowedValueInfo>{
                        "value": "refund",
                        "display_name": "Refund",
                        "desc": "A refund transaction",
                    },
                    <AllowedValueInfo>{
                        "value": "payment_reversal",
                        "display_name": "Payment Reversal",
                        "desc": "A reversal of a previous payment",
                    },
                ),
            },
            "date": {
                "type": IntOrNothingType,
                "display_name": "Date",
                "short_desc": "Transaction date",
                "desc": "Unix timestamp (in seconds) of the transaction date",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Current transaction status",
                "desc": "The current status of the transaction",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "in_progress",
                        "display_name": "In Progress",
                        "desc": "Transaction is currently being processed",
                    },
                    <AllowedValueInfo>{
                        "value": "success",
                        "display_name": "Success",
                        "desc": "Transaction completed successfully",
                    },
                    <AllowedValueInfo>{
                        "value": "voided",
                        "display_name": "Voided",
                        "desc": "Transaction has been voided",
                    },
                    <AllowedValueInfo>{
                        "value": "failure",
                        "display_name": "Failure",
                        "desc": "Transaction failed",
                    },
                    <AllowedValueInfo>{
                        "value": "timeout",
                        "display_name": "Timeout",
                        "desc": "Transaction timed out",
                    },
                    <AllowedValueInfo>{
                        "value": "needs_attention",
                        "display_name": "Needs Attention",
                        "desc": "Transaction requires manual attention",
                    },
                ),
            },
            "amount": {
                "type": IntOrNothingType,
                "display_name": "Amount",
                "short_desc": "Transaction amount",
                "desc": "The amount of the transaction (in cents)",
            },
            "currency_code": {
                "type": StringOrNothingType,
                "display_name": "Currency Code",
                "short_desc": "Transaction currency",
                "desc": "The ISO 4217 currency code for the transaction",
            },
            "gateway": {
                "type": StringOrNothingType,
                "display_name": "Gateway",
                "short_desc": "Payment gateway",
                "desc": "The payment gateway used for the transaction",
            },
            "id_at_gateway": {
                "type": StringOrNothingType,
                "display_name": "ID at Gateway",
                "short_desc": "Gateway transaction identifier",
                "desc": "The identifier of this transaction at the payment gateway",
            },
            "updated_at": {
                "type": IntOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "Unix timestamp (in seconds) when the transaction was last updated",
            },
            "resource_version": {
                "type": IntOrNothingType,
                "display_name": "Resource Version",
                "short_desc": "Version number for concurrency control",
                "desc": "An incrementing version number used for concurrency control",
            },
            "deleted": {
                "type": SoftBoolOrNothingType,
                "display_name": "Deleted",
                "short_desc": "Whether the transaction is deleted",
                "desc": "Indicates whether the transaction has been deleted",
            },
            "linked_invoices": {
                "type": SoftListOrNothingType,
                "display_name": "Linked Invoices",
                "short_desc": "Invoices linked to this transaction",
                "desc": "The list of invoices linked to this transaction",
            },
            "linked_refunds": {
                "type": SoftListOrNothingType,
                "display_name": "Linked Refunds",
                "short_desc": "Refunds linked to this transaction",
                "desc": "The list of refunds linked to this transaction",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ChargebeeDataProvider
