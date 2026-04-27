# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerInventoryCategoryDataType.qc inventory category record
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! An inventory category (from `getInventoryCategories`)
public class BaseLinkerInventoryCategoryDataType inherits HashDataType {
    public {
        const Fields = {
            "category_id": {
                "display_name": "Category ID",
                "short_desc": "Category identifier",
                "type": SoftIntType,
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Category display name",
                "type": SoftStringOrNothingType,
            },
            "parent_id": {
                "display_name": "Parent Category ID",
                "short_desc": "Parent category identifier (0 = top-level)",
                "type": SoftIntOrNothingType,
                "desc": "Identifier of the parent category. `0` indicates a top-level "
                    "category.",
            },
        };
    }
    constructor() : HashDataType("BaseLinkerInventoryCategoryDataType") {
        addQoreFields(Fields);
    }
}
} # namespace BaseLinkerDataProvider
