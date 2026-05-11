# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Katana stock adjustment row sub-type
public class KatanaStockAdjustmentRowDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftIntOrNothingType,
                "display_name": "ID",
                "short_desc": "Row ID",
                "desc": "The unique identifier of the row (assigned by Katana on create)",
            },
            "stock_adjustment_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Stock Adjustment ID",
                "short_desc": "Parent stock adjustment ID",
                "desc": "The ID of the parent stock adjustment",
                "ref_data": "stock_adjustments",
            },
            "variant_id": {
                "type": SoftIntType,
                "display_name": "Variant ID",
                "required": True,
                "short_desc": "Variant ID",
                "desc": "The ID of the variant being adjusted",
                "ref_data": "variants",
            },
            "quantity": {
                "type": SoftFloatType,
                "display_name": "Quantity",
                "required": True,
                "short_desc": "Quantity delta",
                "desc": "Adjustment quantity (positive for increase, negative for decrease)",
            },
            "cost_per_unit": {
                "type": SoftFloatOrNothingType,
                "display_name": "Cost Per Unit",
                "short_desc": "Cost per unit",
                "desc": "Cost per unit for this adjustment row",
            },
            "batch_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Batch ID",
                "short_desc": "Batch identifier",
                "desc": "The ID of the batch this adjustment is associated with",
                "ref_data": "batch_stocks",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

} # namespace KatanaDataProvider
