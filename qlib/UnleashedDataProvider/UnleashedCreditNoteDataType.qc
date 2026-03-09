# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnleashedDataProvider module definition

/*  UnleashedCreditNoteDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the UnleashedDataProvider module
public namespace UnleashedDataProvider {

#! Credit note data type for Unleashed
public class UnleashedCreditNoteDataType inherits HashDataType {
    public {
        const Fields = {
            "Guid": {
                "type": SoftStringOrNothingType,
                "display_name": "Credit Note GUID",
                "short_desc": "Unique credit note identifier",
                "desc": "The unique GUID identifier of the credit note",
            },
            "CreditNumber": {
                "type": SoftStringOrNothingType,
                "display_name": "Credit Number",
                "short_desc": "Credit note number",
                "desc": "The credit note number",
            },
            "CreditDate": {
                "type": SoftDateOrNothingType,
                "display_name": "Credit Date",
                "short_desc": "Credit note date",
                "desc": "The date of the credit note",
            },
            "CreditStatus": {
                "type": SoftStringOrNothingType,
                "display_name": "Credit Status",
                "short_desc": "Status of the credit note",
                "desc": "The current status of the credit note (Parked, Completed)",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Parked", "desc": "Credit note is parked/pending"},
                    <AllowedValueInfo>{"value": "Completed", "desc": "Credit note is completed"},
                ),
            },
            "Customer": {
                "type": AutoHashOrNothingType,
                "display_name": "Customer",
                "short_desc": "Customer details",
                "desc": "The customer for this credit note",
            },
            "Warehouse": {
                "type": AutoHashOrNothingType,
                "display_name": "Warehouse",
                "short_desc": "Warehouse details",
                "desc": "The warehouse for this credit note",
            },
            "Currency": {
                "type": AutoHashOrNothingType,
                "display_name": "Currency",
                "short_desc": "Currency details",
                "desc": "The currency for this credit note",
            },
            "ExchangeRate": {
                "type": SoftFloatOrNothingType,
                "display_name": "Exchange Rate",
                "short_desc": "Currency exchange rate",
                "desc": "The exchange rate for this credit note",
            },
            "SubTotal": {
                "type": SoftFloatOrNothingType,
                "display_name": "Sub Total",
                "short_desc": "Credit note subtotal",
                "desc": "The subtotal before tax",
            },
            "TaxTotal": {
                "type": SoftFloatOrNothingType,
                "display_name": "Tax Total",
                "short_desc": "Total tax",
                "desc": "The total tax amount",
            },
            "Total": {
                "type": SoftFloatOrNothingType,
                "display_name": "Total",
                "short_desc": "Credit note total",
                "desc": "The total credit note amount including tax",
            },
            "CreditLines": {
                "type": new ListDataType("CreditLines", new UnleashedCreditNoteLineDataType(), True),
                "display_name": "Credit Lines",
                "short_desc": "Credit note line items",
                "desc": "The line items for this credit note",
            },
            "Comments": {
                "type": SoftStringOrNothingType,
                "display_name": "Comments",
                "short_desc": "Credit note comments",
                "desc": "Comments or notes for this credit note (max 1024 characters)",
            },
            "Reference": {
                "type": SoftStringOrNothingType,
                "display_name": "Reference",
                "short_desc": "Reference number",
                "desc": "Reference number for this credit note (max 500 characters)",
            },
            "CreatedBy": {
                "type": SoftStringOrNothingType,
                "display_name": "Created By",
                "short_desc": "Creator username",
                "desc": "Username of the person who created this credit note",
            },
            "LastModifiedBy": {
                "type": SoftStringOrNothingType,
                "display_name": "Last Modified By",
                "short_desc": "Modifier username",
                "desc": "Username of the person who last modified this credit note",
            },
            "CreatedOn": {
                "type": SoftDateOrNothingType,
                "display_name": "Created On",
                "short_desc": "Creation date",
                "desc": "When the credit note was created",
            },
            "LastModifiedOn": {
                "type": SoftDateOrNothingType,
                "display_name": "Last Modified On",
                "short_desc": "Last modification date",
                "desc": "When the credit note was last modified",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("UnleashedCreditNoteDataType") {
        addQoreFields(Fields);
    }
}

#! Credit note line data type
public class UnleashedCreditNoteLineDataType inherits HashDataType {
    public {
        const Fields = {
            "Guid": {
                "type": SoftStringOrNothingType,
                "display_name": "Line GUID",
                "short_desc": "Unique line identifier",
                "desc": "The unique GUID identifier of the credit note line",
            },
            "Product": {
                "type": AutoHashOrNothingType,
                "display_name": "Product",
                "short_desc": "Product details",
                "desc": "The product for this line (Guid or ProductCode)",
            },
            "CreditQuantity": {
                "type": SoftFloatOrNothingType,
                "display_name": "Credit Quantity",
                "short_desc": "Quantity credited",
                "desc": "The quantity being credited",
            },
            "CreditPrice": {
                "type": SoftFloatOrNothingType,
                "display_name": "Credit Price",
                "short_desc": "Price per unit",
                "desc": "The price per unit for this credit",
            },
            "Reason": {
                "type": SoftStringOrNothingType,
                "display_name": "Reason",
                "short_desc": "Reason for credit",
                "desc": "The reason for this credit",
            },
            "Return": {
                "type": SoftBoolOrNothingType,
                "display_name": "Return",
                "short_desc": "Is a return",
                "desc": "Whether this is a return of goods",
            },
            "Comments": {
                "type": SoftStringOrNothingType,
                "display_name": "Comments",
                "short_desc": "Line comments",
                "desc": "Comments for this line item (max 1024 characters)",
            },
            "BatchNumbers": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Batch Numbers",
                "short_desc": "Batch tracking numbers",
                "desc": "Batch numbers for this line (if batch tracked)",
            },
            "SerialNumbers": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Serial Numbers",
                "short_desc": "Serial tracking numbers",
                "desc": "Serial numbers for this line (if serialized)",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("UnleashedCreditNoteLineDataType") {
        addQoreFields(Fields);
    }
}

} # namespace UnleashedDataProvider
