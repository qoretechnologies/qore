# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ClickFunnelsOrderDataType.qc ClickFunnels order data type

/*  ClickFunnelsOrderDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace ClickFunnelsDataProvider {

#! ClickFunnels order record type
public class ClickFunnelsOrderDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The order ID",
                "desc": "The unique identifier of the order",
            },
            "public_id": {
                "type": StringOrNothingType,
                "display_name": "Public ID",
                "short_desc": "The public identifier",
                "desc": "The public-facing unique identifier of the order",
            },
            "workspace_id": {
                "type": IntOrNothingType,
                "display_name": "Workspace ID",
                "short_desc": "The workspace ID",
                "desc": "The ID of the workspace this order belongs to",
            },
            "contact_id": {
                "type": IntOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "The associated contact ID",
                "desc": "The ID of the contact associated with this order",
            },
            "order_number": {
                "type": StringOrNothingType,
                "display_name": "Order Number",
                "short_desc": "The order number",
                "desc": "The order number for display purposes",
            },
            "currency": {
                "type": StringOrNothingType,
                "display_name": "Currency",
                "short_desc": "The order currency",
                "desc": "The currency code for the order (e.g., `USD`, `EUR`)",
            },
            "total_amount": {
                "type": StringOrNothingType,
                "display_name": "Total Amount",
                "short_desc": "The total order amount",
                "desc": "The total amount of the order as a string",
            },
            "origination_channel_id": {
                "type": IntOrNothingType,
                "display_name": "Origination Channel ID",
                "short_desc": "The origination channel ID",
                "desc": "The ID of the channel where the order originated",
            },
            "origination_channel_type": {
                "type": StringOrNothingType,
                "display_name": "Origination Channel Type",
                "short_desc": "The origination channel type",
                "desc": "The type of channel where the order originated",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The order status",
                "desc": "The current status of the order",
            },
            "in_trial": {
                "type": SoftBoolOrNothingType,
                "display_name": "In Trial",
                "short_desc": "Whether the order is in trial",
                "desc": "Whether the order is currently in a trial period",
            },
            "created_at": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the order was created",
                "desc": "The timestamp when the order was created",
            },
            "updated_at": {
                "type": DateOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the order was last updated",
                "desc": "The timestamp when the order was last updated",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ClickFunnelsDataProvider
