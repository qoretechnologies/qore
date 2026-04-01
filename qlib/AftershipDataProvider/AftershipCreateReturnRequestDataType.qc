# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AftershipDataProvider module definition

/** AftershipCreateReturnRequestDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Return method data type
public class AftershipReturnMethodDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "display_name": "Return Method Type",
                "short_desc": "Type of return shipping method",
                "desc": "Return shipping method type",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "retailer_label", "display_name": "Retailer Label"},
                    <AllowedValueInfo>{"value": "customer_courier", "display_name": "Customer Courier"},
                    <AllowedValueInfo>{"value": "happy_returns", "display_name": "Happy Returns"},
                    <AllowedValueInfo>{"value": "in_store", "display_name": "In Store"},
                    <AllowedValueInfo>{"value": "green_return", "display_name": "Green Return"},
                    <AllowedValueInfo>{"value": "retail_reworks", "display_name": "Retail Reworks"},
                    <AllowedValueInfo>{"value": "carrier_pickup", "display_name": "Carrier Pickup"},
                    <AllowedValueInfo>{"value": "carrier_dropoff", "display_name": "Carrier Dropoff"},
                ),
            },
        };
    }
    constructor() { addQoreFields(Fields); }
    constructor(Type base_type) : HashDataType(base_type) { addQoreFields(Fields); }
}

#! Return item data type
public class AftershipReturnItemDataType inherits HashDataType {
    public {
        const Fields = {
            "external_order_item_id": {
                "display_name": "External Order Item ID",
                "short_desc": "Item ID from e-commerce platform",
                "desc": "ID of the item on the e-commerce platform",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "10579097059534",
            },
            "intended_return_quantity": {
                "display_name": "Return Quantity",
                "short_desc": "Number of items to return",
                "desc": "Number of items to return (must be >= `1`)",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "example_value": 1,
            },
            "return_reason": {
                "display_name": "Return Reason",
                "short_desc": "Reason for the return",
                "desc": "Reason for returning the item",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "Wrong size",
            },
            "resolution": {
                "display_name": "Resolution",
                "short_desc": "Resolution type",
                "desc": "Resolution type for the return",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "refund", "display_name": "Refund"},
                ),
            },
            "refund_destination": {
                "display_name": "Refund Destination",
                "short_desc": "Where to send the refund",
                "desc": "Where to send the refund",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "original_payment", "display_name": "Original Payment"},
                    <AllowedValueInfo>{"value": "store_credit", "display_name": "Store Credit"},
                ),
            },
            "return_subreason": {
                "display_name": "Return Sub-Reason",
                "short_desc": "Sub-reason for the return",
                "desc": "Sub-reason if the reason supports it",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "Too small",
            },
            "return_reason_comment": {
                "display_name": "Return Reason Comment",
                "short_desc": "Additional customer comments",
                "desc": "Additional customer comments on the return reason",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "return_method": {
                "display_name": "Return Method",
                "short_desc": "Shipping method for the return",
                "desc": "Shipping method for the return",
                "type": new AftershipReturnMethodDataType(AutoHashOrNothingType),
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Singleton instance
public const AftershipReturnItemDataType = new AftershipReturnItemDataType();

#! Request type for creating a return
public class AftershipCreateReturnRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "external_order_id": {
                "display_name": "External Order ID",
                "short_desc": "Order ID from e-commerce platform",
                "desc": "Order ID from the e-commerce platform (used as `order.external_id` in the API request)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "4254002020558",
            },
            "return_items": {
                "display_name": "Return Items",
                "short_desc": "Items to return",
                "desc": "List of items to return with quantities, reasons, and resolutions",
                "type": new ListDataType("AftershipReturnItemList", AftershipReturnItemDataType, True),
                "required": True,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Singleton instance
public const AftershipCreateReturnRequestDataType = new AftershipCreateReturnRequestDataType();
}
