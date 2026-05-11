# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaInventoryDataType inherits HashDataType {
    public {
        const Fields = {
            "variant_id": {"type": SoftIntOrNothingType, "display_name": "Variant ID",
                "short_desc": "Variant ID", "desc": "Variant identifier",
                "ref_data": "variants"},
            "location_id": {"type": SoftIntOrNothingType, "display_name": "Location ID",
                "short_desc": "Location ID", "desc": "Location identifier",
                "ref_data": "locations"},
            "quantity_in_stock": {"type": SoftFloatOrNothingType,
                "display_name": "Quantity In Stock",
                "short_desc": "Quantity in stock",
                "desc": "Current quantity on hand"},
            "quantity_committed": {"type": SoftFloatOrNothingType,
                "display_name": "Quantity Committed",
                "short_desc": "Quantity committed",
                "desc": "Quantity committed to orders"},
            "quantity_expected": {"type": SoftFloatOrNothingType,
                "display_name": "Quantity Expected",
                "short_desc": "Quantity expected",
                "desc": "Quantity expected from incoming POs/MOs"},
            "quantity_missing_or_excess": {"type": SoftFloatOrNothingType,
                "display_name": "Missing/Excess",
                "short_desc": "Missing or excess",
                "desc": "Missing or excess quantity"},
            "quantity_potential": {"type": SoftFloatOrNothingType,
                "display_name": "Quantity Potential",
                "short_desc": "Potential", "desc": "Potential available quantity"},
            "value_in_stock": {"type": SoftFloatOrNothingType,
                "display_name": "Value In Stock",
                "short_desc": "Stock value",
                "desc": "Value of the stock on hand"},
            "average_cost": {"type": SoftFloatOrNothingType,
                "display_name": "Average Cost",
                "short_desc": "Average cost",
                "desc": "Average cost per unit"},
            "reorder_point": {"type": SoftFloatOrNothingType,
                "display_name": "Reorder Point",
                "short_desc": "Reorder point",
                "desc": "Reorder threshold"},
            "safety_stock_level": {"type": SoftFloatOrNothingType,
                "display_name": "Safety Stock Level",
                "short_desc": "Safety stock level",
                "desc": "Safety stock level"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Request body for setting safety stock levels or reorder points on a variant/location pair
/** Used by both @c POST @c /inventory_safety_stock_levels and @c POST @c /inventory_reorder_points,
    which share the same request schema (@c variant_id + @c location_id + @c value).
*/
public class KatanaInventoryThresholdRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "variant_id": {"type": SoftIntType, "required": True,
                "display_name": "Variant ID",
                "short_desc": "Variant ID", "desc": "Variant identifier (required)",
                "ref_data": "variants"},
            "location_id": {"type": SoftIntType, "required": True,
                "display_name": "Location ID",
                "short_desc": "Location ID", "desc": "Location identifier (required)",
                "ref_data": "locations"},
            "value": {"type": SoftFloatType, "required": True,
                "display_name": "Value",
                "short_desc": "Threshold value",
                "desc": "Threshold value to set (safety stock level or reorder point quantity)",
                "example_value": 10.0},
        };
    }
    constructor() { addQoreFields(Fields); }
}

} # namespace KatanaDataProvider
