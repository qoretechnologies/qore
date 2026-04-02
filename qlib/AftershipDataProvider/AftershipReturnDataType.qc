# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AftershipDataProvider module definition

/** AftershipReturnDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the AftershipDataProvider module
public namespace AftershipDataProvider {
#! Return response data type
public class AftershipReturnDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Return ID",
                "short_desc": "Return record identifier",
                "desc": "Unique return record identifier",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "rma_number": {
                "display_name": "RMA Number",
                "short_desc": "Return merchandise authorization number",
                "desc": "Return merchandise authorization number (e.g., `A1B2C3D4`)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "filed_by": {
                "display_name": "Filed By",
                "short_desc": "Who filed the return",
                "desc": "Who filed the return: `shopper`, `merchant`, or `customer_support`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "approval_status": {
                "display_name": "Approval Status",
                "short_desc": "Return approval status",
                "desc": "Return approval status: `submitted`, `approved`, `done`, `rejected`, `expired`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "merchant_note": {
                "display_name": "Merchant Note",
                "short_desc": "Merchant notes",
                "desc": "Notes added by the merchant",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "gift_return": {
                "display_name": "Gift Return",
                "short_desc": "Whether this is a gift return",
                "desc": "`True` if this is a gift return",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "approved_at": {
                "display_name": "Approved At",
                "short_desc": "Approval timestamp",
                "desc": "ISO 8601 timestamp when the return was approved",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "rejected_at": {
                "display_name": "Rejected At",
                "short_desc": "Rejection timestamp",
                "desc": "ISO 8601 timestamp when the return was rejected",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "resolved_at": {
                "display_name": "Resolved At",
                "short_desc": "Resolution timestamp",
                "desc": "ISO 8601 timestamp when the return was resolved",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "refunded_at": {
                "display_name": "Refunded At",
                "short_desc": "Refund timestamp",
                "desc": "ISO 8601 timestamp when the refund was processed",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "created_at": {
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "ISO 8601 timestamp when the return was created",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "updated_at": {
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "ISO 8601 timestamp when the return was last updated",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "external_order_id": {
                "display_name": "External Order ID",
                "short_desc": "Order ID from e-commerce platform",
                "desc": "Order ID from the e-commerce platform",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "order_name": {
                "display_name": "Order Name",
                "short_desc": "Order display name",
                "desc": "Display name of the order (e.g., `#1001`)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "customer_email": {
                "display_name": "Customer Email",
                "short_desc": "Customer email address",
                "desc": "Email address of the customer who filed the return",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "customer_name": {
                "display_name": "Customer Name",
                "short_desc": "Customer name",
                "desc": "Name of the customer who filed the return",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Singleton instance
public const AftershipReturnDataType = new AftershipReturnDataType();
}
