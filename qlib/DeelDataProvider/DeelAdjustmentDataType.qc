# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelAdjustmentDataType.qc Deel invoice adjustment record data type

/*  DeelAdjustmentDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DeelDataProvider module
public namespace DeelDataProvider {

#! Deel invoice adjustment record type
public class DeelAdjustmentDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftStringType,
                "display_name": "ID",
                "short_desc": "The adjustment ID",
                "desc": "The unique identifier of the invoice adjustment",
            },
            "contract_id": {
                "type": StringOrNothingType,
                "display_name": "Contract ID",
                "short_desc": "The associated contract ID",
                "desc": "The ID of the contract this adjustment belongs to",
            },
            "amount": {
                "type": NumberOrNothingType,
                "display_name": "Amount",
                "short_desc": "The adjustment amount",
                "desc": "The monetary amount of the adjustment",
                "example_value": 100.00,
            },
            "currency": {
                "type": StringOrNothingType,
                "display_name": "Currency",
                "short_desc": "The adjustment currency",
                "desc": "The ISO 4217 currency code for the adjustment",
                "example_value": "USD",
            },
            "reason": {
                "type": StringOrNothingType,
                "display_name": "Reason",
                "short_desc": "Reason for the adjustment",
                "desc": "An explanation of why this adjustment was made",
            },
            "adjustment_type": {
                "type": StringOrNothingType,
                "display_name": "Adjustment Type",
                "short_desc": "Type of adjustment",
                "desc": "The type of invoice adjustment "
                    "(e.g., `bonus`, `expense`, `deduction`)",
                "example_value": "bonus",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The adjustment status",
                "desc": "The current status of the adjustment "
                    "(e.g., `pending`, `approved`, `declined`)",
                "example_value": "pending",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the adjustment was created",
                "desc": "The timestamp when the adjustment was created (ISO 8601 format)",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the adjustment was last updated",
                "desc": "The timestamp when the adjustment was last updated (ISO 8601 format)",
            },
            "requester": {
                "type": AutoHashOrNothingType,
                "display_name": "Requester",
                "short_desc": "Who requested the adjustment",
                "desc": "Information about the person who created the adjustment",
            },
            "reviewer": {
                "type": AutoHashOrNothingType,
                "display_name": "Reviewer",
                "short_desc": "Who reviewed the adjustment",
                "desc": "Information about who reviewed (approved/declined) the adjustment",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace DeelDataProvider
