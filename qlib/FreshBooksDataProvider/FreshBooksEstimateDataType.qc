# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FreshBooksDataProvider module definition

/*  FreshBooksEstimateDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! FreshBooks estimate record type
public class FreshBooksEstimateDataType inherits HashDataType {
    private {
        #! Field definitions for FreshBooks estimates
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The estimate ID",
                "desc": "The unique identifier of the estimate",
            },
            "estimateid": {
                "type": IntType,
                "display_name": "Estimate ID",
                "short_desc": "The internal estimate ID",
                "desc": "The internal estimate ID",
            },
            "estimate_number": {
                "type": StringOrNothingType,
                "display_name": "Estimate Number",
                "short_desc": "The estimate number",
                "desc": "The estimate number for reference",
            },
            "customerid": {
                "type": IntType,
                "display_name": "Customer ID",
                "short_desc": "The customer ID",
                "desc": "The ID of the customer the estimate is for",
            },
            "create_date": {
                "type": StringOrNothingType,
                "display_name": "Create Date",
                "short_desc": "The creation date",
                "desc": "The date the estimate was created (YYYY-MM-DD format)",
            },
            "currency_code": {
                "type": StringOrNothingType,
                "display_name": "Currency Code",
                "short_desc": "The currency code",
                "desc": "The ISO 4217 currency code for the estimate",
            },
            "status": {
                "type": IntOrNothingType,
                "display_name": "Status",
                "short_desc": "The estimate status",
                "desc": "The status of the estimate",
            },
            "amount": {
                "type": AutoHashType,
                "display_name": "Amount",
                "short_desc": "The total amount",
                "desc": "The total amount of the estimate (hash with 'amount' and 'code' keys)",
            },
            "notes": {
                "type": StringOrNothingType,
                "display_name": "Notes",
                "short_desc": "The estimate notes",
                "desc": "Notes displayed on the estimate",
            },
            "terms": {
                "type": StringOrNothingType,
                "display_name": "Terms",
                "short_desc": "The estimate terms",
                "desc": "The terms displayed on the estimate",
            },
            "po_number": {
                "type": StringOrNothingType,
                "display_name": "PO Number",
                "short_desc": "The purchase order number",
                "desc": "The purchase order number associated with the estimate",
            },
            "vis_state": {
                "type": IntOrNothingType,
                "display_name": "Visibility State",
                "short_desc": "The visibility state",
                "desc": "The visibility state of the estimate (0=active, 1=deleted, 2=archived)",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! FreshBooks estimate create request type
public class FreshBooksEstimateCreateDataType inherits HashDataType {
    private {
        #! Field definitions for creating estimates
        const Fields = FreshBooksDataProviderBase::AccountIdField + {
            "customerid": {
                "type": IntType,
                "display_name": "Customer ID",
                "short_desc": "The customer ID (required)",
                "desc": "The ID of the customer the estimate is for",
                "required": True,
            },
            "estimate_number": {
                "type": StringOrNothingType,
                "display_name": "Estimate Number",
                "short_desc": "The estimate number",
                "desc": "The estimate number for reference",
            },
            "create_date": {
                "type": StringOrNothingType,
                "display_name": "Create Date",
                "short_desc": "The creation date",
                "desc": "The date the estimate was created (YYYY-MM-DD format)",
            },
            "currency_code": {
                "type": StringOrNothingType,
                "display_name": "Currency Code",
                "short_desc": "The currency code",
                "desc": "The ISO 4217 currency code for the estimate",
            },
            "notes": {
                "type": StringOrNothingType,
                "display_name": "Notes",
                "short_desc": "The estimate notes",
                "desc": "Notes to display on the estimate",
            },
            "terms": {
                "type": StringOrNothingType,
                "display_name": "Terms",
                "short_desc": "The estimate terms",
                "desc": "The terms to display on the estimate",
            },
            "po_number": {
                "type": StringOrNothingType,
                "display_name": "PO Number",
                "short_desc": "The purchase order number",
                "desc": "The purchase order number to associate with the estimate",
            },
            "lines": {
                "type": AutoListOrNothingType,
                "display_name": "Line Items",
                "short_desc": "Estimate line items",
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

#! FreshBooks estimate update request type
public class FreshBooksEstimateUpdateDataType inherits HashDataType {
    private {
        #! Field definitions for updating estimates
        const Fields = FreshBooksDataProviderBase::AccountIdField + {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The estimate ID (required)",
                "desc": "The unique identifier of the estimate to update",
                "required": True,
            },
            "customerid": {
                "type": IntOrNothingType,
                "display_name": "Customer ID",
                "short_desc": "The customer ID",
                "desc": "The ID of the customer the estimate is for",
            },
            "estimate_number": {
                "type": StringOrNothingType,
                "display_name": "Estimate Number",
                "short_desc": "The estimate number",
                "desc": "The estimate number for reference",
            },
            "create_date": {
                "type": StringOrNothingType,
                "display_name": "Create Date",
                "short_desc": "The creation date",
                "desc": "The date of the estimate (YYYY-MM-DD format)",
            },
            "currency_code": {
                "type": StringOrNothingType,
                "display_name": "Currency Code",
                "short_desc": "The currency code",
                "desc": "The ISO 4217 currency code for the estimate",
            },
            "notes": {
                "type": StringOrNothingType,
                "display_name": "Notes",
                "short_desc": "The estimate notes",
                "desc": "Notes to display on the estimate",
            },
            "terms": {
                "type": StringOrNothingType,
                "display_name": "Terms",
                "short_desc": "The estimate terms",
                "desc": "The terms to display on the estimate",
            },
            "po_number": {
                "type": StringOrNothingType,
                "display_name": "PO Number",
                "short_desc": "The purchase order number",
                "desc": "The purchase order number to associate with the estimate",
            },
            "lines": {
                "type": AutoListOrNothingType,
                "display_name": "Line Items",
                "short_desc": "Estimate line items",
                "desc": "List of line items (replaces existing lines - include ALL desired lines). "
                    "Each with fields: description, qty, unit_cost (hash with 'amount' and 'code'), "
                    "name, taxName1, taxAmount1, taxName2, taxAmount2",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace FreshBooksDataProvider
