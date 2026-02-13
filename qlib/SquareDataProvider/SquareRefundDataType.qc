# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SquareDataProvider module definition

/*  SquareRefundDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Square refund record type
public class SquareRefundDataType inherits HashDataType {
    private {
        #! Field definitions for Square refunds
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The refund ID",
                "desc": "The unique identifier of the refund",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The refund status",
                "desc": "The current status of the refund (PENDING, COMPLETED, REJECTED, FAILED)",
            },
            "amount_money": {
                "type": AutoHashOrNothingType,
                "display_name": "Amount",
                "short_desc": "The refund amount",
                "desc": "The refund amount (hash with amount in cents and currency)",
            },
            "payment_id": {
                "type": StringOrNothingType,
                "display_name": "Payment ID",
                "short_desc": "The associated payment ID",
                "desc": "The ID of the payment being refunded",
            },
            "order_id": {
                "type": StringOrNothingType,
                "display_name": "Order ID",
                "short_desc": "The associated order ID",
                "desc": "The ID of the order associated with this refund",
            },
            "reason": {
                "type": StringOrNothingType,
                "display_name": "Reason",
                "short_desc": "The refund reason",
                "desc": "The reason for the refund",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the refund was created",
                "desc": "The timestamp when the refund was created (RFC 3339 format)",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the refund was last updated",
                "desc": "The timestamp when the refund was last updated (RFC 3339 format)",
            },
            "location_id": {
                "type": StringOrNothingType,
                "display_name": "Location ID",
                "short_desc": "The location ID",
                "desc": "The ID of the location where the refund was processed",
            },
            "processing_fee": {
                "type": AutoListOrNothingType,
                "display_name": "Processing Fees",
                "short_desc": "Processing fee details",
                "desc": "List of processing fees associated with the refund",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SquareDataProvider
