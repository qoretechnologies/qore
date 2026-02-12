# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FreshBooksDataProvider module definition

/*  FreshBooksPaymentDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! FreshBooks payment record type
public class FreshBooksPaymentDataType inherits HashDataType {
    private {
        #! Field definitions for FreshBooks payments
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The payment ID",
                "desc": "The unique identifier of the payment",
            },
            "paymentid": {
                "type": IntType,
                "display_name": "Payment ID",
                "short_desc": "The internal payment ID",
                "desc": "The internal payment ID",
            },
            "invoiceid": {
                "type": IntOrNothingType,
                "display_name": "Invoice ID",
                "short_desc": "The associated invoice ID",
                "desc": "The ID of the invoice this payment applies to",
            },
            "clientid": {
                "type": IntOrNothingType,
                "display_name": "Client ID",
                "short_desc": "The client ID",
                "desc": "The ID of the client who made the payment",
            },
            "amount": {
                "type": AutoHashType,
                "display_name": "Amount",
                "short_desc": "The payment amount",
                "desc": "The payment amount (hash with 'amount' and 'code' keys)",
            },
            "date": {
                "type": StringOrNothingType,
                "display_name": "Date",
                "short_desc": "The payment date",
                "desc": "The date the payment was made (YYYY-MM-DD format)",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "The payment type",
                "desc": "The payment method type (e.g. Credit Card, Cash, Check)",
            },
            "note": {
                "type": StringOrNothingType,
                "display_name": "Note",
                "short_desc": "The payment note",
                "desc": "A note associated with the payment",
            },
            "vis_state": {
                "type": IntOrNothingType,
                "display_name": "Visibility State",
                "short_desc": "The visibility state",
                "desc": "The visibility state of the payment (0=active, 1=deleted, 2=archived)",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "The creation timestamp",
                "desc": "The timestamp when the payment was created",
            },
            "updated": {
                "type": StringOrNothingType,
                "display_name": "Updated",
                "short_desc": "The last update timestamp",
                "desc": "The timestamp when the payment was last updated",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! FreshBooks payment create request type
public class FreshBooksPaymentCreateDataType inherits HashDataType {
    private {
        #! Field definitions for creating payments
        const Fields = FreshBooksDataProviderBase::AccountIdField + {
            "invoiceid": {
                "type": IntType,
                "display_name": "Invoice ID",
                "short_desc": "The invoice to apply payment to (required)",
                "desc": "The ID of the invoice this payment applies to",
                "required": True,
            },
            "amount": {
                "type": AutoHashType,
                "display_name": "Amount",
                "short_desc": "The payment amount (required)",
                "desc": "The payment amount (hash with 'amount' string and 'code' currency keys)",
                "required": True,
            },
            "date": {
                "type": StringOrNothingType,
                "display_name": "Date",
                "short_desc": "The payment date",
                "desc": "The date the payment was made (YYYY-MM-DD format)",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "The payment type",
                "desc": "The payment method type (e.g. Credit Card, Cash, Check)",
            },
            "note": {
                "type": StringOrNothingType,
                "display_name": "Note",
                "short_desc": "The payment note",
                "desc": "A note associated with the payment",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! FreshBooks payment update request type
public class FreshBooksPaymentUpdateDataType inherits HashDataType {
    private {
        #! Field definitions for updating payments
        const Fields = FreshBooksDataProviderBase::AccountIdField + {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The payment ID (required)",
                "desc": "The unique identifier of the payment to update",
                "required": True,
            },
            "amount": {
                "type": AutoHashOrNothingType,
                "display_name": "Amount",
                "short_desc": "The payment amount",
                "desc": "The new payment amount (hash with 'amount' string and 'code' currency keys)",
            },
            "date": {
                "type": StringOrNothingType,
                "display_name": "Date",
                "short_desc": "The payment date",
                "desc": "The new date of the payment (YYYY-MM-DD format)",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "The payment type",
                "desc": "The new payment method type (e.g. Credit Card, Cash, Check)",
            },
            "note": {
                "type": StringOrNothingType,
                "display_name": "Note",
                "short_desc": "The payment note",
                "desc": "The new note for the payment",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace FreshBooksDataProvider
