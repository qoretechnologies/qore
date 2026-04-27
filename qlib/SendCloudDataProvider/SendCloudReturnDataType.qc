# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudReturnDataType.qc SendCloud return record and request types

/*  SendCloudReturnDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SendCloudDataProvider module
public namespace SendCloudDataProvider {

#! Single product line within a SendCloud return
public class SendCloudReturnProductDataType inherits HashDataType {
    public {
        const Fields = {
            "product_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Product ID",
                "short_desc": "Product identifier",
                "desc": "Merchant-provided product ID for this returned line item",
            },
            "description": {
                "type": SoftStringOrNothingType,
                "display_name": "Description",
                "short_desc": "Line item description",
                "desc": "Description of the returned item",
            },
            "quantity": {
                "type": SoftIntOrNothingType,
                "display_name": "Quantity",
                "short_desc": "Number of units returned",
                "desc": "Number of units of this product being returned",
            },
            "value": {
                "type": SoftStringOrNothingType,
                "display_name": "Value",
                "short_desc": "Per-unit value (string-encoded)",
                "desc": "Value per unit (string-encoded decimal)",
            },
            "sku": {
                "type": SoftStringOrNothingType,
                "display_name": "SKU",
                "short_desc": "Stock keeping unit",
                "desc": "SKU of the returned item",
            },
            "return_reason": {
                "type": SoftIntOrNothingType,
                "display_name": "Return Reason",
                "short_desc": "Return reason code",
                "desc": "Numeric code identifying the return reason",
            },
            "return_message": {
                "type": SoftStringOrNothingType,
                "display_name": "Return Message",
                "short_desc": "Customer message",
                "desc": "Customer-supplied message describing the return reason",
            },
        };
    }

    constructor() : HashDataType("SendCloudReturnProductDataType") {
        addQoreFields(Fields);
    }
}

#! Return record (response of GET /returns, POST /returns, etc.)
public class SendCloudReturnDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftIntOrNothingType,
                "display_name": "ID",
                "short_desc": "Return ID",
                "desc": "Unique numeric ID of the return",
            },
            "status": {
                "type": SoftStringOrNothingType,
                "display_name": "Status",
                "short_desc": "Return status",
                "desc": "Current status of the return",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "created", "display_name": "Created",
                        "desc": "Return created but not yet announced"},
                    <AllowedValueInfo>{"value": "announced", "display_name": "Announced",
                        "desc": "Return announced to the carrier"},
                    <AllowedValueInfo>{"value": "delivered", "display_name": "Delivered",
                        "desc": "Return delivered"},
                    <AllowedValueInfo>{"value": "cancelled", "display_name": "Cancelled",
                        "desc": "Return cancelled"},
                    <AllowedValueInfo>{"value": "completed", "display_name": "Completed",
                        "desc": "Return processing completed"},
                ),
            },
            "created_at": {
                "type": SoftDateOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "Timestamp when the return was created",
            },
            "updated_at": {
                "type": SoftDateOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "Timestamp when the return was last updated",
            },
            "reason": {
                "type": SoftStringOrNothingType,
                "display_name": "Reason",
                "short_desc": "Overall return reason",
                "desc": "Reason provided for the return",
            },
            "tracking_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Tracking Number",
                "short_desc": "Carrier tracking number",
                "desc": "Tracking number of the return shipment",
            },
            "parcel": {
                "type": AutoHashOrNothingType,
                "display_name": "Parcel",
                "short_desc": "Associated parcel data",
                "desc": "Parcel object associated with the return (subset of parcel fields)",
            },
            "products": {
                "type": new ListDataType("ReturnProductList", new SendCloudReturnProductDataType(), True),
                "display_name": "Products",
                "short_desc": "Returned line items",
                "desc": "List of returned product lines",
            },
            "refund": {
                "type": new SendCloudReturnRefundResponseDataType(AutoHashOrNothingType),
                "display_name": "Refund",
                "short_desc": "Refund details",
                "desc": "Refund object with `total_refund`, `refunded_at`, `message`, and nested `refund_type` sub-object",
            },
        };
    }

    constructor() : HashDataType("SendCloudReturnDataType") {
        addQoreFields(Fields);
    }
}

#! Refund info used in create-return requests
public class SendCloudReturnRefundRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "refund_type": {
                "type": SoftStringOrNothingType,
                "display_name": "Refund Type",
                "short_desc": "Type of refund",
                "desc": "Type of refund offered (`money`, `store_credit`, `exchange`)",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "money", "display_name": "Money",
                        "desc": "Monetary refund"},
                    <AllowedValueInfo>{"value": "store_credit", "display_name": "Store Credit",
                        "desc": "Store credit"},
                    <AllowedValueInfo>{"value": "exchange", "display_name": "Exchange",
                        "desc": "Exchange for another product"},
                ),
            },
            "total_refund": {
                "type": SoftStringOrNothingType,
                "display_name": "Total Refund",
                "short_desc": "Total refund amount",
                "desc": "Total refund amount (string-encoded decimal)",
            },
        };
    }

    constructor() : HashDataType("SendCloudReturnRefundRequestDataType") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type, "SendCloudReturnRefundRequestDataType") {
        addQoreFields(Fields);
    }
}

#! Request type for creating a SendCloud return (POST /returns wrapped in {return: ...})
public class SendCloudReturnCreateRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "outgoing_parcel": {
                "type": SoftIntType,
                "display_name": "Outgoing Parcel",
                "short_desc": "The parcel being returned",
                "desc": "ID of the original outgoing parcel being returned.  Used to derive "
                    "the customer pickup address (`from_address` on the v3 return) and the "
                    "merchant return destination (`to_address`) automatically.",
                "required": True,
                "ref_data": "parcels",
                "example_value": 12345,
            },
            "shipping_option_code": {
                "type": SoftStringOrNothingType,
                "display_name": "Return Shipping Option Code",
                "short_desc": "v3 shipping_option_code for the return shipment",
                "desc": "v3 shipping option code to use for the return (e.g. `dpd:return/return`).  "
                    "Required for v3-cohort accounts; if omitted, the data provider attempts to "
                    "translate the outgoing parcel's shipping method via the SendCloud "
                    "compatibility endpoint, but this is not guaranteed to produce a "
                    "return-capable option.  Use a return-specific code when in doubt.",
                "example_value": "dpd:return/return",
            },
            "weight": {
                "type": SoftStringOrNothingType,
                "display_name": "Weight",
                "short_desc": "Total weight of the return parcel",
                "desc": "Total weight of the return parcel (string-encoded decimal in kg).  "
                    "Defaults to the outgoing parcel's weight when not provided; falls back "
                    "to `1.000` when the outgoing parcel has no weight set.",
                "example_value": "1.000",
            },
            "refund": {
                "type": new SendCloudReturnRefundRequestDataType(AutoHashOrNothingType),
                "display_name": "Refund",
                "short_desc": "Refund details",
                "desc": "Optional refund hash describing the refund type and amount",
            },
            "products": {
                "type": new ListDataType("ReturnProductsInput", new SendCloudReturnProductDataType(), True),
                "display_name": "Products",
                "short_desc": "Products being returned",
                "desc": "List of product lines being returned (at least one entry required)",
                "required": True,
                "example_value": (
                    {
                        "description": "T-Shirt",
                        "quantity": 1,
                        "value": "19.95",
                    },
                ),
            },
            "reason": {
                "type": SoftStringOrNothingType,
                "display_name": "Reason",
                "short_desc": "Overall return reason",
                "desc": "Return reason category",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "damaged", "display_name": "Damaged",
                        "desc": "Product damaged"},
                    <AllowedValueInfo>{"value": "defective", "display_name": "Defective",
                        "desc": "Product defective"},
                    <AllowedValueInfo>{"value": "wrong_product", "display_name": "Wrong Product",
                        "desc": "Wrong product delivered"},
                    <AllowedValueInfo>{"value": "not_as_described", "display_name": "Not as Described",
                        "desc": "Product not as described"},
                    <AllowedValueInfo>{"value": "no_longer_needed", "display_name": "No Longer Needed",
                        "desc": "Product no longer needed"},
                    <AllowedValueInfo>{"value": "other", "display_name": "Other",
                        "desc": "Other reason"},
                ),
            },
            "message": {
                "type": SoftStringOrNothingType,
                "display_name": "Message",
                "short_desc": "Customer-provided message",
                "desc": "Customer-provided message describing the return",
            },
            "address": {
                "type": new SendCloudAddressDataType(AutoHashOrNothingType),
                "display_name": "Pickup Address",
                "short_desc": "Pickup address for the return",
                "desc": "Pickup address for the return (if different from the original shipping address)",
            },
        };
    }

    constructor() : HashDataType("SendCloudReturnCreateRequestDataType") {
        addQoreFields(Fields);
    }
}

#! Request type for updating a SendCloud return (PUT /returns/{id} wrapped in {return: ...})
public class SendCloudReturnUpdateRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftIntType,
                "display_name": "Return ID",
                "short_desc": "Numeric ID of the return to update",
                "desc": "ID of the return to update",
                "required": True,
                "ref_data": "returns",
            },
            "status": {
                "type": SoftStringOrNothingType,
                "display_name": "Status",
                "short_desc": "New status",
                "desc": "New status for the return",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "created", "display_name": "Created",
                        "desc": "Return created"},
                    <AllowedValueInfo>{"value": "announced", "display_name": "Announced",
                        "desc": "Return announced"},
                    <AllowedValueInfo>{"value": "delivered", "display_name": "Delivered",
                        "desc": "Return delivered"},
                    <AllowedValueInfo>{"value": "cancelled", "display_name": "Cancelled",
                        "desc": "Return cancelled"},
                    <AllowedValueInfo>{"value": "completed", "display_name": "Completed",
                        "desc": "Return completed"},
                ),
            },
            "tracking_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Tracking Number",
                "short_desc": "Carrier tracking number",
                "desc": "Tracking number to associate with the return",
            },
            "reason": {
                "type": SoftStringOrNothingType,
                "display_name": "Reason",
                "short_desc": "Updated reason",
                "desc": "Updated return reason",
            },
        };
    }

    constructor() : HashDataType("SendCloudReturnUpdateRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace SendCloudDataProvider
