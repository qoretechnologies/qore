# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerDeleteInventoryProductRequestDataType.qc deleteInventoryProduct request type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Request type for the BaseLinker `deleteInventoryProduct` method
public class BaseLinkerDeleteInventoryProductRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "inventory_id": {
                "display_name": "Inventory ID",
                "short_desc": "Inventory containing the product",
                "type": SoftStringType,
                "required": True,
                "ref_data": "inventories",
            },
            "product_id": {
                "display_name": "Product ID",
                "short_desc": "Product to delete",
                "type": SoftStringType,
                "required": True,
                "ref_data": "inventory_products",
                "depends_on": ("inventory_id",),
            },
        };
    }
    constructor() : HashDataType("BaseLinkerDeleteInventoryProductRequestDataType") {
        addQoreFields(Fields);
    }
}
} # namespace BaseLinkerDataProvider
