# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WaveDataProvider module definition

/*  WaveTransactionDataType.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Wave transaction record type
public class WaveTransactionDataType inherits HashDataType {
    private {
        #! Field definitions for Wave transactions
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The unique identifier of the transaction",
                "desc": "The unique identifier of the transaction",
            },
            "date": {
                "type": DateOrNothingType,
                "display_name": "Date",
                "short_desc": "The transaction date",
                "desc": "The date of the transaction",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The transaction description",
                "desc": "The description of the transaction",
                "example_value": "Office supplies purchase",
            },
            "postedOn": {
                "type": DateOrNothingType,
                "display_name": "Posted On",
                "short_desc": "When the transaction was posted",
                "desc": "The date when the transaction was posted to the ledger",
            },
            "createdAt": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the transaction was created",
                "desc": "The date and time when the transaction was created",
            },
            "modifiedAt": {
                "type": DateOrNothingType,
                "display_name": "Modified At",
                "short_desc": "When the transaction was last modified",
                "desc": "The date and time when the transaction was last modified",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Wave transaction create input type
public class WaveTransactionCreateDataType inherits HashDataType {
    private {
        #! Field definitions for creating transactions
        const Fields = {
            "businessId": {
                "type": StringType,
                "display_name": "Business ID",
                "short_desc": "The business ID (required)",
                "desc": "The ID of the business to create the transaction in",
                "required": True,
            },
            "externalId": {
                "type": StringType,
                "display_name": "External ID",
                "short_desc": "The external ID (required)",
                "desc": "A unique external identifier for the transaction (for idempotency)",
                "example_value": "txn-12345",
                "required": True,
            },
            "date": {
                "type": DateOrNothingType,
                "display_name": "Date",
                "short_desc": "The transaction date",
                "desc": "The date of the transaction",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The transaction description",
                "desc": "The description of the transaction",
                "example_value": "Office supplies purchase",
            },
            "lineItems": {
                "type": AutoListOrNothingType,
                "display_name": "Line Items",
                "short_desc": "The transaction line items",
                "desc": "The line items for the transaction (list of line item input objects with "
                    "accountId, amount, balance type)",
            },
            "anchor": {
                "type": AutoHashOrNothingType,
                "display_name": "Anchor",
                "short_desc": "The anchor account",
                "desc": "The anchor account for the transaction (hash with accountId and amount)",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace WaveDataProvider
