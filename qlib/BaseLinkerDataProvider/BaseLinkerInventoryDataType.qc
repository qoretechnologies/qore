# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerInventoryDataType.qc
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! BaseLinker inventory (catalog) record
public class BaseLinkerInventoryDataType inherits HashDataType {
    public {
        const Fields = {
            "inventory_id": {"display_name": "Inventory ID",
                "short_desc": "Inventory identifier", "type": SoftStringType},
            "name": {"display_name": "Name",
                "short_desc": "Inventory display name", "type": SoftStringOrNothingType},
            "description": {"display_name": "Description",
                "short_desc": "Inventory description", "type": SoftStringOrNothingType},
            "languages": {"display_name": "Languages",
                "short_desc": "Supported language codes",
                "type": AbstractDataProviderType::get(AutoListOrNothingType)},
            "default_language": {"display_name": "Default Language",
                "short_desc": "Default language code", "type": SoftStringOrNothingType},
            "price_groups": {"display_name": "Price Groups",
                "short_desc": "Associated price groups",
                "type": AbstractDataProviderType::get(AutoListOrNothingType)},
            "default_price_group": {"display_name": "Default Price Group",
                "short_desc": "Default price group ID", "type": SoftStringOrNothingType},
            "warehouses": {"display_name": "Warehouses",
                "short_desc": "Associated warehouse identifiers",
                "type": AbstractDataProviderType::get(AutoListOrNothingType)},
            "default_warehouse": {"display_name": "Default Warehouse",
                "short_desc": "Default warehouse ID", "type": SoftStringOrNothingType},
            "reservations": {"display_name": "Reservations Enabled",
                "short_desc": "Whether stock reservations are enabled",
                "type": SoftBoolOrNothingType},
        };
    }
    constructor() : HashDataType("BaseLinkerInventoryDataType") {
        addQoreFields(Fields);
    }
}
} # namespace BaseLinkerDataProvider
