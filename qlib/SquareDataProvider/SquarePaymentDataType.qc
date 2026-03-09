# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SquareDataProvider module definition

/*  SquarePaymentDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Square payment record type
public class SquarePaymentDataType inherits HashDataType {
    public {
        #! Field definitions for Square payments
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The payment ID",
                "desc": "The unique identifier of the payment",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the payment was created",
                "desc": "The timestamp when the payment was created (RFC 3339 format)",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the payment was last updated",
                "desc": "The timestamp when the payment was last updated (RFC 3339 format)",
            },
            "amount_money": {
                "type": AutoHashOrNothingType,
                "display_name": "Amount",
                "short_desc": "The payment amount",
                "desc": "The payment amount (hash with amount in cents and currency)",
            },
            "tip_money": {
                "type": AutoHashOrNothingType,
                "display_name": "Tip Amount",
                "short_desc": "The tip amount",
                "desc": "The tip amount (hash with amount in cents and currency)",
            },
            "total_money": {
                "type": AutoHashOrNothingType,
                "display_name": "Total Amount",
                "short_desc": "The total payment amount",
                "desc": "The total payment amount including tips (hash with amount in cents and currency)",
            },
            "app_fee_money": {
                "type": AutoHashOrNothingType,
                "display_name": "App Fee",
                "short_desc": "The application fee",
                "desc": "The application fee amount (hash with amount in cents and currency)",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The payment status",
                "desc": "The current status of the payment (APPROVED, PENDING, COMPLETED, "
                    "CANCELED, FAILED)",
            },
            "source_type": {
                "type": StringOrNothingType,
                "display_name": "Source Type",
                "short_desc": "The payment source type",
                "desc": "The type of payment source (CARD, BANK_ACCOUNT, WALLET, CASH, "
                    "EXTERNAL, etc.)",
            },
            "card_details": {
                "type": AutoHashOrNothingType,
                "display_name": "Card Details",
                "short_desc": "Card payment details",
                "desc": "Details about the card used for payment (hash with status, card, "
                    "entry_method, cvv_status, avs_status)",
            },
            "location_id": {
                "type": StringOrNothingType,
                "display_name": "Location ID",
                "short_desc": "The location ID",
                "desc": "The ID of the location where the payment was processed",
            },
            "order_id": {
                "type": StringOrNothingType,
                "display_name": "Order ID",
                "short_desc": "The associated order ID",
                "desc": "The ID of the order associated with this payment",
            },
            "customer_id": {
                "type": StringOrNothingType,
                "display_name": "Customer ID",
                "short_desc": "The customer ID",
                "desc": "The ID of the customer associated with this payment",
            },
            "reference_id": {
                "type": StringOrNothingType,
                "display_name": "Reference ID",
                "short_desc": "External reference ID",
                "desc": "An optional reference ID from an external system",
            },
            "receipt_number": {
                "type": StringOrNothingType,
                "display_name": "Receipt Number",
                "short_desc": "The receipt number",
                "desc": "The receipt number for the payment",
            },
            "receipt_url": {
                "type": StringOrNothingType,
                "display_name": "Receipt URL",
                "short_desc": "The receipt URL",
                "desc": "The URL for the payment receipt",
            },
            "refund_ids": {
                "type": AutoListOrNothingType,
                "display_name": "Refund IDs",
                "short_desc": "Associated refund IDs",
                "desc": "List of refund IDs associated with this payment",
            },
            "version_token": {
                "type": StringOrNothingType,
                "display_name": "Version Token",
                "short_desc": "The version token",
                "desc": "The version token for optimistic concurrency on complete/cancel",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SquareDataProvider
