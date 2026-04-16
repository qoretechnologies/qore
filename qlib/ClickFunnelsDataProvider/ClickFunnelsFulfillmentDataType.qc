# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ClickFunnelsFulfillmentDataType.qc ClickFunnels fulfillment data type

/*  ClickFunnelsFulfillmentDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! ClickFunnels fulfillment record type
public class ClickFunnelsFulfillmentDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The fulfillment ID",
                "desc": "The unique identifier of the fulfillment",
            },
            "public_id": {
                "type": StringOrNothingType,
                "display_name": "Public ID",
                "short_desc": "The public identifier",
                "desc": "The public-facing unique identifier of the fulfillment",
            },
            "workspace_id": {
                "type": IntOrNothingType,
                "display_name": "Workspace ID",
                "short_desc": "The workspace ID",
                "desc": "The ID of the workspace this fulfillment belongs to",
            },
            "contact_id": {
                "type": IntOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "The contact ID",
                "desc": "The ID of the contact for this fulfillment",
            },
            "order_id": {
                "type": IntOrNothingType,
                "display_name": "Order ID",
                "short_desc": "The order ID",
                "desc": "The ID of the order for this fulfillment",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The fulfillment status",
                "desc": "The current status of the fulfillment",
            },
            "tracking_code": {
                "type": StringOrNothingType,
                "display_name": "Tracking Code",
                "short_desc": "The shipment tracking code",
                "desc": "The tracking code for the shipment",
            },
            "shipping_provider": {
                "type": StringOrNothingType,
                "display_name": "Shipping Provider",
                "short_desc": "The shipping provider",
                "desc": "The shipping provider for the fulfillment",
            },
            "tracking_url": {
                "type": StringOrNothingType,
                "display_name": "Tracking URL",
                "short_desc": "The shipment tracking URL",
                "desc": "The URL for tracking the shipment",
            },
            "shipped_at": {
                "type": DateOrNothingType,
                "display_name": "Shipped At",
                "short_desc": "When the fulfillment was shipped",
                "desc": "The timestamp when the fulfillment was shipped",
            },
            "created_at": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the fulfillment was created",
                "desc": "The timestamp when the fulfillment was created",
            },
            "updated_at": {
                "type": DateOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the fulfillment was last updated",
                "desc": "The timestamp when the fulfillment was last updated",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ClickFunnelsDataProvider
