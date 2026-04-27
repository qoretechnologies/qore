# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerSetOrderFieldsRequestDataType.qc setOrderFields request type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Request type for the BaseLinker `setOrderFields` method
/** `setOrderFields` updates mutable header fields only. The API does not accept:

    - `order_status_id` (use `set-order-status`)
    - `paid` (use `set-order-payment`)
    - `date_add`, `currency`, `custom_source_id`, `products`

    `setOrderFields` also adds three pick/pack/star workflow flags that are not
    present on `addOrder`.
*/
public class BaseLinkerSetOrderFieldsRequestDataType inherits HashDataType {
    public {
        const ExcludedAddOrderFields = (
            "order_status_id", "date_add", "currency", "paid", "custom_source_id",
            "products",
        );

        const ExtraFields = {
            "pick_state": {
                "display_name": "Pick State",
                "short_desc": "Product-collection workflow flag",
                "type": SoftIntOrNothingType,
                "desc": "Product-collection (picking) workflow flag. `0` = not all products "
                    "collected; `1` = all products collected.",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 0, "display_name": "Not Collected"},
                    <AllowedValueInfo>{"value": 1, "display_name": "Collected"},
                ),
            },
            "pack_state": {
                "display_name": "Pack State",
                "short_desc": "Product-packing workflow flag",
                "type": SoftIntOrNothingType,
                "desc": "Product-packing workflow flag. `0` = not all products packed; "
                    "`1` = all products packed.",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 0, "display_name": "Not Packed"},
                    <AllowedValueInfo>{"value": 1, "display_name": "Packed"},
                ),
            },
            "star": {
                "display_name": "Star Rating",
                "short_desc": "Star rating (0-5)",
                "type": SoftIntOrNothingType,
                "desc": "Star rating in the range `0`-`5`, where `0` means no star.",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 0, "display_name": "No Star"},
                    <AllowedValueInfo>{"value": 1, "display_name": "1 Star"},
                    <AllowedValueInfo>{"value": 2, "display_name": "2 Stars"},
                    <AllowedValueInfo>{"value": 3, "display_name": "3 Stars"},
                    <AllowedValueInfo>{"value": 4, "display_name": "4 Stars"},
                    <AllowedValueInfo>{"value": 5, "display_name": "5 Stars"},
                ),
            },
        };

        const Fields = {
            "order_id": {
                "display_name": "Order ID",
                "short_desc": "Order to update",
                "type": SoftIntType,
                "required": True,
                "desc": "Identifier of the order to update.",
                "ref_data": "orders",
            },
        } + BaseLinkerDataProviderBase::makeFieldsOptional(
            BaseLinkerAddOrderRequestDataType::Fields - ExcludedAddOrderFields)
          + ExtraFields;
    }
    constructor() : HashDataType("BaseLinkerSetOrderFieldsRequestDataType") {
        addQoreFields(Fields);
    }
}
} # namespace BaseLinkerDataProvider
